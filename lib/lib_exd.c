/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* lib_exd.c -- Extended (non-ITRON) user-space syscall wrappers          */
#include "../include/itron.h"

void
print_at(int row, int col, char *s, unsigned char attr)
{
	syscall(-TFN_EXD_VGA_WRITE, row, col, s, attr);
}

void
print_dec_at(int row, int col, unsigned long n, int width,
		unsigned char attr)
{
	syscall(-TFN_EXD_VGA_DEC, row, col, n, width, attr);
}

void
clear_screen(void)
{
	syscall(-TFN_EXD_VGA_CLEAR);
}

void
fill_at(int row, int col, int len, int ch, unsigned char attr)
{
	syscall(-TFN_EXD_VGA_FILL, row, col, len, ch, attr);
}

int
key_read(void)
{
	return (int)syscall(-TFN_EXD_KEY_GETC);
}

void
set_key_task(int id)
{
	syscall(-TFN_EXD_KEY_SETTASK, id);
}

VP
tsk_stack_alloc(int size)
{
	return (VP)syscall(-TFN_EXD_STACK_ALLOC, size);
}
