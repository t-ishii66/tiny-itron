/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* main ---------------------------------------------------------------------*/
#include "../include/itron.h"
#include "../include/types.h"
#include "../kernel/kernel.h"
#include "io.h"
#include "addr.h"
#include "klib.h"
#include "video.h"
#include "i8259.h"
#include "floppy.h"
#include "tss.h"
#include "interrupt.h"
#include "timer.h"
#include "proc.h"
#include "keyboard.h"
#include "smp.h"
#include "kernelval.h"
#include "page.h"
#include "mainP.h"

/* main ---------------------------------------------------------------------*/
int
main(void)
{
	ccli();

	if (cpu_num == 0) {
		/* --- BSP (CPU 0) ---------------------------------------- */
		all_init();		/* i386 init */
		page_init();		/* build page tables */
		page_enable();		/* load CR3, set CR0.PG */
		itron_init();		/* itron init (tsk[], pool, sched) */
		proc_init();		/* create tasks (needs itron_init first) */
		tss_init();		/* tss init (both CPUs) */

		/* Set cpu_num=1 so that when AP enters run->main,
		 * it takes the AP path */
		cpu_num = 1;

		/* smp_init sends SIPI, waits for AP, starts timer,
		 * keyboard, and jumps to first_task */
		smp_init();
	} else {
		/* --- AP (CPU 1) ----------------------------------------- */
		extern void smp_ap_init();
		page_enable();		/* use BSP's page tables */
		smp_ap_init();
	}

	return 0;		/* never executed */
}

/* all_init -----------------------------------------------------------------*/
static void
all_init(void)
{
	idt_init();
	video_init();		/* 6845 */
	printk("TinyITRON/386 for PC/AT (SMP)\n");
	printk("Copyright (c) 2000-2026 by t-ishii66. All rights reserved.\n");
	printk("\n");

	i8259_init();
	timer_init();

	key_init();		/* keyboard */
}
