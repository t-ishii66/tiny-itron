# VGA テキストモードガイド

VGA テキストモードの仕組みと、tiny-itron でどのように画面表示を実現しているかを
ハードウェアレベルから解説する。

---

## 1. 概要

VGA テキストモードとは、ビデオカードが VRAM 上のデータを自動的に文字として
画面に描画するモードである。プログラムは特別な描画 API を呼ぶ必要はなく、
メモリに値を書き込むだけで文字が表示される。

なぜこれが可能かというと、VGA コントローラは VRAM の内容を一定周期
(垂直リフレッシュ、通常 60Hz) でスキャンし、各バイト対をフォント ROM と
属性情報に基づいてピクセルパターンに変換し、ディスプレイに出力するからである。
CPU から見ると VRAM は通常のメモリと同じようにアクセスでき、書き込んだ瞬間に
次のリフレッシュで画面に反映される。

---

## 2. VGA メモリマップ

テキストモードの VRAM は物理アドレス `0xB8000` から始まる。
80 文字 x 25 行の画面を表現するには、1 文字あたり 2 バイト（文字コード + 属性）
が必要で、合計 80 x 25 x 2 = 4000 バイトとなる。

```
i386/videoP.h:
    #define G_BASE    0xb8000      /* VRAM 先頭の物理アドレス */
    #define G_WIDTH   (80 * 2)     /* 1 行のバイト数 = 160 */
    #define G_HEIGHT  26           /* スクロールバッファ含む行数 */
    #define G_TOTAL   G_WIDTH * G_HEIGHT  /* = 4160 バイト */
```

G_HEIGHT が 25 ではなく 26 なのは、ハードウェアスクロール時に 1 行分の
バッファ余裕を持たせるためである（セクション 6 で詳述）。

### メモリレイアウト

各文字は 2 バイトで表現される:

```
アドレス: 0xB8000 + (行 * 160) + (列 * 2)

  バイト 0: 文字コード (ASCII)
  バイト 1: 属性バイト

  ┌──────┬──────┬──────┬──────┬─ ─ ─ ─┬──────┬──────┐
  │ 'H'  │ attr │ 'e'  │ attr │       │ ' '  │ attr │
  └──────┴──────┴──────┴──────┴─ ─ ─ ─┴──────┴──────┘
  B8000  B8001  B8002  B8003         B8098  B8099
  ← col 0 →    ← col 1 →           ← col 79 →

  行 0: 0xB8000 .. 0xB809F  (160 bytes)
  行 1: 0xB80A0 .. 0xB813F  (160 bytes)
   ...
  行 24: 0xB8F00 .. 0xB8F9F (160 bytes)
```

---

## 3. 属性バイト

各文字の隣にある属性バイトは、文字の色と背景色をビットフィールドで指定する。

```
  bit  7    6  5  4    3    2  1  0
  ┌──────┬──────────┬──────┬──────────┐
  │Blink │ 背景 RGB │ 高輝 │ 前景 RGB │
  └──────┴──────────┴──────┴──────────┘
    [7]   [6:4]      [3]    [2:0]

  Blink = 1: 文字が点滅する (多くのエミュレータでは高輝度背景)
  背景 RGB:  背景色 (0-7)
  高輝度:    前景色を明るくする
  前景 RGB:  前景色 (0-7)
```

### 色の対応表

| 値  | 色     | 値  | 色 (高輝度) |
|-----|--------|-----|-------------|
| 0x0 | 黒     | 0x8 | 暗い灰色   |
| 0x1 | 青     | 0x9 | 明るい青   |
| 0x2 | 緑     | 0xA | 明るい緑   |
| 0x3 | シアン | 0xB | 明るいシアン |
| 0x4 | 赤     | 0xC | 明るい赤   |
| 0x5 | マゼンタ | 0xD | 明るいマゼンタ |
| 0x6 | 茶     | 0xE | 黄色       |
| 0x7 | 灰色   | 0xF | 白         |

### tiny-itron で使われる属性値

```
i386/videoP.h:
    #define G_ATTR  0x02    /* printk のデフォルト: 緑 on 黒 */

kernel/user.c:
    #define ATTR_WHITE    0x0F    /* 白 on 黒 (高輝度) */
    #define ATTR_GREEN    0x0A    /* 明るい緑 on 黒 */
    #define ATTR_CYAN     0x0B    /* 明るいシアン on 黒 */
    #define ATTR_YELLOW   0x0E    /* 黄色 on 黒 */
    #define ATTR_GREY     0x07    /* 灰色 on 黒 */
    #define ATTR_DARK     0x08    /* 暗い灰色 on 黒 */
    #define ATTR_MAGENTA  0x0D    /* 明るいマゼンタ on 黒 */
    #define ATTR_RED      0x0C    /* 明るい赤 on 黒 */
```

すべて背景色が 0 (黒) で、前景色のみ指定している。例えば `0x0F` は:
- Blink=0, 背景=000(黒), 高輝度=1, 前景=111(白) → 白文字 on 黒背景

---

## 4. 初期化 (`video_init`)

`video_init()` は 6845 CRT コントローラを初期化する。

```c
/* i386/video.c */
void video_init(void)
{
    c_x = c_y = 0;         /* カーソル位置をリセット */
    c_y_max = 24;
    scrolltop = 0;          /* スクロールオフセット = 0 */
    video_set_6845(G_VID_ORG, scrolltop);  /* display start address = 0 */
}
```

### 6845 CRT コントローラへの I/O ポート操作

6845 はインデックスレジスタ方式でアクセスする。ポート `0x3D4` にレジスタ番号を
書き、ポート `0x3D5` でデータを読み書きする。

```
i386/io.h:
    #define IO_6845    0x3d4    /* インデックスレジスタ */
    #define IO_6845_V  0x3da    /* ステータスレジスタ (垂直リトレース検出用) */
```

`video_set_6845()` は 16 ビット値を 2 つのレジスタ (高/低バイト) に分けて書く:

```c
static void video_set_6845(unsigned short reg, unsigned short val)
{
    video_wait();                              /* 垂直リトレース待ち */
    outb(IO_6845, reg & 0xff);                /* レジスタ番号 (高バイト用) */
    outb(IO_6845 + 1, (val >> 8) & 0xff);    /* 値の高バイト */
    outb(IO_6845, (reg + 1) & 0xff);         /* レジスタ番号 (低バイト用) */
    outb(IO_6845 + 1, val & 0xff);           /* 値の低バイト */
}
```

使用するレジスタ:

| 定数      | レジスタ番号 | 名前                | 用途                       |
|-----------|-------------|---------------------|---------------------------|
| G_VID_ORG | 12, 13      | Start Address H/L   | VRAM 表示開始位置 (文字単位) |
| G_CURSOR  | 14, 15      | Cursor Location H/L | カーソル位置               |

---

## 5. 文字出力の仕組み

### `video_putc` — 1 文字出力

```c
/* i386/video.c */
void video_putc(char c)
{
    unsigned char *p = (unsigned char *)G_BASE;
    p += 2 * scrolltop + c_y * 160 + c_x * 2;
    /* アドレス計算:
     *   G_BASE (0xB8000)
     * + 2 * scrolltop  (スクロールによるオフセット、バイト単位)
     * + c_y * 160       (行: 1行=80文字×2バイト)
     * + c_x * 2         (列: 1文字=2バイト)
     */

    if (c == '\n') {
        c_y++; c_x = 0;           /* 改行: 次の行の先頭へ */
    } else {
        video_wait();
        *p = c;                    /* 文字コードを書く */
        *(p + 1) = G_ATTR;        /* 属性バイト (0x02, 緑) */
        c_x++;
        if (c_x >= 80) {          /* 行末折り返し */
            c_x = 0;
            c_y++;
        }
    }
    if (c_y > 24) {
        video_scroll();            /* 画面下端を超えたらスクロール */
        c_y = 24;
    }
}
```

ポイント:
- `scrolltop` は現在のスクロール位置を **文字単位** で保持する。
  VRAM アドレス計算時には `2 * scrolltop` でバイト単位に変換する
- 属性は常に `G_ATTR` (0x02, 緑) が使われる。色を指定する機能は
  `printk` にはない

### `video_puts` — 文字列出力

`video_putc` を繰り返し呼ぶだけのシンプルな実装:

```c
void video_puts(char *p)
{
    while (*p != '\0')
        video_putc(*p++);
}
```

---

## 6. ハードウェアスクロール

VGA テキストモードには「表示開始アドレスの変更」というハードウェア機能がある。
6845 の Start Address レジスタ (レジスタ 12-13) を変更すると、VRAM のどの位置
から表示を開始するかを制御できる。メモリのコピーなしにスクロールが実現できる。

### `video_scroll` の動作

```c
static void video_scroll(void)
{
    scrolltop += 80;    /* 1行 = 80文字 分だけ表示開始位置を進める */
    c_y_max++;
    video_set_6845(G_VID_ORG, scrolltop);  /* 6845 に新しい開始位置を書く */
    video_clear_line();                     /* 新しい最終行をクリア */

    if (scrolltop > 80 * 25) {
        /* VRAM 末尾に到達 — ラップアラウンド */
        video_copy(G_TOTAL + G_BASE, G_BASE, G_TOTAL);  /* VRAM をコピー */
        c_y = 24;
        c_y_max = 24;
        scrolltop = 0;
        video_set_6845(G_VID_ORG, scrolltop);  /* 先頭に戻す */
        video_clear_line();
    }
}
```

通常のスクロール（`scrolltop` が VRAM 範囲内）:
```
  スクロール前:              スクロール後:
  scrolltop = 0              scrolltop = 80

  VRAM:                      VRAM:
  ┌──────────────┐           ┌──────────────┐
  │ 行 0 (表示)  │ ← 表示    │ 行 0 (非表示)│
  │ 行 1 (表示)  │           │ 行 1 (表示)  │ ← 表示開始
  │   ...        │           │   ...        │
  │ 行 24 (表示) │           │ 行 24 (表示) │
  │              │           │ 行 25 (空白) │ ← 新しい最終行
  └──────────────┘           └──────────────┘
```

G_HEIGHT が 26 (25 + 1) なのは、このスクロール時に 1 行分の余裕を確保するため。
`scrolltop > 80 * 25` (2000 文字超え) になるとバッファの末尾に到達するため、
VRAM 全体をコピーして先頭に巻き戻す。

---

## 7. printk (Ring 0 専用)

`printk` は C の printf 風のフォーマット出力関数である。

### フォーマット指定子

| 指定子 | 型            | 出力                    |
|--------|--------------|------------------------|
| `%s`   | `char*`      | 文字列                  |
| `%x`   | `unsigned int`| 16 進数                |
| `%d`   | `unsigned int`| 10 進数                |
| `%c`   | `char`       | 1 文字                  |

### SMP スピンロック

マルチ CPU 環境では 2 つの CPU が同時に `printk` を呼ぶ可能性がある。
文字出力中にカーソル位置 (`c_x`, `c_y`) が競合すると表示が壊れるため、
`xchgl` ベースのスピンロック (`video_lk`) で排他制御する。

```c
/* i386/video.c */
static unsigned long video_lk = 0;

void printk(char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    smp_lock(&video_lk);       /* スピンロック獲得 */
    /* ... フォーマット処理 ... */
    smp_unlock(&video_lk);     /* スピンロック解放 */
}
```

**なぜ `kernel_lk` ではなく別の `video_lk` を使うのか**:
ISR ハンドラ (`c_intr_irq0` 等) は `kernel_lk` を保持した状態で呼ばれる。
もし `printk` 内でも `kernel_lk` を取得すると、ISR 内から `printk` を
呼んだ瞬間に **同一 CPU でデッドロック** する (`xchgl` スピンロックは非再帰)。
そのため VGA 出力には別のロック変数 `video_lk` を用いる。

現在のランタイムコードでは ISR 内から `printk` を呼ぶ箇所はないが
(画面更新は `vga_write_dec_at` 等のロックなし関数で行っている)、
デバッグ時に ISR 内へ `printk` を差し込むことは頻繁にあるため、
安全策として分離してある。

### カスタム va_list 実装

標準ライブラリを使えないため、`va_list` を独自に実装している:

```c
/* i386/videoP.h */
typedef char*  va_list;
#define va_start(ap, param)  (ap = (char*)&param + sizeof(param))
#define va_arg(ap, type)     ((type*)(ap += sizeof(type)))[-1]
#define va_end(ap)
```

これは x86 の cdecl 呼出規約（引数がスタックに右→左で積まれる）を前提に、
最後の固定引数の直後のアドレスから可変引数を順に読み出す仕組みである。

### Ring 3 から printk を呼べない理由

`printk` → `video_putc` → `video_scroll` → `video_set_6845` → `outb` という
呼び出しチェーンで、最終的に I/O ポート命令 (`outb`) が実行される。
i386 では IOPL (I/O Privilege Level) が CPL (Current Privilege Level) 以上
でなければ I/O ポート命令が実行できない。このカーネルでは IOPL=0 のため、
Ring 3 (CPL=3) から `outb` を実行すると #GP (General Protection Fault) が
発生する。

---

## 8. Ring 3 からの VGA アクセス

ユーザータスク (Ring 3) から画面表示を行うには、2 つの障壁がある:

1. **I/O ポート制限**: `outb`/`inb` は IOPL ≧ CPL でなければ実行できない。
   IOPL=0 のため Ring 3 (CPL=3) からは **#GP** (General Protection Fault、
   一般保護違反例外) が発生する
2. **ページ保護**: VGA VRAM (0xB8000) はページテーブルで Supervisor (U/S=0) に
   設定されている。Ring 3 からメモリアクセスすると **#PF** (Page Fault、
   ページフォルト例外) が発生する

### vga_write_at — I/O ポートを使わない直接書き込み関数

`vga_write_at()` と `vga_write_dec_at()` は I/O ポート命令を一切使わず、
VRAM に直接書き込む関数として設計されている。6845 レジスタの操作（スクロール、
カーソル移動）を行わないため、上記の障壁 1 (#GP) は回避される。

```c
/* i386/video.c */
void vga_write_at(int row, int col, char *s, unsigned char attr)
{
    unsigned short *p = (unsigned short *)G_BASE;
    p += row * 80 + col;       /* 固定位置アドレス計算 */
    while (*s && col < 80) {
        *p++ = (unsigned short)attr << 8 | (unsigned char)*s++;
        col++;
    }
}
```

ただし、これらの関数はスクロールしない。画面上の固定位置に上書きする方式のため、
printk のようなストリーム出力はできない。

**Ring 3 から直接 `vga_write_at()` を呼べるか？** — 呼べない。
障壁 1 (#GP) は避けられるが、障壁 2 (#PF) が残る。`vga_write_at()` は
0xB8000 (Supervisor ページ) に書き込むため、Ring 3 からの実行はページフォルトになる。
`vga_write_at()` はカーネル (Ring 0) から呼ぶための関数である。

### 解決策: syscall ラッパー経由のアクセス

ユーザータスクは syscall 経由でカーネル (Ring 0) に遷移し、Ring 0 で
`vga_write_at()` を実行してもらう。これで障壁 1 (#GP) も障壁 2 (#PF) も回避される:

```
ユーザータスク (Ring 3)       カーネル (Ring 0)
───────────────────────────   ────────────────────────
print_at(row, col, s, attr)
  → syscall(-TFN_EXD_VGA_WRITE, ...)
    → int $0x99                 → c_intr_syscall()
                                  → sys_vga_write_at()
                                    → vga_write_at()
                                      → VRAM 書き込み
                                ← iret
  ← 戻り値
```

### ユーザー側ラッパー (lib/lib_exd.c)

```c
void print_at(int row, int col, char *s, unsigned char attr)
{
    syscall(-TFN_EXD_VGA_WRITE, row, col, s, attr);
}

void print_dec_at(int row, int col, unsigned long n, int width,
                  unsigned char attr)
{
    syscall(-TFN_EXD_VGA_DEC, row, col, n, width, attr);
}

void clear_screen(void)
{
    syscall(-TFN_EXD_VGA_CLEAR);
}

void fill_at(int row, int col, int len, int ch, unsigned char attr)
{
    syscall(-TFN_EXD_VGA_FILL, row, col, len, ch, attr);
}
```

### カーネル側ハンドラ (kernel/sys_exd.c)

```c
ER sys_vga_write_at(W apic, int row, int col, char *s, unsigned char attr)
{
    vga_write_at(row, col, s, attr);
    return E_OK;
}

ER sys_vga_fill_at(W apic, int row, int col, int len, int ch,
                   unsigned char attr)
{
    unsigned short *p = (unsigned short *)0xB8000 + row * 80 + col;
    int i;
    for (i = 0; i < len && col + i < 80; i++)
        p[i] = (unsigned short)attr << 8 | (unsigned char)ch;
    return E_OK;
}
```

### I/O ポートを操作しない設計

`vga_write_at()` 等の関数は I/O ポート (6845 レジスタ) を操作せず、
VRAM (0xB8000) への書き込みだけで画面表示を行う。
6845 は `video_init()` が起動時に初期化済みであり、以降は触る必要がない。

---

## 9. ユーザータスクでの使用例

`kernel/user.c` はデモ画面のレイアウトを固定行・列で管理している。

### 画面レイアウト定数

```c
/* kernel/user.c */
#define ROW_HEADER     0      /* タイトル行 */
#define ROW_SEP        1      /* 区切り線 */
#define ROW_TIMER      3      /* タイマーティック表示 */
#define ROW_TASK1      5      /* Task 1 カウンタ + MBF 受信文字列 */
#define ROW_TASK3      6      /* Task 3 カウンタ + セマフォ状態 */
#define ROW_SHARED     7      /* 共有カウンタ */
#define ROW_TASK2      8      /* Task 2 カウンタ + セマフォ状態 */
#define ROW_KBD        9      /* Task 4 キーボードエコー */
#define ROW_COPYRIGHT  24     /* 著作権表示 */
```

### 具体的な使用パターン

**文字列の表示:**
```c
print_at(ROW_HEADER, 2, "TinyItron/386 SMP (2 CPU)", ATTR_WHITE);
```

**数値の右詰め表示:**
```c
/* 8 桁の右詰めで task_count[1] を緑色で表示 */
print_dec_at(ROW_TASK1, 19, task_count[1], 8, ATTR_GREEN);
```

**セマフォ状態の表示:**
```c
/* セマフォ獲得成功 → 黄色で "LOCK" */
print_at(ROW_TASK3, 31, "LOCK", ATTR_YELLOW);

/* セマフォ獲得失敗 (他 CPU が保持中) → 赤で "BUSY" */
print_at(ROW_TASK3, 31, "BUSY", ATTR_RED);

/* セマフォ未操作 → 暗い灰色で "----" */
print_at(ROW_TASK3, 31, "----", ATTR_DARK);
```

**領域の塗りつぶし (キーボードエコー行のクリア):**
```c
fill_at(ROW_KBD, 20, kbd_max - 20, ' ', 0x0E);
```

### タイマーティックの表示 (ISR からの直接書き込み)

タイマーティックの表示は syscall 経由ではなく、ISR (Ring 0) から
`vga_write_dec_at()` を直接呼んでいる。ISR は Ring 0 で動作するため、
VGA VRAM への直接アクセスに問題はない:

```c
/* i386/timer.c */
void timer_intr(unsigned char apic, unsigned long delta)
{
    if (apic == 0) {
        timer_ticks++;
        vga_write_dec_at(3, 21, timer_ticks, 10, 0x0B);  /* シアン色 */
    }
    sched_timeout(apic, delta);
}
```

---

## 参照ソースファイル

| ファイル           | 内容                                |
|-------------------|-------------------------------------|
| i386/videoP.h     | VGA 定数定義、va_list マクロ         |
| i386/video.h      | 公開 API 宣言                       |
| i386/video.c      | 全実装 (printk, vga_write_at 等)    |
| i386/io.h         | I/O ポートアドレス (IO_6845=0x3D4)  |
| kernel/sys_exd.c  | syscall ハンドラ                    |
| lib/lib_exd.c     | ユーザーラッパー                    |
| kernel/user.c     | 使用例 (画面レイアウト、タスクコード) |
| i386/page.c       | ページテーブル (0xB8000 の U/S 設定) |
