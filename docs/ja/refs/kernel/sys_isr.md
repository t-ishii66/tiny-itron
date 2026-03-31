# sys_isr.c / sys_isr.h

対象ファイル: `kernel/sys_isr.c`, `kernel/sys_isr.h`

## 概要

割り込みハンドラ管理、ISR (Interrupt Service Routine) 管理、サービスコール管理、CPU 例外ハンドラ管理、およびカーネル構成・バージョン情報の参照を実装するファイル。`isr_init()` はカーネル初期化時に呼ばれ、ISR チェーン、サービスコールテーブル、例外ハンドラテーブルを初期化する。

## グローバル変数

| 変数 | 型 | 説明 |
|------|----|------|
| `isr_top[]` | `T_LINK[MAX_INHID+1]` | 割り込み番号ごとの ISR キューヘッド (双方向リンクリスト) |
| `isr[]` | `T_ISR[]` | ISR 管理ブロック配列 (最大 `MAX_ISRID`=15 個、0-15) |
| `inh[]` | `T_INH[]` | 割り込みハンドラテーブル (最大 `MAX_INHID`=16 個) |
| `svc[]` | `T_SVC[]` | サービスコールテーブル (最大 `MAX_FNCD`=16 個) |
| `exc[]` | `T_EXC[]` | CPU 例外ハンドラテーブル (最大 `MAX_EXC`=15 個、0-15) |

## 関数リファレンス

### isr_init

```c
void isr_init(void);
```

**概要:** ISR チェーン、割り込みハンドラテーブル、サービスコールテーブル、例外ハンドラテーブルを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. `isr_top[0..MAX_INHID]` の各要素を自己参照リンク (空リスト) に初期化
2. `inh[0..MAX_INHID]` の `inhatr` と `inthdr` を 0 にクリア
3. `isr[0..MAX_ISRID]` の `act` フラグを 0 にクリア
4. `svc_init()` を呼び出してサービスコールテーブルを初期化
5. `exc_init()` を呼び出して例外ハンドラテーブルを初期化

**注意:** ソースコード上では `svc_init()` と `exc_init()` は `isr_init()` とは別の関数として定義されているが、カーネル初期化時に `isr_init()` から呼び出されるかどうかは呼び出し元の実装に依存する。

---

### sys_def_inh

```c
ER sys_def_inh(W apic, INHNO inhno, T_DINH* pk_dinh);
```

**概要:** 割り込みハンドラを定義 (登録) する。

**引数:**
- `apic` -- APIC ID (CPU 番号)
- `inhno` -- 割り込みハンドラ番号 (0 ~ `MAX_INHID`)
- `pk_dinh` -- 割り込みハンドラ定義パケットへのポインタ

**戻り値:**
- `E_OK` -- 正常終了
- `E_PAR` -- パラメータエラー (`inhno` が範囲外)

**処理内容:**
1. `inhno` の範囲チェック (0 ~ `MAX_INHID`)
2. `inh[inhno].inhatr` に `pk_dinh->inhatr` を設定
3. `inh[inhno].inthdr` に `pk_dinh->inthdr` を設定

---

### sys_cre_isr

```c
ER sys_cre_isr(W apic, ID isrid, T_CISR* pk_cisr);
```

**概要:** ISR (割り込みサービスルーチン) を生成し、指定割り込み番号のキューに挿入する。

**引数:**
- `apic` -- APIC ID
- `isrid` -- ISR ID (0 ~ `MAX_ISRID`)
- `pk_cisr` -- ISR 生成パケットへのポインタ (`isratr`, `exinf`, `intno`, `isr` を含む)

**戻り値:**
- `E_OK` -- 正常終了
- `E_ID` -- ID 不正 (`isrid` が範囲外)
- `E_OBJ` -- オブジェクト状態エラー (指定 ID の ISR が既に存在)

**処理内容:**
1. `isrid` の範囲チェック
2. `isr[isrid].act` が 1 (既存) の場合は `E_OBJ` を返す
3. `pk_cisr` から `isratr`, `exinf`, `intno`, `isr` をコピー
4. `ins_fifo()` で `isr_top[intno]` のキューに FIFO 順で挿入
5. `isr[isrid].act = 1` に設定

---

### sys_acre_isr

```c
ER_ID sys_acre_isr(W apic, T_CISR* pk_cisr);
```

**概要:** ISR ID を自動割り当てして ISR を生成する。

**引数:**
- `apic` -- APIC ID
- `pk_cisr` -- ISR 生成パケットへのポインタ

**戻り値:** 生成された ISR ID、またはエラーコード

**処理内容:** 現在の実装は空関数 (未実装)。

---

### sys_del_isr

```c
ER sys_del_isr(W apic, ID isrid);
```

**概要:** ISR を削除する。

**引数:**
- `apic` -- APIC ID
- `isrid` -- 削除対象の ISR ID

**戻り値:**
- `E_OK` -- 正常終了
- `E_ID` -- ID 不正
- `E_NOEXS` -- オブジェクト未生成

**処理内容:**
1. `isrid` の範囲チェック
2. `isr[isrid].act` が 0 の場合は `E_NOEXS` を返す
3. `wlink_rem()` でキューからリンクを削除
4. `isr[isrid].act = 0` に設定

---

### sys_ref_isr

```c
ER sys_ref_isr(W apic, ID isrid, T_RISR* pk_risr);
```

**概要:** ISR の状態を参照する。

**引数:**
- `apic` -- APIC ID
- `isrid` -- 参照対象の ISR ID
- `pk_risr` -- ISR 状態パケットの格納先ポインタ

**戻り値:**
- `E_OK` -- 正常終了
- `E_ID` -- ID 不正
- `E_NOEXS` -- オブジェクト未生成

**処理内容:** 範囲チェックと存在チェックを行うが、実際のデータ書き込みは未実装 (コメント `/* (^^; */` が記載)。

---

### sys_dis_int

```c
ER sys_dis_int(W apic, INTNO intno);
```

**概要:** 指定割り込みを禁止 (マスク) する。

**引数:**
- `apic` -- APIC ID
- `intno` -- 割り込み番号

**戻り値:** `E_OK` -- 正常終了

**処理内容:** `irq_mask_on(intno)` を呼び出して PIC (i8259) の対応ビットをマスクする。

---

### sys_ena_int

```c
ER sys_ena_int(W apic, INTNO intno);
```

**概要:** 指定割り込みを許可 (アンマスク) する。

**引数:**
- `apic` -- APIC ID
- `intno` -- 割り込み番号

**戻り値:** `E_OK` -- 正常終了

**処理内容:** `irq_mask_off(intno)` を呼び出して PIC の対応ビットをアンマスクする。

---

### svc_init

```c
void svc_init(void);
```

**概要:** サービスコールテーブルを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:** `svc[1..MAX_FNCD]` の `act` フラグを 0 にクリアする。インデックス 0 は使用しない (1 始まり)。

---

### sys_def_svc

```c
ER sys_def_svc(W apic, FN fncd, T_DSVC* pk_dsvc);
```

**概要:** 拡張サービスコールルーチンを定義する。

**引数:**
- `apic` -- APIC ID
- `fncd` -- 機能コード (1 ~ `MAX_FNCD`)
- `pk_dsvc` -- サービスコール定義パケットへのポインタ

**戻り値:**
- `E_OK` -- 正常終了
- `E_PAR` -- パラメータエラー (`fncd` が範囲外)

**処理内容:**
1. `fncd` の範囲チェック (1 ~ `MAX_FNCD`)
2. `svc[fncd].svcatr` に `pk_dsvc->svcatr` を設定
3. `svc[fncd].svcrtn` に `pk_dsvc->svcrtn` を設定
4. `svc[fncd].act = 1` に設定

---

### sys_cal_svc

```c
ER_UINT sys_cal_svc(W apic, FN fncd, VP_INT par1, VP_INT par2, ...);
```

**概要:** 拡張サービスコールを呼び出す。

**引数:**
- `apic` -- APIC ID
- `fncd` -- 機能コード
- `par1`, `par2`, ... -- サービスコールに渡すパラメータ (可変長引数)

**戻り値:** サービスルーチンの戻り値

**処理内容:** 現在の実装は空関数 (未実装)。

---

### exc_init

```c
void exc_init(void);
```

**概要:** CPU 例外ハンドラテーブルを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:** `exc[0..MAX_EXC]` の `exchdr` を 0 にクリアする。

---

### sys_def_exc

```c
ER sys_def_exc(W apic, EXCNO excno, T_DEXC* pk_dexc);
```

**概要:** CPU 例外ハンドラを定義する。

**引数:**
- `apic` -- APIC ID
- `excno` -- 例外番号 (0 ~ `MAX_EXC`)
- `pk_dexc` -- 例外ハンドラ定義パケットへのポインタ

**戻り値:**
- `E_OK` -- 正常終了
- `E_PAR` -- パラメータエラー (`excno` が範囲外)

**処理内容:**
1. `excno` の範囲チェック (0 ~ `MAX_EXC`)
2. `exc[excno].excatr` に `pk_dexc->excatr` を設定
3. `exc[excno].exchdr` に `pk_dexc->exchdr` を設定

---

### sys_ref_cfg

```c
ER sys_ref_cfg(W apic, T_RCFG* pk_rcfg);
```

**概要:** カーネル構成情報を参照する。

**引数:**
- `apic` -- APIC ID
- `pk_rcfg` -- 構成情報パケットの格納先ポインタ

**戻り値:** エラーコード

**処理内容:** 現在の実装は空関数 (未実装)。

---

### sys_ref_ver

```c
ER sys_ref_ver(W apic, T_RVER* pk_rver);
```

**概要:** カーネルバージョン情報を参照する。

**引数:**
- `apic` -- APIC ID
- `pk_rver` -- バージョン情報パケットの格納先ポインタ

**戻り値:** なし (戻り値の `return` 文が欠落)

**処理内容:**
1. `pk_rver->maker` に `TKERNEL_MAKER` (0x0000) を設定
2. `pk_rver->prid` に `TKERNEL_PRID` (0x0001) を設定
3. `pk_rver->spver` に `TKERNEL_SPVER` (0x5400 = Micro ITRON v4.0.0) を設定
4. `pk_rver->prver` に `TKERNEL_PRVER` (0x0000 = version 0.0.0) を設定
5. `pk_rver->prno[0..3]` を `'\0'` にクリア

## 補足

- 1 つの割り込み番号に対して複数の ISR を登録できる。ISR は `isr_top[intno]` を先頭とする双方向リンクリストで管理され、FIFO 順に登録される。
- 割り込みハンドラ (`def_inh`) と ISR (`cre_isr`) は別の仕組みである。`def_inh` は割り込み番号に対して直接ハンドラを設定し、`cre_isr` はキューに ISR を追加する。
- `sys_ref_ver` の戻り値に `return` 文がないため、不定値が返る可能性がある (バグ)。
- `MAX_INHID=16`, `MAX_ISRID=15`, `MAX_FNCD=16`, `MAX_EXC=15` は `include/config.h` で定義されている。
