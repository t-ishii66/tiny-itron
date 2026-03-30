/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/types.h"

/* set_tim ------------------------------------------------------------------*/
ER
set_tim(SYSTIM* p_systim)
{
	return syscall(-TFN_SET_TIM, p_systim);
}

/* get_tim ------------------------------------------------------------------*/
ER
get_tim(SYSTIM* p_systim)
{
	return syscall(-TFN_GET_TIM, p_systim);
}

/* isig_tim (not system call) -----------------------------------------------*/
ER
isig_tim(void)
{
}

/* cre_cyc ------------------------------------------------------------------*/
ER
cre_cyc(ID cycid, T_CCYC* pk_ccyc)
{
	return syscall(-TFN_CRE_CYC, cycid, pk_ccyc);
}

/* acre_cyc -----------------------------------------------------------------*/
ER_ID
acre_cyc(T_CCYC* pk_ccyc)
{
	return syscall(-TFN_ACRE_CYC, pk_ccyc);
}

/* del_cyc ------------------------------------------------------------------*/
ER
del_cyc(ID cycid)
{
	return syscall(-TFN_DEL_CYC, cycid);
}

/* sta_cyc ------------------------------------------------------------------*/
ER
sta_cyc(ID cycid)
{
	return syscall(-TFN_STA_CYC, cycid);
}

/* stp_cyc ------------------------------------------------------------------*/
ER
stp_cyc(ID cycid)
{
	return syscall(-TFN_STP_CYC, cycid);
}

/* ref_cyc ------------------------------------------------------------------*/
ER
ref_cyc(ID cycid, T_RCYC* pk_rcyc)
{
	return syscall(-TFN_REF_CYC, cycid, pk_rcyc);
}

/* alarm ====================================================================*/
/* cre_alm ------------------------------------------------------------------*/
ER
cre_alm(ID almid, T_CALM* pk_calm)
{
	return syscall(-TFN_CRE_ALM, almid, pk_calm);
}

/* acre_alm -----------------------------------------------------------------*/
ER_ID
acre_alm(T_CALM* pk_calm)
{
	return syscall(-TFN_ACRE_ALM, pk_calm);
}

/* del_alm ------------------------------------------------------------------*/
ER
del_alm(ID almid)
{
	return syscall(-TFN_DEL_ALM, almid);
}

/* sta_alm ------------------------------------------------------------------*/
ER
sta_alm(ID almid, RELTIM almtim)
{
	return syscall(-TFN_STA_ALM, almid, almtim);
}

/* stp_alm ------------------------------------------------------------------*/
ER
stp_alm(ID almid)
{
	return syscall(-TFN_STP_ALM, almid);
} 

/* ref_alm ------------------------------------------------------------------*/
ER
ref_alm(ID almid, T_RALM* pk_ralm)
{
	return syscall(-TFN_REF_ALM, almid, pk_ralm);
}

/* over run =================================================================*/
/* def_ovr ------------------------------------------------------------------*/
ER
def_ovr(T_DOVR* pk_dovr)
{
	return syscall(-TFN_DEF_OVR, pk_dovr);
}

/* sta_ovr ------------------------------------------------------------------*/
ER
sta_ovr(ID tskid, OVRTIM ovrtim)
{
	return syscall(-TFN_STA_OVR, tskid, ovrtim);
}

/* stp_ovr ------------------------------------------------------------------*/
ER
stp_ovr(ID tskid)
{
	return syscall(-TFN_STP_OVR, tskid);
}

/* ref_ovr ------------------------------------------------------------------*/
ER
ref_ovr(ID tskid, T_ROVR* pk_rovr)
{
	return syscall(-TFN_REF_OVR, tskid, pk_rovr);
}
