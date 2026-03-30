# Copyright (c) 2000 by t-ishii66. All rights reserved. -----------------#
# kernel library for TinyItron ------------------------------------------------#
#
# Kernel library (klib.s) -- C-callable wrappers for x86 privileged and
# special instructions.
#
# This file provides assembly-language helper functions that the C kernel
# code cannot express directly.  Every function here follows the cdecl
# calling convention:
#   - Arguments are pushed right-to-left onto the caller's stack.
#   - Return value (if any) is in %eax.
#   - The callee preserves %ebx, %esi, %edi, %ebp; may clobber %eax,
#     %ecx, %edx.
#
# Categories of functions provided:
#   1. I/O port access          -- inb, inw, outb, outw
#   2. Atomic operations        -- cxchg  (used by spinlocks)
#   3. Interrupt control        -- ccli, csti  (Ring 0 only)
#   4. CPU control              -- cltr, cwait
#   5. Initial task startup    -- start_first_task, start_second_task
#   6. Register / state queries -- get_cs, get_ds, get_ss, get_esp,
#                                  get_eflags
#   7. Interrupt-context helpers-- iget_uesp, iget_ueip
#   8. System call entry        -- syscall, syscall2
#
# -------------------------------------------------------------------------#

# ---- Global symbol exports ---------------------------------------------#
# Make each function visible to the linker so C code can call them.

.globl	inb
.globl	inw
.globl	outb
.globl	outw
.globl	ccli
.globl	csti
.globl	cltr
.globl	cwait
.globl	cxchg
.globl	test
#.globl	switch_task
.globl	start_first_task
.globl	start_second_task
.globl	get_cs
.globl	get_ds
.globl	get_ss
.globl	get_esp
.globl	get_eflags
.globl	iget_uesp
.globl	iget_ueip
.globl	syscall
.text

# =========================================================================
# inb -- Read a single byte from an I/O port
# =========================================================================
# C prototype:  unsigned char inb(unsigned short port);
#
# Reads one byte from the 16-bit I/O port address given by 'port' and
# returns it zero-extended in %eax.
#
# Stack frame on entry (after CALL):
#   8(%ebp) = port   (only low 16 bits used by IN instruction)
# -------------------------------------------------------------------------#
inb:
		pushl	%ebp              # Save caller's frame pointer
		movl	%esp, %ebp        # Establish our stack frame
		pushl	%edx              # Preserve %edx (callee-save courtesy)
		movl	8(%ebp), %edx     # Load 'port' argument into %edx
		movl	$0, %eax          # Zero %eax so upper bytes are clear
		inb	%dx, %al          # Read byte from port %dx into %al
		popl	%edx              # Restore %edx
		popl	%ebp              # Restore caller's frame pointer
		ret

# =========================================================================
# inw -- Read a 16-bit word from an I/O port
# =========================================================================
# C prototype:  unsigned short inw(unsigned short port);
#
# Same as inb but reads a 16-bit word.  Result is zero-extended in %eax.
#
# Stack frame on entry:
#   8(%ebp) = port
# -------------------------------------------------------------------------#
inw:
		pushl	%ebp              # Save caller's frame pointer
		movl	%esp, %ebp        # Establish stack frame
		pushl	%edx              # Preserve %edx
		movl	8(%ebp), %edx     # Load 'port' into %edx
		movl	$0, %eax          # Zero %eax so upper bytes are clear
		inw	%dx, %ax          # Read 16-bit word from port into %ax
		popl	%edx              # Restore %edx
		popl	%ebp              # Restore frame pointer
		ret
# =========================================================================
# outb -- Write a single byte to an I/O port
# =========================================================================
# C prototype:  void outb(unsigned short port, unsigned char val);
#
# Writes the byte 'val' to the I/O port 'port'.
#
# Stack frame on entry:
#   8(%ebp)  = port   (32-bit push, only low 16 bits matter)
#  12(%ebp)  = val    (32-bit push, only low 8 bits matter)
#
# Note: The 'andl $0xffff, %edx' masks the port to 16 bits explicitly,
# which is a safety measure since C pushes a full 32-bit value.
# -------------------------------------------------------------------------#
outb:
		pushl	%ebp              # Save caller's frame pointer
		movl	%esp, %ebp        # Establish stack frame
		pushl	%edx              # Preserve %edx
#		movl	8(%ebp), %dx      # (old code: load into 16-bit reg directly)
#		movl	10(%ebp), %ax     # (old code: assumed packed 16-bit args)
		movl	8(%ebp), %edx     # Load 'port' into full %edx
		movl	12(%ebp), %eax    # Load 'val' into %eax (low byte = data)
		andl	$0xffff, %edx	  # Mask to 16-bit port address
#		andl	$0xff, %eax	  # (optional: mask to 8 bits -- not needed,
		                          #  OUT only uses %al anyway)
		outb	%al, %dx          # Write byte %al to port %dx
#		out	%al, %dx          # (alternate mnemonic -- same instruction)
		popl	%edx              # Restore %edx
		popl	%ebp              # Restore frame pointer
		ret

# =========================================================================
# outw -- Write a 16-bit word to an I/O port
# =========================================================================
# C prototype:  void outw(unsigned short port, unsigned short val);
#
# Writes the 16-bit word 'val' to the I/O port 'port'.
#
# NOTE: There is a subtle frame-pointer bug here -- %edx is pushed
# before %ebp is set up, so the argument offsets from %ebp are shifted.
# This matches the original code exactly and is preserved as-is.
#
# Stack frame on entry (after pushl %ebp; pushl %edx; movl %esp,%ebp):
#   8(%ebp)  = port
#  12(%ebp)  = val
# -------------------------------------------------------------------------#
outw:
		pushl	%ebp              # Save caller's frame pointer
		pushl	%edx              # Preserve %edx (pushed before frame setup)
		movl	%esp, %ebp        # Establish stack frame (note: after %edx push)
		movl	8(%ebp), %edx     # Load 'port' into %edx
		movl	12(%ebp), %eax    # Load 'val' into %eax (low word = data)
		outw	%ax, %dx          # Write 16-bit word %ax to port %dx
		popl	%edx              # Restore %edx
		popl	%ebp              # Restore frame pointer
		ret
# =========================================================================
# cxchg -- Atomic exchange (used for spinlocks)
# =========================================================================
# C prototype:  int cxchg(int *addr, int val);
#
# Atomically swaps the value at memory location *addr with 'val'.
# Returns the OLD value that was at *addr.
#
# The XCHGL instruction has an implicit LOCK prefix, making it atomic
# even on SMP systems.  This is the basis of the kernel's spinlock
# implementation (smp_lock / smp_unlock).
#
# This instruction does NOT require Ring 0 privilege, so it can be
# called from user-mode (Ring 3) tasks as well.
#
# Stack frame on entry:
#   8(%ebp)  = addr   (pointer to the lock variable)
#  12(%ebp)  = val    (value to exchange in, e.g. 1 to acquire lock)
# -------------------------------------------------------------------------#
cxchg:
		pushl	%ebp              # Save caller's frame pointer
		movl	%esp, %ebp        # Establish stack frame
		push	%ebx              # Preserve %ebx (callee-saved register)
		movl	8(%ebp), %ebx     # Load 'addr' pointer into %ebx
		movl	12(%ebp), %eax    # Load 'val' into %eax
		xchgl	(%ebx), %eax     # Atomic swap: %eax <-> *(%ebx)
		                          # After this, %eax = old value from *addr
		                          # and *addr = val
		popl	%ebx              # Restore %ebx
		popl	%ebp              # Restore frame pointer
		ret                       # Return old value in %eax

# =========================================================================
# ccli -- Disable interrupts (Clear Interrupt Flag)
# =========================================================================
# C prototype:  void ccli(void);
#
# Wrapper for the x86 CLI instruction, which clears the IF (Interrupt
# Flag) in EFLAGS, preventing maskable hardware interrupts from being
# delivered to this CPU.
#
# IMPORTANT: This is a privileged instruction -- it can only be executed
# in Ring 0 (kernel mode).  Calling from Ring 3 (user mode) will trigger
# a General Protection Fault (#GP, exception 13).
#
# Used in syscall handlers and interrupt handlers for critical sections
# where interrupts must be temporarily disabled (cpu_lock/cpu_unlock).
# -------------------------------------------------------------------------#
ccli:
		cli                       # Clear Interrupt Flag (disable interrupts)
		ret
# =========================================================================
# csti -- Enable interrupts (Set Interrupt Flag)
# =========================================================================
# C prototype:  void csti(void);
#
# Wrapper for the x86 STI instruction, which sets the IF (Interrupt
# Flag) in EFLAGS, allowing maskable hardware interrupts to be delivered.
#
# Like CLI, this is Ring 0 only.  Calling from Ring 3 causes #GP.
#
# NOTE: After STI, the CPU guarantees that interrupts are not delivered
# until after the NEXT instruction completes (one-instruction delay).
# -------------------------------------------------------------------------#
csti:
		sti                       # Set Interrupt Flag (enable interrupts)
		ret

# =========================================================================
# cltr -- Load Task Register
# =========================================================================
# C prototype:  void cltr(unsigned short selector);
#
# Loads the Task Register (TR) with the given GDT selector, which must
# point to a valid TSS (Task State Segment) descriptor.  This tells the
# CPU which TSS describes the currently executing task.
#
# The TR is loaded once during boot for each CPU:
#   - CPU 0 loads SEL_TSS0 (0x38) before start_first_task
#   - CPU 1 loads SEL_TSS1 (0x40) before start_second_task
#
# This is a privileged instruction (Ring 0 only).
#
# Stack frame on entry:
#   8(%ebp) = selector (only low 16 bits used by LTR)
# -------------------------------------------------------------------------#
cltr:
		push	%ebp              # Save caller's frame pointer
		movl	%esp, %ebp        # Establish stack frame
		movl	8(%ebp), %eax     # Load selector argument (32 bits)
		ltr	%ax               # Load Task Register with 16-bit selector
		popl	%ebp              # Restore frame pointer
		ret
# =========================================================================
# cwait -- Wait for pending x87 FPU exceptions
# =========================================================================
# C prototype:  void cwait(void);
#
# Wrapper for the x86 FWAIT (WAIT) instruction.  This causes the CPU to
# check for and handle any pending unmasked x87 floating-point exceptions
# before proceeding.  In this kernel, it is rarely (if ever) used since
# no tasks perform FPU operations.
# -------------------------------------------------------------------------#
cwait:
		wait                      # Wait for pending FPU exceptions
		ret
# =========================================================================
# start_first_task -- Begin executing the first task on CPU 0 via iret
# =========================================================================
# C prototype:  void start_first_task(void);  /* does not return */
#
# Loads the Task Register with SEL_TSS0 so the CPU knows where to find
# esp0/ss0 for future Ring 3->0 interrupts, then loads the first task's
# kernel stack pointer and jumps to RESTORE_ALL + iret which pops the
# fake pt_regs frame built by proc_create() and enters Ring 3.
#
# This replaces the previous hardware TSS switch (ljmp) approach.
# Now the initial task startup uses the same RESTORE_ALL + iret path
# as all subsequent task switches, making the design uniform.
#
# Steps:
#   1. Load Task Register with SEL_TSS0 (0x38) via LTR.
#      This tells the CPU where to find esp0/ss0 for interrupts.
#      Unlike LJMP, LTR does NOT load any registers from the TSS.
#   2. Load ESP from current_proc[0]->kern_esp.
#      proc_create() has built a fake pt_regs frame + return address
#      on the kernel stack.
#   3. RET pops the return address (intr_return_restore) and jumps there.
#      RESTORE_ALL pops the fake pt_regs, iret enters Ring 3.
#
# This function transitions from Ring 0 boot code to Ring 3 user-mode
# task execution.  It does not return.
# -------------------------------------------------------------------------#
start_first_task:
		movw	$0x38, %ax        # SEL_TSS0
		ltr	%ax               # Load Task Register (no register swap)
		movl	current_proc, %ebx  # current_proc[0] = &proc[1]
		movl	(%ebx), %esp      # ESP = proc[1].kern_esp
		ret                       # -> intr_return_restore -> RESTORE_ALL -> iret

# =========================================================================
# start_second_task -- Begin executing the first task on CPU 1 via iret
# =========================================================================
# C prototype:  void start_second_task(void);  /* does not return */
#
# Same as start_first_task, but for CPU 1.  Uses SEL_TSS1 (0x40).
# Called by smp_ap_init() on CPU 1 after APIC initialization.
# -------------------------------------------------------------------------#
start_second_task:
		movw	$0x40, %ax        # SEL_TSS1
		ltr	%ax               # Load Task Register
		movl	current_proc+4, %ebx  # current_proc[1] = &proc[2]
		movl	(%ebx), %esp      # ESP = proc[2].kern_esp
		ret                       # -> intr_return_restore -> RESTORE_ALL -> iret

# =========================================================================
# switch_task -- Old software context switch (COMMENTED OUT / UNUSED)
# =========================================================================
# This was an early software-based context switch implementation that
# saved registers on the current task's stack and restored them from the
# next task's stack.  It has been replaced by the save/restore mechanism
# in intr.s which uses per-task register save areas (proc.reg[]).
#
# The original design:
#   C prototype:  void switch_task(int *old_esp_ptr, int new_esp);
#   - Save EBP, EAX, EBX, ECX, EDX on the current stack
#   - Store current ESP into *old_esp_ptr (first argument)
#   - Load ESP from new_esp (second argument)
#   - Restore registers from the new stack
#   - IRET to resume the new task
#
# Stack diagram before switch:
#   |    task-1 stack |
#   |    psw          |   <-- EFLAGS pushed by interrupt
#   |    task-1 PC    |   <-- return address pushed by CALL/interrupt
# -------------------------------------------------------------------------#
#               |    task-1 stack |
#               |    psw          |
#               |    task-1 PC    |

#switch_task:
#		pushl	%ebp
#		movl	%esp, %ebp
#		pushl	%eax
#		pushl	%ebx
#		pushl	%ecx
#		pushl	%edx
#
#		movl	8(%ebp), %ebx
#		movl	%esp, (%ebx)
#
#		movl	12(%ebp), %esp
#
#		popl	%edx
#		popl	%ecx
#		popl	%ebx
#		popl	%eax
#		popl	%ebp
#		iret


# =========================================================================
# Register / state query functions -- placed in .user_text for Ring 3 access
# =========================================================================
# These functions contain no privileged instructions and are safe to call
# from user mode (Ring 3).  They are placed in .user_text so that paging
# protection allows Ring 3 execution.
# -------------------------------------------------------------------------#
.section .user_text

# =========================================================================
# get_cs -- Get the current Code Segment register value
# =========================================================================
# C prototype:  unsigned short get_cs(void);
#
# Returns the current CS register value zero-extended to 32 bits.
# Useful for debugging to verify privilege level:
#   - CS = 0x20 means Ring 0 (kernel mode)
#   - CS = 0x5B means Ring 3 (user mode)
# The low 2 bits of CS encode the Current Privilege Level (CPL).
# -------------------------------------------------------------------------#
get_cs:
		xorl	%eax, %eax        # Zero %eax (clear upper 16 bits)
		mov	%cs, %ax          # Copy CS into low 16 bits of %eax
		ret
# =========================================================================
# get_ds -- Get the current Data Segment register value
# =========================================================================
# C prototype:  unsigned short get_ds(void);
#
# Returns the DS register value.
#   - DS = 0x28 means Ring 0 data segment
#   - DS = 0x60 means Ring 3 data segment
# -------------------------------------------------------------------------#
get_ds:
		xorl	%eax, %eax        # Zero %eax
		mov	%ds, %ax          # Copy DS into %ax
		ret
# =========================================================================
# get_ss -- Get the current Stack Segment register value
# =========================================================================
# C prototype:  unsigned short get_ss(void);
#
# Returns the SS register value.
#   - SS = 0x30 means Ring 0 stack segment
#   - SS = 0x6B means Ring 3 stack segment
# -------------------------------------------------------------------------#
get_ss:
		xorl	%eax, %eax        # Zero %eax
		mov	%ss, %ax          # Copy SS into %ax
		ret
# =========================================================================
# get_esp -- Get the current Stack Pointer
# =========================================================================
# C prototype:  unsigned int get_esp(void);
#
# Returns the current ESP value.  Note that this reflects the stack
# pointer AFTER the CALL instruction has pushed the return address,
# so it is offset by 4 bytes from the caller's perspective.
# Primarily used for debugging and diagnostics.
# -------------------------------------------------------------------------#
get_esp:
		xorl	%eax, %eax        # Zero %eax (redundant since movl follows)
		movl	%esp, %eax        # Copy current ESP into %eax
		ret
# =========================================================================
# get_eflags -- Get the current EFLAGS register
# =========================================================================
# C prototype:  unsigned int get_eflags(void);
#
# Returns the full 32-bit EFLAGS register.  Key flags:
#   - Bit 9 (IF): Interrupt Flag (1 = interrupts enabled)
#   - Bit 12-13 (IOPL): I/O Privilege Level
#   - Bit 0 (CF), Bit 6 (ZF), etc.: arithmetic flags
#
# PUSHFL pushes EFLAGS onto the stack, then POPL reads it into %eax.
# -------------------------------------------------------------------------#
get_eflags:
		pushfl	                  # Push EFLAGS onto stack
		popl	%eax              # Pop into %eax as return value
		ret

.text

# =========================================================================
# Interrupt-context helper functions
# =========================================================================
# These functions are called from within interrupt/syscall C handlers to
# inspect the interrupted task's register state on the stack.
# They rely on a specific call chain and stack layout.
# -------------------------------------------------------------------------#

# =========================================================================
# iget_uesp -- Get the user-mode ESP from the interrupt stack frame
# =========================================================================
# C prototype:  unsigned int iget_uesp(void);  /* call from ISR only */
#
# When a Ring 3 -> Ring 0 interrupt occurs, the CPU pushes the
# interrupted task's SS and ESP onto the kernel stack (in addition to
# EFLAGS, CS, EIP).  This function reads that saved user-mode ESP.
#
# The offset 32(%esp) accounts for the full call chain:
#   interrupt entry -> save (in intr.s) -> C handler -> iget_uesp
# Each level adds return addresses and saved registers to the stack,
# so the user ESP ends up at offset 32 from the current %esp.
#
# NOTE: The commented-out offsets (12, 28) are from earlier versions
# of the call chain before the stack layout was finalized.
# -------------------------------------------------------------------------#
iget_uesp:
		xorl	%eax, %eax        # Zero %eax (clear upper bits)
#		movl	12(%esp), %eax    # (old offset -- no longer correct)

#		movl	28(%esp), %eax    # (old offset -- no longer correct)
		movl	32(%esp), %eax    # Load saved user-mode ESP from stack
		ret
# =========================================================================
# iget_ueip -- Get the return address of the caller
# =========================================================================
# C prototype:  unsigned int iget_ueip(void);
#
# Returns the return address sitting on top of the stack, which is the
# address of the instruction after the CALL to iget_ueip.  This is
# primarily for debugging purposes (e.g., printing where a function
# was called from).
#
# Note: (%esp) after a CALL always holds the return address.
# -------------------------------------------------------------------------#
iget_ueip:
		xorl	%eax, %eax        # Zero %eax
		movl	(%esp), %eax      # Load return address from top of stack
		ret


# =========================================================================
# System call entry points
# =========================================================================

# =========================================================================
# syscall2 -- System call via call gate (UNUSED)
# =========================================================================
# C prototype:  int syscall2(...);
#
# Alternative system call mechanism using an LCALL through a call gate
# at GDT selector 0x70.  Call gates provide automatic Ring 3 -> Ring 0
# privilege transition with parameter copying.
#
# This entry point is currently unused; the kernel uses INT 0x99
# (via the 'syscall' function below) instead.
# -------------------------------------------------------------------------#
syscall2:
		lcall	$0x70, $0         # Far call through call gate selector 0x70
		ret                       # Return after syscall completes
# =========================================================================
# syscall -- System call via software interrupt INT 0x99
# =========================================================================
# C prototype:  int syscall(int func_code, ...);
#
# This is the primary system call entry point for user-mode tasks.
# It triggers software interrupt 0x99 (153), which vectors to
# intr_syscall in intr.s.  The interrupt mechanism automatically:
#   1. Switches from Ring 3 stack to Ring 0 kernel stack (via TSS)
#   2. Pushes SS, ESP, EFLAGS, CS, EIP onto the kernel stack
#   3. Jumps to the IDT handler for vector 0x99
#
# The syscall arguments are passed on the user-mode stack.  The kernel
# handler (c_intr_syscall) reads them via the saved user ESP.
#
# The return value from the kernel is written into the saved EAX slot
# in the task's register save area, so it appears in %eax when the
# task resumes.
#
# IMPORTANT: This function must be in .user_text because it is called
# from Ring 3 user tasks.  If it were in kernel .text (Supervisor only),
# the call instruction would trigger #PF.
# -------------------------------------------------------------------------#
.section .user_text
syscall:
		pushl	%ebp              # Save caller's frame pointer
		movl	%esp, %ebp        # Establish stack frame
		int	$0x99             # Trigger syscall interrupt (vector 153)
		popl	%ebp              # Restore frame pointer after return
		ret                       # Return to caller with result in %eax
.text
