
#ifndef __KOS_ARCH_IF_H__
#define __KOS_ARCH_IF_H__

#include "kos_local.h"

#define kos_hal_if_exe_inth(intno) \
do {\
	kos_fp_t inthdr;\
	kos_dinh_t *dinh;\
	\
	dinh = &g_kos_dinh_list[intno];\
	inthdr = dinh->inthdr;\
	if(inthdr) {\
		((void (*)(kos_vp_int_t))inthdr)(dinh->exinf);\
	}\
} while(0);

#endif
