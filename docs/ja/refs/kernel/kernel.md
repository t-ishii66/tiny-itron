# kernel.c / kernel.h

対象ファイル: `kernel/kernel.c`, `kernel/kernel.h`

## 概要

カーネル初期化のコアモジュール。`itron_init()` を提供し、タスク管理・メモリプール・スケジューラ・同期オブジェクトなど全サブシステムの初期化を一括で行う。また、カーネル全体で使用されるグローバルなデータ構造体 (タスク制御ブロック配列、同期オブジェクト配列、タイムアウト管理構造体など) を定義する。

## 定数・マクロ

本ファイル固有の定数定義はない。以下のヘッダから定数を取得する:

| 定数 | 定義元 | 値 | 説明 |
|------|--------|----|------|
| MAX_TSKID | include/config.h | 16 | タスク ID の最大値 |
| TMAX_TPRI | include/config.h | 16 | タスク優先度の最大値 |
| MAX_SEMID | include/config.h | 16 | セマフォ ID の最大値 |
| MAX_FLGID | include/config.h | 16 | イベントフラグ ID の最大値 |
| MAX_DTQID | include/config.h | 16 | データキュー ID の最大値 |
| MAX_MBXID | include/config.h | 16 | メールボックス ID の最大値 |
| MAX_MTXID | include/config.h | 16 | ミューテックス ID の最大値 |
| MAX_PORID | include/config.h | 16 | ランデブポート ID の最大値 |
| MAX_MBFID | include/config.h | 16 | メッセージバッファ ID の最大値 |
| MAX_MPFID | include/config.h | 16 | 固定長メモリプール ID の最大値 |
| MAX_MPLID | include/config.h | 16 | 可変長メモリプール ID の最大値 |
| MAX_CYCID | include/config.h | 16 | 周期ハンドラ ID の最大値 |
| MAX_ALMID | include/config.h | 16 | アラームハンドラ ID の最大値 |
| MAX_ISRID | include/config.h | 15 | 割り込みサービスルーチン ID の最大値 |
| MAX_INHID | include/config.h | 16 | 割り込みハンドラ ID の最大値 |
| MAX_EXCID | include/config.h | 16 | CPU 例外ハンドラ ID の最大値 |
| MAX_FNCD | include/config.h | 16 | 拡張サービスコール機能コードの最大値 |
| STACK_START | i386/addr.h | 0x700000 | スタックプール開始アドレス |
| STACK_END | i386/addr.h | 0x74ffff | スタックプール終了アドレス |
| MEM_START | i386/addr.h | 0x110000 | ユーザーメモリプール開始アドレス (kmem プールの終端) |
| MEM_END | i386/addr.h | 0x6fffff | メモリプール終了アドレス |

## 構造体・型

本ファイルで定義する構造体はない。使用する型は `kernel/types.h` で定義されている (T_TSK, T_LINK, T_TIMEOUT, T_SEM, T_FLG, T_DTQ, T_MBX, T_MTX, T_POR, T_MBF, T_MPF, T_MPL, T_CYC, T_ALM, T_ISR, T_INH, T_EXC, T_SVC)。

## グローバル変数

| 変数名 | 型 | サイズ | 説明 |
|--------|-----|--------|------|
| `tsk` | `T_TSK[MAX_TSKID + 1]` | 17 要素 | タスク制御ブロック配列。インデックス 0 は未使用、1〜16 がタスク ID に対応 |
| `tsk_pri` | `T_LINK[TMAX_TPRI + 1]` | 17 要素 | 優先度別レディキュー。双方向リンクリストのヘッダ配列。インデックスが優先度に対応 |
| `timeout` | `T_TIMEOUT` | 1 要素 | タイムアウト管理用デルタチェインのヘッダ |
| `sem` | `T_SEM[MAX_SEMID + 1]` | 17 要素 | セマフォ制御ブロック配列 |
| `flg` | `T_FLG[MAX_FLGID + 1]` | 17 要素 | イベントフラグ制御ブロック配列 |
| `dtq` | `T_DTQ[MAX_DTQID + 1]` | 17 要素 | データキュー制御ブロック配列 |
| `mbx` | `T_MBX[MAX_MBXID + 1]` | 17 要素 | メールボックス制御ブロック配列 |
| `mtx` | `T_MTX[MAX_MTXID + 1]` | 17 要素 | ミューテックス制御ブロック配列 |
| `por` | `T_POR[MAX_PORID + 1]` | 17 要素 | ランデブポート制御ブロック配列 |
| `mbf` | `T_MBF[MAX_MBFID + 1]` | 17 要素 | メッセージバッファ制御ブロック配列 |
| `mpf` | `T_MPF[MAX_MPFID + 1]` | 17 要素 | 固定長メモリプール制御ブロック配列 |
| `mpl` | `T_MPL[MAX_MPLID + 1]` | 17 要素 | 可変長メモリプール制御ブロック配列 |
| `cyc` | `T_CYC[MAX_CYCID + 1]` | 17 要素 | 周期ハンドラ制御ブロック配列 |
| `alm` | `T_ALM[MAX_ALMID + 1]` | 17 要素 | アラームハンドラ制御ブロック配列 |
| `isr` | `T_ISR[MAX_ISRID + 1]` | 16 要素 | 割り込みサービスルーチン制御ブロック配列 |
| `inh` | `T_INH[MAX_INHID + 1]` | 17 要素 | 割り込みハンドラ制御ブロック配列 |
| `exc` | `T_EXC[MAX_EXCID + 1]` | 17 要素 | CPU 例外ハンドラ制御ブロック配列 |
| `svc` | `T_SVC[MAX_FNCD + 1]` | 17 要素 | 拡張サービスコール制御ブロック配列 |
| `system_time` | `SYSTIM` | 1 要素 | システム時刻 (l: 下位 32bit, h: 上位 32bit) |

## 関数リファレンス

### itron_init

```c
int itron_init(void);
```

**概要:** カーネル全サブシステムを初期化する。main() から呼び出される。

**引数:** なし

**戻り値:** `0` (E_OK) -- 常に成功

**処理内容:**

以下のサブシステム初期化関数を順に呼び出す:

1. `tsk_init()` -- タスク制御ブロック (tsk[]) の初期化
2. `stack_init((VP)STACK_START, (VP)STACK_END)` -- スタックプールの初期化 (0x700000〜0x74ffff)
3. `mem_init((VP)MEM_START, (VP)MEM_END)` -- ユーザーメモリプールの初期化 (0x110000〜0x6fffff)
4. `kmem_init((VP)&_user_data_end, (VP)MEM_START)` -- カーネルメモリプールの初期化 (Supervisor ページ、〜0x20000〜0x110000)
5. `sched_init()` -- スケジューラの初期化 (優先度キュー、タイムアウトチェイン)
6. `mbf_init()` -- メッセージバッファの初期化
7. `dtq_init()` -- データキューの初期化
8. `sem_init()` -- セマフォの初期化

**呼び出し元:** `main()` (i386/main.c)。`proc_init()` より前に呼ばれなければならない (過去のバグ: proc_init が先だと tsk_init が tskstat を上書きしてタスクが失われる)。

**注意点:**
- CLAUDE.md および MEMORY.md に記載のとおり、`itron_init()` は `proc_init()` の **前** に呼ぶ必要がある。逆順にすると `tsk_init()` が `proc_init()` で設定した tskstat を TTS_NON にクリアし、タスクが永久に失われるバグが発生する。
- 現在の実装では flg_init(), mbx_init(), mtx_init(), por_init(), mpf_init(), mpl_init(), cyc_init(), alm_init(), ovr_init(), isr_init(), tim_init() の呼び出しは省略されている。これらのサブシステムは初期化なしでも (BSS セクションのゼロ初期化により) 基本的な動作が可能。

---

### bcopy

```c
void bcopy(unsigned char* from, unsigned char* to, unsigned long count);
```

**概要:** バイト単位のメモリコピーユーティリティ。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `from` | `unsigned char*` | コピー元アドレス |
| `to` | `unsigned char*` | コピー先アドレス |
| `count` | `unsigned long` | コピーするバイト数 |

**戻り値:** なし (void)

**処理内容:**
前方方向に 1 バイトずつコピーする単純なループ実装。オーバーラップ領域の安全性は考慮されていない (前方コピーのみ)。

**呼び出し元:** カーネル内の各モジュールからメモリコピーが必要な場面で使用される。

**注意点:**
- コピー元とコピー先が重複する場合、コピー方向によっては破壊が起こりうる (memmove 相当の安全性はない)。

## 補足

### kernel.h

`kernel.h` は非常にシンプルなヘッダで、`itron_init()` の関数プロトタイプのみを宣言する:

```c
int itron_init(void);
```

### 初期化順序の重要性

`main()` での初期化は以下の順序でなければならない:

1. `itron_init()` -- サブシステム初期化 (tsk[] のゼロクリアを含む)
2. `proc_init()` -- タスクの実体を作成し tsk[].tskstat = TTS_RUN に設定

この順序が逆だと、`tsk_init()` が proc_init() の設定を上書きし、タスクが TTS_NON (存在しない) 状態になってしまう。

### extern 宣言

`kernel/val.h` にて、本ファイルで定義するグローバル変数の extern 宣言が提供されており、他のカーネルモジュールから参照可能になっている。
