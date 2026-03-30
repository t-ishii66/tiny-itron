/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_TSK_H
#define _SYS_TSK_H

ER sys_cre_tsk(W, ID, T_CTSK*);
ER_ID sys_acre_tsk(W, T_CTSK*);
ER sys_del_tsk(W, ID);
ER sys_act_tsk(W, ID);
ER sys_iact_tsk(W, ID);
ER sys_can_act(W, ID);
ER sys_sta_tsk(W, ID, VP_INT);
void sys_ext_tsk(W);
void sys_exd_tsk(W);
ER sys_ter_tsk(W, ID);
ER sys_chg_pri(W, ID, PRI);
ER sys_get_pri(W, ID, PRI*);
ER sys_ref_tsk(W, ID, T_RTSK*);
ER sys_ref_tst(W, ID, T_RTST*);
ER sys_slp_tsk(W);
ER sys_tslp_tsk(W, TMO);
ER sys_wup_tsk(W, ID);
ER iwup_tsk(W, ID);
ER_UINT sys_can_wup(W, ID);
ER sys_rel_wai(W, ID);
ER sys_irel_wai(W, ID);
ER sys_sus_tsk(W, ID);
ER sys_rsm_tsk(W, ID);
ER sys_frsm_tsk(W, ID);
ER sys_dly_tsk(W, RELTIM);
ER sys_def_tex(W, ID, T_DTEX*);
ER sys_ras_tex(W, ID, TEXPTN);
ER sys_iras_tex(W, ID, TEXPTN);
ER sys_dis_tex(W);
ER sys_ena_tex(W);
BOOL sys_sns_tex(W);
ER sys_ref_tex(W, ID, T_RTEX*);
ER sys_printf(W, char**);

void tsk_stat_change(ID, STAT);
void tsk_init(void);

#endif
