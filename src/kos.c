
#include "kos_local.h"
#include <stdlib.h>
#include <string.h>

#define KOS_ENABLE_PENDSV_SCHEDULE

extern void KOS_INIT_TSK(void);

#define kos_set_pend_sv()	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;

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

const kos_uint_t g_kos_max_tsk = KOS_MAX_TSK;
const kos_uint_t g_kos_max_sem = KOS_MAX_SEM;
const kos_uint_t g_kos_max_flg = KOS_MAX_FLG;
const kos_uint_t g_kos_max_cyc = KOS_MAX_CYC;
const kos_uint_t g_kos_max_pri = KOS_MAX_PRI;
const kos_uint_t g_kos_max_intno = KOS_MAX_INTNO;

/*-----------------------------------------------------------------------------
	ファイル内変数定義
-----------------------------------------------------------------------------*/
static kos_systim_t s_systim;				/* システム時刻 */
static kos_list_t s_tmo_wait_list;			/* タイムアウト待ちのタスクのリスト */
static kos_list_t s_cyc_list;				/* アクティブな周期ハンドラのリスト */
static kos_bool_t s_dsp;					/* ディスパッチ禁止状態 */

/*-----------------------------------------------------------------------------
	ファイル内関数プロトタイプ宣言
-----------------------------------------------------------------------------*/
void kos_schedule_nolock(void);
void kos_cancel_wait_nolock(kos_tcb_t *tcb, kos_er_t er);
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
static KOS_INLINE kos_tcb_t *kos_get_tcb(kos_id_t tskid)
{
	return g_kos_tcb[tskid-1];
}

static KOS_INLINE kos_flg_cb_t *kos_get_flg_cb(kos_id_t flgid)
{
	return g_kos_flg_cb[flgid-1];
}

static KOS_INLINE kos_cyc_cb_t *kos_get_cyc_cb(kos_id_t cycid)
{
	return g_kos_cyc_cb[cycid-1];
}

void kos_schedule_nolock(void)
{
	int i, maxpri;
	kos_tcb_t *cur_tcb, *next_tcb;
	kos_list_t *rdy_que;
	
	if(s_dsp) return;
	
	/* 探索する上限優先度を決める */	
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
	next_tcb = NULL;
	for(i = 0; i < maxpri; i++) {
		if(!kos_list_empty(&rdy_que[i])) {
			next_tcb = (kos_tcb_t *)rdy_que[i].next;
			break;
		}
	}
	
	/* 無ければ何もしない */	
	if(!next_tcb) return;
	
	if(cur_tcb != KOS_NULL && cur_tcb->st.tskstat == KOS_TTS_RUN) {
		/* 以前のタスクが実行状態ならレディキューへ移動 */
		cur_tcb->st.tskstat = KOS_TTS_RDY;
		kos_list_insert_prev(&rdy_que[cur_tcb->st.tskpri-1],
			(kos_list_t *)cur_tcb);
	}
	
	/* レディキューから削除 */
	kos_list_remove((kos_list_t *)next_tcb);
	/* 実行状態へ変更 */
	next_tcb->st.tskstat = KOS_TTS_RUN;
	g_kos_cur_tcb = next_tcb;
	
	/* コンテキストスイッチ */
	if(cur_tcb != KOS_NULL) {
#ifdef KOS_ENABLE_PENDSV_SCHEDULE
		kos_iswitch_ctx(&cur_tcb->sp, next_tcb->sp);
#else
		int cur_is_irq = kos_sns_ctx();
		if(cur_is_irq) {
			kos_iswitch_ctx(&cur_tcb->sp, next_tcb->sp);
		} else {
			kos_switch_ctx(&cur_tcb->sp, next_tcb->sp);
		}
#endif
	} else {
		kos_set_ctx(next_tcb->sp);
	}
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

static void kos_tsk_entry(kos_tcb_t *tcb)
{
	for(;;) {
		((void (*)(kos_vp_int_t))tcb->ctsk.task)(tcb->ctsk.exinf);
		kos_lock;
		tcb->st.tskstat = KOS_TTS_DMT;
		kos_schedule();
		kos_unlock;
	}
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

static void kos_rdy_tsk_nolock(kos_tcb_t *tcb)
{
	tcb->st.tskstat = KOS_TTS_RDY;
	kos_list_insert_prev(&g_kos_rdy_que[tcb->st.tskpri-1], (kos_list_t *)tcb);
}

/*-----------------------------------------------------------------------------
	タスク管理機能
-----------------------------------------------------------------------------*/
kos_er_id_t kos_cre_tsk(const kos_ctsk_t *ctsk)
{
	kos_tcb_t *tcb;
	int empty_index;
	uint32_t *sp;
	kos_er_id_t er_id;
	
	kos_lock;
	
	empty_index = kos_find_null((void **)g_kos_tcb, g_kos_max_tsk);
	if(empty_index < 0) {
		er_id = KOS_E_NOID;
		goto end;
	}
	
	tcb = &g_kos_tcb_inst[empty_index];
	g_kos_tcb[empty_index] = tcb;
	
	tcb->ctsk			= *ctsk;
	tcb->st.tskstat		= KOS_TTS_DMT;
	tcb->st.tskpri		= ctsk->itskpri;
	tcb->st.tskbpri		= ctsk->itskpri;
	tcb->st.tskwait		= 0;
	tcb->st.wobjid		= 0;
	tcb->st.lefttmo		= 0;
	tcb->st.actcnt		= 0;
	tcb->st.wupcnt		= 0;
	tcb->st.suscnt		= 0;
	tcb->id = empty_index + 1;
#ifdef KOS_CFG_STKCHK
	memset(tcb->ctsk.stk, 0xCC, tcb->ctsk.stksz);
#endif
	tcb->sp = sp = (uint32_t *)((uint8_t *)tcb->ctsk.stk + tcb->ctsk.stksz - 4 - 16*4);
	sp[0] = 0;								// R4
	sp[1] = 0;								// R5
	sp[2] = 0;								// R6
	sp[3] = 0;								// R7
	sp[4] = 0;								// R8
	sp[5] = 0;								// R9
	sp[6] = 0;								// R10
	sp[7] = 0;								// R11
	sp[8] = (uint32_t)tcb;					// R0
	sp[9] = 0;								// R1
	sp[10] = 0;								// R2
	sp[11] = 0;								// R3
	sp[12] = 0;								// R12
	sp[13] = (uint32_t)kos_tsk_entry;		// LR
	sp[14] = (uint32_t)kos_tsk_entry;		// PC
	sp[15] = 1<<24;							// PSR
	
	kos_unlock;
end:
	er_id = empty_index + 1;
	
	return er_id;
}

kos_er_t kos_act_tsk(kos_id_t tskid)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	if(tcb->st.tskstat == KOS_TTS_DMT) {
		kos_rdy_tsk_nolock(tcb);
		kos_schedule();
	} else {
		if(tcb->st.actcnt >= KOS_MAX_ACT_CNT) {
			er = KOS_E_QOVR;
		} else {
			tcb->st.actcnt++;
		}
	}
	
end:
	kos_unlock;
	
	return er;
}


kos_er_uint_t kos_can_act(kos_id_t tskid)
{
	kos_er_uint_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return (kos_er_uint_t)KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;

	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = (kos_er_uint_t)KOS_E_NOEXS;
			goto end;
		}
	}
	
	er = (kos_er_uint_t)tcb->st.actcnt;
	tcb->st.actcnt = 0;
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_chg_pri(kos_id_t tskid, kos_pri_t tskpri)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
	if(tskpri > g_kos_max_pri)
		return KOS_E_PAR;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = KOS_E_NOEXS;
			goto end;
		}
		if(tcb->st.tskstat == KOS_TTS_DMT) {
			/* 休止状態ならオブジェクト状態エラーとする */
			er = KOS_E_ILUSE;
			goto end;
		}
	}
	
	if(tskpri == KOS_TPRI_INI) {
		tskpri = tcb->st.tskbpri;
	}
	
	tcb->st.tskpri = tskpri;
	
	kos_schedule();
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_get_pri(kos_id_t tskid, kos_pri_t *p_tskpri)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
	if(p_tskpri == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = KOS_E_NOEXS;
			goto end;
		}
		if(tcb->st.tskstat == KOS_TTS_DMT) {
			/* 休止状態ならオブジェクト状態エラーとする */
			er = KOS_E_ILUSE;
			goto end;
		}
	}
	
	*p_tskpri = tcb->st.tskpri;
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_rel_wai(kos_id_t tskid)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	if(!tcb->st.tskstat & KOS_TTS_WAI) {
		er = KOS_E_OBJ;
		goto end;
	}
	
	kos_cancel_wait_nolock(tcb, KOS_E_RLWAI);
	kos_schedule();
	
end:
	kos_unlock;
	
	return er;
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
	if(!s_dsp) { kos_set_pend_sv(); }
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
	同期・通信機能/イベントフラグ
-----------------------------------------------------------------------------*/
#ifdef KOS_CFG_SPT_FLG
kos_er_id_t kos_cre_flg(const kos_cflg_t *pk_cflg)
{
	kos_flg_cb_t *cb;
	kos_cflg_t *cflg_bk;
	int empty_index;
	kos_er_id_t er_id;
	
	kos_lock;
	
	cb = (kos_flg_cb_t *)kos_malloc(sizeof(kos_flg_cb_t) + sizeof(kos_cflg_t));
	if(!cb) {
		er_id = KOS_E_NOMEM;
		goto end;
	}
	empty_index = kos_find_null((void **)g_kos_flg_cb, KOS_MAX_FLG);
	if(empty_index < 0) {
		er_id = KOS_E_NOID;
		kos_free(cb);
		goto end;
	}
	
	g_kos_flg_cb[empty_index] = cb;
	
	cflg_bk = (kos_cflg_t *)((uint8_t *)cb + sizeof(kos_flg_cb_t));
	*cflg_bk = *pk_cflg;
	cb->cflg = cflg_bk;
	
	cb->flgptn = cflg_bk->iflgptn;
	kos_list_init(&cb->wait_tsk_list);
	
	kos_unlock;
end:
	er_id = empty_index + 1;
	
	return er_id;
}
kos_er_t kos_del_flg(kos_id_t flgid)
{
	kos_flg_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(flgid > g_kos_max_flg || flgid == 0)
		return KOS_E_ID;
#endif
	er = KOS_E_OK;
	kos_lock;
	cb = kos_get_flg_cb(flgid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	/* 待ち行列にいるタスクの待ちを解除 */
	kos_cancel_wait_all_for_delapi_nolock(&cb->wait_tsk_list);
	/* コントロールブロックのメモリを開放 */
	kos_free(cb);
	/* ID=>CB変換をクリア */
	g_kos_sem_cb[flgid-1] = KOS_NULL;
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_set_flg(kos_id_t flgid, kos_flgptn_t setptn)
{
	kos_flg_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(flgid > g_kos_max_flg || flgid == 0)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_flg_cb(flgid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	cb->flgptn |= setptn;
	
	if(!kos_list_empty(&cb->wait_tsk_list)) {
#ifdef KOS_CFG_SPT_FLG_WMUL
		kos_list_t *wait_tsk_list = &cb->wait_tsk_list;
		kos_list_t *l;
		int do_schedule = 0;
		
		for(l = wait_tsk_list->next; l != wait_tsk_list; l = l->next)
		{
			kos_tcb_t *tcb = (kos_tcb_t *)l;
			
			if(tcb->wait_info.flg.wfmode == KOS_TWF_ANDW) {
				if((tcb->wait_info.flg.waiptn & cb->flgptn) != tcb->wait_info.flg.waiptn)
					continue;
			} else {
				if(!(tcb->wait_info.flg.waiptn & cb->flgptn))
					continue;
			}
			do_schedule = 1;
			*tcb->wait_info.flg.relptn = cb->flgptn;
			kos_cancel_wait_nolock(tcb, KOS_E_OK);
			if(cb->cflg->flgatr & KOS_TA_CLR) {
				/* タスクをひとつ解除した時点でクリアする(仕様) */
				cb->flgptn = 0;
				break;
			}
			if(!(cb->cflg->flgatr & KOS_TA_WMUL)) {
				break;
			}
		}
		
		if(do_schedule) {
			kos_schedule();
		}
#else
		if(cb->wfmode == KOS_TWF_ANDW) {
			if((cb->waiptn & cb->flgptn) != cb->waiptn)
				goto end;
		} else {
			if(!(cb->waiptn & cb->flgptn))
				goto end;
		}
		*cb->relptn = cb->flgptn;
		if(cb->cflg->flgatr & KOS_TA_CLR) {
			/* タスクをひとつ解除した時点でクリアする(仕様) */
			cb->flgptn = 0;
		}
		kos_cancel_wait_nolock((kos_tcb_t *)cb->wait_tsk_list.next, KOS_E_OK);
		kos_schedule();
#endif
	}

end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_clr_flg(kos_id_t flgid, kos_flgptn_t clrptn)
{
	kos_flg_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(flgid > g_kos_max_flg || flgid == 0)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_flg_cb(flgid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	cb->flgptn &= clrptn;
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_twai_flg(kos_id_t flgid, kos_flgptn_t waiptn,
	kos_mode_t wfmode, kos_flgptn_t *p_flgptn, kos_tmo_t tmout)
{
	kos_flg_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(flgid > g_kos_max_flg || flgid == 0)
		return KOS_E_ID;
	if(waiptn == 0 || (wfmode != KOS_TWF_ANDW && wfmode != KOS_TWF_ORW) ||
		p_flgptn == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_flg_cb(flgid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
#ifdef KOS_CFG_SPT_FLG_WMUL
	if(!(cb->cflg->flgatr & KOS_TA_WMUL) &&
		!kos_list_empty(&cb->wait_tsk_list))
	{
		er = KOS_E_ILUSE;
		goto end;
	}
#else
	if(!kos_list_empty(&cb->wait_tsk_list)) {
		er = KOS_E_ILUSE;
		goto end;
	}
#endif
	
	if(wfmode == KOS_TWF_ANDW) {
		if(cb->flgptn & waiptn) {
			*p_flgptn = cb->flgptn;
			goto end;
		}
	} else {
		if((cb->flgptn & waiptn) == waiptn) {
			*p_flgptn = cb->flgptn;
			goto end;
		}
	}
	
	if(tmout == KOS_TMO_POL) {
		er = KOS_E_TMOUT;
	} else {
		kos_tcb_t *tcb = g_kos_cur_tcb;
		tcb->st.lefttmo = tmout;
		tcb->st.wobjid = flgid;
		tcb->st.tskwait = KOS_TTW_FLG;
		kos_list_insert_prev(&cb->wait_tsk_list, &tcb->wait_list);
#ifdef KOS_CFG_SPT_FLG_WMUL
		tcb->wait_info.flg.wfmode = wfmode;
		tcb->wait_info.flg.waiptn = waiptn;
		tcb->wait_info.flg.relptn = p_flgptn;
#else
		cb->wfmode = wfmode;
		cb->waiptn = waiptn;
		cb->relptn = p_flgptn;
#endif
		er = kos_wait_nolock(tcb);
	}
end:
	kos_unlock;
	
	return er;
}


kos_er_t kos_ref_flg(kos_id_t flgid, kos_rflg_t *pk_rflg)
{
	kos_flg_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(flgid > g_kos_max_flg || flgid == 0)
		return KOS_E_ID;
	if(pk_rflg == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_flg_cb(flgid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	if(kos_list_empty(&cb->wait_tsk_list)) {
		pk_rflg->wtskid = KOS_TSK_NONE;
	} else {
		kos_tcb_t *tcb = (kos_tcb_t *)cb->wait_tsk_list.next;
		pk_rflg->wtskid = tcb->id;
	}
	pk_rflg->flgptn = cb->flgptn;
	
end:
	kos_unlock;
	
	return er;
}
#endif
/*-----------------------------------------------------------------------------
	起動処理
-----------------------------------------------------------------------------*/
static void kos_idle(void *exinf)
{
	for(;;) {
		__WFI;
	}
}

static uint32_t s_kos_idle_stk[KOS_IDLE_STKSIZE/sizeof(uint32_t)];

static const kos_ctsk_t s_ctsk_idle =
{
	0,
	0,
#ifdef KOS_IDLE_TASK
	KOS_IDLE_TASK,
#else
	kos_idle,
#endif
	KOS_MAX_PRI,
	KOS_IDLE_STKSIZE,
	s_kos_idle_stk
};

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
	memset(g_kos_cyc_cb, 0, sizeof(g_kos_cyc_cb));
	
	for(i = 0; i < KOS_MAX_PRI; i++) {
		kos_list_init(&g_kos_rdy_que[i]);
	}
	
	kos_list_init(&s_tmo_wait_list);
	kos_list_init(&s_cyc_list);
	
	s_dsp = KOS_TRUE;
}

static void kos_init_tsks(void)
{
	kos_id_t tskid;
	tskid = kos_cre_tsk(&s_ctsk_idle);
	kos_act_tsk(tskid);
	tskid = kos_cre_tsk(&s_ctsk_init);
	kos_act_tsk(tskid);
}

static void kos_init_irq(void)
{
	kos_arch_setup_systick_handler();
}

__asm void kos_init_regs(void)
{
	MRS R0, MSP
	MSR PSP, R0
	MOV	R0, #2
	MSR	CONTROL, R0
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
	kos_init();
	kos_ena_dsp();
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
	kos_unlock;
	kos_lock;
	
	return tcb->rel_wai_er;
}

kos_er_t kos_wup_tsk(kos_id_t tskid)
{
	kos_tcb_t *tcb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(tcb == KOS_NULL) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	if((tcb->st.tskstat & KOS_TTS_WAI) &&
		tcb->st.tskwait == KOS_TTW_SLP)
	{
		kos_cancel_wait_nolock(tcb, KOS_E_OK);
		kos_schedule();
	} else if(tcb->st.tskstat == KOS_TTS_DMT) {
		er = KOS_E_OBJ;
		goto end;
	} else {
		if(tcb->st.wupcnt >= KOS_MAX_WUP_CNT) {
			er = KOS_E_QOVR;
			goto end;
		}
		tcb->st.wupcnt++;
	}
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_iwup_tsk(kos_id_t tskid)
{
	kos_tcb_t *tcb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(tcb == KOS_NULL) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	if((tcb->st.tskstat & KOS_TTS_WAI) &&
		tcb->st.tskwait == KOS_TTW_SLP)
	{
		kos_cancel_wait_nolock(tcb, KOS_E_OK);
		kos_schedule();
	} else if(tcb->st.tskstat == KOS_TTS_DMT) {
		er = KOS_E_OBJ;
		goto end;
	} else {
		if(tcb->st.wupcnt >= KOS_MAX_WUP_CNT) {
			er = KOS_E_QOVR;
			goto end;
		}
		tcb->st.wupcnt++;
	}
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_tslp_tsk(kos_tmo_t tmout)
{
	kos_tcb_t *tcb;
	kos_er_t er;
	
	kos_lock;
	
	tcb = g_kos_cur_tcb;
	
	if(tcb->st.wupcnt > 0) {
		tcb->st.wupcnt--;
		er = KOS_E_OK;
	} else {
		if(tmout == KOS_TMO_POL) {
			er = KOS_E_TMOUT;
		} else {
			tcb->st.lefttmo = tmout;
			tcb->st.wobjid = 0;
			tcb->st.tskwait = KOS_TTW_SLP;
			er = kos_wait_nolock(tcb);
		}
	}
	
	kos_unlock;
	
	return er;
}

kos_er_t kos_dly_tsk(kos_reltim_t dlytim)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
	if(dlytim == 0) return KOS_E_OK;
	
	kos_lock;
	
	tcb = g_kos_cur_tcb;
	
	tcb->st.lefttmo = dlytim;
	tcb->st.wobjid = 0;
	tcb->st.tskwait = KOS_TTW_DLY;
	er = kos_wait_nolock(tcb);
	
	kos_unlock;
	
	return er;
}

/*-----------------------------------------------------------------------------
	時間管理機能/システム時刻管理
-----------------------------------------------------------------------------*/

kos_er_t kos_set_tim(kos_systim_t *p_systim)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(p_systim == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	kos_lock;
	s_systim = *p_systim;
	kos_unlock;
	
	return KOS_E_OK;
}

kos_er_t kos_get_tim(kos_systim_t *p_systim)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(p_systim == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	kos_lock;
	*p_systim = s_systim;
	kos_unlock;
	
	return KOS_E_OK;
}

kos_er_t kos_isig_tim(void)
{
	int do_schedule = 0;
	
	kos_lock;
	
	/* システム時刻をインクリメント */
	s_systim++;
	
	/* タイムアウト待ちのタスクを処理 */
	{
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
	
	/* 周期ハンドラの処理 */
	{
		kos_cyc_cb_t *l, *end, *next;
		end = (kos_cyc_cb_t *)&s_cyc_list;
		for(l = (kos_cyc_cb_t *)end->list.next; l != end; ) {
			next = (kos_cyc_cb_t *)l->list.next;
			if(--l->st.lefttim == 0) {
				/* 実行する */
				((void (*)(kos_vp_int_t))l->ccyc->cychdr)(l->ccyc->exinf);
				l->st.lefttim = l->ccyc->cyctim;
			}
			l = next;
		}
	}
	
	//kos_set_pend_sv();
	
	kos_unlock;
	
	return KOS_E_OK;
}

/*-----------------------------------------------------------------------------
	時間管理機能/周期ハンドラ
-----------------------------------------------------------------------------*/
static void kos_sta_cyc_nolock(kos_cyc_cb_t *cb)
{
	cb->st.lefttim = cb->ccyc->cyctim;
	cb->st.cycstat = KOS_TCYC_STA;
	
	kos_list_insert_prev(&s_cyc_list, &cb->list);
}

kos_er_t kos_cre_cyc(const kos_ccyc_t *pk_ccyc)
{
	kos_cyc_cb_t *cb;
	kos_ccyc_t *ccyc_bk;
	int empty_index;
	kos_er_id_t er_id;
	kos_atr_t atr;
	
	kos_lock;
	
	cb = (kos_cyc_cb_t *)kos_malloc(sizeof(kos_cyc_cb_t) + sizeof(kos_ccyc_t));
	if(!cb) {
		er_id = KOS_E_NOMEM;
		goto end;
	}
	empty_index = kos_find_null((void **)g_kos_cyc_cb, KOS_MAX_CYC);
	if(empty_index < 0) {
		er_id = KOS_E_NOID;
		kos_free(cb);
		goto end;
	}
	g_kos_cyc_cb[empty_index] = cb;
	
	ccyc_bk = (kos_ccyc_t *)((uint8_t *)cb + sizeof(kos_cyc_cb_t));
	*ccyc_bk = *pk_ccyc;
	cb->ccyc = ccyc_bk;
	
	cb->st.cycstat = KOS_TCYC_STP;
	cb->st.lefttim = 0; 		/* 必須ではない */
	
	atr = pk_ccyc->cycatr;
	
	if(atr & KOS_TA_STA) {
		kos_sta_cyc_nolock(cb);
	}
	
	kos_unlock;
end:
	er_id = empty_index + 1;
	
	return er_id;
}


kos_er_t kos_del_cyc(kos_id_t cycid)
{
	kos_cyc_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(cycid > g_kos_max_cyc || cycid == 0)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	cb = kos_get_cyc_cb(cycid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	/* 開始状態ならリストから除去 */
	if(cb->st.cycstat == KOS_TCYC_STA) {
		kos_list_remove(&cb->list);
	}
	
	/* コントロールブロックのメモリを開放 */
	kos_free(cb);
	
	/* ID=>CB変換をクリア */
	g_kos_cyc_cb[cycid-1] = KOS_NULL;
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_sta_cyc(kos_id_t cycid)
{
	kos_cyc_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(cycid > g_kos_max_cyc || cycid == 0)
		return KOS_E_ID;
#endif
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_cyc_cb(cycid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	if(cb->st.cycstat == KOS_TCYC_STP) {
		kos_sta_cyc_nolock(cb);
	}
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_stp_cyc(kos_id_t cycid)
{
	kos_cyc_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(cycid > g_kos_max_cyc || cycid == 0)
		return KOS_E_ID;
#endif
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_cyc_cb(cycid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	if(cb->st.cycstat == KOS_TCYC_STA) {
		cb->st.cycstat = KOS_TCYC_STP;
		cb->st.lefttim = 0; 		/* 必須ではない */
	}
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_ref_cyc(kos_id_t cycid, kos_rcyc_t *pk_rcyc)
{
	kos_cyc_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(cycid > g_kos_max_cyc || cycid == 0)
		return KOS_E_ID;
#endif
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_cyc_cb(cycid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	*pk_rcyc = cb->st;
	
end:
	kos_unlock;
	
	return er;
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
	
	//kos_lock;
	*p_tskid = g_kos_cur_tcb->id;
	//kos_unlock;
	
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
	
	//kos_lock;
	*p_tskid = g_kos_cur_tcb->id;
	//kos_unlock;
	
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
	//kos_lock;
	
	if(s_dsp) {
		s_dsp = KOS_FALSE;
		kos_schedule();
	}
	
	//kos_unlock;
	
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

/* do schedule */
__asm void PendSV_Handler(void)
{
	extern kos_schedule_nolock
	
	CPSID		i
	MRS			R0, PSP
	STMDB		R0!, {R4-R11}
	MSR			PSP, R0
	LDR			R0, =kos_schedule_nolock
	MOV			R4, LR
	BLX			R0
	MOV			LR, R4
	MRS			R0, PSP
	LDMIA		R0!, {R4-R11}
	MSR			PSP, R0
	CPSIE		i
	BX			LR
}
