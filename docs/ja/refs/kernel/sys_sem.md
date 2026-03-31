# sys_sem -- セマフォ管理システムコール

対象ファイル: `kernel/sys_sem.c`, `kernel/sys_sem.h`

## 概要

Micro ITRON v4.0.0 仕様に基づくセマフォ管理システムコールの実装。セマフォの生成・削除・シグナル・待ち・ポーリング・タイムアウト付き待ち・参照の操作を提供する。

また、セマフォ以外のカーネルオブジェクト (イベントフラグ、データキュー等) でも共通して使用される待ちキュー操作ヘルパー関数 (`ins_fifo`, `ins_pri`, `wlink_ins`, `wlink_rem`, `wlink_change`, `wlink_dump`) をこのファイルに実装している。

全ての公開関数は第1引数に `W apic` (APIC ID、0 または 1 に正規化済み) を取る。

## 定数・マクロ

セマフォ属性 (`include/itron.h`):

| 定数 | 値 | 意味 |
|------|-----|------|
| `TA_TFIFO` | 0x00 | 待ちキューを FIFO 順で管理 |
| `TA_TPRI` | 0x01 | 待ちキューをタスク優先度順で管理 |

設定上限 (`include/config.h`):

| 定数 | 値 |
|------|-----|
| `MAX_SEMID` | 16 |
| `TMAX_MAXSEM` | 65535 |

## グローバル変数

| 変数 | 型 | 定義場所 | 説明 |
|------|-----|----------|------|
| `sem[]` | `T_SEM[MAX_SEMID]` | `kernel/val.h` (extern) | セマフォ管理ブロック配列 |
| `kernel_lk` | `unsigned long` | `kernel/sched.h` (extern) | Big Kernel Lock (caller が取得済み) |

### T_SEM 構造体 (kernel/types.h)

```c
typedef struct t_sem {
    T_LINK      wlink;      /* 待ちキュー (双方向リンクリスト) */
    ATR         sematr;     /* セマフォ属性 (TA_TFIFO / TA_TPRI) */
    UINT        isemcnt;    /* 初期セマフォカウント */
    UINT        maxsem;     /* 最大セマフォカウント */
    UINT        semcnt;     /* 現在のセマフォカウント */
    STAT        act;        /* 活性状態 (0: 未生成, 1: 生成済み) */
} T_SEM;
```

### wlink2tsk マクロ

```c
#define wlink2tsk(wlink_ptr) (T_TSK*)((char*)wlink_ptr - sizeof(T_LINK))
```

待ちキューリンク (`wlink`) のポインタから、それを含む `T_TSK` 構造体のポインタを逆算する。`T_TSK` 構造体内で `plink` の次に `wlink` が配置されている前提。

## 関数リファレンス

### sem_init

```c
void sem_init(void)
```

**概要:** 全セマフォ管理ブロックを初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. `sem[0]` から `sem[MAX_SEMID]` まで反復
2. 各セマフォの `wlink` を自己参照 (空リスト) に初期化
3. `act` を 0 (未生成) に設定

**呼び出し元:** `itron_init()` (カーネル初期化時)

**注意点:** ループ範囲が `i <= MAX_SEMID` であり、インデックス 0 も初期化している。セマフォ ID は 1 から始まるが、安全のためインデックス 0 も初期化する。

---

### sys_cre_sem

```c
ER sys_cre_sem(W apic, ID semid, T_CSEM* pk_csem)
```

**概要:** セマフォを生成する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `semid` | `ID` | セマフォ ID (1〜MAX_SEMID) |
| `pk_csem` | `T_CSEM*` | セマフォ生成情報パケット |

T_CSEM 構造体:

```c
typedef struct t_csem {
    ATR     sematr;     /* セマフォ属性 */
    UINT    isemcnt;    /* 初期カウント */
    UINT    maxsem;     /* 最大カウント */
} T_CSEM;
```

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ID` | 不正な ID (範囲外) |
| `E_OBJ` | 既に生成済み |

**処理内容:**
1. ID 範囲チェック (1〜MAX_SEMID)
2. `act != 0` なら `E_OBJ`
3. `act = 1` に設定
4. `sematr` を設定
5. `semcnt` と `isemcnt` の両方を `pk_csem->isemcnt` で初期化
6. `maxsem` を設定

**前提条件:** caller holds `kernel_lk`

**呼び出し元:** ユーザーコード (`cre_sem`)

---

### sys_acre_sem

```c
ER_ID sys_acre_sem(W apic, T_CSEM* pk_csem)
```

**概要:** 空きセマフォ ID を自動的に割り当ててセマフォを生成する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `pk_csem` | `T_CSEM*` | セマフォ生成情報パケット |

**戻り値:** 割り当てた ID (正値) またはエラーコード

**処理内容:**
1. `sem[1]` から `sem[MAX_SEMID]` まで走査し、`act == 0` の未使用エントリを検索
2. 見つかれば `sys_cre_sem(apic, i, pk_csem)` に委譲して生成
3. 成功時は割り当てた ID を返す
4. 空きが見つからなければエラーを返す

---

### sys_del_sem

```c
ER sys_del_sem(W apic, ID semid)
```

**概要:** セマフォを削除する。待ちタスクがあれば全て `E_DLT` で起床する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `semid` | `ID` | セマフォ ID |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ID` | 不正な ID |
| `E_NOEXS` | セマフォ未生成 |

**処理内容:**
1. ID・存在チェック
2. 待ちキューを走査し、全ての待ちタスクに対して:
   - `sched_timeout_rem_if_exist()` でタイムアウトキューから tlink を除去
   - `tskstat` を `TTS_RDY` に変更
   - `proc_set_return_value(t->proc, E_DLT)` で syscall 戻り値を設定
   - `sched_ins()` でレディキューに挿入
3. 待ちキューを空にリセット
4. `act = 0` に設定

**呼び出し元:** ユーザーコード (`del_sem`)

**注意点:** 待ちタスクへのエラー通知は `proc_set_return_value()` で pt_regs フレームの EAX スロットに書き込むことで、`RESTORE_ALL` 時に syscall の戻り値として反映される。

---

### sys_sig_sem

```c
ER sys_sig_sem(W apic, ID semid)
```

**概要:** セマフォにシグナル (資源返却) を発行する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `semid` | `ID` | セマフォ ID |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 |
| `E_ID` | 不正な ID |
| `E_NOEXS` | セマフォ未生成 |
| `E_QOVR` | カウント上限超過 (`semcnt + 1 > maxsem`) |

**処理内容:**
1. ID・存在チェック
2. `semcnt + 1 > maxsem` ならオーバーフロー
4. 待ちタスクが存在する場合 (`wlink.next != &wlink`):
   - `sched_timeout_rem_if_exist()` でタイムアウトキューから tlink を除去
   - 先頭タスクを `TTS_RDY` に変更
   - `sched_ins()` でレディキューに挿入
   - `proc_set_return_value(t->proc, E_OK)` で戻り値を設定
   - `wlink_rem()` で待ちキューから削除
5. 待ちタスクがない場合: `semcnt` をインクリメント

**呼び出し元:** ユーザーコード (`sig_sem`)

**注意点:** 待ちタスクがいればカウントを増やさず直接渡す。これが ITRON 仕様のセマフォ動作。

---

### sys_isig_sem

```c
ER sys_isig_sem(W apic, ID semid)
```

**概要:** 割り込みハンドラからセマフォにシグナルを発行する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `semid` | `ID` | セマフォ ID |

**戻り値:** `E_OK`

**戻り値:** `sys_sig_sem` の戻り値と同じ。

**処理内容:** `sys_sig_sem(apic, semid)` に委譲する。

**呼び出し元:** ISR 内から呼ばれることを想定した API。

---

### sys_wai_sem

```c
ER sys_wai_sem(W apic, ID semid)
```

**概要:** セマフォ資源を永久待ちで獲得する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `semid` | `ID` | セマフォ ID |

**戻り値:** `sys_twai_sem` の戻り値と同じ。

**処理内容:** `sys_twai_sem(apic, semid, TMO_FEVR)` に委譲。

**呼び出し元:** ユーザーコード (`wai_sem`)

---

### sys_pol_sem

```c
ER sys_pol_sem(W apic, ID semid)
```

**概要:** セマフォ資源をポーリング (非ブロッキング) で獲得する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `semid` | `ID` | セマフォ ID |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 (資源獲得成功) |
| `E_ID` | 不正な ID |
| `E_NOEXS` | セマフォ未生成 |
| `E_TMOUT` | 資源なし (セマフォカウントが 0) |

**処理内容:**
1. ID・存在チェック
2. `semcnt == 0` なら `E_TMOUT`
3. `semcnt > 0` なら `semcnt` をデクリメント

**呼び出し元:** ユーザーコード (`pol_sem`)

**注意点:** デモではタスク 2 とタスク 3 が `pol_sem` を使用してバイナリセマフォによる CPU 間排他制御を行っている。ブロッキングの `wai_sem` を使うと、CPU 上で唯一のタスクがブロックされる問題を回避するための選択。

---

### sys_twai_sem

```c
ER sys_twai_sem(W apic, ID semid, TMO tmout)
```

**概要:** タイムアウト付きでセマフォ資源を獲得する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `semid` | `ID` | セマフォ ID |
| `tmout` | `TMO` | タイムアウト値 (TMO_FEVR: 永久待ち, TMO_POL: ポーリング) |

**戻り値:**

| 値 | 意味 |
|-----|------|
| `E_OK` | 正常終了 (資源獲得成功) |
| `E_ID` | 不正な ID |
| `E_NOEXS` | セマフォ未生成 |
| `E_TMOUT` | タイムアウトまたはポーリング失敗 |

**処理内容:**
1. ID・存在チェック
2. `semcnt == 0` (資源なし) の場合:
   - `TMO_POL` なら即座に `E_TMOUT`
   - `sematr == TA_TFIFO` なら `ins_fifo()` で FIFO 順に待ちキューに挿入
   - それ以外 (`TA_TPRI`) なら `ins_pri()` で優先度順に待ちキューに挿入
   - `TMO_FEVR` なら `sys_slp_tsk()` で永久待ち
   - タイムアウト指定なら `sys_tslp_tsk()` でタイムアウト付き待ち
   - 待ちから復帰後 `E_TMOUT` を返す
4. `semcnt > 0` の場合: `semcnt` をデクリメント

**呼び出し元:** `sys_wai_sem`、ユーザーコード (`twai_sem`)

**注意点:** 待ちから正常に起床された場合の戻り値は `sys_sig_sem` 内で `proc_set_return_value()` により pt_regs フレームの EAX スロットに `E_OK` が書き込まれるため、ここでの `E_TMOUT` は上書きされる。

---

### sys_ref_sem

```c
ER sys_ref_sem(W apic, ID semid, T_RSEM* pk_rsem)
```

**概要:** セマフォの状態を参照する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `apic` | `W` | APIC ID |
| `semid` | `ID` | セマフォ ID |
| `pk_rsem` | `T_RSEM*` | 参照情報の格納先 |

T_RSEM 構造体:

```c
typedef struct t_rsem {
    ID      wtskid;     /* 待ちキュー先頭のタスク ID */
    UINT    semcnt;     /* 現在のセマフォカウント */
} T_RSEM;
```

**戻り値:** `E_OK`、`E_ID`、`E_NOEXS`

**処理内容:**
1. 待ちキューが空なら `wtskid = TSK_NONE`、空でなければ先頭タスクの ID を設定
3. `semcnt` を設定

**呼び出し元:** ユーザーコード (`ref_sem`)

---

## 待ちキューヘルパー関数

以下の関数は `sys_sem.c` に実装されているが、`sys_sem.h` で宣言され、イベントフラグやデータキューなど他のカーネルオブジェクトからも使用される汎用関数。

### ins_fifo

```c
void ins_fifo(T_LINK* base, T_LINK* wlink)
```

**概要:** 双方向リンクリストの末尾に要素を挿入する (FIFO 順)。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `base` | `T_LINK*` | リストのヘッド (番兵ノード) |
| `wlink` | `T_LINK*` | 挿入する要素 |

**処理内容:**
1. リストの末尾 (`base->next == base_sav` になるまで走査) を見つける
2. 末尾の直後に `wlink` を挿入

**注意点:** 呼び出し元で既にスピンロックが取得されている前提。

---

### ins_pri

```c
void ins_pri(T_LINK* base, T_LINK* wlink)
```

**概要:** 双方向リンクリストに優先度順で要素を挿入する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `base` | `T_LINK*` | リストのヘッド (番兵ノード) |
| `wlink` | `T_LINK*` | 挿入する要素 |

**処理内容:**
1. `wlink2tsk()` で挿入タスクの優先度を取得
2. リストを先頭から走査し、自分より低い優先度 (数値が大きい) のタスクを見つける
3. その直前に `wlink` を挿入

**注意点:** 優先度の数値が小さいほど高優先度。同一優先度の場合は後ろに挿入される (FIFO 順)。

---

### wlink_ins

```c
void wlink_ins(T_LINK* base, T_LINK* wlink)
```

**概要:** リストの先頭 (base の直後) に要素を挿入する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `base` | `T_LINK*` | 挿入位置の前のノード |
| `wlink` | `T_LINK*` | 挿入する要素 |

**処理内容:** `base` と `base->next` の間に `wlink` を挿入。

---

### wlink_rem

```c
void wlink_rem(T_LINK* wlink)
```

**概要:** リストから要素を削除する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `wlink` | `T_LINK*` | 削除する要素 |

**処理内容:** `wlink` の前後のノードを直接接続して、`wlink` をリストから除外する。

---

### wlink_change

```c
void wlink_change(T_LINK* wlink, T_LINK* wlink2)
```

**概要:** 2 つのリスト要素のリンクを交換する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `wlink` | `T_LINK*` | 要素 1 |
| `wlink2` | `T_LINK*` | 要素 2 |

**処理内容:** 2 要素の `prev`/`next` ポインタを交換する。

**注意点:** 隣接する要素同士の交換ではリンク破壊の可能性がある。使用時は注意が必要。

---

### wlink_dump

```c
void wlink_dump(T_LINK* base)
```

**概要:** 待ちキューの内容をデバッグ出力する。

**引数:**

| 引数 | 型 | 説明 |
|------|-----|------|
| `base` | `T_LINK*` | リストのヘッド (番兵ノード) |

**処理内容:** リストを走査し、各ノードのアドレスを `printk` で出力する。

## 補足

### 排他制御

全てのセマフォ操作は Big Kernel Lock (`kernel_lk`) で保護されている。`kernel_lk` は呼び出し元 (`c_intr_syscall` 等) で取得済みであり、セマフォ関数内ではロック操作を行わない。待ちに入る際は内部で `sys_slp_tsk()` が呼ばれるが、`kernel_lk` を既に保持しているためロックのネストは発生しない。

### 待ちキューと起床メカニズム

セマフォで待ち状態に入る際は、タスクの `wlink` を待ちキューに挿入した後、`sys_slp_tsk()` / `sys_tslp_tsk()` を呼んでタスクをブロックする。起床は `sys_sig_sem()` 内で `proc_set_return_value()` により pt_regs フレームの EAX スロットにシステムコール戻り値を書き込むことで行われる。

### デモでの使用例

バイナリセマフォ (初期カウント 1, 最大カウント 1) を使い、CPU 0 のタスク 3 と CPU 1 のタスク 2 が `pol_sem` で `shared_count` への排他アクセスを行っている。
