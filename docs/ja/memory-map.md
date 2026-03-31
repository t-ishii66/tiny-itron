# メモリマップ

tiny-itron のメモリ配置を解説するドキュメントである。
カーネルとユーザータスクは **同一のリニアアドレス空間** にフラットに配置されている。
ページング (CR0.PG=1) は有効で、**全恒等マッピング** (仮想アドレス = 物理アドレス)
かつ **U/S ビット** によりカーネル/ユーザーのメモリ保護を行っている。

---

## 目次

1. [全体像](#1-全体像)
2. [ページングの方針](#2-ページングの方針)
3. [セグメント構成 (GDT)](#3-セグメント構成-gdt)
4. [低位メモリ: ブート・テーブル領域](#4-低位メモリ-ブートテーブル領域)
5. [カーネルコード・データとユーザーコード](#5-カーネルコードデータとユーザーコード)
6. [ユーザーメモリプール (mem_alloc)](#6-ユーザーメモリプール-mem_alloc)
7. [カーネルメモリプール (kmem_alloc)](#7-カーネルメモリプール-kmem_alloc)
8. [スタックプール (stack_alloc)](#8-スタックプール-stack_alloc)
9. [CPU ごとのスタック](#9-cpu-ごとのスタック)
10. [VGA テキストバッファ](#10-vga-テキストバッファ)
11. [Local APIC (メモリマップド I/O)](#11-local-apic-メモリマップド-io)
12. [アドレス空間の全体マップ](#12-アドレス空間の全体マップ)

---

## 1. 全体像

tiny-itron のメモリ配置の最大の特徴は以下の通りである。

- **全恒等マッピングのページング** — CR0.PG=1 でページングが有効。
  全ページが仮想アドレス = 物理アドレスの恒等マッピング (identity mapping)。
  ページテーブルの U/S ビットでカーネル/ユーザーのメモリ保護を行う
- **ユーザーコード・データの分離** — ユーザータスクのコードとデータはリンカスクリプト (`kernel.ld`) により
  カーネルとは別の ELF LOAD セグメントに配置される (VMA = LMA)。
  カーネル BSS 直後のページ境界 (0x1E000 付近) に `.user_text` + `.user_data` が連続して配置
- **フラットセグメント** — カーネル・ユーザーとも base=0, limit=4GB のセグメント
- **二重の保護** — Ring 0/Ring 3 の特権レベル (特権命令の制限) に加え、
  ページングの U/S ビットでメモリ保護を実施。カーネルコード・データ領域は
  Supervisor (U/S=0) で Ring 3 からはアクセス不可。ユーザーコード・データ
  (`.user_text` + `.user_data`) とメモリ/スタックプールのみ User (U/S=1)。
  VGA バッファも Supervisor であり、Ring 3 からの画面出力は syscall 経由で行う

```
  0x00000000 ┌──────────────────────────────────────┐
             │  低位メモリ (BIOS, ブートローダ,      │  ┐ ページング:
             │  GDT, IDT, 16bit ブートコード)        │  │ Supervisor
  0x00003400 ├──────────────────────────────────────┤  │ (Ring 3 → #PF)
             │  カーネルコード + データ               │  │
             │  (.text, .rodata, .data, .bss)       │  │
  0x00010000 ├──────────────────────────────────────┤  │
             │  フロッピー DMA バッファ               │  ┘
  〜0x1E000  ├══════════════════════════════════════┤
             │  ユーザーコード (.user_text)          │  ┐ ページング:
             │  ユーザーデータ (.user_data)          │  │ User + RW
  〜0x1F000  ├══════════════════════════════════════┤  ┘ (PTE_USER)
             │  カーネルメモリプール (kmem_alloc)    │  ┐ ページング:
             │  (〜0x20000 〜 0x110000, 約 960 KB)  │  │ Supervisor
  0x000B8000 ├──────────────────────────────────────┤  │ (Ring 3 → #PF)
             │  VGA テキストバッファ (4 KB)           │  │
             │  (未使用)                             │  ┘
  0x00110000 ├══════════════════════════════════════┤
             │  メモリプール (mem_alloc, 約 5.9 MB)  │  ┐ ページング:
  0x00700000 ├──────────────────────────────────────┤  │ User + RW
             │  スタックプール (stack_alloc, 320 KB) │  │ (PTE_USER)
  0x00750000 ├══════════════════════════════════════┤  ┘
             │  CPU 1 Ring 3 / Ring 0 / 起動スタック │  ┐ ページング:
  0x00780000 ├──────────────────────────────────────┤  │ Supervisor
             │  CPU 0 Ring 3 / Ring 0 / 起動スタック │  │ (Ring 3 → #PF)
  0x007A0000 ├──────────────────────────────────────┤  ┘
             │                                      │
             │          (未マッピング領域)           │
             │                                      │
  0xFEE00000 ├──────────────────────────────────────┤ APIC_BASE
             │  Local APIC レジスタ (4 KB)           │  Supervisor + cache-disable
  0xFEE01000 ├──────────────────────────────────────┤
             │                                      │
  0xFFFFFFFF └──────────────────────────────────────┘
```

> **注意:** 上図はおおむねアドレス順だが、VGA バッファ (0xB8000) と
> FDC バッファ (0x10000) はカーネル領域内に重なっている。
> 正確な配置は [12. アドレス空間の全体マップ](#12-アドレス空間の全体マップ) を参照。

---

## 2. ページングの方針

### i386 のページング概要

i386 のページングは CR0.PG=1 で有効になり、CPU がメモリアクセスする際に
ページディレクトリ → ページテーブルを参照して仮想アドレスを物理アドレスに変換する。
各ページテーブルエントリ (PTE) にはアクセス制御のビットがある:

| ビット | 名前 | 意味 |
|--------|------|------|
| P (bit 0) | Present | ページが物理メモリに存在する (=1) |
| R/W (bit 1) | Read/Write | 1=読み書き可、0=読み取り専用 |
| U/S (bit 2) | User/Supervisor | 1=Ring 3 からアクセス可、0=Ring 0 のみ |
| PCD (bit 4) | Cache Disable | 1=キャッシュ無効 (MMIO 用) |

### tiny-itron のページング方針

tiny-itron は **全恒等マッピング** (identity mapping) でページングを使用する。
仮想アドレスと物理アドレスは常に一致し、ページスワッピングは行わない。
ページングの目的は **U/S ビットによるメモリ保護のみ** である。

```
ページディレクトリ (CR3 → page_dir[])
  ├── entry[0] → page_table[0]   0x000000 - 0x3FFFFF (4 MB)
  ├── entry[1] → page_table[1]   0x400000 - 0x7FFFFF (4 MB)
  ├── entry[2..0x3FA] → not present
  ├── entry[0x3FB] → page_table_apic  Local APIC (0xFEE00000)
  └── entry[0x3FC..0x3FF] → not present
```

### アクセス制御の区分

| 物理アドレス範囲 | PTE フラグ | Ring 3 | 内容 |
|-----------------|-----------|--------|------|
| 0x000000 - `_user_text_start` | Supervisor + RW | **#PF** | カーネル .text/.bss/GDT/IDT |
| `_user_text_start` - `_user_data_end` | User + RW | アクセス可 | .user_text + .user_data |
| `_user_data_end` - 0x10FFFF | Supervisor + RW | **#PF** | kmem プール + VGA + 未使用ギャップ |
| 0x110000 - 0x74FFFF | User + RW | アクセス可 | メモリプール + スタックプール |
| 0x750000 - 0x7FFFFF | Supervisor + RW | **#PF** | CPU スタック (Ring 0, TSS) |
| 0xFEE00000 | Supervisor + PCD | **#PF** | Local APIC MMIO |

U/S の境界は **3 箇所** に分かれている。`page_init()` はリンカシンボル
`_user_text_start` / `_user_data_end` と定数 `MEM_START` (0x110000) /
`USER_MEM_END` (0x750000) を使って、以下の 2 つの User 領域を設定する:

1. **ユーザーコード・データ**: `_user_text_start` 〜 `_user_data_end`
   (ページ境界に切り上げ)
2. **メモリ/スタックプール**: `MEM_START` (0x110000) 〜 `USER_MEM_END` (0x750000)

それ以外はすべて Supervisor である。

### カーネル/ユーザー分離の設計

カーネル領域 (0x000000〜`_user_text_start`) は Supervisor (U/S=0) であり、
Ring 3 のユーザータスクからは直接アクセスできない。これを実現するために
以下の設計変更が行われた:

1. **VGA アクセスの syscall 化** — ユーザータスクは `vga_write_at()` を
   直接呼び出さず、`print_at()`, `print_dec_at()`, `clear_screen()`,
   `fill_at()` 等の syscall ラッパ (`lib/lib_exd.c`) を使用する。
   これらは `INT 0x99` 経由でカーネルの `sys_vga_write()` 等を呼び出す
2. **共有データの `.user_data` 配置** — `shared_count` や `task_count[]` は
   カーネル `.bss` ではなく `.user_data` セクション
   (`__attribute__((section(".user_data")))`) に配置される。
   `.user_data` は User ページにあるため Ring 3 からアクセス可能
3. **カーネルリソースの syscall 化** — `stack_alloc()`,
   `key_dtq_id` 設定もすべて syscall ラッパ (`tsk_stack_alloc()`,
   `set_key_task()`) 経由でアクセスする
4. **syscall ラッパの仕組み** — `.user_text` 内の `syscall()` 関数
   (`klib.s`) が `INT 0x99` を発行し、カーネル Ring 0 のシステムコールハンドラに
   遷移する。カーネル `.text` への直接 `call` は行わない

### ページングの初期化と有効化

```c
/* i386/page.c — page_init() でページテーブル構築 */
extern char _user_text_start, _user_data_end;  /* リンカシンボル */

unsigned long u_start = (unsigned long)&_user_text_start & ~(PAGE_SIZE - 1);
unsigned long u_end   = ((unsigned long)&_user_data_end + PAGE_SIZE - 1)
                        & ~(PAGE_SIZE - 1);

for (i = 0; i < PAGE_TABLE_COUNT; i++) {
    for (j = 0; j < PAGES_PER_TABLE; j++) {
        unsigned long addr = (i * PAGES_PER_TABLE + j) * PAGE_SIZE;
        unsigned long flags = PTE_PRESENT | PTE_RW;

        if (addr >= u_start && addr < u_end)
            flags |= PTE_USER;   /* .user_text + .user_data */
        else if (addr >= MEM_START && addr < USER_MEM_END)
            flags |= PTE_USER;   /* memory + stack pools */

        page_table[i][j] = addr | flags;
    }
}

/* i386/page.c — page_enable() で CR3 ロード + CR0.PG=1 */
void page_enable(void) {
    unsigned long dir = page_get_dir();
    __asm__ volatile(
        "movl %0, %%cr3\n\t"
        "movl %%cr0, %%eax\n\t"
        "orl $0x80000000, %%eax\n\t"   /* PG ビットを設定 */
        "movl %%eax, %%cr0\n\t"
        : : "r"(dir) : "eax"
    );
}
```

上記コードにより `page_table[i][j]` には以下の値が格納される
(現在のビルドでは `_user_text_start`=0x1E000, `_user_data_end`=0x1F820):

```
page_table[0][j] — 0x000000〜0x3FFFFF (4MB)
 j       アドレス範囲              flags   エントリ値          内容
 ─────── ──────────────────────── ─────── ─────────────────── ──────────────────
 0x000   0x000000                 0x03    0x00000003          カーネル (Supervisor)
 0x001   0x001000                 0x03    0x00001003           :
   :         :                      :         :               :
 0x01D   0x01D000                 0x03    0x0001D003          カーネル .bss 末尾付近
 0x01E   0x01E000                 0x07    0x0001E007          ← _user_text_start (User)
 0x01F   0x01F000                 0x07    0x0001F007          .user_text + .user_data
 0x020   0x020000                 0x03    0x00020003          ← gap (Supervisor)
   :         :                      :         :               :
 0x10F   0x10F000                 0x03    0x0010F003          gap 末尾
 0x110   0x110000                 0x07    0x00110007          ← MEM_START (User)
 0x111   0x111000                 0x07    0x00111007          mem pool + stack pool
   :         :                      :         :               :
 0x3FF   0x3FF000                 0x07    0x003FF007          mem pool (User)

page_table[1][j] — 0x400000〜0x7FFFFF (4MB)
 j       アドレス範囲              flags   エントリ値          内容
 ─────── ──────────────────────── ─────── ─────────────────── ──────────────────
 0x000   0x400000                 0x07    0x00400007          mem pool (User)
   :         :                      :         :               :
 0x34F   0x74F000                 0x07    0x0074F007          stack pool 末尾 (User)
 0x350   0x750000                 0x03    0x00750003          ← KERN_STACK_BASE (Supervisor)
 0x351   0x751000                 0x03    0x00751003          per-task カーネルスタック
   :         :                      :         :               :
 0x3FF   0x7FF000                 0x03    0x007FF003          CPU スタック (Supervisor)

page_table_apic[j] — 0xFEC00000〜0xFFFFFFFF (4MB, page_dir[0x3FB])
 j       アドレス                  flags   エントリ値          内容
 ─────── ──────────────────────── ─────── ─────────────────── ──────────────────
 0x000   0xFEC00000               0x00    0x00000000          not present
   :         :                      :         :               :
 0x1FF   0xFEDFF000               0x00    0x00000000          not present
 0x200   0xFEE00000               0x13    0xFEE00013          ← Local APIC MMIO (Supervisor)
 0x201   0xFEE01000               0x00    0x00000000          not present
   :         :                      :         :               :

 page_dir[2]〜page_dir[0x3FA] は全て 0 (not present)。
 0x800000〜0xFEBFFFFF のアクセスは #PF になる。

flags: 0x03 = PTE_PRESENT|PTE_RW (Supervisor)
       0x07 = PTE_PRESENT|PTE_RW|PTE_USER (User)
       0x13 = PTE_PRESENT|PTE_RW|PTE_PCD (Supervisor, キャッシュ無効)
```

`page_enable()` は BSP (CPU 0) と AP (CPU 1) の両方で `main()` 内から呼ばれる。
全恒等マッピングのため、CR0.PG を立てた直後の命令も同じアドレスで実行が継続する。
コンテキストスイッチ時に CR3 を切り替える必要はない (全タスクが同一ページテーブルを共有)。

---

## 3. セグメント構成 (GDT)

GDT はリニアアドレス `0x2000` に配置され、`genasm.c` でビルド時に生成される。

### 主要なセグメント

| セレクタ | 名前 | Base | Limit | DPL | 用途 |
|----------|------|------|-------|-----|------|
| 0x08 | SEL_K16_C | 0x3000 | 0xFFFF | 0 | 16bit カーネルコード (ブート時) |
| 0x10 | SEL_K16_D | 0xB8000 | 0xFFFF | 0 | 16bit データ (VGA ベース) |
| 0x18 | SEL_K16_S | 0x3000 | 0x0 | 0 | 16bit スタック (ブート時) |
| **0x20** | **SEL_K32_C** | **0x0** | **4GB** | **0** | **32bit カーネルコード** |
| **0x28** | **SEL_K32_D** | **0x0** | **4GB** | **0** | **32bit カーネルデータ** |
| **0x30** | **SEL_K32_S** | **0x0** | **4GB** | **0** | **32bit カーネルスタック** |
| 0x38 | SEL_TSS0 | (実行時) | sizeof(tss_t) | 0 | CPU 0 TSS (esp0/ss0 のみ使用) |
| 0x40 | SEL_TSS1 | (実行時) | sizeof(tss_t) | 0 | CPU 1 TSS (esp0/ss0 のみ使用) |
| 0x48 | (未使用) | - | - | - | - |
| 0x50 | (未使用) | - | - | - | - |
| **0x58** | **SEL_U32_C** | **0x0** | **4GB** | **3** | **32bit ユーザーコード** |
| **0x60** | **SEL_U32_D** | **0x0** | **4GB** | **3** | **32bit ユーザーデータ** |
| **0x68** | **SEL_U32_S** | **0x0** | **4GB** | **3** | **32bit ユーザースタック** |

### フラットモデルの意味

カーネルセグメント (0x20/0x28/0x30) もユーザーセグメント (0x58/0x60/0x68) も
**base=0, limit=4GB** である。つまり:

```
カーネルの CS=0x20 で見えるアドレス 0x3400
     ＝ ユーザーの CS=0x58 で見えるアドレス 0x3400
     ＝ 物理アドレス 0x3400
```

両者の **唯一の違いは DPL (Descriptor Privilege Level)** である。
DPL=0 のセグメントは Ring 0 でのみ使用でき、DPL=3 のセグメントは
Ring 3 で使用される。しかしアドレスの **範囲は完全に同一** である。

---

## 4. 低位メモリ: ブート・テーブル領域

```
0x0000 ┌─────────────────────────────┐
       │  リアルモード IVT + BIOS    │  (CPU が使用、OS は触らない)
0x2000 ├─────────────────────────────┤ AL_GDT
       │  GDT (256 バイト)           │  genasm.c で生成 → start.s で lgdt
0x2100 ├─────────────────────────────┤ AL_IDT
       │  IDT (空。実行時に idt_init │  で書き込み)
0x3000 ├─────────────────────────────┤ AL_KERNEL16
       │  start.s (16bit ブートコード)│  A20 有効化, GDT/IDT ロード
       │  table.s (GDT バイト列)     │  → CR0.PE=1 → プロテクトモード
0x3400 ├─────────────────────────────┤ kernel.ld: ". = 0x3400"
       │  (カーネルコード開始)        │
```

- `AL_GDT (0x2000)`: start.s の `lgdt` で CPU にロードされる
- `AL_IDT (0x2100)`: start.s の `lidt` でロード後、`idt_init()` で 256 エントリを設定
- `AL_KERNEL16 (0x3000)`: AP (CPU 1) が SIPI で起動する開始アドレスでもある
  (SIPI vector=0x03 → 0x03 × 0x1000 = 0x3000)

### なぜこのアドレスになるのか — `.org` とイメージ連結

上記のアドレスは偶然ではなく、**ビルド時の `.org` ディレクティブ** と
**`cat` によるバイナリ連結** の結果として決まる。

Makefile は 4 つのバイナリファイルを `cat` で単純に連結してフロッピーイメージを
作る (`cat boot/boot table start kernel > i386`)。`cat` は各ファイルをそのまま
つなげるだけなので、**各ファイルのサイズがそのまま後続ファイルのオフセットを決定する。**
サイズが 1 バイトでもずれれば、以降のすべてのアドレスが狂う。

これを防ぐため、3 つのファイルは `.org` でサイズを固定している:

| ファイル | `.org` | 固定サイズ | ソース |
|----------|--------|-----------|--------|
| boot/boot | `.org 510` + `.word 0xAA55` | 512 B | `boot/boot.s` |
| table | `.org 4096` | 4096 B (4 KB) | `genasm.c` が生成する `table.s` |
| start | `.org 1024` | 1024 B (1 KB) | `start.s` |

ブートセクタ (boot.s) がセクタ 1 以降をリニアアドレス 0x2000 に順番に
読み込むと、各ファイルは以下の位置に配置される:

```
0x2000 + 0            = 0x2000  ← table (4096 B)   = AL_GDT
0x2000 + 4096         = 0x3000  ← start (1024 B)   = AL_KERNEL16
0x2000 + 4096 + 1024  = 0x3400  ← kernel           = kernel.ld の ". = 0x3400"
```

リンカスクリプト `kernel.ld` の開始アドレス (`. = 0x3400`) はこの計算結果と一致する
必要があり、逆に言えば table や start の `.org` 値を変更したら `kernel.ld` も
変更しなければカーネルのアドレスが壊れる。
詳細は [boot-sector.md のセクション 10](boot-sector.md#10-フロッピーディスクイメージの構造) を参照。

---

## 5. カーネルコード・データとユーザーコード

### リンカスクリプト (`kernel.ld`)

リンカスクリプト `i386/kernel.ld` により、カーネルとユーザーコードは
**2 つの LOAD セグメント** として ELF バイナリに配置される。
全セクションが **VMA = LMA** (恒等マッピング) で配置される。

```
LOAD セグメント  セクション                        VMA = LMA  フラグ
───────────────────────────────────────────────────────────────────────
kernel           .text .rodata .data .bss           0x3400     RWX
user             .user_text .user_data              〜0x1E000  RWX
```

### カーネルセグメント (VMA = LMA = 0x3400)

カーネルの `.text`, `.rodata`, `.eh_frame`, `.data`, `.bss` は
アドレス 0x3400 から連続して配置される。
`.bss` にはカーネルグローバル変数 (`proc[]`, `tsk[]`, `current_proc[]`,
ロック変数等) が含まれる。カーネルページは Supervisor (U/S=0) であり、
Ring 3 のユーザータスクからは直接アクセスできない。

カーネルセグメントのサイズは約 105 KB 程度 (filesz=0xF15C) で、
BSS を含む memsz は約 0x1A208 (106 KB)。BSS 末尾は 0x1D608 付近に達する。

### ユーザーセグメント (.user_text + .user_data)

ユーザータスクのコードは `__attribute__((section(".user_text")))` で
`.user_text` セクションに、ユーザーデータは `__attribute__((section(".user_data")))`
で `.user_data` セクションに配置される。カーネル BSS 直後のページ境界
(0x1E000) に `.user_text` → `.user_data` の順で配置され、VMA = LMA で
恒等マッピングされる。これらのページのみ User (U/S=1) である。

`.user_text` に含まれるもの:

| 種別 | 関数 / 内容 |
|------|-------------|
| ユーザータスク | `first_task`, `second_task`, `usr_main`, `kbd_task`, `idle_task` |
| ヘルパ | `delay`, `draw_header` |
| ITRON syscall ラッパ | `syscall` (汎用), `cre_tsk`, `act_tsk`, `slp_tsk`, `wup_tsk`, `cre_sem`, `pol_sem`, `sig_sem`, `cre_dtq`, `rcv_dtq`, `cre_mbf`, `psnd_mbf`, `trcv_mbf` 等 (klib.s) |
| 拡張 syscall ラッパ | `print_at`, `print_dec_at`, `clear_screen`, `fill_at`, `set_key_task`, `tsk_stack_alloc` (lib/lib_exd.c) |
| libc 関数 | `libc.a` 内の `.text` セクション |

`.user_data` に含まれるもの:

| 種別 | 変数 / 内容 |
|------|-------------|
| 共有カウンタ | `shared_count` — セマフォで保護された共有カウンタ |
| タスク統計 | `task_count[]` — タスクごとの実行回数 |

**カーネル関数へのアクセス**: ユーザータスクはカーネルの `vga_write_at()`,
`stack_alloc()` 等を直接呼び出さない。代わりに `print_at()`,
`tsk_stack_alloc()` 等の syscall ラッパ (`lib/lib_exd.c`) を
使用する。これらは `INT 0x99` 経由でカーネル Ring 0 に遷移する。
キーボード入力は DTQ 2 (`ipsnd_dtq`/`rcv_dtq`) で ISR → kbd_task に文字を渡し、
kbd_task → first_task の行転送には MBF 1 (`psnd_mbf`/`trcv_mbf`) を使う

### ELF → フラットバイナリ変換 (`elf.c`)

ホスト側ビルドツール `elf.c` は、ELF の全 PT_LOAD セグメントを
**LMA (p_paddr) 順** に処理してフラットバイナリを生成する。
セグメント間のギャップはゼロパディングで埋められる。

```
フラットバイナリ内のレイアウト:
  オフセット 0                       カーネル .text + .rodata + .data (filesz=0xF15C)
  〜0xF15C                           gap = 47,780 バイト (BSS 分のゼロパディング)
  〜0x1AC00 (= 0x1E000 - 0x3400)     ユーザーコード .user_text + .user_data
  合計: 約 113 KB (ビルドにより変動)
```

### ブートローダのセクタ制限

boot.s はフロッピーのセクタ 1〜299 を読み込む (`cmpw $300`)。
これにより table + start + kernel を最大 約 150 KB まで格納できる。
カーネルバイナリが成長した場合はこの値を増やす必要がある。

---

## 6. ユーザーメモリプール (mem_alloc)

ユーザータスクが直接アクセスするメモリの動的割当に使われる領域。
`pool.c` の `mem_alloc()` / `mem_free()` で管理される。
このプールは **PTE_USER ページ** にあるため、Ring 3 のユーザータスクから
読み書き可能である。

| 定数 | 値 | 意味 |
|------|-------|------|
| MEM_START | 0x110000 | プール開始アドレス |
| MEM_END | 0x6FFFFF | プール終了アドレス |
| サイズ | 約 5.8 MB | 0x6FFFFF - 0x110000 + 1 = 0x5F0000 = 6,225,920 bytes |
| アライメント | 4 KB | MEM_ALIGN = 0xFFFFF000 (定義元: `include/stdio.h`。`~0xFFF` で下位 12 ビットをマスクし 4 KB 境界にアライン) |

> **注意**: MEM_START (0x110000) はカーネル + ユーザーコードの末尾 (約 0x1EDDB)
> よりもはるか上にあるため、衝突しない。恒等マッピングで VMA=LMA なので
> ユーザーコードは 0x1E000 付近に配置されており、0x110000 までは十分な余裕がある。
> この間の Supervisor ギャップにカーネルメモリプール (kmem_alloc) が配置される
> ([7. カーネルメモリプール](#7-カーネルメモリプール-kmem_alloc) を参照)。

割当はファーストフィット方式で、`mem_pool[]` 配列 (最大 256 エントリ) で
空き/使用中を管理する。カーネル内部では以下の用途で `mem_alloc()` が使われる:

- `cre_mpf` — 固定長メモリプールの **ブロック領域** (`get_mpf` でユーザーに返すポインタ)
- `cre_mpl` — 可変長メモリプールの **プール領域** (`get_mpl` でユーザーに返すポインタ)

`get_mpf` / `get_mpl` が返すポインタはこの領域内のアドレスであり、
ユーザータスクが直接読み書きする。そのため PTE_USER ページに配置する必要がある。

> **注意**: カーネル内部バッファ (DTQ/MBF リングバッファ、MBX 優先度ヘッダ、
> MPF/MPL 管理メタデータ) はユーザーに公開する必要がないため、
> カーネルメモリプール (`kmem_alloc`) から確保される。
> これにより Ring 3 からの不正な上書きを防止している。

---

## 7. カーネルメモリプール (kmem_alloc)

カーネル内部専用のメモリ割当領域。`pool.c` の `kmem_alloc()` / `kmem_free()` で
管理される。このプールは **Supervisor ページ** (U/S=0) にあるため、
Ring 3 のユーザータスクからはアクセスできない (#PF が発生する)。

| 項目 | 値 | 意味 |
|------|-------|------|
| 開始 | `_user_data_end` → 0x20000 | pool_init が 4KB 境界に切り上げ |
| 終了 | MEM_START = 0x110000 | ユーザーメモリプールの直前 |
| サイズ | 約 960 KB | 0x110000 - 0x20000 = 0xF0000 = 983,040 bytes |
| アライメント | 4 KB | MEM_ALIGN = 0xFFFFF000 |

### 用途

カーネルオブジェクトの内部バッファなど、ユーザータスクに公開する必要のない
メモリを確保する。ITRON 仕様では syscall 経由でのみアクセスされるバッファは
カーネル内部データであり、Ring 3 から保護すべきである。

| 呼び出し元 | 用途 | サイズ |
|------------|------|--------|
| `cre_dtq` | DTQ リングバッファ | `dtqcnt * sizeof(VW)` |
| `cre_mbf` | MBF リングバッファ | `mbfsz` |
| `cre_mbx` | メールボックス優先度ヘッダ配列 | `maxmpri * sizeof(T_MSG*)` |
| `cre_mpf` | MPF 管理メタデータ (`allocation_t` 配列) | `blkcnt * sizeof(allocation_t)` |
| `cre_mpl` | MPL 管理メタデータ (`allocation_t` 配列) | `MAX_MPL_POOL * sizeof(allocation_t)` |

### mem_alloc との使い分け

| プール | ページ属性 | Ring 3 | 用途 |
|--------|-----------|--------|------|
| `mem_alloc` | PTE_USER | アクセス可 | ユーザーに返すメモリ (MPF ブロック、MPL プール) |
| `kmem_alloc` | Supervisor | **#PF** | カーネル内部バッファ (DTQ, MBF, MBX, 管理メタデータ) |

### 初期化

`itron_init()` 内で `mem_init()` の直後に呼ばれる:

```c
extern char _user_data_end;
kmem_init((VP)&_user_data_end, (VP)MEM_START);
```

`pool_init()` が `_user_data_end` (現在 0x1F820 付近) を 4KB 境界に
切り上げて 0x20000 にアラインする。終端は MEM_START (0x110000) そのまま。
割当はファーストフィット方式で、`kmem_pool[]` 配列 (最大 256 エントリ) で管理する。

### SMP 安全性

`kmem_alloc` / `kmem_free` は呼び出し元の syscall ハンドラが `kernel_lk`
(Big Kernel Lock) を保持した状態で呼ばれるため、独自のスピンロックは持たない。

---

## 8. スタックプール (stack_alloc)

タスク生成時 (`cre_tsk`) に渡すスタック領域の割当に使われる。
カーネル内部の割当関数は `pool.c` の `stack_alloc()` / `stack_free()` である。
ユーザータスク (Ring 3) からは同名の関数を直接呼べないため、
syscall ラッパー `tsk_stack_alloc()` (`lib/lib_exd.c`) を経由して呼び出す:

```
ユーザータスク (Ring 3)          カーネル (Ring 0)
tsk_stack_alloc(1024)  ─INT 0x99→  sys_stack_alloc_sc()  →  stack_alloc(1024)
```

| 定数 | 値 | 意味 |
|------|-------|------|
| STACK_START | 0x700000 | プール開始アドレス |
| STACK_END | 0x74FFFF | プール終了アドレス |
| サイズ | 320 KB | 0x74FFFF - 0x700000 + 1 = 0x50000 = 327,680 bytes = 320 KB |
| アライメント | 4 KB | |

### タスクスタックの割当例

```c
/* kernel/user.c — first_task() でのタスク 3 生成 */
ctsk.stk   = tsk_stack_alloc(1024);   /* syscall ラッパー経由でスタックプールから 1024 バイト確保 */
ctsk.stksz = 1024;
cre_tsk(3, &ctsk);
```

`proc_create()` 内で ESP はスタック領域の **上端** (高いアドレス側) に設定される。
x86 のスタックは高アドレスから低アドレスに向かって成長するためである。

```c
/* i386/proc.c — proc_create() */
unsigned long user_esp = ((unsigned long)pk_ctsk->stk + pk_ctsk->stksz) & ~3UL;
```

### カーネルとタスクスタックの関係

```
  0x700000  ┌─────────────┐ STACK_START
            │ Task 1 stk  │ ← proc_init() → stack_alloc(1024)
  0x700400  ├─────────────┤
            │ Task 2 stk  │ ← proc_init() → stack_alloc(1024)
  0x700800  ├─────────────┤
            │ Task 5 stk  │ ← proc_init() → stack_alloc(1024)  (idle, CPU 0)
  0x700C00  ├─────────────┤
            │ Task 6 stk  │ ← proc_init() → stack_alloc(1024)  (idle, CPU 1)
  0x701000  ├─────────────┤
            │ Task 3 stk  │ ← first_task() → tsk_stack_alloc(1024)
  0x701400  ├─────────────┤
            │ Task 4 stk  │ ← second_task() → tsk_stack_alloc(1024)
  0x701800  ├─────────────┤
            │   (空き)     │
            │     ...      │
  0x74FFFF  └─────────────┘ STACK_END
```

> 全タスクのユーザースタックは `stack_alloc()` でスタックプールから順番に割り当てられる。
> `proc_init()` が Task 1, 2, 5, 6 の順で呼ぶため、先頭 4 スロットはこれらが占有する。
> Task 3, 4 はユーザーコード (`first_task()`, `second_task()`) が `cre_tsk` で
> 生成する際に `tsk_stack_alloc()` (syscall ラッパー → カーネル内 `stack_alloc()`) で
> スタックプールの続きから割り当てる。

---

## 9. スタック構成 (SMP + Per-Task Kernel Stack)

### 起動時スタック

SMP では CPU ごとに独立した起動時スタックが必要である。`addr.h` で定義されている。

| アドレス | 定数 | 用途 |
|----------|------|------|
| 0x7A0000 | CPU0_SP | BSP 起動時スタック (run.s で ESP に設定) |
| 0x770000 | CPU1_SP | AP 起動時スタック (run.s で ESP に設定) |

これらは `main()` の呼び出しやカーネル初期化に使われ、
`start_first_task()`/`start_second_task()` の `ltr` + `RESTORE_ALL` + `iret` でタスクが開始した後は使用されない。

### ユーザースタック (全タスク共通)

全タスクのユーザースタックはスタックプール (0x700000〜0x74FFFF) から `stack_alloc()` で割り当てる。
Task 1/2 は `proc_init()` 内で、Task 3〜6 はユーザーコードの `cre_tsk` 経由で割り当てる。

### タスクごとのカーネルスタック

各タスクは 4KB のカーネルスタックを持つ。`addr.h` で定義:

```c
#define KERN_STACK_SIZE   4096       /* 4KB per task */
#define KERN_STACK_BASE   0x750000   /* base of kernel stack area */
```

タスク N のカーネルスタック先頭 = `KERN_STACK_BASE + (N+1) * KERN_STACK_SIZE`

| タスク ID | カーネルスタック範囲 | kern_stack_top |
|-----------|---------------------|----------------|
| 1 | 0x752000〜0x751001 | 0x752000 |
| 2 | 0x753000〜0x752001 | 0x753000 |
| 3 | 0x754000〜0x753001 | 0x754000 |
| ... | ... | ... |

TSS の esp0 フィールドは、現在実行中のタスクの `kern_stack_top` に
動的に更新される (`tss_update_esp0()`)。Ring 3 → Ring 0 の割り込み時、
CPU は TSS.esp0 から ESP を読み、そのタスクのカーネルスタックに自動的に切り替える。

### 3 種類のスタックの役割

```
ユーザータスク (Ring 3)
  │  ESP → 各タスクの Ring 3 スタック (SP3 or tsk_stack_alloc)
  │
  │  ─── INT 0x99 (syscall) や IRQ 発生 ───
  │  CPU が TSS.esp0 からタスクのカーネルスタックに自動切替
  ▼
カーネル (Ring 0)
  │  ESP → そのタスクの kern_stack_top (タスクごとに異なる)
  │  SAVE_ALL でレジスタを push、C ハンドラ実行
  │  タスクスイッチ時は intr_leave が ESP を新タスクのスタックに切り替え
  │
  │  ─── RESTORE_ALL + iret ───
  │  レジスタを pop、Ring 3 の SS/ESP を復元
  ▼
ユーザータスク (Ring 3) に復帰
```

---

## 10. VGA テキストバッファ

| 定数 | 値 | 意味 |
|------|------|------|
| G_BASE | 0xB8000 | VGA テキストバッファの先頭アドレス |
| サイズ | 80 × 25 × 2 = 4000 バイト | |

1 文字 = 2 バイト (ASCII コード + 属性バイト) で構成される。

```
0xB8000 + (row * 80 + col) * 2 = 文字位置
```

VGA テキストバッファ (0xB8000) のページは **Supervisor (U/S=0)** に設定されている。
Ring 3 のユーザータスクが 0xB8000 に直接書き込むと #PF (page fault) が発生する。

ユーザータスクからの画面出力は **syscall 経由** で行う:

| ユーザー側関数 | syscall 番号 | カーネル側実装 |
|---------------|-------------|---------------|
| `print_at()` | TFN_EXD_VGA_WRITE | `sys_vga_write()` |
| `print_dec_at()` | TFN_EXD_VGA_DEC | `sys_vga_write_dec()` |
| `clear_screen()` | TFN_EXD_VGA_CLEAR | `sys_vga_clear()` |
| `fill_at()` | TFN_EXD_VGA_FILL | `sys_vga_fill_at()` |

これらの syscall ラッパは `lib/lib_exd.c` に定義され、`INT 0x99` で
カーネル Ring 0 に遷移して VGA バッファに書き込む。カーネル Ring 0 コードは
Supervisor ページにもアクセスできるため、0xB8000 への書き込みが可能である。

---

## 11. Local APIC (メモリマップド I/O)

SMP で使用する Local APIC レジスタは物理アドレス `0xFEE00000` にマップされている。
ページングは恒等マッピング (仮想=物理) のため、このアドレスに直接読み書きするだけで
APIC にアクセスできる。APIC ページは Supervisor + cache-disable に設定されている。

| アドレス | レジスタ | 用途 |
|----------|----------|------|
| 0xFEE00020 | APIC_ID | CPU 識別番号 (ビット 24〜31) |
| 0xFEE000B0 | APIC_EOI | 割り込み完了通知 (0 を書き込む) |
| 0xFEE000F0 | APIC_SVR | APIC 有効化 |
| 0xFEE00300 | APIC_ICR_LOW | IPI 送信 (低位) |
| 0xFEE00310 | APIC_ICR_HIGH | IPI 送信 (高位) |
| 0xFEE00320 | APIC_LVT_TIMER | APIC タイマー設定 |
| 0xFEE00380 | APIC_TIMER_INIT_COUNT | タイマー初期値 |
| 0xFEE003E0 | APIC_TIMER_DIV | タイマー分周比 |

2 つの CPU が同じ物理アドレス `0xFEE00000` を読むが、
各 CPU は **自分自身の** Local APIC にアクセスする (ハードウェアが CPU ごとに振り分ける)。
例えば CPU 0 が `APIC_ID` を読むと 0 が、CPU 1 が読むと 1 が返る。

---

## 12. アドレス空間の全体マップ

物理アドレス順の正確なメモリマップを以下に示す。
ページングは全恒等マッピングのため、仮想アドレス = 物理アドレスである。

```
物理アドレス       サイズ      U/S              内容
──────────────────────────────────────────────────────────────────────
0x00000000        8 KB       **Supervisor**    リアルモード IVT + BIOS データ
0x00002000        256 B      **Supervisor**    GDT (AL_GDT)
0x00002100        (〜4 KB)   **Supervisor**    IDT (AL_IDT)
0x00003000        1 KB       **Supervisor**    16bit ブートコード (start.s)
0x00003400        〜105 KB   **Supervisor**    32bit カーネル (.text+.rodata+.data+.bss)
0x00010000        64 KB      **Supervisor**    フロッピー DMA バッファ (FDC_BUFFER) [注1]
0x0001E000        〜3.4 KB   User(RW)          .user_text (ユーザーコード)      [注2]
(続き)            (変動)     User(RW)          .user_data (shared_count 等)     [注3]
0x00020000        960 KB     **Supervisor**    カーネルメモリプール (kmem_alloc) [注4]
0x000B8000        4 KB       **Supervisor**    VGA テキストバッファ (G_BASE)    [注5]
  ...             (未使用)   **Supervisor**
0x00110000        5.9 MB     User(RW)          メモリプール (MEM_START〜MEM_END)
0x00700000        320 KB     User(RW)          スタックプール (STACK_START〜STACK_END)
0x00750000        68 KB      **Supervisor**    タスクごとカーネルスタック (KERN_STACK_BASE, 各4KB×17)
  ...             (未使用)   **Supervisor**    (カーネルスタック末尾 0x761000〜CPU1_SP の間)
0x00770000        64 KB      **Supervisor**    CPU 1 起動時スタック (CPU1_SP, main() のみ)
  ...             (未使用)   **Supervisor**
0x007A0000        (頂点)     **Supervisor**    CPU 0 起動時スタック (CPU0_SP, main() のみ)

> カーネルスタック (0x750000〜) は 16 タスク × 4KB = 64KB。
> 起動時スタックはカーネル初期化のみに使用。タスク実行中は
> 各タスクのカーネルスタックが使われ、TSS.esp0 が動的に更新される。
  ...             (not present)
0xFEE00000        4 KB       **Supervisor+PCD** Local APIC レジスタ (MMIO)
──────────────────────────────────────────────────────────────────────
[注1] FDC_BUFFER はカーネル BSS 領域と重なるが、
      BSS 上の変数配置とは物理的に干渉しない
[注2] .user_text は VMA=LMA で恒等マッピング。
      カーネル BSS 直後のページ境界に配置される (アドレスはビルドにより変動)
[注3] .user_data は .user_text 直後に配置。shared_count, task_count[] 等の
      ユーザータスクが参照するグローバル変数が含まれる
[注4] kmem_alloc プールは _user_data_end (→0x20000 にアライン) 〜 MEM_START (0x110000)。
      Supervisor ページのため Ring 3 からアクセス不可。VGA バッファ (0xB8000) はこの範囲内
[注5] VGA バッファは Supervisor ページ。Ring 3 からの画面出力は syscall 経由
```

### 重要なポイント

- **カーネルとユーザーコードは同じ ELF バイナリ** に含まれるが、
  **別の LOAD セグメント** に分離されている。
  カーネルコード (`.text`) は 0x3400 から、
  ユーザーコード・データ (`.user_text` + `.user_data`) は 0x1E000 付近に配置される。
  **両者とも VMA = LMA** (恒等マッピング) である

- **タスクのスタックは別の場所** (スタックプール 0x700000〜) に確保される

- **ページングでの保護は 2 つの User 領域を設定** する形で実現。
  (1) `.user_text` + `.user_data` (リンカシンボルで動的に決定)、
  (2) メモリ/スタックプール (0x110000〜0x74FFFF)。
  **それ以外はすべて Supervisor** (Ring 0 のみアクセス可)。
  カーネルメモリプール (`kmem_alloc`, 0x20000〜0x110000) は Supervisor ギャップ内にあり、
  Ring 3 からアクセスできない

- **カーネル領域は Supervisor** である。カーネル `.text`, `.bss`, GDT, IDT 等は
  Ring 3 からアクセスできない。ユーザータスクがカーネル機能を利用するには
  `INT 0x99` syscall を使う

- **ユーザー共有データは `.user_data`** に配置される。`shared_count` や
  `task_count[]` は `__attribute__((section(".user_data")))` で
  User ページに配置されるため、Ring 3 のユーザータスクから直接参照できる

- **VGA バッファは Supervisor** である。Ring 3 からの画面出力は
  `print_at()` 等の syscall ラッパ (`lib/lib_exd.c`) 経由で行う。
  カーネル側の `sys_vga_write()` 等が Ring 0 で VGA バッファに書き込む

### ビルド後のアドレスを確認する

`.user_text` の配置アドレスはソース量により変動する (詳細は [build-system.md](build-system.md#リンカスクリプト-kernelld-の詳細) を参照)。
ビルド後に以下のコマンドで実際のアドレスを確認できる:

```bash
# ELF の LOAD セグメント一覧 — user セグメントの VMA を確認
readelf -l i386/_kernel_dbg
#   LOAD  0x00f55c 0x0001e000 0x0001e000 0x0XXXX 0x0XXXX RWE 0x1
#                   ^^^^^^^^^ ← user セグメントの開始アドレス (ビルドにより変動)

# リンカシンボルでピンポイントに確認
nm i386/_kernel_dbg | grep _user_
#   0001e000 T _user_text_start
#   0001ed85 T _user_text_end
#   0001ed88 D _user_data_start
#   0001ed90 D _user_data_end       ← page_init() がこの値を使用
```

> **注意:** `page_init()` はリンカシンボル `_user_text_start` / `_user_data_end`
> を参照して User ページの範囲を動的に決定する。`.user_text` + `.user_data` の
> サイズや配置アドレスが変わっても、`page_init()` が自動的に対応するため
> ページテーブルの手動変更は不要。ただし、メモリ/スタックプールの
> `MEM_START` (0x110000) / `USER_MEM_END` (0x750000) は固定値であり、
> ユーザーセクションがこの範囲に侵入しないことが前提である。

### GDB で確認する

```gdb
# カーネルのシンボルアドレス
(gdb) info address main
(gdb) info address sched_do_next_tsk
# → 0x3400〜0x1XXXX の範囲 (カーネル .text)

# ユーザーコードのシンボルアドレス
(gdb) info address first_task
(gdb) info address kbd_task
# → .user_text 範囲 (readelf -l で確認した範囲内)

# ユーザーデータのシンボルアドレス
(gdb) info address shared_count
(gdb) info address task_count
# → .user_data 範囲 (_user_text_end 以降)

# ページテーブルの内容を確認
(gdb) x/4x page_table         # page_table[0][0..3] — Supervisor (カーネル領域)
(gdb) x/1x page_table+0x1E0   # 0x1E000 付近 — User+RW (.user_text)
(gdb) x/4x page_table+0x110   # 0x110000 付近 — User+RW (メモリプール)
(gdb) x/4x page_table+0x750   # 0x750000 付近 — Supervisor (CPU スタック)

# タスクのカーネルスタックを確認
(gdb) print/x current_proc[0]->kern_esp
# → 0x751XXX〜0x75XXXX (タスクごとカーネルスタック領域内)
(gdb) print/x current_proc[0]->kern_stack_top
# → タスクのカーネルスタック先頭 (TSS.esp0 に設定される値)
```

---

## 参考: 定数の定義場所

| 定数 | 定義ファイル |
|------|-------------|
| AL_GDT, AL_IDT, AL_KERNEL16 | i386/addr.h |
| CPU0_SP | i386/addr.h |
| CPU1_SP | i386/addr.h |
| KERN_STACK_SIZE, KERN_STACK_BASE | i386/addr.h |
| MEM_START (0x110000), MEM_END | i386/addr.h |
| STACK_START, STACK_END | i386/addr.h |
| FDC_BUFFER | i386/addr.h |
| SEL_K32_C, SEL_U32_C 等 | i386/addr.h |
| G_BASE | i386/videoP.h |
| APIC_BASE, APIC_ID 等 | i386/smpP.h |
| `. = 0x3400` (カーネル開始) | i386/kernel.ld |
| `_user_text_start`, `_user_text_end` | i386/kernel.ld |
| `_user_data_start`, `_user_data_end` | i386/kernel.ld |
| USER_MEM_END (0x750000) | i386/pageP.h |
| PAGE_TABLE_COUNT (2), PTE_USER, PTE_RW | i386/page.h, i386/pageP.h |
