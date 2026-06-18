// SPDX-License-Identifier: GPL-2.0

#include "pixelmd_cmd_lmkd_kill.h"

#include <linux/errno.h>
#include <linux/oom.h>
#include <linux/uaccess.h>

/*
 * VH_MM module also uses "trace/events/android_vendor_lmk.h", so if it's not
 * enabled, then probably the file is not there.
 */
#if IS_ENABLED(CONFIG_VH_MM)
#include <trace/events/android_vendor_lmk.h>
#endif

/* A copy of the enum from
 * private/google-modules/soc/gs/drivers/soc/google/vh/include/pixel_mm_hint.h
 *
 * Remove once we can include that file directly.
 */
enum mm_kill_reason {
	MM_PA_KILL,
	MM_PIXELMD_KILL
};

long pixelmd_cmd_lmkd_kill(void __user *param)
{
	__u16 min_oom_score_adj;

	if (copy_from_user(&min_oom_score_adj, param, sizeof(min_oom_score_adj)))
		return -EFAULT;

	if (min_oom_score_adj < 0 || min_oom_score_adj > OOM_SCORE_ADJ_MAX)
		return -EINVAL;

#if IS_ENABLED(CONFIG_VH_MM)
	trace_android_trigger_vendor_lmk_kill(MM_PIXELMD_KILL, min_oom_score_adj);
#endif
	return 0;
}
