/*
 *
 *
 *  Copyright (C) 2005 Mike Isely <isely@pobox.com>
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

#include <linux/i2c.h>
#include <linux/module.h>
#include <media/ir-kbd-i2c.h>
#include "pvrusb2-i2c-core.h"
#include "pvrusb2-hdw-internal.h"
#include "pvrusb2-debug.h"
#include "pvrusb2-fx2-cmd.h"
#include "pvrusb2.h"

#define trace_i2c(...) pvr2_trace(PVR2_TRACE_I2C,__VA_ARGS__)


static unsigned int i2c_scan;
module_param(i2c_scan, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(i2c_scan,"scan i2c bus at insmod time");

static int ir_mode[PVR_NUM] = { [0 ... PVR_NUM-1] = 1 };
module_param_array(ir_mode, int, NULL, 0444);
MODULE_PARM_DESC(ir_mode,"specify: 0=disable IR reception, 1=normal IR");

static int pvr2_disable_ir_video;
module_param_named(disable_autoload_ir_video, pvr2_disable_ir_video,
		   int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(disable_autoload_ir_video,
		 "1=do not try to autoload ir_video IR receiver");

static int pvr2_i2c_write(struct pvr2_hdw *hdw, 
			  u8 i2c_addr,      
			  u8 *data,         
			  u16 length)       
{
	
	int ret;


	if (!data) length = 0;
	if (length > (sizeof(hdw->cmd_buffer) - 3)) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Killing an I2C write to %u that is too large"
			   " (desired=%u limit=%u)",
			   i2c_addr,
			   length,(unsigned int)(sizeof(hdw->cmd_buffer) - 3));
		return -ENOTSUPP;
	}

	LOCK_TAKE(hdw->ctl_lock);

	
	memset(hdw->cmd_buffer, 0, sizeof(hdw->cmd_buffer));

	
	hdw->cmd_buffer[0] = FX2CMD_I2C_WRITE;      
	hdw->cmd_buffer[1] = i2c_addr;  
	hdw->cmd_buffer[2] = length;    
	if (length) memcpy(hdw->cmd_buffer + 3, data, length);

	
	ret = pvr2_send_request(hdw,
				hdw->cmd_buffer,
				length + 3,
				hdw->cmd_buffer,
				1);
	if (!ret) {
		if (hdw->cmd_buffer[0] != 8) {
			ret = -EIO;
			if (hdw->cmd_buffer[0] != 7) {
				trace_i2c("unexpected status"
					  " from i2_write[%d]: %d",
					  i2c_addr,hdw->cmd_buffer[0]);
			}
		}
	}

	LOCK_GIVE(hdw->ctl_lock);

	return ret;
}

static int pvr2_i2c_read(struct pvr2_hdw *hdw, 
			 u8 i2c_addr,       
			 u8 *data,          
			 u16 dlen,          
			 u8 *res,           
			 u16 rlen)          
{
	
	int ret;


	if (!data) dlen = 0;
	if (dlen > (sizeof(hdw->cmd_buffer) - 4)) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Killing an I2C read to %u that has wlen too large"
			   " (desired=%u limit=%u)",
			   i2c_addr,
			   dlen,(unsigned int)(sizeof(hdw->cmd_buffer) - 4));
		return -ENOTSUPP;
	}
	if (res && (rlen > (sizeof(hdw->cmd_buffer) - 1))) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Killing an I2C read to %u that has rlen too large"
			   " (desired=%u limit=%u)",
			   i2c_addr,
			   rlen,(unsigned int)(sizeof(hdw->cmd_buffer) - 1));
		return -ENOTSUPP;
	}

	LOCK_TAKE(hdw->ctl_lock);

	
	memset(hdw->cmd_buffer, 0, sizeof(hdw->cmd_buffer));

	
	hdw->cmd_buffer[0] = FX2CMD_I2C_READ;  
	hdw->cmd_buffer[1] = dlen;  
	hdw->cmd_buffer[2] = rlen;  
	hdw->cmd_buffer[3] = i2c_addr;  
	if (dlen) memcpy(hdw->cmd_buffer + 4, data, dlen);

	
	ret = pvr2_send_request(hdw,
				hdw->cmd_buffer,
				4 + dlen,
				hdw->cmd_buffer,
				rlen + 1);
	if (!ret) {
		if (hdw->cmd_buffer[0] != 8) {
			ret = -EIO;
			if (hdw->cmd_buffer[0] != 7) {
				trace_i2c("unexpected status"
					  " from i2_read[%d]: %d",
					  i2c_addr,hdw->cmd_buffer[0]);
			}
		}
	}

	
	if (res && rlen) {
		if (ret) {
			
			memset(res, 0, rlen);
		} else {
			memcpy(res, hdw->cmd_buffer + 1, rlen);
		}
	}

	LOCK_GIVE(hdw->ctl_lock);

	return ret;
}

static int pvr2_i2c_basic_op(struct pvr2_hdw *hdw,
			     u8 i2c_addr,
			     u8 *wdata,
			     u16 wlen,
			     u8 *rdata,
			     u16 rlen)
{
	if (!rdata) rlen = 0;
	if (!wdata) wlen = 0;
	if (rlen || !wlen) {
		return pvr2_i2c_read(hdw,i2c_addr,wdata,wlen,rdata,rlen);
	} else {
		return pvr2_i2c_write(hdw,i2c_addr,wdata,wlen);
	}
}


static int i2c_24xxx_ir(struct pvr2_hdw *hdw,
			u8 i2c_addr,u8 *wdata,u16 wlen,u8 *rdata,u16 rlen)
{
	u8 dat[4];
	unsigned int stat;

	if (!(rlen || wlen)) {
		
		return 0;
	}

	
	if ((wlen != 0) || (rlen == 0)) return -EIO;

	if (rlen < 3) {
		if (rlen > 0) rdata[0] = 0;
		if (rlen > 1) rdata[1] = 0;
		return 0;
	}

	
	LOCK_TAKE(hdw->ctl_lock); do {
		hdw->cmd_buffer[0] = FX2CMD_GET_IR_CODE;
		stat = pvr2_send_request(hdw,
					 hdw->cmd_buffer,1,
					 hdw->cmd_buffer,4);
		dat[0] = hdw->cmd_buffer[0];
		dat[1] = hdw->cmd_buffer[1];
		dat[2] = hdw->cmd_buffer[2];
		dat[3] = hdw->cmd_buffer[3];
	} while (0); LOCK_GIVE(hdw->ctl_lock);

	
	if (stat != 0) return stat;

	rdata[2] = 0xc1;
	if (dat[0] != 1) {
		
		rdata[0] = 0;
		rdata[1] = 0;
	} else {
		u16 val;
		val = dat[1];
		val <<= 8;
		val |= dat[2];
		val >>= 1;
		val &= ~0x0003;
		val |= 0x8000;
		rdata[0] = (val >> 8) & 0xffu;
		rdata[1] = val & 0xffu;
	}

	return 0;
}

static int i2c_hack_wm8775(struct pvr2_hdw *hdw,
			   u8 i2c_addr,u8 *wdata,u16 wlen,u8 *rdata,u16 rlen)
{
	if (!(rlen || wlen)) {
		
		return 0;
	}
	return pvr2_i2c_basic_op(hdw,i2c_addr,wdata,wlen,rdata,rlen);
}

static int i2c_black_hole(struct pvr2_hdw *hdw,
			   u8 i2c_addr,u8 *wdata,u16 wlen,u8 *rdata,u16 rlen)
{
	return -EIO;
}

static int i2c_hack_cx25840(struct pvr2_hdw *hdw,
			    u8 i2c_addr,u8 *wdata,u16 wlen,u8 *rdata,u16 rlen)
{
	int ret;
	unsigned int subaddr;
	u8 wbuf[2];
	int state = hdw->i2c_cx25840_hack_state;

	if (!(rlen || wlen)) {
		
		
		
		return 0;
	}

	if (state == 3) {
		return pvr2_i2c_basic_op(hdw,i2c_addr,wdata,wlen,rdata,rlen);
	}


	if (wlen == 0) {
		switch (state) {
		case 1: subaddr = 0x0100; break;
		case 2: subaddr = 0x0101; break;
		default: goto fail;
		}
	} else if (wlen == 2) {
		subaddr = (wdata[0] << 8) | wdata[1];
		switch (subaddr) {
		case 0x0100: state = 1; break;
		case 0x0101: state = 2; break;
		default: goto fail;
		}
	} else {
		goto fail;
	}
	if (!rlen) goto success;
	state = 0;
	if (rlen != 1) goto fail;

	wbuf[0] = subaddr >> 8;
	wbuf[1] = subaddr;
	ret = pvr2_i2c_basic_op(hdw,i2c_addr,wbuf,2,rdata,rlen);

	if ((ret != 0) || (*rdata == 0x04) || (*rdata == 0x0a)) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "WARNING: Detected a wedged cx25840 chip;"
			   " the device will not work.");
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "WARNING: Try power cycling the pvrusb2 device.");
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "WARNING: Disabling further access to the device"
			   " to prevent other foul-ups.");
		
		hdw->i2c_func[0x44] = NULL;
		pvr2_hdw_render_useless(hdw);
		goto fail;
	}

	
	pvr2_trace(PVR2_TRACE_CHIPS,"cx25840 appears to be OK.");
	state = 3;

 success:
	hdw->i2c_cx25840_hack_state = state;
	return 0;

 fail:
	hdw->i2c_cx25840_hack_state = state;
	return -EIO;
}

static int pvr2_i2c_xfer(struct i2c_adapter *i2c_adap,
			 struct i2c_msg msgs[],
			 int num)
{
	int ret = -ENOTSUPP;
	pvr2_i2c_func funcp = NULL;
	struct pvr2_hdw *hdw = (struct pvr2_hdw *)(i2c_adap->algo_data);

	if (!num) {
		ret = -EINVAL;
		goto done;
	}
	if (msgs[0].addr < PVR2_I2C_FUNC_CNT) {
		funcp = hdw->i2c_func[msgs[0].addr];
	}
	if (!funcp) {
		ret = -EIO;
		goto done;
	}

	if (num == 1) {
		if (msgs[0].flags & I2C_M_RD) {
			
			u16 tcnt,bcnt,offs;
			if (!msgs[0].len) {
				
				if (funcp(hdw,msgs[0].addr,NULL,0,NULL,0)) {
					ret = -EIO;
					goto done;
				}
				ret = 1;
				goto done;
			}
			tcnt = msgs[0].len;
			offs = 0;
			while (tcnt) {
				bcnt = tcnt;
				if (bcnt > sizeof(hdw->cmd_buffer)-1) {
					bcnt = sizeof(hdw->cmd_buffer)-1;
				}
				if (funcp(hdw,msgs[0].addr,NULL,0,
					  msgs[0].buf+offs,bcnt)) {
					ret = -EIO;
					goto done;
				}
				offs += bcnt;
				tcnt -= bcnt;
			}
			ret = 1;
			goto done;
		} else {
			
			ret = 1;
			if (funcp(hdw,msgs[0].addr,
				  msgs[0].buf,msgs[0].len,NULL,0)) {
				ret = -EIO;
			}
			goto done;
		}
	} else if (num == 2) {
		if (msgs[0].addr != msgs[1].addr) {
			trace_i2c("i2c refusing 2 phase transfer with"
				  " conflicting target addresses");
			ret = -ENOTSUPP;
			goto done;
		}
		if ((!((msgs[0].flags & I2C_M_RD))) &&
		    (msgs[1].flags & I2C_M_RD)) {
			u16 tcnt,bcnt,wcnt,offs;
			tcnt = msgs[1].len;
			wcnt = msgs[0].len;
			offs = 0;
			while (tcnt || wcnt) {
				bcnt = tcnt;
				if (bcnt > sizeof(hdw->cmd_buffer)-1) {
					bcnt = sizeof(hdw->cmd_buffer)-1;
				}
				if (funcp(hdw,msgs[0].addr,
					  msgs[0].buf,wcnt,
					  msgs[1].buf+offs,bcnt)) {
					ret = -EIO;
					goto done;
				}
				offs += bcnt;
				tcnt -= bcnt;
				wcnt = 0;
			}
			ret = 2;
			goto done;
		} else {
			trace_i2c("i2c refusing complex transfer"
				  " read0=%d read1=%d",
				  (msgs[0].flags & I2C_M_RD),
				  (msgs[1].flags & I2C_M_RD));
		}
	} else {
		trace_i2c("i2c refusing %d phase transfer",num);
	}

 done:
	if (pvrusb2_debug & PVR2_TRACE_I2C_TRAF) {
		unsigned int idx,offs,cnt;
		for (idx = 0; idx < num; idx++) {
			cnt = msgs[idx].len;
			printk(KERN_INFO
			       "pvrusb2 i2c xfer %u/%u:"
			       " addr=0x%x len=%d %s",
			       idx+1,num,
			       msgs[idx].addr,
			       cnt,
			       (msgs[idx].flags & I2C_M_RD ?
				"read" : "write"));
			if ((ret > 0) || !(msgs[idx].flags & I2C_M_RD)) {
				if (cnt > 8) cnt = 8;
				printk(" [");
				for (offs = 0; offs < (cnt>8?8:cnt); offs++) {
					if (offs) printk(" ");
					printk("%02x",msgs[idx].buf[offs]);
				}
				if (offs < cnt) printk(" ...");
				printk("]");
			}
			if (idx+1 == num) {
				printk(" result=%d",ret);
			}
			printk("\n");
		}
		if (!num) {
			printk(KERN_INFO
			       "pvrusb2 i2c xfer null transfer result=%d\n",
			       ret);
		}
	}
	return ret;
}

static u32 pvr2_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_EMUL | I2C_FUNC_I2C;
}

static struct i2c_algorithm pvr2_i2c_algo_template = {
	.master_xfer   = pvr2_i2c_xfer,
	.functionality = pvr2_i2c_functionality,
};

static struct i2c_adapter pvr2_i2c_adap_template = {
	.owner         = THIS_MODULE,
	.class	       = 0,
};


static int do_i2c_probe(struct pvr2_hdw *hdw, int addr)
{
	struct i2c_msg msg[1];
	int rc;
	msg[0].addr = 0;
	msg[0].flags = I2C_M_RD;
	msg[0].len = 0;
	msg[0].buf = NULL;
	msg[0].addr = addr;
	rc = i2c_transfer(&hdw->i2c_adap, msg, ARRAY_SIZE(msg));
	return rc == 1;
}

static void do_i2c_scan(struct pvr2_hdw *hdw)
{
	int i;
	printk(KERN_INFO "%s: i2c scan beginning\n", hdw->name);
	for (i = 0; i < 128; i++) {
		if (do_i2c_probe(hdw, i)) {
			printk(KERN_INFO "%s: i2c scan: found device @ 0x%x\n",
			       hdw->name, i);
		}
	}
	printk(KERN_INFO "%s: i2c scan done.\n", hdw->name);
}

static void pvr2_i2c_register_ir(struct pvr2_hdw *hdw)
{
	struct i2c_board_info info;
	struct IR_i2c_init_data *init_data = &hdw->ir_init_data;
	if (pvr2_disable_ir_video) {
		pvr2_trace(PVR2_TRACE_INFO,
			   "Automatic binding of ir_video has been disabled.");
		return;
	}
	memset(&info, 0, sizeof(struct i2c_board_info));
	switch (hdw->ir_scheme_active) {
	case PVR2_IR_SCHEME_24XXX: 
	case PVR2_IR_SCHEME_29XXX: 
		init_data->ir_codes              = RC_MAP_HAUPPAUGE;
		init_data->internal_get_key_func = IR_KBD_GET_KEY_HAUP;
		init_data->type                  = RC_TYPE_RC5;
		init_data->name                  = hdw->hdw_desc->description;
		init_data->polling_interval      = 100; 
		
		info.addr          = 0x18;
		info.platform_data = init_data;
		strlcpy(info.type, "ir_video", I2C_NAME_SIZE);
		pvr2_trace(PVR2_TRACE_INFO, "Binding %s to i2c address 0x%02x.",
			   info.type, info.addr);
		i2c_new_device(&hdw->i2c_adap, &info);
		break;
	case PVR2_IR_SCHEME_ZILOG:     
	case PVR2_IR_SCHEME_24XXX_MCE: 
		init_data->ir_codes              = RC_MAP_HAUPPAUGE;
		init_data->internal_get_key_func = IR_KBD_GET_KEY_HAUP_XVR;
		init_data->type                  = RC_TYPE_RC5;
		init_data->name                  = hdw->hdw_desc->description;
		
		info.addr          = 0x71;
		info.platform_data = init_data;
		strlcpy(info.type, "ir_rx_z8f0811_haup", I2C_NAME_SIZE);
		pvr2_trace(PVR2_TRACE_INFO, "Binding %s to i2c address 0x%02x.",
			   info.type, info.addr);
		i2c_new_device(&hdw->i2c_adap, &info);
		
		info.addr          = 0x70;
		info.platform_data = init_data;
		strlcpy(info.type, "ir_tx_z8f0811_haup", I2C_NAME_SIZE);
		pvr2_trace(PVR2_TRACE_INFO, "Binding %s to i2c address 0x%02x.",
			   info.type, info.addr);
		i2c_new_device(&hdw->i2c_adap, &info);
		break;
	default:
		break;
	}
}

void pvr2_i2c_core_init(struct pvr2_hdw *hdw)
{
	unsigned int idx;

	for (idx = 0; idx < PVR2_I2C_FUNC_CNT; idx++) {
		hdw->i2c_func[idx] = pvr2_i2c_basic_op;
	}

	
	if (ir_mode[hdw->unit_number] == 0) {
		printk(KERN_INFO "%s: IR disabled\n",hdw->name);
		hdw->i2c_func[0x18] = i2c_black_hole;
	} else if (ir_mode[hdw->unit_number] == 1) {
		if (hdw->ir_scheme_active == PVR2_IR_SCHEME_24XXX) {
			hdw->i2c_func[0x18] = i2c_24xxx_ir;
		}
	}
	if (hdw->hdw_desc->flag_has_cx25840) {
		hdw->i2c_func[0x44] = i2c_hack_cx25840;
	}
	if (hdw->hdw_desc->flag_has_wm8775) {
		hdw->i2c_func[0x1b] = i2c_hack_wm8775;
	}

	
	memcpy(&hdw->i2c_adap,&pvr2_i2c_adap_template,sizeof(hdw->i2c_adap));
	memcpy(&hdw->i2c_algo,&pvr2_i2c_algo_template,sizeof(hdw->i2c_algo));
	strlcpy(hdw->i2c_adap.name,hdw->name,sizeof(hdw->i2c_adap.name));
	hdw->i2c_adap.dev.parent = &hdw->usb_dev->dev;
	hdw->i2c_adap.algo = &hdw->i2c_algo;
	hdw->i2c_adap.algo_data = hdw;
	hdw->i2c_linked = !0;
	i2c_set_adapdata(&hdw->i2c_adap, &hdw->v4l2_dev);
	i2c_add_adapter(&hdw->i2c_adap);
	if (hdw->i2c_func[0x18] == i2c_24xxx_ir) {
		if (do_i2c_probe(hdw, 0x71)) {
			pvr2_trace(PVR2_TRACE_INFO,
				   "Device has newer IR hardware;"
				   " disabling unneeded virtual IR device");
			hdw->i2c_func[0x18] = NULL;
			
			hdw->ir_scheme_active = PVR2_IR_SCHEME_24XXX_MCE;
		}
	}
	if (i2c_scan) do_i2c_scan(hdw);

	pvr2_i2c_register_ir(hdw);
}

void pvr2_i2c_core_done(struct pvr2_hdw *hdw)
{
	if (hdw->i2c_linked) {
		i2c_del_adapter(&hdw->i2c_adap);
		hdw->i2c_linked = 0;
	}
}

