# キーボード入力

キーボードのキー押下がどのようにユーザータスクまで届くかを、
ハードウェア (i8259 + 8042) からユーザータスク (kbd_task) まで一貫して追跡する。

## 1. 全体フロー

```
  キー押下
    |
    v
  8042 コントローラ (I/O 0x60, 0x64)
    |  IRQ1 信号
    v
  i8259 PIC (マスタ)         ... CPU 0 にのみ配送
    |  割り込みベクタ 0x81
    v
  CPU 0: Ring 3 → Ring 0     ... 特権遷移、カーネルスタックに切り替え
    |
    v
  intr_irq1 (intr.s)         ... SAVE_ALL → intr_enter → call c_intr_irq1
    |
    v
  c_intr_irq1 (interrupt.c)  ... key_intr() + i8259_reenable()
    |
    v
  key_intr (keyboard.c)      ... スキャンコード読み取り → ASCII 変換
    |                             → ipsnd_dtq(0, key_dtq_id, ch) で DTQ 2 に送信
    v
  ipsnd_dtq (sys_dtq.c)     ... DTQ 2 に文字を格納、rcv_dtq で待機中の Task 4 を起床
    |                             Task 4: TTS_WAI → TTS_RDY、sched_next_tsk(0) で
    |                             両 CPU の next_tsk_flag をセット
    v
  intr_return (intr.s)        ... intr_leave → RESTORE_ALL → iret (CPU 0 は元タスクに復帰)
    :
    : (CPU 1 の次の APIC タイマー割り込み)
    v
  CPU 1: intr_leave           ... next_tsk_flag[1] != 0 → sched_do_next_tsk
    |                             → Task 2 の ESP を保存、Task 4 の ESP をロード
    v
  kbd_task (user.c)           ... rcv_dtq(2) が文字を返す → ローカルバッファに蓄積
    |                             → Enter で psnd_mbf(1, buf, len) → MBF 1 経由 Task 1 に送信
    v
  画面に文字が表示される
```

## 2. ハードウェア: 8042 キーボードコントローラ

PC のキーボード入力は Intel 8042 互換コントローラが管理する。

| I/O ポート | 用途 |
|---|---|
| `0x60` (IO_KEY) | データレジスタ: スキャンコードの読み取り |
| `0x64` (IO_KEY_CS) | コマンド/ステータスレジスタ |

キーを押すと 8042 がスキャンコード (make code) を 0x60 に格納し、
IRQ1 をアサートする。キーを離すと release code (make code | 0x80) が送られる。

## 3. IRQ1 の配送経路

IRQ1 は i8259 PIC マスタの IR1 に接続されている。tiny-itron では PIC を維持し、
I/O APIC は使わないため、**外部 IRQ はすべて CPU 0 (BSP) にのみ配送される**。

これは物理配線による制約である。i8259 PIC の出力 (INTR 信号) は BSP (CPU 0) の
Local APIC の LINT0 ピンに直結されており、AP (CPU 1) には接続されていない。
I/O APIC を使えば IRQ を任意の CPU にルーティングできるが、
tiny-itron では複雑さを避けるため I/O APIC を使わない設計としている。

IRQ1 のベクタ番号は PIC リマップにより **0x81** (= 0x80 + 1) に設定されている
(i8259_init で ICW2 = 0x80)。

### IRQ マスク

起動時は全 IRQ がマスクされている。`key_start()` が `irq_mask_off(2)`
(ビットマスク 0x02 = IR1) を呼んで IRQ1 をアンマスクする。

```c
/* keyboard.c */
void key_start(void) {
    irq_mask_off(2);    /* bit 1 = IRQ1 */
}
```

## 4. 割り込みハンドラ (Ring 0)

IRQ1 が CPU 0 に到着すると、IDT ベクタ 0x81 → `intr_irq1` が呼ばれる。
他の IRQ と同じ標準パターンで処理される:

```asm
# intr.s
intr_irq1:
    SAVE_ALL                # 9 レジスタを per-task カーネルスタックに push
    call  intr_enter        # k_nest[0]++
    call  c_intr_irq1       # C ハンドラ呼び出し
    jmp   intr_return       # intr_leave → RESTORE_ALL → iret
```

C ハンドラ `c_intr_irq1` (interrupt.c) は `key_intr()` を呼び、PIC に EOI を送る:

```c
/* interrupt.c */
void c_intr_irq1(void) {
    smp_lock(&kernel_lk);
    key_intr();
    i8259_reenable();
    smp_unlock(&kernel_lk);
}
```

### なぜ sti しないのか

キーボードハンドラ内で `sti` を呼ぶと、処理中に APIC タイマー等が
ネスト割り込みとして発生する。Ring 0→Ring 0 の割り込みでは CPU が
SS/ESP を push しないため、SAVE_ALL が想定する 14 ワードの pt_regs
フレームが成立せず、レジスタ破壊を引き起こす。
したがって割り込み禁止のまま処理する (IDT のゲートタイプが Interrupt Gate
なので CPU が自動的に IF=0 にする)。

## 5. スキャンコード → ASCII 変換

`key_intr()` (keyboard.c) がキーボードの全処理を担う。

### 5.1 スキャンコード読み取り

```c
c = inb(IO_KEY);           /* 0x60 からスキャンコード読み取り */
outb(IO_KEY_CS, 0xad);     /* キーボード無効化 (処理中の再入防止) */
outb(IO_KEY_CS, 0xae);     /* キーボード再有効化 */
```

### 5.2 修飾キーの追跡

`mode` 変数 (static int) で Shift/Ctrl の状態を追跡する:

| スキャンコード | イベント |
|---|---|
| 0x2a, 0x36 (make) | `mode \|= SHIFT` |
| 0xaa, 0xb6 (release) | `mode &= ~SHIFT` |
| 0x1d (make) | `mode \|= CTRL` |
| 0x9d (release) | `mode &= ~CTRL` |

修飾キー自体は DTQ に送信せず、`return 0` で戻る。

### 5.3 Ctrl+C による停止

Ctrl を押しながら `c` (スキャンコード 0x2e) を押すと、CPU をリセットする:

```c
if ((mode & CTRL) && c == 0x2e) {
    __asm__("cli");
    vga_write_at(12, 28, "  System halted.  ", 0x4F);
    outb(IO_KEY_CS, 0xFE);     /* 8042 経由で CPU リセットライン pulse */
    while (1) { __asm__("hlt"); }
}
```

QEMU を `-no-reboot` で起動していればクリーンに終了する。

### 5.4 ASCII 変換テーブル

`keyboardP.h` に US ASCII キーボード用のスキャンコード→ASCII テーブルが 2 つ定義
されている:

- `scode[]` — 通常状態
- `scode_sh[]` — Shift 押下状態

```c
ch = (mode & SHIFT) ? scode_sh[c] : scode[c];
```

## 6. ISR から DTQ への送信 (ipsnd_dtq)

ASCII 変換後、`key_intr()` は `ipsnd_dtq()` でデータキュー (DTQ 2) に文字を送る:

```c
/* keyboard.c */
if (key_dtq_id > 0)
    ipsnd_dtq(0, key_dtq_id, ch);
```

`key_dtq_id` はグローバル変数で、kbd_task が起動時に `set_key_task(2)` で
DTQ ID = 2 を登録する。kbd_task 起動前は `key_dtq_id = 0` なので、
キー押下があっても DTQ 送信は行われない。

`ipsnd_dtq()` (sys_dtq.c) は `fsnd_dtq` の ISR 版で、以下を行う
(caller holds `kernel_lk` — `c_intr_irq1` で取得済み):

1. DTQ 2 に文字データを格納
2. `rcv_dtq(2)` で待機中の Task 4 がいれば起床 (TTS_WAI → TTS_RDY)
3. スケジューラキューに挿入
4. `sched_next_tsk(0)` で **両 CPU** の `next_tsk_flag[]` をセット

### なぜ「両 CPU」なのか

IRQ1 は CPU 0 で処理されるが、kbd_task は CPU 1 で動作する。
`sched_next_tsk()` は全 CPU の `next_tsk_flag` をセットするため、
CPU 1 の次の APIC タイマー割り込みの `intr_leave` でタスクスイッチが発生し、
Task 4 が CPU 1 上で起床する。

```
  CPU 0 (IRQ1 処理中)              CPU 1 (Task 2 実行中)
  ─────────────────               ─────────────────
  key_intr()                       ...busy loop...
    ipsnd_dtq(0, 2, ch)
      DTQ 2 に格納
      Task 4 を TTS_RDY に
      sched_next_tsk(0)
        next_tsk_flag[0] = 1
        next_tsk_flag[1] = 1       (次の APIC タイマーで参照)
  intr_leave:
    next_tsk_flag[0] を確認          APIC タイマー割り込み
    → Task 4 は CPU 1 なので         intr_leave:
      CPU 0 では切り替えなし           next_tsk_flag[1] != 0
                                      → sched_do_next_tsk(1)
                                      → Task 4 (優先度 1) > Task 2 (優先度 15)
                                      → Task 4 に切り替え
```

## 7. DTQ + MBF の二段構成

キーボード入力には DTQ 2 (文字単位) と MBF 1 (行単位) の 2 つの通信路が使われる:

```
  key_intr (ISR)    DTQ 2 (16要素)    kbd_task (Task 4)    MBF 1 (256B)     first_task (Task 1)
  ──────────────    ──────────────    ─────────────────    ─────────────    ──────────────────
  ipsnd_dtq(0,2,ch) ──→  [ch]  ──→  rcv_dtq(2, &data)
                                     line_buf に蓄積
                                     Enter or 行末ラップ
                                     psnd_mbf(1, buf, len) ──→ [line] ──→ trcv_mbf(1, buf, 20)
```

- **DTQ 2** (ISR → Task 4): `second_task` が `cre_dtq(2, ...)` で作成 (16 要素)。
  ISR は `ipsnd_dtq` (非ブロッキング)、Task 4 は `rcv_dtq` (ブロッキング) で使用
- **MBF 1** (Task 4 → Task 1): `first_task` が `cre_mbf(1, ...)` で作成
  (maxmsz=64, mbfsz=256)。Task 4 は `psnd_mbf` (非ブロッキング) で行文字列を送信、
  Task 1 は `trcv_mbf` (タイムアウト付きブロッキング) で受信

## 8. ユーザータスク側の API

kbd_task はユーザー空間 (Ring 3) で動作する。

### 8.1 set_key_task(dtq_id) — DTQ ID の登録

```
kbd_task (Ring 3)
  → set_key_task(2)                    lib/lib_exd.c: syscall(-TFN_EXD_KEY_SETTASK, 2)
    → int $0x99                        klib.s → intr_syscall
      → sys_key_set_task(apic, 2)      kernel/sys_exd.c: key_dtq_id = 2
```

ISR の `key_intr()` はこのグローバル変数 `key_dtq_id` を参照して
送信先の DTQ を決定する。kbd_task が起動前は `key_dtq_id = 0` なので、
キー押下があっても DTQ 送信は行われない。

### 8.2 kbd_task のメインループ

```c
/* user.c: kbd_task (Task 4, CPU 1, 優先度 1) */
set_key_task(2);               /* ISR に DTQ ID を通知 */

int  line_pos = 0;
char line_buf[64];             /* Enter まで蓄積するバッファ */

while (1) {
    rcv_dtq(2, &data);         /* DTQ 2 をブロッキング受信 (キーが来るまで TTS_WAI) */
    c = (int)data;

    if (c >= ' ' && c < 0x7f && line_pos < 63) {
        /* 印字可能文字: 画面にエコー + バッファに蓄積 */
        print_at(ROW_KBD, kbd_col, s, ATTR_YELLOW);
        line_buf[line_pos++] = (char)c;
        kbd_col++;
        if (kbd_col >= kbd_max) {
            /* 行末ラップ: MBF 送信 + クリア */
            psnd_mbf(1, line_buf, line_pos);
            fill_at(ROW_KBD, 20, kbd_max - 20, ' ', 0x0E);
            kbd_col = 20;  line_pos = 0;
        }
    } else if (c == '\r') {
        /* Enter: MBF 1 に行文字列を送信 */
        if (line_pos > 0)
            psnd_mbf(1, line_buf, line_pos);
        fill_at(ROW_KBD, 20, kbd_max - 20, ' ', 0x0E);
        kbd_col = 20;  line_pos = 0;
    } else if (c == '\b') {
        /* Backspace: バッファとカーソルを 1 つ戻す */
        if (kbd_col > 20 && line_pos > 0) {
            kbd_col--;  line_pos--;
            fill_at(ROW_KBD, kbd_col, 1, ' ', 0x0E);
        }
    }
}
```

kbd_task は 1 文字ずつ DTQ 2 から受信し、ローカルバッファ (`line_buf`) に蓄積する。
Enter キー (`'\r'`) が押されるか、行末 (76 カラム) に達すると、
`psnd_mbf(1, line_buf, line_pos)` で行文字列を MBF 1 に送信し、
画面のエコー行をクリアする。Backspace (`'\b'`) にも対応する。

Task 4 は優先度 1 (最高) なので、起床すると CPU 1 上の Task 2 (優先度 15) を
即座にプリエンプトする。文字を処理したら `rcv_dtq(2)` で再びブロックし、
Task 2 に CPU を返す。

## 9. MBF による行単位のタスク間転送

kbd_task は蓄積した行文字列をメッセージバッファ (MBF 1) で first_task に送信する:

```
  kbd_task (CPU 1)                  first_task (CPU 0)
  ─────────────────                 ─────────────────
  psnd_mbf(1, line_buf, line_pos)   trcv_mbf(1, mbf_buf, 20)
  (非ブロッキング送信、行単位)        (タイムアウト付き受信、20 tick ≈ 0.33s)
                                     戻り値 = メッセージサイズ (> 0) or E_TMOUT
```

first_task (Task 1) はメインループの各反復で `trcv_mbf()` を呼び、
行文字列が来ればメッセージサイズ (正の値) を受信し、来なければ 20 tick 後に
タイムアウト (E_TMOUT) で復帰する。受信した文字列は画面の Row 5 に表示する
(44 文字に切り詰め)。これにより CPU 0 側でもキーボード入力を確認できる。

MBF (メッセージバッファ) は可変長メッセージをカーネル内リングバッファで
コピー転送する ITRON オブジェクトで、DTQ (整数1個) と異なり文字列全体を
一度に送受信できる。

## 10. ソースファイル一覧

| ファイル | 役割 |
|---|---|
| `i386/keyboard.c` | key_init, key_start, key_intr (カーネル側) |
| `i386/keyboardP.h` | スキャンコード→ASCII テーブル、修飾キー定数 |
| `i386/keyboard.h` | keyboard.c の公開ヘッダ |
| `i386/interrupt.c` | c_intr_irq1 (IRQ1 の C ハンドラ) |
| `i386/intr.s` | intr_irq1 (IRQ1 のアセンブリエントリポイント) |
| `kernel/sys_dtq.c` | ipsnd_dtq, rcv_dtq (DTQ 送受信) |
| `kernel/sys_mbf.c` | psnd_mbf, trcv_mbf (MBF 送受信) |
| `kernel/sys_exd.c` | sys_key_set_task (DTQ ID 登録 syscall ハンドラ) |
| `lib/lib_exd.c` | set_key_task (ユーザー空間ラッパー) |
| `kernel/user.c` | kbd_task (Task 4 の実装) |

## 関連ドキュメント

- [context-switch.md](context-switch.md) — SAVE_ALL/RESTORE_ALL と intr_leave の詳細
- [syscall.md](syscall.md) — syscall の全フロー
- [timer-interrupt.md](timer-interrupt.md) — APIC タイマーと intr_leave によるタスクスイッチ
- [smp-basics.md](smp-basics.md) — CPU 間の next_tsk_flag 伝播
- [itron-guide.md](itron-guide.md) — DTQ, slp_tsk/wup_tsk の ITRON API 解説
