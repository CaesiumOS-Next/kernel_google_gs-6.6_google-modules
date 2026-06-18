/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * EdgeTPU support for DMA sync fences.
 *
 * Copyright (C) 2026 Google LLC
 */
#ifndef __EDGETPU_DMA_FENCE_H__
#define __EDGETPU_DMA_FENCE_H__

#include <linux/container_of.h>
#include <linux/seq_file.h>

#include <gcip/gcip-dma-fence.h>

#include "edgetpu-device-group.h"
#include "edgetpu-internal.h"

#define to_etfence(ptr) container_of(ptr, struct edgetpu_dma_fence, gfence)

/**
 * struct edgetpu_dma_fence - EdgeTPU wrapper for GCIP DMA fence.
 * @gfence: The underlying GCIP DMA fence.
 * @group: The device group that owns this fence.
 */
struct edgetpu_dma_fence {
	struct gcip_dma_fence gfence;
	struct edgetpu_device_group *group;
};

/**
 * edgetpu_dma_fence_create() - Creates a DMA sync fence via ioctl.
 * @group: The device group that owns this fence.
 * @datap: Pointer to the data containing the timeline name and seqno.
 *
 * Return: 0 on success or a negative error code on failure.
 */
int edgetpu_dma_fence_create(struct edgetpu_device_group *group,
			     struct edgetpu_create_sync_fence_data *datap);

/**
 * edgetpu_dma_fence_signal() - Signals a DMA sync fence, optionally specifying error status.
 * @datap: Pointer to the data containing the fence fd and error status.
 *
 * Return: 0 on success or a negative error code on failure.
 */
int edgetpu_dma_fence_signal(struct edgetpu_signal_sync_fence_data *datap);

/**
 * edgetpu_dma_fence_status() - Fetches DMA sync fence status.
 * @datap: Pointer to the data where the status will be stored.
 *
 * Return: 0 on success or a negative error code on failure.
 */
int edgetpu_dma_fence_status(struct edgetpu_sync_fence_status *datap);

/**
 * edgetpu_dma_fence_debugfs_show() - Dumps sync fence info from debugfs.
 * @s: T.
 * @unused: Unused argument.
 *
 * Return: 0 on success.
 */
int edgetpu_dma_fence_debugfs_show(struct seq_file *s, void *unused);

#endif /* __EDGETPU_DMA_FENCE_H__ */
