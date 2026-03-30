/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "sys_tsk.h"
#include "sys_sem.h"
#include "sys_mbx.h"
#include "val.h"
#include "pool.h"
#include "sched.h"

/* mbx_init -----------------------------------------------------------------*/
void
mbx_init(void)
{
	int	i;
	for (i = 1 ; i <= MAX_MBXID ; i ++) {
		mbx[i].wlink.next = &(mbx[i].wlink);
		mbx[i].wlink.prev = &(mbx[i].wlink);
		mbx[i].act = 0;
	} 
}

/* sys_cre_mbx --------------------------------------------------------------*/
ER
sys_cre_mbx(W apic, ID mbxid, T_CMBX* pk_cmbx)
{
	int	i;
	if (mbxid < 1 || mbxid > MAX_MBXID)
		return E_ID;
	if (mbx[mbxid].act)
		return E_OBJ;

	mbx[mbxid].mbxatr = pk_cmbx->mbxatr;
	mbx[mbxid].maxmpri = pk_cmbx->maxmpri;
	mbx[mbxid].mprihd = pk_cmbx->mprihd;
	if (mbx[mbxid].mprihd == NULL) {
		mbx[mbxid].mprihd = kmem_alloc(sizeof(VW) *
						(mbx[mbxid].maxmpri + 1));
		mbx[mbxid].mbx_alloc = 1;
	} else
		mbx[mbxid].mbx_alloc = 0;

	for (i = 1 ; i <= mbx[mbxid].maxmpri ; i ++)
		mbx[mbxid].mprihd[i] = 0;

	mbx[mbxid].act = 1;
	return E_OK;
}

ER_ID
sys_acre_mbx(W apic, T_CMBX* pk_cmbx)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_MBXID; i++) {
		if (mbx[i].act == 0) {
			if ((ret = sys_cre_mbx(apic, i, pk_cmbx)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_mbx --------------------------------------------------------------*/
ER
sys_del_mbx(W apic, ID mbxid)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mbxid < 1 || mbxid > MAX_MBXID)
		return E_ID;
	if (mbx[mbxid].act == 0)
		return E_NOEXS;

	/* release all waiting tasks with E_DLT */
	wlink = mbx[mbxid].wlink.next;
	while (wlink != &(mbx[mbxid].wlink)) {
		t = wlink2tsk(wlink);
		t->tskstat = TTS_RDY;
		proc_set_return_value(t->proc, E_DLT);
		sched_ins(t->tskpri, &(t->plink));
		wlink = wlink->next;
	}

	if (mbx[mbxid].mbx_alloc)
		kmem_free(mbx[mbxid].mprihd);
	mbx[mbxid].wlink.next = &(mbx[mbxid].wlink);
	mbx[mbxid].wlink.prev = &(mbx[mbxid].wlink);
	mbx[mbxid].act = 0;

	return E_OK;
}

/* sys_snd_mbx --------------------------------------------------------------*/
ER
sys_snd_mbx(W apic, ID mbxid, T_MSG* pk_msg)
{
	PRI	pri;
	T_LINK*	wlink;
	T_TSK*	t;
	if (mbxid < 1 || mbxid > MAX_MBXID)
		return E_ID;
	if (mbx[mbxid].act == 0)
		return E_NOEXS;
	
	wlink = mbx[mbxid].wlink.next;
	if (wlink != &(mbx[mbxid].wlink)) { /* waiting */
		t = wlink2tsk(wlink); 
		*(t->ppk_msg) = pk_msg;
		t->tskstat = TTS_RDY;
		sched_ins(t->tskpri, &(t->plink));
		wlink_rem(wlink);
	} else {
		if (mbx[mbxid].mbxatr == TA_MFIFO) {
			mbx_ins(mbxid, 1, pk_msg);
		} else { /* should be TA_MPRI */
			pri = ((T_MSG_PRI*)pk_msg)->msgpri;
			mbx_ins(mbxid, pri, pk_msg);
		}
	}
	return E_OK;
}

/* sys_rcv_mbx --------------------------------------------------------------*/
ER
sys_rcv_mbx(W apic, ID mbxid, T_MSG** ppk_msg)
{
	return sys_trcv_mbx(apic, mbxid, ppk_msg, TMO_FEVR);
}

/* sys_prcv_mbx -------------------------------------------------------------*/
ER
sys_prcv_mbx(W apic, ID mbxid, T_MSG** ppk_msg)
{
	PRI	i;
	T_MSG*	m;
	if (mbxid < 1 || mbxid > MAX_MBXID)
		return E_ID;
	if (mbx[mbxid].act == 0)
		return E_NOEXS;
	
	for (i = 1 ; i < mbx[mbxid].maxmpri ; i ++) {
		if (mbx[mbxid].mprihd[i] != NULL) {
			m = (T_MSG*)(mbx[mbxid].mprihd[i]);
			mbx_rem(mbxid, i);
			*ppk_msg = m;
			return E_OK;
		}
	}
	return E_TMOUT;
}

/* sys_trcv_mbx -------------------------------------------------------------*/
ER
sys_trcv_mbx(W apic, ID mbxid, T_MSG** ppk_msg, TMO tmout)
{
	PRI	i;
	T_MSG*	m;
	if (mbxid < 1 || mbxid > MAX_MBXID)
		return E_ID;
	if (mbx[mbxid].act == 0)
		return E_NOEXS;
	
	for (i = 1 ; i < mbx[mbxid].maxmpri ; i ++) {
		if (mbx[mbxid].mprihd[i] != NULL) {
			m = mbx[mbxid].mprihd[i];
			mbx_rem(mbxid, i);
			*ppk_msg = m;
			return E_OK;
		}
	}

	if (tmout == TMO_POL)
		return E_TMOUT;

	/* save receive destination so snd_mbx can deliver the message */
	tsk[c_tskid[apic]].ppk_msg = ppk_msg;

	if (mbx[mbxid].mbxatr == TA_TFIFO) {
		ins_fifo(&(mbx[mbxid].wlink), &(tsk[c_tskid[apic]].wlink));
	} else {	/* should be TA_TPRI */
		ins_pri(&(mbx[mbxid].wlink), &(tsk[c_tskid[apic]].wlink));
	}
	if (tmout == TMO_FEVR)
		sys_slp_tsk(apic);
	else
		sys_tslp_tsk(apic, tmout);
	return E_OK;
}

/* sys_ref_mbx --------------------------------------------------------------*/
ER
sys_ref_mbx(W apic, ID mbxid, T_RMBX* pk_rmbx)
{
	T_LINK*	wlink;
	PRI	i;
	T_MSG*	m;
	if (mbxid < 1 || mbxid > MAX_MBXID)
		return E_ID;
	if (mbx[mbxid].act == 0)
		return E_NOEXS;

	wlink = mbx[mbxid].wlink.next;
	if (wlink != &(mbx[mbxid].wlink)) { /* waiting */
		pk_rmbx->wtskid = (wlink2tsk(wlink))->tskid;
	} else {
		pk_rmbx->wtskid = TSK_NONE;
	}
	m = NULL;
	for (i = 1 ; i < mbx[mbxid].maxmpri ; i ++) {
		if (mbx[mbxid].mprihd[i] != NULL) {
			m = (T_MSG*)mbx[mbxid].mprihd[i];
			break;
		}
	}
	pk_rmbx->pk_msg = m;

	return E_OK;
}

/* mbx_ins_pri --------------------------------------------------------------*/
void
mbx_ins(ID mbxid, PRI pri, T_MSG* pk_msg)
{
	T_MSG*	m;
	m = (T_MSG*)mbx[mbxid].mprihd[pri];
	if (m == NULL) {
		*(T_MSG**)&mbx[mbxid].mprihd[pri] = pk_msg;
		*pk_msg = 0;
	} else {
		while (*m != NULL)
			m = (T_MSG*)*m;
		*m = pk_msg;
		*pk_msg = 0;
	}
}

/* mbx_rem ------------------------------------------------------------------*/
void
mbx_rem(ID mbxid, PRI pri)
{
	T_MSG*	m;
	if (mbx[mbxid].mprihd[pri] == NULL)
		return;
	m = mbx[mbxid].mprihd[pri];
	mbx[mbxid].mprihd[pri] = (T_MSG*)*m;
}
