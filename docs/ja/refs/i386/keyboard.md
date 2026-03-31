# keyboard.md

対象ファイル: `i386/keyboard.c`, `i386/keyboard.h`, `i386/keyboardP.h`

## 概要

PS/2 キーボードドライバ。IRQ1 割り込みでスキャンコードを読み取り、ASCII 文字に変換して DTQ (データキュー) 経由でキーボードタスク (`kbd_task`) に送信する。ISR は `ipsnd_dtq` で DTQ 2 に文字を送り、kbd_task は `rcv_dtq` でブロッキング受信する。

US ASCII キーボードレイアウトのみをサポートする。SHIFT キーと CTRL キーの修飾状態を追跡する。

## 定数・マクロ

### keyboardP.h

| 定数 | 値 | 説明 |
|------|------|------|
| `SHIFT` | `0x1` | SHIFT キー押下状態を示すビットフラグ |
| `CTRL` | `0x2` | CTRL キー押下状態を示すビットフラグ |

### keyboard.c

### io.h (I/O ポートアドレス)

| 定数 | 値 | 説明 |
|------|------|------|
| `IO_KEY` | `0x60` | キーボードデータポート (スキャンコード読み取り) |
| `IO_KEY_CNT` | `0x61` | キーボードコントローラステータスポート |
| `IO_KEY_CS` | `0x64` | キーボードコントローラコマンド/ステータスポート |

## 構造体・型

なし。

## グローバル変数

| 変数名 | 型 | スコープ | 初期値 | 説明 |
|--------|------|----------|--------|------|
| `key_dtq_id` | `int` | `extern` | `0` | キーボード入力の送信先 DTQ ID。`kbd_task` が `set_key_task(2)` で設定。0 の場合は DTQ 送信を行わない |
| `mode` | `int` | `static` | `0` | 修飾キーの現在の状態 (`SHIFT`/`CTRL` ビットの組み合わせ) |
| `scode[76]` | `char[]` | `static` | (テーブル) | 通常時のスキャンコード→ASCII 変換テーブル |
| `scode_sh[76]` | `char[]` | `static` | (テーブル) | SHIFT 押下時のスキャンコード→ASCII 変換テーブル |

## 関数リファレンス

### key_init

```c
void key_init(void);
```

**概要:** キーボードドライバを初期化する。修飾キー状態と DTQ ID をリセットする。

**引数:** なし

**戻り値:** なし

**処理内容:**

1. `mode` を 0 にリセット (修飾キー状態クリア)
2. `key_dtq_id` を 0 にリセット
3. 初期化完了メッセージを printk で表示

**呼び出し元:** `main()` (`i386/main.c`)

**注意点:**
- この時点では IRQ1 はまだマスクされている。`key_start()` を呼ぶまでキーボード割り込みは発生しない。

---

### key_start

```c
void key_start(void);
```

**概要:** IRQ1 (キーボード割り込み) のマスクを解除してキーボード割り込みを有効にする。

**引数:** なし

**戻り値:** なし

**処理内容:**

1. `irq_mask_off(2)` を呼び出して IRQ1 のマスクを解除する

**呼び出し元:** `smp_ap_init()` (`i386/smp.c`) -- AP (CPU 1) の初期化完了後に呼び出される

**注意点:**
- `irq_mask_off()` の引数 `2` はビットマスク (ビット 1 = IRQ1) を示す。
- タイマー (`timer_start()`) と同じタイミングで有効化される。

---

### key_intr

```c
int key_intr(void);
```

**概要:** キーボード割り込みサービスルーチン (ISR)。スキャンコードを読み取り、ASCII 文字に変換して DTQ 経由でキーボードタスクに送信する。

**引数:** なし

**戻り値:** 常に `0`

**処理内容:**

1. **スキャンコード読み取り:** `IO_KEY` (0x60) からスキャンコードを読み取る
2. **コントローラ制御:** `IO_KEY_CS` (0x64) に `0xad` (キーボード無効化) → `0xae` (キーボード有効化) を送信してコントローラをリセット
3. **キーリリース処理:** スキャンコードのビット 7 が立っている場合 (キーリリース):
   - `0xaa` (左 SHIFT リリース) または `0xb6` (右 SHIFT リリース): `mode` から `SHIFT` ビットをクリア
   - `0x9d` (CTRL リリース): `mode` から `CTRL` ビットをクリア
   - いずれも 0 を返して終了
4. **修飾キー押下処理:**
   - `0x2a` (左 SHIFT) または `0x36` (右 SHIFT): `mode` に `SHIFT` ビットをセット、0 を返す
   - `0x1d` (CTRL): `mode` に `CTRL` ビットをセット、0 を返す
5. **Ctrl+C 処理:** `mode & CTRL` かつスキャンコード 0x2e ('c') の場合:
   - `ccli()` で割り込みを禁止
   - `vga_write_at` で "System halted by Ctrl+C" を VGA に表示
   - `IO_KEY_CS` (0x64) に `0xFE` を出力して CPU リセットパルスを送信
   - QEMU が `-no-reboot` オプション付きで起動されている場合、クリーンに終了する
   - `hlt` ループで停止 (フォールバック)
6. **ASCII 変換:** `mode & SHIFT` であれば `scode_sh[c]`、そうでなければ `scode[c]` でスキャンコードを ASCII 文字に変換
7. **DTQ 送信:** `key_dtq_id > 0` の場合、`ipsnd_dtq(0, key_dtq_id, ch)` でデータキューに文字を送信する。`rcv_dtq` で待機中の kbd_task があれば起床する

**呼び出し元:** `c_intr_irq1()` (`i386/interrupt.c`) -- IRQ1 割り込みハンドラ

**注意点:**
- ISR コンテキストで実行されるため、ブロッキング操作は禁止である。`ipsnd_dtq` は ISR 用の非ブロッキング DTQ 送信。
- `ipsnd_dtq(0, key_dtq_id, ch)` の第一引数 `0` は apic ID (CPU 0) を示す。IRQ1 は PIC 経由で CPU 0 にのみ配送されるが、`ipsnd_dtq` 内の `sched_next_tsk` が両 CPU の `next_tsk_flag` をセットするため、CPU 1 上のキーボードタスク (Task 4) も次の APIC タイマー割り込みで起床する。
- DTQ がフルの場合、`ipsnd_dtq` は先頭データを上書きして格納する (`fsnd_dtq` の仕様)。
- スキャンコードテーブルのインデックス範囲 (0-75) を超えるスキャンコードに対する境界チェックは行われていない。

## 補足

### スキャンコードテーブル (scode[])

US ASCII 配列の Set 1 スキャンコードに対応。主な対応は以下の通り:

| スキャンコード | 通常 | SHIFT |
|----------------|------|-------|
| 0x02-0x0B | `1234567890` | `!@#$%^&*()` |
| 0x10-0x19 | `qwertyuiop` | `QWERTYUIOP` |
| 0x1E-0x26 | `asdfghjkl` | `ASDFGHJKL` |
| 0x2C-0x35 | `zxcvbnm,./` | `ZXCVBNM<>?` |
| 0x39 | ` ` (Space) | ` ` (Space) |
| 0x0E | `\b` (Backspace) | `\b` (Backspace) |
| 0x0F | `\t` (Tab) | `\t` (Tab) |
| 0x1C | `\r` (Enter) | `\r` (Enter) |

### Ctrl+C シャットダウン

Ctrl キーを押しながら 'C' キー (スキャンコード 0x2e) を押すと、カーネルが停止する。
ISR コンテキスト (Ring 0) で `outb(0x64, 0xFE)` によりキーボードコントローラ経由で
CPU リセットパルスを送信する。QEMU の `-no-reboot` オプションと組み合わせて、
QEMU プロセスをクリーンに終了させる仕組みである。

### 割り込みフロー

```
IRQ1 発生 (CPU 0)
  -> intr_irq1 (intr.s)
    -> save
    -> c_intr_irq1 (interrupt.c)
      -> key_intr()        -- スキャンコード処理 (Ctrl+C 判定含む)、ipsnd_dtq で DTQ 送信
      -> i8259_reenable()  -- EOI 送信
    -> intr_leave           -- タスクスイッチ判定
    -> RESTORE_ALL + iret
```

### キーボードタスク連携

- `key_dtq_id` は `kbd_task` (Task 4) が起動時に `set_key_task(2)` で DTQ ID を設定する
- `kbd_task` は `rcv_dtq(2)` でブロッキング待ち → ISR の `ipsnd_dtq` で DTQ に文字到着 → 起床 → VGA エコー
- kbd_task は文字をローカルバッファに蓄積し、Enter で `psnd_mbf(1, buf, len)` により MBF 1 経由で Task 1 に行文字列を送信
