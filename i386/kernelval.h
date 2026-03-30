/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "proc.h"

#ifndef _KERNELVAL_H
#define _KERNELVAL_H

#define MAX_CPU		2

extern proc_t*	current_proc[];
extern proc_t	proc[];

extern ID	c_tskid[];

extern int	cpu_num;

extern int	cpu_stat;
extern int	dispatch_stat;

extern int		timer_return[];
extern unsigned long	clock_tick[];
extern unsigned long	lost_tick[];

#endif
