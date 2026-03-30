# sys_tim.c / sys_tim.h

対象ファイル: `kernel/sys_tim.c`, `kernel/sys_tim.h`

## 概要

システム時刻管理のシステムコール実装。Micro ITRON v4.0.0 仕様に基づく、システム時刻 (`SYSTIM`) の設定・取得機構を提供する。

システム時刻は `SYSTIM` 構造体で管理され、上位 32 ビット (`h`) と下位 32 ビット (`l`) の 2 フィールドで 64 ビットの時刻値を表現する。グローバル変数 `system_time` に格納される。

### データ構造

```c
extern SYSTIM system_time;
```

`SYSTIM` は上位・下位の 32 ビット値 (`h`, `l`) で構成される構造体。

## 関数リファレンス

### tim_init

```c
void tim_init(void)
```

**概要:** システム時刻を初期化する。

**引数:** なし

**戻り値:** なし

**処理内容:**
1. `system_time.l = 0` (下位 32 ビットを 0 に設定)
2. `system_time.h = 0` (上位 32 ビットを 0 に設定)

---

### sys_set_tim

```c
ER sys_set_tim(W apic, SYSTIM* p_systim)
```

**概要:** システム時刻を設定する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID (呼び出し元 CPU) |
| p_systim | SYSTIM* | 設定する時刻値 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |

**処理内容:**
1. `p_systim->l` を `system_time.l` にコピー
2. `p_systim->h` を `system_time.h` にコピー
3. E_OK を返す

---

### sys_get_tim

```c
ER sys_get_tim(W apic, SYSTIM* p_systim)
```

**概要:** 現在のシステム時刻を取得する。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |
| p_systim | SYSTIM* | 時刻値格納先 |

**戻り値:**
| 値 | 説明 |
|------|------|
| E_OK | 正常終了 |

**処理内容:**
1. `system_time.l` を `p_systim->l` にコピー
2. `system_time.h` を `p_systim->h` にコピー
3. E_OK を返す

---

### sys_isig_tim

```c
void sys_isig_tim(W apic)
```

**概要:** タイマ割り込みからのシステム時刻更新通知 (非タスクコンテキスト用)。

**引数:**
| 名前 | 型 | 説明 |
|------|------|------|
| apic | W | APIC ID |

**戻り値:** なし

**処理内容:**
1. `system_time.l` に `TIC_NUME` (17) を加算
2. 下位 32 ビットがオーバーフローした場合、`system_time.h` を 1 インクリメント (キャリー)

**呼び出し元:** `timer_intr()` (`i386/timer.c`) から CPU 0 のティックごとに呼ばれる。
タイムアウト処理 (`sched_timeout`) や周期/アラームハンドラ (`cyc_intr`/`alm_intr`) は
`timer_intr` 内で別途呼ばれるため、本関数はシステム時刻の更新のみを担当する。

## 補足

- システム時刻は 64 ビット値を 32 ビット × 2 フィールドで表現する。単位はミリ秒。
- タイマーティック周期は `TIC_NUME = 17` ミリ秒 (`include/config.h`)。PIT を HZ=60 に設定しており、1 ティック ≈ 16.7ms。
- `sys_isig_tim` は `timer_intr()` から CPU 0 のティックごとに呼ばれる。タイムアウト処理や周期/アラームハンドラは `timer_intr` 内で別経路で処理される。
- SMP 環境では `system_time` の読み書きに排他制御が無いため、マルチ CPU 同時アクセスでの一貫性は保証されない。ただし `sys_isig_tim` は CPU 0 からのみ呼ばれるため、書き込み競合は発生しない。
