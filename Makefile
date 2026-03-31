# Top-level Makefile for tiny-itron
# Build order: kernel (libkernel.a) -> lib (libc.a) -> i386 (link + binary)

all:
	$(MAKE) -C kernel
	$(MAKE) -C lib
	$(MAKE) -C i386

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C lib clean
	$(MAKE) -C i386 clean

.PHONY: all clean
