# sys_cyc.c / sys_cyc.h

対象ファイル: `kernel/sys_cyc.c`, `kernel/sys_cyc.h`

## 概要

周期ハンドラ (cyclic handler) のシステムコール実装。Micro ITRON v4.0.0 仕様に基づく、周期的なハンドラ呼び出し機構を提供する。

周期ハンドラは、指定した周期 (`cyctim`) で繰り返し呼び出される関数ハンドラである。初期位相 (`cycphs`) を設定することで、起動開始のタイミングをずらすことができる。タイマ割り込みから `cyc_intr` が呼ばれるたびに残り時間をデクリメントし、0 以下になったらハンドラを呼び出して周期を再設定する。

### データ構造

```c
typedef struct t_cyc {
    ATR      cycatr;    /* 周期ハンドラ属性 (TA_STA 等) */
    VP_INT   exinf;     /* 拡張情報 (ハンドラに渡す引数) */
    FP       cychdr;    /* ハンドラ関数ポインタ */
    RELTIM   cyctim;    /* 現在の残り時間 (カウントダウン) */
    RELTIM   icyctim;   /* 初期周期 (リセット値) */
    RELTIM   cycphs;    /* 位相 (初期遅延) */
    STAT     act;       /* 生成済みフラグ */
    STAT     stat;      /* 動作状態 (TCYC_STA / TCYC_STP) */
} T_CYC;
```

### 定数

- `MAX_CYCID`: 16 (`include/config.h`)
- `TCYC_STA`: 動作中
- `TCYC_STP`: 停止中
- `TA_STA`: 周期ハンドラ属性 (起動時に周期をリセット)

## 関数リファレンス

### cyc_init

```c
void cyc_init(void)
```

**概要:** 全周期ハンドラを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. ID 1 から `MAX_CYCID` まで全エントリをループ
2. `act = 0` (未生成) に設定

---

### sys_cre_cyc

```c
ER sys_cre_cyc(W apic, ID cycid, T_CCYC* pk_ccyc)
```

**概要:** 指定 ID で周期ハンドラを生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| cycid | ID | 周期ハンドラ ID (1 ~ MAX_CYCID) |
| pk_ccyc | T_CCYC* | 生成情報パケット (cycatr, exinf, cychdr, cyctim, cycphs) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_OBJ | 既に生成済み |
| E_PAR | パラメータエラー (cyctim == 0) |

**処理内容:**
1. ID 範囲・存在チェック
2. `cyctim` が 0 の場合 E_PAR を返す
3. `pk_ccyc` から `cycatr`, `exinf`, `cychdr`, `cyctim`, `cycphs` をコピー
4. `icyctim` に `cyctim` の値を保存 (リセット用の初期値)
5. `act = 1`, `stat = TCYC_STP` (生成直後は停止状態) で完了

---

### sys_acre_cyc

```c
ER_ID sys_acre_cyc(W apic, T_CCYC* pk_ccyc)
```

**概要:** 周期ハンドラ ID を自動割り当てで生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| pk_ccyc | T_CCYC* | 生成情報パケット |

**戻り値:** 未定義 (未実装)

**処理内容:** 未実装。関数本体が空。

---

### sys_del_cyc

```c
ER sys_del_cyc(W apic, ID cycid)
```

**概要:** 周期ハンドラを削除する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| cycid | ID | 周期ハンドラ ID |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. `act = 0` で削除完了

---

### sys_sta_cyc

```c
ER sys_sta_cyc(W apic, ID cycid)
```

**概要:** 周期ハンドラを起動する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| cycid | ID | 周期ハンドラ ID |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. `stat = TCYC_STA` (動作中) に設定
3. `TA_STA` 属性が設定されている場合:
   - `cyctim` を `icyctim` (初期周期) にリセット
   - `cycphs` を 0 にリセット

---

### sys_stp_cyc

```c
ER sys_stp_cyc(W apic, ID cycid)
```

**概要:** 周期ハンドラを停止する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| cycid | ID | 周期ハンドラ ID |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. `stat = TCYC_STP` (停止) に設定

---

### sys_ref_cyc

```c
ER sys_ref_cyc(W apic, ID cycid, T_RCYC* pk_rcyc)
```

**概要:** 周期ハンドラの状態を参照する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| cycid | ID | 周期ハンドラ ID |
| pk_rcyc | T_RCYC* | 参照情報格納先 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. `cycstat` に現在の動作状態 (`stat`) を設定
3. `lefttim` に次回起動までの残り時間 (`cycphs + cyctim`) を設定

---

### cyc_intr

```c
void cyc_intr(unsigned long delta)
```

**概要:** タイマ割り込みハンドラから呼ばれ、全周期ハンドラのカウントダウン処理を行う。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| delta | unsigned long | 経過時間 (ティック数) |

**戻り値:** なし

**処理内容:**
1. ID 1 から `MAX_CYCID - 1` までスキャン (ループ条件は `i < MAX_CYCID`)
2. 生成済み (`act != 0`) のエントリのみ処理:
   - 位相 (`cycphs`) が 0 より大きい場合: `cycphs` から `delta` を減算 (初期遅延中)
   - 位相が 0 以下の場合: `cyctim` から `delta` を減算
   - `cyctim` が 0 未満になった場合:
     - `cyctim` を `icyctim` (初期周期) にリセット
     - `stat` が `TCYC_STA` (動作中) であればハンドラ関数を呼び出す: `(*cyc[i].cychdr)(cyc[i].exinf)`

## 補足

- 周期ハンドラは非タスクコンテキスト (タイマ割り込みハンドラ内) で実行される。そのため、ハンドラ内ではブロッキングシステムコールを使用できない。
- `cyc_intr` は生成済みの全周期ハンドラをスキャンするため、`stat == TCYC_STP` でも `cyctim` のカウントダウンは行われる。ただし、ハンドラ呼び出しは `TCYC_STA` の場合のみ実行される。
- 位相 (`cycphs`) は最初の 1 回のみ有効。リセット時に `cycphs` は操作されず、`cyctim` のみが `icyctim` にリセットされる。ただし `sys_sta_cyc` で `TA_STA` 属性がある場合は `cycphs = 0` にリセットされる。
- `cyc_intr` のスキャン範囲が `i < MAX_CYCID` (MAX_CYCID 未満) のため、ID = MAX_CYCID の周期ハンドラは処理されない。
- `sys_acre_cyc` は未実装。
