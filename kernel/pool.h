/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _POOL_H
#define _POOL_H

/* function declarations ----------------------------------------------------*/
ER pool_init(allocation_t*, VP, VP);
void pool_ins(allocation_t*);
void pool_rem(allocation_t*);
VP pool_alloc(allocation_t*, SIZE, unsigned long);
void pool_free(allocation_t*, VP);
void pool_dump(allocation_t*);
ER stack_init(VP, VP);
VP stack_alloc(SIZE);
void stack_free(VP);
void stack_dump(void);
ER mem_init(VP, VP);
VP mem_alloc(SIZE);
void mem_free(VP);
void mem_dump(void);
ER kmem_init(VP, VP);
VP kmem_alloc(SIZE);
void kmem_free(VP);
void kmem_dump(void);

#endif
