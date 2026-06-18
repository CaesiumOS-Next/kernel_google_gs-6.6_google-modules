// SPDX-License-Identifier: GPL-2.0-only
/*
 * EdgeTPU support for dma-fence.
 *
 * Copyright (C) 2026 Google LLC
 */

#include <linux/dma-fence.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include <gcip/gcip-dma-fence.h>

#include "edgetpu-device-group.h"
#include "edgetpu-dma-fence.h"
#include "edgetpu-internal.h"

/**
 * edgetpu_dma_fence_release() - Release a DMA fence.
 * @gfence: The GCIP DMA fence to release.
 */
static void edgetpu_dma_fence_release(struct gcip_dma_fence *gfence)
{
	struct edgetpu_dma_fence *etfence = to_etfence(gfence);

	edgetpu_device_group_put(etfence->group);

	/* The DMA fence release function is required to free the object with RCU protection. */
	kfree_rcu(etfence, gfence.fence.rcu);
}

int edgetpu_dma_fence_create(struct edgetpu_device_group *group,
			     struct edgetpu_create_sync_fence_data *datap)
{
	struct gcip_dma_fence_data data = {
		.timeline_name = datap->timeline_name,
		.seqno = datap->seqno,
		.release = edgetpu_dma_fence_release,
	};
	struct edgetpu_dma_fence *etfence;
	int ret;

	etfence = kzalloc(sizeof(*etfence), GFP_KERNEL);
	if (!etfence)
		return -ENOMEM;

	ret = gcip_dma_fence_init(group->gfence_mgr, &etfence->gfence, &data);
	if (ret)
		goto err_free_etfence;

	etfence->group = edgetpu_device_group_get(group);
	datap->fence = data.fence;

	return ret;

err_free_etfence:
	kfree(etfence);

	return ret;
}

int edgetpu_dma_fence_signal(struct edgetpu_signal_sync_fence_data *datap)
{
	return gcip_dma_fence_signal(datap->fence, datap->error, false);
}

int edgetpu_dma_fence_status(struct edgetpu_sync_fence_status *datap)
{
	return gcip_dma_fence_status(datap->fence, &datap->status);
}

int edgetpu_dma_fence_debugfs_show(struct seq_file *s, void *unused)
{
	struct edgetpu_dev *etdev = s->private;
	struct edgetpu_list_group *list_entry;
	struct edgetpu_device_group *group;

	mutex_lock(&etdev->groups_lock);
	etdev_for_each_group(etdev, list_entry, group)
		gcip_dma_fence_manager_show(group->gfence_mgr, s);
	mutex_unlock(&etdev->groups_lock);

	return 0;
}
