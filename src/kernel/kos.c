/*!
 *	@file	kos.c
 *	@brief	implemnt scheduler and startup routine etc.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "kos_local.h"
#include <string.h>

#ifndef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
kos_int_t kos_find_null(void **a, int len)
{
	int i;
	for(i = 0; i < len; i++) {
		if(a[i] == NULL) return i;
	}
	return -1;
}
#endif

void kos_schedule_impl_nolock(void)
{
	uint32_t i, maxpri;
	kos_tcb_t *cur_tcb, *next_tcb;
	kos_list_t *rdy_que;
	
	KOS_SCHD_PORT_HI();

	/* 切り替え先タスクを探索する下限優先度を決定する */	
	cur_tcb = g_kos_cur_tcb;

	if(cur_tcb->st.tskstat == KOS_TTS_RUN) {
		/* 実行状態なら高い優先度のみを探索 */
		maxpri = cur_tcb->st.tskpri - 1;
	} else {
		/* 実行状態でなければ全優先度を探索 */
		maxpri = g_kos_max_pri;
	}
	
	/* 切り替え先タスクをRDYキューから探索 */
	next_tcb = KOS_NULL;
	rdy_que = g_kos_rdy_que;
	{
		kos_list_t *l = rdy_que;
		kos_list_t *l_end = &rdy_que[maxpri];
		while(l != l_end) {
			kos_list_t *l_next = l->next;
			if(l != l_next) {
				next_tcb = (kos_tcb_t *)l_next;
				break;
			}
			l++;
		}
	}
	
	/* 現在のタスクが実行中で切り替え先が無ければ何もしない。 */
	if(next_tcb == KOS_NULL) {
		KOS_SCHD_PORT_LO();
		return;
	}
	
#ifdef KOS_ARCH_CFG_SPT_PENDSV
	/* レディキューを編集するときは排他をかける */
	kos_ilock;
#endif

	if(cur_tcb->st.tskstat == KOS_TTS_RUN) {
		/* 現在のタスクが実行状態ならレディキューの先頭へ移動 */
		cur_tcb->st.tskstat = KOS_TTS_RDY;
		kos_list_insert_next(&rdy_que[cur_tcb->st.tskpri-1],
			(kos_list_t *)cur_tcb);
		
		kos_dbgprintf("tsk:%04X RDY\r\n", cur_tcb->id);
	}
	
	/* 切り替え先のタスクをレディキューから削除 */
	kos_list_remove((kos_list_t *)next_tcb);
	
#ifdef KOS_ARCH_CFG_SPT_PENDSV
	kos_iunlock;
#endif

	/* 切り替え先のタスクを実行状態へ変更 */
	next_tcb->st.tskstat = KOS_TTS_RUN;
	
	g_kos_cur_tcb = next_tcb;
	
	kos_dbgprintf("tsk:%04X RUN\r\n", next_tcb->id);

	KOS_SCHD_PORT_LO();
	
	/* コンテキストスイッチ */
	kos_arch_swi_sp(&cur_tcb->sp, next_tcb->sp);
}

void kos_rdy_tsk_nolock(kos_tcb_t *tcb)
{
	/* RDY状態に変更 */
	tcb->st.tskstat = KOS_TTS_RDY;

	/* RDYキューへ追加 */
	kos_list_insert_prev(&g_kos_rdy_que[tcb->st.tskpri-1], (kos_list_t *)tcb);
	
	kos_dbgprintf("tsk:%04X RDY\r\n", tcb->id);
}

void kos_wait_nolock(kos_tcb_t *tcb)
{
	/* ディスパッチ禁止状態で呼ばれた場合はエラーを返す  */
	if(g_kos_dsp) {
		tcb->rel_wai_er = KOS_E_CTX;
		return;
	}
	
	/* 無限待ちでなければタイムアウトリストにもつなげる */
	if(tcb->st.lefttmo != KOS_TMO_FEVR) {
		kos_list_insert_prev(&g_kos_tmo_wait_list, &tcb->tmo_list);
	}

	kos_dbgprintf("tsk:%04X WAI\r\n", tcb->id);

	/* WAI状態へ変更 */
	tcb->st.tskstat = KOS_TTS_WAI;

	kos_force_tsk_dsp();
}

void kos_cancel_wait_nolock(kos_tcb_t *tcb, kos_er_t er)
{
	/* エラーコードを設定 */
	tcb->rel_wai_er = er;
	
	/* タイムアウトリストから解除 */
	if(tcb->st.lefttmo != KOS_TMO_FEVR) {
		kos_list_remove(&tcb->tmo_list);
		tcb->st.lefttmo = 0;	/* 必須ではない */
	}
	/* 待ちオブジェクトから削除 */
	if(tcb->st.wobjid != 0) {
		kos_list_remove(&tcb->wait_list);
		tcb->st.wobjid = 0;	/* 必須ではない */
	}
	
	tcb->st.tskwait = 0;	/* 必須ではない */
	
	if(tcb->st.tskstat == KOS_TTS_WAS) {
		tcb->st.tskstat = KOS_TTS_SUS;
	} else {
		kos_rdy_tsk_nolock(tcb);
	}
}

/*
 *	同期・通信オブジェクト削除API用の待ち解除処理です
 *	リスト中のすべてのタスクに対して下記を行います。
 *	1.待ち状態を消す(二重待ちなら強制待ち、待ち状態なら実行可能状態となる。)
 *	2.待ち解除のエラーコードをKOS_E_DLTに設定
 *	3.待ちオブジェクトをなし(=0)にする
 */
kos_bool_t kos_cancel_wait_all_for_delapi_nolock(kos_list_t *wait_tsk_list)
{
	const kos_list_t *l;
	kos_bool_t do_tsk_dsp = KOS_FALSE;
	
	for(l = wait_tsk_list->next; l != wait_tsk_list; l = l->next)
	{
		kos_tcb_t *tcb = (kos_tcb_t *)l;
		
		/* 待ち解除のエラーコードを設定 */
		tcb->rel_wai_er = KOS_E_DLT;
		
		/* 待ちオブジェクトをなしにする */
		tcb->st.wobjid = 0;
		
		/* 待ち状態を解除 */
		if(tcb->st.tskstat == KOS_TTS_WAS) {
			tcb->st.tskstat = KOS_TTS_SUS;
		} else {
			kos_rdy_tsk_nolock(tcb);
			do_tsk_dsp = KOS_TRUE;
		}
	}

	return do_tsk_dsp;
}

void kos_process_tmo(void)
{
	kos_bool_t do_dsp;
	kos_list_t *l, *end, *next;

	do_dsp = KOS_FALSE;

	end = &g_kos_tmo_wait_list;
	for(l = end->next; l != end; ) {
		kos_tcb_t *tcb;
		next = l->next;
		
		tcb = (kos_tcb_t *)((uint8_t *)l - offsetof(kos_tcb_t, tmo_list));
		if(--tcb->st.lefttmo == 0) {
			/* 割り込みをマスクして待ち解除を行う */
			kos_ilock;
			kos_cancel_wait_nolock(tcb, KOS_E_TMOUT);
			kos_iunlock;
			do_dsp = KOS_TRUE;
		}
		l = next;
	}

	/* 1つ以上の待ちを解除したらディスパッチを行う */
	if(do_dsp) {
		kos_itsk_dsp();
	}
}

#ifdef KOS_CFG_FAST_IRQ
/*!
 *	@brief	execute delayed SVC
 *	@author	puchinya
 *	@date	2013.3.30
 */
void kos_local_process_dly_svc(void)
{
	kos_uint_t rp;
	kos_uint_t rp;

	rp = g_kos_dly_svc_fifo_rp;
	while(g_kos_dly_svc_fifo_cnt > 0) {
		kos_dly_svc_t *dly_svc;

		dly_svc = &g_kos_dly_svc_fifo[rp];
		
		dly_svc->fp(dly_svc->arg[0], dly_svc->arg[1], dly_svc->arg[2]);
		
		rp = rp + 1;
		if(rp >= g_kos_dly_svc_fifo_len) rp = 0;
		
		kos_ilock;
		g_kos_dly_svc_fifo_rp = rp;
		g_kos_dly_svc_fifo_cnt--;
		kos_iunlock;
	}
}
#endif

/*-----------------------------------------------------------------------------
	起動処理
-----------------------------------------------------------------------------*/

static void kos_init_vars(void)
{
	int i;
	
	g_kos_dsp = KOS_TRUE;
	g_kos_pend_dsp = KOS_FALSE;
	g_kos_systim = 0;
	
	kos_list_init(&g_kos_tmo_wait_list);
	
	for(i = 0; i < g_kos_max_pri; i++) {
		kos_list_init(&g_kos_rdy_que[i]);
	}
	
	kos_init_cyc();
	
	/* init idle task */
	{
		kos_tcb_t *idle_tcb = &g_kos_idle_tcb_inst;
		
		idle_tcb->id			= KOS_TSK_NONE;
		idle_tcb->st.tskstat	= KOS_TTS_RUN;
		idle_tcb->st.tskpri		= g_kos_max_pri;
		idle_tcb->st.tskwait	= 0;
		idle_tcb->st.wobjid		= 0;
		idle_tcb->st.lefttmo	= 0;
		idle_tcb->st.actcnt		= 0;
		idle_tcb->st.wupcnt		= 0;
		idle_tcb->st.suscnt		= 0;
		
		g_kos_cur_tcb = idle_tcb;
	}
	
#ifdef KOS_CFG_STKCHK
	memset(g_kos_isr_stk, 0xCC, g_kos_isr_stksz);
#endif

#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
	kos_list_init(&g_kos_tcb_unused_list);
	g_kos_last_tskid = 0;
#ifdef KOS_CFG_SPT_SEM
	kos_list_init(&g_kos_sem_cb_unused_list);
	g_kos_last_semid = 0;
#endif
#ifdef KOS_CFG_SPT_FLG
	kos_list_init(&g_kos_flg_cb_unused_list);
	g_kos_last_flgid = 0;
#endif
#ifdef KOS_CFG_SPT_DTQ
	kos_list_init(&g_kos_dtq_cb_unused_list);
	g_kos_last_dtqid = 0;
#endif
#ifdef KOS_CFG_SPT_CYC
	kos_list_init(&g_kos_cyc_cb_unused_list);
	g_kos_last_cycid = 0;
#endif
#endif
#ifdef KOS_CFG_FAST_IRQ
	g_kos_dly_svc_fifo_cnt = 0;
	g_kos_dly_svc_fifo_wp = 0;
	g_kos_dly_svc_fifo_rp = 0;
#endif
}

void kos_init_regs(void)
{
	uint32_t msp = __get_MSP();
	__set_PSP(msp);
	__set_MSP((uint32_t)g_kos_isr_stk + g_kos_isr_stksz);
	__set_CONTROL(2);
}

void kos_init(void)
{
	kos_lock;
	
	kos_init_vars();
	kos_init_regs();
	kos_usr_init();
#ifdef KOS_ARCH_CFG_SPT_SYSTICK
	kos_arch_setup_systick_handler();
#endif
	
	kos_unlock;
}

void kos_start_kernel(void)
{
	/* initialize kernel */
	kos_init();
	
	/* enable dispatch */
	kos_ena_dsp();
	
	/* idle */
	kos_arch_idle();
}
