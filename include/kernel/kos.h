/*!
 *	@file	kos.h
 *	@brief	KOS API's header.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#ifndef __KOS_H__
#define __KOS_H__

#include "kos_usr_cfg.h"
#include "kos_sys_cfg.h"
#include "kos_arch_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

#if __STDC_VERSION__ >= 199901L
#  define KOS_INLINE	inline
#else
#  define KOS_INLINE	__inline
#endif

typedef void 			*kos_vp_t;
typedef void 			*kos_fp_t;
typedef int 			kos_int_t;
typedef unsigned int 	kos_uint_t;
typedef int 			kos_bool_t;
typedef int 			kos_fn_t;
typedef int 			kos_er_t;
typedef int 			kos_id_t;
typedef unsigned int	kos_atr_t;
typedef unsigned int	kos_stat_t;
typedef unsigned int	kos_mode_t;
typedef unsigned int	kos_pri_t;
typedef unsigned int	kos_size_t;
typedef int				kos_tmo_t;
typedef kos_vp_t		kos_vp_int_t;
typedef int				kos_er_bool_t;
typedef int				kos_er_id_t;
typedef unsigned int	kos_er_uint_t;

typedef unsigned int	kos_flgptn_t;
typedef unsigned int	kos_systim_t;
typedef unsigned int	kos_reltim_t;

#define KOS_NULL		0
#define KOS_TRUE		1
#define KOS_FALSE		0
#define KOS_E_OK		0

#define KOS_E_SYS		-5

#define KOS_E_NOSPT		-9
#define KOS_E_RSFN		-10
#define KOS_E_RSATR		-11
#define KOS_E_PAR		-17
#define KOS_E_ID		-18

#define KOS_E_CTX		-25
#define KOS_E_MACV		-26
#define KOS_E_OACV		-27
#define KOS_E_ILUSE		-28

#define KOS_E_NOMEM		-33
#define KOS_E_NOID		-34

#define KOS_E_OBJ		-41
#define KOS_E_NOEXS		-42
#define KOS_E_QOVR		-43

#define KOS_E_RLWAI		-49
#define KOS_E_TMOUT		-50
#define KOS_E_DLT		-51
#define KOS_E_CLS		-52

#define KOS_E_WBLK		-57
#define KOS_E_BOVR		-58

/* attributes */
#define KOS_TA_NULL		0
#define KOS_TA_HLNG		0x00
#define KOS_TA_ASM		0x01
#define KOS_TA_TFIFO	0x00
#define KOS_TA_TPRI		0x01
#define KOS_TA_ACT		0x02

#define KOS_TMO_POL		0
#define KOS_TMO_FEVR	-1
#define KOS_TMO_NBLK	-2

#define KOS_TSK_SELF	0
#define KOS_TSK_NONE	0
#define KOS_TPRI_SELF	0
#define KOS_TPRI_INI	0

/* task stat */
#define KOS_TTS_RUN		0x01
#define KOS_TTS_RDY		0x02
#define KOS_TTS_WAI		0x04
#define KOS_TTS_SUS		0x08
#define KOS_TTS_WAS		0x0c
#define KOS_TTS_DMT		0x10

/* task wait factor */
#define KOS_TTW_SLP		0x0001
#define KOS_TTW_DLY		0x0002
#define KOS_TTW_SEM		0x0004
#define KOS_TTW_FLG		0x0008
#define KOS_TTW_SDTQ	0x0010
#define KOS_TTW_RDTQ	0x0020
#define KOS_TTW_MBX		0x0080

typedef struct {
	kos_atr_t		tskatr;
	kos_vp_int_t	exinf;
	kos_fp_t		task;
	kos_pri_t		itskpri;
	kos_size_t		stksz;
	kos_vp_t		stk;
} kos_ctsk_t;

typedef struct {
	kos_stat_t	tskstat;
	kos_pri_t	tskpri;
	kos_pri_t	tskbpri;
	kos_stat_t	tskwait;
	kos_id_t	wobjid;
	kos_tmo_t	lefttmo;
	kos_uint_t	actcnt;
	kos_uint_t	wupcnt;
	kos_uint_t	suscnt;
} kos_rtsk_t;

typedef struct {
	kos_stat_t	tskstat;
	kos_stat_t	tskwait;
} kos_rtst_t;

typedef struct {
	kos_id_t		tid;
	kos_bool_t		ctx;
	kos_bool_t		loc;
	kos_bool_t		dsp;
	kos_bool_t		dpn;
} kos_rsys_t;

/* タスク管理機能 */
kos_er_id_t kos_cre_tsk(const kos_ctsk_t *ctsk) __attribute__((__nonnull__(1)));
kos_er_t kos_del_tsk(kos_id_t tskid);
kos_er_t kos_act_tsk(kos_id_t tskid);
kos_er_t kos_iact_tsk(kos_id_t tskid);
kos_er_uint_t kos_can_act(kos_id_t tskid);
kos_er_t kos_sta_tsk(kos_id_t tskid, kos_vp_int_t stacd);
void kos_ext_tsk(void);
void kos_exd_tsk(void);
kos_er_t kos_ter_tsk(kos_id_t tskid);
kos_er_t kos_chg_pri(kos_id_t tskid, kos_pri_t tskpri);
kos_er_t kos_get_pri(kos_id_t tskid, kos_pri_t *p_tskpri) __attribute__((__nonnull__(2)));
kos_er_t kos_ref_tsk(kos_id_t tskid, kos_rtsk_t *pk_rtsk);
kos_er_t kos_ref_tst(kos_id_t tskid, kos_rtst_t *pk_rtst);

kos_er_t kos_tslp_tsk(kos_tmo_t tmout);
static KOS_INLINE kos_er_t kos_slp_tsk(void) { return kos_tslp_tsk(KOS_TMO_FEVR); }
kos_er_t kos_wup_tsk(kos_id_t tskid);
kos_er_t kos_iwup_tsk(kos_id_t tskid);
kos_er_t kos_rel_tsk(kos_id_t tskid);
kos_er_t kos_irel_tsk(kos_id_t tskid);
kos_er_t kos_sus_tsk(kos_id_t tskid);
kos_er_t kos_rsm_tsk(kos_id_t tskid);
kos_er_t kos_frsm_tsk(kos_id_t tskid);
kos_er_t kos_dly_tsk(kos_reltim_t dlytim);

/* 同期・通信機能/セマフォ */
#ifdef KOS_CFG_SPT_SEM
typedef struct {
	kos_atr_t	sematr;
	kos_uint_t	isemcnt;
	kos_uint_t	maxsem;
} kos_csem_t;

typedef struct {
	kos_id_t	wtskid;
	kos_uint_t	semcnt;
} kos_rsem_t;

kos_er_id_t kos_cre_sem(const kos_csem_t *pk_csem);
kos_er_t kos_del_sem(kos_id_t semid);
kos_er_t kos_sig_sem(kos_id_t semid);
kos_er_t kos_isig_sem(kos_id_t semid);
kos_er_t kos_twai_sem(kos_id_t semid, kos_tmo_t tmout);
static KOS_INLINE kos_er_t kos_wai_sem(kos_id_t semid) { return kos_twai_sem(semid, KOS_TMO_FEVR); }
static KOS_INLINE kos_er_t kos_pol_sem(kos_id_t semid) { return kos_twai_sem(semid, KOS_TMO_POL); }
kos_er_t kos_ref_sem(kos_id_t semid, kos_rsem_t *pk_rsem);
#endif

/* 同期・通信機能/イベントフラグ */
#ifdef KOS_CFG_SPT_FLG

#define KOS_TA_WSGL		0x00
#ifdef KOS_CFG_SPT_FLG_WMUL
#define KOS_TA_WMUL		0x02
#endif
#define KOS_TA_CLR		0x04

#define KOS_TWF_ANDW	0x00
#define KOS_TWF_ORW		0x01

typedef struct {
	kos_atr_t		flgatr;
	kos_flgptn_t	iflgptn;
} kos_cflg_t;

typedef struct {
	kos_id_t		wtskid;
	kos_flgptn_t	flgptn;
} kos_rflg_t;

kos_er_id_t kos_cre_flg(const kos_cflg_t *pk_cflg);
kos_er_t kos_del_flg(kos_id_t flgid);
kos_er_t kos_set_flg(kos_id_t flgid, kos_flgptn_t setptn);
kos_er_t kos_iset_flg(kos_id_t flgid, kos_flgptn_t setptn);
kos_er_t kos_clr_flg(kos_id_t flgid, kos_flgptn_t clrptn);
kos_er_t kos_twai_flg(kos_id_t flgid, kos_flgptn_t waiptn,
	kos_mode_t wfmode, kos_flgptn_t *p_flgptn, kos_tmo_t tmout);
static KOS_INLINE kos_er_t kos_wai_flg(kos_id_t flgid, kos_flgptn_t waiptn,
	kos_mode_t wfmode, kos_flgptn_t *p_flgptn) { return kos_twai_flg(flgid, waiptn, wfmode, p_flgptn, KOS_TMO_FEVR); }
static KOS_INLINE kos_er_t kos_pol_flg(kos_id_t flgid, kos_flgptn_t waiptn,
	kos_mode_t wfmode, kos_flgptn_t *p_flgptn) { return kos_twai_flg(flgid, waiptn, wfmode, p_flgptn, KOS_TMO_POL); }
kos_er_t kos_ref_flg(kos_id_t flgid, kos_rflg_t *pk_rflg);

#endif
	
/* dtq */
#ifdef KOS_CFG_SPT_DTQ

typedef struct {
	kos_atr_t		dtqatr;
	kos_uint_t		dtqcnt;
	kos_vp_t		dtq;
} kos_cdtq_t;

typedef struct {
	kos_id_t		stskid;
	kos_id_t		rtskid;
	kos_uint_t		sdtqcnt;
} kos_rdtq_t;

kos_er_id_t kos_cre_dtq(const kos_cdtq_t *pk_cdtq);
kos_er_t kos_del_dtq(kos_id_t dtqid);
kos_er_t kos_tsnd_dtq(kos_id_t dtqid, kos_vp_int_t data, kos_tmo_t tmout);
static KOS_INLINE kos_er_t kos_snd_dtq(kos_id_t dtqid, kos_vp_int_t data) { return kos_tsnd_dtq(dtqid, data, KOS_TMO_FEVR); }
static KOS_INLINE kos_er_t kos_psnd_dtq(kos_id_t dtqid, kos_vp_int_t data) { return kos_tsnd_dtq(dtqid, data, KOS_TMO_POL); }
kos_er_t kos_ipsnd_dtq(kos_id_t dtqid, kos_vp_int_t data);
kos_er_t kos_fsnd_dtq(kos_id_t dtqid, kos_vp_int_t data);
kos_er_t kos_ifsnd_dtq(kos_id_t dtqid, kos_vp_int_t data);
kos_er_t kos_trcv_dtq(kos_id_t dtqid, kos_vp_int_t *p_data, kos_tmo_t tmout);
static KOS_INLINE kos_er_t kos_rcv_dtq(kos_id_t dtqid, kos_vp_int_t *p_data) { return kos_trcv_dtq(dtqid, p_data, KOS_TMO_FEVR); }
static KOS_INLINE kos_er_t kos_prcv_dtq(kos_id_t dtqid, kos_vp_int_t *p_data) { return kos_trcv_dtq(dtqid, p_data, KOS_TMO_POL); }
kos_er_t kos_ref_dtq(kos_id_t dtqid, kos_rdtq_t *pk_rdtq);

#endif

/* mbx */


/* other */

/* 時間管理機能/システム時刻管理 */
kos_er_t kos_set_tim(kos_systim_t *p_systim) __attribute__((__nonnull__(1)));
kos_er_t kos_get_tim(kos_systim_t *p_systim) __attribute__((__nonnull__(1)));
kos_er_t kos_isig_tim(void); /* 外部から呼ぶ必要なし */

/* 時間管理機能/周期ハンドラ */
#ifdef KOS_CFG_SPT_CYC

#define KOS_TA_STA		0x02
#define KOS_TA_PHS		0x04

#define KOS_TCYC_STP	0x00
#define KOS_TCYC_STA	0x01

typedef struct {
	kos_atr_t		cycatr;
	kos_vp_int_t	exinf;
	kos_fp_t		cychdr;
	kos_reltim_t	cyctim;
	kos_reltim_t	cycphs;
} kos_ccyc_t;

typedef struct {
	kos_stat_t		cycstat;
	kos_reltim_t	lefttim;
} kos_rcyc_t;

kos_er_t kos_cre_cyc(const kos_ccyc_t *pk_ccyc);
kos_er_t kos_del_cyc(kos_id_t cycid);
kos_er_t kos_sta_cyc(kos_id_t cycid);
kos_er_t kos_stp_cyc(kos_id_t cycid);
kos_er_t kos_ref_cyc(kos_id_t cycid, kos_rcyc_t *pk_rcyc);

#endif

/* システム状態管理機能 */
kos_er_t kos_rot_rdq(kos_pri_t tskpri);
kos_er_t kos_irot_rdq(kos_pri_t tskpri);
kos_er_t kos_get_tid(kos_id_t *p_tskid) __attribute__((__nonnull__(1)));
kos_er_t kos_iget_tid(kos_id_t *p_tskid) __attribute__((__nonnull__(1)));
kos_er_t kos_loc_cpu(void);
kos_er_t kos_iloc_cpu(void);
kos_er_t kos_unl_cpu(void);
kos_er_t kos_iunl_cpu(void);
kos_er_t kos_dis_dsp(void);
kos_er_t kos_ena_dsp(void);
kos_bool_t kos_sns_ctx(void);
kos_bool_t kos_sns_loc(void);
kos_bool_t kos_sns_dsp(void);
kos_bool_t kos_sns_dpn(void);

/* 割り込み管理機能 */

typedef unsigned int kos_intno_t;
typedef unsigned int kos_intpri_t;

typedef struct {
	kos_atr_t		inhatr;	/* interrupt attribute. */
	kos_fp_t		inthdr;	/* interrupt handler. */
	kos_vp_int_t	exinf;	/* interrupt handler's param. */
#ifdef KOS_ARCH_CFG_SPT_IRQPRI
	kos_intpri_t	intpri;	/* interrupt priotiy. */
#endif
} kos_dinh_t;


kos_er_t kos_def_inh(kos_intno_t intno, const kos_dinh_t *pk_dinh);
kos_er_t kos_dis_int(kos_intno_t intno);
kos_er_t kos_ena_int(kos_intno_t intno);
#ifdef KOS_ARCH_CFG_SPT_IRQPRI
kos_er_t kos_vchg_intpri(kos_intno_t intno, kos_intpri_t intpri);
kos_er_t kos_vget_intpri(kos_intno_t intno, kos_intpri_t *p_intpri);
kos_er_t kos_chg_imsk(kos_intpri_t imsk);
kos_er_t kos_get_imsk(kos_intpri_t *p_imsk);
#endif

#ifdef __cplusplus
};
#endif

#endif
