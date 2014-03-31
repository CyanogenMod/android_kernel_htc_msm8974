/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "qseecom_kernel.h"
#include "mpq_sdmx.h"

static struct qseecom_handle *sdmx_qseecom_handles[SDMX_MAX_SESSIONS];
static struct mutex sdmx_lock[SDMX_MAX_SESSIONS];

#define QSEECOM_ALIGN_SIZE	0x40
#define QSEECOM_ALIGN_MASK	(QSEECOM_ALIGN_SIZE - 1)
#define QSEECOM_ALIGN(x)	\
	((x + QSEECOM_ALIGN_SIZE) & (~QSEECOM_ALIGN_MASK))

enum sdmx_cmd_id {
	SDMX_OPEN_SESSION_CMD,
	SDMX_CLOSE_SESSION_CMD,
	SDMX_SET_SESSION_CFG_CMD,
	SDMX_ADD_FILTER_CMD,
	SDMX_REMOVE_FILTER_CMD,
	SDMX_SET_KL_IDX_CMD,
	SDMX_ADD_RAW_PID_CMD,
	SDMX_REMOVE_RAW_PID_CMD,
	SDMX_PROCESS_CMD,
	SDMX_GET_DBG_COUNTERS_CMD,
	SDMX_RESET_DBG_COUNTERS_CMD,
	SDMX_GET_VERSION_CMD,
	SDMX_INVALIDATE_KL_CMD,
	SDMX_SET_LOG_LEVEL_CMD
};

struct sdmx_proc_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u8 flags;
	struct sdmx_buff_descr in_buf_descr;
	u32 inp_fill_cnt;
	u32 in_rd_offset;
	u32 num_filters;
	struct sdmx_filter_status filters_status[];
};

struct sdmx_proc_rsp {
	enum sdmx_status ret;
	u32 inp_fill_cnt;
	u32 in_rd_offset;
	u32 err_indicators;
	u32 status_indicators;
};

struct sdmx_open_ses_req {
	enum sdmx_cmd_id cmd_id;
};

struct sdmx_open_ses_rsp {
	enum sdmx_status ret;
	u32 session_handle;
};

struct sdmx_close_ses_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
};

struct sdmx_close_ses_rsp {
	enum sdmx_status ret;
};

struct sdmx_ses_cfg_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	enum sdmx_proc_mode process_mode;
	enum sdmx_inp_mode input_mode;
	enum sdmx_pkt_format packet_len;
	u8 odd_scramble_bits;
	u8 even_scramble_bits;
};

struct sdmx_ses_cfg_rsp {
	enum sdmx_status ret;
};

struct sdmx_set_kl_ind_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 pid;
	u32 kl_index;
};

struct sdmx_set_kl_ind_rsp {
	enum sdmx_status ret;
};

struct sdmx_add_filt_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 pid;
	enum sdmx_filter filter_type;
	struct sdmx_buff_descr meta_data_buf;
	enum sdmx_buf_mode buffer_mode;
	enum sdmx_raw_out_format ts_out_format;
	u32 flags;
	u32 num_data_bufs;
	struct sdmx_data_buff_descr data_bufs[];
};

struct sdmx_add_filt_rsp {
	enum sdmx_status ret;
	u32 filter_handle;
};

struct sdmx_rem_filt_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 filter_handle;
};

struct sdmx_rem_filt_rsp {
	enum sdmx_status ret;
};

struct sdmx_add_raw_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 filter_handle;
	u32 pid;
};

struct sdmx_add_raw_rsp {
	enum sdmx_status ret;
};

struct sdmx_rem_raw_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 filter_handle;
	u32 pid;
};

struct sdmx_rem_raw_rsp {
	enum sdmx_status ret;
};

struct sdmx_get_counters_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
	u32 num_filters;
};

struct sdmx_get_counters_rsp {
	enum sdmx_status ret;
	struct sdmx_session_dbg_counters session_counters;
	u32 num_filters;
	struct sdmx_filter_dbg_counters filter_counters[];
};

struct sdmx_rst_counters_req {
	enum sdmx_cmd_id cmd_id;
	u32 session_handle;
};

struct sdmx_rst_counters_rsp {
	enum sdmx_status ret;
};

struct sdmx_get_version_req {
	enum sdmx_cmd_id cmd_id;
};

struct sdmx_get_version_rsp {
	enum sdmx_status ret;
	int32_t version;
};

struct sdmx_set_log_level_req {
	enum sdmx_cmd_id cmd_id;
	enum sdmx_log_level level;
	u32 session_handle;
};

struct sdmx_set_log_level_rsp {
	enum sdmx_status ret;
};
static void get_cmd_rsp_buffers(int handle_index,
	void **cmd,
	int *cmd_len,
	void **rsp,
	int *rsp_len)
{
	*cmd = sdmx_qseecom_handles[handle_index]->sbuf;

	if (*cmd_len & QSEECOM_ALIGN_MASK)
		*cmd_len = QSEECOM_ALIGN(*cmd_len);

	*rsp = sdmx_qseecom_handles[handle_index]->sbuf + *cmd_len;

	if (*rsp_len & QSEECOM_ALIGN_MASK)
		*rsp_len = QSEECOM_ALIGN(*rsp_len);

}

int sdmx_get_version(int session_handle, int32_t *version)
{
	int res, cmd_len, rsp_len;
	struct sdmx_get_version_req *cmd;
	struct sdmx_get_version_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS) ||
		(version == NULL))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_get_version_req);
	rsp_len = sizeof(struct sdmx_get_version_rsp);

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	cmd->cmd_id = SDMX_GET_VERSION_CMD;

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;
	*version = rsp->version;

	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;

}
EXPORT_SYMBOL(sdmx_get_version);

int sdmx_open_session(int *session_handle)
{
	int res, cmd_len, rsp_len;
	enum sdmx_status ret, version_ret;
	struct sdmx_open_ses_req *cmd;
	struct sdmx_open_ses_rsp *rsp;
	struct qseecom_handle *qseecom_handle = NULL;
	int32_t version;

	
	if (session_handle == NULL)
		return SDMX_STATUS_GENERAL_FAILURE;

	
	res = qseecom_start_app(&qseecom_handle, "securemm", 4096);

	if (res < 0)
		return SDMX_STATUS_GENERAL_FAILURE;

	cmd_len = sizeof(struct sdmx_open_ses_req);
	rsp_len = sizeof(struct sdmx_open_ses_rsp);

	
	cmd = (struct sdmx_open_ses_req *)qseecom_handle->sbuf;

	if (cmd_len & QSEECOM_ALIGN_MASK)
		cmd_len = QSEECOM_ALIGN(cmd_len);

	rsp = (struct sdmx_open_ses_rsp *)qseecom_handle->sbuf + cmd_len;

	if (rsp_len & QSEECOM_ALIGN_MASK)
		rsp_len = QSEECOM_ALIGN(rsp_len);

	
	*session_handle = SDMX_INVALID_SESSION_HANDLE;

	
	cmd->cmd_id = SDMX_OPEN_SESSION_CMD;

	
	res = qseecom_send_command(qseecom_handle, (void *)cmd, cmd_len,
		(void *)rsp, rsp_len);

	if (res < 0) {
		qseecom_shutdown_app(&qseecom_handle);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	
	*session_handle = rsp->session_handle;

	
	sdmx_qseecom_handles[*session_handle] = qseecom_handle;
	mutex_init(&sdmx_lock[*session_handle]);
	ret = rsp->ret;

	
	version_ret = sdmx_get_version(*session_handle, &version);
	if (SDMX_SUCCESS == version_ret)
		pr_info("TZ SDMX version is %x.%x\n", version >> 8,
			version & 0xFF);
	else
		pr_err("Error reading TZ SDMX version\n");

	return ret;
}
EXPORT_SYMBOL(sdmx_open_session);

int sdmx_close_session(int session_handle)
{
	int res, cmd_len, rsp_len;
	struct sdmx_close_ses_req *cmd;
	struct sdmx_close_ses_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_close_ses_req);
	rsp_len = sizeof(struct sdmx_close_ses_rsp);

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	cmd->cmd_id = SDMX_CLOSE_SESSION_CMD;
	cmd->session_handle = session_handle;

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;

	
	res = qseecom_shutdown_app(&sdmx_qseecom_handles[session_handle]);
	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	sdmx_qseecom_handles[session_handle] = NULL;

	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_close_session);

int sdmx_set_session_cfg(int session_handle,
	enum sdmx_proc_mode proc_mode,
	enum sdmx_inp_mode inp_mode,
	enum sdmx_pkt_format pkt_format,
	u8 odd_scramble_bits,
	u8 even_scramble_bits)
{
	int res, cmd_len, rsp_len;
	struct sdmx_ses_cfg_req *cmd;
	struct sdmx_ses_cfg_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_ses_cfg_req);
	rsp_len = sizeof(struct sdmx_ses_cfg_rsp);

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	cmd->cmd_id = SDMX_SET_SESSION_CFG_CMD;
	cmd->session_handle = session_handle;
	cmd->process_mode = proc_mode;
	cmd->input_mode = inp_mode;
	cmd->packet_len = pkt_format;
	cmd->odd_scramble_bits = odd_scramble_bits;
	cmd->even_scramble_bits = even_scramble_bits;

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;

	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_set_session_cfg);

int sdmx_add_filter(int session_handle,
	u16 pid,
	enum sdmx_filter filterype,
	struct sdmx_buff_descr *meta_data_buf,
	enum sdmx_buf_mode d_buf_mode,
	u32 num_data_bufs,
	struct sdmx_data_buff_descr *data_bufs,
	int *filter_handle,
	enum sdmx_raw_out_format ts_out_format,
	u32 flags)
{
	int res, cmd_len, rsp_len;
	struct sdmx_add_filt_req *cmd;
	struct sdmx_add_filt_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS) ||
		(filter_handle == NULL))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_add_filt_req)
		+ num_data_bufs * sizeof(struct sdmx_data_buff_descr);
	rsp_len = sizeof(struct sdmx_add_filt_rsp);

	
	*filter_handle = SDMX_INVALID_FILTER_HANDLE;

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	cmd->cmd_id = SDMX_ADD_FILTER_CMD;
	cmd->session_handle = session_handle;
	cmd->pid = (u32)pid;
	cmd->filter_type = filterype;
	cmd->ts_out_format = ts_out_format;
	cmd->flags = flags;
	if (meta_data_buf != NULL)
		memcpy(&(cmd->meta_data_buf), meta_data_buf,
			sizeof(struct sdmx_buff_descr));
	else
		memset(&(cmd->meta_data_buf), 0,
			sizeof(struct sdmx_buff_descr));

	cmd->buffer_mode = d_buf_mode;
	cmd->num_data_bufs = num_data_bufs;
	memcpy(cmd->data_bufs, data_bufs,
		num_data_bufs * sizeof(struct sdmx_data_buff_descr));

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	
	*filter_handle = rsp->filter_handle;
	ret = rsp->ret;

	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_add_filter);

int sdmx_remove_filter(int session_handle, int filter_handle)
{
	int res, cmd_len, rsp_len;
	struct sdmx_rem_filt_req *cmd;
	struct sdmx_rem_filt_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_rem_filt_req);
	rsp_len = sizeof(struct sdmx_rem_filt_rsp);

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	cmd->cmd_id = SDMX_REMOVE_FILTER_CMD;
	cmd->session_handle = session_handle;
	cmd->filter_handle = filter_handle;

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;

	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_remove_filter);

int sdmx_set_kl_ind(int session_handle, u16 pid, u32 key_ladder_index)
{
	int res, cmd_len, rsp_len;
	struct sdmx_set_kl_ind_req *cmd;
	struct sdmx_set_kl_ind_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_set_kl_ind_req);
	rsp_len = sizeof(struct sdmx_set_kl_ind_rsp);

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	cmd->cmd_id = SDMX_SET_KL_IDX_CMD;
	cmd->session_handle = session_handle;
	cmd->pid = (u32)pid;
	cmd->kl_index = key_ladder_index;

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;

	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_set_kl_ind);

int sdmx_add_raw_pid(int session_handle, int filter_handle, u16 pid)
{
	int res, cmd_len, rsp_len;
	struct sdmx_add_raw_req *cmd;
	struct sdmx_add_raw_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_add_raw_req);
	rsp_len = sizeof(struct sdmx_add_raw_rsp);

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	cmd->cmd_id = SDMX_ADD_RAW_PID_CMD;
	cmd->session_handle = session_handle;
	cmd->filter_handle = filter_handle;
	cmd->pid = (u32)pid;

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;

	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_add_raw_pid);

int sdmx_remove_raw_pid(int session_handle, int filter_handle, u16 pid)
{
	int res, cmd_len, rsp_len;
	struct sdmx_rem_raw_req *cmd;
	struct sdmx_rem_raw_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_rem_raw_req);
	rsp_len = sizeof(struct sdmx_rem_raw_rsp);

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	cmd->cmd_id = SDMX_REMOVE_RAW_PID_CMD;
	cmd->session_handle = session_handle;
	cmd->filter_handle = filter_handle;
	cmd->pid = (u32)pid;

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;

	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_remove_raw_pid);

int sdmx_process(int session_handle, u8 flags,
	struct sdmx_buff_descr *input_buf_desc,
	u32 *input_fill_count,
	u32 *input_read_offset,
	u32 *error_indicators,
	u32 *status_indicators,
	u32 num_filters,
	struct sdmx_filter_status *filter_status)
{
	int res, cmd_len, rsp_len;
	struct sdmx_proc_req *cmd;
	struct sdmx_proc_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS) ||
		(input_buf_desc == NULL) ||
		(input_fill_count == NULL) || (input_read_offset == NULL) ||
		(error_indicators == NULL) || (status_indicators == NULL) ||
		(filter_status == NULL))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_proc_req)
		+ num_filters * sizeof(struct sdmx_filter_status);
	rsp_len = sizeof(struct sdmx_proc_rsp);

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	cmd->cmd_id = SDMX_PROCESS_CMD;
	cmd->session_handle = session_handle;
	cmd->flags = flags;
	cmd->in_buf_descr.base_addr = input_buf_desc->base_addr;
	cmd->in_buf_descr.size = input_buf_desc->size;
	cmd->inp_fill_cnt = *input_fill_count;
	cmd->in_rd_offset = *input_read_offset;
	cmd->num_filters = num_filters;
	memcpy(cmd->filters_status, filter_status,
		num_filters * sizeof(struct sdmx_filter_status));

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	
	*input_fill_count = rsp->inp_fill_cnt;
	*input_read_offset = rsp->in_rd_offset;
	*error_indicators = rsp->err_indicators;
	*status_indicators = rsp->status_indicators;
	memcpy(filter_status, cmd->filters_status,
		num_filters * sizeof(struct sdmx_filter_status));
	ret = rsp->ret;

	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_process);

int sdmx_get_dbg_counters(int session_handle,
	struct sdmx_session_dbg_counters *session_counters,
	u32 *num_filters,
	struct sdmx_filter_dbg_counters *filter_counters)
{
	int res, cmd_len, rsp_len;
	struct sdmx_get_counters_req *cmd;
	struct sdmx_get_counters_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS) ||
		(session_counters == NULL) || (num_filters == NULL) ||
		(filter_counters == NULL))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_get_counters_req);
	rsp_len = sizeof(struct sdmx_get_counters_rsp)
		+ *num_filters * sizeof(struct sdmx_filter_dbg_counters);

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	cmd->cmd_id = SDMX_GET_DBG_COUNTERS_CMD;
	cmd->session_handle = session_handle;
	cmd->num_filters = *num_filters;

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	
	*session_counters = rsp->session_counters;
	*num_filters = rsp->num_filters;
	memcpy(filter_counters, rsp->filter_counters,
		*num_filters * sizeof(struct sdmx_filter_dbg_counters));
	ret = rsp->ret;

	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_get_dbg_counters);

int sdmx_reset_dbg_counters(int session_handle)
{
	int res, cmd_len, rsp_len;
	struct sdmx_rst_counters_req *cmd;
	struct sdmx_rst_counters_rsp *rsp;
	enum sdmx_status ret;

	if ((session_handle < 0) || (session_handle >= SDMX_MAX_SESSIONS))
		return SDMX_STATUS_INVALID_INPUT_PARAMS;

	cmd_len = sizeof(struct sdmx_rst_counters_req);
	rsp_len = sizeof(struct sdmx_rst_counters_rsp);

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	cmd->cmd_id = SDMX_RESET_DBG_COUNTERS_CMD;
	cmd->session_handle = session_handle;

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);

	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}

	ret = rsp->ret;

	mutex_unlock(&sdmx_lock[session_handle]);

	return ret;
}
EXPORT_SYMBOL(sdmx_reset_dbg_counters);

int sdmx_set_log_level(int session_handle, enum sdmx_log_level level)
{
	int res, cmd_len, rsp_len;
	struct sdmx_set_log_level_req *cmd;
	struct sdmx_set_log_level_rsp *rsp;
	enum sdmx_status ret;

	cmd_len = sizeof(struct sdmx_set_log_level_req);
	rsp_len = sizeof(struct sdmx_set_log_level_rsp);

	
	get_cmd_rsp_buffers(session_handle, (void **)&cmd, &cmd_len,
		(void **)&rsp, &rsp_len);

	
	mutex_lock(&sdmx_lock[session_handle]);

	
	cmd->cmd_id = SDMX_SET_LOG_LEVEL_CMD;
	cmd->session_handle = session_handle;
	cmd->level = level;

	
	res = qseecom_send_command(sdmx_qseecom_handles[session_handle],
		(void *)cmd, cmd_len, (void *)rsp, rsp_len);
	if (res < 0) {
		mutex_unlock(&sdmx_lock[session_handle]);
		return SDMX_STATUS_GENERAL_FAILURE;
	}
	ret = rsp->ret;

	
	mutex_unlock(&sdmx_lock[session_handle]);
	return ret;
}

