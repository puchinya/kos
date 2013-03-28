/* user initalize code. */
#include "kos.h"
#include "mcu.h"
#include "usbcdc.h"
#include "ksh.h"
#include "led_drv.h"
#include <string.h>

/* This function is called by kernel. */
void kos_usr_init(void)
{
	init_led();
	usbdrv_init();
	
	ksh_start();
}
