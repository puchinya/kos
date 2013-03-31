/*!
 *	@file	kos_sys_cfg.h
 *	@author	puchinya
 *	@date	2012.12.22
 */
#ifndef __KOS_SYS_CFG_H__
#define __KOS_SYS_CFG_H__

#define KOS_CFG_STKCHK				/* スタックチェック用に0xccでスタックをフィルする */
#define KOS_CFG_ENA_PAR_CHK			/* API内部でのパラメータチェックの有無 */
#define KOS_CFG_ENA_CTX_CHK			/* API内部でのコンテキストチェックの有無 */
#define KOS_CFG_SPT_SEM				/* セマフォサポート */
#define KOS_CFG_SPT_FLG				/* イベントフラグサポート */
#define KOS_CFG_SPT_DTQ				/* データキューサポート */
//#define KOS_CFG_SPT_FLG_WMUL		/* KOS_TA_WMULのサポート */
#define KOS_MAX_ACT_CNT		0xFF	/* act_tskの上限キューイング数 */
#define KOS_MAX_WUP_CNT		0xFF	/* wup_tskの上限キューイング数 */
#define KOS_MAX_SUS_CNT		0xFF	/* sus_tskの上限キューイング数 */
#define KOS_TMIN_TPRI		1
#define KOS_TMAX_TPRI		KOS_MAX_PRI
#define KOS_TMIN_TINTPRI	KOS_ARCH_MIN_INTPRI
#define KOS_TMAX_TINTPRI	KOS_ARCH_MAX_INTPRI
#define KOS_TBIT_TEXPTN		32
#define KOS_TBIT_FLGPTN		32
#define KOS_TIC_NUME		1
#define KOS_TIC_DENO		1

#define KOS_DISPATCHER_TYPE1
#define KOS_CFG_ENA_ACRE_CONST_TIME_ID_SEARCH	/* acre_xxxのAPI内部でのID探索を一定時間で行います。 */
//#define KOS_CFG_FAST_IRQ

#endif
