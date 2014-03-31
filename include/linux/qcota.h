/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __QCOTA__H
#define __QCOTA__H

#include <linux/types.h>
#include <linux/ioctl.h>

#define QCE_OTA_MAX_BEARER   31
#define OTA_KEY_SIZE 16   

enum qce_ota_dir_enum {
	QCE_OTA_DIR_UPLINK   = 0,
	QCE_OTA_DIR_DOWNLINK = 1,
	QCE_OTA_DIR_LAST
};

enum qce_ota_algo_enum {
	QCE_OTA_ALGO_KASUMI = 0,
	QCE_OTA_ALGO_SNOW3G = 1,
	QCE_OTA_ALGO_LAST
};

struct qce_f8_req {
	uint8_t  *data_in;
	uint8_t  *data_out;
	uint16_t  data_len;
	uint32_t  count_c;
	uint8_t   bearer;
	uint8_t   ckey[OTA_KEY_SIZE];
	enum qce_ota_dir_enum  direction;
	enum qce_ota_algo_enum algorithm;
};

struct qce_f8_multi_pkt_req {
	uint16_t    num_pkt;
	uint16_t    cipher_start;
	uint16_t    cipher_size;
	struct qce_f8_req qce_f8_req;
};

struct qce_f9_req {
	uint8_t   *message;
	uint16_t   msize;
	uint8_t    last_bits;
	uint32_t   mac_i;
	uint32_t   fresh;
	uint32_t   count_i;
	enum qce_ota_dir_enum direction;
	uint8_t    ikey[OTA_KEY_SIZE];
	enum qce_ota_algo_enum algorithm;
};

#define QCOTA_IOC_MAGIC     0x85

#define QCOTA_F8_REQ _IOWR(QCOTA_IOC_MAGIC, 1, struct qce_f8_req)
#define QCOTA_F8_MPKT_REQ _IOWR(QCOTA_IOC_MAGIC, 2, struct qce_f8_multi_pkt_req)
#define QCOTA_F9_REQ _IOWR(QCOTA_IOC_MAGIC, 3, struct qce_f9_req)

#endif 
