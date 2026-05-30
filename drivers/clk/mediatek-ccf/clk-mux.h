/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __DRV_CLK_MTK_MUX_H
#define __DRV_CLK_MTK_MUX_H

#include "clk-mtk.h"

struct udevice;

struct mtk_mux {
	int id;
	const char *name;
	const char * const *parent_names;
	const u8 *parent_index;
	unsigned int flags;

	u32 mux_ofs;
	u32 set_ofs;
	u32 clr_ofs;
	u32 upd_ofs;

	u8 mux_shift;
	u8 mux_width;
	u8 gate_shift;
	s8 upd_shift;

	const char * const drv_name;
	signed char num_parents;
};

#define __GATE_CLR_SET_UPD_FLAGS(_id, _name, _parents, _paridx,		\
			 _num_parents, _mux_ofs, _mux_set_ofs,		\
			 _mux_clr_ofs, _shift, _width, _gate, _upd_ofs,	\
			 _upd, _flags, _drv_name) {				\
		.id = _id,						\
		.name = _name,						\
		.mux_ofs = _mux_ofs,					\
		.set_ofs = _mux_set_ofs,				\
		.clr_ofs = _mux_clr_ofs,				\
		.upd_ofs = _upd_ofs,					\
		.mux_shift = _shift,					\
		.mux_width = _width,					\
		.gate_shift = _gate,					\
		.upd_shift = _upd,					\
		.parent_names = _parents,				\
		.parent_index = _paridx,				\
		.num_parents = _num_parents,				\
		.flags = _flags,					\
		.drv_name = _drv_name,						\
	}

#define GATE_CLR_SET_UPD_FLAGS(_id, _name, _parents, _mux_ofs,		\
			_mux_set_ofs, _mux_clr_ofs, _shift, _width,	\
			_gate, _upd_ofs, _upd, _flags, _drv_name)		\
		__GATE_CLR_SET_UPD_FLAGS(_id, _name, _parents,		\
			NULL, ARRAY_SIZE(_parents), _mux_ofs,		\
			_mux_set_ofs, _mux_clr_ofs, _shift, _width,	\
			_gate, _upd_ofs, _upd, _flags, _drv_name)		\

#define GATE_CLR_SET_UPD_FLAGS_INDEXED(_id, _name, _parents, _paridx,	\
			 _mux_ofs, _mux_set_ofs, _mux_clr_ofs, _shift,	\
			 _width, _gate, _upd_ofs, _upd, _flags, _drv_name)	\
		__GATE_CLR_SET_UPD_FLAGS(_id, _name, _parents,		\
			_paridx, ARRAY_SIZE(_paridx), _mux_ofs,		\
			_mux_set_ofs, _mux_clr_ofs, _shift, _width,	\
			_gate, _upd_ofs, _upd, _flags, _drv_name)		\

#define MUX_GATE_CLR_SET_UPD_FLAGS(_id, _name, _parents, _mux_ofs,	\
			_mux_set_ofs, _mux_clr_ofs, _shift, _width,	\
			_gate, _upd_ofs, _upd, _flags)			\
		GATE_CLR_SET_UPD_FLAGS(_id, _name, _parents, _mux_ofs,	\
			_mux_set_ofs, _mux_clr_ofs, _shift, _width,	\
			_gate, _upd_ofs, _upd, _flags,			\
			"mtk_mux_gate_clr_set_upd")

#define MUX_GATE_CLR_SET_UPD_FLAGS_INDEXED(_id, _name, _parents,	\
			_paridx, _mux_ofs, _mux_set_ofs, _mux_clr_ofs,	\
			_shift, _width, _gate, _upd_ofs, _upd, _flags)	\
		GATE_CLR_SET_UPD_FLAGS_INDEXED(_id, _name, _parents,	\
			_paridx, _mux_ofs, _mux_set_ofs, _mux_clr_ofs,	\
			_shift, _width, _gate, _upd_ofs, _upd, _flags,	\
			"mtk_mux_gate_clr_set_upd")

#define MUX_GATE_CLR_SET_UPD(_id, _name, _parents, _mux_ofs,		\
			_mux_set_ofs, _mux_clr_ofs, _shift, _width,	\
			_gate, _upd_ofs, _upd)				\
		MUX_GATE_CLR_SET_UPD_FLAGS(_id, _name, _parents,	\
			_mux_ofs, _mux_set_ofs, _mux_clr_ofs, _shift,	\
			_width, _gate, _upd_ofs, _upd,			\
			CLK_SET_RATE_PARENT)

#define MUX_GATE_CLR_SET_UPD_INDEXED(_id, _name, _parents, _paridx,	\
			_mux_ofs, _mux_set_ofs, _mux_clr_ofs, _shift,	\
			_width, _gate, _upd_ofs, _upd)			\
		MUX_GATE_CLR_SET_UPD_FLAGS_INDEXED(_id, _name,		\
			_parents, _paridx, _mux_ofs, _mux_set_ofs,	\
			_mux_clr_ofs, _shift, _width, _gate, _upd_ofs,	\
			_upd, CLK_SET_RATE_PARENT)

#define MUX_CLR_SET_UPD(_id, _name, _parents, _mux_ofs,			\
			_mux_set_ofs, _mux_clr_ofs, _shift, _width,	\
			_upd_ofs, _upd)					\
		GATE_CLR_SET_UPD_FLAGS(_id, _name, _parents, _mux_ofs,	\
			_mux_set_ofs, _mux_clr_ofs, _shift, _width,	\
			0, _upd_ofs, _upd, CLK_SET_RATE_PARENT,		\
			"mtk_mux_clr_set_upd")

int mtk_clk_register_muxes(struct udevice *dev,
			   const struct mtk_mux *muxes,
			   int num, struct mtk_clk_priv *priv);

#endif /* __DRV_CLK_MTK_MUX_H */
