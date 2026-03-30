# sys_mbx.c / sys_mbx.h

対象ファイル: `kernel/sys_mbx.c`, `kernel/sys_mbx.h`

## 概要

メールボックス (mailbox) のシステムコール実装。Micro ITRON v4.0.0 仕様に基づく、優先度付きメッセージキュー機構を提供する。

メールボックスは、タスク間でメッセージポインタを受け渡すための同期・通信オブジェクトである。メッセージは優先度別のキュー (`mprihd` 配列) に連結リストとして格納され、受信時には最も高い優先度のキューから先頭メッセージが取り出される。

### データ構造

```c
typedef struct t_mbx {
    T_LINK    wlink;       /* 受信待ちタスクキュー */
    ATR       mbxatr;      /* メールボックス属性 (TA_MFIFO/TA_MPRI, TA_TFIFO/TA_TPRI) */
    PRI       maxmpri;     /* メッセージ優先度の最大値 */
    VP        mprihd;      /* 優先度別メッセージヘッダ配列 */
    STAT      mbx_alloc;   /* mprihd をカーネルが動的確保したか */
    STAT      act;         /* 生成済みフラグ */
} T_MBX;
```

メッセージ格納方式: `mprihd[pri]` は各優先度のメッセージ連結リストの先頭ポインタ。メッセージは `T_MSG` (実質ポインタ) で連結され、末尾は NULL で終端する。`TA_MFIFO` の場合は優先度 1 に一律格納、`TA_MPRI` の場合は `T_MSG_PRI` 構造体の `msgpri` フィールドで振り分ける。

### 定数

- `MAX_MBXID`: 16 (`include/config.h`)

## 関数リファレンス

### mbx_init

```c
void mbx_init(void)
```

**概要:** 全メールボックスを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. ID 1 から `MAX_MBXID` まで全エントリをループ
2. 各エントリの待ちキュー (`wlink`) を空の双方向リンクリスト (自己参照) に初期化
3. `act` フラグを 0 (未生成) に設定

---

### sys_cre_mbx

```c
ER sys_cre_mbx(W apic, ID mbxid, T_CMBX* pk_cmbx)
```

**概要:** 指定 ID でメールボックスを生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID (呼び出し元 CPU) |
| mbxid | ID | メールボックス ID (1 ~ MAX_MBXID) |
| pk_cmbx | T_CMBX* | 生成情報パケット (mbxatr, maxmpri, mprihd) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_OBJ | 既に生成済み |

**処理内容:**
1. ID 範囲チェック (1 ~ MAX_MBXID)
2. 既存オブジェクトチェック
3. `pk_cmbx` から `mbxatr`, `maxmpri`, `mprihd` をコピー
4. `mprihd` が NULL の場合、カーネルが `kmem_alloc` で動的確保 (Supervisor ページ、`mbx_alloc = 1`)
5. 優先度ヘッダ配列 `mprihd[1..maxmpri]` を 0 (NULL) で初期化
6. `act = 1` で生成完了

---

### sys_acre_mbx

```c
ER_ID sys_acre_mbx(W apic, T_CMBX* pk_cmbx)
```

**概要:** メールボックス ID を自動割り当てで生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| pk_cmbx | T_CMBX* | 生成情報パケット |

**戻り値:**
| 値 | 説明 |
|------|------|
| 正値 | 自動割り当てされたメールボックス ID |
| E_NOID | 空き ID なし |
| E_ID / E_OBJ | `sys_cre_mbx` 委譲時のエラー |

**処理内容:**
1. ID 1 から `MAX_MBXID` まで順にスキャンし、`act == 0` の空きエントリを探す
2. 見つかったら `sys_cre_mbx(apic, i, pk_cmbx)` に委譲して生成
3. 生成成功なら割り当てた ID を返す。空きがなければ `E_NOID` を返す

---

### sys_del_mbx

```c
ER sys_del_mbx(W apic, ID mbxid)
```

**概要:** メールボックスを削除する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbxid | ID | メールボックス ID |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. `mbx_alloc` が 1 の場合、`kmem_free` で `mprihd` を解放
3. 待ちキューを空リストにリセット
4. `act = 0` で削除完了

**注意:** 待ちキューに残っているタスクには `proc_set_return_value(t->proc, E_DLT)` で E_DLT が通知され、`TTS_RDY` に変更されてスケジューラキューに挿入される。

---

### sys_snd_mbx

```c
ER sys_snd_mbx(W apic, ID mbxid, T_MSG* pk_msg)
```

**概要:** メールボックスにメッセージを送信する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbxid | ID | メールボックス ID |
| pk_msg | T_MSG* | メッセージポインタ |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. 受信待ちタスクが存在する場合:
   - 先頭の待ちタスクを取り出し (`wlink2tsk`)
   - メッセージポインタを直接配送 (`t->ppk_msg` に書き込み)
   - タスク状態を `TTS_RDY` に変更し、スケジューラキューに挿入
   - 待ちキューから除去
3. 受信待ちタスクがいない場合:
   - `TA_MFIFO`: 優先度 1 のキューに `mbx_ins` で格納
   - `TA_MPRI`: メッセージの `msgpri` フィールドの優先度でキューに格納

---

### sys_rcv_mbx

```c
ER sys_rcv_mbx(W apic, ID mbxid, T_MSG** ppk_msg)
```

**概要:** メールボックスからメッセージを受信する (永久待ち)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbxid | ID | メールボックス ID |
| ppk_msg | T_MSG** | 受信メッセージポインタ格納先 |

**戻り値:** `sys_trcv_mbx(apic, mbxid, ppk_msg, TMO_FEVR)` の戻り値

**処理内容:** `sys_trcv_mbx` に `TMO_FEVR` (永久待ち) を指定して委譲する。

---

### sys_prcv_mbx

```c
ER sys_prcv_mbx(W apic, ID mbxid, T_MSG** ppk_msg)
```

**概要:** メールボックスからメッセージをポーリング受信する (待ちなし)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbxid | ID | メールボックス ID |
| ppk_msg | T_MSG** | 受信メッセージポインタ格納先 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 (メッセージ受信成功) |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |
| E_TMOUT | メッセージなし |

**処理内容:**
1. ID・存在チェック
2. `mprihd[1..maxmpri-1]` を優先度順にスキャンし、最初に見つかったメッセージを取り出す
3. メッセージが見つかれば `mbx_rem` で除去し、`*ppk_msg` に設定して E_OK を返す
4. メッセージが無ければ E_TMOUT を返す

---

### sys_trcv_mbx

```c
ER sys_trcv_mbx(W apic, ID mbxid, T_MSG** ppk_msg, TMO tmout)
```

**概要:** メールボックスからメッセージをタイムアウト付きで受信する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbxid | ID | メールボックス ID |
| ppk_msg | T_MSG** | 受信メッセージポインタ格納先 |
| tmout | TMO | タイムアウト (TMO_POL/TMO_FEVR/正値) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |
| E_TMOUT | タイムアウト (TMO_POL 時) |

**処理内容:**
1. ID・存在チェック
2. `mprihd[1..maxmpri-1]` を優先度順にスキャンし、メッセージがあれば即時取得して返す
3. メッセージが無い場合:
   - `TMO_POL`: 即座に E_TMOUT を返す
   - `tsk[].ppk_msg` に `ppk_msg` を保存 (`snd_mbx` がメッセージを配送する先)
   - `TA_TFIFO`: FIFO 順で待ちキューに挿入
   - `TA_TPRI`: タスク優先度順で待ちキューに挿入
   - `TMO_FEVR`: `sys_slp_tsk` でブロック (永久待ち)
   - 正値: `sys_tslp_tsk` でタイムアウト付きブロック

---

### sys_ref_mbx

```c
ER sys_ref_mbx(W apic, ID mbxid, T_RMBX* pk_rmbx)
```

**概要:** メールボックスの状態を参照する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbxid | ID | メールボックス ID |
| pk_rmbx | T_RMBX* | 参照情報格納先 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. 待ちタスクが存在すれば `wtskid` にその ID を設定、なければ `TSK_NONE`
3. 最高優先度のメッセージを検索し、`pk_msg` に設定 (なければ NULL)

---

### mbx_ins

```c
void mbx_ins(ID mbxid, PRI pri, T_MSG* pk_msg)
```

**概要:** 指定優先度のメッセージキューにメッセージを追加する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| mbxid | ID | メールボックス ID |
| pri | PRI | メッセージ優先度 |
| pk_msg | T_MSG* | メッセージポインタ |

**戻り値:** なし

**処理内容:**
1. 指定優先度のキューが空の場合: `mprihd[pri]` にメッセージを設定し、メッセージの次ポインタを NULL に設定
2. キューが空でない場合: 連結リストの末尾を辿り、末尾の次ポインタにメッセージを追加。メッセージの次ポインタを NULL に設定

メッセージ連結リストは `T_MSG` (実質 `void*`) をポインタとして使用し、メッセージ本体の先頭ワードが次メッセージへのポインタとなる。

---

### mbx_rem

```c
void mbx_rem(ID mbxid, PRI pri)
```

**概要:** 指定優先度のメッセージキューから先頭メッセージを除去する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| mbxid | ID | メールボックス ID |
| pri | PRI | メッセージ優先度 |

**戻り値:** なし

**処理内容:**
1. 指定優先度のキューが空なら何もしない
2. 先頭メッセージの次ポインタを `mprihd[pri]` に設定 (先頭を取り除く)

## 補足

- メッセージの実体はカーネルがコピーしない。送信側が確保したメモリ領域のポインタをそのまま受信側に渡す方式。メッセージ先頭の `sizeof(T_MSG)` バイト (ポインタ1個分) はカーネルが連結リストのリンクに使用するため、ユーザーデータはその直後に配置する必要がある。
- `TA_MPRI` 属性の場合、メッセージは `T_MSG_PRI` 構造体として扱われ、`msgpri` フィールドで優先度が決まる。
- `sys_prcv_mbx` および `sys_trcv_mbx` のスキャン範囲は `i < maxmpri` (maxmpri 未満) となっており、`maxmpri` そのものの優先度キューはスキャンされない点に注意。
- `sys_acre_mbx` は空き ID を自動検索して `sys_cre_mbx` に委譲する。
