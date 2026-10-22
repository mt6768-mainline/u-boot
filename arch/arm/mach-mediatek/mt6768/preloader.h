/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (C) 2026 Roman Vivchar <rva333@protonmail.com> */

#ifndef __MT6768_PRELOADER_H
#define __MT6768_PRELOADER_H

#include <linux/types.h>

#define MT6768_MAX_MBLOCKS	128
#define MT6768_RMEM_NAME_LEN	128
#define MT6768_MAX_RMEM		64

struct mt6768_memory_bank {
	u64 addr;
	u64 size;
};

struct mt6768_reservation {
	u64 addr;
	u64 size;
	bool no_map;
	char name[MT6768_RMEM_NAME_LEN];
};

struct mt6768_dram_info {
	struct mt6768_memory_bank banks[CONFIG_NR_DRAM_BANKS];
	struct mt6768_reservation reserved[MT6768_MAX_RMEM];
	unsigned int n_banks;
	unsigned int n_reserved;
};

int mt6768_parse_preloader(const void *args, u64 size,
			   struct mt6768_dram_info *info);

#endif
