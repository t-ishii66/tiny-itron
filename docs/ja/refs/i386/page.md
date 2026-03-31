# page.c / page.h / pageP.h

対象ファイル: `i386/page.c`, `i386/page.h`, `i386/pageP.h`

## 概要

i386 ページングの初期化モジュール。全ページを恒等マッピング (VA = PA) として設定し、
U/S (User/Supervisor) ビットによるメモリアクセス制御を実装する。

ページスワップは行わない。全ページは常に物理メモリ上に存在する (P=1)。
ページングの目的は、GDT のフラットセグメント (base=0, limit=4GB) では実現できない
Ring 3 からの CPU スタック領域アクセスの防止である。

### メモリ保護領域

| アドレス範囲 | アクセス | 内容 |
|---|---|---|
| 0x00000 〜 `_user_text_start` | Supervisor (RW) | カーネルコード、GDT、IDT |
| `_user_text_start` 〜 `_user_data_end` | User (RW) | .user_text + .user_data |
| `_user_data_end` 〜 0x10FFFF | Supervisor (RW) | カーネルメモリプール (kmem_alloc) + VGA + 未使用 |
| 0x110000 〜 0x74FFFF | User (RW) | メモリプール + スタックプール |
| 0x750000 〜 0x7FFFFF | Supervisor (RW) | CPU スタック (CPU0/CPU1 の SP, SP0, SP3) |
| 0xFEE00000 (1 ページ) | Supervisor (RW, PCD) | Local APIC レジスタ (APIC ID 読み取り用) |

## 定数・マクロ

### page.h: PTE フラグ

| 定数 | 値 | 説明 |
|------|------|------|
| `PTE_PRESENT` | `0x01` | P ビット: ページが物理メモリに存在する |
| `PTE_RW` | `0x02` | R/W ビット: 0=読み取り専用、1=読み書き可能 |
| `PTE_USER` | `0x04` | U/S ビット: 0=Supervisor のみ、1=User アクセス可 |
| `PTE_PWT` | `0x08` | PWT ビット: ライトスルーキャッシュ |
| `PTE_PCD` | `0x10` | PCD ビット: キャッシュ無効 |

### page.h: ページサイズ

| 定数 | 値 | 説明 |
|------|------|------|
| `PAGE_SIZE` | `4096` | 1 ページのサイズ (4KB) |
| `PAGES_PER_TABLE` | `1024` | ページテーブル/ページディレクトリのエントリ数 |

### pageP.h: メモリ保護領域

| 定数 | 値 | 説明 |
|------|------|------|
| `PAGE_TABLE_COUNT` | `2` | マッピングする 4MB 領域の数 (2 = 8MB) |
| `USER_MEM_END` | `0x750000` | メモリプール領域の終端 (ページ境界) |

`MEM_START` (`0x110000`) は `addr.h` で定義されており、`page.c` で User ページ領域の開始アドレスとして使用される。

## 構造体・型

なし。

## グローバル変数

| 変数名 | 型 | スコープ | 説明 |
|--------|------|----------|------|
| `page_dir[1024]` | `unsigned long[]` | `static` | ページディレクトリ (CR3 が指す)。4KB アラインメント |
| `page_table[2][1024]` | `unsigned long[][]` | `static` | ページテーブル。[0] は 0〜4MB、[1] は 4〜8MB をカバー。4KB アラインメント |
| `page_table_apic[1024]` | `unsigned long[]` | `static` | APIC 領域用ページテーブル。0xFEC00000〜0xFEFFFFFF をカバー。4KB アラインメント |

## 関数リファレンス

### page_init

```c
void page_init(void);
```

**概要:** ページテーブルを構築する。恒等マッピング + U/S アクセス制御を設定する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. **ページディレクトリのクリア:** 全 1024 エントリを 0 (not present) に初期化
2. **リンカシンボルの取得:** `extern char _user_text_start, _user_data_end` からユーザー領域の範囲を計算
   - `u_start`: `_user_text_start` をページ境界に切り下げ (`& ~0xFFF`)
   - `u_end`: `_user_data_end` をページ境界に切り上げ (`(addr + 0xFFF) & ~0xFFF`)
3. **ページテーブルの構築:** `PAGE_TABLE_COUNT` (2) 個のページテーブルを設定
   - 各エントリは `addr | flags` の形式で恒等マッピング
   - `addr >= u_start && addr < u_end` の場合: `PTE_PRESENT | PTE_RW | PTE_USER` (.user_text + .user_data)
   - `addr >= MEM_START (0x110000) && addr < USER_MEM_END (0x750000)` の場合: `PTE_PRESENT | PTE_RW | PTE_USER` (メモリプール + スタックプール)
   - それ以外の場合: `PTE_PRESENT | PTE_RW` (Supervisor のみ)
4. **ページディレクトリエントリの設定:** 各ページテーブルへのポインタを登録
   - フラグ: `PTE_PRESENT | PTE_RW | PTE_USER` (ページテーブル側の U/S と AND される)
5. **APIC 領域のマッピング:** 0xFEE00000 (Local APIC レジスタ) を 1 ページマップ
   - ページディレクトリインデックス: `0xFEE00000 >> 22 = 0x3FB`
   - ページテーブルインデックス: `(0xFEE00000 >> 12) & 0x3FF = 0x200`
   - フラグ: `PTE_PRESENT | PTE_RW | PTE_PCD` (Supervisor、キャッシュ無効)

**呼び出し元:** `main()` (BSP パス、`all_init()` の直後)

**注意点:**
- この関数はページテーブルを構築するだけで、ページングは有効化しない。有効化は `page_enable()` で行う。
- APIC 領域のマッピングは必須。`intr_enter`/`intr_leave` (intr.s) が毎回の割り込みで `APIC_ID_REG` (0xFEE00020) を読み取るため、ページング有効化後にこのマッピングがないと #PF が発生する。
- APIC ページは MMIO であるため `PTE_PCD` でキャッシュを無効化する。

---

### page_get_dir

```c
unsigned long page_get_dir(void);
```

**概要:** ページディレクトリの物理アドレスを返す (CR3 にロードする値)。

**引数:** なし

**戻り値:** `unsigned long` -- `page_dir` 配列のアドレス

**処理内容:** `(unsigned long)page_dir` を返す。

**呼び出し元:** `page_enable()`

**注意点:** 恒等マッピングのため、配列のリニアアドレスがそのまま物理アドレスとなる。

---

### page_enable

```c
void page_enable(void);
```

**概要:** ページディレクトリを CR3 にロードし、CR0.PG ビットをセットしてページングを有効化する。

**引数:** なし

**戻り値:** なし (`void`)

**処理内容:**

1. `page_get_dir()` でページディレクトリのアドレスを取得
2. インラインアセンブリで以下を実行:
   - `movl %0, %%cr3` -- ページディレクトリのアドレスを CR3 にロード
   - `movl %%cr0, %%eax` -- CR0 を読み取り
   - `orl $0x80000000, %%eax` -- PG ビット (bit 31) をセット
   - `movl %%eax, %%cr0` -- CR0 に書き戻し (ページング有効化)

**呼び出し元:** `main()` (BSP パスおよび AP パス)

**注意点:**
- 恒等マッピングのため、ページング有効化直後の命令も同じ仮想=物理アドレスにあり、そのまま正常に実行が続く。
- AP (CPU 1) は BSP が構築したページテーブルを共有する。AP は `page_init()` を呼ばず、`page_enable()` のみを呼ぶ。

## 補足

### ページテーブル構造図

```
CR3 → page_dir[1024]
  │
  ├── [0] → page_table[0][1024]      0x000000 〜 0x3FFFFF (4MB)
  │         Supervisor(RW): 0x000000 〜 _user_text_start (カーネル)
  │         User(RW): _user_text_start 〜 _user_data_end (.user_text/.user_data)
  │         Supervisor(RW): _user_data_end 〜 0x10FFFF (kmem プール + VGA + 未使用)
  │         User(RW): 0x110000 〜 0x3FFFFF (メモリプール)
  │
  ├── [1] → page_table[1][1024]      0x400000 〜 0x7FFFFF (4MB)
  │         User(RW): 0x400000 〜 0x74FFFF (メモリプール + スタックプール)
  │         Supervisor(RW): 0x750000 〜 0x7FFFFF (CPU スタック)
  │
  ├── [2..0x3FA] → 0 (not present)
  │
  ├── [0x3FB] → page_table_apic[1024]  0xFEC00000 〜 0xFEFFFFFF
  │             0xFEE00000 の 1 ページのみ present (APIC ID レジスタ)
  │
  └── [0x3FC..0x3FF] → 0 (not present)
```

### カーネル/ユーザー分離

カーネルコード (`.text`) は Supervisor ページに配置され、Ring 3 からはアクセスできない。
Ring 3 のユーザータスクが呼び出す必要のある関数 (syscall ラッパー、レジスタ読み取り、
VGA 書き込みなど) は `.user_text` セクションに配置され、User ページとしてマッピングされる。
この分離はページングの U/S ビットにより実現される。
