// SPDX-License-Identifier: GPL-2.0
#include <clk-uclass.h>
#include <dm.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <regmap.h>
#include <syscon.h>
#include <malloc.h>

#include "clk-mtk.h"
#include "clk-mux.h"

#define MTK_WAIT_FENC_DONE_US 30

struct mtk_clk_mux {
	struct clk clk;
	struct regmap *regmap;
	struct regmap *regmap_hwv;
	const struct mtk_mux *data;
	bool reparent;
};

static inline struct mtk_clk_mux *to_mtk_clk_mux(struct clk *clk)
{
	return container_of(clk, struct mtk_clk_mux, clk);
}

static int mtk_clk_mux_enable_setclr(struct clk *clk)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(clk);

	regmap_write(mux->regmap, mux->data->clr_ofs,
		     BIT(mux->data->gate_shift));

	/*
	 * If the parent has been changed when the clock was disabled, it will
	 * not be effective yet. Set the update bit to ensure the mux gets
	 * updated.
	 */
	if (mux->reparent && mux->data->upd_shift >= 0) {
		regmap_write(mux->regmap, mux->data->upd_ofs,
			     BIT(mux->data->upd_shift));
		mux->reparent = false;
	}

	return 0;
}

static int mtk_clk_mux_disable_setclr(struct clk *clk)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(clk);

	regmap_write(mux->regmap, mux->data->set_ofs,
		     BIT(mux->data->gate_shift));

	return 0;
}

static int mtk_clk_mux_get_active_index(struct mtk_clk_mux *mux)
{
	u32 mask = GENMASK(mux->data->mux_width - 1, 0);
	u32 val;

	regmap_read(mux->regmap, mux->data->mux_ofs, &val);
	val = (val >> mux->data->mux_shift) & mask;

	if (mux->data->parent_index) {
		int i;

		for (i = 0; i < mux->data->num_parents; i++) {
			if (mux->data->parent_index[i] == val)
				return i;
		}

		/* Not found: return an impossible index to generate error */
		return mux->data->num_parents + 1;
	}

	return val;
}

static int mtk_clk_mux_set_parent_setclr(struct clk *clk, struct clk *parent)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(clk);
	u32 mask = GENMASK(mux->data->mux_width - 1, 0);
	u32 val, orig, index;
	int i;

	for (i = 0; i < mux->data->num_parents; i++) {
		if (!strcmp(parent->dev->name, mux->data->parent_names[i])) {
			index = i;
			break;
		}
	}

	if (mux->data->parent_index)
		index = mux->data->parent_index[index];

	regmap_read(mux->regmap, mux->data->mux_ofs, &orig);
	val = (orig & ~(mask << mux->data->mux_shift)) |
	      (index << mux->data->mux_shift);

	if (val != orig) {
		regmap_write(mux->regmap, mux->data->clr_ofs,
			     mask << mux->data->mux_shift);
		regmap_write(mux->regmap, mux->data->set_ofs,
			     index << mux->data->mux_shift);

		if (mux->data->upd_shift >= 0) {
			regmap_write(mux->regmap, mux->data->upd_ofs,
				     BIT(mux->data->upd_shift));
			mux->reparent = true;
		}
	}

	return 0;
}

static ulong mtk_clk_mux_get_rate(struct clk *clk)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(clk);
	int idx = mtk_clk_mux_get_active_index(mux);
	struct udevice *parent_dev;
	struct clk *parent_clk;

	if (idx >= mux->data->num_parents)
		return 0;

	if (uclass_get_device_by_name(UCLASS_CLK, mux->data->parent_names[idx],
				      &parent_dev))
		return 0;

	parent_clk = dev_get_uclass_priv(parent_dev);
	if (!parent_clk)
		return 0;

	return clk_get_rate(parent_clk);
}

const struct clk_ops mtk_mux_clr_set_upd_ops = {
	.set_parent = mtk_clk_mux_set_parent_setclr,
	.get_rate = mtk_clk_mux_get_rate,
};

U_BOOT_DRIVER(mtk_mux_clr_set_upd) = {
	.name = "mtk_mux_clr_set_upd",
	.id = UCLASS_CLK,
	.ops = &mtk_mux_clr_set_upd_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

const struct clk_ops mtk_mux_gate_clr_set_upd_ops = {
	.enable = mtk_clk_mux_enable_setclr,
	.disable = mtk_clk_mux_disable_setclr,
	.set_parent = mtk_clk_mux_set_parent_setclr,
	.get_rate = mtk_clk_mux_get_rate,
};

U_BOOT_DRIVER(mtk_mux_gate_clr_set_upd) = {
	.name = "mtk_mux_gate_clr_set_upd",
	.id = UCLASS_CLK,
	.ops = &mtk_mux_gate_clr_set_upd_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

static struct clk *mtk_clk_register_mux(struct udevice *dev,
					const struct mtk_mux *mux,
					struct regmap *regmap)
{
	struct mtk_clk_mux *clk_mux;
	struct clk *clk;
	const char *parent;
	int ret, idx;

	clk_mux = calloc(1, sizeof(*clk_mux));
	if (!clk_mux)
		return ERR_PTR(-ENOMEM);

	clk_mux->regmap = regmap;
	clk_mux->data = mux;

	clk = &clk_mux->clk;
	clk->id = mux->id;
	clk->flags = mux->flags;

	idx = mtk_clk_mux_get_active_index(clk_mux);

	if (idx >= mux->num_parents)
		idx = 0;

	parent = mux->parent_names[idx];

	ret = clk_register(clk, mux->drv_name, mux->name, parent);
	if (ret) {
		free(clk_mux);
		return ERR_PTR(ret);
	}

	return clk;
}

int mtk_clk_register_muxes(struct udevice *dev, const struct mtk_mux *muxes,
			   int num, struct mtk_clk_priv *priv)
{
	struct regmap *regmap;
	struct clk *clk;
	int i;

	regmap = syscon_node_to_regmap(dev_ofnode(dev));
	if (IS_ERR(regmap)) {
		printf("Cannot find regmap for device\n");
		return PTR_ERR(regmap);
	}

	for (i = 0; i < num; i++) {
		const struct mtk_mux *mux = &muxes[i];

		if (priv->hws[mux->id]) {
			printf("Trying to register duplicate clock ID: %d\n",
			       mux->id);
			continue;
		}

		clk = mtk_clk_register_mux(dev, mux, regmap);

		if (IS_ERR(clk)) {
			printf("Failed to register clk %s\n", mux->name);
			continue;
		}

		priv->hws[mux->id] = clk;
	}

	return 0;
}
