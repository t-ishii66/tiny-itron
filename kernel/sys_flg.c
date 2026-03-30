/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"
#include "sys_flg.h"
#include "sys_sem.h"
#include "sys_tsk.h"
#include "sched.h"

/* flg_init -----------------------------------------------------------------*/
void
flg_init(void)
{
	int	i;
	for (i = 0 ; i <= MAX_FLGID ; i ++) {
		flg[i].wlink.prev = &(flg[i].wlink);
		flg[i].wlink.next = &(flg[i].wlink);
		flg[i].act = 0;
	}
}

/* sys_cre_flg --------------------------------------------------------------*/
ER
sys_cre_flg(W apic, ID flgid, T_CFLG* pk_cflg)
{
	/* caller holds kernel_lk */
	if (flgid < 1 || flgid > MAX_FLGID)
		return E_ID;
	if (flg[flgid].act)
		return E_OBJ;

	flg[flgid].flgatr = pk_cflg->flgatr;
	flg[flgid].iflgptn = flg[flgid].flgptn = pk_cflg->iflgptn;
	flg[flgid].act = 1;
	return E_OK;
}

/* sys_acre_flg -------------------------------------------------------------*/
ER_ID
sys_acre_flg(W apic, T_CFLG* pk_cflg)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_FLGID; i++) {
		if (flg[i].act == 0) {
			if ((ret = sys_cre_flg(apic, i, pk_cflg)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_flg --------------------------------------------------------------*/
ER
sys_del_flg(W apic, ID flgid)
{
	T_LINK*	wlink;
	T_TSK*	t;
	/* caller holds kernel_lk */
	if (flgid < 1 || flgid > MAX_FLGID)
		return E_ID;
	if (flg[flgid].act == 0)
		return E_NOEXS;

	wlink = &(flg[flgid].wlink);
	wlink = wlink->next;
	while (wlink != &(flg[flgid].wlink)) {
		t = wlink2tsk(wlink);
		t->tskstat = TTS_RDY;
		sched_ins(t->tskpri, &(t->plink));
		proc_set_return_value(t->proc, E_DLT);
		wlink = wlink->next;
	}

	flg[flgid].wlink.prev = &(flg[flgid].wlink);
	flg[flgid].wlink.next = &(flg[flgid].wlink);
	flg[flgid].act = 0;
	return E_OK;
}

/* sys_set_flg --------------------------------------------------------------*/
ER
sys_set_flg(W apic, ID flgid, FLGPTN setptn)
{
	T_LINK*	wlink;
	T_TSK*	t;
	/* caller holds kernel_lk */
	if (flgid < 1 || flgid > MAX_FLGID)
		return E_ID;
	if (flg[flgid].act == 0)
		return E_NOEXS;
	flg[flgid].flgptn |= setptn;

	wlink = &(flg[flgid].wlink);
	wlink = wlink->next;
	while (wlink != &(flg[flgid].wlink)) {
		t = wlink2tsk(wlink);
		if (((t->wfmode == TWF_ANDW) &&
					((t->waiptn & flg[flgid].flgptn) == t->waiptn)) ||
			((t->wfmode == TWF_ORW) &&
					(t->waiptn & flg[flgid].flgptn))) {
			t->tskstat = TTS_RDY;
			sched_ins(t->tskpri, &(t->plink));
			proc_set_return_value(t->proc, E_OK);
			*(t->p_flgptn) = flg[flgid].flgptn;

			if (flg[flgid].flgatr & TA_CLR) {
				flg[flgid].flgptn = 0;
				break;
			}
		}
		wlink = wlink->next;
	}

	return E_OK;
}

/* sys_iset_flg (not system call) -------------------------------------------*/
ER
sys_iset_flg(W apic, ID flgid, FLGPTN setptn)
{
	return sys_set_flg(apic, flgid, setptn);
}

/* sys_clr_flg --------------------------------------------------------------*/
ER
sys_clr_flg(W apic, ID flgid, FLGPTN clrptn)
{
	/* caller holds kernel_lk */
	if (flgid < 1 || flgid > MAX_FLGID)
		return E_ID;
	if (flg[flgid].act == 0)
		return E_NOEXS;
	flg[flgid].flgptn &= clrptn;
	return E_OK;
}

/* sys_wai_flg --------------------------------------------------------------*/
ER
sys_wai_flg(W apic, ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN* p_flgptn)
{
	return sys_twai_flg(apic, flgid, waiptn, wfmode, p_flgptn, TMO_FEVR);
}

/* sys_pol_flg --------------------------------------------------------------*/
ER
sys_pol_flg(W apic, ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN* p_flgptn)
{
	ER	ret = E_TMOUT;
	/* caller holds kernel_lk */
	if (flgid < 1 || flgid > MAX_FLGID)
		return E_ID;
	if (flg[flgid].act == 0)
		return E_NOEXS;
	if (((wfmode == TWF_ANDW) && ((waiptn & flg[flgid].flgptn) == waiptn)) ||
		((wfmode == TWF_ORW) && (waiptn & flg[flgid].flgptn))) {

		*(p_flgptn) = flg[flgid].flgptn;
		if (flg[flgid].flgatr & TA_CLR) {
			flg[flgid].flgptn = 0;
		}
		ret = E_OK;
	}

	return ret;
}

/* sys_twai_flg -------------------------------------------------------------*/
ER
sys_twai_flg(W apic, ID flgid, FLGPTN waiptn, MODE wfmode,
						FLGPTN* p_flgptn, TMO tmout)
{
	/* caller holds kernel_lk */
	if (flgid < 1 || flgid > MAX_FLGID)
		return E_ID;
	if (flg[flgid].act == 0)
		return E_NOEXS;

	if (((wfmode == TWF_ANDW) && ((waiptn & flg[flgid].flgptn) == waiptn)) ||
		((wfmode == TWF_ORW) && (waiptn & flg[flgid].flgptn))) {

		*(p_flgptn) = flg[flgid].flgptn;
		if (flg[flgid].flgatr & TA_CLR) {
			flg[flgid].flgptn = 0;
		}
		return E_OK;
	}

	/* TA_WMUL not set and already has a waiting task → E_ILUSE */
	if (!(flg[flgid].flgatr & TA_WMUL) &&
	    flg[flgid].wlink.next != &(flg[flgid].wlink))
		return E_ILUSE;

	if (tmout == TMO_POL)
		return E_TMOUT;
	tsk[c_tskid[apic]].waiptn = waiptn;
	tsk[c_tskid[apic]].wfmode = wfmode;
	tsk[c_tskid[apic]].p_flgptn = p_flgptn;

	if (flg[flgid].flgatr == TA_TFIFO) {
		ins_fifo(&(flg[flgid].wlink), &(tsk[c_tskid[apic]].wlink));
	} else {	/* should be TA_TPRI */
		ins_pri(&(flg[flgid].wlink), &(tsk[c_tskid[apic]].wlink));
	}
	if (tmout == TMO_FEVR)
		sys_slp_tsk(apic);
	else
		sys_tslp_tsk(apic, tmout);

	return E_OK;
}

/* sys_ref_flg --------------------------------------------------------------*/
ER
sys_ref_flg(W apic, ID flgid, T_RFLG* pk_rflg)
{
	T_LINK*	wlink;
	T_TSK*	t;

	/* caller holds kernel_lk */
	if (flgid < 1 || flgid > MAX_FLGID)
		return E_ID;
	if (flg[flgid].act == 0)
		return E_NOEXS;
	wlink = &(flg[flgid].wlink);
	wlink = wlink->next;
	if (wlink == &(flg[flgid].wlink))
		pk_rflg->wtskid = TSK_NONE;
	else {
		t = wlink2tsk(wlink);
		pk_rflg->wtskid = t->tskid;
	}
	pk_rflg->flgptn = flg[flgid].flgptn;
	return E_OK;
}
