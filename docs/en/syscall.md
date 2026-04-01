# System Call Processing Flow Guide

This document explains the entire process from when a user task calls a service call
such as `slp_tsk()`, through kernel dispatch, to receiving the return value.

For details on SAVE_ALL/RESTORE_ALL and intr_enter/intr_leave, see
[context-switch.md](context-switch.md).
This document focuses on the syscall-specific path: how arguments are passed,
the dispatch table, and how return values are written back.

---

## 1. Overview: Why `int $0x99` Is Necessary

User tasks run in Ring 3, while the kernel runs in Ring 0.
Code in Ring 3 is prohibited from directly accessing Ring 0 memory regions
or executing privileged instructions (`cli`, `outb`, etc.).

To invoke kernel services such as task management or semaphore operations,
the CPU's privilege level must transition from Ring 3 to Ring 0.
`int $0x99` (software interrupt, vector 153) accomplishes this transition.

When the `int` instruction executes, the CPU automatically:

1. Loads the Ring 0 stack (SS0:ESP0) from the TSS
2. Saves the Ring 3 SS, ESP, EFLAGS, CS, and EIP onto the Ring 0 stack
3. Jumps to the IDT vector 0x99 handler (`intr_syscall`)

The IDT registration is performed in `interrupt.c:setup_syscall()`:

```c
/* i386/interrupt.c:148 */
set_idt(VECT_SYSCALL, (unsigned long)intr_syscall,
        SEL_K32_C, 0, GT_INTR | 0x60);
```

`0x60` sets DPL=3. This allows `int $0x99` from Ring 3
(if DPL were left at 0, a #GP exception would occur).

---

## 2. Overall Flow Diagram

```
User Task (Ring 3)                    Kernel (Ring 0)
==================                    ================

cre_tsk(3, &ctsk)
  |
  v
lib/lib_tsk.c:cre_tsk()
  |  syscall(-TFN_CRE_TSK, 3, &ctsk)
  |  = syscall(0x05, 3, &ctsk)
  v
i386/klib.s:syscall          (.user_text, Ring 3)
  |  pushl %ebp
  |  movl  %esp, %ebp
  |  int   $0x99  --------> CPU: Ring 3 -> Ring 0 transition
  |                           |  Switch to kernel stack from TSS.esp0
  |                           |  Save SS,ESP,EFLAGS,CS,EIP onto kernel stack
  |                           v
  |                     i386/intr.s:intr_syscall
  |                           |  SAVE_ALL      (push registers onto kernel stack,
  |                           |                 reload DS/ES with kernel segments)
  |                           |  intr_enter    (k_nest++)
  |                           |  push %esp     (pass pt_regs* as argument)
  |                           v
  |                     i386/syscall.c:c_intr_syscall(regs)
  |                           |  Read user stack via regs->esp
  |                           |  itron_syscall(apic, 0x05, 3, &ctsk, ...)
  |                           v
  |                     kernel/syscall.c:itron_syscall()
  |                           |  syscall_entry[0x05].func(apic, 3, &ctsk, ...)
  |                           v
  |                     kernel/sys_tsk.c:sys_cre_tsk(apic, 3, &ctsk)
  |                           |  Task creation processing
  |                           |  return E_OK
  |                           v
  |                     c_intr_syscall:
  |                           |  regs->eax = E_OK   <- Write return value
  |                           |  return
  |                           v
  |                     intr_syscall:
  |                           |  addl $4, %esp  (argument cleanup)
  |                           |  jmp intr_return
  |                           |    intr_leave   (k_nest--, task switch decision)
  |                           |    RESTORE_ALL  (restore registers, EAX=E_OK)
  |                           |    iret  -------> Ring 0 -> Ring 3 return
  |                           |
  v                           |
klib.s:syscall (continued) <--+
  |  popl  %ebp              EAX = E_OK (restored by RESTORE_ALL)
  |  ret
  v
lib_tsk.c:cre_tsk()
  |  return EAX   <- E_OK
  v
Returns to User Task
```

---

## 3. User-Space Wrappers

### 3.1 ITRON Standard Wrappers (lib/lib_tsk.c, lib/lib_sem.c)

Each service call is a thin wrapper that calls `syscall()` with the corresponding
function code.

```c
/* lib/lib_tsk.c:7 */
ER cre_tsk(ID tskid, T_CTSK* pk_ctsk)
{
    return syscall(-TFN_CRE_TSK, tskid, pk_ctsk);
}

/* lib/lib_tsk.c:106 */
ER slp_tsk(void)
{
    return syscall(-TFN_SLP_TSK);
}

/* lib/lib_sem.c:5 */
ER cre_sem(ID semid, T_CSEM* pk_csem)
{
    return syscall(-TFN_CRE_SEM, semid, pk_csem);
}
```

**Calling convention:**
`syscall(func_code, arg1, arg2, ..., arg6)` -- a variadic C function with up to 7 arguments.

- 1st argument `func_code`: pass `-TFN_xxx` (sign inversion yields a positive value)
- 2nd argument onward: service call-specific arguments

### 3.2 Sign Inversion of Function Codes

`TFN_CRE_TSK` is defined as `-0x05` (include/itron.h:134).
The wrapper passes `-TFN_CRE_TSK` = `0x05`.

```
TFN_CRE_TSK = -0x05  (definition in itron.h)
-TFN_CRE_TSK = 0x05  (value passed to syscall() = dispatch table index)
```

Why define them as negative values: the ITRON v4.0 specification mandates that
function codes are defined as negative values. The library inverts the sign to
convert them to positive indices, which are used directly as array subscripts
into the kernel's dispatch table `syscall_entry[]`.

### 3.3 Extended Syscall Wrappers (lib/lib_exd.c)

Non-standard services unique to this project (VGA output, keyboard input, etc.)
also use the same `syscall()` interface:

```c
/* lib/lib_exd.c:6 */
void print_at(int row, int col, char *s, unsigned char attr)
{
    syscall(-TFN_EXD_VGA_WRITE, row, col, s, attr);
}

/* lib/lib_exd.c:31 -- key_read is deprecated (migrated to DTQ) */

/* lib/lib_exd.c:43 */
VP tsk_stack_alloc(int size)
{
    return (VP)syscall(-TFN_EXD_STACK_ALLOC, size);
}
```

---

## 4. Assembly Entry Point (i386/klib.s)

### 4.1 The syscall Function

```asm
/* i386/klib.s:525-532 */
.section .user_text
syscall:
    pushl   %ebp              # Save frame pointer
    movl    %esp, %ebp        # Set up stack frame
    int     $0x99             # Vector 153 -> intr_syscall
    popl    %ebp              # Restore frame pointer
    ret                       # Return to caller (EAX = return value)
```

**Why it is placed in the `.user_text` section:**
This function is called from Ring 3 user tasks. Unless it resides in a page
with the PTE_USER bit set in the page table, accessing it from Ring 3 would
cause a #PF (Page Fault). The kernel's `.text` section is Supervisor-only;
only the `.user_text` section is accessible from User mode.

**Why it creates a stack frame:**
When `int $0x99` executes, the ESP that the CPU saves is the value after
the frame pointer setup. The kernel-side `intr_syscall` uses this ESP to
read arguments from the user stack. Thanks to `pushl %ebp` / `movl %esp, %ebp`,
the arguments are placed at the standard position starting at offset `8(%ebp)`.

### 4.2 syscall2 (lcall -- unused)

```asm
/* i386/klib.s:499-501 */
syscall2:
    lcall   $0x70, $0         # Far call via call gate
    ret
```

An alternative mechanism using a call gate configured at GDT selector 0x70.
Currently unused; the `int $0x99` approach is used instead.

---

## 5. Interrupt Handler: intr_syscall (i386/intr.s)

### 5.1 Overall Structure

```asm
/* i386/intr.s */
intr_syscall:
    SAVE_ALL                        # (1) Push all registers -> build pt_regs
    call    intr_enter              # (2) k_nest++ (interrupt nesting counter)
    pushl   %esp                    # (3) Pass pt_regs* as argument
    call    c_intr_syscall          # (4) Call C dispatcher
    addl    $4, %esp                # (5) Argument cleanup
    jmp     intr_return             # (6) intr_leave + RESTORE_ALL + iret
```

Immediately after `SAVE_ALL`, ESP points to the beginning of the pt_regs frame.
By passing this pointer directly to `c_intr_syscall`, the C code can read and
write register values as fields of the pt_regs structure.

### 5.2 Stack Layout After SAVE_ALL (pt_regs)

Once `SAVE_ALL` completes, the following pt_regs frame is constructed on the
kernel stack:

```
ESP+0x00  ES         (pushed by SAVE_ALL)
ESP+0x04  DS         (pushed by SAVE_ALL)
ESP+0x08  EDI        (pushed by SAVE_ALL)
ESP+0x0C  ESI        (pushed by SAVE_ALL)
ESP+0x10  EBP        (pushed by SAVE_ALL)
ESP+0x14  EBX        (pushed by SAVE_ALL)
ESP+0x18  EDX        (pushed by SAVE_ALL)
ESP+0x1C  ECX        (pushed by SAVE_ALL)
ESP+0x20  EAX        (pushed by SAVE_ALL)     <- Return value write target
ESP+0x24  EIP        (pushed by CPU: instruction after int $0x99)
ESP+0x28  CS         (pushed by CPU: 0x5B = Ring 3)
ESP+0x2C  EFLAGS     (pushed by CPU)
ESP+0x30  ESP        (pushed by CPU: Ring 3 stack pointer)  <- User stack
ESP+0x34  SS         (pushed by CPU: 0x6B = Ring 3)
```

`c_intr_syscall` obtains the user stack pointer from `regs->esp` (offset 0x30).

### 5.3 Reading Arguments from the User Stack

Contents of the user stack pointed to by `regs->esp` (at the time `int $0x99` was issued):

```
User ESP ->
  ustack[0]  saved EBP    (from syscall's pushl %ebp)
  ustack[1]  return addr  (return address from call syscall)
  ustack[2]  func_code    (1st argument: function code, e.g., 0x05)
  ustack[3]  arg1         (2nd argument: e.g., tskid)
  ustack[4]  arg2         (3rd argument: e.g., pk_ctsk)
  ustack[5]  arg3         (4th argument)
  ustack[6]  arg4         (5th argument)
  ustack[7]  arg5         (6th argument)
  ustack[8]  arg6         (7th argument)
```

`c_intr_syscall` uses `regs->esp` as the user stack pointer and reads
arguments using array indexing:

```c
ustack = (unsigned long *)regs->esp;
ret = itron_syscall(apic,
        ustack[2],  /* sysid   (func_code) */
        ustack[3],  /* arg1 */
        ustack[4],  /* arg2 */
        ustack[5],  /* arg3 */
        ustack[6],  /* arg4 */
        ustack[7],  /* arg5 */
        ustack[8]); /* arg6 */
```

---

## 6. C Dispatcher Layer

### 6.1 c_intr_syscall (i386/syscall.c)

```c
/* i386/syscall.c */
W
c_intr_syscall(struct pt_regs *regs)
{
    unsigned long *ustack;
    unsigned long apic;
    W   ret;

    /* Determine CPU from APIC ID */
    apic = *(volatile unsigned long *)APIC_ID;
    apic >>= 24;
    if (apic) apic = 1;

    /* Read syscall arguments from user stack */
    ustack = (unsigned long *)regs->esp;

    smp_lock(&kernel_lk);
    ret = itron_syscall(apic,
            ustack[2],     /* sysid   (user_esp + 8) */
            ustack[3],     /* arg1    (user_esp + 12) */
            ustack[4],     /* arg2    (user_esp + 16) */
            ustack[5],     /* arg3    (user_esp + 20) */
            ustack[6],     /* arg4    (user_esp + 24) */
            ustack[7],     /* arg5    (user_esp + 28) */
            ustack[8]);    /* arg6    (user_esp + 32) */
    smp_unlock(&kernel_lk);

    /* Write return value into the pt_regs EAX slot */
    regs->eax = ret;
    return ret;
}
```

**Argument**: the pt_regs pointer passed by `intr_syscall` via `push %esp`.
pt_regs is the register frame that SAVE_ALL built on the kernel stack.

**Mutual exclusion via `kernel_lk`**:
The kernel data structures manipulated by syscalls (task state, scheduler queues,
semaphores, DTQs, timeout queues, etc.) are also accessed from timer interrupts
(`c_intr_irq0`) and keyboard interrupts (`c_intr_irq1`). In an SMP environment,
a timer interrupt on CPU 1 may fire while CPU 0 is executing a syscall, so a
single spinlock `kernel_lk` (Big Kernel Lock) provides mutual exclusion across
all paths.
For details, see [system-overview.md Section 10](system-overview.md#10-mutual-exclusion-big-kernel-lock).

**APIC ID reading**: reads bits [31:24] of the APIC ID register (0xFEE00020)
to determine the CPU number. As a safety measure, values greater than 1 are
clamped to 0 or 1.

**Return value writing** -- `regs->eax = ret`:

Writes directly to the `eax` field (offset 0x20) of the pt_regs structure.
Since `RESTORE_ALL` executes `popl %eax` from this slot, after `iret` the
user task receives the return value in `%eax`.

### 6.2 itron_syscall (kernel/syscall.c)

```c
/* kernel/syscall.c:29-44 */
W
itron_syscall(unsigned long apic, unsigned long sysid, unsigned long arg1,
    unsigned long arg2, unsigned long arg3, unsigned long arg4,
    unsigned long arg5, unsigned long arg6)
{
    ER  ret;
    if (sysid == -TFN_EXD_TCPIP) {
        /* TCP/IP sub-dispatch (currently stub only) */
        sysid = arg1 - (-TFN_TCP_CRE_REP);
        ret = (*syscall_tcpip_entry[sysid].func)
                    (apic, arg2, arg3, arg4, arg5, arg6);
    } else {
        /* Normal dispatch: look up table using sysid as index */
        ret = (*syscall_entry[sysid].func)
                (apic, arg1, arg2, arg3, arg4, arg5, arg6);
    }
    return ret;
}
```

`sysid` is used directly as the array index into `syscall_entry[]`.
Example: `sysid = 0x05` -> `syscall_entry[5].func` = `sys_cre_tsk`.

Each handler receives `apic` (CPU number) as the first argument,
followed by the arguments passed by the user task as arg1, arg2, etc.

---

## 7. Dispatch Table (kernel/syscallP.h)

### 7.1 Table Structure

```c
/* kernel/syscallP.h:23-25 */
struct syscall_entry {
    ER  (*func)();
};
```

A simple structure containing just a single function pointer.
`syscall_entry[]` is an array of 0x00 to 0xe9 (234 entries), where the
positive value of the function code directly serves as the index.

### 7.2 Major Entry List

| Index | Function | Service Call |
|-------|----------|-------------|
| 0x05 | sys_cre_tsk | cre_tsk -- create task |
| 0x06 | sys_del_tsk | del_tsk -- delete task |
| 0x07 | sys_act_tsk | act_tsk -- activate task |
| 0x0a | sys_ext_tsk | ext_tsk -- exit invoking task |
| 0x0b | sys_exd_tsk | exd_tsk -- exit and delete invoking task |
| 0x0c | sys_ter_tsk | ter_tsk -- terminate task |
| 0x0d | sys_chg_pri | chg_pri -- change priority |
| 0x11 | sys_slp_tsk | slp_tsk -- sleep (wait for wakeup) |
| 0x12 | sys_tslp_tsk | tslp_tsk -- sleep with timeout |
| 0x13 | sys_wup_tsk | wup_tsk -- wake up task |
| 0x19 | sys_dly_tsk | dly_tsk -- delay task |
| 0x21 | sys_cre_sem | cre_sem -- create semaphore |
| 0x23 | sys_sig_sem | sig_sem -- signal semaphore |
| 0x25 | sys_wai_sem | wai_sem -- wait on semaphore |
| 0x26 | sys_pol_sem | pol_sem -- poll semaphore |
| 0x29 | sys_cre_flg | cre_flg -- create event flag |
| 0x2b | sys_set_flg | set_flg -- set event flag |
| 0x2d | sys_wai_flg | wai_flg -- wait on event flag |
| 0x55 | sys_rot_rdq | rot_rdq -- rotate ready queue |
| 0x56 | sys_get_tid | get_tid -- get task ID |
| 0x72 | iwup_tsk | iwup_tsk -- wakeup from interrupt |
| 0xc1 | sys_acre_tsk | acre_tsk -- create task with auto ID |
| 0xe1 | sys_printf | printf -- kernel printf |
| 0xe3 | sys_vga_write_at | print_at -- VGA string output |
| 0xe4 | sys_vga_write_dec_at | print_dec_at -- VGA numeric output |
| 0xe5 | sys_vga_clear | clear_screen -- VGA clear |
| 0xe6 | sys_vga_fill_at | fill_at -- VGA rectangle fill |
| 0xe7 | sys_key_getc_sc | key_read -- deprecated (returns E_NOSPT, migrated to DTQ) |
| 0xe8 | sys_key_set_task | set_key_task -- register keyboard DTQ ID |
| 0xe9 | sys_stack_alloc_sc | tsk_stack_alloc -- dynamic stack allocation |

Unimplemented indices (0x00-0x04, 0x1a, etc.) have `sys_dummy` registered,
which simply returns `E_OK` and does nothing.

---

## 8. Function Code Definitions (include/itron.h)

### 8.1 System

`TFN_xxx` macros are defined as negative values. Library wrappers invert the sign
with `-TFN_xxx` to convert them to positive indices.

```c
/* include/itron.h:134-308 */
#define TFN_CRE_TSK    -0x05
#define TFN_SLP_TSK    -0x11
#define TFN_WUP_TSK    -0x13
#define TFN_CRE_SEM    -0x21
#define TFN_SIG_SEM    -0x23
/* ... */
#define TFN_EXD_VGA_WRITE   -0xe3
#define TFN_EXD_STACK_ALLOC -0xe9
```

### 8.2 List by Category

**Task management (0x05-0x20):**

| Function Code | TFN Macro | Service Call |
|--------------|-----------|-------------|
| 0x05 | TFN_CRE_TSK | cre_tsk |
| 0x06 | TFN_DEL_TSK | del_tsk |
| 0x07 | TFN_ACT_TSK | act_tsk |
| 0x08 | TFN_CAN_ACT | can_act |
| 0x09 | TFN_STA_TSK | sta_tsk |
| 0x0a | TFN_EXT_TSK | ext_tsk |
| 0x0b | TFN_EXD_TSK | exd_tsk |
| 0x0c | TFN_TER_TSK | ter_tsk |
| 0x0d | TFN_CHG_PRI | chg_pri |
| 0x0e | TFN_GET_PRI | get_pri |
| 0x0f | TFN_REF_TSK | ref_tsk |
| 0x10 | TFN_REF_TST | ref_tst |
| 0x11 | TFN_SLP_TSK | slp_tsk |
| 0x12 | TFN_TSLP_TSK | tslp_tsk |
| 0x13 | TFN_WUP_TSK | wup_tsk |
| 0x14 | TFN_CAN_WUP | can_wup |
| 0x15 | TFN_REL_WAI | rel_wai |
| 0x16 | TFN_SUS_TSK | sus_tsk |
| 0x17 | TFN_RSM_TSK | rsm_tsk |
| 0x18 | TFN_FRSM_TSK | frsm_tsk |
| 0x19 | TFN_DLY_TSK | dly_tsk |

**Task exception handling (0x1b-0x20):**

| Function Code | TFN Macro | Service Call |
|--------------|-----------|-------------|
| 0x1b | TFN_DEF_TEX | def_tex |
| 0x1c | TFN_RAS_TEX | ras_tex |
| 0x1d | TFN_DIS_TEX | dis_tex |
| 0x1e | TFN_ENA_TEX | ena_tex |
| 0x1f | TFN_SNS_TEX | sns_tex |
| 0x20 | TFN_REF_TEX | ref_tex |

**Synchronization and communication (0x21-0x44):**

| Function Code | TFN Macro | Service Call |
|--------------|-----------|-------------|
| 0x21 | TFN_CRE_SEM | cre_sem |
| 0x23 | TFN_SIG_SEM | sig_sem |
| 0x25 | TFN_WAI_SEM | wai_sem |
| 0x26 | TFN_POL_SEM | pol_sem |
| 0x29 | TFN_CRE_FLG | cre_flg |
| 0x2b | TFN_SET_FLG | set_flg |
| 0x2d | TFN_WAI_FLG | wai_flg |
| 0x31 | TFN_CRE_DTQ | cre_dtq |
| 0x35 | TFN_SND_DTQ | snd_dtq |
| 0x39 | TFN_RCV_DTQ | rcv_dtq |
| 0x3d | TFN_CRE_MBX | cre_mbx |
| 0x3f | TFN_SND_MBX | snd_mbx |
| 0x41 | TFN_RCV_MBX | rcv_mbx |

**Extended syscalls (0xe1-0xe9):**

| Function Code | TFN Macro | Service Call | Purpose |
|--------------|-----------|-------------|---------|
| 0xe1 | TFN_EXD_PRINT | printf | Kernel printk |
| 0xe3 | TFN_EXD_VGA_WRITE | print_at | VGA string drawing |
| 0xe4 | TFN_EXD_VGA_DEC | print_dec_at | VGA numeric drawing |
| 0xe5 | TFN_EXD_VGA_CLEAR | clear_screen | Screen clear |
| 0xe6 | TFN_EXD_VGA_FILL | fill_at | Rectangle fill |
| 0xe7 | TFN_EXD_KEY_GETC | key_read | Deprecated (migrated to DTQ) |
| 0xe8 | TFN_EXD_KEY_SETTASK | set_key_task | Register keyboard DTQ ID |
| 0xe9 | TFN_EXD_STACK_ALLOC | tsk_stack_alloc | Dynamic stack allocation |

---

## 9. Kernel Handler Implementation Patterns

The first argument of every handler is `W apic` (CPU number, 0 or 1).
This is because `itron_syscall` calls `syscall_entry[sysid].func(apic, arg1, ...)`.

### 9.1 sys_slp_tsk -- Put the Invoking Task into Wait State

```c
/* kernel/sys_tsk.c:293-309 */
ER sys_slp_tsk(W apic)
{
    ID  tskid = c_tskid[apic];    /* Currently running task ID */

    /* caller holds kernel_lk (acquired in c_intr_syscall) */
    if (tsk[tskid].wupcnt >= 1) {
        tsk[tskid].wupcnt--;      /* Consume pending wakeup and return immediately */
        return E_OK;
    }
    sched_rem(&tsk[tskid].plink); /* Remove from ready queue */
    tsk[tskid].tskstat = TTS_WAI; /* Change state to WAITING */
    sched_next_tsk(apic);         /* Notify scheduling event */
    return E_OK;
}
```

`sched_next_tsk(apic)` sets `next_tsk_flag[]` to 1 for both CPUs.
The actual task switch is performed in `sched_next_tsk_check()` within
`intr_leave` (see Section 11).

### 9.2 sys_wup_tsk -- Wake Up a Specified Task

```c
/* kernel/sys_tsk.c:333-369 */
ER sys_wup_tsk(W apic, ID tskid)
{
    int flag = 1;
    if (tskid == TSK_SELF) {
        tskid = c_tskid[apic];
        flag = 0;
    }

    /* caller holds kernel_lk (acquired in c_intr_syscall) */
    if (tsk[tskid].tskstat != TTS_WAI) {
        /* If not in WAI state, increment wupcnt (queuing) */
        /* ... error checks omitted ... */
        tsk[tskid].wupcnt++;
        return E_OK;
    }
    tsk[tskid].tskstat = TTS_RDY;               /* Change to READY */
    if (flag) {
        tsk[c_tskid[apic]].tskstat = TTS_RDY;   /* Also set self to RDY */
        sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink); /* Insert into ready queue */
    }
    sched_timeout_rem_if_exist(&tsk[tskid].tlink); /* Cancel timeout */
    sched_next_tsk(apic);                        /* Notify scheduling */
    return E_OK;
}
```

### 9.3 sys_cre_tsk -- Create a Task

```c
/* kernel/sys_tsk.c:32-71 */
ER sys_cre_tsk(W apic, ID tskid, T_CTSK* pk_ctsk)
{
    if (tskid < 1 || tskid > MAX_TSKID)
        return E_ID;
    if (tsk[tskid].tskstat != TTS_NON)
        return E_OBJ;

    tsk[tskid].tskid = tskid;
    tsk[tskid].proc = proc_create(tskid, pk_ctsk);   /* Allocate proc_t */
    if (pk_ctsk->tskatr & TA_ACT) {
        tsk[tskid].tskstat = TTS_RDY;    /* If TA_ACT, immediately READY */
    } else
        tsk[tskid].tskstat = TTS_DMT;    /* Otherwise DORMANT */
    tsk[tskid].tskbpri = pk_ctsk->itskpri;
    tsk[tskid].tskpri = tsk[tskid].tskbpri;
    /* ... remaining field initialization ... */
    return E_OK;
}
```

`proc_create` allocates a `proc_t` and sets up the initial ESP/EIP.
If created with the `TA_ACT` attribute, the task becomes immediately
schedulable without needing a separate `act_tsk` call.

---

## 10. Return Value Passing

### 10.1 Writing: regs->eax

`c_intr_syscall` writes the kernel handler's return value directly into the
EAX slot of the pt_regs frame:

```c
/* i386/syscall.c */
regs->eax = ret;
```

**The EAX slot in pt_regs**:

```
pt_regs frame (on kernel stack):
  Offset 0x00: ES
  Offset 0x04: DS
  Offset 0x08: EDI
  Offset 0x0C: ESI
  Offset 0x10: EBP
  Offset 0x14: EBX
  Offset 0x18: EDX
  Offset 0x1C: ECX
  Offset 0x20: EAX     <- Return value written here (PT_REGS_EAX_OFFSET)
  Offset 0x24: EIP     (pushed by CPU)
  Offset 0x28: CS
  Offset 0x2C: EFLAGS
  Offset 0x30: ESP     (Ring 3)
  Offset 0x34: SS      (Ring 3)
```

`regs` is the pointer passed by `intr_syscall` via `push %esp`, and its value
equals the ESP immediately after `SAVE_ALL` = the starting address of the
pt_regs frame.

### 10.2 Restoration: RESTORE_ALL -> iret

`RESTORE_ALL` pops the 9 registers from the pt_regs frame in reverse order.
The return value written to the EAX slot (offset 0x20) is restored to the
`%eax` register via `popl %eax`.

The subsequent `iret` returns from Ring 0 to Ring 3, and the user task's
`syscall` function executes `popl %ebp; ret`.
Since the C calling convention uses `%eax` as the return value register,
the wrapper function returns this value directly to its caller.

---

## 11. Syscalls and Task Switching

### 11.1 Scheduling Event Notification

Service calls such as `slp_tsk` and `wup_tsk` call `sched_next_tsk(apic)`
after modifying task state:

```c
/* kernel/sched.c:77-82 */
ID sched_next_tsk(W apic)
{
    next_tsk_flag[0] = 1;   /* Request scheduling on CPU 0 */
    next_tsk_flag[1] = 1;   /* Request scheduling on CPU 1 as well */
    return E_OK;
}
```

Both CPU flags are set because the woken task may have affinity for a
different CPU.

### 11.2 Task Switch Decision in intr_leave

`intr_leave` performs ESP save, scheduler invocation, and ESP restore when the
interrupt nesting counter returns to 0 (i.e., returning from the outermost
interrupt):

```
intr_leave (when k_nest == 0):
  1. current_proc[cpu]->kern_esp = ESP   (save current task's ESP)
  2. sched_next_tsk_check(cpu)           (invoke scheduler)
  3. ESP = current_proc[cpu]->kern_esp   (load new task's ESP)
  4. tss_update_esp0(cpu, kern_stack_top) (update TSS.esp0)
```

`sched_next_tsk_check` invokes the scheduler and may switch
`current_proc[cpu]` to a new task:

```c
/* i386/interrupt.c */
int sched_next_tsk_check(int apic)
{
    proc_t* old_proc;
    extern INT next_tsk_flag[];

    if (next_tsk_flag[apic] != 0) {
        old_proc = current_proc[apic];
        sched_do_next_tsk(apic);       /* Select highest-priority task */
        next_tsk_flag[apic] = 0;
        if (old_proc != current_proc[apic]) {
            return 1;                   /* Task switch occurred */
        }
    }
    return 0;                           /* No switch */
}
```

When a task switch occurs, step 3 of `intr_leave` switches ESP to the new
task's kernel stack. Since the new task's kernel stack contains its saved
pt_regs frame, `RESTORE_ALL` pops the new task's registers, and `iret`
jumps to the new task's code.

### 11.3 slp_tsk/wup_tsk Flow Example

```
Task 1: slp_tsk()
  -> syscall(0x11) -> int $0x99
  -> SAVE_ALL (push Task 1's registers onto kernel stack)
  -> intr_enter (k_nest++)
  -> c_intr_syscall(regs) -> sys_slp_tsk
    -> Set Task 1 to TTS_WAI, set next_tsk_flag via sched_next_tsk
  -> regs->eax = E_OK (write return value into pt_regs EAX slot)
  -> intr_leave
    -> k_nest == 0 and next_tsk_flag[0] == 1
    -> Save ESP into Task 1's kern_esp
    -> sched_next_tsk_check(0) -> sched_do_next_tsk(0)
      -> Task 1 is WAI so not selected; select Task 3 (RDY)
    -> current_proc[0] = changed to Task 3's proc_t
    -> ESP = Task 3's kern_esp (kernel stack switch)
    -> Update TSS.esp0 to Task 3's kern_stack_top
  -> RESTORE_ALL (pop registers from Task 3's kernel stack)
  -> iret -> Task 3 resumes execution

--- Later, Task 3 calls wup_tsk(1) ---

Task 3: wup_tsk(1)
  -> syscall(0x13, 1) -> int $0x99
  -> SAVE_ALL (push Task 3's registers onto kernel stack)
  -> intr_enter (k_nest++)
  -> c_intr_syscall(regs) -> sys_wup_tsk
    -> Set Task 1 to TTS_RDY, insert into ready queue via sched_ins
    -> Set next_tsk_flag via sched_next_tsk
  -> regs->eax = E_OK (write return value)
  -> intr_leave
    -> sched_next_tsk_check -> sched_do_next_tsk
      -> Select Task 1 or Task 3 based on priority
    -> (If Task 1 is selected) ESP = switch to Task 1's kern_esp
    -> Update TSS.esp0 to Task 1's kern_stack_top
  -> RESTORE_ALL (pop registers from Task 1's kernel stack)
  -> iret -> Task 1 resumes with slp_tsk return value E_OK in %eax
```

For details on SAVE_ALL/RESTORE_ALL and intr_enter/intr_leave, see
[context-switch.md](context-switch.md).

---

## Source File Index

| File | Contents |
|------|----------|
| `i386/klib.s` | syscall assembly function (.user_text) |
| `i386/intr.s` | intr_syscall, SAVE_ALL/RESTORE_ALL, intr_enter/intr_leave |
| `i386/syscall.c` | c_intr_syscall (argument reading and return value writing via pt_regs) |
| `i386/proc.h` | proc_t structure, pt_regs structure |
| `kernel/syscall.c` | itron_syscall dispatcher |
| `kernel/syscallP.h` | syscall_entry[] table |
| `include/itron.h:134-308` | TFN_* function code definitions |
| `lib/lib_tsk.c` | Task management wrappers |
| `lib/lib_sem.c` | Semaphore/flag/DTQ/MBX wrappers |
| `lib/lib_exd.c` | Extended syscall wrappers |
| `kernel/sys_tsk.c` | Task management handlers |
| `kernel/sys_exd.c` | Extended syscall handlers |
| `i386/interrupt.h:29` | VECT_SYSCALL (0x99) definition |
| `i386/interrupt.c` | IDT syscall vector registration, sched_next_tsk_check |
| `kernel/sched.c` | sched_next_tsk (flag setting) |
