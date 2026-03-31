# sched.c / sched.h

対象ファイル: `kernel/sched.c`, `kernel/sched.h`

## 概要

ITRON カーネルのスケジューラモジュール。優先度ベースのレディキュー管理、タスク状態遷移、CPU アフィニティに基づくタスク選択、およびデルタチェイン方式のタイムアウト管理を提供する。SMP (2 CPU) 環境に対応しており、Big Kernel Lock (`kernel_lk`) による排他制御を行う。

スケジューラはプリエンプティブ方式ではなく、**システムコール契機** でのみタスク切り替えを行う。タイマー割り込みによるプリエンプションは行わないが、`sched_next_tsk()` で両 CPU にリスケジュール要求を出すことで、次回の割り込み時にタスク切り替えが発生する。

## 定数・マクロ

| 定数 | 定義元 | 値 | 説明 |
|------|--------|----|------|
| CPU_LOCK | sched.h | 1 | CPU ロック状態 (ccli 相当) |
| CPU_UNLOCK | sched.h | 2 | CPU アンロック状態 (csti 相当) |
| DISPATCH_ENABLE | sched.h | 1 | ディスパッチ許可 |
| DISPATCH_DISABLE | sched.h | 2 | ディスパッチ禁止 |
| DISPATCH_SUSPEND | sched.h | 3 | ディスパッチ保留 |
| TMIN_TPRI | include/config.h | 1 | 最小優先度値 (最高優先度) |
| TMAX_TPRI | include/config.h | 16 | 最大優先度値 (最低優先度) |

## 構造体・型

本ファイル固有の構造体定義はない。以下の型を使用する:

- `T_LINK` (kernel/types.h) -- 双方向リンクリストノード (prev, next)
- `T_TIMEOUT` (kernel/types.h) -- タイムアウトチェイン要素 (prev, next, delta)
- `T_TSK` (kernel/types.h) -- タスク制御ブロック (plink, tlink フィールドを使用)

## グローバル変数

| 変数名 | 型 | スコープ | 説明 |
|--------|-----|---------|------|
| `kernel_lk` | `unsigned long` | extern | Big Kernel Lock。全カーネルデータ構造 (tsk[], tsk_pri[], sem[], timeout 等) を保護。sched_do_next_tsk が自身で取得/解放。他は呼び出し元 (c_intr_syscall 等) が取得済み |
| `cpu_stat` | `INT` | extern | CPU ロック状態 (CPU_LOCK または CPU_UNLOCK) |
| `dispatch_stat` | `INT` | extern | ディスパッチ状態 (DISPATCH_ENABLE, DISPATCH_DISABLE, DISPATCH_SUSPEND) |
| `next_tsk_flag[2]` | `INT[2]` | extern | 各 CPU のリスケジュール要求フラグ。[0]=CPU 0, [1]=CPU 1 |

## 関数リファレンス

### sched_init

```c
void sched_init(void);
```

**概要:** スケジューラの初期化。優先度キューとタイムアウトチェインを空の双方向リンクリストとして初期化する。

**引数:** なし

**戻り値:** なし (void)

**処理内容:**
1. 優先度 TMIN_TPRI (1) から TMAX_TPRI (16) まで、`tsk_pri[i]` を自己参照の空リストに初期化 (next = prev = &tsk_pri[i])
2. `cpu_stat` を CPU_UNLOCK に設定
3. `dispatch_stat` を DISPATCH_ENABLE に設定
4. `timeout` を自己参照の空リストに初期化
5. `next_tsk_flag[0]` と `next_tsk_flag[1]` を 0 にクリア

**呼び出し元:** `itron_init()` (kernel/kernel.c)

**注意点:** なし

---

### sched_ins

```c
ER sched_ins(PRI pri, T_LINK* link);
```

**概要:** タスクのリンクノードを指定優先度のレディキュー末尾に挿入する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `pri` | `PRI` | 優先度 (1〜16) |
| `link` | `T_LINK*` | 挿入するタスクの plink フィールドへのポインタ |

**戻り値:**
- `E_OK` -- 成功
- `E_PAR` -- 優先度が範囲外 (pri < TMIN_TPRI または pri > TMAX_TPRI)

**処理内容:**
1. 優先度の範囲チェック
2. `link` を `tsk_pri[pri]` リストの末尾 (prev 側) に挿入

**前提条件:** caller holds `kernel_lk`

**呼び出し元:** `sched_hold_tsk()`, `sys_act_tsk()`, `sys_wup_tsk()`, `sched_timeout()`, 各同期オブジェクトのタスク復帰処理

**注意点:** FIFO 順 -- 同一優先度のタスクはキュー末尾に追加されるため、先に挿入されたタスクが先に選択される。

---

### sched_rem

```c
void sched_rem(T_LINK* link);
```

**概要:** タスクのリンクノードをレディキューから削除する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `link` | `T_LINK*` | 削除するタスクの plink フィールドへのポインタ |

**戻り値:** なし (void)

**処理内容:**
1. 双方向リンクリストから `link` を削除 (前後のノードを相互接続)

**前提条件:** caller holds `kernel_lk`

**呼び出し元:** `sched_hold_tsk()`, タスク状態変更時全般

**注意点:** link がリストに挿入されていない状態で呼ぶと、未定義動作になる。

---

### sched_hold_tsk

```c
ID sched_hold_tsk(ID tskid, STAT s);
```

**概要:** タスクの状態を変更し、レディキュー内での位置を再調整する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `tskid` | `ID` | タスク ID |
| `s` | `STAT` | 新しいタスク状態 (TTS_RDY, TTS_WAI, TTS_SUS 等) |

**戻り値:** `E_OK` -- 常に成功

**処理内容:**
1. `sched_rem()` でタスクの plink をレディキューから削除
2. `tsk[tskid].tskstat` を新状態 s に更新
3. `sched_ins()` でタスクの現在優先度 (`tskpri`) に基づきレディキューに再挿入

**前提条件:** caller holds `kernel_lk`

**呼び出し元:** `sys_slp_tsk()`, `sys_wai_sem()`, `sys_wai_flg()` 等のタスク待ち状態移行処理

**注意点:** タスクは状態が TTS_WAI 等であってもレディキューに残り続ける。sched_do_next_tsk が TTS_RDY のタスクのみを選択するため、WAI 状態のタスクはスキップされる。

---

### sched_next_tsk

```c
ID sched_next_tsk(W apic);
```

**概要:** 両 CPU にリスケジュール要求を送信する。割り込みハンドラから呼ばれ、次回の割り込み時にタスク切り替えが実行されるようフラグを設定する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `apic` | `W` | 呼び出し元の APIC ID (未使用。両 CPU のフラグをセットするため) |

**戻り値:** `E_OK` -- 常に成功

**処理内容:**
1. `next_tsk_flag[0] = 1` (CPU 0 にリスケジュール要求)
2. `next_tsk_flag[1] = 1` (CPU 1 にリスケジュール要求)

**呼び出し元:** `sys_wup_tsk()`, `iwup_tsk()`, `sched_timeout()`, その他タスク状態変更を伴うシステムコール

**注意点:**
- 引数 `apic` は参照されない。常に両 CPU のフラグをセットする。これは CPU 間のタスク起床を実現するため。
- このフラグは `sched_next_tsk_check()` (interrupt.c 内) で確認され、実際の `sched_do_next_tsk()` 呼び出しを行う。

**具体例: CPU 間のタスク起床**

キーボード割り込み (IRQ1) は PIC 経由で CPU 0 にのみ配送される。
一方、キーボードタスク (Task 4) は CPU 1 にアフィニティを持つ。この場合:

1. CPU 0 で IRQ1 発生 → `c_intr_irq1()` → `key_intr()`
2. `key_intr()` 内で `ipsnd_dtq(0, 2, ch)` を呼び、DTQ 2 に文字を格納。`rcv_dtq` で待機中の Task 4 を TTS_WAI → TTS_RDY に変更
3. `ipsnd_dtq()` 内で `sched_next_tsk(0)` → `next_tsk_flag[0] = next_tsk_flag[1] = 1`
4. CPU 1 の次の APIC タイマー割り込みで `sched_next_tsk_check()` が `next_tsk_flag[1] == 1` を検出
5. `sched_do_next_tsk()` が Task 4 (CPU 1 アフィニティ、TTS_RDY) を選択し、タスク切り替え

もし `next_tsk_flag[0]` だけをセットした場合、CPU 1 はリスケジュールを行わず、
Task 4 は次に CPU 1 で別のリスケジュール契機が発生するまで起床できない。
両方のフラグを立てることで、どの CPU にアフィニティを持つタスクが起床しても
確実にリスケジュールされる。

---

### sched_do_next_tsk

```c
ID sched_do_next_tsk(W apic);
```

**概要:** 指定 CPU で実行すべき最高優先度の RDY タスクを検索し、現在のタスクと入れ替える。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `apic` | `W` | 対象 CPU の APIC ID (0 または 1) |

**戻り値:**
- タスク ID -- 新しく実行するタスクの ID
- `E_ID` (-18) -- RDY タスクが見つからなかった場合

**処理内容:**
1. `kernel_lk` スピンロックを取得
2. 現在実行中のタスク (`c_tskid[apic]`) が TTS_RUN であれば TTS_RDY に変更 (プリエンプションの準備)
3. 優先度 1 (最高) から TMAX_TPRI (16) まで、各優先度キューを走査
4. 各キューのタスクについて:
   - `tskstat == TTS_RDY` かつ `proc[tskid].cpu == apic` (CPU アフィニティ一致) を確認
   - 条件を満たすタスクが見つかれば TTS_RUN に変更し、`current_proc[apic]` と `c_tskid[apic]` を更新
5. タスクが見つからなかった場合、元のタスクの状態を TTS_RUN に戻す
6. `kernel_lk` スピンロックを解放

**呼び出し元:** `sched_next_tsk_check()` (i386/interrupt.c)。割り込みハンドラの restore 直前で呼ばれる。

**注意点:**
- CPU アフィニティフィルタリングにより、各 CPU は自分に割り当てられたタスクのみを実行する
- 元のタスクが TTS_RUN 以外 (WAI, SUS 等) の場合は状態復元しない (was_run フラグで管理)
- RDY タスクが見つからない場合は E_ID を返し、呼び出し元は現在のタスクの実行を継続する。この状況を防ぐため、各 CPU に idle_task が配置されている。

---

### sched_timeout_ins

```c
void sched_timeout_ins(T_TIMEOUT* t);
```

**概要:** タイムアウトチェインにタイムアウト要素を挿入する。デルタチェイン方式で管理。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `t` | `T_TIMEOUT*` | 挿入するタイムアウト要素。`t->delta` にタイムアウト値を設定してから呼ぶ |

**戻り値:** なし (void)

**処理内容:**
1. チェインを走査し、デルタ値に基づいて適切な位置を決定
2. 走査中に `t->delta` から前方要素のデルタ値を減算
3. 挿入位置の **直後のエントリだけ** `tp->delta -= t->delta` で調整 (それ以降のエントリは `tp` からの相対値なので変更不要)
4. 双方向リンクリストに挿入

**前提条件:** caller holds `kernel_lk`

**呼び出し元:** `sys_tslp_tsk()`, `sys_twai_sem()`, `sys_dly_tsk()` 等のタイムアウト付き待ち処理

**注意点:**
- デルタチェイン方式: 各要素の delta は前の要素からの差分時間を表す。これにより、先頭要素のみのデクリメントで全要素のタイムアウト管理が可能
- 挿入時に調整するのは直後の 1 エントリのみ。それ以降は直後エントリからの相対値なので不変

---

### sched_timeout_rem

```c
void sched_timeout_rem(T_TIMEOUT* t);
```

**概要:** タイムアウトチェインから要素を削除する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `t` | `T_TIMEOUT*` | 削除するタイムアウト要素 |

**戻り値:** なし (void)

**処理内容:**
1. 後続エントリにデルタ値を加算 (`t->next->delta += t->delta`) して絶対満了時刻を保存
2. 双方向リンクリストから `t` を削除
3. `t` を自己参照にリセット (`t->next = t; t->prev = t`) してキュー外を表明

**前提条件:** caller holds `kernel_lk`

**呼び出し元:** `sched_timeout_rem_if_exist()` 経由で、タスクがタイムアウト前に起床された場合の後処理

---

### sched_timeout_rem_if_exist

```c
void sched_timeout_rem_if_exist(T_TIMEOUT* t);
```

**概要:** タイムアウトチェインに要素が存在する場合のみ削除する (条件付き削除)。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `t` | `T_TIMEOUT*` | 削除を試みるタイムアウト要素 |

**戻り値:** なし (void)

**処理内容:**
1. 自己参照チェック: `t->next == t` なら即座に return (キューに入っていない)
2. キューに入っている場合は `sched_timeout_rem(t)` を呼んで削除

**計算量:** O(1) (自己参照規約により走査不要)

**前提条件:** caller holds `kernel_lk`

**呼び出し元:** `sys_wup_tsk()`, `sys_psnd_dtq()`, `ipsnd_dtq()`, `sys_sig_sem()` 等、タスクがタイムアウトキューに入っているか不明な場面での安全な削除

**注意点:** `tlink` の自己参照規約 (`t->next == t` = キュー外) により、`sched_timeout_rem()` を直接呼ぶ場合と異なり、キューに含まれていなくても安全。

---

### sched_timeout

```c
void sched_timeout(W apic, unsigned long delta);
```

**概要:** タイマーティックごとに呼ばれ、タイムアウトしたタスクを WAI から RDY に遷移させる。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `apic` | `W` | 呼び出し元 CPU の APIC ID |
| `delta` | `unsigned long` | 経過時間 (ティック数) |

**戻り値:** なし (void)

**処理内容:**
1. チェインの先頭要素のデルタ値から `delta` を減算
2. デルタ値が 0 以下になった **全エントリ** をループで処理:
   a. エントリをリストから除去 (残余デルタを次エントリに加算して伝播)
   b. エントリを自己参照にリセット
   c. `tlink2tsk()` マクロで T_TSK ポインタを取得
   d. タスクが TTS_WAI であれば:
      - オブジェクト待ちキュー (sem/flg/dtq) に入っていれば `wlink_rem()` で除去
      - `proc_set_return_value()` で `E_TMOUT` をタスクの EAX にセット
      - TTS_RDY に遷移、`sched_ins()` でレディキューに挿入
   e. タスクが TTS_WAI 以外なら (孤児エントリ): 除去のみ、タスク状態は変更しない
3. 1 つ以上のタスクを起床した場合、`sched_next_tsk()` でリスケジュール要求

**前提条件:** caller holds `kernel_lk`

**呼び出し元:** タイマー割り込みハンドラ (PIT IRQ0 または APIC タイマー) から `timer_intr()` 経由

**注意点:**
- 同一ティックで複数エントリが同時に満了しうる (例: delta=0 で挿入されたエントリ)。ループで全件処理する
- TTS_WAI 以外の孤児エントリ (タスク状態が既に変更された等) は除去のみ行い、タスク状態は変更しない
- `E_TMOUT` は `proc_set_return_value()` でカーネルスタック上の `pt_regs.eax` に書き込まれ、`RESTORE_ALL` + `iret` でタスクに返される

## 補足

### Big Kernel Lock (BKL) 戦略

スケジューラを含む全カーネルデータ構造は、単一のスピンロック `kernel_lk` (Big Kernel Lock) で保護される。

- `sched_do_next_tsk()` のみが自身で `kernel_lk` を取得/解放する (`intr_leave` 経由で呼ばれるため)
- その他の関数 (`sched_ins`, `sched_rem`, `sched_hold_tsk`, `sched_timeout` 等) は呼び出し元 (`c_intr_syscall`, `c_intr_irq0`, `c_intr_irq1`, `c_intr_smp_timer1`) が `kernel_lk` を取得済みであることを前提とする
- `kernel_lk` は `xchgl` ベースのスピンロック (`smp_lock`/`smp_unlock`) であり、Ring 3 からも呼び出し可能。`ccli`/`csti` は不要
- VGA 出力の保護には別ロック `video_lk` を使用する (ISR が `kernel_lk` 保持中に `printk` を呼ぶため、`kernel_lk` を再取得するとデッドロックする)

### CPU アフィニティの仕組み

`sched_do_next_tsk()` はタスク選択時に `proc[tskid].cpu == apic` をチェックする。`proc_t.cpu` フィールドは以下のように設定される:

- Task 1, Task 5: cpu = 0 (BSP)
- Task 2, Task 6: cpu = 1 (AP)
- `proc_create()` で動的に作成されたタスク: 作成元 CPU の APIC ID を継承

### next_tsk_flag の役割

`next_tsk_flag[]` はリスケジュールの要求/応答メカニズムを提供する:

1. システムコールや割り込みがタスク状態を変更
2. `sched_next_tsk()` が両 CPU の `next_tsk_flag` を 1 にセット
3. 次の割り込み (APIC タイマー等) の restore 処理で `sched_next_tsk_check()` がフラグを確認
4. フラグが 1 なら `sched_do_next_tsk()` を呼び出してタスク切り替え
5. フラグを 0 にクリア
