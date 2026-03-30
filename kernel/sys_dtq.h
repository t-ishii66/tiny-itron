/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_DTQ_H
#define _SYS_DTQ_H

void dtq_init(void);
ER sys_cre_dtq(W, ID, T_CDTQ*);
ER_ID sys_acre_dtq(W, T_CDTQ*);
ER sys_del_dtq(W, ID);
ER sys_snd_dtq(W, ID, VP_INT);
ER sys_psnd_dtq(W, ID, VP_INT);
ER ipsnd_dtq(W, ID, VP_INT);
ER sys_tsnd_dtq(W, ID, VP_INT, TMO);
ER sys_fsnd_dtq(W, ID, VP_INT);
ER sys_ifsnd_dtq(W, ID, VP_INT);
ER sys_rcv_dtq(W, ID, VP_INT*);
ER sys_prcv_dtq(W, ID, VP_INT*);
ER sys_trcv_dtq(W, ID, VP_INT*, TMO);
ER sys_ref_dtq(W, ID, T_RDTQ*);

#endif
