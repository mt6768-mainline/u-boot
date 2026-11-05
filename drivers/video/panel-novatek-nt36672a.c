// SPDX-License-Identifier: GPL-2.0+

#include <backlight.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <mipi_dsi.h>
#include <panel.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <power/regulator.h>

#define mipi_dsi_dcs_write_var_seq(dsi, cmd, seq...) do {		\
		const u8 b[] = { cmd, seq };				\
		int ret;						\
		ret = mipi_dsi_dcs_write_buffer(dsi, b, ARRAY_SIZE(b));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static const char * const nt36672a_regulator_names[] = {
	"vddio-supply",
	"vddpos-supply",
	"vddneg-supply",
};

struct nt36672a_panel_desc {
	const struct drm_display_mode *display_mode;

	unsigned int width_mm;
	unsigned int height_mm;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;

	int (*send_init_cmds_1)(struct mipi_dsi_device *dsi);
	int (*send_init_cmds_2)(struct mipi_dsi_device *dsi);
};

struct nt36672a_panel {
	struct mipi_dsi_device *link;
	const struct nt36672a_panel_desc *desc;

	struct udevice *supplies[ARRAY_SIZE(nt36672a_regulator_names)];
	struct udevice *backlight;

	struct gpio_desc reset_gpio;
};

static int nt36672a_panel_power_on(struct nt36672a_panel *pinfo)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(pinfo->supplies); i++) {
		ret = regulator_set_enable_if_allowed(pinfo->supplies[i], true);
		if (ret && ret != -ENOSYS)
			return ret;
	}

	/*
	 * As per downstream kernel, Reset sequence of Tianma FHD panel requires the panel to
	 * be out of reset for 10ms, followed by being held in reset for 10ms. But for Android
	 * AOSP, we needed to bump it upto 200ms otherwise we get white screen sometimes.
	 * FIXME: Try to reduce this 200ms to a lesser value.
	 */
	dm_gpio_set_value(&pinfo->reset_gpio, 1);
	mdelay(200);
	dm_gpio_set_value(&pinfo->reset_gpio, 0);
	mdelay(200);

	return 0;
}

static int nt36672a_panel_prepare(struct udevice *dev)
{
	struct nt36672a_panel *pinfo = dev_get_priv(dev);
	int err;

	err = nt36672a_panel_power_on(pinfo);
	if (err < 0)
		goto poweroff;

	/* send first part of init cmds */
	if (pinfo->desc->send_init_cmds_1) {
		err = pinfo->desc->send_init_cmds_1(pinfo->link);
		if (err < 0)
			goto poweroff;
	}

	err = mipi_dsi_dcs_exit_sleep_mode(pinfo->link);
	if (err < 0)
		goto poweroff;

	/* 0x46 = 70 ms delay */
	mdelay(70);

	err = mipi_dsi_dcs_set_display_on(pinfo->link);
	if (err < 0)
		goto poweroff;

	/* Send rest of the init cmds */
	if (pinfo->desc->send_init_cmds_2) {
		err = pinfo->desc->send_init_cmds_2(pinfo->link);
		if (err < 0)
			goto poweroff;
	}

	mdelay(120);

	return 0;

poweroff:
	dm_gpio_set_value(&pinfo->reset_gpio, 0);
	return err;
}

static int nt36672a_panel_set_backlight(struct udevice *dev, int percent)
{
	struct nt36672a_panel *pinfo = dev_get_priv(dev);
	int ret;

	ret = backlight_enable(pinfo->backlight);
	if (ret)
		return ret;

	return backlight_set_brightness(pinfo->backlight, percent);
}

static int nt36672a_panel_get_modes(struct udevice *dev,
				    const struct drm_display_mode **modes)
{
	struct nt36672a_panel *pinfo = dev_get_priv(dev);

	*modes = pinfo->desc->display_mode;

	return 1;
}

static const struct panel_ops nt36672a_panel_ops = {
	.enable_backlight = nt36672a_panel_prepare,
	.set_backlight = nt36672a_panel_set_backlight,
	.get_modes = nt36672a_panel_get_modes,
};

static int tianma_fhd_video_send_init_cmds_1(struct mipi_dsi_device *dsi)
{
	u8 reg;

	/* skin enhancement mode */
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x22);
	mipi_dsi_dcs_write_seq(dsi, 0x00, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0x01, 0xc0);
	for (reg = 0x02; reg <= 0x10; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0x11, 0x50);
	mipi_dsi_dcs_write_seq(dsi, 0x12, 0x60);
	mipi_dsi_dcs_write_seq(dsi, 0x13, 0x70);
	mipi_dsi_dcs_write_seq(dsi, 0x14, 0x58);
	mipi_dsi_dcs_write_seq(dsi, 0x15, 0x68);
	mipi_dsi_dcs_write_seq(dsi, 0x16, 0x78);
	mipi_dsi_dcs_write_seq(dsi, 0x17, 0x77);
	mipi_dsi_dcs_write_seq(dsi, 0x18, 0x39);
	mipi_dsi_dcs_write_seq(dsi, 0x19, 0x2d);
	mipi_dsi_dcs_write_seq(dsi, 0x1a, 0x2e);
	mipi_dsi_dcs_write_seq(dsi, 0x1b, 0x32);
	mipi_dsi_dcs_write_seq(dsi, 0x1c, 0x37);
	mipi_dsi_dcs_write_seq(dsi, 0x1d, 0x3a);
	for (reg = 0x1e; reg <= 0x28; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0x2d, 0x00);
	for (reg = 0x2f; reg <= 0x3b; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0x3d, 0x40);
	for (reg = 0x3f; reg <= 0x52; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0x53, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x54, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x55, 0xfe);
	mipi_dsi_dcs_write_seq(dsi, 0x56, 0x77);
	mipi_dsi_dcs_write_seq(dsi, 0x58, 0xcd);
	mipi_dsi_dcs_write_seq(dsi, 0x59, 0xd0);
	mipi_dsi_dcs_write_seq(dsi, 0x5a, 0xd0);
	for (reg = 0x5b; reg <= 0x6f; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0x50);
	mipi_dsi_dcs_write_seq(dsi, 0x70, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x71, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x72, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x73, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x74, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0x75, 0x0c);
	mipi_dsi_dcs_write_seq(dsi, 0x76, 0x03);
	mipi_dsi_dcs_write_seq(dsi, 0x77, 0x09);
	mipi_dsi_dcs_write_seq(dsi, 0x78, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x79, 0x68);
	mipi_dsi_dcs_write_seq(dsi, 0x7a, 0x88);
	mipi_dsi_dcs_write_seq(dsi, 0x7c, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x7d, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x7e, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x7f, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x80, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x81, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x83, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x84, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x85, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x86, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x87, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x88, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0x89, 0x91);
	mipi_dsi_dcs_write_seq(dsi, 0x8a, 0x98);
	for (reg = 0x8b; reg <= 0x9f; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0xa0, 0x8a);
	mipi_dsi_dcs_write_seq(dsi, 0xa2, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0xa6, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0xa7, 0x80);
	for (reg = 0xa9; reg <= 0xaf; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0xb7, 0x76);
	mipi_dsi_dcs_write_seq(dsi, 0xb8, 0x76);
	mipi_dsi_dcs_write_seq(dsi, 0xb9, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0xba, 0x0d);
	mipi_dsi_dcs_write_seq(dsi, 0xbb, 0x14);
	mipi_dsi_dcs_write_seq(dsi, 0xbc, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0xbd, 0x18);
	mipi_dsi_dcs_write_seq(dsi, 0xbe, 0x1f);
	mipi_dsi_dcs_write_seq(dsi, 0xbf, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0xc0, 0x0d);
	mipi_dsi_dcs_write_seq(dsi, 0xc1, 0x14);
	mipi_dsi_dcs_write_seq(dsi, 0xc2, 0x03);
	mipi_dsi_dcs_write_seq(dsi, 0xc3, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0xc4, 0x0a);
	mipi_dsi_dcs_write_seq(dsi, 0xc5, 0xa0);
	mipi_dsi_dcs_write_seq(dsi, 0xc6, 0x55);
	mipi_dsi_dcs_write_seq(dsi, 0xc7, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0xc8, 0x39);
	mipi_dsi_dcs_write_seq(dsi, 0xc9, 0x44);
	mipi_dsi_dcs_write_seq(dsi, 0xca, 0x12);
	mipi_dsi_dcs_write_seq(dsi, 0xcd, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0xdb, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0xdc, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0xdd, 0x80);
	for (reg = 0xe0; reg <= 0xe4; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0x80);
	for (reg = 0xe5; reg <= 0xf6; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x23);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	/* dimming enable */
	mipi_dsi_dcs_write_seq(dsi, 0x01, 0x84);
	mipi_dsi_dcs_write_seq(dsi, 0x05, 0x2d);
	mipi_dsi_dcs_write_seq(dsi, 0x06, 0x00);
	/* resolution 1080*2246 */
	mipi_dsi_dcs_write_seq(dsi, 0x11, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x12, 0x7b);
	mipi_dsi_dcs_write_seq(dsi, 0x15, 0x6f);
	mipi_dsi_dcs_write_seq(dsi, 0x16, 0x0b);
	/* UI mode */
	mipi_dsi_dcs_write_seq(dsi, 0x29, 0x0a);
	for (reg = 0x30; reg <= 0x37; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0x38, 0xfc);
	mipi_dsi_dcs_write_seq(dsi, 0x39, 0xf8);
	mipi_dsi_dcs_write_seq(dsi, 0x3a, 0xf4);
	mipi_dsi_dcs_write_seq(dsi, 0x3b, 0xf1);
	mipi_dsi_dcs_write_seq(dsi, 0x3d, 0xee);
	mipi_dsi_dcs_write_seq(dsi, 0x3f, 0xeb);
	mipi_dsi_dcs_write_seq(dsi, 0x40, 0xe8);
	mipi_dsi_dcs_write_seq(dsi, 0x41, 0xe5);
	/* STILL mode */
	mipi_dsi_dcs_write_seq(dsi, 0x2a, 0x13);
	for (reg = 0x45; reg <= 0x4c; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0x4d, 0xed);
	mipi_dsi_dcs_write_seq(dsi, 0x4e, 0xd5);
	mipi_dsi_dcs_write_seq(dsi, 0x4f, 0xbf);
	mipi_dsi_dcs_write_seq(dsi, 0x50, 0xa6);
	mipi_dsi_dcs_write_seq(dsi, 0x51, 0x96);
	mipi_dsi_dcs_write_seq(dsi, 0x52, 0x86);
	mipi_dsi_dcs_write_seq(dsi, 0x53, 0x76);
	mipi_dsi_dcs_write_seq(dsi, 0x54, 0x66);
	/* MOVING mode */
	mipi_dsi_dcs_write_seq(dsi, 0x2b, 0x0e);
	for (reg = 0x58; reg <= 0x5f; reg++)
		mipi_dsi_dcs_write_var_seq(dsi, reg, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0x60, 0xf6);
	mipi_dsi_dcs_write_seq(dsi, 0x61, 0xea);
	mipi_dsi_dcs_write_seq(dsi, 0x62, 0xe1);
	mipi_dsi_dcs_write_seq(dsi, 0x63, 0xd8);
	mipi_dsi_dcs_write_seq(dsi, 0x64, 0xce);
	mipi_dsi_dcs_write_seq(dsi, 0x65, 0xc3);
	mipi_dsi_dcs_write_seq(dsi, 0x66, 0xba);
	mipi_dsi_dcs_write_seq(dsi, 0x67, 0xb3);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x25);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x05, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x26);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x1c, 0xaf);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x51, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0x53, 0x24);
	mipi_dsi_dcs_write_seq(dsi, 0x55, 0x00);

	return 0;
}

static int tianma_fhd_video_send_init_cmds_2(struct mipi_dsi_device *dsi)
{
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x24);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xc3, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xc4, 0x54);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x10);

	return 0;
}

static const struct drm_display_mode tianma_fhd_video_panel_default_mode = {
	.clock		= 161331,

	.hdisplay	= 1080,
	.hsync_start	= 1080 + 40,
	.hsync_end	= 1080 + 40 + 20,
	.htotal		= 1080 + 40 + 20 + 44,

	.vdisplay	= 2246,
	.vsync_start	= 2246 + 15,
	.vsync_end	= 2246 + 15 + 2,
	.vtotal		= 2246 + 15 + 2 + 8,
};

static const struct nt36672a_panel_desc tianma_fhd_video_panel_desc = {
	.display_mode = &tianma_fhd_video_panel_default_mode,

	.width_mm = 68,
	.height_mm = 136,

	.mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_VIDEO
			| MIPI_DSI_MODE_VIDEO_HSE
			| MIPI_DSI_CLOCK_NON_CONTINUOUS
			| MIPI_DSI_MODE_VIDEO_BURST,
	.format = MIPI_DSI_FMT_RGB888,
	.lanes = 4,
	.send_init_cmds_1 = tianma_fhd_video_send_init_cmds_1,
	.send_init_cmds_2 = tianma_fhd_video_send_init_cmds_2,
};

static int tianma_tl065fvxm03_01_fhd_video_send_init_cmds_1(struct mipi_dsi_device *dsi)
{
	/* Page 0x25 */
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x25);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x05, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x66, 0x48);

	/* Page 0x10 */
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x35, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x51, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0x53, 0x24);
	mipi_dsi_dcs_write_seq(dsi, 0x55, 0x00);

	return 0;
}

static const struct drm_display_mode tianma_tl065fvxm03_01_fhd_video_panel_default_mode = {
	.clock		= 171336,

	.hdisplay	= 1080,
	.hsync_start	= 1080 + 60,
	.hsync_end	= 1080 + 60 + 10,
	.htotal		= 1080 + 60 + 10 + 60,

	.vdisplay	= 2340,
	.vsync_start	= 2340 + 10,
	.vsync_end	= 2340 + 10 + 2,
	.vtotal		= 2340 + 10 + 2 + 8,
};

static const struct nt36672a_panel_desc tianma_tl065fvxm03_01_fhd_video_panel_desc = {
	.display_mode = &tianma_tl065fvxm03_01_fhd_video_panel_default_mode,

	.width_mm = 69,
	.height_mm = 151,

	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
		      MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.lanes = 4,
	.send_init_cmds_1 = tianma_tl065fvxm03_01_fhd_video_send_init_cmds_1,
};

static int nt36672a_panel_add(struct udevice *dev)
{
	struct nt36672a_panel *pinfo = dev_get_priv(dev);
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(pinfo->supplies); i++) {
		ret = device_get_supply_regulator(dev, nt36672a_regulator_names[i],
						  &pinfo->supplies[i]);
		if (ret) {
			dev_err(dev, "failed to get %s regulator: %d\n",
				nt36672a_regulator_names[i], ret);
			return ret;
		}
	}

	ret = gpio_request_by_name(dev, "reset-gpios", 0, &pinfo->reset_gpio,
				   GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "failed to get reset gpio from DT: %d\n", ret);
		return ret;
	}

	ret = uclass_get_device_by_phandle(UCLASS_PANEL_BACKLIGHT, dev,
					   "backlight", &pinfo->backlight);
	if (ret) {
		dev_err(dev, "failed to get backlight: %d\n", ret);
		return ret;
	}

	return 0;
}

static int nt36672a_panel_probe(struct udevice *dev)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct nt36672a_panel *pinfo = dev_get_priv(dev);
	const struct nt36672a_panel_desc *desc;
	struct mipi_dsi_device *dsi = plat->device;
	int err;

	desc = (const struct nt36672a_panel_desc *)dev_get_driver_data(dev);
	plat->mode_flags = desc->mode_flags;
	plat->format = desc->format;
	plat->lanes = desc->lanes;
	dsi->mode_flags = desc->mode_flags;
	dsi->format = desc->format;
	dsi->lanes = desc->lanes;
	pinfo->desc = desc;
	pinfo->link = dsi;

	err = nt36672a_panel_add(dev);
	if (err < 0)
		return err;

	return mipi_dsi_attach(dsi);
}

static const struct udevice_id tianma_fhd_video_of_match[] = {
	{ .compatible = "tianma,fhd-video", .data = (ulong)&tianma_fhd_video_panel_desc },
	{ .compatible = "tianma,tl065fvxm03-01", .data = (ulong)&tianma_tl065fvxm03_01_fhd_video_panel_desc },
	{ }
};

U_BOOT_DRIVER(nt36672a_panel_driver) = {
	.name		= "panel-tianma-nt36672a",
	.id		= UCLASS_PANEL,
	.of_match	= tianma_fhd_video_of_match,
	.ops		= &nt36672a_panel_ops,
	.probe		= nt36672a_panel_probe,
	.plat_auto	= sizeof(struct mipi_dsi_panel_plat),
	.priv_auto	= sizeof(struct nt36672a_panel),
};
