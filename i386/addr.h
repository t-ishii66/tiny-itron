/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _ADDR_H
#define _ADDR_H

/* linear address -----------------------------------------------------------*/
#define	AL_BOOT		(unsigned long)0x7c00
/* #define AL_KERNEL16	(unsigned long)0x2a00 */
#define AL_KERNEL16	(unsigned long)0x3000
#define AL_KERNEL32	(unsigned long)0x0L
#define AL_USER32	(unsigned long)0x0L
#define AL_GDT		(unsigned long)0x2000
#define AL_IDT		(unsigned long)0x2100
#define AL_MAX		(unsigned long)0x500000	/* 5M byte */

/* segment offset address (base segment AL_KERNEL16) ------------------------*/
#define AS_GDT		(unsigned short)0
#define AS_IDT		(unsigned short)0

/* selector -----------------------------------------------------------------*/
#define SEL_K16_C	0x08	/* 16 bit kernel code */
#define SEL_K16_D	0x10	/* 16 bit kernel data */
#define SEL_K16_S	0x18	/* 16 bit kernel stack */
#define SEL_K32_C	0x20	/* 32 bit kernel code */
#define SEL_K32_D	0x28	/* 32 bit kernel data */
#define SEL_K32_S	0x30	/* 32 bit kernel stack */
#define SEL_TSS0	0x38	/* CPU 0 task state segment (esp0/ss0 only) */
#define SEL_TSS1	0x40	/* CPU 1 task state segment (esp0/ss0 only) */
#define SEL_U32_C	0x58	/* 32 bit user code */
#define SEL_U32_D	0x60	/* 32 bit user data */
#define SEL_U32_S	0x68	/* 32 bit user stack */
#define SEL_SYSCALL	0x70	/* call gate for system call */

/* 8Mbyte -> 7a0000 in hex */
/* stack pointer (CPU0) stored in tss ---------------------------------------*/
#define CPU0_SP		0x7a0000	/* initial stack pointer (CPU0) */
#define CPU0_SP0	0x790000	/* sp0 stack pointer */
#define CPU0_SP3	0x780000	/* sp3 stack pointer */

/* stack pointer (CPU1) stored in tss ---------------------------------------*/
#define CPU1_SP		0x770000	/* initial stack pointer (CPU1) */
#define CPU1_SP0	0x760000	/* sp0 stack pointer */
#define CPU1_SP3	0x750000	/* sp3 stack pointer */

/* stack pool ---------------------------------------------------------------*/
#define STACK_START	0x700000	/* user stack start */
#define STACK_END	0x74ffff	/* user stack end */

/* per-task kernel stack ----------------------------------------------------*/
/* Each task gets a 4KB kernel stack for interrupt/syscall handling.         */
/* 16 tasks × 4KB = 64KB total.  Located just above the user stack pool     */
/* at 0x750000, which is above USER_MEM_END so pages are Supervisor-only.   */
/* Task N's stack top = KERN_STACK_BASE + (N+1) * KERN_STACK_SIZE.          */
#define KERN_STACK_SIZE		4096		/* 4KB per task */
#define KERN_STACK_BASE		0x750000	/* base of kernel stack area */

/* mem pool -----------------------------------------------------------------*/
#define MEM_START	0x110000	/* user memory pool start (above kernel+user code ~0x1EDDB) */
#define MEM_END		0x6fffff	/* user memory spool end */

/* floppy buffer ------------------------------------------------------------*/
#define FDC_BUFFER	0x10000	/* for DMA */

#endif

#if 0
/* 16 bit start address must be border of 4k page. --------------------------*/
Segmentation (16 bit mode)

0x07c0:0000	boot loader
0x0100:0000	ne2000 buffer start (16)
0x01f0:0000	ne2000 buffer end (32 - 1)
0x0200:0000	GDT
0x0210:0000	IDT
0x0300:0000	kernel 16 bit	/* must be border of 4k page */
0x0340:0000	kernel 32 bit
0x0340:0000	kernel 32 bit
0xa000:0000	graphic v ram
0x700000
/* source code --------------------------------------------------------------*/
boot/boot.s	0x0200:0000 is used
start.s		0x0300:0000 is used
start.s		0x0340:0000 is used
run.s		0x7a0000 is used
run.s		0x770000 is used
Makefile	0x0340:0000 is used
genasm.c	0x0300:0000 is used	
smp.c		0x0300:0000 is used
intr.s		0x28/0x60 (data segment selector) is used
klib.s		0x38/0x40 (SEL_TSS0/SEL_TSS1) are used for ltr.

/* inturrupt vector ---------------------------------------------------------*/
See interruptP.h !!!

#endif
