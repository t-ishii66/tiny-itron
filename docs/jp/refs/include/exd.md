# exd.h

対象ファイル: `include/exd.h`

## 概要

非 ITRON 拡張ユーザー API のプロトタイプ宣言を提供するヘッダファイル。ITRON v4.0.0 仕様には含まれないハードウェア依存機能 (VGA 出力、キーボード入力、スタック割り当て) をユーザータスクから利用するためのインタフェースを定義する。

実装は `lib/lib_exd.c` にあり、各関数は `syscall()` 経由でカーネルの対応ハンドラ (`kernel/sys_exd.c`) を呼び出す。

依存ヘッダ: `itron.h` (VP 型の定義のため)

## インクルードガード

```c
#ifndef _EXD_H
#define _EXD_H
```

## 関数プロトタイプ

### VGA 出力

| プロトタイプ | 説明 |
|-------------|------|
| `void print_at(int row, int col, char *s, unsigned char attr)` | 指定位置に文字列を表示 |
| `void print_dec_at(int row, int col, unsigned long n, int width, unsigned char attr)` | 指定位置に 10 進数を表示 |
| `void clear_screen(void)` | 画面全体をクリア |
| `void fill_at(int row, int col, int len, int ch, unsigned char attr)` | 指定位置を指定文字で塗りつぶし |

### キーボード入力

| プロトタイプ | 説明 |
|-------------|------|
| `int key_read(void)` | 廃止済み (E_NOSPT を返す、DTQ に移行) |
| `void set_key_task(int id)` | キーボード ISR の送信先 DTQ ID を設定 |

### メモリ管理

| プロトタイプ | 説明 |
|-------------|------|
| `VP tsk_stack_alloc(int size)` | スタックプールからメモリ割り当て |

## 命名規約

ITRON 標準 syscall と非 ITRON 拡張 syscall では、関数名の対応関係と TFN コード体系が異なる。

### ITRON 標準 syscall

ユーザー関数名とカーネルハンドラ名が `sys_` プレフィックスの有無で対応する。

```
ユーザー: cre_tsk()  →  カーネル: sys_cre_tsk()
ユーザー: slp_tsk()  →  カーネル: sys_slp_tsk()
ユーザー: sig_sem()  →  カーネル: sys_sig_sem()
```

### 非 ITRON 拡張 syscall

ユーザー関数名とカーネルハンドラ名が異なる命名体系を使う。ユーザー側は簡潔な英語名、カーネル側はハードウェア機能を反映した名前となる。

```
ユーザー: print_at()     →  カーネル: sys_vga_write_at()
ユーザー: print_dec_at() →  カーネル: sys_vga_write_dec_at()
ユーザー: clear_screen() →  カーネル: sys_vga_clear()
ユーザー: fill_at()      →  カーネル: sys_vga_fill_at()
ユーザー: key_read()     →  カーネル: sys_key_getc_sc()  (廃止済み)
ユーザー: set_key_task() →  カーネル: sys_key_set_task()  (DTQ ID を登録)
ユーザー: tsk_stack_alloc()  →  カーネル: sys_stack_alloc_sc()
```

### TFN コード体系

ファンクションコード (TFN) は `include/itron.h` で定義される。ITRON 標準と非 ITRON 拡張で番号範囲が分離されている。

| 区分 | TFN 範囲 | 例 |
|------|---------|-----|
| ITRON 標準 | -0x05 〜 -0xcd | `TFN_CRE_TSK` (-0x05), `TFN_ACRE_ISR` (-0xcd) |
| 非 ITRON 拡張 (レガシー) | -0xe1 〜 -0xe2 | `TFN_EXD_PRINT` (-0xe1), `TFN_EXD_TCPIP` (-0xe2) |
| 非 ITRON 拡張 (新規) | -0xe3 〜 -0xe9 | `TFN_EXD_VGA_WRITE` (-0xe3) 〜 `TFN_EXD_STACK_ALLOC` (-0xe9) |

具体的な TFN 定数 (`itron.h` より):

| 定数 | 値 | 対応カーネルハンドラ |
|------|-----|---------------------|
| `TFN_EXD_VGA_WRITE` | -0xe3 | `sys_vga_write_at` |
| `TFN_EXD_VGA_DEC` | -0xe4 | `sys_vga_write_dec_at` |
| `TFN_EXD_VGA_CLEAR` | -0xe5 | `sys_vga_clear` |
| `TFN_EXD_VGA_FILL` | -0xe6 | `sys_vga_fill_at` |
| `TFN_EXD_KEY_GETC` | -0xe7 | `sys_key_getc_sc` |
| `TFN_EXD_KEY_SETTASK` | -0xe8 | `sys_key_set_task` |
| `TFN_EXD_STACK_ALLOC` | -0xe9 | `sys_stack_alloc_sc` |

## 補足

- `exd.h` はユーザータスクソース (`kernel/user.c`) からインクルードされる。
- `itron.h` を内部でインクルードするため、`VP` 型や `ER` 型は自動的に利用可能となる。
- ITRON 標準 API のプロトタイプは `itron.h` 自体に宣言されており、`exd.h` は非 ITRON 拡張 API のみを担当する。
