# Copyright (c) 2000 by t-ishii66. All rights reserved. -----------------#
# Allegro --------------------------------------------------------------------#

CC = gcc
CFLAGS = -m32 -fno-pie -fno-stack-protector -fno-builtin -g

OBJS = lib_mbf.o lib_tsk.o lib_int.o lib_sem.o lib_tim.o lib_exd.o

# start (1n bit boot) --------------------------------------------------------#
all	: $(OBJS) 
	ar r libc.a $(OBJS)
%.o	: %.c
	$(CC) -c $(CFLAGS) $<
clean:
	rm -rf *.o
	rm libc.a
