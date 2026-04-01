# Timer Interrupt Processing Flow Guide

This document provides a step-by-step walkthrough of the entire sequence from when
a timer interrupt fires while a user task is running, through register saving,
C handler execution, scheduling, and return to user mode (or switching to a
different task).

---

## 1. Overview

Timer interrupts are the heartbeat of the OS and serve the following roles:

- **Scheduling driver**: Waking up tasks that are waiting with a timeout
- **Preemption trigger**: Switching to a higher-priority task upon interrupt return
- **Timekeeping**: Incrementing `timer_ticks` and updating the tick display on screen

tiny-itron has **two timer subsystems**:

| Timer        | Chip       | CPU   | Vector              | Period              |
|-------------|------------|-------|---------------------|---------------------|
| PIT (IRQ0)  | 8254       | CPU 0 | 0x80 (VECT_IRQ0)   | ~16.7ms (FREQ/HZ)  |
| APIC Timer  | Local APIC | CPU 0 | 0x9A (VECT_SMP_TIMER0) | Depends on MAX_TIMER_COUNT |
| APIC Timer  | Local APIC | CPU 1 | 0x9B (VECT_SMP_TIMER1) | Depends on MAX_TIMER_COUNT |

The PIT is routed through the i8259 PIC, so it is delivered only to CPU 0.
The APIC timer is built into each CPU's Local APIC and operates independently per CPU.

---

## 2. Timer Initialization

### 2.1 PIT (Programmable Interval Timer)

The 8254 PIT uses channel 0 in square wave mode (mode 3).

```c
/* i386/timerP.h */
#define FREQ    1193182L    /* PIT input clock frequency (Hz) */
#define SQUARE  0x36        /* Channel 0, mode 3 (square wave), 16-bit */
#define HZ      60          /* Target interrupt rate */

/* i386/timer.c */
void timer_init(void)
{
    unsigned long count;
    count = FREQ / HZ;                  /* = 19886 (0x4DAE) */
    outb(IO_TIMER_C, SQUARE);           /* Port 0x43: control word */
    outb(IO_TIMER_0, count & 0xff);     /* Port 0x40: count low byte */
    outb(IO_TIMER_0, (count >> 8) & 0xff); /* Count high byte */
}
```

With a count value of `1193182 / 60 = 19886`, IRQ0 fires approximately every
16.7ms (roughly 60Hz). The PIT output is connected to IR0 of the i8259 master PIC,
and the interrupt is delivered to CPU 0 at vector 0x80.

### 2.2 APIC Timer

Each CPU's Local APIC has a built-in timer. When configured in Periodic mode,
it decrements from the initial count value down to 0, fires an interrupt when
it reaches 0, and automatically reloads the count value to repeat the cycle.

```c
/* i386/smpP.h */
#define MAX_TIMER_COUNT    0x00080000   /* = 524288 */
#define APIC_TIMER_DIV_16  0x03         /* Divider: bus clock / 16 */
#define APIC_TIMER_PERIODIC 0x20000     /* LVT Timer Periodic bit */

/* i386/smp.c -- BSP (CPU 0) APIC timer initialization */
apic_write(APIC_TIMER_DIV, APIC_TIMER_DIV_16);
apic_write(APIC_LVT_TIMER, APIC_TIMER_PERIODIC | VECT_SMP_TIMER0);
apic_write(APIC_TIMER_INIT_COUNT, MAX_TIMER_COUNT);

/* smp_ap_init() -- AP (CPU 1) APIC timer initialization */
apic_write(APIC_TIMER_DIV, APIC_TIMER_DIV_16);
apic_write(APIC_LVT_TIMER, APIC_TIMER_PERIODIC | VECT_SMP_TIMER1);
apic_write(APIC_TIMER_INIT_COUNT, MAX_TIMER_COUNT);
```

CPU 0 receives interrupts at vector 0x9A, CPU 1 at vector 0x9B.
With 0x80000 = 524288 counts, this corresponds to roughly 100Hz under QEMU.

---

## 3. Interrupt Firing -- CPU Automatic Processing

When a timer interrupt fires while a user task (Ring 3) is executing,
the CPU performs the following steps **automatically in hardware**:

### 3.1 Privilege Level Switch

1. Read SS0 and ESP0 from the TSS (Task State Segment)
2. Switch the stack pointer to the Ring 0 kernel stack
3. Read the gate descriptor from the IDT (Interrupt Descriptor Table)
4. Since the gate type is an Interrupt Gate (GT_INTR), clear the IF (Interrupt Flag)
   -- interrupts are now disabled

### 3.2 Automatic Interrupt Frame Push

The CPU automatically pushes 20 bytes (5 doublewords) onto the Ring 0 stack:

```
  Ring 0 stack (grows downward from TSS ESP0):

  High address
  +--------------------+
  | SS   (Ring 3)      |  ESP+16
  | ESP  (Ring 3)      |  ESP+12   <- User-mode stack pointer
  | EFLAGS             |  ESP+8    <- Saved flags (IF=1 was set)
  | CS   (Ring 3)      |  ESP+4    <- 0x5B (user code segment)
  | EIP  (return addr) |  ESP+0    <- Address of interrupted instruction
  +--------------------+
  Low address  <- ESP points here
```

**Important**: The SS/ESP push only occurs during a Ring 3 to Ring 0 privilege
level change. For Ring 0 to Ring 0 interrupts, SS/ESP are not pushed. This is why
all tasks must run in Ring 3 -- if an interrupt occurs in Ring 0, the pt_regs frame
on the kernel stack would lack SS/ESP, and `iret` would be unable to restore the
stack properly.

### 3.3 IDT Entry

For IRQ0 (PIT):

```c
/* i386/interrupt.c */
set_idt(VECT_IRQ0, (unsigned long)intr_irq0, SEL_K32_C, 0, GT_INTR);
/*      Vector 0x80  Handler address            Kernel CS    Interrupt gate */
```

Since GT_INTR (Interrupt Gate) is used, EFLAGS.IF is automatically cleared when
passing through the interrupt gate. This prevents another timer interrupt from
firing during SAVE_ALL/RESTORE_ALL execution.

---

## 4. SAVE_ALL and intr_enter

At the beginning of every interrupt handler, the `SAVE_ALL` macro saves registers
onto the kernel stack, and `intr_enter` increments the nesting counter.

### 4.1 Assembly Stub (IRQ0 Example)

```asm
# i386/intr.s
intr_irq0:
    SAVE_ALL                # Save registers (build pt_regs frame)
    call    intr_enter      # k_nest++
    call    c_intr_irq0     # Call C handler
    jmp     intr_return     # intr_leave + RESTORE_ALL + iret
```

### 4.2 SAVE_ALL Processing

SAVE_ALL pushes 9 registers onto the kernel stack and reloads DS/ES to kernel
segments:

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

Below the interrupt frame pushed by the CPU (EIP, CS, EFLAGS, ESP, SS), SAVE_ALL
adds 9 more registers, forming the complete pt_regs frame:

```
Kernel stack (after SAVE_ALL):

  Offset  Register    Pushed by
  ------  --------    ---------
  0x00    ES          SAVE_ALL
  0x04    DS          SAVE_ALL
  0x08    EDI         SAVE_ALL
  0x0C    ESI         SAVE_ALL
  0x10    EBP         SAVE_ALL
  0x14    EBX         SAVE_ALL
  0x18    EDX         SAVE_ALL
  0x1C    ECX         SAVE_ALL
  0x20    EAX         SAVE_ALL
  0x24    EIP         CPU (address of interrupted instruction)
  0x28    CS          CPU (0x5B = Ring 3 code segment)
  0x2C    EFLAGS      CPU (pre-interrupt flags, including IF=1)
  0x30    ESP         CPU (Ring 3 stack pointer)
  0x34    SS          CPU (0x6B = Ring 3 stack segment)
```

During the Ring 3 to Ring 0 transition, the CPU automatically switches CS (from the
IDT gate) and SS (from TSS.ss0) to kernel segments, but it does not modify DS or ES.
The `movw $0x28, %ax / movw %ax, %ds / movw %ax, %es` at the end of SAVE_ALL
ensures that all segment registers point to kernel segments while kernel C code
executes:

- CS = 0x20 (K32_C) -- automatically set by the CPU from the IDT gate
- SS = 0x30 (K32_S) -- automatically set by the CPU from TSS.ss0
- DS = 0x28 (K32_D) -- reloaded by SAVE_ALL
- ES = 0x28 (K32_D) -- reloaded by SAVE_ALL

### 4.3 intr_enter -- Incrementing the Nesting Counter

```asm
intr_enter:
    movl    APIC_ID_REG, %eax    # 0xFEE00020: APIC ID register
    shrl    $24, %eax            # APIC ID in bits[31:24]
    testl   %eax, %eax
    jnz     1f
    incl    k_nest0              # CPU 0: k_nest0++
    ret
1:
    incl    k_nest1              # CPU 1: k_nest1++
    ret
```

Each CPU has its own independent nesting counter:

| CPU | Nesting Counter | current_proc    |
|-----|-----------------|-----------------|
| 0   | `k_nest0`      | `current_proc[0]` |
| 1   | `k_nest1`      | `current_proc[1]` |

---

## 5. C Interrupt Handlers

### 5.1 PIT Timer (IRQ0, CPU 0 Only)

```c
/* i386/interrupt.c */
void c_intr_irq0(void)
{
    smp_lock(&kernel_lk);  /* Acquire Big Kernel Lock */
    timer_intr(0, 1);      /* apic=0, delta=1 tick */
    i8259_reenable();       /* Send EOI to PIC + APIC */
    smp_unlock(&kernel_lk);
}
```

### 5.2 APIC Timer (CPU 0)

```c
void c_intr_smp_timer0(void)
{
    smp_eoi();             /* APIC EOI only (PIC not needed) */
}
```

The CPU 0 APIC timer does not call `timer_intr`. Since the PIT already handles
tick processing, the APIC timer only serves to check `next_tsk_flag` (which
happens in `sched_next_tsk_check` within `intr_leave`).

### 5.3 APIC Timer (CPU 1)

```c
void c_intr_smp_timer1(void)
{
    smp_eoi();             /* APIC EOI only */
}
```

Like CPU 0, the CPU 1 APIC timer does not call `timer_intr` either.
The timeout queue delta subtraction is performed solely by CPU 0's PIT;
the APIC timers only provide a trigger for preemptive task switching
(`intr_leave` -> `sched_next_tsk_check`). If both timers called
`sched_timeout`, the deltas would be subtracted twice, causing inaccurate
timeout durations.

### 5.4 timer_intr -- PIT Timer Interrupt Processing

```c
/* i386/timer.c */
void timer_intr(unsigned char apic, unsigned long delta)
{
    if (apic == 0) {
        timer_ticks++;
        vga_write_dec_at(3, 21, timer_ticks, 10, 0x0B);  /* Update display */
    }
    sched_timeout(apic, delta);    /* Process timeouts */
}
```

### 5.5 sched_timeout -- Waking Up Timed-Out Tasks

```c
/* kernel/sched.c (simplified) */
void sched_timeout(W apic, unsigned long delta)
{
    tp = timeout.next;
    if (tp == &timeout) return;

    tp->delta -= delta;

    /* Loop through all expired entries (including simultaneous delta=0 expirations) */
    while (tp != &timeout && tp->delta <= 0) {
        next = tp->next;
        /* Delta correction: propagate remainder to next entry */
        if (next != &timeout) next->delta += tp->delta;
        /* Remove from list, reset to self-referencing */
        tp->prev->next = next; next->prev = tp->prev;
        tp->next = tp; tp->prev = tp;

        tsk_ptr = tlink2tsk(tp);
        if (tsk_ptr->tskstat == TTS_WAI) {
            /* Also remove from object wait queue */
            if (tsk_ptr->wlink.next != &(tsk_ptr->wlink))
                wlink_rem(&(tsk_ptr->wlink));
            proc_set_return_value(tsk_ptr->proc, E_TMOUT);
            tsk_ptr->tskstat = TTS_RDY;
            sched_ins(tsk_ptr->tskpri, &(tsk_ptr->plink));
            woken = 1;
        }
        tp = timeout.next;
    }
    if (woken) sched_next_tsk(apic);
}
```

See [Timeout Processing](timeout.md) for details.

`sched_next_tsk` **does not perform the actual task switch**. It only sets
`next_tsk_flag[]` to 1 for both CPUs. The actual switch is carried out by
`sched_next_tsk_check` within `intr_leave`.

### 5.6 EOI (End of Interrupt) Delivery

**PIC (i8259) EOI:**

```c
/* i386/i8259.c */
void i8259_reenable(void)
{
    outb(IO_I8259_M, 0x20);    /* Non-specific EOI to master PIC */
    outb(IO_I8259_S, 0x20);    /* Non-specific EOI to slave PIC */
    smp_eoi();                  /* Also send APIC EOI */
}
```

**APIC EOI:**

```c
/* i386/smp.c */
void smp_eoi(void)
{
    volatile unsigned long *eoi = (volatile unsigned long *)0xFEE000B0;
    *eoi = 0;    /* Simply write 0 to the APIC EOI register */
}
```

If EOI is not sent, the PIC/APIC will continue blocking interrupts at or below
that priority level. It is important to send EOI at the **end** of the C handler,
which prevents new interrupts from arriving before the EOI.

---

## 6. intr_leave and RESTORE_ALL

When the C handler returns, execution enters the common return path `intr_return`:

```asm
intr_return:
    call    intr_leave          # (1) Nesting management + task switch
intr_return_restore:
    RESTORE_ALL                 # (2) Restore registers
    iret                        # (3) Return to Ring 3
```

### 6.1 intr_leave -- Nesting Counter Decrement and Task Switch

#### Step 1: CPU Identification and Nesting Counter Decrement

```asm
intr_leave:
    movl    APIC_ID_REG, %eax    # 0xFEE00020
    shrl    $24, %eax
    testl   %eax, %eax
    jnz     intr_leave_cpu1

intr_leave_cpu0:
    decl    k_nest0
    jnz     intr_leave_done      # Still nested -> no task switch
```

If the nesting counter is non-zero (returning from a nested interrupt), the task
switch decision is skipped and execution proceeds directly to RESTORE_ALL. Switching
tasks in the middle of a nested interrupt would corrupt the state of the outer
interrupt handler.

#### Step 2: Save Current Task's ESP

```asm
    movl    current_proc, %ebx   # EBX = current_proc[0] (proc_t*)
    movl    %esp, (%ebx)         # proc->kern_esp = ESP
```

The current ESP is saved to the task's `kern_esp`. ESP points to the pt_regs frame
on the kernel stack, which contains the complete saved register state for this task.

#### Step 3: sched_next_tsk_check -- Task Switch Decision

```asm
    pushl   $0                   # Argument: apic = 0
    call    sched_next_tsk_check
    addl    $4, %esp
```

C implementation of `sched_next_tsk_check`:

```c
/* i386/interrupt.c */
int sched_next_tsk_check(int apic)
{
    if (next_tsk_flag[apic] != 0) {
        old_proc = current_proc[apic];
        sched_do_next_tsk(apic);        /* Select highest-priority RDY task */
        next_tsk_flag[apic] = 0;
        if (old_proc != current_proc[apic])
            return 1;                   /* Task switch occurred */
    }
    return 0;                           /* No switch */
}
```

`sched_do_next_tsk` scans the priority queues and finds the highest-priority
TTS_RDY task with matching CPU affinity, then updates `current_proc[apic]`.

#### Step 4: Load New Task's ESP + Update TSS.esp0

```asm
    movl    current_proc, %ebx   # EBX = (possibly new) current_proc[0]
    movl    (%ebx), %esp         # ESP = proc->kern_esp (new task's stack)

    pushl   4(%ebx)              # kern_stack_top
    pushl   $0                   # cpu = 0
    call    tss_update_esp0      # Set TSS.esp0 to new task's kernel stack top
    addl    $8, %esp
    ret                          # -> returns to intr_return_restore
```

If a task switch occurred, `current_proc[cpu]` now points to the **new task's**
proc_t. Since ESP is switched to the new task's `kern_esp`, the subsequent
RESTORE_ALL will pop the pt_regs from the new task's kernel stack.

`tss_update_esp0` updates TSS.esp0 so that the next time an interrupt occurs
while this new task is running, the CPU will switch to the correct kernel stack.

### 6.2 RESTORE_ALL -- Register Restoration

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

RESTORE_ALL pops 9 registers in the reverse order of SAVE_ALL. At this point,
ESP points to the beginning of the pt_regs frame (offset 0x00 = ES), and after
the pops, only the CPU-pushed interrupt frame (EIP, CS, EFLAGS, ESP, SS) remains
on the stack.

### 6.3 iret -- Return to Ring 3

`iret` performs the following:
1. Pop EIP, CS, and EFLAGS from the stack
2. Since this is a Ring 0 to Ring 3 privilege transition, additionally pop ESP and SS
3. The saved EFLAGS contained IF=1, so interrupts are re-enabled
4. User task execution resumes

---

## 7. Branching Based on Whether a Task Switch Occurs

### 7.1 No Switch -- Return to Same Task

A timer interrupt fires, but there are no tasks to wake up and no higher-priority
tasks ready to run:

```
  SAVE_ALL:     Push Task A's registers onto kernel stack A
  intr_enter:   k_nest++
  C handler:    timer_intr -> sched_timeout (no change)
  intr_leave:   k_nest-- (=0)
                Save ESP to kern_esp
                sched_next_tsk_check -> 0 (no switch)
                Load ESP from kern_esp (same value)
  RESTORE_ALL:  Pop registers from kernel stack A
  iret:         Resume execution at Task A's EIP/ESP
```

In this case, the registers pushed by SAVE_ALL are simply popped back by
RESTORE_ALL, and the task continues execution as if the interrupt never happened.

### 7.2 Switch Occurs -- Return to Different Task

A timer interrupt causes `sched_timeout` to wake up Task B, and Task B has
higher priority than Task A:

```
  SAVE_ALL:     Push Task A's registers onto kernel stack A
  intr_enter:   k_nest++
  C handler:    timer_intr -> sched_timeout -> Set Task B to TTS_RDY
                -> sched_next_tsk(apic) sets next_tsk_flag[cpu]=1
  intr_leave:   k_nest-- (=0)
                Save ESP to kernel stack A's kern_esp
                sched_next_tsk_check
                  -> sched_do_next_tsk
                    -> Find Task B (TTS_RDY, higher priority)
                    -> current_proc[cpu] = &proc_B
                    -> Revert Task A to TTS_RDY
                  -> return 1 (switch occurred)
                ESP = Task B's kern_esp (switch to kernel stack B)
                TSS.esp0 = Task B's kern_stack_top
  RESTORE_ALL:  Pop registers from kernel stack B (Task B's state)
  iret:         Resume execution at Task B's EIP/ESP
```

Task A's state remains frozen in the pt_regs frame on kernel stack A.
The next time Task A is scheduled, another interrupt's `intr_leave` will switch
ESP to Task A's `kern_esp`, RESTORE_ALL will restore Task A's registers, and
Task A will resume execution from the exact point where it was interrupted.

---

## 8. Complete Flow Diagram

The following shows the complete flow when a PIT timer interrupt (IRQ0) fires
on CPU 0:

```
Time ------------------------------------------------------------------------->

Task A (Ring 3, CPU 0)
  |
  |  <- PIT interrupt fires (IRQ0)
  |
  +-- [CPU automatic processing] ----------------------------------+
  |   1. Read Task A's kernel stack top from TSS.esp0              |
  |   2. Push SS, ESP, EFLAGS, CS, EIP onto kernel stack           |
  |   3. Read intr_irq0 address from IDT[0x80]                    |
  |   4. IF=0 (auto-cleared due to interrupt gate)                 |
  |   5. Set CS:EIP to intr_irq0                                  |
  +----------------------------------------------------------------+
  |
  +-- intr_irq0: --------------------------------------------------+
  |   |                                                            |
  |   |  SAVE_ALL                                                  |
  |   |    +- Push 9 registers (EAX..ES) onto kernel stack         |
  |   |       -> pt_regs frame complete                            |
  |   |                                                            |
  |   |  call intr_enter                                           |
  |   |    +- k_nest0++                                            |
  |   |                                                            |
  |   |  call c_intr_irq0                                          |
  |   |    +- timer_intr(0, 1)                                     |
  |   |    |   +- timer_ticks++                                    |
  |   |    |   +- vga_write_dec_at (update display)                |
  |   |    |   +- sched_timeout(0, 1)                              |
  |   |    |       +- (if timeout) sched_next_tsk                  |
  |   |    |           -> next_tsk_flag[0] = 1                     |
  |   |    |           -> next_tsk_flag[1] = 1                     |
  |   |    +- i8259_reenable()                                     |
  |   |        +- EOI to PIC master/slave                          |
  |   |        +- APIC EOI                                         |
  |   |                                                            |
  |   |  jmp intr_return                                           |
  |   |    call intr_leave                                         |
  |   |      +- APIC ID -> CPU 0                                   |
  |   |      +- k_nest0--                                          |
  |   |      +- k_nest0 == 0:                                      |
  |   |      |   +- proc_A.kern_esp = ESP                          |
  |   |      |   +- sched_next_tsk_check(0)                        |
  |   |      |   |   +- next_tsk_flag[0] != 0 ?                    |
  |   |      |   |   |   YES -> sched_do_next_tsk(0)               |
  |   |      |   |   |   current_proc[0] = (new task or same)      |
  |   |      |   |   |   NO  -> return 0                           |
  |   |      |   +- ESP = current_proc[0]->kern_esp                |
  |   |      |   |       (on switch: new task's stack)              |
  |   |      |   +- tss_update_esp0(0, kern_stack_top)             |
  |   |      +- ret -> intr_return_restore                         |
  |   |                                                            |
  |   |    RESTORE_ALL                                             |
  |   |      +- Pop 9 registers from (new task's) kernel stack     |
  |   |                                                            |
  |   |    iret                                                    |
  |   |      +- Pop EIP, CS, EFLAGS                                |
  |   |      +- Pop ESP, SS (return to Ring 3)                     |
  |   |      +- IF=1 restored (from EFLAGS)                        |
  |   |                                                            |
  +-- +------------------------------------------------------------+
  |
Task A or Task B (Ring 3, CPU 0)
  <- Resumes execution as if nothing happened
```

---

## Referenced Source Files

| File              | Contents                                           |
|-------------------|----------------------------------------------------|
| i386/intr.s       | SAVE_ALL/RESTORE_ALL, intr_enter/intr_leave, interrupt stubs |
| i386/interrupt.c  | C handlers, sched_next_tsk_check                   |
| i386/interrupt.h  | Vector constants (VECT_IRQ0=0x80, etc.)            |
| i386/smpP.h       | APIC constants (MAX_TIMER_COUNT, etc.)             |
| i386/timer.c      | timer_init, timer_intr                             |
| i386/timerP.h     | PIT constants (FREQ, HZ, SQUARE)                   |
| i386/smp.c        | APIC timer initialization, smp_eoi                 |
| i386/i8259.c      | i8259_reenable (PIC EOI)                           |
| i386/i8259P.h     | EOI constant (0x20)                                |
| kernel/sched.c    | sched_do_next_tsk, sched_next_tsk, sched_timeout   |
| i386/proc.h       | proc_t struct (kern_esp, kern_stack_top, cpu), pt_regs |
