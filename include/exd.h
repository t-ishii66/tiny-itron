/* exd.h -- Extended (non-ITRON) user API */
#ifndef _EXD_H
#define _EXD_H
#include "itron.h"

void print_at(int row, int col, char *s, unsigned char attr);
void print_dec_at(int row, int col, unsigned long n, int width,
		unsigned char attr);
void clear_screen(void);
void fill_at(int row, int col, int len, int ch, unsigned char attr);
int  key_read(void);
void set_key_task(int id);
VP   tsk_stack_alloc(int size);

#endif
