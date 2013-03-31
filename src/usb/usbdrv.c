/*!
 *	@file	usbdrv.c
 *	@brief	usb device driver for FM3 Microcontoroller.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "usbdrv.h"
#include "usbdrvcmn_local.h"
#include <string.h>

static usbdrv_dev_t s_dev[USBDRV_NUM_DEV];

static void usbdrv_irq(usbdrv_dev_t *dev);
static void usbdrv_irqf(usbdrv_dev_t *dev);
static void usbdrv_bus_reset(usbdrv_dev_t *dev);
static void usbdrv_setup_pins(usbdrv_dev_t *dev);
static void usbdrv_ctrl_setup(usbdrv_dev_t *dev);
static int usvdrv_ctrl_read_device_request(usbdrv_dev_t *dev, usb_device_request_t *device_request);
static void usbdrv_ctrl_process_device_request(usbdrv_dev_t *dev, const usb_device_request_t *device_request);
static void usbdrv_ctrl_setup(usbdrv_dev_t *dev);
static void usbdrv_ctrl_ep0_rx(usbdrv_dev_t *dev);
static void usbdrv_ctrl_ep0_tx(usbdrv_dev_t *dev);

static void usbdrv_trans(usbdrv_ep_t *ep);

static void usbdrv_intr_trans(usbdrv_ep_t *ep);
static void usbdrv_intr_tx(usbdrv_ep_t *ep);
static void usbdrv_intr_rx(usbdrv_ep_t *ep);

static void usbdrv_bulk_trans(usbdrv_ep_t *ep);
static void usbdrv_bulk_tx(usbdrv_ep_t *ep);
static void usbdrv_bulk_rx(usbdrv_ep_t *ep);

static unsigned int usbdrv_buf_recv(usbdrv_ep_buf_t *buf,
	__IO uint16_t *dt, unsigned int max_recv_size);
static unsigned int usbdrv_buf_send(usbdrv_ep_buf_t *buf,
	__IO uint16_t *dt, unsigned int max_send_size);

usbdrv_dev_t *usbdrv_open(uint32_t port)
{
	usbdrv_dev_t *dev;
	
	if(port >= USBDRV_NUM_DEV)
		return NULL;
	
	if(!usbdrv_local_use(port))
		return NULL;
	
	dev = &s_dev[port];
	if(port == 0) {
		dev->regs = FM3_USB0;
	} else {
		dev->regs = FM3_USB1;
	}
	dev->port = port;
	
	usbdrv_local_clockon(port);
	
#ifdef USBDRV_CFG_SUPPORT_KOS
	{
		kos_dinh_t dinh;
		kos_intno_t usb_intno;
		kos_intno_t usbh_intno;
		
		dinh.inhatr = KOS_TA_HLNG;
#ifdef KOS_ARCH_CFG_SPT_IRQPRI
		dinh.intpri = USBDRV_CFG_IRQPRI;
#endif
		dinh.exinf = dev;
		
		if(dev->port == 0) {
			usb_intno = USB0F_IRQn;
			usbh_intno = USB0F_USB0H_IRQn;
		} else {
			usb_intno = USB1F_IRQn;
			usbh_intno = USB1F_USB1H_IRQn;
		}
		
		dinh.inthdr = usbdrv_irqf;
		kos_def_inh(usb_intno, &dinh);
		dinh.inthdr = usbdrv_irq;
		kos_def_inh(usbh_intno, &dinh);
		kos_ena_int(usb_intno);
		kos_ena_int(usbh_intno);
	}
#else
	if(dev->port == 0) {
		NVIC_EnableIRQ(USB0F_IRQn);
		NVIC_EnableIRQ(USB0F_USB0H_IRQn);
	} else {
		NVIC_EnableIRQ(USB1F_IRQn);
		NVIC_EnableIRQ(USB1F_USB1H_IRQn);
	}
#endif
	
	return dev;
}

void usbdrv_close(usbdrv_dev_t *dev)
{
#ifdef USBDRV_CFG_SUPPORT_KOS
	kos_intno_t usb_intno;
	kos_intno_t usbh_intno;
	
	if(dev->port == 0) {
		usb_intno = USB0F_IRQn;
		usbh_intno = USB0F_USB0H_IRQn;
	} else {
		usb_intno = USB1F_IRQn;
		usbh_intno = USB1F_USB1H_IRQn;
	}
	
	kos_dis_int(usb_intno);
	kos_dis_int(usbh_intno);
	kos_def_inh(usb_intno, KOS_NULL);
	kos_def_inh(usbh_intno, KOS_NULL);
	
#else
	if(dev->port == 0) {
		NVIC_DisableIRQ(USB0F_IRQn);
		NVIC_DisableIRQ(USB0F_USB0H_IRQn);
	} else {
		NVIC_DisableIRQ(USB1F_IRQn);
		NVIC_DisableIRQ(USB1F_USB1H_IRQn);
	}
#endif
	usbdrv_local_clockoff(dev->port);
	usbdrv_local_unuse(dev->port);
}

void usbdrv_begin_connect_setting(usbdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	const usbdrv_ep_regs_t **ep_regs = g_usb0_all_ep_regs;
	volatile uint32_t dummy;
	
	/* D+プルアップ切断設定 */
	usbdrv_setup_pins(dev);
	
	/* UDCC_RST=1 設定 */
	regs->UDCC_f.RST = 1;
	
	/* STALLの自動クリアを有効化 */
	regs->UDCC_f.STALCLREN = 1;
	
	/* ダミーリード */
	dummy = regs->UDCC;
	
	/* EP0を設定 */
	regs->EP0C = USBDRV_EP0_MAXSIZE;
	
	/* EP1-5を設定 */
	regs->EP1C = USBDRV_EP1_MAXSIZE;
	regs->EP2C = USBDRV_EP2_MAXSIZE;
	regs->EP3C = USBDRV_EP3_MAXSIZE;
	regs->EP4C = USBDRV_EP4_MAXSIZE;
	regs->EP5C = USBDRV_EP5_MAXSIZE;
	
	memset(&dev->ep, 0, sizeof(dev->ep));
	
	{
		usbdrv_ep0_t *ep = &dev->ep0i;
		ep->dev			= dev;
		ep->max_pkts	= USBDRV_EP0_MAXSIZE;
		ep->pkts_msk	= USBDRV_EP0_PKTS_MSK;
		ep->regs		= ep_regs[0];
	}
	{
		usbdrv_ep0_t *ep = &dev->ep0o;
		ep->dev			= dev;
		ep->max_pkts	= USBDRV_EP0_MAXSIZE;
		ep->pkts_msk	= USBDRV_EP0_PKTS_MSK;
		ep->regs		= ep_regs[1];
	}
	{
		usbdrv_ep_t *ep = &dev->ep[0];
		ep->dev			= dev;
		ep->index		= 1;
		ep->pkts_msk	= USBDRV_EP1_PKTS_MSK;
		ep->regs		= ep_regs[2];
	}
	{
		usbdrv_ep_t *ep = &dev->ep[1];
		ep->dev			= dev;
		ep->index		= 2;
		ep->pkts_msk	= USBDRV_EP2_PKTS_MSK;
		ep->regs		= ep_regs[3];
	}
	{
		usbdrv_ep_t *ep = &dev->ep[2];
		ep->dev			= dev;
		ep->index		= 3;
		ep->pkts_msk	= USBDRV_EP3_PKTS_MSK;
		ep->regs		= ep_regs[4];
	}
	{
		usbdrv_ep_t *ep = &dev->ep[3];
		ep->dev			= dev;
		ep->index		= 4;
		ep->pkts_msk	= USBDRV_EP4_PKTS_MSK;
		ep->regs		= ep_regs[5];
	}
	{
		usbdrv_ep_t *ep = &dev->ep[4];
		ep->dev			= dev;
		ep->index		= 5;
		ep->pkts_msk	= USBDRV_EP5_PKTS_MSK;
		ep->regs		= ep_regs[6];
	}
}

usbdrv_er_t usbdrv_config_ep(usbdrv_dev_t *dev, const uint8_t *cfg_desc)
{
	uint32_t total_len;
	uint32_t i;
	const uint8_t *desc;
	
	desc = cfg_desc;
	total_len = ((uint32_t)cfg_desc[3] << 8) | cfg_desc[2];

	for(i = 0; i < total_len; ) {
		uint32_t desc_type, desc_len;
		
		desc_type = desc[1];
		desc_len = desc[0];
		
		if(desc_type == 0x05) {
			uint32_t ep_addr, ep_attrs, ep_max_pkts;
			usbdrv_ep_t *ep;
			uint32_t c;
			
			ep_addr = desc[2];
			ep_attrs = desc[3];
			ep_max_pkts = desc[4];
			ep = &dev->ep[(ep_addr & 0x7F)-1];
			ep->dir = (ep_addr & 0x80) ? USBDRV_EP_DIR_IN : USBDRV_EP_DIR_OUT;
			ep->max_pkts = ep_max_pkts & 0x1FF;
			ep->mode = (usbdrv_ep_mode_t)(ep_attrs & 3);
			//if(ep->dir == USBDRV_EP_DIR_OUT && ep->mode == USBDRV_EP_MODE_BULK) {
			//	*ep->regs->S |= EPS_DRQIE;
			//}
			
			c = 0x8000 | ep->max_pkts;
			c |= ((uint32_t)ep->mode << 13);
			if(ep->dir == USBDRV_EP_DIR_IN)
				c |= 1<<12;
			*ep->regs->C = c;
		}
		
		i += desc_len;
		desc += desc_len;
	}
	
	return USBDRV_E_OK;
}

void usbdrv_end_connect_setting(usbdrv_dev_t *dev)
{
	volatile uint32_t dummy;
	FM3_USB_TypeDef *regs = dev->regs;
	
	/* UDCC_RST=0 設定 */
	regs->UDCC_f.RST = 0;
	/* リセット解除待ち */
	while(regs->UDCC_f.RST);
	
	/* バッファクリア */
	regs->EP0OS_f.BFINI = 0;
	regs->EP0IS_f.BFINI = 0;
	regs->EP1S_f.BFINI = 0;
	regs->EP2S_f.BFINI = 0;
	regs->EP3S = 0;
	regs->EP4S_f.BFINI = 0;
	regs->EP5S_f.BFINI = 0;
	
	/* VBUS検出 */
	
	
	/* UDCC_HCONX=0 設定 */
	regs->UDCC_f.HCONX = 0;
	
	/* D+プルアップ接続設定 */
	
	/* ステータス設定　*/
	regs->UDCS = 0;
	dummy = regs->UDCS;
	
	regs->UDCIE_f.BRSTIE = 1;
	regs->UDCIE_f.CONFIE = 1;
	dummy = regs->UDCIE;	/* dummy read */
	
	/* EP0受信割り込み許可 */
	regs->EP0OS_f.DRQOIE = 1;
	
}

#ifndef USBDRV_CFG_SUPPORT_KOS
void INT8_31_Handler(void)
{
}

void USB0_Handler(void)
{
	usbdrv_irq(&s_dev[0]);
}

void USB1_Handler(void)
{
	usbdrv_irq(&s_dev[1]);
}

void USB0F_Handler(void)
{
	usbdrv_irqf(&s_dev[0]);
}

void USB1F_Handler(void)
{
	usbdrv_irqf(&s_dev[1]);
}
#endif

static void usbdrv_setup_pins(usbdrv_dev_t *dev)
{
	/* UDM0とUDMP0ピンをUSBとして使う */
	FM3_GPIO->SPSR_f.USB0C = 1;
	FM3_GPIO->PFR8_f.P0 = 1;
	FM3_GPIO->PFR8_f.P1 = 1;
	
	/* D+のプルアップ抵抗制御出力にUHCONX(P61)を使う */
	FM3_GPIO->EPFR00_f.USBP0E = 1;
	FM3_GPIO->PFR6_f.P1 = 1;
	
	/* EINT_ch15の入力端子にINT15_1(P60)を使う */
	FM3_GPIO->EPFR06_f.EINT15S1 = 1;
	
	/* USBファンクションとして使う */
	dev->regs->HCNT0_f.HOST = 0;
	
	/* 切断 */
	dev->regs->UDCC_f.HCONX = 1;
	
	/* バスパワー */
	dev->regs->UDCC_f.PWC = 0;
}

/* EP1-5 DRQ */
static void usbdrv_irqf(usbdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	uint16_t st;
	volatile uint32_t dummy;
	
	st = regs->EP1S;
	if(st & EPS_DRQ) {
		usbdrv_trans(&dev->ep[0]);
		
		regs->EP1S_f.DRQ = 0;
		dummy = regs->EP1C;
	}
	
	st = regs->EP2S;
	if(st & EPS_DRQ) {
		usbdrv_trans(&dev->ep[1]);
		
		regs->EP2S_f.DRQ = 0;
		dummy = regs->EP2C;
	}
	
	st = regs->EP3S;
	if(st & EPS_DRQ) {
		usbdrv_trans(&dev->ep[2]);
		
		regs->EP3S &= ~EPS_DRQ;
		dummy = regs->EP3C;
	}
	
	st = regs->EP4S;
	if(st & EPS_DRQ) {
		usbdrv_trans(&dev->ep[2]);
		
		regs->EP4S_f.DRQ = 0;
		dummy = regs->EP4C;
	}
	
	st = regs->EP5S;
	if(st & EPS_DRQ) {
		usbdrv_trans(&dev->ep[2]);
		
		regs->EP5S_f.DRQ = 0;
		dummy = regs->EP5C;
	}
}

/* EP0 DRQI, DRQO, STATUS */
static void usbdrv_irq(usbdrv_dev_t *dev)
{
	stc_usb_udcs_field_t udcs_f = dev->regs->UDCS_f;
	
	if(udcs_f.BRST) {
		/* バスリセット検出 */
		usbdrv_bus_reset(dev);
	} else if(udcs_f.CONF) {
		/* コンフィグレーション検出 */
		dev->regs->UDCS_f.CONF = 0;
	} else if(dev->regs->EP0OS_f.DRQO) {
		if(udcs_f.SETP) {
			usbdrv_ctrl_setup(dev);
		} else {
			usbdrv_ctrl_ep0_rx(dev);
		}
	} else if(dev->regs->EP0IS_f.DRQI) {
		usbdrv_ctrl_ep0_tx(dev);
	} else if(udcs_f.SUSP) {
		/* サスペンド検出 */
		dev->regs->UDCS_f.SUSP = 0;
	} else if(udcs_f.WKUP) {
		/* WakeUp検出 */
		dev->regs->UDCS_f.WKUP = 0;
	} else if(udcs_f.SOF) {
		/* Start Of Frame検出 */
		dev->regs->UDCS_f.SOF = 0;
	} else {
		/* SPK */
	}
}

void usbdrv_stall_ep0(usbdrv_dev_t *dev)
{
	dev->regs->EP0C_f.STAL = 1;
}

/*!
 *	コントロール転送 セットアップステージ
 */
static void usbdrv_ctrl_setup(usbdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	usb_device_request_t device_request;
	int i;
	
	/* 送受信割り込み禁止 */
//	regs->EP0IS_f.DRQIIE = 0;
//	regs->EP0OS_f.DRQOIE = 0;
	regs->EP0IS_f.BFINI = 1;
	regs->EP0IS_f.BFINI = 0;
	
	dev->setup_stat = USBDRV_SETUP_STAT_SETUP;
	
	/* デバイスリクエストを読む */
	i = usvdrv_ctrl_read_device_request(dev, &device_request);
	
	/* リクエストを読み込んだので要因を解除 */
	regs->UDCS_f.SETP = 0;
	
	/* リクエストの読み込みが完了したので受信割り込み要求をクリア */
	regs->EP0OS_f.DRQO = 0;
	
	if(i) {
		/* デバイスリクエストを処理する */
		usbdrv_ctrl_process_device_request(dev, &device_request);
		if(dev->setup_stat == USBDRV_SETUP_STAT_SETUP) {
			dev->setup_stat = USBDRV_SETUP_STAT_HSIN;
			dev->regs->EP0IS_f.DRQIIE = 1;
		}
	} else {
		usbdrv_ep0_stall(dev);
	}
}

static int usvdrv_ctrl_read_device_request(usbdrv_dev_t *dev,
	usb_device_request_t *device_request)
{
	FM3_USB_TypeDef *regs = dev->regs;
	
	if((regs->EP0OS & USBDRV_EP0_PKTS_MSK) != 8)
		return 0;
	
	*(uint16_t *)&device_request->bmRequestType = regs->EP0DT;
	device_request->wValue = regs->EP0DT;
	device_request->wIndex = regs->EP0DT;
	device_request->wLength = regs->EP0DT;
	
	return 1;
}

static void usbdrv_ctrl_process_device_request(usbdrv_dev_t *dev,
	const usb_device_request_t *device_request)
{
	dev->func_table.device_request_func(dev, device_request);
}

static void usbdrv_ctrl_ep0_rx(usbdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	uint32_t pkt_size;
	volatile uint32_t dummy;
	
	if(dev->setup_stat == USBDRV_SETUP_STAT_DATAOUT) {
		pkt_size = regs->EP0OS & USBDRV_EP0_PKTS_MSK;
		
		usbdrv_buf_recv(&dev->ep0o.buf,
			&regs->EP0DT, pkt_size);
		
		if(pkt_size < USBDRV_EP0_MAXSIZE) {
			regs->EP0IS_f.DRQIIE = 1;
			dev->setup_stat = USBDRV_SETUP_STAT_HSOUT;
		}
	} else {
		dev->setup_stat = USBDRV_SETUP_STAT_SETUP;
	}
	
	regs->EP0OS_f.DRQO = 0;
	dummy = regs->EP0C;
}

static void usbdrv_ctrl_ep0_tx(usbdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	uint32_t send_size;
	uint32_t pkt_size;
	volatile uint32_t dummy;
	
	pkt_size = regs->EP0IS & USBDRV_EP0_PKTS_MSK;
	
	send_size = usbdrv_buf_send(&dev->ep0i.buf,
		&regs->EP0DT, USBDRV_EP0_MAXSIZE);
	
	if(send_size == 0) {
		/* 送るものがない */
		regs->EP0IS_f.DRQIIE = 0;
		if(dev->setup_stat == USBDRV_SETUP_STAT_DATAIN) {
			dev->setup_stat = USBDRV_SETUP_STAT_HSOUT;
		} else {
			dev->setup_stat = USBDRV_SETUP_STAT_SETUP;
		}
	} else if(send_size < USBDRV_EP0_MAXSIZE) {
		/* 最終パケットを送信完了 */
		regs->EP0IS_f.DRQIIE = 0;
		dev->setup_stat = USBDRV_SETUP_STAT_HSIN;
	} else {
		regs->EP0IS_f.DRQIIE = 1;
	}
	
	regs->EP0IS_f.DRQI = 0;
	dummy = regs->EP0C;
}

static uint32_t usbdrv_buf_send(usbdrv_ep_buf_t *buf,
	__IO uint16_t *dt, uint32_t max_send_size)
{
	uint8_t *p = buf->user_buf.p;
	__IO uint8_t *dtl;
	__IO uint8_t *dth;
	uint32_t s = buf->user_buf.size;
	uint32_t cps = s;
	uint32_t i;
	
	if(cps > max_send_size) {
		cps = max_send_size;
	}
	
	dtl = (__IO uint8_t *)dt;
	dth = (__IO uint8_t *)dt + 1;
	for(i = 0; i < cps; i++) {
		if(!(i&1))
				*dtl = *p++;
		else
				*dth = *p++;
	}
	
	buf->user_buf.p = p;
	buf->user_buf.size -= cps;
	
	return cps;
}

static uint32_t usbdrv_buf_recv(usbdrv_ep_buf_t *buf,
	__IO uint16_t *dt, uint32_t max_recv_size)
{
	uint8_t *p = buf->user_buf.p;
	__IO uint8_t *dtl;
	__IO uint8_t *dth;
	uint32_t s = buf->user_buf.size;
	uint32_t cps = s;
	uint32_t i;
	
	if(cps > max_recv_size) {
		cps = max_recv_size;
	}
	
	dtl = (__IO uint8_t *)dt;
	dth = (__IO uint8_t *)dt + 1;
	for(i = 0; i < cps; i++) {
		if(!(i&1))
				*p++ = *dtl;
		else
				*p++ = *dth;
	}
	
	buf->user_buf.p = p;
	buf->user_buf.size -= cps;
	
	return cps;
}

static void usbdrv_trans(usbdrv_ep_t *ep)
{
	usbdrv_bulk_trans(ep);
}

static void usbdrv_bulk_tx(usbdrv_ep_t *ep)
{
	const usbdrv_ep_regs_t *regs = ep->regs;
	uint32_t send_size;
	uint32_t pkt_size;
	volatile uint32_t dummy;
	
	pkt_size = *regs->S & ep->pkts_msk;
	
	send_size = usbdrv_buf_send(&ep->buf,
		regs->DT, ep->max_pkts);
	
	if(send_size == 0) {
		/* 送るものがない */
		*regs->S &= ~EPS_DRQIE;
#ifdef USBDRV_CFG_SUPPORT_KOS
		if(ep->buf.tskid != 0) {
			kos_iwup_tsk(ep->buf.tskid);
			ep->buf.tskid = 0;
		}
#else
		ep->buf.flg = 1;
#endif
	} else if(send_size < ep->max_pkts) {
		/* 最終パケットを送信完了 */
		*regs->S &= ~EPS_DRQIE;
#ifdef USBDRV_CFG_SUPPORT_KOS
		if(ep->buf.tskid != 0) {
			kos_iwup_tsk(ep->buf.tskid);
			ep->buf.tskid = 0;
		}
#else
		ep->buf.flg = 1;
#endif
	} 
	
	*regs->S &= ~EPS_DRQ;
	dummy = *regs->C;
}

static void usbdrv_bulk_rx(usbdrv_ep_t *ep)
{
	const usbdrv_ep_regs_t *regs = ep->regs;
	uint32_t recv_size;
	uint32_t pkt_size;
	volatile uint32_t dummy;
	
	pkt_size = *regs->S & ep->pkts_msk;
	
	recv_size = usbdrv_buf_recv(&ep->buf,
		regs->DT, pkt_size);
	
	if(pkt_size < ep->max_pkts) {
		/* 全部受信完了 */
		*regs->S &= ~EPS_DRQIE;
#ifdef USBDRV_CFG_SUPPORT_KOS
		if(ep->buf.tskid != 0) {
			kos_iwup_tsk(ep->buf.tskid);
			ep->buf.tskid = 0;
		}
#else
		ep->buf.flg = 1;
#endif
	}
	
	*regs->S &= ~EPS_DRQ;
	dummy = *regs->C;
}

static void usbdrv_bulk_trans(usbdrv_ep_t *ep)
{
	if(ep->dir == USBDRV_EP_DIR_IN) {
		usbdrv_bulk_tx(ep);
	} else {
		usbdrv_bulk_rx(ep);
	}
}

static void usbdrv_intr_rx(usbdrv_ep_t *ep)
{
}

static void usbdrv_intr_tx(usbdrv_ep_t *ep)
{
}

static void usbdrv_intr_trans(usbdrv_ep_t *ep)
{
	if(ep->dir == USBDRV_EP_DIR_IN) {
		usbdrv_intr_tx(ep);
	} else {
		usbdrv_intr_rx(ep);
	}
}

void usbdrv_ep0_stall(usbdrv_dev_t *dev)
{
	dev->regs->EP0C_f.STAL = 1;
}

void usbdrv_ep0_begin_write(usbdrv_dev_t *dev,
	const uint8_t *buf, uint32_t size)
{
	dev->ep0i.buf.user_buf.p = (uint8_t *)buf;
	dev->ep0i.buf.user_buf.size = size;
	dev->setup_stat = USBDRV_SETUP_STAT_DATAIN;
	dev->regs->EP0IS_f.DRQIIE = 1;
}

void usbdrv_ep0_begin_read(usbdrv_dev_t *dev,
	uint8_t *buf, uint32_t size)
{
	dev->ep0o.buf.user_buf.p = (uint8_t *)buf;
	dev->ep0o.buf.user_buf.size = size;
	dev->setup_stat = USBDRV_SETUP_STAT_DATAOUT;
	dev->regs->EP0OS_f.DRQOIE = 1;
}

int32_t usbdrv_ep_read(usbdrv_ep_t *ep, uint8_t *buf, uint32_t size)
{
	if(size == 0) return 0;
	
	ep->buf.user_buf.p = (uint8_t *)buf;
	ep->buf.user_buf.size = size;
#ifdef USBDRV_CFG_SUPPORT_KOS
	kos_get_tid(&ep->buf.tskid);
#else
	ep->buf.flg = 0;
#endif
	*ep->regs->S |= EPS_DRQIE;
	
#ifdef USBDRV_CFG_SUPPORT_KOS
	kos_slp_tsk();
#else
	while(ep->buf.flg == 0) {
		__WFI();
	}
#endif
	
	return (int32_t)(size - ep->buf.user_buf.size);
}

int32_t usbdrv_ep_write(usbdrv_ep_t *ep, const uint8_t *buf, uint32_t size)
{
	if(size == 0) return 0;
	
	ep->buf.user_buf.p = (uint8_t *)buf;
	ep->buf.user_buf.size = size;
#ifdef USBDRV_CFG_SUPPORT_KOS
	kos_get_tid(&ep->buf.tskid);
#else
	ep->buf.flg = 0;
#endif
	*ep->regs->S |= EPS_DRQIE;
	
#ifdef USBDRV_CFG_SUPPORT_KOS
	kos_slp_tsk();
#else
	while(ep->buf.flg == 0) {
		__WFI();
	}
#endif
	
	return (int32_t)size;
}

static void usbdrv_bus_reset(usbdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	
	regs->UDCS_f.BRST = 0;
	
	/* バッファクリア */
	regs->EP0IS_f.BFINI = 1;
	regs->EP0OS_f.BFINI = 1;
	regs->EP1S_f.BFINI = 1;
	regs->EP2S_f.BFINI = 1;
	regs->EP3S |= 0x8000;
	regs->EP4S_f.BFINI = 1;
	regs->EP5S_f.BFINI = 1;
	
	regs->EP0IS_f.BFINI = 0;
	regs->EP0OS_f.BFINI = 0;
	regs->EP1S_f.BFINI = 0;
	regs->EP2S_f.BFINI = 0;
	regs->EP3S &= 0x7FFF;
	regs->EP4S_f.BFINI = 0;
	regs->EP5S_f.BFINI = 0;
	
}

