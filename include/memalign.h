/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2015 Google, Inc
 */

#ifndef INC_ALIGNMEM_H
#define INC_ALIGNMEM_H

/*
 * ARCH_DMA_MINALIGN is defined in asm/cache.h for each architecture.  It
 * is used to align DMA buffers.
 */
#ifndef __ASSEMBLY__

#define ALIGN(x, a)              (((x) + (a) - 1) & ~((a) - 1))

#define ARCH_DMA_MINALIGN   64
#define ROUND(a, b)         (((a) + (b) - 1) & ~((b) - 1))
#define DIV_ROUND_UP(n,d)   (((n) + (d) - 1) / (d))

#define DIV_ROUND_DOWN_ULL(ll, d) \
    ({ unsigned long long _tmp = (ll); do_div(_tmp, d); _tmp; })

#define DIV_ROUND_UP_ULL(ll, d) DIV_ROUND_DOWN_ULL((ll) + (d) - 1, (d))

/*
 * ALLOC_CACHE_ALIGN_BUFFERマクロはDMAの最小アーキテクチャアライメント要件を
 * 満たすバッファをスタック上に割り当てるために使用される。
 * このようなバッファは読み出しや書き込み操作の前後にキャッシュのフラッシュや
 * 無効化が必要なDMA操作に有用である。
 *
 * マクロが呼び出されるとスタック上に以下のようなサイズの配列を作成する.
 *
 * 1) 配列の先頭はアラインされるのに十分な位置まで進められる。
 *
 * 2) 配列のアライメントされた部分のサイズはそのアーキテクチャでDMAに必要な
 *    最小のアライメントの倍数である。
 *
 * 3) アラインされた部分には要求された元の要素数に十分なスペースがある。
 *
 * マクロは次にこの配列のアライメントされた部分へのポインタを作成し、ポインタに
 * 配列のアライメントされた部分の最初の要素のアドレスを代入する。
 *
 * マクロを次のように呼び出すと
 *
 *      ALLOC_CACHE_ALIGN_BUFFER(uint32_t,buffer,1024);
 *
 * 次のような結果となる。
 *
 *      uint32_t buffer[1024];
 *
 * 以下の違いがある。
 *
 *  1) 結果として得られるバッファは ARCH_DMA_MINALIGN の値に
 *     アラインされることが保証される。
 *
 *  2) マクロによって生成されるバッファ変数は指定された型への
 *     ポインタであり、指定された型の配列ではない。 バッファの
 *     アドレスをDMAハードウェアに渡したい場合、これは非常に
 *     重要である。&bufferの値はこの2つのケースで異なる。マクロの
 *     場合、バッファ用に確保された空間のアドレスではなく、ポインタの
 *     アドレスになる。2番目のケースではバッファのアドレスである。
 *     したがって、ハードコードされたスタックバッファをこのマクロで
 *     置き換える場合はバッファのアドレスを取る場所から&を削除する
 *     必要がある。
 *
 * sizeパラメータはバイト数ではなく、確保する配列要素数であることに注意。
 *
 * このマクロは関数スコープ外では使用できず、関数スコープのスタティック
 * バッファの作成にも使用できない。また、キャッシュラインアライメントの
 * グローバルバッファの作成にも使用できな。
 */
#define PAD_COUNT(s, pad) (((s) - 1) / (pad) + 1)
#define PAD_SIZE(s, pad) (PAD_COUNT(s, pad) * pad)
#define ALLOC_ALIGN_BUFFER_PAD(type, name, size, align, pad)        \
    char __##name[ROUND(PAD_SIZE((size) * sizeof(type), pad), align)  \
              + (align - 1)];                    \
                                    \
    type *name = (type *)ALIGN((uintptr_t)__##name, align)
#define ALLOC_ALIGN_BUFFER(type, name, size, align)        \
    ALLOC_ALIGN_BUFFER_PAD(type, name, size, align, 1)
#define ALLOC_CACHE_ALIGN_BUFFER_PAD(type, name, size, pad)        \
    ALLOC_ALIGN_BUFFER_PAD(type, name, size, ARCH_DMA_MINALIGN, pad)
#define ALLOC_CACHE_ALIGN_BUFFER(type, name, size)            \
    ALLOC_ALIGN_BUFFER(type, name, size, ARCH_DMA_MINALIGN)

/*
 * DEFINE_CACHE_ALIGN_BUFFER() is similar to ALLOC_CACHE_ALIGN_BUFFER, but it's
 * purpose is to allow allocating aligned buffers outside of function scope.
 * Usage of this macro shall be avoided or used with extreme care!
 */
#define DEFINE_ALIGN_BUFFER(type, name, size, align)            \
    static char __##name[ALIGN(size * sizeof(type), align)]    \
            __aligned(align);                \
                                    \
    static type *name = (type *)__##name
#define DEFINE_CACHE_ALIGN_BUFFER(type, name, size)            \
    DEFINE_ALIGN_BUFFER(type, name, size, ARCH_DMA_MINALIGN)

#endif

#endif /* __ALIGNMEM_H */
