// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2026 Roman Vivchar <rva333@protonmail.com> */

#include <fdtdec.h>
#include <init.h>
#include <lmb.h>
#include <log.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sizes.h>

#include "memory.h"

DECLARE_GLOBAL_DATA_PTR;

struct mt6768_dram_info mt6768_memory __section(".data");

int mt6768_load_bootargs(const void *args)
{
	ulong addr = (ulong)args;
	u64 limit;
	int ret;

	if (addr < CONFIG_MTK_MEM_MAP_DDR_BASE_PHY || addr > U32_MAX ||
	    !IS_ALIGNED(addr, sizeof(u32)))
		return log_ret(-EINVAL);

	limit = min((u64)SZ_64K, (u64)SZ_4G - addr);
	ret = mt6768_parse_preloader(args, limit, &mt6768_memory);
	if (ret)
		return log_ret(ret);

	log_info("DRAM: preloader tags at %lx, %u reserved memory entries\n", addr,
		 mt6768_memory.n_reserved);

	return 0;
}

void lmb_arch_add_memory(void)
{
	u64 ram_top = gd->ram_top ? gd->ram_top : 0x100000000ULL;
	unsigned int i;

	/* 1. Add physical DRAM banks to LMB */
	for (i = 0; i < mt6768_memory.n_banks; i++) {
		const struct mt6768_memory_bank *bank = &mt6768_memory.banks[i];

		if (bank->size && lmb_add(bank->addr, bank->size))
			panic("Failed to add DRAM bank to LMB\n");
	}

	/* 2. Explicitly reserve all preloader/LK firmware reservations below ram_top */
	for (i = 0; i < mt6768_memory.n_reserved; i++) {
		phys_addr_t addr = mt6768_memory.reserved[i].addr;
		phys_size_t size = mt6768_memory.reserved[i].size;
		u32 flags = mt6768_memory.reserved[i].no_map ? LMB_NOMAP : LMB_NOOVERWRITE;

		if (addr >= ram_top)
			continue;
		if (addr + size > ram_top)
			size = ram_top - addr;

		if (lmb_alloc_mem(LMB_MEM_ALLOC_ADDR, 0, &addr, size, flags))
			log_warning("Failed to reserve %s @ 0x%llx (0x%llx)\n",
				    mt6768_memory.reserved[i].name,
				    (u64)addr, (u64)size);
	}

	/* 3. Limit DMA below ram_top if configured */
	if (IS_ENABLED(CONFIG_LMB_LIMIT_DMA_BELOW_RAM_TOP)) {
		for (i = 0; i < mt6768_memory.n_banks; i++) {
			phys_addr_t start = mt6768_memory.banks[i].addr;
			phys_size_t size = mt6768_memory.banks[i].size;
			phys_addr_t end = start + size;

			if (start >= ram_top) {
				phys_addr_t addr = start;

				lmb_alloc_mem(LMB_MEM_ALLOC_ADDR, 0, &addr, size, LMB_NOOVERWRITE);
			} else if (end > ram_top) {
				phys_addr_t addr = ram_top;

				lmb_alloc_mem(LMB_MEM_ALLOC_ADDR, 0, &addr, end - ram_top, LMB_NOOVERWRITE);
			}
		}
	}
}
