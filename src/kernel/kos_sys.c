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

kos_er_t kos_rot_rdq(kos_pri_t tskpri)
{
	kos_tcb_t *cur_tcb;
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskpri >= g_kos_max_pri)
		return KOS_E_PAR;
#endif
	
	kos_lock;
	
	cur_tcb = g_kos_cur_tcb;
	
	/* TPRI_SELF指定なら実行中のタスクのベース優先度とする */
	if(tskpri == KOS_TPRI_SELF) {
		tskpri = cur_tcb->st.tskbpri;
	}
	
	if(tskpri == cur_tcb->st.tskpri) {
		kos_rdy_tsk_nolock(cur_tcb);
		kos_tsk_dsp();
	} else {
		kos_list_t *tcb;
		kos_list_t *rdy_que_i;
		
		rdy_que_i = &g_kos_rdy_que[(kos_int_t)tskpri - 1];
		if(kos_list_empty(rdy_que_i)) {
			tcb = rdy_que_i->next;
			kos_list_remove(tcb);
			kos_list_insert_prev(rdy_que_i, tcb);
		}
	}
end:
	kos_unlock;
	
	return KOS_E_OK;
}

kos_er_t kos_irot_rdq(kos_pri_t tskpri)
{
	kos_tcb_t *cur_tcb;
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskpri == KOS_TPRI_SELF || tskpri >= g_kos_max_pri)
		return KOS_E_PAR;
#endif
	
	kos_ilock;
	
	cur_tcb = g_kos_cur_tcb;
	
	if(tskpri == cur_tcb->st.tskpri) {
		kos_rdy_tsk_nolock(cur_tcb);
		kos_itsk_dsp();
	} else {
		kos_list_t *tcb;
		kos_list_t *rdy_que_i;
		
		rdy_que_i = &g_kos_rdy_que[(kos_int_t)tskpri - 1];
		if(kos_list_empty(rdy_que_i)) {
			tcb = rdy_que_i->next;
			kos_list_remove(tcb);
			kos_list_insert_prev(rdy_que_i, tcb);
		}
	}
end:
	kos_iunlock;
	
	return KOS_E_OK;
}

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
		if(g_kos_pend_dsp) {
			g_kos_pend_dsp = KOS_FALSE;
			kos_tsk_dsp();
		}
	}
	
	kos_unlock;
	
	return KOS_E_OK;
}

#ifdef __GNUC__
kos_bool_t kos_sns_ctx(void)
{
	return __get_IPSR() == 0 ? 0 : 1;
}
#else
__asm kos_bool_t kos_sns_ctx(void)
{
	MRS	R0, IPSR
	CMP R0, #0
	MOVNE R0, #1
	BX LR
}
#endif

kos_bool_t kos_sns_dsp(void)
{
	return g_kos_dsp;
}

kos_bool_t kos_sns_dpn(void)
{
	return g_kos_dsp | kos_sns_ctx() | kos_sns_loc();
}
