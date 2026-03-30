/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* SMP private definitions (Local APIC) ------------------------------------*/
#ifndef _SMPP_H
#define _SMPP_H

/* Local APIC base address (fixed for x86) ---------------------------------*/
#define APIC_BASE		0xFEE00000

/* Local APIC registers (offsets from APIC_BASE) ---------------------------*/
#define APIC_ID			(APIC_BASE + 0x020)
#define APIC_VERSION		(APIC_BASE + 0x030)
#define APIC_EOI		(APIC_BASE + 0x0B0)
#define APIC_SVR		(APIC_BASE + 0x0F0)
#define APIC_ICR_LOW		(APIC_BASE + 0x300)
#define APIC_ICR_HIGH		(APIC_BASE + 0x310)
#define APIC_LVT_TIMER		(APIC_BASE + 0x320)
#define APIC_TIMER_INIT_COUNT	(APIC_BASE + 0x380)
#define APIC_TIMER_DIV		(APIC_BASE + 0x3E0)

/* APIC SVR bits -----------------------------------------------------------*/
#define APIC_ENABLED		0x100

/* APIC timer divider values -----------------------------------------------*/
#define APIC_TIMER_DIV_16	0x03

/* APIC timer mode ---------------------------------------------------------*/
#define APIC_TIMER_PERIODIC	0x20000

/* APIC ICR delivery modes -------------------------------------------------*/
#define ICR_INIT		0x00000500
#define ICR_STARTUP		0x00000600
#define ICR_LEVEL_ASSERT	0x00004000
#define ICR_LEVEL_DEASSERT	0x00000000
#define ICR_ALL_EXCLUDING_SELF	0x000C0000

/* APIC timer count --------------------------------------------------------*/
/* APIC timer period: lower = more frequent interrupts.
 * 0x800000 was ~7 Hz in QEMU.  0x80000 is ~16x faster (~100 Hz).
 * Needed so QEMU TCG gives CPU 1 fair scheduling time slices. */
#define MAX_TIMER_COUNT		0x00080000

/* SMP interrupt vectors ---------------------------------------------------*/
#define VECT_APIC		0x98
#define VECT_SMP_TIMER0		0x9a
#define VECT_SMP_TIMER1		0x9b

/* handshake variable (BSP waits for AP) -----------------------------------*/
extern volatile int cpu_second;

#endif
