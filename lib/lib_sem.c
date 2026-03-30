/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"

/* cre_sem ------------------------------------------------------------------*/
ER
cre_sem(ID semid, T_CSEM* pk_csem)
{
	return syscall(-TFN_CRE_SEM, semid, pk_csem);
}

/* acre_sem -----------------------------------------------------------------*/
ER_ID
acre_sem(T_CSEM* pk_csem)
{
	return syscall(-TFN_ACRE_SEM, pk_csem);
}

/* del_sem ------------------------------------------------------------------*/
ER
del_sem(ID semid)
{
	return syscall(-TFN_DEL_SEM, semid);
}

/* sig_sem ------------------------------------------------------------------*/
ER
sig_sem(ID semid)
{
	return syscall(-TFN_SIG_SEM, semid);
}

/* isig_sem -----------------------------------------------------------------*/
ER
isig_sem(ID semid)
{
	return syscall(-TFN_ISIG_SEM, semid);
}

/* wai_sem ------------------------------------------------------------------*/
ER
wai_sem(ID semid)
{
	return syscall(-TFN_WAI_SEM, semid);
}

/* pol_sem ------------------------------------------------------------------*/
ER
pol_sem(ID semid)
{
	return syscall(-TFN_POL_SEM, semid);
}

/* twai_sem -----------------------------------------------------------------*/
ER
twai_sem(ID semid, TMO tmout)
{
	return syscall(-TFN_TWAI_SEM, semid, tmout);
}

/* ref_sem ------------------------------------------------------------------*/
ER
ref_sem(ID semid, T_RSEM* pk_rsem)
{
	return syscall(-TFN_REF_SEM, semid, pk_rsem);
}

/* event flag ===============================================================*/

/* cre_flg ------------------------------------------------------------------*/
ER
cre_flg(ID flgid, T_CFLG* pk_cflg)
{
	return syscall(-TFN_CRE_FLG, flgid, pk_cflg);
}

/* acre_flg -----------------------------------------------------------------*/
ER_ID
acre_flg(T_CFLG* pk_cflg)
{
	return syscall(-TFN_ACRE_FLG, pk_cflg);
}

/* del_flg ------------------------------------------------------------------*/
ER
del_flg(ID flgid)
{
	return syscall(-TFN_DEL_FLG, flgid);
}

/* set_flg ------------------------------------------------------------------*/
ER
set_flg(ID flgid, FLGPTN setptn)
{
	return syscall(-TFN_SET_FLG, flgid, setptn);
}

/* iset_flg (not system call) -----------------------------------------------*/
ER
iset_flg(ID flgid, FLGPTN setptn)
{
}

/* clr_flg ------------------------------------------------------------------*/
ER
clr_flg(ID flgid, FLGPTN clrptn)
{
	return syscall(-TFN_CLR_FLG, flgid, clrptn);
}

/* wai_flg ------------------------------------------------------------------*/
ER
wai_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN* p_flgptn)
{
	return syscall(-TFN_WAI_FLG, flgid, waiptn, wfmode, p_flgptn);
}

/* pol_flg ------------------------------------------------------------------*/
ER
pol_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN* p_flgptn)
{
	return syscall(-TFN_POL_FLG, flgid, waiptn, wfmode, p_flgptn);
}

/* twai_flg -----------------------------------------------------------------*/
ER
twai_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN* p_flgptn, TMO tmout)
{
	return syscall(-TFN_TWAI_FLG, flgid, waiptn, wfmode, p_flgptn, tmout);
}

/* ref_flg ------------------------------------------------------------------*/
ER
ref_flg(ID flgid, T_RFLG* pk_rflg)
{
	return syscall(-TFN_REF_FLG, flgid, pk_rflg);
}


/* data queue ===============================================================*/

/* cre_dtq ------------------------------------------------------------------*/
ER
cre_dtq(ID dtqid, T_CDTQ* pk_cdtq)
{
	return syscall(-TFN_CRE_DTQ, dtqid, pk_cdtq);
}

/* acre_dtq -----------------------------------------------------------------*/
ER_ID
acre_dtq(T_CDTQ* pk_cdtq)
{
	return syscall(-TFN_ACRE_DTQ, pk_cdtq);
}

/* del_dtq ------------------------------------------------------------------*/
ER
del_dtq(ID dtqid)
{
	return syscall(-TFN_DEL_DTQ, dtqid);
}

/* snd_dtq ------------------------------------------------------------------*/
ER
snd_dtq(ID dtqid, VP_INT data)
{
	return syscall(-TFN_SND_DTQ, dtqid, data);
}

/* psnd_dtq -----------------------------------------------------------------*/
ER
psnd_dtq(ID dtqid, VP_INT data)
{
	return syscall(-TFN_PSND_DTQ, dtqid, data);
}
#if 0
/* ipsnd_dtq (not system call) ----------------------------------------------*/
ER
ipsnd_dtq(ID dtqid, VP_INT data)
{
}
#endif

/* tsnd_dtq -----------------------------------------------------------------*/
ER
tsnd_dtq(ID dtqid, VP_INT data, TMO tmout)
{
	return syscall(-TFN_TSND_DTQ, dtqid, data, tmout);
}

/* fsnd_dtq -----------------------------------------------------------------*/
ER
fsnd_dtq(ID dtqid, VP_INT data)
{
	return syscall(-TFN_FSND_DTQ, dtqid, data);
}

/* ifsnd_dtq (not system call) ----------------------------------------------*/
ER
ifsnd_dtq(ID ddtqid, VP_INT data)
{
}

/* rcv_dtq ------------------------------------------------------------------*/
ER
rcv_dtq(ID dtqid, VP_INT* p_data)
{
	return syscall(-TFN_RCV_DTQ, dtqid, p_data);
}

/* prcv_dtq -----------------------------------------------------------------*/
ER
prcv_dtq(ID dtqid, VP_INT* p_data)
{
	return syscall(-TFN_PRCV_DTQ, dtqid, p_data);
}

/* trcv_dtq -----------------------------------------------------------------*/
ER
trcv_dtq(ID dtqid, VP_INT* p_data, TMO tmout)
{
	return syscall(-TFN_TRCV_DTQ, dtqid, p_data, tmout);
}

/* ref_dtq ------------------------------------------------------------------*/
ER
ref_dtq(ID dtqid, T_RDTQ* pk_rdtq)
{
	return syscall(-TFN_REF_DTQ, dtqid, pk_rdtq);
}

/* mail box =================================================================*/

/* cre_mbx ------------------------------------------------------------------*/
ER
cre_mbx(ID mbxid, T_CMBX* pk_cmbx)
{
	return syscall(-TFN_CRE_MBX, mbxid, pk_cmbx);
}

/* acre_mbx -----------------------------------------------------------------*/
ER_ID
acre_mbx(T_CMBX* pk_cmbx)
{
	return syscall(-TFN_ACRE_MBX, pk_cmbx);
}

/* del_mbx ------------------------------------------------------------------*/
ER
del_mbx(ID mbxid)
{
	return syscall(-TFN_DEL_MBX, mbxid);
}

/* snd_mbx ------------------------------------------------------------------*/
ER
snd_mbx(ID mbxid, T_MSG* pk_msg)
{
	return syscall(-TFN_SND_MBX, mbxid, pk_msg);
}

/* rcv_mbx ------------------------------------------------------------------*/
ER
rcv_mbx(ID mbxid, T_MSG** ppk_msg)
{
	return syscall(-TFN_RCV_MBX, mbxid, ppk_msg);
}

/* prcv_mbx -----------------------------------------------------------------*/
ER
prcv_mbx(ID mbxid, T_MSG** ppk_msg, TMO tmout)
{
	return syscall(-TFN_PRCV_MBX, mbxid, ppk_msg, tmout);
}

/* ref_mbx ------------------------------------------------------------------*/
ER
ref_mbx(ID mbxid, T_RMBX* pk_rmbx)
{
	return syscall(-TFN_REF_MBX, mbxid, pk_rmbx);
}


