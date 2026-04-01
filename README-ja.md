<img src="top.png" width="800">

# tiny-itron

**English version: [README.md](README.md)**

[Micro ITRON 4.0 (μITRON)](https://www.ertl.jp/ITRON/SPEC/mitron4-e.html) 仕様をベースにした、i386 (x86/IA-32) 向けの教育用ベアメタル RTOS (リアルタイム OS) カーネル。SMP (2 CPU) 対応、プリエンプティブマルチタスク、割り込みハンドリングとコンテキストスイッチを実装し、QEMU 上で動作する。
2000年頃、趣味で作成したカーネルを github で公開できるようにいくつかの変更を実施。
おもちゃの OS カーネルですが、QEMU 仮想環境ですぐ試していただけます。もしあなたが教科書的にカーネルの学習をしたいなら、このプロジェクトに出来ることはあまりありませんが、プログラミングを楽しみ、ブートセクタや GDT/IDT のセットアップ、タスクスケジューリングがハードウェアレベルでどう動くかを学びたい方にはお役に立てられるのではないかと考えています。
ドキュメント作成に　AI を活用してます。概ね正しく生成されてると思いましたがチェックをすり抜けて
表現が足りない箇所があるかもしれません。その場合はソースコードに対してお使いの　AI　に
解析を依頼してみてください。おそらくそちらのほうが正解です。

![](screenshot.png)

## このプロジェクトの目的

OS の仕組みは、ほとんどの人が教科書で学ぶ。
このプロジェクトは、**実際のカーネルコードを遊びながら動かして学ぶ** ためにある。

tiny-itron は Micro ITRON 4.0 のタスク・セマフォ・イベントフラグといった API スタイルを
借りているが、仕様を忠実に・完全に実装することは目指していない。
多くの ITRON syscall はスタブのままであり、内部アーキテクチャも
簡潔さを優先して仕様から自由に逸脱している。　
これはおもちゃのカーネルである——そして、それこそが利点である。

小さいこと (C とアセンブリ合わせて約 8,000 行) は、システム全体を頭の中に
収められることを意味する。ハードウェアを隠す抽象化レイヤは存在しない。
syscall をユーザ空間からカーネルまでたどれば、すべての命令が見える。
スケジューラに `printk` を 1 行足してリブートすれば、数秒で結果がわかる。

このプロジェクトが焦点を当てるのは、教科書が理論として説明はするが
なかなか **手を動かして触らせてくれない** 2 つのテーマである:

- **ブートプロセス** —— BIOS が 512 バイトのブートセクタをロードするところから、
  リアルモード→プロテクトモード遷移、GDT/IDT/TSS のセットアップ、
  ページング、PIC・APIC の初期化を経て、最初のユーザタスクが Ring 3 で
  動き出すまで。すべてのステップがソースコードに存在し、ドキュメント化されている。

- **ベアメタル上のマルチタスキング** —— `SAVE_ALL`/`RESTORE_ALL` マクロが
  per-task カーネルスタック上に 9 個のレジスタを保存・復元してタスクを切り替える仕組み、
  `intr_leave` が ESP を差し替えるだけでコンテキストスイッチする方法、
  2 つの CPU がスピンロックで協調する方法、
  キーボード割り込みが実行中のタスクをプリエンプトする仕組み。
  これらすべてを GDB でリアルタイムに観察できる。

目標はプロダクション品質ではない。目標は、すべての定数に理由があり、
すべてのレジスタ保存に *なぜそうするか* のコメントがあり、
ユーザ空間からハードウェアまでのあらゆる経路を `grep` と `gdb` だけで
追跡できることである。

## 動作画面

カーネルを実行すると、QEMU に VGA テキストモードの画面が表示される:

```
  TinyITRON/386 SMP (2 CPU)                            [Ctrl+C to quit]
  ============================================================================

    Timer         tick =         12345

  [CPU0] Task1 |  #       142     mbf: hello world

  [CPU0] Task3 /  #        71     [LOCK]---+
                                            \
                                             +---> Shared (sem 1)    #       38

  [CPU1] Task2 -  #       284     [BUSY]---+

  [CPU1] Task4    > hello world_


                      .---------.             .---------.
                      |  CPU 0  |             |  CPU 1  |
                      | Task1,3 |---[ BKL ]---| Task2,4 |
                      |  Idle5  |             |  Idle6  |
                      '---------'             '---------'
                           \                     /
                            '--- Shared Memory --'

  Copyright (c) 2000-2026 t-ishii66. All rights reserved.
```

- **Task 1** と **Task 3** が CPU 0 上で `wup_tsk`/`slp_tsk` により交互に実行
- **Task 2** は CPU 1 上で継続的に実行
- **Task 3** (CPU 0) と **Task 2** (CPU 1) がバイナリセマフォで共有カウンタを
  競合アクセス —— LOCK/BUSY の状態がリアルタイムに表示される
- **Task 4** は最高優先度でキーボード入力をエコーし、Task 2 をプリエンプトする。
  Enter でメッセージバッファ (MBF) 経由で Task 1 に行文字列を送信
- **Timer tick** は CPU 0 上の PIT (IRQ0) で駆動
- 画面下部に **SMP アーキテクチャ図**: 2 CPU 構成と BKL (Big Kernel Lock)、
  共有メモリバスの関係を表示

## 環境構築

Ubuntu 24.04 LTS (amd64) で動作確認している。

```bash
# ビルドツール (GCC, Make) + 32 ビットサポート
sudo apt install build-essential gcc-multilib

# エミュレータ
sudo apt install qemu-system-x86

# デバッガ (任意)
sudo apt install gdb
```

詳しくは [docs/ja/build-system.md](docs/ja/build-system.md) を参照。

## ビルドと実行

```bash
# ビルド
make

# QEMU で実行 (2 CPU)
./run.sh          # curses モード (ターミナル上に VGA テキスト表示、Ctrl+C で終了)
./run.sh -g       # GTK ウィンドウモード (Ctrl+Alt+G でグラブ解除)
./run.sh -G       # GDB モード (ポート 1234 で接続待ち)

# クリーンビルド
make clean && make
```

## GDB デバッグ

```bash
# ターミナル 1
./run.sh -G

# ターミナル 2
gdb i386/_kernel_dbg
(gdb) set architecture i386
(gdb) directory i386 kernel lib
(gdb) target remote :1234
(gdb) break first_task
(gdb) continue
(gdb) info threads            # 両方の CPU が見える
(gdb) p task_count[1]         # Task 1 の実行回数
(gdb) p shared_count          # セマフォで保護された共有カウンタ
```

詳しくは [docs/ja/gdb-debugging.md](docs/ja/gdb-debugging.md) を参照。

## アーキテクチャ

### ハードウェア初期化の流れ

カーネルは起動時に以下のハードウェア機能を順に初期化する:

```
BIOS
  ↓  INT 13h でフロッピーからカーネルをロード
ブートセクタ (boot.s)
  ↓  リアルモード → プロテクトモード遷移
GDT / IDT / TSS (start.s, interrupt.c, tss.c)
  ↓  セグメント、割り込みゲート、タスク状態セグメント
PIC i8259 (interrupt.c)
  ↓  IRQ ルーティング: 外部 IRQ はすべて CPU 0 に配送
Local APIC (smp.c)
  ↓  CPU 識別 (APIC ID)、per-CPU タイマー、EOI
ページング (page.c)
  ↓  恒等写像、User/Supervisor アクセス制御
カーネル起動 → 最初のユーザタスク (Ring 3)
```

### 特権モデル

| Ring | CS     | DS     | SS     | 役割             |
|------|--------|--------|--------|------------------|
| 0    | 0x20   | 0x28   | 0x30   | カーネル         |
| 3    | 0x5B   | 0x63   | 0x6B   | ユーザタスク     |

### SMP 設計

- **2 CPU**: BSP (CPU 0) + AP (CPU 1)、Local APIC ID で識別
- **AP 起動**: INIT IPI + SIPI シーケンス、AP がプロテクトモードに再突入
- **タイマー**: PIT (IRQ0、CPU 0 のみ) + Local APIC タイマー (両 CPU)
- **スピンロック**: `xchgl` ベース (Ring 3 から使用可能、`cli`/`sti` 不要)
- **CPU アフィニティ**: 各タスクは CPU に固定、スケジューラがアフィニティでフィルタ
- **I/O APIC なし**: 意図的な簡略化。PIC がすべての外部 IRQ を処理

### syscall パス

```
ユーザタスク        Ring 3              Ring 0
──────────          ──────              ──────
slp_tsk()
  -> syscall(0x11)
    -> int $0x99  ----[gate]---->  intr_syscall
                                    -> SAVE_ALL (9 regs → per-task kernel stack)
                                    -> intr_enter (k_nest++)
                                    -> c_intr_syscall(pt_regs*)
                                      -> itron_syscall
                                        -> syscall_entry[0x11] = sys_slp_tsk
                                    -> regs->eax = 戻り値
                                    -> intr_leave (k_nest--, タスクスイッチ判定)
                                    -> RESTORE_ALL (9 regs pop)
                                    -> iret  ----[gate]----> ユーザに復帰
```

詳しくは [docs/ja/syscall.md](docs/ja/syscall.md) を参照。

### コンテキストスイッチ

```
SAVE_ALL:    全割り込み/syscall の先頭で実行
             EAX,ECX,EDX,EBX,EBP,ESI,EDI,DS,ES を per-task カーネルスタックに push

intr_leave:  最外の割り込みから戻るときのみ実行 (k_nest == 0)
             current_proc[cpu]->kern_esp = ESP   (現タスクの ESP を保存)
             sched_next_tsk_check(cpu)           (タスクスイッチ判定)
             ESP = current_proc[cpu]->kern_esp   (新タスクの ESP をロード)
             tss_update_esp0()                   (TSS.esp0 を新タスクに更新)

RESTORE_ALL: ES,DS,EDI,ESI,EBP,EBX,EDX,ECX,EAX を pop
             → 新タスクのカーネルスタックから復元されるため、タスクが切り替わる

iret:        CS:EIP, SS:ESP, EFLAGS をアトミックに復元 → 新タスクが Ring 3 で実行開始
```

詳しくは [docs/ja/context-switch.md](docs/ja/context-switch.md) を参照。

## ソース構成

```
i386/           アーキテクチャ依存コード
  boot/           ブートセクタ (boot.s) とローダテーブル
  start.s         リアルモード → プロテクトモード遷移
  main.c          カーネルエントリポイント、初期化シーケンス
  intr.s          SAVE_ALL/RESTORE_ALL、全割り込み/例外/syscall エントリ
  klib.s          start_first/second_task、I/O ポートヘルパ、スピンロック
  proc.c          タスク proc_t 管理、偽 pt_regs フレーム構築、CPU アフィニティ
  interrupt.c     IDT セットアップ、IRQ/例外/syscall ハンドラ登録
  page.c          ページディレクトリ/テーブル設定 (恒等写像、U/S アクセス制御)
  smp.c           AP 起動 (INIT/SIPI)、Local APIC タイマー
  syscall.c       c_intr_syscall (pt_regs* から引数読み出し、戻り値書き戻し)
  video.c         VGA テキストモードドライバ (0xB8000)
  keyboard.c      PS/2 キーボードドライバ、IRQ1
  timer.c         PIT (8254) 初期化
  tss.c           TSS 初期化、動的 esp0 更新

kernel/         アーキテクチャ非依存 ITRON カーネル
  syscall.c       itron_syscall ディスパッチャ
  syscallP.h      syscall_entry[] ディスパッチテーブル
  sys_tsk.c       タスク管理 (cre/act/slp/wup/ter_tsk, ...)
  sys_sem.c       セマフォ (cre/sig/wai/pol_sem)
  sys_flg.c       イベントフラグ
  sys_dtq.c       データキュー (リングバッファ)
  sys_mbf.c       メッセージバッファ
  sys_mbx.c       メールボックス
  sys_exd.c       拡張 syscall (VGA、キーボード、スタック確保)
  sched.c         優先度ベーススケジューラ、レディキュー、タイムアウトキュー
  pool.c          メモリプール (スタック / ユーザメモリ / カーネルメモリ)
  user.c          デモ用ユーザタスク (first_task, second_task, usr_main, kbd_task)

lib/            ユーザ空間ライブラリ (.user_text にリンク)
  lib_tsk.c       タスク管理ラッパー (cre_tsk, slp_tsk, ...)
  lib_sem.c       セマフォ/フラグ/DTQ/メールボックスラッパー
  lib_mbf.c       メッセージバッファラッパー
  lib_exd.c       拡張 syscall ラッパー (print_at, set_key_task, ...)

include/        共有ヘッダ
  itron.h         ITRON 型定義、エラーコード、TFN_* 関数コード
  config.h        カーネル制限 (MAX_TSKID=16, TMAX_TPRI=16, ...)
  exd.h           非 ITRON 拡張 API プロトタイプ

docs/
  ja/             ドキュメント (日本語)
  en/             ドキュメント (英語、準備中)
```

## ドキュメント

すべて日本語 (`docs/ja/`)。`docs/ja/refs/` にはファイルごとの詳細リファレンスもある。

| ドキュメント | 内容 |
|---|---|
| [system-overview.md](docs/ja/system-overview.md) | アーキテクチャ概要 — 全体像を最初に読む |
| [i386-architecture.md](docs/ja/i386-architecture.md) | GDT、IDT、TSS、PIC、ページング — i386 ハードウェア基礎 |
| [build-system.md](docs/ja/build-system.md) | ビルドプロセス、リンカスクリプト、環境構築 |
| [boot-sector.md](docs/ja/boot-sector.md) | ブートセクタとフロッピーロード |
| [memory-map.md](docs/ja/memory-map.md) | 物理メモリ配置 |
| [context-switch.md](docs/ja/context-switch.md) | SAVE_ALL/RESTORE_ALL とコンテキストスイッチの仕組み |
| [syscall.md](docs/ja/syscall.md) | syscall 処理フロー (ユーザ → カーネル → 復帰) |
| [timer-interrupt.md](docs/ja/timer-interrupt.md) | タイマー割り込みと SAVE_ALL/RESTORE_ALL の詳細 |
| [smp-basics.md](docs/ja/smp-basics.md) | SMP 起動、APIC 設定、per-CPU データ |
| [itron-guide.md](docs/ja/itron-guide.md) | ITRON API 入門 |
| [keyboard.md](docs/ja/keyboard.md) | キーボードドライバと DTQ/MBF パイプライン |
| [timeout.md](docs/ja/timeout.md) | タイムアウト機構 (tslp_tsk, trcv_dtq, twai_sem) |
| [vga-text-mode.md](docs/ja/vga-text-mode.md) | VGA テキストモードプログラミング |
| [gdb-debugging.md](docs/ja/gdb-debugging.md) | GDB デバッグガイド |
| [source-guide.md](docs/ja/source-guide.md) | ソースファイルリファレンス |
| [docs/refs/](docs/ja/refs/) | ファイルごとの詳細リファレンス |

## ITRON syscall 対応状況

| カテゴリ          | 実装済み                                                | 状態          |
|-------------------|--------------------------------------------------------|---------------|
| タスク管理        | cre_tsk, act_tsk, slp_tsk, wup_tsk, ter_tsk, chg_pri  | 動作確認済み  |
| タスク管理        | tslp_tsk, ext_tsk, exd_tsk, sus_tsk                    | 実装あり      |
| セマフォ          | cre_sem, sig_sem, wai_sem, pol_sem, twai_sem           | 動作確認済み  |
| イベントフラグ    | cre_flg, set_flg, wai_flg, pol_flg                     | 実装あり      |
| データキュー      | cre_dtq, snd_dtq, psnd_dtq, ipsnd_dtq, rcv_dtq, trcv_dtq | 動作確認済み  |
| メッセージバッファ | cre_mbf, psnd_mbf, trcv_mbf                           | 動作確認済み  |
| メールボックス    | cre_mbx, snd_mbx, rcv_mbx                              | 実装あり      |
| タイマー          | dly_tsk (tslp_tsk ベース)                              | 動作確認済み  |
| 拡張 (独自)       | print_at, set_key_task, clear_screen, tsk_stack_alloc  | 動作確認済み  |

「動作確認済み」= 実行デモで検証済み。「実装あり」= コードは存在するが十分なテストは未実施。

## 経緯

2000 年に [t-ishii66](https://github.com/t-ishii66) が "SMP MicroITRON ver 4.0.0" として
作成。IBM PC/AT 互換機の i386 CPU 向けに Micro ITRON 4.0 仕様を個人で実装した
趣味プロジェクトだった。

2026 年に教育用プラットフォームとして再始動:
詳細なドキュメントを追加し、割り込みハンドリングと SMP コンテキストスイッチの
致命的なバグを修正し、カーネルの動作をリアルタイムで観察できる
マルチタスクデモを構築した。

## ライセンス

フリーソフトウェア。著作権表示はソースファイルを参照。

## クレジット

- 2000年版プログラミング: t-ishii66
- 2026年版割り込み構造、SMP修正: Claude Opus 4.6, t-ishii66
- ドキュメント: Claude Opus 4.6
- コードレビュー: t-ishii66
- ドキュメントレビュー: t-ishii66
- デバッグ拡張: Claude Opus 4.6

Copyright(C) 2000-2026 t-ishii66. All rights reserved.

## 参考文献

- [Micro ITRON 4.0 仕様書](https://www.ertl.jp/ITRON/SPEC/mitron4-e.html) (英語)
- [ITRON プロジェクト](https://www.ertl.jp/ITRON/) —— 東京大学 坂村健教授による設計
- [Intel i386 Programmer's Reference Manual](https://css.csail.mit.edu/6.858/2014/readings/i386.pdf)
- [OSDev Wiki](https://wiki.osdev.org/) —— ベアメタル x86 プログラミングの定番リファレンス

## キーワード

`RTOS` `リアルタイムOS` `ITRON` `Micro ITRON` `μITRON` `i386` `x86` `IA-32`
`ベアメタル` `OSカーネル` `SMP` `マルチプロセッサ` `コンテキストスイッチ` `プリエンプティブマルチタスク`
`ブートセクタ` `GDT` `IDT` `TSS` `PIC` `APIC` `割り込みハンドリング` `タスクスケジューリング`
`QEMU` `教育` `学習` `チュートリアル`
