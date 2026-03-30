/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _VAL_H
#define _VAL_H

extern T_TSK	tsk[];
extern T_LINK	tsk_pri[];
extern ID	c_tskid[];
extern T_TIMEOUT	timeout;
extern T_SEM	sem[];
extern T_FLG	flg[];
extern T_MTX	mtx[];
extern T_POR	por[];
extern T_DTQ	dtq[];
extern T_MBX	mbx[];
extern T_MBF	mbf[];
extern T_MPF	mpf[];
extern T_MPL	mpl[];
extern SYSTIM	system_time;
extern T_CYC	cyc[];
extern T_ALM	alm[];
extern int	dispatch_stat;
extern int	cpu_stat;

extern T_ISR	isr[];
extern T_INH	inh[];
extern T_EXC	exc[];
extern T_SVC	svc[];
#endif
