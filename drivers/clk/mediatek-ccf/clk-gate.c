// SPDX-License-Identifier: GPL-2.0-only

#include "dm/device.h"
#include <dm/device_compat.h>
#include <syscon.h>
#include <linux/clk-provider.h>
#include <linux/printk.h>
#include <regmap.h>
#include <linux/types.h>

#include "clk-mtk.h"
#include "clk-gate.h"

struct mtk_cg {
	struct clk clk;
	struct regmap *regmap;
	const struct mtk_gate *gate;
};

static inline struct mtk_cg *to_mtk_cg(struct clk *clk)
{
	return container_of(clk, struct mtk_cg, clk);
}

static u32 mtk_get_clockgating(struct clk *clk)
{
	struct mtk_cg *cg = to_mtk_cg(clk);
	u32 val;

	regmap_read(cg->regmap, cg->gate->regs->sta_ofs, &val);

	return val & BIT(cg->gate->shift);
}

static int mtk_cg_bit_is_cleared(struct clk *clk)
{
	return mtk_get_clockgating(clk) == 0;
}

static int mtk_cg_bit_is_set(struct clk *clk)
{
	return mtk_get_clockgating(clk) != 0;
}

static void mtk_cg_set_bit(struct clk *clk)
{
	struct mtk_cg *cg = to_mtk_cg(clk);

	regmap_write(cg->regmap, cg->gate->regs->set_ofs, BIT(cg->gate->shift));
}

static void mtk_cg_clr_bit(struct clk *clk)
{
	struct mtk_cg *cg = to_mtk_cg(clk);

	regmap_write(cg->regmap, cg->gate->regs->clr_ofs, BIT(cg->gate->shift));
}

static void mtk_cg_set_bit_no_setclr(struct clk *clk)
{
	struct mtk_cg *cg = to_mtk_cg(clk);

	regmap_set_bits(cg->regmap, cg->gate->regs->sta_ofs,
			BIT(cg->gate->shift));
}

static void mtk_cg_clr_bit_no_setclr(struct clk *clk)
{
	struct mtk_cg *cg = to_mtk_cg(clk);

	regmap_clear_bits(cg->regmap, cg->gate->regs->sta_ofs,
			  BIT(cg->gate->shift));
}

static int mtk_cg_enable(struct clk *clk)
{
	mtk_cg_clr_bit(clk);

	return 0;
}

static int mtk_cg_disable(struct clk *clk)
{
	mtk_cg_set_bit(clk);

	return 0;
}

static int mtk_cg_enable_inv(struct clk *clk)
{
	mtk_cg_set_bit(clk);

	return 0;
}

static int mtk_cg_disable_inv(struct clk *clk)
{
	mtk_cg_clr_bit(clk);

	return 0;
}

static int mtk_cg_enable_no_setclr(struct clk *clk)
{
	mtk_cg_clr_bit_no_setclr(clk);

	return 0;
}

static int mtk_cg_disable_no_setclr(struct clk *clk)
{
	mtk_cg_set_bit_no_setclr(clk);

	return 0;
}

static int mtk_cg_enable_inv_no_setclr(struct clk *clk)
{
	mtk_cg_set_bit_no_setclr(clk);

	return 0;
}

static int mtk_cg_disable_inv_no_setclr(struct clk *clk)
{
	mtk_cg_clr_bit_no_setclr(clk);
	return 0;
}

static ulong mtk_cg_get_rate(struct clk *clk)
{
	ulong rate = clk_get_parent_rate(clk);
	if (IS_ERR_VALUE(rate))
		printf("Clock %s failed to get parent rate: %ld\n",
		       clk->dev->name, rate);
	return rate;
}

static void __maybe_unused mtk_cg_dump(struct udevice *dev)
{
	struct clk *clk = dev_get_uclass_priv(dev);
	struct mtk_cg *cg = to_mtk_cg(clk);
	const struct mtk_gate *gate = cg->gate;

	printf("%-32s: %s (parent: %s)\n", gate->name,
	       mtk_cg_bit_is_cleared(clk) ? "Y" : "N", gate->parent_name);
}

static void __maybe_unused mtk_cg_dump_inv(struct udevice *dev)
{
	struct clk *clk = dev_get_uclass_priv(dev);
	struct mtk_cg *cg = to_mtk_cg(clk);
	const struct mtk_gate *gate = cg->gate;

	printf("%-32s: %s (parent: %s)\n", gate->name,
	       mtk_cg_bit_is_set(clk) ? "Y" : "N", gate->parent_name);
}

static struct clk *mtk_clk_register_gate(struct udevice *dev,
					 const struct mtk_gate *gate,
					 struct regmap *regmap)
{
	struct mtk_cg *cg;
	struct clk *clk;
	int ret;

	cg = calloc(1, sizeof(*cg));
	if (!cg)
		return ERR_PTR(-ENOMEM);

	cg->regmap = regmap;
	cg->gate = gate;

	clk = &cg->clk;
	clk->id = gate->id;
	clk->flags = gate->flags | CLK_SET_RATE_PARENT;

	ret = clk_register(clk, gate->drv_name, gate->name, gate->parent_name);
	if (ret) {
		free(cg);
		return ERR_PTR(ret);
	}

	return clk;
}

int mtk_clk_register_gates(struct udevice *dev, const struct mtk_gate *clks,
			   int num, struct mtk_clk_priv *priv)
{
	int i;
	struct clk *clk;
	struct regmap *regmap;
	struct device_node *node = dev_ofnode(dev).np;

	regmap = syscon_node_to_regmap(dev_ofnode(dev));
	if (IS_ERR(regmap)) {
		pr_err("Cannot find regmap for %pOF: %pe\n", node, regmap);
		return PTR_ERR(regmap);
	}

	for (i = 0; i < num; i++) {
		const struct mtk_gate *gate = &clks[i];

		clk = mtk_clk_register_gate(dev, gate, regmap);

		if (IS_ERR(clk)) {
			printf("Failed to register clk %s\n", gate->name);
			return PTR_ERR(clk);
		}

		priv->hws[gate->id] = clk;
	}

	return 0;
}

const struct clk_ops mtk_clk_gate_ops_setclr = {
	.enable = mtk_cg_enable,
	.disable = mtk_cg_disable,
	.get_rate = mtk_cg_get_rate,
#if IS_ENABLED(CONFIG_CMD_CLK)
	.dump = mtk_cg_dump,
#endif
};

U_BOOT_DRIVER(mtk_cg_setclr) = {
	.name = "mtk_cg_setclr",
	.id = UCLASS_CLK,
	.ops = &mtk_clk_gate_ops_setclr,
	.flags = DM_FLAG_PRE_RELOC,
};

const struct clk_ops mtk_clk_gate_ops_setclr_inv = {
	.enable = mtk_cg_enable_inv,
	.disable = mtk_cg_disable_inv,
	.get_rate = mtk_cg_get_rate,
#if IS_ENABLED(CONFIG_CMD_CLK)
	.dump = mtk_cg_dump,
#endif
};

U_BOOT_DRIVER(mtk_cg_setclr_inv) = {
	.name = "mtk_cg_setclr_inv",
	.id = UCLASS_CLK,
	.ops = &mtk_clk_gate_ops_setclr_inv,
	.flags = DM_FLAG_PRE_RELOC,
};

const struct clk_ops mtk_clk_gate_ops_no_setclr = {
	.enable = mtk_cg_enable_no_setclr,
	.disable = mtk_cg_disable_no_setclr,
	.get_rate = mtk_cg_get_rate,
#if IS_ENABLED(CONFIG_CMD_CLK)
	.dump = mtk_cg_dump_inv,
#endif
};

U_BOOT_DRIVER(mtk_cg_no_setclr) = {
	.name = "mtk_cg_no_setclr",
	.id = UCLASS_CLK,
	.ops = &mtk_clk_gate_ops_no_setclr,
	.flags = DM_FLAG_PRE_RELOC,
};

const struct clk_ops mtk_clk_gate_ops_no_setclr_inv = {
	.enable = mtk_cg_enable_inv_no_setclr,
	.disable = mtk_cg_disable_inv_no_setclr,
	.get_rate = mtk_cg_get_rate,
#if IS_ENABLED(CONFIG_CMD_CLK)
	.dump = mtk_cg_dump_inv,
#endif
};

U_BOOT_DRIVER(mtk_cg_no_setclr_inv) = {
	.name = "mtk_cg_no_setclr_inv",
	.id = UCLASS_CLK,
	.ops = &mtk_clk_gate_ops_no_setclr_inv,
	.flags = DM_FLAG_PRE_RELOC,
};
