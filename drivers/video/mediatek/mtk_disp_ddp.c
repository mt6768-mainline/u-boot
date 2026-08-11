// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <clk.h>
#include <dm/device_compat.h>
#include <dm/of.h>
#include <dm/read.h>
#include <linux/bitfield.h>

#include "mtk_disp.h"
#include "mtk_disp_ddp.h"

#define DISP_CCORR_EN				0x0000
#define CCORR_EN					BIT(0)
#define DISP_CCORR_CFG				0x0020
#define CCORR_ENGINE_EN					BIT(1)
#define DISP_CCORR_SIZE				0x0030

#define DISP_AAL_EN				0x0000
#define AAL_EN						BIT(0)
#define DISP_AAL_SIZE				0x0030
#define DISP_AAL_SIZE_HSIZE				GENMASK(28, 16)
#define DISP_AAL_SIZE_VSIZE				GENMASK(12, 0)
#define DISP_AAL_OUTPUT_SIZE			0x04d8

#define DISP_GAMMA_EN				0x0000
#define GAMMA_EN					BIT(0)
#define DISP_GAMMA_SIZE				0x0030
#define DISP_GAMMA_SIZE_HSIZE				GENMASK(28, 16)
#define DISP_GAMMA_SIZE_VSIZE				GENMASK(12, 0)

#define DISP_DITHER_EN				0x0000
#define DITHER_EN					BIT(0)
#define DISP_DITHER_CFG				0x0020
#define DITHER_RELAY_MODE				BIT(0)
#define DISP_DITHER_SIZE			0x0030

#define DISP_UFO_START				0x0000
#define UFO_BYPASS					BIT(2)

struct mtk_disp_ddp {
	struct clk	clk;
	void __iomem	*regs;
};

void mtk_ccorr_config(struct udevice *dev, struct display_timing *timing)
{
	struct mtk_disp_ddp *priv = dev_get_priv(dev);

	mtk_ddp_write(timing->hactive.typ << 16 | timing->vactive.typ,
		      priv->regs, DISP_CCORR_SIZE);
	mtk_ddp_write(CCORR_ENGINE_EN, priv->regs, DISP_CCORR_CFG);
}

void mtk_ccorr_start(struct udevice *dev)
{
	struct mtk_disp_ddp *priv = dev_get_priv(dev);

	mtk_ddp_write(CCORR_EN, priv->regs, DISP_CCORR_EN);
}

void mtk_aal_config(struct udevice *dev, struct display_timing *timing)
{
	struct mtk_disp_ddp *priv = dev_get_priv(dev);
	u32 sz;

	sz = FIELD_PREP(DISP_AAL_SIZE_HSIZE, timing->hactive.typ);
	sz |= FIELD_PREP(DISP_AAL_SIZE_VSIZE, timing->vactive.typ);

	mtk_ddp_write(sz, priv->regs, DISP_AAL_SIZE);
	mtk_ddp_write(sz, priv->regs, DISP_AAL_OUTPUT_SIZE);
}

void mtk_aal_start(struct udevice *dev)
{
	struct mtk_disp_ddp *priv = dev_get_priv(dev);

	mtk_ddp_write(AAL_EN, priv->regs, DISP_AAL_EN);
}

void mtk_gamma_config(struct udevice *dev, struct display_timing *timing)
{
	struct mtk_disp_ddp *priv = dev_get_priv(dev);
	u32 sz;

	sz = FIELD_PREP(DISP_GAMMA_SIZE_HSIZE, timing->hactive.typ);
	sz |= FIELD_PREP(DISP_GAMMA_SIZE_VSIZE, timing->vactive.typ);

	mtk_ddp_write(sz, priv->regs, DISP_GAMMA_SIZE);
}

void mtk_gamma_start(struct udevice *dev)
{
	struct mtk_disp_ddp *priv = dev_get_priv(dev);

	mtk_ddp_write(GAMMA_EN, priv->regs, DISP_GAMMA_EN);
}

void mtk_dither_config(struct udevice *dev, struct display_timing *timing)
{
	struct mtk_disp_ddp *priv = dev_get_priv(dev);

	mtk_ddp_write(timing->hactive.typ << 16 | timing->vactive.typ,
		      priv->regs, DISP_DITHER_SIZE);

	mtk_ddp_write(DITHER_RELAY_MODE, priv->regs, DISP_DITHER_CFG);
}

void mtk_dither_start(struct udevice *dev)
{
	struct mtk_disp_ddp *priv = dev_get_priv(dev);

	mtk_ddp_write(DITHER_EN, priv->regs, DISP_DITHER_EN);
}

void mtk_ufoe_start(struct udevice *dev)
{
	struct mtk_disp_ddp *priv = dev_get_priv(dev);

	mtk_ddp_write(UFO_BYPASS, priv->regs, DISP_UFO_START);
}

static int mtk_disp_ddp_probe(struct udevice *dev)
{
	struct mtk_disp_ddp *priv = dev_get_priv(dev);
	int ret;

	priv->regs = dev_remap_addr(dev);
	if (IS_ERR(priv->regs)) {
		dev_err(dev, "failed to ioremap registers\n");
		return PTR_ERR(priv->regs);
	}

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret)
		return ret;

	return clk_enable(&priv->clk);
}

static const struct udevice_id mtk_disp_ccorr_ids[] = {
	{ .compatible = "mediatek,mt8192-disp-ccorr" },
	{ }
};

static const struct udevice_id mtk_disp_aal_ids[] = {
	{ .compatible = "mediatek,mt8183-disp-aal" },
	{ }
};

static const struct udevice_id mtk_disp_gamma_ids[] = {
	{ .compatible = "mediatek,mt8163-disp-gamma" },
	{ }
};

static const struct udevice_id mtk_disp_dither_ids[] = {
	{ .compatible = "mediatek,mt8183-disp-dither" },
	{ }
};

static const struct udevice_id mtk_disp_ufoe_ids[] = {
	{ .compatible = "mediatek,mt8173-disp-ufoe" },
	{ }
};

U_BOOT_DRIVER(mtk_disp_ccorr) = {
	.name	   = "mtk_disp_ccorr",
	.id	   = UCLASS_MISC,
	.of_match  = mtk_disp_ccorr_ids,
	.probe	   = mtk_disp_ddp_probe,
	.priv_auto = sizeof(struct mtk_disp_ddp),
};

U_BOOT_DRIVER(mtk_disp_aal) = {
	.name	   = "mtk_disp_aal",
	.id	   = UCLASS_MISC,
	.of_match  = mtk_disp_aal_ids,
	.probe	   = mtk_disp_ddp_probe,
	.priv_auto = sizeof(struct mtk_disp_ddp),
};

U_BOOT_DRIVER(mtk_disp_gamma) = {
	.name	   = "mtk_disp_gamma",
	.id	   = UCLASS_MISC,
	.of_match  = mtk_disp_gamma_ids,
	.probe	   = mtk_disp_ddp_probe,
	.priv_auto = sizeof(struct mtk_disp_ddp),
};

U_BOOT_DRIVER(mtk_disp_dither) = {
	.name	   = "mtk_disp_dither",
	.id	   = UCLASS_MISC,
	.of_match  = mtk_disp_dither_ids,
	.probe	   = mtk_disp_ddp_probe,
	.priv_auto = sizeof(struct mtk_disp_ddp),
};

U_BOOT_DRIVER(mtk_disp_ufoe) = {
	.name	   = "mtk_disp_ufoe",
	.id	   = UCLASS_MISC,
	.of_match  = mtk_disp_ufoe_ids,
	.probe	   = mtk_disp_ddp_probe,
	.priv_auto = sizeof(struct mtk_disp_ddp),
};
