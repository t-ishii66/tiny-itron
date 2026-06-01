# include/stdio.h - メモリ管理構造体と printk 宣言

## 概要

カーネル内メモリ管理のためのデータ構造体と、カーネル printf 関数 (`printk`) の宣言を定義する。
`itron.h` を `#include` しており、ITRON の基本型を使用する。

## 定数

### メモリアロケーション

| 定数 | 値 | 説明 |
|------|------------|------|
| `ALLOCATION_SIZE` | `4096` | アロケーション管理テーブルのエントリ数 |
| `MEM_FLAG_ALLOC` | `0x01` | 割り当て済みフラグ |
| `MEM_FLAG_FREE` | `0x00` | 未使用フラグ |
| `MEM_ALIGN` | `0xfffff000` | 4K ページアラインメントマスク |

`MEM_ALIGN` は下位 12 ビットをマスクすることで、アドレスを 4KB (4096 バイト) 境界にアラインする。
`addr & MEM_ALIGN` とすることで、アドレスの 4K ページ先頭を取得できる。

## 構造体

### allocation_t - メモリアロケーション管理エントリ

```c
typedef struct allocation {
    B       flag;       /* 割り当てフラグ */
    VP      base;       /* メモリブロック先頭アドレス */
    UW      size;       /* メモリブロックサイズ */
} allocation_t;
```

| フィールド | 型 | 説明 |
|------------|------|------|
| `flag` | `B` | 割り当て状態 (`MEM_FLAG_ALLOC`=割り当て済み, `MEM_FLAG_FREE`=未使用) |
| `base` | `VP` | メモリブロックの先頭アドレス |
| `size` | `UW` | メモリブロックのサイズ (バイト) |

最大 `ALLOCATION_SIZE` (4096) 個のエントリで構成される管理テーブルで、カーネル内のメモリ割り当て状態を追跡する。

## 関数

### printk - カーネル printf

```c
void printk(char*, ...);
```

カーネル空間で使用可能な `printf` 相当の関数。
可変引数を受け取り、VGA テキストモードの画面にフォーマット済み文字列を出力する。

ユーザータスクからは `TFN_EXD_PRINT` ファンクションコードを使った syscall 経由で `printk` が呼ばれる (lib 層の `printf` ラッパー)。
