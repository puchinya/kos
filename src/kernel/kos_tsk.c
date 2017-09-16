/*!
 *	@file	kos_tsk.c
 *	@brief	task API's implement.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "kos_local.h"
#include <string.h>

/*-----------------------------------------------------------------------------
	タスク管理機能
-----------------------------------------------------------------------------*/
#define kos_get_tcb(tskid)	(g_kos_tcb[(tskid)-1])

void kos_local_act_tsk_impl_nolock(kos_tcb_t *tcb, kos_bool_t is_ctx);

static void kos_tsk_entry(kos_tcb_t *tcb)
{
	((void (*)(kos_vp_int_t))tcb->ctsk.task)(tcb->ctsk.exinf);
	kos_ext_tsk();
}

void kos_local_act_tsk_impl_nolock(kos_tcb_t *tcb, kos_bool_t is_ctx)
{	
	kos_uint_t *sp;
	
	tcb->sp = sp = (uint32_t *)((uint8_t *)tcb->sp_top + tcb->ctsk.stksz - 16*4);
	sp[0] = 0;								// R4
	sp[1] = 0;								// R5
	sp[2] = 0;								// R6
	sp[3] = 0;								// R7
	sp[4] = 0;								// R8
	sp[5] = 0;								// R9
	sp[6] = 0;								// R10
	sp[7] = 0;								// R11
	sp[8] = (uint32_t)tcb;					// R0
	sp[9] = 0;								// R1
	sp[10] = 0;								// R2
	sp[11] = 0;								// R3
	sp[12] = 0;								// R12
	sp[13] = 0;								// LR
	sp[14] = (uint32_t)kos_tsk_entry;		// PC
	sp[15] = 0x21000000;					// PSR

	kos_rdy_tsk_nolock(tcb);
	if(is_ctx) {
		kos_itsk_dsp();
	} else {
		kos_tsk_dsp();
	}
}

kos_er_id_t kos_cre_tsk(const kos_ctsk_t *ctsk)
{
	kos_tcb_t *tcb;
	kos_int_t empty_index;
	kos_er_id_t er_id;
	
	kos_lock;
	
#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
	{
		kos_id_t last_id = g_kos_last_tskid;

		if(last_id < g_kos_max_tsk) {
			empty_index = last_id;
			g_kos_last_tskid = last_id + 1;
		} else {
			if(kos_list_empty(&g_kos_tcb_unused_list)) {
				er_id = KOS_E_NOID;
				goto end;
			} else {
				kos_tcb_t *unused_tcb = (kos_tcb_t *)g_kos_tcb_unused_list.next;
				empty_index = unused_tcb->id - 1;
				kos_list_remove((kos_list_t *)unused_tcb);
			}
		}
	}
#else
	empty_index = kos_find_null((void **)g_kos_tcb, g_kos_max_tsk);
	if(empty_index < 0) {
		er_id = KOS_E_NOID;
		goto end;
	}
#endif
	
	tcb = &g_kos_tcb_inst[empty_index];
	g_kos_tcb[empty_index] = tcb;
	
	tcb->ctsk			= *ctsk;
	tcb->st.tskstat		= KOS_TTS_DMT;
	tcb->st.tskpri		= ctsk->itskpri;
	tcb->st.tskbpri		= ctsk->itskpri;
	tcb->st.tskwait		= 0;
	tcb->st.wobjid		= 0;
	tcb->st.lefttmo		= 0;
	tcb->st.actcnt		= 0;
	tcb->st.wupcnt		= 0;
	tcb->st.suscnt		= 0;
	tcb->sp				= KOS_NULL;
	if(tcb->ctsk.stk != NULL) {
		tcb->sp_top		= tcb->ctsk.stk;
	} else {
		kos_vp_t sp_top = kos_alloc_nolock(tcb->ctsk.stksz);
		if(sp_top == NULL) {
			/* 登録解除 */
			g_kos_tcb[empty_index] = KOS_NULL;
#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
			kos_list_insert_prev(&g_kos_tcb_unused_list, (kos_list_t *)tcb);
#endif
			er_id = KOS_E_NOMEM;
			goto end;
		}
		tcb->sp_top	= sp_top;
	}
	
	tcb->id = empty_index + 1;
#ifdef KOS_CFG_STKCHK
	memset(tcb->sp_top, 0xCC, tcb->ctsk.stksz);
#endif

	if(ctsk->tskatr & KOS_TA_ACT) {
		tcb->st.actcnt = 1;
		kos_local_act_tsk_impl_nolock(tcb, KOS_FALSE);
	}
	
	er_id = empty_index + 1;
end:
	kos_unlock;
	
	return er_id;
}

kos_er_t kos_del_tsk(kos_id_t tskid)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;

	if(tskid == KOS_TSK_SELF) {
		/* 自身で呼び出した場合は当然実行中なのでエラー */
		er = KOS_E_OBJ;
		goto end;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	/* 休止状態でなければ削除はできない。 */
	if(tcb->st.tskstat != KOS_TTS_DMT) {
		er = KOS_E_OBJ;
		goto end;
	}
	
	if(tcb->ctsk.stk == NULL) {
		kos_free_nolock(tcb->sp_top);
	}

	/* 登録解除 */
	g_kos_tcb[tskid - 1] = KOS_NULL;
#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
	kos_list_insert_prev(&g_kos_tcb_unused_list, (kos_list_t *)tcb);
#endif
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_act_tsk(kos_id_t tskid)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	if(tcb->st.tskstat == KOS_TTS_DMT) {
		kos_local_act_tsk_impl_nolock(tcb, KOS_FALSE);
	} else {
		if(tcb->st.actcnt >= KOS_MAX_ACT_CNT) {
			er = KOS_E_QOVR;
		} else {
			tcb->st.actcnt++;
		}
	}
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_iact_tsk(kos_id_t tskid)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid == KOS_TSK_SELF || tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_ilock;
	
	tcb = kos_get_tcb(tskid);
	if(!tcb) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	if(tcb->st.tskstat == KOS_TTS_DMT) {
		kos_local_act_tsk_impl_nolock(tcb, KOS_TRUE);
	} else {
		if(tcb->st.actcnt >= KOS_MAX_ACT_CNT) {
			er = KOS_E_QOVR;
		} else {
			tcb->st.actcnt++;
		}
	}
	
end:
	kos_unlock;
	
	return er;
}

kos_er_uint_t kos_can_act(kos_id_t tskid)
{
	kos_er_uint_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return (kos_er_uint_t)KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;

	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = (kos_er_uint_t)KOS_E_NOEXS;
			goto end;
		}
	}
	
	er = (kos_er_uint_t)tcb->st.actcnt;
	tcb->st.actcnt = 0;
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_sta_tsk(kos_id_t tskid, kos_vp_int_t stacd)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
	if(tskid == KOS_TSK_SELF)
		return KOS_E_OBJ;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	tcb = kos_get_tcb(tskid);
	if(!tcb) {
		er = KOS_E_NOEXS;
		goto end;
	}
	if(tcb == g_kos_cur_tcb) {
		er = KOS_E_OBJ;
		goto end;
	}
	
	if(tcb->st.tskstat != KOS_TTS_DMT) {
		er = KOS_E_OBJ;
		goto end;
	}
	
	tcb->ctsk.exinf = stacd;
	
	kos_local_act_tsk_impl_nolock(tcb, KOS_FALSE);
end:
	kos_unlock;
	
	return er;
}

void kos_ext_tsk(void)
{
	kos_tcb_t *tcb;
	kos_uint_t actcnt;
	
	kos_lock;
	
	tcb = g_kos_cur_tcb;
	
	tcb->st.tskstat = KOS_TTS_DMT;
	kos_dbgprintf("tsk:%04X DMT\r\n", tcb->id);
	
	actcnt = tcb->st.actcnt;
	if(actcnt > 0) {
		actcnt--;
		tcb->st.actcnt = actcnt;
		kos_local_act_tsk_impl_nolock(tcb, KOS_FALSE);
	} else {
		kos_tsk_dsp();
	}
end:
	kos_unlock;
}

void kos_exd_tsk(void)
{
	kos_tcb_t *tcb;
	
	kos_lock;
	
	tcb = g_kos_cur_tcb;
	
	tcb->st.tskstat = KOS_TTS_DMT;
	kos_dbgprintf("tsk:%04X DMT\r\n", tcb->id);
	
	/* 登録解除 */
	g_kos_tcb[tcb->id - 1] = KOS_NULL;
#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
	kos_list_insert_prev(&g_kos_tcb_unused_list, (kos_list_t *)tcb);
#endif
	
	kos_tsk_dsp();
	
end:
	kos_unlock;
}

kos_er_t kos_ter_tsk(kos_id_t tskid)
{
}

kos_er_t kos_chg_pri(kos_id_t tskid, kos_pri_t tskpri)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	kos_pri_t cur_tskpri, new_tskpri;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
	if(tskpri > g_kos_max_pri)
		return KOS_E_PAR;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = KOS_E_NOEXS;
			goto end;
		}
		if(tcb->st.tskstat == KOS_TTS_DMT) {
			/* 休止状態ならオブジェクト状態エラーとする */
			er = KOS_E_ILUSE;
			goto end;
		}
	}
	
	if(tskpri == KOS_TPRI_INI) {
		tskpri = tcb->ctsk.itskpri;
	}
	
	/* ベース優先度を更新 */
	tcb->st.tskbpri = tskpri;
	
	/* 新しい現在優先度の決定 */
	new_tskpri = tskpri;
	cur_tskpri = tcb->st.tskpri;
	
	if(new_tskpri > cur_tskpri)
		new_tskpri = cur_tskpri;
	
	/* 現在優先度の更新 */
	tcb->st.tskpri = new_tskpri;
	
	/* 現在優先度が変わったまたは現在優先度がベース優先度と
		一致したときは追加処理を行う。*/
	if(new_tskpri != cur_tskpri || new_tskpri == tskpri) {
		
		if(tskid == KOS_TSK_SELF) {
			if(cur_tskpri <= new_tskpri) {
				/* 一旦RDY状態に変えてスケジューリング */
				kos_rdy_tsk_nolock(tcb);
				
				kos_tsk_dsp();
			} else {
				/* 優先度が高くなる場合は実行中のタスクは変化しないので何もしない */
			}
		} else {
			/* RDYキューの繋げ直し */
			if(tcb->st.tskstat == KOS_TTS_RDY) {
				kos_list_remove(&tcb->wait_list);
				kos_list_insert_prev(&g_kos_rdy_que[new_tskpri - 1],
					&tcb->wait_list);
			}
			
			/* 現在実行中のタスク(自タスク)より優先度が高くなっていればスケジューリング */
			if(g_kos_cur_tcb->st.tskpri > new_tskpri) {
				kos_tsk_dsp();
			}
		}
	}
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_get_pri(kos_id_t tskid, kos_pri_t *p_tskpri)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
	if(p_tskpri == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = KOS_E_NOEXS;
			goto end;
		}
		if(tcb->st.tskstat == KOS_TTS_DMT) {
			/* 休止状態ならオブジェクト状態エラーとする */
			er = KOS_E_ILUSE;
			goto end;
		}
	}
	
	*p_tskpri = tcb->st.tskpri;
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_ref_tsk(kos_id_t tskid, kos_rtsk_t *pk_rtsk)
{
	kos_tcb_t *tcb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
	if(pk_rtsk == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(tcb == KOS_NULL) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	*pk_rtsk = tcb->st;
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_ref_tst(kos_id_t tskid, kos_rtst_t *pk_rtst)
{
	kos_tcb_t *tcb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
	if(pk_rtst == KOS_NULL)
		return KOS_E_PAR;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(tcb == KOS_NULL) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	pk_rtst->tskstat = tcb->st.tskstat;
	pk_rtst->tskwait = tcb->st.tskwait;
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_tslp_tsk(kos_tmo_t tmout)
{
	kos_tcb_t *tcb;
	kos_er_t er;
	
	kos_lock;
	
	tcb = g_kos_cur_tcb;
	
	if(tcb->st.wupcnt > 0) {
		tcb->st.wupcnt--;
		er = KOS_E_OK;
	} else {
		if(tmout == KOS_TMO_POL) {
			er = KOS_E_TMOUT;
		} else {
			tcb->st.lefttmo = tmout;
			tcb->st.wobjid = 0;
			tcb->st.tskwait = KOS_TTW_SLP;
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

kos_er_t kos_wup_tsk(kos_id_t tskid)
{
	kos_tcb_t *tcb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(tcb == KOS_NULL) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	if((tcb->st.tskstat & KOS_TTS_WAI) &&
		tcb->st.tskwait == KOS_TTW_SLP)
	{
		kos_cancel_wait_nolock(tcb, KOS_E_OK);
		kos_tsk_dsp();
	} else if(tcb->st.tskstat == KOS_TTS_DMT) {
		er = KOS_E_OBJ;
		goto end;
	} else {
		if(tcb->st.wupcnt >= KOS_MAX_WUP_CNT) {
			er = KOS_E_QOVR;
			goto end;
		}
		tcb->st.wupcnt++;
	}
end:
	kos_unlock;
	
	return er;
}

#ifdef KOS_CFG_FAST_IRQ

static void kos_local_dly_svc_iwup_tsk(kos_vp_int_t arg0,
	kos_vp_int_t arg1, kos_vp_int_t arg2)
{
	kos_id_t tskid = (kos_id_t)arg0;
	kos_tcb_t *tcb;
	
	tcb = kos_get_tcb(tskid);
	if(tcb == KOS_NULL) {
		//er = KOS_E_NOEXS;
		goto end;
	}
	
	if((tcb->st.tskstat & KOS_TTS_WAI) &&
		tcb->st.tskwait == KOS_TTW_SLP)
	{
		kos_cancel_wait_nolock(tcb, KOS_E_OK);
		kos_ischedule_nolock();
	} else if(tcb->st.tskstat == KOS_TTS_DMT) {
		//er = KOS_E_OBJ;
		goto end;
	} else {
		if(tcb->st.wupcnt >= KOS_MAX_WUP_CNT) {
			//er = KOS_E_QOVR;
			goto end;
		}
		tcb->st.wupcnt++;
	}
end:
}

kos_er_t kos_iwup_tsk(kos_id_t tskid)
{
	kos_dly_svc_t *dly_svc;
	kos_uint_t wp;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	/* 非タスクコンテキストでは自タスクを指定できない。 */
	if(tskid == KOS_TSK_SELF || tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_ilock;
	
	if(g_kos_dly_svc_fifo_cnt >= g_kos_dly_svc_fifo_len) {
		er = KOS_E_QOVR;
		goto end;
	}
	
	wp = g_kos_dly_svc_fifo_wp;
	dly_svc = &g_kos_dly_svc_fifo[wp];
	
	dly_svc->fp = kos_local_dly_svc_iwup_tsk;
	dly_svc->arg[0] = (kos_vp_int_t)tskid;
	
	wp = wp + 1;
	if(wp >= g_kos_dly_svc_fifo_len) wp = 0;
	g_kos_dly_svc_fifo_wp = wp;
	g_kos_dly_svc_fifo_cnt++;
	
end:
	kos_iunlock;
	
	return er;
}
#else
kos_er_t kos_iwup_tsk(kos_id_t tskid)
{
	kos_tcb_t *tcb;
	kos_er_t er;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	/* 非タスクコンテキストでは自タスクを指定できない。 */
	if(tskid == KOS_TSK_SELF || tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_ilock;
	
	tcb = kos_get_tcb(tskid);
	if(tcb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	
	if((tcb->st.tskstat & KOS_TTS_WAI) &&
		tcb->st.tskwait == KOS_TTW_SLP)
	{
		kos_cancel_wait_nolock(tcb, KOS_E_OK);
		kos_itsk_dsp();
	} else if(tcb->st.tskstat == KOS_TTS_DMT) {
		er = KOS_E_OBJ;
		goto end;
	} else {
		if(tcb->st.wupcnt >= KOS_MAX_WUP_CNT) {
			er = KOS_E_QOVR;
			goto end;
		}
		tcb->st.wupcnt++;
	}
end:
	kos_iunlock;
	
	return er;
}
#endif

kos_er_t kos_rel_wai(kos_id_t tskid)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		/* 自タスクが指定されたときはE_OBJエラー。 */
		er = KOS_E_OBJ;
		goto end;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	/* 待ち状態または二重待ち状態でなければエラー */
	if(!(tcb->st.tskstat & KOS_TTS_WAI)) {
		er = KOS_E_OBJ;
		goto end;
	}
	
	kos_cancel_wait_nolock(tcb, KOS_E_RLWAI);
	kos_tsk_dsp();
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_irel_wai(kos_id_t tskid)
{
	kos_er_t er;
	kos_tcb_t *tcb;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_ilock;
	
	if(tskid == KOS_TSK_SELF) {
		/* 自タスクが指定されたときはE_OBJエラー。 */
		er = KOS_E_OBJ;
		goto end;
	} else {
		tcb = kos_get_tcb(tskid);
		if(!tcb) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	/* 待ち状態または二重待ち状態でなければエラー */
	if(!(tcb->st.tskstat & KOS_TTS_WAI)) {
		er = KOS_E_OBJ;
		goto end;
	}
	
	kos_cancel_wait_nolock(tcb, KOS_E_RLWAI);
	kos_itsk_dsp();
	
end:
	kos_iunlock;
	
	return er;
}

kos_er_t kos_sus_tsk(kos_id_t tskid)
{
	kos_er_t	er;
	kos_tcb_t	*tcb;
	kos_stat_t	tskstat;
	kos_uint_t	suscnt;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	
	er = KOS_E_OK;
	
	kos_lock;
	
	if(tskid == KOS_TSK_SELF) {
		/* 自タスク指定のときディスパッチ禁止状態ならエラー */
		if(g_kos_dsp) {
			er = KOS_E_CTX;
			goto end;
		}
		tcb = g_kos_cur_tcb;
	} else {
		tcb = kos_get_tcb(tskid);
		if(tcb == KOS_NULL) {
			er = KOS_E_NOEXS;
			goto end;
		}
	}
	
	tskstat = tcb->st.tskstat;
	
	/* すでに休止状態ならエラーとする */
	if(tskstat == KOS_TTS_DMT) {
		return KOS_E_OBJ;
	}
	
	/* キューイング数を加算 */
	suscnt = tcb->st.suscnt;
	if(suscnt >= KOS_MAX_SUS_CNT) {
		er = KOS_E_QOVR;
		goto end;
	}
	tcb->st.suscnt = suscnt + 1;

	/* タスク状態変更 */
	if(tskstat & KOS_TTS_WAI) {
		tcb->st.tskstat = KOS_TTS_WAS;
	} else {
		tcb->st.tskstat = KOS_TTS_SUS;
	}
	
	if(tskstat == KOS_TTS_RDY) {
		/* 実行可能状態ならRDYキューから削除 */
		kos_list_remove((kos_list_t *)tcb);
	} else if(tskstat == KOS_TTS_RUN) {
		/* 実行状態ならスケジューリング */
		kos_tsk_dsp();
	}
end:
	kos_unlock;
	
	return er;
}


kos_er_t kos_rsm_tsk(kos_id_t tskid)
{
	kos_er_t	er;
	kos_tcb_t	*tcb;
	kos_stat_t	tskstat;
	kos_uint_t	suscnt;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	/* 自タスク指定はエラー */
	if(tskid == KOS_TSK_SELF) {
		return KOS_E_OBJ;
	}
	
	er = KOS_E_OK;
	
	kos_lock;
	
	tcb = kos_get_tcb(tskid);
	if(tcb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	/* 自タスク指定はエラー */
	if(tcb == g_kos_cur_tcb) {
		er = KOS_E_OBJ;
		goto end;
	}
	
	/* キューイング数を減算 */
	suscnt = tcb->st.suscnt;
	if(suscnt == 0) {
		/* 0なら強制待ち状態ではないのでエラー */
		er = KOS_E_OBJ;
		goto end;
	}
	suscnt--;
	tcb->st.suscnt = suscnt;
	
	if(suscnt == 0) {
		tskstat = tcb->st.tskstat;
		if(tskstat == KOS_TTS_WAS) {
			tcb->st.tskstat = KOS_TTS_WAI;
		} else {
			kos_rdy_tsk_nolock(tcb);
			kos_tsk_dsp();
		}
	}
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_frsm_tsk(kos_id_t tskid)
{
	kos_er_t	er;
	kos_tcb_t	*tcb;
	kos_stat_t	tskstat;
	kos_uint_t	suscnt;
	
#ifdef KOS_CFG_ENA_PAR_CHK
	if(tskid > g_kos_max_tsk)
		return KOS_E_ID;
#endif
	/* 自タスク指定はエラー */
	if(tskid == KOS_TSK_SELF) {
		return KOS_E_OBJ;
	}
	
	er = KOS_E_OK;
	
	kos_lock;
	
	tcb = kos_get_tcb(tskid);
	if(tcb == KOS_NULL) {
		er = KOS_E_NOEXS;
		goto end;
	}
	/* 自タスク指定はエラー */
	if(tcb == g_kos_cur_tcb) {
		er = KOS_E_OBJ;
		goto end;
	}
	
	/* キューイング数を減算 */
	suscnt = tcb->st.suscnt;
	if(suscnt == 0) {
		/* 0なら強制待ち状態ではないのでエラー */
		er = KOS_E_OBJ;
		goto end;
	}
	
	tcb->st.suscnt = 0;
	
	tskstat = tcb->st.tskstat;
	if(tskstat == KOS_TTS_WAS) {
		tcb->st.tskstat = KOS_TTS_WAI;
	} else {
		kos_rdy_tsk_nolock(tcb);
		kos_tsk_dsp();
	}
	
end:
	kos_unlock;
	
	return er;
}

kos_er_t kos_dly_tsk(kos_reltim_t dlytim)
{
	kos_tcb_t *tcb;
	kos_er_t er;
	
	/* 待ちに移行させるために1にする */
	if(dlytim == 0) {
		dlytim = 1;
	}
	
	kos_lock;
	
	tcb = g_kos_cur_tcb;
	
	tcb->st.lefttmo = dlytim;
	tcb->st.wobjid = 0;
	tcb->st.tskwait = KOS_TTW_DLY;
	
	kos_wait_nolock(tcb);
	
	kos_unlock;
	
	return tcb->rel_wai_er;
}
