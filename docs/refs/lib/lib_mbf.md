# lib_mbf.c

対象ファイル: `lib/lib_mbf.c`

## 概要

ミューテックス、メッセージバッファ、ランデブ、固定長メモリプール、可変長メモリプールに関する ITRON API のユーザライブラリラッパーを提供するファイル。すべての関数は `syscall()` を通じてカーネルのシステムコールハンドラを呼び出す。`syscall()` の第 1 引数には `itron.h` で定義された機能コード (TFN_xxx) の符号反転値を渡す。

## 関数リファレンス

### ミューテックス (8 関数)

#### cre_mtx

```c
ER cre_mtx(ID mtxid, T_CMTX* pk_cmtx);
```

**概要:** ミューテックスを生成する。

**引数:**
- `mtxid` -- ミューテックス ID
- `pk_cmtx` -- ミューテックス生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_MTX, mtxid, pk_cmtx)` を呼び出す。

---

#### acre_mtx

```c
ER_ID acre_mtx(T_CMTX* pk_cmtx);
```

**概要:** ミューテックス ID を自動割り当てして生成する。

**引数:**
- `pk_cmtx` -- ミューテックス生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成されたミューテックス ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_MTX, pk_cmtx)` を呼び出す。

---

#### del_mtx

```c
ER del_mtx(ID mtxid);
```

**概要:** ミューテックスを削除する。

**引数:**
- `mtxid` -- ミューテックス ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_MTX, mtxid)` を呼び出す。

---

#### loc_mtx

```c
ER loc_mtx(ID mtxid);
```

**概要:** ミューテックスをロックする (永久待ち)。

**引数:**
- `mtxid` -- ミューテックス ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_LOC_MTX, mtxid)` を呼び出す。

---

#### ploc_mtx

```c
ER ploc_mtx(ID mtxid);
```

**概要:** ミューテックスをロックする (ポーリング)。

**引数:**
- `mtxid` -- ミューテックス ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_PLOC_MTX, mtxid)` を呼び出す。

---

#### tloc_mtx

```c
ER tloc_mtx(ID mtxid, TMO tmout);
```

**概要:** ミューテックスをタイムアウト付きでロックする。

**引数:**
- `mtxid` -- ミューテックス ID
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_TLOC_MTX, mtxid, tmout)` を呼び出す。

---

#### unl_mtx

```c
ER unl_mtx(ID mtxid);
```

**概要:** ミューテックスをアンロックする。

**引数:**
- `mtxid` -- ミューテックス ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_UNL_MTX, mtxid)` を呼び出す。

---

#### ref_mtx

```c
ER ref_mtx(ID mtxid, T_RMTX* pk_rmtx);
```

**概要:** ミューテックスの状態を参照する。

**引数:**
- `mtxid` -- ミューテックス ID
- `pk_rmtx` -- ミューテックス状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_MTX, mtxid, pk_rmtx)` を呼び出す。

---

### メッセージバッファ (10 関数)

#### cre_mbf

```c
ER cre_mbf(ID mbfid, T_CMBF* pk_cmbf);
```

**概要:** メッセージバッファを生成する。

**引数:**
- `mbfid` -- メッセージバッファ ID
- `pk_cmbf` -- メッセージバッファ生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_MBF, mbfid, pk_cmbf)` を呼び出す。

---

#### acre_mbf

```c
ER_ID acre_mbf(T_CMBF* pk_cmbf);
```

**概要:** メッセージバッファ ID を自動割り当てして生成する。

**引数:**
- `pk_cmbf` -- メッセージバッファ生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成されたメッセージバッファ ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_MBF, pk_cmbf)` を呼び出す。

---

#### del_mbf

```c
ER del_mbf(ID mbfid);
```

**概要:** メッセージバッファを削除する。

**引数:**
- `mbfid` -- メッセージバッファ ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_MBF, mbfid)` を呼び出す。

---

#### snd_mbf

```c
ER snd_mbf(ID mbfid, VP msg, UINT msgsz);
```

**概要:** メッセージバッファにメッセージを送信する (永久待ち)。

**引数:**
- `mbfid` -- メッセージバッファ ID
- `msg` -- 送信メッセージの先頭アドレス
- `msgsz` -- 送信メッセージのサイズ (バイト)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_SND_MBF, mbfid, msg, msgsz)` を呼び出す。

---

#### psnd_mbf

```c
ER psnd_mbf(ID mbfid, VP msg, UINT msgsz);
```

**概要:** メッセージバッファにメッセージを送信する (ポーリング)。

**引数:**
- `mbfid` -- メッセージバッファ ID
- `msg` -- 送信メッセージの先頭アドレス
- `msgsz` -- 送信メッセージのサイズ (バイト)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_PSND_MBF, mbfid, msg, msgsz)` を呼び出す。

---

#### tsnd_mbf

```c
ER tsnd_mbf(ID mbfid, VP msg, UINT msgsz, TMO tmout);
```

**概要:** メッセージバッファにタイムアウト付きでメッセージを送信する。

**引数:**
- `mbfid` -- メッセージバッファ ID
- `msg` -- 送信メッセージの先頭アドレス
- `msgsz` -- 送信メッセージのサイズ (バイト)
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_TSND_MBF, mbfid, msg, msgsz, tmout)` を呼び出す。

---

#### rcv_mbf

```c
ER_UINT rcv_mbf(ID mbfid, VP msg);
```

**概要:** メッセージバッファからメッセージを受信する (永久待ち)。

**引数:**
- `mbfid` -- メッセージバッファ ID
- `msg` -- 受信メッセージの格納先アドレス

**戻り値:** `ER_UINT` -- 受信メッセージのサイズ (正常時)、またはエラーコード (負値)

**処理内容:** `syscall(-TFN_RCV_MBF, mbfid, msg)` を呼び出す。

**特記事項:** 戻り値が `ER_UINT` であり、正常終了時は受信したメッセージのバイト数を返す。

---

#### prcv_mbf

```c
ER_UINT prcv_mbf(ID mbfid, VP msg);
```

**概要:** メッセージバッファからメッセージを受信する (ポーリング)。

**引数:**
- `mbfid` -- メッセージバッファ ID
- `msg` -- 受信メッセージの格納先アドレス

**戻り値:** `ER_UINT` -- 受信メッセージのサイズ、またはエラーコード

**処理内容:** `syscall(-TFN_PRCV_MBF, mbfid, msg)` を呼び出す。

---

#### trcv_mbf

```c
ER_UINT trcv_mbf(ID mbfid, VP msg, TMO tmout);
```

**概要:** メッセージバッファからタイムアウト付きでメッセージを受信する。

**引数:**
- `mbfid` -- メッセージバッファ ID
- `msg` -- 受信メッセージの格納先アドレス
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER_UINT` -- 受信メッセージのサイズ、またはエラーコード

**処理内容:** `syscall(-TFN_TRCV_MBF, mbfid, msg, tmout)` を呼び出す。

---

#### ref_mbf

```c
ER ref_mbf(ID mbfid, T_RMBF* pk_rmbf);
```

**概要:** メッセージバッファの状態を参照する。

**引数:**
- `mbfid` -- メッセージバッファ ID
- `pk_rmbf` -- メッセージバッファ状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_MBF, mbfid, pk_rmbf)` を呼び出す。

---

### ランデブ (12 関数)

#### cre_por

```c
ER cre_por(ID porid, T_CPOR* pk_cpor);
```

**概要:** ランデブポートを生成する。

**引数:**
- `porid` -- ポート ID
- `pk_cpor` -- ポート生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_POR, porid, pk_cpor)` を呼び出す。

---

#### acre_por

```c
ER_ID acre_por(T_CPOR* pk_cpor);
```

**概要:** ポート ID を自動割り当てしてランデブポートを生成する。

**引数:**
- `pk_cpor` -- ポート生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成されたポート ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_POR, pk_cpor)` を呼び出す。

---

#### del_por

```c
ER del_por(ID porid);
```

**概要:** ランデブポートを削除する。

**引数:**
- `porid` -- ポート ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_POR, porid)` を呼び出す。

---

#### cal_por

```c
ER_UINT cal_por(ID porid, RDVPTN calptn, VP msg, UINT cmsgsz);
```

**概要:** ランデブを呼び出す (永久待ち)。

**引数:**
- `porid` -- ポート ID
- `calptn` -- 呼び出しビットパターン
- `msg` -- メッセージの先頭アドレス (送信/受信兼用)
- `cmsgsz` -- 呼び出しメッセージのサイズ

**戻り値:** `ER_UINT` -- 返答メッセージのサイズ (正常時)、またはエラーコード

**処理内容:** `syscall(-TFN_CAL_POR, porid, calptn, msg, cmsgsz)` を呼び出す。

---

#### tcal_por

```c
ER_UINT tcal_por(ID porid, RDVPTN calptn, VP msg, UINT cmsgsz, TMO tmout);
```

**概要:** ランデブをタイムアウト付きで呼び出す。

**引数:**
- `porid` -- ポート ID
- `calptn` -- 呼び出しビットパターン
- `msg` -- メッセージの先頭アドレス
- `cmsgsz` -- 呼び出しメッセージのサイズ
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER_UINT` -- 返答メッセージのサイズ、またはエラーコード

**処理内容:** `syscall(-TFN_TCAL_POR, porid, calptn, msg, cmsgsz, tmout)` を呼び出す。

---

#### acp_por

```c
ER_UINT acp_por(ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg);
```

**概要:** ランデブを受け付ける (永久待ち)。

**引数:**
- `porid` -- ポート ID
- `acpptn` -- 受付ビットパターン
- `p_rdvno` -- ランデブ番号の格納先ポインタ
- `msg` -- 受信メッセージの格納先アドレス

**戻り値:** `ER_UINT` -- 呼び出しメッセージのサイズ、またはエラーコード

**処理内容:** `syscall(-TFN_ACP_POR, porid, acpptn, p_rdvno, msg)` を呼び出す。

---

#### pacp_por

```c
ER_UINT pacp_por(ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg);
```

**概要:** ランデブを受け付ける (ポーリング)。

**引数:**
- `porid` -- ポート ID
- `acpptn` -- 受付ビットパターン
- `p_rdvno` -- ランデブ番号の格納先ポインタ
- `msg` -- 受信メッセージの格納先アドレス

**戻り値:** `ER_UINT` -- 呼び出しメッセージのサイズ、またはエラーコード

**処理内容:** `syscall(-TFN_PACP_POR, porid, acpptn, p_rdvno, msg)` を呼び出す。

---

#### tacp_por

```c
ER_UINT tacp_por(ID porid, RDVPTN acpptn, RDVNO* p_rdvno, VP msg, TMO tmout);
```

**概要:** ランデブをタイムアウト付きで受け付ける。

**引数:**
- `porid` -- ポート ID
- `acpptn` -- 受付ビットパターン
- `p_rdvno` -- ランデブ番号の格納先ポインタ
- `msg` -- 受信メッセージの格納先アドレス
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER_UINT` -- 呼び出しメッセージのサイズ、またはエラーコード

**処理内容:** `syscall(-TFN_TACP_POR, porid, acpptn, p_rdvno, msg, tmout)` を呼び出す。

---

#### fwd_por

```c
ER fwd_por(ID porid, RDVPTN calptn, RDVNO rdvno, VP msg, UINT cmsgsz);
```

**概要:** ランデブを回送する。

**引数:**
- `porid` -- 転送先ポート ID
- `calptn` -- 呼び出しビットパターン
- `rdvno` -- ランデブ番号
- `msg` -- メッセージの先頭アドレス
- `cmsgsz` -- メッセージのサイズ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_FWD_POR, porid, calptn, rdvno, msg, cmsgsz)` を呼び出す。

---

#### rpl_rdv

```c
ER rpl_rdv(RDVNO rdvno, VP msg, UINT rmsgsz);
```

**概要:** ランデブに返答する。

**引数:**
- `rdvno` -- ランデブ番号
- `msg` -- 返答メッセージの先頭アドレス
- `rmsgsz` -- 返答メッセージのサイズ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_RPL_RDV, rdvno, msg, rmsgsz)` を呼び出す。

---

#### ref_por

```c
ER ref_por(ID porid, T_RPOR* pk_rpor);
```

**概要:** ランデブポートの状態を参照する。

**引数:**
- `porid` -- ポート ID
- `pk_rpor` -- ポート状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_POR, porid, pk_rpor)` を呼び出す。

---

#### ref_rdv

```c
ER ref_rdv(RDVNO rdvno, T_RRDV* pk_rrdv);
```

**概要:** ランデブの状態を参照する。

**引数:**
- `rdvno` -- ランデブ番号
- `pk_rrdv` -- ランデブ状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_RDV, rdvno, pk_rrdv)` を呼び出す。

---

### 固定長メモリプール (8 関数)

#### cre_mpf

```c
ER cre_mpf(ID mpfid, T_CMPF* pk_cmpf);
```

**概要:** 固定長メモリプールを生成する。

**引数:**
- `mpfid` -- メモリプール ID
- `pk_cmpf` -- メモリプール生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_MPF, mpfid, pk_cmpf)` を呼び出す。

---

#### acre_mpf

```c
ER_ID acre_mpf(T_CMPF* pk_cmpf);
```

**概要:** メモリプール ID を自動割り当てして固定長メモリプールを生成する。

**引数:**
- `pk_cmpf` -- メモリプール生成パケットへのポインタ

**戻り値:** `ER_ID` -- 生成されたメモリプール ID、またはエラーコード

**処理内容:** `syscall(-TFN_ACRE_MPF, pk_cmpf)` を呼び出す。

---

#### del_mpf

```c
ER del_mpf(ID mpfid);
```

**概要:** 固定長メモリプールを削除する。

**引数:**
- `mpfid` -- メモリプール ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_MPF, mpfid)` を呼び出す。

---

#### get_mpf

```c
ER get_mpf(ID mpfid, VP* p_blk);
```

**概要:** 固定長メモリブロックを獲得する (永久待ち)。

**引数:**
- `mpfid` -- メモリプール ID
- `p_blk` -- 獲得したメモリブロックの先頭アドレス格納先

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_GET_MPF, mpfid, p_blk)` を呼び出す。

---

#### pget_mpf

```c
ER pget_mpf(ID mpfid, VP* p_blk);
```

**概要:** 固定長メモリブロックを獲得する (ポーリング)。

**引数:**
- `mpfid` -- メモリプール ID
- `p_blk` -- 獲得したメモリブロックの先頭アドレス格納先

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_PGET_MPF, mpfid, p_blk)` を呼び出す。

---

#### tget_mpf

```c
ER tget_mpf(ID mpfid, VP* p_blk, TMO tmout);
```

**概要:** 固定長メモリブロックをタイムアウト付きで獲得する。

**引数:**
- `mpfid` -- メモリプール ID
- `p_blk` -- 獲得したメモリブロックの先頭アドレス格納先
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_TGET_MPF, mpfid, p_blk, tmout)` を呼び出す。

---

#### rel_mpf

```c
ER rel_mpf(ID mpfid, VP blk);
```

**概要:** 固定長メモリブロックを返却する。

**引数:**
- `mpfid` -- メモリプール ID
- `blk` -- 返却するメモリブロックの先頭アドレス

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REL_MPF, mpfid, blk)` を呼び出す。

---

#### ref_mpf

```c
ER ref_mpf(ID mpfid, T_RMPF* pk_rmpf);
```

**概要:** 固定長メモリプールの状態を参照する。

**引数:**
- `mpfid` -- メモリプール ID
- `pk_rmpf` -- メモリプール状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_MPF, mpfid, pk_rmpf)` を呼び出す。

---

### 可変長メモリプール (8 関数)

#### cre_mpl

```c
ER cre_mpl(ID mplid, T_CMPL* pk_cmpl);
```

**概要:** 可変長メモリプールを生成する。

**引数:**
- `mplid` -- メモリプール ID
- `pk_cmpl` -- メモリプール生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_CRE_MPL, mplid, pk_cmpl)` を呼び出す。

---

#### acre_mpl

```c
ER acre_mpl(T_CMPL* pk_cmpl);
```

**概要:** メモリプール ID を自動割り当てして可変長メモリプールを生成する。

**引数:**
- `pk_cmpl` -- メモリプール生成パケットへのポインタ

**戻り値:** `ER` -- エラーコード (ITRON 仕様では `ER_ID` だが、本実装では `ER` 型)

**処理内容:** `syscall(-TFN_ACRE_MPL, pk_cmpl)` を呼び出す。

**特記事項:** ITRON 仕様では `acre_mpl` の戻り値は `ER_ID` (自動割り当てされた ID を返す) であるが、本実装では `ER` 型として宣言されている。

---

#### del_mpl

```c
ER del_mpl(ID mplid);
```

**概要:** 可変長メモリプールを削除する。

**引数:**
- `mplid` -- メモリプール ID

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_DEL_MPL, mplid)` を呼び出す。

---

#### get_mpl

```c
ER get_mpl(ID mplid, UINT blksz, VP* p_blk);
```

**概要:** 可変長メモリブロックを獲得する (永久待ち)。

**引数:**
- `mplid` -- メモリプール ID
- `blksz` -- 要求するメモリブロックのサイズ (バイト)
- `p_blk` -- 獲得したメモリブロックの先頭アドレス格納先

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_GET_MPL, mplid, blksz, p_blk)` を呼び出す。

---

#### pget_mpl

```c
ER pget_mpl(ID mplid, UINT blksz, VP* p_blk);
```

**概要:** 可変長メモリブロックを獲得する (ポーリング)。

**引数:**
- `mplid` -- メモリプール ID
- `blksz` -- 要求するメモリブロックのサイズ (バイト)
- `p_blk` -- 獲得したメモリブロックの先頭アドレス格納先

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_PGET_MPL, mplid, blksz, p_blk)` を呼び出す。

---

#### tget_mpl

```c
ER tget_mpl(ID mplid, UINT blksz, VP* p_blk, TMO tmout);
```

**概要:** 可変長メモリブロックをタイムアウト付きで獲得する。

**引数:**
- `mplid` -- メモリプール ID
- `blksz` -- 要求するメモリブロックのサイズ (バイト)
- `p_blk` -- 獲得したメモリブロックの先頭アドレス格納先
- `tmout` -- タイムアウト値 (ミリ秒)

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_TGET_MPL, mplid, blksz, p_blk, tmout)` を呼び出す。

---

#### rel_mpl

```c
ER rel_mpl(ID mplid, VP blk);
```

**概要:** 可変長メモリブロックを返却する。

**引数:**
- `mplid` -- メモリプール ID
- `blk` -- 返却するメモリブロックの先頭アドレス

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REL_MPL, mplid, blk)` を呼び出す。

---

#### ref_mpl

```c
ER ref_mpl(ID mplid, T_RMPL* pk_rmpl);
```

**概要:** 可変長メモリプールの状態を参照する。

**引数:**
- `mplid` -- メモリプール ID
- `pk_rmpl` -- メモリプール状態パケットの格納先ポインタ

**戻り値:** `ER` -- エラーコード

**処理内容:** `syscall(-TFN_REF_MPL, mplid, pk_rmpl)` を呼び出す。

## 補足

- 全 46 関数。
- ミューテックス: 8 関数 (cre/acre/del/loc/ploc/tloc/unl/ref)。
- メッセージバッファ: 10 関数 (cre/acre/del/snd/psnd/tsnd/rcv/prcv/trcv/ref)。`rcv_mbf`, `prcv_mbf`, `trcv_mbf` は戻り値が `ER_UINT` であり、正常終了時は受信メッセージのバイト数を返す。
- ランデブ: 12 関数 (cre/acre/del/cal/tcal/acp/pacp/tacp/fwd/rpl/ref_por/ref_rdv)。`cal_por`, `acp_por`, `pacp_por`, `tacp_por` は戻り値が `ER_UINT`。
- 固定長メモリプール: 8 関数 (cre/acre/del/get/pget/tget/rel/ref)。
- 可変長メモリプール: 8 関数 (cre/acre/del/get/pget/tget/rel/ref)。`acre_mpl` の戻り値型が `ER` (仕様では `ER_ID`) である点に注意。
- 固定長と可変長のメモリプールの違い: 固定長 (`mpf`) はブロックサイズを獲得時に指定しない (生成時に決定)。可変長 (`mpl`) は獲得時に `blksz` でサイズを指定する。
