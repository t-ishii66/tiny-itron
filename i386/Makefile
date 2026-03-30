# Copyright (c) 2000 by t-ishii66. All rights reserved. -----------------#
# Allegro --------------------------------------------------------------------#

CC = gcc
CFLAGS = -Wall -m32 -fno-pie -fno-stack-protector -fno-builtin -g
LD = ld
AS = as
LDFLAGS = -m elf_i386 -n -T kernel.ld -static
ITRON_DIR = ../kernel
ITRON =	$(ITRON_DIR)/libkernel.a\
	../lib/libc.a

OBJS = run.o main.o i8259.o video.o klib.o intr.o interrupt.o floppy.o\
	syscall.o keyboard.o proc.o tss.o 386.o page.o\
	timer.o kernelval.o smp.o $(ITRON)

all: start genasm kernel table
	cat boot/boot table start kernel > i386

# start (16 bit boot) --------------------------------------------------------#
start: start.o
	$(LD) -m elf_i386 --oformat binary -Ttext 0x0 -o start start.o
start.o: start.s
	$(AS) --32 -o start.o start.s

kernel	: elf $(OBJS)
	$(LD) $(LDFLAGS) -e run -o _kernel $(OBJS)
	cp _kernel _kernel_dbg
	strip _kernel
	cp	/dev/null kernel
	./elf _kernel kernel

elf	: elf.c
	$(CC) -o elf elf.c
%.o	: %.c
	$(CC) -c $(CFLAGS) $<
%.o	: %.s
	$(AS) --32 --gstabs -o $@ $<

table: table.o
	$(LD) -m elf_i386 --oformat binary -Ttext 0x0 -o table table.o
table.o: table.s
	$(AS) --32 -o table.o table.s
table.s: genasm
	./genasm

genasm: genasm.c
	$(CC) -o genasm genasm.c

clean:
	rm -rf *.o
	rm -f table.s
	rm -f genasm
