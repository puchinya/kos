/*!
 *	@file	kos_arch.h
 *	@brief	Cortex-M3 archtecture part header file.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#ifndef __KOS_ARCH_H__
#define __KOS_ARCH_H__

#include <mcu.h>

void kos_arch_setup_systick_handler(void);
void kos_arch_idle(void);
#define kos_arch_pend_sv()	\
do { \
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; \
	__DSB(); \
} while(0);

#define kos_arch_loc_cpu()	__disable_irq()
#define kos_arch_unl_cpu()	__enable_irq()
#define kos_arch_sns_loc()	(__get_PRIMASK() & 1)

#define kos_arch_swi_sp(p_store_sp, load_sp) \
do { \
	*(p_store_sp) = (void *)__get_PSP(); \
	__set_PSP((uint32_t)(load_sp)); \
} while(0);

#endif
