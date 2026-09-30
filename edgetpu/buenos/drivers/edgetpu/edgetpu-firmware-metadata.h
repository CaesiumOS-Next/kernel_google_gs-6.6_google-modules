/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Utilities for accessing EdgeTPU metadata region.
 *
 * Copyright (C) 2026 Google LLC
 */

#ifndef __EDGETPU_FIRMWARE_METADATA_H__
#define __EDGETPU_FIRMWARE_METADATA_H__

#include "edgetpu-firmware.h"

/**
 * struct edgetpu_queue_metadata - Metadata for storing mailbox command and response queue
 * information; used when migrating to CMF.
 * @cmd_queue_addr: Device address of the command queue. This address is set by the host.
 * @resp_queue_addr: Device address of the response queue. This address is set by the host.
 * @cmd_queue_size: Size of the command queue.
 * @resp_queue_size: Size of the response queue.
 * @cmd_queue_tail: The tail pointer of the command queue. The host is responsible for updating.
 * @resp_queue_head: The head pointer of the response queue. The host is responsible for updating.
 * @reserved_1: Reserved padding to 64 bytes for cache alignment.
 * @cmd_queue_head: The head pointer of the command queue. The firmware is responsible for updating.
 * @resp_queue_tail: The tail pointer of the response queue. The firmware is responsible for
 * updating.
 * @reserved_2: Reserved padding to 64 bytes for cache alignment.
 */
struct edgetpu_queue_metadata {
	/* Field KD will modify (write-only by KD) */
	u32 cmd_queue_addr;
	u32 resp_queue_addr;
	u32 cmd_queue_size;
	u32 resp_queue_size;
	u32 cmd_queue_tail;
	u32 resp_queue_head;
	u32 reserved_1[10];
	/* Field FW will modify (write-only by FW)*/
	u32 cmd_queue_head;
	u32 resp_queue_tail;
	u32 reserved_2[14];
};

/**
 * struct edgetpu_firmware_metadata - Metadata for information exchange with firmware; used when
 * migrating to CMF.
 * @kd_version: KD version, was using KCI.config_spare_0.
 * @fw_boot_stage: Firmware boot stage, was using KCI.config_spare_1.
 * @reserved: Reserved.
 * @queue_metadata: Array of queue metadata indexed by KCI, IKV, and IIF mailboxes.
 * @reserved_1: Reserved padding.
 */
struct edgetpu_firmware_metadata {
	u32 kd_version;
	u32 fw_boot_stage;
	u32 reserved[14];
	struct edgetpu_queue_metadata queue_metadata[3];
	u8 reserved_1[64];
};

/**
 * edgetpu_firmware_metadata() - Helper function to get firmware metadata, which is placed at the
 * beginning of the shared data region.
 * @etdev: Pointer to the edgetpu device structure.
 *
 * Return: Pointer to the firmware metadata structure.
 */
static inline struct edgetpu_firmware_metadata *edgetpu_firmware_metadata(struct edgetpu_dev *etdev)
{
	return (struct edgetpu_firmware_metadata *)edgetpu_firmware_shared_data_vaddr(etdev);
}

/* Read firmware metadata with no memory barrier / access ordering. */
#define EDGETPU_FIRMWARE_METADATA_READ(device, field) \
	READ_ONCE(edgetpu_firmware_metadata(device)->field)

/* Write firmware metadata with no memory barrier / access ordering. */
#define EDGETPU_FIRMWARE_METADATA_WRITE(device, field, value) \
	WRITE_ONCE(edgetpu_firmware_metadata(device)->field, (value))

#endif /* __EDGETPU_FIRMWARE_METADATA_H__ */
