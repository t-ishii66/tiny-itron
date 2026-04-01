/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* sys_exd.c -- Extended (non-ITRON) syscall handlers                     */
#include "../include/itron.h"
#include "../include/stdio.h"
#include "../i386/video.h"
#include "../i386/keyboard.h"
#include "../kernel/pool.h"

/* sys_vga_write_at --------------------------------------------------------*/
ER
sys_vga_write_at(W apic, int row, int col, char *s, unsigned char attr)
{
	vga_write_at(row, col, s, attr);
	return E_OK;
}

/* sys_vga_write_dec_at ----------------------------------------------------*/
ER
sys_vga_write_dec_at(W apic, int row, int col, unsigned long n,
		int width, unsigned char attr)
{
	vga_write_dec_at(row, col, n, width, attr);
	return E_OK;
}

/* sys_vga_clear -----------------------------------------------------------*/
ER
sys_vga_clear(W apic)
{
	vga_clear();
	return E_OK;
}

/* sys_vga_fill_at ---------------------------------------------------------*/
ER
sys_vga_fill_at(W apic, int row, int col, int len, int ch,
		unsigned char attr)
{
	unsigned short *p = (unsigned short *)0xB8000 + row * 80 + col;
	int i;
	for (i = 0; i < len && col + i < 80; i++)
		p[i] = (unsigned short)attr << 8 | (unsigned char)ch;
	return E_OK;
}

/* sys_key_getc_sc ---------------------------------------------------------*/
/* No longer used — keyboard input is now delivered via DTQ.                */
ER
sys_key_getc_sc(W apic)
{
	return E_NOSPT;
}

/* sys_key_set_task --------------------------------------------------------*/
ER
sys_key_set_task(W apic, int task_id)
{
	key_dtq_id = task_id;
	return E_OK;
}

/* sys_vga_set_cursor ------------------------------------------------------*/
ER
sys_vga_set_cursor(W apic, int row, int col)
{
	vga_set_cursor(row, col);
	return E_OK;
}

/* sys_stack_alloc_sc ------------------------------------------------------*/
ER
sys_stack_alloc_sc(W apic, int size)
{
	return (ER)stack_alloc((SIZE)size);
}
