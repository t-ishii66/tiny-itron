# Build System Guide

This document explains the entire process from building tiny-itron to creating the floppy image.

---

## Development Environment

Development and testing are done on Ubuntu 24.04 LTS (amd64).

### Required Packages

```bash
# Build tools (GCC, GNU Make, binutils)
# gcc-multilib: needed to generate 32-bit (-m32) code on a 64-bit OS
sudo apt install build-essential gcc-multilib

# Emulator
sudo apt install qemu-system-x86

# Debugger (optional)
sudo apt install gdb
```

### Verified Versions

| Tool | Version |
|------|---------|
| GCC | 13.3.0 |
| GNU Make | 4.3 |
| QEMU | 8.2.2 |
| GDB | 15.0 |

> **Note:** GCC generates Pentium Pro or later instructions such as `cmov` by default.
> The QEMU `-cpu` option must be `pentium3` or higher (`-cpu 486` will not work).
> `run.sh` sets this option automatically.

---

## Build Commands

```bash
make                # Full build (kernel → lib → i386, in order)
make clean          # Full clean
```

The top-level `Makefile` invokes the three subdirectories in the correct order.
To build individually:

```bash
make -C kernel      # Kernel library (libkernel.a)
make -C lib         # User library (libc.a)
make -C i386        # Link, binary conversion, image generation
```

The final artifact is `i386/i386` (raw binary for the floppy image).

---

## Build Pipeline Overview

```
  kernel/*.c           lib/*.c
      │                    │
      ▼                    ▼
  libkernel.a          libc.a
      │                    │
      ├────────┬───────────┘
      │        │
      │    i386/*.c, i386/*.s
      │        │
      ▼        ▼
   ld → _kernel (ELF)
            │
        ┌───┤
        │   ▼
        │  strip → _kernel (stripped ELF)
        │   │
        │   ▼
        │  elf → kernel (flat binary)      genasm → table.s → table (GDT binary)
        │   │                                                    │
        │   │           start.s → start (16-bit binary)          │
        │   │               │                                    │
        │   │    boot.s → boot (boot sector, 512 bytes)          │
        │   │       │       │           │                        │
        │   │       ▼       ▼           ▼                        ▼
        │   │      cat boot table start kernel > i386
        │   │                                      │
        │   │                                      ▼
        │   │                          dd → floppy.img (1.44 MB)
        │   │
        ▼   ▼
     _kernel_dbg (copy with symbols, for GDB)
```

---

## Details of Each Step

### Step 1: kernel/ — Compiling the ITRON Kernel

```makefile
# kernel/Makefile
CC = gcc
CFLAGS = -Wall -m32 -fno-pie -fno-stack-protector -fno-builtin

OBJS = kernel.o pool.o sys_tsk.o sys_sns.o sys_tex.o syscall.o sched.o user.o \
       sys_sem.o sys_flg.o sys_dtq.o sys_mbx.o sys_mtx.o sys_mbf.o \
       sys_por.o sys_mpf.o sys_mpl.o sys_tim.o sys_cyc.o sys_alm.o \
       sys_ovr.o sys_rdq.o sys_isr.o

all: $(OBJS)
    ar r libkernel.a $(OBJS)
```

**Meaning of CFLAGS:**
- `-m32` — Generate 32-bit code (required for i386 protected mode even when the host is 64-bit)
- `-fno-pie` — Disable position-independent executables. Forces the compiler to generate absolute-address `call`/`jmp` instructions.
  With PIE enabled, the compiler generates indirect references through the GOT/PLT, but on bare metal there is no dynamic linker, so the GOT remains unresolved and the system crashes.
  This flag is essential for the fixed-address linking in `kernel.ld` (`. = 0x3400`) to work correctly
- `-fno-stack-protector` — Disable stack canaries (`__stack_chk_fail` does not exist)
- `-fno-builtin` — Do not use GCC built-in functions (`memcpy`, etc. do not exist)

Output: `kernel/libkernel.a` (static library)

### Step 2: lib/ — Compiling the User Library

```makefile
# lib/Makefile
OBJS = lib_mbf.o lib_tsk.o lib_int.o lib_sem.o lib_tim.o

all: $(OBJS)
    ar r libc.a $(OBJS)
```

Output: `lib/libc.a` (static library of syscall wrappers)

### Step 3: i386/ — Linking and Binary Generation

#### 3a: genasm — GDT Generation Tool

```makefile
genasm: genasm.c
    $(CC) -o genasm genasm.c    # Tool that runs on the host

table.s: genasm
    ./genasm                     # Generates table.s

table: table.o
    $(LD) -m elf_i386 --oformat binary -Ttext 0x0 -o table table.o
```

`genasm.c` is a program that runs on the host. It writes out each GDT entry
as `.byte` directives into `table.s`.
It generates 9 GDT entries:

| # | Selector | Type |
|---|----------|------|
| 1 | 0x08 | 16-bit kernel code |
| 2 | 0x10 | 16-bit kernel data |
| 3 | 0x18 | 16-bit kernel stack |
| 4 | 0x20 | 32-bit kernel code |
| 5 | 0x28 | 32-bit kernel data |
| 6 | 0x30 | 32-bit kernel stack |
| 7 | 0x58 | 32-bit user code (DPL=3) |
| 8 | 0x60 | 32-bit user data (DPL=3) |
| 9 | 0x68 | 32-bit user stack (DPL=3) |

The TSS entries (0x38 = SEL_TSS0, 0x40 = SEL_TSS1) and the syscall call gate (0x70, SEL_SYSCALL) are
set dynamically at kernel startup via `tss_init()` → `set_gdt()`.
GDT slots 0x48 and 0x50 are currently unused (reserved).

Output: `table` (4096 bytes. The GDT data itself is 256 bytes, but padded by `.org 4096`)

#### 3b: elf — ELF Conversion Tool

```makefile
elf: elf.c
    $(CC) -o elf elf.c           # Tool that runs on the host
```

`elf.c` processes the PT_LOAD segments of an ELF binary in LMA (physical address) order
and outputs a flat binary. It strips the ELF header, section tables, and other metadata,
producing a memory image that the CPU can directly execute.

The BSS region between the two LOAD segments (kernel + user) is filled with zero padding.
This means that when boot.s flat-loads the binary at 0x3400,
`.bss` zero-initialization and correct address placement of `.user_text` are both
completed automatically.

```
ELF (_kernel)                              Flat binary (kernel)

├── ELF header           ─── removed
├── Program headers      ─── removed
│                                          ┌─────────────────────────────┐
├── kernel segment       ─── extract ────→ │ .text + .rodata + .data     │
│     (.text〜.bss)                        │ zero padding (for BSS)      │
│                                          ├─────────────────────────────┤
├── user segment         ─── extract ────→ │ .user_text + .user_data     │
│     (.user_text〜)                       └─────────────────────────────┘
│
└── Section table        ─── removed
```

#### 3c: start — Second-Stage Boot Loader

```makefile
start: start.o
    $(LD) -m elf_i386 --oformat binary -Ttext 0x0 -o start start.o
```

`start.s` is 16-bit real mode code:
1. Enable the A20 line (via the keyboard controller)
2. Load GDT / IDT
3. Set CR0.PE bit → transition to protected mode
4. Far jump to 32-bit code (0x3400 = run.s)

`--oformat binary` outputs a raw binary without ELF headers.
`-Ttext 0x0` places the code starting at address 0.

Output: `start` (1024-byte 16-bit binary, padded by `.org 1024`)

#### 3d: Linking the Kernel

```makefile
LDFLAGS = -m elf_i386 -n -T kernel.ld -static

kernel: elf $(OBJS)
    $(LD) $(LDFLAGS) -e run -o _kernel $(OBJS)
    cp _kernel _kernel_dbg
    strip _kernel
    ./elf _kernel kernel
```

**Meaning of LDFLAGS:**
- `-m elf_i386` — i386 ELF format
- `-n` — Disable page alignment of data segments
- `-T kernel.ld` — Use the linker script to control all section placement (start address 0x3400, `.user_text` separation)
- `-static` — No dynamic linking
- `-e run` — Entry point is the `run` symbol (run.s)

$(OBJS) includes the `.o` files from i386/ as well as `libkernel.a` and `libc.a`.

Processing flow:
1. `ld` generates `_kernel` (ELF)
2. Copy to `_kernel_dbg` (preserve a version with symbols for GDB)
3. `strip` removes the symbol table (size reduction)
4. `./elf` converts ELF → flat binary

Output: `kernel` (flat binary), `_kernel_dbg` (ELF with symbols for GDB)

#### 3e: Boot Sector

```makefile
# i386/boot/Makefile
boot: boot.s
    $(AS) --32 -o boot.o boot.s
    $(LD) -m elf_i386 --oformat binary -Ttext 0x0 -o boot boot.o
```

`boot.s` occupies the first 512 bytes of the floppy. The BIOS loads it automatically.

Processing flow:
1. Copy itself to 0x7000:0000 (freeing up 0x7c00)
2. Read floppy sectors 1-299 into 0x0200:0000 and beyond
3. Far jump to 0x0300:0000 (start.s)

The last 2 bytes are `0xAA55` (BIOS boot signature).

Output: `boot` (512-byte boot sector, `.org 510` + 2-byte signature)

#### 3f: Combining the Final Image

```makefile
all: start genasm kernel table
    cat boot table start kernel > i386
```

Four binaries are simply concatenated:

| File | How size is fixed | Fixed size | cat offset |
|------|-------------------|-----------|------------|
| boot | `.org 510` + 2B signature | 512 B (1 sector) | 0x0000 |
| table (GDT) | `.org 4096` | 4096 B (8 sectors) | 0x0200 |
| start | `.org 1024` | 1024 B (2 sectors) | 0x1200 |
| kernel | variable | ~113 KB | 0x1600 |

Output: `i386/i386` (raw binary image)

---

## How Flat Loading Produces an Executable Image

The kernel becomes executable simply by flat-loading the floppy image.
**`.org`, `kernel.ld`, `-fno-pie`, and `elf.c` work together to make this possible.**
This section explains the entire mechanism as a single flow.

### 4.1 cat Concatenation + Flat Load → Memory Addresses Are Determined

`cat` simply lays files out sequentially, so **each file's byte count directly
determines where the next file starts.** Since `.org` fixes those sizes,
the concatenation order and memory layout always produce the same result.

boot.s reads sectors 1 onward (= everything after boot) into memory starting at
**linear address 0x2000**. The `cat` concatenation order maps directly to memory layout:

```
Floppy                       Memory after load              Corresponding constants
─────────────                ──────────────────             ────────────────────────
Sector 0  (boot)             0x7C00 (loaded by BIOS)        AL_BOOT
                             ↓ boot.s flat-loads sectors 1+ to 0x2000
Sectors 1-8   (table)   →   0x2000                          AL_GDT
Sectors 9-10  (start)   →   0x3000                          AL_KERNEL16
Sectors 11+   (kernel)  →   0x3400                          kernel.ld ". = 0x3400"
     :            :            :     .text, .rodata, .data
     :            :            :     BSS zero region (pre-filled by elf.c)
     :            :            :     .user_text (page-aligned)
     :            :            :     .user_data
Sector ~232                  End of kernel image
```

### 4.2 Origin of 0x3400 — Sum of .org Sizes

The linker script's start address `". = 0x3400"` is not an arbitrary value.
It is **uniquely determined from boot's load destination and the sum of .org sizes**:

```
". = 0x3400"  =  0x2000 (boot's load destination: real mode segment 0x0200 × 16)
               + 4096   (.org size of table)
               + 1024   (.org size of start)
              = 0x3400
```

The constants in `addr.h` are based on the same calculation:

| Constant | Value | Origin |
|----------|-------|--------|
| `AL_GDT` | 0x2000 | boot's load destination |
| `AL_IDT` | 0x2100 | Immediately after GDT (placed within the GDT table) |
| `AL_KERNEL16` | 0x3000 | 0x2000 + 4096 (table size) |
| (kernel start) | 0x3400 | 0x3000 + 1024 (start size) = kernel.ld's ". = 0x3400" |

**If you change even one `.org` value, all subsequent addresses shift.**
For example, changing `.org 4096` in genasm.c to `.org 2048` would place start at 0x2800.
But start.s contains `ljmp $0x20, $0x3400` (hardcoded) to jump to the kernel,
so it would jump to 0x3400 even though the kernel is actually at 0x2C00, causing a failure.

### 4.3 Absolute Address Linking → No Relocation Needed

Thanks to `-fno-pie` (Step 1) and `kernel.ld`'s `. = 0x3400` (Step 3d),
all addresses in the code are resolved assuming the final memory layout.

```
① Compile   gcc -m32 -fno-pie
             → Generates object code with absolute address references
               (direct call/jmp without GOT/PLT)

② Link      ld -T kernel.ld  (". = 0x3400")
             → Resolves all symbols to absolute addresses based at 0x3400
               Example: main = 0x4A2C, first_task = 0x1E120 (values vary by build)

③ ELF→raw   ./elf _kernel kernel
             → Conversion tool built from elf.c. Reads the ELF-format _kernel,
               strips headers, and outputs the flat binary kernel.
               The first byte of the kernel file corresponds to address 0x3400

④ Combine   cat boot table start kernel > i386
             → Due to .org fixed sizes, kernel's offset is 0x1600

⑤ Load      boot.s reads sectors 1+ into 0x2000
             → kernel lands at 0x2000 + 5120 = 0x3400
             → The linker's assumed addresses match the runtime layout
             → No relocation needed, no copying needed, ready to execute as-is
```

### 4.4 elf.c Guarantees BSS and .user_text Placement

Because `elf.c` inserts zero padding for the BSS during flat binary conversion,
`.user_text` is placed at the correct offset within the flat binary
(= `_user_text_start` - 0x3400).
When boot.s flat-loads the image, `.user_text` lands on a page boundary.
**There is no post-load routine that copies data to a page boundary.**

The same applies to BSS zero-initialization: a normal ELF loader would zero-clear
BSS in memory, but `elf.c` writes zeros into the flat binary itself, so
loading from the floppy is sufficient to complete initialization.

### 4.5 Relationship Between Code and Stack at Task Creation

When a user task is created with `cre_tsk`, **the code is not copied.**
The only thing that is new for each task is **the stack**.

```
.user_text (_user_text_start〜)             Stack pool (0x700000〜)
┌──────────────────────┐                   ┌──────────────┐
│ first_task()         │←─ Task 1 EIP     │ stack_alloc()│← Task 1 ESP
│ second_task()        │←─ Task 2 EIP     │ stack_alloc()│← Task 2 ESP
│ usr_main()           │←─ Task 3 EIP     │ stack_alloc()│← Task 3 ESP
│ kbd_task()           │←─ Task 4 EIP     │ stack_alloc()│← Task 4 ESP
│ syscall wrappers     │                   └──────────────┘
│ (cre_tsk, slp_tsk..) │
└──────────────────────┘
       ↑ shared by all tasks            ↑ independent per task
```

`proc_create()` sets the EIP to the task function's address (within `.user_text`)
and the ESP to the top of the stack allocated by `tsk_stack_alloc()` (within the stack pool).
The task function's code is executed directly from its original location in `.user_text`.
Even when multiple tasks run the same function, only one copy of the code exists.

---

## Linker Script (kernel.ld) Details

### Section Layout

`kernel.ld` controls the section layout within the kernel binary:

```
kernel.ld SECTIONS                    Memory layout
──────────────────────                ───────────────────
. = 0x3400;                           ← from .org calculation (§4.2)
.text    (kernel code)        →  0x3400 onwards
.rodata  (read-only data)     →  immediately after .text
.data    (initialized data)   →  after ALIGN(32)
.bss     (zero-initialized)   →  immediately after .data

. = ALIGN(0x1000);                    ← round up to page boundary
.user_text (user code)        →  address indeterminate until build
  *(.user_text)                    Ring 3 task functions
  *libc.a:*(.text)                 syscall wrappers (lib/)
```

### .user_text Address Varies With Build Output

The end of `.bss` is determined by the total size of `.text`/`.rodata`/`.data`/`.bss`,
so it can change whenever the source is modified. `ALIGN(0x1000)` rounds this up
to a page boundary, and that becomes the start address of `.user_text`.

In the current build, the end of `.bss` is around 0x1D608, so the result of `ALIGN(0x1000)` is 0x1E000.
However, if global variables are added, `.bss` grows, and the address could become 0x1F000 or 0x20000.

### Why Two Separate LOAD Segments

- `kernel` segment (RWX): The kernel itself. Because it includes `.bss`, memory size > file size
- `user` segment (RWX): User tasks and syscall wrappers (`.user_text`) + user data (`.user_data`). Code and data that are executed/referenced in Ring 3

### Linker Symbols and Their Interaction With page.c

`page.c` references the linker symbols `_user_text_start` / `_user_data_end`
and sets only the pages for `.user_text` + `.user_data` to User+RW.
Kernel code and data (0x0 to `_user_text_start`) are Supervisor (U/S=0)
and cannot be accessed from Ring 3. The memory/stack pools (0x110000-0x74FFFF) are
also set to User+RW. Even when the `.user_text` address changes, `page_init()`
automatically computes the boundary from linker symbols, so no manual changes are needed.
(See [memory-map.md §2](memory-map.md#2-paging-policy) for details)

### How to Check Addresses After Building

```bash
# Display ELF program headers (VMAs of LOAD segments)
readelf -l i386/_kernel_dbg

# Check specific linker-emitted symbols
nm i386/_kernel_dbg | grep _user_text
# Example output:
#   0001e000 T _user_text_start
#   0001ed85 T _user_text_end
```

> **Measured values from the current build** (will vary with source changes):
>
> | ELF segment | LMA | filesz | memsz | Sections included |
> |-------------|-------|--------|-------|-------------------|
> | kernel | 0x3400 | 0xF41C (62 KB) | 0x1A188 (106 KB) | .text .rodata .eh_frame .data .bss |
> | user | 0x1E000 | 0xF40 (3.9 KB) | 0xF40 | .user_text .user_data |
>
> Flat binary size: 113,472 bytes (= 0xF41C + BSS padding + 0xF40)

---

## Fixed Constants for Memory Regions

The memory pool and stack pool ranges are fixed constants hardcoded in `addr.h`.

```c
/* addr.h */
#define MEM_START       0x110000    /* mem_alloc pool start */
#define MEM_END         0x6fffff    /* mem_alloc pool end */
#define STACK_START     0x700000    /* stack_alloc pool start */
#define STACK_END       0x74ffff    /* stack_alloc pool end */
#define KERN_STACK_SIZE 4096        /* kernel stack per task (4KB) */
#define KERN_STACK_BASE 0x750000    /* kernel stack region start */
```

`pool.c` simply uses these constants and has no knowledge of `page.c`.
Conversely, `page.c` uses `MEM_START` / `USER_MEM_END` to determine the User page range,
but has no knowledge of `pool.c`. **There is no mechanism to automatically check
consistency between the two.**

In practice, `page.c` and `pool.c` manage **the same address range** (0x110000-0x74FFFF).
`page.c` sets the pages in this range to User+RW, and `pool.c` allocates memory
from the same range. The two remain consistent because **both derive their ranges
from the same set of constants in `addr.h`**.

Additionally, the constants in `addr.h` are manually designed so that regions do not overlap:

```
  0x0000           0x1D588  0x1EF40  0x110000          0x750000   0x770000
  |                |        |        |                 |          |         |
  | kern .text/bss | u_text | ~900KB | mem+stk pool    | kern stk | CPU stk |
  +----------------+--------+--------+-----------------+----------+---------+
  |  Supervisor    | User   | Supvsr |    User(RW)     |    Supervisor      |
  |--- loaded by boot.s ----|        |                 |                    |
  |    (addrs from §4)      | unused |                 |                    |

                                               (fixed constants in addr.h)
```

`kern stk` is the 4KB kernel stack region for each task (starting at KERN_STACK_BASE).
The top of Task N's kernel stack = `KERN_STACK_BASE + (N+1) * KERN_STACK_SIZE`.

Between the end of the kernel + user code (currently around 0x1EF40) and MEM_START (0x110000),
there is a gap of approximately 900 KB. This region is used as the kernel memory pool (`kmem_alloc`),
initialized in `itron_init()` via `kmem_init(&_user_data_end, MEM_START)`.

If the kernel grows so large that `.user_data_end` exceeds 0x110000:
- `page.c` computes the User region from linker symbols, so page protection still works correctly
- However, the addresses returned by `pool.c`'s `mem_alloc()` would **physically overlap**
  with the user code region. Neither the linker nor the compiler detects this
- **Symptom**: Memory allocations overwrite user code, causing tasks to halt with illegal instructions

---

## Boot Loader Sector Limit

The `cmpw $300, %ax` in boot.s implicitly determines the upper bound of the kernel binary size:

```
Sectors 1-299 = 299 sectors × 512 B = 153,088 B (approximately 149 KB)
Subtracting table (4096 B) + start (1024 B):
Maximum kernel binary size ≈ 147,968 B (approximately 144 KB)
```

The current kernel binary is approximately 113 KB, leaving about 35 KB of headroom.
If this limit is exceeded:
- boot.s will not load the tail end of the kernel
- The end of `.user_text` will be missing, and syscall wrapper functions will point to nonexistent memory
- When a user task calls a syscall, it executes garbage instructions and halts with a triple fault
- **Since the symptom is a runtime crash rather than a link error, identifying the cause is extremely difficult**

Countermeasure: Increase the value of `cmpw $300` in boot.s. A floppy can hold up to
2880 sectors (1.44 MB), but be aware of the real-mode 64 KB segment boundary.

---

## Impact Matrix for Changes

| Change | Affected files | Symptom |
|--------|---------------|---------|
| Change `.org` in genasm.c | kernel.ld (". = 0x3400"), start.s (far jump target), addr.h (AL_GDT, etc.) | Kernel linked at wrong address → boot failure |
| Change `.org` in start.s | kernel.ld (". = 0x3400"), addr.h (AL_KERNEL16) | Same as above |
| Change ". = 0x3400" in kernel.ld | start.s (far jump target), genasm.c/.org, addr.h | Same as above |
| Kernel grows due to source additions | boot.s (`cmpw $300`) | Tail not loaded → runtime triple fault |
| Remove `-fno-pie` | All `.o` files | GOT/PLT references → link error or runtime crash |
| Remove `-m32` | All `.o` files | 64-bit code generated → cannot execute in protected mode |
| Change `.user_text` in kernel.ld | elf.c (segment processing), page.c (U/S boundary) | User code misplaced → syscall failure |

---

## Running in QEMU

### Floppy Image Generation in run.sh

```bash
# Create an empty 1.44 MB floppy image
dd if=/dev/zero of=floppy.img bs=512 count=2880

# Write the kernel image to the beginning
dd if=i386 of=floppy.img conv=notrunc
```

1.44 MB = 512 bytes × 2880 sectors (= 1,474,560 bytes).
The kernel is approximately 120 KB, so the remaining space is filled with zeros.

### QEMU Command

```bash
qemu-system-i386 \
    -drive file=floppy.img,format=raw,if=floppy \
    -boot a \           # Boot from floppy
    -m 16 \             # 16 MB memory
    -cpu pentium3 \     # Pentium III or higher (required for cmov instruction)
    -accel tcg,thread=multi \   # Multi-threaded TCG
    -smp 2 \            # 2 CPUs
    -display curses     # VGA text display on terminal
```

**Why `-cpu pentium3` is required:**
GCC generates Pentium Pro or later instructions such as `cmov` (conditional move) by default.
With `-cpu 486`, an undefined instruction exception (#UD) occurs.

---

## Memory Transitions During the Boot Sequence

```
BIOS
 │ Loads floppy sector 0 to 0x7C00
 ▼
boot.s (0x7C00)
 │ Copies itself to 0x70000
 │ Loads sectors 1-299 to 0x02000 onward
 ▼
start.s (0x3000)
 │ Enable A20
 │ Load GDT (0x2000) / IDT (0x2100)
 │ Transition to protected mode
 │ far jump → 0x3400
 ▼
run.s (0x3400)
 │ Set kernel segments (DS=0x28, SS=0x30)
 │ Check cpu_num (BSP: stack 0x7a0000, AP: 0x770000)
 │ call main
 ▼
main() [BSP]                 main() [AP]
 │ all_init()                 │ page_enable()
 │ page_init()                │ smp_ap_init()
 │ page_enable()              │   APIC initialization
 │ itron_init()               │   APIC timer start
 │ proc_init()                │   start_second_task()
 │ tss_init()                 │     ╰→ ltr SEL_TSS1
 │ smp_init()                 │         ╰→ RESTORE_ALL → iret
 │   APIC initialization      │             ╰→ second_task() [Ring 3]
 │   SIPI → AP startup
 │   Timer/KBD start
 │   start_first_task()
 │     ╰→ ltr SEL_TSS0
 │         ╰→ RESTORE_ALL → iret
 │             ╰→ first_task() [Ring 3]
```

---

## Clean and Rebuild

```bash
make -C kernel clean
make -C lib clean
make -C i386 clean
make -C i386/boot clean

# Rebuild
make -C kernel && make -C lib && make -C i386
```

---

## Related Documents

- [Source Code Reading Guide](source-guide.md) — File listing and recommended reading order
- [Memory Map](memory-map.md) — Physical address layout
- [i386 Architecture](i386-architecture.md) — GDT, protected mode
- [Boot Sector](boot-sector.md) — Details of boot.s
