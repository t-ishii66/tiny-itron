# sys_alm.c / sys_alm.h

対象ファイル: `kernel/sys_alm.c`, `kernel/sys_alm.h`

## 概要

アラームハンドラ (alarm handler) のシステムコール実装。Micro ITRON v4.0.0 仕様に基づく、ワンショット (一回限り) のタイマハンドラ機構を提供する。

アラームハンドラは、指定した相対時間 (`almtim`) が経過した後に一度だけ呼び出される関数ハンドラである。周期ハンドラ (`sys_cyc`) と異なり、呼び出し後に自動的に停止する。タイマ割り込みから `alm_intr` が呼ばれるたびに残り時間をデクリメントし、0 以下になったらハンドラを呼び出す。

### データ構造

```c
typedef struct t_alm {
    ATR      almatr;    /* アラームハンドラ属性 */
    VP_INT   exinf;     /* 拡張情報 (ハンドラに渡す引数) */
    FP       almhdr;    /* ハンドラ関数ポインタ */
    STAT     almstat;   /* 動作状態 (TALM_STA / TALM_STP) */
    RELTIM   almtim;    /* 残り時間 (カウントダウン) */
    STAT     act;       /* 生成済みフラグ */
} T_ALM;
```

### 定数

- `MAX_ALMID`: 16 (`include/config.h`)
- `TALM_STA`: 動作中 (カウントダウン中)
- `TALM_STP`: 停止中

## 関数リファレンス

### alm_init

```c
void alm_init(void)
```

**概要:** 全アラームハンドラを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. ID 1 から `MAX_ALMID - 1` までループ (ループ条件は `i < MAX_ALMID`)
2. `act = 0` (未生成) に設定

---

### sys_cre_alm

```c
ER sys_cre_alm(W apic, ID almid, T_CALM* pk_calm)
```

**概要:** 指定 ID でアラームハンドラを生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| almid | ID | アラームハンドラ ID (1 ~ MAX_ALMID) |
| pk_calm | T_CALM* | 生成情報パケット (almatr, exinf, almhdr) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_OBJ | 既に生成済み |

**処理内容:**
1. ID 範囲・存在チェック
2. `pk_calm` から `almatr`, `exinf`, `almhdr` をコピー
3. `almstat = TALM_STP` (停止状態) に設定
4. `act = 1` で生成完了

---

### sys_acre_alm

```c
ER_ID sys_acre_alm(W apic, T_CALM* pk_calm)
```

**概要:** アラームハンドラ ID を自動割り当てで生成する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| pk_calm | T_CALM* | 生成情報パケット |

**戻り値:** 未定義 (未実装)

**処理内容:** 未実装。関数本体が空。

---

### sys_del_alm

```c
ER sys_del_alm(W apic, ID almid)
```

**概要:** アラームハンドラを削除する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| almid | ID | アラームハンドラ ID |

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

### sys_sta_alm

```c
ER sys_sta_alm(W apic, ID almid, RELTIM almtim)
```

**概要:** アラームハンドラを起動する。指定時間後にハンドラが呼び出される。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| almid | ID | アラームハンドラ ID |
| almtim | RELTIM | 起動までの相対時間 (ティック数) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. `almtim` フィールドに指定値を設定
3. `almstat = TALM_STA` (動作中) に設定

**注意:** 既に動作中のアラームに対して再度 `sta_alm` を呼ぶと、残り時間が上書きされてリセットされる。

---

### sys_stp_alm

```c
ER sys_stp_alm(W apic, ID almid)
```

**概要:** アラームハンドラを停止する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| almid | ID | アラームハンドラ ID |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. `almstat = TALM_STP` (停止) に設定

---

### sys_ref_alm

```c
ER sys_ref_alm(W apic, ID almid, T_RALM* pk_ralm)
```

**概要:** アラームハンドラの状態を参照する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| almid | ID | アラームハンドラ ID |
| pk_ralm | T_RALM* | 参照情報格納先 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | ID が範囲外 |
| E_NOEXS | オブジェクト未生成 |

**処理内容:**
1. ID・存在チェック
2. `almstat` に現在の動作状態を設定
3. `lefttim` に残り時間 (`almtim`) を設定

---

### alm_intr

```c
void alm_intr(unsigned long delta)
```

**概要:** タイマ割り込みハンドラから呼ばれ、全アラームハンドラのカウントダウン処理を行う。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| delta | unsigned long | 経過時間 (ティック数) |

**戻り値:** なし (実装上は `return E_OK` があるが、戻り値の型は `void`)

**処理内容:**
1. ID 1 から `MAX_ALMID - 1` までスキャン (ループ条件は `i < MAX_ALMID`)
2. 生成済み (`act != 0`) のエントリのみ処理:
   - `almtim` が 0 より大きい場合: `almtim` から `delta` を減算
   - 減算後に `almtim` が 0 未満で、`almstat` が `TALM_STA` の場合:
     - ハンドラ関数を呼び出す: `(*alm[i].almhdr)(alm[i].exinf)`

**注意:** ハンドラ呼び出し後に `almstat` を `TALM_STP` に変更する処理がないため、次回の `alm_intr` 呼び出しでもハンドラが再度呼ばれる可能性がある (`almtim` が負のままであるため)。ワンショット動作を正しく実現するには、ハンドラ呼び出し後に `almstat = TALM_STP` を設定する必要がある。

## 補足

- アラームハンドラは非タスクコンテキスト (タイマ割り込みハンドラ内) で実行される。ハンドラ内ではブロッキングシステムコールを使用できない。
- 周期ハンドラ (`sys_cyc`) との違い: アラームハンドラはワンショットで、一度ハンドラが呼ばれたらそれで終了する。周期ハンドラは自動的に周期をリセットして繰り返す。
- `alm_intr` のスキャン範囲が `i < MAX_ALMID` のため、ID = MAX_ALMID のアラームハンドラは処理されない。
- `alm_intr` の戻り値の型が `void` であるにも関わらず `return E_OK` が記述されており、コンパイラ警告が出る可能性がある。
- `sys_acre_alm` は未実装。
