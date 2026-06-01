# sys_exd.c

対象ファイル: `kernel/sys_exd.c`

## 概要

非 ITRON 拡張システムコール (extended syscall) のカーネル側ハンドラを提供するファイル。VGA テキスト出力、キーボード入力、スタック割り当てなど、ITRON v4.0.0 仕様には含まれないハードウェア依存機能を syscall 経由でユーザータスクに公開する。

全ハンドラは第 1 引数に `W apic` (APIC ID) を受け取る。これは syscall ディスパッチャ (`c_intr_syscall`) が自動的に付与する呼び出し規約であり、ハンドラ側では使用しない。

依存ヘッダ: `i386/video.h` (VGA 関数)、`i386/keyboard.h` (キーボード関数)、`kernel/pool.h` (スタック割り当て)

## 関数リファレンス

### VGA 出力

#### sys_vga_write_at

```c
ER sys_vga_write_at(W apic, int row, int col, char *s, unsigned char attr);
```

**概要:** VGA テキストモードメモリの指定位置に文字列を書き込む。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `apic` | `W` | APIC ID (syscall 規約、未使用) |
| `row` | `int` | 行番号 (0〜24) |
| `col` | `int` | 列番号 (0〜79) |
| `s` | `char *` | 出力する文字列 |
| `attr` | `unsigned char` | VGA カラー属性 (例: 0x0F=白, 0x0A=緑) |

**戻り値:** `ER` -- 常に `E_OK`

**処理内容:** `vga_write_at(row, col, s, attr)` を呼び出す。VGA テキストモードメモリ (0xB8000) に直接書き込む。スクロールは行わない。

---

#### sys_vga_write_dec_at

```c
ER sys_vga_write_dec_at(W apic, int row, int col, unsigned long n,
        int width, unsigned char attr);
```

**概要:** VGA テキストモードメモリの指定位置に符号なし整数を 10 進数で書き込む。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `apic` | `W` | APIC ID (syscall 規約、未使用) |
| `row` | `int` | 行番号 (0〜24) |
| `col` | `int` | 列番号 (0〜79) |
| `n` | `unsigned long` | 出力する数値 |
| `width` | `int` | 表示桁数 (右詰め) |
| `attr` | `unsigned char` | VGA カラー属性 |

**戻り値:** `ER` -- 常に `E_OK`

**処理内容:** `vga_write_dec_at(row, col, n, width, attr)` を呼び出す。

---

#### sys_vga_clear

```c
ER sys_vga_clear(W apic);
```

**概要:** VGA 画面全体をクリアする。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `apic` | `W` | APIC ID (syscall 規約、未使用) |

**戻り値:** `ER` -- 常に `E_OK`

**処理内容:** `vga_clear()` を呼び出す。VGA テキストモードメモリ (80x25) を空白文字で埋める。

---

#### sys_vga_fill_at

```c
ER sys_vga_fill_at(W apic, int row, int col, int len, int ch,
        unsigned char attr);
```

**概要:** VGA テキストモードメモリの指定位置から、指定文字で指定長を塗りつぶす。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `apic` | `W` | APIC ID (syscall 規約、未使用) |
| `row` | `int` | 行番号 (0〜24) |
| `col` | `int` | 開始列番号 (0〜79) |
| `len` | `int` | 塗りつぶす文字数 |
| `ch` | `int` | 塗りつぶす文字コード |
| `attr` | `unsigned char` | VGA カラー属性 |

**戻り値:** `ER` -- 常に `E_OK`

**処理内容:** VGA テキストモードメモリ (0xB8000) に直接アクセスし、`row * 80 + col` の位置から `len` 文字分を `(attr << 8) | ch` で上書きする。行末 (列 79) を超えない範囲でクリップされる。

**注意点:** 他の VGA 関数と異なり、`video.h` の関数を呼ばず VGA メモリに直接書き込む。

---

### キーボード入力

#### sys_key_getc_sc

```c
ER sys_key_getc_sc(W apic);
```

**概要:** 廃止済み。キーボード入力は DTQ (`ipsnd_dtq`/`rcv_dtq`) に移行した。

**戻り値:** `ER` -- 常に `E_NOSPT`

---

#### sys_key_set_task

```c
ER sys_key_set_task(W apic, int task_id);
```

**概要:** キーボード ISR が `ipsnd_dtq` で送信する DTQ の ID を設定する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `apic` | `W` | APIC ID (syscall 規約、未使用) |
| `task_id` | `int` | DTQ ID (引数名は歴史的理由で `task_id` のまま) |

**戻り値:** `ER` -- 常に `E_OK`

**処理内容:** グローバル変数 `key_dtq_id` に `task_id` を設定する。キーボード ISR (`key_intr`) は `ipsnd_dtq(0, key_dtq_id, ch)` でこの DTQ に文字を送信する。

---

### メモリ管理

#### sys_stack_alloc_sc

```c
ER sys_stack_alloc_sc(W apic, int size);
```

**概要:** カーネルのスタックプールからメモリを割り当てる。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `apic` | `W` | APIC ID (syscall 規約、未使用) |
| `size` | `int` | 割り当てサイズ (バイト) |

**戻り値:** `ER` -- 割り当てたメモリのアドレス (`VP` を `ER` にキャスト)。失敗時は `NULL` (0)。

**処理内容:** `stack_alloc((SIZE)size)` を呼び出し、戻り値を `ER` にキャストして返す。`stack_alloc` はカーネルのスタックプール (`kernel/pool.c`) からメモリブロックを割り当てる。

**注意点:** 戻り値はポインタであり、通常の `ER` エラーコードではない。呼び出し元は戻り値を `VP` (void ポインタ) として解釈する必要がある。主にユーザータスクが `cre_tsk` で新タスクを生成する際のスタック領域確保に使用する。

## 補足

### syscall 呼び出し規約

全ハンドラの第 1 引数 `W apic` は、syscall ディスパッチャ (`c_intr_syscall` in `i386/syscall.c`) が割り込みフレームから APIC ID を取得して自動的に渡すもの。拡張 syscall ハンドラではいずれも使用しないが、ITRON syscall ハンドラとの統一的なディスパッチのために引数リストに含まれる。

### 戻り値の二重用途

`sys_key_getc_sc` と `sys_stack_alloc_sc` は、本来 `ER` (エラーコード) 型の戻り値を文字コードやポインタとして流用している。ユーザー側ラッパー (`lib/lib_exd.c`) が適切な型にキャストして返すため、利用者からは型安全に見える。
