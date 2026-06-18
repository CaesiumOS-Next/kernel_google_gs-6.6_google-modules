/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2024 Google LLC.
 *
 * Google firmware tracepoint services header.
 */

#ifndef __FWTP_H
#define __FWTP_H

#include "fwtp_ipc_client.h"

/* FWTP printer data buffer size to hold at least 8 32-bit data items. */
#define FWTP_PRINTER_DATA_BUFFER_SIZE (8 * (sizeof(struct fwtp_data_item) + 4))

/* Size of buffer to use for printing tracepoints. */
#define FWTP_PRINTER_BUFFER_SIZE 128

/* String used to select all filters. */
#define FWTP_SELECT_ALL_FILTERS_STRING "all"

/**
 * struct fwtp_dev - Structure representing an FWTP kernel device.
 *
 * @fwtp_ipc_client: FWTP IPC client record.
 * @dev: Kernel device record.
 * @root_debugfs: Root device debugfs directory.
 * @printer_ctx: Tracepoint printer context.
 * @printer_data_buffer: Buffer used for tracepoint data items.
 * @printer_buffer: Buffer used for printing tracepoints. It should be big
 *                  enough to hold one line of output.
 * @log_enabled: If true, tracepoints will be published to the kernel log.
 * @ftrace_enabled: If true, tracepoints will be published to ftrace.
 */
struct fwtp_dev {
	struct fwtp_ipc_client fwtp_ipc_client;
	struct device *dev;
	struct dentry *root_debugfs;
	struct fwtp_printer_ctx printer_ctx;
	u8 printer_data_buffer[FWTP_PRINTER_DATA_BUFFER_SIZE];
	char printer_buffer[FWTP_PRINTER_BUFFER_SIZE];
	bool log_enabled;
	bool ftrace_enabled;
};

int fwtp_dev_init(struct fwtp_dev *fwtp_dev);
void fwtp_dev_deinit(struct fwtp_dev *fwtp_dev);
int fwtp_dev_get_memio_ring(struct fwtp_dev *fwtp_dev, int ring_num,
			    struct tracepoint_ring *ring);
void fwtp_dev_free_memio_ring(struct fwtp_dev *fwtp_dev,
			      struct tracepoint_ring *ring);
void fwtp_dev_printer_post_process(struct fwtp_printer_ctx *printer_ctx,
				   unsigned int type, u64 timestamp, u32 str_id,
				   const char *str,
				   struct fwtp_data_item_list *data_items);

#endif /* __FWTP_H */
