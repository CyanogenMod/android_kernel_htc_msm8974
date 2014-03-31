/* Copyright (C) by Paul Barton-Davis 1998-1999
 *
 * Some portions of this file are taken from work that is
 * copyright (C) by Hannu Savolainen 1993-1996
 *
 * This program is distributed under the GNU GENERAL PUBLIC LICENSE (GPL)
 * Version 2 (June 1991). See the "COPYING" file distributed with this software
 * for more info.  
 */


#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/firmware.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <sound/core.h>
#include <sound/snd_wavefront.h>
#include <sound/initval.h>

static int wf_raw = 0; 
		   
static int fx_raw = 1; 

static int debug_default = 0;  


#define DEFAULT_OSPATH	"wavefront.os"
static char *ospath = DEFAULT_OSPATH; 

static int wait_usecs = 150; 

static int sleep_interval = 100;   
static int sleep_tries = 50;       

static int reset_time = 2;        

static int ramcheck_time = 20;    

static int osrun_time = 10;       
module_param(wf_raw, int, 0444);
MODULE_PARM_DESC(wf_raw, "if non-zero, assume that we need to boot the OS");
module_param(fx_raw, int, 0444);
MODULE_PARM_DESC(fx_raw, "if non-zero, assume that the FX process needs help");
module_param(debug_default, int, 0444);
MODULE_PARM_DESC(debug_default, "debug parameters for card initialization");
module_param(wait_usecs, int, 0444);
MODULE_PARM_DESC(wait_usecs, "how long to wait without sleeping, usecs");
module_param(sleep_interval, int, 0444);
MODULE_PARM_DESC(sleep_interval, "how long to sleep when waiting for reply");
module_param(sleep_tries, int, 0444);
MODULE_PARM_DESC(sleep_tries, "how many times to try sleeping during a wait");
module_param(ospath, charp, 0444);
MODULE_PARM_DESC(ospath, "pathname to processed ICS2115 OS firmware");
module_param(reset_time, int, 0444);
MODULE_PARM_DESC(reset_time, "how long to wait for a reset to take effect");
module_param(ramcheck_time, int, 0444);
MODULE_PARM_DESC(ramcheck_time, "how many seconds to wait for the RAM test");
module_param(osrun_time, int, 0444);
MODULE_PARM_DESC(osrun_time, "how many seconds to wait for the ICS2115 OS");


#define WF_DEBUG 1

#ifdef WF_DEBUG

#define DPRINT(cond, ...) \
       if ((dev->debug & (cond)) == (cond)) { \
	     snd_printk (__VA_ARGS__); \
       }
#else
#define DPRINT(cond, args...)
#endif 

#define LOGNAME "WaveFront: "


#define STAT_RINTR_ENABLED	0x01
#define STAT_CAN_READ		0x02
#define STAT_INTR_READ		0x04
#define STAT_WINTR_ENABLED	0x10
#define STAT_CAN_WRITE		0x20
#define STAT_INTR_WRITE		0x40

static int wavefront_delete_sample (snd_wavefront_t *, int sampnum);
static int wavefront_find_free_sample (snd_wavefront_t *);

struct wavefront_command {
	int cmd;
	char *action;
	unsigned int read_cnt;
	unsigned int write_cnt;
	int need_ack;
};

static struct {
	int errno;
	const char *errstr;
} wavefront_errors[] = {
	{ 0x01, "Bad sample number" },
	{ 0x02, "Out of sample memory" },
	{ 0x03, "Bad patch number" },
	{ 0x04, "Error in number of voices" },
	{ 0x06, "Sample load already in progress" },
	{ 0x0B, "No sample load request pending" },
	{ 0x0E, "Bad MIDI channel number" },
	{ 0x10, "Download Record Error" },
	{ 0x80, "Success" },
	{ 0x0 }
};

#define NEEDS_ACK 1

static struct wavefront_command wavefront_commands[] = {
	{ WFC_SET_SYNTHVOL, "set synthesizer volume", 0, 1, NEEDS_ACK },
	{ WFC_GET_SYNTHVOL, "get synthesizer volume", 1, 0, 0},
	{ WFC_SET_NVOICES, "set number of voices", 0, 1, NEEDS_ACK },
	{ WFC_GET_NVOICES, "get number of voices", 1, 0, 0 },
	{ WFC_SET_TUNING, "set synthesizer tuning", 0, 2, NEEDS_ACK },
	{ WFC_GET_TUNING, "get synthesizer tuning", 2, 0, 0 },
	{ WFC_DISABLE_CHANNEL, "disable synth channel", 0, 1, NEEDS_ACK },
	{ WFC_ENABLE_CHANNEL, "enable synth channel", 0, 1, NEEDS_ACK },
	{ WFC_GET_CHANNEL_STATUS, "get synth channel status", 3, 0, 0 },
	{ WFC_MISYNTH_OFF, "disable midi-in to synth", 0, 0, NEEDS_ACK },
	{ WFC_MISYNTH_ON, "enable midi-in to synth", 0, 0, NEEDS_ACK },
	{ WFC_VMIDI_ON, "enable virtual midi mode", 0, 0, NEEDS_ACK },
	{ WFC_VMIDI_OFF, "disable virtual midi mode", 0, 0, NEEDS_ACK },
	{ WFC_MIDI_STATUS, "report midi status", 1, 0, 0 },
	{ WFC_FIRMWARE_VERSION, "report firmware version", 2, 0, 0 },
	{ WFC_HARDWARE_VERSION, "report hardware version", 2, 0, 0 },
	{ WFC_GET_NSAMPLES, "report number of samples", 2, 0, 0 },
	{ WFC_INSTOUT_LEVELS, "report instantaneous output levels", 7, 0, 0 },
	{ WFC_PEAKOUT_LEVELS, "report peak output levels", 7, 0, 0 },
	{ WFC_DOWNLOAD_SAMPLE, "download sample",
	  0, WF_SAMPLE_BYTES, NEEDS_ACK },
	{ WFC_DOWNLOAD_BLOCK, "download block", 0, 0, NEEDS_ACK},
	{ WFC_DOWNLOAD_SAMPLE_HEADER, "download sample header",
	  0, WF_SAMPLE_HDR_BYTES, NEEDS_ACK },
	{ WFC_UPLOAD_SAMPLE_HEADER, "upload sample header", 13, 2, 0 },

	/* This command requires a variable number of bytes to be written.
	   There is a hack in snd_wavefront_cmd() to support this. The actual
	   count is passed in as the read buffer ptr, cast appropriately.
	   Ugh.
	*/

	{ WFC_DOWNLOAD_MULTISAMPLE, "download multisample", 0, 0, NEEDS_ACK },


	{ WFC_UPLOAD_MULTISAMPLE, "upload multisample", 2, 1, 0 },
	{ WFC_DOWNLOAD_SAMPLE_ALIAS, "download sample alias",
	  0, WF_ALIAS_BYTES, NEEDS_ACK },
	{ WFC_UPLOAD_SAMPLE_ALIAS, "upload sample alias", WF_ALIAS_BYTES, 2, 0},
	{ WFC_DELETE_SAMPLE, "delete sample", 0, 2, NEEDS_ACK },
	{ WFC_IDENTIFY_SAMPLE_TYPE, "identify sample type", 5, 2, 0 },
	{ WFC_UPLOAD_SAMPLE_PARAMS, "upload sample parameters" },
	{ WFC_REPORT_FREE_MEMORY, "report free memory", 4, 0, 0 },
	{ WFC_DOWNLOAD_PATCH, "download patch", 0, 134, NEEDS_ACK },
	{ WFC_UPLOAD_PATCH, "upload patch", 132, 2, 0 },
	{ WFC_DOWNLOAD_PROGRAM, "download program", 0, 33, NEEDS_ACK },
	{ WFC_UPLOAD_PROGRAM, "upload program", 32, 1, 0 },
	{ WFC_DOWNLOAD_EDRUM_PROGRAM, "download enhanced drum program", 0, 9,
	  NEEDS_ACK},
	{ WFC_UPLOAD_EDRUM_PROGRAM, "upload enhanced drum program", 8, 1, 0},
	{ WFC_SET_EDRUM_CHANNEL, "set enhanced drum program channel",
	  0, 1, NEEDS_ACK },
	{ WFC_DISABLE_DRUM_PROGRAM, "disable drum program", 0, 1, NEEDS_ACK },
	{ WFC_REPORT_CHANNEL_PROGRAMS, "report channel program numbers",
	  32, 0, 0 },
	{ WFC_NOOP, "the no-op command", 0, 0, NEEDS_ACK },
	{ 0x00 }
};

static const char *
wavefront_errorstr (int errnum)

{
	int i;

	for (i = 0; wavefront_errors[i].errstr; i++) {
		if (wavefront_errors[i].errno == errnum) {
			return wavefront_errors[i].errstr;
		}
	}

	return "Unknown WaveFront error";
}

static struct wavefront_command *
wavefront_get_command (int cmd) 

{
	int i;

	for (i = 0; wavefront_commands[i].cmd != 0; i++) {
		if (cmd == wavefront_commands[i].cmd) {
			return &wavefront_commands[i];
		}
	}

	return NULL;
}

static inline int
wavefront_status (snd_wavefront_t *dev) 

{
	return inb (dev->status_port);
}

static int
wavefront_sleep (int limit)

{
	schedule_timeout_interruptible(limit);

	return signal_pending(current);
}

static int
wavefront_wait (snd_wavefront_t *dev, int mask)

{
	int             i;


	for (i = 0; i < wait_usecs; i += 5) {
		if (wavefront_status (dev) & mask) {
			return 1;
		}
		udelay(5);
	}

	for (i = 0; i < sleep_tries; i++) {

		if (wavefront_status (dev) & mask) {
			return 1;
		}

		if (wavefront_sleep (HZ/sleep_interval)) {
			return (0);
		}
	}

	return (0);
}

static int
wavefront_read (snd_wavefront_t *dev)

{
	if (wavefront_wait (dev, STAT_CAN_READ))
		return inb (dev->data_port);

	DPRINT (WF_DEBUG_DATA, "read timeout.\n");

	return -1;
}

static int
wavefront_write (snd_wavefront_t *dev, unsigned char data)

{
	if (wavefront_wait (dev, STAT_CAN_WRITE)) {
		outb (data, dev->data_port);
		return 0;
	}

	DPRINT (WF_DEBUG_DATA, "write timeout.\n");

	return -1;
}

int
snd_wavefront_cmd (snd_wavefront_t *dev, 
		   int cmd, unsigned char *rbuf, unsigned char *wbuf)

{
	int ack;
	unsigned int i;
	int c;
	struct wavefront_command *wfcmd;

	if ((wfcmd = wavefront_get_command (cmd)) == NULL) {
		snd_printk ("command 0x%x not supported.\n",
			cmd);
		return 1;
	}


	if (cmd == WFC_DOWNLOAD_MULTISAMPLE) {
		wfcmd->write_cnt = (unsigned long) rbuf;
		rbuf = NULL;
	}

	DPRINT (WF_DEBUG_CMD, "0x%x [%s] (%d,%d,%d)\n",
			       cmd, wfcmd->action, wfcmd->read_cnt,
			       wfcmd->write_cnt, wfcmd->need_ack);
    
	if (wavefront_write (dev, cmd)) { 
		DPRINT ((WF_DEBUG_IO|WF_DEBUG_CMD), "cannot request "
						     "0x%x [%s].\n",
						     cmd, wfcmd->action);
		return 1;
	} 

	if (wfcmd->write_cnt > 0) {
		DPRINT (WF_DEBUG_DATA, "writing %d bytes "
					"for 0x%x\n",
					wfcmd->write_cnt, cmd);

		for (i = 0; i < wfcmd->write_cnt; i++) {
			if (wavefront_write (dev, wbuf[i])) {
				DPRINT (WF_DEBUG_IO, "bad write for byte "
						      "%d of 0x%x [%s].\n",
						      i, cmd, wfcmd->action);
				return 1;
			}

			DPRINT (WF_DEBUG_DATA, "write[%d] = 0x%x\n",
						i, wbuf[i]);
		}
	}

	if (wfcmd->read_cnt > 0) {
		DPRINT (WF_DEBUG_DATA, "reading %d ints "
					"for 0x%x\n",
					wfcmd->read_cnt, cmd);

		for (i = 0; i < wfcmd->read_cnt; i++) {

			if ((c = wavefront_read (dev)) == -1) {
				DPRINT (WF_DEBUG_IO, "bad read for byte "
						      "%d of 0x%x [%s].\n",
						      i, cmd, wfcmd->action);
				return 1;
			}

			
	    
			if (c == 0xff) { 
				if ((c = wavefront_read (dev)) == -1) {
					DPRINT (WF_DEBUG_IO, "bad read for "
							      "error byte at "
							      "read byte %d "
							      "of 0x%x [%s].\n",
							      i, cmd,
							      wfcmd->action);
					return 1;
				}

				

				if (c == 1 &&
				    wfcmd->cmd == WFC_IDENTIFY_SAMPLE_TYPE) {
					rbuf[0] = WF_ST_EMPTY;
					return (0);

				} else if (c == 3 &&
					   wfcmd->cmd == WFC_UPLOAD_PATCH) {

					return 3;

				} else if (c == 1 &&
					   wfcmd->cmd == WFC_UPLOAD_PROGRAM) {

					return 1;

				} else {

					DPRINT (WF_DEBUG_IO, "error %d (%s) "
							      "during "
							      "read for byte "
							      "%d of 0x%x "
							      "[%s].\n",
							      c,
							      wavefront_errorstr (c),
							      i, cmd,
							      wfcmd->action);
					return 1;

				}
		
		} else {
				rbuf[i] = c;
			}
			
			DPRINT (WF_DEBUG_DATA, "read[%d] = 0x%x\n",i, rbuf[i]);
		}
	}
	
	if ((wfcmd->read_cnt == 0 && wfcmd->write_cnt == 0) || wfcmd->need_ack) {

		DPRINT (WF_DEBUG_CMD, "reading ACK for 0x%x\n", cmd);

	    
		if ((ack = wavefront_read (dev)) == 0) {
			ack = WF_ACK;
		}
	
		if (ack != WF_ACK) {
			if (ack == -1) {
				DPRINT (WF_DEBUG_IO, "cannot read ack for "
						      "0x%x [%s].\n",
						      cmd, wfcmd->action);
				return 1;
		
			} else {
				int err = -1; 

				if (ack == 0xff) { 
		    
					if ((err = wavefront_read (dev)) == -1) {
						DPRINT (WF_DEBUG_DATA,
							"cannot read err "
							"for 0x%x [%s].\n",
							cmd, wfcmd->action);
					}
				}
				
				DPRINT (WF_DEBUG_IO, "0x%x [%s] "
					"failed (0x%x, 0x%x, %s)\n",
					cmd, wfcmd->action, ack, err,
					wavefront_errorstr (err));
				
				return -err;
			}
		}
		
		DPRINT (WF_DEBUG_DATA, "ack received "
					"for 0x%x [%s]\n",
					cmd, wfcmd->action);
	} else {

		DPRINT (WF_DEBUG_CMD, "0x%x [%s] does not need "
				       "ACK (%d,%d,%d)\n",
				       cmd, wfcmd->action, wfcmd->read_cnt,
				       wfcmd->write_cnt, wfcmd->need_ack);
	}

	return 0;
	
}

/***********************************************************************
WaveFront data munging   

Things here are weird. All data written to the board cannot 
have its most significant bit set. Any data item with values 
potentially > 0x7F (127) must be split across multiple bytes.

Sometimes, we need to munge numeric values that are represented on
the x86 side as 8-32 bit values. Sometimes, we need to munge data
that is represented on the x86 side as an array of bytes. The most
efficient approach to handling both cases seems to be to use 2
different functions for munging and 2 for de-munging. This avoids
weird casting and worrying about bit-level offsets.

**********************************************************************/

static unsigned char *
munge_int32 (unsigned int src,
	     unsigned char *dst,
	     unsigned int dst_size)
{
	unsigned int i;

	for (i = 0; i < dst_size; i++) {
		*dst = src & 0x7F;  
		src = src >> 7;     
	                             

		dst++;
 	};
	return dst;
};

static int 
demunge_int32 (unsigned char* src, int src_size)

{
	int i;
 	int outval = 0;
	
 	for (i = src_size - 1; i >= 0; i--) {
		outval=(outval<<7)+src[i];
	}

	return outval;
};

static 
unsigned char *
munge_buf (unsigned char *src, unsigned char *dst, unsigned int dst_size)

{
	unsigned int i;
	unsigned int last = dst_size / 2;

	for (i = 0; i < last; i++) {
		*dst++ = src[i] & 0x7f;
		*dst++ = src[i] >> 7;
	}
	return dst;
}

static 
unsigned char *
demunge_buf (unsigned char *src, unsigned char *dst, unsigned int src_bytes)

{
	int i;
	unsigned char *end = src + src_bytes;
    
	end = src + src_bytes;

	

	for (i = 0; src != end; i++) {
		dst[i] = *src++;
		dst[i] |= (*src++)<<7;
	}

	return dst;
}


static int
wavefront_delete_sample (snd_wavefront_t *dev, int sample_num)

{
	unsigned char wbuf[2];
	int x;

	wbuf[0] = sample_num & 0x7f;
	wbuf[1] = sample_num >> 7;

	if ((x = snd_wavefront_cmd (dev, WFC_DELETE_SAMPLE, NULL, wbuf)) == 0) {
		dev->sample_status[sample_num] = WF_ST_EMPTY;
	}

	return x;
}

static int
wavefront_get_sample_status (snd_wavefront_t *dev, int assume_rom)

{
	int i;
	unsigned char rbuf[32], wbuf[32];
	unsigned int    sc_real, sc_alias, sc_multi;

	
    
	if (snd_wavefront_cmd (dev, WFC_GET_NSAMPLES, rbuf, wbuf)) {
		snd_printk ("cannot request sample count.\n");
		return -1;
	} 
    
	sc_real = sc_alias = sc_multi = dev->samples_used = 0;
    
	for (i = 0; i < WF_MAX_SAMPLE; i++) {
	
		wbuf[0] = i & 0x7f;
		wbuf[1] = i >> 7;

		if (snd_wavefront_cmd (dev, WFC_IDENTIFY_SAMPLE_TYPE, rbuf, wbuf)) {
			snd_printk(KERN_WARNING "cannot identify sample "
				   "type of slot %d\n", i);
			dev->sample_status[i] = WF_ST_EMPTY;
			continue;
		}

		dev->sample_status[i] = (WF_SLOT_FILLED|rbuf[0]);

		if (assume_rom) {
			dev->sample_status[i] |= WF_SLOT_ROM;
		}

		switch (rbuf[0] & WF_ST_MASK) {
		case WF_ST_SAMPLE:
			sc_real++;
			break;
		case WF_ST_MULTISAMPLE:
			sc_multi++;
			break;
		case WF_ST_ALIAS:
			sc_alias++;
			break;
		case WF_ST_EMPTY:
			break;

		default:
			snd_printk ("unknown sample type for "
				    "slot %d (0x%x)\n", 
				    i, rbuf[0]);
		}

		if (rbuf[0] != WF_ST_EMPTY) {
			dev->samples_used++;
		} 
	}

	snd_printk ("%d samples used (%d real, %d aliases, %d multi), "
		    "%d empty\n", dev->samples_used, sc_real, sc_alias, sc_multi,
		    WF_MAX_SAMPLE - dev->samples_used);


	return (0);

}

static int
wavefront_get_patch_status (snd_wavefront_t *dev)

{
	unsigned char patchbuf[WF_PATCH_BYTES];
	unsigned char patchnum[2];
	wavefront_patch *p;
	int i, x, cnt, cnt2;

	for (i = 0; i < WF_MAX_PATCH; i++) {
		patchnum[0] = i & 0x7f;
		patchnum[1] = i >> 7;

		if ((x = snd_wavefront_cmd (dev, WFC_UPLOAD_PATCH, patchbuf,
					patchnum)) == 0) {

			dev->patch_status[i] |= WF_SLOT_FILLED;
			p = (wavefront_patch *) patchbuf;
			dev->sample_status
				[p->sample_number|(p->sample_msb<<7)] |=
				WF_SLOT_USED;
	    
		} else if (x == 3) { 
			dev->patch_status[i] = 0;
		} else {
			snd_printk ("upload patch "
				    "error 0x%x\n", x);
			dev->patch_status[i] = 0;
			return 1;
		}
	}

	

	for (i = 0, cnt = 0, cnt2 = 0; i < WF_MAX_PATCH; i++) {
		if (dev->patch_status[i] & WF_SLOT_FILLED) {
			cnt++;
		}
		if (dev->patch_status[i] & WF_SLOT_USED) {
			cnt2++;
		}
	
	}
	snd_printk ("%d patch slots filled, %d in use\n", cnt, cnt2);

	return (0);
}

static int
wavefront_get_program_status (snd_wavefront_t *dev)

{
	unsigned char progbuf[WF_PROGRAM_BYTES];
	wavefront_program prog;
	unsigned char prognum;
	int i, x, l, cnt;

	for (i = 0; i < WF_MAX_PROGRAM; i++) {
		prognum = i;

		if ((x = snd_wavefront_cmd (dev, WFC_UPLOAD_PROGRAM, progbuf,
					&prognum)) == 0) {

			dev->prog_status[i] |= WF_SLOT_USED;

			demunge_buf (progbuf, (unsigned char *) &prog,
				     WF_PROGRAM_BYTES);

			for (l = 0; l < WF_NUM_LAYERS; l++) {
				if (prog.layer[l].mute) {
					dev->patch_status
						[prog.layer[l].patch_number] |=
						WF_SLOT_USED;
				}
			}
		} else if (x == 1) { 
			dev->prog_status[i] = 0;
		} else {
			snd_printk ("upload program "
				    "error 0x%x\n", x);
			dev->prog_status[i] = 0;
		}
	}

	for (i = 0, cnt = 0; i < WF_MAX_PROGRAM; i++) {
		if (dev->prog_status[i]) {
			cnt++;
		}
	}

	snd_printk ("%d programs slots in use\n", cnt);

	return (0);
}

static int
wavefront_send_patch (snd_wavefront_t *dev, wavefront_patch_info *header)

{
	unsigned char buf[WF_PATCH_BYTES+2];
	unsigned char *bptr;

	DPRINT (WF_DEBUG_LOAD_PATCH, "downloading patch %d\n",
				      header->number);

	dev->patch_status[header->number] |= WF_SLOT_FILLED;

	bptr = buf;
	bptr = munge_int32 (header->number, buf, 2);
	munge_buf ((unsigned char *)&header->hdr.p, bptr, WF_PATCH_BYTES);
    
	if (snd_wavefront_cmd (dev, WFC_DOWNLOAD_PATCH, NULL, buf)) {
		snd_printk ("download patch failed\n");
		return -(EIO);
	}

	return (0);
}

static int
wavefront_send_program (snd_wavefront_t *dev, wavefront_patch_info *header)

{
	unsigned char buf[WF_PROGRAM_BYTES+1];
	int i;

	DPRINT (WF_DEBUG_LOAD_PATCH, "downloading program %d\n",
		header->number);

	dev->prog_status[header->number] = WF_SLOT_USED;

	/* XXX need to zero existing SLOT_USED bit for program_status[i]
	   where `i' is the program that's being (potentially) overwritten.
	*/
    
	for (i = 0; i < WF_NUM_LAYERS; i++) {
		if (header->hdr.pr.layer[i].mute) {
			dev->patch_status[header->hdr.pr.layer[i].patch_number] |=
				WF_SLOT_USED;

		}
	}

	buf[0] = header->number;
	munge_buf ((unsigned char *)&header->hdr.pr, &buf[1], WF_PROGRAM_BYTES);
    
	if (snd_wavefront_cmd (dev, WFC_DOWNLOAD_PROGRAM, NULL, buf)) {
		snd_printk ("download patch failed\n");	
		return -(EIO);
	}

	return (0);
}

static int
wavefront_freemem (snd_wavefront_t *dev)

{
	char rbuf[8];

	if (snd_wavefront_cmd (dev, WFC_REPORT_FREE_MEMORY, rbuf, NULL)) {
		snd_printk ("can't get memory stats.\n");
		return -1;
	} else {
		return demunge_int32 (rbuf, 4);
	}
}

static int
wavefront_send_sample (snd_wavefront_t *dev, 
		       wavefront_patch_info *header,
		       u16 __user *dataptr,
		       int data_is_unsigned)

{

	u16 sample_short = 0;
	u32 length;
	u16 __user *data_end = NULL;
	unsigned int i;
	const unsigned int max_blksize = 4096/2;
	unsigned int written;
	unsigned int blocksize;
	int dma_ack;
	int blocknum;
	unsigned char sample_hdr[WF_SAMPLE_HDR_BYTES];
	unsigned char *shptr;
	int skip = 0;
	int initial_skip = 0;

	DPRINT (WF_DEBUG_LOAD_PATCH, "sample %sdownload for slot %d, "
				      "type %d, %d bytes from 0x%lx\n",
				      header->size ? "" : "header ", 
				      header->number, header->subkey,
				      header->size,
				      (unsigned long) header->dataptr);

	if (header->number == WAVEFRONT_FIND_FREE_SAMPLE_SLOT) {
		int x;

		if ((x = wavefront_find_free_sample (dev)) < 0) {
			return -ENOMEM;
		}
		snd_printk ("unspecified sample => %d\n", x);
		header->number = x;
	}

	if (header->size) {


		if (dev->rom_samples_rdonly) {
			if (dev->sample_status[header->number] & WF_SLOT_ROM) {
				snd_printk ("sample slot %d "
					    "write protected\n",
					    header->number);
				return -EACCES;
			}
		}

		wavefront_delete_sample (dev, header->number);
	}

	if (header->size) {
		dev->freemem = wavefront_freemem (dev);

		if (dev->freemem < (int)header->size) {
			snd_printk ("insufficient memory to "
				    "load %d byte sample.\n",
				    header->size);
			return -ENOMEM;
		}
	
	}

	skip = WF_GET_CHANNEL(&header->hdr.s);

	if (skip > 0 && header->hdr.s.SampleResolution != LINEAR_16BIT) {
		snd_printk ("channel selection only "
			    "possible on 16-bit samples");
		return -(EINVAL);
	}

	switch (skip) {
	case 0:
		initial_skip = 0;
		skip = 1;
		break;
	case 1:
		initial_skip = 0;
		skip = 2;
		break;
	case 2:
		initial_skip = 1;
		skip = 2;
		break;
	case 3:
		initial_skip = 2;
		skip = 3;
		break;
	case 4:
		initial_skip = 3;
		skip = 4;
		break;
	case 5:
		initial_skip = 4;
		skip = 5;
		break;
	case 6:
		initial_skip = 5;
		skip = 6;
		break;
	}

	DPRINT (WF_DEBUG_LOAD_PATCH, "channel selection: %d => "
				      "initial skip = %d, skip = %d\n",
				      WF_GET_CHANNEL (&header->hdr.s),
				      initial_skip, skip);
    
	

	WF_SET_CHANNEL(&header->hdr.s, 0);


	length = header->size / 2;


	shptr = &sample_hdr[0];

	shptr = munge_int32 (header->number, shptr, 2);

	if (header->size) {
		shptr = munge_int32 (length, shptr, 4);
	}


	shptr = munge_int32 (*((u32 *) &header->hdr.s.sampleStartOffset),
			     shptr, 4);
	shptr = munge_int32 (*((u32 *) &header->hdr.s.loopStartOffset),
			     shptr, 4);
	shptr = munge_int32 (*((u32 *) &header->hdr.s.loopEndOffset),
			     shptr, 4);
	shptr = munge_int32 (*((u32 *) &header->hdr.s.sampleEndOffset),
			     shptr, 4);
	
	
	shptr = munge_int32 (header->hdr.s.FrequencyBias, shptr, 3);
	
	
	shptr = munge_int32 (*(&header->hdr.s.FrequencyBias+1),
			     shptr, 2);

	if (snd_wavefront_cmd (dev, 
			   header->size ?
			   WFC_DOWNLOAD_SAMPLE : WFC_DOWNLOAD_SAMPLE_HEADER,
			   NULL, sample_hdr)) {
		snd_printk ("sample %sdownload refused.\n",
			    header->size ? "" : "header ");
		return -(EIO);
	}

	if (header->size == 0) {
		goto sent; 
	}
    
	data_end = dataptr + length;

	

	dataptr += initial_skip;
    
	for (written = 0, blocknum = 0;
	     written < length; written += max_blksize, blocknum++) {
	
		if ((length - written) > max_blksize) {
			blocksize = max_blksize;
		} else {
			
			blocksize = ALIGN(length - written, 8);
		}

		if (snd_wavefront_cmd (dev, WFC_DOWNLOAD_BLOCK, NULL, NULL)) {
			snd_printk ("download block "
				    "request refused.\n");
			return -(EIO);
		}

		for (i = 0; i < blocksize; i++) {

			if (dataptr < data_end) {
		
				__get_user (sample_short, dataptr);
				dataptr += skip;
		
				if (data_is_unsigned) { 

					if (WF_SAMPLE_IS_8BIT(&header->hdr.s)) {
			
			
						((unsigned char*)
						 &sample_short)[0] += 0x7f;
						((unsigned char*)
						 &sample_short)[1] += 0x7f;
			
					} else {
			
			
						sample_short += 0x7fff;
					}
				}

			} else {

			}
	    
			if (i < blocksize - 1) {
				outw (sample_short, dev->block_port);
			} else {
				outw (sample_short, dev->last_block_port);
			}
		}

	
		if ((dma_ack = wavefront_read (dev)) != WF_DMA_ACK) {
			if (dma_ack == -1) {
				snd_printk ("upload sample "
					    "DMA ack timeout\n");
				return -(EIO);
			} else {
				snd_printk ("upload sample "
					    "DMA ack error 0x%x\n",
					    dma_ack);
				return -(EIO);
			}
		}
	}

	dev->sample_status[header->number] = (WF_SLOT_FILLED|WF_ST_SAMPLE);


 sent:
	return (0);
}

static int
wavefront_send_alias (snd_wavefront_t *dev, wavefront_patch_info *header)

{
	unsigned char alias_hdr[WF_ALIAS_BYTES];

	DPRINT (WF_DEBUG_LOAD_PATCH, "download alias, %d is "
				      "alias for %d\n",
				      header->number,
				      header->hdr.a.OriginalSample);
    
	munge_int32 (header->number, &alias_hdr[0], 2);
	munge_int32 (header->hdr.a.OriginalSample, &alias_hdr[2], 2);
	munge_int32 (*((unsigned int *)&header->hdr.a.sampleStartOffset),
		     &alias_hdr[4], 4);
	munge_int32 (*((unsigned int *)&header->hdr.a.loopStartOffset),
		     &alias_hdr[8], 4);
	munge_int32 (*((unsigned int *)&header->hdr.a.loopEndOffset),
		     &alias_hdr[12], 4);
	munge_int32 (*((unsigned int *)&header->hdr.a.sampleEndOffset),
		     &alias_hdr[16], 4);
	munge_int32 (header->hdr.a.FrequencyBias, &alias_hdr[20], 3);
	munge_int32 (*(&header->hdr.a.FrequencyBias+1), &alias_hdr[23], 2);

	if (snd_wavefront_cmd (dev, WFC_DOWNLOAD_SAMPLE_ALIAS, NULL, alias_hdr)) {
		snd_printk ("download alias failed.\n");
		return -(EIO);
	}

	dev->sample_status[header->number] = (WF_SLOT_FILLED|WF_ST_ALIAS);

	return (0);
}

static int
wavefront_send_multisample (snd_wavefront_t *dev, wavefront_patch_info *header)
{
	int i;
	int num_samples;
	unsigned char *msample_hdr;

	msample_hdr = kmalloc(sizeof(WF_MSAMPLE_BYTES), GFP_KERNEL);
	if (! msample_hdr)
		return -ENOMEM;

	munge_int32 (header->number, &msample_hdr[0], 2);


	num_samples = (1<<(header->hdr.ms.NumberOfSamples&7));
	msample_hdr[2] = (unsigned char) header->hdr.ms.NumberOfSamples;

	DPRINT (WF_DEBUG_LOAD_PATCH, "multi %d with %d=%d samples\n",
				      header->number,
				      header->hdr.ms.NumberOfSamples,
				      num_samples);

	for (i = 0; i < num_samples; i++) {
		DPRINT(WF_DEBUG_LOAD_PATCH|WF_DEBUG_DATA, "sample[%d] = %d\n",
		       i, header->hdr.ms.SampleNumber[i]);
		munge_int32 (header->hdr.ms.SampleNumber[i],
		     &msample_hdr[3+(i*2)], 2);
	}
    
	/* Need a hack here to pass in the number of bytes
	   to be written to the synth. This is ugly, and perhaps
	   one day, I'll fix it.
	*/

	if (snd_wavefront_cmd (dev, WFC_DOWNLOAD_MULTISAMPLE, 
			   (unsigned char *) (long) ((num_samples*2)+3),
			   msample_hdr)) {
		snd_printk ("download of multisample failed.\n");
		kfree(msample_hdr);
		return -(EIO);
	}

	dev->sample_status[header->number] = (WF_SLOT_FILLED|WF_ST_MULTISAMPLE);

	kfree(msample_hdr);
	return (0);
}

static int
wavefront_fetch_multisample (snd_wavefront_t *dev, 
			     wavefront_patch_info *header)
{
	int i;
	unsigned char log_ns[1];
	unsigned char number[2];
	int num_samples;

	munge_int32 (header->number, number, 2);
    
	if (snd_wavefront_cmd (dev, WFC_UPLOAD_MULTISAMPLE, log_ns, number)) {
		snd_printk ("upload multisample failed.\n");
		return -(EIO);
	}
    
	DPRINT (WF_DEBUG_DATA, "msample %d has %d samples\n",
				header->number, log_ns[0]);

	header->hdr.ms.NumberOfSamples = log_ns[0];

	

	num_samples = (1 << log_ns[0]);
    
	for (i = 0; i < num_samples; i++) {
		char d[2];
		int val;
	
		if ((val = wavefront_read (dev)) == -1) {
			snd_printk ("upload multisample failed "
				    "during sample loop.\n");
			return -(EIO);
		}
		d[0] = val;

		if ((val = wavefront_read (dev)) == -1) {
			snd_printk ("upload multisample failed "
				    "during sample loop.\n");
			return -(EIO);
		}
		d[1] = val;
	
		header->hdr.ms.SampleNumber[i] =
			demunge_int32 ((unsigned char *) d, 2);
	
		DPRINT (WF_DEBUG_DATA, "msample sample[%d] = %d\n",
					i, header->hdr.ms.SampleNumber[i]);
	}

	return (0);
}


static int
wavefront_send_drum (snd_wavefront_t *dev, wavefront_patch_info *header)

{
	unsigned char drumbuf[WF_DRUM_BYTES];
	wavefront_drum *drum = &header->hdr.d;
	int i;

	DPRINT (WF_DEBUG_LOAD_PATCH, "downloading edrum for MIDI "
		"note %d, patch = %d\n", 
		header->number, drum->PatchNumber);

	drumbuf[0] = header->number & 0x7f;

	for (i = 0; i < 4; i++) {
		munge_int32 (((unsigned char *)drum)[i], &drumbuf[1+(i*2)], 2);
	}

	if (snd_wavefront_cmd (dev, WFC_DOWNLOAD_EDRUM_PROGRAM, NULL, drumbuf)) {
		snd_printk ("download drum failed.\n");
		return -(EIO);
	}

	return (0);
}

static int 
wavefront_find_free_sample (snd_wavefront_t *dev)

{
	int i;

	for (i = 0; i < WF_MAX_SAMPLE; i++) {
		if (!(dev->sample_status[i] & WF_SLOT_FILLED)) {
			return i;
		}
	}
	snd_printk ("no free sample slots!\n");
	return -1;
}

#if 0
static int 
wavefront_find_free_patch (snd_wavefront_t *dev)

{
	int i;

	for (i = 0; i < WF_MAX_PATCH; i++) {
		if (!(dev->patch_status[i] & WF_SLOT_FILLED)) {
			return i;
		}
	}
	snd_printk ("no free patch slots!\n");
	return -1;
}
#endif

static int
wavefront_load_patch (snd_wavefront_t *dev, const char __user *addr)
{
	wavefront_patch_info *header;
	int err;
	
	header = kmalloc(sizeof(*header), GFP_KERNEL);
	if (! header)
		return -ENOMEM;

	if (copy_from_user (header, addr, sizeof(wavefront_patch_info) -
			    sizeof(wavefront_any))) {
		snd_printk ("bad address for load patch.\n");
		err = -EFAULT;
		goto __error;
	}

	DPRINT (WF_DEBUG_LOAD_PATCH, "download "
				      "Sample type: %d "
				      "Sample number: %d "
				      "Sample size: %d\n",
				      header->subkey,
				      header->number,
				      header->size);

	switch (header->subkey) {
	case WF_ST_SAMPLE:  

		if (copy_from_user (&header->hdr.s, header->hdrptr,
				    sizeof (wavefront_sample))) {
			err = -EFAULT;
			break;
		}

		err = wavefront_send_sample (dev, header, header->dataptr, 0);
		break;

	case WF_ST_MULTISAMPLE:

		if (copy_from_user (&header->hdr.s, header->hdrptr,
				    sizeof (wavefront_multisample))) {
			err = -EFAULT;
			break;
		}

		err = wavefront_send_multisample (dev, header);
		break;

	case WF_ST_ALIAS:

		if (copy_from_user (&header->hdr.a, header->hdrptr,
				    sizeof (wavefront_alias))) {
			err = -EFAULT;
			break;
		}

		err = wavefront_send_alias (dev, header);
		break;

	case WF_ST_DRUM:
		if (copy_from_user (&header->hdr.d, header->hdrptr,
				    sizeof (wavefront_drum))) {
			err = -EFAULT;
			break;
		}

		err = wavefront_send_drum (dev, header);
		break;

	case WF_ST_PATCH:
		if (copy_from_user (&header->hdr.p, header->hdrptr,
				    sizeof (wavefront_patch))) {
			err = -EFAULT;
			break;
		}
		
		err = wavefront_send_patch (dev, header);
		break;

	case WF_ST_PROGRAM:
		if (copy_from_user (&header->hdr.pr, header->hdrptr,
				    sizeof (wavefront_program))) {
			err = -EFAULT;
			break;
		}

		err = wavefront_send_program (dev, header);
		break;

	default:
		snd_printk ("unknown patch type %d.\n",
			    header->subkey);
		err = -EINVAL;
		break;
	}

 __error:
	kfree(header);
	return err;
}


static void
process_sample_hdr (u8 *buf)

{
	wavefront_sample s;
	u8 *ptr;

	ptr = buf;


	*((u32 *) &s.sampleStartOffset) = demunge_int32 (ptr, 4); ptr += 4;
	*((u32 *) &s.loopStartOffset) = demunge_int32 (ptr, 4); ptr += 4;
	*((u32 *) &s.loopEndOffset) = demunge_int32 (ptr, 4); ptr += 4;
	*((u32 *) &s.sampleEndOffset) = demunge_int32 (ptr, 4); ptr += 4;
	*((u32 *) &s.FrequencyBias) = demunge_int32 (ptr, 3); ptr += 3;

	s.SampleResolution = *ptr & 0x3;
	s.Loop = *ptr & 0x8;
	s.Bidirectional = *ptr & 0x10;
	s.Reverse = *ptr & 0x40;

	

	memcpy (buf, (unsigned char *) &s, sizeof (wavefront_sample));
}

static int
wavefront_synth_control (snd_wavefront_card_t *acard, 
			 wavefront_control *wc)

{
	snd_wavefront_t *dev = &acard->wavefront;
	unsigned char patchnumbuf[2];
	int i;

	DPRINT (WF_DEBUG_CMD, "synth control with "
		"cmd 0x%x\n", wc->cmd);

	

	switch (wc->cmd) {
		
	case WFC_DISABLE_INTERRUPTS:
		snd_printk ("interrupts disabled.\n");
		outb (0x80|0x20, dev->control_port);
		dev->interrupts_are_midi = 1;
		return 0;

	case WFC_ENABLE_INTERRUPTS:
		snd_printk ("interrupts enabled.\n");
		outb (0x80|0x40|0x20, dev->control_port);
		dev->interrupts_are_midi = 1;
		return 0;

	case WFC_INTERRUPT_STATUS:
		wc->rbuf[0] = dev->interrupts_are_midi;
		return 0;

	case WFC_ROMSAMPLES_RDONLY:
		dev->rom_samples_rdonly = wc->wbuf[0];
		wc->status = 0;
		return 0;

	case WFC_IDENTIFY_SLOT_TYPE:
		i = wc->wbuf[0] | (wc->wbuf[1] << 7);
		if (i <0 || i >= WF_MAX_SAMPLE) {
			snd_printk ("invalid slot ID %d\n",
				i);
			wc->status = EINVAL;
			return -EINVAL;
		}
		wc->rbuf[0] = dev->sample_status[i];
		wc->status = 0;
		return 0;

	case WFC_DEBUG_DRIVER:
		dev->debug = wc->wbuf[0];
		snd_printk ("debug = 0x%x\n", dev->debug);
		return 0;

	case WFC_UPLOAD_PATCH:
		munge_int32 (*((u32 *) wc->wbuf), patchnumbuf, 2);
		memcpy (wc->wbuf, patchnumbuf, 2);
		break;

	case WFC_UPLOAD_MULTISAMPLE:
		wc->status = wavefront_fetch_multisample
			(dev, (wavefront_patch_info *) wc->rbuf);
		return 0;

	case WFC_UPLOAD_SAMPLE_ALIAS:
		snd_printk ("support for sample alias upload "
			"being considered.\n");
		wc->status = EINVAL;
		return -EINVAL;
	}

	wc->status = snd_wavefront_cmd (dev, wc->cmd, wc->rbuf, wc->wbuf);


	if (wc->status == 0) {
		switch (wc->cmd) {

		case WFC_REPORT_FREE_MEMORY:
			dev->freemem = demunge_int32 (wc->rbuf, 4);
			break;

		case WFC_UPLOAD_PATCH:
			demunge_buf (wc->rbuf, wc->rbuf, WF_PATCH_BYTES);
			break;

		case WFC_UPLOAD_PROGRAM:
			demunge_buf (wc->rbuf, wc->rbuf, WF_PROGRAM_BYTES);
			break;

		case WFC_UPLOAD_EDRUM_PROGRAM:
			demunge_buf (wc->rbuf, wc->rbuf, WF_DRUM_BYTES - 1);
			break;

		case WFC_UPLOAD_SAMPLE_HEADER:
			process_sample_hdr (wc->rbuf);
			break;

		case WFC_UPLOAD_SAMPLE_ALIAS:
			snd_printk ("support for "
				    "sample aliases still "
				    "being considered.\n");
			break;

		case WFC_VMIDI_OFF:
			snd_wavefront_midi_disable_virtual (acard);
			break;

		case WFC_VMIDI_ON:
			snd_wavefront_midi_enable_virtual (acard);
			break;
		}
	}

	return 0;
}

int 
snd_wavefront_synth_open (struct snd_hwdep *hw, struct file *file)

{
	if (!try_module_get(hw->card->module))
		return -EFAULT;
	file->private_data = hw;
	return 0;
}

int 
snd_wavefront_synth_release (struct snd_hwdep *hw, struct file *file)

{
	module_put(hw->card->module);
	return 0;
}

int
snd_wavefront_synth_ioctl (struct snd_hwdep *hw, struct file *file,
			   unsigned int cmd, unsigned long arg)

{
	struct snd_card *card;
	snd_wavefront_t *dev;
	snd_wavefront_card_t *acard;
	wavefront_control *wc;
	void __user *argp = (void __user *)arg;
	int err;

	card = (struct snd_card *) hw->card;

	if (snd_BUG_ON(!card))
		return -ENODEV;
	if (snd_BUG_ON(!card->private_data))
		return -ENODEV;

	acard = card->private_data;
	dev = &acard->wavefront;
	
	switch (cmd) {
	case WFCTL_LOAD_SPP:
		if (wavefront_load_patch (dev, argp) != 0) {
			return -EIO;
		}
		break;

	case WFCTL_WFCMD:
		wc = memdup_user(argp, sizeof(*wc));
		if (IS_ERR(wc))
			return PTR_ERR(wc);

		if (wavefront_synth_control (acard, wc) < 0)
			err = -EIO;
		else if (copy_to_user (argp, wc, sizeof (*wc)))
			err = -EFAULT;
		else
			err = 0;
		kfree(wc);
		return err;

	default:
		return -EINVAL;
	}

	return 0;
}



void
snd_wavefront_internal_interrupt (snd_wavefront_card_t *card)
{
	snd_wavefront_t *dev = &card->wavefront;


	if ((wavefront_status(dev) & (STAT_INTR_READ|STAT_INTR_WRITE)) == 0) {
		return;
	}

	spin_lock(&dev->irq_lock);
	dev->irq_ok = 1;
	dev->irq_cnt++;
	spin_unlock(&dev->irq_lock);
	wake_up(&dev->interrupt_sleeper);
}


static int __devinit
snd_wavefront_interrupt_bits (int irq)

{
	int bits;

	switch (irq) {
	case 9:
		bits = 0x00;
		break;
	case 5:
		bits = 0x08;
		break;
	case 12:
		bits = 0x10;
		break;
	case 15:
		bits = 0x18;
		break;
	
	default:
		snd_printk ("invalid IRQ %d\n", irq);
		bits = -1;
	}

	return bits;
}

static void __devinit
wavefront_should_cause_interrupt (snd_wavefront_t *dev, 
				  int val, int port, unsigned long timeout)

{
	wait_queue_t wait;

	init_waitqueue_entry(&wait, current);
	spin_lock_irq(&dev->irq_lock);
	add_wait_queue(&dev->interrupt_sleeper, &wait);
	dev->irq_ok = 0;
	outb (val,port);
	spin_unlock_irq(&dev->irq_lock);
	while (!dev->irq_ok && time_before(jiffies, timeout)) {
		schedule_timeout_uninterruptible(1);
		barrier();
	}
}

static int __devinit
wavefront_reset_to_cleanliness (snd_wavefront_t *dev)

{
	int bits;
	int hwv[2];

	

	bits = snd_wavefront_interrupt_bits (dev->irq);

	

	outb (0x0, dev->control_port); 
  


	outb (0x80 | 0x40 | bits, dev->data_port);	
  

	wavefront_should_cause_interrupt(dev, 0x80|0x40|0x10|0x1,
					 dev->control_port,
					 (reset_time*HZ)/100);


	if (!dev->irq_ok) {
		snd_printk ("intr not received after h/w un-reset.\n");
		goto gone_bad;
	} 


	wavefront_should_cause_interrupt(dev, WFC_HARDWARE_VERSION, 
					 dev->data_port, ramcheck_time*HZ);

	if (!dev->irq_ok) {
		snd_printk ("post-RAM-check interrupt not received.\n");
		goto gone_bad;
	} 

	if (!wavefront_wait (dev, STAT_CAN_READ)) {
		snd_printk ("no response to HW version cmd.\n");
		goto gone_bad;
	}
	
	if ((hwv[0] = wavefront_read (dev)) == -1) {
		snd_printk ("board not responding correctly.\n");
		goto gone_bad;
	}

	if (hwv[0] == 0xFF) { 

		
		if ((hwv[0] = wavefront_read (dev)) == -1) {
			snd_printk ("on-board RAM test failed "
				    "(bad error code).\n");
		} else {
			snd_printk ("on-board RAM test failed "
				    "(error code: 0x%x).\n",
				hwv[0]);
		}
		goto gone_bad;
	}

	

	if ((hwv[1] = wavefront_read (dev)) == -1) {
		snd_printk ("incorrect h/w response.\n");
		goto gone_bad;
	}

	snd_printk ("hardware version %d.%d\n",
		    hwv[0], hwv[1]);

	return 0;


     gone_bad:
	return (1);
}

static int __devinit
wavefront_download_firmware (snd_wavefront_t *dev, char *path)

{
	const unsigned char *buf;
	int len, err;
	int section_cnt_downloaded = 0;
	const struct firmware *firmware;

	err = request_firmware(&firmware, path, dev->card->dev);
	if (err < 0) {
		snd_printk(KERN_ERR "firmware (%s) download failed!!!\n", path);
		return 1;
	}

	len = 0;
	buf = firmware->data;
	for (;;) {
		int section_length = *(signed char *)buf;
		if (section_length == 0)
			break;
		if (section_length < 0 || section_length > WF_SECTION_MAX) {
			snd_printk(KERN_ERR
				   "invalid firmware section length %d\n",
				   section_length);
			goto failure;
		}
		buf++;
		len++;

		if (firmware->size < len + section_length) {
			snd_printk(KERN_ERR "firmware section read error.\n");
			goto failure;
		}

		
		if (wavefront_write(dev, WFC_DOWNLOAD_OS))
			goto failure;
	
		for (; section_length; section_length--) {
			if (wavefront_write(dev, *buf))
				goto failure;
			buf++;
			len++;
		}
	
		
		if (!wavefront_wait(dev, STAT_CAN_READ)) {
			snd_printk(KERN_ERR "time out for firmware ACK.\n");
			goto failure;
		}
		err = inb(dev->data_port);
		if (err != WF_ACK) {
			snd_printk(KERN_ERR
				   "download of section #%d not "
				   "acknowledged, ack = 0x%x\n",
				   section_cnt_downloaded + 1, err);
			goto failure;
		}

		section_cnt_downloaded++;
	}

	release_firmware(firmware);
	return 0;

 failure:
	release_firmware(firmware);
	snd_printk(KERN_ERR "firmware download failed!!!\n");
	return 1;
}


static int __devinit
wavefront_do_reset (snd_wavefront_t *dev)

{
	char voices[1];

	if (wavefront_reset_to_cleanliness (dev)) {
		snd_printk ("hw reset failed.\n");
		goto gone_bad;
	}

	if (dev->israw) {
		if (wavefront_download_firmware (dev, ospath)) {
			goto gone_bad;
		}

		dev->israw = 0;


		wavefront_should_cause_interrupt (dev, WFC_NOOP,
						  dev->data_port,
						  (osrun_time*HZ));

		if (!dev->irq_ok) {
			snd_printk ("no post-OS interrupt.\n");
			goto gone_bad;
		}
		
		
		
		wavefront_should_cause_interrupt (dev, WFC_NOOP,
						  dev->data_port, (10*HZ));
		
		if (!dev->irq_ok) {
			snd_printk ("no post-OS interrupt(2).\n");
			goto gone_bad;
		}

		
		outb (0x80|0x40, dev->control_port); 
	}

	
	if ((dev->freemem = wavefront_freemem (dev)) < 0) {
		goto gone_bad;
	}
		
	snd_printk ("available DRAM %dk\n", dev->freemem / 1024);

	if (wavefront_write (dev, 0xf0) ||
	    wavefront_write (dev, 1) ||
	    (wavefront_read (dev) < 0)) {
		dev->debug = 0;
		snd_printk ("MPU emulation mode not set.\n");
		goto gone_bad;
	}

	voices[0] = 32;

	if (snd_wavefront_cmd (dev, WFC_SET_NVOICES, NULL, voices)) {
		snd_printk ("cannot set number of voices to 32.\n");
		goto gone_bad;
	}


	return 0;

 gone_bad:
	

	outb (0x0, dev->control_port);
	dev->interrupts_are_midi = 0;
	return 1;
}

int __devinit
snd_wavefront_start (snd_wavefront_t *dev)

{
	int samples_are_from_rom;


	if (dev->israw) {
		samples_are_from_rom = 1;
	} else {
		
		samples_are_from_rom = 0;
	}

	if (dev->israw || fx_raw) {
		if (wavefront_do_reset (dev)) {
			return -1;
		}
	}
	

	dev->has_fx = (snd_wavefront_fx_detect (dev) == 0);

	if (dev->has_fx && fx_raw) {
		snd_wavefront_fx_start (dev);
	}

	wavefront_get_sample_status (dev, samples_are_from_rom);
	wavefront_get_program_status (dev);
	wavefront_get_patch_status (dev);


	outb (0x80|0x40|0x20, dev->control_port); 

	return (0);
}

int __devinit
snd_wavefront_detect (snd_wavefront_card_t *card)

{
	unsigned char   rbuf[4], wbuf[4];
	snd_wavefront_t *dev = &card->wavefront;
	

	dev->israw = 0;
	dev->has_fx = 0;
	dev->debug = debug_default;
	dev->interrupts_are_midi = 0;
	dev->irq_cnt = 0;
	dev->rom_samples_rdonly = 1;

	if (snd_wavefront_cmd (dev, WFC_FIRMWARE_VERSION, rbuf, wbuf) == 0) {

		dev->fw_version[0] = rbuf[0];
		dev->fw_version[1] = rbuf[1];

		snd_printk ("firmware %d.%d already loaded.\n",
			    rbuf[0], rbuf[1]);

		
      
		if (snd_wavefront_cmd (dev, WFC_HARDWARE_VERSION,
				       rbuf, wbuf) == 0) {
			dev->hw_version[0] = rbuf[0];
			dev->hw_version[1] = rbuf[1];
		} else {
			snd_printk ("not raw, but no "
				    "hardware version!\n");
			return -1;
		}

		if (!wf_raw) {
			return 0;
		} else {
			snd_printk ("reloading firmware as you requested.\n");
			dev->israw = 1;
		}

	} else {

		dev->israw = 1;
		snd_printk ("no response to firmware probe, assume raw.\n");

	}

	return 0;
}

MODULE_FIRMWARE(DEFAULT_OSPATH);
