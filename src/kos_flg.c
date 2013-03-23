
#include "kos_local.h"

/*-----------------------------------------------------------------------------
	同期・通信機能/イベントフラグ
-----------------------------------------------------------------------------*/
#ifdef KOS_CFG_SPT_FLG

static KOS_INLINE kos_flg_cb_t *kos_get_flg_cb(kos_id_t flgid)
{
	return g_kos_flg_cb[flgid - 1];
}

kos_er_id_t kos_cre_flg(const kos_cflg_t *pk_cflg)
{
	kos_flg_cb_t *cb;
	int empty_index;
	kos_er_id_t er_id;
	
	kos_lock;
	
	empty_index = kos_find_null((void **)g_kos_flg_cb, g_kos_max_flg);
	if(empty_index < 0) {
		er_id = KOS_E_NOID;
		goto end;
	}
	
	cb = &g_kos_flg_cb_inst[empty_index];
	g_kos_flg_cb[empty_index] = cb;
	
	cb->cflg = *pk_cflg;
	cb->flgptn = pk_cflg->iflgptn;
	kos_list_init(&cb->wait_tsk_list);
	
	er_id = empty_index + 1;
end:
	kos_unlock;
	
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
		if(cb->cflg.flgatr & KOS_TA_CLR) {
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
