/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __MTK_DISP_RDMA_H
#define __MTK_DISP_RDMA_H

#include <panel.h>
#include <video.h>

void mtk_rdma_start(struct udevice *dev);
void mtk_rdma_stop(struct udevice *dev);
void mtk_rdma_config(struct udevice *dev, struct display_timing *timing);
void mtk_rdma_layer_config(struct udevice *dev, struct video_uc_plat *plat, struct display_timing *timing);

#endif /* __MTK_DISP_RDMA_H */
