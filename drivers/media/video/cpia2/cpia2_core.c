/****************************************************************************
 *
 *  Filename: cpia2_core.c
 *
 *  Copyright 2001, STMicrolectronics, Inc.
 *      Contact:  steve.miller@st.com
 *
 *  Description:
 *     This is a USB driver for CPia2 based video cameras.
 *     The infrastructure of this driver is based on the cpia usb driver by
 *     Jochen Scharrlach and Johannes Erdfeldt.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Stripped of 2.4 stuff ready for main kernel submit by
 *		Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 ****************************************************************************/

#include "cpia2.h"

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>


#ifdef _CPIA2_DEBUG_

static const char *block_name[] = {
	"System",
	"VC",
	"VP",
	"IDATA"
};
#endif

static unsigned int debugs_on;	


static int apply_vp_patch(struct camera_data *cam);
static int set_default_user_mode(struct camera_data *cam);
static int set_vw_size(struct camera_data *cam, int size);
static int configure_sensor(struct camera_data *cam,
			    int reqwidth, int reqheight);
static int config_sensor_410(struct camera_data *cam,
			    int reqwidth, int reqheight);
static int config_sensor_500(struct camera_data *cam,
			    int reqwidth, int reqheight);
static int set_all_properties(struct camera_data *cam);
static void get_color_params(struct camera_data *cam);
static void wake_system(struct camera_data *cam);
static void set_lowlight_boost(struct camera_data *cam);
static void reset_camera_struct(struct camera_data *cam);
static int cpia2_set_high_power(struct camera_data *cam);

static inline unsigned long kvirt_to_pa(unsigned long adr)
{
	unsigned long kva, ret;

	kva = (unsigned long) page_address(vmalloc_to_page((void *)adr));
	kva |= adr & (PAGE_SIZE-1); 
	ret = __pa(kva);
	return ret;
}

static void *rvmalloc(unsigned long size)
{
	void *mem;
	unsigned long adr;

	
	size = PAGE_ALIGN(size);

	mem = vmalloc_32(size);
	if (!mem)
		return NULL;

	memset(mem, 0, size);	
	adr = (unsigned long) mem;

	while ((long)size > 0) {
		SetPageReserved(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	return mem;
}

static void rvfree(void *mem, unsigned long size)
{
	unsigned long adr;

	if (!mem)
		return;

	size = PAGE_ALIGN(size);

	adr = (unsigned long) mem;
	while ((long)size > 0) {
		ClearPageReserved(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	vfree(mem);
}

int cpia2_do_command(struct camera_data *cam,
		     u32 command, u8 direction, u8 param)
{
	int retval = 0;
	struct cpia2_command cmd;
	unsigned int device = cam->params.pnp_id.device_type;

	cmd.command = command;
	cmd.reg_count = 2;	
	cmd.direction = direction;

	switch (command) {
	case CPIA2_CMD_GET_VERSION:
		cmd.req_mode =
		    CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_SYSTEM;
		cmd.start = CPIA2_SYSTEM_DEVICE_HI;
		break;
	case CPIA2_CMD_GET_PNP_ID:
		cmd.req_mode =
		    CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_SYSTEM;
		cmd.reg_count = 8;
		cmd.start = CPIA2_SYSTEM_DESCRIP_VID_HI;
		break;
	case CPIA2_CMD_GET_ASIC_TYPE:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VC;
		cmd.start = CPIA2_VC_ASIC_ID;
		break;
	case CPIA2_CMD_GET_SENSOR:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.start = CPIA2_VP_SENSOR_FLAGS;
		break;
	case CPIA2_CMD_GET_VP_DEVICE:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.start = CPIA2_VP_DEVICEH;
		break;
	case CPIA2_CMD_SET_VP_BRIGHTNESS:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_VP_BRIGHTNESS:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		if (device == DEVICE_STV_672)
			cmd.start = CPIA2_VP4_EXPOSURE_TARGET;
		else
			cmd.start = CPIA2_VP5_EXPOSURE_TARGET;
		break;
	case CPIA2_CMD_SET_CONTRAST:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_CONTRAST:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VP_YRANGE;
		break;
	case CPIA2_CMD_SET_VP_SATURATION:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_VP_SATURATION:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		if (device == DEVICE_STV_672)
			cmd.start = CPIA2_VP_SATURATION;
		else
			cmd.start = CPIA2_VP5_MCUVSATURATION;
		break;
	case CPIA2_CMD_SET_VP_GPIO_DATA:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_VP_GPIO_DATA:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VP_GPIO_DATA;
		break;
	case CPIA2_CMD_SET_VP_GPIO_DIRECTION:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_VP_GPIO_DIRECTION:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VP_GPIO_DIRECTION;
		break;
	case CPIA2_CMD_SET_VC_MP_GPIO_DATA:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_VC_MP_GPIO_DATA:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VC;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VC_MP_DATA;
		break;
	case CPIA2_CMD_SET_VC_MP_GPIO_DIRECTION:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_VC_MP_GPIO_DIRECTION:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VC;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VC_MP_DIR;
		break;
	case CPIA2_CMD_ENABLE_PACKET_CTRL:
		cmd.req_mode =
		    CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_SYSTEM;
		cmd.start = CPIA2_SYSTEM_INT_PACKET_CTRL;
		cmd.reg_count = 1;
		cmd.buffer.block_data[0] = param;
		break;
	case CPIA2_CMD_SET_FLICKER_MODES:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_FLICKER_MODES:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VP_FLICKER_MODES;
		break;
	case CPIA2_CMD_RESET_FIFO:	
		cmd.req_mode = CAMERAACCESS_TYPE_RANDOM | CAMERAACCESS_VC;
		cmd.reg_count = 2;
		cmd.start = 0;
		cmd.buffer.registers[0].index = CPIA2_VC_ST_CTRL;
		cmd.buffer.registers[0].value = CPIA2_VC_ST_CTRL_SRC_VC |
		    CPIA2_VC_ST_CTRL_DST_USB | CPIA2_VC_ST_CTRL_EOF_DETECT;
		cmd.buffer.registers[1].index = CPIA2_VC_ST_CTRL;
		cmd.buffer.registers[1].value = CPIA2_VC_ST_CTRL_SRC_VC |
		    CPIA2_VC_ST_CTRL_DST_USB |
		    CPIA2_VC_ST_CTRL_EOF_DETECT |
		    CPIA2_VC_ST_CTRL_FIFO_ENABLE;
		break;
	case CPIA2_CMD_SET_HI_POWER:
		cmd.req_mode =
		    CAMERAACCESS_TYPE_RANDOM | CAMERAACCESS_SYSTEM;
		cmd.reg_count = 2;
		cmd.buffer.registers[0].index =
		    CPIA2_SYSTEM_SYSTEM_CONTROL;
		cmd.buffer.registers[1].index =
		    CPIA2_SYSTEM_SYSTEM_CONTROL;
		cmd.buffer.registers[0].value = CPIA2_SYSTEM_CONTROL_CLEAR_ERR;
		cmd.buffer.registers[1].value =
		    CPIA2_SYSTEM_CONTROL_HIGH_POWER;
		break;
	case CPIA2_CMD_SET_LOW_POWER:
		cmd.req_mode =
		    CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_SYSTEM;
		cmd.reg_count = 1;
		cmd.start = CPIA2_SYSTEM_SYSTEM_CONTROL;
		cmd.buffer.block_data[0] = 0;
		break;
	case CPIA2_CMD_CLEAR_V2W_ERR:
		cmd.req_mode =
		    CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_SYSTEM;
		cmd.reg_count = 1;
		cmd.start = CPIA2_SYSTEM_SYSTEM_CONTROL;
		cmd.buffer.block_data[0] = CPIA2_SYSTEM_CONTROL_CLEAR_ERR;
		break;
	case CPIA2_CMD_SET_USER_MODE:   
		cmd.buffer.block_data[0] = param;
	case CPIA2_CMD_GET_USER_MODE:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		if (device == DEVICE_STV_672)
			cmd.start = CPIA2_VP4_USER_MODE;
		else
			cmd.start = CPIA2_VP5_USER_MODE;
		break;
	case CPIA2_CMD_FRAMERATE_REQ:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		if (device == DEVICE_STV_672)
			cmd.start = CPIA2_VP4_FRAMERATE_REQUEST;
		else
			cmd.start = CPIA2_VP5_FRAMERATE_REQUEST;
		cmd.buffer.block_data[0] = param;
		break;
	case CPIA2_CMD_SET_WAKEUP:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_WAKEUP:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VC;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VC_WAKEUP;
		break;
	case CPIA2_CMD_SET_PW_CONTROL:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_PW_CONTROL:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VC;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VC_PW_CTRL;
		break;
	case CPIA2_CMD_GET_VP_SYSTEM_STATE:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VP_SYSTEMSTATE;
		break;
	case CPIA2_CMD_SET_SYSTEM_CTRL:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_SYSTEM_CTRL:
		cmd.req_mode =
		    CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_SYSTEM;
		cmd.reg_count = 1;
		cmd.start = CPIA2_SYSTEM_SYSTEM_CONTROL;
		break;
	case CPIA2_CMD_SET_VP_SYSTEM_CTRL:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_VP_SYSTEM_CTRL:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VP_SYSTEMCTRL;
		break;
	case CPIA2_CMD_SET_VP_EXP_MODES:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_VP_EXP_MODES:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VP_EXPOSURE_MODES;
		break;
	case CPIA2_CMD_SET_DEVICE_CONFIG:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_DEVICE_CONFIG:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VP_DEVICE_CONFIG;
		break;
	case CPIA2_CMD_SET_SERIAL_ADDR:
		cmd.buffer.block_data[0] = param;
		cmd.req_mode =
		    CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_SYSTEM;
		cmd.reg_count = 1;
		cmd.start = CPIA2_SYSTEM_VP_SERIAL_ADDR;
		break;
	case CPIA2_CMD_SET_SENSOR_CR1:
		cmd.buffer.block_data[0] = param;
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		cmd.start = CPIA2_SENSOR_CR1;
		break;
	case CPIA2_CMD_SET_VC_CONTROL:
		cmd.buffer.block_data[0] = param;	
	case CPIA2_CMD_GET_VC_CONTROL:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VC;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VC_VC_CTRL;
		break;
	case CPIA2_CMD_SET_TARGET_KB:
		cmd.req_mode = CAMERAACCESS_TYPE_RANDOM | CAMERAACCESS_VC;
		cmd.reg_count = 1;
		cmd.buffer.registers[0].index = CPIA2_VC_VC_TARGET_KB;
		cmd.buffer.registers[0].value = param;
		break;
	case CPIA2_CMD_SET_DEF_JPEG_OPT:
		cmd.req_mode = CAMERAACCESS_TYPE_RANDOM | CAMERAACCESS_VC;
		cmd.reg_count = 4;
		cmd.buffer.registers[0].index = CPIA2_VC_VC_JPEG_OPT;
		cmd.buffer.registers[0].value =
		    CPIA2_VC_VC_JPEG_OPT_DOUBLE_SQUEEZE;
		cmd.buffer.registers[1].index = CPIA2_VC_VC_USER_SQUEEZE;
		cmd.buffer.registers[1].value = 20;
		cmd.buffer.registers[2].index = CPIA2_VC_VC_CREEP_PERIOD;
		cmd.buffer.registers[2].value = 2;
		cmd.buffer.registers[3].index = CPIA2_VC_VC_JPEG_OPT;
		cmd.buffer.registers[3].value = CPIA2_VC_VC_JPEG_OPT_DEFAULT;
		break;
	case CPIA2_CMD_REHASH_VP4:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		cmd.start = CPIA2_VP_REHASH_VALUES;
		cmd.buffer.block_data[0] = param;
		break;
	case CPIA2_CMD_SET_USER_EFFECTS:  
		cmd.buffer.block_data[0] = param;      
	case CPIA2_CMD_GET_USER_EFFECTS:
		cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
		cmd.reg_count = 1;
		if (device == DEVICE_STV_672)
			cmd.start = CPIA2_VP4_USER_EFFECTS;
		else
			cmd.start = CPIA2_VP5_USER_EFFECTS;
		break;
	default:
		LOG("DoCommand received invalid command\n");
		return -EINVAL;
	}

	retval = cpia2_send_command(cam, &cmd);
	if (retval) {
		return retval;
	}

	switch (command) {
	case CPIA2_CMD_GET_VERSION:
		cam->params.version.firmware_revision_hi =
		    cmd.buffer.block_data[0];
		cam->params.version.firmware_revision_lo =
		    cmd.buffer.block_data[1];
		break;
	case CPIA2_CMD_GET_PNP_ID:
		cam->params.pnp_id.vendor = (cmd.buffer.block_data[0] << 8) |
					    cmd.buffer.block_data[1];
		cam->params.pnp_id.product = (cmd.buffer.block_data[2] << 8) |
					     cmd.buffer.block_data[3];
		cam->params.pnp_id.device_revision =
			(cmd.buffer.block_data[4] << 8) |
			cmd.buffer.block_data[5];
		if (cam->params.pnp_id.vendor == 0x553) {
			if (cam->params.pnp_id.product == 0x100) {
				cam->params.pnp_id.device_type = DEVICE_STV_672;
			} else if (cam->params.pnp_id.product == 0x140 ||
				   cam->params.pnp_id.product == 0x151) {
				cam->params.pnp_id.device_type = DEVICE_STV_676;
			}
		}
		break;
	case CPIA2_CMD_GET_ASIC_TYPE:
		cam->params.version.asic_id = cmd.buffer.block_data[0];
		cam->params.version.asic_rev = cmd.buffer.block_data[1];
		break;
	case CPIA2_CMD_GET_SENSOR:
		cam->params.version.sensor_flags = cmd.buffer.block_data[0];
		cam->params.version.sensor_rev = cmd.buffer.block_data[1];
		break;
	case CPIA2_CMD_GET_VP_DEVICE:
		cam->params.version.vp_device_hi = cmd.buffer.block_data[0];
		cam->params.version.vp_device_lo = cmd.buffer.block_data[1];
		break;
	case CPIA2_CMD_GET_VP_BRIGHTNESS:
		cam->params.color_params.brightness = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_CONTRAST:
		cam->params.color_params.contrast = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_VP_SATURATION:
		cam->params.color_params.saturation = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_VP_GPIO_DATA:
		cam->params.vp_params.gpio_data = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_VP_GPIO_DIRECTION:
		cam->params.vp_params.gpio_direction = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_VC_MP_GPIO_DIRECTION:
		cam->params.vc_params.vc_mp_direction =cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_VC_MP_GPIO_DATA:
		cam->params.vc_params.vc_mp_data = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_FLICKER_MODES:
		cam->params.flicker_control.cam_register =
			cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_WAKEUP:
		cam->params.vc_params.wakeup = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_PW_CONTROL:
		cam->params.vc_params.pw_control = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_SYSTEM_CTRL:
		cam->params.camera_state.system_ctrl = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_VP_SYSTEM_STATE:
		cam->params.vp_params.system_state = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_VP_SYSTEM_CTRL:
		cam->params.vp_params.system_ctrl = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_VP_EXP_MODES:
		cam->params.vp_params.exposure_modes = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_DEVICE_CONFIG:
		cam->params.vp_params.device_config = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_VC_CONTROL:
		cam->params.vc_params.vc_control = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_USER_MODE:
		cam->params.vp_params.video_mode = cmd.buffer.block_data[0];
		break;
	case CPIA2_CMD_GET_USER_EFFECTS:
		cam->params.vp_params.user_effects = cmd.buffer.block_data[0];
		break;
	default:
		break;
	}
	return retval;
}


#define DIR(cmd) ((cmd->direction == TRANSFER_WRITE) ? "Write" : "Read")
#define BINDEX(cmd) (cmd->req_mode & 0x03)

int cpia2_send_command(struct camera_data *cam, struct cpia2_command *cmd)
{
	u8 count;
	u8 start;
	u8 *buffer;
	int retval;

	switch (cmd->req_mode & 0x0c) {
	case CAMERAACCESS_TYPE_RANDOM:
		count = cmd->reg_count * sizeof(struct cpia2_register);
		start = 0;
		buffer = (u8 *) & cmd->buffer;
		if (debugs_on & DEBUG_REG)
			DBG("%s Random: Register block %s\n", DIR(cmd),
			    block_name[BINDEX(cmd)]);
		break;
	case CAMERAACCESS_TYPE_BLOCK:
		count = cmd->reg_count;
		start = cmd->start;
		buffer = cmd->buffer.block_data;
		if (debugs_on & DEBUG_REG)
			DBG("%s Block: Register block %s\n", DIR(cmd),
			    block_name[BINDEX(cmd)]);
		break;
	case CAMERAACCESS_TYPE_MASK:
		count = cmd->reg_count * sizeof(struct cpia2_reg_mask);
		start = 0;
		buffer = (u8 *) & cmd->buffer;
		if (debugs_on & DEBUG_REG)
			DBG("%s Mask: Register block %s\n", DIR(cmd),
			    block_name[BINDEX(cmd)]);
		break;
	case CAMERAACCESS_TYPE_REPEAT:	
		count = cmd->reg_count;
		start = cmd->start;
		buffer = cmd->buffer.block_data;
		if (debugs_on & DEBUG_REG)
			DBG("%s Repeat: Register block %s\n", DIR(cmd),
			    block_name[BINDEX(cmd)]);
		break;
	default:
		LOG("%s: invalid request mode\n",__func__);
		return -EINVAL;
	}

	retval = cpia2_usb_transfer_cmd(cam,
					buffer,
					cmd->req_mode,
					start, count, cmd->direction);
#ifdef _CPIA2_DEBUG_
	if (debugs_on & DEBUG_REG) {
		int i;
		for (i = 0; i < cmd->reg_count; i++) {
			if((cmd->req_mode & 0x0c) == CAMERAACCESS_TYPE_BLOCK)
				KINFO("%s Block: [0x%02X] = 0x%02X\n",
				    DIR(cmd), start + i, buffer[i]);
			if((cmd->req_mode & 0x0c) == CAMERAACCESS_TYPE_RANDOM)
				KINFO("%s Random: [0x%02X] = 0x%02X\n",
				    DIR(cmd), cmd->buffer.registers[i].index,
				    cmd->buffer.registers[i].value);
		}
	}
#endif

	return retval;
};

static void cpia2_get_version_info(struct camera_data *cam)
{
	cpia2_do_command(cam, CPIA2_CMD_GET_VERSION, TRANSFER_READ, 0);
	cpia2_do_command(cam, CPIA2_CMD_GET_PNP_ID, TRANSFER_READ, 0);
	cpia2_do_command(cam, CPIA2_CMD_GET_ASIC_TYPE, TRANSFER_READ, 0);
	cpia2_do_command(cam, CPIA2_CMD_GET_SENSOR, TRANSFER_READ, 0);
	cpia2_do_command(cam, CPIA2_CMD_GET_VP_DEVICE, TRANSFER_READ, 0);
}

int cpia2_reset_camera(struct camera_data *cam)
{
	u8 tmp_reg;
	int retval = 0;
	int i;
	struct cpia2_command cmd;

	retval = configure_sensor(cam,
				  cam->params.roi.width,
				  cam->params.roi.height);
	if (retval < 0) {
		ERR("Couldn't configure sensor, error=%d\n", retval);
		return retval;
	}

	
	cmd.req_mode = CAMERAACCESS_TYPE_RANDOM | CAMERAACCESS_VC;
	cmd.direction = TRANSFER_WRITE;
	cmd.reg_count = 2;
	cmd.buffer.registers[0].index = CPIA2_VC_ST_CTRL;
	cmd.buffer.registers[0].value = CPIA2_VC_ST_CTRL_SRC_VC |
		CPIA2_VC_ST_CTRL_DST_USB | CPIA2_VC_ST_CTRL_EOF_DETECT;
	cmd.buffer.registers[1].index = CPIA2_VC_ST_CTRL;
	cmd.buffer.registers[1].value = CPIA2_VC_ST_CTRL_SRC_VC |
		CPIA2_VC_ST_CTRL_DST_USB |
		CPIA2_VC_ST_CTRL_EOF_DETECT | CPIA2_VC_ST_CTRL_FIFO_ENABLE;

	cpia2_send_command(cam, &cmd);

	cpia2_set_high_power(cam);

	if (cam->params.pnp_id.device_type == DEVICE_STV_672) {
		
		cmd.req_mode = CAMERAACCESS_TYPE_RANDOM | CAMERAACCESS_SYSTEM;
		cmd.buffer.registers[0].index = CPIA2_SYSTEM_INT_PACKET_CTRL;
		cmd.buffer.registers[0].value =
			CPIA2_SYSTEM_INT_PACKET_CTRL_ENABLE_SW_XX;
		cmd.reg_count = 1;
		cpia2_send_command(cam, &cmd);
	}

	schedule_timeout_interruptible(msecs_to_jiffies(100));

	if (cam->params.pnp_id.device_type == DEVICE_STV_672)
		retval = apply_vp_patch(cam);

	
	schedule_timeout_interruptible(msecs_to_jiffies(100));

	if (cam->params.pnp_id.device_type == DEVICE_STV_676) {
		cmd.req_mode = CAMERAACCESS_TYPE_RANDOM | CAMERAACCESS_VP;

		
		cmd.buffer.registers[0].index = CPIA2_VP5_MYBLACK_LEVEL;
		cmd.buffer.registers[0].value = 0; 
		cmd.buffer.registers[1].index = CPIA2_VP5_MCYRANGE;
		cmd.buffer.registers[1].value = 0x92; 
		cmd.buffer.registers[2].index = CPIA2_VP5_MYCEILING;
		cmd.buffer.registers[2].value = 0xFF; 
		cmd.buffer.registers[3].index = CPIA2_VP5_MCUVSATURATION;
		cmd.buffer.registers[3].value = 0xFF; 
		cmd.buffer.registers[4].index = CPIA2_VP5_ANTIFLKRSETUP;
		cmd.buffer.registers[4].value = 0x80;  

		
		cmd.buffer.registers[5].index = CPIA2_VP_RAM_ADDR_H;
		cmd.buffer.registers[5].value = 0x01;
		cmd.buffer.registers[6].index = CPIA2_VP_RAM_ADDR_L;
		cmd.buffer.registers[6].value = 0xE3;
		cmd.buffer.registers[7].index = CPIA2_VP_RAM_DATA;
		cmd.buffer.registers[7].value = 0x02;
		cmd.buffer.registers[8].index = CPIA2_VP_RAM_DATA;
		cmd.buffer.registers[8].value = 0xFC;

		cmd.direction = TRANSFER_WRITE;
		cmd.reg_count = 9;

		cpia2_send_command(cam, &cmd);
	}

	
	
	set_default_user_mode(cam);

	
	schedule_timeout_interruptible(msecs_to_jiffies(100));

	set_all_properties(cam);

	cpia2_do_command(cam, CPIA2_CMD_GET_USER_MODE, TRANSFER_READ, 0);
	DBG("After SetAllProperties(cam), user mode is 0x%0X\n",
	    cam->params.vp_params.video_mode);

	
	cpia2_do_command(cam, CPIA2_CMD_GET_VP_SYSTEM_CTRL, TRANSFER_READ, 0);
	tmp_reg = cam->params.vp_params.system_ctrl;
	cmd.buffer.registers[0].value = tmp_reg &
		(tmp_reg & (CPIA2_VP_SYSTEMCTRL_HK_CONTROL ^ 0xFF));

	cpia2_do_command(cam, CPIA2_CMD_GET_DEVICE_CONFIG, TRANSFER_READ, 0);
	cmd.buffer.registers[1].value = cam->params.vp_params.device_config |
					CPIA2_VP_DEVICE_CONFIG_SERIAL_BRIDGE;
	cmd.buffer.registers[0].index = CPIA2_VP_SYSTEMCTRL;
	cmd.buffer.registers[1].index = CPIA2_VP_DEVICE_CONFIG;
	cmd.req_mode = CAMERAACCESS_TYPE_RANDOM | CAMERAACCESS_VP;
	cmd.reg_count = 2;
	cmd.direction = TRANSFER_WRITE;
	cmd.start = 0;
	cpia2_send_command(cam, &cmd);

	
	cpia2_do_command(cam,
			 CPIA2_CMD_SET_SERIAL_ADDR,
			 TRANSFER_WRITE,
			 CPIA2_SYSTEM_VP_SERIAL_ADDR_SENSOR);

	
	cpia2_do_command(cam,
			 CPIA2_CMD_SET_SENSOR_CR1,
			 TRANSFER_WRITE, CPIA2_SENSOR_CR1_DOWN_AUDIO_REGULATOR);

	
	if (cam->params.pnp_id.device_type == DEVICE_STV_672)
		cpia2_do_command(cam,
				 CPIA2_CMD_SET_SERIAL_ADDR,
				 TRANSFER_WRITE,
				 CPIA2_SYSTEM_VP_SERIAL_ADDR_VP); 
	else
		cpia2_do_command(cam,
				 CPIA2_CMD_SET_SERIAL_ADDR,
				 TRANSFER_WRITE,
				 CPIA2_SYSTEM_VP_SERIAL_ADDR_676_VP); 

	
	if (cam->params.pnp_id.device_type == DEVICE_STV_676)
		cpia2_do_command(cam,
				 CPIA2_CMD_SET_VP_EXP_MODES,
				 TRANSFER_WRITE,
				 CPIA2_VP_EXPOSURE_MODES_COMPILE_EXP);

	
	cpia2_do_command(cam, CPIA2_CMD_GET_DEVICE_CONFIG, TRANSFER_READ, 0);
	cmd.buffer.registers[0].value = cam->params.vp_params.device_config &
				  (CPIA2_VP_DEVICE_CONFIG_SERIAL_BRIDGE ^ 0xFF);

	cpia2_do_command(cam, CPIA2_CMD_GET_VP_SYSTEM_CTRL, TRANSFER_READ, 0);
	cmd.buffer.registers[1].value =
	    cam->params.vp_params.system_ctrl | CPIA2_VP_SYSTEMCTRL_HK_CONTROL;

	cmd.buffer.registers[0].index = CPIA2_VP_DEVICE_CONFIG;
	cmd.buffer.registers[1].index = CPIA2_VP_SYSTEMCTRL;
	cmd.req_mode = CAMERAACCESS_TYPE_RANDOM | CAMERAACCESS_VP;
	cmd.reg_count = 2;
	cmd.direction = TRANSFER_WRITE;

	cpia2_send_command(cam, &cmd);

	
	cpia2_do_command(cam, CPIA2_CMD_GET_VC_CONTROL, TRANSFER_READ, 0);
	if (cam->params.compression.inhibit_htables) {
		tmp_reg = cam->params.vc_params.vc_control |
			  CPIA2_VC_VC_CTRL_INHIBIT_H_TABLES;
	} else  {
		tmp_reg = cam->params.vc_params.vc_control &
			  ~CPIA2_VC_VC_CTRL_INHIBIT_H_TABLES;
	}
	cpia2_do_command(cam, CPIA2_CMD_SET_VC_CONTROL, TRANSFER_WRITE,tmp_reg);

	
	cpia2_do_command(cam, CPIA2_CMD_SET_TARGET_KB,
			 TRANSFER_WRITE, cam->params.vc_params.target_kb);

	
	for (i = 0; i < 50; i++) {
		cpia2_do_command(cam, CPIA2_CMD_GET_PW_CONTROL,
				 TRANSFER_READ, 0);
	}

	tmp_reg = cam->params.vc_params.pw_control;
	tmp_reg &= ~CPIA2_VC_PW_CTRL_VC_RESET_N;

	cpia2_do_command(cam, CPIA2_CMD_SET_PW_CONTROL, TRANSFER_WRITE,tmp_reg);

	tmp_reg |= CPIA2_VC_PW_CTRL_VC_RESET_N;
	cpia2_do_command(cam, CPIA2_CMD_SET_PW_CONTROL, TRANSFER_WRITE,tmp_reg);

	cpia2_do_command(cam, CPIA2_CMD_SET_DEF_JPEG_OPT, TRANSFER_WRITE, 0);

	cpia2_do_command(cam, CPIA2_CMD_GET_USER_MODE, TRANSFER_READ, 0);
	DBG("After VC RESET, user mode is 0x%0X\n",
	    cam->params.vp_params.video_mode);

	return retval;
}

static int cpia2_set_high_power(struct camera_data *cam)
{
	int i;
	for (i = 0; i <= 50; i++) {
		
		cpia2_do_command(cam,CPIA2_CMD_GET_SYSTEM_CTRL,TRANSFER_READ,0);

		
		if(cam->params.camera_state.system_ctrl &
		   CPIA2_SYSTEM_CONTROL_V2W_ERR)
			cpia2_do_command(cam, CPIA2_CMD_CLEAR_V2W_ERR,
					 TRANSFER_WRITE, 0);

		
		cpia2_do_command(cam, CPIA2_CMD_SET_SYSTEM_CTRL,
				 TRANSFER_WRITE, 1);

		
		cpia2_do_command(cam, CPIA2_CMD_GET_VP_SYSTEM_STATE,
				 TRANSFER_READ, 0);
		if (cam->params.vp_params.system_state &
		    CPIA2_VP_SYSTEMSTATE_HK_ALIVE) {
			break;
		} else if (i == 50) {
			cam->params.camera_state.power_mode = LO_POWER_MODE;
			ERR("Camera did not wake up\n");
			return -EIO;
		}
	}

	DBG("System now in high power state\n");
	cam->params.camera_state.power_mode = HI_POWER_MODE;
	return 0;
}

int cpia2_set_low_power(struct camera_data *cam)
{
	cam->params.camera_state.power_mode = LO_POWER_MODE;
	cpia2_do_command(cam, CPIA2_CMD_SET_SYSTEM_CTRL, TRANSFER_WRITE, 0);
	return 0;
}

static int cpia2_send_onebyte_command(struct camera_data *cam,
				      struct cpia2_command *cmd,
				      u8 start, u8 datum)
{
	cmd->buffer.block_data[0] = datum;
	cmd->start = start;
	cmd->reg_count = 1;
	return cpia2_send_command(cam, cmd);
}

static int apply_vp_patch(struct camera_data *cam)
{
	const struct firmware *fw;
	const char fw_name[] = "cpia2/stv0672_vp4.bin";
	int i, ret;
	struct cpia2_command cmd;

	ret = request_firmware(&fw, fw_name, &cam->dev->dev);
	if (ret) {
		printk(KERN_ERR "cpia2: failed to load VP patch \"%s\"\n",
		       fw_name);
		return ret;
	}

	cmd.req_mode = CAMERAACCESS_TYPE_REPEAT | CAMERAACCESS_VP;
	cmd.direction = TRANSFER_WRITE;

	
	cpia2_send_onebyte_command(cam, &cmd, 0x0A, fw->data[0]); 
	cpia2_send_onebyte_command(cam, &cmd, 0x0B, fw->data[1]); 

	
	for (i = 2; i < fw->size; i += 64) {
		cmd.start = 0x0C; 
		cmd.reg_count = min_t(int, 64, fw->size - i);
		memcpy(cmd.buffer.block_data, &fw->data[i], cmd.reg_count);
		cpia2_send_command(cam, &cmd);
	}

	
	cpia2_send_onebyte_command(cam, &cmd, 0x0A, fw->data[0]); 
	cpia2_send_onebyte_command(cam, &cmd, 0x0B, fw->data[1]); 

	
	cpia2_send_onebyte_command(cam, &cmd, 0x0D, 1);

	release_firmware(fw);
	return 0;
}

static int set_default_user_mode(struct camera_data *cam)
{
	unsigned char user_mode;
	unsigned char frame_rate;
	int width = cam->params.roi.width;
	int height = cam->params.roi.height;

	switch (cam->params.version.sensor_flags) {
	case CPIA2_VP_SENSOR_FLAGS_404:
	case CPIA2_VP_SENSOR_FLAGS_407:
	case CPIA2_VP_SENSOR_FLAGS_409:
	case CPIA2_VP_SENSOR_FLAGS_410:
		if ((width > STV_IMAGE_QCIF_COLS)
		    || (height > STV_IMAGE_QCIF_ROWS)) {
			user_mode = CPIA2_VP_USER_MODE_CIF;
		} else {
			user_mode = CPIA2_VP_USER_MODE_QCIFDS;
		}
		frame_rate = CPIA2_VP_FRAMERATE_30;
		break;
	case CPIA2_VP_SENSOR_FLAGS_500:
		if ((width > STV_IMAGE_CIF_COLS)
		    || (height > STV_IMAGE_CIF_ROWS)) {
			user_mode = CPIA2_VP_USER_MODE_VGA;
		} else {
			user_mode = CPIA2_VP_USER_MODE_QVGADS;
		}
		if (cam->params.pnp_id.device_type == DEVICE_STV_672)
			frame_rate = CPIA2_VP_FRAMERATE_15;
		else
			frame_rate = CPIA2_VP_FRAMERATE_30;
		break;
	default:
		LOG("%s: Invalid sensor flag value 0x%0X\n",__func__,
		    cam->params.version.sensor_flags);
		return -EINVAL;
	}

	DBG("Sensor flag = 0x%0x, user mode = 0x%0x, frame rate = 0x%X\n",
	    cam->params.version.sensor_flags, user_mode, frame_rate);
	cpia2_do_command(cam, CPIA2_CMD_SET_USER_MODE, TRANSFER_WRITE,
			 user_mode);
	if(cam->params.vp_params.frame_rate > 0 &&
	   frame_rate > cam->params.vp_params.frame_rate)
		frame_rate = cam->params.vp_params.frame_rate;

	cpia2_set_fps(cam, frame_rate);


	return 0;
}

int cpia2_match_video_size(int width, int height)
{
	if (width >= STV_IMAGE_VGA_COLS && height >= STV_IMAGE_VGA_ROWS)
		return VIDEOSIZE_VGA;

	if (width >= STV_IMAGE_CIF_COLS && height >= STV_IMAGE_CIF_ROWS)
		return VIDEOSIZE_CIF;

	if (width >= STV_IMAGE_QVGA_COLS && height >= STV_IMAGE_QVGA_ROWS)
		return VIDEOSIZE_QVGA;

	if (width >= 288 && height >= 216)
		return VIDEOSIZE_288_216;

	if (width >= 256 && height >= 192)
		return VIDEOSIZE_256_192;

	if (width >= 224 && height >= 168)
		return VIDEOSIZE_224_168;

	if (width >= 192 && height >= 144)
		return VIDEOSIZE_192_144;

	if (width >= STV_IMAGE_QCIF_COLS && height >= STV_IMAGE_QCIF_ROWS)
		return VIDEOSIZE_QCIF;

	return -1;
}

static int set_vw_size(struct camera_data *cam, int size)
{
	int retval = 0;

	cam->params.vp_params.video_size = size;

	switch (size) {
	case VIDEOSIZE_VGA:
		DBG("Setting size to VGA\n");
		cam->params.roi.width = STV_IMAGE_VGA_COLS;
		cam->params.roi.height = STV_IMAGE_VGA_ROWS;
		cam->width = STV_IMAGE_VGA_COLS;
		cam->height = STV_IMAGE_VGA_ROWS;
		break;
	case VIDEOSIZE_CIF:
		DBG("Setting size to CIF\n");
		cam->params.roi.width = STV_IMAGE_CIF_COLS;
		cam->params.roi.height = STV_IMAGE_CIF_ROWS;
		cam->width = STV_IMAGE_CIF_COLS;
		cam->height = STV_IMAGE_CIF_ROWS;
		break;
	case VIDEOSIZE_QVGA:
		DBG("Setting size to QVGA\n");
		cam->params.roi.width = STV_IMAGE_QVGA_COLS;
		cam->params.roi.height = STV_IMAGE_QVGA_ROWS;
		cam->width = STV_IMAGE_QVGA_COLS;
		cam->height = STV_IMAGE_QVGA_ROWS;
		break;
	case VIDEOSIZE_288_216:
		cam->params.roi.width = 288;
		cam->params.roi.height = 216;
		cam->width = 288;
		cam->height = 216;
		break;
	case VIDEOSIZE_256_192:
		cam->width = 256;
		cam->height = 192;
		cam->params.roi.width = 256;
		cam->params.roi.height = 192;
		break;
	case VIDEOSIZE_224_168:
		cam->width = 224;
		cam->height = 168;
		cam->params.roi.width = 224;
		cam->params.roi.height = 168;
		break;
	case VIDEOSIZE_192_144:
		cam->width = 192;
		cam->height = 144;
		cam->params.roi.width = 192;
		cam->params.roi.height = 144;
		break;
	case VIDEOSIZE_QCIF:
		DBG("Setting size to QCIF\n");
		cam->params.roi.width = STV_IMAGE_QCIF_COLS;
		cam->params.roi.height = STV_IMAGE_QCIF_ROWS;
		cam->width = STV_IMAGE_QCIF_COLS;
		cam->height = STV_IMAGE_QCIF_ROWS;
		break;
	default:
		retval = -EINVAL;
	}
	return retval;
}

static int configure_sensor(struct camera_data *cam,
			    int req_width, int req_height)
{
	int retval;

	switch (cam->params.version.sensor_flags) {
	case CPIA2_VP_SENSOR_FLAGS_404:
	case CPIA2_VP_SENSOR_FLAGS_407:
	case CPIA2_VP_SENSOR_FLAGS_409:
	case CPIA2_VP_SENSOR_FLAGS_410:
		retval = config_sensor_410(cam, req_width, req_height);
		break;
	case CPIA2_VP_SENSOR_FLAGS_500:
		retval = config_sensor_500(cam, req_width, req_height);
		break;
	default:
		return -EINVAL;
	}

	return retval;
}

static int config_sensor_410(struct camera_data *cam,
			    int req_width, int req_height)
{
	struct cpia2_command cmd;
	int i = 0;
	int image_size;
	int image_type;
	int width = req_width;
	int height = req_height;

	if (width > STV_IMAGE_CIF_COLS)
		width = STV_IMAGE_CIF_COLS;
	if (height > STV_IMAGE_CIF_ROWS)
		height = STV_IMAGE_CIF_ROWS;

	image_size = cpia2_match_video_size(width, height);

	DBG("Config 410: width = %d, height = %d\n", width, height);
	DBG("Image size returned is %d\n", image_size);
	if (image_size >= 0) {
		set_vw_size(cam, image_size);
		width = cam->params.roi.width;
		height = cam->params.roi.height;

		DBG("After set_vw_size(), width = %d, height = %d\n",
		    width, height);
		if (width <= 176 && height <= 144) {
			DBG("image type = VIDEOSIZE_QCIF\n");
			image_type = VIDEOSIZE_QCIF;
		}
		else if (width <= 320 && height <= 240) {
			DBG("image type = VIDEOSIZE_QVGA\n");
			image_type = VIDEOSIZE_QVGA;
		}
		else {
			DBG("image type = VIDEOSIZE_CIF\n");
			image_type = VIDEOSIZE_CIF;
		}
	} else {
		ERR("ConfigSensor410 failed\n");
		return -EINVAL;
	}

	cmd.req_mode = CAMERAACCESS_TYPE_RANDOM | CAMERAACCESS_VC;
	cmd.direction = TRANSFER_WRITE;

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_FORMAT;
	if (image_type == VIDEOSIZE_CIF) {
		cmd.buffer.registers[i++].value =
		    (u8) (CPIA2_VC_VC_FORMAT_UFIRST |
			  CPIA2_VC_VC_FORMAT_SHORTLINE);
	} else {
		cmd.buffer.registers[i++].value =
		    (u8) CPIA2_VC_VC_FORMAT_UFIRST;
	}

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_CLOCKS;
	if (image_type == VIDEOSIZE_QCIF) {
		if (cam->params.pnp_id.device_type == DEVICE_STV_672) {
			cmd.buffer.registers[i++].value=
				(u8)(CPIA2_VC_VC_672_CLOCKS_CIF_DIV_BY_3 |
				     CPIA2_VC_VC_672_CLOCKS_SCALING |
				     CPIA2_VC_VC_CLOCKS_LOGDIV2);
			DBG("VC_Clocks (0xc4) should be B\n");
		}
		else {
			cmd.buffer.registers[i++].value=
				(u8)(CPIA2_VC_VC_676_CLOCKS_CIF_DIV_BY_3 |
				     CPIA2_VC_VC_CLOCKS_LOGDIV2);
		}
	} else {
		if (cam->params.pnp_id.device_type == DEVICE_STV_672) {
			cmd.buffer.registers[i++].value =
			   (u8) (CPIA2_VC_VC_672_CLOCKS_CIF_DIV_BY_3 |
				 CPIA2_VC_VC_CLOCKS_LOGDIV0);
		}
		else {
			cmd.buffer.registers[i++].value =
			   (u8) (CPIA2_VC_VC_676_CLOCKS_CIF_DIV_BY_3 |
				 CPIA2_VC_VC_676_CLOCKS_SCALING |
				 CPIA2_VC_VC_CLOCKS_LOGDIV0);
		}
	}
	DBG("VC_Clocks (0xc4) = 0x%0X\n", cmd.buffer.registers[i-1].value);

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_IHSIZE_LO;
	if (image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value =
		    (u8) (STV_IMAGE_QCIF_COLS / 4);
	else
		cmd.buffer.registers[i++].value =
		    (u8) (STV_IMAGE_CIF_COLS / 4);

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_XLIM_HI;
	if (image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = (u8) 0;
	else
		cmd.buffer.registers[i++].value = (u8) 1;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_XLIM_LO;
	if (image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = (u8) 208;
	else
		cmd.buffer.registers[i++].value = (u8) 160;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_YLIM_HI;
	if (image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = (u8) 0;
	else
		cmd.buffer.registers[i++].value = (u8) 1;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_YLIM_LO;
	if (image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = (u8) 160;
	else
		cmd.buffer.registers[i++].value = (u8) 64;

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_OHSIZE;
	cmd.buffer.registers[i++].value = cam->params.roi.width / 4;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_OVSIZE;
	cmd.buffer.registers[i++].value = cam->params.roi.height / 4;

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_HCROP;
	if (image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value =
		    (u8) (((STV_IMAGE_QCIF_COLS / 4) - (width / 4)) / 2);
	else
		cmd.buffer.registers[i++].value =
		    (u8) (((STV_IMAGE_CIF_COLS / 4) - (width / 4)) / 2);

	cmd.buffer.registers[i].index = CPIA2_VC_VC_VCROP;
	if (image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value =
		    (u8) (((STV_IMAGE_QCIF_ROWS / 4) - (height / 4)) / 2);
	else
		cmd.buffer.registers[i++].value =
		    (u8) (((STV_IMAGE_CIF_ROWS / 4) - (height / 4)) / 2);

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_HPHASE;
	cmd.buffer.registers[i++].value = (u8) 0;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_VPHASE;
	cmd.buffer.registers[i++].value = (u8) 0;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_HISPAN;
	cmd.buffer.registers[i++].value = (u8) 31;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_VISPAN;
	cmd.buffer.registers[i++].value = (u8) 31;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_HICROP;
	cmd.buffer.registers[i++].value = (u8) 0;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_VICROP;
	cmd.buffer.registers[i++].value = (u8) 0;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_HFRACT;
	cmd.buffer.registers[i++].value = (u8) 0x81;	

	cmd.buffer.registers[i].index = CPIA2_VC_VC_VFRACT;
	cmd.buffer.registers[i++].value = (u8) 0x81;	

	cmd.reg_count = i;

	cpia2_send_command(cam, &cmd);

	return i;
}


static int config_sensor_500(struct camera_data *cam,
			     int req_width, int req_height)
{
	struct cpia2_command cmd;
	int i = 0;
	int image_size = VIDEOSIZE_CIF;
	int image_type = VIDEOSIZE_VGA;
	int width = req_width;
	int height = req_height;
	unsigned int device = cam->params.pnp_id.device_type;

	image_size = cpia2_match_video_size(width, height);

	if (width > STV_IMAGE_CIF_COLS || height > STV_IMAGE_CIF_ROWS)
		image_type = VIDEOSIZE_VGA;
	else if (width > STV_IMAGE_QVGA_COLS || height > STV_IMAGE_QVGA_ROWS)
		image_type = VIDEOSIZE_CIF;
	else if (width > STV_IMAGE_QCIF_COLS || height > STV_IMAGE_QCIF_ROWS)
		image_type = VIDEOSIZE_QVGA;
	else
		image_type = VIDEOSIZE_QCIF;

	if (image_size >= 0) {
		set_vw_size(cam, image_size);
		width = cam->params.roi.width;
		height = cam->params.roi.height;
	} else {
		ERR("ConfigSensor500 failed\n");
		return -EINVAL;
	}

	DBG("image_size = %d, width = %d, height = %d, type = %d\n",
	    image_size, width, height, image_type);

	cmd.req_mode = CAMERAACCESS_TYPE_RANDOM | CAMERAACCESS_VC;
	cmd.direction = TRANSFER_WRITE;
	i = 0;

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_FORMAT;
	cmd.buffer.registers[i].value = (u8) CPIA2_VC_VC_FORMAT_UFIRST;
	if (image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i].value |= (u8) CPIA2_VC_VC_FORMAT_DECIMATING;
	i++;

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_CLOCKS;
	if (device == DEVICE_STV_672) {
		if (image_type == VIDEOSIZE_VGA)
			cmd.buffer.registers[i].value =
				(u8)CPIA2_VC_VC_CLOCKS_LOGDIV1;
		else
			cmd.buffer.registers[i].value =
				(u8)(CPIA2_VC_VC_672_CLOCKS_SCALING |
				     CPIA2_VC_VC_CLOCKS_LOGDIV3);
	} else {
		if (image_type == VIDEOSIZE_VGA)
			cmd.buffer.registers[i].value =
				(u8)CPIA2_VC_VC_CLOCKS_LOGDIV0;
		else
			cmd.buffer.registers[i].value =
				(u8)(CPIA2_VC_VC_676_CLOCKS_SCALING |
				     CPIA2_VC_VC_CLOCKS_LOGDIV2);
	}
	i++;

	DBG("VC_CLOCKS = 0x%X\n", cmd.buffer.registers[i-1].value);

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_IHSIZE_LO;
	if (image_type == VIDEOSIZE_VGA)
		cmd.buffer.registers[i].value =
		    (u8) (STV_IMAGE_VGA_COLS / 4);
	else
		cmd.buffer.registers[i].value =
		    (u8) (STV_IMAGE_QVGA_COLS / 4);
	i++;
	DBG("Input width = %d\n", cmd.buffer.registers[i-1].value);

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_XLIM_HI;
	if (image_type == VIDEOSIZE_VGA)
		cmd.buffer.registers[i++].value = (u8) 2;
	else
		cmd.buffer.registers[i++].value = (u8) 1;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_XLIM_LO;
	if (image_type == VIDEOSIZE_VGA)
		cmd.buffer.registers[i++].value = (u8) 250;
	else if (image_type == VIDEOSIZE_QVGA)
		cmd.buffer.registers[i++].value = (u8) 125;
	else
		cmd.buffer.registers[i++].value = (u8) 160;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_YLIM_HI;
	if (image_type == VIDEOSIZE_VGA)
		cmd.buffer.registers[i++].value = (u8) 2;
	else
		cmd.buffer.registers[i++].value = (u8) 1;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_YLIM_LO;
	if (image_type == VIDEOSIZE_VGA)
		cmd.buffer.registers[i++].value = (u8) 12;
	else if (image_type == VIDEOSIZE_QVGA)
		cmd.buffer.registers[i++].value = (u8) 64;
	else
		cmd.buffer.registers[i++].value = (u8) 6;

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_OHSIZE;
	if (image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = STV_IMAGE_CIF_COLS  / 4;
	else
		cmd.buffer.registers[i++].value = width / 4;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_OVSIZE;
	if (image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = STV_IMAGE_CIF_ROWS  / 4;
	else
		cmd.buffer.registers[i++].value = height / 4;

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_HCROP;
	if (image_type == VIDEOSIZE_VGA)
		cmd.buffer.registers[i++].value =
		    (u8) (((STV_IMAGE_VGA_COLS / 4) - (width / 4)) / 2);
	else if (image_type == VIDEOSIZE_QVGA)
		cmd.buffer.registers[i++].value =
		    (u8) (((STV_IMAGE_QVGA_COLS / 4) - (width / 4)) / 2);
	else if (image_type == VIDEOSIZE_CIF)
		cmd.buffer.registers[i++].value =
		    (u8) (((STV_IMAGE_CIF_COLS / 4) - (width / 4)) / 2);
	else 
		cmd.buffer.registers[i++].value =
			(u8) (((STV_IMAGE_QCIF_COLS / 4) - (width / 4)) / 2);

	cmd.buffer.registers[i].index = CPIA2_VC_VC_VCROP;
	if (image_type == VIDEOSIZE_VGA)
		cmd.buffer.registers[i++].value =
		    (u8) (((STV_IMAGE_VGA_ROWS / 4) - (height / 4)) / 2);
	else if (image_type == VIDEOSIZE_QVGA)
		cmd.buffer.registers[i++].value =
		    (u8) (((STV_IMAGE_QVGA_ROWS / 4) - (height / 4)) / 2);
	else if (image_type == VIDEOSIZE_CIF)
		cmd.buffer.registers[i++].value =
		    (u8) (((STV_IMAGE_CIF_ROWS / 4) - (height / 4)) / 2);
	else 
		cmd.buffer.registers[i++].value =
		    (u8) (((STV_IMAGE_QCIF_ROWS / 4) - (height / 4)) / 2);

	
	cmd.buffer.registers[i].index = CPIA2_VC_VC_HPHASE;
	if (image_type == VIDEOSIZE_CIF || image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = (u8) 36;
	else
		cmd.buffer.registers[i++].value = (u8) 0;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_VPHASE;
	if (image_type == VIDEOSIZE_CIF || image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = (u8) 32;
	else
		cmd.buffer.registers[i++].value = (u8) 0;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_HISPAN;
	if (image_type == VIDEOSIZE_CIF || image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = (u8) 26;
	else
		cmd.buffer.registers[i++].value = (u8) 31;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_VISPAN;
	if (image_type == VIDEOSIZE_CIF || image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = (u8) 21;
	else
		cmd.buffer.registers[i++].value = (u8) 31;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_HICROP;
	cmd.buffer.registers[i++].value = (u8) 0;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_VICROP;
	cmd.buffer.registers[i++].value = (u8) 0;

	cmd.buffer.registers[i].index = CPIA2_VC_VC_HFRACT;
	if (image_type == VIDEOSIZE_CIF || image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = (u8) 0x2B;	
	else
		cmd.buffer.registers[i++].value = (u8) 0x81;	

	cmd.buffer.registers[i].index = CPIA2_VC_VC_VFRACT;
	if (image_type == VIDEOSIZE_CIF || image_type == VIDEOSIZE_QCIF)
		cmd.buffer.registers[i++].value = (u8) 0x13;	
	else
		cmd.buffer.registers[i++].value = (u8) 0x81;	

	cmd.reg_count = i;

	cpia2_send_command(cam, &cmd);

	return i;
}


static int set_all_properties(struct camera_data *cam)
{

	cpia2_set_color_params(cam);

	cpia2_usb_change_streaming_alternate(cam,
					  cam->params.camera_state.stream_mode);

	cpia2_do_command(cam, CPIA2_CMD_SET_USER_EFFECTS, TRANSFER_WRITE,
			 cam->params.vp_params.user_effects);

	cpia2_set_flicker_mode(cam,
			       cam->params.flicker_control.flicker_mode_req);

	cpia2_do_command(cam,
			 CPIA2_CMD_SET_VC_MP_GPIO_DIRECTION,
			 TRANSFER_WRITE, cam->params.vp_params.gpio_direction);
	cpia2_do_command(cam, CPIA2_CMD_SET_VC_MP_GPIO_DATA, TRANSFER_WRITE,
			 cam->params.vp_params.gpio_data);

	wake_system(cam);

	set_lowlight_boost(cam);

	return 0;
}

void cpia2_save_camera_state(struct camera_data *cam)
{
	get_color_params(cam);
	cpia2_do_command(cam, CPIA2_CMD_GET_USER_EFFECTS, TRANSFER_READ, 0);
	cpia2_do_command(cam, CPIA2_CMD_GET_VC_MP_GPIO_DIRECTION, TRANSFER_READ,
			 0);
	cpia2_do_command(cam, CPIA2_CMD_GET_VC_MP_GPIO_DATA, TRANSFER_READ, 0);
	
}

static void get_color_params(struct camera_data *cam)
{
	cpia2_do_command(cam, CPIA2_CMD_GET_VP_BRIGHTNESS, TRANSFER_READ, 0);
	cpia2_do_command(cam, CPIA2_CMD_GET_VP_SATURATION, TRANSFER_READ, 0);
	cpia2_do_command(cam, CPIA2_CMD_GET_CONTRAST, TRANSFER_READ, 0);
}

void cpia2_set_color_params(struct camera_data *cam)
{
	DBG("Setting color params\n");
	cpia2_set_brightness(cam, cam->params.color_params.brightness);
	cpia2_set_contrast(cam, cam->params.color_params.contrast);
	cpia2_set_saturation(cam, cam->params.color_params.saturation);
}

int cpia2_set_flicker_mode(struct camera_data *cam, int mode)
{
	unsigned char cam_reg;
	int err = 0;

	if(cam->params.pnp_id.device_type != DEVICE_STV_672)
		return -EINVAL;

	
	if((err = cpia2_do_command(cam, CPIA2_CMD_GET_FLICKER_MODES,
				   TRANSFER_READ, 0)))
		return err;
	cam_reg = cam->params.flicker_control.cam_register;

	switch(mode) {
	case NEVER_FLICKER:
		cam_reg |= CPIA2_VP_FLICKER_MODES_NEVER_FLICKER;
		cam_reg &= ~CPIA2_VP_FLICKER_MODES_50HZ;
		break;
	case FLICKER_60:
		cam_reg &= ~CPIA2_VP_FLICKER_MODES_NEVER_FLICKER;
		cam_reg &= ~CPIA2_VP_FLICKER_MODES_50HZ;
		break;
	case FLICKER_50:
		cam_reg &= ~CPIA2_VP_FLICKER_MODES_NEVER_FLICKER;
		cam_reg |= CPIA2_VP_FLICKER_MODES_50HZ;
		break;
	default:
		return -EINVAL;
	}

	if((err = cpia2_do_command(cam, CPIA2_CMD_SET_FLICKER_MODES,
				   TRANSFER_WRITE, cam_reg)))
		return err;

	
	if((err = cpia2_do_command(cam, CPIA2_CMD_GET_VP_EXP_MODES,
				   TRANSFER_READ, 0)))
		return err;
	cam_reg = cam->params.vp_params.exposure_modes;

	if (mode == NEVER_FLICKER) {
		cam_reg |= CPIA2_VP_EXPOSURE_MODES_INHIBIT_FLICKER;
	} else {
		cam_reg &= ~CPIA2_VP_EXPOSURE_MODES_INHIBIT_FLICKER;
	}

	if((err = cpia2_do_command(cam, CPIA2_CMD_SET_VP_EXP_MODES,
				   TRANSFER_WRITE, cam_reg)))
		return err;

	if((err = cpia2_do_command(cam, CPIA2_CMD_REHASH_VP4,
				   TRANSFER_WRITE, 1)))
		return err;

	switch(mode) {
	case NEVER_FLICKER:
		cam->params.flicker_control.flicker_mode_req = mode;
		break;
	case FLICKER_60:
		cam->params.flicker_control.flicker_mode_req = mode;
		cam->params.flicker_control.mains_frequency = 60;
		break;
	case FLICKER_50:
		cam->params.flicker_control.flicker_mode_req = mode;
		cam->params.flicker_control.mains_frequency = 50;
		break;
	default:
		err = -EINVAL;
	}

	return err;
}

void cpia2_set_property_flip(struct camera_data *cam, int prop_val)
{
	unsigned char cam_reg;

	cpia2_do_command(cam, CPIA2_CMD_GET_USER_EFFECTS, TRANSFER_READ, 0);
	cam_reg = cam->params.vp_params.user_effects;

	if (prop_val)
	{
		cam_reg |= CPIA2_VP_USER_EFFECTS_FLIP;
	}
	else
	{
		cam_reg &= ~CPIA2_VP_USER_EFFECTS_FLIP;
	}
	cpia2_do_command(cam, CPIA2_CMD_SET_USER_EFFECTS, TRANSFER_WRITE,
			 cam_reg);
}

void cpia2_set_property_mirror(struct camera_data *cam, int prop_val)
{
	unsigned char cam_reg;

	cpia2_do_command(cam, CPIA2_CMD_GET_USER_EFFECTS, TRANSFER_READ, 0);
	cam_reg = cam->params.vp_params.user_effects;

	if (prop_val)
	{
		cam_reg |= CPIA2_VP_USER_EFFECTS_MIRROR;
	}
	else
	{
		cam_reg &= ~CPIA2_VP_USER_EFFECTS_MIRROR;
	}
	cpia2_do_command(cam, CPIA2_CMD_SET_USER_EFFECTS, TRANSFER_WRITE,
			 cam_reg);
}


int cpia2_set_target_kb(struct camera_data *cam, unsigned char value)
{
	DBG("Requested target_kb = %d\n", value);
	if (value != cam->params.vc_params.target_kb) {

		cpia2_usb_stream_pause(cam);

		
		cam->params.vc_params.target_kb = value;
		cpia2_reset_camera(cam);

		cpia2_usb_stream_resume(cam);
	}

	return 0;
}

int cpia2_set_gpio(struct camera_data *cam, unsigned char setting)
{
	int ret;


	ret = cpia2_do_command(cam,
			       CPIA2_CMD_SET_VC_MP_GPIO_DIRECTION,
			       CPIA2_VC_MP_DIR_OUTPUT,
			       255);
	if (ret < 0)
		return ret;
	cam->params.vp_params.gpio_direction = 255;

	ret = cpia2_do_command(cam,
			       CPIA2_CMD_SET_VC_MP_GPIO_DATA,
			       CPIA2_VC_MP_DIR_OUTPUT,
			       setting);
	if (ret < 0)
		return ret;
	cam->params.vp_params.gpio_data = setting;

	return 0;
}

int cpia2_set_fps(struct camera_data *cam, int framerate)
{
	int retval;

	switch(framerate) {
		case CPIA2_VP_FRAMERATE_30:
		case CPIA2_VP_FRAMERATE_25:
			if(cam->params.pnp_id.device_type == DEVICE_STV_672 &&
			   cam->params.version.sensor_flags ==
						    CPIA2_VP_SENSOR_FLAGS_500) {
				return -EINVAL;
			}
			
		case CPIA2_VP_FRAMERATE_15:
		case CPIA2_VP_FRAMERATE_12_5:
		case CPIA2_VP_FRAMERATE_7_5:
		case CPIA2_VP_FRAMERATE_6_25:
			break;
		default:
			return -EINVAL;
	}

	if (cam->params.pnp_id.device_type == DEVICE_STV_672 &&
	    framerate == CPIA2_VP_FRAMERATE_15)
		framerate = 0; 

	retval = cpia2_do_command(cam,
				 CPIA2_CMD_FRAMERATE_REQ,
				 TRANSFER_WRITE,
				 framerate);

	if(retval == 0)
		cam->params.vp_params.frame_rate = framerate;

	return retval;
}

void cpia2_set_brightness(struct camera_data *cam, unsigned char value)
{
	if (cam->params.pnp_id.device_type == DEVICE_STV_672 && value == 0)
		value++;
	DBG("Setting brightness to %d (0x%0x)\n", value, value);
	cpia2_do_command(cam,CPIA2_CMD_SET_VP_BRIGHTNESS, TRANSFER_WRITE,value);
}

void cpia2_set_contrast(struct camera_data *cam, unsigned char value)
{
	DBG("Setting contrast to %d (0x%0x)\n", value, value);
	cam->params.color_params.contrast = value;
	cpia2_do_command(cam, CPIA2_CMD_SET_CONTRAST, TRANSFER_WRITE, value);
}

void cpia2_set_saturation(struct camera_data *cam, unsigned char value)
{
	DBG("Setting saturation to %d (0x%0x)\n", value, value);
	cam->params.color_params.saturation = value;
	cpia2_do_command(cam,CPIA2_CMD_SET_VP_SATURATION, TRANSFER_WRITE,value);
}

static void wake_system(struct camera_data *cam)
{
	cpia2_do_command(cam, CPIA2_CMD_SET_WAKEUP, TRANSFER_WRITE, 0);
}

static void set_lowlight_boost(struct camera_data *cam)
{
	struct cpia2_command cmd;

	if (cam->params.pnp_id.device_type != DEVICE_STV_672 ||
	    cam->params.version.sensor_flags != CPIA2_VP_SENSOR_FLAGS_500)
		return;

	cmd.direction = TRANSFER_WRITE;
	cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
	cmd.reg_count = 3;
	cmd.start = CPIA2_VP_RAM_ADDR_H;

	cmd.buffer.block_data[0] = 0;	
	cmd.buffer.block_data[1] = 0x59;	
	cmd.buffer.block_data[2] = 0;	

	cpia2_send_command(cam, &cmd);

	if (cam->params.vp_params.lowlight_boost) {
		cmd.buffer.block_data[0] = 0x02;	
	} else {
		cmd.buffer.block_data[0] = 0x06;
	}
	cmd.start = CPIA2_VP_RAM_DATA;
	cmd.reg_count = 1;
	cpia2_send_command(cam, &cmd);

	
	cpia2_do_command(cam, CPIA2_CMD_REHASH_VP4, TRANSFER_WRITE, 1);
}

void cpia2_set_format(struct camera_data *cam)
{
	cam->flush = true;

	cpia2_usb_stream_pause(cam);

	
	cpia2_set_low_power(cam);
	cpia2_reset_camera(cam);
	cam->flush = false;

	cpia2_dbg_dump_registers(cam);

	cpia2_usb_stream_resume(cam);
}

void cpia2_dbg_dump_registers(struct camera_data *cam)
{
#ifdef _CPIA2_DEBUG_
	struct cpia2_command cmd;

	if (!(debugs_on & DEBUG_DUMP_REGS))
		return;

	cmd.direction = TRANSFER_READ;

	
	cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_SYSTEM;
	cmd.reg_count = 3;
	cmd.start = 0;
	cpia2_send_command(cam, &cmd);
	printk(KERN_DEBUG "System Device Hi      = 0x%X\n",
	       cmd.buffer.block_data[0]);
	printk(KERN_DEBUG "System Device Lo      = 0x%X\n",
	       cmd.buffer.block_data[1]);
	printk(KERN_DEBUG "System_system control = 0x%X\n",
	       cmd.buffer.block_data[2]);

	
	cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VC;
	cmd.reg_count = 4;
	cmd.start = 0x80;
	cpia2_send_command(cam, &cmd);
	printk(KERN_DEBUG "ASIC_ID       = 0x%X\n",
	       cmd.buffer.block_data[0]);
	printk(KERN_DEBUG "ASIC_REV      = 0x%X\n",
	       cmd.buffer.block_data[1]);
	printk(KERN_DEBUG "PW_CONTRL     = 0x%X\n",
	       cmd.buffer.block_data[2]);
	printk(KERN_DEBUG "WAKEUP        = 0x%X\n",
	       cmd.buffer.block_data[3]);

	cmd.start = 0xA0;	
	cmd.reg_count = 1;
	cpia2_send_command(cam, &cmd);
	printk(KERN_DEBUG "Stream ctrl   = 0x%X\n",
	       cmd.buffer.block_data[0]);

	cmd.start = 0xA4;	
	cpia2_send_command(cam, &cmd);
	printk(KERN_DEBUG "Stream status = 0x%X\n",
	       cmd.buffer.block_data[0]);

	cmd.start = 0xA8;	
	cmd.reg_count = 3;
	cpia2_send_command(cam, &cmd);
	printk(KERN_DEBUG "USB_CTRL      = 0x%X\n",
	       cmd.buffer.block_data[0]);
	printk(KERN_DEBUG "USB_STRM      = 0x%X\n",
	       cmd.buffer.block_data[1]);
	printk(KERN_DEBUG "USB_STATUS    = 0x%X\n",
	       cmd.buffer.block_data[2]);

	cmd.start = 0xAF;	
	cmd.reg_count = 1;
	cpia2_send_command(cam, &cmd);
	printk(KERN_DEBUG "USB settings  = 0x%X\n",
	       cmd.buffer.block_data[0]);

	cmd.start = 0xC0;	
	cmd.reg_count = 26;
	cpia2_send_command(cam, &cmd);
	printk(KERN_DEBUG "VC Control    = 0x%0X\n",
	       cmd.buffer.block_data[0]);
	printk(KERN_DEBUG "VC Format     = 0x%0X\n",
	       cmd.buffer.block_data[3]);
	printk(KERN_DEBUG "VC Clocks     = 0x%0X\n",
	       cmd.buffer.block_data[4]);
	printk(KERN_DEBUG "VC IHSize     = 0x%0X\n",
	       cmd.buffer.block_data[5]);
	printk(KERN_DEBUG "VC Xlim Hi    = 0x%0X\n",
	       cmd.buffer.block_data[6]);
	printk(KERN_DEBUG "VC XLim Lo    = 0x%0X\n",
	       cmd.buffer.block_data[7]);
	printk(KERN_DEBUG "VC YLim Hi    = 0x%0X\n",
	       cmd.buffer.block_data[8]);
	printk(KERN_DEBUG "VC YLim Lo    = 0x%0X\n",
	       cmd.buffer.block_data[9]);
	printk(KERN_DEBUG "VC OHSize     = 0x%0X\n",
	       cmd.buffer.block_data[10]);
	printk(KERN_DEBUG "VC OVSize     = 0x%0X\n",
	       cmd.buffer.block_data[11]);
	printk(KERN_DEBUG "VC HCrop      = 0x%0X\n",
	       cmd.buffer.block_data[12]);
	printk(KERN_DEBUG "VC VCrop      = 0x%0X\n",
	       cmd.buffer.block_data[13]);
	printk(KERN_DEBUG "VC HPhase     = 0x%0X\n",
	       cmd.buffer.block_data[14]);
	printk(KERN_DEBUG "VC VPhase     = 0x%0X\n",
	       cmd.buffer.block_data[15]);
	printk(KERN_DEBUG "VC HIspan     = 0x%0X\n",
	       cmd.buffer.block_data[16]);
	printk(KERN_DEBUG "VC VIspan     = 0x%0X\n",
	       cmd.buffer.block_data[17]);
	printk(KERN_DEBUG "VC HiCrop     = 0x%0X\n",
	       cmd.buffer.block_data[18]);
	printk(KERN_DEBUG "VC ViCrop     = 0x%0X\n",
	       cmd.buffer.block_data[19]);
	printk(KERN_DEBUG "VC HiFract    = 0x%0X\n",
	       cmd.buffer.block_data[20]);
	printk(KERN_DEBUG "VC ViFract    = 0x%0X\n",
	       cmd.buffer.block_data[21]);
	printk(KERN_DEBUG "VC JPeg Opt   = 0x%0X\n",
	       cmd.buffer.block_data[22]);
	printk(KERN_DEBUG "VC Creep Per  = 0x%0X\n",
	       cmd.buffer.block_data[23]);
	printk(KERN_DEBUG "VC User Sq.   = 0x%0X\n",
	       cmd.buffer.block_data[24]);
	printk(KERN_DEBUG "VC Target KB  = 0x%0X\n",
	       cmd.buffer.block_data[25]);

	
	cmd.req_mode = CAMERAACCESS_TYPE_BLOCK | CAMERAACCESS_VP;
	cmd.reg_count = 14;
	cmd.start = 0;
	cpia2_send_command(cam, &cmd);

	printk(KERN_DEBUG "VP Dev Hi     = 0x%0X\n",
	       cmd.buffer.block_data[0]);
	printk(KERN_DEBUG "VP Dev Lo     = 0x%0X\n",
	       cmd.buffer.block_data[1]);
	printk(KERN_DEBUG "VP Sys State  = 0x%0X\n",
	       cmd.buffer.block_data[2]);
	printk(KERN_DEBUG "VP Sys Ctrl   = 0x%0X\n",
	       cmd.buffer.block_data[3]);
	printk(KERN_DEBUG "VP Sensor flg = 0x%0X\n",
	       cmd.buffer.block_data[5]);
	printk(KERN_DEBUG "VP Sensor Rev = 0x%0X\n",
	       cmd.buffer.block_data[6]);
	printk(KERN_DEBUG "VP Dev Config = 0x%0X\n",
	       cmd.buffer.block_data[7]);
	printk(KERN_DEBUG "VP GPIO_DIR   = 0x%0X\n",
	       cmd.buffer.block_data[8]);
	printk(KERN_DEBUG "VP GPIO_DATA  = 0x%0X\n",
	       cmd.buffer.block_data[9]);
	printk(KERN_DEBUG "VP Ram ADDR H = 0x%0X\n",
	       cmd.buffer.block_data[10]);
	printk(KERN_DEBUG "VP Ram ADDR L = 0x%0X\n",
	       cmd.buffer.block_data[11]);
	printk(KERN_DEBUG "VP RAM Data   = 0x%0X\n",
	       cmd.buffer.block_data[12]);
	printk(KERN_DEBUG "Do Call       = 0x%0X\n",
	       cmd.buffer.block_data[13]);

	if (cam->params.pnp_id.device_type == DEVICE_STV_672) {
		cmd.reg_count = 9;
		cmd.start = 0x0E;
		cpia2_send_command(cam, &cmd);
		printk(KERN_DEBUG "VP Clock Ctrl = 0x%0X\n",
		       cmd.buffer.block_data[0]);
		printk(KERN_DEBUG "VP Patch Rev  = 0x%0X\n",
		       cmd.buffer.block_data[1]);
		printk(KERN_DEBUG "VP Vid Mode   = 0x%0X\n",
		       cmd.buffer.block_data[2]);
		printk(KERN_DEBUG "VP Framerate  = 0x%0X\n",
		       cmd.buffer.block_data[3]);
		printk(KERN_DEBUG "VP UserEffect = 0x%0X\n",
		       cmd.buffer.block_data[4]);
		printk(KERN_DEBUG "VP White Bal  = 0x%0X\n",
		       cmd.buffer.block_data[5]);
		printk(KERN_DEBUG "VP WB thresh  = 0x%0X\n",
		       cmd.buffer.block_data[6]);
		printk(KERN_DEBUG "VP Exp Modes  = 0x%0X\n",
		       cmd.buffer.block_data[7]);
		printk(KERN_DEBUG "VP Exp Target = 0x%0X\n",
		       cmd.buffer.block_data[8]);

		cmd.reg_count = 1;
		cmd.start = 0x1B;
		cpia2_send_command(cam, &cmd);
		printk(KERN_DEBUG "VP FlickerMds = 0x%0X\n",
		       cmd.buffer.block_data[0]);
	} else {
		cmd.reg_count = 8 ;
		cmd.start = 0x0E;
		cpia2_send_command(cam, &cmd);
		printk(KERN_DEBUG "VP Clock Ctrl = 0x%0X\n",
		       cmd.buffer.block_data[0]);
		printk(KERN_DEBUG "VP Patch Rev  = 0x%0X\n",
		       cmd.buffer.block_data[1]);
		printk(KERN_DEBUG "VP Vid Mode   = 0x%0X\n",
		       cmd.buffer.block_data[5]);
		printk(KERN_DEBUG "VP Framerate  = 0x%0X\n",
		       cmd.buffer.block_data[6]);
		printk(KERN_DEBUG "VP UserEffect = 0x%0X\n",
		       cmd.buffer.block_data[7]);

		cmd.reg_count = 1;
		cmd.start = CPIA2_VP5_EXPOSURE_TARGET;
		cpia2_send_command(cam, &cmd);
		printk(KERN_DEBUG "VP5 Exp Target= 0x%0X\n",
		       cmd.buffer.block_data[0]);

		cmd.reg_count = 4;
		cmd.start = 0x3A;
		cpia2_send_command(cam, &cmd);
		printk(KERN_DEBUG "VP5 MY Black  = 0x%0X\n",
		       cmd.buffer.block_data[0]);
		printk(KERN_DEBUG "VP5 MCY Range = 0x%0X\n",
		       cmd.buffer.block_data[1]);
		printk(KERN_DEBUG "VP5 MYCEILING = 0x%0X\n",
		       cmd.buffer.block_data[2]);
		printk(KERN_DEBUG "VP5 MCUV Sat  = 0x%0X\n",
		       cmd.buffer.block_data[3]);
	}
#endif
}

static void reset_camera_struct(struct camera_data *cam)
{
	cam->params.color_params.brightness = DEFAULT_BRIGHTNESS;
	cam->params.color_params.contrast = DEFAULT_CONTRAST;
	cam->params.color_params.saturation = DEFAULT_SATURATION;
	cam->params.vp_params.lowlight_boost = 0;

	
	cam->params.flicker_control.flicker_mode_req = NEVER_FLICKER;
	cam->params.flicker_control.mains_frequency = 60;

	
	cam->params.compression.jpeg_options = CPIA2_VC_VC_JPEG_OPT_DEFAULT;
	cam->params.compression.creep_period = 2;
	cam->params.compression.user_squeeze = 20;
	cam->params.compression.inhibit_htables = false;

	
	cam->params.vp_params.gpio_direction = 0;	
	cam->params.vp_params.gpio_data = 0;

	
	cam->params.vc_params.target_kb = DEFAULT_TARGET_KB;

	if(cam->params.pnp_id.device_type == DEVICE_STV_672) {
		if(cam->params.version.sensor_flags == CPIA2_VP_SENSOR_FLAGS_500)
			cam->params.vp_params.frame_rate = CPIA2_VP_FRAMERATE_15;
		else
			cam->params.vp_params.frame_rate = CPIA2_VP_FRAMERATE_30;
	} else {
		cam->params.vp_params.frame_rate = CPIA2_VP_FRAMERATE_30;
	}

	if (cam->params.version.sensor_flags == CPIA2_VP_SENSOR_FLAGS_500) {
		cam->sensor_type = CPIA2_SENSOR_500;
		cam->video_size = VIDEOSIZE_VGA;
		cam->params.roi.width = STV_IMAGE_VGA_COLS;
		cam->params.roi.height = STV_IMAGE_VGA_ROWS;
	} else {
		cam->sensor_type = CPIA2_SENSOR_410;
		cam->video_size = VIDEOSIZE_CIF;
		cam->params.roi.width = STV_IMAGE_CIF_COLS;
		cam->params.roi.height = STV_IMAGE_CIF_ROWS;
	}

	cam->width = cam->params.roi.width;
	cam->height = cam->params.roi.height;
}

struct camera_data *cpia2_init_camera_struct(void)
{
	struct camera_data *cam;

	cam = kzalloc(sizeof(*cam), GFP_KERNEL);

	if (!cam) {
		ERR("couldn't kmalloc cpia2 struct\n");
		return NULL;
	}


	cam->present = 1;
	mutex_init(&cam->v4l2_lock);
	init_waitqueue_head(&cam->wq_stream);

	return cam;
}

int cpia2_init_camera(struct camera_data *cam)
{
	DBG("Start\n");

	cam->mmapped = false;

	
	cpia2_set_high_power(cam);
	cpia2_get_version_info(cam);
	if (cam->params.version.asic_id != CPIA2_ASIC_672) {
		ERR("Device IO error (asicID has incorrect value of 0x%X\n",
		    cam->params.version.asic_id);
		return -ENODEV;
	}

	
	cpia2_do_command(cam, CPIA2_CMD_SET_VC_MP_GPIO_DIRECTION,
			 TRANSFER_WRITE, 0);
	cpia2_do_command(cam, CPIA2_CMD_SET_VC_MP_GPIO_DATA,
			 TRANSFER_WRITE, 0);

	
	reset_camera_struct(cam);

	cpia2_set_low_power(cam);

	DBG("End\n");

	return 0;
}

int cpia2_allocate_buffers(struct camera_data *cam)
{
	int i;

	if(!cam->buffers) {
		u32 size = cam->num_frames*sizeof(struct framebuf);
		cam->buffers = kmalloc(size, GFP_KERNEL);
		if(!cam->buffers) {
			ERR("couldn't kmalloc frame buffer structures\n");
			return -ENOMEM;
		}
	}

	if(!cam->frame_buffer) {
		cam->frame_buffer = rvmalloc(cam->frame_size*cam->num_frames);
		if (!cam->frame_buffer) {
			ERR("couldn't vmalloc frame buffer data area\n");
			kfree(cam->buffers);
			cam->buffers = NULL;
			return -ENOMEM;
		}
	}

	for(i=0; i<cam->num_frames-1; ++i) {
		cam->buffers[i].next = &cam->buffers[i+1];
		cam->buffers[i].data = cam->frame_buffer +i*cam->frame_size;
		cam->buffers[i].status = FRAME_EMPTY;
		cam->buffers[i].length = 0;
		cam->buffers[i].max_length = 0;
		cam->buffers[i].num = i;
	}
	cam->buffers[i].next = cam->buffers;
	cam->buffers[i].data = cam->frame_buffer +i*cam->frame_size;
	cam->buffers[i].status = FRAME_EMPTY;
	cam->buffers[i].length = 0;
	cam->buffers[i].max_length = 0;
	cam->buffers[i].num = i;
	cam->curbuff = cam->buffers;
	cam->workbuff = cam->curbuff->next;
	DBG("buffers=%p, curbuff=%p, workbuff=%p\n", cam->buffers, cam->curbuff,
	    cam->workbuff);
	return 0;
}

void cpia2_free_buffers(struct camera_data *cam)
{
	if(cam->buffers) {
		kfree(cam->buffers);
		cam->buffers = NULL;
	}
	if(cam->frame_buffer) {
		rvfree(cam->frame_buffer, cam->frame_size*cam->num_frames);
		cam->frame_buffer = NULL;
	}
}

long cpia2_read(struct camera_data *cam,
		char __user *buf, unsigned long count, int noblock)
{
	struct framebuf *frame;

	if (!count)
		return 0;

	if (!buf) {
		ERR("%s: buffer NULL\n",__func__);
		return -EINVAL;
	}

	if (!cam) {
		ERR("%s: Internal error, camera_data NULL!\n",__func__);
		return -EINVAL;
	}

	if (!cam->present) {
		LOG("%s: camera removed\n",__func__);
		return 0;	
	}

	if (!cam->streaming) {
		
		cpia2_usb_stream_start(cam,
				       cam->params.camera_state.stream_mode);
	}

	
	frame = cam->curbuff;
	if (noblock && frame->status != FRAME_READY) {
		return -EAGAIN;
	}

	if (frame->status != FRAME_READY) {
		mutex_unlock(&cam->v4l2_lock);
		wait_event_interruptible(cam->wq_stream,
			       !cam->present ||
			       (frame = cam->curbuff)->status == FRAME_READY);
		mutex_lock(&cam->v4l2_lock);
		if (signal_pending(current))
			return -ERESTARTSYS;
		if (!cam->present)
			return 0;
	}

	
	if (frame->length > count)
		return -EFAULT;
	if (copy_to_user(buf, frame->data, frame->length))
		return -EFAULT;

	count = frame->length;

	frame->status = FRAME_EMPTY;

	return count;
}

unsigned int cpia2_poll(struct camera_data *cam, struct file *filp,
			poll_table *wait)
{
	unsigned int status=0;

	if (!cam) {
		ERR("%s: Internal error, camera_data not found!\n",__func__);
		return POLLERR;
	}

	if (!cam->present)
		return POLLHUP;

	if(!cam->streaming) {
		
		cpia2_usb_stream_start(cam,
				       cam->params.camera_state.stream_mode);
	}

	poll_wait(filp, &cam->wq_stream, wait);

	if(!cam->present)
		status = POLLHUP;
	else if(cam->curbuff->status == FRAME_READY)
		status = POLLIN | POLLRDNORM;

	return status;
}

int cpia2_remap_buffer(struct camera_data *cam, struct vm_area_struct *vma)
{
	const char *adr = (const char *)vma->vm_start;
	unsigned long size = vma->vm_end-vma->vm_start;
	unsigned long start_offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long start = (unsigned long) adr;
	unsigned long page, pos;

	if (!cam)
		return -ENODEV;

	DBG("mmap offset:%ld size:%ld\n", start_offset, size);

	if (!cam->present)
		return -ENODEV;

	if (size > cam->frame_size*cam->num_frames  ||
	    (start_offset % cam->frame_size) != 0 ||
	    (start_offset+size > cam->frame_size*cam->num_frames))
		return -EINVAL;

	pos = ((unsigned long) (cam->frame_buffer)) + start_offset;
	while (size > 0) {
		page = kvirt_to_pa(pos);
		if (remap_pfn_range(vma, start, page >> PAGE_SHIFT, PAGE_SIZE, PAGE_SHARED))
			return -EAGAIN;
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	cam->mmapped = true;
	return 0;
}
