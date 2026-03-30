/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "sys_tsk.h"
#include "sys_sem.h"
#include "sys_dtq.h"
#include "val.h"
#include "pool.h"
#include "sched.h"

/* dtq_init -----------------------------------------------------------------*/
void
dtq_init(void)
{
	int	i;
	for (i = 1 ; i < MAX_DTQID ; i ++) {
		dtq[i].wlink_r.next = &(dtq[i].wlink_r);
		dtq[i].wlink_r.prev = &(dtq[i].wlink_r);
		dtq[i].wlink_w.next = &(dtq[i].wlink_w);
		dtq[i].wlink_w.prev = &(dtq[i].wlink_w);
		dtq[i].act = 0;
	}
}

/* sys_cre_dtq --------------------------------------------------------------*/
ER
sys_cre_dtq(W apic, ID dtqid, T_CDTQ* pk_cdtq)
{
	if (dtqid < 1 || dtqid > MAX_DTQID)
		return E_ID;
	if (dtq[dtqid].act != 0)
		return E_OBJ;

	dtq[dtqid].act = 1;
	dtq[dtqid].dtqatr = pk_cdtq->dtqatr;
	dtq[dtqid].dtqcnt = pk_cdtq->dtqcnt;
	dtq[dtqid].dtq = pk_cdtq->dtq;
	if (dtq[dtqid].dtq == NULL) {
		dtq[dtqid].dtq = kmem_alloc(sizeof(VW) * pk_cdtq->dtqcnt);
		dtq[dtqid].dtq_alloc = 1;
	} else 
		dtq[dtqid].dtq_alloc = 0;
	dtq[dtqid].w = dtq[dtqid].r = (VW)dtq[dtqid].dtq;
	return E_OK;
}

/* sys_acre_dtq -------------------------------------------------------------*/
ER_ID
sys_acre_dtq(W apic, T_CDTQ* pk_cdtq)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_DTQID; i++) {
		if (dtq[i].act == 0) {
			if ((ret = sys_cre_dtq(apic, i, pk_cdtq)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_dtq --------------------------------------------------------------*/
ER
sys_del_dtq(W apic, ID dtqid)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (dtqid < 1 || dtqid > MAX_DTQID)
		return E_ID;
	if (dtq[dtqid].act == 0)
		return E_NOEXS;
	if (dtq[dtqid].dtq_alloc) {
		kmem_free(dtq[dtqid].dtq);
	}

	wlink = dtq[dtqid].wlink_r.next;
	while (wlink != &(dtq[dtqid].wlink_r)) {
		t = wlink2tsk(wlink);
		sched_timeout_rem_if_exist(&(t->tlink));
		t->tskstat = TTS_RDY;
		sched_ins(t->tskpri, &(t->plink));
		proc_set_return_value(t->proc, E_DLT);
		wlink = wlink->next;
	}

	wlink = dtq[dtqid].wlink_w.next;
	while (wlink != &(dtq[dtqid].wlink_w)) {
		t = wlink2tsk(wlink);
		sched_timeout_rem_if_exist(&(t->tlink));
		t->tskstat = TTS_RDY;
		sched_ins(t->tskpri, &(t->plink));
		proc_set_return_value(t->proc, E_DLT);
		wlink = wlink->next;
	}

	dtq[dtqid].wlink_r.next = &(dtq[dtqid].wlink_r);
	dtq[dtqid].wlink_r.prev = &(dtq[dtqid].wlink_r);
	dtq[dtqid].wlink_w.next = &(dtq[dtqid].wlink_w);
	dtq[dtqid].wlink_w.prev = &(dtq[dtqid].wlink_w);
	dtq[dtqid].act = 0;
	return E_OK;
}

/* sys_snd_dtq --------------------------------------------------------------*/
ER
sys_snd_dtq(W apic, ID dtqid, VP_INT data)
{
	return sys_tsnd_dtq(apic, dtqid, data, TMO_FEVR);
}

/* sys_psnd_dtq -------------------------------------------------------------*/
ER
sys_psnd_dtq(W apic, ID dtqid, VP_INT data)
{
	T_LINK*	wlink;
	T_TSK*	t;
	VW	next_w;
	if (dtqid < 1 || dtqid > MAX_DTQID)
		return E_ID;
	if (dtq[dtqid].act == 0)
		return E_NOEXS;
	next_w = dtq[dtqid].w;
	next_w ++;
	if (next_w >= (VW)dtq[dtqid].dtq + dtq[dtqid].dtqcnt) {
		next_w = (VW)dtq[dtqid].dtq;
	}

	wlink = dtq[dtqid].wlink_r.next;
	if (wlink != &(dtq[dtqid].wlink_r)) {	/* already waiting */
		wlink_rem(wlink);
		t = wlink2tsk(wlink);
		/* cancel pending timeout (trcv_dtq with TMO) */
		sched_timeout_rem_if_exist(&(t->tlink));
		t->tskstat = TTS_RDY;
		*(t->p_data) = data;
		sched_ins(t->tskpri, &(t->plink));
		return E_OK;
	}

	if (next_w == dtq[dtqid].r) {	/* must sleep -> error */
		return E_TMOUT;
	} else {
		*(dtq[dtqid].w) = data;
		dtq[dtqid].w = next_w;
	}
	return E_OK;
}

/* ipsnd_dtq (not system call) ----------------------------------------------*/
ER
ipsnd_dtq(W apic, ID dtqid, VP_INT data)
{
	T_LINK*	wlink;
	T_TSK*	t;
	VW	next_w;
	if (dtqid < 1 || dtqid > MAX_DTQID)
		return E_ID;
	if (dtq[dtqid].act == 0)
		return E_NOEXS;
	next_w = dtq[dtqid].w;
	next_w ++;
	if (next_w >= (VW)dtq[dtqid].dtq + dtq[dtqid].dtqcnt) {
		next_w = (VW)dtq[dtqid].dtq;
	}

	wlink = dtq[dtqid].wlink_r.next;
	if (wlink != &(dtq[dtqid].wlink_r)) {	/* already waiting */
		t = wlink2tsk(wlink);
		wlink_rem(wlink);
		/* cancel pending timeout (trcv_dtq with TMO) */
		sched_timeout_rem_if_exist(&(t->tlink));
		t->tskstat = TTS_RDY;
		*(t->p_data) = data;
		sched_ins(t->tskpri, &(t->plink));
		sched_next_tsk(apic);
		return E_OK;
	}

	if (next_w == dtq[dtqid].r) { /* no room */
		return E_TMOUT;
	}

	*(dtq[dtqid].w) = data;
	dtq[dtqid].w = next_w;
	return E_OK;
}

/* sys_tsnd_dtq -------------------------------------------------------------*/
ER
sys_tsnd_dtq(W apic, ID dtqid, VP_INT data, TMO tmout)
{
	T_LINK*	wlink;
	T_TSK*	t;
	VW	next_w;
	if (dtqid < 1 || dtqid > MAX_DTQID)
		return E_ID;
	if (dtq[dtqid].act == 0)
		return E_NOEXS;
	next_w = dtq[dtqid].w;
	next_w ++;
	if (next_w >= (VW)dtq[dtqid].dtq + dtq[dtqid].dtqcnt) {
		next_w = (VW)dtq[dtqid].dtq;
	}

	wlink = dtq[dtqid].wlink_r.next;
	if (wlink != &(dtq[dtqid].wlink_r)) {	/* already waiting */
		wlink_rem(wlink);
		t = wlink2tsk(wlink);
		/* cancel pending timeout (trcv_dtq with TMO) */
		sched_timeout_rem_if_exist(&(t->tlink));
		t->tskstat = TTS_RDY;
		*(t->p_data) = data;
		sched_ins(t->tskpri, &(t->plink));
		return E_OK;
	}

	if (next_w == dtq[dtqid].r) {	/* must sleep */
		if (tmout == TMO_POL)
			return E_TMOUT;

		if (dtq[dtqid].dtqatr == TA_TFIFO) {
			ins_fifo(&(dtq[dtqid].wlink_w),
					&(tsk[c_tskid[apic]].wlink));
		} else {	/* should be TA_TPRI */
			ins_pri(&(dtq[dtqid].wlink_w),
					&(tsk[c_tskid[apic]].wlink));
		}
		tsk[c_tskid[apic]].data = data;
		if (tmout == TMO_FEVR)
			sys_slp_tsk(apic);
		else
			sys_tslp_tsk(apic, tmout);
	} else {
		*(dtq[dtqid].w) = data;
		dtq[dtqid].w = next_w;
	}
	return E_OK;
}

/* sys_fsnd_dtq -------------------------------------------------------------*/
ER
sys_fsnd_dtq(W apic, ID dtqid, VP_INT data)
{
	T_LINK*	wlink;
	T_TSK*	t;
	VW	next_w;
	if (dtqid < 1 || dtqid > MAX_DTQID)
		return E_ID;
	if (dtq[dtqid].act == 0)
		return E_NOEXS;
	next_w = dtq[dtqid].w;
	next_w ++;
	if (next_w >= (VW)dtq[dtqid].dtq + dtq[dtqid].dtqcnt) {
		next_w = (VW)dtq[dtqid].dtq;
	}

	wlink = dtq[dtqid].wlink_r.next;
	if (wlink != &(dtq[dtqid].wlink_r)) {	/* already waiting */
		wlink_rem(wlink);
		t = wlink2tsk(wlink);
		/* cancel pending timeout (trcv_dtq with TMO) */
		sched_timeout_rem_if_exist(&(t->tlink));
		t->tskstat = TTS_RDY;
		*(t->p_data) = data;
		sched_ins(t->tskpri, &(t->plink));
		return E_OK;
	}

	if (next_w == dtq[dtqid].r) {	/* must sleep */
		dtq[dtqid].r ++;
		if (dtq[dtqid].r >= (VW)dtq[dtqid].dtq + dtq[dtqid].dtqcnt)
			dtq[dtqid].r = (VW)dtq[dtqid].dtq;
	}
	*(dtq[dtqid].w) = data;
	dtq[dtqid].w = next_w;
	return E_OK;
}

/* sys_ifsnd_dtq ------------------------------------------------------------*/
ER
sys_ifsnd_dtq(W apic, ID dtqid, VP_INT data)
{
	return sys_fsnd_dtq(apic, dtqid, data);
}

/* sys_rcv_dtq --------------------------------------------------------------*/
ER
sys_rcv_dtq(W apic, ID dtqid, VP_INT* p_data)
{
	return sys_trcv_dtq(apic, dtqid, p_data, TMO_FEVR);
}

ER
sys_prcv_dtq(W apic, ID dtqid, VP_INT* p_data)
{
	if (dtqid < 1 || dtqid > MAX_DTQID)
		return E_ID;
	if (dtq[dtqid].act == 0)
		return E_NOEXS;
	if (dtq[dtqid].w != dtq[dtqid].r) {
		*p_data = *(dtq[dtqid].r);
		dtq[dtqid].r++;
		if (dtq[dtqid].r >= (VW)dtq[dtqid].dtq + dtq[dtqid].dtqcnt)
			dtq[dtqid].r = (VW)dtq[dtqid].dtq;
		return E_OK;
	}
	return E_TMOUT;
}

/* sys_trcv_dtq -------------------------------------------------------------*/
ER
sys_trcv_dtq(W apic, ID dtqid, VP_INT* p_data, TMO tmout)
{
#if 0
	T_LINK*	wlink;
	T_TSK*	t;
#endif
	if (dtqid < 1 || dtqid > MAX_DTQID)
		return E_ID;
	if (dtq[dtqid].act == 0)
		return E_NOEXS;

	if (dtq[dtqid].w != dtq[dtqid].r) {	/* data exist */
		*p_data = *(dtq[dtqid].r);
		dtq[dtqid].r ++;
		if (dtq[dtqid].r >= (VW)dtq[dtqid].dtq + dtq[dtqid].dtqcnt)
			dtq[dtqid].r = (VW)dtq[dtqid].dtq;
		return E_OK;
	}

	if (tmout == TMO_POL)
		return E_TMOUT;
	if (dtq[dtqid].dtqatr == TA_TFIFO) {
		ins_fifo(&(dtq[dtqid].wlink_r), &(tsk[c_tskid[apic]].wlink));
	} else {	/* should be TA_TPRI */
		ins_pri(&(dtq[dtqid].wlink_r), &(tsk[c_tskid[apic]].wlink));
	}
	tsk[c_tskid[apic]].p_data = p_data;

	if (tmout == TMO_FEVR)
		sys_slp_tsk(apic);
	else
		sys_tslp_tsk(apic, tmout);
	return E_OK;
}

ER
sys_ref_dtq(W apic, ID dtqid, T_RDTQ* pk_rdtq)
{
	T_LINK*	wlink;
	if (dtqid < 1 || dtqid > MAX_DTQID)
		return E_ID;
	if (dtq[dtqid].act == 0)
		return E_NOEXS;

	/* head of send-wait queue */
	wlink = dtq[dtqid].wlink_w.next;
	if (wlink != &(dtq[dtqid].wlink_w))
		pk_rdtq->stskid = (wlink2tsk(wlink))->tskid;
	else
		pk_rdtq->stskid = TSK_NONE;

	/* head of receive-wait queue */
	wlink = dtq[dtqid].wlink_r.next;
	if (wlink != &(dtq[dtqid].wlink_r))
		pk_rdtq->rtskid = (wlink2tsk(wlink))->tskid;
	else
		pk_rdtq->rtskid = TSK_NONE;

	/* count data currently in ring buffer */
	if (dtq[dtqid].w >= dtq[dtqid].r)
		pk_rdtq->sdtqcnt = dtq[dtqid].w - dtq[dtqid].r;
	else
		pk_rdtq->sdtqcnt = dtq[dtqid].dtqcnt -
			(dtq[dtqid].r - dtq[dtqid].w);
	return E_OK;
}
