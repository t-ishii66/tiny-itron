# sys_mbf.c / sys_mbf.h

対象ファイル: `kernel/sys_mbf.c`, `kernel/sys_mbf.h`

## 概要

メッセージバッファ (message buffer) のシステムコール実装。Micro ITRON v4.0.0 仕様に基づく、可変長メッセージの循環バッファ機構を提供する。

メールボックスとは異なり、メッセージバッファはメッセージの実体をカーネル管理のリングバッファにコピーする。これにより、送信側はメッセージ送信後にバッファを再利用できる。各メッセージはサイズプレフィクス (`unsigned long`, 4バイト) とメッセージ本体で構成され、4バイト境界にアラインされる。

### データ構造

```c
typedef struct t_mbf {
    T_LINK         wlink_r;         /* 受信待ちタスクキュー */
    T_LINK         wlink_s;         /* 送信待ちタスクキュー */
    T_TSK*         wtsk;            /* (未使用) */
    ATR            mbfatr;          /* メッセージバッファ属性 */
    UINT           maxmsz;          /* 最大メッセージサイズ */
    SIZE           mbfsz;           /* バッファ全体のサイズ */
    VP             mbf;             /* バッファ先頭ポインタ */
    UINT           smsgcnt;         /* 格納メッセージ数 */
    unsigned char* mbf_end;         /* バッファ末尾ポインタ */
    unsigned char* mbf_r;           /* 読み出しポインタ */
    unsigned char* mbf_w;           /* 書き込みポインタ */
    VP             mbf_alloc_base;  /* 動的確保時の元ポインタ */
    STAT           mbf_alloc;       /* カーネルが動的確保したか */
    STAT           act;             /* 生成済みフラグ */
} T_MBF;
```

リングバッファのレイアウト:
```
[size(4B)][message data...][size(4B)][message data...]...
 ^mbf_r                     ^mbf_w                        ^mbf_end
```

### 定数

- `MAX_MBFID`: 16 (`include/config.h`)

## 関数リファレンス

### mbf_init

```c
void mbf_init(void)
```

**概要:** 全メッセージバッファを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. ID 1 から `MAX_MBFID` まで全エントリをループ
2. 各エントリの送信待ちキュー (`wlink_s`) と受信待ちキュー (`wlink_r`) を空リストに初期化
3. `act = 0` に設定

---

### sys_cre_mbf

```c
ER sys_cre_mbf(W apic, ID mbfid, T_CMBF* pk_cmbf)
```

**概要:** 指定 ID でメッセージバッファを生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbfid | ID | メッセージバッファ ID (1 ~ MAX_MBFID) |
| pk_cmbf | T_CMBF* | 生成情報パケット (mbfatr, maxmsz, mbfsz, mbf) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_OBJ | 既に生成済み |

**処理内容:**
1. ID 範囲・存在チェック
2. `pk_cmbf` から `mbfatr`, `maxmsz`, `mbfsz`, `mbf` をコピー
3. `mbf` が NULL の場合、`kmem_alloc` で動的確保 (Supervisor ページ、`mbf_alloc = 1`)
4. バッファ先頭アドレスを 4 バイト境界に切り上げアライン
5. バッファ末尾アドレスを 4 バイト境界に切り下げアライン
6. `mbf_end` にアライン済み末尾アドレスを設定
7. `act = 1`, `smsgcnt = 0` で生成完了

**注意:** `mbf_r` と `mbf_w` の初期化が明示的に行われていない (0 初期化に依存)。

---

### sys_acre_mbf

```c
ER_ID sys_acre_mbf(W apic, T_CMBF* pk_cmbf)
```

**概要:** メッセージバッファ ID を自動割り当てで生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| pk_cmbf | T_CMBF* | 生成情報パケット |

**戻り値:** 未定義 (未実装)

**処理内容:** 未実装。関数本体が空。

---

### sys_del_mbf

```c
ER sys_del_mbf(W apic, ID mbfid)
```

**概要:** メッセージバッファを削除する。待ちタスクには E_DLT を通知する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbfid | ID | メッセージバッファ ID |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. 送信待ちキュー (`wlink_s`) の全タスクに E_DLT を通知し、`TTS_RDY` に変更してスケジューラキューに挿入
3. 受信待ちキュー (`wlink_r`) の全タスクに同様の処理
4. 動的確保されたバッファ (`mbf_alloc_base`) を `kmem_free` で解放 (Supervisor ページ)
5. 両キューを空リストにリセット、`act = 0`

---

### sys_snd_mbf

```c
ER sys_snd_mbf(W apic, ID mbfid, VP msg, UINT msgsz)
```

**概要:** メッセージバッファにメッセージを送信する (永久待ち)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbfid | ID | メッセージバッファ ID |
| msg | VP | メッセージデータポインタ |
| msgsz | UINT | メッセージサイズ (バイト) |

**戻り値:** `sys_tsnd_mbf(apic, mbfid, msg, msgsz, TMO_FEVR)` の戻り値

**処理内容:** `sys_tsnd_mbf` に `TMO_FEVR` を指定して委譲する。

---

### sys_psnd_mbf

```c
ER sys_psnd_mbf(W apic, ID mbfid, VP msg, UINT msgsz)
```

**概要:** メッセージバッファにメッセージをポーリング送信する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbfid | ID | メッセージバッファ ID |
| msg | VP | メッセージデータポインタ |
| msgsz | UINT | メッセージサイズ |

**戻り値:** 未定義 (未実装)

**処理内容:** 未実装。関数本体が空。

---

### sys_tsnd_mbf

```c
ER sys_tsnd_mbf(W apic, ID mbfid, VP msg, UINT msgsz, TMO tmout)
```

**概要:** メッセージバッファにメッセージをタイムアウト付きで送信する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbfid | ID | メッセージバッファ ID |
| msg | VP | メッセージデータポインタ |
| msgsz | UINT | メッセージサイズ |
| tmout | TMO | タイムアウト |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |
| E_TMOUT | タイムアウト |

**処理内容:**
1. ID・存在チェック
2. 受信待ちタスクが存在する場合:
   - メッセージデータを受信タスクのバッファに直接コピー (`bcopy`)
   - 戻り値レジスタにメッセージサイズを設定
   - 待ちキューから除去し、`TTS_RDY` に変更してスケジューラキューに挿入
   - E_OK を返す
3. 受信待ちタスクがいない場合:
   - `TMO_POL` なら E_TMOUT を返す
   - 送信待ちキューにタスクを挿入 (`TA_TFIFO` で FIFO 順、それ以外は優先度順)
   - 先頭の送信待ちタスクのメッセージがバッファに収まるなら `mbf_put` で格納し、待ちキューから除去
   - 収まらなければ `sys_slp_tsk` / `sys_tslp_tsk` でブロック

---

### sys_rcv_mbf

```c
ER_UINT sys_rcv_mbf(W apic, ID mbfid, VP msg)
```

**概要:** メッセージバッファからメッセージを受信する (永久待ち)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbfid | ID | メッセージバッファ ID |
| msg | VP | 受信バッファポインタ |

**戻り値:** `sys_trcv_mbf(apic, mbfid, msg, TMO_FEVR)` の戻り値 (正常時: メッセージサイズ)

**処理内容:** `sys_trcv_mbf` に `TMO_FEVR` を指定して委譲する。

---

### sys_prcv_mbf

```c
ER_UINT sys_prcv_mbf(W apic, ID mbfid, VP msg)
```

**概要:** メッセージバッファからメッセージをポーリング受信する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbfid | ID | メッセージバッファ ID |
| msg | VP | 受信バッファポインタ |

**戻り値:** 未定義 (未実装)

**処理内容:** 未実装。関数本体が空。

---

### sys_trcv_mbf

```c
ER_UINT sys_trcv_mbf(W apic, ID mbfid, VP msg, TMO tmout)
```

**概要:** メッセージバッファからメッセージをタイムアウト付きで受信する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbfid | ID | メッセージバッファ ID |
| msg | VP | 受信バッファポインタ |
| tmout | TMO | タイムアウト |

**戻り値:**
| 値 | 説明 |
|------|------|
| 正値 | メッセージサイズ (正常終了) |
| E_OK | タイムアウト待ち後の復帰 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |
| E_TMOUT | タイムアウト |

**処理内容:**
1. ID・存在チェック
2. `mbf_check` でバッファにメッセージがあるか確認
3. メッセージがある場合:
   - `mbf_get` でリングバッファから取り出し、`msg` にコピー
   - `mbf_snd_check` で送信待ちタスクの再評価
   - メッセージサイズを返す
4. メッセージが無い場合:
   - `TMO_POL` なら E_TMOUT を返す
   - 受信待ちキューにタスクを挿入
   - `sys_slp_tsk` / `sys_tslp_tsk` でブロック

---

### sys_ref_mbf

```c
ER sys_ref_mbf(W apic, ID mbfid, T_RMBF* pk_rmbf)
```

**概要:** メッセージバッファの状態を参照する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| mbfid | ID | メッセージバッファ ID |
| pk_rmbf | T_RMBF* | 参照情報格納先 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. 送信待ちタスク (`stskid`)、受信待ちタスク (`rtskid`) を設定 (なければ `TSK_NONE`)
3. 格納メッセージ数 (`smsgcnt`) と空き容量 (`fmbfsz`) を設定

---

### mbf_snd_check

```c
void mbf_snd_check(ID mbfid)
```

**概要:** 送信待ちキューのタスクを再評価し、バッファに空きがあればメッセージを格納する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| mbfid | ID | メッセージバッファ ID |

**戻り値:** なし

**処理内容:**
1. 送信待ちキューを先頭から順にスキャン
2. バッファの空き容量 (`mbf_rest`) がタスクのメッセージサイズ以上なら:
   - `mbf_put` でメッセージをバッファに格納
   - タスクを `TTS_RDY` に変更し、戻り値に E_OK を設定
   - スケジューラキューに挿入し、待ちキューから除去
3. 空き容量が足りなければスキャン終了

---

### mbf_rest

```c
UINT mbf_rest(ID mbfid)
```

**概要:** リングバッファの空き容量を計算する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| mbfid | ID | メッセージバッファ ID |

**戻り値:** 空き容量 (バイト)。`sizeof(unsigned long)` (サイズプレフィクス分) を差し引いた値。

**処理内容:**
- `mbf_w < mbf_r`: `mbf_r - mbf_w` を計算
- `mbf_w >= mbf_r`: `(mbf_end - mbf_w) + (mbf_r - mbf)` を計算 (ラップアラウンド考慮)
- サイズプレフィクス分 (`sizeof(unsigned long)`) を差し引いて返す

---

### mbf_check

```c
UINT mbf_check(ID mbfid)
```

**概要:** リングバッファにメッセージが存在するか確認する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| mbfid | ID | メッセージバッファ ID |

**戻り値:** 0 = メッセージなし、1 = メッセージあり

**処理内容:** `mbf_w == mbf_r` なら 0 (空)、それ以外なら 1 を返す。

---

### mbf_put

```c
void mbf_put(ID mbfid, VP msg, UINT msgsz)
```

**概要:** リングバッファにメッセージを格納する (サイズプレフィクス付き)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| mbfid | ID | メッセージバッファ ID |
| msg | VP | メッセージデータポインタ |
| msgsz | UINT | メッセージサイズ |

**戻り値:** なし

**処理内容:**
1. サイズ値 (`unsigned long`) を `mbf_do_put` でバッファに書き込み
2. メッセージ本体を `mbf_do_put` でバッファに書き込み
3. `smsgcnt` をインクリメント

---

### mbf_get

```c
ER_UINT mbf_get(ID mbfid, VP msg)
```

**概要:** リングバッファからメッセージを取り出す。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| mbfid | ID | メッセージバッファ ID |
| msg | VP | 受信バッファポインタ |

**戻り値:** メッセージサイズ (バイト)

**処理内容:**
1. サイズ値を `mbf_do_get` で読み出し
2. サイズ分のメッセージ本体を `mbf_do_get` で読み出し
3. `smsgcnt` をデクリメント
4. メッセージサイズを返す

---

### mbf_do_put

```c
void mbf_do_put(ID mbfid, VP msg, UINT msgsz)
```

**概要:** リングバッファへの低レベル書き込み (ラップアラウンド対応)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| mbfid | ID | メッセージバッファ ID |
| msg | VP | 書き込みデータポインタ |
| msgsz | UINT | 書き込みサイズ |

**戻り値:** なし

**処理内容:**
1. `mbf_w < mbf_r` の場合: 連続領域にコピー
2. `mbf_w >= mbf_r` の場合:
   - 末尾までの領域に収まるなら連続コピー
   - 収まらなければ末尾まで部分コピーし、先頭に戻って残りをコピー (ラップアラウンド)
3. `mbf_w` がバッファ末尾を超えた場合、先頭にリセット

**注意:** この関数は呼び出し前に `mbf_rest` で空き容量を確認済みであることを前提とする。

---

### mbf_do_get

```c
void mbf_do_get(ID mbfid, VP msg, UINT msgsz)
```

**概要:** リングバッファからの低レベル読み出し (ラップアラウンド対応)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| mbfid | ID | メッセージバッファ ID |
| msg | VP | 読み出し先バッファポインタ |
| msgsz | UINT | 読み出しサイズ |

**戻り値:** なし

**処理内容:**
1. `mbf_r < mbf_w` の場合: 連続領域からコピー
2. `mbf_r >= mbf_w` の場合:
   - 末尾までの領域に収まるなら連続コピー
   - 収まらなければ末尾まで部分コピーし、先頭に戻って残りをコピー (ラップアラウンド)
3. `mbf_r` がバッファ末尾を超えた場合、先頭にリセット

## 補足

- メッセージバッファはメールボックスと異なり、メッセージの実体をコピーする。送信側は送信後にメッセージ領域を自由に再利用できる。
- リングバッファの各メッセージには 4 バイトのサイズプレフィクスが付加される。実効的なバッファ容量は `mbfsz` より `sizeof(unsigned long)` 分少なくなる。
- バッファ先頭と末尾は 4 バイト境界にアラインされる。これにより、アラインされていないアドレスからの `kmem_alloc` でも正しく動作する。
- `mbf_rcv_check` 関数が実装されているが、現在のコードからは呼び出されていない (将来的な使用のための予備実装)。
- `sys_psnd_mbf`, `sys_prcv_mbf`, `sys_acre_mbf` は未実装。
- 送信待ちと受信待ちで別々のキュー (`wlink_s`, `wlink_r`) を持つ。両方のキューに同時にタスクが存在することは通常発生しない。
