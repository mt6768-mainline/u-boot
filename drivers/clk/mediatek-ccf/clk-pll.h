/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __DRV_CLK_MTK_PLL_H
#define __DRV_CLK_MTK_PLL_H

#include "clk-mtk.h"

struct mtk_pll_div_table {
	u32 div;
	unsigned long freq;
};

#define HAVE_RST_BAR BIT(0)
#define PLL_AO BIT(1)
#define POSTDIV_MASK GENMASK(2, 0)

struct mtk_pll_data {
	int id;
	const char *name;
	u32 reg;
	u32 pwr_reg;
	u32 en_mask;
	u32 fenc_sta_ofs;
	u32 pd_reg;
	u32 tuner_reg;
	u32 tuner_en_reg;
	u8 tuner_en_bit;
	int pd_shift;
	unsigned int flags;
	u32 rst_bar_mask;
	unsigned long fmin;
	unsigned long fmax;
	int pcwbits;
	int pcwibits;
	u32 pcw_reg;
	int pcw_shift;
	u32 pcw_chg_reg;
	const struct mtk_pll_div_table *div_table;
	u32 en_reg;
	u32 en_set_reg;
	u32 en_clr_reg;
	u8 pll_en_bit;
	u8 pcw_chg_bit;
	u8 fenc_sta_bit;
};

struct mtk_clk_pll {
	struct clk clk;
	void __iomem *base_addr;
	void __iomem *pd_addr;
	void __iomem *pwr_addr;
	void __iomem *tuner_addr;
	void __iomem *tuner_en_addr;
	void __iomem *pcw_addr;
	void __iomem *pcw_chg_addr;
	void __iomem *en_addr;
	void __iomem *en_set_addr;
	void __iomem *en_clr_addr;
	void __iomem *fenc_addr;
	const struct mtk_pll_data *data;
};

int mtk_clk_register_plls(struct udevice *dev, const struct mtk_pll_data *plls,
			  int num_plls, struct mtk_clk_priv *priv);

#endif /* __DRV_CLK_MTK_PLL_H */
