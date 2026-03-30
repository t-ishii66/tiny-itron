/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/stdio.h"
#include "../include/itron.h"
#include "../include/config.h"
#include "pool.h"
#include "poolP.h"

/* pool_init ----------------------------------------------------------------*/
ER
pool_init(allocation_t* a, VP start, VP end)
{
	start = (VP)(((UW)start + (UW)~MEM_ALIGN) & (UW)MEM_ALIGN);
	end = (VP)((UW)end & (UW)MEM_ALIGN);
	a->base = start;
	a->size = (UW)(end - start);
	a->flag = MEM_FLAG_FREE;
	(a + 1)->size = 0;
	return E_OK;
}

/* pool_ins -----------------------------------------------------------------*/
void
pool_ins(allocation_t* a)
{
	int	i;
	i = 0;
	while ((a + i)->size != 0)
		i ++;

	for (; i >= 1 ; i --) {
		*(a + i) = *(a + i - 1);
	}
}

/* pool_rem -----------------------------------------------------------------*/
void
pool_rem(allocation_t* a)
{
	int	i;
	i = 0;
	while ((a + i)->size != 0) {
		*(a + i) = *(a + i + 1);
		i ++;
	}
}

/* pool_alloc ---------------------------------------------------------------*/
VP
pool_alloc(allocation_t* a, SIZE stksz, unsigned long max_a)
{
	int	i;
	SIZE	new_stksz = (stksz + ~MEM_ALIGN) & MEM_ALIGN;
	i = 0;
	while ((a + i)->size != 0) {
		if ((a + i)->flag == MEM_FLAG_FREE) {
			if ((a + i)->size > new_stksz) {
				pool_ins(a + i + 1);
				(a + i + 1)->size = (a + i)->size - new_stksz;
				(a + i + 1)->flag = MEM_FLAG_FREE;
				(a + i + 1)->base = (a + i)->base + new_stksz;

				(a + i)->size = new_stksz;
				(a + i)->flag = MEM_FLAG_ALLOC;

				return (a + i)->base;
			} else if ((a + i)->size == new_stksz) {
				(a + i)->flag = MEM_FLAG_ALLOC;
				return (a + i)->base;
			}
		}
		i ++;
		if (i >= max_a)
			break;
	}

	return (VP)NULL;
}

/* pool_free ----------------------------------------------------------------*/
void
pool_free(allocation_t* a, VP stack)
{
	int	i;
	i = 0;
	while ((a + i)->size != 0) {
		if ((a + i)->base == stack) {
			(a + i)->flag = MEM_FLAG_FREE;
			if (i > 0 && (a + i - 1)->flag == MEM_FLAG_FREE) {
				(a + i - 1)->size += (a + i)->size;	
				pool_rem(a + 1);
				i --;
			}
			if ((a + i + 1)->flag == MEM_FLAG_FREE) {
				(a + i)->size += (a + i + 1)->size;
				pool_rem(a + i + 1);
			}
			return;
		}
		i ++;
	}
	/* panic */
}

/* pool_dump ----------------------------------------------------------------*/
void
pool_dump(allocation_t* a)
{
	int	i = 0;
	printk("----------------------------------------------------------\n");
	while ((a + i)->size != 0) {
		printk("%d: base=%x, size=%x, flag=%x\n", i,
			(a + i)->base, (a + i)->size, (a + i)->flag);
		i ++;
	}	
}

/* stack_init ---------------------------------------------------------------*/
ER
stack_init(VP start, VP end)
{
	return pool_init(stack_pool, start, end);
}

/* stack_alloc --------------------------------------------------------------*/
VP
stack_alloc(SIZE stksz)
{
	/* caller holds kernel_lk */
	return pool_alloc(stack_pool, stksz, MAX_STACK_POOL);
}

/* stack_free ---------------------------------------------------------------*/
void
stack_free(VP stk)
{
	/* caller holds kernel_lk */
	pool_free(stack_pool, stk);
}

/* stack_dump ---------------------------------------------------------------*/
void
stack_dump(void)
{
	pool_dump(stack_pool);
}

/* mem_init -----------------------------------------------------------------*/
ER
mem_init(VP start, VP end)
{
	return pool_init(mem_pool, start, end);
}

/* mem_alloc ----------------------------------------------------------------*/
VP
mem_alloc(SIZE stksz)
{
	/* caller holds kernel_lk */
	return pool_alloc(mem_pool, stksz, MAX_MEM_POOL);
}

/* mem_free -----------------------------------------------------------------*/
void
mem_free(VP stk)
{
	/* caller holds kernel_lk */
	pool_free(mem_pool, stk);
}

/* mem_dump -----------------------------------------------------------------*/
void
mem_dump(void)
{
	pool_dump(mem_pool);
}

/* kmem_init ----------------------------------------------------------------*/
/* Initialize kernel-only memory pool.                                       */
/* This pool lives in Supervisor-only pages (between kernel end and          */
/* MEM_START), so Ring 3 user tasks cannot access the allocated memory.      */
/* Use this for kernel-internal buffers that must not be user-writable.      */
ER
kmem_init(VP start, VP end)
{
	return pool_init(kmem_pool, start, end);
}

/* kmem_alloc ---------------------------------------------------------------*/
VP
kmem_alloc(SIZE size)
{
	/* caller holds kernel_lk */
	return pool_alloc(kmem_pool, size, MAX_KMEM_POOL);
}

/* kmem_free ----------------------------------------------------------------*/
void
kmem_free(VP ptr)
{
	/* caller holds kernel_lk */
	pool_free(kmem_pool, ptr);
}

/* kmem_dump ----------------------------------------------------------------*/
void
kmem_dump(void)
{
	pool_dump(kmem_pool);
}
