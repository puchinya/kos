
#include "kos_local.h"

/*-----------------------------------------------------------------------------
	時間管理機能/システム時刻管理
-----------------------------------------------------------------------------*/
static kos_systim_t s_systim;				/* システム時刻 */

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
	/* 多重割り込み禁止 */
	kos_lock;
	
	/* システム時刻をインクリメント */
	s_systim++;
	
	/* タイムアウト待ちのタスクを処理 */
	kos_process_tmo();
	
	/* 周期ハンドラの処理 */
	kos_process_cyc();
	
	//kos_set_pend_sv();
	
	kos_unlock;
	
	return KOS_E_OK;
}
