
#include "kos_local.h"
#include <stdlib.h>
#include <string.h>

#define KOS_ENABLE_PENDSV_SCHEDULE

extern void KOS_INIT_TSK(void);

#define kos_set_pend_sv()	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; __DSB();

/*-----------------------------------------------------------------------------
	グローバル変数定義
-----------------------------------------------------------------------------*/
kos_tcb_t	*g_kos_cur_tcb;							/* execution task control block */
kos_dinh_t	g_kos_dinh_list[KOS_MAX_INTNO + 1];
kos_list_t	g_kos_rdy_que[KOS_MAX_PRI];				/* レディーキュー */
kos_tcb_t	g_kos_tcb_inst[KOS_MAX_TSK];
kos_tcb_t	*g_kos_tcb[KOS_MAX_TSK];
kos_sem_cb_t *g_kos_sem_cb[KOS_MAX_SEM];
kos_flg_cb_t *g_kos_flg_cb[KOS_MAX_FLG];
kos_cyc_cb_t *g_kos_cyc_cb[KOS_MAX_CYC];
kos_uint_t g_kos_isr_stk[KOS_ISR_STKSIZE / sizeof(kos_uint_t)];
void *g_kos_idle_sp;

const kos_uint_t g_kos_max_tsk = KOS_MAX_TSK;
const kos_uint_t g_kos_max_sem = KOS_MAX_SEM;
const kos_uint_t g_kos_max_flg = KOS_MAX_FLG;
const kos_uint_t g_kos_max_cyc = KOS_MAX_CYC;
const kos_uint_t g_kos_max_pri = KOS_MAX_PRI;
const kos_uint_t g_kos_max_intno = KOS_MAX_INTNO;
const kos_uint_t g_kos_isr_stksz = KOS_ISR_STKSIZE;

/*-----------------------------------------------------------------------------
	ファイル内変数定義
-----------------------------------------------------------------------------*/
static kos_list_t s_tmo_wait_list;			/* タイムアウト待ちのタスクのリスト */
static kos_bool_t s_dsp;					/* ディスパッチ禁止状態 */

/*-----------------------------------------------------------------------------
	ファイル内関数プロトタイプ宣言
-----------------------------------------------------------------------------*/
void kos_schedule_nolock(void);
void kos_cancel_wait_nolock(kos_tcb_t *tcb, kos_er_t er);
static void kos_swap_PSP(void **backup_sp, void *new_sp);
static void kos_switch_ctx(void **bk_sp, void *new_sp);
static void kos_iswitch_ctx(void **bk_sp, void *new_sp);
void kos_set_ctx(void *new_sp);

void *kos_malloc(kos_size_t size)
{
	return malloc(size);
}

void kos_free(void *p)
{
	free(p);
}

int kos_find_null(void **a, int len)
{
	int i;
	for(i = 0; i < len; i++) {
		if(a[i] == NULL) return i;
	}
	return -1;
}

/*-----------------------------------------------------------------------------
	インライン関数
-----------------------------------------------------------------------------*/

void kos_schedule_nolock(void)
{
	int i, maxpri;
	kos_tcb_t *cur_tcb, *next_tcb;
	kos_list_t *rdy_que;
	
	if(s_dsp) return;
	
	/* 探索する下限優先度を決める */	
	cur_tcb = g_kos_cur_tcb;
	if(cur_tcb != KOS_NULL) {
		if(cur_tcb->st.tskstat == KOS_TTS_RUN) {
			maxpri = cur_tcb->st.tskpri-1;
		} else {
			maxpri = KOS_MAX_PRI;
		}
	} else {
		maxpri = KOS_MAX_PRI;
	}
	
	/* レディキューを探索 */
	rdy_que = g_kos_rdy_que;
	next_tcb = KOS_NULL;
	for(i = 0; i < maxpri; i++) {
		if(!kos_list_empty(&rdy_que[i])) {
			next_tcb = (kos_tcb_t *)rdy_que[i].next;
			break;
		}
	}
	
	/* 現在のタスクが実行中で切り替え先が無ければ何もしない */	
	if(next_tcb == KOS_NULL) {
		if(cur_tcb == KOS_NULL || cur_tcb->st.tskstat == KOS_TTS_RUN) {
			return;
		}
	}
	
	if(cur_tcb != KOS_NULL && cur_tcb->st.tskstat == KOS_TTS_RUN) {
		/* 現在のタスクが実行状態ならレディキューの末尾へ移動 */
		cur_tcb->st.tskstat = KOS_TTS_RDY;
		kos_list_insert_prev(&rdy_que[cur_tcb->st.tskpri-1],
			(kos_list_t *)cur_tcb);
	}
	
	if(next_tcb) {
		
		/* 切り替え先のタスクをレディキューから削除 */
		kos_list_remove((kos_list_t *)next_tcb);
		
		/* 切り替え先のタスクを実行状態へ変更 */
		next_tcb->st.tskstat = KOS_TTS_RUN;
	}
	
	g_kos_cur_tcb = next_tcb;
	
	/* コンテキストスイッチ */
	{
		void **backup_sp;
		void *new_sp;
		
		backup_sp = cur_tcb ? &cur_tcb->sp : &g_kos_idle_sp;
		new_sp = next_tcb ? next_tcb->sp : g_kos_idle_sp;
		
		kos_swap_PSP(backup_sp, new_sp);
	}
}

static __asm void kos_swap_PSP(void **backup_sp, void *new_sp)
{
	/* PSPをバックアップ */
	MRS		R2, PSP
	STR		R2, [R0]
	/* 新しいPSPに変更 */
	MSR		PSP, R1
	BX		LR
}

__asm void kos_switch_ctx(void **bk_sp, void *new_sp)
{
	MRS		R2, PSR
	MOV		R3, #0
	MOVT	R3, #0x0100
	ORR		R2, R2, R3
	PUSH	{R2}
	PUSH	{LR}
	PUSH	{LR}
	PUSH	{R12}
	PUSH	{R0-R3}
	PUSH	{R4-R11}
	
	STR		SP, [R0]
	
	MOV		SP, R1
	POP		{R4-R11}
	LDR		R0, [SP, #24]
	MOV		R1, #1
	ORR		R0, R0, R1
	STR		R0, [SP, #24]
	POP		{R0-R3}
	ADD		SP, #16
	LDR		R12, [SP, #-4]
	MSR		PSR, R12
	LDR		R12, [SP, #-16]
	LDR		LR, [SP, #-12]
	CPSIE	i
	LDR		PC, [SP, #-8]
}

__asm void kos_iswitch_ctx(void **bk_sp, void *new_sp)
{
	/* PSPをバックアップ */
	MRS		R2, PSP
	STR		R2, [R0]
	/* MSP初期化 */
//	MOV		R0, #0
//	LDR		SP, [R0]
	
	/* Thread Modeへ */
//	MOV		R2, #0xFFFD
//	MOVT	R2, #0xFFFF
//	LDMIA	R1!, {R4-R11}
	MSR		PSP, R1
//	CPSIE	i
//	BX		R2
	BX		LR
}

__asm void kos_set_ctx(void *new_sp)
{
#ifdef KOS_ENABLE_PENDSV_SCHEDULE
	/* PSPを入れ替え */
	MSR	PSP, R0
	BX	LR
#else
	/* PSPを入れ替え */
	MSR	PSP, R0
	MOV	R0, #0
	LDR	R0, [R0]
	/* MSPを初期化 */
	MSR	MSP, R0
	MOV	R0, #2
	MSR	CONTROL, R0
	
	POP		{R4-R11}
	POP		{R0-R3}
	POP		{R12}
	POP		{LR}
	POP		{LR}
	POP		{R1}
	MSR		PSR, R1
	CPSIE	i
	BX		LR
#endif
}

void kos_rdy_tsk_nolock(kos_tcb_t *tcb)
{
	tcb->st.tskstat = KOS_TTS_RDY;
	kos_list_insert_prev(&g_kos_rdy_que[tcb->st.tskpri-1], (kos_list_t *)tcb);
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

void kos_schedule(void)
{
#ifdef KOS_ENABLE_PENDSV_SCHEDULE
	if(!s_dsp) {
		kos_set_pend_sv();
		kos_unlock;
		kos_lock;
	}
#else
	kos_schedule_nolock()
#endif
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
	
	g_kos_cur_tcb = KOS_NULL;
	
	memset(g_kos_tcb, 0, sizeof(g_kos_tcb));
	
	for(i = 0; i < KOS_MAX_PRI; i++) {
		kos_list_init(&g_kos_rdy_que[i]);
	}
	
	kos_list_init(&s_tmo_wait_list);
	
	kos_init_cyc();
	
	s_dsp = KOS_TRUE;
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
}

void kos_init(void)
{
	kos_lock;
	
	kos_init_regs();
	kos_init_vars();
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
		__WFI();
	}
}

kos_er_t kos_wait_nolock(kos_tcb_t *tcb)
{
	if(tcb->st.tskwait == KOS_TTS_SUS) {
		tcb->st.tskstat = KOS_TTS_WAS;
	} else {
		tcb->st.tskstat = KOS_TTS_WAI;
	}
	/* 無限待ちでなければタイムアウトリストにもつなげる */
	if(tcb->st.lefttmo != KOS_TMO_FEVR) {
		kos_list_insert_prev(&s_tmo_wait_list, &tcb->tmo_list);
	}
	
	kos_schedule();
	
	return tcb->rel_wai_er;
}

void kos_process_tmo(void)
{
	int do_schedule = 0;
	
	kos_list_t *l, *end, *next;
	end = &s_tmo_wait_list;
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
		kos_schedule();
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
	
	if(g_kos_cur_tcb == KOS_NULL) {
		*p_tskid = KOS_TSK_NONE;
	} else {
		*p_tskid = g_kos_cur_tcb->id;
	}
	
	return KOS_E_OK;
}

kos_er_t kos_loc_cpu(void)
{
	__disable_irq();
	return KOS_E_OK;
}

kos_er_t kos_iloc_cpu(void)
{
	__disable_irq();
	return KOS_E_OK;
}

kos_er_t kos_unl_cpu(void)
{
	__enable_irq();
	return KOS_E_OK;
}

kos_er_t kos_iunl_cpu(void)
{
	__enable_irq();
	return KOS_E_OK;
}

kos_er_t kos_dis_dsp(void)
{
	s_dsp = KOS_TRUE;
	
	return KOS_E_OK;
}

kos_er_t kos_ena_dsp(void)
{
	kos_lock;
	
	if(s_dsp) {
		s_dsp = KOS_FALSE;
		kos_schedule();
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
	return s_dsp;
}

kos_bool_t kos_sns_dpn(void)
{
	return s_dsp || kos_sns_ctx();
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

/*-----------------------------------------------------------------------------
	システム割り込みハンドラ
-----------------------------------------------------------------------------*/

