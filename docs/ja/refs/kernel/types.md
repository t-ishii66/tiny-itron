# types.h / val.h

対象ファイル: `kernel/types.h`, `kernel/val.h`

## 概要

カーネル内部で使用されるデータ構造体と、それらのグローバル変数の extern 宣言を定義する。`types.h` は全カーネルオブジェクト (タスク、セマフォ、イベントフラグ、データキュー、メールボックス、ミューテックス、メッセージバッファ、ランデブポート、メモリプール、周期ハンドラ、アラームハンドラ、割り込みサービスルーチンなど) の制御ブロック構造体を定義する。`val.h` はこれらの配列の extern 宣言を提供し、カーネルモジュール間のデータ共有を実現する。

## 定数・マクロ

### リンクポインタ → T_TSK 変換マクロ

| マクロ | 定義 | 説明 |
|--------|------|------|
| `wlink2tsk(ptr)` | `(T_TSK*)((char*)ptr - sizeof(T_LINK))` | wlink ポインタから T_TSK ポインタを算出。T_TSK 内の wlink は plink の直後に配置されているため、sizeof(T_LINK) を減算 |
| `plink2tsk(ptr)` | `(T_TSK*)(ptr)` | plink ポインタから T_TSK ポインタを算出。plink は T_TSK の先頭メンバのためキャストのみ |
| `tlink2tsk(ptr)` | `(T_TSK*)((char*)ptr - 2 * sizeof(T_LINK))` | tlink ポインタから T_TSK ポインタを算出。tlink は plink, wlink の後に配置されているため、2 * sizeof(T_LINK) を減算 |

### その他の定数

| 定数 | 定義元 | 値 | 説明 |
|------|--------|----|------|
| MAX_MPL_POOL | types.h | 64 | T_MPL の pool 配列の最大エントリ数 |

## 構造体・型

### T_LINK

```c
typedef struct link {
    struct link*    prev;
    struct link*    next;
} T_LINK;
```

**説明:** 汎用の双方向リンクリストノード。タスクの優先度キュー (plink)、待ちキュー (wlink)、同期オブジェクトの待ちキューなど、カーネル全体で広く使用される。空リストの状態では prev と next が自分自身を指す。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `prev` | `struct link*` | 前のノードへのポインタ |
| `next` | `struct link*` | 次のノードへのポインタ |

---

### T_TIMEOUT

```c
typedef struct timeout {
    struct timeout* prev;
    struct timeout* next;
    TMO             delta;
} T_TIMEOUT;
```

**説明:** タイムアウトチェインの要素。デルタチェイン方式で管理される。delta は前の要素からの差分時間を表し、先頭要素のみのデクリメントで全タイムアウトの管理が可能。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `prev` | `struct timeout*` | 前の要素へのポインタ |
| `next` | `struct timeout*` | 次の要素へのポインタ |
| `delta` | `TMO` (UINT) | 前の要素からの差分タイムアウト値 |

---

### T_TSK

```c
typedef struct tsk {
    T_LINK      plink;
    T_LINK      wlink;
    T_TIMEOUT   tlink;
    ID          tskid;
    PRI         tskbpri;
    PRI         tskpri;
    proc_t*     proc;
    T_CTSK      ctsk;
    STAT        tskstat;
    UINT        actcnt;
    UINT        wupcnt;
    UINT        suscnt;
    /* event flag */
    FLGPTN*     p_flgptn;
    FLGPTN      waiptn;
    MODE        wfmode;
    /* data queue */
    VP_INT      data;
    VP_INT*     p_data;
    /* mail box */
    T_MSG**     ppk_msg;
    /* mutex */
    UINT        mtxcnt;
    /* message buffer */
    VP          msg;
    UINT        msgsz;
    /* memory pool */
    VP*         p_blk;
    UINT        blksz;
    /* rendezvous */
    RDVPTN      acpptn;
    RDVPTN      calptn;
    VP          por_msg;
    UINT        por_msgsz;
    RDVNO*      p_rdvno;
    RDVNO       rdvno;
    /* exception handler */
    TEXPTN      pndptn;
    STAT        texstat;
    T_DTEX      tex;
    /* over run handler */
    ATR         ovratr;
    FP          ovrhdr;
    OVRTIM      ovrtim;
    STAT        ovrstat;
    VP_INT      exinf;
    /* tcp/ip network */
    VP          net_data;
    VP          net_data_len;
} T_TSK;
```

**説明:** タスク制御ブロック (TCB)。カーネルが管理する各タスクの全状態情報を保持する。ITRON 仕様のタスク管理フィールドに加え、各同期オブジェクトの待ちパラメータや例外ハンドラ情報も含む。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `plink` | `T_LINK` | 優先度キュー用リンクノード。`plink2tsk()` で T_TSK* に変換可能 |
| `wlink` | `T_LINK` | 待ちキュー用リンクノード。`wlink2tsk()` で T_TSK* に変換可能 |
| `tlink` | `T_TIMEOUT` | タイムアウトチェイン用ノード。`tlink2tsk()` で T_TSK* に変換可能 |
| `tskid` | `ID` | タスク ID (1〜MAX_TSKID) |
| `tskbpri` | `PRI` | ベース優先度 (生成時に指定された優先度) |
| `tskpri` | `PRI` | 現在の優先度 (ミューテックスの優先度継承により変動可能) |
| `proc` | `proc_t*` | プロセス構造体へのポインタ (コンテキスト情報、CPU アフィニティ等) |
| `ctsk` | `T_CTSK` | タスク生成時の初期引数 (cre_tsk のパラメータ) |
| `tskstat` | `STAT` | タスク状態: TTS_RUN(0x01), TTS_RDY(0x02), TTS_WAI(0x04), TTS_SUS(0x08), TTS_WAS(0x0c), TTS_DMT(0x10), TTS_NON(0x00) |
| `actcnt` | `UINT` | 起動要求キューイング数 (TMAX_ACTCNT まで) |
| `wupcnt` | `UINT` | 起床要求キューイング数 (TMAX_WUPCNT まで) |
| `suscnt` | `UINT` | 強制待ちネスト数 (TMAX_SUSCNT まで) |
| `p_flgptn` | `FLGPTN*` | イベントフラグ: 待ち解除時のフラグパターン戻り値格納先 |
| `waiptn` | `FLGPTN` | イベントフラグ: 待ちフラグパターン |
| `wfmode` | `MODE` | イベントフラグ: 待ちモード (TWF_ANDW or TWF_ORW) |
| `data` | `VP_INT` | データキュー: 送受信データ |
| `p_data` | `VP_INT*` | データキュー: 受信データ戻り値格納先 |
| `ppk_msg` | `T_MSG**` | メールボックス: 受信メッセージパケット格納先 |
| `mtxcnt` | `UINT` | ミューテックス: 保持中のミューテックス数 |
| `msg` | `VP` | メッセージバッファ: メッセージ本体ポインタ |
| `msgsz` | `UINT` | メッセージバッファ: メッセージサイズ |
| `p_blk` | `VP*` | メモリプール: 確保ブロック戻り値格納先 |
| `blksz` | `UINT` | メモリプール: 要求ブロックサイズ |
| `acpptn` | `RDVPTN` | ランデブ: 受付側ビットパターン |
| `calptn` | `RDVPTN` | ランデブ: 呼出側ビットパターン |
| `por_msg` | `VP` | ランデブ: メッセージポインタ |
| `por_msgsz` | `UINT` | ランデブ: メッセージサイズ |
| `p_rdvno` | `RDVNO*` | ランデブ: ランデブ番号戻り値格納先 |
| `rdvno` | `RDVNO` | ランデブ: ランデブ番号 (sched_ins 時にクリアされるべき) |
| `pndptn` | `TEXPTN` | タスク例外: 保留中の例外パターン |
| `texstat` | `STAT` | タスク例外: 状態 (0=dis-tex, 1=ena-tex) |
| `tex` | `T_DTEX` | タスク例外: ハンドラ定義 |
| `ovratr` | `ATR` | オーバーランハンドラ: 属性 |
| `ovrhdr` | `FP` | オーバーランハンドラ: ハンドラアドレス |
| `ovrtim` | `OVRTIM` | オーバーランハンドラ: プロセッサ時間 |
| `ovrstat` | `STAT` | オーバーランハンドラ: 状態 |
| `exinf` | `VP_INT` | 拡張情報 |
| `net_data` | `VP` | TCP/IP: ネットワークデータポインタ |
| `net_data_len` | `VP` | TCP/IP: ネットワークデータ長 |

---

### T_SEM

```c
typedef struct t_sem {
    T_LINK      wlink;
    ATR         sematr;
    UINT        isemcnt;
    UINT        maxsem;
    UINT        semcnt;
    STAT        act;
} T_SEM;
```

**説明:** セマフォ制御ブロック。カウンティングセマフォの管理情報を保持する。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `wlink` | `T_LINK` | 待ちタスクキュー (wai_sem で待機中のタスクのリスト) |
| `sematr` | `ATR` | セマフォ属性 (TA_TFIFO: FIFO 順, TA_TPRI: 優先度順) |
| `isemcnt` | `UINT` | 初期カウント値 |
| `maxsem` | `UINT` | 最大カウント値 |
| `semcnt` | `UINT` | 現在のカウント値 |
| `act` | `STAT` | 生成状態フラグ |

---

### T_FLG

```c
typedef struct t_flg {
    T_LINK      wlink;
    ATR         flgatr;
    FLGPTN      flgptn;
    FLGPTN      iflgptn;
    STAT        act;
} T_FLG;
```

**説明:** イベントフラグ制御ブロック。ビットパターンベースの同期機構の管理情報を保持する。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `wlink` | `T_LINK` | 待ちタスクキュー |
| `flgatr` | `ATR` | フラグ属性 (TA_WSGL: 単一待ち, TA_WMUL: 複数待ち, TA_CLR: 自動クリア) |
| `flgptn` | `FLGPTN` | 現在のフラグビットパターン (32bit) |
| `iflgptn` | `FLGPTN` | 初期フラグパターン |
| `act` | `STAT` | 生成状態フラグ |

---

### T_DTQ

```c
typedef struct t_dtq {
    T_LINK      wlink_w;
    T_LINK      wlink_r;
    ATR         dtqatr;
    UINT        dtqcnt;
    VP          dtq;
    STAT        act;
    STAT        dtq_alloc;
    VW          r;
    VW          w;
} T_DTQ;
```

**説明:** データキュー制御ブロック。FIFO 方式のデータ送受信キューの管理情報を保持する。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `wlink_w` | `T_LINK` | 送信待ちタスクキュー (キューフル時に snd_dtq で待機するタスク) |
| `wlink_r` | `T_LINK` | 受信待ちタスクキュー (キュー空時に rcv_dtq で待機するタスク) |
| `dtqatr` | `ATR` | データキュー属性 |
| `dtqcnt` | `UINT` | データキューのエントリ数 (容量) |
| `dtq` | `VP` | データ格納バッファへのポインタ |
| `act` | `STAT` | 生成状態フラグ |
| `dtq_alloc` | `STAT` | カーネルによるバッファ割り当てフラグ |
| `r` | `VW` | リングバッファ読み出し位置 |
| `w` | `VW` | リングバッファ書き込み位置 |

---

### T_MBX

```c
typedef struct t_mbx {
    T_LINK      wlink;
    ATR         mbxatr;
    PRI         maxmpri;
    VP          mprihd;
    STAT        mbx_alloc;
    STAT        act;
} T_MBX;
```

**説明:** メールボックス制御ブロック。メッセージパケットの送受信キューの管理情報を保持する。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `wlink` | `T_LINK` | 受信待ちタスクキュー |
| `mbxatr` | `ATR` | メールボックス属性 (TA_MFIFO/TA_MPRI: メッセージ順序) |
| `maxmpri` | `PRI` | メッセージ優先度の最大値 |
| `mprihd` | `VP` | 優先度別メッセージヘッダ |
| `mbx_alloc` | `STAT` | カーネルによるバッファ割り当てフラグ |
| `act` | `STAT` | 生成状態フラグ |

---

### T_MTX

```c
typedef struct t_mtx {
    ID          tskid;
    T_LINK      wlink;
    ATR         mtxatr;
    PRI         ceilpri;
    W           mtxlock;
    STAT        act;
} T_MTX;
```

**説明:** ミューテックス制御ブロック。排他制御機構の管理情報を保持する。優先度継承/上限プロトコルに対応。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `tskid` | `ID` | ロックを保持しているタスクの ID |
| `wlink` | `T_LINK` | 待ちタスクキュー |
| `mtxatr` | `ATR` | ミューテックス属性 (TA_INHERIT: 優先度継承, TA_CEILING: 優先度上限) |
| `ceilpri` | `PRI` | 上限優先度 (TA_CEILING 使用時) |
| `mtxlock` | `W` | ロックパラメータ |
| `act` | `STAT` | 生成状態フラグ |

---

### T_MBF

```c
typedef struct t_mbf {
    T_LINK          wlink_r;
    T_LINK          wlink_s;
    T_TSK*          wtsk;
    ATR             mbfatr;
    UINT            maxmsz;
    SIZE            mbfsz;
    VP              mbf;
    UINT            smsgcnt;
    unsigned char*  mbf_end;
    unsigned char*  mbf_r;
    unsigned char*  mbf_w;
    VP              mbf_alloc_base;
    STAT            mbf_alloc;
    STAT            act;
} T_MBF;
```

**説明:** メッセージバッファ制御ブロック。可変長メッセージの送受信バッファの管理情報を保持する。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `wlink_r` | `T_LINK` | 受信待ちタスクキュー |
| `wlink_s` | `T_LINK` | 送信待ちタスクキュー |
| `wtsk` | `T_TSK*` | 待機中のタスクへのポインタ |
| `mbfatr` | `ATR` | メッセージバッファ属性 |
| `maxmsz` | `UINT` | メッセージの最大サイズ |
| `mbfsz` | `SIZE` | バッファの総サイズ |
| `mbf` | `VP` | バッファ領域へのポインタ |
| `smsgcnt` | `UINT` | 格納済みメッセージ数 |
| `mbf_end` | `unsigned char*` | バッファ終端アドレス |
| `mbf_r` | `unsigned char*` | 読み出しポインタ |
| `mbf_w` | `unsigned char*` | 書き込みポインタ |
| `mbf_alloc_base` | `VP` | カーネル割り当てバッファのベースアドレス |
| `mbf_alloc` | `STAT` | カーネルによるバッファ割り当てフラグ |
| `act` | `STAT` | 生成状態フラグ |

---

### T_POR

```c
typedef struct t_por {
    T_LINK      wlink_c;
    T_LINK      wlink_a;
    ATR         poratr;
    UINT        maxcmsz;
    UINT        maxrmsz;
    UINT        rdvno;
    STAT        act;
} T_POR;
```

**説明:** ランデブポート制御ブロック。ランデブ (同期メッセージ通信) の管理情報を保持する。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `wlink_c` | `T_LINK` | 呼出側 (cal_por) 待ちタスクキュー |
| `wlink_a` | `T_LINK` | 受付側 (acp_por) 待ちタスクキュー |
| `poratr` | `ATR` | ランデブポート属性 |
| `maxcmsz` | `UINT` | 呼出メッセージの最大サイズ |
| `maxrmsz` | `UINT` | 返答メッセージの最大サイズ |
| `rdvno` | `UINT` | ランデブ番号 |
| `act` | `STAT` | 生成状態フラグ |

---

### T_MPF

```c
typedef struct t_mpf {
    T_LINK          wlink;
    ATR             mpfatr;
    UINT            blkcnt;
    UINT            blksz;
    VP              mpf;
    allocation_t*   pool;
    STAT            mpf_alloc;
    STAT            act;
} T_MPF;
```

**説明:** 固定長メモリプール制御ブロック。固定サイズのメモリブロックの管理情報を保持する。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `wlink` | `T_LINK` | 待ちタスクキュー (get_mpf で待機中のタスク) |
| `mpfatr` | `ATR` | メモリプール属性 |
| `blkcnt` | `UINT` | ブロック数 |
| `blksz` | `UINT` | 1 ブロックのサイズ |
| `mpf` | `VP` | プール領域へのポインタ |
| `pool` | `allocation_t*` | allocation_t 配列へのポインタ |
| `mpf_alloc` | `STAT` | カーネルによるプール領域割り当てフラグ |
| `act` | `STAT` | 生成状態フラグ |

---

### T_MPL

```c
typedef struct t_mpl {
    T_LINK          wlink;
    ATR             mplatr;
    SIZE            mplsz;
    VP              mpl;
    allocation_t*   pool;
    STAT            mpl_alloc;
    STAT            act;
} T_MPL;
```

**説明:** 可変長メモリプール制御ブロック。可変サイズのメモリブロックの管理情報を保持する。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `wlink` | `T_LINK` | 待ちタスクキュー (get_mpl で待機中のタスク) |
| `mplatr` | `ATR` | メモリプール属性 |
| `mplsz` | `SIZE` | プール全体のサイズ |
| `mpl` | `VP` | プール領域へのポインタ |
| `pool` | `allocation_t*` | allocation_t 配列へのポインタ |
| `mpl_alloc` | `STAT` | カーネルによるプール領域割り当てフラグ |
| `act` | `STAT` | 生成状態フラグ |

---

### T_CYC

```c
typedef struct t_cyc {
    ATR         cycatr;
    VP_INT      exinf;
    FP          cychdr;
    RELTIM      cyctim;
    RELTIM      icyctim;
    RELTIM      cycphs;
    STAT        act;
    STAT        stat;
} T_CYC;
```

**説明:** 周期ハンドラ制御ブロック。一定周期で呼び出されるハンドラの管理情報を保持する。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `cycatr` | `ATR` | 周期ハンドラ属性 (TA_STA: 生成時に動作開始, TA_PHS: 位相指定) |
| `exinf` | `VP_INT` | 拡張情報 |
| `cychdr` | `FP` | ハンドラ関数アドレス |
| `cyctim` | `RELTIM` | 起動周期 |
| `icyctim` | `RELTIM` | 初期起動周期 |
| `cycphs` | `RELTIM` | 起動位相 |
| `act` | `STAT` | 生成状態フラグ |
| `stat` | `STAT` | 動作状態 (TCYC_STP: 停止, TCYC_STA: 動作中) |

---

### T_ALM

```c
typedef struct t_alm {
    ATR         almatr;
    VP_INT      exinf;
    FP          almhdr;
    STAT        almstat;
    RELTIM      almtim;
    STAT        act;
} T_ALM;
```

**説明:** アラームハンドラ制御ブロック。指定時間後に 1 回だけ呼び出されるハンドラの管理情報を保持する。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `almatr` | `ATR` | アラームハンドラ属性 |
| `exinf` | `VP_INT` | 拡張情報 |
| `almhdr` | `FP` | ハンドラ関数アドレス |
| `almstat` | `STAT` | 動作状態 (TALM_STP: 停止, TALM_STA: 動作中) |
| `almtim` | `RELTIM` | アラーム時間 |
| `act` | `STAT` | 生成状態フラグ |

---

### T_ISR

```c
typedef struct t_isr {
    T_LINK      wlink;
    ATR         isratr;
    VP_INT      exinf;
    INTNO       intno;
    FP          isr;
    STAT        act;
} T_ISR;
```

**説明:** 割り込みサービスルーチン (ISR) 制御ブロック。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `wlink` | `T_LINK` | ISR チェイン用リンクノード (同一割り込み番号に複数 ISR を登録可能) |
| `isratr` | `ATR` | ISR 属性 |
| `exinf` | `VP_INT` | 拡張情報 |
| `intno` | `INTNO` | 割り込み番号 |
| `isr` | `FP` | ISR 関数アドレス |
| `act` | `STAT` | 生成状態フラグ |

---

### T_INH

```c
typedef struct t_inh {
    ATR         inhatr;
    FP          inthdr;
} T_INH;
```

**説明:** 割り込みハンドラ制御ブロック。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `inhatr` | `ATR` | 割り込みハンドラ属性 |
| `inthdr` | `FP` | ハンドラ関数アドレス |

---

### T_SVC

```c
typedef struct t_svc {
    ATR         svcatr;
    FP          svcrtn;
    STAT        act;
} T_SVC;
```

**説明:** 拡張サービスコール制御ブロック。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `svcatr` | `ATR` | サービスコール属性 |
| `svcrtn` | `FP` | サービスルーチン関数アドレス |
| `act` | `STAT` | 生成状態フラグ |

---

### T_EXC

```c
typedef struct t_exc {
    ATR         excatr;
    FP          exchdr;
} T_EXC;
```

**説明:** CPU 例外ハンドラ制御ブロック。

| フィールド | 型 | 説明 |
|------------|-----|------|
| `excatr` | `ATR` | CPU 例外ハンドラ属性 |
| `exchdr` | `FP` | ハンドラ関数アドレス |

## グローバル変数

### val.h で宣言される extern 変数

| 変数名 | 型 | 定義元 | 説明 |
|--------|-----|--------|------|
| `tsk[]` | `T_TSK` | kernel/kernel.c | タスク制御ブロック配列 |
| `tsk_pri[]` | `T_LINK` | kernel/kernel.c | 優先度別レディキュー配列 |
| `c_tskid[]` | `ID` | i386/proc.c | 各 CPU で現在実行中のタスク ID |
| `timeout` | `T_TIMEOUT` | kernel/kernel.c | タイムアウトチェインヘッダ |
| `sem[]` | `T_SEM` | kernel/kernel.c | セマフォ制御ブロック配列 |
| `flg[]` | `T_FLG` | kernel/kernel.c | イベントフラグ制御ブロック配列 |
| `mtx[]` | `T_MTX` | kernel/kernel.c | ミューテックス制御ブロック配列 |
| `por[]` | `T_POR` | kernel/kernel.c | ランデブポート制御ブロック配列 |
| `dtq[]` | `T_DTQ` | kernel/kernel.c | データキュー制御ブロック配列 |
| `mbx[]` | `T_MBX` | kernel/kernel.c | メールボックス制御ブロック配列 |
| `mbf[]` | `T_MBF` | kernel/kernel.c | メッセージバッファ制御ブロック配列 |
| `mpf[]` | `T_MPF` | kernel/kernel.c | 固定長メモリプール制御ブロック配列 |
| `mpl[]` | `T_MPL` | kernel/kernel.c | 可変長メモリプール制御ブロック配列 |
| `system_time` | `SYSTIM` | kernel/kernel.c | システム時刻 |
| `cyc[]` | `T_CYC` | kernel/kernel.c | 周期ハンドラ制御ブロック配列 |
| `alm[]` | `T_ALM` | kernel/kernel.c | アラームハンドラ制御ブロック配列 |
| `dispatch_stat` | `int` | kernel/sched.c | ディスパッチ状態 |
| `cpu_stat` | `int` | kernel/sched.c | CPU ロック状態 |
| `isr[]` | `T_ISR` | kernel/kernel.c | ISR 制御ブロック配列 |
| `inh[]` | `T_INH` | kernel/kernel.c | 割り込みハンドラ制御ブロック配列 |
| `exc[]` | `T_EXC` | kernel/kernel.c | CPU 例外ハンドラ制御ブロック配列 |
| `svc[]` | `T_SVC` | kernel/kernel.c | 拡張サービスコール制御ブロック配列 |

## 関数リファレンス

本ファイルに関数定義はない。

## 補足

### T_TSK のメモリレイアウトとリンクマクロ

T_TSK 構造体のメモリレイアウトは以下のとおり。リンクマクロはこのレイアウトに依存する:

```
T_TSK:
  offset 0:                    plink (T_LINK: prev, next)
  offset sizeof(T_LINK):       wlink (T_LINK: prev, next)
  offset 2*sizeof(T_LINK):     tlink (T_TIMEOUT: prev, next, delta)
  offset ...:                  tskid, tskbpri, tskpri, ...
```

- `plink2tsk(ptr)`: plink はオフセット 0 なので単純キャスト
- `wlink2tsk(ptr)`: wlink はオフセット sizeof(T_LINK) なのでその分を減算
- `tlink2tsk(ptr)`: tlink はオフセット 2*sizeof(T_LINK) なのでその分を減算

### タスク状態遷移

```
TTS_NON (未生成)
  ↓ cre_tsk
TTS_DMT (休止)
  ↓ act_tsk
TTS_RDY (実行可能)
  ↔ sched_do_next_tsk ↔ TTS_RUN (実行中)
  ↓ slp_tsk / wai_sem / ...
TTS_WAI (待ち)
  ↓ wup_tsk / sig_sem / ...
TTS_RDY
```

### ITRON 基本型の定義 (itron.h)

val.h と types.h で使用される基本型は include/itron.h で定義される:

| 型 | 実体 | 説明 |
|----|------|------|
| `B` | `char` | 符号付き 8bit |
| `H` | `short` | 符号付き 16bit |
| `W` | `long` | 符号付き 32bit |
| `UB` | `unsigned char` | 符号なし 8bit |
| `UH` | `unsigned short` | 符号なし 16bit |
| `UW` | `unsigned long` | 符号なし 32bit |
| `VP` | `char*` | 未知型ポインタ |
| `FP` | `void (*)()` | 関数ポインタ |
| `INT` / `ER` / `ID` | `int` | 整数 / エラーコード / オブジェクト ID |
| `ATR` / `STAT` / `MODE` | `unsigned int` | 属性 / 状態 / モード |
| `PRI` | `int` | 優先度 |
| `SIZE` | `unsigned int` | メモリサイズ |
| `TMO` | `unsigned int` | タイムアウト値 |
| `SYSTIM` | `struct { W l; W h; }` | システム時刻 (64bit) |
| `FLGPTN` | `unsigned int` | フラグビットパターン (32bit) |
| `RDVPTN` / `RDVNO` | `unsigned int` | ランデブパターン / 番号 |
| `VP_INT` | `int` | データまたはポインタ (32bit) |
