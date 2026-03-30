/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_ISR_H
#define _SYS_ISR_H

void isr_init(void);
ER sys_def_inh(W, INHNO, T_DINH*);
ER sys_cre_isr(W, ID, T_CISR*);
ER_ID sys_acre_isr(W, T_CISR*);
ER sys_del_isr(W, ID);
ER sys_ref_isr(W, ID, T_RISR*);
ER sys_dis_int(W, INTNO);
ER sys_ena_int(W, INTNO);
ER sys_def_svc(W, FN, T_DSVC*);
ER_UINT sys_cal_svc(W, FN, VP_INT, VP_INT, ...);
ER sys_def_exc(W, EXCNO, T_DEXC*);
ER sys_ref_cfg(W, T_RCFG*);
ER sys_ref_ver(W, T_RVER*);

#endif
