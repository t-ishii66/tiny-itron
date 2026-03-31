# proc.c / proc.h / procP.h

対象ファイル: `i386/proc.c`, `i386/proc.h`, `i386/procP.h`

## 概要

i386 レイヤのプロセス (タスク) 管理モジュール。プロセス構造体 (`proc_t`) の管理、
タスクの生成・削除、per-task カーネルスタック上の初期フレーム構築を行う。

ITRON カーネル層 (`kernel/`) がタスク状態 (`TTS_RUN`, `TTS_RDY` など) を管理するのに対し、
このモジュールは i386 固有のコンテキスト (カーネルスタック、CPU アフィニティ) を管理する。

`proc_t` は 16 バイトの小さな構造体で、タスクごとに 1 つ存在する。
レジスタの保存・復元は per-task カーネルスタック上の `pt_regs` フレームで行われ、
タスクスイッチは `intr_leave` での ESP スワップで実現される。

## 定数・マクロ

### Per-Task カーネルスタック定数 (addr.h)

| 定数 | 値 | 説明 |
|------|------|------|
| `KERN_STACK_SIZE` | `4096` | 1 タスクあたりのカーネルスタックサイズ (4KB) |
| `KERN_STACK_BASE` | `0x750000` | カーネルスタック領域のベースアドレス |

タスク N のカーネルスタックトップ = `KERN_STACK_BASE + (N + 1) * KERN_STACK_SIZE`。

### その他 (proc.h, procP.h)

| 定数 | 値 | 説明 |
|------|------|------|
| `INIT_EFLAGS` | `0x00000200` | 初期 EFLAGS: IF=1 (割り込み許可) |
| `PT_REGS_EAX_OFFSET` | `0x20` | pt_regs 内の EAX フィールドのバイトオフセット |

## 構造体・型

### proc_t (プロセス構造体)

```c
typedef struct proc {
    unsigned long   kern_esp;        /* saved kernel stack pointer */
    unsigned long   kern_stack_top;  /* top of kernel stack (set in TSS.esp0) */
    unsigned long   saved_eflags;    /* for proc_eflags_save/restore */
    int             cpu;             /* CPU affinity (0 or 1) */
} proc_t;
```

**サイズ:** 16 バイト (4 フィールド × 4 バイト)

**フィールド詳細:**

- `kern_esp`: `intr_leave` が保存するカーネルスタックポインタ。タスクが非実行状態のとき、
  このスタック上に pt_regs フレーム全体が保存されている。タスクスイッチ時に `intr_leave` が
  `movl %esp, (%ebx)` で保存し、`movl (%ebx), %esp` で復元する。
- `kern_stack_top`: カーネルスタックの最上位アドレス。Ring 3→Ring 0 の遷移時に CPU が
  TSS.esp0 から読み取る値。`tss_update_esp0()` でタスクスイッチごとに更新される。
- `saved_eflags`: タスク例外処理で IF ビットを退避・復元するための一時領域。
- `cpu`: CPU アフィニティ。0 = CPU 0 (BSP)、1 = CPU 1 (AP)。
  `sched_do_next_tsk()` がこの値でフィルタリングする。

### struct pt_regs (レジスタフレーム)

```c
struct pt_regs {
    unsigned long es;       /* 0x00: SAVE_ALL */
    unsigned long ds;       /* 0x04: SAVE_ALL */
    unsigned long edi;      /* 0x08: SAVE_ALL */
    unsigned long esi;      /* 0x0C: SAVE_ALL */
    unsigned long ebp;      /* 0x10: SAVE_ALL */
    unsigned long ebx;      /* 0x14: SAVE_ALL */
    unsigned long edx;      /* 0x18: SAVE_ALL */
    unsigned long ecx;      /* 0x1C: SAVE_ALL */
    unsigned long eax;      /* 0x20: SAVE_ALL */
    unsigned long eip;      /* 0x24: CPU (interrupt frame) */
    unsigned long cs;       /* 0x28: CPU (interrupt frame) */
    unsigned long eflags;   /* 0x2C: CPU (interrupt frame) */
    unsigned long esp;      /* 0x30: CPU (Ring 3->0 only) */
    unsigned long ss;       /* 0x34: CPU (Ring 3->0 only) */
};
```

**サイズ:** 56 バイト (14 × 4 バイト)

Ring 3→Ring 0 の割り込み時、CPU が SS, ESP, EFLAGS, CS, EIP を push し、
続いて SAVE_ALL マクロが EAX, ECX, EDX, EBX, EBP, ESI, EDI, DS, ES を push する。
この結果、カーネルスタック上に pt_regs フレームが構築される。

```
          Low address (ESP)
      ┌─────────────────────┐
      │ ES          (0x00)  │  ← SAVE_ALL (最後に push)
      │ DS          (0x04)  │
      │ EDI         (0x08)  │
      │ ESI         (0x0C)  │
      │ EBP         (0x10)  │
      │ EBX         (0x14)  │
      │ EDX         (0x18)  │
      │ ECX         (0x1C)  │
      │ EAX         (0x20)  │  ← SAVE_ALL (最初に push)
      ├─────────────────────┤
      │ EIP         (0x24)  │  ← CPU (interrupt frame)
      │ CS          (0x28)  │
      │ EFLAGS      (0x2C)  │
      │ ESP (user)  (0x30)  │  ← Ring 3→0 のみ
      │ SS  (user)  (0x34)  │
      └─────────────────────┘
          High address (kern_stack_top)
```

## グローバル変数

### current_proc

```c
proc_t* current_proc[MAX_CPU];  /* kernelval.c で定義 */
```

per-CPU の現在実行中プロセスへのポインタ。
`current_proc[0]` は CPU 0 の現在タスク、`current_proc[1]` は CPU 1 の現在タスク。
`intr_leave` がタスクスイッチ時に参照・更新する。

### proc

```c
proc_t proc[MAX_TSKID];  /* kernelval.c で定義 */
```

プロセステーブル。`MAX_TSKID` (16) エントリ。
タスク ID をインデックスとしてアクセスされる (例: `proc[1]` = タスク 1)。

## 関数リファレンス

### proc_init

```c
void proc_init(void)
```

**概要:** プロセステーブルを初期化し、初期タスク (Task 1, 2, 5, 6) を生成する。

**処理内容:**

1. 全 `proc[]` エントリを初期化:
   - `kern_esp = 0`
   - `kern_stack_top = KERN_STACK_BASE + (i + 1) * KERN_STACK_SIZE`
   - `saved_eflags = 0`
   - `cpu = 0`
2. Task 1 (`first_task`): CPU 0、優先度 15。`sys_cre_tsk()` + `sys_act_tsk()` で ITRON 層にも登録。状態を `TTS_RUN` に設定。`current_proc[0]` に設定。
3. Task 2 (`second_task`): CPU 1、優先度 15。同様に生成。`current_proc[1]` に設定。
4. Task 5 (`idle_task`): CPU 0、優先度 16 (最低)。`exinf = 5`。
5. Task 6 (`idle_task`): CPU 1、優先度 16。`exinf = 6`。
6. タイマー変数の初期化: `timer_return[]`, `clock_tick[]`, `lost_tick[]` を全 CPU 分ゼロ初期化。

**呼び出し元:** `main()` (BSP パス、`itron_init()` の後)

**注意点:**
- `itron_init()` が先に呼ばれていないと、`sys_cre_tsk()` が正しく動作しない。
- ここでは cre_tsk + act_tsk + TTS_RUN 設定のみ。実際の実行は `start_first_task` / `start_second_task` で開始される。

---

### proc_create

```c
proc_t* proc_create(ID tskid, T_CTSK* pk_ctsk)
```

**概要:** タスクの per-task カーネルスタック上に初期フレームを構築する。

**戻り値:** `proc_t*` -- `&proc[tskid]` (タスク固有の proc_t へのポインタ)。

**処理内容:**

タスクが最初にスケジュールされたとき、`intr_leave` の `ret` が `intr_return_restore` に飛び、
`RESTORE_ALL` + `iret` で Ring 3 のタスクエントリポイントに遷移する。
この動作のために、カーネルスタック上に以下の偽フレームを構築する:

```
      Low address (kern_esp)
  ┌──────────────────────────────┐
  │ intr_return_restore (戻り先) │  ← intr_leave の ret が pop
  ├──────────────────────────────┤
  │ ES  = SEL_U32_D | 3         │  ← RESTORE_ALL が pop する
  │ DS  = SEL_U32_D | 3         │     pt_regs フレーム
  │ EDI = 0                     │
  │ ESI = 0                     │
  │ EBP = 0                     │
  │ EBX = 0                     │
  │ EDX = 0                     │
  │ ECX = 0                     │
  │ EAX = 0                     │
  ├──────────────────────────────┤
  │ EIP = pk_ctsk->task          │  ← iret が pop する
  │ CS  = SEL_U32_C | 3         │     CPU interrupt frame
  │ EFLAGS = INIT_EFLAGS (IF=1) │
  │ ESP = user_esp               │
  │ SS  = SEL_U32_S | 3         │
  └──────────────────────────────┘
      High address (kern_stack_top)
```

1. `kern_stack_top` を計算・設定
2. CPU interrupt frame (SS, ESP, EFLAGS, CS, EIP) を構築
3. SAVE_ALL frame (EAX..ES を 0 に、DS/ES は Ring 3 セグメント) を構築
4. `intr_return_restore` のアドレスを push (intr_leave の ret が使う戻り先)
5. `kern_esp` に現在の SP を保存
6. APIC ID から CPU アフィニティを継承
7. `proc_set_tsk_arg()` でユーザースタックに引数と `ext_tsk` を配置

**呼び出し元:** `sys_cre_tsk()` (ITRON カーネル層のタスク生成)

---

### proc_set_tsk_arg

```c
ER proc_set_tsk_arg(ID tskid, VP_INT arg)
```

**概要:** タスクのユーザースタックに引数と復帰アドレスを設定する。

**処理内容:**

1. カーネルスタック上の偽 interrupt frame から ESP スロットを読み取る
   (`kern_stack_top - 2 * 4` が ESP の位置)
2. ユーザースタックに `arg` (タスク関数の引数) を push
3. ユーザースタックに `ext_tsk` (タスク終了ラッパー) のアドレスを push
4. 更新した ESP 値を偽フレームに書き戻す

**呼び出し元:** `proc_create()`

**注意点:**
- `ext_tsk` はユーザー空間の syscall ラッパーで、タスク関数が `return` した場合に
  自動的にタスク終了処理が呼ばれる。

---

### proc_delete

```c
ER proc_delete(ID tskid)
```

**概要:** プロセス構造体を解放する。

**戻り値:** `ER` -- 常に `E_OK`。

**処理内容:** 現在の実装では何もしない (`return E_OK`)。
カーネルスタックは固定アドレスのため解放不要。

**呼び出し元:** `sys_del_tsk()` (ITRON カーネル層のタスク削除)

---

### proc_eflags_save

```c
void proc_eflags_save(ID tskid)
```

**概要:** タスクの EFLAGS を退避し、割り込みを禁止状態に設定する。

**処理内容:**

1. カーネルスタック上の pt_regs フレーム (`kern_stack_top - sizeof(struct pt_regs)`) にアクセス
2. EFLAGS の IF ビット (0x200) がセットされている場合、`saved_eflags` に退避
3. pt_regs の EFLAGS から IF ビットをクリア

**呼び出し元:** タスク例外処理の前処理

---

### proc_eflags_restore

```c
void proc_eflags_restore(ID tskid)
```

**概要:** 退避した EFLAGS を復元する。

**処理内容:** pt_regs の EFLAGS を `saved_eflags` の値で上書きする。

**呼び出し元:** タスク例外処理の後処理

---

### proc_set_return_value

```c
void proc_set_return_value(proc_t *p, unsigned long val)
```

**概要:** 待ち状態のタスクにシステムコール戻り値を設定する。

**処理内容:**

待ち状態のタスクは `intr_leave` の `movl %esp, (%ebx)` で kern_esp が保存されている。
kern_esp の位置には `call intr_leave` の戻りアドレスがあり、その直上に pt_regs フレームがある
(`kern_esp + 4` が pt_regs の先頭)。pt_regs の EAX フィールドに `val` を書き込む。

```c
struct pt_regs *regs = (struct pt_regs *)(p->kern_esp + 4);
regs->eax = val;
```

RESTORE_ALL が EAX を pop するとき、この値がタスクの EAX レジスタに復元される。

**呼び出し元:** `sys_sig_sem()`, `sys_psnd_dtq()`, `sys_del_dtq()`, `sched_timeout()` 等、
待ちタスクを起床させて E_OK / E_DLT / E_TMOUT / E_RLWAI を返す全ての syscall 実装。

## 補足

### CPU アフィニティの決定

- `proc_init()`: Task 1 = CPU 0、Task 2 = CPU 1、Task 5 = CPU 0、Task 6 = CPU 1 (明示設定)
- `proc_create()`: APIC ID を読み取り、作成元 CPU のアフィニティを継承
- `sched_do_next_tsk()`: `proc[].cpu` と現在の CPU を比較してフィルタリング

### proc_create が構築するフレームとタスク起動の流れ

1. `proc_create()` がカーネルスタック上に偽の pt_regs + 戻りアドレスを構築
2. `intr_leave` がタスクスイッチ時に `kern_esp` を ESP にロード
3. `intr_leave` の `ret` が戻りアドレス (`intr_return_restore`) を pop
4. `intr_return_restore` で `RESTORE_ALL` → `iret` が実行
5. CPU が偽 interrupt frame の EIP/CS/EFLAGS/ESP/SS を pop して Ring 3 に遷移
6. タスクのエントリポイントが Ring 3 で実行開始
