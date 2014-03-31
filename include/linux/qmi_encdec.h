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

#ifndef _QMI_ENCDEC_H_
#define _QMI_ENCDEC_H_

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/socket.h>
#include <linux/gfp.h>

#define QMI_REQUEST_CONTROL_FLAG 0x00
#define QMI_RESPONSE_CONTROL_FLAG 0x02
#define QMI_INDICATION_CONTROL_FLAG 0x04
#define QMI_HEADER_SIZE 7

enum elem_type {
	QMI_OPT_FLAG = 1,
	QMI_DATA_LEN,
	QMI_UNSIGNED_1_BYTE,
	QMI_UNSIGNED_2_BYTE,
	QMI_UNSIGNED_4_BYTE,
	QMI_UNSIGNED_8_BYTE,
	QMI_SIGNED_2_BYTE_ENUM,
	QMI_SIGNED_4_BYTE_ENUM,
	QMI_STRUCT,
	QMI_EOTI,
};

enum array_type {
	NO_ARRAY = 0,
	STATIC_ARRAY = 1,
	VAR_LEN_ARRAY = 2,
};

struct elem_info {
	enum elem_type data_type;
	uint32_t elem_len;
	uint32_t elem_size;
	enum array_type is_array;
	uint8_t tlv_type;
	uint32_t offset;
	struct elem_info *ei_array;
};

struct msg_desc {
	uint16_t msg_id;
	int max_msg_len;
	struct elem_info *ei_array;
};

struct qmi_header {
	unsigned char cntl_flag;
	uint16_t txn_id;
	uint16_t msg_id;
	uint16_t msg_len;
} __attribute__((__packed__));

static inline void encode_qmi_header(unsigned char *buf,
			unsigned char cntl_flag, uint16_t txn_id,
			uint16_t msg_id, uint16_t msg_len)
{
	struct qmi_header *hdr = (struct qmi_header *)buf;

	hdr->cntl_flag = cntl_flag;
	hdr->txn_id = txn_id;
	hdr->msg_id = msg_id;
	hdr->msg_len = msg_len;
}

static inline void decode_qmi_header(unsigned char *buf,
			unsigned char *cntl_flag, uint16_t *txn_id,
			uint16_t *msg_id, uint16_t *msg_len)
{
	struct qmi_header *hdr = (struct qmi_header *)buf;

	*cntl_flag = hdr->cntl_flag;
	*txn_id = hdr->txn_id;
	*msg_id = hdr->msg_id;
	*msg_len = hdr->msg_len;
}

#ifdef CONFIG_QMI_ENCDEC
int qmi_kernel_encode(struct msg_desc *desc,
		      void *out_buf, uint32_t out_buf_len,
		      void *in_c_struct);

int qmi_kernel_decode(struct msg_desc *desc, void *out_c_struct,
		      void *in_buf, uint32_t in_buf_len);

bool qmi_verify_max_msg_len(struct msg_desc *desc);

#else
static inline int qmi_kernel_encode(struct msg_desc *desc,
				    void *out_buf, uint32_t out_buf_len,
				    void *in_c_struct)
{
	return -EOPNOTSUPP;
}

static inline int qmi_kernel_decode(struct msg_desc *desc,
				    void *out_c_struct,
				    void *in_buf, uint32_t in_buf_len)
{
	return -EOPNOTSUPP;
}

static inline bool qmi_verify_max_msg_len(struct msg_desc *desc)
{
	return false;
}
#endif

#endif
