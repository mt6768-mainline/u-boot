/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __DRV_CLK_GATE_H
#define __DRV_CLK_GATE_H

#include "clk-mtk.h"

struct mtk_gate_regs {
	u32 sta_ofs;
	u32 clr_ofs;
	u32 set_ofs;
};

struct mtk_gate {
	int id;
	const char *name;
	const char *parent_name;
	const struct mtk_gate_regs *regs;
	int shift;
	const char *const drv_name;
	unsigned long flags;
};

#define GATE_MTK_FLAGS(_id, _name, _parent, _regs, _shift, _drv_name, _flags) \
	{                                                                     \
		.id = _id,                                                    \
		.name = _name,                                                \
		.parent_name = _parent,                                       \
		.regs = _regs,                                                \
		.shift = _shift,                                              \
		.drv_name = _drv_name,                                        \
		.flags = _flags,                                              \
	}

#define GATE_MTK(_id, _name, _parent, _regs, _shift, _drv_name) \
	GATE_MTK_FLAGS(_id, _name, _parent, _regs, _shift, _drv_name, 0)

int mtk_clk_register_gates(struct udevice *dev, const struct mtk_gate *clks,
			   int num, struct mtk_clk_priv *priv);

#endif /* __DRV_CLK_GATE_H */
