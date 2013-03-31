/*!
 *	@file	usbdrvcmn_local.h
 *	@brief	usb device/host common local header file.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#ifndef __USBDRVCMN_LOCAL_H_
#define __USBDRVCMN_LOCAL_H_

#include "usbdrvcmn.h"

#define EPS_BFINI			(1<<15)
#define EPS_DRQIE			(1<<14)
#define EPS_SPKIE			(1<<13)
#define EPS_BUSY			(1<<11)
#define EPS_DRQ				(1<<10)
#define EPS_SPK				(1<<9)
#define EP1S_SIZE_MASK		0x1FF
#define EP2_5S_SIZE_MASK	0x7F

extern const usbdrv_ep_regs_t *g_usb0_all_ep_regs[];
extern const usbdrv_ep_regs_t *g_usb1_all_ep_regs[];

int32_t usbdrv_local_use(uint32_t port);
int32_t usbdrv_local_unuse(uint32_t port);
void usbdrv_local_clockon(uint32_t port);
void usbdrv_local_clockoff(uint32_t port);

#endif
