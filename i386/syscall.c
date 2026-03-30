/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "klib.h"
#include "../include/itron.h"
#include "../include/syscall.h"
#include "proc.h"
#include "kernelval.h"
#include "smp.h"
#include "smpP.h"

/* Big Kernel Lock (defined in kernel/sched.c) */
extern unsigned long kernel_lk;

W itron_syscall(unsigned long, unsigned long,
		unsigned long, unsigned long, unsigned long,
		unsigned long, unsigned long, unsigned long);

/* c_intr_syscall -----------------------------------------------------------*/
/* Called from intr_syscall with a pointer to the pt_regs frame on the
 * kernel stack.  Reads the syscall arguments from the USER stack (the
 * library wrapper syscall() pushes them before int $0x99), dispatches
 * to itron_syscall, and writes the return value into regs->eax so the
 * task receives it when RESTORE_ALL pops EAX.
 *
 * User stack layout at the time of int $0x99 (set up by klib.s syscall):
 *   [user_esp + 0]   saved EBP (pushed by syscall wrapper)
 *   [user_esp + 4]   return address (to lib function)
 *   [user_esp + 8]   func_code (sysid)
 *   [user_esp + 12]  arg1
 *   [user_esp + 16]  arg2
 *   ...                                                                    */
W
c_intr_syscall(struct pt_regs *regs)
{
	unsigned long *ustack;
	unsigned long apic;
	W	ret;

	/* Determine CPU from APIC ID */
	apic = *(volatile unsigned long *)APIC_ID;
	apic >>= 24;
	if (apic) apic = 1;

	/* Read syscall arguments from user stack */
	ustack = (unsigned long *)regs->esp;

	smp_lock(&kernel_lk);
	ret = itron_syscall(apic,
			ustack[2],	/* sysid   (user_esp + 8) */
			ustack[3],	/* arg1    (user_esp + 12) */
			ustack[4],	/* arg2    (user_esp + 16) */
			ustack[5],	/* arg3    (user_esp + 20) */
			ustack[6],	/* arg4    (user_esp + 24) */
			ustack[7],	/* arg5    (user_esp + 28) */
			ustack[8]);	/* arg6    (user_esp + 32) */
	smp_unlock(&kernel_lk);

	/* Write return value into the pt_regs EAX slot.  RESTORE_ALL will
	 * pop this into EAX, so the task receives the return value.        */
	regs->eax = ret;
	return ret;
}
