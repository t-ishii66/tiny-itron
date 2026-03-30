# Copyright (c) 2000 by t-ishii66. All rights reserved. -----------------#
# boot -----------------------------------------------------------------------#

AS = as
LD = ld

boot:	boot.s
	$(AS) --32 -o boot.o boot.s
	$(LD) -m elf_i386 --oformat binary -Ttext 0x0 -o boot boot.o
	rm -f boot.o
clean:
	rm -f boot.o boot
