/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"
#include "sched.h"

/* sys_rot_rdq --------------------------------------------------------------*/
ER
sys_rot_rdq(W apic, PRI tskpri)
{
	T_LINK	*link;
	if (tskpri == TPRI_SELF)
		tskpri = tsk[c_tskid[apic]].tskpri;
	if (tskpri < TMIN_TPRI || tskpri > TMAX_TPRI)
		return E_PAR;
	/* Move head of the specified priority queue to tail */
	link = tsk_pri[tskpri].next;
	if (link != &tsk_pri[tskpri]) {
		sched_rem(link);
		sched_ins(tskpri, link);
	}
	return E_OK;
}

/* sys_irot_rdq (not system call) -------------------------------------------*/
ER
sys_irot_rdq(W apic, PRI tskpri)
{
	return sys_rot_rdq(apic, tskpri);
}

/* sys_get_tid --------------------------------------------------------------*/
ER
sys_get_tid(W apic, ID* p_tskid)
{
	*p_tskid = c_tskid[apic];
	return E_OK;
}

/* sys_iget_tid (not system call) -------------------------------------------*/
ER
sys_iget_tid(W apic, ID* p_tskid)
{
	*p_tskid = c_tskid[apic];
	return E_OK;
}

/* sys_loc_cpu --------------------------------------------------------------*/
ER
sys_loc_cpu(W apic)
{
	ccli();		/* disable interrupt */
	return E_OK;
}

/* sys_iloc_cpu (not system call) -------------------------------------------*/
ER
sys_iloc_cpu(W apic)
{
	return sys_loc_cpu(apic);
}

/* sys_unl_cpu --------------------------------------------------------------*/
ER
sys_unl_cpu(W apic)
{
	csti();
	return E_OK;
}

/* sys_iunl_cpu (not system call) -------------------------------------------*/
ER
sys_iunl_cpu(W apic)
{
	return sys_unl_cpu(apic);
}

/* sys_dis_dsp --------------------------------------------------------------*/
ER
sys_dis_dsp(W apic)
{
	dispatch_stat = DISPATCH_DISABLE;
	return E_OK;
}

/* sys_ena_dsp --------------------------------------------------------------*/
ER
sys_ena_dsp(W apic)
{
	dispatch_stat = DISPATCH_ENABLE;
	return E_OK;
}

/* sys_sns_ctx --------------------------------------------------------------*/
extern unsigned long k_nest0, k_nest1;

BOOL
sys_sns_ctx(W apic)
{
	if (apic == 0)
		return (k_nest0 > 0) ? TRUE : FALSE;
	else
		return (k_nest1 > 0) ? TRUE : FALSE;
}

/* sys_sns_loc --------------------------------------------------------------*/
BOOL
sys_sns_loc(W apic)
{
	if (cpu_stat == CPU_LOCK)
		return TRUE;
	return FALSE;
}

/* sys_sns_dsp --------------------------------------------------------------*/
BOOL
sys_sns_dsp(W apic)
{
	if (dispatch_stat == DISPATCH_DISABLE)
		return TRUE;
	return FALSE;
}

/* sys_sns_dpn --------------------------------------------------------------*/
BOOL
sys_sns_dpn(W apic)
{
	return FALSE;
}

/* sys_ref_sys --------------------------------------------------------------*/
ER
sys_ref_sys(W apic, T_RSYS* pk_rsys)
{
	return E_OK;
}
