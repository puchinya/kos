
#include "led_drv.h"
#include <mcu.h>

void init_led(void)
{
	FM3_GPIO->PFRF_f.P3 = 0;
	FM3_GPIO->PZRF_f.P3 = 1;
	FM3_GPIO->DDRF_f.P3 = 1;
	FM3_GPIO->PDORF_f.P3 = 0;
}

void led_on(int no)
{
	FM3_GPIO->PDORF_f.P3 = 0;
}

void led_off(int no)
{
	FM3_GPIO->PDORF_f.P3 = 1;
}
