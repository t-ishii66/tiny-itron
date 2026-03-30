/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_RDQ_H
#define _SYS_RDQ_H

ER sys_rot_rdq(W, PRI);
ER sys_irot_rdq(W, PRI);
ER sys_get_tid(W, ID*);
ER sys_iget_tid(W, ID*);
ER sys_loc_cpu(W);
ER sys_iloc_cpu(W);
ER sys_unl_cpu(W);
ER sys_iunl_cpu(W);
ER sys_dis_dsp(W);
ER sys_ena_dsp(W);
BOOL sys_sns_ctx(W);
BOOL sys_sns_loc(W);
BOOL sys_sns_dsp(W);
BOOL sys_sns_dpn(W);
ER sys_ref_sys(W, T_RSYS*);

#endif
