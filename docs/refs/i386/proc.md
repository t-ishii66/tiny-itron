# proc.c / proc.h / procP.h

対象ファイル: `i386/proc.c`, `i386/proc.h`, `i386/procP.h`

## 概要

i386 レイヤのプロセス (タスク) 管理モジュール。プロセス構造体 (`proc_t`) の管理、
タスクの生成・削除、レジスタセーブ領域の初期化を行う。

ITRON カーネル層 (`kernel/`) がタスク状態 (`TTS_RUN`, `TTS_RDY` など) を管理するのに対し、
このモジュールは i386 固有のコンテキスト (レジスタ、スタック、CPU アフィニティ) を管理する。

`proc_t` 構造体の `reg[]` 配列は、`intr.s` の `save`/`restore` マクロがスタックとして
使用するレジスタ保存領域である。`save` のたびに `stack` ポインタが 52 バイト進み、
`restore` で 52 バイト戻る。

## 定数・マクロ

### レジスタオフセット (proc.h)

`reg[]` 配列内のインデックスとして使用される定数:

| 定数 | 値 | 説明 |
|------|------|------|
| `ECX` | 0 | ECX レジスタ |
| `EDX` | 1 | EDX レジスタ |
| `ESP` | 2 | ESP (スタックポインタ) |
| `EIP` | 3 | EIP (命令ポインタ) |
| `EBP` | 4 | EBP (フレームポインタ) |
| `ESI` | 5 | ESI |
| `EDI` | 6 | EDI |
| `EFLAGS` | 7 | EFLAGS |
| `EAX` | 8 | EAX レジスタ |
| `EBX` | 9 | EBX レジスタ |
| `DS` | 10 | DS セグメントレジスタ |
| `ES` | 11 | ES セグメントレジスタ |
| `SAV_EFLAGS` | 12 | EFLAGS 退避用 |

### フラグ (proc.h)

| 定数 | 値 | 説明 |
|------|------|------|
| `FLAG_USE` | 1 | プロセス構造体使用中 |
| `FLAG_NON` | 0 | プロセス構造体未使用 |

### 初期 EFLAGS (procP.h)

| 定数 | 値 | 説明 |
|------|------|------|
| `INIT_EFLAGS` | `0x00000200` | 割り込み許可 (IF=1) |

## 構造体・型

### proc_t (プロセス構造体)

```c
typedef struct proc {
    unsigned long   stack;      /* save/restore スタックポインタ */
    unsigned long   reg[64];    /* レジスタ保存領域 (save/restore がスタックとして使用) */
    int             flag;       /* FLAG_USE or FLAG_NON */
    int             id;         /* タスク ID (1-based) */
    int             cpu;        /* CPU アフィニティ (0 or 1) */
} proc_t;
```

**フィールド詳細:**

- `stack`: `save`/`restore` マクロが使用するスタックポインタ。初期値は `&reg[0]` で、`save` のたびに 52 バイト (13 スロット x 4 バイト) 加算される。
- `reg[64]`: 64 エントリの `unsigned long` 配列。最初の 13 エントリ (0-12) は初期レジスタ値の保持に使用。`save` が呼ばれるたびに 13 エントリ分消費される。64 エントリあるため約 4 段のネスト (64 / 13 = 4) まで対応。
- `flag`: プロセス構造体の使用状態。`proc_create()` で `FLAG_USE`、`proc_delete()` で `FLAG_NON` に設定。
- `id`: タスク ID。`proc_init()` で配列インデックスに設定。
- `cpu`: CPU アフィニティ。0 = CPU 0、1 = CPU 1。`sched_do_next_tsk()` がこの値でフィルタリングする。

### save フレームレイアウト

`save` マクロが 1 回の保存で書き込む 13 スロット (52 バイト):

```
offset  0: ECX
offset  1: EDX
offset  2: ESP (ユーザーモードスタックポインタ)
offset  3: EIP (復帰アドレス)
offset  4: EBP
offset  5: ESI
offset  6: EDI
offset  7: EFLAGS
offset  8: EAX
offset  9: EBX
offset 10: DS
offset 11: ES
offset 12: old_stack (前の save フレームへのポインタ)
```

## グローバル変数

### current_proc

```c
proc_t* current_proc[MAX_CPU];
```

per-CPU の現在実行中プロセスへのポインタ。`kernelval.c` で定義。
`current_proc[0]` は CPU 0 の現在タスク、`current_proc[1]` は CPU 1 の現在タスク。

### proc

```c
proc_t proc[MAX_TSKID];
```

プロセステーブル。`MAX_TSKID` (16) エントリ。`kernelval.c` で定義。
タスク ID をインデックスとしてアクセスされる (例: `proc[1]` = タスク 1)。

## 関数リファレンス

### proc_init

```c
void proc_init(void)
```

**概要:** プロセステーブルを初期化し、初期タスク (Task 1, 2, 5, 6) を生成する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. 全 `proc[]` エントリを初期化:
   - `stack` を `&reg[0]` に設定 (save スタックの初期位置)
   - `id` を配列インデックスに設定
   - `cpu` を 0 に設定
2. Task 1 (`first_task`): CPU 0、優先度 `TMAX_TPRI - 1` (15)。`sys_cre_tsk()` + `sys_act_tsk()` で ITRON 層にも登録。状態を `TTS_RUN` に設定。`current_proc[0]` に設定。
3. Task 2 (`second_task`): CPU 1、優先度 `TMAX_TPRI - 1` (15)。同様に生成。`current_proc[1]` に設定。
4. Task 5 (`idle_task`): CPU 0、優先度 `TMAX_TPRI` (16、最低優先度)。`exinf = 5`。
5. Task 6 (`idle_task`): CPU 1、優先度 `TMAX_TPRI` (16)。`exinf = 6`。
6. タイマー変数の初期化: `timer_return[]`, `clock_tick[]`, `lost_tick[]` を全 CPU 分ゼロ初期化。

**呼び出し元:** `main()` (BSP パス、`itron_init()` の後)

**注意点:**
- `itron_init()` が先に呼ばれていないと、`sys_cre_tsk()` が正しく動作しない。
- Task 5, 6 はアイドルタスクで、他に実行可能なタスクがない場合に CPU を占有する。
- `exinf` (拡張情報) はタスク関数の引数として渡される。Task 5 は `exinf = 5`、Task 6 は `exinf = 6` で、`idle_task()` が自身のタスク ID を識別するのに使用。

---

### proc_create

```c
proc_t* proc_create(ID tskid, T_CTSK* pk_ctsk)
```

**概要:** 新しいタスクのプロセス構造体を初期化する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `tskid` | `ID` | タスク ID |
| `pk_ctsk` | `T_CTSK*` | タスク生成パラメータ (関数ポインタ、スタック、サイズなど) |

**戻り値:** `proc_t*` -- `proc` 配列の先頭ポインタ (タスク固有のポインタではない)。

**処理内容:**

1. `stack` を `&reg[0] + 52` (最初の save フレームの終端) に設定
2. 全レジスタ (EAX-EDI) を 0 に初期化
3. `EFLAGS` を `INIT_EFLAGS` (0x200, IF=1) に設定
4. `EIP` を `pk_ctsk->task` (タスク関数のアドレス) に設定
5. `ESP` を `pk_ctsk->stk + pk_ctsk->stksz` (スタックの最上位) に設定し、4 バイト境界にアラインメント
6. `DS`, `ES` を `SEL_U32_D` (ユーザーデータセグメント) に設定
7. `flag` を `FLAG_USE` に設定
8. APIC ID を読み取り、`cpu` (CPU アフィニティ) を設定 (作成元 CPU を継承)
9. `proc_set_tsk_arg()` でタスク引数と復帰アドレスをスタックに設定

**呼び出し元:** `sys_cre_tsk()` (ITRON カーネル層のタスク生成)

**注意点:**
- `stack` の初期値 `reg[0] + 52` は、最初の `save` が `reg[0]` から 13 スロット分書き込むことを想定した設定。`save` は `stack` を基点にして書き込みを開始し、書き込み後に `stack += 52` する。
- CPU アフィニティは作成元タスクの実行 CPU を継承する。Task 3 を Task 1 (CPU 0) が作成すると CPU 0 に割り当てられ、Task 4 を Task 2 (CPU 1) が作成すると CPU 1 に割り当てられる。

---

### proc_set_tsk_arg

```c
ER proc_set_tsk_arg(ID tskid, VP_INT arg)
```

**概要:** タスクスタックに引数と復帰アドレスを設定する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `tskid` | `ID` | タスク ID |
| `arg` | `VP_INT` | タスク関数への引数 (拡張情報 `exinf`) |

**戻り値:** `ER` -- 常に `E_OK`。

**処理内容:**

1. `proc[tskid].reg[ESP]` からスタックポインタを取得
2. スタックに `arg` (タスク関数の引数) をプッシュ
3. スタックに `ext_tsk` (ユーザー空間のタスク終了ラッパー) のアドレスをプッシュ
4. 更新したスタックポインタを `reg[ESP]` に保存

**呼び出し元:** `proc_create()`

**注意点:**
- `ext_tsk` (ユーザー空間の syscall ラッパー) が復帰アドレスとして設定されるため、タスク関数が `return` した場合に自動的にタスク終了処理が呼ばれる。タスクは Ring 3 で実行されるため、カーネル内の `sys_ext_tsk` ではなくユーザー空間の `ext_tsk` を使用する。
- スタックレイアウト: `[ext_tsk] [arg] ... (スタック上位)`。C の関数呼び出し規約に従い、`arg` は `esp+4` に、復帰アドレスは `esp` に配置される。

---

### proc_delete

```c
ER proc_delete(ID tskid)
```

**概要:** プロセス構造体を未使用状態にする。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `tskid` | `ID` | タスク ID |

**戻り値:** `ER` -- 常に `E_OK`。

**処理内容:**

`proc[tskid].flag` を `FLAG_NON` に設定する。

**呼び出し元:** `sys_del_tsk()` (ITRON カーネル層のタスク削除)

**注意点:** スタックやレジスタ領域の解放は行わない。

---

### proc_eflags_save

```c
void proc_eflags_save(ID tskid)
```

**概要:** タスクの EFLAGS を退避し、割り込みを禁止状態に設定する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `tskid` | `ID` | タスク ID |

**戻り値:** なし (`void`)

**処理内容:**

1. 現在の `reg[EFLAGS]` で IF ビット (bit 9、0x200) がセットされている場合、`reg[SAV_EFLAGS]` に退避
2. `reg[EFLAGS]` の IF ビットをクリア (`& 0xfffffdff`)

**呼び出し元:** タスク例外処理の前処理

**注意点:**
- IF ビットが既にクリアされている場合は `SAV_EFLAGS` に退避しない (前の保存値を保持)。
- `0xfffffdff` は `~0x00000200` に相当。

---

### proc_eflags_restore

```c
void proc_eflags_restore(ID tskid)
```

**概要:** 退避した EFLAGS を復元する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `tskid` | `ID` | タスク ID |

**戻り値:** なし (`void`)

**処理内容:**

`reg[EFLAGS]` を `reg[SAV_EFLAGS]` の値で上書きする。

**呼び出し元:** タスク例外処理の後処理

**注意点:** `proc_eflags_save()` と対で使用する。

## 補足

### reg[] のスタック的使用

`save`/`restore` (intr.s) は `proc_t.reg[]` をスタックとして使用する:

```
初期状態:
  stack → reg[0]  (空)

save 後 (1回目):
  reg[0]  = ECX
  reg[1]  = EDX
  ...
  reg[12] = old_stack (= &reg[0] の初期値)
  stack → reg[13]  (次のフレーム先頭)

save 後 (2回目, 割り込みネスト):
  reg[13] = ECX
  ...
  reg[25] = old_stack (= &reg[13])
  stack → reg[26]
```

`reg[64]` は約 4 段のネスト (64 / 13 = 4.9) まで対応。

### CPU アフィニティの決定

- `proc_init()`: Task 1 = CPU 0、Task 2 = CPU 1、Task 5 = CPU 0、Task 6 = CPU 1 (明示設定)
- `proc_create()`: APIC ID を読み取り、作成元 CPU のアフィニティを継承
- `sched_do_next_tsk()`: `proc[].cpu` と現在の CPU を比較してフィルタリング
