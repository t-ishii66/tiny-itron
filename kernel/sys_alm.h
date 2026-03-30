/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_ALM_H
#define _SYS_ALM_H

void alm_init(void);
ER sys_cre_alm(W, ID, T_CALM*);
ER_ID sys_acre_alm(W, T_CALM*);
ER sys_del_alm(W, ID);
ER sys_sta_alm(W, ID, RELTIM);
ER sys_stp_alm(W, ID);
ER sys_ref_alm(W, ID, T_RALM*);

#endif
