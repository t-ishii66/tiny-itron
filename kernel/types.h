/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _LOCALTYPES_H
#define _LOCALTYPES_H
#include "../include/stdio.h"

/* T_LINK -------------------------------------------------------------------*/
typedef struct link {
	struct link*	prev;
	struct link*	next;
} T_LINK;

/* T_TIMEOUT ----------------------------------------------------------------*/
typedef struct timeout {
	struct timeout*	prev;
	struct timeout*	next;
	TMO		delta;
} T_TIMEOUT;

/* T_TSK --------------------------------------------------------------------*/
typedef struct tsk {
	T_LINK		plink;		/* priority link */
	T_LINK		wlink;		/* wait link */
	T_TIMEOUT	tlink;		/* time out link */
	ID		tskid;
	PRI		tskbpri;	/* base priority */
	PRI		tskpri;		/* current priority */
	proc_t*		proc;		/* pointer to struct proc */
	T_CTSK		ctsk;		/* initial argument */

	STAT		tskstat;
	UINT		actcnt;
	UINT		wupcnt;
	UINT		suscnt;

	/* event flag -------------------------------------------------------*/
	FLGPTN*		p_flgptn;	/* return parameter */
	FLGPTN		waiptn;		/* waiting event flag */
	MODE		wfmode;

	/* data queue -------------------------------------------------------*/
	VP_INT		data;		/* data of data queue */
	VP_INT*		p_data;		/* data of data queue */

	/* mail box ---------------------------------------------------------*/
	T_MSG**		ppk_msg;	/* message packet */

	/* mutex ------------------------------------------------------------*/
	UINT		mtxcnt;		/* mutex count */

	/* message buffer ---------------------------------------------------*/
	VP		msg;
	UINT		msgsz;

	/* memory pool ------------------------------------------------------*/
	VP*		p_blk;
	UINT		blksz;

	/* rendezvous -------------------------------------------------------*/
	RDVPTN		acpptn;	
	RDVPTN		calptn;
	VP		por_msg;
	UINT		por_msgsz;
	RDVNO*		p_rdvno;
	RDVNO		rdvno;		/* should be cleared by sched_ins */

	/* exception handler ------------------------------------------------*/
	TEXPTN		pndptn;
	STAT		texstat;	/* 0: dis-tex. 1: ena-tex */
	T_DTEX		tex;		/* exception handler */

	/* over run handler -------------------------------------------------*/
	ATR		ovratr;
	FP		ovrhdr;
	OVRTIM		ovrtim;
	STAT		ovrstat;
	VP_INT		exinf;

	/* tcp/ip network ---------------------------------------------------*/
	VP		net_data;
	VP		net_data_len;
} T_TSK;

#define wlink2tsk(wlink_ptr)	(T_TSK*)((char*)wlink_ptr - sizeof(T_LINK))
#define plink2tsk(plink_ptr)	(T_TSK*)(plink_ptr)
#define tlink2tsk(tlink_ptr)	(T_TSK*)((char*)tlink_ptr - 2 * sizeof(T_LINK))

/* T_SEM --------------------------------------------------------------------*/
typedef struct t_sem {
	T_LINK		wlink;
	ATR		sematr;		/* semaphore attribute */
	UINT		isemcnt;	/* initial sem count */
	UINT		maxsem;	
	UINT		semcnt;		/* current sem count */
	STAT		act;
} T_SEM;

/* T_FLG --------------------------------------------------------------------*/
typedef struct t_flg {
	T_LINK		wlink;
	ATR		flgatr;		/* flag attribute */
	FLGPTN		flgptn;		/* current flag pattern */
	FLGPTN		iflgptn;	/* initial flag pattern */
	STAT		act;
} T_FLG;

/* T_DTQ --------------------------------------------------------------------*/
typedef struct t_dtq {
	T_LINK		wlink_w;		/* sending queue */
	T_LINK		wlink_r;		/* receiving queue */
	ATR		dtqatr;
	UINT		dtqcnt;
	VP		dtq;
	STAT		act;
	STAT		dtq_alloc;		/* kernel allocation */
	/* ring buffer ------------------------------------------------------*/
	VW		r;
	VW		w;
} T_DTQ;

/* T_MBX --------------------------------------------------------------------*/
typedef struct t_mbx {
	T_LINK		wlink;		/* receiving queue */
	ATR		mbxatr;	
	PRI		maxmpri;
	VP		mprihd;
	STAT		mbx_alloc;
	STAT		act;
} T_MBX;

/* T_MTX --------------------------------------------------------------------*/
typedef struct t_mtx {
	ID		tskid;		/* locking task */	
	T_LINK		wlink;		/* waiting tasks */
	ATR		mtxatr;
	PRI		ceilpri;

	W		mtxlock;	/* lock parameter */
	STAT		act;		
} T_MTX;

/* T_MBF --------------------------------------------------------------------*/
typedef struct t_mbf {
	T_LINK		wlink_r;
	T_LINK		wlink_s;
	T_TSK*		wtsk;
	ATR		mbfatr;
	UINT		maxmsz;
	SIZE		mbfsz;
	VP		mbf;	

	UINT		smsgcnt;
	unsigned char*	mbf_end;
	unsigned char*	mbf_r;
	unsigned char*	mbf_w;
	VP		mbf_alloc_base;
	STAT		mbf_alloc;
	STAT		act;
} T_MBF;

/* T_POR --------------------------------------------------------------------*/
typedef struct t_por {
	T_LINK		wlink_c;	/* call */
	T_LINK		wlink_a;	/* accept */
	ATR		poratr;
	UINT		maxcmsz;
	UINT		maxrmsz;
	UINT		rdvno;

	STAT		act;
} T_POR;

/* T_MPF --------------------------------------------------------------------*/
typedef struct t_mpf {
	T_LINK		wlink;
	ATR		mpfatr;
	UINT		blkcnt;
	UINT		blksz;
	VP		mpf;

	allocation_t*	pool;
	STAT		mpf_alloc;
	STAT		act;
} T_MPF;

#define MAX_MPL_POOL	64
/* T_MPL --------------------------------------------------------------------*/
typedef struct t_mpl {
	T_LINK		wlink;
	ATR		mplatr;
	SIZE		mplsz;
	VP		mpl;

	allocation_t*	pool;
	STAT		mpl_alloc;
	STAT		act;
} T_MPL;

/* T_CYC --------------------------------------------------------------------*/
typedef struct t_cyc {
	ATR		cycatr;
	VP_INT		exinf;
	FP		cychdr;
	RELTIM		cyctim;
	RELTIM		icyctim;
	RELTIM		cycphs;

	STAT		act;
	STAT		stat;
} T_CYC;

/* T_ALM --------------------------------------------------------------------*/
typedef struct t_alm {
	ATR		almatr;
	VP_INT		exinf;
	FP		almhdr;
	STAT		almstat;
	RELTIM		almtim;
/*
	RELTIM		lefttim;
*/
	STAT		act;
} T_ALM;

/* T_ISR --------------------------------------------------------------------*/
typedef struct t_isr {
	T_LINK	wlink;
	ATR	isratr;
	VP_INT	exinf;
	INTNO	intno;
	FP	isr;
	STAT	act;
} T_ISR;

/* T_INH --------------------------------------------------------------------*/
typedef struct t_inh {
	ATR	inhatr;
	FP	inthdr;
} T_INH;

/* T_SVC --------------------------------------------------------------------*/
typedef struct t_svc {
	ATR	svcatr;
	FP	svcrtn;
	STAT	act;
} T_SVC;

/* T_EXC --------------------------------------------------------------------*/
typedef struct t_exc {
	ATR	excatr;
	FP	exchdr;
} T_EXC;

#endif
