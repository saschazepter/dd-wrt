// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/mfd/core.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/of_platform.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define RTL8380_EXT_GPIO_INDIRECT_ACCESS	0xA09C
#define RTL8390_EXT_GPIO_INDIRECT_ACCESS	0x0224
#define RTL9300_EXT_GPIO_INDIRECT_ACCESS	0xC620

#define RTL83XX_AUX_MDIO_DATA_OFFSET		16
#define RTL83XX_AUX_MDIO_RCMD_FAIL		0

#define RTL93XX_AUX_MDIO_DATA_OFFSET		12
#define RTL93XX_AUX_MDIO_RCMD_FAIL		BIT(28)

#define REALTEK_AUX_MDIO_REG			GENMASK(11, 7)
#define REALTEK_AUX_MDIO_PHY_ADDR		GENMASK(6, 2)
#define REALTEK_AUX_MDIO_WRITE			BIT(1)
#define REALTEK_AUX_MDIO_READ			0
#define REALTEK_AUX_MDIO_EXEC			BIT(0)

struct realtek_aux_mdio_info {
	unsigned int cmd_reg;
	unsigned int data_offset;
	unsigned int rcmd_fail_mask;
	unsigned int timeout_us;
};

static const struct realtek_aux_mdio_info info_rtl838x = {
	.cmd_reg = RTL8380_EXT_GPIO_INDIRECT_ACCESS,
	.data_offset = RTL83XX_AUX_MDIO_DATA_OFFSET,
	.rcmd_fail_mask = RTL83XX_AUX_MDIO_RCMD_FAIL,
	.timeout_us = 1700,
};

static const struct realtek_aux_mdio_info info_rtl839x = {
	.cmd_reg = RTL8390_EXT_GPIO_INDIRECT_ACCESS,
	.data_offset = RTL83XX_AUX_MDIO_DATA_OFFSET,
	.rcmd_fail_mask = RTL83XX_AUX_MDIO_RCMD_FAIL,
	.timeout_us = 4120,
};

static const struct realtek_aux_mdio_info info_rtl930x = {
	.cmd_reg = RTL9300_EXT_GPIO_INDIRECT_ACCESS,
	.data_offset = RTL93XX_AUX_MDIO_DATA_OFFSET,
	.rcmd_fail_mask = RTL93XX_AUX_MDIO_RCMD_FAIL,
	.timeout_us = 19000,
};

struct realtek_aux_mdio_ctrl {
	struct device *dev;
	struct regmap *map;
	const struct realtek_aux_mdio_info *info;
};

#define mii_bus_to_ctrl(bus)	((struct realtek_aux_mdio_ctrl *) bus->priv)

static int realtek_aux_mdio_cmd(struct realtek_aux_mdio_ctrl *ctrl, int addr, int regnum,
		u32 rw_bit, u16 *data)
{
	unsigned int cmd;
	int err;

	cmd = rw_bit | REALTEK_AUX_MDIO_EXEC;
	cmd |= FIELD_PREP(REALTEK_AUX_MDIO_PHY_ADDR, addr);
	cmd |= FIELD_PREP(REALTEK_AUX_MDIO_REG, regnum);

	if (rw_bit == REALTEK_AUX_MDIO_WRITE)
		cmd |= *data << ctrl->info->data_offset;

	err = regmap_write(ctrl->map, ctrl->info->cmd_reg, cmd);
	if (err)
		return err;

	err = regmap_read_poll_timeout_atomic(ctrl->map, ctrl->info->cmd_reg, cmd,
			!(cmd & REALTEK_AUX_MDIO_EXEC), 3, ctrl->info->timeout_us);
	if (err)
		return err;

	if (rw_bit == REALTEK_AUX_MDIO_READ) {
		if (cmd & ctrl->info->rcmd_fail_mask)
			return -EIO;

		*data = (cmd >> ctrl->info->data_offset) & GENMASK(15, 0);
	}

	return 0;
}

static int realtek_aux_mdio_read(struct mii_bus *bus, int addr, int regnum)
{
	struct realtek_aux_mdio_ctrl *ctrl = mii_bus_to_ctrl(bus);
	u16 data;
	int err;

	err = realtek_aux_mdio_cmd(ctrl, addr, regnum, REALTEK_AUX_MDIO_READ, &data);

	if (err)
		return err;
	else
		return data;
}

static int realtek_aux_mdio_write(struct mii_bus *bus, int addr, int regnum, u16 val)
{
	struct realtek_aux_mdio_ctrl *ctrl = mii_bus_to_ctrl(bus);

	return realtek_aux_mdio_cmd(ctrl, addr, regnum, REALTEK_AUX_MDIO_WRITE, &val);
}

static int realtek_aux_mdio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct realtek_aux_mdio_ctrl *ctrl;
	struct mii_bus *bus;

	bus = devm_mdiobus_alloc_size(&pdev->dev, sizeof(*ctrl));
	if (!bus)
		return -ENOMEM;

	ctrl = bus->priv;
	ctrl->dev = &pdev->dev;
	ctrl->info = (const struct realtek_aux_mdio_info *) device_get_match_data(ctrl->dev);
	ctrl->map = syscon_node_to_regmap(np->parent);
	if (IS_ERR(ctrl->map))
		return PTR_ERR(ctrl->map);

	bus->name = "Realtek auxiliary MDIO bus";
	snprintf(bus->id, MII_BUS_ID_SIZE, "realtek-aux-mdio") ;
	bus->parent = ctrl->dev;
	bus->read = realtek_aux_mdio_read;
	bus->write = realtek_aux_mdio_write;
	/* Don't have interrupts */
	for (unsigned int i = 0; i < PHY_MAX_ADDR; i++)
		bus->irq[i] = PHY_POLL;

	return devm_of_mdiobus_register(ctrl->dev, bus, np);
}

static const struct of_device_id realtek_aux_mdio_of_match[] = {
	{
		.compatible = "realtek,rtl8380-aux-mdio",
		.data = &info_rtl838x,
	},
	{
		.compatible = "realtek,rtl8390-aux-mdio",
		.data = &info_rtl839x,
	},
	{
		.compatible = "realtek,rtl9300-aux-mdio",
		.data = &info_rtl930x,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, realtek_aux_mdio_of_match);

static struct platform_driver realtek_aux_mdio_driver = {
	.driver = {
		.name = "realtek-otto-aux-mdio",
		.of_match_table = realtek_aux_mdio_of_match
	},
	.probe = realtek_aux_mdio_probe,
};
module_platform_driver(realtek_aux_mdio_driver);

MODULE_AUTHOR("Sander Vanheule <sander@svanheule.net>");
MODULE_DESCRIPTION("Realtek otto auxiliary MDIO bus");
MODULE_LICENSE("GPL v2");
