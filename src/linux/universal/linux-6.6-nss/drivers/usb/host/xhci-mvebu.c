// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2014 Marvell
 * Author: Gregory CLEMENT <gregory.clement@free-electrons.com>
 */

#include <linux/io.h>
#include <linux/mbus.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include "xhci-mvebu.h"
#include "xhci.h"

#define USB3_MAX_WINDOWS	4
#define USB3_WIN_CTRL(w)	(0x0 + ((w) * 8))
#define USB3_WIN_BASE(w)	(0x4 + ((w) * 8))

#define BLKN_GPIO         50

static void xhci_mvebu_mbus_config(void __iomem *base,
			const struct mbus_dram_target_info *dram)
{
	int win;

	/* Clear all existing windows */
	for (win = 0; win < USB3_MAX_WINDOWS; win++) {
		writel(0, base + USB3_WIN_CTRL(win));
		writel(0, base + USB3_WIN_BASE(win));
	}

	/* Program each DRAM CS in a seperate window */
	for (win = 0; win < dram->num_cs; win++) {
		const struct mbus_dram_window *cs = &dram->cs[win];

		writel(((cs->size - 1) & 0xffff0000) | (cs->mbus_attr << 8) |
		       (dram->mbus_dram_target_id << 4) | 1,
		       base + USB3_WIN_CTRL(win));

		writel((cs->base & 0xffff0000), base + USB3_WIN_BASE(win));
	}
}

int xhci_mvebu_mbus_init_quirk(struct usb_hcd *hcd)
{
	struct device *dev = hcd->self.controller;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource	*res;
	void __iomem *base;
	const struct mbus_dram_target_info *dram;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -ENODEV;

	/*
	 * We don't use devm_ioremap() because this mapping should
	 * only exists for the duration of this probe function.
	 */
	base = ioremap(res->start, resource_size(res));
	if (!base)
		return -ENODEV;

	dram = mv_mbus_dram_info();
	xhci_mvebu_mbus_config(base, dram);

	/*
	 * This memory area was only needed to configure the MBus
	 * windows, and is therefore no longer useful.
	 */
	iounmap(base);

	return 0;
}

int xhci_mvebu_vbus_init_quirk(void)
{
        int gpio = BLKN_GPIO;

        gpio_set_value(gpio, GPIOF_OUT_INIT_LOW);
        mdelay(3000);
        gpio_set_value(gpio, GPIOF_OUT_INIT_HIGH);
        return 0;
}

