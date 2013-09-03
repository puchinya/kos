
#include <kos_sys_cfg.h>

	.syntax unified
	.cpu cortex-m3
	.fpu softvfp
	.thumb

	.global	kos_schedule_impl_nolock

	.section	.text
	.align	2

.global	PendSV_Handler
.thumb
.thumb_func
.type	PendSV_Handler, %function

PendSV_Handler:
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
	.size	PendSV_Handler, .-PendSV_Handler
