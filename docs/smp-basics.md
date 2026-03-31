# SMP (対称型マルチプロセッシング) の基礎

本ドキュメントは、tiny-itron の SMP (2-CPU) 実装を読み解くために必要な
ハードウェアとソフトウェアの基礎知識をまとめたものである。

---

## 目次

1. [SMP とは何か](#1-smp-とは何か)
2. [CPU の識別: BSP と AP](#2-cpu-の識別-bsp-と-ap)
3. [Local APIC: CPU ごとの割り込みコントローラ](#3-local-apic-cpu-ごとの割り込みコントローラ)
4. [AP の起動シーケンス (INIT-SIPI プロトコル)](#4-ap-の起動シーケンス-init-sipi-プロトコル)
5. [メモリレイアウト: CPU ごとのスタック](#5-メモリレイアウト-cpu-ごとのスタック)
6. [排他制御: なぜロックが必要か](#6-排他制御-なぜロックが必要か)
7. [スピンロック: xchg 命令によるアトミック操作](#7-スピンロック-xchg-命令によるアトミック操作)
8. [Per-CPU データ: CPU ごとの状態管理](#8-per-cpu-データ-cpu-ごとの状態管理)
9. [SMP スケジューリング: CPU アフィニティ](#9-smp-スケジューリング-cpu-アフィニティ)
10. [tiny-itron の起動を追いかける](#10-tiny-itron-の起動を追いかける)
11. [まとめ: 重要な原則](#11-まとめ-重要な原則)

---

## 1. SMP とは何か

**SMP (Symmetric Multi-Processing)** とは、
2 つ以上の CPU が同じメモリ空間を共有し、対等な立場で動作するアーキテクチャである。

```
        ┌───────────┐  ┌───────────┐
        │   CPU 0   │  │   CPU 1   │
        │  (BSP)    │  │   (AP)    │
        └─────┬─────┘  └─────┬─────┘
              │               │
        ══════╧═══════════════╧══════  共有メモリバス
              │
        ┌─────┴─────────────────────┐
        │         メインメモリ         │
        │  コード、データ、スタック     │
        └───────────────────────────┘
```

**ポイント:**
- 両方の CPU が **同じ命令セット** を実行できる (「対称」の意味)
- 両方の CPU が **同じメモリ** を読み書きできる
- 各 CPU は **独立に** 命令を実行している (本当に同時に動く)
- OS が各 CPU に仕事 (タスク) を割り当てる

### シングルプロセッサとの最大の違い

シングルプロセッサでは「マルチタスク」と言っても、実際にはタイマー割り込みで
高速に切り替えているだけで、**ある瞬間に実行されている命令は 1 つ** だけだった。

SMP では **本当に 2 つの命令が同時に実行される**。
これが後述するロックの問題を引き起こす。

---

## 2. CPU の識別: BSP と AP

PC が電源を入れると、複数の CPU のうち **1 つだけ** が最初に動き出す。
残りの CPU は停止した状態で待機している。

| 用語 | 正式名称 | 役割 |
|------|----------|------|
| **BSP** | Bootstrap Processor | 最初に起動する CPU。OS の初期化を行う |
| **AP**  | Application Processor | BSP が明示的に起動するまで停止している |

tiny-itron では:
- **CPU 0 = BSP** — ハードウェア初期化、カーネル初期化、タスク生成を全て行う
- **CPU 1 = AP** — BSP から起動信号を受け取った後、自分用のタスクを実行する

### 自分がどの CPU か知る方法

各 CPU には **APIC ID** という固有の番号が割り当てられている。
これは Local APIC というハードウェアのレジスタから読み取れる。

```c
/* i386/interrupt.c — APIC ID の読み取り */
int get_apic_index(void)
{
    volatile unsigned long *apic_id = (volatile unsigned long *)0xFEE00020;
    unsigned long id = (*apic_id >> 24) & 0xFF;
    return (id == 0) ? 0 : 1;
}
```

- アドレス `0xFEE00020` を読む
- ビット 24〜31 に APIC ID が入っている
- APIC ID が 0 なら CPU 0 (BSP)、それ以外なら CPU 1 (AP)

この関数は割り込みハンドラの中で頻繁に呼ばれる。
「今、どの CPU がこの割り込みを処理しているのか?」を知るためである。

---

## 3. Local APIC: CPU ごとの割り込みコントローラ

### PIC の限界

シングルプロセッサ時代の PC では、**PIC (i8259)** という割り込みコントローラが
キーボードやタイマーの割り込みを CPU に通知していた。
しかし PIC は CPU が 1 つしかない前提で設計されており、
「どの CPU に割り込みを送るか」を選ぶ機能がない。

### Local APIC とは

SMP 対応のために、Intel は **Local APIC (Advanced Programmable Interrupt
Controller)** を各 CPU の内部に組み込んだ。

```
    ┌──────────────────┐    ┌──────────────────┐
    │      CPU 0       │    │      CPU 1       │
    │  ┌────────────┐  │    │  ┌────────────┐  │
    │  │ Local APIC │  │    │  │ Local APIC │  │
    │  │ ID = 0     │  │    │  │ ID = 1     │  │
    │  └──────┬─────┘  │    │  └──────┬─────┘  │
    └─────────┼────────┘    └─────────┼────────┘
              │                       │
     ═════════╧═══════════════════════╧═══  APIC バス
              │
    ┌─────────┴────────┐
    │  PIC (i8259)     │  ← キーボード、PIT タイマー等の外部割り込み
    └──────────────────┘
```

**Local APIC の主な役割:**

1. **CPU の識別** — APIC ID レジスタで自分の番号がわかる
2. **割り込みの受信** — 外部割り込み (PIC 経由) やタイマー割り込みを受け取る
3. **IPI の送受信** — CPU 間でメッセージ (割り込み) を送り合える
4. **EOI (End of Interrupt)** — 割り込み処理完了を APIC に通知する
5. **タイマー** — CPU ごとに独立したタイマーを持っている

### APIC レジスタ (メモリマップド I/O)

Local APIC のレジスタは、物理アドレス `0xFEE00000` から始まるメモリ領域に
マップされている。通常のメモリ読み書き命令でアクセスできる。

```c
/* i386/smpP.h — 主要な APIC レジスタ */
#define APIC_BASE       0xFEE00000

#define APIC_ID         (APIC_BASE + 0x020)  /* CPU 識別番号 */
#define APIC_EOI        (APIC_BASE + 0x0B0)  /* 割り込み完了通知 */
#define APIC_SVR        (APIC_BASE + 0x0F0)  /* APIC 有効化 */
#define APIC_ICR_LOW    (APIC_BASE + 0x300)  /* IPI 送信 (下位) */
#define APIC_ICR_HIGH   (APIC_BASE + 0x310)  /* IPI 送信 (上位) */
#define APIC_LVT_TIMER  (APIC_BASE + 0x320)  /* タイマー設定 */
```

> **メモリマップド I/O とは:** ハードウェアのレジスタを、
> あたかもメモリの特定番地にあるかのようにアクセスする方式。
> C 言語からは、そのアドレスへのポインタ経由で読み書きするだけで
> ハードウェアを制御できる。

### EOI (End of Interrupt)

割り込みを処理し終わったら、APIC に「終わったよ」と通知する必要がある。
これをしないと、次の割り込みが来ない。

```c
/* i386/smp.c */
void smp_eoi(void)
{
    volatile unsigned long *eoi = (volatile unsigned long *)APIC_EOI;
    *eoi = 0;  /* 0 を書き込むだけで EOI */
}
```

tiny-itron では PIC (i8259) と Local APIC を併用している:
- **外部割り込み (キーボード, PIT):** PIC → CPU 0 のみに配送。PIC EOI が必要
- **APIC タイマー割り込み:** 各 CPU の Local APIC が生成。APIC EOI が必要

---

## 4. AP の起動シーケンス (INIT-SIPI プロトコル)

電源投入時、AP は停止している。BSP が AP を起動するには、
Intel が定めた **INIT-SIPI プロトコル** に従って IPI (Inter-Processor Interrupt)
を送る必要がある。

### 手順

```
BSP                                     AP
 │                                       │ (停止中)
 │── INIT IPI ──────────────────────────>│
 │         (AP をリセット状態にする)       │
 │                                       │
 │    ... 10ms 待つ ...                   │
 │                                       │
 │── Startup IPI (vector=0x03) ────────>│
 │         (AP が 0x3000 番地から実行開始)  │
 │                                       │
 │    ... 200us 待つ ...                  │
 │                                       │
 │── Startup IPI (vector=0x03) ────────>│  ← 安全のため 2 回送る
 │                                       │
 │                                       │── リアルモードで起動
 │                                       │── プロテクトモードに遷移
 │                                       │── main() を呼ぶ
 │                                       │
 │<── cpu_second = 1 (メモリ経由) ────── │  ← ハンドシェイク
 │                                       │
 │  (両 CPU が独立に動作開始)              │
```

### IPI の送り方 (ICR レジスタ)

IPI は APIC の **ICR (Interrupt Command Register)** に値を書き込むことで送信される。

```c
/* i386/smp.c — AP 起動シーケンス (BSP が実行) */

/* 1. INIT IPI を全 AP に送信 */
*icr_high = 0;
*icr_low  = ICR_INIT | ICR_LEVEL_ASSERT | ICR_ALL_EXCLUDING_SELF;
/* 少し待つ */
*icr_low  = ICR_INIT | ICR_LEVEL_DEASSERT | ICR_ALL_EXCLUDING_SELF;

/* 2. Startup IPI を 2 回送信 (vector = 0x03 → 物理アドレス 0x3000) */
*icr_low  = ICR_STARTUP | ICR_ALL_EXCLUDING_SELF | 0x03;
/* 少し待つ */
*icr_low  = ICR_STARTUP | ICR_ALL_EXCLUDING_SELF | 0x03;
```

**Startup IPI のベクタ番号:**
ベクタ番号 `0x03` は `0x03 * 0x1000 = 0x3000` を意味する。
AP はこの物理アドレスから **リアルモード** で実行を開始する。

> **リアルモードとは:** x86 CPU が電源投入時に動作するモード。
> 16 ビットで、アドレス空間は 1MB に制限される。
> OS は通常すぐにプロテクトモード (32 ビット) に切り替える。

### ハンドシェイク

BSP は AP が初期化を完了するのを待つ必要がある。
tiny-itron では共有メモリ上の変数 `cpu_second` を使った単純なハンドシェイクを行う。

```c
/* BSP 側 (smp_init) */
while (cpu_second == 0)
    ;  /* AP が準備完了するまでビジーウェイト */

/* AP 側 (smp_ap_init) */
cpu_second = 1;  /* BSP に「準備完了」を通知 */
```

`cpu_second` には `volatile` が付いている。これはコンパイラに対して
「この変数は外部 (他の CPU) から変更される可能性があるので、
レジスタにキャッシュせず毎回メモリから読め」と指示するものである。

---

## 5. メモリレイアウト: CPU ごとのスタック

### なぜ CPU ごとにスタックが必要か

スタックは関数の引数、ローカル変数、リターンアドレスを格納する領域である。
2 つの CPU が同時に関数を呼び出すと、1 つのスタックでは内容が混ざって壊れてしまう。
したがって **CPU ごとに独立したスタック** が必要になる。

```
  メモリアドレス (低い → 高い)
  ─────────────────────────────────────────────

  0x110000 ┬─────────────────── メモリプール (MEM_START)
           │   ...
  0x700000 ┬─────────────────── スタックプール (タスクスタック)
           │   ...
  0x750000 ┬─────────────────── Per-task カーネルスタック (16 タスク × 4KB)
           │   Task 1: 0x750000〜0x751000
           │   Task 2: 0x751000〜0x752000
           │   ...
  0x770000 ┬─────────────────── CPU 1 初期スタック (main() のみ)
           │   ...
  0x7A0000 ┬─────────────────── CPU 0 初期スタック (main() のみ)
```

> **スタックの成長方向:** x86 のスタックは高いアドレスから低いアドレスに向かって
> 伸びる。`0x7A0000` が「スタックの底」であり、push するたびにアドレスが減る。

### Ring 0 と Ring 3

x86 には **特権レベル (Ring)** という仕組みがある。

| Ring | 用途 | できること |
|------|------|-----------|
| Ring 0 | OS カーネル | 全命令が実行可能 (cli, sti, in, out 等) |
| Ring 3 | ユーザーアプリ | 特権命令は実行不可 (#GP 例外が発生) |

ユーザータスク (Ring 3) が syscall や割り込みでカーネル (Ring 0) に入るとき、
CPU は TSS.esp0 に設定されたカーネルスタックに自動的に切り替える。
tiny-itron では **タスクごとに 4KB のカーネルスタック**を持ち、
TSS.esp0 をタスクスイッチ毎に動的に更新する。

```c
/* i386/addr.h — CPU ごとの起動時スタック + per-task カーネルスタック */
#define CPU0_SP         0x7a0000   /* CPU 0 初期スタック (main() のみ) */
#define CPU1_SP         0x770000   /* CPU 1 初期スタック (main() のみ) */
#define KERN_STACK_BASE 0x750000   /* per-task カーネルスタック領域の先頭 */
#define KERN_STACK_SIZE 4096       /* 各タスク 4KB */
/* Task N の頂上 = KERN_STACK_BASE + (N+1) * KERN_STACK_SIZE */
```

タスク起動後、Ring 0 での実行はすべて per-task カーネルスタック上で行われる。
`CPU0_SP`/`CPU1_SP` はカーネル初期化 (`main()`) の間だけ使われ、その後は使用されない。

### run.s での CPU 判定とスタック選択

AP が起動すると、BSP と同じ `run` ラベルに到達する。
ここで `cpu_num` 変数を見て、自分が BSP か AP かを判定し、
適切なスタックを選ぶ。

```asm
# i386/run.s
run:
    # ... セグメントレジスタの設定 ...

    cmpl  $0, cpu_num      # cpu_num == 0 なら BSP
    jne   run_ap

    # BSP: CPU 0 のスタックを使用
    movl  $0x07a0000, %esp
    call  main
    jmp   halt

run_ap:
    # AP: CPU 1 のスタックを使用
    movl  $0x0770000, %esp
    call  main
    jmp   halt
```

---

## 6. 排他制御: なぜロックが必要か

### 問題の具体例

以下の C コードを 2 つの CPU が同時に実行する場面を考える。

```c
int count = 0;

void increment(void) {
    count = count + 1;
}
```

一見問題なさそうだが、`count = count + 1` は CPU レベルでは **3 つの操作** になる:

```
1. メモリから count の値を読む (LOAD)
2. 値に 1 を足す (ADD)
3. 結果をメモリに書き戻す (STORE)
```

2 つの CPU が同時にこれを実行すると:

```
CPU 0                    CPU 1                     count の値
─────────────────────────────────────────────────────────────
LOAD count → 0                                     0
                         LOAD count → 0            0
ADD  0 + 1 = 1                                     0
                         ADD  0 + 1 = 1            0
STORE 1 → count                                    1
                         STORE 1 → count           1  ← 期待値は 2!
```

**2 回インクリメントしたのに、結果は 1。**
これが **レースコンディション (競合状態)** である。

### 解決策: ロック

レースコンディションを防ぐには、「ある操作を実行中は、
他の CPU に同じデータを触らせない」仕組みが必要である。
これを **排他制御 (mutual exclusion)** と呼び、
そのための仕組みを **ロック** と呼ぶ。

```
CPU 0                    CPU 1                     count の値
─────────────────────────────────────────────────────────────
ロック獲得 ✓                                        0
LOAD count → 0          ロック獲得 → 失敗 (待機)   0
ADD  0 + 1 = 1                    (スピン...)      0
STORE 1 → count                   (スピン...)      1
ロック解放               ロック獲得 ✓               1
                         LOAD count → 1            1
                         ADD  1 + 1 = 2            1
                         STORE 2 → count           2  ← 正しい!
                         ロック解放
```

---

## 7. スピンロック: xchg 命令によるアトミック操作

### アトミック操作とは

ロックを実装するには、「メモリの読み取りと書き込みを **分割できない 1 つの操作**
として行う」命令が必要である。
これを **アトミック (atomic = 不可分) 操作** と呼ぶ。

x86 には `xchg` (exchange) というアトミック命令がある。

```
xchgl %eax, (%ebx)
```

この命令は以下の 2 つを **同時に、他の CPU が割り込めない形で** 実行する:
1. `(%ebx)` が指すメモリの値を `%eax` に読み込む
2. `%eax` の元の値を `(%ebx)` が指すメモリに書き込む

つまり「レジスタとメモリの値を入れ替える」操作であり、
Intel のマニュアルで `xchg` は **暗黙的にバスロック** がかかると定められている。

### tiny-itron のスピンロック実装

```c
/* i386/smp.c */
void smp_lock(unsigned long *p)
{
    while (cxchg(p, 1))       /* *p と 1 を交換。旧値が 0 なら獲得成功 */
        __asm__ volatile("pause");  /* CPU を少し休ませる (後述) */
}

void smp_unlock(unsigned long *p)
{
    cxchg(p, 0);              /* *p に 0 を書き込み → ロック解放 */
}
```

`cxchg` は klib.s で定義されたアセンブリ関数:

```asm
# i386/klib.s — アトミック交換
cxchg:
    pushl   %ebp
    movl    %esp, %ebp
    pushl   %ebx
    movl    8(%ebp), %ebx      # 第 1 引数: ロック変数のアドレス
    movl    12(%ebp), %eax     # 第 2 引数: 書き込みたい値
    xchgl   (%ebx), %eax       # アトミックに交換
    popl    %ebx
    popl    %ebp
    ret                         # 戻り値 (EAX) = 交換前の値
```

### ロック獲得の動作原理

ロック変数は `0` (空き) か `1` (使用中) の値を取る。

```
ロック獲得: cxchg(p, 1)
  *p が 0 (空き) の場合:
    → *p に 1 を書く (ロック獲得)
    → 旧値 0 が返る → while ループ脱出 → 成功!

  *p が 1 (他の CPU が保持中) の場合:
    → *p に 1 を書く (変わらず)
    → 旧値 1 が返る → while ループ継続 → スピン (繰り返し試行)

ロック解放: cxchg(p, 0)
  → *p に 0 を書く → 他の CPU が獲得可能になる
```

この方式を **スピンロック** と呼ぶ。獲得できるまでループ (spin) し続けるためである。

### pause 命令

スピンループの中に `pause` 命令を入れている理由:

- Hyper-Threading (1 つの CPU コアで 2 スレッドを動かす技術) の環境で、
  スピンしているスレッドが CPU リソースを無駄に消費するのを軽減する
- CPU の投機実行パイプラインをクリアし、ロック解放の検出を早める
- `pause` がない CPU (Pentium 3 以前) では NOP として扱われるので無害

### なぜ cli/sti ではダメなのか

シングルプロセッサでは、`cli` (割り込み禁止) と `sti` (割り込み許可) で
排他制御ができた。割り込みを禁止すれば、他のコードに CPU を奪われないからである。

SMP では **cli/sti は自分の CPU の割り込みしか制御できない**。
他の CPU は cli とは無関係に動き続けるため、排他制御にならない。

さらに tiny-itron では、ロックがユーザータスク (Ring 3) から呼ばれる場合がある
(例: `video.c` の画面出力)。Ring 3 では `cli`/`sti` は特権命令なので
使えない (実行すると #GP 例外が発生する)。

`xchg` ベースのスピンロックなら、特権レベルに関係なく動作する。

---

## 8. Per-CPU データ: CPU ごとの状態管理

SMP カーネルでは、「現在どのタスクを実行中か」「割り込みのネスト回数」等の
情報を **CPU ごとに独立して** 管理する必要がある。
CPU 0 と CPU 1 は別のタスクを同時に実行しているからである。

### tiny-itron の Per-CPU 変数

tiny-itron で実際に使われている per-CPU 変数は以下の 4 つである。

```c
/* i386/kernelval.c */
proc_t* current_proc[MAX_CPU];   /* 各 CPU が今実行中のプロセス */
ID      c_tskid[MAX_CPU];       /* 各 CPU が今実行中のタスク ID */

/* kernel/sched.c */
INT     next_tsk_flag[2];       /* 各 CPU のリスケジュール要求フラグ */
```

```asm
# i386/intr.s (アセンブリで個別変数として定義)
k_nest0:  .long 0               # CPU 0 の割り込みネスト深度
k_nest1:  .long 0               # CPU 1 の割り込みネスト深度
```

`k_nest` はアセンブリ (`intr_enter`/`intr_leave`) が毎回の割り込みで
高速にアクセスする必要があるため、C 配列ではなく個別変数として定義されている。

使い方:

```c
int cpu = get_apic_index();           /* 0 or 1 */
current_proc[cpu] = &proc[task_id];   /* 自分の CPU のタスクを更新 */
c_tskid[cpu] = task_id;
```

### per-CPU であるべき基準

ある変数が per-CPU であるべきかの判断基準は
**「2 つの CPU が同時に独立して読み書きする可能性があるか」** である。

| 変数 | per-CPU の理由 |
|------|---------------|
| `current_proc[]` | 各 CPU が別タスクを同時に実行する |
| `c_tskid[]` | 同上 |
| `next_tsk_flag[]` | CPU 0 の IRQ0 が CPU 1 のフラグをセットし、CPU 1 の APIC タイマーがそれを読む |
| `k_nest0/1` | 各 CPU で割り込みが独立にネストする |

一方、システムティック (`timer_ticks`) は per-CPU ではなくグローバル 1 個で正しい。
PIT (IRQ0) は CPU 0 のみに配送される唯一のシステムクロック源であり、
CPU 1 の APIC タイマーはプリエンプション契機のみに使用されるためである。

### 割り込みハンドラでの CPU 判定

割り込みが発生したとき、ハンドラは最初に「自分はどの CPU で動いているか」を
調べる。これにより正しい per-CPU データにアクセスできる。

```asm
# i386/intr.s — intr_enter (SAVE_ALL 直後に呼ばれる)
intr_enter:
    movl   0xFEE00020, %eax    # APIC ID レジスタ (MMIO) を読む
    shrl   $24, %eax           # ビット 24-31 を抽出 → EAX = 0 or 1
    testl  %eax, %eax
    jnz    1f                  # CPU 1 なら分岐
    incl   k_nest0             # CPU 0: ネストカウンタ++
    ret
1:
    incl   k_nest1             # CPU 1: ネストカウンタ++
    ret
```

`intr_enter` は APIC ID で CPU を判定し、per-CPU のネストカウンタをインクリメントする。
対応する `intr_leave` がデクリメントし、k_nest が 0 に戻ったときにタスクスイッチを判定する。
per-CPU の `current_proc[]` ポインタは `intr_leave` が参照する
(`current_proc+4` は `current_proc[1]` — ポインタ 4 バイトで配列の 2 番目)。

### syscall は必ず呼び出し元の CPU が処理する

SMP 環境では「syscall はどの CPU が処理するのか」を正しく理解する必要がある。

外部割り込み (IRQ) は PIC/APIC のルーティングにより **別の CPU** に配送される可能性がある。
たとえば IRQ1 (キーボード) は PIC 経由で CPU 0 に配送される。

一方、**syscall (`INT 0x99`) はソフトウェア割り込み**であり、
CPU がその命令を実行した時点で即座に IDT を参照してハンドラに飛ぶ。
「どの CPU に配送するか」という判断は存在しない。
これは `INT` 命令の根本的な動作であり、SMP でも変わらない。

```
外部割り込み (IRQ):  デバイス → PIC → CPU 0 に配送  (ルーティングあり)
ソフトウェア割り込み: CPU 1 で INT 0x99 実行 → CPU 1 が処理  (常に自 CPU)
```

したがって:
- CPU 1 のタスクが `cre_tsk()` を呼ぶと、CPU 1 上で `c_intr_syscall` → `sys_cre_tsk` が実行される
- CPU 0 のタスクが `sig_sem()` を呼ぶと、CPU 0 上で処理される
- `get_apic_index()` は syscall ハンドラ内で「呼び出し元の CPU」を正しく返す

tiny-itron ではこの性質を利用して、syscall ハンドラ内の `apic` 引数
(= `get_apic_index()`) をカーネル全体で活用している。

---

## 9. SMP スケジューリング: CPU アフィニティ

### CPU アフィニティとは

SMP では、各タスクを「どの CPU で実行するか」を決める必要がある。
この割り当てを **CPU アフィニティ (affinity = 親和性)** と呼ぶ。

tiny-itron では、タスク作成時に **作成元の CPU と同じ CPU** にアフィニティが設定される:

```c
/* i386/proc.c — タスク作成時の CPU アフィニティ設定 */
proc[id].cpu = get_apic_index();  /* 作成した CPU に割り当て */
```

一度設定されたアフィニティは変更されない (静的アフィニティ)。
これは実装が単純で、キャッシュの効率も良い。

### スケジューラの動作

スケジューラは **CPU ごとに独立して** 動作し、
自分の CPU にアフィニティを持つタスクの中から最高優先度のものを選ぶ。

```c
/* kernel/sched.c — sched_do_next_tsk (簡略化) */
ID sched_do_next_tsk(W apic)
{
    smp_lock(&kernel_lk);

    for (pri = 1; pri <= TMAX_TPRI; pri++) {
        /* 優先度 pri のタスクリストを走査 */
        for (each task t in tsk_pri[pri]) {
            if (t->tskstat == TTS_RDY &&
                proc[t->tskid].cpu == apic) {  /* この CPU のタスクか? */
                t->tskstat = TTS_RUN;
                current_proc[apic] = &proc[t->tskid];
                c_tskid[apic] = t->tskid;
                smp_unlock(&kernel_lk);
                return t->tskid;
            }
        }
    }

    smp_unlock(&kernel_lk);
    return E_ID;  /* 実行可能なタスクがない */
}
```

**ポイント:**
- `kernel_lk` (BKL) でロックを取ってから走査する (他の CPU との競合を防ぐ)
- 優先度は 1 (最高) から 16 (最低) まで順に探す
- `proc[t->tskid].cpu == apic` で CPU アフィニティをチェックしている
- 見つかったら `TTS_RUN` (実行中) に変更して返す

### アイドルタスク

全てのタスクが待ち状態のとき、CPU は何をするか?
tiny-itron では各 CPU に **アイドルタスク** を用意している。
最低優先度 (16) で無限ループするだけのタスクである。

```c
/* kernel/user.c */
void idle_task(void)
{
    while (1)
        __asm__ volatile("pause");  /* 省電力で待機 */
}
```

アイドルタスクが常に RDY 状態で存在するため、
`sched_do_next_tsk()` が RDY タスクを見つけられず WAI 状態のタスクを
誤って実行してしまう問題 (ghost running) を防ぐ。
スケジューラは「実行すべきタスクが 1 つもない」状態にならない。

---

## 10. tiny-itron の起動を追いかける

ここまでの知識を使って、tiny-itron が電源投入から 2 CPU で動作するまでの
流れを追いかけてみよう。

### Phase 1: BSP の起動 (CPU 0)

```
電源 ON
  │
  ▼
start.s (0x3000)          ← BIOS が制御を渡す (リアルモード)
  │  A20 有効化
  │  GDT/IDT ロード
  │  プロテクトモードに遷移
  ▼
run.s (0x3400)            ← 32 ビットモード
  │  cpu_num == 0 → BSP と判定
  │  ESP = 0x7A0000 (CPU 0 スタック)
  ▼
main() [i386/main.c]
  │  all_init()           ← IDT, VGA, PIC, タイマー, キーボード初期化
  │  page_init()          ← ページテーブル構築
  │  page_enable()        ← CR0.PG=1 (ページング有効化)
  │  itron_init()         ← ITRON カーネル初期化
  │  proc_init()          ← プロセス構造体初期化、初期タスク生成
  │  tss_init()           ← TSS 設定 (両 CPU 分)
  │  cpu_num = 1          ← 次に main() に来る CPU は AP だと示す
  ▼
smp_init() [i386/smp.c]
  │  Local APIC 有効化 (BSP)
  │  APIC タイマー設定
  │  INIT IPI 送信 ─────────────────────────┐
  │  SIPI 送信 (vector=0x03 → 0x3000) ──────┤
  │                                          │
  │  while (cpu_second == 0) ; ← AP 待ち     │
  │                                          ▼
  │                              [Phase 2: AP が起動]
  │
  │  ← cpu_second が 1 になった!
  │
  │  timer_start()        ← PIT タイマー開始
  │  key_start()          ← キーボード IRQ 有効化
  ▼
start_first_task()
  │  ltr $0x38             ← Task Register に TSS 0 をロード
  │  ESP = kern_esp → ret  ← RESTORE_ALL → iret で Ring 3 へ
  ▼
first_task() [kernel/user.c]
  │  セマフォ 1 を作成、タスク 3 (usr_main) を生成・起動
  │  Task 1 と Task 3 が wup_tsk/slp_tsk で交互に実行
  ▼
  (CPU 0 がタスクを実行し続ける)
```

### Phase 2: AP の起動 (CPU 1)

```
SIPI 受信 → 0x3000 から実行開始
  │
  ▼
start.s (0x3000)          ← リアルモード (BSP と同じコード)
  │  A20 有効化
  │  GDT/IDT ロード
  │  プロテクトモードに遷移
  ▼
run.s (0x3400)
  │  cpu_num == 1 → AP と判定
  │  ESP = 0x770000 (CPU 1 スタック)
  ▼
main() [i386/main.c]
  │  cpu_num != 0 → AP パスへ
  │  page_enable()          ← BSP のページテーブルを共有
  ▼
smp_ap_init() [i386/smp.c]
  │  Local APIC 有効化 (AP)
  │  APIC タイマー設定
  │  cpu_second = 1       ← BSP に「準備完了」を通知
  ▼
start_second_task()
  │  ltr $0x40             ← Task Register に TSS 1 をロード
  │  ESP = kern_esp → ret  ← RESTORE_ALL → iret で Ring 3 へ
  ▼
second_task() [kernel/user.c]
  │  タスク 4 (kbd_task) を生成・起動
  │  カウントアップ + セマフォで Task 3 と shared_count を競合
  ▼
  (CPU 1 がタスクを実行し続ける)
```

### タスク構成 (起動完了後)

```
CPU 0                          CPU 1
┌────────────────────┐        ┌────────────────────┐
│ Task 1: first_task │        │ Task 2: second_task│
│   (pri=15)         │        │   (pri=15)         │
│ Task 3: usr_main   │        │ Task 4: kbd_task   │
│   (pri=15)         │        │   (pri=1, 最高)    │
│ Task 5: idle_task  │        │ Task 6: idle_task  │
│   (pri=16, 最低)   │        │   (pri=16, 最低)   │
└────────────────────┘        └────────────────────┘
```

---

## 11. まとめ: 重要な原則

### SMP プログラミングの 5 つのルール

**1. 共有データにはロックが必要**

複数の CPU がアクセスするデータは、必ずロックで保護する。
読み取りだけでも、他の CPU が書き込み中かもしれない。

**2. ロックの粒度を意識する**

ロック 1 つで全てを保護する (粗粒度) のは安全だが性能が出ない。
用途別にロックを分ける (細粒度) と性能は上がるがデッドロックのリスクが増す。
tiny-itron は単一の `kernel_lk` (Big Kernel Lock) で全カーネルデータを保護する。
2 CPU・教育用カーネルでは正しさの保証が細粒度ロックの性能メリットに勝る。

**3. ロック順序を統一する**

複数のロックを同時に取る場合、全ての場所で **同じ順序** で取ること。
順序が逆転するとデッドロック (お互いにロック待ちで永久停止) が起こる。

```
正しい例:  CPU 0: lock(A) → lock(B)    CPU 1: lock(A) → lock(B)
危険な例:  CPU 0: lock(A) → lock(B)    CPU 1: lock(B) → lock(A)  ← デッドロック!
```

**4. volatile を正しく使う**

`volatile` は「この変数はプログラムの流れとは無関係に値が変わり得る」と
コンパイラに伝える修飾子である。具体的には、コンパイラに対して
「この変数へのアクセスを省略・並べ替え・レジスタキャッシュしてはならない」
と指示する。

他の CPU が変更する可能性がある変数には `volatile` を付ける。
付けないと、コンパイラが「この変数は自分しか変えないから
レジスタに置いたままでいい」と最適化してしまい、変更を見落とす。

**5. CPU ごとのデータは配列で管理する**

`current_proc[cpu]`、`c_tskid[cpu]` のように、
CPU 番号をインデックスにした配列で per-CPU データを管理する。
こうすることで、ロックなしで自分の CPU のデータに安全にアクセスできる
(他の CPU は自分のインデックスしか書き込まないため)。

### 本コードを読むためのチェックリスト

- [ ] `smp_lock` / `smp_unlock` を見たら → 共有データを保護している
- [ ] `get_apic_index()` を見たら → CPU 番号で per-CPU データを選んでいる
- [ ] `current_proc[apic]` を見たら → その CPU が実行中のタスク
- [ ] `c_tskid[apic]` を見たら → その CPU が実行中のタスク ID
- [ ] `sched_next_tsk(apic)` を見たら → リスケジュール要求 (両 CPU に通知)
- [ ] `sched_do_next_tsk(apic)` を見たら → 実際のタスク選択 (自分の CPU のみ)
- [ ] `smp_eoi()` を見たら → APIC 割り込み完了通知
- [ ] `volatile` を見たら → 他の CPU や割り込みから変更される変数
- [ ] `xchgl` を見たら → アトミック操作 (スピンロックの基盤)

---

## 参考: 用語一覧

| 用語 | 意味 |
|------|------|
| SMP | Symmetric Multi-Processing。複数 CPU が対等に動作する方式 |
| BSP | Bootstrap Processor。最初に起動する CPU |
| AP | Application Processor。BSP が起動する追加の CPU |
| APIC | Advanced Programmable Interrupt Controller |
| Local APIC | 各 CPU 内蔵の割り込みコントローラ |
| IPI | Inter-Processor Interrupt。CPU 間の割り込み |
| SIPI | Startup IPI。AP を起動するための特殊な IPI |
| ICR | Interrupt Command Register。IPI を送信するための APIC レジスタ |
| EOI | End of Interrupt。割り込み処理完了の通知 |
| TSS | Task State Segment。x86 のタスク切り替え用データ構造 |
| スピンロック | ロック獲得までループ (spin) し続ける排他制御方式 |
| アトミック操作 | 他者が割り込めない不可分な操作 |
| レースコンディション | 複数実行主体の実行順序に依存するバグ |
| デッドロック | 複数のロックを巡ってお互いに待ち合う永久停止 |
| CPU アフィニティ | タスクをどの CPU で実行するかの設定 |
| per-CPU データ | CPU ごとに独立して管理される変数 |
| volatile | コンパイラの最適化を抑制し、毎回メモリから読むことを強制する修飾子 |
| Ring 0 / Ring 3 | x86 の特権レベル。0 がカーネル、3 がユーザー |
| PIC (i8259) | 旧式の割り込みコントローラ (シングル CPU 向け) |
| メモリマップド I/O | ハードウェアレジスタをメモリアドレスとしてアクセスする方式 |
