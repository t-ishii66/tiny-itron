/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_CYC_H
#define _SYS_CYC_H

void cyc_init(void);
ER sys_cre_cyc(W, ID, T_CCYC*);
ER_ID sys_acre_cyc(W, T_CCYC*);
ER sys_del_cyc(W, ID);
ER sys_sta_cyc(W, ID);
ER sys_stp_cyc(W, ID);
ER sys_ref_cyc(W, ID, T_RCYC*);

#endif
