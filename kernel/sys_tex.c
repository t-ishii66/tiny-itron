/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/types.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"

/* sys_def_tex --------------------------------------------------------------*/
ER
sys_def_tex(W apic, ID tskid, T_DTEX* pk_dtex)
{
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	if (tsk[tskid].tskstat == TTS_DMT)
		return E_OBJ;

	if (pk_dtex == NULL) {
		tsk[tskid].tex.texatr = NULL;
		tsk[tskid].tex.texrtn = NULL;
		tsk[tskid].tex.exinf = NULL;
		tsk[tskid].texstat = TTEX_DIS;
	} else {
		tsk[tskid].tex.texatr = pk_dtex->texatr;
		tsk[tskid].tex.texrtn = pk_dtex->texrtn;
		tsk[tskid].tex.exinf = pk_dtex->exinf;
	}
	return E_OK;	
}

/* sys_ras_tex --------------------------------------------------------------*/
ER
sys_ras_tex(W apic, ID tskid, TEXPTN rasptn)
{
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	if (tsk[tskid].tskstat == TTS_DMT || tsk[tskid].tex.texrtn == NULL)
		return E_OBJ;
	if (rasptn == 0)
		return E_PAR;
	tsk[tskid].pndptn |= rasptn;
	return E_OK;
}

/* sys_iras_tex -------------------------------------------------------------*/
ER
sys_iras_tex(W apic, ID tskid, TEXPTN rasptn)
{
	return sys_ras_tex(apic, tskid, rasptn);
}

/* sys_dis_tex --------------------------------------------------------------*/
ER
sys_dis_tex(W apic)
{
	ID	tskid = c_tskid[apic];
	if (tsk[tskid].tex.texrtn == NULL)
		return E_OBJ;
	tsk[tskid].texstat = TTEX_DIS;
	return E_OK;
}

/* sys_ena_tex --------------------------------------------------------------*/
ER
sys_ena_tex(W apic)
{
	ID	tskid = c_tskid[apic];
	if (tsk[tskid].tex.texrtn == NULL)
		return E_OBJ;
	tsk[tskid].texstat = TTEX_ENA;
	/* execute exception handler */
	if (tsk[tskid].pndptn) {
		stack_adjust(apic, tsk[tskid].tex.texrtn,
				tsk[tskid].pndptn, tsk[tskid].tex.exinf);
		tsk[tskid].pndptn = 0;
	}
	return E_OK;
}

/* sys_sns_tex --------------------------------------------------------------*/
BOOL
sys_sns_tex(W apic)
{
	ID	tskid = c_tskid[apic];
	if (tsk[tskid].texstat == TTEX_DIS)
		return TRUE;	
	else
		return FALSE;
}

/* sys_ref_tex --------------------------------------------------------------*/
ER
sys_ref_tex(W apic, ID tskid, T_RTEX* pk_rtex)
{
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];		
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	if (tsk[tskid].tskstat == TTS_NON)
		return E_NOEXS;
	if (tsk[tskid].tskstat == TTS_DMT || tsk[tskid].tex.texrtn == NULL)
		return E_OBJ;
	pk_rtex->texstat = tsk[tskid].texstat;
	pk_rtex->pndptn = tsk[tskid].pndptn;
	return E_OK;
}


/* check_tex (called by exception handler) ----------------------------------*/
void
check_tex(W apic)
{
	ID	tskid = c_tskid[apic];

	if (tsk[tskid].pndptn && tsk[tskid].texstat == TTEX_ENA) {
		tsk[tskid].texstat = TTEX_DIS;
		stack_adjust(apic, tsk[tskid].tex.texrtn,
				tsk[tskid].pndptn, tsk[tskid].tex.exinf);
		tsk[tskid].pndptn = 0;
	}
}
