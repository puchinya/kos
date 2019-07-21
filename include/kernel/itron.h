/*!
 *	@file	itron.h
 *	@brief	uItron 4.0 API's header.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#ifndef __ITRON_H__
#define __ITRON_H__

#include "kos.h"

#define TMIN_TPRI		KOS_TMIN_TPRI
#define TMAX_TPRI		KOS_TMAX_TPRI

#define TMIN_TINTPRI	KOS_TMIN_TINTPRI
#define TMAX_TINTPRI	KOS_TMAX_TINTPRI

#define TMAX_ACTCNT		KOS_MAX_ACT_CNT
#define TMAX_WUPCNT		KOS_MAX_WUP_CNT
#define TMAX_SUSCNT		KOS_MAX_SUS_CNT

#define TBIT_TEXPTN		KOS_TBIT_TEXPTN
#define TBIT_FLGPTN		KOS_TBIT_FLGPTN
#define TBIT_RDVPTN		KOS_TBIT_RDVPTN

#define TIC_NUME		KOS_TIC_NUME
#define TIC_DENO		KOS_TIC_DENO

#define VP		kos_vp_t
#define FP		kos_fp_t
#define INT		kos_int_t
#define UINT	kos_uint_t
#define BOOL	kos_bool_t
#define FN		kos_fn_t
#define ID		kos_id_t
#define ATR		kos_atr_t
#define STAT	kos_stat_t
#define MODE	kos_mode_t
#define PRI		kos_pri_t
#define INTPRI	kos_intpri_t
#define SIZE	kos_size_t
#define TMO		kos_tmo_t
#define VP_INT	kos_vp_int_t
#define ER		kos_er_t
#define ER_BOOL	kos_er_bool_t
#define ER_ID	kos_er_id_t
#define ER_UINT	kos_er_uint_t

#define FLGPTN	kos_flgptn_t
#define SYSTIM	kos_systim_t
#define RELTIM	kos_reltim_t

#define NULL	KOS_NULL
#define TRUE	KOS_TRUE
#define FALSE	KOS_FALSE

#define E_OK	KOS_E_OK
#define E_SYS	KOS_E_SYS
#define E_NOSPT	KOS_E_NOSPT
#define E_RSFN	KOS_E_RSFN
#define E_RSATR	KOS_E_RSATR
#define E_PAR	KOS_E_PAR
#define E_ID	KOS_E_ID
#define E_CTX	KOS_E_CTX
#define E_MACV	KOS_E_MACV
#define E_OACV	KOS_E_OACV
#define E_ILUSE	KOS_E_ILUSE
#define E_NOMEM	KOS_E_NOMEM
#define E_NOID	KOS_E_NOID
#define E_OBJ	KOS_E_OBJ
#define E_NOEXS	KOS_E_NOEXS
#define E_QOVR	KOS_E_QOVR
#define E_RLWAI	KOS_E_RLWAI
#define E_TMOUT	KOS_E_TMOUT
#define E_DLT	KOS_E_DLT
#define E_CLS	KOS_E_CLS
#define E_WBLK	KOS_E_WBLK
#define E_BOVR	KOS_E_BOVR

#define TA_NULL		KOS_TA_NULL
#define TA_HLNG		KOS_TA_HLNG
#define TA_ASM		KOS_TA_ASM
#define TA_TFIFO	KOS_TA_TFIFO
#define TA_TPRI		KOS_TA_TPRI
#define TA_ACT		KOS_TA_ACT
#define TA_WSGL		KOS_TA_WSGL
#define TA_WMUL		KOS_TA_WMUL
#define TA_CLR		KOS_TA_CLR
#define TA_STA		KOS_TA_STA
#define TA_PHS		KOS_TA_PHS

#define TMO_POL		KOS_TMO_POL
#define TMO_FEVR	KOS_TMO_FEVR
#define TMO_NBLK	KOS_TMO_NBLK

#define TSK_SELF	KOS_TSK_SELF
#define TSK_NONE	KOS_TSK_NONE
#define TPRI_SELF	KOS_TPRI_SELF
#define TPRI_INI	KOS_TPRI_INI

#define TTS_RUN		KOS_TTS_RUN
#define TTS_RDY		KOS_TTS_RDY
#define TTS_WAI		KOS_TTS_WAI
#define TTS_SUS		KOS_TTS_SUS
#define TTS_WAS		KOS_TTS_WAS
#define TTS_DMT		KOS_TTS_DMT

#define TTW_SLP		KOS_TTW_SLP
#define TTW_DLY		KOS_TTW_DLY
#define TTW_SEM		KOS_TTW_SEM
#define TTW_FLG		KOS_TTW_FLG
#define TTW_SDTQ	KOS_TTW_SDTQ
#define TTW_RDTQ	KOS_TTW_RDTQ
#define TTW_MBX		KOS_TTW_MBX

#define TWF_ANDW	KOS_TWF_ANDW
#define TWF_ORW		KOS_TWF_ORW

#define TCYC_STP	KOS_TCYC_STP
#define TCYC_STA	KOS_TCYC_STA

#define T_CTSK		kos_ctsk_t
#define T_RTSK		kos_rtsk_t
#define INTNO		kos_intno_t
#define T_DINH		kos_dinh_t
#define T_RSYS		kos_rsys_t
#define T_CSEM		kos_csem_t
#define T_RSEM		kos_rsem_t
#define T_CFLG		kos_cflg_t
#define T_RFLG		kos_rflg_t
#define T_CDTQ		kos_cdtq_t
#define T_RDTQ		kos_rdtq_t
#define T_CCYC		kos_ccyc_t
#define T_RCYC		kos_rcyc_t

#define acre_tsk	kos_cre_tsk
#define del_tsk		kos_del_tsk
#define act_tsk		kos_act_tsk
#define iact_tsk	kos_iact_tsk
#define can_tsk		kos_can_tsk
#define sta_tsk		kos_sta_tsk
#define ext_tsk		kos_ext_tsk
#define exd_tsk		kos_exd_tsk
#define ter_tsk		kos_ter_tsk
#define chg_pri		kos_chg_pri
#define get_pri		kos_get_pri
#define ref_tsk		kos_ref_tsk
#define ret_tsk		kos_ret_tsk

#define slp_tsk		kos_slp_tsk
#define tslp_tsk	kos_tslp_tsk
#define wup_tsk		kos_wup_tsk
#define iwup_tsk	kos_iwup_tsk
#define rel_tsk		kos_rel_tsk
#define irel_tsk	kos_irel_tsk
#define sus_tsk		kos_sus_tsk
#define rsm_tsk		kos_rsm_tsk
#define frsm_tsk	kos_frsm_tsk
#define dly_tsk		kos_dly_tsk

#define acre_sem	kos_cre_sem
#define del_sem		kos_del_sem
#define sig_sem		kos_sig_sem
#define isig_sem	kos_isig_sem
#define twai_sem	kos_twai_sem
#define wai_sem		kos_wai_sem
#define pol_sem		kos_pol_sem
#define ref_sem		kos_ref_sem

#define acre_flg	kos_cre_flg
#define del_flg		kos_del_flg
#define set_flg		kos_set_flg
#define iset_flg	kos_iset_flg
#define clr_flg		kos_clr_flg
#define twai_flg	kos_twai_flg
#define wai_flg		kos_wai_flg
#define pol_flg		kos_pol_flg
#define ref_flg		kos_ref_flg

#define acre_dtq	kos_cre_dtq
#define del_dtq		kos_del_dtq
#define tsnd_dtq	kos_tsnd_dtq
#define snd_dtq		kos_snd_dtq
#define psnd_dtq	kos_psnd_dtq
#define ipsnd_dtq	kos_ipsnd_dtq
#define fsnd_dtq	kos_fsnd_dtq
#define ifsnd_dtq	kos_ifsnd_dtq
#define trcv_dtq	kos_trcv_dtq
#define rcv_dtq		kos_rcv_dtq
#define prcv_dtq	kos_prcv_dtq
#define ref_dtq		kos_ref_dtq

#define set_tim		kos_set_tim
#define get_tim		kos_get_tim

#define acre_cyc	kos_cre_cyc
#define del_cyc		kos_del_cyc
#define sta_cyc		kos_sta_cyc
#define stp_cyc		kos_stp_cyc
#define ref_cyc		kos_ref_cyc


#define acre_mtx	kos_cre_mtx
#define del_mtx		kos_del_mtx
#define loc_mtx		kos_loc_mtx
#define iloc_mtx	kos_iloc_mtx
#define ploc_mtx	kos_ploc_mtx

#define rot_rdq		kos_rot_rdq
#define irot_rdq	kos_irot_rdq
#define get_tid		kos_get_tid
#define iget_tid	kos_iget_tid
#define loc_cpu		kos_loc_cpu
#define iloc_cpu	kos_iloc_cpu
#define unl_cpu		kos_unl_cpu
#define iunl_cpu	kos_iunl_cpu
#define dis_dsp		kos_dis_dsp
#define ena_dsp		kos_ena_dsp
#define sns_ctx		kos_sns_ctx
#define sns_loc		kos_sns_loc
#define sns_dsp		kos_sns_dsp
#define sns_dqn		kos_sns_dqn

#define def_inh		kos_def_inh
#define dis_int		kos_dis_int
#define ena_int		kos_ena_int
#define vchg_intpri	kos_vchg_intpri
#define vget_intpri	kos_vget_intpri
#define chg_imsk	kos_chg_imsk
#define get_imsk	kos_get_imsk

#endif
