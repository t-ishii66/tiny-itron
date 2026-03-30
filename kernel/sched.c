/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/kernelval.h"
#include "../i386/smp.h"
#include "types.h"
#include "val.h"
#include "sched.h"
#include "sys_sem.h"

/* Big Kernel Lock ----------------------------------------------------------*/
/* Single lock protecting all kernel data structures (tsk[], tsk_pri[],
 * semaphores, flags, pools, timeout lists).  Acquired at the entry points
 * (syscall, IRQ0, IRQ1, APIC timer1) and in sched_do_next_tsk.
 * video_lk remains separate to avoid deadlock with printk.              */
unsigned long kernel_lk = 0;

INT	cpu_stat;
INT	dispatch_stat;
INT	next_tsk_flag[2];

/* sched_init ---------------------------------------------------------------*/
void
sched_init(void)
{
	H	i;
	for (i = TMIN_TPRI ; i <= TMAX_TPRI ; i ++) {
		tsk_pri[i].next = &(tsk_pri[i]);
		tsk_pri[i].prev = &(tsk_pri[i]);
	}

	cpu_stat = CPU_UNLOCK;
	dispatch_stat = DISPATCH_ENABLE;

	timeout.next = &timeout;
	timeout.prev = &timeout;
	next_tsk_flag[0] = next_tsk_flag[1] = 0;
}

/* sched_ins ----------------------------------------------------------------*/
ER
sched_ins(PRI pri, T_LINK* link)
{
	if (pri < TMIN_TPRI || pri > TMAX_TPRI)
		return E_PAR;
	/* caller holds kernel_lk */
	link->next = &tsk_pri[pri];
	link->prev = tsk_pri[pri].prev;
	tsk_pri[pri].prev->next = link;
	tsk_pri[pri].prev = link;
	return E_OK;
}

/* sched_rem ----------------------------------------------------------------*/
void
sched_rem(T_LINK* link)
{
	/* caller holds kernel_lk */
	link->prev->next = link->next;
	link->next->prev = link->prev;
}

/* sched_hold_tsk -----------------------------------------------------------*/
ID
sched_hold_tsk(ID tskid, STAT s)
{
	/* caller holds kernel_lk */
	sched_rem(&tsk[tskid].plink);
	tsk[tskid].tskstat = s;
	sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink);
	return E_OK;
}

/* sched_next_tsk (this must be called by interrupt handler) ----------------*/
/*                (don't call from system call) -----------------------------*/
/* Signal both CPUs to reschedule so that cross-CPU wakeups work.           */
ID
sched_next_tsk(W apic)
{
	next_tsk_flag[0] = 1;
	next_tsk_flag[1] = 1;
	return E_OK;
}

/* sched_do_next_tsk --------------------------------------------------------*/
/* Find highest-priority RDY task for the specified CPU (via cpu affinity). */
/* Handles preemption: temporarily sets current RUN task to RDY so it can  */
/* be found again if no higher-priority task exists.                        */

ID
sched_do_next_tsk(W apic)
{
	int	i;
	T_LINK*	link;
	T_TSK*	t;
	ID	old_id;
	int	was_run = 0;

	smp_lock(&kernel_lk);

	/* Mark current task as RDY for preemption support ------------------*/
	old_id = c_tskid[apic];
	if (old_id > 0 && old_id < MAX_TSKID &&
	    tsk[old_id].tskstat == TTS_RUN) {
		tsk[old_id].tskstat = TTS_RDY;
		was_run = 1;
	}

	for (i = 1 ; i <= TMAX_TPRI ; i ++) {
		link = tsk_pri[i].next;
		while (link != &tsk_pri[i]) {
			t = plink2tsk(link);
			if (t->tskstat == TTS_RDY &&
			    proc[t->tskid].cpu == apic) {
				t->tskstat = TTS_RUN;
				/* new task ---------------------------------*/
				current_proc[apic] = &proc[t->tskid];
				c_tskid[apic] = t->tskid;

				smp_unlock(&kernel_lk);
				return t->tskid;
			}
			link = link->next;
		}
	}

	/* No RDY task found - restore old task ----------------------------*/
	if (was_run) {
		tsk[old_id].tskstat = TTS_RUN;
	}

	smp_unlock(&kernel_lk);
	return E_ID;
}

/* sched_timeout_ins --------------------------------------------------------*/
void
sched_timeout_ins(T_TIMEOUT* t)
{
	T_TIMEOUT*	tp = timeout.next;
	/* caller holds kernel_lk */
	while (tp != &timeout) {
		if (tp->delta >= t->delta) {
			/* only adjust the immediate successor: it becomes
			 * relative to t instead of to tp->prev.  Entries
			 * beyond tp are still relative to tp, so they are
			 * unchanged.                                       */
			tp->delta -= t->delta;
			break;
		}
		t->delta -= tp->delta;
		tp = tp->next;
	}
	tp = tp->prev;
	t->prev = tp;
	tp->next->prev = t;
	t->next = tp->next;
	tp->next = t;
}

/* sched_timeout_rem --------------------------------------------------------*/
void
sched_timeout_rem(T_TIMEOUT* t)
{
	/* caller holds kernel_lk */
	/* propagate delta to preserve absolute expiry of subsequent entries */
	if (t->next != &timeout)
		t->next->delta += t->delta;
	t->prev->next = t->next;
	t->next->prev = t->prev;
	/* mark as not-in-queue (self-referencing) */
	t->next = t;
	t->prev = t;
}

/* sched_timeout_rem_if_exist -----------------------------------------------*/
/* O(1) removal using self-referencing convention:
 * tlink.next == tlink means not in the timeout queue.                       */
void
sched_timeout_rem_if_exist(T_TIMEOUT* t)
{
	/* caller holds kernel_lk */
	if (t->next == t)
		return;		/* not in timeout queue */
	sched_timeout_rem(t);
}

/* sched_timeout ------------------------------------------------------------*/
/* Called every timer tick with delta=1.  Decrements the head entry's delta
 * and processes all expired entries (delta <= 0).
 *
 * 
 * Three bugs fixed vs original:
 *   Bug A: E_TMOUT was never set — task saw stale return value (E_OK)
 *   Bug B: non-WAI orphan entries were never removed, blocking the queue
 *   Bug C: only one entry per tick was processed, even when multiple expire
 */
void
sched_timeout(W apic, unsigned long delta)
{
	extern T_TIMEOUT	timeout;
	extern T_TSK		tsk[];
	T_TIMEOUT*		tp;
	T_TSK*			tsk_ptr;
	int			woken = 0;

	/* caller holds kernel_lk */
	tp = timeout.next;
	if (tp == &timeout)
		return;

	tp->delta -= delta;

	/* process all expired entries (Bug C: loop, not single) */
	while (tp != &timeout && tp->delta <= 0) {
		T_TIMEOUT *next = tp->next;

		/* always remove expired entry regardless of tskstat (Bug B) */
		if (next != &timeout)
			next->delta += tp->delta;  /* carry residual (<= 0) */
		tp->prev->next = next;
		next->prev = tp->prev;
		/* mark as not-in-queue (self-referencing) */
		tp->next = tp;
		tp->prev = tp;

		tsk_ptr = tlink2tsk(tp);
		if (tsk_ptr->tskstat == TTS_WAI) {
			/* remove from object wait queue (sem/flg/dtq/...) */
			if (tsk_ptr->wlink.next != &(tsk_ptr->wlink))
				wlink_rem(&(tsk_ptr->wlink));
			proc_set_return_value(tsk_ptr->proc, E_TMOUT);  /* Bug A */
			tsk_ptr->tskstat = TTS_RDY;
			sched_ins(tsk_ptr->tskpri, &(tsk_ptr->plink));
			woken = 1;
		}

		tp = timeout.next;
	}

	if (woken)
		sched_next_tsk(apic);
}
