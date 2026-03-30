/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _PROC_H
#define _PROC_H

/* proc ---------------------------------------------------------------------*/
/* Per-task kernel stack design:
 * Each task has its own 4KB kernel stack.  When a Ring 3->Ring 0 interrupt
 * occurs, the CPU switches to the task's kernel stack (via TSS.esp0).
 * SAVE_ALL pushes registers onto this stack, and RESTORE_ALL pops them.
 * On task switch, we simply save/restore ESP -- the entire register state
 * lives on each task's kernel stack.  This is the standard pattern used
 * by Linux and other production kernels.                                   */
typedef struct proc {
	unsigned long	kern_esp;	/* saved kernel stack pointer */
	unsigned long	kern_stack_top;	/* top of kernel stack (set in TSS.esp0) */
	unsigned long	saved_eflags;	/* for proc_eflags_save/restore */
	int		cpu;		/* CPU affinity (0 or 1) */
} proc_t;

/* pt_regs: register frame pushed by CPU + SAVE_ALL on the kernel stack.
 *
 * On a Ring 3->Ring 0 interrupt, the CPU pushes SS, ESP, EFLAGS, CS, EIP.
 * Then SAVE_ALL pushes EAX, ECX, EDX, EBX, EBP, ESI, EDI, DS, ES.
 * The resulting stack frame (low address at top):
 *
 *   Offset  Field       Pushed by
 *   ------  -----       ---------
 *    0x00   ES          SAVE_ALL
 *    0x04   DS          SAVE_ALL
 *    0x08   EDI         SAVE_ALL
 *    0x0C   ESI         SAVE_ALL
 *    0x10   EBP         SAVE_ALL
 *    0x14   EBX         SAVE_ALL
 *    0x18   EDX         SAVE_ALL
 *    0x1C   ECX         SAVE_ALL
 *    0x20   EAX         SAVE_ALL
 *    0x24   EIP         CPU (interrupt frame)
 *    0x28   CS          CPU (interrupt frame)
 *    0x2C   EFLAGS      CPU (interrupt frame)
 *    0x30   ESP         CPU (Ring 3->0 only)
 *    0x34   SS          CPU (Ring 3->0 only)
 */
struct pt_regs {
	unsigned long es;
	unsigned long ds;
	unsigned long edi;
	unsigned long esi;
	unsigned long ebp;
	unsigned long ebx;
	unsigned long edx;
	unsigned long ecx;
	unsigned long eax;
	/* CPU-pushed interrupt frame */
	unsigned long eip;
	unsigned long cs;
	unsigned long eflags;
	unsigned long esp;	/* Ring 3 ESP (only on cross-privilege) */
	unsigned long ss;	/* Ring 3 SS  (only on cross-privilege) */
};

/* Byte offset of EAX within pt_regs (used by kernel/ to set return values) */
#define PT_REGS_EAX_OFFSET	0x20

/* function declarations ----------------------------------------------------*/
void proc_init(void);
proc_t* proc_create(ID, T_CTSK*);
ER proc_set_tsk_arg(ID, VP_INT);
ER proc_delete(ID);
void proc_eflags_save(ID);
void proc_eflags_restore(ID);
void proc_switch(void);
void proc_set_return_value(proc_t *p, unsigned long val);
extern proc_t*	current_proc[];

#endif
