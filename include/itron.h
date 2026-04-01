/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _ITRON_H
#define _ITRON_H

typedef	char		B;		/* signed 8 bit */
typedef	short		H;		/* signed 16 bit */
typedef long		W;		/* signed 32 bit */
typedef	unsigned char	UB;		/* unsigned 8 bit */ 
typedef	unsigned short	UH;		/* unsigned 16 bit */ 
typedef	unsigned long	UW;		/* unsigned 32 bit */ 
typedef	unsigned char*	VB;		/* unknown data type 8 bit */
typedef	unsigned short*	VH;		/* unknown data type 16 bit */
typedef	unsigned long*	VW;		/* unknown data type 32 bit */

#if 0	/* 64 bit support ---------------------------------------------------*/
typedef	unsigned long	D;		/* signed 64 bit */ 
typedef	unsigned long	UD;		/* unsigned 64 bit */ 
typedef	unsigned long	VD;		/* unknown data type 64 bit */ 
#endif

#if 0
typedef	void*		VP;		/* pointer to unknown type */
#else
typedef	char*		VP;		/* pointer to unknown type */
#endif
typedef	void		(*FP)();	/* program start address */
typedef	int		INT;		/* natural integer */	
typedef	unsigned int	UINT;		/* natural unsigned integer */

typedef	INT		BOOL;		/* boolean (TRUE of FALSE) */
typedef	INT		FN;		/* function code */
typedef	INT		ER;		/* error code */
typedef	INT		ID;		/* object ID number */
typedef	UINT		ATR;		/* object attribute */
typedef	UINT		STAT;		/* object status */
typedef	UINT		MODE;		/* mode of survice call */
typedef	INT		PRI;		/* priority */
typedef	UINT		SIZE;		/* size of memory area */
typedef	UINT 		TMO;		/* time out */
typedef	UINT		RELTIM;		/* relative time */	
typedef	struct {
	W l;
	W h;
} SYSTIM;				/* system time */
typedef	INT		VP_INT;		/* pointer to unknown data type */
typedef	INT		ER_BOOL;	/* error code or boolean */
typedef	INT		ER_ID;		/* error code or ID number */
typedef	UINT 		ER_UINT;	/* error code or unsigned integer */

typedef UINT		TEXPTN;		/* bit pattern of task exception */
typedef UINT		FLGPTN;		/* bit pattern of event flag */
typedef UINT		T_MSG;		/* (?) message header */
typedef UINT		RDVPTN;		/* bit pattern of rendezvous */
typedef UINT		RDVNO;		/* number of rendezvous */
typedef UINT		OVRTIM;		/* processor time */
typedef UINT		INHNO;		/* number of interrupt handler */
typedef UINT		INTNO;		/* interrupt number */
typedef UINT		EXCNO;		/* number of cpu exception */

/* T_MSG_PRI ----------------------------------------------------------------*/
typedef struct t_msg_pri {
	T_MSG		msgque;
	PRI		msgpri;
} T_MSG_PRI;

/* general ------------------------------------------------------------------*/
#define	NULL	0
#define	TRUE	1
#define	FALSE	0

/* error code ---------------------------------------------------------------*/
#define	E_OK	0	/* no error */
#define E_SYS	-5	/* system error */
#define E_NOSPT	-9	/* non support */
#define E_RSFN	-10	/* reserved function code */
#define	E_RSATR	-11	/* reserved attribute */
#define E_PAR	-17	/* parameter error */
#define E_ID	-18	/* illegal ID number */
#define E_CTX	-25	/* context error */
#define E_MACV	-26	/* memory access violation */
#define	E_OACV	-27	/* object access violation */
#define	E_ILUSE	-28	/* service call illegal usage */
#define E_NOMEM	-33	/* lack of memory */
#define	E_NOID	-34	/* lack of ID number */
#define E_OBJ	-41	/* object status error */
#define E_NOEXS	-42	/* object not exist */
#define	E_QOVR	-43	/* queuing over flow */
#define E_RLWAI	-49	/* release waiting status */
#define E_TMOUT	-50	/* polling error or time out */
#define E_DLT	-51	/* delete waiting object */
#define E_CLS	-52	/* change status of waiting object */
#define	E_WBLK	-57	/* accept non blocking */
#define E_BOVR	-58	/* buffer over flow */

/* constant & macro ---------------------------------------------------------*/
#define TA_NULL		0	/* not specify object attribute */

#define TA_HLNG		0x00
#define TA_ASM		0x01

#define TA_TFIFO	0x00
#define TA_TPRI		0x01

#define TA_MFIFO	0x00
#define TA_MPRI		0x02

#define TA_ACT		0x02
#define TA_RSTR		0x04

#define TA_WSGL		0x00

#define TA_WMUL		0x02

#define TA_CLR		0x04

#define TA_INHERIT	0x02
#define TA_CEILING	0x03
#define TA_STA		0x02
#define TA_PHS		0x04

#define TWF_ANDW	0x00
#define TWF_ORW		0x01

/* time out -----------------------------------------------------------------*/
#define TMO_POL		0	/* polling */
#define TMO_FEVR	-1	/* waiting forever */
#define TMO_NBLK	-2	/* non blocking */

/* error code ---------------------------------------------------------------*/
#define MERCD(x)	(x & 0xff)		/* main error code */
#define SERCD(x)	((x >> 8) & 0xff)	/* sub error code */

/* function code ------------------------------------------------------------*/
#define	TFN_CRE_TSK	-0x05	/* cre_tsk function code */
#define TFN_DEL_TSK	-0x06	/* del_tsk function code */
#define TFN_ACT_TSK	-0x07	/* act_tsk function code */
#define TFN_CAN_ACT	-0x08	/* can_act function code */
#define TFN_STA_TSK	-0x09	/* sta_tsk function code */ 
#define TFN_EXT_TSK	-0x0a	/* ext_tsk function code */
#define TFN_EXD_TSK	-0x0b	/* exd_tsk function code */
#define TFN_TER_TSK	-0x0c	/* ter_tsk function code */
#define TFN_CHG_PRI	-0x0d	/* chg_pri function code */
#define TFN_GET_PRI	-0x0e	/* get_pri function code */
#define TFN_REF_TSK	-0x0f	/* ref_tsk function code */
#define TFN_REF_TST	-0x10	/* ref_tst function code */
#define	TFN_SLP_TSK	-0x11	/* slp_tsk function code */
#define TFN_TSLP_TSK	-0x12	/* tslp_tsk function code */
#define TFN_WUP_TSK	-0x13	/* wup_tsk function code */
#define TFN_CAN_WUP	-0x14	/* can_wup function code */
#define TFN_REL_WAI	-0x15	/* rel_wai function code */
#define TFN_SUS_TSK	-0x16	/* sus_tsk function code */
#define TFN_RSM_TSK	-0x17	/* rsm_tsk function code */
#define TFN_FRSM_TSK	-0x18	/* frsm_tsk function code */
#define TFN_DLY_TSK	-0x19	/* dly_tsk function code */
#define TFN_DEF_TEX	-0x1b	/* def_tex function code */
#define TFN_RAS_TEX	-0x1c	/* ras_tex function code */
#define TFN_DIS_TEX	-0x1d	/* dis_tex function code */
#define TFN_ENA_TEX	-0x1e	/* ena_tex function code */
#define TFN_SNS_TEX	-0x1f	/* sns_tex function code */
#define TFN_REF_TEX	-0x20	/* ref_tex function code */
#define TFN_CRE_SEM	-0x21	/* cre_sem function code */
#define TFN_DEL_SEM	-0x22	/* del_sem function code */
#define TFN_SIG_SEM	-0x23	/* sig_sem function code */
#define TFN_WAI_SEM	-0x25	/* wai_sem function code */
#define TFN_POL_SEM	-0x26	/* pol_sem function code */
#define TFN_TWAI_SEM	-0x27	/* twai_sem function code */
#define TFN_REF_SEM	-0x28	/* ref_sem function code */
#define TFN_CRE_FLG	-0x29	/* cre_flg function code */
#define TFN_DEL_FLG	-0x2a	/* del_flg function code */
#define TFN_SET_FLG	-0x2b	/* set_flg function code */
#define TFN_CLR_FLG	-0x2c	/* clr_flg function code */
#define TFN_WAI_FLG	-0x2d	/* wai_flg function code */
#define TFN_POL_FLG	-0x2e	/* pol_flg function code */
#define TFN_TWAI_FLG	-0x2f	/* twai_flg function code */
#define TFN_REF_FLG	-0x30	/* ref_flg function code */
#define TFN_CRE_DTQ	-0x31	/* cre_dtq function code */
#define TFN_DEL_DTQ	-0x32	/* del_dtq function code */
#define TFN_SND_DTQ	-0x35	/* snd_dtq function code */
#define TFN_PSND_DTQ	-0x36	/* psnd_dtq function code */
#define TFN_TSND_DTQ	-0x37	/* tsnd_dtq function code */
#define TFN_FSND_DTQ	-0x38	/* fsnd_dtq function code */
#define TFN_RCV_DTQ	-0x39	/* rcv_dtq function code */
#define TFN_PRCV_DTQ	-0x3a	/* prcv_dtq function code */
#define TFN_TRCV_DTQ	-0x3b	/* trcv_dtq function code */
#define TFN_REF_DTQ	-0x3c	/* ref_dtq function code */
#define TFN_CRE_MBX	-0x3d	/* cre_mbx function code */
#define TFN_DEL_MBX	-0x3e	/* del_mbx function code */
#define TFN_SND_MBX	-0x3f	/* snd_mbx function code */
#define TFN_RCV_MBX	-0x41	/* rcv_mbx function code */
#define TFN_PRCV_MBX	-0x42	/* prcv_mbx function code */
#define TFN_TRCV_MBX	-0x43	/* trcv_mbx function code */
#define TFN_REF_MBX	-0x44	/* ref_mbx function code */
#define TFN_CRE_MPF	-0x45	/* cre_mpf function code */
#define TFN_DEL_MPF	-0x46	/* del_mpf function code */
#define TFN_REL_MPF	-0x47	/* rel_mpf function code */
#define TFN_GET_MPF	-0x49	/* get_mpf function code */
#define TFN_PGET_MPF	-0x4a	/* pget_mpf function code */
#define TFN_TGET_MPF	-0x4b	/* tget_mpf function code */
#define TFN_REF_MPF	-0x4c	/* ref_mpf function code */
#define TFN_SET_TIM	-0x4d	/* set_tim function code */
#define TFN_GET_TIM	-0x4e	/* get_tim function code */
#define TFN_CRE_CYC	-0x4f	/* cre_cyc function code */
#define TFN_DEL_CYC	-0x50	/* del_cyc function code */
#define TFN_STA_CYC	-0x51	/* sta_cyc function code */
#define TFN_STP_CYC	-0x52	/* stp_cyc function code */
#define TFN_REF_CYC	-0x53	/* ref_cyc function code */
#define TFN_ROT_RDQ	-0x55	/* rot_rdq function code */
#define TFN_GET_TID	-0x56	/* get_tid function code */
#define TFN_LOC_CPU	-0x59	/* loc_cpu function code */
#define TFN_UNL_CPU	-0x5a	/* unl_cpu function code */
#define TFN_DIS_DSP	-0x5b	/* dis_dsp function code */
#define TFN_ENA_DSP	-0x5c	/* ena_dsp function code */
#define TFN_SNS_CTX	-0x5d	/* sns_ctx function code */
#define TFN_SNS_LOC	-0x5e	/* sns_loc function code */
#define TFN_SNS_DSP	-0x5f	/* sns_dsp function code */
#define TFN_SNS_DPN	-0x60	/* sns_dpn function code */
#define TFN_REF_SYS	-0x61	/* ref_sys function code */
#define TFN_DEF_INH	-0x65	/* def_inh function code */
#define TFN_CRE_ISR	-0x66	/* cre_isr function code */
#define TFN_DEL_ISR	-0x67	/* del_isr function code */
#define TFN_REF_ISR	-0x68	/* ref_isr function code */
#define TFN_DIS_INT	-0x69	/* dis_int function code */
#define TFN_ENA_INT	-0x6a	/* ena_int function code */
#define TFN_DEF_SVC	-0x6d	/* def_svc function code */
#define TFN_DEF_EXC	-0x6e	/* def_exc function code */
#define TFN_REF_CFG	-0x6f	/* def_cfg function code */
#define TFN_REF_VER	-0x70	/* ref_ver function code */
#define TFN_IACT_TSK	-0x71	/* iact_tsk function code */
#define TFN_IWUP_TSK	-0x72	/* iwup_tsk function code */
#define TFN_IREL_WAI	-0x73	/* irel_wai function code */
#define TFN_IRAS_TEX	-0x74	/* iras_tex function code */
#define TFN_ISIG_SEM	-0x75	/* isig_sem function code */
#define TFN_ISET_FLG	-0x76	/* iset_flg function code */
#define TFN_IPSND_DTQ	-0x77	/* ipsnd_dtq function code */
#define TFN_IFSND_DTQ	-0x78	/* ifsnd_dtq function code */
#define TFN_IROT_RDQ	-0x79	/* irot_rdq function code */
#define TFN_IGET_TID	-0x7a	/* iget_tid function code */
#define TFN_ILOC_CPU	-0x7b	/* iloc_cpu function code */
#define TFN_IUNL_CPU	-0x7c	/* iunl_cpu function code */
#define TFN_ISIG_TIM	-0x7d	/* isig_tim function code */
#define TFN_CRE_MTX	-0x81	/* cre_mtx function code */
#define TFN_DEL_MTX	-0x82	/* del_mtx function code */
#define TFN_UNL_MTX	-0x83	/* unl_mtx function code */
#define TFN_LOC_MTX	-0x85	/* loc_mtx function code */
#define TFN_PLOC_MTX	-0x86	/* tloc_mtx function code */
#define TFN_TLOC_MTX	-0x87	/* ploc_mtx function code */
#define TFN_REF_MTX	-0x88	/* ref_mtx function code */
#define TFN_CRE_MBF	-0x89	/* cre_mbf function code */
#define TFN_DEL_MBF	-0x8a	/* del_mbf function code */
#define TFN_SND_MBF	-0x8d	/* snd_mbf function code */ 
#define TFN_PSND_MBF	-0x8e	/* psnd_mbf function code */
#define TFN_TSND_MBF	-0x8f	/* tsnd_mbf function code */
#define TFN_RCV_MBF	-0x91	/* rcv_mbf function code */
#define TFN_PRCV_MBF	-0x92	/* prcv_mbf function code */
#define TFN_TRCV_MBF	-0x93	/* trcv_mbf function code */
#define TFN_REF_MBF	-0x94	/* ref_mbf function code */
#define TFN_CRE_POR	-0x95	/* cre_por function code */
#define TFN_DEL_POR	-0x96	/* del_por function code */
#define TFN_CAL_POR	-0x97	/* cal_por function code */
#define TFN_TCAL_POR	-0x98	/* tcal_por function code */
#define TFN_ACP_POR	-0x99	/* acp_por function code */
#define TFN_PACP_POR	-0x9a	/* pacp_por function code */
#define TFN_TACP_POR	-0x9b	/* tacp_por function code */
#define TFN_FWD_POR	-0x9c	/* fwd_por function code */
#define TFN_RPL_RDV	-0x9d	/* rpl_rdv function code */
#define TFN_REF_POR	-0x9e	/* ref_por function code */
#define TFN_REF_RDV	-0x9f	/* ref_rdv function code */
#define TFN_CRE_MPL	-0xa1	/* cre_mpl function code */
#define TFN_DEL_MPL	-0xa2	/* del_mpl function code */
#define TFN_REL_MPL	-0xa3	/* rel_mpl function code */
#define TFN_GET_MPL	-0xa5	/* get_mpl function code */
#define TFN_PGET_MPL	-0xa6	/* pget_mpl function code */
#define TFN_TGET_MPL	-0xa7	/* tget_mpl function code */
#define TFN_REF_MPL	-0xa8	/* ref_mpl function code */
#define TFN_CRE_ALM	-0xa9	/* cre_alm function code */ 	
#define TFN_DEL_ALM	-0xaa	/* del_alm function code */
#define TFN_STA_ALM	-0xab	/* sta_alm function code */
#define TFN_STP_ALM	-0xac	/* stp_alm function code */
#define TFN_REF_ALM	-0xad	/* ref_alm function code */
#define TFN_DEF_OVR	-0xb1	/* def_ovr function code */
#define TFN_STA_OVR	-0xb2	/* sta_ovr function code */
#define TFN_STP_OVR	-0xb3	/* stp_ovr function code */
#define TFN_REF_OVR	-0xb4	/* ref_ovr function code */
#define TFN_ACRE_TSK	-0xc1	/* acre_tsk function code */
#define TFN_ACRE_SEM	-0xc2	/* acre_sem function code */
#define TFN_ACRE_FLG	-0xc3	/* acre_flg function code */
#define TFN_ACRE_DTQ	-0xc4	/* acre_dtq function code */
#define TFN_ACRE_MBX	-0xc5	/* acre_mbx function code */
#define TFN_ACRE_MTX	-0xc6	/* acre_mtx function code */
#define TFN_ACRE_MBF	-0xc7	/* acre_mbf function code */
#define TFN_ACRE_POR	-0xc8	/* acre_por function code */
#define TFN_ACRE_MPF	-0xc9	/* acre_mpf function code */
#define TFN_ACRE_MPL	-0xca	/* acre_mpl function code */
#define TFN_ACRE_CYC	-0xcb	/* acre_cyc function code */
#define TFN_ACRE_ALM	-0xcc	/* acre_alm function code */ 
#define TFN_ACRE_ISR	-0xcd	/* acre_isr function code */

#define TFN_EXD_PRINT	-0xe1	/* printf function code */
#define TFN_EXD_TCPIP	-0xe2	/* tcp/ip function code */

/* extended (non-ITRON) syscalls -------------------------------------------*/
#define TFN_EXD_VGA_WRITE   -0xe3	/* vga_write_at */
#define TFN_EXD_VGA_DEC     -0xe4	/* vga_write_dec_at */
#define TFN_EXD_VGA_CLEAR   -0xe5	/* vga_clear */
#define TFN_EXD_VGA_FILL    -0xe6	/* vga_fill_at */
#define TFN_EXD_KEY_GETC    -0xe7	/* key_getc */
#define TFN_EXD_KEY_SETTASK -0xe8	/* set key_task_id */
#define TFN_EXD_STACK_ALLOC -0xe9	/* tsk_stack_alloc */
#define TFN_EXD_VGA_CURSOR  -0xea	/* vga_set_cursor */

/* task status --------------------------------------------------------------*/
#define TTS_RUN		0x01
#define TTS_RDY		0x02
#define TTS_WAI		0x04
#define TTS_SUS		0x08
#define TTS_WAS		0x0c
#define TTS_DMT		0x10
#define TTS_NON		0x00

/* waiting ------------------------------------------------------------------*/
#define TTW_SLP		0x0001	/* waiting wake-up */
#define TTW_DLY		0x0002	/* waiting time passage */
#define TTW_SEM		0x0004	/* waiting semapho resource */
#define TTW_FLG		0x0008	/* waiting ivent flag */
#define TTW_SDTQ	0x0010	/* wait status of sending data queue */
#define TTW_RDTQ	0x0020	/* wait status of receiving data queue */
#define TTW_MBX		0x0040	/* wait status of receiving mail box */ 
#define TTW_MTX		0x0080	/* wait status of locking mutex lock */
#define TTW_SMBF	0x0100	/* wait status of sending message buffer */
#define TTW_RMBF	0x0200	/* wait status of receiving message buffer */
#define TTW_CAL		0x0400	/* wait status of calling randezvous */
#define TTW_ACP		0x0800	/* wait status of receiving randezvous */
#define TTW_RDV		0x1000	/* wait status of raandezvous exiting */
#define TTW_MPF		0x2000	/* getting fixed size memory */
#define TTW_MPL		0x4000	/* getting variable size memory */

#define TTEX_ENA	0x00
#define TTEX_DIS	0x01
#define TCYC_STP	0x00
#define TCYC_STA	0x01

#define TALM_STP	0x00
#define TALM_STA	0x01

#define TOVR_STP	0x00
#define TOVR_STA	0x01

#define TSK_SELF	0
#define TSK_NONE	0
#define TPRI_SELF	0
#define TPRI_INI	0

#include "types.h"
#if 0
/* task management ----------------------------------------------------------*/
ER	cre_tsk(ID, T_CTSK*);
ER_ID	acre_tsk(T_CTSK*);	
ER	del_tsk(ID);
ER	act_tsk(ID);
ER	iact_tsk(ID);
ER_UINT	can_act(ID);
ER	sta_tsk(ID, VP_INT);
void	ext_tsk(void);
void	exd_tsk(void);
ER	ter_tsk(ID);
ER	chg_pri(ID, PRI);
ER	get_pri(ID, PRI*);
ER	ref_tsk(ID, T_RTSK*);
ER	ref_tst(ID, T_RTST*);

/* task corresponding function ----------------------------------------------*/
ER	slp_tsk(void);
ER	tslp_tsk(TMO);
ER	wup_tsk(ID);
ER	iwup_tsk(ID);
ER_UINT	can_wup(ID);
ER	rel_wai(ID);
ER	irel_wai(ID);
ER	sus_tsk(ID);
ER	rsm_tsk(ID);
ER	frsm_tsk(ID);
ER	dly_tsk(RELTIM);

/* task exception function --------------------------------------------------*/

ER	loc_cpu(void);
ER	unl_cpu(void);
#endif

#endif
