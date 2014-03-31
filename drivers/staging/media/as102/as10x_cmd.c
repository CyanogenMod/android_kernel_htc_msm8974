/*
 * Abilis Systems Single DVB-T Receiver
 * Copyright (C) 2008 Pierrick Hascoet <pierrick.hascoet@abilis.com>
 * Copyright (C) 2010 Devin Heitmueller <dheitmueller@kernellabs.com>
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
#include "as10x_types.h"
#include "as10x_cmd.h"

int as10x_cmd_turn_on(struct as10x_bus_adapter_t *adap)
{
	int error;
	struct as10x_cmd_t *pcmd, *prsp;

	ENTER();

	pcmd = adap->cmd;
	prsp = adap->rsp;

	
	as10x_cmd_build(pcmd, (++adap->cmd_xid),
			sizeof(pcmd->body.turn_on.req));

	
	pcmd->body.turn_on.req.proc_id = cpu_to_le16(CONTROL_PROC_TURNON);

	
	if (adap->ops->xfer_cmd) {
		error = adap->ops->xfer_cmd(adap, (uint8_t *) pcmd,
					    sizeof(pcmd->body.turn_on.req) +
					    HEADER_SIZE,
					    (uint8_t *) prsp,
					    sizeof(prsp->body.turn_on.rsp) +
					    HEADER_SIZE);
	} else {
		error = AS10X_CMD_ERROR;
	}

	if (error < 0)
		goto out;

	
	error = as10x_rsp_parse(prsp, CONTROL_PROC_TURNON_RSP);

out:
	LEAVE();
	return error;
}

int as10x_cmd_turn_off(struct as10x_bus_adapter_t *adap)
{
	int error;
	struct as10x_cmd_t *pcmd, *prsp;

	ENTER();

	pcmd = adap->cmd;
	prsp = adap->rsp;

	
	as10x_cmd_build(pcmd, (++adap->cmd_xid),
			sizeof(pcmd->body.turn_off.req));

	
	pcmd->body.turn_off.req.proc_id = cpu_to_le16(CONTROL_PROC_TURNOFF);

	
	if (adap->ops->xfer_cmd) {
		error = adap->ops->xfer_cmd(
			adap, (uint8_t *) pcmd,
			sizeof(pcmd->body.turn_off.req) + HEADER_SIZE,
			(uint8_t *) prsp,
			sizeof(prsp->body.turn_off.rsp) + HEADER_SIZE);
	} else {
		error = AS10X_CMD_ERROR;
	}

	if (error < 0)
		goto out;

	
	error = as10x_rsp_parse(prsp, CONTROL_PROC_TURNOFF_RSP);

out:
	LEAVE();
	return error;
}

int as10x_cmd_set_tune(struct as10x_bus_adapter_t *adap,
		       struct as10x_tune_args *ptune)
{
	int error;
	struct as10x_cmd_t *preq, *prsp;

	ENTER();

	preq = adap->cmd;
	prsp = adap->rsp;

	
	as10x_cmd_build(preq, (++adap->cmd_xid),
			sizeof(preq->body.set_tune.req));

	
	preq->body.set_tune.req.proc_id = cpu_to_le16(CONTROL_PROC_SETTUNE);
	preq->body.set_tune.req.args.freq = cpu_to_le32(ptune->freq);
	preq->body.set_tune.req.args.bandwidth = ptune->bandwidth;
	preq->body.set_tune.req.args.hier_select = ptune->hier_select;
	preq->body.set_tune.req.args.modulation = ptune->modulation;
	preq->body.set_tune.req.args.hierarchy = ptune->hierarchy;
	preq->body.set_tune.req.args.interleaving_mode  =
		ptune->interleaving_mode;
	preq->body.set_tune.req.args.code_rate  = ptune->code_rate;
	preq->body.set_tune.req.args.guard_interval = ptune->guard_interval;
	preq->body.set_tune.req.args.transmission_mode  =
		ptune->transmission_mode;

	
	if (adap->ops->xfer_cmd) {
		error = adap->ops->xfer_cmd(adap,
					    (uint8_t *) preq,
					    sizeof(preq->body.set_tune.req)
					    + HEADER_SIZE,
					    (uint8_t *) prsp,
					    sizeof(prsp->body.set_tune.rsp)
					    + HEADER_SIZE);
	} else {
		error = AS10X_CMD_ERROR;
	}

	if (error < 0)
		goto out;

	
	error = as10x_rsp_parse(prsp, CONTROL_PROC_SETTUNE_RSP);

out:
	LEAVE();
	return error;
}

int as10x_cmd_get_tune_status(struct as10x_bus_adapter_t *adap,
			      struct as10x_tune_status *pstatus)
{
	int error;
	struct as10x_cmd_t  *preq, *prsp;

	ENTER();

	preq = adap->cmd;
	prsp = adap->rsp;

	
	as10x_cmd_build(preq, (++adap->cmd_xid),
			sizeof(preq->body.get_tune_status.req));

	
	preq->body.get_tune_status.req.proc_id =
		cpu_to_le16(CONTROL_PROC_GETTUNESTAT);

	
	if (adap->ops->xfer_cmd) {
		error = adap->ops->xfer_cmd(
			adap,
			(uint8_t *) preq,
			sizeof(preq->body.get_tune_status.req) + HEADER_SIZE,
			(uint8_t *) prsp,
			sizeof(prsp->body.get_tune_status.rsp) + HEADER_SIZE);
	} else {
		error = AS10X_CMD_ERROR;
	}

	if (error < 0)
		goto out;

	
	error = as10x_rsp_parse(prsp, CONTROL_PROC_GETTUNESTAT_RSP);
	if (error < 0)
		goto out;

	
	pstatus->tune_state = prsp->body.get_tune_status.rsp.sts.tune_state;
	pstatus->signal_strength  =
		le16_to_cpu(prsp->body.get_tune_status.rsp.sts.signal_strength);
	pstatus->PER = le16_to_cpu(prsp->body.get_tune_status.rsp.sts.PER);
	pstatus->BER = le16_to_cpu(prsp->body.get_tune_status.rsp.sts.BER);

out:
	LEAVE();
	return error;
}

int as10x_cmd_get_tps(struct as10x_bus_adapter_t *adap, struct as10x_tps *ptps)
{
	int error;
	struct as10x_cmd_t *pcmd, *prsp;

	ENTER();

	pcmd = adap->cmd;
	prsp = adap->rsp;

	
	as10x_cmd_build(pcmd, (++adap->cmd_xid),
			sizeof(pcmd->body.get_tps.req));

	
	pcmd->body.get_tune_status.req.proc_id =
		cpu_to_le16(CONTROL_PROC_GETTPS);

	
	if (adap->ops->xfer_cmd) {
		error = adap->ops->xfer_cmd(adap,
					    (uint8_t *) pcmd,
					    sizeof(pcmd->body.get_tps.req) +
					    HEADER_SIZE,
					    (uint8_t *) prsp,
					    sizeof(prsp->body.get_tps.rsp) +
					    HEADER_SIZE);
	} else {
		error = AS10X_CMD_ERROR;
	}

	if (error < 0)
		goto out;

	
	error = as10x_rsp_parse(prsp, CONTROL_PROC_GETTPS_RSP);
	if (error < 0)
		goto out;

	
	ptps->modulation = prsp->body.get_tps.rsp.tps.modulation;
	ptps->hierarchy = prsp->body.get_tps.rsp.tps.hierarchy;
	ptps->interleaving_mode = prsp->body.get_tps.rsp.tps.interleaving_mode;
	ptps->code_rate_HP = prsp->body.get_tps.rsp.tps.code_rate_HP;
	ptps->code_rate_LP = prsp->body.get_tps.rsp.tps.code_rate_LP;
	ptps->guard_interval = prsp->body.get_tps.rsp.tps.guard_interval;
	ptps->transmission_mode  = prsp->body.get_tps.rsp.tps.transmission_mode;
	ptps->DVBH_mask_HP = prsp->body.get_tps.rsp.tps.DVBH_mask_HP;
	ptps->DVBH_mask_LP = prsp->body.get_tps.rsp.tps.DVBH_mask_LP;
	ptps->cell_ID = le16_to_cpu(prsp->body.get_tps.rsp.tps.cell_ID);

out:
	LEAVE();
	return error;
}

int as10x_cmd_get_demod_stats(struct as10x_bus_adapter_t *adap,
			      struct as10x_demod_stats *pdemod_stats)
{
	int error;
	struct as10x_cmd_t *pcmd, *prsp;

	ENTER();

	pcmd = adap->cmd;
	prsp = adap->rsp;

	
	as10x_cmd_build(pcmd, (++adap->cmd_xid),
			sizeof(pcmd->body.get_demod_stats.req));

	
	pcmd->body.get_demod_stats.req.proc_id =
		cpu_to_le16(CONTROL_PROC_GET_DEMOD_STATS);

	
	if (adap->ops->xfer_cmd) {
		error = adap->ops->xfer_cmd(adap,
				(uint8_t *) pcmd,
				sizeof(pcmd->body.get_demod_stats.req)
				+ HEADER_SIZE,
				(uint8_t *) prsp,
				sizeof(prsp->body.get_demod_stats.rsp)
				+ HEADER_SIZE);
	} else {
		error = AS10X_CMD_ERROR;
	}

	if (error < 0)
		goto out;

	
	error = as10x_rsp_parse(prsp, CONTROL_PROC_GET_DEMOD_STATS_RSP);
	if (error < 0)
		goto out;

	
	pdemod_stats->frame_count =
		le32_to_cpu(prsp->body.get_demod_stats.rsp.stats.frame_count);
	pdemod_stats->bad_frame_count =
		le32_to_cpu(prsp->body.get_demod_stats.rsp.stats.bad_frame_count);
	pdemod_stats->bytes_fixed_by_rs =
		le32_to_cpu(prsp->body.get_demod_stats.rsp.stats.bytes_fixed_by_rs);
	pdemod_stats->mer =
		le16_to_cpu(prsp->body.get_demod_stats.rsp.stats.mer);
	pdemod_stats->has_started =
		prsp->body.get_demod_stats.rsp.stats.has_started;

out:
	LEAVE();
	return error;
}

int as10x_cmd_get_impulse_resp(struct as10x_bus_adapter_t *adap,
			       uint8_t *is_ready)
{
	int error;
	struct as10x_cmd_t *pcmd, *prsp;

	ENTER();

	pcmd = adap->cmd;
	prsp = adap->rsp;

	
	as10x_cmd_build(pcmd, (++adap->cmd_xid),
			sizeof(pcmd->body.get_impulse_rsp.req));

	
	pcmd->body.get_impulse_rsp.req.proc_id =
		cpu_to_le16(CONTROL_PROC_GET_IMPULSE_RESP);

	
	if (adap->ops->xfer_cmd) {
		error = adap->ops->xfer_cmd(adap,
					(uint8_t *) pcmd,
					sizeof(pcmd->body.get_impulse_rsp.req)
					+ HEADER_SIZE,
					(uint8_t *) prsp,
					sizeof(prsp->body.get_impulse_rsp.rsp)
					+ HEADER_SIZE);
	} else {
		error = AS10X_CMD_ERROR;
	}

	if (error < 0)
		goto out;

	
	error = as10x_rsp_parse(prsp, CONTROL_PROC_GET_IMPULSE_RESP_RSP);
	if (error < 0)
		goto out;

	
	*is_ready = prsp->body.get_impulse_rsp.rsp.is_ready;

out:
	LEAVE();
	return error;
}

void as10x_cmd_build(struct as10x_cmd_t *pcmd,
		     uint16_t xid, uint16_t cmd_len)
{
	pcmd->header.req_id = cpu_to_le16(xid);
	pcmd->header.prog = cpu_to_le16(SERVICE_PROG_ID);
	pcmd->header.version = cpu_to_le16(SERVICE_PROG_VERSION);
	pcmd->header.data_len = cpu_to_le16(cmd_len);
}

int as10x_rsp_parse(struct as10x_cmd_t *prsp, uint16_t proc_id)
{
	int error;

	
	error = prsp->body.common.rsp.error;

	if ((error == 0) &&
	    (le16_to_cpu(prsp->body.common.rsp.proc_id) == proc_id)) {
		return 0;
	}

	return AS10X_CMD_ERROR;
}
