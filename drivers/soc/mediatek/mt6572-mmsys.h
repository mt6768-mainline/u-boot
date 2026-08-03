/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026 Roman Vivchar <rva333@protonmail.com>
 */

#ifndef __SOC_MEDIATEK_MT6572_MMSYS_H
#define __SOC_MEDIATEK_MT6572_MMSYS_H

#define MT6572_MMSYS_SW_RST_B	0x138

/* OVL output */
#define DISP_OVL_MOUT_EN	0x30
#define DISP_OVL_MOUT_EN_RDMA	BIT(0)
#define DISP_OVL_MOUT_EN_WDMA	BIT(1)
#define DISP_OVL_MOUT_EN_COLOR	BIT(2)

/* RDMA output */
#define DISP_RDMA_OUT_SEL	0x50
#define DISP_RDMA_OUT_SEL_COLOR	0
#define DISP_RDMA_OUT_SEL_DSI	1
#define DISP_RDMA_OUT_SEL_MASK	1

/* COLOR input */
#define DISP_COLOR_SEL		0x54
#define DISP_COLOR_SEL_RDMA	0
#define DISP_COLOR_SEL_OVL	1
#define DISP_COLOR_SEL_MASK	1

/* DSI input */
#define DISP_DSI_SEL		0x58
#define DISP_DSI_SEL_BLS	0
#define DISP_DSI_SEL_RDMA	1
#define DISP_DSI_SEL_MASK	1

/* DPI input */
#define DISP_DPI_SEL		0x5c
#define DISP_DPI_SEL_BLS	0
#define DISP_DPI_SEL_RDMA	1
#define DISP_DPI_SEL_MASK	1

/* Display output */
#define DISP_OUT_SEL		0x4c
#define DISP_OUT_SEL_DSI	0
#define DISP_OUT_SEL_DPI	1
#define DISP_OUT_SEL_MASK	3

/* MT6572 has few possible paths:
 * 1. OVL -> RDMA -> DSI
 * 2. OVL -> RDMA -> COLOR ->(1) BLS -> {DSI, DPI, DBI}
 * 3. OVL -> WDMA (2)
 *
 * (1) - This part seems to be hardwired, there's no configuration register
 * for COLOR output neither for BLS input.
 *
 * (2) - both OVL -> RDMA -> ... and OVL -> WDMA connections are possible
 * at the same time.
 */
static const struct mtk_mmsys_routes mt6572_mmsys_routing_table[] = {
	MMSYS_ROUTE(OVL, 0, RDMA, 0, DISP_OVL_MOUT_EN, DISP_OVL_MOUT_EN_RDMA,
	            DISP_OVL_MOUT_EN_RDMA),
	MMSYS_ROUTE(OVL, 0, WDMA, 0, DISP_OVL_MOUT_EN, DISP_OVL_MOUT_EN_WDMA,
	            DISP_OVL_MOUT_EN_WDMA),
	MMSYS_ROUTE(OVL, 0, COLOR, 0, DISP_OVL_MOUT_EN, DISP_OVL_MOUT_EN_COLOR,
	            DISP_OVL_MOUT_EN_COLOR),

	MMSYS_ROUTE(RDMA, 0, COLOR, 0, DISP_RDMA_OUT_SEL, DISP_RDMA_OUT_SEL_MASK,
	            DISP_RDMA_OUT_SEL_COLOR),
	MMSYS_ROUTE(RDMA, 0, DSI, 0, DISP_RDMA_OUT_SEL, DISP_RDMA_OUT_SEL_MASK,
	            DISP_RDMA_OUT_SEL_DSI),

	MMSYS_ROUTE(RDMA, 0, COLOR, 0, DISP_COLOR_SEL, DISP_COLOR_SEL_MASK,
	            DISP_COLOR_SEL_RDMA),
	MMSYS_ROUTE(OVL, 0, COLOR, 0, DISP_COLOR_SEL, DISP_COLOR_SEL_MASK,
	            DISP_COLOR_SEL_OVL),

	MMSYS_ROUTE(BLS, 0, DSI, 0, DISP_DSI_SEL, DISP_DSI_SEL_MASK, DISP_DSI_SEL_BLS),
	MMSYS_ROUTE(RDMA, 0, DSI, 0, DISP_DSI_SEL, DISP_DSI_SEL_MASK, DISP_DSI_SEL_RDMA),

	MMSYS_ROUTE(BLS, 0, DPI, 0, DISP_DPI_SEL, DISP_DPI_SEL_MASK, DISP_DPI_SEL_BLS),
	MMSYS_ROUTE(RDMA, 0, DPI, 0, DISP_DPI_SEL, DISP_DPI_SEL_MASK, DISP_DPI_SEL_RDMA),

	/*
	 * Only BLS can choose output, other paths like OVL -> RDMA -> DSI
	 * always output only to the DSI
	 */
	MMSYS_ROUTE(BLS, 0, DSI, 0, DISP_OUT_SEL, DISP_OUT_SEL_MASK, DISP_OUT_SEL_DSI),
	MMSYS_ROUTE(BLS, 0, DPI, 0, DISP_OUT_SEL, DISP_OUT_SEL_MASK, DISP_OUT_SEL_DPI),
	/* DBI can be used too, but the driver doesn't support DBI */
};

#endif /* __SOC_MEDIATEK_MT6572_MMSYS_H */
