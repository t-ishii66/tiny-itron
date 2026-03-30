/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/config.h"
#include "kernelval.h"

/* cpu number (0 = BSP, incremented by AP) ----------------------------------*/
int	cpu_num = 0;

/* pointer to proc ----------------------------------------------------------*/
proc_t*	current_proc[MAX_CPU];	/* points to current process (per-CPU) */

/* proc ---------------------------------------------------------------------*/
proc_t	proc[MAX_TSKID];	/* process table */
ID	c_tskid[MAX_CPU];	/* current task id (per-CPU) */

/* cpu_status ---------------------------------------------------------------*/
extern int	cpu_stat;		/* CPU_LOCK or CPU_UNLOCK */
extern int	dispatch_stat;		/* DISPATCH_ENABLE or DISPATCH_DISABLE */

/* timer --------------------------------------------------------------------*/
int		timer_return[MAX_CPU];
unsigned long	clock_tick[MAX_CPU];
unsigned long	lost_tick[MAX_CPU];
