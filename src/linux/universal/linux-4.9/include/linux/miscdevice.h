#ifndef _LINUX_MISCDEVICE_H
#define _LINUX_MISCDEVICE_H
#include <linux/major.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/device.h>

/*
 *	These allocations are managed by device@lanana.org. If you use an
 *	entry that is not in assigned your entry may well be moved and
 *	reassigned, or set dynamic if a fixed value is not justified.
 */

#define PSMOUSE_MINOR		1
#define MS_BUSMOUSE_MINOR	2
#define ATIXL_BUSMOUSE_MINOR	3
/*#define AMIGAMOUSE_MINOR	4	FIXME OBSOLETE */
#define ATARIMOUSE_MINOR	5
#define SUN_MOUSE_MINOR		6
#define APOLLO_MOUSE_MINOR	7
#define PC110PAD_MINOR		9
/*#define ADB_MOUSE_MINOR	10	FIXME OBSOLETE */
#define CRYPTODEV_MINOR		70	/* /dev/crypto */
#define WATCHDOG_MINOR		130	/* Watchdog timer     */
#define TEMP_MINOR		131	/* Temperature Sensor */
#define RTC_MINOR		135
#define EFI_RTC_MINOR		136	/* EFI Time services */
#define VHCI_MINOR		137
#define SUN_OPENPROM_MINOR	139
#define DMAPI_MINOR		140	/* DMAPI */
#define NVRAM_MINOR		144
#define SGI_MMTIMER		153
#define STORE_QUEUE_MINOR	155
#define I2O_MINOR		166
#define HWRNG_MINOR		183
#define MICROCODE_MINOR		184
#define VFIO_MINOR		196
#define TUN_MINOR		200
#define CUSE_MINOR		203
#define MWAVE_MINOR		219	/* ACP/Mwave Modem */
#define MPT_MINOR		220
#define MPT2SAS_MINOR		221
#define MPT3SAS_MINOR		222
#define UINPUT_MINOR		223
#define MISC_MCELOG_MINOR	227
#define HPET_MINOR		228
#define FUSE_MINOR		229
#define KVM_MINOR		232
#define BTRFS_MINOR		234
#define AUTOFS_MINOR		235
#define MAPPER_CTRL_MINOR	236
#define LOOP_CTRL_MINOR		237
#define VHOST_NET_MINOR		238
#define UHID_MINOR		239
#define USERIO_MINOR		240
#define MISC_DYNAMIC_MINOR	255

struct device;
struct attribute_group;

struct miscdevice  {
	int minor;
	const char *name;
	const struct file_operations *fops;
	struct list_head list;
	struct device *parent;
	struct device *this_device;
	const struct attribute_group **groups;
	const char *nodename;
	umode_t mode;
};

extern int misc_register(struct miscdevice *misc);
extern void misc_deregister(struct miscdevice *misc);

/*
 * Helper macro for drivers that don't do anything special in module init / exit
 * call. This helps in eleminating of boilerplate code.
 */
#define module_misc_device(__misc_device) \
	module_driver(__misc_device, misc_register, misc_deregister)

#define MODULE_ALIAS_MISCDEV(minor)				\
	MODULE_ALIAS("char-major-" __stringify(MISC_MAJOR)	\
	"-" __stringify(minor))
#endif
