/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/config.h"
#include "../i386/proc.h"
#include "types.h"
#include "val.h"

/* forward declarations -----------------------------------------------------*/
ER_UINT sys_tcal_por(W apic, ID porid, RDVPTN calptn, VP msg, UINT cmsgsz,
		TMO tmout, RDVNO rdvno);
ER_UINT sys_tacp_por(W apic, ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg,
		UINT cmsgsz, TMO tmout);

/* por_init -----------------------------------------------------------------*/
void
por_init(void)
{
	int	i;
	for (i = 1 ;  i <= MAX_PORID ; i ++) {
		por[i].wlink_c.next = &(por[i].wlink_c);
		por[i].wlink_c.prev = &(por[i].wlink_c);
		por[i].wlink_a.next = &(por[i].wlink_a);
		por[i].wlink_a.prev = &(por[i].wlink_a);
		por[i].act = 0;
	}
}

/* sys_cre_por --------------------------------------------------------------*/
ER
sys_cre_por(W apic, ID porid, T_CPOR* pk_cpor)
{
	if (porid < 1 || porid > MAX_PORID)
		return E_ID;
	if (por[porid].act)
		return E_OBJ;
	por[porid].poratr = pk_cpor->poratr;
	por[porid].maxcmsz = pk_cpor->maxcmsz;
	por[porid].maxrmsz = pk_cpor->maxrmsz;
	por[porid].rdvno = 0;

	por[porid].act = 1;
	return E_OK;
}

/* sys_acre_por -------------------------------------------------------------*/
ER_ID
sys_acre_por(W apic, T_CPOR* pk_cpor)
{
	ER_ID	i;
	ER	ret;
	for (i = 1; i <= MAX_PORID; i++) {
		if (por[i].act == 0) {
			if ((ret = sys_cre_por(apic, i, pk_cpor)) < 0)
				return ret;
			return i;
		}
	}
	return E_NOID;
}

/* sys_del_por --------------------------------------------------------------*/
ER
sys_del_por(W apic, ID porid)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (porid < 1 || porid > MAX_PORID)
		return E_ID;
	if (por[porid].act == 0)
		return E_NOEXS;

	wlink = por[porid].wlink_c.next;
	while (wlink != &(por[porid].wlink_c)) {
		t = wlink2tsk(wlink);
		t->tskstat = TTS_RDY;
		proc_set_return_value(t->proc, E_DLT);
		sched_ins(t->tskpri, &(t->plink));
		wlink = wlink->next;
	}
	wlink = por[porid].wlink_a.next;
	while (wlink != &(por[porid].wlink_a)) {
		t = wlink2tsk(wlink);
		t->tskstat = TTS_RDY;
		proc_set_return_value(t->proc, E_DLT);
		sched_ins(t->tskpri, &(t->plink));
		wlink = wlink->next;
	}
	por[porid].wlink_c.next = &(por[porid].wlink_c);
	por[porid].wlink_c.prev = &(por[porid].wlink_c);
	por[porid].wlink_a.next = &(por[porid].wlink_a);
	por[porid].wlink_a.prev = &(por[porid].wlink_a);
	por[porid].act = 0;
	return E_OK;
}

/* sys_cal_por --------------------------------------------------------------*/
ER_UINT
sys_cal_por(W apic, ID porid, RDVPTN calptn, VP msg, UINT cmsgsz)
{
	return sys_tcal_por(apic, porid, calptn, msg, cmsgsz, TMO_FEVR, 0);
}

/* sys_tcal_por -------------------------------------------------------------*/
ER_UINT
sys_tcal_por(W apic, ID porid, RDVPTN calptn, VP msg, UINT cmsgsz,
							TMO tmout, RDVNO rdvno)
{
	T_LINK*	wlink;
	T_TSK*	t;
	W	i;
	if (porid < 1 || porid > MAX_PORID)
		return E_ID;
	if (por[porid].act == 0)
		return E_NOEXS;
	
	wlink = por[porid].wlink_a.next;
	if (wlink != &(por[porid].wlink_a)) {
		t = wlink2tsk(wlink);
		if (calptn & t->acpptn) {
			wlink_rem(wlink);	
			tsk[c_tskid[apic]].tskid = TTS_WAI;
			sched_rem(&tsk[c_tskid[apic]].plink);
			t->tskstat = TTS_RDY;
			bcopy(msg, t->por_msg, cmsgsz);
			proc_set_return_value(t->proc, cmsgsz);
			if (rdvno != 0) {
				*(t->p_rdvno) = rdvno;
			} else {
				*(t->p_rdvno) = (por[porid].rdvno << 16) |
						c_tskid[apic] & 0xffff;
			}
			por[porid].rdvno ++;
			sched_ins(t->tskpri, &tsk[c_tskid[apic]].plink);
			sched_next_tsk(apic);
			return E_OK;
		}
		wlink = wlink->next;
	}
	if (tmout == TMO_POL)
		return E_TMOUT;
	if (por[porid].poratr & TA_TFIFO) {
		ins_fifo(&(por[porid].wlink_c), &(tsk[c_tskid[apic]].plink));
	} else {	/* should be TA_FPRI */
		ins_pri(&(por[porid].wlink_c), &(tsk[c_tskid[apic]].plink));
	}
	tsk[c_tskid[apic]].calptn = calptn;
	tsk[c_tskid[apic]].por_msg = msg;
	tsk[c_tskid[apic]].por_msgsz = cmsgsz;
	tsk[c_tskid[apic]].rdvno = rdvno;
	if (tmout == TMO_FEVR)
		sys_slp_tsk(apic);
	else
		sys_tslp_tsk(apic, tmout);


	return E_OK;
}

/* sys_acp_por --------------------------------------------------------------*/
ER_UINT
sys_acp_por(W apic, ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg)
{
	return sys_tacp_por(apic, porid, acpptn, p_rdvno, msg, 0, TMO_FEVR);
}

/* sys_pacp_por -------------------------------------------------------------*/
ER_UINT
sys_pacp_por(W apic, ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg)
{
	return sys_tacp_por(apic, porid, acpptn, p_rdvno, msg, 0, TMO_POL);
}

/* sys_tacp_por -------------------------------------------------------------*/
ER_UINT
sys_tacp_por(W apic, ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg,
					UINT cmsgsz, TMO tmout)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (porid < 1 || porid > MAX_PORID)
		return E_ID;
	if (por[porid].act == 0)
		return E_NOEXS;

	wlink = por[porid].wlink_c.next;	
	if (wlink != &(por[porid].wlink_c)) {
		t = wlink2tsk(wlink);
		if (acpptn & t->calptn) {
			wlink_rem(wlink);	
			bcopy(t->por_msg, msg, cmsgsz);
			if (t->rdvno != 0)
				*p_rdvno = t->rdvno;
			else
				*p_rdvno = (por[porid].rdvno << 16) |
							c_tskid[apic] & 0xffff;
			por[porid].rdvno ++;
			return cmsgsz;
		}
		wlink = wlink->next;
	}
	if (tmout == TMO_POL)
		return E_TMOUT;
	if (por[porid].poratr & TA_TFIFO) {
		ins_fifo(&(por[porid].wlink_a), &(tsk[c_tskid[apic]].plink));
	} else {	/* should be TA_FPRI */
		ins_pri(&(por[porid].wlink_a), &(tsk[c_tskid[apic]].plink));
	}
	tsk[c_tskid[apic]].acpptn = acpptn;
	tsk[c_tskid[apic]].por_msg = msg;
	tsk[c_tskid[apic]].p_rdvno = p_rdvno;
	if (tmout == TMO_FEVR)
		sys_slp_tsk(apic);
	else
		sys_tslp_tsk(apic, tmout);
	return E_OK;
}

/* sys_fwd_por --------------------------------------------------------------*/
ER
sys_fwd_por(W apic, ID porid, RDVPTN calptn, RDVNO rdvno, VP msg, UINT cmsgsz)
{
	T_LINK*	wlink;

	return sys_tcal_por(apic, porid, calptn, msg, cmsgsz, TMO_FEVR, rdvno);
}

/* sys_rpl_rdv --------------------------------------------------------------*/
ER
sys_rpl_rdv(W apic, RDVNO rdvno, VP msg, UINT rmsgsz)
{
	ID	tskid;
/*
	if (porid < 1 || porid > MAX_PORID)
		return E_ID;
	if (por[porid].act == 0)
		return E_NOEXS;
*/

	tskid = rdvno & 0xffff;
	if (!(tsk[tskid].tskstat == TTS_WAI && tsk[tskid].rdvno == rdvno))
		return E_OBJ;

	bcopy(msg, tsk[tskid].por_msg, rmsgsz);
	proc_set_return_value(tsk[tskid].proc, rmsgsz);
	sched_ins(tsk[tskid].tskpri, &(tsk[tskid].plink));	

	return E_OK;
}

/* sys_ref_por --------------------------------------------------------------*/
ER
sys_ref_por(W apic, ID porid, T_RPOR* pk_rpor)
{
	T_LINK*	wlink;
	T_TSK*	t;
	if (porid < 1 || porid > MAX_PORID)
		return E_ID;
	if (por[porid].act == 0)
		return E_NOEXS;
	wlink = por[porid].wlink_c.next;	
	if (wlink != &(por[porid].wlink_c)) {
		t = wlink2tsk(wlink);
		pk_rpor->ctskid = t->tskid;
	} else
		pk_rpor->ctskid = TSK_NONE;

	wlink = por[porid].wlink_a.next;	
	if (wlink != &(por[porid].wlink_a)) {
		t = wlink2tsk(wlink);
		pk_rpor->atskid = t->tskid;
	} else
		pk_rpor->atskid = TSK_NONE;
	return E_OK;
}

/* sys_ref_rdv --------------------------------------------------------------*/
ER
sys_ref_rdv(W apic, RDVNO rdvno, T_RRDV* pk_rrdv)
{
	ID	tskid;
/*
	if (porid < 1 || porid > MAX_PORID)
		return E_ID;
	if (por[porid].act == 0)
		return E_NOEXS;
	
*/
	tskid = rdvno & 0xffff;
	if (!(tsk[tskid].tskstat == TTS_WAI && tsk[tskid].rdvno == rdvno))
		return E_OBJ;

	pk_rrdv->wtskid = rdvno & 0xffff;	/* (? */
	return E_OK;
}
