# SMP コードレビュー: 提案と課題 (第 2 版)

全ソースコードを再精査した結果を、重要度順にまとめる。

前回 (第 1 版) で指摘した項目のうち、修正済みのものは末尾の付録に記載する。

---

## 1. 重大: バグ (正しく動かないコード)

### 1.1 sys_tcal_por の tskstat 代入先がタイポ

`kernel/sys_por.c:111`:
```c
tsk[c_tskid[apic]].tskid = TTS_WAI;   /* ← tskid に状態を代入している */
```

**問題:** `.tskid` (タスク ID) に `TTS_WAI` を書き込んでいる。
正しくは `.tskstat = TTS_WAI;`。これにより呼び出しタスクの ID が破壊され、
以降のスケジューラ操作でクラッシュまたは誤動作する。

### 1.2 sys_tcal_por の rdvno 比較が恒偽

`kernel/sys_por.c:116`:
```c
if (rdvno != rdvno) {         /* ← 常に FALSE */
    *(t->p_rdvno) = rdvno;
}
```

**問題:** 変数を自分自身と比較しているため、if ブロックは絶対に実行されない。
`sys_fwd_por` 経由で rdvno を渡す場合を区別する意図と思われるが、
条件が壊れているためランデブー番号が常に自動生成される。

**提案:** `if (rdvno != 0)` に修正する。

### 1.3 sys_cre_mtx に return 文がない

`kernel/sys_mtx.c:20-32`:
```c
ER sys_cre_mtx(W apic, ID mtxid, T_CMTX* pk_cmtx) {
    ...
    mtx[mtxid].act = 1;
    /* ← ここで関数が終わる。return E_OK; がない */
}
```

**問題:** ER を返す関数だが、正常終了時に return 文がない。
戻り値は不定となり、呼び出し側がエラーと誤判定する可能性がある。

### 1.4 sys_get_mpl が syscall() を呼んでいる

`kernel/sys_mpl.c:84-87`:
```c
ER sys_get_mpl(W apic, ID mplid, UINT blksz, VP* p_blk) {
    return syscall(apic, mplid, blksz, p_blk, TMO_FEVR);
}
```

**問題:** カーネル内部関数 (`sys_*`) から `syscall()` (ユーザー空間のラッパー) を
呼んでいる。正しくは `return sys_tget_mpl(apic, mplid, blksz, p_blk, TMO_FEVR);`。

### 1.5 klib.s の outw でスタックフレームが壊れている

`i386/klib.s:68-77`:
```asm
outw:
    pushl   %ebp        # [esp] = saved ebp
    pushl   %edx        # [esp] = saved edx
    movl    %esp, %ebp  # ebp → saved edx
    movl    8(%ebp), %edx   # ← これは return address を読んでいる!
    movl    12(%ebp), %eax  # ← これは第 1 引数
```

**問題:** 標準のプロローグ (push ebp → mov esp,ebp) の前に edx を push しているため、
`8(%ebp)` がリターンアドレスを指し、引数のオフセットがずれている。
現在 `outw` はアクティブコードから呼ばれていないため実害はないが、
将来使った場合に間違った I/O ポートに書き込む。

**提案:** push 順序を修正: `pushl %ebp; movl %esp,%ebp; pushl %edx` にする。

### 1.6 sys_twai_sem のロック内で sys_slp_tsk を呼んでいる

`kernel/sys_sem.c:180-183`:
```c
smp_lock(&sem_lk);
...
    sys_slp_tsk(apic);    /* ← sem_lk 保持中にスリープ */
...
smp_unlock(&sem_lk);
```

**問題:** `sem_lk` を保持したまま `sys_slp_tsk` でタスクがブロックされると、
他の CPU が `sig_sem` を呼んだ際に `sem_lk` で永久にスピンする (デッドロック)。
`sys_slp_tsk` 内でさらに `sched_lk` を取得するため、ロック順序の問題もある。

**提案:** `sys_slp_tsk` の呼び出し前に `smp_unlock(&sem_lk)` を行う。
`sys_twai_sem`、`sys_tloc_mtx`、`sys_tcal_por`、`sys_tacp_por`、
`sys_tsnd_dtq`、`sys_trcv_dtq`、`sys_trcv_mbx`、`sys_tsnd_mbf`、
`sys_trcv_mbf`、`sys_tget_mpf`、`sys_tget_mpl` に同様のパターンがある。

---

## 2. 中程度: SMP レースコンディション (ロック不足)

### 2.1 sys_rel_wai にスケジューラロックがない

`kernel/sys_tsk.c:409-411`:
```c
tsk[tskid].tskstat = TTS_RDY;
sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink);
```

**問題:** `tskstat` の変更と `sched_ins` を `sched_lk` なしで行っている。
`sched_do_next_tsk` (他 CPU) との競合で不整合が生じる。

### 2.2 sys_chg_pri にスケジューラロックがない

`kernel/sys_tsk.c:236-239`:
```c
sched_rem(&tsk[tskid].plink);
tsk[tskid].tskpri = tskpri;
sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink);
```

### 2.3 sys_rot_rdq にスケジューラロックがない

`kernel/sys_rdq.c:16-17`:
```c
sched_rem(&tsk[c_tskid[apic]].plink);
sched_ins(tsk[c_tskid[apic]].tskpri, &tsk[c_tskid[apic]].plink);
```

### 2.4 同期オブジェクト削除時の tskstat 変更にスケジューラロックがない

`sys_del_sem`、`sys_del_flg`、`sys_del_dtq`、`sys_del_mbx`、`sys_del_mtx`、
`sys_del_mbf`、`sys_del_mpf`、`sys_del_mpl`、`sys_del_por` の各関数で、
待ちタスクを TTS_RDY に戻して `sched_ins` するコードが `sched_lk` で保護されていない。

例: `kernel/sys_sem.c:74-76`:
```c
t->tskstat = TTS_RDY;
t->proc->reg[EAX] = E_DLT;
sched_ins(t->tskpri, &(t->plink));
```

**提案 (2.1-2.4 共通):** `sched_lk` で保護する。
これらは全て syscall コンテキスト (INT ゲート、ローカル割り込み禁止) で実行されるため、
ロック順序は `sched_lk` → `sched_lk_insrem` で統一できる。

### 2.5 sys_sig_sem / sys_unl_mtx / sys_rel_mpl での tskstat 変更

`sys_sig_sem:110`、`sys_unl_mtx:148-149`、`sys_rel_mpl:146-149` で、
待ちタスクを RDY に遷移させて `sched_ins` する操作がロックなし。
`sem_lk` はセマフォデータを保護するが、`sched_do_next_tsk` との競合には無力。

### 2.6 sys_tloc_mtx の優先度継承にスケジューラロックがない

`kernel/sys_mtx.c:107-112`:
```c
t->tskpri = tsk[c_tskid[apic]].tskpri;
sched_rem(&(t->plink));
sched_ins(t->tskpri, &(t->plink));
```

---

## 3. 軽微: コード品質と正確さ

### 3.1 sys_ref_mtx の未初期化変数使用

`kernel/sys_mtx.c:171-172`:
```c
if (mtx[mtxid].wlink.next != &(mtx[mtxid].wlink.next)) {   /* ← .next と比較 (wlink と比較すべき) */
    t = wlink2tsk(wlink);   /* ← wlink は未初期化 (ローカル変数が代入されていない) */
```

**問題:** `wlink` ローカル変数に `mtx[mtxid].wlink.next` を代入し忘れている。
また、比較対象が `&(mtx[mtxid].wlink.next)` (next フィールドのアドレス) になっており、
`&(mtx[mtxid].wlink)` (リストヘッドのアドレス) と比較すべき。

### 3.2 sys_rpl_rdv / sys_ref_rdv の条件式が不正確

`kernel/sys_por.c:229`:
```c
if (!(tsk[tskid].tskstat == TTS_WAI) && (tsk[tskid].rdvno == rdvno))
    return E_OBJ;
```

**問題:** この条件は「WAI でない AND rdvno が一致」のとき E_OBJ を返す。
しかし本来は「WAI でない OR rdvno が不一致」のとき E_OBJ を返すべき。
正しくは `if (tsk[tskid].tskstat != TTS_WAI || tsk[tskid].rdvno != rdvno)` か。

### 3.3 sys_tcal_por:120 の演算子優先度

```c
*(t->p_rdvno) = (por[porid].rdvno << 16) | c_tskid[apic] & 0xffff;
```

**問題:** `&` は `|` より優先度が高いため、実際は
`(por[porid].rdvno << 16) | (c_tskid[apic] & 0xffff)` と解釈される。
意図通りだが、括弧を明示すべき。同じパターンが `sys_tacp_por:184` にもある。

### 3.4 lib/lib_tsk.c の void 関数が値を返している

```c
void ext_tsk(void) {
    return syscall(-TFN_EXT_TSK);   /* void なのに return + 値 */
}
void exd_tsk(void) {
    return syscall(-TFN_EXD_TSK);   /* 同上 */
}
```

### 3.5 lib の空スタブ関数が ER 型なのに値を返さない

`lib/lib_int.c` の `irot_rdq`、`iget_tid`、`iunl_cpu` 等、
戻り値が ER 型だが空の関数本体で何も返さない。不定値が戻る。

**提案:** `return E_NOSPT;` (未サポート) を返す。

### 3.6 sys_sus_tsk のビジーウェイトループ

`kernel/sys_tsk.c:447`:
```c
for (k = 0 ; k < 100000 ; k ++) {
    if (tsk[tskid].tskstat != TTS_RUN)
        break;
}
```

他 CPU で RUN 中のタスクを SUS にするためにポーリングしている。
SMP では IPI でリスケジュールを要求するのが正しい方法だが、
現状では `sys_sus_tsk` を使わないのが安全。

### 3.7 htons/htonl マクロに括弧が不足

`include/types.h:273-277`:
```c
#define htons(x)  ((((x << 8) & 0xff00) | ...
```

`x` にマクロ引数の括弧 `(x)` がないため、`htons(a + b)` で展開が壊れる。
ネットワークスタックを unused に移動済みなので現在は問題ないが、
将来使う場合は修正が必要。

---

## 4. 設計上の検討事項

### 4.1 スケジューラのロック粒度

現在、スケジューラは 3 つのロックを使っている:
- `sched_lk` — `sched_hold_tsk`, `sched_do_next_tsk`, `sys_slp_tsk`, `sys_wup_tsk`, `iwup_tsk`
- `sched_lk_insrem` — `sched_ins`, `sched_rem`
- `sched_lk_timeout` — タイムアウトリスト操作

セクション 2 の問題を全て修正するには、`tskstat` を変更する全箇所に
`sched_lk` を追加する必要がある。

**提案:** 教育目的では `sched_lk` 1 つに統合するのが最もわかりやすい。
`sched_ins`/`sched_rem` の `sched_lk_insrem` と `sched_timeout_*` の
`sched_lk_timeout` を `sched_lk` に統合し、ロック設計を簡素化する。

### 4.2 AP 起動時の A20 再初期化

AP は `start.s` を再実行するため `a20_init` が 2 回呼ばれる。
実害はないが、キーボードコントローラへの書き込みが BSP と競合する可能性がゼロではない。

### 4.3 APIC タイマーの周波数調整

`smpP.h` の `MAX_TIMER_COUNT = 0x00800000` (~18Hz) は QEMU 向けの概算値。
キーボード入力の遅延が気になる場合はこの値を小さくする。

---

## 5. 修正の優先順位 (提案)

| 優先度 | 項目 | 工数 | 効果 |
|:---:|:---|:---:|:---|
| 1 | 1.1 sys_por.c タイポ修正 | 小 | ランデブーのクラッシュ防止 |
| 2 | 1.2 sys_por.c 恒偽条件修正 | 小 | ランデブー番号の正確さ |
| 3 | 1.3 sys_cre_mtx return 追加 | 小 | ミューテックス作成の正確さ |
| 4 | 1.4 sys_get_mpl 修正 | 小 | メモリプール取得の動作 |
| 5 | 1.6 ロック内スリープ修正 | 中 | デッドロック防止 |
| 6 | 2.1-2.6 sched_lk 追加 | 中 | SMP タスク切替の安全性 |
| 7 | 3.1 sys_ref_mtx 修正 | 小 | ミューテックス状態照会の正確さ |
| 8 | 3.2 sys_rpl_rdv 条件修正 | 小 | ランデブー返答の安全性 |
| 9 | 4.1 ロック設計統一 | 大 | 全体の安全性向上 |

---

## 付録: 修正済みの項目 (第 1 版からの差分)

| 旧番号 | 内容 | 修正内容 |
|:---:|:---|:---|
| 1.1 | iwup_tsk にロックなし | `sched_lk` で保護 |
| 1.2 | sys_wup_tsk にロックなし | `sched_lk` で保護。sys_slp_tsk, sys_tslp_tsk も同様 |
| 1.3 | pool.c にロックなし | `pool_stack_lk`, `pool_mem_lk` スピンロック追加 |
| 1.4 | sched_timeout のロック隙間 | リスト除去をロック内で実行。sched_timeout_rem_if_exist もロック保護追加 |
| 2.1 | アイドルタスクなし | Task 5 (CPU 0), Task 6 (CPU 1) を TMAX_TPRI で追加 |
| 2.2 | timer_ticks の非アトミック | CPU 0 のみでインクリメント |
| 2.3 | irq_enter/irq_exit が per-CPU でない | `get_apic_index()` で per-CPU 化 |
| 2.4 | timer_ticks 表示の競合 | 2.2 の修正により CPU 0 限定で解消 |
| 3.1 | smp_lock/cxchg の型 | `unsigned long *` に統一 |
| 3.2 | pause ヒントなし | `__asm__ volatile("pause")` 追加 |
| 3.3 | reg[256] が過大 | `reg[64]` に縮小。save/restore がスタックとして使うため最低 26 エントリ必要 (初期 13 + 1 フレーム 13)。reg[16] では 1 回の割り込みで溢れ #GP が発生した |
| 3.4 | per-CPU 配列が MAX_TSKID | `MAX_CPU=2` に統一 |
| 3.5 | デッドコード | nest, old_stack, old_eip, cpu_lock_apic, sched_tsk(), 未使用 APIC 定義を削除 |
