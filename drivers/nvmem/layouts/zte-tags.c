// SPDX-License-Identifier: GPL-2.0-only
/* ZTE factory tags: CRC-protected variable-length key/value records. */

#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/sizes.h>

#include "zte-tags.h"

static int zte_tags_add_cells(struct nvmem_layout *layout)
{
	static const char * const names[] = { "mac-address", "mac-address-lan" };
	struct nvmem_device *nvmem = layout->nvmem;
	struct nvmem_cell_info cell = { .bytes = ETH_ALEN };
	struct device_node *np;
	u8 header[ZTE_TAGS_HEADER_SIZE];
	size_t size;
	u32 len;
	u8 *table;
	int offsets[ARRAY_SIZE(names)];
	int i, ret;

	ret = nvmem_device_read(nvmem, 0, sizeof(header), header);
	if (ret != sizeof(header))
		return ret < 0 ? ret : -EIO;
	if (get_unaligned_le32(header) != 0x33333333)
		return -EINVAL;
	len = get_unaligned_le32(header + 4);
	/* SR1010 has a 1 MiB tags partition; never allocate from unchecked data. */
	if (len > SZ_1M - sizeof(header))
		return -EINVAL;
	size = sizeof(header) + len;
	if (size > nvmem_dev_size(nvmem))
		return -EINVAL;
	table = kmalloc(size, GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = nvmem_device_read(nvmem, 0, size, table);
	if (ret != size) {
		ret = ret < 0 ? ret : -EIO;
		goto out;
	}
	/* Validate both configured addresses before publishing either cell. */
	for (i = 0; i < ARRAY_SIZE(names); i++) {
		ret = zte_tags_mac_offset(table, size, 0x0100 + i);
		if (ret < 0)
			goto out;
		offsets[i] = ret;
	}
	np = of_nvmem_layout_get_container(nvmem);
	for (i = 0; i < ARRAY_SIZE(names); i++) {
		cell.name = names[i];
		cell.offset = offsets[i];
		cell.np = of_get_child_by_name(np, cell.name);
		ret = nvmem_add_one_cell(nvmem, &cell);
		if (ret) {
			of_node_put(cell.np);
			break;
		}
	}
	of_node_put(np);
out:
	kfree(table);
	return ret;
}

static int zte_tags_probe(struct nvmem_layout *layout)
{
	layout->add_cells = zte_tags_add_cells;
	return nvmem_layout_register(layout);
}

static void zte_tags_remove(struct nvmem_layout *layout)
{
	nvmem_layout_unregister(layout);
}

static const struct of_device_id zte_tags_of_match[] = {
	{ .compatible = "zte,sr1010-tags" },
	{ }
};
MODULE_DEVICE_TABLE(of, zte_tags_of_match);

static struct nvmem_layout_driver zte_tags_driver = {
	.driver = {
		.name = "zte-tags",
		.of_match_table = zte_tags_of_match,
	},
	.probe = zte_tags_probe,
	.remove = zte_tags_remove,
};
module_nvmem_layout_driver(zte_tags_driver);

MODULE_DESCRIPTION("ZTE SR1010 factory tags NVMEM layout");
MODULE_LICENSE("GPL");
