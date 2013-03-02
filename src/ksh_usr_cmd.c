
#include "ksh.h"
#include "kos.h"
#include <string.h>
#include <stdint.h>
#include "led_drv.h"

static void led_blink_start(void);
static void led_blink_stop(void);
static int usr_cmd_led_on(int argc, char *argv[]);
static int usr_cmd_led_off(int argc, char *argv[]);
static int usr_cmd_led_blink_on(int argc, char *argv[]);
static int usr_cmd_led_blink_off(int argc, char *argv[]);

const ksh_cmd g_ksh_usr_cmd_tbl[] =
{
	{ "ledon", usr_cmd_led_on },
	{ "ledoff", usr_cmd_led_off },
	{ "ledblinkon", usr_cmd_led_blink_on },
	{ "ledblinkoff", usr_cmd_led_blink_off },
	{ NULL, NULL }
};

static int usr_cmd_led_on(int argc, char *argv[])
{
	led_on(0);
	return 0;
}

static int usr_cmd_led_off(int argc, char *argv[])
{
	led_off(0);
	return 0;
}

static int s_blink = 0;

static void led_tsk(void)
{
	int b = 0;
	
	for(;;) {
		if(b) {
			led_on(0);
			b = 0;
		} else {
			led_off(0);
			b = 1;
		}
		if(!s_blink) {
			kos_slp_tsk();
		}
		kos_dly_tsk(50);
	}
}

static int s_led_tsk_stk[0x400/sizeof(int)];

const kos_ctsk_t s_ctsk_led_tsk =
{
	0,
	0,
	led_tsk,
	3,
	sizeof(s_led_tsk_stk),
	s_led_tsk_stk
};

static kos_id_t s_led_tsk_id = 0;

static void led_blink_start(void)
{
	if(s_blink) return;
	s_blink = 1;
	if(s_led_tsk_id == 0) {
		s_led_tsk_id = kos_cre_tsk(&s_ctsk_led_tsk);
		kos_act_tsk(s_led_tsk_id);
	} else {
		kos_wup_tsk(s_led_tsk_id);
	}
}

static void led_blink_stop(void)
{
	if(!s_blink) return;
	s_blink = 0;
}


static int usr_cmd_led_blink_on(int argc, char *argv[])
{
	led_blink_start();
	return 0;
}

static int usr_cmd_led_blink_off(int argc, char *argv[])
{
	led_blink_stop();
	return 0;
}
