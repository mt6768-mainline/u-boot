// SPDX-License-Identifier: GPL-2.0-only
#include <clk-uclass.h>
#include <dm.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <malloc.h>

#include "clk-mtk.h"

int mtk_clk_register_fixed_clks(struct udevice *dev,
				const struct mtk_fixed_clk *clks, int num,
				struct mtk_clk_priv *priv)
{
	int i;
	struct clk *clk;

	if (!priv)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		const struct mtk_fixed_clk *rc = &clks[i];

		if (priv->hws[rc->id])
			continue;

		clk = clk_register_fixed_rate(NULL, rc->name, rc->rate);
		if (IS_ERR(clk)) {
			printf("Failed to register clk %s\n", rc->name);
			continue;
		}
		priv->hws[rc->id] = clk;
	}

	return 0;
}

int mtk_clk_register_factors(struct udevice *dev,
			     const struct mtk_fixed_factor *clks, int num,
			     struct mtk_clk_priv *priv)
{
	int i;
	struct clk *clk;

	if (!priv)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		const struct mtk_fixed_factor *ff = &clks[i];

		if (priv->hws[ff->id])
			continue;

		clk = clk_register_fixed_factor(dev, ff->name, ff->parent_name,
						ff->flags, ff->mult, ff->div);

		if (IS_ERR(clk)) {
			printf("Failed to register clk %s\n", ff->name);
			continue;
		}
		priv->hws[ff->id] = clk;
	}

	return 0;
}

static struct clk *mtk_clk_register_composite(struct udevice *dev,
					      const struct mtk_composite *mc,
					      void __iomem *base)
{
	struct clk *clk;
	struct clk_mux *mux = NULL;
	struct clk_gate *gate = NULL;
	struct clk_divider *div = NULL;
	struct clk *mux_hw = NULL, *gate_hw = NULL, *div_hw = NULL;
	const struct clk_ops *mux_ops = NULL, *gate_ops = NULL, *div_ops = NULL;
	const char *const *parent_names;
	int num_parents;

	if (mc->mux_shift >= 0) {
		mux = calloc(1, sizeof(*mux));
		if (!mux)
			return ERR_PTR(-ENOMEM);

		mux->reg = base + mc->mux_reg;
		mux->mask = BIT(mc->mux_width) - 1;
		mux->shift = mc->mux_shift;
		mux->flags = mc->mux_flags;
		mux_hw = &mux->clk;
		mux_ops = &clk_mux_ops;

		parent_names = mc->parent_names;
		num_parents = mc->num_parents;
	} else {
		parent_names = &mc->parent;
		num_parents = 1;
	}

	if (mc->gate_shift >= 0) {
		gate = calloc(1, sizeof(*gate));
		if (!gate)
			goto err_out;

		gate->reg = base + mc->gate_reg;
		gate->bit_idx = mc->gate_shift;
		gate->flags = CLK_GATE_SET_TO_DISABLE;

		gate_hw = &gate->clk;
		gate_ops = &clk_gate_ops;
	}

	if (mc->divider_shift >= 0) {
		div = calloc(1, sizeof(*div));
		if (!div)
			goto err_out;

		div->reg = base + mc->divider_reg;
		div->shift = mc->divider_shift;
		div->width = mc->divider_width;

		div_hw = &div->clk;
		div_ops = &clk_divider_ops;
	}

	clk = clk_register_composite(dev, mc->name, parent_names, num_parents,
				     mux_hw, mux_ops, div_hw, div_ops, gate_hw,
				     gate_ops, mc->flags);

	if (IS_ERR(clk))
		goto err_out;

	return clk;

err_out:
	free(div);
	free(gate);
	free(mux);
	return ERR_PTR(-ENOMEM);
}

int mtk_clk_register_composites(struct udevice *dev,
				const struct mtk_composite *mcs, int num,
				void __iomem *base, struct mtk_clk_priv *priv)
{
	struct clk *clk;
	int i;

	if (!priv)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		const struct mtk_composite *mc = &mcs[i];

		if (priv->hws[mc->id])
			continue;

		clk = mtk_clk_register_composite(dev, mc, base);

		if (IS_ERR(clk)) {
			printf("Failed to register clk %s\n", mc->name);
			continue;
		}
		priv->hws[mc->id] = clk;
	}

	return 0;
}

int mtk_clk_register_dividers(struct udevice *dev,
			      const struct mtk_clk_divider *mcds, int num,
			      void __iomem *base, struct mtk_clk_priv *priv)
{
	struct clk *clk;
	int i;

	if (!priv)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		const struct mtk_clk_divider *mcd = &mcds[i];

		if (priv->hws[mcd->id])
			continue;

		clk = clk_register_divider(dev, mcd->name, mcd->parent_name,
					   mcd->flags, base + mcd->div_reg,
					   mcd->div_shift, mcd->div_width,
					   mcd->clk_divider_flags);

		if (IS_ERR(clk)) {
			printf("Failed to register clk %s\n", mcd->name);
			continue;
		}
		priv->hws[mcd->id] = clk;
	}

	return 0;
}
