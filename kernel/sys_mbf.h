/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_MBF_H
#define _SYS_MBF_H

void mbf_init(void);
ER sys_cre_mbf(W, ID, T_CMBF*);
ER_ID sys_acre_mbf(W, T_CMBF*);
ER sys_del_mbf(W, ID);
ER sys_snd_mbf(W, ID, VP, UINT);
ER sys_psnd_mbf(W, ID, VP, UINT);
ER sys_tsnd_mbf(W, ID, VP, UINT, TMO);
ER_UINT sys_rcv_mbf(W, ID, VP);
ER_UINT sys_prcv_mbf(W, ID, VP);
ER_UINT sys_trcv_mbf(W, ID, VP, TMO);
ER sys_ref_mbf(W, ID, T_RMBF*);

#endif
