# main.c / mainP.h

対象ファイル: `i386/main.c`, `i386/mainP.h`

## 概要

カーネルのエントリポイント。BSP (CPU 0) と AP (CPU 1) の両方がここに到達する。
`cpu_num` の値に基づいて BSP パスと AP パスに分岐する。

BSP パスでは、i386 ハードウェア初期化、ITRON カーネル初期化、タスク・TSS 初期化を
順番に実行し、最後に SMP 初期化 (`smp_init()`) を呼び出す。`smp_init()` 内で
AP への SIPI 送信、PIT/キーボード開始、`start_first_task()` が行われるため、
`main()` の `return 0` には到達しない。

AP パスでは `smp_ap_init()` を呼び出し、Local APIC 初期化と
`start_second_task()` を実行する。こちらも `return 0` には到達しない。

## 定数・マクロ

なし (`mainP.h` は `all_init` の static 宣言のみ)。

## 構造体・型

なし。

## グローバル変数

なし (他モジュールで定義された変数を参照)。

## 関数リファレンス

### main

```c
int main(void)
```

**概要:** カーネルのエントリポイント。Protected Mode に遷移した後、`run.s` から呼び出される。

**引数:** なし

**戻り値:** `int` -- 常に 0 だが、実際には到達しない。

**処理内容:**

1. `ccli()` で割り込みを禁止する
2. `cpu_num == 0` (BSP) の場合:
   - `all_init()` -- i386 ハードウェア初期化
   - `page_init()` -- ページテーブル構築 (恒等マッピング + U/S アクセス制御)
   - `page_enable()` -- CR3 ロード、CR0.PG=1 でページング有効化
   - `itron_init()` -- ITRON カーネル初期化 (タスク管理構造体、メモリプール、スケジューラ)
   - `proc_init()` -- プロセス構造体初期化、タスク 1/2/5/6 の生成
   - `tss_init()` -- TSS (Task State Segment) 初期化 (両 CPU 分)
   - `cltr(SEL_TSS_DUMMY0)` -- CPU 0 の TR レジスタに dummy TSS をロード
   - `cpu_num = 1` -- AP が `main()` に入った時に AP パスに分岐するよう設定
   - `smp_init()` -- SMP 初期化 (APIC 設定、SIPI 送信、タスク起動)
3. `cpu_num != 0` (AP) の場合:
   - `page_enable()` -- BSP が構築したページテーブルを共有してページング有効化
   - `smp_ap_init()` -- AP 側の Local APIC 初期化とタスク起動

**呼び出し元:** `run.s` (アセンブリ起動コード)

**注意点:**
- `itron_init()` は `proc_init()` より前に呼ぶ必要がある。逆順だと `tsk_init()` がタスク状態をゼロクリアしてしまい、`proc_init()` で設定した状態が失われる。
- `main()` 内で `csti()` を呼んではいけない。割り込み有効化は `start_first_task()` / `start_second_task()` の TSS スイッチで EFLAGS の IF=1 をロードすることで行う。
- `cpu_num` はグローバル変数であり、BSP が `cpu_num = 1` に設定した後に AP が `main()` に到達するシーケンスに依存している。

---

### all_init

```c
static void all_init(void)
```

**概要:** i386 ハードウェアの初期化を行う静的関数。BSP のみが呼び出す。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. `idt_init()` -- IDT (割り込みディスクリプタテーブル) を初期化
2. `video_init()` -- 6845 CRT コントローラ (VGA テキストモード) を初期化
3. 著作権表示を `printk()` で出力
4. `i8259_init()` -- PIC (8259A) を初期化
5. `timer_init()` -- PIT (8254) タイマーを初期化
6. `key_init()` -- キーボードコントローラを初期化

**呼び出し元:** `main()` (BSP パス内)

**注意点:**
- `mainP.h` で `static` 宣言されており、`main.c` 内でのみ使用可能。
- 初期化の順序は重要。`idt_init()` が最初に呼ばれ、以降の割り込み関連初期化が IDT に依存する。

## 補足

### 起動シーケンス全体の流れ

```
BIOS → boot.s → start.s (Protected Mode 移行) → run.s → main()
  BSP: all_init → page_init → page_enable → itron_init → proc_init → tss_init → smp_init
    → SIPI → AP 起動待ち → timer_start → key_start → start_first_task
  AP:  page_enable → smp_ap_init → APIC 初期化 → cpu_second=1 → start_second_task
```

### mainP.h の内容

`mainP.h` は `all_init()` の `static` 関数プロトタイプ宣言のみを含む。
これは C の慣例として、private ヘッダに static 関数の前方宣言を分離するパターンである。
