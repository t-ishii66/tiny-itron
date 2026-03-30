/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* 6845 video controller ----------------------------------------------------*/
/* CAUTION (DON'T use from USER process, because scroll I/O may happen. -----*/

#include "smp.h"
#include "klib.h"
#include "io.h"
#include "videoP.h"
#include "video.h"
#include "../include/stdio.h"

static unsigned long video_lk = 0;

/* video_init ---------------------------------------------------------------*/
void
video_init(void)
{
	c_x = c_y = 0;
	c_y_max = 24;	
	scrolltop = 0;
	video_set_6845(G_VID_ORG, scrolltop);
/*
	outb((unsigned long)IO_6845, (unsigned long)(0x01));
	printk("6845: initialized\n");
*/
}

/* printk2 (for system call) ------------------------------------------------*/
void
printk2(char** sp)
{
	char*	s = *sp;
	va_list	ap;
	va_start2(ap, sp);
	smp_lock((unsigned long)&video_lk);
	while (*s != '\0') {
		if (*s == '%') {
			switch (*(++ s)) {
				case 's':
					video_puts(va_arg(ap, char*));
					break;
				case 'x':
					video_putn(va_arg(ap, long), 16);
					break;
				case 'd':
					video_putn(va_arg(ap, long), 10);
					break;
				case 'c':
					video_putc((char)va_arg(ap, long));
					break;
				default:
					video_putc('%');
					video_putc(*(s - 1));
			}
		} else {
			video_putc(*s);
		}
		s ++;
	}
	smp_unlock((unsigned long)&video_lk);
}

/* CAUTION (DON'T use from USER process, because scroll I/O may happen. -----*/
/* printk -------------------------------------------------------------------*/
void
printk(char* s, ...)
{
	va_list	ap;
	va_start(ap, s);
	smp_lock((unsigned long)&video_lk);
	while (*s != '\0') {
		if (*s == '%') {
			switch (*(++ s)) {
				case 's':
					video_puts(va_arg(ap, char*));
					break;
				case 'x':
					video_putn(va_arg(ap, long), 16);
					break;
				case 'd':
					video_putn(va_arg(ap, long), 10);
					break;
				case 'c':
					video_putc((char)va_arg(ap, long));
					break;
				default:
					video_putc('%');
					video_putc(*(s - 1));
			}
		} else {
			video_putc(*s);
		}
		s ++;
	}
	smp_unlock((unsigned long)&video_lk);
}

/* CAUTION (DON'T use from USER process, because scroll I/O may happen. -----*/
/* video_puts ---------------------------------------------------------------*/
void
video_puts(char* p)
{
	static char c = '#';
	while (*p != '\0') {
		video_putc(*p ++);
		if (c_y == 20)
			video_putc(c);
	}
}

/* video_putc ---------------------------------------------------------------*/
void
video_putc(char c)
{
	unsigned char*	p = (unsigned char*)G_BASE;	
	p += 2 * scrolltop + c_y * 160 + c_x * 2;
	if (c == '\n') {
		c_y ++; c_x = 0;
	} else {
		video_wait();
		*p = c;
		*(p + 1) = G_ATTR;
		c_x ++;
		if (c_x >= 80) {
			c_x = 0;
			c_y ++;
		}
	}
	if (c_y > 24) {
		video_scroll();
		c_y = 24;
	}
}

/* video_set_6845 -----------------------------------------------------------*/
static void
video_set_6845(unsigned short reg, unsigned short val)
{
	/* wait virtical return line */
	video_wait();
	outb((unsigned long)IO_6845, (unsigned long)(reg & 0xff));
	outb((unsigned long)(IO_6845 + 1), (unsigned long)(val >> 8) & 0xff);
	outb((unsigned long)IO_6845, (unsigned long)((reg + 1) & 0xff));
	outb((unsigned long)(IO_6845 + 1), (unsigned long)(val) & 0xff);
}

/* video_scroll -------------------------------------------------------------*/
static void
video_scroll(void)
{
	scrolltop += 80;
	c_y_max ++;
	video_set_6845(G_VID_ORG, scrolltop);
	video_clear_line();
	if (scrolltop > 80 * 25) {
		c_x = 0;
		video_wait();
		video_copy((unsigned long)(G_TOTAL + G_BASE),
				(unsigned long)G_BASE,
				(unsigned short)G_TOTAL); 
		c_y = 24;
		c_y_max = 24;
		scrolltop = 0;
		video_set_6845(G_VID_ORG, scrolltop);
		video_clear_line();
		c_x = 0;
	}
}

static void
video_clear_line(void)
{
	unsigned char*	p = (unsigned char*)G_BASE;	
	int	i;
	p += 2 * scrolltop + c_y * 160 + c_x * 2;
	for (i = 0 ; i < 80 * 2 ; i ++)
		*p ++ = '\0';
}

static void
video_cursor(void)
{
	unsigned short c = c_y * 160 + c_x;
	video_set_6845(G_CURSOR, c);
}

/* video_copy ---------------------------------------------------------------*/
static void
video_copy(unsigned long src, unsigned long dst, unsigned short len)
{
	unsigned short	i;
	unsigned char*	p_src = (unsigned char*)src;
	unsigned char*	p_dst = (unsigned char*)dst;
	for (i = 0 ; i < len ; i ++) {
		*p_dst = *p_src;
		p_dst ++; p_src ++;
	}
}

/* video_wait ---------------------------------------------------------------*/
static void
video_wait(void)
{
/*
	while (inb(IO_6845_V) != 0x08)
		; 
*/
}

/* == VGA direct-write functions (no scroll, safe from any ring) =========== */

/* vga_clear ----------------------------------------------------------------*/
void
vga_clear(void)
{
	unsigned short *p = (unsigned short *)G_BASE;
	int i;
	for (i = 0; i < 80 * 25; i++)
		p[i] = 0x0700 | ' ';	/* grey-on-black space */
	scrolltop = 0;
	c_x = 0;
	c_y = 0;
	/* video_set_6845 uses outb (I/O port instruction).  If called from
	 * Ring 3 with IOPL=0, outb causes #GP.  The hardware origin is
	 * already set to 0 by video_init (Ring 0), so skip it here. */
}

/* vga_write_at -------------------------------------------------------------*/
void
vga_write_at(int row, int col, char *s, unsigned char attr)
{
	unsigned short *p = (unsigned short *)G_BASE;
	p += row * 80 + col;
	while (*s && col < 80) {
		*p++ = (unsigned short)attr << 8 | (unsigned char)*s++;
		col++;
	}
}

/* vga_write_dec_at ---------------------------------------------------------*/
/* Write unsigned long n right-justified in 'width' columns at (row, col).   */
void
vga_write_dec_at(int row, int col, unsigned long n, int width, unsigned char attr)
{
	char buf[12];
	int i, len;

	/* convert to decimal string */
	if (n == 0) {
		buf[0] = '0';
		len = 1;
	} else {
		len = 0;
		while (n > 0 && len < 11) {
			buf[len++] = '0' + (n % 10);
			n /= 10;
		}
	}
	/* reverse */
	for (i = 0; i < len / 2; i++) {
		char tmp = buf[i];
		buf[i] = buf[len - 1 - i];
		buf[len - 1 - i] = tmp;
	}
	buf[len] = '\0';

	/* pad with spaces and write */
	{
		unsigned short *p = (unsigned short *)G_BASE + row * 80 + col;
		int pad = width - len;
		if (pad < 0) pad = 0;
		for (i = 0; i < pad; i++)
			p[i] = (unsigned short)attr << 8 | ' ';
		for (i = 0; i < len; i++)
			p[pad + i] = (unsigned short)attr << 8 | (unsigned char)buf[i];
	}
}

/* video_putn ---------------------------------------------------------------*/
void
video_putn(unsigned int n, int mode)
{
	int	d;
	char	buf[64];
	char	c;
	int	i, j;

	if (n == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		video_puts(buf);
		return;
	}
	
	if (mode == 16) {
		buf[0] = '\0';
		j = 0;
		while (n != 0) {
			for (i = j ; i >= 0 ; i --)
				buf[i + 2] = buf[i];
			j += 2;
			c = n & 0x0f;
			if (c <= 9)
				buf[1] = '0' + c;
			else
				buf[1] = 'a' + c - 10;			
			c = (n & 0xf0) >> 4;
			if (c <= 9)
				buf[0] = '0' + c;
			else
				buf[0] = 'a' + c - 10;			
			n >>= 8;
		}	
		video_puts(buf);
	} else if (mode == 10) {
		buf[0] = '\0';
		j = 0;
		while (n != 0) {
			for (i = j ; i >= 0 ; i --)
				buf[i + 1] = buf[i];

			d = n % 10;	/* amari */
			buf[0] = '0' + d;
			n = (n - d) / 10;
			j ++;
		}	
		video_puts(buf);
	}
}
