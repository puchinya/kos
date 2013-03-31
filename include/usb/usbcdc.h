/*!
 *	@file	usbcdc.h
 *	@brief	usb CDC class driver.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#ifndef __USBCDC_H__
#define __USBCDC_H__

#include "usbdrv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct usbcdc_t usbcdc_t;

usbcdc_t *usbcdc_open(uint32_t usb_port);
usbdrv_er_t usbcdc_close(usbcdc_t *cdc);
int32_t usbcdc_write(usbcdc_t *cdc, const void *buf, uint32_t buf_size);
int32_t usbcdc_read(usbcdc_t *cdc, void *buf, uint32_t buf_size);
usbdrv_er_t usbcdc_set_inbuf(usbcdc_t *cdc, uint8_t *buf, uint32_t buf_size);
usbdrv_er_t usbcdc_set_outbuf(usbcdc_t *cdc, uint8_t *buf, uint32_t buf_size);
usbdrv_er_t usbcdc_set_read_timeout(usbcdc_t *cdc, usbdrv_tmo_t tmo);
usbdrv_er_t usbcdc_set_write_timeout(usbcdc_t *cdc, usbdrv_tmo_t tmo);

#ifdef __cplusplus
};
#endif
	
#endif
