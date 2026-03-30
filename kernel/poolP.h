/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _LIBP_H
#define _LIBP_H

#define MAX_STACK_POOL		256
#define MAX_MEM_POOL		256
#define MAX_KMEM_POOL		256

static allocation_t	stack_pool[MAX_STACK_POOL];
static allocation_t	mem_pool[MAX_MEM_POOL];
static allocation_t	kmem_pool[MAX_KMEM_POOL];

#endif
