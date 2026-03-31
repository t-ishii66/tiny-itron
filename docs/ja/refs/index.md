# ソースコードリファレンス索引

tiny-itron カーネルの全ソースファイルに対応するテクニカルリファレンス。

## i386/ --- ハードウェア依存層

### ブート・起動

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [boot-boot](i386/boot-boot.md) | boot/boot.s | BIOS ブートローダー |
| [start](i386/start.md) | start.s | リアルモード→プロテクトモード遷移 |
| [run](i386/run.md) | run.s | 32 ビットエントリポイント |
| [table](i386/table.md) | table.s, 386.s | GDT 定義 |

### コア

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [main](i386/main.md) | main.c, mainP.h | カーネルメイン |
| [386](i386/386.md) | 386.c, 386.h, 386P.h | セグメント・ゲート記述子 |
| [intr](i386/intr.md) | intr.s | 割り込みハンドラ (save/restore) |
| [interrupt](i386/interrupt.md) | interrupt.c/h/P.h | 割り込み管理 |
| [proc](i386/proc.md) | proc.c/h/P.h | プロセス管理 |
| [tss](i386/tss.md) | tss.c/h/P.h | タスク状態セグメント |
| [smp](i386/smp.md) | smp.c/h/P.h | SMP 対応 (APIC) |
| [page](i386/page.md) | page.c/h/P.h | ページング (恒等マッピング) |
| [syscall](i386/syscall.md) | syscall.c | syscall ディスパッチャ (i386 層) |
| [kernelval](i386/kernelval.md) | kernelval.c/h | グローバル変数 |

### デバイスドライバ

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [i8259](i386/i8259.md) | i8259.c/h/P.h | PIC (8259A) |
| [timer](i386/timer.md) | timer.c/h/P.h | PIT (8253) タイマー |
| [keyboard](i386/keyboard.md) | keyboard.c/h/P.h | PS/2 キーボード |
| [floppy](i386/floppy.md) | floppy.c/h/P.h | フロッピーディスク |
| [video](i386/video.md) | video.c/s/h/P.h | VGA テキストモード |

### ユーティリティ

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [klib](i386/klib.md) | klib.s, klib.h | カーネルライブラリ (I/O, CLI/STI) |
| [addr](i386/addr.md) | addr.h | メモリマップ・セレクタ |
| [io](i386/io.md) | io.h | I/O ポート定義 |

### ホストツール

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [genasm](i386/genasm.md) | genasm.c | GDT バイナリ生成 |
| [elf](i386/elf.md) | elf.c | ELF ローダー |

## kernel/ --- ITRON カーネル

### コア

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [kernel](kernel/kernel.md) | kernel.c/h | カーネル初期化 |
| [syscall](kernel/syscall.md) | syscall.c, syscallP.h | syscall ディスパッチテーブル |
| [sched](kernel/sched.md) | sched.c/h | スケジューラ |
| [pool](kernel/pool.md) | pool.c/h/P.h | メモリプール管理 |
| [user](kernel/user.md) | user.c | ユーザータスク (デモ) |
| [sys_exd](kernel/sys_exd.md) | sys_exd.c | 非 ITRON 拡張 syscall |
| [types](kernel/types.md) | types.h, val.h | カーネル内部データ構造 |

### タスク管理

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [sys_tsk](kernel/sys_tsk.md) | sys_tsk.c/h | タスク管理 syscall |

### 同期・通信

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [sys_sem](kernel/sys_sem.md) | sys_sem.c/h | セマフォ |
| [sys_flg](kernel/sys_flg.md) | sys_flg.c/h | イベントフラグ |
| [sys_dtq](kernel/sys_dtq.md) | sys_dtq.c/h | データキュー |
| [sys_mbx](kernel/sys_mbx.md) | sys_mbx.c/h | メールボックス |
| [sys_mtx](kernel/sys_mtx.md) | sys_mtx.c/h | ミューテックス |
| [sys_mbf](kernel/sys_mbf.md) | sys_mbf.c/h | メッセージバッファ |
| [sys_por](kernel/sys_por.md) | sys_por.c/h | ランデブポート |

### メモリ管理

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [sys_mpf](kernel/sys_mpf.md) | sys_mpf.c/h | 固定長メモリプール |
| [sys_mpl](kernel/sys_mpl.md) | sys_mpl.c/h | 可変長メモリプール |

### 時間管理

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [sys_tim](kernel/sys_tim.md) | sys_tim.c/h | システム時刻 |
| [sys_cyc](kernel/sys_cyc.md) | sys_cyc.c/h | 周期ハンドラ |
| [sys_alm](kernel/sys_alm.md) | sys_alm.c/h | アラームハンドラ |
| [sys_ovr](kernel/sys_ovr.md) | sys_ovr.c/h | オーバーランハンドラ |

### システム管理

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [sys_rdq](kernel/sys_rdq.md) | sys_rdq.c/h | レディキュー・CPU 制御 |
| [sys_isr](kernel/sys_isr.md) | sys_isr.c/h | 割り込みサービスルーチン |
| [sys_tex](kernel/sys_tex.md) | sys_tex.c | タスク例外処理 |
| [sys_sns](kernel/sys_sns.md) | sys_sns.c/h | センス関数 (スタブ) |

## lib/ --- ユーザーライブラリ

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [lib_tsk](lib/lib_tsk.md) | lib_tsk.c | タスク管理ラッパー |
| [lib_sem](lib/lib_sem.md) | lib_sem.c | セマフォ・フラグ・DTQ・MBX ラッパー |
| [lib_tim](lib/lib_tim.md) | lib_tim.c | 時間管理ラッパー |
| [lib_int](lib/lib_int.md) | lib_int.c | 割り込み・システム管理ラッパー |
| [lib_mbf](lib/lib_mbf.md) | lib_mbf.c | MTX・MBF・POR・MPF・MPL ラッパー |
| [lib_exd](lib/lib_exd.md) | lib_exd.c | 非 ITRON 拡張ラッパー |

## include/ --- 共通ヘッダ

| リファレンス | 対象ファイル | 概要 |
|-------------|-------------|------|
| [itron](include/itron.md) | itron.h | ITRON API 型・定数・ファンクションコード |
| [types](include/types.md) | types.h | ITRON 標準データ構造体 |
| [config](include/config.md) | config.h | カーネル構成定数 |
| [syscall](include/syscall.md) | syscall.h | syscall 定数 |
| [stdio](include/stdio.md) | stdio.h | メモリ管理構造体・printk |
| [exd](include/exd.md) | exd.h | 非 ITRON 拡張 API |
