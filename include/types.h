/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _TYPES_H
#define _TYPES_H

/* T_CTSK -------------------------------------------------------------------*/
typedef struct t_ctsk {
	ATR	tskatr;		/* task attribute */
	VP_INT	exinf;		/* task expanded information */
	FP	task;		/* task start address */
	PRI	itskpri;	/* task priority at start up */	
	SIZE	stksz;		/* task stack size */
	VP	stk;		/* task stack pointer */
} T_CTSK;

/* T_RTSK -------------------------------------------------------------------*/
typedef struct t_rtsk {
	STAT	tskstat;	/* task status */
	PRI	tskpri;		/* task current priority */
	PRI	tskbpri;	/* task base priority */
	STAT	tskwait;	/* factor of waiting */
	ID	wobjid;		/* ID number of target object */
	TMO	lefttmo;	/* left time of time out */
	UINT	actcnt;		/* queuing number of activation request */
	UINT	wupcnt;		/* queuing number of wake-up request */
	UINT	suscnt;		/* nesst number of suspension */
} T_RTSK;

/* T_RTST -------------------------------------------------------------------*/
typedef struct t_rtst {
	STAT	tskstat;	/* task status */
	STAT	tskwait;	/* factor of waiting */
} T_RTST;

/* T_DTEX -------------------------------------------------------------------*/
typedef struct t_dtex {
	ATR	texatr;		/* attribute of task exception routine */
	FP	texrtn;		/* start address of task exception routine */
	VP_INT	exinf;		/* extended information */
} T_DTEX;

/* T_RTEX -------------------------------------------------------------------*/
typedef struct t_rtex {
	STAT	texstat;	/* status of task exception routine */
	TEXPTN	pndptn;		/* factor of pending exception */
} T_RTEX;

/* T_CSEM -------------------------------------------------------------------*/
typedef struct t_csem {
	ATR	sematr;		/* attribute of semaphore */
	UINT	isemcnt;	/* initial number of semaphore resource */	
	UINT	maxsem;		/* maximun number of semaphore resource */
} T_CSEM;

/* T_RSEM -------------------------------------------------------------------*/
typedef struct t_rsem {
	ID	wtskid;		/* task id, head of waiting queue */
	UINT	semcnt;		/* current number of semaphore resource */
} T_RSEM;

/* T_CFLG -------------------------------------------------------------------*/
typedef struct t_cflg {
	ATR	flgatr;		/* attribute of event flag */
	FLGPTN	iflgptn;	/* initial bit pattern of event flag */
} T_CFLG;

/* T_RFLG -------------------------------------------------------------------*/
typedef struct t_rflg {
	ID	wtskid;		/* task id, head of waiting queue */
	FLGPTN	flgptn;		/* current bit pattern of event flag */
} T_RFLG;

/* T_CDTQ -------------------------------------------------------------------*/
typedef struct t_cdtq {
	ATR	dtqatr;		/* attribute of data queue */
	UINT	dtqcnt;		/* number of data */
	VP	dtq;		/* head address of data queue */
} T_CDTQ;

/* T_RDTQ -------------------------------------------------------------------*/
typedef struct t_rdtq {
	ID	stskid;		/* task id, head of data queue to be sent*/
	ID	rtskid;		/* task id,head of data queue be be received */
	UINT	sdtqcnt;	/* number of data in data queue */
} T_RDTQ;

/* T_CMBX -------------------------------------------------------------------*/
typedef struct t_cmbx {
	ATR	mbxatr;		/* attribute of mail box */
	PRI	maxmpri;	/* maximum priority of message */
	VP	mprihd;		/* head address of message queue */
} T_CMBX;

/* T_RMBX -------------------------------------------------------------------*/
typedef struct t_rmbx {
	ID	wtskid;		/* task id, head of waiting mail box queue */ 
	T_MSG*	pk_msg;		/* head address of message packet, head of 
				   message queue */
} T_RMBX;

/* T_CMTX -------------------------------------------------------------------*/
typedef struct t_cmtx {
	ATR	mtxatr;		/* attribute of mutex */
	PRI	ceilpri;	/* maximum priorty of mutex */
} T_CMTX;

/* T_RMTX -------------------------------------------------------------------*/
typedef struct t_rmtx {
	ID	htskid;		/* task id locking mutex */
	ID	wtskid;		/* task id head of mutex waiting queue */
} T_RMTX;

/* T_CMBF -------------------------------------------------------------------*/
typedef struct t_cmbf {
	ATR	mbfatr;		/* attribute of message buffer */
	UINT	maxmsz;		/* maximum size of message */
	SIZE	mbfsz;		/* size of message buffer (byte) */
	VP	mbf;		/* head address of message buffer */
} T_CMBF;

/* T_RMBF -------------------------------------------------------------------*/
typedef struct t_rmbf {
	ID	stskid;		/* task id, head of message sending 
				   waiting queue */
	ID	rtskid;		/* task id, head of message receiving 
				   waiting queue */
	UINT	smsgcnt;	/* number of data in message buffer */ 
	SIZE	fmbfsz;		/* space area size of message buffer */
} T_RMBF;

/* T_CPOR -------------------------------------------------------------------*/
typedef struct t_cpor {
	ATR	poratr;		/* attribute of rendezvous */
	UINT	maxcmsz;	/* maximum size of calling message */
	UINT	maxrmsz;	/* maximum size of receiving message */
} T_CPOR;

/* T_RPOR -------------------------------------------------------------------*/
typedef struct t_rpor {
	ID	ctskid;		/* task id, head of calling rendezvous queue */ 
	ID	atskid;		/* task id, head of receiving rendezvous
				   queue */
} T_RPOR;

/* T_RRDV -------------------------------------------------------------------*/
typedef struct t_rrdv {
	ID	wtskid;		/* task id waiting exit of rendezvous */
} T_RRDV;

/* T_CMPF -------------------------------------------------------------------*/
typedef struct t_cmpf {
	ATR	mpfatr;		/* attribute of fixed size memory pool */
	UINT	blkcnt;		/* number of available memory block */
	UINT	blksz;		/* size of memory block */
	VP	mpf;		/* head address of fixed size memory pool */
} T_CMPF;

/* T_RMPF -------------------------------------------------------------------*/
typedef struct t_rmpf {
	ID	wtskid;		/* task id, head of fixed size memory pool
				   waiting queue */
	UINT	fblkcnt;	/* number of memory block in fixed size
				   memory pool */
} T_RMPF;

/* T_CMPL -------------------------------------------------------------------*/
typedef struct t_cmpl {
	ATR	mplatr;		/* attribute of variable size memory pool */
	SIZE	mplsz;		/* size of variable size memory pool */
	VP	mpl;		/* head address of variable size memory pool */
} T_CMPL;

/* T_RMPL -------------------------------------------------------------------*/
typedef struct t_rmpl {
	ID	wtskid;		/* task id, head of variable size memory pool
				   waiting queue */
	SIZE	fmplsz;		/* total size of space area of variable size
				   memory pool */
	UINT	fblksz;		/* size of currently available memory block */
} T_RMPL; 

/* T_CCYC -------------------------------------------------------------------*/
typedef struct t_ccyc {
	ATR	cycatr;		/* attribute of cyclic handler */	
	VP_INT	exinf;		/* extended information of cyclic handler */
	FP	cychdr;		/* start address of cyclic handler */
	RELTIM	cyctim;		/* frequency of cyclic handler */
	RELTIM	cycphs;		/* phase of cyclic handler */
} T_CCYC;

/* T_RCYC -------------------------------------------------------------------*/
typedef struct t_rcyc {
	STAT	cycstat;	/* status of cyclic handler */
	RELTIM	lefttim;	/* rest time till cyclic handler starts */
} T_RCYC;

/* T_CALM -------------------------------------------------------------------*/
typedef struct t_calm {
	ATR	almatr;		/* attribute of alarm handler */
	VP_INT	exinf;		/* extended information of alarm handler */
	FP	almhdr;		/* head address of alarm handler */
} T_CALM;

/* T_RALM -------------------------------------------------------------------*/
typedef struct t_ralm {
	STAT	almstat;	/* status of alarm handler */
	RELTIM	lefttim;	/* rest time till alarm handler starts */
} T_RALM;

/* T_DOVR -------------------------------------------------------------------*/
typedef struct t_dovr {
	ATR	ovratr;		/* attribute of over run handler */
	FP	ovrhdr;		/* head address of over run handler */
} T_DOVR;

/* T_ROVR -------------------------------------------------------------------*/
typedef struct t_rovr {
	STAT	ovrstat;	/* status of over run handler */
	OVRTIM	leftotm;	/* rest processor time */
} T_ROVR;

/* T_RSYS -------------------------------------------------------------------*/
typedef struct t_rsys {
} T_RSYS;

/* T_DINH -------------------------------------------------------------------*/
typedef struct t_dinh {
	ATR	inhatr;		/* attribute of interrupt handler */
	FP	inthdr;		/* head address of interrupt handler */
} T_DINH;

/* T_CISR -------------------------------------------------------------------*/
typedef struct t_cisr {
	ATR	isratr;		/* attribute of interrupt service routine */
	VP_INT	exinf;		/* extended information of interrupt service
				   routine */
	INTNO	intno;		/* interrupt number to add interrupt
				   service routine */
	FP	isr;		/* head address of interrupt service routine */	
} T_CISR;

/* T_RISR -------------------------------------------------------------------*/
typedef struct t_risr {
} T_RISR;

/* T_DSVC -------------------------------------------------------------------*/
typedef struct t_dsvc {
	ATR	svcatr;		/* attribute of extended service call */
	FP	svcrtn;		/* start address of extended service call */
} T_DSVC;


/* T_DEXC -------------------------------------------------------------------*/
typedef struct t_dexc {
	ATR	excatr;		/* attribute of cpu exception handler */
	FP	exchdr;		/* start address of cpu exception handler */
} T_DEXC;

/* T_RCFG -------------------------------------------------------------------*/
typedef struct t_rcfg {
} T_RCFG;

/* T_RVER -------------------------------------------------------------------*/
typedef struct t_rver {
	UH	maker;		/* maker code of kernel */
	UH	prid;		/* kernel id */
	UH	spver;		/* ITRON version number */
	UH	prver;		/* kernel version number */
	UH	prno[4];	/* management information of kernel product */	
} T_RVER;


/* i386 ---------------------------------------------------------------------*/
#define htons(x)	((((x << 8) & 0xff00) | ((x >> 8) & 0x00ff)) & 0xffff)
#define htonl(x)		(((x << 8) & 0x00ff0000) | \
				((x << 24) & 0xff000000) | \
				((x >> 8) & 0x0000ff00) | \
				((x >> 24) & 0x000000ff))
#define ntohs(x)	htons(x)
#define ntohl(x)	htonl(x)

#endif
