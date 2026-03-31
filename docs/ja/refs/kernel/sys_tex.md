# sys_tex.c

対象ファイル: `kernel/sys_tex.c` (ヘッダ宣言は `kernel/sys_tsk.h` に含まれる)

## 概要

タスク例外処理機能のシステムコールを実装するファイル。タスク例外ハンドラの定義、例外の発生 (raise)、例外処理の有効/無効切り替え、例外状態の参照を提供する。例外が有効化されたとき、保留中の例外パターンがあれば `stack_adjust()` を使ってユーザスタック上にハンドラ呼び出しフレームをプッシュし、タスクの復帰時に例外ハンドラが実行される仕組みとなっている。

## インクルードファイル

- `include/itron.h` -- ITRON 型定義
- `include/types.h` -- 拡張型定義
- `include/config.h` -- カーネル設定定数
- `i386/proc.h` -- プロセス管理構造体
- `kernel/types.h` -- カーネル内部型定義
- `kernel/val.h` -- グローバル変数の extern 宣言

## 関数リファレンス

### sys_def_tex

```c
ER sys_def_tex(W apic, ID tskid, T_DTEX* pk_dtex);
```

**概要:** タスク例外処理ルーチンを定義する。

**引数:**
- `apic` -- APIC ID (CPU 番号)
- `tskid` -- 対象タスク ID。`TSK_SELF` の場合は自タスク
- `pk_dtex` -- 例外処理定義パケットへのポインタ。`NULL` の場合はハンドラを解除

**戻り値:**
- `E_OK` -- 正常終了
- `E_ID` -- ID 不正 (`tskid` が 1 ~ `MAX_TSKID` の範囲外)
- `E_NOEXS` -- オブジェクト未生成 (タスク状態が `TTS_NON`)
- `E_OBJ` -- オブジェクト状態エラー (タスク状態が `TTS_DMT`: 休止状態)

**処理内容:**
1. `tskid` が `TSK_SELF` の場合、`c_tskid[apic]` で自タスク ID を取得
2. `tskid` の範囲チェックとタスク状態チェック
3. `pk_dtex` が `NULL` の場合:
   - `tsk[tskid].tex` の `texatr`, `texrtn`, `exinf` を `NULL` にクリア
   - `tsk[tskid].texstat` を `TTEX_DIS` に設定
4. `pk_dtex` が非 `NULL` の場合:
   - `pk_dtex` から `texatr`, `texrtn`, `exinf` をコピー

---

### sys_ras_tex

```c
ER sys_ras_tex(W apic, ID tskid, TEXPTN rasptn);
```

**概要:** タスク例外処理を要求する (例外パターンを設定)。

**引数:**
- `apic` -- APIC ID
- `tskid` -- 対象タスク ID。`TSK_SELF` の場合は自タスク
- `rasptn` -- 要求する例外パターン (ビットパターン)

**戻り値:**
- `E_OK` -- 正常終了
- `E_ID` -- ID 不正
- `E_NOEXS` -- オブジェクト未生成
- `E_OBJ` -- オブジェクト状態エラー (休止中、または例外ハンドラ未定義)
- `E_PAR` -- パラメータエラー (`rasptn` が 0)

**処理内容:**
1. `tskid` の解決と範囲チェック
2. タスク状態と例外ハンドラの存在チェック
3. `rasptn` が 0 の場合は `E_PAR` を返す
4. `tsk[tskid].pndptn |= rasptn` で保留中の例外パターンに OR で追加

---

### sys_iras_tex

```c
ER sys_iras_tex(W apic, ID tskid, TEXPTN rasptn);
```

**概要:** 割り込みコンテキスト用のタスク例外処理要求。

**引数:**
- `apic` -- APIC ID
- `tskid` -- 対象タスク ID
- `rasptn` -- 要求する例外パターン

**戻り値:** `sys_ras_tex()` と同じ

**処理内容:** `sys_ras_tex(apic, tskid, rasptn)` をそのまま呼び出す。

---

### sys_dis_tex

```c
ER sys_dis_tex(W apic);
```

**概要:** タスク例外処理を禁止する。

**引数:**
- `apic` -- APIC ID

**戻り値:**
- `E_OK` -- 正常終了
- `E_OBJ` -- オブジェクト状態エラー (例外ハンドラが未定義)

**処理内容:**
1. `c_tskid[apic]` で自タスク ID を取得
2. 例外ハンドラ (`texrtn`) が `NULL` の場合は `E_OBJ` を返す
3. `tsk[tskid].texstat = TTEX_DIS` に設定

---

### sys_ena_tex

```c
ER sys_ena_tex(W apic);
```

**概要:** タスク例外処理を許可する。保留中の例外があれば即座にハンドラを起動する。

**引数:**
- `apic` -- APIC ID

**戻り値:**
- `E_OK` -- 正常終了
- `E_OBJ` -- オブジェクト状態エラー (例外ハンドラが未定義)

**処理内容:**
1. `c_tskid[apic]` で自タスク ID を取得
2. 例外ハンドラが `NULL` の場合は `E_OBJ` を返す
3. `tsk[tskid].texstat = TTEX_ENA` に設定
4. `tsk[tskid].pndptn` が非 0 (保留中の例外あり) の場合:
   - `stack_adjust(apic, texrtn, pndptn, exinf)` を呼び出してユーザスタックにハンドラフレームをプッシュ
   - `tsk[tskid].pndptn = 0` にクリア

---

### sys_sns_tex

```c
BOOL sys_sns_tex(W apic);
```

**概要:** タスク例外処理の禁止状態を参照する。

**引数:**
- `apic` -- APIC ID

**戻り値:** `TRUE` -- 例外処理禁止中、`FALSE` -- 例外処理許可中

**処理内容:** `tsk[c_tskid[apic]].texstat` が `TTEX_DIS` であれば `TRUE`、それ以外は `FALSE` を返す。

---

### sys_ref_tex

```c
ER sys_ref_tex(W apic, ID tskid, T_RTEX* pk_rtex);
```

**概要:** タスク例外処理の状態を参照する。

**引数:**
- `apic` -- APIC ID
- `tskid` -- 対象タスク ID。`TSK_SELF` の場合は自タスク
- `pk_rtex` -- 例外状態パケットの格納先ポインタ

**戻り値:**
- (暗黙的に `E_OK` 相当だが `return` 文が欠落)
- `E_ID` -- ID 不正
- `E_NOEXS` -- オブジェクト未生成
- `E_OBJ` -- オブジェクト状態エラー (休止中、または例外ハンドラ未定義)

**処理内容:**
1. `tskid` の解決と範囲チェック
2. `pk_rtex->texstat` に `tsk[tskid].texstat` を設定
3. `pk_rtex->pndptn` に `tsk[tskid].pndptn` を設定

**注意:** 正常終了時の `return` 文が欠落しているため、不定値が返る (バグ)。

---

### check_tex

```c
void check_tex(W apic);
```

**概要:** 保留中のタスク例外を処理する。例外処理が有効かつ保留パターンがある場合、例外ハンドラを起動する。

**引数:**
- `apic` -- APIC ID

**戻り値:** なし

**処理内容:**
1. `c_tskid[apic]` で自タスク ID を取得
2. `pndptn` が非 0 かつ `texstat` が `TTEX_ENA` の場合:
   - `texstat` を `TTEX_DIS` に変更 (再帰防止)
   - `stack_adjust(apic, texrtn, pndptn, exinf)` を呼び出し
   - `pndptn` を 0 にクリア

## 補足

- `stack_adjust()` は `i386/proc.h` で宣言されたアーキテクチャ依存関数であり、ユーザスタック上に例外ハンドラの呼び出しフレームを構築する。syscall からの復帰時にハンドラが実行される仕組みとなっている。
- 例外パターン (`TEXPTN`) は 32 ビットのビットパターン (`TBIT_TEXPTN=32`、`include/config.h` で定義)。`ras_tex` は OR で累積し、ハンドラ実行時にクリアされる。
- `check_tex()` はハンドラ起動前に `texstat` を `TTEX_DIS` に変更する。これにより、例外ハンドラ内からの再帰的な例外発生を防止している。
- ヘッダ宣言 (`sys_def_tex`, `sys_ras_tex` 等) は `kernel/sys_tsk.h` に含まれている。独立した `sys_tex.h` は存在しない。
