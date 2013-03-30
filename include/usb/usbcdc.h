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

usbcdc_t *usbcdc_create(int usb_port);
void usbcdc_destory(usbcdc_t *cdc);
int usbcdc_open(usbcdc_t *cdc);
int usbcdc_close(usbcdc_t *cdc);
int usbcdc_write(usbcdc_t *cdc, const void *buf, unsigned int buf_size);
int usbcdc_read(usbcdc_t *cdc, void *buf, unsigned int buf_size);
int usbcdc_set_inbuf(usbcdc_t *cdc, uint8_t *buf, size_t buf_size);
int usbcdc_set_outbuf(usbcdc_t *cdc, uint8_t *buf, size_t buf_size);
int usbcdc_set_read_timeout(usbcdc_t *cdc, int tmo);
int usbcdc_set_write_timeout(usbcdc_t *cdc, int tmo);

#ifdef __cplusplus
};
#endif
	
#endif
