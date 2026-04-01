/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
/* user processes ===========================================================*/
/*
 * Educational multitask demo — SMP (2-CPU) version:
 *
 * CPU 0 (Processor 1):
 *   Task1 (first_task): creates semaphore + Task 3, counts up
 *   Task3 (usr_main):   counts up, uses semaphore (cross-CPU with Task 2)
 *   Task5 (idle_task):  idle (pause loop, lowest priority)
 *
 * CPU 1 (Processor 2):
 *   Task2 (second_task): creates Task 4, counts up, uses semaphore
 *   Task4 (kbd_task):    keyboard input echo (highest priority)
 *   Task6 (idle_task):   idle (pause loop, lowest priority)
 *
 * Task 2 and Task 3 compete for binary semaphore 1 to access shared_count.
 * pol_sem (non-blocking) is used to show contention between CPUs.
 *
 * Idle tasks (5, 6) are always RDY at TMAX_TPRI.  User tasks run at
 * TMAX_TPRI - 1 so the idle task only executes when every other task
 * on that CPU is blocked (WAI/SLP/SUS).  This prevents "ghost running"
 * of a WAI task when sched_do_next_tsk finds no RDY candidate.
 *
 * Row 0: Header
 * Row 1: Separator
 * Row 3: Timer tick (updated by ISR in timer.c)
 * Row 5: [CPU0] Task1 count + MBF received text
 * Row 7: [CPU0] Task3 count + [LOCK]---+ / [BUSY]---+
 * Row 8:                                 \  (arrow when Task3 holds sem)
 * Row 9:                                  +---> Shared (sem 1) count
 * Row 10:                                /  (arrow when Task2 holds sem)
 * Row 11: [CPU1] Task2 count + [LOCK]---+ / [BUSY]---+
 * Row 13: [CPU1] Task4 keyboard echo
 *
 * Task 4 buffers keypresses and sends line strings to MBF 1 on Enter
 * (psnd_mbf).  Task 1 receives from MBF 1 via trcv_mbf (timeout = pacing).
 *
 * GDB inspection:
 *   p shared_count, p task_count[1..3], p key_dtq_id
 *   p tsk[5].tskstat, p tsk[6].tskstat  (idle tasks)
 */
#include "../include/itron.h"
#include "../include/config.h"
#include "../include/exd.h"
#include "../i386/proc.h"

/* -- shared state (inspect with GDB) -------------------------------------- */
__attribute__((section(".user_data")))
UW shared_count = 0;			/* semaphore-protected counter */
__attribute__((section(".user_data")))
UW task_count[MAX_TSKID];		/* per-task run count */
__attribute__((section(".user_data")))
char spin_chars[] = { '|', '/', '-', '\\' };

/* -- VGA attribute constants ---------------------------------------------- */
#define ATTR_WHITE	0x0F
#define ATTR_GREEN	0x0A
#define ATTR_CYAN	0x0B
#define ATTR_YELLOW	0x0E
#define ATTR_GREY	0x07
#define ATTR_DARK	0x08
#define ATTR_MAGENTA	0x0D
#define ATTR_RED	0x0C

/* -- screen layout rows --------------------------------------------------- */
#define ROW_HEADER	0
#define ROW_SEP		1
#define ROW_TIMER	3
#define ROW_TASK1	5
#define ROW_TASK3	7
#define ROW_ARROW_UP	8	/* diagonal '\' from Task3 to Shared */
#define ROW_SHARED	9
#define ROW_ARROW_DN	10	/* diagonal '/' from Task2 to Shared */
#define ROW_TASK2	11
#define ROW_KBD		13
#define ROW_COPYRIGHT	24

/* column where the '+' at end of [LOCK]---+ sits */
#define COL_PLUS_TASK	39
/* column for the diagonal arrow char '\' or '/' */
#define COL_DIAG	40
/* column where '+--->' starts on the Shared row */
#define COL_PLUS_SHARED	41

/* -- busy-wait delay ------------------------------------------------------ */
#define DELAY_COUNT	20000000UL
/* wrap counters before they exceed the 8-digit VGA display field */
#define COUNTER_WRAP	10000000UL

__attribute__((section(".user_text")))
static void
delay(void)
{
	volatile UW k;
	for (k = 0; k < DELAY_COUNT; k++)
		;
}

/* -- semaphore timing ----------------------------------------------------- */
/* LOCK 中に何回 shared_count を増やすか (見える程度に長く保持)               */
#define SEM_HOLD	20
/* リリース後、次に取りに行くまで何回スキップするか                           */
#define SEM_REST	40

/* -- draw_header: initial screen layout ----------------------------------- */
__attribute__((section(".user_text")))
static void
draw_header(void)
{
	clear_screen();
	print_at(ROW_HEADER, 2,
		"TinyITRON/386 SMP (2 CPU)", ATTR_WHITE);
	print_at(ROW_HEADER, 56,
		"[Ctrl+C to quit]", ATTR_GREY);
	print_at(ROW_SEP, 2,
		"============================================================================",
		ATTR_GREY);

	print_at(ROW_TIMER, 4, "Timer", ATTR_CYAN);
	print_at(ROW_TIMER, 14, "tick =", ATTR_GREY);

	print_at(ROW_TASK1, 2, "[CPU0]", ATTR_CYAN);
	print_at(ROW_TASK1, 9, "Task1", ATTR_GREEN);
	print_at(ROW_TASK1, 18, "#", ATTR_GREY);
	print_at(ROW_TASK1, 31, "mbf:", ATTR_DARK);
	print_at(ROW_TASK1, 36, "-", ATTR_DARK);

	/* Task3 row: [CPU0] Task3 # nnnn  [----]     */
	print_at(ROW_TASK3, 2, "[CPU0]", ATTR_CYAN);
	print_at(ROW_TASK3, 9, "Task3", ATTR_GREEN);
	print_at(ROW_TASK3, 18, "#", ATTR_GREY);

	/* Shared row: static arrow stub + label */
	print_at(ROW_SHARED, COL_PLUS_SHARED,
		"+---> Shared (sem 1)", ATTR_DARK);
	print_at(ROW_SHARED, 65, "#", ATTR_GREY);

	/* Task2 row: [CPU1] Task2 # nnnn  [----]     */
	print_at(ROW_TASK2, 2, "[CPU1]", ATTR_MAGENTA);
	print_at(ROW_TASK2, 9, "Task2", ATTR_MAGENTA);
	print_at(ROW_TASK2, 18, "#", ATTR_GREY);

	print_at(ROW_KBD, 2, "[CPU1]", ATTR_MAGENTA);
	print_at(ROW_KBD, 9, "Task4", ATTR_YELLOW);
	print_at(ROW_KBD, 18, ">", ATTR_GREY);

	print_at(ROW_COPYRIGHT, 2,
		"Copyright (c) 2000-2026 t-ishii66. All rights reserved.", ATTR_DARK);

	/* place hardware cursor at kbd_task input position (col 20 = after "> ") */
	set_cursor(ROW_KBD, 20);

	/* -- SMP architecture diagram (rows 16-22) -- */
	/* CPU 0 box (cyan, matching [CPU0] labels) */
	print_at(16, 22, ".---------.", ATTR_CYAN);
	print_at(17, 22, "|  CPU 0  |", ATTR_CYAN);
	print_at(18, 22, "| Task1,3 |", ATTR_CYAN);
	print_at(19, 22, "|  Idle5  |", ATTR_CYAN);
	print_at(20, 22, "'---------'", ATTR_CYAN);

	/* CPU 1 box (magenta, matching [CPU1] labels) */
	print_at(16, 46, ".---------.", ATTR_MAGENTA);
	print_at(17, 46, "|  CPU 1  |", ATTR_MAGENTA);
	print_at(18, 46, "| Task2,4 |", ATTR_MAGENTA);
	print_at(19, 46, "|  Idle6  |", ATTR_MAGENTA);
	print_at(20, 46, "'---------'", ATTR_MAGENTA);

	/* BKL connection between CPUs */
	print_at(18, 33, "---[ BKL ]---", ATTR_YELLOW);

	/* Shared memory bus below */
	print_at(21, 28, "\\", ATTR_DARK);
	print_at(21, 50, "/", ATTR_DARK);
	print_at(22, 29, "'---", ATTR_DARK);
	print_at(22, 33, " Shared Memory ", ATTR_WHITE);
	print_at(22, 48, "---'", ATTR_DARK);
}

/* == first_task (Task1, ID=1, CPU 0) ====================================== */
/* Created by proc_init(). Creates semaphore, MBF, and Task 3.              */
/* Alternates with Task 3 via wup_tsk/slp_tsk.                              */
/* Receives keyboard lines from MBF 1 via trcv_mbf (timeout = pacing).      */
__attribute__((section(".user_text")))
void
first_task(void)
{
	T_CSEM	csem;
	T_CMBF	cmbf;
	T_CTSK	ctsk;
	void	usr_main(VP_INT);

	/* -- create binary semaphore (sem 1) ------------------------------ */
	csem.sematr  = TA_TFIFO;
	csem.isemcnt = 1;
	csem.maxsem  = 1;
	cre_sem(1, &csem);

	/* -- create message buffer (mbf 1) for keyboard lines ------------- */
	/* kbd_task accumulates chars and sends a line string on Enter.
	 * maxmsz=64 allows lines up to 64 bytes; mbfsz=256 holds several. */
	cmbf.mbfatr = TA_TFIFO;
	cmbf.maxmsz = 64;
	cmbf.mbfsz  = 256;
	cmbf.mbf    = (VP)0;		/* kernel allocates via mem_alloc */
	cre_mbf(1, &cmbf);

	/* -- create Task 3 (usr_main, CPU 0) ------------------------------ */
	ctsk.task    = (FP)usr_main;
	ctsk.stk     = tsk_stack_alloc(1024);
	ctsk.stksz   = 1024;
	ctsk.itskpri = TMAX_TPRI - 1;
	ctsk.exinf   = 3;
	cre_tsk(3, &ctsk);
	act_tsk(3);

	/* -- draw fixed screen layout ------------------------------------- */
	draw_header();

	/* -- main loop: count up, alternate with Task 3 ------------------- */
	while (1) {
		char	mbf_buf[64];
		ER_UINT	mbf_ret;

		task_count[1]++;
		if (task_count[1] >= COUNTER_WRAP) task_count[1] = 0;
		{
			char sp[2] = { spin_chars[task_count[1] & 3], 0 };
			print_at(ROW_TASK1, 15, sp, ATTR_GREEN);
		}
		print_dec_at(ROW_TASK1, 19, task_count[1], 8, ATTR_GREEN);

		/* receive a line from MBF 1 with timeout.
		 * If a line arrives: returns message size (> 0).
		 * If empty Enter:   returns 0 → clear display.
		 * If no message: blocks for 20 ticks (~0.33s), returns E_TMOUT.
		 * Either way, the timeout provides pacing (no busy-wait). */
		mbf_ret = trcv_mbf(1, mbf_buf, 20);
		if ((INT)mbf_ret > 0) {
			/* truncate to fit columns 36-79 (44 chars max) */
			INT len = (INT)mbf_ret;
			if (len > 80 - 36)
				len = 80 - 36;
			mbf_buf[len] = '\0';
			print_at(ROW_TASK1, 36, mbf_buf, ATTR_YELLOW);
			if (36 + len < 80)
				fill_at(ROW_TASK1, 36 + len,
					80 - 36 - len, ' ', ATTR_YELLOW);
		} else if ((INT)mbf_ret == 0) {
			/* empty Enter: reset to initial display */
			print_at(ROW_TASK1, 36, "-", ATTR_DARK);
			fill_at(ROW_TASK1, 37, 80 - 37, ' ', ATTR_DARK);
		}

		wup_tsk(3);		/* wake Task 3 */
		slp_tsk();		/* wait for Task 3 to wake us */
	}
}

/* == usr_main (Task3, ID=3, CPU 0) ======================================== */
/* Competes with Task 2 (CPU 1) for semaphore 1 to access shared_count.    */
/* Phases: LOCK (hold sem, inc shared) → ---- (rest) → try again            */
__attribute__((section(".user_text")))
void
usr_main(VP_INT arg)
{
	INT have_sem = 0;
	INT phase_cnt = 0;

	slp_tsk();			/* wait for first wake from Task 1 */

	while (1) {
		task_count[3]++;
		if (task_count[3] >= COUNTER_WRAP) task_count[3] = 0;
		{
			char sp[2] = { spin_chars[task_count[3] & 3], 0 };
			print_at(ROW_TASK3, 15, sp, ATTR_GREEN);
		}
		print_dec_at(ROW_TASK3, 19, task_count[3], 8, ATTR_GREEN);

		if (have_sem) {
			/* holding semaphore — increment shared */
			shared_count++;
			if (shared_count >= COUNTER_WRAP) shared_count = 0;
			print_dec_at(ROW_SHARED, 67, shared_count,
					8, ATTR_GREEN);
			phase_cnt++;
			if (phase_cnt >= SEM_HOLD) {
				sig_sem(1);
				have_sem = 0;
				phase_cnt = 0;
				/* clear arrow: [LOCK]---+ → blank, '\' → blank */
				fill_at(ROW_TASK3, 30, 10, ' ', ATTR_DARK);
				fill_at(ROW_ARROW_UP, COL_DIAG, 1,
					' ', ATTR_DARK);
				/* dim the +---> and shared label */
				print_at(ROW_SHARED, COL_PLUS_SHARED,
					"+---> Shared (sem 1)",
					ATTR_DARK);
			}
		} else if (phase_cnt < SEM_REST) {
			/* resting — don't touch semaphore */
			phase_cnt++;
		} else {
			/* try to acquire */
			if (pol_sem(1) == E_OK) {
				have_sem = 1;
				phase_cnt = 0;
				/* draw arrow: [LOCK]---+ \ +---> in green */
				print_at(ROW_TASK3, 30,
					"[LOCK]---+", ATTR_YELLOW);
				fill_at(ROW_ARROW_UP, COL_DIAG, 1,
					'\\', ATTR_GREEN);
				print_at(ROW_SHARED, COL_PLUS_SHARED,
					"+---> Shared (sem 1)",
					ATTR_GREEN);
				shared_count++;
				if (shared_count >= COUNTER_WRAP) shared_count = 0;
				print_dec_at(ROW_SHARED, 67, shared_count,
						8, ATTR_GREEN);
			} else {
				print_at(ROW_TASK3, 30,
					"[BUSY]---+", ATTR_RED);
				/* no diagonal — blocked */
			}
		}

		delay();
		wup_tsk(1);		/* wake Task 1 */
		slp_tsk();		/* wait for Task 1 to wake us */
	}
}

/* == check_seg (debugging utility, called from arp.c etc.) ================ */
__attribute__((section(".user_text")))
void
check_seg(void)
{
	H cs = get_cs();
	H ds = get_ds();
	H ss = get_ss();
	UW esp = get_esp();
	UW eflags = get_eflags();
	printf("%x:%x:%x:%x:%x\n", cs, ds, ss, esp, eflags);
}

/* == second_task (Task2, ID=2, CPU 1) ===================================== */
/* Runs on CPU 1. Creates Task 4 (keyboard). Counts up and competes with   */
/* Task 3 (CPU 0) for semaphore 1.                                          */
__attribute__((section(".user_text")))
void
second_task(void)
{
	{
		T_CDTQ	cdtq;
		T_CTSK	ctsk;
		void	kbd_task(VP_INT);

		/* -- create DTQ 2 for keyboard ISR → kbd_task ----------- */
		cdtq.dtqatr = TA_TFIFO;
		cdtq.dtqcnt = 16;
		cdtq.dtq    = (VP)0;
		cre_dtq(2, &cdtq);

		/* -- create Task 4 (keyboard, CPU 1, highest priority) -- */
		ctsk.task    = (FP)kbd_task;
		ctsk.stk     = tsk_stack_alloc(1024);
		ctsk.stksz   = 1024;
		ctsk.itskpri = 1;
		ctsk.exinf   = 4;
		cre_tsk(4, &ctsk);
		act_tsk(4);
	}

	/* -- main loop: count up, compete for semaphore ------------------- */
	{
		INT have_sem = 0;
		INT phase_cnt = 0;

		while (1) {
			task_count[2]++;
			if (task_count[2] >= COUNTER_WRAP) task_count[2] = 0;
			{
				char sp[2] = { spin_chars[task_count[2] & 3], 0 };
				print_at(ROW_TASK2, 15, sp, ATTR_MAGENTA);
			}
			print_dec_at(ROW_TASK2, 19, task_count[2],
					8, ATTR_MAGENTA);

			if (have_sem) {
				/* holding semaphore — increment shared */
				shared_count++;
				if (shared_count >= COUNTER_WRAP) shared_count = 0;
				print_dec_at(ROW_SHARED, 67, shared_count,
						8, ATTR_MAGENTA);
				phase_cnt++;
				if (phase_cnt >= SEM_HOLD) {
					sig_sem(1);
					have_sem = 0;
					phase_cnt = 0;
					/* clear arrow */
					fill_at(ROW_TASK2, 30, 10,
						' ', ATTR_DARK);
					fill_at(ROW_ARROW_DN, COL_DIAG, 1,
						' ', ATTR_DARK);
					print_at(ROW_SHARED, COL_PLUS_SHARED,
						"+---> Shared (sem 1)",
						ATTR_DARK);
				}
			} else if (phase_cnt < SEM_REST) {
				/* resting — don't touch semaphore */
				phase_cnt++;
			} else {
				/* try to acquire */
				if (pol_sem(1) == E_OK) {
					have_sem = 1;
					phase_cnt = 0;
					/* draw arrow: [LOCK]---+ / +---> in magenta */
					print_at(ROW_TASK2, 30,
						"[LOCK]---+", ATTR_YELLOW);
					fill_at(ROW_ARROW_DN, COL_DIAG, 1,
						'/', ATTR_MAGENTA);
					print_at(ROW_SHARED, COL_PLUS_SHARED,
						"+---> Shared (sem 1)",
						ATTR_MAGENTA);
					shared_count++;
					if (shared_count >= COUNTER_WRAP) shared_count = 0;
					print_dec_at(ROW_SHARED, 67,
						shared_count, 8, ATTR_MAGENTA);
				} else {
					print_at(ROW_TASK2, 30,
						"[BUSY]---+", ATTR_RED);
				}
			}

			delay();
		}
	}
}

/* == idle_task (Task5/6, CPU 0/1, lowest priority) ======================== */
/* Per-CPU idle task. Always RDY, runs when no other task is available.      */
/* Prevents "ghost running" of WAI tasks when a CPU has no RDY tasks.       */
__attribute__((section(".user_text")))
void
idle_task(void)
{
	while (1)
		__asm__ volatile("pause");
}

/* == kbd_task (Task4, ID=4, CPU 1) ======================================== */
/* Keyboard input echo. Highest priority on CPU 1, preempts Task 2.         */
/* IRQ1 → ipsnd_dtq(DTQ 2) → rcv_dtq blocks here until a key arrives.      */
/* Printable chars are buffered locally; Enter sends the line to MBF 1.     */
__attribute__((section(".user_text")))
void
kbd_task(VP_INT arg)
{
	INT	c;
	INT	kbd_col = 20;		/* cursor column (start after "> ") */
	INT	kbd_max = 76;		/* wrap before screen edge */
	INT	line_pos = 0;		/* current position in line_buf */
	INT	key_cnt = 0;		/* keypress counter (for spinner) */
	char	line_buf[64];		/* accumulates chars until Enter */
	VP_INT	data;

	/* tell keyboard ISR to send scancodes to DTQ 2 */
	set_key_task(2);

	while (1) {
		rcv_dtq(2, &data);	/* block until keyboard ISR sends a char */
		c = (INT)data;

		/* spinner: shows Task4 waking on each keypress */
		key_cnt++;
		{
			char sp[2] = { spin_chars[key_cnt & 3], 0 };
			print_at(ROW_KBD, 15, sp, ATTR_YELLOW);
		}

		if (c >= ' ' && c < 0x7f && line_pos < 63) {
			/* printable char: echo on screen + accumulate */
			char s[2];
			s[0] = (char)c;
			s[1] = '\0';
			print_at(ROW_KBD, kbd_col, s, ATTR_YELLOW);
			line_buf[line_pos++] = (char)c;
			kbd_col++;
			if (kbd_col >= kbd_max) {
				/* line wrap: send what we have + reset */
				psnd_mbf(1, line_buf, line_pos);
				fill_at(ROW_KBD, 20,
					kbd_max - 20, ' ', 0x0E);
				kbd_col = 20;
				line_pos = 0;
			}
			set_cursor(ROW_KBD, kbd_col);
		} else if (c == '\r') {
			/* Enter: send line (or empty) via MBF 1 */
			psnd_mbf(1, line_buf, line_pos);
			fill_at(ROW_KBD, 20,
				kbd_max - 20, ' ', 0x0E);
			kbd_col = 20;
			line_pos = 0;
			set_cursor(ROW_KBD, kbd_col);
		} else if (c == '\b') {
			/* Backspace: erase last char from buffer + screen */
			if (kbd_col > 20 && line_pos > 0) {
				kbd_col--;
				line_pos--;
				fill_at(ROW_KBD, kbd_col, 1, ' ', 0x0E);
				set_cursor(ROW_KBD, kbd_col);
			}
		}
	}
}

