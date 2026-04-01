# VGA Text Mode Guide

This document explains how VGA text mode works and how tiny-itron implements
screen output, starting from the hardware level.

---

## 1. Overview

VGA text mode is a video card mode in which the hardware automatically renders
data in VRAM as characters on screen. Programs do not need to call any special
drawing API -- simply writing values to memory causes characters to appear.

This is possible because the VGA controller scans the VRAM contents at a fixed
interval (the vertical refresh, typically 60 Hz) and converts each byte pair
into a pixel pattern based on the font ROM and attribute information, then
outputs it to the display. From the CPU's perspective, VRAM can be accessed
just like ordinary memory, and whatever is written will be reflected on screen
at the next refresh.

---

## 2. VGA Memory Map

The text mode VRAM begins at physical address `0xB8000`.
To represent an 80-column x 25-row screen, each character requires 2 bytes
(character code + attribute), for a total of 80 x 25 x 2 = 4000 bytes.

```
i386/videoP.h:
    #define G_BASE    0xb8000      /* Physical address of VRAM start */
    #define G_WIDTH   (80 * 2)     /* Bytes per row = 160 */
    #define G_HEIGHT  26           /* Number of rows including scroll buffer */
    #define G_TOTAL   G_WIDTH * G_HEIGHT  /* = 4160 bytes */
```

G_HEIGHT is 26 rather than 25 to provide one extra row of buffer space during
hardware scrolling (discussed in detail in Section 6).

### Memory Layout

Each character is represented by 2 bytes:

```
Address: 0xB8000 + (row * 160) + (col * 2)

  Byte 0: Character code (ASCII)
  Byte 1: Attribute byte

  +------+------+------+------+- - - - -+------+------+
  | 'H'  | attr | 'e'  | attr |         | ' '  | attr |
  +------+------+------+------+- - - - -+------+------+
  B8000  B8001  B8002  B8003           B8098  B8099
  <- col 0 ->   <- col 1 ->           <- col 79 ->

  Row  0: 0xB8000 .. 0xB809F  (160 bytes)
  Row  1: 0xB80A0 .. 0xB813F  (160 bytes)
   ...
  Row 24: 0xB8F00 .. 0xB8F9F (160 bytes)
```

---

## 3. Attribute Byte

The attribute byte next to each character specifies the foreground and
background colors using a bit field.

```
  bit  7    6  5  4    3    2  1  0
  +------+----------+------+----------+
  |Blink | BG  RGB  | High | FG  RGB  |
  +------+----------+------+----------+
    [7]   [6:4]      [3]    [2:0]

  Blink = 1: Character blinks (on many emulators this gives a high-intensity background)
  BG RGB:    Background color (0-7)
  High:      Makes the foreground color brighter
  FG RGB:    Foreground color (0-7)
```

### Color Table

| Value | Color   | Value | Color (High Intensity) |
|-------|---------|-------|------------------------|
| 0x0   | Black   | 0x8   | Dark gray              |
| 0x1   | Blue    | 0x9   | Light blue             |
| 0x2   | Green   | 0xA   | Light green            |
| 0x3   | Cyan    | 0xB   | Light cyan             |
| 0x4   | Red     | 0xC   | Light red              |
| 0x5   | Magenta | 0xD   | Light magenta          |
| 0x6   | Brown   | 0xE   | Yellow                 |
| 0x7   | Gray    | 0xF   | White                  |

### Attribute Values Used in tiny-itron

```
i386/videoP.h:
    #define G_ATTR  0x02    /* printk default: green on black */

kernel/user.c:
    #define ATTR_WHITE    0x0F    /* White on black (high intensity) */
    #define ATTR_GREEN    0x0A    /* Light green on black */
    #define ATTR_CYAN     0x0B    /* Light cyan on black */
    #define ATTR_YELLOW   0x0E    /* Yellow on black */
    #define ATTR_GREY     0x07    /* Gray on black */
    #define ATTR_DARK     0x08    /* Dark gray on black */
    #define ATTR_MAGENTA  0x0D    /* Light magenta on black */
    #define ATTR_RED      0x0C    /* Light red on black */
```

All of these have a background of 0 (black) and specify only the foreground
color. For example, `0x0F` breaks down as:
- Blink=0, BG=000 (black), High=1, FG=111 (white) -- white text on a black background

---

## 4. Initialization (`video_init`)

`video_init()` initializes the 6845 CRT controller.

```c
/* i386/video.c */
void video_init(void)
{
    c_x = c_y = 0;         /* Reset cursor position */
    c_y_max = 24;
    scrolltop = 0;          /* Scroll offset = 0 */
    video_set_6845(G_VID_ORG, scrolltop);  /* display start address = 0 */
}
```

### I/O Port Access to the 6845 CRT Controller

The 6845 uses an index register scheme for access. You write the register
number to port `0x3D4`, then read or write data via port `0x3D5`.

```
i386/io.h:
    #define IO_6845    0x3d4    /* Index register */
    #define IO_6845_V  0x3da    /* Status register (for vertical retrace detection) */
```

`video_set_6845()` splits a 16-bit value and writes it across two registers
(high/low byte):

```c
static void video_set_6845(unsigned short reg, unsigned short val)
{
    video_wait();                              /* Wait for vertical retrace */
    outb(IO_6845, reg & 0xff);                /* Register number (high byte) */
    outb(IO_6845 + 1, (val >> 8) & 0xff);    /* Value high byte */
    outb(IO_6845, (reg + 1) & 0xff);         /* Register number (low byte) */
    outb(IO_6845 + 1, val & 0xff);           /* Value low byte */
}
```

Registers used:

| Constant  | Register Nos. | Name                | Purpose                              |
|-----------|---------------|---------------------|--------------------------------------|
| G_VID_ORG | 12, 13        | Start Address H/L   | VRAM display start position (in characters) |
| G_CURSOR  | 14, 15        | Cursor Location H/L | Cursor position                      |

---

## 5. Character Output Mechanism

### `video_putc` -- Single Character Output

```c
/* i386/video.c */
void video_putc(char c)
{
    unsigned char *p = (unsigned char *)G_BASE;
    p += 2 * scrolltop + c_y * 160 + c_x * 2;
    /* Address calculation:
     *   G_BASE (0xB8000)
     * + 2 * scrolltop  (scroll offset, in bytes)
     * + c_y * 160       (row: 1 row = 80 chars x 2 bytes)
     * + c_x * 2         (column: 1 char = 2 bytes)
     */

    if (c == '\n') {
        c_y++; c_x = 0;           /* Newline: move to start of next row */
    } else {
        video_wait();
        *p = c;                    /* Write character code */
        *(p + 1) = G_ATTR;        /* Write attribute byte (0x02, green) */
        c_x++;
        if (c_x >= 80) {          /* Wrap at end of row */
            c_x = 0;
            c_y++;
        }
    }
    if (c_y > 24) {
        video_scroll();            /* Scroll when past bottom of screen */
        c_y = 24;
    }
}
```

Key points:
- `scrolltop` holds the current scroll position in **character units**.
  When computing the VRAM address, it is converted to byte units via
  `2 * scrolltop`
- The attribute is always `G_ATTR` (0x02, green). `printk` provides no
  mechanism for specifying colors

### `video_puts` -- String Output

A simple implementation that calls `video_putc` repeatedly:

```c
void video_puts(char *p)
{
    while (*p != '\0')
        video_putc(*p++);
}
```

---

## 6. Hardware Scrolling

VGA text mode provides a hardware feature called "changing the display start
address". By modifying the 6845 Start Address register (registers 12-13), you
can control where in VRAM the display begins. This achieves scrolling without
copying memory.

### How `video_scroll` Works

```c
static void video_scroll(void)
{
    scrolltop += 80;    /* Advance display start by 1 row = 80 characters */
    c_y_max++;
    video_set_6845(G_VID_ORG, scrolltop);  /* Write new start position to 6845 */
    video_clear_line();                     /* Clear the new bottom row */

    if (scrolltop > 80 * 25) {
        /* Reached end of VRAM -- wrap around */
        video_copy(G_TOTAL + G_BASE, G_BASE, G_TOTAL);  /* Copy VRAM */
        c_y = 24;
        c_y_max = 24;
        scrolltop = 0;
        video_set_6845(G_VID_ORG, scrolltop);  /* Reset to start */
        video_clear_line();
    }
}
```

Normal scrolling (when `scrolltop` is within VRAM range):
```
  Before scroll:             After scroll:
  scrolltop = 0              scrolltop = 80

  VRAM:                      VRAM:
  +----------------+         +----------------+
  | Row 0 (visible)| <- disp | Row 0 (hidden) |
  | Row 1 (visible)|         | Row 1 (visible)| <- display start
  |   ...          |         |   ...          |
  | Row 24 (visible)|        | Row 24 (visible)|
  |                |         | Row 25 (blank) | <- new bottom row
  +----------------+         +----------------+
```

G_HEIGHT is 26 (25 + 1) to provide this one extra row of headroom during
scrolling. When `scrolltop > 80 * 25` (exceeding 2000 characters), the buffer
end is reached, so the entire VRAM is copied back to the beginning and the
position is reset.

---

## 7. printk (Ring 0 Only)

`printk` is a printf-style formatted output function in C.

### Format Specifiers

| Specifier | Type           | Output            |
|-----------|----------------|-------------------|
| `%s`      | `char*`        | String            |
| `%x`      | `unsigned int` | Hexadecimal       |
| `%d`      | `unsigned int` | Decimal           |
| `%c`      | `char`         | Single character  |

### SMP Spinlock

In a multi-CPU environment, two CPUs may call `printk` simultaneously.
If the cursor position (`c_x`, `c_y`) is accessed concurrently during
character output, the display will be corrupted. To prevent this, an
`xchgl`-based spinlock (`video_lk`) provides mutual exclusion.

```c
/* i386/video.c */
static unsigned long video_lk = 0;

void printk(char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    smp_lock(&video_lk);       /* Acquire spinlock */
    /* ... format processing ... */
    smp_unlock(&video_lk);     /* Release spinlock */
}
```

**Why use a separate `video_lk` instead of `kernel_lk`?**
ISR handlers (such as `c_intr_irq0`) are called while already holding
`kernel_lk`. If `printk` also tried to acquire `kernel_lk`, calling `printk`
from within an ISR would cause an **immediate deadlock on the same CPU**
(since `xchgl` spinlocks are non-reentrant). For this reason, VGA output
uses a separate lock variable, `video_lk`.

In the current runtime code, there are no places where `printk` is called
from within an ISR (screen updates use lock-free functions like
`vga_write_dec_at`). However, it is common to insert `printk` calls inside
ISRs during debugging, so the separate lock is maintained as a safety measure.

### Custom va_list Implementation

Since the standard library is not available, `va_list` is implemented from
scratch:

```c
/* i386/videoP.h */
typedef char*  va_list;
#define va_start(ap, param)  (ap = (char*)&param + sizeof(param))
#define va_arg(ap, type)     ((type*)(ap += sizeof(type)))[-1]
#define va_end(ap)
```

This relies on the x86 cdecl calling convention (arguments are pushed onto
the stack from right to left) and reads variadic arguments sequentially
starting from the address immediately after the last fixed parameter.

### Why printk Cannot Be Called from Ring 3

The call chain `printk` -> `video_putc` -> `video_scroll` -> `video_set_6845`
-> `outb` ultimately executes an I/O port instruction (`outb`).
On i386, I/O port instructions can only be executed when the IOPL (I/O
Privilege Level) is greater than or equal to the CPL (Current Privilege
Level). Since this kernel sets IOPL=0, executing `outb` from Ring 3 (CPL=3)
triggers a #GP (General Protection Fault).

---

## 8. VGA Access from Ring 3

To display output from a user task (Ring 3), two barriers must be overcome:

1. **I/O port restriction**: `outb`/`inb` can only be executed when
   IOPL >= CPL. Since IOPL=0, attempting these from Ring 3 (CPL=3)
   triggers a **#GP** (General Protection Fault)
2. **Page protection**: VGA VRAM (0xB8000) is marked as Supervisor (U/S=0)
   in the page table. A memory access from Ring 3 triggers a **#PF**
   (Page Fault)

### vga_write_at -- Direct Write Function Without I/O Port Access

`vga_write_at()` and `vga_write_dec_at()` are designed to write directly to
VRAM without using any I/O port instructions. Since they do not operate on
the 6845 registers (scrolling, cursor movement), they avoid barrier 1 (#GP)
described above.

```c
/* i386/video.c */
void vga_write_at(int row, int col, char *s, unsigned char attr)
{
    unsigned short *p = (unsigned short *)G_BASE;
    p += row * 80 + col;       /* Calculate address at fixed position */
    while (*s && col < 80) {
        *p++ = (unsigned short)attr << 8 | (unsigned char)*s++;
        col++;
    }
}
```

However, these functions do not scroll. They overwrite fixed positions on
screen, so they cannot provide stream-style output like printk.

**Can `vga_write_at()` be called directly from Ring 3?** -- No, it cannot.
While barrier 1 (#GP) is avoided, barrier 2 (#PF) remains. `vga_write_at()`
writes to 0xB8000 (a Supervisor page), so executing it from Ring 3 results
in a page fault. `vga_write_at()` is a function intended to be called from
the kernel (Ring 0).

### Solution: Access via syscall Wrappers

User tasks transition to the kernel (Ring 0) via a syscall and have
`vga_write_at()` executed in Ring 0. This avoids both barrier 1 (#GP) and
barrier 2 (#PF):

```
User task (Ring 3)              Kernel (Ring 0)
----------------------------    -------------------------
print_at(row, col, s, attr)
  -> syscall(-TFN_EXD_VGA_WRITE, ...)
    -> int $0x99                -> c_intr_syscall()
                                  -> sys_vga_write_at()
                                    -> vga_write_at()
                                      -> VRAM write
                                <- iret
  <- return value
```

### User-Side Wrappers (lib/lib_exd.c)

```c
void print_at(int row, int col, char *s, unsigned char attr)
{
    syscall(-TFN_EXD_VGA_WRITE, row, col, s, attr);
}

void print_dec_at(int row, int col, unsigned long n, int width,
                  unsigned char attr)
{
    syscall(-TFN_EXD_VGA_DEC, row, col, n, width, attr);
}

void clear_screen(void)
{
    syscall(-TFN_EXD_VGA_CLEAR);
}

void fill_at(int row, int col, int len, int ch, unsigned char attr)
{
    syscall(-TFN_EXD_VGA_FILL, row, col, len, ch, attr);
}
```

### Kernel-Side Handlers (kernel/sys_exd.c)

```c
ER sys_vga_write_at(W apic, int row, int col, char *s, unsigned char attr)
{
    vga_write_at(row, col, s, attr);
    return E_OK;
}

ER sys_vga_fill_at(W apic, int row, int col, int len, int ch,
                   unsigned char attr)
{
    unsigned short *p = (unsigned short *)0xB8000 + row * 80 + col;
    int i;
    for (i = 0; i < len && col + i < 80; i++)
        p[i] = (unsigned short)attr << 8 | (unsigned char)ch;
    return E_OK;
}
```

### Design Without I/O Port Operations

Functions like `vga_write_at()` perform screen output solely by writing
to VRAM (0xB8000) without operating on I/O ports (6845 registers).
The 6845 is already initialized by `video_init()` at boot time, and there
is no need to touch it afterward.

---

## 9. Usage Examples in User Tasks

`kernel/user.c` manages the demo screen layout using fixed rows and columns.

### Screen Layout Constants

```c
/* kernel/user.c */
#define ROW_HEADER     0      /* Title row */
#define ROW_SEP        1      /* Separator line */
#define ROW_TIMER      3      /* Timer tick display */
#define ROW_TASK1      5      /* Task 1 counter + MBF received string */
#define ROW_TASK3      6      /* Task 3 counter + semaphore status */
#define ROW_SHARED     7      /* Shared counter */
#define ROW_TASK2      8      /* Task 2 counter + semaphore status */
#define ROW_KBD        9      /* Task 4 keyboard echo */
#define ROW_COPYRIGHT  24     /* Copyright notice */
```

### Concrete Usage Patterns

**Displaying a string:**
```c
print_at(ROW_HEADER, 2, "TinyItron/386 SMP (2 CPU)", ATTR_WHITE);
```

**Right-aligned numeric display:**
```c
/* Display task_count[1] in green, right-aligned in 8 digits */
print_dec_at(ROW_TASK1, 19, task_count[1], 8, ATTR_GREEN);
```

**Displaying semaphore status:**
```c
/* Semaphore acquired successfully -> "LOCK" in yellow */
print_at(ROW_TASK3, 31, "LOCK", ATTR_YELLOW);

/* Semaphore acquisition failed (held by other CPU) -> "BUSY" in red */
print_at(ROW_TASK3, 31, "BUSY", ATTR_RED);

/* Semaphore not operated -> "----" in dark gray */
print_at(ROW_TASK3, 31, "----", ATTR_DARK);
```

**Filling a region (clearing the keyboard echo row):**
```c
fill_at(ROW_KBD, 20, kbd_max - 20, ' ', 0x0E);
```

### Timer Tick Display (Direct Write from ISR)

Timer tick display is not done through a syscall. Instead, `vga_write_dec_at()`
is called directly from the ISR (Ring 0). Since the ISR runs in Ring 0, direct
access to VGA VRAM is not a problem:

```c
/* i386/timer.c */
void timer_intr(unsigned char apic, unsigned long delta)
{
    if (apic == 0) {
        timer_ticks++;
        vga_write_dec_at(3, 21, timer_ticks, 10, 0x0B);  /* Cyan */
    }
    sched_timeout(apic, delta);
}
```

---

## Referenced Source Files

| File              | Contents                                   |
|-------------------|--------------------------------------------|
| i386/videoP.h     | VGA constant definitions, va_list macros   |
| i386/video.h      | Public API declarations                    |
| i386/video.c      | Full implementation (printk, vga_write_at, etc.) |
| i386/io.h         | I/O port addresses (IO_6845=0x3D4)         |
| kernel/sys_exd.c  | Syscall handlers                           |
| lib/lib_exd.c     | User-side wrappers                         |
| kernel/user.c     | Usage examples (screen layout, task code)  |
| i386/page.c       | Page table (U/S settings for 0xB8000)      |
