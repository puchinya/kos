/*!
 *	@file	kos_tim.c
 *	@brief	system time API's implement.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "kos_local.h"

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
	g_kos_systim = *p_systim;
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
	*p_systim = g_kos_systim;
	kos_unlock;
	
	return KOS_E_OK;
}

kos_er_t kos_isig_tim(void)
{
	if(!kos_sns_ctx()) {
		return KOS_E_CTX;
	}
	
	/* 多重割り込み禁止 */
	kos_ilock;
	
	/* システム時刻をインクリメント */
	g_kos_systim++;
	
	/* タイムアウト待ちのタスクを処理 */
	kos_process_tmo();
	
	/* 周期ハンドラの処理 */
	kos_process_cyc();
	
	kos_iunlock;
	
	return KOS_E_OK;
}
