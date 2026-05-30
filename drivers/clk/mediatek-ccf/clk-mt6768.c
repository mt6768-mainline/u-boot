// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Copyright (C) 2022 Luka Panio.
 * Copyright (C) 2022 Jami Kettunen
 */

#include "dm/devres.h"
#include "dm/read.h"
#include "linux/kernel.h"
#include <linux/delay.h>

#include "clk-gate.h"
#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-pll.h"

#include <dt-bindings/clock/mediatek,mt6768-clk.h>

static const struct mtk_fixed_clk top_fixed_clks[] = {
	FIXED_CLK(CLK_TOP_F_FRTC, "f_frtc_ck", "clk32k", 32768),
	FIXED_CLK(CLK_TOP_CLK26M, "clk_26m_ck", "clk26m", 26000000),
	FIXED_CLK(CLK_TOP_F_F26M, "f_f26m_ck", "clk_26m_ck", 26000000),
	FIXED_CLK(CLK_TOP_DMPLL, "dmpll_ck", NULL, 466000000),
	FIXED_CLK(CLK_TOP_CLK13M, "clk13m", "clk_26m_ck", 13000000),
	FIXED_CLK(CLK_TOP_ULPOSC1, "ulposc1", NULL, 260000000),
};

static const struct mtk_fixed_factor top_pll_divs[] = {
	FACTOR(CLK_TOP_SYSPLL, "syspll_ck", "mainpll", 1, 1),
	FACTOR(CLK_TOP_SYSPLL_D2, "syspll_d2", "mainpll", 1, 2),
	FACTOR(CLK_TOP_SYSPLL1_D2, "syspll1_d2", "syspll_d2", 1, 2),
	FACTOR(CLK_TOP_SYSPLL1_D4, "syspll1_d4", "syspll_d2", 1, 4),
	FACTOR(CLK_TOP_SYSPLL1_D8, "syspll1_d8", "syspll_d2", 1, 8),
	FACTOR(CLK_TOP_SYSPLL1_D16, "syspll1_d16", "syspll_d2", 1, 16),
	FACTOR(CLK_TOP_SYSPLL_D3, "syspll_d3", "mainpll", 1, 3),
	FACTOR(CLK_TOP_SYSPLL2_D2, "syspll2_d2", "syspll_d3", 1, 2),
	FACTOR(CLK_TOP_SYSPLL2_D4, "syspll2_d4", "syspll_d3", 1, 4),
	FACTOR(CLK_TOP_SYSPLL2_D8, "syspll2_d8", "syspll_d3", 1, 8),
	FACTOR(CLK_TOP_SYSPLL_D5, "syspll_d5", "mainpll", 1, 5),
	FACTOR(CLK_TOP_SYSPLL3_D2, "syspll3_d2", "syspll_d5", 1, 2),
	FACTOR(CLK_TOP_SYSPLL3_D4, "syspll3_d4", "syspll_d5", 1, 4),
	FACTOR(CLK_TOP_SYSPLL_D7, "syspll_d7", "mainpll", 1, 7),
	FACTOR(CLK_TOP_SYSPLL4_D2, "syspll4_d2", "syspll_d7", 1, 2),
	FACTOR(CLK_TOP_SYSPLL4_D4, "syspll4_d4", "syspll_d7", 1, 4),

	FACTOR(CLK_TOP_UNIVPLL, "univpll", "univ2pll", 1, 2),
	FACTOR(CLK_TOP_USB20_192M, "usb20_192m_ck", "univpll", 2, 13),
	FACTOR(CLK_TOP_USB20_192M_D4, "usb20_192m_d4", "usb20_192m_ck", 1, 4),
	FACTOR(CLK_TOP_USB20_192M_D8, "usb20_192m_d8", "usb20_192m_ck", 1, 8),
	FACTOR(CLK_TOP_USB20_192M_D16, "usb20_192m_d16", "usb20_192m_ck", 1,
	       16),
	FACTOR(CLK_TOP_USB20_192M_D32, "usb20_192m_d32", "usb20_192m_ck", 1,
	       32),
	FACTOR(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL1_D2, "univpll1_d2", "univpll_d2", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL1_D4, "univpll1_d4", "univpll_d2", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3", "univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL2_D2, "univpll2_d2", "univpll_d3", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL2_D4, "univpll2_d4", "univpll_d3", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL2_D8, "univpll2_d8", "univpll_d3", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL2_D32, "univpll2_d32", "univpll_d3", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL3_D2, "univpll3_d2", "univpll_d5", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL3_D4, "univpll3_d4", "univpll_d5", 1, 4),
	FACTOR(CLK_TOP_MMPLL, "mmpll_ck", "mmpll", 1, 1),
	FACTOR(CLK_TOP_MMPLL_D2, "mmpll_d2", "mmpll_ck", 1, 2),
	FACTOR(CLK_TOP_MPLL, "mpll_ck", "mpll", 1, 1),
	FACTOR(CLK_TOP_DA_MPLL_104M_DIV, "mpll_104m_div", "mpll_ck", 1, 2),
	FACTOR(CLK_TOP_DA_MPLL_52M_DIV, "mpll_52m_div", "mpll_ck", 1, 4),
	FACTOR(CLK_TOP_MFGPLL, "mfgpll_ck", "mfgpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck", "msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll_ck", 1, 2),
	FACTOR(CLK_TOP_APLL1, "apll1_ck", "apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2", "apll1_ck", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4", "apll1_ck", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8", "apll1_ck", 1, 8),
	FACTOR(CLK_TOP_ULPOSC1, "ulposc1_ck", "ulposc1", 1, 1),
	FACTOR(CLK_TOP_ULPOSC1_D2, "ulposc1_d2", "ulposc1_ck", 1, 2),
	FACTOR(CLK_TOP_ULPOSC1_D4, "ulposc1_d4", "ulposc1_ck", 1, 4),
	FACTOR(CLK_TOP_ULPOSC1_D8, "ulposc1_d8", "ulposc1_ck", 1, 8),
	FACTOR(CLK_TOP_ULPOSC1_D16, "ulposc1_d16", "ulposc1_ck", 1, 16),
	FACTOR(CLK_TOP_ULPOSC1_D32, "ulposc1_d32", "ulposc1_ck", 1, 32),
	FACTOR(CLK_TOP_F_BIST2FPC, "f_bist2fpc_ck", "univpll2_d2", 1, 1),
	FACTOR(CLK_TOP_ARMPLL_DIVIDER_PLL0, "arm_div_pll0", "armpll", 1, 1),
	FACTOR(CLK_TOP_ARMPLL_DIVIDER_PLL1, "arm_div_pll1", "armpll_l", 1, 1),
	FACTOR(CLK_TOP_ARMPLL_DIVIDER_PLL2, "arm_div_pll2", "ccipll", 1, 1),
	FACTOR(CLK_TOP_DA_USB20_48M_DIV, "usb20_48m_div", "usb20_192m_d4", 1,
	       1),
	FACTOR(CLK_TOP_DA_UNIV_48M_DIV, "univ_48m_div", "univpll_d2", 1, 1),
};

static const struct mtk_fixed_factor top_bus_divs[] = {
	FACTOR(CLK_TOP_AXI, "axi_ck", "axi_sel", 1, 1),
	FACTOR(CLK_TOP_MM, "mm_ck", "mm_sel", 1, 1),
	FACTOR(CLK_TOP_SCP, "scp_ck", "scp_sel", 1, 1),
	FACTOR(CLK_TOP_MFG, "mfg_ck", "mfg_sel", 1, 1),
	FACTOR(CLK_TOP_F_FUART, "f_fuart_ck", "uart_sel", 1, 1),
	FACTOR(CLK_TOP_SPI, "spi_ck", "spi_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC50_0_HCLK, "msdc50_0_hclk_ck", "msdc5hclk", 1, 1),
	FACTOR(CLK_TOP_MSDC50_0, "msdc50_0_ck", "msdc50_0_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1, "msdc30_1_ck", "msdc30_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUDIO, "audio_ck", "audio_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_1, "aud_1_ck", "aud_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN1, "aud_engen1_ck", "aud_engen1_sel", 1, 1),
	FACTOR(CLK_TOP_F_FDISP_PWM, "f_fdisp_pwm_ck", "disp_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_SSPM, "sspm_ck", "sspm_sel", 1, 1),
	FACTOR(CLK_TOP_DXCC, "dxcc_ck", "dxcc_sel", 1, 1),
	FACTOR(CLK_TOP_I2C, "i2c_ck", "i2c_sel", 1, 1),
	FACTOR(CLK_TOP_F_FPWM, "f_fpwm_ck", "pwm_sel", 1, 1),
	FACTOR(CLK_TOP_F_FSENINF, "f_fseninf_ck", "seninf_sel", 1, 1),
	FACTOR(CLK_TOP_AES_FDE, "aes_fde_ck", "aes_fde_sel", 1, 1),
	FACTOR(CLK_TOP_VENC, "venc_ck", "venc_sel", 1, 1),
	FACTOR(CLK_TOP_CAM, "cam_ck", "cam_sel", 1, 1),
};

static const char *const axi_parents[] = { "clk26m", "syspll_d7", "syspll1_d4",
					   "syspll3_d2" };

static const char *const mem_parents[] = { "clk26m", "dmpll_ck", "apll1_ck" };

static const char *const mm_parents[] = { "clk26m",	 "mmpll_ck",
					  "syspll1_d2",	 "syspll_d5",
					  "syspll1_d4",	 "univpll_d5",
					  "univpll1_d2", "mmpll_d2" };

static const char *const scp_parents[] = { "clk26m",	  "syspll4_d2",
					   "univpll2_d2", "syspll1_d2",
					   "univpll1_d2", "syspll_d3",
					   "univpll_d3" };

static const char *const mfg_parents[] = { "clk26m", "mfgpll_ck", "syspll_d3",
					   "univpll_d3" };

static const char *const atb_parents[] = { "clk26m", "syspll1_d4",
					   "syspll1_d2" };

static const char *const camtg_parents[] = { "clk26m",	      "usb20_192m_d8",
					     "univpll2_d8",   "usb20_192m_d4",
					     "univpll2_d32",  "usb20_192m_d16",
					     "usb20_192m_d32" };

static const char *const uart_parents[] = { "clk26m", "univpll2_d8" };

static const char *const spi_parents[] = { "clk26m", "syspll3_d2", "syspll4_d2",
					   "syspll2_d4" };

static const char *const msdc5hclk_parents[] = { "clk26m", "syspll1_d2",
						 "univpll1_d4", "syspll2_d2" };

static const char *const msdc50_0_parents[] = { "clk26m",      "msdcpll_ck",
						"syspll2_d2",  "syspll4_d2",
						"univpll1_d2", "syspll1_d2",
						"univpll_d5",  "univpll1_d4" };

static const char *const msdc30_1_parents[] = { "clk26m",	 "msdcpll_d2",
						"univpll2_d2",	 "syspll2_d2",
						"syspll1_d4",	 "univpll1_d4",
						"usb20_192m_d4", "syspll2_d4" };

static const char *const audio_parents[] = { "clk26m", "syspll3_d4",
					     "syspll4_d4", "syspll1_d16" };

static const char *const aud_intbus_parents[] = { "clk26m", "syspll1_d4",
						  "syspll4_d2" };

static const char *const aud_1_parents[] = { "clk26m", "apll1_ck" };

static const char *const aud_engen1_parents[] = { "clk26m", "apll1_d2",
						  "apll1_d4", "apll1_d8" };

static const char *const disp_pwm_parents[] = { "clk26m", "univpll2_d4",
						"ulposc1_d2", "ulposc1_d8" };

static const char *const sspm_parents[] = { "clk26m", "syspll1_d2",
					    "syspll_d3" };

static const char *const dxcc_parents[] = { "clk26m", "syspll1_d2",
					    "syspll1_d4", "syspll1_d8" };

static const char *const usb_top_parents[] = { "clk26m", "univpll3_d4" };

static const char *const spm_parents[] = { "clk26m", "syspll1_d8" };

static const char *const i2c_parents[] = { "clk26m", "univpll3_d4",
					   "univpll3_d2", "syspll1_d8",
					   "syspll2_d8" };

static const char *const pwm_parents[] = { "clk26m", "univpll3_d4",
					   "syspll1_d8" };

static const char *const seninf_parents[] = { "clk26m", "univpll1_d4",
					      "univpll1_d2", "univpll2_d2" };

static const char *const aes_fde_parents[] = { "clk26m",      "msdcpll_ck",
					       "univpll_d3",  "univpll2_d2",
					       "univpll1_d2", "syspll1_d2" };

static const char *const ulposc_parents[] = { "clk26m", "ulposc1_d4",
					      "ulposc1_d8", "ulposc1_d16",
					      "ulposc1_d32" };

static const char *const camtm_parents[] = { "clk26m", "univpll1_d4",
					     "univpll1_d2", "univpll2_d2" };

static const char *const venc_parents[] = { "clk26m",	  "mmpll_ck",
					    "syspll1_d2", "syspll_d5",
					    "syspll1_d4", "syspll_d3",
					    "univpll_d3", "univpll1_d2" };

static const char *const cam_parents[] = { "clk26m",	  "syspll_d2",
					   "syspll1_d2",  "syspll_d5",
					   "mmpll_ck",	  "univpll_d5",
					   "univpll1_d2", "mmpll_d2" };

/*
 * CRITICAL CLOCK:
 * axi_sel is the main bus clock of whole SOC.
 * mem_sel is ???
 */
static const struct mtk_mux top_muxes[] = {
	/* CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_AXI_SEL, "axi_sel", axi_parents,
				   0x40, 0x44, 0x48, 0, 2, 7, 0x004, 0,
				   CLK_IS_CRITICAL),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_MEM_SEL, "mem_sel", mem_parents,
				   0x40, 0x44, 0x48, 8, 2, 15, 0x004, 1,
				   CLK_IS_CRITICAL),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MM_SEL, "mm_sel", mm_parents, 0x40, 0x44,
			     0x48, 16, 3, 23, 0x004, 2),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SCP_SEL, "scp_sel", scp_parents, 0x40,
			     0x44, 0x48, 24, 3, 31, 0x004, 3),
	/* CLK_CFG_1 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_SEL, "mfg_sel", mfg_parents, 0x50,
			     0x54, 0x58, 0, 2, 7, 0x004, 4),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ATB_SEL, "atb_sel", atb_parents, 0x50,
			     0x54, 0x58, 8, 2, 15, 0x004, 5),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG_SEL, "camtg_sel", camtg_parents,
			     0x50, 0x54, 0x58, 16, 3, 23, 0x004, 6),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG1_SEL, "camtg1_sel", camtg_parents,
			     0x50, 0x54, 0x58, 24, 3, 31, 0x004, 7),
	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL, "camtg2_sel", camtg_parents,
			     0x60, 0x64, 0x68, 0, 3, 7, 0x004, 8),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL, "camtg3_sel", camtg_parents,
			     0x60, 0x64, 0x68, 8, 3, 15, 0x004, 9),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL, "uart_sel", uart_parents, 0x60,
			     0x64, 0x68, 16, 1, 23, 0x004, 10),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI_SEL, "spi_sel", spi_parents, 0x60,
			     0x64, 0x68, 24, 2, 31, 0x004, 11),
	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL, "msdc5hclk",
			     msdc5hclk_parents, 0x70, 0x74, 0x78, 0, 2, 7,
			     0x004, 12),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL, "msdc50_0_sel",
			     msdc50_0_parents, 0x70, 0x74, 0x78, 8, 3, 15,
			     0x004, 13),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel",
			     msdc30_1_parents, 0x70, 0x74, 0x78, 16, 3, 23,
			     0x004, 14),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_SEL, "audio_sel", audio_parents,
			     0x70, 0x74, 0x78, 24, 2, 31, 0x004, 15),
	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel",
			     aud_intbus_parents, 0x80, 0x84, 0x88, 0, 2, 7,
			     0x004, 16),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL, "aud_1_sel", aud_1_parents,
			     0x80, 0x84, 0x88, 8, 1, 15, 0x004, 17),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL, "aud_engen1_sel",
			     aud_engen1_parents, 0x80, 0x84, 0x88, 16, 2, 23,
			     0x004, 18),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL, "disp_pwm_sel",
			     disp_pwm_parents, 0x80, 0x84, 0x88, 24, 2, 31,
			     0x004, 19),
	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSPM_SEL, "sspm_sel", sspm_parents, 0x90,
			     0x94, 0x98, 0, 2, 7, 0x004, 20),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DXCC_SEL, "dxcc_sel", dxcc_parents, 0x90,
			     0x94, 0x98, 8, 2, 15, 0x004, 21),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL, "usb_top_sel",
			     usb_top_parents, 0x90, 0x94, 0x98, 16, 1, 23,
			     0x004, 22),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPM_SEL, "spm_sel", spm_parents, 0x90,
			     0x94, 0x98, 24, 1, 31, 0x004, 23),
	/* CLK_CFG_6 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_I2C_SEL, "i2c_sel", i2c_parents, 0xa0,
			     0xa4, 0xa8, 0, 3, 7, 0x004, 24),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWM_SEL, "pwm_sel", pwm_parents, 0xa0,
			     0xa4, 0xa8, 8, 2, 15, 0x004, 25),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF_SEL, "seninf_sel", seninf_parents,
			     0xa0, 0xa4, 0xa8, 16, 2, 23, 0x004, 26),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_FDE_SEL, "aes_fde_sel",
			     aes_fde_parents, 0xa0, 0xa4, 0xa8, 24, 3, 31,
			     0x004, 27),
	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWRAP_ULPOSC_SEL, "ulposc_sel",
			     ulposc_parents, 0xb0, 0xb4, 0xb8, 0, 3, 7, 0x004,
			     28),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTM_SEL, "camtm_sel", camtm_parents,
			     0xb0, 0xb4, 0xb8, 8, 2, 15, 0x004, 29),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_VENC_SEL, "venc_sel", venc_parents, 0xb0,
			     0xb4, 0xb8, 16, 3, 23, 0x004, 30),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAM_SEL, "cam_sel", cam_parents, 0xb0,
			     0xb4, 0xb8, 24, 3, 31, 0x004, 31),
};

// TODO: do we need TOP0? only TOP1 below

static const struct mtk_gate_regs top0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs top1_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x104,
	.sta_ofs = 0x104,
};

#define GATE_TOP0(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &top0_cg_regs, _shift, "mtk_cg_no_setclr")

#define GATE_TOP1(_id, _name, _parent, _shift)               \
	GATE_MTK(_id, _name, _parent, &top1_cg_regs, _shift, \
		 "mtk_cg_no_setclr_inv")

static const struct mtk_gate top_clks[] = {
	/* TOP0 */
	GATE_TOP0(CLK_TOP_MD_32K, "md_32k", "f_frtc_ck", 8),
	GATE_TOP0(CLK_TOP_MD_26M, "md_26m", "f_f26m_ck", 9),
	GATE_TOP0(CLK_TOP_MD2_32K, "md2_32k", "f_frtc_ck", 10),
	GATE_TOP0(CLK_TOP_MD2_26M, "md2_26m", "f_f26m_ck", 11),
	/* TOP1 */
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL0_EN, "arm_div_pll0_en",
		  "arm_div_pll0", 3),
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL1_EN, "arm_div_pll1_en",
		  "arm_div_pll1", 4),
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL2_EN, "arm_div_pll2_en",
		  "arm_div_pll2", 5),
	GATE_TOP1(CLK_TOP_FMEM_OCC_DRC_EN, "drc_en", "univpll2_d2", 6),
	GATE_TOP1(CLK_TOP_USB20_48M_EN, "usb20_48m_en", "usb20_48m_div", 8),
	GATE_TOP1(CLK_TOP_UNIVPLL_48M_EN, "univpll_48m_en", "univ_48m_div", 9),
	GATE_TOP1(CLK_TOP_MPLL_104M_EN, "mpll_104m_en", "mpll_104m_div", 10),
	GATE_TOP1(CLK_TOP_MPLL_52M_EN, "mpll_52m_en", "mpll_52m_div", 11),
	GATE_TOP1(CLK_TOP_F_UFS_MP_SAP_CFG_EN, "ufs_sap", "f_f26m_ck", 12),
	GATE_TOP1(CLK_TOP_F_BIST2FPC_EN, "bist2fpc", "f_bist2fpc_ck", 16),
};

static const struct mtk_gate_regs infra0_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x200,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs infra1_cg_regs = {
	.set_ofs = 0x74,
	.clr_ofs = 0x74,
	.sta_ofs = 0x74,
};

static const struct mtk_gate_regs infra2_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs infra3_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs infra4_cg_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xa8,
	.sta_ofs = 0xac,
};

static const struct mtk_gate_regs infra5_cg_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0xc8,
};

#define GATE_INFRA0(_id, _name, _parent, _shift)               \
	GATE_MTK(_id, _name, _parent, &infra0_cg_regs, _shift, \
		 "mtk_cg_no_setclr_inv")

#define GATE_INFRA1(_id, _name, _parent, _shift)               \
	GATE_MTK(_id, _name, _parent, &infra1_cg_regs, _shift, \
		 "mtk_cg_no_setclr")

#define GATE_INFRA2(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &infra2_cg_regs, _shift, "mtk_cg_setclr")

#define GATE_INFRA3(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &infra3_cg_regs, _shift, "mtk_cg_setclr")

#define GATE_INFRA4(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &infra4_cg_regs, _shift, "mtk_cg_setclr")

#define GATE_INFRA5(_id, _name, _parent, _shift) \
	GATE_MTK(_id, _name, _parent, &infra5_cg_regs, _shift, "mtk_cg_setclr")

static const struct mtk_gate infra_clks[] = {
	/* INFRA0 */
	GATE_INFRA0(CLK_INFRA_TOPAXI_DISABLE, "ifr_axi_dis", "axi_ck", 31),
	/* INFRA1 */
	GATE_INFRA1(CLK_INFRA_PERI_DCM_RG_FORCE_CLKOFF, "ifr_dcmforce",
		    "axi_ck", 2),
	/* INFRA2 */
	GATE_INFRA2(CLK_INFRA_PMIC_TMR, "ifr_pmic_tmr", "f_f26m_ck", 0),
	GATE_INFRA2(CLK_INFRA_PMIC_AP, "ifr_pmic_ap", "f_f26m_ck", 1),
	GATE_INFRA2(CLK_INFRA_PMIC_MD, "ifr_pmic_md", "f_f26m_ck", 2),
	GATE_INFRA2(CLK_INFRA_PMIC_CONN, "ifr_pmic_conn", "f_f26m_ck", 3),
	GATE_INFRA2(CLK_INFRA_SCP_CORE, "ifr_scp_core", "scp_ck", 4),
	GATE_INFRA2(CLK_INFRA_SEJ, "ifr_sej", "axi_ck", 5),
	GATE_INFRA2(CLK_INFRA_APXGPT, "ifr_apxgpt", "axi_ck", 6),
	GATE_INFRA2(CLK_INFRA_ICUSB, "ifr_icusb", "axi_ck", 8),
	GATE_INFRA2(CLK_INFRA_GCE, "ifr_gce", "axi_ck", 9),
	GATE_INFRA2(CLK_INFRA_THERM, "ifr_therm", "axi_ck", 10),
	GATE_INFRA2(CLK_INFRA_I2C_AP, "ifr_i2c_ap", "i2c_ck", 11),
	GATE_INFRA2(CLK_INFRA_I2C_CCU, "ifr_i2c_ccu", "i2c_ck", 12),
	GATE_INFRA2(CLK_INFRA_I2C_SSPM, "ifr_i2c_sspm", "i2c_ck", 13),
	GATE_INFRA2(CLK_INFRA_I2C_RSV, "ifr_i2c_rsv", "i2c_ck", 14),
	GATE_INFRA2(CLK_INFRA_PWM_HCLK, "ifr_pwm_hclk", "axi_ck", 15),
	GATE_INFRA2(CLK_INFRA_PWM1, "ifr_pwm1", "f_fpwm_ck", 16),
	GATE_INFRA2(CLK_INFRA_PWM2, "ifr_pwm2", "f_fpwm_ck", 17),
	GATE_INFRA2(CLK_INFRA_PWM3, "ifr_pwm3", "f_fpwm_ck", 18),
	GATE_INFRA2(CLK_INFRA_PWM4, "ifr_pwm4", "f_fpwm_ck", 19),
	GATE_INFRA2(CLK_INFRA_PWM5, "ifr_pwm5", "f_fpwm_ck", 20),
	GATE_INFRA2(CLK_INFRA_PWM, "ifr_pwm", "f_fpwm_ck", 21),
	GATE_INFRA2(CLK_INFRA_UART0, "ifr_uart0", "f_fuart_ck", 22),
	GATE_INFRA2(CLK_INFRA_UART1, "ifr_uart1", "f_fuart_ck", 23),
	GATE_INFRA2(CLK_INFRA_GCE_26M, "ifr_gce_26m", "f_f26m_ck", 27),
	GATE_INFRA2(CLK_INFRA_CQ_DMA_FPC, "ifr_dma", "axi_ck", 28),
	GATE_INFRA2(CLK_INFRA_BTIF, "ifr_btif", "axi_ck", 31),
	/* INFRA3 */
	GATE_INFRA3(CLK_INFRA_SPI0, "ifr_spi0", "spi_ck", 1),
	GATE_INFRA3(CLK_INFRA_MSDC0, "ifr_msdc0", "msdc50_0_hclk_ck", 2),
	GATE_INFRA3(CLK_INFRA_MSDC1, "ifr_msdc1", "axi_ck", 4),
	GATE_INFRA3(CLK_INFRA_DVFSRC, "ifr_dvfsrc", "f_f26m_ck", 7),
	GATE_INFRA3(CLK_INFRA_GCPU, "ifr_gcpu", "axi_ck", 8),
	GATE_INFRA3(CLK_INFRA_TRNG, "ifr_trng", "axi_ck", 9),
	GATE_INFRA3(CLK_INFRA_AUXADC, "ifr_auxadc", "f_f26m_ck", 10),
	GATE_INFRA3(CLK_INFRA_CPUM, "ifr_cpum", "axi_ck", 11),
	GATE_INFRA3(CLK_INFRA_CCIF1_AP, "ifr_ccif1_ap", "axi_ck", 12),
	GATE_INFRA3(CLK_INFRA_CCIF1_MD, "ifr_ccif1_md", "axi_ck", 13),
	GATE_INFRA3(CLK_INFRA_AUXADC_MD, "ifr_auxadc_md", "f_f26m_ck", 14),
	GATE_INFRA3(CLK_INFRA_AP_DMA, "ifr_ap_dma", "axi_ck", 18),
	GATE_INFRA3(CLK_INFRA_XIU, "ifr_xiu", "axi_ck", 19),
	GATE_INFRA3(CLK_INFRA_DEVICE_APC, "ifr_dapc", "axi_ck", 20),
	GATE_INFRA3(CLK_INFRA_CCIF_AP, "ifr_ccif_ap", "axi_ck", 23),
	GATE_INFRA3(CLK_INFRA_DEBUGTOP, "ifr_debugtop", "axi_ck", 24),
	GATE_INFRA3(CLK_INFRA_AUDIO, "ifr_audio", "axi_ck", 25),
	GATE_INFRA3(CLK_INFRA_CCIF_MD, "ifr_ccif_md", "axi_ck", 26),
	GATE_INFRA3(CLK_INFRA_DXCC_SEC_CORE, "ifr_secore", "dxcc_ck", 27),
	GATE_INFRA3(CLK_INFRA_DXCC_AO, "ifr_dxcc_ao", "dxcc_ck", 28),
	GATE_INFRA3(CLK_INFRA_DRAMC_F26M, "ifr_dramc26", "f_f26m_ck", 31),
	/* INFRA4 */
	GATE_INFRA4(CLK_INFRA_RG_PWM_FBCLK6, "ifr_pwmfb", "f_f26m_ck", 0),
	GATE_INFRA4(CLK_INFRA_DISP_PWM, "ifr_disp_pwm", "f_fdisp_pwm_ck", 2),
	GATE_INFRA4(CLK_INFRA_CLDMA_BCLK, "ifr_cldmabclk", "axi_ck", 3),
	GATE_INFRA4(CLK_INFRA_AUDIO_26M_BCLK, "ifr_audio26m", "f_f26m_ck", 4),
	GATE_INFRA4(CLK_INFRA_SPI1, "ifr_spi1", "spi_ck", 6),
	GATE_INFRA4(CLK_INFRA_I2C4, "ifr_i2c4", "i2c_ck", 7),
	GATE_INFRA4(CLK_INFRA_MODEM_TEMP_SHARE, "ifr_mdtemp", "f_f26m_ck", 8),
	GATE_INFRA4(CLK_INFRA_SPI2, "ifr_spi2", "spi_ck", 9),
	GATE_INFRA4(CLK_INFRA_SPI3, "ifr_spi3", "spi_ck", 10),
	GATE_INFRA4(CLK_INFRA_SSPM, "ifr_hf_fsspm", "sspm_ck", 15),
	GATE_INFRA4(CLK_INFRA_I2C5, "ifr_i2c5", "i2c_ck", 18),
	GATE_INFRA4(CLK_INFRA_I2C5_ARBITER, "ifr_i2c5a", "i2c_ck", 19),
	GATE_INFRA4(CLK_INFRA_I2C5_IMM, "ifr_i2c5_imm", "i2c_ck", 20),
	GATE_INFRA4(CLK_INFRA_I2C1_ARBITER, "ifr_i2c1a", "i2c_ck", 21),
	GATE_INFRA4(CLK_INFRA_I2C1_IMM, "ifr_i2c1_imm", "i2c_ck", 22),
	GATE_INFRA4(CLK_INFRA_I2C2_ARBITER, "ifr_i2c2a", "i2c_ck", 23),
	GATE_INFRA4(CLK_INFRA_I2C2_IMM, "ifr_i2c2_imm", "i2c_ck", 24),
	GATE_INFRA4(CLK_INFRA_SPI4, "ifr_spi4", "spi_ck", 25),
	GATE_INFRA4(CLK_INFRA_SPI5, "ifr_spi5", "spi_ck", 26),
	GATE_INFRA4(CLK_INFRA_CQ_DMA, "ifr_cq_dma", "axi_ck", 27),
	GATE_INFRA4(CLK_INFRA_FAES_FDE, "ifr_faes_fde_ck", "aes_fde_ck", 29),
	/* INFRA5 */
	GATE_INFRA5(CLK_INFRA_MSDC0_SELF, "ifr_msdc0sf", "msdc50_0_ck", 0),
	GATE_INFRA5(CLK_INFRA_MSDC1_SELF, "ifr_msdc1sf", "msdc50_0_ck", 1),
	GATE_INFRA5(CLK_INFRA_SSPM_26M_SELF, "ifr_sspm_26m", "f_f26m_ck", 3),
	GATE_INFRA5(CLK_INFRA_SSPM_32K_SELF, "ifr_sspm_32k", "f_frtc_ck", 4),
	GATE_INFRA5(CLK_INFRA_I2C6, "ifr_i2c6", "i2c_ck", 6),
	GATE_INFRA5(CLK_INFRA_AP_MSDC0, "ifr_ap_msdc0", "msdc50_0_ck", 7),
	GATE_INFRA5(CLK_INFRA_MD_MSDC0, "ifr_md_msdc0", "msdc50_0_ck", 8),
	GATE_INFRA5(CLK_INFRA_MSDC0_SRC, "ifr_msdc0_clk", "msdc50_0_ck", 9),
	GATE_INFRA5(CLK_INFRA_MSDC1_SRC, "ifr_msdc1_clk", "msdc30_1_ck", 10),
	GATE_INFRA5(CLK_INFRA_SEJ_F13M, "ifr_sej_f13m", "f_f26m_ck", 15),
	GATE_INFRA5(CLK_INFRA_AES_TOP0_BCLK, "ifr_aes", "axi_ck", 16),
	GATE_INFRA5(CLK_INFRA_MCU_PM_BCLK, "ifr_mcu_pm_bclk", "axi_ck", 17),
	GATE_INFRA5(CLK_INFRA_CCIF2_AP, "ifr_ccif2_ap", "axi_ck", 18),
	GATE_INFRA5(CLK_INFRA_CCIF2_MD, "ifr_ccif2_md", "axi_ck", 19),
	GATE_INFRA5(CLK_INFRA_CCIF3_AP, "ifr_ccif3_ap", "axi_ck", 20),
	GATE_INFRA5(CLK_INFRA_CCIF3_MD, "ifr_ccif3_md", "axi_ck", 21),
};

static const struct mtk_gate_regs apmixed_cg_regs = {
	.set_ofs = 0x14,
	.clr_ofs = 0x14,
	.sta_ofs = 0x14,
};

#define GATE_APMIXED(_id, _name, _parent, _shift)               \
	GATE_MTK(_id, _name, _parent, &apmixed_cg_regs, _shift, \
		 "mtk_cg_no_setclr_inv")

static const struct mtk_gate apmixed_clks[] = {
	GATE_APMIXED(CLK_APMIXED_SSUSB26M, "apmixed_ssusb26m", "f_f26m_ck", 4),
	GATE_APMIXED(CLK_APMIXED_APPLL26M, "apmixed_appll26m", "f_f26m_ck", 5),
	GATE_APMIXED(CLK_APMIXED_MIPIC0_26M, "apmixed_mipic026m", "f_f26m_ck",
		     6),
	GATE_APMIXED(CLK_APMIXED_MDPLLGP26M, "apmixed_mdpll26m", "f_f26m_ck",
		     7),
	GATE_APMIXED(CLK_APMIXED_MMSYS_F26M, "apmixed_mmsys26m", "f_f26m_ck",
		     8),
	GATE_APMIXED(CLK_APMIXED_UFS26M, "apmixed_ufs26m", "f_f26m_ck", 9),
	GATE_APMIXED(CLK_APMIXED_MIPIC1_26M, "apmixed_mipic126m", "f_f26m_ck",
		     11),
	GATE_APMIXED(CLK_APMIXED_MEMPLL26M, "apmixed_mempll26m", "f_f26m_ck",
		     13),
	GATE_APMIXED(CLK_APMIXED_CLKSQ_LVPLL_26M, "apmixed_lvpll26m",
		     "f_f26m_ck", 14),
	GATE_APMIXED(CLK_APMIXED_MIPID0_26M, "apmixed_mipid026m", "f_f26m_ck",
		     16),
};

#define MT6768_PLL_FMAX (3800UL * MHZ)
#define MT6768_PLL_FMIN (1500UL * MHZ)
#define MT6768_INTEGER_BITS 8

#define CON0_MT6768_RST_BAR BIT(23)
#define CON0_MT6768_EN_MASK BIT(0)

#define PLL(_id, _name, _reg, _pwr_reg, _flags, _pcwbits, _pd_reg, _pd_shift, \
	    _tuner_reg, _tuner_en_reg, _tuner_en_bit, _pcw_reg, _pcw_shift)   \
	{                                                                     \
		.id = _id,                                                    \
		.name = _name,                                                \
		.reg = _reg,                                                  \
		.pwr_reg = _pwr_reg,                                          \
		.en_mask = CON0_MT6768_EN_MASK,                               \
		.flags = _flags,                                              \
		.rst_bar_mask = CON0_MT6768_RST_BAR,                          \
		.fmax = MT6768_PLL_FMAX,                                      \
		.fmin = MT6768_PLL_FMIN,                                      \
		.pcwbits = _pcwbits,                                          \
		.pcwibits = MT6768_INTEGER_BITS,                              \
		.pd_reg = _pd_reg,                                            \
		.pd_shift = _pd_shift,                                        \
		.tuner_reg = _tuner_reg,                                      \
		.tuner_en_reg = _tuner_en_reg,                                \
		.tuner_en_bit = _tuner_en_bit,                                \
		.pcw_reg = _pcw_reg,                                          \
		.pcw_shift = _pcw_shift,                                      \
	}

static const struct mtk_pll_data plls[] = {
	PLL(CLK_APMIXED_ARMPLL, "armpll", 0x0208, 0x0214, PLL_AO, 22, 0x020C,
	    24, 0, 0, 0, 0x020C, 0),
	PLL(CLK_APMIXED_ARMPLL_L, "armpll_l", 0x0218, 0x0224, PLL_AO, 22,
	    0x021C, 24, 0, 0, 0, 0x021C, 0),
	PLL(CLK_APMIXED_CCIPLL, "ccipll", 0x0228, 0x0234, PLL_AO, 22, 0x022C,
	    24, 0, 0, 0, 0x022C, 0),
	PLL(CLK_APMIXED_UNIV2PLL, "univ2pll", 0x0238, 0x0244, HAVE_RST_BAR, 22,
	    0x023C, 24, 0, 0, 0, 0x023C, 0),
	PLL(CLK_APMIXED_MFGPLL, "mfgpll", 0x0248, 0x0254, HAVE_RST_BAR, 22,
	    0x024C, 24, 0, 0, 0, 0x024C, 0),

	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x0258, 0x0264, PLL_AO, 22, 0x025C,
	    24, 0, 0, 0, 0x025C, 0),
	/* APLL pcw is at 0x310, postdiv at 0x30c. Also, it has turner regs.*/
	PLL(CLK_APMIXED_APLL1, "apll1", 0x0308, 0x0318, HAVE_RST_BAR, 32,
	    0x030C, 24, 0x0040, 0x000C, 0, 0x0310, 0),
	PLL(CLK_APMIXED_MMPLL, "mmpll", 0x031C, 0x0328, HAVE_RST_BAR, 22,
	    0x0320, 24, 0, 0, 0, 0x0320, 0),
	PLL(CLK_APMIXED_MPLL, "mpll", 0x032C, 0x0338, PLL_AO, 22, 0x0330, 24, 0,
	    0, 0, 0x0330, 0),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", 0x033C, 0x0348, HAVE_RST_BAR, 22,
	    0x0320, 24, 0, 0, 0, 0x0340, 0),
};

static int mt6768_top_probe(struct udevice *dev)
{
	struct mtk_clk_priv *priv = dev_get_priv(dev);
	void __iomem *base = dev_read_addr_ptr(dev);
	struct udevice *apmixed_dev;

	if (!base)
		return -ENOENT;

	mtk_clk_register_fixed_clks(dev, top_fixed_clks,
				    ARRAY_SIZE(top_fixed_clks), priv);

	uclass_get_device_by_driver(
		UCLASS_CLK, DM_DRIVER_GET(mt6768_apmixedsys), &apmixed_dev);

	priv->hws = devm_kcalloc(dev, CLK_TOP_NR_CLK, sizeof(struct clk *),
				 GFP_KERNEL);
	if (!priv->hws)
		return -ENOMEM;

	priv->num_hws = CLK_TOP_NR_CLK;

	mtk_clk_register_factors(dev, top_pll_divs, ARRAY_SIZE(top_pll_divs),
				 priv);
	mtk_clk_register_muxes(dev, top_muxes, ARRAY_SIZE(top_muxes), priv);
	mtk_clk_register_factors(dev, top_bus_divs, ARRAY_SIZE(top_bus_divs),
				 priv);
	mtk_clk_register_gates(dev, top_clks, ARRAY_SIZE(top_clks), priv);

	return 0;
}

static int mt6768_infra_probe(struct udevice *dev)
{
	struct mtk_clk_priv *priv = dev_get_priv(dev);
	struct udevice *apmixed_dev;

	priv->hws = devm_kcalloc(dev, CLK_INFRA_NR_CLK, sizeof(struct clk *),
				 GFP_KERNEL);
	if (!priv->hws)
		return -ENOMEM;

	uclass_get_device_by_driver(
		UCLASS_CLK, DM_DRIVER_GET(mt6768_apmixedsys), &apmixed_dev);

	priv->num_hws = CLK_INFRA_NR_CLK;

	mtk_clk_register_gates(dev, infra_clks, ARRAY_SIZE(infra_clks), priv);

	return 0;
}

static int mt6768_apmixed_probe(struct udevice *dev)
{
	struct mtk_clk_priv *priv = dev_get_priv(dev);
	void __iomem *base = dev_read_addr_ptr(dev);

	if (!base)
		return -ENOENT;

	priv->hws = devm_kcalloc(dev, CLK_APMIXED_NR_CLK, sizeof(struct clk *),
				 GFP_KERNEL);
	if (!priv->hws)
		return -ENOMEM;

	priv->num_hws = CLK_TOP_NR_CLK;

	mtk_clk_register_plls(dev, plls, ARRAY_SIZE(plls), priv);
	mtk_clk_register_gates(dev, apmixed_clks, ARRAY_SIZE(apmixed_clks),
			       priv);

	return 0;
}

static int mt6768_clk_of_xlate(struct clk *clk,
			       struct ofnode_phandle_args *args)
{
	struct mtk_clk_priv *priv = dev_get_priv(clk->dev);

	if (args->args_count != 1)
		return -EINVAL;

	if (args->args[0] >= priv->num_hws)
		return -EINVAL;

	clk->id = args->args[0];

	return 0;
}

static ulong mt6768_clk_get_rate(struct clk *clk)
{
	struct mtk_clk_priv *priv = dev_get_priv(clk->dev);
	struct clk *c;

	if (clk->id >= priv->num_hws)
		return -EINVAL;

	c = priv->hws[clk->id];
	if (!c || IS_ERR(c))
		return -ENOSYS;

	return clk_get_rate(c);
}

static ulong mt6768_clk_set_rate(struct clk *clk, ulong rate)
{
	struct mtk_clk_priv *priv = dev_get_priv(clk->dev);
	struct clk *c = priv->hws[clk->id];

	if (!c || IS_ERR(c))
		return -ENOSYS;
	return clk_set_rate(c, rate);
}

static int mt6768_clk_enable(struct clk *clk)
{
	struct mtk_clk_priv *priv = dev_get_priv(clk->dev);
	struct clk *c = priv->hws[clk->id];

	if (!c || IS_ERR(c))
		return -ENOSYS;
	return clk_enable(c);
}

static int mt6768_clk_disable(struct clk *clk)
{
	struct mtk_clk_priv *priv = dev_get_priv(clk->dev);
	struct clk *c = priv->hws[clk->id];

	if (!c || IS_ERR(c))
		return -ENOSYS;
	return clk_disable(c);
}

static int mt6768_clk_set_parent(struct clk *clk, struct clk *parent)
{
	struct mtk_clk_priv *priv = dev_get_priv(clk->dev);
	struct clk *c = priv->hws[clk->id];

	if (!c || IS_ERR(c))
		return -ENOSYS;
	return clk_set_parent(c, parent);
}

static const struct clk_ops mt6768_clk_ops = {
	.of_xlate = mt6768_clk_of_xlate,
	.get_rate = mt6768_clk_get_rate,
	.set_rate = mt6768_clk_set_rate,
	.enable = mt6768_clk_enable,
	.disable = mt6768_clk_disable,
	.set_parent = mt6768_clk_set_parent,
};

static const struct udevice_id mt6768_topckgen_compat[] = {
	{ .compatible = "mediatek,mt6768-topckgen" },
	{}
};

U_BOOT_DRIVER(mt6768_topckgen) = {
	.name = "mt6768-topckgen",
	.id = UCLASS_CLK,
	.of_match = mt6768_topckgen_compat,
	.ops = &mt6768_clk_ops,
	.probe = mt6768_top_probe,
	.priv_auto = sizeof(struct mtk_clk_priv),
	.flags = DM_FLAG_PRE_RELOC,
};

static const struct udevice_id mt6768_infracfg_ao_compat[] = {
	{ .compatible = "mediatek,mt6768-infracfg_ao" },
	{}
};

U_BOOT_DRIVER(mt6768_infracfg_ao) = {
	.name = "mt6768-infracfg_ao",
	.id = UCLASS_CLK,
	.of_match = mt6768_infracfg_ao_compat,
	.ops = &mt6768_clk_ops,
	.probe = mt6768_infra_probe,
	.priv_auto = sizeof(struct mtk_clk_priv),
	.flags = DM_FLAG_PRE_RELOC,
};

static const struct udevice_id mt6768_apmixedsys_compat[] = {
	{ .compatible = "mediatek,mt6768-apmixedsys" },
	{}
};

U_BOOT_DRIVER(mt6768_apmixedsys) = {
	.name = "mt6768-apmixedsys",
	.id = UCLASS_CLK,
	.of_match = mt6768_apmixedsys_compat,
	.ops = &mt6768_clk_ops,
	.probe = mt6768_apmixed_probe,
	.priv_auto = sizeof(struct mtk_clk_priv),
	.flags = DM_FLAG_PRE_RELOC,
};
