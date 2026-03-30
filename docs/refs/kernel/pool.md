# pool.c / pool.h / poolP.h

対象ファイル: `kernel/pool.c`, `kernel/pool.h`, `kernel/poolP.h`

## 概要

カーネル内メモリプールマネージャ。3 つの独立したプールを管理する:

1. **スタックプール** (`stack_pool`) — タスクの Ring 3 スタック割当用 (PTE_USER ページ)
2. **ユーザーメモリプール** (`mem_pool`) — ユーザーがアクセスするメモリ割当用 (PTE_USER ページ)
3. **カーネルメモリプール** (`kmem_pool`) — カーネル内部バッファ割当用 (Supervisor ページ)

各プールは `allocation_t` 構造体の配列で管理され、ファーストフィット方式でメモリの確保・解放を行う。解放時には隣接する空きブロックの自動マージ (コアレッシング) を実行する。

SMP 環境でのスレッドセーフ性は、スタックプールとメモリプールそれぞれに独立したスピンロック (`pool_stack_lk`, `pool_mem_lk`) で保証される。カーネルメモリプール (`kmem_alloc`/`kmem_free`) は呼び出し元が `kernel_lk` (Big Kernel Lock) を保持しているため、独自のスピンロックを持たない。

## 定数・マクロ

| 定数 | 定義元 | 値 | 説明 |
|------|--------|----|------|
| MAX_STACK_POOL | poolP.h | 256 | スタックプールの allocation_t 配列の最大エントリ数 |
| MAX_MEM_POOL | poolP.h | 256 | メモリプールの allocation_t 配列の最大エントリ数 |
| MAX_KMEM_POOL | poolP.h | 256 | カーネルメモリプールの allocation_t 配列の最大エントリ数 |
| ALLOCATION_SIZE | include/stdio.h | 4096 | アロケーション単位サイズ (参考値、pool.c では直接使用しない) |
| MEM_FLAG_ALLOC | include/stdio.h | 0x01 | 割り当て済みフラグ |
| MEM_FLAG_FREE | include/stdio.h | 0x00 | 空きフラグ |
| MEM_ALIGN | include/stdio.h | 0xfffff000 | メモリアラインメントマスク (4KB 境界) |
| STACK_START | i386/addr.h | 0x700000 | スタックプール開始アドレス |
| STACK_END | i386/addr.h | 0x74ffff | スタックプール終了アドレス |
| MEM_START | i386/addr.h | 0x110000 | ユーザーメモリプール開始アドレス |
| MEM_END | i386/addr.h | 0x6fffff | メモリプール終了アドレス |

## 構造体・型

### allocation_t

```c
typedef struct allocation {
    B       flag;   /* MEM_FLAG_ALLOC (0x01) or MEM_FLAG_FREE (0x00) */
    VP      base;   /* ブロックの開始アドレス */
    UW      size;   /* ブロックのサイズ (バイト) */
} allocation_t;
```

**説明:** メモリブロックの管理情報。プール内の各ブロック (空きまたは割り当て済み) に 1 つ割り当てられる。プール配列は `size == 0` のエントリで終端される。

定義元: `include/stdio.h`

## グローバル変数

| 変数名 | 型 | スコープ | 説明 |
|--------|-----|---------|------|
| `stack_pool` | `allocation_t[MAX_STACK_POOL]` | static (poolP.h) | スタックプールの管理配列 (256 エントリ) |
| `mem_pool` | `allocation_t[MAX_MEM_POOL]` | static (poolP.h) | メモリプールの管理配列 (256 エントリ) |
| `kmem_pool` | `allocation_t[MAX_KMEM_POOL]` | static (poolP.h) | カーネルメモリプールの管理配列 (256 エントリ) |
| `pool_stack_lk` | `unsigned long` | static (pool.c) | スタックプール操作用スピンロック |
| `pool_mem_lk` | `unsigned long` | static (pool.c) | メモリプール操作用スピンロック |

## 関数リファレンス

### pool_init

```c
ER pool_init(allocation_t* a, VP start, VP end);
```

**概要:** プールを初期化する。指定されたメモリ範囲を 1 つの空きブロックとして登録する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `a` | `allocation_t*` | プール管理配列の先頭ポインタ |
| `start` | `VP` | プールの開始アドレス (4KB 境界に切り上げ) |
| `end` | `VP` | プールの終了アドレス (4KB 境界に切り下げ) |

**戻り値:** `E_OK` -- 常に成功

**処理内容:**
1. `start` を 4KB 境界に切り上げアラインメント: `(start + ~MEM_ALIGN) & MEM_ALIGN`
2. `end` を 4KB 境界に切り下げアラインメント: `end & MEM_ALIGN`
3. 配列の最初のエントリ `a[0]` を空きブロックとして設定 (base=start, size=end-start, flag=MEM_FLAG_FREE)
4. 配列の 2 番目のエントリ `a[1].size = 0` を終端マーカとして設定

**呼び出し元:** `stack_init()`, `mem_init()`, `kmem_init()`

**注意点:** アラインメント処理により、実際に使用可能なプールサイズは指定範囲よりも小さくなる場合がある。

---

### pool_ins

```c
void pool_ins(allocation_t* a);
```

**概要:** プール管理配列に新しいエントリを挿入するためのスペースを作る (配列要素を 1 つ後方にシフト)。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `a` | `allocation_t*` | 挿入位置のポインタ |

**戻り値:** なし (void)

**処理内容:**
1. 終端マーカ (size == 0) を見つけるまで走査
2. 終端マーカから `a` の位置まで、各エントリを 1 つ後方にコピー (後ろから前へ)
3. `a` の位置にスペースが空く

**呼び出し元:** `pool_alloc()`

**注意点:** 配列のオーバーフローチェックは行われない。MAX_STACK_POOL / MAX_MEM_POOL を超えるとバッファオーバーランが発生する。

---

### pool_rem

```c
void pool_rem(allocation_t* a);
```

**概要:** プール管理配列からエントリを削除する (配列要素を 1 つ前方にシフト)。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `a` | `allocation_t*` | 削除位置のポインタ |

**戻り値:** なし (void)

**処理内容:**
1. `a` の位置から、後続の各エントリを 1 つ前方にコピー
2. 終端マーカ (size == 0) もコピーされるため、配列長が 1 縮む

**呼び出し元:** `pool_free()`

**注意点:** なし

---

### pool_alloc

```c
VP pool_alloc(allocation_t* a, SIZE stksz, unsigned long max_a);
```

**概要:** プールからメモリブロックを確保する (ファーストフィット方式)。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `a` | `allocation_t*` | プール管理配列の先頭ポインタ |
| `stksz` | `SIZE` | 要求サイズ (バイト) |
| `max_a` | `unsigned long` | 管理配列の最大エントリ数 (オーバーフロー防止) |

**戻り値:**
- 確保したブロックのベースアドレス (`VP`) -- 成功
- `NULL` -- 確保可能な空きブロックが見つからなかった場合

**処理内容:**
1. 要求サイズを 4KB 境界にアラインメント: `(stksz + ~MEM_ALIGN) & MEM_ALIGN`
2. 配列を先頭から走査 (ファーストフィット)
3. 空きブロック (flag == MEM_FLAG_FREE) を発見した場合:
   - 要求サイズより大きい場合: ブロックを分割。`pool_ins()` で新エントリを挿入し、残余部分を新しい空きブロックとして登録
   - 要求サイズと完全一致の場合: そのまま割り当て
   - フラグを MEM_FLAG_ALLOC に変更し、ベースアドレスを返す
4. 走査が max_a に達するか終端に到達したら NULL を返す

**呼び出し元:** `stack_alloc()`, `mem_alloc()`, `kmem_alloc()`

**注意点:**
- 全ての確保は 4KB 境界にアラインされる。小さなサイズの要求でも最低 4KB が消費される。
- ファーストフィット方式のため、フラグメンテーションが発生する可能性がある。

---

### pool_free

```c
void pool_free(allocation_t* a, VP stack);
```

**概要:** 確保済みメモリブロックを解放し、隣接する空きブロックとマージする。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `a` | `allocation_t*` | プール管理配列の先頭ポインタ |
| `stack` | `VP` | 解放するブロックのベースアドレス |

**戻り値:** なし (void)

**処理内容:**
1. 配列を走査し、`base == stack` のエントリを検索
2. 見つかったらフラグを MEM_FLAG_FREE に変更
3. 前方マージ: 直前のエントリが空きなら、サイズを加算し `pool_rem()` で現エントリを削除
4. 後方マージ: 直後のエントリが空きなら、サイズを加算し `pool_rem()` で後続エントリを削除
5. 見つからなかった場合は何もしない (コメントに "panic" とあるが、実際にはパニック処理なし)

**呼び出し元:** `stack_free()`, `mem_free()`, `kmem_free()`

**注意点:**
- 前方マージのコードに `pool_rem(a + 1)` と書かれているが、正しくは `pool_rem(a + i)` であるべき可能性がある (i > 1 の場合のバグ)。

---

### pool_dump

```c
void pool_dump(allocation_t* a);
```

**概要:** プールの状態をデバッグ出力する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `a` | `allocation_t*` | プール管理配列の先頭ポインタ |

**戻り値:** なし (void)

**処理内容:**
1. 区切り線を printk で出力
2. 配列を走査し、各エントリの index, base, size, flag を printk で出力
3. 終端マーカ (size == 0) で終了

**呼び出し元:** `stack_dump()`, `mem_dump()`, `kmem_dump()`。デバッグ目的で呼び出される。

**注意点:** なし

---

### stack_init

```c
ER stack_init(VP start, VP end);
```

**概要:** スタックプールを初期化する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `start` | `VP` | スタックプール開始アドレス (STACK_START = 0x700000) |
| `end` | `VP` | スタックプール終了アドレス (STACK_END = 0x74ffff) |

**戻り値:** `E_OK` -- 成功

**処理内容:** `pool_init(stack_pool, start, end)` を呼び出す。

**呼び出し元:** `itron_init()` (kernel/kernel.c)

**注意点:** なし

---

### stack_alloc

```c
VP stack_alloc(SIZE stksz);
```

**概要:** スタックプールからメモリを確保する (スピンロック保護付き)。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `stksz` | `SIZE` | 要求するスタックサイズ (バイト) |

**戻り値:**
- 確保したブロックのベースアドレス -- 成功
- `NULL` -- 確保失敗

**処理内容:**
1. `pool_stack_lk` スピンロックを取得
2. `pool_alloc(stack_pool, stksz, MAX_STACK_POOL)` を呼び出し
3. スピンロック解放
4. 結果を返す

**呼び出し元:** `proc_create()` (i386/proc.c), `first_task()` (kernel/user.c) -- タスク作成時のスタック確保

**注意点:**
- 返されるアドレスはブロックの **ベース (先頭)** アドレス。x86 のスタックは下方向に伸びるため、スタックポインタとしては `base + stksz` (ブロックの末尾) を使用する必要がある。
- この処理は proc_create 内で適切に行われている (過去に base を直接使ってしまうバグがあった)。

---

### stack_free

```c
void stack_free(VP stk);
```

**概要:** スタックプールのメモリを解放する (スピンロック保護付き)。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `stk` | `VP` | 解放するブロックのベースアドレス |

**戻り値:** なし (void)

**処理内容:**
1. `pool_stack_lk` スピンロックを取得
2. `pool_free(stack_pool, stk)` を呼び出し
3. スピンロック解放

**呼び出し元:** タスク削除時

**注意点:** なし

---

### stack_dump

```c
void stack_dump(void);
```

**概要:** スタックプールの状態をデバッグ出力する。

**引数:** なし

**戻り値:** なし (void)

**処理内容:** `pool_dump(stack_pool)` を呼び出す。

**呼び出し元:** デバッグ目的

**注意点:** なし

---

### mem_init

```c
ER mem_init(VP start, VP end);
```

**概要:** 汎用メモリプールを初期化する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `start` | `VP` | メモリプール開始アドレス (MEM_START = 0x110000) |
| `end` | `VP` | メモリプール終了アドレス (MEM_END = 0x6fffff) |

**戻り値:** `E_OK` -- 成功

**処理内容:** `pool_init(mem_pool, start, end)` を呼び出す。

**呼び出し元:** `itron_init()` (kernel/kernel.c)

**注意点:** なし

---

### mem_alloc

```c
VP mem_alloc(SIZE stksz);
```

**概要:** ユーザーメモリプール (PTE_USER ページ) からメモリを確保する (スピンロック保護付き)。ユーザータスクがアクセスするメモリ (MPF ブロック、MPL プール領域) の確保に使用する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `stksz` | `SIZE` | 要求するサイズ (バイト) |

**戻り値:**
- 確保したブロックのベースアドレス -- 成功
- `NULL` -- 確保失敗

**処理内容:**
1. `pool_mem_lk` スピンロックを取得
2. `pool_alloc(mem_pool, stksz, MAX_MEM_POOL)` を呼び出し
3. スピンロック解放
4. 結果を返す

**呼び出し元:** `sys_cre_mpf` (ブロック領域), `sys_cre_mpl` (プール領域) — ユーザータスクに返すメモリの確保

**注意点:** なし

---

### mem_free

```c
void mem_free(VP stk);
```

**概要:** ユーザーメモリプール (PTE_USER ページ) のメモリを解放する (スピンロック保護付き)。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `stk` | `VP` | 解放するブロックのベースアドレス |

**戻り値:** なし (void)

**処理内容:**
1. `pool_mem_lk` スピンロックを取得
2. `pool_free(mem_pool, stk)` を呼び出し
3. スピンロック解放

**呼び出し元:** `sys_del_mpf` (ブロック領域), `sys_del_mpl` (プール領域)

**注意点:** なし

---

### mem_dump

```c
void mem_dump(void);
```

**概要:** 汎用メモリプールの状態をデバッグ出力する。

**引数:** なし

**戻り値:** なし (void)

**処理内容:** `pool_dump(mem_pool)` を呼び出す。

**呼び出し元:** デバッグ目的

**注意点:** なし

---

### kmem_init

```c
ER kmem_init(VP start, VP end);
```

**概要:** カーネル専用メモリプールを初期化する。このプールは Supervisor ページ (`_user_data_end` 〜 `MEM_START`) に配置され、Ring 3 のユーザータスクからアクセスできない。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `start` | `VP` | プール開始アドレス (`&_user_data_end`, 4KB 境界に切り上げ → 0x20000) |
| `end` | `VP` | プール終了アドレス (`MEM_START` = 0x110000) |

**戻り値:** `E_OK` -- 成功

**処理内容:** `pool_init(kmem_pool, start, end)` を呼び出す。

**呼び出し元:** `itron_init()` (kernel/kernel.c)

**注意点:** なし

---

### kmem_alloc

```c
VP kmem_alloc(SIZE size);
```

**概要:** カーネル専用メモリプールからメモリを確保する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `size` | `SIZE` | 要求するサイズ (バイト) |

**戻り値:**
- 確保したブロックのベースアドレス -- 成功
- `NULL` -- 確保失敗

**処理内容:** `pool_alloc(kmem_pool, size, MAX_KMEM_POOL)` を呼び出す。

**呼び出し元:** `sys_cre_dtq`, `sys_cre_mbf`, `sys_cre_mbx`, `sys_cre_mpf`, `sys_cre_mpl` — カーネル内部バッファの確保

**注意点:** 呼び出し元が `kernel_lk` を保持していることを前提とする。独自のスピンロックは持たない。

---

### kmem_free

```c
void kmem_free(VP ptr);
```

**概要:** カーネル専用メモリプールのメモリを解放する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `ptr` | `VP` | 解放するブロックのベースアドレス |

**戻り値:** なし (void)

**処理内容:** `pool_free(kmem_pool, ptr)` を呼び出す。

**呼び出し元:** `sys_del_dtq`, `sys_del_mbf`, `sys_del_mbx`, `sys_del_mpf`, `sys_del_mpl` — カーネル内部バッファの解放

**注意点:** 呼び出し元が `kernel_lk` を保持していることを前提とする。

---

### kmem_dump

```c
void kmem_dump(void);
```

**概要:** カーネル専用メモリプールの状態をデバッグ出力する。

**引数:** なし

**戻り値:** なし (void)

**処理内容:** `pool_dump(kmem_pool)` を呼び出す。

**呼び出し元:** デバッグ目的

**注意点:** なし

## 補足

### メモリプールのアーキテクチャ

```
allocation_t 配列:
+-------+--------+------+
| flag  | base   | size |  [0] 空きまたは使用中
+-------+--------+------+
| flag  | base   | size |  [1] 空きまたは使用中
+-------+--------+------+
| ...   | ...    | ...  |
+-------+--------+------+
| -     | -      | 0    |  [n] 終端マーカ (size=0)
+-------+--------+------+
```

### メモリレイアウト

| プール | 開始 | 終了 | サイズ | ページ属性 | 用途 |
|--------|------|------|--------|-----------|------|
| カーネルメモリプール | 0x20000 | 0x110000 | 約 960KB | Supervisor | カーネル内部バッファ (DTQ, MBF, MBX, 管理メタデータ) |
| ユーザーメモリプール | 0x110000 | 0x6fffff | 約 6MB | User (PTE_USER) | ユーザーアクセス可能メモリ (MPF ブロック, MPL プール) |
| スタックプール | 0x700000 | 0x74ffff | 約 320KB | User (PTE_USER) | タスクスタックの確保 |

### SMP 安全性

`stack_alloc`/`stack_free` と `mem_alloc`/`mem_free` はそれぞれ独立したスピンロック (`pool_stack_lk`, `pool_mem_lk`) で保護されており、異なるプールへの同時アクセスは許可される。同一プールへの同時アクセスはスピンロックにより直列化される。

`kmem_alloc`/`kmem_free` は独自のスピンロックを持たない。呼び出し元の syscall ハンドラが `kernel_lk` (Big Kernel Lock) を保持しているため、暗黙的に排他制御される。
