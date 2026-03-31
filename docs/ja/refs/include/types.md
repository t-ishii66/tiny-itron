# include/types.h - ITRON 標準データ構造体定義

## 概要

Micro ITRON v4.0.0 仕様のサービスコールで使用するパケット (データ構造体) を定義する。
`itron.h` の末尾から `#include` される。

全 38 構造体と、i386 向けバイトオーダー変換マクロを定義する。

## タスク管理

### T_CTSK - タスク生成情報パケット

`cre_tsk` / `acre_tsk` に渡すタスク生成情報。

```c
typedef struct t_ctsk {
    ATR     tskatr;     /* タスク属性 */
    VP_INT  exinf;      /* タスク拡張情報 */
    FP      task;       /* タスク起動アドレス */
    PRI     itskpri;    /* タスク初期優先度 */
    SIZE    stksz;      /* タスクスタックサイズ */
    VP      stk;        /* タスクスタックポインタ */
} T_CTSK;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `tskatr` | `ATR` | タスク属性 (TA_HLNG, TA_ACT 等) |
| `exinf` | `VP_INT` | タスク拡張情報 (ユーザー任意) |
| `task` | `FP` | タスク開始関数アドレス |
| `itskpri` | `PRI` | 初期優先度 (1=最高, 16=最低) |
| `stksz` | `SIZE` | スタックサイズ (バイト) |
| `stk` | `VP` | スタック領域ポインタ (NULL で自動割り当て) |

### T_RTSK - タスク状態参照パケット

`ref_tsk` で返されるタスク状態情報。

```c
typedef struct t_rtsk {
    STAT    tskstat;    /* タスク状態 */
    PRI     tskpri;     /* タスク現在優先度 */
    PRI     tskbpri;    /* タスクベース優先度 */
    STAT    tskwait;    /* 待ち要因 */
    ID      wobjid;     /* 待ちオブジェクト ID */
    TMO     lefttmo;    /* タイムアウト残り時間 */
    UINT    actcnt;     /* 起動要求キューイング数 */
    UINT    wupcnt;     /* 起床要求キューイング数 */
    UINT    suscnt;     /* 強制待ちネスト数 */
} T_RTSK;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `tskstat` | `STAT` | タスク状態 (TTS_RUN, TTS_RDY 等) |
| `tskpri` | `PRI` | 現在の優先度 (ミューテックス等で変化) |
| `tskbpri` | `PRI` | ベース優先度 (chg_pri で設定) |
| `tskwait` | `STAT` | 待ち要因 (TTW_SLP, TTW_SEM 等) |
| `wobjid` | `ID` | 待ち対象オブジェクトの ID |
| `lefttmo` | `TMO` | タイムアウト残り時間 |
| `actcnt` | `UINT` | 起動要求キューイング数 |
| `wupcnt` | `UINT` | 起床要求キューイング数 |
| `suscnt` | `UINT` | 強制待ちネスト数 |

### T_RTST - タスク状態参照パケット (簡易)

`ref_tst` で返される簡易タスク状態情報。

```c
typedef struct t_rtst {
    STAT    tskstat;    /* タスク状態 */
    STAT    tskwait;    /* 待ち要因 */
} T_RTST;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `tskstat` | `STAT` | タスク状態 |
| `tskwait` | `STAT` | 待ち要因 |

## タスク例外処理

### T_DTEX - タスク例外処理ルーチン定義パケット

`def_tex` に渡すタスク例外処理ルーチン定義情報。

```c
typedef struct t_dtex {
    ATR     texatr;     /* タスク例外処理ルーチン属性 */
    FP      texrtn;     /* タスク例外処理ルーチン開始アドレス */
    VP_INT  exinf;      /* 拡張情報 */
} T_DTEX;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `texatr` | `ATR` | タスク例外処理ルーチン属性 |
| `texrtn` | `FP` | タスク例外処理ルーチン開始アドレス |
| `exinf` | `VP_INT` | 拡張情報 |

### T_RTEX - タスク例外処理状態参照パケット

`ref_tex` で返されるタスク例外処理状態情報。

```c
typedef struct t_rtex {
    STAT    texstat;    /* タスク例外処理状態 */
    TEXPTN  pndptn;     /* 保留中の例外要因 */
} T_RTEX;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `texstat` | `STAT` | タスク例外処理状態 (TTEX_ENA/TTEX_DIS) |
| `pndptn` | `TEXPTN` | 保留中の例外要因ビットパターン |

## セマフォ

### T_CSEM - セマフォ生成情報パケット

`cre_sem` / `acre_sem` に渡すセマフォ生成情報。

```c
typedef struct t_csem {
    ATR     sematr;     /* セマフォ属性 */
    UINT    isemcnt;    /* セマフォ初期資源数 */
    UINT    maxsem;     /* セマフォ最大資源数 */
} T_CSEM;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `sematr` | `ATR` | セマフォ属性 (TA_TFIFO/TA_TPRI) |
| `isemcnt` | `UINT` | 初期資源数 |
| `maxsem` | `UINT` | 最大資源数 (上限: TMAX_MAXSEM=65535) |

### T_RSEM - セマフォ状態参照パケット

`ref_sem` で返されるセマフォ状態情報。

```c
typedef struct t_rsem {
    ID      wtskid;     /* 待ちタスク ID (待ちキュー先頭) */
    UINT    semcnt;     /* 現在の資源数 */
} T_RSEM;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `wtskid` | `ID` | 待ちキュー先頭のタスク ID (TSK_NONE=待ちなし) |
| `semcnt` | `UINT` | 現在のセマフォ資源数 |

## イベントフラグ

### T_CFLG - イベントフラグ生成情報パケット

`cre_flg` / `acre_flg` に渡すイベントフラグ生成情報。

```c
typedef struct t_cflg {
    ATR     flgatr;     /* イベントフラグ属性 */
    FLGPTN  iflgptn;    /* イベントフラグ初期ビットパターン */
} T_CFLG;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `flgatr` | `ATR` | イベントフラグ属性 (TA_WSGL/TA_WMUL, TA_CLR) |
| `iflgptn` | `FLGPTN` | 初期ビットパターン (32 ビット) |

### T_RFLG - イベントフラグ状態参照パケット

`ref_flg` で返されるイベントフラグ状態情報。

```c
typedef struct t_rflg {
    ID      wtskid;     /* 待ちタスク ID (待ちキュー先頭) */
    FLGPTN  flgptn;     /* 現在のビットパターン */
} T_RFLG;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `wtskid` | `ID` | 待ちキュー先頭のタスク ID |
| `flgptn` | `FLGPTN` | 現在のビットパターン |

## データキュー

### T_CDTQ - データキュー生成情報パケット

`cre_dtq` / `acre_dtq` に渡すデータキュー生成情報。

```c
typedef struct t_cdtq {
    ATR     dtqatr;     /* データキュー属性 */
    UINT    dtqcnt;     /* データ数 */
    VP      dtq;        /* データキュー領域先頭アドレス */
} T_CDTQ;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `dtqatr` | `ATR` | データキュー属性 (TA_TFIFO/TA_TPRI) |
| `dtqcnt` | `UINT` | データキュー容量 (データ数) |
| `dtq` | `VP` | データキュー領域先頭アドレス |

### T_RDTQ - データキュー状態参照パケット

`ref_dtq` で返されるデータキュー状態情報。

```c
typedef struct t_rdtq {
    ID      stskid;     /* 送信待ちタスク ID (待ちキュー先頭) */
    ID      rtskid;     /* 受信待ちタスク ID (待ちキュー先頭) */
    UINT    sdtqcnt;    /* データキュー内のデータ数 */
} T_RDTQ;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `stskid` | `ID` | 送信待ちキュー先頭のタスク ID |
| `rtskid` | `ID` | 受信待ちキュー先頭のタスク ID |
| `sdtqcnt` | `UINT` | キュー内のデータ数 |

## メールボックス

### T_CMBX - メールボックス生成情報パケット

`cre_mbx` / `acre_mbx` に渡すメールボックス生成情報。

```c
typedef struct t_cmbx {
    ATR     mbxatr;     /* メールボックス属性 */
    PRI     maxmpri;    /* メッセージ最大優先度 */
    VP      mprihd;     /* メッセージキュー先頭アドレス */
} T_CMBX;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `mbxatr` | `ATR` | メールボックス属性 (TA_TFIFO/TA_TPRI, TA_MFIFO/TA_MPRI) |
| `maxmpri` | `PRI` | メッセージ最大優先度 |
| `mprihd` | `VP` | メッセージキュー先頭アドレス |

### T_RMBX - メールボックス状態参照パケット

`ref_mbx` で返されるメールボックス状態情報。

```c
typedef struct t_rmbx {
    ID      wtskid;     /* 待ちタスク ID (待ちキュー先頭) */
    T_MSG*  pk_msg;     /* メッセージパケット先頭アドレス */
} T_RMBX;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `wtskid` | `ID` | 待ちキュー先頭のタスク ID |
| `pk_msg` | `T_MSG*` | メッセージキュー先頭のメッセージパケット |

## ミューテックス

### T_CMTX - ミューテックス生成情報パケット

`cre_mtx` / `acre_mtx` に渡すミューテックス生成情報。

```c
typedef struct t_cmtx {
    ATR     mtxatr;     /* ミューテックス属性 */
    PRI     ceilpri;    /* ミューテックス上限優先度 */
} T_CMTX;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `mtxatr` | `ATR` | ミューテックス属性 (TA_INHERIT/TA_CEILING) |
| `ceilpri` | `PRI` | 上限優先度 (TA_CEILING 使用時) |

### T_RMTX - ミューテックス状態参照パケット

`ref_mtx` で返されるミューテックス状態情報。

```c
typedef struct t_rmtx {
    ID      htskid;     /* ロック保持タスク ID */
    ID      wtskid;     /* 待ちタスク ID (待ちキュー先頭) */
} T_RMTX;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `htskid` | `ID` | ミューテックスをロック中のタスク ID |
| `wtskid` | `ID` | 待ちキュー先頭のタスク ID |

## メッセージバッファ

### T_CMBF - メッセージバッファ生成情報パケット

`cre_mbf` / `acre_mbf` に渡すメッセージバッファ生成情報。

```c
typedef struct t_cmbf {
    ATR     mbfatr;     /* メッセージバッファ属性 */
    UINT    maxmsz;     /* メッセージ最大サイズ */
    SIZE    mbfsz;      /* メッセージバッファサイズ (バイト) */
    VP      mbf;        /* メッセージバッファ領域先頭アドレス */
} T_CMBF;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `mbfatr` | `ATR` | メッセージバッファ属性 |
| `maxmsz` | `UINT` | メッセージ最大サイズ |
| `mbfsz` | `SIZE` | バッファ全体サイズ (バイト) |
| `mbf` | `VP` | バッファ領域先頭アドレス |

### T_RMBF - メッセージバッファ状態参照パケット

`ref_mbf` で返されるメッセージバッファ状態情報。

```c
typedef struct t_rmbf {
    ID      stskid;     /* 送信待ちタスク ID (待ちキュー先頭) */
    ID      rtskid;     /* 受信待ちタスク ID (待ちキュー先頭) */
    UINT    smsgcnt;    /* バッファ内のメッセージ数 */
    SIZE    fmbfsz;     /* 空き領域サイズ */
} T_RMBF;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `stskid` | `ID` | 送信待ちキュー先頭のタスク ID |
| `rtskid` | `ID` | 受信待ちキュー先頭のタスク ID |
| `smsgcnt` | `UINT` | バッファ内のメッセージ数 |
| `fmbfsz` | `SIZE` | 空き領域サイズ (バイト) |

## ランデブポート

### T_CPOR - ランデブポート生成情報パケット

`cre_por` / `acre_por` に渡すランデブポート生成情報。

```c
typedef struct t_cpor {
    ATR     poratr;     /* ランデブポート属性 */
    UINT    maxcmsz;    /* 呼出しメッセージ最大サイズ */
    UINT    maxrmsz;    /* 返答メッセージ最大サイズ */
} T_CPOR;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `poratr` | `ATR` | ランデブポート属性 |
| `maxcmsz` | `UINT` | 呼出しメッセージ最大サイズ |
| `maxrmsz` | `UINT` | 返答メッセージ最大サイズ |

### T_RPOR - ランデブポート状態参照パケット

`ref_por` で返されるランデブポート状態情報。

```c
typedef struct t_rpor {
    ID      ctskid;     /* 呼出し待ちタスク ID (待ちキュー先頭) */
    ID      atskid;     /* 受付待ちタスク ID (待ちキュー先頭) */
} T_RPOR;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `ctskid` | `ID` | 呼出し待ちキュー先頭のタスク ID |
| `atskid` | `ID` | 受付待ちキュー先頭のタスク ID |

### T_RRDV - ランデブ状態参照パケット

`ref_rdv` で返されるランデブ状態情報。

```c
typedef struct t_rrdv {
    ID      wtskid;     /* ランデブ終了待ちタスク ID */
} T_RRDV;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `wtskid` | `ID` | ランデブ終了待ちのタスク ID |

## 固定長メモリプール

### T_CMPF - 固定長メモリプール生成情報パケット

`cre_mpf` / `acre_mpf` に渡す固定長メモリプール生成情報。

```c
typedef struct t_cmpf {
    ATR     mpfatr;     /* 固定長メモリプール属性 */
    UINT    blkcnt;     /* メモリブロック数 */
    UINT    blksz;      /* メモリブロックサイズ */
    VP      mpf;        /* メモリプール領域先頭アドレス */
} T_CMPF;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `mpfatr` | `ATR` | 固定長メモリプール属性 |
| `blkcnt` | `UINT` | メモリブロック数 |
| `blksz` | `UINT` | 個々のメモリブロックサイズ (バイト) |
| `mpf` | `VP` | メモリプール領域先頭アドレス |

### T_RMPF - 固定長メモリプール状態参照パケット

`ref_mpf` で返される固定長メモリプール状態情報。

```c
typedef struct t_rmpf {
    ID      wtskid;     /* 待ちタスク ID (待ちキュー先頭) */
    UINT    fblkcnt;    /* 空きメモリブロック数 */
} T_RMPF;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `wtskid` | `ID` | 待ちキュー先頭のタスク ID |
| `fblkcnt` | `UINT` | 空きメモリブロック数 |

## 可変長メモリプール

### T_CMPL - 可変長メモリプール生成情報パケット

`cre_mpl` / `acre_mpl` に渡す可変長メモリプール生成情報。

```c
typedef struct t_cmpl {
    ATR     mplatr;     /* 可変長メモリプール属性 */
    SIZE    mplsz;      /* メモリプールサイズ */
    VP      mpl;        /* メモリプール領域先頭アドレス */
} T_CMPL;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `mplatr` | `ATR` | 可変長メモリプール属性 |
| `mplsz` | `SIZE` | メモリプールサイズ (バイト) |
| `mpl` | `VP` | メモリプール領域先頭アドレス |

### T_RMPL - 可変長メモリプール状態参照パケット

`ref_mpl` で返される可変長メモリプール状態情報。

```c
typedef struct t_rmpl {
    ID      wtskid;     /* 待ちタスク ID (待ちキュー先頭) */
    SIZE    fmplsz;     /* 空き領域合計サイズ */
    UINT    fblksz;     /* 現在取得可能な最大ブロックサイズ */
} T_RMPL;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `wtskid` | `ID` | 待ちキュー先頭のタスク ID |
| `fmplsz` | `SIZE` | 空き領域の合計サイズ (バイト) |
| `fblksz` | `UINT` | 現在取得可能な最大ブロックサイズ |

## 周期ハンドラ

### T_CCYC - 周期ハンドラ生成情報パケット

`cre_cyc` / `acre_cyc` に渡す周期ハンドラ生成情報。

```c
typedef struct t_ccyc {
    ATR     cycatr;     /* 周期ハンドラ属性 */
    VP_INT  exinf;      /* 周期ハンドラ拡張情報 */
    FP      cychdr;     /* 周期ハンドラ開始アドレス */
    RELTIM  cyctim;     /* 周期ハンドラ起動周期 */
    RELTIM  cycphs;     /* 周期ハンドラ起動位相 */
} T_CCYC;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `cycatr` | `ATR` | 周期ハンドラ属性 (TA_STA, TA_PHS) |
| `exinf` | `VP_INT` | 拡張情報 |
| `cychdr` | `FP` | 周期ハンドラ関数アドレス |
| `cyctim` | `RELTIM` | 起動周期 |
| `cycphs` | `RELTIM` | 起動位相 |

### T_RCYC - 周期ハンドラ状態参照パケット

`ref_cyc` で返される周期ハンドラ状態情報。

```c
typedef struct t_rcyc {
    STAT    cycstat;    /* 周期ハンドラ状態 */
    RELTIM  lefttim;    /* 次回起動までの残り時間 */
} T_RCYC;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `cycstat` | `STAT` | 周期ハンドラ状態 (TCYC_STP/TCYC_STA) |
| `lefttim` | `RELTIM` | 次回起動までの残り時間 |

## アラームハンドラ

### T_CALM - アラームハンドラ生成情報パケット

`cre_alm` / `acre_alm` に渡すアラームハンドラ生成情報。

```c
typedef struct t_calm {
    ATR     almatr;     /* アラームハンドラ属性 */
    VP_INT  exinf;      /* アラームハンドラ拡張情報 */
    FP      almhdr;     /* アラームハンドラ開始アドレス */
} T_CALM;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `almatr` | `ATR` | アラームハンドラ属性 |
| `exinf` | `VP_INT` | 拡張情報 |
| `almhdr` | `FP` | アラームハンドラ関数アドレス |

### T_RALM - アラームハンドラ状態参照パケット

`ref_alm` で返されるアラームハンドラ状態情報。

```c
typedef struct t_ralm {
    STAT    almstat;    /* アラームハンドラ状態 */
    RELTIM  lefttim;    /* 起動までの残り時間 */
} T_RALM;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `almstat` | `STAT` | アラームハンドラ状態 (TALM_STP/TALM_STA) |
| `lefttim` | `RELTIM` | 起動までの残り時間 |

## オーバーランハンドラ

### T_DOVR - オーバーランハンドラ定義パケット

`def_ovr` に渡すオーバーランハンドラ定義情報。

```c
typedef struct t_dovr {
    ATR     ovratr;     /* オーバーランハンドラ属性 */
    FP      ovrhdr;     /* オーバーランハンドラ開始アドレス */
} T_DOVR;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `ovratr` | `ATR` | オーバーランハンドラ属性 |
| `ovrhdr` | `FP` | オーバーランハンドラ関数アドレス |

### T_ROVR - オーバーランハンドラ状態参照パケット

`ref_ovr` で返されるオーバーランハンドラ状態情報。

```c
typedef struct t_rovr {
    STAT    ovrstat;    /* オーバーランハンドラ状態 */
    OVRTIM  leftotm;    /* 残りプロセッサ時間 */
} T_ROVR;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `ovrstat` | `STAT` | オーバーランハンドラ状態 (TOVR_STP/TOVR_STA) |
| `leftotm` | `OVRTIM` | 残りプロセッサ時間 |

## システム管理

### T_RSYS - システム状態参照パケット

`ref_sys` で返されるシステム状態情報。

```c
typedef struct t_rsys {
} T_RSYS;
```

空の構造体。本実装では未使用。

### T_RCFG - コンフィグレーション情報参照パケット

`ref_cfg` で返されるコンフィグレーション情報。

```c
typedef struct t_rcfg {
} T_RCFG;
```

空の構造体。本実装では未使用。

### T_RVER - バージョン情報参照パケット

`ref_ver` で返されるバージョン情報。

```c
typedef struct t_rver {
    UH      maker;      /* カーネルメーカーコード */
    UH      prid;       /* カーネル ID */
    UH      spver;      /* ITRON バージョン番号 */
    UH      prver;      /* カーネルバージョン番号 */
    UH      prno[4];    /* カーネル製品管理情報 */
} T_RVER;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `maker` | `UH` | メーカーコード (TKERNEL_MAKER=0x0000) |
| `prid` | `UH` | カーネル ID (TKERNEL_PRID=0x0001) |
| `spver` | `UH` | ITRON 仕様バージョン (TKERNEL_SPVER=0x5400 = v4.0.0) |
| `prver` | `UH` | カーネルバージョン (TKERNEL_PRVER=0x0000) |
| `prno` | `UH[4]` | 製品管理情報 (4 要素) |

## 割り込み管理

### T_DINH - 割り込みハンドラ定義パケット

`def_inh` に渡す割り込みハンドラ定義情報。

```c
typedef struct t_dinh {
    ATR     inhatr;     /* 割り込みハンドラ属性 */
    FP      inthdr;     /* 割り込みハンドラ開始アドレス */
} T_DINH;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `inhatr` | `ATR` | 割り込みハンドラ属性 |
| `inthdr` | `FP` | 割り込みハンドラ関数アドレス |

### T_CISR - 割り込みサービスルーチン生成情報パケット

`cre_isr` / `acre_isr` に渡す割り込みサービスルーチン生成情報。

```c
typedef struct t_cisr {
    ATR     isratr;     /* 割り込みサービスルーチン属性 */
    VP_INT  exinf;      /* 拡張情報 */
    INTNO   intno;      /* 割り込み番号 */
    FP      isr;        /* 割り込みサービスルーチン開始アドレス */
} T_CISR;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `isratr` | `ATR` | 割り込みサービスルーチン属性 |
| `exinf` | `VP_INT` | 拡張情報 |
| `intno` | `INTNO` | 対象割り込み番号 |
| `isr` | `FP` | 割り込みサービスルーチン関数アドレス |

### T_RISR - 割り込みサービスルーチン状態参照パケット

`ref_isr` で返される割り込みサービスルーチン状態情報。

```c
typedef struct t_risr {
} T_RISR;
```

空の構造体。本実装では未使用。

## 拡張サービスコール・CPU 例外

### T_DSVC - 拡張サービスコール定義パケット

`def_svc` に渡す拡張サービスコール定義情報。

```c
typedef struct t_dsvc {
    ATR     svcatr;     /* 拡張サービスコール属性 */
    FP      svcrtn;     /* 拡張サービスコール開始アドレス */
} T_DSVC;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `svcatr` | `ATR` | 拡張サービスコール属性 |
| `svcrtn` | `FP` | 拡張サービスコール関数アドレス |

### T_DEXC - CPU 例外ハンドラ定義パケット

`def_exc` に渡す CPU 例外ハンドラ定義情報。

```c
typedef struct t_dexc {
    ATR     excatr;     /* CPU 例外ハンドラ属性 */
    FP      exchdr;     /* CPU 例外ハンドラ開始アドレス */
} T_DEXC;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `excatr` | `ATR` | CPU 例外ハンドラ属性 |
| `exchdr` | `FP` | CPU 例外ハンドラ関数アドレス |

## バイトオーダー変換マクロ (i386)

i386 はリトルエンディアンのため、ネットワークバイトオーダー (ビッグエンディアン) との変換が必要。

| マクロ | 説明 |
|--------|------|
| `htons(x)` | ホスト→ネットワーク (16 ビット) |
| `htonl(x)` | ホスト→ネットワーク (32 ビット) |
| `ntohs(x)` | ネットワーク→ホスト (16 ビット)。`htons` と同一 |
| `ntohl(x)` | ネットワーク→ホスト (32 ビット)。`htonl` と同一 |

```c
#define htons(x)  ((((x << 8) & 0xff00) | ((x >> 8) & 0x00ff)) & 0xffff)
#define htonl(x)  (((x << 8) & 0x00ff0000) | \
                   ((x << 24) & 0xff000000) | \
                   ((x >> 8) & 0x0000ff00) | \
                   ((x >> 24) & 0x000000ff))
#define ntohs(x)  htons(x)
#define ntohl(x)  htonl(x)
```

リトルエンディアン <-> ビッグエンディアンの変換は対称操作であるため、`ntohs` / `ntohl` はそれぞれ `htons` / `htonl` のエイリアスとして定義されている。
