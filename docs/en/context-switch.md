# Context Switch in Detail

This document traces the operation of the `SAVE_ALL` / `RESTORE_ALL` macros and
`intr_enter` / `intr_leave` routines (i386/intr.s), which form the core of tiny-itron.

A context switch is the operation where the CPU "saves the current task's registers
and restores another task's registers."
In this OS, it happens automatically at interrupt/syscall entry and exit points.

---

## Overall Flow

```
When user task A (Ring 3) calls slp_tsk():

Task A (Ring 3)
    |
    | int $0x99
    v
+-- intr_syscall -----------------------------------+
|   SAVE_ALL         <- push all registers          |
|   call intr_enter  <- k_nest++                    |
|   push %esp        <- pass pt_regs* as argument   |
|   call c_intr_syscall                             |
|     -> itron_syscall                              |
|       -> sys_slp_tsk                              |
|         -> sched_next_tsk  (sets flag only)       |
|   jmp intr_return                                 |
|     call intr_leave <- k_nest--, switch ESP       |
|     RESTORE_ALL    <- pop all registers           |
|     iret           <- return to Task B            |
+---------------------------------------------------+
Task B (Ring 3)
```

Key points:
- Kernel C code runs between `SAVE_ALL` and `RESTORE_ALL`
- The task switch occurs inside `intr_leave`: by switching ESP,
  `RESTORE_ALL` restores the new task's registers
- If no switch occurs, the same stack's registers are restored as-is

---

## Per-Task Kernel Stack

Each task has a 4KB kernel stack (kern_stack).
When a Ring 3 to Ring 0 interrupt occurs, the CPU reads the stack pointer from TSS.esp0
and switches to that task's kernel stack. TSS.esp0 is always set to
`kern_stack_top` (the top address of the stack, i.e., the empty state).
Since the kernel stack is not in use while running in Ring 3,
each time an interrupt occurs, the CPU starts using the stack fresh from `kern_stack_top`.

```
proc_t structure:
  kern_esp       <- saved kernel ESP (top of the register frame)
  kern_stack_top <- top of kernel stack (value set in TSS.esp0)
  saved_eflags   <- for proc_eflags_save/restore
  cpu            <- CPU affinity (0 or 1)
```

Task switch mechanism:
1. `intr_leave` saves the current ESP to `current_proc[cpu]->kern_esp`
2. `sched_next_tsk_check()` changes `current_proc[cpu]` to the new task
3. The new task's `kern_esp` is loaded into ESP
4. TSS.esp0 is updated to the new task's `kern_stack_top`
5. `RESTORE_ALL` pops the registers from the new task's stack
6. `iret` returns to the new task's Ring 3

This is the same pattern used in general-purpose kernels such as Linux.

---

## SAVE_ALL in Detail

### Prerequisite: Interrupt Frame

When a Ring 3 to Ring 0 interrupt/syscall occurs, the CPU automatically pushes
the following onto the task's kernel stack (TSS SS0:ESP0):

```
High address
    +----------------+
    | SS (user)      |   * CPU pushes (Ring 3->0 only)
    | ESP (user)     |   * CPU pushes (Ring 3->0 only)
    | EFLAGS         |
    | CS             |
    | EIP (return)   |
    +----------------+
    |                | <- ESP (SAVE_ALL starts pushing from here)
Low address
```

When returning from an interrupt, the `iret` instruction performs the reverse.
It pops EIP, CS, and EFLAGS from the kernel stack, and if returning to Ring 3,
also pops ESP and SS. In other words, **`iret` pops the frame that the CPU pushed
at the interrupt entry, returning to the interrupted instruction**.

### SAVE_ALL Macro

```asm
.macro SAVE_ALL
    pushl   %eax
    pushl   %ecx
    pushl   %edx
    pushl   %ebx
    pushl   %ebp
    pushl   %esi
    pushl   %edi
    pushl   %ds
    pushl   %es
    movw    $0x28, %ax      # SEL_K32_D (kernel data segment)
    movw    %ax, %ds
    movw    %ax, %es
.endm
```

After pushing 9 registers, DS/ES are reloaded with the kernel segment.
After SAVE_ALL, ESP points to the beginning of the pt_regs frame.

### pt_regs Frame Layout

```
Offset      Register     Pushed by
--------------------------------------------
0x00         ES          SAVE_ALL
0x04         DS          SAVE_ALL
0x08         EDI         SAVE_ALL
0x0C         ESI         SAVE_ALL
0x10         EBP         SAVE_ALL
0x14         EBX         SAVE_ALL
0x18         EDX         SAVE_ALL
0x1C         ECX         SAVE_ALL
0x20         EAX         SAVE_ALL
0x24         EIP         CPU (interrupt frame)
0x28         CS          CPU (interrupt frame)
0x2C         EFLAGS      CPU (interrupt frame)
0x30         ESP         CPU (Ring 3->0 only)
0x34         SS          CPU (Ring 3->0 only)
```

This structure is also accessible from C code as `struct pt_regs` in `proc.h`.

---

## intr_enter in Detail

```asm
intr_enter:
    movl    APIC_ID_REG, %eax    # Read 0xFEE00020 to determine CPU
    shrl    $24, %eax            # Bits 24-31 -> APIC ID (0 or 1)
    testl   %eax, %eax
    jnz     1f
    incl    k_nest0              # CPU 0 nest counter++
    ret
1:
    incl    k_nest1              # CPU 1 nest counter++
    ret
```

This tracks the interrupt nesting depth. When k_nest goes from 0 to 1, it is the first interrupt.

> **In the current implementation, nested interrupts do not occur.** All interrupt handlers
> are called via interrupt gates (automatic IF clear), and no handler calls `sti`,
> so k_nest only ever transitions 0->1->0.

---

## intr_leave in Detail (The Heart of Task Switching)

```asm
intr_leave:
    # Determine CPU by APIC ID
    movl    APIC_ID_REG, %eax
    shrl    $24, %eax
    testl   %eax, %eax
    jnz     intr_leave_cpu1

intr_leave_cpu0:
    decl    k_nest0
    jnz     intr_leave_done      # Nested -> skip

    # -- Return from outermost interrupt (CPU 0) --

    # 1. Save current ESP
    movl    current_proc, %ebx   # %ebx = current_proc[0]
    movl    %esp, (%ebx)         # current_proc[0]->kern_esp = ESP

    # 2. Call scheduler (current_proc may change)
    pushl   $0
    call    sched_next_tsk_check
    addl    $4, %esp

    # 3. Load ESP from (new) current_proc
    movl    current_proc, %ebx   # %ebx = new current_proc[0]
    movl    (%ebx), %esp         # ESP = new current_proc[0]->kern_esp

    # 4. Update TSS.esp0 to new task's kernel stack top
    pushl   4(%ebx)              # kern_stack_top
    pushl   $0                   # cpu = 0
    call    tss_update_esp0
    addl    $8, %esp

    ret

intr_leave_done:
    ret
```

### Why Does Switching ESP Alone Achieve a Task Switch?

The key insight: **a complete register frame (pt_regs) is saved on each task's
kernel stack.** Simply switching ESP to point to the register frame on the new
task's kernel stack is enough -- the subsequent `RESTORE_ALL` and `iret` will
correctly restore the new task's registers.

```
What a task switch looks like:

Task A's kernel stack:            Task B's kernel stack:
+--------------------+            +--------------------+
| SS  (Task A)       |            | SS  (Task B)       |
| ESP (Task A)       |            | ESP (Task B)       |
| EFLAGS             |            | EFLAGS             |
| CS                 |            | CS                 |
| EIP (Task A)       |            | EIP (Task B)       |
| EAX                |            | EAX                |
| ...                |            | ...                |
| ES                 | <- old ESP | ES                 | <- new ESP
+--------------------+            +--------------------+

intr_leave switches ESP:
  old ESP (Task A) -> new ESP (Task B)

The subsequent RESTORE_ALL pops Task B's registers from the new ESP:
  pop %es, %ds, %edi, ..., %eax

The final iret pops Task B's EIP/ESP/CS/EFLAGS/SS:
  -> returns to Task B's user space
```

---

## RESTORE_ALL in Detail

```asm
.macro RESTORE_ALL
    popl    %es
    popl    %ds
    popl    %edi
    popl    %esi
    popl    %ebp
    popl    %ebx
    popl    %edx
    popl    %ecx
    popl    %eax
.endm
```

Pops 9 registers in the reverse order of SAVE_ALL.
After this, ESP points to the CPU-pushed interrupt frame (EIP, CS, EFLAGS, ESP, SS),
and `iret` pops those to return to user space.

---

## Common Return Path (intr_return)

All interrupt/syscall handlers converge via `jmp intr_return`:

```asm
intr_return:
    call    intr_leave           # k_nest--, task switch
    # Fall through
intr_return_restore:
    RESTORE_ALL
    iret
```

In the normal path (returning to an already-running task), `call intr_leave`
automatically pushes the return address onto the stack, so the `ret` at the end
of `intr_leave` naturally returns to `intr_return_restore`.

**For a new task's first startup**, the task has never received an interrupt yet,
so there is no return address or register frame on the stack.
Therefore, `proc_create()` (`proc.c:91-128`) pre-builds a complete fake frame
on the kernel stack:

```
  (Low address = ESP)
  +-------------------------------+
  | address of intr_return_restore | <- popped by ret
  +-------------------------------+
  | ES, DS, EDI..EAX (all zero)   | <- popped by RESTORE_ALL
  +-------------------------------+
  | EIP = task's entry point       | <- loaded into EIP by iret
  | CS  = Ring 3 code segment      |
  | EFLAGS (IF=1)                  |
  | ESP = top of user stack        |
  | SS  = Ring 3 stack segment     |
  +-------------------------------+
  (High address = bottom of stack)
```

When ESP is switched to this kernel stack and `ret` is executed:

1. `ret` pops `intr_return_restore` -> proceeds to `RESTORE_ALL` + `iret`
2. `RESTORE_ALL` pops ES, DS, EDI through EAX to initialize registers
3. `iret` pops EIP, CS, EFLAGS, ESP, SS -> execution begins at the Ring 3 task entry point

> The source of the ESP switch varies by situation. For the first two tasks,
> `start_first_task` / `start_second_task` (`klib.s`) load ESP directly.
> For subsequent tasks, `intr_leave` switches ESP as part of the task switch.

---

## Full Trace of the Syscall Path

Example: Task 1 calls `slp_tsk()` and switches to Task 3.

### 1. User Space: slp_tsk() Call

```c
// lib/lib_tsk.c
ER slp_tsk(void) {
    return syscall(-TFN_SLP_TSK);
    // -> issues int $0x99
}
```

> **Note**: `syscall()` receives the TFN as a **negative** value. The `neg` instruction
> at the syscall entry point in `klib.s` converts it to a positive value, which becomes
> the index into the function table.

### 2. CPU: Interrupt Frame Generation

`int $0x99` is a software interrupt, but the CPU's behavior is identical to a
hardware interrupt. It references IDT entry 0x99, and since a Ring 3 to Ring 0
privilege transition occurs, the CPU obtains the kernel stack from TSS SS0:ESP0
and automatically pushes the following registers:

```
  SS (0x6B, user stack)
  ESP (Task 1's stack pointer)
  EFLAGS
  CS (0x5B, user code)
  EIP (instruction after int $0x99)
```

### 3. intr_syscall: SAVE_ALL + Argument Passing

```asm
intr_syscall:
    SAVE_ALL                       # Push all registers -> pt_regs frame complete
    call    intr_enter             # k_nest: 0->1
    pushl   %esp                   # arg: pass pt_regs* to C function
    call    c_intr_syscall
    addl    $4, %esp
    jmp     intr_return
```

Since ESP after SAVE_ALL points to the pt_regs structure, it is passed directly as a pointer.

#### Why Can a C Structure Read and Write Registers on the Stack?

The technique used here is **overlaying a structure onto the stack.**
The registers pushed onto the kernel stack by the CPU and SAVE_ALL at interrupt entry
are laid out contiguously in memory with no gaps. By making the field order of
`struct pt_regs` match this layout, simply passing ESP as a `struct pt_regs *`
to a C function allows structure field accesses to directly read and write the
register values on the stack.

```
Kernel stack (low address at top)            struct pt_regs (proc.h)
-----------------------------------------------------------------
                                          struct pt_regs {
ESP+0x00 | ES          | pushed by SAVE_ALL    unsigned long es;
ESP+0x04 | DS          | pushed by SAVE_ALL    unsigned long ds;
ESP+0x08 | EDI         | pushed by SAVE_ALL    unsigned long edi;
ESP+0x0C | ESI         | pushed by SAVE_ALL    unsigned long esi;
ESP+0x10 | EBP         | pushed by SAVE_ALL    unsigned long ebp;
ESP+0x14 | EBX         | pushed by SAVE_ALL    unsigned long ebx;
ESP+0x18 | EDX         | pushed by SAVE_ALL    unsigned long edx;
ESP+0x1C | ECX         | pushed by SAVE_ALL    unsigned long ecx;
ESP+0x20 | EAX         | pushed by SAVE_ALL    unsigned long eax;
         +-------------+                     /* pushed by CPU */
ESP+0x24 | EIP         | pushed by CPU         unsigned long eip;
ESP+0x28 | CS          | pushed by CPU         unsigned long cs;
ESP+0x2C | EFLAGS      | pushed by CPU         unsigned long eflags;
ESP+0x30 | ESP (user)  | pushed by CPU (*)     unsigned long esp;
ESP+0x34 | SS  (user)  | pushed by CPU (*)     unsigned long ss;
         +-------------+                  };
                        (*) CPU pushes only on Ring 3->0 privilege transition
```

What `pushl %esp` passes is the starting address of this frame, so
the C function can read and write the value at stack offset 0x20 simply by writing
`regs->eax`. No special memory allocation or copying is needed.

### 4. c_intr_syscall -> sys_slp_tsk

```c
// i386/syscall.c
void c_intr_syscall(struct pt_regs *regs) {
    // Read syscall number and arguments from regs
    W ret = itron_syscall(apic, regs->eax, ...);
    // Write return value to the EAX slot of pt_regs
    regs->eax = ret;
}
```

By writing the return value to the EAX slot of pt_regs, when `RESTORE_ALL`
performs `pop %eax`, the return value ends up in the user's EAX.

> **Why EAX?** -- In the i386 C calling convention (cdecl), function return values
> are passed in the EAX register. The user-side wrapper function
> `slp_tsk()` -> `syscall()` simply `return`s the value of EAX after returning
> from `int $0x99`. The compiler generates code that assumes the `return` value
> is in EAX, so by writing the return value to `regs->eax` on the kernel side,
> it arrives at the user task as a normal function return value.

### 5. sys_slp_tsk: Task State Change

```c
// kernel/sys_tsk.c
ER sys_slp_tsk(W apic) {
    ID tskid = c_tskid[apic];    // = 1

    /* caller holds kernel_lk (acquired in c_intr_syscall) */
    sched_rem(&tsk[1].plink);     // Remove from priority queue
    tsk[1].tskstat = TTS_WAI;     // Task 1 -> WAI state
    sched_next_tsk(apic);         // next_tsk_flag[0] = next_tsk_flag[1] = 1
    return E_OK;
}
```

`sched_next_tsk()` sets the reschedule flag for both CPUs regardless of the `apic` argument.
This design is necessary for cross-CPU task wakeup (e.g., waking a task on CPU 1 from IRQ1 on CPU 0).
See the [sched_next_tsk reference](refs/kernel/sched.md#sched_next_tsk) for details and examples.
The actual switch is performed in `intr_leave`.

### 6. intr_return -> intr_leave

```
intr_leave (CPU 0):
  k_nest0: 1->0

  # Save ESP to current_proc[0]->kern_esp
  # (top of pt_regs on Task 1's kernel stack)

  sched_next_tsk_check(0):
    -> sched_do_next_tsk(0)
      -> finds Task 3, current_proc[0] = &proc[3]

  # Load ESP from proc[3].kern_esp
  # (top of pt_regs on Task 3's kernel stack)

  # TSS.esp0 = proc[3].kern_stack_top updated
```

### 7. RESTORE_ALL + iret -> To Task 3

Since ESP now points to Task 3's kernel stack,
`RESTORE_ALL` pops Task 3's registers, and
`iret` returns to Task 3's EIP/ESP.

---

## When No Task Switch Occurs

Example: APIC timer interrupt (when no task switch is needed)

```
Task 2 running
    | APIC timer interrupt
    v
intr_smp_timer1:
    SAVE_ALL                     # Push Task 2's registers
    call intr_enter              # k_nest1: 0->1
    call c_intr_smp_timer1       # EOI only (preemption trigger)
    jmp  intr_return
        call intr_leave          # k_nest1: 1->0
                                 # Save ESP -> sched_next_tsk_check
                                 # -> no change -> load same ESP
        RESTORE_ALL              # Pop same Task 2's registers
        iret                     # Return to Task 2
```

Since ESP is saved and then immediately loaded with the same value,
the stack state is effectively unchanged before and after SAVE_ALL/RESTORE_ALL.

> **Note: A task switch can still occur on a timer interrupt.**
> The example above is for the case when `next_tsk_flag[1]` is 0.
> If another CPU or handler has set `next_tsk_flag[1] = 1` via `sched_next_tsk()`,
> then `sched_next_tsk_check()` will call `sched_do_next_tsk()`, and a task switch
> will occur. This is the OS's preemption mechanism -- for example, switching to
> Task 4 (CPU 1) that was woken by a keyboard interrupt (CPU 0) happens in
> `intr_leave` of CPU 1's timer interrupt.
> See the [sched_next_tsk reference](refs/kernel/sched.md#sched_next_tsk) for details.

---

## First Task Startup (ltr + iret Method)

The first task startup also uses the same RESTORE_ALL + iret path as subsequent switches.
Hardware TSS switching (ljmp) is not used.

```asm
# i386/klib.s
start_first_task:
    movw    $0x38, %ax          # SEL_TSS0
    ltr     %ax                 # Load into Task Register (no register exchange)
    movl    current_proc, %ebx  # current_proc[0] = &proc[1]
    movl    (%ebx), %esp        # ESP = proc[1].kern_esp
    ret                         # -> intr_return_restore -> RESTORE_ALL -> iret

start_second_task:
    movw    $0x40, %ax          # SEL_TSS1
    ltr     %ax                 # Load into Task Register
    movl    current_proc+4, %ebx
    movl    (%ebx), %esp        # ESP = proc[2].kern_esp
    ret                         # -> intr_return_restore -> RESTORE_ALL -> iret
```

`ltr` only loads the TSS selector into the Task Register -- it does not save or
restore registers. The CPU will use this TSS's esp0/ss0 for the next Ring 3 to
Ring 0 interrupt.

Since `proc_create()` has built a fake pt_regs frame + the return address of
`intr_return_restore` on the kernel stack, `ret` -> `RESTORE_ALL` -> `iret`
transitions to Ring 3.

This approach ensures that both the initial startup and normal task switches
follow the same code path.

### Initial Frame Built by proc_create

`proc_create()` builds a "fake" interrupt frame on the new task's kernel stack.
This allows the task to start on the first `intr_leave` -> `RESTORE_ALL` + `iret`:

```
Kernel stack (high address at top):
+--------------------+ <- kern_stack_top (= TSS.esp0)
| SS  (SEL_U32_S)   |   <- iret pops -> Ring 3 SS
| ESP (user stack)   |   <- iret pops -> Ring 3 ESP
| EFLAGS (IF=1)     |   <- iret pops -> interrupts enabled
| CS  (SEL_U32_C)   |   <- iret pops -> Ring 3 CS
| EIP (task entry)   |   <- iret pops -> task's entry point
| EAX = 0            |
| ECX = 0            |
| EDX = 0            |
| EBX = 0            |
| EBP = 0            |
| ESI = 0            |
| EDI = 0            |
| DS  (SEL_U32_D|3)  |
| ES  (SEL_U32_D|3)  |   <- kern_esp (RESTORE_ALL pops from here)
+--------------------+
| intr_return_restore |  <- where intr_leave's ret jumps to
+--------------------+
```

---

## Nested Interrupt Design and Current Status

### Design: Infrastructure Exists for Nested Interrupts

`intr_enter`/`intr_leave` are designed with nested interrupts in mind:

- `k_nest0`/`k_nest1` track the interrupt nesting depth
- Since pushes go onto each task's kernel stack, nested frames are
  stored independently
- Inner `intr_leave` calls do not perform a task switch (k_nest > 0).
  Only the outermost `intr_leave` considers switching

### Current Status: Nested Interrupts Do Not Occur

However, in the current implementation, **nested interrupts never occur**. The reasons are:

1. **All interrupts go through interrupt gates (GT_INTR)** --
   The CPU automatically clears IF, so interrupts are disabled upon handler entry
2. **No handler calls `sti`** --
   The entire handler executes with interrupts disabled
3. **Interrupts are only re-enabled at the moment of `iret`** --
   `iret` atomically restores EFLAGS (IF=1)

Therefore, k_nest only ever transitions 0->1->0.

---

## Common Pitfalls

### 1. Forgetting to Update TSS.esp0 Causes Corruption

If TSS.esp0 is not updated to the new task's `kern_stack_top` after a task switch,
the next interrupt will cause the CPU to switch to the old task's kernel stack,
resulting in stack corruption. The `tss_update_esp0()` call in `intr_leave` is critical.

### 2. sched_next_tsk Only Sets a Flag

`sched_next_tsk()` only sets `next_tsk_flag[0] = next_tsk_flag[1] = 1` -- it does
not perform the actual switch. The switch is performed by `sched_next_tsk_check()`
-> `sched_do_next_tsk()`, which is called from `intr_leave`.

### 3. Syscall Return Values Go into regs->eax

```c
regs->eax = ret;  // Write to the EAX slot of the pt_regs frame
```

When RESTORE_ALL performs `pop %eax`, this value is loaded into EAX.
When writing the return value from elsewhere in the kernel, use `proc_set_return_value()`:

```c
void proc_set_return_value(proc_t *p, unsigned long val) {
    struct pt_regs *regs = (struct pt_regs *)(p->kern_esp + 4);
    regs->eax = val;
}
```

`kern_esp + 4` is the offset needed to skip the return address (`intr_return_restore`)
that was pushed by `call intr_leave` in `intr_return`.
When `intr_leave` returns via `ret`, it lands at `intr_return_restore`, but
`kern_esp` points below that return address (= the start of the pt_regs that
`RESTORE_ALL` will pop), so +4 is needed to access pt_regs.

---

## Related Documents

- [Source Code Reading Guide](source-guide.md) -- Overview of all files
- [System Overview](system-overview.md) -- Cooperative behavior between tasks
- [i386 Architecture](i386-architecture.md) -- Privilege levels, TSS
- [SMP Basics](smp-basics.md) -- Per-CPU variables, spinlocks
