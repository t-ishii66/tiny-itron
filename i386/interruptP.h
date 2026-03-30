/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/

#ifndef _INTERRUPTP_H
#define _INTERRUPTP_H

gate_t*	idt = (gate_t*)AL_IDT;

struct intr_table {
	void 		(*func)(void);
	unsigned char	vect;
};

struct intr_table intr_table[] = {
	{intr_divide,			0},
	{intr_singlestep, 		1},
	{intr_nmi,			2},
	{intr_breakpoint,		3},
	{intr_overflow,			4},
	{intr_bounds,			5},
	{intr_opcode,			6},
	{intr_copr_not_available,	7},
	{intr_doublefault,		8},
	{intr_copr_seg_overrun,		9},
	{intr_tss,			10},
	{intr_segment_not_present,	11},
	{intr_stack,			12},
	{intr_general,			13},
	{intr_page,			14},
	{intr_copr_error,		0x10},
	{intr_irq0,			VECT_IRQ0},
	{intr_irq1,			VECT_IRQ1},
	{intr_irq2,			VECT_IRQ2},
	{intr_irq3,			VECT_IRQ3},
	{intr_irq4,			VECT_IRQ4},
	{intr_irq5,			VECT_IRQ5},
	{intr_irq6,			VECT_IRQ6},
	{intr_irq7,			VECT_IRQ7},
	{intr_irq8,			VECT_IRQ8},
	{intr_irq9,			VECT_IRQ9},
	{intr_irq10,			VECT_IRQ10},
	{intr_irq11,			VECT_IRQ11},
	{intr_irq12,			VECT_IRQ12},
	{intr_irq13,			VECT_IRQ13},
	{intr_irq14,			VECT_IRQ14},
	{intr_irq15,			VECT_IRQ15}
};

#define N_INTR_TABLE	(sizeof(intr_table) / sizeof(struct intr_table))

static void setup_trap(void);
static void setup_irq(void);
static void setup_syscall(void);

#endif
