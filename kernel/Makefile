# Copyright (c) 2000 by t-ishii66. All rights reserved. -----------------#
# Allegro --------------------------------------------------------------------#

CC = gcc
#CFLAGS = -m386 -msoft-float -fno-builtin
CFLAGS = -Wall -m32 -fno-pie -fno-stack-protector -fno-builtin -g

OBJS = kernel.o pool.o sys_tsk.o sys_sns.o sys_tex.o syscall.o sched.o user.o\
	sys_sem.o sys_flg.o sys_dtq.o sys_mbx.o sys_mtx.o sys_mbf.o\
	sys_por.o sys_mpf.o sys_mpl.o sys_tim.o sys_cyc.o sys_alm.o\
	sys_ovr.o sys_rdq.o sys_isr.o sys_exd.o
LIBS = 

# start (1n bit boot) --------------------------------------------------------#
all	: $(OBJS)
	ar r libkernel.a $(OBJS)
%.o	: %.c
	$(CC) -c $(CFLAGS) $<
clean:
	rm -rf *.o
	rm libkernel.a
