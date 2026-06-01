# kernelval.c / kernelval.h

対象ファイル: `i386/kernelval.c`, `i386/kernelval.h`

## 概要

i386 レイヤのグローバルカーネル変数を集約して定義するモジュール。
プロセステーブル、per-CPU の現在タスク情報、CPU 状態、タイマー関連変数を提供する。

これらの変数は複数のモジュール (`proc.c`, `interrupt.c`, `syscall.c`, `smp.c`,
`intr.s` など) から参照される。`kernelval.h` で `extern` 宣言を提供し、
`kernelval.c` で実体を定義する。

## 定数・マクロ

### MAX_CPU (kernelval.h)

| 定数 | 値 | 説明 |
|------|------|------|
| `MAX_CPU` | 2 | サポートする CPU の最大数 |

per-CPU 配列のサイズとして使用される (`current_proc[MAX_CPU]`, `c_tskid[MAX_CPU]`,
`timer_return[MAX_CPU]`, `clock_tick[MAX_CPU]`, `lost_tick[MAX_CPU]`)。

## 構造体・型

なし (proc_t は `proc.h` で定義)。

## グローバル変数

### cpu_num

```c
int cpu_num = 0;
```

**説明:** CPU 番号。BSP (Boot Strap Processor) は 0、AP (Application Processor) は 1。

**用途:** `main()` で BSP/AP の分岐に使用。BSP が `cpu_num = 1` に設定した後、AP が `main()` に到達すると AP パスに分岐する。

**初期値:** 0 (BSP)

**参照元:** `main.c` (起動パス分岐), `run.s` (CPU 1 スタック選択)

---

### current_proc

```c
proc_t* current_proc[MAX_CPU];
```

**説明:** per-CPU の現在実行中プロセスへのポインタ配列。

**用途:**
- `current_proc[0]`: CPU 0 で現在実行中のタスクの `proc_t` 構造体
- `current_proc[1]`: CPU 1 で現在実行中のタスクの `proc_t` 構造体

割り込みハンドラ (`intr.s` の `save`/`restore`) がレジスタの保存先・復元元を決定するために参照する。`sched_do_next_tsk()` がタスク切り替え時に更新する。

**初期値:** `proc_init()` で Task 1 (CPU 0) と Task 2 (CPU 1) のポインタに設定。

**参照元:** `intr.s` (save/restore), `interrupt.c` (stack_adjust), `syscall.c` (戻り値書き込み), `proc.c` (初期化)

---

### proc

```c
proc_t proc[MAX_TSKID];
```

**説明:** プロセステーブル。全タスクの `proc_t` 構造体を保持する配列。

**用途:** タスク ID (1-based) をインデックスとしてアクセスする。`proc[1]` = タスク 1, `proc[2]` = タスク 2, ...。`proc[0]` は通常使用されない。

**サイズ:** `MAX_TSKID` = 16 エントリ (`include/config.h` で定義)。

**参照元:** `proc.c` (生成/削除), `interrupt.c` (stack_adjust), 他

---

### c_tskid

```c
ID c_tskid[MAX_CPU];
```

**説明:** per-CPU の現在タスク ID 配列。

**用途:**
- `c_tskid[0]`: CPU 0 で現在実行中のタスク ID
- `c_tskid[1]`: CPU 1 で現在実行中のタスク ID

`current_proc[]` がポインタを保持するのに対し、こちらはタスク ID (整数) を保持する。
ITRON カーネル層でタスク状態の変更 (`tsk_stat_change` など) に使用される。

**初期値:** `proc_init()` で `c_tskid[0] = 1`, `c_tskid[1] = 2` に設定。

**参照元:** `proc.c` (初期化), カーネル層 (スケジューラ)

---

### cpu_stat

```c
extern int cpu_stat;
```

**説明:** CPU のロック状態。`CPU_LOCK` (ロック中) または `CPU_UNLOCK` (アンロック)。

**用途:** `cpu_lock()` / `cpu_unlock()` の状態管理に使用。

**参照元:** カーネル層

**注意点:** `kernelval.c` では `extern` 宣言のみ。実体はカーネル層で定義される。

---

### dispatch_stat

```c
extern int dispatch_stat;
```

**説明:** ディスパッチ (タスク切り替え) の有効/無効状態。`DISPATCH_ENABLE` (有効) または `DISPATCH_DISABLE` (無効)。

**用途:** ディスパッチ禁止中はタスク切り替えを抑制する。

**参照元:** カーネル層

**注意点:** `kernelval.c` では `extern` 宣言のみ。実体はカーネル層で定義される。

---

### timer_return

```c
int timer_return[MAX_CPU];
```

**説明:** per-CPU のタイマー割り込み抑制フラグ。

**用途:**
- 1 に設定: タイマー割り込み処理をスキップ (IRQ 処理中に再入を防止)
- 0 に設定: 通常のタイマー割り込み処理を行う

`irq_enter()` で 1 に設定、`irq_exit()` で 0 に復帰する。

**初期値:** `proc_init()` で全 CPU 分 0 に初期化。

**参照元:** `interrupt.c` (irq_enter/irq_exit), `timer.c` (timer_intr)

---

### clock_tick

```c
unsigned long clock_tick[MAX_CPU];
```

**説明:** per-CPU のティックカウンタ。タイマー割り込みのたびにインクリメントされる。

**用途:** システム経過時間の追跡。`timer_intr()` で更新される。

**初期値:** `proc_init()` で全 CPU 分 0 に初期化。

**参照元:** `timer.c` (timer_intr)

---

### lost_tick

```c
unsigned long lost_tick[MAX_CPU];
```

**説明:** per-CPU のロストティックカウンタ。タイマー割り込みが抑制された期間のティック数を記録する。

**用途:** `timer_return` フラグにより抑制されたタイマー割り込みのカウント。デバッグやシステム監視に使用。

**初期値:** `proc_init()` で全 CPU 分 0 に初期化。

**参照元:** `timer.c` (timer_intr)

## 関数リファレンス

なし (変数定義のみのモジュール)。

## 補足

### 変数の定義場所と extern 宣言の関係

| 変数 | 定義場所 | extern 宣言 |
|------|----------|-------------|
| `cpu_num` | `kernelval.c` | `kernelval.h` |
| `current_proc[]` | `kernelval.c` | `kernelval.h`, `proc.h` |
| `proc[]` | `kernelval.c` | `kernelval.h`, `proc.h` |
| `c_tskid[]` | `kernelval.c` | `kernelval.h` |
| `cpu_stat` | カーネル層 | `kernelval.c` (extern), `kernelval.h` |
| `dispatch_stat` | カーネル層 | `kernelval.c` (extern), `kernelval.h` |
| `timer_return[]` | `kernelval.c` | `kernelval.h` |
| `clock_tick[]` | `kernelval.c` | `kernelval.h` |
| `lost_tick[]` | `kernelval.c` | `kernelval.h` |

### GDB で確認する場合

```
(gdb) p cpu_num
(gdb) p current_proc[0]
(gdb) p current_proc[1]
(gdb) p *current_proc[0]
(gdb) p c_tskid[0]
(gdb) p c_tskid[1]
(gdb) p clock_tick[0]
(gdb) p clock_tick[1]
(gdb) p timer_return[0]
(gdb) p timer_return[1]
```
