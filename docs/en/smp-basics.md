# SMP (Symmetric Multi-Processing) Basics

This document covers the hardware and software fundamentals needed
to understand tiny-itron's SMP (2-CPU) implementation.

---

## Table of Contents

1. [What Is SMP?](#1-what-is-smp)
2. [CPU Identification: BSP and AP](#2-cpu-identification-bsp-and-ap)
3. [Local APIC: Per-CPU Interrupt Controller](#3-local-apic-per-cpu-interrupt-controller)
4. [AP Boot Sequence (INIT-SIPI Protocol)](#4-ap-boot-sequence-init-sipi-protocol)
5. [Memory Layout: Per-CPU Stacks](#5-memory-layout-per-cpu-stacks)
6. [Mutual Exclusion: Why Locks Are Needed](#6-mutual-exclusion-why-locks-are-needed)
7. [Spinlocks: Atomic Operations with the xchg Instruction](#7-spinlocks-atomic-operations-with-the-xchg-instruction)
8. [Per-CPU Data: Managing Per-CPU State](#8-per-cpu-data-managing-per-cpu-state)
9. [SMP Scheduling: CPU Affinity](#9-smp-scheduling-cpu-affinity)
10. [Following the tiny-itron Boot Process](#10-following-the-tiny-itron-boot-process)
11. [Summary: Key Principles](#11-summary-key-principles)

---

## 1. What Is SMP?

**SMP (Symmetric Multi-Processing)** is an architecture where
two or more CPUs share the same memory space and operate as equal peers.

```
        ┌───────────┐  ┌───────────┐
        │   CPU 0   │  │   CPU 1   │
        │  (BSP)    │  │   (AP)    │
        └─────┬─────┘  └─────┬─────┘
              │               │
        ══════╧═══════════════╧══════  Shared memory bus
              │
        ┌─────┴─────────────────────┐
        │        Main memory        │
        │  Code, data, stacks       │
        └───────────────────────────┘
```

**Key points:**
- Both CPUs can execute the **same instruction set** (this is what "symmetric" means)
- Both CPUs can **read and write the same memory**
- Each CPU executes instructions **independently** (they truly run simultaneously)
- The OS assigns work (tasks) to each CPU

### The Biggest Difference from Single-Processor Systems

On a single-processor system, even though we call it "multitasking," in reality
the CPU is just rapidly switching between tasks on timer interrupts --
**only one instruction is executing at any given moment**.

With SMP, **two instructions are truly executing at the same time**.
This is what gives rise to the locking problems discussed later.

---

## 2. CPU Identification: BSP and AP

When a PC is powered on, only **one** of the multiple CPUs starts running first.
The remaining CPUs sit idle in a halted state.

| Term | Full Name | Role |
|------|-----------|------|
| **BSP** | Bootstrap Processor | The first CPU to start. Performs OS initialization |
| **AP**  | Application Processor | Remains halted until explicitly started by the BSP |

In tiny-itron:
- **CPU 0 = BSP** -- handles all hardware initialization, kernel initialization, and task creation
- **CPU 1 = AP** -- executes its own tasks after receiving a startup signal from the BSP

### How a CPU Knows Which One It Is

Each CPU is assigned a unique number called an **APIC ID**.
This can be read from a register in the Local APIC hardware.

```c
/* i386/interrupt.c — Reading the APIC ID */
int get_apic_index(void)
{
    volatile unsigned long *apic_id = (volatile unsigned long *)0xFEE00020;
    unsigned long id = (*apic_id >> 24) & 0xFF;
    return (id == 0) ? 0 : 1;
}
```

- Read the value at address `0xFEE00020`
- Bits 24-31 contain the APIC ID
- If the APIC ID is 0, this is CPU 0 (BSP); otherwise it is CPU 1 (AP)

This function is called frequently inside interrupt handlers
to determine "which CPU is currently handling this interrupt?"

---

## 3. Local APIC: Per-CPU Interrupt Controller

### Limitations of the PIC

In the single-processor PC era, the **PIC (i8259)** interrupt controller
notified the CPU of interrupts from devices like the keyboard and timer.
However, the PIC was designed with the assumption of only one CPU
and has no mechanism to select which CPU receives an interrupt.

### What Is the Local APIC?

To support SMP, Intel embedded a **Local APIC (Advanced Programmable Interrupt
Controller)** inside each CPU.

```
    ┌──────────────────┐    ┌──────────────────┐
    │      CPU 0       │    │      CPU 1       │
    │  ┌────────────┐  │    │  ┌────────────┐  │
    │  │ Local APIC │  │    │  │ Local APIC │  │
    │  │ ID = 0     │  │    │  │ ID = 1     │  │
    │  └──────┬─────┘  │    │  └──────┬─────┘  │
    └─────────┼────────┘    └─────────┼────────┘
              │                       │
     ═════════╧═══════════════════════╧═══  APIC bus
              │
    ┌─────────┴────────┐
    │  PIC (i8259)     │  ← External interrupts: keyboard, PIT timer, etc.
    └──────────────────┘
```

**Main roles of the Local APIC:**

1. **CPU identification** -- The APIC ID register identifies which CPU this is
2. **Receiving interrupts** -- Receives external interrupts (via PIC) and timer interrupts
3. **Sending/receiving IPIs** -- Enables CPUs to send messages (interrupts) to each other
4. **EOI (End of Interrupt)** -- Notifies the APIC that interrupt processing is complete
5. **Timer** -- Each CPU has its own independent timer

### APIC Registers (Memory-Mapped I/O)

The Local APIC registers are mapped to a memory region starting at physical
address `0xFEE00000`. They can be accessed using ordinary memory read/write
instructions.

```c
/* i386/smpP.h — Key APIC registers */
#define APIC_BASE       0xFEE00000

#define APIC_ID         (APIC_BASE + 0x020)  /* CPU identification number */
#define APIC_EOI        (APIC_BASE + 0x0B0)  /* End of interrupt notification */
#define APIC_SVR        (APIC_BASE + 0x0F0)  /* APIC enable */
#define APIC_ICR_LOW    (APIC_BASE + 0x300)  /* IPI send (low) */
#define APIC_ICR_HIGH   (APIC_BASE + 0x310)  /* IPI send (high) */
#define APIC_LVT_TIMER  (APIC_BASE + 0x320)  /* Timer configuration */
```

> **What is memory-mapped I/O?** A method of accessing hardware registers
> as if they were located at specific memory addresses.
> From C, you simply read and write through a pointer to that address
> to control the hardware.

### EOI (End of Interrupt)

After finishing processing an interrupt, you need to notify the APIC
that you are done. If you don't, no further interrupts will arrive.

```c
/* i386/smp.c */
void smp_eoi(void)
{
    volatile unsigned long *eoi = (volatile unsigned long *)APIC_EOI;
    *eoi = 0;  /* Just writing 0 sends the EOI */
}
```

tiny-itron uses both the PIC (i8259) and Local APIC together:
- **External interrupts (keyboard, PIT):** PIC delivers them to CPU 0 only. PIC EOI is required
- **APIC timer interrupts:** Generated by each CPU's Local APIC. APIC EOI is required

---

## 4. AP Boot Sequence (INIT-SIPI Protocol)

At power-on, the AP is halted. To start the AP, the BSP must send
an IPI (Inter-Processor Interrupt) following the **INIT-SIPI protocol**
defined by Intel.

### Procedure

```
BSP                                     AP
 │                                       │ (halted)
 │── INIT IPI ──────────────────────────>│
 │         (Reset the AP)                │
 │                                       │
 │    ... wait 10ms ...                  │
 │                                       │
 │── Startup IPI (vector=0x03) ────────>│
 │         (AP begins execution at 0x3000)│
 │                                       │
 │    ... wait 200us ...                 │
 │                                       │
 │── Startup IPI (vector=0x03) ────────>│  ← sent twice for safety
 │                                       │
 │                                       │── Starts in real mode
 │                                       │── Transitions to protected mode
 │                                       │── Calls main()
 │                                       │
 │<── cpu_second = 1 (via memory) ───── │  ← handshake
 │                                       │
 │  (Both CPUs now operating independently)│
```

### How to Send an IPI (ICR Register)

IPIs are sent by writing a value to the APIC's **ICR (Interrupt Command Register)**.

```c
/* i386/smp.c — AP boot sequence (executed by BSP) */

/* 1. Send INIT IPI to all APs */
*icr_high = 0;
*icr_low  = ICR_INIT | ICR_LEVEL_ASSERT | ICR_ALL_EXCLUDING_SELF;
/* wait briefly */
*icr_low  = ICR_INIT | ICR_LEVEL_DEASSERT | ICR_ALL_EXCLUDING_SELF;

/* 2. Send Startup IPI twice (vector = 0x03 → physical address 0x3000) */
*icr_low  = ICR_STARTUP | ICR_ALL_EXCLUDING_SELF | 0x03;
/* wait briefly */
*icr_low  = ICR_STARTUP | ICR_ALL_EXCLUDING_SELF | 0x03;
```

**Startup IPI vector number:**
The vector number `0x03` means `0x03 * 0x1000 = 0x3000`.
The AP begins execution from this physical address in **real mode**.

> **What is real mode?** The mode in which an x86 CPU operates at power-on.
> It is 16-bit, with an address space limited to 1MB.
> An OS typically switches to protected mode (32-bit) as soon as possible.

### Handshake

The BSP needs to wait until the AP has finished its initialization.
In tiny-itron, a simple handshake is performed using the shared memory
variable `cpu_second`.

```c
/* BSP side (smp_init) */
while (cpu_second == 0)
    ;  /* Busy-wait until the AP is ready */

/* AP side (smp_ap_init) */
cpu_second = 1;  /* Notify the BSP: "ready" */
```

`cpu_second` is declared `volatile`. This instructs the compiler:
"This variable may be modified externally (by another CPU),
so do not cache it in a register -- read it from memory every time."

---

## 5. Memory Layout: Per-CPU Stacks

### Why Each CPU Needs Its Own Stack

The stack is a memory region that stores function arguments, local variables,
and return addresses. If two CPUs call functions simultaneously, a single stack
would have its contents interleaved and corrupted.
Therefore, **each CPU needs its own independent stack**.

```
  Memory addresses (low → high)
  ─────────────────────────────────────────────

  0x110000 ┬─────────────────── Memory pool (MEM_START)
           │   ...
  0x700000 ┬─────────────────── Stack pool (task stacks)
           │   ...
  0x750000 ┬─────────────────── Per-task kernel stacks (16 tasks x 4KB)
           │   Task 1: 0x750000-0x751000
           │   Task 2: 0x751000-0x752000
           │   ...
  0x770000 ┬─────────────────── CPU 1 initial stack (main() only)
           │   ...
  0x7A0000 ┬─────────────────── CPU 0 initial stack (main() only)
```

> **Stack growth direction:** On x86, the stack grows from high addresses
> toward low addresses. `0x7A0000` is the "bottom of the stack," and
> the address decreases with each push.

### Ring 0 and Ring 3

x86 has a mechanism called **privilege levels (Rings)**.

| Ring | Usage | Capabilities |
|------|-------|-------------|
| Ring 0 | OS kernel | All instructions are executable (cli, sti, in, out, etc.) |
| Ring 3 | User applications | Privileged instructions cannot be executed (#GP exception occurs) |

When a user task (Ring 3) enters the kernel (Ring 0) via a syscall or interrupt,
the CPU automatically switches to the kernel stack specified in TSS.esp0.
In tiny-itron, **each task has a 4KB kernel stack**, and TSS.esp0 is
dynamically updated on every task switch.

```c
/* i386/addr.h — Per-CPU boot stacks + per-task kernel stacks */
#define CPU0_SP         0x7a0000   /* CPU 0 initial stack (main() only) */
#define CPU1_SP         0x770000   /* CPU 1 initial stack (main() only) */
#define KERN_STACK_BASE 0x750000   /* Start of per-task kernel stack area */
#define KERN_STACK_SIZE 4096       /* 4KB per task */
/* Top of Task N = KERN_STACK_BASE + (N+1) * KERN_STACK_SIZE */
```

After task startup, all Ring 0 execution takes place on the per-task kernel stack.
`CPU0_SP`/`CPU1_SP` are used only during kernel initialization (`main()`) and
are not used thereafter.

### CPU Detection and Stack Selection in run.s

When the AP starts, it reaches the same `run` label as the BSP.
Here, it checks the `cpu_num` variable to determine whether it is
the BSP or the AP, and selects the appropriate stack.

```asm
# i386/run.s
run:
    # ... segment register setup ...

    cmpl  $0, cpu_num      # if cpu_num == 0, this is BSP
    jne   run_ap

    # BSP: use CPU 0 stack
    movl  $0x07a0000, %esp
    call  main
    jmp   halt

run_ap:
    # AP: use CPU 1 stack
    movl  $0x0770000, %esp
    call  main
    jmp   halt
```

---

## 6. Mutual Exclusion: Why Locks Are Needed

### A Concrete Example of the Problem

Consider the following C code being executed simultaneously by two CPUs:

```c
int count = 0;

void increment(void) {
    count = count + 1;
}
```

This looks harmless at first glance, but `count = count + 1` translates to
**three operations** at the CPU level:

```
1. Read the value of count from memory (LOAD)
2. Add 1 to the value (ADD)
3. Write the result back to memory (STORE)
```

When two CPUs execute this simultaneously:

```
CPU 0                    CPU 1                     Value of count
─────────────────────────────────────────────────────────────
LOAD count → 0                                     0
                         LOAD count → 0            0
ADD  0 + 1 = 1                                     0
                         ADD  0 + 1 = 1            0
STORE 1 → count                                    1
                         STORE 1 → count           1  ← expected 2!
```

**We incremented twice, but the result is 1.**
This is a **race condition**.

### The Solution: Locks

To prevent race conditions, we need a mechanism that says
"while one CPU is performing an operation, prevent other CPUs from touching
the same data." This is called **mutual exclusion**, and the mechanism
used to achieve it is called a **lock**.

```
CPU 0                    CPU 1                     Value of count
─────────────────────────────────────────────────────────────
Acquire lock: OK                                    0
LOAD count → 0          Acquire lock → FAIL (wait) 0
ADD  0 + 1 = 1                    (spinning...)    0
STORE 1 → count                   (spinning...)    1
Release lock             Acquire lock: OK           1
                         LOAD count → 1            1
                         ADD  1 + 1 = 2            1
                         STORE 2 → count           2  ← correct!
                         Release lock
```

---

## 7. Spinlocks: Atomic Operations with the xchg Instruction

### What Are Atomic Operations?

To implement a lock, you need an instruction that performs "a memory read
and write as a **single indivisible operation**."
This is called an **atomic operation** (atomic = indivisible).

x86 has an atomic instruction called `xchg` (exchange):

```
xchgl %eax, (%ebx)
```

This instruction performs the following two actions **simultaneously,
in a way that no other CPU can intervene**:
1. Read the value at the memory location pointed to by `(%ebx)` into `%eax`
2. Write the original value of `%eax` to the memory location pointed to by `(%ebx)`

In other words, it "swaps the values of a register and a memory location."
The Intel manual specifies that `xchg` **implicitly asserts a bus lock**.

### tiny-itron's Spinlock Implementation

```c
/* i386/smp.c */
void smp_lock(unsigned long *p)
{
    while (cxchg(p, 1))       /* Swap *p with 1. If old value was 0, lock acquired */
        __asm__ volatile("pause");  /* Let the CPU rest briefly (see below) */
}

void smp_unlock(unsigned long *p)
{
    cxchg(p, 0);              /* Write 0 to *p → release the lock */
}
```

`cxchg` is an assembly function defined in klib.s:

```asm
# i386/klib.s — Atomic exchange
cxchg:
    pushl   %ebp
    movl    %esp, %ebp
    pushl   %ebx
    movl    8(%ebp), %ebx      # 1st argument: address of lock variable
    movl    12(%ebp), %eax     # 2nd argument: value to write
    xchgl   (%ebx), %eax       # Atomic exchange
    popl    %ebx
    popl    %ebp
    ret                         # Return value (EAX) = value before exchange
```

### How Lock Acquisition Works

The lock variable holds either `0` (free) or `1` (held).

```
Lock acquisition: cxchg(p, 1)
  If *p is 0 (free):
    → Write 1 to *p (lock acquired)
    → Old value 0 is returned → exit while loop → success!

  If *p is 1 (held by another CPU):
    → Write 1 to *p (no change)
    → Old value 1 is returned → continue while loop → spin (keep retrying)

Lock release: cxchg(p, 0)
  → Write 0 to *p → other CPUs can now acquire it
```

This mechanism is called a **spinlock** because it loops (spins) until
the lock can be acquired.

### The pause Instruction

The reason for including the `pause` instruction inside the spin loop:

- In Hyper-Threading environments (where one CPU core runs two threads),
  it reduces wasted CPU resources consumed by the spinning thread
- It clears the CPU's speculative execution pipeline, speeding up
  detection of when the lock is released
- On CPUs that don't have `pause` (Pentium 3 and earlier), it is treated
  as a NOP, so it is harmless

### Why cli/sti Is Not Sufficient

On single-processor systems, `cli` (disable interrupts) and `sti` (enable
interrupts) could be used for mutual exclusion. Disabling interrupts
prevents any other code from preempting the CPU.

With SMP, **cli/sti only controls interrupts on the current CPU**.
Other CPUs continue running regardless of cli, so it does not provide
mutual exclusion.

Furthermore, in tiny-itron, locks may be called from user tasks (Ring 3)
(for example, screen output in `video.c`). In Ring 3, `cli`/`sti` are
privileged instructions and cannot be used (executing them triggers
a #GP exception).

An `xchg`-based spinlock works regardless of privilege level.

---

## 8. Per-CPU Data: Managing Per-CPU State

In an SMP kernel, information such as "which task is currently running"
and "the interrupt nesting count" must be managed **independently for
each CPU**. This is because CPU 0 and CPU 1 are running different tasks
simultaneously.

### Per-CPU Variables in tiny-itron

There are four per-CPU variables actually used in tiny-itron:

```c
/* i386/kernelval.c */
proc_t* current_proc[MAX_CPU];   /* Process currently running on each CPU */
ID      c_tskid[MAX_CPU];       /* Task ID currently running on each CPU */

/* kernel/sched.c */
INT     next_tsk_flag[2];       /* Reschedule request flag for each CPU */
```

```asm
# i386/intr.s (defined as individual variables in assembly)
k_nest0:  .long 0               # CPU 0 interrupt nesting depth
k_nest1:  .long 0               # CPU 1 interrupt nesting depth
```

`k_nest` is defined as individual variables rather than a C array because
the assembly code (`intr_enter`/`intr_leave`) needs fast access to it
on every interrupt.

Usage:

```c
int cpu = get_apic_index();           /* 0 or 1 */
current_proc[cpu] = &proc[task_id];   /* Update the task for this CPU */
c_tskid[cpu] = task_id;
```

### Criteria for What Should Be Per-CPU

The criterion for whether a variable should be per-CPU is:
**"Is there a possibility that two CPUs will read and write it
independently at the same time?"**

| Variable | Reason for being per-CPU |
|----------|-------------------------|
| `current_proc[]` | Each CPU runs a different task simultaneously |
| `c_tskid[]` | Same as above |
| `next_tsk_flag[]` | CPU 0's IRQ0 sets CPU 1's flag, and CPU 1's APIC timer reads it |
| `k_nest0/1` | Interrupts nest independently on each CPU |

On the other hand, the system tick counter (`timer_ticks`) is correctly
a single global variable rather than per-CPU. The PIT (IRQ0) is the sole
system clock source, delivered only to CPU 0, and CPU 1's APIC timer
is used only as a preemption trigger.

### CPU Detection in Interrupt Handlers

When an interrupt occurs, the handler first determines "which CPU am I
running on?" This allows it to access the correct per-CPU data.

```asm
# i386/intr.s — intr_enter (called right after SAVE_ALL)
intr_enter:
    movl   0xFEE00020, %eax    # Read APIC ID register (MMIO)
    shrl   $24, %eax           # Extract bits 24-31 → EAX = 0 or 1
    testl  %eax, %eax
    jnz    1f                  # Branch if CPU 1
    incl   k_nest0             # CPU 0: increment nesting counter
    ret
1:
    incl   k_nest1             # CPU 1: increment nesting counter
    ret
```

`intr_enter` uses the APIC ID to determine the CPU and increments the
per-CPU nesting counter. The corresponding `intr_leave` decrements it,
and when k_nest returns to 0, it determines whether a task switch is needed.
The per-CPU `current_proc[]` pointer is referenced by `intr_leave`
(`current_proc+4` is `current_proc[1]` -- with 4-byte pointers, this is
the second element of the array).

### Syscalls Are Always Processed by the Calling CPU

In an SMP environment, it is important to correctly understand
"which CPU processes a syscall?"

External interrupts (IRQs) may be **delivered to a different CPU** depending
on PIC/APIC routing. For example, IRQ1 (keyboard) is delivered to CPU 0
via the PIC.

On the other hand, **a syscall (`INT 0x99`) is a software interrupt**: when
the CPU executes that instruction, it immediately references the IDT and
jumps to the handler. There is no "which CPU should this be routed to?"
decision involved. This is a fundamental behavior of the `INT` instruction
and does not change in SMP.

```
External interrupt (IRQ):     Device → PIC → delivered to CPU 0  (routing exists)
Software interrupt:           CPU 1 executes INT 0x99 → CPU 1 handles it  (always the issuing CPU)
```

Therefore:
- If a task on CPU 1 calls `cre_tsk()`, `c_intr_syscall` -> `sys_cre_tsk` runs on CPU 1
- If a task on CPU 0 calls `sig_sem()`, it is processed on CPU 0
- `get_apic_index()` correctly returns "the calling CPU" inside syscall handlers

tiny-itron leverages this property by using the `apic` argument
(= `get_apic_index()`) throughout the kernel in syscall handlers.

---

## 9. SMP Scheduling: CPU Affinity

### What Is CPU Affinity?

In SMP, you need to decide "which CPU should each task run on?"
This assignment is called **CPU affinity**.

In tiny-itron, at task creation time, the affinity is set to **the same CPU
as the creating task**:

```c
/* i386/proc.c — Setting CPU affinity at task creation */
proc[id].cpu = get_apic_index();  /* Assign to the CPU that created the task */
```

Once set, the affinity is never changed (static affinity).
This is simple to implement and good for cache efficiency.

### How the Scheduler Works

The scheduler operates **independently on each CPU** and selects the
highest-priority task that has affinity for its own CPU.

```c
/* kernel/sched.c — sched_do_next_tsk (simplified) */
ID sched_do_next_tsk(W apic)
{
    smp_lock(&kernel_lk);

    for (pri = 1; pri <= TMAX_TPRI; pri++) {
        /* Scan the task list at priority pri */
        for (each task t in tsk_pri[pri]) {
            if (t->tskstat == TTS_RDY &&
                proc[t->tskid].cpu == apic) {  /* Task for this CPU? */
                t->tskstat = TTS_RUN;
                current_proc[apic] = &proc[t->tskid];
                c_tskid[apic] = t->tskid;
                smp_unlock(&kernel_lk);
                return t->tskid;
            }
        }
    }

    smp_unlock(&kernel_lk);
    return E_ID;  /* No runnable task */
}
```

**Key points:**
- Acquires `kernel_lk` (BKL) before scanning (to prevent races with the other CPU)
- Searches priorities from 1 (highest) to 16 (lowest)
- `proc[t->tskid].cpu == apic` checks the CPU affinity
- When a task is found, changes it to `TTS_RUN` (running) and returns

### Idle Task

What does a CPU do when all tasks are in a wait state?
In tiny-itron, each CPU has an **idle task**.
It simply loops forever at the lowest priority (16).

```c
/* kernel/user.c */
void idle_task(void)
{
    while (1)
        __asm__ volatile("pause");  /* Wait in low-power mode */
}
```

Because an idle task is always present in the RDY state,
it prevents the problem where `sched_do_next_tsk()` fails to find an RDY task
and mistakenly runs a WAI-state task (ghost running).
The scheduler never encounters a situation where "there are no tasks to run at all."

---

## 10. Following the tiny-itron Boot Process

Using the knowledge covered so far, let's trace how tiny-itron goes from
power-on to running with 2 CPUs.

### Phase 1: BSP Boot (CPU 0)

```
Power ON
  │
  ▼
start.s (0x3000)          ← BIOS transfers control (real mode)
  │  Enable A20
  │  Load GDT/IDT
  │  Transition to protected mode
  ▼
run.s (0x3400)            ← 32-bit mode
  │  cpu_num == 0 → identified as BSP
  │  ESP = 0x7A0000 (CPU 0 stack)
  ▼
main() [i386/main.c]
  │  all_init()           ← IDT, VGA, PIC, timer, keyboard initialization
  │  page_init()          ← Build page tables
  │  page_enable()        ← CR0.PG=1 (enable paging)
  │  itron_init()         ← ITRON kernel initialization
  │  proc_init()          ← Process structure initialization, create initial tasks
  │  tss_init()           ← TSS setup (for both CPUs)
  │  cpu_num = 1          ← Indicates the next CPU to reach main() is the AP
  ▼
smp_init() [i386/smp.c]
  │  Enable Local APIC (BSP)
  │  Configure APIC timer
  │  Send INIT IPI ──────────────────────────┐
  │  Send SIPI (vector=0x03 → 0x3000) ──────┤
  │                                          │
  │  while (cpu_second == 0) ; ← wait for AP │
  │                                          ▼
  │                              [Phase 2: AP boots]
  │
  │  ← cpu_second became 1!
  │
  │  timer_start()        ← Start PIT timer
  │  key_start()          ← Enable keyboard IRQ
  ▼
start_first_task()
  │  ltr $0x38             ← Load TSS 0 into Task Register
  │  ESP = kern_esp → ret  ← RESTORE_ALL → iret transitions to Ring 3
  ▼
first_task() [kernel/user.c]
  │  Create semaphore 1, create and activate Task 3 (usr_main)
  │  Task 1 and Task 3 alternate execution via wup_tsk/slp_tsk
  ▼
  (CPU 0 continues executing tasks)
```

### Phase 2: AP Boot (CPU 1)

```
SIPI received → execution starts at 0x3000
  │
  ▼
start.s (0x3000)          ← Real mode (same code as BSP)
  │  Enable A20
  │  Load GDT/IDT
  │  Transition to protected mode
  ▼
run.s (0x3400)
  │  cpu_num == 1 → identified as AP
  │  ESP = 0x770000 (CPU 1 stack)
  ▼
main() [i386/main.c]
  │  cpu_num != 0 → take AP path
  │  page_enable()          ← Share BSP's page tables
  ▼
smp_ap_init() [i386/smp.c]
  │  Enable Local APIC (AP)
  │  Configure APIC timer
  │  cpu_second = 1       ← Notify BSP: "ready"
  ▼
start_second_task()
  │  ltr $0x40             ← Load TSS 1 into Task Register
  │  ESP = kern_esp → ret  ← RESTORE_ALL → iret transitions to Ring 3
  ▼
second_task() [kernel/user.c]
  │  Create and activate Task 4 (kbd_task)
  │  Count up + compete with Task 3 for shared_count via semaphore
  ▼
  (CPU 1 continues executing tasks)
```

### Task Layout (After Boot Completes)

```
CPU 0                          CPU 1
┌────────────────────┐        ┌────────────────────┐
│ Task 1: first_task │        │ Task 2: second_task│
│   (pri=15)         │        │   (pri=15)         │
│ Task 3: usr_main   │        │ Task 4: kbd_task   │
│   (pri=15)         │        │   (pri=1, highest) │
│ Task 5: idle_task  │        │ Task 6: idle_task  │
│   (pri=16, lowest) │        │   (pri=16, lowest) │
└────────────────────┘        └────────────────────┘
```

---

## 11. Summary: Key Principles

### Five Rules of SMP Programming

**1. Shared data requires locks**

Any data accessed by multiple CPUs must be protected with a lock.
Even read-only access needs protection, because another CPU might be
writing at the same time.

**2. Be mindful of lock granularity**

Protecting everything with a single lock (coarse-grained) is safe but
limits performance. Splitting locks by purpose (fine-grained) improves
performance but increases the risk of deadlocks.
tiny-itron protects all kernel data with a single `kernel_lk` (Big Kernel
Lock). For a 2-CPU educational kernel, guaranteeing correctness outweighs
the performance benefits of fine-grained locking.

**3. Use a consistent lock ordering**

When acquiring multiple locks at the same time, always acquire them in
**the same order** everywhere. If the order is reversed, a deadlock
(permanent stall where each CPU waits for the other's lock) can occur.

```
Correct:   CPU 0: lock(A) → lock(B)    CPU 1: lock(A) → lock(B)
Dangerous: CPU 0: lock(A) → lock(B)    CPU 1: lock(B) → lock(A)  ← deadlock!
```

**4. Use volatile correctly**

`volatile` is a qualifier that tells the compiler "this variable's value may
change independently of the program's control flow." Specifically, it
instructs the compiler "you must not omit, reorder, or cache accesses to
this variable in a register."

Variables that may be modified by another CPU should be marked `volatile`.
Without it, the compiler may optimize away the access, reasoning that
"only I modify this variable, so I can keep it in a register," and miss
the change.

**5. Manage per-CPU data using arrays**

Manage per-CPU data with arrays indexed by CPU number, like
`current_proc[cpu]` and `c_tskid[cpu]`.
This allows each CPU to safely access its own data without locks
(since no other CPU writes to its index).

### Checklist for Reading This Code

- [ ] When you see `smp_lock` / `smp_unlock` -- shared data is being protected
- [ ] When you see `get_apic_index()` -- a CPU number is being used to select per-CPU data
- [ ] When you see `current_proc[apic]` -- the task running on that CPU
- [ ] When you see `c_tskid[apic]` -- the task ID running on that CPU
- [ ] When you see `sched_next_tsk(apic)` -- a reschedule request (notifies both CPUs)
- [ ] When you see `sched_do_next_tsk(apic)` -- actual task selection (for this CPU only)
- [ ] When you see `smp_eoi()` -- APIC end-of-interrupt notification
- [ ] When you see `volatile` -- a variable modified by another CPU or an interrupt
- [ ] When you see `xchgl` -- an atomic operation (the foundation of spinlocks)

---

## Reference: Glossary

| Term | Meaning |
|------|---------|
| SMP | Symmetric Multi-Processing. An architecture where multiple CPUs operate as equal peers |
| BSP | Bootstrap Processor. The first CPU to start |
| AP | Application Processor. An additional CPU started by the BSP |
| APIC | Advanced Programmable Interrupt Controller |
| Local APIC | The interrupt controller built into each CPU |
| IPI | Inter-Processor Interrupt. An interrupt sent between CPUs |
| SIPI | Startup IPI. A special IPI used to boot an AP |
| ICR | Interrupt Command Register. An APIC register used to send IPIs |
| EOI | End of Interrupt. Notification that interrupt processing is complete |
| TSS | Task State Segment. An x86 data structure for task switching |
| Spinlock | A mutual exclusion mechanism that loops (spins) until the lock is acquired |
| Atomic operation | An indivisible operation that cannot be interrupted by others |
| Race condition | A bug that depends on the execution order of multiple concurrent actors |
| Deadlock | A permanent stall where multiple actors wait for each other's locks |
| CPU affinity | The setting that determines which CPU a task runs on |
| Per-CPU data | Variables managed independently for each CPU |
| volatile | A qualifier that suppresses compiler optimizations and forces memory reads every time |
| Ring 0 / Ring 3 | x86 privilege levels. 0 is kernel, 3 is user |
| PIC (i8259) | A legacy interrupt controller (designed for single-CPU systems) |
| Memory-mapped I/O | A method of accessing hardware registers as memory addresses |
