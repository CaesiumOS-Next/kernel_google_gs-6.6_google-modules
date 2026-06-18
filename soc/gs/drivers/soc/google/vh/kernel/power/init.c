// SPDX-License-Identifier: GPL-2.0-only
/* init.c
 *
 * Android Vendor Hook Support
 *
 * Copyright 2025 Google LLC
 */

#include <linux/module.h>
#include <linux/pm_qos.h>
/* Required for enums used in trace/hooks/power.h */
#include <trace/hooks/power.h>

#include "process.h"

static int vh_power_init(void)
{
	int ret = 0;
	ret = register_trace_android_vh_try_to_freeze_todo_logging(
						vh_try_to_freeze_todo_logging_pixel_mod, NULL);

	return ret;
}

static void __exit vh_power_exit(void)
{
	unregister_trace_android_vh_try_to_freeze_todo_logging(
			vh_try_to_freeze_todo_logging_pixel_mod, NULL);
}

module_init(vh_power_init);
module_exit(vh_power_exit);

MODULE_AUTHOR("Ananth Raghavan Subramanian <ananthrs@google.com>");
MODULE_DESCRIPTION("Pixel Power Vendor Hooks Driver");
MODULE_LICENSE("GPL v2");
