# コンテキストスイッチ詳解

tiny-itron の核心部分である `SAVE_ALL` / `RESTORE_ALL` マクロと
`intr_enter` / `intr_leave` ルーチン (i386/intr.s) の動作を追跡する。

コンテキストスイッチとは、CPU が「今のタスクのレジスタを保存して、
別のタスクのレジスタを復元する」操作のこと。
この OS では割り込み/syscall の出入口で自動的に行われる。

---

## 全体の流れ

```
ユーザータスク A (Ring 3) が slp_tsk() を呼ぶ場合:

Task A (Ring 3)
    │
    │ int $0x99
    ▼
┌─ intr_syscall ────────────────────────────────┐
│   SAVE_ALL         ← 全レジスタを push         │
│   call intr_enter  ← k_nest++                 │
│   push %esp        ← pt_regs* を引数に         │
│   call c_intr_syscall                          │
│     → itron_syscall                             │
│       → sys_slp_tsk                            │
│         → sched_next_tsk  (フラグ設定のみ)      │
│   jmp intr_return                              │
│     call intr_leave ← k_nest--, ESP 切り替え    │
│     RESTORE_ALL    ← 全レジスタを pop           │
│     iret           ← Task B に戻る             │
└────────────────────────────────────────────────┘
Task B (Ring 3)
```

ポイント:
- `SAVE_ALL` と `RESTORE_ALL` の間でカーネルの C コードが動く
- タスクスイッチは `intr_leave` 内で行われる: ESP を切り替えることで
  `RESTORE_ALL` が新タスクのレジスタを復元する
- 切り替わらない場合は同じスタックのレジスタがそのまま復元される

---

## タスクごとのカーネルスタック

各タスクは 4KB のカーネルスタック (kern_stack) を持つ。
Ring 3 → Ring 0 の割り込み時、CPU は TSS.esp0 からスタックポインタを読み、
そのタスクのカーネルスタックに切り替える。TSS.esp0 には常に
`kern_stack_top`（スタックの最上位アドレス＝空の状態）が設定されている。
Ring 3 での実行中カーネルスタックは使われていないため、
割り込みが発生するたびに CPU は `kern_stack_top` から新たにスタックを使い始める。

```
proc_t 構造体:
  kern_esp       ← 保存されたカーネル ESP (レジスタフレームの先頭)
  kern_stack_top ← カーネルスタックの頂上 (TSS.esp0 に設定する値)
  saved_eflags   ← proc_eflags_save/restore 用
  cpu            ← CPU アフィニティ (0 or 1)
```

タスクスイッチのメカニズム:
1. `intr_leave` が現在の ESP を `current_proc[cpu]->kern_esp` に保存
2. `sched_next_tsk_check()` が `current_proc[cpu]` を新タスクに変更
3. 新タスクの `kern_esp` を ESP にロード
4. TSS.esp0 を新タスクの `kern_stack_top` に更新
5. `RESTORE_ALL` が新タスクのスタック上のレジスタを pop
6. `iret` が新タスクの Ring 3 に戻る

これは Linux 等の一般的なカーネルと同じパターンである。

---

## SAVE_ALL の詳細

### 前提: 割り込みフレーム

Ring 3 → Ring 0 の割り込み/syscall が発生すると、CPU が自動的に
タスクのカーネルスタック (TSS の SS0:ESP0) に以下を push する:

```
高アドレス
    ┌──────────────┐
    │ SS (user)    │   ※ Ring 3→0 のみ CPU が push
    │ ESP (user)   │   ※ Ring 3→0 のみ CPU が push
    │ EFLAGS       │
    │ CS           │
    │ EIP (戻り先) │
    ├──────────────┤
    │              │ ← ESP (ここから SAVE_ALL が push を開始)
低アドレス
```

割り込みから復帰するときは `iret` 命令がこの逆を行う。
EIP・CS・EFLAGS をカーネルスタックから pop し、Ring 3 への復帰であれば
さらに ESP・SS も pop する。つまり **割り込みエントリで CPU が push した
フレームを `iret` が pop して、中断した命令に戻る**。

### SAVE_ALL マクロ

```asm
.macro SAVE_ALL
    pushl   %eax
    pushl   %ecx
    pushl   %edx
    pushl   %ebx
    pushl   %ebp
    pushl   %esi
    pushl   %edi
    pushl   %ds
    pushl   %es
    movw    $0x28, %ax      # SEL_K32_D (カーネルデータセグメント)
    movw    %ax, %ds
    movw    %ax, %es
.endm
```

9 個のレジスタを push した後、DS/ES をカーネルセグメントにリロードする。
SAVE_ALL 後の ESP は pt_regs フレームの先頭を指す。

### pt_regs フレームレイアウト

```
オフセット   レジスタ     push したのは
────────────────────────────────────────────
0x00         ES          SAVE_ALL
0x04         DS          SAVE_ALL
0x08         EDI         SAVE_ALL
0x0C         ESI         SAVE_ALL
0x10         EBP         SAVE_ALL
0x14         EBX         SAVE_ALL
0x18         EDX         SAVE_ALL
0x1C         ECX         SAVE_ALL
0x20         EAX         SAVE_ALL
0x24         EIP         CPU (割り込みフレーム)
0x28         CS          CPU (割り込みフレーム)
0x2C         EFLAGS      CPU (割り込みフレーム)
0x30         ESP         CPU (Ring 3→0 のみ)
0x34         SS          CPU (Ring 3→0 のみ)
```

この構造体は `proc.h` の `struct pt_regs` として C コードからもアクセスできる。

---

## intr_enter の詳細

```asm
intr_enter:
    movl    APIC_ID_REG, %eax    # 0xFEE00020 を読んで CPU を判定
    shrl    $24, %eax            # ビット 24-31 → APIC ID (0 or 1)
    testl   %eax, %eax
    jnz     1f
    incl    k_nest0              # CPU 0 のネストカウンタ++
    ret
1:
    incl    k_nest1              # CPU 1 のネストカウンタ++
    ret
```

割り込みネスト深度を追跡する。k_nest が 0→1 になるのは最初の割り込み。

> **現在の実装では多重割り込みは発生しない。** すべての割り込みハンドラは
> 割り込みゲート (IF 自動クリア) 経由で呼ばれ、ハンドラ内で `sti` を呼ばないため、
> k_nest は常に 0→1→0 の遷移のみ。

---

## intr_leave の詳細 (タスクスイッチの核心)

```asm
intr_leave:
    # APIC ID で CPU 判定
    movl    APIC_ID_REG, %eax
    shrl    $24, %eax
    testl   %eax, %eax
    jnz     intr_leave_cpu1

intr_leave_cpu0:
    decl    k_nest0
    jnz     intr_leave_done      # ネスト中 → スキップ

    # ── 最外の割り込みからの復帰 (CPU 0) ──

    # 1. 現在の ESP を保存
    movl    current_proc, %ebx   # %ebx = current_proc[0]
    movl    %esp, (%ebx)         # current_proc[0]->kern_esp = ESP

    # 2. スケジューラ呼び出し (current_proc が変わる可能性あり)
    pushl   $0
    call    sched_next_tsk_check
    addl    $4, %esp

    # 3. (新しい) current_proc の ESP をロード
    movl    current_proc, %ebx   # %ebx = 新しい current_proc[0]
    movl    (%ebx), %esp         # ESP = new current_proc[0]->kern_esp

    # 4. TSS.esp0 を新タスクのカーネルスタック先頭に更新
    pushl   4(%ebx)              # kern_stack_top
    pushl   $0                   # cpu = 0
    call    tss_update_esp0
    addl    $8, %esp

    ret

intr_leave_done:
    ret
```

### なぜ ESP の切り替えだけでタスクスイッチできるのか

キーとなる洞察: **各タスクのカーネルスタックに完全なレジスタフレーム
(pt_regs) が保存されている**。ESP を新タスクのカーネルスタック上の
レジスタフレーム先頭に切り替えるだけで、後続の `RESTORE_ALL` と `iret` が
新タスクのレジスタを正しく復元する。

```
タスクスイッチの様子:

Task A のカーネルスタック:         Task B のカーネルスタック:
┌──────────────────┐              ┌──────────────────┐
│ SS  (Task A)     │              │ SS  (Task B)     │
│ ESP (Task A)     │              │ ESP (Task B)     │
│ EFLAGS           │              │ EFLAGS           │
│ CS               │              │ CS               │
│ EIP (Task A)     │              │ EIP (Task B)     │
│ EAX              │              │ EAX              │
│ ...              │              │ ...              │
│ ES               │ ← old ESP   │ ES               │ ← new ESP
└──────────────────┘              └──────────────────┘

intr_leave で ESP を切り替え:
  old ESP (Task A) → new ESP (Task B)

その後の RESTORE_ALL は新 ESP が指す Task B のレジスタを pop:
  pop %es, %ds, %edi, ..., %eax

最後の iret は Task B の EIP/ESP/CS/EFLAGS/SS を pop:
  → Task B のユーザー空間に復帰
```

---

## RESTORE_ALL の詳細

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

SAVE_ALL の逆順に 9 個のレジスタを pop する。
ESP は CPU が push した割り込みフレーム (EIP, CS, EFLAGS, ESP, SS) を
指す状態になり、`iret` がそれらを pop してユーザー空間に戻る。

---

## 共通復帰パス (intr_return)

すべての割り込み/syscall ハンドラは `jmp intr_return` で合流する:

```asm
intr_return:
    call    intr_leave           # k_nest--, タスクスイッチ
    # Fall through
intr_return_restore:
    RESTORE_ALL
    iret
```

通常のパス (既に動いているタスクへの復帰) では、`call intr_leave` が
リターンアドレスをスタックに自動的に push するため、`intr_leave` 末尾の
`ret` は自然に `intr_return_restore` に戻る。

**新タスクの初回起動**では、まだ割り込みを受けたことがないため
スタック上にリターンアドレスもレジスタフレームも存在しない。
そこで `proc_create()` (`proc.c:91-128`) がカーネルスタック上に
偽のフレームを一式構築しておく:

```
  (低アドレス = ESP)
  ┌─────────────────────────────┐
  │ intr_return_restore のアドレス │ ← ret で pop される
  ├─────────────────────────────┤
  │ ES, DS, EDI..EAX (全て 0)    │ ← RESTORE_ALL で pop される
  ├─────────────────────────────┤
  │ EIP = タスクのエントリポイント  │ ← iret で EIP にロードされる
  │ CS  = Ring 3 コードセグメント  │
  │ EFLAGS (IF=1)                │
  │ ESP = ユーザースタックの先頭    │
  │ SS  = Ring 3 スタックセグメント │
  └─────────────────────────────┘
  (高アドレス = スタックの底)
```

起動時に ESP をこのカーネルスタックに切り替えて `ret` を実行すると:

1. `ret` が `intr_return_restore` を pop → `RESTORE_ALL` + `iret` に進む
2. `RESTORE_ALL` が ES, DS, EDI〜EAX を pop してレジスタを初期化
3. `iret` が EIP, CS, EFLAGS, ESP, SS を pop → Ring 3 のタスクエントリポイントで実行開始

> ESP の切り替え元は状況によって異なる。最初の 2 タスクでは
> `start_first_task` / `start_second_task` (`klib.s`) が直接 ESP をロードする。
> それ以降のタスクでは `intr_leave` がタスクスイッチの一環として ESP を切り替える。

---

## syscall パスの全体トレース

Task 1 が `slp_tsk()` を呼び、Task 3 に切り替わる例:

### 1. ユーザー空間: slp_tsk() 呼び出し

```c
// lib/lib_tsk.c
ER slp_tsk(void) {
    return syscall(-TFN_SLP_TSK);
    // → int $0x99 を発行
}
```

> **注意**: `syscall()` は TFN を**負値**として受け取る。`klib.s` の syscall
> エントリポイントで `neg` 命令により正の値に変換され、関数テーブルのインデックスとなる。

### 2. CPU: 割り込みフレーム生成

`int $0x99` はソフトウェア割り込みだが、CPU の動作はハードウェア割り込みと同じである。
IDT エントリ 0x99 を参照し、Ring 3 → Ring 0 の特権遷移が発生するため、
CPU は TSS の SS0:ESP0 からカーネルスタックを取得し、以下のレジスタを自動的に push する:

```
  SS (0x6B, ユーザースタック)
  ESP (Task 1 のスタックポインタ)
  EFLAGS
  CS (0x5B, ユーザーコード)
  EIP (int $0x99 の次の命令)
```

### 3. intr_syscall: SAVE_ALL + 引数渡し

```asm
intr_syscall:
    SAVE_ALL                       # 全レジスタを push → pt_regs フレーム完成
    call    intr_enter             # k_nest: 0→1
    pushl   %esp                   # arg: pt_regs* を C 関数に渡す
    call    c_intr_syscall
    addl    $4, %esp
    jmp     intr_return
```

SAVE_ALL 後の ESP が pt_regs 構造体を指すので、そのままポインタとして渡す。

#### なぜ C の構造体でスタック上のレジスタを読み書きできるのか

ここで使われているテクニックは **構造体のスタックへのオーバーレイ** である。
割り込みエントリで CPU と SAVE_ALL がカーネルスタック上に push したレジスタ群は、
メモリ上に隙間なく並んでいる。この並び順と `struct pt_regs` のフィールド順を
一致させておけば、ESP をそのまま `struct pt_regs *` として C 関数に渡すだけで、
構造体のフィールドアクセスがスタック上のレジスタ値の読み書きになる。

```
カーネルスタック (低アドレスが上)           struct pt_regs (proc.h)
─────────────────────────────────────────────────────────────────
                                          struct pt_regs {
ESP+0x00 │ ES          │ SAVE_ALL が push    unsigned long es;
ESP+0x04 │ DS          │ SAVE_ALL が push    unsigned long ds;
ESP+0x08 │ EDI         │ SAVE_ALL が push    unsigned long edi;
ESP+0x0C │ ESI         │ SAVE_ALL が push    unsigned long esi;
ESP+0x10 │ EBP         │ SAVE_ALL が push    unsigned long ebp;
ESP+0x14 │ EBX         │ SAVE_ALL が push    unsigned long ebx;
ESP+0x18 │ EDX         │ SAVE_ALL が push    unsigned long edx;
ESP+0x1C │ ECX         │ SAVE_ALL が push    unsigned long ecx;
ESP+0x20 │ EAX         │ SAVE_ALL が push    unsigned long eax;
         ├─────────────┤                     /* CPU が push */
ESP+0x24 │ EIP         │ CPU が push         unsigned long eip;
ESP+0x28 │ CS          │ CPU が push         unsigned long cs;
ESP+0x2C │ EFLAGS      │ CPU が push         unsigned long eflags;
ESP+0x30 │ ESP (user)  │ CPU が push (※)    unsigned long esp;
ESP+0x34 │ SS  (user)  │ CPU が push (※)    unsigned long ss;
         └─────────────┘                  };
                        (※) Ring 3→0 の特権遷移時のみ CPU が push
```

`pushl %esp` で渡されるのはこのフレームの先頭アドレスなので、
C 関数は `regs->eax` と書くだけでオフセット 0x20 のスタック上の値を
読み書きできる。特別なメモリ確保やコピーは必要ない。

### 4. c_intr_syscall → sys_slp_tsk

```c
// i386/syscall.c
void c_intr_syscall(struct pt_regs *regs) {
    // regs から syscall 番号と引数を読み取り
    W ret = itron_syscall(apic, regs->eax, ...);
    // 戻り値を pt_regs の EAX スロットに書き込み
    regs->eax = ret;
}
```

pt_regs の EAX スロットに戻り値を書き込むことで、`RESTORE_ALL` で
`pop %eax` した時に戻り値がユーザーの EAX に入る。

> **なぜ EAX なのか** — i386 の C 呼び出し規約 (cdecl) では、関数の戻り値は
> EAX レジスタで返す約束になっている。ユーザー側のラッパー関数
> `slp_tsk()` → `syscall()` は、`int $0x99` からの復帰後に EAX の値を
> そのまま `return` する。コンパイラは `return` 文の値が EAX にあることを
> 前提としたコードを生成するため、カーネル側で `regs->eax` に戻り値を
> 書き込んでおけば、ユーザータスクには通常の関数戻り値として届く。

### 5. sys_slp_tsk: タスク状態変更

```c
// kernel/sys_tsk.c
ER sys_slp_tsk(W apic) {
    ID tskid = c_tskid[apic];    // = 1

    /* caller holds kernel_lk (c_intr_syscall で取得済み) */
    sched_rem(&tsk[1].plink);     // 優先度キューから除去
    tsk[1].tskstat = TTS_WAI;     // Task 1 → WAI 状態
    sched_next_tsk(apic);         // next_tsk_flag[0] = next_tsk_flag[1] = 1
    return E_OK;
}
```

`sched_next_tsk()` は引数 `apic` によらず両 CPU のリスケジュールフラグを立てる。
これは CPU 間のタスク起床 (例: CPU 0 の IRQ1 で CPU 1 のタスクを起こす) に必要な設計である。
詳細と具体例は [sched_next_tsk リファレンス](refs/kernel/sched.md#sched_next_tsk) を参照。
実際の切り替えは `intr_leave` で行う。

### 6. intr_return → intr_leave

```
intr_leave (CPU 0):
  k_nest0: 1→0

  # ESP を current_proc[0]->kern_esp に保存
  # (Task 1 のカーネルスタック上の pt_regs 先頭)

  sched_next_tsk_check(0):
    → sched_do_next_tsk(0)
      → Task 3 を発見、current_proc[0] = &proc[3]

  # ESP を proc[3].kern_esp からロード
  # (Task 3 のカーネルスタック上の pt_regs 先頭)

  # TSS.esp0 = proc[3].kern_stack_top に更新
```

### 7. RESTORE_ALL + iret → Task 3 へ

ESP は Task 3 のカーネルスタックを指しているので、
`RESTORE_ALL` は Task 3 のレジスタを pop し、
`iret` は Task 3 の EIP/ESP に戻る。

---

## タスクスイッチが起きない場合

例: APIC タイマー割り込み (タスク切り替え不要の場合)

```
Task 2 実行中
    │ APIC タイマー割り込み
    ▼
intr_smp_timer1:
    SAVE_ALL                     # Task 2 のレジスタを push
    call intr_enter              # k_nest1: 0→1
    call c_intr_smp_timer1       # EOI のみ (プリエンプション契機)
    jmp  intr_return
        call intr_leave          # k_nest1: 1→0
                                 # ESP を保存 → sched_next_tsk_check
                                 # → 変更なし → 同じ ESP をロード
        RESTORE_ALL              # 同じ Task 2 のレジスタを pop
        iret                     # Task 2 に戻る
```

ESP が保存されて直後に同じ値がロードされるため、
結果的に SAVE_ALL/RESTORE_ALL 前後でスタック状態は変わらない。

> **注意: タイマー割り込みでもタスクスイッチは起きうる。**
> 上の例は `next_tsk_flag[1]` が 0 の場合である。
> もし別の CPU やハンドラが `sched_next_tsk()` で `next_tsk_flag[1] = 1` を
> セットしていれば、`sched_next_tsk_check()` が `sched_do_next_tsk()` を呼び、
> タスクスイッチが発生する。これがこの OS のプリエンプション機構であり、
> キーボード割り込み (CPU 0) で起床した Task 4 (CPU 1) への切り替えなどは
> CPU 1 のタイマー割り込みの `intr_leave` で実行される。
> 詳細は [sched_next_tsk リファレンス](refs/kernel/sched.md#sched_next_tsk) を参照。

---

## 初回のタスク起動 (ltr + iret 方式)

最初のタスク起動も、2 回目以降と同じ RESTORE_ALL + iret パスを使う。
ハードウェア TSS スイッチ (ljmp) は使わない。

```asm
# i386/klib.s
start_first_task:
    movw    $0x38, %ax          # SEL_TSS0
    ltr     %ax                 # Task Register にロード (レジスタ交換なし)
    movl    current_proc, %ebx  # current_proc[0] = &proc[1]
    movl    (%ebx), %esp        # ESP = proc[1].kern_esp
    ret                         # → intr_return_restore → RESTORE_ALL → iret

start_second_task:
    movw    $0x40, %ax          # SEL_TSS1
    ltr     %ax                 # Task Register にロード
    movl    current_proc+4, %ebx
    movl    (%ebx), %esp        # ESP = proc[2].kern_esp
    ret                         # → intr_return_restore → RESTORE_ALL → iret
```

`ltr` は Task Register に TSS セレクタをロードするだけで、レジスタの保存・復元は
行わない。CPU はこの TSS の esp0/ss0 を、次の Ring 3→Ring 0 割り込みで使う。

`proc_create()` がカーネルスタック上に偽の pt_regs フレーム + `intr_return_restore`
の戻りアドレスを構築しているため、`ret` → `RESTORE_ALL` → `iret` で Ring 3 に遷移する。

この方式により、初回起動も通常のタスクスイッチも同じコードパスを通る。

### proc_create が構築する初期フレーム

`proc_create()` は新タスクのカーネルスタックに「偽の」割り込みフレームを構築する。
これにより、最初の `intr_leave` → `RESTORE_ALL` + `iret` でタスクが起動する:

```
カーネルスタック (高アドレスが上):
┌──────────────────┐ ← kern_stack_top (= TSS.esp0)
│ SS  (SEL_U32_S)  │   ← iret が pop → Ring 3 の SS
│ ESP (user stack) │   ← iret が pop → Ring 3 の ESP
│ EFLAGS (IF=1)    │   ← iret が pop → 割り込み有効
│ CS  (SEL_U32_C)  │   ← iret が pop → Ring 3 の CS
│ EIP (task entry) │   ← iret が pop → タスクのエントリポイント
│ EAX = 0          │
│ ECX = 0          │
│ EDX = 0          │
│ EBX = 0          │
│ EBP = 0          │
│ ESI = 0          │
│ EDI = 0          │
│ DS  (SEL_U32_D|3)│
│ ES  (SEL_U32_D|3)│   ← kern_esp (RESTORE_ALL がここから pop)
├──────────────────┤
│ intr_return_restore│  ← intr_leave の ret が飛ぶ先
└──────────────────┘
```

---

## ネスト割り込みの設計と現状

### 設計: 多重割り込みに対応できるインフラがある

`intr_enter`/`intr_leave` は多重割り込みを想定した設計になっている:

- `k_nest0`/`k_nest1` が割り込みのネスト深度を追跡する
- 各タスクのカーネルスタックに push するため、ネストしても
  各フレームは独立に保存される
- 内側の `intr_leave` はタスクスイッチしない (k_nest > 0)。
  外側の `intr_leave` で初めてスイッチを検討する

### 現状: 多重割り込みは発生しない

しかし、現在の実装では **多重割り込みは一切発生しない**。理由は:

1. **すべての割り込みが割り込みゲート (GT_INTR) 経由** —
   CPU が IF を自動クリアするため、ハンドラ進入時点で割り込み禁止になる
2. **どのハンドラも `sti` を呼ばない** —
   ハンドラ全体が割り込み禁止のまま実行される
3. **割り込みが再許可されるのは `iret` の瞬間のみ** —
   `iret` が EFLAGS (IF=1) をアトミックに復元する

したがって k_nest は常に 0→1→0 の遷移しかしない。

---

## よくある落とし穴

### 1. TSS.esp0 の更新を忘れると壊れる

タスクスイッチ後に TSS.esp0 を新タスクの `kern_stack_top` に更新しないと、
次の割り込みで CPU が旧タスクのカーネルスタックに切り替えてしまい、
スタック破壊が起きる。`intr_leave` の `tss_update_esp0()` 呼び出しが重要。

### 2. sched_next_tsk はフラグを立てるだけ

`sched_next_tsk()` は `next_tsk_flag[0] = next_tsk_flag[1] = 1` と
するだけで、実際の切り替えは行わない。切り替えは `intr_leave` から
呼ばれる `sched_next_tsk_check()` → `sched_do_next_tsk()` で行う。

### 3. syscall の戻り値は regs->eax に書く

```c
regs->eax = ret;  // pt_regs フレームの EAX スロットに書き込み
```

RESTORE_ALL が `pop %eax` する時にこの値が EAX にロードされる。
カーネル内の他の場所で戻り値を書く場合は `proc_set_return_value()` を使う:

```c
void proc_set_return_value(proc_t *p, unsigned long val) {
    struct pt_regs *regs = (struct pt_regs *)(p->kern_esp + 4);
    regs->eax = val;
}
```

`kern_esp + 4` は `intr_return` の `call intr_leave` で push された
戻りアドレス (`intr_return_restore`) をスキップするためのオフセット。
`intr_leave` から `ret` で戻ると `intr_return_restore` に着地するが、
`kern_esp` はその戻りアドレスの下 (= `RESTORE_ALL` が pop する pt_regs の先頭) を
指しているため、pt_regs にアクセスするには +4 が必要。

---

## 関連ドキュメント

- [ソースコード読解ガイド](source-guide.md) — ファイル全体の概要
- [システム概要](system-overview.md) — タスク間の協調動作
- [i386 アーキテクチャ](i386-architecture.md) — 特権レベル、TSS
- [SMP 基礎](smp-basics.md) — per-CPU 変数、スピンロック
