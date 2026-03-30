/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_FLG_H
#define _SYS_FLG_H

void flg_init(void);
ER sys_cre_flg(W, ID, T_CFLG*);
ER_ID sys_acre_flg(W, T_CFLG*);
ER sys_del_flg(W, ID);
ER sys_set_flg(W, ID, FLGPTN);
ER sys_iset_flg(W, ID, FLGPTN);
ER sys_clr_flg(W, ID, FLGPTN);
ER sys_wai_flg(W, ID, FLGPTN, MODE, FLGPTN*);
ER sys_pol_flg(W, ID, FLGPTN, MODE, FLGPTN*);
ER sys_twai_flg(W, ID, FLGPTN, MODE, FLGPTN*, TMO);
ER sys_ref_flg(W, ID, T_RFLG*);

#endif
