// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2025-2026 Roman Vivchar <rva333@protonmail.com>
 *
 * Backlight driver for the Kinetic KTD3137
 */

#define LOG_CATEGORY UCLASS_PANEL_BACKLIGHT

#include <backlight.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <i2c.h>
#include <asm/gpio.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <power/regulator.h>

#define KTD3137_REG_DEV_ID		0x00
#define KTD3137_REG_SW_RESET		0x01
#define KTD3137_REG_MODE		0x02
#define KTD3137_REG_CONTROL		0x03
#define KTD3137_REG_RATIO_LSB		0x04
#define KTD3137_REG_RATIO_MSB		0x05
#define KTD3137_REG_PWM			0x06
#define KTD3137_REG_RAMP_ON		0x07
#define KTD3137_REG_TRANS_RAMP		0x08
#define KTD3137_REG_FLASH_SETTING	0x09
#define KTD3137_REG_STATUS		0x0a
#define KTD3137_REG_MAX			0x0a

/* KTD3137_REG_DEV_ID */
#define KTD3137_DEV_ID_A		0x12
#define KTD3137_DEV_ID_B		0x18

/* KTD3137_REG_MODE */
#define KTD3137_MODE_BL_EN		BIT(0)
#define KTD3137_MODE_CURRENT_MASK	GENMASK(7, 3)

/* KTD3137_REG_CONTROL */
#define KTD3137_CONTROL_LINEAR		(BIT(1) | BIT(2))
#define KTD3137_CONTROL_IND_2P6A	BIT(3)
#define KTD3137_CONTROL_OVP_32V		BIT(5)
#define KTD3137_CONTROL_FREQ_1000KHZ	BIT(6)
#define KTD3137_CONTROL_DEFAULT		(KTD3137_CONTROL_IND_2P6A | \
					 KTD3137_CONTROL_OVP_32V | \
					 KTD3137_CONTROL_FREQ_1000KHZ)

/* KTD3137_REG_RATIO_LSB */
#define KTD3137_RATIO_LSB_MASK		GENMASK(2, 0)

/* KTD3137_REG_PWM */
#define KTD3137_PWM_CH1_EN		BIT(0)
#define KTD3137_PWM_CH2_EN		BIT(1)
#define KTD3137_PWM_FILTER_6LSB		(BIT(3) | BIT(4))
#define KTD3137_PWM_DISABLE		BIT(7)
#define KTD3137_PWM_DEFAULT		(KTD3137_PWM_CH1_EN | \
					 KTD3137_PWM_CH2_EN | \
					 KTD3137_PWM_FILTER_6LSB | \
					 KTD3137_PWM_DISABLE)

/* KTD3137_REG_RAMP_ON */
#define KTD3137_RAMP_ON_MASK		GENMASK(7, 4)
#define KTD3137_RAMP_OFF_MASK		GENMASK(3, 0)
/* Ramp ON: 2ms, OFF: 8ms */
#define KTD3137_RAMP_DEFAULT		(FIELD_PREP(KTD3137_RAMP_ON_MASK, 4) | \
					 FIELD_PREP(KTD3137_RAMP_OFF_MASK, 4))

#define KTD3137_DEFAULT_BRIGHTNESS	2047
#define KTD3137_MAX_BRIGHTNESS		2047

#define KTD3137_I_MIN_UA		5000
#define KTD3137_I_MAX_UA		29800
#define KTD3137_I_STEP_UA		800
#define KTD3137_I_DEFAULT_UA		20200

struct ktd3137 {
	struct udevice *dev;
	struct gpio_desc enable_gpio;
	u32 led_current;
	u32 default_brightness;
	u32 brightness;
};

static int ktd3137_init(struct ktd3137 *ktd)
{
	u32 val = KTD3137_CONTROL_DEFAULT | KTD3137_CONTROL_LINEAR;
	int ret;

	ret = dm_i2c_reg_write(ktd->dev, KTD3137_REG_CONTROL, val);
	if (ret)
		return ret;

	ret = dm_i2c_reg_write(ktd->dev, KTD3137_REG_PWM, KTD3137_PWM_DEFAULT);
	if (ret)
		return ret;

	ret = dm_i2c_reg_write(ktd->dev, KTD3137_REG_RAMP_ON, KTD3137_RAMP_DEFAULT);
	if (ret)
		return ret;

	ret = dm_i2c_reg_write(ktd->dev, KTD3137_REG_TRANS_RAMP, 0x00);
	if (ret)
		return ret;

	return dm_i2c_reg_clrset(ktd->dev, KTD3137_REG_MODE,
				 KTD3137_MODE_CURRENT_MASK, ktd->led_current);
}

static int ktd3137_backlight_update_status(struct ktd3137 *ktd)
{
	unsigned int brightness = ktd->brightness;
	int ret;

	if (brightness > 0) {
		ret = dm_i2c_reg_write(ktd->dev, KTD3137_REG_RATIO_LSB,
				       brightness & KTD3137_RATIO_LSB_MASK);
		if (ret)
			return ret;

		ret = dm_i2c_reg_write(ktd->dev, KTD3137_REG_RATIO_MSB,
				       brightness >> 3);
		if (ret)
			return ret;
	}

	return dm_i2c_reg_clrset(ktd->dev, KTD3137_REG_MODE,
				 KTD3137_MODE_BL_EN,
				 brightness ? KTD3137_MODE_BL_EN : 0);
}

static int ktd3137_backlight_enable(struct udevice *dev)
{
	struct ktd3137 *ktd = dev_get_priv(dev);

	return ktd3137_backlight_update_status(ktd);
}

static int ktd3137_backlight_set_brightness(struct udevice *dev, int percent)
{
	struct ktd3137 *ktd = dev_get_priv(dev);

	if (percent == BACKLIGHT_DEFAULT) {
		ktd->brightness = ktd->default_brightness;
	} else if (percent == BACKLIGHT_OFF || percent < BACKLIGHT_MIN) {
		ktd->brightness = 0;
	} else {
		if (percent > BACKLIGHT_MAX)
			percent = BACKLIGHT_MAX;
		ktd->brightness = percent * KTD3137_MAX_BRIGHTNESS / BACKLIGHT_MAX;
	}

	return ktd3137_backlight_update_status(ktd);
}

static const struct backlight_ops ktd3137_backlight_ops = {
	.enable = ktd3137_backlight_enable,
	.set_brightness = ktd3137_backlight_set_brightness,
};

static int ktd3137_parse_dt(struct ktd3137 *ktd)
{
	u32 max_ua = KTD3137_I_DEFAULT_UA;
	u32 val;

	dev_read_u32(ktd->dev, "led-max-microamp", &max_ua);

	if (max_ua < KTD3137_I_MIN_UA || max_ua > KTD3137_I_MAX_UA) {
		dev_err(ktd->dev, "led-max-microamp (%u) out of range\n", max_ua);
		return -EINVAL;
	}

	val = (max_ua - KTD3137_I_MIN_UA) / KTD3137_I_STEP_UA;
	ktd->led_current = FIELD_PREP(KTD3137_MODE_CURRENT_MASK, val);

	ktd->default_brightness = KTD3137_DEFAULT_BRIGHTNESS;
	dev_read_u32(ktd->dev, "default-brightness", &ktd->default_brightness);
	ktd->brightness = ktd->default_brightness;

	return 0;
}

static int ktd3137_probe(struct udevice *dev)
{
	struct ktd3137 *ktd = dev_get_priv(dev);
	struct udevice *vin;
	int ret, val;

	if (device_get_uclass_id(dev->parent) != UCLASS_I2C)
		return -EPROTONOSUPPORT;

	ktd->dev = dev;

	ret = device_get_supply_regulator(dev, "vin-supply", &vin);
	if (!ret) {
		ret = regulator_set_enable_if_allowed(vin, true);
		if (ret && ret != -ENOSYS) {
			dev_err(dev, "failed to power up\n");
			return ret;
		}
	} else if (ret != -ENOENT && ret != -ENOSYS) {
		dev_err(dev, "failed to get vin-supply\n");
		return ret;
	}

	ret = gpio_request_by_name(dev, "enable-gpios", 0, &ktd->enable_gpio,
				   GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get enable gpio\n");
		return ret;
	}

	/* HWEN should stay HIGH for at least 150us before I2C can be used */
	udelay(150);

	val = dm_i2c_reg_read(dev, KTD3137_REG_DEV_ID);
	if (val < 0) {
		dev_err(dev, "failed to read chip id\n");
		return val;
	}

	if (val != KTD3137_DEV_ID_A && val != KTD3137_DEV_ID_B) {
		dev_err(dev, "unsupported chip id: 0x%02x\n", val);
		return -ENODEV;
	}

	ret = ktd3137_parse_dt(ktd);
	if (ret)
		return ret;

	ret = ktd3137_init(ktd);
	if (ret) {
		dev_err(dev, "failed to init chip\n");
		return ret;
	}

	return 0;
}

static const struct udevice_id ktd3137_match_table[] = {
	{ .compatible = "kinetic,ktd3137" },
	{ }
};

U_BOOT_DRIVER(ktd3137_backlight) = {
	.name		= "ktd3137_backlight",
	.id		= UCLASS_PANEL_BACKLIGHT,
	.of_match	= ktd3137_match_table,
	.probe		= ktd3137_probe,
	.ops		= &ktd3137_backlight_ops,
	.priv_auto	= sizeof(struct ktd3137),
};
