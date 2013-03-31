/*!
 *	@file	usbdrvcmn.c
 *	@brief	usb device/host common file.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "usbdrvcmn_local.h"

static uint16_t s_usbdrv_usemsk;
static uint16_t	s_usbdrv_clockmsk;

static void usbdrv_local_clockon_impl(void);
static void usbdrv_local_clockoff_impl(void);

static const usbdrv_ep_regs_t s_usb0_ep0i_regs =
{
	&FM3_USB0->EP0C,
	&FM3_USB0->EP0IS,
	&FM3_USB0->EP0DT
};

static const usbdrv_ep_regs_t s_usb0_ep0o_regs =
{
	&FM3_USB0->EP0C,
	&FM3_USB0->EP0OS,
	&FM3_USB0->EP0DT
};

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

const usbdrv_ep_regs_t *g_usb0_all_ep_regs[] =
{
	&s_usb0_ep0i_regs,
	&s_usb0_ep0o_regs,
	&s_usb0_ep1_regs,
	&s_usb0_ep2_regs,
	&s_usb0_ep3_regs,
	&s_usb0_ep4_regs,
	&s_usb0_ep5_regs
};


static const usbdrv_ep_regs_t s_usb1_ep0i_regs =
{
	&FM3_USB1->EP0C,
	&FM3_USB1->EP0IS,
	&FM3_USB1->EP0DT
};

static const usbdrv_ep_regs_t s_usb1_ep0o_regs =
{
	&FM3_USB1->EP0C,
	&FM3_USB1->EP0OS,
	&FM3_USB1->EP0DT
};

static const usbdrv_ep_regs_t s_usb1_ep1_regs =
{
	&FM3_USB1->EP1C,
	&FM3_USB1->EP1S,
	&FM3_USB1->EP1DT
};

static const usbdrv_ep_regs_t s_usb1_ep2_regs =
{
	&FM3_USB1->EP2C,
	&FM3_USB1->EP2S,
	&FM3_USB1->EP2DT
};

static const usbdrv_ep_regs_t s_usb1_ep3_regs =
{
	&FM3_USB1->EP3C,
	&FM3_USB1->EP3S,
	&FM3_USB1->EP3DT
};

static const usbdrv_ep_regs_t s_usb1_ep4_regs =
{
	&FM3_USB1->EP4C,
	&FM3_USB1->EP4S,
	&FM3_USB1->EP4DT
};

static const usbdrv_ep_regs_t s_usb1_ep5_regs =
{
	&FM3_USB1->EP5C,
	&FM3_USB1->EP5S,
	&FM3_USB1->EP5DT
};

const usbdrv_ep_regs_t *g_usb1_all_ep_regs[] =
{
	&s_usb1_ep0i_regs,
	&s_usb1_ep0o_regs,
	&s_usb1_ep1_regs,
	&s_usb1_ep2_regs,
	&s_usb1_ep3_regs,
	&s_usb1_ep4_regs,
	&s_usb1_ep5_regs
};

usbdrv_er_t usbdrv_init(void)
{
	s_usbdrv_clockmsk = 0;
	
	return USBDRV_E_OK;
}

int32_t usbdrv_local_use(uint32_t port)
{
	uint16_t msk;
	
	msk = 1 << port;
	if(s_usbdrv_usemsk & msk)
		return 0;
	
	s_usbdrv_usemsk |= msk;
	
	return 1;
}

int32_t usbdrv_local_unuse(uint32_t port)
{
	uint16_t msk;
	
	msk = 1 << port;
	s_usbdrv_usemsk &= ~msk;
	
	return 1;
}

void usbdrv_local_clockon(uint32_t port)
{
	uint16_t msk;
	msk = 1 << port;
	if(s_usbdrv_clockmsk == 0) {
		usbdrv_local_clockon_impl();
	}
	s_usbdrv_clockmsk |= msk;
}

void usbdrv_local_clockoff(uint32_t port)
{
	uint16_t msk;
	msk = 1 << port;
	s_usbdrv_clockmsk &= ~msk;
	if(s_usbdrv_clockmsk == 0) {
		usbdrv_local_clockoff_impl();
	}
}

static void usbdrv_local_clockon_impl(void)
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

static void usbdrv_local_clockoff_impl(void)
{
}
