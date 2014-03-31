/*
 * Abilis Systems Single DVB-T Receiver
 * Copyright (C) 2008 Pierrick Hascoet <pierrick.hascoet@abilis.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include "as102_drv.h"
#include "as10x_cmd.h"

int as10x_cmd_add_PID_filter(struct as10x_bus_adapter_t *adap,
			     struct as10x_ts_filter *filter)
{
	int error;
	struct as10x_cmd_t *pcmd, *prsp;

	ENTER();

	pcmd = adap->cmd;
	prsp = adap->rsp;

	
	as10x_cmd_build(pcmd, (++adap->cmd_xid),
			sizeof(pcmd->body.add_pid_filter.req));

	
	pcmd->body.add_pid_filter.req.proc_id =
		cpu_to_le16(CONTROL_PROC_SETFILTER);
	pcmd->body.add_pid_filter.req.pid = cpu_to_le16(filter->pid);
	pcmd->body.add_pid_filter.req.stream_type = filter->type;

	if (filter->idx < 16)
		pcmd->body.add_pid_filter.req.idx = filter->idx;
	else
		pcmd->body.add_pid_filter.req.idx = 0xFF;

	
	if (adap->ops->xfer_cmd) {
		error = adap->ops->xfer_cmd(adap, (uint8_t *) pcmd,
				sizeof(pcmd->body.add_pid_filter.req)
				+ HEADER_SIZE, (uint8_t *) prsp,
				sizeof(prsp->body.add_pid_filter.rsp)
				+ HEADER_SIZE);
	} else {
		error = AS10X_CMD_ERROR;
	}

	if (error < 0)
		goto out;

	
	error = as10x_rsp_parse(prsp, CONTROL_PROC_SETFILTER_RSP);

	if (error == 0) {
		
		filter->idx = prsp->body.add_pid_filter.rsp.filter_id;
	}

out:
	LEAVE();
	return error;
}

int as10x_cmd_del_PID_filter(struct as10x_bus_adapter_t *adap,
			     uint16_t pid_value)
{
	int error;
	struct as10x_cmd_t *pcmd, *prsp;

	ENTER();

	pcmd = adap->cmd;
	prsp = adap->rsp;

	
	as10x_cmd_build(pcmd, (++adap->cmd_xid),
			sizeof(pcmd->body.del_pid_filter.req));

	
	pcmd->body.del_pid_filter.req.proc_id =
		cpu_to_le16(CONTROL_PROC_REMOVEFILTER);
	pcmd->body.del_pid_filter.req.pid = cpu_to_le16(pid_value);

	
	if (adap->ops->xfer_cmd) {
		error = adap->ops->xfer_cmd(adap, (uint8_t *) pcmd,
				sizeof(pcmd->body.del_pid_filter.req)
				+ HEADER_SIZE, (uint8_t *) prsp,
				sizeof(prsp->body.del_pid_filter.rsp)
				+ HEADER_SIZE);
	} else {
		error = AS10X_CMD_ERROR;
	}

	if (error < 0)
		goto out;

	
	error = as10x_rsp_parse(prsp, CONTROL_PROC_REMOVEFILTER_RSP);

out:
	LEAVE();
	return error;
}

int as10x_cmd_start_streaming(struct as10x_bus_adapter_t *adap)
{
	int error;
	struct as10x_cmd_t *pcmd, *prsp;

	ENTER();

	pcmd = adap->cmd;
	prsp = adap->rsp;

	
	as10x_cmd_build(pcmd, (++adap->cmd_xid),
			sizeof(pcmd->body.start_streaming.req));

	
	pcmd->body.start_streaming.req.proc_id =
		cpu_to_le16(CONTROL_PROC_START_STREAMING);

	
	if (adap->ops->xfer_cmd) {
		error = adap->ops->xfer_cmd(adap, (uint8_t *) pcmd,
				sizeof(pcmd->body.start_streaming.req)
				+ HEADER_SIZE, (uint8_t *) prsp,
				sizeof(prsp->body.start_streaming.rsp)
				+ HEADER_SIZE);
	} else {
		error = AS10X_CMD_ERROR;
	}

	if (error < 0)
		goto out;

	
	error = as10x_rsp_parse(prsp, CONTROL_PROC_START_STREAMING_RSP);

out:
	LEAVE();
	return error;
}

int as10x_cmd_stop_streaming(struct as10x_bus_adapter_t *adap)
{
	int8_t error;
	struct as10x_cmd_t *pcmd, *prsp;

	ENTER();

	pcmd = adap->cmd;
	prsp = adap->rsp;

	
	as10x_cmd_build(pcmd, (++adap->cmd_xid),
			sizeof(pcmd->body.stop_streaming.req));

	
	pcmd->body.stop_streaming.req.proc_id =
		cpu_to_le16(CONTROL_PROC_STOP_STREAMING);

	
	if (adap->ops->xfer_cmd) {
		error = adap->ops->xfer_cmd(adap, (uint8_t *) pcmd,
				sizeof(pcmd->body.stop_streaming.req)
				+ HEADER_SIZE, (uint8_t *) prsp,
				sizeof(prsp->body.stop_streaming.rsp)
				+ HEADER_SIZE);
	} else {
		error = AS10X_CMD_ERROR;
	}

	if (error < 0)
		goto out;

	
	error = as10x_rsp_parse(prsp, CONTROL_PROC_STOP_STREAMING_RSP);

out:
	LEAVE();
	return error;
}
