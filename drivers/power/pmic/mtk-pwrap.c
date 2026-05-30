#include <asm/io.h>
#include <dm.h>
#include <dm/device.h>
#include <linux/delay.h>
#include <power/pmic.h>
#include <linux/iopoll.h>

#define PWRAP_POLL_DELAY_US 10
#define PWRAP_POLL_TIMEOUT_US 10000

/* macro for wrapper status */
#define PWRAP_GET_WACS_RDATA(x) (((x) >> 0) & 0x0000ffff)
#define PWRAP_GET_WACS_ARB_FSM(x) (((x) >> 1) & 0x00000007)
#define PWRAP_GET_WACS_FSM(x) (((x) >> 16) & 0x00000007)
#define PWRAP_GET_WACS_REQ(x) (((x) >> 19) & 0x00000001)
#define PWRAP_STATE_SYNC_IDLE0 BIT(20)
#define PWRAP_STATE_INIT_DONE0 BIT(21)
#define PWRAP_STATE_INIT_DONE0_MT8186 BIT(22)
#define PWRAP_STATE_INIT_DONE1 BIT(15)

/* macro for WACS FSM */
#define PWRAP_WACS_FSM_IDLE 0x00
#define PWRAP_WACS_FSM_REQ 0x02
#define PWRAP_WACS_FSM_WFDLE 0x04
#define PWRAP_WACS_FSM_WFVLDCLR 0x06
#define PWRAP_WACS_INIT_DONE 0x01
#define PWRAP_WACS_WACS_SYNC_IDLE 0x01
#define PWRAP_WACS_SYNC_BUSY 0x00

struct pmic_wrapper_data {
	const u32 *regs;
	const bool io32;
};

struct pmic_wrapper {
	void __iomem *base;
	const struct pmic_wrapper_data *data;
};

enum pwrap_regs {
	PWRAP_WACS2_CMD,
	PWRAP_WACS2_RDATA,
	PWRAP_WACS2_VLDCLR,
};

static u32 pwrap_readl(struct pmic_wrapper *wrp, enum pwrap_regs reg)
{
	return readl(wrp->base + wrp->data->regs[reg]);
}

static void pwrap_writel(struct pmic_wrapper *wrp, u32 val, enum pwrap_regs reg)
{
	writel(val, wrp->base + wrp->data->regs[reg]);
}

static u32 pwrap_get_fsm_state(struct pmic_wrapper *wrp)
{
	u32 val;
	val = pwrap_readl(wrp, PWRAP_WACS2_RDATA);
	return PWRAP_GET_WACS_FSM(val);
}

static int pwrap_is_fsm_idle(struct pmic_wrapper *wrp)
{
	return pwrap_get_fsm_state(wrp) == PWRAP_WACS_FSM_IDLE;
}

static bool pwrap_is_fsm_vldclr(struct pmic_wrapper *wrp)
{
	return pwrap_get_fsm_state(wrp) == PWRAP_WACS_FSM_WFVLDCLR;
}

static inline void pwrap_leave_fsm_vldclr(struct pmic_wrapper *wrp)
{
	if (pwrap_is_fsm_vldclr(wrp))
		pwrap_writel(wrp, 1, PWRAP_WACS2_VLDCLR);
}

static int pwrap_read16(struct pmic_wrapper *wrp, u32 adr, u16 *rdata)
{
	bool tmp;
	int ret;
	u32 val;

	ret = readx_poll_timeout(pwrap_is_fsm_idle, wrp, tmp, tmp,
				 PWRAP_POLL_TIMEOUT_US);
	if (ret) {
		pwrap_leave_fsm_vldclr(wrp);
		return ret;
	}

	val = (adr >> 1) << 16;
	pwrap_writel(wrp, val, PWRAP_WACS2_CMD);

	ret = readx_poll_timeout(pwrap_is_fsm_vldclr, wrp, tmp, tmp,
				 PWRAP_POLL_TIMEOUT_US);
	if (ret)
		return ret;

	val = pwrap_readl(wrp, PWRAP_WACS2_RDATA);
	*rdata = PWRAP_GET_WACS_RDATA(val);

	pwrap_writel(wrp, 1, PWRAP_WACS2_VLDCLR);

	return 0;
}

static int pwrap_read32(struct pmic_wrapper *wrp, u32 adr, u32 *rdata)
{
	bool tmp;
	int ret, msb;

	*rdata = 0;
	for (msb = 0; msb < 2; msb++) {
		ret = readx_poll_timeout(pwrap_is_fsm_idle, wrp, tmp, tmp,
					 PWRAP_POLL_TIMEOUT_US);

		if (ret) {
			pwrap_leave_fsm_vldclr(wrp);
			return ret;
		}

		pwrap_writel(wrp, ((msb << 30) | (adr << 16)), PWRAP_WACS2_CMD);

		ret = readx_poll_timeout(pwrap_is_fsm_vldclr, wrp, tmp, tmp,
					 PWRAP_POLL_TIMEOUT_US);
		if (ret)
			return ret;

		*rdata += (PWRAP_GET_WACS_RDATA(
				   pwrap_readl(wrp, PWRAP_WACS2_RDATA))
			   << (16 * msb));

		pwrap_writel(wrp, 1, PWRAP_WACS2_VLDCLR);
	}

	return 0;
}

static int pwrap_read(struct udevice *dev, u32 addr, u8 *buf, int len)
{
	struct pmic_wrapper *wrp = dev_get_priv(dev);
	if (wrp->data->io32) {
		if (len != 4)
			return -EINVAL;

		return pwrap_read32(wrp, addr, (u32 *)buf);
	} else {
		if (len != 2)
			return -EINVAL;

		return pwrap_read16(wrp, addr, (u16 *)buf);
	}
}

static int pwrap_write16(struct pmic_wrapper *wrp, u32 adr, u16 wdata)
{
	bool tmp;
	int ret;

	ret = readx_poll_timeout(pwrap_is_fsm_idle, wrp, tmp, tmp,
				 PWRAP_POLL_TIMEOUT_US);
	if (ret) {
		pwrap_leave_fsm_vldclr(wrp);
		return ret;
	}

	pwrap_writel(wrp, BIT(31) | ((adr >> 1) << 16) | wdata,
		     PWRAP_WACS2_CMD);

	return 0;
}

static int pwrap_write32(struct pmic_wrapper *wrp, u32 adr, u32 wdata)
{
	bool tmp;
	int ret, msb, rdata;

	for (msb = 0; msb < 2; msb++) {
		ret = readx_poll_timeout(pwrap_is_fsm_idle, wrp, tmp, tmp,
					 PWRAP_POLL_TIMEOUT_US);
		if (ret) {
			pwrap_leave_fsm_vldclr(wrp);
			return ret;
		}

		pwrap_writel(wrp,
			     (1 << 31) | (msb << 30) | (adr << 16) |
				     ((wdata >> (msb * 16)) & 0xffff),
			     PWRAP_WACS2_CMD);

		/*
		 * The pwrap_read operation is the requirement of hardware used
		 * for the synchronization between two successive 16-bit
		 * pwrap_writel operations composing one 32-bit bus writing.
		 * Otherwise, we'll find the result fails on the lower 16-bit
		 * pwrap writing.
		 */
		if (!msb)
			pwrap_read32(wrp, adr, &rdata);
	}

	return 0;
}

static int pwrap_write(struct udevice *dev, u32 addr, const u8 *buf, int len)
{
	struct pmic_wrapper *wrp = dev_get_priv(dev);
	if (wrp->data->io32) {
		if (len != 4)
			return -EINVAL;

		return pwrap_write32(wrp, addr, *(u32 *)buf);
	} else {
		if (len != 2)
			return -EINVAL;

		return pwrap_write16(wrp, addr, *(u16 *)buf);
	}
}

static int pwrap_probe(struct udevice *dev)
{
	struct pmic_wrapper *wrp = dev_get_priv(dev);
	wrp->data = (const struct pmic_wrapper_data *)dev_get_driver_data(dev);

	wrp->base = dev_read_addr_ptr(dev);
	if (!wrp->base)
		return -EINVAL;

#define MT6358_LDO_VEMC_CON1 0x1b2a
#define MT6358_LDO_VMCH_CON1 0x1ce6

	u16 data;
	pwrap_read16(wrp, MT6358_LDO_VEMC_CON1, &data);
	if (data & BIT(15))
		printf("vemc is ON: %d\n", data);
	else
		printf("vemc is OFF: %d\n", data);

	pwrap_read16(wrp, MT6358_LDO_VMCH_CON1, &data);
	if (data & BIT(15))
		printf("vmch is ON: %d\n", data);
	else
		printf("vmch is OFF: %d\n", data);

#define MT6358_VMC_ANA_CON0 0x1e4c
#define MT6358_LDO_VMC_CON0 0x1cc4
#define MT6358_VMCH_ANA_CON0 0x1e48
#define MT6358_LDO_VMCH_CON0 0x1cd8

	u16 val;

	/*vmc*/
	(pwrap_read16(wrp, MT6358_VMC_ANA_CON0, &val));
	val &= ~0xF0F;
	val |= (13 << 8);
	(pwrap_write16(wrp, MT6358_VMC_ANA_CON0, val));

	(pwrap_read16(wrp, MT6358_LDO_VMC_CON0, &val));
	val |= BIT(0);
	(pwrap_write16(wrp, MT6358_LDO_VMC_CON0, val));

	mdelay(5);

	/*vmch*/
	(pwrap_read16(wrp, MT6358_VMCH_ANA_CON0, &val));
	val &= ~0x70F;
	val |= (5 << 8);
	(pwrap_write16(wrp, MT6358_VMCH_ANA_CON0, val));

	(pwrap_read16(wrp, MT6358_LDO_VMCH_CON0, &val));
	val |= BIT(0);
	(pwrap_write16(wrp, MT6358_LDO_VMCH_CON0, val));

	mdelay(10);

	pwrap_read16(wrp, MT6358_LDO_VEMC_CON1, &data);
	if (data & BIT(15))
		printf("vemc is ON: %d\n", data);
	else
		printf("vemc is OFF: %d\n", data);

	pwrap_read16(wrp, MT6358_LDO_VMCH_CON1, &data);
	if (data & BIT(15))
		printf("vmch is ON: %d\n", data);
	else
		printf("vmch is OFF: %d\n", data);

	return 0;
}

static int pwrap_reg_count(struct udevice *dev)
{
	return 0xd00;
}

static struct dm_pmic_ops pwrap_ops = {
	.reg_count = pwrap_reg_count,
	.read = pwrap_read,
	.write = pwrap_write,
};

static const u32 mt6768_regs[] = {
	[PWRAP_WACS2_CMD] = 0xc20,
	[PWRAP_WACS2_RDATA] = 0xc24,
	[PWRAP_WACS2_VLDCLR] = 0xc28,
};

static const struct pmic_wrapper_data mt6768_pwrap = {
	.regs = mt6768_regs,
	.io32 = false,
};

static const struct udevice_id pwrap_match[] = {
	{ .compatible = "mediatek,mt6768-pwrap", .data = (ulong)&mt6768_pwrap },
	{}
};

U_BOOT_DRIVER(mtk_pwrap) = {
	.name = "mtk_pwrap",
	.id = UCLASS_PMIC,
	.of_match = pwrap_match,
	.bind = dm_scan_fdt_dev,
	.priv_auto = sizeof(struct pmic_wrapper),
	.probe = pwrap_probe,
	.ops = &pwrap_ops,
};
