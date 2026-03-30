/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"

/* ovr_init -----------------------------------------------------------------*/
void
ovr_init(void)
{
}

/* sys_def_ovr --------------------------------------------------------------*/
ER
sys_def_ovr(W apic, T_DOVR* pk_dovr)
{
	if (pk_dovr == NULL) {
		/* NULL means cancel overrun handler (spec requirement) */
		tsk[c_tskid[apic]].ovratr = 0;
		tsk[c_tskid[apic]].ovrhdr = 0;
		return E_OK;
	}
	tsk[c_tskid[apic]].ovratr = pk_dovr->ovratr;
	tsk[c_tskid[apic]].ovrhdr = pk_dovr->ovrhdr;
	return E_OK;
}

/* sys_sta_ovr --------------------------------------------------------------*/
ER
sys_sta_ovr(W apic, ID tskid, OVRTIM ovrtim)
{
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	tsk[tskid].ovrtim = ovrtim;
	tsk[tskid].ovrstat = TOVR_STA;

	return E_OK;
}

/* sys_stp_ovr --------------------------------------------------------------*/
ER
sys_stp_ovr(W apic, ID tskid)
{
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;
	tsk[tskid].ovrstat = TOVR_STP;
	return E_OK;
}

/* sys_ref_ovr --------------------------------------------------------------*/
ER
sys_ref_ovr(W apic, ID tskid, T_ROVR* pk_rovr)
{
	if (tskid == TSK_SELF)
		tskid = c_tskid[apic];
	if (tskid < 1 || tskid > MAX_TSKID)
		return E_ID;

	pk_rovr->ovrstat = tsk[tskid].ovrstat;
	pk_rovr->leftotm = tsk[tskid].ovrtim;

	return E_OK;
}

/*---------------------------------------------------------------------------*/
void
ovr_intr(W apic, unsigned long delta)
{
	ID	tskid = c_tskid[apic];
	if (tsk[tskid].ovrstat == TOVR_STA) {
		tsk[tskid].ovrtim -= delta;
		if (tsk[tskid].ovrtim < 0) {
			tsk[tskid].ovrstat = TOVR_STP;
			(*tsk[tskid].ovrhdr)(tskid, tsk[tskid].exinf);
		}
	}
	return;
}
