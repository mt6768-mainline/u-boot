// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2026 Roman Vivchar <rva333@protonmail.com> */

#include <asm/unaligned.h>
#include <linux/build_bug.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sizes.h>
#include <linux/string.h>

#include "preloader.h"

#define BOOT_TAG_MEM			0x88610005
#define MBLOCK_MAGIC			0x99999999
#define MBLOCK_VERSION			2
#define MT6768_DRAM_BASE		0x40000000ULL
#define MT6768_DRAM_LIMIT		(MT6768_DRAM_BASE + SZ_8G)
#define MT6768_MAX_BOOTARGS_SIZE	SZ_64K

struct pl_range {
	u64 start;
	u64 size;
} __packed;

struct pl_mblock {
	struct pl_range range;
	u32 rank;
	u32 padding;
} __packed;

struct pl_reserved {
	struct pl_range range;
	u32 mapping;
	char name[MT6768_RMEM_NAME_LEN];
	u32 padding;
} __packed;

struct pl_mblock_info {
	u32 count;
	u32 padding;
	struct pl_mblock blocks[MT6768_MAX_MBLOCKS];
	u32 magic;
	u32 version;
	u32 reserved_count;
	u32 reserved_padding;
	struct pl_reserved reserved[MT6768_MAX_MBLOCKS];
} __packed;

struct pl_dram_info {
	u32 count;
	u32 padding;
	struct pl_range ranks[4];
} __packed;

struct pl_memory_tag {
	u32 rank_count;
	u32 padding;
	u64 rank_size[4];
	struct pl_mblock_info mblocks;
	struct pl_dram_info dram;
	struct pl_range lca;
	struct pl_range tee;
} __packed;

static bool valid_range(u64 addr, u64 size)
{
	return size && addr >= MT6768_DRAM_BASE &&
	       addr < MT6768_DRAM_LIMIT && size <= MT6768_DRAM_LIMIT - addr;
}

static bool in_dram(const struct mt6768_dram_info *info, u64 addr, u64 size)
{
	unsigned int i;

	if (!valid_range(addr, size))
		return false;

	for (i = 0; i < info->n_banks; i++) {
		const struct mt6768_memory_bank *bank = &info->banks[i];

		if (addr >= bank->addr && addr + size <= bank->addr + bank->size)
			return true;
	}

	return false;
}

static int add_reserved(struct mt6768_dram_info *info, u64 addr, u64 size,
			const char *name, bool no_map)
{
	struct mt6768_reservation *r;
	unsigned int i;

	if (!size)
		return 0;
	if (!in_dram(info, addr, size))
		return -EINVAL;

	for (i = 0; i < info->n_reserved; i++) {
		r = &info->reserved[i];
		if (r->addr == addr && r->size == size) {
			r->no_map |= no_map;
			return 0;
		}
	}

	if (info->n_reserved == MT6768_MAX_RMEM)
		return -ENOSPC;

	r = &info->reserved[info->n_reserved++];
	r->addr = addr;
	r->size = size;
	r->no_map = no_map;

	strlcpy(r->name, name, sizeof(r->name));

	log_debug("add %s @ 0x%llx for 0x%llx\n", r->name, r->addr, r->size);

	return 0;
}

static int parse_memory(const struct pl_memory_tag *mem,
			struct mt6768_dram_info *info)
{
	const struct pl_mblock_info *blocks = &mem->mblocks;
	u32 ranks = get_unaligned_le32(&mem->dram.count);
	u32 reserved = get_unaligned_le32(&blocks->reserved_count);
	u64 previous_end = 0;
	unsigned int i;
	int ret;

	if (!ranks || ranks > ARRAY_SIZE(mem->dram.ranks) ||
	    reserved > MT6768_MAX_MBLOCKS)
		return -EINVAL;
	if (get_unaligned_le32(&blocks->magic) != MBLOCK_MAGIC ||
	    get_unaligned_le32(&blocks->version) != MBLOCK_VERSION)
		return -ENOTSUPP;

	for (i = 0; i < ranks; i++) {
		u64 addr = get_unaligned_le64(&mem->dram.ranks[i].start);
		u64 size = get_unaligned_le64(&mem->dram.ranks[i].size);
		struct mt6768_memory_bank *bank;

		if (!valid_range(addr, size) || addr < previous_end)
			return -EINVAL;

		if (info->n_banks && addr == previous_end) {
			bank = &info->banks[info->n_banks - 1];
			bank->size += size;
		} else {
			if (info->n_banks == CONFIG_NR_DRAM_BANKS)
				return -ENOSPC;
			bank = &info->banks[info->n_banks++];
			bank->addr = addr;
			bank->size = size;
		}
		previous_end = addr + size;
	}

	for (i = 0; i < reserved; i++) {
		const struct pl_reserved *r = &blocks->reserved[i];

		ret = add_reserved(info, get_unaligned_le64(&r->range.start),
				   get_unaligned_le64(&r->range.size), r->name,
				   !get_unaligned_le32(&r->mapping));
		if (ret)
			return ret;
	}

	ret = add_reserved(info, get_unaligned_le64(&mem->lca.start),
			   get_unaligned_le64(&mem->lca.size), "preloader-lca", true);
	if (ret)
		return ret;

	return add_reserved(info, get_unaligned_le64(&mem->tee.start),
			    get_unaligned_le64(&mem->tee.size), "preloader-tee", true);
}

int mt6768_parse_preloader(const void *args, u64 size,
			   struct mt6768_dram_info *info)
{
	const u8 *tag = args;

	memset(info, 0, sizeof(*info));
	if (!args || size < 8 || size > MT6768_MAX_BOOTARGS_SIZE)
		return -EINVAL;

	while (size >= 8) {
		u32 words = get_unaligned_le32(tag);
		u32 type = get_unaligned_le32(tag + 4);
		u64 bytes = (u64)words * sizeof(u32);

		if (!words)
			return -ENOENT;
		if (words < 2 || bytes > size)
			return -EINVAL;
		if (type == BOOT_TAG_MEM) {
			if (bytes < 8 + sizeof(struct pl_memory_tag))
				return -EINVAL;
			return parse_memory((const void *)(tag + 8), info);
		}
		tag += bytes;
		size -= bytes;
	}

	return -ENOENT;
}
