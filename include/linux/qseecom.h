#ifndef __QSEECOM_H_
#define __QSEECOM_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define MAX_ION_FD  4
#define MAX_APP_NAME_SIZE  32
#define QSEECOM_HASH_SIZE  32
struct qseecom_register_listener_req {
	uint32_t listener_id; 
	int32_t ifd_data_fd; 
	uint32_t virt_sb_base; 
	uint32_t sb_size; 
};

struct qseecom_send_cmd_req {
	void *cmd_req_buf; 
	unsigned int cmd_req_len; 
	void *resp_buf; 
	unsigned int resp_len; 
};

struct qseecom_ion_fd_info {
	int32_t fd;
	uint32_t cmd_buf_offset;
};
struct qseecom_send_modfd_cmd_req {
	void *cmd_req_buf; 
	unsigned int cmd_req_len; 
	void *resp_buf; 
	unsigned int resp_len; 
	struct qseecom_ion_fd_info ifd_data[MAX_ION_FD];
};

struct qseecom_send_resp_req {
	void *resp_buf; 
	unsigned int resp_len; 
};

struct qseecom_load_img_req {
	uint32_t mdt_len; 
	uint32_t img_len; 
	int32_t  ifd_data_fd; 
	char	 img_name[MAX_APP_NAME_SIZE]; 
	int app_id; 
};

struct qseecom_set_sb_mem_param_req {
	int32_t ifd_data_fd; 
	uint32_t virt_sb_base; 
	uint32_t sb_len; 
};

struct qseecom_qseos_version_req {
	unsigned int qseos_version; 
};

struct qseecom_qseos_app_load_query {
	char app_name[MAX_APP_NAME_SIZE]; 
	int app_id; 
};

struct qseecom_send_svc_cmd_req {
	uint32_t cmd_id;
	void *cmd_req_buf; 
	unsigned int cmd_req_len; 
	void *resp_buf; 
	unsigned int resp_len; 
};

enum qseecom_key_management_usage_type {
	QSEOS_KM_USAGE_DISK_ENCRYPTION = 0x01,
};

struct qseecom_create_key_req {
	unsigned char hash32[QSEECOM_HASH_SIZE];
	enum qseecom_key_management_usage_type usage;
};

struct qseecom_wipe_key_req {
	enum qseecom_key_management_usage_type usage;
};

#define SHA256_DIGEST_LENGTH	(256/8)
struct qseecom_save_partition_hash_req {
	int partition_id; 
	char digest[SHA256_DIGEST_LENGTH]; 
};

struct qseecom_is_es_activated_req {
	int is_activated; 
};

enum qseecom_buffer_protection {
	QSEOS_UNPROTECTED_BUFFER,
	QSEOS_PROTECT_BUFFER,
	QSEOS_UNPROTECT_PROTECTED_BUFFER,
};

enum qseecom_bandwidth_request_mode {
	INACTIVE = 0,
	LOW,
	MEDIUM,
	HIGH,
};

struct qseecom_send_modfd_listener_resp {
	void *resp_buf_ptr; 
	unsigned int resp_len; 
	struct qseecom_ion_fd_info ifd_data[MAX_ION_FD]; 
	enum qseecom_buffer_protection protection_mode; 
};

#define QSEECOM_IOC_MAGIC    0x97


#define QSEECOM_IOCTL_REGISTER_LISTENER_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 1, struct qseecom_register_listener_req)

#define QSEECOM_IOCTL_UNREGISTER_LISTENER_REQ \
	_IO(QSEECOM_IOC_MAGIC, 2)

#define QSEECOM_IOCTL_SEND_CMD_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 3, struct qseecom_send_cmd_req)

#define QSEECOM_IOCTL_SEND_MODFD_CMD_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 4, struct qseecom_send_modfd_cmd_req)

#define QSEECOM_IOCTL_RECEIVE_REQ \
	_IO(QSEECOM_IOC_MAGIC, 5)

#define QSEECOM_IOCTL_SEND_RESP_REQ \
	_IO(QSEECOM_IOC_MAGIC, 6)

#define QSEECOM_IOCTL_LOAD_APP_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 7, struct qseecom_load_img_req)

#define QSEECOM_IOCTL_SET_MEM_PARAM_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 8, struct qseecom_set_sb_mem_param_req)

#define QSEECOM_IOCTL_UNLOAD_APP_REQ \
	_IO(QSEECOM_IOC_MAGIC, 9)

#define QSEECOM_IOCTL_GET_QSEOS_VERSION_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 10, struct qseecom_qseos_version_req)

#define QSEECOM_IOCTL_PERF_ENABLE_REQ \
	_IO(QSEECOM_IOC_MAGIC, 11)

#define QSEECOM_IOCTL_PERF_DISABLE_REQ \
	_IO(QSEECOM_IOC_MAGIC, 12)

#define QSEECOM_IOCTL_LOAD_EXTERNAL_ELF_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 13, struct qseecom_load_img_req)

#define QSEECOM_IOCTL_UNLOAD_EXTERNAL_ELF_REQ \
	_IO(QSEECOM_IOC_MAGIC, 14)

#define QSEECOM_IOCTL_APP_LOADED_QUERY_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 15, struct qseecom_qseos_app_load_query)

#define QSEECOM_IOCTL_SEND_CMD_SERVICE_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 16, struct qseecom_send_svc_cmd_req)

#define QSEECOM_IOCTL_CREATE_KEY_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 17, struct qseecom_create_key_req)

#define QSEECOM_IOCTL_WIPE_KEY_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 18, struct qseecom_wipe_key_req)

#define QSEECOM_IOCTL_SAVE_PARTITION_HASH_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 19, struct qseecom_save_partition_hash_req)

#define QSEECOM_IOCTL_IS_ES_ACTIVATED_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 20, struct qseecom_is_es_activated_req)

#define QSEECOM_IOCTL_SEND_MODFD_RESP \
	_IOWR(QSEECOM_IOC_MAGIC, 21, struct qseecom_send_modfd_listener_resp)

#define QSEECOM_IOCTL_UNPROTECT_BUF \
	_IOWR(QSEECOM_IOC_MAGIC, 22, int)

#define QSEECOM_IOCTL_SET_BUS_SCALING_REQ \
	_IOWR(QSEECOM_IOC_MAGIC, 23, int)

#endif 
