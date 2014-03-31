/*
 * Copyright (c) 2004-2011 Atheros Communications Inc.
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef BMI_H
#define BMI_H



#define BMI_NO_COMMAND                      0

#define BMI_DONE                            1

#define BMI_READ_MEMORY                     2

#define BMI_WRITE_MEMORY                    3

#define BMI_EXECUTE                         4

#define BMI_SET_APP_START                   5

#define BMI_READ_SOC_REGISTER               6

#define BMI_WRITE_SOC_REGISTER              7

#define BMI_GET_TARGET_ID                  8
#define BMI_GET_TARGET_INFO                8

#define TARGET_VERSION_SENTINAL 0xffffffff
#define TARGET_TYPE_AR6003      3
#define TARGET_TYPE_AR6004      5
#define BMI_ROMPATCH_INSTALL               9

#define BMI_ROMPATCH_UNINSTALL             10

#define BMI_ROMPATCH_ACTIVATE              11

#define BMI_ROMPATCH_DEACTIVATE            12


#define BMI_LZ_STREAM_START                13

#define BMI_LZ_DATA                        14

#define BMI_COMMUNICATION_TIMEOUT       1000 

struct ath6kl;
struct ath6kl_bmi_target_info {
	__le32 byte_count;   
	__le32 version;      
	__le32 type;         
} __packed;

#define ath6kl_bmi_write_hi32(ar, item, val)				\
	({								\
		u32 addr;						\
		__le32 v;						\
									\
		addr = ath6kl_get_hi_item_addr(ar, HI_ITEM(item));	\
		v = cpu_to_le32(val);					\
		ath6kl_bmi_write(ar, addr, (u8 *) &v, sizeof(v));	\
	})

#define ath6kl_bmi_read_hi32(ar, item, val)				\
	({								\
		u32 addr, *check_type = val;				\
		__le32 tmp;						\
		int ret;						\
									\
		(void) (check_type == val);				\
		addr = ath6kl_get_hi_item_addr(ar, HI_ITEM(item));	\
		ret = ath6kl_bmi_read(ar, addr, (u8 *) &tmp, 4);	\
		*val = le32_to_cpu(tmp);				\
		ret;							\
	})

int ath6kl_bmi_init(struct ath6kl *ar);
void ath6kl_bmi_cleanup(struct ath6kl *ar);
void ath6kl_bmi_reset(struct ath6kl *ar);

int ath6kl_bmi_done(struct ath6kl *ar);
int ath6kl_bmi_get_target_info(struct ath6kl *ar,
			       struct ath6kl_bmi_target_info *targ_info);
int ath6kl_bmi_read(struct ath6kl *ar, u32 addr, u8 *buf, u32 len);
int ath6kl_bmi_write(struct ath6kl *ar, u32 addr, u8 *buf, u32 len);
int ath6kl_bmi_execute(struct ath6kl *ar,
		       u32 addr, u32 *param);
int ath6kl_bmi_set_app_start(struct ath6kl *ar,
			     u32 addr);
int ath6kl_bmi_reg_read(struct ath6kl *ar, u32 addr, u32 *param);
int ath6kl_bmi_reg_write(struct ath6kl *ar, u32 addr, u32 param);
int ath6kl_bmi_lz_data(struct ath6kl *ar,
		       u8 *buf, u32 len);
int ath6kl_bmi_lz_stream_start(struct ath6kl *ar,
			       u32 addr);
int ath6kl_bmi_fast_download(struct ath6kl *ar,
			     u32 addr, u8 *buf, u32 len);
#endif
