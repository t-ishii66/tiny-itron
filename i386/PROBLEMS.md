# i386 ディレクトリ - コードの問題点

`i386/` ディレクトリ内のソースコードをレビューした結果、以下の問題を発見した。

---

## 致命的な問題

### 1. `smp_cpu_lock` の名前衝突と型の不一致

シンボル `smp_cpu_lock` が、異なるファイルで**関数**と**変数**の両方として使われている:

- `kernelval.c:23` -- `char smp_cpu_lock[2];` (2バイトの配列として定義)
- `service.c:35` -- `void smp_cpu_lock()` (関数として定義)
- `smp.c:13` -- `extern int smp_cpu_lock[];` (int配列として参照)
- `interrupt.c:26` -- `extern int smp_cpu_lock[];` (int配列として参照)

3つの問題がある:

1. **名前衝突**: 関数と変数が同じリンカシンボル名を共有している。
2. **型の不一致**: 変数は `char[2]` だが `int[]` として参照されている。`sizeof(int)` は4バイトのため、`smp_cpu_lock[0]` を `int` としてアクセスすると2バイトのオブジェクトから4バイト読み取ってしまう(バッファオーバーリード)。
3. **意味の混乱**: `interrupt.c:318` は `smp_cpu_lock[0]` をロックフラグとしてテスト・`++` で変更しているが、リンカがこれを関数アドレスに解決した場合、関数の機械語コードをデータとして操作することになる。

### 2. `proc[]` 配列の off-by-one (`proc_init()`)

- `kernelval.c:9`: `proc_t proc[MAX_TSKID];` -- 有効なインデックスは 0 〜 `MAX_TSKID - 1` (0..15)
- `proc.c:24`: `for (i = 0 ; i <= MAX_TSKID ; i ++)` -- `proc[16]` にアクセスし、配列の末尾を超える

起動のたびに `proc` 配列の範囲外にメモリ書き込みが発生する。

### 3. スピンロック/ビジーウェイト変数に `volatile` がない

複数の変数がビジーウェイトループで使われているが `volatile` が付いていない。コンパイラがループを最適化で除去する可能性があり、無限ハングまたはウェイトのスキップが起こりうる:

- **`service.c:31`**: `unsigned long sclk[2] = {0, 0};` -- `while (sclk[0] == 1)` (60行目) および `while (sclk[1] == 1)` (66行目) で使用
- **`smp.c:11`**: `static int cpu_second = 0;` -- `while (cpu_second == 0)` (101行目) で使用
- **`floppy.c:126`**: `fdc_sleep(int* val)` -- `*val = 1; while (*val);` としているが `val` が `volatile int*` でない
- **`interrupt.c:318-321`**: `smp_cpu_lock[0]` が `while (smp_cpu_lock[0])` で volatile なしに使用

### 4. ビットマスク比較が常に偽 (`proc_eflags_save()`)

`proc.c:119`:
```c
if ((proc[tskid].reg[EFLAGS] & 0x00000200) == 1)
```

`0x200` とのビットANDの結果は `0` か `0x200` (512) であり、`1` にはならない。この条件は常に偽であり、EFLAGSの保存ロジックは実行されない。正しい比較: `!= 0` または `== 0x200`。

注意: このコードは118行目の `return;` により、そもそも到達不能でもある(問題 #6 参照)。

### 5. NE2000 の DMA完了チェックが反転している

`ne2000.c:697-700`:
```c
for (i = 0 ; i < 256 ; i ++) {
    if ((inb(IO_DP8390 + DP_ISR) & ISR_RDC) == 0)
        break;
}
```

`ISR_RDC` ビットは Remote DMA Complete 時に**セット**される。`== 0` のチェックはビットが**クリア**の時(= DMA未完了)にブレークしてしまう。正しいチェックとの比較 (`ne2000.c:605`):
```c
if ((inb(IO_DP8390 + DP_ISR) & ISR_RDC) != 0)
    break;
```

NE2000の書き込みパス (`ne2000_putblock`) が DMA完了を正しく待たないため、ネットワーク上でデータ破損を引き起こす可能性がある。

---

## 中程度の問題

### 6. デッドコード: `proc.c` の関数内で早期 `return` によりロジックが到達不能

3つの関数で `return;` 文のために残りのコードが到達不能になっている:

- `proc_eflags_save()` (118行目): `return;` の後に EFLAGS のチェックと変更がある
- `proc_eflags_restore()` (128行目): `return;` の後に復元ロジックがある
- `proc_switch()` (136行目): `return;` の後に切り替えロジックがある

デバッグ用のスタブと思われるが、SMP の EFLAGS 保存/復元および手動プロセス切り替えが完全に機能しない状態になっている。

### 7. `fdc_result()` でエラーリターンが到達不能

`floppy.c:71-72` および `77-78`:
```c
continue;
    return ERR_STAT;  /* 到達不能 */
```

`continue` 文が次行の `return ERR_STAT` をスキップする。エラー条件が無視されてループが継続し、`return ERR_STAT` は実行されない。

### 8. キーボードのスキャンコード配列の境界チェック不足

`keyboard.c:42-45`:
```c
if (mode & SHIFT)
    printk("%c", scode_sh[c]);
else
    printk("%c", scode[c]);
```

`scode[]` と `scode_sh[]` 配列 (`keyboardP.h` で定義) は約86要素。`inb(IO_KEY)` から取得した生のスキャンコード `c` は 0〜127 の範囲をとりうるが(ビット7のキーアップチェックは26行目で一部のスキャンコードのみ)、配列サイズを超えるスキャンコードで境界外読み取りが発生する。

### 9. `elf.c` のバッファオーバーフローリスク

`elf.c:9`:
```c
static char buf[640000];
```

`elf.c:53`:
```c
readn(fd_s, buf, e_ph[i].p_filesz);
```

`e_ph[i].p_filesz <= sizeof(buf)` のチェックがない。`p_filesz` が大きい ELF ファイルを入力するとバッファオーバーフローが発生する。これはホスト側のビルドツールでありカーネルコードではないが、注意が必要。

### 10. APIC ICR 書き込みの括弧が紛らわしい

`smp.c:97`:
```c
*p = ((a & (0xfff0f000) | 0x603));
```

`0xfff0f000` の周りの余分な括弧が紛らわしい。Cの演算子優先順位により (`&` は `|` より優先)、実際には `(a & 0xfff0f000) | 0x603` と評価されるが、括弧の付け方から `a & (0xfff0f000 | 0x603)` = `a & 0xfff0f603` を意図した可能性もある。115行目にも同じパターンがある。

### 11. APIC レジスタアクセスに `volatile` がない

`smp.c` では APIC のメモリマップドレジスタを通常の `unsigned long*` ポインタでアクセスしている(34, 42, 46, 59, 66行目など):
```c
unsigned long* p = APIC_EOI;
*p = 0L;
```

メモリマップドI/Oレジスタは、コンパイラによるアクセスの並べ替え・キャッシュ・除去を防ぐために `volatile` ポインタを通じてアクセスしなければならない。`APIC_VERSION`, `APIC_ID`, `SVR`, `LVT1`, `LVT2`, `LVT3`, `ICR_HI`, `ICR_LOW`, `TIMER`, `TIMER_INIT_COUNT`, `APIC_EOI` の全てのアクセスが該当する。

### 12. `smp_timer()` のポインタキャストの誤り

`smp.c:158`:
```c
p = (unsigned long)TIMER_INIT_COUNT;
```

整数(アドレス)を `unsigned long*` ポインタに代入しているが、キャストが `unsigned long*` ではなく `unsigned long` になっている。コンパイルは通るが、技術的には未定義動作である。`(unsigned long*)TIMER_INIT_COUNT` とすべき。

---

## 軽微な問題

### 13. `readn()` と `writen()` の戻り値が未定義

`elf.c:92-103` および `106-117`: 両関数は `int` を返す宣言だが、エラー時の `return -1` しか明示的な return がない。正常終了時は関数の末尾からそのまま抜けており、戻り値が未定義動作となる。

### 14. `elf.c` の `open()` で mode 引数が欠けている

`elf.c:81`:
```c
fd_d = open(argv[2], O_RDWR | O_CREAT)
```

`O_CREAT` 使用時には `open()` に第3引数の `mode` が必要。引数がないためファイルのパーミッションが不定になる。

### 15. デバッグ用 `printk()` がコードに残っている

多くのファイルにカラム0(通常のインデントの外)に一時的なデバッグ出力と思われる `printk()` 文が残っている:

- `proc.c:117` -- EFLAGS の値を出力
- `interrupt.c:329` -- timer0 割り込みごとに "0" を出力
- `interrupt.c:372` -- timer1 割り込みごとに "1" を出力
- `floppy.c:70,76,86` -- fdc_result のデバッグ出力
- `elf.c:18,36-42,57` -- ELF解析のデバッグ出力

### 16. `elf.c` の `write_space()` で未初期化データを書き込む

`elf.c:16`:
```c
lseek(fd_d, len - 1, SEEK_CUR);
writen(fd_d, buf, 1);
```

ローカル変数 `buf[2]` (グローバルバッファとは別) から未初期化の1バイトを書き込んでセグメント間のギャップを埋めている。パディングバイトの値は不定。

### 17. マジックナンバーの多用

ハードウェアレジスタのオフセット、ビットマスク、メモリアドレス、セグメントセレクタが、コードベース全体で生の数値リテラルとして使われている(例: `0xfff0f000`, `0x603`, `0x00020000`, `proc_create` 内の `52`)。名前付き定数にすることで可読性と保守性が向上する。
