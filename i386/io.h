/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* I/O ports ----------------------------------------------------------------*/
#ifndef _IO_H
#define _IO_H

/* interrupt vector ---------------------------------------------------------*/
#define VINTR_TIMER	0x08
#define VINTR_KEY	0x09
#define VINTR_FD	0x0e
#define VINTR_HD	0x76

/* i8259 --------------------------------------------------------------------*/
#define IO_I8259_M	0x020
#define IO_I8259_S	0x0a0
#define IO_I8259_MD	0x021
#define IO_I8259_SD	0x0a1

/* 6845 video ---------------------------------------------------------------*/
#define IO_6845		0x3d4
#define IO_6845_V	0x3da

/* floppy drive -------------------------------------------------------------*/
#define IO_FDC_CMD	0x3f0
#define IO_FDC_STAT	0x3f4
#define IO_FDC_DATA	0x3f5
#define IO_FDC_DOR	0x3f2

/* dma ----------------------------------------------------------------------*/
#define IO_DMA_INIT	0x00a
#define IO_DMA_STAT1	0x00b
#define IO_DMA_STAT2	0x00c
#define IO_DMA_COUNT	0x005
#define IO_DMA_TOP	0x081
#define IO_DMA_ADDR	0x004

/* timer (8253) -------------------------------------------------------------*/
#define IO_TIMER_0	0x40
#define IO_TIMER_1	0x41
#define IO_TIMER_2	0x42
#define IO_TIMER_C	0x43

/* keyboard -----------------------------------------------------------------*/
#define IO_KEY		0x60
#define IO_KEY_CNT	0x61
#define IO_KEY_CS	0x64

/* pcmcia -------------------------------------------------------------------*/
#define IO_PCC_INDEX	0x3e0
#define IO_PCC_DATA	0x3e1

/* ne2000 -------------------------------------------------------------------*/
#define IO_NE2000	0x300
#define IO_DP8390	0x300

#endif
