# lib_tsk.c

対象ファイル: `lib/lib_tsk.c`

## 概要

タスク管理に関する ITRON API のユーザライブラリラッパーを提供するファイル。タスクの生成・削除・起動・終了、優先度変更、スリープ/起床、待ち解除、強制待ち、遅延、タスク例外処理、および拡張 printf 機能を含む。ほとんどの関数は `syscall()` を通じてカーネルのシステムコールハンドラを呼び出す。`syscall()` の第 1 引数には `itron.h` で定義された機能コード (TFN_xxx) の符号反転値を渡す。

## 関数リファレンス

### タスク生成・削除

#### cre_tsk

```c
ER cre_tsk(ID tskid, T_CTSK* pk_ctsk);
```

**概要:** タスクを生成する。

**引数:**
- `tskid` -- 生成するタスクの ID
- `pk_ctsk` -- タスク生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_TSK, tskid, pk_ctsk)` を呼び出す。

---

#### acre_tsk

```c
ER_ID acre_tsk(T_CTSK* pk_ctsk);
```

**概要:** タスク ID を自動割り当てしてタスクを生成する。

**引数:**
- `pk_ctsk` -- タスク生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成されたタスク ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_TSK, pk_ctsk)` を呼び出す。

---

#### del_tsk

```c
ER del_tsk(ID tskid);
```

**概要:** タスクを削除する。

**引数:**
- `tskid` -- 削除対象のタスク ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_TSK, tskid)` を呼び出す。

---

### タスク制御

#### act_tsk

```c
ER act_tsk(ID tskid);
```

**概要:** タスクを起動する。

**引数:**
- `tskid` -- 起動対象のタスク ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_ACT_TSK, tskid)` を呼び出す。

---

#### iact_tsk

```c
ER iact_tsk(ID tskid);
```

**概要:** 割り込みコンテキスト用のタスク起動。

**引数:**
- `tskid` -- 起動対象のタスク ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `sys_iact_tsk(0, tskid)` を**直接呼び出す** (syscall 経由ではない)。APIC ID として 0 がハードコードされている。

**特記事項:** 他の関数と異なり、syscall トラップを使わずカーネル関数を直接呼び出す。割り込みハンドラ内 (既に Ring 0) から使用されることを前提としている。

---

#### can_act

```c
ER_UINT can_act(ID tskid);
```

**概要:** タスクの起動要求をキャンセルし、キューイングされていた起動要求数を返す。

**引数:**
- `tskid` -- 対象タスク ID

**戻り値:** `ER_UINT` -- キューイングされていた起動要求数、またはエラーコード

**処理内容:** `syscall(-TFN_CAN_ACT, tskid)` を呼び出す。

---

#### sta_tsk

```c
ER sta_tsk(ID tskid, VP_INT stacd);
```

**概要:** タスクを起動コード付きで起動する。

**引数:**
- `tskid` -- 起動対象のタスク ID
- `stacd` -- タスクに渡す起動コード

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_STA_TSK, tskid, stacd)` を呼び出す。

---

#### ext_tsk

```c
void ext_tsk(void);
```

**概要:** 自タスクを終了する。

**引数:** なし

**戻り値:** なし (void 宣言だが、内部で syscall の戻り値を return している)

**処理内容:** `syscall(-TFN_EXT_TSK)` を呼び出す。

---

#### exd_tsk

```c
void exd_tsk(void);
```

**概要:** 自タスクを終了して削除する。

**引数:** なし

**戻り値:** なし

**処理内容:** `syscall(-TFN_EXD_TSK)` を呼び出す。

---

#### ter_tsk

```c
ER ter_tsk(ID tskid);
```

**概要:** タスクを強制終了する。

**引数:**
- `tskid` -- 強制終了対象のタスク ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_TER_TSK, tskid)` を呼び出す。

---

### 優先度

#### chg_pri

```c
ER chg_pri(ID tskid, PRI tskpri);
```

**概要:** タスクの優先度を変更する。

**引数:**
- `tskid` -- 対象タスク ID
- `tskpri` -- 新しい優先度

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CHG_PRI, tskid, tskpri)` を呼び出す。

---

#### get_pri

```c
ER get_pri(ID tskid, PRI* p_tskpri);
```

**概要:** タスクの優先度を取得する。

**引数:**
- `tskid` -- 対象タスク ID
- `p_tskpri` -- 優先度の格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_GET_PRI, tskid, p_tskpri)` を呼び出す。

---

### タスク状態参照

#### ref_tsk

```c
ER ref_tsk(ID tskid, T_RTSK* pk_rtsk);
```

**概要:** タスクの詳細状態を参照する。

**引数:**
- `tskid` -- 対象タスク ID
- `pk_rtsk` -- タスク状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_TSK, tskid, pk_rtsk)` を呼び出す。

---

#### ref_tst

```c
ER ref_tst(ID tskid, T_RTST* pk_rtst);
```

**概要:** タスクの簡易状態を参照する。

**引数:**
- `tskid` -- 対象タスク ID
- `pk_rtst` -- タスク状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_TST, tskid, pk_rtst)` を呼び出す。

---

### スリープ/起床

#### slp_tsk

```c
ER slp_tsk(void);
```

**概要:** 自タスクを起床待ちにする (永久待ち)。

**引数:** なし

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_SLP_TSK)` を呼び出す。

---

#### tslp_tsk

```c
ER tslp_tsk(TMO tmout);
```

**概要:** 自タスクをタイムアウト付きの起床待ちにする。

**引数:**
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_TSLP_TSK, tmout)` を呼び出す。

---

#### wup_tsk

```c
ER wup_tsk(ID tskid);
```

**概要:** タスクを起床する。

**引数:**
- `tskid` -- 起床対象のタスク ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_WUP_TSK, tskid)` を呼び出す。

---

#### iwup_tsk (コメントアウト)

```c
#if 0
ER iwup_tsk(ID tskid);
#endif
```

**概要:** 割り込みコンテキスト用のタスク起床。

**処理内容:** `#if 0` で無効化されている。有効時は `sys_iwup_tsk(tskid)` を直接呼び出す想定。

---

#### can_wup

```c
ER_UINT can_wup(ID tskid);
```

**概要:** タスクの起床要求をキャンセルし、キューイングされていた起床要求数を返す。

**引数:**
- `tskid` -- 対象タスク ID

**戻り値:** `ER_UINT` -- キューイングされていた起床要求数、またはエラーコード

**処理内容:** `syscall(-TFN_CAN_WUP, tskid)` を呼び出す。

---

### 待ち解除

#### rel_wai

```c
ER rel_wai(ID tskid);
```

**概要:** タスクの待ち状態を強制解除する。

**引数:**
- `tskid` -- 対象タスク ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REL_WAI, tskid)` を呼び出す。

---

#### irel_wai (スタブ)

```c
ER irel_wai(ID tskid);
```

**概要:** 割り込みコンテキスト用の待ち解除。

**引数:**
- `tskid` -- 対象タスク ID

**戻り値:** なし (不定値)

**処理内容:** 関数本体は空。`#if 0` で囲まれた `sys_irel_wai()` の呼び出しがコメントアウトされている。

---

### 強制待ち

#### sus_tsk

```c
ER sus_tsk(ID tskid);
```

**概要:** タスクを強制待ち状態にする。

**引数:**
- `tskid` -- 対象タスク ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_SUS_TSK, tskid)` を呼び出す。

---

#### rsm_tsk

```c
ER rsm_tsk(ID tskid);
```

**概要:** タスクの強制待ちを解除する。

**引数:**
- `tskid` -- 対象タスク ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_RSM_TSK, tskid)` を呼び出す。

---

#### frsm_tsk

```c
ER frsm_tsk(ID tskid);
```

**概要:** タスクの強制待ちを強制解除する (ネスト回数に関わらず)。

**引数:**
- `tskid` -- 対象タスク ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_FRSM_TSK, tskid)` を呼び出す。

---

### 遅延

#### dly_tsk

```c
ER dly_tsk(RELTIM dlytim);
```

**概要:** 自タスクを指定時間遅延させる。

**引数:**
- `dlytim` -- 遅延時間 (相対時間)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DLY_TSK, dlytim)` を呼び出す。

**注意:** カーネル側の `sys_dly_tsk` は `sys_tslp_tsk` に委譲して実際の遅延を行う。タイムアウト満了時に E_OK が返る。

---

### タスク例外処理

#### def_tex

```c
ER def_tex(ID tskid, T_DTEX* pk_dtex);
```

**概要:** タスク例外処理ルーチンを定義する。

**引数:**
- `tskid` -- 対象タスク ID
- `pk_dtex` -- 例外処理定義パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEF_TEX, tskid, pk_dtex)` を呼び出す。

---

#### ras_tex

```c
ER ras_tex(ID tskid, TEXPTN rasptn);
```

**概要:** タスク例外処理を要求する。

**引数:**
- `tskid` -- 対象タスク ID
- `rasptn` -- 要求する例外パターン

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_RAS_TEX, tskid, rasptn)` を呼び出す。

---

#### dis_tex

```c
ER dis_tex(void);
```

**概要:** タスク例外処理を禁止する。

**引数:** なし

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DIS_TEX)` を呼び出す。

---

#### ena_tex

```c
ER ena_tex(void);
```

**概要:** タスク例外処理を許可する。

**引数:** なし

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_ENA_TEX)` を呼び出す。

---

#### sns_tex

```c
BOOL sns_tex(void);
```

**概要:** タスク例外処理の禁止状態を参照する。

**引数:** なし

**戻り値:** `BOOL` -- `TRUE`: 例外禁止中、`FALSE`: 例外許可中

**処理内容:** `syscall(-TFN_SNS_TEX)` を呼び出す。

---

#### ref_tex

```c
ER ref_tex(ID tskid, T_RTEX* pk_rtex);
```

**概要:** タスク例外処理の状態を参照する。

**引数:**
- `tskid` -- 対象タスク ID
- `pk_rtex` -- 例外状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_TEX, tskid, pk_rtex)` を呼び出す。

---

### 拡張機能

#### printf

```c
ER printf(char *s, ...);
```

**概要:** カーネル経由で文字列を出力する (ITRON 標準外の拡張機能)。

**引数:**
- `s` -- 書式文字列
- `...` -- 可変長引数

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_EXD_PRINT, &s)` を呼び出す。可変長引数のアドレス (`&s`) をカーネルに渡すことで、カーネル側でスタック上の引数にアクセスする。

## 補足

- 全 32 関数 (コメントアウトされた `iwup_tsk` を含む)。
- `iact_tsk` はカーネル関数 `sys_iact_tsk` を直接呼び出す唯一の関数であり、APIC ID として 0 がハードコードされている。
- `iwup_tsk` は `#if 0` で無効化されている。
- `irel_wai` は関数本体が空のスタブである。
- `ext_tsk` と `exd_tsk` は `void` 戻り値だが、内部で `return syscall(...)` を行っている (コンパイラ警告が出る可能性がある)。
