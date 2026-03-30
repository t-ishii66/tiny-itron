/* page private -------------------------------------------------------------*/
#ifndef _PAGEP_H
#define _PAGEP_H

/* Memory protection regions ------------------------------------------------*/
/*                                                                           */
/* Paging is used for access control only (U/S bit), not for page swapping.  */
/* All pages are always present (P=1) in physical memory.                    */
/* All pages use identity mapping (virtual = physical).                      */
/*                                                                           */
/* Region                         Access      Contents                       */
/* -------                        ------      --------                       */
/* 0x00000  - _user_text_start    Supervisor  Kernel .text/.bss/GDT/IDT      */
/* _user_text_start - _user_data_end  User(RW)  .user_text + .user_data      */
/* _user_data_end - 0x10FFFF      Supervisor  Unused kernel region           */
/* 0x110000 - 0x74FFFF            User(RW)    Memory pool + stack pool       */
/* 0x750000 - 0x7FFFFF            Supervisor  CPU stacks (Ring 0, TSS)       */
/*                                                                           */
/* Total mapped: 8 MB (2 page directory entries, 2 page tables)              */

/* number of 4MB regions to map (2 = 8MB) -----------------------------------*/
#define PAGE_TABLE_COUNT	2

/* boundary address for access control (page-aligned) -----------------------*/
#define USER_MEM_END	0x750000	/* end of user-accessible region */

#endif
