/*!
 *	@file	kos_usr_cfg.h
 *	@author	puchinya
 *	@date	2012.12.22
 */
#ifndef __KOS_USR_CFG_H__
#define __KOS_USR_CFG_H__

#define KOS_MAX_PRI			16
#define KOS_MAX_TSK			16
#define KOS_MAX_SEM			16
#define KOS_MAX_FLG			16
#define KOS_MAX_CYC			16
#define KOS_MAX_INTNO		46
#define KOS_SYSMPL_SIZE		(1<<10)			/* OS用メモリプールのサイズ */
#define KOS_IDLE_STKSIZE	0x100			/* IDLEタスクのスタックサイズ */
#define KOS_INIT_TSK		init_task		/* 初期化タスクのエントリーポイント名 */
#define KOS_INIT_STKSIZE	0x200			/* 初期化タスクのスタックサイズ */
//#define KOS_IDLE_TASK

#endif
