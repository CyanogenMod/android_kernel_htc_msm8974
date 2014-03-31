#define DRIVER_VERSION "v0.6"
#define DRIVER_AUTHOR "Bernd Porr, BerndPorr@f2s.com"
#define DRIVER_DESC "Stirling/ITL USB-DUX SIGMA -- Bernd.Porr@f2s.com"
/*
   comedi/drivers/usbdux.c
   Copyright (C) 2011 Bernd Porr, Bernd.Porr@f2s.com

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/usb.h>
#include <linux/fcntl.h>
#include <linux/compiler.h>
#include <linux/firmware.h>
#include "comedi_fc.h"
#include "../comedidev.h"

#define BOARDNAME "usbduxsigma"

#define BULK_TIMEOUT 1000

#define USBDUXSUB_FIRMWARE 0xA0
#define VENDOR_DIR_IN  0xC0
#define VENDOR_DIR_OUT 0x40

#define USBDUXSUB_CPUCS 0xE600

#define USBDUXSUB_MINOR 32

#define TB_LEN 0x2000

#define ISOINEP           6

#define ISOOUTEP          2

#define COMMAND_OUT_EP     1

#define COMMAND_IN_EP        8

#define PWM_EP         4

#define MIN_PWM_PERIOD  ((long)(1E9/300))

#define PWM_DEFAULT_PERIOD ((long)(1E9/100))

#define NUMCHANNELS 16

#define SIZEADIN          ((sizeof(int32_t)))

#define SIZEINBUF         (((NUMCHANNELS+1)*SIZEADIN))

#define SIZEINSNBUF       16

#define NUMOUTCHANNELS    8

#define SIZEDAOUT          ((sizeof(uint8_t)+sizeof(int16_t)))

#define SIZEOUTBUF         ((8*SIZEDAOUT))

#define SIZEOFDUXBUFFER    ((8*SIZEDAOUT+2))

#define NUMOFINBUFFERSFULL     5

#define NUMOFOUTBUFFERSFULL    5

#define NUMOFINBUFFERSHIGH     10

#define NUMOFOUTBUFFERSHIGH    10

#define NUMUSBDUX             16

#define SUBDEV_AD             0

#define SUBDEV_DA             1

#define SUBDEV_DIO            2

#define SUBDEV_PWM            3

#define RETRIES 10

static const struct comedi_lrange range_usbdux_ai_range = { 1, {
								BIP_RANGE
								(2.65/2.0)
								}
};

static const struct comedi_lrange range_usbdux_ao_range = { 1, {
								UNI_RANGE
								(2.5),
							       }
};


struct usbduxsub {
	
	int attached;
	
	int probed;
	
	struct usb_device *usbdev;
	
	int numOfInBuffers;
	
	int numOfOutBuffers;
	
	struct urb **urbIn;
	struct urb **urbOut;
	
	struct urb *urbPwm;
	
	unsigned int pwmPeriod;
	
	uint8_t pwmDelay;
	
	int sizePwmBuf;
	
	int32_t *inBuffer;
	
	int8_t *insnBuffer;
	
	int16_t *outBuffer;
	
	int ifnum;
	
	struct usb_interface *interface;
	
	struct comedi_device *comedidev;
	
	short int high_speed;
	
	short int ai_cmd_running;
	short int ao_cmd_running;
	
	short int pwm_cmd_running;
	
	short int ai_continuous;
	short int ao_continuous;
	
	int ai_sample_count;
	int ao_sample_count;
	
	unsigned int ai_timer;
	unsigned int ao_timer;
	
	unsigned int ai_counter;
	unsigned int ao_counter;
	
	unsigned int ai_interval;
	
	uint8_t *dac_commands;
	
	uint8_t *dux_commands;
	struct semaphore sem;
};

static struct usbduxsub usbduxsub[NUMUSBDUX];

static DEFINE_SEMAPHORE(start_stop_sem);

static int usbduxsub_unlink_InURBs(struct usbduxsub *usbduxsub_tmp)
{
	int i = 0;
	int err = 0;

	if (usbduxsub_tmp && usbduxsub_tmp->urbIn) {
		for (i = 0; i < usbduxsub_tmp->numOfInBuffers; i++) {
			if (usbduxsub_tmp->urbIn[i]) {
				usb_kill_urb(usbduxsub_tmp->urbIn[i]);
			}
			dev_dbg(&usbduxsub_tmp->interface->dev,
				"comedi: usbdux: unlinked InURB %d, err=%d\n",
				i, err);
		}
	}
	return err;
}

static int usbdux_ai_stop(struct usbduxsub *this_usbduxsub, int do_unlink)
{
	int ret = 0;

	if (!this_usbduxsub) {
		pr_err("comedi?: usbdux_ai_stop: this_usbduxsub=NULL!\n");
		return -EFAULT;
	}
	dev_dbg(&this_usbduxsub->interface->dev, "comedi: usbdux_ai_stop\n");

	if (do_unlink) {
		
		ret = usbduxsub_unlink_InURBs(this_usbduxsub);
	}

	this_usbduxsub->ai_cmd_running = 0;

	return ret;
}

static int usbdux_ai_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	struct usbduxsub *this_usbduxsub;
	int res = 0;

	
	this_usbduxsub = dev->private;
	if (!this_usbduxsub)
		return -EFAULT;

	dev_dbg(&this_usbduxsub->interface->dev, "comedi: usbdux_ai_cancel\n");

	
	down(&this_usbduxsub->sem);
	if (!(this_usbduxsub->probed)) {
		up(&this_usbduxsub->sem);
		return -ENODEV;
	}
	
	res = usbdux_ai_stop(this_usbduxsub, this_usbduxsub->ai_cmd_running);
	up(&this_usbduxsub->sem);
	return res;
}

static void usbduxsub_ai_IsocIrq(struct urb *urb)
{
	int i, err, n;
	struct usbduxsub *this_usbduxsub;
	struct comedi_device *this_comedidev;
	struct comedi_subdevice *s;
	int32_t v;
	unsigned int dio_state;

	
	this_comedidev = urb->context;
	
	this_usbduxsub = this_comedidev->private;
	
	s = this_comedidev->subdevices + SUBDEV_AD;

	
	switch (urb->status) {
	case 0:
		
		memcpy(this_usbduxsub->inBuffer,
		       urb->transfer_buffer, SIZEINBUF);
		break;
	case -EILSEQ:
		
		
		
		dev_dbg(&urb->dev->dev,
			"comedi%d: usbdux: CRC error in ISO IN stream.\n",
			this_usbduxsub->comedidev->minor);

		break;

	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
	case -ECONNABORTED:
		
		if (this_usbduxsub->ai_cmd_running) {
			
			
			s->async->events |= COMEDI_CB_EOA;
			s->async->events |= COMEDI_CB_ERROR;
			comedi_event(this_usbduxsub->comedidev, s);
			
			usbdux_ai_stop(this_usbduxsub, 0);
		}
		return;

	default:
		
		
		if (this_usbduxsub->ai_cmd_running) {
			dev_err(&urb->dev->dev,
				"Non-zero urb status received in ai intr "
				"context: %d\n", urb->status);
			s->async->events |= COMEDI_CB_EOA;
			s->async->events |= COMEDI_CB_ERROR;
			comedi_event(this_usbduxsub->comedidev, s);
			
			usbdux_ai_stop(this_usbduxsub, 0);
		}
		return;
	}

	if (unlikely((!(this_usbduxsub->ai_cmd_running)))) {
		return;
	}

	urb->dev = this_usbduxsub->usbdev;

	
	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (unlikely(err < 0)) {
		dev_err(&urb->dev->dev,
			"comedi_: urb resubmit failed in int-context!"
			"err=%d\n",
			err);
		if (err == -EL2NSYNC)
			dev_err(&urb->dev->dev,
				"buggy USB host controller or bug in IRQ "
				"handler!\n");
		s->async->events |= COMEDI_CB_EOA;
		s->async->events |= COMEDI_CB_ERROR;
		comedi_event(this_usbduxsub->comedidev, s);
		
		usbdux_ai_stop(this_usbduxsub, 0);
		return;
	}

	
	dio_state = be32_to_cpu(this_usbduxsub->inBuffer[0]);

	this_usbduxsub->ai_counter--;
	if (likely(this_usbduxsub->ai_counter > 0))
		return;

	
	this_usbduxsub->ai_counter = this_usbduxsub->ai_timer;

	
	if (!(this_usbduxsub->ai_continuous)) {
		
		this_usbduxsub->ai_sample_count--;
		
		if (this_usbduxsub->ai_sample_count < 0) {
			
			usbdux_ai_stop(this_usbduxsub, 0);
			
			s->async->events |= COMEDI_CB_EOA;
			comedi_event(this_usbduxsub->comedidev, s);
			return;
		}
	}
	
	n = s->async->cmd.chanlist_len;
	for (i = 0; i < n; i++) {
		
		v = be32_to_cpu(this_usbduxsub->inBuffer[i+1]);
		
		v = v & 0x00ffffff;
		
		v = v ^ 0x00800000;
		
		err = cfc_write_array_to_buffer(s, &v, sizeof(uint32_t));
		if (unlikely(err == 0)) {
			
			usbdux_ai_stop(this_usbduxsub, 0);
			return;
		}
	}
	
	s->async->events |= COMEDI_CB_BLOCK | COMEDI_CB_EOS;
	comedi_event(this_usbduxsub->comedidev, s);
}

static int usbduxsub_unlink_OutURBs(struct usbduxsub *usbduxsub_tmp)
{
	int i = 0;
	int err = 0;

	if (usbduxsub_tmp && usbduxsub_tmp->urbOut) {
		for (i = 0; i < usbduxsub_tmp->numOfOutBuffers; i++) {
			if (usbduxsub_tmp->urbOut[i])
				usb_kill_urb(usbduxsub_tmp->urbOut[i]);

			dev_dbg(&usbduxsub_tmp->interface->dev,
				"comedi: usbdux: unlinked OutURB %d: res=%d\n",
				i, err);
		}
	}
	return err;
}

static int usbdux_ao_stop(struct usbduxsub *this_usbduxsub, int do_unlink)
{
	int ret = 0;

	if (!this_usbduxsub)
		return -EFAULT;
	dev_dbg(&this_usbduxsub->interface->dev, "comedi: usbdux_ao_cancel\n");

	if (do_unlink)
		ret = usbduxsub_unlink_OutURBs(this_usbduxsub);

	this_usbduxsub->ao_cmd_running = 0;

	return ret;
}

static int usbdux_ao_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	struct usbduxsub *this_usbduxsub = dev->private;
	int res = 0;

	if (!this_usbduxsub)
		return -EFAULT;

	
	down(&this_usbduxsub->sem);
	if (!(this_usbduxsub->probed)) {
		up(&this_usbduxsub->sem);
		return -ENODEV;
	}
	
	res = usbdux_ao_stop(this_usbduxsub, this_usbduxsub->ao_cmd_running);
	up(&this_usbduxsub->sem);
	return res;
}

static void usbduxsub_ao_IsocIrq(struct urb *urb)
{
	int i, ret;
	uint8_t *datap;
	struct usbduxsub *this_usbduxsub;
	struct comedi_device *this_comedidev;
	struct comedi_subdevice *s;

	
	this_comedidev = urb->context;
	
	this_usbduxsub = this_comedidev->private;

	s = this_comedidev->subdevices + SUBDEV_DA;

	switch (urb->status) {
	case 0:
		
		break;

	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
	case -ECONNABORTED:
		
		
		if (this_usbduxsub->ao_cmd_running) {
			s->async->events |= COMEDI_CB_EOA;
			comedi_event(this_usbduxsub->comedidev, s);
			usbdux_ao_stop(this_usbduxsub, 0);
		}
		return;

	default:
		
		if (this_usbduxsub->ao_cmd_running) {
			dev_err(&urb->dev->dev,
				"comedi_: Non-zero urb status received in ao "
				"intr context: %d\n", urb->status);
			s->async->events |= COMEDI_CB_ERROR;
			s->async->events |= COMEDI_CB_EOA;
			comedi_event(this_usbduxsub->comedidev, s);
			
			usbdux_ao_stop(this_usbduxsub, 0);
		}
		return;
	}

	
	if (!(this_usbduxsub->ao_cmd_running))
		return;

	
	this_usbduxsub->ao_counter--;
	if ((int)this_usbduxsub->ao_counter <= 0) {
		
		this_usbduxsub->ao_counter = this_usbduxsub->ao_timer;

		
		if (!(this_usbduxsub->ao_continuous)) {
			
			this_usbduxsub->ao_sample_count--;
			if (this_usbduxsub->ao_sample_count < 0) {
				
				usbdux_ao_stop(this_usbduxsub, 0);
				s->async->events |= COMEDI_CB_EOA;
				comedi_event(this_usbduxsub->comedidev, s);
				
				return;
			}
		}
		
		((uint8_t *) (urb->transfer_buffer))[0] =
		    s->async->cmd.chanlist_len;
		for (i = 0; i < s->async->cmd.chanlist_len; i++) {
			short temp;
			if (i >= NUMOUTCHANNELS)
				break;

			
			datap =
			    (&(((uint8_t *) urb->transfer_buffer)[i * 2 + 1]));
			
			ret = comedi_buf_get(s->async, &temp);
			datap[0] = temp;
			datap[1] = this_usbduxsub->dac_commands[i];
			
			
			if (ret < 0) {
				dev_err(&urb->dev->dev,
					"comedi: buffer underflow\n");
				s->async->events |= COMEDI_CB_EOA;
				s->async->events |= COMEDI_CB_OVERFLOW;
			}
			
			s->async->events |= COMEDI_CB_BLOCK;
			comedi_event(this_usbduxsub->comedidev, s);
		}
	}
	urb->transfer_buffer_length = SIZEOUTBUF;
	urb->dev = this_usbduxsub->usbdev;
	urb->status = 0;
	if (this_usbduxsub->ao_cmd_running) {
		if (this_usbduxsub->high_speed) {
			
			urb->interval = 8;
		} else {
			
			urb->interval = 1;
		}
		urb->number_of_packets = 1;
		urb->iso_frame_desc[0].offset = 0;
		urb->iso_frame_desc[0].length = SIZEOUTBUF;
		urb->iso_frame_desc[0].status = 0;
		ret = usb_submit_urb(urb, GFP_ATOMIC);
		if (ret < 0) {
			dev_err(&urb->dev->dev,
				"comedi_: ao urb resubm failed in int-cont. "
				"ret=%d", ret);
			if (ret == EL2NSYNC)
				dev_err(&urb->dev->dev,
					"buggy USB host controller or bug in "
					"IRQ handling!\n");

			s->async->events |= COMEDI_CB_EOA;
			s->async->events |= COMEDI_CB_ERROR;
			comedi_event(this_usbduxsub->comedidev, s);
			
			usbdux_ao_stop(this_usbduxsub, 0);
		}
	}
}

static int usbduxsub_start(struct usbduxsub *usbduxsub)
{
	int errcode = 0;
	uint8_t local_transfer_buffer[16];

	
	local_transfer_buffer[0] = 0;
	errcode = usb_control_msg(usbduxsub->usbdev,
				  
				  usb_sndctrlpipe(usbduxsub->usbdev, 0),
				  
				  USBDUXSUB_FIRMWARE,
				  
				  VENDOR_DIR_OUT,
				  
				  USBDUXSUB_CPUCS,
				  
				  0x0000,
				  
				  local_transfer_buffer,
				  
				  1,
				  
				  BULK_TIMEOUT);
	if (errcode < 0) {
		dev_err(&usbduxsub->interface->dev,
			"comedi_: control msg failed (start)\n");
		return errcode;
	}
	return 0;
}

static int usbduxsub_stop(struct usbduxsub *usbduxsub)
{
	int errcode = 0;

	uint8_t local_transfer_buffer[16];

	
	local_transfer_buffer[0] = 1;
	errcode = usb_control_msg(usbduxsub->usbdev,
				  usb_sndctrlpipe(usbduxsub->usbdev, 0),
				  
				  USBDUXSUB_FIRMWARE,
				  
				  VENDOR_DIR_OUT,
				  
				  USBDUXSUB_CPUCS,
				  
				  0x0000, local_transfer_buffer,
				  
				  1,
				  
				  BULK_TIMEOUT);
	if (errcode < 0) {
		dev_err(&usbduxsub->interface->dev,
			"comedi_: control msg failed (stop)\n");
		return errcode;
	}
	return 0;
}

static int usbduxsub_upload(struct usbduxsub *usbduxsub,
			    uint8_t *local_transfer_buffer,
			    unsigned int startAddr, unsigned int len)
{
	int errcode;

	errcode = usb_control_msg(usbduxsub->usbdev,
				  usb_sndctrlpipe(usbduxsub->usbdev, 0),
				  
				  USBDUXSUB_FIRMWARE,
				  
				  VENDOR_DIR_OUT,
				  
				  startAddr,
				  
				  0x0000,
				  
				  local_transfer_buffer,
				  
				  len,
				  
				  BULK_TIMEOUT);
	dev_dbg(&usbduxsub->interface->dev, "comedi_: result=%d\n", errcode);
	if (errcode < 0) {
		dev_err(&usbduxsub->interface->dev,
			"comedi_: upload failed\n");
		return errcode;
	}
	return 0;
}

#define FIRMWARE_MAX_LEN 0x4000

static int firmwareUpload(struct usbduxsub *usbduxsub,
			  const u8 *firmwareBinary, int sizeFirmware)
{
	int ret;
	uint8_t *fwBuf;

	if (!firmwareBinary)
		return 0;

	if (sizeFirmware > FIRMWARE_MAX_LEN) {
		dev_err(&usbduxsub->interface->dev,
			"usbduxsigma firmware binary it too large for FX2.\n");
		return -ENOMEM;
	}

	
	fwBuf = kmemdup(firmwareBinary, sizeFirmware, GFP_KERNEL);
	if (!fwBuf) {
		dev_err(&usbduxsub->interface->dev,
			"comedi_: mem alloc for firmware failed\n");
		return -ENOMEM;
	}

	ret = usbduxsub_stop(usbduxsub);
	if (ret < 0) {
		dev_err(&usbduxsub->interface->dev,
			"comedi_: can not stop firmware\n");
		kfree(fwBuf);
		return ret;
	}

	ret = usbduxsub_upload(usbduxsub, fwBuf, 0, sizeFirmware);
	if (ret < 0) {
		dev_err(&usbduxsub->interface->dev,
			"comedi_: firmware upload failed\n");
		kfree(fwBuf);
		return ret;
	}
	ret = usbduxsub_start(usbduxsub);
	if (ret < 0) {
		dev_err(&usbduxsub->interface->dev,
			"comedi_: can not start firmware\n");
		kfree(fwBuf);
		return ret;
	}
	kfree(fwBuf);
	return 0;
}

static int usbduxsub_submit_InURBs(struct usbduxsub *usbduxsub)
{
	int i, errFlag;

	if (!usbduxsub)
		return -EFAULT;

	
	for (i = 0; i < usbduxsub->numOfInBuffers; i++) {
		
		usbduxsub->urbIn[i]->interval = usbduxsub->ai_interval;
		usbduxsub->urbIn[i]->context = usbduxsub->comedidev;
		usbduxsub->urbIn[i]->dev = usbduxsub->usbdev;
		usbduxsub->urbIn[i]->status = 0;
		usbduxsub->urbIn[i]->transfer_flags = URB_ISO_ASAP;
		dev_dbg(&usbduxsub->interface->dev,
			"comedi%d: submitting in-urb[%d]: %p,%p intv=%d\n",
			usbduxsub->comedidev->minor, i,
			(usbduxsub->urbIn[i]->context),
			(usbduxsub->urbIn[i]->dev),
			(usbduxsub->urbIn[i]->interval));
		errFlag = usb_submit_urb(usbduxsub->urbIn[i], GFP_ATOMIC);
		if (errFlag) {
			dev_err(&usbduxsub->interface->dev,
				"comedi_: ai: usb_submit_urb(%d) error %d\n",
				i, errFlag);
			return errFlag;
		}
	}
	return 0;
}

static int usbduxsub_submit_OutURBs(struct usbduxsub *usbduxsub)
{
	int i, errFlag;

	if (!usbduxsub)
		return -EFAULT;

	for (i = 0; i < usbduxsub->numOfOutBuffers; i++) {
		dev_dbg(&usbduxsub->interface->dev,
			"comedi_: submitting out-urb[%d]\n", i);
		
		usbduxsub->urbOut[i]->context = usbduxsub->comedidev;
		usbduxsub->urbOut[i]->dev = usbduxsub->usbdev;
		usbduxsub->urbOut[i]->status = 0;
		usbduxsub->urbOut[i]->transfer_flags = URB_ISO_ASAP;
		errFlag = usb_submit_urb(usbduxsub->urbOut[i], GFP_ATOMIC);
		if (errFlag) {
			dev_err(&usbduxsub->interface->dev,
				"comedi_: ao: usb_submit_urb(%d) error %d\n",
				i, errFlag);
			return errFlag;
		}
	}
	return 0;
}

static int chanToInterval(int nChannels)
{
	if (nChannels <= 2)
		
		return 2;
	if (nChannels <= 8)
		
		return 4;
	
	return 8;
}

static int usbdux_ai_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_cmd *cmd)
{
	int err = 0, tmp, i;
	unsigned int tmpTimer;
	struct usbduxsub *this_usbduxsub = dev->private;

	if (!(this_usbduxsub->probed))
		return -ENODEV;

	dev_dbg(&this_usbduxsub->interface->dev,
		"comedi%d: usbdux_ai_cmdtest\n", dev->minor);

	
	
	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW | TRIG_INT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	
	tmp = cmd->scan_begin_src;
	
	cmd->scan_begin_src &= TRIG_TIMER;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	
	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_NOW;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	
	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	
	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_COUNT | TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	if (cmd->scan_begin_src != TRIG_FOLLOW &&
	    cmd->scan_begin_src != TRIG_EXT &&
	    cmd->scan_begin_src != TRIG_TIMER)
		err++;
	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	
	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}

	if (cmd->scan_begin_src == TRIG_FOLLOW) {
		
		if (cmd->scan_begin_arg != 0) {
			cmd->scan_begin_arg = 0;
			err++;
		}
	}

	if (cmd->scan_begin_src == TRIG_TIMER) {
		if (this_usbduxsub->high_speed) {
			i = chanToInterval(cmd->chanlist_len);
			if (cmd->scan_begin_arg < (1000000 / 8 * i)) {
				cmd->scan_begin_arg = 1000000 / 8 * i;
				err++;
			}
			tmpTimer =
			    ((unsigned int)(cmd->scan_begin_arg / 125000)) *
			    125000;
			if (cmd->scan_begin_arg != tmpTimer) {
				cmd->scan_begin_arg = tmpTimer;
				err++;
			}
		} else {
			
			
			if (cmd->scan_begin_arg < 1000000) {
				cmd->scan_begin_arg = 1000000;
				err++;
			}
			tmpTimer = ((unsigned int)(cmd->scan_begin_arg /
						   1000000)) * 1000000;
			if (cmd->scan_begin_arg != tmpTimer) {
				cmd->scan_begin_arg = tmpTimer;
				err++;
			}
		}
	}
	
	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}

	if (cmd->stop_src == TRIG_COUNT) {
		
	} else {
		
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	return 0;
}

static void create_adc_command(unsigned int chan,
			       uint8_t *muxsg0,
			       uint8_t *muxsg1)
{
	if (chan < 8)
		(*muxsg0) = (*muxsg0) | (1 << chan);
	else if (chan < 16)
		(*muxsg1) = (*muxsg1) | (1 << (chan-8));
}



#define SENDADCOMMANDS            0
#define SENDDACOMMANDS            1
#define SENDDIOCONFIGCOMMAND      2
#define SENDDIOBITSCOMMAND        3
#define SENDSINGLEAD              4
#define SENDPWMON                 7
#define SENDPWMOFF                8

static int send_dux_commands(struct usbduxsub *this_usbduxsub, int cmd_type)
{
	int result, nsent;

	this_usbduxsub->dux_commands[0] = cmd_type;
#ifdef NOISY_DUX_DEBUGBUG
	printk(KERN_DEBUG "comedi%d: usbdux: dux_commands: ",
	       this_usbduxsub->comedidev->minor);
	for (result = 0; result < SIZEOFDUXBUFFER; result++)
		printk(" %02x", this_usbduxsub->dux_commands[result]);
	printk("\n");
#endif
	result = usb_bulk_msg(this_usbduxsub->usbdev,
			      usb_sndbulkpipe(this_usbduxsub->usbdev,
					      COMMAND_OUT_EP),
			      this_usbduxsub->dux_commands, SIZEOFDUXBUFFER,
			      &nsent, BULK_TIMEOUT);
	if (result < 0)
		dev_err(&this_usbduxsub->interface->dev, "comedi%d: "
			"could not transmit dux_command to the usb-device, "
			"err=%d\n", this_usbduxsub->comedidev->minor, result);

	return result;
}

static int receive_dux_commands(struct usbduxsub *this_usbduxsub, int command)
{
	int result = (-EFAULT);
	int nrec;
	int i;

	for (i = 0; i < RETRIES; i++) {
		result = usb_bulk_msg(this_usbduxsub->usbdev,
				      usb_rcvbulkpipe(this_usbduxsub->usbdev,
						      COMMAND_IN_EP),
				      this_usbduxsub->insnBuffer, SIZEINSNBUF,
				      &nrec, BULK_TIMEOUT);
		if (result < 0) {
			dev_err(&this_usbduxsub->interface->dev, "comedi%d: "
				"insn: USB error %d "
				"while receiving DUX command"
				"\n", this_usbduxsub->comedidev->minor,
				result);
			return result;
		}
		if (this_usbduxsub->insnBuffer[0] == command)
			return result;
	}
	dev_err(&this_usbduxsub->interface->dev, "comedi%d: insn: "
		"wrong data returned from firmware: want %d, got %d.\n",
		this_usbduxsub->comedidev->minor, command,
		this_usbduxsub->insnBuffer[0]);
	return -EFAULT;
}

static int usbdux_ai_inttrig(struct comedi_device *dev,
			     struct comedi_subdevice *s, unsigned int trignum)
{
	int ret;
	struct usbduxsub *this_usbduxsub = dev->private;
	if (!this_usbduxsub)
		return -EFAULT;

	down(&this_usbduxsub->sem);
	if (!(this_usbduxsub->probed)) {
		up(&this_usbduxsub->sem);
		return -ENODEV;
	}
	dev_dbg(&this_usbduxsub->interface->dev,
		"comedi%d: usbdux_ai_inttrig\n", dev->minor);

	if (trignum != 0) {
		dev_err(&this_usbduxsub->interface->dev,
			"comedi%d: usbdux_ai_inttrig: invalid trignum\n",
			dev->minor);
		up(&this_usbduxsub->sem);
		return -EINVAL;
	}
	if (!(this_usbduxsub->ai_cmd_running)) {
		this_usbduxsub->ai_cmd_running = 1;
		ret = usbduxsub_submit_InURBs(this_usbduxsub);
		if (ret < 0) {
			dev_err(&this_usbduxsub->interface->dev,
				"comedi%d: usbdux_ai_inttrig: "
				"urbSubmit: err=%d\n", dev->minor, ret);
			this_usbduxsub->ai_cmd_running = 0;
			up(&this_usbduxsub->sem);
			return ret;
		}
		s->async->inttrig = NULL;
	} else {
		dev_err(&this_usbduxsub->interface->dev,
			"comedi%d: ai_inttrig but acqu is already running\n",
			dev->minor);
	}
	up(&this_usbduxsub->sem);
	return 1;
}

static int usbdux_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned int chan;
	int i, ret;
	struct usbduxsub *this_usbduxsub = dev->private;
	int result;
	uint8_t muxsg0 = 0;
	uint8_t muxsg1 = 0;
	uint8_t sysred = 0;

	if (!this_usbduxsub)
		return -EFAULT;

	dev_dbg(&this_usbduxsub->interface->dev,
		"comedi%d: usbdux_ai_cmd\n", dev->minor);

	
	down(&this_usbduxsub->sem);

	if (!(this_usbduxsub->probed)) {
		up(&this_usbduxsub->sem);
		return -ENODEV;
	}
	if (this_usbduxsub->ai_cmd_running) {
		dev_err(&this_usbduxsub->interface->dev, "comedi%d: "
			"ai_cmd not possible. Another ai_cmd is running.\n",
			dev->minor);
		up(&this_usbduxsub->sem);
		return -EBUSY;
	}
	
	s->async->cur_chan = 0;

	
	this_usbduxsub->dux_commands[1] = cmd->chanlist_len;

	
	this_usbduxsub->dux_commands[2] = 0x12;

	
	this_usbduxsub->dux_commands[3] = 0x03;

	
	this_usbduxsub->dux_commands[4] = 0x00;

	for (i = 0; i < cmd->chanlist_len; i++) {
		chan = CR_CHAN(cmd->chanlist[i]);
		create_adc_command(chan, &muxsg0, &muxsg1);
		if (i >= NUMCHANNELS) {
			dev_err(&this_usbduxsub->interface->dev,
				"comedi%d: channel list too long\n",
				dev->minor);
			break;
		}
	}
	this_usbduxsub->dux_commands[5] = muxsg0;
	this_usbduxsub->dux_commands[6] = muxsg1;
	this_usbduxsub->dux_commands[7] = sysred;

	dev_dbg(&this_usbduxsub->interface->dev,
		"comedi %d: sending commands to the usb device: size=%u\n",
		dev->minor, NUMCHANNELS);

	result = send_dux_commands(this_usbduxsub, SENDADCOMMANDS);
	if (result < 0) {
		up(&this_usbduxsub->sem);
		return result;
	}

	if (this_usbduxsub->high_speed) {
		this_usbduxsub->ai_interval =
			chanToInterval(cmd->chanlist_len);
		this_usbduxsub->ai_timer = cmd->scan_begin_arg / (125000 *
							  (this_usbduxsub->
							   ai_interval));
	} else {
		
		this_usbduxsub->ai_interval = 1;
		this_usbduxsub->ai_timer = cmd->scan_begin_arg / 1000000;
	}
	if (this_usbduxsub->ai_timer < 1) {
		dev_err(&this_usbduxsub->interface->dev, "comedi%d: ai_cmd: "
			"timer=%d, scan_begin_arg=%d. "
			"Not properly tested by cmdtest?\n", dev->minor,
			this_usbduxsub->ai_timer, cmd->scan_begin_arg);
		up(&this_usbduxsub->sem);
		return -EINVAL;
	}
	this_usbduxsub->ai_counter = this_usbduxsub->ai_timer;

	if (cmd->stop_src == TRIG_COUNT) {
		
		this_usbduxsub->ai_sample_count = cmd->stop_arg;
		this_usbduxsub->ai_continuous = 0;
	} else {
		
		this_usbduxsub->ai_continuous = 1;
		this_usbduxsub->ai_sample_count = 0;
	}

	if (cmd->start_src == TRIG_NOW) {
		
		this_usbduxsub->ai_cmd_running = 1;
		ret = usbduxsub_submit_InURBs(this_usbduxsub);
		if (ret < 0) {
			this_usbduxsub->ai_cmd_running = 0;
			
			up(&this_usbduxsub->sem);
			return ret;
		}
		s->async->inttrig = NULL;
	} else {
		
		
		
		s->async->inttrig = usbdux_ai_inttrig;
	}
	up(&this_usbduxsub->sem);
	return 0;
}

static int usbdux_ai_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int32_t one = 0;
	int chan;
	int err;
	struct usbduxsub *this_usbduxsub = dev->private;
	uint8_t muxsg0 = 0;
	uint8_t muxsg1 = 0;
	uint8_t sysred = 0;

	if (!this_usbduxsub)
		return 0;

	dev_dbg(&this_usbduxsub->interface->dev,
		"comedi%d: ai_insn_read, insn->n=%d, insn->subdev=%d\n",
		dev->minor, insn->n, insn->subdev);

	down(&this_usbduxsub->sem);
	if (!(this_usbduxsub->probed)) {
		up(&this_usbduxsub->sem);
		return -ENODEV;
	}
	if (this_usbduxsub->ai_cmd_running) {
		dev_err(&this_usbduxsub->interface->dev,
			"comedi%d: ai_insn_read not possible. "
			"Async Command is running.\n", dev->minor);
		up(&this_usbduxsub->sem);
		return 0;
	}

	
	
	this_usbduxsub->dux_commands[1] = 0x16;

	
	this_usbduxsub->dux_commands[2] = 0x80;

	
	this_usbduxsub->dux_commands[3] = 0x00;

	chan = CR_CHAN(insn->chanspec);
	create_adc_command(chan, &muxsg0, &muxsg1);

	this_usbduxsub->dux_commands[4] = muxsg0;
	this_usbduxsub->dux_commands[5] = muxsg1;
	this_usbduxsub->dux_commands[6] = sysred;

	
	err = send_dux_commands(this_usbduxsub, SENDSINGLEAD);
	if (err < 0) {
		up(&this_usbduxsub->sem);
		return err;
	}

	for (i = 0; i < insn->n; i++) {
		err = receive_dux_commands(this_usbduxsub, SENDSINGLEAD);
		if (err < 0) {
			up(&this_usbduxsub->sem);
			return 0;
		}
		
		one = be32_to_cpu(*((int32_t *)
				    ((this_usbduxsub->insnBuffer)+1)));
		
		one = one & 0x00ffffff;
		
		one = one ^ 0x00800000;
		data[i] = one;
	}
	up(&this_usbduxsub->sem);
	return i;
}




static int usbdux_getstatusinfo(struct comedi_device *dev, int chan)
{
	struct usbduxsub *this_usbduxsub = dev->private;
	uint8_t sysred = 0;
	uint32_t one;
	int err;

	if (!this_usbduxsub)
		return 0;

	if (this_usbduxsub->ai_cmd_running) {
		dev_err(&this_usbduxsub->interface->dev,
			"comedi%d: status read not possible. "
			"Async Command is running.\n", dev->minor);
		return 0;
	}

	
	this_usbduxsub->dux_commands[1] = 0x12;

	
	this_usbduxsub->dux_commands[2] = 0x80;

	
	this_usbduxsub->dux_commands[3] = 0x00;

	if (chan == 1) {
		
		sysred = sysred | 1;
	} else if (chan == 2) {
		
		sysred = sysred | 4;
	} else if (chan == 3) {
		
		sysred = sysred | 8;
	} else if (chan == 4) {
		
		sysred = sysred | 16;
	} else if (chan == 5) {
		
		sysred = sysred | 32;
	}

	this_usbduxsub->dux_commands[4] = 0;
	this_usbduxsub->dux_commands[5] = 0;
	this_usbduxsub->dux_commands[6] = sysred;

	
	err = send_dux_commands(this_usbduxsub, SENDSINGLEAD);
	if (err < 0)
		return err;

	err = receive_dux_commands(this_usbduxsub, SENDSINGLEAD);
	if (err < 0)
		return err;

	
	one = be32_to_cpu(*((int32_t *)((this_usbduxsub->insnBuffer)+1)));
	
	one = one & 0x00ffffff;
	one = one ^ 0x00800000;

	return (int)one;
}







static int usbdux_ao_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);
	struct usbduxsub *this_usbduxsub = dev->private;

	if (!this_usbduxsub)
		return -EFAULT;

	down(&this_usbduxsub->sem);
	if (!(this_usbduxsub->probed)) {
		up(&this_usbduxsub->sem);
		return -ENODEV;
	}
	for (i = 0; i < insn->n; i++)
		data[i] = this_usbduxsub->outBuffer[chan];

	up(&this_usbduxsub->sem);
	return i;
}

static int usbdux_ao_insn_write(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	int i, err;
	int chan = CR_CHAN(insn->chanspec);
	struct usbduxsub *this_usbduxsub = dev->private;

	if (!this_usbduxsub)
		return -EFAULT;

	dev_dbg(&this_usbduxsub->interface->dev,
		"comedi%d: ao_insn_write\n", dev->minor);

	down(&this_usbduxsub->sem);
	if (!(this_usbduxsub->probed)) {
		up(&this_usbduxsub->sem);
		return -ENODEV;
	}
	if (this_usbduxsub->ao_cmd_running) {
		dev_err(&this_usbduxsub->interface->dev,
			"comedi%d: ao_insn_write: "
			"ERROR: asynchronous ao_cmd is running\n", dev->minor);
		up(&this_usbduxsub->sem);
		return 0;
	}

	for (i = 0; i < insn->n; i++) {
		dev_dbg(&this_usbduxsub->interface->dev,
			"comedi%d: ao_insn_write: data[chan=%d,i=%d]=%d\n",
			dev->minor, chan, i, data[i]);

		
		this_usbduxsub->dux_commands[1] = 1;
		
		this_usbduxsub->dux_commands[2] = data[i];
		this_usbduxsub->outBuffer[chan] = data[i];
		this_usbduxsub->dux_commands[3] = chan;
		err = send_dux_commands(this_usbduxsub, SENDDACOMMANDS);
		if (err < 0) {
			up(&this_usbduxsub->sem);
			return err;
		}
	}
	up(&this_usbduxsub->sem);

	return i;
}

static int usbdux_ao_inttrig(struct comedi_device *dev,
			     struct comedi_subdevice *s, unsigned int trignum)
{
	int ret;
	struct usbduxsub *this_usbduxsub = dev->private;

	if (!this_usbduxsub)
		return -EFAULT;

	down(&this_usbduxsub->sem);

	if (!(this_usbduxsub->probed)) {
		ret = -ENODEV;
		goto out;
	}
	if (trignum != 0) {
		dev_err(&this_usbduxsub->interface->dev,
			"comedi%d: usbdux_ao_inttrig: invalid trignum\n",
			dev->minor);
		ret = -EINVAL;
		goto out;
	}
	if (!(this_usbduxsub->ao_cmd_running)) {
		this_usbduxsub->ao_cmd_running = 1;
		ret = usbduxsub_submit_OutURBs(this_usbduxsub);
		if (ret < 0) {
			dev_err(&this_usbduxsub->interface->dev,
				"comedi%d: usbdux_ao_inttrig: submitURB: "
				"err=%d\n", dev->minor, ret);
			this_usbduxsub->ao_cmd_running = 0;
			goto out;
		}
		s->async->inttrig = NULL;
	} else {
		dev_err(&this_usbduxsub->interface->dev,
			"comedi%d: ao_inttrig but acqu is already running.\n",
			dev->minor);
	}
	ret = 1;
out:
	up(&this_usbduxsub->sem);
	return ret;
}

static int usbdux_ao_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_cmd *cmd)
{
	int err = 0, tmp;
	struct usbduxsub *this_usbduxsub = dev->private;

	if (!this_usbduxsub)
		return -EFAULT;

	if (!(this_usbduxsub->probed))
		return -ENODEV;

	dev_dbg(&this_usbduxsub->interface->dev,
		"comedi%d: usbdux_ao_cmdtest\n", dev->minor);

	
	
	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW | TRIG_INT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	
	tmp = cmd->scan_begin_src;
	
	
	if (0) {		
		
		
		cmd->scan_begin_src &= TRIG_FOLLOW;
	} else {
		
		cmd->scan_begin_src &= TRIG_TIMER;
	}
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	
	tmp = cmd->convert_src;

	
	cmd->convert_src &= TRIG_NOW;

	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	
	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	
	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_COUNT | TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	if (cmd->scan_begin_src != TRIG_FOLLOW &&
	    cmd->scan_begin_src != TRIG_EXT &&
	    cmd->scan_begin_src != TRIG_TIMER)
		err++;
	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	

	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}

	if (cmd->scan_begin_src == TRIG_FOLLOW) {
		
		if (cmd->scan_begin_arg != 0) {
			cmd->scan_begin_arg = 0;
			err++;
		}
	}

	if (cmd->scan_begin_src == TRIG_TIMER) {
		
		if (cmd->scan_begin_arg < 1000000) {
			cmd->scan_begin_arg = 1000000;
			err++;
		}
	}
	
	if (cmd->convert_src == TRIG_TIMER) {
		if (cmd->convert_arg < 125000) {
			cmd->convert_arg = 125000;
			err++;
		}
	}

	
	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}

	if (cmd->stop_src == TRIG_COUNT) {
		
	} else {
		
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	dev_dbg(&this_usbduxsub->interface->dev, "comedi%d: err=%d, "
		"scan_begin_src=%d, scan_begin_arg=%d, convert_src=%d, "
		"convert_arg=%d\n", dev->minor, err, cmd->scan_begin_src,
		cmd->scan_begin_arg, cmd->convert_src, cmd->convert_arg);

	if (err)
		return 3;

	return 0;
}

static int usbdux_ao_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned int chan, gain;
	int i, ret;
	struct usbduxsub *this_usbduxsub = dev->private;

	if (!this_usbduxsub)
		return -EFAULT;

	down(&this_usbduxsub->sem);
	if (!(this_usbduxsub->probed)) {
		up(&this_usbduxsub->sem);
		return -ENODEV;
	}
	dev_dbg(&this_usbduxsub->interface->dev,
		"comedi%d: %s\n", dev->minor, __func__);

	
	s->async->cur_chan = 0;
	for (i = 0; i < cmd->chanlist_len; ++i) {
		chan = CR_CHAN(cmd->chanlist[i]);
		gain = CR_RANGE(cmd->chanlist[i]);
		if (i >= NUMOUTCHANNELS) {
			dev_err(&this_usbduxsub->interface->dev,
				"comedi%d: %s: channel list too long\n",
				dev->minor, __func__);
			break;
		}
		this_usbduxsub->dac_commands[i] = chan;
		dev_dbg(&this_usbduxsub->interface->dev,
			"comedi%d: dac command for ch %d is %x\n",
			dev->minor, i, this_usbduxsub->dac_commands[i]);
	}

	
	
	if (0) {		
		
		
		this_usbduxsub->ao_timer = cmd->convert_arg / 125000;
	} else {
		
		
		this_usbduxsub->ao_timer = cmd->scan_begin_arg / 1000000;
		dev_dbg(&this_usbduxsub->interface->dev,
			"comedi%d: scan_begin_src=%d, scan_begin_arg=%d, "
			"convert_src=%d, convert_arg=%d\n", dev->minor,
			cmd->scan_begin_src, cmd->scan_begin_arg,
			cmd->convert_src, cmd->convert_arg);
		dev_dbg(&this_usbduxsub->interface->dev,
			"comedi%d: ao_timer=%d (ms)\n",
			dev->minor, this_usbduxsub->ao_timer);
		if (this_usbduxsub->ao_timer < 1) {
			dev_err(&this_usbduxsub->interface->dev,
				"comedi%d: usbdux: ao_timer=%d, "
				"scan_begin_arg=%d. "
				"Not properly tested by cmdtest?\n",
				dev->minor, this_usbduxsub->ao_timer,
				cmd->scan_begin_arg);
			up(&this_usbduxsub->sem);
			return -EINVAL;
		}
	}
	this_usbduxsub->ao_counter = this_usbduxsub->ao_timer;

	if (cmd->stop_src == TRIG_COUNT) {
		
		
		
		if (0) {	
			this_usbduxsub->ao_sample_count =
			    (cmd->stop_arg) * (cmd->scan_end_arg);
		} else {
			
			
			
			this_usbduxsub->ao_sample_count = cmd->stop_arg;
		}
		this_usbduxsub->ao_continuous = 0;
	} else {
		
		this_usbduxsub->ao_continuous = 1;
		this_usbduxsub->ao_sample_count = 0;
	}

	if (cmd->start_src == TRIG_NOW) {
		
		this_usbduxsub->ao_cmd_running = 1;
		ret = usbduxsub_submit_OutURBs(this_usbduxsub);
		if (ret < 0) {
			this_usbduxsub->ao_cmd_running = 0;
			
			up(&this_usbduxsub->sem);
			return ret;
		}
		s->async->inttrig = NULL;
	} else {
		
		
		
		s->async->inttrig = usbdux_ao_inttrig;
	}

	up(&this_usbduxsub->sem);
	return 0;
}

static int usbdux_dio_insn_config(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	int chan = CR_CHAN(insn->chanspec);


	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
		s->io_bits |= 1 << chan;	
		break;
	case INSN_CONFIG_DIO_INPUT:
		s->io_bits &= ~(1 << chan);
		break;
	case INSN_CONFIG_DIO_QUERY:
		data[1] =
		    (s->io_bits & (1 << chan)) ? COMEDI_OUTPUT : COMEDI_INPUT;
		break;
	default:
		return -EINVAL;
		break;
	}
	
	
	return insn->n;
}

static int usbdux_dio_insn_bits(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn,
				unsigned int *data)
{

	struct usbduxsub *this_usbduxsub = dev->private;
	int err;

	if (!this_usbduxsub)
		return -EFAULT;

	if (insn->n != 2)
		return -EINVAL;

	down(&this_usbduxsub->sem);

	if (!(this_usbduxsub->probed)) {
		up(&this_usbduxsub->sem);
		return -ENODEV;
	}

	s->state &= ~data[0];
	s->state |= data[0] & data[1];
	
	this_usbduxsub->dux_commands[1] = (s->io_bits) & 0x000000FF;
	this_usbduxsub->dux_commands[4] = (s->state) & 0x000000FF;
	this_usbduxsub->dux_commands[2] = ((s->io_bits) & 0x0000FF00) >> 8;
	this_usbduxsub->dux_commands[5] = ((s->state) & 0x0000FF00) >> 8;
	this_usbduxsub->dux_commands[3] = ((s->io_bits) & 0x00FF0000) >> 16;
	this_usbduxsub->dux_commands[6] = ((s->state) & 0x00FF0000) >> 16;

	
	
	err = send_dux_commands(this_usbduxsub, SENDDIOBITSCOMMAND);
	if (err < 0) {
		up(&this_usbduxsub->sem);
		return err;
	}
	err = receive_dux_commands(this_usbduxsub, SENDDIOBITSCOMMAND);
	if (err < 0) {
		up(&this_usbduxsub->sem);
		return err;
	}

	data[1] = (((unsigned int)(this_usbduxsub->insnBuffer[1]))&0xff) |
		((((unsigned int)(this_usbduxsub->insnBuffer[2]))&0xff) << 8) |
		((((unsigned int)(this_usbduxsub->insnBuffer[3]))&0xff) << 16);

	s->state = data[1];

	up(&this_usbduxsub->sem);
	return 2;
}


static int usbduxsub_unlink_PwmURBs(struct usbduxsub *usbduxsub_tmp)
{
	int err = 0;

	if (usbduxsub_tmp && usbduxsub_tmp->urbPwm) {
		if (usbduxsub_tmp->urbPwm)
			usb_kill_urb(usbduxsub_tmp->urbPwm);
		dev_dbg(&usbduxsub_tmp->interface->dev,
			"comedi: unlinked PwmURB: res=%d\n", err);
	}
	return err;
}

static int usbdux_pwm_stop(struct usbduxsub *this_usbduxsub, int do_unlink)
{
	int ret = 0;

	if (!this_usbduxsub)
		return -EFAULT;

	dev_dbg(&this_usbduxsub->interface->dev, "comedi: %s\n", __func__);
	if (do_unlink)
		ret = usbduxsub_unlink_PwmURBs(this_usbduxsub);

	this_usbduxsub->pwm_cmd_running = 0;

	return ret;
}

static int usbdux_pwm_cancel(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{
	struct usbduxsub *this_usbduxsub = dev->private;
	int res = 0;

	
	res = usbdux_pwm_stop(this_usbduxsub, this_usbduxsub->pwm_cmd_running);

	dev_dbg(&this_usbduxsub->interface->dev,
		"comedi %d: sending pwm off command to the usb device.\n",
		dev->minor);
	res = send_dux_commands(this_usbduxsub, SENDPWMOFF);
	if (res < 0)
		return res;

	return res;
}

static void usbduxsub_pwm_irq(struct urb *urb)
{
	int ret;
	struct usbduxsub *this_usbduxsub;
	struct comedi_device *this_comedidev;
	struct comedi_subdevice *s;

	

	
	this_comedidev = urb->context;
	
	this_usbduxsub = this_comedidev->private;

	s = this_comedidev->subdevices + SUBDEV_DA;

	switch (urb->status) {
	case 0:
		
		break;

	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
	case -ECONNABORTED:
		if (this_usbduxsub->pwm_cmd_running)
			usbdux_pwm_stop(this_usbduxsub, 0);

		return;

	default:
		
		if (this_usbduxsub->pwm_cmd_running) {
			dev_err(&this_usbduxsub->interface->dev,
				"comedi_: Non-zero urb status received in "
				"pwm intr context: %d\n", urb->status);
			usbdux_pwm_stop(this_usbduxsub, 0);
		}
		return;
	}

	
	if (!(this_usbduxsub->pwm_cmd_running))
		return;

	urb->transfer_buffer_length = this_usbduxsub->sizePwmBuf;
	urb->dev = this_usbduxsub->usbdev;
	urb->status = 0;
	if (this_usbduxsub->pwm_cmd_running) {
		ret = usb_submit_urb(urb, GFP_ATOMIC);
		if (ret < 0) {
			dev_err(&this_usbduxsub->interface->dev,
				"comedi_: pwm urb resubm failed in int-cont. "
				"ret=%d", ret);
			if (ret == EL2NSYNC)
				dev_err(&this_usbduxsub->interface->dev,
					"buggy USB host controller or bug in "
					"IRQ handling!\n");

			
			usbdux_pwm_stop(this_usbduxsub, 0);
		}
	}
}

static int usbduxsub_submit_PwmURBs(struct usbduxsub *usbduxsub)
{
	int errFlag;

	if (!usbduxsub)
		return -EFAULT;

	dev_dbg(&usbduxsub->interface->dev, "comedi_: submitting pwm-urb\n");

	
	usb_fill_bulk_urb(usbduxsub->urbPwm,
			  usbduxsub->usbdev,
			  usb_sndbulkpipe(usbduxsub->usbdev, PWM_EP),
			  usbduxsub->urbPwm->transfer_buffer,
			  usbduxsub->sizePwmBuf, usbduxsub_pwm_irq,
			  usbduxsub->comedidev);

	errFlag = usb_submit_urb(usbduxsub->urbPwm, GFP_ATOMIC);
	if (errFlag) {
		dev_err(&usbduxsub->interface->dev,
			"comedi_: usbduxsigma: pwm: usb_submit_urb error %d\n",
			errFlag);
		return errFlag;
	}
	return 0;
}

static int usbdux_pwm_period(struct comedi_device *dev,
			     struct comedi_subdevice *s, unsigned int period)
{
	struct usbduxsub *this_usbduxsub = dev->private;
	int fx2delay = 255;

	if (period < MIN_PWM_PERIOD) {
		dev_err(&this_usbduxsub->interface->dev,
			"comedi%d: illegal period setting for pwm.\n",
			dev->minor);
		return -EAGAIN;
	} else {
		fx2delay = period / ((int)(6 * 512 * (1.0 / 0.033))) - 6;
		if (fx2delay > 255) {
			dev_err(&this_usbduxsub->interface->dev,
				"comedi%d: period %d for pwm is too low.\n",
				dev->minor, period);
			return -EAGAIN;
		}
	}
	this_usbduxsub->pwmDelay = fx2delay;
	this_usbduxsub->pwmPeriod = period;
	dev_dbg(&this_usbduxsub->interface->dev, "%s: frequ=%d, period=%d\n",
		__func__, period, fx2delay);
	return 0;
}

static int usbdux_pwm_start(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	int ret, i;
	struct usbduxsub *this_usbduxsub = dev->private;

	dev_dbg(&this_usbduxsub->interface->dev, "comedi%d: %s\n",
		dev->minor, __func__);

	if (this_usbduxsub->pwm_cmd_running) {
		
		return 0;
	}

	this_usbduxsub->dux_commands[1] = ((uint8_t) this_usbduxsub->pwmDelay);
	ret = send_dux_commands(this_usbduxsub, SENDPWMON);
	if (ret < 0)
		return ret;

	
	for (i = 0; i < this_usbduxsub->sizePwmBuf; i++)
		((char *)(this_usbduxsub->urbPwm->transfer_buffer))[i] = 0;

	this_usbduxsub->pwm_cmd_running = 1;
	ret = usbduxsub_submit_PwmURBs(this_usbduxsub);
	if (ret < 0) {
		this_usbduxsub->pwm_cmd_running = 0;
		return ret;
	}
	return 0;
}

static int usbdux_pwm_pattern(struct comedi_device *dev,
			      struct comedi_subdevice *s, int channel,
			      unsigned int value, unsigned int sign)
{
	struct usbduxsub *this_usbduxsub = dev->private;
	int i, szbuf;
	char *pBuf;
	char pwm_mask;
	char sgn_mask;
	char c;

	if (!this_usbduxsub)
		return -EFAULT;

	
	pwm_mask = (1 << channel);
	
	sgn_mask = (16 << channel);
	
	
	szbuf = this_usbduxsub->sizePwmBuf;
	pBuf = (char *)(this_usbduxsub->urbPwm->transfer_buffer);
	for (i = 0; i < szbuf; i++) {
		c = *pBuf;
		
		c = c & (~pwm_mask);
		
		if (i < value)
			c = c | pwm_mask;
		
		if (!sign) {
			
			c = c & (~sgn_mask);
		} else {
			
			c = c | sgn_mask;
		}
		*(pBuf++) = c;
	}
	return 1;
}

static int usbdux_pwm_write(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn, unsigned int *data)
{
	struct usbduxsub *this_usbduxsub = dev->private;

	if (!this_usbduxsub)
		return -EFAULT;

	if ((insn->n) != 1) {
		return -EINVAL;
	}

	return usbdux_pwm_pattern(dev, s, CR_CHAN(insn->chanspec), data[0], 0);
}

static int usbdux_pwm_read(struct comedi_device *x1,
			   struct comedi_subdevice *x2, struct comedi_insn *x3,
			   unsigned int *x4)
{
	
	return -EINVAL;
};

static int usbdux_pwm_config(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data)
{
	struct usbduxsub *this_usbduxsub = dev->private;
	switch (data[0]) {
	case INSN_CONFIG_ARM:
		
		dev_dbg(&this_usbduxsub->interface->dev,
			"comedi%d: %s: pwm on\n", dev->minor, __func__);
		if (data[1] != 0)
			return -EINVAL;
		return usbdux_pwm_start(dev, s);
	case INSN_CONFIG_DISARM:
		dev_dbg(&this_usbduxsub->interface->dev,
			"comedi%d: %s: pwm off\n", dev->minor, __func__);
		return usbdux_pwm_cancel(dev, s);
	case INSN_CONFIG_GET_PWM_STATUS:
		data[1] = this_usbduxsub->pwm_cmd_running;
		return 0;
	case INSN_CONFIG_PWM_SET_PERIOD:
		dev_dbg(&this_usbduxsub->interface->dev,
			"comedi%d: %s: setting period\n", dev->minor,
			__func__);
		return usbdux_pwm_period(dev, s, data[1]);
	case INSN_CONFIG_PWM_GET_PERIOD:
		data[1] = this_usbduxsub->pwmPeriod;
		return 0;
	case INSN_CONFIG_PWM_SET_H_BRIDGE:
		return usbdux_pwm_pattern(dev, s,
					  
					  CR_CHAN(insn->chanspec),
					  
					  data[1],
					  
					  (data[2] != 0));
	case INSN_CONFIG_PWM_GET_H_BRIDGE:
		
		return -EINVAL;
	}
	return -EINVAL;
}


static void tidy_up(struct usbduxsub *usbduxsub_tmp)
{
	int i;

	if (!usbduxsub_tmp)
		return;
	dev_dbg(&usbduxsub_tmp->interface->dev, "comedi_: tiding up\n");

	
	if (usbduxsub_tmp->interface)
		usb_set_intfdata(usbduxsub_tmp->interface, NULL);

	usbduxsub_tmp->probed = 0;

	if (usbduxsub_tmp->urbIn) {
		if (usbduxsub_tmp->ai_cmd_running) {
			usbduxsub_tmp->ai_cmd_running = 0;
			usbduxsub_unlink_InURBs(usbduxsub_tmp);
		}
		for (i = 0; i < usbduxsub_tmp->numOfInBuffers; i++) {
			kfree(usbduxsub_tmp->urbIn[i]->transfer_buffer);
			usbduxsub_tmp->urbIn[i]->transfer_buffer = NULL;
			usb_kill_urb(usbduxsub_tmp->urbIn[i]);
			usb_free_urb(usbduxsub_tmp->urbIn[i]);
			usbduxsub_tmp->urbIn[i] = NULL;
		}
		kfree(usbduxsub_tmp->urbIn);
		usbduxsub_tmp->urbIn = NULL;
	}
	if (usbduxsub_tmp->urbOut) {
		if (usbduxsub_tmp->ao_cmd_running) {
			usbduxsub_tmp->ao_cmd_running = 0;
			usbduxsub_unlink_OutURBs(usbduxsub_tmp);
		}
		for (i = 0; i < usbduxsub_tmp->numOfOutBuffers; i++) {
			if (usbduxsub_tmp->urbOut[i]->transfer_buffer) {
				kfree(usbduxsub_tmp->
				      urbOut[i]->transfer_buffer);
				usbduxsub_tmp->urbOut[i]->transfer_buffer =
				    NULL;
			}
			if (usbduxsub_tmp->urbOut[i]) {
				usb_kill_urb(usbduxsub_tmp->urbOut[i]);
				usb_free_urb(usbduxsub_tmp->urbOut[i]);
				usbduxsub_tmp->urbOut[i] = NULL;
			}
		}
		kfree(usbduxsub_tmp->urbOut);
		usbduxsub_tmp->urbOut = NULL;
	}
	if (usbduxsub_tmp->urbPwm) {
		if (usbduxsub_tmp->pwm_cmd_running) {
			usbduxsub_tmp->pwm_cmd_running = 0;
			usbduxsub_unlink_PwmURBs(usbduxsub_tmp);
		}
		kfree(usbduxsub_tmp->urbPwm->transfer_buffer);
		usbduxsub_tmp->urbPwm->transfer_buffer = NULL;
		usb_kill_urb(usbduxsub_tmp->urbPwm);
		usb_free_urb(usbduxsub_tmp->urbPwm);
		usbduxsub_tmp->urbPwm = NULL;
	}
	kfree(usbduxsub_tmp->inBuffer);
	usbduxsub_tmp->inBuffer = NULL;
	kfree(usbduxsub_tmp->insnBuffer);
	usbduxsub_tmp->insnBuffer = NULL;
	kfree(usbduxsub_tmp->outBuffer);
	usbduxsub_tmp->outBuffer = NULL;
	kfree(usbduxsub_tmp->dac_commands);
	usbduxsub_tmp->dac_commands = NULL;
	kfree(usbduxsub_tmp->dux_commands);
	usbduxsub_tmp->dux_commands = NULL;
	usbduxsub_tmp->ai_cmd_running = 0;
	usbduxsub_tmp->ao_cmd_running = 0;
	usbduxsub_tmp->pwm_cmd_running = 0;
}

static void usbdux_firmware_request_complete_handler(const struct firmware *fw,
						     void *context)
{
	struct usbduxsub *usbduxsub_tmp = context;
	struct usb_device *usbdev = usbduxsub_tmp->usbdev;
	int ret;

	if (fw == NULL) {
		dev_err(&usbdev->dev,
			"Firmware complete handler without firmware!\n");
		return;
	}

	ret = firmwareUpload(usbduxsub_tmp, fw->data, fw->size);

	if (ret) {
		dev_err(&usbdev->dev,
			"Could not upload firmware (err=%d)\n", ret);
		goto out;
	}
	comedi_usb_auto_config(usbdev, BOARDNAME);
out:
	release_firmware(fw);
}

static int usbduxsigma_probe(struct usb_interface *uinterf,
			   const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(uinterf);
	struct device *dev = &uinterf->dev;
	int i;
	int index;
	int ret;

	dev_dbg(dev, "comedi_: usbdux_: "
		"finding a free structure for the usb-device\n");

	down(&start_stop_sem);
	
	index = -1;
	for (i = 0; i < NUMUSBDUX; i++) {
		if (!(usbduxsub[i].probed)) {
			index = i;
			break;
		}
	}

	
	if (index == -1) {
		dev_err(dev, "Too many usbduxsigma-devices connected.\n");
		up(&start_stop_sem);
		return -EMFILE;
	}
	dev_dbg(dev, "comedi_: usbdux: "
		"usbduxsub[%d] is ready to connect to comedi.\n", index);

	sema_init(&(usbduxsub[index].sem), 1);
	
	usbduxsub[index].usbdev = udev;

	
	usbduxsub[index].interface = uinterf;
	
	usbduxsub[index].ifnum = uinterf->altsetting->desc.bInterfaceNumber;
	
	
	usb_set_intfdata(uinterf, &(usbduxsub[index]));

	dev_dbg(dev, "comedi_: usbdux: ifnum=%d\n", usbduxsub[index].ifnum);

	
	usbduxsub[index].high_speed =
	    (usbduxsub[index].usbdev->speed == USB_SPEED_HIGH);

	
	usbduxsub[index].dac_commands = kzalloc(NUMOUTCHANNELS, GFP_KERNEL);
	if (!usbduxsub[index].dac_commands) {
		dev_err(dev, "comedi_: usbduxsigma: "
			"error alloc space for dac commands\n");
		tidy_up(&(usbduxsub[index]));
		up(&start_stop_sem);
		return -ENOMEM;
	}
	
	usbduxsub[index].dux_commands = kzalloc(SIZEOFDUXBUFFER, GFP_KERNEL);
	if (!usbduxsub[index].dux_commands) {
		dev_err(dev, "comedi_: usbduxsigma: "
			"error alloc space for dux commands\n");
		tidy_up(&(usbduxsub[index]));
		up(&start_stop_sem);
		return -ENOMEM;
	}
	
	usbduxsub[index].inBuffer = kzalloc(SIZEINBUF, GFP_KERNEL);
	if (!(usbduxsub[index].inBuffer)) {
		dev_err(dev, "comedi_: usbduxsigma: "
			"could not alloc space for inBuffer\n");
		tidy_up(&(usbduxsub[index]));
		up(&start_stop_sem);
		return -ENOMEM;
	}
	
	usbduxsub[index].insnBuffer = kzalloc(SIZEINSNBUF, GFP_KERNEL);
	if (!(usbduxsub[index].insnBuffer)) {
		dev_err(dev, "comedi_: usbduxsigma: "
			"could not alloc space for insnBuffer\n");
		tidy_up(&(usbduxsub[index]));
		up(&start_stop_sem);
		return -ENOMEM;
	}
	
	usbduxsub[index].outBuffer = kzalloc(SIZEOUTBUF, GFP_KERNEL);
	if (!(usbduxsub[index].outBuffer)) {
		dev_err(dev, "comedi_: usbduxsigma: "
			"could not alloc space for outBuffer\n");
		tidy_up(&(usbduxsub[index]));
		up(&start_stop_sem);
		return -ENOMEM;
	}
	
	i = usb_set_interface(usbduxsub[index].usbdev,
			      usbduxsub[index].ifnum, 3);
	if (i < 0) {
		dev_err(dev, "comedi_: usbduxsigma%d: "
			"could not set alternate setting 3 in high speed.\n",
			index);
		tidy_up(&(usbduxsub[index]));
		up(&start_stop_sem);
		return -ENODEV;
	}
	if (usbduxsub[index].high_speed)
		usbduxsub[index].numOfInBuffers = NUMOFINBUFFERSHIGH;
	else
		usbduxsub[index].numOfInBuffers = NUMOFINBUFFERSFULL;

	usbduxsub[index].urbIn =
	    kzalloc(sizeof(struct urb *) * usbduxsub[index].numOfInBuffers,
		    GFP_KERNEL);
	if (!(usbduxsub[index].urbIn)) {
		dev_err(dev, "comedi_: usbduxsigma: "
			"Could not alloc. urbIn array\n");
		tidy_up(&(usbduxsub[index]));
		up(&start_stop_sem);
		return -ENOMEM;
	}
	for (i = 0; i < usbduxsub[index].numOfInBuffers; i++) {
		
		usbduxsub[index].urbIn[i] = usb_alloc_urb(1, GFP_KERNEL);
		if (usbduxsub[index].urbIn[i] == NULL) {
			dev_err(dev, "comedi_: usbduxsigma%d: "
				"Could not alloc. urb(%d)\n", index, i);
			tidy_up(&(usbduxsub[index]));
			up(&start_stop_sem);
			return -ENOMEM;
		}
		usbduxsub[index].urbIn[i]->dev = usbduxsub[index].usbdev;
		
		
		usbduxsub[index].urbIn[i]->context = NULL;
		usbduxsub[index].urbIn[i]->pipe =
		    usb_rcvisocpipe(usbduxsub[index].usbdev, ISOINEP);
		usbduxsub[index].urbIn[i]->transfer_flags = URB_ISO_ASAP;
		usbduxsub[index].urbIn[i]->transfer_buffer =
		    kzalloc(SIZEINBUF, GFP_KERNEL);
		if (!(usbduxsub[index].urbIn[i]->transfer_buffer)) {
			dev_err(dev, "comedi_: usbduxsigma%d: "
				"could not alloc. transb.\n", index);
			tidy_up(&(usbduxsub[index]));
			up(&start_stop_sem);
			return -ENOMEM;
		}
		usbduxsub[index].urbIn[i]->complete = usbduxsub_ai_IsocIrq;
		usbduxsub[index].urbIn[i]->number_of_packets = 1;
		usbduxsub[index].urbIn[i]->transfer_buffer_length = SIZEINBUF;
		usbduxsub[index].urbIn[i]->iso_frame_desc[0].offset = 0;
		usbduxsub[index].urbIn[i]->iso_frame_desc[0].length =
			SIZEINBUF;
	}

	
	if (usbduxsub[index].high_speed)
		usbduxsub[index].numOfOutBuffers = NUMOFOUTBUFFERSHIGH;
	else
		usbduxsub[index].numOfOutBuffers = NUMOFOUTBUFFERSFULL;

	usbduxsub[index].urbOut =
	    kzalloc(sizeof(struct urb *) * usbduxsub[index].numOfOutBuffers,
		    GFP_KERNEL);
	if (!(usbduxsub[index].urbOut)) {
		dev_err(dev, "comedi_: usbduxsigma: "
			"Could not alloc. urbOut array\n");
		tidy_up(&(usbduxsub[index]));
		up(&start_stop_sem);
		return -ENOMEM;
	}
	for (i = 0; i < usbduxsub[index].numOfOutBuffers; i++) {
		
		usbduxsub[index].urbOut[i] = usb_alloc_urb(1, GFP_KERNEL);
		if (usbduxsub[index].urbOut[i] == NULL) {
			dev_err(dev, "comedi_: usbduxsigma%d: "
				"Could not alloc. urb(%d)\n", index, i);
			tidy_up(&(usbduxsub[index]));
			up(&start_stop_sem);
			return -ENOMEM;
		}
		usbduxsub[index].urbOut[i]->dev = usbduxsub[index].usbdev;
		
		
		usbduxsub[index].urbOut[i]->context = NULL;
		usbduxsub[index].urbOut[i]->pipe =
		    usb_sndisocpipe(usbduxsub[index].usbdev, ISOOUTEP);
		usbduxsub[index].urbOut[i]->transfer_flags = URB_ISO_ASAP;
		usbduxsub[index].urbOut[i]->transfer_buffer =
		    kzalloc(SIZEOUTBUF, GFP_KERNEL);
		if (!(usbduxsub[index].urbOut[i]->transfer_buffer)) {
			dev_err(dev, "comedi_: usbduxsigma%d: "
				"could not alloc. transb.\n", index);
			tidy_up(&(usbduxsub[index]));
			up(&start_stop_sem);
			return -ENOMEM;
		}
		usbduxsub[index].urbOut[i]->complete = usbduxsub_ao_IsocIrq;
		usbduxsub[index].urbOut[i]->number_of_packets = 1;
		usbduxsub[index].urbOut[i]->transfer_buffer_length =
			SIZEOUTBUF;
		usbduxsub[index].urbOut[i]->iso_frame_desc[0].offset = 0;
		usbduxsub[index].urbOut[i]->iso_frame_desc[0].length =
		    SIZEOUTBUF;
		if (usbduxsub[index].high_speed) {
			
			usbduxsub[index].urbOut[i]->interval = 8;
		} else {
			
			usbduxsub[index].urbOut[i]->interval = 1;
		}
	}

	
	if (usbduxsub[index].high_speed) {
		
		usbduxsub[index].sizePwmBuf = 512;
		usbduxsub[index].urbPwm = usb_alloc_urb(0, GFP_KERNEL);
		if (usbduxsub[index].urbPwm == NULL) {
			dev_err(dev, "comedi_: usbduxsigma%d: "
				"Could not alloc. pwm urb\n", index);
			tidy_up(&(usbduxsub[index]));
			up(&start_stop_sem);
			return -ENOMEM;
		}
		usbduxsub[index].urbPwm->transfer_buffer =
		    kzalloc(usbduxsub[index].sizePwmBuf, GFP_KERNEL);
		if (!(usbduxsub[index].urbPwm->transfer_buffer)) {
			dev_err(dev, "comedi_: usbduxsigma%d: "
				"could not alloc. transb. for pwm\n", index);
			tidy_up(&(usbduxsub[index]));
			up(&start_stop_sem);
			return -ENOMEM;
		}
	} else {
		usbduxsub[index].urbPwm = NULL;
		usbduxsub[index].sizePwmBuf = 0;
	}

	usbduxsub[index].ai_cmd_running = 0;
	usbduxsub[index].ao_cmd_running = 0;
	usbduxsub[index].pwm_cmd_running = 0;

	
	usbduxsub[index].probed = 1;
	up(&start_stop_sem);

	ret = request_firmware_nowait(THIS_MODULE,
				      FW_ACTION_HOTPLUG,
				      "usbduxsigma_firmware.bin",
				      &udev->dev,
				      GFP_KERNEL,
				      usbduxsub + index,
				      usbdux_firmware_request_complete_handler
				      );

	if (ret) {
		dev_err(dev, "Could not load firmware (err=%d)\n", ret);
		return ret;
	}

	dev_info(dev, "comedi_: successfully initialised.\n");
	
	return 0;
}

static void usbduxsigma_disconnect(struct usb_interface *intf)
{
	struct usbduxsub *usbduxsub_tmp = usb_get_intfdata(intf);
	struct usb_device *udev = interface_to_usbdev(intf);

	if (!usbduxsub_tmp) {
		dev_err(&intf->dev,
			"comedi_: disconnect called with null pointer.\n");
		return;
	}
	if (usbduxsub_tmp->usbdev != udev) {
		dev_err(&intf->dev, "comedi_: BUG! wrong ptr!\n");
		return;
	}
	if (usbduxsub_tmp->ai_cmd_running)
		
		usbdux_ai_stop(usbduxsub_tmp, 1);
	if (usbduxsub_tmp->ao_cmd_running)
		
		usbdux_ao_stop(usbduxsub_tmp, 1);
	comedi_usb_auto_unconfig(udev);
	down(&start_stop_sem);
	down(&usbduxsub_tmp->sem);
	tidy_up(usbduxsub_tmp);
	up(&usbduxsub_tmp->sem);
	up(&start_stop_sem);
	dev_info(&intf->dev, "comedi_: disconnected from the usb\n");
}

static int usbduxsigma_attach(struct comedi_device *dev,
			      struct comedi_devconfig *it)
{
	int ret;
	int index;
	int i;
	struct usbduxsub *udev;

	int offset;

	struct comedi_subdevice *s = NULL;
	dev->private = NULL;

	down(&start_stop_sem);
	index = -1;
	for (i = 0; i < NUMUSBDUX; i++) {
		if ((usbduxsub[i].probed) && (!usbduxsub[i].attached)) {
			index = i;
			break;
		}
	}

	if (index < 0) {
		printk(KERN_ERR "comedi%d: usbduxsigma: error: attach failed,"
		       "dev not connected to the usb bus.\n", dev->minor);
		up(&start_stop_sem);
		return -ENODEV;
	}

	udev = &usbduxsub[index];
	down(&udev->sem);
	
	udev->comedidev = dev;

	
	if (comedi_aux_data(it->options, 0) &&
	    it->options[COMEDI_DEVCONF_AUX_DATA_LENGTH]) {
		firmwareUpload(udev, comedi_aux_data(it->options, 0),
			       it->options[COMEDI_DEVCONF_AUX_DATA_LENGTH]);
	}

	dev->board_name = BOARDNAME;

	
	if (udev->high_speed) {
		
		dev->n_subdevices = 4;
	} else {
		
		dev->n_subdevices = 3;
	}

	
	ret = alloc_subdevices(dev, dev->n_subdevices);
	if (ret < 0) {
		dev_err(&udev->interface->dev,
			"comedi%d: no space for subdev\n", dev->minor);
		up(&udev->sem);
		up(&start_stop_sem);
		return ret;
	}

	
	dev->private = udev;

	
	s = dev->subdevices + SUBDEV_AD;
	
	
	
	dev->read_subdev = s;
	
	
	s->private = NULL;
	
	s->type = COMEDI_SUBD_AI;
	
	s->subdev_flags = SDF_READABLE | SDF_GROUND |
		SDF_CMD_READ | SDF_LSAMPL;
	
	s->n_chan = NUMCHANNELS;
	
	s->len_chanlist = NUMCHANNELS;
	
	s->insn_read = usbdux_ai_insn_read;
	s->do_cmdtest = usbdux_ai_cmdtest;
	s->do_cmd = usbdux_ai_cmd;
	s->cancel = usbdux_ai_cancel;
	
	s->maxdata = 0x00FFFFFF;
	
	s->range_table = (&range_usbdux_ai_range);

	
	s = dev->subdevices + SUBDEV_DA;
	
	s->type = COMEDI_SUBD_AO;
	
	dev->write_subdev = s;
	
	
	s->private = NULL;
	
	s->subdev_flags = SDF_WRITABLE | SDF_GROUND | SDF_CMD_WRITE;
	
	s->n_chan = 4;
	
	s->len_chanlist = 4;
	
	s->maxdata = 0x00ff;
	
	s->range_table = (&range_usbdux_ao_range);
	
	s->do_cmdtest = usbdux_ao_cmdtest;
	s->do_cmd = usbdux_ao_cmd;
	s->cancel = usbdux_ao_cancel;
	s->insn_read = usbdux_ao_insn_read;
	s->insn_write = usbdux_ao_insn_write;

	
	s = dev->subdevices + SUBDEV_DIO;
	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	
	s->n_chan = 24;
	s->maxdata = 1;
	s->range_table = (&range_digital);
	s->insn_bits = usbdux_dio_insn_bits;
	s->insn_config = usbdux_dio_insn_config;
	
	s->private = NULL;

	if (udev->high_speed) {
		
		s = dev->subdevices + SUBDEV_PWM;
		s->type = COMEDI_SUBD_PWM;
		s->subdev_flags = SDF_WRITABLE | SDF_PWM_HBRIDGE;
		s->n_chan = 8;
		
		s->maxdata = udev->sizePwmBuf;
		s->insn_write = usbdux_pwm_write;
		s->insn_read = usbdux_pwm_read;
		s->insn_config = usbdux_pwm_config;
		usbdux_pwm_period(dev, s, PWM_DEFAULT_PERIOD);
	}
	
	udev->attached = 1;

	up(&udev->sem);

	up(&start_stop_sem);

	offset = usbdux_getstatusinfo(dev, 0);
	if (offset < 0)
		dev_err(&udev->interface->dev,
			"Communication to USBDUXSIGMA failed!"
			"Check firmware and cabling.");

	dev_info(&udev->interface->dev,
		 "comedi%d: attached, ADC_zero = %x", dev->minor, offset);

	return 0;
}

static int usbduxsigma_detach(struct comedi_device *dev)
{
	struct usbduxsub *usbduxsub_tmp;

	if (!dev) {
		printk(KERN_ERR
		       "comedi? usbduxsigma detach: dev=NULL\n");
		return -EFAULT;
	}

	usbduxsub_tmp = dev->private;
	if (!usbduxsub_tmp) {
		printk(KERN_ERR
		       "comedi?: usbduxsigma detach: private=NULL\n");
		return -EFAULT;
	}

	dev_dbg(&usbduxsub_tmp->interface->dev,
		"comedi%d: detach usb device\n",
		dev->minor);

	down(&usbduxsub_tmp->sem);
	
	
	dev->private = NULL;
	usbduxsub_tmp->attached = 0;
	usbduxsub_tmp->comedidev = NULL;
	dev_info(&usbduxsub_tmp->interface->dev,
		"comedi%d: successfully detached.\n", dev->minor);
	up(&usbduxsub_tmp->sem);
	return 0;
}

static struct comedi_driver driver_usbduxsigma = {
	.driver_name = "usbduxsigma",
	.module = THIS_MODULE,
	.attach = usbduxsigma_attach,
	.detach = usbduxsigma_detach,
};

static const struct usb_device_id usbduxsigma_table[] = {
	{USB_DEVICE(0x13d8, 0x0020)},
	{USB_DEVICE(0x13d8, 0x0021)},
	{USB_DEVICE(0x13d8, 0x0022)},
	{}			
};

MODULE_DEVICE_TABLE(usb, usbduxsigma_table);

static struct usb_driver usbduxsigma_driver = {
	.name = BOARDNAME,
	.probe = usbduxsigma_probe,
	.disconnect = usbduxsigma_disconnect,
	.id_table = usbduxsigma_table,
};

static int __init init_usbduxsigma(void)
{
	printk(KERN_INFO KBUILD_MODNAME ": "
	       DRIVER_VERSION ":" DRIVER_DESC "\n");
	usb_register(&usbduxsigma_driver);
	comedi_driver_register(&driver_usbduxsigma);
	return 0;
}

static void __exit exit_usbduxsigma(void)
{
	comedi_driver_unregister(&driver_usbduxsigma);
	usb_deregister(&usbduxsigma_driver);
}

module_init(init_usbduxsigma);
module_exit(exit_usbduxsigma);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
