# syscall.c

対象ファイル: `i386/syscall.c`

## 概要

i386 レイヤのシステムコールディスパッチャ。ユーザーモード (Ring 3) から
`int 0x99` で発行されたシステムコールを受け取り、ITRON カーネル層の
`itron_syscall()` に転送する。

`intr_syscall` (intr.s) から `struct pt_regs *regs` を引数として呼び出される。
ユーザースタック上のシステムコール引数を `regs->esp` 経由で読み取り、
戻り値を `regs->eax` に書き込む。RESTORE_ALL が `regs->eax` を EAX レジスタに
pop するため、ユーザータスクは EAX で戻り値を受け取る。

## 定数・マクロ

なし。

## 構造体・型

なし。外部で定義された `struct pt_regs` (proc.h) を使用する。

## 外部参照

| シンボル | 定義元 | 説明 |
|---------|--------|------|
| `kernel_lk` | kernel/sched.c | Big Kernel Lock。全カーネルデータ構造を保護する |
| `itron_syscall` | kernel/syscall.c | ITRON カーネル層のシステムコールディスパッチャ |

## 関数リファレンス

### c_intr_syscall

```c
W c_intr_syscall(struct pt_regs *regs)
```

**概要:** システムコールの C 言語側エントリポイント。

**引数:**

| 引数 | 型 | 説明 |
|------|------|------|
| `regs` | `struct pt_regs *` | SAVE_ALL が構築した pt_regs フレームへのポインタ (カーネルスタック上) |

**戻り値:** `W` -- システムコールの戻り値 (エラーコードまたは結果)。

**処理内容:**

1. **CPU 判定:**
   APIC ID レジスタ (`0xFEE00020`) をビット [31:24] で読み取り、0/1 に正規化。

2. **ユーザースタックからの引数読み取り:**
   `regs->esp` がユーザースタックポインタ (`int $0x99` 時点の ESP)。
   klib.s の `syscall` ラッパーが `pushl %ebp; movl %esp, %ebp` した後に
   `int $0x99` を発行するため、ユーザースタックのレイアウトは:

   ```
   ustack[0]   saved EBP    (user_esp + 0)
   ustack[1]   return addr  (user_esp + 4)
   ustack[2]   sysid        (user_esp + 8)  ← func_code
   ustack[3]   arg1         (user_esp + 12)
   ustack[4]   arg2         (user_esp + 16)
   ustack[5]   arg3         (user_esp + 20)
   ustack[6]   arg4         (user_esp + 24)
   ustack[7]   arg5         (user_esp + 28)
   ustack[8]   arg6         (user_esp + 32)
   ```

3. **Big Kernel Lock の取得:**
   `smp_lock(&kernel_lk)` でスピンロックを取得。
   全カーネルデータ構造へのアクセスはこのロック下で行われる。

4. **ITRON カーネル層への転送:**
   ```c
   ret = itron_syscall(apic, ustack[2], ustack[3], ustack[4],
                       ustack[5], ustack[6], ustack[7], ustack[8]);
   ```
   引数: apic, sysid, arg1, arg2, arg3, arg4, arg5, arg6 (計 8 個)。

5. **Big Kernel Lock の解放:**
   `smp_unlock(&kernel_lk)` でロック解放。

6. **戻り値の書き込み:**
   `regs->eax = ret` で pt_regs フレームの EAX スロットに戻り値を書き込む。
   RESTORE_ALL がこの値を EAX レジスタに pop し、`iret` 後にユーザータスクが受け取る。

**呼び出し元:** `intr_syscall` (intr.s)

## 補足

### システムコール呼び出しフロー

```
ユーザータスク (Ring 3):
  lib/lib_tsk.c 等のラッパー関数
    → syscall(sysid, arg1, arg2, ...) を呼び出し (引数をスタックに push)

klib.s syscall ルーチン:
    pushl   %ebp
    movl    %esp, %ebp
    int     $0x99                   → CPU が Ring 3→Ring 0 に遷移
    popl    %ebp
    ret                             ← EAX に戻り値

intr.s intr_syscall:
    SAVE_ALL                        → pt_regs フレーム構築
    call    intr_enter              → k_nest[cpu]++
    pushl   %esp                    → pt_regs* を引数に push
    call    c_intr_syscall          → この関数
    addl    $4, %esp
    jmp     intr_return             → intr_leave → RESTORE_ALL → iret

c_intr_syscall:
    smp_lock(&kernel_lk)            → Big Kernel Lock 取得
    itron_syscall(apic, sysid, ...) → ITRON カーネル層で処理
    smp_unlock(&kernel_lk)          → ロック解放
    regs->eax = ret                 → pt_regs の EAX スロットに戻り値書き込み
```

### 戻り値の受け渡しメカニズム

```
カーネルスタック上の pt_regs:
  ┌─────────────────────┐
  │ ES          (0x00)  │
  │ ...                 │
  │ EAX         (0x20)  │ ← c_intr_syscall が regs->eax = ret で書き込む
  │ EIP         (0x24)  │
  │ ...                 │
  └─────────────────────┘

RESTORE_ALL:
  popl %es; ...; popl %eax   ← pt_regs から EAX に ret が pop される

iret:
  Ring 3 に復帰 → ユーザータスクの EAX に戻り値が入っている
```

### システムコール中のタスクスイッチ

`slp_tsk` や `wai_sem` など、呼び出しタスクがブロックするシステムコールでは、
`itron_syscall()` 内部で `sched_do_next_tsk()` が呼ばれ、
`sched_next_tsk_check` 用のフラグがセットされる。
実際のタスクスイッチは `intr_leave` 内で ESP スワップとして実行される。

戻り値の書き込みは `sched_do_next_tsk()` の前に行われるのではなく、
ブロック解除時 (sig_sem, wup_tsk 等) に `proc_set_return_value()` で
待ちタスクの pt_regs.eax に直接書き込まれる。

### システムコール番号

システムコール番号 (`sysid`) は `include/syscall.h` で定義されている。
負の値は TFN_xxx マクロ (例: `TFN_CRE_TSK = -0x05`) として定義され、
lib/ のラッパー関数が対応する番号を `syscall()` に渡す。
