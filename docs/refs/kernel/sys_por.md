# sys_por.c / sys_por.h

対象ファイル: `kernel/sys_por.c`, `kernel/sys_por.h`

## 概要

ランデブポート (rendezvous port) のシステムコール実装。Micro ITRON v4.0.0 仕様に基づく、パターンマッチングと双方向メッセージ通信機構を提供する。

ランデブは、クライアント・サーバ型の同期通信方式である。呼び出し側 (caller) はランデブポートに対して呼び出しパターン (`calptn`) 付きでメッセージを送信し、受け付け側 (acceptor/server) は受け付けパターン (`acpptn`) を指定してメッセージを受信する。パターンのビットAND が非ゼロの場合にマッチが成立する。ランデブ成立後、呼び出し側はサーバの応答 (`rpl_rdv`) まで待機状態 (TTS_WAI) に遷移する。

### データ構造

```c
typedef struct t_por {
    T_LINK    wlink_c;    /* 呼び出し待ちタスクキュー (caller) */
    T_LINK    wlink_a;    /* 受け付け待ちタスクキュー (acceptor) */
    ATR       poratr;     /* ポート属性 (TA_TFIFO/TA_TPRI) */
    UINT      maxcmsz;    /* 呼び出しメッセージ最大サイズ */
    UINT      maxrmsz;    /* 返答メッセージ最大サイズ */
    UINT      rdvno;      /* ランデブ番号カウンタ (上位16ビット) */
    STAT      act;        /* 生成済みフラグ */
} T_POR;
```

タスク構造体の関連フィールド:
- `tsk[].acpptn`: 受け付けパターン
- `tsk[].calptn`: 呼び出しパターン
- `tsk[].por_msg`: メッセージバッファポインタ
- `tsk[].por_msgsz`: メッセージサイズ
- `tsk[].p_rdvno`: ランデブ番号格納先ポインタ
- `tsk[].rdvno`: ランデブ番号

ランデブ番号 (`RDVNO`) の構成:
- 上位16ビット: ポートの `rdvno` カウンタ (生成ごとにインクリメント)
- 下位16ビット: 呼び出しタスクのタスク ID

### 定数

- `MAX_PORID`: 16 (`include/config.h`)
- `TBIT_RDVPTN`: 32 ビット (`include/config.h`)

## 関数リファレンス

### por_init

```c
void por_init(void)
```

**概要:** 全ランデブポートを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. ID 1 から `MAX_PORID` まで全エントリをループ
2. 呼び出し待ちキュー (`wlink_c`) と受け付け待ちキュー (`wlink_a`) を空リストに初期化
3. `act = 0` に設定

---

### sys_cre_por

```c
ER sys_cre_por(W apic, ID porid, T_CPOR* pk_cpor)
```

**概要:** 指定 ID でランデブポートを生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| porid | ID | ポート ID (1 ~ MAX_PORID) |
| pk_cpor | T_CPOR* | 生成情報パケット (poratr, maxcmsz, maxrmsz) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_OBJ | 既に生成済み |

**処理内容:**
1. ID 範囲・存在チェック
2. `pk_cpor` から `poratr`, `maxcmsz`, `maxrmsz` をコピー
3. `rdvno` を 0 に初期化
4. `act = 1` で生成完了

---

### sys_acre_por

```c
ER_ID sys_acre_por(W apic, T_CPOR* pk_cpor)
```

**概要:** ランデブポート ID を自動割り当てで生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| pk_cpor | T_CPOR* | 生成情報パケット |

**戻り値:** 未定義 (未実装)

**処理内容:** 未実装。関数本体が空。

---

### sys_del_por

```c
ER sys_del_por(W apic, ID porid)
```

**概要:** ランデブポートを削除する。待ちタスクには E_DLT を通知する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| porid | ID | ポート ID |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. 呼び出し待ちキュー (`wlink_c`) の全タスクに E_DLT を通知し、`TTS_RDY` に変更
3. 受け付け待ちキュー (`wlink_a`) の全タスクに同様の処理
4. 両キューを空リストにリセット
5. `act = 0` で削除完了

---

### sys_cal_por

```c
ER_UINT sys_cal_por(W apic, ID porid, RDVPTN calptn, VP msg, UINT cmsgsz)
```

**概要:** ランデブを呼び出す (永久待ち)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| porid | ID | ポート ID |
| calptn | RDVPTN | 呼び出しパターン (ビットパターン) |
| msg | VP | メッセージバッファ (送信/受信兼用) |
| cmsgsz | UINT | 呼び出しメッセージサイズ |

**戻り値:** `sys_tcal_por` の戻り値 (正常時: 返答メッセージサイズ)

**処理内容:** `sys_tcal_por` に `TMO_FEVR` と `rdvno = 0` を指定して委譲する。

---

### sys_tcal_por

```c
ER_UINT sys_tcal_por(W apic, ID porid, RDVPTN calptn, VP msg, UINT cmsgsz, TMO tmout, RDVNO rdvno)
```

**概要:** ランデブをタイムアウト付きで呼び出す。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| porid | ID | ポート ID |
| calptn | RDVPTN | 呼び出しパターン |
| msg | VP | メッセージバッファ |
| cmsgsz | UINT | メッセージサイズ |
| tmout | TMO | タイムアウト |
| rdvno | RDVNO | 転送時のランデブ番号 (0 なら新規生成) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | ランデブ成立 (返答待ちに遷移) |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |
| E_TMOUT | タイムアウト |

**処理内容:**
1. ID・存在チェック
2. 受け付け待ちタスクが存在し、パターンが一致する場合 (`calptn & t->acpptn != 0`):
   - 呼び出しタスクを `TTS_WAI` に変更し、スケジューラキューから除去
   - 受け付けタスクを `TTS_RDY` に変更
   - メッセージを受け付けタスクのバッファにコピー
   - 戻り値にメッセージサイズを設定
   - ランデブ番号を生成・設定 (上位16ビット: カウンタ、下位16ビット: 呼び出しタスク ID)
   - `rdvno` カウンタをインクリメント
   - 受け付けタスクをスケジューラキューに挿入
   - `sched_next_tsk` でタスク切り替えを要求
3. パターン不一致またはキューが空の場合:
   - `TMO_POL` なら E_TMOUT を返す
   - 呼び出し待ちキューに挿入 (`TA_TFIFO` で FIFO 順、`TA_TPRI` で優先度順)
   - タスクの `calptn`, `por_msg`, `por_msgsz`, `rdvno` を保存
   - `sys_slp_tsk` / `sys_tslp_tsk` でブロック

---

### sys_acp_por

```c
ER_UINT sys_acp_por(W apic, ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg)
```

**概要:** ランデブを受け付ける (永久待ち)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| porid | ID | ポート ID |
| acpptn | RDVPTN | 受け付けパターン |
| p_rdvno | RDVNO* | ランデブ番号格納先 |
| msg | VP | 受信メッセージバッファ |

**戻り値:** `sys_tacp_por` の戻り値 (正常時: メッセージサイズ)

**処理内容:** `sys_tacp_por` に `TMO_FEVR` を指定して委譲する。

---

### sys_pacp_por

```c
ER_UINT sys_pacp_por(W apic, ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg)
```

**概要:** ランデブをポーリング受け付けする。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| porid | ID | ポート ID |
| acpptn | RDVPTN | 受け付けパターン |
| p_rdvno | RDVNO* | ランデブ番号格納先 |
| msg | VP | 受信メッセージバッファ |

**戻り値:** 未定義 (未実装)

**処理内容:** 未実装。関数本体が空。

---

### sys_tacp_por

```c
ER_UINT sys_tacp_por(W apic, ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg, UINT cmsgsz, TMO tmout)
```

**概要:** ランデブをタイムアウト付きで受け付ける。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| porid | ID | ポート ID |
| acpptn | RDVPTN | 受け付けパターン |
| p_rdvno | RDVNO* | ランデブ番号格納先 |
| msg | VP | 受信メッセージバッファ |
| cmsgsz | UINT | メッセージサイズ |
| tmout | TMO | タイムアウト |

**戻り値:**
| 値 | 説明 |
|------|------|
| 正値 | メッセージサイズ (ランデブ成立) |
| E_OK | 待ち後の復帰 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |
| E_TMOUT | タイムアウト |

**処理内容:**
1. ID・存在チェック
2. 呼び出し待ちタスクが存在し、パターンが一致する場合 (`acpptn & t->calptn != 0`):
   - 呼び出しタスクを待ちキューから除去
   - メッセージを受け付けタスクのバッファにコピー
   - ランデブ番号を設定 (呼び出しタスクの `rdvno` があればそれを使用、なければ新規生成)
   - `rdvno` カウンタをインクリメント
   - メッセージサイズを返す
3. マッチしない場合:
   - `TMO_POL` なら E_TMOUT を返す
   - 受け付け待ちキューに挿入
   - タスクの `acpptn`, `por_msg`, `p_rdvno` を保存
   - `sys_slp_tsk` / `sys_tslp_tsk` でブロック

---

### sys_fwd_por

```c
ER sys_fwd_por(W apic, ID porid, RDVPTN calptn, RDVNO rdvno, VP msg, UINT cmsgsz)
```

**概要:** ランデブを他のポートに転送する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| porid | ID | 転送先ポート ID |
| calptn | RDVPTN | 呼び出しパターン |
| rdvno | RDVNO | 元のランデブ番号 |
| msg | VP | メッセージバッファ |
| cmsgsz | UINT | メッセージサイズ |

**戻り値:** `sys_tcal_por` の戻り値

**処理内容:** `sys_tcal_por` に元のランデブ番号 (`rdvno`) を渡して呼び出す。これにより、元の呼び出し元タスクが返答を受け取れるようにランデブ番号が引き継がれる。

---

### sys_rpl_rdv

```c
ER sys_rpl_rdv(W apic, RDVNO rdvno, VP msg, UINT rmsgsz)
```

**概要:** ランデブに返答する (サーバ側が呼び出し元に応答を返す)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| rdvno | RDVNO | ランデブ番号 |
| msg | VP | 返答メッセージ |
| rmsgsz | UINT | 返答メッセージサイズ |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_OBJ | タスクがランデブ待ち状態でないか、ランデブ番号不一致 |

**処理内容:**
1. ランデブ番号の下位16ビットからタスク ID を抽出
2. 該当タスクが `TTS_WAI` 状態でランデブ番号が一致するか確認 (不一致なら E_OBJ)
3. 返答メッセージを呼び出しタスクのバッファにコピー
4. 戻り値レジスタに返答メッセージサイズを設定
5. 呼び出しタスクをスケジューラキューに挿入 (実行可能に復帰)

**注意:** 条件判定 `!(tsk[tskid].tskstat == TTS_WAI) && (tsk[tskid].rdvno == rdvno)` は論理的に `tskstat != TTS_WAI && rdvno == rdvno` と評価されるため、意図と異なる可能性がある (本来は `||` が正しい)。

---

### sys_ref_por

```c
ER sys_ref_por(W apic, ID porid, T_RPOR* pk_rpor)
```

**概要:** ランデブポートの状態を参照する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| porid | ID | ポート ID |
| pk_rpor | T_RPOR* | 参照情報格納先 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. 呼び出し待ちタスク (`ctskid`) を設定 (なければ `TSK_NONE`)
3. 受け付け待ちタスク (`atskid`) を設定 (なければ `TSK_NONE`)

---

### sys_ref_rdv

```c
ER sys_ref_rdv(W apic, RDVNO rdvno, T_RRDV* pk_rrdv)
```

**概要:** ランデブの状態を参照する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| rdvno | RDVNO | ランデブ番号 |
| pk_rrdv | T_RRDV* | 参照情報格納先 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_OBJ | 該当ランデブが存在しない |

**処理内容:**
1. ランデブ番号からタスク ID を抽出
2. タスク状態とランデブ番号の整合性を確認
3. `wtskid` にランデブ番号の下位16ビット (タスク ID) を設定

## 補足

- ランデブ通信は以下の3段階で構成される:
  1. **呼び出し** (`cal_por`): クライアントがメッセージを送信し、ランデブ成立を待つ
  2. **受け付け** (`acp_por`): サーバがメッセージを受信し、処理を行う
  3. **返答** (`rpl_rdv`): サーバが結果を返し、クライアントが復帰する
- パターンマッチング: `calptn & acpptn != 0` でマッチが成立する。これにより、サーバが特定の種類のリクエストのみを受け付けることが可能。
- `sys_fwd_por` は多段ランデブを可能にする。中間サーバが別のポートに転送しても、元の呼び出しタスクが最終的な返答を受け取る。
- `sys_acre_por`, `sys_pacp_por` は未実装。
- ヘッダファイル (`sys_por.h`) では `sys_tcal_por` と `sys_tacp_por` のシグネチャが実装と異なる (TMO 引数の位置が異なる)。
