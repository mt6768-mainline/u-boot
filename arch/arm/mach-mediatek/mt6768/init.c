// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Roman Vivchar <rva333@protonmail.com>
 */

#include <clk.h>
#include <dm.h>
#include <fdtdec.h>
#include <ram.h>
#include <asm/arch/misc.h>
#include <asm/armv8/mmu.h>
#include <asm/sections.h>
#include <asm/system.h>
#include <dm/uclass.h>

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}

void reset_cpu(void)
{
	psci_system_reset();
}

int print_cpuinfo(void)
{
	printf("CPU:   MediaTek MT6768\n");
	return 0;
}

