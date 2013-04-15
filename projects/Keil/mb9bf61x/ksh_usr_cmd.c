/* ksh user command. */
#include <ksh.h>
#include <kos.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "led_drv.h"

static void led_blink_start(void);
static void led_blink_stop(void);
static int usr_cmd_led_on(int argc, char *argv[]);
static int usr_cmd_led_off(int argc, char *argv[]);
static int usr_cmd_led_blink_on(int argc, char *argv[]);
static int usr_cmd_led_blink_off(int argc, char *argv[]);
static int usr_cmd_test_dtq(int argc, char *argv[]);

const ksh_cmd g_ksh_usr_cmd_tbl[] =
{
	{ "ledon", usr_cmd_led_on },
	{ "ledoff", usr_cmd_led_off },
	{ "ledblinkon", usr_cmd_led_blink_on },
	{ "ledblinkoff", usr_cmd_led_blink_off },
	{ "test_dtq", usr_cmd_test_dtq },
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

static int s_dtq_tsk_stk[0x400/sizeof(int)];
static kos_id_t s_dtq_tsk_id;
static kos_id_t s_dtq_id;

static void dtq_tsk(void)
{
	kos_vp_int_t data;
	char s[32];
	for(;;) {
		kos_rcv_dtq(s_dtq_id, &data);
		sprintf(s, "rcv %d\r\n", data);
		ksh_printl(s, strlen(s));
	}
}

const kos_ctsk_t s_ctsk_dtq_tsk =
{
	0,
	0,
	dtq_tsk,
	3,
	sizeof(s_dtq_tsk_stk),
	s_dtq_tsk_stk
};


static kos_vp_int_t s_dtq_buf[16];

static int usr_cmd_test_dtq(int argc, char *argv[])
{
	int i;
	kos_cdtq_t cdtq;
	cdtq.dtqcnt = 16;
	cdtq.dtqatr = KOS_TA_TFIFO;
	cdtq.dtq = (kos_vp_t)s_dtq_buf;
	
	if(s_dtq_id == 0) {
		s_dtq_id = kos_cre_dtq(&cdtq);
	}
	
	if(s_dtq_tsk_id == 0) {
		s_dtq_tsk_id = kos_cre_tsk(&s_ctsk_dtq_tsk);
		kos_act_tsk(s_dtq_tsk_id);
	}
	
	for(i = 0; i < 20; i++) {
		kos_snd_dtq(s_dtq_id, (kos_vp_int_t)i);
		kos_dly_tsk(80);
	}
	
	return 0;
}
