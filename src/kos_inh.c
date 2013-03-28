/*!
 *	@file	kos_inh.c
 *	@brief	interrupt handler API's implement.
 *
 *	Copyright (c) 2013 puchinya All rights reserved.<br>
 *	@b License BSD 2-Clause license
 *	@b Create 2013/03/28
 *	@author	puchinya
 */

#include "kos_local.h"

kos_er_t kos_def_inh(kos_intno_t intno, const kos_dinh_t *pk_dinh)
{
#ifdef KOS_CFG_ENA_PAR_CHK
	if(intno > KOS_MAX_INTNO) {
		return KOS_E_PAR;
	}
	if(pk_dinh) {
		if(pk_dinh->inthdr == KOS_NULL) {
			return KOS_E_PAR;
		}
	}
#endif
	
	kos_lock;
	if(pk_dinh) {
		g_kos_dinh_list[intno] = *pk_dinh;
	} else {
		g_kos_dinh_list[intno].inthdr = KOS_NULL;
	}
	kos_unlock;
	
	return KOS_E_OK;
}
