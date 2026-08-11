/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __MTK_DISP_DDP_H
#define __MTK_DISP_DDP_H

#include <panel.h>

void mtk_ccorr_config(struct udevice *dev, struct display_timing *timing);
void mtk_ccorr_start(struct udevice *dev);

void mtk_aal_config(struct udevice *dev, struct display_timing *timing);
void mtk_aal_start(struct udevice *dev);

void mtk_gamma_config(struct udevice *dev, struct display_timing *timing);
void mtk_gamma_start(struct udevice *dev);

void mtk_dither_config(struct udevice *dev, struct display_timing *timing);
void mtk_dither_start(struct udevice *dev);

void mtk_ufoe_start(struct udevice *dev);

#endif /* __MTK_DISP_DDP_H */
