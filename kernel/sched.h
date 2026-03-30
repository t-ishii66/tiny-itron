/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SCHED_H
#define _SCHED_H

/* cpu lock -----------------------------------------------------------------*/
#define CPU_LOCK	1
#define CPU_UNLOCK	2

/* dispatch -----------------------------------------------------------------*/
#define DISPATCH_ENABLE		1
#define DISPATCH_DISABLE	2
#define DISPATCH_SUSPEND	3

/* Big Kernel Lock (protects all kernel data structures) -------------------*/
extern unsigned long kernel_lk;

/* function declarations ----------------------------------------------------*/
void sched_init(void);
ER sched_ins(PRI, T_LINK*);
void sched_rem(T_LINK*);
ID sched_hold_tsk(ID, STAT);
ID sched_next_tsk(W);
void sched_timeout_ins(T_TIMEOUT*);
void sched_timeout_rem(T_TIMEOUT*);
void sched_timeout_rem_if_exist(T_TIMEOUT*);
void sched_timeout(W, unsigned long);

#endif
