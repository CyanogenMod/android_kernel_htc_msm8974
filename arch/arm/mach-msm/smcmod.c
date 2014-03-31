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
 *
 */

#define KMSG_COMPONENT "SMCMOD"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/msm_ion.h>
#include <asm/smcmod.h>
#include <mach/scm.h>
#include <mach/socinfo.h>

static DEFINE_MUTEX(ioctl_lock);

#define SMCMOD_SVC_DEFAULT (0)
#define SMCMOD_SVC_CRYPTO (1)
#define SMCMOD_CRYPTO_CMD_CIPHER (1)
#define SMCMOD_CRYPTO_CMD_MSG_DIGEST_FIXED (2)
#define SMCMOD_CRYPTO_CMD_MSG_DIGEST (3)

struct smcmod_cipher_scm_req {
	uint32_t algorithm;
	uint32_t operation;
	uint32_t mode;
	uint32_t key_phys_addr;
	uint32_t key_size;
	uint32_t plain_text_phys_addr;
	uint32_t plain_text_size;
	uint32_t cipher_text_phys_addr;
	uint32_t cipher_text_size;
	uint32_t init_vector_phys_addr;
	uint32_t init_vector_size;
};

struct smcmod_msg_digest_scm_req {
	uint32_t algorithm;
	uint32_t key_phys_addr;
	uint32_t key_size;
	uint32_t input_phys_addr;
	uint32_t input_size;
	uint32_t output_phys_addr;
	uint32_t output_size;
	uint8_t verify;
} __packed;

static int smcmod_ion_fd_to_phys(int32_t fd, struct ion_client *ion_clientp,
	struct ion_handle **ion_handlep, uint32_t *phys_addrp, size_t *sizep)
{
	int ret = 0;

	
	if ((fd < 0) || IS_ERR_OR_NULL(ion_clientp) ||
		IS_ERR_OR_NULL(ion_handlep) || IS_ERR_OR_NULL(phys_addrp) ||
		IS_ERR_OR_NULL(sizep))
		return -EINVAL;

	
	*ion_handlep = ion_import_dma_buf(ion_clientp, fd);

	
	if (IS_ERR_OR_NULL(*ion_handlep))
		return -EINVAL;

	
	ret = ion_phys(ion_clientp, *ion_handlep, (ion_phys_addr_t *)phys_addrp,
		sizep);

	return ret;
}

static int smcmod_send_buf_cmd(struct smcmod_buf_req *reqp)
{
	int ret = 0;
	struct ion_client *ion_clientp = NULL;
	struct ion_handle *ion_cmd_handlep = NULL;
	struct ion_handle *ion_resp_handlep = NULL;
	void *cmd_vaddrp = NULL;
	void *resp_vaddrp = NULL;
	unsigned long cmd_buf_size = 0;
	unsigned long resp_buf_size = 0;

	
	if (IS_ERR_OR_NULL(reqp))
		return -EINVAL;

	
	if (reqp->ion_cmd_fd < 0)
		return -EINVAL;

	
	ion_clientp = msm_ion_client_create(UINT_MAX, "smcmod");

	
	if (IS_ERR_OR_NULL(ion_clientp))
		return -EINVAL;

	
	ion_cmd_handlep = ion_import_dma_buf(ion_clientp, reqp->ion_cmd_fd);

	
	if (IS_ERR_OR_NULL(ion_cmd_handlep)) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	
	if (ion_handle_get_size(ion_clientp, ion_cmd_handlep,
		&cmd_buf_size) < 0) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	if (reqp->cmd_len > cmd_buf_size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	
	cmd_vaddrp = ion_map_kernel(ion_clientp, ion_cmd_handlep);

	
	if (IS_ERR_OR_NULL(cmd_vaddrp)) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	
	if (reqp->ion_resp_fd >= 0) {
		
		ion_resp_handlep =
			ion_import_dma_buf(ion_clientp, reqp->ion_resp_fd);

		
		if (IS_ERR_OR_NULL(ion_resp_handlep)) {
			ret = -EINVAL;
			goto buf_cleanup;
		}

		
		if (ion_handle_get_size(ion_clientp, ion_resp_handlep,
			&resp_buf_size) < 0) {
			ret = -EINVAL;
			goto buf_cleanup;
		}

		if (reqp->resp_len > resp_buf_size) {
			ret = -EINVAL;
			goto buf_cleanup;
		}

		
		resp_vaddrp = ion_map_kernel(ion_clientp, ion_resp_handlep);

		
		if (IS_ERR_OR_NULL(resp_vaddrp)) {
			ret = -EINVAL;
			goto buf_cleanup;
		}
	}


	
	reqp->return_val = scm_call(reqp->service_id, reqp->command_id,
		cmd_vaddrp, reqp->cmd_len, resp_vaddrp, reqp->resp_len);


buf_cleanup:
	
	if (!IS_ERR_OR_NULL(ion_clientp)) {
		if (!IS_ERR_OR_NULL(ion_cmd_handlep)) {
			if (!IS_ERR_OR_NULL(cmd_vaddrp))
				ion_unmap_kernel(ion_clientp, ion_cmd_handlep);
			ion_free(ion_clientp, ion_cmd_handlep);
		}

		if (!IS_ERR_OR_NULL(ion_resp_handlep)) {
			if (!IS_ERR_OR_NULL(resp_vaddrp))
				ion_unmap_kernel(ion_clientp, ion_resp_handlep);
			ion_free(ion_clientp, ion_resp_handlep);
		}

		ion_client_destroy(ion_clientp);
	}

	return ret;
}

static int smcmod_send_cipher_cmd(struct smcmod_cipher_req *reqp)
{
	int ret = 0;
	struct smcmod_cipher_scm_req scm_req;
	struct ion_client *ion_clientp = NULL;
	struct ion_handle *ion_key_handlep = NULL;
	struct ion_handle *ion_plain_handlep = NULL;
	struct ion_handle *ion_cipher_handlep = NULL;
	struct ion_handle *ion_iv_handlep = NULL;
	size_t size = 0;

	if (IS_ERR_OR_NULL(reqp))
		return -EINVAL;

	
	if ((reqp->ion_plain_text_fd < 0) ||
		(reqp->ion_cipher_text_fd < 0) ||
		(reqp->ion_init_vector_fd < 0))
		return -EINVAL;

	
	ion_clientp = msm_ion_client_create(UINT_MAX, "smcmod");

	
	if (IS_ERR_OR_NULL(ion_clientp))
		return -EINVAL;

	
	scm_req.algorithm = reqp->algorithm;
	scm_req.operation = reqp->operation;
	scm_req.mode = reqp->mode;
	scm_req.key_phys_addr = 0;
	scm_req.key_size = reqp->key_size;
	scm_req.plain_text_size = reqp->plain_text_size;
	scm_req.cipher_text_size = reqp->cipher_text_size;
	scm_req.init_vector_size = reqp->init_vector_size;

	if (!reqp->key_is_null) {
		
		ret = smcmod_ion_fd_to_phys(reqp->ion_key_fd, ion_clientp,
			&ion_key_handlep, &scm_req.key_phys_addr, &size);
		if (ret < 0)
			goto buf_cleanup;

		if (reqp->key_size > size) {
			ret = -EINVAL;
			goto buf_cleanup;
		}
	}

	
	ret = smcmod_ion_fd_to_phys(reqp->ion_plain_text_fd, ion_clientp,
		&ion_plain_handlep, &scm_req.plain_text_phys_addr, &size);

	if (ret < 0)
		goto buf_cleanup;

	if (reqp->plain_text_size > size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	
	ret = smcmod_ion_fd_to_phys(reqp->ion_cipher_text_fd, ion_clientp,
		&ion_cipher_handlep, &scm_req.cipher_text_phys_addr, &size);
	if (ret < 0)
		goto buf_cleanup;

	if (reqp->cipher_text_size > size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	
	ret = smcmod_ion_fd_to_phys(reqp->ion_init_vector_fd, ion_clientp,
		&ion_iv_handlep, &scm_req.init_vector_phys_addr, &size);
	if (ret < 0)
		goto buf_cleanup;

	if (reqp->init_vector_size > size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	msm_ion_do_cache_op(ion_clientp, ion_key_handlep, NULL,
		scm_req.key_size, ION_IOC_CLEAN_CACHES);
	msm_ion_do_cache_op(ion_clientp, ion_iv_handlep, NULL,
		scm_req.init_vector_size, ION_IOC_CLEAN_CACHES);

	
	if (reqp->operation)
		msm_ion_do_cache_op(ion_clientp, ion_cipher_handlep, NULL,
			scm_req.cipher_text_size, ION_IOC_CLEAN_CACHES);
	else
		msm_ion_do_cache_op(ion_clientp, ion_plain_handlep, NULL,
			scm_req.plain_text_size, ION_IOC_CLEAN_CACHES);

	
	reqp->return_val = scm_call(SMCMOD_SVC_CRYPTO,
		SMCMOD_CRYPTO_CMD_CIPHER, &scm_req,
		sizeof(scm_req), NULL, 0);

	

	
	if (reqp->operation)
		msm_ion_do_cache_op(ion_clientp, ion_plain_handlep, NULL,
			scm_req.plain_text_size, ION_IOC_INV_CACHES);
	else
		msm_ion_do_cache_op(ion_clientp, ion_cipher_handlep, NULL,
			scm_req.cipher_text_size, ION_IOC_INV_CACHES);

buf_cleanup:
	
	if (!IS_ERR_OR_NULL(ion_clientp)) {
		if (!IS_ERR_OR_NULL(ion_key_handlep))
			ion_free(ion_clientp, ion_key_handlep);

		if (!IS_ERR_OR_NULL(ion_plain_handlep))
			ion_free(ion_clientp, ion_plain_handlep);

		if (!IS_ERR_OR_NULL(ion_cipher_handlep))
			ion_free(ion_clientp, ion_cipher_handlep);

		if (!IS_ERR_OR_NULL(ion_iv_handlep))
			ion_free(ion_clientp, ion_iv_handlep);

		ion_client_destroy(ion_clientp);
	}

	return ret;
}
static int smcmod_send_msg_digest_cmd(struct smcmod_msg_digest_req *reqp)
{
	int ret = 0;
	struct smcmod_msg_digest_scm_req scm_req;
	struct ion_client *ion_clientp = NULL;
	struct ion_handle *ion_key_handlep = NULL;
	struct ion_handle *ion_input_handlep = NULL;
	struct ion_handle *ion_output_handlep = NULL;
	size_t size = 0;

	if (IS_ERR_OR_NULL(reqp))
		return -EINVAL;

	
	if ((reqp->ion_input_fd < 0) || (reqp->ion_output_fd < 0))
		return -EINVAL;

	
	ion_clientp = msm_ion_client_create(UINT_MAX, "smcmod");

	
	if (IS_ERR_OR_NULL(ion_clientp))
		return -EINVAL;

	
	scm_req.algorithm = reqp->algorithm;
	scm_req.key_phys_addr = 0;
	scm_req.key_size = reqp->key_size;
	scm_req.input_size = reqp->input_size;
	scm_req.output_size = reqp->output_size;
	scm_req.verify = 0;

	if (!reqp->key_is_null) {
		
		ret = smcmod_ion_fd_to_phys(reqp->ion_key_fd, ion_clientp,
			&ion_key_handlep, &scm_req.key_phys_addr, &size);
		if (ret < 0)
			goto buf_cleanup;

		if (reqp->key_size > size) {
			ret = -EINVAL;
			goto buf_cleanup;
		}
	}

	
	ret = smcmod_ion_fd_to_phys(reqp->ion_input_fd, ion_clientp,
		&ion_input_handlep, &scm_req.input_phys_addr, &size);
	if (ret < 0)
		goto buf_cleanup;

	if (reqp->input_size > size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	
	ret = smcmod_ion_fd_to_phys(reqp->ion_output_fd, ion_clientp,
		&ion_output_handlep, &scm_req.output_phys_addr, &size);
	if (ret < 0)
		goto buf_cleanup;

	if (reqp->output_size > size) {
		ret = -EINVAL;
		goto buf_cleanup;
	}

	msm_ion_do_cache_op(ion_clientp, ion_key_handlep, NULL,
		scm_req.key_size, ION_IOC_CLEAN_CACHES);
	msm_ion_do_cache_op(ion_clientp, ion_input_handlep, NULL,
		scm_req.input_size, ION_IOC_CLEAN_CACHES);

	
	if (reqp->fixed_block)
		reqp->return_val = scm_call(SMCMOD_SVC_CRYPTO,
			SMCMOD_CRYPTO_CMD_MSG_DIGEST_FIXED,
			&scm_req,
			sizeof(scm_req),
			NULL, 0);
	else
		reqp->return_val = scm_call(SMCMOD_SVC_CRYPTO,
			SMCMOD_CRYPTO_CMD_MSG_DIGEST,
			&scm_req,
			sizeof(scm_req),
			NULL, 0);

	
	msm_ion_do_cache_op(ion_clientp, ion_output_handlep, NULL,
		scm_req.output_size, ION_IOC_INV_CACHES);

buf_cleanup:
	
	if (!IS_ERR_OR_NULL(ion_clientp)) {
		if (!IS_ERR_OR_NULL(ion_key_handlep))
			ion_free(ion_clientp, ion_key_handlep);

		if (!IS_ERR_OR_NULL(ion_input_handlep))
			ion_free(ion_clientp, ion_input_handlep);

		if (!IS_ERR_OR_NULL(ion_output_handlep))
			ion_free(ion_clientp, ion_output_handlep);

		ion_client_destroy(ion_clientp);
	}

	return ret;
}

static int smcmod_send_dec_cmd(struct smcmod_decrypt_req *reqp)
{
	struct ion_client *ion_clientp;
	struct ion_handle *ion_handlep = NULL;
	int ion_fd;
	int ret;
	u32 pa;
	size_t size;
	struct {
		u32 args[4];
	} req;
	struct {
		u32 args[3];
	} rsp;

	ion_clientp = msm_ion_client_create(UINT_MAX, "smcmod");
	if (IS_ERR_OR_NULL(ion_clientp))
		return PTR_ERR(ion_clientp);

	switch (reqp->operation) {
	case SMCMOD_DECRYPT_REQ_OP_METADATA: {
		ion_fd = reqp->request.metadata.ion_fd;
		ret = smcmod_ion_fd_to_phys(ion_fd, ion_clientp,
					    &ion_handlep, &pa, &size);
		if (ret)
			goto error;

		req.args[0] = reqp->request.metadata.len;
		req.args[1] = pa;
		break;
	}
	case SMCMOD_DECRYPT_REQ_OP_IMG_FRAG: {
		ion_fd = reqp->request.img_frag.ion_fd;
		ret = smcmod_ion_fd_to_phys(ion_fd, ion_clientp,
					    &ion_handlep, &pa, &size);
		if (ret)
			goto error;

		req.args[0] = reqp->request.img_frag.ctx_id;
		req.args[1] = reqp->request.img_frag.last_frag;
		req.args[2] = reqp->request.img_frag.frag_len;
		req.args[3] = pa + reqp->request.img_frag.offset;
		break;
	}
	default:
		ret = -EINVAL;
		goto error;
	}


	ret = scm_call(reqp->service_id, reqp->command_id,
		       &req, sizeof(req), &rsp, sizeof(rsp));
	if (ret)
		goto error;

	switch (reqp->operation) {
	case SMCMOD_DECRYPT_REQ_OP_METADATA:
		reqp->response.metadata.status = rsp.args[0];
		reqp->response.metadata.ctx_id = rsp.args[1];
		reqp->response.metadata.end_offset = rsp.args[2] - pa;
		break;
	case SMCMOD_DECRYPT_REQ_OP_IMG_FRAG: {
		reqp->response.img_frag.status = rsp.args[0];
		break;
	}
	default:
		break;
	}

error:
	if (!IS_ERR_OR_NULL(ion_clientp)) {
		if (!IS_ERR_OR_NULL(ion_handlep))
			ion_free(ion_clientp, ion_handlep);
		ion_client_destroy(ion_clientp);
	}
	return ret;
}

static int smcmod_ioctl_check(unsigned cmd)
{
	switch (cmd) {
	case SMCMOD_IOCTL_SEND_REG_CMD:
	case SMCMOD_IOCTL_SEND_BUF_CMD:
	case SMCMOD_IOCTL_SEND_CIPHER_CMD:
	case SMCMOD_IOCTL_SEND_MSG_DIGEST_CMD:
	case SMCMOD_IOCTL_GET_VERSION:
		if (!cpu_is_fsm9xxx())
			return -EINVAL;
		break;
	case SMCMOD_IOCTL_SEND_DECRYPT_CMD:
		if (!cpu_is_msm8226())
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static long smcmod_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;

	
	if (!argp)
		return -EINVAL;

	mutex_lock(&ioctl_lock);

	ret = smcmod_ioctl_check(cmd);
	if (ret)
		goto cleanup;

	switch (cmd) {
	case SMCMOD_IOCTL_SEND_REG_CMD:
		{
			struct smcmod_reg_req req;

			
			if (copy_from_user((void *)&req, argp, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}

			if (req.num_args == 1) {
				req.return_val =
					scm_call_atomic1(req.service_id,
					req.command_id, req.args[0]);
			} else if (req.num_args == 2) {
				req.return_val =
					scm_call_atomic2(req.service_id,
					req.command_id, req.args[0],
					req.args[1]);
			} else {
				ret = -EINVAL;
				goto cleanup;
			}

			
			if (copy_to_user(argp, (void *)&req, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}
		}
		break;

	case SMCMOD_IOCTL_SEND_BUF_CMD:
		{
			struct smcmod_buf_req req;

			
			if (copy_from_user((void *)&req, argp, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}

			
			ret = smcmod_send_buf_cmd(&req);
			if (ret < 0)
				goto cleanup;

			
			if (copy_to_user(argp, (void *)&req, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}
		}
		break;

	case SMCMOD_IOCTL_SEND_CIPHER_CMD:
		{
			struct smcmod_cipher_req req;

			
			if (copy_from_user((void *)&req, argp, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}

			ret = smcmod_send_cipher_cmd(&req);
			if (ret < 0)
				goto cleanup;

			
			if (copy_to_user(argp, (void *)&req, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}
		}
		break;

	case SMCMOD_IOCTL_SEND_MSG_DIGEST_CMD:
		{
			struct smcmod_msg_digest_req req;

			
			if (copy_from_user((void *)&req, argp, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}

			ret = smcmod_send_msg_digest_cmd(&req);
			if (ret < 0)
				goto cleanup;

			
			if (copy_to_user(argp, (void *)&req, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}
		}
		break;

	case SMCMOD_IOCTL_GET_VERSION:
		{
			uint32_t req;

			
			req = scm_get_version();

			
			if (copy_to_user(argp, (void *)&req, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}
		}
		break;

	case SMCMOD_IOCTL_SEND_DECRYPT_CMD:
		{
			struct smcmod_decrypt_req req;

			if (copy_from_user((void *)&req, argp, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}

			ret = smcmod_send_dec_cmd(&req);
			if (ret < 0)
				goto cleanup;

			if (copy_to_user(argp, (void *)&req, sizeof(req))) {
				ret = -EFAULT;
				goto cleanup;
			}
		}
		break;

	default:
		ret = -EINVAL;
	}

cleanup:
	mutex_unlock(&ioctl_lock);
	return ret;
}

static int smcmod_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int smcmod_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations smcmod_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = smcmod_ioctl,
	.open = smcmod_open,
	.release = smcmod_release,
};

static struct miscdevice smcmod_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = SMCMOD_DEV,
	.fops = &smcmod_fops
};

static int __init smcmod_init(void)
{
	return misc_register(&smcmod_misc_dev);
}

static void __exit smcmod_exit(void)
{
	misc_deregister(&smcmod_misc_dev);
}

MODULE_DESCRIPTION("Qualcomm SMC Module");
MODULE_LICENSE("GPL v2");

module_init(smcmod_init);
module_exit(smcmod_exit);
