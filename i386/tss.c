/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#include "../include/itron.h"
#include "../include/types.h"
#include "proc.h"
#include "addr.h"
#include "386.h"
#include "tss.h"
#include "tssP.h"
#include "kernelval.h"

/* Per-CPU TSS instances ----------------------------------------------------*/
/* TSS is used ONLY for esp0/ss0 -- the CPU reads these fields on every
 * Ring 3->Ring 0 interrupt to know which kernel stack to switch to.
 * start_first_task/start_second_task load the Task Register via LTR
 * (not LJMP), so no hardware task switch occurs and no dummy TSS is needed.
 * The full register fields (EIP, ESP, CS, etc.) in the TSS are unused.     */
tss_t	tss0, tss1;

/* tss_init -----------------------------------------------------------------*/
/* Set up the two TSSes with esp0/ss0 and register them in the GDT.
 * esp0 = the initial task's kern_stack_top (where the CPU should switch
 * ESP on the next Ring 3->Ring 0 interrupt).
 * ss0  = kernel stack segment (SEL_K32_S = 0x30).                          */
void
tss_init(void)
{
	int	i;
	char	*p;

	/* Zero both TSSes */
	p = (char *)&tss0;
	for (i = 0; i < (int)sizeof(tss_t); i++) *p++ = 0;
	p = (char *)&tss1;
	for (i = 0; i < (int)sizeof(tss_t); i++) *p++ = 0;

	/* CPU 0: Task 1 */
	tss0.esp0 = proc[1].kern_stack_top;
	tss0.ss0  = SEL_K32_S;

	/* CPU 1: Task 2 */
	tss1.esp0 = proc[2].kern_stack_top;
	tss1.ss0  = SEL_K32_S;

	/* Register TSSes in GDT */
	set_gdt(SEL_TSS0, (unsigned long)&tss0,
		(unsigned long)sizeof(tss_t), ST_TSS, 0, _32BIT);
	set_gdt(SEL_TSS1, (unsigned long)&tss1,
		(unsigned long)sizeof(tss_t), ST_TSS, 0, _32BIT);
}


/* tss_update_esp0 ----------------------------------------------------------*/
/* Update TSS.esp0 for the given CPU.  Called on task switch so that the
 * next Ring 3->Ring 0 interrupt uses the new task's kernel stack.          */
void
tss_update_esp0(int cpu, unsigned long esp0)
{
	if (cpu == 0)
		tss0.esp0 = esp0;
	else
		tss1.esp0 = esp0;
}
