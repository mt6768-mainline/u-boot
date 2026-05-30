/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __DRV_CLK_MTK_H
#define __DRV_CLK_MTK_H

#include <linux/clk-provider.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <dm/device.h>

#define MAX_MUX_GATE_BIT 31
#define INVALID_MUX_GATE_BIT (MAX_MUX_GATE_BIT + 1)
#define MHZ (1000 * 1000)
#define MTK_WAIT_HWV_DONE_US 30
#define CLK_DUMMY 0

struct mtk_clk_priv {
	struct clk **hws;
	const struct clk_ops **hw_ops;
	int num_hws;
};

struct mtk_fixed_clk {
	int id;
	const char *name;
	const char *parent;
	unsigned long rate;
};

#define FIXED_CLK(_id, _name, _parent, _rate) \
	{                                     \
		.id = _id,                    \
		.name = _name,                \
		.parent = _parent,            \
		.rate = _rate,                \
	}

int mtk_clk_register_fixed_clks(struct udevice *dev,
				const struct mtk_fixed_clk *clks, int num,
				struct mtk_clk_priv *priv);

struct mtk_fixed_factor {
	int id;
	const char *name;
	const char *parent_name;
	int mult;
	int div;
	unsigned long flags;
};

#define FACTOR_FLAGS(_id, _name, _parent, _mult, _div, _fl) \
	{                                                   \
		.id = _id,                                  \
		.name = _name,                              \
		.parent_name = _parent,                     \
		.mult = _mult,                              \
		.div = _div,                                \
		.flags = _fl,                               \
	}

#define FACTOR(_id, _name, _parent, _mult, _div) \
	FACTOR_FLAGS(_id, _name, _parent, _mult, _div, CLK_SET_RATE_PARENT)

int mtk_clk_register_factors(struct udevice *dev,
			     const struct mtk_fixed_factor *clks, int num,
			     struct mtk_clk_priv *priv);

struct mtk_composite {
	int id;
	const char *name;
	const char *const *parent_names;
	const char *parent;
	unsigned flags;

	uint32_t mux_reg;
	uint32_t divider_reg;
	uint32_t gate_reg;

	signed char mux_shift;
	signed char mux_width;
	signed char gate_shift;

	signed char divider_shift;
	signed char divider_width;

	u8 mux_flags;
	signed char num_parents;
};

#define MUX_GATE_FLAGS_2(_id, _name, _parents, _reg, _shift, _width, _gate, \
			 _flags, _muxflags)                                 \
	{                                                                   \
		.id = _id,                                                  \
		.name = _name,                                              \
		.mux_reg = _reg,                                            \
		.mux_shift = _shift,                                        \
		.mux_width = _width,                                        \
		.gate_reg = _reg,                                           \
		.gate_shift = _gate,                                        \
		.divider_shift = -1,                                        \
		.parent_names = _parents,                                   \
		.num_parents = ARRAY_SIZE(_parents),                        \
		.flags = _flags,                                            \
		.mux_flags = _muxflags,                                     \
	}

#define MUX_GATE_FLAGS(_id, _name, _parents, _reg, _shift, _width, _gate,   \
		       _flags)                                              \
	MUX_GATE_FLAGS_2(_id, _name, _parents, _reg, _shift, _width, _gate, \
			 _flags, 0)

#define MUX_GATE(_id, _name, _parents, _reg, _shift, _width, _gate)       \
	MUX_GATE_FLAGS(_id, _name, _parents, _reg, _shift, _width, _gate, \
		       CLK_SET_RATE_PARENT)

#define MUX_FLAGS(_id, _name, _parents, _reg, _shift, _width, _flags) \
	{                                                             \
		.id = _id,                                            \
		.name = _name,                                        \
		.mux_reg = _reg,                                      \
		.mux_shift = _shift,                                  \
		.mux_width = _width,                                  \
		.gate_shift = -1,                                     \
		.divider_shift = -1,                                  \
		.parent_names = _parents,                             \
		.num_parents = ARRAY_SIZE(_parents),                  \
		.flags = _flags,                                      \
	}

#define MUX(_id, _name, _parents, _reg, _shift, _width)       \
	MUX_FLAGS(_id, _name, _parents, _reg, _shift, _width, \
		  CLK_SET_RATE_PARENT)

#define DIV_GATE(_id, _name, _parent, _gate_reg, _gate_shift, _div_reg, \
		 _div_width, _div_shift)                                \
	{                                                               \
		.id = _id,                                              \
		.parent = _parent,                                      \
		.name = _name,                                          \
		.divider_reg = _div_reg,                                \
		.divider_shift = _div_shift,                            \
		.divider_width = _div_width,                            \
		.gate_reg = _gate_reg,                                  \
		.gate_shift = _gate_shift,                              \
		.mux_shift = -1,                                        \
		.flags = 0,                                             \
	}

int mtk_clk_register_composites(struct udevice *dev,
				const struct mtk_composite *mcs, int num,
				void __iomem *base, struct mtk_clk_priv *priv);

struct mtk_clk_divider {
	int id;
	const char *name;
	const char *parent_name;
	unsigned long flags;

	u32 div_reg;
	unsigned char div_shift;
	unsigned char div_width;
	unsigned char clk_divider_flags;
	const struct clk_div_table *clk_div_table;
};

#define DIV_ADJ(_id, _name, _parent, _reg, _shift, _width) \
	{                                                  \
		.id = _id,                                 \
		.name = _name,                             \
		.parent_name = _parent,                    \
		.div_reg = _reg,                           \
		.div_shift = _shift,                       \
		.div_width = _width,                       \
	}

int mtk_clk_register_dividers(struct udevice *dev,
			      const struct mtk_clk_divider *mcds, int num,
			      void __iomem *base, struct mtk_clk_priv *priv);

struct clk *mtk_clk_register_ref2usb_tx(struct udevice *dev, const char *name,
					const char *parent_name,
					void __iomem *reg);

#endif /* __DRV_CLK_MTK_H */
