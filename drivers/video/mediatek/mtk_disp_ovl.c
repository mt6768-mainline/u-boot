// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <asm/io.h>
#include <clk.h>
#include <dm/read.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <reset.h>
#include <video.h>

#include "mtk_disp.h"
#include "mtk_disp_ovl.h"

#define DISP_REG_OVL_INTEN					0x0004
#define OVL_FME_CPL_INT							BIT(1)
#define OVL_FME_UND_INT							BIT(2)
#define OVL_RDMA0_EOF_ABNORMAL_INT	BIT(5)
#define OVL_RDMA1_EOF_ABNORMAL_INT	BIT(6)
#define OVL_RDMA0_FIFO_UND_INT			BIT(9)
#define OVL_RDMA1_FIFO_UND_INT			BIT(10)

#define DISP_REG_OVL_INTSTA			0x0008
#define OVL_FME_UND							BIT(2)
#define OVL_RDMA0_EOF_ABNORMAL	BIT(5)
#define OVL_RDMA1_EOF_ABNORMAL	BIT(6)
#define OVL_RDMA0_FIFO_UND			BIT(9)
#define OVL_RDMA1_FIFO_UND			BIT(10)

#define DISP_REG_OVL_EN				0x000c
#define DISP_REG_OVL_RST			0x0014
#define DISP_REG_OVL_ROI_SIZE			0x0020
#define DISP_REG_OVL_DATAPATH_CON		0x0024
#define OVL_LAYER_SMI_ID_EN				BIT(0)
#define OVL_BGCLR_SEL_IN				BIT(2)
#define OVL_LAYER_AFBC_EN(n)				BIT(4+n)
#define DISP_REG_OVL_ROI_BGCLR			0x0028
#define DISP_REG_OVL_SRC_CON			0x002c
#define DISP_REG_OVL_CON(n)			(0x0030 + 0x20 * (n))
#define DISP_REG_OVL_SRC_SIZE(n)		(0x0038 + 0x20 * (n))
#define DISP_REG_OVL_OFFSET(n)			(0x003c + 0x20 * (n))
#define DISP_REG_OVL_PITCH_MSB(n)		(0x0040 + 0x20 * (n))
#define OVL_PITCH_MSB_2ND_SUBBUF			BIT(16)
#define DISP_REG_OVL_PITCH(n)			(0x0044 + 0x20 * (n))
#define OVL_CONST_BLEND					BIT(28)
#define DISP_REG_OVL_RDMA_CTRL(n)		(0x00c0 + 0x20 * (n))
#define DISP_REG_OVL_RDMA_GMC(n)		(0x00c8 + 0x20 * (n))
#define DISP_REG_OVL_ADDR_MT2701		0x0040
#define DISP_REG_OVL_CLRFMT_EXT			0x02d0
#define OVL_CON_CLRFMT_BIT_DEPTH_MASK(n)		(GENMASK(1, 0) << (4 * (n)))
#define OVL_CON_CLRFMT_BIT_DEPTH(depth, n)		((depth) << (4 * (n)))
#define OVL_CON_CLRFMT_8_BIT				(0)
#define OVL_CON_CLRFMT_10_BIT				(1)
#define DISP_REG_OVL_ADDR_MT8173		0x0f40
#define DISP_REG_OVL_ADDR(ovl, n)		((ovl)->data->addr + 0x20 * (n))
#define DISP_REG_OVL_HDR_ADDR(ovl, n)		((ovl)->data->addr + 0x20 * (n) + 0x04)
#define DISP_REG_OVL_HDR_PITCH(ovl, n)		((ovl)->data->addr + 0x20 * (n) + 0x08)

#define GMC_THRESHOLD_BITS	16
#define GMC_THRESHOLD_HIGH	((1 << GMC_THRESHOLD_BITS) / 4)
#define GMC_THRESHOLD_LOW	((1 << GMC_THRESHOLD_BITS) / 8)

#define OVL_CON_CLRFMT_MAN	BIT(23)
#define OVL_CON_BYTE_SWAP	BIT(24)

/* OVL_CON_RGB_SWAP works only if OVL_CON_CLRFMT_MAN is enabled */
#define OVL_CON_RGB_SWAP	BIT(25)

#define OVL_CON_CLRFMT_RGB	(1 << 12)
#define OVL_CON_CLRFMT_ARGB8888	(2 << 12)
#define OVL_CON_CLRFMT_RGBA8888	(3 << 12)
#define OVL_CON_CLRFMT_ABGR8888	(OVL_CON_CLRFMT_ARGB8888 | OVL_CON_BYTE_SWAP)
#define OVL_CON_CLRFMT_BGRA8888	(OVL_CON_CLRFMT_RGBA8888 | OVL_CON_BYTE_SWAP)
#define OVL_CON_CLRFMT_UYVY	(4 << 12)
#define OVL_CON_CLRFMT_YUYV	(5 << 12)
#define OVL_CON_MTX_YUV_TO_RGB	(6 << 16)
#define OVL_CON_CLRFMT_PARGB8888 ((3 << 12) | OVL_CON_CLRFMT_MAN)
#define OVL_CON_CLRFMT_PABGR8888 (OVL_CON_CLRFMT_PARGB8888 | OVL_CON_RGB_SWAP)
#define OVL_CON_CLRFMT_PBGRA8888 (OVL_CON_CLRFMT_PARGB8888 | OVL_CON_BYTE_SWAP)
#define OVL_CON_CLRFMT_PRGBA8888 (OVL_CON_CLRFMT_PABGR8888 | OVL_CON_BYTE_SWAP)
#define OVL_CON_CLRFMT_RGB565(ovl)	((ovl)->data->fmt_rgb565_is_0 ? \
					0 : OVL_CON_CLRFMT_RGB)
#define OVL_CON_CLRFMT_RGB888(ovl)	((ovl)->data->fmt_rgb565_is_0 ? \
					OVL_CON_CLRFMT_RGB : 0)
#define	OVL_CON_AEN		BIT(8)
#define	OVL_CON_ALPHA		0xff
#define	OVL_CON_VIRT_FLIP	BIT(9)
#define	OVL_CON_HORZ_FLIP	BIT(10)

#define OVL_COLOR_ALPHA		GENMASK(31, 24)

struct mtk_disp_ovl_data {
	unsigned int addr;
	unsigned int gmc_bits;
	unsigned int layer_nr;
	bool fmt_rgb565_is_0;
	bool smi_id_en;
	bool supports_afbc;
	const u32 blend_modes;
	const u32 *formats;
	size_t num_formats;
	bool supports_clrfmt_ext;
};

struct mtk_disp_ovl {
	void __iomem *regs;
	struct clk clk;
	const struct mtk_disp_ovl_data *data;
};

void mtk_ovl_start(struct udevice *dev)
{
	struct mtk_disp_ovl *ovl = dev_get_priv(dev);

	if (ovl->data->smi_id_en) {
		unsigned int reg;

		reg = readl(ovl->regs + DISP_REG_OVL_DATAPATH_CON);
		reg = reg | OVL_LAYER_SMI_ID_EN;
		writel_relaxed(reg, ovl->regs + DISP_REG_OVL_DATAPATH_CON);
	}
	writel_relaxed(0x1, ovl->regs + DISP_REG_OVL_EN);
}

void mtk_ovl_stop(struct udevice *dev)
{
	struct mtk_disp_ovl *ovl = dev_get_priv(dev);

	writel_relaxed(0x0, ovl->regs + DISP_REG_OVL_EN);
	if (ovl->data->smi_id_en) {
		unsigned int reg;

		reg = readl(ovl->regs + DISP_REG_OVL_DATAPATH_CON);
		reg = reg & ~OVL_LAYER_SMI_ID_EN;
		writel_relaxed(reg, ovl->regs + DISP_REG_OVL_DATAPATH_CON);
	}
}

static void mtk_ovl_set_bit_depth(struct udevice *dev, int idx)
{
	struct mtk_disp_ovl *ovl = dev_get_priv(dev);
	unsigned int bit_depth = OVL_CON_CLRFMT_8_BIT;

	if (!ovl->data->supports_clrfmt_ext)
		return;

	mtk_ddp_write_mask(OVL_CON_CLRFMT_BIT_DEPTH(bit_depth, idx),
			   ovl->regs, DISP_REG_OVL_CLRFMT_EXT,
			   OVL_CON_CLRFMT_BIT_DEPTH_MASK(idx));
}

void mtk_ovl_config(struct udevice *dev, struct display_timing *timing)
{
	struct mtk_disp_ovl *ovl = dev_get_priv(dev);
	u32 w = timing->hactive.typ;
	u32 h = timing->vactive.typ;

	if (w != 0 && h != 0)
		mtk_ddp_write_relaxed(h << 16 | w, ovl->regs, DISP_REG_OVL_ROI_SIZE);

	/*
	 * The background color must be opaque black (ARGB),
	 * otherwise the alpha blending will have no effect
	 */
	mtk_ddp_write_relaxed(OVL_COLOR_ALPHA, ovl->regs, DISP_REG_OVL_ROI_BGCLR);

	mtk_ddp_write(0x1, ovl->regs, DISP_REG_OVL_RST);
	mtk_ddp_write(0x0, ovl->regs, DISP_REG_OVL_RST);
}

static void mtk_ovl_layer_on(struct udevice *dev, unsigned int idx)
{
	unsigned int gmc_thrshd_l;
	unsigned int gmc_thrshd_h;
	unsigned int gmc_value;
	struct mtk_disp_ovl *ovl = dev_get_priv(dev);

	mtk_ddp_write(0x1, ovl->regs,
		      DISP_REG_OVL_RDMA_CTRL(idx));
	gmc_thrshd_l = GMC_THRESHOLD_LOW >>
		      (GMC_THRESHOLD_BITS - ovl->data->gmc_bits);
	gmc_thrshd_h = GMC_THRESHOLD_HIGH >>
		      (GMC_THRESHOLD_BITS - ovl->data->gmc_bits);
	if (ovl->data->gmc_bits == 10)
		gmc_value = gmc_thrshd_h | gmc_thrshd_h << 16;
	else
		gmc_value = gmc_thrshd_l | gmc_thrshd_l << 8 |
			    gmc_thrshd_h << 16 | gmc_thrshd_h << 24;
	mtk_ddp_write(gmc_value, ovl->regs, DISP_REG_OVL_RDMA_GMC(idx));
	mtk_ddp_write_mask(BIT(idx), ovl->regs, DISP_REG_OVL_SRC_CON, BIT(idx));
}

void mtk_ovl_layer_config(struct udevice *dev, struct video_uc_plat *plat, struct display_timing *timing)
{
	struct mtk_disp_ovl *ovl = dev_get_priv(dev);
	u32 con;
	u32 height = timing->vactive.typ;
	u32 width = timing->hactive.typ;
	u32 pitch = width * VNBYTES(VIDEO_BPP32);

	/* Always use XRGB8888, which is ARGB8888 in the hw */
	con = OVL_CON_CLRFMT_ARGB8888;
	/* Constant alpha value */
	con |= 0xff;

	/* CONST_BLD must be enabled for XRGB format */
	pitch |= OVL_CONST_BLEND;

	/* Only layer 0 is needed */
	mtk_ddp_write_relaxed(con, ovl->regs,
			      DISP_REG_OVL_CON(0));
	mtk_ddp_write_relaxed(pitch, ovl->regs, DISP_REG_OVL_PITCH(0));
	mtk_ddp_write_relaxed((height << 16) | width, ovl->regs,
			      DISP_REG_OVL_SRC_SIZE(0));
	mtk_ddp_write_relaxed(0x0, ovl->regs,
			      DISP_REG_OVL_OFFSET(0));
	mtk_ddp_write_relaxed(plat->base, ovl->regs,
			      DISP_REG_OVL_ADDR(ovl, 0));

	mtk_ovl_set_bit_depth(dev, 0);
	mtk_ovl_layer_on(dev, 0);
}

static int mtk_disp_ovl_probe(struct udevice *dev)
{
	struct mtk_disp_ovl *priv = dev_get_priv(dev);
	struct reset_ctl *rst;
	int ret;

	priv->regs = dev_remap_addr(dev);
	if (!priv->regs)
		return -EINVAL;

	priv->data = (struct mtk_disp_ovl_data *)dev_get_driver_data(dev);
	if (!priv->data)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret)
		return ret;
	
	ret = clk_enable(&priv->clk);
	if (ret)
		return ret;

	rst = devm_reset_control_get_optional(dev, NULL);
	if (IS_ERR(rst))
		return log_ret(PTR_ERR(rst));

	ret = reset_reset(rst, 1000);
	if (ret)
		return log_ret(ret);

	return 0;
}

static const struct mtk_disp_ovl_data mt6572_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_MT2701,
	.gmc_bits = 10,
	.layer_nr = 4,
	.fmt_rgb565_is_0 = false,
};

static const struct udevice_id mtk_disp_ovl_ids[] = {
	{ .compatible = "mediatek,mt6572-disp-ovl", .data = (ulong)&mt6572_ovl_driver_data },
	{ }
};

U_BOOT_DRIVER(mtk_disp_ovl) = {
	.name	   = "mtk_disp_ovl",
	.id	   = UCLASS_MISC,
	.of_match  = mtk_disp_ovl_ids,
	.probe	   = mtk_disp_ovl_probe,
	.priv_auto = sizeof(struct mtk_disp_ovl),
};
