/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/types.h"

/* mutex ====================================================================*/
/* cre_mtx ------------------------------------------------------------------*/
ER
cre_mtx(ID mtxid, T_CMTX* pk_cmtx)
{
	return syscall(-TFN_CRE_MTX, mtxid, pk_cmtx);
}

/* acre_mtx -----------------------------------------------------------------*/
ER_ID
acre_mtx(T_CMTX* pk_cmtx)
{
	return syscall(-TFN_ACRE_MTX, pk_cmtx);
}

/* del_mtx ------------------------------------------------------------------*/
ER
del_mtx(ID mtxid)
{
	return syscall(-TFN_DEL_MTX, mtxid);
}

/* loc_mtx ------------------------------------------------------------------*/
ER
loc_mtx(ID mtxid)
{
	return syscall(-TFN_LOC_MTX, mtxid);
}

/* ploc_mtx -----------------------------------------------------------------*/
ER
ploc_mtx(ID mtxid)
{
	return syscall(-TFN_PLOC_MTX, mtxid);
}

/* tloc_mtx -----------------------------------------------------------------*/
ER
tloc_mtx(ID mtxid, TMO tmout)
{
	return syscall(-TFN_TLOC_MTX, mtxid, tmout);
}

/* unl_mtx ------------------------------------------------------------------*/
ER
unl_mtx(ID mtxid)
{
	return syscall(-TFN_UNL_MTX, mtxid);
}

/* ref_mtx ------------------------------------------------------------------*/
ER
ref_mtx(ID mtxid, T_RMTX* pk_rmtx)
{
	return syscall(-TFN_REF_MTX, mtxid, pk_rmtx);
}

/* message buffer ===========================================================*/
/* cre_mbf ------------------------------------------------------------------*/
ER
cre_mbf(ID mbfid, T_CMBF* pk_cmbf)
{
	return syscall(-TFN_CRE_MBF, mbfid, pk_cmbf);
}

/* acre_mbf -----------------------------------------------------------------*/
ER_ID
acre_mbf(T_CMBF* pk_cmbf)
{
	return syscall(-TFN_ACRE_MBF, pk_cmbf);
}

/* del_mbf ------------------------------------------------------------------*/
ER
del_mbf(ID mbfid)
{
	return syscall(-TFN_DEL_MBF, mbfid);
}

/* snd_mbf ------------------------------------------------------------------*/
ER
snd_mbf(ID mbfid, VP msg, UINT msgsz)
{
	return syscall(-TFN_SND_MBF, mbfid, msg, msgsz);
}

/* psnd_mbf -----------------------------------------------------------------*/
ER
psnd_mbf(ID mbfid, VP msg, UINT msgsz)
{
	return syscall(-TFN_PSND_MBF, mbfid, msg, msgsz);
}

/* tsnd_mbf -----------------------------------------------------------------*/
ER
tsnd_mbf(ID mbfid, VP msg, UINT msgsz, TMO tmout)
{
	return syscall(-TFN_TSND_MBF, mbfid, msg, msgsz, tmout);
}

/* rcv_mbf ------------------------------------------------------------------*/
ER_UINT
rcv_mbf(ID mbfid, VP msg)
{
	return syscall(-TFN_RCV_MBF, mbfid, msg);
}

/* prcv_mbf -----------------------------------------------------------------*/
ER_UINT
prcv_mbf(ID mbfid, VP msg)
{
	return syscall(-TFN_PRCV_MBF, mbfid, msg);
}

/* trcv_mbf -----------------------------------------------------------------*/
ER_UINT
trcv_mbf(ID mbfid, VP msg, TMO tmout)
{
	return syscall(-TFN_TRCV_MBF, mbfid, msg, tmout);
}

/* ref_mbf ------------------------------------------------------------------*/
ER
ref_mbf(ID mbfid, T_RMBF* pk_rmbf)
{
	return syscall(-TFN_REF_MBF, mbfid, pk_rmbf);
}

/* rendezvous ===============================================================*/
/* cre_por ------------------------------------------------------------------*/
ER
cre_por(ID porid, T_CPOR* pk_cpor)
{
	return syscall(-TFN_CRE_POR, porid, pk_cpor);
}

/* acre_por -----------------------------------------------------------------*/
ER_ID
acre_por(T_CPOR* pk_cpor)
{
	return syscall(-TFN_ACRE_POR, pk_cpor);
}

/* del_por ------------------------------------------------------------------*/
ER
del_por(ID porid)
{
	return syscall(-TFN_DEL_POR, porid);
}

/* cal_por ------------------------------------------------------------------*/
ER_UINT
cal_por(ID porid, RDVPTN calptn, VP msg, UINT cmsgsz)
{
	return syscall(-TFN_CAL_POR, porid, calptn, msg, cmsgsz);
}

/* tcal_por -----------------------------------------------------------------*/
ER_UINT
tcal_por(ID porid, RDVPTN calptn, VP msg, UINT cmsgsz, TMO tmout)
{
	return syscall(-TFN_TCAL_POR, porid, calptn, msg, cmsgsz, tmout);
}

/* acp_por ------------------------------------------------------------------*/
ER_UINT
acp_por(ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg)
{
	return syscall(-TFN_ACP_POR, porid, acpptn, p_rdvno, msg);
}

/* pacp_por -----------------------------------------------------------------*/
ER_UINT
pacp_por(ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg)
{
	return syscall(-TFN_PACP_POR, porid, acpptn, p_rdvno, msg);
}

/* tacp_por -----------------------------------------------------------------*/
ER_UINT
tacp_por(ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg, TMO tmout)
{
	return syscall(-TFN_TACP_POR, porid, acpptn, p_rdvno, msg, tmout);
}

/* fwd_por ------------------------------------------------------------------*/
ER
fwd_por(ID porid, RDVPTN calptn, RDVNO rdvno, VP msg, UINT cmsgsz)
{
	return syscall(-TFN_FWD_POR, porid, calptn, rdvno, msg, cmsgsz);
}

/* rpl_rdv ------------------------------------------------------------------*/
ER
rpl_rdv(RDVNO rdvno, VP msg, UINT rmsgsz)
{
	return syscall(-TFN_RPL_RDV, rdvno, msg, rmsgsz);
}

/* ref_por ------------------------------------------------------------------*/
ER
ref_por(ID porid, T_RPOR* pk_rpor)
{
	return syscall(-TFN_REF_POR, porid, pk_rpor);
}

/* ref_rdv ------------------------------------------------------------------*/
ER
ref_rdv(RDVNO rdvno, T_RRDV* pk_rrdv)
{
	return syscall(-TFN_REF_RDV, rdvno, pk_rrdv);
}

/* memory pool ==============================================================*/

/* cre_mpf ------------------------------------------------------------------*/
ER
cre_mpf(ID mpfid, T_CMPF* pk_cmpf)
{
	return syscall(-TFN_CRE_MPF, mpfid, pk_cmpf);
}

/* acre_mpf -----------------------------------------------------------------*/
ER_ID
acre_mpf(T_CMPF* pk_cmpf)
{
	return syscall(-TFN_ACRE_MPF, pk_cmpf);
}

/* del_mpf ------------------------------------------------------------------*/
ER
del_mpf(ID mpfid)
{
	return syscall(-TFN_DEL_MPF, mpfid);
}

/* get_mpf ------------------------------------------------------------------*/
ER
get_mpf(ID mpfid, VP* p_blk)
{
	return syscall(-TFN_GET_MPF, mpfid, p_blk);
}

/* pget_mpf -----------------------------------------------------------------*/
ER
pget_mpf(ID mpfid, VP* p_blk)
{
	return syscall(-TFN_PGET_MPF, mpfid, p_blk);
}

/* tget_mpf -----------------------------------------------------------------*/
ER
tget_mpf(ID mpfid, VP* p_blk, TMO tmout)
{
	return syscall(-TFN_TGET_MPF, mpfid, p_blk, tmout);
}

/* rel_mpf ------------------------------------------------------------------*/
ER
rel_mpf(ID mpfid, VP blk)
{
	return syscall(-TFN_REL_MPF, mpfid, blk);
}

/* ref_mpf ------------------------------------------------------------------*/
ER
ref_mpf(ID mpfid, T_RMPF* pk_rmpf)
{
	return syscall(-TFN_REF_MPF, mpfid, pk_rmpf);
}

/*===========================================================================*/
/* cre_mpl ------------------------------------------------------------------*/
ER
cre_mpl(ID mplid, T_CMPL* pk_cmpl)
{
	return syscall(-TFN_CRE_MPL, mplid, pk_cmpl);
}

/* acre_mpl -----------------------------------------------------------------*/
ER
acre_mpl(T_CMPL* pk_cmpl)
{
	return syscall(-TFN_ACRE_MPL, pk_cmpl);
}

/* del_mpl ------------------------------------------------------------------*/
ER
del_mpl(ID mplid)
{
	return syscall(-TFN_DEL_MPL, mplid);
}

/* get_mpl ------------------------------------------------------------------*/
ER
get_mpl(ID mplid, UINT blksz, VP* p_blk)
{
	return syscall(-TFN_GET_MPL, mplid, blksz, p_blk);
}

/* pget_mpl -----------------------------------------------------------------*/
ER
pget_mpl(ID mplid, UINT blksz, VP* p_blk)
{
	return syscall(-TFN_PGET_MPL, mplid, blksz, p_blk);
}

/* tget_mpl -----------------------------------------------------------------*/
ER
tget_mpl(ID mplid, UINT blksz, VP* p_blk, TMO tmout)
{
	return syscall(-TFN_TGET_MPL, mplid, blksz, p_blk, tmout);
}

/* rel_mpl ------------------------------------------------------------------*/
ER
rel_mpl(ID mplid, VP blk)
{
	return syscall(-TFN_REL_MPL, mplid, blk);
}

/* ref_mpl ------------------------------------------------------------------*/
ER
ref_mpl(ID mplid, T_RMPL* pk_rmpl)
{
	return syscall(-TFN_REF_MPL, mplid, pk_rmpl);
}


