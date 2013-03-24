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

__asm void kos_arch_idle(void)
{
LOOP
	B	LOOP
}

void kos_arch_setup_systick_handler(void)
{
	uint32_t period;
	
	period = SystemCoreClock / 100;	
	SysTick_Config(period);
	
	NVIC_SetPriority(PendSV_IRQn, 255);
	NVIC_SetPriority(SysTick_IRQn, 254);
}

/*-----------------------------------------------------------------------------
	CPU lock API's
-----------------------------------------------------------------------------*/
kos_er_t kos_loc_cpu(void)
{
	__disable_irq();
	return KOS_E_OK;
}

kos_er_t kos_iloc_cpu(void)
{
	__disable_irq();
	return KOS_E_OK;
}

kos_er_t kos_unl_cpu(void)
{
	__enable_irq();
	return KOS_E_OK;
}

kos_er_t kos_iunl_cpu(void)
{
	__enable_irq();
	return KOS_E_OK;
}

__asm kos_bool_t kos_sns_loc(void)
{
	MRS	R0, PRIMASK
	AND R0, #1
	BX	LR
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

#ifdef KOS_DISPATCHER_TYPE1
uint32_t g_kos_pendsv_ret_pc;

__asm void pendsv_after_proc(void)
{
	extern g_kos_pendsv_ret_pc
	extern kos_schedule_impl_nolock

	PUSH	{R0-R1}	/* dummy for PC */
	MRS		R0, APSR
	PUSH	{R0-R3, R12, LR}
	
	LDR		R0, =g_kos_pendsv_ret_pc
	LDR		R0, [R0]
	STR		R0, [SP, #28]
	
	BL		kos_schedule_impl_nolock
	
	POP		{R0-R3, R12, LR}
	MSR		APSR, R0
	CPSIE	i
	
	POP		{R0, PC}
}

__asm void PendSV_Handler(void)
{
	extern g_kos_pendsv_ret_pc
	
	CPSID		i
	MRS			R0, PSP
	/* 戻り先PCをg_kos_pendsv_ret_adrに退避 */
	LDR			R1, [R0, #24]
	/* PCの下位1bitを立てる */
	ORR			R1, #1
	LDR			R2, =g_kos_pendsv_ret_pc
	STR			R1, [R2]
	/* 戻り先PCを入れ替え */
	LDR			R2, =pendsv_after_proc
	STR			R2, [R0, #24]
	
	/* PSRを退避 */
	LDR			R1, [R0, #28]
	
	/* EXPRのITCをクリア */
	MOV			R2, #0x0000
	MOVT		R2, #0xF900
	AND			R1, R2
	STR			R1, [R0, #28]
	
	/* pendsv_after_procへ */
	BX			LR
	NOP
}

#else
/* do schedule */
__asm void PendSV_Handler(void)
{
	extern kos_schedule_impl_nolock
	
	CPSID		i
	
	/* store R4-R11 to PSP */
	MRS			R0, PSP
	STMDB		R0!, {R4-R11}
	MSR			PSP, R0
	
	MOV			R11, LR
	BL			kos_schedule_impl_nolock
	MOV			LR, R11
	
	/* load R4-R11 from PSP */
	MRS			R0, PSP
	LDMIA		R0!, {R4-R11}
	MSR			PSP, R0
	
	CPSIE		i
	
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

void USB0_Handler(void) {
	kos_arch_inthdr();
}

void USB0F_Handler(void) {
	kos_arch_inthdr();
}

void USB1_Handler(void) {
	kos_arch_inthdr();
}

void USB1F_Handler(void) {
	kos_arch_inthdr();
}
