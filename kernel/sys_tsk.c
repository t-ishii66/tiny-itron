/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/types.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "../i386/video.h"
#include "types.h"
#include "val.h"
#include "sched.h"

/* tsk_init (not system call) (only called by initialize process) -----------*/
void
tsk_init(void)
{
	H	i;
	for (i = 0 ; i < MAX_TSKID ; i ++) {
		tsk[i].wupcnt = 0;
		tsk[i].suscnt = 0;
		tsk[i].tskstat = TTS_NON;
		/* self-referencing = "not in any queue" */
		tsk[i].wlink.next = &(tsk[i].wlink);
		tsk[i].wlink.prev = &(tsk[i].wlink);
		tsk[i].tlink.next = &(tsk[i].tlink);
		tsk[i].tlink.prev = &(tsk[i].tlink);
	}
}

/* tsk_stat_change (not system call) (only called by initialize process) ----*/
void
tsk_stat_change(ID tskid, STAT s)
{
	tsk[tskid].tskstat = s;
}

/* sys_cre_tsk --------------------------------------------------------------*/
ER
sys_cre_tsk(W apic, ID tskid, T_CTSK* pk_ctsk)
{
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat != TTS_NON)
		return E_OBJ;

	tsk[tskid].tskid = tskid;
	/* use proc_create --------------------------------------------------*/
	tsk[tskid].proc = proc_create(tskid, pk_ctsk);
	if (pk_ctsk->tskatr & TA_ACT) {
		tsk[tskid].tskstat = TTS_RDY;		/* initial status */
	} else
		tsk[tskid].tskstat = TTS_DMT;		/* initial status */
	tsk[tskid].tskbpri = pk_ctsk->itskpri;	/* initial priority */
	tsk[tskid].tskpri = tsk[tskid].tskbpri;
	tsk[tskid].actcnt = 0;
	tsk[tskid].wupcnt = 0;
	tsk[tskid].suscnt = 0;
	tsk[tskid].ctsk.tskatr = pk_ctsk->tskatr; /* copy initial argument */
	tsk[tskid].ctsk.exinf = pk_ctsk->exinf;
	tsk[tskid].ctsk.task = pk_ctsk->task;
	tsk[tskid].ctsk.itskpri = pk_ctsk->itskpri;
	tsk[tskid].ctsk.stksz = pk_ctsk->stksz;
	tsk[tskid].ctsk.stk = pk_ctsk->stk;

	tsk[tskid].waiptn = 0;

	tsk[tskid].pndptn = 0;
	tsk[tskid].texstat = TTEX_DIS;
	tsk[tskid].tex.texatr = 0;
	tsk[tskid].tex.texrtn = 0;

	tsk[tskid].mtxcnt = 0;

	tsk[tskid].ovrhdr = 0;
	tsk[tskid].ovrstat = TOVR_STP;
	return E_OK;
}

/* sys_acre_tsk -------------------------------------------------------------*/
ER_ID
sys_acre_tsk(W apic, T_CTSK* pk_ctsk)
{
	ER_ID	i;
	ER	ret;
	for (i = 1 ; i < MAX_TSKID ; i ++) {
		if (tsk[i].tskstat == TTS_NON) {
			if ((ret = cre_tsk(i, pk_ctsk)) < 0)
				return ret; 
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_tsk --------------------------------------------------------------*/
ER
sys_del_tsk(W apic, ID tskid)
{
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	if (tsk[tskid].tskstat != TTS_DMT)
		return E_OBJ;
	proc_delete(tskid);
	tsk[tskid].tskstat = TTS_NON;
	return E_OK;
}

/* sys_act_tsk --------------------------------------------------------------*/
ER
sys_act_tsk(W apic, ID tskid)
{
	if (tskid == 0)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	if (tsk[tskid].tskstat != TTS_DMT) {
		if (tsk[tskid].actcnt < TMAX_ACTCNT) {
			tsk[tskid].actcnt ++;
			return E_OK;
		}
		return E_QOVR;		
	}
	tsk[tskid].tskstat = TTS_RDY;
	sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink);
	return E_OK;
}

/* sys_iact_tsk (none system call) ------------------------------------------*/
ER
sys_iact_tsk(W apic, ID tskid)
{
	if (tskid == 0)
		return E_ID;
	return sys_act_tsk(apic, tskid);	
}

/* sys_can_act --------------------------------------------------------------*/
ER
sys_can_act(W apic, ID tskid)
{
	UINT	wakup;

	if (tskid == 0)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	wakup = tsk[tskid].actcnt;
	tsk[tskid].actcnt = 0;
	return (ER)wakup;	
}

/* sys_sta_tsk --------------------------------------------------------------*/
ER
sys_sta_tsk(W apic, ID tskid, VP_INT stacd)
{
	if (tskid == 0)
		return E_OBJ;
	proc_set_tsk_arg(tskid, stacd);
	return sys_act_tsk(apic, tskid);
}

/* sys_ext_tsk --------------------------------------------------------------*/
void
sys_ext_tsk(W apic)
{
	tsk[c_tskid[apic]].tskstat = TTS_DMT;
	sched_rem(&tsk[c_tskid[apic]].plink);
	if (tsk[c_tskid[apic]].actcnt) {
		tsk[c_tskid[apic]].actcnt --;
		tsk[c_tskid[apic]].tskstat = TTS_NON;
		sys_cre_tsk(apic, c_tskid[apic], &tsk[c_tskid[apic]].ctsk);
		sys_act_tsk(apic, c_tskid[apic]);
		return;
	}
	tsk[c_tskid[apic]].tskstat = TTS_NON;
	sys_cre_tsk(apic, c_tskid[apic], &tsk[c_tskid[apic]].ctsk);
	tsk[c_tskid[apic]].tskstat = TTS_DMT;

	/* find another task */
	sched_next_tsk(apic);	
}

/* sys_exd_tsk --------------------------------------------------------------*/
void
sys_exd_tsk(W apic)
{
	sched_rem(&tsk[c_tskid[apic]].plink);
	tsk[c_tskid[apic]].tskstat = TTS_NON;
	proc_delete(c_tskid[apic]);
	sched_next_tsk(apic);	
}

/* sys_ter_tsk --------------------------------------------------------------*/
ER
sys_ter_tsk(W apic, ID tskid)
{
	if (tskid == 0)
		return E_ILUSE;
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	if (tsk[tskid].tskstat == TTS_DMT)
		return E_OBJ;

	tsk[tskid].tskstat = TTS_DMT;
	sched_rem(&tsk[tskid].plink);
	/* must delete waiting queue */

	if (tsk[tskid].actcnt) {
		tsk[tskid].actcnt --;
		tsk[tskid].tskstat = TTS_NON;
		sys_cre_tsk(apic, tskid, &tsk[tskid].ctsk);
		return sys_act_tsk(apic, tskid);
	}
	tsk[tskid].tskstat = TTS_NON;
	sys_cre_tsk(apic, tskid, &tsk[tskid].ctsk);
	tsk[tskid].tskstat = TTS_DMT;
	return E_OK;
}

/* sys_chg_pri --------------------------------------------------------------*/
ER
sys_chg_pri(W apic, ID tskid, PRI tskpri)
{
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tskpri < 0 || tskpri > TMAX_TPRI)
		return E_PAR;

	if (tskpri == TPRI_INI)
		tskpri = tsk[tskid].ctsk.itskpri;
	
	if (tsk[tskid].tskpri != tskpri) {
		sched_rem(&tsk[tskid].plink);
	}
	tsk[tskid].tskpri = tskpri;
	sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink);
	return E_OK;
}

/* sys_get_pri --------------------------------------------------------------*/
ER
sys_get_pri(W apic, ID tskid, PRI* p_tskpri)
{
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	*p_tskpri = tsk[tskid].tskpri;
	return E_OK;	
}

/* sys_ref_tsk --------------------------------------------------------------*/
ER
sys_ref_tsk(W apic, ID tskid, T_RTSK* pk_rtsk)
{
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	
	pk_rtsk->tskstat = tsk[tskid].tskstat;
	pk_rtsk->tskpri = tsk[tskid].tskpri;
	pk_rtsk->tskbpri = tsk[tskid].tskbpri;
	pk_rtsk->tskwait = 0;		/* ? */
	pk_rtsk->wobjid = 0;		/* ? */
	pk_rtsk->lefttmo = 0;		/* ? */
	pk_rtsk->actcnt = tsk[tskid].actcnt;
	pk_rtsk->wupcnt = tsk[tskid].wupcnt;
	pk_rtsk->suscnt = tsk[tskid].suscnt;

	return E_OK;
}

/* sys_ref_tst --------------------------------------------------------------*/
ER
sys_ref_tst(W apic, ID tskid, T_RTST* pk_rtst)
{
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;

	pk_rtst->tskstat = tsk[tskid].tskstat;
	pk_rtst->tskwait = /* tsk[tskid].tskwait; */ 0;
	return E_OK;
}

/* sys_slp_tsk --------------------------------------------------------------*/
ER
sys_slp_tsk(W apic)
{
	ID	tskid = c_tskid[apic];

	/* caller holds kernel_lk */
	if (tsk[tskid].wupcnt >= 1) {
		tsk[tskid].wupcnt --;
		return E_OK;
	}
	sched_rem(&tsk[tskid].plink);
	tsk[tskid].tskstat = TTS_WAI;
	sched_next_tsk(apic);
	return E_OK;
}

/* sys_tslp_tsk -------------------------------------------------------------*/
ER
sys_tslp_tsk(W apic, TMO tmout)
{
	ID	tskid = c_tskid[apic];

	/* caller holds kernel_lk */
	if (tsk[tskid].wupcnt >= 1) {
		tsk[tskid].wupcnt --;
		return E_OK;
	}
	tsk[tskid].tlink.delta = tmout;
	sched_rem(&tsk[tskid].plink);
	tsk[tskid].tskstat = TTS_WAI;
	sched_timeout_ins(&tsk[tskid].tlink);
	sched_next_tsk(apic);
	return E_OK;
}

/* sys_wup_tsk --------------------------------------------------------------*/
ER
sys_wup_tsk(W apic, ID tskid)
{
	int	flag = 1;
	if (tskid == TSK_SELF) {
		tskid = c_tskid[apic];
		flag = 0;
	}

	/* caller holds kernel_lk */
	if (tsk[tskid].tskstat != TTS_WAI) {
		if (tsk[tskid].tskstat == TTS_NON)
			return E_NOEXS;
		if (tsk[tskid].tskstat == TTS_DMT)
			return E_OBJ;
		if (tsk[tskid].wupcnt > TMAX_WUPCNT)
			return E_QOVR;
		tsk[tskid].wupcnt ++;
		return E_OK;
	}
	tsk[tskid].tskstat = TTS_RDY;
	if (flag) { /* normal -----------------------------------------------*/
		tsk[c_tskid[apic]].tskstat = TTS_RDY;
		sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink);
	}
	sched_timeout_rem_if_exist(&tsk[tskid].tlink);
	sched_next_tsk(apic);
	return E_OK;
}

/* iwup_tsk (not system call) -----------------------------------------------*/
ER
iwup_tsk(W apic, ID tskid)
{
	/* caller holds kernel_lk */
	if (tsk[tskid].tskstat != TTS_WAI) {
		if (tsk[tskid].tskstat == TTS_NON)
			return E_NOEXS;
		if (tsk[tskid].tskstat == TTS_DMT)
			return E_OBJ;
		if (tsk[tskid].wupcnt > TMAX_WUPCNT)
			return E_QOVR;
		tsk[tskid].wupcnt ++;
		return E_OK;
	}
	tsk[tskid].tskstat = TTS_RDY;
	tsk[c_tskid[apic]].tskstat = TTS_RDY;
	sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink);
	sched_timeout_rem_if_exist(&tsk[tskid].tlink);
	sched_next_tsk(apic);
	return E_OK;
}

/* sys_can_wup --------------------------------------------------------------*/
ER_UINT
sys_can_wup(W apic, ID tskid)
{
	ER_UINT	cnt;
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	if (tsk[tskid].tskstat == TTS_DMT)
		return E_OBJ;

	cnt = tsk[tskid].wupcnt;
	tsk[tskid].wupcnt = 0;
	return cnt;
}

/* sys_rel_wai --------------------------------------------------------------*/
ER
sys_rel_wai(W apic, ID tskid)
{
	if (tskid == TSK_SELF)
		return E_OBJ;
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	if (tsk[tskid].tskstat == TTS_DMT)
		return E_OBJ;

	if (tsk[tskid].tskstat == TTS_WAI) {
		proc_set_return_value(tsk[tskid].proc, E_RLWAI);
		wlink_rem(&tsk[tskid].wlink);
		tsk[tskid].tskstat = TTS_RDY;
		sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink);
	} else if (tsk[tskid].tskstat == TTS_WAS) {
		proc_set_return_value(tsk[tskid].proc, E_RLWAI);
		wlink_rem(&tsk[tskid].wlink);
		tsk[tskid].tskstat = TTS_SUS;
	}
	return E_OK;
}

/* sys_irel_wai -------------------------------------------------------------*/
ER
sys_irel_wai(W apic, ID tskid)
{
	return sys_rel_wai(apic, tskid);
}

/* sys_sus_tsk --------------------------------------------------------------*/
ER
sys_sus_tsk(W apic, ID tskid)
{
	unsigned long	k;
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	if (tsk[tskid].tskstat == TTS_DMT)
		return E_OBJ;
	if (tsk[tskid].suscnt < TMAX_SUSCNT)
		tsk[tskid].suscnt ++;
	else
		return E_QOVR;

	if (tsk[tskid].tskstat == TTS_RUN) {
/*
		sched_(tskid, TTS_RDY);
*/
		for (k = 0 ; k < 100000 ; k ++) {
			if (tsk[tskid].tskstat != TTS_RUN)
				break;
		}
	}
	if (tsk[tskid].tskstat == TTS_RDY) {
		tsk[tskid].tskstat = TTS_SUS;
	} else if (tsk[tskid].tskstat == TTS_WAI) {
		tsk[tskid].tskstat = TTS_WAS;
	}
	sched_rem(&tsk[tskid].plink);
	/* CAUTION: task may be running on another CPU. */
	/* timer handler must check tskstat and if taskstat is not TTS_RUN */
	/* dispatch routine must be called. */

	return E_OK;
}

/* sys_rsm_tsk --------------------------------------------------------------*/
ER
sys_rsm_tsk(W apic, ID tskid)
{
	if (tskid == TSK_SELF)
		return E_OBJ;
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	if (tsk[tskid].suscnt == 0)
		return E_OBJ;
	tsk[tskid].suscnt--;
	if (tsk[tskid].suscnt == 0) {
		if (tsk[tskid].tskstat == TTS_SUS) {
			tsk[tskid].tskstat = TTS_RDY;
			sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink);
		} else if (tsk[tskid].tskstat == TTS_WAS) {
			tsk[tskid].tskstat = TTS_WAI;
		}
	}
	return E_OK;
}

/* sys_frsm_tsk -------------------------------------------------------------*/
ER
sys_frsm_tsk(W apic, ID tskid)
{
	if (tskid == TSK_SELF)
		return E_OBJ;
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	if (tsk[tskid].suscnt == 0)
		return E_OBJ;
	tsk[tskid].suscnt = 0;
	if (tsk[tskid].tskstat == TTS_SUS) {
		tsk[tskid].tskstat = TTS_RDY;
		sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink);
	} else if (tsk[tskid].tskstat == TTS_WAS) {
		tsk[tskid].tskstat = TTS_WAI;
	}
	return E_OK;
}

/* sys_dly_tsk --------------------------------------------------------------*/
ER
sys_dly_tsk(W apic, RELTIM dlytim)
{
	ID	tskid = c_tskid[apic];
	if (dlytim == 0)
		return E_OK;
	tsk[tskid].tlink.delta = dlytim;
	sched_rem(&tsk[tskid].plink);
	tsk[tskid].tskstat = TTS_WAI;
	sched_timeout_ins(&tsk[tskid].tlink);
	sched_next_tsk(apic);
	return E_OK;
}

/* sys_printf ---------------------------------------------------------------*/
ER
sys_printf(W apic, char** sp)
{
	printk2(sp);
	return E_OK;
}

/*===========================================================================*/
void
tsk_dump(void)
{
	int	i;
	printk("+ tsk dump ----------------------------------------------+\n");
	for (i = 0 ; i < MAX_TSKID ; i ++) {
		if (tsk[i].tskstat != TTS_NON) {
			printk("tsk[%d]: stat=%x\n", i, tsk[i].tskstat);	
		}
	}
}
