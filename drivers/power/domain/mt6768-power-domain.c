// SPDX-License-Identifier: GPL-2.0-only

#include <dm.h>
#include <dt-bindings/power/mediatek,mt6768-power-controller.h>
#include <linux/bitops.h>

#include "mtk-power-domain.h"

#define MT6768_TOP_AXI_PROT_EN_SET		0x2a0
#define MT6768_TOP_AXI_PROT_EN_CLR		0x2a4
#define MT6768_TOP_AXI_PROT_EN_STA		0x228
#define MT6768_TOP_AXI_PROT_EN_1_SET		0x2a8
#define MT6768_TOP_AXI_PROT_EN_1_CLR		0x2ac
#define MT6768_TOP_AXI_PROT_EN_1_STA		0x258

#define MT6768_SMI_COMMON_CLAMP_EN		0x3c0
#define MT6768_SMI_COMMON_CLAMP_EN_SET		0x3c4
#define MT6768_SMI_COMMON_CLAMP_EN_CLR		0x3c8

#define MT6768_TOP_AXI_PROT_EN_MD1_STEP1		BIT(7)
#define MT6768_TOP_AXI_PROT_EN_MD1_STEP2		(BIT(3) | BIT(4))
#define MT6768_TOP_AXI_PROT_EN_1_MD1_STEP2		BIT(6)
#define MT6768_TOP_AXI_PROT_EN_CONN_STEP1		BIT(13)
#define MT6768_TOP_AXI_PROT_EN_1_CONN_STEP1		BIT(18)
#define MT6768_TOP_AXI_PROT_EN_CONN_STEP2		(BIT(14) | BIT(16))
#define MT6768_TOP_AXI_PROT_EN_1_MM_STEP1		(GENMASK(20, 19) | GENMASK(31, 30))
#define MT6768_TOP_AXI_PROT_EN_1_MM_STEP2		GENMASK(17, 16)
#define MT6768_TOP_AXI_PROT_EN_MM_STEP3		GENMASK(11, 10)
#define MT6768_TOP_AXI_PROT_EN_MM_STEP4		GENMASK(2, 1)
#define MT6768_TOP_AXI_PROT_EN_MFG_STEP1		BIT(25)
#define MT6768_TOP_AXI_PROT_EN_MFG_STEP2		GENMASK(22, 21)
#define MT6768_TOP_AXI_PROT_EN_1_ISP_STEP1		BIT(20)
#define MT6768_TOP_AXI_PROT_EN_1_CAM_STEP1		(BIT(19) | BIT(21))
#define MT6768_TOP_AXI_PROT_EN_CAM_STEP2		BIT(20)
#define MT6768_TOP_AXI_PROT_EN_1_VENC_STEP1		BIT(31)
#define MT6768_TOP_AXI_PROT_EN_1_VDEC_STEP1		BIT(30)
#define MT6768_SMI_COMMON_CLAMP_ISP		BIT(2)
#define MT6768_SMI_COMMON_CLAMP_CAM		BIT(3)
#define MT6768_SMI_COMMON_CLAMP_VENC		BIT(4)
#define MT6768_SMI_COMMON_CLAMP_VDEC		BIT(1)

#define MT6768_BUS_PROT_INFRA(_mask)				\
	BUS_PROT_WR_IGN(INFRA, _mask,				\
			MT6768_TOP_AXI_PROT_EN_SET,		\
			MT6768_TOP_AXI_PROT_EN_CLR,		\
			MT6768_TOP_AXI_PROT_EN_STA)

#define MT6768_BUS_PROT_INFRA_1(_mask)				\
	BUS_PROT_WR_IGN(INFRA, _mask,				\
			MT6768_TOP_AXI_PROT_EN_1_SET,		\
			MT6768_TOP_AXI_PROT_EN_1_CLR,		\
			MT6768_TOP_AXI_PROT_EN_1_STA)

#define MT6768_BUS_PROT_SMI(_mask)				\
	BUS_PROT_WR(SMI, _mask,					\
		    MT6768_SMI_COMMON_CLAMP_EN_SET,		\
		    MT6768_SMI_COMMON_CLAMP_EN_CLR,		\
		    MT6768_SMI_COMMON_CLAMP_EN)

static const struct mtk_scp_domain_data mt6768_scp_domain[] = {
	[MT6768_POWER_DOMAIN_MD1] = {
		.sta_mask = BIT(0),
		.ctl_offs = 0x320,
		.pwr_sta_offs = 0x180,
		.pwr_sta2nd_offs = 0x184,
		.bp_cfg = {
			MT6768_BUS_PROT_INFRA(MT6768_TOP_AXI_PROT_EN_MD1_STEP1),
			MT6768_BUS_PROT_INFRA(MT6768_TOP_AXI_PROT_EN_MD1_STEP2),
			MT6768_BUS_PROT_INFRA_1(MT6768_TOP_AXI_PROT_EN_1_MD1_STEP2),
		},
	},
	[MT6768_POWER_DOMAIN_CONN] = {
		.sta_mask = BIT(1),
		.ctl_offs = 0x32c,
		.pwr_sta_offs = 0x180,
		.pwr_sta2nd_offs = 0x184,
		.bp_cfg = {
			MT6768_BUS_PROT_INFRA(MT6768_TOP_AXI_PROT_EN_CONN_STEP1),
			MT6768_BUS_PROT_INFRA_1(MT6768_TOP_AXI_PROT_EN_1_CONN_STEP1),
			MT6768_BUS_PROT_INFRA(MT6768_TOP_AXI_PROT_EN_CONN_STEP2),
		},
	},
	[MT6768_POWER_DOMAIN_MM] = {
		.sta_mask = BIT(5),
		.ctl_offs = 0x30c,
		.pwr_sta_offs = 0x180,
		.pwr_sta2nd_offs = 0x184,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_cfg = {
			MT6768_BUS_PROT_INFRA_1(MT6768_TOP_AXI_PROT_EN_1_MM_STEP1),
			MT6768_BUS_PROT_INFRA_1(MT6768_TOP_AXI_PROT_EN_1_MM_STEP2),
			MT6768_BUS_PROT_INFRA(MT6768_TOP_AXI_PROT_EN_MM_STEP3),
			MT6768_BUS_PROT_INFRA(MT6768_TOP_AXI_PROT_EN_MM_STEP4),
		},
	},
	[MT6768_POWER_DOMAIN_MFG] = {
		.sta_mask = BIT(11),
		.ctl_offs = 0x338,
		.pwr_sta_offs = 0x180,
		.pwr_sta2nd_offs = 0x184,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.domain_supply = true,
		.bp_cfg = {
			MT6768_BUS_PROT_INFRA(MT6768_TOP_AXI_PROT_EN_MFG_STEP1),
			MT6768_BUS_PROT_INFRA(MT6768_TOP_AXI_PROT_EN_MFG_STEP2),
		},
	},
	[MT6768_POWER_DOMAIN_ISP] = {
		.sta_mask = BIT(6),
		.ctl_offs = 0x308,
		.pwr_sta_offs = 0x180,
		.pwr_sta2nd_offs = 0x184,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_cfg = {
			MT6768_BUS_PROT_INFRA_1(MT6768_TOP_AXI_PROT_EN_1_ISP_STEP1),
			MT6768_BUS_PROT_SMI(MT6768_SMI_COMMON_CLAMP_ISP),
		},
	},
	[MT6768_POWER_DOMAIN_MFG_CORE0] = {
		.sta_mask = BIT(12),
		.ctl_offs = 0x34c,
		.pwr_sta_offs = 0x180,
		.pwr_sta2nd_offs = 0x184,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT6768_POWER_DOMAIN_MFG_CORE1] = {
		.sta_mask = BIT(13),
		.ctl_offs = 0x310,
		.pwr_sta_offs = 0x180,
		.pwr_sta2nd_offs = 0x184,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
	},
	[MT6768_POWER_DOMAIN_MFG_ASYNC] = {
		.sta_mask = BIT(14),
		.ctl_offs = 0x334,
		.pwr_sta_offs = 0x180,
		.pwr_sta2nd_offs = 0x184,
		.domain_supply = true,
	},
	[MT6768_POWER_DOMAIN_CAM] = {
		.sta_mask = BIT(7),
		.ctl_offs = 0x344,
		.pwr_sta_offs = 0x180,
		.pwr_sta2nd_offs = 0x184,
		.sram_pdn_bits = GENMASK(9, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.bp_cfg = {
			MT6768_BUS_PROT_INFRA_1(MT6768_TOP_AXI_PROT_EN_1_CAM_STEP1),
			MT6768_BUS_PROT_INFRA(MT6768_TOP_AXI_PROT_EN_CAM_STEP2),
			MT6768_BUS_PROT_SMI(MT6768_SMI_COMMON_CLAMP_CAM),
		},
	},
	[MT6768_POWER_DOMAIN_VENC] = {
		.sta_mask = BIT(9),
		.ctl_offs = 0x304,
		.pwr_sta_offs = 0x180,
		.pwr_sta2nd_offs = 0x184,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_cfg = {
			MT6768_BUS_PROT_INFRA_1(MT6768_TOP_AXI_PROT_EN_1_VENC_STEP1),
			MT6768_BUS_PROT_SMI(MT6768_SMI_COMMON_CLAMP_VENC),
		},
	},
	[MT6768_POWER_DOMAIN_VDEC] = {
		.sta_mask = BIT(8),
		.ctl_offs = 0x370,
		.pwr_sta_offs = 0x180,
		.pwr_sta2nd_offs = 0x184,
		.sram_pdn_bits = BIT(8),
		.sram_pdn_ack_bits = BIT(12),
		.bp_cfg = {
			MT6768_BUS_PROT_INFRA_1(MT6768_TOP_AXI_PROT_EN_1_VDEC_STEP1),
			MT6768_BUS_PROT_SMI(MT6768_SMI_COMMON_CLAMP_VDEC),
		},
	},
};

MTK_SCP_SOC_DATA(mt6768, mt6768_scp_domain);

static const struct udevice_id mt6768_power_domain_ids[] = {
	{ .compatible = "mediatek,mt6768-power-controller", .data = (ulong)&mt6768_scp_soc_data },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(mt6768_power_domain) = {
	.name = "mt6768_power_domain",
	.id = UCLASS_POWER_DOMAIN,
	.ops = &mtk_power_domain_ops,
	.probe = mtk_power_controller_probe,
	.of_match = mt6768_power_domain_ids,
	.priv_auto = sizeof(struct mtk_scpsys),
};
