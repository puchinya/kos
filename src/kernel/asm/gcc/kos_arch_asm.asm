  .syntax unified
  .cpu cortex-m3
  .fpu softvfp
  .thumb

.global g_kos_pendsv_ret_pc
.global PendSV_Handler
.global kos_schedule_impl_nolock
.global kos_arch_swi_ctx

.section .text
.type pendsv_after_proc, %function
.type PendSV_Handler, %function
.type kos_arch_swi_ctx, %function

kos_arch_swi_ctx:
	PUSH	{R4-R11,LR}
	LDR	R1, [R1, #16]
	STR	SP, [R0, #16]
	MOV	SP, R1
	POP	{R4-R11,LR}
	BX		LR

pendsv_after_proc:
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

PendSV_Handler:
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


.section	.bss
.align		4

g_kos_pendsv_ret_pc:
	.word	0
