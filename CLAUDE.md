# tiny-itron プロジェクト方針

## プロジェクトの価値基準

このプロジェクトの目的は「動かす」ことではなく **「理解する」** ことにある。
コードが動作するだけでは不十分で、**なぜその値なのか、どこから来たのか** を
読者が納得できることに価値がある。

コードを書く・修正するときは以下を常に意識すること:

- **数値の根拠**: アドレスや定数を使うとき、それが計算で導出された値なのか、
  決め打ちで問題ない値なのかを明確にする。「0x3400」は boot/table/start の
  .org サイズから計算される値であり、「0x110000」はカーネル末尾との十分な
  ギャップがあるから決め打ちで問題ない値である。この区別が重要
- **コメントで「なぜ」を書く**: what (何をしているか) だけでなく
  why (なぜその方法なのか、なぜその値なのか) を書く。
  ソースコードのコメントはすべて英語で統一する
- **アセンブリファイルには特に詳細なコメントを**:
  手書きアセンブリ (intr.s, klib.s, start.s, boot.s 等) は、各命令が何をしているか、
  レジスタに何が入っているか、スタックの状態がどうなっているかを
  行単位またはブロック単位でコメントする。
  コンパイラ生成の .s ファイル (genasm 等) は対象外
- **ドキュメントの整合性**: コードを変更したらドキュメントも更新する。
  特に build-system.md と memory-map.md は実装と密結合しており、
  stale な記述は誤解の元になる
- **過剰な抽象化より明快さ**: 汎用化やリファクタリングよりも、
  「この値はここから来ている」と追跡できる素朴さを優先する

## ドキュメント記述方針

- **対象読者**: OS・組み込みに興味のある学生・エンジニア。x86 や ITRON の前提知識なし
- **言語**: ドキュメント (docs/) は日本語。ソースコード中のコメントは英語
- **正確さの原則**:
  - コードを引用するなら、実際のソースと一致させる (関数名・構造体フィールド等)
  - サイズ・個数の記述はコード変更で大きく変わった場合に更新する
  - コマンド例は実際に実行可能であること
- **具体性の原則**:
  - 入出力ファイルを明示する (「バイナリに変換する」ではなく「`_kernel` (ELF) → `kernel` (フラットバイナリ)」)
  - 仕組みの説明では具体的な名前を使う (「スタックに保存」ではなく「SAVE_ALL マクロで per-task カーネルスタックに push」)
  - 主語を省略しない (「保存される」ではなく「CPU が SS/ESP を push する」)
- **図の活用**:
  - 混乱しやすい内容 (スタックレイアウト、メモリマップ、状態遷移等) はテキスト図で示す
  - 「言葉で説明するより図のほうが早い」と感じたら図にする
- **構成の原則**:
  - 1 ドキュメント 1 テーマ
  - 冒頭 1〜2 行で目的を述べる
  - 関連ドキュメントへのリンクを末尾に記載
- **参照可能性の原則**:
  - 各セクションはなるべく外部ドキュメントからリンクされることを前提に書く
  - リンク先のセクションだけ読んでも意味がわかる自己完結性を持たせる
  - 他ドキュメントと内容が重複する場合、権威ソース側を自己完結に保ち、
    参照する側は概要 + リンクに留める
  - 「前後を読まないとわからない」セクションは参照先として不適格
- **コードとの同期**:
  - アーキテクチャ変更時は必ず関連ドキュメントを更新する
  - 特に build-system.md, memory-map.md, context-switch.md はコードと密結合
  - docs/refs/ は参考資料のため、優先度は低い

## プロジェクト概要

Micro ITRON v4.0.0 仕様の教育用 RTOS カーネル (i386)。
SMP (2 CPU, Local APIC) 対応。QEMU `-smp 2` で動作する。

## ビルドと実行

```bash
# ビルド
make -C kernel && make -C lib && make -C i386

# QEMU で実行 (2 CPU)
./run.sh          # curses モード (デフォルト、ターミナル上に VGA テキスト表示)
./run.sh -g       # GTK ウィンドウモード (Ctrl+Alt+G でグラブ解除)
./run.sh -n       # nographic (シリアルのみ、VGA 出力は見えない)
./run.sh -d       # デバッグ (割り込みログ → qemu.log)
./run.sh -G       # GDB モード (ポート 1234 で待機)
```

### QEMU GTK モードの注意
GTK ウィンドウをクリックすると QEMU が入力をグラブ (キャプチャ) する。
全てのキーボード・マウス入力が VM に送られるため、デスクトップが操作不能に見える。
**Ctrl+Alt+G** でグラブを解除できる。

## アーキテクチャ上の注意点

### CPU レベル
- GCC はデフォルトで cmovs 等の Pentium Pro 以降の命令を生成する
- QEMU の `-cpu 486` では動かない。`-cpu pentium3` 以上を使う
- 将来 `-march=i486` を CFLAGS に追加すれば 486 対応可能だが、現時点では未対応

### 特権レベル
- Ring 0 (カーネル): CS=0x20, DS=0x28, SS=0x30
- Ring 3 (ユーザー): CS=0x5B, DS=0x63, SS=0x6B
- ユーザータスクから ccli()/csti() を呼ぶと #GP 例外が発生する

### タスクスイッチ
- 最初のタスクも含め、すべてのタスク遷移は RESTORE_ALL + iret で行う
- start_first_task/start_second_task は ltr で Task Register をロード後、
  proc.kern_esp → ret → intr_return_restore で Ring 3 に遷移
- 以降はソフトウェアコンテキストスイッチ (SAVE_ALL/RESTORE_ALL + intr_leave in intr.s)
- APIC ID を読んで CPU を判定し、per-CPU の current_proc[] を使う
- スケジューラはタイマーによるプリエンプションではなく、syscall 契機のみで切り替わる

### SMP アーキテクチャ
- IRQ ルーティング: PIC (i8259) を維持。外部 IRQ は CPU 0 のみに配送
- Local APIC: CPU 識別 (APIC ID)、APIC タイマー、EOI に使用
- I/O APIC: 使わない (複雑さ回避)
- システムティック: PIT (IRQ0, CPU 0 のみ) が唯一の sched_timeout 呼び出し元
- APIC タイマー (両 CPU): プリエンプティブタスクスイッチの契機のみ (EOI + intr_leave)
- スピンロック: `xchgl` ベース (Ring 3 安全、cli/sti 不要)

### スピンロック
- smp_lock/smp_unlock は xchgl ベースのスピンロック (Ring 3 で呼び出し可能)
- cpu_lock/cpu_unlock は ccli/csti (syscall ハンドラ内、Ring 0 でのみ使用)
- kernel_lk (Big Kernel Lock): 全カーネルデータ構造を保護する単一ロック。
  c_intr_syscall, c_intr_irq0, c_intr_irq1, c_intr_smp_timer1 で取得し、
  syscall/ISR 内部では「caller holds kernel_lk」を前提とする。
  sched_do_next_tsk のみ自身で取得/解放 (intr_leave 経由)
- video_lk: VGA 出力専用。ISR が kernel_lk 保持中に printk を呼ぶため別ロック

### AP (CPU 1) 起動シーケンス
1. BSP が smp_init() で Local APIC を有効化
2. INIT IPI → SIPI (vector=0x03 → 0x3000) を AP に送信
3. AP は start.s を再実行し Protected Mode に移行
4. run.s で cpu_num を確認して AP パスに分岐 (CPU1 スタック使用)
5. main() で smp_ap_init() を呼び、Local APIC 初期化 + start_second_task()

### PIC (i8259) 初期化
- ICW1-4 の後に OCW1 で全 IRQ をマスクすること
- QEMU では PIC 初期化後に全 IRQ がアンマスクされる (実機と異なる可能性)

### 割り込みと起動順序
- main() で csti() を呼んではいけない。初回タスク起動の RESTORE_ALL + iret が EFLAGS の IF=1 をロードする
- Ring 0 で割り込みを受けると、SAVE_ALL が Ring 3→0 前提の 14 ワード pt_regs フレームを想定するが、Ring 0→0 では SS/ESP が push されずフレームが 2 ワード不足する

## カーネル構成

### 主要定数 (include/config.h)
- TMAX_TPRI = 16 (優先度 1〜16、数値が小さいほど高優先度)
- MAX_TSKID = 16
- MAX_SEMID = 16, MAX_FLGID = 16
- タイマーティック: ~17ms (PIT, HZ=60), APIC タイマーは独立周期

### syscall 実装状況
- cre_tsk, act_tsk, slp_tsk, wup_tsk: 動作確認済み
- cre_sem, wai_sem, sig_sem: 実装あり (未テスト)
- cre_flg, set_flg, wai_flg: 実装あり (未テスト)
- dly_tsk: 動作確認済み (sched_timeout_ins でタイムアウトキューに挿入)
- tslp_tsk: 動作確認済み (タイムアウト付き待ちの基盤)
- trcv_dtq, twai_sem: 動作確認済み (タイムアウト付き DTQ 受信・セマフォ待ち)
- cre_mbf, psnd_mbf, trcv_mbf: 動作確認済み (メッセージバッファ送受信)

## ユーザータスク設計方針

### 目的
教育用デモ。2 CPU で複数タスクがマルチタスクで動作する様子を画面で確認でき、
GDB でタスク状態を調査できることを目標とする。

### 設計原則
- 簡素であること
- 画面出力は落ち着いた表示 (1〜2 秒に 1 行程度)
- GDB で観察しやすいグローバル変数を用意する
- CPU 0: Task 1 と Task 3 が wup_tsk/slp_tsk で交互に実行
- CPU 1: Task 2 が継続実行、Task 4 (キーボード) がプリエンプション
- Task 2 と Task 3 がバイナリセマフォ (pol_sem) で shared_count を競合アクセス
- 画面に各タスクの CPU を [CPU0]/[CPU1] で表示

### タスク構成
| タスク ID | 関数 | CPU | 優先度 | 役割 |
|-----------|------|-----|--------|------|
| 1 | first_task | 0 | 15 (低) | セマフォ・MBF・Task3 生成、カウントアップ |
| 2 | second_task | 1 | 15 (低) | Task4 生成、カウントアップ + セマフォ |
| 3 | usr_main | 0 | 15 (低) | カウントアップ + セマフォ |
| 4 | kbd_task | 1 | 1 (最高) | キーボード入力エコー |

### セマフォ (Task 2 と Task 3 の競合)
- バイナリセマフォ (sem 1, count=1) で shared_count を保護
- pol_sem (非ブロッキング) を使用。ブロッキング wai_sem は CPU 1 で唯一のタスクが
  ブロックする場合に問題があるため回避
- LOCK: セマフォ獲得成功 → shared_count++。BUSY: 他 CPU がロック中
- CPU 間の排他制御デモ

### キーボードの仕組み
- IRQ1 → c_intr_irq1 → key_intr (ISR で ASCII 変換 + ipsnd_dtq で DTQ 2 に送信)
- IRQ1 は CPU 0 に配送 (PIC ルーティング)。ipsnd_dtq が両 CPU の next_tsk_flag を
  セットするため、CPU 1 の次の APIC タイマー割り込みで Task 4 が起床
- kbd_task (Task 4) が rcv_dtq(2) でブロック待ち → DTQ に文字到着で起床 → VGA エコー
- kbd_task は文字をローカルバッファに蓄積し、Enter で psnd_mbf(1, buf, len) により
  MBF 1 経由で Task 1 に行文字列を送信。Task 1 は trcv_mbf(1, buf, 20) で受信
- key_dtq_id = 2: kbd_task が起動時に set_key_task(2) で設定 (ISR がどの DTQ に送るか知るため)

### CPU アフィニティ
- proc_t.cpu フィールドでタスクの実行 CPU を決定
- proc_init で Task 1 (cpu=0), Task 2 (cpu=1) を明示設定
- proc_create で新タスクは作成元 CPU のアフィニティを継承 (APIC ID 読み取り)
- sched_do_next_tsk が CPU アフィニティでフィルタリング

### GDB で確認できるグローバル変数
- `shared_count`: セマフォで保護された共有カウンタ
- `task_count[]`: タスクごとの実行回数
- `key_dtq_id`: キーボード用 DTQ ID (2)
- `cpu_num`: CPU 番号 (0=BSP, 1=AP)
- `cpu_second`: AP ハンドシェイク完了フラグ

## GDB デバッグ

docs/gdb-debugging.md を参照。

```bash
# ターミナル 1
./run.sh -G

# ターミナル 2
gdb i386/_kernel_dbg
(gdb) set architecture i386
(gdb) target remote :1234
(gdb) break first_task
(gdb) continue
(gdb) info threads        # 2 CPU が見える
```
