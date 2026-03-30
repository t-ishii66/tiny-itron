/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/types.h"
#include "proc.h"
#include "../kernel/types.h"
#include "../kernel/sys_tsk.h"
#include "../kernel/pool.h"
#include "../kernel/sched.h"
#include "addr.h"
#include "interrupt.h"
#include "procP.h"
#include "kernelval.h"
#include "smpP.h"

/* proc_init ----------------------------------------------------------------*/
void
proc_init(void)
{
	int	i;
	T_CTSK	ctsk;
	void first_task();
	void second_task();
	void idle_task();
	extern	T_TSK	tsk[];

	for (i = 0 ; i < MAX_TSKID ; i ++) {
		proc[i].kern_esp = 0;
		proc[i].kern_stack_top = KERN_STACK_BASE + (i + 1) * KERN_STACK_SIZE;
		proc[i].saved_eflags = 0;
		proc[i].cpu = 0;
	}

	/* first_task (CPU 0) -----------------------------------------------*/
	ctsk.tskatr = 0;
	ctsk.task = (FP)first_task;
	ctsk.stk = stack_alloc(1024);
	ctsk.stksz = 1024;
	ctsk.itskpri = TMAX_TPRI - 1;
	sys_cre_tsk(0, 1, &ctsk);
	sys_act_tsk(0, 1);
	tsk_stat_change(1, TTS_RUN);
	proc[1].cpu = 0;

	current_proc[0] = &proc[1];
	c_tskid[0] = 1;

	/* second_task (CPU 1) ----------------------------------------------*/
	ctsk.task = (FP)second_task;
	ctsk.stk = stack_alloc(1024);
	ctsk.stksz = 1024;
	ctsk.itskpri = TMAX_TPRI - 1;
	sys_cre_tsk(0, 2, &ctsk);
	sys_act_tsk(0, 2);
	tsk_stat_change(2, TTS_RUN);
	proc[2].cpu = 1;		/* runs on CPU 1 */

	current_proc[1] = &proc[2];
	c_tskid[1] = 2;

	/* idle_task (CPU 0, lowest priority) -------------------------------*/
	ctsk.task = (FP)idle_task;
	ctsk.stk = stack_alloc(1024);
	ctsk.stksz = 1024;
	ctsk.itskpri = TMAX_TPRI;
	ctsk.exinf = 5;
	sys_cre_tsk(0, 5, &ctsk);
	sys_act_tsk(0, 5);
	proc[5].cpu = 0;

	/* idle_task (CPU 1, lowest priority) -------------------------------*/
	ctsk.stk = stack_alloc(1024);
	ctsk.exinf = 6;
	sys_cre_tsk(0, 6, &ctsk);
	sys_act_tsk(0, 6);
	proc[6].cpu = 1;

	/* timer variable initialize ----------------------------------------*/
	timer_return[0] = 0;
	clock_tick[0] = 0;
	lost_tick[0] = 0;
	timer_return[1] = 0;
	clock_tick[1] = 0;
	lost_tick[1] = 0;
}

/* proc_create --------------------------------------------------------------*/
/* Build a fake interrupt frame + SAVE_ALL frame on the task's kernel stack.
 * When RESTORE_ALL + iret execute for the first time, this frame causes
 * the CPU to jump to the task's entry point in Ring 3.                     */
proc_t*
proc_create(ID tskid, T_CTSK* pk_ctsk)
{
	ER proc_set_tsk_arg(ID, VP_INT);
	extern void intr_return_restore();
	unsigned long stack_top = KERN_STACK_BASE + (tskid + 1) * KERN_STACK_SIZE;
	unsigned long *sp = (unsigned long *)stack_top;

	/* User-mode stack pointer (top of allocated user stack, aligned) */
	unsigned long user_esp = ((unsigned long)pk_ctsk->stk + pk_ctsk->stksz) & ~3UL;

	proc[tskid].kern_stack_top = stack_top;

	/* --- CPU interrupt frame (popped by iret, Ring 3->0 layout) --- */
	*--sp = SEL_U32_S | 3;			/* SS  (Ring 3) */
	*--sp = user_esp;			/* ESP (Ring 3) */
	*--sp = INIT_EFLAGS;			/* EFLAGS (IF=1) */
	*--sp = SEL_U32_C | 3;			/* CS  (Ring 3) */
	*--sp = (unsigned long)pk_ctsk->task;	/* EIP (task entry) */

	/* --- SAVE_ALL frame (popped by RESTORE_ALL) --- */
	*--sp = 0;		/* EAX */
	*--sp = 0;		/* ECX */
	*--sp = 0;		/* EDX */
	*--sp = 0;		/* EBX */
	*--sp = 0;		/* EBP */
	*--sp = 0;		/* ESI */
	*--sp = 0;		/* EDI */
	*--sp = SEL_U32_D | 3;	/* DS  (Ring 3 data segment) */
	*--sp = SEL_U32_D | 3;	/* ES  (Ring 3 data segment) */

	/* --- Return address for intr_leave's "ret" instruction ---
	 * When this task is first scheduled by intr_leave, it loads
	 * kern_esp into ESP and does "ret".  This pops the return
	 * address and jumps to intr_return_restore (RESTORE_ALL; iret),
	 * which pops the pt_regs frame and enters Ring 3.              */
	*--sp = (unsigned long)intr_return_restore;

	proc[tskid].kern_esp = (unsigned long)sp;

	/* inherit CPU affinity from creating CPU */
	{
		volatile unsigned long *apic_reg =
			(volatile unsigned long *)APIC_ID;
		unsigned long id = *apic_reg >> 24;
		proc[tskid].cpu = (id != 0) ? 1 : 0;
	}

	proc_set_tsk_arg(tskid, pk_ctsk->exinf);

	return &proc[tskid];
}

/* proc_set_tsk_arg ---------------------------------------------------------*/
/* Push task argument and ext_tsk return address onto the USER stack.
 * These are in the fake interrupt frame's ESP slot, so we modify that.     */
ER
proc_set_tsk_arg(ID tskid, VP_INT arg)
{
	extern void ext_tsk();
	unsigned long	p = (unsigned long)arg;

	/* Find the user ESP in the fake frame on the kernel stack.
	 * kern_stack_top - 4 = SS, -8 = ESP, so ESP slot is at
	 * kern_stack_top - 8 bytes (2 longs from top).                     */
	unsigned long *esp_slot = (unsigned long *)(proc[tskid].kern_stack_top - 2 * 4);
	unsigned long user_esp = *esp_slot;
	unsigned long *uesp = (unsigned long *)user_esp;

	*(-- uesp) = (unsigned long)p;		/* argument */
	*(-- uesp) = (unsigned long)ext_tsk;	/* return to ext_tsk (user space) */

	*esp_slot = (unsigned long)uesp;
	return E_OK;
}

/* proc_delete --------------------------------------------------------------*/
ER
proc_delete(ID tskid)
{
	return E_OK;
}

/* proc_eflags_save ---------------------------------------------------------*/
/* Save EFLAGS from the task's kernel stack frame (for task exception).
 * The pt_regs frame is at the top of the kernel stack (for a first-level
 * interrupt from Ring 3): kern_stack_top - sizeof(struct pt_regs).         */
void
proc_eflags_save(ID tskid)
{
	struct pt_regs *regs = (struct pt_regs *)
		(proc[tskid].kern_stack_top - sizeof(struct pt_regs));
	if ((regs->eflags & 0x00000200) != 0)
		proc[tskid].saved_eflags = regs->eflags;
	regs->eflags &= 0xfffffdff;	/* clear IF */
}

/* proc_eflags_restore ------------------------------------------------------*/
void
proc_eflags_restore(ID tskid)
{
	struct pt_regs *regs = (struct pt_regs *)
		(proc[tskid].kern_stack_top - sizeof(struct pt_regs));
	regs->eflags = proc[tskid].saved_eflags;
}

/* proc_set_return_value ----------------------------------------------------*/
/* Set the return value (EAX) in a sleeping task's kernel stack frame.
 * Called by kernel/ syscall implementations (e.g. sig_sem, set_flg) to
 * deliver a return code to a task that was blocked and is now being woken.
 *
 * The task's kern_esp was saved by intr_leave's "movl %esp, (%ebx)" which
 * includes the return address from "call intr_leave" on top.  The pt_regs
 * frame starts at kern_esp + 4 (after the return address).                */
void
proc_set_return_value(proc_t *p, unsigned long val)
{
	struct pt_regs *regs = (struct pt_regs *)(p->kern_esp + 4);
	regs->eax = val;
}
