# lib_exd.c

対象ファイル: `lib/lib_exd.c`

## 概要

非 ITRON 拡張システムコールのユーザライブラリラッパーを提供するファイル。VGA テキスト出力、キーボード入力、スタック割り当てなど、ITRON v4.0.0 仕様には含まれないハードウェア依存機能をユーザータスクから呼び出すための API を定義する。

各関数は `syscall()` を通じて対応する `TFN_EXD_*` ファンクションコードでカーネルのハンドラ (`kernel/sys_exd.c`) を呼び出す。`syscall()` の第 1 引数にはファンクションコードの符号反転値を渡す (ITRON ラッパーと同じ規約)。

## 関数リファレンス

### VGA 出力

#### print_at

```c
void print_at(int row, int col, char *s, unsigned char attr);
```

**概要:** VGA 画面の指定位置に文字列を表示する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `row` | `int` | 行番号 (0〜24) |
| `col` | `int` | 列番号 (0〜79) |
| `s` | `char *` | 出力する文字列 |
| `attr` | `unsigned char` | VGA カラー属性 (例: 0x0F=白, 0x0A=緑) |

**戻り値:** なし (void)

**処理内容:** `syscall(-TFN_EXD_VGA_WRITE, row, col, s, attr)` を呼び出す。カーネル側で `sys_vga_write_at()` → `vga_write_at()` が実行される。

**注意点:** Ring 3 (ユーザーモード) から安全に VGA 出力できる。`printk()` と異なりスクロールは行わない。

---

#### print_dec_at

```c
void print_dec_at(int row, int col, unsigned long n, int width,
        unsigned char attr);
```

**概要:** VGA 画面の指定位置に符号なし整数を 10 進数で表示する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `row` | `int` | 行番号 (0〜24) |
| `col` | `int` | 列番号 (0〜79) |
| `n` | `unsigned long` | 出力する数値 |
| `width` | `int` | 表示桁数 (右詰め) |
| `attr` | `unsigned char` | VGA カラー属性 |

**戻り値:** なし (void)

**処理内容:** `syscall(-TFN_EXD_VGA_DEC, row, col, n, width, attr)` を呼び出す。カーネル側で `sys_vga_write_dec_at()` → `vga_write_dec_at()` が実行される。

---

#### clear_screen

```c
void clear_screen(void);
```

**概要:** VGA 画面全体をクリアする。

**引数:** なし

**戻り値:** なし (void)

**処理内容:** `syscall(-TFN_EXD_VGA_CLEAR)` を呼び出す。カーネル側で `sys_vga_clear()` → `vga_clear()` が実行される。

---

#### fill_at

```c
void fill_at(int row, int col, int len, int ch, unsigned char attr);
```

**概要:** VGA 画面の指定位置から、指定文字で指定長を塗りつぶす。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `row` | `int` | 行番号 (0〜24) |
| `col` | `int` | 開始列番号 (0〜79) |
| `len` | `int` | 塗りつぶす文字数 |
| `ch` | `int` | 塗りつぶす文字コード |
| `attr` | `unsigned char` | VGA カラー属性 |

**戻り値:** なし (void)

**処理内容:** `syscall(-TFN_EXD_VGA_FILL, row, col, len, ch, attr)` を呼び出す。カーネル側で `sys_vga_fill_at()` が VGA メモリに直接書き込む。行末 (列 79) でクリップされる。

---

### キーボード入力

#### key_read (廃止済み)

```c
int key_read(void);
```

**概要:** 廃止済み。キーボード入力は DTQ (`rcv_dtq`) に移行した。呼び出すと `E_NOSPT` が返る。

---

#### set_key_task

```c
void set_key_task(int id);
```

**概要:** キーボード ISR が `ipsnd_dtq` で送信する DTQ の ID を設定する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `id` | `int` | キーボード入力の送信先 DTQ ID |

**戻り値:** なし (void)

**処理内容:** `syscall(-TFN_EXD_KEY_SETTASK, id)` を呼び出す。カーネル側で `sys_key_set_task()` がグローバル変数 `key_dtq_id` を設定する。

**使用例:** `kbd_task` (Task 4) が初期化時に `set_key_task(2)` を呼び出し、キーボード ISR が DTQ 2 に文字を送信するように設定する。

---

### メモリ管理

#### tsk_stack_alloc

```c
VP tsk_stack_alloc(int size);
```

**概要:** カーネルのスタックプールからメモリを割り当てる。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `size` | `int` | 割り当てサイズ (バイト) |

**戻り値:** `VP` -- 割り当てたメモリブロックの先頭アドレス。失敗時は `NULL`。

**処理内容:** `syscall(-TFN_EXD_STACK_ALLOC, size)` を呼び出す。カーネル側で `sys_stack_alloc_sc()` → `stack_alloc()` が実行される。戻り値を `VP` (void ポインタ) にキャストして返す。

**使用例:** ユーザータスクが `cre_tsk` で新タスクを生成する際、`T_CTSK.stk` にこの関数で確保した領域を指定する。

## 補足

- 全 7 関数。ITRON 標準 API (`lib/lib_tsk.c` 等) と同じ `syscall()` トラップ機構を使用する。
- VGA 出力関数 (`print_at`, `print_dec_at`, `clear_screen`, `fill_at`) は Ring 3 から安全に画面操作できる手段を提供する。`printk()` は `outb` (I/O ポートアクセス) を含む `video_scroll()` を内部で呼ぶため、Ring 3 から呼び出すと #GP 例外が発生する。
- `key_read` は廃止済み。キーボード入力は DTQ 経由 (`ipsnd_dtq`/`rcv_dtq`) に移行した。`set_key_task` は DTQ ID の登録に使う。
