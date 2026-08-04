// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#include <clk.h>
#include <dm/read.h>
#include <dm/of.h>
#include <dm/device_compat.h>

#include "mtk_disp.h"
#include "mtk_disp_color.h"

#define DISP_COLOR_CFG_MAIN			0x0400
#define DISP_COLOR_START_MT2701			0x0f00
#define DISP_COLOR_START_MT8167			0x0400
#define DISP_COLOR_START_MT8173			0x0c00
#define DISP_COLOR_START(comp)			((comp)->data->color_offset)
#define DISP_COLOR_WIDTH(comp)			(DISP_COLOR_START(comp) + 0x50)
#define DISP_COLOR_HEIGHT(comp)			(DISP_COLOR_START(comp) + 0x54)

#define COLOR_BYPASS_ALL			BIT(7)
#define COLOR_SEQ_SEL				BIT(13)

struct mtk_disp_color_data {
	unsigned int color_offset;
};

/*
 * struct mtk_disp_color - DISP_COLOR driver structure
 * @crtc: associated crtc to report irq events to
 * @data: platform colour driver data
 */
struct mtk_disp_color {
	struct clk				clk;
	void __iomem				*regs;
	const struct mtk_disp_color_data	*data;
};

void mtk_color_config(struct udevice *dev, struct display_timing *timing)
{
	struct mtk_disp_color *color = dev_get_priv(dev);
	u32 w = timing->hactive.typ;
	u32 h = timing->vactive.typ;

	mtk_ddp_write(w, color->regs, DISP_COLOR_WIDTH(color));
	mtk_ddp_write(h, color->regs, DISP_COLOR_HEIGHT(color));
}

void mtk_color_start(struct udevice *dev)
{
	struct mtk_disp_color *color = dev_get_priv(dev);

	writel(COLOR_BYPASS_ALL | COLOR_SEQ_SEL,
	       color->regs + DISP_COLOR_CFG_MAIN);
	writel(0x1, color->regs + DISP_COLOR_START(color));
}

static int mtk_disp_color_probe(struct udevice *dev)
{
	struct mtk_disp_color *priv = dev_get_priv(dev);
	int ret;

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret)
		return ret;

	priv->regs = dev_remap_addr(dev);
	if (IS_ERR(priv->regs)) {
		dev_err(dev, "failed to ioremap color\n");
		return PTR_ERR(priv->regs);
	}

	priv->data = (struct mtk_disp_color_data *)dev_get_driver_data(dev);

	ret = clk_prepare_enable(&priv->clk);
	if (ret)
		return ret;

	return 0;
}

static const struct mtk_disp_color_data mt2701_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT2701,
};

static const struct udevice_id mtk_disp_color_driver_dt_match[] = {
	{ .compatible = "mediatek,mt2701-disp-color", .data = (ulong)&mt2701_color_driver_data },
	{ }
};

U_BOOT_DRIVER(mtk_disp_color) = {
	.name	   = "mtk_disp_color",
	.id	   = UCLASS_MISC,
	.of_match  = mtk_disp_color_driver_dt_match,
	.probe	   = mtk_disp_color_probe,
	.priv_auto = sizeof(struct mtk_disp_color),
};
