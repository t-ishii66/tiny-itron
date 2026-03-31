# tiny-itron RTOS GDB デバッグガイド

Micro ITRON v4.0.0 (i386) カーネルを QEMU + GDB でデバッグする方法をまとめたガイドです。

---

## 目次

1. [準備](#1-準備)
2. [デバッグシンボル付きビルド](#2-デバッグシンボル付きビルド)
3. [QEMU の GDB モードでの起動](#3-qemu-の-gdb-モードでの起動)
4. [GDB の接続方法](#4-gdb-の接続方法)
5. [よく使うブレークポイント](#5-よく使うブレークポイント)
6. [タスク状態の調査](#6-タスク状態の調査)
7. [カーネル/ユーザーモードの判別](#7-カーネルユーザーモードの判別)
8. [よく使う GDB コマンド](#8-よく使う-gdb-コマンド)
9. [トラブルシューティング](#9-トラブルシューティング)

---

## 1. 準備

### 必要なツール

#### QEMU

i386 システムエミュレータが必要です。

```bash
# Ubuntu / Debian
sudo apt install qemu-system-x86

# Arch Linux
sudo pacman -S qemu-system-x86

# Fedora
sudo dnf install qemu-system-x86
```

#### GDB

32 ビット i386 ターゲットに対応した GDB が必要です。ホストが x86_64 であれば通常の `gdb` でも動作しますが、クロス環境の場合は `gdb-multiarch` または `i386-elf-gdb` を使用してください。

```bash
# Ubuntu / Debian (推奨: gdb-multiarch)
sudo apt install gdb-multiarch

# macOS (Homebrew で i386-elf-gdb をインストール)
brew install i386-elf-gdb

# Arch Linux
sudo pacman -S gdb
```

本ガイドでは `gdb` コマンドで記述しますが、環境に合わせて `gdb-multiarch` や `i386-elf-gdb` に読み替えてください。

---

## 2. デバッグシンボル付きビルド

### _kernel と _kernel_dbg

`i386/Makefile` のビルドフローは以下の通り:

```makefile
kernel	: elf $(OBJS)
	$(LD) $(LDFLAGS) -e run -o _kernel $(OBJS)
	cp _kernel _kernel_dbg       # シンボル付きコピーを保存
	strip _kernel                # フロッピーイメージ用にシンボル除去
	cp	/dev/null kernel
	./elf _kernel kernel         # ELF → フラットバイナリ変換
```

- `_kernel_dbg` — シンボル付き ELF。**GDB ではこちらを使う**
- `_kernel` — strip 済み ELF。`elf` ツールでフラットバイナリに変換される
- `kernel` — フラットバイナリ。フロッピーイメージに結合される

通常のビルド (`make -C i386`) で `_kernel_dbg` は自動生成されるため、
strip を無効化する必要はない。

**仕組み:** QEMU が実行するのはフロッピーイメージ上のフラットバイナリ
(strip 済み、シンボル情報なし) である。GDB は QEMU の GDB サーバーに
TCP で接続して CPU レジスタやメモリを読み書きするが、それだけでは
アドレスの数字しか見えない。`_kernel_dbg` は GDB に
「アドレス 0xXXXX = 関数 main」といったシンボル情報を教えるために渡す。
`_kernel_dbg` と実行バイナリはリンクアドレスが同一なので、
GDB はシンボルを正しく解決できる。
フロッピーイメージの中身は一切変わらない。

### ソースレベルデバッグを有効にする場合

デフォルトでは関数名・グローバル変数のシンボルは使えるが、
ソース行番号の対応 (ステップ実行、`list` コマンド等) には
DWARF デバッグ情報が必要になる。必要に応じて以下を設定する。

#### (a) CFLAGS に -g を追加

各 Makefile の `CFLAGS` に `-g` を追加する:

**`i386/Makefile`:**
```makefile
CFLAGS = -Wall -m32 -fno-pie -fno-stack-protector -fno-builtin -g
```

**`kernel/Makefile`:**
```makefile
CFLAGS = -Wall -m32 -fno-pie -fno-stack-protector -fno-builtin -g
```

**`lib/Makefile`:**
```makefile
CFLAGS = -m32 -fno-pie -fno-stack-protector -fno-builtin -g
```

#### (b) アセンブリファイルのデバッグ情報

アセンブリファイル (`.s`) のデバッグ情報も必要な場合は、`i386/Makefile` のアセンブリルールに `--gstabs` を追加する:

```makefile
%.o	: %.s
	$(AS) --32 --gstabs -o $@ $<
```

#### (c) クリーンビルド

変更を反映するため、全てのオブジェクトファイルを再生成する:

```bash
make -C kernel clean && make -C kernel
make -C lib clean && make -C lib
make -C i386 clean && make -C i386
```

> **注意:** `-g` を付けても `_kernel_dbg` のサイズが増えるだけで、
> フロッピーイメージ (`i386/i386`) には影響しない。
> `elf` ツールが LOAD セグメントのみを抽出し、DWARF セクションは含まれないため。

---

## 3. QEMU の GDB モードでの起動

### run.sh の -G オプション

`run.sh` には GDB デバッグ用のオプション `-G` が用意されています。

```bash
./run.sh -G
```

このコマンドは QEMU に以下のオプションを追加します。

| QEMU オプション | 意味 |
|:--|:--|
| `-s` | GDB サーバーを TCP ポート 1234 で起動 |
| `-S` | CPU を起動時に停止した状態にする (GDB からの `continue` を待つ) |

起動すると以下のメッセージが表示されます。

```
GDB mode: waiting for connection on port 1234...
Starting QEMU...
```

QEMU は CPU が停止した状態で待機しています。別のターミナルから GDB を接続するまでカーネルは実行されません。

### 他のオプションとの組み合わせ

```bash
# GTK ウィンドウモード + GDB
./run.sh -g -G

# デバッグログ出力 + GDB
./run.sh -d -G
# → 割り込みログが qemu.log に記録されます
```

---

## 4. GDB の接続方法

### 基本的な接続手順

ターミナル 1 で QEMU を GDB モードで起動した後、ターミナル 2 で GDB を起動します。

```bash
# ターミナル 2
gdb i386/_kernel_dbg
```

GDB プロンプトで以下のコマンドを実行します。

```gdb
# アーキテクチャを i386 に設定 (gdb-multiarch の場合)
set architecture i386

# ソースの検索パスを追加
directory i386 kernel lib

# シンボルファイルの読み込み (必要に応じて)
# gdb 起動時に i386/_kernel_dbg を指定していれば不要
symbol-file i386/_kernel_dbg

# QEMU の GDB サーバーに接続
target remote :1234
```

### .gdbinit による自動化

プロジェクトルートに `.gdbinit` ファイルを作成すると、毎回のコマンド入力を省略できます。

```gdb
# .gdbinit
set architecture i386

# ソースの検索パス (プロジェクトルートから実行する場合)
# -g/--gstabs が記録するパスは各ディレクトリからの相対パスのため必要
directory i386 kernel lib

symbol-file i386/_kernel_dbg
target remote :1234

# カーネルのロードアドレスは 0x3400
# シンボルがずれる場合は以下を使用
# add-symbol-file i386/_kernel_dbg 0x3400

# --- 表示設定 (必要に応じてコメントを外す) ---
# 構造体を1フィールド1行でインデント表示
# set print pretty on
# 配列を1要素1行で表示
# set print array on
# 配列・文字列の表示要素数を無制限にする (デフォルトは 200)
# set print elements 0

# カーネル起動直後の main にブレークポイント
break main
continue
```

> **注意:** GDB が `.gdbinit` を自動読み込みしない場合は、`gdb -x .gdbinit` で明示的に指定するか、`~/.config/gdb/gdbinit` に `set auto-load safe-path /` を追加してください。

### 接続確認

接続に成功すると、GDB は QEMU が停止している位置 (リアルモードのリセットベクタ `0xfff0` 付近、またはブートローダの先頭) で停止します。

```gdb
(gdb) info registers
# → eip が 0x0000fff0 付近であれば正常
```

カーネルの `main` 関数まで実行を進めるには:

```gdb
(gdb) break main
(gdb) continue
```

---

## 5. よく使うブレークポイント

### カーネル起動シーケンス

```gdb
# カーネルエントリポイント (32ビットモード移行後)
break run

# カーネルメイン関数
# → all_init(), page_init(), page_enable(), itron_init(),
#   proc_init(), tss_init() を経て start_first_task() を呼ぶ
break main

# ハードウェア初期化
break all_init

# 最初のタスク起動 (ltr + RESTORE_ALL + iret)
break start_first_task
```

### タスク実行

```gdb
# 最初のユーザータスク (タスク ID 1)
# → セマフォ 1 を作成し、タスク ID 3 (usr_main) を生成・起動する
break first_task

# メインユーザータスク (タスク ID 3)
break usr_main

# メインユーザータスク内のセマフォ取得成功時
break usr_main
# (usr_main 内の pol_sem 成功パスにブレークを置く等)
```

### 割り込み・コンテキストスイッチ

```gdb
# タイマー割り込みハンドラ (IRQ0 -> INT 0x80)
break c_intr_irq0

# 割り込みエントリ・タスクスイッチ (アセンブリ)
break intr_enter
break intr_leave

# スケジューラ
break sched_do_next_tsk
```

### 条件付きブレークポイント

```gdb
# タスク ID 4 のスケジューリング時のみ停止
break sched_do_next_tsk if apic == 0

# タイマー割り込みの 10 回目のみ停止
break c_intr_irq0
ignore 1 9

# VGA 出力で特定の文字を書き込む時に停止
break video_putc
```

---

## 6. タスク状態の調査

### レジスタの確認

```gdb
# 全レジスタの表示
info registers

# 個別のレジスタ
print/x $eip
print/x $esp
print/x $eflags
print/x $cs
print/x $ds
print/x $ss
```

### スタックの確認

```gdb
# 現在のスタックの先頭 20 ワード (32ビット) を表示
x/20x $esp

# バックトレース (シンボル付きビルドの場合)
backtrace
bt full
```

### proc 構造体の調査

tiny-itron のタスクは `proc_t` 構造体で管理されています。

```c
typedef struct proc {
    unsigned long kern_esp;       /* カーネルスタックの現在位置 */
    unsigned long kern_stack_top; /* カーネルスタックの先頭 (TSS.esp0 に設定する値) */
    unsigned long saved_eflags;   /* proc_eflags_save/restore 用 */
    int           cpu;            /* CPU アフィニティ (0 or 1) */
} proc_t;
```

各タスクは 4KB のカーネルスタックを持ち、割り込み時に SAVE_ALL が
レジスタを push して pt_regs フレームを構築する。`kern_esp` は
このフレームの先頭を指す。

pt_regs フレーム (カーネルスタック上):

| オフセット | レジスタ | push 元 |
|:--|:--|:--|
| 0x00 | ES | SAVE_ALL |
| 0x04 | DS | SAVE_ALL |
| 0x08 | EDI | SAVE_ALL |
| 0x0C | ESI | SAVE_ALL |
| 0x10 | EBP | SAVE_ALL |
| 0x14 | EBX | SAVE_ALL |
| 0x18 | EDX | SAVE_ALL |
| 0x1C | ECX | SAVE_ALL |
| 0x20 | EAX | SAVE_ALL |
| 0x24 | EIP | CPU |
| 0x28 | CS | CPU |
| 0x2C | EFLAGS | CPU |
| 0x30 | ESP (Ring 3) | CPU |
| 0x34 | SS (Ring 3) | CPU |

GDB で proc 構造体を調査するコマンド:

```gdb
# 現在のプロセスポインタ
print current_proc[0]

# proc 構造体の内容を表示
print *current_proc[0]

# カーネルスタックの現在位置
print/x current_proc[0]->kern_esp

# カーネルスタック先頭 (TSS.esp0 に設定される値)
print/x current_proc[0]->kern_stack_top

# pt_regs フレームからレジスタ値を読む (kern_esp が pt_regs を指す)
# EAX (戻り値): kern_esp + 0x20
x/x current_proc[0]->kern_esp + 0x20

# EIP (中断箇所): kern_esp + 0x24
x/x current_proc[0]->kern_esp + 0x24

# ESP (Ring 3 スタック): kern_esp + 0x30
x/x current_proc[0]->kern_esp + 0x30

# pt_regs を構造体として表示
print *(struct pt_regs *)current_proc[0]->kern_esp
```

### スケジューラの状態

```gdb
# CPU ロック状態
print cpu_stat

# ディスパッチ許可/禁止状態
print dispatch_stat

# 次タスクフラグ
print next_tsk_flag[0]

# 割り込みネストカウンタ (アセンブリで定義、下記の注意を参照)
print *(int*)&k_nest0
print *(int*)&k_nest1
```

> **注意:** `k_nest0`/`k_nest1` など intr.s 内の `.long` で定義された変数は
> DWARF 型情報を持たない。GDB で `print k_nest0` とすると "has unknown type"、
> `print (int)k_nest0` とするとアドレス値がそのまま表示されてしまう。
> 値を読むには `print *(int*)&k_nest0` または `x/1dw &k_nest0` を使う。

---

## 7. カーネル/ユーザーモードの判別

### CS レジスタによる判別

i386 のセグメントセレクタの下位 2 ビットが CPL (Current Privilege Level) を示します。

```gdb
# 現在の CS セレクタを確認
print/x $cs
```

| CS 値 | モード | 説明 |
|:--|:--|:--|
| `0x20` | Ring 0 (カーネルモード) | カーネルコードセグメント |
| `0x5b` | Ring 3 (ユーザーモード) | ユーザーコードセグメント |

その他のセグメントセレクタ:

| セグメント | Ring 0 (カーネル) | Ring 3 (ユーザー) |
|:--|:--|:--|
| CS | `0x20` | `0x5b` |
| DS | `0x28` | `0x63` |
| SS | `0x30` | `0x6b` |

```gdb
# データセグメントとスタックセグメントも確認
print/x $ds
print/x $ss

# 全セグメントレジスタを一覧表示
info registers cs ds ss es fs gs
```

### システムコールのトレース

tiny-itron はシステムコールに `INT 0x99` を使用します。ユーザーモードからカーネルモードへの遷移をステップ実行で追跡できます。

```gdb
# システムコール発行箇所にブレークポイント
break intr_syscall

# または syscall ラッパー関数
break syscall

# 停止後、stepi で命令単位で追跡
stepi
# → INT 0x99 が実行されると CS が 0x5b -> 0x20 に変化
print/x $cs
```

### タイマー割り込み (INT 0x80) のトレース

タイマー割り込み (IRQ0) は INT 0x80 としてカーネルに通知されます。

```gdb
# タイマー割り込みエントリ
break intr_irq0

# 停止後
print/x $cs
# → 0x20 (カーネルモード) で実行されている

# 割り込み前のコンテキスト (ユーザーモードからの場合)
# スタック上に保存された CS を確認
x/5x $esp
# → [EIP, CS, EFLAGS, ESP, SS] の順で保存されている
# → CS が 0x5b ならユーザーモードからの割り込み
```

---

## 8. よく使う GDB コマンド

### 実行制御

| コマンド | 短縮 | 説明 |
|:--|:--|:--|
| `continue` | `c` | 実行を再開 |
| `stepi` | `si` | 1 命令 (アセンブリレベル) ステップ実行 |
| `nexti` | `ni` | 1 命令ステップ実行 (call をまたぐ) |
| `step` | `s` | 1 行 (ソースレベル) ステップ実行 |
| `next` | `n` | 1 行ステップ実行 (関数呼び出しをまたぐ) |
| `finish` | `fin` | 現在の関数から戻るまで実行 |
| `until *addr` | | 指定アドレスまで実行 |

### ブレークポイント

```gdb
# 関数名で設定
break main
break video_putc

# アドレスで設定
break *0x3400

# 条件付き
break c_intr_irq0 if k_nest0 == 0

# ハードウェアウォッチポイント (メモリ書き込み監視)
watch current_proc[0]
watch *(int*)0x3400

# 一覧表示
info breakpoints

# 削除
delete 1
delete    # 全て削除

# 一時的に無効化/有効化
disable 1
enable 1
```

### メモリ・レジスタの調査

```gdb
# レジスタ全表示
info registers

# メモリ内容の表示
# x/[個数][フォーマット][サイズ] [アドレス]
#   フォーマット: x(16進), d(10進), s(文字列), i(逆アセンブル)
#   サイズ: b(1バイト), h(2バイト), w(4バイト), g(8バイト)

x/20x $esp          # スタック上位 20 ワード (16進)
x/10i $eip          # 現在の EIP から 10 命令を逆アセンブル
x/s 0x3500          # 文字列として表示
x/32b 0x3400        # 32 バイトを 1 バイト単位で表示

# 変数の表示
print variable_name
print/x variable_name    # 16 進数
print *pointer           # ポインタの中身

# 逆アセンブル
disassemble main
disassemble intr_leave
disassemble $eip, $eip+50    # 範囲指定
```

### 表示レイアウト

```gdb
# アセンブリ + レジスタの TUI 表示
layout asm
layout regs

# ソースコード表示 (シンボル付きの場合)
layout src

# TUI の切り替え
tui enable
tui disable

# ソース + アセンブリの分割表示
layout split
```

### その他の便利コマンド

```gdb
# GDT/IDT/TSS の内容を確認 (QEMU モニタ経由)
# QEMU モニタ (Ctrl-Alt-2 で切替、または -monitor stdio) で:
#   info registers
#   info tlb
#   info mem

# GDB から QEMU モニタコマンドを実行
monitor info registers
monitor info mem

# 特定のアドレスに値を書き込む
set *(int*)0x3400 = 0x90909090

# 自動表示 (ステップ毎に表示)
display/x $eip
display/x $cs
display/i $eip

# 自動表示の解除
undisplay 1
```

---

## 9. トラブルシューティング

### 問題: アーキテクチャが正しく設定されない

**症状:** GDB が 16 ビットモードとして命令を解釈し、逆アセンブル結果がおかしい。

**原因:** QEMU は BIOS のリアルモードから開始するため、GDB がアーキテクチャを 16 ビット (i8086) として認識することがあります。

**解決方法:**

```gdb
set architecture i386
```

`run` シンボル (エントリポイント、アドレス `0x3400`) 以降は 32 ビットプロテクトモードで動作するため、`i386` を指定してください。ブートローダ部分 (`start.s`) のデバッグ時のみ `i8086` が必要です。

### 問題: シンボルが見つからない

**症状:** `break main` で "Function "main" not defined." と表示される。

**原因 1:** strip 済みの `_kernel` を使っている。

**解決方法:** GDB 起動時に `i386/_kernel_dbg` (シンボル付き) を指定する。
`_kernel` は strip されておりシンボルがない。

**原因 2:** シンボルファイルが正しく読み込まれていない。

**解決方法:**

```gdb
symbol-file i386/_kernel_dbg
```

### 問題: シンボルのアドレスがずれている

**症状:** ブレークポイントで停止するが、ソース表示がずれている。または、ブレークポイントに到達しない。

**原因:** カーネルのリンクアドレス (`-Ttext=0x3400`) と実際のロードアドレスが一致しない場合。

**解決方法:**

```gdb
# シンボルファイルのテキストセクション開始アドレスを明示指定
add-symbol-file i386/_kernel_dbg 0x3400
```

### 問題: target remote 接続できない

**症状:** `target remote :1234` で "Connection refused" が表示される。

**原因:** QEMU が `-s` オプション付きで起動されていない、またはまだ起動していない。

**解決方法:**

1. QEMU を `-G` オプション付きで起動していることを確認: `./run.sh -G`
2. QEMU が先に起動してから GDB で接続する
3. ポートが他のプロセスに使用されていないか確認: `ss -tlnp | grep 1234`

### 問題: continue 後にブレークポイントに到達しない

**症状:** `break main` を設定して `continue` しても停止しない。

**原因:** QEMU は BIOS → ブートローダ → カーネルの順に実行します。0x3400 はカーネルバイナリのエントリポイント (`run.s`) であり、`main()` はその先の可変アドレスに配置されます。BIOS 実行中は 16 ビットモードであり、カーネルコードはまだロードされていません。GDB は `break main` でシンボルを解決するため、具体的なアドレスを知る必要はありません。

**解決方法:**

ブートローダがカーネルを正しくロードし、プロテクトモードに移行する必要があります。以下を確認してください。

```gdb
# カーネルのエントリアドレスにハードウェアブレークポイントを設定
hbreak *0x3400
continue
```

`hbreak` (ハードウェアブレークポイント) はモード遷移を跨いでも動作します。通常の `break` (ソフトウェアブレークポイント) はモード遷移後に機能しない場合があります。

> **理由**: ソフトウェアブレークポイントは命令を INT 3 (0xCC) に置換する仕組みのため、
> リアルモード→プロテクトモード遷移でアドレス空間の解釈が変わると正しく機能しなくなる。
> ハードウェアブレークポイントは CPU のデバッグレジスタ (DR0-DR3) を使い、モードに依存しない。

### 問題: stepi でタスクスイッチ後にコンテキストを見失う

**症状:** `start_first_task` の後、GDB が実行位置を追跡できなくなる。

**原因:** `ret` → `intr_return_restore` → `RESTORE_ALL` → `iret` でレジスタが全て入れ替わり、
ESP も Ring 3 スタックに切り替わるため、GDB のステップ実行が不連続になる。

**解決方法:**

```gdb
# タスクのエントリポイントにブレークポイントを設定してから continue
break first_task
continue

# 現在の TSS の内容を QEMU モニタで確認
monitor info registers
```

### 問題: タイマー割り込みで頻繁に停止する

**症状:** `break c_intr_irq0` を設定すると、頻繁にブレークポイントに到達してデバッグが困難。

**解決方法:**

```gdb
# N 回目以降のみ停止
break c_intr_irq0
ignore 1 100    # 最初の 100 回を無視

# 条件付きブレークポイント
break c_intr_irq0 if k_nest0 > 0    # ネスト割り込み時のみ

# タイマー割り込みを一時無効にして他の部分をデバッグ
# (QEMU モニタで)
monitor info pic
```

### 参考: QEMU の便利なオプション

```bash
# 割り込みと CPU リセットのログを出力
./run.sh -d -G
# → qemu.log に詳細なログが記録される

# QEMU モニタを stdio に出力 (GDB と併用時)
# run.sh を編集して以下を追加:
#   -monitor stdio
```

---

## 付録: デバッグセッションの実例

以下は、カーネル起動からユーザータスク実行までをデバッグする典型的なセッションです。

```
# ターミナル 1: QEMU 起動
$ ./run.sh -G
GDB mode: waiting for connection on port 1234...
Starting QEMU...

# ターミナル 2: GDB 接続
$ gdb i386/_kernel_dbg
(gdb) set architecture i386
(gdb) directory i386 kernel lib
Source directories searched: /home/.../tiny-itron/i386:...
(gdb) target remote :1234
Remote debugging using :1234
0x0000fff0 in ?? ()

# カーネルの main にブレークポイントを設定
(gdb) break main
Breakpoint 1 at 0x3xxx: file main.c, line 24.
(gdb) continue
Continuing.

Breakpoint 1, main () at main.c:24
24          ccli();

# main 関数内をステップ実行
(gdb) next
25          all_init();
(gdb) next
26          page_init();
(gdb) next
27          page_enable();
(gdb) next
28          itron_init();
(gdb) next
...

# first_task にブレークポイントを設定
(gdb) break first_task
Breakpoint 2 at 0x...
(gdb) continue
Continuing.

Breakpoint 2, first_task () at user.c:11
11          T_CTSK  ctsk;

# ユーザーモードに入ったことを確認
(gdb) print/x $cs
$1 = 0x5b

# usr_main にブレークポイントを設定
(gdb) break usr_main
Breakpoint 3 at 0x...
(gdb) continue
Continuing.

Breakpoint 3, usr_main (arg=0x12345) at user.c:105

# タイマー割り込みの動作を確認
(gdb) break c_intr_irq0
Breakpoint 4 at 0x...
(gdb) continue
Continuing.

Breakpoint 4, c_intr_irq0 () at interrupt.c:...
(gdb) print/x $cs
$2 = 0x20
# → カーネルモード (Ring 0) で割り込みハンドラが実行されている

# コンテキストスイッチの追跡
(gdb) break sched_do_next_tsk
(gdb) continue
```

---

## 付録: メモリマップ概要

| アドレス | 内容 |
|:--|:--|
| `0x0000` - `0x33FF` | ブートローダ、GDT/IDT テーブル |
| `0x3400` - | カーネルコード (`.text` セクション開始) |
| `0x7A0000` | カーネルスタック初期値 (`run.s` で設定) |
| `0xB8000` - `0xBFFFF` | VGA テキストバッファ |

---

本ガイドは tiny-itron Micro ITRON v4.0.0 for i386 を対象としています。
