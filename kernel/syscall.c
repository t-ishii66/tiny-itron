/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/syscall.h"
#include "../include/itron.h"
#include "../i386/proc.h"
#include "types.h"
#include "sys_tsk.h"
#include "sys_sns.h"
#include "sys_sem.h"
#include "sys_flg.h"
#include "sys_dtq.h"
#include "sys_mbx.h"
#include "sys_mpf.h"
#include "sys_tim.h"
#include "sys_cyc.h"

#include "sys_mbf.h"
#include "sys_mpl.h"
#include "sys_ovr.h"
#include "sys_mtx.h"
#include "sys_isr.h"
#include "sys_por.h"
#include "sys_alm.h"
#include "sys_dtq.h"
#include "sys_rdq.h"

#include "syscallP.h"

/* itron_syscall -------------------------------------------------------------*/
W
itron_syscall(unsigned long apic, unsigned long sysid, unsigned long arg1,
	unsigned long arg2, unsigned long arg3, unsigned long arg4,
	unsigned long arg5, unsigned long arg6)
{
	ER	ret;
	if (sysid == -TFN_EXD_TCPIP) {
		sysid = arg1 - (-TFN_TCP_CRE_REP);
		ret = (*syscall_tcpip_entry[sysid].func)
					(apic, arg2, arg3, arg4, arg5, arg6);
	} else {
		ret = (*syscall_entry[sysid].func)
				(apic, arg1, arg2, arg3, arg4, arg5, arg6);
	}
	return ret;
}

/* sys_dummy ----------------------------------------------------------------*/
ER
sys_dummy(void)
{
	return E_NOSPT;
}
