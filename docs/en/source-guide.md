# Source Code Guide

This document describes the source code structure and the role of each file in tiny-itron.

---

## Directory Structure

```
tiny-itron/
├── i386/           ← Hardware-dependent code (x86)
│   ├── boot/       ← Boot sector (512 bytes)
│   ├── *.s         ← Assembly (boot, interrupts, helpers)
│   ├── *.c         ← C code (initialization, drivers, TSS)
│   └── Makefile
├── kernel/         ← ITRON kernel (hardware-independent)
│   ├── sys_*.c     ← Syscall implementations (tasks, semaphores, extensions, etc.)
│   ├── sys_exd.c   ← Non-ITRON extension syscalls
│   ├── sched.c     ← Scheduler
│   ├── pool.c      ← Memory allocator
│   ├── user.c      ← User tasks (demo)
│   └── Makefile
├── lib/            ← User-side library (syscall wrappers)
│   ├── lib_*.c     ← Thin wrappers that issue int $0x99
│   ├── lib_exd.c   ← Non-ITRON extension syscall wrappers
│   └── Makefile
├── include/        ← Shared headers
│   ├── itron.h     ← ITRON type definitions (ER, ID, VP_INT, etc.)
│   ├── types.h     ← Basic types (W, H, UW, etc.)
│   ├── config.h    ← Constants (MAX_TSKID, TMAX_TPRI, etc.)
│   ├── syscall.h   ← Syscall numbers (TFN_xxx)
│   └── exd.h       ← Non-ITRON extension API prototypes
├── docs/           ← Documentation
└── run.sh          ← QEMU launch script
```

---

## File List and Roles

### i386/ --- Hardware-Dependent Code

| File | Lines | Role |
|------|-------|------|
| `boot/boot.s` | ~455 | **Boot sector**: Loaded by BIOS, reads kernel from floppy |
| `start.s` | ~253 | **Second-stage boot**: A20 enable, GDT/IDT load, transition to protected mode |
| `run.s` | ~185 | **32-bit entry**: Kernel segment setup, CPU identification, call to main() |
| `genasm.c` | ~106 | **Build tool**: Generates GDT as table.s (runs on host) |
| `elf.c` | ~205 | **Build tool**: ELF to flat binary conversion (runs on host) |
| `main.c` | ~68 | **Kernel entry**: BSP/AP branching, starting point of initialization sequence |
| `386.c` | ~36 | **GDT/IDT operations**: set_gdt(), set_gate() |
| `interrupt.c` | ~452 | **Interrupt handlers**: IDT initialization, C handlers for IRQ 0-15 |
| `intr.s` | ~598 | **Interrupt entry**: SAVE_ALL/RESTORE_ALL, syscall entry (most critical file) |
| `proc.c` | ~210 | **Process management**: Task creation in proc_init(), fake frame construction in proc_create() |
| `tss.c` | ~63 | **TSS management**: esp0/ss0 initialization and dynamic update (loaded via ltr) |
| `smp.c` | ~174 | **SMP initialization**: APIC configuration, IPI transmission, AP startup |
| `video.c` | ~329 | **VGA driver**: printk(), vga_write_at() |
| `keyboard.c` | ~76 | **Keyboard driver**: Scan code to ASCII conversion |
| `timer.c` | ~52 | **Timer**: PIT initialization, timer_intr() |
| `i8259.c` | ~55 | **PIC driver**: i8259 master/slave initialization |
| `syscall.c` | ~61 | **Syscall dispatch**: c_intr_syscall() -> itron_syscall() |
| `page.c` | ~205 | **Paging**: Identity mapping, U/S access control |
| `klib.s` | ~535 | **Low-level helpers**: ccli/csti, cltr, start_first/second_task, syscall, etc. |
| `kernelval.c` | ~22 | **Shared variables**: current_proc[], c_tskid[] declarations |

### kernel/ --- ITRON Kernel

| File | Lines | Role |
|------|-------|------|
| `kernel.c` | ~60 | **Kernel initialization**: itron_init() --- initializes tsk, pool, sched, dtq, etc. |
| `syscall.c` | ~51 | **Syscall table**: itron_syscall() --- dispatches TFN_xxx to sys_xxx() |
| `sys_tsk.c` | ~549 | **Task management**: cre/act/slp/wup/ext_tsk, etc. |
| `sys_sem.c` | ~269 | **Semaphores**: cre/wai/pol/sig_sem |
| `sys_dtq.c` | ~332 | **Data queues**: cre/snd/rcv_dtq (ring buffer) |
| `sys_flg.c` | ~238 | **Event flags**: cre/set/wai_flg |
| `sys_mbx.c` | ~236 | **Mailboxes**: (unused) |
| `sys_mtx.c` | ~190 | **Mutexes**: (unused) |
| `sys_exd.c` | ~67 | **Extension syscalls**: Non-ITRON syscall handlers (VGA, keyboard, stack) |
| `sched.c` | ~216 | **Scheduler**: Priority queue, task switch decisions |
| `pool.c` | ~175 | **Memory management**: First-fit allocator (stack_pool, mem_pool, kmem_pool) |
| `user.c` | ~394 | **User tasks**: Demo code for Tasks 1-6 (using syscall-based API) |

### lib/ --- User Library

| File | Role |
|------|------|
| `lib_tsk.c` | Syscall wrappers for task management (cre_tsk, slp_tsk, etc.) |
| `lib_sem.c` | Syscall wrappers for semaphores (cre_sem, pol_sem, etc.) |
| `lib_tim.c` | Syscall wrappers for timers |
| `lib_int.c` | Syscall wrappers for interrupt-related operations |
| `lib_mbf.c` | Message buffer wrappers |
| `lib_exd.c` | Non-ITRON syscall wrappers (print_at, set_key_task, tsk_stack_alloc, etc.) |

### include/ --- Shared Headers

| File | Role |
|------|------|
| `itron.h` | ITRON standard types (ER, ID, VP_INT, FP, PRI, etc.) and API prototypes |
| `types.h` | Basic types (W, H, UW, UH, B, UB) |
| `config.h` | Constants (MAX_TSKID=16, TMAX_TPRI=16, MAX_SEMID=16, etc.) |
| `syscall.h` | Syscall numbers (TFN_CRE_TSK=-0x05, etc.) |
| `stdio.h` | printk prototype |
| `exd.h` | Non-ITRON extension API prototypes (print_at, set_key_task, etc.) |

---

## Important Data Structures

### proc_t (i386/proc.h) --- Process Control Block (HW-Dependent)

```
proc_t {
    unsigned long kern_esp;       <- Saved ESP for kernel stack
    unsigned long kern_stack_top; <- Top of kernel stack (set in TSS.esp0)
    unsigned long saved_eflags;   <- Saved EFLAGS for task exceptions
    int           cpu;            <- CPU affinity (0 or 1)
}
```

- Each task has a 4KB kernel stack (KERN_STACK_BASE + (tskid+1) * 4096)
- On interrupt, SAVE_ALL pushes 9 registers onto the kernel stack, forming the pt_regs frame
- A task switch is completed simply by swapping kern_esp in intr_leave

### T_TSK (kernel/types.h) --- Task Control Block (ITRON Layer)

```
T_TSK {
    ID      tskid;    <- Task ID (1-16)
    STAT    tskstat;  <- Task state (TTS_RUN, TTS_RDY, TTS_WAI, ...)
    PRI     tskpri;   <- Current priority (1=highest, 16=lowest)
    PRI     tskbpri;  <- Base priority
    UINT    wupcnt;   <- Wakeup request counter
    T_CTSK  ctsk;     <- Copy of creation parameters
    T_LINK  plink;    <- Priority queue link
    T_LINK  wlink;    <- Wait queue link
    proc_t* proc;     <- Pointer to HW-dependent part
    ...
}
```

### Shared Variables (i386/kernelval.c)

```c
proc_t* current_proc[2];  // per-CPU: currently running process
ID      c_tskid[2];       // per-CPU: current task ID
INT     next_tsk_flag[2];  // per-CPU: reschedule request flag
```

---

## Additional Notes

### The `W apic` Pattern

Many functions take `W apic` as their first argument. This is the CPU number (0 or 1), used as an index into per-CPU arrays:
- `current_proc[apic]` --- the current task on that CPU
- `c_tskid[apic]` --- the current task ID on that CPU
- `next_tsk_flag[apic]` --- reschedule request

### Identically Named Headers

`kernel/types.h` and `include/types.h` are **different files**.
The former defines kernel-internal structures (T_TSK, T_SEM, etc.), while the latter defines basic types (W, H, etc.).

### The `sys_` and `i` Prefixes

- `sys_slp_tsk()` --- syscall handler (task context)
- `iwup_tsk()` --- version called from interrupt handlers (interrupt context)

---

## Related Documents

- [System Overview](system-overview.md) --- Overall task behavior and interactions
- [ITRON Guide](itron-guide.md) --- ITRON API concepts and usage
- [Context Switch Details](context-switch.md) --- Detailed explanation of SAVE_ALL/RESTORE_ALL and intr_leave
- [i386 Architecture](i386-architecture.md) --- GDT, IDT, privilege levels
- [Memory Map](memory-map.md) --- Physical address layout
- [SMP Basics](smp-basics.md) --- How the 2-CPU configuration works
- [GDB Debugging](gdb-debugging.md) --- How to use GDB
