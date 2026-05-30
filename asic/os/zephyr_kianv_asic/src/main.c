// SPDX-License-Identifier: Apache-2.0
/* Copyright (c) 2026 Hirosh Dabui <hirosh@dabui.de> */

#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/sys/printk.h>
#include <errno.h>

#define RAMFS_NODE DT_NODELABEL(ramfs)

FS_FSTAB_DECLARE_ENTRY(RAMFS_NODE);

int main(void)
{
	struct fs_mount_t *ram_mount = &FS_FSTAB_ENTRY(RAMFS_NODE);
	int rc;

	printk("Zephyr RTOS on KianV RV32IMA ASIC SoC\n");
	(void)disk_access_init("RAM");
	rc = fs_mount(ram_mount);
	if ((rc < 0) && (rc != -EALREADY) && (rc != -EBUSY)) {
		printk("RAM mount err %d\n", rc);
	}
	printk("Shell ready\n");
	return 0;
}
