/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* Written with reference to the B-Free project's build_boot2.c.        */
/*                                                                      */
/* elf.c -- ELF-to-raw-binary converter (host tool)                     */
/* ================================================================     */
/*                                                                      */
/* This is a HOST-SIDE build tool, not part of the kernel itself.       */
/* It runs on the build machine (Linux) during `make` to convert the    */
/* kernel from ELF format to a flat (raw) binary suitable for booting.  */
/*                                                                      */
/* The kernel ELF has multiple LOAD segments with different VMA/LMA:    */
/*   - kernel segment: VMA=LMA=0x3400 (identity mapped)                */
/*   - user segment:   VMA=0x100000, LMA=after kernel BSS              */
/*                                                                      */
/* This tool processes all PT_LOAD segments in LMA order, writing them  */
/* into a flat binary with gaps preserved so that boot.s can load the   */
/* entire image at the base LMA (0x3400) and each segment lands at its  */
/* correct physical address.                                            */
/*                                                                      */
/* Build & usage (from i386/Makefile):                                  */
/*   cc -o elf elf.c           # compile on the host                   */
/*   ld ... -o _kernel *.o     # link kernel as ELF                    */
/*   ./elf _kernel kernel      # extract raw binary                    */
/*                                                                      */
/* The resulting 'kernel' file is then concatenated into the floppy     */
/* image:  cat boot/boot table start kernel > i386                     */
/*                                                                      */
/* ELF program header fields used:                                      */
/*   p_type   -- segment type (we look for PT_LOAD)                    */
/*   p_paddr  -- physical/load address (LMA) for flat binary placement */
/*   p_vaddr  -- virtual address (used for debug output only)          */
/*   p_offset -- offset of the segment data within the ELF file        */
/*   p_filesz -- size of the segment data in the file                  */
/*   p_memsz  -- size of the segment in memory (may be > filesz for BSS)*/
/* ================================================================     */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <elf.h>

/* Maximum kernel size: 640 KB.  The entire kernel image must fit in
 * this buffer when reading segment data from the ELF file. */
static char		buf[640000];

int readn(int fd, char* buf, int len);
int writen(int fd, char* buf, int len);

/* write_space --------------------------------------------------------------*/
/* Advance the output file position by 'len' bytes, inserting zero padding.
 *
 * Implementation: seek forward (len - 1) bytes, then write 1 byte.
 * This extends the file to the desired position without writing all
 * the intervening bytes (the OS fills them with zeros). */
void
write_space(int fd_d, int len)
{
	char	b[2] = {0, 0};
	lseek(fd_d, len - 1, SEEK_CUR);
	writen(fd_d, b, 1);
printf("# write_space: %d\n", len);
}

/* main ---------------------------------------------------------------------*/
/* Usage: ./elf <input_elf> <output_raw>
 *
 * Opens the input ELF file and output raw binary file, then processes
 * all PT_LOAD segments in LMA (p_paddr) order.  Gaps between segments
 * are filled with zero padding so the flat binary has the correct
 * physical memory layout.
 *
 * LMA is used (not VMA) because non-identity-mapped segments (e.g.
 * user code at VMA 0x100000) have their LMA set to the physical
 * address where they should appear in the flat binary. */
int
main(int argc, char* argv[])
{
	int		fd_s, fd_d;
	int		i, j;
	Elf32_Ehdr	e_eh;
	Elf32_Phdr	e_ph[32];
	int		order[32];	/* indices sorted by LMA */
	int		nload = 0;
	Elf32_Addr	base_lma;
	int		bytes_written = 0;

	if (argc < 3) {
		fprintf(stderr, "elf error 0\n");
		exit(1);
	}

	/* Open source (ELF kernel) and destination (raw binary) */
	if (((fd_s = open(argv[1], O_RDONLY)) < 0) ||
			(fd_d = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
		fprintf(stderr, "elf error 1\n");
		exit(1);
	}

	/* Read ELF header and program headers */
	readn(fd_s, (char*)&e_eh, sizeof(Elf32_Ehdr));
	lseek(fd_s, e_eh.e_phoff, SEEK_SET);
	readn(fd_s, (char*)e_ph, e_eh.e_phentsize * e_eh.e_phnum);

	/* Debug output: dump all program headers */
printf("#-----------------------------------------------------------------#\n");
printf("# [t]ype, [f]lag, [v]addr, [p]addr(LMA), [m]emsz, [s]filesz, [o]ffset\n");
printf("# e_eh.e_phnum = %d\n", e_eh.e_phnum);

	/* Collect PT_LOAD segments with nonzero filesz */
	for (i = 0; i < e_eh.e_phnum; i++) {
printf("# ph[%d]: t=0x%x f=0x%x v=0x%x p=0x%x m=0x%x s=0x%x o=%d\n",
	i, e_ph[i].p_type, e_ph[i].p_flags, e_ph[i].p_vaddr,
	e_ph[i].p_paddr, e_ph[i].p_memsz, e_ph[i].p_filesz, e_ph[i].p_offset);

		if (e_ph[i].p_type == PT_LOAD && e_ph[i].p_filesz > 0)
			order[nload++] = i;
	}

	/* Sort by LMA (p_paddr) -- simple insertion sort */
	for (i = 1; i < nload; i++) {
		int key = order[i];
		j = i - 1;
		while (j >= 0 && e_ph[order[j]].p_paddr > e_ph[key].p_paddr) {
			order[j + 1] = order[j];
			j--;
		}
		order[j + 1] = key;
	}

	if (nload == 0) {
		fprintf(stderr, "elf error: no PT_LOAD segments\n");
		exit(1);
	}

	base_lma = e_ph[order[0]].p_paddr;

	/* Process each LOAD segment in LMA order */
	for (i = 0; i < nload; i++) {
		int idx = order[i];
		int gap;

		/* Calculate gap between end of previous data and start of this segment */
		gap = (int)(e_ph[idx].p_paddr - base_lma) - bytes_written;
printf("# segment %d: LMA=0x%x VMA=0x%x filesz=0x%x gap=%d\n",
	idx, e_ph[idx].p_paddr, e_ph[idx].p_vaddr, e_ph[idx].p_filesz, gap);

		if (gap > 0)
			write_space(fd_d, gap);

		/* Read segment data from ELF and write to raw binary */
		lseek(fd_s, e_ph[idx].p_offset, SEEK_SET);
		readn(fd_s, buf, e_ph[idx].p_filesz);
		writen(fd_d, buf, e_ph[idx].p_filesz);

		bytes_written = (int)(e_ph[idx].p_paddr - base_lma)
				+ e_ph[idx].p_filesz;
	}

printf("# total output: %d bytes (0x%x)\n", bytes_written, bytes_written);
	close(fd_s);
	close(fd_d);
	exit(0);
}

/* readn --------------------------------------------------------------------*/
/* Read exactly 'len' bytes from file descriptor 'fd' into 'buf'.
 * Handles partial reads by looping until all bytes are read.
 * Returns the number of bytes successfully read, or -1 on error. */
int
readn(int fd, char* buf, int len)
{
	int	ret;
	int	total = 0;
	while (len > 0) {
		ret = read(fd, buf, len);
		if (ret < 0)
			return -1;
		buf += ret;
		len -= ret;
		total += ret;
	}
	return total;
}

/* writen -------------------------------------------------------------------*/
/* Write exactly 'len' bytes from 'buf' to file descriptor 'fd'.
 * Handles partial writes by looping until all bytes are written.
 * Returns the number of bytes successfully written, or -1 on error. */
int
writen(int fd, char* buf, int len)
{
	int	ret;
	int	total = 0;
	while (len > 0) {
		ret = write(fd, buf, len);
		if (ret < 0)
			return -1;
		buf += ret;
		len -= ret;
		total += ret;
	}
	return total;
}
