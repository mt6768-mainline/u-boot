#ifndef MTK_DISP_H
#define MTK_DISP_H

#include <asm/io.h>

static inline void mtk_ddp_write(unsigned int value, void __iomem *regs,
                                 unsigned int offset) {
	writel(value, regs + offset);
}

static inline void mtk_ddp_write_relaxed(unsigned int value, void __iomem *regs,
                                         unsigned int offset) {
	writel_relaxed(value, regs + offset);
}

static inline void mtk_ddp_write_mask(unsigned int value, void __iomem *regs,
                                      unsigned int offset, unsigned int mask) {
	u32 tmp = readl(regs + offset);

	tmp = (tmp & ~mask) | (value & mask);
	writel(tmp, regs + offset);
}

struct udevice;
int mtk_dsi_ddp_start(struct udevice *dev);
void mtk_dsi_ddp_stop(struct udevice *dev);

#endif
