# Copyright (c) 2000 by t-ishii66. All rights reserved. -----------------#
# run.s -- 32-bit protected mode bootstrap entry point -----------------------#
# SMP version: BSP uses CPU0_SP, AP uses CPU1_SP -----------------------------#
#
# This file is the first code that executes in 32-bit protected mode.
# start.s transitions from real mode to protected mode, sets up the GDT,
# enables PE in CR0, and then performs a far jump to SEL_K32_C:0x3400
# which lands here at the 'run' label.
#
# At this point:
#   - The CPU is in 32-bit flat protected mode (PE=1 in CR0)
#   - CS is already loaded with SEL_K32_C (32-bit kernel code, base 0, flat)
#   - All other segment registers (DS, ES, FS, GS, SS) still hold real-mode
#     values and must be reloaded with valid protected-mode selectors
#   - Paging is NOT enabled (linear address = physical address)
#
# Both CPUs (BSP and AP) enter through this same code path:
#   - BSP (CPU 0): Enters here on initial power-on boot
#   - AP  (CPU 1): Enters here after receiving SIPI from BSP. The AP
#                   re-executes start.s (at physical 0x3000, the SIPI
#                   vector) which transitions to protected mode and
#                   jumps here just like the BSP did.
#
# The global variable 'cpu_num' distinguishes BSP from AP:
#   - cpu_num == 0: This is the BSP (Boot Strap Processor, CPU 0)
#   - cpu_num != 0: This is an AP (Application Processor, CPU 1)
#   cpu_num is set by smp_init() on the BSP before sending SIPI to the AP.
#
# Each CPU gets its own stack region to avoid corruption:
#   - BSP stack pointer: 0x7A0000 (CPU0_SP) -- stack grows downward from here
#   - AP  stack pointer: 0x770000 (CPU1_SP) -- stack grows downward from here
#
# After setting up segments and stack, both CPUs call main().
# main() internally checks cpu_num to determine whether to run the BSP
# initialization path (setting up devices, creating tasks, starting Task 1)
# or the AP initialization path (APIC init, starting Task 2).
#
# GDT selectors used here:
#   0x28 = SEL_K32_D  -- 32-bit kernel data segment (base 0, limit 4GB, flat)
#   0x30 = SEL_K32_S  -- 32-bit kernel stack segment (base 0, limit 4GB, flat)
#   (CS was already set to SEL_K32_C by the far jump from start.s)
#
#-----------------------------------------------------------------------------#

		.text

# Export 'run' so start.s can reference it in the far jump target.
.globl		run

# Import 'cpu_num' -- defined in C code (smp.c). Indicates which CPU we are.
.extern		cpu_num

#-----------------------------------------------------------------------------#
# run -- 32-bit protected mode entry point
#
# Called from start.s via far jump (ljmp SEL_K32_C, $0x3400).
# Interrupts may be in an undefined state from the mode switch, so we
# immediately disable them. Interrupts will not be re-enabled until
# start_first_task() / start_second_task() loads a TSS with EFLAGS.IF=1.
#-----------------------------------------------------------------------------#
run:
		# Disable interrupts immediately. We are in a fragile state:
		# segment registers still hold real-mode values, and no valid
		# IDT is set up yet. Any interrupt here would triple-fault.
		cli

		# Load all data segment registers with SEL_K32_D (0x28).
		# This is a flat 32-bit kernel data segment with base=0 and
		# limit=4GB, giving us direct access to all physical memory.
		# We cannot use a 32-bit immediate move to a segment register,
		# so we go through AX as an intermediary.
		mov	$0x28, %ax
		mov	%ax, %ds	# DS: default data segment for most instructions
		mov	%ax, %es	# ES: used by string instructions (movs, stos, etc.)
		mov	%ax, %fs	# FS: general purpose, not currently used by kernel
		mov	%ax, %gs	# GS: general purpose, not currently used by kernel

		# Load SS with SEL_K32_S (0x30).
		# This is the 32-bit kernel stack segment. Like DS, it is flat
		# with base=0 and limit=4GB. A separate selector is used so the
		# GDT can distinguish data vs stack access types if needed.
		# Note: ESP is not set yet -- we set it below after determining
		# which CPU we are (BSP vs AP need different stack regions).
		mov	$0x30, %ax
		mov	%ax, %ss

		#-----------------------------------------------------------------#
		# Determine which CPU we are by checking the global 'cpu_num'.
		#
		# cpu_num starts as 0 (BSS zero-initialized). The BSP hits this
		# code first and sees cpu_num==0, taking the BSP path.
		#
		# Later, the BSP calls smp_init() which sets cpu_num=1 and sends
		# a SIPI to the AP. The AP re-executes start.s -> run.s and
		# arrives here, seeing cpu_num!=0, taking the AP path.
		#-----------------------------------------------------------------#
		cmpl	$0, cpu_num
		jne	ap_entry

		#-----------------------------------------------------------------#
		# BSP path (CPU 0)
		#-----------------------------------------------------------------#

		# Set the BSP stack pointer to CPU0_SP (0x7A0000).
		# The x86 stack grows downward, so this is the TOP of the BSP's
		# stack region. The stack will grow toward lower addresses as
		# functions are called and local variables are allocated.
		movl	$0x07a0000, %esp

		# Ensure interrupts are disabled before calling main().
		# (Redundant with the cli above, but serves as a safety measure
		# in case any code path could reach here with IF=1.)
		cli

		# Call main() -- the C entry point for kernel initialization.
		# For the BSP, main() will:
		#   1. Initialize all kernel subsystems (IDT, PIC, APIC, etc.)
		#   2. Initialize ITRON data structures (tasks, semaphores, etc.)
		#   3. Create initial tasks (Task 1 on CPU 0, Task 2 on CPU 1)
		#   4. Call start_first_task() which does a hardware TSS switch
		#      to Task 1, entering Ring 3 with interrupts enabled.
		#      start_first_task() never returns.
		#
		# If main() somehow returns (it should not), fall through to halt.
		call	main
		jmp	halt

		#-----------------------------------------------------------------#
		# AP path (CPU 1)
		#-----------------------------------------------------------------#
ap_entry:
		# Set the AP stack pointer to CPU1_SP (0x770000).
		# This is a separate stack region from the BSP to prevent the
		# two CPUs from corrupting each other's stack frames. The AP
		# stack also grows downward from this address.
		movl	$0x0770000, %esp

		# Ensure interrupts are disabled before calling main().
		# The AP must not take interrupts until its Local APIC is
		# properly initialized and its first task is running in Ring 3.
		cli

		# Call main() -- for the AP, main() will:
		#   1. Detect cpu_num != 0 and call smp_ap_init()
		#   2. smp_ap_init() initializes the AP's Local APIC
		#   3. Call start_second_task() which does a hardware TSS switch
		#      to Task 2, entering Ring 3 with interrupts enabled.
		#      start_second_task() never returns.
		#
		# If main() somehow returns (it should not), fall through to halt.
		call	main

		#-----------------------------------------------------------------#
		# halt -- infinite loop for error recovery
		#
		# Neither the BSP nor the AP should ever reach here. main() is
		# expected to enter a task via TSS switch and never return.
		# If we do end up here, just spin forever. The CPU will continue
		# to consume power but will not execute any further useful code.
		# Interrupts remain disabled (cli was called above), so this CPU
		# is effectively dead.
		#-----------------------------------------------------------------#
halt:
		jmp	halt

#-----------------------------------------------------------------------------#
# Data section
#-----------------------------------------------------------------------------#
.data
.align	4

# stack_ptr -- legacy far pointer structure (UNUSED)
#
# This appears to be a leftover from an earlier version of the bootstrap
# that may have used 'lss' (load far pointer into SS:ESP) to set up the
# stack in a single instruction. The offset 0x70000 and selector 0x30
# would have loaded SS=0x30 and ESP=0x70000 simultaneously.
#
# This is no longer used -- the current code loads SS and ESP separately,
# and each CPU gets its own stack pointer (0x7A0000 or 0x770000).
# Retained for historical reference.
stack_ptr:
		.long	0x0070000	# offset
		.word	0x30		# selector
.align	4
