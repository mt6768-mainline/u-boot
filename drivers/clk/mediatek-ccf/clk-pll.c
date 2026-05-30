// SPDX-License-Identifier: GPL-2.0-only
#include <clk-uclass.h>
#include <dm.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <div64.h>
#include <malloc.h>
#include "clk-mtk.h"
#include "clk-pll.h"

#define MHZ (1000 * 1000)
#define REG_CON0 0
#define REG_CON1 4
#define CON0_BASE_EN BIT(0)
#define CON0_PWR_ON BIT(0)
#define CON0_ISO_EN BIT(1)
#define PCW_CHG_BIT 31
#define AUDPLL_TUNER_EN BIT(31)
#define INTEGER_BITS 7

static inline struct mtk_clk_pll *to_mtk_clk_pll(struct clk *clk)
{
	return container_of(clk, struct mtk_clk_pll, clk);
}

static void __mtk_pll_tuner_enable(struct mtk_clk_pll *pll)
{
	u32 r;

	if (pll->tuner_en_addr) {
		r = readl(pll->tuner_en_addr) | BIT(pll->data->tuner_en_bit);
		writel(r, pll->tuner_en_addr);
	} else if (pll->tuner_addr) {
		r = readl(pll->tuner_addr) | AUDPLL_TUNER_EN;
		writel(r, pll->tuner_addr);
	}
}

static void __mtk_pll_tuner_disable(struct mtk_clk_pll *pll)
{
	u32 r;
	if (pll->tuner_en_addr) {
		r = readl(pll->tuner_en_addr) & ~BIT(pll->data->tuner_en_bit);
		writel(r, pll->tuner_en_addr);
	} else if (pll->tuner_addr) {
		r = readl(pll->tuner_addr) & ~AUDPLL_TUNER_EN;
		writel(r, pll->tuner_addr);
	}
}

static void mtk_pll_set_rate_regs(struct mtk_clk_pll *pll, u32 pcw, int postdiv)
{
	u32 chg, val;

	/* disable tuner */
	__mtk_pll_tuner_disable(pll);

	/* set postdiv */
	val = readl(pll->pd_addr);
	val &= ~(POSTDIV_MASK << pll->data->pd_shift);
	val |= (ffs(postdiv) - 1) << pll->data->pd_shift;

	/* postdiv and pcw need to set at the same time if on same register */
	if (pll->pd_addr != pll->pcw_addr) {
		writel(val, pll->pd_addr);
		val = readl(pll->pcw_addr);
	}

	/* set pcw */
	val &= ~GENMASK(pll->data->pcw_shift + pll->data->pcwbits - 1,
			pll->data->pcw_shift);
	val |= pcw << pll->data->pcw_shift;
	writel(val, pll->pcw_addr);
	chg = readl(pll->pcw_chg_addr) |
	      BIT(pll->data->pcw_chg_bit ?: PCW_CHG_BIT);
	writel(chg, pll->pcw_chg_addr);
	if (pll->tuner_addr)
		writel(val + 1, pll->tuner_addr);

	/* restore tuner_en */
	__mtk_pll_tuner_enable(pll);

	udelay(20);
}

static void mtk_pll_calc_values(struct mtk_clk_pll *pll, u32 *pcw, u32 *postdiv,
				u32 freq, u32 fin)
{
	unsigned long fmin = pll->data->fmin ? pll->data->fmin : (1000 * MHZ);
	const struct mtk_pll_div_table *div_table = pll->data->div_table;
	u64 _pcw;
	int ibits;
	u32 val;

	if (freq > pll->data->fmax)
		freq = pll->data->fmax;

	if (div_table) {
		if (freq > div_table[0].freq)
			freq = div_table[0].freq;

		for (val = 0; div_table[val + 1].freq != 0; val++) {
			if (freq > div_table[val + 1].freq)
				break;
		}
		*postdiv = 1 << val;
	} else {
		for (val = 0; val < 5; val++) {
			*postdiv = 1 << val;
			if ((u64)freq * *postdiv >= fmin)
				break;
		}
	}

	/* _pcw = freq * postdiv / fin * 2^pcwfbits */
	ibits = pll->data->pcwibits ? pll->data->pcwibits : INTEGER_BITS;
	_pcw = ((u64)freq << val) << (pll->data->pcwbits - ibits);
	do_div(_pcw, fin);

	*pcw = (u32)_pcw;
}

static ulong mtk_pll_set_rate(struct clk *clk, ulong rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(clk);
	ulong parent_rate = clk_get_parent_rate(clk);
	u32 pcw = 0;
	u32 postdiv;

	mtk_pll_calc_values(pll, &pcw, &postdiv, rate, parent_rate);
	mtk_pll_set_rate_regs(pll, pcw, postdiv);

	return rate;
}

static ulong mtk_pll_get_rate(struct clk *clk)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(clk);
	ulong parent_rate = clk_get_parent_rate(clk);
	u32 postdiv, pcw;
	int pcwbits = pll->data->pcwbits;
	int pcwfbits = 0, ibits;
	u64 vco;
	u8 c = 0;

	postdiv = (readl(pll->pd_addr) >> pll->data->pd_shift) & POSTDIV_MASK;
	postdiv = BIT(postdiv);

	pcw = readl(pll->pcw_addr) >> pll->data->pcw_shift;
	pcw &= GENMASK(pll->data->pcwbits - 1, 0);

	ibits = pll->data->pcwibits ? pll->data->pcwibits : INTEGER_BITS;
	if (pcwbits > ibits)
		pcwfbits = pcwbits - ibits;

	vco = (u64)parent_rate * pcw;

	if (pcwfbits && (vco & GENMASK(pcwfbits - 1, 0)))
		c = 1;

	vco >>= pcwfbits;
	if (c)
		vco++;

	return ((unsigned long)vco + postdiv - 1) / postdiv;
}

static int mtk_pll_enable(struct clk *clk)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(clk);
	u32 r;

	r = readl(pll->pwr_addr) | CON0_PWR_ON;
	writel(r, pll->pwr_addr);
	udelay(1);

	r = readl(pll->pwr_addr) & ~CON0_ISO_EN;
	writel(r, pll->pwr_addr);
	udelay(1);

	r = readl(pll->en_addr) | BIT(pll->data->pll_en_bit);
	writel(r, pll->en_addr);

	if (pll->data->en_mask) {
		r = readl(pll->base_addr + REG_CON0) | pll->data->en_mask;
		writel(r, pll->base_addr + REG_CON0);
	}

	__mtk_pll_tuner_enable(pll);

	udelay(20);

	if (pll->data->flags & HAVE_RST_BAR) {
		r = readl(pll->base_addr + REG_CON0);
		r |= pll->data->rst_bar_mask;
		writel(r, pll->base_addr + REG_CON0);
	}

	return 0;
}

static int mtk_pll_disable(struct clk *clk)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(clk);
	u32 r;

	if (pll->data->flags & HAVE_RST_BAR) {
		r = readl(pll->base_addr + REG_CON0);
		r &= ~pll->data->rst_bar_mask;
		writel(r, pll->base_addr + REG_CON0);
	}

	__mtk_pll_tuner_disable(pll);

	if (pll->data->en_mask) {
		r = readl(pll->base_addr + REG_CON0) & ~pll->data->en_mask;
		writel(r, pll->base_addr + REG_CON0);
	}

	r = readl(pll->en_addr) & ~BIT(pll->data->pll_en_bit);
	writel(r, pll->en_addr);

	r = readl(pll->pwr_addr) | CON0_ISO_EN;
	writel(r, pll->pwr_addr);

	r = readl(pll->pwr_addr) & ~CON0_PWR_ON;
	writel(r, pll->pwr_addr);

	return 0;
}

const struct clk_ops mtk_clk_pll_ops = {
	.enable = mtk_pll_enable,
	.disable = mtk_pll_disable,
	.get_rate = mtk_pll_get_rate,
	.set_rate = mtk_pll_set_rate,
};

U_BOOT_DRIVER(mtk_clk_pll) = {
	.name = "mtk_clk_pll",
	.id = UCLASS_CLK,
	.ops = &mtk_clk_pll_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

static struct clk *mtk_clk_register_pll(struct udevice *dev,
					const struct mtk_pll_data *data,
					void __iomem *base)
{
	struct mtk_clk_pll *pll;
	struct clk *clk;
	int ret;

	pll = calloc(1, sizeof(*pll));
	if (!pll)
		return ERR_PTR(-ENOMEM);

	pll->data = data;
	pll->base_addr = base + data->reg;
	pll->pwr_addr = base + data->pwr_reg;
	pll->pd_addr = base + data->pd_reg;
	pll->pcw_addr = base + data->pcw_reg;
	pll->pcw_chg_addr = data->pcw_chg_reg ? base + data->pcw_chg_reg :
						pll->base_addr + REG_CON1;
	pll->tuner_addr = data->tuner_reg ? base + data->tuner_reg : NULL;
	pll->tuner_en_addr = (data->tuner_en_reg || data->tuner_en_bit) ?
				     base + data->tuner_en_reg :
				     NULL;
	pll->en_addr = data->en_reg ? base + data->en_reg :
				      pll->base_addr + REG_CON0;
	pll->en_set_addr = data->en_set_reg ? base + data->en_set_reg : NULL;
	pll->en_clr_addr = data->en_clr_reg ? base + data->en_clr_reg : NULL;
	pll->fenc_addr = base + data->fenc_sta_ofs;

	clk = &pll->clk;
	clk->id = pll->data->id;
	clk->flags = pll->data->flags;

	ret = clk_register(clk, "mtk_clk_pll", data->name, "clk26m");
	if (ret) {
		free(pll);
		return ERR_PTR(ret);
	}

	return clk;
}

int mtk_clk_register_plls(struct udevice *dev, const struct mtk_pll_data *plls,
			  int num_plls, struct mtk_clk_priv *priv)
{
	void __iomem *base = dev_read_addr_ptr(dev);
	int i;

	for (i = 0; i < num_plls; i++) {
		const struct mtk_pll_data *pll = &plls[i];
		struct clk *clk;

		if (priv->hws[pll->id])
			continue;

		clk = mtk_clk_register_pll(dev, pll, base);
		if (IS_ERR(clk)) {
			printf("Failed to register pll %s\n", pll->name);
			continue;
		}

		priv->hws[pll->id] = clk;
	}

	return 0;
}
