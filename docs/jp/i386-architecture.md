# i386 アーキテクチャ基礎 — tiny-itron を読むために

本ドキュメントは、tiny-itron のソースコードを理解するために必要な
Intel 386 (IA-32) のハードウェア知識をまとめたものである。

---

## 目次

1. [リアルモードとプロテクトモード](#1-リアルモードとプロテクトモード)
2. [セグメンテーション](#2-セグメンテーション)
   - [2.1 セグメントとは](#21-セグメントとは)
   - [2.2 セグメントディスクリプタ](#22-セグメントディスクリプタ)
   - [2.3 GDT (Global Descriptor Table)](#23-gdt-global-descriptor-table)
   - [2.4 セレクタ](#24-セレクタ)
   - [2.5 tiny-itron の GDT レイアウト](#25-tiny-itron-の-gdt-レイアウト)
3. [特権レベル (Ring 0 / Ring 3)](#3-特権レベル-ring-0--ring-3)
   - [3.1 4 つのリング](#31-4-つのリング)
   - [3.2 CPL・DPL・RPL](#32-cpldplrpl)
   - [3.3 特権遷移とスタック切り替え](#33-特権遷移とスタック切り替え)
   - [3.4 tiny-itron での特権レベル](#34-tiny-itron-での特権レベル)
4. [IDT (Interrupt Descriptor Table)](#4-idt-interrupt-descriptor-table)
   - [4.1 ゲートディスクリプタ](#41-ゲートディスクリプタ)
   - [4.2 割り込みゲートとトラップゲート](#42-割り込みゲートとトラップゲート)
   - [4.3 tiny-itron の IDT レイアウト](#43-tiny-itron-の-idt-レイアウト)
5. [TSS (Task State Segment)](#5-tss-task-state-segment)
   - [5.1 TSS の構造](#51-tss-の構造)
   - [5.2 ハードウェアタスクスイッチ](#52-ハードウェアタスクスイッチ)
   - [5.3 tiny-itron での TSS の使い方](#53-tiny-itron-での-tss-の使い方)
6. [PIC (i8259 割り込みコントローラ)](#6-pic-i8259-割り込みコントローラ)
   - [6.1 PIC の役割](#61-pic-の役割)
   - [6.2 マスタとスレーブのカスケード接続](#62-マスタとスレーブのカスケード接続)
   - [6.3 初期化シーケンス (ICW1〜ICW4)](#63-初期化シーケンス-icw1icw4)
   - [6.4 IRQ マスクと EOI](#64-irq-マスクと-eoi)
7. [割り込みの流れ: SAVE_ALL / RESTORE_ALL](#7-割り込みの流れ-save_all--restore_all)
   - [7.1 割り込み発生時の CPU の動作](#71-割り込み発生時の-cpu-の動作)
   - [7.2 SAVE_ALL の仕組み](#72-save_all-の仕組み)
   - [7.3 RESTORE_ALL と intr_leave の仕組み](#73-restore_all-と-intr_leave-の仕組み)
   - [7.4 割り込みのネスト](#74-割り込みのネスト)
8. [システムコール](#8-システムコール)
   - [8.1 INT 命令によるソフトウェア割り込み](#81-int-命令によるソフトウェア割り込み)
   - [8.2 tiny-itron のシステムコールの流れ](#82-tiny-itron-のシステムコールの流れ)
9. [メモリレイアウト](#9-メモリレイアウト)
10. [A20 ライン](#10-a20-ライン)
11. [ページング (恒等マッピング)](#11-ページング-恒等マッピング)
12. [参考: ソースファイルと対応する概念](#12-参考-ソースファイルと対応する概念)

---

## 1. リアルモードとプロテクトモード

Intel 386 以降の CPU は 2 つの主要な動作モードを持つ。

**リアルモード (Real Mode)**:
電源投入時に CPU が起動するモード。8086 互換の 16 ビット環境。
- アドレスは `セグメント:オフセット` で計算 → 物理アドレス = セグメント × 16 + オフセット
- アクセスできるメモリは最大 1 MB
- メモリ保護やタスク分離の機能がない

**プロテクトモード (Protected Mode)**:
386 の本来の 32 ビット動作モード。OS が使う。
- 最大 4 GB のリニアアドレス空間
- セグメンテーションによるメモリ保護
- 特権レベル (Ring 0〜3) によるアクセス制御
- 割り込みディスクリプタテーブル (IDT) によるゲート機構

**モード切り替え** (`i386/start.s`):

```
 リアルモード                        プロテクトモード
 ┌─────────────────┐    CR0.PE=1    ┌─────────────────┐
 │ 16 ビット       │ ──────────────→│ 32 ビット       │
 │ セグメント:オフセ│    far jump    │ GDT/IDT による  │
 │ ットで 1 MB     │    で CS 更新   │ メモリ保護      │
 └─────────────────┘                └─────────────────┘
```

tiny-itron では `start.s` がリアルモードからプロテクトモードへ移行する:

```asm
# CR0 の PE ビット (bit 0) を 1 にセット
movl    %cr0, %eax
orl     $0x00000001, %eax
movl    %eax, %cr0

# far jump でパイプラインをフラッシュし、CS を新しいセレクタに切り替え
ljmp    $0x08, $flush_start     # SEL_K16_C (16 ビットカーネルコード)
```

---

## 2. セグメンテーション

### 2.1 セグメントとは

プロテクトモードでは、メモリはセグメント (論理的な領域) に分割して管理される。
プログラムがメモリにアクセスするとき、CPU は次の計算を行う:

```
リニアアドレス = セグメントのベースアドレス + オフセット
```

セグメントには「ベースアドレス」「リミット (サイズ)」「種類 (コード/データ/スタック)」
「特権レベル」などの属性がある。これらはセグメントディスクリプタに格納される。

### 2.2 セグメントディスクリプタ

セグメントディスクリプタは **8 バイト**の構造体で、セグメントの属性を記述する。

```
ビット位置:
 63      56 55  52 51  48 47      40 39      32
 ┌────────┬──────┬──────┬─────────┬──────────┐
 │base_h  │G D 0 │limit │ P DPL S │ base_m   │
 │(31:24) │  AVL │(19:16)│ Type   │ (23:16)  │
 └────────┴──────┴──────┴─────────┴──────────┘
 31                    16 15                  0
 ┌───────────────────────┬────────────────────┐
 │   base_l (15:0)       │   limit_l (15:0)   │
 └───────────────────────┴────────────────────┘
```

tiny-itron での C 構造体 (`i386/386.h`):
```c
typedef struct seg {
    unsigned short  limit_l;    /* リミットの下位 16 ビット */
    unsigned short  base_l;     /* ベースアドレスの下位 16 ビット */
    unsigned char   base_m;     /* ベースアドレスのビット 16〜23 */
    unsigned char   type;       /* P + DPL + S + Type (アクセス権) */
    unsigned char   limit_h;    /* G + D/B + リミット上位 4 ビット */
    unsigned char   base_h;     /* ベースアドレスのビット 24〜31 */
} seg_t;
```

**主なフィールド**:

| フィールド | 意味 |
|-----------|------|
| Base (32 bit) | セグメントの開始リニアアドレス (ページング有効時、物理アドレスへの変換はページング段で行われる) |
| Limit (20 bit) | セグメントのサイズ (G=1 なら 4KB 単位) |
| P (Present) | 1 = セグメントがメモリに存在する |
| DPL | ディスクリプタ特権レベル (0〜3) |
| S | 0 = システムセグメント (TSS 等), 1 = コード/データ |
| Type | セグメントの種類 (実行/読み取り/書き込み等) |
| G (Granularity) | 0 = バイト単位, 1 = 4KB ページ単位 |
| D/B | 0 = 16 ビット, 1 = 32 ビット |

**Type フィールドの主な値** (`i386/386.h`):

| 定数 | 値 | 意味 |
|------|------|------|
| `ST_CODE` | 0x9A | コードセグメント (実行+読み取り, P=1, DPL=0) |
| `ST_DATA` | 0x92 | データセグメント (読み書き, P=1, DPL=0) |
| `ST_STACK` | 0x96 | スタックセグメント (読み書き+拡張方向下, P=1, DPL=0) |
| `ST_TSS` | 0x89 | TSS ディスクリプタ (システムセグメント) |
| `_32BIT` | 0xC0 | D/B=1, G=1 (32 ビット, 4KB 粒度) |
| `_16BIT` | 0x00 | D/B=0, G=0 (16 ビット, バイト粒度) |

### 2.3 GDT (Global Descriptor Table)

GDT はセグメントディスクリプタの配列で、メモリ上の固定アドレスに配置される。
CPU は `GDTR` レジスタ (GDT Register) で GDT の位置とサイズを知る。

```
GDTR レジスタ:
┌──────────────────────┬──────────────┐
│  ベースアドレス (32 bit)│ リミット(16 bit)│
└──────────────────────┴──────────────┘

GDT レジスタの設定 (start.s):
lgdt  gdt_ptr        # GDTR にアドレスとサイズをロード

gdt_ptr:
    .word  256-1      # リミット = 255 (256 バイト = 32 エントリ)
    .word  0x2000, 0  # ベースアドレス = 0x2000
```

tiny-itron では GDT は物理アドレス **0x2000** に配置される。

### 2.4 セレクタ

セグメントレジスタ (CS, DS, SS, ES, FS, GS) にはセレクタ値をロードする。
セレクタは GDT のどのエントリを参照するかを指定する 16 ビットの値。

```
15                 3  2  1  0
┌──────────────────┬──┬─────┐
│   Index          │TI│ RPL │
│  (GDT エントリ番号)│  │     │
└──────────────────┴──┴─────┘

Index: GDT 内のディスクリプタ番号 (0〜8191)
TI:    0 = GDT, 1 = LDT (tiny-itron では常に 0)
RPL:   要求特権レベル (0〜3)
```

**セレクタ値の計算例**:
- GDT エントリ 4 (Index=4), TI=0, RPL=0 → `4 × 8 = 0x20`
- GDT エントリ 11 (Index=11), TI=0, RPL=3 → `11 × 8 + 3 = 0x5B`

### 2.5 tiny-itron の GDT レイアウト

`i386/addr.h` で定義される全セレクタ:

```
GDT (物理アドレス 0x2000)
┌──────┬────────┬───────────────────────────────────────────┐
│Index │セレクタ│ 用途                                       │
├──────┼────────┼───────────────────────────────────────────┤
│  0   │ 0x00  │ NULL ディスクリプタ (CPU が要求する空エントリ)  │
│  1   │ 0x08  │ 16 ビットカーネルコード (SEL_K16_C)          │
│  2   │ 0x10  │ 16 ビットカーネルデータ (SEL_K16_D)          │
│  3   │ 0x18  │ 16 ビットカーネルスタック (SEL_K16_S)        │
│  4   │ 0x20  │ 32 ビットカーネルコード (SEL_K32_C)          │
│  5   │ 0x28  │ 32 ビットカーネルデータ (SEL_K32_D)          │
│  6   │ 0x30  │ 32 ビットカーネルスタック (SEL_K32_S)        │
│  7   │ 0x38  │ TSS0 — CPU 0 用 (SEL_TSS0, esp0/ss0 のみ)   │
│  8   │ 0x40  │ TSS1 — CPU 1 用 (SEL_TSS1, esp0/ss0 のみ)   │
│  9   │ 0x48  │ (未使用)                                     │
│ 10   │ 0x50  │ (未使用)                                     │
│ 11   │ 0x58  │ 32 ビットユーザーコード (SEL_U32_C)          │
│ 12   │ 0x60  │ 32 ビットユーザーデータ (SEL_U32_D)          │
│ 13   │ 0x68  │ 32 ビットユーザースタック (SEL_U32_S)        │
│ 14   │ 0x70  │ (未使用 — addr.h で SEL_SYSCALL として定義されているが、│
│      │       │  実際の syscall は IDT ベクタ 0x99 を使用)      │
└──────┴────────┴───────────────────────────────────────────┘
```

**重要**: Index 1〜3 (16 ビットセグメント) は `start.s` がリアルモードから
プロテクトモードへ遷移する際に一時的に使用される。カーネル本体が動く頃には
32 ビットセグメント (Index 4〜6) が使われる。

**フラットモデル**: tiny-itron の 32 ビットセグメント (Index 4〜6, 11〜13) は
ベースアドレスが 0、リミットが最大値に設定されており、事実上
論理アドレス＝リニアアドレスとなる (いわゆるフラットモデル)。
セグメンテーションは主に**特権レベルの分離**のために使われている。

> **16 ビットセグメントの例外**: Index 1〜3 はリアルモード→プロテクトモード遷移用で、
> ベースアドレスが 0 ではない (`0x08` → base=0x3000, `0x10` → base=0xB8000,
> `0x18` → base=0x3000)。これらは `start.s` での遷移中のみ使用され、
> 32 ビットカーネルが動く頃には参照されない。

---

## 3. 特権レベル (Ring 0 / Ring 3)

### 3.1 4 つのリング

i386 は 4 段階の特権レベル (Ring 0〜Ring 3) をサポートする。
数値が小さいほど特権が高い。

```
         ┌─────────────────────┐
         │      Ring 0         │  カーネル (最高特権)
         │  ┌───────────────┐  │
         │  │   Ring 1      │  │  (未使用)
         │  │ ┌───────────┐ │  │
         │  │ │  Ring 2   │ │  │  (未使用)
         │  │ │ ┌───────┐ │ │  │
         │  │ │ │Ring 3 │ │ │  │  ユーザータスク (最低特権)
         │  │ │ └───────┘ │ │  │
         │  │ └───────────┘ │  │
         │  └───────────────┘  │
         └─────────────────────┘
```

tiny-itron では Ring 0 (カーネル) と Ring 3 (ユーザー) の 2 つだけを使う。

### 3.2 CPL・DPL・RPL

特権レベルの判定に関わる 3 つの概念:

| 略語 | 名前 | 場所 | 意味 |
|------|------|------|------|
| CPL | Current Privilege Level | CS レジスタの下位 2 ビット | 現在の実行特権レベル |
| DPL | Descriptor Privilege Level | セグメントディスクリプタ内 | そのセグメントの特権レベル |
| RPL | Requested Privilege Level | セレクタの下位 2 ビット | アクセス要求時の特権レベル |

**アクセスチェック** (データセグメントの場合):
CPU はデータセグメントをロードする際に `max(CPL, RPL) <= DPL` を検査する。
違反すると **#GP (General Protection Fault)**。

> **補足:** これはデータセグメントへのアクセス規則を簡略化したもの。
> 実際の保護規則はセグメント種別によって異なる。
> 例: SS のロードは `CPL == RPL == DPL` が必要、
> conforming code segment は DPL <= CPL で転送可能、など。
> tiny-itron ではフラットモデル (全セグメントが 0-4GB) のため、
> セグメント単位の保護はほぼ関与しない。

### 3.3 特権遷移とスタック切り替え

Ring 3 から Ring 0 への遷移 (割り込み、syscall) が起きると、
CPU は自動的に以下を行う:

1. TSS の `ss0` と `esp0` を読んで Ring 0 のスタックに切り替える
2. 旧 SS, 旧 ESP, EFLAGS, 旧 CS, 旧 EIP をスタックに push する

```
Ring 3 → Ring 0 割り込みスタックフレーム:

    ┌──────────────┐  ← TSS の ESP0 (push 前の初期値)
    │  旧 SS       │  +16  ← Ring 3 のスタックセグメント
    │  旧 ESP      │  +12  ← Ring 3 のスタックポインタ
    │  EFLAGS      │  +8   ← 割り込み前のフラグ
    │  旧 CS       │  +4   ← 割り込み前のコードセグメント
    │  旧 EIP      │  +0   ← 割り込み前の命令アドレス
    └──────────────┘  ← 新しい ESP (5 回 push した後)
```

**重要**: Ring 0 → Ring 0 の割り込みでは SS/ESP は push されない
(スタック切り替えが不要なため)。tiny-itron はすべてのタスクを Ring 3 で
実行するため、通常のタスク割り込みでは常に 5 ワードが push される。

### 3.4 tiny-itron での特権レベル

| 状態 | CS | DS | SS | Ring |
|------|------|------|------|------|
| カーネルモード | 0x20 | 0x28 | 0x30 | 0 |
| ユーザーモード | 0x5B | 0x63 | 0x6B | 3 |

**なぜ 0x5B, 0x63, 0x6B なのか**:
- `SEL_U32_C` = 0x58 (GDT Index 11)。Ring 3 で使うので RPL=3 を付加 → `0x58 | 3 = 0x5B`
- `SEL_U32_D` = 0x60 (GDT Index 12)。同様に `0x60 | 3 = 0x63`
- `SEL_U32_S` = 0x68 (GDT Index 13)。同様に `0x68 | 3 = 0x6B`

**Ring 3 での制限**:
- `cli` / `sti` 命令を実行すると **#GP** (General Protection Fault)
- I/O 命令 (`in` / `out`) は TSS の I/O ビットマップで制御される
- スピンロック (`smp_lock`) は `xchgl` 命令を使うので Ring 3 でも安全

---

## 4. IDT (Interrupt Descriptor Table)

### 4.1 ゲートディスクリプタ

IDT は**ゲートディスクリプタ**の配列である。各エントリは割り込みベクタ番号に
対応し、割り込みハンドラのアドレスと属性を保持する。

```
ゲートディスクリプタ (8 バイト):
 63                48 47      40 39      32
 ┌──────────────────┬─────────┬──────────┐
 │  offset_h        │ P DPL   │ count    │
 │  (31:16)         │  Type   │ (パラメータ)│
 └──────────────────┴─────────┴──────────┘
 31                16 15                  0
 ┌──────────────────┬────────────────────┐
 │  selector        │   offset_l (15:0)  │
 └──────────────────┴────────────────────┘
```

tiny-itron の C 構造体 (`i386/386.h`):
```c
typedef struct gate {
    unsigned short  offset_l;   /* ハンドラアドレスの下位 16 ビット */
    unsigned short  sel;        /* ハンドラのコードセグメントセレクタ */
    unsigned char   count;      /* パラメータ数 (コールゲートで使用) */
    unsigned char   type;       /* P + DPL + Type */
    unsigned short  offset_h;   /* ハンドラアドレスの上位 16 ビット */
} gate_t;
```

### 4.2 割り込みゲートとトラップゲート

| 種類 | 定数 | 値 | IF フラグ | 用途 |
|------|------|------|-----------|------|
| 割り込みゲート | `GT_INTR` | 0x8E | クリア (割り込み禁止) | IRQ ハンドラ、一部の CPU 例外 |
| トラップゲート | `GT_TRAP` | 0x8F | 変更なし | 大部分の CPU 例外 |

> **注:** IDT に置けるゲート種別は割り込みゲート、トラップゲート、タスクゲートの 3 種類。
> コールゲート (`GT_CALL = 0x8C`) は GDT/LDT に置くシステムディスクリプタであり、
> IDT のゲート種別ではない。tiny-itron では syscall にコールゲートではなく
> DPL=3 の割り込みゲートを使う (後述)。

**割り込みゲート** (GT_INTR = 0x8E):
ハンドラに入ると CPU が自動的に `IF=0` (割り込み禁止) にする。
ハードウェア割り込み (IRQ) のハンドラに使う。

**トラップゲート** (GT_TRAP = 0x8F):
IF フラグを変更しない。CPU 例外のハンドラに使われることが多い。

> tiny-itron では #DE (Divide Error, vector 0)、#GP (General Protection, vector 13)、
> #PF (Page Fault, vector 14) の 3 例外は GT_INTR で登録されている。
> これらの例外が発生すると IF がクリアされ、ハンドラ内で割り込みが禁止される。
> 残りの例外 (#DB, NMI, #BP, #OF, #BR, #UD, #NM, #DF, #TS, #NP, #SS 等) は
> GT_TRAP で登録されている。

**DPL の重要性** (ソフトウェア割り込み `INT n` の場合):
- DPL=0 のゲート → Ring 0 からの `INT n` のみ許可
- DPL=3 のゲート → Ring 3 からの `INT n` も許可

> **注:** この DPL チェックは `INT n` 命令 (ソフトウェア割り込み) に対してのみ行われる。
> 外部 IRQ や CPU 例外のようなハードウェア起因の割り込みでは、
> DPL に関係なくハンドラが実行される。

tiny-itron の syscall は GDT のコールゲートではなく、**IDT の割り込みゲート**を使う。
IDT ベクタ 0x99 に `GT_INTR | 0x60 = 0xEE` (DPL=3 の割り込みゲート) が設定されており、
ユーザータスクから `INT 0x99` で呼び出せる。DPL=3 なので Ring 3 からの
`INT` 命令を許可しつつ、割り込みゲートなので IF がクリアされる。

### 4.3 tiny-itron の IDT レイアウト

IDT は物理アドレス **0x2100** に配置される。256 エントリ。

```
IDT (物理アドレス 0x2100)
┌────────┬──────────────────────────────────────────────┐
│ ベクタ │ 用途                                          │
├────────┼──────────────────────────────────────────────┤
│   0    │ #DE  Divide Error (ゼロ除算)          [GT_INTR]  │
│   1    │ #DB  Debug (シングルステップ)           [GT_TRAP]  │
│   2    │ NMI  Non-Maskable Interrupt          [GT_TRAP]  │
│   3    │ #BP  Breakpoint (INT 3)              [GT_TRAP]  │
│   4    │ #OF  Overflow                        [GT_TRAP]  │
│   5    │ #BR  Bound Range Exceeded            [GT_TRAP]  │
│   6    │ #UD  Invalid Opcode                  [GT_TRAP]  │
│   7    │ #NM  Device Not Available (FPU)      [GT_TRAP]  │
│   8    │ #DF  Double Fault                    [GT_TRAP]  │
│   9    │ Coprocessor Segment Overrun          [GT_TRAP]  │
│  10    │ #TS  Invalid TSS                     [GT_TRAP]  │
│  11    │ #NP  Segment Not Present             [GT_TRAP]  │
│  12    │ #SS  Stack-Segment Fault             [GT_TRAP]  │
│  13    │ #GP  General Protection Fault        [GT_INTR]  │
│  14    │ #PF  Page Fault                      [GT_INTR]  │
│  15    │ Coprocessor Error (intr_copr_error)  [GT_TRAP]  │
│ 16-31  │ (予約 / 未登録)                                  │
├────────┼──────────────────────────────────────────────┤
│  0x80  │ IRQ0  PIT タイマー (~17ms, HZ=60)  [GT_INTR]   │
│  0x81  │ IRQ1  キーボード                   [GT_INTR]   │
│  0x82  │ IRQ2  カスケード (スレーブ PIC)     [GT_INTR]   │
│  0x83  │ IRQ3  COM2                        [GT_INTR]   │
│  0x84  │ IRQ4  COM1                        [GT_INTR]   │
│  0x85  │ IRQ5  LPT2                        [GT_INTR]   │
│  0x86  │ IRQ6  フロッピー                   [GT_INTR]   │
│  0x87  │ IRQ7  LPT1 / スプリアス            [GT_INTR]   │
│  0x90  │ IRQ8  RTC                         [GT_INTR]   │
│ 0x91-97│ IRQ9-15                           [GT_INTR]   │
├────────┼──────────────────────────────────────────────┤
│  0x98  │ APIC スプリアスベクタ                          │
│  0x99  │ ITRON システムコール  [GT_INTR | 0x60, DPL=3]  │
│  0x9A  │ APIC タイマー CPU 0                [GT_INTR]   │
│  0x9B  │ APIC タイマー CPU 1                [GT_INTR]   │
├────────┼──────────────────────────────────────────────┤
│ その他  │ デフォルトハンドラ (intr_default)    [GT_TRAP]  │
└────────┴──────────────────────────────────────────────┘
```

**初期化コード** (`i386/interrupt.c`):
```c
void idt_init(void) {
    /* まず全 256 エントリをデフォルトハンドラで埋める */
    for (i = 0; i < 256; i++)
        set_idt(i, (unsigned long)intr_default, SEL_K32_C, 0, GT_TRAP);

    setup_trap();       /* CPU 例外 (0〜15) を登録 */
    setup_irq();        /* IRQ (0x80〜0x97) を登録 */
    setup_syscall();    /* システムコール (0x99) を登録 */
}
```

> **注意:** APIC 関連のベクタ (0x98 スプリアス、0x9A/0x9B タイマー) は
> `idt_init()` では登録されない。これらは後から `smp_init()`
> (`i386/smp.c`) で `set_idt()` により登録される。

---

## 5. TSS (Task State Segment)

### 5.1 TSS の構造

TSS は CPU がタスクの状態 (レジスタ、スタックポインタ等) を保存する領域である。
i386 では 104 バイトの固定構造を持つ。

```
TSS の構造 (104 バイト):
┌────────────────────────────┐  オフセット
│  prev_link (前タスクリンク)  │   0
│  esp0 (Ring 0 スタック)     │   4
│  ss0  (Ring 0 SS)          │   8
│  esp1, ss1 (Ring 1)        │  12, 16
│  esp2, ss2 (Ring 2)        │  20, 24
│  cr3 (ページディレクトリ)    │  28
│  eip                       │  32
│  eflags                    │  36
│  eax, ecx, edx, ebx       │  40, 44, 48, 52
│  esp, ebp, esi, edi        │  56, 60, 64, 68
│  es, cs, ss, ds, fs, gs   │  72〜96
│  ldt                       │  100
│  I/O ビットマップベース      │  102
└────────────────────────────┘
```

tiny-itron の C 構造体 (`i386/tss.h`):
```c
typedef struct tss {
    unsigned short  prev_link, dummy0;
    unsigned long   esp0;
    unsigned short  ss0, dummy1;
    unsigned long   esp1;
    unsigned short  ss1, dummy2;
    unsigned long   esp2;
    unsigned short  ss2, dummy3;
    unsigned long   cr3;
    unsigned long   eip, eflags;
    unsigned long   eax, ecx, edx, ebx, esp, ebp, esi, edi;
    unsigned short  es, dummy4, cs, dummy5, ss, dummy6;
    unsigned short  ds, dummy7, fs, dummy8, gs, dummy9;
    unsigned short  ldt, dummy10;
    unsigned short  t, io_base;
} tss_t;
```

### 5.2 ハードウェアタスクスイッチ (参考)

i386 は `ljmp` (far jump) 命令で TSS セレクタを指定すると、
**ハードウェアが自動的に**全レジスタを保存・復元するタスクスイッチを行う。

```
CPU の動作 (ljmp $SEL_TSS0, $0):
1. 現在の全レジスタを「旧タスク」の TSS に保存
2. GDT から SEL_TSS0 の TSS ディスクリプタを読む
3. TSS ディスクリプタが指す TSS 構造体から全レジスタをロード
4. TSS の EIP から実行開始
```

> tiny-itron ではハードウェアタスクスイッチは使わない。
> 初回タスク起動を含め、すべてのタスク遷移は `RESTORE_ALL` + `iret` で行う。

### 5.3 tiny-itron での TSS の使い方

tiny-itron では TSS を **esp0/ss0 の提供のみ** に使う。

**初回タスク起動 (ltr + iret)**

各 CPU は `start_first_task` / `start_second_task` で `ltr` により Task Register に
TSS セレクタをロードする。`ltr` はレジスタの保存・復元を行わず、CPU に
「この TSS の esp0/ss0 を Ring 3→Ring 0 割り込みで使え」と伝えるだけ。

この仕組みが成立するのは、`start_first_task` の**前**に `proc_create` がカーネルスタック上に
偽の pt_regs フレームを構築しているからである。`proc_create` (`i386/proc.c`) は
タスク生成時 (`cre_tsk` / `proc_init`) に以下のフレームをカーネルスタックに書き込む:

```
proc_create が構築する偽フレーム (proc.c):

    ┌──────────────────────┐  ← kern_stack_top
    │  SS   = 0x6B         │  ← SEL_U32_S | 3 (Ring 3 スタック)
    │  ESP  = user_esp     │  ← ユーザースタックの頂上
    │  EFLAGS = 0x200      │  ← INIT_EFLAGS (IF=1: 割り込み許可)
    │  CS   = 0x5B         │  ← SEL_U32_C | 3 (Ring 3 コード)
    │  EIP  = task entry   │  ← タスク関数のアドレス (例: first_task)
    ├──────────────────────┤
    │  EAX = 0             │
    │  ECX〜EDI = 0        │  ← SAVE_ALL に対応する 9 レジスタ (全て 0)
    │  DS  = 0x63          │  ← SEL_U32_D | 3
    │  ES  = 0x63          │
    ├──────────────────────┤
    │  intr_return_restore │  ← ret で飛ぶアドレス
    └──────────────────────┘  ← kern_esp (proc_create が保存)
```

`start_first_task` はこの仕込み済みのスタックを消費するだけである:

```asm
/* klib.s */
start_first_task:
    movw    $0x38, %ax          /* SEL_TSS0 */
    ltr     %ax                 /* Task Register にロード (レジスタ交換なし) */
    movl    current_proc, %ebx  /* current_proc[0] = &proc[1] */
    movl    (%ebx), %esp        /* ESP = proc[1].kern_esp ← 偽フレームの底 */
    ret                         /* pop → intr_return_restore に飛ぶ */
                                /*   → RESTORE_ALL: 9 レジスタを pop */
                                /*   → iret: EIP/CS/EFLAGS/ESP/SS を pop */
                                /*   → Ring 3 で task entry から実行開始 */
```

こうすることで、初回タスク起動も通常の割り込み復帰も同じパス
(`RESTORE_ALL` + `iret`) を使い、特別な起動ルーチンが不要になっている。

**Ring 0 スタックの提供 (動的 esp0 更新)**

TSS は Ring 3 → Ring 0 の割り込み時に使用する Ring 0 スタック (`esp0`, `ss0`) を
CPU に提供する。CPU は割り込み発生時に TSS の `esp0`/`ss0` を読み、Ring 0 の
スタックに切り替えてから割り込みフレームを push する。

各タスクは独自の 4KB カーネルスタックを持つ。タスクスイッチ時に
`tss_update_esp0()` を呼び、新タスクの `kern_stack_top` の値で TSS.esp0 を
上書きする。これにより次の割り込みは新タスクのカーネルスタックに切り替わる。

**タスクスイッチ**: 初回起動も 2 回目以降も同じパスを使う。
`SAVE_ALL`/`RESTORE_ALL` + `intr_leave` (`intr.s`) で管理される。
`intr_leave` が ESP を新タスクのカーネルスタックに切り替え、
`RESTORE_ALL` + `iret` が新タスクのレジスタを復元する。

---

## 6. PIC (i8259 割り込みコントローラ)

### 6.1 PIC の役割

PIC (Programmable Interrupt Controller) は、外部デバイスからの割り込み信号を
受け取り、CPU に通知する役割を持つ。i8259 は IBM PC 互換機の標準 PIC である。

```
 Devices                     PIC                    CPU
 +-----------+            +----------+
 | Timer     |-- IRQ0 --->|          |
 | Keyboard  |-- IRQ1 --->|  Master  |--- INT --->  CPU
 | ...       |-- IRQ3~7 ->|  i8259   |
 |           |            +----+-----+
 |           |                 ^ Cascade (IRQ2)
 |           |            +----+-----+
 | RTC       |-- IRQ8 --->|  Slave   |
 | ...       |-- IRQ9~15->|  i8259   |
 +-----------+            +----------+
```

### 6.2 マスタとスレーブのカスケード接続

PC/AT 互換機では 2 つの i8259 がカスケード (縦列) 接続されている:
- **マスタ PIC**: IRQ0〜7 を管理。I/O ポート 0x20/0x21
- **スレーブ PIC**: IRQ8〜15 を管理。I/O ポート 0xA0/0xA1
- スレーブの出力がマスタの IRQ2 に接続される

これにより合計 15 本の割り込みライン (IRQ0〜1, IRQ3〜15) が使える。

### 6.3 初期化シーケンス (ICW1〜ICW4)

PIC は 4 つの **ICW** (Initialization Command Word) で初期化する。
tiny-itron の初期化コード (`i386/i8259.c`):

```c
/* ── マスタ PIC (I/O ポート 0x20/0x21) ── */
outb(0x20, 0x11);       /* ICW1: エッジトリガ、ICW4 あり */
outb(0x21, 0x80);       /* ICW2: IRQ0 → ベクタ 0x80 にマッピング */
outb(0x21, 0x04);       /* ICW3: IRQ2 にスレーブが接続 */
outb(0x21, 0x0D);       /* ICW4: バッファモード (マスタ) */

/* ── スレーブ PIC (I/O ポート 0xA0/0xA1) ── */
outb(0xA0, 0x11);       /* ICW1: エッジトリガ、ICW4 あり */
outb(0xA1, 0x90);       /* ICW2: IRQ8 → ベクタ 0x90 にマッピング */
outb(0xA1, 0x02);       /* ICW3: マスタの IRQ2 に接続 */
outb(0xA1, 0x09);       /* ICW4: バッファモード (スレーブ) */
```

**ICW2 (ベクタマッピング)** が最も重要である。
PIC はデバイスからの IRQ を CPU の割り込みベクタに変換する:

| IRQ | デバイス | ベクタ | ICW2 の設定 |
|-----|---------|--------|-----------|
| IRQ0 | PIT タイマー | 0x80 | マスタ ICW2 = 0x80 |
| IRQ1 | キーボード | 0x81 | (0x80 + 1) |
| IRQ6 | フロッピー | 0x86 | (0x80 + 6) |
| IRQ8 | RTC | 0x90 | スレーブ ICW2 = 0x90 |

**なぜ 0x80 から?**: ベクタ 0〜31 は CPU 例外に予約されている。
衝突を避けるため、IRQ のベクタを 0x80 以降に移動する。
(Linux では 0x20〜0x2F を使うが、tiny-itron は 0x80 を選んだ。)

### 6.4 IRQ マスクと EOI

**IRQ マスク (OCW1)**:
初期化後、全 IRQ をマスク (無効化) し、必要な IRQ だけ個別に有効化する:

```c
outb(0x21, 0xFF);   /* マスタ: 全 IRQ マスク */
outb(0xA1, 0xFF);   /* スレーブ: 全 IRQ マスク */

/* 後から個別にアンマスク */
irq_mask_off(0x01);  /* IRQ0 (タイマー) を有効化: ビット0 をクリア */
irq_mask_off(0x02);  /* IRQ1 (キーボード) を有効化 */
```

**EOI (End of Interrupt)**:
割り込みハンドラの処理が終わったら、PIC に EOI を送って
「次の割り込みを受け付けてよい」と通知する:

```c
void i8259_reenable(void) {
    outb(0x20, 0x20);   /* マスタに EOI 送信 */
    outb(0xA0, 0x20);   /* スレーブに EOI 送信 */
    smp_eoi();           /* APIC にも EOI 送信 */
}
```

---

## 7. 割り込みの流れ: SAVE_ALL / RESTORE_ALL

### 7.1 割り込み発生時の CPU の動作

Ring 3 のユーザータスクが実行中に IRQ が発生すると、CPU は以下の処理を行う:

```
1. IF をクリア (割り込みゲートの場合)
2. TSS の ss0/esp0 を読んで Ring 0 スタック (タスクのカーネルスタック) に切り替え
3. スタックに SS, ESP, EFLAGS, CS, EIP を push
4. IDT からハンドラのアドレスを取得
5. ハンドラにジャンプ

カーネルスタック (TSS.esp0 から下方向):
                    ┌──────────────┐
                    │  旧 SS       │  esp+16
                    │  旧 ESP      │  esp+12
                    │  EFLAGS      │  esp+8
                    │  旧 CS       │  esp+4
                    │  旧 EIP      │  esp+0   ← ESP (Ring 0)
                    └──────────────┘
```

各タスクは専用の 4KB カーネルスタックを持つ。タスクスイッチ時に
`tss_update_esp0()` が新しいタスクの `kern_stack_top` の値で TSS.esp0 を
上書きするため、次の割り込みは必ず正しいタスクのカーネルスタックに切り替わる。

### 7.2 SAVE_ALL の仕組み

`SAVE_ALL` マクロ (`i386/intr.s`) は全割り込みハンドラの先頭で展開され、
全汎用レジスタとセグメントレジスタをカーネルスタックに push する。

CPU が push した割り込みフレーム (EIP, CS, EFLAGS, ESP, SS) の下に
9 個のレジスタを追加することで、完全な `pt_regs` フレームが構成される:

```
pt_regs frame (on kernel stack):

              +----------+
        0x34  |  SS      |  --+
        0x30  |  ESP     |    |  CPU pushes
        0x2C  |  EFLAGS  |    |  (Ring 3 -> Ring 0)
        0x28  |  CS      |    |
        0x24  |  EIP     |  --+
              +----------+
        0x20  |  EAX     |  --+
        0x1C  |  ECX     |    |
        0x18  |  EDX     |    |
        0x14  |  EBX     |    |  SAVE_ALL pushes
        0x10  |  EBP     |    |
        0x0C  |  ESI     |    |
        0x08  |  EDI     |    |
        0x04  |  DS      |    |
        0x00  |  ES      |  --+  <- ESP after SAVE_ALL
              +----------+
```

**SAVE_ALL の処理**:

```asm
.macro SAVE_ALL
    pushl   %eax            # 9 レジスタを push
    pushl   %ecx
    pushl   %edx
    pushl   %ebx
    pushl   %ebp
    pushl   %esi
    pushl   %edi
    pushl   %ds
    pushl   %es
    movw    $0x28, %ax      # DS/ES をカーネルデータセグメントにリロード
    movw    %ax, %ds        # (CPU は CS/SS を自動切り替えするが
    movw    %ax, %es        #  DS/ES は変更しないため)
.endm
```

**SAVE_ALL 後の `intr_enter`**: APIC ID から CPU 番号を判定し、
`k_nest` (割り込みネストカウンタ) をインクリメントする。

### 7.3 RESTORE_ALL と intr_leave の仕組み

割り込みハンドラの C 関数が戻った後、共通復帰パス `intr_return` を通る:

```asm
intr_return:
    call    intr_leave          # (1) ネスト管理 + タスクスイッチ
intr_return_restore:
    RESTORE_ALL                 # (2) レジスタ復元
    iret                        # (3) Ring 3 に復帰
```

**intr_leave の処理**:

```
intr_leave:
  1. APIC ID を読んで CPU 番号を判定
  2. k_nest をデクリメント
  3. k_nest > 0 (ネスト中) なら何もせず ret
  4. k_nest == 0 (最外の割り込み) なら:
     a. current_proc[cpu]->kern_esp = ESP   (現タスクの ESP を保存)
     b. sched_next_tsk_check(cpu)           (タスクスイッチ判定)
        → 必要なら current_proc[cpu] が新タスクに変更される
     c. ESP = current_proc[cpu]->kern_esp   (新タスクの ESP をロード)
     d. tss_update_esp0(cpu, kern_stack_top) (新タスクの kern_stack_top で TSS.esp0 を上書き)
  5. ret → intr_return_restore に戻る
```

**タスクスイッチの仕組み**:

タスクスイッチは ESP の差し替えだけで実現される。各タスクのカーネルスタックには
そのタスクの `pt_regs` フレームが保存されているため:
- `intr_leave` が ESP を新タスクの `kern_esp` に切り替えると
- `RESTORE_ALL` は新タスクの `pt_regs` からレジスタを pop し
- `iret` は新タスクの EIP/ESP/CS/SS にジャンプする

```
Old task's kernel stack:           New task's kernel stack:
  +-------------+                    +-------------+
  |  pt_regs    |                    |  pt_regs    |
  | (old task's |   ESP moves to     | (new task's |
  |  registers) |  ===============>  |  registers) |
  +-------------+                    +-------------+
                                         ^
                                         |
                                  RESTORE_ALL + iret
                                  pop from here
```

**RESTORE_ALL の処理**:

```asm
.macro RESTORE_ALL
    popl    %es             # SAVE_ALL の逆順に 9 レジスタを pop
    popl    %ds
    popl    %edi
    popl    %esi
    popl    %ebp
    popl    %ebx
    popl    %edx
    popl    %ecx
    popl    %eax
.endm
```

### 7.4 割り込みのネスト

`k_nest0`/`k_nest1` は CPU ごとの割り込みネストカウンタ。
`intr_enter`/`intr_leave` で管理する。

```
例: IRQ0 処理中に #PF (Page Fault) が発生
k_nest = 0 (ユーザー実行中)
  → IRQ0: SAVE_ALL + intr_enter (k_nest=1)
      → #PF: SAVE_ALL + intr_enter (k_nest=2)
      ← #PF: intr_leave (k_nest=1) — スケジュール判定なし
              RESTORE_ALL + iret → IRQ0 ハンドラに戻る
  ← IRQ0: intr_leave (k_nest=0) — ここでスケジュール判定 + ESP 切り替え
          RESTORE_ALL + iret → ユーザータスクに戻る
```

> **注**: tiny-itron ではすべての IRQ ハンドラが割り込みゲート (GT_INTR) で
> 登録されており、ハンドラ進入時に IF がクリアされる。復帰パスまで `sti` は
> 実行されないため、**通常の IRQ 同士がネストすることはない**。
>
> ネストが起きるのは NMI (マスク不可割り込み) や CPU 例外の場合に限られる。
> IF フラグが抑止するのはマスク可能な外部割り込み (IRQ) だけであり、
> CPU 例外 (#PF, #GP, #DE 等) は CPU が命令を実行した結果として
> 同期的に発生するため、IF=0 でも発生する。上の例では IRQ0 ハンドラ内の
> コードがページフォルトを起こすケースを示している。

スケジュール判定は **k_nest が 0 に戻ったとき (最外の割り込みから戻るとき)** のみ行う。
これにより、途中の割り込みでタスクが切り替わる不整合を防ぐ。

ネスト時はカーネルスタック上に複数の割り込みフレームが積まれる。
ただし外側 (Ring 3→Ring 0) では CPU が SS/ESP を含む 14 ワードの `pt_regs` フレームを
形成するのに対し、内側 (Ring 0→Ring 0) では CPU が SS/ESP を push しないため
12 ワードの短いフレームになる:

```
カーネルスタック (ネスト時):
  ┌─────────────────────┐  kern_stack_top (= TSS.esp0)
  │  SS, ESP, EFLAGS,   │
  │  CS, EIP (外側)      │  CPU が push (Ring 3 → Ring 0)
  │  SAVE_ALL (外側)     │  9 レジスタ
  ├─────────────────────┤
  │  EFLAGS, CS, EIP    │  CPU が push (Ring 0 → Ring 0, SS/ESP なし)
  │  SAVE_ALL (内側)     │  9 レジスタ
  │  C 関数のフレーム     │
  └─────────────────────┘  ← ESP
```

---

## 8. システムコール

### 8.1 INT 命令によるソフトウェア割り込み

i386 では `INT n` 命令でソフトウェア割り込みを発生させることができる。
CPU はハードウェア割り込みと同じ手順で IDT を参照し、ハンドラにジャンプする。

tiny-itron では `INT 0x99` をシステムコールに使う:

```c
/* klib.s — ユーザー空間から呼ばれる syscall ラッパー */
syscall:
    pushl   %ebp
    movl    %esp, %ebp
    int     $0x99           /* ベクタ 0x99 → IDT → intr_syscall */
    popl    %ebp
    ret
```

### 8.2 tiny-itron のシステムコールの流れ

```
ユーザータスク                カーネル
     │
     │  syscall(sysid, arg1, arg2, ...)
     │  → int $0x99
     │                           │
     ├──── Ring 3 → Ring 0 ─────→│
     │  (CPU がカーネルスタックに    │
     │   切り替え: TSS.esp0)       │
     │                           │
     │                     intr_syscall:
     │                       SAVE_ALL        ← レジスタ push (pt_regs 構築)
     │                       intr_enter      ← k_nest++
     │                       push %esp       ← pt_regs* を引数に
     │                       call c_intr_syscall(regs)
     │                           │
     │                       regs->esp からユーザスタックを読む
     │                       itron_syscall(apic, sysid, args...)
     │                           │
     │                       regs->eax = ret ← 戻り値を EAX スロットに
     │                           │
     │                       jmp intr_return
     │                         intr_leave    ← k_nest--, タスクスイッチ判定
     │                         RESTORE_ALL   ← レジスタ pop (EAX=戻り値)
     │                         iret          ← Ring 3 に戻る
     │                           │
     ←─── Ring 0 → Ring 3 ─────┤
     │
     │  EAX に戻り値が入っている
```

**引数の渡し方**:
ユーザー空間の `syscall()` はスタックに引数を積んで `INT 0x99` を呼ぶ。
`intr_syscall` (アセンブリ) は `SAVE_ALL` で pt_regs フレームを構築し、
ESP (= pt_regs ポインタ) を引数として `c_intr_syscall` を呼ぶ。
C 関数側で `regs->esp` からユーザスタックを読み出し、引数を取得する。

```asm
/* intr_syscall (intr.s) */
intr_syscall:
    SAVE_ALL                        /* 全レジスタを push → pt_regs 構築 */
    call    intr_enter              /* k_nest++ */
    pushl   %esp                    /* arg: pt_regs* */
    call    c_intr_syscall          /* C ハンドラ */
    addl    $4, %esp                /* 引数クリーンアップ */
    jmp     intr_return             /* intr_leave + RESTORE_ALL + iret */
```

**戻り値の書き込み** (`i386/syscall.c`):
```c
/* pt_regs の EAX スロットに直接書き込む。
 * RESTORE_ALL が EAX を pop するので、タスクは戻り値を受け取る。 */
regs->eax = ret;
```

---

## 9. メモリレイアウト

tiny-itron はページングを有効化しているが、全ページを**恒等マッピング** (VA=PA)
としているため、リニアアドレスと物理アドレスは常に一致する。
ページングの目的は U/S ビットによるメモリ保護のみである
(詳細はセクション 11 参照)。

```
0x00000000 +-------------------------------------------+
           | (base of physical memory)                 |
0x00002000 | GDT (segment table)                       |
0x00002100 | IDT (interrupt table)                     |
0x00003000 | start.s (second-stage loader, 1KB)        |
0x00003400 | run.s + kernel code (.text + .data)       |
           |    ... kernel .bss                        |
0x00010000 | FDC DMA buffer                            |
           |                                           |
~0x01F000  | .user_text + .user_data                   |  <- page-aligned
~0x021000  | Kernel memory pool (_user_data_end)       |  <- kmem_alloc region
           |    (DTQ/MBF/MPF buffers, etc.)            |     Supervisor-only pages
0x00110000 | User memory pool (MEM_START)              |  <- 1 MB + 64 KB
           |    (dynamic memory allocation)            |
0x006FFFFF | (MEM_END)                                 |
0x00700000 | User stack pool (STACK_START)             |
           |    (allocated by cre_tsk)                 |
0x0074FFFF | (STACK_END)                               |
0x00750000 | Per-task kernel stacks                    |  <- KERN_STACK_BASE
           |   (4KB each, Task 1-16 = 16 本)          |  top = BASE + (N+1)*4096
0x00761000 | (end of kernel stack area)                |
           |                                           |
0x00770000 | CPU 1: boot stack (CPU1_SP)               |  <- used only during main()
           |                                           |
0x007A0000 | CPU 0: boot stack (CPU0_SP)               |  <- used only during main()
           |                                           |
0x000B8000 | VGA text buffer (80x25x2 bytes)           |
           |                                           |
0xFEE00000 | Local APIC registers (MMIO)               |
           |                                           |
0xFFFFFFFF +-------------------------------------------+
```

**スタック**は x86 では下方向 (高アドレス→低アドレス) に伸びるので、
`CPU0_SP = 0x7A0000` は初期スタックの**頂上** (最初の push がここから始まる)。

**初期スタック** (`CPU0_SP`, `CPU1_SP`) は `main()` の実行中のみ使用される。
タスク起動後は **per-task カーネルスタック** に切り替わり、初期スタックは使われない。

```
Per-task kernel stacks (KERN_STACK_BASE = 0x750000):

0x750000 +----------------+ Task 0 base (unused)
         |  ^ grows down  | 4KB
0x751000 +----------------+ Task 1 kern_stack_top
         |  ^ grows down  | 4KB
0x752000 +----------------+ Task 2 kern_stack_top
         |  ...           |
0x761000 +----------------+ Task 16 end

top of each task = KERN_STACK_BASE + (tskid + 1) * 4096
TSS.esp0 is overwritten with new task's kern_stack_top on each task switch
```

---

## 10. A20 ライン

**A20 ライン問題**: 8086 は 20 本のアドレスバス (A0〜A19) を持ち、
1 MB (2^20) までアドレスできた。1 MB を超えるアドレスは折り返して
0 番地付近にマップされた (ラップアラウンド)。

80286 以降は 24 本以上のアドレスバスを持つが、互換性のため
**A20 ライン (21 番目のアドレス線) はデフォルトで無効**になっている。
プロテクトモードで 1 MB 以上のメモリにアクセスするには、A20 を有効化する必要がある。

tiny-itron での A20 有効化 (`i386/start.s`):
```asm
a20_init:
    call    a20_init_wait
    movb    $0xD1, %al          /* キーボードコントローラに書き込みコマンド */
    movw    $0x64, %dx
    outb    %al, %dx
    call    a20_init_wait
    movb    $0xDF, %al          /* A20 有効化ビットをセット */
    movw    $0x60, %dx
    outb    %al, %dx
    call    a20_init_wait
    ret
```

これはキーボードコントローラ (i8042) 経由で A20 を有効化する伝統的な方法。
歴史的に、キーボードコントローラが A20 ラインのゲート制御を担当していた。

---

## 11. ページング (恒等マッピング)

i386 はセグメンテーションに加えてページング機構も持つ。
ページングが有効な場合、リニアアドレスはさらに物理アドレスに変換される:

```
論理アドレス  →  セグメンテーション  →  リニアアドレス  →  ページング  →  物理アドレス
(セレクタ:オフセット)
```

tiny-itron では**ページングを有効化している** (CR0 の PG ビット = 1)。
ただし全ページを**恒等マッピング** (VA = PA) として設定しているため、
仮想アドレスと物理アドレスは常に一致する。

### 目的: U/S ビットによるアクセス制御

ページングの主な目的は、仮想アドレス変換ではなく、ページテーブルの
**U/S (User/Supervisor) ビット** を使ったアクセス制御である:

| アドレス範囲 | U/S | アクセス権 | 内容 |
|---|---|---|---|
| 0x00000 〜 `_user_text_start` | Supervisor | Ring 0 のみ | カーネル .text/.data/.bss, GDT/IDT |
| `_user_text_start` 〜 `_user_data_end` | User | Ring 3 からアクセス可 | .user_text + .user_data |
| `_user_data_end` 〜 0x10FFFF | Supervisor | Ring 0 のみ | ギャップ (VGA バッファ 0xB8000 含む) |
| 0x110000 〜 0x74FFFF | User | Ring 3 からアクセス可 | メモリプール + スタックプール |
| 0x750000 〜 0x7FFFFF | Supervisor | Ring 0 のみ | Per-task カーネルスタック + CPU ブートスタック |

`_user_text_start` と `_user_data_end` は `kernel.ld` で定義されるリンカシンボルで、
カーネルサイズの変化に応じて値が変動する。`page.c` の `page_init()` が実行時に
これらのアドレスを読み取り、ページテーブルの U/S ビットを設定する。
カーネルコード (0x3400〜) は Supervisor ページにあるため、Ring 3 からは直接アクセスできない。
ユーザータスクは `.user_text` 内の syscall ラッパーを介して `int $0x99` で Ring 0 に
遷移し、カーネル関数を呼び出す。

### ページテーブル構造

```
page_dir[1024]          ページディレクトリ (CR3 が指す)
  ├── [0] → page_table[0][1024]    0x000000 〜 0x3FFFFF (4MB)
  ├── [1] → page_table[1][1024]    0x400000 〜 0x7FFFFF (4MB)
  └── [0x3FB] → page_table_apic[1024]  0xFEC00000 〜 0xFEFFFFFF
                                         (APIC レジスタ領域、PCD=1)
```

`page_table[0]` と `page_table[1]` で合計 8MB をカバーする。
0x750000 以上のページは Supervisor に設定される。
APIC 領域 (0xFEE00000) は `intr_enter`/`intr_leave` が APIC ID を読むために必要で、
PCD (Page Cache Disable) ビットを立ててキャッシュを無効化する。

### 初期化と有効化

- `page_init()`: BSP が `all_init()` の直後に呼び出す。ページテーブルを構築する。
- `page_enable()`: CR3 にページディレクトリのアドレスをロードし、CR0.PG を 1 にセットする。
  BSP と AP の両方が呼び出す (AP は BSP が構築したページテーブルを共有する)。

---

## 12. 参考: ソースファイルと対応する概念

| ソースファイル | 関連する概念 |
|---------------|-------------|
| `i386/start.s` | リアルモード→プロテクトモード遷移, A20, GDTR/IDTR ロード |
| `i386/run.s` | 32 ビットエントリ, CPU 判定, カーネルスタック設定 |
| `i386/386.c`, `386.h` | GDT ディスクリプタ設定 (`set_gdt`), ゲート設定 (`set_gate`) |
| `i386/addr.h` | セレクタ定数, メモリアドレス定数, スタックアドレス |
| `i386/tss.c`, `tss.h` | TSS 構造体, 初期化 (esp0/ss0 のみ), 動的 esp0 更新 |
| `i386/interrupt.c`, `interrupt.h` | IDT 初期化, IRQ/例外/syscall ハンドラ登録 |
| `i386/interruptP.h` | IDT ポインタ, 割り込みテーブル |
| `i386/intr.s` | SAVE_ALL/RESTORE_ALL (コンテキスト保存/復元), 全割り込みエントリ |
| `i386/i8259.c` | PIC (i8259) 初期化, IRQ マスク, EOI |
| `i386/klib.s` | I/O 命令ラッパー, `cli`/`sti`, `ltr` タスク起動, `syscall` |
| `i386/syscall.c` | システムコールディスパッチ, 戻り値の書き込み |
| `i386/smp.c`, `smpP.h` | Local APIC 初期化, APIC タイマー, IPI, CPU 識別 |
| `i386/proc.c`, `proc.h` | プロセス構造体, レジスタ保存域, CPU アフィニティ |
| `i386/io.h` | I/O ポートアドレス定数 |
