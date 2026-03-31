# sys_flg -- イベントフラグ管理システムコール

対象ファイル: `kernel/sys_flg.c`, `kernel/sys_flg.h`

## 概要

Micro ITRON v4.0.0 仕様に基づくイベントフラグ管理システムコールの実装。イベントフラグの生成・削除・セット・クリア・待ち・ポーリング・タイムアウト付き待ち・参照の操作を提供する。

イベントフラグはビットパターン (`FLGPTN`, 32 ビット) で状態を管理し、AND 待ち (全ビット一致) または OR 待ち (いずれかのビット一致) の 2 つの待ちモードをサポートする。

全ての公開関数は第1引数に `W apic` (APIC ID、0 または 1 に正規化済み) を取る。

## 定数・マクロ

イベントフラグ属性 (`include/itron.h`):

| 定数 | 値 | 意味 |
|------|-----|------|
| `TA_TFIFO` | 0x00 | 待ちキューを FIFO 順で管理 |
| `TA_TPRI` | 0x01 | 待ちキューをタスク優先度順で管理 |
| `TA_WSGL` | 0x00 | 単一タスクのみ待ち許可 |
| `TA_WMUL` | 0x02 | 複数タスクの待ち許可 |
| `TA_CLR` | 0x04 | 条件成立時にフラグパターンを自動クリア |

待ちモード:

| 定数 | 値 | 意味 |
|------|-----|------|
| `TWF_ANDW` | 0x00 | AND 待ち (全ビット一致) |
| `TWF_ORW` | 0x01 | OR 待ち (いずれかのビット一致) |

設定上限 (`include/config.h`):

| 定数 | 値 |
|------|-----|
| `MAX_FLGID` | 16 |
| `TBIT_FLGPTN` | 32 |

## グローバル変数

| 変数 | 型 | 定義場所 | 説明 |
|------|-----|----------|------|
| `flg[]` | `T_FLG[MAX_FLGID]` | `kernel/val.h` (extern) | イベントフラグ管理ブロック配列 |
| `kernel_lk` | `unsigned long` | `kernel/sched.h` (extern) | Big Kernel Lock (caller が取得済み) |

### T_FLG 構造体 (kernel/types.h)

```c
typedef struct t_flg {
    T_LINK      wlink;      /* 待ちキュー (双方向リンクリスト) */
    ATR         flgatr;     /* フラグ属性 */
    FLGPTN      flgptn;     /* 現在のフラグビットパターン */
    FLGPTN      iflgptn;    /* 初期フラグビットパターン */
    STAT        act;        /* 活性状態 (0: 未生成, 1: 生成済み) */
} T_FLG;
```

### T_TSK 内のイベントフラグ関連フィールド

```c
FLGPTN*     p_flgptn;   /* 条件成立時のフラグパターン格納先 */
FLGPTN      waiptn;      /* 待ちビットパターン */
MODE        wfmode;      /* 待ちモード (TWF_ANDW / TWF_ORW) */
```

## 関数リファレンス

### flg_init

```c
void flg_init(void)
```

**概要:** 全イベントフラグ管理ブロックを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. `flg[0]` から `flg[MAX_FLGID]` まで反復
2. 各フラグの `wlink` を自己参照 (空リスト) に初期化
3. `act` を 0 (未生成) に設定

**呼び出し元:** `itron_init()` (カーネル初期化時)

---

### sys_cre_flg

```c
ER sys_cre_flg(W apic, ID flgid, T_CFLG* pk_cflg)
```

**概要:** イベントフラグを生成する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `flgid` | `ID` | イベントフラグ ID (1〜MAX_FLGID) |
| `pk_cflg` | `T_CFLG*` | フラグ生成情報パケット |

T_CFLG 構造体:

```c
typedef struct t_cflg {
    ATR     flgatr;     /* フラグ属性 */
    FLGPTN  iflgptn;    /* 初期ビットパターン */
} T_CFLG;
```

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ID` | 不正な ID (範囲外) |
| `E_OBJ` | 既に生成済み |

**処理内容:**
1. ID 範囲チェック
2. `act != 0` なら `E_OBJ`
3. `flgatr` を設定
4. `flgptn` と `iflgptn` の両方を `pk_cflg->iflgptn` で初期化
5. `act = 1` に設定

**前提条件:** caller holds `kernel_lk`

**呼び出し元:** ユーザーコード (`cre_flg`)

---

### sys_acre_flg

```c
ER_ID sys_acre_flg(W apic, T_CFLG* pk_cflg)
```

**概要:** 空きイベントフラグ ID を自動割り当てしてフラグを生成する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `pk_cflg` | `T_CFLG*` | フラグ生成情報パケット |

**戻り値:** `E_OK`

**処理内容:** 現時点ではスタブ実装 (`E_OK` を返すのみ)。ID 自動割り当ては未実装。

---

### sys_del_flg

```c
ER sys_del_flg(W apic, ID flgid)
```

**概要:** イベントフラグを削除する。待ちタスクがあれば全て `E_DLT` で起床する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `flgid` | `ID` | イベントフラグ ID |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ID` | 不正な ID |
| `E_NOEXS` | フラグ未生成 |

**処理内容:**
1. スピンロック取得
2. ID・存在チェック
3. 待ちキューを走査し、全ての待ちタスクに対して:
   - `tskstat` を `TTS_RDY` に変更
   - `sched_ins()` でレディキューに挿入
   - `proc->reg[EAX]` に `E_DLT` を設定
4. 待ちキューを空にリセット
5. `act = 0` に設定

**呼び出し元:** ユーザーコード (`del_flg`)

---

### sys_set_flg

```c
ER sys_set_flg(W apic, ID flgid, FLGPTN setptn)
```

**概要:** イベントフラグのビットパターンをセット (OR) する。条件が成立した待ちタスクを起床する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `flgid` | `ID` | イベントフラグ ID |
| `setptn` | `FLGPTN` | セットするビットパターン |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ID` | 不正な ID |
| `E_NOEXS` | フラグ未生成 |

**処理内容:**
1. スピンロック取得
2. `flgptn |= setptn` でビットを OR 合成
3. 待ちキューを走査し、各タスクについて条件判定:
   - **AND 待ち** (`TWF_ANDW`): `waiptn == flgptn` (全ビット一致) で条件成立
   - **OR 待ち** (`TWF_ORW`): `waiptn & flgptn` (いずれかのビット一致) で条件成立
4. 条件成立した場合:
   - `tskstat` を `TTS_RDY` に変更
   - `sched_ins()` でレディキューに挿入
   - `proc->reg[EAX]` に `E_OK` を設定
   - タスクの `p_flgptn` に現在のフラグパターンを格納
   - `TA_CLR` 属性なら `flgptn = 0` にクリアして走査終了

**呼び出し元:** ユーザーコード (`set_flg`)

**注意点:**
- AND 待ちの判定は `waiptn == flgptn` による完全一致であり、ITRON 仕様の `(waiptn & flgptn) == waiptn` (指定ビットが全て立っているか) とは異なる可能性がある。
- `TA_CLR` 属性が設定されている場合、最初の条件成立でフラグが 0 にクリアされ、後続の待ちタスクは起床されない。

---

### sys_iset_flg

```c
ER sys_iset_flg(W apic, ID flgid, FLGPTN setptn)
```

**概要:** 割り込みハンドラからイベントフラグをセットする。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `flgid` | `ID` | イベントフラグ ID |
| `setptn` | `FLGPTN` | セットするビットパターン |

**戻り値:** `E_OK`

**処理内容:** 現時点ではスタブ実装 (`E_OK` を返すのみ)。

---

### sys_clr_flg

```c
ER sys_clr_flg(W apic, ID flgid, FLGPTN clrptn)
```

**概要:** イベントフラグのビットパターンをクリア (AND) する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `flgid` | `ID` | イベントフラグ ID |
| `clrptn` | `FLGPTN` | クリアパターン (AND マスク) |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ID` | 不正な ID |
| `E_NOEXS` | フラグ未生成 |

**処理内容:**
1. スピンロック取得
2. `flgptn &= clrptn` で AND 演算

**呼び出し元:** ユーザーコード (`clr_flg`)

**注意点:** `clrptn` でビットが 0 の位置がクリアされる。例えば `clrptn = 0xFFFFFFF0` なら下位 4 ビットがクリアされる。

---

### sys_wai_flg

```c
ER sys_wai_flg(W apic, ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN* p_flgptn)
```

**概要:** イベントフラグを永久待ちで待つ。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `flgid` | `ID` | イベントフラグ ID |
| `waiptn` | `FLGPTN` | 待ちビットパターン |
| `wfmode` | `MODE` | 待ちモード (TWF_ANDW / TWF_ORW) |
| `p_flgptn` | `FLGPTN*` | 条件成立時のフラグパターン格納先 |

**戻り値:** `sys_twai_flg` の戻り値と同じ。

**処理内容:** `sys_twai_flg(apic, flgid, waiptn, wfmode, p_flgptn, TMO_FEVR)` に委譲。

**呼び出し元:** ユーザーコード (`wai_flg`)

---

### sys_pol_flg

```c
ER sys_pol_flg(W apic, ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN* p_flgptn)
```

**概要:** イベントフラグをポーリング (非ブロッキング) で検査する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `flgid` | `ID` | イベントフラグ ID |
| `waiptn` | `FLGPTN` | 待ちビットパターン |
| `wfmode` | `MODE` | 待ちモード |
| `p_flgptn` | `FLGPTN*` | 条件成立時のフラグパターン格納先 |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 条件成立 |
| `E_ID` | 不正な ID |
| `E_NOEXS` | フラグ未生成 |
| `E_TMOUT` | 条件不成立 |

**処理内容:**
1. スピンロック取得
2. 条件判定:
   - AND 待ち: `waiptn == flgptn` で成立
   - OR 待ち: `waiptn & flgptn` で成立
3. 条件成立:
   - `p_flgptn` に現在のフラグパターンを格納
   - `TA_CLR` 属性なら `flgptn = 0` にクリア
4. 条件不成立: `E_TMOUT`

**呼び出し元:** ユーザーコード (`pol_flg`)

---

### sys_twai_flg

```c
ER sys_twai_flg(W apic, ID flgid, FLGPTN waiptn, MODE wfmode,
                FLGPTN* p_flgptn, TMO tmout)
```

**概要:** タイムアウト付きでイベントフラグを待つ。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `flgid` | `ID` | イベントフラグ ID |
| `waiptn` | `FLGPTN` | 待ちビットパターン |
| `wfmode` | `MODE` | 待ちモード |
| `p_flgptn` | `FLGPTN*` | 条件成立時のフラグパターン格納先 |
| `tmout` | `TMO` | タイムアウト値 |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 条件成立 (即時、または待ち後に起床) |
| `E_ID` | 不正な ID |
| `E_NOEXS` | フラグ未生成 |
| `E_ILUSE` | TA_WSGL 属性で既に他タスクが待ち中 |
| `E_TMOUT` | ポーリング失敗 |

**処理内容:**
1. スピンロック取得
2. 即時条件判定 (AND/OR):
   - 成立 → `p_flgptn` に格納、`TA_CLR` ならクリア、`E_OK` を返す
3. `TA_WSGL` 属性 (単一待ち) で既に待ちキューにタスクがある場合: `E_ILUSE`
4. `TMO_POL` なら `E_TMOUT`
5. タスクの待ち情報を設定:
   - `tsk[].waiptn = waiptn`
   - `tsk[].wfmode = wfmode`
   - `tsk[].p_flgptn = p_flgptn`
6. 待ちキューに挿入:
   - `TA_TFIFO` → `ins_fifo()` で FIFO 順
   - それ以外 → `ins_pri()` で優先度順
7. `TMO_FEVR` → `sys_slp_tsk()` で永久待ち
8. タイムアウト指定 → `sys_tslp_tsk()` でタイムアウト付き待ち

**呼び出し元:** `sys_wai_flg`、ユーザーコード (`twai_flg`)

**注意点:**
- `TA_WSGL` チェックは待ちキューの空判定ではなく、`flgatr & TA_WSGL` で行われる。`TA_WSGL == 0` なので、この条件は常に偽となり、実質的に `TA_WSGL` の制限は機能していない。フラグ属性に `TA_WMUL` が設定されていない場合に制限がかかる意図と思われる。
- 待ちから起床された場合のフラグパターン格納は `sys_set_flg()` 側で行われる。

---

### sys_ref_flg

```c
ER sys_ref_flg(W apic, ID flgid, T_RFLG* pk_rflg)
```

**概要:** イベントフラグの状態を参照する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `flgid` | `ID` | イベントフラグ ID |
| `pk_rflg` | `T_RFLG*` | 参照情報の格納先 |

T_RFLG 構造体:

```c
typedef struct t_rflg {
    ID      wtskid;     /* 待ちキュー先頭のタスク ID */
    FLGPTN  flgptn;     /* 現在のフラグビットパターン */
} T_RFLG;
```

**戻り値:** `E_OK`、`E_ID`、`E_NOEXS`

**処理内容:**
1. スピンロック取得
2. 待ちキューが空なら `wtskid = TSK_NONE`
3. 空でなければ先頭タスクの ID を設定
4. 現在の `flgptn` を設定

**呼び出し元:** ユーザーコード (`ref_flg`)

## 補足

### AND 待ちの判定について

現在の実装では AND 待ちの条件判定が `waiptn == flgptn` (完全一致) になっている。ITRON 仕様では `(waiptn & flgptn) == waiptn` (指定ビットが全て立っている) が正しい判定である。現在の実装では、待ちパターン以外のビットが立っていると条件が成立しない点に注意が必要。

### TA_WSGL の実装

`TA_WSGL` は値が 0x00 であるため、`flgatr & TA_WSGL` は常に 0 (偽) となる。`sys_twai_flg` 内の `TA_WSGL` チェックは意図通りに機能していない可能性がある。正しくは `!(flgatr & TA_WMUL)` で判定すべきと思われる。

### 排他制御

全てのイベントフラグ操作は Big Kernel Lock (`kernel_lk`) で保護されている。`kernel_lk` は呼び出し元 (`c_intr_syscall` 等) で取得済みであり、イベントフラグ関数内ではロック操作を行わない。

### 条件成立時のフラグパターン通知

待ちタスクが起床される際、`sys_set_flg()` 内でタスクの `p_flgptn` ポインタを通じてフラグパターンが書き戻される。このポインタは `sys_twai_flg()` でタスク構造体に保存されたユーザー空間のアドレスである。
