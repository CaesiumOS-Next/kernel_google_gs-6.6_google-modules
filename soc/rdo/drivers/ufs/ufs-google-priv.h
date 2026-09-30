/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _UFS_GOOGLE_PRIV_H_
#define _UFS_GOOGLE_PRIV_H_

#include <ufs/ufshcd.h>

/* UFSHCD error handling flags */
enum {
	UFSHCD_EH_IN_PROGRESS = (1 << 0),
};

#define ufshcd_eh_in_progress(h) \
	((h)->eh_flags & UFSHCD_EH_IN_PROGRESS)

#endif /* _UFS_GOOGLE_PRIV_H_ */
