# Keyboard Input

This document traces how a keyboard key press reaches a user task,
following the path end-to-end from hardware (i8259 + 8042) to user task (kbd_task).

## 1. Overall Flow

```
  Key press
    |
    v
  8042 Controller (I/O 0x60, 0x64)
    |  IRQ1 signal
    v
  i8259 PIC (master)              ... Delivered to CPU 0 only
    |  Interrupt vector 0x81
    v
  CPU 0: Ring 3 -> Ring 0         ... Privilege transition, switch to kernel stack
    |
    v
  intr_irq1 (intr.s)             ... SAVE_ALL -> intr_enter -> call c_intr_irq1
    |
    v
  c_intr_irq1 (interrupt.c)      ... key_intr() + i8259_reenable()
    |
    v
  key_intr (keyboard.c)          ... Read scan code -> ASCII conversion
    |                                 -> Send to DTQ 2 via ipsnd_dtq(0, key_dtq_id, ch)
    v
  ipsnd_dtq (sys_dtq.c)          ... Store character in DTQ 2, wake Task 4 waiting on rcv_dtq
    |                                 Task 4: TTS_WAI -> TTS_RDY, sched_next_tsk(0) sets
    |                                 next_tsk_flag on both CPUs
    v
  intr_return (intr.s)            ... intr_leave -> RESTORE_ALL -> iret (CPU 0 returns to original task)
    :
    : (Next APIC timer interrupt on CPU 1)
    v
  CPU 1: intr_leave               ... next_tsk_flag[1] != 0 -> sched_do_next_tsk
    |                                 -> Save Task 2's ESP, load Task 4's ESP
    v
  kbd_task (user.c)               ... rcv_dtq(2) returns the character -> accumulate in local buffer
    |                                 -> On Enter, psnd_mbf(1, buf, len) -> send to Task 1 via MBF 1
    v
  Character is displayed on screen
```

## 2. Hardware: 8042 Keyboard Controller

Keyboard input on a PC is managed by an Intel 8042-compatible controller.

| I/O Port | Purpose |
|---|---|
| `0x60` (IO_KEY) | Data register: read scan code |
| `0x64` (IO_KEY_CS) | Command/status register |

When a key is pressed, the 8042 places the scan code (make code) in port 0x60
and asserts IRQ1. When the key is released, a release code (make code | 0x80) is sent.

## 3. IRQ1 Delivery Path

IRQ1 is connected to IR1 on the i8259 PIC master. Since tiny-itron retains the PIC
and does not use the I/O APIC, **all external IRQs are delivered only to CPU 0 (BSP)**.

This is a constraint imposed by the physical wiring. The i8259 PIC output (INTR signal) is
directly connected to the LINT0 pin of the BSP (CPU 0) Local APIC and is not connected
to the AP (CPU 1). While the I/O APIC could route IRQs to any CPU,
tiny-itron avoids using the I/O APIC to reduce complexity.

The vector number for IRQ1 is set to **0x81** (= 0x80 + 1) through PIC remapping
(ICW2 = 0x80 in i8259_init).

### IRQ Mask

At startup, all IRQs are masked. `key_start()` calls `irq_mask_off(2)`
(bitmask 0x02 = IR1) to unmask IRQ1.

```c
/* keyboard.c */
void key_start(void) {
    irq_mask_off(2);    /* bit 1 = IRQ1 */
}
```

## 4. Interrupt Handler (Ring 0)

When IRQ1 arrives at CPU 0, IDT vector 0x81 dispatches to `intr_irq1`.
It is handled using the same standard pattern as other IRQs:

```asm
# intr.s
intr_irq1:
    SAVE_ALL                # Push 9 registers onto per-task kernel stack
    call  intr_enter        # k_nest[0]++
    call  c_intr_irq1       # Call C handler
    jmp   intr_return       # intr_leave -> RESTORE_ALL -> iret
```

The C handler `c_intr_irq1` (interrupt.c) calls `key_intr()` and sends EOI to the PIC:

```c
/* interrupt.c */
void c_intr_irq1(void) {
    smp_lock(&kernel_lk);
    key_intr();
    i8259_reenable();
    smp_unlock(&kernel_lk);
}
```

### Why We Do Not Use sti

Calling `sti` inside the keyboard handler would allow nested interrupts (such as the
APIC timer) to fire during processing. On Ring 0-to-Ring 0 interrupts, the CPU does not
push SS/ESP, so the 14-word pt_regs frame that SAVE_ALL expects cannot be formed correctly,
leading to register corruption.
Therefore, interrupts remain disabled throughout the handler (the IDT gate type is
Interrupt Gate, so the CPU automatically clears IF=0).

## 5. Scan Code to ASCII Conversion

`key_intr()` (keyboard.c) handles all keyboard processing.

### 5.1 Reading the Scan Code

```c
c = inb(IO_KEY);           /* Read scan code from 0x60 */
outb(IO_KEY_CS, 0xad);     /* Disable keyboard (prevent re-entry during processing) */
outb(IO_KEY_CS, 0xae);     /* Re-enable keyboard */
```

### 5.2 Tracking Modifier Keys

The `mode` variable (static int) tracks the state of Shift/Ctrl:

| Scan Code | Event |
|---|---|
| 0x2a, 0x36 (make) | `mode \|= SHIFT` |
| 0xaa, 0xb6 (release) | `mode &= ~SHIFT` |
| 0x1d (make) | `mode \|= CTRL` |
| 0x9d (release) | `mode &= ~CTRL` |

Modifier keys themselves are not sent to the DTQ; the function returns with `return 0`.

### 5.3 Ctrl+C Halt

Pressing `c` (scan code 0x2e) while holding Ctrl resets the CPU:

```c
if ((mode & CTRL) && c == 0x2e) {
    __asm__("cli");
    vga_write_at(12, 28, "  System halted.  ", 0x4F);
    outb(IO_KEY_CS, 0xFE);     /* Pulse CPU reset line via 8042 */
    while (1) { __asm__("hlt"); }
}
```

If QEMU is started with `-no-reboot`, this results in a clean shutdown.

### 5.4 ASCII Conversion Tables

Two scan code-to-ASCII tables for a US ASCII keyboard layout are defined in `keyboardP.h`:

- `scode[]` -- normal state
- `scode_sh[]` -- Shift held

```c
ch = (mode & SHIFT) ? scode_sh[c] : scode[c];
```

## 6. Sending from ISR to DTQ (ipsnd_dtq)

After ASCII conversion, `key_intr()` sends the character to data queue DTQ 2 via `ipsnd_dtq()`:

```c
/* keyboard.c */
if (key_dtq_id > 0)
    ipsnd_dtq(0, key_dtq_id, ch);
```

`key_dtq_id` is a global variable. kbd_task registers DTQ ID = 2 at startup by calling
`set_key_task(2)`. Before kbd_task starts, `key_dtq_id = 0`, so key presses
do not trigger any DTQ transmission.

`ipsnd_dtq()` (sys_dtq.c) is the ISR version of `fsnd_dtq` and performs the following
(caller holds `kernel_lk` -- already acquired in `c_intr_irq1`):

1. Store the character data in DTQ 2
2. If Task 4 is waiting on `rcv_dtq(2)`, wake it up (TTS_WAI -> TTS_RDY)
3. Insert into the scheduler queue
4. Call `sched_next_tsk(0)` to set `next_tsk_flag[]` on **both CPUs**

### Why "Both CPUs"

IRQ1 is processed on CPU 0, but kbd_task runs on CPU 1.
`sched_next_tsk()` sets `next_tsk_flag` on all CPUs, so a task switch occurs
in `intr_leave` during the next APIC timer interrupt on CPU 1,
waking Task 4 on CPU 1.

```
  CPU 0 (processing IRQ1)          CPU 1 (running Task 2)
  ─────────────────                ─────────────────
  key_intr()                       ...busy loop...
    ipsnd_dtq(0, 2, ch)
      Store in DTQ 2
      Set Task 4 to TTS_RDY
      sched_next_tsk(0)
        next_tsk_flag[0] = 1
        next_tsk_flag[1] = 1       (checked on next APIC timer)
  intr_leave:
    Check next_tsk_flag[0]           APIC timer interrupt
    -> Task 4 is on CPU 1, so        intr_leave:
       no switch on CPU 0               next_tsk_flag[1] != 0
                                         -> sched_do_next_tsk(1)
                                         -> Task 4 (priority 1) > Task 2 (priority 15)
                                         -> Switch to Task 4
```

## 7. Two-Stage Design: DTQ + MBF

Keyboard input uses two communication channels: DTQ 2 (per-character) and MBF 1 (per-line):

```
  key_intr (ISR)    DTQ 2 (16 entries)  kbd_task (Task 4)    MBF 1 (256B)     first_task (Task 1)
  ──────────────    ──────────────      ─────────────────    ─────────────    ──────────────────
  ipsnd_dtq(0,2,ch) -->  [ch]  -->  rcv_dtq(2, &data)
                                    Accumulate in line_buf
                                    Enter or line-end wrap
                                    psnd_mbf(1, buf, len) -->  [line] -->  trcv_mbf(1, buf, 20)
```

- **DTQ 2** (ISR -> Task 4): Created by `second_task` via `cre_dtq(2, ...)` (16 entries).
  The ISR uses `ipsnd_dtq` (non-blocking); Task 4 uses `rcv_dtq` (blocking).
- **MBF 1** (Task 4 -> Task 1): Created by `first_task` via `cre_mbf(1, ...)`
  (maxmsz=64, mbfsz=256). Task 4 sends line strings with `psnd_mbf` (non-blocking);
  Task 1 receives with `trcv_mbf` (blocking with timeout).

## 8. User Task API

kbd_task runs in user space (Ring 3).

### 8.1 set_key_task(dtq_id) -- DTQ ID Registration

```
kbd_task (Ring 3)
  -> set_key_task(2)                    lib/lib_exd.c: syscall(-TFN_EXD_KEY_SETTASK, 2)
    -> int $0x99                        klib.s -> intr_syscall
      -> sys_key_set_task(apic, 2)      kernel/sys_exd.c: key_dtq_id = 2
```

The ISR's `key_intr()` references this global variable `key_dtq_id` to determine
which DTQ to send to. Before kbd_task starts, `key_dtq_id = 0`, so key presses
do not trigger any DTQ transmission.

### 8.2 kbd_task Main Loop

```c
/* user.c: kbd_task (Task 4, CPU 1, priority 1) */
set_key_task(2);               /* Notify ISR of DTQ ID */

int  line_pos = 0;
char line_buf[64];             /* Buffer to accumulate until Enter */

while (1) {
    rcv_dtq(2, &data);         /* Blocking receive from DTQ 2 (TTS_WAI until key arrives) */
    c = (int)data;

    if (c >= ' ' && c < 0x7f && line_pos < 63) {
        /* Printable character: echo to screen + accumulate in buffer */
        print_at(ROW_KBD, kbd_col, s, ATTR_YELLOW);
        line_buf[line_pos++] = (char)c;
        kbd_col++;
        if (kbd_col >= kbd_max) {
            /* Line-end wrap: send via MBF + clear */
            psnd_mbf(1, line_buf, line_pos);
            fill_at(ROW_KBD, 20, kbd_max - 20, ' ', 0x0E);
            kbd_col = 20;  line_pos = 0;
        }
    } else if (c == '\r') {
        /* Enter: send line string to MBF 1 */
        if (line_pos > 0)
            psnd_mbf(1, line_buf, line_pos);
        fill_at(ROW_KBD, 20, kbd_max - 20, ' ', 0x0E);
        kbd_col = 20;  line_pos = 0;
    } else if (c == '\b') {
        /* Backspace: move buffer and cursor back by one */
        if (kbd_col > 20 && line_pos > 0) {
            kbd_col--;  line_pos--;
            fill_at(ROW_KBD, kbd_col, 1, ' ', 0x0E);
        }
    }
}
```

kbd_task receives one character at a time from DTQ 2 and accumulates it in a local buffer
(`line_buf`). When the Enter key (`'\r'`) is pressed or the line end (column 76) is reached,
it sends the line string to MBF 1 via `psnd_mbf(1, line_buf, line_pos)` and clears the
echo line on screen. Backspace (`'\b'`) is also supported.

Task 4 has priority 1 (highest), so when it wakes up, it immediately preempts Task 2
(priority 15) on CPU 1. After processing the character, it blocks again on `rcv_dtq(2)`,
returning the CPU to Task 2.

## 9. Line-Level Inter-Task Transfer via MBF

kbd_task sends accumulated line strings to first_task through message buffer MBF 1:

```
  kbd_task (CPU 1)                  first_task (CPU 0)
  ─────────────────                 ─────────────────
  psnd_mbf(1, line_buf, line_pos)   trcv_mbf(1, mbf_buf, 20)
  (non-blocking send, per-line)      (receive with timeout, 20 ticks ~ 0.33s)
                                     Return value = message size (> 0) or E_TMOUT
```

first_task (Task 1) calls `trcv_mbf()` on each iteration of its main loop.
If a line string arrives, it receives the message size (a positive value); if not,
it returns with a timeout (E_TMOUT) after 20 ticks. The received string is displayed
on Row 5 of the screen (truncated to 44 characters). This allows keyboard input to be
confirmed on the CPU 0 side as well.

MBF (message buffer) is an ITRON object that copies variable-length messages through
a kernel-internal ring buffer. Unlike DTQ (which transfers a single integer), MBF can
send and receive an entire string at once.

## 10. Source File List

| File | Role |
|---|---|
| `i386/keyboard.c` | key_init, key_start, key_intr (kernel side) |
| `i386/keyboardP.h` | Scan code-to-ASCII tables, modifier key constants |
| `i386/keyboard.h` | Public header for keyboard.c |
| `i386/interrupt.c` | c_intr_irq1 (C handler for IRQ1) |
| `i386/intr.s` | intr_irq1 (assembly entry point for IRQ1) |
| `kernel/sys_dtq.c` | ipsnd_dtq, rcv_dtq (DTQ send/receive) |
| `kernel/sys_mbf.c` | psnd_mbf, trcv_mbf (MBF send/receive) |
| `kernel/sys_exd.c` | sys_key_set_task (DTQ ID registration syscall handler) |
| `lib/lib_exd.c` | set_key_task (user space wrapper) |
| `kernel/user.c` | kbd_task (Task 4 implementation) |

## Related Documents

- [context-switch.md](context-switch.md) -- Details of SAVE_ALL/RESTORE_ALL and intr_leave
- [syscall.md](syscall.md) -- Full syscall flow
- [timer-interrupt.md](timer-interrupt.md) -- APIC timer and task switching via intr_leave
- [smp-basics.md](smp-basics.md) -- Cross-CPU next_tsk_flag propagation
- [itron-guide.md](itron-guide.md) -- ITRON API guide for DTQ, slp_tsk/wup_tsk
