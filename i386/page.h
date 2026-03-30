/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _PAGE_H
#define _PAGE_H

/* page table entry flags ---------------------------------------------------*/
#define PTE_PRESENT	0x01	/* P:   page is present in memory */
#define PTE_RW		0x02	/* R/W: 0=read-only, 1=read-write */
#define PTE_USER	0x04	/* U/S: 0=supervisor only, 1=user accessible */
#define PTE_PWT		0x08	/* PWT: write-through caching */
#define PTE_PCD		0x10	/* PCD: cache disable */

/* page sizes ---------------------------------------------------------------*/
#define PAGE_SIZE	4096	/* 4 KB per page */
#define PAGES_PER_TABLE	1024	/* entries per page table / page directory */

/* function declarations ----------------------------------------------------*/
void page_init(void);
void page_enable(void);
unsigned long page_get_dir(void);

#endif
