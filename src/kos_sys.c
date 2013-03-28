/*!
 *	@file	kos_sys.c
 *	@brief	system state API's implement.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/28
 *	@author	puchinya
 */

#include "kos_local.h"

/*!
 *	@brief	get execution task ID
 */
kos_er_t kos_get_tid(kos_id_t *p_tskid)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(p_tskid == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	*p_tskid = g_kos_cur_tcb->id;
	
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
	
	*p_tskid = g_kos_cur_tcb->id;
	
	return KOS_E_OK;
}

kos_er_t kos_dis_dsp(void)
{
	g_kos_dsp = KOS_TRUE;
	
	return KOS_E_OK;
}

kos_er_t kos_ena_dsp(void)
{
	kos_lock;
	
	if(g_kos_dsp) {
		g_kos_dsp = KOS_FALSE;
		kos_schedule_nolock();
	}
	
	kos_unlock;
	
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
	return g_kos_dsp;
}

kos_bool_t kos_sns_dpn(void)
{
	return g_kos_dsp | kos_sns_ctx() | kos_sns_loc();
}
