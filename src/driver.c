// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Panasonic Let's Note CF-SZ6 USB always-on charging support.
 *
 * This is an independent implementation of the firmware protocol used by
 * Panasonic's System Interface Device (ACPI MAT0021). It does not contain or
 * link any Panasonic code.
 */

#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/unaligned.h>
#include <asm/io.h>

#include "protocol.h"

#define DRIVER_NAME             "panasonic_usb_charge"
#define BIOS_ROM_BASE           0x000f0000UL
#define BIOS_ROM_SIZE           0x00010000UL
#define MISC_SEARCH_START       0x0000c000U
#define MISC_SEARCH_STEP        0x80U
#define MISC_TABLE_COPY_SIZE    0x80U
#define MISC_SIGNATURE          "MEI_"
#define MISC_LENGTH_OFFSET      0x14U
#define MISC_LENGTH_UNIT        0x10U
#define MISC_ASMI_TYPE_OFFSET   0x17U
#define MISC_SMI_PORT_OFFSET    0x52U
#define MISC_SMI_VALUE_OFFSET   0x54U

struct usbchg_device {
	struct device *dev;
	void __iomem *bios_rom;
	void *packet_page;
	phys_addr_t packet_phys;
	unsigned long smi_port;
	u8 smi_value;
	u8 asmi_type;
	struct mutex lock;
};

static bool force;
module_param(force, bool, 0444);
MODULE_PARM_DESC(force, "Allow probing unsupported Panasonic models");

static bool supported_machine(void)
{
	const char *vendor = dmi_get_system_info(DMI_SYS_VENDOR);
	const char *product = dmi_get_system_info(DMI_PRODUCT_NAME);

	return vendor && product && !strcmp(vendor, "Panasonic Corporation") &&
	       !strncmp(product, "CFSZ6", 5);
}

static bool table_checksum_valid(const u8 *table, size_t available)
{
	size_t length;
	u8 sum = 0;
	size_t i;

	if (available <= MISC_LENGTH_OFFSET)
		return false;
	length = (size_t)table[MISC_LENGTH_OFFSET] * MISC_LENGTH_UNIT;
	if (!length || length > available)
		return false;
	for (i = 0; i < length; i++)
		sum += table[i];
	return sum == 0;
}

static int find_misc_table(struct usbchg_device *usbchg, u8 *table)
{
	u8 candidate[MISC_TABLE_COPY_SIZE];
	size_t offset;

	for (offset = MISC_SEARCH_START;
	     offset + MISC_TABLE_COPY_SIZE <= BIOS_ROM_SIZE;
	     offset += MISC_SEARCH_STEP) {
		memcpy_fromio(candidate, usbchg->bios_rom + offset,
			      sizeof(candidate));
		if (memcmp(candidate, MISC_SIGNATURE, 4))
			continue;
		if (!table_checksum_valid(candidate, sizeof(candidate))) {
			dev_warn(usbchg->dev,
				 "ignoring Panasonic MISC table with bad checksum at %#lx\n",
				 BIOS_ROM_BASE + offset);
			continue;
		}
		memcpy(table, candidate, MISC_TABLE_COPY_SIZE);
		dev_info(usbchg->dev, "found Panasonic MISC table at %#lx\n",
			 BIOS_ROM_BASE + offset);
		return 0;
	}

	return -ENODEV;
}

static int firmware_call_locked(struct usbchg_device *usbchg,
				struct usbchg_packet *packet)
{
	unsigned long irq_flags;

	memcpy(usbchg->packet_page, packet, sizeof(*packet));
	wmb();

	/*
	 * Panasonic's x86-64 driver puts the low 32 bits of the packet's
	 * physical address in ESI, then issues OUT DX, AL. Keep that calling
	 * convention exactly: the SMM handler obtains the buffer address from
	 * ESI and executes synchronously before OUT returns.
	 */
	preempt_disable();
	local_irq_save(irq_flags);
	asm volatile("movl %0, %%esi\n\t"
		     "outb %%al, %%dx"
		     :
		     : "r" ((u32)usbchg->packet_phys),
		       "a" (usbchg->smi_value), "d" ((u16)usbchg->smi_port)
		     : "esi", "memory");
	local_irq_restore(irq_flags);
	preempt_enable();

	rmb();
	memcpy(packet, usbchg->packet_page, sizeof(*packet));
	if (packet->function) {
		dev_err(usbchg->dev, "firmware returned error %#02x\n",
			packet->function);
		return -EIO;
	}

	return 0;
}

static int firmware_call(struct usbchg_device *usbchg,
			 struct usbchg_packet *packet)
{
	int ret;

	mutex_lock(&usbchg->lock);
	ret = firmware_call_locked(usbchg, packet);
	mutex_unlock(&usbchg->lock);
	return ret;
}

static int read_flags(struct usbchg_device *usbchg, u8 *flags)
{
	struct usbchg_packet packet = usbchg_query_packet();
	int ret;

	ret = firmware_call(usbchg, &packet);
	if (!ret)
		*flags = packet.flags;
	return ret;
}

static int update_flags(struct usbchg_device *usbchg, u8 bit, bool enabled)
{
	struct usbchg_packet query = usbchg_query_packet();
	struct usbchg_packet update;
	bool always_on;
	bool ac_only;
	int ret;

	mutex_lock(&usbchg->lock);
	ret = firmware_call_locked(usbchg, &query);
	if (ret)
		goto out;

	always_on = !!(query.flags & USBCHG_ALWAYS_ON);
	ac_only = !!(query.flags & USBCHG_AC_ONLY);
	if (bit == USBCHG_ALWAYS_ON)
		always_on = enabled;
	else
		ac_only = enabled;

	update = usbchg_set_packet(always_on, ac_only);
	ret = firmware_call_locked(usbchg, &update);
out:
	mutex_unlock(&usbchg->lock);
	return ret;
}

static ssize_t always_on_charge_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct usbchg_device *usbchg = dev_get_drvdata(dev);
	u8 flags;
	int ret = read_flags(usbchg, &flags);

	if (ret)
		return ret;
	return sysfs_emit(buf, "%u\n", !!(flags & USBCHG_ALWAYS_ON));
}

static ssize_t always_on_charge_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct usbchg_device *usbchg = dev_get_drvdata(dev);
	bool enabled;
	int ret;

	ret = kstrtobool(buf, &enabled);
	if (ret)
		return ret;
	ret = update_flags(usbchg, USBCHG_ALWAYS_ON, enabled);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(always_on_charge);

static ssize_t ac_only_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct usbchg_device *usbchg = dev_get_drvdata(dev);
	u8 flags;
	int ret = read_flags(usbchg, &flags);

	if (ret)
		return ret;
	return sysfs_emit(buf, "%u\n", !!(flags & USBCHG_AC_ONLY));
}

static ssize_t ac_only_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct usbchg_device *usbchg = dev_get_drvdata(dev);
	bool enabled;
	int ret;

	ret = kstrtobool(buf, &enabled);
	if (ret)
		return ret;
	ret = update_flags(usbchg, USBCHG_AC_ONLY, enabled);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(ac_only);

static ssize_t raw_flags_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct usbchg_device *usbchg = dev_get_drvdata(dev);
	u8 flags;
	int ret = read_flags(usbchg, &flags);

	if (ret)
		return ret;
	return sysfs_emit(buf, "0x%02x\n", flags);
}
static DEVICE_ATTR_RO(raw_flags);

static struct attribute *usbchg_attrs[] = {
	&dev_attr_always_on_charge.attr,
	&dev_attr_ac_only.attr,
	&dev_attr_raw_flags.attr,
	NULL,
};
ATTRIBUTE_GROUPS(usbchg);

static int usbchg_probe(struct platform_device *pdev)
{
	struct usbchg_device *usbchg;
	u8 table[MISC_TABLE_COPY_SIZE];
	u8 flags;
	int ret;

	if (!force && !supported_machine()) {
		dev_err(&pdev->dev,
			"unsupported model; force=1 is required outside CF-SZ6\n");
		return -ENODEV;
	}

	usbchg = devm_kzalloc(&pdev->dev, sizeof(*usbchg), GFP_KERNEL);
	if (!usbchg)
		return -ENOMEM;
	usbchg->dev = &pdev->dev;
	mutex_init(&usbchg->lock);

	usbchg->bios_rom = devm_ioremap(&pdev->dev, BIOS_ROM_BASE,
					 BIOS_ROM_SIZE);
	if (!usbchg->bios_rom)
		return dev_err_probe(&pdev->dev, -ENOMEM,
				     "could not map legacy BIOS ROM\n");

	ret = find_misc_table(usbchg, table);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "Panasonic MISC table not found\n");

	usbchg->asmi_type = table[MISC_ASMI_TYPE_OFFSET] & 0x03;
	if (usbchg->asmi_type != 1 && usbchg->asmi_type != 2)
		return dev_err_probe(&pdev->dev, -EINVAL,
				     "unsupported ASMI type %u\n",
				     usbchg->asmi_type);

	usbchg->smi_port = get_unaligned_le16(table + MISC_SMI_PORT_OFFSET);
	usbchg->smi_value = get_unaligned_le16(table + MISC_SMI_VALUE_OFFSET);
	if (!usbchg->smi_port || usbchg->smi_port > 0xffff)
		return dev_err_probe(&pdev->dev, -EINVAL,
				     "invalid SMI port %#lx\n", usbchg->smi_port);
	if (!devm_request_region(&pdev->dev, usbchg->smi_port, 1,
				  DRIVER_NAME))
		return dev_err_probe(&pdev->dev, -EBUSY,
				     "SMI port %#lx is busy\n", usbchg->smi_port);

	usbchg->packet_page = (void *)__get_free_page(GFP_KERNEL | GFP_DMA32 |
						      __GFP_ZERO);
	if (!usbchg->packet_page)
		return -ENOMEM;
	usbchg->packet_phys = virt_to_phys(usbchg->packet_page);
	if (upper_32_bits(usbchg->packet_phys)) {
		free_page((unsigned long)usbchg->packet_page);
		return dev_err_probe(&pdev->dev, -ERANGE,
				     "firmware packet is above 4 GiB\n");
	}

	platform_set_drvdata(pdev, usbchg);
	ret = read_flags(usbchg, &flags);
	if (ret) {
		free_page((unsigned long)usbchg->packet_page);
		return dev_err_probe(&pdev->dev, ret,
				     "USB charge status query failed\n");
	}

	dev_info(&pdev->dev,
		 "ready: ASMI type %u, SMI port %#lx, always-on=%u, AC-only=%u\n",
		 usbchg->asmi_type, usbchg->smi_port,
		 !!(flags & USBCHG_ALWAYS_ON), !!(flags & USBCHG_AC_ONLY));
	return 0;
}

static void usbchg_remove(struct platform_device *pdev)
{
	struct usbchg_device *usbchg = platform_get_drvdata(pdev);

	free_page((unsigned long)usbchg->packet_page);
}

static const struct acpi_device_id usbchg_acpi_ids[] = {
	{ "MAT0021", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, usbchg_acpi_ids);

static struct platform_driver usbchg_driver = {
	.probe = usbchg_probe,
	.remove = usbchg_remove,
	.driver = {
		.name = DRIVER_NAME,
		.acpi_match_table = usbchg_acpi_ids,
		.dev_groups = usbchg_groups,
	},
};
module_platform_driver(usbchg_driver);

MODULE_AUTHOR("0xNOY");
MODULE_DESCRIPTION("Panasonic Let's Note CF-SZ6 USB always-on charging");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.0");
