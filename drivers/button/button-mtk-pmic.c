// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017 MediaTek, Inc.
 * Author: Chen Zhong <chen.zhong@mediatek.com>
 * Copyright (c) 2026 Ben Grisdale <bengris32@protonmail.ch>
 */

#include <button.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <power/pmic.h>
#include <power/mt6323.h>
#include <power/mt6357.h>
#include <power/mt6359.h>
#include <linux/bitops.h>

#define MTK_PMIC_RST_DU_MASK	GENMASK(9, 8)
#define MTK_PMIC_PWRKEY_RST	BIT(6)
#define MTK_PMIC_HOMEKEY_RST	BIT(5)

#define MTK_PMIC_PWRKEY_INDEX	0
#define MTK_PMIC_HOMEKEY_INDEX	1
#define MTK_PMIC_MAX_KEY_COUNT	2

enum mtk_pmic_key_lp_mode {
	LP_DISABLE,
	LP_ONEKEY,
	LP_TWOKEY,
};

struct mtk_pmic_key_regs {
	u32 deb_reg;
	u32 deb_mask;
	u32 rst_en_mask;
};

#define MTK_PMIC_KEY_REGS(_deb_reg, _deb_mask, _rst_mask)	\
{								\
	.deb_reg		= _deb_reg,			\
	.deb_mask		= _deb_mask,			\
	.rst_en_mask		= _rst_mask,			\
}

struct mtk_pmic_regs {
	struct mtk_pmic_key_regs keys_regs[MTK_PMIC_MAX_KEY_COUNT];
	u32 pmic_rst_reg;
	u32 rst_lprst_mask; /* Long-press reset timeout bitmask */
};

static const struct mtk_pmic_regs mt6323_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEY_REGS(MT6323_CHRSTATUS, 0x2,
				   MTK_PMIC_PWRKEY_RST),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEY_REGS(MT6323_CHRSTATUS, 0x4,
				   MTK_PMIC_HOMEKEY_RST),
	.pmic_rst_reg = MT6323_TOP_RST_MISC,
	.rst_lprst_mask = MTK_PMIC_RST_DU_MASK,
};

static const struct mtk_pmic_regs mt6357_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEY_REGS(MT6357_TOPSTATUS, 0x2,
				   MTK_PMIC_PWRKEY_RST),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEY_REGS(MT6357_TOPSTATUS, 0x8,
				   MTK_PMIC_HOMEKEY_RST),
	.pmic_rst_reg = MT6357_TOP_RST_MISC,
	.rst_lprst_mask = MTK_PMIC_RST_DU_MASK,
};

static const struct mtk_pmic_regs mt6359_regs = {
	.keys_regs[MTK_PMIC_PWRKEY_INDEX] =
		MTK_PMIC_KEY_REGS(MT6359_TOPSTATUS, 0x2,
				   MTK_PMIC_PWRKEY_RST),
	.keys_regs[MTK_PMIC_HOMEKEY_INDEX] =
		MTK_PMIC_KEY_REGS(MT6359_TOPSTATUS, 0x8,
				   MTK_PMIC_HOMEKEY_RST),
	.pmic_rst_reg = MT6359_TOP_RST_MISC,
	.rst_lprst_mask = MTK_PMIC_RST_DU_MASK,
};

struct mtk_pmic_button_priv {
	struct udevice *pmic;
	u32 deb_reg;
	u32 deb_mask;
	int code;
};

static void mtk_pmic_key_lp_reset_setup(struct udevice *dev,
					 const struct mtk_pmic_regs *regs)
{
	const struct mtk_pmic_key_regs *kregs_home, *kregs_pwr;
	u32 long_press_mode, long_press_debounce;
	u32 value, mask;
	int error;

	kregs_home = &regs->keys_regs[MTK_PMIC_HOMEKEY_INDEX];
	kregs_pwr = &regs->keys_regs[MTK_PMIC_PWRKEY_INDEX];

	error = dev_read_u32(dev, "power-off-time-sec",
			     &long_press_debounce);
	if (error)
		long_press_debounce = 0;

	mask = regs->rst_lprst_mask;
	value = long_press_debounce << (ffs(regs->rst_lprst_mask) - 1);

	error  = dev_read_u32(dev, "mediatek,long-press-mode",
			      &long_press_mode);
	if (error)
		long_press_mode = LP_DISABLE;

	switch (long_press_mode) {
	case LP_TWOKEY:
		value |= kregs_home->rst_en_mask;
		/* fall through */
	case LP_ONEKEY:
		value |= kregs_pwr->rst_en_mask;
		/* fall through */
	case LP_DISABLE:
		mask |= kregs_home->rst_en_mask;
		mask |= kregs_pwr->rst_en_mask;
		break;
	default:
		break;
	}

	pmic_clrsetbits(dev->parent, regs->pmic_rst_reg, mask, value);
}

static enum button_state_t mtk_pmic_button_get_state(struct udevice *dev)
{
	struct mtk_pmic_button_priv *priv = dev_get_priv(dev);
	int val;

	val = pmic_reg_read(priv->pmic, priv->deb_reg);
	if (val < 0)
		return BUTTON_OFF;

	return (val & priv->deb_mask) ? BUTTON_OFF : BUTTON_ON;
}

static int mtk_pmic_button_get_code(struct udevice *dev)
{
	struct mtk_pmic_button_priv *priv = dev_get_priv(dev);

	return priv->code;
}

static int mtk_pmic_button_probe(struct udevice *dev)
{
	struct button_uc_plat *uc_plat = dev_get_uclass_plat(dev);
	struct mtk_pmic_button_priv *priv = dev_get_priv(dev);
	struct udevice *parent = dev->parent;
	const struct mtk_pmic_regs *regs;
	int index;

	/* Top level PMIC Keys node won't have a label. */
	if (!uc_plat->label) {
		regs = (const struct mtk_pmic_regs *)dev_get_driver_data(dev);
		if (regs)
			mtk_pmic_key_lp_reset_setup(dev, regs);
		return 0;
	}

	regs = (const struct mtk_pmic_regs *)dev_get_driver_data(parent);
	if (!regs)
		return -EINVAL;

	if (!strcmp(dev->name, "power")) {
		index = MTK_PMIC_PWRKEY_INDEX;
	} else if (!strcmp(dev->name, "home")) {
		index = MTK_PMIC_HOMEKEY_INDEX;
	} else {
		return -EINVAL;
	}

	priv->pmic = parent->parent;
	priv->deb_reg = regs->keys_regs[index].deb_reg;
	priv->deb_mask = regs->keys_regs[index].deb_mask;

	dev_read_u32(dev, "linux,code", &priv->code);

	return 0;
}

static int mtk_pmic_button_bind(struct udevice *parent)
{
	struct udevice *dev;
	ofnode node;
	int ret;

	dev_for_each_subnode(node, parent) {
		struct button_uc_plat *uc_plat;
		const char *label;
		const char *name = ofnode_get_name(node);

		if (!ofnode_is_enabled(node))
			continue;

		if (!strcmp(name, "power")) {
			label = "Power Button";
		} else if (!strcmp(name, "home")) {
			label = "Home Button";
		} else {
			continue;
		}

		ret = device_bind_driver_to_node(parent, "mtk_pmic_button",
						 name, node, &dev);
		if (ret)
			return ret;

		uc_plat = dev_get_uclass_plat(dev);
		uc_plat->label = label;
	}

	return 0;
}

static const struct button_ops mtk_pmic_button_ops = {
	.get_state	= mtk_pmic_button_get_state,
	.get_code	= mtk_pmic_button_get_code,
};

static const struct udevice_id mtk_pmic_button_ids[] = {
	{ .compatible = "mediatek,mt6323-keys", .data = (ulong)&mt6323_regs },
	{ .compatible = "mediatek,mt6357-keys", .data = (ulong)&mt6357_regs },
	{ .compatible = "mediatek,mt6359-keys", .data = (ulong)&mt6359_regs },
	{ }
};

U_BOOT_DRIVER(mtk_pmic_button) = {
	.name		= "mtk_pmic_button",
	.id		= UCLASS_BUTTON,
	.of_match	= mtk_pmic_button_ids,
	.bind		= mtk_pmic_button_bind,
	.probe		= mtk_pmic_button_probe,
	.ops		= &mtk_pmic_button_ops,
	.priv_auto	= sizeof(struct mtk_pmic_button_priv),
};
