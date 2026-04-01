# System Overview — How the tiny-itron Demo Works

This document explains the big picture of tiny-itron, from boot to steady-state operation.
It describes how six tasks run on two CPUs and cooperate through ITRON APIs,
mutual exclusion, and interrupts, with references to the actual code.

---

## Table of Contents

1. [Task Layout Overview](#1-task-layout-overview)
2. [Boot Sequence](#2-boot-sequence)
3. [Task 1 (first_task) Behavior](#3-task-1-first_task-behavior)
4. [Task 3 (usr_main) Behavior](#4-task-3-usr_main-behavior)
5. [Task 2 (second_task) Behavior](#5-task-2-second_task-behavior)
6. [Task 4 (kbd_task) Behavior](#6-task-4-kbd_task-behavior)
7. [Tasks 5 and 6 (idle_task) Role](#7-tasks-5-and-6-idle_task-role)
8. [Inter-Task Communication Overview](#8-inter-task-communication-overview)
9. [Scheduler and Task Switching](#9-scheduler-and-task-switching)
10. [Mutual Exclusion: Big Kernel Lock](#10-mutual-exclusion-big-kernel-lock)
11. [Interrupt Flow](#11-interrupt-flow)
12. [VGA Screen Layout](#12-vga-screen-layout)
13. [Timeline: One Cycle of Steady-State Operation](#13-timeline-one-cycle-of-steady-state-operation)
14. [System Halt via Ctrl-C](#14-system-halt-via-ctrl-c)

---

## 1. Task Layout Overview

```
┌─────────────────────────────────┬─────────────────────────────────┐
│           CPU 0                 │           CPU 1                 │
├─────────────────────────────────┼─────────────────────────────────┤
│  Task 1 (first_task)            │  Task 2 (second_task)           │
│    Priority 15, count-up        │    Priority 15, count-up        │
│    MBF receive, alternates      │    Semaphore contention         │
│    with Task 3                  │    (pol_sem)                    │
│                                 │                                 │
│  Task 3 (usr_main)              │  Task 4 (kbd_task)              │
│    Priority 15, semaphore       │    Priority 1 (highest)         │
│    contention                   │    Keyboard input handling      │
│    Alternates with Task 1       │    → Sends lines to Task 1      │
│                                 │      via MBF                    │
│                                 │                                 │
│  Task 5 (idle_task)             │  Task 6 (idle_task)             │
│    Priority 16 (lowest),        │    Priority 16 (lowest),        │
│    pause loop                   │    pause loop                   │
└─────────────────────────────────┴─────────────────────────────────┘
```

| Task ID | Function | CPU | Priority | Created by |
|---------|----------|-----|----------|------------|
| 1 | `first_task` | 0 | 15 | `proc_init` (at boot) |
| 2 | `second_task` | 1 | 15 | `proc_init` (at boot) |
| 3 | `usr_main` | 0 | 15 | Task 1 via `cre_tsk` + `act_tsk` |
| 4 | `kbd_task` | 1 | 1 | Task 2 via `cre_tsk` + `act_tsk` |
| 5 | `idle_task` | 0 | 16 | `proc_init` (at boot) |
| 6 | `idle_task` | 1 | 16 | `proc_init` (at boot) |

---

## 2. Boot Sequence

The flow from power-on to all tasks running:

```
  CPU 0 (BSP)                              CPU 1 (AP)
  ─────────                                ─────────
  start.s: Real mode → Protected mode
  run.s: cpu_num=0 → main()
  │
  ├─ all_init()
  │   ├─ idt_init()       IDT setup
  │   ├─ video_init()     VGA initialization
  │   ├─ i8259_init()     PIC initialization
  │   ├─ timer_init()     PIT initialization
  │   └─ key_init()       Keyboard initialization
  │
  ├─ page_init()           Build page tables (full identity mapping + CPU stack protection)
  ├─ page_enable()         Load CR3 + set CR0.PG=1 to enable paging
  │
  ├─ itron_init()
  │   ├─ tsk_init()       Initialize tsk[] to TTS_NON
  │   ├─ sem_init()       Initialize sem[]
  │   ├─ dtq_init()       Initialize dtq[]
  │   ├─ sched_init()     Initialize priority queues
  │   └─ pool_init()      Initialize memory pool
  │
  ├─ proc_init()                Prepare data structures for all tasks on CPU 0
  │   ├─ Create Task 1 (affinity=CPU0, TTS_RUN)
  │   ├─ Create Task 2 (affinity=CPU1, TTS_RUN)
  │   ├─ Create Task 5 (affinity=CPU0, idle)
  │   └─ Create Task 6 (affinity=CPU1, idle)
  │   * At this point, only cre_tsk + act_tsk + TTS_RUN setup is performed.
  │     Actual execution begins via start_first_task / start_second_task.
  │
  ├─ tss_init()           TSS setup (for both CPUs)
  │
  ├─ smp_init()
  │   ├─ Enable Local APIC
  │   ├─ Configure APIC timer
  │   ├─ Send INIT IPI → SIPI to AP ─────→  AP starts
  │   │                                        │
  │   ├─ while(!cpu_second) wait               ├─ start.s: re-execute
  │   │                                  ←─── ├─ run.s: cpu_num=1 → main()
  │   │                                        │
  │   │                                        ├─ page_enable()
  │   │                                        │   Load CR3 (same page table as BSP)
  │   │                                        └─ smp_ap_init()
  │   │                                            ├─ Load TSS
  │   │                                            ├─ Enable APIC
  │   │                                            ├─ Configure APIC timer
  │   │                                            ├─ cpu_second = 1
  │   │         cpu_second=1 confirmed ←───────────┤
  │   │                                            └─ start_second_task()
  │   ├─ timer_start()    Enable PIT IRQ0                          │
  │   ├─ key_start()      Enable IRQ1                              │
  │   └─ start_first_task()                                        │
  │           │                                                    │
  │     ltr SEL_TSS0                                  ltr SEL_TSS1
  │     ESP=kern_esp → ret                            ESP=kern_esp → ret
  │     → RESTORE_ALL → iret                          → RESTORE_ALL → iret
  │     → Transition to Ring 3                        → Transition to Ring 3
  │           │                                             │
  ▼           ▼                                             ▼
Task 1 (first_task) starts                     Task 2 (second_task) starts
```

Key points:

- **Initialization order**: The order `itron_init()` → `proc_init()` is mandatory.
  If reversed, `tsk_init()` would overwrite the state set by `proc_init()`.
- **Unified task startup**: Even the first task starts via `ltr` + `RESTORE_ALL` + `iret`.
  Hardware TSS switching via `ljmp` is not used.
- **Ring transition**: The kernel runs in Ring 0, but user tasks run in Ring 3.
  The fake frame built by `proc_create` contains EFLAGS with IF=1, so `iret` enables interrupts.
- **Paging**: The BSP builds page tables with `page_init()` and enables paging with
  `page_enable()` (CR0.PG=1). The AP loads the same page tables via `page_enable()`.
  All pages use identity mapping (virtual address = physical address), and the U/S bit
  provides Supervisor protection for kernel code/data, CPU stack regions (0x750000 and above),
  and the VGA buffer (0xB8000). Only user code (.user_text) and user data (.user_data) are
  mapped as User pages (see `memory-map.md` for details).

---

## 3. Task 1 (first_task) Behavior

**File**: `kernel/user.c:141`
**CPU**: 0, **Priority**: 15

Task 1 creates the system's shared objects immediately after startup,
then alternates with Task 3 to perform count-ups.

### Initialization Phase

```c
/* 1. Create binary semaphore (ID=1, count=1) */
csem.sematr  = TA_TFIFO;
csem.isemcnt = 1;
csem.maxsem  = 1;
cre_sem(1, &csem);

/* 2. Create message buffer (ID=1, maxmsz=64, mbfsz=256) */
cmbf.mbfatr = TA_TFIFO;
cmbf.maxmsz = 64;         /* Max 64 bytes per message */
cmbf.mbfsz  = 256;        /* 256-byte ring buffer */
cmbf.mbf    = (VP)0;      /* Kernel allocates the buffer */
cre_mbf(1, &cmbf);

/* 3. Create and activate Task 3 */
ctsk.task    = (FP)usr_main;
ctsk.stk     = tsk_stack_alloc(1024);  /* Allocate stack (syscall) */
ctsk.itskpri = TMAX_TPRI - 1;     /* Priority 15 */
cre_tsk(3, &ctsk);
act_tsk(3);

/* 4. Draw VGA screen header (via clear_screen/print_at syscalls) */
draw_header();
```

Task 1 creates the semaphore, message buffer, and Task 3.
Task 2 does not create either the semaphore or the MBF -- it assumes Task 1 creates them first.

`draw_header()` uses the `clear_screen()` and `print_at()` syscalls to draw the header
and fixed labels on the VGA screen. Direct access to the VGA buffer (0xB8000) from Ring 3
is prohibited by page protection (Supervisor), so all writes go through syscalls.

`shared_count` and `task_count[]` are placed in the `.user_data` section (User pages).
Ring 3 user tasks can read and write them directly.

### Main Loop

```c
while (1) {
    task_count[1]++;                                       /* Count up */
    print_dec_at(ROW_TASK1, 19, task_count[1], 8, ...);   /* Display on screen (syscall) */

    /* Receive a line string from keyboard via MBF 1, with timeout */
    mbf_ret = trcv_mbf(1, mbf_buf, 20);   /* 20 ticks ≈ 0.33s */
    if ((int)mbf_ret > 0) {
        /* Receive successful: mbf_ret is the message size (bytes) */
        int len = (int)mbf_ret;
        if (len > 80 - 36)
            len = 80 - 36;                /* Truncate to display area (44 chars) */
        mbf_buf[len] = '\0';
        print_at(ROW_TASK1, 36, mbf_buf, ATTR_YELLOW);  /* syscall */
        /* Clear remaining display area */
        if (36 + len < 80)
            fill_at(ROW_TASK1, 36 + len, 80 - 36 - len, ' ', ATTR_YELLOW);
    }

    wup_tsk(3);       /* Wake Task 3 → Task 3 becomes RDY */
    slp_tsk();        /* Go to WAI → yield CPU to Task 3 */
}
```

What happens in one cycle:
1. Increment `task_count[1]` and display it on screen
2. Receive a line string from MBF 1 with timeout (`trcv_mbf`).
   If a line arrives, it is received immediately; otherwise, timeout (E_TMOUT) after 20 ticks.
   The timeout serves as pacing, so no `delay()` is needed
3. Wake Task 3 with `wup_tsk(3)`, then sleep with `slp_tsk()`

---

## 4. Task 3 (usr_main) Behavior

**File**: `kernel/user.c:215`
**CPU**: 0, **Priority**: 15

Task 3 is created by Task 1 and is responsible for **demonstrating cross-CPU
access to a shared counter (`shared_count`) using a semaphore**.

### State Machine (3 Phases)

Task 3 cycles through three phases: acquire, hold, and rest:

```
  ┌──────────┐   pol_sem(1)==E_OK   ┌──────────┐
  │   REST   │ ───────────────────→ │   LOCK   │
  │ (resting)│                      │ (holding) │
  │ phase_cnt│                      │ phase_cnt│
  │  < 40    │  ←───────────────── │  >= 20   │
  └──────────┘    sig_sem(1)        └──────────┘
       │
       │ pol_sem(1)==E_TMOUT
       ▼
  Display "BUSY" on screen
  (other CPU holds the lock)
```

### Main Loop Details

```c
while (1) {
    task_count[3]++;

    if (have_sem) {
        /* LOCK phase: holding the semaphore */
        shared_count++;         /* Increment shared counter */
        phase_cnt++;
        if (phase_cnt >= SEM_HOLD) {   /* Release after holding 20 times */
            sig_sem(1);
            have_sem = 0;
            phase_cnt = 0;
        }
    } else if (phase_cnt < SEM_REST) {
        /* REST phase: don't touch the semaphore (skip 40 iterations) */
        phase_cnt++;
    } else {
        /* TRY phase: attempt to acquire the semaphore */
        if (pol_sem(1) == E_OK) {      /* Non-blocking acquire */
            have_sem = 1;
            shared_count++;
        } else {
            /* BUSY: Task 2 (CPU 1) holds the lock */
        }
    }

    delay();
    wup_tsk(1);    /* Wake Task 1 */
    slp_tsk();     /* Go to sleep */
}
```

**Alternation pattern with Task 1**:

```
  Task 1                Task 3
  ──────                ──────
  Running               WAI (sleeping)
    │
  wup_tsk(3) ──→       Transitions to RDY
  slp_tsk()             │
  Transitions to WAI ←── Starts running
                        │
                      Processing (semaphore, etc.)
                        │
                      wup_tsk(1) ──→ Task 1 becomes RDY
                      slp_tsk()
                      Transitions to WAI ←── Task 1 starts running
```

Two tasks alternate like ping-pong on the same CPU (CPU 0).

---

## 5. Task 2 (second_task) Behavior

**File**: `kernel/user.c:279`
**CPU**: 1, **Priority**: 15

Task 2 runs on CPU 1 and **contends for a semaphore across CPUs** with Task 3.

### Initialization Phase

```c
/* Create Task 4 (keyboard) */
ctsk.task    = (FP)kbd_task;
ctsk.stk     = tsk_stack_alloc(1024);  /* Allocate stack (syscall) */
ctsk.itskpri = 1;                  /* Highest priority */
cre_tsk(4, &ctsk);
act_tsk(4);
```

Since Task 4 is created on CPU 1, it inherits CPU affinity 1.
With priority 1 (highest), Task 2 gets preempted whenever Task 4 becomes RDY.

### Main Loop

Task 2's semaphore handling uses almost the same logic as Task 3,
but differs in that **it does not use `slp_tsk` / `wup_tsk` and loops continuously**:

```c
while (1) {
    task_count[2]++;

    /* Same LOCK/REST/TRY 3-phase as Task 3 */
    if (have_sem) {
        /* LOCK phase: increment shared_count while holding semaphore */
        shared_count++;
        phase_cnt++;
        if (phase_cnt >= SEM_HOLD) {
            sig_sem(1);         /* Release semaphore */
            have_sem = 0;
            phase_cnt = 0;      /* → Transition to REST phase */
        }
    } else if (phase_cnt < SEM_REST) {
        /* REST phase: don't touch the semaphore (skip 40 iterations) */
        phase_cnt++;
    } else {
        /* TRY phase: attempt to acquire the semaphore */
        if (pol_sem(1) == E_OK) {
            have_sem = 1;
            phase_cnt = 0;      /* → Transition to LOCK phase */
            shared_count++;
        }
    }

    delay();
    /* ← No slp_tsk(). Keeps looping continuously */
}
```

Why Task 2 does not call `slp_tsk`: Task 2 is the only "continuously running" task on CPU 1.
If it went to sleep, there would be no one to wake it up (Task 4 only runs when a key is pressed).

### Cross-CPU Semaphore Contention

When Task 2 (CPU 1) and Task 3 (CPU 0) call `pol_sem(1)` simultaneously:

```
  CPU 0 (Task 3)              CPU 1 (Task 2)
  ──────────────              ──────────────
  pol_sem(1)                  pol_sem(1)
    │                           │
  int $0x99                   int $0x99
    │                           │
  c_intr_syscall              c_intr_syscall
    ├─ kernel_lk acquire ✓ ──┐     ├─ kernel_lk acquire → spin-wait
    │  sys_pol_sem          │     │  │
    │  semcnt=1, success    │     │  │
    │  semcnt=0             │     │  │
    ├─ kernel_lk release ────┘     │  │
    │                       ←───┘  │
    │                           ├─ kernel_lk acquire ✓
    │                           │  sys_pol_sem
    │                           │  semcnt=0, E_TMOUT (*)
    │                           ├─ kernel_lk release
    ▼                           ▼
  LOCK displayed              BUSY displayed

* ITRON's pol_sem is a polling (non-blocking) acquire.
  Returning E_TMOUT when the resource is unavailable is per specification
  (equivalent to TMO_POL). It means "immediate failure", not "timed out".
```

`kernel_lk` (BKL) guarantees atomicity across CPUs.
Since `c_intr_syscall` acquires `kernel_lk` before calling `itron_syscall`,
two CPUs can never decrement the semaphore counter simultaneously.

---

## 6. Task 4 (kbd_task) Behavior

**File**: `kernel/user.c:365`
**CPU**: 1, **Priority**: 1 (highest)

Task 4 is an **event-driven task** that processes keyboard input.
At startup, it registers DTQ ID (= 2) with the keyboard driver via the
`set_key_task(2)` syscall (so the ISR knows which DTQ to send characters to via `ipsnd_dtq`).
Normally it blocks on `rcv_dtq(2, &data)` waiting for DTQ 2.
When a key is pressed, the ISR sends a character to DTQ 2, waking Task 4.
Received characters are accumulated in a local buffer (`line_buf[64]`), and on Enter or
when the line is full, `psnd_mbf(1, line_buf, line_pos)` sends the line string to
Task 1 via MBF 1. Backspace is also supported.
Screen display uses the `print_at()` syscall, and line clearing uses the `fill_at()` syscall.
Direct writes to the VGA buffer (0xB8000) are impossible due to Supervisor page protection.

### Operation Flow

```
                   CPU 0                          CPU 1
                   ─────                          ─────
                                            Task 4: Blocked on rcv_dtq(2) (TTS_WAI)
  Key is pressed
        │
  IRQ1 → PIC → Delivered to CPU 0
        │
  key_intr() (ISR)
    ├─ Read scan code
    ├─ Convert to ASCII
    └─ ipsnd_dtq(0, 2, ch)
         │  (kernel_lk already acquired by c_intr_irq1)
         ├─ Store character in DTQ 2
         ├─ Task 4 is waiting on DTQ 2 → wake it
         │    tsk[4].tskstat = TTS_RDY
         │    sched_ins(1, &tsk[4].plink)
         └─ sched_next_tsk(0)
              next_tsk_flag[0] = 1
              next_tsk_flag[1] = 1  ──→  Notify CPU 1 as well
                                          │
                                    Next APIC timer interrupt
                                          │
                                    intr_leave → sched_next_tsk_check(1)
                                          │
                                    sched_do_next_tsk(1)
                                      Task 4 (priority 1) > Task 2 (priority 15)
                                      → Switch to Task 4
                                          │
                                    Task 4 runs
                                      ├─ rcv_dtq(2) returns the character
                                      ├─ Buffer in line_buf
                                      ├─ Echo display via print_at() (syscall)
                                      ├─ Enter or line full: psnd_mbf(1, buf, len)
                                      │    Send line to Task 1 via MBF 1
                                      └─ Block again on rcv_dtq(2) (TTS_WAI)
                                          │
                                    Return to Task 2
```

### How Preemption Works

Task 4 has priority 1, Task 2 has priority 15.
When Task 4 becomes RDY, `sched_do_next_tsk` searches the priority queue from the top
and finds Task 4, triggering a switch.

```c
/* sched.c: sched_do_next_tsk */
for (i = 1; i <= TMAX_TPRI; i++) {
    /* Search from priority 1 → Task 4 is found first */
    if (t->tskstat == TTS_RDY && proc[t->tskid].cpu == apic)
        /* Switch to Task 4 */
}
```

Task 2 is temporarily changed to TTS_RDY, and when Task 4 blocks again on `rcv_dtq(2)` (TTS_WAI),
Task 2 is selected again and resumes.

### Line String Transfer via MBF

Task 4 sends accumulated line strings to Task 1 via **message buffer 1**:

```
  Task 4 (CPU 1)                       MBF 1                 Task 1 (CPU 0)
  ──────────────                  (256B ring buffer)          ──────────────
  psnd_mbf(1, "hello", 5) ──→  [5|hello]  ──→  trcv_mbf(1, buf, 20)
                                                 → return value 5, buf="hello"
```

`psnd_mbf` is a non-blocking send. It returns `E_TMOUT` if the buffer is full.
`trcv_mbf` is a blocking receive with timeout. It returns the message size (a positive value)
if a message arrives, or `E_TMOUT` if nothing arrives within 20 ticks.

---

## 7. Tasks 5 and 6 (idle_task) Role

**File**: `kernel/user.c:354`
**CPU**: 0 (Task 5) / 1 (Task 6), **Priority**: 16 (lowest)

```c
void idle_task(void)
{
    while (1)
        __asm__ volatile("pause");
}
```

idle_task simply idles the CPU and serves as a "catch-all" when no other tasks
are available to run.

### Why It Is Necessary

The scheduler (`sched_do_next_tsk`) searches the priority queue for RDY tasks.
If all tasks on a CPU are in WAI state, no RDY task is found, and the scheduler
continues running the current task (a "ghost" problem where a WAI task keeps executing).

idle_task is always RDY and has the lowest priority, so:
- If other tasks are RDY, they are selected first
- If all tasks are WAI, idle_task is selected and safely idles

---

## 8. Inter-Task Communication Overview

```
         CPU 0                                        CPU 1
  ┌───────────────────┐                        ┌───────────────────┐
  │ Task 1  (pri 15)  │◄──── MBF 1 ──────────│ Task 4  (pri  1)  │
  │     ↕ slp/wup     │   (line string send)   │   rcv_dtq(2)      │
  │ Task 3  (pri 15)  │                        │ Task 2  (pri 15)  │
  │ Task 5  (idle)    │                        │ Task 6  (idle)    │
  └───────────────────┘                        └───────────────────┘

  Task 3 ─ pol/sig_sem(1) ─┐            ┌─ pol/sig_sem(1) ─ Task 2
                            ▼            ▼
                       ┌─────────────────┐
                       │      sem 1      │
                       │  shared_count   │
                       └─────────────────┘

  IRQ1 (PIC → CPU 0): key_intr() ── ipsnd_dtq(2) ──→ DTQ 2 → Task 4 wakes
  Task 4 (pri 1) wakes on DTQ receive and preempts Task 2 (pri 15)
```

### Summary of Communication Mechanisms

| Mechanism | Usage | Description |
|-----------|-------|-------------|
| `slp_tsk` / `wup_tsk` | Task 1 ↔ Task 3 | Alternating execution on same CPU |
| `ipsnd_dtq` / `rcv_dtq` | ISR → DTQ 2 → Task 4 | Task wake-up via DTQ on key interrupt |
| `pol_sem` / `sig_sem` | Task 2 vs Task 3 | Cross-CPU mutual exclusion |
| `psnd_mbf` / `trcv_mbf` | Task 4 → MBF 1 → Task 1 | Cross-CPU line string send/receive |
| Preemption | Task 4 preempts Task 2 | Priority-based interrupt |

---

## 9. Scheduler and Task Switching

### When Task Switches Occur

This kernel has **no forced preemption by timer**. Switches occur on two occasions:

1. **Syscall-triggered**: `sched_next_tsk()` is called inside `slp_tsk()`, `wup_tsk()`, etc.
2. **On interrupt return**: `intr_leave` checks `next_tsk_flag` and calls `sched_do_next_tsk()`

### sched_next_tsk — Reschedule Request

```c
/* sched.c */
ID sched_next_tsk(W apic)
{
    next_tsk_flag[0] = 1;   /* Set flag for both CPUs */
    next_tsk_flag[1] = 1;
    return E_OK;
}
```

This function only sets flags; it does not perform the actual switch.
The switch happens during `intr_leave` on return from an interrupt.

### intr_leave → sched_next_tsk_check → sched_do_next_tsk

```
  Interrupt handler (after returning from C function)
    │
    jmp intr_return
    │
    call intr_leave
    │
    ├─ Decrement k_nest
    ├─ k_nest == 0? (Returning from outermost interrupt?)
    │   ├─ YES:
    │   │   ├─ current_proc[cpu]->kern_esp = ESP  (Save current task's ESP)
    │   │   ├─ sched_next_tsk_check(apic)
    │   │   │         │
    │   │   │         ├─ next_tsk_flag[apic] != 0?
    │   │   │         │   ├─ YES: sched_do_next_tsk(apic)
    │   │   │         │   │         Search priority queue
    │   │   │         │   │         Update current_proc[apic]
    │   │   │         │   └─ NO: Do nothing
    │   │   │         └─ next_tsk_flag[apic] = 0
    │   │   ├─ ESP = current_proc[cpu]->kern_esp  (Load new task's ESP)
    │   │   └─ tss_update_esp0(cpu, kern_stack_top)  (Update TSS.esp0)
    │   └─ NO: Nested interrupt, do nothing
    │
    RESTORE_ALL  (Pop 9 registers: ES, DS, EDI, ESI, EBP, EBX, EDX, ECX, EAX)
    iret         (Pop EIP, CS, EFLAGS, ESP, SS → return to Ring 3)
```

### sched_do_next_tsk — Task Selection Algorithm

```c
/* sched.c */
ID sched_do_next_tsk(W apic)
{
    smp_lock(&kernel_lk);

    /* 1. Set current task back to RDY (for preemption) */
    old_id = c_tskid[apic];
    if (tsk[old_id].tskstat == TTS_RUN) {
        tsk[old_id].tskstat = TTS_RDY;
        was_run = 1;
    }

    /* 2. Search from priority 1 (highest) to 16 (lowest) */
    for (i = 1; i <= TMAX_TPRI; i++) {
        /* Find a RDY task at this priority level for this CPU */
        if (t->tskstat == TTS_RDY && proc[t->tskid].cpu == apic) {
            t->tskstat = TTS_RUN;
            current_proc[apic] = &proc[t->tskid];
            smp_unlock(&kernel_lk);
            return t->tskid;
        }
    }

    /* 3. If none found, restore the original task */
    if (was_run)
        tsk[old_id].tskstat = TTS_RUN;
    smp_unlock(&kernel_lk);
}
```

The CPU affinity filter (`proc[t->tskid].cpu == apic`) ensures each CPU
selects only tasks assigned to it.

---

## 10. Mutual Exclusion: Big Kernel Lock

In an SMP environment, two CPUs can access kernel data simultaneously.
tiny-itron protects all kernel data structures with a **Big Kernel Lock (BKL)** --
a single spinlock `kernel_lk`.

### Why BKL

All interrupt handlers are invoked through interrupt gates (GT_INTR), so the CPU
automatically clears IF on handler entry.
On a single CPU, there is no contention while interrupts are disabled, so the lock's
role is **cross-CPU exclusion only**.

With BKL, lock boundary errors **cannot occur in principle**.
For a 2-CPU educational kernel, guaranteeing correctness matters more than
the performance benefits of fine-grained locking.

### The 5 Lock Acquisition Sites

`kernel_lk` is acquired/released only at kernel entry points.
Each syscall function and timer handler operates under the assumption that
"caller holds kernel_lk" and does not perform lock operations internally.

| Acquisition site | File | Protected scope |
|-----------------|------|-----------------|
| `c_intr_syscall` | i386/syscall.c | Entire `itron_syscall` (all syscalls) |
| `c_intr_irq0` | i386/interrupt.c | `timer_intr(0,1)` + PIC EOI |
| `c_intr_irq1` | i386/interrupt.c | `key_intr()` + PIC EOI |
| `c_intr_smp_timer1` | i386/interrupt.c | `timer_intr(1,1)` + APIC EOI |
| `sched_do_next_tsk` | kernel/sched.c | Entire function (called via `intr_leave`) |

Only `sched_do_next_tsk` acquires and releases the lock itself.
The other four sites acquire it right after entering the kernel and release it just before returning.

### Why video_lk Is Separate

`printk` can be called from both syscalls (`sys_printf`) and ISRs.
ISRs execute while holding `kernel_lk`, so if `printk` tried to acquire `kernel_lk`,
it would **deadlock on the same CPU** (the `xchgl` spinlock is non-reentrant).
Therefore, VGA output protection uses a separate lock variable `video_lk`.

In the current runtime code, no ISR calls `printk`
(screen updates use lock-free functions like `vga_write_dec_at`), but inserting
`printk` into ISRs during debugging is common, so the separation is maintained
as a safety measure.

### Relationship with IF=0

Since interrupt gates (GT_INTR) set IF=0, interrupts do not fire on the same CPU
while kernel code is executing.
Therefore, the BKL is responsible **only for exclusion between the two CPUs**.

```
  CPU 0                           CPU 1
  ─────                           ─────
  int $0x99 → IF=0                APIC timer → IF=0
  smp_lock(&kernel_lk) ✓          smp_lock(&kernel_lk) → spin-wait
  itron_syscall(...)               │
    sys_slp_tsk(...)              │ (spins until CPU 0 releases)
  smp_unlock(&kernel_lk)          │
                              ←─ Lock acquired ✓
                                  timer_intr(1,1)
                                  smp_unlock(&kernel_lk)
```

### Code Example: sys_pol_sem (BKL Version)

Since `kernel_lk` is already acquired in `c_intr_syscall`,
there are no lock operations inside the syscall function:

```c
/* sys_sem.c: sys_pol_sem */
ER sys_pol_sem(W apic, ID semid)
{
    /* caller holds kernel_lk */
    if (semid < 1 || semid > MAX_SEMID)
        return E_ID;
    if (sem[semid].semcnt == 0)
        return E_TMOUT;         /* No resource available */
    sem[semid].semcnt--;        /* Acquire resource */
    return E_OK;
}
```

Cross-CPU exclusion is guaranteed by the outer `kernel_lk`, so two CPUs
can never read/write `semcnt` simultaneously.

### Pseudocode for sched_do_next_tsk

Since `sched_do_next_tsk` is called from `intr_leave`, it acquires the lock
itself, unlike the syscall path:

```c
ID sched_do_next_tsk(W apic)
{
    smp_lock(&kernel_lk);

    /* Set current task back to RDY (for preemption) */
    if (tsk[old_id].tskstat == TTS_RUN)
        tsk[old_id].tskstat = TTS_RDY;

    /* Search from priority 1 (highest) → 16 (lowest) for RDY tasks */
    for (i = 1; i <= TMAX_TPRI; i++) {
        if (t->tskstat == TTS_RDY && proc[t->tskid].cpu == apic) {
            t->tskstat = TTS_RUN;
            current_proc[apic] = &proc[t->tskid];
            smp_unlock(&kernel_lk);
            return t->tskid;
        }
    }

    /* No RDY task found → restore original task */
    smp_unlock(&kernel_lk);
    return E_ID;
}
```

Since all `tskstat` modifications occur within the lock,
races where two CPUs modify task state simultaneously are prevented.

### Spinlock Implementation

```c
/* smp.c */
void smp_lock(unsigned long *p)
{
    while (cxchg(p, 1))           /* Atomically exchange via xchgl instruction */
        __asm__ volatile("pause"); /* Save power while spinning with pause */
}

void smp_unlock(unsigned long *p)
{
    cxchg(p, 0);
}
```

`xchgl` is an x86 atomic instruction that indivisibly exchanges the values in
memory and a register. Since it does not use `cli`/`sti`, **it can be called
from Ring 3 (user mode)**. This is what distinguishes it from `cpu_lock`/`cpu_unlock`
(cli/sti-based, Ring 0 only).

---

## 11. Interrupt Flow

### Interrupt Sources and Delivery Targets

| Interrupt source | Vector | Delivery target | Handler |
|-----------------|--------|-----------------|---------|
| PIT (timer) | IRQ0 (0x80) | CPU 0 only | `c_intr_irq0` → `timer_intr(0, 1)` |
| Keyboard | IRQ1 (0x81) | CPU 0 only | `c_intr_irq1` → `key_intr()` |
| APIC timer 0 | 0x9A | CPU 0 | `c_intr_smp_timer0` → EOI only |
| APIC timer 1 | 0x9B | CPU 1 | `c_intr_smp_timer1` → EOI only |
| syscall | 0x99 | Calling CPU | `c_intr_syscall` → `itron_syscall` |

IRQs via the PIC (i8259) are delivered only to CPU 0.
CPU 1 uses only the APIC timer as its timer source.

### Full Flow of a Keyboard Interrupt

```
1. Key press
     ↓
2. IRQ1 → PIC → Delivered to CPU 0 (does not go to CPU 1)
     ↓
3. intr.s: intr_irq1
     ├─ SAVE_ALL             Save registers (build pt_regs)
     ├─ call intr_enter      k_nest0++
     ├─ call c_intr_irq1
     │   └─ key_intr()
     │       ├─ inb(IO_KEY)      Read scan code
     │       ├─ Ctrl+C check → CPU reset if matched (see below)
     │       ├─ Convert to ASCII
     │       └─ ipsnd_dtq(0, key_dtq_id, ch)
     │            │  key_dtq_id = 2 (set by kbd_task via set_key_task(2))
     │            │  (kernel_lk already acquired by c_intr_irq1)
     │            ├─ Store character in DTQ 2
     │            ├─ Task 4 waiting on rcv_dtq(2) → wake it
     │            │    tsk[4].tskstat = TTS_RDY
     │            │    sched_ins(1, &tsk[4].plink)
     │            └─ sched_next_tsk(0)
     │                 next_tsk_flag[0] = 1
     │                 next_tsk_flag[1] = 1
     │
     ├─ jmp intr_return
     │   ├─ intr_leave
     │   │   └─ sched_next_tsk_check(0)
     │   │       (Task 4 is not on CPU 0, so no switch occurs)
     │   ├─ RESTORE_ALL
     │   └─ iret

     --- Meanwhile on CPU 1 ---

4. APIC timer interrupt (fires periodically)
     ↓
5. intr.s: intr_smp_timer1
     ├─ SAVE_ALL
     ├─ call intr_enter      k_nest1++
     ├─ call c_intr_smp_timer1
     │   └─ smp_eoi()         # EOI only (no timeout processing)
     ├─ jmp intr_return
     │   ├─ intr_leave
     │   │   ├─ sched_next_tsk_check(1)
     │   │   │   ├─ next_tsk_flag[1] == 1 → sched_do_next_tsk(1)
     │   │   │   ├─ Find Task 4 (priority 1, cpu=1, RDY)
     │   │   │   └─ current_proc[1] = &proc[4]
     │   │   ├─ ESP = proc[4].kern_esp (Task 4's kernel stack)
     │   │   └─ tss_update_esp0(1, proc[4].kern_stack_top)
     │   ├─ RESTORE_ALL (pop Task 4's registers)
     │   └─ iret → resume in Task 4's context
```

### Role of the APIC Timer

CPU 0's APIC timer only sends EOI (`c_intr_smp_timer0`).
The PIT (IRQ0) handles timer ticks.

CPU 1's APIC timer also only sends EOI (`c_intr_smp_timer1`).
Only CPU 0's PIT performs delta decrements on the timeout queue. The APIC timer
provides the opportunity for preemptive task switches
(`intr_leave` → `sched_next_tsk_check`).
Task switches on CPU 1 happen on the return path from these APIC timer interrupts.

---

## 12. VGA Screen Layout

Screen during steady-state operation (screenshot at a [LOCK] display moment):

![screenshot](../../screenshot.png)

```
  Row:
   0:  TinyITRON/386 SMP (2 CPU)                      [Ctrl+C to quit]
   1:  ======================================================================...
   2:
   3:    Timer      tick =     25366
   4:
   5:  [CPU0] Task1 \ #    1151   mbf: -
   6:
   7:  [CPU0] Task3 - #    1150   [LOCK]---+
   8:
   9:                            +---> Shared (sem 1)   #     3095
  10:
  11:  [CPU1] Task2 : #   10856   [BUSY]---+
  12:
  13:  [CPU1] Task4   > _
  14:
  15:
  16:                      .---------.             .---------.
  17:                      |  CPU 0  |             |  CPU 1  |
  18:                      | Task1,3 |---[ BKL ]---| Task2,4 |
  19:                      |  Idle5  |             |  Idle6  |
  20:                      '---------'             '---------'
  21:                            \                     /
  22:                             '--- Shared Memory ---'
  23:
  24:  Copyright (c) 2000-2026 t-ishii66. All rights reserved.
```

| Row | Content | Updated by |
|-----|---------|------------|
| 0 | Header | Task 1 (`draw_header`) |
| 1 | Separator | Task 1 (`draw_header`) |
| 3 | PIT timer tick | `timer_intr` (ISR, direct VGA write from Ring 0) |
| 5 | Task 1 count + MBF received string | Task 1 |
| 7 | Task 3 count + semaphore state | Task 3 |
| 8 | Diagonal arrow `\` (shown only while Task 3 holds the semaphore) | Task 3 |
| 9 | `shared_count` value (green=Task3, magenta=Task2) | Task 2 or Task 3 |
| 10 | Diagonal arrow `/` (shown only while Task 2 holds the semaphore) | Task 2 |
| 11 | Task 2 count + semaphore state | Task 2 |
| 13 | Task 4 keyboard echo | Task 4 |
| 16-22 | SMP architecture diagram (static) | Task 1 (`draw_header`) |
| 24 | Copyright (static) | Task 1 (`draw_header`) |

Semaphore display (rows 7, 11):
- `[LOCK]---+` (yellow) + diagonal arrow (green/magenta): Semaphore held, `shared_count` increasing
- `[BUSY]---+` (red): `pol_sem` returned `E_TMOUT` (other CPU holds the lock)
- Blank: Resting phase (not touching the semaphore)

`+---> Shared (sem 1)` and `shared_count` color:
- **Green** = Task 3 (CPU 0) updated last
- **Magenta** = Task 2 (CPU 1) updated last
- **Dark gray** = Neither task holds the semaphore

**VGA access and syscalls**: The VGA text buffer (0xB8000) is mapped as a Supervisor page,
so direct access from Ring 3 user tasks causes a #PF (page fault).
All screen writes go through the `print_at()`, `print_dec_at()`, `fill_at()`, and
`clear_screen()` syscalls (defined in lib/lib_exd.c), which execute inside the kernel.
Only the timer tick (row 3) writes directly to VGA from the ISR (Ring 0).

---

## 13. Timeline: One Cycle of Steady-State Operation

How CPU 0 and CPU 1 operate in parallel:

```
Time →

CPU 0:
  ┌─ Task 1 ─┐  ┌─ Task 3 ─┐  ┌─ Task 1 ─┐  ┌─ Task 3 ─┐
  │ count++   │  │ count++   │  │ count++   │  │ count++   │
  │ trcv_mbf  │  │ pol_sem   │  │ trcv_mbf  │  │ pol_sem   │
  │ delay()   │  │ delay()   │  │ delay()   │  │ delay()   │
  │ wup_tsk(3)│  │ wup_tsk(1)│  │ wup_tsk(3)│  │ wup_tsk(1)│
  │ slp_tsk() │  │ slp_tsk() │  │ slp_tsk() │  │ slp_tsk() │
  └───────────┘  └───────────┘  └───────────┘  └───────────┘

CPU 1:
  ┌──────────── Task 2 ─────────────────────────────────────────┐
  │ count++  delay()  count++  delay()  count++  delay()  ...   │
  │ pol_sem           pol_sem           pol_sem                 │
  └──────────────────────────────┬──────┬────────────────────────┘
                                 │      │
                           ┌─ Task 4 ─┐
                           │ rcv_dtq(2)│  ← Only on key interrupt
                           │ echo+buf  │
                           │ Enter:    │
                           │ psnd_mbf  │
                           │ rcv_dtq(2)│
                           └───────────┘
                                 │
                   ←─── Return to Task 2 ───→

  Timer interrupts:
  ↑   ↑   ↑   ↑   ↑   ↑   ↑   ↑   ↑   ↑
  PIT (CPU0)         APIC (CPU1)
```

- On CPU 0, Task 1 and Task 3 alternate in an orderly fashion via `slp_tsk`/`wup_tsk`
- On CPU 1, Task 2 runs almost exclusively, with Task 4 preempting only on key input
- Semaphore contention occurs asynchronously (Task 2 loops continuously, Task 3 alternates)
- The `shared_count` color switching between green and magenta on screen
  provides visual confirmation of cross-CPU semaphore contention

---

## 14. System Halt via Ctrl-C

### Overview

When the user presses **Ctrl-C**, the system halts safely.
Combined with QEMU's `-no-reboot` option, this causes the VM to shut down cleanly.

### Operation Flow

```
  Press Ctrl on keyboard
    │
  IRQ1 → key_intr()
    ├─ scancode 0x1d → mode |= CTRL
    └─ return (not a character key, so nothing is buffered)

  Then press C on keyboard
    │
  IRQ1 → key_intr()
    ├─ scancode 0x2e, mode & CTRL == true
    ├─ cli                          Disable interrupts
    ├─ vga_write_at(12, 28,         Display message
    │    "  System halted.  ",      in center of screen
    │    0x4F)                      (white text, red background)
    ├─ outb(0x64, 0xFE)             Pulse CPU reset line
    │                               via keyboard controller
    └─ while(1) { hlt; }            Fallback if reset fails
```

### CPU Reset via Keyboard Controller

```c
/* keyboard.c: Ctrl+C handler */
if ((mode & CTRL) && c == 0x2e) {
    __asm__("cli");
    vga_write_at(12, 28, "  System halted.  ", 0x4F);
    outb(IO_KEY_CS, 0xFE);     /* Send 0xFE to port 0x64 */
    while (1) { __asm__("hlt"); }
}
```

Writing command 0xFE to port 0x64 of the i8042 keyboard controller causes
the controller to pulse the CPU reset line. This is a standard reboot method
for IBM PC/AT compatible machines.

### Behavior in QEMU

QEMU is launched with the `-no-reboot` option (`run.sh`).
When a CPU reset occurs, QEMU **shuts down the VM** instead of rebooting.

- curses mode (`./run.sh`): Control returns to the terminal
- GTK mode (`./run.sh -g`): The window closes
- nographic mode (`./run.sh -n`): The process exits

This allows the user to naturally terminate the QEMU session with Ctrl-C.

The Ctrl-C handling proceeds as `cli` → VGA display → CPU reset inside `key_intr()`,
so it completes reliably without being interrupted by other interrupts.
