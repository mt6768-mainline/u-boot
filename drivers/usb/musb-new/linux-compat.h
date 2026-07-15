#ifndef __LINUX_COMPAT_H__
#define __LINUX_COMPAT_H__

#include <malloc.h>
#include <asm/cache.h>
#include <cpu_func.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/compat.h>

#define device_init_wakeup(dev, a) do {} while (0)

#define platform_data device_data

#define msleep(a)	udelay(a * 1000)

/*
 * Map U-Boot config options to Linux ones
 */
#ifdef CONFIG_OMAP34XX
#define CFG_SOC_OMAP3430
#endif

/* DMA mapping helpers for U-Boot without an IOMMU. */

#define DMA_TO_DEVICE		0
#define DMA_FROM_DEVICE		1
#define DMA_BIDIRECTIONAL	2
#define DMA_NONE		3

static inline dma_addr_t dma_map_single(void *dev, void *ptr,
					size_t size, int direction)
{
	unsigned long addr = (unsigned long)ptr;
	unsigned long start = rounddown(addr, ARCH_DMA_MINALIGN);
	unsigned long end = roundup(addr + size, ARCH_DMA_MINALIGN);

	if (direction == DMA_FROM_DEVICE)
		invalidate_dcache_range(start, end);
	else
		flush_dcache_range(start, end);

	return (dma_addr_t)(uintptr_t)ptr;
}

static inline void dma_unmap_single(void *dev, dma_addr_t dma_addr,
				    size_t size, int direction)
{
	unsigned long start = rounddown((unsigned long)dma_addr, ARCH_DMA_MINALIGN);
	unsigned long end = roundup((unsigned long)dma_addr + size,
				    ARCH_DMA_MINALIGN);

	if (direction != DMA_TO_DEVICE)
		invalidate_dcache_range(start, end);
}

static inline void dma_sync_single_for_device(void *dev, dma_addr_t dma_addr,
					      size_t size, int direction)
{
	unsigned long start = rounddown((unsigned long)dma_addr, ARCH_DMA_MINALIGN);
	unsigned long end = roundup((unsigned long)dma_addr + size,
				    ARCH_DMA_MINALIGN);

	if (direction == DMA_FROM_DEVICE)
		invalidate_dcache_range(start, end);
	else
		flush_dcache_range(start, end);
}

static inline void dma_sync_single_for_cpu(void *dev, dma_addr_t dma_addr,
					   size_t size, int direction)
{
	unsigned long start = rounddown((unsigned long)dma_addr, ARCH_DMA_MINALIGN);
	unsigned long end = roundup((unsigned long)dma_addr + size,
				    ARCH_DMA_MINALIGN);

	if (direction != DMA_TO_DEVICE)
		invalidate_dcache_range(start, end);
}

static inline int dma_mapping_error(void *dev, dma_addr_t dma_addr)
{
	return 0;
}

#endif /* __LINUX_COMPAT_H__ */
