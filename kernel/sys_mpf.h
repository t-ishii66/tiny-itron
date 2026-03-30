/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_MPF_H
#define _SYS_MPF_H

void mpf_init(void);
ER sys_cre_mpf(W, ID, T_CMPF*);
ER_ID sys_acre_mpf(W, T_CMPF*);
ER sys_del_mpf(W, ID);
ER sys_get_mpf(W, ID, VP*);
ER sys_pget_mpf(W, ID, VP*);
ER sys_tget_mpf(W, ID, VP*, TMO);
ER sys_rel_mpf(W, ID, VP);
ER sys_ref_mpf(W, ID, T_RMPF*);

#endif
