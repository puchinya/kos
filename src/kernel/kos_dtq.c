/*!
 *	@file	kos_dtq.c
 *	@brief	datq queue API's implement.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "kos_local.h"

#ifdef KOS_CFG_SPT_DTQ

#define CB_TO_INDEX(cb)			(((uintptr_t)cb - (uintptr_t)g_kos_dtq_cb_inst) / sizeof(kos_dtq_cb_t))
#define kos_get_dtq_cb(dtqid)	(g_kos_dtq_cb[(dtqid) - 1])

kos_er_id_t kos_cre_dtq(const kos_cdtq_t *pk_cdtq)
{
	kos_dtq_cb_t *cb;
	int empty_index;
	kos_er_id_t er_id;
	
	kos_lock;
	
#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
	{
		kos_id_t last_id = g_kos_last_dtqid;

		if(last_id < g_kos_max_dtq) {
			empty_index = last_id;
			g_kos_last_dtqid = last_id + 1;
		} else {
			if(kos_list_empty(&g_kos_dtq_cb_unused_list)) {
				er_id = KOS_E_NOID;
				goto end;
			} else {
				kos_dtq_cb_t *unused_cb = (kos_dtq_cb_t *)g_kos_dtq_cb_unused_list.next;
				empty_index = CB_TO_INDEX(unused_cb);
				kos_list_remove((kos_list_t *)unused_cb);
			}
		}
	}
#else
	empty_index = kos_find_null((void **)g_kos_dtq_cb, g_kos_max_dtq);
	if(empty_index < 0) {
		er_id = KOS_E_NOID;
		goto end;
	}
#endif
	
	cb = &g_kos_dtq_cb_inst[empty_index];
	g_kos_dtq_cb[empty_index] = cb;
	
	cb->cdtq = *pk_cdtq;
	cb->dtq_rp = 0;
	cb->dtq_wp = 0;
	cb->sdtqcnt = 0;
	kos_list_init(&cb->r_wait_tsk_list);
	kos_list_init(&cb->s_wait_tsk_list);
	
	er_id = empty_index + 1;

	if(cb->cdtq.dtq == NULL) {
		cb->dtq = kos_alloc_nolock(sizeof(kos_vp_int_t) * cb->cdtq.dtqcnt);
		if(cb->dtq == NULL) {
			g_kos_dtq_cb[empty_index] = KOS_NULL;
#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
			kos_list_insert_prev(&g_kos_dtq_cb_unused_list, (kos_list_t *)cb);
#endif
			er_id = KOS_E_NOID;
			goto end;
		}
	} else {
		cb->dtq = cb->cdtq.dtq;
	}
end:
	kos_unlock;
	
	return er_id;
}

kos_er_t kos_del_dtq(kos_id_t dtqid)
{
	kos_dtq_cb_t *cb;
	kos_er_t er;
	kos_bool_t do_tsk_dsp;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(dtqid > g_kos_max_dtq || dtqid == 0)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_dtq_cb(dtqid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	/* 待ち行列にいるタスクの待ちを解除 */
	do_tsk_dsp = kos_cancel_wait_all_for_delapi_nolock(&cb->r_wait_tsk_list);
	do_tsk_dsp |= kos_cancel_wait_all_for_delapi_nolock(&cb->s_wait_tsk_list);

	if(cb->cdtq.dtq == NULL) {
		kos_free_nolock(cb->dtq);
	}

	/* 登録解除 */
	g_kos_dtq_cb[dtqid - 1] = KOS_NULL;
#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
	kos_list_insert_prev(&g_kos_dtq_cb_unused_list, (kos_list_t *)cb);
#endif
	
	/* スケジューラー起動 */
	if(do_tsk_dsp) {
		kos_tsk_dsp();
	}
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_tsnd_dtq(kos_id_t dtqid, kos_vp_int_t data, kos_tmo_t tmout)
{
	kos_dtq_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(dtqid > g_kos_max_dtq || dtqid == 0)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_dtq_cb(dtqid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end_unlock;
	}
	
	if(!kos_list_empty(&cb->r_wait_tsk_list)) {
		/* 受信待ちのタスクがいればデータを渡して終わり */
		kos_tcb_t *tcb;
		
		tcb = (kos_tcb_t *)cb->r_wait_tsk_list.next;
		*((kos_vp_int_t *)tcb->wait_exinf) = data;
		
		kos_cancel_wait_nolock(tcb, KOS_E_OK);
		kos_tsk_dsp();
		
	} else {
		if(cb->sdtqcnt == cb->cdtq.dtqcnt) {
			/* 空きがない場合 */
			if(tmout == KOS_TMO_POL) {
				er = KOS_E_TMOUT;
				goto end_unlock;
			} else {
				kos_tcb_t *tcb = g_kos_cur_tcb;
				kos_list_insert_prev(&cb->s_wait_tsk_list, &tcb->wait_list);
				tcb->st.lefttmo = tmout;
				tcb->st.wobjid = dtqid;
				tcb->st.tskwait = KOS_TTW_SDTQ;
				tcb->wait_exinf = (kos_vp_t)data;
				kos_wait_nolock(tcb);
				kos_unlock;
				er = tcb->rel_wai_er;
				goto end;
			}
		} else {
			/* 空きがあればキューにデータを追加 */
			kos_uint_t wp;
			
			wp = cb->dtq_wp;
			
			((kos_vp_int_t *)cb->dtq)[wp] = data;
			
			wp++;
			if(wp >= cb->cdtq.dtqcnt) wp = 0;
			cb->dtq_wp = wp;
			cb->sdtqcnt++;
			
			if(!kos_list_empty(&cb->r_wait_tsk_list)) {
				kos_tcb_t *tcb = (kos_tcb_t *)cb->r_wait_tsk_list.next;
				
				kos_cancel_wait_nolock(tcb, KOS_E_OK);
				kos_tsk_dsp();
			}
		}
	}
end_unlock:
	kos_unlock;
end:
	
	return er;
}

kos_er_t kos_ipsnd_dtq(kos_id_t dtqid, kos_vp_int_t data)
{
	kos_dtq_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(dtqid > g_kos_max_dtq || dtqid == 0)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_ilock;
	
	cb = kos_get_dtq_cb(dtqid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	if(!kos_list_empty(&cb->r_wait_tsk_list)) {
		/* 受信待ちのタスクがいればデータを渡して終わり */
		kos_tcb_t *tcb;
		
		tcb = (kos_tcb_t *)cb->r_wait_tsk_list.next;
		*((kos_vp_int_t *)tcb->wait_exinf) = data;
		
		kos_cancel_wait_nolock(tcb, KOS_E_OK);
		kos_itsk_dsp();
		
	} else {
		if(cb->sdtqcnt == cb->cdtq.dtqcnt) {
			/* 空きがない場合 */
			er = KOS_E_TMOUT;
			goto end;
		} else {
			/* 空きがあればキューにデータを追加 */
			kos_uint_t wp;
			
			wp = cb->dtq_wp;
			
			((kos_vp_int_t *)cb->dtq)[wp] = data;
			
			wp++;
			if(wp >= cb->cdtq.dtqcnt) wp = 0;
			cb->dtq_wp = wp;
			cb->sdtqcnt++;
			
			if(!kos_list_empty(&cb->r_wait_tsk_list)) {
				kos_tcb_t *tcb = (kos_tcb_t *)cb->r_wait_tsk_list.next;
				
				kos_cancel_wait_nolock(tcb, KOS_E_OK);
				kos_itsk_dsp();
			}
		}
	}
end:
	kos_iunlock;
	
	return er;
}

kos_er_t kos_fsnd_dtq(kos_id_t dtqid, kos_vp_int_t data)
{
	return KOS_E_NOSPT;
}

kos_er_t kos_ifsnd_dtq(kos_id_t dtqid, kos_vp_int_t data)
{
	return KOS_E_NOSPT;
}

kos_er_t kos_trcv_dtq(kos_id_t dtqid, kos_vp_int_t *p_data, kos_tmo_t tmout)
{
	kos_dtq_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(dtqid > g_kos_max_dtq || dtqid == 0)
		return KOS_E_ID;
	if(p_data == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_dtq_cb(dtqid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end_unlock;
	}
	
	if(cb->sdtqcnt > 0) {
		/* キューにデータがあれば取り出す */
		kos_uint_t rp;
		
		rp = cb->dtq_rp;
		
		*p_data = ((kos_vp_int_t *)cb->dtq)[rp];
		
		rp++;
		if(rp >= cb->cdtq.dtqcnt) rp = 0;
		cb->dtq_rp = rp;
		cb->sdtqcnt--;
		
		/* 送信待ちのタスクが入れば送信データをキューの末尾に追加して待ちを解除 */
		if(!kos_list_empty(&cb->s_wait_tsk_list)) {
			kos_tcb_t *tcb;
			
			tcb = (kos_tcb_t *)cb->s_wait_tsk_list.next;
			
			((kos_vp_int_t *)cb->dtq)[cb->dtq_wp++] = (kos_vp_int_t)tcb->wait_exinf;
			cb->sdtqcnt++;
			
			kos_cancel_wait_nolock(tcb, KOS_E_OK);
			kos_tsk_dsp();
		}
	} else {
		if(!kos_list_empty(&cb->s_wait_tsk_list)) {
			/* データ送信待ちのタスクが入ればデータを取得して待ちを解除 */
			kos_tcb_t *tcb;
			
			tcb = (kos_tcb_t *)cb->s_wait_tsk_list.next;
			
			*p_data = (kos_vp_int_t)tcb->wait_exinf;
			
			kos_cancel_wait_nolock(tcb, KOS_E_OK);
			kos_tsk_dsp();
			
		} else if(tmout == KOS_TMO_POL) {
			er = KOS_E_TMOUT;
			goto end_unlock;
		} else {
			/* データが送信されるまで待つ */
			kos_tcb_t *tcb = g_kos_cur_tcb;
			kos_list_insert_prev(&cb->r_wait_tsk_list, &tcb->wait_list);
			tcb->st.lefttmo = tmout;
			tcb->st.wobjid = dtqid;
			tcb->st.tskwait = KOS_TTW_RDTQ;
			tcb->wait_exinf = (kos_vp_t)p_data;
			kos_wait_nolock(tcb);
			kos_unlock;
			er = tcb->rel_wai_er;
			goto end;
		}
	}
end_unlock:
	kos_unlock;
end:
	
	return er;
}

kos_er_t kos_ref_dtq(kos_id_t dtqid, kos_rdtq_t *pk_rdtq)
{
	kos_dtq_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(dtqid > g_kos_max_dtq || dtqid == 0)
		return KOS_E_ID;
	if(pk_rdtq == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_dtq_cb(dtqid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	if(kos_list_empty(&cb->s_wait_tsk_list)) {
		pk_rdtq->stskid = KOS_TSK_NONE;
	} else {
		kos_tcb_t *tcb = (kos_tcb_t *)cb->s_wait_tsk_list.next;
		pk_rdtq->stskid = tcb->id;
	}
	if(kos_list_empty(&cb->r_wait_tsk_list)) {
		pk_rdtq->rtskid = KOS_TSK_NONE;
	} else {
		kos_tcb_t *tcb = (kos_tcb_t *)cb->r_wait_tsk_list.next;
		pk_rdtq->rtskid = tcb->id;
	}
	
	pk_rdtq->sdtqcnt = cb->sdtqcnt;
	
end:
	kos_unlock;
	
	return er;
}

#endif
