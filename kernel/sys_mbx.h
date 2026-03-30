/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_MBX_H
#define _SYS_MBX_H

void mbx_init(void);
ER sys_cre_mbx(W, ID, T_CMBX*);
ER_ID sys_acre_mbx(W, T_CMBX*);
ER sys_del_mbx(W, ID);
ER sys_snd_mbx(W, ID, T_MSG*);
ER sys_rcv_mbx(W, ID, T_MSG**);
ER sys_prcv_mbx(W, ID, T_MSG**);
ER sys_trcv_mbx(W, ID, T_MSG**, TMO);
ER sys_ref_mbx(W, ID, T_RMBX*);

void mbx_ins(ID, PRI, T_MSG*);
void mbx_rem(ID, PRI);

#endif
