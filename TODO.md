# TODO.md — タイムアウト処理の修正計画

## 背景

タイムアウト付き待ちサービスコール (`trcv_dtq`, `twai_sem`, `twai_flg`, `trcv_mbx`,
`tloc_mtx` 等) のタイムアウト処理に複数の不具合がある。
現在のデモコード (`user.c`) では `rcv_dtq` (永久待ち) しか使っていないため顕在化しないが、
有限タイムアウトを使うと全て発現する。

影響ファイル:
- `kernel/sched.c` — sched_timeout, sched_timeout_rem, sched_timeout_rem_if_exist
- `kernel/sys_sem.c`, `sys_flg.c`, `sys_dtq.c`, `sys_mbx.c`, `sys_mtx.c`
- `kernel/sys_mbf.c`, `sys_por.c`, `sys_mpl.c`, `sys_mpf.c`

## 課題一覧

### A. sched_timeout が E_TMOUT を設定しない

**場所**: `kernel/sched.c:188-216`

**問題**: タイムアウト満了時に `proc_set_return_value(tsk_ptr->proc, E_TMOUT)` を
呼んでいない。syscall ハンドラが `regs->eax` に書いた値がそのまま返る。

**現状の各 syscall の return 値** (sys_tslp_tsk 呼び出し後):

| syscall | return 値 | タイムアウト時の結果 |
|---------|----------|-------------------|
| sys_twai_sem | E_TMOUT | 正しい (偶然) |
| sys_twai_flg | E_OK | **誤り** |
| sys_trcv_dtq | E_OK | **誤り** |
| sys_tsnd_dtq | E_OK | **誤り** |
| sys_trcv_mbx | E_OK | **誤り** |
| sys_tloc_mtx | E_OK | **誤り** |
| sys_tsnd_mbf | E_OK | **誤り** |
| sys_trcv_mbf | E_OK | **誤り** |
| sys_tcal_por | E_OK | **誤り** |
| sys_tacp_por | E_OK | **誤り** |

**修正方針の選択肢**:

1. **sched_timeout 側で E_TMOUT を設定する** (推奨)
   - sched_timeout 内でタイムアウト満了時に `proc_set_return_value(E_TMOUT)` を呼ぶ
   - 全 syscall が自動的に正しい戻り値を得る (1 箇所の修正で全 syscall をカバー)
   - sys_twai_sem の `return E_TMOUT` は冗長になるが害はない

2. **各 syscall 側で return E_TMOUT にする** (sys_twai_sem パターン)
   - 全 syscall の `return E_OK` → `return E_TMOUT` に変更
   - 正常起床パスで `proc_set_return_value(E_OK)` を呼ぶ必要がある (課題 E と関連)
   - 変更箇所が多い

### B. 正常起床時にタイムアウトキューからエントリが残る

**場所**: 各オブジェクトの signal/send 関数

**問題**: タイムアウト付きで待ちに入ったタスクが、タイムアウト前にデータ到着や
シグナルで起床された場合、tlink (タイムアウトキュー) のエントリが除去されない。

**現状**: `sched_timeout_rem_if_exist` を呼ぶのは以下の 2 箇所のみ:
- `sys_wup_tsk` (sys_tsk.c:353)
- `sys_rel_wai` (sys_tsk.c:376)

**呼んでいない箇所** (タイムアウト付き待ちタスクを起床するパス):
- `sys_sig_sem` → twai_sem の待ちタスク起床
- `sys_set_flg` → twai_flg の待ちタスク起床
- `ipsnd_dtq` / `sys_psnd_dtq` / `sys_tsnd_dtq` → trcv_dtq の待ちタスク起床
- `sys_snd_mbx` → trcv_mbx の待ちタスク起床
- `sys_unl_mtx` → tloc_mtx の待ちタスク起床
- `sys_snd_mbf` → trcv_mbf の待ちタスク起床
- `sys_rcv_mbf` → tsnd_mbf の待ちタスク起床
- `sys_cal_por` / `sys_acp_por` → ランデブの待ちタスク起床

**影響**: 孤児エントリがタイムアウトキューの先頭に残る。
`sched_timeout` は `tskstat == TTS_WAI` をチェックして TTS_WAI 以外なら何もしないが、
**エントリも除去しない**。結果:
1. 孤児エントリが先頭に居座り続ける
2. delta が毎ティック減算されて負の値になる
3. 後続の全エントリのタイムアウト処理がブロックされる

**修正方針の選択肢**:

1. **sched_timeout を堅牢にする** (推奨)
   - delta <= 0 のエントリは tskstat に関わらず必ず除去する
   - 孤児エントリがあっても安全に処理できる

2. **各起床パスで sched_timeout_rem_if_exist を呼ぶ**
   - 変更箇所が多い (10 箇所以上)
   - 抜け漏れリスクが高い

3. **両方** (防御的プログラミング)
   - sched_timeout を堅牢にし、かつ起床パスでも除去する

### C. sched_timeout が 1 ティックに 1 エントリしか処理しない

**場所**: `kernel/sched.c:188-216`

**問題**: 先頭エントリを処理したら `return` する。同時に満了した複数エントリ
(delta == 0 が連続) があっても 2 番目以降は次のティックまで処理されない。

**修正**: 先頭エントリが delta <= 0 の間ループする。

### D. sched_timeout_rem / sched_timeout_rem_if_exist がデルタを補正しない

**場所**: `kernel/sched.c:162-186`

**問題**: タイムアウトキューはデルタ符号化リスト (各エントリの delta は
前エントリからの相対値)。非先頭エントリを除去するとき、除去エントリの
delta を次エントリに加算しないと、後続エントリが早く発火する。

**例**:
```
[A: delta=300, B: delta=200, C: delta=100]
 → A は 300ms 後、B は 500ms 後、C は 600ms 後に発火

B を除去 → [A: delta=300, C: delta=100]
 → C は 400ms 後に発火してしまう (本来 600ms 後)

正しくは: C.delta += B.delta → [A: delta=300, C: delta=300]
 → C は 600ms 後に発火 (正しい)
```

**修正**: 除去時に `if (t->next != &timeout) t->next->delta += t->delta;` を追加。

### E. オブジェクト削除時にタイムアウトキューからエントリが残る

**場所**: `sys_del_sem`, `sys_del_dtq`, `sys_del_flg`, `sys_del_mbx`, `sys_del_mtx` 等

**問題**: 課題 B と同根。オブジェクト削除で待ちタスクに E_DLT を返すとき、
tlink を除去していない。

**修正**: 課題 B の方針 1 (sched_timeout の堅牢化) で自動的にカバーされる。
方針 3 を採る場合は各 del_xxx でも sched_timeout_rem_if_exist を呼ぶ。

## 修正の優先順と依存関係

```
D (デルタ補正)    ← 他の修正の前提。rem が壊れていると他の修正も壊れる
  ↓
B (孤児エントリ)  ← sched_timeout の堅牢化 + (任意で) 各起床パスの修正
  ↓
C (複数エントリ)  ← sched_timeout のループ化。B の修正と同時にやると自然
  ↓
A (E_TMOUT)      ← B/C が正しく動いてから戻り値を修正
  ↓
E (削除時)       ← B の方針次第
```

## 検証方法

修正後に有限タイムアウトを使うテストタスクを `user.c` に追加して動作確認:
- `trcv_dtq(dtqid, &data, 500)` でタイムアウト → E_TMOUT が返ること
- データ到着が先 → E_OK が返り、タイムアウトキューが汚れないこと
- 複数タスクが同時タイムアウト → 全て正しく起床すること
