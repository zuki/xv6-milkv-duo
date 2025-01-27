/* SPDX-License-Identifier: GPL-2.0 */
#ifndef INC_DMA_MAPPING_H
#define INC_DMA_MAPPING_H

#include <common/types.h>
#include <memalign.h>

#define dma_mapping_error(x, y)	    0

enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3,
};

void invalidate_dcache_range(unsigned long start, unsigned long end);
void flush_dcache_range(unsigned long start, unsigned long end);

/**
 * DMAデバイスが利用できるようにバッファをマップする.
 *
 * ドライバからの利用を想定したLinuxライクなDMA API。ドライバから
 * 基礎となるキャッシュ操作を隠蔽する。DMA転送を開始する前にこの
 * 関数を呼び出す。U-Bootのほとんどのアーキテクチャでは仮想アドレスは
 * 物理アドレスと一致する（ただし、sandboxのような例外もある）。
 * U-Bootはドライバレベルでiommuをサポートしていないため、DMAアドレスとも
 * 一致する。したがって、このヘルパー関数は現在のところ、キャッシュ処理を
 * 実行して、DMAデバイスのレジスタに設定されることを意図したストレート
 * マップされたdma_addressを返すだけである。
 *
 * @vaddr: address of the buffer
 * @len: length of the buffer
 * @dir: the direction of DMA
 */
static inline dma_addr_t dma_map_single(void *vaddr, size_t len,
					enum dma_data_direction dir)
{
	unsigned long addr = (unsigned long)vaddr;

	len = ALIGN(len, ARCH_DMA_MINALIGN);

	if (dir == DMA_FROM_DEVICE)
		invalidate_dcache_range(addr, addr + len);
	else
		flush_dcache_range(addr, addr + len);

	return addr;
}

/**
 * Unmap a buffer to make it available to CPU
 *
 * Linux-like DMA API that is intended to be used from drivers. This hides the
 * underlying cache operation from drivers. Call this after finishin the DMA
 * transfer.
 *
 * @addr: DMA address
 * @len: length of the buffer
 * @dir: the direction of DMA
 */
static inline void dma_unmap_single(dma_addr_t addr, size_t len,
				    enum dma_data_direction dir)
{
	len = ALIGN(len, ARCH_DMA_MINALIGN);

	if (dir != DMA_TO_DEVICE)
		invalidate_dcache_range(addr, addr + len);
}

#endif
