/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_MTX_H
#define _SYS_MTX_H

void mtx_init(void);
ER sys_cre_mtx(W, ID, T_CMTX*);
ER_ID sys_acre_mtx(W, T_CMTX*);
ER sys_del_mtx(W, ID);
ER sys_loc_mtx(W, ID);
ER sys_ploc_mtx(W, ID);
ER sys_tloc_mtx(W, ID, TMO);
ER sys_unl_mtx(W, ID);
ER sys_ref_mtx(W, ID, T_RMTX*);

#endif
