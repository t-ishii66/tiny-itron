# システムコール処理フローガイド

ユーザータスクが `slp_tsk()` 等のサービスコールを呼び出してから、
カーネル内でディスパッチされ、戻り値を受け取るまでの全過程を解説する。

SAVE_ALL/RESTORE_ALL と intr_enter/intr_leave の詳細は
[context-switch.md](context-switch.md) を参照。
本ドキュメントでは syscall 固有のパス（引数の渡し方、ディスパッチテーブル、
戻り値の書き込み方法）に焦点を当てる。

---

## 1. 概要: なぜ `int $0x99` が必要か

ユーザータスクは Ring 3 で動作し、カーネルは Ring 0 で動作する。
Ring 3 のコードは Ring 0 のメモリ領域への直接アクセスや特権命令
（`cli`, `outb` 等）の実行が禁止されている。

タスク管理やセマフォ操作などのカーネルサービスを呼び出すには、
CPU の特権レベルを Ring 3 → Ring 0 に遷移させる必要がある。
`int $0x99` (ソフトウェア割り込み、ベクタ 153) がこの遷移を実現する。

`int` 命令の実行時、CPU は自動的に:

1. TSS からRing 0 のスタック (SS0:ESP0) をロード
2. Ring 3 の SS, ESP, EFLAGS, CS, EIP を Ring 0 スタックに退避
3. IDT ベクタ 0x99 のハンドラ (`intr_syscall`) にジャンプ

IDT への登録は `interrupt.c:setup_syscall()` で行われる:

```c
/* i386/interrupt.c:148 */
set_idt(VECT_SYSCALL, (unsigned long)intr_syscall,
        SEL_K32_C, 0, GT_INTR | 0x60);
```

`0x60` は DPL=3 を設定する。これにより Ring 3 からの `int $0x99` が
許可される（DPL=0 のままだと #GP 例外が発生する）。

---

## 2. 全体フロー図

```
User Task (Ring 3)                    Kernel (Ring 0)
==================                    ================

cre_tsk(3, &ctsk)
  |
  v
lib/lib_tsk.c:cre_tsk()
  |  syscall(-TFN_CRE_TSK, 3, &ctsk)
  |  = syscall(0x05, 3, &ctsk)
  v
i386/klib.s:syscall          (.user_text, Ring 3)
  |  pushl %ebp
  |  movl  %esp, %ebp
  |  int   $0x99  --------> CPU: Ring 3→0 遷移
  |                           |  TSS.esp0 からカーネルスタックに切り替え
  |                           |  SS,ESP,EFLAGS,CS,EIP をカーネルスタックに退避
  |                           v
  |                     i386/intr.s:intr_syscall
  |                           |  SAVE_ALL      (レジスタをカーネルスタックに push,
  |                           |                 DS/ES をカーネルセグメントにリロード)
  |                           |  intr_enter    (k_nest++)
  |                           |  push %esp     (pt_regs* を引数に)
  |                           v
  |                     i386/syscall.c:c_intr_syscall(regs)
  |                           |  regs->esp からユーザスタックを読む
  |                           |  itron_syscall(apic, 0x05, 3, &ctsk, ...)
  |                           v
  |                     kernel/syscall.c:itron_syscall()
  |                           |  syscall_entry[0x05].func(apic, 3, &ctsk, ...)
  |                           v
  |                     kernel/sys_tsk.c:sys_cre_tsk(apic, 3, &ctsk)
  |                           |  タスク生成処理
  |                           |  return E_OK
  |                           v
  |                     c_intr_syscall:
  |                           |  regs->eax = E_OK   ← 戻り値書き込み
  |                           |  return
  |                           v
  |                     intr_syscall:
  |                           |  addl $4, %esp  (引数クリーンアップ)
  |                           |  jmp intr_return
  |                           |    intr_leave   (k_nest--, タスクスイッチ判定)
  |                           |    RESTORE_ALL  (レジスタ復帰, EAX=E_OK)
  |                           |    iret  -------> Ring 0→3 復帰
  |                           |
  v                           |
klib.s:syscall (続き)   <-----+
  |  popl  %ebp              EAX = E_OK (RESTORE_ALL が復元)
  |  ret
  v
lib_tsk.c:cre_tsk()
  |  return EAX   ← E_OK
  v
User Task に戻る
```

---

## 3. ユーザ空間ラッパー

### 3.1 ITRON 標準ラッパー (lib/lib_tsk.c, lib/lib_sem.c)

各サービスコールは、対応する関数コードで `syscall()` を呼ぶ薄いラッパーである。

```c
/* lib/lib_tsk.c:7 */
ER cre_tsk(ID tskid, T_CTSK* pk_ctsk)
{
    return syscall(-TFN_CRE_TSK, tskid, pk_ctsk);
}

/* lib/lib_tsk.c:106 */
ER slp_tsk(void)
{
    return syscall(-TFN_SLP_TSK);
}

/* lib/lib_sem.c:5 */
ER cre_sem(ID semid, T_CSEM* pk_csem)
{
    return syscall(-TFN_CRE_SEM, semid, pk_csem);
}
```

**呼び出し規約:**
`syscall(func_code, arg1, arg2, ..., arg6)` — 最大 7 引数の可変長 C 関数。

- 第 1 引数 `func_code`: `-TFN_xxx` を渡す（符号反転で正の値になる）
- 第 2 引数以降: サービスコール固有の引数

### 3.2 関数コードの符号反転

`TFN_CRE_TSK` は `-0x05` と定義されている（include/itron.h:134）。
ラッパーは `-TFN_CRE_TSK` = `0x05` を渡す。

```
TFN_CRE_TSK = -0x05  (itron.h での定義)
-TFN_CRE_TSK = 0x05  (syscall() に渡される値 = ディスパッチテーブルのインデックス)
```

なぜ負の値で定義するのか: ITRON v4.0 仕様では、関数コードを負の値で定義する
規約がある。ライブラリが符号反転して正のインデックスに変換し、
カーネルのディスパッチテーブル `syscall_entry[]` の配列添字として直接使う。

### 3.3 拡張 syscall ラッパー (lib/lib_exd.c)

ITRON 標準にない独自サービス（VGA 出力、キーボード入力など）も
同じ `syscall()` インターフェースを使う:

```c
/* lib/lib_exd.c:6 */
void print_at(int row, int col, char *s, unsigned char attr)
{
    syscall(-TFN_EXD_VGA_WRITE, row, col, s, attr);
}

/* lib/lib_exd.c:31 — key_read は廃止済み (DTQ に移行) */

/* lib/lib_exd.c:43 */
VP tsk_stack_alloc(int size)
{
    return (VP)syscall(-TFN_EXD_STACK_ALLOC, size);
}
```

---

## 4. アセンブリエントリポイント (i386/klib.s)

### 4.1 syscall 関数

```asm
/* i386/klib.s:525-532 */
.section .user_text
syscall:
    pushl   %ebp              # フレームポインタ保存
    movl    %esp, %ebp        # スタックフレーム設定
    int     $0x99             # ベクタ 153 → intr_syscall
    popl    %ebp              # フレームポインタ復帰
    ret                       # 呼び出し元に戻る (EAX = 戻り値)
```

**`.user_text` セクションに配置する理由:**
この関数は Ring 3 のユーザータスクから呼ばれる。ページテーブルで
PTE_USER ビットが設定されたページに配置しないと、Ring 3 からの
アクセスで #PF (Page Fault) が発生する。カーネルの `.text` セクションは
Supervisor 専用であり、`.user_text` セクションのみが User アクセス可能。

**スタックフレームを作る理由:**
`int $0x99` 実行時、CPU が退避する ESP はフレームポインタ設定後の値になる。
カーネル側の `intr_syscall` がこの ESP を使ってユーザースタック上の
引数を読み取る。`pushl %ebp` / `movl %esp, %ebp` により、
引数はオフセット `8(%ebp)` から始まる標準的な位置に配置される。

### 4.2 syscall2 (lcall — 未使用)

```asm
/* i386/klib.s:499-501 */
syscall2:
    lcall   $0x70, $0         # コールゲート経由の far call
    ret
```

GDT セレクタ 0x70 に設定されたコールゲートを使う代替手段。
現在は使用されておらず、`int $0x99` 方式が採用されている。

---

## 5. 割り込みハンドラ: intr_syscall (i386/intr.s)

### 5.1 全体構造

```asm
/* i386/intr.s */
intr_syscall:
    SAVE_ALL                        # (1) 全レジスタを push → pt_regs 構築
    call    intr_enter              # (2) k_nest++ (割り込みネストカウンタ)
    pushl   %esp                    # (3) pt_regs* を引数として渡す
    call    c_intr_syscall          # (4) C ディスパッチャ呼び出し
    addl    $4, %esp                # (5) 引数クリーンアップ
    jmp     intr_return             # (6) intr_leave + RESTORE_ALL + iret
```

`SAVE_ALL` 直後の ESP は pt_regs フレームの先頭を指す。
このポインタをそのまま `c_intr_syscall` に渡すことで、C 側で
pt_regs 構造体のフィールドとしてレジスタ値を読み書きできる。

### 5.2 SAVE_ALL 後のスタックレイアウト (pt_regs)

`SAVE_ALL` が完了すると、カーネルスタック上に以下の pt_regs フレームが
構成される:

```
ESP+0x00  ES         (SAVE_ALL が push)
ESP+0x04  DS         (SAVE_ALL が push)
ESP+0x08  EDI        (SAVE_ALL が push)
ESP+0x0C  ESI        (SAVE_ALL が push)
ESP+0x10  EBP        (SAVE_ALL が push)
ESP+0x14  EBX        (SAVE_ALL が push)
ESP+0x18  EDX        (SAVE_ALL が push)
ESP+0x1C  ECX        (SAVE_ALL が push)
ESP+0x20  EAX        (SAVE_ALL が push)     ← 戻り値の書き込み先
ESP+0x24  EIP        (CPU が push: int $0x99 の次の命令)
ESP+0x28  CS         (CPU が push: 0x5B = Ring 3)
ESP+0x2C  EFLAGS     (CPU が push)
ESP+0x30  ESP        (CPU が push: Ring 3 スタックポインタ)  ← ユーザスタック
ESP+0x34  SS         (CPU が push: 0x6B = Ring 3)
```

`c_intr_syscall` は `regs->esp` (オフセット 0x30) からユーザスタックの
ポインタを取得する。

### 5.3 ユーザスタックからの引数読み取り

`regs->esp` が指すユーザスタックの内容（`int $0x99` 発行時点）:

```
User ESP →
  ustack[0]  saved EBP    (syscall の pushl %ebp)
  ustack[1]  return addr  (call syscall の戻りアドレス)
  ustack[2]  func_code    (第 1 引数: 関数コード, 例: 0x05)
  ustack[3]  arg1         (第 2 引数: 例: tskid)
  ustack[4]  arg2         (第 3 引数: 例: pk_ctsk)
  ustack[5]  arg3         (第 4 引数)
  ustack[6]  arg4         (第 5 引数)
  ustack[7]  arg5         (第 6 引数)
  ustack[8]  arg6         (第 7 引数)
```

`c_intr_syscall` は `regs->esp` をユーザスタックポインタとして使い、
配列インデックスで引数を読み出す:

```c
ustack = (unsigned long *)regs->esp;
ret = itron_syscall(apic,
        ustack[2],  /* sysid   (func_code) */
        ustack[3],  /* arg1 */
        ustack[4],  /* arg2 */
        ustack[5],  /* arg3 */
        ustack[6],  /* arg4 */
        ustack[7],  /* arg5 */
        ustack[8]); /* arg6 */
```

---

## 6. C ディスパッチャ層

### 6.1 c_intr_syscall (i386/syscall.c)

```c
/* i386/syscall.c */
W
c_intr_syscall(struct pt_regs *regs)
{
    unsigned long *ustack;
    unsigned long apic;
    W   ret;

    /* Determine CPU from APIC ID */
    apic = *(volatile unsigned long *)APIC_ID;
    apic >>= 24;
    if (apic) apic = 1;

    /* Read syscall arguments from user stack */
    ustack = (unsigned long *)regs->esp;

    smp_lock(&kernel_lk);
    ret = itron_syscall(apic,
            ustack[2],     /* sysid   (user_esp + 8) */
            ustack[3],     /* arg1    (user_esp + 12) */
            ustack[4],     /* arg2    (user_esp + 16) */
            ustack[5],     /* arg3    (user_esp + 20) */
            ustack[6],     /* arg4    (user_esp + 24) */
            ustack[7],     /* arg5    (user_esp + 28) */
            ustack[8]);    /* arg6    (user_esp + 32) */
    smp_unlock(&kernel_lk);

    /* Write return value into the pt_regs EAX slot */
    regs->eax = ret;
    return ret;
}
```

**引数**: `intr_syscall` が `push %esp` で渡した pt_regs ポインタ。
pt_regs は SAVE_ALL がカーネルスタック上に構築したレジスタフレームである。

**`kernel_lk` による排他制御**:
syscall が操作するカーネルデータ構造 (タスク状態、スケジューラキュー、セマフォ、
DTQ、タイムアウトキュー等) は、タイマー割り込み (`c_intr_irq0`) やキーボード
割り込み (`c_intr_irq1`) からも操作される。SMP 環境では CPU 0 で syscall を
実行中に CPU 1 でタイマー割り込みが発火する可能性があるため、単一のスピンロック
`kernel_lk` (Big Kernel Lock) で全パスを排他する。
詳細は [system-overview.md セクション 10](system-overview.md#10-排他制御-big-kernel-lock) を参照。

**APIC ID の読み取り**: APIC ID レジスタ (0xFEE00020) のビット [31:24] を
読んで CPU 番号を判定する。1 より大きい場合も 0 or 1 に丸める安全策。

**戻り値の書き込み** — `regs->eax = ret`:

pt_regs 構造体の `eax` フィールド (オフセット 0x20) に直接書き込む。
`RESTORE_ALL` がこのスロットを `popl %eax` するので、`iret` 後に
ユーザータスクは `%eax` に戻り値を受け取る。

### 6.2 itron_syscall (kernel/syscall.c)

```c
/* kernel/syscall.c:29-44 */
W
itron_syscall(unsigned long apic, unsigned long sysid, unsigned long arg1,
    unsigned long arg2, unsigned long arg3, unsigned long arg4,
    unsigned long arg5, unsigned long arg6)
{
    ER  ret;
    if (sysid == -TFN_EXD_TCPIP) {
        /* TCP/IP サブディスパッチ (現在はスタブのみ) */
        sysid = arg1 - (-TFN_TCP_CRE_REP);
        ret = (*syscall_tcpip_entry[sysid].func)
                    (apic, arg2, arg3, arg4, arg5, arg6);
    } else {
        /* 通常ディスパッチ: sysid をインデックスとしてテーブル参照 */
        ret = (*syscall_entry[sysid].func)
                (apic, arg1, arg2, arg3, arg4, arg5, arg6);
    }
    return ret;
}
```

`sysid` がそのまま `syscall_entry[]` の配列インデックスになる。
例: `sysid = 0x05` → `syscall_entry[5].func` = `sys_cre_tsk`。

各ハンドラには第 1 引数として `apic` (CPU 番号) が渡され、
続いてユーザータスクが渡した引数が arg1, arg2, ... として続く。

---

## 7. ディスパッチテーブル (kernel/syscallP.h)

### 7.1 テーブル構造

```c
/* kernel/syscallP.h:23-25 */
struct syscall_entry {
    ER  (*func)();
};
```

関数ポインタ 1 つだけのシンプルな構造体。
`syscall_entry[]` は 0x00 〜 0xe9 (234 エントリ) の配列で、
関数コードの正値がそのままインデックスになる。

### 7.2 主要エントリ一覧

| インデックス | 関数 | サービスコール |
|-------------|------|---------------|
| 0x05 | sys_cre_tsk | cre_tsk — タスク生成 |
| 0x06 | sys_del_tsk | del_tsk — タスク削除 |
| 0x07 | sys_act_tsk | act_tsk — タスク起動 |
| 0x0a | sys_ext_tsk | ext_tsk — 自タスク終了 |
| 0x0b | sys_exd_tsk | exd_tsk — 自タスク終了と削除 |
| 0x0c | sys_ter_tsk | ter_tsk — 他タスク強制終了 |
| 0x0d | sys_chg_pri | chg_pri — 優先度変更 |
| 0x11 | sys_slp_tsk | slp_tsk — 起床待ち |
| 0x12 | sys_tslp_tsk | tslp_tsk — タイムアウト付き起床待ち |
| 0x13 | sys_wup_tsk | wup_tsk — タスク起床 |
| 0x19 | sys_dly_tsk | dly_tsk — 遅延 |
| 0x21 | sys_cre_sem | cre_sem — セマフォ生成 |
| 0x23 | sys_sig_sem | sig_sem — セマフォ返却 |
| 0x25 | sys_wai_sem | wai_sem — セマフォ獲得待ち |
| 0x26 | sys_pol_sem | pol_sem — セマフォポーリング |
| 0x29 | sys_cre_flg | cre_flg — イベントフラグ生成 |
| 0x2b | sys_set_flg | set_flg — フラグセット |
| 0x2d | sys_wai_flg | wai_flg — フラグ待ち |
| 0x55 | sys_rot_rdq | rot_rdq — レディキュー回転 |
| 0x56 | sys_get_tid | get_tid — タスク ID 取得 |
| 0x72 | iwup_tsk | iwup_tsk — 割り込みからの起床 |
| 0xc1 | sys_acre_tsk | acre_tsk — タスク自動 ID 生成 |
| 0xe1 | sys_printf | printf — カーネル printf |
| 0xe3 | sys_vga_write_at | print_at — VGA 文字列表示 |
| 0xe4 | sys_vga_write_dec_at | print_dec_at — VGA 数値表示 |
| 0xe5 | sys_vga_clear | clear_screen — VGA クリア |
| 0xe6 | sys_vga_fill_at | fill_at — VGA 矩形塗りつぶし |
| 0xe7 | sys_key_getc_sc | key_read — 廃止 (E_NOSPT を返す、DTQ に移行) |
| 0xe8 | sys_key_set_task | set_key_task — キーボード DTQ ID 登録 |
| 0xe9 | sys_stack_alloc_sc | tsk_stack_alloc — スタック動的確保 |

未実装のインデックス (0x00-0x04, 0x1a 等) は `sys_dummy` が登録されており、
`E_OK` を返すだけで何もしない。

---

## 8. 関数コード定義 (include/itron.h)

### 8.1 体系

`TFN_xxx` マクロは負の値で定義される。ライブラリラッパーが `-TFN_xxx` で
符号反転し、正のインデックスに変換する。

```c
/* include/itron.h:134-308 */
#define TFN_CRE_TSK    -0x05
#define TFN_SLP_TSK    -0x11
#define TFN_WUP_TSK    -0x13
#define TFN_CRE_SEM    -0x21
#define TFN_SIG_SEM    -0x23
/* ... */
#define TFN_EXD_VGA_WRITE   -0xe3
#define TFN_EXD_STACK_ALLOC -0xe9
```

### 8.2 カテゴリ別一覧

**タスク管理 (0x05-0x20):**

| 関数コード | TFN マクロ | サービスコール |
|-----------|-----------|---------------|
| 0x05 | TFN_CRE_TSK | cre_tsk |
| 0x06 | TFN_DEL_TSK | del_tsk |
| 0x07 | TFN_ACT_TSK | act_tsk |
| 0x08 | TFN_CAN_ACT | can_act |
| 0x09 | TFN_STA_TSK | sta_tsk |
| 0x0a | TFN_EXT_TSK | ext_tsk |
| 0x0b | TFN_EXD_TSK | exd_tsk |
| 0x0c | TFN_TER_TSK | ter_tsk |
| 0x0d | TFN_CHG_PRI | chg_pri |
| 0x0e | TFN_GET_PRI | get_pri |
| 0x0f | TFN_REF_TSK | ref_tsk |
| 0x10 | TFN_REF_TST | ref_tst |
| 0x11 | TFN_SLP_TSK | slp_tsk |
| 0x12 | TFN_TSLP_TSK | tslp_tsk |
| 0x13 | TFN_WUP_TSK | wup_tsk |
| 0x14 | TFN_CAN_WUP | can_wup |
| 0x15 | TFN_REL_WAI | rel_wai |
| 0x16 | TFN_SUS_TSK | sus_tsk |
| 0x17 | TFN_RSM_TSK | rsm_tsk |
| 0x18 | TFN_FRSM_TSK | frsm_tsk |
| 0x19 | TFN_DLY_TSK | dly_tsk |

**タスク例外処理 (0x1b-0x20):**

| 関数コード | TFN マクロ | サービスコール |
|-----------|-----------|---------------|
| 0x1b | TFN_DEF_TEX | def_tex |
| 0x1c | TFN_RAS_TEX | ras_tex |
| 0x1d | TFN_DIS_TEX | dis_tex |
| 0x1e | TFN_ENA_TEX | ena_tex |
| 0x1f | TFN_SNS_TEX | sns_tex |
| 0x20 | TFN_REF_TEX | ref_tex |

**同期・通信 (0x21-0x44):**

| 関数コード | TFN マクロ | サービスコール |
|-----------|-----------|---------------|
| 0x21 | TFN_CRE_SEM | cre_sem |
| 0x23 | TFN_SIG_SEM | sig_sem |
| 0x25 | TFN_WAI_SEM | wai_sem |
| 0x26 | TFN_POL_SEM | pol_sem |
| 0x29 | TFN_CRE_FLG | cre_flg |
| 0x2b | TFN_SET_FLG | set_flg |
| 0x2d | TFN_WAI_FLG | wai_flg |
| 0x31 | TFN_CRE_DTQ | cre_dtq |
| 0x35 | TFN_SND_DTQ | snd_dtq |
| 0x39 | TFN_RCV_DTQ | rcv_dtq |
| 0x3d | TFN_CRE_MBX | cre_mbx |
| 0x3f | TFN_SND_MBX | snd_mbx |
| 0x41 | TFN_RCV_MBX | rcv_mbx |

**拡張 syscall (0xe1-0xe9):**

| 関数コード | TFN マクロ | サービスコール | 用途 |
|-----------|-----------|---------------|------|
| 0xe1 | TFN_EXD_PRINT | printf | カーネル printk |
| 0xe3 | TFN_EXD_VGA_WRITE | print_at | VGA 文字列描画 |
| 0xe4 | TFN_EXD_VGA_DEC | print_dec_at | VGA 数値描画 |
| 0xe5 | TFN_EXD_VGA_CLEAR | clear_screen | 画面クリア |
| 0xe6 | TFN_EXD_VGA_FILL | fill_at | 矩形塗りつぶし |
| 0xe7 | TFN_EXD_KEY_GETC | key_read | 廃止 (DTQ に移行) |
| 0xe8 | TFN_EXD_KEY_SETTASK | set_key_task | キーボード DTQ ID 登録 |
| 0xe9 | TFN_EXD_STACK_ALLOC | tsk_stack_alloc | スタック動的確保 |

---

## 9. カーネルハンドラの実装パターン

全ハンドラの第 1 引数は `W apic` (CPU 番号 0 or 1) である。
これは `itron_syscall` が `syscall_entry[sysid].func(apic, arg1, ...)` と
呼び出すため。

### 9.1 sys_slp_tsk — 自タスクを待ち状態にする

```c
/* kernel/sys_tsk.c:293-309 */
ER sys_slp_tsk(W apic)
{
    ID  tskid = c_tskid[apic];    /* 現在実行中のタスク ID */

    /* caller holds kernel_lk (c_intr_syscall で取得済み) */
    if (tsk[tskid].wupcnt >= 1) {
        tsk[tskid].wupcnt--;      /* 起床要求があれば消費して即復帰 */
        return E_OK;
    }
    sched_rem(&tsk[tskid].plink); /* レディキューから削除 */
    tsk[tskid].tskstat = TTS_WAI; /* 状態を WAITING に変更 */
    sched_next_tsk(apic);         /* スケジューリングイベント通知 */
    return E_OK;
}
```

`sched_next_tsk(apic)` は両 CPU の `next_tsk_flag[]` を 1 にセットする。
実際のタスクスイッチは `intr_leave` 内の `sched_next_tsk_check()` で行われる
（セクション 11 参照）。

### 9.2 sys_wup_tsk — 指定タスクを起床させる

```c
/* kernel/sys_tsk.c:333-369 */
ER sys_wup_tsk(W apic, ID tskid)
{
    int flag = 1;
    if (tskid == TSK_SELF) {
        tskid = c_tskid[apic];
        flag = 0;
    }

    /* caller holds kernel_lk (c_intr_syscall で取得済み) */
    if (tsk[tskid].tskstat != TTS_WAI) {
        /* WAI でなければ wupcnt をインクリメント (キューイング) */
        /* ... エラーチェック省略 ... */
        tsk[tskid].wupcnt++;
        return E_OK;
    }
    tsk[tskid].tskstat = TTS_RDY;               /* READY に変更 */
    if (flag) {
        tsk[c_tskid[apic]].tskstat = TTS_RDY;   /* 自タスクも RDY に */
        sched_ins(tsk[tskid].tskpri, &tsk[tskid].plink); /* RDYキューに挿入 */
    }
    sched_timeout_rem_if_exist(&tsk[tskid].tlink); /* タイムアウト解除 */
    sched_next_tsk(apic);                        /* スケジューリング通知 */
    return E_OK;
}
```

### 9.3 sys_cre_tsk — タスク生成

```c
/* kernel/sys_tsk.c:32-71 */
ER sys_cre_tsk(W apic, ID tskid, T_CTSK* pk_ctsk)
{
    if (tskid < 1 || tskid > MAX_TSKID)
        return E_ID;
    if (tsk[tskid].tskstat != TTS_NON)
        return E_OBJ;

    tsk[tskid].tskid = tskid;
    tsk[tskid].proc = proc_create(tskid, pk_ctsk);   /* proc_t 割り当て */
    if (pk_ctsk->tskatr & TA_ACT) {
        tsk[tskid].tskstat = TTS_RDY;    /* TA_ACT なら即 READY */
    } else
        tsk[tskid].tskstat = TTS_DMT;    /* そうでなければ DORMANT */
    tsk[tskid].tskbpri = pk_ctsk->itskpri;
    tsk[tskid].tskpri = tsk[tskid].tskbpri;
    /* ... 残りのフィールド初期化 ... */
    return E_OK;
}
```

`proc_create` は `proc_t` を割り当て、初期 ESP/EIP を設定する。
`TA_ACT` 属性付きで生成すれば `act_tsk` なしで即座にスケジュール可能になる。

---

## 10. 戻り値の受け渡し

### 10.1 書き込み: regs->eax

`c_intr_syscall` がカーネルハンドラの戻り値を pt_regs フレームの EAX スロットに
直接書き込む:

```c
/* i386/syscall.c */
regs->eax = ret;
```

**pt_regs の EAX スロット**:

```
pt_regs フレーム (カーネルスタック上):
  オフセット 0x00: ES
  オフセット 0x04: DS
  オフセット 0x08: EDI
  オフセット 0x0C: ESI
  オフセット 0x10: EBP
  オフセット 0x14: EBX
  オフセット 0x18: EDX
  オフセット 0x1C: ECX
  オフセット 0x20: EAX     ← ここに戻り値を書く (PT_REGS_EAX_OFFSET)
  オフセット 0x24: EIP     (CPU が push)
  オフセット 0x28: CS
  オフセット 0x2C: EFLAGS
  オフセット 0x30: ESP     (Ring 3)
  オフセット 0x34: SS      (Ring 3)
```

`regs` は `intr_syscall` が `push %esp` で渡したポインタであり、
`SAVE_ALL` 直後の ESP の値 = pt_regs フレームの先頭アドレスである。

### 10.2 復元: RESTORE_ALL → iret

`RESTORE_ALL` が pt_regs フレームから 9 レジスタを逆順に pop する。
EAX スロット (オフセット 0x20) に書き込まれた戻り値は `popl %eax` で
`%eax` レジスタに復元される。

その後の `iret` で Ring 0 → Ring 3 に復帰し、ユーザータスクの
`syscall` 関数内の `popl %ebp; ret` が実行される。
C の呼び出し規約で `%eax` が戻り値レジスタなので、
ラッパー関数はこの値をそのまま呼び出し元に返す。

---

## 11. syscall とタスクスイッチ

### 11.1 スケジューリングイベントの通知

`slp_tsk` や `wup_tsk` 等のサービスコールはタスクの状態を変更した後、
`sched_next_tsk(apic)` を呼ぶ:

```c
/* kernel/sched.c:77-82 */
ID sched_next_tsk(W apic)
{
    next_tsk_flag[0] = 1;   /* CPU 0 にスケジューリング要求 */
    next_tsk_flag[1] = 1;   /* CPU 1 にもスケジューリング要求 */
    return E_OK;
}
```

両 CPU のフラグをセットするのは、起床されたタスクが別 CPU のアフィニティを
持つ可能性があるため。

### 11.2 intr_leave でのタスクスイッチ判定

`intr_leave` は割り込みネストカウンタが 0 に戻ったとき（最外の割り込みからの
復帰時）に、ESP の保存・スケジューラ呼び出し・ESP の復元を行う:

```
intr_leave (k_nest == 0 の場合):
  1. current_proc[cpu]->kern_esp = ESP   (現タスクの ESP を保存)
  2. sched_next_tsk_check(cpu)           (スケジューラ呼び出し)
  3. ESP = current_proc[cpu]->kern_esp   (新タスクの ESP をロード)
  4. tss_update_esp0(cpu, kern_stack_top) (TSS.esp0 を更新)
```

`sched_next_tsk_check` はスケジューラを呼び出し、`current_proc[cpu]` を
新タスクに切り替える可能性がある:

```c
/* i386/interrupt.c */
int sched_next_tsk_check(int apic)
{
    proc_t* old_proc;
    extern INT next_tsk_flag[];

    if (next_tsk_flag[apic] != 0) {
        old_proc = current_proc[apic];
        sched_do_next_tsk(apic);       /* 最高優先度タスクを選択 */
        next_tsk_flag[apic] = 0;
        if (old_proc != current_proc[apic]) {
            return 1;                   /* タスクスイッチ発生 */
        }
    }
    return 0;                           /* スイッチなし */
}
```

タスクスイッチが発生した場合、`intr_leave` のステップ 3 で ESP が
新タスクのカーネルスタックに切り替わる。新タスクのカーネルスタックには
そのタスクの pt_regs フレームが保存されているため、`RESTORE_ALL` は
新タスクのレジスタを pop し、`iret` は新タスクのコードにジャンプする。

### 11.3 slp_tsk/wup_tsk のフロー例

```
Task 1: slp_tsk()
  → syscall(0x11) → int $0x99
  → SAVE_ALL (Task 1 のレジスタをカーネルスタックに push)
  → intr_enter (k_nest++)
  → c_intr_syscall(regs) → sys_slp_tsk
    → Task 1 を TTS_WAI に、sched_next_tsk で next_tsk_flag を設定
  → regs->eax = E_OK (戻り値を pt_regs の EAX スロットに書き込み)
  → intr_leave
    → k_nest == 0 かつ next_tsk_flag[0] == 1
    → Task 1 の kern_esp に ESP を保存
    → sched_next_tsk_check(0) → sched_do_next_tsk(0)
      → Task 1 は WAI なので選ばれない、Task 3 (RDY) を選択
    → current_proc[0] = Task 3 の proc_t に変更
    → ESP = Task 3 の kern_esp (カーネルスタック切り替え)
    → TSS.esp0 を Task 3 の kern_stack_top に更新
  → RESTORE_ALL (Task 3 のカーネルスタックからレジスタ pop)
  → iret → Task 3 の実行再開

--- 後に Task 3 が wup_tsk(1) を呼ぶ ---

Task 3: wup_tsk(1)
  → syscall(0x13, 1) → int $0x99
  → SAVE_ALL (Task 3 のレジスタをカーネルスタックに push)
  → intr_enter (k_nest++)
  → c_intr_syscall(regs) → sys_wup_tsk
    → Task 1 を TTS_RDY に、sched_ins でレディキューに挿入
    → sched_next_tsk で next_tsk_flag を設定
  → regs->eax = E_OK (戻り値書き込み)
  → intr_leave
    → sched_next_tsk_check → sched_do_next_tsk
      → 優先度に基づいて Task 1 or Task 3 を選択
    → (Task 1 が選ばれた場合) ESP = Task 1 の kern_esp に切り替え
    → TSS.esp0 を Task 1 の kern_stack_top に更新
  → RESTORE_ALL (Task 1 のカーネルスタックからレジスタ pop)
  → iret → Task 1 は slp_tsk の戻り値 E_OK を %eax に持って再開
```

SAVE_ALL/RESTORE_ALL と intr_enter/intr_leave の詳細は
[context-switch.md](context-switch.md) を参照。

---

## ソースファイル索引

| ファイル | 内容 |
|---------|------|
| `i386/klib.s` | syscall アセンブリ関数 (.user_text) |
| `i386/intr.s` | intr_syscall, SAVE_ALL/RESTORE_ALL, intr_enter/intr_leave |
| `i386/syscall.c` | c_intr_syscall (pt_regs 経由の引数読み取り・戻り値書き込み) |
| `i386/proc.h` | proc_t 構造体, pt_regs 構造体 |
| `kernel/syscall.c` | itron_syscall ディスパッチャ |
| `kernel/syscallP.h` | syscall_entry[] テーブル |
| `include/itron.h:134-308` | TFN_* 関数コード定義 |
| `lib/lib_tsk.c` | タスク管理ラッパー |
| `lib/lib_sem.c` | セマフォ/フラグ/DTQ/MBX ラッパー |
| `lib/lib_exd.c` | 拡張 syscall ラッパー |
| `kernel/sys_tsk.c` | タスク管理ハンドラ |
| `kernel/sys_exd.c` | 拡張 syscall ハンドラ |
| `i386/interrupt.h:29` | VECT_SYSCALL (0x99) 定義 |
| `i386/interrupt.c` | IDT への syscall ベクタ登録, sched_next_tsk_check |
| `kernel/sched.c` | sched_next_tsk (フラグ設定) |
