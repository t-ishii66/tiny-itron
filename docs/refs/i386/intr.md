# intr.s

対象ファイル: `i386/intr.s`

## 概要

割り込みハンドリングの中核を担うアセンブリファイル。カーネルで最も重要なファイルである。以下の機能を提供する:

1. **レジスタ退避 (`save`)**: 割り込み発生時に現在のプロセスのレジスタをプロセス構造体の `reg[]` 配列に退避する。APIC ID を読み取って CPU を判定し、per-CPU の `current_proc[]` を使用する
2. **レジスタ復帰 (`restore`)**: 割り込みからの復帰時にレジスタを復帰する。ネストカウンタ (`k_nest`) が 0 になった場合にスケジューラを呼び出し、タスク切り替えを行う
3. **ユーザーモード復帰 (`user_restore`)**: ユーザーモードへの特殊な復帰処理
4. **CPU 例外ハンドラ**: 除算エラー、一般保護例外など
5. **IRQ ハンドラ**: PIC (i8259) 経由の外部割り込み (IRQ0-15)
6. **APIC タイマーハンドラ**: per-CPU の APIC タイマー割り込み
7. **システムコールハンドラ**: `int 0x99` によるシステムコールエントリ

## 定数・マクロ

| 名前 | 値 | 説明 |
|------|------|------|
| `APIC_ID_REG` | `0xFEE00020` | Local APIC の ID レジスタのアドレス。ビット 24-31 に APIC ID が格納される |

## グローバル変数

### k_nest0, k_nest1

```asm
k_nest0:  .long  0
k_nest1:  .long  0
```

**概要:** per-CPU の割り込みネストカウンタ。`save` でインクリメント、`restore` でデクリメントされる。`k_nest == 0` の場合のみ `restore` でスケジューラを呼び出す。

## 外部シンボル参照

| シンボル | 定義元 | 説明 |
|---------|--------|------|
| `current_proc` | proc.c | per-CPU の現在実行中プロセスポインタ配列 (`proc_t* current_proc[2]`) |
| `sleep_current_proc` | proc.c | 現在プロセスをスリープ状態にする関数 |
| `ena_tex` | kernel | タスク例外処理の有効化 |
| `sched_next_tsk_check` | sched.c | 次タスクへの切り替えチェック (タスクスイッチの判定と実行) |

## ラベル・ルーチンリファレンス

### save

```asm
save:
    pushl  %ebx
    pushl  %eax
    movl   APIC_ID_REG, %eax
    shrl   $24, %eax
    testl  %eax, %eax
    jnz    save_cpu1
```

**概要:** 割り込み発生時のレジスタ退避ルーチン。現在の CPU を APIC ID で判定し、対応する `current_proc[cpu]` のプロセス構造体の `reg[]` スタックに 13 個のレジスタを退避する。

**処理内容:**

1. EBX と EAX をスタックに一時退避する (後で reg[] に保存するため)
2. `APIC_ID_REG` (0xFEE00020) を読み取り、ビット 24 以降をシフトして APIC ID を取得する
3. APIC ID が 0 なら CPU 0 パス (`save_cpu0`)、非 0 なら CPU 1 パス (`save_cpu1`) に分岐する
4. CPU に応じた `current_proc[cpu]` のアドレスを EBX にロードする
5. 対応する `k_nest` カウンタをインクリメントする
6. `save_common` でレジスタ退避を実行する:
   - プロセス構造体の `stack` フィールド (先頭 4 バイト) に格納されている現在のレジスタスタックポインタを読み出す
   - `stack` ポインタを 52 バイト進める (13 レジスタ x 4 バイト)
   - 旧ポインタが指す位置に 13 個のレジスタを保存する

**退避されるレジスタ (13 個、計 52 バイト):**

| オフセット | レジスタ | 取得元 |
|-----------|---------|--------|
| 0 | ECX | レジスタ直接 |
| 4 | EDX | レジスタ直接 |
| 8 | ESP | 割り込みフレーム `24(%esp)` (Ring 3→0 遷移時にハードウェアが退避した ESP) |
| 12 | EIP | 割り込みフレーム `12(%esp)` (ハードウェアが退避した戻りアドレス) |
| 16 | EBP | レジスタ直接 |
| 20 | ESI | レジスタ直接 |
| 24 | EDI | レジスタ直接 |
| 28 | EFLAGS | `pushfl` / `popl` で取得 |
| 32 | EAX | スタック `(%esp)` (冒頭で退避した値) |
| 36 | EBX | スタック `4(%esp)` (冒頭で退避した値) |
| 40 | DS | セグメントレジスタ直接 |
| 44 | ES | セグメントレジスタ直接 |
| 48 | old_stack | 前回の `stack` ポインタ値 (ネスト復帰用) |

**呼び出し元:** 全ての割り込みハンドラ (`intr_irq0` 〜 `intr_irq15`, `intr_divide`, `intr_default`, `intr_general`, `intr_smp_timer0`, `intr_smp_timer1`, `intr_syscall`)

**注意点:**

- ESP の取得は `24(%esp)` を参照する。これは Ring 3→Ring 0 の特権レベル遷移時にハードウェアが SS と ESP をスタックにプッシュすることを前提としている。Ring 0→Ring 0 の割り込みでは SS/ESP がプッシュされないため、`save` は Ring 0 での割り込みでは正しく動作しない
- `save` は `call` で呼ばれるため、リターンアドレスがスタックにプッシュされている。この点もスタックオフセットの計算に影響する

### save_cpu0

```asm
save_cpu0:
    leal  current_proc, %ebx
    movl  k_nest0, %eax
    incl  %eax
    movl  %eax, k_nest0
    jmp   save_common
```

**概要:** CPU 0 用の save パス。`current_proc[0]` と `k_nest0` を使用する。

### save_cpu1

```asm
save_cpu1:
    leal  current_proc+4, %ebx
    movl  k_nest1, %eax
    incl  %eax
    movl  %eax, k_nest1
```

**概要:** CPU 1 用の save パス。`current_proc[1]` と `k_nest1` を使用する。

### save_common

```asm
save_common:
    movl  (%ebx), %ebx          # ebx = current_proc[cpu]
    pushl (%ebx)                 # old stack ptr を退避
    movl  (%ebx), %eax
    addl  $52, %eax
    movl  %eax, (%ebx)          # stack += 52 (新しい退避フレームを確保)
    popl  %ebx                   # ebx = old stack ptr (退避先アドレス)
    movl  %ecx, (%ebx)          # ECX を退避
    ...
```

**概要:** CPU 共通のレジスタ退避処理。

**処理内容:**

1. `current_proc[cpu]` のポインタをデリファレンスしてプロセス構造体のアドレスを取得する
2. プロセス構造体の先頭 (= `stack` フィールド) から現在のレジスタスタックポインタを読み出す
3. `stack` を 52 バイト加算して新しいフレームを確保する
4. 旧ポインタが指す位置に 13 個のレジスタを順次保存する
5. DS と ES をカーネルデータセグメント (SEL_K32_D = 0x28) にリロードする。CPU は Ring 3→Ring 0 遷移時に CS と SS を自動切り替えするが DS/ES は変更しないため、save で明示的にリロードする
6. 退避完了後、冒頭でスタックに退避した EAX と EBX を復帰して `ret` する

### restore

```asm
restore:
    movl  APIC_ID_REG, %eax
    shrl  $24, %eax
    testl %eax, %eax
    jnz   restore_cpu1
```

**概要:** 割り込みからの復帰ルーチン。レジスタを復帰し、必要に応じてタスクスイッチを行う。

**処理内容:**

1. APIC ID を読み取って CPU を判定する
2. CPU に応じた `k_nest` カウンタをデクリメントする
3. `k_nest` が 0 になった場合 (最外の割り込みから復帰する場合):
   - `sched_next_tsk_check(apic)` を呼び出す (引数: CPU 番号 0 または 1)
   - 戻り値が非 0 の場合、タスクスイッチが発生したため `current_proc[cpu]` を再ロードする
4. プロセス構造体の `stack` ポインタを 52 バイト減算して退避フレームに戻す
5. 13 個のレジスタを順次復帰する:
   - ECX, EDX, EBP, ESI, EDI: レジスタに直接復帰
   - EFLAGS: IF ビット (ビット 9) をクリアしてから `popfl` で復帰する。これにより、復帰処理中にタイマー割り込みが発生することを防ぐ。実際の EFLAGS は `iret` で割り込みフレームからロードされる
6. `k_nest` が 0 の場合 (最外レベル) のみタスクスイッチ処理を実行する:
   - ESP を割り込みフレームの対応位置 (`16(%esp)`) に書き戻す
   - EIP を割り込みフレームの対応位置 (`4(%esp)`) に書き戻す
   - これにより `iret` 実行時に新しいタスクの EIP/ESP に遷移する
7. EAX, EBX, DS, ES を復帰して `ret` する

**呼び出し元:** 全ての割り込みハンドラ (`save` の後に呼ばれる)

**注意点:**

- EFLAGS 復帰時に IF ビットをクリアする (`andl $0xfffffdff, %eax`) ことで、復帰のクリティカルセクション中の割り込みを防止する
- タスクスイッチ時の ESP/EIP 書き戻しは、`iret` が参照するスタック上の割り込みフレームを直接書き換えることで実現する

### restore_cpu0

```asm
restore_cpu0:
    leal  current_proc, %ebx
    movl  k_nest0, %eax
    decl  %eax
    movl  %eax, k_nest0
    ...
    pushl $0              # apic = 0
    call  sched_next_tsk_check
    addl  $4, %esp
```

**概要:** CPU 0 用の restore パス。`k_nest0` をデクリメントし、ネストレベル 0 で `sched_next_tsk_check(0)` を呼び出す。

### restore_cpu1

```asm
restore_cpu1:
    leal  current_proc+4, %ebx
    movl  k_nest1, %eax
    decl  %eax
    movl  %eax, k_nest1
    ...
    pushl $1              # apic = 1
    call  sched_next_tsk_check
    addl  $4, %esp
```

**概要:** CPU 1 用の restore パス。`k_nest1` をデクリメントし、ネストレベル 0 で `sched_next_tsk_check(1)` を呼び出す。

### user_restore

```asm
user_restore:
    addl  $8, %esp
    call  ena_tex
    popfl
    popl  %edi
    popl  %esi
    popl  %edx
    popl  %ecx
    popl  %ebx
    popl  %eax
    ret
```

**概要:** ユーザーモードへの復帰処理。`save` / `restore` を使用しない特殊なルーチン。

**処理内容:**

1. ESP を 8 バイト加算する (スタック上の不要なデータをスキップ)
2. `ena_tex()` を呼び出してタスク例外処理を有効化する
3. EFLAGS, EDI, ESI, EDX, ECX, EBX, EAX をスタックから順次復帰する
4. `ret` で呼び出し元に戻る

**呼び出し元:** カーネルからユーザーモードへ復帰する特定のパス

### intr_default

```asm
intr_default:
    call  save
    call  c_intr_default
    call  restore
    iret
```

**概要:** デフォルト割り込みハンドラ。未登録の割り込みベクタに対応する。

**処理内容:** `save` → `c_intr_default` (C 関数) → `restore` → `iret` の標準パターン。

**呼び出し元:** IDT で未設定の割り込みベクタ

### intr_divide

```asm
intr_divide:
    call  save
    call  c_intr_divide
    call  restore
    iret
```

**概要:** 除算エラー例外ハンドラ (INT 0)。

**処理内容:** `save` → `c_intr_divide` (C 関数) → `restore` → `iret` の標準パターン。

**呼び出し元:** CPU が除算エラーを検出した場合

### intr_singlestep

```asm
intr_singlestep:
    iret
```

**概要:** シングルステップ例外ハンドラ (INT 1)。何もせず `iret` で復帰する。

### intr_nmi

```asm
intr_nmi:
    iret
```

**概要:** NMI (Non-Maskable Interrupt) ハンドラ (INT 2)。何もせず `iret` で復帰する。

### intr_breakpoint

```asm
intr_breakpoint:
    iret
```

**概要:** ブレークポイント例外ハンドラ (INT 3)。何もせず `iret` で復帰する。

### intr_overflow

```asm
intr_overflow:
    iret
```

**概要:** オーバーフロー例外ハンドラ (INT 4)。何もせず `iret` で復帰する。

### intr_bounds

```asm
intr_bounds:
    iret
```

**概要:** 境界範囲超過例外ハンドラ (INT 5)。何もせず `iret` で復帰する。

### intr_opcode

```asm
intr_opcode:
    iret
```

**概要:** 無効オペコード例外ハンドラ (INT 6)。何もせず `iret` で復帰する。

### intr_copr_not_available

```asm
intr_copr_not_available:
    iret
```

**概要:** コプロセッサ使用不可例外ハンドラ (INT 7)。何もせず `iret` で復帰する。

### intr_doublefault

```asm
intr_doublefault:
    iret
```

**概要:** ダブルフォルト例外ハンドラ (INT 8)。エラーコードを破棄して `iret` で復帰する。

**注意点:** ベクタ 8 はエラーコード付き例外。`addl $4, %esp` でエラーコードを破棄してから `iret` する。

### intr_copr_seg_overrun

```asm
intr_copr_seg_overrun:
    iret
```

**概要:** コプロセッサセグメントオーバーラン例外ハンドラ (INT 9)。何もせず `iret` で復帰する。

### intr_tss

```asm
intr_tss:
    addl  $4, %esp
    iret
```

**概要:** 無効 TSS 例外ハンドラ (INT 10)。エラーコードを破棄して `iret` で復帰する。

### intr_segment_not_present

```asm
intr_segment_not_present:
    addl  $4, %esp
    iret
```

**概要:** セグメント不在例外ハンドラ (INT 11)。エラーコードを破棄して `iret` で復帰する。

### intr_stack

```asm
intr_stack:
    addl  $4, %esp
    iret
```

**概要:** スタック例外ハンドラ (INT 12)。エラーコードを破棄して `iret` で復帰する。

### intr_general

```asm
intr_general:
    popl  gp_error_code        # エラーコードを保存
    call  save
    call  c_intr_general
    call  restore
    iret
```

**概要:** 一般保護例外ハンドラ (INT 13)。エラーコードを `gp_error_code` グローバル変数に保存してから `save` → `c_intr_general` → `restore` → `iret` を実行する。

**処理内容:**

1. スタック上のエラーコードを `gp_error_code` に pop する (エラーコード除去 + 保存を同時に実行)
2. `save` でレジスタ退避
3. `c_intr_general` で診断情報を出力して停止する
4. `restore` → `iret` (通常は c_intr_general が hlt で停止するため到達しない)

**呼び出し元:** Ring 3 から `cli`/`sti` を実行した場合、無効なセグメントアクセスなど

**注意点:** エラーコードは `save` の前に除去する必要がある。エラーコードがスタックに残っていると `save` が読み取るフレームレイアウト (EIP, CS, EFLAGS, ESP, SS) がずれて正しく保存できない。

### intr_page

```asm
intr_page:
    addl  $4, %esp
    call  save
    call  c_intr_page
    call  restore
    iret
```

**概要:** ページフォルト例外ハンドラ (INT 14)。エラーコードを破棄してから `save` → `c_intr_page` → `restore` → `iret` を実行する。

**処理内容:**

1. `addl $4, %esp` でエラーコードを破棄する
2. `save` でレジスタ退避
3. `c_intr_page` でフォルトアドレス (CR2) を読み取り処理する
4. `restore` → `iret` で復帰する

**注意点:** ベクタ 8, 10, 11, 12, 13, 14 はエラーコード付き例外。エラーコードを `iret` 前に除去しないと、`iret` がエラーコードを EIP として解釈し、即座に再フォルト→ダブルフォルト→トリプルフォルトとなる。

### intr_copr_error

```asm
intr_copr_error:
    iret
```

**概要:** コプロセッサエラー例外ハンドラ (INT 16)。何もせず `iret` で復帰する。

### intr_irq0

```asm
intr_irq0:
    call  save
    call  c_intr_irq0
    call  restore
    iret
```

**概要:** IRQ 0 (PIT タイマー) ハンドラ。

**処理内容:** `save` → `c_intr_irq0` → `restore` → `iret` の標準パターン。

**呼び出し元:** PIT (Programmable Interval Timer) の周期割り込み (~17ms, HZ=60)

### intr_irq1

```asm
intr_irq1:
    call  save
    call  c_intr_irq1
    call  restore
    iret
```

**概要:** IRQ 1 (キーボード) ハンドラ。他の IRQ ハンドラと同じ標準パターン (`save` → C ハンドラ → `restore` → `iret`) を使用する。

**処理内容:**

1. `save` でレジスタ退避
2. `c_intr_irq1` でキーボード ISR を実行する (スキャンコード処理 + EOI)
3. `restore` でレジスタ復帰 (k_nest=0 ならタスクスイッチ検討)
4. `iret` で割り込みから復帰

**呼び出し元:** キーボードコントローラ (i8042) からの割り込み

**注意点:** 以前は `sti` でネスト割り込みを許可していたが、Ring 0→Ring 0 の割り込みでは CPU が SS/ESP をプッシュしないため `save` がゴミを読む問題があり、簡素化された。

### intr_irq2 〜 intr_irq15

```asm
intr_irqN:
    call  save
    call  c_intr_irqN
    call  restore
    iret
```

**概要:** IRQ 2〜15 のハンドラ。全て `save` → `c_intr_irqN` → `restore` → `iret` の標準パターン。

| ラベル | IRQ | 割り込みソース |
|--------|-----|-------------|
| `intr_irq2` | 2 | カスケード (スレーブ PIC) |
| `intr_irq3` | 3 | COM2 |
| `intr_irq4` | 4 | COM1 |
| `intr_irq5` | 5 | LPT2 |
| `intr_irq6` | 6 | フロッピーディスクコントローラ |
| `intr_irq7` | 7 | LPT1 / スプリアス |
| `intr_irq8` | 8 | RTC |
| `intr_irq9` | 9 | リダイレクト (IRQ2) |
| `intr_irq10` | 10 | 予約 |
| `intr_irq11` | 11 | 予約 |
| `intr_irq12` | 12 | PS/2 マウス |
| `intr_irq13` | 13 | FPU 例外 |
| `intr_irq14` | 14 | プライマリ IDE |
| `intr_irq15` | 15 | セカンダリ IDE |

### intr_smp_timer0

```asm
intr_smp_timer0:
    call  save
    call  c_intr_smp_timer0
    call  restore
    iret
```

**概要:** CPU 0 用 APIC タイマーハンドラ。

**処理内容:** `save` → `c_intr_smp_timer0` → `restore` → `iret` の標準パターン。

**呼び出し元:** CPU 0 の Local APIC タイマー

### intr_smp_timer1

```asm
intr_smp_timer1:
    call  save
    call  c_intr_smp_timer1
    call  restore
    iret
```

**概要:** CPU 1 用 APIC タイマーハンドラ。

**処理内容:** `save` → `c_intr_smp_timer1` → `restore` → `iret` の標準パターン。

**呼び出し元:** CPU 1 の Local APIC タイマー

### intr_syscall

```asm
intr_syscall:
    call  save
    movl  APIC_ID_REG, %eax
    shrl  $24, %eax
    testl %eax, %eax
    jz    1f
    movl  $1, %eax
1:
    movl  12(%esp), %ebx
    pushl 36(%ebx)
    pushl 32(%ebx)
    pushl 28(%ebx)
    pushl 24(%ebx)
    pushl 20(%ebx)
    pushl 16(%ebx)
    pushl 12(%ebx)
    pushl 8(%ebx)
    pushl %eax
    call  c_intr_syscall
    addl  $36, %esp
    call  restore
    iret
```

**概要:** システムコールディスパッチャ。`int 0x99` で呼び出される。

**処理内容:**

1. `save` でレジスタ退避
2. APIC ID を読み取って CPU 番号 (0 または 1) を取得する
3. `save` 後のスタックから退避フレームのアドレスを取得 (`12(%esp)` = EBX が指す退避フレーム)
4. 退避フレームから以下の 8 個の引数をスタックにプッシュする:
   - オフセット 8: ESP (引数ポインタとして使用)
   - オフセット 12: EIP
   - オフセット 16: EBP
   - オフセット 20: ESI
   - オフセット 24: EDI
   - オフセット 28: EFLAGS
   - オフセット 32: EAX
   - オフセット 36: EBX
5. CPU 番号 (`apic`) を最初の引数としてプッシュする
6. `c_intr_syscall(apic, esp, eip, ebp, esi, edi, eflags, eax, ebx)` を呼び出す
7. スタックを 36 バイトクリーンアップする (9 引数 x 4 バイト)
8. `restore` → `iret` で復帰する

**呼び出し元:** klib.s の `syscall` 関数 (`int $0x99`)

**注意点:**

- ユーザータスクのシステムコール番号は EBP レジスタ経由で渡される
- `c_intr_syscall` の戻り値は `proc->stack - 20` (退避フレームの EAX スロット) に書き込まれ、`restore` 経由でユーザータスクの EAX に返される

## 補足

### 割り込みフレームのスタックレイアウト

Ring 3 → Ring 0 遷移時の CPU が自動的にプッシュするスタックフレーム:

```
高アドレス
+----------+
| SS       |  (Ring 3 のスタックセグメント)
+----------+
| ESP      |  (Ring 3 のスタックポインタ)
+----------+
| EFLAGS   |
+----------+
| CS       |
+----------+
| EIP      |  ← iret で戻るアドレス
+----------+
低アドレス (現在の ESP)
```

`save` が `call` で呼ばれた時点のスタック (save 内部、EAX/EBX プッシュ後):

```
高アドレス
+----------+
| SS       |  +28
+----------+
| ESP      |  +24 → save がユーザー ESP として読み取る
+----------+
| EFLAGS   |  +20
+----------+
| CS       |  +16
+----------+
| EIP      |  +12 → save がユーザー EIP として読み取る
+----------+
| ret addr |  +8  (call save のリターンアドレス)
+----------+
| EBX      |  +4
+----------+
| EAX      |  +0  ← ESP
+----------+
低アドレス
```

### レジスタ退避領域 (proc.reg[]) のレイアウト

```
+48  old_stack_ptr (前回の stack ポインタ)
+44  ES
+40  DS
+36  EBX
+32  EAX
+28  EFLAGS
+24  EDI
+20  ESI
+16  EBP
+12  EIP
+8   ESP
+4   EDX
+0   ECX   ← stack ポインタが指す位置
```

1 フレームあたり 52 バイト (13 x 4)。割り込みがネストする場合、`stack` ポインタが 52 バイトずつ加算されて新しいフレームが確保される。

### タスクスイッチの仕組み

1. `restore` 内で `k_nest` が 0 になった場合、`sched_next_tsk_check(apic)` が呼ばれる
2. スケジューラが次のタスクを選択すると、`current_proc[cpu]` が新しいプロセスに更新される
3. `restore` は更新された `current_proc[cpu]` からレジスタを復帰する
4. 新タスクの ESP と EIP が割り込みフレーム上に書き込まれるため、`iret` 実行時に新タスクのコンテキストに遷移する
5. 結果的に、旧タスクの `save` で退避されたレジスタは `reg[]` に残り、旧タスクが次に実行される際に `restore` で復帰される
