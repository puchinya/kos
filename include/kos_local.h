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

#include "kos.h"
#include "mcu.h"

#define kos_lock	__disable_irq()
#define kos_unlock	__enable_irq()
#define kos_ilock	__disable_irq()
#define kos_iunlock	__enable_irq()

#define KOS_ARRAY_LEN(n)	(sizeof(n)/sizeof((n)[0]));

typedef struct kos_list_t kos_list_t;
typedef union kos_tcb_wait_info_t kos_tcb_wait_info_t;
typedef struct kos_tcb_t kos_tcb_t;
typedef struct kos_sem_cb_t kos_sem_cb_t;
typedef struct kos_flg_cb_t kos_flg_cb_t;
typedef struct kos_cyc_cb_t kos_cyc_cb_t;

struct kos_list_t {
	kos_list_t	*next, *prev;
};

#undef KOS_TCB_HAS_WAIT_INFO

union kos_tcb_wait_info_t {
#if defined(KOS_CFG_SPT_FLG) && defined(KOS_CFG_SPT_FLG_WMUL)
	struct {
		kos_mode_t		wfmode;
		kos_flgptn_t	waiptn;
		kos_flgptn_t	*relptn;	/* 待ち解除されたときのビットパターン */
	} flg;
#undef KOS_TCB_HAS_WAIT_INFO
#define KOS_TCB_HAS_WAIT_INFO
#endif
	int	dummy;
};

struct kos_tcb_t {
	kos_list_t			wait_list;	/* 待ち要因に対してつなぐリスト */
	kos_list_t			tmo_list;	/* タイムアウトのためにつなぐリスト */
	void				*sp;		/* current stack pointer */
	kos_id_t			id;			/* task id */
	kos_ctsk_t			ctsk;		/* task create parameters */
	kos_rtsk_t			st;
#ifdef KOS_TCB_HAS_WAIT_INFO
	kos_tcb_wait_info_t	wait_info;
#endif
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
extern kos_cyc_cb_t	*g_kos_cyc_cb[];

extern kos_tcb_t	g_kos_tcb_inst[];
extern kos_sem_cb_t	g_kos_sem_cb_inst[];
extern kos_flg_cb_t	g_kos_flg_cb_inst[];
extern kos_cyc_cb_t	g_kos_cyc_cb_inst[];

extern kos_uint_t	g_kos_isr_stk[];

extern void			*g_kos_idle_sp;
extern kos_list_t	g_kos_tmo_wait_list;
extern kos_bool_t	g_kos_dsp;
extern kos_bool_t	g_kos_pend_schedule;

extern const kos_uint_t g_kos_max_tsk;
extern const kos_uint_t g_kos_max_sem;
extern const kos_uint_t g_kos_max_flg;
extern const kos_uint_t g_kos_max_cyc;
extern const kos_uint_t g_kos_max_pri;
extern const kos_uint_t g_kos_max_intno;
extern const kos_uint_t	g_kos_isr_stksz;

void kos_start_kernel(void);
void kos_arch_setup_systick_handler(void);	/* for kos_arch.c */


static KOS_INLINE void kos_list_init(kos_list_t *l)
{
	l->next = l;
	l->prev = l;
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

void kos_init(void);
int kos_find_null(void **a, int len);

void kos_process_tmo(void);
void kos_rdy_tsk_nolock(kos_tcb_t *tcb);
kos_er_t kos_wait_nolock(kos_tcb_t *tcb);
void kos_cancel_wait_nolock(kos_tcb_t *tcb, kos_er_t er);
void kos_schedule_now_nolock(void);
void kos_schedule_nolock(void);
void kos_ischedule_nolock(void);
void kos_cancel_wait_all_for_delapi_nolock(kos_list_t *wait_tsk_list);

/* kos_cyc.c */
void kos_init_cyc(void);
void kos_process_cyc(void);

/* debug message */
#define KOS_ENA_DBG_MSG
#ifdef KOS_ENA_DBG_MSG
#include <stdio.h>
#define kos_dbgprintf(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define kos_dbgprintf(fmt, ...) 
#endif

#endif
