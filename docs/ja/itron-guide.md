# Micro ITRON 4.0 ガイド — tiny-itron を読むために

本ドキュメントは、tiny-itron のソースコードで使われている
Micro ITRON 4.0 の概念と API を解説する。
ITRON 仕様は膨大だが、**このコードが実際に使う機能だけ** に絞っている。

> **注意:** tiny-itron は Micro ITRON 4.0 仕様の部分的な実装 (rough implementation) であり、
> 多くの API がスタブや簡略化された形で実装されている。
> 本ドキュメントの説明も tiny-itron の実装に即した簡略化を含んでおり、
> 仕様の正確な定義とは異なる場合がある。
> API の詳細な仕様・エラー条件・エッジケースについては、必ず一次資料を参照すること:
>
> - **µITRON4.0 Specification Ver. 4.00.00**
>   <https://libdrc.org/ITRON/SPEC/FILE/mitron-400e.pdf>


---

## 目次

1. [Micro ITRON とは](#1-micro-itron-とは)
2. [基本概念: タスクと状態遷移](#2-基本概念-タスクと状態遷移)
3. [タスク管理 API](#3-タスク管理-api)
4. [同期: セマフォ](#4-同期-セマフォ)
5. [通信: データキュー](#5-通信-データキュー)
6. [割り込みコンテキスト](#6-割り込みコンテキスト)
7. [syscall の仕組み](#7-syscall-の仕組み)
8. [型とエラーコード (リファレンス)](#8-型とエラーコード-リファレンス)
9. [このコードで使わない ITRON 機能](#9-このコードで使わない-itron-機能)

---

## 1. Micro ITRON とは

ITRON (Industrial TRON) は、1984 年に始まった TRON プロジェクトの一部で、
組み込みシステム向けのリアルタイム OS **仕様** である。

重要な特徴:

- **仕様だけで実装は各自**。Linux のように「ソースコードが一つ」ではなく、
  仕様書に従って各社・各個人が独自にカーネルを実装する。
  (tiny-itron は ITRON仕様の一部を実装したもの)
- **静的設計が中心**: µITRON4.0 には `acre_tsk` / `acre_sem` 等の動的生成 API もあるが、
  組み込み用途ではオブジェクト数を事前に決めてビルドするのが一般的。
  tiny-itron では `MAX_TSKID = 16` 等の固定上限を使う。
- **リアルタイム**: 「応答時間の上限が保証できる」ことを重視する。
  優先度ベースのプリエンプティブスケジューリングが基本。

tiny-itron が対象とするのは **Micro ITRON 4.0.0 仕様** (1999 年) である。
`include/config.h` にバージョン番号が記されている:

```c
#define TKERNEL_SPVER   0x5400    /* Micro ITRON v4.0.0 */
```

---

## 2. 基本概念: タスクと状態遷移

### タスクとは

ITRON における **タスク** は、実行の単位である。
UNIX のスレッドに近く、各タスクは固有のスタックと優先度を持つ。
µITRON4.0 仕様はアドレス空間やメモリ保護方式を規定しておらず、実装依存である。
tiny-itron ではすべてのタスクが同じアドレス空間を共有し、メモリ保護はない。

### タスク ID と優先度

- **タスク ID**: 1 〜 `MAX_TSKID` (16)。0 は無効値 (`TSK_NONE`)。
- **優先度**: 1 (最高) 〜 `TMAX_TPRI` (16、最低)。
  数値が **小さいほど高優先度** である。

### 状態遷移

ITRON はタスクの状態を厳密に定義している。
このコードで使う状態は以下の 5 つ:

```
                     cre_tsk
    [NON-EXISTENT] ─────────→ [DORMANT]
      (TTS_NON)                (TTS_DMT)
                                  │
                              act_tsk
                                  │
                                  ▼
                         ┌──────────────────────┐
                         │                      │
                         ▼                      │
                      [READY] ←──────┐          │
                      (TTS_RDY)      │          │
                         │           │          │ wup_tsk
                     スケジューラ   wup_tsk      │ sig_sem
                       が選択     sig_sem      │ snd_dtq
                         │        snd_dtq      │ rel_wai
                         ▼        rel_wai      │
                      [RUNNING] ─────┘          │
                      (TTS_RUN)  プリエンプション │
                         │                      │
                      slp_tsk                   │
                      wai_sem                   │
                      rcv_dtq                   │
                         │                      │
                         ▼                      │
                      [WAITING] ───────────────┘
                      (TTS_WAI)   条件充足時
```

各状態の意味:

| 定数 | 値 | 意味 |
|------|------|------|
| `TTS_NON` | 0x00 | 未生成 (タスクが存在しない) |
| `TTS_DMT` | 0x10 | 休止状態 (生成済みだが未起動) |
| `TTS_RDY` | 0x02 | 実行可能状態 (CPU 待ち) |
| `TTS_RUN` | 0x01 | 実行状態 (CPU で実行中) |
| `TTS_WAI` | 0x04 | 待ち状態 (イベント待ち) |

仕様には `TTS_SUS` (強制待ち) や `TTS_WAS` (二重待ち) もあるが、
このコードのデモでは使用していない。

---

## 3. タスク管理 API

### cre_tsk — タスクの生成

```c
ER cre_tsk(ID tskid, T_CTSK *pk_ctsk);
```

タスク ID `tskid` に新しいタスクを生成する。生成後は **DORMANT** 状態。
パラメータは `T_CTSK` 構造体で渡す:

```c
typedef struct t_ctsk {
    ATR     tskatr;     /* タスク属性 (通常 0) */
    VP_INT  exinf;      /* 拡張情報 (タスク関数の引数) */
    FP      task;       /* タスク開始アドレス (関数ポインタ) */
    PRI     itskpri;    /* 初期優先度 */
    SIZE    stksz;      /* スタックサイズ (バイト) */
    VP      stk;        /* スタック領域へのポインタ */
} T_CTSK;
```

### act_tsk — タスクの起動

```c
ER act_tsk(ID tskid);
```

DORMANT 状態のタスクを **READY** 状態に遷移させる。
すでに READY/RUNNING のタスクに対して呼ぶと、起動要求がキューイングされる
(`actcnt` がインクリメントされる)。

### slp_tsk / wup_tsk — 待ちと起床

```c
ER slp_tsk(void);      /* 自タスクを WAITING にする */
ER wup_tsk(ID tskid);  /* 指定タスクを起床する */
```

最もシンプルなタスク間同期。`slp_tsk` で自分を眠らせ、
他のタスクが `wup_tsk` で起こす。

### 実際のコード例: Task 1 と Task 3 の交互実行

`kernel/user.c` では Task 1 と Task 3 が `wup_tsk` / `slp_tsk` で
交互に実行される:

```c
/* first_task (Task 1) — kernel/user.c */
void first_task(void)
{
    /* ... セマフォ・Task 3 生成 ... */

    while (1) {
        task_count[1]++;
        /* ... 画面表示 ... */
        delay();
        wup_tsk(3);     /* Task 3 を起こす */
        slp_tsk();       /* 自分は眠る */
    }
}

/* usr_main (Task 3) — kernel/user.c */
void usr_main(VP_INT arg)
{
    slp_tsk();           /* 最初の起床を待つ */

    while (1) {
        task_count[3]++;
        /* ... 処理 ... */
        delay();
        wup_tsk(1);     /* Task 1 を起こす */
        slp_tsk();       /* 自分は眠る */
    }
}
```

この「起こして眠る」パターンで、2 つのタスクが交互に 1 回ずつ動作する。

### タスク生成の実例

Task 1 がセマフォ、メッセージバッファ、Task 3 を生成するコード:

```c
/* first_task の冒頭 — kernel/user.c */
T_CTSK ctsk;

ctsk.task    = (FP)usr_main;        /* Task 3 のエントリ関数 */
ctsk.stk     = tsk_stack_alloc(1024); /* 1024 バイトのスタック (syscall ラッパー) */
ctsk.stksz   = 1024;
ctsk.itskpri = TMAX_TPRI - 1;      /* 優先度 15 */
ctsk.exinf   = 3;                   /* 拡張情報 (タスク ID) */
cre_tsk(3, &ctsk);                  /* ID=3 で生成 */
act_tsk(3);                         /* 起動 */
```

---

## 4. 同期: セマフォ

### セマフォとは

セマフォはカウンタ付きの同期オブジェクトである。
「資源の数」をカウンタで管理し、資源がなければタスクを待たせる。

カウンタが 0 か 1 しか取らない場合を **バイナリセマフォ** と呼び、
排他制御 (ミューテックス) に使える。

### cre_sem — セマフォの生成

```c
ER cre_sem(ID semid, T_CSEM *pk_csem);
```

パラメータ構造体:

```c
typedef struct t_csem {
    ATR     sematr;     /* 属性 (TA_TFIFO: 待ち行列は FIFO) */
    UINT    isemcnt;    /* 初期カウント */
    UINT    maxsem;     /* 最大カウント */
} T_CSEM;
```

### sig_sem / pol_sem — シグナルとポーリング獲得

```c
ER sig_sem(ID semid);   /* カウント +1 (待ちタスクがあれば起床) */
ER pol_sem(ID semid);   /* カウント -1 (0 なら即座に E_TMOUT) */
```

`pol_sem` は **非ブロッキング** (ポーリング) 版である。
カウントが 0 の場合、タスクをブロックせず `E_TMOUT` を返す。

仕様には `wai_sem` (ブロッキング版) もある。カウントが 0 なら
タスクを WAITING にして、他のタスクが `sig_sem` するまで待つ。
このデモでは `pol_sem` のみ使用している。

### 実際のコード例: CPU 間のセマフォ競合

Task 2 (CPU 1) と Task 3 (CPU 0) がバイナリセマフォ 1 で
`shared_count` を保護する:

```c
/* セマフォ生成 (first_task) — kernel/user.c */
T_CSEM csem;
csem.sematr  = TA_TFIFO;
csem.isemcnt = 1;       /* 初期値 1 (1 タスクだけ獲得可) */
csem.maxsem  = 1;       /* バイナリセマフォ */
cre_sem(1, &csem);
```

```c
/* Task 3 のセマフォ獲得ループ — kernel/user.c */
if (pol_sem(1) == E_OK) {       /* 獲得成功 (LOCK) */
    shared_count++;
    /* ... SEM_HOLD 回繰り返し ... */
    sig_sem(1);                  /* 解放 */
} else {
    /* E_TMOUT: 他 CPU がロック中 (BUSY) */
}
```

VGA 画面には `LOCK` (獲得中)、`BUSY` (競合中)、`----` (休止中) が表示され、
2 つの CPU がセマフォで排他していることが視覚的にわかる。

---

## 5. 通信: データキュー

### データキューとは

データキュー (data queue) は、整数サイズのデータを FIFO で送受信する
メッセージキューである。内部はリングバッファで実装されている。

セマフォが「はい/いいえ」の同期信号だけなのに対し、
データキューは **値を伴うメッセージ** を送れる。

### cre_dtq — データキューの生成

```c
ER cre_dtq(ID dtqid, T_CDTQ *pk_cdtq);
```

パラメータ構造体:

```c
typedef struct t_cdtq {
    ATR     dtqatr;     /* 属性 (TA_TFIFO) */
    UINT    dtqcnt;     /* キューの要素数 */
    VP      dtq;        /* バッファアドレス (NULL ならカーネルが確保) */
} T_CDTQ;
```

### psnd_dtq / prcv_dtq — ポーリング送受信

```c
ER psnd_dtq(ID dtqid, VP_INT data);       /* 送信 (満杯なら E_TMOUT) */
ER prcv_dtq(ID dtqid, VP_INT *p_data);    /* 受信 (空なら E_TMOUT) */
```

いずれも **非ブロッキング** 版。仕様には `snd_dtq` / `rcv_dtq`
(ブロッキング版) もある。このデモでは kbd_task (Task 4) が DTQ 2 の
ブロッキング受信 (`rcv_dtq`) を使い、ISR が `ipsnd_dtq` で文字を送信する。
なお Task 4 → Task 1 の転送にはデータキューではなくメッセージバッファ (MBF) を使用する。

### 実際のコード例: ISR → kbd_task のキー文字送信

キーボード ISR (CPU 0) が受け取ったキー文字を
kbd_task (Task 4, CPU 1) にデータキューで送る:

```c
/* データキュー生成 (second_task) — kernel/user.c */
T_CDTQ cdtq;
cdtq.dtqatr = TA_TFIFO;
cdtq.dtqcnt = 16;       /* 16 要素分のバッファ */
cdtq.dtq    = 0;         /* NULL → カーネルが確保 */
cre_dtq(2, &cdtq);
```

```c
/* key_intr (ISR) — 送信側 */
ipsnd_dtq(0, key_dtq_id, ch);   /* 文字 ch を DTQ 2 に送る */
```

```c
/* kbd_task (Task 4) — 受信側 */
VP_INT data;
rcv_dtq(2, &data);              /* DTQ 2 をブロッキング受信 */
c = (int)data;
```

`VP_INT` は `int` の typedef なので、文字コードをそのまま整数として渡せる。

kbd_task → first_task の転送にはメッセージバッファ (MBF) を使用する。
詳細は [keyboard.md](keyboard.md) セクション 7, 9 を参照。

---

## 6. 割り込みコンテキスト

### タスクコンテキスト vs 非タスクコンテキスト

µITRON4.0 は 2 つの実行コンテキストを区別する:

| | タスクコンテキスト | 非タスクコンテキスト |
|---|---|---|
| 誰が実行 | 通常のタスク | 割り込みハンドラ、周期/アラーム/CPU 例外ハンドラ等 |
| 待ちに入れる | はい (`slp_tsk` 等) | いいえ |
| 使える API | すべて | `i` 付き版のみ |

非タスクコンテキストは「いま何かの仕事を中断して呼ばれている」状態なので、
自分自身が `slp_tsk` で眠ることはできない。
tiny-itron で実際に使われる非タスクコンテキストは割り込みハンドラ (ISR) のみ。

### iwup_tsk — 割り込みハンドラからタスクを起こす

```c
ER iwup_tsk(W apic, ID tskid);
```

`wup_tsk` の非タスクコンテキスト版。
**`i` で始まる API** は非タスクコンテキスト用という命名規則がある。
tiny-itron では割り込みハンドラ (ISR) から呼ばれる。

他にも `iact_tsk`、`isig_sem`、`ipsnd_dtq` 等がある。

### 実際のコード例: キーボード ISR

キーボード割り込み (IRQ 1) の全体の流れ:

```
1. キーが押される
2. IRQ1 → PIC → CPU 0 に割り込み配送
3. CPU 0 の ISR (key_intr) が呼ばれる
4. key_intr がスキャンコードを読み、ASCII に変換
5. ipsnd_dtq(0, key_dtq_id, ch) で DTQ 2 に文字を送信
6. DTQ 2 で rcv_dtq 待ちの Task 4 が起床 (TTS_WAI → TTS_RDY)
7. 割り込みから復帰
8. CPU 1 の次の APIC タイマー割り込みで
   スケジューラが Task 4 (最高優先度) に切り替え
9. Task 4 の rcv_dtq(2) が文字を返す
```

ISR のコード (`i386/keyboard.c`):

```c
int key_intr(void)
{
    /* ... スキャンコード → ASCII 変換 ... */

    /* DTQ 経由でキーボードタスクに文字を送信 */
    if (key_dtq_id > 0)
        ipsnd_dtq(0, key_dtq_id, ch);

    return 0;
}
```

IRQ 1 は PIC 経由で CPU 0 に配送されるが、Task 4 は CPU 1 で動く。
`ipsnd_dtq` 内部の `sched_next_tsk` が両 CPU のリスケジューリングフラグをセットするため、
CPU 1 の次のタイマー割り込みで Task 4 が選択される。

---

## 7. syscall の仕組み

ユーザータスクは Ring 3 (非特権) で動作し、カーネルは Ring 0 (特権) で
動作する。ユーザーが ITRON API を呼ぶとき、特権レベルの壁を越える
必要がある。この仕組みを **syscall** (システムコール) と呼ぶ。

### 全体の流れ

```
ユーザータスク (Ring 3)              カーネル (Ring 0)
  │                                    │
  │  cre_tsk(3, &ctsk)                │
  │    ↓                              │
  │  lib/lib_tsk.c                    │
  │    syscall(-TFN_CRE_TSK,          │
  │            3, &ctsk)              │
  │    ↓                              │
  │  klib.s: syscall                  │
  │    int $0x99                      │
  │    ─────────────────────────→     │
  │                              intr.s: intr_syscall
  │                                SAVE_ALL (9 レジスタを per-task
  │                                  カーネルスタックに push → pt_regs 構築)
  │                                intr_enter (k_nest++, CPU 判定)
  │                                    ↓
  │                              i386/syscall.c: c_intr_syscall(pt_regs*)
  │                                    ↓
  │                              kernel/syscall.c: itron_syscall
  │                                    ↓
  │                              syscallP.h のテーブルで
  │                              関数コードからハンドラを検索
  │                                    ↓
  │                              kernel/sys_tsk.c: sys_cre_tsk
  │                                    ↓
  │                              regs->eax に戻り値を書き込み
  │                                    ↓
  │                              intr_leave (k_nest--, タスクスイッチ判定)
  │                              RESTORE_ALL (pt_regs から 9 レジスタを pop)
  │    ←─────────────────────────     │
  │  iret (EAX=戻り値で Ring 3 に復帰) │
```

### 関数コード (TFN_xxx)

各 API に一意の **関数コード** が割り当てられている。
定義は `include/itron.h` にある:

```c
#define TFN_CRE_TSK    -0x05
#define TFN_ACT_TSK    -0x07
#define TFN_SLP_TSK    -0x11
#define TFN_WUP_TSK    -0x13
#define TFN_CRE_SEM    -0x21
#define TFN_POL_SEM    -0x26
#define TFN_SIG_SEM    -0x23
#define TFN_CRE_DTQ    -0x31
#define TFN_PSND_DTQ   -0x36
#define TFN_PRCV_DTQ   -0x3a
#define TFN_IWUP_TSK   -0x72
```

### ライブラリラッパー (lib/)

`lib/lib_tsk.c` や `lib/lib_sem.c` が薄いラッパーになっている:

```c
/* lib/lib_tsk.c */
ER cre_tsk(ID tskid, T_CTSK *pk_ctsk)
{
    return syscall(-TFN_CRE_TSK, tskid, pk_ctsk);
}

ER slp_tsk(void)
{
    return syscall(-TFN_SLP_TSK);
}
```

`syscall()` は `i386/klib.s` のアセンブリルーチンで、
引数をスタックに積んだまま `int $0x99` を発行する。

### ディスパッチテーブル

`kernel/syscallP.h` に関数コードからカーネル関数へのマッピングテーブルがある。
`itron_syscall()` がこのテーブルを引いて対応するハンドラを呼び出す。

---

## 8. 型とエラーコード (リファレンス)

### 主要な型 (`include/itron.h`)

| 型 | C での定義 | 意味 |
|----|-----------|------|
| `ER` | `int` | エラーコード (戻り値) |
| `ID` | `int` | オブジェクト ID |
| `FP` | `void (*)()` | 関数ポインタ |
| `PRI` | `int` | 優先度 |
| `ATR` | `unsigned int` | オブジェクト属性 |
| `STAT` | `unsigned int` | オブジェクト状態 |
| `TMO` | `unsigned int` | タイムアウト値 |
| `RELTIM` | `unsigned int` | 相対時間 |
| `VP_INT` | `int` | ポインタまたは整数 |
| `SIZE` | `unsigned int` | メモリサイズ |
| `VP` | `char *` | 汎用ポインタ |

### 主要なエラーコード

| 定数 | 値 | 意味 |
|------|------|------|
| `E_OK` | 0 | 正常終了 |
| `E_SYS` | -5 | システムエラー |
| `E_PAR` | -17 | パラメータエラー |
| `E_ID` | -18 | 不正な ID 番号 |
| `E_OBJ` | -41 | オブジェクト状態エラー |
| `E_NOEXS` | -42 | オブジェクト未生成 |
| `E_QOVR` | -43 | キューイングオーバーフロー |
| `E_TMOUT` | -50 | タイムアウト / ポーリング失敗 |
| `E_DLT` | -51 | 待ちオブジェクトが削除された |

`pol_sem` や `prcv_dtq` 等のポーリング版 API が「資源なし」を返すとき `E_TMOUT` になる。
名前が紛らわしいが、ITRON 仕様ではポーリング失敗もタイムアウトも
同じエラーコードを使う。

### 属性定数

| 定数 | 値 | 意味 |
|------|------|------|
| `TA_TFIFO` | 0x00 | 待ち行列を FIFO 順にする |
| `TA_TPRI` | 0x01 | 待ち行列を優先度順にする |

### タイムアウト定数

| 定数 | 値 | 意味 |
|------|------|------|
| `TMO_POL` | 0 | ポーリング (即座に返る) |
| `TMO_FEVR` | -1 | 永久に待つ |

---

## 9. このコードで使わない ITRON 機能

Micro ITRON 4.0 仕様には多数の機能があるが、`kernel/user.c` のデモで
使っているのはごく一部である。以下は **実装コードは `kernel/` にあるが、
デモからは呼ばれていない** 機能の一覧:

| カテゴリ | API 例 | 説明 |
|----------|--------|------|
| イベントフラグ | `cre_flg`, `set_flg`, `wai_flg` | ビットパターンによる同期 |
| メールボックス | `cre_mbx`, `snd_mbx`, `rcv_mbx` | メッセージポインタの送受信 |
| ミューテックス | `cre_mtx`, `loc_mtx`, `unl_mtx` | 優先度逆転防止付き排他 |
| メッセージバッファ | `cre_mbf`, `snd_mbf`, `rcv_mbf` | 可変長メッセージ (MBF 1 で使用中) |
| ランデブ | `cre_por`, `cal_por`, `acp_por` | 同期型メッセージ交換 |
| 固定長メモリプール | `cre_mpf`, `get_mpf`, `rel_mpf` | 固定サイズのメモリ管理 |
| 可変長メモリプール | `cre_mpl`, `get_mpl`, `rel_mpl` | 可変サイズのメモリ管理 |
| 周期ハンドラ | `cre_cyc`, `sta_cyc`, `stp_cyc` | 周期的に呼ばれる関数 |
| アラームハンドラ | `cre_alm`, `sta_alm`, `stp_alm` | 指定時刻に呼ばれる関数 |
| タスク例外 | `def_tex`, `ras_tex` | タスクへの非同期例外通知 |
| オーバーラン | `def_ovr`, `sta_ovr` | CPU 時間の監視 |

これらの機能に興味がある場合は、`kernel/` 以下の対応するソースファイル
(例: `kernel/sys_flg.c`, `kernel/sys_mbx.c`, `kernel/sys_mtx.c`) を参照のこと。
