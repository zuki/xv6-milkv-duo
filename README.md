# xv6のMilk-v Duo (RISC-V) へのポート


[xhackerrustc/rvspoc-p2308-xv6-riscv](https://github.com/xhackerustc/rvspoc-p2308-xv6-riscv)を
ベースに使用し、[BigBrotherJu/rvspoc-p2308-xv6-riscv](https://github.com/BigBrotherJu/rvspoc-p2308-xv6-riscv)と[Virus-V/rvspoc-p2308-xv6-riscv](https://github.com/Virus-V/rvspoc-p2308-xv6-riscv)（Milkv用）、[Hongqin-Li/rpi-os](https://github.com/Hongqin-Li/rpi-os)（raspberry pi 3/4用）を参考にして機能を追加していく。

## 追加機能

1. `rpi-os`のemmcをmilk-v版のu-bootを参考にriscv対応してSDカードの読み取りが可能に (tag: v0.1)
2. [cyanurus](https://github.com/redcap97/cyanurus)からBuddy, Slabシステムを取り込み`kalloc`の代わりに使用 (tag: v0.1.2)
3. SDカード上のxv6ファイルシステムが稼働 (tag: v0.1.3)
4. ユーザライブラリとしてmuslを使用 (tag: v0.1.4)
5. シグナル機能を追加 (tag: v0.1.5)
6. mmcをu-boot由来のソースに変更 (tag: v0.1.7)
7. sdでno-1-8-vでも正常に稼働
8. ブロック長を4096に変更
9. 開発をMilk-v Duo 256に変更 (tag: duo256-v0.2)
10. mmap機能を追加 (tag: duo256_v0.3)
11. coreutils, busybox, dashを導入 (tag: duo256_v0.3.1)
12. Dynamic linkを実装 (tag: duo256_v0.4)
13. Dynamic link版のcoreutils, busybox, dashを導入(tag: duo256_v0.4.1)
