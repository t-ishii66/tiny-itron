/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../i386/addr.h"
#include "../include/itron.h"
#include "../include/types.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "pool.h"
#include "sched.h"
#include "sys_tsk.h"
#include "sys_mbf.h"
#include "sys_dtq.h"
#include "kernel.h"

/* linker symbols ----------------------------------------------------------*/
extern char _user_data_end;

/* system variables --------------------------------------------------------*/
T_TSK		tsk[MAX_TSKID + 1];			/* task structure */
T_LINK		tsk_pri[TMAX_TPRI + 1];			/* task priority */
T_TIMEOUT	timeout;				/* time out */

T_SEM		sem[MAX_SEMID + 1];
T_FLG		flg[MAX_FLGID + 1];
T_DTQ		dtq[MAX_DTQID + 1];
T_MBX		mbx[MAX_MBXID + 1];
T_MTX		mtx[MAX_MTXID + 1];
T_POR		por[MAX_PORID + 1];
SYSTIM		system_time;
T_SVC		svc[MAX_FNCD + 1];
T_MBF		mbf[MAX_MBFID + 1];
T_MPF		mpf[MAX_MPFID + 1];
T_MPL		mpl[MAX_MPLID + 1];
T_CYC		cyc[MAX_CYCID + 1];
T_ALM		alm[MAX_ALMID + 1];
T_ISR		isr[MAX_ISRID + 1];
T_INH		inh[MAX_INHID + 1];
T_EXC		exc[MAX_EXCID + 1];

/* itron_init --------------------------------------------------------------*/
int
itron_init(void)
{
	tsk_init();
	stack_init((VP)STACK_START, (VP)STACK_END);
	mem_init((VP)MEM_START, (VP)MEM_END);
	kmem_init((VP)&_user_data_end, (VP)MEM_START);
	sched_init();

	mbf_init();
	dtq_init();
	sem_init();

	return 0;
}

/* bcopy --------------------------------------------------------------------*/
void
bcopy(unsigned char* from, unsigned char* to, unsigned long count)
{
	while (count --)
		*to ++ = *from ++;
}

