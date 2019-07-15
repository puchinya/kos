/*!
 *	@file	kos_mtx.c
 *	@brief	mutex API's implement.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "kos_local.h"

#ifdef KOS_CFG_SPT_MTX

#define CB_TO_INDEX(cb)			(((uintptr_t)cb - (uintptr_t)g_kos_mtx_cb_inst) / sizeof(kos_mtx_cb_t))
#define kos_get_mtx_cb(mtxid)	(g_kos_mtx_cb[(mtxid) - 1])

kos_er_id_t kos_cre_mtx(const kos_cmtx_t *pk_cmtx)
{
	kos_mtx_cb_t *cb;
	kos_int_t empty_index;
	kos_er_id_t er_id;

	kos_lock;

	{
		kos_id_t last_id = g_kos_last_mtxid;

		if(last_id < g_kos_max_mtx) {
			empty_index = last_id;
			g_kos_last_mtxid = last_id + 1;
		} else {
			if(kos_list_empty(&g_kos_mtx_cb_unused_list)) {
				er_id = KOS_E_NOID;
				goto end;
			} else {
				kos_mtx_cb_t *unused_cb = (kos_mtx_cb_t *)g_kos_sem_cb_unused_list.next;
				empty_index = CB_TO_INDEX(unused_cb);
				kos_list_remove((kos_list_t *)unused_cb);
			}
		}
	}

	cb = &g_kos_mtx_cb_inst[empty_index];
	g_kos_mtx_cb[empty_index] = cb;

	cb->cmtx = *pk_cmtx;
	cb->htsk_tcb = KOS_NULL;
	cb->loc_cnt = 0;
	cb->htsk_list.next = KOS_NULL;
	cb->htsk_list.prev = KOS_NULL;
	kos_list_init(&cb->wait_tsk_list);

	er_id = empty_index + 1;
end:
	kos_unlock;

	return er_id;
}

kos_er_t kos_del_mtx(kos_id_t mtxid)
{
	kos_mtx_cb_t *cb;
	kos_er_t er;
	kos_bool_t do_tsk_dsp;

	/* IDの値が有効範囲であることをチェック */
#ifdef KOS_CFG_ENA_PAR_CHK
	if(mtxid > g_kos_max_sem || mtxid == 0)
		return KOS_E_ID;
#endif

	er = KOS_E_OK;

	kos_lock;

	cb = kos_get_mtx_cb(mtxid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}

	/* ロックを獲得してるタスクがいれば、解放する */
	{
		kos_tcb_t *htsk_tcb;
		htsk_tcb = cb->htsk_tcb;
		if(htsk_tcb != NULL) {
			/* タスクの獲得済みミューテックス一覧から削除 */
			kos_list_remove(&cb->htsk_list);
			/* タスクがミューテックスのロックを一つもっ獲得していないなら、優先度を元に戻す */
			if(kos_list_empty(&htsk_tcb->loc_mtx_list)) {
				//htsk_tcb->
				// @TODO 優先度を元に戻す。
			}
			cb->htsk_tcb = NULL;
		}
	}

	/* 待ち行列にいるタスクの待ちを解除 */
	do_tsk_dsp = kos_cancel_wait_all_for_delapi_nolock(&cb->wait_tsk_list);

	/* 登録解除 */
	g_kos_mtx_cb[mtxid - 1] = KOS_NULL;
	kos_list_insert_prev((kos_list_t *)&g_kos_mtx_cb_unused_list, (kos_list_t *)cb);

	/* スケジューラー起動 */
	if(do_tsk_dsp) {
		kos_tsk_dsp();
	}
end:
	kos_unlock;

	return er;
}

kos_er_t kos_tloc_mtx(kos_id_t mtxid, kos_tmo_t tmout)
{
	kos_mtx_cb_t *cb;
	kos_er_t er;

#ifdef KOS_CFG_ENA_PAR_CHK
	if(mtxid > g_kos_max_mtx || mtxid == 0)
		return KOS_E_ID;
#endif

	er = KOS_E_OK;

	kos_lock;

	cb = kos_get_mtx_cb(mtxid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end_unlock;
	}
	if(cb->htsk_tcb == KOS_NULL) {
		// ロックされていない場合
		cb->htsk_tcb = g_kos_cur_tcb;
		// タスクの獲得済ミューテックス一覧に追加
		kos_list_insert_prev(&cb->htsk_tcb->loc_mtx_list,
				&cb->htsk_list);
	} else {
		if(cb->htsk_tcb == g_kos_cur_tcb) {
			// 多重ロック
			cb->loc_cnt++;
		} else {
			if(tmout == KOS_TMO_POL) {
				er = KOS_E_TMOUT;
			} else {
				kos_tcb_t *tcb = g_kos_cur_tcb;
				kos_list_insert_prev(&cb->wait_tsk_list, &tcb->wait_list);
				tcb->st.lefttmo = tmout;
				tcb->st.wobjid = mtxid;
				tcb->st.tskwait = KOS_TTW_MTX;
				kos_wait_nolock(tcb);
				kos_unlock;
				er = tcb->rel_wai_er;
				goto end;
			}
		}
	}

end_unlock:
	kos_unlock;
end:

	return er;

}

kos_er_t kos_unl_mtx(kos_id_t mtxid)
{
	kos_mtx_cb_t *cb;
	kos_er_t er;

#ifdef KOS_CFG_ENA_PAR_CHK
	if(mtxid > g_kos_max_mtx || mtxid == 0)
		return KOS_E_ID;
#endif

	er = KOS_E_OK;

	kos_lock;

	cb = kos_get_mtx_cb(mtxid);

	er = kos_unl_mtx_core(cb);

end:
	kos_unlock;

	return er;
}

kos_er_t kos_unl_mtx_core(kos_mtx_cb_t *cb)
{
	kos_er_t er;

	er = KOS_E_OK;

	if(kos_list_empty(&cb->wait_tsk_list)) {
		if(cb->htsk_tcb != g_kos_cur_tcb) {
			// 異常状態
			er = KOS_E_SYS;
		} else {
			if(cb->loc_cnt > 0) {
				cb->loc_cnt--;
			} else {
				cb->htsk_tcb = KOS_NULL;
				kos_list_remove(&cb->htsk_list);
			}
		}
	} else {
		kos_tcb_t *tcb = (kos_tcb_t *)cb->wait_tsk_list.next;
		cb->htsk_tcb = tcb;
		kos_cancel_wait_nolock(tcb, KOS_E_OK);
		kos_tsk_dsp();
	}

	return er;
}

#endif
