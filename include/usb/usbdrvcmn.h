/*!
 *	@file	usbdrvcmn.h
 *	@brief	usb device/host common header.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#ifndef __USBDRVCMN_H__
#define __USBDRVCMN_H__

#include "usb.h"
#include <mcu.h>

#define USBDRV_CFG_SUPPORT_KOS
#define USBDRV_CFG_SUPPORT_CLIENT
#define USBDRV_CFG_SUPPORT_HOST
#define USBDRV_CFG_IRQPRI	1

#ifdef USBDRV_CFG_SUPPORT_KOS
#include <kos.h>
#endif

#define USBDRV_NUM_DEV	2
#define USBDRV_NONEP0_CNT	5

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

#ifdef USBDRV_CFG_SUPPORT_KOS
#define usbdrv_er_t			kos_er_t
#define usbdrv_tmo_t		kos_tmo_t
#define USBDRV_E_OK			KOS_E_OK
#define USBDRV_E_PAR		KOS_E_PAR
#define USBDRV_E_TMOUT		KOS_E_TMOUT
#define USBDRV_E_USB_BASE	-100
#else
#define usbdrv_er_t			int32_t
#define usbdrv_tmo_t		uint32_t
#define USBDRV_E_OK			0
#define USBDRV_E_PAR		-1
#define USBDRV_E_TMOUT		-2
#define USBDRV_E_USB_BASE	-100
#endif

#define USBDRV_E_USB_LSTSOF	(USBDRV_E_USB_BASE - 1)
#define USBDRV_E_USB_HS		(USBDRV_E_USB_BASE - 2)
#define USBDRV_E_USB_TG		(USBDRV_E_USB_BASE - 3)
#define USBDRV_E_USB_NAK	(USBDRV_E_USB_BASE - 4)
#define USBDRV_E_USB_STALL	(USBDRV_E_USB_BASE - 5)
#define USBDRV_E_USB_NULL	(USBDRV_E_USB_BASE - 6)

typedef struct usbdrv_dev_t usbdrv_dev_t;
typedef struct usbdrv_abstract_ep_t usbdrv_abstract_ep_t;
typedef struct usbdrv_ep0_t usbdrv_ep0_t;
typedef struct usbdrv_ep_t usbdrv_ep_t;

typedef struct usbdrv_fifo_t {
	uint8_t				*buf;
	uint32_t			buf_size;
	uint32_t			rp;
	uint32_t			wp;
} usbdrv_fifo_t;

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
			uint32_t			size;
		} user_buf;
	};
#ifdef USBDRV_CFG_SUPPORT_KOS
	kos_id_t					tskid;
#else
	__IO uint32_t				flg;
#endif
} usbdrv_ep_buf_t;

struct usbdrv_abstract_ep_t
{
	usbdrv_dev_t			*dev;
	const usbdrv_ep_regs_t	*regs;
	uint16_t				max_pkts;
	uint16_t				pkts_msk;
	usbdrv_ep_buf_t 		buf;
};

struct usbdrv_ep0_t
{
	usbdrv_dev_t			*dev;
	const usbdrv_ep_regs_t	*regs;
	uint16_t				max_pkts;
	uint16_t				pkts_msk;
	usbdrv_ep_buf_t 		buf;
};

struct usbdrv_ep_t
{
	usbdrv_dev_t			*dev;
	const usbdrv_ep_regs_t	*regs;
	uint16_t				max_pkts;
	uint16_t				pkts_msk;
	usbdrv_ep_buf_t 		buf;
	uint32_t				index;
	usbdrv_ep_mode_t		mode;
	usbdrv_ep_dir_t			dir;
};

usbdrv_er_t usbdrv_init(void);

#endif
