
#ifndef __KOS_ARCH_CFG_H__
#define __KOS_ARCH_CFG_H__

#define KOS_ARCH_CFG_SPT_SYSTICK	/* support cpu internal tick handler */
#define KOS_ARCH_CFG_SPT_PENDSV		/* support PendSV handler */
#define KOS_ARCH_CFG_SPT_IRQPRI		/* support interrupt priority */
#define KOS_ARCH_MIN_INTPRI	0
#define KOS_ARCH_MAX_INTPRI	KOS_ARCH_CFG_SPT_SYSTICK

#endif
