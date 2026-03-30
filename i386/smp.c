/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* SMP support (2-CPU with Local APIC) -------------------------------------*/
#include "klib.h"
#include "386.h"
#include "smpP.h"
#include "smp.h"
#include "addr.h"
#include "tss.h"
#include "interrupt.h"
#include "video.h"
#include "kernelval.h"

/* handshake variable ------------------------------------------------------*/
volatile int cpu_second = 0;

/* smp_lock -----------------------------------------------------------------*/
/* xchgl-based spinlock. Safe from Ring 3 (no cli/sti needed).             */
void
smp_lock(unsigned long *p)
{
	while (cxchg(p, 1))
		__asm__ volatile("pause");
}

/* smp_unlock ---------------------------------------------------------------*/
void
smp_unlock(unsigned long *p)
{
	cxchg(p, 0);
}

/* smp_eoi ------------------------------------------------------------------*/
void
smp_eoi(void)
{
	volatile unsigned long *eoi = (volatile unsigned long *)APIC_EOI;
	*eoi = 0;
}

/* cpu_lock -----------------------------------------------------------------*/
void
cpu_lock(void)
{
	ccli();
}

/* cpu_unlock ---------------------------------------------------------------*/
void
cpu_unlock(void)
{
	csti();
}

/* apic_write ---------------------------------------------------------------*/
static void
apic_write(unsigned long reg, unsigned long val)
{
	volatile unsigned long *p = (volatile unsigned long *)reg;
	*p = val;
}

/* apic_read ----------------------------------------------------------------*/
static unsigned long
apic_read(unsigned long reg)
{
	volatile unsigned long *p = (volatile unsigned long *)reg;
	return *p;
}

/* delay_loop ---------------------------------------------------------------*/
static void
delay_loop(int count)
{
	volatile int i;
	for (i = 0; i < count; i++)
		;
}

/* smp_init -----------------------------------------------------------------*/
/* Called from main() on BSP. Initializes APIC, sends SIPI to AP.          */
void
smp_init(void)
{
	unsigned long svr;
	extern void timer_start();
	extern void key_start();

	/* --- Enable Local APIC on BSP ----------------------------------- */
	svr = apic_read(APIC_SVR);
	svr |= APIC_ENABLED;
	svr = (svr & 0xFFFFFF00) | VECT_APIC;	/* spurious vector */
	apic_write(APIC_SVR, svr);

	/* --- Setup APIC timer on BSP ------------------------------------ */
	apic_write(APIC_TIMER_DIV, APIC_TIMER_DIV_16);
	apic_write(APIC_LVT_TIMER, APIC_TIMER_PERIODIC | VECT_SMP_TIMER0);
	apic_write(APIC_TIMER_INIT_COUNT, MAX_TIMER_COUNT);

	/* --- Register APIC timer IDT entries ---------------------------- */
	set_idt(VECT_SMP_TIMER0, (unsigned long)intr_smp_timer0,
		SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_SMP_TIMER1, (unsigned long)intr_smp_timer1,
		SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_APIC, (unsigned long)intr_default,
		SEL_K32_C, 0, GT_INTR);

	printk("BSP: APIC init complete\n");

	/* --- Send INIT IPI to all other CPUs ---------------------------- */
	apic_write(APIC_ICR_HIGH, 0);
	apic_write(APIC_ICR_LOW,
		ICR_ALL_EXCLUDING_SELF | ICR_INIT | ICR_LEVEL_ASSERT);
	delay_loop(100000);

	/* deassert */
	apic_write(APIC_ICR_LOW,
		ICR_ALL_EXCLUDING_SELF | ICR_INIT | ICR_LEVEL_DEASSERT);
	delay_loop(100000);

	/* --- Send SIPI (Startup IPI) ------------------------------------ */
	/* vector = 0x03 means AP starts at 0x3000 physical                */
	/* start.s is at 0x3000, which sets up protected mode and jumps    */
	/* to 0x3400 (run), where cpu_num distinguishes BSP from AP        */
	apic_write(APIC_ICR_HIGH, 0);
	apic_write(APIC_ICR_LOW,
		ICR_ALL_EXCLUDING_SELF | ICR_STARTUP | 0x03);
	delay_loop(100000);

	/* send SIPI again (Intel recommends sending twice) */
	apic_write(APIC_ICR_HIGH, 0);
	apic_write(APIC_ICR_LOW,
		ICR_ALL_EXCLUDING_SELF | ICR_STARTUP | 0x03);
	delay_loop(100000);

	printk("BSP: SIPI sent, waiting for AP...\n");

	/* --- Wait for AP handshake -------------------------------------- */
	while (!cpu_second)
		;

	printk("BSP: AP started\n");

	/* --- Start PIT timer (IRQ0) and keyboard (IRQ1) on BSP ---------- */
	timer_start();
	key_start();

	/* --- Jump to first task on BSP ---------------------------------- */
	start_first_task();
}

/* smp_ap_init --------------------------------------------------------------*/
/* Called from main() on AP (CPU 1). Initializes its APIC and starts task. */
void
smp_ap_init(void)
{
	unsigned long svr;

	/* Enable Local APIC on AP */
	svr = apic_read(APIC_SVR);
	svr |= APIC_ENABLED;
	svr = (svr & 0xFFFFFF00) | VECT_APIC;
	apic_write(APIC_SVR, svr);

	/* Setup APIC timer on AP */
	apic_write(APIC_TIMER_DIV, APIC_TIMER_DIV_16);
	apic_write(APIC_LVT_TIMER, APIC_TIMER_PERIODIC | VECT_SMP_TIMER1);
	apic_write(APIC_TIMER_INIT_COUNT, MAX_TIMER_COUNT);

	/* Signal BSP that AP is ready */
	cpu_second = 1;

	/* Jump to first task on AP (Ring 3 via RESTORE_ALL + iret) */
	start_second_task();
}
