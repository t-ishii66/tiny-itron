# PROBLEMS.md - 教育用コードとしての改善点

## 2026-03-27 Micro ITRON 4.0.0 仕様照合メモ（実装済み API のみ）

一次資料:
- µITRON4.0 Specification Ver. 4.00.00
  <https://libdrc.org/ITRON/SPEC/FILE/mitron-400e.pdf>

方針:
- `sys_dummy` や空関数など、明らかな未実装は除外
- 「実装済みに見えるサービス」が仕様どおりかを重点確認
- 今回は task / mailbox / mutex / time 関連を中心に確認

### 1. ~~`rel_wai` / `irel_wai` が解除後の待ちサービスコールを `E_RLWAI` にしていない~~ → 修正済み
- **修正**: `sys_rel_wai()` の TTS_WAI / TTS_WAS 両分岐に `proc_set_return_value(E_RLWAI)` と `wlink_rem()` を追加

### 2. `ref_tsk` / `ref_tst` が待ち理由・待ち対象・残り時間を返していない
- **ファイル**: `kernel/sys_tsk.c:255`
- **問題**: `sys_ref_tsk()` は `tskwait`, `wobjid`, `lefttmo` を常に 0 にしており、`sys_ref_tst()` も `tskwait` を固定 0 にしている。
- **仕様とのズレ**: µITRON4.0 の `ref_tsk` / `ref_tst` は、タスクの待ち種別や待ち対象オブジェクト、残りタイムアウトを参照するための API であり、これらを固定値にするのは契約不履行。
- **影響**: デバッグや状態監視から、タスクが何待ちかを判別できない。

### 3. ~~`trcv_mbx` / `rcv_mbx` が待ち受信時の返却先ポインタを保存していない~~ → 修正済み
- **修正**: `sys_trcv_mbx()` で待ちキュー挿入前に `tsk[c_tskid[apic]].ppk_msg = ppk_msg` を追加

### 4. ~~`TA_INHERIT` mutex の優先度継承条件が逆~~ → 修正済み
- **修正**: `sys_tloc_mtx()` の比較を `<` → `>` に変更 (数値大=低優先度のとき継承する)

### 5. ~~`get_tim` が進行する system time を返していない~~ → 修正済み
- **修正**: `sys_isig_tim()` に `system_time.l += TIC_NUME` (~17ms 加算、オーバーフロー時キャリー) を実装。`timer_intr()` から CPU 0 のティックごとに呼び出すよう変更。

## 2026-03-27 `docs/itron-guide.md` の µITRON4.0 仕様照合メモ

一次資料:
- µITRON4.0 Specification Ver. 4.00.00
  <https://libdrc.org/ITRON/SPEC/FILE/mitron-400e.pdf>

### 6. ~~「ITRON は静的設計が基本」という説明は広すぎる~~ → 修正済み
- **修正**: 「静的設計」→「静的設計が中心」に変更。`acre_xxx` の存在と、tiny-itron が固定上限を使う実装制約であることを明記。

### 7. ~~「全タスクが同じアドレス空間を共有し、メモリ保護なし」は µITRON 一般論としては言い過ぎ~~ → 修正済み
- **修正**: µITRON4.0 はメモリ保護方式を規定していないこと、tiny-itron の実装特性であることを明記。

### 8. ~~実行コンテキストを「タスク」と「割り込み」の二分にしているのは仕様より狭い~~ → 修正済み
- **修正**: 「割り込みコンテキスト」→「非タスクコンテキスト」に変更。割り込みハンドラ以外に周期/アラーム/CPU 例外ハンドラもあることを表に反映。tiny-itron では ISR のみが使われる旨を補足。

### 9. ~~`i` 付き API を「割り込みハンドラ専用」としている~~ → 修正済み
- **修正**: 「割り込みハンドラ専用」→「非タスクコンテキスト用」に変更。tiny-itron では ISR から呼ばれる旨を補足。
