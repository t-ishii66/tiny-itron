# tiny-itron

A toy RTOS kernel for i386 with SMP (2 CPUs), inspired by the [Micro ITRON 4.0](https://www.ertl.jp/ITRON/SPEC/mitron4-e.html) specification.

## Why This Project Exists

Most people learn operating systems from textbooks.
This project exists so you can learn by **reading and running real kernel code**.

tiny-itron borrows the task/semaphore/event-flag API style from Micro ITRON 4.0,
but it does not aim to be a faithful or complete implementation of that specification.
Many ITRON syscalls are stubs, and the internal architecture departs freely from
the spec where simplicity wins. This is a toy kernel -- and that is the point.

Being small (~8,000 lines of C and assembly) means you can hold the entire system
in your head. There is no abstraction layer hiding the hardware. When you trace
a syscall from user space to the kernel and back, you see every instruction.
When you add a `printk` in the scheduler and reboot, the feedback loop is seconds.

The focus is on two things that textbooks explain in theory but rarely let you
**touch**:

- **The boot process** -- from the BIOS loading a 512-byte boot sector,
  through real-mode to protected-mode transition, GDT/IDT/TSS setup,
  paging, PIC and APIC initialization, all the way to the first user task
  running in Ring 3. Every step is in the source and documented.

- **Multitasking on bare metal** -- how `save`/`restore` in assembly
  captures and swaps 13 registers to switch between tasks, how the
  scheduler picks the next task, how two CPUs coordinate with spinlocks,
  and how a keyboard interrupt can preempt a running task. You can
  watch all of this happen in real time with GDB.

The goal is not to be production-ready. The goal is that every constant has a
reason, every register save has a comment explaining *why*, and you can follow
any path from user space to hardware and back with nothing but `grep` and `gdb`.

## What It Looks Like

When you run the kernel, QEMU shows a VGA text-mode display:

```
  TinyItron/386 SMP (2 CPU)                            [Ctrl+C to quit]
  ============================================================================

      Timer  tick = 12345

  [CPU0] Task1 #     142     dtq: a
  [CPU0] Task3 #      71     LOCK
           Shared (sem 1)    #      38
  [CPU1] Task2 #     284     BUSY
  [CPU1] Task4 > hello world
```

- **Task 1** and **Task 3** alternate on CPU 0 via `wup_tsk`/`slp_tsk`
- **Task 2** runs continuously on CPU 1
- **Task 3** (CPU 0) and **Task 2** (CPU 1) compete for a binary semaphore
  protecting a shared counter -- you can see LOCK/BUSY status in real time
- **Task 4** echoes keyboard input at the highest priority, preempting Task 2
- **Timer tick** is driven by the PIT (IRQ0) on CPU 0

## Building and Running

**Requirements:** GCC (i386 cross or native 32-bit), GNU Make, QEMU (`qemu-system-i386`).

```bash
# Build
make -C kernel && make -C lib && make -C i386

# Run in QEMU (2 CPUs)
./run.sh          # curses mode (VGA text in terminal, Ctrl+C to quit)
./run.sh -g       # GTK window mode (Ctrl+Alt+G to release grab)
./run.sh -G       # GDB mode (waits for connection on port 1234)
```

## Debugging with GDB

```bash
# Terminal 1
./run.sh -G

# Terminal 2
gdb i386/_kernel
(gdb) set architecture i386
(gdb) target remote :1234
(gdb) break first_task
(gdb) continue
(gdb) info threads            # shows both CPUs
(gdb) p task_count[1]         # Task 1 run count
(gdb) p shared_count          # semaphore-protected counter
```

See [docs/gdb-debugging.md](docs/ja/gdb-debugging.md) for detailed walkthrough.

## Architecture

### Hardware Stack

```
+---------------------+
|  Boot Sector (512B) |  Loads kernel from floppy via BIOS INT 13h
+---------------------+
|  GDT / IDT / TSS    |  Protected mode setup, segment descriptors
+---------------------+
|  PIC (i8259)         |  IRQ routing -- all external IRQs to CPU 0
+---------------------+
|  Local APIC          |  CPU identification, per-CPU timers, EOI
+---------------------+
|  Paging              |  Identity-mapped, User/Supervisor access control
+---------------------+
```

### Privilege Model

| Ring | CS     | DS     | SS     | Role         |
|------|--------|--------|--------|--------------|
| 0    | 0x20   | 0x28   | 0x30   | Kernel       |
| 3    | 0x5B   | 0x63   | 0x6B   | User tasks   |

### SMP Design

- **2 CPUs**: BSP (CPU 0) + AP (CPU 1), identified by Local APIC ID
- **AP boot**: INIT IPI + SIPI sequence, AP re-enters protected mode
- **Timers**: PIT (IRQ0, CPU 0 only) + Local APIC timer (both CPUs)
- **Spinlocks**: `xchgl`-based (works from Ring 3, no `cli`/`sti` needed)
- **CPU affinity**: each task is bound to a CPU, scheduler filters by affinity
- **No I/O APIC**: deliberate simplification; PIC handles all external IRQs

### Syscall Path

```
User task           Ring 3              Ring 0
---------           ------              ------
slp_tsk()
  -> syscall(0x11)
    -> int $0x99  ----[gate]---->  intr_syscall
                                    -> save (regs -> proc.reg[])
                                    -> c_intr_syscall
                                      -> itron_syscall
                                        -> syscall_entry[0x11] = sys_slp_tsk
                                    -> write return value to save frame
                                    -> restore (may switch tasks)
                                    -> iret  ----[gate]----> back to user
```

See [docs/syscall.md](docs/ja/syscall.md) for the complete trace.

### Context Switch

```
save:   reads APIC ID -> selects per-CPU current_proc[]
        advances proc->stack by 52 bytes (13 regs x 4)
        stores ECX,EDX,ESP,EIP,EBP,ESI,EDI,EFLAGS,EAX,EBX,DS,ES

restore: decrements proc->stack by 52
         if nest count == 0: calls sched_next_tsk_check()
           -> may change current_proc[] to a different task
         loads registers from (possibly different) task's save area
         overwrites iret frame with new task's EIP/ESP

iret:   atomically restores CS:EIP, SS:ESP, EFLAGS -> new task runs
```

See [docs/timer-interrupt.md](docs/ja/timer-interrupt.md) and [docs/context-switch.md](docs/ja/context-switch.md) for details.

## Source Layout

```
i386/           Architecture-dependent code
  boot/           Boot sector (boot.s) and loader tables
  start.s         Real mode -> protected mode transition
  main.c          Kernel entry point, initialization sequence
  intr.s          save/restore, all interrupt/exception/syscall stubs
  klib.s          syscall wrapper, TSS task launch, I/O port helpers
  proc.c          Task proc_t management, CPU affinity
  interrupt.c     IDT setup, PIC init, sched_next_tsk_check
  page.c          Page directory/table setup (identity-mapped)
  smp.c           AP boot (INIT/SIPI), Local APIC timer
  syscall.c       c_intr_syscall (return value writeback)
  video.c         VGA text-mode driver (0xB8000)
  keyboard.c      PS/2 keyboard driver, ring buffer, IRQ1
  timer.c         PIT (8254) initialization
  tss.c           TSS descriptor setup

kernel/         Architecture-independent ITRON kernel
  syscall.c       itron_syscall dispatcher
  syscallP.h      syscall_entry[] dispatch table (234 entries)
  sys_tsk.c       Task management (cre/act/slp/wup/ter_tsk, ...)
  sys_sem.c       Semaphores (cre/sig/wai/pol_sem)
  sys_flg.c       Event flags
  sys_dtq.c       Data queues
  sys_mbx.c       Mailboxes
  sys_exd.c       Extended syscalls (VGA, keyboard, stack alloc)
  sched.c         Priority-based scheduler, ready queues
  pool.c          Memory pool (stack allocation)
  user.c          Demo user tasks (first_task, second_task, kbd_task)

lib/            User-space library (linked into .user_text)
  lib_tsk.c       Task management wrappers (cre_tsk, slp_tsk, ...)
  lib_sem.c       Semaphore/flag/DTQ/mailbox wrappers
  lib_exd.c       Extended syscall wrappers (print_at, key_read, ...)

include/        Shared headers
  itron.h         ITRON types, error codes, TFN_* function codes
  config.h        Kernel limits (MAX_TSKID=16, TMAX_TPRI=16, ...)

docs/           Documentation (Japanese)
  system-overview.md    Architecture overview
  syscall.md            Syscall processing flow (user -> kernel -> return)
  timer-interrupt.md    Timer interrupt and save/restore detail
  context-switch.md     Context switch mechanics
  memory-map.md         Physical memory layout
  build-system.md       Build process and linker script
  boot-sector.md        Boot sector and floppy loading
  smp-basics.md         SMP boot and APIC configuration
  i386-architecture.md  GDT, IDT, TSS, paging
  itron-guide.md        ITRON API introduction
  gdb-debugging.md      GDB debugging guide
  vga-text-mode.md      VGA text mode programming
  source-guide.md       Source file reference
  refs/                 Per-file reference documentation
```

## ITRON Syscall Support

| Category          | Implemented                                           | Status        |
|-------------------|-------------------------------------------------------|---------------|
| Task management   | cre_tsk, act_tsk, slp_tsk, wup_tsk, ter_tsk, chg_pri | Working       |
| Task management   | tslp_tsk, ext_tsk, exd_tsk, sus_tsk                  | Implemented   |
| Semaphore         | cre_sem, sig_sem, wai_sem, pol_sem                    | Working       |
| Event flag        | cre_flg, set_flg, wai_flg, pol_flg                   | Implemented   |
| Data queue        | cre_dtq, snd_dtq, psnd_dtq, prcv_dtq                 | Working       |
| Mailbox           | cre_mbx, snd_mbx, rcv_mbx                            | Implemented   |
| Extended (custom) | print_at, key_read, clear_screen, tsk_stack_alloc         | Working       |

"Working" = verified in the running demo. "Implemented" = code exists, not yet stress-tested.

## History

Originally written in 2000 by [t-ishii66](https://github.com/t-ishii66) as
"SMP MicroITRON ver 4.0.0" -- a hobby implementation of the Micro ITRON 4.0
specification for IBM PC/AT compatibles with i386 CPUs.

In 2026, the project was revived as an educational platform:
extensive documentation was added, critical bugs in interrupt handling and
SMP context switching were fixed, and a visual multitask demo was built
to make the kernel's behavior observable in real time.

## License

Free software. See source files for copyright notices.

## Credits

- Programming: t-ishii66
- Document: Claude Opus 4.6
- Review: t-ishii66
- Extension for Debugging: Claude Opus 4.6

Copyright(C) 2026 t-ishii66. All rights reserved.

## References

- [Micro ITRON 4.0 Specification](https://www.ertl.jp/ITRON/SPEC/mitron4-e.html) (English)
- [ITRON Project](https://www.ertl.jp/ITRON/) -- designed by Prof. Ken Sakamura, University of Tokyo
- [Intel i386 Programmer's Reference Manual](https://css.csail.mit.edu/6.858/2014/readings/i386.pdf)
- [OSDev Wiki](https://wiki.osdev.org/) -- invaluable for bare-metal x86 programming
