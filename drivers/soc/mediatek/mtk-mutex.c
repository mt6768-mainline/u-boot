// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <clk.h>
#include <linux/iopoll.h>
#include <dm/of.h>
#include <linux/regmap.h>
#include <soc/mediatek/mtk-mmsys.h>
#include <soc/mediatek/mtk-mutex.h>
#include <dm/device_compat.h>
#include <linux/bug.h>
#include <dm/read.h>

#define MTK_MUTEX_MAX_HANDLES			10

#define MT2701_MUTEX0_MOD0			0x2c
#define MT2701_MUTEX0_SOF0			0x30
#define MT2701_MUTEX0_MOD1			0x34

#define MT8183_MUTEX0_MOD0			0x30
#define MT8183_MUTEX0_MOD1			0x34
#define MT8183_MUTEX0_SOF0			0x2c

#define DISP_REG_MUTEX_EN(n)			(0x20 + 0x20 * (n))
#define DISP_REG_MUTEX(n)			(0x24 + 0x20 * (n))
#define DISP_REG_MUTEX_RST(n)			(0x28 + 0x20 * (n))
/*
 * Some SoCs may have multiple MUTEX_MOD registers as more than 32 mods
 * are present, hence requiring multiple 32-bits registers.
 *
 * The mutex_table_mod fully represents that by defining the number of
 * the mod sequentially, later used as a bit number, which can be more
 * than 0..31.
 *
 * In order to retain compatibility with older SoCs, we perform R/W on
 * the single 32 bits registers, but this requires us to translate the
 * mutex ID bit accordingly.
 */
#define DISP_REG_MUTEX_MOD(mutex, id, n) ({ \
	const typeof(mutex) _mutex = (mutex); \
	u32 _offset = (id) < 32 ? \
		      _mutex->data->mutex_mod_reg : \
		      _mutex->data->mutex_mod1_reg; \
	_offset + 0x20 * (n); \
})
#define DISP_REG_MUTEX_SOF(mutex_sof_reg, n)	(mutex_sof_reg + 0x20 * (n))

#define INT_MUTEX				BIT(1)

#define MT2712_MUTEX_MOD_DISP_PWM2		10
#define MT2712_MUTEX_MOD_DISP_OVL0		11
#define MT2712_MUTEX_MOD_DISP_OVL1		12
#define MT2712_MUTEX_MOD_DISP_RDMA0		13
#define MT2712_MUTEX_MOD_DISP_RDMA1		14
#define MT2712_MUTEX_MOD_DISP_RDMA2		15
#define MT2712_MUTEX_MOD_DISP_WDMA0		16
#define MT2712_MUTEX_MOD_DISP_WDMA1		17
#define MT2712_MUTEX_MOD_DISP_COLOR0		18
#define MT2712_MUTEX_MOD_DISP_COLOR1		19
#define MT2712_MUTEX_MOD_DISP_AAL0		20
#define MT2712_MUTEX_MOD_DISP_UFOE		22
#define MT2712_MUTEX_MOD_DISP_PWM0		23
#define MT2712_MUTEX_MOD_DISP_PWM1		24
#define MT2712_MUTEX_MOD_DISP_OD0		25
#define MT2712_MUTEX_MOD2_DISP_AAL1		33
#define MT2712_MUTEX_MOD2_DISP_OD1		34

#define MT2701_MUTEX_MOD_DISP_OVL		3
#define MT2701_MUTEX_MOD_DISP_WDMA		6
#define MT2701_MUTEX_MOD_DISP_COLOR		7
#define MT2701_MUTEX_MOD_DISP_BLS		9
#define MT2701_MUTEX_MOD_DISP_RDMA0		10
#define MT2701_MUTEX_MOD_DISP_RDMA1		12

#define MT2712_MUTEX_SOF_SINGLE_MODE		0
#define MT2712_MUTEX_SOF_DSI0			1
#define MT2712_MUTEX_SOF_DSI1			2
#define MT2712_MUTEX_SOF_DPI0			3
#define MT2712_MUTEX_SOF_DPI1			4
#define MT2712_MUTEX_SOF_DSI2			5
#define MT2712_MUTEX_SOF_DSI3			6

#define MT6572_MUTEX_MOD_DISP_OVL		3
#define MT6572_MUTEX_MOD_DISP_WDMA		6
#define MT6572_MUTEX_MOD_DISP_COLOR		7
#define MT6572_MUTEX_MOD_DISP_BLS		9
// XXX: this is RDMA1, RDMA0 is MDP
#define MT6572_MUTEX_MOD_DISP_RDMA0		10

struct mtk_mutex {
	u8 id;
	bool claimed;
};

enum mtk_mutex_sof_id {
	MUTEX_SOF_SINGLE_MODE,
	MUTEX_SOF_DSI0,
	MUTEX_SOF_DSI1,
	MUTEX_SOF_DPI0,
	MUTEX_SOF_DPI1,
	MUTEX_SOF_DSI2,
	MUTEX_SOF_DSI3,
	MUTEX_SOF_DP_INTF0,
	MUTEX_SOF_DP_INTF1,
	DDP_MUTEX_SOF_MAX,
};

struct mtk_mutex_data {
	const u8 *mutex_mod;
	const u8 *mutex_table_mod;
	const u16 *mutex_sof;
	const u16 mutex_mod_reg;
	const u16 mutex_mod1_reg;
	const u16 mutex_sof_reg;
	const bool no_clk;
};

struct mtk_mutex_ctx {
	struct udevice			*dev;
	struct clk			clk;
	void __iomem			*regs;
	struct mtk_mutex		mutex[MTK_MUTEX_MAX_HANDLES];
	const struct mtk_mutex_data	*data;
	phys_addr_t			addr;
};

static const u8 mt6572_mutex_mod[DDP_COMPONENT_ID_MAX] = {
	[DDP_COMPONENT_BLS] = MT6572_MUTEX_MOD_DISP_BLS,
	[DDP_COMPONENT_COLOR0] = MT6572_MUTEX_MOD_DISP_COLOR,
	[DDP_COMPONENT_OVL0] = MT6572_MUTEX_MOD_DISP_OVL,
	[DDP_COMPONENT_RDMA0] = MT6572_MUTEX_MOD_DISP_RDMA0,
	[DDP_COMPONENT_WDMA0] = MT6572_MUTEX_MOD_DISP_WDMA,
};

static const u16 mt2712_mutex_sof[DDP_MUTEX_SOF_MAX] = {
	[MUTEX_SOF_SINGLE_MODE] = MUTEX_SOF_SINGLE_MODE,
	[MUTEX_SOF_DSI0] = MUTEX_SOF_DSI0,
	[MUTEX_SOF_DSI1] = MUTEX_SOF_DSI1,
	[MUTEX_SOF_DPI0] = MUTEX_SOF_DPI0,
	[MUTEX_SOF_DPI1] = MUTEX_SOF_DPI1,
	[MUTEX_SOF_DSI2] = MUTEX_SOF_DSI2,
	[MUTEX_SOF_DSI3] = MUTEX_SOF_DSI3,
};

static const struct mtk_mutex_data mt6572_mutex_driver_data = {
	.mutex_mod = mt6572_mutex_mod,
	.mutex_sof = mt2712_mutex_sof,
	.mutex_mod_reg = MT2701_MUTEX0_MOD0,
	.mutex_sof_reg = MT2701_MUTEX0_SOF0,
};

struct mtk_mutex *mtk_mutex_get(struct udevice *dev)
{
	struct mtk_mutex_ctx *mtx = dev_get_priv(dev);
	int i;

	for (i = 0; i < MTK_MUTEX_MAX_HANDLES; i++)
		if (!mtx->mutex[i].claimed) {
			mtx->mutex[i].claimed = true;
			return &mtx->mutex[i];
		}

	return ERR_PTR(-EBUSY);
}
EXPORT_SYMBOL_GPL(mtk_mutex_get);

void mtk_mutex_put(struct mtk_mutex *mutex)
{
	struct mtk_mutex_ctx *mtx = container_of(mutex, struct mtk_mutex_ctx,
						 mutex[mutex->id]);

	WARN_ON(&mtx->mutex[mutex->id] != mutex);

	mutex->claimed = false;
}
EXPORT_SYMBOL_GPL(mtk_mutex_put);

void mtk_mutex_add_comp(struct mtk_mutex *mutex,
			enum mtk_ddp_comp_id id)
{
	struct mtk_mutex_ctx *mtx = container_of(mutex, struct mtk_mutex_ctx,
						 mutex[mutex->id]);
	unsigned int reg;
	unsigned int sof_id, mod_id;
	unsigned int offset;

	WARN_ON(&mtx->mutex[mutex->id] != mutex);

	switch (id) {
	case DDP_COMPONENT_DSI0:
		sof_id = MUTEX_SOF_DSI0;
		break;
	case DDP_COMPONENT_DSI1:
		sof_id = MUTEX_SOF_DSI0;
		break;
	case DDP_COMPONENT_DSI2:
		sof_id = MUTEX_SOF_DSI2;
		break;
	case DDP_COMPONENT_DSI3:
		sof_id = MUTEX_SOF_DSI3;
		break;
	case DDP_COMPONENT_DPI0:
		sof_id = MUTEX_SOF_DPI0;
		break;
	case DDP_COMPONENT_DPI1:
		sof_id = MUTEX_SOF_DPI1;
		break;
	case DDP_COMPONENT_DP_INTF0:
		sof_id = MUTEX_SOF_DP_INTF0;
		break;
	case DDP_COMPONENT_DP_INTF1:
		sof_id = MUTEX_SOF_DP_INTF1;
		break;
	default:
		offset = DISP_REG_MUTEX_MOD(mtx, mtx->data->mutex_mod[id], mutex->id);
		mod_id = mtx->data->mutex_mod[id] % 32;
		reg = readl_relaxed(mtx->regs + offset);
		reg |= BIT(mod_id);
		writel_relaxed(reg, mtx->regs + offset);
		return;
	}

	writel_relaxed(mtx->data->mutex_sof[sof_id],
		       mtx->regs +
		       DISP_REG_MUTEX_SOF(mtx->data->mutex_sof_reg, mutex->id));
}
EXPORT_SYMBOL_GPL(mtk_mutex_add_comp);

void mtk_mutex_remove_comp(struct mtk_mutex *mutex,
			   enum mtk_ddp_comp_id id)
{
	struct mtk_mutex_ctx *mtx = container_of(mutex, struct mtk_mutex_ctx,
						 mutex[mutex->id]);
	unsigned int reg;
	unsigned int mod_id;
	unsigned int offset;

	WARN_ON(&mtx->mutex[mutex->id] != mutex);

	switch (id) {
	case DDP_COMPONENT_DSI0:
	case DDP_COMPONENT_DSI1:
	case DDP_COMPONENT_DSI2:
	case DDP_COMPONENT_DSI3:
	case DDP_COMPONENT_DPI0:
	case DDP_COMPONENT_DPI1:
	case DDP_COMPONENT_DP_INTF0:
	case DDP_COMPONENT_DP_INTF1:
		writel_relaxed(MUTEX_SOF_SINGLE_MODE,
			       mtx->regs +
			       DISP_REG_MUTEX_SOF(mtx->data->mutex_sof_reg,
						  mutex->id));
		break;
	default:
		offset = DISP_REG_MUTEX_MOD(mtx, mtx->data->mutex_mod[id], mutex->id);
		mod_id = mtx->data->mutex_mod[id] % 32;
		reg = readl_relaxed(mtx->regs + offset);
		reg &= ~BIT(mod_id);
		writel_relaxed(reg, mtx->regs + offset);
		break;
	}
}
EXPORT_SYMBOL_GPL(mtk_mutex_remove_comp);

void mtk_mutex_enable(struct mtk_mutex *mutex)
{
	struct mtk_mutex_ctx *mtx = container_of(mutex, struct mtk_mutex_ctx,
						 mutex[mutex->id]);

	WARN_ON(&mtx->mutex[mutex->id] != mutex);

	writel(1, mtx->regs + DISP_REG_MUTEX_EN(mutex->id));
}
EXPORT_SYMBOL_GPL(mtk_mutex_enable);

void mtk_mutex_disable(struct mtk_mutex *mutex)
{
	struct mtk_mutex_ctx *mtx = container_of(mutex, struct mtk_mutex_ctx,
						 mutex[mutex->id]);

	WARN_ON(&mtx->mutex[mutex->id] != mutex);

	writel(0, mtx->regs + DISP_REG_MUTEX_EN(mutex->id));
}
EXPORT_SYMBOL_GPL(mtk_mutex_disable);

void mtk_mutex_acquire(struct mtk_mutex *mutex)
{
	struct mtk_mutex_ctx *mtx = container_of(mutex, struct mtk_mutex_ctx,
						 mutex[mutex->id]);
	u32 tmp;

	writel(1, mtx->regs + DISP_REG_MUTEX_EN(mutex->id));
	writel(1, mtx->regs + DISP_REG_MUTEX(mutex->id));
	if (readl_poll_timeout(mtx->regs + DISP_REG_MUTEX(mutex->id),
				      tmp, tmp & INT_MUTEX, 10000))
		pr_err("could not acquire mutex %d\n", mutex->id);
}
EXPORT_SYMBOL_GPL(mtk_mutex_acquire);

void mtk_mutex_release(struct mtk_mutex *mutex)
{
	struct mtk_mutex_ctx *mtx = container_of(mutex, struct mtk_mutex_ctx,
						 mutex[mutex->id]);

	writel(0, mtx->regs + DISP_REG_MUTEX(mutex->id));
}
EXPORT_SYMBOL_GPL(mtk_mutex_release);

int mtk_mutex_write_mod(struct mtk_mutex *mutex,
			enum mtk_mutex_mod_index idx, bool clear)
{
	struct mtk_mutex_ctx *mtx = container_of(mutex, struct mtk_mutex_ctx,
						 mutex[mutex->id]);
	unsigned int reg;
	u32 offset, mod_id;

	WARN_ON(&mtx->mutex[mutex->id] != mutex);

	if (idx < MUTEX_MOD_IDX_MDP_RDMA0 ||
	    idx >= MUTEX_MOD_IDX_MAX) {
		dev_err(mtx->dev, "Not supported MOD table index : %d", idx);
		return -EINVAL;
	}

	offset = DISP_REG_MUTEX_MOD(mtx, mtx->data->mutex_table_mod[idx], mutex->id);
	mod_id = mtx->data->mutex_table_mod[idx] % 32;

	reg = readl_relaxed(mtx->regs + offset);
	if (clear)
		reg &= ~BIT(mod_id);
	else
		reg |= BIT(mod_id);

	writel_relaxed(reg, mtx->regs + offset);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mutex_write_mod);

int mtk_mutex_write_sof(struct mtk_mutex *mutex,
			enum mtk_mutex_sof_index idx)
{
	struct mtk_mutex_ctx *mtx = container_of(mutex, struct mtk_mutex_ctx,
						 mutex[mutex->id]);

	WARN_ON(&mtx->mutex[mutex->id] != mutex);

	if (idx < MUTEX_SOF_IDX_SINGLE_MODE ||
	    idx >= MUTEX_SOF_IDX_MAX) {
		dev_err(mtx->dev, "Not supported SOF index : %d", idx);
		return -EINVAL;
	}

	writel_relaxed(idx, mtx->regs +
		       DISP_REG_MUTEX_SOF(mtx->data->mutex_sof_reg, mutex->id));

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mutex_write_sof);


static int mtk_mutex_probe(struct udevice *dev)
{
	struct mtk_mutex_ctx *mtx = dev_get_priv(dev);
	int i, ret;

	mtx->dev = dev;
	mtx->regs = dev_remap_addr(dev);
	if (!mtx->regs)
		return -EINVAL;

	mtx->data = (const struct mtk_mutex_data *)dev_get_driver_data(dev);

	for (i = 0; i < MTK_MUTEX_MAX_HANDLES; i++)
		mtx->mutex[i].id = i;

	ret = clk_get_by_index(dev, 0, &mtx->clk);
	if (ret)
		return ret;

	ret = clk_enable(&mtx->clk);
	if (ret)
		return ret;

	return 0;
}

static const struct udevice_id mutex_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6572-disp-mutex", .data = (ulong)&mt6572_mutex_driver_data },
	{ /* sentinel */ },
};

U_BOOT_DRIVER(mtk_mutex) = {
	.name = "mediatek_mutex",
	.id = UCLASS_MISC,
	.of_match = mutex_driver_dt_match,
	.probe = mtk_mutex_probe,
	.priv_auto = sizeof(struct mtk_mutex_ctx),
};

MODULE_AUTHOR("Yongqiang Niu <yongqiang.niu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC MUTEX driver");
MODULE_LICENSE("GPL");
