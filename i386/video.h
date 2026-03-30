/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/

#ifndef _VIDEO_H
#define _VIDEO_H

void video_init(void);
void video_puts(char*);
void video_putc(char);
void video_putn(unsigned int, int);
void printk2(char**);
void printk(char*, ...);

/* VGA direct-write functions (no scroll, safe from any ring) */
void vga_clear(void);
void vga_write_at(int row, int col, char *s, unsigned char attr);
void vga_write_dec_at(int row, int col, unsigned long n, int width, unsigned char attr);

#endif
