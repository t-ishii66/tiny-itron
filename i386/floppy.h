/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _FLOPPY_H
#define _FLOPPY_H

int fdc_intr(void);
int fdc_rw(long, char*, int);
int fdc_out(unsigned char);
int fdc_seek(int, int, int);
int fdc_result(void);
int fdc_ready(void);
void fdc_sleep(int*);
void fdc_recalibrate(void);
void fdc_start_motor(void);
void fdc_stop_motor(void);
void dma_setup(unsigned long, int);

#define fdc_read(n, buf)	fdc_rw(n, buf, 0)
#define fdc_wriet(n, buf)	fdc_rw(n, buf, 1)

#endif
