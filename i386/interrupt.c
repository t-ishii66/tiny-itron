/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* interrupt (32 bit protect mode) ------------------------------------------*/
#include "../include/itron.h"
#include "386.h"
#include "addr.h"
#include "proc.h"
#include "io.h"
#include "i8259.h"
#include "video.h"
#include "timer.h"
#include "floppy.h"
#include "keyboard.h"
#include "klib.h"
#include "smp.h"
#include "smpP.h"
#include "kernelval.h"
#include "interrupt.h"
#include "interruptP.h"
#include "tss.h"

/* Big Kernel Lock (defined in kernel/sched.c) */
extern unsigned long kernel_lk;

/* get_apic_index -----------------------------------------------------------*/
static int
get_apic_index(void)
{
	volatile unsigned long *p = (volatile unsigned long *)APIC_ID;
	unsigned long id = *p;
	id >>= 24;
	return (id != 0) ? 1 : 0;
}

/* idt_init -----------------------------------------------------------------*/
void
idt_init(void)
{
	short	i;
	/* set default interrupt handler ------------------------------------*/
	for (i = 0 ; i < 256 ; i ++) {
		set_idt(i, (unsigned long)intr_default, SEL_K32_C, 0, GT_TRAP);
	}
	setup_trap();
	setup_irq();
	setup_syscall();
}

/* set_idt ------------------------------------------------------------------*/
void
set_idt(short n, unsigned long base, unsigned short sel,
			unsigned char count, unsigned char type)
{
	idt[n].offset_l = (unsigned short)(base & 0xffff);
	idt[n].offset_h = (unsigned short)((base >> 16) & 0xffff);
	idt[n].sel	= sel;
	idt[n].count	= count;
	idt[n].type	= type;
}

/* interrupt handlers -------------------------------------------------------*/
void
c_intr_default(void)
{
	printk("default intr\n");
	i8259_reenable();
}
void
c_intr_default2(void)
{
	printk("Double fault\n");
}

/* c_intr_general -- #GP fault handler with diagnostics --------------------*/
extern unsigned long gp_error_code;
extern proc_t *current_proc[];

void
c_intr_general(void)
{
	unsigned long apic;

	/* Read APIC ID to determine CPU */
	apic = *(volatile unsigned long *)0xFEE00020;
	apic >>= 24;
	if (apic) apic = 1;

	/* The pt_regs frame sits at the top of the kernel stack (for a
	 * first-level interrupt from Ring 3).  Compute its address from
	 * kern_stack_top - sizeof(struct pt_regs).                         */
	{
		proc_t *p = current_proc[apic];
		struct pt_regs *regs = (struct pt_regs *)
			(p->kern_stack_top - sizeof(struct pt_regs));
		printk("#GP err=%x eip=%x esp=%x cs=%x cpu=%d\n",
			gp_error_code,
			regs->eip,
			regs->esp,
			regs->cs,
			apic);
	}
	for (;;)
		__asm__ volatile("hlt");
}

/* setup_trap ---------------------------------------------------------------*/
static void
setup_trap(void)
{
	set_idt(0, (unsigned long)intr_divide, SEL_K32_C, 0, GT_INTR);
	set_idt(1, (unsigned long)intr_singlestep, SEL_K32_C, 0, GT_TRAP);
	set_idt(2, (unsigned long)intr_nmi, SEL_K32_C, 0, GT_TRAP);
	set_idt(3, (unsigned long)intr_breakpoint, SEL_K32_C, 0, GT_TRAP);
	set_idt(4, (unsigned long)intr_overflow, SEL_K32_C, 0, GT_TRAP);
	set_idt(5, (unsigned long)intr_bounds, SEL_K32_C, 0, GT_TRAP);
	set_idt(6, (unsigned long)intr_opcode, SEL_K32_C, 0, GT_TRAP);
	set_idt(7, (unsigned long)intr_copr_not_available, SEL_K32_C,0,GT_TRAP);
	set_idt(8, (unsigned long)intr_doublefault, SEL_K32_C, 0, GT_TRAP);
	set_idt(9, (unsigned long)intr_copr_seg_overrun, SEL_K32_C, 0, GT_TRAP);
	set_idt(10, (unsigned long)intr_tss, SEL_K32_C, 0, GT_TRAP);
	set_idt(11,(unsigned long)intr_segment_not_present,SEL_K32_C,0,GT_TRAP);
	set_idt(12, (unsigned long)intr_stack, SEL_K32_C, 0, GT_TRAP);
	set_idt(13, (unsigned long)intr_general, SEL_K32_C, 0, GT_INTR);
	set_idt(14, (unsigned long)intr_page, SEL_K32_C, 0, GT_INTR);
	set_idt(15, (unsigned long)intr_copr_error, SEL_K32_C, 0, GT_TRAP);
}

/* setup_irq ----------------------------------------------------------------*/
static void
setup_irq(void)
{
	set_idt(VECT_IRQ0, (unsigned long)intr_irq0, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ1, (unsigned long)intr_irq1, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ2, (unsigned long)intr_irq2, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ3, (unsigned long)intr_irq3, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ4, (unsigned long)intr_irq4, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ5, (unsigned long)intr_irq5, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ6, (unsigned long)intr_irq6, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ7, (unsigned long)intr_irq7, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ8, (unsigned long)intr_irq8, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ9, (unsigned long)intr_irq9, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ10, (unsigned long)intr_irq10, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ11, (unsigned long)intr_irq11, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ12, (unsigned long)intr_irq12, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ13, (unsigned long)intr_irq13, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ14, (unsigned long)intr_irq14, SEL_K32_C, 0, GT_INTR);
	set_idt(VECT_IRQ15, (unsigned long)intr_irq15, SEL_K32_C, 0, GT_INTR);
}

/* setup_syscall ------------------------------------------------------------*/
static void
setup_syscall(void)
{
	/* CAUTION !!! priority of system call segment must be 3 ------------*/
	set_idt(VECT_SYSCALL, (unsigned long)intr_syscall,
					SEL_K32_C, 0, GT_INTR | 0x60);
}

/* Page fault test state ----------------------------------------------------*/
/* Set by user task before deliberate kernel access; cleared by handler.    */
volatile int pf_test_active = 0;
volatile unsigned long pf_test_addr = 0;

/* c_intr_page (#PF) -------------------------------------------------------*/
void
c_intr_page(void)
{
	unsigned long cr2;
	__asm__ volatile("movl %%cr2, %0" : "=r"(cr2));

	if (pf_test_active) {
		int apic = get_apic_index();
		proc_t *p = current_proc[apic];
		struct pt_regs *regs = (struct pt_regs *)
			(p->kern_stack_top - sizeof(struct pt_regs));

		pf_test_addr = cr2;
		pf_test_active = 0;

		/* Skip the faulting instruction. */
		{
			unsigned char *insn = (unsigned char *)regs->eip;
			int skip = 2;
			if (*insn == 0xA1)
				skip = 5;
			regs->eip += skip;
		}
		return;
	}

	{
		int apic = get_apic_index();
		proc_t *p = current_proc[apic];
		struct pt_regs *regs = (struct pt_regs *)
			(p->kern_stack_top - sizeof(struct pt_regs));
		printk("#PF cr2=%x eip=%x ds=%x cpu=%d\n",
			cr2, regs->eip, regs->ds, apic);
		for (;;) ;	/* halt to debug */
	}
}

/* c_intr_divide ------------------------------------------------------------*/
void
c_intr_divide(void)
{
}

/* c_intr_irq0 (timer) ------------------------------------------------------*/
void
c_intr_irq0(void)
{
	smp_lock(&kernel_lk);
	timer_intr(0, 1);
	i8259_reenable();
	smp_unlock(&kernel_lk);
}

/* c_intr_irq1 (keyboard) ---------------------------------------------------*/
void
c_intr_irq1(void)
{
	smp_lock(&kernel_lk);
	key_intr();
	i8259_reenable();
	smp_unlock(&kernel_lk);
}

/* c_intr_irq2 --------------------------------------------------------------*/
void
c_intr_irq2(void)
{
	printk("IRQ2\n");
	i8259_reenable();
}

/* c_intr_irq3 --------------------------------------------------------------*/
void
c_intr_irq3(void)
{
	printk("IRQ3\n");
	i8259_reenable();
}

void
c_intr_irq4(void)
{
	printk("IRQ4\n");
	i8259_reenable();
}

void
c_intr_irq5(void)
{
	printk("IRQ5\n");
	i8259_reenable();
}

/* c_intr_irq6 (floppy) -----------------------------------------------------*/
void
c_intr_irq6(void)
{
	printk("IRQ6: floppy interrupt\n");
	fdc_intr();
	i8259_reenable();
}

/* c_intr_irq7 (printer/incorrect interrupt) --------------------------------*/
void
c_intr_irq7(void)
{
	unsigned long a = 0xb;
	outb(IO_I8259_M, a);
	a = inb(IO_I8259_M);
	if (!(a & 0x80)) {
		return;
	}
	printk("7");
	i8259_reenable();
}

void
c_intr_irq8(void)
{
	printk("IRQ8\n");
	i8259_reenable();
}

void
c_intr_irq9(void)
{
	printk("KEY\n");
	i8259_reenable();
}

void
c_intr_irq10(void)
{
	printk("IRQ10\n");
	i8259_reenable();
}

void
c_intr_irq11(void)
{
	printk("IRQ11\n");
	i8259_reenable();
}

void
c_intr_irq12(void)
{
	printk("IRQ12\n");
	i8259_reenable();
}

void
c_intr_irq13(void)
{
	printk("IRQ13\n");
	i8259_reenable();
}

void
c_intr_irq14(void)
{
	printk("IRQ14\n");
	i8259_reenable();
}

void
c_intr_irq15(void)
{
	printk("IRQ15\n");
	i8259_reenable();
}

/* c_intr_smp_timer0 (APIC timer, CPU 0) -----------------------------------*/
/* APIC timer on CPU 0 provides additional preemptive task-switch
 * opportunities.  PIT (IRQ0) already handles the system tick and
 * timeout queue, so this handler only needs EOI.                        */
void
c_intr_smp_timer0(void)
{
	smp_eoi();
}

/* c_intr_smp_timer1 (APIC timer, CPU 1) -----------------------------------*/
/* APIC timer provides periodic interrupts for preemptive task switching
 * (via intr_leave).  It does NOT call timer_intr/sched_timeout — only
 * the PIT on CPU 0 manages the system tick and timeout queue.  This
 * avoids double-decrementing the delta-encoded timeout list.            */
void
c_intr_smp_timer1(void)
{
	smp_eoi();
}

/* irq_mask_on --------------------------------------------------------------*/
void
irq_mask_on(int n)
{
	unsigned long	a;
	if (n & 0xff) {
		a = inb(IO_I8259_MD);
		outb(IO_I8259_MD, a | (n & 0xff));
	}
	n >>= 8;
	if (n & 0xff) {
		a = inb(IO_I8259_SD);
		outb(IO_I8259_SD, a | (n & 0xff));
	}
}

/* irq_mask_off -------------------------------------------------------------*/
void
irq_mask_off(int n)
{
	unsigned long	a;
	if (n & 0xff) {
		a = inb(IO_I8259_MD);
		n = n ^ 0xff;
		outb(IO_I8259_MD, a & n);
	}
	n >>= 8;
	if (n & 0xff) {
		a = inb(IO_I8259_SD);
		n = n ^ 0xff;
		outb(IO_I8259_SD, a & n);
	}
}

/* irq_enter (disable timer interrupt during IRQ handling) ------------------*/
void
irq_enter(void)
{
	int apic = get_apic_index();
	timer_return[apic] = 1;
	irq_mask_on(0xfffe);
}

/* irq_exit (reset interrupt mask) ------------------------------------------*/
void
irq_exit(void)
{
	int apic = get_apic_index();
	timer_return[apic] = 0;
	irq_mask_off(0xfffe);
}

/* stack_adjust -------------------------------------------------------------*/
/* Manipulate the user stack to invoke a task exception handler.
 * Reads/writes registers via the pt_regs frame on the kernel stack.        */
void
stack_adjust(W apic, void (*func)(), TEXPTN texptn, VP_INT exinf)
{
	proc_t*	p;
	unsigned long*	esp;
	extern void user_restore();
	int idx = get_apic_index();

	p = current_proc[idx];
	struct pt_regs *regs = (struct pt_regs *)
		(p->kern_stack_top - sizeof(struct pt_regs));

	esp = (unsigned long*)(regs->esp);
	*(-- esp) = regs->eip;
	*(-- esp) = regs->eax;
	*(-- esp) = regs->ebx;
	*(-- esp) = regs->ecx;
	*(-- esp) = regs->edx;
	*(-- esp) = regs->esi;
	*(-- esp) = regs->edi;
	*(-- esp) = regs->eflags;
	*(-- esp) = exinf;
	*(-- esp) = texptn;
	*(-- esp) = (unsigned long)user_restore;
	regs->esp = (unsigned long)esp;
	regs->eip = (unsigned long)func;
}

/* sched_next_tsk_check -----------------------------------------------------*/
int
sched_next_tsk_check(int apic)
{
	proc_t*	old_proc;
	extern INT	next_tsk_flag[];

	if (next_tsk_flag[apic] != 0) {
		old_proc = current_proc[apic];
		sched_do_next_tsk(apic);
		next_tsk_flag[apic] = 0;
		if (old_proc != current_proc[apic]) {
			return 1;
		}
	}
	return 0;
}
