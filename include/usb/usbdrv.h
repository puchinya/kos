/*!
 *	@file	usbdrv.h
 *	@brief	usb device driver for FM3 Microcontoroller.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#ifndef __USBDRV_H__
#define __USBDRV_H__

#include "usbdrvcmn.h"

typedef int (*usbdrv_device_request_func_t)(usbdrv_dev_t *dev,
	const usb_device_request_t *device_request);

typedef struct {
	usbdrv_device_request_func_t	device_request_func;
} usbdrv_func_table_t;

typedef enum {
	USBDRV_SETUP_STAT_SETUP,
	USBDRV_SETUP_STAT_HSIN,
	USBDRV_SETUP_STAT_HSOUT,
	USBDRV_SETUP_STAT_DATAIN,
	USBDRV_SETUP_STAT_DATAOUT
} usbdrv_setup_stat_t;

struct usbdrv_dev_t
{
	FM3_USB_TypeDef			*regs;
	uint32_t				port;
	usbdrv_func_table_t		func_table;
	void					*unique_data;
	usbdrv_setup_stat_t		setup_stat;
	usbdrv_ep0_t			ep0i;
	usbdrv_ep0_t			ep0o;
	usbdrv_ep_t				ep[USBDRV_NONEP0_CNT];
};

typedef void (*usbdrv_rw_finish_cb_func_t)(void *unique_data, usbdrv_er_t status, uint32_t rw_bytes);

typedef struct {
	void	*unique_data;
	usbdrv_rw_finish_cb_func_t	func;
} usbdrv_rw_finish_cb_t;

static void usbdrv_fifo_init(usbdrv_fifo_t *fifo, uint8_t *buf, uint32_t buf_size)
{
	fifo->buf = buf;
	fifo->buf_size = buf_size;
	fifo->rp = 0;
	fifo->wp = 0;
}

usbdrv_dev_t *usbdrv_open(uint32_t port);
void usbdrv_close(usbdrv_dev_t *dev);

void usbdrv_begin_connect_setting(usbdrv_dev_t *dev);
void usbdrv_end_connect_setting(usbdrv_dev_t *dev);

/* EP0にSTALL状態を設定します。 */
void usbdrv_ep0_stall(usbdrv_dev_t *dev);
void usbdrv_ep0_begin_write(usbdrv_dev_t *dev,
	const uint8_t *buf, uint32_t size);
void usbdrv_ep0_begin_read(usbdrv_dev_t *dev,
	uint8_t *buf, uint32_t size);
usbdrv_er_t usbdrv_config_ep(usbdrv_dev_t *dev, const uint8_t *cfg_desc);

/* EP1-5の関数 */
void usbdrv_ep_wait_for_idle(usbdrv_ep_t *ep);
usbdrv_er_t usbdrv_ep_set_trans_mode(usbdrv_ep_t *ep, usbdrv_ep_data_trans_mode_t trans_mode);
void usbdrv_ep_set_stall(usbdrv_ep_t *ep);
void usbdrv_ep_set_buf(usbdrv_ep_t *ep, uint8_t *buf, uint32_t size);
int32_t usbdrv_ep_read(usbdrv_ep_t *ep, uint8_t *buf, uint32_t size);
int32_t usbdrv_ep_write(usbdrv_ep_t *ep, const uint8_t *buf, uint32_t size);
int32_t usbdrv_ep_begin_read(usbdrv_ep_t *ep, const uint8_t *buf, uint32_t size, const usbdrv_rw_finish_cb_t *cb);
int32_t usbdrv_ep_begin_write(usbdrv_ep_t *ep, uint8_t *buf, uint32_t size, const usbdrv_rw_finish_cb_t *cb);
void usbdrv_ep_reset(usbdrv_ep_t *ep);

#endif
