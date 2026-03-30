/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* page -- Page tables for memory protection (U/S bit only)                 */
/*                                                                          */
/* Purpose:                                                                 */
/*   This kernel does NOT perform page swapping.  All pages are always      */
/*   present (P=1) and use identity mapping (virtual = physical).           */
/*   The SOLE purpose of enabling i386 paging (CR0.PG=1) is to enforce     */
/*   memory protection via the U/S (User/Supervisor) bit in page table     */
/*   entries:                                                               */
/*     U/S = 0 (Supervisor) -- Ring 0 only; Ring 3 access triggers #PF     */
/*     U/S = 1 (User)       -- Ring 3 can also access the page             */
/*                                                                          */
/*   Without paging, the flat GDT segments (base=0, limit=4GB) would let   */
/*   user tasks read/write all physical memory, including the kernel.       */
/*   Paging + U/S bits add the missing layer of protection.                 */
/*                                                                          */
/* How code gets to its address (build pipeline context):                   */
/*   The kernel binary is linked at a fixed address (". = 0x3400" in        */
/*   kernel.ld) with -fno-pie, producing absolute-addressed code.           */
/*   elf.c converts the ELF to a flat binary (zero-padding BSS).            */
/*   boot.s loads this flat binary to linear address 0x3400 by reading      */
/*   floppy sectors sequentially into memory starting at 0x2000.            */
/*   No relocation or copying occurs after loading -- the code runs         */
/*   in-place at the address the linker assumed.                            */
/*                                                                          */
/*   .user_text is placed at a page boundary by kernel.ld (ALIGN(0x1000)   */
/*   after .bss).  elf.c bakes this alignment into the flat binary as       */
/*   zero padding, so boot.s's sequential load places .user_text at a       */
/*   page-aligned address automatically.  The actual address depends on     */
/*   kernel size -- it is NOT hardcoded.  page_init() reads the address     */
/*   from the linker symbol _user_text_start at runtime.                    */
/*                                                                          */
/* Memory map (User = U/S=1, Supervisor = U/S=0):                          */
/*                                                                          */
/*   Address range                   Access       Contents                  */
/*   ─────────────────────────────   ──────────   ─────────────────────     */
/*   0x00000 .. _user_text_start     Supervisor   Kernel .text/.bss/GDT/IDT */
/*   _user_text_start .. _user_data_end  User+RW  .user_text + .user_data  */
/*   _user_data_end .. 0x10FFFF      Supervisor   (gap)                     */
/*   0x110000 .. 0x74FFFF            User+RW      mem pool + stack pool     */
/*   0x750000 .. 0x7FFFFF            Supervisor   CPU stacks (Ring 0)       */
/*   0xFEE00000 (1 page)             Supervisor   Local APIC MMIO          */
/*                                                                          */
/*   _user_text_start / _user_data_end are linker symbols defined in        */
/*   kernel.ld.  Their values change when kernel code grows.  page_init()   */
/*   reads them at runtime, so no manual update is needed.                  */
/*   MEM_START (0x110000) and USER_MEM_END (0x750000) are fixed constants   */
/*   defined in addr.h and pageP.h respectively.                            */
/*                                                                          */
/* i386 page table structure:                                               */
/*                                                                          */
/*   CR3 -> page_dir[] (1024 entries, 4KB)                                  */
/*     entry[0] -> page_table[0]  covers 0x000000 - 0x3FFFFF (4MB)         */
/*     entry[1] -> page_table[1]  covers 0x400000 - 0x7FFFFF (4MB)         */
/*     entry[0x3FB] -> page_table_apic  covers APIC at 0xFEE00000          */
/*     other entries -> not present                                         */
/*                                                                          */
/*   Each page table has 1024 entries, each mapping one 4KB page.           */

#include "addr.h"
#include "page.h"
#include "pageP.h"

/* page directory and page tables -------------------------------------------*/
/* Must be aligned to 4KB (page size) boundaries.  The CPU requires this   */
/* because the low 12 bits of CR3 and page directory entries are used for   */
/* flags, not address bits.                                                */
static unsigned long page_dir[PAGES_PER_TABLE]
		__attribute__((aligned(PAGE_SIZE)));
static unsigned long page_table[PAGE_TABLE_COUNT][PAGES_PER_TABLE]
		__attribute__((aligned(PAGE_SIZE)));
static unsigned long page_table_apic[PAGES_PER_TABLE]
		__attribute__((aligned(PAGE_SIZE)));

/* page_init ----------------------------------------------------------------*/
/* Build identity-mapped page tables with two User (U/S=1) regions:         */
/*                                                                          */
/*   Region 1: .user_text + .user_data                                      */
/*     Boundaries obtained from linker symbols _user_text_start and         */
/*     _user_data_end (defined in kernel.ld).  These are NOT hardcoded      */
/*     constants -- they move when kernel code grows, because kernel.ld     */
/*     places .user_text at ALIGN(0x1000) after .bss.  This function        */
/*     rounds them to page boundaries (u_start, u_end) at runtime.          */
/*                                                                          */
/*   Region 2: memory pool + stack pool  (MEM_START .. USER_MEM_END)        */
/*     Fixed constants: 0x110000 .. 0x750000 (defined in addr.h / pageP.h)  */
/*                                                                          */
/*   Everything else is Supervisor (U/S=0):                                 */
/*     - Kernel code/data/BSS (0x0 .. _user_text_start)                     */
/*     - Gap between user data and pools                                    */
/*     - CPU stacks (0x750000 .. 0x7FFFFF)                                  */
/*     - VGA buffer at 0xB8000 (falls within kernel region)                 */
/*                                                                          */
/* All pages use identity mapping (virtual = physical).  No page is ever    */
/* marked not-present.  Paging is NOT enabled here -- call page_enable().   */
/*                                                                          */
/* PTE format:  [31:12] phys addr | [2] U/S | [1] R/W | [0] Present        */

/* linker symbols -- their ADDRESS is the value, not their content.         */
/* _user_text_start = start of .user_text section (page-aligned by ALIGN)   */
/* _user_data_end   = end of .user_data section (may not be page-aligned)   */
/* page_init() rounds these to page boundaries for the PTE loop.            */
extern char _user_text_start, _user_data_end;

void
page_init(void)
{
	int		i;

	/* Get .user_text/.user_data boundaries from linker symbols and
	 * round to page boundaries.
	 * u_start: round DOWN to include the first page of .user_text
	 * u_end:   round UP to include the last page of .user_data
	 * Example: if _user_text_start=0x1E000, _user_data_end=0x1EF40
	 *          then u_start=0x1E000, u_end=0x1F000 (2 pages) */
	unsigned long	u_start = (unsigned long)&_user_text_start
					& ~(PAGE_SIZE - 1);
	unsigned long	u_end   = ((unsigned long)&_user_data_end
					+ PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	/* --- Step 1: Clear the page directory ---------------------------------*/
	/* All 1024 entries are set to 0 (not present).  Only the first          */
	/* PAGE_TABLE_COUNT entries will be filled in below.                     */
	for (i = 0 ; i < PAGES_PER_TABLE ; i ++)
		page_dir[i] = 0;

	/* --- Step 2: Fill page tables ----------------------------------------*/
	/* Identity map every page (addr = virtual = physical).                 */
	/* Default: Supervisor + RW.  Mark two regions as User:                 */
	/*   (a) u_start..u_end    -- .user_text + .user_data (linker-derived)  */
	/*   (b) MEM_START..USER_MEM_END -- memory pool + stack pool (fixed)    */
	/* Anything outside these two regions is Supervisor-only (U/S=0):       */
	/*   kernel code at 0x3400, VGA at 0xB8000, CPU stacks at 0x750000+    */
	for (i = 0 ; i < PAGE_TABLE_COUNT ; i ++) {
		int	j;
		for (j = 0 ; j < PAGES_PER_TABLE ; j ++) {
			unsigned long addr = (i * PAGES_PER_TABLE + j)
						* PAGE_SIZE;
			unsigned long flags = PTE_PRESENT | PTE_RW;

			if (addr >= u_start && addr < u_end)
				flags |= PTE_USER;	/* (a) .user_text + .user_data */
			else if (addr >= MEM_START && addr < USER_MEM_END)
				flags |= PTE_USER;	/* (b) memory + stack pools */

			page_table[i][j] = addr | flags;
		}
	}

	/* --- Step 3: Set up page directory entries ----------------------------*/
	/* Each page directory entry points to a page table.                    */
	/* The flags on the directory entry are ORed with the page table        */
	/* entry flags by the CPU.  We set U/S=1 and R/W=1 here so that        */
	/* the per-page flags in the page table have full control.              */
	for (i = 0 ; i < PAGE_TABLE_COUNT ; i ++) {
		page_dir[i] = (unsigned long)page_table[i]
				| PTE_PRESENT | PTE_RW | PTE_USER;
	}

	/* --- Step 4: Map Local APIC MMIO region (0xFEE00000) -----------------*/
	/* The Local APIC registers are at physical 0xFEE00000-0xFEE00FFF.      */
	/* save/restore in intr.s read APIC_ID (0xFEE00020) on every interrupt. */
	/* Without this mapping, any interrupt after paging is enabled would     */
	/* trigger #PF.  Marked Supervisor + cache-disable (MMIO).              */
	{
		int pd_idx = 0xFEE00000 >> 22;           /* = 0x3FB */
		int pt_idx = (0xFEE00000 >> 12) & 0x3FF; /* = 0x200 */

		for (i = 0; i < PAGES_PER_TABLE; i++)
			page_table_apic[i] = 0;

		page_table_apic[pt_idx] = 0xFEE00000
				| PTE_PRESENT | PTE_RW | PTE_PCD;

		page_dir[pd_idx] = (unsigned long)page_table_apic
				| PTE_PRESENT | PTE_RW;
	}
}

/* page_get_dir -------------------------------------------------------------*/
/* Return the physical address of the page directory for loading into CR3.  */
unsigned long
page_get_dir(void)
{
	return (unsigned long)page_dir;
}

/* page_enable --------------------------------------------------------------*/
/* Load the page directory into CR3 and enable paging by setting CR0.PG.    */
/* Must be called after page_init() has built the page tables.              */
/* Identity mapping ensures this code continues to execute correctly after   */
/* paging is turned on (the instruction following CR0 write is still at     */
/* the same virtual = physical address).                                     */
void
page_enable(void)
{
	unsigned long dir = page_get_dir();
	__asm__ volatile(
		"movl %0, %%cr3\n\t"
		"movl %%cr0, %%eax\n\t"
		"orl $0x80000000, %%eax\n\t"
		"movl %%eax, %%cr0\n\t"
		: : "r"(dir) : "eax"
	);
}
