/*!
 *	@file	kos_vars.c
 *	@brief	global variables.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "kos_local.h"

/* zero initialized variables. */
kos_tcb_t		*g_kos_tcb[KOS_MAX_TSK];
kos_sem_cb_t	*g_kos_sem_cb[KOS_MAX_SEM];
kos_flg_cb_t	*g_kos_flg_cb[KOS_MAX_FLG];
kos_cyc_cb_t	*g_kos_cyc_cb[KOS_MAX_CYC];
kos_dinh_t		g_kos_dinh_list[KOS_MAX_INTNO + 1];

/* non initialized variables. */
#ifdef __ARMCC_VERSION
#pragma arm section zidata = ".bss_noinit"
#endif
kos_list_t		g_kos_rdy_que[KOS_MAX_PRI];		/* ready queue. */
kos_tcb_t		g_kos_tcb_inst[KOS_MAX_TSK];
kos_sem_cb_t	g_kos_sem_cb_inst[KOS_MAX_SEM];
kos_flg_cb_t	g_kos_flg_cb_inst[KOS_MAX_FLG];
kos_cyc_cb_t	g_kos_cyc_cb_inst[KOS_MAX_CYC];
kos_tcb_t		g_kos_idle_tcb_inst;

kos_uint_t		g_kos_isr_stk[KOS_ISR_STKSIZE / sizeof(kos_uint_t)];

kos_tcb_t		*g_kos_cur_tcb;					/* executing task control block. */
kos_bool_t		g_kos_dsp;						/* disabling dispatch */
kos_bool_t		g_kos_pend_schedule;			/* pending scheduler */
kos_list_t		g_kos_tmo_wait_list;			/* wating timeout. */

#ifdef __ARMCC_VERSION
#pragma arm section zidata
#endif

/* variables with an initial value. */

/* constant variables. */
const kos_uint_t	g_kos_max_tsk = KOS_MAX_TSK;
const kos_uint_t	g_kos_max_sem = KOS_MAX_SEM;
const kos_uint_t	g_kos_max_flg = KOS_MAX_FLG;
const kos_uint_t	g_kos_max_cyc = KOS_MAX_CYC;
const kos_uint_t	g_kos_max_pri = KOS_MAX_PRI;
const kos_uint_t	g_kos_max_intno = KOS_MAX_INTNO;
const kos_uint_t	g_kos_isr_stksz = KOS_ISR_STKSIZE;
