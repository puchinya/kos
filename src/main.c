
#include "mcu.h"
#include "usbcdc.h"
#include "kos.h"
#include "ksh.h"
#include "led_drv.h"
#include <string.h>

void init_task(void)
{
	init_led();
	usbdrv_init();
	
	ksh_start();
}
