// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <clk.h>
#include <dm/device_compat.h>
#include <dm/of.h>
#include <dm/read.h>
#include <video.h>

#include "mtk_disp.h"
#include "mtk_disp_rdma.h"

#define DISP_REG_RDMA_INT_ENABLE		0x0000
#define DISP_REG_RDMA_INT_STATUS		0x0004
#define RDMA_TARGET_LINE_INT				BIT(5)
#define RDMA_FIFO_UNDERFLOW_INT				BIT(4)
#define RDMA_EOF_ABNORMAL_INT				BIT(3)
#define RDMA_FRAME_END_INT				BIT(2)
#define RDMA_FRAME_START_INT				BIT(1)
#define RDMA_REG_UPDATE_INT				BIT(0)
#define DISP_REG_RDMA_GLOBAL_CON		0x0010
#define RDMA_ENGINE_EN					BIT(0)
#define RDMA_MODE_MEMORY				BIT(1)
#define DISP_REG_RDMA_SIZE_CON_0		0x0014
#define RDMA_MATRIX_ENABLE				BIT(17)
#define RDMA_MATRIX_INT_MTX_SEL				GENMASK(23, 20)
#define RDMA_MATRIX_INT_MTX_BT601_to_RGB		(6 << 20)
#define DISP_REG_RDMA_SIZE_CON_1		0x0018
#define DISP_REG_RDMA_TARGET_LINE		0x001c
#define DISP_RDMA_MEM_CON			0x0024

#define MEM_MODE_INPUT_FORMAT_RGB565_MT65XX		(0x004 << 4)
#define MEM_MODE_INPUT_FORMAT_RGB888_MT65XX		(0x008 << 4)

#define MEM_MODE_INPUT_FORMAT_RGB565			(0x000 << 4)
#define MEM_MODE_INPUT_FORMAT_RGB888			(0x001 << 4)
#define MEM_MODE_INPUT_FORMAT_RGBA8888			(0x002 << 4)
#define MEM_MODE_INPUT_FORMAT_ARGB8888			(0x003 << 4)
#define MEM_MODE_INPUT_FORMAT_UYVY			(0x004 << 4)
#define MEM_MODE_INPUT_FORMAT_YUYV			(0x005 << 4)
#define MEM_MODE_INPUT_SWAP				BIT(8)
#define DISP_RDMA_MEM_SRC_PITCH			0x002c
#define DISP_RDMA_MEM_GMC_SETTING_0		0x0030
#define DISP_REG_RDMA_FIFO_CON			0x0040
#define RDMA_FIFO_UNDERFLOW_EN				BIT(31)
#define RDMA_FIFO_PSEUDO_SIZE(bytes)			(((bytes) / 16) << 16)
#define RDMA_OUTPUT_VALID_FIFO_THRESHOLD(bytes)		((bytes) / 16)
#define RDMA_FIFO_SIZE(rdma)			((rdma)->data->fifo_size)
#define DISP_RDMA_MEM_START_ADDR		0x0f00

#define RDMA_MEM_GMC				0x40402020

struct mtk_disp_rdma_data {
	unsigned int fifo_size;
	const u32 *formats;
	size_t num_formats;
	bool broken_height;
};

/*
 * struct mtk_disp_rdma - DISP_RDMA driver structure
 * @data: local driver data
 */
struct mtk_disp_rdma {
	struct clk			clk;
	void __iomem			*regs;
	const struct mtk_disp_rdma_data	*data;
	u32				fifo_size;
};

static void rdma_update_bits(struct udevice *dev, unsigned int reg,
			     unsigned int mask, unsigned int val)
{
	struct mtk_disp_rdma *rdma = dev_get_priv(dev);
	unsigned int tmp = readl(rdma->regs + reg);

	tmp = (tmp & ~mask) | (val & mask);
	writel(tmp, rdma->regs + reg);
}

void mtk_rdma_start(struct udevice *dev)
{
	rdma_update_bits(dev, DISP_REG_RDMA_GLOBAL_CON, RDMA_ENGINE_EN,
			 RDMA_ENGINE_EN);
}

void mtk_rdma_stop(struct udevice *dev)
{
	rdma_update_bits(dev, DISP_REG_RDMA_GLOBAL_CON, RDMA_ENGINE_EN, 0);
}

void mtk_rdma_config(struct udevice *dev, struct display_timing *timing)
{
	struct mtk_disp_rdma *rdma = dev_get_priv(dev);
	u32 width = timing->hactive.typ;
	u32 height = timing->vactive.typ;
	u32 rdma_fifo_size, threshold, reg;

	/* Some SoCs require their default value after reset or 0 to make RDMA happy. */
	if (rdma->data->broken_height)
		height = 0;

	mtk_ddp_write_mask(width, rdma->regs, DISP_REG_RDMA_SIZE_CON_0, 0xfff);
	mtk_ddp_write_mask(height, rdma->regs, DISP_REG_RDMA_SIZE_CON_1, 0xfffff);

	if (rdma->fifo_size)
		rdma_fifo_size = rdma->fifo_size;
	else
		rdma_fifo_size = RDMA_FIFO_SIZE(rdma);

	/*
	 * Enable FIFO underflow since DSI and DPI can't be blocked.
	 * Keep the FIFO pseudo size reset default of 8 KiB. Set the
	 * output threshold to 70% of max fifo size to make sure the
	 * threhold will not overflow
	 */
	threshold = rdma_fifo_size * 7 / 10;
	reg = RDMA_FIFO_UNDERFLOW_EN |
	      RDMA_FIFO_PSEUDO_SIZE(rdma_fifo_size) |
	      RDMA_OUTPUT_VALID_FIFO_THRESHOLD(threshold);
	mtk_ddp_write(reg, rdma->regs, DISP_REG_RDMA_FIFO_CON);
}

void mtk_rdma_layer_config(struct udevice *dev, struct video_uc_plat *plat, struct display_timing *timing)
{
	struct mtk_disp_rdma *rdma = dev_get_priv(dev);
	u32 width = timing->hactive.typ;

	/* Always use XRGB8888, which is RGBA8888 in the hw */
	mtk_ddp_write_relaxed(MEM_MODE_INPUT_FORMAT_RGBA8888, rdma->regs, DISP_RDMA_MEM_CON);

	mtk_ddp_write_mask(0, rdma->regs, DISP_REG_RDMA_SIZE_CON_0, RDMA_MATRIX_ENABLE);
	mtk_ddp_write_relaxed(plat->base, rdma->regs, DISP_RDMA_MEM_START_ADDR);
	mtk_ddp_write_relaxed(width * VNBYTES(VIDEO_BPP32), rdma->regs, DISP_RDMA_MEM_SRC_PITCH);
	mtk_ddp_write(RDMA_MEM_GMC, rdma->regs, DISP_RDMA_MEM_GMC_SETTING_0);
	mtk_ddp_write_mask(RDMA_MODE_MEMORY, rdma->regs, DISP_REG_RDMA_GLOBAL_CON,
	                   RDMA_MODE_MEMORY);

}

static int mtk_disp_rdma_probe(struct udevice *dev)
{
	struct mtk_disp_rdma *priv = dev_get_priv(dev);
	int ret;

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret)
		return ret;

	priv->regs = dev_remap_addr(dev);
	if (IS_ERR(priv->regs)) {
		dev_err(dev, "failed to ioremap rdma\n");
		return PTR_ERR(priv->regs);
	}

	ret = ofnode_read_u32(dev_ofnode(dev), "mediatek,rdma-fifo-size", &priv->fifo_size);
	if (ret && (ret != -EINVAL)) {
		dev_err(dev, "Failed to get rdma fifo size\n");
		return ret;
	}

	ret = clk_prepare_enable(&priv->clk);
	if (ret)
		return ret;

	/* Disable and clear pending interrupts */
	writel(0x0, priv->regs + DISP_REG_RDMA_INT_ENABLE);
	writel(0x0, priv->regs + DISP_REG_RDMA_INT_STATUS);

	priv->data = (struct mtk_disp_rdma_data *)dev_get_driver_data(dev);

	return 0;
}

static const struct mtk_disp_rdma_data mt6572_rdma_driver_data = {
	.fifo_size = 3840,
	.broken_height = true,
};

static const struct udevice_id mtk_disp_rdma_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6572-disp-rdma",
	  .data = (ulong)&mt6572_rdma_driver_data},
	{ }
};

U_BOOT_DRIVER(mtk_disp_rdma) = {
	.name	   = "mtk_disp_rdma",
	.id	   = UCLASS_MISC,
	.of_match  = mtk_disp_rdma_driver_dt_match,
	.probe	   = mtk_disp_rdma_probe,
	.priv_auto = sizeof(struct mtk_disp_rdma),
};
