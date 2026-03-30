# lib_sem.c

対象ファイル: `lib/lib_sem.c`

## 概要

セマフォ、イベントフラグ、データキュー、メールボックスに関する ITRON API のユーザライブラリラッパーを提供するファイル。すべての関数は `syscall()` を通じてカーネルのシステムコールハンドラを呼び出す。`syscall()` の第 1 引数には `itron.h` で定義された機能コード (TFN_xxx) の符号反転値を渡す。

## 関数リファレンス

### セマフォ (9 関数)

#### cre_sem

```c
ER cre_sem(ID semid, T_CSEM* pk_csem);
```

**概要:** セマフォを生成する。

**引数:**
- `semid` -- セマフォ ID
- `pk_csem` -- セマフォ生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_SEM, semid, pk_csem)` を呼び出す。

---

#### acre_sem

```c
ER_ID acre_sem(T_CSEM* pk_csem);
```

**概要:** セマフォ ID を自動割り当てしてセマフォを生成する。

**引数:**
- `pk_csem` -- セマフォ生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成されたセマフォ ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_SEM, pk_csem)` を呼び出す。

---

#### del_sem

```c
ER del_sem(ID semid);
```

**概要:** セマフォを削除する。

**引数:**
- `semid` -- セマフォ ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_SEM, semid)` を呼び出す。

---

#### sig_sem

```c
ER sig_sem(ID semid);
```

**概要:** セマフォに資源を返却する (カウント +1)。

**引数:**
- `semid` -- セマフォ ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_SIG_SEM, semid)` を呼び出す。

---

#### isig_sem

```c
ER isig_sem(ID semid);
```

**概要:** 割り込みコンテキスト用のセマフォ資源返却。

**引数:**
- `semid` -- セマフォ ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_ISIG_SEM, semid)` を呼び出す。

---

#### wai_sem

```c
ER wai_sem(ID semid);
```

**概要:** セマフォの資源を獲得する (永久待ち)。

**引数:**
- `semid` -- セマフォ ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_WAI_SEM, semid)` を呼び出す。

---

#### pol_sem

```c
ER pol_sem(ID semid);
```

**概要:** セマフォの資源を獲得する (ポーリング、非ブロッキング)。

**引数:**
- `semid` -- セマフォ ID

**戻り値:** `ER` -- エラーコード (`E_TMOUT`: 資源なし)

**処理内容:** `syscall(-TFN_POL_SEM, semid)` を呼び出す。

---

#### twai_sem

```c
ER twai_sem(ID semid, TMO tmout);
```

**概要:** セマフォの資源をタイムアウト付きで獲得する。

**引数:**
- `semid` -- セマフォ ID
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_TWAI_SEM, semid, tmout)` を呼び出す。

---

#### ref_sem

```c
ER ref_sem(ID semid, T_RSEM* pk_rsem);
```

**概要:** セマフォの状態を参照する。

**引数:**
- `semid` -- セマフォ ID
- `pk_rsem` -- セマフォ状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_SEM, semid, pk_rsem)` を呼び出す。

---

### イベントフラグ (10 関数)

#### cre_flg

```c
ER cre_flg(ID flgid, T_CFLG* pk_cflg);
```

**概要:** イベントフラグを生成する。

**引数:**
- `flgid` -- イベントフラグ ID
- `pk_cflg` -- イベントフラグ生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_FLG, flgid, pk_cflg)` を呼び出す。

---

#### acre_flg

```c
ER_ID acre_flg(T_CFLG* pk_cflg);
```

**概要:** イベントフラグ ID を自動割り当てして生成する。

**引数:**
- `pk_cflg` -- イベントフラグ生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成されたイベントフラグ ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_FLG, pk_cflg)` を呼び出す。

---

#### del_flg

```c
ER del_flg(ID flgid);
```

**概要:** イベントフラグを削除する。

**引数:**
- `flgid` -- イベントフラグ ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_FLG, flgid)` を呼び出す。

---

#### set_flg

```c
ER set_flg(ID flgid, FLGPTN setptn);
```

**概要:** イベントフラグをセットする (ビット OR)。

**引数:**
- `flgid` -- イベントフラグ ID
- `setptn` -- セットするビットパターン

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_SET_FLG, flgid, setptn)` を呼び出す。

---

#### iset_flg (スタブ)

```c
ER iset_flg(ID flgid, FLGPTN setptn);
```

**概要:** 割り込みコンテキスト用のイベントフラグセット。

**引数:**
- `flgid` -- イベントフラグ ID
- `setptn` -- セットするビットパターン

**戻り値:** なし (不定値)

**処理内容:** 関数本体は空 (未実装)。

---

#### clr_flg

```c
ER clr_flg(ID flgid, FLGPTN clrptn);
```

**概要:** イベントフラグをクリアする (ビット AND)。

**引数:**
- `flgid` -- イベントフラグ ID
- `clrptn` -- クリアパターン (0 のビットがクリアされる)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CLR_FLG, flgid, clrptn)` を呼び出す。

---

#### wai_flg

```c
ER wai_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN* p_flgptn);
```

**概要:** イベントフラグの待ち (永久待ち)。

**引数:**
- `flgid` -- イベントフラグ ID
- `waiptn` -- 待ちビットパターン
- `wfmode` -- 待ちモード (AND/OR 等)
- `p_flgptn` -- 条件成立時のフラグパターン格納先

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_WAI_FLG, flgid, waiptn, wfmode, p_flgptn)` を呼び出す。

---

#### pol_flg

```c
ER pol_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN* p_flgptn);
```

**概要:** イベントフラグの待ち (ポーリング)。

**引数:**
- `flgid` -- イベントフラグ ID
- `waiptn` -- 待ちビットパターン
- `wfmode` -- 待ちモード
- `p_flgptn` -- 条件成立時のフラグパターン格納先

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_POL_FLG, flgid, waiptn, wfmode, p_flgptn)` を呼び出す。

---

#### twai_flg

```c
ER twai_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN* p_flgptn, TMO tmout);
```

**概要:** イベントフラグのタイムアウト付き待ち。

**引数:**
- `flgid` -- イベントフラグ ID
- `waiptn` -- 待ちビットパターン
- `wfmode` -- 待ちモード
- `p_flgptn` -- 条件成立時のフラグパターン格納先
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_TWAI_FLG, flgid, waiptn, wfmode, p_flgptn, tmout)` を呼び出す。

---

#### ref_flg

```c
ER ref_flg(ID flgid, T_RFLG* pk_rflg);
```

**概要:** イベントフラグの状態を参照する。

**引数:**
- `flgid` -- イベントフラグ ID
- `pk_rflg` -- フラグ状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_FLG, flgid, pk_rflg)` を呼び出す。

---

### データキュー (12 関数)

#### cre_dtq

```c
ER cre_dtq(ID dtqid, T_CDTQ* pk_cdtq);
```

**概要:** データキューを生成する。

**引数:**
- `dtqid` -- データキュー ID
- `pk_cdtq` -- データキュー生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_DTQ, dtqid, pk_cdtq)` を呼び出す。

---

#### acre_dtq

```c
ER_ID acre_dtq(T_CDTQ* pk_cdtq);
```

**概要:** データキュー ID を自動割り当てして生成する。

**引数:**
- `pk_cdtq` -- データキュー生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成されたデータキュー ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_DTQ, pk_cdtq)` を呼び出す。

---

#### del_dtq

```c
ER del_dtq(ID dtqid);
```

**概要:** データキューを削除する。

**引数:**
- `dtqid` -- データキュー ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_DTQ, dtqid)` を呼び出す。

---

#### snd_dtq

```c
ER snd_dtq(ID dtqid, VP_INT data);
```

**概要:** データキューにデータを送信する (永久待ち)。

**引数:**
- `dtqid` -- データキュー ID
- `data` -- 送信データ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_SND_DTQ, dtqid, data)` を呼び出す。

---

#### psnd_dtq

```c
ER psnd_dtq(ID dtqid, VP_INT data);
```

**概要:** データキューにデータを送信する (ポーリング)。

**引数:**
- `dtqid` -- データキュー ID
- `data` -- 送信データ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_PSND_DTQ, dtqid, data)` を呼び出す。

---

#### ipsnd_dtq (コメントアウト)

```c
#if 0
ER ipsnd_dtq(ID dtqid, VP_INT data);
#endif
```

**概要:** 割り込みコンテキスト用のデータキュー送信 (ポーリング)。

**処理内容:** `#if 0` で無効化されている。関数本体は空。

---

#### tsnd_dtq

```c
ER tsnd_dtq(ID dtqid, VP_INT data, TMO tmout);
```

**概要:** データキューにタイムアウト付きでデータを送信する。

**引数:**
- `dtqid` -- データキュー ID
- `data` -- 送信データ
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_TSND_DTQ, dtqid, data, tmout)` を呼び出す。

---

#### fsnd_dtq

```c
ER fsnd_dtq(ID dtqid, VP_INT data);
```

**概要:** データキューにデータを強制送信する。

**引数:**
- `dtqid` -- データキュー ID
- `data` -- 送信データ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_FSND_DTQ, dtqid, data)` を呼び出す。

---

#### ifsnd_dtq (スタブ)

```c
ER ifsnd_dtq(ID ddtqid, VP_INT data);
```

**概要:** 割り込みコンテキスト用のデータキュー強制送信。

**引数:**
- `ddtqid` -- データキュー ID (引数名に typo あり: `ddtqid`)
- `data` -- 送信データ

**戻り値:** なし (不定値)

**処理内容:** 関数本体は空 (未実装)。

---

#### rcv_dtq

```c
ER rcv_dtq(ID dtqid, VP_INT* p_data);
```

**概要:** データキューからデータを受信する (永久待ち)。

**引数:**
- `dtqid` -- データキュー ID
- `p_data` -- 受信データの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_RCV_DTQ, dtqid, p_data)` を呼び出す。

---

#### prcv_dtq

```c
ER prcv_dtq(ID dtqid, VP_INT* p_data);
```

**概要:** データキューからデータを受信する (ポーリング)。

**引数:**
- `dtqid` -- データキュー ID
- `p_data` -- 受信データの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_PRCV_DTQ, dtqid, p_data)` を呼び出す。

---

#### trcv_dtq

```c
ER trcv_dtq(ID dtqid, VP_INT* p_data, TMO tmout);
```

**概要:** データキューからタイムアウト付きでデータを受信する。

**引数:**
- `dtqid` -- データキュー ID
- `p_data` -- 受信データの格納先ポインタ
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_TRCV_DTQ, dtqid, p_data, tmout)` を呼び出す。

---

#### ref_dtq

```c
ER ref_dtq(ID dtqid, T_RDTQ* pk_rdtq);
```

**概要:** データキューの状態を参照する。

**引数:**
- `dtqid` -- データキュー ID
- `pk_rdtq` -- データキュー状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_DTQ, dtqid, pk_rdtq)` を呼び出す。

---

### メールボックス (7 関数)

#### cre_mbx

```c
ER cre_mbx(ID mbxid, T_CMBX* pk_cmbx);
```

**概要:** メールボックスを生成する。

**引数:**
- `mbxid` -- メールボックス ID
- `pk_cmbx` -- メールボックス生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_MBX, mbxid, pk_cmbx)` を呼び出す。

---

#### acre_mbx

```c
ER_ID acre_mbx(T_CMBX* pk_cmbx);
```

**概要:** メールボックス ID を自動割り当てして生成する。

**引数:**
- `pk_cmbx` -- メールボックス生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成されたメールボックス ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_MBX, pk_cmbx)` を呼び出す。

---

#### del_mbx

```c
ER del_mbx(ID mbxid);
```

**概要:** メールボックスを削除する。

**引数:**
- `mbxid` -- メールボックス ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_MBX, mbxid)` を呼び出す。

---

#### snd_mbx

```c
ER snd_mbx(ID mbxid, T_MSG* pk_msg);
```

**概要:** メールボックスにメッセージを送信する。

**引数:**
- `mbxid` -- メールボックス ID
- `pk_msg` -- 送信メッセージへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_SND_MBX, mbxid, pk_msg)` を呼び出す。

---

#### rcv_mbx

```c
ER rcv_mbx(ID mbxid, T_MSG** ppk_msg);
```

**概要:** メールボックスからメッセージを受信する (永久待ち)。

**引数:**
- `mbxid` -- メールボックス ID
- `ppk_msg` -- 受信メッセージポインタの格納先

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_RCV_MBX, mbxid, ppk_msg)` を呼び出す。

---

#### prcv_mbx

```c
ER prcv_mbx(ID mbxid, T_MSG** ppk_msg, TMO tmout);
```

**概要:** メールボックスからメッセージを受信する (ポーリング)。

**引数:**
- `mbxid` -- メールボックス ID
- `ppk_msg` -- 受信メッセージポインタの格納先
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_PRCV_MBX, mbxid, ppk_msg, tmout)` を呼び出す。

**注意:** ITRON 仕様では `prcv_mbx` はポーリング (タイムアウトなし) であるが、本実装ではタイムアウト引数を受け取る。シグネチャが仕様と異なる可能性がある。

---

#### ref_mbx

```c
ER ref_mbx(ID mbxid, T_RMBX* pk_rmbx);
```

**概要:** メールボックスの状態を参照する。

**引数:**
- `mbxid` -- メールボックス ID
- `pk_rmbx` -- メールボックス状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_MBX, mbxid, pk_rmbx)` を呼び出す。

## 補足

- セマフォ: 9 関数 (cre/acre/del/sig/isig/wai/pol/twai/ref)
- イベントフラグ: 10 関数 (cre/acre/del/set/iset/clr/wai/pol/twai/ref)。`iset_flg` は空のスタブ。
- データキュー: 12 関数 (cre/acre/del/snd/psnd/tsnd/fsnd/ipsnd/ifsnd/rcv/prcv/trcv/ref)。`ipsnd_dtq` は `#if 0` で無効、`ifsnd_dtq` は空のスタブ。
- メールボックス: 7 関数 (cre/acre/del/snd/rcv/prcv/ref)。
- `ifsnd_dtq` の引数名が `ddtqid` (d が重複) となっている。
