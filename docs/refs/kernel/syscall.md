# syscall.c / syscallP.h

対象ファイル: `kernel/syscall.c`, `kernel/syscallP.h`

## 概要

システムコールディスパッチテーブルとディスパッチャの実装。ユーザータスクからのシステムコール (syscall) を受け取り、機能コード (sysid) に基づいて対応するカーネル内部関数にディスパッチする。ITRON 仕様の全システムコール (タスク管理、同期・通信、メモリ管理、タイマー、割り込み管理など) の 234 エントリと、TCP/IP 拡張用のスタブテーブルを持つ。非 ITRON 拡張 syscall のハンドラは `kernel/sys_exd.c` に実装され、extern プロトタイプが `syscallP.h` の先頭で宣言されている。

## 定数・マクロ

| 定数 | 定義元 | 値 | 説明 |
|------|--------|----|------|
| TFN_EXD_TCPIP | include/itron.h | -0xe2 | TCP/IP 拡張機能コード。この値のとき tcpip テーブルに分岐 |
| TFN_TCP_CRE_REP | syscallP.h | -0x201 | TCP/IP テーブルのオフセット計算基準 |

## 構造体・型

### syscall_entry

```c
struct syscall_entry {
    ER (*func)();
};
```

**説明:** システムコールディスパッチテーブルのエントリ。各エントリは対応するカーネル関数へのポインタを 1 つ保持する。関数ポインタの型は `ER (*)()` (引数の型チェックなし) で宣言されており、実際の関数は引数の数と型が異なるため、一部のエントリでキャストが行われている。

## グローバル変数

### syscall_entry[]

| 変数名 | 型 | 説明 |
|--------|-----|------|
| `syscall_entry[]` | `struct syscall_entry[234]` | メインディスパッチテーブル。インデックス 0x00〜0xe9 |

主要なマッピング:

| 範囲 | カテゴリ | 代表的なエントリ |
|------|---------|-----------------|
| 0x00-0x04 | 予約 | sys_dummy (未使用) |
| 0x05-0x20 | タスク管理 | sys_cre_tsk(0x05), sys_act_tsk(0x07), sys_slp_tsk(0x11), sys_wup_tsk(0x13), sys_dly_tsk(0x19) |
| 0x21-0x28 | セマフォ | sys_cre_sem(0x21), sys_sig_sem(0x23), sys_wai_sem(0x25), sys_pol_sem(0x26) |
| 0x29-0x30 | イベントフラグ | sys_cre_flg(0x29), sys_set_flg(0x2b), sys_wai_flg(0x2d) |
| 0x31-0x3c | データキュー | sys_cre_dtq(0x31), sys_snd_dtq(0x35), sys_psnd_dtq(0x36), sys_rcv_dtq(0x39), sys_prcv_dtq(0x3a) |
| 0x3d-0x44 | メールボックス | sys_cre_mbx(0x3d), sys_snd_mbx(0x3f), sys_rcv_mbx(0x41) |
| 0x45-0x4c | 固定長メモリプール | sys_cre_mpf(0x45), sys_get_mpf(0x49), sys_rel_mpf(0x47) |
| 0x4d-0x4e | システム時刻 | sys_set_tim(0x4d), sys_get_tim(0x4e) |
| 0x4f-0x53 | 周期ハンドラ | sys_cre_cyc(0x4f), sys_sta_cyc(0x51), sys_stp_cyc(0x52) |
| 0x55-0x61 | システム状態・RDQ | sys_rot_rdq(0x55), sys_get_tid(0x56), sys_loc_cpu(0x59), sys_sns_ctx(0x5d) |
| 0x65-0x6a | 割り込み管理 | sys_def_inh(0x65), sys_cre_isr(0x66), sys_dis_int(0x69), sys_ena_int(0x6a) |
| 0x6d-0x70 | 拡張 SVC・例外・参照 | sys_def_svc(0x6d), sys_def_exc(0x6e), sys_ref_cfg(0x6f), sys_ref_ver(0x70) |
| 0x71-0x7d | 割り込み内呼出 (i-API) | sys_iact_tsk(0x71), iwup_tsk(0x72), ipsnd_dtq(0x77), sys_isig_tim(0x7d) |
| 0x81-0x88 | ミューテックス | sys_cre_mtx(0x81), sys_loc_mtx(0x85), sys_unl_mtx(0x83) |
| 0x89-0x94 | メッセージバッファ | sys_cre_mbf(0x89), sys_snd_mbf(0x8d), sys_rcv_mbf(0x91) |
| 0x95-0x9f | ランデブ | sys_cre_por(0x95), sys_cal_por(0x97), sys_acp_por(0x99), sys_rpl_rdv(0x9d) |
| 0xa1-0xa8 | 可変長メモリプール | sys_cre_mpl(0xa1), sys_get_mpl(0xa5), sys_rel_mpl(0xa3) |
| 0xa9-0xad | アラームハンドラ | sys_cre_alm(0xa9), sys_sta_alm(0xab), sys_stp_alm(0xac) |
| 0xb1-0xb4 | オーバーランハンドラ | sys_def_ovr(0xb1), sys_sta_ovr(0xb2), sys_stp_ovr(0xb3) |
| 0xc1-0xcd | 自動 ID 割当生成 (acre) | sys_acre_tsk(0xc1), sys_acre_sem(0xc2), sys_acre_flg(0xc3), ... |
| 0xe1 | printf (実装依存) | sys_printf |
| 0xe2 | TCP/IP 拡張分岐 | sys_dummy |
| 0xe3-0xe9 | 非 ITRON 拡張 | sys_vga_write_at(0xe3), sys_vga_write_dec_at(0xe4), sys_vga_clear(0xe5), sys_vga_fill_at(0xe6), sys_key_getc_sc(0xe7), sys_key_set_task(0xe8), sys_stack_alloc_sc(0xe9) |

### syscall_tcpip_entry[]

| 変数名 | 型 | 説明 |
|--------|-----|------|
| `syscall_tcpip_entry[]` | `struct syscall_entry[39]` | TCP/IP 拡張用ディスパッチテーブル (全エントリ sys_dummy) |

| 範囲 | カテゴリ | 説明 |
|------|---------|------|
| 0x201-0x213 | TCP | tcp_cre_rep, tcp_del_rep, tcp_cre_cep, ..., tcp_get_opt (全てスタブ) |
| 0x221-0x227 | UDP | udp_cre_cep, udp_del_cep, ..., udp_get_opt (全てスタブ) |

## 関数リファレンス

### itron_syscall

```c
W itron_syscall(unsigned long apic, unsigned long sysid, unsigned long arg1,
    unsigned long arg2, unsigned long arg3, unsigned long arg4,
    unsigned long arg5, unsigned long arg6);
```

**概要:** メインのシステムコールディスパッチャ。割り込みハンドラ (c_intr_syscall) から呼び出され、機能コードに基づいて適切なカーネル関数を実行する。

**引数:**

| 引数 | 型 | 説明 |
|------|----|------|
| `apic` | `unsigned long` | 呼び出し元 CPU の APIC ID (0 または 1) |
| `sysid` | `unsigned long` | システムコール機能コード (正の値に変換済み)。itron.h の TFN_* 定数の絶対値に対応 |
| `arg1`〜`arg6` | `unsigned long` | システムコールの引数 (最大 6 個。呼び出されるハンドラの仕様に依存) |

**戻り値:** `W` (INT) -- 呼び出されたカーネル関数の戻り値 (通常は ER 型エラーコード)

**処理内容:**

1. `sysid == -TFN_EXD_TCPIP` (0xe2) の場合:
   - TCP/IP 拡張として処理。arg1 から TFN_TCP_CRE_REP のオフセットを計算し、`syscall_tcpip_entry[]` のインデックスとして使用
   - 引数を 1 つシフトして (arg2〜arg6) ハンドラを呼び出す
2. それ以外の場合:
   - `sysid` を直接 `syscall_entry[]` のインデックスとして使用
   - apic と arg1〜arg6 をそのままハンドラに渡す

**呼び出し元:** `c_intr_syscall()` (i386/interrupt.c) -- syscall 割り込みハンドラの C 部分

**注意点:**
- sysid は itron.h の TFN_* 定数の符号を反転した正の値。例: TFN_CRE_TSK = -0x05 → sysid = 0x05
- テーブル範囲外のインデックスに対する境界チェックは行われない
- 戻り値は c_intr_syscall により `proc->stack - 20` (save フレームの EAX スロット) に書き込まれ、ユーザー空間に返される

---

### sys_dummy

```c
ER sys_dummy(void);
```

**概要:** 未実装システムコールのスタブ関数。

**引数:** なし (可変長引数で呼ばれるが無視)

**戻り値:** `E_OK` (0) -- 常に成功

**処理内容:** 何もせず E_OK を返す。

**呼び出し元:** syscall_entry[] および syscall_tcpip_entry[] の未実装エントリから呼び出される。

**注意点:**
- 未実装のシステムコールが呼ばれてもエラーにはならず、E_OK が返る。デバッグ時に意図しない成功として問題を隠蔽する可能性がある。

## 補足

### ディスパッチの流れ

```
ユーザータスク (Ring 3)
  ↓ INT 0x99 (VECT_SYSCALL)
intr.s: syscall ハンドラ (Ring 0)
  ↓ save コンテキスト
c_intr_syscall()
  ↓ sysid 取得、引数展開
itron_syscall(apic, sysid, args...)
  ↓ テーブルルックアップ
sys_xxx_yyy(apic, args...)
  ↓ 戻り値
c_intr_syscall: proc->stack - 20 に書き込み
  ↓ restore コンテキスト
ユーザータスク (Ring 3) に復帰
```

### 割り込み内呼出 (i-API)

0x71〜0x7d のエントリは割り込みハンドラ内から呼び出される API (i-API) に対応する。通常のシステムコールとは異なり、割り込みコンテキストから安全に呼べるよう設計されている。例: `ipsnd_dtq` (0x7a) はキーボード ISR から DTQ 経由で kbd_task に文字を送信するために使用される。

### TCP/IP 拡張

TCP/IP 機能は現在のビルドから除外されており、`syscall_tcpip_entry[]` の全エントリは `sys_dummy` スタブとなっている。将来の拡張用にテーブル構造のみが維持されている。
