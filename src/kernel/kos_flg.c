/*!
 *	@file	kos_flg.c
 *	@brief	event flag API's implement.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "kos_local.h"

/*-----------------------------------------------------------------------------
	同期・通信機能/イベントフラグ
-----------------------------------------------------------------------------*/
#ifdef KOS_CFG_SPT_FLG

#define CB_TO_INDEX(cb)			(((uintptr_t)cb - (uintptr_t)g_kos_flg_cb_inst) / sizeof(kos_flg_cb_t))
#define kos_get_flg_cb(flgid)	(g_kos_flg_cb[(flgid) - 1])

kos_er_id_t kos_cre_flg(const kos_cflg_t *pk_cflg)
{
	kos_flg_cb_t *cb;
	kos_int_t empty_index;
	kos_er_id_t er_id;
	
	kos_lock;
	
#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
	{
		kos_id_t last_id = g_kos_last_flgid;

		if(last_id < g_kos_max_flg) {
			empty_index = last_id;
			g_kos_last_flgid = last_id + 1;
		} else {
			if(kos_list_empty(&g_kos_flg_cb_unused_list)) {
				er_id = KOS_E_NOID;
				goto end;
			} else {
				kos_flg_cb_t *unused_cb = (kos_flg_cb_t *)g_kos_flg_cb_unused_list.next;
				empty_index = CB_TO_INDEX(unused_cb);
				kos_list_remove((kos_list_t *)unused_cb);
			}
		}
	}
#else
	empty_index = kos_find_null((void **)g_kos_flg_cb, g_kos_max_flg);
	if(empty_index < 0) {
		er_id = KOS_E_NOID;
		goto end;
	}
#endif
	
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
	kos_bool_t do_tsk_dsp;
	
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
	do_tsk_dsp = kos_cancel_wait_all_for_delapi_nolock(&cb->wait_tsk_list);

	/* 登録解除 */
	g_kos_flg_cb[flgid - 1] = KOS_NULL;
#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
	kos_list_insert_prev(&g_kos_flg_cb_unused_list, (kos_list_t *)cb);
#endif
	
	/* スケジューラー起動 */
	if(do_tsk_dsp) {
		kos_tsk_dsp();
	}
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
		kos_bool_t do_tsk_dsp = KOS_FALSE;
		
		for(l = wait_tsk_list->next; l != wait_tsk_list; l = l->next) {
			kos_tcb_t *tcb;
			kos_flg_wait_exinf *wait_exinf;
			
			tcb = (kos_tcb_t *)l;
			wait_exinf = (kos_flg_wait_exinf *)tcb->wait_exinf;
			
			if(wait_exinf->wfmode == KOS_TWF_ANDW) {
				if((wait_exinf->waiptn & cb->flgptn) != wait_exinf->waiptn)
					continue;
			} else {
				if(!(wait_exinf->waiptn & cb->flgptn))
					continue;
			}
			
			wait_exinf->relptn = cb->flgptn;
			
			kos_cancel_wait_nolock(tcb, KOS_E_OK);
			do_tsk_dsp = KOS_TRUE;
			
			if(cb->cflg->flgatr & KOS_TA_CLR) {
				/* TA_CLRが指定されているなら、タスクをひとつ解除した時点でクリアする(仕様) */
				cb->flgptn = 0;
				break;
			}
			if(!(cb->cflg->flgatr & KOS_TA_WMUL)) {
				break;
			}
		}
		
		if(do_tsk_dsp) {
			kos_tsk_dsp();
		}
#else
		do {
			if(cb->wfmode == KOS_TWF_ANDW) {
				if((cb->waiptn & cb->flgptn) != cb->waiptn)
					break;
			} else {
				if(!(cb->waiptn & cb->flgptn))
					break;
			}
			*cb->relptn = cb->flgptn;
			if(cb->cflg.flgatr & KOS_TA_CLR) {
				/* タスクをひとつ解除した時点でクリアする(仕様) */
				cb->flgptn = 0;
			}
			kos_cancel_wait_nolock((kos_tcb_t *)cb->wait_tsk_list.next, KOS_E_OK);
			kos_tsk_dsp();
		} while(0);
#endif
	}

end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_iset_flg(kos_id_t flgid, kos_flgptn_t setptn)
{
	kos_flg_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(flgid > g_kos_max_flg || flgid == 0)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_ilock;
	
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
		kos_bool_t do_tsk_dsp = KOS_FALSE;
		
		for(l = wait_tsk_list->next; l != wait_tsk_list; l = l->next) {
			kos_tcb_t *tcb;
			kos_flg_wait_exinf *wait_exinf;
			
			tcb = (kos_tcb_t *)l;
			wait_exinf = (kos_flg_wait_exinf *)tcb->wait_exinf;
			
			if(wait_exinf->wfmode == KOS_TWF_ANDW) {
				if((wait_exinf->waiptn & cb->flgptn) != wait_exinf->waiptn)
					continue;
			} else {
				if(!(wait_exinf->waiptn & cb->flgptn))
					continue;
			}
			
			wait_exinf->relptn = cb->flgptn;
			
			kos_cancel_wait_nolock(tcb, KOS_E_OK);
			do_tsk_dsp = KOS_TRUE;
			
			if(cb->cflg->flgatr & KOS_TA_CLR) {
				/* TA_CLRが指定されているなら、タスクをひとつ解除した時点でクリアする(仕様) */
				cb->flgptn = 0;
				break;
			}
			if(!(cb->cflg->flgatr & KOS_TA_WMUL)) {
				break;
			}
		}
		
		if(do_tsk_dsp) {
			kos_itsk_dsp();
		}
#else
		do {
			if(cb->wfmode == KOS_TWF_ANDW) {
				if((cb->waiptn & cb->flgptn) != cb->waiptn)
					break;
			} else {
				if(!(cb->waiptn & cb->flgptn))
					break;
			}
			*cb->relptn = cb->flgptn;
			if(cb->cflg.flgatr & KOS_TA_CLR) {
				/* タスクをひとつ解除した時点でクリアする(仕様) */
				cb->flgptn = 0;
			}
			kos_cancel_wait_nolock((kos_tcb_t *)cb->wait_tsk_list.next, KOS_E_OK);
			kos_itsk_dsp();
		} while(0);
#endif
	}
end:
	kos_iunlock;
	
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
		goto end_unlock;
	}
	
#ifdef KOS_CFG_SPT_FLG_WMUL
	if(!(cb->cflg->flgatr & KOS_TA_WMUL) &&
		!kos_list_empty(&cb->wait_tsk_list))
	{
		er = KOS_E_ILUSE;
		goto end_unlock;
	}
#else
	if(!kos_list_empty(&cb->wait_tsk_list)) {
		er = KOS_E_ILUSE;
		goto end_unlock;
	}
#endif
	
	if(wfmode == KOS_TWF_ANDW) {
		if(cb->flgptn & waiptn) {
			*p_flgptn = cb->flgptn;
			goto end_unlock;
		}
	} else {
		if((cb->flgptn & waiptn) == waiptn) {
			*p_flgptn = cb->flgptn;
			goto end_unlock;
		}
	}
	
	if(tmout == KOS_TMO_POL) {
		er = KOS_E_TMOUT;
	} else {
		kos_tcb_t *tcb;

		tcb = g_kos_cur_tcb;
		tcb->st.lefttmo = tmout;
		tcb->st.wobjid = flgid;
		tcb->st.tskwait = KOS_TTW_FLG;
		kos_list_insert_prev(&cb->wait_tsk_list, &tcb->wait_list);
		
#ifdef KOS_CFG_SPT_FLG_WMUL
		{
			kos_flg_wait_exinf_t wait_exinf;
			wait_exinf.wfmod = wfmode;
			wait_exinf.waiptn = waiptn;
			wait_exinf.relptn = p_flgptn;
			tcb->wait_exinf = (kos_vp_t)&wait_exinf;
		}
#else
		cb->wfmode = wfmode;
		cb->waiptn = waiptn;
		cb->relptn = p_flgptn;
#endif
		kos_wait_nolock(tcb);
		kos_unlock;
		er = tcb->rel_wai_er;
		goto end;
	}
end_unlock:
	kos_unlock;
end:
	
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
