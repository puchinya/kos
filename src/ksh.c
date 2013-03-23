
#include "ksh.h"
#include "kos.h"
#include "usbcdc.h"
#include <string.h>

static usbcdc_t *s_cdc;
static char s_line_buf[CFG_KSH_MAX_LINE_LEN+1];
static char *s_argv[CFG_KSH_MAX_ARGS];

static int ksh_exec_cmd(char *cmd, int cmd_len);
static int ksh_parse_cmd_arg(char *cmd, char *argv[], int max_args);
static const ksh_cmd *ksh_find_cmd(const char *cmd_name);

int ksh_getchar(void)
{
	uint8_t b[1];
	usbcdc_read(s_cdc, b, 1);
	return (int)b[0];
}

void ksh_putchar(int ch)
{
	uint8_t b[1];
	b[0] = (uint8_t)ch;
	usbcdc_write(s_cdc, b, 1);
}

void ksh_printl(const char *s, int len)
{
	usbcdc_write(s_cdc, s, len);
}


void ksh_print(const char *s)
{
	ksh_printl(s, strlen(s));
}

int ksh_readline(char *buf, size_t buf_size)
{
	int ch;
	int len = 0;
	int max_len = (int)buf_size - 1;
	
	for(;;) {
		ch = ksh_getchar();
		switch(ch) {
		case '\r':
			ksh_printl("\r\n", 2);
			goto end;
		case '\n':
			break;
		case '\b':
			if(len > 0) {
				ksh_printl("\b \b", 3);
				len--;
			}
			break;
		default:
			if(len < max_len) {
				buf[len++] = ch;
				ksh_putchar(ch);
			}
			break;
		}
	}
end:
	buf[len] = '\0';
	return len;
}

static int ksh_parse_cmd_arg(char *cmd, char *argv[], int max_args)
{
	char *prev = cmd;
	int cnt = 0;
	
	for(;;) {
		char *next = prev;
		int last = 0;
		
		for(;;) {
			int ch = *next;
			if(ch == ' ' || ch == '\0') {
				if(ch == '\0') last = 1;
				*next = '\0';
				next++;
				break;
			}
			next++;
		}
		
		argv[cnt] = prev;
		cnt++;
		if(last || cnt >= max_args) break;
	}
	return cnt;
}

static int ksh_cmd_ver(int argc, char *argv[]);
static int ksh_cmd_help(int argc, char *argv[]);

static const ksh_cmd s_cmd_tbl[] =
{
	{ "ver", ksh_cmd_ver },
	{ "help", ksh_cmd_help },
	{ NULL, NULL }
};

static const ksh_cmd *ksh_find_cmd(const char *cmd_name)
{
	const ksh_cmd *cmd = NULL;
	const ksh_cmd *tbl = s_cmd_tbl;
	for(;;) {
		const char *name = tbl->name;
		if(!name) break;
		if(strcmp(name, cmd_name) == 0) {
			cmd = tbl;
			break;
		}
		tbl++;
	}
	
	if(!cmd) {
		tbl = g_ksh_usr_cmd_tbl;
		for(;;) {
			const char *name = tbl->name;
			if(!name) break;
			if(strcmp(name, cmd_name) == 0) {
				cmd = tbl;
				break;
			}
			tbl++;
		}
	}
	return cmd;
}

static int ksh_cmd_ver(int argc, char *argv[])
{
	ksh_print("ksh version 1.0\r\n");
	return 0;
}

static int ksh_cmd_help(int argc, char *argv[])
{
	const ksh_cmd *tbl;
	tbl	= s_cmd_tbl;
	for(;;) {
		if(!tbl->name) break;
		ksh_print(tbl->name);
		ksh_printl("\r\n", 2);
		tbl++;
	}
	tbl	= g_ksh_usr_cmd_tbl;
	for(;;) {
		if(!tbl->name) break;
		ksh_print(tbl->name);
		ksh_printl("\r\n", 2);
		tbl++;
	}
	return 0;
}

static int ksh_exec_cmd(char *cmd, int cmd_len)
{
	int argc;
	char *cmd_name;
	
	argc = ksh_parse_cmd_arg(cmd, s_argv, CFG_KSH_MAX_ARGS);
	if(argc > 0) {
		const ksh_cmd *cmd;
		cmd_name = s_argv[0];
		
		cmd = ksh_find_cmd(cmd_name);
		
		if(cmd) {
			cmd->func(argc, s_argv);
		} else {
			ksh_print("invalid command ''");
			ksh_print(cmd_name);
			ksh_print("'\r\n");
		}
	}
	return 0;
}

static void ksh_tsk(void)
{
	int len;
	s_cdc = usbcdc_create(0);
	usbcdc_open(s_cdc);
	
	ksh_getchar();
	ksh_print("ksh version 1.0\r\n");
	for(;;) {
		ksh_putchar('>');
		len = ksh_readline(s_line_buf, CFG_KSH_MAX_LINE_LEN+1);
		if(len > 0) {
			ksh_exec_cmd(s_line_buf, len);
		}
	}
}

static int s_ksh_tsk_stk[0x400/sizeof(int)];

const kos_ctsk_t s_ctsk_ksh_tsk =
{
	0,
	0,
	ksh_tsk,
	2,
	sizeof(s_ksh_tsk_stk),
	s_ksh_tsk_stk
};

static kos_id_t s_ksh_tsk_id = 0;

void ksh_start(void)
{
	s_ksh_tsk_id = kos_cre_tsk(&s_ctsk_ksh_tsk);
	kos_act_tsk(s_ksh_tsk_id);
}
