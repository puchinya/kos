
#include "kos_local.h"
#include <string.h>

/*-----------------------------------------------------------------------------
	時間管理機能/周期ハンドラ
-----------------------------------------------------------------------------*/

static kos_list_t s_cyc_list;				/* アクティブな周期ハンドラのリスト */

static KOS_INLINE kos_cyc_cb_t *kos_get_cyc_cb(kos_id_t cycid)
{
	return g_kos_cyc_cb[cycid-1];
}

void kos_init_cyc(void)
{
	kos_list_init(&s_cyc_list);
}

void kos_process_cyc(void)
{
	kos_cyc_cb_t *l, *end, *next;
	
	end = (kos_cyc_cb_t *)&s_cyc_list;
	
	for(l = (kos_cyc_cb_t *)end->list.next; l != end; ) {
		next = (kos_cyc_cb_t *)l->list.next;
		if(--l->st.lefttim == 0) {
			/* 実行する */
			((void (*)(kos_vp_int_t))l->ccyc.cychdr)(l->ccyc.exinf);
			l->st.lefttim = l->ccyc.cyctim;
		}
		l = next;
	}
}

static void kos_sta_cyc_nolock(kos_cyc_cb_t *cb)
{
	cb->st.lefttim = cb->ccyc.cyctim;
	cb->st.cycstat = KOS_TCYC_STA;
	
	kos_list_insert_prev(&s_cyc_list, &cb->list);
}

kos_er_t kos_cre_cyc(const kos_ccyc_t *pk_ccyc)
{
	kos_cyc_cb_t *cb;
	int empty_index;
	kos_er_id_t er_id;
	kos_atr_t atr;
	
	kos_lock;
	
	empty_index = kos_find_null((void **)g_kos_cyc_cb, g_kos_max_cyc);
	if(empty_index < 0) {
		er_id = KOS_E_NOID;
		kos_free(cb);
		goto end;
	}
	
	cb = &g_kos_cyc_cb_inst[empty_index];
	g_kos_cyc_cb[empty_index] = cb;
	
	cb->ccyc = *pk_ccyc;
	cb->st.cycstat = KOS_TCYC_STP;
	cb->st.lefttim = 0; 		/* 必須ではない */
	
	atr = pk_ccyc->cycatr;
	
	if(atr & KOS_TA_STA) {
		kos_sta_cyc_nolock(cb);
	}
	
	er_id = empty_index + 1;
end:
	kos_unlock;
	
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
