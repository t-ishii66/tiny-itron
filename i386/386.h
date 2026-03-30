/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _386_H
#define _386_H

/* seg_t --------------------------------------------------------------------*/
typedef struct seg {
	unsigned short	limit_l;
	unsigned short	base_l;
	unsigned char	base_m;
	unsigned char	type;
	unsigned char	limit_h;
	unsigned char	base_h;
} seg_t;

/* gate_t -------------------------------------------------------------------*/
typedef struct gate {
	unsigned short	offset_l;
	unsigned short	sel;
	unsigned char	count;
	unsigned char	type;
	unsigned short	offset_h;
} gate_t;

/* segment type -------------------------------------------------------------*/
#define ST_CODE		0x9a
#define ST_DATA		0x92
#define ST_STACK	0x96
#define ST_TSS		0x89
#define ST_CALL		0x84

/* other types --------------------------------------------------------------*/
#define _16BIT		0x00
#define _32BIT		0xc0

/* gate type ----------------------------------------------------------------*/
#define GT_INTR		0x8e
#define GT_TRAP 	0x8f
#define GT_CALL		0x8c

/* function declarations ----------------------------------------------------*/
void set_gdt(short, unsigned long, unsigned long, unsigned char,
		unsigned char, unsigned char);
void set_gate(short, unsigned long, unsigned short, unsigned char,
		unsigned char);
#endif
