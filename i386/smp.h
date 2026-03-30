/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* SMP support -------------------------------------------------------------*/
#ifndef _SMP_H
#define _SMP_H

void smp_lock(unsigned long *);
void smp_unlock(unsigned long *);
void smp_eoi(void);
void smp_init(void);

#endif
