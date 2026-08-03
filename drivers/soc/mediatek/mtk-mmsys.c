// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 */

#include "power-domain.h"
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <dm/read.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <reset-uclass.h>
#include <soc/mediatek/mtk-mmsys.h>

#include "mtk-mmsys.h"
#include "mt6572-mmsys.h"

#define MMSYS_SW_RESET_PER_REG 32

static const struct mtk_mmsys_driver_data mt6572_mmsys_driver_data = {
	.clk_driver = "mt6572-mm",
	.routes = mt6572_mmsys_routing_table,
	.num_routes = ARRAY_SIZE(mt6572_mmsys_routing_table),
	.sw0_rst_offset = MT6572_MMSYS_SW_RST_B,
	.num_resets = 32,
};

struct mtk_mmsys {
	void __iomem *regs;
	const struct mtk_mmsys_driver_data *data;
};

static void mtk_mmsys_update_bits(struct mtk_mmsys *mmsys, u32 offset, u32 mask, u32 val)
{
	u32 tmp;

	tmp = readl_relaxed(mmsys->regs + offset);
	tmp = (tmp & ~mask) | (val & mask);
	writel_relaxed(tmp, mmsys->regs + offset);
}

void mtk_mmsys_ddp_connect(struct udevice *dev,
			   enum mtk_ddp_comp_id cur,
			   enum mtk_ddp_comp_id next)
{
	struct mtk_mmsys *mmsys = (struct mtk_mmsys *)dev_get_priv(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->routes;
	int i;

	for (i = 0; i < mmsys->data->num_routes; i++)
		if (cur == routes[i].from_comp && next == routes[i].to_comp)
			mtk_mmsys_update_bits(mmsys, routes[i].addr, routes[i].mask,
					      routes[i].val);
}
EXPORT_SYMBOL_GPL(mtk_mmsys_ddp_connect);

void mtk_mmsys_ddp_disconnect(struct udevice *dev,
			      enum mtk_ddp_comp_id cur,
			      enum mtk_ddp_comp_id next)
{
	struct mtk_mmsys *mmsys = (struct mtk_mmsys *)dev_get_priv(dev);
	const struct mtk_mmsys_routes *routes = mmsys->data->routes;
	int i;

	for (i = 0; i < mmsys->data->num_routes; i++)
		if (cur == routes[i].from_comp && next == routes[i].to_comp)
			mtk_mmsys_update_bits(mmsys, routes[i].addr, routes[i].mask, 0);
}
EXPORT_SYMBOL_GPL(mtk_mmsys_ddp_disconnect);

static int mtk_mmsys_bind(struct udevice *dev)
{
	struct mtk_mmsys_driver_data *data =
		(struct mtk_mmsys_driver_data *)dev_get_driver_data(dev);
	int ret;

	if (data && data->num_resets > 0) {
		ret = device_bind_driver_to_node(dev, "mtk_mmsys_reset",
						 "mmsys_reset", dev_ofnode(dev),
						 NULL);
		if (ret) {
			dev_err(dev, "Failed to bind reset driver: %d\n", ret);
			return ret;
		}
	}

	if (data && data->clk_driver) {
		ret = device_bind_driver_to_node(dev, data->clk_driver,
						 "mmsys_clk", dev_ofnode(dev),
						 NULL);
		if (ret) {
			dev_err(dev, "Failed to bind clock driver: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int mtk_mmsys_probe(struct udevice *dev)
{
	struct mtk_mmsys *mmsys = dev_get_priv(dev);
	struct power_domain pd;
	int ret;

	mmsys->regs = dev_remap_addr(dev);
	if (IS_ERR(mmsys->regs)) {
		ret = PTR_ERR(mmsys->regs);
		dev_err(dev, "Failed to ioremap mmsys registers: %d\n", ret);
		return ret;
	}

	ret = power_domain_get(dev, &pd);
	if (ret)
		return ret;

	ret = power_domain_on(&pd);
	if (ret)
		return ret;

	mmsys->data = (const struct mtk_mmsys_driver_data *)dev_get_driver_data(dev);

	return 0;
}

static int mtk_mmsys_reset_update(struct reset_ctl *rcdev, unsigned long id,
				  bool assert)
{
	struct mtk_mmsys *mmsys = dev_get_priv(rcdev->dev->parent);
	u32 offset;
	u32 reg;

	if (mmsys->data->rst_tb) {
		if (id >= mmsys->data->num_resets) {
			dev_err(rcdev->dev, "Invalid reset ID: %lu (>=%u)\n",
				id, mmsys->data->num_resets);
			return -EINVAL;
		}
		id = mmsys->data->rst_tb[id];
	}

	offset = (id / MMSYS_SW_RESET_PER_REG) * sizeof(u32);
	id = id % MMSYS_SW_RESET_PER_REG;
	reg = mmsys->data->sw0_rst_offset + offset;

	if (assert)
		mtk_mmsys_update_bits(mmsys, reg, BIT(id), 0);
	else
		mtk_mmsys_update_bits(mmsys, reg, BIT(id), BIT(id));

	return 0;
}

static int mtk_mmsys_reset_assert(struct reset_ctl *rcdev)
{
	return mtk_mmsys_reset_update(rcdev, rcdev->id, true);
}

static int mtk_mmsys_reset_deassert(struct reset_ctl *rcdev)
{
	return mtk_mmsys_reset_update(rcdev, rcdev->id, false);
}

static int mtk_mmsys_reset(struct reset_ctl *rcdev, unsigned long delay)
{
	int ret;

	ret = mtk_mmsys_reset_assert(rcdev);
	if (ret)
		return ret;

	udelay(1000);

	return mtk_mmsys_reset_deassert(rcdev);
}

const struct reset_ops mmsys_reset_ops = {
	.rst_assert = mtk_mmsys_reset_assert,
	.rst_deassert = mtk_mmsys_reset_deassert,
	.rst_reset = mtk_mmsys_reset,
};

static const struct udevice_id of_match_mtk_mmsys[] = {
	{ .compatible = "mediatek,mt6572-mmsys", .data = (ulong)&mt6572_mmsys_driver_data },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(mtk_mmsys) = {
	.name = "mtk_mmsys",
	.id = UCLASS_SYSCON,
	.of_match = of_match_mtk_mmsys,
	.bind = mtk_mmsys_bind,
	.probe = mtk_mmsys_probe,
	.priv_auto = sizeof(struct mtk_mmsys),
};

U_BOOT_DRIVER(mtk_mmsys_reset) = {
	.name = "mtk_mmsys_reset",
	.id = UCLASS_RESET,
	.ops = &mmsys_reset_ops,
};

MODULE_AUTHOR("Yongqiang Niu <yongqiang.niu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC MMSYS driver");
MODULE_LICENSE("GPL");
