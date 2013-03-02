
#ifndef __KSH_H__
#define __KSH_H__

#include <stddef.h>

#define CFG_KSH_MAX_ARGS		16
#define CFG_KSH_MAX_LINE_LEN	256

typedef int (*ksh_cmd_func)(int argc, char *argv[]);

typedef struct {
	const char		*name;
	ksh_cmd_func	func;
} ksh_cmd;

void ksh_start(void);
int ksh_getchar(void);
void ksh_putchar(int ch);
void ksh_printl(const char *s, int len);
void ksh_print(const char *s);
int ksh_readline(char *buf, size_t buf_size);

extern const ksh_cmd g_ksh_usr_cmd_tbl[];

#endif
