// SPDX-License-Identifier: GPL-2.0
/*
 * MUSB OTG driver - support for Mentor's DMA controller
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2007 by Texas Instruments
 */

#include <dm.h>
#include <dm/device_compat.h>
#include <log.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <malloc.h>
#include <asm/io.h>

#include "musb_core.h"
#include "musb_dma.h"

#define MUSB_HSDMA_CHANNEL_OFFSET(_bchannel, _offset)		\
	(MUSB_HSDMA_BASE + (_bchannel << 4) + _offset)

#define musb_read_hsdma_addr(mbase, bchannel)	\
	musb_readl(mbase,	\
		   MUSB_HSDMA_CHANNEL_OFFSET(bchannel, MUSB_HSDMA_ADDRESS))

#define musb_write_hsdma_addr(mbase, bchannel, addr) \
	musb_writel(mbase, \
		    MUSB_HSDMA_CHANNEL_OFFSET(bchannel, MUSB_HSDMA_ADDRESS), \
		    addr)

#define musb_read_hsdma_count(mbase, bchannel)	\
	musb_readl(mbase,	\
		   MUSB_HSDMA_CHANNEL_OFFSET(bchannel, MUSB_HSDMA_COUNT))

#define musb_write_hsdma_count(mbase, bchannel, len) \
	musb_writel(mbase, \
		    MUSB_HSDMA_CHANNEL_OFFSET(bchannel, MUSB_HSDMA_COUNT), \
		    len)
/* control register (16-bit): */
#define MUSB_HSDMA_ENABLE_SHIFT		0
#define MUSB_HSDMA_TRANSMIT_SHIFT	1
#define MUSB_HSDMA_MODE1_SHIFT		2
#define MUSB_HSDMA_IRQENABLE_SHIFT	3
#define MUSB_HSDMA_ENDPOINT_SHIFT	4
#define MUSB_HSDMA_BUSERROR_SHIFT	8
#define MUSB_HSDMA_BURSTMODE_SHIFT	9
#define MUSB_HSDMA_BURSTMODE		(3 << MUSB_HSDMA_BURSTMODE_SHIFT)
#define MUSB_HSDMA_BURSTMODE_UNSPEC	0
#define MUSB_HSDMA_BURSTMODE_INCR4	1
#define MUSB_HSDMA_BURSTMODE_INCR8	2
#define MUSB_HSDMA_BURSTMODE_INCR16	3

#define MUSB_HSDMA_CHANNELS		8

struct musb_dma_controller;

struct musb_dma_channel {
	struct dma_channel		channel;
	struct musb_dma_controller	*controller;
	u32				start_addr;
	u32				len;
	u16				max_packet_sz;
	u8				idx;
	u8				epnum;
	u8				transmit;
};

struct musb_dma_controller {
	struct dma_controller		controller;
	struct musb_dma_channel		channel[MUSB_HSDMA_CHANNELS];
	void				*private_data;
	void __iomem			*base;
	u8				channel_count;
	u8				used_channels;
	int				irq;
};

static void dma_channel_release(struct dma_channel *channel);

static void dma_controller_stop(struct musb_dma_controller *controller)
{
	struct musb *musb = controller->private_data;
	struct dma_channel *channel;
	u8 bit;

	if (controller->used_channels != 0) {
		dev_err(musb->controller,
			"Stopping DMA controller while channel active\n");

		for (bit = 0; bit < MUSB_HSDMA_CHANNELS; bit++) {
			if (controller->used_channels & (1 << bit)) {
				channel = &controller->channel[bit].channel;
				dma_channel_release(channel);

				if (!controller->used_channels)
					break;
			}
		}
	}
}

static struct dma_channel *dma_channel_allocate(struct dma_controller *c,
				struct musb_hw_ep *hw_ep, u8 transmit)
{
	struct musb_dma_controller *controller = container_of(c,
			struct musb_dma_controller, controller);
	struct musb_dma_channel *musb_channel = NULL;
	struct dma_channel *channel = NULL;
	u8 bit;

	for (bit = 0; bit < MUSB_HSDMA_CHANNELS; bit++) {
		if (!(controller->used_channels & (1 << bit))) {
			controller->used_channels |= (1 << bit);
			musb_channel = &(controller->channel[bit]);
			musb_channel->controller = controller;
			musb_channel->idx = bit;
			musb_channel->epnum = hw_ep->epnum;
			musb_channel->transmit = transmit;
			channel = &(musb_channel->channel);
			channel->private_data = musb_channel;
			channel->status = MUSB_DMA_STATUS_FREE;
			channel->max_len = 0x100000;
			/* Tx => mode 1; Rx => mode 0 */
			channel->desired_mode = transmit;
			channel->actual_len = 0;
			break;
		}
	}

	return channel;
}

static void dma_channel_release(struct dma_channel *channel)
{
	struct musb_dma_channel *musb_channel = channel->private_data;

	channel->actual_len = 0;
	musb_channel->start_addr = 0;
	musb_channel->len = 0;

	musb_channel->controller->used_channels &=
		~(1 << musb_channel->idx);

	channel->status = MUSB_DMA_STATUS_UNKNOWN;
}

static void configure_channel(struct dma_channel *channel,
				u16 packet_sz, u8 mode,
				dma_addr_t dma_addr, u32 len)
{
	struct musb_dma_channel *musb_channel = channel->private_data;
	struct musb_dma_controller *controller = musb_channel->controller;
	struct musb *musb = controller->private_data;
	void __iomem *mbase = controller->base;
	u8 bchannel = musb_channel->idx;
	u16 csr = 0;

	dev_dbg(musb->controller, "%p, pkt_sz %d, addr %pad, len %d, mode %d\n",
			channel, packet_sz, &dma_addr, len, mode);

	if (mode)
		csr |= 1 << MUSB_HSDMA_MODE1_SHIFT;
	csr |= MUSB_HSDMA_BURSTMODE_INCR16
				<< MUSB_HSDMA_BURSTMODE_SHIFT;

	csr |= (musb_channel->epnum << MUSB_HSDMA_ENDPOINT_SHIFT)
		| (1 << MUSB_HSDMA_ENABLE_SHIFT)
		| (1 << MUSB_HSDMA_IRQENABLE_SHIFT)
		| (musb_channel->transmit
				? (1 << MUSB_HSDMA_TRANSMIT_SHIFT)
				: 0);

	/* address/count */
	musb_write_hsdma_addr(mbase, bchannel, dma_addr);
	musb_write_hsdma_count(mbase, bchannel, len);

	/* control (this should start things) */
	musb_writew(mbase,
		MUSB_HSDMA_CHANNEL_OFFSET(bchannel, MUSB_HSDMA_CONTROL),
		csr);
}

static int dma_channel_program(struct dma_channel *channel,
				u16 packet_sz, u8 mode,
				dma_addr_t dma_addr, u32 len)
{
	struct musb_dma_channel *musb_channel = channel->private_data;
	struct musb_dma_controller *controller = musb_channel->controller;
	struct musb *musb = controller->private_data;

	dev_dbg(musb->controller, "ep%d-%s pkt_sz %d, dma_addr %pad length %d, mode %d\n",
		musb_channel->epnum,
		musb_channel->transmit ? "Tx" : "Rx",
		packet_sz, &dma_addr, len, mode);

	if (channel->status == MUSB_DMA_STATUS_UNKNOWN ||
		channel->status == MUSB_DMA_STATUS_BUSY)
		return 0;

	/*
	 * The DMA engine in RTL1.8 and above cannot handle
	 * DMA addresses that are not aligned to a 4 byte boundary.
	 * It ends up masking the last two bits of the address
	 * programmed in DMA_ADDR.
	 *
	 * Fail such DMA transfers, so that the backup PIO mode
	 * can carry out the transfer
	 */
	if ((musb->hwvers >= MUSB_HWVERS_1800) && (dma_addr % 4))
		return 0;

	if (mode && len < packet_sz)
		return 0;

	channel->actual_len = 0;
	musb_channel->start_addr = dma_addr;
	musb_channel->len = len;
	musb_channel->max_packet_sz = packet_sz;
	channel->status = MUSB_DMA_STATUS_BUSY;

	configure_channel(channel, packet_sz, mode, dma_addr, len);

	return 1;
}

static int dma_channel_abort(struct dma_channel *channel)
{
	struct musb_dma_channel *musb_channel = channel->private_data;
	void __iomem *mbase = musb_channel->controller->base;
	struct musb *musb = musb_channel->controller->private_data;

	u8 bchannel = musb_channel->idx;
	u16 csr;

	if (channel->status == MUSB_DMA_STATUS_BUSY) {
		void __iomem *epio = musb->endpoints[musb_channel->epnum].regs;
		if (musb_channel->transmit) {
			musb_ep_select(mbase, musb_channel->epnum);

			/*
			 * The programming guide says that we must clear
			 * the DMAENAB bit before the DMAMODE bit...
			 */
			csr = musb_readw(epio, MUSB_TXCSR);
			csr &= ~(MUSB_TXCSR_AUTOSET | MUSB_TXCSR_DMAENAB);
			musb_writew(epio, MUSB_TXCSR, csr);
			csr &= ~MUSB_TXCSR_DMAMODE;
			musb_writew(epio, MUSB_TXCSR, csr);
		} else {
			musb_ep_select(mbase, musb_channel->epnum);

			csr = musb_readw(epio, MUSB_RXCSR);
			csr &= ~(MUSB_RXCSR_AUTOCLEAR |
				 MUSB_RXCSR_DMAENAB |
				 MUSB_RXCSR_DMAMODE);
			musb_writew(epio, MUSB_RXCSR, csr);
		}

		musb_writew(mbase,
			MUSB_HSDMA_CHANNEL_OFFSET(bchannel, MUSB_HSDMA_CONTROL),
			0);
		musb_write_hsdma_addr(mbase, bchannel, 0);
		musb_write_hsdma_count(mbase, bchannel, 0);
		channel->status = MUSB_DMA_STATUS_FREE;
	}

	return 0;
}

irqreturn_t dma_controller_irq(int irq, void *private_data)
{
	struct musb_dma_controller *controller = private_data;
	struct musb *musb = controller->private_data;
	struct musb_dma_channel *musb_channel;
	struct dma_channel *channel;

	void __iomem *mbase = controller->base;

	irqreturn_t retval = IRQ_NONE;

	unsigned long flags;

	u8 bchannel;
	u8 int_hsdma;

	u32 addr, count;
	u16 csr;

	spin_lock_irqsave(&musb->lock, flags);

	/* because U-Boot MUSB driver SUCKS i can't do musb_clearb */
	int_hsdma = musb_readb(mbase, MUSB_HSDMA_INTR);
	musb_writeb(mbase, MUSB_HSDMA_INTR, int_hsdma);

	if (!int_hsdma) {
		dev_dbg(musb->controller, "spurious DMA irq\n");

		for (bchannel = 0; bchannel < MUSB_HSDMA_CHANNELS; bchannel++) {
			musb_channel = (struct musb_dma_channel *)
					&(controller->channel[bchannel]);
			channel = &musb_channel->channel;
			if (channel->status == MUSB_DMA_STATUS_BUSY) {
				count = musb_read_hsdma_count(mbase, bchannel);

				if (count == 0)
					int_hsdma |= (1 << bchannel);
			}
		}

		dev_dbg(musb->controller, "int_hsdma = 0x%x\n", int_hsdma);

		if (!int_hsdma)
			goto done;
	}

	for (bchannel = 0; bchannel < MUSB_HSDMA_CHANNELS; bchannel++) {
		if (int_hsdma & (1 << bchannel)) {
			musb_channel = (struct musb_dma_channel *)
					&(controller->channel[bchannel]);
			channel = &musb_channel->channel;

			if (channel->status != MUSB_DMA_STATUS_BUSY)
				continue;

			csr = musb_readw(mbase,
					MUSB_HSDMA_CHANNEL_OFFSET(bchannel,
							MUSB_HSDMA_CONTROL));

			if (csr & (1 << MUSB_HSDMA_BUSERROR_SHIFT)) {
				musb_channel->channel.status =
					MUSB_DMA_STATUS_BUS_ABORT;
			} else {
				addr = musb_read_hsdma_addr(mbase,
						bchannel);
				channel->actual_len = addr
					- musb_channel->start_addr;

				dev_dbg(musb->controller, "ch %p, 0x%x -> 0x%x (%zu / %d) %s\n",
					channel, musb_channel->start_addr,
					addr, channel->actual_len,
					musb_channel->len,
					(channel->actual_len
						< musb_channel->len) ?
					"=> reconfig 0" : "=> complete");

				channel->status = MUSB_DMA_STATUS_FREE;

				/* completed */
				if (musb_channel->transmit &&
					(!channel->desired_mode ||
					(channel->actual_len %
					    musb_channel->max_packet_sz))) {
					u8  epnum  = musb_channel->epnum;
					u16 txcsr;
					void __iomem *epio = musb->endpoints[epnum].regs;

					/*
					 * The programming guide says that we
					 * must clear DMAENAB before DMAMODE.
					 */
					musb_ep_select(mbase, epnum);
					txcsr = musb_readw(epio, MUSB_TXCSR);

					if (channel->desired_mode == 1) {
						/* Mode 1: short packet */
						txcsr &= ~(MUSB_TXCSR_DMAENAB
							| MUSB_TXCSR_AUTOSET);
						musb_writew(epio, MUSB_TXCSR, txcsr);
						/* Send out the packet */
						txcsr &= ~MUSB_TXCSR_DMAMODE;
						txcsr |= MUSB_TXCSR_DMAENAB;
					}
					txcsr |=  MUSB_TXCSR_TXPKTRDY;
					musb_writew(epio, MUSB_TXCSR, txcsr);
				}
				musb_dma_completion(musb, musb_channel->epnum,
						    musb_channel->transmit);
			}
		}
	}

	retval = IRQ_HANDLED;
done:
	spin_unlock_irqrestore(&musb->lock, flags);
	return retval;
}

void musbhs_dma_controller_destroy(struct dma_controller *c)
{
	struct musb_dma_controller *controller = container_of(c,
			struct musb_dma_controller, controller);

	dma_controller_stop(controller);

	if (controller->irq)
		free_irq(controller->irq, c);

	free(controller);
}

static struct musb_dma_controller *
dma_controller_alloc(struct musb *musb, void __iomem *base)
{
	struct musb_dma_controller *controller;

	controller = calloc(1, sizeof(*controller));
	if (!controller)
		return NULL;

	controller->channel_count = MUSB_HSDMA_CHANNELS;
	controller->private_data = musb;
	controller->base = base;

	controller->controller.channel_alloc = dma_channel_allocate;
	controller->controller.channel_release = dma_channel_release;
	controller->controller.channel_program = dma_channel_program;
	controller->controller.channel_abort = dma_channel_abort;
	return controller;
}

struct dma_controller *
musbhs_dma_controller_create(struct musb *musb, void __iomem *base)
{
	struct musb_dma_controller *controller;
	struct device *dev = musb->controller;
	int irq = musb->nIrq;

	if (irq <= 0) {
		dev_err(dev, "No DMA interrupt line!\n");
		return NULL;
	}

	controller = dma_controller_alloc(musb, base);
	if (!controller)
		return NULL;

	if (request_irq(irq, dma_controller_irq, 0,
			dev_name(musb->controller), controller)) {
		dev_err(dev, "request_irq %d failed!\n", irq);
		musb->ops->dma_exit(&controller->controller);

		return NULL;
	}

	controller->irq = irq;

	return &controller->controller;
}

struct dma_controller *
musbhs_dma_controller_create_noirq(struct musb *musb, void __iomem *base)
{
	struct musb_dma_controller *controller;

	controller = dma_controller_alloc(musb, base);
	if (!controller)
		return NULL;

	return &controller->controller;
}
