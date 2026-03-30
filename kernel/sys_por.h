/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_POR_H
#define _SYS_POR_H

void por_init(void);
ER sys_cre_por(W, ID, T_CPOR*);
ER_ID sys_acre_por(W, T_CPOR*);
ER sys_del_por(W, ID);
ER_UINT sys_cal_por(W, ID, RDVPTN, VP, UINT);
ER_UINT sys_tcal_por(W, ID, RDVPTN, VP, UINT, TMO);
ER_UINT sys_acp_por(W, ID, RDVPTN, RDVNO*, VP);
ER_UINT sys_pacp_por(W, ID, RDVPTN, RDVNO*, VP);
ER_UINT sys_tacp_por(W, ID, RDVPTN, RDVNO*, VP, TMO);
ER sys_fwd_por(W, ID, RDVPTN, RDVNO, VP, UINT);
ER sys_rpl_rdv(W, RDVNO, VP, UINT);
ER sys_ref_por(W, ID, T_RPOR*);
ER sys_ref_rdv(W, RDVNO, T_RRDV*);

#endif
