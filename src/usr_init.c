/* user initalize code. */
#include <usbcdc.h>
#include <ksh.h>
#include <led_drv.h>

/* This function is called by kernel. */
void kos_usr_init(void)
{
	init_led();
	usbdrv_init();
	
	ksh_start();
}
