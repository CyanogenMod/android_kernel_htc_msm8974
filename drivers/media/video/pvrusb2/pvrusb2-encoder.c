/*
 *
 *
 *  Copyright (C) 2005 Mike Isely <isely@pobox.com>
 *  Copyright (C) 2004 Aurelien Alleaume <slts@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/device.h>   
#include <linux/firmware.h>
#include "pvrusb2-util.h"
#include "pvrusb2-encoder.h"
#include "pvrusb2-hdw-internal.h"
#include "pvrusb2-debug.h"
#include "pvrusb2-fx2-cmd.h"



#define IVTV_MBOX_FIRMWARE_DONE 0x00000004
#define IVTV_MBOX_DRIVER_DONE 0x00000002
#define IVTV_MBOX_DRIVER_BUSY 0x00000001

#define MBOX_BASE 0x44


static int pvr2_encoder_write_words(struct pvr2_hdw *hdw,
				    unsigned int offs,
				    const u32 *data, unsigned int dlen)
{
	unsigned int idx,addr;
	unsigned int bAddr;
	int ret;
	unsigned int chunkCnt;

	/*

	Format: First byte must be 0x01.  Remaining 32 bit words are
	spread out into chunks of 7 bytes each, with the first 4 bytes
	being the data word (little endian), and the next 3 bytes
	being the address where that data word is to be written (big
	endian).  Repeat request for additional words, with offset
	adjusted accordingly.

	*/
	while (dlen) {
		chunkCnt = 8;
		if (chunkCnt > dlen) chunkCnt = dlen;
		memset(hdw->cmd_buffer,0,sizeof(hdw->cmd_buffer));
		bAddr = 0;
		hdw->cmd_buffer[bAddr++] = FX2CMD_MEM_WRITE_DWORD;
		for (idx = 0; idx < chunkCnt; idx++) {
			addr = idx + offs;
			hdw->cmd_buffer[bAddr+6] = (addr & 0xffu);
			hdw->cmd_buffer[bAddr+5] = ((addr>>8) & 0xffu);
			hdw->cmd_buffer[bAddr+4] = ((addr>>16) & 0xffu);
			PVR2_DECOMPOSE_LE(hdw->cmd_buffer, bAddr,data[idx]);
			bAddr += 7;
		}
		ret = pvr2_send_request(hdw,
					hdw->cmd_buffer,1+(chunkCnt*7),
					NULL,0);
		if (ret) return ret;
		data += chunkCnt;
		dlen -= chunkCnt;
		offs += chunkCnt;
	}

	return 0;
}


static int pvr2_encoder_read_words(struct pvr2_hdw *hdw,
				   unsigned int offs,
				   u32 *data, unsigned int dlen)
{
	unsigned int idx;
	int ret;
	unsigned int chunkCnt;


	while (dlen) {
		chunkCnt = 16;
		if (chunkCnt > dlen) chunkCnt = dlen;
		if (chunkCnt < 16) chunkCnt = 1;
		hdw->cmd_buffer[0] =
			((chunkCnt == 1) ?
			 FX2CMD_MEM_READ_DWORD : FX2CMD_MEM_READ_64BYTES);
		hdw->cmd_buffer[1] = 0;
		hdw->cmd_buffer[2] = 0;
		hdw->cmd_buffer[3] = 0;
		hdw->cmd_buffer[4] = 0;
		hdw->cmd_buffer[5] = ((offs>>16) & 0xffu);
		hdw->cmd_buffer[6] = ((offs>>8) & 0xffu);
		hdw->cmd_buffer[7] = (offs & 0xffu);
		ret = pvr2_send_request(hdw,
					hdw->cmd_buffer,8,
					hdw->cmd_buffer,
					(chunkCnt == 1 ? 4 : 16 * 4));
		if (ret) return ret;

		for (idx = 0; idx < chunkCnt; idx++) {
			data[idx] = PVR2_COMPOSE_LE(hdw->cmd_buffer,idx*4);
		}
		data += chunkCnt;
		dlen -= chunkCnt;
		offs += chunkCnt;
	}

	return 0;
}


static int pvr2_encoder_cmd(void *ctxt,
			    u32 cmd,
			    int arg_cnt_send,
			    int arg_cnt_recv,
			    u32 *argp)
{
	unsigned int poll_count;
	unsigned int try_count = 0;
	int retry_flag;
	int ret = 0;
	unsigned int idx;
	
	u32 wrData[16];
	u32 rdData[16];
	struct pvr2_hdw *hdw = (struct pvr2_hdw *)ctxt;



	if (arg_cnt_send > (ARRAY_SIZE(wrData) - 4)) {
		pvr2_trace(
			PVR2_TRACE_ERROR_LEGS,
			"Failed to write cx23416 command"
			" - too many input arguments"
			" (was given %u limit %lu)",
			arg_cnt_send, (long unsigned) ARRAY_SIZE(wrData) - 4);
		return -EINVAL;
	}

	if (arg_cnt_recv > (ARRAY_SIZE(rdData) - 4)) {
		pvr2_trace(
			PVR2_TRACE_ERROR_LEGS,
			"Failed to write cx23416 command"
			" - too many return arguments"
			" (was given %u limit %lu)",
			arg_cnt_recv, (long unsigned) ARRAY_SIZE(rdData) - 4);
		return -EINVAL;
	}


	LOCK_TAKE(hdw->ctl_lock); do {

		if (!hdw->state_encoder_ok) {
			ret = -EIO;
			break;
		}

		retry_flag = 0;
		try_count++;
		ret = 0;
		wrData[0] = 0;
		wrData[1] = cmd;
		wrData[2] = 0;
		wrData[3] = 0x00060000;
		for (idx = 0; idx < arg_cnt_send; idx++) {
			wrData[idx+4] = argp[idx];
		}
		for (; idx < ARRAY_SIZE(wrData) - 4; idx++) {
			wrData[idx+4] = 0;
		}

		ret = pvr2_encoder_write_words(hdw,MBOX_BASE,wrData,idx);
		if (ret) break;
		wrData[0] = IVTV_MBOX_DRIVER_DONE|IVTV_MBOX_DRIVER_BUSY;
		ret = pvr2_encoder_write_words(hdw,MBOX_BASE,wrData,1);
		if (ret) break;
		poll_count = 0;
		while (1) {
			poll_count++;
			ret = pvr2_encoder_read_words(hdw,MBOX_BASE,rdData,
						      arg_cnt_recv+4);
			if (ret) {
				break;
			}
			if (rdData[0] & IVTV_MBOX_FIRMWARE_DONE) {
				break;
			}
			if (rdData[0] && (poll_count < 1000)) continue;
			if (!rdData[0]) {
				retry_flag = !0;
				pvr2_trace(
					PVR2_TRACE_ERROR_LEGS,
					"Encoder timed out waiting for us"
					"; arranging to retry");
			} else {
				pvr2_trace(
					PVR2_TRACE_ERROR_LEGS,
					"***WARNING*** device's encoder"
					" appears to be stuck"
					" (status=0x%08x)",rdData[0]);
			}
			pvr2_trace(
				PVR2_TRACE_ERROR_LEGS,
				"Encoder command: 0x%02x",cmd);
			for (idx = 4; idx < arg_cnt_send; idx++) {
				pvr2_trace(
					PVR2_TRACE_ERROR_LEGS,
					"Encoder arg%d: 0x%08x",
					idx-3,wrData[idx]);
			}
			ret = -EBUSY;
			break;
		}
		if (retry_flag) {
			if (try_count < 20) continue;
			pvr2_trace(
				PVR2_TRACE_ERROR_LEGS,
				"Too many retries...");
			ret = -EBUSY;
		}
		if (ret) {
			del_timer_sync(&hdw->encoder_run_timer);
			hdw->state_encoder_ok = 0;
			pvr2_trace(PVR2_TRACE_STBITS,
				   "State bit %s <-- %s",
				   "state_encoder_ok",
				   (hdw->state_encoder_ok ? "true" : "false"));
			if (hdw->state_encoder_runok) {
				hdw->state_encoder_runok = 0;
				pvr2_trace(PVR2_TRACE_STBITS,
				   "State bit %s <-- %s",
					   "state_encoder_runok",
					   (hdw->state_encoder_runok ?
					    "true" : "false"));
			}
			pvr2_trace(
				PVR2_TRACE_ERROR_LEGS,
				"Giving up on command."
				"  This is normally recovered via a firmware"
				" reload and re-initialization; concern"
				" is only warranted if this happens repeatedly"
				" and rapidly.");
			break;
		}
		wrData[0] = 0x7;
		for (idx = 0; idx < arg_cnt_recv; idx++) {
			argp[idx] = rdData[idx+4];
		}

		wrData[0] = 0x0;
		ret = pvr2_encoder_write_words(hdw,MBOX_BASE,wrData,1);
		if (ret) break;

	} while(0); LOCK_GIVE(hdw->ctl_lock);

	return ret;
}


static int pvr2_encoder_vcmd(struct pvr2_hdw *hdw, int cmd,
			     int args, ...)
{
	va_list vl;
	unsigned int idx;
	u32 data[12];

	if (args > ARRAY_SIZE(data)) {
		pvr2_trace(
			PVR2_TRACE_ERROR_LEGS,
			"Failed to write cx23416 command"
			" - too many arguments"
			" (was given %u limit %lu)",
			args, (long unsigned) ARRAY_SIZE(data));
		return -EINVAL;
	}

	va_start(vl, args);
	for (idx = 0; idx < args; idx++) {
		data[idx] = va_arg(vl, u32);
	}
	va_end(vl);

	return pvr2_encoder_cmd(hdw,cmd,args,0,data);
}


static int pvr2_encoder_prep_config(struct pvr2_hdw *hdw)
{
	int ret = 0;
	int encMisc3Arg = 0;

#if 0
	LOCK_TAKE(hdw->ctl_lock); do {
		u32 dat[1];
		dat[0] = 0x80000640;
		pvr2_encoder_write_words(hdw,0x01fe,dat,1);
		pvr2_encoder_write_words(hdw,0x023e,dat,1);
	} while(0); LOCK_GIVE(hdw->ctl_lock);
#endif



#if 0
	ret |= pvr2_encoder_vcmd(hdw, CX2341X_ENC_MISC,4, 5,0,0,0);
#endif

	if (hdw->hdw_desc->flag_has_cx25840) {
		encMisc3Arg = 1;
	} else {
		encMisc3Arg = 0;
	}
	ret |= pvr2_encoder_vcmd(hdw, CX2341X_ENC_MISC,4, 3,
				 encMisc3Arg,0,0);

	ret |= pvr2_encoder_vcmd(hdw, CX2341X_ENC_MISC,4, 8,0,0,0);

#if 0
	ret |= pvr2_encoder_vcmd(hdw, CX2341X_ENC_MISC,4, 4,1,0,0);
#endif

	ret |= pvr2_encoder_vcmd(hdw, CX2341X_ENC_MISC,4, 0,3,0,0);
	ret |= pvr2_encoder_vcmd(hdw, CX2341X_ENC_MISC,4,15,0,0,0);

	ret |= pvr2_encoder_vcmd(hdw, CX2341X_ENC_MISC, 2, 4, 1);

	return ret;
}

int pvr2_encoder_adjust(struct pvr2_hdw *hdw)
{
	int ret;
	ret = cx2341x_update(hdw,pvr2_encoder_cmd,
			     (hdw->enc_cur_valid ? &hdw->enc_cur_state : NULL),
			     &hdw->enc_ctl_state);
	if (ret) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Error from cx2341x module code=%d",ret);
	} else {
		memcpy(&hdw->enc_cur_state,&hdw->enc_ctl_state,
		       sizeof(struct cx2341x_mpeg_params));
		hdw->enc_cur_valid = !0;
	}
	return ret;
}


int pvr2_encoder_configure(struct pvr2_hdw *hdw)
{
	int ret;
	int val;
	pvr2_trace(PVR2_TRACE_ENCODER,"pvr2_encoder_configure"
		   " (cx2341x module)");
	hdw->enc_ctl_state.port = CX2341X_PORT_STREAMING;
	hdw->enc_ctl_state.width = hdw->res_hor_val;
	hdw->enc_ctl_state.height = hdw->res_ver_val;
	hdw->enc_ctl_state.is_50hz = ((hdw->std_mask_cur & V4L2_STD_525_60) ?
				      0 : 1);

	ret = 0;

	ret |= pvr2_encoder_prep_config(hdw);

	
	val = 0xf0;
	if (hdw->hdw_desc->flag_has_cx25840) {
		
		val = 0x140;
	}

	if (!ret) ret = pvr2_encoder_vcmd(
		hdw,CX2341X_ENC_SET_NUM_VSYNC_LINES, 2,
		val, val);

	
	if (!ret) ret = pvr2_encoder_vcmd(
		hdw,CX2341X_ENC_SET_EVENT_NOTIFICATION, 4,
		0, 0, 0x10000000, 0xffffffff);

	if (!ret) ret = pvr2_encoder_vcmd(
		hdw,CX2341X_ENC_SET_VBI_LINE, 5,
		0xffffffff,0,0,0,0);

	if (ret) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Failed to configure cx23416");
		return ret;
	}

	ret = pvr2_encoder_adjust(hdw);
	if (ret) return ret;

	ret = pvr2_encoder_vcmd(
		hdw, CX2341X_ENC_INITIALIZE_INPUT, 0);

	if (ret) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Failed to initialize cx23416 video input");
		return ret;
	}

	return 0;
}


int pvr2_encoder_start(struct pvr2_hdw *hdw)
{
	int status;

	
	pvr2_write_register(hdw, 0x0048, 0xbfffffff);

	pvr2_encoder_vcmd(hdw,CX2341X_ENC_MUTE_VIDEO,1,
			  hdw->input_val == PVR2_CVAL_INPUT_RADIO ? 1 : 0);

	switch (hdw->active_stream_type) {
	case pvr2_config_vbi:
		status = pvr2_encoder_vcmd(hdw,CX2341X_ENC_START_CAPTURE,2,
					   0x01,0x14);
		break;
	case pvr2_config_mpeg:
		status = pvr2_encoder_vcmd(hdw,CX2341X_ENC_START_CAPTURE,2,
					   0,0x13);
		break;
	default: 
		status = pvr2_encoder_vcmd(hdw,CX2341X_ENC_START_CAPTURE,2,
					   0,0x13);
		break;
	}
	return status;
}

int pvr2_encoder_stop(struct pvr2_hdw *hdw)
{
	int status;

	
	pvr2_write_register(hdw, 0x0048, 0xffffffff);

	switch (hdw->active_stream_type) {
	case pvr2_config_vbi:
		status = pvr2_encoder_vcmd(hdw,CX2341X_ENC_STOP_CAPTURE,3,
					   0x01,0x01,0x14);
		break;
	case pvr2_config_mpeg:
		status = pvr2_encoder_vcmd(hdw,CX2341X_ENC_STOP_CAPTURE,3,
					   0x01,0,0x13);
		break;
	default: 
		status = pvr2_encoder_vcmd(hdw,CX2341X_ENC_STOP_CAPTURE,3,
					   0x01,0,0x13);
		break;
	}

	return status;
}


