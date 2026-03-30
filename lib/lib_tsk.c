/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/types.h"

/* cre_tsk ------------------------------------------------------------------*/
ER
cre_tsk(ID tskid, T_CTSK* pk_ctsk)
{
	return syscall(-TFN_CRE_TSK, tskid, pk_ctsk);
}

/* acre_tsk -----------------------------------------------------------------*/
ER_ID
acre_tsk(T_CTSK* pk_ctsk)
{
	return syscall(-TFN_ACRE_TSK, pk_ctsk);
}

/* del_tsk ------------------------------------------------------------------*/
ER
del_tsk(ID tskid)
{
	return syscall(-TFN_DEL_TSK, tskid);
}

/* act_tsk ------------------------------------------------------------------*/
ER
act_tsk(ID tskid)
{
	return syscall(-TFN_ACT_TSK, tskid);
}

/* iact_tsk (not system call) -----------------------------------------------*/
ER
iact_tsk(ID tskid)
{
	return sys_iact_tsk(0, tskid);
}

/* can_act ------------------------------------------------------------------*/
ER_UINT
can_act(ID tskid)
{
	return syscall(-TFN_CAN_ACT, tskid);
}

/* sta_tsk ------------------------------------------------------------------*/
ER
sta_tsk(ID tskid, VP_INT stacd)
{
	return syscall(-TFN_STA_TSK, tskid, stacd);
}

/* ext_tsk ------------------------------------------------------------------*/
void
ext_tsk(void)
{
	return syscall(-TFN_EXT_TSK);
}

/* exd_tsk ------------------------------------------------------------------*/
void
exd_tsk(void)
{
	return syscall(-TFN_EXD_TSK);
}

/* ter_tsk ------------------------------------------------------------------*/
ER
ter_tsk(ID tskid)
{
	return syscall(-TFN_TER_TSK, tskid);
}

/* chg_pri ------------------------------------------------------------------*/
ER
chg_pri(ID tskid, PRI tskpri)
{
	return syscall(-TFN_CHG_PRI, tskid, tskpri);
}

/* get_pri ------------------------------------------------------------------*/
ER
get_pri(ID tskid, PRI* p_tskpri)
{
	return syscall(-TFN_GET_PRI, tskid, p_tskpri);
}

/* ref_tsk ------------------------------------------------------------------*/
ER
ref_tsk(ID tskid, T_RTSK* pk_rtsk)
{
	return syscall(-TFN_REF_TSK, tskid, pk_rtsk);
}

/* ref_tst ------------------------------------------------------------------*/
ER
ref_tst(ID tskid, T_RTST* pk_rtst)
{
	return syscall(-TFN_REF_TST, tskid, pk_rtst);
}

/*===========================================================================*/ 
/* slp_tsk ------------------------------------------------------------------*/
ER
slp_tsk(void)
{
	return syscall(-TFN_SLP_TSK);
}

/* tslp_tsk -----------------------------------------------------------------*/
ER
tslp_tsk(TMO tmout)
{
	return syscall(-TFN_TSLP_TSK, tmout);
}

/* wup_tsk ------------------------------------------------------------------*/
ER
wup_tsk(ID tskid)
{
	return syscall(-TFN_WUP_TSK, tskid);
}

/* iwup_tsk (not system call) -----------------------------------------------*/
#if 0 /* (? ?) */
ER
iwup_tsk(ID tskid)
{
	return sys_iwup_tsk(tskid);
}
#endif

/* can_wup ------------------------------------------------------------------*/
ER_UINT
can_wup(ID tskid)
{
	return syscall(-TFN_CAN_WUP, tskid);
}

/* rel_wai ------------------------------------------------------------------*/
ER
rel_wai(ID tskid)
{
	return syscall(-TFN_REL_WAI, tskid);
}

/* irel_wai (not system call) -----------------------------------------------*/
ER
irel_wai(ID tskid)
{
#if 0
	return sys_irel_wai(
#endif
}

/* sus_tsk ------------------------------------------------------------------*/
ER
sus_tsk(ID tskid)
{
	return syscall(-TFN_SUS_TSK, tskid);
}

/* rsm_tsk ------------------------------------------------------------------*/
ER
rsm_tsk(ID tskid)
{
	return syscall(-TFN_RSM_TSK, tskid);
}

/* frsm_tsk -----------------------------------------------------------------*/
ER
frsm_tsk(ID tskid)
{
	return syscall(-TFN_FRSM_TSK, tskid);
}

/* dly_tsk ------------------------------------------------------------------*/
ER
dly_tsk(RELTIM dlytim)
{
	return syscall(-TFN_DLY_TSK, dlytim);
}

/*===========================================================================*/
/* def_tex ------------------------------------------------------------------*/
ER
def_tex(ID tskid, T_DTEX* pk_dtex)
{
	return syscall(-TFN_DEF_TEX, tskid, pk_dtex);
}

/* ras_tex ------------------------------------------------------------------*/
ER
ras_tex(ID tskid, TEXPTN rasptn)
{
	return syscall(-TFN_RAS_TEX, tskid, rasptn);
}

/* dis_tex ------------------------------------------------------------------*/
ER
dis_tex(void)
{
	return syscall(-TFN_DIS_TEX);
}

/* ena_tex ------------------------------------------------------------------*/
ER
ena_tex(void)
{
	return syscall(-TFN_ENA_TEX);
}

/* sns_tex ------------------------------------------------------------------*/
BOOL
sns_tex(void)
{
	return syscall(-TFN_SNS_TEX);
}

/* ref_tex ------------------------------------------------------------------*/
ER
ref_tex(ID tskid, T_RTEX* pk_rtex)
{
	return syscall(-TFN_REF_TEX, tskid, pk_rtex);
}

/* not itron service function ===============================================*/
ER
printf(char *s, ...)
{
	return syscall(-TFN_EXD_PRINT, &s); 
}
