// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Roman Vivchar <rva333@protonmail.com>
 */

#include <clk.h>
#include <dm.h>
#include <fdtdec.h>
#include <fdt_support.h>
#include <init.h>
#include <log.h>
#include <lmb.h>
#include <mapmem.h>
#include <ram.h>
#include <asm/arch/misc.h>
#include <asm/armv8/mmu.h>
#include <asm/sections.h>
#include <asm/system.h>
#include <dm/uclass.h>
#include <env.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/sizes.h>
#include <linux/string.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

#define MT6768_MAX_RMEM		32

struct memory_bank {
	phys_addr_t addr;
	phys_size_t size;
};

static struct memory_bank mt6768_banks[CONFIG_NR_DRAM_BANKS] __section(".data");
static int mt6768_n_banks __section(".data") = 0;
static struct memory_bank mt6768_reserved_memory[MT6768_MAX_RMEM] __section(".data");
static const char * mt6768_reserved_memory_names[MT6768_MAX_RMEM] __section(".data");
static int mt6768_n_reserved __section(".data") = 0;

static int mt6768_parse_lk_memory(const void *prev)
{
	int root = fdt_path_offset(prev, "/");
	if (root < 0)
		return log_ret(root);

	int memory = fdt_path_offset(prev, "/memory");
	if (memory < 0)
		return log_ret(memory);

	while (mt6768_n_banks < CONFIG_NR_DRAM_BANKS) {
		fdt_size_t size;
		fdt_addr_t addr = fdtdec_get_addr_size_auto_parent(prev, root, memory, "reg",
								   mt6768_n_banks, &size, false);

		if (addr == FDT_ADDR_T_NONE)
			break;

		log_debug("add %d bank @ 0x%llx\n", mt6768_n_banks, addr);

		mt6768_banks[mt6768_n_banks].addr = addr;
		mt6768_banks[mt6768_n_banks].size = size;
		mt6768_n_banks++;
	}

	return 0;
}

static int mt6768_parse_reserved(const void *prev)
{
	int node;
	int reserved_memory = fdt_path_offset(prev, "/reserved-memory");
	if (reserved_memory < 0)
		return log_ret(reserved_memory);

	fdt_for_each_subnode(node, prev, reserved_memory) {
		const char *name = fdt_get_name(prev, node, NULL);
		fdt_size_t size;

		if (mt6768_n_reserved == MT6768_MAX_RMEM) {
			log_warning("too many reserved memory nodes\n");
			return 0;
		}

		fdt_addr_t addr = fdtdec_get_addr_size_auto_parent(prev, reserved_memory, node,
								   "reg", 0, &size, false);
		if (addr == FDT_ADDR_T_NONE || !size) {
			log_debug("skip %s\n", name);
			continue;
		}

		mt6768_reserved_memory[mt6768_n_reserved].addr = addr;
		mt6768_reserved_memory[mt6768_n_reserved].size = size;
		mt6768_reserved_memory_names[mt6768_n_reserved] = name;
		mt6768_n_reserved++;

		log_debug("add reserved memory node %s @ 0x%llx (0x%llx)\n", name, addr, size);
	}

	return 0;
}

static int mt6768_reserve_memory(void *fdt)
{
	int reserved_memory = fdt_path_offset(fdt, "/reserved-memory");
	int root = fdt_path_offset(fdt, "/");
	int ret;

	if (root < 0)
		return log_ret(root);

	if (reserved_memory < 0) {
		reserved_memory = fdt_add_subnode(fdt, root, "reserved-memory");
		if (reserved_memory < 0)
			return log_ret(reserved_memory);

		ret = fdt_setprop_u32(fdt, reserved_memory, "#address-cells", 2);
		if (ret)
			return log_ret(ret);

		ret = fdt_setprop_u32(fdt, reserved_memory, "#size-cells", 2);
		if (ret)
			return log_ret(ret);

		ret = fdt_setprop_empty(fdt, reserved_memory, "ranges");
		if (ret)
			return log_ret(ret);
	}


	for (int i = 0; i < mt6768_n_reserved; i++) {
		int node = fdt_add_subnode(fdt, reserved_memory, mt6768_reserved_memory_names[i]);
		if (node == -FDT_ERR_EXISTS)
			continue;
		else if (node < 0)
			return log_ret(node);

		fdt32_t cells[4] = { cpu_to_fdt32(upper_32_bits(mt6768_reserved_memory[i].addr)),
				     cpu_to_fdt32(lower_32_bits(mt6768_reserved_memory[i].addr)),
				     cpu_to_fdt32(upper_32_bits(mt6768_reserved_memory[i].size)),
				     cpu_to_fdt32(lower_32_bits(mt6768_reserved_memory[i].size))};

		ret = fdt_setprop(fdt, node, "reg", &cells, sizeof(cells));
		if (ret)
			return log_ret(ret);

		ret = fdt_setprop_empty(fdt, node, "no-map");
		if (ret)
			return log_ret(ret);

		log_debug("added %s reserved memory node @ 0x%llx (0x%llx)\n",
			  mt6768_reserved_memory_names[i], mt6768_reserved_memory[i].addr,
			  mt6768_reserved_memory[i].size);
	}

	return 0;
}

static void mt6768_fix_mem_map(void)
{
	phys_addr_t top = 0;
	int i;

	for (i = 0; i < mt6768_n_banks; i++) {
		phys_addr_t end = mt6768_banks[i].addr + mt6768_banks[i].size;

		if (end > top)
			top = end;
	}
	if (!top)
		return;

	top = ALIGN(top, SZ_2M);

	if (!mem_map || top <= mem_map[0].phys)
		return;

	if (top > mem_map[0].phys + SZ_8G) {
		return;
	}

	if (top - mem_map[0].phys == mem_map[0].size)
		return;

	mem_map[0].size = top - mem_map[0].phys;
}

int dram_init(void)
{
	const void *prev = (const void *)get_prev_bl_fdt_addr();
	phys_size_t total = 0;
	phys_addr_t base = 0;
	int ret;

	ret = mt6768_parse_lk_memory(prev);
	if (ret)
		return log_ret(ret);

	if (mt6768_n_banks <= 0)
		return log_ret(-EINVAL);

	base = mt6768_banks[0].addr;
	for (int i = 0; i < mt6768_n_banks; i++)
		total += mt6768_banks[i].size;

	gd->ram_base = (unsigned long)base;
	gd->ram_size = total;

	return log_ret(mt6768_parse_reserved(prev));
}

int dram_init_banksize(void)
{
	if (mt6768_n_banks <= 0)
		return -EINVAL;
	
	u64 start[CONFIG_NR_DRAM_BANKS];
	u64 size[CONFIG_NR_DRAM_BANKS];
	int ret, i;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++)
		if (i < mt6768_n_banks) {
			gd->dram[i].start = mt6768_banks[i].addr;
			gd->dram[i].size = mt6768_banks[i].size;
			start[i] = mt6768_banks[i].addr;
			size[i] = mt6768_banks[i].size;
		} else {
			gd->dram[i].start = 0;
			gd->dram[i].size = 0;
			start[i] = 0;
			size[i] = 0;
		}

	mt6768_fix_mem_map();


	ret = fdt_fixup_memory_banks((void *)gd->fdt_blob, start, size, mt6768_n_banks);
	if (ret)
		return log_ret(ret);

	ret = mt6768_reserve_memory((void *)gd->fdt_blob);
	if (ret)
		return log_ret(ret);

	return 0;
}

phys_addr_t board_get_usable_ram_top(phys_size_t total_size)
{
	/* some peripherals with DMA like MUSB may get unhappy if U-Boot is > 4 GB */
	if (gd->ram_top > SZ_4G)
		return SZ_4G;

	return gd->ram_top;
}

int ft_system_setup(void *blob, struct bd_info *bd)
{
	u64 start[CONFIG_NR_DRAM_BANKS];
	u64 size[CONFIG_NR_DRAM_BANKS];
	int banks = 0;
	int ret, i;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		if (!gd->dram[i].size)
			break;
		start[i] = gd->dram[i].start;
		size[i] = gd->dram[i].size;
		banks++;
	}

	if (!banks)
		return log_ret(-ENOENT);

	ret = fdt_fixup_memory_banks(blob, start, size, banks);
	if (ret)
		return log_ret(ret);
	
	return mt6768_reserve_memory(blob);
}

void reset_cpu(void)
{
	psci_system_reset();
}

#define APMIXEDSYS_BASE		0x1000c000
#define AP_PLL_CON3		0x0c
#define PLLON_CON0		0x44
#define PLLON_CON1		0x48

#define INFRACFG_AO_BASE	0x10001000
#define PERI_BUS_DCM_CTRL	0x074
#define INFRA_CG1		BIT(2)
#define INFRA_TOPAXI_SI0_CTL	0x200
#define INFRA_CG0		BIT(31)

int mach_cpu_init(void)
{
	void __iomem *apmixed = (void __iomem *)APMIXEDSYS_BASE;
	void __iomem *infracfg = (void __iomem *)INFRACFG_AO_BASE;

	/* MPLL, ARM/CCIPLLs, MAINPLL set to HW mode, TDCLKSQ, CLKSQ1 */
	clrsetbits_le32(apmixed + AP_PLL_CON3, 0x1e, 0);
	clrsetbits_le32(apmixed + PLLON_CON0, ~0x01041041, 0);
	clrsetbits_le32(apmixed + PLLON_CON1, ~0x01041041, 0);

	/* Disable TOPAXI protection */
	setbits_le32(infracfg + INFRA_TOPAXI_SI0_CTL, INFRA_CG0);
	/* Disable DCM */
	clrbits_le32(infracfg + PERI_BUS_DCM_CTRL, INFRA_CG1);

	return 0;
}

int print_cpuinfo(void)
{
	printf("CPU:   MediaTek MT6768\n");
	return 0;
}
