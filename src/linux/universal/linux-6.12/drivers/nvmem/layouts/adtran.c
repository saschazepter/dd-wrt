// SPDX-License-Identifier: GPL-2.0
#include <linux/ctype.h>
#include <linux/etherdevice.h>
#include <linux/nvmem-consumer.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/platform_device.h>

/*
 * Adtran devices usually come with a main MAC address ending on 0 and
 * hence may have up to 16 MAC addresses per device.
 * The main MAC address is stored as variable MFG_MAC in ASCII format.
 */
static int adtran_mac_address_pp(void *priv, const char *id, int index,
				  unsigned int offset, void *buf,
				  size_t bytes)
{
	u8 mac[ETH_ALEN];

	if (WARN_ON(bytes != 3 * ETH_ALEN - 1))
		return -EINVAL;

	if (!mac_pton(buf, mac))
		return -EINVAL;

	if (index)
		eth_addr_add(mac, index);

	ether_addr_copy(buf, mac);

	return 0;
}

static int adtran_add_cells(struct nvmem_layout *layout)
{
	struct nvmem_device *nvmem = layout->nvmem;
	struct nvmem_cell_info info;
	struct device_node *layout_np;
	char mfginfo[1024], *c, *t, *p;
	int ret = -EINVAL;

	ret = nvmem_device_read(nvmem, 0, sizeof(mfginfo), mfginfo);
	if (ret < 0)
		return ret;
	else if (ret != sizeof(mfginfo))
		return -EIO;

	layout_np = of_nvmem_layout_get_container(nvmem);
	if (!layout_np)
		return -ENOENT;

	c = mfginfo;
	while (*c != 0xff) {
		memset(&info, 0, sizeof(info));
		if (*c == '#')
			goto nextline;

		t = strchr(c, '=');
		if (!t)
			goto nextline;

		*t = '\0';
		++t;
		info.offset = t - mfginfo;
		/* process variable name: convert to lower-case, '_' -> '-' */
		p = c;
		do {
			*p = tolower(*p);
			if (*p == '_')
				*p = '-';
		} while (*++p);
		info.name = c;
		c = strchr(t, 0xa); /* find newline */
		if (!c)
			break;

		info.bytes = c - t;
		if (!strcmp(info.name, "mfg-mac")) {
			info.raw_len = info.bytes;
			info.bytes = ETH_ALEN;
			info.read_post_process = adtran_mac_address_pp;
		}

		info.np = of_get_child_by_name(layout_np, info.name);
		ret = nvmem_add_one_cell(nvmem, &info);
		if (ret)
			break;

		++c;
		continue;

nextline:
		c = strchr(c, 0xa); /* find newline */
		if (!c)
			break;
		++c;
	}

	of_node_put(layout_np);

	return ret;
}

static int adtran_probe(struct nvmem_layout *layout)
{
	layout->add_cells = adtran_add_cells;

	return nvmem_layout_register(layout);
}

static void adtran_remove(struct nvmem_layout *layout)
{
	nvmem_layout_unregister(layout);
}

static const struct of_device_id adtran_of_match_table[] = {
	{ .compatible = "adtran,mfginfo" },
	{},
};
MODULE_DEVICE_TABLE(of, adtran_of_match_table);

static struct nvmem_layout_driver adtran_layout = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "adtran-layout",
		.of_match_table = adtran_of_match_table,
	},
	.probe = adtran_probe,
	.remove = adtran_remove,
};
module_nvmem_layout_driver(adtran_layout);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Golle <daniel@makrotopia.org>");
MODULE_DESCRIPTION("NVMEM layout driver for Adtran mfginfo");
