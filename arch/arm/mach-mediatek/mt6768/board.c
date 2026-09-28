// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Roman Vivchar <rva333@protonmail.com>
 *
 * Common board support for the MediaTek MT6768 SoC.
 */

#include <dm.h>
#include <env.h>
#include <log.h>
#include <lmb.h>
#include <linux/sizes.h>

int board_init(void)
{
	struct udevice *dev;
	int ret;

	if (CONFIG_IS_ENABLED(USB_GADGET)) {
		ret = uclass_get_device(UCLASS_USB_GADGET_GENERIC, 0, &dev);
		if (ret) {
			pr_err("%s: Cannot find USB device\n", __func__);
			return ret;
		}
	}

	return 0;
}

int misc_init_r(void)
{
	int ret;
	phys_addr_t addr;

	if (env_get("kernel_addr_r"))
		return 0;

	/* alloc free memory for the decompressed kernel image */
	ret = lmb_alloc_mem(LMB_MEM_ALLOC_MAX, SZ_2M, &addr, SZ_64M, LMB_NONE);
	if (ret)
		return log_ret(ret);

	env_set_hex("kernel_addr_r", addr);

	return 0;
}
