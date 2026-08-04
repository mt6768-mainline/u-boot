/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __MTK_DISP_COLOR_H
#define __MTK_DISP_COLOR_H

#include <dm/device.h>

void mtk_color_config(struct udevice *dev, struct display_timing *timing);
void mtk_color_start(struct udevice *dev);

#endif /* __MTK_DISP_COLOR_H */
