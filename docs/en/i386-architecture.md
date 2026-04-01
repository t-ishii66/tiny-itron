# i386 Architecture Basics — What You Need to Read tiny-itron

This document summarizes the Intel 386 (IA-32) hardware knowledge
required to understand the tiny-itron source code.

---

## Table of Contents

1. [Real Mode and Protected Mode](#1-real-mode-and-protected-mode)
2. [Segmentation](#2-segmentation)
   - [2.1 What Is a Segment?](#21-what-is-a-segment)
   - [2.2 Segment Descriptors](#22-segment-descriptors)
   - [2.3 GDT (Global Descriptor Table)](#23-gdt-global-descriptor-table)
   - [2.4 Selectors](#24-selectors)
   - [2.5 tiny-itron's GDT Layout](#25-tiny-itrons-gdt-layout)
3. [Privilege Levels (Ring 0 / Ring 3)](#3-privilege-levels-ring-0--ring-3)
   - [3.1 The Four Rings](#31-the-four-rings)
   - [3.2 CPL, DPL, and RPL](#32-cpl-dpl-and-rpl)
   - [3.3 Privilege Transitions and Stack Switching](#33-privilege-transitions-and-stack-switching)
   - [3.4 Privilege Levels in tiny-itron](#34-privilege-levels-in-tiny-itron)
4. [IDT (Interrupt Descriptor Table)](#4-idt-interrupt-descriptor-table)
   - [4.1 Gate Descriptors](#41-gate-descriptors)
   - [4.2 Interrupt Gates and Trap Gates](#42-interrupt-gates-and-trap-gates)
   - [4.3 tiny-itron's IDT Layout](#43-tiny-itrons-idt-layout)
5. [TSS (Task State Segment)](#5-tss-task-state-segment)
   - [5.1 TSS Structure](#51-tss-structure)
   - [5.2 Hardware Task Switching](#52-hardware-task-switching)
   - [5.3 How tiny-itron Uses the TSS](#53-how-tiny-itron-uses-the-tss)
6. [PIC (i8259 Interrupt Controller)](#6-pic-i8259-interrupt-controller)
   - [6.1 Role of the PIC](#61-role-of-the-pic)
   - [6.2 Master-Slave Cascade Connection](#62-master-slave-cascade-connection)
   - [6.3 Initialization Sequence (ICW1-ICW4)](#63-initialization-sequence-icw1-icw4)
   - [6.4 IRQ Masking and EOI](#64-irq-masking-and-eoi)
7. [Interrupt Flow: SAVE_ALL / RESTORE_ALL](#7-interrupt-flow-save_all--restore_all)
   - [7.1 What the CPU Does When an Interrupt Occurs](#71-what-the-cpu-does-when-an-interrupt-occurs)
   - [7.2 How SAVE_ALL Works](#72-how-save_all-works)
   - [7.3 How RESTORE_ALL and intr_leave Work](#73-how-restore_all-and-intr_leave-work)
   - [7.4 Nested Interrupts](#74-nested-interrupts)
8. [System Calls](#8-system-calls)
   - [8.1 Software Interrupts via the INT Instruction](#81-software-interrupts-via-the-int-instruction)
   - [8.2 System Call Flow in tiny-itron](#82-system-call-flow-in-tiny-itron)
9. [Memory Layout](#9-memory-layout)
10. [The A20 Line](#10-the-a20-line)
11. [Paging (Identity Mapping)](#11-paging-identity-mapping)
12. [Reference: Source Files and Corresponding Concepts](#12-reference-source-files-and-corresponding-concepts)

---

## 1. Real Mode and Protected Mode

Intel 386 and later CPUs have two main operating modes.

**Real Mode**:
The mode the CPU starts in when powered on. An 8086-compatible 16-bit environment.
- Addresses are calculated as `segment:offset` -> physical address = segment x 16 + offset
- Maximum accessible memory is 1 MB
- No memory protection or task isolation features

**Protected Mode**:
The native 32-bit operating mode of the 386. Used by operating systems.
- Up to 4 GB linear address space
- Memory protection through segmentation
- Access control through privilege levels (Ring 0-3)
- Gate mechanisms via the Interrupt Descriptor Table (IDT)

**Mode transition** (`i386/start.s`):

```
 Real Mode                          Protected Mode
 +-------------------+    CR0.PE=1    +-------------------+
 | 16-bit            | ------------->| 32-bit            |
 | segment:offset    |    far jump    | Memory protection |
 | addressing, 1 MB  |    updates CS  | via GDT/IDT      |
 +-------------------+                +-------------------+
```

In tiny-itron, `start.s` transitions from real mode to protected mode:

```asm
# Set the PE bit (bit 0) of CR0 to 1
movl    %cr0, %eax
orl     $0x00000001, %eax
movl    %eax, %cr0

# Far jump to flush the pipeline and switch CS to the new selector
ljmp    $0x08, $flush_start     # SEL_K16_C (16-bit kernel code)
```

---

## 2. Segmentation

### 2.1 What Is a Segment?

In protected mode, memory is managed by dividing it into segments (logical regions).
When a program accesses memory, the CPU performs the following calculation:

```
Linear address = Segment base address + Offset
```

Each segment has attributes such as "base address," "limit (size)," "type (code/data/stack),"
and "privilege level." These are stored in a segment descriptor.

### 2.2 Segment Descriptors

A segment descriptor is an **8-byte** structure that describes the attributes of a segment.

```
Bit positions:
 63      56 55  52 51  48 47      40 39      32
 +--------+------+------+---------+----------+
 |base_h  |G D 0 |limit | P DPL S | base_m   |
 |(31:24) |  AVL |(19:16)| Type   | (23:16)  |
 +--------+------+------+---------+----------+
 31                    16 15                  0
 +-----------------------+--------------------+
 |   base_l (15:0)       |   limit_l (15:0)   |
 +-----------------------+--------------------+
```

C structure in tiny-itron (`i386/386.h`):
```c
typedef struct seg {
    unsigned short  limit_l;    /* Lower 16 bits of limit */
    unsigned short  base_l;     /* Lower 16 bits of base address */
    unsigned char   base_m;     /* Base address bits 16-23 */
    unsigned char   type;       /* P + DPL + S + Type (access rights) */
    unsigned char   limit_h;    /* G + D/B + upper 4 bits of limit */
    unsigned char   base_h;     /* Base address bits 24-31 */
} seg_t;
```

**Key fields**:

| Field | Meaning |
|-------|---------|
| Base (32 bit) | Starting linear address of the segment (when paging is enabled, translation to a physical address occurs in the paging stage) |
| Limit (20 bit) | Size of the segment (in 4KB units if G=1) |
| P (Present) | 1 = Segment is present in memory |
| DPL | Descriptor Privilege Level (0-3) |
| S | 0 = System segment (TSS, etc.), 1 = Code/Data |
| Type | Segment type (execute/read/write, etc.) |
| G (Granularity) | 0 = Byte granularity, 1 = 4KB page granularity |
| D/B | 0 = 16-bit, 1 = 32-bit |

**Common Type field values** (`i386/386.h`):

| Constant | Value | Meaning |
|----------|-------|---------|
| `ST_CODE` | 0x9A | Code segment (execute+read, P=1, DPL=0) |
| `ST_DATA` | 0x92 | Data segment (read/write, P=1, DPL=0) |
| `ST_STACK` | 0x96 | Stack segment (read/write+expand-down, P=1, DPL=0) |
| `ST_TSS` | 0x89 | TSS descriptor (system segment) |
| `_32BIT` | 0xC0 | D/B=1, G=1 (32-bit, 4KB granularity) |
| `_16BIT` | 0x00 | D/B=0, G=0 (16-bit, byte granularity) |

### 2.3 GDT (Global Descriptor Table)

The GDT is an array of segment descriptors placed at a fixed address in memory.
The CPU knows the location and size of the GDT through the `GDTR` register (GDT Register).

```
GDTR register:
+------------------------+----------------+
|  Base address (32 bit) | Limit (16 bit) |
+------------------------+----------------+

Setting the GDT register (start.s):
lgdt  gdt_ptr        # Load address and size into GDTR

gdt_ptr:
    .word  256-1      # Limit = 255 (256 bytes = 32 entries)
    .word  0x2000, 0  # Base address = 0x2000
```

In tiny-itron, the GDT is placed at physical address **0x2000**.

### 2.4 Selectors

Segment registers (CS, DS, SS, ES, FS, GS) are loaded with selector values.
A selector is a 16-bit value that specifies which GDT entry to reference.

```
15                 3  2  1  0
+------------------+--+-----+
|   Index          |TI| RPL |
|  (GDT entry no.) |  |     |
+------------------+--+-----+

Index: Descriptor number within the GDT (0-8191)
TI:    0 = GDT, 1 = LDT (always 0 in tiny-itron)
RPL:   Requested Privilege Level (0-3)
```

**Examples of selector value calculation**:
- GDT entry 4 (Index=4), TI=0, RPL=0 -> `4 x 8 = 0x20`
- GDT entry 11 (Index=11), TI=0, RPL=3 -> `11 x 8 + 3 = 0x5B`

### 2.5 tiny-itron's GDT Layout

All selectors defined in `i386/addr.h`:

```
GDT (physical address 0x2000)
+------+--------+-------------------------------------------+
|Index |Selector| Purpose                                    |
+------+--------+-------------------------------------------+
|  0   | 0x00   | NULL descriptor (empty entry required by CPU) |
|  1   | 0x08   | 16-bit kernel code (SEL_K16_C)             |
|  2   | 0x10   | 16-bit kernel data (SEL_K16_D)             |
|  3   | 0x18   | 16-bit kernel stack (SEL_K16_S)            |
|  4   | 0x20   | 32-bit kernel code (SEL_K32_C)             |
|  5   | 0x28   | 32-bit kernel data (SEL_K32_D)             |
|  6   | 0x30   | 32-bit kernel stack (SEL_K32_S)            |
|  7   | 0x38   | TSS0 -- for CPU 0 (SEL_TSS0, esp0/ss0 only) |
|  8   | 0x40   | TSS1 -- for CPU 1 (SEL_TSS1, esp0/ss0 only) |
|  9   | 0x48   | (unused)                                   |
| 10   | 0x50   | (unused)                                   |
| 11   | 0x58   | 32-bit user code (SEL_U32_C)               |
| 12   | 0x60   | 32-bit user data (SEL_U32_D)               |
| 13   | 0x68   | 32-bit user stack (SEL_U32_S)              |
| 14   | 0x70   | (unused -- defined as SEL_SYSCALL in addr.h,|
|      |        |  but actual syscalls use IDT vector 0x99)  |
+------+--------+-------------------------------------------+
```

**Important**: Index 1-3 (16-bit segments) are used temporarily by `start.s`
when transitioning from real mode to protected mode. By the time the kernel
proper is running, the 32-bit segments (Index 4-6) are in use.

**Flat model**: The 32-bit segments in tiny-itron (Index 4-6, 11-13) have
their base address set to 0 and their limit set to the maximum value, so
logical addresses effectively equal linear addresses (the so-called flat model).
Segmentation is used primarily for **privilege level separation**.

> **Exception for 16-bit segments**: Index 1-3 are for the real mode to protected mode transition
> and do not have a base address of 0 (`0x08` -> base=0x3000, `0x10` -> base=0xB8000,
> `0x18` -> base=0x3000). These are only used during the transition in `start.s`
> and are not referenced once the 32-bit kernel is running.

---

## 3. Privilege Levels (Ring 0 / Ring 3)

### 3.1 The Four Rings

The i386 supports four privilege levels (Ring 0 through Ring 3).
Lower numbers indicate higher privilege.

```
         +---------------------+
         |      Ring 0         |  Kernel (highest privilege)
         |  +---------------+  |
         |  |   Ring 1      |  |  (unused)
         |  | +-----------+ |  |
         |  | |  Ring 2   | |  |  (unused)
         |  | | +-------+ | |  |
         |  | | |Ring 3 | | |  |  User tasks (lowest privilege)
         |  | | +-------+ | |  |
         |  | +-----------+ |  |
         |  +---------------+  |
         +---------------------+
```

tiny-itron uses only two: Ring 0 (kernel) and Ring 3 (user).

### 3.2 CPL, DPL, and RPL

Three concepts are involved in privilege level checks:

| Abbreviation | Name | Location | Meaning |
|------|------|------|------|
| CPL | Current Privilege Level | Lower 2 bits of CS register | Current execution privilege level |
| DPL | Descriptor Privilege Level | Within the segment descriptor | Privilege level of that segment |
| RPL | Requested Privilege Level | Lower 2 bits of the selector | Privilege level at the time of access request |

**Access check** (for data segments):
When loading a data segment, the CPU checks that `max(CPL, RPL) <= DPL`.
A violation results in a **#GP (General Protection Fault)**.

> **Note:** This is a simplified version of the data segment access rules.
> The actual protection rules differ depending on the segment type.
> For example: loading SS requires `CPL == RPL == DPL`,
> a conforming code segment allows transfer when DPL <= CPL, and so on.
> In tiny-itron, because the flat model is used (all segments span 0-4GB),
> segment-level protection is largely uninvolved.

### 3.3 Privilege Transitions and Stack Switching

When a transition from Ring 3 to Ring 0 occurs (interrupt, syscall),
the CPU automatically performs the following:

1. Reads `ss0` and `esp0` from the TSS and switches to the Ring 0 stack
2. Pushes the old SS, old ESP, EFLAGS, old CS, and old EIP onto the stack

```
Ring 3 -> Ring 0 interrupt stack frame:

    +----------------+  <- TSS ESP0 (initial value before pushes)
    |  Old SS        |  +16  <- Ring 3 stack segment
    |  Old ESP       |  +12  <- Ring 3 stack pointer
    |  EFLAGS        |  +8   <- Flags before interrupt
    |  Old CS        |  +4   <- Code segment before interrupt
    |  Old EIP       |  +0   <- Instruction address before interrupt
    +----------------+  <- New ESP (after 5 pushes)
```

**Important**: On a Ring 0 to Ring 0 interrupt, SS/ESP are NOT pushed
(because no stack switch is needed). Since tiny-itron runs all tasks in Ring 3,
normal task interrupts always push 5 words.

### 3.4 Privilege Levels in tiny-itron

| State | CS | DS | SS | Ring |
|-------|------|------|------|------|
| Kernel mode | 0x20 | 0x28 | 0x30 | 0 |
| User mode | 0x5B | 0x63 | 0x6B | 3 |

**Why 0x5B, 0x63, 0x6B?**:
- `SEL_U32_C` = 0x58 (GDT Index 11). Since it is used in Ring 3, RPL=3 is added -> `0x58 | 3 = 0x5B`
- `SEL_U32_D` = 0x60 (GDT Index 12). Similarly, `0x60 | 3 = 0x63`
- `SEL_U32_S` = 0x68 (GDT Index 13). Similarly, `0x68 | 3 = 0x6B`

**Restrictions in Ring 3**:
- Executing `cli` / `sti` instructions causes a **#GP** (General Protection Fault)
- I/O instructions (`in` / `out`) are controlled by the TSS I/O bitmap
- Spinlocks (`smp_lock`) use the `xchgl` instruction, which is safe in Ring 3

---

## 4. IDT (Interrupt Descriptor Table)

### 4.1 Gate Descriptors

The IDT is an array of **gate descriptors**. Each entry corresponds to an
interrupt vector number and holds the address and attributes of the interrupt handler.

```
Gate descriptor (8 bytes):
 63                48 47      40 39      32
 +------------------+---------+----------+
 |  offset_h        | P DPL   | count    |
 |  (31:16)         |  Type   | (params) |
 +------------------+---------+----------+
 31                16 15                  0
 +------------------+--------------------+
 |  selector        |   offset_l (15:0)  |
 +------------------+--------------------+
```

C structure in tiny-itron (`i386/386.h`):
```c
typedef struct gate {
    unsigned short  offset_l;   /* Lower 16 bits of handler address */
    unsigned short  sel;        /* Code segment selector for the handler */
    unsigned char   count;      /* Parameter count (used with call gates) */
    unsigned char   type;       /* P + DPL + Type */
    unsigned short  offset_h;   /* Upper 16 bits of handler address */
} gate_t;
```

### 4.2 Interrupt Gates and Trap Gates

| Type | Constant | Value | IF Flag | Usage |
|------|----------|-------|---------|-------|
| Interrupt gate | `GT_INTR` | 0x8E | Cleared (interrupts disabled) | IRQ handlers, some CPU exceptions |
| Trap gate | `GT_TRAP` | 0x8F | Unchanged | Most CPU exceptions |

> **Note:** The gate types that can be placed in the IDT are interrupt gates, trap gates,
> and task gates. A call gate (`GT_CALL = 0x8C`) is a system descriptor placed in the GDT/LDT,
> not a type of IDT gate. In tiny-itron, syscalls use a DPL=3 interrupt gate in the IDT
> rather than a call gate (described below).

**Interrupt gate** (GT_INTR = 0x8E):
When entering the handler, the CPU automatically sets `IF=0` (disables interrupts).
Used for hardware interrupt (IRQ) handlers.

**Trap gate** (GT_TRAP = 0x8F):
Does not change the IF flag. Often used for CPU exception handlers.

> In tiny-itron, three exceptions -- #DE (Divide Error, vector 0), #GP (General Protection,
> vector 13), and #PF (Page Fault, vector 14) -- are registered with GT_INTR.
> When these exceptions occur, IF is cleared and interrupts are disabled within the handler.
> The remaining exceptions (#DB, NMI, #BP, #OF, #BR, #UD, #NM, #DF, #TS, #NP, #SS, etc.)
> are registered with GT_TRAP.

**Importance of DPL** (for software interrupts via `INT n`):
- DPL=0 gate -> Only `INT n` from Ring 0 is allowed
- DPL=3 gate -> `INT n` from Ring 3 is also allowed

> **Note:** This DPL check only applies to the `INT n` instruction (software interrupts).
> For hardware-triggered interrupts such as external IRQs or CPU exceptions,
> the handler executes regardless of DPL.

tiny-itron's syscalls use an **interrupt gate in the IDT**, not a call gate in the GDT.
IDT vector 0x99 has `GT_INTR | 0x60 = 0xEE` (a DPL=3 interrupt gate) set,
allowing user tasks to invoke it via `INT 0x99`. Because DPL=3, the `INT` instruction
is permitted from Ring 3, and because it is an interrupt gate, IF is cleared on entry.

### 4.3 tiny-itron's IDT Layout

The IDT is placed at physical address **0x2100**. It has 256 entries.

```
IDT (physical address 0x2100)
+--------+----------------------------------------------+
| Vector | Purpose                                       |
+--------+----------------------------------------------+
|   0    | #DE  Divide Error (division by zero)  [GT_INTR] |
|   1    | #DB  Debug (single step)              [GT_TRAP] |
|   2    | NMI  Non-Maskable Interrupt           [GT_TRAP] |
|   3    | #BP  Breakpoint (INT 3)               [GT_TRAP] |
|   4    | #OF  Overflow                         [GT_TRAP] |
|   5    | #BR  Bound Range Exceeded             [GT_TRAP] |
|   6    | #UD  Invalid Opcode                   [GT_TRAP] |
|   7    | #NM  Device Not Available (FPU)       [GT_TRAP] |
|   8    | #DF  Double Fault                     [GT_TRAP] |
|   9    | Coprocessor Segment Overrun           [GT_TRAP] |
|  10    | #TS  Invalid TSS                      [GT_TRAP] |
|  11    | #NP  Segment Not Present              [GT_TRAP] |
|  12    | #SS  Stack-Segment Fault              [GT_TRAP] |
|  13    | #GP  General Protection Fault         [GT_INTR] |
|  14    | #PF  Page Fault                       [GT_INTR] |
|  15    | Coprocessor Error (intr_copr_error)   [GT_TRAP] |
| 16-31  | (reserved / unregistered)                      |
+--------+----------------------------------------------+
|  0x80  | IRQ0  PIT timer (~17ms, HZ=60)       [GT_INTR] |
|  0x81  | IRQ1  Keyboard                       [GT_INTR] |
|  0x82  | IRQ2  Cascade (slave PIC)             [GT_INTR] |
|  0x83  | IRQ3  COM2                           [GT_INTR] |
|  0x84  | IRQ4  COM1                           [GT_INTR] |
|  0x85  | IRQ5  LPT2                           [GT_INTR] |
|  0x86  | IRQ6  Floppy                         [GT_INTR] |
|  0x87  | IRQ7  LPT1 / Spurious                [GT_INTR] |
|  0x90  | IRQ8  RTC                            [GT_INTR] |
| 0x91-97| IRQ9-15                              [GT_INTR] |
+--------+----------------------------------------------+
|  0x98  | APIC spurious vector                          |
|  0x99  | ITRON system call  [GT_INTR | 0x60, DPL=3]   |
|  0x9A  | APIC timer CPU 0                     [GT_INTR] |
|  0x9B  | APIC timer CPU 1                     [GT_INTR] |
+--------+----------------------------------------------+
| Other  | Default handler (intr_default)       [GT_TRAP] |
+--------+----------------------------------------------+
```

**Initialization code** (`i386/interrupt.c`):
```c
void idt_init(void) {
    /* First, fill all 256 entries with the default handler */
    for (i = 0; i < 256; i++)
        set_idt(i, (unsigned long)intr_default, SEL_K32_C, 0, GT_TRAP);

    setup_trap();       /* Register CPU exceptions (0-15) */
    setup_irq();        /* Register IRQs (0x80-0x97) */
    setup_syscall();    /* Register system call (0x99) */
}
```

> **Note:** APIC-related vectors (0x98 spurious, 0x9A/0x9B timers) are not registered
> in `idt_init()`. They are registered later by `smp_init()`
> (`i386/smp.c`) using `set_idt()`.

---

## 5. TSS (Task State Segment)

### 5.1 TSS Structure

The TSS is a region where the CPU saves task state (registers, stack pointers, etc.).
On the i386, it has a fixed 104-byte structure.

```
TSS structure (104 bytes):
+----------------------------+  Offset
|  prev_link (back link)     |   0
|  esp0 (Ring 0 stack)       |   4
|  ss0  (Ring 0 SS)          |   8
|  esp1, ss1 (Ring 1)        |  12, 16
|  esp2, ss2 (Ring 2)        |  20, 24
|  cr3 (page directory)      |  28
|  eip                       |  32
|  eflags                    |  36
|  eax, ecx, edx, ebx       |  40, 44, 48, 52
|  esp, ebp, esi, edi        |  56, 60, 64, 68
|  es, cs, ss, ds, fs, gs   |  72-96
|  ldt                       |  100
|  I/O bitmap base           |  102
+----------------------------+
```

C structure in tiny-itron (`i386/tss.h`):
```c
typedef struct tss {
    unsigned short  prev_link, dummy0;
    unsigned long   esp0;
    unsigned short  ss0, dummy1;
    unsigned long   esp1;
    unsigned short  ss1, dummy2;
    unsigned long   esp2;
    unsigned short  ss2, dummy3;
    unsigned long   cr3;
    unsigned long   eip, eflags;
    unsigned long   eax, ecx, edx, ebx, esp, ebp, esi, edi;
    unsigned short  es, dummy4, cs, dummy5, ss, dummy6;
    unsigned short  ds, dummy7, fs, dummy8, gs, dummy9;
    unsigned short  ldt, dummy10;
    unsigned short  t, io_base;
} tss_t;
```

### 5.2 Hardware Task Switching (Reference)

When the i386 executes an `ljmp` (far jump) instruction targeting a TSS selector,
the **hardware automatically** saves and restores all registers, performing a task switch.

```
CPU operation (ljmp $SEL_TSS0, $0):
1. Save all current registers to the "old task's" TSS
2. Read the TSS descriptor for SEL_TSS0 from the GDT
3. Load all registers from the TSS structure pointed to by the descriptor
4. Begin execution from the EIP in the TSS
```

> tiny-itron does not use hardware task switching.
> All task transitions, including first-time task startup, are performed via `RESTORE_ALL` + `iret`.

### 5.3 How tiny-itron Uses the TSS

tiny-itron uses the TSS **only to provide esp0/ss0**.

**First task startup (ltr + iret)**

Each CPU loads a TSS selector into the Task Register via `ltr` in
`start_first_task` / `start_second_task`. `ltr` does not save or restore registers;
it simply tells the CPU "use this TSS's esp0/ss0 for Ring 3 to Ring 0 interrupts."

This works because, **before** `start_first_task`, `proc_create` has already built
a fake pt_regs frame on the kernel stack. `proc_create` (`i386/proc.c`) writes
the following frame to the kernel stack when a task is created (`cre_tsk` / `proc_init`):

```
Fake frame built by proc_create (proc.c):

    +----------------------+  <- kern_stack_top
    |  SS   = 0x6B         |  <- SEL_U32_S | 3 (Ring 3 stack)
    |  ESP  = user_esp     |  <- Top of user stack
    |  EFLAGS = 0x200      |  <- INIT_EFLAGS (IF=1: interrupts enabled)
    |  CS   = 0x5B         |  <- SEL_U32_C | 3 (Ring 3 code)
    |  EIP  = task entry   |  <- Address of the task function (e.g., first_task)
    +----------------------+
    |  EAX = 0             |
    |  ECX-EDI = 0         |  <- 9 registers corresponding to SAVE_ALL (all zero)
    |  DS  = 0x63          |  <- SEL_U32_D | 3
    |  ES  = 0x63          |
    +----------------------+
    |  intr_return_restore |  <- Address jumped to by ret
    +----------------------+  <- kern_esp (saved by proc_create)
```

`start_first_task` simply consumes this pre-built stack:

```asm
/* klib.s */
start_first_task:
    movw    $0x38, %ax          /* SEL_TSS0 */
    ltr     %ax                 /* Load into Task Register (no register exchange) */
    movl    current_proc, %ebx  /* current_proc[0] = &proc[1] */
    movl    (%ebx), %esp        /* ESP = proc[1].kern_esp <- bottom of fake frame */
    ret                         /* pop -> jumps to intr_return_restore */
                                /*   -> RESTORE_ALL: pop 9 registers */
                                /*   -> iret: pop EIP/CS/EFLAGS/ESP/SS */
                                /*   -> Execution starts at task entry in Ring 3 */
```

This way, both first-time task startup and normal interrupt return use the same path
(`RESTORE_ALL` + `iret`), eliminating the need for a special startup routine.

**Providing the Ring 0 stack (dynamic esp0 updates)**

The TSS provides the Ring 0 stack (`esp0`, `ss0`) used when an interrupt transitions
from Ring 3 to Ring 0. When an interrupt occurs, the CPU reads `esp0`/`ss0` from the TSS
and switches to the Ring 0 stack before pushing the interrupt frame.

Each task has its own 4KB kernel stack. On a task switch,
`tss_update_esp0()` is called to overwrite TSS.esp0 with the new task's
`kern_stack_top` value. This ensures that the next interrupt switches to the
correct task's kernel stack.

**Task switching**: Both first-time startup and subsequent switches use the same path.
They are managed by `SAVE_ALL`/`RESTORE_ALL` + `intr_leave` (`intr.s`).
`intr_leave` switches ESP to the new task's kernel stack,
and `RESTORE_ALL` + `iret` restores the new task's registers.

---

## 6. PIC (i8259 Interrupt Controller)

### 6.1 Role of the PIC

The PIC (Programmable Interrupt Controller) receives interrupt signals from
external devices and notifies the CPU. The i8259 is the standard PIC for
IBM PC compatibles.

```
 Devices                     PIC                    CPU
 +-----------+            +----------+
 | Timer     |-- IRQ0 --->|          |
 | Keyboard  |-- IRQ1 --->|  Master  |--- INT --->  CPU
 | ...       |-- IRQ3~7 ->|  i8259   |
 |           |            +----+-----+
 |           |                 ^ Cascade (IRQ2)
 |           |            +----+-----+
 | RTC       |-- IRQ8 --->|  Slave   |
 | ...       |-- IRQ9~15->|  i8259   |
 +-----------+            +----------+
```

### 6.2 Master-Slave Cascade Connection

In PC/AT compatibles, two i8259 chips are cascade-connected:
- **Master PIC**: Manages IRQ0-7. I/O ports 0x20/0x21
- **Slave PIC**: Manages IRQ8-15. I/O ports 0xA0/0xA1
- The slave's output is connected to the master's IRQ2

This provides a total of 15 interrupt lines (IRQ0-1, IRQ3-15).

### 6.3 Initialization Sequence (ICW1-ICW4)

The PIC is initialized with four **ICWs** (Initialization Command Words).
tiny-itron's initialization code (`i386/i8259.c`):

```c
/* -- Master PIC (I/O ports 0x20/0x21) -- */
outb(0x20, 0x11);       /* ICW1: edge-triggered, ICW4 needed */
outb(0x21, 0x80);       /* ICW2: Map IRQ0 -> vector 0x80 */
outb(0x21, 0x04);       /* ICW3: Slave connected to IRQ2 */
outb(0x21, 0x0D);       /* ICW4: buffered mode (master) */

/* -- Slave PIC (I/O ports 0xA0/0xA1) -- */
outb(0xA0, 0x11);       /* ICW1: edge-triggered, ICW4 needed */
outb(0xA1, 0x90);       /* ICW2: Map IRQ8 -> vector 0x90 */
outb(0xA1, 0x02);       /* ICW3: Connected to master's IRQ2 */
outb(0xA1, 0x09);       /* ICW4: buffered mode (slave) */
```

**ICW2 (vector mapping)** is the most important.
The PIC converts device IRQs into CPU interrupt vectors:

| IRQ | Device | Vector | ICW2 setting |
|-----|--------|--------|-------------|
| IRQ0 | PIT timer | 0x80 | Master ICW2 = 0x80 |
| IRQ1 | Keyboard | 0x81 | (0x80 + 1) |
| IRQ6 | Floppy | 0x86 | (0x80 + 6) |
| IRQ8 | RTC | 0x90 | Slave ICW2 = 0x90 |

**Why start at 0x80?**: Vectors 0-31 are reserved for CPU exceptions.
To avoid conflicts, IRQ vectors are remapped to 0x80 and above.
(Linux uses 0x20-0x2F, but tiny-itron chose 0x80.)

### 6.4 IRQ Masking and EOI

**IRQ masking (OCW1)**:
After initialization, all IRQs are masked (disabled), then only the needed IRQs are individually enabled:

```c
outb(0x21, 0xFF);   /* Master: mask all IRQs */
outb(0xA1, 0xFF);   /* Slave: mask all IRQs */

/* Unmask individually later */
irq_mask_off(0x01);  /* Enable IRQ0 (timer): clear bit 0 */
irq_mask_off(0x02);  /* Enable IRQ1 (keyboard) */
```

**EOI (End of Interrupt)**:
After the interrupt handler finishes processing, it sends an EOI to the PIC
to signal "you may accept the next interrupt":

```c
void i8259_reenable(void) {
    outb(0x20, 0x20);   /* Send EOI to master */
    outb(0xA0, 0x20);   /* Send EOI to slave */
    smp_eoi();           /* Also send EOI to APIC */
}
```

---

## 7. Interrupt Flow: SAVE_ALL / RESTORE_ALL

### 7.1 What the CPU Does When an Interrupt Occurs

When an IRQ fires while a Ring 3 user task is executing, the CPU performs the following:

```
1. Clear IF (for interrupt gates)
2. Read ss0/esp0 from TSS, switch to Ring 0 stack (task's kernel stack)
3. Push SS, ESP, EFLAGS, CS, EIP onto the stack
4. Look up handler address from IDT
5. Jump to handler

Kernel stack (growing down from TSS.esp0):
                    +----------------+
                    |  Old SS        |  esp+16
                    |  Old ESP       |  esp+12
                    |  EFLAGS        |  esp+8
                    |  Old CS        |  esp+4
                    |  Old EIP       |  esp+0   <- ESP (Ring 0)
                    +----------------+
```

Each task has a dedicated 4KB kernel stack. On task switches,
`tss_update_esp0()` overwrites TSS.esp0 with the new task's `kern_stack_top` value,
ensuring the next interrupt switches to the correct task's kernel stack.

### 7.2 How SAVE_ALL Works

The `SAVE_ALL` macro (`i386/intr.s`) is expanded at the beginning of every interrupt handler,
pushing all general-purpose registers and segment registers onto the kernel stack.

By adding 9 registers below the interrupt frame (EIP, CS, EFLAGS, ESP, SS) pushed by the CPU,
a complete `pt_regs` frame is formed:

```
pt_regs frame (on kernel stack):

              +----------+
        0x34  |  SS      |  --+
        0x30  |  ESP     |    |  CPU pushes
        0x2C  |  EFLAGS  |    |  (Ring 3 -> Ring 0)
        0x28  |  CS      |    |
        0x24  |  EIP     |  --+
              +----------+
        0x20  |  EAX     |  --+
        0x1C  |  ECX     |    |
        0x18  |  EDX     |    |
        0x14  |  EBX     |    |  SAVE_ALL pushes
        0x10  |  EBP     |    |
        0x0C  |  ESI     |    |
        0x08  |  EDI     |    |
        0x04  |  DS      |    |
        0x00  |  ES      |  --+  <- ESP after SAVE_ALL
              +----------+
```

**What SAVE_ALL does**:

```asm
.macro SAVE_ALL
    pushl   %eax            # Push 9 registers
    pushl   %ecx
    pushl   %edx
    pushl   %ebx
    pushl   %ebp
    pushl   %esi
    pushl   %edi
    pushl   %ds
    pushl   %es
    movw    $0x28, %ax      # Reload DS/ES with the kernel data segment
    movw    %ax, %ds        # (The CPU auto-switches CS/SS but
    movw    %ax, %es        #  does not change DS/ES)
.endm
```

**`intr_enter` after SAVE_ALL**: Determines the CPU number from the APIC ID
and increments `k_nest` (the interrupt nesting counter).

### 7.3 How RESTORE_ALL and intr_leave Work

After the interrupt handler's C function returns, execution flows through the
common return path `intr_return`:

```asm
intr_return:
    call    intr_leave          # (1) Nesting management + task switch
intr_return_restore:
    RESTORE_ALL                 # (2) Restore registers
    iret                        # (3) Return to Ring 3
```

**What intr_leave does**:

```
intr_leave:
  1. Read APIC ID to determine CPU number
  2. Decrement k_nest
  3. If k_nest > 0 (still nested), just ret
  4. If k_nest == 0 (outermost interrupt):
     a. current_proc[cpu]->kern_esp = ESP   (save current task's ESP)
     b. sched_next_tsk_check(cpu)           (task switch decision)
        -> If needed, current_proc[cpu] is changed to the new task
     c. ESP = current_proc[cpu]->kern_esp   (load new task's ESP)
     d. tss_update_esp0(cpu, kern_stack_top) (overwrite TSS.esp0 with new task's kern_stack_top)
  5. ret -> returns to intr_return_restore
```

**How task switching works**:

Task switching is achieved solely by swapping ESP. Since each task's kernel stack
contains that task's `pt_regs` frame:
- When `intr_leave` switches ESP to the new task's `kern_esp`
- `RESTORE_ALL` pops registers from the new task's `pt_regs`
- `iret` jumps to the new task's EIP/ESP/CS/SS

```
Old task's kernel stack:           New task's kernel stack:
  +-------------+                    +-------------+
  |  pt_regs    |                    |  pt_regs    |
  | (old task's |   ESP moves to     | (new task's |
  |  registers) |  ===============>  |  registers) |
  +-------------+                    +-------------+
                                         ^
                                         |
                                  RESTORE_ALL + iret
                                  pop from here
```

**What RESTORE_ALL does**:

```asm
.macro RESTORE_ALL
    popl    %es             # Pop 9 registers in reverse order of SAVE_ALL
    popl    %ds
    popl    %edi
    popl    %esi
    popl    %ebp
    popl    %ebx
    popl    %edx
    popl    %ecx
    popl    %eax
.endm
```

### 7.4 Nested Interrupts

`k_nest0`/`k_nest1` are per-CPU interrupt nesting counters,
managed by `intr_enter`/`intr_leave`.

```
Example: #PF (Page Fault) occurs during IRQ0 processing
k_nest = 0 (user execution)
  -> IRQ0: SAVE_ALL + intr_enter (k_nest=1)
      -> #PF: SAVE_ALL + intr_enter (k_nest=2)
      <- #PF: intr_leave (k_nest=1) -- no scheduling decision
              RESTORE_ALL + iret -> return to IRQ0 handler
  <- IRQ0: intr_leave (k_nest=0) -- scheduling decision + ESP swap here
          RESTORE_ALL + iret -> return to user task
```

> **Note**: In tiny-itron, all IRQ handlers are registered with interrupt gates (GT_INTR),
> so IF is cleared on handler entry. Since `sti` is never executed before the return path,
> **normal IRQs never nest with each other**.
>
> Nesting only occurs with NMI (Non-Maskable Interrupt) or CPU exceptions.
> The IF flag only suppresses maskable external interrupts (IRQs);
> CPU exceptions (#PF, #GP, #DE, etc.) occur synchronously as a result of
> the CPU executing an instruction, so they can occur even when IF=0.
> The example above shows a case where code in the IRQ0 handler causes a page fault.

The scheduling decision is made **only when k_nest returns to 0 (returning from the outermost interrupt)**.
This prevents inconsistencies from task switching in the middle of nested interrupt handling.

When interrupts nest, multiple interrupt frames are stacked on the kernel stack.
However, the outer frame (Ring 3 to Ring 0) forms a 14-word `pt_regs` frame
where the CPU pushes SS/ESP, while the inner frame (Ring 0 to Ring 0) forms a shorter
12-word frame since the CPU does not push SS/ESP:

```
Kernel stack (during nesting):
  +---------------------+  kern_stack_top (= TSS.esp0)
  |  SS, ESP, EFLAGS,   |
  |  CS, EIP (outer)    |  CPU pushes (Ring 3 -> Ring 0)
  |  SAVE_ALL (outer)   |  9 registers
  +---------------------+
  |  EFLAGS, CS, EIP    |  CPU pushes (Ring 0 -> Ring 0, no SS/ESP)
  |  SAVE_ALL (inner)   |  9 registers
  |  C function frame   |
  +---------------------+  <- ESP
```

---

## 8. System Calls

### 8.1 Software Interrupts via the INT Instruction

On the i386, the `INT n` instruction can generate a software interrupt.
The CPU follows the same procedure as for hardware interrupts: it looks up the IDT
and jumps to the handler.

tiny-itron uses `INT 0x99` for system calls:

```c
/* klib.s -- syscall wrapper called from user space */
syscall:
    pushl   %ebp
    movl    %esp, %ebp
    int     $0x99           /* Vector 0x99 -> IDT -> intr_syscall */
    popl    %ebp
    ret
```

### 8.2 System Call Flow in tiny-itron

```
User task                    Kernel
     |
     |  syscall(sysid, arg1, arg2, ...)
     |  -> int $0x99
     |                           |
     +---- Ring 3 -> Ring 0 ---->|
     |  (CPU switches to kernel  |
     |   stack: TSS.esp0)        |
     |                           |
     |                     intr_syscall:
     |                       SAVE_ALL        <- Push registers (build pt_regs)
     |                       intr_enter      <- k_nest++
     |                       push %esp       <- pt_regs* as argument
     |                       call c_intr_syscall(regs)
     |                           |
     |                       Read user stack via regs->esp
     |                       itron_syscall(apic, sysid, args...)
     |                           |
     |                       regs->eax = ret <- Write return value to EAX slot
     |                           |
     |                       jmp intr_return
     |                         intr_leave    <- k_nest--, task switch decision
     |                         RESTORE_ALL   <- Pop registers (EAX=return value)
     |                         iret          <- Return to Ring 3
     |                           |
     <---- Ring 0 -> Ring 3 ----|
     |
     |  Return value is in EAX
```

**How arguments are passed**:
The user-space `syscall()` pushes arguments onto the stack and calls `INT 0x99`.
`intr_syscall` (assembly) builds the pt_regs frame with `SAVE_ALL`,
then calls `c_intr_syscall` with ESP (= pt_regs pointer) as the argument.
The C function reads the user stack via `regs->esp` to retrieve the arguments.

```asm
/* intr_syscall (intr.s) */
intr_syscall:
    SAVE_ALL                        /* Push all registers -> build pt_regs */
    call    intr_enter              /* k_nest++ */
    pushl   %esp                    /* arg: pt_regs* */
    call    c_intr_syscall          /* C handler */
    addl    $4, %esp                /* Argument cleanup */
    jmp     intr_return             /* intr_leave + RESTORE_ALL + iret */
```

**Writing the return value** (`i386/syscall.c`):
```c
/* Write directly to the EAX slot of pt_regs.
 * RESTORE_ALL pops EAX, so the task receives the return value. */
regs->eax = ret;
```

---

## 9. Memory Layout

tiny-itron enables paging, but all pages are **identity-mapped** (VA=PA),
so linear addresses and physical addresses always match.
The purpose of paging is solely for memory protection via the U/S bit
(see section 11 for details).

```
0x00000000 +-------------------------------------------+
           | (base of physical memory)                 |
0x00002000 | GDT (segment table)                       |
0x00002100 | IDT (interrupt table)                     |
0x00003000 | start.s (second-stage loader, 1KB)        |
0x00003400 | run.s + kernel code (.text + .data)       |
           |    ... kernel .bss                        |
0x00010000 | FDC DMA buffer                            |
           |                                           |
~0x01F000  | .user_text + .user_data                   |  <- page-aligned
~0x021000  | Kernel memory pool (_user_data_end)       |  <- kmem_alloc region
           |    (DTQ/MBF/MPF buffers, etc.)            |     Supervisor-only pages
0x00110000 | User memory pool (MEM_START)              |  <- 1 MB + 64 KB
           |    (dynamic memory allocation)            |
0x006FFFFF | (MEM_END)                                 |
0x00700000 | User stack pool (STACK_START)             |
           |    (allocated by cre_tsk)                 |
0x0074FFFF | (STACK_END)                               |
0x00750000 | Per-task kernel stacks                    |  <- KERN_STACK_BASE
           |   (4KB each, Task 1-16 = 16 stacks)      |  top = BASE + (N+1)*4096
0x00761000 | (end of kernel stack area)                |
           |                                           |
0x00770000 | CPU 1: boot stack (CPU1_SP)               |  <- used only during main()
           |                                           |
0x007A0000 | CPU 0: boot stack (CPU0_SP)               |  <- used only during main()
           |                                           |
0x000B8000 | VGA text buffer (80x25x2 bytes)           |
           |                                           |
0xFEE00000 | Local APIC registers (MMIO)               |
           |                                           |
0xFFFFFFFF +-------------------------------------------+
```

**Stacks** on x86 grow downward (from high addresses to low addresses), so
`CPU0_SP = 0x7A0000` is the **top** of the initial stack (the first push starts here).

**Initial stacks** (`CPU0_SP`, `CPU1_SP`) are used only during `main()` execution.
After task startup, execution switches to **per-task kernel stacks**, and the initial stacks are no longer used.

```
Per-task kernel stacks (KERN_STACK_BASE = 0x750000):

0x750000 +----------------+ Task 0 base (unused)
         |  ^ grows down  | 4KB
0x751000 +----------------+ Task 1 kern_stack_top
         |  ^ grows down  | 4KB
0x752000 +----------------+ Task 2 kern_stack_top
         |  ...           |
0x761000 +----------------+ Task 16 end

top of each task = KERN_STACK_BASE + (tskid + 1) * 4096
TSS.esp0 is overwritten with new task's kern_stack_top on each task switch
```

---

## 10. The A20 Line

**The A20 line problem**: The 8086 had 20 address bus lines (A0-A19),
capable of addressing up to 1 MB (2^20). Addresses exceeding 1 MB wrapped around
to the vicinity of address 0 (wraparound).

The 80286 and later have 24 or more address bus lines, but for compatibility,
**the A20 line (the 21st address line) is disabled by default**.
To access memory above 1 MB in protected mode, A20 must be enabled.

Enabling A20 in tiny-itron (`i386/start.s`):
```asm
a20_init:
    call    a20_init_wait
    movb    $0xD1, %al          /* Write command to keyboard controller */
    movw    $0x64, %dx
    outb    %al, %dx
    call    a20_init_wait
    movb    $0xDF, %al          /* Set A20 enable bit */
    movw    $0x60, %dx
    outb    %al, %dx
    call    a20_init_wait
    ret
```

This is the traditional method of enabling A20 via the keyboard controller (i8042).
Historically, the keyboard controller was responsible for gating the A20 line.

---

## 11. Paging (Identity Mapping)

In addition to segmentation, the i386 also has a paging mechanism.
When paging is enabled, linear addresses are further translated to physical addresses:

```
Logical address  ->  Segmentation  ->  Linear address  ->  Paging  ->  Physical address
(selector:offset)
```

tiny-itron **enables paging** (CR0 PG bit = 1).
However, all pages are configured as **identity mappings** (VA = PA),
so virtual addresses and physical addresses always match.

### Purpose: Access Control via the U/S Bit

The main purpose of paging is not virtual address translation but rather access control
using the **U/S (User/Supervisor) bit** in the page tables:

| Address Range | U/S | Access Rights | Contents |
|---|---|---|---|
| 0x00000 - `_user_text_start` | Supervisor | Ring 0 only | Kernel .text/.data/.bss, GDT/IDT |
| `_user_text_start` - `_user_data_end` | User | Accessible from Ring 3 | .user_text + .user_data |
| `_user_data_end` - 0x10FFFF | Supervisor | Ring 0 only | Gap (includes VGA buffer at 0xB8000) |
| 0x110000 - 0x74FFFF | User | Accessible from Ring 3 | Memory pool + stack pool |
| 0x750000 - 0x7FFFFF | Supervisor | Ring 0 only | Per-task kernel stacks + CPU boot stacks |

`_user_text_start` and `_user_data_end` are linker symbols defined in `kernel.ld`,
whose values change as the kernel size changes. `page_init()` in `page.c` reads
these addresses at runtime and sets the U/S bits in the page tables.
Kernel code (0x3400-) resides in Supervisor pages and cannot be directly accessed from Ring 3.
User tasks use syscall wrappers in `.user_text` to transition to Ring 0 via `int $0x99`
and call kernel functions.

### Page Table Structure

```
page_dir[1024]          Page directory (pointed to by CR3)
  +-- [0] -> page_table[0][1024]    0x000000 - 0x3FFFFF (4MB)
  +-- [1] -> page_table[1][1024]    0x400000 - 0x7FFFFF (4MB)
  +-- [0x3FB] -> page_table_apic[1024]  0xFEC00000 - 0xFEFFFFFF
                                         (APIC register region, PCD=1)
```

`page_table[0]` and `page_table[1]` together cover 8MB.
Pages at 0x750000 and above are set to Supervisor.
The APIC region (0xFEE00000) is needed because `intr_enter`/`intr_leave` read the APIC ID,
and the PCD (Page Cache Disable) bit is set to disable caching.

### Initialization and Enabling

- `page_init()`: Called by the BSP immediately after `all_init()`. Builds the page tables.
- `page_enable()`: Loads the page directory address into CR3 and sets CR0.PG to 1.
  Called by both BSP and AP (the AP shares the page tables built by the BSP).

---

## 12. Reference: Source Files and Corresponding Concepts

| Source File | Related Concepts |
|-------------|-----------------|
| `i386/start.s` | Real mode to protected mode transition, A20, GDTR/IDTR loading |
| `i386/run.s` | 32-bit entry, CPU identification, kernel stack setup |
| `i386/386.c`, `386.h` | GDT descriptor setup (`set_gdt`), gate setup (`set_gate`) |
| `i386/addr.h` | Selector constants, memory address constants, stack addresses |
| `i386/tss.c`, `tss.h` | TSS structure, initialization (esp0/ss0 only), dynamic esp0 updates |
| `i386/interrupt.c`, `interrupt.h` | IDT initialization, IRQ/exception/syscall handler registration |
| `i386/interruptP.h` | IDT pointer, interrupt table |
| `i386/intr.s` | SAVE_ALL/RESTORE_ALL (context save/restore), all interrupt entries |
| `i386/i8259.c` | PIC (i8259) initialization, IRQ masking, EOI |
| `i386/klib.s` | I/O instruction wrappers, `cli`/`sti`, `ltr` task startup, `syscall` |
| `i386/syscall.c` | System call dispatch, return value writing |
| `i386/smp.c`, `smpP.h` | Local APIC initialization, APIC timer, IPI, CPU identification |
| `i386/proc.c`, `proc.h` | Process structure, register save area, CPU affinity |
| `i386/io.h` | I/O port address constants |
