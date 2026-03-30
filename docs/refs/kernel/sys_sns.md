# sys_sns.c / sys_sns.h

対象ファイル: `kernel/sys_sns.c`, `kernel/sys_sns.h`

## 概要

状態参照 (sense) 関数および CPU ロック関数のスタブファイル。実際の実装は `kernel/sys_rdq.c` に存在する。本ファイルは組織的な分類目的で存在し、ヘッダファイルでは `sys_loc_cpu` と `sys_unl_cpu` の extern 宣言のみを提供する。

## ファイル内容

### sys_sns.c

```c
/* sys_sns.c - stub file. Actual implementations in sys_rdq.c */
```

ソースファイルは著作権表示とスタブであることを示すコメントのみで構成される。関数の実装は含まれない。

### sys_sns.h

```c
ER sys_loc_cpu(W);
ER sys_unl_cpu(W);
```

ヘッダファイルは `sys_loc_cpu` と `sys_unl_cpu` の 2 つの関数宣言のみを含む。ヘッダガードは使用されていない。

## 関連する実装

以下の関数は `kernel/sys_rdq.c` に実装されている。詳細は [sys_rdq.md](sys_rdq.md) を参照。

| 関数 | 戻り値 | 説明 |
|------|--------|------|
| `sys_loc_cpu(W)` | `ER` | CPU ロック (`ccli()` による割り込み禁止) |
| `sys_unl_cpu(W)` | `ER` | CPU ロック解除 (`csti()` による割り込み許可) |
| `sys_sns_ctx(W)` | `BOOL` | 非タスクコンテキスト判定 (常に `FALSE` を返すスタブ) |
| `sys_sns_loc(W)` | `BOOL` | CPU ロック状態判定 |
| `sys_sns_dsp(W)` | `BOOL` | ディスパッチ禁止状態判定 |
| `sys_sns_dpn(W)` | `BOOL` | ディスパッチ保留状態判定 (常に `FALSE` を返すスタブ) |

## 補足

- `sys_sns.h` は `sys_loc_cpu` と `sys_unl_cpu` のみを宣言しているが、これらの関数は `sys_rdq.h` でも宣言されている。一方のヘッダをインクルードすればもう一方は不要である。
- `sys_rdq.h` にはすべての sense 関数 (`sys_sns_ctx`, `sys_sns_loc`, `sys_sns_dsp`, `sys_sns_dpn`) と `sys_ref_sys` の宣言も含まれており、完全な宣言セットを提供する。
- この分離は ITRON 仕様のモジュール分類 (システム状態管理) に基づくものと考えられるが、実装上はすべて `sys_rdq.c` に統合されている。
