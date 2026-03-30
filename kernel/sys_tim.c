/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"

/* tim_init -----------------------------------------------------------------*/
void
tim_init(void)
{
	system_time.l = 0;
	system_time.h = 0;
}

/* sys_set_tim --------------------------------------------------------------*/
ER
sys_set_tim(W apic, SYSTIM* p_systim)
{
	system_time.l = p_systim->l;
	system_time.h = p_systim->h;
	return E_OK;
}

/* sys_get_tim --------------------------------------------------------------*/
ER
sys_get_tim(W apic, SYSTIM* p_systim)
{
	p_systim->l = system_time.l;
	p_systim->h = system_time.h;
	return E_OK;
}

/* sys_isig_tim (not system call) -------------------------------------------*/
/* Called from timer_intr() on each tick (~17ms, PIT HZ=60).
 * Advances system_time by TIC_NUME milliseconds.
 * Other tick-driven processing (sched_timeout, cyc_intr, alm_intr)
 * is handled separately by timer_intr / the APIC timer handler.      */
void
sys_isig_tim(W apic)
{
	unsigned long old_l = system_time.l;
	system_time.l += TIC_NUME;
	if (system_time.l < old_l)	/* overflow: carry to high word */
		system_time.h++;
}
