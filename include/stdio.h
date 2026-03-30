/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _STDIO_H
#define _STDIO_H

#include "itron.h"

#define ALLOCATION_SIZE		4096

#define MEM_FLAG_ALLOC		0x01
#define MEM_FLAG_FREE		0x00
#define MEM_ALIGN		0xfffff000

/* allocation ---------------------------------------------------------------*/
typedef struct allocation {
	B			flag;
	VP			base;	
	UW			size;
} allocation_t;

/* functions ----------------------------------------------------------------*/
void printk(char*, ...);

#endif
