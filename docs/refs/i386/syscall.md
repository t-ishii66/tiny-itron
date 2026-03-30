# syscall.c

対象ファイル: `i386/syscall.c`

## 概要

i386 レイヤのシステムコールディスパッチャ。ユーザーモード (Ring 3) から
`int 0x99` で発行されたシステムコールを受け取り、ITRON カーネル層の
`itron_syscall()` に転送する。

システムコールの戻り値は、プロセスの現在アクティブな `save` フレーム内の
EAX スロットに直接書き込まれる。これにより、`restore` マクロがフレームから
レジスタを復元する際に、戻り値が EAX レジスタに正しくロードされる。

## 定数・マクロ

なし。

## 構造体・型

なし。

## グローバル変数

なし (他モジュールで定義された `current_proc[]` を参照)。

## 関数リファレンス

### c_intr_syscall

```c
W c_intr_syscall(unsigned long apic, unsigned long sysid,
                 unsigned long arg1, unsigned long arg2, unsigned long arg3,
                 unsigned long arg4, unsigned long arg5, unsigned long arg6,
                 unsigned long arg7)
```

**概要:** システムコールの C 言語側エントリポイント。アセンブリ側エントリ (`intr_syscall` in `intr.s`) から呼び出される。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `apic` | `unsigned long` | APIC ID (正規化されて 0 または 1 になる) |
| `sysid` | `unsigned long` | システムコール番号 |
| `arg1` - `arg7` | `unsigned long` | システムコール引数 (最大 7 個) |

**戻り値:** `W` -- システムコールの戻り値 (エラーコードまたは結果)。

**処理内容:**

1. **APIC インデックスの正規化:**
   - `apic` が非ゼロの場合 1 に設定。APIC ID は環境により 0 以外の値を取る可能性があるため、0/1 に正規化。

2. **カーネル層への転送:**
   - `itron_syscall(apic, sysid, arg1, ..., arg7)` を呼び出し、ITRON カーネル層でシステムコールを処理。

3. **戻り値の書き込み:**
   - `current_proc[apic]->stack - 20` のアドレスに戻り値 (`ret`) を書き込む。

**呼び出し元:** `intr_syscall` (アセンブリ側、`intr.s`)

**注意点:**

- **戻り値の書き込み位置の計算:**
  - `save`/`restore` は `proc.reg[]` をスタックとして使用する
  - `save` は 13 スロット (52 バイト) のフレームを書き込み、`proc.stack` を 52 バイト進める
  - フレーム内の EAX スロットはフレーム先頭から 32 バイトのオフセット (スロット 8)
  - したがって、現在のアクティブフレームの EAX = `proc.stack - 52 + 32` = `proc.stack - 20`
  - `proc.reg[EAX]` (= `reg[8]`) に書き込むと初期フレームの EAX にしか書き込めず、ネストした `save` の後は `restore` が読み取るフレームとは一致しない

```
  reg[] のレイアウト:

  reg[0]:  ECX   ← 初期フレーム
  reg[1]:  EDX
  reg[2]:  ESP
  ...
  reg[8]:  EAX   ← proc.reg[EAX] はここ (初期フレーム)
  ...
  reg[12]: old_stack
  reg[13]: ECX   ← 2番目のフレーム (1回目の save 後)
  ...
  reg[21]: EAX   ← restore が読むのはここ
  ...
  reg[25]: old_stack
           ↑
       stack が指す位置

  stack - 20 = reg[21] のアドレス = 正しい EAX スロット
```

- システムコールの処理中にタスクスイッチが発生する場合がある (`slp_tsk` など)。その場合、`itron_syscall()` 内で `sched_do_next_tsk()` が呼ばれ、`current_proc[apic]` が更新される。戻り値は新しい (切り替え先の) タスクではなく、元のタスクの `save` フレームに書き込まれるべきだが、`current_proc` は既に更新されている可能性がある点に注意。

## 補足

### システムコール呼び出しフロー

```
ユーザータスク (Ring 3):
  システムコールラッパー (lib/)
    → int 0x99 (ソフトウェア割り込み)

CPU (Ring 3 → Ring 0 遷移):
  → TSS の ESP0/SS0 でカーネルスタックに切り替え
  → IDT[0x99] → intr_syscall (intr.s)

intr_syscall (アセンブリ):
  → save マクロ (レジスタ保存)
  → APIC ID 読み取り
  → c_intr_syscall(apic, sysid, arg1, ..., arg7)
  → restore マクロ (レジスタ復元)
  → iret (Ring 0 → Ring 3 復帰)
```

### システムコール番号

システムコール番号 (`sysid`) は `include/syscall.h` で定義されている。
主なシステムコール:

| 番号 | 名前 | 機能 |
|------|------|------|
| - | `cre_tsk` | タスク生成 |
| - | `act_tsk` | タスク起動 |
| - | `slp_tsk` | タスク休止 |
| - | `wup_tsk` | タスク起床 |
| - | `cre_sem` | セマフォ生成 |
| - | `wai_sem` | セマフォ待ち |
| - | `sig_sem` | セマフォ返却 |
