/*!
 *	@file	usbhdrv.c
 *	@brief	usb host driver for FM3 Microcontoroller.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "usbhdrv.h"
#include "usbdrvcmn_local.h"
#include <string.h>

static usbhdrv_dev_t s_devh[USBDRV_NUM_DEV];
static void usbhdrv_irq(usbhdrv_dev_t *dev);
static void usbhdrv_irqh(usbhdrv_dev_t *dev);
static void usbhdrv_setup_regs(usbhdrv_dev_t *dev);

usbhdrv_dev_t *usbhdrv_open(uint32_t port)
{
	usbhdrv_dev_t *dev;
	
	if(port >= USBDRV_NUM_DEV)
		return NULL;
	
	if(!usbdrv_local_use(port))
		return NULL;
	
	dev = &s_devh[port];
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
		
		dinh.inthdr = usbhdrv_irq;
		kos_def_inh(usb_intno, &dinh);
		dinh.inthdr = usbhdrv_irqh;
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
	
	usbhdrv_setup_regs(dev);
	
	return dev;
}

usbdrv_er_t usbhdrv_close(usbhdrv_dev_t *dev)
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
	
	return USBDRV_E_OK;
}

static void usbhdrv_setup_regs(usbhdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	volatile uint32_t dummy;
	
	if(dev->port == 0) {
	} else {
		/* UDM1とUDMP1ピンをUSBとして使う */
		FM3_GPIO->SPSR_f.USB1C = 1;
		FM3_GPIO->PFR8_f.P2 = 1;
		FM3_GPIO->PFR8_f.P3 = 1;
		
		//FM3_GPIO->DDR6_f.P2 = 1;
		//FM3_GPIO->PDOR6_f.P2 = 1;
	}
	
	/* USBファンクションをリセット */
	regs->UDCC_f.RST = 1;
	
	/* USBホストとして使う */
	regs->UDCC_f.HCONX = 1;
	regs->HCNT0_f.HOST = 1;
	while(regs->HCNT0_f.HOST == 0);
	
	/* 全USBホスト割り込みを無効化 */
	regs->HCNT1_f.SOFSTEP = 0;
	regs->HCNT1_f.CANCEL = 0;
	regs->HCNT1_f.RETRY = 0;
	regs->HCNT0_f.RWKIRE = 0;
	regs->HCNT0_f.URIRE = 0;
	regs->HCNT0_f.CMPIRE = 0;
	regs->HCNT0_f.CNNIRE = 0;
	regs->HCNT0_f.DIRE = 0;
	regs->HCNT0_f.SOFIRE = 0;
}

usbdrv_er_t usbhdrv_wait_for_device(usbhdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	volatile uint32_t dummy;
	int32_t tmode;
	
	/* デバイスの接続待ち */
	while(regs->HIRQ_f.CNNIRQ == 0) {
		kos_dly_tsk(1);
	}
	
	/* スピードを設定 */
	tmode = regs->HSTATE_f.TMODE;
	
	regs->HSTATE_f.CLKSEL = tmode;
	dummy = regs->HSTATE;
	
	/* 一致するまで待機 */
	while(regs->HSTATE_f.CLKSEL != tmode);
	
	/* USBファンクションのリセットを解除 */
	regs->UDCC_f.RST = 0;
	
	/* バスリセットを発行 */
	regs->HCNT0_f.URST = 1;
	
	/* バスリセット完了待ち */
	while(regs->HIRQ_f.URIRQ == 0) {
		kos_dly_tsk(1);
	}
	while(regs->HIRQ_f.CNNIRQ == 0) {
		kos_dly_tsk(1);
	}
	
	regs->UDCC_f.RST = 1;
	
	tmode = regs->HSTATE_f.TMODE;
	
	regs->HSTATE_f.CLKSEL = tmode;
	
	/* 一致するまで待機 */
	while(regs->HSTATE_f.CLKSEL != tmode);
	
	/* エンドポイントの設定 */
	regs->EP1C = 0xC440;
	regs->EP2C = 0xD440;
	
	/* リセットを解除 */
	regs->UDCC_f.RST = 0;
	
	regs->EP1S_f.BFINI = 0;
	regs->EP2S_f.BFINI = 0;
	
	regs->HCNT1_f.RETRY = 1;
	
	regs->HFCOMP = 0;
	regs->HRTIMER = 0xFFFF;
	regs->HRTIMER1 = 0x3;
	
	/* ホストアドレスを0に設定 */
	regs->HADR = 0;
	
	regs->HEOF = 0x2C9;
	
	return USBDRV_E_OK;
}

usbdrv_er_t usbhdrv_setup_transaction(usbhdrv_dev_t *dev,
	const usb_device_request_t *device_request)
{
	FM3_USB_TypeDef *regs = dev->regs;
	
	regs->EP2S_f.BFINI = 0;
	
	regs->EP2DT = *(uint16_t *)&device_request->bmRequestType;
	regs->EP2DT = device_request->wValue;
	regs->EP2DT = device_request->wIndex;
	regs->EP2DT = device_request->wLength;
	regs->EP2S_f.DRQ = 0;
	
	regs->HADR = 0;
	regs->HTOKEN = 0x10;
	
	/* 完了待ち */
	while(regs->HIRQ_f.CMPIRQ == 0);
	
	if(regs->HERR_f.LSTOF)
		return USBDRV_E_USB_LSTSOF;
	if(regs->HERR_f.TOUT)
		return USBDRV_E_TMOUT;
	switch(regs->HERR & 0x3) {
	case 0: /* ACK */
		break;
	case 1:
		return USBDRV_E_USB_NAK;
	case 2:
		return USBDRV_E_USB_STALL;
	case 3:
		return USBDRV_E_USB_NULL;
	}
	
	return USBDRV_E_OK;
}

usbdrv_er_t usbhdrv_begin_in_transaction(usbhdrv_dev_t *dev,
	uint32_t ep_no)
{
	FM3_USB_TypeDef *regs = dev->regs;
	
	regs->EP1S_f.BFINI = 0;
	
	dev->toggle = 0x80;
	dev->trans_size = 0;
	dev->last_recv_size = 0;
	
	return USBDRV_E_OK;
}

usbdrv_er_t usbhdrv_end_in_transaction(usbhdrv_dev_t *dev,
	uint32_t ep_no)
{
	return USBDRV_E_OK;
}

usbdrv_er_t usbhdrv_in_transaction(usbhdrv_dev_t *dev,
	uint32_t ep_no, uint16_t *buf, uint32_t buf_size)
{
	//usbdrv_abstract_ep_t *ep;
	FM3_USB_TypeDef *regs = dev->regs;
	uint32_t i = 0;
	uint32_t recv_size, read_size;
	
	//if(ep_no == 0) {
	//	ep = (usbdrv_abstract_ep_t *)&dev->ep0i;
	//} else {
	//	ep = (usbdrv_abstract_ep_t *)&dev->ep[ep_no - 1];
	//}
	
	for(;;) {
		if(dev->trans_size == 0) {
			regs->HTOKEN = 0x20 | dev->toggle;
			dev->toggle = dev->toggle == 0 ? 0x80 : 0x00;
			
			/* 完了待ち */
			while(regs->HIRQ_f.CMPIRQ == 0);
			
			if(regs->HERR_f.LSTOF)
				return USBDRV_E_USB_LSTSOF;
			if(regs->HERR_f.TOUT)
				return USBDRV_E_TMOUT;
			if(regs->HERR_f.TGERR)
				return USBDRV_E_USB_TG;
			switch(regs->HERR & 0x3) {
			case 0: /* ACK */
				break;
			case 1:
				return USBDRV_E_USB_NAK;
			case 2:
				return USBDRV_E_USB_STALL;
			case 3:
				return USBDRV_E_USB_NULL;
			}
			
			recv_size = regs->EP1S & 0x1FF;
			dev->last_recv_size = recv_size;
		} else {
			recv_size = dev->trans_size;
		}
		
		read_size = recv_size;
		if(read_size > buf_size)
			read_size = buf_size;
		
		for( ; i < read_size; i += 2) {
			buf[i] = regs->EP1DT;
		}
		buf_size -= read_size;
		
		dev->trans_size = recv_size - read_size;
		
		if(dev->trans_size == 0) {
			regs->EP1S_f.DRQ = 0;
			if(dev->last_recv_size >= 0x40) {
				continue;
			}
		}
		break;
	}
	
	return USBDRV_E_OK;
}

usbdrv_er_t usbhdrv_out_transaction(usbhdrv_dev_t *dev,
	uint32_t ep_no, const uint16_t *data, uint32_t data_size)
{
	FM3_USB_TypeDef *regs = dev->regs;
	uint32_t i = 0;
	
	do {
		for( ; i < data_size; i += 2) {
			regs->EP0DT = data[i];
		}
		regs->EP0OS_f.DRQO = 0;
		regs->HTOKEN = 0x30;
		
		/* 完了待ち */
		while(!regs->HIRQ_f.CMPIRQ);
		
		if(regs->HERR_f.LSTOF)
			return USBDRV_E_USB_LSTSOF;
		if(regs->HERR_f.TOUT)
			return USBDRV_E_TMOUT;
		if(regs->HERR_f.HS0)
			return USBDRV_E_USB_HS;
		if(regs->HERR_f.HS1)
			return USBDRV_E_USB_HS;
	} while(data_size > 0);
	
	return USBDRV_E_OK;
}

usbdrv_er_t usbhdrv_start(usbhdrv_dev_t *dev)
{
	usbdrv_er_t er;
	
	/* デバイスの接続を待つ*/
	er = usbhdrv_wait_for_device(dev);
	
	if(er != USBDRV_E_OK) {
		return er;
	}
	
	/* デバイスディスクプリタを取得 */
	{
		usb_device_request_t device_request;
		uint16_t data[0x12/2];
		
		device_request.bmRequestType = USB_REQUEST_TYPE_HOST_TO_DEVICE;
		device_request.bRequest = USB_REQUEST_CODE_GET_DESCRIPTOR;
		device_request.wValue = USB_DESCRIPTOR_TYPE_DEVICE;
		device_request.wIndex = 0;
		device_request.wLength = 0x12;
		
		usbhdrv_setup_transaction(dev, &device_request);
		usbhdrv_begin_in_transaction(dev, 0);
		usbhdrv_in_transaction(dev, 0, data, 0x12);
		usbhdrv_end_in_transaction(dev, 0);
		
	}

	return USBDRV_E_OK;
}

static void usbhdrv_irq(usbhdrv_dev_t *dev)
{
}

static void usbhdrv_irqh(usbhdrv_dev_t *dev)
{
}
