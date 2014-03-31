/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MSM_IPC_LOGGING_H
#define _MSM_IPC_LOGGING_H

#include <linux/types.h>

#define MAX_MSG_SIZE 255

enum {
	TSV_TYPE_MSG_START = 1,
	TSV_TYPE_SKB = TSV_TYPE_MSG_START,
	TSV_TYPE_STRING,
	TSV_TYPE_MSG_END = TSV_TYPE_STRING,
};

struct tsv_header {
	unsigned char type;
	unsigned char size; 
};

struct encode_context {
	struct tsv_header hdr;
	char buff[MAX_MSG_SIZE];
	int offset;
};

struct decode_context {
	int output_format;      
	char *buff;             
	int size;               
};

#if defined(CONFIG_MSM_IPC_LOGGING)
void *ipc_log_context_create(int max_num_pages, const char *modname);

void msg_encode_start(struct encode_context *ectxt, uint32_t type);

int tsv_timestamp_write(struct encode_context *ectxt);

int tsv_pointer_write(struct encode_context *ectxt, void *pointer);

int tsv_int32_write(struct encode_context *ectxt, int32_t n);

int tsv_byte_array_write(struct encode_context *ectxt,
			 void *data, int data_size);

void msg_encode_end(struct encode_context *ectxt);

void ipc_log_write(void *ctxt, struct encode_context *ectxt);

int ipc_log_string(void *ilctxt, const char *fmt, ...) __printf(2, 3);

int ipc_log_extract(void *ilctxt, char *buff, int size);

#define IPC_SPRINTF_DECODE(dctxt, args...) \
do { \
	int i; \
	i = scnprintf(dctxt->buff, dctxt->size, args); \
	dctxt->buff += i; \
	dctxt->size -= i; \
} while (0)

void tsv_timestamp_read(struct encode_context *ectxt,
			struct decode_context *dctxt, const char *format);

void tsv_pointer_read(struct encode_context *ectxt,
		      struct decode_context *dctxt, const char *format);

int32_t tsv_int32_read(struct encode_context *ectxt,
		       struct decode_context *dctxt, const char *format);

void tsv_byte_array_read(struct encode_context *ectxt,
			 struct decode_context *dctxt, const char *format);

int add_deserialization_func(void *ctxt, int type,
			void (*dfunc)(struct encode_context *,
				      struct decode_context *));

int ipc_log_context_destroy(void *ctxt);

#else

static inline void *ipc_log_context_create(int max_num_pages,
	const char *modname)
{ return NULL; }

static inline void msg_encode_start(struct encode_context *ectxt,
	uint32_t type) { }

static inline int tsv_timestamp_write(struct encode_context *ectxt)
{ return -EINVAL; }

static inline int tsv_pointer_write(struct encode_context *ectxt, void *pointer)
{ return -EINVAL; }

static inline int tsv_int32_write(struct encode_context *ectxt, int32_t n)
{ return -EINVAL; }

static inline int tsv_byte_array_write(struct encode_context *ectxt,
			 void *data, int data_size)
{ return -EINVAL; }

static inline void msg_encode_end(struct encode_context *ectxt) { }

static inline void ipc_log_write(void *ctxt, struct encode_context *ectxt) { }

static inline int ipc_log_string(void *ilctxt, const char *fmt, ...)
{ return -EINVAL; }

static inline int ipc_log_extract(void *ilctxt, char *buff, int size)
{ return -EINVAL; }

#define IPC_SPRINTF_DECODE(dctxt, args...) do { } while (0)

static inline void tsv_timestamp_read(struct encode_context *ectxt,
			struct decode_context *dctxt, const char *format) { }

static inline void tsv_pointer_read(struct encode_context *ectxt,
		      struct decode_context *dctxt, const char *format) { }

static inline int32_t tsv_int32_read(struct encode_context *ectxt,
		       struct decode_context *dctxt, const char *format)
{ return 0; }

static inline void tsv_byte_array_read(struct encode_context *ectxt,
			 struct decode_context *dctxt, const char *format) { }

static inline int add_deserialization_func(void *ctxt, int type,
			void (*dfunc)(struct encode_context *,
				      struct decode_context *))
{ return 0; }

static inline int ipc_log_context_destroy(void *ctxt)
{ return 0; }

#endif

#endif
