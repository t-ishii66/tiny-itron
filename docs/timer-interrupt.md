# タイマー割り込み処理フローガイド

ユーザータスクが実行中にタイマー割り込みが発生し、レジスタ退避 → C ハンドラ →
スケジューリング → 復帰（または別タスクへの切り替え）に至る全過程を
ステップバイステップで解説する。

---

## 1. 概要

タイマー割り込みは OS の心臓部であり、以下の役割を担う:

- **スケジューリング駆動**: タイムアウト待ちのタスクを起床させる
- **プリエンプション契機**: 割り込み復帰時に、より高優先度のタスクがあれば切り替える
- **時刻管理**: `timer_ticks` のインクリメント、画面上のティック表示

tiny-itron には **2 系統のタイマー** がある:

| タイマー     | チップ  | CPU   | ベクタ           | 周期              |
|-------------|---------|-------|------------------|-------------------|
| PIT (IRQ0)  | 8254    | CPU 0 | 0x80 (VECT_IRQ0) | ~16.7ms (FREQ/HZ) |
| APIC Timer  | Local APIC | CPU 0 | 0x9A (VECT_SMP_TIMER0) | MAX_TIMER_COUNT に依存 |
| APIC Timer  | Local APIC | CPU 1 | 0x9B (VECT_SMP_TIMER1) | MAX_TIMER_COUNT に依存 |

PIT は i8259 PIC を経由するため CPU 0 のみに配送される。
APIC タイマーは各 CPU のローカル APIC に内蔵されており、CPU ごとに独立して動作する。

---

## 2. タイマーの初期化

### 2.1 PIT (Programmable Interval Timer)

8254 PIT はチャネル 0 を矩形波モード (mode 3) で使用する。

```c
/* i386/timerP.h */
#define FREQ    1193182L    /* PIT 入力クロック周波数 (Hz) */
#define SQUARE  0x36        /* チャネル 0, mode 3 (矩形波), 16bit */
#define HZ      60          /* 目標割り込みレート */

/* i386/timer.c */
void timer_init(void)
{
    unsigned long count;
    count = FREQ / HZ;                  /* = 19886 (0x4DAE) */
    outb(IO_TIMER_C, SQUARE);           /* ポート 0x43: 制御ワード */
    outb(IO_TIMER_0, count & 0xff);     /* ポート 0x40: カウント低バイト */
    outb(IO_TIMER_0, (count >> 8) & 0xff); /* カウント高バイト */
}
```

カウント値 `1193182 / 60 = 19886` により、約 16.7ms (≒60Hz) ごとに IRQ0 が発生。
PIT の出力は i8259 マスター PIC の IR0 に接続され、ベクタ 0x80 で CPU 0 に
割り込みが配送される。

### 2.2 APIC タイマー

各 CPU の Local APIC にはタイマーが内蔵されている。Periodic モードで設定すると、
初期カウント値から 0 までデクリメントし、0 到達で割り込みを発生させ、
自動的にカウント値をリロードして繰り返す。

```c
/* i386/smpP.h */
#define MAX_TIMER_COUNT    0x00080000   /* = 524288 */
#define APIC_TIMER_DIV_16  0x03         /* 分周比: バスクロック / 16 */
#define APIC_TIMER_PERIODIC 0x20000     /* LVT Timer の Periodic ビット */

/* i386/smp.c -- BSP (CPU 0) の APIC タイマー初期化 */
apic_write(APIC_TIMER_DIV, APIC_TIMER_DIV_16);
apic_write(APIC_LVT_TIMER, APIC_TIMER_PERIODIC | VECT_SMP_TIMER0);
apic_write(APIC_TIMER_INIT_COUNT, MAX_TIMER_COUNT);

/* smp_ap_init() -- AP (CPU 1) の APIC タイマー初期化 */
apic_write(APIC_TIMER_DIV, APIC_TIMER_DIV_16);
apic_write(APIC_LVT_TIMER, APIC_TIMER_PERIODIC | VECT_SMP_TIMER1);
apic_write(APIC_TIMER_INIT_COUNT, MAX_TIMER_COUNT);
```

CPU 0 はベクタ 0x9A、CPU 1 はベクタ 0x9B で割り込みが入る。
0x80000 = 524288 カウントで、QEMU 環境では約 100Hz 相当。

---

## 3. 割り込み発生 — CPU の自動処理

ユーザータスク (Ring 3) の実行中にタイマー割り込みが発生すると、
CPU は以下の処理を **ハードウェアで自動的に** 行う:

### 3.1 特権レベルの切り替え

1. TSS (Task State Segment) から SS0 と ESP0 を読み出す
2. スタックポインタを Ring 0 カーネルスタックに切り替える
3. IDT (Interrupt Descriptor Table) からゲートディスクリプタを読む
4. ゲートタイプが Interrupt Gate (GT_INTR) なので IF (Interrupt Flag) をクリア
   → 割り込み禁止状態になる

### 3.2 割り込みフレームの自動プッシュ

CPU が Ring 0 スタックに 20 バイト (5 ダブルワード) を自動プッシュする:

```
  Ring 0 スタック (TSS の ESP0 から下方向に成長):

  高アドレス
  ┌──────────────────┐
  │ SS   (Ring 3)    │  ESP+16
  │ ESP  (Ring 3)    │  ESP+12   ← ユーザーモードのスタックポインタ
  │ EFLAGS           │  ESP+8    ← IF=1 だったフラグを保存
  │ CS   (Ring 3)    │  ESP+4    ← 0x5B (ユーザーコードセグメント)
  │ EIP  (戻り先)    │  ESP+0    ← 割り込まれた命令のアドレス
  └──────────────────┘
  低アドレス  ← ESP はここを指す
```

**重要**: この SS/ESP のプッシュは Ring 3→Ring 0 の特権レベル変更時のみ発生する。
Ring 0→Ring 0 の割り込みでは SS/ESP はプッシュされない。これが、すべてのタスクを
Ring 3 で実行しなければならない理由である（Ring 0 での割り込みでは、カーネルスタック上の
pt_regs フレームに SS/ESP が含まれず、`iret` でのスタック復元が行えない）。

### 3.3 IDT エントリ

IRQ0 (PIT) の場合:

```c
/* i386/interrupt.c */
set_idt(VECT_IRQ0, (unsigned long)intr_irq0, SEL_K32_C, 0, GT_INTR);
/*      ベクタ 0x80  ハンドラアドレス          カーネルCS    割り込みゲート */
```

GT_INTR (Interrupt Gate) を使っているため、割り込みゲートを通過した時点で
EFLAGS.IF が自動クリアされる。これにより、SAVE_ALL/RESTORE_ALL 実行中に別の
タイマー割り込みが入ることを防ぐ。

---

## 4. SAVE_ALL と intr_enter

すべての割り込みハンドラの先頭で、`SAVE_ALL` マクロによりレジスタをカーネル
スタックに退避し、`intr_enter` でネストカウンタをインクリメントする。

### 4.1 アセンブラスタブ (IRQ0 の例)

```asm
# i386/intr.s
intr_irq0:
    SAVE_ALL                # レジスタ退避 (pt_regs フレーム構築)
    call    intr_enter      # k_nest++
    call    c_intr_irq0     # C ハンドラ呼び出し
    jmp     intr_return     # intr_leave + RESTORE_ALL + iret
```

### 4.2 SAVE_ALL の処理

SAVE_ALL は 9 レジスタをカーネルスタックに push し、DS/ES をカーネルセグメント
にリロードする:

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

CPU が push した割り込みフレーム (EIP, CS, EFLAGS, ESP, SS) の下に
SAVE_ALL が 9 レジスタを追加し、完全な pt_regs フレームが構成される:

```
カーネルスタック (SAVE_ALL 後):

  Offset  Register    Pushed by
  ------  --------    ---------
  0x00    ES          SAVE_ALL
  0x04    DS          SAVE_ALL
  0x08    EDI         SAVE_ALL
  0x0C    ESI         SAVE_ALL
  0x10    EBP         SAVE_ALL
  0x14    EBX         SAVE_ALL
  0x18    EDX         SAVE_ALL
  0x1C    ECX         SAVE_ALL
  0x20    EAX         SAVE_ALL
  0x24    EIP         CPU (割り込まれた命令のアドレス)
  0x28    CS          CPU (0x5B = Ring 3 コードセグメント)
  0x2C    EFLAGS      CPU (割り込み前のフラグ、IF=1 含む)
  0x30    ESP         CPU (Ring 3 スタックポインタ)
  0x34    SS          CPU (0x6B = Ring 3 スタックセグメント)
```

CPU は Ring 3→Ring 0 遷移時に CS (IDT ゲートから) と SS (TSS.ss0 から) を
自動的にカーネル用セグメントに切り替えるが、DS と ES は変更しない。
SAVE_ALL 末尾の `movw $0x28, %ax / movw %ax, %ds / movw %ax, %es` により、
カーネル C コードの実行中はすべてのセグメントレジスタがカーネル用を指す:

- CS = 0x20 (K32_C) — CPU が IDT ゲートから自動設定
- SS = 0x30 (K32_S) — CPU が TSS.ss0 から自動設定
- DS = 0x28 (K32_D) — SAVE_ALL がリロード
- ES = 0x28 (K32_D) — SAVE_ALL がリロード

### 4.3 intr_enter — ネストカウンタのインクリメント

```asm
intr_enter:
    movl    APIC_ID_REG, %eax    # 0xFEE00020: APIC ID レジスタ
    shrl    $24, %eax            # bits[31:24] に APIC ID
    testl   %eax, %eax
    jnz     1f
    incl    k_nest0              # CPU 0: k_nest0++
    ret
1:
    incl    k_nest1              # CPU 1: k_nest1++
    ret
```

CPU ごとに独立したネストカウンタを持つ:

| CPU | ネストカウンタ | current_proc |
|-----|---------------|-------------|
| 0   | `k_nest0`    | `current_proc[0]` |
| 1   | `k_nest1`    | `current_proc[1]` |

---

## 5. C 割り込みハンドラ

### 5.1 PIT タイマー (IRQ0, CPU 0 のみ)

```c
/* i386/interrupt.c */
void c_intr_irq0(void)
{
    smp_lock(&kernel_lk);  /* Big Kernel Lock 取得 */
    timer_intr(0, 1);      /* apic=0, delta=1 ティック */
    i8259_reenable();       /* PIC + APIC に EOI 送信 */
    smp_unlock(&kernel_lk);
}
```

### 5.2 APIC タイマー (CPU 0)

```c
void c_intr_smp_timer0(void)
{
    smp_eoi();             /* APIC EOI のみ (PIC 不要) */
}
```

CPU 0 の APIC タイマーは `timer_intr` を呼ばない。PIT が既にティック処理を
行っているため、APIC タイマーは `next_tsk_flag` の確認（`intr_leave` 内の
`sched_next_tsk_check` で行われる）のみを担う。

### 5.3 APIC タイマー (CPU 1)

```c
void c_intr_smp_timer1(void)
{
    smp_eoi();             /* APIC EOI のみ */
}
```

CPU 0 と同様、CPU 1 の APIC タイマーも `timer_intr` を呼ばない。
タイムアウトキューの delta 減算は CPU 0 の PIT だけが行い、APIC タイマーは
プリエンプティブなタスクスイッチの契機 (`intr_leave` → `sched_next_tsk_check`)
を提供するのみ。もし両方のタイマーが `sched_timeout` を呼ぶと、delta が
二重に減算されてタイムアウト時間が不正確になる。

### 5.4 timer_intr — PIT タイマー割り込み処理

```c
/* i386/timer.c */
void timer_intr(unsigned char apic, unsigned long delta)
{
    if (apic == 0) {
        timer_ticks++;
        vga_write_dec_at(3, 21, timer_ticks, 10, 0x0B);  /* 画面更新 */
    }
    sched_timeout(apic, delta);    /* タイムアウト処理 */
}
```

### 5.5 sched_timeout — タイムアウト待ちタスクの起床

```c
/* kernel/sched.c (簡略化) */
void sched_timeout(W apic, unsigned long delta)
{
    tp = timeout.next;
    if (tp == &timeout) return;

    tp->delta -= delta;

    /* 満了した全エントリをループで処理 (delta=0 の同時満了を含む) */
    while (tp != &timeout && tp->delta <= 0) {
        next = tp->next;
        /* デルタ補正: 残余を次エントリに伝播 */
        if (next != &timeout) next->delta += tp->delta;
        /* リストから除去、自己参照にリセット */
        tp->prev->next = next; next->prev = tp->prev;
        tp->next = tp; tp->prev = tp;

        tsk_ptr = tlink2tsk(tp);
        if (tsk_ptr->tskstat == TTS_WAI) {
            /* オブジェクト待ちキューからも除去 */
            if (tsk_ptr->wlink.next != &(tsk_ptr->wlink))
                wlink_rem(&(tsk_ptr->wlink));
            proc_set_return_value(tsk_ptr->proc, E_TMOUT);
            tsk_ptr->tskstat = TTS_RDY;
            sched_ins(tsk_ptr->tskpri, &(tsk_ptr->plink));
            woken = 1;
        }
        tp = timeout.next;
    }
    if (woken) sched_next_tsk(apic);
}
```

詳細は [タイムアウト処理](timeout.md) を参照。

`sched_next_tsk` は **実際のタスクスイッチを行わない**。両 CPU の
`next_tsk_flag[]` を 1 に設定するだけである。実際のスイッチは `intr_leave` 内の
`sched_next_tsk_check` で行われる。

### 5.6 EOI (End of Interrupt) 送信

**PIC (i8259) の EOI:**

```c
/* i386/i8259.c */
void i8259_reenable(void)
{
    outb(IO_I8259_M, 0x20);    /* マスター PIC に非固有 EOI */
    outb(IO_I8259_S, 0x20);    /* スレーブ PIC に非固有 EOI */
    smp_eoi();                  /* APIC EOI も送信 */
}
```

**APIC の EOI:**

```c
/* i386/smp.c */
void smp_eoi(void)
{
    volatile unsigned long *eoi = (volatile unsigned long *)0xFEE000B0;
    *eoi = 0;    /* APIC EOI レジスタに 0 を書くだけ */
}
```

EOI を送らないと、PIC/APIC はその優先度以下の割り込みをブロックし続ける。
C ハンドラの **最後** に EOI を送るのが重要で、EOI 前に新しい割り込みが
入ることを防いでいる。

---

## 6. intr_leave と RESTORE_ALL

C ハンドラが戻ると、共通復帰パス `intr_return` に入る:

```asm
intr_return:
    call    intr_leave          # (1) ネスト管理 + タスクスイッチ
intr_return_restore:
    RESTORE_ALL                 # (2) レジスタ復元
    iret                        # (3) Ring 3 に復帰
```

### 6.1 intr_leave — ネストカウンタのデクリメントとタスクスイッチ

#### Step 1: CPU 判定とネストカウンタのデクリメント

```asm
intr_leave:
    movl    APIC_ID_REG, %eax    # 0xFEE00020
    shrl    $24, %eax
    testl   %eax, %eax
    jnz     intr_leave_cpu1

intr_leave_cpu0:
    decl    k_nest0
    jnz     intr_leave_done      # まだネスト中 → タスクスイッチなし
```

ネストカウンタが 0 でない場合（ネストした割り込みからの復帰）は、タスクスイッチの
判定を行わず、直接 RESTORE_ALL に進む。ネストした割り込みの途中でタスクを
切り替えると、外側の割り込みハンドラの状態が破壊されてしまうためである。

#### Step 2: 現タスクの ESP を保存

```asm
    movl    current_proc, %ebx   # EBX = current_proc[0] (proc_t*)
    movl    %esp, (%ebx)         # proc->kern_esp = ESP
```

現在の ESP をタスクの `kern_esp` に保存する。ESP はカーネルスタック上の
pt_regs フレームを指しており、このタスクの全レジスタ状態がここに保存されている。

#### Step 3: sched_next_tsk_check — タスクスイッチ判定

```asm
    pushl   $0                   # 引数: apic = 0
    call    sched_next_tsk_check
    addl    $4, %esp
```

`sched_next_tsk_check` の C 実装:

```c
/* i386/interrupt.c */
int sched_next_tsk_check(int apic)
{
    if (next_tsk_flag[apic] != 0) {
        old_proc = current_proc[apic];
        sched_do_next_tsk(apic);        /* 最高優先度の RDY タスクを選択 */
        next_tsk_flag[apic] = 0;
        if (old_proc != current_proc[apic])
            return 1;                   /* タスクスイッチ発生 */
    }
    return 0;                           /* スイッチなし */
}
```

`sched_do_next_tsk` は優先度キューを走査し、指定 CPU アフィニティを持つ
最高優先度の TTS_RDY タスクを見つけて `current_proc[apic]` を更新する。

#### Step 4: 新タスクの ESP をロード + TSS.esp0 更新

```asm
    movl    current_proc, %ebx   # EBX = (possibly new) current_proc[0]
    movl    (%ebx), %esp         # ESP = proc->kern_esp (新タスクのスタック)

    pushl   4(%ebx)              # kern_stack_top
    pushl   $0                   # cpu = 0
    call    tss_update_esp0      # TSS.esp0 を新タスクのカーネルスタック先頭に
    addl    $8, %esp
    ret                          # → intr_return_restore に戻る
```

タスクスイッチが発生した場合、`current_proc[cpu]` は **新しいタスク** の
proc_t を指している。ESP が新タスクの `kern_esp` に切り替わるため、
以降の RESTORE_ALL は新タスクのカーネルスタック上の pt_regs をpop する。

`tss_update_esp0` で TSS.esp0 を更新するのは、次回この新タスクに
割り込みが発生したとき、CPU が正しいカーネルスタックに切り替えるため。

### 6.2 RESTORE_ALL — レジスタの復元

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

SAVE_ALL の逆順で 9 レジスタを pop する。この時点の ESP は pt_regs の先頭
(オフセット 0x00 = ES) を指しており、pop 後は CPU が push した割り込みフレーム
(EIP, CS, EFLAGS, ESP, SS) だけが残る。

### 6.3 iret — Ring 3 への復帰

`iret` は以下を行う:
1. EIP, CS, EFLAGS をスタックからポップ
2. Ring 0→Ring 3 への特権遷移なので、さらに ESP, SS をポップ
3. EFLAGS にはIF=1 が保存されているため、割り込みが再び有効になる
4. ユーザータスクの実行が再開する

---

## 7. タスクスイッチの有無による分岐

### 7.1 スイッチなし — 同じタスクに復帰

タイマー割り込みが発生したが、起床すべきタスクも、より高優先度のタスクもない場合:

```
  SAVE_ALL:     Task A のレジスタをカーネルスタック A に push
  intr_enter:   k_nest++
  C handler:    timer_intr → sched_timeout (変化なし)
  intr_leave:   k_nest-- (=0)
                ESP を kern_esp に保存
                sched_next_tsk_check → 0 (スイッチなし)
                ESP を kern_esp からロード (同じ値)
  RESTORE_ALL:  カーネルスタック A からレジスタを pop
  iret:         Task A の EIP/ESP で実行再開
```

この場合、SAVE_ALL で push したレジスタをそのまま RESTORE_ALL で pop するだけで、
タスクの実行は割り込みがなかったかのように継続する。

### 7.2 スイッチあり — 別タスクに復帰

タイマー割り込みで `sched_timeout` がタスク B を起床させ、Task B が
Task A より高優先度である場合:

```
  SAVE_ALL:     Task A のレジスタをカーネルスタック A に push
  intr_enter:   k_nest++
  C handler:    timer_intr → sched_timeout → Task B を TTS_RDY に
                → sched_next_tsk(apic) で next_tsk_flag[cpu]=1
  intr_leave:   k_nest-- (=0)
                ESP をカーネルスタック A の kern_esp に保存
                sched_next_tsk_check
                  → sched_do_next_tsk
                    → Task B を発見 (TTS_RDY, 高優先度)
                    → current_proc[cpu] = &proc_B
                    → Task A を TTS_RDY に戻す
                  → return 1 (スイッチ発生)
                ESP = Task B の kern_esp (カーネルスタック B に切り替え)
                TSS.esp0 = Task B の kern_stack_top
  RESTORE_ALL:  カーネルスタック B からレジスタを pop (Task B の状態)
  iret:         Task B の EIP/ESP で実行再開
```

Task A の状態はカーネルスタック A 上の pt_regs フレームに凍結されたまま残る。
次に Task A がスケジュールされたとき、別の割り込みの `intr_leave` が ESP を
Task A の `kern_esp` に切り替え、RESTORE_ALL が Task A のレジスタを復元し、
Task A は中断された場所から実行を再開する。

---

## 8. 全体フロー図

以下は CPU 0 で PIT タイマー (IRQ0) が発生した場合の完全なフローである:

```
時間 ──────────────────────────────────────────────────────────────────→

Task A (Ring 3, CPU 0)
  │
  │  ← PIT 割り込み発生 (IRQ0)
  │
  ├── [CPU 自動処理] ─────────────────────────────────────────┐
  │   1. TSS.esp0 から Task A のカーネルスタック先頭を読む    │
  │   2. SS, ESP, EFLAGS, CS, EIP をカーネルスタックに push  │
  │   3. IDT[0x80] から intr_irq0 のアドレスを読む           │
  │   4. IF=0 (割り込みゲートのため自動クリア)               │
  │   5. CS:EIP を intr_irq0 に設定                          │
  ├───────────────────────────────────────────────────────────┘
  │
  ├── intr_irq0: ────────────────────────────────────────────┐
  │   │                                                      │
  │   │  SAVE_ALL                                            │
  │   │    └ 9 レジスタ (EAX〜ES) をカーネルスタックに push  │
  │   │      → pt_regs フレーム完成                          │
  │   │                                                      │
  │   │  call intr_enter                                     │
  │   │    └ k_nest0++                                       │
  │   │                                                      │
  │   │  call c_intr_irq0                                    │
  │   │    ├ timer_intr(0, 1)                                │
  │   │    │   ├ timer_ticks++                               │
  │   │    │   ├ vga_write_dec_at (画面更新)                 │
  │   │    │   └ sched_timeout(0, 1)                         │
  │   │    │       └ (タイムアウトがあれば) sched_next_tsk    │
  │   │    │           → next_tsk_flag[0] = 1                │
  │   │    │           → next_tsk_flag[1] = 1                │
  │   │    └ i8259_reenable()                                │
  │   │        ├ PIC マスター/スレーブに EOI                  │
  │   │        └ APIC EOI                                    │
  │   │                                                      │
  │   │  jmp intr_return                                     │
  │   │    call intr_leave                                   │
  │   │      ├ APIC ID → CPU 0                               │
  │   │      ├ k_nest0--                                     │
  │   │      ├ k_nest0 == 0:                                 │
  │   │      │   ├ proc_A.kern_esp = ESP                     │
  │   │      │   ├ sched_next_tsk_check(0)                   │
  │   │      │   │   ├ next_tsk_flag[0] != 0 ?               │
  │   │      │   │   │   YES → sched_do_next_tsk(0)         │
  │   │      │   │   │   current_proc[0] = (新タスクor同じ)  │
  │   │      │   │   │   NO  → return 0                     │
  │   │      │   ├ ESP = current_proc[0]->kern_esp           │
  │   │      │   │       (スイッチ時: 新タスクのスタックに)   │
  │   │      │   └ tss_update_esp0(0, kern_stack_top)        │
  │   │      └ ret → intr_return_restore                     │
  │   │                                                      │
  │   │    RESTORE_ALL                                       │
  │   │      └ 9 レジスタを (新タスクの) カーネルスタックから │
  │   │        pop                                           │
  │   │                                                      │
  │   │    iret                                              │
  │   │      ├ EIP, CS, EFLAGS をポップ                      │
  │   │      ├ ESP, SS をポップ (Ring 3 に復帰)              │
  │   │      └ IF=1 に復帰 (EFLAGS から)                    │
  │   │                                                      │
  └── └──────────────────────────────────────────────────────┘
  │
Task A or Task B (Ring 3, CPU 0)
  ← 割り込みがなかったかのように実行再開
```

---

## 参照ソースファイル

| ファイル           | 内容                                     |
|-------------------|------------------------------------------|
| i386/intr.s       | SAVE_ALL/RESTORE_ALL、intr_enter/intr_leave、割り込みスタブ |
| i386/interrupt.c  | C ハンドラ、sched_next_tsk_check         |
| i386/interrupt.h  | ベクタ定数 (VECT_IRQ0=0x80 等)           |
| i386/smpP.h       | APIC 定数 (MAX_TIMER_COUNT 等)           |
| i386/timer.c      | timer_init, timer_intr                   |
| i386/timerP.h     | PIT 定数 (FREQ, HZ, SQUARE)             |
| i386/smp.c        | APIC タイマー初期化、smp_eoi             |
| i386/i8259.c      | i8259_reenable (PIC EOI)                 |
| i386/i8259P.h     | EOI 定数 (0x20)                          |
| kernel/sched.c    | sched_do_next_tsk, sched_next_tsk, sched_timeout |
| i386/proc.h       | proc_t 構造体 (kern_esp, kern_stack_top, cpu), pt_regs |
