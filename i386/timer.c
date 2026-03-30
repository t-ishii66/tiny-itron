/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/types.h"
#include "proc.h"
#include "../kernel/types.h"
#include "../kernel/sched.h"
#include "video.h"
#include "klib.h"
#include "io.h"
#include "interrupt.h"
#include "kernelval.h"
#include "timerP.h"

volatile unsigned long timer_ticks = 0;

/* timer_init ---------------------------------------------------------------*/
void
timer_init(void)
{
	unsigned long	count;
	count = FREQ / HZ;
	outb(IO_TIMER_C, (unsigned long)SQUARE);
	outb(IO_TIMER_0, (unsigned long)(count & 0xff));
	outb(IO_TIMER_0, (unsigned long)((count >> 8) & 0xff));
	printk("timer: initialized\n");
}

/* timer_intr ---------------------------------------------------------------*/
void
timer_intr(unsigned char apic, unsigned long delta)
{
	extern void sys_isig_tim(int);

	if (apic == 0) {
		timer_ticks++;
		/* wrap before exceeding the 10-digit VGA display field */
		if (timer_ticks >= 1000000000UL)
			timer_ticks = 0;
		vga_write_dec_at(3, 21, timer_ticks, 10, 0x0B);
		sys_isig_tim(0);	/* advance system_time by TIC_NUME (~17ms) */
	}
	sched_timeout(apic, delta);
}

/* timer_stop ---------------------------------------------------------------*/
void
timer_stop(void)
{
	irq_mask_on(1);
}

/* timer_start --------------------------------------------------------------*/
void
timer_start(void)
{
	irq_mask_off(1);
}
