/*!
 *	@file	kos_local.h
 *	@brief	local definitions header file.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#ifndef __KOS_LOCAL_H__
#define __KOS_LOCAL_H__

#include <stddef.h>
#include "kos.h"
#include "kos_arch.h"

#define KOS_LOCK_PORT_HI()	//GPIOB->BSRRL = 0x0080
#define KOS_LOCK_PORT_LO()	//GPIOB->BSRRH = 0x0080
#define KOS_SCHD_PORT_HI()	//GPIOB->BSRRL = 0x0100
#define KOS_SCHD_PORT_LO()	//GPIOB->BSRRH = 0x0100


#ifdef KOS_CFG_FAST_IRQ
#define kos_lock	kos_arch_mask_pendsv()
#define kos_unlock	kos_arch_unmask_pendsv()
#else
#define kos_lock	do { kos_arch_loc_cpu(); KOS_LOCK_PORT_HI(); } while(0)
#define kos_unlock	do { KOS_LOCK_PORT_LO(); kos_arch_unl_cpu(); } while(0)
#endif
#define kos_ilock	do { kos_arch_loc_cpu(); KOS_LOCK_PORT_HI(); } while(0)
#define kos_iunlock	do { KOS_LOCK_PORT_LO(); kos_arch_unl_cpu(); } while(0)

#define KOS_ARRAY_LEN(n)	(sizeof(n)/sizeof((n)[0]));

typedef struct kos_list_t kos_list_t;
typedef union kos_tcb_wait_info_t kos_tcb_wait_info_t;
typedef struct kos_tcb_t kos_tcb_t;
typedef struct kos_sem_cb_t kos_sem_cb_t;
typedef struct kos_flg_cb_t kos_flg_cb_t;
typedef struct kos_dtq_cb_t kos_dtq_cb_t;
typedef struct kos_cyc_cb_t kos_cyc_cb_t;

struct kos_list_t {
	kos_list_t	*next, *prev;
};

typedef struct {
	kos_mode_t		wfmode;
	kos_flgptn_t	waiptn;
	kos_flgptn_t	*relptn;	/* 待ち解除されたときのビットパターン */
} kos_flg_wait_exinf_t;

struct kos_tcb_t {
	kos_list_t			wait_list;	/* RDYキューまたは待機リストに対してつなぐリスト */
	kos_list_t			tmo_list;	/* タイムアウトのためにつなぐリスト */
	void				*sp;		/* current stack pointer */
	kos_vp_t			sp_top;
	kos_id_t			id;			/* task id */
	kos_ctsk_t			ctsk;		/* task create parameters */
	kos_rtsk_t			st;
	kos_vp_t			wait_exinf;	/* 待ち状態のときに格納する拡張情報(同期・通信API実装で使用) */
	kos_er_t			rel_wai_er;
};

struct kos_sem_cb_t {
	kos_csem_t			csem;			/* semaphore create parameters */
	kos_uint_t			semcnt;			/* semaphore counter */
	kos_list_t			wait_tsk_list;	/* wait task list */
};

struct kos_flg_cb_t {
	kos_cflg_t			cflg;
	kos_flgptn_t		flgptn;
	kos_list_t			wait_tsk_list;
#ifndef KOS_CFG_SPT_FLG_WMUL
	kos_mode_t			wfmode;
	kos_mode_t			waiptn;
	kos_flgptn_t		*relptn;
#endif
};

struct kos_dtq_cb_t {
	kos_cdtq_t			cdtq;
	kos_uint_t			dtq_rp;
	kos_uint_t			dtq_wp;
	kos_uint_t			sdtqcnt;
	kos_list_t			r_wait_tsk_list;
	kos_list_t			s_wait_tsk_list;
};

struct kos_cyc_cb_t {
	kos_list_t			list;
	kos_ccyc_t			ccyc;
	kos_rcyc_t			st;
};

extern kos_tcb_t 	*g_kos_cur_tcb;
extern kos_list_t	g_kos_rdy_que[];
extern kos_dinh_t	g_kos_dinh_list[];

extern kos_tcb_t	*g_kos_tcb[];
extern kos_sem_cb_t	*g_kos_sem_cb[];
extern kos_flg_cb_t	*g_kos_flg_cb[];
extern kos_dtq_cb_t	*g_kos_dtq_cb[];
extern kos_cyc_cb_t	*g_kos_cyc_cb[];

extern kos_tcb_t	g_kos_tcb_inst[];
extern kos_sem_cb_t	g_kos_sem_cb_inst[];
extern kos_flg_cb_t	g_kos_flg_cb_inst[];
extern kos_dtq_cb_t	g_kos_dtq_cb_inst[];
extern kos_cyc_cb_t	g_kos_cyc_cb_inst[];
extern kos_tcb_t	g_kos_idle_tcb_inst;

#ifdef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
extern kos_list_t	g_kos_tcb_unused_list;
extern kos_id_t		g_kos_last_tskid;
#ifdef KOS_CFG_SPT_SEM
extern kos_list_t	g_kos_sem_cb_unused_list;
extern kos_id_t		g_kos_last_semid;
#endif
#ifdef KOS_CFG_SPT_FLG
extern kos_list_t	g_kos_flg_cb_unused_list;
extern kos_id_t		g_kos_last_flgid;
#endif
#ifdef KOS_CFG_SPT_DTQ
extern kos_list_t	g_kos_dtq_cb_unused_list;
extern kos_id_t		g_kos_last_dtqid;
#endif
#ifdef KOS_CFG_SPT_CYC
extern kos_list_t	g_kos_cyc_cb_unused_list;
extern kos_id_t		g_kos_last_cycid;
#endif
#endif

extern kos_uint_t	g_kos_isr_stk[];

extern kos_list_t	g_kos_tmo_wait_list;
extern kos_bool_t	g_kos_dsp;
extern kos_bool_t	g_kos_pend_dsp;
extern kos_systim_t g_kos_systim;

extern const kos_uint_t g_kos_max_tsk;
extern const kos_uint_t g_kos_max_sem;
extern const kos_uint_t g_kos_max_flg;
extern const kos_uint_t g_kos_max_dtq;
extern const kos_uint_t g_kos_max_cyc;
extern const kos_uint_t g_kos_max_pri;
extern const kos_uint_t g_kos_max_intno;
extern const kos_uint_t	g_kos_isr_stksz;

#ifdef KOS_CFG_FAST_IRQ

typedef void (*kos_dly_svc_fp_t)(kos_vp_int_t arg0, kos_vp_int_t arg1, kos_vp_int_t arg2);
typedef struct {
	kos_dly_svc_fp_t	fp;
	kos_vp_int_t		arg[3];
} kos_dly_svc_t;
extern kos_dly_svc_t g_kos_dly_svc_fifo[];
extern kos_uint_t g_kos_dly_svc_fifo_len;
extern volatile kos_uint_t g_kos_dly_svc_fifo_cnt;
extern volatile kos_uint_t g_kos_dly_svc_fifo_wp;
extern volatile kos_uint_t g_kos_dly_svc_fifo_rp;

void kos_local_process_dly_svc(void);
#endif

static KOS_INLINE void kos_list_init(kos_list_t *l)
{
	l->next = l;
	l->prev = l;
}

static KOS_INLINE void kos_list_insert_next(kos_list_t *l, kos_list_t *n)
{
	l->next->prev = n;
	n->next = l->next;
	n->prev = l;
	l->next = n;
}

static KOS_INLINE void kos_list_insert_prev(kos_list_t *l, kos_list_t *n)
{
	l->prev->next = n;
	n->prev = l->prev;
	n->next = l;
	l->prev = n;
}

static KOS_INLINE void kos_list_remove(kos_list_t *l)
{
	l->prev->next = l->next;
	l->next->prev = l->prev;
}

static KOS_INLINE int kos_list_empty(kos_list_t *l)
{
	return l == l->next;
}


/* ディスパッチャ起動要求を設定します */
#ifdef KOS_ARCH_CFG_SPT_PENDSV
#define kos_tsk_dsp()		if(g_kos_dsp) { g_kos_pend_dsp = KOS_TRUE; } else { kos_arch_pend_sv(); }
#define kos_force_tsk_dsp()	kos_arch_pend_sv()
#define kos_itsk_dsp()		kos_tsk_dsp()
#else
#define kos_tsk_dsp()
#define kos_itsk_dsp()
#endif

#ifndef KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH
kos_int_t kos_find_null(void **a, int len);
#endif

void kos_process_tmo(void);
void kos_rdy_tsk_nolock(kos_tcb_t *tcb);
void kos_wait_nolock(kos_tcb_t *tcb);
void kos_cancel_wait_nolock(kos_tcb_t *tcb, kos_er_t er);

void kos_schedule_nolock(void);
void kos_ischedule_nolock(void);
void kos_schedule_impl_nolock(void);
#define kos_force_schedule_nolock() do { g_kos_pend_schedule = KOS_TRUE; kos_schedule_nolock(); } while(0);
#define kos_force_ischedule_nolock() do { g_kos_pend_schedule = KOS_TRUE; kos_ischedule_nolock(); } while(0);
kos_bool_t kos_cancel_wait_all_for_delapi_nolock(kos_list_t *wait_tsk_list);

/* kos_cyc.c */
void kos_init_cyc(void);
void kos_process_cyc(void);

/* kos_heap.c */
kos_er_t kos_init_heap(void);
kos_vp_t kos_alloc_nolock(kos_size_t size);
void kos_free_nolock(kos_vp_t p);

/* debug message */
//#define KOS_ENA_DBG_MSG
#ifdef KOS_ENA_DBG_MSG
#include <stdio.h>
#define kos_dbgprintf(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define kos_dbgprintf(fmt, ...) 
#endif

#endif
