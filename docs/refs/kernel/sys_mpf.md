# sys_mpf.c / sys_mpf.h

対象ファイル: `kernel/sys_mpf.c`, `kernel/sys_mpf.h`

## 概要

固定長メモリプール (fixed-size memory pool) のシステムコール実装。Micro ITRON v4.0.0 仕様に基づく、固定サイズのメモリブロック割り当て機構を提供する。

固定長メモリプールは、同一サイズのメモリブロックを効率的に管理するための機構である。プール生成時にブロックサイズとブロック数を指定し、プールアロケータ (`pool.h` の `allocation_t`) でブロックの割り当て・解放を管理する。空きブロックが無い場合、タスクは待ちキューに入ってブロックが解放されるまで待機できる。

### データ構造

```c
typedef struct t_mpf {
    T_LINK         wlink;      /* 待ちタスクキュー */
    ATR            mpfatr;     /* プール属性 (TA_TFIFO/TA_TPRI) */
    UINT           blkcnt;     /* ブロック数 */
    UINT           blksz;      /* ブロックサイズ (バイト) */
    VP             mpf;        /* メモリプール先頭ポインタ */
    allocation_t*  pool;       /* プールアロケータ管理配列 */
    STAT           mpf_alloc;  /* カーネルが mpf を動的確保したか */
    STAT           act;        /* 生成済みフラグ */
} T_MPF;
```

### 定数

- `MAX_MPFID`: 16 (`include/config.h`)

## 関数リファレンス

### mpf_init

```c
void mpf_init(void)
```

**概要:** 全固定長メモリプールを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. ID 0 から `MAX_MPFID` まで全エントリをループ
2. 各エントリの待ちキュー (`wlink`) を空の双方向リンクリストに初期化
3. `act = 0` に設定

---

### sys_cre_mpf

```c
ER sys_cre_mpf(W apic, ID mpfid, T_CMPF* pk_cmpf)
```

**概要:** 指定 ID で固定長メモリプールを生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mpfid | ID | メモリプール ID (1 ~ MAX_MPFID) |
| pk_cmpf | T_CMPF* | 生成情報パケット (mpfatr, blkcnt, blksz, mpf) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_OBJ | 既に生成済み |

**処理内容:**
1. ID 範囲・存在チェック
2. `pk_cmpf` から `mpfatr`, `blkcnt`, `blksz`, `mpf` をコピー
3. `mpf` が NULL の場合、`mem_alloc` で `blkcnt * blksz` バイトを動的確保 (User ページ、`mpf_alloc = 1`)。`get_mpf` がユーザータスクにポインタを返すため PTE_USER 領域に配置
4. プールアロケータ管理配列を `kmem_alloc` で確保 (`sizeof(allocation_t) * blkcnt`)。管理メタデータはカーネル内部のみ使用するため Supervisor ページに配置
5. `pool_init` でプールアロケータを初期化 (先頭アドレスと全体サイズを指定)
6. `act = 1` で生成完了

---

### sys_acre_mpf

```c
ER_ID sys_acre_mpf(W apic, T_CMPF* pk_cmpf)
```

**概要:** 固定長メモリプール ID を自動割り当てで生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| pk_cmpf | T_CMPF* | 生成情報パケット |

**戻り値:** 未定義 (未実装)

**処理内容:** 未実装。関数本体が空。

---

### sys_del_mpf

```c
ER sys_del_mpf(W apic, ID mpfid)
```

**概要:** 固定長メモリプールを削除する。待ちタスクには E_DLT を通知する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mpfid | ID | メモリプール ID |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. 待ちキューの全タスクに E_DLT を通知し、`TTS_RDY` に変更してスケジューラキューに挿入
3. カーネルが動的確保した `mpf` を `mem_free` で解放 (User ページ)
4. プールアロケータ管理配列 (`pool`) を `kmem_free` で解放 (Supervisor ページ)
5. 待ちキューを空リストにリセット、`act = 0`

---

### sys_get_mpf

```c
ER sys_get_mpf(W apic, ID mpfid, VP* p_blk)
```

**概要:** 固定長メモリブロックを取得する (永久待ち)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mpfid | ID | メモリプール ID |
| p_blk | VP* | 取得ブロックポインタ格納先 |

**戻り値:** `sys_tget_mpf(apic, mpfid, p_blk, TMO_FEVR)` の戻り値

**処理内容:** `sys_tget_mpf` に `TMO_FEVR` を指定して委譲する。

---

### sys_pget_mpf

```c
ER sys_pget_mpf(W apic, ID mpfid, VP* p_blk)
```

**概要:** 固定長メモリブロックをポーリング取得する (待ちなし)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mpfid | ID | メモリプール ID |
| p_blk | VP* | 取得ブロックポインタ格納先 |

**戻り値:** 未定義 (未実装)

**処理内容:** 未実装。関数本体が空。

---

### sys_tget_mpf

```c
ER sys_tget_mpf(W apic, ID mpfid, VP* p_blk, TMO tmout)
```

**概要:** 固定長メモリブロックをタイムアウト付きで取得する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mpfid | ID | メモリプール ID |
| p_blk | VP* | 取得ブロックポインタ格納先 |
| tmout | TMO | タイムアウト |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 (ブロック取得成功) |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |
| E_TMOUT | タイムアウト |

**処理内容:**
1. ID・存在チェック
2. `pool_alloc` でブロック割り当てを試行
3. 割り当て成功 (NULL でない) なら `*p_blk` に設定して E_OK を返す
4. 割り当て失敗の場合:
   - `TMO_POL` なら E_TMOUT を返す
   - タスクの `p_blk` を保存 (解放時にブロックを渡すため)
   - 待ちキューに挿入 (`TA_TFIFO` で FIFO 順、それ以外は優先度順)
   - `sys_slp_tsk` / `sys_tslp_tsk` でブロック

---

### sys_rel_mpf

```c
ER sys_rel_mpf(W apic, ID mpfid, VP blk)
```

**概要:** 固定長メモリブロックを返却する。待ちタスクがあればブロックを直接渡す。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mpfid | ID | メモリプール ID |
| blk | VP | 返却するブロックポインタ |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. 待ちタスクが存在する場合:
   - 先頭待ちタスクを取り出し
   - 返却されたブロックを直接待ちタスクに渡す (`*(t->p_blk) = blk`)
   - タスクを `TTS_RDY` に変更してスケジューラキューに挿入
   - `pool_free` を呼ばずに E_OK を返す (ブロックは待ちタスクに移譲)
3. 待ちタスクが無い場合:
   - `pool_free` でブロックをプールに返却
   - E_OK を返す

---

### sys_ref_mpf

```c
ER sys_ref_mpf(W apic, ID mpfid, T_RMPF* pk_rmpf)
```

**概要:** 固定長メモリプールの状態を参照する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mpfid | ID | メモリプール ID |
| pk_rmpf | T_RMPF* | 参照情報格納先 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. 待ちタスクがいれば `wtskid` にその ID を設定、いなければ `TSK_NONE`
3. `fblkcnt` (空きブロック数) は常に 0 を返す

**注意:** `fblkcnt` の実装が未完成 (コメントで `pool_free_size()` の実装が必要と記載されている)。

## 補足

- プール管理は `pool.h` / `pool.c` のアロケータに委譲される。`pool_init`, `pool_alloc`, `pool_free` 関数でブロックの管理を行う。
- `sys_rel_mpf` では、待ちタスクが存在する場合にプールを経由せず直接ブロックを渡す最適化が行われている。これにより不要な `pool_free` + `pool_alloc` の往復を回避する。
- `sys_pget_mpf`, `sys_acre_mpf` は未実装。
- `sys_ref_mpf` の `fblkcnt` は未実装 (常に 0)。正確な空きブロック数を返すには `pool_free_size()` 関数の実装が必要。
