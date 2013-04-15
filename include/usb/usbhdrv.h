/*!
 *	@file	usbhdrv.h
 *	@brief	usb host driver for FM3 Microcontoroller.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#ifndef __USBHDRV_H__
#define __USBHDRV_H__

#include "usbdrvcmn.h"

typedef struct usbhdrv_dev_t usbhdrv_dev_t;

struct usbhdrv_dev_t
{
	FM3_USB_TypeDef		*regs;
	uint32_t			port;
	uint16_t			toggle;
	uint16_t			trans_size;
	uint16_t			last_recv_size;
};

usbhdrv_dev_t *usbhdrv_open(uint32_t port);
usbdrv_er_t usbhdrv_close(usbhdrv_dev_t *dev);

usbdrv_er_t usbhdrv_wait_for_device(usbhdrv_dev_t *dev);

usbdrv_er_t usbhdrv_send_sof_token(usbhdrv_dev_t *dev);

usbdrv_er_t usbhdrv_setup_transaction(usbhdrv_dev_t *dev,
	const usb_device_request_t *device_request);

usbdrv_er_t usbhdrv_begin_in_transaction(usbhdrv_dev_t *dev,
	uint32_t ep_no);
usbdrv_er_t usbhdrv_end_in_transaction(usbhdrv_dev_t *dev,
	uint32_t ep_no);
usbdrv_er_t usbhdrv_in_transaction(usbhdrv_dev_t *dev,
	uint32_t ep_no, uint16_t *buf, uint32_t buf_size);


#endif
