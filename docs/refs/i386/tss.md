# tss.c / tss.h / tssP.h

対象ファイル: `i386/tss.c`, `i386/tss.h`, `i386/tssP.h`

## 概要

i386 の Task State Segment (TSS) を管理するモジュール。TSS はハードウェアタスクスイッチと
特権レベル遷移時のスタック切り替えに使用される。

本カーネルでは、最初のタスク起動時にハードウェア TSS スイッチ (`ljmp`) を使用して
Ring 0 から Ring 3 に遷移する。以降のタスク切り替えはソフトウェアコンテキストスイッチ
(`save`/`restore` in `intr.s`) で行い、TSS は Ring 3 → Ring 0 遷移時の
スタックポインタ (ESP0/SS0) の参照にのみ使用される。

各 CPU に 2 つの TSS が用意される:
- `tss0` / `tss1`: Ring 3 タスク用 TSS (ユーザーモードセグメントを設定)
- `tss_dummy0` / `tss_dummy1`: カーネル用ダミー TSS (初期 TR ロード用)

## 定数・マクロ

### INIT_EFLAGS (tssP.h)

| 定数 | 値 | 説明 |
|------|------|------|
| `INIT_EFLAGS` | `0x200` | 初期 EFLAGS 値。IF=1 (割り込み許可) |

## 構造体・型

### tss_t (Task State Segment)

```c
typedef struct tss {
    unsigned short  prev_link;      /* 前タスクのセレクタ (ネストしたタスクスイッチ用) */
    unsigned short  dummy0;
    unsigned long   esp0;           /* Ring 0 スタックポインタ */
    unsigned short  ss0;            /* Ring 0 スタックセグメント */
    unsigned short  dummy1;
    unsigned long   esp1;           /* Ring 1 スタックポインタ */
    unsigned short  ss1;            /* Ring 1 スタックセグメント */
    unsigned short  dummy2;
    unsigned long   esp2;           /* Ring 2 スタックポインタ */
    unsigned short  ss2;            /* Ring 2 スタックセグメント */
    unsigned short  dummy3;
    unsigned long   cr3;            /* ページディレクトリベース */
    unsigned long   eip;            /* 命令ポインタ */
    unsigned long   eflags;         /* フラグレジスタ */
    unsigned long   eax;
    unsigned long   ecx;
    unsigned long   edx;
    unsigned long   ebx;
    unsigned long   esp;            /* スタックポインタ */
    unsigned long   ebp;            /* フレームポインタ */
    unsigned long   esi;
    unsigned long   edi;
    unsigned short  es;             /* ES セグメント */
    unsigned short  dummy4;
    unsigned short  cs;             /* CS セグメント */
    unsigned short  dummy5;
    unsigned short  ss;             /* SS セグメント */
    unsigned short  dummy6;
    unsigned short  ds;             /* DS セグメント */
    unsigned short  dummy7;
    unsigned short  fs;             /* FS セグメント */
    unsigned short  dummy8;
    unsigned short  gs;             /* GS セグメント */
    unsigned short  dummy9;
    unsigned short  ldt;            /* LDT セレクタ */
    unsigned short  dummy10;
    unsigned short  t;              /* デバッグトラップフラグ */
    unsigned short  io_base;        /* I/O マップベースアドレス */
} tss_t;
```

Intel i386 の TSS 構造 (104 バイト) に対応する構造体。
`dummy*` フィールドは 16 ビットセグメントレジスタを 32 ビット境界にアラインするためのパディング。

## グローバル変数

### tss_dummy0, tss_dummy1 (tssP.h)

```c
static tss_t tss_dummy0, tss_dummy1;
```

カーネルモード用ダミー TSS。起動時に `cltr(SEL_TSS_DUMMY0/1)` で TR レジスタにロードされる。
`ljmp` でタスク TSS (`tss0`/`tss1`) にスイッチする前の「出発元」TSS として必要。

### tss0, tss1 (tssP.h)

```c
static tss_t tss0, tss1;
```

Ring 3 タスク用 TSS。`start_first_task()` / `start_second_task()` の `ljmp` でロードされ、
ユーザーモードのセグメントセレクタと EFLAGS (IF=1) が CPU に設定される。

## 関数リファレンス

### tss_init

```c
void tss_init(void)
```

**概要:** 全 4 つの TSS を初期化し、対応する GDT エントリを設定する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

**CPU 0 (BSP):**

1. `tss0` を設定 (`set_tss`):
   - `func` = `first_task`
   - `cs` = `SEL_U32_C | 3` (ユーザーコード、RPL=3)
   - `ds` = `SEL_U32_D` (ユーザーデータ)
   - `ss` = `SEL_U32_S | 3` (ユーザースタック、RPL=3)
   - `esp` = `CPU0_SP3` (0x780000)
   - `ss0` = `SEL_K32_S` (カーネルスタック)
   - `esp0` = `CPU0_SP0` (0x790000)
   - `eflags` = `INIT_EFLAGS` (IF=1)
2. `tss_dummy0` を設定 (`set_tss`):
   - カーネルモードセグメント (`SEL_K32_C/D/S`)
   - 他は `tss0` と同じスタック設定
3. GDT エントリを設定:
   - `SEL_TSS0` (0x38): `tss0` の TSS ディスクリプタ
   - `SEL_TSS_DUMMY0` (0x48): `tss_dummy0` の TSS ディスクリプタ

**CPU 1 (AP):**

4. `tss1` を設定 (`set_tss`):
   - `func` = `second_task`
   - `esp` = `CPU1_SP3` (0x750000)
   - `esp0` = `CPU1_SP0` (0x760000)
   - 他は `tss0` と同様のユーザーモード設定
5. `tss_dummy1` を設定 (`set_tss`):
   - カーネルモードセグメント
6. GDT エントリを設定:
   - `SEL_TSS1` (0x40): `tss1` の TSS ディスクリプタ
   - `SEL_TSS_DUMMY1` (0x50): `tss_dummy1` の TSS ディスクリプタ

**呼び出し元:** `main()` (BSP パス)

**注意点:**
- `tss_init()` は BSP (CPU 0) で実行されるが、CPU 1 用の TSS も同時に初期化する。
- GDT エントリの TSS ディスクリプタは `ST_TSS` (0x89) タイプ。ハードウェアタスクスイッチ後、CPU は自動的にこれを BSY (0x8B) に変更する。
- `RPL=3` が CS と SS に設定されているため、`ljmp` 後にユーザーモード (Ring 3) で実行が開始される。

---

### set_tss

```c
void set_tss(tss_t* t, int (*func)(),
             unsigned short cs, unsigned short ds,
             unsigned short ss, unsigned char* esp,
             unsigned short ss0, unsigned char* esp0,
             unsigned long eflags)
```

**概要:** TSS 構造体をゼロクリアし、指定されたパラメータで初期化する。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `t` | `tss_t*` | 初期化対象の TSS 構造体 |
| `func` | `int (*)()` | タスク開始アドレス (EIP に設定) |
| `cs` | `unsigned short` | コードセグメントセレクタ |
| `ds` | `unsigned short` | データセグメントセレクタ (DS に設定) |
| `ss` | `unsigned short` | スタックセグメントセレクタ |
| `esp` | `unsigned char*` | スタックポインタ初期値 |
| `ss0` | `unsigned short` | Ring 0 スタックセグメントセレクタ |
| `esp0` | `unsigned char*` | Ring 0 スタックポインタ |
| `eflags` | `unsigned long` | 初期 EFLAGS 値 |

**戻り値:** なし (`void`)

**処理内容:**

1. TSS 構造体全体を `'\0'` でゼロクリア (104 バイト)
2. `eip`, `cs`, `ds`, `esp`, `ss`, `esp0`, `ss0`, `eflags` を設定

**呼び出し元:** `tss_init()`

**注意点:**
- ゼロクリアにより、`cr3`, `es`, `fs`, `gs`, `ldt`, `t`, `io_base` などは 0 に設定される。
- `io_base` が 0 のため、Ring 3 からの I/O ポートアクセスは I/O ビットマップに基づいて制御されるが、本カーネルではページングを使用しないため影響は限定的。

## 補足

### TSS とタスクスイッチの関係

```
起動時:
  main() → cltr(SEL_TSS_DUMMY0)  ← TR にダミー TSS をロード
  smp_init() → start_first_task()
    → ljmp SEL_TSS0               ← ハードウェアタスクスイッチ
    → Ring 3 で first_task() 開始   (EFLAGS.IF=1 が自動ロード)

以降:
  割り込み (Ring 3→Ring 0):
    TSS の ESP0/SS0 がカーネルスタックとして自動ロード
  タスクスイッチ:
    intr.s の save/restore でソフトウェアスイッチ (TSS は不使用)
```

### スタックポインタの配置

| 定数 | アドレス | 用途 |
|------|----------|------|
| `CPU0_SP` | `0x7a0000` | CPU 0 初期スタック (起動時) |
| `CPU0_SP0` | `0x790000` | CPU 0 Ring 0 スタック (TSS ESP0) |
| `CPU0_SP3` | `0x780000` | CPU 0 Ring 3 スタック (TSS ESP) |
| `CPU1_SP` | `0x770000` | CPU 1 初期スタック (起動時) |
| `CPU1_SP0` | `0x760000` | CPU 1 Ring 0 スタック (TSS ESP0) |
| `CPU1_SP3` | `0x750000` | CPU 1 Ring 3 スタック (TSS ESP) |
