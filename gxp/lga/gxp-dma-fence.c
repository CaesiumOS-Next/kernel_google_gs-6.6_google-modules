// SPDX-License-Identifier: GPL-2.0-only
/*
 * GXP support for DMA fence.
 *
 * Copyright (C) 2023 Google LLC
 */

#include <linux/slab.h>

#include <gcip/gcip-dma-fence.h>

#include "gxp-dma-fence.h"
#include "gxp-internal.h"
#include "gxp-vd.h"
#include "gxp.h"

static void gxp_dma_fence_add_to_vd(struct gxp_dma_fence *gxp_fence, struct gxp_virtual_device *vd)
{
	gxp_fence->vd = gxp_vd_get(vd);

	mutex_lock(&vd->fence_list_lock);
	list_add_tail(&gxp_fence->fence_list, &vd->gxp_fence_list);
	mutex_unlock(&vd->fence_list_lock);
}

static void gxp_dma_fence_del_from_vd(struct gxp_dma_fence *gxp_fence)
{
	struct gxp_virtual_device *vd = gxp_fence->vd;

	mutex_lock(&vd->fence_list_lock);
	list_del(&gxp_fence->fence_list);
	mutex_unlock(&vd->fence_list_lock);

	gxp_fence->vd = NULL;
	gxp_vd_put(vd);
}

static void gxp_dma_fence_release(struct gcip_dma_fence *gfence)
{
	struct gxp_dma_fence *gxp_fence = to_gxp_fence(gfence);

	gxp_dma_fence_del_from_vd(gxp_fence);
	gcip_dma_fence_exit(gfence);
	kfree(gxp_fence);
}

int gxp_dma_fence_create(struct gxp_dev *gxp, struct gxp_virtual_device *vd,
			 struct gxp_create_sync_fence_data *datap)
{
	struct gcip_dma_fence_data data = {
		.timeline_name = datap->timeline_name,
		.seqno = datap->seqno,
		.release = gxp_dma_fence_release,
	};
	struct gxp_dma_fence *gxp_fence;
	int ret;

	gxp_fence = kzalloc(sizeof(*gxp_fence), GFP_KERNEL);
	if (!gxp_fence)
		return -ENOMEM;

	ret = gcip_dma_fence_init(gxp->gfence_mgr, &gxp_fence->gfence, &data);
	if (ret)
		goto err_free_gxp_fence;

	gxp_dma_fence_add_to_vd(gxp_fence, vd);

	datap->fence = data.fence;

	return ret;

err_free_gxp_fence:
	kfree(gxp_fence);

	return ret;
}
