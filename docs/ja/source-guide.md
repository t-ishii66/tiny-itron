# ソースコードガイド

tiny-itron のソースコード構成とファイルの役割を説明する。

---

## ディレクトリ構成

```
tiny-itron/
├── i386/           ← ハードウェア依存コード (x86)
│   ├── boot/       ← ブートセクタ (512 バイト)
│   ├── *.s         ← アセンブリ (ブート、割り込み、ヘルパー)
│   ├── *.c         ← C コード (初期化、ドライバ、TSS)
│   └── Makefile
├── kernel/         ← ITRON カーネル (ハードウェア非依存)
│   ├── sys_*.c     ← syscall 実装 (タスク、セマフォ、拡張等)
│   ├── sys_exd.c   ← 非 ITRON 拡張 syscall
│   ├── sched.c     ← スケジューラ
│   ├── pool.c      ← メモリアロケータ
│   ├── user.c      ← ユーザータスク (デモ)
│   └── Makefile
├── lib/            ← ユーザー側ライブラリ (syscall ラッパー)
│   ├── lib_*.c     ← int $0x99 を発行する薄いラッパー
│   ├── lib_exd.c   ← 非 ITRON 拡張 syscall ラッパー
│   └── Makefile
├── include/        ← 共有ヘッダ
│   ├── itron.h     ← ITRON 型定義 (ER, ID, VP_INT 等)
│   ├── types.h     ← 基本型 (W, H, UW 等)
│   ├── config.h    ← 定数 (MAX_TSKID, TMAX_TPRI 等)
│   ├── syscall.h   ← syscall 番号 (TFN_xxx)
│   └── exd.h       ← 非 ITRON 拡張 API プロトタイプ
├── docs/           ← ドキュメント
└── run.sh          ← QEMU 起動スクリプト
```

---

## ファイル一覧と役割

### i386/ — ハードウェア依存コード

| ファイル | 行数 | 役割 |
|----------|------|------|
| `boot/boot.s` | ~455 | **ブートセクタ**: BIOS からロード、フロッピーからカーネル読み込み |
| `start.s` | ~253 | **セカンドブート**: A20 有効化、GDT/IDT ロード、プロテクトモード移行 |
| `run.s` | ~185 | **32 ビットエントリ**: カーネルセグメント設定、CPU 判定、main() 呼び出し |
| `genasm.c` | ~106 | **ビルドツール**: GDT を table.s として生成 (ホスト上で実行) |
| `elf.c` | ~205 | **ビルドツール**: ELF → フラットバイナリ変換 (ホスト上で実行) |
| `main.c` | ~68 | **カーネルエントリ**: BSP/AP 分岐、初期化順序の起点 |
| `386.c` | ~36 | **GDT/IDT 操作**: set_gdt(), set_gate() |
| `interrupt.c` | ~452 | **割り込みハンドラ**: IDT 初期化、IRQ 0-15 の C ハンドラ |
| `intr.s` | ~598 | **割り込みエントリ**: SAVE_ALL/RESTORE_ALL、syscall エントリ (最重要) |
| `proc.c` | ~210 | **プロセス管理**: proc_init() でタスク生成、proc_create() で偽フレーム構築 |
| `tss.c` | ~63 | **TSS 管理**: esp0/ss0 の初期化と動的更新 (ltr でロード) |
| `smp.c` | ~174 | **SMP 初期化**: APIC 設定、IPI 送信、AP 起動 |
| `video.c` | ~329 | **VGA ドライバ**: printk(), vga_write_at() |
| `keyboard.c` | ~76 | **キーボードドライバ**: スキャンコード→ASCII 変換 |
| `timer.c` | ~52 | **タイマー**: PIT 初期化、timer_intr() |
| `i8259.c` | ~55 | **PIC ドライバ**: i8259 マスター/スレーブ初期化 |
| `syscall.c` | ~61 | **syscall ディスパッチ**: c_intr_syscall() → itron_syscall() |
| `page.c` | ~205 | **ページング**: 恒等マッピング、U/S アクセス制御 |
| `klib.s` | ~535 | **低レベルヘルパー**: ccli/csti, cltr, start_first/second_task, syscall 等 |
| `kernelval.c` | ~22 | **共有変数**: current_proc[], c_tskid[] 宣言 |

### kernel/ — ITRON カーネル

| ファイル | 行数 | 役割 |
|----------|------|------|
| `kernel.c` | ~60 | **カーネル初期化**: itron_init() — tsk, pool, sched, dtq 等の初期化 |
| `syscall.c` | ~51 | **syscall テーブル**: itron_syscall() — TFN_xxx → sys_xxx() ディスパッチ |
| `sys_tsk.c` | ~549 | **タスク管理**: cre/act/slp/wup/ext_tsk 等 |
| `sys_sem.c` | ~269 | **セマフォ**: cre/wai/pol/sig_sem |
| `sys_dtq.c` | ~332 | **データキュー**: cre/snd/rcv_dtq (リングバッファ) |
| `sys_flg.c` | ~238 | **イベントフラグ**: cre/set/wai_flg |
| `sys_mbx.c` | ~236 | **メールボックス**: (未使用) |
| `sys_mtx.c` | ~190 | **ミューテックス**: (未使用) |
| `sys_exd.c` | ~67 | **拡張 syscall**: 非 ITRON 系 syscall ハンドラ (VGA, キーボード, スタック) |
| `sched.c` | ~216 | **スケジューラ**: 優先度キュー、タスク切り替え判定 |
| `pool.c` | ~175 | **メモリ管理**: First-Fit アロケータ (stack_pool, mem_pool, kmem_pool) |
| `user.c` | ~394 | **ユーザータスク**: Task 1-6 のデモコード (syscall ベース API 使用) |

### lib/ — ユーザーライブラリ

| ファイル | 役割 |
|----------|------|
| `lib_tsk.c` | タスク管理の syscall ラッパー (cre_tsk, slp_tsk 等) |
| `lib_sem.c` | セマフォの syscall ラッパー (cre_sem, pol_sem 等) |
| `lib_tim.c` | タイマーの syscall ラッパー |
| `lib_int.c` | 割り込み関連ラッパー |
| `lib_mbf.c` | メッセージバッファラッパー |
| `lib_exd.c` | 非 ITRON 系 syscall ラッパー (print_at, set_key_task, tsk_stack_alloc 等) |

### include/ — 共有ヘッダ

| ファイル | 役割 |
|----------|------|
| `itron.h` | ITRON 標準型 (ER, ID, VP_INT, FP, PRI 等) と API プロトタイプ |
| `types.h` | 基本型 (W, H, UW, UH, B, UB) |
| `config.h` | 定数 (MAX_TSKID=16, TMAX_TPRI=16, MAX_SEMID=16 等) |
| `syscall.h` | syscall 番号 (TFN_CRE_TSK=-0x05 等) |
| `stdio.h` | printk プロトタイプ |
| `exd.h` | 非 ITRON 拡張 API プロトタイプ (print_at, set_key_task 等) |

---

## 重要なデータ構造

### proc_t (i386/proc.h) — プロセス制御ブロック (HW 依存)

```
proc_t {
    unsigned long kern_esp;       ← カーネルスタックの保存 ESP
    unsigned long kern_stack_top; ← カーネルスタックの先頭 (TSS.esp0 に設定)
    unsigned long saved_eflags;   ← タスク例外用 EFLAGS 保存
    int           cpu;            ← CPU アフィニティ (0 or 1)
}
```

- 各タスクは 4KB のカーネルスタックを持つ (KERN_STACK_BASE + (tskid+1) * 4096)
- 割り込み時に SAVE_ALL が 9 レジスタをカーネルスタックに push し、pt_regs フレームを形成
- タスクスイッチは intr_leave が kern_esp を差し替えるだけで完了

### T_TSK (kernel/types.h) — タスク制御ブロック (ITRON 層)

```
T_TSK {
    ID      tskid;    ← タスク ID (1〜16)
    STAT    tskstat;  ← タスク状態 (TTS_RUN, TTS_RDY, TTS_WAI, ...)
    PRI     tskpri;   ← 現在の優先度 (1=最高, 16=最低)
    PRI     tskbpri;  ← 基本優先度
    UINT    wupcnt;   ← 起床要求カウンタ
    T_CTSK  ctsk;     ← 生成時パラメータのコピー
    T_LINK  plink;    ← 優先度キューのリンク
    T_LINK  wlink;    ← 待ちキューのリンク
    proc_t* proc;     ← HW 依存部へのポインタ
    ...
}
```

### 共有変数 (i386/kernelval.c)

```c
proc_t* current_proc[2];  // per-CPU: 現在実行中のプロセス
ID      c_tskid[2];       // per-CPU: 現在のタスク ID
INT     next_tsk_flag[2];  // per-CPU: リスケジュール要求フラグ
```

---

## 補足

### `W apic` パターン

多くの関数が第一引数に `W apic` を取る。これは CPU 番号 (0 or 1) で、
per-CPU 配列のインデックスとして使われる:
- `current_proc[apic]` — その CPU の現在タスク
- `c_tskid[apic]` — その CPU の現在タスク ID
- `next_tsk_flag[apic]` — リスケジュール要求

### 同名ヘッダ

`kernel/types.h` と `include/types.h` は**別ファイル**。
前者はカーネル内部構造体 (T_TSK, T_SEM 等)、後者は基本型 (W, H 等)。

### `sys_` と `i` プレフィックス

- `sys_slp_tsk()` — syscall ハンドラ (タスクコンテキスト)
- `iwup_tsk()` — 割り込みハンドラから呼ぶ版 (割り込みコンテキスト)

---

## 関連ドキュメント

- [システム概要](system-overview.md) — タスク全体の動きと連携
- [ITRON ガイド](itron-guide.md) — ITRON API の概念と使い方
- [コンテキストスイッチ詳解](context-switch.md) — SAVE_ALL/RESTORE_ALL と intr_leave の詳解
- [i386 アーキテクチャ](i386-architecture.md) — GDT、IDT、特権レベル
- [メモリマップ](memory-map.md) — 物理アドレス配置
- [SMP 基礎](smp-basics.md) — 2 CPU 構成の仕組み
- [GDB デバッグ](gdb-debugging.md) — GDB の使い方
