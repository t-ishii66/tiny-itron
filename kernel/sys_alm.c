/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"

/* alm_init -----------------------------------------------------------------*/
void
alm_init(void)
{
	int	i;
	for (i = 1 ; i < MAX_ALMID ; i ++) {
		alm[i].act = 0;
	}
}

/* sys_cre_alm --------------------------------------------------------------*/
ER
sys_cre_alm(W apic, ID almid, T_CALM* pk_calm)
{
	if (almid < 1 || almid > MAX_ALMID)
		return E_ID;
	if (alm[almid].act)
		return E_OBJ;

	alm[almid].almatr = pk_calm->almatr;
	alm[almid].exinf = pk_calm->exinf;
	alm[almid].almhdr = pk_calm->almhdr;
	alm[almid].almstat = TALM_STP;
	alm[almid].act = 1;
	return E_OK;	
}

/* sys_acre_alm -------------------------------------------------------------*/
ER_ID
sys_acre_alm(W apic, T_CALM* pk_calm)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_ALMID; i++) {
		if (alm[i].act == 0) {
			if ((ret = sys_cre_alm(apic, i, pk_calm)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_alm --------------------------------------------------------------*/
ER
sys_del_alm(W apic, ID almid)
{
	if (almid < 1 || almid > MAX_ALMID)
		return E_ID;
	if (alm[almid].act == 0)
		return E_NOEXS;

	alm[almid].act = 0;
	return E_OK;
}

/* sys_sta_alm --------------------------------------------------------------*/
ER
sys_sta_alm(W apic, ID almid, RELTIM almtim)
{
	if (almid < 1 || almid > MAX_ALMID)
		return E_ID;
	if (alm[almid].act == 0)
		return E_NOEXS;

	alm[almid].almtim = almtim;
	alm[almid].almstat = TALM_STA;

	return E_OK;
}

/* sys_stp_alm --------------------------------------------------------------*/
ER
sys_stp_alm(W apic, ID almid)
{
	if (almid < 1 || almid > MAX_ALMID)
		return E_ID;
	if (alm[almid].act == 0)
		return E_NOEXS;

	alm[almid].almstat = TALM_STP;
	return E_OK;
} 

/* sys_ref_alm --------------------------------------------------------------*/
ER
sys_ref_alm(W apic, ID almid, T_RALM* pk_ralm)
{
	if (almid < 1 || almid > MAX_ALMID)
		return E_ID;
	if (alm[almid].act == 0)
		return E_NOEXS;
	
	pk_ralm->almstat = alm[almid].almstat;
	pk_ralm->lefttim = alm[almid].almtim;
	return E_OK;
}

/*---------------------------------------------------------------------------*/

void
alm_intr(unsigned long delta)
{
	int	i;
	for (i = 1 ; i < MAX_ALMID ; i ++) {
		if (alm[i].act) {
			if (alm[i].almtim > 0) {
				alm[i].almtim -= delta;
				if (alm[i].almtim < 0 &&
						alm[i].almstat == TALM_STA) {
					(*alm[i].almhdr)(alm[i].exinf);
				}
			}
		}
	}
	return;
}
