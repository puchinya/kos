
#include "kos_local.h"
#include <string.h>

extern void KOS_INIT_TSK(void);

#define kos_set_pend_sv()	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; __DSB();

void kos_schedule_impl_nolock(void);

int kos_find_null(void **a, int len)
{
	int i;
	for(i = 0; i < len; i++) {
		if(a[i] == NULL) return i;
	}
	return -1;
}

void kos_schedule_impl_nolock(void)
{
	int i, maxpri;
	kos_tcb_t *cur_tcb, *next_tcb;
	kos_list_t *rdy_que;
	
	/* 切り替え先タスクを探索する下限優先度を決定する */	
	cur_tcb = g_kos_cur_tcb;

	if(cur_tcb->st.tskstat == KOS_TTS_RUN) {
		/* 実行状態なら高い優先度のみ探索 */
		maxpri = cur_tcb->st.tskpri - 1;
	} else {
		/* 実行状態でなければ全優先度を探索 */
		maxpri = g_kos_max_pri;
	}
	
	/* 切り替え先タスクをRDYキューから探索 */
	next_tcb = KOS_NULL;
	rdy_que = g_kos_rdy_que;
	for(i = 0; i < maxpri; i++) {
		if(!kos_list_empty(&rdy_que[i])) {
			next_tcb = (kos_tcb_t *)rdy_que[i].next;
			break;
		}
	}
	
	/* 現在のタスクが実行中で切り替え先が無ければ何もしない */	
	if(next_tcb == KOS_NULL) {
		if(cur_tcb->st.tskstat == KOS_TTS_RUN) {
			return;
		}
	}
	
	if(cur_tcb->st.tskstat == KOS_TTS_RUN) {
		/* 現在のタスクが実行状態ならレディキューの末尾へ移動 */
		cur_tcb->st.tskstat = KOS_TTS_RDY;
		kos_list_insert_prev(&rdy_que[cur_tcb->st.tskpri-1],
			(kos_list_t *)cur_tcb);
		
		kos_dbgprintf("tsk:%04X RDY\r\n", cur_tcb->id);
	}
	
	/* 切り替え先のタスクをレディキューから削除 */
	kos_list_remove((kos_list_t *)next_tcb);
	
	/* 切り替え先のタスクを実行状態へ変更 */
	next_tcb->st.tskstat = KOS_TTS_RUN;
	
	g_kos_cur_tcb = next_tcb;
	
	kos_dbgprintf("tsk:%04X RUN\r\n", next_tcb->id);
	
	/* コンテキストスイッチ */
	/* PSPをバックアップ */
	cur_tcb->sp = (void *)__get_PSP();
	
	/* 新しいPSPに変更 */
	__set_PSP((uint32_t)next_tcb->sp);
}

void kos_rdy_tsk_nolock(kos_tcb_t *tcb)
{
	tcb->st.tskstat = KOS_TTS_RDY;
	kos_list_insert_prev(&g_kos_rdy_que[tcb->st.tskpri-1], (kos_list_t *)tcb);
	
	kos_dbgprintf("tsk:%04X RDY\r\n", tcb->id);
	
	g_kos_pend_schedule = KOS_TRUE;
}

void kos_wait_nolock(kos_tcb_t *tcb)
{
	if(g_kos_dsp) {
		tcb->rel_wai_er = KOS_E_CTX;
		return;
	}
	
	kos_dbgprintf("tsk:%04X WAI\r\n", tcb->id);
	tcb->st.tskstat = KOS_TTS_WAI;
	g_kos_pend_schedule = KOS_TRUE;
	
	/* 無限待ちでなければタイムアウトリストにもつなげる */
	if(tcb->st.lefttmo != KOS_TMO_FEVR) {
		kos_list_insert_prev(&g_kos_tmo_wait_list, &tcb->tmo_list);
	}
	
	kos_schedule_nolock();
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

void kos_schedule_nolock(void)
{
	if(g_kos_pend_schedule && !g_kos_dsp) {
		g_kos_pend_schedule = KOS_FALSE;
		kos_set_pend_sv();
	}
}

void kos_ischedule_nolock(void)
{
	if(g_kos_pend_schedule && !g_kos_dsp) {
		g_kos_pend_schedule = KOS_FALSE;
		kos_set_pend_sv();
	}
}

/*
 *	同期・通信オブジェクト削除API用の待ち解除処理です
 *	リスト中のすべてのタスクに対して下記を行います。
 *	1.待ち状態を消す(二重待ちなら強制待ち、待ち状態なら実行可能状態となる。)
 *	2.待ち解除のエラーコードをKOS_E_DLTに設定
 *	3.待ちオブジェクトをなし(=0)にする
 */
void kos_cancel_wait_all_for_delapi_nolock(kos_list_t *wait_tsk_list)
{
	const kos_list_t *l;
	
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
		}
	}
}

/*-----------------------------------------------------------------------------
	起動処理
-----------------------------------------------------------------------------*/

static uint32_t s_kos_init_stk[KOS_INIT_STKSIZE/sizeof(uint32_t)];

static const kos_ctsk_t s_ctsk_init =
{
	0,
	0,
	KOS_INIT_TSK,
	1,
	KOS_INIT_STKSIZE,
	s_kos_init_stk
};

static void kos_init_vars(void)
{
	int i;
	
	g_kos_dsp = KOS_TRUE;
	g_kos_pend_schedule = KOS_FALSE;
	
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
}

static void kos_init_tsks(void)
{
	kos_id_t tskid;
	
	tskid = kos_cre_tsk(&s_ctsk_init);
	kos_act_tsk(tskid);
}

static void kos_init_irq(void)
{
	kos_arch_setup_systick_handler();
}

__asm void kos_init_regs(void)
{
	extern g_kos_isr_stk
	extern g_kos_isr_stksz
	
	MRS R0, MSP
	MSR PSP, R0
	
	LDR R0, =g_kos_isr_stk
	LDR R1, =g_kos_isr_stksz
	LDR R1, [R1]
	
	ADD R0, R1
	MSR MSP, R0
	
	MOV	R0, #2
	MSR	CONTROL, R0
	
	BX	LR
	NOP
}

void kos_init(void)
{
	kos_lock;
	
	kos_init_vars();
	kos_init_regs();
	kos_init_tsks();
	kos_init_irq();
	
	kos_unlock;
}

void kos_start_kernel(void)
{
	/* initialize kernel */
	kos_init();
	
	/* enable dispatch */
	kos_ena_dsp();
	
	/* idle */
	for(;;) {
		//__WFI();
		__NOP();
	}
}

void kos_process_tmo(void)
{
	int do_schedule = 0;
	
	kos_list_t *l, *end, *next;
	end = &g_kos_tmo_wait_list;
	for(l = end->next; l != end; ) {
		kos_tcb_t *tcb;
		next = l->next;
		
		tcb = (kos_tcb_t *)((uint8_t *)l - offsetof(kos_tcb_t, tmo_list));
		if(--tcb->st.lefttmo == 0) {
			/* 待ち解除 */
			kos_cancel_wait_nolock(tcb, KOS_E_TMOUT);
			do_schedule = 1;
		}
		l = next;
	}
	
	if(do_schedule) {
		kos_ischedule_nolock();
	}
}

/*-----------------------------------------------------------------------------
	システム状態管理機能
-----------------------------------------------------------------------------*/

/*!
 *	@brief	get execution task ID
 */
kos_er_t kos_get_tid(kos_id_t *p_tskid)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(p_tskid == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	*p_tskid = g_kos_cur_tcb->id;
	
	return KOS_E_OK;
}

/*!
 *	@brief	get execution task ID
 */
kos_er_t kos_iget_tid(kos_id_t *p_tskid)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(p_tskid == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	*p_tskid = g_kos_cur_tcb->id;
	
	return KOS_E_OK;
}

kos_er_t kos_dis_dsp(void)
{
	g_kos_dsp = KOS_TRUE;
	
	return KOS_E_OK;
}

kos_er_t kos_ena_dsp(void)
{
	kos_lock;
	
	if(g_kos_dsp) {
		g_kos_dsp = KOS_FALSE;
		kos_schedule_nolock();
	}
	
	kos_unlock;
	
	return KOS_E_OK;
}

__asm kos_bool_t kos_sns_ctx(void)
{
	MRS	R0, IPSR
	CMP R0, #0
	MOVNE R0, #1
	BX LR
}

kos_bool_t kos_sns_dsp(void)
{
	return g_kos_dsp;
}

kos_bool_t kos_sns_dpn(void)
{
	return g_kos_dsp | kos_sns_ctx() | kos_sns_loc();
}

/*-----------------------------------------------------------------------------
	割り込み管理機能
-----------------------------------------------------------------------------*/
kos_er_t kos_def_inh(kos_intno_t intno, const kos_dinh_t *pk_dinh)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(intno > KOS_MAX_INTNO) {
		return KOS_E_PAR;
	}
	if(pk_dinh) {
		if(pk_dinh->inthdr == KOS_NULL) {
			return KOS_E_PAR;
		}
	}
#endif
	
	kos_lock;
	if(pk_dinh) {
		g_kos_dinh_list[intno] = *pk_dinh;
	} else {
		g_kos_dinh_list[intno].inthdr = KOS_NULL;
	}
	kos_unlock;
	
	return KOS_E_OK;
}
