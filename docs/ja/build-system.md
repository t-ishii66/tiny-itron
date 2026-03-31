# ビルドシステム解説

tiny-itron のビルドからフロッピーイメージ作成までの全工程を説明する。

---

## 開発環境

Ubuntu 24.04 LTS (amd64) で開発・動作確認している。

### 必要なパッケージ

```bash
# ビルドツール (GCC, GNU Make, binutils)
# gcc-multilib: 64 ビット OS 上で 32 ビット (-m32) コードを生成するために必要
sudo apt install build-essential gcc-multilib

# エミュレータ
sudo apt install qemu-system-x86

# デバッガ (任意)
sudo apt install gdb
```

### 確認済みバージョン

| ツール | バージョン |
|--------|-----------|
| GCC | 13.3.0 |
| GNU Make | 4.3 |
| QEMU | 8.2.2 |
| GDB | 15.0 |

> **注:** GCC はデフォルトで `cmov` 等の Pentium Pro 以降の命令を生成する。
> QEMU の `-cpu` オプションは `pentium3` 以上が必要 (`-cpu 486` は不可)。
> `run.sh` がこのオプションを自動設定する。

---

## ビルドコマンド

```bash
make                # 全体ビルド (kernel → lib → i386 の順)
make clean          # 全体クリーン
```

トップレベルの `Makefile` が 3 つのサブディレクトリを正しい順序で呼び出す。
個別にビルドする場合:

```bash
make -C kernel      # カーネルライブラリ (libkernel.a)
make -C lib         # ユーザーライブラリ (libc.a)
make -C i386        # リンク、バイナリ変換、イメージ生成
```

最終成果物は `i386/i386` (フロッピーイメージ用の生バイナリ)。

---

## ビルドパイプライン全体図

```
  kernel/*.c           lib/*.c
      │                    │
      ▼                    ▼
  libkernel.a          libc.a
      │                    │
      ├────────┬───────────┘
      │        │
      │    i386/*.c, i386/*.s
      │        │
      ▼        ▼
   ld → _kernel (ELF)
            │
        ┌───┤
        │   ▼
        │  strip → _kernel (stripped ELF)
        │   │
        │   ▼
        │  elf → kernel (flat binary)      genasm → table.s → table (GDT binary)
        │   │                                                    │
        │   │           start.s → start (16-bit binary)          │
        │   │               │                                    │
        │   │    boot.s → boot (boot sector, 512 bytes)          │
        │   │       │       │           │                        │
        │   │       ▼       ▼           ▼                        ▼
        │   │      cat boot table start kernel > i386
        │   │                                      │
        │   │                                      ▼
        │   │                          dd → floppy.img (1.44 MB)
        │   │
        ▼   ▼
     _kernel_dbg (GDB 用、strip 前のコピー)
```

---

## 各ステップの詳細

### ステップ 1: kernel/ — ITRON カーネルのコンパイル

```makefile
# kernel/Makefile
CC = gcc
CFLAGS = -Wall -m32 -fno-pie -fno-stack-protector -fno-builtin

OBJS = kernel.o pool.o sys_tsk.o sys_sns.o sys_tex.o syscall.o sched.o user.o \
       sys_sem.o sys_flg.o sys_dtq.o sys_mbx.o sys_mtx.o sys_mbf.o \
       sys_por.o sys_mpf.o sys_mpl.o sys_tim.o sys_cyc.o sys_alm.o \
       sys_ovr.o sys_rdq.o sys_isr.o

all: $(OBJS)
    ar r libkernel.a $(OBJS)
```

**CFLAGS の意味:**
- `-m32` — 32 ビットコード生成 (ホストが 64 ビットでも i386 プロテクトモードに必要)
- `-fno-pie` — 位置独立実行ファイルを無効化。絶対アドレスの `call`/`jmp` を生成させる。
  PIE が有効だと GOT/PLT 経由の間接参照が生成されるが、ベアメタルにはダイナミック
  リンカが存在せず GOT が未解決のままクラッシュする。
  `kernel.ld` の固定アドレスリンク (`. = 0x3400`) が正しく機能するために必須
- `-fno-stack-protector` — スタックカナリア無効化 (`__stack_chk_fail` が存在しない)
- `-fno-builtin` — GCC の組み込み関数を使わない (`memcpy` 等が存在しない)

出力: `kernel/libkernel.a` (静的ライブラリ)

### ステップ 2: lib/ — ユーザーライブラリのコンパイル

```makefile
# lib/Makefile
OBJS = lib_mbf.o lib_tsk.o lib_int.o lib_sem.o lib_tim.o

all: $(OBJS)
    ar r libc.a $(OBJS)
```

出力: `lib/libc.a` (syscall ラッパーの静的ライブラリ)

### ステップ 3: i386/ — リンクとバイナリ生成

#### 3a: genasm — GDT 生成ツール

```makefile
genasm: genasm.c
    $(CC) -o genasm genasm.c    # ホスト上で実行するツール

table.s: genasm
    ./genasm                     # table.s を生成

table: table.o
    $(LD) -m elf_i386 --oformat binary -Ttext 0x0 -o table table.o
```

`genasm.c` はホスト上で動くプログラムで、GDT の各エントリを
`.byte` ディレクティブとして `table.s` に書き出す。
9 つの GDT エントリを生成:

| # | セレクタ | 種類 |
|---|----------|------|
| 1 | 0x08 | 16 ビットカーネルコード |
| 2 | 0x10 | 16 ビットカーネルデータ |
| 3 | 0x18 | 16 ビットカーネルスタック |
| 4 | 0x20 | 32 ビットカーネルコード |
| 5 | 0x28 | 32 ビットカーネルデータ |
| 6 | 0x30 | 32 ビットカーネルスタック |
| 7 | 0x58 | 32 ビットユーザーコード (DPL=3) |
| 8 | 0x60 | 32 ビットユーザーデータ (DPL=3) |
| 9 | 0x68 | 32 ビットユーザースタック (DPL=3) |

TSS エントリ (0x38 = SEL_TSS0, 0x40 = SEL_TSS1) と syscall コールゲート (0x70, SEL_SYSCALL) は
カーネル起動後に `tss_init()` → `set_gdt()` で動的に設定される。
GDT スロット 0x48, 0x50 は現在未使用 (予約済み)。

出力: `table` (4096 バイト。GDT データ自体は 256 バイトだが `.org 4096` でパディング)

#### 3b: elf — ELF 変換ツール

```makefile
elf: elf.c
    $(CC) -o elf elf.c           # ホスト上で実行するツール
```

`elf.c` は ELF バイナリの PT_LOAD セグメントを LMA (物理アドレス) 順に処理し、
フラットバイナリとして出力する。ELF ヘッダ・セクションテーブル等のメタデータを
除去し、CPU が直接実行できるメモリイメージを生成する。

2 つの LOAD セグメント (kernel + user) 間の BSS 領域はゼロパディングで埋める。
これにより boot.s がフラットバイナリを 0x3400 にべたロードするだけで、
`.bss` のゼロ初期化と `.user_text` の正しいアドレス配置が自動的に完了する。

```
ELF (_kernel)                              フラットバイナリ (kernel)

├── ELF ヘッダ        ─── 除去
├── プログラムヘッダ   ─── 除去
│                                          ┌─────────────────────────────┐
├── kernel セグメント  ─── 中身を抽出 ───→ │ .text + .rodata + .data     │
│     (.text〜.bss)                        │ ゼロパディング (BSS 分)     │
│                                          ├─────────────────────────────┤
├── user セグメント    ─── 中身を抽出 ───→ │ .user_text + .user_data     │
│     (.user_text〜)                       └─────────────────────────────┘
│
└── セクションテーブル ─── 除去
```

#### 3c: start — セカンドブートローダ

```makefile
start: start.o
    $(LD) -m elf_i386 --oformat binary -Ttext 0x0 -o start start.o
```

`start.s` は 16 ビットリアルモードコード:
1. A20 ライン有効化 (キーボードコントローラ経由)
2. GDT / IDT ロード
3. CR0.PE ビットセット → プロテクトモード移行
4. far jump で 32 ビットコード (0x3400 = run.s) へ

`--oformat binary` で ELF ヘッダなしの生バイナリとして出力。
`-Ttext 0x0` でアドレス 0 から配置。

出力: `start` (1024 バイトの 16 ビットバイナリ。`.org 1024` でパディング)

#### 3d: カーネル本体のリンク

```makefile
LDFLAGS = -m elf_i386 -n -T kernel.ld -static

kernel: elf $(OBJS)
    $(LD) $(LDFLAGS) -e run -o _kernel $(OBJS)
    cp _kernel _kernel_dbg
    strip _kernel
    ./elf _kernel kernel
```

**LDFLAGS の意味:**
- `-m elf_i386` — i386 ELF 形式
- `-n` — データセグメントのページアライメントを無効化
- `-T kernel.ld` — リンカスクリプトで全セクション配置を制御 (開始アドレス 0x3400、`.user_text` 分離)
- `-static` — 動的リンクなし
- `-e run` — エントリポイントは `run` シンボル (run.s)

$(OBJS) には i386/ の `.o` ファイルと `libkernel.a`、`libc.a` が含まれる。

処理の流れ:
1. `ld` で `_kernel` (ELF) を生成
2. `_kernel_dbg` にコピー (GDB 用にシンボル付きを保存)
3. `strip` でシンボルテーブルを除去 (サイズ削減)
4. `./elf` で ELF → フラットバイナリ変換

出力: `kernel` (フラットバイナリ)、`_kernel_dbg` (GDB 用 ELF)

#### 3e: ブートセクタ

```makefile
# i386/boot/Makefile
boot: boot.s
    $(AS) --32 -o boot.o boot.s
    $(LD) -m elf_i386 --oformat binary -Ttext 0x0 -o boot boot.o
```

`boot.s` はフロッピーの先頭 512 バイト。BIOS が自動的にロードする。

処理の流れ:
1. 自分自身を 0x7000:0000 にコピー (0x7c00 を解放)
2. フロッピーのセクタ 1〜299 を 0x0200:0000 以降に読み込み
3. far jump で 0x0300:0000 (start.s) に移行

最後の 2 バイトは `0xAA55` (BIOS ブートシグネチャ)。

出力: `boot` (512 バイトのブートセクタ。`.org 510` + 2 バイト署名)

#### 3f: 最終イメージの結合

```makefile
all: start genasm kernel table
    cat boot table start kernel > i386
```

4 つのバイナリを単純に連結:

| ファイル | サイズ固定の方法 | 固定サイズ | cat オフセット |
|----------|-----------------|-----------|---------------|
| boot | `.org 510` + 2B 署名 | 512 B (1 セクタ) | 0x0000 |
| table (GDT) | `.org 4096` | 4096 B (8 セクタ) | 0x0200 |
| start | `.org 1024` | 1024 B (2 セクタ) | 0x1200 |
| kernel | 可変 | ~113 KB | 0x1600 |

出力: `i386/i386` (生バイナリイメージ)

---

## べたロードで実行可能な仕組み

フロッピーイメージをべたロードするだけでカーネルが実行可能になる。
**それを成立させるために `.org`・`kernel.ld`・`-fno-pie`・`elf.c` が連携している。**
この仕組みの全体を 1 つの流れで説明する。

### 4.1 cat 連結 + べたロード → メモリアドレスが確定する

`cat` はファイルをそのまま並べるだけなので、**各ファイルのバイト数がそのまま
後続ファイルの開始位置を決める。** `.org` がそのサイズを固定しているため、
連結順序とメモリ配置は常に同じ結果になる。

boot.s はセクタ 1 以降 (= boot の直後) を **リニアアドレス 0x2000** に先頭から
順に読み込む。`cat` 連結の順序がそのままメモリ配置に反映される:

```
フロッピー                   ロード後のメモリ           対応する定数
─────────────                ──────────────────        ────────────
セクタ 0  (boot)             0x7C00 (BIOS がロード)     AL_BOOT
                             ↓ boot.s がセクタ 1〜を 0x2000 にべたロード
セクタ 1〜8   (table)   →   0x2000                     AL_GDT
セクタ 9〜10  (start)   →   0x3000                     AL_KERNEL16
セクタ 11〜   (kernel)  →   0x3400                     kernel.ld ". = 0x3400"
     :            :            :     .text, .rodata, .data
     :            :            :     BSS ゼロ領域 (elf.c がゼロ埋め済み)
     :            :            :     .user_text (ページ境界)
     :            :            :     .user_data
セクタ ~232                  カーネルイメージ末尾
```

### 4.2 0x3400 の由来 — .org サイズの合計

リンカスクリプトの開始アドレス `". = 0x3400"` は恣意的な値ではなく、
**boot のロード先と .org サイズの合計から一意に決まる**:

```
". = 0x3400"  =  0x2000 (boot のロード先: リアルモードセグメント 0x0200 × 16)
               + 4096   (table の .org サイズ)
               + 1024   (start の .org サイズ)
              = 0x3400
```

`addr.h` の定数も同じ計算に基づく:

| 定数 | 値 | 由来 |
|------|------|------|
| `AL_GDT` | 0x2000 | boot のロード先 |
| `AL_IDT` | 0x2100 | GDT の直後 (GDT テーブル内に配置) |
| `AL_KERNEL16` | 0x3000 | 0x2000 + 4096 (table サイズ) |
| (カーネル開始) | 0x3400 | 0x3000 + 1024 (start サイズ) = kernel.ld の ". = 0x3400" |

**もし `.org` の値を 1 つでも変更すると、以降のすべてのアドレスがずれる。**
例えば genasm.c の `.org 4096` を `.org 2048` に変えると、start は 0x2800 に
配置される。しかし start.s は `ljmp $0x20, $0x3400` (ハードコード) で
カーネルにジャンプするため、カーネルが実際には 0x2C00 にいるのに
0x3400 にジャンプしてしまい破綻する。

### 4.3 絶対アドレスリンク → リロケーション不要

`-fno-pie` (ステップ 1) と `kernel.ld` の `. = 0x3400` (ステップ 3d) により、
コード内のすべてのアドレスは最終的なメモリ配置を前提に解決されている。

```
① コンパイル  gcc -m32 -fno-pie
              → 絶対アドレス参照のオブジェクトコードを生成
                (GOT/PLT を使わない直接 call/jmp)

② リンク      ld -T kernel.ld  (". = 0x3400")
              → 全シンボルを 0x3400 基準の絶対アドレスに解決
                例: main = 0x4A2C, first_task = 0x1E120 (値はビルドにより変動)

③ ELF→raw変換  ./elf _kernel kernel
              → elf.c がビルドした変換ツール。ELF 形式の _kernel を読み、
                ヘッダを除去してフラットバイナリ kernel を出力する。
                kernel ファイルの先頭バイトがアドレス 0x3400 に対応する

④ 結合        cat boot table start kernel > i386
              → .org 固定サイズにより kernel のオフセットは 0x1600

⑤ ロード      boot.s がセクタ 1〜を 0x2000 に読み込む
              → kernel は 0x2000 + 5120 = 0x3400 に配置
              → リンカが想定したアドレスと実行時の配置が一致
              → リロケーション不要、コピー不要、そのまま実行可能
```

### 4.4 elf.c が BSS と .user_text の配置を保証する

`elf.c` がフラットバイナリ変換時に BSS 分のゼロパディングを挿入するため、
`.user_text` はフラットバイナリ内の正しいオフセット
(= `_user_text_start` - 0x3400) に配置される。
boot.s がべたロードするだけで `.user_text` はページ境界に着地する。
**ロード後にページ境界へコピーする処理は存在しない。**

BSS のゼロ初期化についても同様で、通常の ELF ローダは BSS をメモリ上で
ゼロクリアするが、`elf.c` がフラットバイナリ内にゼロを書き出しているため、
フロッピーからロードするだけで完了する。

### 4.5 タスク生成時のコードとスタックの関係

`cre_tsk` でユーザータスクを生成するとき、**コードはコピーされない**。
タスクごとに新しいのは **スタックだけ** である。

```
.user_text (_user_text_start〜)             スタックプール (0x700000〜)
┌──────────────────────┐                   ┌──────────────┐
│ first_task()         │←─ Task 1 EIP     │ stack_alloc()│← Task 1 ESP
│ second_task()        │←─ Task 2 EIP     │ stack_alloc()│← Task 2 ESP
│ usr_main()           │←─ Task 3 EIP     │ stack_alloc()│← Task 3 ESP
│ kbd_task()           │←─ Task 4 EIP     │ stack_alloc()│← Task 4 ESP
│ syscall ラッパー      │                   └──────────────┘
│ (cre_tsk, slp_tsk..) │
└──────────────────────┘
       ↑ 全タスクが共有参照              ↑ タスクごとに独立
```

`proc_create()` は EIP にタスク関数のアドレス (`.user_text` 内) を設定し、
ESP に `tsk_stack_alloc()` で確保したスタック (スタックプール内) の上端を設定する。
タスク関数のコードは `.user_text` の元の場所から直接実行される。
複数タスクが同じ関数を実行する場合でも、コードは 1 箇所に 1 つだけ存在する。

---

## リンカスクリプト (kernel.ld) の詳細

### セクション配置

`kernel.ld` はカーネルバイナリ内部のセクション配置を制御する:

```
kernel.ld の SECTIONS              メモリ上の配置
──────────────────────             ───────────────────
. = 0x3400;                           ← .org 計算から (§4.2)
.text    (カーネルコード)     →  0x3400 〜
.rodata  (読み取り専用データ) →  .text 直後
.data    (初期値ありデータ)   →  ALIGN(32) 後
.bss     (ゼロ初期化データ)   →  .data 直後

. = ALIGN(0x1000);                    ← ページ境界に切り上げ
.user_text (ユーザーコード)   →  アドレスはビルドするまで不定
  *(.user_text)                    Ring 3 タスク関数
  *libc.a:*(.text)                 syscall ラッパー (lib/)
```

### .user_text のアドレスはビルド結果で変動する

`.bss` の末尾は `.text`/`.rodata`/`.data`/`.bss` の総量で決まるため、
ソースを変更するたびに変わりうる。`ALIGN(0x1000)` がそれをページ境界に
切り上げた値が `.user_text` の開始アドレスとなる。

現在のビルドでは `.bss` 末尾が 0x1D608 付近なので、`ALIGN(0x1000)` の結果は 0x1E000。
しかしグローバル変数を追加すれば `.bss` が伸び、0x1F000 や 0x20000 にもなりうる。

### 2 つの LOAD セグメントに分ける理由

- `kernel` セグメント (RWX): カーネル本体。`.bss` を含むためメモリサイズ > ファイルサイズ
- `user` セグメント (RWX): ユーザータスクと syscall ラッパー (`.user_text`) + ユーザーデータ (`.user_data`)。Ring 3 で実行・参照されるコードとデータ

### リンカシンボルと page.c の連動

`page.c` はリンカシンボル `_user_text_start` / `_user_data_end` を参照し、
`.user_text` + `.user_data` のページのみ User+RW に設定する。
カーネルコード・データ (0x0〜`_user_text_start`) は Supervisor (U/S=0) であり、
Ring 3 からはアクセスできない。メモリ/スタックプール (0x110000〜0x74FFFF) も
User+RW に設定される。`.user_text` のアドレスが変わっても `page_init()` が
リンカシンボルから自動的に境界を算出するため、手動変更は不要である。
(詳細は [memory-map.md §2](memory-map.md#2-ページングの方針) を参照)

### ビルド後のアドレス確認方法

```bash
# ELF のプログラムヘッダ (LOAD セグメントの VMA) を表示
readelf -l i386/_kernel_dbg

# リンカが出力したシンボルでピンポイントに確認
nm i386/_kernel_dbg | grep _user_text
# 出力例:
#   0001e000 T _user_text_start
#   0001ed85 T _user_text_end
```

> **現在のビルドでの実測値** (ソース変更で変動する):
>
> | ELF セグメント | LMA | filesz | memsz | 含まれるセクション |
> |---------------|-------|--------|-------|-------------------|
> | kernel | 0x3400 | 0xF41C (62 KB) | 0x1A188 (106 KB) | .text .rodata .eh_frame .data .bss |
> | user | 0x1E000 | 0xF40 (3.9 KB) | 0xF40 | .user_text .user_data |
>
> フラットバイナリサイズ: 113,472 バイト (= 0xF41C + BSS パディング + 0xF40)

---

## メモリ領域の固定定数

メモリプールとスタックプールの範囲は `addr.h` にハードコードされた固定定数である。

```c
/* addr.h */
#define MEM_START       0x110000    /* mem_alloc プール開始 */
#define MEM_END         0x6fffff    /* mem_alloc プール終了 */
#define STACK_START     0x700000    /* stack_alloc プール開始 */
#define STACK_END       0x74ffff    /* stack_alloc プール終了 */
#define KERN_STACK_SIZE 4096        /* タスクごとのカーネルスタック (4KB) */
#define KERN_STACK_BASE 0x750000    /* カーネルスタック領域の開始 */
```

`pool.c` はこれらの定数を使うだけで、`page.c` の存在を知らない。
逆に `page.c` は `MEM_START` / `USER_MEM_END` で User ページ範囲を決めるが、
`pool.c` のことは知らない。**両者の整合性を自動的にチェックする仕組みは存在しない。**

実際には `page.c` と `pool.c` は **同じアドレス範囲** (0x110000〜0x74FFFF) を
管理している。`page.c` がこの範囲のページを User+RW に設定し、`pool.c` が
同じ範囲からメモリを割り当てる。両者が整合するのは、**どちらも `addr.h` の
同じ定数群から範囲を導出している** ためである。

加えて、`addr.h` の定数は各領域が重ならないよう手動で設計されている:

```
  0x0000           0x1D588  0x1EF40  0x110000          0x750000   0x770000
  |                |        |        |                 |          |         |
  | kern .text/bss | u_text | ~900KB | mem+stk pool    | kern stk | CPU stk |
  +----------------+--------+--------+-----------------+----------+---------+
  |  Supervisor    | User   | Supvsr |    User(RW)     |    Supervisor      |
  |--- loaded by boot.s ----|        |                 |                    |
  |    (addrs from §4)      | unused |                 |                    |

                                               (fixed constants in addr.h)
```

`kern stk` は各タスクの 4KB カーネルスタック領域 (KERN_STACK_BASE 〜)。
タスク N のカーネルスタック先頭 = `KERN_STACK_BASE + (N+1) * KERN_STACK_SIZE`。

カーネル + ユーザーコードの末尾 (現在 〜0x1EF40) と MEM_START (0x110000) の間には
約 900 KB のギャップがある。この領域はカーネルメモリプール (`kmem_alloc`) として
使用される (`itron_init()` で `kmem_init(&_user_data_end, MEM_START)` により初期化)。

仮にカーネルが極端に肥大化して `.user_data_end` が 0x110000 を超えた場合:
- `page.c` はリンカシンボルから User 領域を算出するので、ページ保護は正しく動く
- しかし `pool.c` の `mem_alloc()` が返すアドレスがユーザーコード領域と
  **物理的に重なる**。リンカもコンパイラもこれを検出しない
- **症状**: メモリ確保でユーザーコードが上書きされ、タスクが不正命令で停止する

---

## ブートローダのセクタ制限

boot.s の `cmpw $300, %ax` はカーネルバイナリサイズの上限を暗黙的に決める:

```
セクタ 1〜299 = 299 セクタ × 512 B = 153,088 B (約 149 KB)
そこから table (4096 B) + start (1024 B) を引くと:
カーネルバイナリの最大サイズ ≈ 147,968 B (約 144 KB)
```

現在のカーネルバイナリは約 113 KB なので約 35 KB の余裕がある。
この制限を超えた場合:
- boot.s はカーネルの末尾を読み込まない
- `.user_text` の末端が欠落し、syscall ラッパーの関数が存在しないメモリを指す
- ユーザータスクが syscall を呼ぶとゴミ命令を実行し、トリプルフォルトで停止する
- **症状がリンクエラーではなく実行時クラッシュなので、原因特定が非常に困難**

対策: boot.s の `cmpw $300` の値を増やす。フロッピーは最大 2880 セクタ (1.44 MB)
まで格納できるが、リアルモードの 64 KB セグメント境界に注意が必要。

---

## 変更時の影響マトリクス

| 変更内容 | 影響を受けるファイル | 症状 |
|----------|---------------------|------|
| genasm.c の `.org` 変更 | kernel.ld (". = 0x3400")、start.s (far jump 先)、addr.h (AL_GDT 等) | カーネルが間違ったアドレスにリンク → 起動失敗 |
| start.s の `.org` 変更 | kernel.ld (". = 0x3400")、addr.h (AL_KERNEL16) | 同上 |
| kernel.ld の ". = 0x3400" 変更 | start.s (far jump 先)、genasm.c/.org、addr.h | 同上 |
| ソース追加でカーネル肥大化 | boot.s (`cmpw $300`) | 末尾未ロード → 実行時トリプルフォルト |
| `-fno-pie` 削除 | 全 `.o` ファイル | GOT/PLT 参照 → リンクエラーまたは実行時クラッシュ |
| `-m32` 削除 | 全 `.o` ファイル | 64bit コード生成 → プロテクトモードで実行不能 |
| kernel.ld の `.user_text` 変更 | elf.c (セグメント処理)、page.c (U/S 境界) | ユーザーコード配置ずれ → syscall 失敗 |

---

## QEMU での実行

### run.sh のフロッピーイメージ生成

```bash
# 1.44 MB の空フロッピーイメージを作成
dd if=/dev/zero of=floppy.img bs=512 count=2880

# カーネルイメージを先頭に書き込み
dd if=i386 of=floppy.img conv=notrunc
```

1.44 MB = 512 バイト × 2880 セクタ (= 1,474,560 バイト)。
カーネルは約 120 KB なので、残りの領域は 0 で埋められる。

### QEMU コマンド

```bash
qemu-system-i386 \
    -drive file=floppy.img,format=raw,if=floppy \
    -boot a \           # フロッピーから起動
    -m 16 \             # メモリ 16 MB
    -cpu pentium3 \     # Pentium III 以上 (cmov 命令に必要)
    -accel tcg,thread=multi \   # マルチスレッド TCG
    -smp 2 \            # 2 CPU
    -display curses     # ターミナル上に VGA テキスト表示
```

**`-cpu pentium3` が必要な理由:**
GCC はデフォルトで `cmov` (条件付きムーブ) 等の Pentium Pro 以降の
命令を生成する。`-cpu 486` では未定義命令例外 (#UD) が発生する。

---

## 起動シーケンスのメモリ遷移

```
BIOS
 │ フロッピーのセクタ 0 を 0x7C00 にロード
 ▼
boot.s (0x7C00)
 │ 自身を 0x70000 にコピー
 │ セクタ 1-299 を 0x02000 以降にロード
 ▼
start.s (0x3000)
 │ A20 有効化
 │ GDT (0x2000) / IDT (0x2100) ロード
 │ プロテクトモード移行
 │ far jump → 0x3400
 ▼
run.s (0x3400)
 │ カーネルセグメント設定 (DS=0x28, SS=0x30)
 │ cpu_num 判定 (BSP: スタック 0x7a0000, AP: 0x770000)
 │ call main
 ▼
main() [BSP]                 main() [AP]
 │ all_init()                 │ page_enable()
 │ page_init()                │ smp_ap_init()
 │ page_enable()              │   APIC 初期化
 │ itron_init()               │   APIC タイマー開始
 │ proc_init()                │   start_second_task()
 │ tss_init()                 │     ╰→ ltr SEL_TSS1
 │ smp_init()                 │         ╰→ RESTORE_ALL → iret
 │   APIC 初期化              │             ╰→ second_task() [Ring 3]
 │   SIPI → AP 起動
 │   タイマー/KBD 開始
 │   start_first_task()
 │     ╰→ ltr SEL_TSS0
 │         ╰→ RESTORE_ALL → iret
 │             ╰→ first_task() [Ring 3]
```

---

## クリーンとリビルド

```bash
make -C kernel clean
make -C lib clean
make -C i386 clean
make -C i386/boot clean

# リビルド
make -C kernel && make -C lib && make -C i386
```

---

## 関連ドキュメント

- [ソースコード読解ガイド](source-guide.md) — ファイル一覧と読み進め順
- [メモリマップ](memory-map.md) — 物理アドレス配置
- [i386 アーキテクチャ](i386-architecture.md) — GDT、プロテクトモード
- [ブートセクタ](boot-sector.md) — boot.s の詳細
