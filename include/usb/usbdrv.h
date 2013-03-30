/*!
 *	@file	usbdrv.h
 *	@brief	usb device driver.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#ifndef __USBDRV_H__
#define __USBDRV_H__

#include <stddef.h>
#include "usb.h"
#include <mcu.h>

#define USBDRV_CFG_SUPPORT_KOS

#ifdef USBDRV_CFG_SUPPORT_KOS
#include <kos.h>
#endif

#define USBDRV_NONEP0_CNT		5

#define USBDRV_EP0_MAXSIZE	64
#define USBDRV_EP1_MAXSIZE	256
#define USBDRV_EP2_MAXSIZE	32
#define USBDRV_EP3_MAXSIZE	32
#define USBDRV_EP4_MAXSIZE	32
#define USBDRV_EP5_MAXSIZE	32

#define USBDRV_EP0_PKTS_MSK	0x3F
#define USBDRV_EP1_PKTS_MSK	0x1FF
#define USBDRV_EP2_PKTS_MSK	0x7F
#define USBDRV_EP3_PKTS_MSK	0x7F
#define USBDRV_EP4_PKTS_MSK	0x7F
#define USBDRV_EP5_PKTS_MSK	0x7F

typedef struct usbdrv_dev_t usbdrv_dev_t;
typedef struct usbdrv_ep0_t usbdrv_ep0_t;
typedef struct usbdrv_ep_t usbdrv_ep_t;

typedef int (*usbdrv_device_request_func_t)(usbdrv_dev_t *dev,
	const usb_device_request_t *device_request);

typedef struct {
	usbdrv_device_request_func_t	device_request_func;
} usbdrv_func_table_t;

typedef struct usbdrv_fifo_t
{
	uint8_t				*buf;
	size_t				buf_size;
	size_t				rp;
	size_t				wp;
} usbdrv_fifo_t;

typedef enum {
	/* コントロール転送 */
	USBDRV_EP_STAT_SETUP,
	USBDRV_EP_STAT_HSIN,
	USBDRV_EP_STAT_HSOUT,
	USBDRV_EP_STAT_DATAIN,
	USBDRV_EP_STAT_DATAOUT
} usbdrv_ep_stat_t;

typedef enum {
	USBDRV_EP_DIR_IN,
	USBDRV_EP_DIR_OUT
} usbdrv_ep_dir_t;

typedef enum {
	USBDRV_EP_MODE_CTRL	= 0,
	USBDRV_EP_MODE_ISO	= 1,
	USBDRV_EP_MODE_BULK	= 2,
	USBDRV_EP_MODE_INTR	= 3
} usbdrv_ep_mode_t;

typedef enum {
	USBDRV_EP_DATA_TRANS_MODE_CPU_DIRECT,
	USBDRV_EP_DATA_TRANS_MODE_CPU_BUFFERED,
	USBDRV_EP_DATA_TRANS_MODE_DMA
} usbdrv_ep_data_trans_mode_t;

typedef struct {
	__IO uint16_t	*C;
	__IO uint16_t	*S;
	__IO uint16_t	*DT;
} usbdrv_ep_regs_t;

typedef struct {
	usbdrv_ep_data_trans_mode_t	trans_mode;
	union {
		usbdrv_fifo_t			temp_buf;
		struct {
			uint8_t				*p;
			unsigned int		size;
		} user_buf;
	};
#ifdef USBDRV_CFG_SUPPORT_KOS
	kos_id_t					tskid;
#else
	volatile unsigned int		flg;
#endif
} usbdrv_ep_buf_t;

struct usbdrv_ep0_t
{
	usbdrv_dev_t			*dev;
	const usbdrv_ep_regs_t	*regs;
	unsigned short			max_pkts;
	unsigned short			pkts_msk;
	usbdrv_ep_stat_t		stat;
	usbdrv_ep_buf_t 		tx_buf;
	usbdrv_ep_buf_t 		rx_buf;
};

struct usbdrv_ep_t
{
	usbdrv_dev_t			*dev;
	int						index;
	const usbdrv_ep_regs_t	*regs;
	unsigned short			max_pkts;
	unsigned short			pkts_msk;
	usbdrv_ep_mode_t		mode;
	usbdrv_ep_stat_t		stat;
	usbdrv_ep_dir_t			dir;
	usbdrv_ep_buf_t 		buf;
};

struct usbdrv_dev_t
{
	FM3_USB_TypeDef			*regs;
	int						is_opened;
	usbdrv_func_table_t		func_table;
	void					*unique_data;
	usbdrv_ep0_t			ep0;
	usbdrv_ep_t				ep[USBDRV_NONEP0_CNT];
};

typedef void (*usbdrv_rw_finish_cb_func_t)(void *unique_data, int status, unsigned int rw_bytes);

typedef struct {
	void	*unique_data;
	usbdrv_rw_finish_cb_func_t	func;
} usbdrv_rw_finish_cb_t;

static void usbdrv_fifo_init(usbdrv_fifo_t *fifo, uint8_t *buf, size_t buf_size)
{
	fifo->buf = buf;
	fifo->buf_size = buf_size;
	fifo->rp = 0;
	fifo->wp = 0;
}

#define USBDRV_E_PARAM	-1
#define USBDRV_E_STAT	-2

int usbdrv_init(void);
usbdrv_dev_t *usbdrv_create(int port);
void usbdrv_destory(usbdrv_dev_t *dev);

void usbhdrv_open_host(usbdrv_dev_t *dev);

void usbdrv_begin_connect_setting(usbdrv_dev_t *dev);
void usbdrv_end_connect_setting(usbdrv_dev_t *dev);

/* EP0にSTALL状態を設定します。 */
void usbdrv_ep0_stall(usbdrv_dev_t *dev);
void usbdrv_ep0_begin_write(usbdrv_dev_t *dev,
	const uint8_t *buf, unsigned int size);
void usbdrv_ep0_begin_read(usbdrv_dev_t *dev,
	uint8_t *buf, unsigned int size);
int usbdrv_config_ep(usbdrv_dev_t *dev, const uint8_t *cfg_desc);

/* EP1-5の関数 */
void usbdrv_ep_wait_for_idle(usbdrv_ep_t *ep);
int usbdrv_ep_set_trans_mode(usbdrv_ep_t *ep, usbdrv_ep_data_trans_mode_t trans_mode);
void usbdrv_ep_set_stall(usbdrv_ep_t *ep);
void usbdrv_ep_set_buf(usbdrv_ep_t *ep, uint8_t *buf, unsigned int size);
int usbdrv_ep_read(usbdrv_ep_t *ep, uint8_t *buf, unsigned int size);
int usbdrv_ep_write(usbdrv_ep_t *ep, const uint8_t *buf, unsigned int size);
int usbdrv_ep_begin_read(usbdrv_ep_t *ep, const uint8_t *buf, unsigned int size, const usbdrv_rw_finish_cb_t *cb);
int usbdrv_ep_begin_write(usbdrv_ep_t *ep, uint8_t *buf, unsigned int size, const usbdrv_rw_finish_cb_t *cb);
void usbdrv_ep_reset(usbdrv_ep_t *ep);

#endif
