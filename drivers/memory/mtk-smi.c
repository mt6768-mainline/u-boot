// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 */
#include "dm/read.h"
#include <clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <dm/of.h>
#include <dm/device_compat.h>
#include <dt-bindings/memory/mtk-memory-port.h>

/* SMI COMMON */
#define SMI_L1LEN			0x100
#define MT6572_SMI_L1LEN		0x200

#define MT6572_SMI_L1_ARB0		0x204
#define MT6572_SMI_L1_ARB1		0x208

#define SMI_L1_ARB			0x200
#define SMI_BUS_SEL			0x220
#define SMI_BUS_LARB_SHIFT(larbid)	((larbid) << 1)
/* All are MMU0 defaultly. Only specialize mmu1 here. */
#define F_MMU1_LARB(larbid)		(0x1 << SMI_BUS_LARB_SHIFT(larbid))

#define SMI_READ_FIFO_TH		0x230
#define SMI_M4U_TH			0x234
#define SMI_FIFO_TH1			0x238
#define SMI_FIFO_TH2			0x23c
#define SMI_DCM				0x300
#define SMI_DUMMY			0x444

/* SMI LARB */
#define SMI_LARB_STAT                   0x0
#define SMI_LARB_SLP_CON                0xc
#define SLP_PROT_EN                     BIT(0)
#define SLP_PROT_RDY                    BIT(16)

#define SMI_LARB_CMD_THRT_CON		0x24
#define SMI_LARB_THRT_RD_NU_LMT_MSK	GENMASK(7, 4)
#define SMI_LARB_THRT_RD_NU_LMT		(5 << 4)

#define SMI_LARB_SW_FLAG		0x40
#define SMI_LARB_SW_FLAG_1		0x1

#define SMI_LARB_BWFILTER_EN     0x60
#define SMI_LARB_OSTD_CTRL_EN    0x64

#define SMI_LARB_OSTDL_PORT		0x200
#define SMI_LARB_OSTDL_PORTx(id)	(SMI_LARB_OSTDL_PORT + (((id) & 0x1f) << 2))

#define SMI_LARB_FIFO_STAT0      0x600

/* Below are about mmu enable registers, they are different in SoCs */
/* gen1: mt2701 */
#define REG_SMI_SECUR_CON_BASE		0x5c0

/* every register control 8 port, register offset 0x4 */
#define REG_SMI_SECUR_CON_OFFSET(id)	(((id) >> 3) << 2)
#define REG_SMI_SECUR_CON_ADDR(id)	\
	(REG_SMI_SECUR_CON_BASE + REG_SMI_SECUR_CON_OFFSET(id))

/*
 * every port have 4 bit to control, bit[port + 3] control virtual or physical,
 * bit[port + 2 : port + 1] control the domain, bit[port] control the security
 * or non-security.
 */
#define SMI_SECUR_CON_VAL_MSK(id)	(~(0xf << (((id) & 0x7) << 2)))
#define SMI_SECUR_CON_VAL_VIRT(id)	BIT((((id) & 0x7) << 2) + 3)
/* mt2701 domain should be set to 3 */
#define SMI_SECUR_CON_VAL_DOMAIN(id)	(0x3 << ((((id) & 0x7) << 2) + 1))

/* gen2: */
/* mt8167 */
#define MT8167_SMI_LARB_MMU_EN		0xfc0

/* mt8173 */
#define MT8173_SMI_LARB_MMU_EN		0xf00

/* general */
#define SMI_LARB_NONSEC_CON(id)		(0x380 + ((id) * 4))
#define F_MMU_EN			BIT(0)
#define BANK_SEL(id)			({		\
	u32 _id = (id) & 0x3;				\
	(_id << 8 | _id << 10 | _id << 12 | _id << 14);	\
})

#define SMI_COMMON_INIT_REGS_NR		6
#define SMI_LARB_PORT_NR_MAX		32

#define MTK_SMI_FLAG_THRT_UPDATE	BIT(0)
#define MTK_SMI_FLAG_SW_FLAG		BIT(1)
#define MTK_SMI_FLAG_SLEEP_CTL		BIT(2)
#define MTK_SMI_FLAG_CFG_PORT_SEC_CTL	BIT(3)
#define MTK_SMI_FLAG_BW_CALIBRATE     BIT(4)
#define MTK_SMI_CAPS(flags, _x)		(!!((flags) & (_x)))

struct mtk_smi_reg_pair {
	unsigned int		offset;
	u32			value;
};

enum mtk_smi_type {
	MTK_SMI_MT65XX,
	MTK_SMI_GEN1,
	MTK_SMI_GEN2,		/* gen2 smi common */
};

struct mtk_smi_common_plat {
	enum mtk_smi_type	type;
	bool			has_gals;
	u32			bus_sel; /* Balance some larbs to enter mmu0 or mmu1 */

	const struct mtk_smi_reg_pair	*init;
};

struct mtk_smi_larb_gen {
	int				(*config_port)(struct udevice *dev);
	unsigned int			larb_direct_to_common_mask;
	unsigned int			flags_general;
	const u8			(*ostd)[SMI_LARB_PORT_NR_MAX];
};

struct mtk_smi {
	struct clk_bulk		clks;
	void __iomem		*smi_ao_base;
	void __iomem		*base;
	const struct mtk_smi_common_plat *plat;
};

struct mtk_smi_larb { /* larb: local arbiter */
	void __iomem			*base;
	struct udevice			*smi_common_dev; /* common or sub-common dev */
	const struct mtk_smi_larb_gen	*larb_gen;
	int				larbid;
	u32				*mmu;
	struct clk			clk;
};

static int mtk_smi_larb_config_port_gen0(struct udevice *dev)
{
	struct mtk_smi_larb *larb = dev_get_priv(dev);
	const struct mtk_smi_larb_gen *larb_gen = larb->larb_gen;
	const u8 *larbostd = larb_gen->ostd ? larb_gen->ostd[larb->larbid] :
					      NULL;
	u32 i, reg_val, tmp;
	int ret;

	/* gen 2 part */
	for (i = 0; i < SMI_LARB_PORT_NR_MAX && larbostd && !!larbostd[i]; i++)
		writel_relaxed(larbostd[i],
			       larb->base + SMI_LARB_OSTDL_PORTx(i));

	/* some gen 0 SoCs need BW calibration */
	if (MTK_SMI_CAPS(larb_gen->flags_general, MTK_SMI_FLAG_BW_CALIBRATE)) {
		reg_val = readl_relaxed(larb->base + SMI_LARB_STAT);
		if (!reg_val) {
			writel_relaxed(0xffffffff,
				       larb->base + SMI_LARB_BWFILTER_EN);

			ret = readl_poll_timeout(
				larb->base + SMI_LARB_FIFO_STAT0, tmp,
				tmp == 0xaaaa, 64 * 500);
			if (ret)
				dev_warn(dev,
					 "BW limiter calibration timeout\n");

			writel_relaxed(0, larb->base + SMI_LARB_BWFILTER_EN);
			writel_relaxed(0xffffffff,
				       larb->base + SMI_LARB_OSTD_CTRL_EN);
		}
	}

	return 0;
}

static const u8 mtk_smi_larb_mt6572_ostd[][SMI_LARB_PORT_NR_MAX] = {
  [0] = {0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
  	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f},
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt6572 = {
	.config_port = mtk_smi_larb_config_port_gen0,
	.flags_general = MTK_SMI_FLAG_BW_CALIBRATE,
	.ostd = mtk_smi_larb_mt6572_ostd,
};

static const struct udevice_id mtk_smi_larb_of_ids[] = {
	{.compatible = "mediatek,mt6572-smi-larb", .data = (ulong)&mtk_smi_larb_mt6572},
	{}
};

static int mtk_smi_larb_probe(struct udevice *dev)
{
	struct mtk_smi_larb *larb = dev_get_priv(dev);
	int ret;

	larb->larb_gen =
		(const struct mtk_smi_larb_gen *)dev_get_driver_data(dev);

	larb->base = dev_remap_addr(dev);
	if (IS_ERR(larb->base))
		return PTR_ERR(larb->base);

	dev_read_u32(dev, "mediatek,larb-id", &larb->larbid);

	/* Make sure SMI common is probed before any larb */
	ret = uclass_get_device_by_phandle(UCLASS_MEMORY, dev, "mediatek,smi",
					   &larb->smi_common_dev);
	if (ret) {
		dev_err(dev, "Failed to get smi common device: %d\n", ret);
		return ret;
	}

	ret = clk_get_by_index(dev, 0, &larb->clk);
	if (ret)
		return ret;

	ret = clk_enable(&larb->clk);
	if (ret)
		return ret;

	if (larb->larb_gen->config_port) {
		ret = larb->larb_gen->config_port(dev);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct mtk_smi_reg_pair mtk_smi_common_mt6572_init[SMI_COMMON_INIT_REGS_NR] = {
	{MT6572_SMI_L1LEN, 0x3},
	{MT6572_SMI_L1_ARB0, 0},
	{MT6572_SMI_L1_ARB1, 0},
	{SMI_M4U_TH, 0x29ca7},
	{SMI_READ_FIFO_TH, 0x320},
};

static const struct mtk_smi_common_plat mtk_smi_common_mt6572 = {
	.type     = MTK_SMI_MT65XX,
	.init     = mtk_smi_common_mt6572_init,
};

static const struct udevice_id mtk_smi_common_of_ids[] = {
	{.compatible = "mediatek,mt6572-smi-common", .data = (ulong)&mtk_smi_common_mt6572},
	{}
};

static int mtk_smi_common_probe(struct udevice *dev)
{
	struct mtk_smi *common = dev_get_priv(dev);
	const struct mtk_smi_reg_pair *init;
	int i, ret;

	common->plat = (const struct mtk_smi_common_plat *)dev_get_driver_data(dev);

	ret = clk_get_bulk(dev, &common->clks);
	if (ret)
		return ret;

	ret = clk_enable_bulk(&common->clks);
	if (ret)
		return ret;

	switch (common->plat->type) {
		case MTK_SMI_MT65XX:
			/*
			 * gen 0 uses 2 mmio ranges: ao base for iommu configuration,
			 * and ext base for ostd, fifo and bw limiter setup
			 */
			common->smi_ao_base = dev_remap_addr_index(dev, 0);
			if (IS_ERR(common->smi_ao_base))
				return PTR_ERR(common->smi_ao_base);
			common->base = dev_remap_addr_index(dev, 1);
			if (IS_ERR(common->base))
				return PTR_ERR(common->base);
			break;
		case MTK_SMI_GEN1:
		case MTK_SMI_GEN2:
			return -EINVAL;
	}

	init = common->plat->init;
	for (i = 0; i < SMI_COMMON_INIT_REGS_NR && init && init[i].offset; i++)
		writel_relaxed(init[i].value, common->base + init[i].offset);

	return 0;
}

U_BOOT_DRIVER(mtk_smi_larb) = {
	.name = "mtk_smi_larb",
	.id = UCLASS_MEMORY,
	.of_match = mtk_smi_larb_of_ids,
	.probe = mtk_smi_larb_probe,
	.priv_auto = sizeof(struct mtk_smi_larb),
};

U_BOOT_DRIVER(mtk_smi_common) = {
	.name = "mtk_smi_common",
	.id = UCLASS_MEMORY,
	.of_match = mtk_smi_common_of_ids,
	.probe = mtk_smi_common_probe,
	.priv_auto = sizeof(struct mtk_smi),
};

MODULE_DESCRIPTION("MediaTek SMI driver");
MODULE_LICENSE("GPL v2");
