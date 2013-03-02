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

#endif
