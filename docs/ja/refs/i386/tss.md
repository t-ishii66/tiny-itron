# tss.c / tss.h / tssP.h

対象ファイル: `i386/tss.c`, `i386/tss.h`, `i386/tssP.h`

## 概要

i386 の Task State Segment (TSS) を管理するモジュール。

本カーネルでは TSS は **Ring 3→Ring 0 遷移時のスタック切り替え** にのみ使用される。
Ring 3 で割り込みが発生すると、CPU は TSS の ESP0/SS0 フィールドを参照して
カーネルスタックに自動切り替えする。タスクスイッチごとに `tss_update_esp0()` で
ESP0 を更新し、各タスクの per-task カーネルスタックを指すようにする。

**ハードウェアタスクスイッチ (`ljmp`) は使用しない。** 初回タスク起動は `ltr` で
Task Register をロード後、`proc.kern_esp` → `ret` → `intr_return_restore` →
RESTORE_ALL → `iret` で Ring 3 に遷移する。以降のタスクスイッチは `intr_leave` での
ESP スワップによるソフトウェアコンテキストスイッチで行う。

TSS インスタンスは CPU ごとに 1 つ、計 2 つのみ (`tss0`, `tss1`)。
ダミー TSS は存在しない。

## 定数・マクロ

### INIT_EFLAGS (tssP.h)

| 定数 | 値 | 説明 |
|------|------|------|
| `INIT_EFLAGS` | `0x200` | 初期 EFLAGS 値。IF=1 (割り込み許可)。`proc_create()` で使用 |

**注意:** `INIT_EFLAGS` は tssP.h で定義されているが、`tss_init()` では使用されない。
TSS のレジスタフィールド (eip, eflags, cs 等) は全て未使用のためゼロのまま。
この定数は `proc_create()` が per-task カーネルスタック上の偽フレームを構築する際に使用する。

## 構造体・型

### tss_t (Task State Segment)

```c
typedef struct tss {
    unsigned short  prev_link;      /* 前タスクのセレクタ */
    unsigned short  dummy0;
    unsigned long   esp0;           /* Ring 0 スタックポインタ ★使用 */
    unsigned short  ss0;            /* Ring 0 スタックセグメント ★使用 */
    unsigned short  dummy1;
    unsigned long   esp1;           /* Ring 1 スタックポインタ (未使用) */
    unsigned short  ss1;
    unsigned short  dummy2;
    unsigned long   esp2;           /* Ring 2 スタックポインタ (未使用) */
    unsigned short  ss2;
    unsigned short  dummy3;
    unsigned long   cr3;            /* ページディレクトリベース (未使用) */
    unsigned long   eip;            /* 命令ポインタ (未使用) */
    unsigned long   eflags;         /* フラグレジスタ (未使用) */
    unsigned long   eax;            /* (未使用) */
    unsigned long   ecx;
    unsigned long   edx;
    unsigned long   ebx;
    unsigned long   esp;            /* (未使用) */
    unsigned long   ebp;
    unsigned long   esi;
    unsigned long   edi;
    unsigned short  es;             /* (未使用) */
    unsigned short  dummy4;
    unsigned short  cs;             /* (未使用) */
    unsigned short  dummy5;
    unsigned short  ss;             /* (未使用) */
    unsigned short  dummy6;
    unsigned short  ds;             /* (未使用) */
    unsigned short  dummy7;
    unsigned short  fs;
    unsigned short  dummy8;
    unsigned short  gs;
    unsigned short  dummy9;
    unsigned short  ldt;
    unsigned short  dummy10;
    unsigned short  t;              /* デバッグトラップフラグ */
    unsigned short  io_base;        /* I/O マップベースアドレス */
} tss_t;
```

**サイズ:** 104 バイト (Intel i386 TSS 仕様)

**実際に使用されるフィールド:** `esp0` と `ss0` のみ。
他のフィールド (eip, eflags, cs, ds, esp 等) はハードウェアタスクスイッチ用だが、
本カーネルでは `ljmp` を使わないため全て未使用 (ゼロのまま)。

`dummy*` フィールドは 16 ビットセグメントレジスタを 32 ビット境界にアラインするためのパディング。

## グローバル変数

### tss0, tss1

```c
tss_t tss0, tss1;  /* tss.c で定義 */
```

per-CPU の TSS インスタンス。`tss0` は CPU 0 用、`tss1` は CPU 1 用。

- `tss_init()` で初期化 (esp0, ss0 の設定 + GDT 登録)
- `start_first_task()` が `ltr(SEL_TSS0)` で Task Register に tss0 をロード
- `start_second_task()` が `ltr(SEL_TSS1)` で Task Register に tss1 をロード
- 以降、タスクスイッチのたびに `tss_update_esp0()` で esp0 が更新される

**注意:** `tss0`, `tss1` は `static` ではなく外部リンケージを持つ。
`tss.h` で `extern tss_t tss0, tss1;` として宣言されている。

## 関数リファレンス

### tss_init

```c
void tss_init(void)
```

**概要:** 2 つの TSS をゼロクリアし、esp0/ss0 を設定して GDT に登録する。

**処理内容:**

1. `tss0` と `tss1` を 104 バイトずつゼロクリア
2. CPU 0 (tss0):
   - `esp0 = proc[1].kern_stack_top` (Task 1 のカーネルスタックトップ)
   - `ss0 = SEL_K32_S` (0x30、カーネルスタックセグメント)
3. CPU 1 (tss1):
   - `esp0 = proc[2].kern_stack_top` (Task 2 のカーネルスタックトップ)
   - `ss0 = SEL_K32_S`
4. GDT に TSS ディスクリプタを登録:
   - `SEL_TSS0` (0x38): tss0 の TSS ディスクリプタ (タイプ ST_TSS = 0x89)
   - `SEL_TSS1` (0x40): tss1 の TSS ディスクリプタ

**呼び出し元:** `main()` (BSP パス、`proc_init()` の後)

**前提条件:** `proc_init()` が先に呼ばれている必要がある (`proc[1].kern_stack_top` と
`proc[2].kern_stack_top` が設定済みであること)。

**注意点:**
- eip, eflags, cs, ds, ss, esp などのレジスタフィールドは設定しない (ゼロのまま)。
  ハードウェアタスクスイッチを使わないため不要
- GDT エントリ 0x48 と 0x50 (旧アーキテクチャでダミー TSS に使われていたスロット) は未使用

---

### tss_update_esp0

```c
void tss_update_esp0(int cpu, unsigned long esp0)
```

**概要:** 指定 CPU の TSS.esp0 を更新する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `cpu` | `int` | CPU 番号 (0 または 1) |
| `esp0` | `unsigned long` | 新しい esp0 値 (タスクの `kern_stack_top`) |

**戻り値:** なし (`void`)

**処理内容:**
```c
if (cpu == 0)
    tss0.esp0 = esp0;
else
    tss1.esp0 = esp0;
```

**呼び出し元:** `intr_leave` (intr.s) — タスクスイッチ時に新タスクの `kern_stack_top` を設定

**目的:** Ring 3→Ring 0 の割り込み時、CPU は TSS.esp0 を参照してカーネルスタックに切り替える。
タスクスイッチ後、次の割り込みで正しいタスクのカーネルスタックが使われるよう
`intr_leave` が毎回この関数を呼ぶ。

## 補足

### TSS の役割 (本カーネルにおける)

```
Ring 3 → Ring 0 遷移時:
  CPU が TSS.esp0 を読み取り、ESP を自動切り替え
  CPU が TSS.ss0 を読み取り、SS を自動切り替え

Ring 0 内:
  TSS は参照されない (ソフトウェアコンテキストスイッチ)
```

TSS のレジスタフィールド (eip, eflags, cs 等) はハードウェアタスクスイッチ (`ljmp`) 用だが、
本カーネルでは使用しない。

### TSS と GDT の関係

| GDT セレクタ | 値 | 内容 |
|-------------|------|------|
| `SEL_TSS0` | `0x38` | CPU 0 用 TSS ディスクリプタ |
| `SEL_TSS1` | `0x40` | CPU 1 用 TSS ディスクリプタ |

**旧アーキテクチャとの違い:**
- 旧: 4 TSS (tss0, tss1, tss_dummy0, tss_dummy1) + `ljmp` によるハードウェアタスクスイッチ
- 現: 2 TSS (tss0, tss1) のみ。`ltr` で Task Register をロードし、以降は esp0 の動的更新のみ

### 起動シーケンスにおける TSS の使用

```
main() (CPU 0):
  tss_init()                           ← tss0/tss1 を初期化、GDT 登録
  ...
  start_first_task()                   ← klib.s
    cltr(SEL_TSS0)                     ← ltr で Task Register に tss0 をロード
    movl current_proc, %ebx
    movl (%ebx), %esp                  ← Task 1 の kern_esp を ESP にロード
    ret → intr_return_restore          ← RESTORE_ALL → iret で Ring 3 へ

smp_ap_init() (CPU 1):
  start_second_task()                  ← klib.s
    cltr(SEL_TSS1)                     ← ltr で Task Register に tss1 をロード
    movl current_proc+4, %ebx
    movl (%ebx), %esp                  ← Task 2 の kern_esp を ESP にロード
    ret → intr_return_restore          ← RESTORE_ALL → iret で Ring 3 へ
```

`ltr` は Task Register に TSS セレクタをロードするだけで、レジスタの切り替えは行わない。
実際の Ring 3 への遷移は `RESTORE_ALL` + `iret` で行われる。

### タスクスイッチ時の esp0 更新フロー

```
1. タイマー割り込み発生
   CPU: TSS.esp0 → カーネルスタック (現タスク)
   ↓
2. SAVE_ALL (現タスクのレジスタを退避)
   ↓
3. intr_leave (タスクスイッチ)
   ESP = 新タスクの kern_esp
   tss_update_esp0(cpu, 新タスクの kern_stack_top)  ★ここで更新
   ↓
4. RESTORE_ALL → iret (新タスクへ)
   ↓
5. 次の割り込み発生
   CPU: TSS.esp0 → カーネルスタック (新タスク) ← Step 3 で更新した値が使われる
```
