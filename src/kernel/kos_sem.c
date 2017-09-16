/*!
 *	@file	kos_tsk.c
 *	@brief	semaphore API's implement.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "kos_local.h"

/*-----------------------------------------------------------------------------
	同期・通信機能/セマフォ
-----------------------------------------------------------------------------*/
#ifdef KOS_CFG_SPT_SEM

#define CB_TO_INDEX(cb)			(((uintptr_t)cb - (uintptr_t)g_kos_sem_cb_inst) / sizeof(kos_sem_cb_t))
#define kos_get_sem_cb(semid)	(g_kos_sem_cb[(semid) - 1])

kos_er_id_t kos_cre_sem(const kos_csem_t *pk_csem)
{
	kos_sem_cb_t *cb;
	kos_int_t empty_index;
	kos_er_id_t er_id;
	
	kos_lock;
	
#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
	{
		kos_id_t last_id = g_kos_last_semid;

		if(last_id < g_kos_max_sem) {
			empty_index = last_id;
			g_kos_last_semid = last_id + 1;
		} else {
			if(kos_list_empty(&g_kos_sem_cb_unused_list)) {
				er_id = KOS_E_NOID;
				goto end;
			} else {
				kos_sem_cb_t *unused_cb = (kos_sem_cb_t *)g_kos_sem_cb_unused_list.next;
				empty_index = CB_TO_INDEX(unused_cb);
				kos_list_remove((kos_list_t *)unused_cb);
			}
		}
	}
#else
	empty_index = kos_find_null((void **)g_kos_sem_cb, g_kos_max_sem);
	if(empty_index < 0) {
		er_id = KOS_E_NOID;
		goto end;
	}
#endif
	
	cb = &g_kos_sem_cb_inst[empty_index];
	g_kos_sem_cb[empty_index] = cb;
	
	cb->csem = *pk_csem;
	cb->semcnt = pk_csem->isemcnt;
	kos_list_init(&cb->wait_tsk_list);
	
	er_id = empty_index + 1;
end:
	kos_unlock;
	
	return er_id;
}

kos_er_t kos_del_sem(kos_id_t semid)
{
	kos_sem_cb_t *cb;
	kos_er_t er;
	kos_bool_t do_tsk_dsp;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(semid > g_kos_max_sem || semid == 0)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_sem_cb(semid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	/* 待ち行列にいるタスクの待ちを解除 */
	do_tsk_dsp = kos_cancel_wait_all_for_delapi_nolock(&cb->wait_tsk_list);
	
	/* 登録解除 */
	g_kos_sem_cb[semid - 1] = KOS_NULL;
#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
	kos_list_insert_prev(&g_kos_sem_cb_unused_list, (kos_list_t *)cb);
#endif
	
	/* スケジューラー起動 */
	if(do_tsk_dsp) {
		kos_tsk_dsp();
	}
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_twai_sem(kos_id_t semid, kos_tmo_t tmout)
{
	kos_sem_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(semid > g_kos_max_sem || semid == 0)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_sem_cb(semid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end_unlock;
	}
	if(cb->semcnt > 0) {
		cb->semcnt--;
	} else {
		if(tmout == KOS_TMO_POL) {
			er = KOS_E_TMOUT;
		} else {
			kos_tcb_t *tcb = g_kos_cur_tcb;
			kos_list_insert_prev(&cb->wait_tsk_list, &tcb->wait_list);
			tcb->st.lefttmo = tmout;
			tcb->st.wobjid = semid;
			tcb->st.tskwait = KOS_TTW_SEM;
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

kos_er_t kos_sig_sem(kos_id_t semid)
{
	kos_sem_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(semid > g_kos_max_sem || semid == 0)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_sem_cb(semid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	if(kos_list_empty(&cb->wait_tsk_list)) {
		if(cb->semcnt < cb->csem.maxsem) {
			cb->semcnt++;
		} else {
			er = KOS_E_QOVR;
			goto end;
		}
	} else {
		kos_tcb_t *tcb = (kos_tcb_t *)cb->wait_tsk_list.next;
		
		kos_cancel_wait_nolock(tcb, KOS_E_OK);
		kos_tsk_dsp();
	}
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_isig_sem(kos_id_t semid)
{
	kos_sem_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(semid > g_kos_max_sem || semid == 0)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_ilock;
	
	cb = kos_get_sem_cb(semid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	if(kos_list_empty(&cb->wait_tsk_list)) {
		if(cb->semcnt < cb->csem.maxsem) {
			cb->semcnt++;
		} else {
			er = KOS_E_QOVR;
			goto end;
		}
	} else {
		kos_tcb_t *tcb = (kos_tcb_t *)cb->wait_tsk_list.next;
		
		kos_cancel_wait_nolock(tcb, KOS_E_OK);
		kos_itsk_dsp();
	}
end:
	kos_iunlock;
	
	return er;
}

kos_er_t kos_ref_sem(kos_id_t semid, kos_rsem_t *pk_rsem)
{
	kos_sem_cb_t *cb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(semid > g_kos_max_sem || semid == 0)
		return KOS_E_ID;
	if(pk_rsem == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	cb = kos_get_sem_cb(semid);
	if(cb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	if(kos_list_empty(&cb->wait_tsk_list)) {
		pk_rsem->wtskid = KOS_TSK_NONE;
	} else {
		kos_tcb_t *tcb = (kos_tcb_t *)cb->wait_tsk_list.next;
		pk_rsem->wtskid = tcb->id;
	}
	
	pk_rsem->semcnt = cb->semcnt;
	
end:
	kos_unlock;
	
	return er;
}

#endif
