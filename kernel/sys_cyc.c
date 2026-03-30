/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"

/* cyc_init -----------------------------------------------------------------*/
void
cyc_init(void)
{
	int	i;
	for (i = 1 ; i <= MAX_CYCID ; i ++) {
		cyc[i].act = 0;
	} 
}

/* sys_cre_cyc --------------------------------------------------------------*/
ER
sys_cre_cyc(W apic, ID cycid, T_CCYC* pk_ccyc)
{
	if (cycid < 1 || cycid > MAX_CYCID)
		return E_ID;
	if (cyc[cycid].act)
		return E_OBJ;
	if (pk_ccyc->cyctim == 0)
		return E_PAR;
	cyc[cycid].cycatr = pk_ccyc->cycatr;
	cyc[cycid].exinf = pk_ccyc->exinf;
	cyc[cycid].cychdr = pk_ccyc->cychdr;
	cyc[cycid].cyctim = pk_ccyc->cyctim;
	cyc[cycid].icyctim = pk_ccyc->cyctim;
	cyc[cycid].cycphs = pk_ccyc->cycphs;

	cyc[cycid].act = 1;
	cyc[cycid].stat = TCYC_STP;
	return E_OK;
}

/* sys_acre_cyc -------------------------------------------------------------*/
ER_ID
sys_acre_cyc(W apic, T_CCYC* pk_ccyc)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_CYCID; i++) {
		if (cyc[i].act == 0) {
			if ((ret = sys_cre_cyc(apic, i, pk_ccyc)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_cyc --------------------------------------------------------------*/
ER
sys_del_cyc(W apic, ID cycid)
{
	if (cycid < 1 || cycid > MAX_CYCID)
		return E_ID;
	if (cyc[cycid].act == 0)
		return E_NOEXS;

	cyc[cycid].act = 0;
	return E_OK;
}

/* sys_sta_cyc --------------------------------------------------------------*/
ER
sys_sta_cyc(W apic, ID cycid)
{
	if (cycid < 1 || cycid > MAX_CYCID)
		return E_ID;
	if (cyc[cycid].act == 0)
		return E_NOEXS;

	cyc[cycid].stat = TCYC_STA;
	if (cyc[cycid].cycatr & TA_STA) {
		cyc[cycid].cyctim = cyc[cycid].icyctim;
		cyc[cycid].cycphs = 0;
	}
	return E_OK;
}

/* sys_stp_cyc --------------------------------------------------------------*/
ER
sys_stp_cyc(W apic, ID cycid)
{
	if (cycid < 1 || cycid > MAX_CYCID)
		return E_ID;
	if (cyc[cycid].act == 0)
		return E_NOEXS;

	cyc[cycid].stat = TCYC_STP;
	return E_OK;
}

/* sys_ref_cyc --------------------------------------------------------------*/
ER
sys_ref_cyc(W apic, ID cycid, T_RCYC* pk_rcyc)
{
	if (cycid < 1 || cycid > MAX_CYCID)
		return E_ID;
	if (cyc[cycid].act == 0)
		return E_NOEXS;

	pk_rcyc->cycstat = cyc[cycid].stat;
	pk_rcyc->lefttim = cyc[cycid].cycphs + cyc[cycid].cyctim;

	return E_OK;
}

/*---------------------------------------------------------------------------*/
void
cyc_intr(unsigned long delta)
{
	int	i;
	for (i = 1 ; i < MAX_CYCID ; i ++) {
		if (cyc[i].act) {
			if (cyc[i].cycphs > 0) {
				cyc[i].cycphs -= delta;
			} else {
				cyc[i].cycphs = 0;
				cyc[i].cyctim -= delta;
			}
			if (cyc[i].cyctim < 0) {
				cyc[i].cyctim = cyc[i].icyctim;
				if (cyc[i].stat == TCYC_STA) {
					(*cyc[i].cychdr)(cyc[i].exinf);
				}	
			}
		}
	}
}
