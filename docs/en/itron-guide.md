# Micro ITRON 4.0 Guide — Background for Reading tiny-itron

This document explains the Micro ITRON 4.0 concepts and APIs used
in the tiny-itron source code.
The ITRON specification is vast, but this guide covers **only the features actually used by this code**.

> **Note:** tiny-itron is a rough (partial) implementation of the Micro ITRON 4.0 specification,
> and many APIs are implemented as stubs or in simplified form.
> The explanations in this document also include simplifications specific to the tiny-itron implementation,
> and may differ from the precise definitions in the specification.
> For detailed API specifications, error conditions, and edge cases, always refer to the primary source:
>
> - **µITRON4.0 Specification Ver. 4.00.00**
>   <https://libdrc.org/ITRON/SPEC/FILE/mitron-400e.pdf>


---

## Table of Contents

1. [What is Micro ITRON?](#1-what-is-micro-itron)
2. [Core Concepts: Tasks and State Transitions](#2-core-concepts-tasks-and-state-transitions)
3. [Task Management API](#3-task-management-api)
4. [Synchronization: Semaphores](#4-synchronization-semaphores)
5. [Communication: Data Queues](#5-communication-data-queues)
6. [Interrupt Context](#6-interrupt-context)
7. [How syscalls Work](#7-how-syscalls-work)
8. [Types and Error Codes (Reference)](#8-types-and-error-codes-reference)
9. [ITRON Features Not Used in This Code](#9-itron-features-not-used-in-this-code)

---

## 1. What is Micro ITRON?

ITRON (Industrial TRON) is part of the TRON Project, which started in 1984.
It is a real-time OS **specification** for embedded systems.

Key characteristics:

- **Specification only — each party implements their own kernel.** Unlike Linux, where there is a single source tree,
  each company or individual implements their own kernel following the specification.
  (tiny-itron is an implementation of a subset of the ITRON specification.)
- **Primarily static design**: µITRON4.0 does include dynamic creation APIs like `acre_tsk` / `acre_sem`,
  but in embedded use it is common to fix the number of objects at build time.
  tiny-itron uses fixed upper limits such as `MAX_TSKID = 16`.
- **Real-time**: The emphasis is on guaranteeing an upper bound on response time.
  Priority-based preemptive scheduling is the fundamental approach.

tiny-itron targets the **Micro ITRON 4.0.0 specification** (1999).
The version number is recorded in `include/config.h`:

```c
#define TKERNEL_SPVER   0x5400    /* Micro ITRON v4.0.0 */
```

---

## 2. Core Concepts: Tasks and State Transitions

### What is a Task?

A **task** in ITRON is a unit of execution.
It is similar to a UNIX thread — each task has its own stack and priority.
The µITRON4.0 specification does not prescribe address spaces or memory protection; these are implementation-dependent.
In tiny-itron, all tasks share the same address space and there is no memory protection.

### Task IDs and Priorities

- **Task ID**: 1 through `MAX_TSKID` (16). 0 is the invalid value (`TSK_NONE`).
- **Priority**: 1 (highest) through `TMAX_TPRI` (16, lowest).
  A **smaller number means higher priority**.

### State Transitions

ITRON strictly defines task states.
The five states used in this code are:

```
                     cre_tsk
    [NON-EXISTENT] ─────────→ [DORMANT]
      (TTS_NON)                (TTS_DMT)
                                  │
                              act_tsk
                                  │
                                  ▼
                         ┌──────────────────────┐
                         │                      │
                         ▼                      │
                      [READY] ←──────┐          │
                      (TTS_RDY)      │          │
                         │           │          │ wup_tsk
                      Scheduler    wup_tsk      │ sig_sem
                       selects    sig_sem      │ snd_dtq
                         │        snd_dtq      │ rel_wai
                         ▼        rel_wai      │
                      [RUNNING] ─────┘          │
                      (TTS_RUN)  Preemption     │
                         │                      │
                      slp_tsk                   │
                      wai_sem                   │
                      rcv_dtq                   │
                         │                      │
                         ▼                      │
                      [WAITING] ───────────────┘
                      (TTS_WAI)   When condition
                                  is satisfied
```

Meaning of each state:

| Constant | Value | Meaning |
|------|------|------|
| `TTS_NON` | 0x00 | Non-existent (task does not exist) |
| `TTS_DMT` | 0x10 | Dormant (created but not started) |
| `TTS_RDY` | 0x02 | Ready (waiting for CPU) |
| `TTS_RUN` | 0x01 | Running (executing on a CPU) |
| `TTS_WAI` | 0x04 | Waiting (waiting for an event) |

The specification also defines `TTS_SUS` (suspended) and `TTS_WAS` (waiting-suspended),
but they are not used in this code's demo.

---

## 3. Task Management API

### cre_tsk — Create a Task

```c
ER cre_tsk(ID tskid, T_CTSK *pk_ctsk);
```

Creates a new task with ID `tskid`. After creation, the task is in the **DORMANT** state.
Parameters are passed via the `T_CTSK` structure:

```c
typedef struct t_ctsk {
    ATR     tskatr;     /* Task attributes (usually 0) */
    VP_INT  exinf;      /* Extended information (argument to task function) */
    FP      task;       /* Task start address (function pointer) */
    PRI     itskpri;    /* Initial priority */
    SIZE    stksz;      /* Stack size (bytes) */
    VP      stk;        /* Pointer to stack area */
} T_CTSK;
```

### act_tsk — Activate a Task

```c
ER act_tsk(ID tskid);
```

Transitions a DORMANT task to the **READY** state.
If called on a task that is already READY/RUNNING, the activation request
is queued (`actcnt` is incremented).

### slp_tsk / wup_tsk — Sleep and Wakeup

```c
ER slp_tsk(void);      /* Put the calling task into WAITING */
ER wup_tsk(ID tskid);  /* Wake up the specified task */
```

The simplest form of inter-task synchronization. `slp_tsk` puts the calling task to sleep,
and another task wakes it up with `wup_tsk`.

### Actual Code Example: Alternating Execution of Task 1 and Task 3

In `kernel/user.c`, Task 1 and Task 3 alternate execution using
`wup_tsk` / `slp_tsk`:

```c
/* first_task (Task 1) — kernel/user.c */
void first_task(void)
{
    /* ... create semaphore, Task 3, etc. ... */

    while (1) {
        task_count[1]++;
        /* ... screen output ... */
        delay();
        wup_tsk(3);     /* Wake up Task 3 */
        slp_tsk();       /* Go to sleep */
    }
}

/* usr_main (Task 3) — kernel/user.c */
void usr_main(VP_INT arg)
{
    slp_tsk();           /* Wait for the first wakeup */

    while (1) {
        task_count[3]++;
        /* ... processing ... */
        delay();
        wup_tsk(1);     /* Wake up Task 1 */
        slp_tsk();       /* Go to sleep */
    }
}
```

This "wake the other, then sleep" pattern makes the two tasks alternate, each running once in turn.

### Actual Code Example: Task Creation

Code where Task 1 creates a semaphore, a message buffer, and Task 3:

```c
/* Beginning of first_task — kernel/user.c */
T_CTSK ctsk;

ctsk.task    = (FP)usr_main;        /* Entry function for Task 3 */
ctsk.stk     = tsk_stack_alloc(1024); /* 1024-byte stack (for syscall wrapper) */
ctsk.stksz   = 1024;
ctsk.itskpri = TMAX_TPRI - 1;      /* Priority 15 */
ctsk.exinf   = 3;                   /* Extended information (task ID) */
cre_tsk(3, &ctsk);                  /* Create with ID=3 */
act_tsk(3);                         /* Activate */
```

---

## 4. Synchronization: Semaphores

### What is a Semaphore?

A semaphore is a synchronization object with a counter.
It manages a "resource count" and blocks tasks when no resources are available.

When the counter only takes values 0 or 1, it is called a **binary semaphore**
and can be used for mutual exclusion (mutex).

### cre_sem — Create a Semaphore

```c
ER cre_sem(ID semid, T_CSEM *pk_csem);
```

Parameter structure:

```c
typedef struct t_csem {
    ATR     sematr;     /* Attributes (TA_TFIFO: wait queue is FIFO) */
    UINT    isemcnt;    /* Initial count */
    UINT    maxsem;     /* Maximum count */
} T_CSEM;
```

### sig_sem / pol_sem — Signal and Polling Acquire

```c
ER sig_sem(ID semid);   /* Count +1 (wakes a waiting task if any) */
ER pol_sem(ID semid);   /* Count -1 (returns E_TMOUT immediately if 0) */
```

`pol_sem` is the **non-blocking** (polling) version.
If the count is 0, it returns `E_TMOUT` without blocking the task.

The specification also defines `wai_sem` (blocking version). If the count is 0,
it puts the task into WAITING until another task calls `sig_sem`.
This demo only uses `pol_sem`.

### Actual Code Example: Cross-CPU Semaphore Contention

Task 2 (CPU 1) and Task 3 (CPU 0) protect `shared_count` with
binary semaphore 1:

```c
/* Semaphore creation (first_task) — kernel/user.c */
T_CSEM csem;
csem.sematr  = TA_TFIFO;
csem.isemcnt = 1;       /* Initial value 1 (only one task can acquire) */
csem.maxsem  = 1;       /* Binary semaphore */
cre_sem(1, &csem);
```

```c
/* Task 3's semaphore acquisition loop — kernel/user.c */
if (pol_sem(1) == E_OK) {       /* Acquired (LOCK) */
    shared_count++;
    /* ... repeat SEM_HOLD times ... */
    sig_sem(1);                  /* Release */
} else {
    /* E_TMOUT: other CPU holds the lock (BUSY) */
}
```

The VGA screen displays `LOCK` (holding), `BUSY` (contended), and `----` (idle),
giving a visual demonstration that the two CPUs are performing mutual exclusion via the semaphore.

---

## 5. Communication: Data Queues

### What is a Data Queue?

A data queue is a message queue that sends and receives integer-sized data in FIFO order.
Internally, it is implemented as a ring buffer.

While a semaphore only provides a "yes/no" synchronization signal,
a data queue can carry **messages with values**.

### cre_dtq — Create a Data Queue

```c
ER cre_dtq(ID dtqid, T_CDTQ *pk_cdtq);
```

Parameter structure:

```c
typedef struct t_cdtq {
    ATR     dtqatr;     /* Attributes (TA_TFIFO) */
    UINT    dtqcnt;     /* Number of queue elements */
    VP      dtq;        /* Buffer address (NULL = kernel allocates) */
} T_CDTQ;
```

### psnd_dtq / prcv_dtq — Polling Send/Receive

```c
ER psnd_dtq(ID dtqid, VP_INT data);       /* Send (E_TMOUT if full) */
ER prcv_dtq(ID dtqid, VP_INT *p_data);    /* Receive (E_TMOUT if empty) */
```

Both are **non-blocking** versions. The specification also defines `snd_dtq` / `rcv_dtq`
(blocking versions). In this demo, kbd_task (Task 4) uses blocking receive
(`rcv_dtq`) on DTQ 2, and the ISR sends characters via `ipsnd_dtq`.
Note that the transfer from Task 4 to Task 1 uses a message buffer (MBF) rather than a data queue.

### Actual Code Example: Key Character Transfer from ISR to kbd_task

The keyboard ISR (CPU 0) sends received key characters to
kbd_task (Task 4, CPU 1) via a data queue:

```c
/* Data queue creation (second_task) — kernel/user.c */
T_CDTQ cdtq;
cdtq.dtqatr = TA_TFIFO;
cdtq.dtqcnt = 16;       /* Buffer for 16 elements */
cdtq.dtq    = 0;         /* NULL → kernel allocates */
cre_dtq(2, &cdtq);
```

```c
/* key_intr (ISR) — sender side */
ipsnd_dtq(0, key_dtq_id, ch);   /* Send character ch to DTQ 2 */
```

```c
/* kbd_task (Task 4) — receiver side */
VP_INT data;
rcv_dtq(2, &data);              /* Blocking receive from DTQ 2 */
c = (int)data;
```

`VP_INT` is a typedef for `int`, so a character code can be passed directly as an integer.

The transfer from kbd_task to first_task uses a message buffer (MBF).
See [keyboard.md](keyboard.md) sections 7 and 9 for details.

---

## 6. Interrupt Context

### Task Context vs. Non-Task Context

µITRON4.0 distinguishes between two execution contexts:

| | Task Context | Non-Task Context |
|---|---|---|
| Who executes | Normal tasks | Interrupt handlers, cyclic/alarm/CPU exception handlers, etc. |
| Can enter waiting state | Yes (`slp_tsk`, etc.) | No |
| Available APIs | All | Only `i`-prefixed versions |

Non-task context is in a state where "some work has been interrupted to handle this,"
so it cannot put itself to sleep with `slp_tsk`.
In tiny-itron, the only non-task context actually used is interrupt handlers (ISRs).

### iwup_tsk — Wake Up a Task from an Interrupt Handler

```c
ER iwup_tsk(W apic, ID tskid);
```

This is the non-task context version of `wup_tsk`.
The naming convention is that **APIs beginning with `i`** are for non-task context.
In tiny-itron, these are called from interrupt handlers (ISRs).

Other examples include `iact_tsk`, `isig_sem`, and `ipsnd_dtq`.

### Actual Code Example: Keyboard ISR

The complete flow of a keyboard interrupt (IRQ 1):

```
1. A key is pressed
2. IRQ1 → PIC → interrupt delivered to CPU 0
3. CPU 0's ISR (key_intr) is called
4. key_intr reads the scan code and converts it to ASCII
5. ipsnd_dtq(0, key_dtq_id, ch) sends the character to DTQ 2
6. Task 4, which was waiting on rcv_dtq for DTQ 2, wakes up (TTS_WAI → TTS_RDY)
7. Return from interrupt
8. On CPU 1's next APIC timer interrupt,
   the scheduler switches to Task 4 (highest priority)
9. Task 4's rcv_dtq(2) returns the character
```

ISR code (`i386/keyboard.c`):

```c
int key_intr(void)
{
    /* ... scan code → ASCII conversion ... */

    /* Send character to keyboard task via DTQ */
    if (key_dtq_id > 0)
        ipsnd_dtq(0, key_dtq_id, ch);

    return 0;
}
```

IRQ 1 is delivered to CPU 0 via the PIC, but Task 4 runs on CPU 1.
The `sched_next_tsk` call inside `ipsnd_dtq` sets the rescheduling flag on both CPUs,
so Task 4 is selected on CPU 1's next timer interrupt.

---

## 7. How syscalls Work

User tasks run in Ring 3 (unprivileged), while the kernel runs in Ring 0 (privileged).
When a user calls an ITRON API, it must cross the privilege level boundary.
This mechanism is called a **syscall** (system call).

### Overall Flow

```
User task (Ring 3)                   Kernel (Ring 0)
  │                                    │
  │  cre_tsk(3, &ctsk)                │
  │    ↓                              │
  │  lib/lib_tsk.c                    │
  │    syscall(-TFN_CRE_TSK,          │
  │            3, &ctsk)              │
  │    ↓                              │
  │  klib.s: syscall                  │
  │    int $0x99                      │
  │    ─────────────────────────→     │
  │                              intr.s: intr_syscall
  │                                SAVE_ALL (push 9 registers onto per-task
  │                                  kernel stack → build pt_regs)
  │                                intr_enter (k_nest++, CPU identification)
  │                                    ↓
  │                              i386/syscall.c: c_intr_syscall(pt_regs*)
  │                                    ↓
  │                              kernel/syscall.c: itron_syscall
  │                                    ↓
  │                              Look up handler from function code
  │                              via table in syscallP.h
  │                                    ↓
  │                              kernel/sys_tsk.c: sys_cre_tsk
  │                                    ↓
  │                              Write return value to regs->eax
  │                                    ↓
  │                              intr_leave (k_nest--, task switch decision)
  │                              RESTORE_ALL (pop 9 registers from pt_regs)
  │    ←─────────────────────────     │
  │  iret (return to Ring 3 with      │
  │        return value in EAX)       │
```

### Function Codes (TFN_xxx)

Each API is assigned a unique **function code**.
These are defined in `include/itron.h`:

```c
#define TFN_CRE_TSK    -0x05
#define TFN_ACT_TSK    -0x07
#define TFN_SLP_TSK    -0x11
#define TFN_WUP_TSK    -0x13
#define TFN_CRE_SEM    -0x21
#define TFN_POL_SEM    -0x26
#define TFN_SIG_SEM    -0x23
#define TFN_CRE_DTQ    -0x31
#define TFN_PSND_DTQ   -0x36
#define TFN_PRCV_DTQ   -0x3a
#define TFN_IWUP_TSK   -0x72
```

### Library Wrappers (lib/)

`lib/lib_tsk.c` and `lib/lib_sem.c` provide thin wrappers:

```c
/* lib/lib_tsk.c */
ER cre_tsk(ID tskid, T_CTSK *pk_ctsk)
{
    return syscall(-TFN_CRE_TSK, tskid, pk_ctsk);
}

ER slp_tsk(void)
{
    return syscall(-TFN_SLP_TSK);
}
```

`syscall()` is an assembly routine in `i386/klib.s` that issues `int $0x99`
with the arguments left on the stack.

### Dispatch Table

`kernel/syscallP.h` contains a mapping table from function codes to kernel functions.
`itron_syscall()` looks up this table and calls the corresponding handler.

---

## 8. Types and Error Codes (Reference)

### Principal Types (`include/itron.h`)

| Type | C Definition | Meaning |
|----|-----------|------|
| `ER` | `int` | Error code (return value) |
| `ID` | `int` | Object ID |
| `FP` | `void (*)()` | Function pointer |
| `PRI` | `int` | Priority |
| `ATR` | `unsigned int` | Object attribute |
| `STAT` | `unsigned int` | Object state |
| `TMO` | `unsigned int` | Timeout value |
| `RELTIM` | `unsigned int` | Relative time |
| `VP_INT` | `int` | Pointer or integer |
| `SIZE` | `unsigned int` | Memory size |
| `VP` | `char *` | Generic pointer |

### Principal Error Codes

| Constant | Value | Meaning |
|------|------|------|
| `E_OK` | 0 | Success |
| `E_SYS` | -5 | System error |
| `E_PAR` | -17 | Parameter error |
| `E_ID` | -18 | Invalid ID number |
| `E_OBJ` | -41 | Object state error |
| `E_NOEXS` | -42 | Object does not exist |
| `E_QOVR` | -43 | Queuing overflow |
| `E_TMOUT` | -50 | Timeout / polling failure |
| `E_DLT` | -51 | Waiting object was deleted |

When polling APIs like `pol_sem` or `prcv_dtq` return "no resource available," the error code is `E_TMOUT`.
The name is confusing, but in the ITRON specification, both polling failure and timeout
use the same error code.

### Attribute Constants

| Constant | Value | Meaning |
|------|------|------|
| `TA_TFIFO` | 0x00 | Wait queue in FIFO order |
| `TA_TPRI` | 0x01 | Wait queue in priority order |

### Timeout Constants

| Constant | Value | Meaning |
|------|------|------|
| `TMO_POL` | 0 | Polling (return immediately) |
| `TMO_FEVR` | -1 | Wait forever |

---

## 9. ITRON Features Not Used in This Code

The Micro ITRON 4.0 specification includes many features, but the demo in `kernel/user.c`
uses only a small subset. The following is a list of features that **have implementation code
in `kernel/` but are not called from the demo**:

| Category | Example APIs | Description |
|----------|--------|------|
| Event flags | `cre_flg`, `set_flg`, `wai_flg` | Synchronization via bit patterns |
| Mailboxes | `cre_mbx`, `snd_mbx`, `rcv_mbx` | Sending/receiving message pointers |
| Mutexes | `cre_mtx`, `loc_mtx`, `unl_mtx` | Mutual exclusion with priority inversion prevention |
| Message buffers | `cre_mbf`, `snd_mbf`, `rcv_mbf` | Variable-length messages (MBF 1 is in use) |
| Rendezvous | `cre_por`, `cal_por`, `acp_por` | Synchronous message exchange |
| Fixed-size memory pools | `cre_mpf`, `get_mpf`, `rel_mpf` | Fixed-size memory management |
| Variable-size memory pools | `cre_mpl`, `get_mpl`, `rel_mpl` | Variable-size memory management |
| Cyclic handlers | `cre_cyc`, `sta_cyc`, `stp_cyc` | Periodically invoked functions |
| Alarm handlers | `cre_alm`, `sta_alm`, `stp_alm` | Functions invoked at a specified time |
| Task exceptions | `def_tex`, `ras_tex` | Asynchronous exception notification to tasks |
| Overrun handlers | `def_ovr`, `sta_ovr` | CPU time monitoring |

If you are interested in these features, refer to the corresponding source files
under `kernel/` (e.g., `kernel/sys_flg.c`, `kernel/sys_mbx.c`, `kernel/sys_mtx.c`).
