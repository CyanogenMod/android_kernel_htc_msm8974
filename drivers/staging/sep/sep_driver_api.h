/*
 *
 *  sep_driver_api.h - Security Processor Driver api definitions
 *
 *  Copyright(c) 2009-2011 Intel Corporation. All rights reserved.
 *  Contributions(c) 2009-2011 Discretix. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  CONTACTS:
 *
 *  Mark Allyn		mark.a.allyn@intel.com
 *  Jayant Mangalampalli jayant.mangalampalli@intel.com
 *
 *  CHANGES:
 *
 *  2010.09.14  Upgrade to Medfield
 *  2011.02.22  Enable kernel crypto
 *
 */

#ifndef __SEP_DRIVER_API_H__
#define __SEP_DRIVER_API_H__

#define SEP_DRIVER_SRC_REPLY		1
#define SEP_DRIVER_SRC_REQ		2
#define SEP_DRIVER_SRC_PRINTF		3

#define SEP_DRIVER_POWERON		1
#define SEP_DRIVER_POWEROFF		2

enum type_of_request {
	NO_REQUEST,
	AES_CBC,
	AES_ECB,
	DES_CBC,
	DES_ECB,
	DES3_ECB,
	DES3_CBC,
	SHA1,
	MD5,
	SHA224,
	SHA256
	};

enum hash_stage {
	HASH_INIT,
	HASH_UPDATE,
	HASH_FINISH,
	HASH_DIGEST,
	HASH_FINUP_DATA,
	HASH_FINUP_FINISH
};

struct sep_dcblock {
	
	u32	input_mlli_address;
	
	u32	input_mlli_num_entries;
	
	u32	input_mlli_data_size;
	
	u32	output_mlli_address;
	
	u32	output_mlli_num_entries;
	
	u32	output_mlli_data_size;
	
	aligned_u64 out_vr_tail_pt;
	
	u32	tail_data_size;
	
	u8	tail_data[68];
};

struct build_dcb_struct {
	
	aligned_u64 app_in_address;
	
	u32  data_in_size;
	
	aligned_u64 app_out_address;
	u32  block_size;
	u32  tail_block_size;

	
	u32  is_applet;
};

struct build_dcb_struct_kernel {
	
	void *app_in_address;
	
	ssize_t  data_in_size;
	
	void *app_out_address;
	u32  block_size;
	u32  tail_block_size;

	
	u32  is_applet;

	struct scatterlist *src_sg;
	struct scatterlist *dst_sg;
};

struct sep_dma_map {
	
	dma_addr_t    dma_addr;
	
	size_t        size;
};

struct sep_dma_resource {
	struct page **in_page_array;

	struct page **out_page_array;

	
	u32 in_num_pages;

	
	u32 out_num_pages;

	
	struct sep_dma_map *in_map_array;

	
	struct sep_dma_map *out_map_array;

	
	u32 in_map_num_entries;

	
	u32 out_map_num_entries;

	
	struct scatterlist *src_sg;
	struct scatterlist *dst_sg;
};


struct rar_hndl_to_bus_struct {

	
	aligned_u64 rar_handle;
};

struct sep_lli_entry {
	
	u32 bus_address;

	
	u32 block_size;
};

struct sep_fastcall_hdr {
	u32 magic;
	u32 secure_dma;
	u32 msg_len;
	u32 num_dcbs;
};

struct sep_call_status {
	unsigned long status;
};

struct sep_dma_context {
	
	u32 nr_dcb_creat;
	
	u32 num_lli_tables_created;
	
	u32 dmatables_len;
	
	u32 input_data_len;
	
	bool secure_dma;
	struct sep_dma_resource dma_res_arr[SEP_MAX_NUM_SYNC_DMA_OPS];
	
	struct scatterlist *src_sg;
	struct scatterlist *dst_sg;
};

struct sep_private_data {
	struct sep_queue_info *my_queue_elem;
	struct sep_device *device;
	struct sep_call_status call_status;
	struct sep_dma_context *dma_ctx;
};



void sep_queue_status_remove(struct sep_device *sep,
				      struct sep_queue_info **queue_elem);
struct sep_queue_info *sep_queue_status_add(
						struct sep_device *sep,
						u32 opcode,
						u32 size,
						u32 pid,
						u8 *name, size_t name_len);

int sep_create_dcb_dmatables_context_kernel(struct sep_device *sep,
			struct sep_dcblock **dcb_region,
			void **dmatables_region,
			struct sep_dma_context **dma_ctx,
			const struct build_dcb_struct_kernel *dcb_data,
			const u32 num_dcbs);

ssize_t sep_activate_dcb_dmatables_context(struct sep_device *sep,
					struct sep_dcblock **dcb_region,
					void **dmatables_region,
					struct sep_dma_context *dma_ctx);

int sep_prepare_input_output_dma_table_in_dcb(struct sep_device *sep,
	unsigned long  app_in_address,
	unsigned long  app_out_address,
	u32  data_in_size,
	u32  block_size,
	u32  tail_block_size,
	bool isapplet,
	bool	is_kva,
	bool    secure_dma,
	struct sep_dcblock *dcb_region,
	void **dmatables_region,
	struct sep_dma_context **dma_ctx,
	struct scatterlist *src_sg,
	struct scatterlist *dst_sg);

int sep_free_dma_table_data_handler(struct sep_device *sep,
					   struct sep_dma_context **dma_ctx);
int sep_send_command_handler(struct sep_device *sep);

int sep_wait_transaction(struct sep_device *sep);

#define SEP_IOC_MAGIC_NUMBER	's'

#define SEP_IOCSENDSEPCOMMAND	 \
	_IO(SEP_IOC_MAGIC_NUMBER, 0)

#define SEP_IOCENDTRANSACTION	 \
	_IO(SEP_IOC_MAGIC_NUMBER, 15)

#define SEP_IOCPREPAREDCB					\
	_IOW(SEP_IOC_MAGIC_NUMBER, 35, struct build_dcb_struct)

#define SEP_IOCFREEDCB					\
	_IO(SEP_IOC_MAGIC_NUMBER, 36)

struct sep_device;

#define SEP_IOCPREPAREDCB_SECURE_DMA	\
	_IOW(SEP_IOC_MAGIC_NUMBER, 38, struct build_dcb_struct)

#define SEP_IOCFREEDCB_SECURE_DMA	\
	_IO(SEP_IOC_MAGIC_NUMBER, 39)

#endif
