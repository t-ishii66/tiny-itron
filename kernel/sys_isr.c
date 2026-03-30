/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"

T_LINK	isr_top[MAX_INHID + 1];

/* isr_init -----------------------------------------------------------------*/
void
isr_init(void)
{
	int	i;
	for (i = 0 ; i <= MAX_INHID ; i ++) {
		isr_top[i].next = &isr_top[i];
		isr_top[i].prev = &isr_top[i];
		inh[i].inhatr = 0;
		inh[i].inthdr = 0;
	}
	for (i = 0 ; i <= MAX_ISRID ; i ++) {
		isr[i].act = 0;
	} 
}

/* sys_def_inh --------------------------------------------------------------*/
ER
sys_def_inh(W apic, INHNO inhno, T_DINH* pk_dinh)
{
	if (inhno < 0 || inhno > MAX_INHID)
		return E_PAR;

	inh[inhno].inhatr = pk_dinh->inhatr;
	inh[inhno].inthdr = pk_dinh->inthdr;
	return E_OK;
}

/* sys_cre_isr --------------------------------------------------------------*/
ER
sys_cre_isr(W apic, ID isrid, T_CISR* pk_cisr)
{
	if (isrid < 0 || isrid > MAX_ISRID)
		return E_ID;
	if (isr[isrid].act == 1)
		return E_OBJ;

	isr[isrid].isratr = pk_cisr->isratr;
	isr[isrid].exinf = pk_cisr->exinf;
	isr[isrid].intno = pk_cisr->intno;
	isr[isrid].isr = pk_cisr->isr;

	ins_fifo(&isr_top[pk_cisr->intno], &isr[isrid].wlink);
	isr[isrid].act = 1;

	return E_OK;
}

/* sys_acre_isr -------------------------------------------------------------*/
ER_ID
sys_acre_isr(W apic, T_CISR* pk_cisr)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_ISRID; i++) {
		if (isr[i].act == 0) {
			if ((ret = sys_cre_isr(apic, i, pk_cisr)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_isr --------------------------------------------------------------*/
ER
sys_del_isr(W apic, ID isrid)
{
	if (isrid < 0 || isrid > MAX_ISRID)
		return E_ID;
	if (isr[isrid].act == 0)
		return E_NOEXS;
	wlink_rem(&isr[isrid].wlink);
	isr[isrid].act = 0;
	return E_OK;
}

/* sys_ref_isr --------------------------------------------------------------*/
ER
sys_ref_isr(W apic, ID isrid, T_RISR* pk_risr)
{
	if (isrid < 0 || isrid > MAX_ISRID)
		return E_ID;
	if (isr[isrid].act == 0)
		return E_NOEXS;

	/* T_RISR has no fields defined in this implementation */
	return E_OK;
}

/* sys_dis_int --------------------------------------------------------------*/
ER
sys_dis_int(W apic, INTNO intno)
{
	irq_mask_on(intno);	
	return E_OK;
}

/* sys_ena_int --------------------------------------------------------------*/
ER
sys_ena_int(W apic, INTNO intno)
{
	irq_mask_off(intno);	
	return E_OK;
}

/*===========================================================================*/
void
svc_init(void)
{
	int	i;
	for (i = 1 ; i <= MAX_FNCD ; i ++) {
		svc[i].act = 0;
	}
}

/* sys_def_svc --------------------------------------------------------------*/
ER
sys_def_svc(W apic, FN fncd, T_DSVC* pk_dsvc)
{
	if (fncd < 1 || fncd > MAX_FNCD)
		return E_PAR;
	svc[fncd].svcatr = pk_dsvc->svcatr;
	svc[fncd].svcrtn = pk_dsvc->svcrtn;
	svc[fncd].act = 1;
	return E_OK;
}

/* sys_cal_svc (?) ----------------------------------------------------------*/
ER_UINT
sys_cal_svc(W apic, FN fncd, VP_INT par1, VP_INT par2, ...)
{
	return E_NOSPT;
}

/*===========================================================================*/
void
exc_init(void)
{
	int	i;
	for (i = 0 ; i <= MAX_EXC ; i ++) {
		exc[i].exchdr = 0;
	}
}

/* sys_def_exc --------------------------------------------------------------*/
ER
sys_def_exc(W apic, EXCNO excno, T_DEXC* pk_dexc)
{
	if (excno < 0 || excno > MAX_EXC)
		return E_PAR;
	exc[excno].excatr = pk_dexc->excatr;
	exc[excno].exchdr = pk_dexc->exchdr;
	return E_OK;
}

/* sys_ref_cfg --------------------------------------------------------------*/
ER
sys_ref_cfg(W apic, T_RCFG* pk_rcfg)
{
	return E_NOSPT;
}

/* sys_ref_ver --------------------------------------------------------------*/
ER
sys_ref_ver(W apic, T_RVER* pk_rver)
{
	pk_rver->maker = TKERNEL_MAKER;
	pk_rver->prid = TKERNEL_PRID;
	pk_rver->spver = TKERNEL_SPVER;
	pk_rver->prver = TKERNEL_PRVER;
	pk_rver->prno[0] = pk_rver->prno[1] =
	pk_rver->prno[2] = pk_rver->prno[3] = '\0';
	return E_OK;
}

