// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Chen Zhong <chen.zhong@mediatek.com>
 * Copyright (c) 2026 Ben Grisdale <bengris32@protonmail.ch>
 */

#include <dm.h>
#include <power/regulator.h>
#include <power/mt6323.h>
#include <power/pmic.h>
#include <linux/bitops.h>

enum mt6323_regulator_type {
	MT6323_REG_TYPE_RANGE,
	MT6323_REG_TYPE_TABLE,
	MT6323_REG_TYPE_FIXED,
};

struct mt6323_linear_range {
	unsigned int min;
	unsigned int min_sel;
	unsigned int max_sel;
	unsigned int step;
};

struct mt6323_regulator_desc {
	const char *name;
	const char *of_match;
	enum mt6323_regulator_type type;
	int id;
	unsigned int n_voltages;
	const unsigned int *volt_table;
	const struct mt6323_linear_range *linear_ranges;
	int n_linear_ranges;
	unsigned int min_uV;
	unsigned int vsel_reg;
	unsigned int vsel_mask;
	unsigned int enable_reg;
	unsigned int enable_mask;
};

struct mt6323_regulator_info {
	struct mt6323_regulator_desc desc;
	u32 vselon_reg;
	u32 vselctrl_reg;
	u32 vselctrl_mask;
};

#define REGULATOR_LINEAR_RANGE(_min_uV, _min_sel, _max_sel, _step_uV)	\
{									\
	.min		= _min_uV,					\
	.min_sel	= _min_sel,					\
	.max_sel	= _max_sel,					\
	.step		= _step_uV,					\
}

#define MT6323_BUCK(match, vreg, min, max, step, volt_ranges, enreg,	\
		    vosel, vosel_mask, voselon, vosel_ctrl)		\
	[MT6323_ID_##vreg] = {						\
		.desc = {						\
			.name = #vreg,					\
			.of_match = of_match_ptr(match),		\
			.type = MT6323_REG_TYPE_RANGE,			\
			.id = MT6323_ID_##vreg,				\
			.n_voltages = ((max) - (min)) / (step) + 1,	\
			.linear_ranges = volt_ranges,			\
			.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
			.vsel_reg = vosel,				\
			.vsel_mask = vosel_mask,			\
			.enable_reg = enreg,				\
			.enable_mask = BIT(0),				\
		},							\
		.vselon_reg = voselon,					\
		.vselctrl_reg = vosel_ctrl,				\
		.vselctrl_mask = BIT(1),				\
	}

#define MT6323_LDO(match, vreg, ldo_volt_table, enreg, enbit, vosel,	\
		   vosel_mask)						\
	[MT6323_ID_##vreg] = {						\
		.desc = {						\
			.name = #vreg,					\
			.of_match = of_match_ptr(match),		\
			.type = MT6323_REG_TYPE_TABLE,			\
			.id = MT6323_ID_##vreg,				\
			.n_voltages = ARRAY_SIZE(ldo_volt_table),	\
			.volt_table = ldo_volt_table,			\
			.vsel_reg = vosel,				\
			.vsel_mask = vosel_mask,			\
			.enable_reg = enreg,				\
			.enable_mask = BIT(enbit),			\
		},							\
	}

#define MT6323_REG_FIXED(match, vreg, enreg, enbit, volt)		\
	[MT6323_ID_##vreg] = {						\
		.desc = {						\
			.name = #vreg,					\
			.of_match = of_match_ptr(match),		\
			.type = MT6323_REG_TYPE_FIXED,			\
			.id = MT6323_ID_##vreg,				\
			.n_voltages = 1,				\
			.enable_reg = enreg,				\
			.enable_mask = BIT(enbit),			\
			.min_uV = volt,					\
		},							\
	}

static int mt6323_range_find_value(const struct mt6323_linear_range *r,
				   unsigned int sel,
				   unsigned int *val)
{
	if (!val || sel < r->min_sel || sel > r->max_sel)
		return -EINVAL;

	*val = r->min + r->step * (sel - r->min_sel);

	return 0;
}

static int mt6323_range_find_selector(const struct mt6323_linear_range *r,
				      int val, unsigned int *sel)
{
	int num_vals = r->max_sel - r->min_sel + 1;
	int ret = -EINVAL;

	if (val >= r->min && val <= r->min + r->step * (num_vals - 1)) {
		if (r->step) {
			*sel = r->min_sel + ((val - r->min) / r->step);
			ret = 0;
		} else {
			*sel = r->min_sel;
			ret = 0;
		}
	}
	return ret;
}

static int mt6323_set_voltage_sel_regmap(struct udevice *dev,
					 struct mt6323_regulator_info *info,
					 unsigned int sel)
{
	sel <<= ffs(info->desc.vsel_mask) - 1;

	return pmic_clrsetbits(dev->parent, info->desc.vsel_reg,
			       info->desc.vsel_mask, sel);
}

static int mt6323_get_voltage_sel(struct udevice *dev, struct mt6323_regulator_info *info)
{
	int selector;

	selector = pmic_reg_read(dev->parent, info->desc.vsel_reg);
	if (selector < 0)
		return selector;

	selector &= info->desc.vsel_mask;
	selector >>= ffs(info->desc.vsel_mask) - 1;

	return selector;
}

static int mt6323_get_enable(struct udevice *dev)
{
	struct mt6323_regulator_info *info = dev_get_priv(dev);
	int ret;

	ret = pmic_reg_read(dev->parent, info->desc.enable_reg);
	if (ret < 0)
		return ret;

	return ret & info->desc.enable_mask ? true : false;
}

static int mt6323_set_enable(struct udevice *dev, bool enable)
{
	struct mt6323_regulator_info *info = dev_get_priv(dev);

	return pmic_clrsetbits(dev->parent, info->desc.enable_reg,
			       info->desc.enable_mask,
			       enable ? info->desc.enable_mask : 0);
}

static int mt6323_get_value(struct udevice *dev)
{
	struct mt6323_regulator_info *info = dev_get_priv(dev);
	unsigned int val_uV;
	int selector, idx, ret;

	switch (info->desc.type) {
	case MT6323_REG_TYPE_RANGE:
		selector = mt6323_get_voltage_sel(dev, info);

		ret = -EINVAL;
		for (idx = 0; idx < info->desc.n_linear_ranges; idx++) {
			ret = mt6323_range_find_value(&info->desc.linear_ranges[idx],
						      selector, &val_uV);
			if (!ret)
				break;
		}

		if (ret)
			return ret;

		return val_uV;

	case MT6323_REG_TYPE_TABLE:
		selector = mt6323_get_voltage_sel(dev, info);

		if (selector >= info->desc.n_voltages)
			return -EINVAL;

		return info->desc.volt_table[selector];

	case MT6323_REG_TYPE_FIXED:
		return info->desc.min_uV;
	}

	return -EINVAL;
}

static int mt6323_set_value(struct udevice *dev, int uV)
{
	struct mt6323_regulator_info *info = dev_get_priv(dev);
	unsigned int selector;
	int idx, ret;

	switch (info->desc.type) {
	case MT6323_REG_TYPE_RANGE:
		ret = -EINVAL;
		for (idx = 0; idx < info->desc.n_linear_ranges; idx++) {
			ret = mt6323_range_find_selector(&info->desc.linear_ranges[idx],
							 uV, &selector);
			if (!ret)
				break;
		}

		if (ret)
			return ret;

		return mt6323_set_voltage_sel_regmap(dev, info, selector);

	case MT6323_REG_TYPE_TABLE:
		ret = -EINVAL;
		for (idx = 0; idx < info->desc.n_voltages; idx++) {
			if (info->desc.volt_table[idx] == uV) {
				selector = idx;
				ret = 0;
				break;
			}
		}

		if (ret)
			return ret;

		return mt6323_set_voltage_sel_regmap(dev, info, selector);

	case MT6323_REG_TYPE_FIXED:
		if (uV != info->desc.min_uV)
			return -EINVAL;
		return 0;
	}

	return -EINVAL;
}

static const struct mt6323_linear_range buck_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(700000, 0, 0x7f, 6250),
};

static const struct mt6323_linear_range buck_volt_range2[] = {
	REGULATOR_LINEAR_RANGE(1400000, 0, 0x7f, 12500),
};

static const struct mt6323_linear_range buck_volt_range3[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x3f, 50000),
};

static const unsigned int ldo_volt_table1[] = {
	3300000, 3400000, 3500000, 3600000,
};

static const unsigned int ldo_volt_table2[] = {
	1500000, 1800000, 2500000, 2800000,
};

static const unsigned int ldo_volt_table3[] = {
	1800000, 3300000,
};

static const unsigned int ldo_volt_table4[] = {
	3000000, 3300000,
};

static const unsigned int ldo_volt_table5[] = {
	1200000, 1300000, 1500000, 1800000, 2000000, 2800000, 3000000, 3300000,
};

static const unsigned int ldo_volt_table6[] = {
	1200000, 1300000, 1500000, 1800000, 2500000, 2800000, 3000000, 2000000,
};

static const unsigned int ldo_volt_table7[] = {
	1200000, 1300000, 1500000, 1800000,
};

static const unsigned int ldo_volt_table8[] = {
	1800000, 3000000,
};

static const unsigned int ldo_volt_table9[] = {
	1200000, 1350000, 1500000, 1800000,
};

static const unsigned int ldo_volt_table10[] = {
	1200000, 1300000, 1500000, 1800000,
};

static struct mt6323_regulator_info mt6323_regulators[] = {
	MT6323_BUCK("buck-vproc", VPROC, 700000, 1493750, 6250,
		    buck_volt_range1, MT6323_VPROC_CON7, MT6323_VPROC_CON9, 0x7f,
		    MT6323_VPROC_CON10, MT6323_VPROC_CON5),
	MT6323_BUCK("buck-vsys", VSYS, 1400000, 2987500, 12500,
		    buck_volt_range2, MT6323_VSYS_CON7, MT6323_VSYS_CON9, 0x7f,
		    MT6323_VSYS_CON10, MT6323_VSYS_CON5),
	MT6323_BUCK("buck-vpa", VPA, 500000, 3650000, 50000,
		    buck_volt_range3, MT6323_VPA_CON7, MT6323_VPA_CON9,
		    0x3f, MT6323_VPA_CON10, MT6323_VPA_CON5),
	MT6323_REG_FIXED("ldo-vtcxo", VTCXO, MT6323_ANALDO_CON1, 10, 2800000),
	MT6323_REG_FIXED("ldo-vcn28", VCN28, MT6323_ANALDO_CON19, 12, 2800000),
	MT6323_LDO("ldo-vcn33-bt", VCN33_BT, ldo_volt_table1,
		   MT6323_ANALDO_CON16, 7, MT6323_ANALDO_CON16, 0xC),
	MT6323_LDO("ldo-vcn33-wifi", VCN33_WIFI, ldo_volt_table1,
		   MT6323_ANALDO_CON17, 12, MT6323_ANALDO_CON16, 0xC),
	MT6323_REG_FIXED("ldo-va", VA, MT6323_ANALDO_CON2, 14, 2800000),
	MT6323_LDO("ldo-vcama", VCAMA, ldo_volt_table2,
		   MT6323_ANALDO_CON4, 15, MT6323_ANALDO_CON10, 0x60),
	MT6323_REG_FIXED("ldo-vio28", VIO28, MT6323_DIGLDO_CON0, 14, 2800000),
	MT6323_REG_FIXED("ldo-vusb", VUSB, MT6323_DIGLDO_CON2, 14, 3300000),
	MT6323_LDO("ldo-vmc", VMC, ldo_volt_table3,
		   MT6323_DIGLDO_CON3, 12, MT6323_DIGLDO_CON24, 0x10),
	MT6323_LDO("ldo-vmch", VMCH, ldo_volt_table4,
		   MT6323_DIGLDO_CON5, 14, MT6323_DIGLDO_CON26, 0x80),
	MT6323_LDO("ldo-vemc3v3", VEMC3V3, ldo_volt_table4,
		   MT6323_DIGLDO_CON6, 14, MT6323_DIGLDO_CON27, 0x80),
	MT6323_LDO("ldo-vgp1", VGP1, ldo_volt_table5,
		   MT6323_DIGLDO_CON7, 15, MT6323_DIGLDO_CON28, 0xE0),
	MT6323_LDO("ldo-vgp2", VGP2, ldo_volt_table6,
		   MT6323_DIGLDO_CON8, 15, MT6323_DIGLDO_CON29, 0xE0),
	MT6323_LDO("ldo-vgp3", VGP3, ldo_volt_table7,
		   MT6323_DIGLDO_CON9, 15, MT6323_DIGLDO_CON30, 0x60),
	MT6323_REG_FIXED("ldo-vcn18", VCN18, MT6323_DIGLDO_CON11, 14, 1800000),
	MT6323_LDO("ldo-vsim1", VSIM1, ldo_volt_table8,
		   MT6323_DIGLDO_CON13, 15, MT6323_DIGLDO_CON34, 0x20),
	MT6323_LDO("ldo-vsim2", VSIM2, ldo_volt_table8,
		   MT6323_DIGLDO_CON14, 15, MT6323_DIGLDO_CON35, 0x20),
	MT6323_REG_FIXED("ldo-vrtc", VRTC, MT6323_DIGLDO_CON15, 8, 2800000),
	MT6323_LDO("ldo-vcamaf", VCAMAF, ldo_volt_table5,
		   MT6323_DIGLDO_CON31, 15, MT6323_DIGLDO_CON32, 0xE0),
	MT6323_LDO("ldo-vibr", VIBR, ldo_volt_table5,
		   MT6323_DIGLDO_CON39, 15, MT6323_DIGLDO_CON40, 0xE0),
	MT6323_REG_FIXED("ldo-vrf18", VRF18, MT6323_DIGLDO_CON45, 15, 1825000),
	MT6323_LDO("ldo-vm", VM, ldo_volt_table9,
		   MT6323_DIGLDO_CON47, 14, MT6323_DIGLDO_CON48, 0x30),
	MT6323_REG_FIXED("ldo-vio18", VIO18, MT6323_DIGLDO_CON49, 14, 1800000),
	MT6323_LDO("ldo-vcamd", VCAMD, ldo_volt_table10,
		   MT6323_DIGLDO_CON51, 14, MT6323_DIGLDO_CON52, 0x60),
	MT6323_REG_FIXED("ldo-vcamio", VCAMIO, MT6323_DIGLDO_CON53, 14, 1800000),
};

static int mt6323_regulator_probe(struct udevice *dev)
{
	struct mt6323_regulator_info *priv = dev_get_priv(dev);
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(mt6323_regulators); i++) {
		if (!strcmp(dev->name, mt6323_regulators[i].desc.of_match)) {
			*priv = mt6323_regulators[i];

			if (priv->vselctrl_reg) {
				ret = pmic_reg_read(dev->parent, priv->vselctrl_reg);
				if (ret < 0)
					return ret;
				if (ret & priv->vselctrl_mask)
					priv->desc.vsel_reg = priv->vselon_reg;
			}
			return 0;
		}
	}

	return -ENOENT;
}

static const struct dm_regulator_ops mt6323_regulator_ops = {
	.get_value  = mt6323_get_value,
	.set_value  = mt6323_set_value,
	.get_enable = mt6323_get_enable,
	.set_enable = mt6323_set_enable,
};

U_BOOT_DRIVER(mt6323_regulator) = {
	.name	   = MT6323_REGULATOR_DRIVER,
	.id	   = UCLASS_REGULATOR,
	.ops	   = &mt6323_regulator_ops,
	.probe	   = mt6323_regulator_probe,
	.priv_auto = sizeof(struct mt6323_regulator_info),
};
