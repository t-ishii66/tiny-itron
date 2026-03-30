# Copyright (c) 2000 by t-ishii66. All rights reserved. -----------------#
# assembler interrupt entry --------------------------------------------------#
# 32 bit protect mode --------------------------------------------------------#
# SMP (2-CPU) version --------------------------------------------------------#
#
# ===========================================================================#
# OVERVIEW: Interrupt Entry/Exit for an SMP ITRON Kernel (i386)
# ===========================================================================#
#
# This file implements the low-level interrupt entry and exit paths for a
# 2-CPU (SMP) ITRON RTOS kernel running in i386 protected mode.
#
# --------------------------------------------------------------------------
# KEY CONCEPTS
# --------------------------------------------------------------------------
#
# 1. Per-Task Kernel Stacks
#    ~~~~~~~~~~~~~~~~~~~~~~
#    Each task has its own 4KB kernel stack.  When a Ring 3->Ring 0 interrupt
#    occurs, the CPU switches ESP to the task's kernel stack (via TSS.esp0).
#    SAVE_ALL pushes all registers onto this stack, and RESTORE_ALL pops
#    them back.  On task switch, the kernel simply saves the current ESP
#    into proc.kern_esp and loads the new task's ESP -- all register state
#    is preserved on each task's own kernel stack.
#
#    This is the standard pattern used by Linux and other production kernels,
#    making the code easier to understand for OS students.
#
# 2. SAVE_ALL / RESTORE_ALL Macros
#    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    SAVE_ALL pushes 9 registers (EAX, ECX, EDX, EBX, EBP, ESI, EDI, DS, ES)
#    onto the kernel stack, then reloads DS/ES with the kernel data segment.
#    Combined with the CPU-pushed interrupt frame (EIP, CS, EFLAGS, ESP, SS),
#    this creates a complete pt_regs frame on the stack:
#
#      Offset  Register    Pushed by
#      ------  --------    ---------
#      0x00    ES          SAVE_ALL
#      0x04    DS          SAVE_ALL
#      0x08    EDI         SAVE_ALL
#      0x0C    ESI         SAVE_ALL
#      0x10    EBP         SAVE_ALL
#      0x14    EBX         SAVE_ALL
#      0x18    EDX         SAVE_ALL
#      0x1C    ECX         SAVE_ALL
#      0x20    EAX         SAVE_ALL
#      0x24    EIP         CPU
#      0x28    CS          CPU
#      0x2C    EFLAGS      CPU
#      0x30    ESP         CPU (Ring 3->0 only)
#      0x34    SS          CPU (Ring 3->0 only)
#
# 3. Per-CPU State via APIC ID
#    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    Both CPUs share the same code.  The APIC ID register at 0xFEE00020
#    (bits [31:24]) distinguishes CPU 0 from CPU 1.  Each CPU has its own:
#      - current_proc[cpu]  (pointer to the running task's proc_t)
#      - k_nest[cpu]        (interrupt nesting depth counter)
#
# 4. Interrupt Nesting (k_nest[])
#    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    intr_enter increments k_nest[cpu]; intr_leave decrements it.
#    A task switch is ONLY attempted when k_nest reaches 0 (returning
#    from the outermost interrupt).
#
# 5. Task Switching in intr_leave
#    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    When nest count reaches 0, intr_leave:
#      a. Saves current ESP into current_proc[cpu]->kern_esp
#      b. Calls sched_next_tsk_check(cpu) which may update current_proc
#      c. Loads ESP from (possibly new) current_proc[cpu]->kern_esp
#      d. Updates TSS.esp0 to the new task's kern_stack_top
#    RESTORE_ALL then pops registers from the (possibly new) kernel stack,
#    and iret returns to the (possibly new) task.
#
# ===========================================================================#

# --- Global symbols: exception handlers (IDT vectors 0-15) -----------------#
.globl		intr_divide              # Vector 0:  Divide-by-zero (#DE)
.globl		intr_singlestep          # Vector 1:  Debug / Single-step (#DB)
.globl		intr_nmi                 # Vector 2:  Non-Maskable Interrupt
.globl		intr_breakpoint          # Vector 3:  Breakpoint (#BP)
.globl		intr_overflow            # Vector 4:  Overflow (#OF)
.globl		intr_bounds              # Vector 5:  Bound Range Exceeded (#BR)
.globl		intr_opcode              # Vector 6:  Invalid Opcode (#UD)
.globl		intr_copr_not_available  # Vector 7:  Device Not Available (#NM)
.globl		intr_doublefault         # Vector 8:  Double Fault (#DF)
.globl		intr_copr_seg_overrun    # Vector 9:  Coprocessor Segment Overrun
.globl		intr_tss                 # Vector 10: Invalid TSS (#TS)
.globl		intr_segment_not_present # Vector 11: Segment Not Present (#NP)
.globl		intr_stack               # Vector 12: Stack-Segment Fault (#SS)
.globl		intr_general             # Vector 13: General Protection (#GP)
.globl		intr_page                # Vector 14: Page Fault (#PF)
.globl		intr_copr_error          # Vector 16: x87 FPU Error (#MF)

# --- Global symbols: hardware IRQ handlers (PIC, vectors 32-47) ------------#
.globl		intr_irq0               # IRQ 0:  PIT timer (CPU 0 only)
.globl		intr_irq1               # IRQ 1:  Keyboard (CPU 0 only)
.globl		intr_irq2               # IRQ 2:  Cascade (PIC2 -> PIC1)
.globl		intr_irq3               # IRQ 3:  COM2 / COM4
.globl		intr_irq4               # IRQ 4:  COM1 / COM3
.globl		intr_irq5               # IRQ 5:  LPT2 / Sound card
.globl		intr_irq6               # IRQ 6:  Floppy disk controller
.globl		intr_irq7               # IRQ 7:  LPT1 / Spurious
.globl		intr_irq8               # IRQ 8:  RTC (CMOS real-time clock)
.globl		intr_irq9               # IRQ 9:  Redirected IRQ 2
.globl		intr_irq10              # IRQ 10: Available
.globl		intr_irq11              # IRQ 11: Available
.globl		intr_irq12              # IRQ 12: PS/2 Mouse
.globl		intr_irq13              # IRQ 13: FPU / Coprocessor
.globl		intr_irq14              # IRQ 14: Primary ATA (Hard disk)
.globl		intr_irq15              # IRQ 15: Secondary ATA

# --- Global symbols: software interrupt and APIC timers --------------------#
.globl		intr_syscall             # System call entry (int $0x99)
.globl		intr_smp_timer0          # Local APIC timer for CPU 0
.globl		intr_smp_timer1          # Local APIC timer for CPU 1

# --- Global symbols: misc --------------------------------------------------#
.globl		intr_default             # Catch-all default interrupt handler
.globl		user_restore             # Syscall return path (task exception support)

# --- External symbols (defined in C) ---------------------------------------#
.extern		current_proc             # proc_t* current_proc[2]: per-CPU current task
.extern		sleep_current_proc       # Function to put current task to sleep
.extern		ena_tex                  # Function to enable task exceptions
.extern		sched_next_tsk_check     # int sched_next_tsk_check(int apic)
.extern		tss_update_esp0          # void tss_update_esp0(int cpu, unsigned long esp0)

# --- Local APIC ID Register ------------------------------------------------#
.equ APIC_ID_REG, 0xFEE00020

# --- Per-CPU interrupt nesting counters ------------------------------------#
.globl		k_nest0
.globl		k_nest1
k_nest0:	.long	0                # Interrupt nesting depth for CPU 0
k_nest1:	.long	0                # Interrupt nesting depth for CPU 1

# ===========================================================================#
# SAVE_ALL / RESTORE_ALL Macros
# ===========================================================================#

# SAVE_ALL: push all general-purpose registers and segment registers.
# On entry, the CPU has already pushed (from high to low address):
#   SS, ESP, EFLAGS, CS, EIP    (Ring 3->0 privilege transition)
# After this macro, ESP points to a complete pt_regs frame (14 dwords).
#
# Stack grows downward.  After each push, ESP -= 4:
.macro SAVE_ALL
	pushl	%eax		# [ESP+0x20] EAX (syscall number if int $0x99)
	pushl	%ecx		# [ESP+0x1C] ECX
	pushl	%edx		# [ESP+0x18] EDX
	pushl	%ebx		# [ESP+0x14] EBX
	pushl	%ebp		# [ESP+0x10] EBP
	pushl	%esi		# [ESP+0x0C] ESI
	pushl	%edi		# [ESP+0x08] EDI
	pushl	%ds		# [ESP+0x04] DS  (user DS before switch)
	pushl	%es		# [ESP+0x00] ES  (user ES before switch)
	# Now reload DS/ES with kernel data segment so C code can
	# access kernel data.  User values are saved on the stack above.
	movw	$0x28, %ax	# SEL_K32_D (GDT index 5, RPL=0)
	movw	%ax, %ds
	movw	%ax, %es
	# ESP now points to the base of pt_regs.
	# Layout (all offsets from ESP):
	#   0x00=ES 0x04=DS 0x08=EDI 0x0C=ESI 0x10=EBP 0x14=EBX
	#   0x18=EDX 0x1C=ECX 0x20=EAX 0x24=EIP 0x28=CS 0x2C=EFLAGS
	#   0x30=ESP(user) 0x34=SS(user)   [last two: Ring 3->0 only]
.endm

# RESTORE_ALL: pop all registers in reverse order of SAVE_ALL.
# ESP must point to the base of a pt_regs frame.
# After this macro, ESP points to the CPU interrupt frame (EIP,CS,EFLAGS,...),
# ready for iret.
.macro RESTORE_ALL
	popl	%es		# restore user ES
	popl	%ds		# restore user DS
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	popl	%edx
	popl	%ecx
	popl	%eax
	# ESP now points to EIP -- iret will pop EIP, CS, EFLAGS
	# (and ESP, SS if returning to Ring 3).
.endm

# ===========================================================================#
# intr_enter -- Increment per-CPU interrupt nesting counter
# ===========================================================================#
# Called after SAVE_ALL.  Determines which CPU via APIC ID and increments
# the corresponding k_nest counter.
#
intr_enter:
	movl	APIC_ID_REG, %eax	# read Local APIC ID register (MMIO)
	shrl	$24, %eax		# APIC ID is in bits [31:24]; EAX = 0 or 1
	testl	%eax, %eax		# CPU 0 (BSP) has APIC ID 0
	jnz	1f			# jump if CPU 1 (AP)
	incl	k_nest0			# CPU 0: k_nest0++
	ret
1:
	incl	k_nest1			# CPU 1: k_nest1++
	ret

# ===========================================================================#
# intr_leave -- Decrement nesting counter, do task switch if outermost
# ===========================================================================#
# Called after the C handler returns and before RESTORE_ALL.
# If this is the outermost interrupt return (k_nest reaches 0):
#   1. Save current ESP to current_proc[cpu]->kern_esp
#   2. Call sched_next_tsk_check(cpu) -- may change current_proc[cpu]
#   3. Load ESP from (possibly new) current_proc[cpu]->kern_esp
#   4. Update TSS.esp0 to new task's kern_stack_top
#
intr_leave:
	movl	APIC_ID_REG, %eax	# read APIC ID to identify this CPU
	shrl	$24, %eax		# EAX = 0 (CPU 0) or 1 (CPU 1)
	testl	%eax, %eax
	jnz	intr_leave_cpu1		# branch to CPU 1 path

# ------------- CPU 0 path -------------------------------------------------#
intr_leave_cpu0:
	decl	k_nest0			# k_nest0-- (one fewer nesting level)
	jnz	intr_leave_done		# still nested -- skip task switch

	# --- Outermost interrupt return on CPU 0. ---
	# This is the only point where a task switch can occur.
	#
	# current_proc is a C array: proc_t* current_proc[2].
	# current_proc[0] is at address 'current_proc' (offset +0 for CPU 0).
	# proc_t layout: { kern_esp (offset 0), kern_stack_top (offset 4), ... }

	# Step 1: Save current ESP into the running task's proc_t.
	#   EBX = current_proc[0] (pointer to proc_t)
	#   (%ebx) = proc_t.kern_esp (first field, offset 0)
	#   ESP currently points to the pt_regs frame on this task's kernel stack.
	movl	current_proc, %ebx	# EBX = current_proc[0]
	movl	%esp, (%ebx)		# current_proc[0]->kern_esp = ESP

	# Step 2: Let the scheduler decide whether to switch tasks.
	#   sched_next_tsk_check(0) may change current_proc[0] to a
	#   different task's proc_t if a higher-priority task is ready.
	pushl	$0			# arg: apic = 0 (cdecl calling convention)
	call	sched_next_tsk_check	# int sched_next_tsk_check(int apic)
	addl	$4, %esp		# clean up argument (cdecl caller cleans)

	# Step 3: Load ESP from (possibly new) current_proc[0]->kern_esp.
	#   If the scheduler switched tasks, current_proc[0] now points to the
	#   NEW task's proc_t, so we load the new task's kernel stack pointer.
	#   If no switch, we reload our own ESP (same value we saved in step 1).
	#   THIS IS THE ACTUAL CONTEXT SWITCH -- ESP now belongs to the new task.
	movl	current_proc, %ebx	# re-read: may have changed in step 2
	movl	(%ebx), %esp		# ESP = new task's kern_esp

	# Step 4: Update TSS.esp0 for the new task.
	#   TSS.esp0 tells the CPU where to switch ESP on the NEXT Ring 3->0
	#   transition.  Must point to the top of the new task's kernel stack.
	#   4(%ebx) = proc_t.kern_stack_top (second field, offset 4).
	pushl	4(%ebx)			# arg2: kern_stack_top (new esp0 value)
	pushl	$0			# arg1: cpu = 0
	call	tss_update_esp0		# tss_update_esp0(cpu, esp0)
	addl	$8, %esp		# clean up 2 arguments

	ret				# return to intr_return (or intr_return_restore)

# ------------- CPU 1 path -------------------------------------------------#
# Same logic as CPU 0 but uses current_proc[1] (at address current_proc+4)
# and k_nest1.
intr_leave_cpu1:
	decl	k_nest1			# k_nest1--
	jnz	intr_leave_done		# still nested -- skip task switch

	# Step 1: Save ESP into current_proc[1]->kern_esp
	movl	current_proc+4, %ebx	# EBX = current_proc[1]
	movl	%esp, (%ebx)		# current_proc[1]->kern_esp = ESP

	# Step 2: Scheduler check (may switch current_proc[1])
	pushl	$1			# arg: apic = 1
	call	sched_next_tsk_check
	addl	$4, %esp

	# Step 3: Load (possibly new) ESP
	movl	current_proc+4, %ebx	# re-read current_proc[1]
	movl	(%ebx), %esp		# ESP = new task's kern_esp

	# Step 4: Update TSS.esp0 for the new task on CPU 1
	pushl	4(%ebx)			# arg2: kern_stack_top
	pushl	$1			# arg1: cpu = 1
	call	tss_update_esp0
	addl	$8, %esp

	ret

intr_leave_done:
	ret				# nested interrupt: just return to caller

# ===========================================================================#
# Common interrupt return path
# ===========================================================================#
# All interrupt/syscall stubs jump here after the C handler returns.
#
intr_return:
	call	intr_leave		# k_nest--, possible task switch (ESP swap)
	# Fall through to intr_return_restore.
	#
	# Why does this work for task switch?
	#   intr_leave's "ret" pops a return address from the stack.
	#   - If no task switch: ret returns here (the address after "call").
	#   - If task switch: the new task's kern_esp was set up by
	#     proc_create() to have the address of intr_return_restore
	#     at the position "ret" pops from -- so "ret" still lands here.
	#
.globl	intr_return_restore
intr_return_restore:
	RESTORE_ALL			# pop ES,DS,EDI,ESI,EBP,EBX,EDX,ECX,EAX
	iret				# pop EIP,CS,EFLAGS (and ESP,SS if Ring 3)

# ===========================================================================#
# user_restore -- Special return path for system calls with task exceptions
# ===========================================================================#
#
# When a task exception handler is invoked, the user stack has been set up as:
#   [ESP+0]   return addr (user_restore)
#   [ESP+4]   texptn
#   [ESP+8]   exinf
#   [ESP+12]  EFLAGS
#   [ESP+16]  EDI .. EAX
#   [ESP+40]  original EIP
#
user_restore:
	# On entry, user stack looks like (set up by stack_adjust in interrupt.c):
	#   ESP+0:  (return addr -- we're here via "ret" from exception handler)
	#   ESP+0:  texptn       (task exception pattern, consumed by handler)
	#   ESP+4:  exinf        (extended info, consumed by handler)
	#   ESP+8:  EFLAGS       (saved before exception handler ran)
	#   ESP+12: EDI
	#   ESP+16: ESI
	#   ESP+20: EDX
	#   ESP+24: ECX
	#   ESP+28: EBX
	#   ESP+32: EAX
	#   ESP+36: original EIP (where we ultimately return to)
	#
	# The exception handler has already consumed texptn/exinf as arguments
	# but they remain on the stack.  Skip them:
	addl	$8, %esp	# discard texptn and exinf (8 bytes)
	call	ena_tex		# re-enable task exceptions for future delivery
	popfl			# restore EFLAGS (especially IF and arithmetic flags)
	popl	%edi		# restore all GPRs that were saved by stack_adjust
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%eax
	ret			# pop original EIP -- resume interrupted user code

# ===========================================================================#
# EXCEPTION AND INTERRUPT HANDLER STUBS
# ===========================================================================#
#
# Each stub follows this pattern:
#   SAVE_ALL         -- push all registers onto kernel stack
#   call intr_enter  -- k_nest++
#   call c_handler   -- invoke C handler
#   jmp intr_return  -- k_nest--, task switch check, RESTORE_ALL, iret
#

# --- Default handler (catch-all for unregistered interrupts) ----------------#
intr_default:
	SAVE_ALL
	call	intr_enter
	call	c_intr_default
	jmp	intr_return

# --- Exception: Divide by zero (#DE, vector 0) -----------------------------#
intr_divide:
	SAVE_ALL
	call	intr_enter
	call	c_intr_divide
	jmp	intr_return
# --- Exception: Debug / Single step (#DB, vector 1) ------------------------#
intr_singlestep:
	iret
# --- Exception: Non-Maskable Interrupt (vector 2) --------------------------#
intr_nmi:
	iret
# --- Exception: Breakpoint (#BP, vector 3) ---------------------------------#
intr_breakpoint:
	iret
# --- Exception: Overflow (#OF, vector 4) -----------------------------------#
intr_overflow:
	iret
# --- Exception: Bound Range Exceeded (#BR, vector 5) ----------------------#
intr_bounds:
	iret
# --- Exception: Invalid Opcode (#UD, vector 6) ----------------------------#
intr_opcode:
	iret
# --- Exception: Device Not Available (#NM, vector 7) ----------------------#
intr_copr_not_available:
	iret
# --- Exception: Double Fault (#DF, vector 8) -------------------------------#
# CPU pushes error code (always 0).  Must discard before iret, otherwise
# iret would pop the error code as EIP and immediately re-fault.
intr_doublefault:
	addl	$4, %esp		# discard error code
	iret
# --- Exception: Coprocessor Segment Overrun (vector 9) --------------------#
intr_copr_seg_overrun:
	iret				# no error code for this exception
# --- Exception: Invalid TSS (#TS, vector 10) ------------------------------#
# CPU pushes error code (selector index that caused the fault).
intr_tss:
	addl	$4, %esp		# discard error code
	iret
# --- Exception: Segment Not Present (#NP, vector 11) ----------------------#
# CPU pushes error code (selector index of missing segment).
intr_segment_not_present:
	addl	$4, %esp		# discard error code
	iret
# --- Exception: Stack-Segment Fault (#SS, vector 12) ----------------------#
# CPU pushes error code (selector index or 0).
intr_stack:
	addl	$4, %esp		# discard error code
	iret
# --- Exception: General Protection Fault (#GP, vector 13) -----------------#
# CPU pushes error code for #GP.  We need to extract it before SAVE_ALL
# because SAVE_ALL expects the stack to look like a normal interrupt frame
# (EIP at the top of the CPU-pushed portion).
#
# Stack on entry:  [ESP+0]=error_code  [ESP+4]=EIP  [ESP+8]=CS  ...
# We save error_code to a global variable, then discard it so SAVE_ALL
# sees the standard layout.
intr_general:
	pushl	%eax			# temporarily save EAX (need it as scratch)
	# Stack: [ESP+0]=saved_EAX  [ESP+4]=error_code  [ESP+8]=EIP  ...
	movl	4(%esp), %eax		# EAX = GP error code
	movl	%eax, gp_error_code	# store in global for c_intr_general to read
	popl	%eax			# restore original EAX
	# Stack: [ESP+0]=error_code  [ESP+4]=EIP  ...
	addl	$4, %esp		# discard error code from stack
	# Stack: [ESP+0]=EIP  [ESP+4]=CS  [ESP+8]=EFLAGS  ... (standard frame)
	SAVE_ALL
	call	intr_enter
	call	c_intr_general		# prints diagnostic and halts
	jmp	intr_return
.data
.globl gp_error_code
gp_error_code:	.long 0			# most recent #GP error code (for diagnostics)
.text
# --- Exception: Page Fault (#PF, vector 14) --------------------------------#
# CPU pushes error code for #PF.  The C handler reads CR2 directly for the
# fault address, so we only need to discard the error code before SAVE_ALL.
# Stack on entry:  [ESP+0]=error_code  [ESP+4]=EIP  ...
intr_page:
	addl	$4, %esp		# discard error code; standard frame remains
	SAVE_ALL
	call	intr_enter
	call	c_intr_page		# reads CR2 for fault address
	jmp	intr_return
# --- Exception: x87 FPU Error (#MF, vector 16) ----------------------------#
intr_copr_error:
	iret

# ===========================================================================#
# HARDWARE IRQ HANDLERS (PIC i8259, remapped to vectors 0x80-0x97)
# ===========================================================================#
# All external hardware IRQs are routed through the i8259 PIC and delivered
# only to CPU 0 (BSP).

# --- IRQ 0: PIT Timer -------------------------------------------------------#
intr_irq0:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq0
	jmp	intr_return

# --- IRQ 1: Keyboard --------------------------------------------------------#
intr_irq1:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq1
	jmp	intr_return

# --- IRQ 2-15 ---------------------------------------------------------------#
intr_irq2:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq2
	jmp	intr_return
intr_irq3:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq3
	jmp	intr_return
intr_irq4:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq4
	jmp	intr_return
intr_irq5:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq5
	jmp	intr_return
intr_irq6:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq6
	jmp	intr_return
intr_irq7:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq7
	jmp	intr_return
intr_irq8:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq8
	jmp	intr_return
intr_irq9:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq9
	jmp	intr_return
intr_irq10:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq10
	jmp	intr_return
intr_irq11:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq11
	jmp	intr_return
intr_irq12:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq12
	jmp	intr_return
intr_irq13:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq13
	jmp	intr_return
intr_irq14:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq14
	jmp	intr_return
intr_irq15:
	SAVE_ALL
	call	intr_enter
	call	c_intr_irq15
	jmp	intr_return

# ===========================================================================#
# LOCAL APIC TIMER HANDLERS (per-CPU timers)
# ===========================================================================#

# --- APIC Timer for CPU 0 --------------------------------------------------#
intr_smp_timer0:
	SAVE_ALL
	call	intr_enter
	call	c_intr_smp_timer0
	jmp	intr_return

# --- APIC Timer for CPU 1 --------------------------------------------------#
intr_smp_timer1:
	SAVE_ALL
	call	intr_enter
	call	c_intr_smp_timer1
	jmp	intr_return

# ===========================================================================#
# intr_syscall -- System call entry point (int $0x99)
# ===========================================================================#
#
# User tasks invoke system calls via `int $0x99`.  The system call number
# is passed in EAX, and arguments are in the user-mode stack (pushed by
# the library wrapper in lib/lib_tsk.c before `int $0x99`).
#
# After SAVE_ALL, ESP points to the pt_regs frame.  We pass a pointer to
# this frame as the argument to c_intr_syscall, which reads register values
# directly from the frame and writes the return value into regs->eax.
#
intr_syscall:
	SAVE_ALL			# save all user registers onto kernel stack
	call	intr_enter		# k_nest[cpu]++
	# Pass a pointer to the pt_regs frame (== current ESP) to the C handler.
	# c_intr_syscall reads the syscall number from regs->eax and arguments
	# from the user stack via regs->esp.  It writes the return value back
	# into regs->eax so RESTORE_ALL will pop it into EAX for the user.
	pushl	%esp			# arg: pt_regs* (ESP points to SAVE_ALL frame)
	call	c_intr_syscall		# void c_intr_syscall(struct pt_regs *regs)
	addl	$4, %esp		# clean up 1 argument (cdecl)
	jmp	intr_return		# intr_leave → RESTORE_ALL → iret
