/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* i8259 --------------------------------------------------------------------*/
#include "klib.h"
#include "video.h"
#include "interrupt.h"
#include "io.h"
#include "smp.h"
#include "i8259.h"
#include "i8259P.h"

/* i8259_init ---------------------------------------------------------------*/
void
i8259_init(void)
{
	/* master -----------------------------------------------------------*/
	outb(IO_I8259_M, 0x11);		/* ICW1: trigger use ICW4 */
	outb(IO_I8259_MD, VECT_IRQ0);	/* ICW2: vector IRQ0 - IRQ7 */
	outb(IO_I8259_MD, 0x04);	/* ICW3: slave is connected with 2 */
	outb(IO_I8259_MD, 0xd);		/* ICW4: buffer mode (master) */

	/* slave ------------------------------------------------------------*/
	outb(IO_I8259_S, 0x11);		/* ICW1: trigger use ICW4 */
	outb(IO_I8259_SD, VECT_IRQ8);	/* ICW2: vector IRQ8 - IRQ15 */
	outb(IO_I8259_SD, 0x02);	/* ICW3: slave is connected with 2 */
	outb(IO_I8259_SD, 0x09);	/* ICW4: buffer mode (slave) */

	/* mask all IRQs until explicitly enabled ----------------------------*/
	outb(IO_I8259_MD, 0xff);	/* OCW1: mask all master IRQs */
	outb(IO_I8259_SD, 0xff);	/* OCW1: mask all slave IRQs */
	printk("i8259: initialized\n");
}

/* i8250_restore ------------------------------------------------------------*/
void
i8259_restore(void)
{
	outb(IO_I8259_M, 0x11);
	outb(IO_I8259_MD, 0x08);
	outb(IO_I8259_MD, 0x04);
	outb(IO_I8259_MD, 0x0d);

	outb(IO_I8259_S, 0x11);
	outb(IO_I8259_SD, 0x70);
	outb(IO_I8259_SD, 0x02);
	outb(IO_I8259_SD, 0x09);
}

/* i8259_reenable -----------------------------------------------------------*/
void
i8259_reenable(void)
{
	outb(IO_I8259_M, EOI);
	outb(IO_I8259_S, EOI);
	smp_eoi();
}
