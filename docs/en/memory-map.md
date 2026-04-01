# Memory Map

This document describes the memory layout of tiny-itron.
The kernel and user tasks are laid out flat within a **single linear address space**.
Paging (CR0.PG=1) is enabled with **full identity mapping** (virtual address = physical address),
and memory protection between kernel and user is enforced via the **U/S bit**.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Paging Policy](#2-paging-policy)
3. [Segment Configuration (GDT)](#3-segment-configuration-gdt)
4. [Low Memory: Boot and Table Area](#4-low-memory-boot-and-table-area)
5. [Kernel Code/Data and User Code](#5-kernel-codedata-and-user-code)
6. [User Memory Pool (mem_alloc)](#6-user-memory-pool-mem_alloc)
7. [Kernel Memory Pool (kmem_alloc)](#7-kernel-memory-pool-kmem_alloc)
8. [Stack Pool (stack_alloc)](#8-stack-pool-stack_alloc)
9. [Per-CPU Stacks](#9-per-cpu-stacks)
10. [VGA Text Buffer](#10-vga-text-buffer)
11. [Local APIC (Memory-Mapped I/O)](#11-local-apic-memory-mapped-io)
12. [Full Address Space Map](#12-full-address-space-map)

---

## 1. Overview

The key characteristics of tiny-itron's memory layout are as follows:

- **Full identity-mapped paging** --- CR0.PG=1 enables paging.
  All pages use identity mapping where virtual address = physical address.
  The U/S bit in page table entries controls kernel/user memory protection
- **Separate user code and data** --- User task code and data are placed in a separate ELF LOAD segment
  from the kernel by the linker script (`kernel.ld`), with VMA = LMA.
  `.user_text` + `.user_data` are placed contiguously at the page boundary just after kernel BSS (around 0x1E000)
- **Flat segments** --- Both kernel and user segments have base=0, limit=4GB
- **Dual protection** --- In addition to Ring 0/Ring 3 privilege levels (restricting privileged instructions),
  the U/S bit in paging enforces memory protection. The kernel code/data region is
  Supervisor (U/S=0) and inaccessible from Ring 3. Only user code/data
  (`.user_text` + `.user_data`) and the memory/stack pools are User (U/S=1).
  The VGA buffer is also Supervisor, so screen output from Ring 3 goes through syscalls

```
  0x00000000 +--------------------------------------+
             |  Low memory (BIOS, boot loader,      |  | Paging:
             |  GDT, IDT, 16-bit boot code)         |  | Supervisor
  0x00003400 +--------------------------------------+  | (Ring 3 -> #PF)
             |  Kernel code + data                  |  |
             |  (.text, .rodata, .data, .bss)       |  |
  0x00010000 +--------------------------------------+  |
             |  Floppy DMA buffer                   |  +
  ~0x1E000   +======================================+
             |  User code (.user_text)              |  | Paging:
             |  User data (.user_data)              |  | User + RW
  ~0x1F000   +======================================+  + (PTE_USER)
             |  Kernel memory pool (kmem_alloc)     |  | Paging:
             |  (~0x20000 - 0x110000, ~960 KB)      |  | Supervisor
  0x000B8000 +--------------------------------------+  | (Ring 3 -> #PF)
             |  VGA text buffer (4 KB)              |  |
             |  (unused)                            |  +
  0x00110000 +======================================+
             |  Memory pool (mem_alloc, ~5.9 MB)    |  | Paging:
  0x00700000 +--------------------------------------+  | User + RW
             |  Stack pool (stack_alloc, 320 KB)    |  | (PTE_USER)
  0x00750000 +======================================+  +
             |  CPU 1 Ring 3 / Ring 0 / boot stack  |  | Paging:
  0x00780000 +--------------------------------------+  | Supervisor
             |  CPU 0 Ring 3 / Ring 0 / boot stack  |  | (Ring 3 -> #PF)
  0x007A0000 +--------------------------------------+  +
             |                                      |
             |          (unmapped region)            |
             |                                      |
  0xFEE00000 +--------------------------------------+ APIC_BASE
             |  Local APIC registers (4 KB)         |  Supervisor + cache-disable
  0xFEE01000 +--------------------------------------+
             |                                      |
  0xFFFFFFFF +--------------------------------------+
```

> **Note:** The diagram above is roughly in address order, but the VGA buffer (0xB8000)
> and the FDC buffer (0x10000) overlap within the kernel region.
> See [12. Full Address Space Map](#12-full-address-space-map) for the precise layout.

---

## 2. Paging Policy

### i386 Paging Overview

i386 paging is enabled by setting CR0.PG=1. When the CPU accesses memory, it walks
the page directory and page tables to translate virtual addresses to physical addresses.
Each page table entry (PTE) contains access control bits:

| Bit | Name | Meaning |
|-----|------|---------|
| P (bit 0) | Present | Page exists in physical memory (=1) |
| R/W (bit 1) | Read/Write | 1=read-write, 0=read-only |
| U/S (bit 2) | User/Supervisor | 1=accessible from Ring 3, 0=Ring 0 only |
| PCD (bit 4) | Cache Disable | 1=caching disabled (for MMIO) |

### tiny-itron's Paging Policy

tiny-itron uses **full identity mapping** for paging.
Virtual addresses always equal physical addresses, and no page swapping is performed.
The sole purpose of paging is **memory protection via the U/S bit**.

```
Page directory (CR3 -> page_dir[])
  +-- entry[0] -> page_table[0]   0x000000 - 0x3FFFFF (4 MB)
  +-- entry[1] -> page_table[1]   0x400000 - 0x7FFFFF (4 MB)
  +-- entry[2..0x3FA] -> not present
  +-- entry[0x3FB] -> page_table_apic  Local APIC (0xFEE00000)
  +-- entry[0x3FC..0x3FF] -> not present
```

### Access Control Zones

| Physical Address Range | PTE Flags | Ring 3 | Contents |
|------------------------|-----------|--------|----------|
| 0x000000 - `_user_text_start` | Supervisor + RW | **#PF** | Kernel .text/.bss/GDT/IDT |
| `_user_text_start` - `_user_data_end` | User + RW | Accessible | .user_text + .user_data |
| `_user_data_end` - 0x10FFFF | Supervisor + RW | **#PF** | kmem pool + VGA + unused gap |
| 0x110000 - 0x74FFFF | User + RW | Accessible | Memory pool + stack pool |
| 0x750000 - 0x7FFFFF | Supervisor + RW | **#PF** | CPU stacks (Ring 0, TSS) |
| 0xFEE00000 | Supervisor + PCD | **#PF** | Local APIC MMIO |

The U/S boundary is split into **3 zones**. `page_init()` uses the linker symbols
`_user_text_start` / `_user_data_end` and the constants `MEM_START` (0x110000) /
`USER_MEM_END` (0x750000) to configure the following 2 User regions:

1. **User code and data**: `_user_text_start` through `_user_data_end`
   (rounded up to page boundary)
2. **Memory/stack pools**: `MEM_START` (0x110000) through `USER_MEM_END` (0x750000)

Everything else is Supervisor.

### Kernel/User Separation Design

The kernel region (0x000000 through `_user_text_start`) is Supervisor (U/S=0),
making it inaccessible to Ring 3 user tasks. To achieve this, the following
design changes were made:

1. **VGA access through syscalls** --- User tasks do not call `vga_write_at()` directly.
   Instead, they use syscall wrappers (`lib/lib_exd.c`) such as `print_at()`,
   `print_dec_at()`, `clear_screen()`, and `fill_at()`.
   These invoke the kernel's `sys_vga_write()` etc. via `INT 0x99`
2. **Shared data in `.user_data`** --- `shared_count` and `task_count[]` are placed
   not in the kernel `.bss` but in the `.user_data` section
   (`__attribute__((section(".user_data")))`).
   Since `.user_data` resides in User pages, it is accessible from Ring 3
3. **Kernel resources through syscalls** --- `stack_alloc()` and
   `key_dtq_id` configuration are all accessed through syscall wrappers
   (`tsk_stack_alloc()`, `set_key_task()`)
4. **How syscall wrappers work** --- The `syscall()` function in `.user_text`
   (`klib.s`) issues `INT 0x99`, transitioning to the kernel's Ring 0 syscall handler.
   No direct `call` to kernel `.text` is made

### Paging Initialization and Activation

```c
/* i386/page.c --- page_init() builds the page tables */
extern char _user_text_start, _user_data_end;  /* linker symbols */

unsigned long u_start = (unsigned long)&_user_text_start & ~(PAGE_SIZE - 1);
unsigned long u_end   = ((unsigned long)&_user_data_end + PAGE_SIZE - 1)
                        & ~(PAGE_SIZE - 1);

for (i = 0; i < PAGE_TABLE_COUNT; i++) {
    for (j = 0; j < PAGES_PER_TABLE; j++) {
        unsigned long addr = (i * PAGES_PER_TABLE + j) * PAGE_SIZE;
        unsigned long flags = PTE_PRESENT | PTE_RW;

        if (addr >= u_start && addr < u_end)
            flags |= PTE_USER;   /* .user_text + .user_data */
        else if (addr >= MEM_START && addr < USER_MEM_END)
            flags |= PTE_USER;   /* memory + stack pools */

        page_table[i][j] = addr | flags;
    }
}

/* i386/page.c --- page_enable() loads CR3 and sets CR0.PG=1 */
void page_enable(void) {
    unsigned long dir = page_get_dir();
    __asm__ volatile(
        "movl %0, %%cr3\n\t"
        "movl %%cr0, %%eax\n\t"
        "orl $0x80000000, %%eax\n\t"   /* Set the PG bit */
        "movl %%eax, %%cr0\n\t"
        : : "r"(dir) : "eax"
    );
}
```

The code above populates `page_table[i][j]` with the following values
(in the current build, `_user_text_start`=0x1E000, `_user_data_end`=0x1F820):

```
page_table[0][j] --- 0x000000-0x3FFFFF (4MB)
 j       Address range                  flags   Entry value          Contents
 ------- ---------------------------- ------- ------------------- ------------------
 0x000   0x000000                     0x03    0x00000003          Kernel (Supervisor)
 0x001   0x001000                     0x03    0x00001003           :
   :         :                          :         :               :
 0x01D   0x01D000                     0x03    0x0001D003          Near end of kernel .bss
 0x01E   0x01E000                     0x07    0x0001E007          <- _user_text_start (User)
 0x01F   0x01F000                     0x07    0x0001F007          .user_text + .user_data
 0x020   0x020000                     0x03    0x00020003          <- gap (Supervisor)
   :         :                          :         :               :
 0x10F   0x10F000                     0x03    0x0010F003          End of gap
 0x110   0x110000                     0x07    0x00110007          <- MEM_START (User)
 0x111   0x111000                     0x07    0x00111007          mem pool + stack pool
   :         :                          :         :               :
 0x3FF   0x3FF000                     0x07    0x003FF007          mem pool (User)

page_table[1][j] --- 0x400000-0x7FFFFF (4MB)
 j       Address range                  flags   Entry value          Contents
 ------- ---------------------------- ------- ------------------- ------------------
 0x000   0x400000                     0x07    0x00400007          mem pool (User)
   :         :                          :         :               :
 0x34F   0x74F000                     0x07    0x0074F007          End of stack pool (User)
 0x350   0x750000                     0x03    0x00750003          <- KERN_STACK_BASE (Supervisor)
 0x351   0x751000                     0x03    0x00751003          Per-task kernel stack
   :         :                          :         :               :
 0x3FF   0x7FF000                     0x03    0x007FF003          CPU stacks (Supervisor)

page_table_apic[j] --- 0xFEC00000-0xFFFFFFFF (4MB, page_dir[0x3FB])
 j       Address                        flags   Entry value          Contents
 ------- ---------------------------- ------- ------------------- ------------------
 0x000   0xFEC00000                   0x00    0x00000000          not present
   :         :                          :         :               :
 0x1FF   0xFEDFF000                   0x00    0x00000000          not present
 0x200   0xFEE00000                   0x13    0xFEE00013          <- Local APIC MMIO (Supervisor)
 0x201   0xFEE01000                   0x00    0x00000000          not present
   :         :                          :         :               :

 page_dir[2] through page_dir[0x3FA] are all 0 (not present).
 Accesses to 0x800000-0xFEBFFFFF result in #PF.

flags: 0x03 = PTE_PRESENT|PTE_RW (Supervisor)
       0x07 = PTE_PRESENT|PTE_RW|PTE_USER (User)
       0x13 = PTE_PRESENT|PTE_RW|PTE_PCD (Supervisor, cache disabled)
```

`page_enable()` is called from `main()` on both the BSP (CPU 0) and AP (CPU 1).
Because of full identity mapping, execution continues at the same address
immediately after setting CR0.PG. There is no need to switch CR3 on context
switches (all tasks share the same page tables).

---

## 3. Segment Configuration (GDT)

The GDT is placed at linear address `0x2000` and generated at build time by `genasm.c`.

### Principal Segments

| Selector | Name | Base | Limit | DPL | Purpose |
|----------|------|------|-------|-----|---------|
| 0x08 | SEL_K16_C | 0x3000 | 0xFFFF | 0 | 16-bit kernel code (boot time) |
| 0x10 | SEL_K16_D | 0xB8000 | 0xFFFF | 0 | 16-bit data (VGA base) |
| 0x18 | SEL_K16_S | 0x3000 | 0x0 | 0 | 16-bit stack (boot time) |
| **0x20** | **SEL_K32_C** | **0x0** | **4GB** | **0** | **32-bit kernel code** |
| **0x28** | **SEL_K32_D** | **0x0** | **4GB** | **0** | **32-bit kernel data** |
| **0x30** | **SEL_K32_S** | **0x0** | **4GB** | **0** | **32-bit kernel stack** |
| 0x38 | SEL_TSS0 | (runtime) | sizeof(tss_t) | 0 | CPU 0 TSS (only esp0/ss0 used) |
| 0x40 | SEL_TSS1 | (runtime) | sizeof(tss_t) | 0 | CPU 1 TSS (only esp0/ss0 used) |
| 0x48 | (unused) | - | - | - | - |
| 0x50 | (unused) | - | - | - | - |
| **0x58** | **SEL_U32_C** | **0x0** | **4GB** | **3** | **32-bit user code** |
| **0x60** | **SEL_U32_D** | **0x0** | **4GB** | **3** | **32-bit user data** |
| **0x68** | **SEL_U32_S** | **0x0** | **4GB** | **3** | **32-bit user stack** |

### What the Flat Model Means

Both the kernel segments (0x20/0x28/0x30) and the user segments (0x58/0x60/0x68)
have **base=0, limit=4GB**. This means:

```
Address 0x3400 as seen through kernel CS=0x20
     = Address 0x3400 as seen through user CS=0x58
     = Physical address 0x3400
```

The **only difference between the two is the DPL (Descriptor Privilege Level)**.
DPL=0 segments can only be used in Ring 0, and DPL=3 segments are used in
Ring 3. However, the address **range is completely identical**.

---

## 4. Low Memory: Boot and Table Area

```
0x0000 +-----------------------------+
       |  Real-mode IVT + BIOS      |  (Used by the CPU; OS does not touch)
0x2000 +-----------------------------+ AL_GDT
       |  GDT (256 bytes)           |  Generated by genasm.c -> loaded by lgdt in start.s
0x2100 +-----------------------------+ AL_IDT
       |  IDT (empty; populated at  |  runtime by idt_init)
0x3000 +-----------------------------+ AL_KERNEL16
       |  start.s (16-bit boot code)|  A20 enable, GDT/IDT load
       |  table.s (GDT byte data)   |  -> CR0.PE=1 -> protected mode
0x3400 +-----------------------------+ kernel.ld: ". = 0x3400"
       |  (kernel code starts here) |
```

- `AL_GDT (0x2000)`: Loaded into the CPU by `lgdt` in start.s
- `AL_IDT (0x2100)`: Loaded by `lidt` in start.s, then `idt_init()` populates all 256 entries
- `AL_KERNEL16 (0x3000)`: Also the start address where the AP (CPU 1) begins execution after SIPI
  (SIPI vector=0x03 -> 0x03 x 0x1000 = 0x3000)

### Why These Addresses --- `.org` and Image Concatenation

These addresses are not arbitrary. They are determined by **`.org` directives at build time**
and **`cat`-based binary concatenation**.

The Makefile concatenates 4 binary files with `cat` to produce the floppy image
(`cat boot/boot table start kernel > i386`). Since `cat` simply appends each file in sequence,
**the size of each file directly determines the offset of the next file.**
If the size is off by even a single byte, all subsequent addresses will be wrong.

To prevent this, 3 of the files fix their sizes using `.org`:

| File | `.org` | Fixed Size | Source |
|------|--------|-----------|--------|
| boot/boot | `.org 510` + `.word 0xAA55` | 512 B | `boot/boot.s` |
| table | `.org 4096` | 4096 B (4 KB) | `table.s` generated by `genasm.c` |
| start | `.org 1024` | 1024 B (1 KB) | `start.s` |

When the boot sector (boot.s) reads sectors starting from sector 1 into linear address 0x2000,
each file is placed at the following location:

```
0x2000 + 0            = 0x2000  <- table (4096 B)   = AL_GDT
0x2000 + 4096         = 0x3000  <- start (1024 B)   = AL_KERNEL16
0x2000 + 4096 + 1024  = 0x3400  <- kernel           = kernel.ld's ". = 0x3400"
```

The start address in the linker script `kernel.ld` (`. = 0x3400`) must match this calculation.
Conversely, if you change the `.org` value of table or start, you must also change `kernel.ld`,
or the kernel's addresses will be broken.
See [boot-sector.md section 10](boot-sector.md#10-floppy-disk-image-structure) for details.

---

## 5. Kernel Code/Data and User Code

### Linker Script (`kernel.ld`)

The linker script `i386/kernel.ld` places the kernel and user code as
**2 LOAD segments** in the ELF binary.
All sections are placed with **VMA = LMA** (identity mapping).

```
LOAD segment   Sections                            VMA = LMA  Flags
-----------------------------------------------------------------------
kernel         .text .rodata .data .bss             0x3400     RWX
user           .user_text .user_data                ~0x1E000   RWX
```

### Kernel Segment (VMA = LMA = 0x3400)

The kernel's `.text`, `.rodata`, `.eh_frame`, `.data`, and `.bss` are
placed contiguously starting at address 0x3400.
`.bss` contains kernel global variables (`proc[]`, `tsk[]`, `current_proc[]`,
lock variables, etc.). Kernel pages are Supervisor (U/S=0), making them
inaccessible to Ring 3 user tasks.

The kernel segment is approximately 105 KB in size (filesz=0xF15C),
and including BSS, memsz is approximately 0x1A208 (106 KB). The BSS end reaches around 0x1D608.

### User Segment (.user_text + .user_data)

User task code is placed in the `.user_text` section using `__attribute__((section(".user_text")))`,
and user data is placed in the `.user_data` section using `__attribute__((section(".user_data")))`.
They are placed in order `.user_text` followed by `.user_data` at the page boundary just after
kernel BSS (0x1E000), with VMA = LMA for identity mapping. Only these pages are User (U/S=1).

Contents of `.user_text`:

| Type | Functions / Contents |
|------|---------------------|
| User tasks | `first_task`, `second_task`, `usr_main`, `kbd_task`, `idle_task` |
| Helpers | `delay`, `draw_header` |
| ITRON syscall wrappers | `syscall` (generic), `cre_tsk`, `act_tsk`, `slp_tsk`, `wup_tsk`, `cre_sem`, `pol_sem`, `sig_sem`, `cre_dtq`, `rcv_dtq`, `cre_mbf`, `psnd_mbf`, `trcv_mbf`, etc. (klib.s) |
| Extended syscall wrappers | `print_at`, `print_dec_at`, `clear_screen`, `fill_at`, `set_key_task`, `tsk_stack_alloc` (lib/lib_exd.c) |
| libc functions | `.text` section from `libc.a` |

Contents of `.user_data`:

| Type | Variables / Contents |
|------|---------------------|
| Shared counter | `shared_count` --- shared counter protected by semaphore |
| Task statistics | `task_count[]` --- execution count per task |

**Accessing kernel functions**: User tasks do not directly call kernel functions
like `vga_write_at()` or `stack_alloc()`. Instead, they use syscall wrappers
(`lib/lib_exd.c`) such as `print_at()` and `tsk_stack_alloc()`.
These transition to kernel Ring 0 via `INT 0x99`.
Keyboard input is passed from the ISR to kbd_task through DTQ 2 (`ipsnd_dtq`/`rcv_dtq`),
and line transfers from kbd_task to first_task use MBF 1 (`psnd_mbf`/`trcv_mbf`)

### ELF to Flat Binary Conversion (`elf.c`)

The host-side build tool `elf.c` processes all PT_LOAD segments
**in LMA (p_paddr) order** to generate a flat binary.
Gaps between segments are zero-padded.

```
Layout within the flat binary:
  Offset 0                         Kernel .text + .rodata + .data (filesz=0xF15C)
  ~0xF15C                          gap = 47,780 bytes (zero-padding for BSS)
  ~0x1AC00 (= 0x1E000 - 0x3400)   User code .user_text + .user_data
  Total: ~113 KB (varies by build)
```

### Boot Loader Sector Limit

boot.s reads floppy sectors 1 through 299 (`cmpw $300`).
This allows storing up to approximately 150 KB for table + start + kernel.
If the kernel binary grows, this value needs to be increased.

---

## 6. User Memory Pool (mem_alloc)

This region is used for dynamic allocation of memory that user tasks access directly.
It is managed by `mem_alloc()` / `mem_free()` in `pool.c`.
This pool resides in **PTE_USER pages**, making it readable and writable
from Ring 3 user tasks.

| Constant | Value | Meaning |
|----------|-------|---------|
| MEM_START | 0x110000 | Pool start address |
| MEM_END | 0x6FFFFF | Pool end address |
| Size | ~5.8 MB | 0x6FFFFF - 0x110000 + 1 = 0x5F0000 = 6,225,920 bytes |
| Alignment | 4 KB | MEM_ALIGN = 0xFFFFF000 (defined in `include/stdio.h`; masks the lower 12 bits with `~0xFFF` to align to 4 KB boundary) |

> **Note**: MEM_START (0x110000) is far above the end of kernel + user code (~0x1EDDB),
> so there is no conflict. With identity mapping (VMA=LMA), user code is placed
> around 0x1E000, leaving ample room before 0x110000.
> The Supervisor gap in between holds the kernel memory pool (kmem_alloc)
> (see [7. Kernel Memory Pool](#7-kernel-memory-pool-kmem_alloc)).

Allocation uses a first-fit strategy, managed by the `mem_pool[]` array (up to 256 entries).
Within the kernel, `mem_alloc()` is used for the following purposes:

- `cre_mpf` --- **Block area** for fixed-size memory pools (pointers returned to users by `get_mpf`)
- `cre_mpl` --- **Pool area** for variable-size memory pools (pointers returned to users by `get_mpl`)

The pointers returned by `get_mpf` / `get_mpl` are addresses within this region,
and user tasks read/write them directly. Therefore, they must reside in PTE_USER pages.

> **Note**: Kernel internal buffers (DTQ/MBF ring buffers, MBX priority headers,
> MPF/MPL management metadata) do not need to be exposed to users,
> so they are allocated from the kernel memory pool (`kmem_alloc`).
> This prevents unauthorized overwrites from Ring 3.

---

## 7. Kernel Memory Pool (kmem_alloc)

This is a kernel-internal-only memory allocation region managed by
`kmem_alloc()` / `kmem_free()` in `pool.c`.
This pool resides in **Supervisor pages** (U/S=0), making it inaccessible
from Ring 3 user tasks (#PF will occur).

| Item | Value | Meaning |
|------|-------|---------|
| Start | `_user_data_end` -> 0x20000 | pool_init rounds up to 4KB boundary |
| End | MEM_START = 0x110000 | Just before the user memory pool |
| Size | ~960 KB | 0x110000 - 0x20000 = 0xF0000 = 983,040 bytes |
| Alignment | 4 KB | MEM_ALIGN = 0xFFFFF000 |

### Usage

Allocates memory for kernel object internal buffers and other data that does not need to be
exposed to user tasks. Under the ITRON specification, buffers accessed only through syscalls
are kernel-internal data and should be protected from Ring 3.

| Caller | Purpose | Size |
|--------|---------|------|
| `cre_dtq` | DTQ ring buffer | `dtqcnt * sizeof(VW)` |
| `cre_mbf` | MBF ring buffer | `mbfsz` |
| `cre_mbx` | Mailbox priority header array | `maxmpri * sizeof(T_MSG*)` |
| `cre_mpf` | MPF management metadata (`allocation_t` array) | `blkcnt * sizeof(allocation_t)` |
| `cre_mpl` | MPL management metadata (`allocation_t` array) | `MAX_MPL_POOL * sizeof(allocation_t)` |

### Choosing Between mem_alloc and kmem_alloc

| Pool | Page Attribute | Ring 3 | Purpose |
|------|---------------|--------|---------|
| `mem_alloc` | PTE_USER | Accessible | Memory returned to users (MPF blocks, MPL pools) |
| `kmem_alloc` | Supervisor | **#PF** | Kernel internal buffers (DTQ, MBF, MBX, management metadata) |

### Initialization

Called within `itron_init()` right after `mem_init()`:

```c
extern char _user_data_end;
kmem_init((VP)&_user_data_end, (VP)MEM_START);
```

`pool_init()` rounds up `_user_data_end` (currently around 0x1F820) to a 4KB boundary,
aligning it to 0x20000. The end is MEM_START (0x110000) as-is.
Allocation uses a first-fit strategy, managed by the `kmem_pool[]` array (up to 256 entries).

### SMP Safety

`kmem_alloc` / `kmem_free` are called while the caller's syscall handler holds `kernel_lk`
(the Big Kernel Lock), so they do not have their own spinlock.

---

## 8. Stack Pool (stack_alloc)

This region is used to allocate stack areas passed to `cre_tsk` during task creation.
The kernel-internal allocation functions are `stack_alloc()` / `stack_free()` in `pool.c`.
Since user tasks (Ring 3) cannot directly call the kernel function of the same name,
they go through the syscall wrapper `tsk_stack_alloc()` (`lib/lib_exd.c`):

```
User task (Ring 3)                  Kernel (Ring 0)
tsk_stack_alloc(1024)  --INT 0x99->  sys_stack_alloc_sc()  ->  stack_alloc(1024)
```

| Constant | Value | Meaning |
|----------|-------|---------|
| STACK_START | 0x700000 | Pool start address |
| STACK_END | 0x74FFFF | Pool end address |
| Size | 320 KB | 0x74FFFF - 0x700000 + 1 = 0x50000 = 327,680 bytes = 320 KB |
| Alignment | 4 KB | |

### Task Stack Allocation Example

```c
/* kernel/user.c --- first_task() creating task 3 */
ctsk.stk   = tsk_stack_alloc(1024);   /* Allocate 1024 bytes from stack pool via syscall wrapper */
ctsk.stksz = 1024;
cre_tsk(3, &ctsk);
```

Inside `proc_create()`, ESP is set to the **top** (high address end) of the stack area.
This is because the x86 stack grows from high addresses toward low addresses.

```c
/* i386/proc.c --- proc_create() */
unsigned long user_esp = ((unsigned long)pk_ctsk->stk + pk_ctsk->stksz) & ~3UL;
```

### Relationship Between Kernel and Task Stacks

```
  0x700000  +-------------+ STACK_START
            | Task 1 stk  | <- proc_init() -> stack_alloc(1024)
  0x700400  +-------------+
            | Task 2 stk  | <- proc_init() -> stack_alloc(1024)
  0x700800  +-------------+
            | Task 5 stk  | <- proc_init() -> stack_alloc(1024)  (idle, CPU 0)
  0x700C00  +-------------+
            | Task 6 stk  | <- proc_init() -> stack_alloc(1024)  (idle, CPU 1)
  0x701000  +-------------+
            | Task 3 stk  | <- first_task() -> tsk_stack_alloc(1024)
  0x701400  +-------------+
            | Task 4 stk  | <- second_task() -> tsk_stack_alloc(1024)
  0x701800  +-------------+
            |   (free)     |
            |     ...      |
  0x74FFFF  +-------------+ STACK_END
```

> All user task stacks are allocated sequentially from the stack pool via `stack_alloc()`.
> Since `proc_init()` allocates for Tasks 1, 2, 5, and 6 in that order, these occupy the first 4 slots.
> Tasks 3 and 4 are allocated from the continuation of the stack pool when user code
> (`first_task()`, `second_task()`) creates them via `cre_tsk`, using
> `tsk_stack_alloc()` (syscall wrapper -> kernel's `stack_alloc()`).

---

## 9. Stack Configuration (SMP + Per-Task Kernel Stack)

### Boot-Time Stacks

In SMP, each CPU needs an independent boot-time stack. These are defined in `addr.h`.

| Address | Constant | Purpose |
|---------|----------|---------|
| 0x7A0000 | CPU0_SP | BSP boot-time stack (set to ESP in run.s) |
| 0x770000 | CPU1_SP | AP boot-time stack (set to ESP in run.s) |

These are used for calling `main()` and kernel initialization.
After `start_first_task()`/`start_second_task()` launch tasks via `ltr` + `RESTORE_ALL` + `iret`,
they are no longer used.

### User Stacks (Common to All Tasks)

All task user stacks are allocated from the stack pool (0x700000-0x74FFFF) via `stack_alloc()`.
Tasks 1 and 2 are allocated within `proc_init()`, and Tasks 3 through 6 are allocated
through `cre_tsk` in user code.

### Per-Task Kernel Stacks

Each task has a 4KB kernel stack. Defined in `addr.h`:

```c
#define KERN_STACK_SIZE   4096       /* 4KB per task */
#define KERN_STACK_BASE   0x750000   /* base of kernel stack area */
```

Kernel stack top for task N = `KERN_STACK_BASE + (N+1) * KERN_STACK_SIZE`

| Task ID | Kernel Stack Range | kern_stack_top |
|---------|-------------------|----------------|
| 1 | 0x752000-0x751001 | 0x752000 |
| 2 | 0x753000-0x752001 | 0x753000 |
| 3 | 0x754000-0x753001 | 0x754000 |
| ... | ... | ... |

The TSS esp0 field is dynamically updated to the `kern_stack_top` of the currently
running task (`tss_update_esp0()`). On a Ring 3 to Ring 0 interrupt, the CPU reads
ESP from TSS.esp0 and automatically switches to that task's kernel stack.

### Roles of the Three Stack Types

```
User task (Ring 3)
  |  ESP -> each task's Ring 3 stack (SP3 or tsk_stack_alloc)
  |
  |  --- INT 0x99 (syscall) or IRQ fires ---
  |  CPU automatically switches to task's kernel stack via TSS.esp0
  v
Kernel (Ring 0)
  |  ESP -> that task's kern_stack_top (different for each task)
  |  SAVE_ALL pushes registers, C handler executes
  |  On task switch, intr_leave swaps ESP to the new task's stack
  |
  |  --- RESTORE_ALL + iret ---
  |  Registers are popped, Ring 3 SS/ESP are restored
  v
Returns to user task (Ring 3)
```

---

## 10. VGA Text Buffer

| Constant | Value | Meaning |
|----------|-------|---------|
| G_BASE | 0xB8000 | Start address of VGA text buffer |
| Size | 80 x 25 x 2 = 4000 bytes | |

Each character = 2 bytes (ASCII code + attribute byte).

```
0xB8000 + (row * 80 + col) * 2 = character position
```

The VGA text buffer (0xB8000) page is set to **Supervisor (U/S=0)**.
If a Ring 3 user task writes directly to 0xB8000, a #PF (page fault) occurs.

Screen output from user tasks is performed **through syscalls**:

| User-side function | Syscall number | Kernel-side implementation |
|-------------------|----------------|--------------------------|
| `print_at()` | TFN_EXD_VGA_WRITE | `sys_vga_write()` |
| `print_dec_at()` | TFN_EXD_VGA_DEC | `sys_vga_write_dec()` |
| `clear_screen()` | TFN_EXD_VGA_CLEAR | `sys_vga_clear()` |
| `fill_at()` | TFN_EXD_VGA_FILL | `sys_vga_fill_at()` |

These syscall wrappers are defined in `lib/lib_exd.c` and transition to
kernel Ring 0 via `INT 0x99` to write to the VGA buffer. Kernel Ring 0 code
can access Supervisor pages, so it can write to 0xB8000.

---

## 11. Local APIC (Memory-Mapped I/O)

The Local APIC registers used for SMP are mapped at physical address `0xFEE00000`.
Since paging uses identity mapping (virtual = physical), APIC access is done by
simply reading from and writing to this address. The APIC page is set to Supervisor + cache-disable.

| Address | Register | Purpose |
|---------|----------|---------|
| 0xFEE00020 | APIC_ID | CPU identification number (bits 24-31) |
| 0xFEE000B0 | APIC_EOI | End-of-interrupt notification (write 0) |
| 0xFEE000F0 | APIC_SVR | APIC enable |
| 0xFEE00300 | APIC_ICR_LOW | IPI send (low half) |
| 0xFEE00310 | APIC_ICR_HIGH | IPI send (high half) |
| 0xFEE00320 | APIC_LVT_TIMER | APIC timer configuration |
| 0xFEE00380 | APIC_TIMER_INIT_COUNT | Timer initial count |
| 0xFEE003E0 | APIC_TIMER_DIV | Timer divider ratio |

Both CPUs read the same physical address `0xFEE00000`, but each CPU accesses
**its own** Local APIC (the hardware routes accesses per CPU).
For example, when CPU 0 reads `APIC_ID` it gets 0, and when CPU 1 reads it, it gets 1.

---

## 12. Full Address Space Map

Below is the precise memory map in physical address order.
Since paging uses full identity mapping, virtual address = physical address.

```
Physical address    Size        U/S              Contents
----------------------------------------------------------------------
0x00000000        8 KB       **Supervisor**    Real-mode IVT + BIOS data
0x00002000        256 B      **Supervisor**    GDT (AL_GDT)
0x00002100        (~4 KB)    **Supervisor**    IDT (AL_IDT)
0x00003000        1 KB       **Supervisor**    16-bit boot code (start.s)
0x00003400        ~105 KB    **Supervisor**    32-bit kernel (.text+.rodata+.data+.bss)
0x00010000        64 KB      **Supervisor**    Floppy DMA buffer (FDC_BUFFER) [Note 1]
0x0001E000        ~3.4 KB    User(RW)          .user_text (user code)         [Note 2]
(cont.)           (varies)   User(RW)          .user_data (shared_count etc.) [Note 3]
0x00020000        960 KB     **Supervisor**    Kernel memory pool (kmem_alloc) [Note 4]
0x000B8000        4 KB       **Supervisor**    VGA text buffer (G_BASE)       [Note 5]
  ...             (unused)   **Supervisor**
0x00110000        5.9 MB     User(RW)          Memory pool (MEM_START-MEM_END)
0x00700000        320 KB     User(RW)          Stack pool (STACK_START-STACK_END)
0x00750000        68 KB      **Supervisor**    Per-task kernel stacks (KERN_STACK_BASE, 4KB x 17)
  ...             (unused)   **Supervisor**    (Between end of kernel stacks 0x761000 and CPU1_SP)
0x00770000        64 KB      **Supervisor**    CPU 1 boot-time stack (CPU1_SP, main() only)
  ...             (unused)   **Supervisor**
0x007A0000        (top)      **Supervisor**    CPU 0 boot-time stack (CPU0_SP, main() only)

> Kernel stacks (0x750000-) are 16 tasks x 4KB = 64KB.
> Boot-time stacks are used only during kernel initialization. During task execution,
> each task's kernel stack is used, and TSS.esp0 is dynamically updated.
  ...             (not present)
0xFEE00000        4 KB       **Supervisor+PCD** Local APIC registers (MMIO)
----------------------------------------------------------------------
[Note 1] FDC_BUFFER overlaps the kernel BSS region,
         but does not physically conflict with variable placement in BSS
[Note 2] .user_text uses VMA=LMA for identity mapping.
         Placed at the page boundary right after kernel BSS (address varies by build)
[Note 3] .user_data is placed immediately after .user_text. Contains global variables
         referenced by user tasks such as shared_count and task_count[]
[Note 4] kmem_alloc pool spans from _user_data_end (->aligned to 0x20000) to MEM_START (0x110000).
         Supervisor pages, inaccessible from Ring 3. VGA buffer (0xB8000) falls within this range
[Note 5] VGA buffer is a Supervisor page. Screen output from Ring 3 goes through syscalls
```

### Key Points

- **Kernel and user code are in the same ELF binary** but are
  **separated into different LOAD segments**.
  Kernel code (`.text`) starts at 0x3400,
  and user code/data (`.user_text` + `.user_data`) are placed around 0x1E000.
  **Both use VMA = LMA** (identity mapping)

- **Task stacks are in a separate region** (stack pool at 0x700000 and above)

- **Paging protection is implemented by defining 2 User regions**.
  (1) `.user_text` + `.user_data` (dynamically determined from linker symbols),
  (2) memory/stack pools (0x110000-0x74FFFF).
  **Everything else is Supervisor** (accessible only from Ring 0).
  The kernel memory pool (`kmem_alloc`, 0x20000-0x110000) is within the Supervisor gap
  and inaccessible from Ring 3

- **The kernel region is Supervisor**. Kernel `.text`, `.bss`, GDT, IDT, etc. are
  inaccessible from Ring 3. User tasks must use `INT 0x99` syscalls
  to use kernel functionality

- **User shared data is placed in `.user_data`**. `shared_count` and
  `task_count[]` are placed in User pages using `__attribute__((section(".user_data")))`,
  making them directly accessible from Ring 3 user tasks

- **The VGA buffer is Supervisor**. Screen output from Ring 3 is done through
  syscall wrappers (`lib/lib_exd.c`) such as `print_at()`.
  The kernel-side `sys_vga_write()` etc. write to the VGA buffer in Ring 0

### Checking Addresses After Building

The placement address of `.user_text` varies depending on source code size
(see [build-system.md](build-system.md#linker-script-kernelld-details) for details).
You can check the actual addresses after building with the following commands:

```bash
# List ELF LOAD segments --- check the VMA of the user segment
readelf -l i386/_kernel_dbg
#   LOAD  0x00f55c 0x0001e000 0x0001e000 0x0XXXX 0x0XXXX RWE 0x1
#                   ^^^^^^^^^ <- user segment start address (varies by build)

# Check precisely using linker symbols
nm i386/_kernel_dbg | grep _user_
#   0001e000 T _user_text_start
#   0001ed85 T _user_text_end
#   0001ed88 D _user_data_start
#   0001ed90 D _user_data_end       <- value used by page_init()
```

> **Note:** `page_init()` references the linker symbols `_user_text_start` / `_user_data_end`
> to dynamically determine the range of User pages. Even if the size or placement address
> of `.user_text` + `.user_data` changes, `page_init()` adapts automatically, so
> no manual changes to the page tables are needed. However, the memory/stack pool
> constants `MEM_START` (0x110000) / `USER_MEM_END` (0x750000) are hardcoded, and
> it is assumed that user sections do not encroach into this range.

### Checking with GDB

```gdb
# Kernel symbol addresses
(gdb) info address main
(gdb) info address sched_do_next_tsk
# -> Range 0x3400-0x1XXXX (kernel .text)

# User code symbol addresses
(gdb) info address first_task
(gdb) info address kbd_task
# -> .user_text range (within the range confirmed by readelf -l)

# User data symbol addresses
(gdb) info address shared_count
(gdb) info address task_count
# -> .user_data range (after _user_text_end)

# Examine page table contents
(gdb) x/4x page_table         # page_table[0][0..3] --- Supervisor (kernel region)
(gdb) x/1x page_table+0x1E0   # Around 0x1E000 --- User+RW (.user_text)
(gdb) x/4x page_table+0x110   # Around 0x110000 --- User+RW (memory pool)
(gdb) x/4x page_table+0x750   # Around 0x750000 --- Supervisor (CPU stacks)

# Examine task kernel stacks
(gdb) print/x current_proc[0]->kern_esp
# -> 0x751XXX-0x75XXXX (within per-task kernel stack region)
(gdb) print/x current_proc[0]->kern_stack_top
# -> Top of task's kernel stack (value set in TSS.esp0)
```

---

## Reference: Where Constants Are Defined

| Constant | Definition File |
|----------|----------------|
| AL_GDT, AL_IDT, AL_KERNEL16 | i386/addr.h |
| CPU0_SP | i386/addr.h |
| CPU1_SP | i386/addr.h |
| KERN_STACK_SIZE, KERN_STACK_BASE | i386/addr.h |
| MEM_START (0x110000), MEM_END | i386/addr.h |
| STACK_START, STACK_END | i386/addr.h |
| FDC_BUFFER | i386/addr.h |
| SEL_K32_C, SEL_U32_C, etc. | i386/addr.h |
| G_BASE | i386/videoP.h |
| APIC_BASE, APIC_ID, etc. | i386/smpP.h |
| `. = 0x3400` (kernel start) | i386/kernel.ld |
| `_user_text_start`, `_user_text_end` | i386/kernel.ld |
| `_user_data_start`, `_user_data_end` | i386/kernel.ld |
| USER_MEM_END (0x750000) | i386/pageP.h |
| PAGE_TABLE_COUNT (2), PTE_USER, PTE_RW | i386/page.h, i386/pageP.h |
