// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * block device NVMEM provider
 *
 * Copyright (c) 2024 Daniel Golle <daniel@makrotopia.org>
 *
 * Useful on devices using a partition on an eMMC for MAC addresses or
 * Wi-Fi calibration EEPROM data.
 */

#include <linux/blkdev.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/pagemap.h>
#include <linux/property.h>

/* List of all NVMEM devices */
static LIST_HEAD(nvmem_devices);
static DEFINE_MUTEX(devices_mutex);

struct blk_nvmem {
	struct nvmem_device	*nvmem;
	struct device		*dev;
	struct list_head	list;
};

static int blk_nvmem_reg_read(void *priv, unsigned int from,
			      void *val, size_t bytes)
{
	blk_mode_t mode = BLK_OPEN_READ | BLK_OPEN_RESTRICT_WRITES;
	unsigned long offs = from & ~PAGE_MASK, to_read;
	pgoff_t f_index = from >> PAGE_SHIFT;
	struct blk_nvmem *bnv = priv;
	size_t bytes_left = bytes;
	struct file *bdev_file;
	struct folio *folio;
	void *p;
	int ret = 0;

	bdev_file = bdev_file_open_by_dev(bnv->dev->devt, mode, priv, NULL);
	if (!bdev_file)
		return -ENODEV;

	if (IS_ERR(bdev_file))
		return PTR_ERR(bdev_file);

	while (bytes_left) {
		folio = read_mapping_folio(bdev_file->f_mapping, f_index++, NULL);
		if (IS_ERR(folio)) {
			ret = PTR_ERR(folio);
			goto err_release_bdev;
		}
		to_read = min_t(unsigned long, bytes_left, PAGE_SIZE - offs);
		p = folio_address(folio) + offset_in_folio(folio, offs);
		memcpy(val, p, to_read);
		offs = 0;
		bytes_left -= to_read;
		val += to_read;
		folio_put(folio);
	}

err_release_bdev:
	fput(bdev_file);

	return ret;
}

static int blk_nvmem_register(struct device *dev)
{
	struct block_device *bdev = dev_to_bdev(dev);
	struct device_node *np = dev_of_node(dev);
	struct nvmem_config config = {};
	struct blk_nvmem *bnv;

	/* skip devices which do not have a device tree node */
	if (!np)
		return 0;

	/* skip devices without an nvmem layout defined */
	if (!of_get_child_by_name(np, "nvmem-layout"))
		return 0;

	/*
	 * skip devices which don't have GENHD_FL_NVMEM set
	 *
	 * This flag is used for mtdblock and ubiblock devices because
	 * both, MTD and UBI already implement their own NVMEM provider.
	 * To avoid registering multiple NVMEM providers for the same
	 * device node, don't register the block NVMEM provider for them.
	 */
	if (!(bdev->bd_disk->flags & GENHD_FL_NVMEM))
		return 0;

	/*
	 * skip block device too large to be represented as NVMEM devices
	 * which are using an 'int' as address
	 */
	if (bdev_nr_bytes(bdev) > INT_MAX)
		return -EFBIG;

	bnv = kzalloc(sizeof(struct blk_nvmem), GFP_KERNEL);
	if (!bnv)
		return -ENOMEM;

	config.id = NVMEM_DEVID_NONE;
	config.dev = &bdev->bd_device;
	config.name = dev_name(&bdev->bd_device);
	config.owner = THIS_MODULE;
	config.priv = bnv;
	config.reg_read = blk_nvmem_reg_read;
	config.size = bdev_nr_bytes(bdev);
	config.word_size = 1;
	config.stride = 1;
	config.read_only = true;
	config.root_only = true;
	config.ignore_wp = true;
	config.of_node = to_of_node(dev->fwnode);

	bnv->dev = &bdev->bd_device;
	bnv->nvmem = nvmem_register(&config);
	if (IS_ERR(bnv->nvmem)) {
		dev_err_probe(&bdev->bd_device, PTR_ERR(bnv->nvmem),
			      "Failed to register NVMEM device\n");

		kfree(bnv);
		return PTR_ERR(bnv->nvmem);
	}

	mutex_lock(&devices_mutex);
	list_add_tail(&bnv->list, &nvmem_devices);
	mutex_unlock(&devices_mutex);

	return 0;
}

static void blk_nvmem_unregister(struct device *dev)
{
	struct blk_nvmem *bnv_c, *bnv = NULL;

	mutex_lock(&devices_mutex);
	list_for_each_entry(bnv_c, &nvmem_devices, list) {
		if (bnv_c->dev == dev) {
			bnv = bnv_c;
			break;
		}
	}

	if (!bnv) {
		mutex_unlock(&devices_mutex);
		return;
	}

	list_del(&bnv->list);
	mutex_unlock(&devices_mutex);
	nvmem_unregister(bnv->nvmem);
	kfree(bnv);
}

static int blk_nvmem_handler(struct notifier_block *this, unsigned long code, void *obj)
{
	struct device *dev = (struct device *)obj;

	switch (code) {
	case BLK_DEVICE_ADD:
		return blk_nvmem_register(dev);
		break;
	case BLK_DEVICE_REMOVE:
		blk_nvmem_unregister(dev);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct notifier_block blk_nvmem_notifier = {
	.notifier_call = blk_nvmem_handler,
};

static int __init blk_nvmem_init(void)
{
	blk_register_notify(&blk_nvmem_notifier);

	return 0;
}

static void __exit blk_nvmem_exit(void)
{
	blk_unregister_notify(&blk_nvmem_notifier);
}

module_init(blk_nvmem_init);
module_exit(blk_nvmem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Golle <daniel@makrotopia.org>");
MODULE_DESCRIPTION("block device NVMEM provider");
