/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "video.h"
#include "io.h"
#include "klib.h"
#include "interrupt.h"
#include "keyboardP.h"

/* DTQ ID for keyboard input (set by kbd_task via set_key_task syscall) */
int	key_dtq_id = 0;

/* kernel-level ipsnd_dtq (3 args: apic, dtqid, data) */
extern int ipsnd_dtq(int, int, int);

/* key_init ---------------------------------------------------------------- */
void
key_init(void)
{
	mode = 0;
	key_dtq_id = 0;
	printk("keyboard: initialized\n");
}

/* key_start: unmask IRQ1 (call from main before start_first_task) --------- */
void
key_start(void)
{
	irq_mask_off(2);	/* bit 1 = IRQ1 (keyboard) */
}

/* key_intr: called from c_intr_irq1 (ISR context) ------------------------ */
int
key_intr(void)
{
	unsigned char	c;
	char		ch;

	c = inb(IO_KEY);
	outb(IO_KEY_CS, 0xad);
	outb(IO_KEY_CS, 0xae);

	if (c & 0x80) {		/* key release */
		if (c == 0xaa || c == 0xb6)
			mode &= ~SHIFT;
		else if (c == 0x9d)
			mode &= ~CTRL;
		return 0;
	}

	if (c == 0x2a || c == 0x36) {
		mode |= SHIFT;
		return 0;
	} else if (c == 0x1d) {
		mode |= CTRL;
		return 0;
	}

	/* Ctrl+C: reset the CPU (QEMU -no-reboot will exit cleanly) */
	if ((mode & CTRL) && c == 0x2e) {
		__asm__("cli");
		vga_write_at(12, 28, "  System halted.  ", 0x4F);
		outb(IO_KEY_CS, 0xFE);		/* pulse CPU reset line */
		while (1) { __asm__("hlt"); }	/* fallback if reset fails */
	}

	/* decode scancode to ASCII */
	if (mode & SHIFT)
		ch = scode_sh[c];
	else
		ch = scode[c];

	/* send to DTQ for keyboard task */
	if (key_dtq_id > 0)
		ipsnd_dtq(0, key_dtq_id, ch);

	return 0;
}
