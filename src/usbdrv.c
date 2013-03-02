
#include "usbdrv.h"
#include <string.h>

#define USBDRV_NUM_DEV	2

static usbdrv_dev_t s_dev[USBDRV_NUM_DEV];

static void usbdrv_clockon(void);
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

#define EPS_BFINI					(1<<15)
#define EPS_DRQIE					(1<<14)
#define EPS_SPKIE					(1<<13)
#define EPS_BUSY					(1<<11)
#define EPS_DRQ						(1<<10)
#define EPS_SPK						(1<<9)
#define EP1S_SIZE_MASK		0x1FF
#define EP2_5S_SIZE_MASK	0x7F


static const usbdrv_ep_regs_t s_usb0_ep1_regs =
{
	&FM3_USB0->EP1C,
	&FM3_USB0->EP1S,
	&FM3_USB0->EP1DT
};

static const usbdrv_ep_regs_t s_usb0_ep2_regs =
{
	&FM3_USB0->EP2C,
	&FM3_USB0->EP2S,
	&FM3_USB0->EP2DT
};

static const usbdrv_ep_regs_t s_usb0_ep3_regs =
{
	&FM3_USB0->EP3C,
	&FM3_USB0->EP3S,
	&FM3_USB0->EP3DT
};

static const usbdrv_ep_regs_t s_usb0_ep4_regs =
{
	&FM3_USB0->EP4C,
	&FM3_USB0->EP4S,
	&FM3_USB0->EP4DT
};

static const usbdrv_ep_regs_t s_usb0_ep5_regs =
{
	&FM3_USB0->EP5C,
	&FM3_USB0->EP5S,
	&FM3_USB0->EP5DT
};

static const usbdrv_ep_regs_t *s_usb0_all_ep_regs[] =
{
	&s_usb0_ep1_regs,
	&s_usb0_ep2_regs,
	&s_usb0_ep3_regs,
	&s_usb0_ep4_regs,
	&s_usb0_ep5_regs
};

int usbdrv_init(void)
{
	s_dev[0].regs = FM3_USB0;
	s_dev[1].regs = FM3_USB1;
	
	usbdrv_clockon();
#ifdef USBDRV_CFG_SUPPORT_KOS
	{
		kos_dinh_t dinh;
		
		dinh.inhatr = 0;
		
		dinh.inthdr = usbdrv_irq;
		dinh.exinf = &s_dev[0];
		kos_def_inh(USB0F_IRQn, &dinh);
		dinh.inthdr = usbdrv_irqf;
		dinh.exinf = &s_dev[0];
		kos_def_inh(USB0F_USB0H_IRQn, &dinh);
		dinh.inthdr = usbdrv_irq;
		dinh.exinf = &s_dev[1];
		kos_def_inh(USB1F_IRQn, &dinh);
		dinh.inthdr = usbdrv_irqf;
		dinh.exinf = &s_dev[1];
		kos_def_inh(USB1F_USB1H_IRQn, &dinh);
		
		kos_ena_int(USB0F_IRQn);
		kos_ena_int(USB0F_USB0H_IRQn);
		kos_ena_int(USB1F_IRQn);
		kos_ena_int(USB1F_USB1H_IRQn);
	}
#else
	NVIC_EnableIRQ(USB0F_IRQn);
	NVIC_EnableIRQ(USB0F_USB0H_IRQn);
	NVIC_EnableIRQ(USB1F_IRQn);
	NVIC_EnableIRQ(USB1F_USB1H_IRQn);
#endif
	
	return 0;
}

usbdrv_dev_t *usbdrv_create(int port)
{
	usbdrv_dev_t *dev;
	
	if(port < 0 || port >= USBDRV_NUM_DEV)
		return NULL;
	
	dev = &s_dev[port];
	if(dev->is_opened)
		return NULL;
	
	dev->is_opened = 1;
	return dev;
}

void usbdrv_begin_connect_setting(usbdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	const usbdrv_ep_regs_t **ep_regs = s_usb0_all_ep_regs;
	volatile int dummy;
	
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
		usbdrv_ep_t *ep = &dev->ep[0];
		ep->dev				= dev;
		ep->index			= 1;
		ep->pkts_msk	= USBDRV_EP1_PKTS_MSK;
		ep->regs			= ep_regs[0];
	}
	{
		usbdrv_ep_t *ep = &dev->ep[1];
		ep->dev				= dev;
		ep->index			= 2;
		ep->pkts_msk	= USBDRV_EP2_PKTS_MSK;
		ep->regs			= ep_regs[1];
	}
	{
		usbdrv_ep_t *ep = &dev->ep[2];
		ep->dev				= dev;
		ep->index			= 3;
		ep->pkts_msk	= USBDRV_EP3_PKTS_MSK;
		ep->regs			= ep_regs[2];
	}
	{
		usbdrv_ep_t *ep = &dev->ep[3];
		ep->dev				= dev;
		ep->index			= 4;
		ep->pkts_msk	= USBDRV_EP4_PKTS_MSK;
		ep->regs			= ep_regs[3];
	}
	{
		usbdrv_ep_t *ep = &dev->ep[4];
		ep->dev				= dev;
		ep->index			= 5;
		ep->pkts_msk	= USBDRV_EP5_PKTS_MSK;
		ep->regs			= ep_regs[4];
	}
}

int usbdrv_config_ep(usbdrv_dev_t *dev, const uint8_t *cfg_desc)
{
	unsigned int total_len;
	unsigned int i;
	const uint8_t *desc;
	
	desc = cfg_desc;
	total_len = ((unsigned int)cfg_desc[3] << 8) | cfg_desc[2];

	for(i = 0; i < total_len; ) {
		unsigned int desc_type, desc_len;
		
		desc_type = desc[1];
		desc_len = desc[0];
		
		if(desc_type == 0x05) {
			unsigned int ep_addr, ep_attrs, ep_max_pkts;
			usbdrv_ep_t *ep;
			unsigned int c;
			
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
			c |= ((unsigned int)ep->mode << 13);
			if(ep->dir == USBDRV_EP_DIR_IN)
				c |= 1<<12;
			*ep->regs->C = c;
		}
		
		i += desc_len;
		desc += desc_len;
	}
	
	return 0;
}

void usbdrv_end_connect_setting(usbdrv_dev_t *dev)
{
	volatile int dummy;
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

void usbdrv_destory(usbdrv_dev_t *dev)
{
	dev->is_opened = 0;
}

void INT8_31_Handler(void)
{
}

#ifndef USBDRV_CFG_SUPPORT_KOS
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

static void usbdrv_clockon(void)
{
	
	/* UCEN=0 を設定 */
	FM3_USBETHERNETCLK->UCCR_f.UCEN0 = 0;
	FM3_USBETHERNETCLK->UCCR_f.UCEN1 = 0;
	
	/* UCEN=1 になるまで待機 */
	while(FM3_USBETHERNETCLK->UCCR_f.UCEN0 != 0);
	while(FM3_USBETHERNETCLK->UCCR_f.UCEN1 != 0);
	
	/* UPLLEN=0 を設定 */
	FM3_USBETHERNETCLK->UPCR1_f.UPLLEN = 0;
	
	/* UCSEL を設定 */
	FM3_USBETHERNETCLK->UCCR_f.UCSEL0 = 1;
	FM3_USBETHERNETCLK->UCCR_f.UCSEL1 = 1;
	
	/* PLL発振クロックの場合(UCSEL=1) */
	
	/* UPINC を設定 */
	FM3_USBETHERNETCLK->UPCR1_f.UPINC = 0;
	/* UPOWT を設定 */
	FM3_USBETHERNETCLK->UPCR2 = 7;
	/* UPLLK を設定 */
	FM3_USBETHERNETCLK->UPCR3 = 0;
	/* UPLLN を設定 */
	FM3_USBETHERNETCLK->UPCR4 = 0x3B;
	/* UPLLM を設定 */
	FM3_USBETHERNETCLK->UPCR5 = 0x04;
	/* UPCSE=0 を設定 */
	FM3_USBETHERNETCLK->UPINT_ENR_f.UPCSE = 0;
	
	/* 割り込みは使わない */
	
	/* UPLLEN=0 を設定 */
	FM3_USBETHERNETCLK->UPCR1_f.UPLLEN = 1;
	
	/* UPRDY=1 になるまで待機 */
	while(FM3_USBETHERNETCLK->UP_STR_f.UPRDY != 1);
	
	FM3_USBETHERNETCLK->UCCR_f.UCEN0 = 1;
	FM3_USBETHERNETCLK->UCCR_f.UCEN1 = 1;
	
	/* 5cycle待つ */
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	
	FM3_USBETHERNETCLK->USBEN0 = 1;
	FM3_USBETHERNETCLK->USBEN1 = 1;
}

/* EP1-5 DRQ */
static void usbdrv_irqf(usbdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	uint16_t st;
	volatile int dummy;
	
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
	
	dev->ep0.stat = USBDRV_EP_STAT_SETUP;
	
	/* デバイスリクエストを読む */
	i = usvdrv_ctrl_read_device_request(dev, &device_request);
	
	/* リクエストを読み込んだので要因を解除 */
	regs->UDCS_f.SETP = 0;
	
	regs->EP0OS_f.DRQO = 0;
	
	if(i == 0) {
	
		/* デバイスリクエストを処理する */
		usbdrv_ctrl_process_device_request(dev, &device_request);
	
	} else {
		usbdrv_ep0_stall(dev);
	}
}

static int usvdrv_ctrl_read_device_request(usbdrv_dev_t *dev, usb_device_request_t *device_request)
{
	FM3_USB_TypeDef *regs = dev->regs;
	
	if((regs->EP0OS & 0x4F) != 8)
		return -1;
	
	*(uint16_t *)&device_request->bmRequestType = regs->EP0DT;
	device_request->wValue = regs->EP0DT;
	device_request->wIndex = regs->EP0DT;
	device_request->wLength = regs->EP0DT;
	
	return 0;
}

static void usbdrv_ctrl_process_device_request(usbdrv_dev_t *dev,
	const usb_device_request_t *device_request)
{
	dev->func_table.device_request_func(dev, device_request);
}

static void usbdrv_ctrl_ep0_rx(usbdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	unsigned int pkt_size;
	volatile int dummy;
	
	if(dev->ep0.stat == USBDRV_EP_STAT_DATAOUT) {
		pkt_size = regs->EP0OS & 0x7f;
		
		usbdrv_buf_recv(&dev->ep0.rx_buf,
			&regs->EP0DT, pkt_size);
		
		if(pkt_size < USBDRV_EP0_MAXSIZE) {
			regs->EP0IS_f.DRQIIE = 1;
			dev->ep0.stat = USBDRV_EP_STAT_HSOUT;
		}
	} else {
		dev->ep0.stat = USBDRV_EP_STAT_SETUP;
	}
	
	regs->EP0OS_f.DRQO = 0;
	dummy = regs->EP0C;
}

static void usbdrv_ctrl_ep0_tx(usbdrv_dev_t *dev)
{
	FM3_USB_TypeDef *regs = dev->regs;
	unsigned int send_size;
	unsigned int pkt_size;
	volatile int dummy;
	
	pkt_size = regs->EP0IS & 0x7f;
	
	send_size = usbdrv_buf_send(&dev->ep0.tx_buf,
		&regs->EP0DT, USBDRV_EP0_MAXSIZE);
	
	if(send_size == 0) {
		/* 送るものがない */
		regs->EP0IS_f.DRQIIE = 0;
		if(dev->ep0.stat == USBDRV_EP_STAT_DATAIN) {
			dev->ep0.stat = USBDRV_EP_STAT_HSOUT;
		} else {
			dev->ep0.stat = USBDRV_EP_STAT_SETUP;
		}
	} else if(send_size < USBDRV_EP0_MAXSIZE) {
		/* 最終パケットを送信完了 */
		regs->EP0IS_f.DRQIIE = 0;
		dev->ep0.stat = USBDRV_EP_STAT_HSIN;
	} else {
		regs->EP0IS_f.DRQIIE = 1;
	}
	
	regs->EP0IS_f.DRQI = 0;
	dummy = regs->EP0C;
}

static unsigned int usbdrv_buf_send(usbdrv_ep_buf_t *buf,
	__IO uint16_t *dt, unsigned int max_send_size)
{
	uint8_t *p = buf->user_buf.p;
	__IO uint8_t *dtl;
	__IO uint8_t *dth;
	unsigned int s = buf->user_buf.size;
	unsigned int cps = s;
	unsigned int i;
	
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

static unsigned int usbdrv_buf_recv(usbdrv_ep_buf_t *buf,
	__IO uint16_t *dt, unsigned int max_recv_size)
{
	uint8_t *p = buf->user_buf.p;
	__IO uint8_t *dtl;
	__IO uint8_t *dth;
	unsigned int s = buf->user_buf.size;
	unsigned int cps = s;
	unsigned int i;
	
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
	unsigned int send_size;
	unsigned int pkt_size;
	volatile int dummy;
	
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
	unsigned int recv_size;
	unsigned int pkt_size;
	volatile int dummy;
	
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
	const uint8_t *buf, unsigned int size)
{
	dev->ep0.tx_buf.user_buf.p = (uint8_t *)buf;
	dev->ep0.tx_buf.user_buf.size = size;
	dev->regs->EP0IS_f.DRQIIE = 1;
}

void usbdrv_ep0_begin_read(usbdrv_dev_t *dev,
	uint8_t *buf, unsigned int size)
{
	dev->ep0.rx_buf.user_buf.p = (uint8_t *)buf;
	dev->ep0.rx_buf.user_buf.size = size;
	dev->regs->EP0OS_f.DRQOIE = 1;
}

int usbdrv_ep_read(usbdrv_ep_t *ep, uint8_t *buf, unsigned int size)
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
	
	return (int)(size - ep->buf.user_buf.size);
}

int usbdrv_ep_write(usbdrv_ep_t *ep, const uint8_t *buf, unsigned int size)
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
	
	return (int)size;
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

