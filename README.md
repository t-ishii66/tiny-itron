# tiny-itron

[Micro ITRON 4.0](https://www.ertl.jp/ITRON/SPEC/mitron4-e.html) 仕様をベースにした、i386 向けのおもちゃの RTOS カーネル。SMP (2 CPU) 対応。

## このプロジェクトの目的

OS の仕組みは、ほとんどの人が教科書で学ぶ。
このプロジェクトは、**実際のカーネルコードを読んで動かすことで学ぶ** ためにある。

tiny-itron は Micro ITRON 4.0 のタスク・セマフォ・イベントフラグといった API スタイルを
借りているが、仕様を忠実に・完全に実装することは目指していない。
多くの ITRON syscall はスタブのままであり、内部アーキテクチャも
簡潔さを優先して仕様から自由に逸脱している。これはおもちゃのカーネルである
——そして、それこそが利点である。

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

- **ベアメタル上のマルチタスキング** —— アセンブリの `save`/`restore` が
  13 個のレジスタを保存・復元してタスクを切り替える仕組み、
  スケジューラが次のタスクを選ぶ方法、2 つの CPU がスピンロックで協調する方法、
  キーボード割り込みが実行中のタスクをプリエンプトする仕組み。
  これらすべてを GDB でリアルタイムに観察できる。

目標はプロダクション品質ではない。目標は、すべての定数に理由があり、
すべてのレジスタ保存に *なぜそうするか* のコメントがあり、
ユーザ空間からハードウェアまでのあらゆる経路を `grep` と `gdb` だけで
追跡できることである。

## 動作画面

カーネルを実行すると、QEMU に VGA テキストモードの画面が表示される:

```
  TinyItron/386 SMP (2 CPU)                            [Ctrl+C to quit]
  ============================================================================

      Timer  tick = 12345

  [CPU0] Task1 #     142     dtq: a
  [CPU0] Task3 #      71     LOCK
           Shared (sem 1)    #      38
  [CPU1] Task2 #     284     BUSY
  [CPU1] Task4 > hello world
```

- **Task 1** と **Task 3** が CPU 0 上で `wup_tsk`/`slp_tsk` により交互に実行
- **Task 2** は CPU 1 上で継続的に実行
- **Task 3** (CPU 0) と **Task 2** (CPU 1) がバイナリセマフォで共有カウンタを
  競合アクセス —— LOCK/BUSY の状態がリアルタイムに表示される
- **Task 4** は最高優先度でキーボード入力をエコーし、Task 2 をプリエンプトする
- **Timer tick** は CPU 0 上の PIT (IRQ0) で駆動

## ビルドと実行

**必要なもの:** GCC (i386 クロスまたはネイティブ 32 ビット)、GNU Make、QEMU (`qemu-system-i386`)

```bash
# ビルド
make -C kernel && make -C lib && make -C i386

# QEMU で実行 (2 CPU)
./run.sh          # curses モード (ターミナル上に VGA テキスト表示、Ctrl+C で終了)
./run.sh -g       # GTK ウィンドウモード (Ctrl+Alt+G でグラブ解除)
./run.sh -G       # GDB モード (ポート 1234 で接続待ち)
```

## GDB デバッグ

```bash
# ターミナル 1
./run.sh -G

# ターミナル 2
gdb i386/_kernel
(gdb) set architecture i386
(gdb) target remote :1234
(gdb) break first_task
(gdb) continue
(gdb) info threads            # 両方の CPU が見える
(gdb) p task_count[1]         # Task 1 の実行回数
(gdb) p shared_count          # セマフォで保護された共有カウンタ
```

詳しくは [docs/gdb-debugging.md](docs/gdb-debugging.md) を参照。

## アーキテクチャ

### ハードウェアスタック

```
+---------------------+
|  ブートセクタ (512B) |  BIOS INT 13h でフロッピーからカーネルをロード
+---------------------+
|  GDT / IDT / TSS    |  プロテクトモード設定、セグメントディスクリプタ
+---------------------+
|  PIC (i8259)         |  IRQ ルーティング —— 外部 IRQ はすべて CPU 0 に配送
+---------------------+
|  Local APIC          |  CPU 識別、CPU ごとのタイマー、EOI
+---------------------+
|  ページング          |  恒等写像、User/Supervisor アクセス制御
+---------------------+
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
                                    -> save (regs -> proc.reg[])
                                    -> c_intr_syscall
                                      -> itron_syscall
                                        -> syscall_entry[0x11] = sys_slp_tsk
                                    -> 戻り値をセーブフレームに書き込み
                                    -> restore (タスク切替の可能性あり)
                                    -> iret  ----[gate]----> ユーザに復帰
```

詳しくは [docs/syscall.md](docs/syscall.md) を参照。

### コンテキストスイッチ

```
save:    APIC ID を読み取り → CPU ごとの current_proc[] を選択
         proc->stack を 52 バイト進める (13 レジスタ × 4)
         ECX,EDX,ESP,EIP,EBP,ESI,EDI,EFLAGS,EAX,EBX,DS,ES を保存

restore: proc->stack を 52 バイト戻す
         ネストカウント == 0 なら: sched_next_tsk_check() を呼ぶ
           → current_proc[] が別のタスクに変わる可能性あり
         (別のタスクかもしれない) セーブ領域からレジスタを復元
         iret フレームを新タスクの EIP/ESP で上書き

iret:    CS:EIP, SS:ESP, EFLAGS をアトミックに復元 → 新タスクが実行開始
```

詳しくは [docs/timer-interrupt.md](docs/timer-interrupt.md) および [docs/context-switch.md](docs/context-switch.md) を参照。

## ソース構成

```
i386/           アーキテクチャ依存コード
  boot/           ブートセクタ (boot.s) とローダテーブル
  start.s         リアルモード → プロテクトモード遷移
  main.c          カーネルエントリポイント、初期化シーケンス
  intr.s          save/restore、全割り込み/例外/syscall スタブ
  klib.s          syscall ラッパー、TSS タスク起動、I/O ポートヘルパ
  proc.c          タスク proc_t 管理、CPU アフィニティ
  interrupt.c     IDT セットアップ、PIC 初期化、sched_next_tsk_check
  page.c          ページディレクトリ/テーブル設定 (恒等写像)
  smp.c           AP 起動 (INIT/SIPI)、Local APIC タイマー
  syscall.c       c_intr_syscall (戻り値書き戻し)
  video.c         VGA テキストモードドライバ (0xB8000)
  keyboard.c      PS/2 キーボードドライバ、リングバッファ、IRQ1
  timer.c         PIT (8254) 初期化
  tss.c           TSS ディスクリプタ設定

kernel/         アーキテクチャ非依存 ITRON カーネル
  syscall.c       itron_syscall ディスパッチャ
  syscallP.h      syscall_entry[] ディスパッチテーブル (234 エントリ)
  sys_tsk.c       タスク管理 (cre/act/slp/wup/ter_tsk, ...)
  sys_sem.c       セマフォ (cre/sig/wai/pol_sem)
  sys_flg.c       イベントフラグ
  sys_dtq.c       データキュー
  sys_mbx.c       メールボックス
  sys_exd.c       拡張 syscall (VGA、キーボード、スタック確保)
  sched.c         優先度ベーススケジューラ、レディキュー
  pool.c          メモリプール (スタック確保)
  user.c          デモ用ユーザタスク (first_task, second_task, kbd_task)

lib/            ユーザ空間ライブラリ (.user_text にリンク)
  lib_tsk.c       タスク管理ラッパー (cre_tsk, slp_tsk, ...)
  lib_sem.c       セマフォ/フラグ/DTQ/メールボックスラッパー
  lib_exd.c       拡張 syscall ラッパー (print_at, key_read, ...)

include/        共有ヘッダ
  itron.h         ITRON 型定義、エラーコード、TFN_* 関数コード
  config.h        カーネル制限 (MAX_TSKID=16, TMAX_TPRI=16, ...)

docs/           ドキュメント (日本語)
  system-overview.md    アーキテクチャ概要
  syscall.md            syscall 処理フロー (ユーザ → カーネル → 復帰)
  timer-interrupt.md    タイマー割り込みと save/restore の詳細
  context-switch.md     コンテキストスイッチの仕組み
  memory-map.md         物理メモリ配置
  build-system.md       ビルドプロセスとリンカスクリプト
  boot-sector.md        ブートセクタとフロッピーロード
  smp-basics.md         SMP 起動と APIC 設定
  i386-architecture.md  GDT、IDT、TSS、ページング
  itron-guide.md        ITRON API 入門
  gdb-debugging.md      GDB デバッグガイド
  vga-text-mode.md      VGA テキストモードプログラミング
  source-guide.md       ソースファイルリファレンス
  refs/                 ファイルごとのリファレンスドキュメント
```

## ITRON syscall 対応状況

| カテゴリ          | 実装済み                                                | 状態          |
|-------------------|--------------------------------------------------------|---------------|
| タスク管理        | cre_tsk, act_tsk, slp_tsk, wup_tsk, ter_tsk, chg_pri  | 動作確認済み  |
| タスク管理        | tslp_tsk, ext_tsk, exd_tsk, sus_tsk                    | 実装あり      |
| セマフォ          | cre_sem, sig_sem, wai_sem, pol_sem                     | 動作確認済み  |
| イベントフラグ    | cre_flg, set_flg, wai_flg, pol_flg                     | 実装あり      |
| データキュー      | cre_dtq, snd_dtq, psnd_dtq, prcv_dtq                  | 動作確認済み  |
| メールボックス    | cre_mbx, snd_mbx, rcv_mbx                              | 実装あり      |
| 拡張 (独自)       | print_at, key_read, clear_screen, tsk_stack_alloc          | 動作確認済み  |

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
