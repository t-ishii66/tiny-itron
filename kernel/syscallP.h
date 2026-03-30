/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYSCALLP_H
#define _SYSCALLP_H

ER sys_dummy(void);
extern void printk(char*, ...);

/* extended (non-ITRON) syscall handlers (sys_exd.c) */
ER sys_vga_write_at();
ER sys_vga_write_dec_at();
ER sys_vga_clear();
ER sys_vga_fill_at();
ER sys_key_getc_sc();
ER sys_key_set_task();
ER sys_stack_alloc_sc();

/* TCP/IP function code (network removed, kept for dispatch stub) */
#ifndef TFN_TCP_CRE_REP
#define TFN_TCP_CRE_REP		-0x201
#endif

/* syscall_entry ------------------------------------------------------------*/
struct syscall_entry {
	ER	(*func)();
};

/* syscall_entry[] ----------------------------------------------------------*/
struct syscall_entry syscall_entry[] = {
	sys_dummy,			/* 0x00 */
	sys_dummy,			/* 0x01 */
	sys_dummy,			/* 0x02 */
	sys_dummy,			/* 0x03 */
	sys_dummy,			/* 0x04 */
	sys_cre_tsk,			/* 0x05 */
	sys_del_tsk,			/* 0x06 */
	sys_act_tsk,			/* 0x07 */
	sys_can_act,			/* 0x08 */
	sys_sta_tsk,			/* 0x09 */
	(ER (*)())sys_ext_tsk,		/* 0x0a */
	(ER (*)())sys_exd_tsk,		/* 0x0b */
	sys_ter_tsk,			/* 0x0c */
	sys_chg_pri,			/* 0x0d */
	sys_get_pri,			/* 0x0e */
	sys_ref_tsk,			/* 0x0f */
	sys_ref_tst,			/* 0x10 */		
	sys_slp_tsk,			/* 0x11 */
	sys_tslp_tsk,			/* 0x12 */
	sys_wup_tsk,			/* 0x13 */
	(ER (*)())sys_can_wup,		/* 0x14 */
	sys_rel_wai,			/* 0x15 */
	sys_sus_tsk,			/* 0x16 */
	sys_rsm_tsk,			/* 0x17 */
	sys_frsm_tsk,			/* 0x18 */
	sys_dly_tsk,			/* 0x19 */
	sys_dummy,			/* 0x1a */
	sys_def_tex,			/* 0x1b */
	sys_ras_tex,			/* 0x1c */
	sys_dis_tex,			/* 0x1d */
	sys_ena_tex,			/* 0x1e */
	sys_sns_tex,			/* 0x1f */
	sys_ref_tex,			/* 0x20 */
#if 1
	sys_cre_sem,			/* 0x21 */
	sys_del_sem,			/* 0x22 */
	sys_sig_sem,			/* 0x23 */
	sys_dummy,			/* 0x24 */
	sys_wai_sem,			/* 0x25 */
	sys_pol_sem,			/* 0x26 */
	sys_twai_sem,			/* 0x27 */
	sys_ref_sem,			/* 0x28 */
	sys_cre_flg,			/* 0x29 */
	sys_del_flg,			/* 0x2a */
	sys_set_flg,			/* 0x2b */
	sys_clr_flg,			/* 0x2c */
	sys_wai_flg,			/* 0x2d */
	sys_pol_flg,			/* 0x2e */
	sys_twai_flg,			/* 0x2f */
	sys_ref_flg,			/* 0x30 */
	sys_cre_dtq,			/* 0x31 */
	sys_del_dtq,			/* 0x32 */
	sys_dummy,			/* 0x33 */
	sys_dummy,			/* 0x34 */
	sys_snd_dtq,			/* 0x35 */
	sys_psnd_dtq,			/* 0x36 */
	sys_tsnd_dtq,			/* 0x37 */
	sys_fsnd_dtq,			/* 0x38 */
	sys_rcv_dtq,			/* 0x39 */
	sys_prcv_dtq,			/* 0x3a */
	sys_trcv_dtq,			/* 0x3b */
	sys_ref_dtq,			/* 0x3c */
	sys_cre_mbx,			/* 0x3d */
	sys_del_mbx,			/* 0x3e */
	sys_snd_mbx,			/* 0x3f */
	sys_dummy,			/* 0x40 */
	sys_rcv_mbx,			/* 0x41 */
	sys_prcv_mbx,			/* 0x42 */
	sys_trcv_mbx,			/* 0x43 */
	sys_ref_mbx,			/* 0x44 */
	sys_cre_mpf,			/* 0x45 */
	sys_del_mpf,			/* 0x46 */
	sys_rel_mpf,			/* 0x47 */
	sys_dummy,			/* 0x48 */
	sys_get_mpf,			/* 0x49 */
	sys_pget_mpf,			/* 0x4a */
	sys_tget_mpf,			/* 0x4b */
	sys_ref_mpf,			/* 0x4c */
	sys_set_tim,			/* 0x4d */
	sys_get_tim,			/* 0x4e */
	sys_cre_cyc,			/* 0x4f */
	sys_del_cyc,			/* 0x50 */
	sys_sta_cyc,			/* 0x51 */
	sys_stp_cyc,			/* 0x52 */
	sys_ref_cyc,			/* 0x53 */
	sys_dummy,			/* 0x54 */
	sys_rot_rdq,			/* 0x55 */
	sys_get_tid,			/* 0x56 */
	sys_dummy,			/* 0x57 */
	sys_dummy,			/* 0x58 */
	sys_loc_cpu,			/* 0x59 */
	sys_unl_cpu,			/* 0x5a */
	sys_dis_dsp,			/* 0x5b */
	sys_ena_dsp,			/* 0x5c */
	sys_sns_ctx,			/* 0x5d */
	sys_sns_loc,			/* 0x5e */
	sys_sns_dsp,			/* 0x5f */
	sys_sns_dpn,			/* 0x60 */
	sys_ref_sys,			/* 0x61 */
	sys_dummy,			/* 0x62 */
	sys_dummy,			/* 0x63 */
	sys_dummy,			/* 0x64 */
	sys_def_inh,			/* 0x65 */
	sys_cre_isr,			/* 0x66 */
	sys_del_isr,			/* 0x67 */
	sys_ref_isr,			/* 0x68 */
	sys_dis_int,			/* 0x69 */
	sys_ena_int,			/* 0x6a */
	sys_dummy,			/* 0x6b (sys_chg_ixx) */
	sys_dummy,			/* 0x6c (sys_get_ixx) */
	sys_def_svc,			/* 0x6d */
	sys_def_exc,			/* 0x6e */
	sys_ref_cfg,			/* 0x6f */
	sys_ref_ver,			/* 0x70 */
	sys_iact_tsk,			/* 0x71 */
	iwup_tsk,			/* 0x72 */
	sys_irel_wai,			/* 0x73 */
	sys_iras_tex,			/* 0x74 */
	sys_isig_sem,			/* 0x75 */
	sys_iset_flg,			/* 0x76 */
	ipsnd_dtq,			/* 0x77 */
	sys_ifsnd_dtq,			/* 0x78 */
	sys_irot_rdq,			/* 0x79 */
	sys_iget_tid,			/* 0x7a */
	sys_iloc_cpu,			/* 0x7b */
	sys_iunl_cpu,			/* 0x7c */
	sys_isig_tim,			/* 0x7d */
	sys_dummy,			/* 0x7e */
	sys_dummy,			/* 0x7f */
	sys_dummy,			/* 0x80 */
	sys_cre_mtx,			/* 0x81 */
	sys_del_mtx,			/* 0x82 */
	sys_unl_mtx,			/* 0x83 */
	sys_dummy,			/* 0x84 */
	sys_loc_mtx,			/* 0x85 */
	sys_ploc_mtx,			/* 0x86 */
	sys_tloc_mtx,			/* 0x87 */
	sys_ref_mtx,			/* 0x88 */
	sys_cre_mbf,			/* 0x89 */
	sys_del_mbf,			/* 0x8a */
	sys_dummy,			/* 0x8b */
	sys_dummy,			/* 0x8c */
	sys_snd_mbf,			/* 0x8d */
	sys_psnd_mbf,			/* 0x8e */
	sys_tsnd_mbf,			/* 0x8f */
	sys_dummy,			/* 0x90 */
	(ER (*)())sys_rcv_mbf,		/* 0x91 */
	(ER (*)())sys_prcv_mbf,		/* 0x92 */
	(ER (*)())sys_trcv_mbf,		/* 0x93 */
	sys_ref_mbf,			/* 0x94 */
	sys_cre_por,			/* 0x95 */
	sys_del_por,			/* 0x96 */
	(ER (*)())sys_cal_por,		/* 0x97 */
	(ER (*)())sys_tcal_por,		/* 0x98 */
	(ER (*)())sys_acp_por,		/* 0x99 */
	(ER (*)())sys_pacp_por,		/* 0x9a */
	(ER (*)())sys_tacp_por,		/* 0x9b */
	sys_fwd_por,			/* 0x9c */
	sys_rpl_rdv,			/* 0x9d */
	sys_ref_por,			/* 0x9e */
	sys_ref_rdv,			/* 0x9f */
	sys_dummy,			/* 0xa0 */
	sys_cre_mpl,			/* 0xa1 */
	sys_del_mpl,			/* 0xa2 */
	sys_rel_mpl,			/* 0xa3 */
	sys_dummy,			/* 0xa4 */
	sys_get_mpl,			/* 0xa5 */
	sys_pget_mpl,			/* 0xa6 */
	sys_tget_mpl,			/* 0xa7 */
	sys_ref_mpl,			/* 0xa8 */

	sys_cre_alm,			/* 0xa9 */
	sys_del_alm,			/* 0xaa */
	sys_sta_alm,			/* 0xab */
	sys_stp_alm,			/* 0xac */
	sys_ref_alm,			/* 0xad */
	sys_dummy,			/* 0xae */
	sys_dummy,			/* 0xaf */
	sys_dummy,			/* 0xb0 */
	sys_def_ovr,			/* 0xb1 */
	sys_sta_ovr,			/* 0xb2 */
	sys_stp_ovr,			/* 0xb3 */
	sys_ref_ovr,			/* 0xb4 */
	sys_dummy,			/* 0xb5 */
	sys_dummy,			/* 0xb6 */
	sys_dummy,			/* 0xb7 */
	sys_dummy,			/* 0xb8 */
	sys_dummy,			/* 0xb9 */
	sys_dummy,			/* 0xba */
	sys_dummy,			/* 0xbb */
	sys_dummy,			/* 0xbc */
	sys_dummy,			/* 0xbd */
	sys_dummy,			/* 0xbe */
	sys_dummy,			/* 0xbf */
	sys_dummy,			/* 0xc0 */
	sys_acre_tsk,			/* 0xc1 */
	sys_acre_sem,			/* 0xc2 */
	sys_acre_flg,			/* 0xc3 */
	sys_acre_dtq,			/* 0xc4 */
	sys_acre_mbx,			/* 0xc5 */
	sys_acre_mtx,			/* 0xc6 */
	sys_acre_mbf,			/* 0xc7 */
	sys_acre_por,			/* 0xc8 */
	sys_acre_mpf,			/* 0xc9 */
	sys_acre_mpl,			/* 0xca */
	sys_acre_cyc,			/* 0xcb */
	sys_acre_alm,			/* 0xcc */
	sys_acre_isr,			/* 0xcd */
	sys_dummy,			/* 0xce */
	sys_dummy,			/* 0xcf */
	sys_dummy,			/* 0xd0 */

	sys_dummy,			/* 0xd1 */
	sys_dummy,			/* 0xd2 */
	sys_dummy,			/* 0xd3 */
	sys_dummy,			/* 0xd4 */
	sys_dummy,			/* 0xd5 */
	sys_dummy,			/* 0xd6 */
	sys_dummy,			/* 0xd7 */
	sys_dummy,			/* 0xd8 */
	sys_dummy,			/* 0xd9 */
	sys_dummy,			/* 0xda */
	sys_dummy,			/* 0xdb */
	sys_dummy,			/* 0xdc */
	sys_dummy,			/* 0xdd */
	sys_dummy,			/* 0xde */
	sys_dummy,			/* 0xdf */
	sys_dummy,			/* 0xe0 */
	/* inplement dependent service call ---------------------------------*/
	sys_printf,			/* 0xe1 */
	sys_dummy,			/* 0xe2 */
	sys_vga_write_at,		/* 0xe3 */
	sys_vga_write_dec_at,		/* 0xe4 */
	sys_vga_clear,			/* 0xe5 */
	sys_vga_fill_at,		/* 0xe6 */
	sys_key_getc_sc,		/* 0xe7 */
	sys_key_set_task,		/* 0xe8 */
	sys_stack_alloc_sc		/* 0xe9 */
#endif
};

/* syscall_tcpip_entry[] (network removed — all stubs) ---------------------*/
struct syscall_entry syscall_tcpip_entry[] = {
	sys_dummy,		/* 0x201 tcp_cre_rep */
	sys_dummy,		/* 0x202 tcp_del_rep */
	sys_dummy,		/* 0x203 tcp_cre_cep */
	sys_dummy,		/* 0x204 tcp_del_cep */
	sys_dummy,		/* 0x205 tcp_acp_cep */
	sys_dummy,		/* 0x206 tcp_con_cep */
	sys_dummy,		/* 0x207 tcp_sht_cep */
	sys_dummy,		/* 0x208 tcp_cls_cep */
	sys_dummy,		/* 0x209 tcp_snd_dat */
	sys_dummy,		/* 0x20a tcp_rcv_dat */
	sys_dummy,		/* 0x20b tcp_get_buf */
	sys_dummy,		/* 0x20c tcp_snd_buf */
	sys_dummy,		/* 0x20d tcp_rcv_buf */
	sys_dummy,		/* 0x20e tcp_rel_buf */
	sys_dummy,		/* 0x20f tcp_snd_oob */
	sys_dummy,		/* 0x210 tcp_rcv_oob */
	sys_dummy,		/* 0x211 tcp_can_cep */
	sys_dummy,		/* 0x212 tcp_set_opt */
	sys_dummy,		/* 0x213 tcp_get_opt */
	sys_dummy,		/* 0x214 */
	sys_dummy,		/* 0x215 */
	sys_dummy,		/* 0x216 */
	sys_dummy,		/* 0x217 */
	sys_dummy,		/* 0x218 */
	sys_dummy,		/* 0x219 */
	sys_dummy,		/* 0x21a */
	sys_dummy,		/* 0x21b */
	sys_dummy,		/* 0x21c */
	sys_dummy,		/* 0x21d */
	sys_dummy,		/* 0x21e */
	sys_dummy,		/* 0x21f */
	sys_dummy,		/* 0x220 */
	sys_dummy,		/* 0x221 udp_cre_cep */
	sys_dummy,		/* 0x222 udp_del_cep */
	sys_dummy,		/* 0x223 udp_snd_dat */
	sys_dummy,		/* 0x224 udp_rcv_dat */
	sys_dummy,		/* 0x225 udp_can_cep */
	sys_dummy,		/* 0x226 udp_set_opt */
	sys_dummy,		/* 0x227 udp_get_opt */
};


#endif
