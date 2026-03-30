/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "sched.h"
#include "val.h"
#include "sys_tsk.h"
#include "sys_sem.h"

/* sem_init -----------------------------------------------------------------*/
void
sem_init(void)
{
	int	i;
	for (i = 0 ; i <= MAX_SEMID ; i ++) {
		sem[i].wlink.prev = &(sem[i].wlink);
		sem[i].wlink.next = &(sem[i].wlink);
		sem[i].act = 0;
	}
}

/* sys_cre_sem --------------------------------------------------------------*/
ER
sys_cre_sem(W apic, ID semid, T_CSEM* pk_csem)
{
	/* caller holds kernel_lk */
	if (semid < 1 || semid > MAX_SEMID)
		return E_ID;
	if (sem[semid].act)
		return E_OBJ;

	sem[semid].act = 1;
	sem[semid].sematr = pk_csem->sematr;
	sem[semid].semcnt = sem[semid].isemcnt = pk_csem->isemcnt;
	sem[semid].maxsem = pk_csem->maxsem;
	return E_OK;
}

/* sys_acre_sem -------------------------------------------------------------*/
ER_ID
sys_acre_sem(W apic, T_CSEM* pk_csem)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_SEMID; i++) {
		if (sem[i].act == 0) {
			if ((ret = sys_cre_sem(apic, i, pk_csem)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_sem --------------------------------------------------------------*/
ER
sys_del_sem(W apic, ID semid)
{
	T_LINK*	wlink;
	T_TSK*	t;
	/* caller holds kernel_lk */
	if (semid < 1 || semid > MAX_SEMID)
		return E_ID;
	if (sem[semid].act == 0)
		return E_NOEXS;

	wlink = &(sem[semid].wlink);
	wlink = wlink->next;
	while (wlink != &(sem[semid].wlink)) {
		t = wlink2tsk(wlink);
		sched_timeout_rem_if_exist(&(t->tlink));
		t->tskstat = TTS_RDY;
		proc_set_return_value(t->proc, E_DLT);
		sched_ins(t->tskpri, &(t->plink));
		wlink = wlink->next;
	}
	sem[semid].wlink.prev = &(sem[semid].wlink);
	sem[semid].wlink.next = &(sem[semid].wlink);
	sem[semid].act = 0;
	return E_OK;
}

/* sys_sig_sem --------------------------------------------------------------*/
ER
sys_sig_sem(W apic, ID semid)
{
	T_LINK*	wlink;
	T_TSK*	t;
	/* caller holds kernel_lk */
	if (semid < 1 || semid > MAX_SEMID)
		return E_ID;
	if (sem[semid].act == 0)
		return E_NOEXS;
	wlink = &(sem[semid].wlink);
	wlink = wlink->next;
	if (wlink != &(sem[semid].wlink)) {
		t = wlink2tsk(wlink);
		wlink_rem(wlink);
		/* cancel pending timeout (twai_sem with TMO) */
		sched_timeout_rem_if_exist(&(t->tlink));
		t->tskstat = TTS_RDY;
		sched_ins(t->tskpri, &(t->plink));
		proc_set_return_value(t->proc, E_OK);
	} else {
		if (sem[semid].semcnt + 1 > sem[semid].maxsem)
			return E_QOVR;
		sem[semid].semcnt ++;
	}
	return E_OK;
}

ER
sys_isig_sem(W apic, ID semid)
{
	return sys_sig_sem(apic, semid);
}

/* sys_wai_sem --------------------------------------------------------------*/
ER
sys_wai_sem(W apic, ID semid)
{
	return sys_twai_sem(apic, semid, TMO_FEVR);
}

/* sys_pol_sem --------------------------------------------------------------*/
ER
sys_pol_sem(W apic, ID semid)
{
	/* caller holds kernel_lk */
	if (semid < 1 || semid > MAX_SEMID)
		return E_ID;
	if (sem[semid].act == 0)
		return E_NOEXS;
	if (sem[semid].semcnt == 0)
		return E_TMOUT;
	sem[semid].semcnt --;
	return E_OK;
}

/* sys_twai_sem -------------------------------------------------------------*/
ER
sys_twai_sem(W apic, ID semid, TMO tmout)
{
	/* caller holds kernel_lk */
	if (semid < 1 || semid > MAX_SEMID)
		return E_ID;
	if (sem[semid].act == 0)
		return E_NOEXS;
	if (sem[semid].semcnt == 0) {
		if (tmout == TMO_POL)
			return E_TMOUT;
		if (sem[semid].sematr == TA_TFIFO) {
			ins_fifo(&(sem[semid].wlink),
						&(tsk[c_tskid[apic]].wlink));
		} else {	/* should be TA_TPRI */
			ins_pri(&(sem[semid].wlink),
						&(tsk[c_tskid[apic]].wlink));
		}
		if (tmout == TMO_FEVR)
			sys_slp_tsk(apic);
		else
			sys_tslp_tsk(apic, tmout);
		return E_TMOUT;
	} else {
		sem[semid].semcnt --;
	}
	return E_OK;
}

/* sys_ref_sem --------------------------------------------------------------*/
ER
sys_ref_sem(W apic, ID semid, T_RSEM* pk_rsem)
{
	T_LINK*	wlink;
	T_TSK*	t;
	/* caller holds kernel_lk */
	if (semid < 1 || semid > MAX_SEMID)
		return E_ID;
	if (sem[semid].act == 0)
		return E_NOEXS;
	wlink = &(sem[semid].wlink);
	if (wlink->next == wlink)
		pk_rsem->wtskid = TSK_NONE;
	else {
		t = wlink2tsk(wlink);
		pk_rsem->wtskid = t->tskid;
	}
	pk_rsem->semcnt = sem[semid].semcnt;
	return E_OK;
}

/*===========================================================================*/
/* ins_fifo (caller holds kernel_lk) ----------------------------------------*/
void
ins_fifo(T_LINK* base, T_LINK* wlink)
{
	T_LINK*	base_sav = base;
	while (base->next != base_sav)
		base = base->next;

	wlink->prev = base;
	wlink->next = base->next;
	base->next->prev = wlink;
	base->next = wlink;
}

/* ins_pri (caller holds kernel_lk) -----------------------------------------*/
void
ins_pri(T_LINK* base, T_LINK* wlink)
{
	T_TSK*	t, *me;
	T_LINK*	base_sav = base;
	me = wlink2tsk(wlink);
	base = base->next;
	while (base != base_sav) {
		t = wlink2tsk(base);
		if (t->tskpri > me->tskpri)
			break;
		base = base->next;
	}
	base = base->prev;

	wlink->prev = base;
	wlink->next = base->next;
	base->next->prev = wlink;
	base->next = wlink;
}

/* wlink_ins ----------------------------------------------------------------*/
void
wlink_ins(T_LINK* base, T_LINK* wlink)
{
	wlink->prev = base;
	wlink->next = base->next;
	base->next->prev = wlink;
	base->next = wlink;
}

/* wlink_rem ----------------------------------------------------------------*/
void
wlink_rem(T_LINK* wlink)
{
	wlink->prev->next = wlink->next;
	wlink->next->prev = wlink->prev;
	/* mark as not-in-queue so sched_timeout's check skips it */
	wlink->next = wlink;
	wlink->prev = wlink;
}

/* wlink_change -------------------------------------------------------------*/
void
wlink_change(T_LINK* wlink, T_LINK *wlink2)
{
	T_LINK	tmp;
	tmp.next = wlink->next;
	tmp.prev = wlink->prev;

	wlink->next = wlink2->next;
	wlink->prev = wlink2->prev;
	wlink2->next = tmp.next;
	wlink2->prev = tmp.prev;
}

void
wlink_dump(T_LINK* base)
{
	T_LINK*	wlink = base->next;

	printk("wlink-dump------------------------------------\n");
	while (wlink != base) {
		printk("wlink:%x\n", wlink);
		wlink = wlink->next;
	}
}
