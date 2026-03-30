/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_MPL_H
#define _SYS_MPL_H

void mpl_init(void);
ER sys_cre_mpl(W, ID, T_CMPL*);
ER sys_acre_mpl(W, T_CMPL*);
ER sys_del_mpl(W, ID);
ER sys_get_mpl(W, ID, UINT, VP*);
ER sys_pget_mpl(W, ID, UINT, VP*);
ER sys_tget_mpl(W, ID, UINT, VP*, TMO);
ER sys_rel_mpl(W, ID, VP);
ER sys_ref_mpl(W, ID, T_RMPL*);

#endif
