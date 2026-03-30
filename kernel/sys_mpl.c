/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"
#include "pool.h"


/* mpl_init -----------------------------------------------------------------*/
void
mpl_init(void)
{
	int	i;
	for (i = 1 ; i < MAX_MPLID ; i ++) {
		mpl[i].wlink.next = &mpl[i].wlink;
		mpl[i].wlink.prev = &mpl[i].wlink;
		mpl[i].act = 0;
	}
}

/* sys_cre_mpl --------------------------------------------------------------*/
ER
sys_cre_mpl(W apic, ID mplid, T_CMPL* pk_cmpl)
{
	if (mplid < 1 || mplid > MAX_MPLID)
		return E_ID;
	if (mpl[mplid].act)
		return E_OBJ;

	mpl[mplid].mplatr = pk_cmpl->mplatr;
	mpl[mplid].mplsz = pk_cmpl->mplsz;
	mpl[mplid].mpl = pk_cmpl->mpl;

	if (mpl[mplid].mpl == NULL) {
		mpl[mplid].mpl = mem_alloc(mpl[mplid].mplsz);
		mpl[mplid].mpl_alloc = 1;
	} else
		mpl[mplid].mpl_alloc = 0;

	mpl[mplid].pool = kmem_alloc(MAX_MPL_POOL * sizeof(allocation_t));
	pool_init(mpl[mplid].pool, mpl[mplid].mpl,
					mpl[mplid].mpl + mpl[mplid].mplsz);
	mpl[mplid].act = 1;
	return E_OK;
}

/* sys_acre_mpl -------------------------------------------------------------*/
ER_ID
sys_acre_mpl(W apic, T_CMPL* pk_cmpl)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_MPLID; i++) {
		if (mpl[i].act == 0) {
			if ((ret = sys_cre_mpl(apic, i, pk_cmpl)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_mpl --------------------------------------------------------------*/
ER
sys_del_mpl(W apic, ID mplid)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mplid < 1 || mplid > MAX_MPLID)
		return E_ID;
	if (mpl[mplid].act == 0)
		return E_NOEXS;

	wlink = mpl[mplid].wlink.next;
	while (wlink != &mpl[mplid].wlink) {
		t = wlink2tsk(wlink);
		t->tskstat = TTS_RDY;
		proc_set_return_value(t->proc, E_DLT);
		sched_ins(t->tskpri, &(t->plink));
		wlink = wlink->next;
	}

	if (mpl[mplid].mpl_alloc)
		mem_free(mpl[mplid].mpl);
	kmem_free(mpl[mplid].pool);
	mpl[mplid].wlink.next = &mpl[mplid].wlink;
	mpl[mplid].wlink.prev = &mpl[mplid].wlink;
	mpl[mplid].act = 0;
	return E_OK;
}

/* sys_get_mpl --------------------------------------------------------------*/
ER
sys_get_mpl(W apic, ID mplid, UINT blksz, VP* p_blk)
{
	return sys_tget_mpl(apic, mplid, blksz, p_blk, TMO_FEVR);
}

/* sys_pget_mpl -------------------------------------------------------------*/
ER
sys_pget_mpl(W apic, ID mplid, UINT blksz, VP* p_blk)
{
	return sys_tget_mpl(apic, mplid, blksz, p_blk, TMO_POL);
}

/* sys_tget_mpl -------------------------------------------------------------*/
ER
sys_tget_mpl(W apic, ID mplid, UINT blksz, VP* p_blk, TMO tmout)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mplid < 1 || mplid > MAX_MPLID)
		return E_ID;
	if (mpl[mplid].act == 0)
		return E_NOEXS;

	*p_blk = pool_alloc(mpl[mplid].pool, blksz, MAX_MPL_POOL);
	if (*p_blk != NULL)
		return E_OK;

	if (tmout == TMO_POL)
		return E_TMOUT;
	
	if (mpl[mplid].mplatr & TA_TFIFO) {
		ins_fifo(&mpl[mplid].wlink, &tsk[c_tskid[apic]].wlink);
	} else {
		ins_pri(&mpl[mplid].wlink, &tsk[c_tskid[apic]].wlink);
	}
	tsk[c_tskid[apic]].p_blk = p_blk;
	tsk[c_tskid[apic]].blksz = blksz;
	if (tmout == TMO_FEVR)
		sys_slp_tsk(apic);
	else
		sys_tslp_tsk(apic, tmout);

	return E_OK;
}

/* sys_rel_mpl --------------------------------------------------------------*/
ER
sys_rel_mpl(W apic, ID mplid, VP blk)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mplid < 1 || mplid > MAX_MPLID)
		return E_ID;
	if (mpl[mplid].act == 0)
		return E_NOEXS;
	
	pool_free(mpl[mplid].pool, blk);
	wlink = mpl[mplid].wlink.next;
	while (wlink != &mpl[mplid].wlink) {
		t = wlink2tsk(wlink);
		if ((*(t->p_blk) = pool_alloc(mpl[mplid].pool, t->blksz,
						MAX_MPL_POOL)) == NULL)
			break;
		t->tskstat = TTS_RDY;
		wlink = wlink->next;  
		wlink_rem(wlink->prev);
		sched_ins(t->tskpri, &(t->plink));
	}

	return E_OK;
}

/* sys_ref_mpl --------------------------------------------------------------*/
ER
sys_ref_mpl(W apic, ID mplid, T_RMPL* pk_rmpl)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mplid < 1 || mplid > MAX_MPLID)
		return E_ID;
	if (mpl[mplid].act == 0)
		return E_NOEXS;

	wlink = mpl[mplid].wlink.next;
	if (wlink != &mpl[mplid].wlink) {
		t = wlink2tsk(wlink);
		pk_rmpl->wtskid = t->tskid;
	} else
		pk_rmpl->wtskid = TSK_NONE;

	pk_rmpl->fmplsz = 0;	/* m(..)m */
	pk_rmpl->fblksz = 0;	/* m(..)m */

	return E_OK;
}
