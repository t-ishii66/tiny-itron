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

---

## 2026-03-30 docs/refs ソースコード照合 — 不整合リスト

アーキテクチャ変更 (per-task kernel stack, SAVE_ALL/RESTORE_ALL, hardware TSS switch 廃止等) 後に
docs/refs/ の記述が更新されていなかった箇所の一覧。

### CRITICAL — 全面書き直しが必要

#### C1. refs/i386/proc.md → 修正済み
- proc_t が `{ stack, reg[64], flag, id, cpu }` のまま。実際は `{ kern_esp, kern_stack_top, saved_eflags, cpu }`
- レジスタオフセット定数 (ECX=0, EDX=1...) が残存。実際は struct pt_regs に置換済み
- FLAG_USE/FLAG_NON が残存。実際は削除済み
- save frame (13 slot, 52 bytes) の記述。実際は pt_regs (14 dwords, 56 bytes) on per-task kernel stack
- proc_init, proc_create, proc_set_tsk_arg, proc_delete, proc_eflags_save/restore すべての説明が旧アーキテクチャ
- proc_set_return_value(), struct pt_regs, PT_REGS_EAX_OFFSET, KERN_STACK_* が未記載

#### C2. refs/i386/intr.md → 修正済み
- save/restore ルーチンの記述。実際は SAVE_ALL/RESTORE_ALL マクロ + intr_enter/intr_leave
- save_cpu0/save_cpu1/save_common, restore_cpu0/restore_cpu1 が残存。実際は intr_enter, intr_leave_cpu0/cpu1
- ハンドラスタブパターン `call save; call handler; call restore; iret` → 実際は `SAVE_ALL; call intr_enter; call handler; jmp intr_return`
- intr_general, intr_syscall の記述が旧アーキテクチャ
- スタックフレーム図、タスクスイッチメカニズムが旧アーキテクチャ

#### C3. refs/i386/syscall.md → 修正済み
- `c_intr_syscall(apic, sysid, arg1..arg7)` → 実際は `c_intr_syscall(struct pt_regs *regs)`
- 戻り値 `proc->stack - 20` → 実際は `regs->eax`
- 引数をスタックから読む仕組み (user stack via regs->esp) が未記載
- kernel_lk 取得/解放が未記載
- reg[] レイアウト図が旧アーキテクチャ

#### C4. refs/i386/tss.md → 修正済み
- 4 TSS (tss0, tss1, tss_dummy0, tss_dummy1) + hardware task switch (ljmp) の記述
- 実際は 2 TSS のみ、dummy なし、ltr + kern_esp ロード + ret で起動
- set_tss() 関数が存在しない
- tss_update_esp0() が未記載
- SEL_TSS_DUMMY0/1 が存在しない

### HIGH — 複数セクションが不正確

#### H1. refs/i386/klib.md
- start_first_task/start_second_task が `ljmp` → 実際は `ltr + movl current_proc + ret`
- cltr の使われ方の説明が旧アーキテクチャ

#### H2. refs/i386/smp.md
- smp_init/smp_ap_init のタスク起動が hardware TSS switch → 実際は ltr + ret
- cltr(SEL_TSS_DUMMY1) の呼び出しが存在しない

#### H3. refs/i386/interrupt.md
- c_intr_general のシグネチャ `(W apic, W esp)` → 実際は `(void)`
- c_intr_irq0/irq1 に kernel_lk 取得/解放が未記載
- stack_adjust が proc->reg[] 参照 → 実際は pt_regs 参照
- irq_enter/irq_exit が IRQ1 で使用と誤記 (実際は未使用)

#### H4. refs/i386/main.md
- main() で cltr(SEL_TSS_DUMMY0) 呼び出しが存在しない
- TSS switch で EFLAGS IF=1 → 実際は RESTORE_ALL + iret

#### H5. refs/kernel/user.md
- ROW 定数がすべて古い (ROW_TASK3=6→7, ROW_SHARED=7→9, ROW_TASK2=8→11, ROW_KBD=9→13)
- SMP アーキテクチャ図、矢印アニメーション未記載
- COUNTER_WRAP, spin_chars 未記載
- dly_tsk がスタブと誤記 (実装済み)
- TinyItron → TinyITRON

#### H6. refs/kernel/sys_dtq.md
- acre_dtq/ref_dtq/ifsnd_dtq がスタブと誤記 (実装済み)
- 複数関数で ID チェックなしと誤記 (実装済み)
- sched_timeout_rem_if_exist 呼び出しが未記載
- proc->reg[EAX] → proc_set_return_value()

#### H7. refs/kernel/sys_sem.md
- acre_sem/isig_sem がスタブと誤記 (実装済み)
- sched_timeout_rem_if_exist 呼び出しが未記載
- sig_sem のフロー順序が実装と異なる
- proc->reg[EAX] → proc_set_return_value()

#### H8. refs/kernel/sys_mbf.md
- acre_mbf/psnd_mbf/prcv_mbf がスタブ/未実装と誤記 (実装済み)
- mbf_r/mbf_w が初期化されていないと誤記 (初期化済み)
- tsnd_mbf の処理フローが実装と異なる
- sched_timeout_rem_if_exist 呼び出しが未記載

#### H9. refs/kernel/pool.md
- pool_stack_lk/pool_mem_lk がソースに存在しない (kernel_lk に統一済み)
- stack_alloc/mem_alloc のスピンロック取得の記述が誤り

### MEDIUM — 個別の事実が不正確

#### M1. refs/i386/addr.md
- SEL_TSS_DUMMY0/1 が残存、KERN_STACK_SIZE/KERN_STACK_BASE が未記載
- GDT エントリ表に dummy TSS が残存

#### M2. refs/i386/386.md
- GDT レイアウト表に SEL_TSS_DUMMY0/1 が残存

#### M3. refs/i386/table.md
- GDT レイアウト表に Dummy TSS0/1 が残存
- genasm.c の TSS エントリ説明で 0x48/0x50 が残存

#### M4. refs/i386/timer.md
- timer_ticks の wrap (1000000000UL) が未記載
- sys_isig_tim(0) 呼び出しが未記載
- timer_start の呼び出し元が smp_ap_init → 実際は smp_init

#### M5. refs/i386/page.md
- MEM_START の定義元が pageP.h → 実際は addr.h
- save/restore → intr_enter/intr_leave

#### M6. refs/kernel/syscall.md
- sys_dummy が E_OK → 実際は E_NOSPT
- proc->stack - 20 の記述が旧アーキテクチャ

#### M7. refs/kernel/sys_mbx.md
- acre_mbx がスタブと誤記 (実装済み)
- del_mbx の E_DLT 通知が未実装と誤記 (実装済み)

#### M8. refs/kernel/sys_mpf.md
- acre_mpf/pget_mpf がスタブと誤記 (実装済み)

#### M9. refs/kernel/sys_mpl.md
- acre_mpl/pget_mpl がスタブと誤記 (実装済み)
- get_mpl の syscall() バグが未修正と誤記 (修正済み)

#### M10. refs/lib/lib_tsk.md
- dly_tsk がスタブと誤記 (実装済み)

#### M11. refs/lib/lib_int.md
- sns_ctx の戻り値説明で TRUE/FALSE の意味が逆

#### M12. refs/include/itron.md
- 拡張 TFN 7 個 (TFN_EXD_VGA_WRITE 〜 TFN_EXD_STACK_ALLOC, -0xe3〜-0xe9) が未記載

### LOW

#### L1. refs/lib/lib_sem.md
- DTQ 関数数「12 関数」→ 実際は 13 関数
