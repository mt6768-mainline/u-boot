// SPDX-License-Identifier: GPL-2.0

#include <dm/device_compat.h>
#include <dm/ofnode_graph.h>
#include <dm/platdata.h>
#include <dm/read.h>
#include <dm/uclass.h>
#include <panel.h>
#include <power-domain.h>
#include <reset.h>
#include <video.h>

#include <soc/mediatek/mtk-mmsys.h>
#include <soc/mediatek/mtk-mutex.h>

#include "mtk_disp.h"
#include "mtk_disp_ovl.h"
#include "mtk_disp_rdma.h"
#include "mtk_disp_color.h"

struct mtk_ddp_comp_funcs {
	void (*config)(struct udevice *dev, struct display_timing *timing);
	void (*layer_config)(struct udevice *dev, struct video_uc_plat *plat,
			    struct display_timing *timing);
	void (*start)(struct udevice *dev);
};

struct mtk_ddp_match {
	const char *compatible;
	enum uclass_id uclass;
	enum mtk_ddp_comp_id comp_id;
	const struct mtk_ddp_comp_funcs *funcs;
};

struct mtk_ddp_node {
	struct udevice *dev;
	enum mtk_ddp_comp_id comp_id;
	const struct mtk_ddp_comp_funcs *funcs;
};

static const struct mtk_ddp_comp_funcs ddp_ovl = {
	.config = mtk_ovl_config,
	.layer_config = mtk_ovl_layer_config,
	.start = mtk_ovl_start,
};

static const struct mtk_ddp_comp_funcs ddp_rdma = {
	.config = mtk_rdma_config,
	/* layer_config is not needed for direct mode */
	.start = mtk_rdma_start,
};

static const struct mtk_ddp_comp_funcs ddp_color = {
	.config = mtk_color_config,
	.start = mtk_color_start,
};

static const struct mtk_ddp_match mtk_ddp_matches[] = {
	{ "mediatek,mt6572-disp-ovl",	UCLASS_MISC, DDP_COMPONENT_OVL0, &ddp_ovl },
	{ "mediatek,mt6572-disp-rdma",	UCLASS_MISC, DDP_COMPONENT_RDMA0, &ddp_rdma },
	{ "mediatek,mt2701-disp-color",	UCLASS_MISC, DDP_COMPONENT_COLOR0, &ddp_color },
	{ "mediatek,mt2701-disp-pwm",	UCLASS_MISC, DDP_COMPONENT_BLS, NULL },
	{ "mediatek,mt6572-dsi",	UCLASS_DSI_HOST, DDP_COMPONENT_DSI0, NULL },
};

static const struct mtk_ddp_match *mtk_disp_find_match(ofnode node)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_ddp_matches); i++)
		if (ofnode_device_is_compatible(node, mtk_ddp_matches[i].compatible))
			return &mtk_ddp_matches[i];

	return NULL;
}

static int mtk_video_bind(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);

	/* 1920x1080 as safe default for 65xx */
	plat->size = 1920 * 1080 * VNBYTES(VIDEO_BPP32);

	return 0;
}

static int mtk_video_probe(struct udevice *dev)
{
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct udevice *mmsys, *mutex, *cdev, *panel;
	struct mtk_ddp_node nodes[10];
	struct display_timing timing;
	const struct mtk_ddp_match *match;
	ofnode curr_node, next_node;
	int i, ret, route_count;
	struct mtk_mutex *mtx;

	/* Always XRGB8888 */
	uc_priv->bpix = VIDEO_BPP32;
	uc_priv->format = VIDEO_X8R8G8B8;

	video_set_flush_dcache(dev, true);

	log_debug("bringing up display pipeline...\n");

	ret = uclass_get_device_by_driver(UCLASS_SYSCON, DM_DRIVER_GET(mtk_mmsys), &mmsys);
	if (ret)
		return log_ret(ret);

	ret = dev_power_domain_on(mmsys);
	if (ret)
		return log_ret(ret);

	ret = uclass_get_device_by_driver(UCLASS_MEMORY, DM_DRIVER_GET(mtk_smi_common), &cdev);
	if (ret)
		return log_ret(ret);

	ret = uclass_get_device_by_driver(UCLASS_MEMORY, DM_DRIVER_GET(mtk_smi_larb), &cdev);
	if (ret)
		return log_ret(ret);

	ret = uclass_get_device_by_driver(UCLASS_MISC, DM_DRIVER_GET(mtk_mutex), &mutex);
	if (ret)
		return log_ret(ret);

	for (i = 0, curr_node = dev_ofnode(mmsys);
	     ofnode_valid(curr_node) && i < ARRAY_SIZE(nodes);
	     i++, curr_node = next_node) {
		if (i == 0)
			next_node = ofnode_graph_get_remote_node(curr_node, -1, 0);
		else
			next_node = ofnode_graph_get_remote_node(curr_node, 1, 0);

		if (!ofnode_valid(next_node))
			break;

		match = mtk_disp_find_match(next_node);
		if (!match) {
			log_err("unsupported display component %s\n", ofnode_get_name(next_node));
			return log_ret(-ENODEV);
		}

		ret = uclass_get_device_by_ofnode(match->uclass, next_node, &cdev);
		if (ret)
			log_warning("failed to probe %s\n", ofnode_get_name(next_node));

		nodes[i].dev = cdev;
		nodes[i].comp_id = match->comp_id;
		nodes[i].funcs = match->funcs;

		if (match->comp_id == DDP_COMPONENT_DSI0) {
			i++;
			break;
		}
	}
	route_count = i;

	if (route_count == 0) {
		log_err("no display routes found\n");
		return log_ret(-EINVAL);
	}
	if (nodes[route_count - 1].comp_id != DDP_COMPONENT_DSI0)
		return log_ret(-EINVAL);

	log_debug("we have %d routes:\n", route_count);
	for (i = 0; i < route_count; i++)
		log_debug("%s\n", nodes[i].dev->name);

	/* Once DSI is probed, the panel should be ready too */
	ret = uclass_first_device_err(UCLASS_PANEL, &panel);
	if (ret)
		return log_ret(ret);

	ret = panel_get_display_timing(panel, &timing);
	if (ret)
		return log_ret(ret);
	if (!timing.hactive.typ || !timing.vactive.typ ||
	    (u64)timing.hactive.typ * timing.vactive.typ * VNBYTES(VIDEO_BPP32) > plat->size)
		return log_ret(-EINVAL);

	uc_priv->xsize = timing.hactive.typ;
	uc_priv->ysize = timing.vactive.typ;
	uc_priv->rot = dev_read_u32_default(panel, "rot", 0);

	mtx = mtk_mutex_get(mutex);
	if (IS_ERR(mtx))
		return PTR_ERR(mtx);
	for (i = 0; i < route_count - 1; i++) {
		mtk_mmsys_ddp_connect(mmsys, nodes[i].comp_id, nodes[i + 1].comp_id);
		mtk_mutex_add_comp(mtx, nodes[i].comp_id);
	}
	mtk_mutex_add_comp(mtx, nodes[i].comp_id);
	mtk_mutex_enable(mtx);

	for (i = 0; i < route_count; i++)
		if (nodes[i].funcs && nodes[i].funcs->config)
			nodes[i].funcs->config(nodes[i].dev, &timing);

	for (i = 0; i < route_count; i++)
		if (nodes[i].funcs && nodes[i].funcs->layer_config)
			nodes[i].funcs->layer_config(nodes[i].dev, plat, &timing);

	mtk_mutex_acquire(mtx);
	for (i = 0; i < route_count; i++)
		if (nodes[i].funcs && nodes[i].funcs->start)
			nodes[i].funcs->start(nodes[i].dev);
	mtk_mutex_release(mtx);

	ret = mtk_dsi_ddp_start(nodes[route_count - 1].dev);
	if (ret) {
		mtk_mutex_disable(mtx);
		mtk_mutex_put(mtx);
	}

	return ret;
}

U_BOOT_DRIVER(mtk_video) = {
	.name = "mtk_video",
	.id = UCLASS_VIDEO,
	.plat_auto = sizeof(struct video_uc_plat),
	.bind = mtk_video_bind,
	.probe = mtk_video_probe,
	.flags = DM_FLAG_PRE_RELOC,
};

U_BOOT_DRVINFO(mtk_video_inst) = {
    .name = "mtk_video",
};
