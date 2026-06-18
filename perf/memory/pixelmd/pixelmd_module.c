// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "pixelmd: " fmt

#include "pixelmd_device.h"
#include "pixelmd_cmd_kswapd.h"

#include <linux/module.h>

static struct pixelmd_device pixelmd_device;

static int __init pixelmd_init(void)
{
	int ret;

	ret = pixelmd_kswapd_register_hooks();
	if (ret) {
		pr_err("failed to register hooks: %d\n", ret);
		return ret;
	}

	ret = pixelmd_device_init(&pixelmd_device);
	if (ret < 0) {
		pixelmd_kswapd_unregister_hooks();
		pr_err("failed to initialize device: %d\n", ret);
		return ret;
	}

	pr_info("loaded\n");
	return 0;
}

static void __exit pixelmd_exit(void)
{
	pixelmd_kswapd_unregister_hooks();
	pixelmd_device_destroy(&pixelmd_device);

	pr_info("unloaded\n");
}

module_init(pixelmd_init);
module_exit(pixelmd_exit);

MODULE_LICENSE("GPL");
