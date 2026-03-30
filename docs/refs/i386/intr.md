# intr.s

対象ファイル: `i386/intr.s`

## 概要

割り込みハンドリングの中核を担うアセンブリファイル。カーネルで最も重要なファイルである。
以下の機能を提供する:

1. **SAVE_ALL / RESTORE_ALL マクロ**: 割り込み発生時のレジスタ退避・復帰。per-task カーネルスタック上に `pt_regs` フレームを構築する
2. **intr_enter**: per-CPU の割り込みネストカウンタ (`k_nest`) をインクリメント
3. **intr_leave**: ネストカウンタをデクリメントし、最外レベルのとき ESP スワップによるタスクスイッチを実行
4. **intr_return / intr_return_restore**: 共通の割り込み復帰パス (RESTORE_ALL + iret)
5. **CPU 例外ハンドラ**: 除算エラー、一般保護例外、ページフォルトなど
6. **IRQ ハンドラ**: PIC (i8259) 経由の外部割り込み (IRQ0-15)
7. **APIC タイマーハンドラ**: per-CPU の Local APIC タイマー割り込み
8. **システムコールハンドラ**: `int 0x99` によるシステムコールエントリ

すべてのハンドラスタブは同一パターンで実装される:

```
SAVE_ALL            ← レジスタ退避、pt_regs フレーム構築
call intr_enter     ← k_nest[cpu]++
call c_handler      ← C 言語ハンドラ呼び出し
jmp  intr_return    ← intr_leave → RESTORE_ALL → iret
```

## 定数・マクロ

### APIC_ID_REG

| 名前 | 値 | 説明 |
|------|------|------|
| `APIC_ID_REG` | `0xFEE00020` | Local APIC の ID レジスタ (MMIO)。ビット [31:24] に APIC ID が格納される |

### SAVE_ALL マクロ

```asm
.macro SAVE_ALL
    pushl   %eax        # [ESP+0x20]
    pushl   %ecx        # [ESP+0x1C]
    pushl   %edx        # [ESP+0x18]
    pushl   %ebx        # [ESP+0x14]
    pushl   %ebp        # [ESP+0x10]
    pushl   %esi        # [ESP+0x0C]
    pushl   %edi        # [ESP+0x08]
    pushl   %ds         # [ESP+0x04]
    pushl   %es         # [ESP+0x00]
    movw    $0x28, %ax  # SEL_K32_D (カーネルデータセグメント)
    movw    %ax, %ds
    movw    %ax, %es
.endm
```

**概要:** 9 個のレジスタ (EAX, ECX, EDX, EBX, EBP, ESI, EDI, DS, ES) を per-task カーネルスタックに push する。CPU が Ring 3→Ring 0 遷移時に push した 5 個 (SS, ESP, EFLAGS, CS, EIP) と合わせて、14 dword (56 バイト) の `pt_regs` フレームが完成する。

push 完了後、DS/ES をカーネルデータセグメント (`SEL_K32_D = 0x28`) にリロードする。
CPU は Ring 3→Ring 0 遷移時に CS と SS を自動切り替えするが、DS/ES は変更しないため
明示的なリロードが必要。

### RESTORE_ALL マクロ

```asm
.macro RESTORE_ALL
    popl    %es
    popl    %ds
    popl    %edi
    popl    %esi
    popl    %ebp
    popl    %ebx
    popl    %edx
    popl    %ecx
    popl    %eax
.endm
```

**概要:** SAVE_ALL の逆順で 9 個のレジスタを pop する。実行後、ESP は CPU interrupt frame
(EIP, CS, EFLAGS, ESP, SS) を指しており、`iret` で Ring 3 に復帰できる。

## グローバル変数

### k_nest0, k_nest1

```asm
k_nest0:  .long  0
k_nest1:  .long  0
```

**概要:** per-CPU の割り込みネストカウンタ。`intr_enter` でインクリメント、`intr_leave` でデクリメントされる。値が 0 になったとき (最外の割り込みから復帰するとき) のみタスクスイッチを試みる。

### gp_error_code

```asm
gp_error_code:  .long 0
```

**概要:** 直近の #GP 例外のエラーコード。`intr_general` がスタックから退避し、`c_intr_general` が診断に使用する。

## 外部シンボル参照

| シンボル | 定義元 | 説明 |
|---------|--------|------|
| `current_proc` | kernelval.c | per-CPU の現在実行中プロセスポインタ配列 (`proc_t* current_proc[2]`) |
| `sleep_current_proc` | proc.c | 現在プロセスをスリープ状態にする関数 |
| `ena_tex` | kernel | タスク例外処理の有効化 |
| `sched_next_tsk_check` | sched.c | `int sched_next_tsk_check(int apic)` — 次タスクへの切り替えチェック。`current_proc[apic]` を更新する可能性がある |
| `tss_update_esp0` | tss.c | `void tss_update_esp0(int cpu, unsigned long esp0)` — TSS.esp0 を更新 |

## pt_regs フレームレイアウト

SAVE_ALL 完了後、カーネルスタック上に以下のフレームが構築される:

```
          Low address (ESP)
      ┌─────────────────────┐
      │ ES          (0x00)  │  ← SAVE_ALL (最後に push)
      │ DS          (0x04)  │
      │ EDI         (0x08)  │
      │ ESI         (0x0C)  │
      │ EBP         (0x10)  │
      │ EBX         (0x14)  │
      │ EDX         (0x18)  │
      │ ECX         (0x1C)  │
      │ EAX         (0x20)  │  ← SAVE_ALL (最初に push)
      ├─────────────────────┤
      │ EIP         (0x24)  │  ← CPU (interrupt frame)
      │ CS          (0x28)  │
      │ EFLAGS      (0x2C)  │
      │ ESP (user)  (0x30)  │  ← Ring 3→0 のみ
      │ SS  (user)  (0x34)  │
      └─────────────────────┘
          High address (kern_stack_top)
```

これは `struct pt_regs` (proc.h で定義) と同一のレイアウトである。

## ルーチンリファレンス

### intr_enter

```asm
intr_enter:
    movl    APIC_ID_REG, %eax   # APIC ID 読み取り
    shrl    $24, %eax           # ビット [31:24] → EAX = 0 or 1
    testl   %eax, %eax
    jnz     1f
    incl    k_nest0             # CPU 0: k_nest0++
    ret
1:
    incl    k_nest1             # CPU 1: k_nest1++
    ret
```

**概要:** APIC ID で CPU を判定し、対応する `k_nest` カウンタをインクリメントする。

**呼び出し元:** 全ハンドラスタブ (SAVE_ALL 直後)

---

### intr_leave

**概要:** ネストカウンタをデクリメントし、最外レベル (k_nest == 0) のときタスクスイッチを実行する。
割り込みハンドリングの最も重要なルーチンで、タスクスイッチの核心部分を担う。

**処理内容 (CPU 0 パス、CPU 1 も同一ロジック):**

```asm
intr_leave_cpu0:
    decl    k_nest0             # k_nest0--
    jnz     intr_leave_done     # まだネスト中 → タスクスイッチなし

    # Step 1: 現在の ESP を running task の proc_t に保存
    movl    current_proc, %ebx      # EBX = current_proc[0]
    movl    %esp, (%ebx)            # current_proc[0]->kern_esp = ESP

    # Step 2: スケジューラに問い合わせ (current_proc[0] が変わる可能性)
    pushl   $0
    call    sched_next_tsk_check    # int sched_next_tsk_check(0)
    addl    $4, %esp

    # Step 3: (更新された可能性のある) current_proc[0] から ESP をロード
    movl    current_proc, %ebx      # 再読み込み
    movl    (%ebx), %esp            # ESP = new task の kern_esp ★ここがタスクスイッチ

    # Step 4: TSS.esp0 を新タスクの kern_stack_top に更新
    pushl   4(%ebx)                 # proc_t.kern_stack_top (offset 4)
    pushl   $0                      # cpu = 0
    call    tss_update_esp0
    addl    $8, %esp

    ret                             # → intr_return_restore に戻る
```

**タスクスイッチの仕組み:**

Step 3 の `movl (%ebx), %esp` が実質的なコンテキストスイッチである。
この時点で ESP が新タスクのカーネルスタックを指すようになり、
以降の RESTORE_ALL は新タスクの pt_regs を pop し、
`iret` は新タスクの EIP/CS/EFLAGS/ESP/SS で Ring 3 に復帰する。

旧タスクのレジスタは Step 1 で保存した ESP が指すカーネルスタック上に残り、
旧タスクが再スケジュールされたとき Step 3 で ESP がロードされて復帰する。

**新規タスクの初回スケジュール:**

`proc_create()` は新タスクのカーネルスタック上に偽の pt_regs フレームと
戻りアドレス (`intr_return_restore`) を構築する。
`intr_leave` の `ret` がこの戻りアドレスを pop し、
`intr_return_restore` → RESTORE_ALL → `iret` で Ring 3 に遷移する。

**ネスト中の場合:**

`k_nest` が 0 にならない場合 (ネストした割り込みの復帰)、`intr_leave_done` の `ret` で
単純に呼び出し元に戻る。タスクスイッチは行わない。

---

### intr_return / intr_return_restore

```asm
intr_return:
    call    intr_leave          # k_nest--, possible task switch
    # fall through to intr_return_restore

intr_return_restore:
    RESTORE_ALL                 # pop ES,DS,EDI,ESI,EBP,EBX,EDX,ECX,EAX
    iret                        # pop EIP,CS,EFLAGS (and ESP,SS if Ring 3)
```

**概要:** 全ハンドラスタブの共通復帰パス。`intr_leave` でネスト管理とタスクスイッチを行い、
RESTORE_ALL で pt_regs フレームからレジスタを復帰し、`iret` で Ring 3 に戻る。

`intr_return_restore` は `.globl` シンボルで、`proc_create()` が新タスクの偽フレームに
戻りアドレスとして設定する。

---

### user_restore

```asm
user_restore:
    addl    $8, %esp    # texptn, exinf をスキップ (8 バイト)
    call    ena_tex     # タスク例外を再有効化
    popfl               # EFLAGS 復帰
    popl    %edi        # GPR 復帰 (7 個)
    popl    %esi
    popl    %edx
    popl    %ecx
    popl    %ebx
    popl    %eax
    ret                 # 元の EIP に戻る
```

**概要:** タスク例外ハンドラからの復帰パス。`interrupt.c` の `stack_adjust()` がユーザースタック上に
例外ハンドラの引数と退避レジスタを構築し、例外ハンドラが `return` すると `user_restore` に到達する。

**ユーザースタックレイアウト (user_restore エントリ時):**

```
ESP+0:  texptn     (例外パターン、ハンドラが消費済み)
ESP+4:  exinf      (拡張情報、ハンドラが消費済み)
ESP+8:  EFLAGS     (退避値)
ESP+12: EDI
ESP+16: ESI
ESP+20: EDX
ESP+24: ECX
ESP+28: EBX
ESP+32: EAX
ESP+36: 元の EIP   (ret で復帰するアドレス)
```

---

### 例外ハンドラ

#### エラーコードなし (ベクタ 0-7, 9, 16)

| ラベル | ベクタ | 例外名 | 処理 |
|--------|--------|--------|------|
| `intr_divide` | 0 | #DE (除算エラー) | SAVE_ALL → intr_enter → c_intr_divide → intr_return |
| `intr_singlestep` | 1 | #DB (デバッグ) | `iret` のみ |
| `intr_nmi` | 2 | NMI | `iret` のみ |
| `intr_breakpoint` | 3 | #BP | `iret` のみ |
| `intr_overflow` | 4 | #OF | `iret` のみ |
| `intr_bounds` | 5 | #BR | `iret` のみ |
| `intr_opcode` | 6 | #UD | `iret` のみ |
| `intr_copr_not_available` | 7 | #NM | `iret` のみ |
| `intr_copr_seg_overrun` | 9 | (旧式) | `iret` のみ |
| `intr_copr_error` | 16 | #MF | `iret` のみ |

#### エラーコード付き (ベクタ 8, 10-14)

CPU がエラーコードを自動的にスタックに push する。エラーコードを `iret` 前に除去しないと、
`iret` がエラーコードを EIP として解釈し、即座に再フォルト → ダブルフォルト → トリプルフォルトとなる。

| ラベル | ベクタ | 例外名 | 処理 |
|--------|--------|--------|------|
| `intr_doublefault` | 8 | #DF | `addl $4, %esp` → `iret` |
| `intr_tss` | 10 | #TS | `addl $4, %esp` → `iret` |
| `intr_segment_not_present` | 11 | #NP | `addl $4, %esp` → `iret` |
| `intr_stack` | 12 | #SS | `addl $4, %esp` → `iret` |
| `intr_general` | 13 | #GP | エラーコード保存 → `addl $4, %esp` → 標準パターン |
| `intr_page` | 14 | #PF | `addl $4, %esp` → 標準パターン |

**intr_general の詳細:**

```asm
intr_general:
    pushl   %eax                # EAX を一時退避
    movl    4(%esp), %eax       # EAX = エラーコード
    movl    %eax, gp_error_code # グローバル変数に保存
    popl    %eax                # EAX 復帰
    addl    $4, %esp            # エラーコード除去
    SAVE_ALL
    call    intr_enter
    call    c_intr_general      # 診断出力して停止
    jmp     intr_return
```

エラーコードを `gp_error_code` グローバル変数に保存してから除去する。
`c_intr_general` は通常 hlt で停止するため、`intr_return` には到達しない。

**intr_page の詳細:**

エラーコードを単に破棄してから標準パターンで処理する。
`c_intr_page` は CR2 レジスタからフォルトアドレスを直接読み取る。

---

### IRQ ハンドラ (IRQ 0-15)

全て同一パターン: `SAVE_ALL → call intr_enter → call c_intr_irqN → jmp intr_return`

PIC (i8259) 経由の外部割り込みは CPU 0 (BSP) のみに配送される。

| ラベル | IRQ | 割り込みソース |
|--------|-----|-------------|
| `intr_irq0` | 0 | PIT タイマー (~17ms, HZ=60) |
| `intr_irq1` | 1 | キーボード (i8042) |
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

---

### APIC タイマーハンドラ

| ラベル | CPU | 説明 |
|--------|-----|------|
| `intr_smp_timer0` | 0 | CPU 0 の Local APIC タイマー |
| `intr_smp_timer1` | 1 | CPU 1 の Local APIC タイマー |

どちらも同一パターン: `SAVE_ALL → call intr_enter → call c_intr_smp_timerN → jmp intr_return`

---

### intr_syscall

```asm
intr_syscall:
    SAVE_ALL                    # ユーザーレジスタを per-task カーネルスタックに退避
    call    intr_enter          # k_nest[cpu]++
    pushl   %esp                # 引数: pt_regs* (ESP が pt_regs フレームを指す)
    call    c_intr_syscall      # W c_intr_syscall(struct pt_regs *regs)
    addl    $4, %esp            # 引数クリーンアップ (cdecl)
    jmp     intr_return         # intr_leave → RESTORE_ALL → iret
```

**概要:** `int 0x99` によるシステムコールエントリ。

**処理内容:**

1. SAVE_ALL で全レジスタを退避。ESP が pt_regs フレームの先頭を指す
2. `intr_enter` で k_nest をインクリメント
3. ESP (= pt_regs へのポインタ) を引数として `c_intr_syscall` を呼び出す
4. `c_intr_syscall` は `regs->esp` 経由でユーザースタックからシステムコール番号と引数を読み取り、
   戻り値を `regs->eax` に書き込む
5. `intr_return` → `intr_leave` → RESTORE_ALL で `regs->eax` が EAX にロードされ、
   `iret` でユーザータスクに戻り値が返る

**注意点:**

- システムコール番号は EAX レジスタ経由ではなく、ユーザースタック上に置かれている
  (klib.s の `syscall` ラッパーが `int $0x99` 前に push)
- 旧アーキテクチャでは 9 引数を push して呼び出していたが、現在は pt_regs ポインタ 1 つのみ

## 補足

### ハンドラスタブのパターン

```
              ┌────────────────────────────────────────────┐
              │          Handler Stub Pattern              │
              └────────────────────────────────────────────┘

   Ring 3 で実行中                  割り込み発生
          ↓                              ↓
   CPU が SS,ESP,EFLAGS,CS,EIP を自動 push (TSS.esp0 → カーネルスタック)
          ↓
   ┌─────────────────┐
   │    SAVE_ALL      │  EAX,ECX,EDX,EBX,EBP,ESI,EDI,DS,ES を push
   │                  │  DS/ES をカーネルセグメントにリロード
   └────────┬────────┘
            ↓
   ┌─────────────────┐
   │   intr_enter     │  k_nest[cpu]++
   └────────┬────────┘
            ↓
   ┌─────────────────┐
   │   C handler      │  c_intr_irq0, c_intr_syscall 等
   └────────┬────────┘
            ↓
   ┌─────────────────┐
   │   intr_leave     │  k_nest[cpu]--
   │                  │  k_nest==0 なら ESP スワップ (タスクスイッチ)
   │                  │  tss_update_esp0() で TSS 更新
   └────────┬────────┘
            ↓
   ┌─────────────────┐
   │  RESTORE_ALL     │  ES,DS,EDI,ESI,EBP,EBX,EDX,ECX,EAX を pop
   └────────┬────────┘
            ↓
   ┌─────────────────┐
   │     iret         │  EIP,CS,EFLAGS,ESP,SS を pop → Ring 3 復帰
   └─────────────────┘
```

### タスクスイッチの全体フロー

1. タイマー割り込み (APIC タイマーまたは PIT) が発生
2. SAVE_ALL が現在タスクの全レジスタを per-task カーネルスタックに退避
3. `intr_enter` で k_nest をインクリメント
4. C ハンドラが `sched_next_tsk(apic)` でタスクスイッチフラグを設定
5. `intr_leave` で k_nest をデクリメント → 0 になった場合:
   - 現在の ESP を `current_proc[cpu]->kern_esp` に保存
   - `sched_next_tsk_check(cpu)` を呼び出し、`current_proc[cpu]` が新タスクに更新される
   - 新タスクの `kern_esp` を ESP にロード → **ここがコンテキストスイッチ**
   - `tss_update_esp0()` で TSS.esp0 を新タスクの `kern_stack_top` に更新
6. RESTORE_ALL が新タスクのカーネルスタックからレジスタを pop
7. `iret` が新タスクの EIP/CS/EFLAGS/ESP/SS で Ring 3 に復帰

### Ring 0→Ring 0 割り込みの制約

Ring 0 で割り込みが発生した場合、CPU は SS/ESP を push **しない**。
そのため pt_regs フレームは 12 dword (48 バイト) になり、RESTORE_ALL + `iret` が
想定する 14 dword フレームとは不整合が起きる。

本カーネルでは Ring 0 で割り込みを許可しない設計で、この問題を回避している:
- 全ハンドラスタブは `sti` を呼ばない
- C ハンドラ内部でも `csti()` は使わない
- `intr_leave` 内で push/call する際は k_nest > 0 のままであり、ネスト判定で安全

### syscall 呼び出しフロー (klib.s 側)

```
ユーザータスク:
  cre_sem(1, &pk_csem)              lib/lib_sem.c
    → syscall(TFN_CRE_SEM, ...)    lib/lib_sem.c (引数をスタックに push)
      → int $0x99                   klib.s の syscall ルーチン

klib.s syscall:
    pushl   %ebp                    # caller の EBP を退避
    movl    %esp, %ebp              # スタックフレーム確立
    int     $0x99                   # ソフトウェア割り込み → intr_syscall
    popl    %ebp                    # EBP 復帰
    ret                             # EAX に戻り値

ユーザースタック (int $0x99 時点):
    [user_esp + 0]   saved EBP
    [user_esp + 4]   return address
    [user_esp + 8]   func_code (sysid)
    [user_esp + 12]  arg1
    [user_esp + 16]  arg2
    ...
```
