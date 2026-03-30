/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/

#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"
#include "pool.h"
#include "sched.h"

/* forward declarations -----------------------------------------------------*/
ER sys_tsnd_mbf(W apic, ID mbfid, VP msg, UINT msgsz, TMO tmout);
ER_UINT sys_trcv_mbf(W apic, ID mbfid, VP msg, TMO tmout);
UINT mbf_rest(ID mbfid);
UINT mbf_check(ID mbfid);
void mbf_put(ID mbfid, VP msg, UINT msgsz);
ER_UINT mbf_get(ID mbfid, VP msg);
void mbf_snd_check(ID mbfid);
void mbf_do_put(ID mbfid, VP msg, UINT msgsz);
void mbf_do_get(ID mbfid, VP msg, UINT msgsz);

/* mbf_init -----------------------------------------------------------------*/
void
mbf_init(void)
{
	int	i;
	for (i = 1 ; i <= MAX_MBFID ; i ++) {
		mbf[i].wlink_s.next = &(mbf[i].wlink_s);
		mbf[i].wlink_s.prev = &(mbf[i].wlink_s);
		mbf[i].wlink_r.next = &(mbf[i].wlink_r);
		mbf[i].wlink_r.prev = &(mbf[i].wlink_r);
		mbf[i].act = 0;
	}
}

/* sys_cre_mbf --------------------------------------------------------------*/
ER
sys_cre_mbf(W apic, ID mbfid, T_CMBF* pk_cmbf)
{
	char*	p;
	if (mbfid < 1 || mbfid > MAX_MBFID)
		return E_ID;
	if (mbf[mbfid].act)
		return E_OBJ;

	mbf[mbfid].mbfatr = pk_cmbf->mbfatr;
	mbf[mbfid].maxmsz = pk_cmbf->maxmsz;
	mbf[mbfid].mbfsz = pk_cmbf->mbfsz;
	mbf[mbfid].mbf = pk_cmbf->mbf;
	
	if (mbf[mbfid].mbf == NULL) {
		mbf[mbfid].mbf = kmem_alloc(mbf[mbfid].mbfsz);
		mbf[mbfid].mbf_alloc = 1;
		mbf[mbfid].mbf_alloc_base = mbf[mbfid].mbf;
	} else
		mbf[mbfid].mbf_alloc = 0;

	p = mbf[mbfid].mbf;
	if ((unsigned long)p % 4) {
		p += 4 - ((unsigned long)p % 4);
		mbf[mbfid].mbf = p;
	}

	mbf[mbfid].mbf_end = mbf[mbfid].mbf;
	mbf[mbfid].mbf_end += mbf[mbfid].mbfsz - 1;
	p = mbf[mbfid].mbf_end;
	if ((unsigned long)p % 4) {
		p -= ((unsigned long)p % 4);
		mbf[mbfid].mbf_end = p;
	}
	/* initialize read/write pointers to start of ring buffer */
	mbf[mbfid].mbf_w = (unsigned char *)mbf[mbfid].mbf;
	mbf[mbfid].mbf_r = (unsigned char *)mbf[mbfid].mbf;
	mbf[mbfid].act = 1;
	mbf[mbfid].smsgcnt = 0;
	return E_OK;
}

/* sys_acre_mbf -------------------------------------------------------------*/
ER_ID
sys_acre_mbf(W apic, T_CMBF* pk_cmbf)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_MBFID; i++) {
		if (mbf[i].act == 0) {
			if ((ret = sys_cre_mbf(apic, i, pk_cmbf)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_mbf --------------------------------------------------------------*/
ER
sys_del_mbf(W apic, ID mbfid)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mbfid < 1 || mbfid > MAX_MBFID)
		return E_ID;
	if (mbf[mbfid].act == 0)
		return E_NOEXS;

	wlink = mbf[mbfid].wlink_s.next;
	while (wlink != &(mbf[mbfid].wlink_s)) {
		t = wlink2tsk(wlink);
		t->tskstat = TTS_RDY;
		proc_set_return_value(t->proc, E_DLT);
		sched_ins(t->tskpri, &(t->plink));
		wlink = wlink->next;
	}

	wlink = mbf[mbfid].wlink_r.next;
	while (wlink != &(mbf[mbfid].wlink_r)) {
		t = wlink2tsk(wlink);
		t->tskstat = TTS_RDY;
		proc_set_return_value(t->proc, E_DLT);
		sched_ins(t->tskpri, &(t->plink));
		wlink = wlink->next;
	}

	if (mbf[mbfid].mbf_alloc)
		kmem_free(mbf[mbfid].mbf_alloc_base);
	mbf[mbfid].wlink_s.next = &(mbf[mbfid].wlink_s);
	mbf[mbfid].wlink_s.prev = &(mbf[mbfid].wlink_s);
	mbf[mbfid].wlink_r.next = &(mbf[mbfid].wlink_r);
	mbf[mbfid].wlink_r.prev = &(mbf[mbfid].wlink_r);
	mbf[mbfid].act = 0;
	return E_OK;
}

/* sys_snd_mbf --------------------------------------------------------------*/
ER
sys_snd_mbf(W apic, ID mbfid, VP msg, UINT msgsz)
{
	return sys_tsnd_mbf(apic, mbfid, msg, msgsz, TMO_FEVR);
}

/* sys_psnd_mbf -------------------------------------------------------------*/
ER
sys_psnd_mbf(W apic, ID mbfid, VP msg, UINT msgsz)
{
	return sys_tsnd_mbf(apic, mbfid, msg, msgsz, TMO_POL);
}

/* sys_tsnd_mbf -------------------------------------------------------------*/
ER
sys_tsnd_mbf(W apic, ID mbfid, VP msg, UINT msgsz, TMO tmout)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mbfid < 1 || mbfid > MAX_MBFID)
		return E_ID;
	if (mbf[mbfid].act == 0)
		return E_NOEXS;

	/* 1) If a receiver is already waiting, deliver directly */
	wlink = mbf[mbfid].wlink_r.next;
	if (wlink != &(mbf[mbfid].wlink_r)) {
		t = wlink2tsk(wlink);
		bcopy(msg, t->msg, msgsz);
		proc_set_return_value(t->proc, msgsz);
		wlink_rem(wlink);
		sched_timeout_rem_if_exist(&(t->tlink));
		t->tskstat = TTS_RDY;
		sched_ins(t->tskpri, &(t->plink));
		return E_OK;
	}

	/* 2) No receiver — try to store in ring buffer */
	if (msgsz < mbf_rest(mbfid)) {
		mbf_put(mbfid, msg, msgsz);
		return E_OK;
	}

	/* 3) Buffer full — non-blocking send fails immediately */
	if (tmout == TMO_POL)
		return E_TMOUT;

	/* 4) Block sender until space is available */
	tsk[c_tskid[apic]].msg = msg;
	tsk[c_tskid[apic]].msgsz = msgsz;
	wlink_rem(&(tsk[c_tskid[apic]].wlink));
	if (mbf[mbfid].mbfatr & TA_TFIFO) {
		ins_fifo(&(mbf[mbfid].wlink_s), &(tsk[c_tskid[apic]].wlink));
	} else {	/* should be TA_TPRI */
		ins_pri(&(mbf[mbfid].wlink_s), &(tsk[c_tskid[apic]].wlink));
	}
	if (tmout == TMO_FEVR)
		sys_slp_tsk(apic);
	else
		sys_tslp_tsk(apic, tmout);

	return E_OK;
}

/* sys_rcv_mbf --------------------------------------------------------------*/
ER_UINT
sys_rcv_mbf(W apic, ID mbfid, VP msg)
{
	return sys_trcv_mbf(apic, mbfid, msg, TMO_FEVR);
}

/* sys_prcv_mbf -------------------------------------------------------------*/
ER_UINT
sys_prcv_mbf(W apic, ID mbfid, VP msg)
{
	return sys_trcv_mbf(apic, mbfid, msg, TMO_POL);
}

/* sys_trcv_mbf -------------------------------------------------------------*/
ER_UINT
sys_trcv_mbf(W apic, ID mbfid, VP msg, TMO tmout)
{
	T_LINK*	wlink;
	T_TSK*	t;
	ER_UINT	ret;
	if (mbfid < 1 || mbfid > MAX_MBFID)
		return E_ID;
	if (mbf[mbfid].act == 0)
		return E_NOEXS;

	if (mbf_check(mbfid)) {	/* message exists */
		ret = mbf_get(mbfid, msg);
		mbf_snd_check(mbfid);
		return ret;
	}

	if (tmout == TMO_POL)
		return E_TMOUT;

	/* save msg pointer so the sender's direct delivery path can
	 * bcopy into it (cf. DTQ saving p_data before sleeping) */
	tsk[c_tskid[apic]].msg = msg;

	/* defensive: remove from any stale wait queue left by a prior
	 * trcv_mbf that timed out (wlink stays orphaned in wlink_r after
	 * timeout).  Safe no-op if wlink is self-pointing (fresh). */
	wlink_rem(&(tsk[c_tskid[apic]].wlink));

	if (mbf[mbfid].mbfatr & TA_TFIFO) {
		ins_fifo(&(mbf[mbfid].wlink_r), &(tsk[c_tskid[apic]].wlink));
	} else {	/* should be TA_TPRI */
		ins_pri(&(mbf[mbfid].wlink_r), &(tsk[c_tskid[apic]].wlink));
	}

	if (tmout == TMO_FEVR)
		sys_slp_tsk(apic);
	else
		sys_tslp_tsk(apic, tmout);

	return E_OK;
}

/* sys_ref_mbf --------------------------------------------------------------*/
ER
sys_ref_mbf(W apic, ID mbfid, T_RMBF* pk_rmbf)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (mbfid < 1 || mbfid > MAX_MBFID)
		return E_ID;
	if (mbf[mbfid].act == 0)
		return E_NOEXS;

	wlink = mbf[mbfid].wlink_s.next;
	if (wlink != &(mbf[mbfid].wlink_s)) {
		t = wlink2tsk(wlink);
		pk_rmbf->stskid = t->tskid;
	} else
		pk_rmbf->stskid = TSK_NONE;
	wlink = mbf[mbfid].wlink_r.next;
	if (wlink != &(mbf[mbfid].wlink_r)) {
		t = wlink2tsk(wlink);
		pk_rmbf->rtskid = t->tskid;
	} else
		pk_rmbf->rtskid = TSK_NONE;

	pk_rmbf->smsgcnt = mbf[mbfid].smsgcnt;
	pk_rmbf->fmbfsz = mbf_rest(mbfid);

	return E_OK;
}

/*===========================================================================*/

void
mbf_snd_check(ID mbfid)
{
	T_LINK*	wlink;
	T_TSK*	t;

	wlink = mbf[mbfid].wlink_s.next;
	while (wlink != &(mbf[mbfid].wlink_s)) {
		t = wlink2tsk(wlink);
		if (mbf_rest(mbfid) > t->msgsz) {
			mbf_put(mbfid, t->msg, t->msgsz);	
			t->tskstat = TTS_RDY;
			proc_set_return_value(t->proc, E_OK);
			sched_ins(t->tskpri, &(t->plink));
			wlink = wlink->next;
			wlink_rem(wlink->prev);
		} else {
			break;
		}
		wlink = wlink->next;
	}
}

/* mbf_rcv_check (maybe not used) -------------------------------------------*/
void
mbf_rcv_check(ID mbfid)
{
	T_LINK*	wlink;
	T_TSK*	t;

	wlink = mbf[mbfid].wlink_r.next;
	while (wlink != &(mbf[mbfid].wlink_r)) {
		t = wlink2tsk(wlink);
		if (mbf_check(mbfid)) {
			proc_set_return_value(t->proc, mbf_get(mbfid, t->msg));
			t->tskstat = TTS_RDY;
			sched_ins(t->tskpri, &(t->plink));
			wlink = wlink->next;
			wlink_rem(wlink->prev);
		} else {
			break;
		}
		wlink = wlink->next;
	}
}

/* mbf_rest -----------------------------------------------------------------*/
UINT
mbf_rest(ID mbfid)
{
	UINT		len;

	if (mbf[mbfid].mbf_w < mbf[mbfid].mbf_r) {
		len = (UINT)(mbf[mbfid].mbf_r - mbf[mbfid].mbf_w);
	} else {
		len = (UINT)(mbf[mbfid].mbf_end - mbf[mbfid].mbf_w);
		len += (UINT)(mbf[mbfid].mbf_r -
				(unsigned char*)mbf[mbfid].mbf);
	}

	len -= sizeof(unsigned long);
	return len;
}

/* mbf_check ----------------------------------------------------------------*/
UINT
mbf_check(ID mbfid)
{
	if (mbf[mbfid].mbf_w == mbf[mbfid].mbf_r)
		return 0;
	return 1;
}

/* mbf_put ------------------------------------------------------------------*/
void
mbf_put(ID mbfid, VP msg, UINT msgsz)
{
	unsigned long	size = msgsz;
	mbf_do_put(mbfid, (char*)&size, sizeof(unsigned long));
	mbf_do_put(mbfid, msg, msgsz);
	mbf[mbfid].smsgcnt ++;
}

/* mbf_get ------------------------------------------------------------------*/
ER_UINT
mbf_get(ID mbfid, VP msg)
{
	unsigned long	size;
	mbf_do_get(mbfid, (unsigned char*)&size, sizeof(unsigned long));
	mbf_do_get(mbfid, msg, size);
	mbf[mbfid].smsgcnt --;
	return (ER_UINT)size;
}

/* mbf_do_put ---------------------------------------------------------------*/
/*         (must already check the size of message buffer by mbf_rest) ------*/
/*         (this function ignore these checkes) -----------------------------*/
void
mbf_do_put(ID mbfid, VP msg, UINT msgsz)
{
	char*	p;
	unsigned long	size = msgsz;
	unsigned long	delta;

	if (mbf[mbfid].mbf_w < mbf[mbfid].mbf_r) {
		bcopy((unsigned char*)msg, mbf[mbfid].mbf_w, msgsz);
		mbf[mbfid].mbf_w += msgsz;
	} else {
		if (mbf[mbfid].mbf_w + msgsz - 1 <= mbf[mbfid].mbf_end) {
			bcopy((unsigned char*)msg, mbf[mbfid].mbf_w, msgsz);
			mbf[mbfid].mbf_w += msgsz;
		} else {
			delta = (mbf[mbfid].mbf_end - mbf[mbfid].mbf_w) + 1;
			bcopy((unsigned char*)msg, mbf[mbfid].mbf_w, delta);
			msg += delta;
			mbf[mbfid].mbf_w = mbf[mbfid].mbf;
			bcopy((unsigned char*)msg, mbf[mbfid].mbf_w,
								msgsz - delta);
		
			mbf[mbfid].mbf_w += msgsz - delta;
		}
	}

	if (mbf[mbfid].mbf_w > mbf[mbfid].mbf_end) {
		mbf[mbfid].mbf_w = mbf[mbfid].mbf;
	}
}

/* mbf_do_get ---------------------------------------------------------------*/
/*         (must already check the size of message buffer by mbf_rest) ------*/
/*         (this function ignore these checkes) -----------------------------*/
void
mbf_do_get(ID mbfid, VP msg, UINT msgsz)
{
	char*	p;
	unsigned long	size = msgsz;
	unsigned long	delta;

	if (mbf[mbfid].mbf_r < mbf[mbfid].mbf_w) {
		bcopy(mbf[mbfid].mbf_r, (unsigned char*)msg, msgsz);
		mbf[mbfid].mbf_r += msgsz;
	} else {
		if (mbf[mbfid].mbf_r + msgsz - 1 <= mbf[mbfid].mbf_end) {
			bcopy(mbf[mbfid].mbf_r, (unsigned char*)msg, msgsz);
			mbf[mbfid].mbf_r += msgsz;
		} else {
			delta = (mbf[mbfid].mbf_end - mbf[mbfid].mbf_r) + 1;
			bcopy(mbf[mbfid].mbf_r, (unsigned char*)msg, delta);
			msg += delta;
			mbf[mbfid].mbf_r = mbf[mbfid].mbf;
			bcopy(mbf[mbfid].mbf_r, (unsigned char*)msg,
								msgsz - delta);
		
			mbf[mbfid].mbf_r += msgsz - delta;
		}
	}
	if (mbf[mbfid].mbf_r > mbf[mbfid].mbf_end) {
		mbf[mbfid].mbf_r = mbf[mbfid].mbf;
	}
}
