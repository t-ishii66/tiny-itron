# sys_tsk -- タスク管理システムコール

対象ファイル: `kernel/sys_tsk.c`, `kernel/sys_tsk.h`

## 概要

Micro ITRON v4.0.0 仕様に基づくタスク管理システムコールの実装。タスクの生成・削除・起動・終了・状態遷移・優先度変更・起床/待ち・強制待ち/再開・参照などの操作を提供する。

全ての公開関数は第1引数に `W apic` (APIC ID、0 または 1 に正規化済み) を取り、呼び出し元 CPU を識別する。現在実行中のタスク ID は `c_tskid[apic]` で取得する。

## 定数・マクロ

タスク状態 (`include/itron.h`):

| 定数 | 値 | 意味 |
|------|-----|------|
| `TTS_NON` | 0x00 | 未生成 |
| `TTS_RUN` | 0x01 | 実行中 |
| `TTS_RDY` | 0x02 | 実行可能 |
| `TTS_WAI` | 0x04 | 待ち状態 |
| `TTS_SUS` | 0x08 | 強制待ち |
| `TTS_WAS` | 0x0c | 二重待ち (WAI + SUS) |
| `TTS_DMT` | 0x10 | 休止状態 |

キューイング上限 (`include/config.h`):

| 定数 | 値 |
|------|-----|
| `TMAX_ACTCNT` | 16 |
| `TMAX_WUPCNT` | 16 |
| `TMAX_SUSCNT` | 16 |
| `MAX_TSKID` | 16 |

特殊定数:

| 定数 | 値 | 意味 |
|------|-----|------|
| `TSK_SELF` | 0 | 自タスク指定 |
| `TPRI_INI` | 0 | 初期優先度に戻す |

## グローバル変数

| 変数 | 型 | 定義場所 | 説明 |
|------|-----|----------|------|
| `tsk[]` | `T_TSK[MAX_TSKID]` | `kernel/val.h` (extern) | タスク管理ブロック配列 |
| `c_tskid[]` | `ID[]` | `kernel/val.h` (extern) | CPU ごとの現在実行中タスク ID |
| `kernel_lk` | `unsigned long` | `kernel/sched.h` (extern) | Big Kernel Lock (caller が取得済み) |

## 関数リファレンス

### tsk_init

```c
void tsk_init(void)
```

**概要:** 全タスク管理ブロックを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. `tsk[0]` から `tsk[MAX_TSKID-1]` まで反復
2. 各タスクの `wupcnt` を 0、`suscnt` を 0、`tskstat` を `TTS_NON` に設定

**呼び出し元:** `itron_init()` (カーネル初期化時)

**注意点:** `proc_init()` より前に呼ばれる必要がある。過去に呼び出し順序が逆でバグが発生した (proc_init が先にタスクを作成 → tsk_init がそれを上書き)。

---

### tsk_stat_change

```c
void tsk_stat_change(ID tskid, STAT s)
```

**概要:** タスクの状態を直接変更する (初期化専用)。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `tskid` | `ID` | タスク ID |
| `s` | `STAT` | 設定する状態値 |

**戻り値:** なし

**処理内容:** `tsk[tskid].tskstat = s` を直接代入する。

**呼び出し元:** 初期化プロセスのみ。

**注意点:** バリデーションなし。内部専用関数。

---

### sys_cre_tsk

```c
ER sys_cre_tsk(W apic, ID tskid, T_CTSK* pk_ctsk)
```

**概要:** タスクを生成する。タスク ID を指定してタスク管理ブロックとプロセス構造体を初期化する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID (0 or 1) |
| `tskid` | `ID` | タスク ID (1〜MAX_TSKID) |
| `pk_ctsk` | `T_CTSK*` | タスク生成情報パケット |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ID` | 不正な ID (範囲外) |
| `E_OBJ` | 既に存在する (TTS_NON 以外) |

**処理内容:**
1. ID 範囲チェック (1〜MAX_TSKID)
2. 状態が `TTS_NON` でなければ `E_OBJ`
3. `proc_create()` でプロセス構造体を生成
4. `tskatr` に `TA_ACT` が含まれていれば `TTS_RDY`、そうでなければ `TTS_DMT` に設定
5. ベース優先度・現在優先度を `itskpri` で初期化
6. `actcnt`, `wupcnt`, `suscnt` を 0 に初期化
7. `T_CTSK` の内容を `tsk[tskid].ctsk` にコピー (再起動時に使用)
8. イベントフラグ・タスク例外・ミューテックス・オーバーラン関連を初期化

**呼び出し元:** `sys_acre_tsk`, `sys_ext_tsk`, `sys_ter_tsk`, ユーザーコード (`cre_tsk`)

**注意点:** `proc_create()` はスタック割り当てと CPU アフィニティ (呼び出し元の APIC ID) の設定を行う。

---

### sys_acre_tsk

```c
ER_ID sys_acre_tsk(W apic, T_CTSK* pk_ctsk)
```

**概要:** 空きタスク ID を自動的に割り当ててタスクを生成する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `pk_ctsk` | `T_CTSK*` | タスク生成情報パケット |

**戻り値:**

| 値 | 意味 |
|-----|------|
| 正値 | 割り当てたタスク ID |
| `E_NOID` | 空き ID なし |
| 負値 | `cre_tsk` からのエラー |

**処理内容:**
1. `tsk[1]` から順に `TTS_NON` のスロットを検索
2. 見つかったら `cre_tsk(i, pk_ctsk)` を呼び出し
3. 成功ならタスク ID を返す

**呼び出し元:** ユーザーコード (`acre_tsk`)

**注意点:** 内部で `cre_tsk()` を呼んでいるが、`sys_cre_tsk()` ではなくラッパー `cre_tsk()` を使用している点に注意。

---

### sys_del_tsk

```c
ER sys_del_tsk(W apic, ID tskid)
```

**概要:** 休止状態のタスクを削除する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ID` | 不正な ID |
| `E_NOEXS` | タスク未生成 |
| `E_OBJ` | TTS_DMT でない (休止状態以外は削除不可) |

**処理内容:**
1. ID・状態チェック
2. `proc_delete(tskid)` でプロセス構造体を解放
3. `tskstat` を `TTS_NON` に設定

**呼び出し元:** `sys_exd_tsk`, ユーザーコード (`del_tsk`)

---

### sys_act_tsk

```c
ER sys_act_tsk(W apic, ID tskid)
```

**概要:** 休止状態のタスクを起動 (活性化) する。既に起動済みなら起動要求をキューイングする。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (0 = 自タスク) |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ID` | 不正な ID |
| `E_NOEXS` | タスク未生成 |
| `E_QOVR` | 起動要求キューイングオーバーフロー |

**処理内容:**
1. `tskid == 0` の場合、`c_tskid[apic]` に置換
2. 状態が `TTS_DMT` でない場合:
   - `actcnt < TMAX_ACTCNT` なら `actcnt` をインクリメント
   - 上限超過なら `E_QOVR`
3. `TTS_DMT` の場合:
   - 状態を `TTS_RDY` に変更
   - `sched_ins()` でレディキューに挿入

**呼び出し元:** `sys_iact_tsk`, `sys_sta_tsk`, `sys_ext_tsk`, `sys_ter_tsk`, ユーザーコード (`act_tsk`)

**注意点:** `sched_next_tsk()` は呼ばない。起動したタスクへの即座の切り替えは行わない。

---

### sys_iact_tsk

```c
ER sys_iact_tsk(W apic, ID tskid)
```

**概要:** 割り込みハンドラから呼べる `act_tsk`。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (0 は不可) |

**戻り値:** `sys_act_tsk` と同じ。`tskid == 0` なら `E_ID`。

**処理内容:** `tskid == 0` のチェック後、`sys_act_tsk()` に委譲。

**呼び出し元:** 割り込みハンドラ

---

### sys_can_act

```c
ER sys_can_act(W apic, ID tskid)
```

**概要:** キューイングされた起動要求数を返し、カウンタをクリアする。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (0 = 自タスク) |

**戻り値:**

| 値 | 意味 |
|-----|------|
| 0以上 | キューイングされていた起動要求数 |
| `E_ID` | 不正な ID |
| `E_NOEXS` | タスク未生成 |

**処理内容:**
1. `tskid == 0` なら自タスク
2. `actcnt` を保存、0 にクリア、保存値を返す

**呼び出し元:** ユーザーコード (`can_act`)

---

### sys_sta_tsk

```c
ER sys_sta_tsk(W apic, ID tskid, VP_INT stacd)
```

**概要:** タスクに起動コード (引数) を設定して起動する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (0 は不可) |
| `stacd` | `VP_INT` | タスクへの起動引数 |

**戻り値:** `sys_act_tsk` と同じ。`tskid == 0` なら `E_OBJ`。

**処理内容:**
1. `proc_set_tsk_arg()` で引数を設定
2. `sys_act_tsk()` を呼び出し

**呼び出し元:** ユーザーコード (`sta_tsk`)

---

### sys_ext_tsk

```c
void sys_ext_tsk(W apic)
```

**概要:** 自タスクを終了する。起動要求が残っていれば再生成・再起動する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |

**戻り値:** なし (復帰しない)

**処理内容:**
1. 自タスクを `TTS_DMT` に変更、`sched_rem()` でレディキューから削除
2. `actcnt > 0` の場合:
   - `actcnt` をデクリメント
   - `TTS_NON` に設定 → `sys_cre_tsk()` で再生成 → `sys_act_tsk()` で再起動
   - return (再起動のため制御を戻す)
3. `actcnt == 0` の場合:
   - `TTS_NON` → `sys_cre_tsk()` で再生成 → `TTS_DMT` に戻す
   - `sched_next_tsk()` で次のタスクを選択

**呼び出し元:** ユーザーコード (`ext_tsk`)

**注意点:** `actcnt == 0` の場合でもタスクを再生成して `TTS_DMT` に置く。タスク自体は消えず休止状態に戻る。

---

### sys_exd_tsk

```c
void sys_exd_tsk(W apic)
```

**概要:** 自タスクを終了し、完全に削除する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |

**戻り値:** なし (復帰しない)

**処理内容:**
1. `sched_rem()` でレディキューから削除
2. `tskstat` を `TTS_NON` に設定
3. `proc_delete()` でプロセス構造体を解放
4. `sched_next_tsk()` で次のタスクを選択

**呼び出し元:** ユーザーコード (`exd_tsk`)

---

### sys_ter_tsk

```c
ER sys_ter_tsk(W apic, ID tskid)
```

**概要:** 他タスクを強制終了する。起動要求が残っていれば再生成・再起動する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (0 は不可) |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ILUSE` | tskid == 0 (自タスク指定は不可) |
| `E_ID` | 不正な ID |
| `E_NOEXS` | タスク未生成 |
| `E_OBJ` | 既に休止状態 |

**処理内容:**
1. 状態を `TTS_DMT` に変更、`sched_rem()` でキューから削除
2. `actcnt > 0` の場合: デクリメント → 再生成 → 再起動
3. `actcnt == 0` の場合: 再生成 → `TTS_DMT` で休止

**呼び出し元:** ユーザーコード (`ter_tsk`)

**注意点:** コメントに「must delete waiting queue」とあるが、待ちキューからの削除は未実装。

---

### sys_chg_pri

```c
ER sys_chg_pri(W apic, ID tskid, PRI tskpri)
```

**概要:** タスクの現在優先度を変更する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (TSK_SELF = 自タスク) |
| `tskpri` | `PRI` | 新しい優先度 (TPRI_INI = 初期優先度) |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ID` | 不正な ID |
| `E_PAR` | 不正な優先度 |

**処理内容:**
1. `TPRI_INI` の場合、初期優先度 (`ctsk.itskpri`) に置換
2. 現在の優先度と異なる場合、`sched_rem()` でキューから削除
3. 新しい優先度を設定
4. `sched_ins()` で新しい優先度のキューに挿入

**呼び出し元:** ユーザーコード (`chg_pri`)

---

### sys_get_pri

```c
ER sys_get_pri(W apic, ID tskid, PRI* p_tskpri)
```

**概要:** タスクの現在優先度を取得する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (TSK_SELF = 自タスク) |
| `p_tskpri` | `PRI*` | 優先度の格納先 |

**戻り値:** `E_OK` または `E_ID`

**処理内容:** `tsk[tskid].tskpri` を `*p_tskpri` に格納。

**呼び出し元:** ユーザーコード (`get_pri`)

---

### sys_slp_tsk

```c
ER sys_slp_tsk(W apic)
```

**概要:** 自タスクを起床待ち状態にする。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |

**戻り値:** `E_OK`

**処理内容:**
1. `wupcnt >= 1` の場合: デクリメントして即座に復帰
2. そうでなければ:
   - `sched_rem()` でレディキューから削除
   - 状態を `TTS_WAI` に変更
   - `sched_next_tsk()` で次のタスクを選択

**前提条件:** caller holds `kernel_lk`

**呼び出し元:** ユーザーコード (`slp_tsk`)、セマフォ・イベントフラグ・データキュー内部

**注意点:** セマフォ等の待ち関数からも内部的に呼ばれ、タスクをブロックする汎用機構として使われている。

---

### sys_tslp_tsk

```c
ER sys_tslp_tsk(W apic, TMO tmout)
```

**概要:** タイムアウト付きで自タスクを起床待ち状態にする。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tmout` | `TMO` | タイムアウト値 |

**戻り値:** `E_OK`

**処理内容:**
1. スケジューラロック取得
2. `wupcnt >= 1` の場合: デクリメントして即座に復帰
3. そうでなければ:
   - `tlink.delta` にタイムアウト値を設定
   - レディキューから削除、`TTS_WAI` に変更
   - `sched_timeout_ins()` でタイムアウトキューに挿入
   - `sched_next_tsk()` で次のタスクを選択

**呼び出し元:** ユーザーコード (`tslp_tsk`)、セマフォ・イベントフラグ・データキュー内部

**注意点:** `TMO_POL` のチェックは行っていない (呼び出し元で処理される想定)。

---

### sys_wup_tsk

```c
ER sys_wup_tsk(W apic, ID tskid)
```

**概要:** 待ち状態のタスクを起床する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (TSK_SELF = 自タスク) |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_NOEXS` | タスク未生成 |
| `E_OBJ` | 休止状態 |
| `E_QOVR` | 起床要求キューイングオーバーフロー |

**処理内容:**
1. スケジューラロック取得
2. 対象が `TTS_WAI` でない場合:
   - `TTS_NON` → `E_NOEXS`
   - `TTS_DMT` → `E_OBJ`
   - その他: `wupcnt` をインクリメント (上限チェック)
3. `TTS_WAI` の場合:
   - 状態を `TTS_RDY` に変更
   - 他タスクへの wup (flag==1): 呼び出し元タスクも `TTS_RDY` に、対象をレディキューに挿入
   - タイムアウトキューから削除 (存在する場合)
   - `sched_next_tsk()` でスケジューリング

**呼び出し元:** ユーザーコード (`wup_tsk`)

**注意点:** TSK_SELF を指定した場合、`flag=0` になりレディキューへの挿入を行わない分岐に入る。

---

### iwup_tsk

```c
ER iwup_tsk(W apic, ID tskid)
```

**概要:** 割り込みハンドラからタスクを起床する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID |

**戻り値:** `sys_wup_tsk` と同じ。

**処理内容:**
1. スケジューラロック取得
2. 対象が `TTS_WAI` でない場合: `wupcnt` インクリメント (エラーチェック含む)
3. `TTS_WAI` の場合:
   - `TTS_RDY` に変更
   - 現在実行中タスクも `TTS_RDY` に設定
   - `sched_ins()` でレディキューに挿入
   - タイムアウトキューから削除
   - `sched_next_tsk()` を呼び出し

**呼び出し元:** キーボード割り込みハンドラ (`key_intr`)

**注意点:** `sched_next_tsk()` は両 CPU の `next_tsk_flag` をセットするため、CPU 0 で発生した IRQ1 により CPU 1 のタスクも次の APIC タイマー割り込みで切り替わる。

---

### sys_can_wup

```c
ER_UINT sys_can_wup(W apic, ID tskid)
```

**概要:** キューイングされた起床要求数を返し、カウンタをクリアする。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (TSK_SELF = 自タスク) |

**戻り値:** 起床要求数、または `E_ID` / `E_NOEXS` / `E_OBJ`

**処理内容:** `wupcnt` を保存、0 にクリア、保存値を返す。

**呼び出し元:** ユーザーコード (`can_wup`)

---

### sys_rel_wai

```c
ER sys_rel_wai(W apic, ID tskid)
```

**概要:** タスクの待ち状態を強制解除する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (TSK_SELF は不可) |

**戻り値:** `E_OK`、`E_OBJ`、`E_ID`、`E_NOEXS`

**処理内容:**
1. `TTS_WAI` の場合:
   - `proc_set_return_value(E_RLWAI)` で待ちサービスコールの戻り値を設定
   - `wlink_rem()` でオブジェクト待ちキューから除去
   - `TTS_RDY` に変更、レディキューに挿入
2. `TTS_WAS` の場合:
   - `proc_set_return_value(E_RLWAI)` で待ちサービスコールの戻り値を設定
   - `wlink_rem()` でオブジェクト待ちキューから除去
   - `TTS_SUS` に変更 (強制待ち部分は維持)

**呼び出し元:** ユーザーコード (`rel_wai`)

---

### sys_irel_wai

```c
ER sys_irel_wai(W apic, ID tskid)
```

**概要:** 割り込みハンドラから待ち状態を解除する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID |

**戻り値:** `E_OK`

**処理内容:** 現時点ではスタブ実装 (`E_OK` を返すのみ)。

---

### sys_sus_tsk

```c
ER sys_sus_tsk(W apic, ID tskid)
```

**概要:** タスクを強制待ち状態にする。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (TSK_SELF = 自タスク) |

**戻り値:** `E_OK`、`E_ID`、`E_NOEXS`、`E_OBJ`、`E_QOVR`

**処理内容:**
1. `suscnt` をインクリメント (上限チェック: `TMAX_SUSCNT`)
2. 対象が `TTS_RUN` の場合: ビジーウェイト (最大 100000 回) で状態変化を待つ
3. `TTS_RDY` → `TTS_SUS` に変更
4. `TTS_WAI` → `TTS_WAS` に変更
5. `sched_rem()` でレディキューから削除

**呼び出し元:** ユーザーコード (`sus_tsk`)

**注意点:** 他 CPU で実行中のタスクに対するサスペンドは、ビジーウェイトで状態変化を待つ暫定実装。SMP 環境ではタイマーハンドラが `tskstat` を確認してディスパッチする必要がある。

---

### sys_rsm_tsk

```c
ER sys_rsm_tsk(W apic, ID tskid)
```

**概要:** 強制待ちタスクを再開する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID |

**戻り値:** `E_OK`

**処理内容:** 現時点ではスタブ実装 (`E_OK` を返すのみ)。

---

### sys_frsm_tsk

```c
ER sys_frsm_tsk(W apic, ID tskid)
```

**概要:** 強制待ちタスクを強制再開する (suscnt を一括クリア)。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID |

**戻り値:** `E_OK`

**処理内容:** 現時点ではスタブ実装 (`E_OK` を返すのみ)。

---

### sys_dly_tsk

```c
ER sys_dly_tsk(W apic, RELTIM dlytim)
```

**概要:** 自タスクを指定時間だけ遅延させる。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `dlytim` | `RELTIM` | 遅延時間 |

**戻り値:** `E_OK`

**処理内容:** 現時点ではスタブ実装 (`E_OK` を返すのみ、実際の遅延なし)。

**注意点:** 実際の遅延は未実装。

---

### sys_ref_tsk

```c
ER sys_ref_tsk(W apic, ID tskid, T_RTSK* pk_rtsk)
```

**概要:** タスクの詳細状態を参照する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (TSK_SELF = 自タスク) |
| `pk_rtsk` | `T_RTSK*` | 参照情報の格納先 |

**戻り値:** `E_OK` または `E_ID`

**処理内容:** `T_RTSK` 構造体に以下を格納:
- `tskstat`: 現在の状態
- `tskpri`: 現在の優先度
- `tskbpri`: ベース優先度
- `tskwait`: 0 (未実装)
- `wobjid`: 0 (未実装)
- `lefttmo`: 0 (未実装)
- `actcnt`, `wupcnt`, `suscnt`

---

### sys_ref_tst

```c
ER sys_ref_tst(W apic, ID tskid, T_RTST* pk_rtst)
```

**概要:** タスクの状態のみを参照する (簡易版)。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `tskid` | `ID` | タスク ID (TSK_SELF = 自タスク) |
| `pk_rtst` | `T_RTST*` | 参照情報の格納先 |

**戻り値:** `E_OK` または `E_ID`

**処理内容:** `tskstat` と `tskwait` (常に 0) を格納。

---

### sys_printf

```c
ER sys_printf(W apic, char** sp)
```

**概要:** ユーザータスクからカーネルの `printk2()` を呼び出す。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `sp` | `char**` | `printk2` に渡す書式文字列と引数 |

**戻り値:** `E_OK`

**処理内容:** `printk2(sp)` を呼び出す。

**呼び出し元:** ユーザーコード (printf syscall、ファンクションコード `TFN_EXD_PRINT`)

---

### tsk_dump

```c
void tsk_dump(void)
```

**概要:** 全アクティブタスクの状態をデバッグ出力する。

**引数:** なし

**戻り値:** なし

**処理内容:** `TTS_NON` でない全タスクについて ID と `tskstat` を `printk` で出力する。

**呼び出し元:** デバッグ用

---

### タスク例外ハンドラ関連 (sys_tsk.h で宣言)

以下の関数は `sys_tsk.h` で宣言されているが、`sys_tsk.c` には実装がない (別ファイルに実装、またはスタブ):

| 関数 | 概要 |
|------|------|
| `sys_def_tex(W, ID, T_DTEX*)` | タスク例外処理ルーチン定義 |
| `sys_ras_tex(W, ID, TEXPTN)` | タスク例外処理要求 |
| `sys_iras_tex(W, ID, TEXPTN)` | 割り込みからのタスク例外処理要求 |
| `sys_dis_tex(W)` | タスク例外処理禁止 |
| `sys_ena_tex(W)` | タスク例外処理許可 |
| `sys_sns_tex(W)` | タスク例外処理禁止状態参照 |
| `sys_ref_tex(W, ID, T_RTEX*)` | タスク例外処理状態参照 |

## 補足

### タスク状態遷移図

```
TTS_NON --[cre_tsk]--> TTS_DMT --[act_tsk]--> TTS_RDY <---> TTS_RUN
                          ^                      |              |
                          |                      v              v
                       [ext_tsk]              TTS_SUS       TTS_WAI
                       [ter_tsk]                 ^              |
                       [del_tsk]                 |              v
                                              TTS_WAS (WAI+SUS)
```

### スピンロックの使用

全てのタスク管理関数は Big Kernel Lock (`kernel_lk`) で保護されている。`kernel_lk` は呼び出し元 (`c_intr_syscall` 等) で取得済みであり、各関数内ではロック操作を行わない。`iwup_tsk` も ISR ハンドラ (`c_intr_irq1` 等) が `kernel_lk` を取得済みの状態で呼ばれる。

### actcnt による再起動メカニズム

`sys_ext_tsk` と `sys_ter_tsk` は、タスク終了時に `actcnt > 0` であれば自動的にタスクを再生成・再起動する。再生成には `tsk[].ctsk` に保存された初期生成パラメータを使用する。
