// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026
 * Based on Linux mt6779-keypad.c
 */

#include <dm.h>
#include <fdtdec.h>
#include <input.h>
#include <keyboard.h>
#include <key_matrix.h>
#include <log.h>
#include <stdio_dev.h>
#include <time.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/input.h>

#define MTK_KPD_MEM             0x0004
#define MTK_KPD_DEBOUNCE        0x0018
#define MTK_KPD_DEBOUNCE_MASK   GENMASK(13, 0)
#define MTK_KPD_DEBOUNCE_MAX_MS 256
#define MTK_KPD_SEL             0x0020
#define MTK_KPD_SEL_DOUBLE_KP_MODE  BIT(0)
#define MTK_KPD_SEL_COL         GENMASK(15, 10)
#define MTK_KPD_SEL_ROW         GENMASK(9, 4)
#define MTK_KPD_SEL_COLMASK(c)  GENMASK((c) + 9, 10)
#define MTK_KPD_SEL_ROWMASK(r)  GENMASK((r) + 3, 4)
#define MTK_KPD_NUM_MEMS        5

enum {
	KBC_MAX_KPENT = 16,
	KBC_REPEAT_RATE_MS = 30,
	KBC_REPEAT_DELAY_MS = 240,
};

struct mtk_keypad_priv {
	struct input_config *input;
	struct key_matrix matrix;
	void __iomem *base;
	u32 keys_per_group;
	unsigned int last_poll_ms;
};

static void mtk_keypad_calc_row_col(struct mtk_keypad_priv *priv,
				    unsigned int key, unsigned int *row,
				    unsigned int *col)
{
	if (priv->keys_per_group == 2) {
		*row = key / 13;
		*col = (key % 13) / 2;
	} else {
		*row = key / 9;
		*col = key % 9;
	}
}

static int mtk_keypad_check(struct input_config *input)
{
	struct mtk_keypad_priv *priv = dev_get_priv(input->dev);
	struct key_matrix_key keys[KBC_MAX_KPENT];
	int keycodes[KBC_MAX_KPENT];
	int n_keycodes;
	int key_cnt;
	int i;

	if (get_timer(priv->last_poll_ms) < KBC_REPEAT_RATE_MS)
		return 1;

	priv->last_poll_ms = get_timer(0);

	for (key_cnt = 0, i = 0; i < MTK_KPD_NUM_MEMS; i++) {
		u32 word, pressed;

		word = readl(priv->base + MTK_KPD_MEM + (i * 4));
		pressed = (~word) & 0xffff;

		while (pressed) {
			u32 bit = ffs(pressed) - 1;
			u32 key = i * 16 + bit;
			u32 row, col;

			pressed &= ~BIT(bit);

			mtk_keypad_calc_row_col(priv, key, &row, &col);

			if (row >= priv->matrix.num_rows ||
			    col >= priv->matrix.num_cols)
				continue;

			if (key_cnt < KBC_MAX_KPENT) {
				keys[key_cnt].valid = 1;
				keys[key_cnt].row = row;
				keys[key_cnt].col = col;
				key_cnt++;
			}
		}
	}

	n_keycodes = key_matrix_decode(&priv->matrix, keys, key_cnt, keycodes,
				       KBC_MAX_KPENT);
	input_send_keycodes(priv->input, keycodes, n_keycodes);

	return 1;
}

static int mtk_keypad_start(struct udevice *dev)
{
	struct mtk_keypad_priv *priv = dev_get_priv(dev);
	u32 debounce, sel;

	debounce = dev_read_u32_default(dev, "debounce-delay-ms", 16);
	if (debounce > MTK_KPD_DEBOUNCE_MAX_MS) {
		debug("%s: Debounce time exceeds maximum %dms\n", __func__,
		      MTK_KPD_DEBOUNCE_MAX_MS);
		debounce = MTK_KPD_DEBOUNCE_MAX_MS;
	}

	writel((debounce * (1 << 5)) & MTK_KPD_DEBOUNCE_MASK,
	       priv->base + MTK_KPD_DEBOUNCE);

	sel = readl(priv->base + MTK_KPD_SEL);

	if (priv->keys_per_group == 2)
		sel |= MTK_KPD_SEL_DOUBLE_KP_MODE;
	else
		sel &= ~MTK_KPD_SEL_DOUBLE_KP_MODE;

	sel &= ~(MTK_KPD_SEL_ROW | MTK_KPD_SEL_COL);
	sel |= MTK_KPD_SEL_ROWMASK(priv->matrix.num_rows) & MTK_KPD_SEL_ROW;
	sel |= MTK_KPD_SEL_COLMASK(priv->matrix.num_cols) & MTK_KPD_SEL_COL;

	writel(sel, priv->base + MTK_KPD_SEL);

	priv->last_poll_ms = get_timer(0);

	debug("%s: MediaTek keypad ready\n", __func__);
	return 0;
}

static int mtk_keypad_probe(struct udevice *dev)
{
	struct mtk_keypad_priv *priv = dev_get_priv(dev);
	struct keyboard_priv *uc_priv = dev_get_uclass_priv(dev);
	struct stdio_dev *sdev = &uc_priv->sdev;
	struct input_config *input = &uc_priv->input;
	u32 rows, cols;
	int ret;

	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base) {
		debug("%s: No keyboard register found\n", __func__);
		return -EINVAL;
	}

	priv->keys_per_group =
		dev_read_u32_default(dev, "mediatek,keys-per-group", 1);
	if (priv->keys_per_group != 1 && priv->keys_per_group != 2) {
		debug("%s: Invalid keys-per-group: %d\n", __func__,
		      priv->keys_per_group);
		return -EINVAL;
	}

	input_set_delays(input, KBC_REPEAT_DELAY_MS, KBC_REPEAT_RATE_MS);

	rows = dev_read_u32_default(dev, "keypad,num-rows", 8);
	cols = dev_read_u32_default(dev, "keypad,num-columns", 9);

	ret = key_matrix_init(&priv->matrix, rows, cols, 1);
	if (ret) {
		debug("%s: Could not init key matrix: %d\n", __func__, ret);
		return ret;
	}

	ret = key_matrix_decode_fdt(dev, &priv->matrix);
	if (ret) {
		debug("%s: Could not decode key matrix from fdt: %d\n",
		      __func__, ret);
		return ret;
	}

	input_add_tables(input, false);

	priv->input = input;
	input->dev = dev;
	input->read_keys = mtk_keypad_check;

	strcpy(sdev->name, "mtk-keypad");
	ret = input_stdio_register(sdev);
	if (ret) {
		debug("%s: input_stdio_register() failed\n", __func__);
		return ret;
	}

	return 0;
}

static const struct keyboard_ops mtk_keypad_ops = {
	.start = mtk_keypad_start,
};

static const struct udevice_id mtk_keypad_ids[] = {
	{ .compatible = "mediatek,mt6779-keypad" },
	{ .compatible = "mediatek,mt6873-keypad" },
	{}
};

U_BOOT_DRIVER(mtk_keypad) = {
	.name = "mtk_keypad",
	.id = UCLASS_KEYBOARD,
	.of_match = mtk_keypad_ids,
	.probe = mtk_keypad_probe,
	.ops = &mtk_keypad_ops,
	.priv_auto = sizeof(struct mtk_keypad_priv),
};
