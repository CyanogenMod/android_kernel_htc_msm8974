/*
 *  Copyright (C) 2004 Bernd Porr, Bernd.Porr@f2s.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/usb.h>
#include <linux/fcntl.h>
#include <linux/compiler.h>
#include "comedi_fc.h"
#include "../comedidev.h"

#define DRIVER_VERSION "v1.0"
#define DRIVER_AUTHOR "Bernd Porr, BerndPorr@f2s.com"
#define DRIVER_DESC "USB-DUXfast, BerndPorr@f2s.com"
#define BOARDNAME "usbduxfast"

#define EZTIMEOUT	30

#define USBDUXFASTSUB_FIRMWARE	0xA0
#define VENDOR_DIR_IN		0xC0
#define VENDOR_DIR_OUT		0x40

#define USBDUXFASTSUB_CPUCS	0xE600

#define TB_LEN	0x2000

#define BULKINEP	6

#define CHANNELLISTEP	4

#define NUMCHANNELS	32

#define WAVESIZE	0x20

#define SIZEADIN	(sizeof(int16_t))

#define SIZEINBUF	512

#define SIZEINSNBUF	512

#define SIZEOFDUXBUFFER	256

#define NUMOFINBUFFERSHIGH	10

#define NUMUSBDUXFAST	16

#define N_SUBDEVICES	1

#define SUBDEV_AD	0

#define MIN_SAMPLING_PERIOD	9

#define MAX_SAMPLING_PERIOD	500

#define PACKETS_TO_IGNORE	4

static const struct comedi_lrange range_usbduxfast_ai_range = {
	2, {BIP_RANGE(0.75), BIP_RANGE(0.5)}
};

struct usbduxfastsub_s {
	int attached;		
	int probed;		
	struct usb_device *usbdev;	
	struct urb *urbIn;	
	int8_t *transfer_buffer;
	int16_t *insnBuffer;	
	int ifnum;		
	struct usb_interface *interface;	
	
	struct comedi_device *comedidev;
	short int ai_cmd_running;	
	short int ai_continous;	
	long int ai_sample_count;	
	uint8_t *dux_commands;	
	int ignore;		
	struct semaphore sem;
};

static struct usbduxfastsub_s usbduxfastsub[NUMUSBDUXFAST];

static DEFINE_SEMAPHORE(start_stop_sem);

#define SENDADCOMMANDS            0
#define SENDINITEP6               1

static int send_dux_commands(struct usbduxfastsub_s *udfs, int cmd_type)
{
	int tmp, nsent;

	udfs->dux_commands[0] = cmd_type;

#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi%d: usbduxfast: dux_commands: ",
	       udfs->comedidev->minor);
	for (tmp = 0; tmp < SIZEOFDUXBUFFER; tmp++)
		printk(" %02x", udfs->dux_commands[tmp]);
	printk("\n");
#endif

	tmp = usb_bulk_msg(udfs->usbdev,
			   usb_sndbulkpipe(udfs->usbdev, CHANNELLISTEP),
			   udfs->dux_commands, SIZEOFDUXBUFFER, &nsent, 10000);
	if (tmp < 0)
		printk(KERN_ERR "comedi%d: could not transmit dux_commands to"
		       "the usb-device, err=%d\n", udfs->comedidev->minor, tmp);
	return tmp;
}

static int usbduxfastsub_unlink_InURBs(struct usbduxfastsub_s *udfs)
{
	int j = 0;
	int err = 0;

	if (udfs && udfs->urbIn) {
		udfs->ai_cmd_running = 0;
		
		usb_kill_urb(udfs->urbIn);
		j = 0;
	}
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi: usbduxfast: unlinked InURB: res=%d\n", j);
#endif
	return err;
}

static int usbduxfast_ai_stop(struct usbduxfastsub_s *udfs, int do_unlink)
{
	int ret = 0;

	if (!udfs) {
		printk(KERN_ERR "comedi?: usbduxfast_ai_stop: udfs=NULL!\n");
		return -EFAULT;
	}
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi: usbduxfast_ai_stop\n");
#endif

	udfs->ai_cmd_running = 0;

	if (do_unlink)
		
		ret = usbduxfastsub_unlink_InURBs(udfs);

	return ret;
}

static int usbduxfast_ai_cancel(struct comedi_device *dev,
				struct comedi_subdevice *s)
{
	struct usbduxfastsub_s *udfs;
	int ret;

	
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi: usbduxfast_ai_cancel\n");
#endif
	udfs = dev->private;
	if (!udfs) {
		printk(KERN_ERR "comedi: usbduxfast_ai_cancel: udfs=NULL\n");
		return -EFAULT;
	}
	down(&udfs->sem);
	if (!udfs->probed) {
		up(&udfs->sem);
		return -ENODEV;
	}
	
	ret = usbduxfast_ai_stop(udfs, 1);
	up(&udfs->sem);

	return ret;
}

static void usbduxfastsub_ai_Irq(struct urb *urb)
{
	int n, err;
	struct usbduxfastsub_s *udfs;
	struct comedi_device *this_comedidev;
	struct comedi_subdevice *s;
	uint16_t *p;

	
	if (!urb) {
		printk(KERN_ERR "comedi_: usbduxfast_: ao int-handler called "
		       "with urb=NULL!\n");
		return;
	}
	
	this_comedidev = urb->context;
	if (!this_comedidev) {
		printk(KERN_ERR "comedi_: usbduxfast_: urb context is a NULL "
		       "pointer!\n");
		return;
	}
	
	udfs = this_comedidev->private;
	if (!udfs) {
		printk(KERN_ERR "comedi_: usbduxfast_: private of comedi "
		       "subdev is a NULL pointer!\n");
		return;
	}
	
	if (unlikely(!udfs->ai_cmd_running)) {
		return;
	}

	if (unlikely(!udfs->attached)) {
		
		return;
	}
	
	s = this_comedidev->subdevices + SUBDEV_AD;

	
	switch (urb->status) {
	case 0:
		break;

	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
	case -ECONNABORTED:
		
		s->async->events |= COMEDI_CB_EOA;
		s->async->events |= COMEDI_CB_ERROR;
		comedi_event(udfs->comedidev, s);
		
		usbduxfast_ai_stop(udfs, 0);
		return;

	default:
		printk("comedi%d: usbduxfast: non-zero urb status received in "
		       "ai intr context: %d\n",
		       udfs->comedidev->minor, urb->status);
		s->async->events |= COMEDI_CB_EOA;
		s->async->events |= COMEDI_CB_ERROR;
		comedi_event(udfs->comedidev, s);
		usbduxfast_ai_stop(udfs, 0);
		return;
	}

	p = urb->transfer_buffer;
	if (!udfs->ignore) {
		if (!udfs->ai_continous) {
			
			n = urb->actual_length / sizeof(uint16_t);
			if (unlikely(udfs->ai_sample_count < n)) {
				cfc_write_array_to_buffer(s,
							  urb->transfer_buffer,
							  udfs->ai_sample_count
							  * sizeof(uint16_t));
				usbduxfast_ai_stop(udfs, 0);
				
				s->async->events |= COMEDI_CB_EOA;
				comedi_event(udfs->comedidev, s);
				return;
			}
			udfs->ai_sample_count -= n;
		}
		
		err = cfc_write_array_to_buffer(s, urb->transfer_buffer,
						urb->actual_length);
		if (unlikely(err == 0)) {
			
			usbduxfast_ai_stop(udfs, 0);
			return;
		}

		
		comedi_event(udfs->comedidev, s);

	} else {
		
		udfs->ignore--;
	}

	urb->dev = udfs->usbdev;
	urb->status = 0;
	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		printk(KERN_ERR "comedi%d: usbduxfast: urb resubm failed: %d",
		       udfs->comedidev->minor, err);
		s->async->events |= COMEDI_CB_EOA;
		s->async->events |= COMEDI_CB_ERROR;
		comedi_event(udfs->comedidev, s);
		usbduxfast_ai_stop(udfs, 0);
	}
}

static int usbduxfastsub_start(struct usbduxfastsub_s *udfs)
{
	int ret;
	unsigned char local_transfer_buffer[16];

	
	local_transfer_buffer[0] = 0;
	
	ret = usb_control_msg(udfs->usbdev, usb_sndctrlpipe(udfs->usbdev, 0), USBDUXFASTSUB_FIRMWARE,
				VENDOR_DIR_OUT,	
				USBDUXFASTSUB_CPUCS,	
				0x0000,	
				
				local_transfer_buffer,
				1,	
				EZTIMEOUT);	
	if (ret < 0) {
		printk("comedi_: usbduxfast_: control msg failed (start)\n");
		return ret;
	}

	return 0;
}

static int usbduxfastsub_stop(struct usbduxfastsub_s *udfs)
{
	int ret;
	unsigned char local_transfer_buffer[16];

	
	local_transfer_buffer[0] = 1;
	
	ret = usb_control_msg(udfs->usbdev, usb_sndctrlpipe(udfs->usbdev, 0), USBDUXFASTSUB_FIRMWARE,
			      VENDOR_DIR_OUT,	
			      USBDUXFASTSUB_CPUCS,	
			      0x0000,	
			      local_transfer_buffer, 1,	
			      EZTIMEOUT);	
	if (ret < 0) {
		printk(KERN_ERR "comedi_: usbduxfast: control msg failed "
		       "(stop)\n");
		return ret;
	}

	return 0;
}

static int usbduxfastsub_upload(struct usbduxfastsub_s *udfs,
				unsigned char *local_transfer_buffer,
				unsigned int startAddr, unsigned int len)
{
	int ret;

#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi: usbduxfast: uploading %d bytes", len);
	printk(KERN_DEBUG " to addr %d, first byte=%d.\n",
	       startAddr, local_transfer_buffer[0]);
#endif
	
	ret = usb_control_msg(udfs->usbdev, usb_sndctrlpipe(udfs->usbdev, 0), USBDUXFASTSUB_FIRMWARE,
				VENDOR_DIR_OUT,	
				startAddr,	
				0x0000,	
				
				local_transfer_buffer,
				len,	
				EZTIMEOUT);	

#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi_: usbduxfast: result=%d\n", ret);
#endif

	if (ret < 0) {
		printk(KERN_ERR "comedi_: usbduxfast: uppload failed\n");
		return ret;
	}

	return 0;
}

static int usbduxfastsub_submit_InURBs(struct usbduxfastsub_s *udfs)
{
	int ret;

	if (!udfs)
		return -EFAULT;

	usb_fill_bulk_urb(udfs->urbIn, udfs->usbdev,
			  usb_rcvbulkpipe(udfs->usbdev, BULKINEP),
			  udfs->transfer_buffer,
			  SIZEINBUF, usbduxfastsub_ai_Irq, udfs->comedidev);

#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi%d: usbduxfast: submitting in-urb: "
	       "0x%p,0x%p\n", udfs->comedidev->minor, udfs->urbIn->context,
	       udfs->urbIn->dev);
#endif
	ret = usb_submit_urb(udfs->urbIn, GFP_ATOMIC);
	if (ret) {
		printk(KERN_ERR "comedi_: usbduxfast: ai: usb_submit_urb error"
		       " %d\n", ret);
		return ret;
	}
	return 0;
}

static int usbduxfast_ai_cmdtest(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_cmd *cmd)
{
	int err = 0, stop_mask = 0;
	long int steps, tmp;
	int minSamplPer;
	struct usbduxfastsub_s *udfs = dev->private;

	if (!udfs->probed)
		return -ENODEV;

#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi%d: usbduxfast_ai_cmdtest\n", dev->minor);
	printk(KERN_DEBUG "comedi%d: usbduxfast: convert_arg=%u "
	       "scan_begin_arg=%u\n",
	       dev->minor, cmd->convert_arg, cmd->scan_begin_arg);
#endif
	

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW | TRIG_EXT | TRIG_INT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER | TRIG_FOLLOW | TRIG_EXT;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_TIMER | TRIG_EXT;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
	stop_mask = TRIG_COUNT | TRIG_NONE;
	cmd->stop_src &= stop_mask;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;


	if (cmd->start_src != TRIG_NOW &&
	    cmd->start_src != TRIG_EXT && cmd->start_src != TRIG_INT)
		err++;
	if (cmd->scan_begin_src != TRIG_TIMER &&
	    cmd->scan_begin_src != TRIG_FOLLOW &&
	    cmd->scan_begin_src != TRIG_EXT)
		err++;
	if (cmd->convert_src != TRIG_TIMER && cmd->convert_src != TRIG_EXT)
		err++;
	if (cmd->stop_src != TRIG_COUNT &&
	    cmd->stop_src != TRIG_EXT && cmd->stop_src != TRIG_NONE)
		err++;

	
	if (cmd->start_src == TRIG_EXT && cmd->stop_src == TRIG_EXT)
		err++;

	if (err)
		return 2;

	

	if (cmd->start_src == TRIG_NOW && cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}

	if (!cmd->chanlist_len)
		err++;

	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}

	if (cmd->chanlist_len == 1)
		minSamplPer = 1;
	else
		minSamplPer = MIN_SAMPLING_PERIOD;

	if (cmd->convert_src == TRIG_TIMER) {
		steps = cmd->convert_arg * 30;
		if (steps < (minSamplPer * 1000))
			steps = minSamplPer * 1000;

		if (steps > (MAX_SAMPLING_PERIOD * 1000))
			steps = MAX_SAMPLING_PERIOD * 1000;

		
		tmp = steps / 30;
		if (cmd->convert_arg != tmp) {
			cmd->convert_arg = tmp;
			err++;
		}
	}

	if (cmd->scan_begin_src == TRIG_TIMER)
		err++;

	
	switch (cmd->stop_src) {
	case TRIG_COUNT:
		if (!cmd->stop_arg) {
			cmd->stop_arg = 1;
			err++;
		}
		break;
	case TRIG_NONE:
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
		break;
	default:
		break;
	}

	if (err)
		return 3;

	

	return 0;

}

static int usbduxfast_ai_inttrig(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 unsigned int trignum)
{
	int ret;
	struct usbduxfastsub_s *udfs = dev->private;

	if (!udfs)
		return -EFAULT;

	down(&udfs->sem);
	if (!udfs->probed) {
		up(&udfs->sem);
		return -ENODEV;
	}
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi%d: usbduxfast_ai_inttrig\n", dev->minor);
#endif

	if (trignum != 0) {
		printk(KERN_ERR "comedi%d: usbduxfast_ai_inttrig: invalid"
		       " trignum\n", dev->minor);
		up(&udfs->sem);
		return -EINVAL;
	}
	if (!udfs->ai_cmd_running) {
		udfs->ai_cmd_running = 1;
		ret = usbduxfastsub_submit_InURBs(udfs);
		if (ret < 0) {
			printk(KERN_ERR "comedi%d: usbduxfast_ai_inttrig: "
			       "urbSubmit: err=%d\n", dev->minor, ret);
			udfs->ai_cmd_running = 0;
			up(&udfs->sem);
			return ret;
		}
		s->async->inttrig = NULL;
	} else {
		printk(KERN_ERR "comedi%d: ai_inttrig but acqu is already"
		       " running\n", dev->minor);
	}
	up(&udfs->sem);
	return 1;
}

#define LENBASE	(1+0x00)
#define OPBASE	(1+0x08)
#define OUTBASE	(1+0x10)
#define LOGBASE	(1+0x18)

static int usbduxfast_ai_cmd(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned int chan, gain, rngmask = 0xff;
	int i, j, ret;
	struct usbduxfastsub_s *udfs;
	int result;
	long steps, steps_tmp;

#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi%d: usbduxfast_ai_cmd\n", dev->minor);
#endif
	udfs = dev->private;
	if (!udfs)
		return -EFAULT;

	down(&udfs->sem);
	if (!udfs->probed) {
		up(&udfs->sem);
		return -ENODEV;
	}
	if (udfs->ai_cmd_running) {
		printk(KERN_ERR "comedi%d: ai_cmd not possible. Another ai_cmd"
		       " is running.\n", dev->minor);
		up(&udfs->sem);
		return -EBUSY;
	}
	
	s->async->cur_chan = 0;

	udfs->ignore = PACKETS_TO_IGNORE;

	if (cmd->chanlist_len > 0) {
		gain = CR_RANGE(cmd->chanlist[0]);
		for (i = 0; i < cmd->chanlist_len; ++i) {
			chan = CR_CHAN(cmd->chanlist[i]);
			if (chan != i) {
				printk(KERN_ERR "comedi%d: cmd is accepting "
				       "only consecutive channels.\n",
				       dev->minor);
				up(&udfs->sem);
				return -EINVAL;
			}
			if ((gain != CR_RANGE(cmd->chanlist[i]))
			    && (cmd->chanlist_len > 3)) {
				printk(KERN_ERR "comedi%d: the gain must be"
				       " the same for all channels.\n",
				       dev->minor);
				up(&udfs->sem);
				return -EINVAL;
			}
			if (i >= NUMCHANNELS) {
				printk(KERN_ERR "comedi%d: channel list too"
				       " long\n", dev->minor);
				break;
			}
		}
	}
	steps = 0;
	if (cmd->scan_begin_src == TRIG_TIMER) {
		printk(KERN_ERR "comedi%d: usbduxfast: "
		       "scan_begin_src==TRIG_TIMER not valid.\n", dev->minor);
		up(&udfs->sem);
		return -EINVAL;
	}
	if (cmd->convert_src == TRIG_TIMER)
		steps = (cmd->convert_arg * 30) / 1000;

	if ((steps < MIN_SAMPLING_PERIOD) && (cmd->chanlist_len != 1)) {
		printk(KERN_ERR "comedi%d: usbduxfast: ai_cmd: steps=%ld, "
		       "scan_begin_arg=%d. Not properly tested by cmdtest?\n",
		       dev->minor, steps, cmd->scan_begin_arg);
		up(&udfs->sem);
		return -EINVAL;
	}
	if (steps > MAX_SAMPLING_PERIOD) {
		printk(KERN_ERR "comedi%d: usbduxfast: ai_cmd: sampling rate "
		       "too low.\n", dev->minor);
		up(&udfs->sem);
		return -EINVAL;
	}
	if ((cmd->start_src == TRIG_EXT) && (cmd->chanlist_len != 1)
	    && (cmd->chanlist_len != 16)) {
		printk(KERN_ERR "comedi%d: usbduxfast: ai_cmd: TRIG_EXT only"
		       " with 1 or 16 channels possible.\n", dev->minor);
		up(&udfs->sem);
		return -EINVAL;
	}
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi%d: usbduxfast: steps=%ld, convert_arg=%u\n",
	       dev->minor, steps, cmd->convert_arg);
#endif

	switch (cmd->chanlist_len) {
	case 1:

		if (CR_RANGE(cmd->chanlist[0]) > 0)
			rngmask = 0xff - 0x04;
		else
			rngmask = 0xff;


		
		if (cmd->start_src == TRIG_EXT) {
			
			udfs->dux_commands[LENBASE + 0] = 0x01;
			
			udfs->dux_commands[OPBASE + 0] = 0x01;
			udfs->dux_commands[OUTBASE + 0] = 0xFF & rngmask;
			
			udfs->dux_commands[LOGBASE + 0] = 0x00;
		} else {	
			udfs->dux_commands[LENBASE + 0] = 1;
			udfs->dux_commands[OPBASE + 0] = 0;
			udfs->dux_commands[OUTBASE + 0] = 0xFF & rngmask;
			udfs->dux_commands[LOGBASE + 0] = 0;
		}

		if (steps < MIN_SAMPLING_PERIOD) {
			
			if (steps <= 1) {

				
				udfs->dux_commands[LENBASE + 1] = 0x89;
				
				udfs->dux_commands[OPBASE + 1] = 0x03;
				udfs->dux_commands[OUTBASE + 1] =
				    0xFF & rngmask;
				
				udfs->dux_commands[LOGBASE + 1] = 0xFF;
			} else {
				udfs->dux_commands[LENBASE + 1] = steps - 1;
				
				udfs->dux_commands[OPBASE + 1] = 0x02;
				udfs->dux_commands[OUTBASE + 1] =
				    0xFF & rngmask;
				
				udfs->dux_commands[LOGBASE + 1] = 0;
				
				udfs->dux_commands[LENBASE + 2] = 0x09;
				
				udfs->dux_commands[OPBASE + 2] = 0x01;
				udfs->dux_commands[OUTBASE + 2] =
				    0xFF & rngmask;
				
				udfs->dux_commands[LOGBASE + 2] = 0xFF;
			}
		} else {

			
			steps = steps - 1;

			
			udfs->dux_commands[LENBASE + 1] = steps / 2;
			udfs->dux_commands[OPBASE + 1] = 0;
			udfs->dux_commands[OUTBASE + 1] = 0xFF & rngmask;
			udfs->dux_commands[LOGBASE + 1] = 0;

			
			udfs->dux_commands[LENBASE + 2] = steps - steps / 2;
			udfs->dux_commands[OPBASE + 2] = 0;
			udfs->dux_commands[OUTBASE + 2] = 0xFF & rngmask;
			udfs->dux_commands[LOGBASE + 2] = 0;

			

			
			udfs->dux_commands[LENBASE + 3] = 0x09;
			
			udfs->dux_commands[OPBASE + 3] = 0x03;
			udfs->dux_commands[OUTBASE + 3] = 0xFF & rngmask;
			
			udfs->dux_commands[LOGBASE + 3] = 0xFF;
		}
		break;

	case 2:

		if (CR_RANGE(cmd->chanlist[0]) > 0)
			rngmask = 0xff - 0x04;
		else
			rngmask = 0xff;

		udfs->dux_commands[LENBASE + 0] = 1;
		
		udfs->dux_commands[OPBASE + 0] = 0x02;
		udfs->dux_commands[OUTBASE + 0] = 0xFF & rngmask;
		udfs->dux_commands[LOGBASE + 0] = 0;

		
		steps_tmp = steps - 1;

		if (CR_RANGE(cmd->chanlist[1]) > 0)
			rngmask = 0xff - 0x04;
		else
			rngmask = 0xff;

		
		udfs->dux_commands[LENBASE + 1] = steps_tmp / 2;
		udfs->dux_commands[OPBASE + 1] = 0;
		
		udfs->dux_commands[OUTBASE + 1] = 0xFE & rngmask;
		udfs->dux_commands[LOGBASE + 1] = 0;

		
		udfs->dux_commands[LENBASE + 2] = steps_tmp - steps_tmp / 2;
		udfs->dux_commands[OPBASE + 2] = 0;
		udfs->dux_commands[OUTBASE + 2] = 0xFF & rngmask;
		udfs->dux_commands[LOGBASE + 2] = 0;

		udfs->dux_commands[LENBASE + 3] = 1;
		
		udfs->dux_commands[OPBASE + 3] = 0x02;
		udfs->dux_commands[OUTBASE + 3] = 0xFF & rngmask;
		udfs->dux_commands[LOGBASE + 3] = 0;

		steps_tmp = steps - 2;

		if (CR_RANGE(cmd->chanlist[0]) > 0)
			rngmask = 0xff - 0x04;
		else
			rngmask = 0xff;

		
		udfs->dux_commands[LENBASE + 4] = steps_tmp / 2;
		udfs->dux_commands[OPBASE + 4] = 0;
		
		udfs->dux_commands[OUTBASE + 4] = (0xFF - 0x02) & rngmask;
		udfs->dux_commands[LOGBASE + 4] = 0;

		
		udfs->dux_commands[LENBASE + 5] = steps_tmp - steps_tmp / 2;
		udfs->dux_commands[OPBASE + 5] = 0;
		udfs->dux_commands[OUTBASE + 5] = 0xFF & rngmask;
		udfs->dux_commands[LOGBASE + 5] = 0;

		udfs->dux_commands[LENBASE + 6] = 1;
		udfs->dux_commands[OPBASE + 6] = 0;
		udfs->dux_commands[OUTBASE + 6] = 0xFF & rngmask;
		udfs->dux_commands[LOGBASE + 6] = 0;
		break;

	case 3:
		for (j = 0; j < 1; j++) {
			if (CR_RANGE(cmd->chanlist[j]) > 0)
				rngmask = 0xff - 0x04;
			else
				rngmask = 0xff;
			udfs->dux_commands[LENBASE + j * 2] = steps / 2;
			
			udfs->dux_commands[OPBASE + j * 2] = 0x02;
			
			udfs->dux_commands[OUTBASE + j * 2] = 0xFF & rngmask;
			udfs->dux_commands[LOGBASE + j * 2] = 0;

			if (CR_RANGE(cmd->chanlist[j + 1]) > 0)
				rngmask = 0xff - 0x04;
			else
				rngmask = 0xff;

			
			udfs->dux_commands[LENBASE + j * 2 + 1] =
			    steps - steps / 2;
			
			udfs->dux_commands[OPBASE + j * 2 + 1] = 0;
			
			udfs->dux_commands[OUTBASE + j * 2 + 1] =
			    0xFE & rngmask;
			udfs->dux_commands[LOGBASE + j * 2 + 1] = 0;
		}

		
		steps_tmp = steps - 2;

		
		udfs->dux_commands[LENBASE + 4] = steps_tmp / 2;
		
		udfs->dux_commands[OPBASE + 4] = 0x02;
		udfs->dux_commands[OUTBASE + 4] = 0xFF & rngmask;
		udfs->dux_commands[LOGBASE + 4] = 0;

		if (CR_RANGE(cmd->chanlist[0]) > 0)
			rngmask = 0xff - 0x04;
		else
			rngmask = 0xff;

		
		udfs->dux_commands[LENBASE + 5] = steps_tmp - steps_tmp / 2;
		
		udfs->dux_commands[OPBASE + 5] = 0;
		
		udfs->dux_commands[OUTBASE + 5] = (0xFF - 0x02) & rngmask;
		udfs->dux_commands[LOGBASE + 5] = 0;

		udfs->dux_commands[LENBASE + 6] = 1;
		udfs->dux_commands[OPBASE + 6] = 0;
		udfs->dux_commands[OUTBASE + 6] = 0xFF & rngmask;
		udfs->dux_commands[LOGBASE + 6] = 0;

	case 16:
		if (CR_RANGE(cmd->chanlist[0]) > 0)
			rngmask = 0xff - 0x04;
		else
			rngmask = 0xff;

		if (cmd->start_src == TRIG_EXT) {

			
			udfs->dux_commands[LENBASE + 0] = 0x01;
			
			udfs->dux_commands[OPBASE + 0] = 0x01;
			
			udfs->dux_commands[OUTBASE + 0] =
			    (0xFF - 0x02) & rngmask;
			
			udfs->dux_commands[LOGBASE + 0] = 0x00;
		} else {

			
			udfs->dux_commands[LENBASE + 0] = 255;
			udfs->dux_commands[OPBASE + 0] = 0;
			
			udfs->dux_commands[OUTBASE + 0] =
			    (0xFF - 0x02) & rngmask;
			udfs->dux_commands[LOGBASE + 0] = 0;
		}

		
		udfs->dux_commands[LENBASE + 1] = 1;
		
		udfs->dux_commands[OPBASE + 1] = 0x02;
		udfs->dux_commands[OUTBASE + 1] = 0xFF & rngmask;
		udfs->dux_commands[LOGBASE + 1] = 0;

		
		steps = steps - 2;

		
		udfs->dux_commands[LENBASE + 2] = steps / 2;
		udfs->dux_commands[OPBASE + 2] = 0;
		udfs->dux_commands[OUTBASE + 2] = 0xFE & rngmask;
		udfs->dux_commands[LOGBASE + 2] = 0;

		
		udfs->dux_commands[LENBASE + 3] = steps - steps / 2;
		udfs->dux_commands[OPBASE + 3] = 0;
		udfs->dux_commands[OUTBASE + 3] = 0xFF & rngmask;
		udfs->dux_commands[LOGBASE + 3] = 0;

		
		udfs->dux_commands[LENBASE + 4] = 0x09;
		
		udfs->dux_commands[OPBASE + 4] = 0x01;
		udfs->dux_commands[OUTBASE + 4] = 0xFF & rngmask;
		
		udfs->dux_commands[LOGBASE + 4] = 0xFF;

		break;

	default:
		printk(KERN_ERR "comedi %d: unsupported combination of "
		       "channels\n", dev->minor);
		up(&udfs->sem);
		return -EFAULT;
	}

#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi %d: sending commands to the usb device\n",
	       dev->minor);
#endif
	
	result = send_dux_commands(udfs, SENDADCOMMANDS);
	if (result < 0) {
		printk(KERN_ERR "comedi%d: adc command could not be submitted."
		       "Aborting...\n", dev->minor);
		up(&udfs->sem);
		return result;
	}
	if (cmd->stop_src == TRIG_COUNT) {
		udfs->ai_sample_count = cmd->stop_arg * cmd->scan_end_arg;
		if (udfs->ai_sample_count < 1) {
			printk(KERN_ERR "comedi%d: "
			       "(cmd->stop_arg)*(cmd->scan_end_arg)<1, "
			       "aborting.\n", dev->minor);
			up(&udfs->sem);
			return -EFAULT;
		}
		udfs->ai_continous = 0;
	} else {
		
		udfs->ai_continous = 1;
		udfs->ai_sample_count = 0;
	}

	if ((cmd->start_src == TRIG_NOW) || (cmd->start_src == TRIG_EXT)) {
		
		udfs->ai_cmd_running = 1;
		ret = usbduxfastsub_submit_InURBs(udfs);
		if (ret < 0) {
			udfs->ai_cmd_running = 0;
			
			up(&udfs->sem);
			return ret;
		}
		s->async->inttrig = NULL;
	} else {
		s->async->inttrig = usbduxfast_ai_inttrig;
	}
	up(&udfs->sem);

	return 0;
}

static int usbduxfast_ai_insn_read(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   struct comedi_insn *insn, unsigned int *data)
{
	int i, j, n, actual_length;
	int chan, range, rngmask;
	int err;
	struct usbduxfastsub_s *udfs;

	udfs = dev->private;
	if (!udfs) {
		printk(KERN_ERR "comedi%d: ai_insn_read: no usb dev.\n",
		       dev->minor);
		return -ENODEV;
	}
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi%d: ai_insn_read, insn->n=%d, "
	       "insn->subdev=%d\n", dev->minor, insn->n, insn->subdev);
#endif
	down(&udfs->sem);
	if (!udfs->probed) {
		up(&udfs->sem);
		return -ENODEV;
	}
	if (udfs->ai_cmd_running) {
		printk(KERN_ERR "comedi%d: ai_insn_read not possible. Async "
		       "Command is running.\n", dev->minor);
		up(&udfs->sem);
		return -EBUSY;
	}
	
	chan = CR_CHAN(insn->chanspec);
	range = CR_RANGE(insn->chanspec);
	

	if (range > 0)
		rngmask = 0xff - 0x04;
	else
		rngmask = 0xff;

	
	udfs->dux_commands[LENBASE + 0] = 1;
	
	udfs->dux_commands[OPBASE + 0] = 0x02;
	udfs->dux_commands[OUTBASE + 0] = 0xFF & rngmask;
	udfs->dux_commands[LOGBASE + 0] = 0;

	
	udfs->dux_commands[LENBASE + 1] = 12;
	udfs->dux_commands[OPBASE + 1] = 0;
	udfs->dux_commands[OUTBASE + 1] = 0xFE & rngmask;
	udfs->dux_commands[LOGBASE + 1] = 0;

	udfs->dux_commands[LENBASE + 2] = 1;
	udfs->dux_commands[OPBASE + 2] = 0;
	udfs->dux_commands[OUTBASE + 2] = 0xFE & rngmask;
	udfs->dux_commands[LOGBASE + 2] = 0;

	udfs->dux_commands[LENBASE + 3] = 1;
	udfs->dux_commands[OPBASE + 3] = 0;
	udfs->dux_commands[OUTBASE + 3] = 0xFE & rngmask;
	udfs->dux_commands[LOGBASE + 3] = 0;

	udfs->dux_commands[LENBASE + 4] = 1;
	udfs->dux_commands[OPBASE + 4] = 0;
	udfs->dux_commands[OUTBASE + 4] = 0xFE & rngmask;
	udfs->dux_commands[LOGBASE + 4] = 0;

	
	udfs->dux_commands[LENBASE + 5] = 12;
	udfs->dux_commands[OPBASE + 5] = 0;
	udfs->dux_commands[OUTBASE + 5] = 0xFF & rngmask;
	udfs->dux_commands[LOGBASE + 5] = 0;

	udfs->dux_commands[LENBASE + 6] = 1;
	udfs->dux_commands[OPBASE + 6] = 0;
	udfs->dux_commands[OUTBASE + 6] = 0xFF & rngmask;
	udfs->dux_commands[LOGBASE + 0] = 0;

#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi %d: sending commands to the usb device\n",
	       dev->minor);
#endif
	
	err = send_dux_commands(udfs, SENDADCOMMANDS);
	if (err < 0) {
		printk(KERN_ERR "comedi%d: adc command could not be submitted."
		       "Aborting...\n", dev->minor);
		up(&udfs->sem);
		return err;
	}
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi%d: usbduxfast: submitting in-urb: "
	       "0x%p,0x%p\n", udfs->comedidev->minor, udfs->urbIn->context,
	       udfs->urbIn->dev);
#endif
	for (i = 0; i < PACKETS_TO_IGNORE; i++) {
		err = usb_bulk_msg(udfs->usbdev,
				   usb_rcvbulkpipe(udfs->usbdev, BULKINEP),
				   udfs->transfer_buffer, SIZEINBUF,
				   &actual_length, 10000);
		if (err < 0) {
			printk(KERN_ERR "comedi%d: insn timeout. No data.\n",
			       dev->minor);
			up(&udfs->sem);
			return err;
		}
	}
	
	for (i = 0; i < insn->n;) {
		err = usb_bulk_msg(udfs->usbdev,
				   usb_rcvbulkpipe(udfs->usbdev, BULKINEP),
				   udfs->transfer_buffer, SIZEINBUF,
				   &actual_length, 10000);
		if (err < 0) {
			printk(KERN_ERR "comedi%d: insn data error: %d\n",
			       dev->minor, err);
			up(&udfs->sem);
			return err;
		}
		n = actual_length / sizeof(uint16_t);
		if ((n % 16) != 0) {
			printk(KERN_ERR "comedi%d: insn data packet "
			       "corrupted.\n", dev->minor);
			up(&udfs->sem);
			return -EINVAL;
		}
		for (j = chan; (j < n) && (i < insn->n); j = j + 16) {
			data[i] = ((uint16_t *) (udfs->transfer_buffer))[j];
			i++;
		}
	}
	up(&udfs->sem);
	return i;
}

#define FIRMWARE_MAX_LEN 0x2000

static int firmwareUpload(struct usbduxfastsub_s *usbduxfastsub,
			  const u8 *firmwareBinary, int sizeFirmware)
{
	int ret;
	uint8_t *fwBuf;

	if (!firmwareBinary)
		return 0;

	if (sizeFirmware > FIRMWARE_MAX_LEN) {
		dev_err(&usbduxfastsub->interface->dev,
			"comedi_: usbduxfast firmware binary it too large for FX2.\n");
		return -ENOMEM;
	}

	
	fwBuf = kmemdup(firmwareBinary, sizeFirmware, GFP_KERNEL);
	if (!fwBuf) {
		dev_err(&usbduxfastsub->interface->dev,
			"comedi_: mem alloc for firmware failed\n");
		return -ENOMEM;
	}

	ret = usbduxfastsub_stop(usbduxfastsub);
	if (ret < 0) {
		dev_err(&usbduxfastsub->interface->dev,
			"comedi_: can not stop firmware\n");
		kfree(fwBuf);
		return ret;
	}

	ret = usbduxfastsub_upload(usbduxfastsub, fwBuf, 0, sizeFirmware);
	if (ret < 0) {
		dev_err(&usbduxfastsub->interface->dev,
			"comedi_: firmware upload failed\n");
		kfree(fwBuf);
		return ret;
	}
	ret = usbduxfastsub_start(usbduxfastsub);
	if (ret < 0) {
		dev_err(&usbduxfastsub->interface->dev,
			"comedi_: can not start firmware\n");
		kfree(fwBuf);
		return ret;
	}
	kfree(fwBuf);
	return 0;
}

static void tidy_up(struct usbduxfastsub_s *udfs)
{
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi_: usbduxfast: tiding up\n");
#endif

	if (!udfs)
		return;

	
	if (udfs->interface)
		usb_set_intfdata(udfs->interface, NULL);

	udfs->probed = 0;

	if (udfs->urbIn) {
		
		usb_kill_urb(udfs->urbIn);

		kfree(udfs->transfer_buffer);
		udfs->transfer_buffer = NULL;

		usb_free_urb(udfs->urbIn);
		udfs->urbIn = NULL;
	}

	kfree(udfs->insnBuffer);
	udfs->insnBuffer = NULL;

	kfree(udfs->dux_commands);
	udfs->dux_commands = NULL;

	udfs->ai_cmd_running = 0;
}

static void usbduxfast_firmware_request_complete_handler(const struct firmware
							 *fw, void *context)
{
	struct usbduxfastsub_s *usbduxfastsub_tmp = context;
	struct usb_device *usbdev = usbduxfastsub_tmp->usbdev;
	int ret;

	if (fw == NULL)
		return;

	ret = firmwareUpload(usbduxfastsub_tmp, fw->data, fw->size);

	if (ret) {
		dev_err(&usbdev->dev,
			"Could not upload firmware (err=%d)\n", ret);
		goto out;
	}

	comedi_usb_auto_config(usbdev, BOARDNAME);
 out:
	release_firmware(fw);
}

static int usbduxfastsub_probe(struct usb_interface *uinterf,
			       const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(uinterf);
	int i;
	int index;
	int ret;

	if (udev->speed != USB_SPEED_HIGH) {
		printk(KERN_ERR "comedi_: usbduxfast_: This driver needs"
		       "USB 2.0 to operate. Aborting...\n");
		return -ENODEV;
	}
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi_: usbduxfast_: finding a free structure for "
	       "the usb-device\n");
#endif
	down(&start_stop_sem);
	
	index = -1;
	for (i = 0; i < NUMUSBDUXFAST; i++) {
		if (!usbduxfastsub[i].probed) {
			index = i;
			break;
		}
	}

	
	if (index == -1) {
		printk(KERN_ERR "Too many usbduxfast-devices connected.\n");
		up(&start_stop_sem);
		return -EMFILE;
	}
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi_: usbduxfast: usbduxfastsub[%d] is ready to "
	       "connect to comedi.\n", index);
#endif

	sema_init(&(usbduxfastsub[index].sem), 1);
	
	usbduxfastsub[index].usbdev = udev;

	
	usbduxfastsub[index].interface = uinterf;
	
	usbduxfastsub[index].ifnum = uinterf->altsetting->desc.bInterfaceNumber;
	usb_set_intfdata(uinterf, &(usbduxfastsub[index]));

#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi_: usbduxfast: ifnum=%d\n",
	       usbduxfastsub[index].ifnum);
#endif
	
	usbduxfastsub[index].dux_commands = kmalloc(SIZEOFDUXBUFFER,
						    GFP_KERNEL);
	if (!usbduxfastsub[index].dux_commands) {
		printk(KERN_ERR "comedi_: usbduxfast: error alloc space for "
		       "dac commands\n");
		tidy_up(&(usbduxfastsub[index]));
		up(&start_stop_sem);
		return -ENOMEM;
	}
	
	usbduxfastsub[index].insnBuffer = kmalloc(SIZEINSNBUF, GFP_KERNEL);
	if (!usbduxfastsub[index].insnBuffer) {
		printk(KERN_ERR "comedi_: usbduxfast: could not alloc space "
		       "for insnBuffer\n");
		tidy_up(&(usbduxfastsub[index]));
		up(&start_stop_sem);
		return -ENOMEM;
	}
	
	i = usb_set_interface(usbduxfastsub[index].usbdev,
			      usbduxfastsub[index].ifnum, 1);
	if (i < 0) {
		printk(KERN_ERR "comedi_: usbduxfast%d: could not switch to "
		       "alternate setting 1.\n", index);
		tidy_up(&(usbduxfastsub[index]));
		up(&start_stop_sem);
		return -ENODEV;
	}
	usbduxfastsub[index].urbIn = usb_alloc_urb(0, GFP_KERNEL);
	if (!usbduxfastsub[index].urbIn) {
		printk(KERN_ERR "comedi_: usbduxfast%d: Could not alloc."
		       "urb\n", index);
		tidy_up(&(usbduxfastsub[index]));
		up(&start_stop_sem);
		return -ENOMEM;
	}
	usbduxfastsub[index].transfer_buffer = kmalloc(SIZEINBUF, GFP_KERNEL);
	if (!usbduxfastsub[index].transfer_buffer) {
		printk(KERN_ERR "comedi_: usbduxfast%d: could not alloc. "
		       "transb.\n", index);
		tidy_up(&(usbduxfastsub[index]));
		up(&start_stop_sem);
		return -ENOMEM;
	}
	
	usbduxfastsub[index].probed = 1;
	up(&start_stop_sem);

	ret = request_firmware_nowait(THIS_MODULE,
				      FW_ACTION_HOTPLUG,
				      "usbduxfast_firmware.bin",
				      &udev->dev,
				      GFP_KERNEL,
				      usbduxfastsub + index,
				      usbduxfast_firmware_request_complete_handler);

	if (ret) {
		dev_err(&udev->dev, "could not load firmware (err=%d)\n", ret);
		return ret;
	}

	printk(KERN_INFO "comedi_: usbduxfast%d has been successfully "
	       "initialized.\n", index);
	
	return 0;
}

static void usbduxfastsub_disconnect(struct usb_interface *intf)
{
	struct usbduxfastsub_s *udfs = usb_get_intfdata(intf);
	struct usb_device *udev = interface_to_usbdev(intf);

	if (!udfs) {
		printk(KERN_ERR "comedi_: usbduxfast: disconnect called with "
		       "null pointer.\n");
		return;
	}
	if (udfs->usbdev != udev) {
		printk(KERN_ERR "comedi_: usbduxfast: BUG! called with wrong "
		       "ptr!!!\n");
		return;
	}

	comedi_usb_auto_unconfig(udev);

	down(&start_stop_sem);
	down(&udfs->sem);
	tidy_up(udfs);
	up(&udfs->sem);
	up(&start_stop_sem);

#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi_: usbduxfast: disconnected from the usb\n");
#endif
}

static int usbduxfast_attach(struct comedi_device *dev,
			     struct comedi_devconfig *it)
{
	int ret;
	int index;
	int i;
	struct comedi_subdevice *s = NULL;
	dev->private = NULL;

	down(&start_stop_sem);
	index = -1;
	for (i = 0; i < NUMUSBDUXFAST; i++) {
		if (usbduxfastsub[i].probed && !usbduxfastsub[i].attached) {
			index = i;
			break;
		}
	}

	if (index < 0) {
		printk(KERN_ERR "comedi%d: usbduxfast: error: attach failed, "
		       "no usbduxfast devs connected to the usb bus.\n",
		       dev->minor);
		up(&start_stop_sem);
		return -ENODEV;
	}

	down(&(usbduxfastsub[index].sem));
	
	usbduxfastsub[index].comedidev = dev;

	
	if (comedi_aux_data(it->options, 0) &&
	    it->options[COMEDI_DEVCONF_AUX_DATA_LENGTH]) {
		firmwareUpload(&usbduxfastsub[index],
			       comedi_aux_data(it->options, 0),
			       it->options[COMEDI_DEVCONF_AUX_DATA_LENGTH]);
	}

	dev->board_name = BOARDNAME;

	
	dev->n_subdevices = N_SUBDEVICES;

	
	ret = alloc_subdevices(dev, N_SUBDEVICES);
	if (ret < 0) {
		printk(KERN_ERR "comedi%d: usbduxfast: error alloc space for "
		       "subdev\n", dev->minor);
		up(&(usbduxfastsub[index].sem));
		up(&start_stop_sem);
		return ret;
	}

	printk(KERN_INFO "comedi%d: usbduxfast: usb-device %d is attached to "
	       "comedi.\n", dev->minor, index);
	
	dev->private = usbduxfastsub + index;
	
	s = dev->subdevices + SUBDEV_AD;
	dev->read_subdev = s;
	
	s->private = NULL;
	
	s->type = COMEDI_SUBD_AI;
	
	s->subdev_flags = SDF_READABLE | SDF_GROUND | SDF_CMD_READ;
	
	s->n_chan = 16;
	
	s->len_chanlist = 16;
	
	s->insn_read = usbduxfast_ai_insn_read;
	s->do_cmdtest = usbduxfast_ai_cmdtest;
	s->do_cmd = usbduxfast_ai_cmd;
	s->cancel = usbduxfast_ai_cancel;
	
	s->maxdata = 0x1000;
	
	s->range_table = &range_usbduxfast_ai_range;

	
	usbduxfastsub[index].attached = 1;

	up(&(usbduxfastsub[index].sem));
	up(&start_stop_sem);
	printk(KERN_INFO "comedi%d: successfully attached to usbduxfast.\n",
	       dev->minor);

	return 0;
}

static int usbduxfast_detach(struct comedi_device *dev)
{
	struct usbduxfastsub_s *udfs;

	if (!dev) {
		printk(KERN_ERR "comedi?: usbduxfast: detach without dev "
		       "variable...\n");
		return -EFAULT;
	}
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi%d: usbduxfast: detach usb device\n",
	       dev->minor);
#endif

	udfs = dev->private;
	if (!udfs) {
		printk(KERN_ERR "comedi?: usbduxfast: detach without ptr to "
		       "usbduxfastsub[]\n");
		return -EFAULT;
	}

	down(&udfs->sem);
	down(&start_stop_sem);
	dev->private = NULL;
	udfs->attached = 0;
	udfs->comedidev = NULL;
#ifdef CONFIG_COMEDI_DEBUG
	printk(KERN_DEBUG "comedi%d: usbduxfast: detach: successfully "
	       "removed\n", dev->minor);
#endif
	up(&start_stop_sem);
	up(&udfs->sem);
	return 0;
}

static struct comedi_driver driver_usbduxfast = {
	.driver_name = "usbduxfast",
	.module = THIS_MODULE,
	.attach = usbduxfast_attach,
	.detach = usbduxfast_detach
};

static const struct usb_device_id usbduxfastsub_table[] = {
	
	{USB_DEVICE(0x13d8, 0x0010)},	
	{USB_DEVICE(0x13d8, 0x0011)},	
	{}			
};

MODULE_DEVICE_TABLE(usb, usbduxfastsub_table);

static struct usb_driver usbduxfastsub_driver = {
#ifdef COMEDI_HAVE_USB_DRIVER_OWNER
	.owner = THIS_MODULE,
#endif
	.name = BOARDNAME,
	.probe = usbduxfastsub_probe,
	.disconnect = usbduxfastsub_disconnect,
	.id_table = usbduxfastsub_table
};

static int __init init_usbduxfast(void)
{
	printk(KERN_INFO
	       KBUILD_MODNAME ": " DRIVER_VERSION ":" DRIVER_DESC "\n");
	usb_register(&usbduxfastsub_driver);
	comedi_driver_register(&driver_usbduxfast);
	return 0;
}

static void __exit exit_usbduxfast(void)
{
	comedi_driver_unregister(&driver_usbduxfast);
	usb_deregister(&usbduxfastsub_driver);
}

module_init(init_usbduxfast);
module_exit(exit_usbduxfast);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
