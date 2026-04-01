# Timeout Processing

This document explains the internal mechanism of service calls with timeout
(`tslp_tsk`, `trcv_dtq`, `twai_sem`, etc.).

## Overview

In the ITRON specification, wait service calls accept a TMO (timeout value).
If the wait condition is not satisfied within the specified number of ticks,
the task is woken up with an `E_TMOUT` return value. This kernel manages
timeouts using a **delta-encoded doubly linked list**.

## Full Flow from Interrupt to Task Resumption

Timeout processing does not end with just manipulating data structures.
You need to understand the entire sequence: a hardware interrupt fires,
the timeout queue is processed, and the task resumes in Ring 3.

### Timer Interrupt Sources

Each CPU has multiple timers, but only one calls `sched_timeout`:

| CPU | Timer | Vector | Role |
|-----|-------|--------|------|
| CPU 0 | PIT (i8253) | IRQ0 (0x20) | **System tick**: ~17ms period (HZ=60). Calls `timer_intr(0,1)` -> `sched_timeout(0,1)`. The only timer that decrements deltas in the timeout queue |
| CPU 0 | APIC timer | 0xFD | **Task switch trigger only**: EOI only. Provides an opportunity to check `next_tsk_flag` since it passes through `intr_leave` |
| CPU 1 | APIC timer | 0xFE | **Task switch trigger only**: EOI only. Since PIT does not reach CPU 1, this is the sole trigger for preemptive switching |

> **Note**: The APIC timer on CPU 0 is essentially unnecessary. The PIT (IRQ0)
> already provides periodic interrupts to CPU 0 and also serves as a
> preemption trigger since it passes through `intr_leave`. The APIC timer on
> CPU 1 is indispensable since PIT does not reach CPU 1, but for CPU 0 the
> PIT alone suffices. Currently both CPUs have their APIC timers enabled
> simply as-is, with no deeper design intent.

**Important**: Only **CPU 0's PIT** calls `sched_timeout`. The timeout queue
is shared across all CPUs (protected by `kernel_lk`), but delta decrements
are based solely on PIT's ~17ms period. If multiple timers called
`sched_timeout`, deltas would be decremented twice, causing inaccurate
timeout durations.

The APIC timer's role is to provide a trigger for preemptive task switching.
After `sched_timeout` or `sched_next_tsk` sets `next_tsk_flag`, the next
APIC timer interrupt causes `intr_leave` to detect the flag and execute
a task switch via `sched_do_next_tsk`.

### Call Chain (Using CPU 0's PIT as an Example)

```
PIT fires (~17ms period, HZ=60)
  |
  v
CPU: Interrupts the Ring 3 task, pushes SS/ESP/EFLAGS/CS/EIP onto per-task kernel stack
  |
  v
intr_irq0 (intr.s):
  SAVE_ALL            <- Pushes 9 registers (EAX through ES) onto kernel stack (pt_regs complete)
  call intr_enter     <- k_nest[0]++
  call c_intr_irq0    <- Into C handler
  |
  v
c_intr_irq0 (interrupt.c):
  smp_lock(&kernel_lk)   <- Acquire Big Kernel Lock
  timer_intr(0, 1)       <- Common timer processing
  |  |
  |  v
  |  timer_intr (timer.c):
  |    timer_ticks++; screen update
  |    sched_timeout(0, 1)   <- * Timeout queue processing (described below)
  |       |
  |       +- If there are expired entries:
  |       |    tskstat = TTS_RDY, sched_ins(), proc_set_return_value(E_TMOUT)
  |       |    sched_next_tsk()  <- next_tsk_flag[0]=1, next_tsk_flag[1]=1
  |       |
  |       +- If no expired entries: do nothing
  |
  i8259_reenable()
  smp_unlock(&kernel_lk)     <- Release lock
  |
  v
intr_return (intr.s):
  call intr_leave            <- k_nest[0]--, task switch decision if it reaches 0
  |
  v
intr_leave (intr.s):                    * Only when k_nest[0] == 0
  (1) Save ESP to current_proc[0]->kern_esp
  (2) Call sched_next_tsk_check(0)
  |    +- If next_tsk_flag[0] is 1, call sched_do_next_tsk(0):
  |         Select the highest-priority TTS_RDY task from the priority queue,
  |         update current_proc[0]
  (3) Load (new) current_proc[0]->kern_esp into ESP
  (4) Update TSS.esp0 via tss_update_esp0()
  ret                        <- Pop the return address from the new ESP
  |
  v
intr_return_restore (intr.s):
  RESTORE_ALL   <- Pop ES,DS,EDI,...,EAX from the new task's kernel stack
                   (EAX contains E_TMOUT)
  iret          <- Pop EIP,CS,EFLAGS,ESP,SS -> return to Ring 3
  |
  v
Task resumes in Ring 3. EAX = E_TMOUT (-50).
The syscall wrapper (lib/lib_sem.c, etc.) returns EAX as the function's return value.
```

### Key Points

**sched_timeout only "schedules a wakeup"**. All `sched_timeout` does is
change the task's state to TTS_RDY, insert it into the scheduler queue, and
set `next_tsk_flag`. The actual CPU register switch (ESP swap) happens in
`intr_leave`. This separation means there is no need to worry about register
save consistency inside the timer ISR.

**E_TMOUT is written onto the kernel stack**. While a task is blocked, its
kernel stack holds the `pt_regs` frame saved by `SAVE_ALL`.
`proc_set_return_value(proc, E_TMOUT)` writes `E_TMOUT` to `pt_regs.eax`
located at `proc->kern_esp + 4`. Later, `RESTORE_ALL` pops this value into
the EAX register.

**Task switching on CPU 1**. CPU 1's APIC timer does not call
`sched_timeout`, but the CPU 1 path in `intr_leave` checks
`next_tsk_flag[1]`. If CPU 0's PIT handler set `next_tsk_flag[1]=1` via
`sched_next_tsk`, the task switch occurs on the return path from CPU 1's
next APIC timer interrupt.

## Delta-Encoded List

The timeout queue is a doubly linked list of `T_TIMEOUT` structures, with a
sentinel node `timeout` as the head. Each entry's `delta` field holds the
**relative tick count from the previous entry**.

```
timeout (sentinel)
  |
[delta=5] -> [delta=3] -> [delta=10] -> timeout
```

Absolute expiration ticks for this example:
- 1st entry: 5 ticks from now
- 2nd entry: 5+3 = 8 ticks from now
- 3rd entry: 5+3+10 = 18 ticks from now

Advantage: Per-tick processing only needs to decrement the head entry's
delta by 1. There is no need to traverse all entries.

## T_TIMEOUT Structure

```c
typedef struct timeout {
    struct timeout*  prev;
    struct timeout*  next;
    TMO              delta;   /* Relative tick count from previous entry */
} T_TIMEOUT;
```

This is embedded as the `tlink` field within each `T_TSK` structure.
The `tlink2tsk()` macro can compute the owning task structure from a
timeout entry.

## Insertion: sched_timeout_ins

`sched_timeout_ins(T_TIMEOUT* t)` traverses the list while decoding delta
values and inserts at the appropriate position.

**Algorithm** (`kernel/sched.c`):

1. Traverse from the head. If the current `tp->delta` is greater than or
   equal to the insertion entry's `t->delta`, insert here
2. During traversal, subtract each passed entry's delta from `t->delta`
3. Once the insertion position is found, adjust **only the immediately
   following entry** with `tp->delta -= t->delta`. Subsequent entries are
   relative to `tp`, so they need no change
4. Link `t` immediately after `tp->prev`

```
Before insertion: [5] -> [3] -> [10]    Insert: entry with delta=7

Traverse: tp=[5], 7 >= 5 -> t->delta = 7-5 = 2, continue
Traverse: tp=[3], 2 < 3  -> insert here
          Adjust tp onward: [3] -> [3-2=1], [10] -> unchanged

Result: [5] -> [2] -> [1] -> [10]
        Absolute values: 5, 7, 8, 18  (checkmark)
```

## Per-Tick Processing: sched_timeout

`sched_timeout(W apic, unsigned long delta)` is called from the PIT timer
(IRQ0) and APIC timer ISRs with `delta=1`.

**Processing flow**:

1. Decrement the head entry's `delta` by 1
2. Process all entries whose `delta <= 0` in a loop:
   a. Remove the entry from the list (add its residual delta to the next entry)
   b. If the task is TTS_WAI:
      - If it is in an object wait queue (sem/flg/dtq), remove it with `wlink_rem`
      - Set `E_TMOUT` in the task's EAX via `proc_set_return_value`
      - Transition to TTS_RDY and insert into the scheduler queue
   c. If the task is not TTS_WAI (orphan entry): remove only, do not change task state
3. If one or more tasks were woken up, reschedule via `sched_next_tsk`

**Important**: Multiple entries can expire in the same tick. For example, if
`dly_tsk(5)` and `trcv_dtq(dtq, &data, 5)` are registered simultaneously,
the second entry is inserted with `delta=0`. After 5 ticks, both expire.

## Removal: sched_timeout_rem / sched_timeout_rem_if_exist

When the wait condition is satisfied before the timeout (e.g., woken by
`wup_tsk` or `snd_dtq`), the timeout entry must be removed.

**Delta correction is essential**: If the removed entry's delta is not added
to the next entry, the absolute expiration times of subsequent entries will
be wrong.

```
Before removal: [5] -> [2] -> [10]
                Absolute values: 5, 7, 17

Removing [2]:
  Add delta=2 to the next [10] -> [10+2=12]

After removal: [5] -> [12]
               Absolute values: 5, 17  (checkmark)  (entry at tick 7 is gone, 17 is preserved)
```

`sched_timeout_rem` removes by direct pointer and resets tlink to
self-referencing. `sched_timeout_rem_if_exist` checks whether tlink is
self-referencing (`t->next == t`) to determine if it is in the queue, and
calls `sched_timeout_rem` if so (O(1)). The latter is used in places like
`sys_wup_tsk` and `sys_psnd_dtq` where it is unknown whether the task is in
the timeout queue.

## Timeout Wait Operation Example: trcv_dtq

When `trcv_dtq(dtqid, &data, 20)` is called:

### Normal Wakeup Path (Data Arrives)

1. `sys_trcv_dtq` checks the DTQ ring buffer -> empty
2. Insert the task's wlink into the DTQ receive wait queue (`ins_fifo`)
3. Call `sys_tslp_tsk(apic, 20)`:
   - Set tlink.delta = 20
   - Remove plink from the scheduler queue
   - tskstat = TTS_WAI
   - Insert into the timeout queue via `sched_timeout_ins`
   - Reschedule via `sched_next_tsk`
4. Another task calls `snd_dtq` -> data arrives:
   - `sys_psnd_dtq` / `ipsnd_dtq` removes the task's wlink from the DTQ receive wait queue
   - `wlink_rem` resets wlink to self-referencing
   - tskstat = TTS_RDY, insert plink into the scheduler queue
   - Set return value via `proc_set_return_value(proc, E_OK)`
5. `sched_timeout_rem_if_exist` removes tlink from the timeout queue
6. Because the task is now TTS_RDY, `sched_do_next_tsk` in `intr_leave`
   on the next timer interrupt selects this task. ESP is swapped to this
   task's kernel stack, `RESTORE_ALL` pops `pt_regs` (EAX=E_OK), and
   `iret` returns to Ring 3. `trcv_dtq` returns `E_OK`

### Timeout Wakeup Path

1-3. Same as the normal wakeup path
4. 20 ticks pass with no data arriving
5. PIT timer interrupt -> `c_intr_irq0` -> `timer_intr(0,1)` -> `sched_timeout(0,1)`:
   - The head entry's delta reaches 0
   - If wlink is not self-referencing, remove it from the DTQ receive wait queue via `wlink_rem`
   - Write `E_TMOUT` to `pt_regs.eax` on the kernel stack via `proc_set_return_value(proc, E_TMOUT)`
   - tskstat = TTS_RDY, insert plink into the scheduler queue
   - Set `next_tsk_flag` via `sched_next_tsk`
6. On the return path from the same interrupt: `intr_return` -> `intr_leave`:
   - k_nest reaches 0, `sched_next_tsk_check` -> `sched_do_next_tsk` selects
     the TTS_RDY task
   - ESP is swapped to this task's kernel stack
   - `RESTORE_ALL` pops `pt_regs` (EAX = E_TMOUT)
   - `iret` returns to Ring 3
   - The syscall wrapper returns EAX as the return value -> `trcv_dtq` returns `E_TMOUT`

## E_TMOUT Return Mechanism

How `E_TMOUT` is returned to a task woken by timeout:

1. While the task is blocked, its kernel stack holds the `pt_regs` saved by `SAVE_ALL`
2. `proc_set_return_value(proc, E_TMOUT)` writes `E_TMOUT (-50)` to `pt_regs.eax`
3. When the task resumes via `RESTORE_ALL` + `iret`, E_TMOUT is loaded into EAX
4. The syscall wrapper (`lib/lib_sem.c`) returns EAX as the return value

## Self-Referencing Convention (wlink / tlink)

wlink and tlink use the same self-referencing convention to determine in O(1)
whether they are in a queue.

| Field | Purpose | Meaning of Self-Reference |
|-------|---------|--------------------------|
| `T_TSK.wlink` | Object wait queue (sem/flg/dtq) | Not in any wait queue |
| `T_TSK.tlink` | Timeout queue | Not waiting for timeout |

- **Self-referencing** (`x.next == &x`): Not in any queue
- **Not self-referencing**: Connected to some queue

Both are initialized to self-referencing in `tsk_init`. They are also reset
to self-referencing upon removal (`wlink_rem`, `sched_timeout_rem`).

`sched_timeout` checks whether wlink is self-referencing and only calls
`wlink_rem` when it is connected. `sched_timeout_rem_if_exist` determines
whether tlink is in the queue by checking if it is self-referencing. This
prevents double removal when a timeout arrives after a normal wakeup.

## TMO Units

In the ITRON specification, TMO is in milliseconds, but this implementation
uses tick counts directly. Since 1 PIT tick is approximately 17ms (HZ=60),
TMO=20 corresponds to roughly 0.33 seconds.

Ideally a tick-to-millisecond conversion should be added in the future, but
for educational purposes, tick counts make it easier to understand the
behavior.

## Related Documents

- [Context Switch](context-switch.md) -- SAVE_ALL/RESTORE_ALL, pt_regs frame
- [Timer Interrupt](timer-interrupt.md) -- PIT/APIC timers and sched_timeout invocation
- [syscall](syscall.md) -- System call calling convention and return values
- [ITRON Guide](itron-guide.md) -- ITRON specification task state transitions
