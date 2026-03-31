# sys_ovr.c / sys_ovr.h

対象ファイル: `kernel/sys_ovr.c`, `kernel/sys_ovr.h`

## 概要

オーバランハンドラ (overrun handler) のシステムコール実装。Micro ITRON v4.0.0 仕様に基づく、タスクごとの実行時間監視機構を提供する。

オーバランハンドラは、指定タスクの実行時間が指定値を超過した場合に呼び出されるハンドラである。タスクごとに残り実行時間 (`ovrtim`) を設定し、タイマ割り込みのたびにデクリメントする。0 以下になるとハンドラが呼び出され、監視を自動停止する。これにより、特定のタスクが CPU 時間を過度に消費することを検出できる。

### データ構造

タスク構造体 (`T_TSK`) の関連フィールド:

```c
typedef struct tsk {
    /* ... */
    ATR      ovratr;   /* オーバランハンドラ属性 */
    FP       ovrhdr;   /* ハンドラ関数ポインタ */
    OVRTIM   ovrtim;   /* 残り実行時間 (カウントダウン) */
    STAT     ovrstat;  /* 監視状態 (TOVR_STA / TOVR_STP) */
    VP_INT   exinf;    /* 拡張情報 */
    /* ... */
} T_TSK;
```

### 定数

- `MAX_TSKID`: 16 (`include/config.h`)
- `TOVR_STA`: 監視中
- `TOVR_STP`: 監視停止中
- `TSK_SELF`: 自タスク指定

## 関数リファレンス

### ovr_init

```c
void ovr_init(void)
```

**概要:** オーバランハンドラの初期化処理。

**引数:** なし

**戻り値:** なし

**処理内容:** 現在の実装では何も行わない (空関数)。タスク構造体のオーバラン関連フィールドはタスク生成時に初期化される。

---

### sys_def_ovr

```c
ER sys_def_ovr(W apic, T_DOVR* pk_dovr)
```

**概要:** 呼び出しタスクのオーバランハンドラを定義する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID (呼び出し元 CPU) |
| pk_dovr | T_DOVR* | 定義情報パケット (ovratr, ovrhdr) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |

**処理内容:**
1. 呼び出しタスク (`c_tskid[apic]`) の `ovratr` に `pk_dovr->ovratr` を設定
2. 呼び出しタスクの `ovrhdr` に `pk_dovr->ovrhdr` を設定
3. E_OK を返す

**注意:** `pk_dovr` が NULL の場合のハンドラ解除処理は実装されていない。ITRON 仕様では NULL 指定でハンドラを解除する。

---

### sys_sta_ovr

```c
ER sys_sta_ovr(W apic, ID tskid, OVRTIM ovrtim)
```

**概要:** 指定タスクのオーバラン監視を開始する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| tskid | ID | 対象タスク ID (TSK_SELF で自タスク) |
| ovrtim | OVRTIM | 残り実行時間 (ティック数) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | タスク ID が範囲外 |

**処理内容:**
1. `TSK_SELF` の場合、呼び出しタスク ID に変換
2. ID 範囲チェック (1 ~ MAX_TSKID)
3. `ovrtim` フィールドに指定値を設定
4. `ovrstat = TOVR_STA` (監視中) に設定

---

### sys_stp_ovr

```c
ER sys_stp_ovr(W apic, ID tskid)
```

**概要:** 指定タスクのオーバラン監視を停止する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| tskid | ID | 対象タスク ID (TSK_SELF で自タスク) |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | タスク ID が範囲外 |

**処理内容:**
1. `TSK_SELF` の場合、呼び出しタスク ID に変換
2. ID 範囲チェック
3. `ovrstat = TOVR_STP` (監視停止) に設定

---

### sys_ref_ovr

```c
ER sys_ref_ovr(W apic, ID tskid, T_ROVR* pk_rovr)
```

**概要:** 指定タスクのオーバランハンドラ状態を参照する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| tskid | ID | 対象タスク ID (TSK_SELF で自タスク) |
| pk_rovr | T_ROVR* | 参照情報格納先 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |
| E_ID | タスク ID が範囲外 |

**処理内容:**
1. `TSK_SELF` の場合、呼び出しタスク ID に変換
2. ID 範囲チェック
3. `ovrstat` に現在の監視状態を設定
4. `leftotm` に残り実行時間 (`ovrtim`) を設定

---

### ovr_intr

```c
void ovr_intr(W apic, unsigned long delta)
```

**概要:** タイマ割り込みハンドラから呼ばれ、現在実行中タスクのオーバラン時間を更新する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID (割り込み発生 CPU) |
| delta | unsigned long | 経過時間 (ティック数) |

**戻り値:** なし (実装上は `return E_OK` があるが、戻り値の型は `void`)

**処理内容:**
1. 割り込み発生 CPU で現在実行中のタスク ID を取得 (`c_tskid[apic]`)
2. 該当タスクの `ovrstat` が `TOVR_STA` (監視中) の場合:
   - `ovrtim` から `delta` を減算
   - `ovrtim` が 0 未満になった場合:
     - `ovrstat = TOVR_STP` (監視停止) に設定
     - ハンドラ関数を呼び出す: `(*tsk[tskid].ovrhdr)(tskid, tsk[tskid].exinf)`

## 補足

- 他のタイマハンドラ (`cyc_intr`, `alm_intr`) と異なり、`ovr_intr` は全オブジェクトをスキャンするのではなく、呼び出し CPU で現在実行中のタスクのみを対象とする。これは、オーバランが「タスクの実行時間」を監視するものであるため。
- ハンドラ呼び出し後、`ovrstat` は `TOVR_STP` に自動設定される。再度監視を開始するには `sys_sta_ovr` を呼ぶ必要がある。これは `alm_intr` と異なり、正しくワンショット動作が実装されている。
- ハンドラは非タスクコンテキスト (タイマ割り込みハンドラ内) で実行される。ブロッキングシステムコールは使用できない。
- ハンドラの引数は `(tskid, exinf)` で、どのタスクがオーバランしたかを識別できる。
- `sys_def_ovr` はグローバルにハンドラを定義するのではなく、呼び出しタスク個別にハンドラを設定する。ITRON 仕様ではシステム全体で1つのオーバランハンドラだが、本実装ではタスクごとに異なるハンドラを設定可能。
- `ovr_intr` の戻り値の型が `void` であるにも関わらず `return E_OK` が記述されており、コンパイラ警告が出る可能性がある。
- SMP 環境では、各 CPU のタイマ割り込みで `ovr_intr` が呼ばれ、その CPU で実行中のタスクのみが監視対象となる。
