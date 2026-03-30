/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../i386/proc.h"
#include "../include/config.h"
#include "types.h"
#include "val.h"
#include "pool.h"

/* mpf_init -----------------------------------------------------------------*/
void
mpf_init(void)
{
	int	i;
	for (i = 0 ; i <= MAX_MPFID ; i ++) {
		mpf[i].wlink.next = &(mpf[i].wlink);
		mpf[i].wlink.prev = &(mpf[i].wlink);
		mpf[i].act = 0;
	} 
}

/* sys_cre_mpf --------------------------------------------------------------*/
ER
sys_cre_mpf(W apic, ID mpfid, T_CMPF* pk_cmpf)
{
	if (mpfid < 1 || mpfid > MAX_MPFID)
		return E_ID;
	if (mpf[mpfid].act)
		return E_OBJ;

	mpf[mpfid].mpfatr = pk_cmpf->mpfatr;
	mpf[mpfid].blkcnt = pk_cmpf->blkcnt;
	mpf[mpfid].blksz = pk_cmpf->blksz;
	mpf[mpfid].mpf = pk_cmpf->mpf;

	if (mpf[mpfid].mpf == NULL) {
		mpf[mpfid].mpf = mem_alloc(
				mpf[mpfid].blkcnt * mpf[mpfid].blksz);
		mpf[mpfid].mpf_alloc = 1;
	} else
		mpf[mpfid].mpf_alloc = 0;

	mpf[mpfid].pool = kmem_alloc(sizeof(allocation_t) * mpf[mpfid].blkcnt);
	pool_init(mpf[mpfid].pool, mpf[mpfid].mpf, 
				mpf[mpfid].blkcnt * mpf[mpfid].blksz);

	mpf[mpfid].act = 1;
	return E_OK;
}

/* sys_acre_mpf -------------------------------------------------------------*/
ER_ID
sys_acre_mpf(W apic, T_CMPF* pk_cmpf)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_MPFID; i++) {
		if (mpf[i].act == 0) {
			if ((ret = sys_cre_mpf(apic, i, pk_cmpf)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_mpf --------------------------------------------------------------*/
ER
sys_del_mpf(W apic, ID mpfid)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mpfid < 1 || mpfid > MAX_MPFID)
		return E_ID;
	if (mpf[mpfid].act == 0)
		return E_NOEXS;

	wlink = mpf[mpfid].wlink.next;
	while (wlink != &(mpf[mpfid].wlink)) {
		t = wlink2tsk(wlink);
		t->tskstat = TTS_RDY;
		proc_set_return_value(t->proc, E_DLT);
		sched_ins(t->tskpri, &(t->plink));
		wlink = wlink->next;
	}
	if (mpf[mpfid].mpf_alloc)
		mem_free(mpf[mpfid].mpf);

	kmem_free(mpf[mpfid].pool);
	mpf[mpfid].wlink.next = &(mpf[mpfid].wlink);
	mpf[mpfid].wlink.prev = &(mpf[mpfid].wlink);
	mpf[mpfid].act = 0;
	return E_OK;
}

/* sys_get_mpf --------------------------------------------------------------*/
ER
sys_get_mpf(W apic, ID mpfid, VP* p_blk)
{
	return sys_tget_mpf(apic, mpfid, p_blk, TMO_FEVR);
}

/* sys_pget_mpf -------------------------------------------------------------*/
ER
sys_pget_mpf(W apic, ID mpfid, VP* p_blk)
{
	return sys_tget_mpf(apic, mpfid, p_blk, TMO_POL);
}

/* sys_tget_mpf -------------------------------------------------------------*/
ER
sys_tget_mpf(W apic, ID mpfid, VP* p_blk, TMO tmout)
{
	if (mpfid < 1 || mpfid > MAX_MPFID)
		return E_ID;
	if (mpf[mpfid].act == 0)
		return E_NOEXS;
	if ((*p_blk = pool_alloc(mpf[mpfid].pool, mpf[mpfid].blksz,
						mpf[mpfid].blkcnt)) != NULL) {
		return E_OK;
	}

	if (tmout == TMO_POL)
		return E_TMOUT;

	tsk[c_tskid[apic]].p_blk = p_blk;
	if (mpf[mpfid].mpfatr & TA_TFIFO) {
		ins_fifo(&mpf[mpfid].wlink, &tsk[c_tskid[apic]].wlink);
	} else {	/* should be TA_TPRI */
		ins_pri(&mpf[mpfid].wlink, &tsk[c_tskid[apic]].wlink);
	}

	if (tmout == TMO_FEVR)
		sys_slp_tsk(apic);
	else
		sys_tslp_tsk(apic, tmout);

	return E_OK;
}

/* sys_rel_mpf --------------------------------------------------------------*/
ER
sys_rel_mpf(W apic, ID mpfid, VP blk)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mpfid < 1 || mpfid > MAX_MPFID)
		return E_ID;
	if (mpf[mpfid].act == 0)
		return E_NOEXS;

	wlink = mpf[mpfid].wlink.next;
	if (wlink != &(mpf[mpfid].wlink)) {
		t = wlink2tsk(wlink);	
		wlink_rem(wlink);
		*(t->p_blk) = blk;
		t->tskstat = TTS_RDY;
		sched_ins(t->tskpri, &(t->plink));
		return E_OK;
	}
	pool_free(mpf[mpfid].pool, blk);
	return E_OK;
}

/* sys_ref_mpf --------------------------------------------------------------*/
ER
sys_ref_mpf(W apic, ID mpfid, T_RMPF* pk_rmpf)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mpfid < 1 || mpfid > MAX_MPFID)
		return E_ID;
	if (mpf[mpfid].act == 0)
		return E_NOEXS;

	wlink = mpf[mpfid].wlink.next;
	if (wlink != &(mpf[mpfid].wlink)) {
		t = wlink2tsk(wlink);
		pk_rmpf->wtskid = t->tskid;
	} else
		pk_rmpf->wtskid = TSK_NONE;
				/* correct this code !!! */
	pk_rmpf->fblkcnt = 0;	/*********************************************/ 	
				/* make pool_free_size()  and return */
				/* the value div blksz */
	return E_OK;
}
