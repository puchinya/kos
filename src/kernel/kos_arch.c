/*!
 *	@file	kos_arch.c
 *	@brief	archtecuture part.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/23
 *	@author	puchinya
 */

#include "kos_arch_if.h"

/*-----------------------------------------------------------------------------
	initialize rutine
-----------------------------------------------------------------------------*/
int main(void)
{
	kos_start_kernel();
	
	return 0;
}

#ifdef __GNUC__
void kos_arch_idle(void)
{
	for(;;) {
		__NOP();
	}
}
#else
__asm void kos_arch_idle(void)
{
LOOP
	B	LOOP
}
#endif

void kos_arch_setup_systick_handler(void)
{
	uint32_t period;
	
	NVIC_SetPriority(PendSV_IRQn, KOS_ARCH_PRENSV_PRI);
	
	period = SystemCoreClock / 2 / 100;
	SysTick_Config(period);
	
	NVIC_SetPriority(SysTick_IRQn, KOS_ARCH_SYSTICK_PRI);
}

/*-----------------------------------------------------------------------------
	CPU lock API's
-----------------------------------------------------------------------------*/
kos_er_t kos_loc_cpu(void)
{
	kos_arch_loc_cpu();
	return KOS_E_OK;
}

kos_er_t kos_iloc_cpu(void)
{
	kos_arch_loc_cpu();
	return KOS_E_OK;
}

kos_er_t kos_unl_cpu(void)
{
	kos_arch_unl_cpu();
	return KOS_E_OK;
}

kos_er_t kos_iunl_cpu(void)
{
	kos_arch_unl_cpu();
	return KOS_E_OK;
}

kos_bool_t kos_sns_loc(void)
{
	return (kos_bool_t)kos_arch_sns_loc();
}

/*-----------------------------------------------------------------------------
	archtecuture inplement API
-----------------------------------------------------------------------------*/
kos_er_t kos_dis_int(kos_intno_t intno)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(intno > KOS_MAX_INTNO) {
		return KOS_E_PAR;
	}
#endif
	
	NVIC_DisableIRQ((IRQn_Type)intno);
	
	return KOS_E_OK;
}

kos_er_t kos_ena_int(kos_intno_t intno)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(intno > KOS_MAX_INTNO) {
		return KOS_E_PAR;
	}
#endif
	
	NVIC_EnableIRQ((IRQn_Type)intno);
	
	return KOS_E_OK;
}

kos_er_t kos_vchg_intpri(kos_intno_t intno, kos_intpri_t intpri)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(intno > KOS_MAX_INTNO) {
		return KOS_E_PAR;
	}
#endif

	NVIC_SetPriority((IRQn_Type)intno, (uint32_t)intpri);
	
	return KOS_E_OK;
}

kos_er_t kos_vget_intpri(kos_intno_t intno, kos_intpri_t *p_intpri)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(intno > KOS_MAX_INTNO || p_intpri == KOS_NULL) {
		return KOS_E_PAR;
	}
#endif

	*p_intpri = NVIC_GetPriority((IRQn_Type)intno);
	
	return KOS_E_OK;
}

kos_er_t kos_chg_imsk(kos_intpri_t imsk)
{
	__set_BASEPRI(imsk);
	
	return KOS_E_OK;
}

kos_er_t kos_get_imsk(kos_intpri_t *p_imsk)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(p_imsk == KOS_NULL) {
		return KOS_E_PAR;
	}
#endif
	*p_imsk = __get_BASEPRI();
	
	return KOS_E_OK;
}

/*-----------------------------------------------------------------------------
	interrupt rutines
-----------------------------------------------------------------------------*/
void kos_arch_inthdr(void)
{
	kos_intno_t intno = (__get_IPSR() & 0x1FF) - 0x10;
	kos_hal_if_exe_inth(intno);
}

void SysTick_Handler(void)
{
	kos_isig_tim();
}

#ifndef __GNUC__
/* do schedule */
__asm void PendSV_Handler(void)
{
	extern kos_schedule_impl_nolock
	
#ifndef KOS_CFG_FAST_IRQ
	CPSID		i
#endif
	
	/* store R4-R11 to PSP */
	MRS			R0, PSP
	STMDB		R0!, {R4-R11}
	MSR			PSP, R0
	
	MOV			R11, LR
#ifdef KOS_CFG_FAST_IRQ
	BL			kos_local_process_dly_svc
#endif
	BL			kos_schedule_impl_nolock
	MOV			LR, R11
	
	/* load R4-R11 from PSP */
	MRS			R0, PSP
	LDMIA		R0!, {R4-R11}
	MSR			PSP, R0
	
#ifndef KOS_CFG_FAST_IRQ
	CPSIE		i
#endif
	
	BX			LR
}
#endif

void NMI_Handler(void)
{
	__NOP();
}

void HardFault_Handler(void)
{
	__NOP();
}

void MemManage_Handler(void)
{
	__NOP();
}

void BusFault_Handler(void)
{
	__NOP();
}

void UsageFault_Handler(void)
{
	__NOP();
}
