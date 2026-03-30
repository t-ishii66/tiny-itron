# sys_rdq.c / sys_rdq.h

対象ファイル: `kernel/sys_rdq.c`, `kernel/sys_rdq.h`

## 概要

レディキュー操作およびシステム管理系のシステムコールを実装するファイル。タスクのレディキュー回転、現在タスク ID の取得、CPU ロック/アンロック、ディスパッチ禁止/許可、各種状態参照 (sense) 関数を含む。全関数の第 1 引数 `W apic` は呼び出し元の APIC ID (CPU 番号) であり、syscall ハンドラが自動的に設定する。

## グローバル変数 (参照)

| 変数 | 型 | 説明 |
|------|----|------|
| `c_tskid[]` | `ID[]` | CPU ごとの現在実行中タスク ID |
| `tsk[]` | `T_TSK[]` | タスク管理ブロック配列 |
| `dispatch_stat` | `int` | ディスパッチ状態 (`DISPATCH_ENABLE` / `DISPATCH_DISABLE`) |
| `cpu_stat` | `int` | CPU ロック状態 (`CPU_LOCK` / `CPU_UNLOCK`) |

## 関数リファレンス

### sys_rot_rdq

```c
ER sys_rot_rdq(W apic, PRI tskpri);
```

**概要:** 指定優先度のレディキューを回転する。

**引数:**
- `apic` -- APIC ID (CPU 番号)
- `tskpri` -- 回転対象の優先度。`TPRI_SELF` の場合は自タスクのベース優先度を使用

**戻り値:** `E_OK` -- 正常終了

**処理内容:**
1. `tskpri` が `TPRI_SELF` の場合、`tsk[c_tskid[apic]].tskbpri` から自タスクのベース優先度を取得
2. `sched_rem()` で現在タスクをレディキューから削除
3. `sched_ins()` で同じ優先度のレディキュー末尾に再挿入
4. `E_OK` を返す

---

### sys_irot_rdq

```c
ER sys_irot_rdq(W apic, PRI tskpri);
```

**概要:** 割り込みコンテキスト用のレディキュー回転 (非タスクコンテキスト版)。

**引数:**
- `apic` -- APIC ID
- `tskpri` -- 回転対象の優先度

**戻り値:** `E_OK` -- 正常終了

**処理内容:** 現在の実装はスタブであり、何も行わず `E_OK` を返す。

---

### sys_get_tid

```c
ER sys_get_tid(W apic, ID* p_tskid);
```

**概要:** 現在実行中のタスク ID を取得する。

**引数:**
- `apic` -- APIC ID (CPU 番号)
- `p_tskid` -- タスク ID の格納先ポインタ

**戻り値:** `E_OK` -- 正常終了

**処理内容:** `c_tskid[apic]` の値を `*p_tskid` に書き込む。各 CPU は独立した現在タスク ID を持つため、APIC ID をインデックスに使用する。

---

### sys_iget_tid

```c
ER sys_iget_tid(W apic, ID* p_tskid);
```

**概要:** 割り込みコンテキスト用のタスク ID 取得 (非タスクコンテキスト版)。

**引数:**
- `apic` -- APIC ID
- `p_tskid` -- タスク ID の格納先ポインタ

**戻り値:** `E_OK` -- 正常終了

**処理内容:** 現在の実装はスタブであり、何も行わず `E_OK` を返す。`p_tskid` への書き込みは行われない。

---

### sys_loc_cpu

```c
ER sys_loc_cpu(W apic);
```

**概要:** CPU をロックする (割り込み禁止)。

**引数:**
- `apic` -- APIC ID

**戻り値:** `E_OK` -- 正常終了

**処理内容:** `ccli()` を呼び出して割り込みを禁止する。Ring 0 (カーネルモード) でのみ使用可能。Ring 3 から呼んだ場合は #GP 例外が発生する。

---

### sys_iloc_cpu

```c
ER sys_iloc_cpu(W apic);
```

**概要:** 割り込みコンテキスト用の CPU ロック (非タスクコンテキスト版)。

**引数:**
- `apic` -- APIC ID

**戻り値:** `E_OK` -- 正常終了

**処理内容:** 現在の実装はスタブであり、何も行わず `E_OK` を返す。

---

### sys_unl_cpu

```c
ER sys_unl_cpu(W apic);
```

**概要:** CPU ロックを解除する (割り込み許可)。

**引数:**
- `apic` -- APIC ID

**戻り値:** `E_OK` -- 正常終了

**処理内容:** `csti()` を呼び出して割り込みを許可する。

---

### sys_iunl_cpu

```c
ER sys_iunl_cpu(W apic);
```

**概要:** 割り込みコンテキスト用の CPU ロック解除 (非タスクコンテキスト版)。

**引数:**
- `apic` -- APIC ID

**戻り値:** `E_OK` -- 正常終了

**処理内容:** 現在の実装はスタブであり、何も行わず `E_OK` を返す。

---

### sys_dis_dsp

```c
ER sys_dis_dsp(W apic);
```

**概要:** ディスパッチを禁止する。

**引数:**
- `apic` -- APIC ID

**戻り値:** `E_OK` -- 正常終了

**処理内容:** グローバル変数 `dispatch_stat` を `DISPATCH_DISABLE` (値: 2) に設定する。ディスパッチ禁止中はタスク切り替えが抑制される。

---

### sys_ena_dsp

```c
ER sys_ena_dsp(W apic);
```

**概要:** ディスパッチを許可する。

**引数:**
- `apic` -- APIC ID

**戻り値:** `E_OK` -- 正常終了

**処理内容:** グローバル変数 `dispatch_stat` を `DISPATCH_ENABLE` (値: 1) に設定する。

---

### sys_sns_ctx

```c
BOOL sys_sns_ctx(W apic);
```

**概要:** 非タスクコンテキスト (割り込みコンテキスト) かどうかを判定する。

**引数:**
- `apic` -- APIC ID

**戻り値:** `TRUE` -- 非タスクコンテキスト、`FALSE` -- タスクコンテキスト

**処理内容:** 現在の実装は常に `FALSE` を返す (スタブ)。ライブラリ側 `sns_ctx()` ではアセンブリによる CS セレクタ判定で実装されている。

---

### sys_sns_loc

```c
BOOL sys_sns_loc(W apic);
```

**概要:** CPU ロック状態かどうかを判定する。

**引数:**
- `apic` -- APIC ID

**戻り値:** `TRUE` -- CPU ロック中、`FALSE` -- ロックなし

**処理内容:** グローバル変数 `cpu_stat` が `CPU_LOCK` (値: 1) であれば `TRUE`、それ以外は `FALSE` を返す。

---

### sys_sns_dsp

```c
BOOL sys_sns_dsp(W apic);
```

**概要:** ディスパッチ禁止状態かどうかを判定する。

**引数:**
- `apic` -- APIC ID

**戻り値:** `TRUE` -- ディスパッチ禁止中、`FALSE` -- ディスパッチ許可中

**処理内容:** グローバル変数 `dispatch_stat` が `DISPATCH_DISABLE` (値: 2) であれば `TRUE`、それ以外は `FALSE` を返す。

---

### sys_sns_dpn

```c
BOOL sys_sns_dpn(W apic);
```

**概要:** ディスパッチ保留状態かどうかを判定する。

**引数:**
- `apic` -- APIC ID

**戻り値:** `TRUE` -- ディスパッチ保留あり、`FALSE` -- 保留なし

**処理内容:** 現在の実装は常に `FALSE` を返す (スタブ)。

---

### sys_ref_sys

```c
ER sys_ref_sys(W apic, T_RSYS* pk_rsys);
```

**概要:** システム状態を参照する。

**引数:**
- `apic` -- APIC ID
- `pk_rsys` -- システム状態パケットの格納先ポインタ

**戻り値:** `E_OK` -- 正常終了

**処理内容:** 現在の実装はスタブであり、`pk_rsys` への書き込みを行わず `E_OK` を返す。

## 補足

- `sys_rot_rdq` は現在タスクの削除・再挿入を行うが、`sched_next_tsk()` の呼び出し (ディスパッチ契機の設定) は行っていない。コメント `/* dispatch ! */` が記載されているが、実際のディスパッチ処理は未実装である。
- `i` プレフィックスの関数 (`sys_irot_rdq`, `sys_iget_tid`, `sys_iloc_cpu`, `sys_iunl_cpu`) は割り込みハンドラ内から呼び出すための非タスクコンテキスト版であるが、現時点ではすべてスタブ実装 (`E_OK` を返すのみ) である。
- `sched.h` で定義される定数: `CPU_LOCK=1`, `CPU_UNLOCK=2`, `DISPATCH_ENABLE=1`, `DISPATCH_DISABLE=2`, `DISPATCH_SUSPEND=3`。
