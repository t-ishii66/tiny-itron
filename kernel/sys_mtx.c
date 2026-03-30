/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"

/* mtx_init -----------------------------------------------------------------*/
void
mtx_init(void)
{
	int	i;
	for (i = 1 ; i < MAX_MTXID ; i ++) {
		mtx[i].wlink.next = &(mtx[i].wlink);
		mtx[i].wlink.prev = &(mtx[i].wlink);
		mtx[i].act = 0;
	}
}

/* sys_cre_mtx --------------------------------------------------------------*/
ER
sys_cre_mtx(W apic, ID mtxid, T_CMTX* pk_cmtx)
{
	if (mtxid < 1 || mtxid > MAX_MTXID)
		return E_ID;
	if (mtx[mtxid].act)
		return E_OBJ;

	mtx[mtxid].mtxatr = pk_cmtx->mtxatr;
	mtx[mtxid].ceilpri = pk_cmtx->ceilpri;
	mtx[mtxid].act = 1;
	return E_OK;
}

/* sys_acre_mtx -------------------------------------------------------------*/
ER_ID
sys_acre_mtx(W apic, T_CMTX* pk_cmtx)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_MTXID; i++) {
		if (mtx[i].act == 0) {
			if ((ret = sys_cre_mtx(apic, i, pk_cmtx)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_mtx --------------------------------------------------------------*/
ER
sys_del_mtx(W apic, ID mtxid)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mtxid < 1 || mtxid > MAX_MTXID)
		return E_ID;
	if (mtx[mtxid].act == 0)
		return E_NOEXS;

	/* if mtx[mtxid] is locked priority change is requierd */

	wlink = mtx[mtxid].wlink.next;
	while (wlink != &(mtx[mtxid].wlink)) {
		t = wlink2tsk(wlink);
		t->tskstat = TTS_RDY;
		proc_set_return_value(t->proc, E_DLT);
		sched_ins(t->tskpri, &(t->plink));
		wlink = wlink->next;
	}
	mtx[mtxid].act = 0;
	return E_OK;
}

/* sys_loc_mtx --------------------------------------------------------------*/
ER
sys_loc_mtx(W apic, ID mtxid)
{
	return sys_tloc_mtx(apic, mtxid, TMO_FEVR);
}

/* sys_ploc_mtx -------------------------------------------------------------*/
ER
sys_ploc_mtx(W apic, ID mtxid)
{
	return sys_tloc_mtx(apic, mtxid, TMO_POL);
}

/* sys_tloc_mtx -------------------------------------------------------------*/
ER
sys_tloc_mtx(W apic, ID mtxid, TMO tmout)
{
	T_TSK*	t;
	if (mtxid < 1 || mtxid > MAX_MTXID)
		return E_ID;
	if (mtx[mtxid].act == 0)
		return E_NOEXS;
	
	if (mtx[mtxid].mtxlock == 0) {
		mtx[mtxid].mtxlock = 1;
		mtx[mtxid].tskid = c_tskid[apic];	/* record owner */
		tsk[c_tskid[apic]].mtxcnt ++;
		return E_OK;
	}
	if (mtx[mtxid].tskid == c_tskid[apic])
		return E_ILUSE;
	if (mtx[mtxid].mtxatr & TA_CEILING) {
		if  (tsk[c_tskid[apic]].tskbpri > mtx[mtxid].ceilpri)
			return E_ILUSE;
	}
	if (tmout == TMO_POL)
		return E_TMOUT;

	if (mtx[mtxid].mtxatr & TA_TFIFO) {
		ins_fifo(&(mtx[mtxid].wlink), &(tsk[c_tskid[apic]].wlink));
	} else {	/* should be TA_TPRI */
		ins_pri(&(mtx[mtxid].wlink), &(tsk[c_tskid[apic]].wlink));
	}
	/* priority inheritance: boost holder if waiter has higher priority
	 * (smaller numeric value = higher priority in µITRON) */
	if (tsk[mtx[mtxid].tskid].tskpri > tsk[c_tskid[apic]].tskpri) {
		t = &tsk[mtx[mtxid].tskid];
		t->tskpri = tsk[c_tskid[apic]].tskpri;
		/* caution --------------------------------------------------*/
		sched_rem(&(t->plink));
		sched_ins(t->tskpri, &(t->plink));
	}
	if (tmout == TMO_FEVR)
		sys_slp_tsk(apic);
	else
		sys_tslp_tsk(apic, tmout);

	return E_OK;
}

/* sys_unl_mtx --------------------------------------------------------------*/
ER
sys_unl_mtx(W apic, ID mtxid)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mtxid < 1 || mtxid > MAX_MTXID)
		return E_ID;
	if (mtx[mtxid].act == 0)
		return E_NOEXS;

	if (mtx[mtxid].mtxlock == 0)
		return E_ILUSE;
	if (mtx[mtxid].tskid != c_tskid[apic])	/* only owner can unlock */
		return E_ILUSE;
	wlink = mtx[mtxid].wlink.next;
	if (wlink != &(mtx[mtxid].wlink)) {
		t = wlink2tsk(wlink);
		mtx[mtxid].mtxlock = 1;
		mtx[mtxid].tskid = t->tskid;
		t->mtxcnt ++;
		wlink_rem(wlink);
		t->tskstat = TTS_RDY;		/* wake waiting task */
		proc_set_return_value(t->proc, E_OK);
		sched_ins(t->tskpri, &(t->plink));
	} else {
		mtx[mtxid].mtxlock = 0;		/* no waiter: fully unlock */
	}
	t = &(tsk[c_tskid[apic]]);
	t->mtxcnt --;
	if (t->mtxcnt == 0) {
		if (t->tskbpri != t->tskpri) {
			t->tskpri = t->tskbpri;
			sched_rem(&(t->plink));
			sched_ins(t->tskpri, &(t->plink));
		}
	}
	return E_OK;
}

/* sys_ref_mtx --------------------------------------------------------------*/
ER
sys_ref_mtx(W apic, ID mtxid, T_RMTX* pk_rmtx)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mtxid < 1 || mtxid > MAX_MTXID)
		return E_ID;
	if (mtx[mtxid].act == 0)
		return E_NOEXS;

	if (mtx[mtxid].mtxlock == 0)
		pk_rmtx->htskid = TSK_NONE;
	else
		pk_rmtx->htskid = mtx[mtxid].tskid;

	wlink = mtx[mtxid].wlink.next;
	if (wlink != &(mtx[mtxid].wlink)) {
		t = wlink2tsk(wlink);
		pk_rmtx->wtskid = t->tskid;
	} else
		pk_rmtx->wtskid = TSK_NONE;
	return E_OK;
}
