/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/

#ifndef _KLIB_H
#define _KLIB_H

extern int inb(unsigned long);
extern int inw(unsigned long);
extern void outb(unsigned long, unsigned long);
extern void outw(unsigned long, unsigned long);
extern void ccli(void);
extern void csti(void);
extern void cwait(void);
extern int cxchg(unsigned long *, unsigned long);
extern void cltr(unsigned long);
extern void jmp_task(void);
extern void resume(unsigned long);
extern short	get_cs(void);
extern short	get_ds(void);
extern short	get_ss(void);
extern long	get_esp(void);
extern long	get_eflags(void);
extern long	iget_uesp(void);
extern long	iget_ueip(void);
extern int	syscall(void);
extern void	start_first_task(void);
extern void	start_second_task(void);

#endif
