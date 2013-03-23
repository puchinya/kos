
#include "kos_local.h"

/*-----------------------------------------------------------------------------
	同期・通信機能/セマフォ
-----------------------------------------------------------------------------*/
#ifdef KOS_CFG_SPT_SEM

static KOS_INLINE kos_sem_cb_t *kos_get_sem_cb(kos_id_t semid)
{
	return g_kos_sem_cb[semid - 1];
}

kos_er_id_t kos_cre_sem(const kos_csem_t *pk_csem)
{
	kos_sem_cb_t *cb;
	int empty_index;
	kos_er_id_t er_id;
	
	kos_lock;
	
	empty_index = kos_find_null((void **)g_kos_sem_cb, g_kos_max_sem);
	if(empty_index < 0) {
		er_id = KOS_E_NOID;
		goto end;
	}
	
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
	kos_cancel_wait_all_for_delapi_nolock(&cb->wait_tsk_list);
	
	/* コントロールブロックのメモリを開放 */
	kos_free(cb);
	
	/* ID=>CB変換をクリア */
	g_kos_sem_cb[semid-1] = KOS_NULL;
	
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
		goto end;
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
			er = kos_wait_nolock(tcb);
		}
	}
end:
	kos_unlock;
	
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
		kos_schedule();
	}
end:
	kos_unlock;
	
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
