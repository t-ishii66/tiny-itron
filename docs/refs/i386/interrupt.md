# interrupt.c / interrupt.h / interruptP.h

対象ファイル: `i386/interrupt.c`, `i386/interrupt.h`, `i386/interruptP.h`

## 概要

i386 の割り込み管理モジュール。IDT (Interrupt Descriptor Table) の初期化、
例外ハンドラ・IRQ ハンドラ・システムコールエントリの登録、および各種
C 言語レベルの割り込みハンドラ (`c_intr_*`) を提供する。

割り込みベクタは以下の範囲に割り当てられている:
- 例外 (0-15): CPU 例外 (除算エラー、一般保護例外など)
- IRQ0-7 (0x80-0x87): マスター PIC (8259A)
- IRQ8-15 (0x90-0x97): スレーブ PIC (8259A)
- APIC (0x98): スプリアス割り込み
- システムコール (0x99): ユーザーモードからのシステムコール
- APIC タイマー (0x9a, 0x9b): CPU 0/CPU 1 各自の Local APIC タイマー

## 定数・マクロ

### 割り込みベクタ番号 (interrupt.h)

| 定数 | 値 | 説明 |
|------|------|------|
| `VECT_IRQ0` | `0x80` | IRQ0 (PIT タイマー) |
| `VECT_IRQ1` | `0x81` | IRQ1 (キーボード) |
| `VECT_IRQ2` | `0x82` | IRQ2 (カスケード) |
| `VECT_IRQ3` | `0x83` | IRQ3 (COM2) |
| `VECT_IRQ4` | `0x84` | IRQ4 (COM1) |
| `VECT_IRQ5` | `0x85` | IRQ5 |
| `VECT_IRQ6` | `0x86` | IRQ6 (FDC) |
| `VECT_IRQ7` | `0x87` | IRQ7 (プリンタ/スプリアス) |
| `VECT_IRQ8` | `0x90` | IRQ8 (RTC) |
| `VECT_IRQ9` | `0x91` | IRQ9 |
| `VECT_IRQ10` | `0x92` | IRQ10 |
| `VECT_IRQ11` | `0x93` | IRQ11 |
| `VECT_IRQ12` | `0x94` | IRQ12 (PS/2 マウス) |
| `VECT_IRQ13` | `0x95` | IRQ13 (FPU) |
| `VECT_IRQ14` | `0x96` | IRQ14 (IDE プライマリ) |
| `VECT_IRQ15` | `0x97` | IRQ15 (IDE セカンダリ) |
| `VECT_SYSCALL` | `0x99` | システムコール |

### interruptP.h の定義

| 定数 | 説明 |
|------|------|
| `N_INTR_TABLE` | `intr_table` 配列の要素数 (`sizeof(intr_table) / sizeof(struct intr_table)`) |

## 構造体・型

### struct intr_table (interruptP.h)

```c
struct intr_table {
    void        (*func)(void);  /* アセンブリ側エントリポイント */
    unsigned char   vect;       /* 割り込みベクタ番号 */
};
```

割り込みハンドラ関数とベクタ番号の対応テーブル用構造体。
`interruptP.h` で `intr_table[]` 配列として定義されている。

## グローバル変数

### idt (interruptP.h)

```c
gate_t* idt = (gate_t*)AL_IDT;
```

IDT の先頭を指すポインタ。物理アドレス `0x2100` に配置されている。

### intr_table (interruptP.h)

```c
struct intr_table intr_table[];
```

例外ハンドラ (0-15) と IRQ ハンドラ (VECT_IRQ0-VECT_IRQ15) の
アセンブリ側エントリ関数とベクタ番号の対応テーブル。
合計 32 エントリ (例外 16 + IRQ 16)。

## 関数リファレンス

### idt_init

```c
void idt_init(void)
```

**概要:** IDT 全 256 エントリを初期化する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. 全 256 エントリに `intr_default` ハンドラをトラップゲート (`GT_TRAP`) として設定
2. `setup_trap()` で例外ハンドラ (0-15) を登録
3. `setup_irq()` で IRQ ハンドラ (VECT_IRQ0-VECT_IRQ15) を登録
4. `setup_syscall()` でシステムコールエントリ (VECT_SYSCALL) を登録

**呼び出し元:** `all_init()` (`main.c`)

**注意点:**
- デフォルトハンドラは `GT_TRAP` (トラップゲート) として設定される。トラップゲートは IF フラグをクリアしない。
- IRQ ハンドラは `GT_INTR` (割り込みゲート) として上書き登録される。割り込みゲートは自動的に IF をクリアする。

---

### set_idt

```c
void set_idt(short n, unsigned long base, unsigned short sel,
             unsigned char count, unsigned char type)
```

**概要:** IDT の指定エントリにゲートディスクリプタを設定する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `n` | `short` | IDT エントリのインデックス (0-255) |
| `base` | `unsigned long` | ハンドラ関数のアドレス |
| `sel` | `unsigned short` | コードセグメントセレクタ (通常 `SEL_K32_C`) |
| `count` | `unsigned char` | パラメータカウント (通常 0) |
| `type` | `unsigned char` | ゲートタイプ (`GT_INTR`, `GT_TRAP` など) |

**戻り値:** なし (`void`)

**処理内容:**

`idt[n]` に対して `offset_l`, `offset_h`, `sel`, `count`, `type` を設定する。
`set_gate()` とは異なり、`idt` ポインタを直接使用し、`n` をそのまま配列インデックスとして使う。

**呼び出し元:** `idt_init()`, `setup_trap()`, `setup_irq()`, `setup_syscall()`, `smp_init()`

**注意点:** なし。

---

### setup_trap

```c
static void setup_trap(void)
```

**概要:** CPU 例外 (ベクタ 0-15) のハンドラを IDT に登録する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

例外ハンドラを IDT に登録する。大部分は `GT_TRAP` (トラップゲート) だが、
一部は `GT_INTR` (割り込みゲート) として登録される:

| ベクタ | ハンドラ | 例外 | ゲート |
|--------|----------|------|--------|
| 0 | `intr_divide` | 除算エラー (#DE) | GT_INTR |
| 1 | `intr_singlestep` | デバッグ (#DB) | GT_TRAP |
| 2 | `intr_nmi` | NMI | GT_TRAP |
| 3 | `intr_breakpoint` | ブレークポイント (#BP) | GT_TRAP |
| 4 | `intr_overflow` | オーバーフロー (#OF) | GT_TRAP |
| 5 | `intr_bounds` | BOUND 範囲超過 (#BR) | GT_TRAP |
| 6 | `intr_opcode` | 無効オペコード (#UD) | GT_TRAP |
| 7 | `intr_copr_not_available` | デバイス使用不可 (#NM) | GT_TRAP |
| 8 | `intr_doublefault` | ダブルフォールト (#DF) | GT_TRAP |
| 9 | `intr_copr_seg_overrun` | コプロセッサセグメントオーバーラン | GT_TRAP |
| 10 | `intr_tss` | 無効 TSS (#TS) | GT_TRAP |
| 11 | `intr_segment_not_present` | セグメント不在 (#NP) | GT_TRAP |
| 12 | `intr_stack` | スタックセグメント障害 (#SS) | GT_TRAP |
| 13 | `intr_general` | 一般保護例外 (#GP) | GT_INTR |
| 14 | `intr_page` | ページフォールト (#PF) | GT_INTR |
| 15 | `intr_copr_error` | コプロセッサエラー (#MF) | GT_TRAP |

**呼び出し元:** `idt_init()`

**注意点:**
- ハンドラ関数名 (`intr_*`) はアセンブリ側 (`intr.s`) で定義されたエントリポイント。C 言語ハンドラ (`c_intr_*`) を呼び出す前に `save` マクロでレジスタを保存する。
- ベクタ 0, 13, 14 は `GT_INTR` (割り込みゲート) を使用する。割り込みゲートはエントリ時に自動的に IF=0 にするため、ハンドラ実行中にネスト割り込みが発生しない。

---

### setup_irq

```c
static void setup_irq(void)
```

**概要:** IRQ0-15 のハンドラを IDT に登録する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

IRQ0 (`VECT_IRQ0` = 0x80) から IRQ15 (`VECT_IRQ15` = 0x97) までの 16 個の
IRQ ハンドラを割り込みゲート (`GT_INTR`) として登録する。

**呼び出し元:** `idt_init()`

**注意点:**
- IRQ 0-7 はマスター PIC (ベクタ 0x80-0x87)、IRQ 8-15 はスレーブ PIC (ベクタ 0x90-0x97) に対応。
- 通常の PC/AT とはベクタ割り当てが異なる (標準は IRQ0=0x20)。

---

### setup_syscall

```c
static void setup_syscall(void)
```

**概要:** システムコール用の割り込みゲートを IDT に登録する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

`VECT_SYSCALL` (0x99) に `intr_syscall` ハンドラを登録する。
ゲートタイプは `GT_INTR | 0x60` で、DPL=3 (ユーザーモードからの呼び出しを許可) に設定。

**呼び出し元:** `idt_init()`

**注意点:**
- DPL=3 でないと Ring 3 のユーザータスクから `int 0x99` で呼び出せない (#GP が発生する)。
- `0x60` は `DPL=3` を type フィールドの bit 5-6 に設定する値 (`3 << 5 = 0x60`)。

---

### c_intr_irq0

```c
void c_intr_irq0(void)
```

**概要:** PIT タイマー割り込み (IRQ0) の C 言語ハンドラ。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. `timer_intr(0, 1)` を呼び出す (CPU 0、ティック数 1)
2. `i8259_reenable()` で PIC の EOI を送信

**呼び出し元:** `intr_irq0` (アセンブリ側エントリから)

**注意点:** PIT タイマーは CPU 0 のみに配送される (PIC 経由)。

---

### c_intr_irq1

```c
void c_intr_irq1(void)
```

**概要:** キーボード割り込み (IRQ1) の C 言語ハンドラ。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. `key_intr()` を呼び出す (スキャンコード読み取り、ASCII 変換、`ipsnd_dtq` で DTQ 送信)
2. `i8259_reenable()` で PIC の EOI を送信

**呼び出し元:** `intr_irq1` (アセンブリ側エントリから)

**注意点:** IRQ1 は CPU 0 に配送される。`ipsnd_dtq` 内の `sched_next_tsk` が両 CPU の `next_tsk_flag` をセットし、CPU 1 の APIC タイマー割り込みで kbd_task が起床される。

---

### c_intr_general

```c
void c_intr_general(W apic, W esp)
```

**概要:** 一般保護例外 (#GP, ベクタ 13) の C 言語ハンドラ。診断情報を出力して停止する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `apic` | `W` | CPU 番号 (APIC ID、0 or 1) |
| `esp` | `W` | save 後の ESP 値 |

**戻り値:** なし (関数は復帰しない、`hlt` で停止)

**処理内容:**

1. `gp_error_code` グローバル変数からエラーコードを取得する (アセンブリ側で保存済み)
2. `proc->stack - 52` から save フレームを取得し、EIP (オフセット 3)、ESP (オフセット 2)、CS (オフセット 0) を読み出す
3. `printk` でエラーコード、EIP、ESP、APIC ID を表示する
4. APIC ID レジスタ (0xFEE00020) の内容を表示する
5. `hlt` ループで停止する

**呼び出し元:** `intr_general` (アセンブリ側エントリから)

**注意点:** `intr_general` はエントリ時にエラーコードを `gp_error_code` に保存してからスタック上のエラーコードを破棄 (`addl $4, %esp`) した後に `save` を呼ぶ。

---

### c_intr_page

```c
void c_intr_page(void)
```

**概要:** ページフォールト (#PF, ベクタ 14) の C 言語ハンドラ。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. CR2 レジスタからフォルトアドレスを読み取る
2. `pf_test_active` フラグが立っている場合 (ページフォルトテスト中):
   - フォルトアドレスを `pf_test_addr` に記録する
   - save フレーム中の EIP を命令長分 (0xA1 なら 5 バイト、0x8B なら 2 バイト) 進めてスキップする
3. `pf_test_active` でない場合:
   - `printk` で診断情報 (フォルトアドレス、EIP) を表示して `hlt` で停止する

**呼び出し元:** `intr_page` (アセンブリ側エントリから)

**注意点:** ページフォルトテスト機能は、ページング設定の検証に使用される。`pf_test_active = 1` に設定してからメモリアクセスを行い、フォルトが発生すれば Supervisor 保護が正しく機能していることを確認できる。

---

### c_intr_irq2 - c_intr_irq5

```c
void c_intr_irq2(void)
void c_intr_irq3(void)
void c_intr_irq4(void)
void c_intr_irq5(void)
```

**概要:** IRQ2-5 のハンドラ。デバッグ用にメッセージを表示し、EOI を送信する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:** `printk()` で IRQ 番号を表示し、`i8259_reenable()` を呼び出す。

**呼び出し元:** 各 `intr_irq*` (アセンブリ側エントリ)

**注意点:** 通常のシステム動作では呼ばれない。

---

### c_intr_irq6

```c
void c_intr_irq6(void)
```

**概要:** フロッピーディスク割り込み (IRQ6) のハンドラ。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. `printk()` で IRQ6 メッセージを表示
2. `fdc_intr()` を呼び出す (FDC 割り込み処理)
3. `i8259_reenable()` で EOI を送信

**呼び出し元:** `intr_irq6` (アセンブリ側エントリ)

**注意点:** なし。

---

### c_intr_irq7

```c
void c_intr_irq7(void)
```

**概要:** IRQ7 (プリンタ/スプリアス割り込み) のハンドラ。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. PIC の ISR (In-Service Register) を読み取る (`outb(0x0b)` + `inb`)
2. ISR の bit 7 が 0 の場合はスプリアス割り込みとして無視 (EOI なしで return)
3. bit 7 が 1 の場合は `"7"` を表示し、`i8259_reenable()` を呼び出す

**呼び出し元:** `intr_irq7` (アセンブリ側エントリ)

**注意点:** IRQ7 のスプリアス割り込みは PIC のハードウェア特性で発生する。ISR を確認して真の割り込みかどうかを判定する。

---

### c_intr_irq8 - c_intr_irq15

```c
void c_intr_irq8(void)
void c_intr_irq9(void)
void c_intr_irq10(void)
void c_intr_irq11(void)
void c_intr_irq12(void)
void c_intr_irq13(void)
void c_intr_irq14(void)
void c_intr_irq15(void)
```

**概要:** IRQ8-15 (スレーブ PIC) のハンドラ。デバッグ用にメッセージを表示し、EOI を送信する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:** `printk()` で IRQ 番号を表示し、`i8259_reenable()` を呼び出す。

**呼び出し元:** 各 `intr_irq*` (アセンブリ側エントリ)

**注意点:** IRQ9 のハンドラは `"KEY\n"` を出力する (歴史的な理由と思われる)。

---

### c_intr_smp_timer0

```c
void c_intr_smp_timer0(void)
```

**概要:** CPU 0 の APIC タイマー割り込みハンドラ。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

`smp_eoi()` で APIC の EOI を送信する。タイマー処理 (`timer_intr`) は呼ばない。

**呼び出し元:** `intr_smp_timer0` (アセンブリ側エントリ)

**注意点:** CPU 0 は PIT (IRQ0) でタイマー割り込みを処理するため、APIC タイマーでは EOI のみ行う。APIC タイマーは CPU 0 ではタスクスイッチのチェックポイントとしてのみ機能する。

---

### c_intr_smp_timer1

```c
void c_intr_smp_timer1(void)
```

**概要:** CPU 1 の APIC タイマー割り込みハンドラ。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. `smp_eoi()` で APIC の EOI を送信

**呼び出し元:** `intr_smp_timer1` (アセンブリ側エントリ)

**注意点:** `timer_intr` / `sched_timeout` は呼ばない。タイムアウトキューの delta 減算は CPU 0 の PIT (IRQ0) だけが行う。APIC タイマーは `intr_leave` でのタスクスイッチ契機を提供するのみ。

---

### irq_mask_on

```c
void irq_mask_on(int n)
```

**概要:** 指定した IRQ をマスク (無効化) する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `n` | `int` | マスクビット。下位 8 ビット = マスター PIC、上位 8 ビット (`n >> 8`) = スレーブ PIC |

**戻り値:** なし (`void`)

**処理内容:**

1. `n & 0xff` が非ゼロの場合、マスター PIC のデータポートを読み取り、指定ビットを OR してマスク設定
2. `(n >> 8) & 0xff` が非ゼロの場合、スレーブ PIC のデータポートに同様の処理

**呼び出し元:** `irq_enter()`

**注意点:** ビットマスクの各ビットが対応する IRQ に対応 (bit 0 = IRQ0, bit 1 = IRQ1, ...)。

---

### irq_mask_off

```c
void irq_mask_off(int n)
```

**概要:** 指定した IRQ のマスクを解除 (有効化) する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `n` | `int` | アンマスクビット。下位 8 ビット = マスター PIC、上位 8 ビット = スレーブ PIC |

**戻り値:** なし (`void`)

**処理内容:**

1. `n & 0xff` が非ゼロの場合、マスター PIC のデータポートを読み取り、ビットを反転して AND でマスク解除
2. `(n >> 8) & 0xff` が非ゼロの場合、スレーブ PIC のデータポートに同様の処理

**呼び出し元:** `irq_exit()`

**注意点:** `n` のビットを XOR で反転してから AND を取るため、指定したビットのみがクリアされる。

---

### irq_enter

```c
void irq_enter(void)
```

**概要:** IRQ ハンドラの前処理。タイマー割り込みを一時的に無効化する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. `get_apic_index()` で現在の CPU インデックスを取得
2. `timer_return[apic] = 1` でタイマー割り込み抑制フラグをセット
3. `irq_mask_on(0xfffe)` で IRQ1-7 をマスク (IRQ0 以外のマスター PIC IRQ を無効化)

**呼び出し元:** キーボード IRQ 処理の前処理として使用

**注意点:** `0xfffe` は bit 0 (IRQ0) 以外のマスター PIC 全 IRQ をマスクする。

---

### irq_exit

```c
void irq_exit(void)
```

**概要:** IRQ ハンドラの後処理。タイマー割り込みを再有効化する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. `get_apic_index()` で現在の CPU インデックスを取得
2. `timer_return[apic] = 0` でタイマー割り込み抑制フラグをクリア
3. `irq_mask_off(0xfffe)` でマスクを解除

**呼び出し元:** キーボード IRQ 処理の後処理として使用

**注意点:** `irq_enter()` と対で使用する。

---

### stack_adjust

```c
void stack_adjust(W apic, void (*func)(), TEXPTN texptn, VP_INT exinf)
```

**概要:** タスク例外ハンドラのスタックフレームを構築する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `apic` | `W` | APIC インデックス (未使用、`get_apic_index()` で再取得) |
| `func` | `void (*)()` | 例外ハンドラ関数 |
| `texptn` | `TEXPTN` | タスク例外パターン |
| `exinf` | `VP_INT` | 拡張情報 |

**戻り値:** なし (`void`)

**処理内容:**

1. `get_apic_index()` で CPU インデックスを取得
2. `current_proc[idx]` からプロセス構造体を取得
3. プロセスの `reg[ESP]` からスタックポインタを取得
4. 現在のレジスタ値 (EIP, EAX, EBX, ECX, EDX, ESI, EDI, EFLAGS) をスタックにプッシュ
5. `exinf`, `texptn`, `user_restore` のアドレスをスタックにプッシュ
6. `reg[ESP]` と `reg[EIP]` を更新 (EIP は `func` のアドレスに設定)

**呼び出し元:** タスク例外処理機構

**注意点:**
- `user_restore` は例外ハンドラからの復帰時にレジスタを復元するアセンブリルーチン。
- 引数 `apic` は使用されず、内部で `get_apic_index()` を再呼び出ししている。

---

### sched_next_tsk_check

```c
int sched_next_tsk_check(int apic)
```

**概要:** タスク切り替えが必要かチェックし、必要なら実行する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `apic` | `int` | CPU インデックス (0 または 1) |

**戻り値:** `int` -- タスク切り替えが発生した場合 1、それ以外は 0。

**処理内容:**

1. `next_tsk_flag[apic]` が非ゼロか確認
2. 非ゼロの場合、`sched_do_next_tsk(apic)` を呼び出してスケジューリングを実行
3. `next_tsk_flag[apic]` をクリア
4. `current_proc[apic]` が変更されたか (旧プロセスと異なるか) を確認
5. 変更された場合は 1 を返す (呼び出し元でコンテキストスイッチを行う)

**呼び出し元:** `intr.s` (割り込みハンドラの `restore` マクロから)

**注意点:**
- この関数の戻り値により、`intr.s` の `restore` マクロが別プロセスの `save` フレームから復帰するか、同じプロセスに戻るかを決定する。
- `next_tsk_flag` は `sched_next_tsk()` によってセットされる。

## 補足

### ヘルパー関数

#### get_apic_index

```c
static int get_apic_index(void)
```

APIC ID レジスタ (`0xFEE00020`) を読み取り、CPU インデックス (0 または 1) を返す。
APIC ID の上位 8 ビット (bit 24-31) を取得し、0 以外なら 1 を返す。

### 割り込みゲート vs トラップゲート

- 割り込みゲート (`GT_INTR` = 0x8e): ハンドラ進入時に EFLAGS の IF をクリア (割り込み禁止)
- トラップゲート (`GT_TRAP` = 0x8f): IF をクリアしない

本カーネルでは、例外はトラップゲート、ハードウェア IRQ とシステムコールは割り込みゲートを使用する。

### IRQ ベクタの配置

マスター PIC の IRQ0-7 を 0x80-0x87、スレーブ PIC の IRQ8-15 を 0x90-0x97 に
配置している。標準的な PC/AT の割り当て (0x20-0x2F) とは異なるが、CPU 例外 (0-31) との
衝突を避けるための設計。
