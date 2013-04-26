/*!
 *	@file	kos_usr_cfg.h
 *	@author	puchinya
 *	@date	2012.12.22
 */
#ifndef __KOS_USR_CFG_H__
#define __KOS_USR_CFG_H__

#define KOS_MAX_PRI					16
#define KOS_MAX_TSK					16
#define KOS_MAX_SEM					16
#define KOS_MAX_FLG					16
#define KOS_MAX_DTQ					16
#define KOS_MAX_CYC					16
#define KOS_MAX_INTNO				82
#define KOS_DLY_SVC_FIFO_LEN		32
#define KOS_SYSMPL_SIZE				(1<<10)			/* OS用メモリプールのサイズ */
#define KOS_ISR_STKSIZE				0x200			/* ISR(Interrupt Service Routine) stack size */

#endif
