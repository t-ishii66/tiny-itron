# smp.c / smp.h / smpP.h

対象ファイル: `i386/smp.c`, `i386/smp.h`, `i386/smpP.h`

## 概要

SMP (Symmetric Multi-Processing) サポートモジュール。2 CPU 構成での
Local APIC の初期化、スピンロック、APIC タイマー設定、AP (Application Processor)
起動シーケンスを提供する。

本カーネルの SMP アーキテクチャは以下の特徴を持つ:
- I/O APIC を使用しない (PIC のみ、外部 IRQ は CPU 0 のみに配送)
- Local APIC は CPU 識別 (APIC ID)、APIC タイマー、EOI に使用
- スピンロックは `xchgl` ベースで Ring 3 からも呼び出し可能

## 定数・マクロ

### APIC レジスタアドレス (smpP.h)

Local APIC のベースアドレスは `0xFEE00000` で固定。

| 定数 | 値 | 説明 |
|------|------|------|
| `APIC_BASE` | `0xFEE00000` | Local APIC ベースアドレス |
| `APIC_ID` | `APIC_BASE + 0x020` | APIC ID レジスタ |
| `APIC_VERSION` | `APIC_BASE + 0x030` | APIC バージョンレジスタ |
| `APIC_EOI` | `APIC_BASE + 0x0B0` | EOI (End of Interrupt) レジスタ |
| `APIC_SVR` | `APIC_BASE + 0x0F0` | スプリアス割り込みベクタレジスタ |
| `APIC_ICR_LOW` | `APIC_BASE + 0x300` | ICR (Interrupt Command Register) 下位 |
| `APIC_ICR_HIGH` | `APIC_BASE + 0x310` | ICR 上位 |
| `APIC_LVT_TIMER` | `APIC_BASE + 0x320` | LVT タイマーレジスタ |
| `APIC_TIMER_INIT_COUNT` | `APIC_BASE + 0x380` | タイマー初期カウント |
| `APIC_TIMER_DIV` | `APIC_BASE + 0x3E0` | タイマー分周レジスタ |

### APIC 制御フラグ (smpP.h)

| 定数 | 値 | 説明 |
|------|------|------|
| `APIC_ENABLED` | `0x100` | APIC 有効化ビット (SVR の bit 8) |
| `APIC_TIMER_DIV_16` | `0x03` | タイマー分周値 = 16 |
| `APIC_TIMER_PERIODIC` | `0x20000` | 周期モード (LVT タイマーの bit 17) |

### IPI (Inter-Processor Interrupt) フラグ (smpP.h)

| 定数 | 値 | 説明 |
|------|------|------|
| `ICR_INIT` | `0x00000500` | INIT IPI |
| `ICR_STARTUP` | `0x00000600` | Startup IPI (SIPI) |
| `ICR_LEVEL_ASSERT` | `0x00004000` | レベルアサート |
| `ICR_LEVEL_DEASSERT` | `0x00000000` | レベルディアサート |
| `ICR_ALL_EXCLUDING_SELF` | `0x000C0000` | 自分以外の全 CPU に配送 |

### タイマー・割り込みベクタ (smpP.h)

| 定数 | 値 | 説明 |
|------|------|------|
| `MAX_TIMER_COUNT` | `0x00080000` | APIC タイマー初期カウント値 (QEMU で約 100 Hz) |
| `VECT_APIC` | `0x98` | APIC スプリアス割り込みベクタ |
| `VECT_SMP_TIMER0` | `0x9a` | CPU 0 APIC タイマー割り込みベクタ |
| `VECT_SMP_TIMER1` | `0x9b` | CPU 1 APIC タイマー割り込みベクタ |

## 構造体・型

なし。

## グローバル変数

### cpu_second

```c
volatile int cpu_second = 0;
```

BSP-AP 間のハンドシェイク変数。AP の初期化完了を BSP に通知するために使用。
- 初期値 0: AP 未起動
- AP が `smp_ap_init()` の最後で 1 に設定
- BSP が `smp_init()` 内で `while (!cpu_second)` でポーリング待機

`volatile` 宣言により、コンパイラの最適化でポーリングループが除去されることを防ぐ。

## 関数リファレンス

### smp_lock

```c
void smp_lock(unsigned long *p)
```

**概要:** `xchgl` ベースのスピンロックを獲得する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `p` | `unsigned long*` | ロック変数へのポインタ (0=未ロック, 1=ロック中) |

**戻り値:** なし (`void`)

**処理内容:**

1. `cxchg(p, 1)` で `*p` を 1 にアトミックに交換し、旧値を取得
2. 旧値が 0 (未ロック) なら関数から復帰 (ロック獲得成功)
3. 旧値が 1 (他者がロック中) なら `pause` 命令を実行してスピンウェイト
4. 1 に戻ってリトライ

**呼び出し元:** カーネル内のクリティカルセクション保護

**注意点:**
- `cxchg` はアセンブリマクロで `xchgl` 命令を実行する。`xchgl` は暗黙的に LOCK プレフィックスを持つため、明示的な `lock` は不要。
- `pause` 命令はスピンループのパフォーマンスを改善し、ハイパースレッディング環境での電力消費を抑える。
- `cli`/`sti` を使用しないため、Ring 3 (ユーザーモード) からも安全に呼び出せる。

---

### smp_unlock

```c
void smp_unlock(unsigned long *p)
```

**概要:** スピンロックを解放する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `p` | `unsigned long*` | ロック変数へのポインタ |

**戻り値:** なし (`void`)

**処理内容:**

`cxchg(p, 0)` で `*p` を 0 (未ロック) にアトミックに設定する。

**呼び出し元:** `smp_lock()` と対で使用

**注意点:** `xchgl` を使用するため、メモリバリアとしても機能する。

---

### smp_eoi

```c
void smp_eoi(void)
```

**概要:** Local APIC に EOI (End of Interrupt) を送信する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

APIC EOI レジスタ (`0xFEE000B0`) に 0 を書き込む。

**呼び出し元:** `c_intr_smp_timer0()`, `c_intr_smp_timer1()`

**注意点:** APIC 割り込み (APIC タイマーなど) の処理後に必ず呼び出す必要がある。EOI を送信しないと次の割り込みが配送されない。

---

### cpu_lock

```c
void cpu_lock(void)
```

**概要:** 割り込みを禁止する (CLI ラッパー)。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

`ccli()` を呼び出して `cli` 命令を実行する。

**呼び出し元:** syscall ハンドラ内 (Ring 0 コンテキスト)

**注意点:** Ring 0 でのみ使用可能。Ring 3 から呼び出すと #GP 例外が発生する。

---

### cpu_unlock

```c
void cpu_unlock(void)
```

**概要:** 割り込みを許可する (STI ラッパー)。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

`csti()` を呼び出して `sti` 命令を実行する。

**呼び出し元:** `cpu_lock()` と対で使用

**注意点:** Ring 0 でのみ使用可能。

---

### apic_write

```c
static void apic_write(unsigned long reg, unsigned long val)
```

**概要:** Local APIC レジスタに値を書き込む。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `reg` | `unsigned long` | レジスタの物理アドレス (`APIC_BASE + offset`) |
| `val` | `unsigned long` | 書き込む値 |

**戻り値:** なし (`void`)

**処理内容:**

`reg` を `volatile unsigned long*` にキャストして MMIO 書き込みを行う。

**呼び出し元:** `smp_init()`, `smp_ap_init()`

**注意点:** `volatile` によりコンパイラの最適化を防止し、確実に MMIO アクセスを行う。

---

### apic_read

```c
static unsigned long apic_read(unsigned long reg)
```

**概要:** Local APIC レジスタから値を読み取る。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `reg` | `unsigned long` | レジスタの物理アドレス |

**戻り値:** `unsigned long` -- レジスタの値。

**処理内容:**

`reg` を `volatile unsigned long*` にキャストして MMIO 読み取りを行う。

**呼び出し元:** `smp_init()`, `smp_ap_init()`

**注意点:** なし。

---

### delay_loop

```c
static void delay_loop(int count)
```

**概要:** 単純なビジーウェイトループによる遅延。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `count` | `int` | ループ回数 |

**戻り値:** なし (`void`)

**処理内容:**

`volatile int` を使ったカウントダウンループで指定回数だけ空回しする。

**呼び出し元:** `smp_init()` (INIT IPI/SIPI 送信後の待機)

**注意点:** `volatile` によりコンパイラがループを最適化で除去しないようにしている。実際の遅延時間は CPU 速度に依存する。

---

### smp_init

```c
void smp_init(void)
```

**概要:** BSP (CPU 0) の SMP 初期化。APIC 設定、AP への SIPI 送信、タスク起動を行う。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. **BSP の Local APIC 有効化:**
   - SVR (Spurious Vector Register) の `APIC_ENABLED` ビットをセット
   - スプリアスベクタを `VECT_APIC` (0x98) に設定

2. **BSP の APIC タイマー設定:**
   - 分周値を `APIC_TIMER_DIV_16` (16 分周) に設定
   - 周期モード (`APIC_TIMER_PERIODIC`) で `VECT_SMP_TIMER0` (0x9a) ベクタを設定
   - 初期カウントを `MAX_TIMER_COUNT` (0x80000) に設定

3. **IDT エントリ登録:**
   - `VECT_SMP_TIMER0` → `intr_smp_timer0`
   - `VECT_SMP_TIMER1` → `intr_smp_timer1`
   - `VECT_APIC` → `intr_default` (スプリアス用)

4. **INIT IPI 送信:**
   - `ICR_ALL_EXCLUDING_SELF | ICR_INIT | ICR_LEVEL_ASSERT` で INIT アサート
   - `delay_loop(100000)` で待機
   - `ICR_LEVEL_DEASSERT` で INIT ディアサート
   - `delay_loop(100000)` で待機

5. **SIPI (Startup IPI) 送信 (2回):**
   - `ICR_ALL_EXCLUDING_SELF | ICR_STARTUP | 0x03` でベクタ 0x03 の SIPI を送信
   - ベクタ 0x03 は物理アドレス 0x3000 を意味 (`start.s` が配置されている)
   - Intel の推奨に従い 2 回送信

6. **AP ハンドシェイク待機:**
   - `while (!cpu_second)` でポーリング

7. **タイマー・キーボード起動:**
   - `timer_start()` で PIT タイマー (IRQ0) を開始
   - `key_start()` でキーボード (IRQ1) を有効化

8. **タスク起動:**
   - `start_first_task()` でハードウェア TSS スイッチにより Ring 3 の `first_task` に遷移

**呼び出し元:** `main()` (BSP パス)

**注意点:**
- `start_first_task()` は戻らない。`ljmp SEL_TSS0` でハードウェアタスクスイッチを行い、Ring 3 の `first_task()` に直接遷移する。
- SIPI のベクタ値 `0x03` は「AP の開始物理アドレス / 0x1000」を意味する。つまり `0x03 * 0x1000 = 0x3000`。
- タイマーとキーボードの開始は AP のハンドシェイク完了後に行う。これにより、AP の初期化が完了する前に割り込みが発生することを防ぐ。

---

### smp_ap_init

```c
void smp_ap_init(void)
```

**概要:** AP (CPU 1) の初期化。TSS ロード、APIC 設定、タスク起動を行う。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. **TR レジスタロード:**
   - `cltr(SEL_TSS_DUMMY1)` で CPU 1 用ダミー TSS をロード

2. **Local APIC 有効化:**
   - SVR の `APIC_ENABLED` ビットをセット
   - スプリアスベクタを `VECT_APIC` (0x98) に設定

3. **APIC タイマー設定:**
   - 分周値を `APIC_TIMER_DIV_16` に設定
   - 周期モードで `VECT_SMP_TIMER1` (0x9b) ベクタを設定
   - 初期カウントを `MAX_TIMER_COUNT` に設定

4. **ハンドシェイク:**
   - `cpu_second = 1` で BSP に初期化完了を通知

5. **タスク起動:**
   - `start_second_task()` でハードウェア TSS スイッチにより Ring 3 の `second_task` に遷移

**呼び出し元:** `main()` (AP パス)

**注意点:**
- `start_second_task()` は戻らない。
- AP は CPU 1 専用の APIC タイマーベクタ (`VECT_SMP_TIMER1` = 0x9b) を使用する。これにより CPU 0 のタイマー (`VECT_SMP_TIMER0`) とは別のハンドラが呼ばれる。
- IDT は共有メモリ上にあるため、BSP が既に `VECT_SMP_TIMER1` のエントリを登録済み。AP 側で再登録する必要はない。

## 補足

### AP 起動シーケンスの全体像

```
BSP (CPU 0):                          AP (CPU 1):
  smp_init()
    APIC 有効化
    APIC タイマー設定
    IDT 登録
    INIT IPI 送信 ─────────────────→  リセット
    delay
    SIPI 送信 (0x03) ──────────────→  0x3000 (start.s) で実行開始
    delay                               Protected Mode 遷移
    SIPI 送信 (2回目) ─────────────→  (既に実行中なら無視)
    while (!cpu_second) 待機             run.s → main() → smp_ap_init()
                                          TR ロード
                                          APIC 有効化
                                          タイマー設定
    cpu_second=1 検出 ←────────────   cpu_second = 1
    timer_start()                         start_second_task()
    key_start()                           → Ring 3 second_task()
    start_first_task()
    → Ring 3 first_task()
```

### スピンロックの使い分け

| ロック | 命令 | Ring | 用途 |
|--------|------|------|------|
| `smp_lock`/`smp_unlock` | `xchgl` | 0, 3 | CPU 間排他 (ユーザーモードから呼び出し可能) |
| `cpu_lock`/`cpu_unlock` | `cli`/`sti` | 0 のみ | CPU 内割り込み禁止 (syscall ハンドラ内で使用) |

### APIC タイマーの周期

`MAX_TIMER_COUNT = 0x80000` (524,288)、分周 16。QEMU 環境で約 100 Hz (10ms 周期)。
この値は QEMU の TCG モードで CPU 1 に適切なタイムスライスを与えるために調整されている。
