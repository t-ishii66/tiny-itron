# タイムアウト処理

タイムアウト付き待ちサービスコール (`tslp_tsk`, `trcv_dtq`, `twai_sem` 等) の
内部メカニズムを解説する。

## 概要

ITRON 仕様では、待ちサービスコールに TMO (タイムアウト値) を指定できる。
指定ティック数が経過しても待ち条件が成立しない場合、`E_TMOUT` を返してタスクを
起床する。本カーネルでは **デルタ符号化二重連結リスト** でタイムアウトを管理する。

## 割り込みからタスク復帰までの全体フロー

タイムアウト処理は「データ構造の操作」だけでは完結しない。ハードウェア割り込みが
発火し、タイムアウトキューを処理し、タスクが Ring 3 で再開するまでの一連の流れを
理解する必要がある。

### タイマー割り込みの発生源

各 CPU には複数のタイマーがあるが、`sched_timeout` を呼ぶのは 1 つだけ:

| CPU | タイマー | ベクタ | 役割 |
|-----|---------|--------|------|
| CPU 0 | PIT (i8253) | IRQ0 (0x20) | **システムティック**: ~17ms 周期 (HZ=60)。`timer_intr(0,1)` → `sched_timeout(0,1)` を呼ぶ。タイムアウトキューの delta 減算を行う唯一のタイマー |
| CPU 0 | APIC タイマー | 0xFD | **タスクスイッチ契機のみ**: EOI だけ。`intr_leave` を通るので `next_tsk_flag` チェックの機会になる |
| CPU 1 | APIC タイマー | 0xFE | **タスクスイッチ契機のみ**: EOI だけ。CPU 1 には PIT が来ないため、これがプリエンプティブスイッチの唯一の契機 |

> **注**: CPU 0 の APIC タイマーは実質的に不要である。PIT (IRQ0) がすでに
> CPU 0 に周期割り込みを提供しており、`intr_leave` を通るためプリエンプション
> 契機も兼ねている。CPU 1 の APIC タイマーは PIT が届かない CPU 1 にとって
> 不可欠だが、CPU 0 側は PIT だけで足りる。現状は両 CPU とも APIC タイマーを
> 有効にしているだけで、深い設計意図はない。

**重要**: `sched_timeout` を呼ぶのは **CPU 0 の PIT だけ** である。タイムアウト
キューは全 CPU で共有 (`kernel_lk` で排他) だが、delta の減算は PIT の ~17ms 周期
のみに基づく。もし複数のタイマーが `sched_timeout` を呼ぶと、delta が二重に
減算されてタイムアウト時間が不正確になる。

APIC タイマーの役割は、プリエンプティブなタスクスイッチの契機を提供すること。
`sched_timeout` や `sched_next_tsk` が `next_tsk_flag` をセットした後、
次の APIC タイマー割り込みで `intr_leave` がそのフラグを検出し、
`sched_do_next_tsk` でタスクスイッチを実行する。

### コールチェイン (CPU 0 の PIT を例に)

```
PIT 発火 (~17ms 周期, HZ=60)
  │
  ▼
CPU: Ring 3 のタスクを中断、SS/ESP/EFLAGS/CS/EIP を per-task カーネルスタックに push
  │
  ▼
intr_irq0 (intr.s):
  SAVE_ALL            ← EAX〜ES の 9 レジスタをカーネルスタックに push (pt_regs 完成)
  call intr_enter     ← k_nest[0]++
  call c_intr_irq0    ← C ハンドラへ
  │
  ▼
c_intr_irq0 (interrupt.c):
  smp_lock(&kernel_lk)   ← Big Kernel Lock 取得
  timer_intr(0, 1)       ← タイマー共通処理
  │  │
  │  ▼
  │  timer_intr (timer.c):
  │    timer_ticks++; 画面更新
  │    sched_timeout(0, 1)   ← ★ タイムアウトキュー処理 (後述)
  │       │
  │       ├─ 満了エントリがあれば:
  │       │    tskstat = TTS_RDY, sched_ins(), proc_set_return_value(E_TMOUT)
  │       │    sched_next_tsk()  ← next_tsk_flag[0]=1, next_tsk_flag[1]=1
  │       │
  │       └─ 満了エントリがなければ: 何もしない
  │
  i8259_reenable()
  smp_unlock(&kernel_lk)     ← ロック解放
  │
  ▼
intr_return (intr.s):
  call intr_leave            ← k_nest[0]--, 0 に達したらタスクスイッチ判定
  │
  ▼
intr_leave (intr.s):                    ※ k_nest[0] == 0 の場合のみ
  ① ESP を current_proc[0]->kern_esp に保存
  ② sched_next_tsk_check(0) を呼ぶ
  │    └─ next_tsk_flag[0] が 1 なら sched_do_next_tsk(0):
  │         優先度キューから最高優先度の TTS_RDY タスクを選び、
  │         current_proc[0] を更新
  ③ (新しい) current_proc[0]->kern_esp を ESP にロード
  ④ tss_update_esp0() で TSS.esp0 を更新
  ret                        ← 新 ESP 上の return address を pop
  │
  ▼
intr_return_restore (intr.s):
  RESTORE_ALL   ← 新タスクのカーネルスタックから ES,DS,EDI,...,EAX を pop
                   (EAX には E_TMOUT が入っている)
  iret          ← EIP,CS,EFLAGS,ESP,SS を pop → Ring 3 に復帰
  │
  ▼
タスクが Ring 3 で再開。EAX = E_TMOUT (-50)。
syscall ラッパー (lib/lib_sem.c 等) が EAX を関数の返却値として返す。
```

### ポイント

**sched_timeout は「起床予約」しかしない**。`sched_timeout` が行うのは
タスクの状態を TTS_RDY に変え、スケジューラキューに挿入し、
`next_tsk_flag` を立てることだけ。実際の CPU レジスタ切り替え (ESP のスワップ)
は `intr_leave` で行われる。この分離により、タイマー ISR 内でレジスタ保存の
整合性を気にする必要がない。

**E_TMOUT はカーネルスタック上に書き込まれる**。タスクがブロック中、
そのカーネルスタックには `SAVE_ALL` で保存された `pt_regs` フレームがある。
`proc_set_return_value(proc, E_TMOUT)` は `proc->kern_esp + 4` の位置にある
`pt_regs.eax` に `E_TMOUT` を書き込む。後で `RESTORE_ALL` がこの値を
EAX レジスタに pop する。

**CPU 1 でのタスクスイッチ**。CPU 1 の APIC タイマーは `sched_timeout` を
呼ばないが、`intr_leave` の CPU 1 パスで `next_tsk_flag[1]` をチェックする。
CPU 0 の PIT ハンドラ内で `sched_next_tsk` が `next_tsk_flag[1]=1` をセット
していれば、CPU 1 の次の APIC タイマー割り込みの帰り道でタスクスイッチが起きる。

## デルタ符号化リスト

タイムアウトキューは `T_TIMEOUT` 構造体の二重連結リストで、番兵ノード `timeout`
をヘッドとする。各エントリの `delta` フィールドは **前エントリからの相対ティック数**
を保持する。

```
timeout (番兵)
  ↓
[delta=5] → [delta=3] → [delta=10] → timeout
```

この例の絶対満了ティック:
- 1 番目: 5 ティック後
- 2 番目: 5+3 = 8 ティック後
- 3 番目: 5+3+10 = 18 ティック後

利点: 毎ティックの処理は先頭エントリの delta を 1 減算するだけで済む。
全エントリを走査する必要がない。

## T_TIMEOUT 構造体

```c
typedef struct timeout {
    struct timeout*  prev;
    struct timeout*  next;
    TMO              delta;   /* 前エントリからの相対ティック数 */
} T_TIMEOUT;
```

各 `T_TSK` 構造体内に `tlink` フィールドとして埋め込まれている。
`tlink2tsk()` マクロでタイムアウトエントリからタスク構造体を逆算できる。

## 挿入: sched_timeout_ins

`sched_timeout_ins(T_TIMEOUT* t)` はデルタ値をデコードしながらリストを走査し、
適切な位置に挿入する。

**アルゴリズム** (`kernel/sched.c`):

1. 先頭から順に走査。現在の `tp->delta` が挿入エントリの `t->delta` 以上なら、
   そこに挿入する
2. 走査中、通過した各エントリの delta を `t->delta` から減算していく
3. 挿入位置が決まったら、**直後のエントリだけ** `tp->delta -= t->delta` で調整する。
   それ以降のエントリは `tp` からの相対値なので変更不要
4. `tp->prev` の直後に `t` をリンクする

```
挿入前: [5] → [3] → [10]    挿入: delta=7 のエントリ

走査: tp=[5], 7 >= 5 → t->delta = 7-5 = 2, 次へ
走査: tp=[3], 2 < 3  → ここに挿入
      tp 以降を調整: [3] → [3-2=1], [10] → そのまま

結果: [5] → [2] → [1] → [10]
       絶対値: 5, 7, 8, 18 ✓
```

## 毎ティック処理: sched_timeout

`sched_timeout(W apic, unsigned long delta)` は PIT タイマー (IRQ0) および
APIC タイマーの ISR から `delta=1` で呼ばれる。

**処理フロー**:

1. 先頭エントリの `delta` を 1 減算
2. `delta <= 0` になった全エントリをループで処理:
   a. エントリをリストから除去 (残余 delta を次エントリに加算)
   b. タスクが TTS_WAI なら:
      - オブジェクト待ちキュー (sem/flg/dtq) に入っていれば `wlink_rem` で除去
      - `proc_set_return_value` で `E_TMOUT` をタスクの EAX にセット
      - TTS_RDY に遷移、スケジューラキューに挿入
   c. タスクが TTS_WAI 以外なら (孤児エントリ): 除去のみ、タスク状態は変更しない
3. 1 つ以上のタスクを起床した場合、`sched_next_tsk` でリスケジュール

**重要**: 同じティックで複数エントリが満了することがある。
例えば `dly_tsk(5)` と `trcv_dtq(dtq, &data, 5)` が同時に登録された場合、
2 番目のエントリは `delta=0` で挿入される。5 ティック後に両方とも満了する。

## 除去: sched_timeout_rem / sched_timeout_rem_if_exist

タイムアウト前に待ち条件が成立した場合 (例: `wup_tsk` や `snd_dtq` で起床)、
タイムアウトエントリを除去する必要がある。

**デルタ補正が必須**: 除去するエントリの delta を次エントリに加算しないと、
後続エントリの絶対満了時刻がずれる。

```
除去前: [5] → [2] → [10]
        絶対値: 5, 7, 17

[2] を除去する場合:
  次の [10] に delta=2 を加算 → [10+2=12]

除去後: [5] → [12]
        絶対値: 5, 17 ✓  (7 ティック目のエントリが消え、17 は保存)
```

`sched_timeout_rem` は直接ポインタで除去し、tlink を自己参照にリセットする。
`sched_timeout_rem_if_exist` は tlink の自己参照チェック (`t->next == t`) で
キュー内か判定し、入っていれば `sched_timeout_rem` を呼ぶ (O(1))。
後者は `sys_wup_tsk` や `sys_psnd_dtq` など、タスクがタイムアウトキューに
入っているか不明な場合に使う。

## タイムアウト付き待ちの動作例: trcv_dtq

`trcv_dtq(dtqid, &data, 20)` を呼んだ場合:

### 正常起床パス (データ到着)

1. `sys_trcv_dtq` が DTQ リングバッファを確認 → 空
2. タスクの wlink を DTQ 受信待ちキューに挿入 (`ins_fifo`)
3. `sys_tslp_tsk(apic, 20)` を呼び出し:
   - tlink.delta = 20 に設定
   - plink をスケジューラキューから除去
   - tskstat = TTS_WAI
   - `sched_timeout_ins` でタイムアウトキューに挿入
   - `sched_next_tsk` でリスケジュール
4. 別タスクが `snd_dtq` → データ到着:
   - `sys_psnd_dtq` / `ipsnd_dtq` が DTQ 受信待ちキューからタスクの wlink を除去
   - `wlink_rem` で wlink を自己参照にリセット
   - tskstat = TTS_RDY、plink をスケジューラキューに挿入
   - `proc_set_return_value(proc, E_OK)` で返却値を設定
5. `sched_timeout_rem_if_exist` がタイムアウトキューから tlink を除去
6. タスクが TTS_RDY になったことで、次のタイマー割り込みの `intr_leave` で
   `sched_do_next_tsk` がこのタスクを選択。ESP がこのタスクのカーネルスタックに
   スワップされ、`RESTORE_ALL` が `pt_regs` (EAX=E_OK) を pop、`iret` で Ring 3 復帰。
   `trcv_dtq` は `E_OK` を返す

### タイムアウト起床パス

1-3. 正常起床パスと同じ
4. 20 ティック経過してもデータが到着しない
5. PIT タイマー割り込み → `c_intr_irq0` → `timer_intr(0,1)` → `sched_timeout(0,1)`:
   - 先頭エントリの delta が 0 に到達
   - wlink が自己参照でなければ DTQ 受信待ちキューから `wlink_rem` で除去
   - `proc_set_return_value(proc, E_TMOUT)` でカーネルスタック上の `pt_regs.eax` に
     `E_TMOUT` を書き込む
   - tskstat = TTS_RDY、plink をスケジューラキューに挿入
   - `sched_next_tsk` で `next_tsk_flag` をセット
6. 同じ割り込みの帰り道: `intr_return` → `intr_leave`:
   - k_nest が 0 に達し、`sched_next_tsk_check` → `sched_do_next_tsk` が
     TTS_RDY のタスクを選択
   - ESP がこのタスクのカーネルスタックにスワップされる
   - `RESTORE_ALL` が `pt_regs` を pop (EAX = E_TMOUT)
   - `iret` で Ring 3 に復帰
   - syscall ラッパーが EAX を返却値として返す → `trcv_dtq` は `E_TMOUT` を返す

## E_TMOUT の返却メカニズム

タイムアウトで起床されたタスクに `E_TMOUT` を返す仕組み:

1. タスクがブロック中、カーネルスタックには `SAVE_ALL` で保存された `pt_regs` がある
2. `proc_set_return_value(proc, E_TMOUT)` が `pt_regs.eax` に `E_TMOUT (-50)` を書き込む
3. タスクが `RESTORE_ALL` + `iret` で再開すると、EAX に E_TMOUT がロードされる
4. syscall ラッパー (`lib/lib_sem.c`) が EAX を返却値として返す

## 自己参照規約 (wlink / tlink)

wlink と tlink は同じ自己参照規約を使い、キューに入っているか O(1) で判定する。

| フィールド | 用途 | 自己参照の意味 |
|-----------|------|---------------|
| `T_TSK.wlink` | オブジェクト待ちキュー (sem/flg/dtq) | どの待ちキューにも入っていない |
| `T_TSK.tlink` | タイムアウトキュー | タイムアウト待ちしていない |

- **自己参照** (`x.next == &x`): キューに入っていない
- **非自己参照**: いずれかのキューに接続中

`tsk_init` で両方を自己参照に初期化する。除去時にも自己参照にリセットする
(`wlink_rem`、`sched_timeout_rem`)。

`sched_timeout` は wlink が自己参照かどうかをチェックし、接続中の場合のみ
`wlink_rem` を呼ぶ。`sched_timeout_rem_if_exist` は tlink が自己参照かどうかで
キュー内か判定する。これにより、正常起床後にタイムアウトが到達しても
二重除去にならない。

## TMO の単位

ITRON 仕様では TMO はミリ秒だが、本実装ではティック数をそのまま使う。
PIT の 1 ティック ≈ 17ms (HZ=60) なので、TMO=20 は約 0.33 秒に相当する。

将来的にはティック→ミリ秒変換を入れるべきだが、教育用途では
ティック数の方が動作の理解が容易である。

## 関連ドキュメント

- [コンテキストスイッチ](context-switch.md) — SAVE_ALL/RESTORE_ALL、pt_regs フレーム
- [タイマー割り込み](timer-interrupt.md) — PIT/APIC タイマーと sched_timeout の呼び出し
- [syscall](syscall.md) — システムコールの呼び出し規約と返却値
- [ITRON ガイド](itron-guide.md) — ITRON 仕様のタスク状態遷移
