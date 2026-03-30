/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/addr.h"

/* rot_rdq ------------------------------------------------------------------*/
ER
rot_rdq(PRI tskpri)
{
	return syscall(-TFN_ROT_RDQ, tskpri);
}

/* irot_rdq (not system call) -----------------------------------------------*/
ER
irot_rdq(PRI tskpri)
{
}

/* get_tid ------------------------------------------------------------------*/
ER
get_tid(ID* p_tskid)
{
	return syscall(-TFN_GET_TID, p_tskid);
}

/* iget_tid (not system call) -----------------------------------------------*/
ER
iget_tid(ID* p_tskid)
{
}

/* loc_cpu ------------------------------------------------------------------*/
ER
loc_cpu(void)
{
	return syscall(-TFN_LOC_CPU);
}

/* iloc_cpu (not system call) -----------------------------------------------*/
ER
iloc_cpu(void)
{
/*
	return sys_iloc_cpu();
*/
}

/* unl_cpu ------------------------------------------------------------------*/
ER
unl_cpu(void)
{
	return syscall(-TFN_UNL_CPU);
}

/* iunl_cpu (not system call) -----------------------------------------------*/
ER
iunl_cpu(void)
{
}

/* dis_dsp ------------------------------------------------------------------*/
ER
dis_dsp(void)
{
	return syscall(-TFN_DIS_DSP);
}

/* ena_dsp ------------------------------------------------------------------*/
ER
ena_dsp(void)
{
	return syscall(-TFN_ENA_DSP);
}

/* sns_ctx ------------------------------------------------------------------*/
BOOL
sns_ctx(void)
{
	H	cs = get_cs();
	if (cs == SEL_K32_C)
		return FALSE;
	return TRUE;
/*
	return syscall(-TFN_SNS_CTX);
*/
	
}

/* sns_loc ------------------------------------------------------------------*/
BOOL
sns_loc(void)
{
	return syscall(-TFN_SNS_LOC);
}

/* sns_dsp ------------------------------------------------------------------*/
BOOL
sns_dsp(void)
{
	return syscall(-TFN_SNS_DSP);
}

/* sns_dpn ------------------------------------------------------------------*/
BOOL
sns_dpn(void)
{
	return syscall(-TFN_SNS_DPN);
}

/* ref_sys ------------------------------------------------------------------*/
ER
ref_sys(T_RSYS* pk_rsys)
{
	return syscall(-TFN_REF_SYS, pk_rsys);
}

/* interrupt ================================================================*/
/* def_inh ------------------------------------------------------------------*/
ER
def_inh(INHNO inhno, T_DINH* pk_dinh)
{
	return syscall(-TFN_DEF_INH, inhno, pk_dinh);
}

/* cre_isr ------------------------------------------------------------------*/
ER
cre_isr(ID isrid, T_CISR* pk_cisr)
{
	return syscall(-TFN_CRE_ISR, isrid, pk_cisr);
}

/* acre_isr -----------------------------------------------------------------*/
ER_ID
acre_isr(T_CISR* pk_cisr)
{
	return syscall(-TFN_ACRE_ISR, pk_cisr);
}

/* del_isr ------------------------------------------------------------------*/
ER
del_isr(ID isrid)
{
	return syscall(-TFN_DEL_ISR, isrid);
}

/* ref_isr ------------------------------------------------------------------*/
ER
ref_isr(ID isrid, T_RISR* pk_risr)
{
	return syscall(-TFN_REF_ISR, isrid, pk_risr);
}

/* dis_int ------------------------------------------------------------------*/
ER
dis_int(INTNO intno)
{
	return syscall(-TFN_DIS_INT, intno);
}

/* ena_int ------------------------------------------------------------------*/
ER
ena_int(INTNO intno)
{
	return syscall(-TFN_ENA_INT, intno);
}

/* def_svc ------------------------------------------------------------------*/
ER
def_svc(FN fncd, T_DSVC* pk_dsvc)
{
	return syscall(-TFN_DEF_SVC, fncd, pk_dsvc);
}

/* cal_svc (?) --------------------------------------------------------------*/
ER_UINT
cal_svc(FN fncd, VP_INT par1, VP_INT par2, ...)
{
/*
	return syscall(-TFN_CAL_SVC, fncd, par1, par2);
*/
}

/* def_exc ------------------------------------------------------------------*/
ER
def_exc(EXCNO excno, T_DEXC* pk_dexc)
{
	return syscall(-TFN_DEF_EXC, excno, pk_dexc);
}

/* ref_cfg ------------------------------------------------------------------*/
ER
ref_cfg(T_RCFG* pk_rcfg)
{
	return syscall(-TFN_REF_CFG, pk_rcfg);
}

/* ref_ver ------------------------------------------------------------------*/
ER
ref_ver(T_RVER* pk_rver)
{
	return syscall(-TFN_REF_VER, pk_rver);
}

