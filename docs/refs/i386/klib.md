# klib.s / klib.h

対象ファイル: `i386/klib.s`, `i386/klib.h`

## 概要

低レベルカーネルライブラリ。C 言語から呼び出し可能なアセンブリルーチン群。I/O ポートアクセス、割り込み制御、レジスタ読み取り、タスク開始、システムコール発行などの機能を提供する。全ての関数は cdecl 呼び出し規約に従う。

ソースファイル klib.s では、Ring 3 から呼び出し可能な関数群 (`get_cs`, `get_ds`, `get_ss`, `get_esp`, `get_eflags`, `syscall`) が `.section .user_text` ディレクティブにより `.user_text` セクションに配置される。これにより、ページングの U/S ビットで User アクセス可能なページにマッピングされ、Ring 3 のユーザータスクから直接呼び出せる。それ以外の関数 (I/O ポート操作、割り込み制御など) は通常の `.text` セクション (Supervisor ページ) に配置される。

## 関数リファレンス

### inb

```c
extern int inb(unsigned long port);
```

```asm
inb:
    pushl  %ebp
    movl   %esp, %ebp
    pushl  %edx
    movl   8(%ebp), %edx
    movl   $0, %eax
    inb    %dx, %al
    popl   %edx
    popl   %ebp
    ret
```

**概要:** 指定された I/O ポートから 1 バイトを読み取る。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `port` | `unsigned long` | 読み取り対象の I/O ポートアドレス (下位 16 ビットのみ使用) |

**戻り値:** 読み取ったバイト値 (EAX の下位 8 ビット、上位は 0)

**処理内容:** x86 の `in` 命令を使用して DX レジスタで指定されたポートから AL に 1 バイト読み取る。

**呼び出し元:** キーボードドライバ、PIC 制御、ビデオドライバなど

### inw

```c
extern int inw(unsigned long port);
```

```asm
inw:
    pushl  %ebp
    movl   %esp, %ebp
    pushl  %edx
    movl   8(%ebp), %edx
    movl   $0, %eax
    inw    %dx, %ax
    popl   %edx
    popl   %ebp
    ret
```

**概要:** 指定された I/O ポートから 2 バイト (ワード) を読み取る。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `port` | `unsigned long` | 読み取り対象の I/O ポートアドレス |

**戻り値:** 読み取ったワード値 (EAX の下位 16 ビット、上位は 0)

**処理内容:** x86 の `inw` 命令を使用して DX レジスタで指定されたポートから AX に 2 バイト読み取る。

**呼び出し元:** NE2000 ネットワークドライバなど

### outb

```c
extern void outb(unsigned long port, unsigned long value);
```

```asm
outb:
    pushl  %ebp
    movl   %esp, %ebp
    pushl  %edx
    movl   8(%ebp), %edx
    movl   12(%ebp), %eax
    andl   $0xffff, %edx
    outb   %al, %dx
    popl   %edx
    popl   %ebp
    ret
```

**概要:** 指定された I/O ポートに 1 バイトを書き込む。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `port` | `unsigned long` | 書き込み先の I/O ポートアドレス |
| `value` | `unsigned long` | 書き込む値 (下位 8 ビットのみ使用) |

**戻り値:** なし

**処理内容:** EDX の下位 16 ビットにポートアドレスを設定し、EAX の下位 8 ビットの値を `outb` 命令で書き込む。

**呼び出し元:** PIC 制御、タイマー設定、キーボードコントローラ、ビデオコントローラなど

**注意点:** ポートアドレスは `andl $0xffff, %edx` で 16 ビットにマスクされる。

### outw

```c
extern void outw(unsigned long port, unsigned long value);
```

```asm
outw:
    pushl  %ebp
    pushl  %edx
    movl   %esp, %ebp
    movl   8(%ebp), %edx
    movl   12(%ebp), %eax
    outw   %ax, %dx
    popl   %edx
    popl   %ebp
    ret
```

**概要:** 指定された I/O ポートに 2 バイト (ワード) を書き込む。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `port` | `unsigned long` | 書き込み先の I/O ポートアドレス |
| `value` | `unsigned long` | 書き込む値 (下位 16 ビットのみ使用) |

**戻り値:** なし

**処理内容:** `outw` 命令で AX の値を DX で指定されたポートに書き込む。

**呼び出し元:** NE2000 ネットワークドライバなど

**注意点:** スタックフレームの構築順序が `inb`/`outb` と異なる (`pushl %ebp` の後に `pushl %edx` を行い、その後 `movl %esp, %ebp` しているため、引数のオフセットが異なる可能性がある)。実際にはオフセット計算は正しく動作する。

### cxchg

```c
extern int cxchg(unsigned long *ptr, unsigned long value);
```

```asm
cxchg:
    pushl  %ebp
    movl   %esp, %ebp
    push   %ebx
    movl   8(%ebp), %ebx
    movl   12(%ebp), %eax
    xchgl  (%ebx), %eax
    popl   %ebx
    popl   %ebp
    ret
```

**概要:** アトミック交換命令。指定されたメモリアドレスの値と引数の値をアトミックに交換する。スピンロックの実装に使用される。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `ptr` | `unsigned long *` | 交換対象のメモリアドレス |
| `value` | `unsigned long` | 交換する新しい値 |

**戻り値:** 交換前のメモリの値 (EAX)

**処理内容:** x86 の `xchgl` 命令を使用する。`xchgl` は暗黙的に LOCK プレフィックスが付与されるため、マルチプロセッサ環境でもアトミック性が保証される。

**呼び出し元:** `smp_lock()`, `smp_unlock()` (スピンロック実装)

**注意点:** `xchgl` は Ring 3 (ユーザーモード) でも実行可能であるため、`cli`/`sti` に依存しないスピンロックが実現できる。

### ccli

```c
extern void ccli(void);
```

```asm
ccli:
    cli
    ret
```

**概要:** 割り込みを禁止する (EFLAGS の IF ビットをクリアする)。

**引数:** なし

**戻り値:** なし

**処理内容:** x86 の `cli` (Clear Interrupt Flag) 命令を実行する。

**呼び出し元:** カーネル内の割り込み禁止が必要な箇所 (syscall ハンドラ内、初期化処理など)

**注意点:** `cli` は特権命令であり、Ring 0 でのみ実行可能。Ring 3 (ユーザーモード) から呼び出すと一般保護例外 (#GP) が発生する。

### csti

```c
extern void csti(void);
```

```asm
csti:
    sti
    ret
```

**概要:** 割り込みを有効化する (EFLAGS の IF ビットをセットする)。

**引数:** なし

**戻り値:** なし

**処理内容:** x86 の `sti` (Set Interrupt Flag) 命令を実行する。

**呼び出し元:** カーネル内の割り込み有効化が必要な箇所

**注意点:** `sti` は特権命令であり、Ring 0 でのみ実行可能。Ring 3 (ユーザーモード) から呼び出すと一般保護例外 (#GP) が発生する。`main()` 内で直接 `csti()` を呼んではならない。割り込みの有効化は `start_first_task()` / `start_second_task()` が `RESTORE_ALL` + `iret` で偽 pt_regs フレームの EFLAGS (IF=1) をロードすることで行う。

### cltr

```c
extern void cltr(unsigned long selector);
```

```asm
cltr:
    push   %ebp
    movl   %esp, %ebp
    movl   8(%ebp), %eax
    ltr    %ax
    popl   %ebp
    ret
```

**概要:** タスクレジスタ (TR) にセレクタをロードする。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `selector` | `unsigned long` | ロードする TSS セグメントのセレクタ (下位 16 ビットのみ使用) |

**戻り値:** なし

**処理内容:** x86 の `ltr` (Load Task Register) 命令を使用してタスクレジスタにセレクタをロードする。

**呼び出し元:** カーネル初期化時 (TSS 設定後)

**注意点:** `ltr` は特権命令であり、Ring 0 でのみ実行可能。TSS セグメントは GDT に登録済みでなければならない。

### cwait

```c
extern void cwait(void);
```

```asm
cwait:
    wait
    ret
```

**概要:** FPU (浮動小数点ユニット) の処理完了を待機する。

**引数:** なし

**戻り値:** なし

**処理内容:** x86 の `wait` (FWAIT) 命令を実行する。FPU の保留中の例外がある場合は例外が発生する。

**呼び出し元:** (現在のカーネルでは未使用)

### get_cs

```c
extern short get_cs(void);
```

```asm
get_cs:
    xorl  %eax, %eax
    mov   %cs, %ax
    ret
```

**概要:** 現在の CS (コードセグメント) レジスタの値を返す。

**引数:** なし

**戻り値:** CS レジスタの値 (16 ビット)

**処理内容:** CS レジスタの値を EAX の下位 16 ビットにコピーして返す。

**呼び出し元:** デバッグ・診断用

**セクション:** `.user_text` -- 特権命令を含まないため、Ring 3 のユーザータスクから呼び出し可能。

### get_ds

```c
extern short get_ds(void);
```

```asm
get_ds:
    xorl  %eax, %eax
    mov   %ds, %ax
    ret
```

**概要:** 現在の DS (データセグメント) レジスタの値を返す。

**引数:** なし

**戻り値:** DS レジスタの値 (16 ビット)

**処理内容:** DS レジスタの値を EAX の下位 16 ビットにコピーして返す。

**呼び出し元:** デバッグ・診断用

**セクション:** `.user_text` -- 特権命令を含まないため、Ring 3 のユーザータスクから呼び出し可能。

### get_ss

```c
extern short get_ss(void);
```

```asm
get_ss:
    xorl  %eax, %eax
    mov   %ss, %ax
    ret
```

**概要:** 現在の SS (スタックセグメント) レジスタの値を返す。

**引数:** なし

**戻り値:** SS レジスタの値 (16 ビット)

**処理内容:** SS レジスタの値を EAX の下位 16 ビットにコピーして返す。

**呼び出し元:** デバッグ・診断用

**セクション:** `.user_text` -- 特権命令を含まないため、Ring 3 のユーザータスクから呼び出し可能。

### get_esp

```c
extern long get_esp(void);
```

```asm
get_esp:
    xorl  %eax, %eax
    movl  %esp, %eax
    ret
```

**概要:** 現在の ESP (スタックポインタ) レジスタの値を返す。

**引数:** なし

**戻り値:** ESP レジスタの値 (32 ビット)

**処理内容:** ESP の値を EAX にコピーして返す。

**呼び出し元:** デバッグ・診断用

**注意点:** 返される ESP の値は `get_esp` 関数呼び出し後のものであるため、呼び出し元の実際の ESP とは若干異なる (リターンアドレスがスタックにプッシュされているため)。

**セクション:** `.user_text` -- 特権命令を含まないため、Ring 3 のユーザータスクから呼び出し可能。

### get_eflags

```c
extern long get_eflags(void);
```

```asm
get_eflags:
    pushfl
    popl  %eax
    ret
```

**概要:** 現在の EFLAGS レジスタの値を返す。

**引数:** なし

**戻り値:** EFLAGS レジスタの値 (32 ビット)

**処理内容:** `pushfl` で EFLAGS をスタックにプッシュし、`popl` で EAX に取得する。

**呼び出し元:** デバッグ・診断用 (IF ビットの確認など)

**セクション:** `.user_text` -- 特権命令を含まないため、Ring 3 のユーザータスクから呼び出し可能。

### iget_uesp

```c
extern long iget_uesp(void);
```

```asm
iget_uesp:
    xorl  %eax, %eax
    movl  32(%esp), %eax
    ret
```

**概要:** 割り込みフレームからユーザーモードの ESP を読み取る。

**引数:** なし

**戻り値:** ユーザーモードの ESP 値 (32 ビット)

**処理内容:** 現在のスタック上の割り込みフレームからユーザー ESP (`32(%esp)`) を読み取って返す。

**呼び出し元:** 割り込みハンドラ内 (C 関数からの呼び出し)

**注意点:** スタック上のオフセット (`32(%esp)`) は、`save` 後の呼び出しコンテキストに依存する。呼び出すタイミングによって正しい値が得られない場合がある。

### iget_ueip

```c
extern long iget_ueip(void);
```

```asm
iget_ueip:
    xorl  %eax, %eax
    movl  (%esp), %eax
    ret
```

**概要:** 割り込みフレームからユーザーモードの EIP を読み取る。

**引数:** なし

**戻り値:** ユーザーモードの EIP 値 (32 ビット)

**処理内容:** 現在のスタック上の戻りアドレス (`(%esp)`) を読み取って返す。

**呼び出し元:** 割り込みハンドラ内

**注意点:** 実際には `(%esp)` は `iget_ueip` 自体のリターンアドレスであり、ユーザー EIP ではない可能性がある。使用時にはスタックレイアウトを慎重に考慮する必要がある。

### start_first_task

```c
extern void start_first_task(void);
```

```asm
start_first_task:
    movw    $0x38, %ax          /* SEL_TSS0 */
    ltr     %ax                 /* Load Task Register (no register swap) */
    movl    current_proc, %ebx  /* current_proc[0] = &proc[1] */
    movl    (%ebx), %esp        /* ESP = proc[1].kern_esp */
    ret                         /* -> intr_return_restore -> RESTORE_ALL -> iret */
```

**概要:** CPU 0 (BSP) の最初のユーザータスクを開始する。

**引数:** なし

**戻り値:** 戻らない (`ret` で `intr_return_restore` にジャンプし、Ring 3 に遷移する)

**処理内容:**

1. `ltr` で Task Register に `SEL_TSS0` (0x38) をロードする。これにより CPU が Ring 3→Ring 0 遷移時に TSS0 の esp0/ss0 を参照するようになる。`ltr` はレジスタの切り替えを行わない (TSS の内容はロードされない)
2. `current_proc[0]` (= `&proc[1]`) から `kern_esp` を ESP にロードする
3. `ret` が `kern_esp` 上の戻りアドレス (`intr_return_restore`) を pop し、`RESTORE_ALL` + `iret` で Ring 3 に遷移する

**呼び出し元:** `smp_init()` (BSP 初期化の最終ステップ)

**注意点:**

- ハードウェアタスクスイッチ (`ljmp`) は使用しない。`ltr` + ESP ロード + `ret` によるソフトウェア遷移
- `proc_create()` が構築した偽 pt_regs フレームの EFLAGS に IF=1 が設定されているため、`iret` 完了時に割り込みが有効化される。これが `main()` 内で `csti()` を呼んではならない理由である

### start_second_task

```c
extern void start_second_task(void);
```

```asm
start_second_task:
    movw    $0x40, %ax          /* SEL_TSS1 */
    ltr     %ax                 /* Load Task Register */
    movl    current_proc+4, %ebx  /* current_proc[1] = &proc[2] */
    movl    (%ebx), %esp        /* ESP = proc[2].kern_esp */
    ret                         /* -> intr_return_restore -> RESTORE_ALL -> iret */
```

**概要:** CPU 1 (AP) の最初のユーザータスクを開始する。

**引数:** なし

**戻り値:** 戻らない (`ret` で `intr_return_restore` にジャンプし、Ring 3 に遷移する)

**処理内容:** `start_first_task()` と同じパターン。`SEL_TSS1` (0x40) を Task Register にロードし、`current_proc[1]` (= `&proc[2]`) の `kern_esp` を ESP にロードして `ret` する。

**呼び出し元:** `smp_ap_init()` (AP 初期化の最終ステップ)

**注意点:** Ring 0 で直接 `second_task()` を呼ぶと、SAVE_ALL が Ring 3→Ring 0 遷移を前提とした 14 ワードの pt_regs フレームを想定するが、Ring 0→Ring 0 では SS/ESP が push されずフレームが 2 ワード不足するため、必ず `ltr` + `ret` → `iret` で Ring 3 に遷移する必要がある。

### syscall

```c
extern int syscall(void);
```

```asm
syscall:
    pushl  %ebp
    movl   %esp, %ebp
    int    $0x99
    popl   %ebp
    ret
```

**概要:** システムコールを発行する。ソフトウェア割り込み `int 0x99` を発生させ、カーネルのシステムコールハンドラ (`intr_syscall`) を呼び出す。

**引数:** なし (引数はユーザータスク側でレジスタに設定しておく)

**戻り値:** システムコールの戻り値 (EAX)

**処理内容:**

1. EBP を退避する
2. `int $0x99` を発行する。これにより `intr_syscall` が呼ばれ、レジスタに設定されたシステムコール引数がカーネルに渡される
3. カーネル処理完了後、`iret` で復帰し、戻り値が EAX に設定されている
4. EBP を復帰して `ret` する

**呼び出し元:** ユーザータスクの ITRON API ラッパー関数 (`cre_tsk`, `act_tsk`, `slp_tsk` など)

**セクション:** `.user_text` -- Ring 3 のユーザータスクから呼び出される。`int $0x99` はソフトウェア割り込みであり特権命令ではない (IDT の DPL=3 に設定されている)。

**注意点:** コメントアウトされた `syscall2` は `lcall $0x70, $0` (コールゲート方式) の代替実装であるが、現在は `int $0x99` (ソフトウェア割り込み方式) が使用されている。

### syscall2 (未使用)

```asm
syscall2:
    lcall  $0x70, $0
    ret
```

**概要:** コールゲート方式のシステムコール (未使用)。`SEL_SYSCALL` (0x70) へのコールゲートによるシステムコール。

**注意点:** 現在は使用されていない。`int 0x99` 方式に置き換えられている。

## klib.h の追加宣言

klib.h には `klib.s` に実装がない以下の関数宣言も含まれる:

| 関数 | 説明 |
|------|------|
| `jmp_task(void)` | タスクジャンプ (未実装/別ファイル) |
| `resume(unsigned long)` | レジューム (未実装/別ファイル) |

## 補足

### I/O ポート操作の使い分け

| 関数 | データサイズ | 用途例 |
|------|-----------|--------|
| `inb` / `outb` | 8 ビット | PIC (i8259)、タイマー (8253)、キーボード (8042)、VGA (6845) |
| `inw` / `outw` | 16 ビット | NE2000 ネットワークカード |

### 割り込み制御の方針

| 関数 | 特権レベル | 用途 |
|------|-----------|------|
| `ccli()` / `csti()` | Ring 0 のみ | syscall ハンドラ内、カーネル初期化 (`cpu_lock`/`cpu_unlock`) |
| `cxchg()` | Ring 0/3 両方 | スピンロック (`smp_lock`/`smp_unlock`) |

`cli`/`sti` は Ring 3 で使用できないため、ユーザータスクから呼び出し可能な排他制御には `cxchg` ベースのスピンロックを使用する。
