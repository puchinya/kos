#include <stdio.h>
#include <mcu.h>

struct __FILE {
	int handle; /* Add whatever you need here */
};

FILE __stdout;
FILE __stdin;
volatile int32_t ITM_RxBuffer;

int fputc(int ch, FILE *f) {
	ITM_SendChar(ch);
	return(ch);
}

int fgetc(FILE *f)
{
	return ITM_ReceiveChar();
}
