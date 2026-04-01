# tiny-itron RTOS GDB Debugging Guide

A guide on how to debug the Micro ITRON v4.0.0 (i386) kernel using QEMU + GDB.

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Building with Debug Symbols](#2-building-with-debug-symbols)
3. [Starting QEMU in GDB Mode](#3-starting-qemu-in-gdb-mode)
4. [Connecting GDB](#4-connecting-gdb)
5. [Commonly Used Breakpoints](#5-commonly-used-breakpoints)
6. [Inspecting Task State](#6-inspecting-task-state)
7. [Distinguishing Kernel/User Mode](#7-distinguishing-kerneluser-mode)
8. [Commonly Used GDB Commands](#8-commonly-used-gdb-commands)
9. [Troubleshooting](#9-troubleshooting)

---

## 1. Prerequisites

### Required Tools

#### QEMU

You need the i386 system emulator.

```bash
# Ubuntu / Debian
sudo apt install qemu-system-x86

# Arch Linux
sudo pacman -S qemu-system-x86

# Fedora
sudo dnf install qemu-system-x86
```

#### GDB

You need a GDB that supports the 32-bit i386 target. On an x86_64 host, the standard `gdb` usually works, but in cross-compilation environments, use `gdb-multiarch` or `i386-elf-gdb`.

```bash
# Ubuntu / Debian (recommended: gdb-multiarch)
sudo apt install gdb-multiarch

# macOS (install i386-elf-gdb via Homebrew)
brew install i386-elf-gdb

# Arch Linux
sudo pacman -S gdb
```

This guide uses the `gdb` command, but substitute `gdb-multiarch` or `i386-elf-gdb` as appropriate for your environment.

---

## 2. Building with Debug Symbols

### _kernel and _kernel_dbg

The build flow in `i386/Makefile` is as follows:

```makefile
kernel	: elf $(OBJS)
	$(LD) $(LDFLAGS) -e run -o _kernel $(OBJS)
	cp _kernel _kernel_dbg       # Save a copy with symbols
	strip _kernel                # Strip symbols for the floppy image
	cp	/dev/null kernel
	./elf _kernel kernel         # ELF -> flat binary conversion
```

- `_kernel_dbg` -- ELF with symbols. **Use this one with GDB**
- `_kernel` -- Stripped ELF. Converted to a flat binary by the `elf` tool
- `kernel` -- Flat binary. Combined into the floppy image

A normal build (`make -C i386`) automatically generates `_kernel_dbg`, so there is no need to disable stripping.

**How it works:** What QEMU actually executes is the flat binary on the floppy image (stripped, no symbol information). GDB connects to QEMU's GDB server over TCP and can read/write CPU registers and memory, but without symbols it can only show raw addresses. `_kernel_dbg` is loaded into GDB to provide symbol information such as "address 0xXXXX = function main". Since `_kernel_dbg` and the running binary share the same link addresses, GDB can resolve symbols correctly. The floppy image contents are not affected at all.

### Enabling Source-Level Debugging

By default, function names and global variable symbols are available, but source line number correspondence (step execution, `list` command, etc.) requires DWARF debug information. Configure the following as needed.

#### (a) Add -g to CFLAGS

Add `-g` to `CFLAGS` in each Makefile:

**`i386/Makefile`:**
```makefile
CFLAGS = -Wall -m32 -fno-pie -fno-stack-protector -fno-builtin -g
```

**`kernel/Makefile`:**
```makefile
CFLAGS = -Wall -m32 -fno-pie -fno-stack-protector -fno-builtin -g
```

**`lib/Makefile`:**
```makefile
CFLAGS = -m32 -fno-pie -fno-stack-protector -fno-builtin -g
```

#### (b) Debug Information for Assembly Files

If you also need debug information for assembly files (`.s`), add `--gstabs` to the assembly rule in `i386/Makefile`:

```makefile
%.o	: %.s
	$(AS) --32 --gstabs -o $@ $<
```

#### (c) Clean Build

Regenerate all object files to apply the changes:

```bash
make -C kernel clean && make -C kernel
make -C lib clean && make -C lib
make -C i386 clean && make -C i386
```

> **Note:** Adding `-g` only increases the size of `_kernel_dbg`; it does not affect the floppy image (`i386/i386`). The `elf` tool extracts only LOAD segments and does not include DWARF sections.

---

## 3. Starting QEMU in GDB Mode

### The -G Option in run.sh

`run.sh` provides a `-G` option for GDB debugging.

```bash
./run.sh -G
```

This command adds the following options to QEMU:

| QEMU Option | Meaning |
|:--|:--|
| `-s` | Start a GDB server on TCP port 1234 |
| `-S` | Halt the CPU at startup (waits for `continue` from GDB) |

On startup, the following message is displayed:

```
GDB mode: waiting for connection on port 1234...
Starting QEMU...
```

QEMU is waiting with the CPU halted. The kernel will not execute until GDB is connected from another terminal.

### Combining with Other Options

```bash
# GTK window mode + GDB
./run.sh -g -G

# Debug log output + GDB
./run.sh -d -G
# -> Interrupt logs are recorded in qemu.log
```

---

## 4. Connecting GDB

### Basic Connection Procedure

After starting QEMU in GDB mode in terminal 1, launch GDB in terminal 2.

```bash
# Terminal 2
gdb i386/_kernel_dbg
```

Execute the following commands at the GDB prompt:

```gdb
# Set architecture to i386 (for gdb-multiarch)
set architecture i386

# Add source search paths
directory i386 kernel lib

# Load the symbol file (if needed)
# Not necessary if i386/_kernel_dbg was specified when starting GDB
symbol-file i386/_kernel_dbg

# Connect to QEMU's GDB server
target remote :1234
```

### Automation with .gdbinit

Creating a `.gdbinit` file in the project root saves you from typing these commands every time.

```gdb
# .gdbinit
set architecture i386

# Source search paths (when running from the project root)
# Needed because the paths recorded by -g/--gstabs are relative to each directory
directory i386 kernel lib

symbol-file i386/_kernel_dbg
target remote :1234

# The kernel load address is 0x3400
# If symbols are misaligned, use the following
# add-symbol-file i386/_kernel_dbg 0x3400

# --- Display settings (uncomment as needed) ---
# Display structs with one field per line, indented
# set print pretty on
# Display arrays with one element per line
# set print array on
# Set element display limit to unlimited (default is 200)
# set print elements 0

# Set a breakpoint at main right after kernel startup
break main
continue
```

> **Note:** If GDB does not automatically load `.gdbinit`, specify it explicitly with `gdb -x .gdbinit`, or add `set auto-load safe-path /` to `~/.config/gdb/gdbinit`.

### Verifying the Connection

On successful connection, GDB will stop at the position where QEMU is halted (near the real-mode reset vector `0xfff0`, or at the beginning of the bootloader).

```gdb
(gdb) info registers
# -> If eip is around 0x0000fff0, the connection is normal
```

To advance execution to the kernel's `main` function:

```gdb
(gdb) break main
(gdb) continue
```

---

## 5. Commonly Used Breakpoints

### Kernel Boot Sequence

```gdb
# Kernel entry point (after transition to 32-bit mode)
break run

# Kernel main function
# -> Calls all_init(), page_init(), page_enable(), itron_init(),
#    proc_init(), tss_init(), then start_first_task()
break main

# Hardware initialization
break all_init

# First task startup (ltr + RESTORE_ALL + iret)
break start_first_task
```

### Task Execution

```gdb
# First user task (task ID 1)
# -> Creates semaphore 1 and creates/activates task ID 3 (usr_main)
break first_task

# Main user task (task ID 3)
break usr_main

# When semaphore acquisition succeeds in the main user task
break usr_main
# (e.g., set a break at the pol_sem success path inside usr_main)
```

### Interrupts and Context Switching

```gdb
# Timer interrupt handler (IRQ0 -> INT 0x80)
break c_intr_irq0

# Interrupt entry / task switch (assembly)
break intr_enter
break intr_leave

# Scheduler
break sched_do_next_tsk
```

### Conditional Breakpoints

```gdb
# Stop only when scheduling task ID 4
break sched_do_next_tsk if apic == 0

# Stop only on the 10th timer interrupt
break c_intr_irq0
ignore 1 9

# Stop when a specific character is written to VGA output
break video_putc
```

---

## 6. Inspecting Task State

### Checking Registers

```gdb
# Display all registers
info registers

# Individual registers
print/x $eip
print/x $esp
print/x $eflags
print/x $cs
print/x $ds
print/x $ss
```

### Checking the Stack

```gdb
# Display the top 20 words (32-bit) of the current stack in hex
x/20x $esp

# Backtrace (with symbol-enabled build)
backtrace
bt full
```

### Examining the proc Structure

Tasks in tiny-itron are managed by `proc_t` structures.

```c
typedef struct proc {
    unsigned long kern_esp;       /* Current position of kernel stack */
    unsigned long kern_stack_top; /* Top of kernel stack (value set in TSS.esp0) */
    unsigned long saved_eflags;   /* For proc_eflags_save/restore */
    int           cpu;            /* CPU affinity (0 or 1) */
} proc_t;
```

Each task has a 4KB kernel stack. When an interrupt occurs, SAVE_ALL pushes registers to build the pt_regs frame. `kern_esp` points to the top of this frame.

pt_regs frame (on the kernel stack):

| Offset | Register | Pushed By |
|:--|:--|:--|
| 0x00 | ES | SAVE_ALL |
| 0x04 | DS | SAVE_ALL |
| 0x08 | EDI | SAVE_ALL |
| 0x0C | ESI | SAVE_ALL |
| 0x10 | EBP | SAVE_ALL |
| 0x14 | EBX | SAVE_ALL |
| 0x18 | EDX | SAVE_ALL |
| 0x1C | ECX | SAVE_ALL |
| 0x20 | EAX | SAVE_ALL |
| 0x24 | EIP | CPU |
| 0x28 | CS | CPU |
| 0x2C | EFLAGS | CPU |
| 0x30 | ESP (Ring 3) | CPU |
| 0x34 | SS (Ring 3) | CPU |

GDB commands to examine the proc structure:

```gdb
# Current process pointer
print current_proc[0]

# Display contents of the proc structure
print *current_proc[0]

# Current position of the kernel stack
print/x current_proc[0]->kern_esp

# Top of kernel stack (the value set in TSS.esp0)
print/x current_proc[0]->kern_stack_top

# Read register values from the pt_regs frame (kern_esp points to pt_regs)
# EAX (return value): kern_esp + 0x20
x/x current_proc[0]->kern_esp + 0x20

# EIP (point of interruption): kern_esp + 0x24
x/x current_proc[0]->kern_esp + 0x24

# ESP (Ring 3 stack): kern_esp + 0x30
x/x current_proc[0]->kern_esp + 0x30

# Display pt_regs as a structure
print *(struct pt_regs *)current_proc[0]->kern_esp
```

### Scheduler State

```gdb
# CPU lock state
print cpu_stat

# Dispatch enable/disable state
print dispatch_stat

# Next task flag
print next_tsk_flag[0]

# Interrupt nesting counter (defined in assembly, see note below)
print *(int*)&k_nest0
print *(int*)&k_nest1
```

> **Note:** Variables defined with `.long` in intr.s, such as `k_nest0`/`k_nest1`, have no DWARF type information. If you type `print k_nest0` in GDB, you get "has unknown type"; if you try `print (int)k_nest0`, the address value itself is displayed. To read the value, use `print *(int*)&k_nest0` or `x/1dw &k_nest0`.

---

## 7. Distinguishing Kernel/User Mode

### Identification via the CS Register

The lower 2 bits of the i386 segment selector indicate the CPL (Current Privilege Level).

```gdb
# Check the current CS selector
print/x $cs
```

| CS Value | Mode | Description |
|:--|:--|:--|
| `0x20` | Ring 0 (kernel mode) | Kernel code segment |
| `0x5b` | Ring 3 (user mode) | User code segment |

Other segment selectors:

| Segment | Ring 0 (Kernel) | Ring 3 (User) |
|:--|:--|:--|
| CS | `0x20` | `0x5b` |
| DS | `0x28` | `0x63` |
| SS | `0x30` | `0x6b` |

```gdb
# Also check the data and stack segments
print/x $ds
print/x $ss

# List all segment registers
info registers cs ds ss es fs gs
```

### Tracing System Calls

tiny-itron uses `INT 0x99` for system calls. You can trace the transition from user mode to kernel mode with step execution.

```gdb
# Set a breakpoint at the system call entry point
break intr_syscall

# Or at the syscall wrapper function
break syscall

# After stopping, trace instruction by instruction with stepi
stepi
# -> When INT 0x99 is executed, CS changes from 0x5b to 0x20
print/x $cs
```

### Tracing Timer Interrupts (INT 0x80)

Timer interrupts (IRQ0) are delivered to the kernel as INT 0x80.

```gdb
# Timer interrupt entry
break intr_irq0

# After stopping
print/x $cs
# -> Executing in 0x20 (kernel mode)

# Pre-interrupt context (if interrupted from user mode)
# Check the CS saved on the stack
x/5x $esp
# -> Saved in the order [EIP, CS, EFLAGS, ESP, SS]
# -> If CS is 0x5b, the interrupt came from user mode
```

---

## 8. Commonly Used GDB Commands

### Execution Control

| Command | Shortcut | Description |
|:--|:--|:--|
| `continue` | `c` | Resume execution |
| `stepi` | `si` | Step one instruction (assembly level) |
| `nexti` | `ni` | Step one instruction (step over calls) |
| `step` | `s` | Step one line (source level) |
| `next` | `n` | Step one line (step over function calls) |
| `finish` | `fin` | Execute until the current function returns |
| `until *addr` | | Execute until the specified address |

### Breakpoints

```gdb
# Set by function name
break main
break video_putc

# Set by address
break *0x3400

# Conditional
break c_intr_irq0 if k_nest0 == 0

# Hardware watchpoint (monitor memory writes)
watch current_proc[0]
watch *(int*)0x3400

# List all breakpoints
info breakpoints

# Delete
delete 1
delete    # Delete all

# Temporarily disable/enable
disable 1
enable 1
```

### Examining Memory and Registers

```gdb
# Display all registers
info registers

# Display memory contents
# x/[count][format][size] [address]
#   format: x(hex), d(decimal), s(string), i(disassemble)
#   size: b(1 byte), h(2 bytes), w(4 bytes), g(8 bytes)

x/20x $esp          # Top 20 words of the stack (hex)
x/10i $eip          # Disassemble 10 instructions from the current EIP
x/s 0x3500          # Display as a string
x/32b 0x3400        # Display 32 bytes, one byte at a time

# Display variables
print variable_name
print/x variable_name    # Hexadecimal
print *pointer           # Dereference a pointer

# Disassemble
disassemble main
disassemble intr_leave
disassemble $eip, $eip+50    # Specify a range
```

### Display Layout

```gdb
# TUI display with assembly + registers
layout asm
layout regs

# Source code display (with symbol-enabled build)
layout src

# Toggle TUI
tui enable
tui disable

# Split view with source + assembly
layout split
```

### Other Useful Commands

```gdb
# Check GDT/IDT/TSS contents (via QEMU monitor)
# In the QEMU monitor (switch with Ctrl-Alt-2, or use -monitor stdio):
#   info registers
#   info tlb
#   info mem

# Execute QEMU monitor commands from GDB
monitor info registers
monitor info mem

# Write a value to a specific address
set *(int*)0x3400 = 0x90909090

# Auto-display (shown after each step)
display/x $eip
display/x $cs
display/i $eip

# Remove auto-display
undisplay 1
```

---

## 9. Troubleshooting

### Problem: Architecture is not set correctly

**Symptom:** GDB interprets instructions as 16-bit mode, and disassembly output looks wrong.

**Cause:** Since QEMU starts in BIOS real mode, GDB may detect the architecture as 16-bit (i8086).

**Solution:**

```gdb
set architecture i386
```

After the `run` symbol (the entry point, at address `0x3400`), execution is in 32-bit protected mode, so specify `i386`. The `i8086` setting is only needed when debugging the bootloader portion (`start.s`).

### Problem: Symbols not found

**Symptom:** `break main` displays "Function "main" not defined."

**Cause 1:** You are using the stripped `_kernel`.

**Solution:** Specify `i386/_kernel_dbg` (with symbols) when starting GDB. `_kernel` is stripped and contains no symbols.

**Cause 2:** The symbol file is not loaded correctly.

**Solution:**

```gdb
symbol-file i386/_kernel_dbg
```

### Problem: Symbol addresses are misaligned

**Symptom:** Execution stops at a breakpoint, but source display is offset. Or, breakpoints are never reached.

**Cause:** The kernel's link address (`-Ttext=0x3400`) does not match the actual load address.

**Solution:**

```gdb
# Explicitly specify the text section start address of the symbol file
add-symbol-file i386/_kernel_dbg 0x3400
```

### Problem: Cannot connect with target remote

**Symptom:** `target remote :1234` displays "Connection refused".

**Cause:** QEMU was not started with the `-s` option, or has not started yet.

**Solution:**

1. Verify that QEMU was started with the `-G` option: `./run.sh -G`
2. Start QEMU first, then connect with GDB
3. Check that the port is not in use by another process: `ss -tlnp | grep 1234`

### Problem: Breakpoint is not reached after continue

**Symptom:** After setting `break main` and executing `continue`, execution does not stop.

**Cause:** QEMU executes in the order BIOS -> bootloader -> kernel. Address 0x3400 is the kernel binary's entry point (`run.s`), and `main()` is placed at a variable address beyond that. During BIOS execution, the CPU is in 16-bit mode and the kernel code has not been loaded yet. GDB resolves the symbol when you type `break main`, so you do not need to know the specific address.

**Solution:**

The bootloader must correctly load the kernel and transition to protected mode. Verify the following:

```gdb
# Set a hardware breakpoint at the kernel entry address
hbreak *0x3400
continue
```

`hbreak` (hardware breakpoint) works across mode transitions. A regular `break` (software breakpoint) may not function correctly after a mode transition.

> **Reason:** Software breakpoints work by replacing an instruction with INT 3 (0xCC), so they can stop working when the address space interpretation changes during a real-mode to protected-mode transition. Hardware breakpoints use the CPU's debug registers (DR0-DR3) and are mode-independent.

### Problem: Losing context after task switch with stepi

**Symptom:** After `start_first_task`, GDB can no longer track the execution position.

**Cause:** The sequence `ret` -> `intr_return_restore` -> `RESTORE_ALL` -> `iret` replaces all registers and switches ESP to the Ring 3 stack, causing GDB's step execution to become discontinuous.

**Solution:**

```gdb
# Set a breakpoint at the task entry point and then continue
break first_task
continue

# Check the current TSS contents via the QEMU monitor
monitor info registers
```

### Problem: Stopping too frequently on timer interrupts

**Symptom:** Setting `break c_intr_irq0` causes the breakpoint to be hit so frequently that debugging becomes difficult.

**Solution:**

```gdb
# Stop only after N occurrences
break c_intr_irq0
ignore 1 100    # Ignore the first 100 hits

# Conditional breakpoint
break c_intr_irq0 if k_nest0 > 0    # Only on nested interrupts

# Temporarily disable timer interrupts to debug other parts
# (In the QEMU monitor)
monitor info pic
```

### Reference: Useful QEMU Options

```bash
# Output interrupt and CPU reset logs
./run.sh -d -G
# -> Detailed logs are recorded in qemu.log

# Output QEMU monitor to stdio (when used with GDB)
# Edit run.sh and add the following:
#   -monitor stdio
```

---

## Appendix: Example Debugging Session

The following is a typical session for debugging from kernel startup to user task execution.

```
# Terminal 1: Start QEMU
$ ./run.sh -G
GDB mode: waiting for connection on port 1234...
Starting QEMU...

# Terminal 2: Connect GDB
$ gdb i386/_kernel_dbg
(gdb) set architecture i386
(gdb) directory i386 kernel lib
Source directories searched: /home/.../tiny-itron/i386:...
(gdb) target remote :1234
Remote debugging using :1234
0x0000fff0 in ?? ()

# Set a breakpoint at the kernel's main
(gdb) break main
Breakpoint 1 at 0x3xxx: file main.c, line 24.
(gdb) continue
Continuing.

Breakpoint 1, main () at main.c:24
24          ccli();

# Step through the main function
(gdb) next
25          all_init();
(gdb) next
26          page_init();
(gdb) next
27          page_enable();
(gdb) next
28          itron_init();
(gdb) next
...

# Set a breakpoint at first_task
(gdb) break first_task
Breakpoint 2 at 0x...
(gdb) continue
Continuing.

Breakpoint 2, first_task () at user.c:11
11          T_CTSK  ctsk;

# Verify that we have entered user mode
(gdb) print/x $cs
$1 = 0x5b

# Set a breakpoint at usr_main
(gdb) break usr_main
Breakpoint 3 at 0x...
(gdb) continue
Continuing.

Breakpoint 3, usr_main (arg=0x12345) at user.c:105

# Check timer interrupt behavior
(gdb) break c_intr_irq0
Breakpoint 4 at 0x...
(gdb) continue
Continuing.

Breakpoint 4, c_intr_irq0 () at interrupt.c:...
(gdb) print/x $cs
$2 = 0x20
# -> The interrupt handler is executing in kernel mode (Ring 0)

# Trace context switching
(gdb) break sched_do_next_tsk
(gdb) continue
```

---

## Appendix: Memory Map Overview

| Address | Contents |
|:--|:--|
| `0x0000` - `0x33FF` | Bootloader, GDT/IDT tables |
| `0x3400` - | Kernel code (`.text` section start) |
| `0x7A0000` | Initial kernel stack value (set in `run.s`) |
| `0xB8000` - `0xBFFFF` | VGA text buffer |

---

This guide covers tiny-itron Micro ITRON v4.0.0 for i386.
