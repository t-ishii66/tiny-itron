# lib_tim.c

対象ファイル: `lib/lib_tim.c`

## 概要

時間管理に関する ITRON API のユーザライブラリラッパーを提供するファイル。システム時刻の設定・取得、周期ハンドラの生成・制御、アラームハンドラの生成・制御、およびオーバーランハンドラの定義・制御を含む。すべての関数は `syscall()` を通じてカーネルのシステムコールハンドラを呼び出す。

## 関数リファレンス

### システム時刻 (3 関数)

#### set_tim

```c
ER set_tim(SYSTIM* p_systim);
```

**概要:** システム時刻を設定する。

**引数:**
- `p_systim` -- 設定するシステム時刻へのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_SET_TIM, p_systim)` を呼び出す。

---

#### get_tim

```c
ER get_tim(SYSTIM* p_systim);
```

**概要:** システム時刻を取得する。

**引数:**
- `p_systim` -- システム時刻の格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_GET_TIM, p_systim)` を呼び出す。

---

#### isig_tim (スタブ)

```c
ER isig_tim(void);
```

**概要:** タイムティック供給 (割り込みハンドラから呼び出し)。

**引数:** なし

**戻り値:** なし (不定値)

**処理内容:** 関数本体は空 (未実装)。通常、タイマ割り込みハンドラからカーネルのタイムティック処理を起動するために使用する。

---

### 周期ハンドラ (6 関数)

#### cre_cyc

```c
ER cre_cyc(ID cycid, T_CCYC* pk_ccyc);
```

**概要:** 周期ハンドラを生成する。

**引数:**
- `cycid` -- 周期ハンドラ ID
- `pk_ccyc` -- 周期ハンドラ生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_CYC, cycid, pk_ccyc)` を呼び出す。

---

#### acre_cyc

```c
ER_ID acre_cyc(T_CCYC* pk_ccyc);
```

**概要:** 周期ハンドラ ID を自動割り当てして生成する。

**引数:**
- `pk_ccyc` -- 周期ハンドラ生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成された周期ハンドラ ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_CYC, pk_ccyc)` を呼び出す。

---

#### del_cyc

```c
ER del_cyc(ID cycid);
```

**概要:** 周期ハンドラを削除する。

**引数:**
- `cycid` -- 周期ハンドラ ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_CYC, cycid)` を呼び出す。

---

#### sta_cyc

```c
ER sta_cyc(ID cycid);
```

**概要:** 周期ハンドラを動作開始する。

**引数:**
- `cycid` -- 周期ハンドラ ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_STA_CYC, cycid)` を呼び出す。

---

#### stp_cyc

```c
ER stp_cyc(ID cycid);
```

**概要:** 周期ハンドラを動作停止する。

**引数:**
- `cycid` -- 周期ハンドラ ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_STP_CYC, cycid)` を呼び出す。

---

#### ref_cyc

```c
ER ref_cyc(ID cycid, T_RCYC* pk_rcyc);
```

**概要:** 周期ハンドラの状態を参照する。

**引数:**
- `cycid` -- 周期ハンドラ ID
- `pk_rcyc` -- 周期ハンドラ状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_CYC, cycid, pk_rcyc)` を呼び出す。

---

### アラームハンドラ (6 関数)

#### cre_alm

```c
ER cre_alm(ID almid, T_CALM* pk_calm);
```

**概要:** アラームハンドラを生成する。

**引数:**
- `almid` -- アラームハンドラ ID
- `pk_calm` -- アラームハンドラ生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_ALM, almid, pk_calm)` を呼び出す。

---

#### acre_alm

```c
ER_ID acre_alm(T_CALM* pk_calm);
```

**概要:** アラームハンドラ ID を自動割り当てして生成する。

**引数:**
- `pk_calm` -- アラームハンドラ生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成されたアラームハンドラ ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_ALM, pk_calm)` を呼び出す。

---

#### del_alm

```c
ER del_alm(ID almid);
```

**概要:** アラームハンドラを削除する。

**引数:**
- `almid` -- アラームハンドラ ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_ALM, almid)` を呼び出す。

---

#### sta_alm

```c
ER sta_alm(ID almid, RELTIM almtim);
```

**概要:** アラームハンドラを動作開始する。

**引数:**
- `almid` -- アラームハンドラ ID
- `almtim` -- アラーム起動までの相対時間

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_STA_ALM, almid, almtim)` を呼び出す。

---

#### stp_alm

```c
ER stp_alm(ID almid);
```

**概要:** アラームハンドラを動作停止する。

**引数:**
- `almid` -- アラームハンドラ ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_STP_ALM, almid)` を呼び出す。

---

#### ref_alm

```c
ER ref_alm(ID almid, T_RALM* pk_ralm);
```

**概要:** アラームハンドラの状態を参照する。

**引数:**
- `almid` -- アラームハンドラ ID
- `pk_ralm` -- アラームハンドラ状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_ALM, almid, pk_ralm)` を呼び出す。

---

### オーバーランハンドラ (4 関数)

#### def_ovr

```c
ER def_ovr(T_DOVR* pk_dovr);
```

**概要:** オーバーランハンドラを定義する。

**引数:**
- `pk_dovr` -- オーバーランハンドラ定義パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEF_OVR, pk_dovr)` を呼び出す。

---

#### sta_ovr

```c
ER sta_ovr(ID tskid, OVRTIM ovrtim);
```

**概要:** オーバーランハンドラを動作開始する。

**引数:**
- `tskid` -- 対象タスク ID
- `ovrtim` -- オーバーラン検出時間

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_STA_OVR, tskid, ovrtim)` を呼び出す。

---

#### stp_ovr

```c
ER stp_ovr(ID tskid);
```

**概要:** オーバーランハンドラを動作停止する。

**引数:**
- `tskid` -- 対象タスク ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_STP_OVR, tskid)` を呼び出す。

---

#### ref_ovr

```c
ER ref_ovr(ID tskid, T_ROVR* pk_rovr);
```

**概要:** オーバーランハンドラの状態を参照する。

**引数:**
- `tskid` -- 対象タスク ID
- `pk_rovr` -- オーバーランハンドラ状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_OVR, tskid, pk_rovr)` を呼び出す。

## 補足

- 全 19 関数。
- システム時刻: 3 関数 (set/get/isig)。`isig_tim` は空のスタブ。
- 周期ハンドラ: 6 関数 (cre/acre/del/sta/stp/ref)。
- アラームハンドラ: 6 関数 (cre/acre/del/sta/stp/ref)。
- オーバーランハンドラ: 4 関数 (def/sta/stp/ref)。
- タイマーティックは `include/config.h` で ~17ms (PIT, HZ=60) に設定されている (`TIC_NUME=17`, `TIC_DENO=1000`)。APIC タイマーは独立した周期で動作する。
