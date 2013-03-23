
#include "kos_arch_if.h"

/*-----------------------------------------------------------------------------
	initialize rutine
-----------------------------------------------------------------------------*/
int main(void)
{
	kos_start_kernel();
	
	return 0;
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
#define DEF_INTR_CODE(funcname, intno) \
void funcname(void)\
{\
	kos_hal_if_exe_inth((kos_intno_t)(intno));\
}

DEF_INTR_CODE(USB0_Handler, USB0F_IRQn);
DEF_INTR_CODE(USB0F_Handler, USB0F_USB0H_IRQn);
DEF_INTR_CODE(USB1_Handler, USB1F_IRQn);
DEF_INTR_CODE(USB1F_Handler, USB1F_USB1H_IRQn);

void SysTick_Handler(void)
{
	kos_isig_tim();
}

/* do schedule */
__asm void PendSV_Handler(void)
{
	extern kos_schedule_nolock
	
	/* store R4-R11 to PSP */
	MRS			R0, PSP
	STMDB		R0!, {R4-R11}
	MSR			PSP, R0
	
	MOV			R11, LR
	BL			kos_schedule_nolock
	MOV			LR, R11
	
	/* load R4-R11 from PSP */
	MRS			R0, PSP
	LDMIA		R0!, {R4-R11}
	MSR			PSP, R0
	
	BX			LR
}

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
