/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* floppy controller --------------------------------------------------------*/
#include "video.h"
#include "klib.h"
#include "interrupt.h"
#include "io.h"
#include "floppy.h"
#include "floppyP.h"

/* fdc_init -----------------------------------------------------------------*/
void
fdc_init(void)
{
}

/* fdc_wr (fdc_read/fdc_write) ----------------------------------------------*/
int
fdc_rw(long addr, char* buf, int mode)
{
	unsigned char	c, h ,s;
	unsigned char	h2;

	c = addr / (N_HEAD * N_SECTOR);
	h2 = addr - c * (N_HEAD * N_SECTOR);
	h = (h2 / N_SECTOR);
	s = h2 - h * N_SECTOR;
	s ++;			/* sector is from 1 to 18 */
	fdc_start_motor();
	if (fdc_seek(c, h, s) < 0) {
		fdc_recalibrate();
		return -1;
	}
	if (mode) {
		dma_setup((unsigned long)buf, DMA_WRITE);
		fdc_out(FDC_WRITE);
	} else {
		dma_setup((unsigned long)buf, DMA_READ);
		fdc_out(FDC_READ);
	}
	fdc_out(h << 2);
	fdc_out(c);
	fdc_out(h);
	fdc_out(s);
	fdc_out(2);	/* len[512/128]  see minix */
	fdc_out(18);	/* only 1 sector read or write */
	fdc_out(GAP);
	fdc_out(DTL);

	/* wait interrupt */
printk("fdc_rw: sleep\n");
	fdc_sleep(&fdc_sleep_val);
printk("fdc_rw: after sleep\n");
	
	if (fdc_result() < 0)
		return -1;
	/* check result !!! */	
	return 0;
}

/* fdc_result ---------------------------------------------------------------*/
int
fdc_result(void)
{
	int	i;
	unsigned int	ret;
	long	k;
	for (i = 0 ; i < MAX_RESULTS ; i ++) {
		ret = inb(IO_FDC_STAT);	
		if ((ret & MASTER) == 0) {
printk("result:1\n");
continue;
			return ERR_STAT;
		}
		ret = inb(IO_FDC_STAT);
		if ((ret & DIRECTION) == 0)  {
printk("result:2\n");
continue;
			return ERR_STAT;
		}
		ret = inb(IO_FDC_DATA);
		for (k = 0 ; k < 100 ; k ++);
		ret = inb(IO_FDC_STAT);
		if ((ret & CTL_BUSY) == 0)
			return 0;
	}
printk("result:3\n");
	return ERR_STAT;	
}

/* fdc_out ------------------------------------------------------------------*/
int
fdc_out(unsigned char cmd)
{
	unsigned char	r;
	int	retry;
	long	i;

	if (fdc_need_reset) {
printk("fdc_out: entrance error !\n");
		return -1;
	}

	retry = MAX_RETRY;
	while (-- retry) {
		r = inb(IO_FDC_STAT);
		if ((r & (MASTER | DIRECTION)) == CTL_ACCEPTING) {
			outb(IO_FDC_DATA, cmd);
			return 0;
		}
		for (i = 0 ; i < 1000000 ; i ++);
	}
printk("fdc_out: error !\n");
	return -1;
}

/* fdc_intr -----------------------------------------------------------------*/
int
fdc_intr(void)
{
	fdc_sleep_val = 0;
	return 0;
}

/* fdc_sleep ----------------------------------------------------------------*/
void
fdc_sleep(int* val)
{
	*val = 1;
	while (*val) ;
}

/* fdc_reset ----------------------------------------------------------------*/
int
fdc_reset(void)
{
	ccli();
	fdc_motor = 0;
	fdc_goal = 0;

	/* drive select A: and DMA & I/O interface enabled */
	outb(IO_FDC_DOR, 0);
	outb(IO_FDC_DOR, ENABLE_IF);

	csti();
printk("fdc_reset: sleep\n");
	fdc_sleep(&fdc_sleep_val);
printk("fdc_reset: after sleep\n");

	fdc_start_motor();
	/* specify */
	fdc_out(FDC_SPECIFY);
	fdc_out(SPEC_1);
	fdc_out(SPEC_2);
/*
	fdc_stop_motor();
*/
	fdc_need_reset = 0;

	return 0;
}

/* fdc_sense_result ---------------------------------------------------------*/
int
fdc_sense_result(void)
{
	unsigned int	r;
	int	i;
	long	k;
	for (i = 0 ; i < MAX_RESULTS ; i ++) {
		r = inb(IO_FDC_STAT);
		if ((r & (MASTER | DIRECTION | CTL_BUSY)) == CTL_ACCEPTING)
			return 0;
		r = inb(IO_FDC_DATA);
		for (k = 0 ; k < 100 ; k ++);
	} 
	return -1;
}

/* fdc_sense ----------------------------------------------------------------*/
int
fdc_sense(void)
{
	fdc_out(FDC_SENSE);
	return fdc_sense_result();
}

/* fdc_recalibrate ----------------------------------------------------------*/
void
fdc_recalibrate(void)
{
	fdc_out(FDC_RECALIBRATE);
	fdc_out(0);	/* drive A: */
	fdc_sleep(&fdc_sleep_val);
	fdc_sense();
}

/* fdc_seek -----------------------------------------------------------------*/
int
fdc_seek(int cyl, int head, int sect)
{
	fdc_start_motor();
	if (fdc_ready() < 0)
printk("fdc_seek:not ready error\n");

printk("SEEK(");
printk("%x,%x,%x)\n", cyl, head, sect);
	fdc_out(FDC_SEEK);
/*
	fdc_out((unsigned char)(head << 2 | 0x00));
*/
	fdc_out(0);
	fdc_out((unsigned char)cyl);

	fdc_sleep(&fdc_sleep_val);
	return fdc_sense();
}

/* fdc_ready ----------------------------------------------------------------*/
int
fdc_ready(void)
{
	int	i;
	int	r;
	long	k;
	for (i = 0 ; i < MAX_RETRY ; i ++) {
		r = inb(IO_FDC_STAT);
		if ((r & CTL_BUSY) == 0)
			return 0;
		for (k = 0 ; k < 10000 ; k ++);
	}
	return -1;
}

/* fdc_start_motor ----------------------------------------------------------*/
void
fdc_start_motor(void)
{
	outb(IO_FDC_DOR, 0x1c);	/* start motor A: */
}

/* fdc_stop_motor -----------------------------------------------------------*/
void
fdc_stop_motor(void)
{
	outb(IO_FDC_DOR, 0x0c);	/* stop motor A: */
}

/* dma_setup ----------------------------------------------------------------*/
void
dma_setup(unsigned long addr, int mode)
{
/*
	unsigned char	addr_l, addr_m, addr_h;
*/
	ccli();
	outb(IO_DMA_STAT2, mode);
	outb(IO_DMA_STAT1, mode);
	outb(IO_DMA_ADDR, addr & 0xff);
	outb(IO_DMA_ADDR, (addr >> 8) & 0xff);
	outb(IO_DMA_TOP, (addr >> 16) & 0x0f);
	outb(IO_DMA_COUNT, (512 - 1) & 0xff);
	outb(IO_DMA_COUNT, ((512 - 1) >> 8) & 0xff);
	csti();
	outb(IO_DMA_INIT, 2);
}
