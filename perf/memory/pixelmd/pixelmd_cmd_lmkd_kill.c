// SPDX-License-Identifier: GPL-2.0

#include "pixelmd_cmd_lmkd_kill.h"

#include <linux/errno.h>
#include <linux/oom.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>
#include <linux/uaccess.h>
#include <trace/hooks/signal.h>

#include <linux/cred.h>
#include <linux/string.h>

#include "pixelmd_events.h"

/*
 * From system/core/libcutils/include/private/android_filesystem_config.h
 */
#define AID_LMKD 1069

/*
 * This hook is called on the return path of __send_signal_locked().
 */
static void vh_send_signal_locked(void *data, int result, int ret, int sig, struct task_struct *t,
				  enum pid_type type, struct kernel_siginfo *info)
{
	struct pixelmd_lmkd_kill_event event = { 0 };

	// We're interested only in SIGKILLs from LMKD.
	if (sig != SIGKILL || from_kuid(&init_user_ns, current_uid()) != AID_LMKD)
		return;

	/*
	 * This hook (vh_send_signal_locked) is called with t->sighand->siglock held,
	 * so it is safe to access t->signal without any additional locking.
	 *
	 * t->signal shouldn't realistically be NULL here, so if it is NULL, we simply
	 * skip reporting the event, instead of inventing "invalid" OOM score adj value.
	 */
	if (!t->signal)
		return;
	event.oom_score_adj = t->signal->oom_score_adj;

	event.pid = task_tgid_nr(t);
	event.uid = from_kuid(&init_user_ns, task_uid(t));

	pixelmd_write_event(PIXELMD_SOURCE_LMKD, PIXELMD_EVENT_LMKD_KILL, &event, sizeof(event));
}

int pixelmd_lmkd_kill_register_hooks(void)
{
	return register_trace_android_vh_send_signal_locked(vh_send_signal_locked, NULL);
}

void pixelmd_lmkd_kill_unregister_hooks(void)
{
	unregister_trace_android_vh_send_signal_locked(vh_send_signal_locked, NULL);
}

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
