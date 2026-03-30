# sys_mpl.c / sys_mpl.h

対象ファイル: `kernel/sys_mpl.c`, `kernel/sys_mpl.h`

## 概要

可変長メモリプール (variable-size memory pool) のシステムコール実装。Micro ITRON v4.0.0 仕様に基づく、任意サイズのメモリブロック割り当て機構を提供する。

固定長メモリプール (`sys_mpf`) と異なり、可変長メモリプールでは要求ごとに異なるサイズのメモリブロックを割り当てることができる。内部管理には `allocation_t` 配列 (最大 `MAX_MPL_POOL = 64` エントリ) を使用し、プールアロケータで空き領域の管理を行う。

### データ構造

```c
#define MAX_MPL_POOL  64

typedef struct t_mpl {
    T_LINK         wlink;      /* 待ちタスクキュー */
    ATR            mplatr;     /* プール属性 (TA_TFIFO/TA_TPRI) */
    SIZE           mplsz;      /* プール全体のサイズ (バイト) */
    VP             mpl;        /* メモリプール先頭ポインタ */
    allocation_t*  pool;       /* プールアロケータ管理配列 */
    STAT           mpl_alloc;  /* カーネルが mpl を動的確保したか */
    STAT           act;        /* 生成済みフラグ */
} T_MPL;
```

### 定数

- `MAX_MPLID`: 16 (`include/config.h`)
- `MAX_MPL_POOL`: 64 (`kernel/types.h`) -- プールアロケータの最大エントリ数

## 関数リファレンス

### mpl_init

```c
void mpl_init(void)
```

**概要:** 全可変長メモリプールを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. ID 1 から `MAX_MPLID - 1` まで全エントリをループ (ループ条件は `i < MAX_MPLID`)
2. 各エントリの待ちキュー (`wlink`) を空の双方向リンクリストに初期化
3. `act = 0` に設定

---

### sys_cre_mpl

```c
ER sys_cre_mpl(W apic, ID mplid, T_CMPL* pk_cmpl)
```

**概要:** 指定 ID で可変長メモリプールを生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mplid | ID | メモリプール ID (1 ~ MAX_MPLID) |
| pk_cmpl | T_CMPL* | 生成情報パケット (mplatr, mplsz, mpl) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_OBJ | 既に生成済み |

**処理内容:**
1. ID 範囲・存在チェック
2. `pk_cmpl` から `mplatr`, `mplsz`, `mpl` をコピー
3. `mpl` が NULL の場合、`mem_alloc` で `mplsz` バイトを動的確保 (User ページ、`mpl_alloc = 1`)。`get_mpl` がユーザータスクにポインタを返すため PTE_USER 領域に配置
4. プールアロケータ管理配列を `kmem_alloc` で確保 (`MAX_MPL_POOL * sizeof(allocation_t)`)。管理メタデータはカーネル内部のみ使用するため Supervisor ページに配置
5. `pool_init` でプールアロケータを初期化 (先頭アドレスと末尾アドレス `mpl + mplsz` を指定)
6. `act = 1` で生成完了

---

### sys_acre_mpl

```c
ER sys_acre_mpl(W apic, T_CMPL* pk_cmpl)
```

**概要:** 可変長メモリプール ID を自動割り当てで生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| pk_cmpl | T_CMPL* | 生成情報パケット |

**戻り値:**
| 値 | 説明 |
|------|------|
| 正値 | 自動割り当てされたメモリプール ID |
| E_NOID | 空き ID なし |
| E_ID / E_OBJ | `sys_cre_mpl` 委譲時のエラー |

**処理内容:**
1. ID 1 から `MAX_MPLID` まで順にスキャンし、`act == 0` の空きエントリを探す
2. 見つかったら `sys_cre_mpl(apic, i, pk_cmpl)` に委譲して生成
3. 生成成功なら割り当てた ID を返す。空きがなければ `E_NOID` を返す

---

### sys_del_mpl

```c
ER sys_del_mpl(W apic, ID mplid)
```

**概要:** 可変長メモリプールを削除する。待ちタスクには E_DLT を通知する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mplid | ID | メモリプール ID |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. 待ちキューの全タスクに E_DLT を通知し、`TTS_RDY` に変更してスケジューラキューに挿入
3. カーネルが動的確保した `mpl` を `mem_free` で解放 (User ページ)
4. プールアロケータ管理配列 (`pool`) を `kmem_free` で解放 (Supervisor ページ)
5. 待ちキューを空リストにリセット、`act = 0`

---

### sys_get_mpl

```c
ER sys_get_mpl(W apic, ID mplid, UINT blksz, VP* p_blk)
```

**概要:** 可変長メモリブロックを取得する (永久待ち)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mplid | ID | メモリプール ID |
| blksz | UINT | 要求ブロックサイズ (バイト) |
| p_blk | VP* | 取得ブロックポインタ格納先 |

**戻り値:** `sys_tget_mpl(apic, mplid, blksz, p_blk, TMO_FEVR)` の戻り値

**処理内容:** `sys_tget_mpl` に `TMO_FEVR` を指定して委譲する。

---

### sys_pget_mpl

```c
ER sys_pget_mpl(W apic, ID mplid, UINT blksz, VP* p_blk)
```

**概要:** 可変長メモリブロックをポーリング取得する (待ちなし)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mplid | ID | メモリプール ID |
| blksz | UINT | 要求ブロックサイズ |
| p_blk | VP* | 取得ブロックポインタ格納先 |

**戻り値:** `sys_tget_mpl(apic, mplid, blksz, p_blk, TMO_POL)` の戻り値

**処理内容:** `sys_tget_mpl` に `TMO_POL` (ポーリング) を指定して委譲する。

---

### sys_tget_mpl

```c
ER sys_tget_mpl(W apic, ID mplid, UINT blksz, VP* p_blk, TMO tmout)
```

**概要:** 可変長メモリブロックをタイムアウト付きで取得する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mplid | ID | メモリプール ID |
| blksz | UINT | 要求ブロックサイズ |
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
2. `pool_alloc` で要求サイズのブロック割り当てを試行 (最大エントリ数 `MAX_MPL_POOL` を指定)
3. 割り当て成功 (NULL でない) なら `*p_blk` に設定して E_OK を返す
4. 割り当て失敗の場合:
   - `TMO_POL` なら E_TMOUT を返す
   - 待ちキューに挿入 (`TA_TFIFO` で FIFO 順、それ以外は優先度順)
   - タスクの `p_blk` と `blksz` を保存 (解放時に再割り当てするため)
   - `sys_slp_tsk` / `sys_tslp_tsk` でブロック

---

### sys_rel_mpl

```c
ER sys_rel_mpl(W apic, ID mplid, VP blk)
```

**概要:** 可変長メモリブロックを返却する。待ちタスクの要求を満たせるなら割り当てる。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mplid | ID | メモリプール ID |
| blk | VP | 返却するブロックポインタ |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. `pool_free` でブロックをプールに返却
3. 待ちキューの先頭から順にスキャン:
   - `pool_alloc` で待ちタスクの要求サイズのブロック割り当てを試行
   - 割り当て成功なら、ブロックポインタを待ちタスクに渡し (`*(t->p_blk)`)、`TTS_RDY` に変更してスケジューラキューに挿入
   - 割り当て失敗なら、残りのタスクも同様に失敗すると判断してスキャン終了

---

### sys_ref_mpl

```c
ER sys_ref_mpl(W apic, ID mplid, T_RMPL* pk_rmpl)
```

**概要:** 可変長メモリプールの状態を参照する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mplid | ID | メモリプール ID |
| pk_rmpl | T_RMPL* | 参照情報格納先 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. 待ちタスクがいれば `wtskid` にその ID を設定、いなければ `TSK_NONE`
3. `fmplsz` (空き合計サイズ) と `fblksz` (最大空きブロックサイズ) は常に 0 を返す

**注意:** `fmplsz` と `fblksz` の実装が未完成 (コメント `/* m(..)m */` で表記)。

## 補足

- 固定長メモリプール (`sys_mpf`) との違い: 可変長プールは `pool_alloc` に任意のサイズを指定でき、異なるサイズの割り当て要求に対応できる。ただし、管理エントリ数に上限 (`MAX_MPL_POOL = 64`) がある。
- `sys_rel_mpl` では `pool_free` でブロックをプールに返却してから、待ちタスクに再割り当てを試みる。固定長プールの `sys_rel_mpf` がブロックを直接渡す方式と異なり、可変長プールではサイズが異なるため `pool_alloc` を経由する必要がある。
- `sys_pget_mpl` は `sys_tget_mpl(TMO_POL)` に委譲する。`sys_acre_mpl` は空き ID を自動検索して `sys_cre_mpl` に委譲する。
- `sys_ref_mpl` の空きサイズ関連フィールド (`fmplsz`, `fblksz`) は未実装。
