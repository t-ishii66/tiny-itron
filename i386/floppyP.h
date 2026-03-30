/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _FLOPPYP_H
#define _FLOPPYP_H

/* fdc cmd ------------------------------------------------------------------*/
#define FDC_SEEK	0x0f
#define FDC_READ	0xe6
#define FDC_WRITE	0xc5
#define FDC_SENSE	0x08
#define FDC_RECALIBRATE	0x07
#define FDC_SPECIFY	0x03

/* support only 3.5 1.44M ---------------------------------------------------*/
#define SECTOR_SIZE	512
#define N_SECT		18	/* 0-17 (1-18) */
#define N_CYL		80	/* 0-79 */
#define N_HEAD		2	/* 0-1  */
#define GAP 		0x2a
#define DTL		0xff
#if 0
#define SPEC_1		0xdf	/* ?? */
#define SPEC_2		0x02	/* ?? */
#else
#define SPEC_1		0xcf	/* ?? */
#define SPEC_2		0x10	/* ?? */
#endif

/* error code ---------------------------------------------------------------*/
#define ERR_SEEK	-1
#define ERR_TRANS	-2
#define ERR_STAT	-3
#define ERR_RECALIBRATE	-4
#define ERR_WPROTECT	-5
#define ERR_DRIVE	-6

/* status -------------------------------------------------------------------*/
#define MASTER		0x80
#define DIRECTION	0x40
#define CTL_ACCEPTING	0x80	
#define CTL_BUSY	0x10
#define ENABLE_IF	0x0c	

/* dma ----------------------------------------------------------------------*/
#define DMA_READ	0x46
#define DMA_WRITE	0x4a

/* max ----------------------------------------------------------------------*/
#define MAX_RESULTS	15
#define MAX_RETRY	5

/* basic parameters ---------------------------------------------------------*/
#define N_CYLINDER	80
#define N_SECTOR	18
#define N_HEAD		2

/* variables ----------------------------------------------------------------*/
static int fdc_sleep_val;
static int fdc_need_reset = 0;
static int fdc_motor = 0;
static int fdc_goal = 0;

#endif
