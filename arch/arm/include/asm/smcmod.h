
#ifndef __SMCMOD_H_
#define __SMCMOD_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define SMCMOD_DEV "smcmod"

#define SMCMOD_REG_REQ_MAX_ARGS	2

struct smcmod_reg_req {
	uint32_t service_id; 
	uint32_t command_id; 
	uint8_t  num_args; 
	uint32_t args[SMCMOD_REG_REQ_MAX_ARGS]; 
	uint32_t return_val; 
};

struct smcmod_buf_req {
	uint32_t service_id;
	uint32_t command_id; 
	int32_t ion_cmd_fd; 
	uint32_t cmd_len; 
	int32_t ion_resp_fd; 
	uint32_t resp_len; 
	uint32_t return_val; 
};

struct smcmod_cipher_req {
	uint32_t algorithm; 
	uint32_t operation; 
	uint32_t mode; 
	int32_t ion_key_fd; 
	uint32_t key_size; 
	int32_t ion_plain_text_fd; 
	uint32_t plain_text_size; 
	int32_t ion_cipher_text_fd; 
	uint32_t cipher_text_size; 
	int32_t ion_init_vector_fd; 
	uint32_t init_vector_size; 
	uint32_t key_is_null; 
	uint32_t return_val; 
};

struct smcmod_msg_digest_req {
	uint32_t algorithm; 
	int32_t ion_key_fd; 
	uint32_t key_size; 
	int32_t ion_input_fd; 
	uint32_t input_size; 
	int32_t ion_output_fd; 
	uint32_t output_size; 
	uint32_t fixed_block; 
	uint32_t key_is_null; 
	uint32_t return_val; 
} __packed;

struct smcmod_decrypt_req {
	uint32_t service_id;
	uint32_t command_id;
#define SMCMOD_DECRYPT_REQ_OP_METADATA	1
#define SMCMOD_DECRYPT_REQ_OP_IMG_FRAG	2
	uint32_t operation;
	union {
		struct {
			uint32_t len;
			uint32_t ion_fd;
		} metadata;
		struct {
			uint32_t ctx_id;
			uint32_t last_frag;
			uint32_t frag_len;
			uint32_t ion_fd;
			uint32_t offset;
		} img_frag;
	} request;
	union {
		struct {
			uint32_t status;
			uint32_t ctx_id;
			uint32_t end_offset;
		} metadata;
		struct {
			uint32_t status;
		} img_frag;
	} response;
};

#define SMCMOD_IOC_MAGIC	0x97

#define SMCMOD_IOCTL_SEND_REG_CMD \
	_IOWR(SMCMOD_IOC_MAGIC, 32, struct smcmod_reg_req)
#define SMCMOD_IOCTL_SEND_BUF_CMD \
	_IOWR(SMCMOD_IOC_MAGIC, 33, struct smcmod_buf_req)
#define SMCMOD_IOCTL_SEND_CIPHER_CMD \
	_IOWR(SMCMOD_IOC_MAGIC, 34, struct smcmod_cipher_req)
#define SMCMOD_IOCTL_SEND_MSG_DIGEST_CMD \
	_IOWR(SMCMOD_IOC_MAGIC, 35, struct smcmod_msg_digest_req)
#define SMCMOD_IOCTL_GET_VERSION _IOWR(SMCMOD_IOC_MAGIC, 36, uint32_t)
#define SMCMOD_IOCTL_SEND_DECRYPT_CMD \
	_IOWR(SMCMOD_IOC_MAGIC, 37, struct smcmod_decrypt_req)

#endif 
