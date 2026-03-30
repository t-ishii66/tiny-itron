/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "addr.h"
#include "386.h"
#include "386P.h"

seg_t*	gdt = (seg_t*)AL_GDT;

/* set_gdt ------------------------------------------------------------------*/
void
set_gdt(short n, unsigned long base, unsigned long limit,
		unsigned char type, unsigned char dpl, unsigned char gda)
{
	n /= 8;
	(gdt + n)->base_l = (unsigned short)(base & 0xffff);
	(gdt + n)->base_m = (unsigned char)((base >> 16) & 0xff);
	(gdt + n)->base_h = (unsigned char)((base >> 24) & 0xff);
	(gdt + n)->limit_l = (unsigned short)(limit & 0xffff);
	(gdt + n)->limit_h = (unsigned char)
		((limit >> 16) & 0x0f) + gda;
	(gdt + n)->type = type | (dpl << 5);	
}

/* set_gate -----------------------------------------------------------------*/
void
set_gate(short n, unsigned long offset, unsigned short sel,
				unsigned char type, unsigned char count)
{
	gate_t*		gate;
	n /= 8;
	gate = (gate_t*)(gdt + n);
	gate->offset_l = offset & 0xffff;
	gate->sel = sel;
	gate->count = count;
	gate->type = type;
	gate->offset_h = (offset >> 16) & 0xffff;
}
