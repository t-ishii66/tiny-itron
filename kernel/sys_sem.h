/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_SEM_H
#define _SYS_SEM_H

ER sys_cre_sem(W, ID, T_CSEM*);
ER_ID sys_acre_sem(W, T_CSEM*);
ER sys_del_sem(W, ID);
ER sys_sig_sem(W, ID);
ER sys_isig_sem(W, ID);
ER sys_wai_sem(W, ID);
ER sys_pol_sem(W, ID);
ER sys_twai_sem(W, ID, TMO);
ER sys_ref_sem(W, ID, T_RSEM*);

void sem_init(void);
void ins_fifo(T_LINK*, T_LINK*);
void ins_pri(T_LINK*, T_LINK*);
void wlink_ins(T_LINK*, T_LINK*);
void wlink_rem(T_LINK*);
void wlink_change(T_LINK*, T_LINK*);
void wlink_dump(T_LINK*);

#endif
