# lib_int.c

対象ファイル: `lib/lib_int.c`

## 概要

割り込み管理、システム管理に関する ITRON API のユーザライブラリラッパーを提供するファイル。レディキュー操作、タスク ID 取得、CPU ロック、ディスパッチ制御、状態参照 (sense)、割り込みハンドラ / ISR 管理、サービスコール、CPU 例外ハンドラ、カーネル構成・バージョン参照を含む。`sns_ctx` のみアセンブリによる直接判定を行い、syscall を経由しない。

インクルードファイル: `include/itron.h`, `include/config.h`, `i386/addr.h`

## 関数リファレンス

### レディキュー操作 (2 関数)

#### rot_rdq

```c
ER rot_rdq(PRI tskpri);
```

**概要:** 指定優先度のレディキューを回転する。

**引数:**
- `tskpri` -- 回転対象の優先度

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_ROT_RDQ, tskpri)` を呼び出す。

---

#### irot_rdq (スタブ)

```c
ER irot_rdq(PRI tskpri);
```

**概要:** 割り込みコンテキスト用のレディキュー回転。

**引数:**
- `tskpri` -- 回転対象の優先度

**戻り値:** なし (不定値)

**処理内容:** 関数本体は空 (未実装)。

---

### タスク ID 取得 (2 関数)

#### get_tid

```c
ER get_tid(ID* p_tskid);
```

**概要:** 現在実行中のタスク ID を取得する。

**引数:**
- `p_tskid` -- タスク ID の格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_GET_TID, p_tskid)` を呼び出す。

---

#### iget_tid (スタブ)

```c
ER iget_tid(ID* p_tskid);
```

**概要:** 割り込みコンテキスト用のタスク ID 取得。

**引数:**
- `p_tskid` -- タスク ID の格納先ポインタ

**戻り値:** なし (不定値)

**処理内容:** 関数本体は空 (未実装)。

---

### CPU ロック (4 関数)

#### loc_cpu

```c
ER loc_cpu(void);
```

**概要:** CPU をロックする (割り込み禁止)。

**引数:** なし

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_LOC_CPU)` を呼び出す。カーネル側で `ccli()` が実行される。

---

#### iloc_cpu (スタブ)

```c
ER iloc_cpu(void);
```

**概要:** 割り込みコンテキスト用の CPU ロック。

**引数:** なし

**戻り値:** なし (不定値)

**処理内容:** 関数本体は空。コメントアウトされた `sys_iloc_cpu()` の直接呼び出しが残っている。

---

#### unl_cpu

```c
ER unl_cpu(void);
```

**概要:** CPU ロックを解除する (割り込み許可)。

**引数:** なし

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_UNL_CPU)` を呼び出す。カーネル側で `csti()` が実行される。

---

#### iunl_cpu (スタブ)

```c
ER iunl_cpu(void);
```

**概要:** 割り込みコンテキスト用の CPU ロック解除。

**引数:** なし

**戻り値:** なし (不定値)

**処理内容:** 関数本体は空 (未実装)。

---

### ディスパッチ制御 (2 関数)

#### dis_dsp

```c
ER dis_dsp(void);
```

**概要:** ディスパッチを禁止する。

**引数:** なし

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DIS_DSP)` を呼び出す。

---

#### ena_dsp

```c
ER ena_dsp(void);
```

**概要:** ディスパッチを許可する。

**引数:** なし

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_ENA_DSP)` を呼び出す。

---

### 状態参照 - sense (4 関数)

#### sns_ctx

```c
BOOL sns_ctx(void);
```

**概要:** 非タスクコンテキスト (割り込みコンテキスト) かどうかを判定する。

**引数:** なし

**戻り値:** `TRUE` -- 非タスクコンテキスト (カーネルモード)、`FALSE` -- タスクコンテキスト (ユーザモード)

**処理内容:** **syscall を使用しない。** `get_cs()` でアセンブリにより現在の CS セレクタを取得し、`SEL_K32_C` (0x20: カーネルコードセグメント) と比較する。CS が `SEL_K32_C` でなければ `TRUE` (非タスクコンテキスト) を返す。

**特記事項:** カーネル側の `sys_sns_ctx` は常に `FALSE` を返すスタブだが、ライブラリ側では CS セレクタによる実装が行われている。ただしロジックが反転しており、カーネルモード (CS==SEL_K32_C) のとき `FALSE` を返す点に注意。コメントアウトされた `syscall(-TFN_SNS_CTX)` がコード中に残っている。

---

#### sns_loc

```c
BOOL sns_loc(void);
```

**概要:** CPU ロック状態かどうかを判定する。

**引数:** なし

**戻り値:** `BOOL` -- `TRUE`: ロック中、`FALSE`: 非ロック

**処理内容:** `syscall(-TFN_SNS_LOC)` を呼び出す。

---

#### sns_dsp

```c
BOOL sns_dsp(void);
```

**概要:** ディスパッチ禁止状態かどうかを判定する。

**引数:** なし

**戻り値:** `BOOL` -- `TRUE`: ディスパッチ禁止中、`FALSE`: ディスパッチ許可中

**処理内容:** `syscall(-TFN_SNS_DSP)` を呼び出す。

---

#### sns_dpn

```c
BOOL sns_dpn(void);
```

**概要:** ディスパッチ保留状態かどうかを判定する。

**引数:** なし

**戻り値:** `BOOL` -- `TRUE`: 保留あり、`FALSE`: 保留なし

**処理内容:** `syscall(-TFN_SNS_DPN)` を呼び出す。

---

### システム状態参照 (1 関数)

#### ref_sys

```c
ER ref_sys(T_RSYS* pk_rsys);
```

**概要:** システム状態を参照する。

**引数:**
- `pk_rsys` -- システム状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_SYS, pk_rsys)` を呼び出す。

---

### 割り込みハンドラ / ISR 管理 (7 関数)

#### def_inh

```c
ER def_inh(INHNO inhno, T_DINH* pk_dinh);
```

**概要:** 割り込みハンドラを定義する。

**引数:**
- `inhno` -- 割り込みハンドラ番号
- `pk_dinh` -- 割り込みハンドラ定義パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEF_INH, inhno, pk_dinh)` を呼び出す。

---

#### cre_isr

```c
ER cre_isr(ID isrid, T_CISR* pk_cisr);
```

**概要:** ISR (割り込みサービスルーチン) を生成する。

**引数:**
- `isrid` -- ISR ID
- `pk_cisr` -- ISR 生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_ISR, isrid, pk_cisr)` を呼び出す。

---

#### acre_isr

```c
ER_ID acre_isr(T_CISR* pk_cisr);
```

**概要:** ISR ID を自動割り当てして ISR を生成する。

**引数:**
- `pk_cisr` -- ISR 生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成された ISR ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_ISR, pk_cisr)` を呼び出す。

---

#### del_isr

```c
ER del_isr(ID isrid);
```

**概要:** ISR を削除する。

**引数:**
- `isrid` -- ISR ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_ISR, isrid)` を呼び出す。

---

#### ref_isr

```c
ER ref_isr(ID isrid, T_RISR* pk_risr);
```

**概要:** ISR の状態を参照する。

**引数:**
- `isrid` -- ISR ID
- `pk_risr` -- ISR 状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_ISR, isrid, pk_risr)` を呼び出す。

---

#### dis_int

```c
ER dis_int(INTNO intno);
```

**概要:** 指定割り込みを禁止する。

**引数:**
- `intno` -- 割り込み番号

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DIS_INT, intno)` を呼び出す。

---

#### ena_int

```c
ER ena_int(INTNO intno);
```

**概要:** 指定割り込みを許可する。

**引数:**
- `intno` -- 割り込み番号

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_ENA_INT, intno)` を呼び出す。

---

### サービスコール (2 関数)

#### def_svc

```c
ER def_svc(FN fncd, T_DSVC* pk_dsvc);
```

**概要:** 拡張サービスコールルーチンを定義する。

**引数:**
- `fncd` -- 機能コード
- `pk_dsvc` -- サービスコール定義パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEF_SVC, fncd, pk_dsvc)` を呼び出す。

---

#### cal_svc (スタブ / コメントアウト)

```c
ER_UINT cal_svc(FN fncd, VP_INT par1, VP_INT par2, ...);
```

**概要:** 拡張サービスコールを呼び出す。

**引数:**
- `fncd` -- 機能コード
- `par1`, `par2`, ... -- サービスコールに渡すパラメータ (可変長引数)

**戻り値:** なし (不定値)

**処理内容:** 関数本体は空。コメントアウトされた `syscall(-TFN_CAL_SVC, fncd, par1, par2)` がコード中に残っている。

---

### CPU 例外ハンドラ (1 関数)

#### def_exc

```c
ER def_exc(EXCNO excno, T_DEXC* pk_dexc);
```

**概要:** CPU 例外ハンドラを定義する。

**引数:**
- `excno` -- 例外番号
- `pk_dexc` -- 例外ハンドラ定義パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEF_EXC, excno, pk_dexc)` を呼び出す。

---

### カーネル構成・バージョン参照 (2 関数)

#### ref_cfg

```c
ER ref_cfg(T_RCFG* pk_rcfg);
```

**概要:** カーネル構成情報を参照する。

**引数:**
- `pk_rcfg` -- 構成情報パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_CFG, pk_rcfg)` を呼び出す。

---

#### ref_ver

```c
ER ref_ver(T_RVER* pk_rver);
```

**概要:** カーネルバージョン情報を参照する。

**引数:**
- `pk_rver` -- バージョン情報パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_VER, pk_rver)` を呼び出す。

## 補足

- 全 27 関数 (スタブ含む)。
- レディキュー: 2 関数。`irot_rdq` は空のスタブ。
- タスク ID: 2 関数。`iget_tid` は空のスタブ。
- CPU ロック: 4 関数。`iloc_cpu`, `iunl_cpu` は空のスタブ。
- ディスパッチ: 2 関数。
- sense: 4 関数。`sns_ctx` のみ syscall を使わず、アセンブリで CS セレクタを直接チェック。
- システム参照: 1 関数。
- 割り込み: 7 関数 (def_inh / cre_isr / acre_isr / del_isr / ref_isr / dis_int / ena_int)。
- サービスコール: 2 関数。`cal_svc` はコメントアウト。
- 例外: 1 関数。
- 構成・バージョン: 2 関数。
- `sns_ctx` は `i386/addr.h` で定義された `SEL_K32_C` (0x20) を使用する。
