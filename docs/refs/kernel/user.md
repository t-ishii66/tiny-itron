# user.c

対象ファイル: `kernel/user.c`

## 概要

ユーザータスクおよび教育用デモアプリケーションの実装。SMP (2 CPU) 環境で複数タスクがマルチタスクで動作する様子を VGA 画面上に表示し、GDB でのデバッグ観察を容易にする。

CPU 0 では Task 1 と Task 3 が `wup_tsk`/`slp_tsk` で交互に実行され、CPU 1 では Task 2 が連続実行し、Task 4 (キーボード) が最高優先度でプリエンプションする。Task 2 と Task 3 はバイナリセマフォ (sem 1) で `shared_count` への排他アクセスを行い、CPU 間の競合デモを実現する。

ユーザータスクからの VGA アクセスはすべて非 ITRON syscall (`print_at`, `print_dec_at`, `clear_screen`, `fill_at`) を経由する。直接の `vga_write_at()` 呼び出しは行わない。ヘッダインクルードは `#include "../include/exd.h"` を使用し、`#include "../i386/video.h"` は不要。

### タスク構成

| タスク ID | 関数 | CPU | 優先度 | 役割 |
|-----------|------|-----|--------|------|
| 1 | first_task | 0 | 15 (TMAX_TPRI - 1) | セマフォ・MBF・Task3 生成、カウントアップ |
| 2 | second_task | 1 | 15 (TMAX_TPRI - 1) | Task4 生成、カウントアップ + セマフォ |
| 3 | usr_main | 0 | 15 (TMAX_TPRI - 1) | カウントアップ + セマフォ |
| 4 | kbd_task | 1 | 1 (最高) | キーボード入力エコー |
| 5 | idle_task | 0 | 16 (TMAX_TPRI) | アイドルタスク (pause ループ) |
| 6 | idle_task | 1 | 16 (TMAX_TPRI) | アイドルタスク (pause ループ) |

## 定数・マクロ

### VGA カラー属性

| 定数 | 値 | 色 |
|------|-----|-----|
| ATTR_WHITE | 0x0F | 白 (高輝度) |
| ATTR_GREEN | 0x0A | 緑 |
| ATTR_CYAN | 0x0B | シアン |
| ATTR_YELLOW | 0x0E | 黄 |
| ATTR_GREY | 0x07 | グレー (通常輝度) |
| ATTR_DARK | 0x08 | 暗いグレー |
| ATTR_MAGENTA | 0x0D | マゼンタ |
| ATTR_RED | 0x0C | 赤 |

### 画面レイアウト行番号

| 定数 | 値 | 表示内容 |
|------|-----|---------|
| ROW_HEADER | 0 | ヘッダ行 ("TinyITRON/386 SMP (2 CPU)") |
| ROW_SEP | 1 | 区切り線 |
| ROW_TIMER | 3 | タイマーティック表示 (ISR が更新) |
| ROW_TASK1 | 5 | [CPU0] Task1 カウント + MBF 受信文字列 |
| ROW_TASK3 | 7 | [CPU0] Task3 カウント + LOCK/BUSY/---- |
| ROW_ARROW_UP | 8 | Task3→Shared 間の対角線 (`\`) |
| ROW_SHARED | 9 | 共有カウンタ (sem 1 保護) |
| ROW_ARROW_DN | 10 | Task2→Shared 間の対角線 (`/`) |
| ROW_TASK2 | 11 | [CPU1] Task2 カウント + LOCK/BUSY/---- |
| ROW_KBD | 13 | [CPU1] Task4 キーボード入力エコー |
| ROW_COPYRIGHT | 24 | 著作権表示 |

### タイミング定数

| 定数 | 値 | 説明 |
|------|-----|------|
| DELAY_COUNT | 20000000UL | ビジーウェイトのループ回数 |
| COUNTER_WRAP | 10000000UL | task_count のラップアラウンド上限 (10M で 0 に戻る) |
| SEM_HOLD | 20 | セマフォ保持中の shared_count インクリメント回数 |
| SEM_REST | 40 | セマフォ解放後、次の取得試行までのスキップ回数 |

### スピナー文字配列

```c
char spin_chars[] = { '|', '/', '-', '\\' };
```

各タスクのカウンタ表示の横にスピナーアニメーション (`|`, `/`, `-`, `\`) を表示する。`task_count[id] & 3` でインデックスを決定し、タスクが動作中であることを視覚的に示す。

## 構造体・型

本ファイル固有の構造体定義はない。

## グローバル変数

| 変数名 | 型 | 説明 |
|--------|-----|------|
| `shared_count` | `unsigned long` | セマフォ (sem 1) で保護された共有カウンタ。Task 2 と Task 3 が競合アクセスする。GDB で `p shared_count` で確認可能。`.user_data` セクション配置 (`__attribute__((section(".user_data")))`) |
| `task_count[MAX_TSKID]` | `unsigned long[16]` | タスクごとの実行回数。インデックスはタスク ID (1〜4 が有効)。GDB で `p task_count[1]` 等で確認可能。`.user_data` セクション配置 (`__attribute__((section(".user_data")))`) |

## 関数リファレンス

### delay

```c
static void delay(void);
```

**概要:** ビジーウェイトによる遅延。DELAY_COUNT (20000000) 回のループを実行する。

**引数:** なし

**戻り値:** なし (void)

**処理内容:**
volatile 修飾された変数 k を 0 から DELAY_COUNT まで空ループする。volatile により最適化で除去されることを防ぐ。

**呼び出し元:** `first_task()`, `usr_main()`, `second_task()`

**注意点:** 実際の遅延時間は CPU クロックと QEMU のエミュレーション速度に依存する。`dly_tsk` は実装済みだが、ビジーウェイトはタスクの実行回数カウントアップ中に CPU を占有し続けるため、意図的に使用している。

---

### draw_header

```c
static void draw_header(void);
```

**概要:** VGA 画面の初期レイアウトを描画する。

**引数:** なし

**戻り値:** なし (void)

**処理内容:**
1. `clear_screen()` (syscall) で画面をクリア
2. ROW_HEADER: "TinyITRON/386 SMP (2 CPU)" と "[Ctrl+C to quit]" を `print_at()` で表示
3. ROW_SEP: 区切り線 (=) を表示
4. ROW_TIMER: "Timer" "tick =" ラベルを表示
5. ROW_TASK1: "[CPU0] Task1 # mbf: -" を表示
6. ROW_TASK3: "[CPU0] Task3 # ----" を表示
7. ROW_SHARED: "Shared (sem 1) #" を表示
8. ROW_TASK2: "[CPU1] Task2 # ----" を表示
9. ROW_KBD: "[CPU1] Task4 >" を表示
10. ROW_COPYRIGHT: 著作権表示を表示

**呼び出し元:** `first_task()`

**注意点:** すべての VGA 表示は `clear_screen()` および `print_at()` syscall を経由する。直接の VGA メモリアクセスは行わない。

---

### first_task

```c
void first_task(void);
```

**概要:** Task 1 (ID=1, CPU 0, 優先度 TMAX_TPRI-1)。セマフォ、メッセージバッファ、Task 3 を生成し、メインループでカウントアップしながら Task 3 と交互に実行する。

**引数:** なし (proc_init() で直接登録)

**戻り値:** なし (void、無限ループ)

**処理内容:**

初期化フェーズ:
1. バイナリセマフォ (sem 1) を生成: `cre_sem(1, &csem)` (TA_TFIFO, isemcnt=1, maxsem=1)
2. メッセージバッファ (MBF 1) を生成: `cre_mbf(1, &cmbf)` (TA_TFIFO, maxmsz=64, mbfsz=256, mbf=NULL でカーネル割り当て)
3. Task 3 (usr_main) を生成: `cre_tsk(3, &ctsk)` + `act_tsk(3)` (スタック `tsk_stack_alloc(1024)` で確保、優先度 TMAX_TPRI-1)
4. `draw_header()` で画面初期描画

メインループ:
1. `task_count[1]++` でカウンタをインクリメント
2. `print_dec_at()` でカウンタ値を VGA に表示
3. `trcv_mbf(1, mbf_buf, 20)` で MBF 1 をタイムアウト付き受信し、kbd_task から送られた行文字列を受信。受信した文字列を `print_at()` で画面に表示 (44 文字に切り詰め)。タイムアウト (20 tick) がペーシングの役割を果たす
4. `wup_tsk(3)` で Task 3 を起床
5. `slp_tsk()` で自タスクを休止 (Task 3 に実行を譲る)

**呼び出し元:** `proc_init()` (i386/proc.c) で CPU 0 の最初のタスクとして登録

**注意点:**
- Task 1 と Task 3 は wup_tsk/slp_tsk で交互実行するピンポンパターン
- MBF 1 はタイムアウト付きブロッキング受信 (trcv_mbf) を使用。メッセージがなければ 20 tick 後に E_TMOUT で復帰
- `trcv_mbf` の戻り値は `ER_UINT` (unsigned) なので、E_TMOUT (-50) との比較には `(int)` キャストが必要

---

### usr_main

```c
void usr_main(VP_INT arg);
```

**概要:** Task 3 (ID=3, CPU 0, 優先度 TMAX_TPRI-1)。Task 2 (CPU 1) とバイナリセマフォ (sem 1) を競合的に使用し、shared_count を保護する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `arg` | `VP_INT` | タスク拡張情報 (未使用) |

**戻り値:** なし (void、無限ループ)

**処理内容:**

初期化:
1. `slp_tsk()` で Task 1 からの最初の起床を待つ

メインループ (3 フェーズのステートマシン):
1. **LOCK フェーズ** (`have_sem == 1`):
   - セマフォを保持中。`shared_count++` を実行し、`print_dec_at()` で緑色で表示
   - SEM_HOLD (20) 回インクリメントしたら `sig_sem(1)` でセマフォを解放
   - `print_at()` で表示を "----" に変更
2. **REST フェーズ** (`have_sem == 0`, `phase_cnt < SEM_REST`):
   - セマフォを触らない休息期間。phase_cnt をインクリメント
3. **ACQUIRE フェーズ** (`phase_cnt >= SEM_REST`):
   - `pol_sem(1)` で非ブロッキング取得を試みる
   - 成功: `print_at()` で "LOCK" (黄色) を表示、shared_count をインクリメント
   - 失敗: `print_at()` で "BUSY" (赤色) を表示 (他 CPU がロック中)

各フェーズ後:
- `delay()` でビジーウェイト
- `wup_tsk(1)` で Task 1 を起床
- `slp_tsk()` で自タスクを休止

**呼び出し元:** タスクディスパッチャ (`cre_tsk` に渡した関数ポインタで実行開始)

**注意点:**
- `pol_sem` (非ブロッキング) を使用する理由: wai_sem (ブロッキング) を使うと、CPU 0 で唯一の実行タスクがブロックされる状況が生じ得るため回避
- shared_count の色が緑 (Task 3) かマゼンタ (Task 2) かで、どちらが最後にインクリメントしたかが視覚的にわかる

---

---

### second_task

```c
void second_task(void);
```

**概要:** Task 2 (ID=2, CPU 1, 優先度 TMAX_TPRI-1)。Task 4 (キーボード) を生成し、メインループでカウントアップしながらセマフォを競合使用する。

**引数:** なし (proc_init() で直接登録)

**戻り値:** なし (void、無限ループ)

**処理内容:**

初期化フェーズ:
1. Task 4 (kbd_task) を生成: `cre_tsk(4, &ctsk)` + `act_tsk(4)` (スタック `tsk_stack_alloc(1024)` で確保、優先度 1 = 最高)

メインループ (3 フェーズのステートマシン、usr_main と同じ構造):
1. **LOCK フェーズ**: セマフォ保持中に shared_count を `print_dec_at()` でマゼンタ色で表示
2. **REST フェーズ**: 休息期間 (SEM_REST 回スキップ)
3. **ACQUIRE フェーズ**: `pol_sem(1)` で非ブロッキング取得を試行。`print_at()` で LOCK/BUSY を表示

各フェーズ後:
- `delay()` でビジーウェイト
- Task 1 と異なり、slp_tsk を使用しない (連続実行)

**呼び出し元:** `proc_init()` (i386/proc.c) で CPU 1 の最初のタスクとして登録

**注意点:**
- Task 2 は slp_tsk を呼ばず、delay() のみで制御されるため、CPU 1 を継続的に使用する
- Task 4 (優先度 1) がキーボード入力時にプリエンプションする

---

### idle_task

```c
void idle_task(void);
```

**概要:** アイドルタスク (Task 5/6)。各 CPU で最低優先度 (TMAX_TPRI) で常に RDY 状態を維持する。

**引数:** なし

**戻り値:** なし (void、無限ループ)

**処理内容:** 無限ループで `pause` 命令を実行する。pause はスピンウェイト時の CPU 電力消費を軽減するヒント命令。

**呼び出し元:** `proc_init()` (i386/proc.c) で Task 5 (CPU 0), Task 6 (CPU 1) として登録

**注意点:**
- 全ユーザータスクが WAI/SLP 状態のとき、sched_do_next_tsk が RDY タスクを見つけられないことを防ぐ安全弁
- idle_task がないと、CPU が「ゴースト実行」(WAI 状態のタスクの文脈で走り続ける) する問題が発生する

---

### kbd_task

```c
void kbd_task(VP_INT arg);
```

**概要:** Task 4 (ID=4, CPU 1, 優先度 1 = 最高)。キーボード入力をエコー表示し、行単位でメッセージバッファ (MBF 1) 経由で Task 1 に送信する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `arg` | `VP_INT` | タスク拡張情報 (未使用) |

**戻り値:** なし (void、無限ループ)

**処理内容:**

初期化:
1. `set_key_task(2)` (syscall) でキーボード DTQ ID を設定 (キーボード ISR がどの DTQ に `ipsnd_dtq` で文字を送るか知るため)

メインループ:
1. `rcv_dtq(2, &data)` で DTQ 2 をブロッキング受信 (キーが来るまで TTS_WAI)
2. 印刷可能文字 (0x20〜0x7e) の場合:
   - `print_at()` (syscall) で ROW_KBD 行に黄色でエコー表示
   - `line_buf[line_pos++]` にバッファリング
   - `kbd_col` を進める。右端 (76 カラム) に達したら `psnd_mbf(1, line_buf, line_pos)` で MBF 1 に送信し、20 カラム目から再開。`fill_at()` (syscall) で行クリア
3. Enter (`'\r'`) の場合:
   - `line_pos > 0` なら `psnd_mbf(1, line_buf, line_pos)` で MBF 1 に行送信
   - `fill_at()` で行クリア、カーソルとバッファをリセット
4. Backspace (`'\b'`) の場合:
   - `kbd_col > 20 && line_pos > 0` なら、カーソルとバッファを 1 つ戻し `fill_at()` で 1 文字クリア

**呼び出し元:** タスクディスパッチャ (`cre_tsk` に渡した関数ポインタで実行開始)

**注意点:**
- IRQ1 は PIC ルーティングにより CPU 0 に配送されるが、ISR 内の `ipsnd_dtq` が両 CPU の `next_tsk_flag` をセットするため、CPU 1 の次の APIC タイマー割り込みで Task 4 が起床する
- 行クリアは `fill_at()` syscall を使用。直接の VGA メモリ (0xB8000) アクセスは行わない
- MBF 1 への送信は psnd_mbf (非ブロッキング) を使用。バッファが満杯なら送信は静かに失敗する
- `line_buf[64]` に最大 63 文字を蓄積する (NUL 終端分を除く)

---

### check_seg

```c
void check_seg(void);
```

**概要:** セグメントレジスタと ESP/EFLAGS のデバッグ表示。

**引数:** なし

**戻り値:** なし (void)

**処理内容:**
1. `get_cs()`, `get_ds()`, `get_ss()`, `get_esp()`, `get_eflags()` でレジスタ値を取得
2. `printf()` で 16 進数フォーマットとして出力

**セクション属性:** `__attribute__((section(".user_text")))`

**呼び出し元:** デバッグ目的で任意の箇所から呼び出し可能

**注意点:**
- Ring 3 で実行される場合、CS=0x5B, DS=0x63, SS=0x6B が期待値
- Ring 0 で実行される場合、CS=0x20, DS=0x28, SS=0x30 が期待値
- `.user_text` セクションに配置され、`printf()` (syscall 経由) を使用するため Ring 3 から安全に呼び出せる

## 補足

### 画面表示の配色と意味

| 色 | 属性値 | 使用箇所 | 意味 |
|----|--------|---------|------|
| 緑 (GREEN) | 0x0A | Task 1/3 カウンタ、shared_count (Task 3) | CPU 0 のタスク活動 |
| マゼンタ (MAGENTA) | 0x0D | Task 2 カウンタ、shared_count (Task 2) | CPU 1 のタスク活動 |
| 黄 (YELLOW) | 0x0E | LOCK 表示、キーボード入力 | セマフォ獲得成功、ユーザー入力 |
| 赤 (RED) | 0x0C | BUSY 表示 | セマフォ獲得失敗 (他 CPU がロック中) |
| シアン (CYAN) | 0x0B | [CPU0]/[CPU1] ラベル、Timer | CPU 識別 |
| 暗いグレー (DARK) | 0x08 | ---- 表示、初期値 | 非アクティブ状態 |

### セマフォ競合のデモフロー

```
Task 3 (CPU 0)              Task 2 (CPU 1)
    |                            |
    +-- pol_sem(1) 成功 ------+  |
    |   "LOCK" 表示            |  +-- pol_sem(1) 失敗
    |   shared_count++ (緑)    |  |   "BUSY" 表示
    |   (SEM_HOLD 回繰り返し)  |  |
    +-- sig_sem(1) 解放 ------+  |
    |   "----" 表示            |  +-- pol_sem(1) 成功
    |   (SEM_REST 回休息)      |  |   "LOCK" 表示
    |                            |   shared_count++ (マゼンタ)
```

### GDB で観察可能な変数一覧

| 変数 | 型 | 説明 |
|------|-----|------|
| `shared_count` | unsigned long | セマフォ保護共有カウンタ |
| `task_count[1]` | unsigned long | Task 1 の実行回数 |
| `task_count[2]` | unsigned long | Task 2 の実行回数 |
| `task_count[3]` | unsigned long | Task 3 の実行回数 |
| `task_count[4]` | unsigned long | Task 4 の実行回数 |
| `tsk[5].tskstat` | STAT | Task 5 (idle, CPU 0) の状態 |
| `tsk[6].tskstat` | STAT | Task 6 (idle, CPU 1) の状態 |

### キーボード入力の流れ

```
キー押下
  ↓
IRQ1 → PIC → CPU 0 に配送
  ↓
key_intr (ISR): ASCII 変換 → ipsnd_dtq(0, 2, ch) で DTQ 2 に送信
  ↓
DTQ 2 で rcv_dtq 待ちの Task 4 が起床: next_tsk_flag[0]=next_tsk_flag[1]=1
  ↓
CPU 1 の APIC タイマー割り込み → sched_next_tsk_check
  ↓
sched_do_next_tsk: Task 4 (pri=1) が Task 2 (pri=15) をプリエンプション
  ↓
kbd_task: rcv_dtq(2) が文字を返す → line_buf に蓄積 + print_at でエコー
  ↓
Enter or 行末: psnd_mbf(1, line_buf, line_pos) で Task 1 に行送信
  ↓
rcv_dtq(2) で再びブロック → Task 2 に復帰
```
