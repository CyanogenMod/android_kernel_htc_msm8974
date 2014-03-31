/****************************************************************************
 *    ixj.c
 *
 * Device Driver for Quicknet Technologies, Inc.'s Telephony cards
 * including the Internet PhoneJACK, Internet PhoneJACK Lite,
 * Internet PhoneJACK PCI, Internet LineJACK, Internet PhoneCARD and
 * SmartCABLE
 *
 *    (c) Copyright 1999-2001  Quicknet Technologies, Inc.
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version
 *    2 of the License, or (at your option) any later version.
 *
 * Author:          Ed Okerson, <eokerson@quicknet.net>
 *
 * Contributors:    Greg Herlein, <gherlein@quicknet.net>
 *                  David W. Erhart, <derhart@quicknet.net>
 *                  John Sellers, <jsellers@quicknet.net>
 *                  Mike Preston, <mpreston@quicknet.net>
 *    
 * Fixes:           David Huggins-Daines, <dhd@cepstral.com>
 *                  Fabio Ferrari, <fabio.ferrari@digitro.com.br>
 *                  Artis Kugevics, <artis@mt.lv>
 *                  Daniele Bellucci, <bellucda@tiscali.it>
 *
 * More information about the hardware related to this driver can be found  
 * at our website:    http://www.quicknet.net
 *
 * IN NO EVENT SHALL QUICKNET TECHNOLOGIES, INC. BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF QUICKNET
 * TECHNOLOGIES, INC. HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *    
 * QUICKNET TECHNOLOGIES, INC. SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND QUICKNET TECHNOLOGIES, INC. HAS NO OBLIGATION
 * TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 ***************************************************************************/

/*
 * Revision 4.8  2003/07/09 19:39:00  Daniele Bellucci
 * Audit some copy_*_user and minor cleanup.
 *
 * Revision 4.7  2001/08/13 06:19:33  craigs
 * Added additional changes from Alan Cox and John Anderson for
 * 2.2 to 2.4 cleanup and bounds checking
 *
 * Revision 4.6  2001/08/13 01:05:05  craigs
 * Really fixed PHONE_QUERY_CODEC problem this time
 *
 * Revision 4.5  2001/08/13 00:11:03  craigs
 * Fixed problem in handling of PHONE_QUERY_CODEC, thanks to Shane Anderson
 *
 * Revision 4.4  2001/08/07 07:58:12  craigs
 * Changed back to three digit version numbers
 * Added tagbuild target to allow automatic and easy tagging of versions
 *
 * Revision 4.3  2001/08/07 07:24:47  craigs
 * Added ixj-ver.h to allow easy configuration management of driver
 * Added display of version number in /prox/ixj
 *
 * Revision 4.2  2001/08/06 07:07:19  craigs
 * Reverted IXJCTL_DSP_TYPE and IXJCTL_DSP_VERSION files to original
 * behaviour of returning int rather than short *
 *
 * Revision 4.1  2001/08/05 00:17:37  craigs
 * More changes for correct PCMCIA installation
 * Start of changes for backward Linux compatibility
 *
 * Revision 4.0  2001/08/04 12:33:12  craigs
 * New version using GNU autoconf
 *
 * Revision 3.105  2001/07/20 23:14:32  eokerson
 * More work on CallerID generation when using ring cadences.
 *
 * Revision 3.104  2001/07/06 01:33:55  eokerson
 * Some bugfixes from Robert Vojta <vojta@ipex.cz> and a few mods to the Makefile.
 *
 * Revision 3.103  2001/07/05 19:20:16  eokerson
 * Updated HOWTO
 * Changed mic gain to 30dB on Internet LineJACK mic/speaker port.
 *
 * Revision 3.102  2001/07/03 23:51:21  eokerson
 * Un-mute mic on Internet LineJACK when in speakerphone mode.
 *
 * Revision 3.101  2001/07/02 19:26:56  eokerson
 * Removed initialiazation of ixjdebug and ixj_convert_loaded so they will go in the .bss instead of the .data
 *
 * Revision 3.100  2001/07/02 19:18:27  eokerson
 * Changed driver to make dynamic allocation possible.  We now pass IXJ * between functions instead of array indexes.
 * Fixed the way the POTS and PSTN ports interact during a PSTN call to allow local answering.
 * Fixed speaker mode on Internet LineJACK.
 *
 * Revision 3.99  2001/05/09 14:11:16  eokerson
 * Fixed kmalloc error in ixj_build_filter_cadence.  Thanks David Chan <cat@waulogy.stanford.edu>.
 *
 * Revision 3.98  2001/05/08 19:55:33  eokerson
 * Fixed POTS hookstate detection while it is connected to PSTN port.
 *
 * Revision 3.97  2001/05/08 00:01:04  eokerson
 * Fixed kernel oops when sending caller ID data.
 *
 * Revision 3.96  2001/05/04 23:09:30  eokerson
 * Now uses one kernel timer for each card, instead of one for the entire driver.
 *
 * Revision 3.95  2001/04/25 22:06:47  eokerson
 * Fixed squawking at beginning of some G.723.1 calls.
 *
 * Revision 3.94  2001/04/03 23:42:00  eokerson
 * Added linear volume ioctls
 * Added raw filter load ioctl
 *
 * Revision 3.93  2001/02/27 01:00:06  eokerson
 * Fixed blocking in CallerID.
 * Reduced size of ixj structure for smaller driver footprint.
 *
 * Revision 3.92  2001/02/20 22:02:59  eokerson
 * Fixed isapnp and pcmcia module compatibility for 2.4.x kernels.
 * Improved PSTN ring detection.
 * Fixed wink generation on POTS ports.
 *
 * Revision 3.91  2001/02/13 00:55:44  eokerson
 * Turn AEC back on after changing frame sizes.
 *
 * Revision 3.90  2001/02/12 16:42:00  eokerson
 * Added ALAW codec, thanks to Fabio Ferrari for the table based converters to make ALAW from ULAW.
 *
 * Revision 3.89  2001/02/12 15:41:16  eokerson
 * Fix from Artis Kugevics - Tone gains were not being set correctly.
 *
 * Revision 3.88  2001/02/05 23:25:42  eokerson
 * Fixed lockup bugs with deregister.
 *
 * Revision 3.87  2001/01/29 21:00:39  eokerson
 * Fix from Fabio Ferrari <fabio.ferrari@digitro.com.br> to properly handle EAGAIN and EINTR during non-blocking write.
 * Updated copyright date.
 *
 * Revision 3.86  2001/01/23 23:53:46  eokerson
 * Fixes to G.729 compatibility.
 *
 * Revision 3.85  2001/01/23 21:30:36  eokerson
 * Added verbage about cards supported.
 * Removed commands that put the card in low power mode at some times that it should not be in low power mode.
 *
 * Revision 3.84  2001/01/22 23:32:10  eokerson
 * Some bugfixes from David Huggins-Daines, <dhd@cepstral.com> and other cleanups.
 *
 * Revision 3.83  2001/01/19 14:51:41  eokerson
 * Fixed ixj_WriteDSPCommand to decrement usage counter when command fails.
 *
 * Revision 3.82  2001/01/19 00:34:49  eokerson
 * Added verbosity to write overlap errors.
 *
 * Revision 3.81  2001/01/18 23:56:54  eokerson
 * Fixed PSTN line test functions.
 *
 * Revision 3.80  2001/01/18 22:29:27  eokerson
 * Updated AEC/AGC values for different cards.
 *
 * Revision 3.79  2001/01/17 02:58:54  eokerson
 * Fixed AEC reset after Caller ID.
 * Fixed Codec lockup after Caller ID on Call Waiting when not using 30ms frames.
 *
 * Revision 3.78  2001/01/16 19:43:09  eokerson
 * Added support for Linux 2.4.x kernels.
 *
 * Revision 3.77  2001/01/09 04:00:52  eokerson
 * Linetest will now test the line, even if it has previously succeeded.
 *
 * Revision 3.76  2001/01/08 19:27:00  eokerson
 * Fixed problem with standard cable on Internet PhoneCARD.
 *
 * Revision 3.75  2000/12/22 16:52:14  eokerson
 * Modified to allow hookstate detection on the POTS port when the PSTN port is selected.
 *
 * Revision 3.74  2000/12/08 22:41:50  eokerson
 * Added capability for G729B.
 *
 * Revision 3.73  2000/12/07 23:35:16  eokerson
 * Added capability to have different ring pattern before CallerID data.
 * Added hookstate checks in CallerID routines to stop FSK.
 *
 * Revision 3.72  2000/12/06 19:31:31  eokerson
 * Modified signal behavior to only send one signal per event.
 *
 * Revision 3.71  2000/12/06 03:23:08  eokerson
 * Fixed CallerID on Call Waiting.
 *
 * Revision 3.70  2000/12/04 21:29:37  eokerson
 * Added checking to Smart Cable gain functions.
 *
 * Revision 3.69  2000/12/04 21:05:20  eokerson
 * Changed ixjdebug levels.
 * Added ioctls to change gains in Internet Phone CARD Smart Cable.
 *
 * Revision 3.68  2000/12/04 00:17:21  craigs
 * Changed mixer voice gain to +6dB rather than 0dB
 *
 * Revision 3.67  2000/11/30 21:25:51  eokerson
 * Fixed write signal errors.
 *
 * Revision 3.66  2000/11/29 22:42:44  eokerson
 * Fixed PSTN ring detect problems.
 *
 * Revision 3.65  2000/11/29 07:31:55  craigs
 * Added new 425Hz filter co-efficients
 * Added card-specific DTMF prescaler initialisation
 *
 * Revision 3.64  2000/11/28 14:03:32  craigs
 * Changed certain mixer initialisations to be 0dB rather than 12dB
 * Added additional information to /proc/ixj
 *
 * Revision 3.63  2000/11/28 11:38:41  craigs
 * Added display of AEC modes in AUTO and AGC mode
 *
 * Revision 3.62  2000/11/28 04:05:44  eokerson
 * Improved PSTN ring detection routine.
 *
 * Revision 3.61  2000/11/27 21:53:12  eokerson
 * Fixed flash detection.
 *
 * Revision 3.60  2000/11/27 15:57:29  eokerson
 * More work on G.729 load routines.
 *
 * Revision 3.59  2000/11/25 21:55:12  eokerson
 * Fixed errors in G.729 load routine.
 *
 * Revision 3.58  2000/11/25 04:08:29  eokerson
 * Added board locks around G.729 and TS85 load routines.
 *
 * Revision 3.57  2000/11/24 05:35:17  craigs
 * Added ability to retrieve mixer values on LineJACK
 * Added complete initialisation of all mixer values at startup
 * Fixed spelling mistake
 *
 * Revision 3.56  2000/11/23 02:52:11  robertj
 * Added cvs change log keyword.
 * Fixed bug in capabilities list when using G.729 module.
 *
 */

#include "ixj-ver.h"

#define PERFMON_STATS
#define IXJDEBUG 0
#define MAXRINGS 5

#include <linux/module.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>	
#include <linux/fs.h>		
#include <linux/errno.h>	
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/pci.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <linux/isapnp.h>

#include "ixj.h"

#define TYPE(inode) (iminor(inode) >> 4)
#define NUM(inode) (iminor(inode) & 0xf)

static DEFINE_MUTEX(ixj_mutex);
static int ixjdebug;
static int hertz = HZ;
static int samplerate = 100;

module_param(ixjdebug, int, 0);

static DEFINE_PCI_DEVICE_TABLE(ixj_pci_tbl) = {
	{ PCI_VENDOR_ID_QUICKNET, PCI_DEVICE_ID_QUICKNET_XJ,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ }
};
MODULE_DEVICE_TABLE(pci, ixj_pci_tbl);


#ifdef IXJ_DYN_ALLOC

static IXJ *ixj[IXJMAX];
#define	get_ixj(b)	ixj[(b)]

 
static IXJ *ixj_alloc()
{
	for(cnt=0; cnt<IXJMAX; cnt++)
	{
		if(ixj[cnt] == NULL || !ixj[cnt]->DSPbase)
		{
			j = kmalloc(sizeof(IXJ), GFP_KERNEL);
			if (j == NULL)
				return NULL;
			ixj[cnt] = j;
			return j;
		}
	}
	return NULL;
}

static void ixj_fsk_free(IXJ *j)
{
	kfree(j->fskdata);
	j->fskdata = NULL;
}

static void ixj_fsk_alloc(IXJ *j)
{
	if(!j->fskdata) {
		j->fskdata = kmalloc(8000, GFP_KERNEL);
		if (!j->fskdata) {
			if(ixjdebug & 0x0200) {
				printk("IXJ phone%d - allocate failed\n", j->board);
			}
			return;
		} else {
			j->fsksize = 8000;
			if(ixjdebug & 0x0200) {
				printk("IXJ phone%d - allocate succeeded\n", j->board);
			}
		}
	}
}

#else

static IXJ ixj[IXJMAX];
#define	get_ixj(b)	(&ixj[(b)])

 
static IXJ *ixj_alloc(void)
{
	int cnt;
	for(cnt=0; cnt<IXJMAX; cnt++) {
		if(!ixj[cnt].DSPbase)
			return &ixj[cnt];
	}
	return NULL;
}

static inline void ixj_fsk_free(IXJ *j) {;}

static inline void ixj_fsk_alloc(IXJ *j)
{
	j->fsksize = 8000;
}

#endif

#ifdef PERFMON_STATS
#define ixj_perfmon(x)	((x)++)
#else
#define ixj_perfmon(x)	do { } while(0)
#endif

static int ixj_convert_loaded;

static int ixj_WriteDSPCommand(unsigned short, IXJ *j);


static int Stub(IXJ * J, unsigned long arg)
{
	return 0;
}

static IXJ_REGFUNC ixj_PreRead = &Stub;
static IXJ_REGFUNC ixj_PostRead = &Stub;
static IXJ_REGFUNC ixj_PreWrite = &Stub;
static IXJ_REGFUNC ixj_PostWrite = &Stub;

static void ixj_read_frame(IXJ *j);
static void ixj_write_frame(IXJ *j);
static void ixj_init_timer(IXJ *j);
static void ixj_add_timer(IXJ *	j);
static void ixj_timeout(unsigned long ptr);
static int read_filters(IXJ *j);
static int LineMonitor(IXJ *j);
static int ixj_fasync(int fd, struct file *, int mode);
static int ixj_set_port(IXJ *j, int arg);
static int ixj_set_pots(IXJ *j, int arg);
static int ixj_hookstate(IXJ *j);
static int ixj_record_start(IXJ *j);
static void ixj_record_stop(IXJ *j);
static void set_rec_volume(IXJ *j, int volume);
static int get_rec_volume(IXJ *j);
static int set_rec_codec(IXJ *j, int rate);
static void ixj_vad(IXJ *j, int arg);
static int ixj_play_start(IXJ *j);
static void ixj_play_stop(IXJ *j);
static int ixj_set_tone_on(unsigned short arg, IXJ *j);
static int ixj_set_tone_off(unsigned short, IXJ *j);
static int ixj_play_tone(IXJ *j, char tone);
static void ixj_aec_start(IXJ *j, int level);
static int idle(IXJ *j);
static void ixj_ring_on(IXJ *j);
static void ixj_ring_off(IXJ *j);
static void aec_stop(IXJ *j);
static void ixj_ringback(IXJ *j);
static void ixj_busytone(IXJ *j);
static void ixj_dialtone(IXJ *j);
static void ixj_cpt_stop(IXJ *j);
static char daa_int_read(IXJ *j);
static char daa_CR_read(IXJ *j, int cr);
static int daa_set_mode(IXJ *j, int mode);
static int ixj_linetest(IXJ *j);
static int ixj_daa_write(IXJ *j);
static int ixj_daa_cid_read(IXJ *j);
static void DAA_Coeff_US(IXJ *j);
static void DAA_Coeff_UK(IXJ *j);
static void DAA_Coeff_France(IXJ *j);
static void DAA_Coeff_Germany(IXJ *j);
static void DAA_Coeff_Australia(IXJ *j);
static void DAA_Coeff_Japan(IXJ *j);
static int ixj_init_filter(IXJ *j, IXJ_FILTER * jf);
static int ixj_init_filter_raw(IXJ *j, IXJ_FILTER_RAW * jfr);
static int ixj_init_tone(IXJ *j, IXJ_TONE * ti);
static int ixj_build_cadence(IXJ *j, IXJ_CADENCE __user * cp);
static int ixj_build_filter_cadence(IXJ *j, IXJ_FILTER_CADENCE __user * cp);
static int SCI_Control(IXJ *j, int control);
static int SCI_Prepare(IXJ *j);
static int SCI_WaitHighSCI(IXJ *j);
static int SCI_WaitLowSCI(IXJ *j);
static DWORD PCIEE_GetSerialNumber(WORD wAddress);
static int ixj_PCcontrol_wait(IXJ *j);
static void ixj_pre_cid(IXJ *j);
static void ixj_write_cid(IXJ *j);
static void ixj_write_cid_bit(IXJ *j, int bit);
static int set_base_frame(IXJ *j, int size);
static int set_play_codec(IXJ *j, int rate);
static void set_rec_depth(IXJ *j, int depth);
static int ixj_mixer(long val, IXJ *j);


static inline void ixj_read_HSR(IXJ *j)
{
	j->hsr.bytes.low = inb_p(j->DSPbase + 8);
	j->hsr.bytes.high = inb_p(j->DSPbase + 9);
}

static inline int IsControlReady(IXJ *j)
{
	ixj_read_HSR(j);
	return j->hsr.bits.controlrdy ? 1 : 0;
}

static inline int IsPCControlReady(IXJ *j)
{
	j->pccr1.byte = inb_p(j->XILINXbase + 3);
	return j->pccr1.bits.crr ? 1 : 0;
}

static inline int IsStatusReady(IXJ *j)
{
	ixj_read_HSR(j);
	return j->hsr.bits.statusrdy ? 1 : 0;
}

static inline int IsRxReady(IXJ *j)
{
	ixj_read_HSR(j);
	ixj_perfmon(j->rxreadycheck);
	return j->hsr.bits.rxrdy ? 1 : 0;
}

static inline int IsTxReady(IXJ *j)
{
	ixj_read_HSR(j);
	ixj_perfmon(j->txreadycheck);
	return j->hsr.bits.txrdy ? 1 : 0;
}

static inline void set_play_volume(IXJ *j, int volume)
{
	if (ixjdebug & 0x0002)
		printk(KERN_INFO "IXJ: /dev/phone%d Setting Play Volume to 0x%4.4x\n", j->board, volume);
	ixj_WriteDSPCommand(0xCF02, j);
	ixj_WriteDSPCommand(volume, j);
}

static int set_play_volume_linear(IXJ *j, int volume)
{
	int newvolume, dspplaymax;

	if (ixjdebug & 0x0002)
		printk(KERN_INFO "IXJ: /dev/phone %d Setting Linear Play Volume to 0x%4.4x\n", j->board, volume);
	if(volume > 100 || volume < 0) {
		return -1;
	}

	
	switch (j->cardtype) {
	case QTI_PHONEJACK:
		dspplaymax = 0x380;
		break;
	case QTI_LINEJACK:
		if(j->port == PORT_PSTN) {
			dspplaymax = 0x48;
		} else {
			dspplaymax = 0x100;
		}
		break;
	case QTI_PHONEJACK_LITE:
		dspplaymax = 0x380;
		break;
	case QTI_PHONEJACK_PCI:
		dspplaymax = 0x6C;
		break;
	case QTI_PHONECARD:
		dspplaymax = 0x50;
		break;
	default:
		return -1;
	}
	newvolume = (dspplaymax * volume) / 100;
	set_play_volume(j, newvolume);
	return 0;
}

static inline void set_play_depth(IXJ *j, int depth)
{
	if (depth > 60)
		depth = 60;
	if (depth < 0)
		depth = 0;
	ixj_WriteDSPCommand(0x5280 + depth, j);
}

static inline int get_play_volume(IXJ *j)
{
	ixj_WriteDSPCommand(0xCF00, j);
	return j->ssr.high << 8 | j->ssr.low;
}

static int get_play_volume_linear(IXJ *j)
{
	int volume, newvolume, dspplaymax;

	
	switch (j->cardtype) {
	case QTI_PHONEJACK:
		dspplaymax = 0x380;
		break;
	case QTI_LINEJACK:
		if(j->port == PORT_PSTN) {
			dspplaymax = 0x48;
		} else {
			dspplaymax = 0x100;
		}
		break;
	case QTI_PHONEJACK_LITE:
		dspplaymax = 0x380;
		break;
	case QTI_PHONEJACK_PCI:
		dspplaymax = 0x6C;
		break;
	case QTI_PHONECARD:
		dspplaymax = 100;
		break;
	default:
		return -1;
	}
	volume = get_play_volume(j);
	newvolume = (volume * 100) / dspplaymax;
	if(newvolume > 100)
		newvolume = 100;
	return newvolume;
}

static inline BYTE SLIC_GetState(IXJ *j)
{
	if (j->cardtype == QTI_PHONECARD) {
		j->pccr1.byte = 0;
		j->psccr.bits.dev = 3;
		j->psccr.bits.rw = 1;
		outw_p(j->psccr.byte << 8, j->XILINXbase + 0x00);
		ixj_PCcontrol_wait(j);
		j->pslic.byte = inw_p(j->XILINXbase + 0x00) & 0xFF;
		ixj_PCcontrol_wait(j);
		if (j->pslic.bits.powerdown)
			return PLD_SLIC_STATE_OC;
		else if (!j->pslic.bits.ring0 && !j->pslic.bits.ring1)
			return PLD_SLIC_STATE_ACTIVE;
		else
			return PLD_SLIC_STATE_RINGING;
	} else {
		j->pld_slicr.byte = inb_p(j->XILINXbase + 0x01);
	}
	return j->pld_slicr.bits.state;
}

static bool SLIC_SetState(BYTE byState, IXJ *j)
{
	bool fRetVal = false;

	if (j->cardtype == QTI_PHONECARD) {
		if (j->flags.pcmciasct) {
			switch (byState) {
			case PLD_SLIC_STATE_TIPOPEN:
			case PLD_SLIC_STATE_OC:
				j->pslic.bits.powerdown = 1;
				j->pslic.bits.ring0 = j->pslic.bits.ring1 = 0;
				fRetVal = true;
				break;
			case PLD_SLIC_STATE_RINGING:
				if (j->readers || j->writers) {
					j->pslic.bits.powerdown = 0;
					j->pslic.bits.ring0 = 1;
					j->pslic.bits.ring1 = 0;
					fRetVal = true;
				}
				break;
			case PLD_SLIC_STATE_OHT:	

			case PLD_SLIC_STATE_STANDBY:
			case PLD_SLIC_STATE_ACTIVE:
				if (j->readers || j->writers) {
					j->pslic.bits.powerdown = 0;
				} else {
					j->pslic.bits.powerdown = 1;
				}
				j->pslic.bits.ring0 = j->pslic.bits.ring1 = 0;
				fRetVal = true;
				break;
			case PLD_SLIC_STATE_APR:	

			case PLD_SLIC_STATE_OHTPR:	

			default:
				fRetVal = false;
				break;
			}
			j->psccr.bits.dev = 3;
			j->psccr.bits.rw = 0;
			outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase + 0x00);
			ixj_PCcontrol_wait(j);
		}
	} else {
		
		switch (byState) {
		case PLD_SLIC_STATE_OC:
			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_RINGING:
			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 1;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_ACTIVE:
			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_OHT:	

			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 0;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_TIPOPEN:
			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_STANDBY:
			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 0;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 1;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_APR:	

			j->pld_slicw.bits.c1 = 0;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		case PLD_SLIC_STATE_OHTPR:	

			j->pld_slicw.bits.c1 = 1;
			j->pld_slicw.bits.c2 = 1;
			j->pld_slicw.bits.c3 = 1;
			j->pld_slicw.bits.b2en = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			fRetVal = true;
			break;
		default:
			fRetVal = false;
			break;
		}
	}

	return fRetVal;
}

static int ixj_wink(IXJ *j)
{
	BYTE slicnow;

	slicnow = SLIC_GetState(j);

	j->pots_winkstart = jiffies;
	SLIC_SetState(PLD_SLIC_STATE_OC, j);

	msleep(jiffies_to_msecs(j->winktime));

	SLIC_SetState(slicnow, j);
	return 0;
}

static void ixj_init_timer(IXJ *j)
{
	init_timer(&j->timer);
	j->timer.function = ixj_timeout;
	j->timer.data = (unsigned long)j;
}

static void ixj_add_timer(IXJ *j)
{
	j->timer.expires = jiffies + (hertz / samplerate);
	add_timer(&j->timer);
}

static void ixj_tone_timeout(IXJ *j)
{
	IXJ_TONE ti;

	j->tone_state++;
	if (j->tone_state == 3) {
		j->tone_state = 0;
		if (j->cadence_t) {
			j->tone_cadence_state++;
			if (j->tone_cadence_state >= j->cadence_t->elements_used) {
				switch (j->cadence_t->termination) {
				case PLAY_ONCE:
					ixj_cpt_stop(j);
					break;
				case REPEAT_LAST_ELEMENT:
					j->tone_cadence_state--;
					ixj_play_tone(j, j->cadence_t->ce[j->tone_cadence_state].index);
					break;
				case REPEAT_ALL:
					j->tone_cadence_state = 0;
					if (j->cadence_t->ce[j->tone_cadence_state].freq0) {
						ti.tone_index = j->cadence_t->ce[j->tone_cadence_state].index;
						ti.freq0 = j->cadence_t->ce[j->tone_cadence_state].freq0;
						ti.gain0 = j->cadence_t->ce[j->tone_cadence_state].gain0;
						ti.freq1 = j->cadence_t->ce[j->tone_cadence_state].freq1;
						ti.gain1 = j->cadence_t->ce[j->tone_cadence_state].gain1;
						ixj_init_tone(j, &ti);
					}
					ixj_set_tone_on(j->cadence_t->ce[0].tone_on_time, j);
					ixj_set_tone_off(j->cadence_t->ce[0].tone_off_time, j);
					ixj_play_tone(j, j->cadence_t->ce[0].index);
					break;
				}
			} else {
				if (j->cadence_t->ce[j->tone_cadence_state].gain0) {
					ti.tone_index = j->cadence_t->ce[j->tone_cadence_state].index;
					ti.freq0 = j->cadence_t->ce[j->tone_cadence_state].freq0;
					ti.gain0 = j->cadence_t->ce[j->tone_cadence_state].gain0;
					ti.freq1 = j->cadence_t->ce[j->tone_cadence_state].freq1;
					ti.gain1 = j->cadence_t->ce[j->tone_cadence_state].gain1;
					ixj_init_tone(j, &ti);
				}
				ixj_set_tone_on(j->cadence_t->ce[j->tone_cadence_state].tone_on_time, j);
				ixj_set_tone_off(j->cadence_t->ce[j->tone_cadence_state].tone_off_time, j);
				ixj_play_tone(j, j->cadence_t->ce[j->tone_cadence_state].index);
			}
		}
	}
}

static inline void ixj_kill_fasync(IXJ *j, IXJ_SIGEVENT event, int dir)
{
	if(j->ixj_signals[event]) {
		if(ixjdebug & 0x0100)
			printk("Sending signal for event %d\n", event);
			
		
		kill_fasync(&(j->async_queue), j->ixj_signals[event], dir);
	}
}

static void ixj_pstn_state(IXJ *j)
{
	int var;
	union XOPXR0 XR0, daaint;

	var = 10;

	XR0.reg = j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.reg;
	daaint.reg = 0;
	XR0.bitreg.RMR = j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR;

	j->pld_scrr.byte = inb_p(j->XILINXbase);
	if (j->pld_scrr.bits.daaflag) {
		daa_int_read(j);
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.RING) {
			if(time_after(jiffies, j->pstn_sleeptil) && !(j->flags.pots_pstn && j->hookstate)) {
				daaint.bitreg.RING = 1;
				if(ixjdebug & 0x0008) {
					printk(KERN_INFO "IXJ DAA Ring Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
				}
			} else {
				daa_set_mode(j, SOP_PU_RESET);
			}
		}
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.Caller_ID) {
			daaint.bitreg.Caller_ID = 1;
			j->pstn_cid_intr = 1;
			j->pstn_cid_received = jiffies;
			if(ixjdebug & 0x0008) {
				printk(KERN_INFO "IXJ DAA Caller_ID Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
			}
		}
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.Cadence) {
			daaint.bitreg.Cadence = 1;
			if(ixjdebug & 0x0008) {
				printk(KERN_INFO "IXJ DAA Cadence Interrupt /dev/phone%d at %ld\n", j->board, jiffies);
			}
		}
		if(j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK != XR0.bitreg.VDD_OK) {
			daaint.bitreg.VDD_OK = 1;
			daaint.bitreg.SI_0 = j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK;
		}
	}
	daa_CR_read(j, 1);
	if(j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR != XR0.bitreg.RMR && time_after(jiffies, j->pstn_sleeptil) && !(j->flags.pots_pstn && j->hookstate)) {
		daaint.bitreg.RMR = 1;
		daaint.bitreg.SI_1 = j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR;
		if(ixjdebug & 0x0008) {
                        printk(KERN_INFO "IXJ DAA RMR /dev/phone%d was %s for %ld\n", j->board, XR0.bitreg.RMR?"on":"off", jiffies - j->pstn_last_rmr);
		}
		j->pstn_prev_rmr = j->pstn_last_rmr;
		j->pstn_last_rmr = jiffies;
	}
	switch(j->daa_mode) {
		case SOP_PU_SLEEP:
			if (daaint.bitreg.RING) {
				if (!j->flags.pstn_ringing) {
					if (j->daa_mode != SOP_PU_RINGING) {
						j->pstn_ring_int = jiffies;
						daa_set_mode(j, SOP_PU_RINGING);
					}
				}
			}
			break;
		case SOP_PU_RINGING:
			if (daaint.bitreg.RMR) {
				if (ixjdebug & 0x0008) {
					printk(KERN_INFO "IXJ Ring Cadence a state = %d /dev/phone%d at %ld\n", j->cadence_f[4].state, j->board, jiffies);
				}
				if (daaint.bitreg.SI_1) {                
					j->flags.pstn_rmr = 1;
					j->pstn_ring_start = jiffies;
					j->pstn_ring_stop = 0;
					j->ex.bits.pstn_ring = 0;
					if (j->cadence_f[4].state == 0) {
						j->cadence_f[4].state = 1;
						j->cadence_f[4].on1min = jiffies + (long)((j->cadence_f[4].on1 * hertz * (100 - var)) / 10000);
						j->cadence_f[4].on1dot = jiffies + (long)((j->cadence_f[4].on1 * hertz * (100)) / 10000);
						j->cadence_f[4].on1max = jiffies + (long)((j->cadence_f[4].on1 * hertz * (100 + var)) / 10000);
					} else if (j->cadence_f[4].state == 2) {
						if((time_after(jiffies, j->cadence_f[4].off1min) &&
						    time_before(jiffies, j->cadence_f[4].off1max))) {
							if (j->cadence_f[4].on2) {
								j->cadence_f[4].state = 3;
								j->cadence_f[4].on2min = jiffies + (long)((j->cadence_f[4].on2 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].on2dot = jiffies + (long)((j->cadence_f[4].on2 * (hertz * (100)) / 10000));
								j->cadence_f[4].on2max = jiffies + (long)((j->cadence_f[4].on2 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].off1);
							}
							j->cadence_f[4].state = 0;
						}
					} else if (j->cadence_f[4].state == 4) {
						if((time_after(jiffies, j->cadence_f[4].off2min) &&
						    time_before(jiffies, j->cadence_f[4].off2max))) {
							if (j->cadence_f[4].on3) {
								j->cadence_f[4].state = 5;
								j->cadence_f[4].on3min = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].on3dot = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100)) / 10000));
								j->cadence_f[4].on3max = jiffies + (long)((j->cadence_f[4].on3 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].off2);
							}
							j->cadence_f[4].state = 0;
						}
					} else if (j->cadence_f[4].state == 6) {
						if((time_after(jiffies, j->cadence_f[4].off3min) &&
						    time_before(jiffies, j->cadence_f[4].off3max))) {
							j->cadence_f[4].state = 7;
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].off3);
							}
							j->cadence_f[4].state = 0;
						}
					} else {
						j->cadence_f[4].state = 0;
					}
				} else {                                
					j->pstn_ring_start = 0;
					j->pstn_ring_stop = jiffies;
					if (j->cadence_f[4].state == 1) {
						if(!j->cadence_f[4].on1) {
							j->cadence_f[4].state = 7;
						} else if((time_after(jiffies, j->cadence_f[4].on1min) &&
					          time_before(jiffies, j->cadence_f[4].on1max))) {
							if (j->cadence_f[4].off1) {
								j->cadence_f[4].state = 2;
								j->cadence_f[4].off1min = jiffies + (long)((j->cadence_f[4].off1 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].off1dot = jiffies + (long)((j->cadence_f[4].off1 * (hertz * (100)) / 10000));
								j->cadence_f[4].off1max = jiffies + (long)((j->cadence_f[4].off1 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].on1);
							}
							j->cadence_f[4].state = 0;
						}
					} else if (j->cadence_f[4].state == 3) {
						if((time_after(jiffies, j->cadence_f[4].on2min) &&
						    time_before(jiffies, j->cadence_f[4].on2max))) {
							if (j->cadence_f[4].off2) {
								j->cadence_f[4].state = 4;
								j->cadence_f[4].off2min = jiffies + (long)((j->cadence_f[4].off2 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].off2dot = jiffies + (long)((j->cadence_f[4].off2 * (hertz * (100)) / 10000));
								j->cadence_f[4].off2max = jiffies + (long)((j->cadence_f[4].off2 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							if (ixjdebug & 0x0008) {
								printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
										j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
										j->cadence_f[4].on2);
							}
							j->cadence_f[4].state = 0;
						}
					} else if (j->cadence_f[4].state == 5) {
						if((time_after(jiffies, j->cadence_f[4].on3min) &&
						    time_before(jiffies, j->cadence_f[4].on3max))) {
							if (j->cadence_f[4].off3) {
								j->cadence_f[4].state = 6;
								j->cadence_f[4].off3min = jiffies + (long)((j->cadence_f[4].off3 * (hertz * (100 - var)) / 10000));
								j->cadence_f[4].off3dot = jiffies + (long)((j->cadence_f[4].off3 * (hertz * (100)) / 10000));
								j->cadence_f[4].off3max = jiffies + (long)((j->cadence_f[4].off3 * (hertz * (100 + var)) / 10000));
							} else {
								j->cadence_f[4].state = 7;
							}
						} else {
							j->cadence_f[4].state = 0;
						}
					} else {
						if (ixjdebug & 0x0008) {
							printk(KERN_INFO "IXJ Ring Cadence fail state = %d /dev/phone%d at %ld should be %d\n",
									j->cadence_f[4].state, j->board, jiffies - j->pstn_prev_rmr,
									j->cadence_f[4].on3);
						}
						j->cadence_f[4].state = 0;
					}
				}
				if (ixjdebug & 0x0010) {
					printk(KERN_INFO "IXJ Ring Cadence b state = %d /dev/phone%d at %ld\n", j->cadence_f[4].state, j->board, jiffies);
				}
				if (ixjdebug & 0x0010) {
					switch(j->cadence_f[4].state) {
						case 1:
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].on1, j->cadence_f[4].on1min, j->cadence_f[4].on1dot, j->cadence_f[4].on1max);
							break;
						case 2:
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].off1, j->cadence_f[4].off1min, j->cadence_f[4].off1dot, j->cadence_f[4].off1max);
							break;
						case 3:
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].on2, j->cadence_f[4].on2min, j->cadence_f[4].on2dot, j->cadence_f[4].on2max);
							break;
						case 4:
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].off2, j->cadence_f[4].off2min, j->cadence_f[4].off2dot, j->cadence_f[4].off2max);
							break;
						case 5:
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].on3, j->cadence_f[4].on3min, j->cadence_f[4].on3dot, j->cadence_f[4].on3max);
							break;
						case 6:	
							printk(KERN_INFO "IXJ /dev/phone%d Next Ring Cadence state at %u min %ld - %ld - max %ld\n", j->board,
						j->cadence_f[4].off3, j->cadence_f[4].off3min, j->cadence_f[4].off3dot, j->cadence_f[4].off3max);
							break;
					}
				}
			}
			if (j->cadence_f[4].state == 7) {
				j->cadence_f[4].state = 0;
				j->pstn_ring_stop = jiffies;
				j->ex.bits.pstn_ring = 1;
				ixj_kill_fasync(j, SIG_PSTN_RING, POLL_IN);
				if(ixjdebug & 0x0008) {
					printk(KERN_INFO "IXJ Ring int set /dev/phone%d at %ld\n", j->board, jiffies);
				}
			}
			if((j->pstn_ring_int != 0 && time_after(jiffies, j->pstn_ring_int + (hertz * 5)) && !j->flags.pstn_rmr) ||
			   (j->pstn_ring_stop != 0 && time_after(jiffies, j->pstn_ring_stop + (hertz * 5)))) {
				if(ixjdebug & 0x0008) {
					printk("IXJ DAA no ring in 5 seconds /dev/phone%d at %ld\n", j->board, jiffies);
					printk("IXJ DAA pstn ring int /dev/phone%d at %ld\n", j->board, j->pstn_ring_int);
					printk("IXJ DAA pstn ring stop /dev/phone%d at %ld\n", j->board, j->pstn_ring_stop);
				}
				j->pstn_ring_stop = j->pstn_ring_int = 0;
				daa_set_mode(j, SOP_PU_SLEEP);
			} 
			outb_p(j->pld_scrw.byte, j->XILINXbase);
			if (j->pstn_cid_intr && time_after(jiffies, j->pstn_cid_received + hertz)) {
				ixj_daa_cid_read(j);
				j->ex.bits.caller_id = 1;
				ixj_kill_fasync(j, SIG_CALLER_ID, POLL_IN);
				j->pstn_cid_intr = 0;
			}
			if (daaint.bitreg.Cadence) {
				if(ixjdebug & 0x0008) {
					printk("IXJ DAA Cadence interrupt going to sleep /dev/phone%d\n", j->board);
				}
				daa_set_mode(j, SOP_PU_SLEEP);
				j->ex.bits.pstn_ring = 0;
			}
			break;
		case SOP_PU_CONVERSATION:
			if (daaint.bitreg.VDD_OK) {
				if(!daaint.bitreg.SI_0) {
					if (!j->pstn_winkstart) {
						if(ixjdebug & 0x0008) {
							printk("IXJ DAA possible wink /dev/phone%d %ld\n", j->board, jiffies);
						}
						j->pstn_winkstart = jiffies;
					} 
				} else {
					if (j->pstn_winkstart) {
						if(ixjdebug & 0x0008) {
							printk("IXJ DAA possible wink end /dev/phone%d %ld\n", j->board, jiffies);
						}
						j->pstn_winkstart = 0;
					}
				}
			}
			if (j->pstn_winkstart && time_after(jiffies, j->pstn_winkstart + ((hertz * j->winktime) / 1000))) {
				if(ixjdebug & 0x0008) {
					printk("IXJ DAA wink detected going to sleep /dev/phone%d %ld\n", j->board, jiffies);
				}
				daa_set_mode(j, SOP_PU_SLEEP);
				j->pstn_winkstart = 0;
				j->ex.bits.pstn_wink = 1;
				ixj_kill_fasync(j, SIG_PSTN_WINK, POLL_IN);
			}
			break;
	}
}

static void ixj_timeout(unsigned long ptr)
{
	int board;
	unsigned long jifon;
	IXJ *j = (IXJ *)ptr;
	board = j->board;

	if (j->DSPbase && atomic_read(&j->DSPWrite) == 0 && test_and_set_bit(board, (void *)&j->busyflags) == 0) {
		ixj_perfmon(j->timerchecks);
		j->hookstate = ixj_hookstate(j);
		if (j->tone_state) {
			if (!(j->hookstate)) {
				ixj_cpt_stop(j);
				if (j->m_hook) {
					j->m_hook = 0;
					j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j, SIG_HOOKSTATE, POLL_IN);
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				return;
			}
			if (j->tone_state == 1)
				jifon = ((hertz * j->tone_on_time) * 25 / 100000);
			else
				jifon = ((hertz * j->tone_on_time) * 25 / 100000) + ((hertz * j->tone_off_time) * 25 / 100000);
			if (time_before(jiffies, j->tone_start_jif + jifon)) {
				if (j->tone_state == 1) {
					ixj_play_tone(j, j->tone_index);
					if (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						return;
					}
				} else {
					ixj_play_tone(j, 0);
					if (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						return;
					}
				}
			} else {
				ixj_tone_timeout(j);
				if (j->flags.dialtone) {
					ixj_dialtone(j);
				}
				if (j->flags.busytone) {
					ixj_busytone(j);
					if (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						return;
					}
				}
				if (j->flags.ringback) {
					ixj_ringback(j);
					if (j->dsp.low == 0x20) {
						clear_bit(board, &j->busyflags);
						ixj_add_timer(j);
						return;
					}
				}
				if (!j->tone_state) {
					ixj_cpt_stop(j);
				}
			}
		}
		if (!(j->tone_state && j->dsp.low == 0x20)) {
			if (IsRxReady(j)) {
				ixj_read_frame(j);
			}
			if (IsTxReady(j)) {
				ixj_write_frame(j);
			}
		}
		if (j->flags.cringing) {
			if (j->hookstate & 1) {
				j->flags.cringing = 0;
				ixj_ring_off(j);
			} else if(j->cadence_f[5].enable && ((!j->cadence_f[5].en_filter) || (j->cadence_f[5].en_filter && j->flags.firstring))) {
				switch(j->cadence_f[5].state) {
					case 0:
						j->cadence_f[5].on1dot = jiffies + (long)((j->cadence_f[5].on1 * (hertz * 100) / 10000));
						if (time_before(jiffies, j->cadence_f[5].on1dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_on(j);
						}
						j->cadence_f[5].state = 1;
						break;
					case 1:
						if (time_after(jiffies, j->cadence_f[5].on1dot)) {
							j->cadence_f[5].off1dot = jiffies + (long)((j->cadence_f[5].off1 * (hertz * 100) / 10000));
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_off(j);
							j->cadence_f[5].state = 2;
						}
						break;
					case 2:
						if (time_after(jiffies, j->cadence_f[5].off1dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_on(j);
							if (j->cadence_f[5].on2) {
								j->cadence_f[5].on2dot = jiffies + (long)((j->cadence_f[5].on2 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 3;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 3:
						if (time_after(jiffies, j->cadence_f[5].on2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_off(j);
							if (j->cadence_f[5].off2) {
								j->cadence_f[5].off2dot = jiffies + (long)((j->cadence_f[5].off2 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 4;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 4:
						if (time_after(jiffies, j->cadence_f[5].off2dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_on(j);
							if (j->cadence_f[5].on3) {
								j->cadence_f[5].on3dot = jiffies + (long)((j->cadence_f[5].on3 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 5;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 5:
						if (time_after(jiffies, j->cadence_f[5].on3dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							ixj_ring_off(j);
							if (j->cadence_f[5].off3) {
								j->cadence_f[5].off3dot = jiffies + (long)((j->cadence_f[5].off3 * (hertz * 100) / 10000));
								j->cadence_f[5].state = 6;
							} else {
								j->cadence_f[5].state = 7;
							}
						}
						break;
					case 6:
						if (time_after(jiffies, j->cadence_f[5].off3dot)) {
							if(ixjdebug & 0x0004) {
								printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
							}
							j->cadence_f[5].state = 7;
						}
						break;
					case 7:
						if(ixjdebug & 0x0004) {
							printk("Ringing cadence state = %d - %ld\n", j->cadence_f[5].state, jiffies);
						}
						j->flags.cidring = 1;
						j->cadence_f[5].state = 0;
						break;
				}
				if (j->flags.cidring && !j->flags.cidsent) {
					j->flags.cidsent = 1;
					if(j->fskdcnt) {
						SLIC_SetState(PLD_SLIC_STATE_OHT, j);
						ixj_pre_cid(j);
					}
					j->flags.cidring = 0;
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				return;
			} else {
				if (time_after(jiffies, j->ring_cadence_jif + (hertz / 2))) {
					if (j->flags.cidring && !j->flags.cidsent) {
						j->flags.cidsent = 1;
						if(j->fskdcnt) {
							SLIC_SetState(PLD_SLIC_STATE_OHT, j);
							ixj_pre_cid(j);
						}
						j->flags.cidring = 0;
					}
					j->ring_cadence_t--;
					if (j->ring_cadence_t == -1)
						j->ring_cadence_t = 15;
					j->ring_cadence_jif = jiffies;

					if (j->ring_cadence & 1 << j->ring_cadence_t) {
						if(j->flags.cidsent && j->cadence_f[5].en_filter)
							j->flags.firstring = 1;
						else
							ixj_ring_on(j);
					} else {
						ixj_ring_off(j);
						if(!j->flags.cidsent)
							j->flags.cidring = 1;
					}
				}
				clear_bit(board, &j->busyflags);
				ixj_add_timer(j);
				return;
			}
		}
		if (!j->flags.ringing) {
			if (j->hookstate) { 
				if (j->dsp.low != 0x20 &&
				    SLIC_GetState(j) != PLD_SLIC_STATE_ACTIVE) {
					SLIC_SetState(PLD_SLIC_STATE_ACTIVE, j);
				}
				LineMonitor(j);
				read_filters(j);
				ixj_WriteDSPCommand(0x511B, j);
				j->proc_load = j->ssr.high << 8 | j->ssr.low;
				if (!j->m_hook && (j->hookstate & 1)) {
					j->m_hook = j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j, SIG_HOOKSTATE, POLL_IN);
				}
			} else {
				if (j->ex.bits.dtmf_ready) {
					j->dtmf_wp = j->dtmf_rp = j->ex.bits.dtmf_ready = 0;
				}
				if (j->m_hook) {
					j->m_hook = 0;
					j->ex.bits.hookstate = 1;
					ixj_kill_fasync(j, SIG_HOOKSTATE, POLL_IN);
				}
			}
		}
		if (j->cardtype == QTI_LINEJACK && !j->flags.pstncheck && j->flags.pstn_present) {
			ixj_pstn_state(j);
		}
		if (j->ex.bytes) {
			wake_up_interruptible(&j->poll_q);	
		}
		clear_bit(board, &j->busyflags);
	}
	ixj_add_timer(j);
}

static int ixj_status_wait(IXJ *j)
{
	unsigned long jif;

	jif = jiffies + ((60 * hertz) / 100);
	while (!IsStatusReady(j)) {
		ixj_perfmon(j->statuswait);
		if (time_after(jiffies, jif)) {
			ixj_perfmon(j->statuswaitfail);
			return -1;
		}
	}
	return 0;
}

static int ixj_PCcontrol_wait(IXJ *j)
{
	unsigned long jif;

	jif = jiffies + ((60 * hertz) / 100);
	while (!IsPCControlReady(j)) {
		ixj_perfmon(j->pcontrolwait);
		if (time_after(jiffies, jif)) {
			ixj_perfmon(j->pcontrolwaitfail);
			return -1;
		}
	}
	return 0;
}

static int ixj_WriteDSPCommand(unsigned short cmd, IXJ *j)
{
	BYTES bytes;
	unsigned long jif;

	atomic_inc(&j->DSPWrite);
	if(atomic_read(&j->DSPWrite) > 1) {
		printk("IXJ %d DSP write overlap attempting command 0x%4.4x\n", j->board, cmd);
		return -1;
	}
	bytes.high = (cmd & 0xFF00) >> 8;
	bytes.low = cmd & 0x00FF;
	jif = jiffies + ((60 * hertz) / 100);
	while (!IsControlReady(j)) {
		ixj_perfmon(j->iscontrolready);
		if (time_after(jiffies, jif)) {
			ixj_perfmon(j->iscontrolreadyfail);
			atomic_dec(&j->DSPWrite);
			if(atomic_read(&j->DSPWrite) > 0) {
				printk("IXJ %d DSP overlaped command 0x%4.4x during control ready failure.\n", j->board, cmd);
				while(atomic_read(&j->DSPWrite) > 0) {
					atomic_dec(&j->DSPWrite);
				}
			}
			return -1;
		}
	}
	outb(bytes.low, j->DSPbase + 6);
	outb(bytes.high, j->DSPbase + 7);

	if (ixj_status_wait(j)) {
		j->ssr.low = 0xFF;
		j->ssr.high = 0xFF;
		atomic_dec(&j->DSPWrite);
		if(atomic_read(&j->DSPWrite) > 0) {
			printk("IXJ %d DSP overlaped command 0x%4.4x during status wait failure.\n", j->board, cmd);
			while(atomic_read(&j->DSPWrite) > 0) {
				atomic_dec(&j->DSPWrite);
			}
		}
		return -1;
	}
	j->ssr.low = inb_p(j->DSPbase + 2);
	j->ssr.high = inb_p(j->DSPbase + 3);
	atomic_dec(&j->DSPWrite);
	if(atomic_read(&j->DSPWrite) > 0) {
		printk("IXJ %d DSP overlaped command 0x%4.4x\n", j->board, cmd);
		while(atomic_read(&j->DSPWrite) > 0) {
			atomic_dec(&j->DSPWrite);
		}
	}
	return 0;
}

static inline int ixj_gpio_read(IXJ *j)
{
	if (ixj_WriteDSPCommand(0x5143, j))
		return -1;

	j->gpio.bytes.low = j->ssr.low;
	j->gpio.bytes.high = j->ssr.high;

	return 0;
}

static inline void LED_SetState(int state, IXJ *j)
{
	if (j->cardtype == QTI_LINEJACK) {
		j->pld_scrw.bits.led1 = state & 0x1 ? 1 : 0;
		j->pld_scrw.bits.led2 = state & 0x2 ? 1 : 0;
		j->pld_scrw.bits.led3 = state & 0x4 ? 1 : 0;
		j->pld_scrw.bits.led4 = state & 0x8 ? 1 : 0;

		outb(j->pld_scrw.byte, j->XILINXbase);
	}
}

static int ixj_set_port(IXJ *j, int arg)
{
	if (j->cardtype == QTI_PHONEJACK_LITE) {
		if (arg != PORT_POTS)
			return 10;
		else
			return 0;
	}
	switch (arg) {
	case PORT_POTS:
		j->port = PORT_POTS;
		switch (j->cardtype) {
		case QTI_PHONECARD:
			if (j->flags.pcmciasct == 1)
				SLIC_SetState(PLD_SLIC_STATE_ACTIVE, j);
			else
				return 11;
			break;
		case QTI_PHONEJACK_PCI:
			j->pld_slicw.pcib.mic = 0;
			j->pld_slicw.pcib.spk = 0;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			break;
		case QTI_LINEJACK:
			ixj_set_pots(j, 0);			
			if (ixj_WriteDSPCommand(0xC528, j))		
				return 2;
			j->pld_scrw.bits.daafsyncen = 0;	

			outb(j->pld_scrw.byte, j->XILINXbase);
			j->pld_clock.byte = 0;
			outb(j->pld_clock.byte, j->XILINXbase + 0x04);
			j->pld_slicw.bits.rly1 = 1;
			j->pld_slicw.bits.spken = 0;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			ixj_mixer(0x1200, j);	
			ixj_mixer(0x1401, j);	
			ixj_mixer(0x1300, j);       
			ixj_mixer(0x1501, j);       
			ixj_mixer(0x0E80, j);	
			ixj_mixer(0x0F00, j);	
			ixj_mixer(0x0080, j);	
			ixj_mixer(0x0180, j);	
			SLIC_SetState(PLD_SLIC_STATE_STANDBY, j);
			break;
		case QTI_PHONEJACK:
			j->gpio.bytes.high = 0x0B;
			j->gpio.bits.gpio6 = 0;
			j->gpio.bits.gpio7 = 0;
			ixj_WriteDSPCommand(j->gpio.word, j);
			break;
		}
		break;
	case PORT_PSTN:
		if (j->cardtype == QTI_LINEJACK) {
			ixj_WriteDSPCommand(0xC534, j);	

			j->pld_slicw.bits.rly3 = 0;
			j->pld_slicw.bits.rly1 = 1;
			j->pld_slicw.bits.spken = 0;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			j->port = PORT_PSTN;
		} else {
			return 4;
		}
		break;
	case PORT_SPEAKER:
		j->port = PORT_SPEAKER;
		switch (j->cardtype) {
		case QTI_PHONECARD:
			if (j->flags.pcmciasct) {
				SLIC_SetState(PLD_SLIC_STATE_OC, j);
			}
			break;
		case QTI_PHONEJACK_PCI:
			j->pld_slicw.pcib.mic = 1;
			j->pld_slicw.pcib.spk = 1;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			break;
		case QTI_LINEJACK:
			ixj_set_pots(j, 0);			
			if (ixj_WriteDSPCommand(0xC528, j))		
				return 2;
			j->pld_scrw.bits.daafsyncen = 0;	

			outb(j->pld_scrw.byte, j->XILINXbase);
			j->pld_clock.byte = 0;
			outb(j->pld_clock.byte, j->XILINXbase + 0x04);
			j->pld_slicw.bits.rly1 = 1;
			j->pld_slicw.bits.spken = 1;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			ixj_mixer(0x1201, j);	
			ixj_mixer(0x1400, j);	
			ixj_mixer(0x1301, j);       
			ixj_mixer(0x1500, j);       
			ixj_mixer(0x0E06, j);	
			ixj_mixer(0x0F80, j);	
			ixj_mixer(0x0000, j);	
			ixj_mixer(0x0100, j);	
			break;
		case QTI_PHONEJACK:
			j->gpio.bytes.high = 0x0B;
			j->gpio.bits.gpio6 = 0;
			j->gpio.bits.gpio7 = 1;
			ixj_WriteDSPCommand(j->gpio.word, j);
			break;
		}
		break;
	case PORT_HANDSET:
		if (j->cardtype != QTI_PHONEJACK) {
			return 5;
		} else {
			j->gpio.bytes.high = 0x0B;
			j->gpio.bits.gpio6 = 1;
			j->gpio.bits.gpio7 = 0;
			ixj_WriteDSPCommand(j->gpio.word, j);
			j->port = PORT_HANDSET;
		}
		break;
	default:
		return 6;
		break;
	}
	return 0;
}

static int ixj_set_pots(IXJ *j, int arg)
{
	if (j->cardtype == QTI_LINEJACK) {
		if (arg) {
			if (j->port == PORT_PSTN) {
				j->pld_slicw.bits.rly1 = 0;
				outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
				j->flags.pots_pstn = 1;
				return 1;
			} else {
				j->flags.pots_pstn = 0;
				return 0;
			}
		} else {
			j->pld_slicw.bits.rly1 = 1;
			outb(j->pld_slicw.byte, j->XILINXbase + 0x01);
			j->flags.pots_pstn = 0;
			return 1;
		}
	} else {
		return 0;
	}
}

static void ixj_ring_on(IXJ *j)
{
	if (j->dsp.low == 0x20)	
	 {
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring On /dev/phone%d\n", 	j->board);

		j->gpio.bytes.high = 0x0B;
		j->gpio.bytes.low = 0x00;
		j->gpio.bits.gpio1 = 1;
		j->gpio.bits.gpio2 = 1;
		j->gpio.bits.gpio5 = 0;
		ixj_WriteDSPCommand(j->gpio.word, j);	
	} else			
	{
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring On /dev/phone%d\n", j->board);

		SLIC_SetState(PLD_SLIC_STATE_RINGING, j);
	}
}

static int ixj_siadc(IXJ *j, int val)
{
	if(j->cardtype == QTI_PHONECARD){
		if(j->flags.pcmciascp){
			if(val == -1)
				return j->siadc.bits.rxg;

			if(val < 0 || val > 0x1F)
				return -1;

			j->siadc.bits.hom = 0;				
			j->siadc.bits.lom = 0;				
			j->siadc.bits.rxg = val;			
			j->psccr.bits.addr = 6;				
			j->psccr.bits.rw = 0;				
			j->psccr.bits.dev = 0;
			outb(j->siadc.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);
			return j->siadc.bits.rxg;
		}
	}
	return -1;
}

static int ixj_sidac(IXJ *j, int val)
{
	if(j->cardtype == QTI_PHONECARD){
		if(j->flags.pcmciascp){
			if(val == -1)
				return j->sidac.bits.txg;

			if(val < 0 || val > 0x1F)
				return -1;

			j->sidac.bits.srm = 1;				
			j->sidac.bits.slm = 1;				
			j->sidac.bits.txg = val;			
			j->psccr.bits.addr = 7;				
			j->psccr.bits.rw = 0;				
			j->psccr.bits.dev = 0;
			outb(j->sidac.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);
			return j->sidac.bits.txg;
		}
	}
	return -1;
}

static int ixj_pcmcia_cable_check(IXJ *j)
{
	j->pccr1.byte = inb_p(j->XILINXbase + 0x03);
	if (!j->flags.pcmciastate) {
		j->pccr2.byte = inb_p(j->XILINXbase + 0x02);
		if (j->pccr1.bits.drf || j->pccr2.bits.rstc) {
			j->flags.pcmciastate = 4;
			return 0;
		}
		if (j->pccr1.bits.ed) {
			j->pccr1.bits.ed = 0;
			j->psccr.bits.dev = 3;
			j->psccr.bits.rw = 1;
			outw_p(j->psccr.byte << 8, j->XILINXbase + 0x00);
			ixj_PCcontrol_wait(j);
			j->pslic.byte = inw_p(j->XILINXbase + 0x00) & 0xFF;
			j->pslic.bits.led2 = j->pslic.bits.det ? 1 : 0;
			j->psccr.bits.dev = 3;
			j->psccr.bits.rw = 0;
			outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase + 0x00);
			ixj_PCcontrol_wait(j);
			return j->pslic.bits.led2 ? 1 : 0;
		} else if (j->flags.pcmciasct) {
			return j->r_hook;
		} else {
			return 1;
		}
	} else if (j->flags.pcmciastate == 4) {
		if (!j->pccr1.bits.drf) {
			j->flags.pcmciastate = 3;
		}
		return 0;
	} else if (j->flags.pcmciastate == 3) {
		j->pccr2.bits.pwr = 0;
		j->pccr2.bits.rstc = 1;
		outb(j->pccr2.byte, j->XILINXbase + 0x02);
		j->checkwait = jiffies + (hertz * 2);
		j->flags.incheck = 1;
		j->flags.pcmciastate = 2;
		return 0;
	} else if (j->flags.pcmciastate == 2) {
		if (j->flags.incheck) {
			if (time_before(jiffies, j->checkwait)) {
				return 0;
			} else {
				j->flags.incheck = 0;
			}
		}
		j->pccr2.bits.pwr = 0;
		j->pccr2.bits.rstc = 0;
		outb_p(j->pccr2.byte, j->XILINXbase + 0x02);
		j->flags.pcmciastate = 1;
		return 0;
	} else if (j->flags.pcmciastate == 1) {
		j->flags.pcmciastate = 0;
		if (!j->pccr1.bits.drf) {
			j->psccr.bits.dev = 3;
			j->psccr.bits.rw = 1;
			outb_p(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);
			j->flags.pcmciascp = 1;		

			j->flags.pcmciasct = (inw_p(j->XILINXbase + 0x00) >> 8) & 0x03;		

			if (j->flags.pcmciasct == 3) {
				j->flags.pcmciastate = 4;
				return 0;
			} else if (j->flags.pcmciasct == 0) {
				j->pccr2.bits.pwr = 1;
				j->pccr2.bits.rstc = 0;
				outb_p(j->pccr2.byte, j->XILINXbase + 0x02);
				j->port = PORT_SPEAKER;
			} else {
				j->port = PORT_POTS;
			}
			j->sic1.bits.cpd = 0;				
			j->sic1.bits.mpd = 0;				
			j->sic1.bits.hpd = 0;				
			j->sic1.bits.lpd = 0;				
			j->sic1.bits.spd = 1;				
			j->psccr.bits.addr = 1;				
			j->psccr.bits.rw = 0;				
			j->psccr.bits.dev = 0;
			outb(j->sic1.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			j->sic2.bits.al = 0;				
			j->sic2.bits.dl2 = 0;				
			j->sic2.bits.dl1 = 0;				
			j->sic2.bits.pll = 0;				
			j->sic2.bits.hpd = 0;				
			j->psccr.bits.addr = 2;				
			j->psccr.bits.rw = 0;				
			j->psccr.bits.dev = 0;
			outb(j->sic2.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			j->psccr.bits.addr = 3;				
			j->psccr.bits.rw = 0;				
			j->psccr.bits.dev = 0;
			outb(0x00, j->XILINXbase + 0x00);		
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			j->psccr.bits.addr = 4;				
			j->psccr.bits.rw = 0;				
			j->psccr.bits.dev = 0;
			outb(0x09, j->XILINXbase + 0x00);		
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			j->sirxg.bits.lig = 1;				
			j->sirxg.bits.lim = 1;				
			j->sirxg.bits.mcg = 0;				
			j->sirxg.bits.mcm = 0;				
			j->sirxg.bits.him = 0;				
			j->sirxg.bits.iir = 1;				
			j->psccr.bits.addr = 5;				
			j->psccr.bits.rw = 0;				
			j->psccr.bits.dev = 0;
			outb(j->sirxg.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			ixj_siadc(j, 0x17);
			ixj_sidac(j, 0x1D);

			j->siaatt.bits.sot = 0;
			j->psccr.bits.addr = 9;				
			j->psccr.bits.rw = 0;				
			j->psccr.bits.dev = 0;
			outb(j->siaatt.byte, j->XILINXbase + 0x00);
			outb(j->psccr.byte, j->XILINXbase + 0x01);
			ixj_PCcontrol_wait(j);

			if (j->flags.pcmciasct == 1 && !j->readers && !j->writers) {
				j->psccr.byte = j->pslic.byte = 0;
				j->pslic.bits.powerdown = 1;
				j->psccr.bits.dev = 3;
				j->psccr.bits.rw = 0;
				outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase + 0x00);
				ixj_PCcontrol_wait(j);
			}
		}
		return 0;
	} else {
		j->flags.pcmciascp = 0;
		return 0;
	}
	return 0;
}

static int ixj_hookstate(IXJ *j)
{
	int fOffHook = 0;

	switch (j->cardtype) {
	case QTI_PHONEJACK:
		ixj_gpio_read(j);
		fOffHook = j->gpio.bits.gpio3read ? 1 : 0;
		break;
	case QTI_LINEJACK:
	case QTI_PHONEJACK_LITE:
	case QTI_PHONEJACK_PCI:
		SLIC_GetState(j);
		if(j->cardtype == QTI_LINEJACK && j->flags.pots_pstn == 1 && (j->readers || j->writers)) {
			fOffHook = j->pld_slicr.bits.potspstn ? 1 : 0;
			if(fOffHook != j->p_hook) {
				if(!j->checkwait) {
					j->checkwait = jiffies;
				} 
				if(time_before(jiffies, j->checkwait + 2)) {
					fOffHook ^= 1;
				} else {
					j->checkwait = 0;
				}
				j->p_hook = fOffHook;
	 			printk("IXJ : /dev/phone%d pots-pstn hookstate check %d at %ld\n", j->board, fOffHook, jiffies);
			}
		} else {
			if (j->pld_slicr.bits.state == PLD_SLIC_STATE_ACTIVE ||
			    j->pld_slicr.bits.state == PLD_SLIC_STATE_STANDBY) {
				if (j->flags.ringing || j->flags.cringing) {
					if (!in_interrupt()) {
						msleep(20);
					}
					SLIC_GetState(j);
					if (j->pld_slicr.bits.state == PLD_SLIC_STATE_RINGING) {
						ixj_ring_on(j);
					}
				}
				if (j->cardtype == QTI_PHONEJACK_PCI) {
					j->pld_scrr.byte = inb_p(j->XILINXbase);
					fOffHook = j->pld_scrr.pcib.det ? 1 : 0;
				} else
					fOffHook = j->pld_slicr.bits.det ? 1 : 0;
			}
		}
		break;
	case QTI_PHONECARD:
		fOffHook = ixj_pcmcia_cable_check(j);
		break;
	}
	if (j->r_hook != fOffHook) {
		j->r_hook = fOffHook;
		if (j->port == PORT_SPEAKER || j->port == PORT_HANDSET) { 
			j->ex.bits.hookstate = 1;
			ixj_kill_fasync(j, SIG_HOOKSTATE, POLL_IN);
		} else if (!fOffHook) {
			j->flash_end = jiffies + ((60 * hertz) / 100);
		}
	}
	if (fOffHook) {
		if(time_before(jiffies, j->flash_end)) {
			j->ex.bits.flash = 1;
			j->flash_end = 0;
			ixj_kill_fasync(j, SIG_FLASH, POLL_IN);
		}
	} else {
		if(time_before(jiffies, j->flash_end)) {
			fOffHook = 1;
		}
	}

	if (j->port == PORT_PSTN && j->daa_mode == SOP_PU_CONVERSATION)
		fOffHook |= 2;

	if (j->port == PORT_SPEAKER) {
		if(j->cardtype == QTI_PHONECARD) {
			if(j->flags.pcmciascp && j->flags.pcmciasct) {
				fOffHook |= 2;
			}
		} else {
			fOffHook |= 2;
		}
	}

	if (j->port == PORT_HANDSET)
		fOffHook |= 2;

	return fOffHook;
}

static void ixj_ring_off(IXJ *j)
{
	if (j->dsp.low == 0x20)	
	 {
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring Off\n");
		j->gpio.bytes.high = 0x0B;
		j->gpio.bytes.low = 0x00;
		j->gpio.bits.gpio1 = 0;
		j->gpio.bits.gpio2 = 1;
		j->gpio.bits.gpio5 = 0;
		ixj_WriteDSPCommand(j->gpio.word, j);
	} else			
	{
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Ring Off\n");

		if(!j->flags.cidplay)
			SLIC_SetState(PLD_SLIC_STATE_STANDBY, j);

		SLIC_GetState(j);
	}
}

static void ixj_ring_start(IXJ *j)
{
	j->flags.cringing = 1;
	if (ixjdebug & 0x0004)
		printk(KERN_INFO "IXJ Cadence Ringing Start /dev/phone%d\n", j->board);
	if (ixj_hookstate(j) & 1) {
		if (j->port == PORT_POTS)
			ixj_ring_off(j);
		j->flags.cringing = 0;
		if (ixjdebug & 0x0004)
			printk(KERN_INFO "IXJ Cadence Ringing Stopped /dev/phone%d off hook\n", j->board);
	} else if(j->cadence_f[5].enable && (!j->cadence_f[5].en_filter)) {
		j->ring_cadence_jif = jiffies;
		j->flags.cidsent = j->flags.cidring = 0;
		j->cadence_f[5].state = 0;
		if(j->cadence_f[5].on1)
			ixj_ring_on(j);
	} else {
		j->ring_cadence_jif = jiffies;
		j->ring_cadence_t = 15;
		if (j->ring_cadence & 1 << j->ring_cadence_t) {
			ixj_ring_on(j);
		} else {
			ixj_ring_off(j);
		}
		j->flags.cidsent = j->flags.cidring = j->flags.firstring = 0;
	}
}

static int ixj_ring(IXJ *j)
{
	char cntr;
	unsigned long jif;

	j->flags.ringing = 1;
	if (ixj_hookstate(j) & 1) {
		ixj_ring_off(j);
		j->flags.ringing = 0;
		return 1;
	}
	for (cntr = 0; cntr < j->maxrings; cntr++) {
		jif = jiffies + (1 * hertz);
		ixj_ring_on(j);
		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				ixj_ring_off(j);
				j->flags.ringing = 0;
				return 1;
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
		}
		jif = jiffies + (3 * hertz);
		ixj_ring_off(j);
		while (time_before(jiffies, jif)) {
			if (ixj_hookstate(j) & 1) {
				msleep(10);
				if (ixj_hookstate(j) & 1) {
					j->flags.ringing = 0;
					return 1;
				}
			}
			schedule_timeout_interruptible(1);
			if (signal_pending(current))
				break;
		}
	}
	ixj_ring_off(j);
	j->flags.ringing = 0;
	return 0;
}

static int ixj_open(struct phone_device *p, struct file *file_p)
{
	IXJ *j = get_ixj(p->board);
	file_p->private_data = j;

	if (!j->DSPbase)
		return -ENODEV;

        if (file_p->f_mode & FMODE_READ) {
		if(!j->readers) {
	                j->readers++;
        	} else {
                	return -EBUSY;
		}
        }

	if (file_p->f_mode & FMODE_WRITE) {
		if(!j->writers) {
			j->writers++;
		} else {
			if (file_p->f_mode & FMODE_READ){
				j->readers--;
			}
			return -EBUSY;
		}
	}

	if (j->cardtype == QTI_PHONECARD) {
		j->pslic.bits.powerdown = 0;
		j->psccr.bits.dev = 3;
		j->psccr.bits.rw = 0;
		outw_p(j->psccr.byte << 8 | j->pslic.byte, j->XILINXbase + 0x00);
		ixj_PCcontrol_wait(j);
	}

	j->flags.cidplay = 0;
	j->flags.cidcw_ack = 0;

	if (ixjdebug & 0x0002)
		printk(KERN_INFO "Opening board %d\n", p->board);

	j->framesread = j->frameswritten = 0;
	return 0;
}

static int ixj_release(struct inode *inode, struct file *file_p)
{
	IXJ_TONE ti;
	int cnt;
	IXJ *j = file_p->private_data;
	int board = j->p.board;

	while(test_and_set_bit(board, (void *)&j->busyflags) != 0)
		schedule_timeout_interruptible(1);
	if (ixjdebug & 0x0002)
		printk(KERN_INFO "Closing board %d\n", NUM(inode));

	if (j->cardtype == QTI_PHONECARD)
		ixj_set_port(j, PORT_SPEAKER);
	else
		ixj_set_port(j, PORT_POTS);

	aec_stop(j);
	ixj_play_stop(j);
	ixj_record_stop(j);
	set_play_volume(j, 0x100);
	set_rec_volume(j, 0x100);
	ixj_ring_off(j);

	
	ti.tone_index = 10;
	ti.gain0 = 1;
	ti.freq0 = hz941;
	ti.gain1 = 0;
	ti.freq1 = hz1209;
	ixj_init_tone(j, &ti);
	ti.tone_index = 11;
	ti.gain0 = 1;
	ti.freq0 = hz941;
	ti.gain1 = 0;
	ti.freq1 = hz1336;
	ixj_init_tone(j, &ti);
	ti.tone_index = 12;
	ti.gain0 = 1;
	ti.freq0 = hz941;
	ti.gain1 = 0;
	ti.freq1 = hz1477;
	ixj_init_tone(j, &ti);
	ti.tone_index = 13;
	ti.gain0 = 1;
	ti.freq0 = hz800;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 14;
	ti.gain0 = 1;
	ti.freq0 = hz1000;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 15;
	ti.gain0 = 1;
	ti.freq0 = hz1250;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 16;
	ti.gain0 = 1;
	ti.freq0 = hz950;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 17;
	ti.gain0 = 1;
	ti.freq0 = hz1100;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 18;
	ti.gain0 = 1;
	ti.freq0 = hz1400;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 19;
	ti.gain0 = 1;
	ti.freq0 = hz1500;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 20;
	ti.gain0 = 1;
	ti.freq0 = hz1600;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 21;
	ti.gain0 = 1;
	ti.freq0 = hz1800;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 22;
	ti.gain0 = 1;
	ti.freq0 = hz2100;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 23;
	ti.gain0 = 1;
	ti.freq0 = hz1300;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 24;
	ti.gain0 = 1;
	ti.freq0 = hz2450;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);
	ti.tone_index = 25;
	ti.gain0 = 1;
	ti.freq0 = hz350;
	ti.gain1 = 0;
	ti.freq1 = hz440;
	ixj_init_tone(j, &ti);
	ti.tone_index = 26;
	ti.gain0 = 1;
	ti.freq0 = hz440;
	ti.gain1 = 0;
	ti.freq1 = hz480;
	ixj_init_tone(j, &ti);
	ti.tone_index = 27;
	ti.gain0 = 1;
	ti.freq0 = hz480;
	ti.gain1 = 0;
	ti.freq1 = hz620;
	ixj_init_tone(j, &ti);

	set_rec_depth(j, 2);	

	set_play_depth(j, 2);	

	j->ex.bits.dtmf_ready = 0;
	j->dtmf_state = 0;
	j->dtmf_wp = j->dtmf_rp = 0;
	j->rec_mode = j->play_mode = -1;
	j->flags.ringing = 0;
	j->maxrings = MAXRINGS;
	j->ring_cadence = USA_RING_CADENCE;
	if(j->cadence_f[5].enable) {
		j->cadence_f[5].enable = j->cadence_f[5].en_filter = j->cadence_f[5].state = 0;
	}
	j->drybuffer = 0;
	j->winktime = 320;
	j->flags.dtmf_oob = 0;
	for (cnt = 0; cnt < 4; cnt++)
		j->cadence_f[cnt].enable = 0;

	idle(j);

	if(j->cardtype == QTI_PHONECARD) {
		SLIC_SetState(PLD_SLIC_STATE_OC, j);
	}

	if (file_p->f_mode & FMODE_READ)
		j->readers--;
	if (file_p->f_mode & FMODE_WRITE)
		j->writers--;

	if (j->read_buffer && !j->readers) {
		kfree(j->read_buffer);
		j->read_buffer = NULL;
		j->read_buffer_size = 0;
	}
	if (j->write_buffer && !j->writers) {
		kfree(j->write_buffer);
		j->write_buffer = NULL;
		j->write_buffer_size = 0;
	}
	j->rec_codec = j->play_codec = 0;
	j->rec_frame_size = j->play_frame_size = 0;
	j->flags.cidsent = j->flags.cidring = 0;

	if(j->cardtype == QTI_LINEJACK && !j->readers && !j->writers) {
		ixj_set_port(j, PORT_PSTN);
		daa_set_mode(j, SOP_PU_SLEEP);
		ixj_set_pots(j, 1);
	}
	ixj_WriteDSPCommand(0x0FE3, j);	

	
	for (cnt = 0; cnt < 35; cnt++)
		j->ixj_signals[cnt] = SIGIO;

	
	j->ex_sig.bits.dtmf_ready = j->ex_sig.bits.hookstate = j->ex_sig.bits.flash = j->ex_sig.bits.pstn_ring = 
	j->ex_sig.bits.caller_id = j->ex_sig.bits.pstn_wink = j->ex_sig.bits.f0 = j->ex_sig.bits.f1 = j->ex_sig.bits.f2 = 
	j->ex_sig.bits.f3 = j->ex_sig.bits.fc0 = j->ex_sig.bits.fc1 = j->ex_sig.bits.fc2 = j->ex_sig.bits.fc3 = 1;

	file_p->private_data = NULL;
	clear_bit(board, &j->busyflags);
	return 0;
}

static int read_filters(IXJ *j)
{
	unsigned short fc, cnt, trg;
	int var;

	trg = 0;
	if (ixj_WriteDSPCommand(0x5144, j)) {
		if(ixjdebug & 0x0001) {
			printk(KERN_INFO "Read Frame Counter failed!\n");
		}
		return -1;
	}
	fc = j->ssr.high << 8 | j->ssr.low;
	if (fc == j->frame_count)
		return 1;

	j->frame_count = fc;

	if (j->dtmf_proc)
		return 1;

	var = 10;

	for (cnt = 0; cnt < 4; cnt++) {
		if (ixj_WriteDSPCommand(0x5154 + cnt, j)) {
			if(ixjdebug & 0x0001) {
				printk(KERN_INFO "Select Filter %d failed!\n", cnt);
			}
			return -1;
		}
		if (ixj_WriteDSPCommand(0x515C, j)) {
			if(ixjdebug & 0x0001) {
				printk(KERN_INFO "Read Filter History %d failed!\n", cnt);
			}
			return -1;
		}
		j->filter_hist[cnt] = j->ssr.high << 8 | j->ssr.low;

		if (j->cadence_f[cnt].enable) {
			if (j->filter_hist[cnt] & 3 && !(j->filter_hist[cnt] & 12)) {
				if (j->cadence_f[cnt].state == 0) {
					j->cadence_f[cnt].state = 1;
					j->cadence_f[cnt].on1min = jiffies + (long)((j->cadence_f[cnt].on1 * (hertz * (100 - var)) / 10000));
					j->cadence_f[cnt].on1dot = jiffies + (long)((j->cadence_f[cnt].on1 * (hertz * (100)) / 10000));
					j->cadence_f[cnt].on1max = jiffies + (long)((j->cadence_f[cnt].on1 * (hertz * (100 + var)) / 10000));
				} else if (j->cadence_f[cnt].state == 2 &&
					   (time_after(jiffies, j->cadence_f[cnt].off1min) &&
					    time_before(jiffies, j->cadence_f[cnt].off1max))) {
					if (j->cadence_f[cnt].on2) {
						j->cadence_f[cnt].state = 3;
						j->cadence_f[cnt].on2min = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on2dot = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100)) / 10000));
						j->cadence_f[cnt].on2max = jiffies + (long)((j->cadence_f[cnt].on2 * (hertz * (100 + var)) / 10000));
					} else {
						j->cadence_f[cnt].state = 7;
					}
				} else if (j->cadence_f[cnt].state == 4 &&
					   (time_after(jiffies, j->cadence_f[cnt].off2min) &&
					    time_before(jiffies, j->cadence_f[cnt].off2max))) {
					if (j->cadence_f[cnt].on3) {
						j->cadence_f[cnt].state = 5;
						j->cadence_f[cnt].on3min = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100 - var)) / 10000));
						j->cadence_f[cnt].on3dot = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100)) / 10000));
						j->cadence_f[cnt].on3max = jiffies + (long)((j->cadence_f[cnt].on3 * (hertz * (100 + var)) / 10000));
					} else {
						j->cadence_f[cnt].state = 7;
					}
				} else {
					j->cadence_f[cnt].state = 0;
				}
			} else if (j->filter_hist[cnt] & 12 && !(j->filter_hist[cnt] & 3)) {
				if (j->cadence_f[cnt].state == 1) {
					if(!j->cadence_f[cnt].on1) {
						j->cadence_f[cnt].state = 7;
					} else if((time_after(jiffies, j->cadence_f[cnt].on1min) &&
					  time_before(jiffies, j->cadence_f[cnt].on1max))) {
						if(j->cadence_f[cnt].off1) {
							j->cadence_f[cnt].state = 2;
							j->cadence_f[cnt].off1min = jiffies + (long)((j->cadence_f[cnt].off1 * (hertz * (100 - var)) / 10000));
							j->cadence_f[cnt].off1dot = jiffies + (long)((j->cadence_f[cnt].off1 * (hertz * (100)) / 10000));
							j->cadence_f[cnt].off1max = jiffies + (long)((j->cadence_f[cnt].off1 * (hertz * (100 + var)) / 10000));
						} else {
							j->cadence_f[cnt].state = 7;
						}
					} else {
						j->cadence_f[cnt].state = 0;
					}
				} else if (j->cadence_f[cnt].state == 3) {
					if((time_after(jiffies, j->cadence_f[cnt].on2min) &&
					    time_before(jiffies, j->cadence_f[cnt].on2max))) {
						if(j->cadence_f[cnt].off2) {
							j->cadence_f[cnt].state = 4;
							j->cadence_f[cnt].off2min = jiffies + (long)((j->cadence_f[cnt].off2 * (hertz * (100 - var)) / 10000));
							j->cadence_f[cnt].off2dot = jiffies + (long)((j->cadence_f[cnt].off2 * (hertz * (100)) / 10000));
							j->cadence_f[cnt].off2max = jiffies + (long)((j->cadence_f[cnt].off2 * (hertz * (100 + var)) / 10000));
						} else {
							j->cadence_f[cnt].state = 7;
						}
					} else {
						j->cadence_f[cnt].state = 0;
					}
				} else if (j->cadence_f[cnt].state == 5) {
					if ((time_after(jiffies, j->cadence_f[cnt].on3min) &&
					    time_before(jiffies, j->cadence_f[cnt].on3max))) {
						if(j->cadence_f[cnt].off3) {
							j->cadence_f[cnt].state = 6;
							j->cadence_f[cnt].off3min = jiffies + (long)((j->cadence_f[cnt].off3 * (hertz * (100 - var)) / 10000));
							j->cadence_f[cnt].off3dot = jiffies + (long)((j->cadence_f[cnt].off3 * (hertz * (100)) / 10000));
							j->cadence_f[cnt].off3max = jiffies + (long)((j->cadence_f[cnt].off3 * (hertz * (100 + var)) / 10000));
						} else {
							j->cadence_f[cnt].state = 7;
						}
					} else {
						j->cadence_f[cnt].state = 0;
					}
				} else {
					j->cadence_f[cnt].state = 0;
				}
			} else {
				switch(j->cadence_f[cnt].state) {
					case 1:
						if(time_after(jiffies, j->cadence_f[cnt].on1dot) &&
						   !j->cadence_f[cnt].off1 &&
						   !j->cadence_f[cnt].on2 && !j->cadence_f[cnt].off2 &&
						   !j->cadence_f[cnt].on3 && !j->cadence_f[cnt].off3) {
							j->cadence_f[cnt].state = 7;
						}
						break;
					case 3:
						if(time_after(jiffies, j->cadence_f[cnt].on2dot) &&
						   !j->cadence_f[cnt].off2 &&
						   !j->cadence_f[cnt].on3 && !j->cadence_f[cnt].off3) {
							j->cadence_f[cnt].state = 7;
						}
						break;
					case 5:
						if(time_after(jiffies, j->cadence_f[cnt].on3dot) &&
						   !j->cadence_f[cnt].off3) {
							j->cadence_f[cnt].state = 7;
						}
						break;
				}
			}

			if (ixjdebug & 0x0040) {
				printk(KERN_INFO "IXJ Tone Cadence state = %d /dev/phone%d at %ld\n", j->cadence_f[cnt].state, j->board, jiffies);
				switch(j->cadence_f[cnt].state) {
					case 0:
						printk(KERN_INFO "IXJ /dev/phone%d No Tone detected\n", j->board);
						break;
					case 1:
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %u %ld - %ld - %ld\n", j->board,
					j->cadence_f[cnt].on1, j->cadence_f[cnt].on1min, j->cadence_f[cnt].on1dot, j->cadence_f[cnt].on1max);
						break;
					case 2:
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %ld - %ld\n", j->board, j->cadence_f[cnt].off1min, 
															j->cadence_f[cnt].off1max);
						break;
					case 3:
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %ld - %ld\n", j->board, j->cadence_f[cnt].on2min,
															j->cadence_f[cnt].on2max);
						break;
					case 4:
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %ld - %ld\n", j->board, j->cadence_f[cnt].off2min,
															j->cadence_f[cnt].off2max);
						break;
					case 5:
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %ld - %ld\n", j->board, j->cadence_f[cnt].on3min,
															j->cadence_f[cnt].on3max);
						break;
					case 6:	
						printk(KERN_INFO "IXJ /dev/phone%d Next Tone Cadence state at %ld - %ld\n", j->board, j->cadence_f[cnt].off3min,
															j->cadence_f[cnt].off3max);
						break;
				}
			} 
		}
		if (j->cadence_f[cnt].state == 7) {
			j->cadence_f[cnt].state = 0;
			if (j->cadence_f[cnt].enable == 1)
				j->cadence_f[cnt].enable = 0;
			switch (cnt) {
			case 0:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 0 triggered %ld\n", jiffies);
				}
				j->ex.bits.fc0 = 1;
				ixj_kill_fasync(j, SIG_FC0, POLL_IN);
				break;
			case 1:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 1 triggered %ld\n", jiffies);
				}
				j->ex.bits.fc1 = 1;
				ixj_kill_fasync(j, SIG_FC1, POLL_IN);
				break;
			case 2:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 2 triggered %ld\n", jiffies);
				}
				j->ex.bits.fc2 = 1;
				ixj_kill_fasync(j, SIG_FC2, POLL_IN);
				break;
			case 3:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter Cadence 3 triggered %ld\n", jiffies);
				}
				j->ex.bits.fc3 = 1;
				ixj_kill_fasync(j, SIG_FC3, POLL_IN);
				break;
			}
		}
		if (j->filter_en[cnt] && ((j->filter_hist[cnt] & 3 && !(j->filter_hist[cnt] & 12)) ||
					  (j->filter_hist[cnt] & 12 && !(j->filter_hist[cnt] & 3)))) {
			if((j->filter_hist[cnt] & 3 && !(j->filter_hist[cnt] & 12))) {
				trg = 1;
			} else if((j->filter_hist[cnt] & 12 && !(j->filter_hist[cnt] & 3))) {
				trg = 0;
			}
			switch (cnt) {
			case 0:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter 0 triggered %d at %ld\n", trg, jiffies);
				}
				j->ex.bits.f0 = 1;
				ixj_kill_fasync(j, SIG_F0, POLL_IN);
				break;
			case 1:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter 1 triggered %d at %ld\n", trg, jiffies);
				}
				j->ex.bits.f1 = 1;
				ixj_kill_fasync(j, SIG_F1, POLL_IN);
				break;
			case 2:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter 2 triggered %d at %ld\n", trg, jiffies);
				}
				j->ex.bits.f2 = 1;
				ixj_kill_fasync(j, SIG_F2, POLL_IN);
				break;
			case 3:
				if(ixjdebug & 0x0020) {
					printk(KERN_INFO "Filter 3 triggered %d at %ld\n", trg, jiffies);
				}
				j->ex.bits.f3 = 1;
				ixj_kill_fasync(j, SIG_F3, POLL_IN);
				break;
			}
		}
	}
	return 0;
}

static int LineMonitor(IXJ *j)
{
	if (j->dtmf_proc) {
		return -1;
	}
	j->dtmf_proc = 1;

	if (ixj_WriteDSPCommand(0x7000, j))		
		return -1;

	j->dtmf.bytes.high = j->ssr.high;
	j->dtmf.bytes.low = j->ssr.low;
	if (!j->dtmf_state && j->dtmf.bits.dtmf_valid) {
		j->dtmf_state = 1;
		j->dtmf_current = j->dtmf.bits.digit;
	}
	if (j->dtmf_state && !j->dtmf.bits.dtmf_valid)	
	 {
		if(!j->cidcw_wait) {
			j->dtmfbuffer[j->dtmf_wp] = j->dtmf_current;
			j->dtmf_wp++;
			if (j->dtmf_wp == 79)
				j->dtmf_wp = 0;
			j->ex.bits.dtmf_ready = 1;
			if(j->ex_sig.bits.dtmf_ready) {
				ixj_kill_fasync(j, SIG_DTMF_READY, POLL_IN);
			}
		}
		else if(j->dtmf_current == 0x00 || j->dtmf_current == 0x0D) {
			if(ixjdebug & 0x0020) {
				printk("IXJ phone%d saw CIDCW Ack DTMF %d from display at %ld\n", j->board, j->dtmf_current, jiffies);
			}
			j->flags.cidcw_ack = 1;
		}
		j->dtmf_state = 0;
	}
	j->dtmf_proc = 0;

	return 0;
}


static void ulaw2alaw(unsigned char *buff, unsigned long len)
{
	static unsigned char table_ulaw2alaw[] =
	{
		0x2A, 0x2B, 0x28, 0x29, 0x2E, 0x2F, 0x2C, 0x2D, 
		0x22, 0x23, 0x20, 0x21, 0x26, 0x27, 0x24, 0x25, 
		0x3A, 0x3B, 0x38, 0x39, 0x3E, 0x3F, 0x3C, 0x3D, 
		0x32, 0x33, 0x30, 0x31, 0x36, 0x37, 0x34, 0x35, 
		0x0B, 0x08, 0x09, 0x0E, 0x0F, 0x0C, 0x0D, 0x02, 
		0x03, 0x00, 0x01, 0x06, 0x07, 0x04, 0x05, 0x1A, 
		0x1B, 0x18, 0x19, 0x1E, 0x1F, 0x1C, 0x1D, 0x12, 
		0x13, 0x10, 0x11, 0x16, 0x17, 0x14, 0x15, 0x6B, 
		0x68, 0x69, 0x6E, 0x6F, 0x6C, 0x6D, 0x62, 0x63, 
		0x60, 0x61, 0x66, 0x67, 0x64, 0x65, 0x7B, 0x79, 
		0x7E, 0x7F, 0x7C, 0x7D, 0x72, 0x73, 0x70, 0x71, 
		0x76, 0x77, 0x74, 0x75, 0x4B, 0x49, 0x4F, 0x4D, 
		0x42, 0x43, 0x40, 0x41, 0x46, 0x47, 0x44, 0x45, 
		0x5A, 0x5B, 0x58, 0x59, 0x5E, 0x5F, 0x5C, 0x5D, 
		0x52, 0x52, 0x53, 0x53, 0x50, 0x50, 0x51, 0x51, 
		0x56, 0x56, 0x57, 0x57, 0x54, 0x54, 0x55, 0xD5, 
		0xAA, 0xAB, 0xA8, 0xA9, 0xAE, 0xAF, 0xAC, 0xAD, 
		0xA2, 0xA3, 0xA0, 0xA1, 0xA6, 0xA7, 0xA4, 0xA5, 
		0xBA, 0xBB, 0xB8, 0xB9, 0xBE, 0xBF, 0xBC, 0xBD, 
		0xB2, 0xB3, 0xB0, 0xB1, 0xB6, 0xB7, 0xB4, 0xB5, 
		0x8B, 0x88, 0x89, 0x8E, 0x8F, 0x8C, 0x8D, 0x82, 
		0x83, 0x80, 0x81, 0x86, 0x87, 0x84, 0x85, 0x9A, 
		0x9B, 0x98, 0x99, 0x9E, 0x9F, 0x9C, 0x9D, 0x92, 
		0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95, 0xEB, 
		0xE8, 0xE9, 0xEE, 0xEF, 0xEC, 0xED, 0xE2, 0xE3, 
		0xE0, 0xE1, 0xE6, 0xE7, 0xE4, 0xE5, 0xFB, 0xF9, 
		0xFE, 0xFF, 0xFC, 0xFD, 0xF2, 0xF3, 0xF0, 0xF1, 
		0xF6, 0xF7, 0xF4, 0xF5, 0xCB, 0xC9, 0xCF, 0xCD, 
		0xC2, 0xC3, 0xC0, 0xC1, 0xC6, 0xC7, 0xC4, 0xC5, 
		0xDA, 0xDB, 0xD8, 0xD9, 0xDE, 0xDF, 0xDC, 0xDD, 
		0xD2, 0xD2, 0xD3, 0xD3, 0xD0, 0xD0, 0xD1, 0xD1, 
		0xD6, 0xD6, 0xD7, 0xD7, 0xD4, 0xD4, 0xD5, 0xD5
	};

	while (len--)
	{
		*buff = table_ulaw2alaw[*(unsigned char *)buff];
		buff++;
	}
}

static void alaw2ulaw(unsigned char *buff, unsigned long len)
{
	static unsigned char table_alaw2ulaw[] =
	{
		0x29, 0x2A, 0x27, 0x28, 0x2D, 0x2E, 0x2B, 0x2C, 
		0x21, 0x22, 0x1F, 0x20, 0x25, 0x26, 0x23, 0x24, 
		0x39, 0x3A, 0x37, 0x38, 0x3D, 0x3E, 0x3B, 0x3C, 
		0x31, 0x32, 0x2F, 0x30, 0x35, 0x36, 0x33, 0x34, 
		0x0A, 0x0B, 0x08, 0x09, 0x0E, 0x0F, 0x0C, 0x0D, 
		0x02, 0x03, 0x00, 0x01, 0x06, 0x07, 0x04, 0x05, 
		0x1A, 0x1B, 0x18, 0x19, 0x1E, 0x1F, 0x1C, 0x1D, 
		0x12, 0x13, 0x10, 0x11, 0x16, 0x17, 0x14, 0x15, 
		0x62, 0x63, 0x60, 0x61, 0x66, 0x67, 0x64, 0x65, 
		0x5D, 0x5D, 0x5C, 0x5C, 0x5F, 0x5F, 0x5E, 0x5E, 
		0x74, 0x76, 0x70, 0x72, 0x7C, 0x7E, 0x78, 0x7A, 
		0x6A, 0x6B, 0x68, 0x69, 0x6E, 0x6F, 0x6C, 0x6D, 
		0x48, 0x49, 0x46, 0x47, 0x4C, 0x4D, 0x4A, 0x4B, 
		0x40, 0x41, 0x3F, 0x3F, 0x44, 0x45, 0x42, 0x43, 
		0x56, 0x57, 0x54, 0x55, 0x5A, 0x5B, 0x58, 0x59, 
		0x4F, 0x4F, 0x4E, 0x4E, 0x52, 0x53, 0x50, 0x51, 
		0xA9, 0xAA, 0xA7, 0xA8, 0xAD, 0xAE, 0xAB, 0xAC, 
		0xA1, 0xA2, 0x9F, 0xA0, 0xA5, 0xA6, 0xA3, 0xA4, 
		0xB9, 0xBA, 0xB7, 0xB8, 0xBD, 0xBE, 0xBB, 0xBC, 
		0xB1, 0xB2, 0xAF, 0xB0, 0xB5, 0xB6, 0xB3, 0xB4, 
		0x8A, 0x8B, 0x88, 0x89, 0x8E, 0x8F, 0x8C, 0x8D, 
		0x82, 0x83, 0x80, 0x81, 0x86, 0x87, 0x84, 0x85, 
		0x9A, 0x9B, 0x98, 0x99, 0x9E, 0x9F, 0x9C, 0x9D, 
		0x92, 0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95, 
		0xE2, 0xE3, 0xE0, 0xE1, 0xE6, 0xE7, 0xE4, 0xE5, 
		0xDD, 0xDD, 0xDC, 0xDC, 0xDF, 0xDF, 0xDE, 0xDE, 
		0xF4, 0xF6, 0xF0, 0xF2, 0xFC, 0xFE, 0xF8, 0xFA, 
		0xEA, 0xEB, 0xE8, 0xE9, 0xEE, 0xEF, 0xEC, 0xED, 
		0xC8, 0xC9, 0xC6, 0xC7, 0xCC, 0xCD, 0xCA, 0xCB, 
		0xC0, 0xC1, 0xBF, 0xBF, 0xC4, 0xC5, 0xC2, 0xC3, 
		0xD6, 0xD7, 0xD4, 0xD5, 0xDA, 0xDB, 0xD8, 0xD9, 
		0xCF, 0xCF, 0xCE, 0xCE, 0xD2, 0xD3, 0xD0, 0xD1
	};

        while (len--)
        {
                *buff = table_alaw2ulaw[*(unsigned char *)buff];
                buff++;
	}
}

static ssize_t ixj_read(struct file * file_p, char __user *buf, size_t length, loff_t * ppos)
{
	unsigned long i = *ppos;
	IXJ * j = get_ixj(NUM(file_p->f_path.dentry->d_inode));

	DECLARE_WAITQUEUE(wait, current);

	if (j->flags.inread)
		return -EALREADY;

	j->flags.inread = 1;

	add_wait_queue(&j->read_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	mb();

	while (!j->read_buffer_ready || (j->dtmf_state && j->flags.dtmf_oob)) {
		++j->read_wait;
		if (file_p->f_flags & O_NONBLOCK) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->read_q, &wait);
			j->flags.inread = 0;
			return -EAGAIN;
		}
		if (!ixj_hookstate(j)) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->read_q, &wait);
			j->flags.inread = 0;
			return 0;
		}
		interruptible_sleep_on(&j->read_q);
		if (signal_pending(current)) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->read_q, &wait);
			j->flags.inread = 0;
			return -EINTR;
		}
	}

	remove_wait_queue(&j->read_q, &wait);
	set_current_state(TASK_RUNNING);
	
	if(j->rec_codec == ALAW)
		ulaw2alaw(j->read_buffer, min(length, j->read_buffer_size));
	i = copy_to_user(buf, j->read_buffer, min(length, j->read_buffer_size));
	j->read_buffer_ready = 0;
	if (i) {
		j->flags.inread = 0;
		return -EFAULT;
	} else {
		j->flags.inread = 0;
		return min(length, j->read_buffer_size);
	}
}

static ssize_t ixj_enhanced_read(struct file * file_p, char __user *buf, size_t length,
			  loff_t * ppos)
{
	int pre_retval;
	ssize_t read_retval = 0;
	IXJ *j = get_ixj(NUM(file_p->f_path.dentry->d_inode));

	pre_retval = ixj_PreRead(j, 0L);
	switch (pre_retval) {
	case NORMAL:
		read_retval = ixj_read(file_p, buf, length, ppos);
		ixj_PostRead(j, 0L);
		break;
	case NOPOST:
		read_retval = ixj_read(file_p, buf, length, ppos);
		break;
	case POSTONLY:
		ixj_PostRead(j, 0L);
		break;
	default:
		read_retval = pre_retval;
	}
	return read_retval;
}

static ssize_t ixj_write(struct file *file_p, const char __user *buf, size_t count, loff_t * ppos)
{
	unsigned long i = *ppos;
	IXJ *j = file_p->private_data;

	DECLARE_WAITQUEUE(wait, current);

	if (j->flags.inwrite)
		return -EALREADY;

	j->flags.inwrite = 1;

	add_wait_queue(&j->write_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	mb();


	while (!j->write_buffers_empty) {
		++j->write_wait;
		if (file_p->f_flags & O_NONBLOCK) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->write_q, &wait);
			j->flags.inwrite = 0;
			return -EAGAIN;
		}
		if (!ixj_hookstate(j)) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->write_q, &wait);
			j->flags.inwrite = 0;
			return 0;
		}
		interruptible_sleep_on(&j->write_q);
		if (signal_pending(current)) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&j->write_q, &wait);
			j->flags.inwrite = 0;
			return -EINTR;
		}
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&j->write_q, &wait);
	if (j->write_buffer_wp + count >= j->write_buffer_end)
		j->write_buffer_wp = j->write_buffer;
	i = copy_from_user(j->write_buffer_wp, buf, min(count, j->write_buffer_size));
	if (i) {
		j->flags.inwrite = 0;
		return -EFAULT;
	}
       if(j->play_codec == ALAW)
               alaw2ulaw(j->write_buffer_wp, min(count, j->write_buffer_size));
	j->flags.inwrite = 0;
	return min(count, j->write_buffer_size);
}

static ssize_t ixj_enhanced_write(struct file * file_p, const char __user *buf, size_t count, loff_t * ppos)
{
	int pre_retval;
	ssize_t write_retval = 0;

	IXJ *j = get_ixj(NUM(file_p->f_path.dentry->d_inode));

	pre_retval = ixj_PreWrite(j, 0L);
	switch (pre_retval) {
	case NORMAL:
		write_retval = ixj_write(file_p, buf, count, ppos);
		if (write_retval > 0) {
			ixj_PostWrite(j, 0L);
			j->write_buffer_wp += write_retval;
			j->write_buffers_empty--;
		}
		break;
	case NOPOST:
		write_retval = ixj_write(file_p, buf, count, ppos);
		if (write_retval > 0) {
			j->write_buffer_wp += write_retval;
			j->write_buffers_empty--;
		}
		break;
	case POSTONLY:
		ixj_PostWrite(j, 0L);
		break;
	default:
		write_retval = pre_retval;
	}
	return write_retval;
}

static void ixj_read_frame(IXJ *j)
{
	int cnt, dly;

	if (j->read_buffer) {
		for (cnt = 0; cnt < j->rec_frame_size * 2; cnt += 2) {
			if (!(cnt % 16) && !IsRxReady(j)) {
				dly = 0;
				while (!IsRxReady(j)) {
					if (dly++ > 5) {
						dly = 0;
						break;
					}
					udelay(10);
				}
			}
			
			if (j->rec_codec == G729 && (cnt == 0 || cnt == 10 || cnt == 20)) {
				inb_p(j->DSPbase + 0x0E);
				inb_p(j->DSPbase + 0x0F);
			}
			*(j->read_buffer + cnt) = inb_p(j->DSPbase + 0x0E);
			*(j->read_buffer + cnt + 1) = inb_p(j->DSPbase + 0x0F);
		}
		++j->framesread;
		if (j->intercom != -1) {
			if (IsTxReady(get_ixj(j->intercom))) {
				for (cnt = 0; cnt < j->rec_frame_size * 2; cnt += 2) {
					if (!(cnt % 16) && !IsTxReady(j)) {
						dly = 0;
						while (!IsTxReady(j)) {
							if (dly++ > 5) {
								dly = 0;
								break;
							}
							udelay(10);
						}
					}
					outb_p(*(j->read_buffer + cnt), get_ixj(j->intercom)->DSPbase + 0x0C);
					outb_p(*(j->read_buffer + cnt + 1), get_ixj(j->intercom)->DSPbase + 0x0D);
				}
				get_ixj(j->intercom)->frameswritten++;
			}
		} else {
			j->read_buffer_ready = 1;
			wake_up_interruptible(&j->read_q);	

			wake_up_interruptible(&j->poll_q);	

			if(j->ixj_signals[SIG_READ_READY])
				ixj_kill_fasync(j, SIG_READ_READY, POLL_OUT);
		}
	}
}

static short fsk[][6][20] =
{
	{
		{
			0, 17846, 29934, 32364, 24351, 8481, -10126, -25465, -32587, -29196,
			-16384, 1715, 19260, 30591, 32051, 23170, 6813, -11743, -26509, -32722
		},
		{
			-28377, -14876, 3425, 20621, 31163, 31650, 21925, 5126, -13328, -27481,
			-32767, -27481, -13328, 5126, 21925, 31650, 31163, 20621, 3425, -14876
		},
		{
			-28377, -32722, -26509, -11743, 6813, 23170, 32051, 30591, 19260, 1715,
			-16384, -29196, -32587, -25465, -10126, 8481, 24351, 32364, 29934, 17846
		},
		{
			0, -17846, -29934, -32364, -24351, -8481, 10126, 25465, 32587, 29196,
			16384, -1715, -19260, -30591, -32051, -23170, -6813, 11743, 26509, 32722
		},
		{
			28377, 14876, -3425, -20621, -31163, -31650, -21925, -5126, 13328, 27481,
			32767, 27481, 13328, -5126, -21925, -31650, -31163, -20621, -3425, 14876
		},
		{
			28377, 32722, 26509, 11743, -6813, -23170, -32051, -30591, -19260, -1715,
			16384, 29196, 32587, 25465, 10126, -8481, -24351, -32364, -29934, -17846
		}
	},
	{
		{
			0, 10126, 19260, 26509, 31163, 32767, 31163, 26509, 19260, 10126,
			0, -10126, -19260, -26509, -31163, -32767, -31163, -26509, -19260, -10126
		},
		{
			-28377, -21925, -13328, -3425, 6813, 16384, 24351, 29934, 32587, 32051,
			28377, 21925, 13328, 3425, -6813, -16384, -24351, -29934, -32587, -32051
		},
		{
			-28377, -32051, -32587, -29934, -24351, -16384, -6813, 3425, 13328, 21925,
			28377, 32051, 32587, 29934, 24351, 16384, 6813, -3425, -13328, -21925
		},
		{
			0, -10126, -19260, -26509, -31163, -32767, -31163, -26509, -19260, -10126,
			0, 10126, 19260, 26509, 31163, 32767, 31163, 26509, 19260, 10126
		},
		{
			28377, 21925, 13328, 3425, -6813, -16383, -24351, -29934, -32587, -32051,
			-28377, -21925, -13328, -3425, 6813, 16383, 24351, 29934, 32587, 32051
		},
		{
			28377, 32051, 32587, 29934, 24351, 16384, 6813, -3425, -13328, -21925,
			-28377, -32051, -32587, -29934, -24351, -16384, -6813, 3425, 13328, 21925
		}
	}
};


static void ixj_write_cid_bit(IXJ *j, int bit)
{
	while (j->fskcnt < 20) {
		if(j->fskdcnt < (j->fsksize - 1))
			j->fskdata[j->fskdcnt++] = fsk[bit][j->fskz][j->fskcnt];

		j->fskcnt += 3;
	}
	j->fskcnt %= 20;

	if (!bit)
		j->fskz++;
	if (j->fskz >= 6)
		j->fskz = 0;

}

static void ixj_write_cid_byte(IXJ *j, char byte)
{
	IXJ_CBYTE cb;

		cb.cbyte = byte;
		ixj_write_cid_bit(j, 0);
		ixj_write_cid_bit(j, cb.cbits.b0 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b1 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b2 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b3 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b4 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b5 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b6 ? 1 : 0);
		ixj_write_cid_bit(j, cb.cbits.b7 ? 1 : 0);
		ixj_write_cid_bit(j, 1);
}

static void ixj_write_cid_seize(IXJ *j)
{
	int cnt;

	for (cnt = 0; cnt < 150; cnt++) {
		ixj_write_cid_bit(j, 0);
		ixj_write_cid_bit(j, 1);
	}
	for (cnt = 0; cnt < 180; cnt++) {
		ixj_write_cid_bit(j, 1);
	}
}

static void ixj_write_cidcw_seize(IXJ *j)
{
	int cnt;

	for (cnt = 0; cnt < 80; cnt++) {
		ixj_write_cid_bit(j, 1);
	}
}

static int ixj_write_cid_string(IXJ *j, char *s, int checksum)
{
	int cnt;

	for (cnt = 0; cnt < strlen(s); cnt++) {
		ixj_write_cid_byte(j, s[cnt]);
		checksum = (checksum + s[cnt]);
	}
	return checksum;
}

static void ixj_pad_fsk(IXJ *j, int pad)
{
	int cnt; 

	for (cnt = 0; cnt < pad; cnt++) {
		if(j->fskdcnt < (j->fsksize - 1))
			j->fskdata[j->fskdcnt++] = 0x0000;
	}
	for (cnt = 0; cnt < 720; cnt++) {
		if(j->fskdcnt < (j->fsksize - 1))
			j->fskdata[j->fskdcnt++] = 0x0000;
	}
}

static void ixj_pre_cid(IXJ *j)
{
	j->cid_play_codec = j->play_codec;
	j->cid_play_frame_size = j->play_frame_size;
	j->cid_play_volume = get_play_volume(j);
	j->cid_play_flag = j->flags.playing;

	j->cid_rec_codec = j->rec_codec;
	j->cid_rec_volume = get_rec_volume(j);
	j->cid_rec_flag = j->flags.recording;

	j->cid_play_aec_level = j->aec_level;

	switch(j->baseframe.low) {
		case 0xA0:
			j->cid_base_frame_size = 20;
			break;
		case 0x50:
			j->cid_base_frame_size = 10;
			break;
		case 0xF0:
			j->cid_base_frame_size = 30;
			break;
	}

	ixj_play_stop(j);
	ixj_cpt_stop(j);

	j->flags.cidplay = 1;

	set_base_frame(j, 30);
	set_play_codec(j, LINEAR16);
	set_play_volume(j, 0x1B);
	ixj_play_start(j);
}

static void ixj_post_cid(IXJ *j)
{
	ixj_play_stop(j);

	if(j->cidsize > 5000) {
		SLIC_SetState(PLD_SLIC_STATE_STANDBY, j);
	}
	j->flags.cidplay = 0;
	if(ixjdebug & 0x0200) {
		printk("IXJ phone%d Finished Playing CallerID data %ld\n", j->board, jiffies);
	}

	ixj_fsk_free(j);

	j->fskdcnt = 0;
	set_base_frame(j, j->cid_base_frame_size);
	set_play_codec(j, j->cid_play_codec);
	ixj_aec_start(j, j->cid_play_aec_level);
	set_play_volume(j, j->cid_play_volume);

	set_rec_codec(j, j->cid_rec_codec);
	set_rec_volume(j, j->cid_rec_volume);

	if(j->cid_rec_flag)
		ixj_record_start(j);

	if(j->cid_play_flag)
		ixj_play_start(j);

	if(j->cid_play_flag) {
		wake_up_interruptible(&j->write_q);	
	}
}

static void ixj_write_cid(IXJ *j)
{
	char sdmf1[50];
	char sdmf2[50];
	char sdmf3[80];
	char mdmflen, len1, len2, len3;
	int pad;

	int checksum = 0;

	if (j->dsp.low == 0x20 || j->flags.cidplay)
		return;

	j->fskz = j->fskphase = j->fskcnt = j->fskdcnt = 0;
	j->cidsize = j->cidcnt = 0;

	ixj_fsk_alloc(j);

	strcpy(sdmf1, j->cid_send.month);
	strcat(sdmf1, j->cid_send.day);
	strcat(sdmf1, j->cid_send.hour);
	strcat(sdmf1, j->cid_send.min);
	strcpy(sdmf2, j->cid_send.number);
	strcpy(sdmf3, j->cid_send.name);

	len1 = strlen(sdmf1);
	len2 = strlen(sdmf2);
	len3 = strlen(sdmf3);
	mdmflen = len1 + len2 + len3 + 6;

	while(1){
		ixj_write_cid_seize(j);

		ixj_write_cid_byte(j, 0x80);
		checksum = 0x80;
		ixj_write_cid_byte(j, mdmflen);
		checksum = checksum + mdmflen;

		ixj_write_cid_byte(j, 0x01);
		checksum = checksum + 0x01;
		ixj_write_cid_byte(j, len1);
		checksum = checksum + len1;
		checksum = ixj_write_cid_string(j, sdmf1, checksum);
		if(ixj_hookstate(j) & 1)
			break;

		ixj_write_cid_byte(j, 0x02);
		checksum = checksum + 0x02;
		ixj_write_cid_byte(j, len2);
		checksum = checksum + len2;
		checksum = ixj_write_cid_string(j, sdmf2, checksum);
		if(ixj_hookstate(j) & 1)
			break;

		ixj_write_cid_byte(j, 0x07);
		checksum = checksum + 0x07;
		ixj_write_cid_byte(j, len3);
		checksum = checksum + len3;
		checksum = ixj_write_cid_string(j, sdmf3, checksum);
		if(ixj_hookstate(j) & 1)
			break;

		checksum %= 256;
		checksum ^= 0xFF;
		checksum += 1;

		ixj_write_cid_byte(j, (char) checksum);

		pad = j->fskdcnt % 240;
		if (pad) {
			pad = 240 - pad;
		}
		ixj_pad_fsk(j, pad);
		break;
	}

	ixj_write_frame(j);
}

static void ixj_write_cidcw(IXJ *j)
{
	IXJ_TONE ti;

	char sdmf1[50];
	char sdmf2[50];
	char sdmf3[80];
	char mdmflen, len1, len2, len3;
	int pad;

	int checksum = 0;

	if (j->dsp.low == 0x20 || j->flags.cidplay)
		return;

	j->fskz = j->fskphase = j->fskcnt = j->fskdcnt = 0;
	j->cidsize = j->cidcnt = 0;

	ixj_fsk_alloc(j);

	j->flags.cidcw_ack = 0;

	ti.tone_index = 23;
	ti.gain0 = 1;
	ti.freq0 = hz440;
	ti.gain1 = 0;
	ti.freq1 = 0;
	ixj_init_tone(j, &ti);

	ixj_set_tone_on(1500, j);
	ixj_set_tone_off(32, j);
	if(ixjdebug & 0x0200) {
		printk("IXJ cidcw phone%d first tone start at %ld\n", j->board, jiffies);
	}
	ixj_play_tone(j, 23);

	clear_bit(j->board, &j->busyflags);
	while(j->tone_state)
		schedule_timeout_interruptible(1);
	while(test_and_set_bit(j->board, (void *)&j->busyflags) != 0)
		schedule_timeout_interruptible(1);
	if(ixjdebug & 0x0200) {
		printk("IXJ cidcw phone%d first tone end at %ld\n", j->board, jiffies);
	}

	ti.tone_index = 24;
	ti.gain0 = 1;
	ti.freq0 = hz2130;
	ti.gain1 = 0;
	ti.freq1 = hz2750;
	ixj_init_tone(j, &ti);

	ixj_set_tone_off(10, j);
	ixj_set_tone_on(600, j);
	if(ixjdebug & 0x0200) {
		printk("IXJ cidcw phone%d second tone start at %ld\n", j->board, jiffies);
	}
	ixj_play_tone(j, 24);

	clear_bit(j->board, &j->busyflags);
	while(j->tone_state)
		schedule_timeout_interruptible(1);
	while(test_and_set_bit(j->board, (void *)&j->busyflags) != 0)
		schedule_timeout_interruptible(1);
	if(ixjdebug & 0x0200) {
		printk("IXJ cidcw phone%d sent second tone at %ld\n", j->board, jiffies);
	}

	j->cidcw_wait = jiffies + ((50 * hertz) / 100);

	clear_bit(j->board, &j->busyflags);
	while(!j->flags.cidcw_ack && time_before(jiffies, j->cidcw_wait))
		schedule_timeout_interruptible(1);
	while(test_and_set_bit(j->board, (void *)&j->busyflags) != 0)
		schedule_timeout_interruptible(1);
	j->cidcw_wait = 0;
	if(!j->flags.cidcw_ack) {
		if(ixjdebug & 0x0200) {
			printk("IXJ cidcw phone%d did not receive ACK from display %ld\n", j->board, jiffies);
		}
		ixj_post_cid(j);
		if(j->cid_play_flag) {
			wake_up_interruptible(&j->write_q);	
		}
		return;
	} else {
		ixj_pre_cid(j);
	}
	j->flags.cidcw_ack = 0;
	strcpy(sdmf1, j->cid_send.month);
	strcat(sdmf1, j->cid_send.day);
	strcat(sdmf1, j->cid_send.hour);
	strcat(sdmf1, j->cid_send.min);
	strcpy(sdmf2, j->cid_send.number);
	strcpy(sdmf3, j->cid_send.name);

	len1 = strlen(sdmf1);
	len2 = strlen(sdmf2);
	len3 = strlen(sdmf3);
	mdmflen = len1 + len2 + len3 + 6;

	ixj_write_cidcw_seize(j);

	ixj_write_cid_byte(j, 0x80);
	checksum = 0x80;
	ixj_write_cid_byte(j, mdmflen);
	checksum = checksum + mdmflen;

	ixj_write_cid_byte(j, 0x01);
	checksum = checksum + 0x01;
	ixj_write_cid_byte(j, len1);
	checksum = checksum + len1;
	checksum = ixj_write_cid_string(j, sdmf1, checksum);

	ixj_write_cid_byte(j, 0x02);
	checksum = checksum + 0x02;
	ixj_write_cid_byte(j, len2);
	checksum = checksum + len2;
	checksum = ixj_write_cid_string(j, sdmf2, checksum);

	ixj_write_cid_byte(j, 0x07);
	checksum = checksum + 0x07;
	ixj_write_cid_byte(j, len3);
	checksum = checksum + len3;
	checksum = ixj_write_cid_string(j, sdmf3, checksum);

	checksum %= 256;
	checksum ^= 0xFF;
	checksum += 1;

	ixj_write_cid_byte(j, (char) checksum);

	pad = j->fskdcnt % 240;
	if (pad) {
		pad = 240 - pad;
	}
	ixj_pad_fsk(j, pad);
	if(ixjdebug & 0x0200) {
		printk("IXJ cidcw phone%d sent FSK data at %ld\n", j->board, jiffies);
	}
}

static void ixj_write_vmwi(IXJ *j, int msg)
{
	char mdmflen;
	int pad;

	int checksum = 0;

	if (j->dsp.low == 0x20 || j->flags.cidplay)
		return;

	j->fskz = j->fskphase = j->fskcnt = j->fskdcnt = 0;
	j->cidsize = j->cidcnt = 0;

	ixj_fsk_alloc(j);

	mdmflen = 3;

	if (j->port == PORT_POTS)
		SLIC_SetState(PLD_SLIC_STATE_OHT, j);

	ixj_write_cid_seize(j);

	ixj_write_cid_byte(j, 0x82);
	checksum = 0x82;
	ixj_write_cid_byte(j, mdmflen);
	checksum = checksum + mdmflen;

	ixj_write_cid_byte(j, 0x0B);
	checksum = checksum + 0x0B;
	ixj_write_cid_byte(j, 1);
	checksum = checksum + 1;

	if(msg) {
		ixj_write_cid_byte(j, 0xFF);
		checksum = checksum + 0xFF;
	}
	else {
		ixj_write_cid_byte(j, 0x00);
		checksum = checksum + 0x00;
	}

	checksum %= 256;
	checksum ^= 0xFF;
	checksum += 1;

	ixj_write_cid_byte(j, (char) checksum);

	pad = j->fskdcnt % 240;
	if (pad) {
		pad = 240 - pad;
	}
	ixj_pad_fsk(j, pad);
}

static void ixj_write_frame(IXJ *j)
{
	int cnt, frame_count, dly;
	IXJ_WORD dat;

	frame_count = 0;
	if(j->flags.cidplay) {
		for(cnt = 0; cnt < 480; cnt++) {
			if (!(cnt % 16) && !IsTxReady(j)) {
				dly = 0;
				while (!IsTxReady(j)) {
					if (dly++ > 5) {
						dly = 0;
						break;
					}
					udelay(10);
				}
			}
			dat.word = j->fskdata[j->cidcnt++];
			outb_p(dat.bytes.low, j->DSPbase + 0x0C);
			outb_p(dat.bytes.high, j->DSPbase + 0x0D);
			cnt++;
		}
		if(j->cidcnt >= j->fskdcnt) {
			ixj_post_cid(j);
		}
		if (j->write_buffer_rp > j->write_buffer_wp) {
			j->write_buffer_rp += j->cid_play_frame_size * 2;
			if (j->write_buffer_rp >= j->write_buffer_end) {
				j->write_buffer_rp = j->write_buffer;
			}
			j->write_buffers_empty++;
			wake_up_interruptible(&j->write_q);	

			wake_up_interruptible(&j->poll_q);	
		}
	} else if (j->write_buffer && j->write_buffers_empty < 1) { 
		if (j->write_buffer_wp > j->write_buffer_rp) {
			frame_count =
			    (j->write_buffer_wp - j->write_buffer_rp) / (j->play_frame_size * 2);
		}
		if (j->write_buffer_rp > j->write_buffer_wp) {
			frame_count =
			    (j->write_buffer_wp - j->write_buffer) / (j->play_frame_size * 2) +
			    (j->write_buffer_end - j->write_buffer_rp) / (j->play_frame_size * 2);
		}
		if (frame_count >= 1) {
			if (j->ver.low == 0x12 && j->play_mode && j->flags.play_first_frame) {
				BYTES blankword;

				switch (j->play_mode) {
				case PLAYBACK_MODE_ULAW:
				case PLAYBACK_MODE_ALAW:
					blankword.low = blankword.high = 0xFF;
					break;
				case PLAYBACK_MODE_8LINEAR:
				case PLAYBACK_MODE_16LINEAR:
				default:
					blankword.low = blankword.high = 0x00;
					break;
				case PLAYBACK_MODE_8LINEAR_WSS:
					blankword.low = blankword.high = 0x80;
					break;
				}
				for (cnt = 0; cnt < 16; cnt++) {
					if (!(cnt % 16) && !IsTxReady(j)) {
						dly = 0;
						while (!IsTxReady(j)) {
							if (dly++ > 5) {
								dly = 0;
								break;
							}
							udelay(10);
						}
					}
					outb_p((blankword.low), j->DSPbase + 0x0C);
					outb_p((blankword.high), j->DSPbase + 0x0D);
				}
				j->flags.play_first_frame = 0;
			} else	if (j->play_codec == G723_63 && j->flags.play_first_frame) {
				for (cnt = 0; cnt < 24; cnt++) {
					BYTES blankword;

					if(cnt == 12) {
						blankword.low = 0x02;
						blankword.high = 0x00;
					}
					else {
						blankword.low = blankword.high = 0x00;
					}
					if (!(cnt % 16) && !IsTxReady(j)) {
						dly = 0;
						while (!IsTxReady(j)) {
							if (dly++ > 5) {
								dly = 0;
								break;
							}
							udelay(10);
						}
					}
					outb_p((blankword.low), j->DSPbase + 0x0C);
					outb_p((blankword.high), j->DSPbase + 0x0D);
				}
				j->flags.play_first_frame = 0;
			}
			for (cnt = 0; cnt < j->play_frame_size * 2; cnt += 2) {
				if (!(cnt % 16) && !IsTxReady(j)) {
					dly = 0;
					while (!IsTxReady(j)) {
						if (dly++ > 5) {
							dly = 0;
							break;
						}
						udelay(10);
					}
				}
			
				if (j->play_codec == G729 && (cnt == 0 || cnt == 10 || cnt == 20)) {
					if (j->write_buffer_rp[cnt] == 0 &&
					    j->write_buffer_rp[cnt + 1] == 0 &&
					    j->write_buffer_rp[cnt + 2] == 0 &&
					    j->write_buffer_rp[cnt + 3] == 0 &&
					    j->write_buffer_rp[cnt + 4] == 0 &&
					    j->write_buffer_rp[cnt + 5] == 0 &&
					    j->write_buffer_rp[cnt + 6] == 0 &&
					    j->write_buffer_rp[cnt + 7] == 0 &&
					    j->write_buffer_rp[cnt + 8] == 0 &&
					    j->write_buffer_rp[cnt + 9] == 0) {
					
						outb_p(0x00, j->DSPbase + 0x0C);
						outb_p(0x00, j->DSPbase + 0x0D);
					} else {
					
						outb_p(0x01, j->DSPbase + 0x0C);
						outb_p(0x00, j->DSPbase + 0x0D);
					}
				}
				outb_p(*(j->write_buffer_rp + cnt), j->DSPbase + 0x0C);
				outb_p(*(j->write_buffer_rp + cnt + 1), j->DSPbase + 0x0D);
				*(j->write_buffer_rp + cnt) = 0;
				*(j->write_buffer_rp + cnt + 1) = 0;
			}
			j->write_buffer_rp += j->play_frame_size * 2;
			if (j->write_buffer_rp >= j->write_buffer_end) {
				j->write_buffer_rp = j->write_buffer;
			}
			j->write_buffers_empty++;
			wake_up_interruptible(&j->write_q);	

			wake_up_interruptible(&j->poll_q);	

			++j->frameswritten;
		}
	} else {
		j->drybuffer++;
	}
	if(j->ixj_signals[SIG_WRITE_READY]) {
		ixj_kill_fasync(j, SIG_WRITE_READY, POLL_OUT);
	}
}

static int idle(IXJ *j)
{
	if (ixj_WriteDSPCommand(0x0000, j))		

		return 0;

	if (j->ssr.high || j->ssr.low) {
		return 0;
	} else {
		j->play_mode = -1;
		j->flags.playing = 0;
		j->rec_mode = -1;
		j->flags.recording = 0;
		return 1;
        }
}

static int set_base_frame(IXJ *j, int size)
{
	unsigned short cmd;
	int cnt;

	idle(j);
	j->cid_play_aec_level = j->aec_level;
	aec_stop(j);
	for (cnt = 0; cnt < 10; cnt++) {
		if (idle(j))
			break;
	}
	if (j->ssr.high || j->ssr.low)
		return -1;
	if (j->dsp.low != 0x20) {
		switch (size) {
		case 30:
			cmd = 0x07F0;
			
			break;
		case 20:
			cmd = 0x07A0;
			
			break;
		case 10:
			cmd = 0x0750;
			
			break;
		default:
			return -1;
		}
	} else {
		if (size == 30)
			return size;
		else
			return -1;
	}
	if (ixj_WriteDSPCommand(cmd, j)) {
		j->baseframe.high = j->baseframe.low = 0xFF;
		return -1;
	} else {
		j->baseframe.high = j->ssr.high;
		j->baseframe.low = j->ssr.low;
		
		if(j->baseframe.high == 0x00 && j->baseframe.low == 0x00) {
			return -1;
		}
	}
	ixj_aec_start(j, j->cid_play_aec_level);
	return size;
}

static int set_rec_codec(IXJ *j, int rate)
{
	int retval = 0;

	j->rec_codec = rate;

	switch (rate) {
	case G723_63:
		if (j->ver.low != 0x12 || ixj_convert_loaded) {
			j->rec_frame_size = 12;
			j->rec_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case G723_53:
		if (j->ver.low != 0x12 || ixj_convert_loaded) {
			j->rec_frame_size = 10;
			j->rec_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case TS85:
		if (j->dsp.low == 0x20 || j->flags.ts85_loaded) {
			j->rec_frame_size = 16;
			j->rec_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case TS48:
		if (j->ver.low != 0x12 || ixj_convert_loaded) {
			j->rec_frame_size = 9;
			j->rec_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case TS41:
		if (j->ver.low != 0x12 || ixj_convert_loaded) {
			j->rec_frame_size = 8;
			j->rec_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case G728:
		if (j->dsp.low != 0x20) {
			j->rec_frame_size = 48;
			j->rec_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case G729:
		if (j->dsp.low != 0x20) {
			if (!j->flags.g729_loaded) {
				retval = 1;
				break;
			}
			switch (j->baseframe.low) {
			case 0xA0:
				j->rec_frame_size = 10;
				break;
			case 0x50:
				j->rec_frame_size = 5;
				break;
			default:
				j->rec_frame_size = 15;
				break;
			}
			j->rec_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case G729B:
		if (j->dsp.low != 0x20) {
			if (!j->flags.g729_loaded) {
				retval = 1;
				break;
			}
			switch (j->baseframe.low) {
			case 0xA0:
				j->rec_frame_size = 12;
				break;
			case 0x50:
				j->rec_frame_size = 6;
				break;
			default:
				j->rec_frame_size = 18;
				break;
			}
			j->rec_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case ULAW:
		switch (j->baseframe.low) {
		case 0xA0:
			j->rec_frame_size = 80;
			break;
		case 0x50:
			j->rec_frame_size = 40;
			break;
		default:
			j->rec_frame_size = 120;
			break;
		}
		j->rec_mode = 4;
		break;
	case ALAW:
		switch (j->baseframe.low) {
		case 0xA0:
			j->rec_frame_size = 80;
			break;
		case 0x50:
			j->rec_frame_size = 40;
			break;
		default:
			j->rec_frame_size = 120;
			break;
		}
		j->rec_mode = 4;
		break;
	case LINEAR16:
		switch (j->baseframe.low) {
		case 0xA0:
			j->rec_frame_size = 160;
			break;
		case 0x50:
			j->rec_frame_size = 80;
			break;
		default:
			j->rec_frame_size = 240;
			break;
		}
		j->rec_mode = 5;
		break;
	case LINEAR8:
		switch (j->baseframe.low) {
		case 0xA0:
			j->rec_frame_size = 80;
			break;
		case 0x50:
			j->rec_frame_size = 40;
			break;
		default:
			j->rec_frame_size = 120;
			break;
		}
		j->rec_mode = 6;
		break;
	case WSS:
		switch (j->baseframe.low) {
		case 0xA0:
			j->rec_frame_size = 80;
			break;
		case 0x50:
			j->rec_frame_size = 40;
			break;
		default:
			j->rec_frame_size = 120;
			break;
		}
		j->rec_mode = 7;
		break;
	default:
		kfree(j->read_buffer);
		j->rec_frame_size = 0;
		j->rec_mode = -1;
		j->read_buffer = NULL;
		j->read_buffer_size = 0;
		retval = 1;
		break;
	}
	return retval;
}

static int ixj_record_start(IXJ *j)
{
	unsigned short cmd = 0x0000;

	if (j->read_buffer) {
		ixj_record_stop(j);
	}
	j->flags.recording = 1;
	ixj_WriteDSPCommand(0x0FE0, j);	

	if(ixjdebug & 0x0002)
		printk("IXJ %d Starting Record Codec %d at %ld\n", j->board, j->rec_codec, jiffies);

	if (!j->rec_mode) {
		switch (j->rec_codec) {
		case G723_63:
			cmd = 0x5131;
			break;
		case G723_53:
			cmd = 0x5132;
			break;
		case TS85:
			cmd = 0x5130;	

			break;
		case TS48:
			cmd = 0x5133;	

			break;
		case TS41:
			cmd = 0x5134;	

			break;
		case G728:
			cmd = 0x5135;
			break;
		case G729:
		case G729B:
			cmd = 0x5136;
			break;
		default:
			return 1;
		}
		if (ixj_WriteDSPCommand(cmd, j))
			return -1;
	}
	if (!j->read_buffer) {
		if (!j->read_buffer)
			j->read_buffer = kmalloc(j->rec_frame_size * 2, GFP_ATOMIC);
		if (!j->read_buffer) {
			printk("Read buffer allocation for ixj board %d failed!\n", j->board);
			return -ENOMEM;
		}
	}
	j->read_buffer_size = j->rec_frame_size * 2;

	if (ixj_WriteDSPCommand(0x5102, j))		

		return -1;

	switch (j->rec_mode) {
	case 0:
		cmd = 0x1C03;	

		break;
	case 4:
		if (j->ver.low == 0x12) {
			cmd = 0x1E03;	

		} else {
			cmd = 0x1E01;	

		}
		break;
	case 5:
		if (j->ver.low == 0x12) {
			cmd = 0x1E83;	

		} else {
			cmd = 0x1E81;	

		}
		break;
	case 6:
		if (j->ver.low == 0x12) {
			cmd = 0x1F03;	

		} else {
			cmd = 0x1F01;	

		}
		break;
	case 7:
		if (j->ver.low == 0x12) {
			cmd = 0x1F83;	
		} else {
			cmd = 0x1F81;	
		}
		break;
	}
	if (ixj_WriteDSPCommand(cmd, j))
		return -1;

	if (j->flags.playing) {
		ixj_aec_start(j, j->aec_level);
	}
	return 0;
}

static void ixj_record_stop(IXJ *j)
{
	if (ixjdebug & 0x0002)
		printk("IXJ %d Stopping Record Codec %d at %ld\n", j->board, j->rec_codec, jiffies);

	kfree(j->read_buffer);
	j->read_buffer = NULL;
	j->read_buffer_size = 0;
	if (j->rec_mode > -1) {
		ixj_WriteDSPCommand(0x5120, j);
		j->rec_mode = -1;
	}
	j->flags.recording = 0;
}
static void ixj_vad(IXJ *j, int arg)
{
	if (arg)
		ixj_WriteDSPCommand(0x513F, j);
	else
		ixj_WriteDSPCommand(0x513E, j);
}

static void set_rec_depth(IXJ *j, int depth)
{
	if (depth > 60)
		depth = 60;
	if (depth < 0)
		depth = 0;
	ixj_WriteDSPCommand(0x5180 + depth, j);
}

static void set_dtmf_prescale(IXJ *j, int volume)
{
	ixj_WriteDSPCommand(0xCF07, j);
	ixj_WriteDSPCommand(volume, j);
}

static int get_dtmf_prescale(IXJ *j)
{
	ixj_WriteDSPCommand(0xCF05, j);
	return j->ssr.high << 8 | j->ssr.low;
}

static void set_rec_volume(IXJ *j, int volume)
{
	if(j->aec_level == AEC_AGC) {
		if (ixjdebug & 0x0002)
			printk(KERN_INFO "IXJ: /dev/phone%d Setting AGC Threshold to 0x%4.4x\n", j->board, volume);
		ixj_WriteDSPCommand(0xCF96, j);
		ixj_WriteDSPCommand(volume, j);
	} else {
		if (ixjdebug & 0x0002)
			printk(KERN_INFO "IXJ: /dev/phone %d Setting Record Volume to 0x%4.4x\n", j->board, volume);
		ixj_WriteDSPCommand(0xCF03, j);
		ixj_WriteDSPCommand(volume, j);
	}
}

static int set_rec_volume_linear(IXJ *j, int volume)
{
	int newvolume, dsprecmax;

	if (ixjdebug & 0x0002)
		printk(KERN_INFO "IXJ: /dev/phone %d Setting Linear Record Volume to 0x%4.4x\n", j->board, volume);
	if(volume > 100 || volume < 0) {
	  return -1;
	}

	
	switch (j->cardtype) {
	case QTI_PHONEJACK:
		dsprecmax = 0x440;
		break;
	case QTI_LINEJACK:
		dsprecmax = 0x180;
		ixj_mixer(0x0203, j);	
		ixj_mixer(0x0303, j);	
		ixj_mixer(0x0C00, j);	
		break;
	case QTI_PHONEJACK_LITE:
		dsprecmax = 0x4C0;
		break;
	case QTI_PHONEJACK_PCI:
		dsprecmax = 0x100;
		break;
	case QTI_PHONECARD:
		dsprecmax = 0x400;
		break;
	default:
		return -1;
	}
	newvolume = (dsprecmax * volume) / 100;
	set_rec_volume(j, newvolume);
	return 0;
}

static int get_rec_volume(IXJ *j)
{
	if(j->aec_level == AEC_AGC) {
		if (ixjdebug & 0x0002)
			printk(KERN_INFO "Getting AGC Threshold\n");
		ixj_WriteDSPCommand(0xCF86, j);
		if (ixjdebug & 0x0002)
			printk(KERN_INFO "AGC Threshold is 0x%2.2x%2.2x\n", j->ssr.high, j->ssr.low);
		return j->ssr.high << 8 | j->ssr.low;
	} else {
		if (ixjdebug & 0x0002)
			printk(KERN_INFO "Getting Record Volume\n");
		ixj_WriteDSPCommand(0xCF01, j);
		return j->ssr.high << 8 | j->ssr.low;
	}
}

static int get_rec_volume_linear(IXJ *j)
{
	int volume, newvolume, dsprecmax;

	switch (j->cardtype) {
	case QTI_PHONEJACK:
		dsprecmax = 0x440;
		break;
	case QTI_LINEJACK:
		dsprecmax = 0x180;
		break;
	case QTI_PHONEJACK_LITE:
		dsprecmax = 0x4C0;
		break;
	case QTI_PHONEJACK_PCI:
		dsprecmax = 0x100;
		break;
	case QTI_PHONECARD:
		dsprecmax = 0x400;
		break;
	default:
		return -1;
	}
	volume = get_rec_volume(j);
	newvolume = (volume * 100) / dsprecmax;
	if(newvolume > 100)
		newvolume = 100;
	return newvolume;
}

static int get_rec_level(IXJ *j)
{
	int retval;

	ixj_WriteDSPCommand(0xCF88, j);

	retval = j->ssr.high << 8 | j->ssr.low;
	retval = (retval * 256) / 240;
	return retval;
}

static void ixj_aec_start(IXJ *j, int level)
{
	j->aec_level = level;
	if (ixjdebug & 0x0002)
		printk(KERN_INFO "AGC set = 0x%2.2x\n", j->aec_level);
	if (!level) {
		aec_stop(j);
	} else {
		if (j->rec_codec == G729 || j->play_codec == G729 || j->rec_codec == G729B || j->play_codec == G729B) {
			ixj_WriteDSPCommand(0xE022, j);	

			ixj_WriteDSPCommand(0x0300, j);
		}
		ixj_WriteDSPCommand(0xB001, j);	

		ixj_WriteDSPCommand(0xE013, j);	

		switch (level) {
		case AEC_LOW:
			ixj_WriteDSPCommand(0x0000, j);	

			ixj_WriteDSPCommand(0xE011, j);
			ixj_WriteDSPCommand(0xFFFF, j);

			ixj_WriteDSPCommand(0xCF97, j);	
			ixj_WriteDSPCommand(0x0000, j);	
			
			break;

		case AEC_MED:
			ixj_WriteDSPCommand(0x0600, j);	

			ixj_WriteDSPCommand(0xE011, j);
			ixj_WriteDSPCommand(0x0080, j);

			ixj_WriteDSPCommand(0xCF97, j);	
			ixj_WriteDSPCommand(0x0000, j);	
			
			break;

		case AEC_HIGH:
			ixj_WriteDSPCommand(0x0C00, j);	

			ixj_WriteDSPCommand(0xE011, j);
			ixj_WriteDSPCommand(0x0080, j);

			ixj_WriteDSPCommand(0xCF97, j);	
			ixj_WriteDSPCommand(0x0000, j);	
			
			break;

		case AEC_AGC:
                        
			ixj_WriteDSPCommand(0x0002, j);	

			ixj_WriteDSPCommand(0xE011, j);
			ixj_WriteDSPCommand(0x0100, j);	

			ixj_WriteDSPCommand(0xE012, j);	

			if(j->cardtype == QTI_LINEJACK || j->cardtype == QTI_PHONECARD)
				ixj_WriteDSPCommand(0x0224, j);
			else
				ixj_WriteDSPCommand(0x1224, j);

			ixj_WriteDSPCommand(0xE014, j);
			ixj_WriteDSPCommand(0x0003, j);	

			ixj_WriteDSPCommand(0xE338, j);	

			
			ixj_WriteDSPCommand(0xCF90, j);	
			ixj_WriteDSPCommand(0x0020, j);	
	
			ixj_WriteDSPCommand(0xCF91, j);	
			ixj_WriteDSPCommand(0x1000, j);	
			
			ixj_WriteDSPCommand(0xCF92, j);	
			ixj_WriteDSPCommand(0x0800, j);	
		
			ixj_WriteDSPCommand(0xCF93, j);	
			ixj_WriteDSPCommand(0x1F40, j);	
			
			ixj_WriteDSPCommand(0xCF94, j);	
			ixj_WriteDSPCommand(0x0005, j);	
			
			ixj_WriteDSPCommand(0xCF95, j);	
			ixj_WriteDSPCommand(0x000D, j);	
			
			ixj_WriteDSPCommand(0xCF96, j);	
			ixj_WriteDSPCommand(0x1200, j);	
			
			ixj_WriteDSPCommand(0xCF97, j);	
			ixj_WriteDSPCommand(0x0001, j);	
			
			break;

		case AEC_AUTO:
			ixj_WriteDSPCommand(0x0002, j);	

			ixj_WriteDSPCommand(0xE011, j);
			ixj_WriteDSPCommand(0x0100, j);	

			ixj_WriteDSPCommand(0xE012, j);	

			if(j->cardtype == QTI_LINEJACK || j->cardtype == QTI_PHONECARD)
				ixj_WriteDSPCommand(0x0224, j);
			else
				ixj_WriteDSPCommand(0x1224, j);

			ixj_WriteDSPCommand(0xE014, j);
			ixj_WriteDSPCommand(0x0003, j);	

			ixj_WriteDSPCommand(0xE338, j);	

			break;
		}
	}
}

static void aec_stop(IXJ *j)
{
	j->aec_level = AEC_OFF;
	if (j->rec_codec == G729 || j->play_codec == G729 || j->rec_codec == G729B || j->play_codec == G729B) {
		ixj_WriteDSPCommand(0xE022, j);	

		ixj_WriteDSPCommand(0x0700, j);
	}
	if (j->play_mode != -1 && j->rec_mode != -1)
	{
		ixj_WriteDSPCommand(0xB002, j);	
	}
}

static int set_play_codec(IXJ *j, int rate)
{
	int retval = 0;

	j->play_codec = rate;

	switch (rate) {
	case G723_63:
		if (j->ver.low != 0x12 || ixj_convert_loaded) {
			j->play_frame_size = 12;
			j->play_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case G723_53:
		if (j->ver.low != 0x12 || ixj_convert_loaded) {
			j->play_frame_size = 10;
			j->play_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case TS85:
		if (j->dsp.low == 0x20 || j->flags.ts85_loaded) {
			j->play_frame_size = 16;
			j->play_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case TS48:
		if (j->ver.low != 0x12 || ixj_convert_loaded) {
			j->play_frame_size = 9;
			j->play_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case TS41:
		if (j->ver.low != 0x12 || ixj_convert_loaded) {
			j->play_frame_size = 8;
			j->play_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case G728:
		if (j->dsp.low != 0x20) {
			j->play_frame_size = 48;
			j->play_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case G729:
		if (j->dsp.low != 0x20) {
			if (!j->flags.g729_loaded) {
				retval = 1;
				break;
			}
			switch (j->baseframe.low) {
			case 0xA0:
				j->play_frame_size = 10;
				break;
			case 0x50:
				j->play_frame_size = 5;
				break;
			default:
				j->play_frame_size = 15;
				break;
			}
			j->play_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case G729B:
		if (j->dsp.low != 0x20) {
			if (!j->flags.g729_loaded) {
				retval = 1;
				break;
			}
			switch (j->baseframe.low) {
			case 0xA0:
				j->play_frame_size = 12;
				break;
			case 0x50:
				j->play_frame_size = 6;
				break;
			default:
				j->play_frame_size = 18;
				break;
			}
			j->play_mode = 0;
		} else {
			retval = 1;
		}
		break;
	case ULAW:
		switch (j->baseframe.low) {
		case 0xA0:
			j->play_frame_size = 80;
			break;
		case 0x50:
			j->play_frame_size = 40;
			break;
		default:
			j->play_frame_size = 120;
			break;
		}
		j->play_mode = 2;
		break;
	case ALAW:
		switch (j->baseframe.low) {
		case 0xA0:
			j->play_frame_size = 80;
			break;
		case 0x50:
			j->play_frame_size = 40;
			break;
		default:
			j->play_frame_size = 120;
			break;
		}
		j->play_mode = 2;
		break;
	case LINEAR16:
		switch (j->baseframe.low) {
		case 0xA0:
			j->play_frame_size = 160;
			break;
		case 0x50:
			j->play_frame_size = 80;
			break;
		default:
			j->play_frame_size = 240;
			break;
		}
		j->play_mode = 6;
		break;
	case LINEAR8:
		switch (j->baseframe.low) {
		case 0xA0:
			j->play_frame_size = 80;
			break;
		case 0x50:
			j->play_frame_size = 40;
			break;
		default:
			j->play_frame_size = 120;
			break;
		}
		j->play_mode = 4;
		break;
	case WSS:
		switch (j->baseframe.low) {
		case 0xA0:
			j->play_frame_size = 80;
			break;
		case 0x50:
			j->play_frame_size = 40;
			break;
		default:
			j->play_frame_size = 120;
			break;
		}
		j->play_mode = 5;
		break;
	default:
		kfree(j->write_buffer);
		j->play_frame_size = 0;
		j->play_mode = -1;
		j->write_buffer = NULL;
		j->write_buffer_size = 0;
		retval = 1;
		break;
	}
	return retval;
}

static int ixj_play_start(IXJ *j)
{
	unsigned short cmd = 0x0000;

	if (j->write_buffer) {
		ixj_play_stop(j);
	}

	if(ixjdebug & 0x0002)
		printk("IXJ %d Starting Play Codec %d at %ld\n", j->board, j->play_codec, jiffies);

	j->flags.playing = 1;
	ixj_WriteDSPCommand(0x0FE0, j);	

	j->flags.play_first_frame = 1;
	j->drybuffer = 0;

	if (!j->play_mode) {
		switch (j->play_codec) {
		case G723_63:
			cmd = 0x5231;
			break;
		case G723_53:
			cmd = 0x5232;
			break;
		case TS85:
			cmd = 0x5230;	

			break;
		case TS48:
			cmd = 0x5233;	

			break;
		case TS41:
			cmd = 0x5234;	

			break;
		case G728:
			cmd = 0x5235;
			break;
		case G729:
		case G729B:
			cmd = 0x5236;
			break;
		default:
			return 1;
		}
		if (ixj_WriteDSPCommand(cmd, j))
			return -1;
	}
	j->write_buffer = kmalloc(j->play_frame_size * 2, GFP_ATOMIC);
	if (!j->write_buffer) {
		printk("Write buffer allocation for ixj board %d failed!\n", j->board);
		return -ENOMEM;
	}
	j->write_buffers_empty = 1; 
	j->write_buffer_size = j->play_frame_size * 2;
	j->write_buffer_end = j->write_buffer + j->play_frame_size * 2;
	j->write_buffer_rp = j->write_buffer_wp = j->write_buffer;

	if (ixj_WriteDSPCommand(0x5202, j))		

		return -1;

	switch (j->play_mode) {
	case 0:
		cmd = 0x2C03;
		break;
	case 2:
		if (j->ver.low == 0x12) {
			cmd = 0x2C23;
		} else {
			cmd = 0x2C21;
		}
		break;
	case 4:
		if (j->ver.low == 0x12) {
			cmd = 0x2C43;
		} else {
			cmd = 0x2C41;
		}
		break;
	case 5:
		if (j->ver.low == 0x12) {
			cmd = 0x2C53;
		} else {
			cmd = 0x2C51;
		}
		break;
	case 6:
		if (j->ver.low == 0x12) {
			cmd = 0x2C63;
		} else {
			cmd = 0x2C61;
		}
		break;
	}
	if (ixj_WriteDSPCommand(cmd, j))
		return -1;

	if (ixj_WriteDSPCommand(0x2000, j))		
		return -1;

	if (ixj_WriteDSPCommand(0x2000 + j->play_frame_size, j))	
		return -1;

	if (j->flags.recording) {
		ixj_aec_start(j, j->aec_level);
	}

	return 0;
}

static void ixj_play_stop(IXJ *j)
{
	if (ixjdebug & 0x0002)
		printk("IXJ %d Stopping Play Codec %d at %ld\n", j->board, j->play_codec, jiffies);

	kfree(j->write_buffer);
	j->write_buffer = NULL;
	j->write_buffer_size = 0;
	if (j->play_mode > -1) {
		ixj_WriteDSPCommand(0x5221, j);	

		j->play_mode = -1;
	}
	j->flags.playing = 0;
}

static inline int get_play_level(IXJ *j)
{
	int retval;

	ixj_WriteDSPCommand(0xCF8F, j); 
	return j->ssr.high << 8 | j->ssr.low;
	retval = j->ssr.high << 8 | j->ssr.low;
	retval = (retval * 256) / 240;
	return retval;
}

static unsigned int ixj_poll(struct file *file_p, poll_table * wait)
{
	unsigned int mask = 0;

	IXJ *j = get_ixj(NUM(file_p->f_path.dentry->d_inode));

	poll_wait(file_p, &(j->poll_q), wait);
	if (j->read_buffer_ready > 0)
		mask |= POLLIN | POLLRDNORM;	
	if (j->write_buffers_empty > 0)
		mask |= POLLOUT | POLLWRNORM;	
	if (j->ex.bytes)
		mask |= POLLPRI;
	return mask;
}

static int ixj_play_tone(IXJ *j, char tone)
{
	if (!j->tone_state) {
		if(ixjdebug & 0x0002) {
			printk("IXJ %d starting tone %d at %ld\n", j->board, tone, jiffies);
		}
		if (j->dsp.low == 0x20) {
			idle(j);
		}
		j->tone_start_jif = jiffies;

		j->tone_state = 1;
	}

	j->tone_index = tone;
	if (ixj_WriteDSPCommand(0x6000 + j->tone_index, j))
		return -1;

	return 0;
}

static int ixj_set_tone_on(unsigned short arg, IXJ *j)
{
	j->tone_on_time = arg;

	if (ixj_WriteDSPCommand(0x6E04, j))		

		return -1;

	if (ixj_WriteDSPCommand(arg, j))
		return -1;

	return 0;
}

static int SCI_WaitHighSCI(IXJ *j)
{
	int cnt;

	j->pld_scrr.byte = inb_p(j->XILINXbase);
	if (!j->pld_scrr.bits.sci) {
		for (cnt = 0; cnt < 10; cnt++) {
			udelay(32);
			j->pld_scrr.byte = inb_p(j->XILINXbase);

			if ((j->pld_scrr.bits.sci))
				return 1;
		}
		if (ixjdebug & 0x0001)
			printk(KERN_INFO "SCI Wait High failed %x\n", j->pld_scrr.byte);
		return 0;
	} else
		return 1;
}

static int SCI_WaitLowSCI(IXJ *j)
{
	int cnt;

	j->pld_scrr.byte = inb_p(j->XILINXbase);
	if (j->pld_scrr.bits.sci) {
		for (cnt = 0; cnt < 10; cnt++) {
			udelay(32);
			j->pld_scrr.byte = inb_p(j->XILINXbase);

			if (!(j->pld_scrr.bits.sci))
				return 1;
		}
		if (ixjdebug & 0x0001)
			printk(KERN_INFO "SCI Wait Low failed %x\n", j->pld_scrr.byte);
		return 0;
	} else
		return 1;
}

static int SCI_Control(IXJ *j, int control)
{
	switch (control) {
	case SCI_End:
		j->pld_scrw.bits.c0 = 0;	

		j->pld_scrw.bits.c1 = 0;	

		break;
	case SCI_Enable_DAA:
		j->pld_scrw.bits.c0 = 1;	

		j->pld_scrw.bits.c1 = 0;	

		break;
	case SCI_Enable_Mixer:
		j->pld_scrw.bits.c0 = 0;	

		j->pld_scrw.bits.c1 = 1;	

		break;
	case SCI_Enable_EEPROM:
		j->pld_scrw.bits.c0 = 1;	

		j->pld_scrw.bits.c1 = 1;	

		break;
	default:
		return 0;
		break;
	}
	outb_p(j->pld_scrw.byte, j->XILINXbase);

	switch (control) {
	case SCI_End:
		return 1;
		break;
	case SCI_Enable_DAA:
	case SCI_Enable_Mixer:
	case SCI_Enable_EEPROM:
		if (!SCI_WaitHighSCI(j))
			return 0;
		break;
	default:
		return 0;
		break;
	}
	return 1;
}

static int SCI_Prepare(IXJ *j)
{
	if (!SCI_Control(j, SCI_End))
		return 0;

	if (!SCI_WaitLowSCI(j))
		return 0;

	return 1;
}

static int ixj_get_mixer(long val, IXJ *j)
{
	int reg = (val & 0x1F00) >> 8;
        return j->mix.vol[reg];
}

static int ixj_mixer(long val, IXJ *j)
{
	BYTES bytes;

	bytes.high = (val & 0x1F00) >> 8;
	bytes.low = val & 0x00FF;

        
        j->mix.vol[bytes.high] = bytes.low;

	outb_p(bytes.high & 0x1F, j->XILINXbase + 0x03);	

	outb_p(bytes.low, j->XILINXbase + 0x02);	

	SCI_Control(j, SCI_Enable_Mixer);

	SCI_Control(j, SCI_End);

	return 0;
}

static int daa_load(BYTES * p_bytes, IXJ *j)
{
	outb_p(p_bytes->high, j->XILINXbase + 0x03);
	outb_p(p_bytes->low, j->XILINXbase + 0x02);
	if (!SCI_Control(j, SCI_Enable_DAA))
		return 0;
	else
		return 1;
}

static int ixj_daa_cr4(IXJ *j, char reg)
{
	BYTES bytes;

	switch (j->daa_mode) {
	case SOP_PU_SLEEP:
		bytes.high = 0x14;
		break;
	case SOP_PU_RINGING:
		bytes.high = 0x54;
		break;
	case SOP_PU_CONVERSATION:
		bytes.high = 0x94;
		break;
	case SOP_PU_PULSEDIALING:
		bytes.high = 0xD4;
		break;
	}

	j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.reg = reg;

	switch (j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.bitreg.AGX) {
	case 0:
		j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.bitreg.AGR_Z = 0;
		break;
	case 1:
		j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.bitreg.AGR_Z = 2;
		break;
	case 2:
		j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.bitreg.AGR_Z = 1;
		break;
	case 3:
		j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.bitreg.AGR_Z = 3;
		break;
	}

	bytes.low = j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.reg;

	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Prepare(j))
		return 0;

	return 1;
}

static char daa_int_read(IXJ *j)
{
	BYTES bytes;

	if (!SCI_Prepare(j))
		return 0;

	bytes.high = 0x38;
	bytes.low = 0x00;
	outb_p(bytes.high, j->XILINXbase + 0x03);
	outb_p(bytes.low, j->XILINXbase + 0x02);

	if (!SCI_Control(j, SCI_Enable_DAA))
		return 0;

	bytes.high = inb_p(j->XILINXbase + 0x03);
	bytes.low = inb_p(j->XILINXbase + 0x02);
	if (bytes.low != ALISDAA_ID_BYTE) {
		if (ixjdebug & 0x0001)
			printk("Cannot read DAA ID Byte high = %d low = %d\n", bytes.high, bytes.low);
		return 0;
	}
	if (!SCI_Control(j, SCI_Enable_DAA))
		return 0;
	if (!SCI_Control(j, SCI_End))
		return 0;

	bytes.high = inb_p(j->XILINXbase + 0x03);
	bytes.low = inb_p(j->XILINXbase + 0x02);

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.reg = bytes.high;

	return 1;
}

static char daa_CR_read(IXJ *j, int cr)
{
	IXJ_WORD wdata;
	BYTES bytes;

	if (!SCI_Prepare(j))
		return 0;

	switch (j->daa_mode) {
	case SOP_PU_SLEEP:
		bytes.high = 0x30 + cr;
		break;
	case SOP_PU_RINGING:
		bytes.high = 0x70 + cr;
		break;
	case SOP_PU_CONVERSATION:
		bytes.high = 0xB0 + cr;
		break;
	case SOP_PU_PULSEDIALING:
	default:
		bytes.high = 0xF0 + cr;
		break;
	}

	bytes.low = 0x00;

	outb_p(bytes.high, j->XILINXbase + 0x03);
	outb_p(bytes.low, j->XILINXbase + 0x02);

	if (!SCI_Control(j, SCI_Enable_DAA))
		return 0;

	bytes.high = inb_p(j->XILINXbase + 0x03);
	bytes.low = inb_p(j->XILINXbase + 0x02);
	if (bytes.low != ALISDAA_ID_BYTE) {
		if (ixjdebug & 0x0001)
			printk("Cannot read DAA ID Byte high = %d low = %d\n", bytes.high, bytes.low);
		return 0;
	}
	if (!SCI_Control(j, SCI_Enable_DAA))
		return 0;
	if (!SCI_Control(j, SCI_End))
		return 0;

	wdata.word = inw_p(j->XILINXbase + 0x02);

	switch(cr){
		case 5:
			j->m_DAAShadowRegs.SOP_REGS.SOP.cr5.reg = wdata.bytes.high;
			break;
		case 4:
			j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.reg = wdata.bytes.high;
			break;
		case 3:
			j->m_DAAShadowRegs.SOP_REGS.SOP.cr3.reg = wdata.bytes.high;
			break;
		case 2:
			j->m_DAAShadowRegs.SOP_REGS.SOP.cr2.reg = wdata.bytes.high;
			break;
		case 1:
			j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.reg = wdata.bytes.high;
			break;
		case 0:
			j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg = wdata.bytes.high;
			break;
		default:
			return 0;
	}
	return 1;
}

static int ixj_daa_cid_reset(IXJ *j)
{
	int i;
	BYTES bytes;

	if (ixjdebug & 0x0002)
		printk("DAA Clearing CID ram\n");

	if (!SCI_Prepare(j))
		return 0;

	bytes.high = 0x58;
	bytes.low = 0x00;
	outb_p(bytes.high, j->XILINXbase + 0x03);
	outb_p(bytes.low, j->XILINXbase + 0x02);

	if (!SCI_Control(j, SCI_Enable_DAA))
		return 0;

	if (!SCI_WaitHighSCI(j))
		return 0;

	for (i = 0; i < ALISDAA_CALLERID_SIZE - 1; i += 2) {
		bytes.high = bytes.low = 0x00;
		outb_p(bytes.high, j->XILINXbase + 0x03);

		if (i < ALISDAA_CALLERID_SIZE - 1)
			outb_p(bytes.low, j->XILINXbase + 0x02);

		if (!SCI_Control(j, SCI_Enable_DAA))
			return 0;

		if (!SCI_WaitHighSCI(j))
			return 0;

	}

	if (!SCI_Control(j, SCI_End))
		return 0;

	if (ixjdebug & 0x0002)
		printk("DAA CID ram cleared\n");

	return 1;
}

static int ixj_daa_cid_read(IXJ *j)
{
	int i;
	BYTES bytes;
	char CID[ALISDAA_CALLERID_SIZE];
	bool mContinue;
	char *pIn, *pOut;

	if (!SCI_Prepare(j))
		return 0;

	bytes.high = 0x78;
	bytes.low = 0x00;
	outb_p(bytes.high, j->XILINXbase + 0x03);
	outb_p(bytes.low, j->XILINXbase + 0x02);

	if (!SCI_Control(j, SCI_Enable_DAA))
		return 0;

	if (!SCI_WaitHighSCI(j))
		return 0;

	bytes.high = inb_p(j->XILINXbase + 0x03);
	bytes.low = inb_p(j->XILINXbase + 0x02);
	if (bytes.low != ALISDAA_ID_BYTE) {
		if (ixjdebug & 0x0001)
			printk("DAA Get Version Cannot read DAA ID Byte high = %d low = %d\n", bytes.high, bytes.low);
		return 0;
	}
	for (i = 0; i < ALISDAA_CALLERID_SIZE; i += 2) {
		bytes.high = bytes.low = 0x00;
		outb_p(bytes.high, j->XILINXbase + 0x03);
		outb_p(bytes.low, j->XILINXbase + 0x02);

		if (!SCI_Control(j, SCI_Enable_DAA))
			return 0;

		if (!SCI_WaitHighSCI(j))
			return 0;

		CID[i + 0] = inb_p(j->XILINXbase + 0x03);
		CID[i + 1] = inb_p(j->XILINXbase + 0x02);
	}

	if (!SCI_Control(j, SCI_End))
		return 0;

	pIn = CID;
	pOut = j->m_DAAShadowRegs.CAO_REGS.CAO.CallerID;
	mContinue = true;
	while (mContinue) {
		if ((pIn[1] & 0x03) == 0x01) {
			pOut[0] = pIn[0];
		}
		if ((pIn[2] & 0x0c) == 0x04) {
			pOut[1] = ((pIn[2] & 0x03) << 6) | ((pIn[1] & 0xfc) >> 2);
		}
		if ((pIn[3] & 0x30) == 0x10) {
			pOut[2] = ((pIn[3] & 0x0f) << 4) | ((pIn[2] & 0xf0) >> 4);
		}
		if ((pIn[4] & 0xc0) == 0x40) {
			pOut[3] = ((pIn[4] & 0x3f) << 2) | ((pIn[3] & 0xc0) >> 6);
		} else {
			mContinue = false;
		}
		pIn += 5, pOut += 4;
	}
	memset(&j->cid, 0, sizeof(PHONE_CID));
	pOut = j->m_DAAShadowRegs.CAO_REGS.CAO.CallerID;
	pOut += 4;
	strncpy(j->cid.month, pOut, 2);
	pOut += 2;
	strncpy(j->cid.day, pOut, 2);
	pOut += 2;
	strncpy(j->cid.hour, pOut, 2);
	pOut += 2;
	strncpy(j->cid.min, pOut, 2);
	pOut += 3;
	j->cid.numlen = *pOut;
	pOut += 1;
	strncpy(j->cid.number, pOut, j->cid.numlen);
	pOut += j->cid.numlen + 1;
	j->cid.namelen = *pOut;
	pOut += 1;
	strncpy(j->cid.name, pOut, j->cid.namelen);

	ixj_daa_cid_reset(j);
	return 1;
}

static char daa_get_version(IXJ *j)
{
	BYTES bytes;

	if (!SCI_Prepare(j))
		return 0;

	bytes.high = 0x35;
	bytes.low = 0x00;
	outb_p(bytes.high, j->XILINXbase + 0x03);
	outb_p(bytes.low, j->XILINXbase + 0x02);

	if (!SCI_Control(j, SCI_Enable_DAA))
		return 0;

	bytes.high = inb_p(j->XILINXbase + 0x03);
	bytes.low = inb_p(j->XILINXbase + 0x02);
	if (bytes.low != ALISDAA_ID_BYTE) {
		if (ixjdebug & 0x0001)
			printk("DAA Get Version Cannot read DAA ID Byte high = %d low = %d\n", bytes.high, bytes.low);
		return 0;
	}
	if (!SCI_Control(j, SCI_Enable_DAA))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;

	bytes.high = inb_p(j->XILINXbase + 0x03);
	bytes.low = inb_p(j->XILINXbase + 0x02);
	if (ixjdebug & 0x0002)
		printk("DAA CR5 Byte high = 0x%x low = 0x%x\n", bytes.high, bytes.low);
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr5.reg = bytes.high;
	return bytes.high;
}

static int daa_set_mode(IXJ *j, int mode)
{

	BYTES bytes;

	j->flags.pstn_rmr = 0;

	if (!SCI_Prepare(j))
		return 0;

	switch (mode) {
	case SOP_PU_RESET:
		j->pld_scrw.bits.daafsyncen = 0;	

		outb_p(j->pld_scrw.byte, j->XILINXbase);
		j->pld_slicw.bits.rly2 = 0;
		outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
		bytes.high = 0x10;
		bytes.low = j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg;
		daa_load(&bytes, j);
		if (!SCI_Prepare(j))
			return 0;

		j->daa_mode = SOP_PU_SLEEP;
		break;
	case SOP_PU_SLEEP:
		if(j->daa_mode == SOP_PU_SLEEP)
		{
			break;
		}
		if (ixjdebug & 0x0008)
			printk(KERN_INFO "phone DAA: SOP_PU_SLEEP at %ld\n", jiffies);
		{
			j->pld_scrw.bits.daafsyncen = 0;	

			outb_p(j->pld_scrw.byte, j->XILINXbase);
			j->pld_slicw.bits.rly2 = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
			bytes.high = 0x10;
			bytes.low = j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg;
			daa_load(&bytes, j);
			if (!SCI_Prepare(j))
				return 0;
		}
		j->pld_scrw.bits.daafsyncen = 0;	

		outb_p(j->pld_scrw.byte, j->XILINXbase);
		j->pld_slicw.bits.rly2 = 0;
		outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
		bytes.high = 0x10;
		bytes.low = j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg;
		daa_load(&bytes, j);
		if (!SCI_Prepare(j))
			return 0;

		j->daa_mode = SOP_PU_SLEEP;
		j->flags.pstn_ringing = 0;
		j->ex.bits.pstn_ring = 0;
		j->pstn_sleeptil = jiffies + (hertz / 4);
		wake_up_interruptible(&j->read_q);      
		wake_up_interruptible(&j->write_q);     
		wake_up_interruptible(&j->poll_q);      
 		break;
	case SOP_PU_RINGING:
		if (ixjdebug & 0x0008)
			printk(KERN_INFO "phone DAA: SOP_PU_RINGING at %ld\n", jiffies);
		j->pld_scrw.bits.daafsyncen = 0;	

		outb_p(j->pld_scrw.byte, j->XILINXbase);
		j->pld_slicw.bits.rly2 = 0;
		outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
		bytes.high = 0x50;
		bytes.low = j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg;
		daa_load(&bytes, j);
		if (!SCI_Prepare(j))
			return 0;
		j->daa_mode = SOP_PU_RINGING;
		break;
	case SOP_PU_CONVERSATION:
		if (ixjdebug & 0x0008)
			printk(KERN_INFO "phone DAA: SOP_PU_CONVERSATION at %ld\n", jiffies);
		bytes.high = 0x90;
		bytes.low = j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg;
		daa_load(&bytes, j);
		if (!SCI_Prepare(j))
			return 0;
		j->pld_slicw.bits.rly2 = 1;
		outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
		j->pld_scrw.bits.daafsyncen = 1;	

		outb_p(j->pld_scrw.byte, j->XILINXbase);
		j->daa_mode = SOP_PU_CONVERSATION;
		j->flags.pstn_ringing = 0;
		j->ex.bits.pstn_ring = 0;
		j->pstn_sleeptil = jiffies;
		j->pstn_ring_start = j->pstn_ring_stop = j->pstn_ring_int = 0;
		break;
	case SOP_PU_PULSEDIALING:
		if (ixjdebug & 0x0008)
			printk(KERN_INFO "phone DAA: SOP_PU_PULSEDIALING at %ld\n", jiffies);
		j->pld_scrw.bits.daafsyncen = 0;	

		outb_p(j->pld_scrw.byte, j->XILINXbase);
		j->pld_slicw.bits.rly2 = 0;
		outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
		bytes.high = 0xD0;
		bytes.low = j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg;
		daa_load(&bytes, j);
		if (!SCI_Prepare(j))
			return 0;
		j->daa_mode = SOP_PU_PULSEDIALING;
		break;
	default:
		break;
	}
	return 1;
}

static int ixj_daa_write(IXJ *j)
{
	BYTES bytes;

	j->flags.pstncheck = 1;

	daa_set_mode(j, SOP_PU_SLEEP);

	if (!SCI_Prepare(j))
		return 0;

	outb_p(j->pld_scrw.byte, j->XILINXbase);

	bytes.high = 0x14;
	bytes.low = j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.reg;
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.SOP_REGS.SOP.cr3.reg;
	bytes.low = j->m_DAAShadowRegs.SOP_REGS.SOP.cr2.reg;
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.reg;
	bytes.low = j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Prepare(j))
		return 0;

	bytes.high = 0x1F;
	bytes.low = j->m_DAAShadowRegs.XOP_REGS.XOP.xr7.reg;
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.XOP_xr6_W.reg;
	bytes.low = j->m_DAAShadowRegs.XOP_REGS.XOP.xr5.reg;
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.XOP_REGS.XOP.xr4.reg;
	bytes.low = j->m_DAAShadowRegs.XOP_REGS.XOP.xr3.reg;
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.XOP_REGS.XOP.xr2.reg;
	bytes.low = j->m_DAAShadowRegs.XOP_REGS.XOP.xr1.reg;
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.XOP_xr0_W.reg;
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Prepare(j))
		return 0;

	bytes.high = 0x00;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[7];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[6];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[5];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[4];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x01;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[7];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[6];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[5];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[4];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x02;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[7];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[6];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[5];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[4];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x03;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[7];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[6];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[5];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[4];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x04;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[7];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[6];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[5];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[4];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x05;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[7];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[6];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[5];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[4];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x06;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[7];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[6];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[5];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[4];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x07;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[7];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[6];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[5];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[4];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x08;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[7];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[6];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[5];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[4];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x09;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x0A;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x0B;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x0C;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x0D;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x0E;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[7];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[6];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[5];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[4];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	if (!SCI_Control(j, SCI_End))
		return 0;
	if (!SCI_WaitLowSCI(j))
		return 0;

	bytes.high = 0x0F;
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[7];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[6];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[5];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[4];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[3];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[2];
	bytes.low = j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[1];
	if (!daa_load(&bytes, j))
		return 0;

	bytes.high = j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[0];
	bytes.low = 0x00;
	if (!daa_load(&bytes, j))
		return 0;

	udelay(32);
	j->pld_scrr.byte = inb_p(j->XILINXbase);
	if (!SCI_Control(j, SCI_End))
		return 0;

	outb_p(j->pld_scrw.byte, j->XILINXbase);

	if (ixjdebug & 0x0002)
		printk("DAA Coefficients Loaded\n");

	j->flags.pstncheck = 0;
	return 1;
}

static int ixj_set_tone_off(unsigned short arg, IXJ *j)
{
	j->tone_off_time = arg;
	if (ixj_WriteDSPCommand(0x6E05, j))		

		return -1;
	if (ixj_WriteDSPCommand(arg, j))
		return -1;
	return 0;
}

static int ixj_get_tone_on(IXJ *j)
{
	if (ixj_WriteDSPCommand(0x6E06, j))		

		return -1;
	return 0;
}

static int ixj_get_tone_off(IXJ *j)
{
	if (ixj_WriteDSPCommand(0x6E07, j))		

		return -1;
	return 0;
}

static void ixj_busytone(IXJ *j)
{
	j->flags.ringback = 0;
	j->flags.dialtone = 0;
	j->flags.busytone = 1;
	ixj_set_tone_on(0x07D0, j);
	ixj_set_tone_off(0x07D0, j);
	ixj_play_tone(j, 27);
}

static void ixj_dialtone(IXJ *j)
{
	j->flags.ringback = 0;
	j->flags.dialtone = 1;
	j->flags.busytone = 0;
	if (j->dsp.low == 0x20) {
		return;
	} else {
		ixj_set_tone_on(0xFFFF, j);
		ixj_set_tone_off(0x0000, j);
		ixj_play_tone(j, 25);
	}
}

static void ixj_cpt_stop(IXJ *j)
{
	if(j->tone_state || j->tone_cadence_state)
	{
		j->flags.dialtone = 0;
		j->flags.busytone = 0;
		j->flags.ringback = 0;
		ixj_set_tone_on(0x0001, j);
		ixj_set_tone_off(0x0000, j);
		ixj_play_tone(j, 0);
		j->tone_state = j->tone_cadence_state = 0;
		if (j->cadence_t) {
			kfree(j->cadence_t->ce);
			kfree(j->cadence_t);
			j->cadence_t = NULL;
		}
	}
	if (j->play_mode == -1 && j->rec_mode == -1)
		idle(j);
	if (j->play_mode != -1 && j->dsp.low == 0x20)
		ixj_play_start(j);
	if (j->rec_mode != -1 && j->dsp.low == 0x20)
		ixj_record_start(j);
}

static void ixj_ringback(IXJ *j)
{
	j->flags.busytone = 0;
	j->flags.dialtone = 0;
	j->flags.ringback = 1;
	ixj_set_tone_on(0x0FA0, j);
	ixj_set_tone_off(0x2EE0, j);
	ixj_play_tone(j, 26);
}

static void ixj_testram(IXJ *j)
{
	ixj_WriteDSPCommand(0x3001, j);	
}

static int ixj_build_cadence(IXJ *j, IXJ_CADENCE __user * cp)
{
	ixj_cadence *lcp;
	IXJ_CADENCE_ELEMENT __user *cep;
	IXJ_CADENCE_ELEMENT *lcep;
	IXJ_TONE ti;
	int err;

	lcp = kmalloc(sizeof(ixj_cadence), GFP_KERNEL);
	if (lcp == NULL)
		return -ENOMEM;

	err = -EFAULT;
	if (copy_from_user(&lcp->elements_used,
			   &cp->elements_used, sizeof(int)))
		goto out;
	if (copy_from_user(&lcp->termination,
			   &cp->termination, sizeof(IXJ_CADENCE_TERM)))
		goto out;
	if (get_user(cep, &cp->ce))
		goto out;

	err = -EINVAL;
	if ((unsigned)lcp->elements_used >= ~0U/sizeof(IXJ_CADENCE_ELEMENT))
		goto out;

	err = -ENOMEM;
	lcep = kmalloc(sizeof(IXJ_CADENCE_ELEMENT) * lcp->elements_used, GFP_KERNEL);
	if (!lcep)
		goto out;

	err = -EFAULT;
	if (copy_from_user(lcep, cep, sizeof(IXJ_CADENCE_ELEMENT) * lcp->elements_used))
		goto out1;

	if (j->cadence_t) {
		kfree(j->cadence_t->ce);
		kfree(j->cadence_t);
	}
	lcp->ce = (void *) lcep;
	j->cadence_t = lcp;
	j->tone_cadence_state = 0;
	ixj_set_tone_on(lcp->ce[0].tone_on_time, j);
	ixj_set_tone_off(lcp->ce[0].tone_off_time, j);
	if (j->cadence_t->ce[j->tone_cadence_state].freq0) {
		ti.tone_index = j->cadence_t->ce[j->tone_cadence_state].index;
		ti.freq0 = j->cadence_t->ce[j->tone_cadence_state].freq0;
		ti.gain0 = j->cadence_t->ce[j->tone_cadence_state].gain0;
		ti.freq1 = j->cadence_t->ce[j->tone_cadence_state].freq1;
		ti.gain1 = j->cadence_t->ce[j->tone_cadence_state].gain1;
		ixj_init_tone(j, &ti);
	}
	ixj_play_tone(j, lcp->ce[0].index);
	return 1;
out1:
	kfree(lcep);
out:
	kfree(lcp);
	return err;
}

static int ixj_build_filter_cadence(IXJ *j, IXJ_FILTER_CADENCE __user * cp)
{
	IXJ_FILTER_CADENCE *lcp;
	lcp = memdup_user(cp, sizeof(IXJ_FILTER_CADENCE));
	if (IS_ERR(lcp)) {
		if(ixjdebug & 0x0001) {
			printk(KERN_INFO "Could not allocate memory for cadence or could not copy cadence to kernel\n");
		}
		return PTR_ERR(lcp);
        }
	if (lcp->filter > 5) {
		if(ixjdebug & 0x0001) {
			printk(KERN_INFO "Cadence out of range\n");
		}
		kfree(lcp);
		return -1;
	}
	j->cadence_f[lcp->filter].state = 0;
	j->cadence_f[lcp->filter].enable = lcp->enable;
	j->filter_en[lcp->filter] = j->cadence_f[lcp->filter].en_filter = lcp->en_filter;
	j->cadence_f[lcp->filter].on1 = lcp->on1;
	j->cadence_f[lcp->filter].on1min = 0;
	j->cadence_f[lcp->filter].on1max = 0;
	j->cadence_f[lcp->filter].off1 = lcp->off1;
	j->cadence_f[lcp->filter].off1min = 0;
	j->cadence_f[lcp->filter].off1max = 0;
	j->cadence_f[lcp->filter].on2 = lcp->on2;
	j->cadence_f[lcp->filter].on2min = 0;
	j->cadence_f[lcp->filter].on2max = 0;
	j->cadence_f[lcp->filter].off2 = lcp->off2;
	j->cadence_f[lcp->filter].off2min = 0;
	j->cadence_f[lcp->filter].off2max = 0;
	j->cadence_f[lcp->filter].on3 = lcp->on3;
	j->cadence_f[lcp->filter].on3min = 0;
	j->cadence_f[lcp->filter].on3max = 0;
	j->cadence_f[lcp->filter].off3 = lcp->off3;
	j->cadence_f[lcp->filter].off3min = 0;
	j->cadence_f[lcp->filter].off3max = 0;
	if(ixjdebug & 0x0002) {
		printk(KERN_INFO "Cadence %d loaded\n", lcp->filter);
	}
	kfree(lcp);
	return 0;
}

static void add_caps(IXJ *j)
{
	j->caps = 0;
	j->caplist[j->caps].cap = PHONE_VENDOR_QUICKNET;
	strcpy(j->caplist[j->caps].desc, "Quicknet Technologies, Inc. (www.quicknet.net)");
	j->caplist[j->caps].captype = vendor;
	j->caplist[j->caps].handle = j->caps;
	j->caps++;
	j->caplist[j->caps].captype = device;
	switch (j->cardtype) {
	case QTI_PHONEJACK:
		strcpy(j->caplist[j->caps].desc, "Quicknet Internet PhoneJACK");
		break;
	case QTI_LINEJACK:
		strcpy(j->caplist[j->caps].desc, "Quicknet Internet LineJACK");
		break;
	case QTI_PHONEJACK_LITE:
		strcpy(j->caplist[j->caps].desc, "Quicknet Internet PhoneJACK Lite");
		break;
	case QTI_PHONEJACK_PCI:
		strcpy(j->caplist[j->caps].desc, "Quicknet Internet PhoneJACK PCI");
		break;
	case QTI_PHONECARD:
		strcpy(j->caplist[j->caps].desc, "Quicknet Internet PhoneCARD");
		break;
	}
	j->caplist[j->caps].cap = j->cardtype;
	j->caplist[j->caps].handle = j->caps;
	j->caps++;
	strcpy(j->caplist[j->caps].desc, "POTS");
	j->caplist[j->caps].captype = port;
	j->caplist[j->caps].cap = pots;
	j->caplist[j->caps].handle = j->caps;
	j->caps++;

 	
	switch (j->cardtype) {
	case QTI_PHONEJACK:
	case QTI_LINEJACK:
	case QTI_PHONEJACK_PCI:
	case QTI_PHONECARD:
		strcpy(j->caplist[j->caps].desc, "SPEAKER");
		j->caplist[j->caps].captype = port;
		j->caplist[j->caps].cap = speaker;
		j->caplist[j->caps].handle = j->caps;
		j->caps++;
        default:
     		break;
	}

 	
	switch (j->cardtype) {
	case QTI_PHONEJACK:
		strcpy(j->caplist[j->caps].desc, "HANDSET");
		j->caplist[j->caps].captype = port;
		j->caplist[j->caps].cap = handset;
		j->caplist[j->caps].handle = j->caps;
		j->caps++;
		break;
        default:
     		break;
	}

 	
	switch (j->cardtype) {
	case QTI_LINEJACK:
		strcpy(j->caplist[j->caps].desc, "PSTN");
		j->caplist[j->caps].captype = port;
		j->caplist[j->caps].cap = pstn;
		j->caplist[j->caps].handle = j->caps;
		j->caps++;
		break;
        default:
     		break;
	}

	
	strcpy(j->caplist[j->caps].desc, "ULAW");
	j->caplist[j->caps].captype = codec;
	j->caplist[j->caps].cap = ULAW;
	j->caplist[j->caps].handle = j->caps;
	j->caps++;

	strcpy(j->caplist[j->caps].desc, "LINEAR 16 bit");
	j->caplist[j->caps].captype = codec;
	j->caplist[j->caps].cap = LINEAR16;
	j->caplist[j->caps].handle = j->caps;
	j->caps++;

	strcpy(j->caplist[j->caps].desc, "LINEAR 8 bit");
	j->caplist[j->caps].captype = codec;
	j->caplist[j->caps].cap = LINEAR8;
	j->caplist[j->caps].handle = j->caps;
	j->caps++;

	strcpy(j->caplist[j->caps].desc, "Windows Sound System");
	j->caplist[j->caps].captype = codec;
	j->caplist[j->caps].cap = WSS;
	j->caplist[j->caps].handle = j->caps;
	j->caps++;

	
	strcpy(j->caplist[j->caps].desc, "ALAW");
	j->caplist[j->caps].captype = codec;
	j->caplist[j->caps].cap = ALAW;
	j->caplist[j->caps].handle = j->caps;
	j->caps++;

	
	if (j->dsp.low != 0x20 || j->ver.low != 0x12) {
		strcpy(j->caplist[j->caps].desc, "G.723.1 6.3kbps");
		j->caplist[j->caps].captype = codec;
		j->caplist[j->caps].cap = G723_63;
		j->caplist[j->caps].handle = j->caps;
		j->caps++;

		strcpy(j->caplist[j->caps].desc, "G.723.1 5.3kbps");
		j->caplist[j->caps].captype = codec;
		j->caplist[j->caps].cap = G723_53;
		j->caplist[j->caps].handle = j->caps;
		j->caps++;

		strcpy(j->caplist[j->caps].desc, "TrueSpeech 4.8kbps");
		j->caplist[j->caps].captype = codec;
		j->caplist[j->caps].cap = TS48;
		j->caplist[j->caps].handle = j->caps;
		j->caps++;

		strcpy(j->caplist[j->caps].desc, "TrueSpeech 4.1kbps");
		j->caplist[j->caps].captype = codec;
		j->caplist[j->caps].cap = TS41;
		j->caplist[j->caps].handle = j->caps;
		j->caps++;
	}

	
	if (j->dsp.low == 0x20 || j->flags.ts85_loaded) {
		strcpy(j->caplist[j->caps].desc, "TrueSpeech 8.5kbps");
		j->caplist[j->caps].captype = codec;
		j->caplist[j->caps].cap = TS85;
		j->caplist[j->caps].handle = j->caps;
		j->caps++;
	}

	
	if (j->dsp.low == 0x21) {
		strcpy(j->caplist[j->caps].desc, "G.728 16kbps");
		j->caplist[j->caps].captype = codec;
		j->caplist[j->caps].cap = G728;
		j->caplist[j->caps].handle = j->caps;
		j->caps++;
	}

	
	if (j->dsp.low != 0x20 && j->flags.g729_loaded) {
		strcpy(j->caplist[j->caps].desc, "G.729A 8kbps");
		j->caplist[j->caps].captype = codec;
		j->caplist[j->caps].cap = G729;
		j->caplist[j->caps].handle = j->caps;
		j->caps++;
	}
	if (j->dsp.low != 0x20 && j->flags.g729_loaded) {
		strcpy(j->caplist[j->caps].desc, "G.729B 8kbps");
		j->caplist[j->caps].captype = codec;
		j->caplist[j->caps].cap = G729B;
		j->caplist[j->caps].handle = j->caps;
		j->caps++;
	}
}

static int capabilities_check(IXJ *j, struct phone_capability *pcreq)
{
	int cnt;
	int retval = 0;
	for (cnt = 0; cnt < j->caps; cnt++) {
		if (pcreq->captype == j->caplist[cnt].captype
		    && pcreq->cap == j->caplist[cnt].cap) {
			retval = 1;
			break;
		}
	}
	return retval;
}

static long do_ixj_ioctl(struct file *file_p, unsigned int cmd, unsigned long arg)
{
	IXJ_TONE ti;
	IXJ_FILTER jf;
	IXJ_FILTER_RAW jfr;
	void __user *argp = (void __user *)arg;
	struct inode *inode = file_p->f_path.dentry->d_inode;
	unsigned int minor = iminor(inode);
	unsigned int raise, mant;
	int board = NUM(inode);

	IXJ *j = get_ixj(NUM(inode));

	int retval = 0;

	while(test_and_set_bit(board, (void *)&j->busyflags) != 0)
		schedule_timeout_interruptible(1);
	if (ixjdebug & 0x0040)
		printk("phone%d ioctl, cmd: 0x%x, arg: 0x%lx\n", minor, cmd, arg);
	if (minor >= IXJMAX) {
		clear_bit(board, &j->busyflags);
		return -ENODEV;
	}
	if (!capable(CAP_SYS_ADMIN)) {
		switch (cmd) {
		case IXJCTL_TESTRAM:
		case IXJCTL_HZ:
			retval = -EPERM;
		}
	}
	switch (cmd) {
	case IXJCTL_TESTRAM:
		ixj_testram(j);
		retval = (j->ssr.high << 8) + j->ssr.low;
		break;
	case IXJCTL_CARDTYPE:
		retval = j->cardtype;
		break;
	case IXJCTL_SERIAL:
		retval = j->serial;
		break;
	case IXJCTL_VERSION:
		{
			char arg_str[100];
			snprintf(arg_str, sizeof(arg_str),
				"\nDriver version %i.%i.%i", IXJ_VER_MAJOR,
				IXJ_VER_MINOR, IXJ_BLD_VER);
			if (copy_to_user(argp, arg_str, strlen(arg_str)))
				retval = -EFAULT;
		}
		break;
	case PHONE_RING_CADENCE:
		j->ring_cadence = arg;
		break;
	case IXJCTL_CIDCW:
		if(arg) {
			if (copy_from_user(&j->cid_send, argp, sizeof(PHONE_CID))) {
				retval = -EFAULT;
				break;
			}
		} else {
			memset(&j->cid_send, 0, sizeof(PHONE_CID));
		}
		ixj_write_cidcw(j);
		break;
        
        case OLD_PHONE_RING_START:
                arg = 0;
                
 	case PHONE_RING_START:
		if(arg) {
			if (copy_from_user(&j->cid_send, argp, sizeof(PHONE_CID))) {
				retval = -EFAULT;
				break;
			}
			ixj_write_cid(j);
		} else {
			memset(&j->cid_send, 0, sizeof(PHONE_CID));
		}
		ixj_ring_start(j);
		break;
	case PHONE_RING_STOP:
		j->flags.cringing = 0;
		if(j->cadence_f[5].enable) {
			j->cadence_f[5].state = 0;
		}
		ixj_ring_off(j);
		break;
	case PHONE_RING:
		retval = ixj_ring(j);
		break;
	case PHONE_EXCEPTION:
		retval = j->ex.bytes;
		if(j->ex.bits.flash) {
			j->flash_end = 0;
			j->ex.bits.flash = 0;
		}
		j->ex.bits.pstn_ring = 0;
		j->ex.bits.caller_id = 0;
		j->ex.bits.pstn_wink = 0;
		j->ex.bits.f0 = 0;
		j->ex.bits.f1 = 0;
		j->ex.bits.f2 = 0;
		j->ex.bits.f3 = 0;
		j->ex.bits.fc0 = 0;
		j->ex.bits.fc1 = 0;
		j->ex.bits.fc2 = 0;
		j->ex.bits.fc3 = 0;
		j->ex.bits.reserved = 0;
		break;
	case PHONE_HOOKSTATE:
		j->ex.bits.hookstate = 0;
		retval = j->hookstate;  
		break;
	case IXJCTL_SET_LED:
		LED_SetState(arg, j);
		break;
	case PHONE_FRAME:
		retval = set_base_frame(j, arg);
		break;
	case PHONE_REC_CODEC:
		retval = set_rec_codec(j, arg);
		break;
	case PHONE_VAD:
		ixj_vad(j, arg);
		break;
	case PHONE_REC_START:
		ixj_record_start(j);
		break;
	case PHONE_REC_STOP:
		ixj_record_stop(j);
		break;
	case PHONE_REC_DEPTH:
		set_rec_depth(j, arg);
		break;
	case PHONE_REC_VOLUME:
		if(arg == -1) {
			retval = get_rec_volume(j);
		}
		else {
			set_rec_volume(j, arg);
			retval = arg;
		}
		break;
	case PHONE_REC_VOLUME_LINEAR:
		if(arg == -1) {
			retval = get_rec_volume_linear(j);
		}
		else {
			set_rec_volume_linear(j, arg);
			retval = arg;
		}
		break;
	case IXJCTL_DTMF_PRESCALE:
		if(arg == -1) {
			retval = get_dtmf_prescale(j);
		}
		else {
			set_dtmf_prescale(j, arg);
			retval = arg;
		}
		break;
	case PHONE_REC_LEVEL:
		retval = get_rec_level(j);
		break;
	case IXJCTL_SC_RXG:
		retval = ixj_siadc(j, arg);
		break;
	case IXJCTL_SC_TXG:
		retval = ixj_sidac(j, arg);
		break;
	case IXJCTL_AEC_START:
		ixj_aec_start(j, arg);
		break;
	case IXJCTL_AEC_STOP:
		aec_stop(j);
		break;
	case IXJCTL_AEC_GET_LEVEL:
		retval = j->aec_level;
		break;
	case PHONE_PLAY_CODEC:
		retval = set_play_codec(j, arg);
		break;
	case PHONE_PLAY_START:
		retval = ixj_play_start(j);
		break;
	case PHONE_PLAY_STOP:
		ixj_play_stop(j);
		break;
	case PHONE_PLAY_DEPTH:
		set_play_depth(j, arg);
		break;
	case PHONE_PLAY_VOLUME:
		if(arg == -1) {
			retval = get_play_volume(j);
		}
		else {
			set_play_volume(j, arg);
			retval = arg;
		}
		break;
	case PHONE_PLAY_VOLUME_LINEAR:
		if(arg == -1) {
			retval = get_play_volume_linear(j);
		}
		else {
			set_play_volume_linear(j, arg);
			retval = arg;
		}
		break;
	case PHONE_PLAY_LEVEL:
		retval = get_play_level(j);
		break;
	case IXJCTL_DSP_TYPE:
		retval = (j->dsp.high << 8) + j->dsp.low;
		break;
	case IXJCTL_DSP_VERSION:
		retval = (j->ver.high << 8) + j->ver.low;
		break;
	case IXJCTL_HZ:
		hertz = arg;
		break;
	case IXJCTL_RATE:
		if (arg > hertz)
			retval = -1;
		else
			samplerate = arg;
		break;
	case IXJCTL_DRYBUFFER_READ:
		put_user(j->drybuffer, (unsigned long __user *) argp);
		break;
	case IXJCTL_DRYBUFFER_CLEAR:
		j->drybuffer = 0;
		break;
	case IXJCTL_FRAMES_READ:
		put_user(j->framesread, (unsigned long __user *) argp);
		break;
	case IXJCTL_FRAMES_WRITTEN:
		put_user(j->frameswritten, (unsigned long __user *) argp);
		break;
	case IXJCTL_READ_WAIT:
		put_user(j->read_wait, (unsigned long __user *) argp);
		break;
	case IXJCTL_WRITE_WAIT:
		put_user(j->write_wait, (unsigned long __user *) argp);
		break;
	case PHONE_MAXRINGS:
		j->maxrings = arg;
		break;
	case PHONE_SET_TONE_ON_TIME:
		ixj_set_tone_on(arg, j);
		break;
	case PHONE_SET_TONE_OFF_TIME:
		ixj_set_tone_off(arg, j);
		break;
	case PHONE_GET_TONE_ON_TIME:
		if (ixj_get_tone_on(j)) {
			retval = -1;
		} else {
			retval = (j->ssr.high << 8) + j->ssr.low;
		}
		break;
	case PHONE_GET_TONE_OFF_TIME:
		if (ixj_get_tone_off(j)) {
			retval = -1;
		} else {
			retval = (j->ssr.high << 8) + j->ssr.low;
		}
		break;
	case PHONE_PLAY_TONE:
		if (!j->tone_state)
			retval = ixj_play_tone(j, arg);
		else
			retval = -1;
		break;
	case PHONE_GET_TONE_STATE:
		retval = j->tone_state;
		break;
	case PHONE_DTMF_READY:
		retval = j->ex.bits.dtmf_ready;
		break;
	case PHONE_GET_DTMF:
		if (ixj_hookstate(j)) {
			if (j->dtmf_rp != j->dtmf_wp) {
				retval = j->dtmfbuffer[j->dtmf_rp];
				j->dtmf_rp++;
				if (j->dtmf_rp == 79)
					j->dtmf_rp = 0;
				if (j->dtmf_rp == j->dtmf_wp) {
					j->ex.bits.dtmf_ready = j->dtmf_rp = j->dtmf_wp = 0;
				}
			}
		}
		break;
	case PHONE_GET_DTMF_ASCII:
		if (ixj_hookstate(j)) {
			if (j->dtmf_rp != j->dtmf_wp) {
				switch (j->dtmfbuffer[j->dtmf_rp]) {
				case 10:
					retval = 42;	

					break;
				case 11:
					retval = 48;	

					break;
				case 12:
					retval = 35;	

					break;
				case 28:
					retval = 65;	

					break;
				case 29:
					retval = 66;	

					break;
				case 30:
					retval = 67;	

					break;
				case 31:
					retval = 68;	

					break;
				default:
					retval = 48 + j->dtmfbuffer[j->dtmf_rp];
					break;
				}
				j->dtmf_rp++;
				if (j->dtmf_rp == 79)
					j->dtmf_rp = 0;
				if(j->dtmf_rp == j->dtmf_wp)
				{
					j->ex.bits.dtmf_ready = j->dtmf_rp = j->dtmf_wp = 0;
				}
			}
		}
		break;
	case PHONE_DTMF_OOB:
		j->flags.dtmf_oob = arg;
		break;
	case PHONE_DIALTONE:
		ixj_dialtone(j);
		break;
	case PHONE_BUSY:
		ixj_busytone(j);
		break;
	case PHONE_RINGBACK:
		ixj_ringback(j);
		break;
	case PHONE_WINK:
		if(j->cardtype == QTI_PHONEJACK) 
			retval = -1;
		else 
			retval = ixj_wink(j);
		break;
	case PHONE_CPT_STOP:
		ixj_cpt_stop(j);
		break;
        case PHONE_QUERY_CODEC:
        {
                struct phone_codec_data pd;
                int val;
                int proto_size[] = {
                        -1,
                        12, 10, 16, 9, 8, 48, 5,
                        40, 40, 80, 40, 40, 6
                };
                if(copy_from_user(&pd, argp, sizeof(pd))) {
                        retval = -EFAULT;
			break;
		}
                if(pd.type<1 || pd.type>13) {
                        retval = -EPROTONOSUPPORT;
			break;
		}
                if(pd.type<G729)
                        val=proto_size[pd.type];
                else switch(j->baseframe.low)
                {
                        case 0xA0:val=2*proto_size[pd.type];break;
                        case 0x50:val=proto_size[pd.type];break;
                        default:val=proto_size[pd.type]*3;break;
                }
                pd.buf_min=pd.buf_max=pd.buf_opt=val;
                if(copy_to_user(argp, &pd, sizeof(pd)))
                        retval = -EFAULT;
        	break;
        }
	case IXJCTL_DSP_IDLE:
		idle(j);
		break;
	case IXJCTL_MIXER:
                if ((arg & 0xff) == 0xff)
			retval = ixj_get_mixer(arg, j);
                else
			ixj_mixer(arg, j);
		break;
	case IXJCTL_DAA_COEFF_SET:
		switch (arg) {
		case DAA_US:
			DAA_Coeff_US(j);
			retval = ixj_daa_write(j);
			break;
		case DAA_UK:
			DAA_Coeff_UK(j);
			retval = ixj_daa_write(j);
			break;
		case DAA_FRANCE:
			DAA_Coeff_France(j);
			retval = ixj_daa_write(j);
			break;
		case DAA_GERMANY:
			DAA_Coeff_Germany(j);
			retval = ixj_daa_write(j);
			break;
		case DAA_AUSTRALIA:
			DAA_Coeff_Australia(j);
			retval = ixj_daa_write(j);
			break;
		case DAA_JAPAN:
			DAA_Coeff_Japan(j);
			retval = ixj_daa_write(j);
			break;
		default:
			retval = 1;
			break;
		}
		break;
	case IXJCTL_DAA_AGAIN:
		ixj_daa_cr4(j, arg | 0x02);
		break;
	case IXJCTL_PSTN_LINETEST:
		retval = ixj_linetest(j);
		break;
	case IXJCTL_VMWI:
		ixj_write_vmwi(j, arg);
		break;
	case IXJCTL_CID:
		if (copy_to_user(argp, &j->cid, sizeof(PHONE_CID))) 
			retval = -EFAULT;
		j->ex.bits.caller_id = 0;
		break;
	case IXJCTL_WINK_DURATION:
		j->winktime = arg;
		break;
	case IXJCTL_PORT:
		if (arg)
			retval = ixj_set_port(j, arg);
		else
			retval = j->port;
		break;
	case IXJCTL_POTS_PSTN:
		retval = ixj_set_pots(j, arg);
		break;
	case PHONE_CAPABILITIES:
		add_caps(j);
		retval = j->caps;
		break;
	case PHONE_CAPABILITIES_LIST:
		add_caps(j);
		if (copy_to_user(argp, j->caplist, sizeof(struct phone_capability) * j->caps)) 
			retval = -EFAULT;
		break;
	case PHONE_CAPABILITIES_CHECK:
		{
			struct phone_capability cap;
			if (copy_from_user(&cap, argp, sizeof(cap))) 
				retval = -EFAULT;
			else {
				add_caps(j);
				retval = capabilities_check(j, &cap);
			}
		}
		break;
	case PHONE_PSTN_SET_STATE:
		daa_set_mode(j, arg);
		break;
	case PHONE_PSTN_GET_STATE:
		retval = j->daa_mode;
		j->ex.bits.pstn_ring = 0;
		break;
	case IXJCTL_SET_FILTER:
		if (copy_from_user(&jf, argp, sizeof(jf))) 
			retval = -EFAULT;
		else
			retval = ixj_init_filter(j, &jf);
		break;
	case IXJCTL_SET_FILTER_RAW:
		if (copy_from_user(&jfr, argp, sizeof(jfr))) 
			retval = -EFAULT;
		else
			retval = ixj_init_filter_raw(j, &jfr);
		break;
	case IXJCTL_GET_FILTER_HIST:
		if(arg<0||arg>3)
			retval = -EINVAL;
		else
			retval = j->filter_hist[arg];
		break;
	case IXJCTL_INIT_TONE:
		if (copy_from_user(&ti, argp, sizeof(ti)))
			retval = -EFAULT;
		else
			retval = ixj_init_tone(j, &ti);
		break;
	case IXJCTL_TONE_CADENCE:
		retval = ixj_build_cadence(j, argp);
		break;
	case IXJCTL_FILTER_CADENCE:
		retval = ixj_build_filter_cadence(j, argp);
		break;
	case IXJCTL_SIGCTL:
		if (copy_from_user(&j->sigdef, argp, sizeof(IXJ_SIGDEF))) {
			retval = -EFAULT;
			break;
		}
		j->ixj_signals[j->sigdef.event] = j->sigdef.signal;
		if(j->sigdef.event < 33) {
			raise = 1;
			for(mant = 0; mant < j->sigdef.event; mant++){
				raise *= 2;
			}
			if(j->sigdef.signal)
				j->ex_sig.bytes |= raise; 
			else
				j->ex_sig.bytes &= (raise^0xffff); 
		}
		break;
	case IXJCTL_INTERCOM_STOP:
		if(arg < 0 || arg >= IXJMAX)
			return -EINVAL;
		j->intercom = -1;
		ixj_record_stop(j);
		ixj_play_stop(j);
		idle(j);
		get_ixj(arg)->intercom = -1;
		ixj_record_stop(get_ixj(arg));
		ixj_play_stop(get_ixj(arg));
		idle(get_ixj(arg));
		break;
	case IXJCTL_INTERCOM_START:
		if(arg < 0 || arg >= IXJMAX)
			return -EINVAL;
		j->intercom = arg;
		ixj_record_start(j);
		ixj_play_start(j);
		get_ixj(arg)->intercom = board;
		ixj_play_start(get_ixj(arg));
		ixj_record_start(get_ixj(arg));
		break;
	}
	if (ixjdebug & 0x0040)
		printk("phone%d ioctl end, cmd: 0x%x, arg: 0x%lx\n", minor, cmd, arg);
	clear_bit(board, &j->busyflags);
	return retval;
}

static long ixj_ioctl(struct file *file_p, unsigned int cmd, unsigned long arg)
{
	long ret;
	mutex_lock(&ixj_mutex);
	ret = do_ixj_ioctl(file_p, cmd, arg);
	mutex_unlock(&ixj_mutex);
	return ret;
}

static int ixj_fasync(int fd, struct file *file_p, int mode)
{
	IXJ *j = get_ixj(NUM(file_p->f_path.dentry->d_inode));

	return fasync_helper(fd, file_p, mode, &j->async_queue);
}

static const struct file_operations ixj_fops =
{
        .owner          = THIS_MODULE,
        .read           = ixj_enhanced_read,
        .write          = ixj_enhanced_write,
        .poll           = ixj_poll,
        .unlocked_ioctl = ixj_ioctl,
        .release        = ixj_release,
        .fasync         = ixj_fasync,
        .llseek	 = default_llseek,
};

static int ixj_linetest(IXJ *j)
{
	j->flags.pstncheck = 1;	
	j->flags.pstn_present = 0; 

	daa_int_read(j);	
	
	
	

	j->pld_slicw.bits.rly1 = 0;
	j->pld_slicw.bits.rly2 = 0;
	j->pld_slicw.bits.rly3 = 0;
	outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
	j->pld_scrw.bits.daafsyncen = 0;	

	outb_p(j->pld_scrw.byte, j->XILINXbase);
	j->pld_slicr.byte = inb_p(j->XILINXbase + 0x01);
	if (j->pld_slicr.bits.potspstn) {
		j->flags.pots_pstn = 1;
		j->flags.pots_correct = 0;
		LED_SetState(0x4, j);
	} else {
		j->flags.pots_pstn = 0;
		j->pld_slicw.bits.rly1 = 0;
		j->pld_slicw.bits.rly2 = 0;
		j->pld_slicw.bits.rly3 = 1;
		outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
		j->pld_scrw.bits.daafsyncen = 0;	

		outb_p(j->pld_scrw.byte, j->XILINXbase);
		daa_set_mode(j, SOP_PU_CONVERSATION);
		msleep(1000);
		daa_int_read(j);
		daa_set_mode(j, SOP_PU_RESET);
		if (j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK) {
			j->flags.pots_correct = 0;	
			LED_SetState(0x4, j);
			j->pld_slicw.bits.rly3 = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
		} else {
			j->flags.pots_correct = 1;
			LED_SetState(0x8, j);
			j->pld_slicw.bits.rly1 = 1;
			j->pld_slicw.bits.rly2 = 0;
			j->pld_slicw.bits.rly3 = 0;
			outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
		}
	}
	j->pld_slicw.bits.rly3 = 0;
	outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
	daa_set_mode(j, SOP_PU_CONVERSATION);
	msleep(1000);
	daa_int_read(j);
	daa_set_mode(j, SOP_PU_RESET);
	if (j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK) {
		j->pstn_sleeptil = jiffies + (hertz / 4);
		j->flags.pstn_present = 1;
	} else {
		j->flags.pstn_present = 0;
	}
	if (j->flags.pstn_present) {
		if (j->flags.pots_correct) {
			LED_SetState(0xA, j);
		} else {
			LED_SetState(0x6, j);
		}
	} else {
		if (j->flags.pots_correct) {
			LED_SetState(0x9, j);
		} else {
			LED_SetState(0x5, j);
		}
	}
	j->flags.pstncheck = 0;	
	return j->flags.pstn_present;
}

static int ixj_selfprobe(IXJ *j)
{
	unsigned short cmd;
	int cnt;
	BYTES bytes;

        init_waitqueue_head(&j->poll_q);
        init_waitqueue_head(&j->read_q);
        init_waitqueue_head(&j->write_q);

	while(atomic_read(&j->DSPWrite) > 0)
		atomic_dec(&j->DSPWrite);
	if (ixjdebug & 0x0002)
		printk(KERN_INFO "Write IDLE to Software Control Register\n");
	ixj_WriteDSPCommand(0x0FE0, j);	

	if (ixj_WriteDSPCommand(0x0000, j))		
		return -1;
	if (j->ssr.low || j->ssr.high)
		return -1;
	if (ixjdebug & 0x0002)
		printk(KERN_INFO "Get Device ID Code\n");
	if (ixj_WriteDSPCommand(0x3400, j))		
		return -1;
	j->dsp.low = j->ssr.low;
	j->dsp.high = j->ssr.high;
	if (ixjdebug & 0x0002)
		printk(KERN_INFO "Get Device Version Code\n");
	if (ixj_WriteDSPCommand(0x3800, j))		
		return -1;
	j->ver.low = j->ssr.low;
	j->ver.high = j->ssr.high;
	if (!j->cardtype) {
		if (j->dsp.low == 0x21) {
			bytes.high = bytes.low = inb_p(j->XILINXbase + 0x02);
			outb_p(bytes.low ^ 0xFF, j->XILINXbase + 0x02);
			bytes.low = inb_p(j->XILINXbase + 0x02);
			if (bytes.low == bytes.high)	
				
			 {
				j->cardtype = QTI_PHONEJACK_LITE;
				if (!request_region(j->XILINXbase, 4, "ixj control")) {
					printk(KERN_INFO "ixj: can't get I/O address 0x%x\n", j->XILINXbase);
					return -1;
				}
				j->pld_slicw.pcib.e1 = 1;
				outb_p(j->pld_slicw.byte, j->XILINXbase);
			} else {
				j->cardtype = QTI_LINEJACK;

				if (!request_region(j->XILINXbase, 8, "ixj control")) {
					printk(KERN_INFO "ixj: can't get I/O address 0x%x\n", j->XILINXbase);
					return -1;
				}
			}
		} else if (j->dsp.low == 0x22) {
			j->cardtype = QTI_PHONEJACK_PCI;
			request_region(j->XILINXbase, 4, "ixj control");
			j->pld_slicw.pcib.e1 = 1;
			outb_p(j->pld_slicw.byte, j->XILINXbase);
		} else
			j->cardtype = QTI_PHONEJACK;
	} else {
		switch (j->cardtype) {
		case QTI_PHONEJACK:
			if (!j->dsp.low != 0x20) {
				j->dsp.high = 0x80;
				j->dsp.low = 0x20;
				ixj_WriteDSPCommand(0x3800, j);
				j->ver.low = j->ssr.low;
				j->ver.high = j->ssr.high;
			}
			break;
		case QTI_LINEJACK:
			if (!request_region(j->XILINXbase, 8, "ixj control")) {
				printk(KERN_INFO "ixj: can't get I/O address 0x%x\n", j->XILINXbase);
				return -1;
			}
			break;
		case QTI_PHONEJACK_LITE:
		case QTI_PHONEJACK_PCI:
			if (!request_region(j->XILINXbase, 4, "ixj control")) {
				printk(KERN_INFO "ixj: can't get I/O address 0x%x\n", j->XILINXbase);
				return -1;
			}
			j->pld_slicw.pcib.e1 = 1;
			outb_p(j->pld_slicw.byte, j->XILINXbase);
			break;
		case QTI_PHONECARD:
			break;
		}
	}
	if (j->dsp.low == 0x20 || j->cardtype == QTI_PHONEJACK_LITE || j->cardtype == QTI_PHONEJACK_PCI) {
		if (ixjdebug & 0x0002)
			printk(KERN_INFO "Write CODEC config to Software Control Register\n");
		if (ixj_WriteDSPCommand(0xC462, j))		
			return -1;
		if (ixjdebug & 0x0002)
			printk(KERN_INFO "Write CODEC timing to Software Control Register\n");
		if (j->cardtype == QTI_PHONEJACK) {
			cmd = 0x9FF2;
		} else {
			cmd = 0x9FF5;
		}
		if (ixj_WriteDSPCommand(cmd, j))	
			return -1;
	} else {
		if (set_base_frame(j, 30) != 30)
			return -1;
		if (ixjdebug & 0x0002)
			printk(KERN_INFO "Write CODEC config to Software Control Register\n");
		if (j->cardtype == QTI_PHONECARD) {
			if (ixj_WriteDSPCommand(0xC528, j))		
				return -1;
		}
		if (j->cardtype == QTI_LINEJACK) {
			if (ixj_WriteDSPCommand(0xC528, j))		
				return -1;
			if (ixjdebug & 0x0002)
				printk(KERN_INFO "Turn on the PLD Clock at 8Khz\n");
			j->pld_clock.byte = 0;
			outb_p(j->pld_clock.byte, j->XILINXbase + 0x04);
		}
	}

	if (j->dsp.low == 0x20) {
		if (ixjdebug & 0x0002)
			printk(KERN_INFO "Configure GPIO pins\n");
		j->gpio.bytes.high = 0x09;
		j->gpio.bits.gpio1 = 1;
		j->gpio.bits.gpio2 = 1;
		j->gpio.bits.gpio3 = 0;
		j->gpio.bits.gpio4 = 1;
		j->gpio.bits.gpio5 = 1;
		j->gpio.bits.gpio6 = 1;
		j->gpio.bits.gpio7 = 1;
		ixj_WriteDSPCommand(j->gpio.word, j);	
		if (ixjdebug & 0x0002)
			printk(KERN_INFO "Enable SLIC\n");
		j->gpio.bytes.high = 0x0B;
		j->gpio.bytes.low = 0x00;
		j->gpio.bits.gpio1 = 0;
		j->gpio.bits.gpio2 = 1;
		j->gpio.bits.gpio5 = 0;
		ixj_WriteDSPCommand(j->gpio.word, j);	
		j->port = PORT_POTS;
	} else {
		if (j->cardtype == QTI_LINEJACK) {
			LED_SetState(0x1, j);
			msleep(100);
			LED_SetState(0x2, j);
			msleep(100);
			LED_SetState(0x4, j);
			msleep(100);
			LED_SetState(0x8, j);
			msleep(100);
			LED_SetState(0x0, j);
			daa_get_version(j);
			if (ixjdebug & 0x0002)
				printk("Loading DAA Coefficients\n");
			DAA_Coeff_US(j);
			if (!ixj_daa_write(j)) {
				printk("DAA write failed on board %d\n", j->board);
				return -1;
			}
			if(!ixj_daa_cid_reset(j)) {
				printk("DAA CID reset failed on board %d\n", j->board);
				return -1;
			}
			j->flags.pots_correct = 0;
			j->flags.pstn_present = 0;
			ixj_linetest(j);
			if (j->flags.pots_correct) {
				j->pld_scrw.bits.daafsyncen = 0;	

				outb_p(j->pld_scrw.byte, j->XILINXbase);
				j->pld_slicw.bits.rly1 = 1;
				j->pld_slicw.bits.spken = 1;
				outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
				SLIC_SetState(PLD_SLIC_STATE_STANDBY, j);
				j->port = PORT_POTS;
			}
			ixj_set_port(j, PORT_PSTN);
			ixj_set_pots(j, 1);
			if (ixjdebug & 0x0002)
				printk(KERN_INFO "Enable Mixer\n");
			ixj_mixer(0x0000, j);	
			ixj_mixer(0x0100, j);	

			ixj_mixer(0x0203, j);	
			ixj_mixer(0x0303, j);	

			ixj_mixer(0x0480, j);	
			ixj_mixer(0x0580, j);	

			ixj_mixer(0x0680, j);	
			ixj_mixer(0x0780, j);	

			ixj_mixer(0x0880, j);	
			ixj_mixer(0x0980, j);	

			ixj_mixer(0x0A80, j);	
			ixj_mixer(0x0B80, j);	

			ixj_mixer(0x0C00, j);	
			ixj_mixer(0x0D80, j);	

			ixj_mixer(0x0E80, j);	

			ixj_mixer(0x0F00, j);	

			ixj_mixer(0x1000, j);	
			ixj_mixer(0x110C, j);


			ixj_mixer(0x1200, j);	
			ixj_mixer(0x1401, j);

			ixj_mixer(0x1300, j);       
			ixj_mixer(0x1501, j);

			ixj_mixer(0x1700, j);	

			ixj_mixer(0x1800, j);	

			ixj_mixer(0x1901, j);	

			if (ixjdebug & 0x0002)
				printk(KERN_INFO "Setting Default US Ring Cadence Detection\n");
			j->cadence_f[4].state = 0;
			j->cadence_f[4].on1 = 0;	
			j->cadence_f[4].off1 = 0;
			j->cadence_f[4].on2 = 0;
			j->cadence_f[4].off2 = 0;
			j->cadence_f[4].on3 = 0;
			j->cadence_f[4].off3 = 0;	
			j->pstn_last_rmr = jiffies;

		} else {
			if (j->cardtype == QTI_PHONECARD) {
				ixj_WriteDSPCommand(0xCF07, j);
				ixj_WriteDSPCommand(0x00B0, j);
				ixj_set_port(j, PORT_SPEAKER);
			} else {
				ixj_set_port(j, PORT_POTS);
				SLIC_SetState(PLD_SLIC_STATE_STANDBY, j);
			}
		}
	}

	j->intercom = -1;
	j->framesread = j->frameswritten = 0;
	j->read_wait = j->write_wait = 0;
	j->rxreadycheck = j->txreadycheck = 0;

	
	if (j->cardtype == QTI_LINEJACK) {
		set_dtmf_prescale(j, 0x10); 
	} else {
		set_dtmf_prescale(j, 0x40); 
	}
	set_play_volume(j, 0x100);
	set_rec_volume(j, 0x100);

	if (ixj_WriteDSPCommand(0x0000, j))		
		return -1;
	if (j->ssr.low || j->ssr.high)
		return -1;

	if (ixjdebug & 0x0002)
		printk(KERN_INFO "Enable Line Monitor\n");

	if (ixjdebug & 0x0002)
		printk(KERN_INFO "Set Line Monitor to Asyncronous Mode\n");

	if (ixj_WriteDSPCommand(0x7E01, j))		
		return -1;

	if (ixjdebug & 0x002)
		printk(KERN_INFO "Enable DTMF Detectors\n");

	if (ixj_WriteDSPCommand(0x5151, j))		
		return -1;

	if (ixj_WriteDSPCommand(0x6E01, j))		
		return -1;

	set_rec_depth(j, 2);	

	set_play_depth(j, 2);	

	j->ex.bits.dtmf_ready = 0;
	j->dtmf_state = 0;
	j->dtmf_wp = j->dtmf_rp = 0;
	j->rec_mode = j->play_mode = -1;
	j->flags.ringing = 0;
	j->maxrings = MAXRINGS;
	j->ring_cadence = USA_RING_CADENCE;
	j->drybuffer = 0;
	j->winktime = 320;
	j->flags.dtmf_oob = 0;
	for (cnt = 0; cnt < 4; cnt++)
		j->cadence_f[cnt].enable = 0;
	
	ixj_WriteDSPCommand(0x0FE3, j);	

	
	for (cnt = 0; cnt < 35; cnt++)
		j->ixj_signals[cnt] = SIGIO;

	
	j->ex_sig.bits.dtmf_ready = j->ex_sig.bits.hookstate = j->ex_sig.bits.flash = j->ex_sig.bits.pstn_ring = 
	j->ex_sig.bits.caller_id = j->ex_sig.bits.pstn_wink = j->ex_sig.bits.f0 = j->ex_sig.bits.f1 = j->ex_sig.bits.f2 = 
	j->ex_sig.bits.f3 = j->ex_sig.bits.fc0 = j->ex_sig.bits.fc1 = j->ex_sig.bits.fc2 = j->ex_sig.bits.fc3 = 1;
#ifdef IXJ_DYN_ALLOC
	j->fskdata = NULL;
#endif
	j->fskdcnt = 0;
	j->cidcw_wait = 0;
 
	
	j->p.f_op = &ixj_fops;
	j->p.open = ixj_open;
	j->p.board = j->board;
	phone_register_device(&j->p, PHONE_UNIT_ANY);

	ixj_init_timer(j);
	ixj_add_timer(j);
	return 0;
}

 
IXJ *ixj_pcmcia_probe(unsigned long dsp, unsigned long xilinx)
{
	IXJ *j = ixj_alloc();

	j->board = 0;

	j->DSPbase = dsp;
	j->XILINXbase = xilinx;
	j->cardtype = QTI_PHONECARD;
	ixj_selfprobe(j);
	return j;
}

EXPORT_SYMBOL(ixj_pcmcia_probe);		

static int ixj_get_status_proc(char *buf)
{
	int len;
	int cnt;
	IXJ *j;
	len = 0;
	len += sprintf(buf + len, "\nDriver version %i.%i.%i", IXJ_VER_MAJOR, IXJ_VER_MINOR, IXJ_BLD_VER);
	len += sprintf(buf + len, "\nsizeof IXJ struct %Zd bytes", sizeof(IXJ));
	len += sprintf(buf + len, "\nsizeof DAA struct %Zd bytes", sizeof(DAA_REGS));
	len += sprintf(buf + len, "\nUsing old telephony API");
	len += sprintf(buf + len, "\nDebug Level %d\n", ixjdebug);

	for (cnt = 0; cnt < IXJMAX; cnt++) {
		j = get_ixj(cnt);
		if(j==NULL)
			continue;
		if (j->DSPbase) {
			len += sprintf(buf + len, "\nCard Num %d", cnt);
			len += sprintf(buf + len, "\nDSP Base Address 0x%4.4x", j->DSPbase);
			if (j->cardtype != QTI_PHONEJACK)
				len += sprintf(buf + len, "\nXILINX Base Address 0x%4.4x", j->XILINXbase);
			len += sprintf(buf + len, "\nDSP Type %2.2x%2.2x", j->dsp.high, j->dsp.low);
			len += sprintf(buf + len, "\nDSP Version %2.2x.%2.2x", j->ver.high, j->ver.low);
			len += sprintf(buf + len, "\nSerial Number %8.8x", j->serial);
			switch (j->cardtype) {
			case (QTI_PHONEJACK):
				len += sprintf(buf + len, "\nCard Type = Internet PhoneJACK");
				break;
			case (QTI_LINEJACK):
				len += sprintf(buf + len, "\nCard Type = Internet LineJACK");
				if (j->flags.g729_loaded)
					len += sprintf(buf + len, " w/G.729 A/B");
				len += sprintf(buf + len, " Country = %d", j->daa_country);
				break;
			case (QTI_PHONEJACK_LITE):
				len += sprintf(buf + len, "\nCard Type = Internet PhoneJACK Lite");
				if (j->flags.g729_loaded)
					len += sprintf(buf + len, " w/G.729 A/B");
				break;
			case (QTI_PHONEJACK_PCI):
				len += sprintf(buf + len, "\nCard Type = Internet PhoneJACK PCI");
				if (j->flags.g729_loaded)
					len += sprintf(buf + len, " w/G.729 A/B");
				break;
			case (QTI_PHONECARD):
				len += sprintf(buf + len, "\nCard Type = Internet PhoneCARD");
				if (j->flags.g729_loaded)
					len += sprintf(buf + len, " w/G.729 A/B");
				len += sprintf(buf + len, "\nSmart Cable %spresent", j->pccr1.bits.drf ? "not " : "");
				if (!j->pccr1.bits.drf)
					len += sprintf(buf + len, "\nSmart Cable type %d", j->flags.pcmciasct);
				len += sprintf(buf + len, "\nSmart Cable state %d", j->flags.pcmciastate);
				break;
			default:
				len += sprintf(buf + len, "\nCard Type = %d", j->cardtype);
				break;
			}
			len += sprintf(buf + len, "\nReaders %d", j->readers);
			len += sprintf(buf + len, "\nWriters %d", j->writers);
			add_caps(j);
			len += sprintf(buf + len, "\nCapabilities %d", j->caps);
			if (j->dsp.low != 0x20)
				len += sprintf(buf + len, "\nDSP Processor load %d", j->proc_load);
			if (j->flags.cidsent)
				len += sprintf(buf + len, "\nCaller ID data sent");
			else
				len += sprintf(buf + len, "\nCaller ID data not sent");

			len += sprintf(buf + len, "\nPlay CODEC ");
			switch (j->play_codec) {
			case G723_63:
				len += sprintf(buf + len, "G.723.1 6.3");
				break;
			case G723_53:
				len += sprintf(buf + len, "G.723.1 5.3");
				break;
			case TS85:
				len += sprintf(buf + len, "TrueSpeech 8.5");
				break;
			case TS48:
				len += sprintf(buf + len, "TrueSpeech 4.8");
				break;
			case TS41:
				len += sprintf(buf + len, "TrueSpeech 4.1");
				break;
			case G728:
				len += sprintf(buf + len, "G.728");
				break;
			case G729:
				len += sprintf(buf + len, "G.729");
				break;
			case G729B:
				len += sprintf(buf + len, "G.729B");
				break;
			case ULAW:
				len += sprintf(buf + len, "uLaw");
				break;
			case ALAW:
				len += sprintf(buf + len, "aLaw");
				break;
			case LINEAR16:
				len += sprintf(buf + len, "16 bit Linear");
				break;
			case LINEAR8:
				len += sprintf(buf + len, "8 bit Linear");
				break;
			case WSS:
				len += sprintf(buf + len, "Windows Sound System");
				break;
			default:
				len += sprintf(buf + len, "NO CODEC CHOSEN");
				break;
			}
			len += sprintf(buf + len, "\nRecord CODEC ");
			switch (j->rec_codec) {
			case G723_63:
				len += sprintf(buf + len, "G.723.1 6.3");
				break;
			case G723_53:
				len += sprintf(buf + len, "G.723.1 5.3");
				break;
			case TS85:
				len += sprintf(buf + len, "TrueSpeech 8.5");
				break;
			case TS48:
				len += sprintf(buf + len, "TrueSpeech 4.8");
				break;
			case TS41:
				len += sprintf(buf + len, "TrueSpeech 4.1");
				break;
			case G728:
				len += sprintf(buf + len, "G.728");
				break;
			case G729:
				len += sprintf(buf + len, "G.729");
				break;
			case G729B:
				len += sprintf(buf + len, "G.729B");
				break;
			case ULAW:
				len += sprintf(buf + len, "uLaw");
				break;
			case ALAW:
				len += sprintf(buf + len, "aLaw");
				break;
			case LINEAR16:
				len += sprintf(buf + len, "16 bit Linear");
				break;
			case LINEAR8:
				len += sprintf(buf + len, "8 bit Linear");
				break;
			case WSS:
				len += sprintf(buf + len, "Windows Sound System");
				break;
			default:
				len += sprintf(buf + len, "NO CODEC CHOSEN");
				break;
			}
			len += sprintf(buf + len, "\nAEC ");
			switch (j->aec_level) {
			case AEC_OFF:
				len += sprintf(buf + len, "Off");
				break;
			case AEC_LOW:
				len += sprintf(buf + len, "Low");
				break;
			case AEC_MED:
				len += sprintf(buf + len, "Med");
				break;
			case AEC_HIGH:
				len += sprintf(buf + len, "High");
				break;
			case AEC_AUTO:
				len += sprintf(buf + len, "Auto");
				break;
			case AEC_AGC:
				len += sprintf(buf + len, "AEC/AGC");
				break;
			default:
				len += sprintf(buf + len, "unknown(%i)", j->aec_level);
				break;
			}

			len += sprintf(buf + len, "\nRec volume 0x%x", get_rec_volume(j));
			len += sprintf(buf + len, "\nPlay volume 0x%x", get_play_volume(j));
			len += sprintf(buf + len, "\nDTMF prescale 0x%x", get_dtmf_prescale(j));
			
			len += sprintf(buf + len, "\nHook state %d", j->hookstate); 

			if (j->cardtype == QTI_LINEJACK) {
				len += sprintf(buf + len, "\nPOTS Correct %d", j->flags.pots_correct);
				len += sprintf(buf + len, "\nPSTN Present %d", j->flags.pstn_present);
				len += sprintf(buf + len, "\nPSTN Check %d", j->flags.pstncheck);
				len += sprintf(buf + len, "\nPOTS to PSTN %d", j->flags.pots_pstn);
				switch (j->daa_mode) {
				case SOP_PU_SLEEP:
					len += sprintf(buf + len, "\nDAA PSTN On Hook");
					break;
				case SOP_PU_RINGING:
					len += sprintf(buf + len, "\nDAA PSTN Ringing");
					len += sprintf(buf + len, "\nRinging state = %d", j->cadence_f[4].state);
					break;
				case SOP_PU_CONVERSATION:
					len += sprintf(buf + len, "\nDAA PSTN Off Hook");
					break;
				case SOP_PU_PULSEDIALING:
					len += sprintf(buf + len, "\nDAA PSTN Pulse Dialing");
					break;
				}
				len += sprintf(buf + len, "\nDAA RMR = %d", j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.bitreg.RMR);
				len += sprintf(buf + len, "\nDAA VDD OK = %d", j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.bitreg.VDD_OK);
				len += sprintf(buf + len, "\nDAA CR0 = 0x%02x", j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg);
				len += sprintf(buf + len, "\nDAA CR1 = 0x%02x", j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.reg);
				len += sprintf(buf + len, "\nDAA CR2 = 0x%02x", j->m_DAAShadowRegs.SOP_REGS.SOP.cr2.reg);
				len += sprintf(buf + len, "\nDAA CR3 = 0x%02x", j->m_DAAShadowRegs.SOP_REGS.SOP.cr3.reg);
				len += sprintf(buf + len, "\nDAA CR4 = 0x%02x", j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.reg);
				len += sprintf(buf + len, "\nDAA CR5 = 0x%02x", j->m_DAAShadowRegs.SOP_REGS.SOP.cr5.reg);
				len += sprintf(buf + len, "\nDAA XR0 = 0x%02x", j->m_DAAShadowRegs.XOP_REGS.XOP.xr0.reg);
				len += sprintf(buf + len, "\nDAA ringstop %ld - jiffies %ld", j->pstn_ring_stop, jiffies);
			}
			switch (j->port) {
			case PORT_POTS:
				len += sprintf(buf + len, "\nPort POTS");
				break;
			case PORT_PSTN:
				len += sprintf(buf + len, "\nPort PSTN");
				break;
			case PORT_SPEAKER:
				len += sprintf(buf + len, "\nPort SPEAKER/MIC");
				break;
			case PORT_HANDSET:
				len += sprintf(buf + len, "\nPort HANDSET");
				break;
			}
			if (j->dsp.low == 0x21 || j->dsp.low == 0x22) {
				len += sprintf(buf + len, "\nSLIC state ");
				switch (SLIC_GetState(j)) {
				case PLD_SLIC_STATE_OC:
					len += sprintf(buf + len, "OC");
					break;
				case PLD_SLIC_STATE_RINGING:
					len += sprintf(buf + len, "RINGING");
					break;
				case PLD_SLIC_STATE_ACTIVE:
					len += sprintf(buf + len, "ACTIVE");
					break;
				case PLD_SLIC_STATE_OHT:	
					len += sprintf(buf + len, "OHT");
					break;
				case PLD_SLIC_STATE_TIPOPEN:
					len += sprintf(buf + len, "TIPOPEN");
					break;
				case PLD_SLIC_STATE_STANDBY:
					len += sprintf(buf + len, "STANDBY");
					break;
				case PLD_SLIC_STATE_APR:	
					len += sprintf(buf + len, "APR");
					break;
				case PLD_SLIC_STATE_OHTPR:	
					len += sprintf(buf + len, "OHTPR");
					break;
				default:
					len += sprintf(buf + len, "%d", SLIC_GetState(j));
					break;
				}
			}
			len += sprintf(buf + len, "\nBase Frame %2.2x.%2.2x", j->baseframe.high, j->baseframe.low);
			len += sprintf(buf + len, "\nCID Base Frame %2d", j->cid_base_frame_size);
#ifdef PERFMON_STATS
			len += sprintf(buf + len, "\nTimer Checks %ld", j->timerchecks);
			len += sprintf(buf + len, "\nRX Ready Checks %ld", j->rxreadycheck);
			len += sprintf(buf + len, "\nTX Ready Checks %ld", j->txreadycheck);
			len += sprintf(buf + len, "\nFrames Read %ld", j->framesread);
			len += sprintf(buf + len, "\nFrames Written %ld", j->frameswritten);
			len += sprintf(buf + len, "\nDry Buffer %ld", j->drybuffer);
			len += sprintf(buf + len, "\nRead Waits %ld", j->read_wait);
			len += sprintf(buf + len, "\nWrite Waits %ld", j->write_wait);
                        len += sprintf(buf + len, "\nStatus Waits %ld", j->statuswait);
                        len += sprintf(buf + len, "\nStatus Wait Fails %ld", j->statuswaitfail);
                        len += sprintf(buf + len, "\nPControl Waits %ld", j->pcontrolwait);
                        len += sprintf(buf + len, "\nPControl Wait Fails %ld", j->pcontrolwaitfail);
                        len += sprintf(buf + len, "\nIs Control Ready Checks %ld", j->iscontrolready);
                        len += sprintf(buf + len, "\nIs Control Ready Check failures %ld", j->iscontrolreadyfail);
 
#endif
			len += sprintf(buf + len, "\n");
		}
	}
	return len;
}

static int ixj_read_proc(char *page, char **start, off_t off,
                              int count, int *eof, void *data)
{
        int len = ixj_get_status_proc(page);
        if (len <= off+count) *eof = 1;
        *start = page + off;
        len -= off;
        if (len>count) len = count;
        if (len<0) len = 0;
        return len;
}


static void cleanup(void)
{
	int cnt;
	IXJ *j;

	for (cnt = 0; cnt < IXJMAX; cnt++) {
		j = get_ixj(cnt);
		if(j != NULL && j->DSPbase) {
			if (ixjdebug & 0x0002)
				printk(KERN_INFO "IXJ: Deleting timer for /dev/phone%d\n", cnt);
			del_timer(&j->timer);
			if (j->cardtype == QTI_LINEJACK) {
				j->pld_scrw.bits.daafsyncen = 0;	

				outb_p(j->pld_scrw.byte, j->XILINXbase);
				j->pld_slicw.bits.rly1 = 0;
				j->pld_slicw.bits.rly2 = 0;
				j->pld_slicw.bits.rly3 = 0;
				outb_p(j->pld_slicw.byte, j->XILINXbase + 0x01);
				LED_SetState(0x0, j);
				if (ixjdebug & 0x0002)
					printk(KERN_INFO "IXJ: Releasing XILINX address for /dev/phone%d\n", cnt);
				release_region(j->XILINXbase, 8);
			} else if (j->cardtype == QTI_PHONEJACK_LITE || j->cardtype == QTI_PHONEJACK_PCI) {
				if (ixjdebug & 0x0002)
					printk(KERN_INFO "IXJ: Releasing XILINX address for /dev/phone%d\n", cnt);
				release_region(j->XILINXbase, 4);
			}
			kfree(j->read_buffer);
			kfree(j->write_buffer);
			if (j->dev)
				pnp_device_detach(j->dev);
			if (ixjdebug & 0x0002)
				printk(KERN_INFO "IXJ: Unregistering /dev/phone%d from LTAPI\n", cnt);
			phone_unregister_device(&j->p);
			if (ixjdebug & 0x0002)
				printk(KERN_INFO "IXJ: Releasing DSP address for /dev/phone%d\n", cnt);
			release_region(j->DSPbase, 16);
#ifdef IXJ_DYN_ALLOC
			if (ixjdebug & 0x0002)
				printk(KERN_INFO "IXJ: Freeing memory for /dev/phone%d\n", cnt);
			kfree(j);
			ixj[cnt] = NULL;
#endif
		}
	}
	if (ixjdebug & 0x0002)
		printk(KERN_INFO "IXJ: Removing /proc/ixj\n");
	remove_proc_entry ("ixj", NULL);
}

typedef struct {
	BYTE length;
	DWORD bits;
} DATABLOCK;

static void PCIEE_WriteBit(WORD wEEPROMAddress, BYTE lastLCC, BYTE byData)
{
	lastLCC = lastLCC & 0xfb;
	lastLCC = lastLCC | (byData ? 4 : 0);
	outb(lastLCC, wEEPROMAddress);	

	mdelay(1);
	lastLCC = lastLCC | 0x01;
	outb(lastLCC, wEEPROMAddress);	

	byData = byData << 1;
	lastLCC = lastLCC & 0xfe;
	mdelay(1);
	outb(lastLCC, wEEPROMAddress);	

}

static BYTE PCIEE_ReadBit(WORD wEEPROMAddress, BYTE lastLCC)
{
	mdelay(1);
	lastLCC = lastLCC | 0x01;
	outb(lastLCC, wEEPROMAddress);	

	lastLCC = lastLCC & 0xfe;
	mdelay(1);
	outb(lastLCC, wEEPROMAddress);	

	return ((inb(wEEPROMAddress) >> 3) & 1);
}

static bool PCIEE_ReadWord(WORD wAddress, WORD wLoc, WORD * pwResult)
{
	BYTE lastLCC;
	WORD wEEPROMAddress = wAddress + 3;
	DWORD i;
	BYTE byResult;
	*pwResult = 0;
	lastLCC = inb(wEEPROMAddress);
	lastLCC = lastLCC | 0x02;
	lastLCC = lastLCC & 0xfe;
	outb(lastLCC, wEEPROMAddress);	

	mdelay(1);		

	PCIEE_WriteBit(wEEPROMAddress, lastLCC, 1);
	PCIEE_WriteBit(wEEPROMAddress, lastLCC, 1);
	PCIEE_WriteBit(wEEPROMAddress, lastLCC, 0);
	for (i = 0; i < 8; i++) {
		PCIEE_WriteBit(wEEPROMAddress, lastLCC, wLoc & 0x80 ? 1 : 0);
		wLoc <<= 1;
	}

	for (i = 0; i < 16; i++) {
		byResult = PCIEE_ReadBit(wEEPROMAddress, lastLCC);
		*pwResult = (*pwResult << 1) | byResult;
	}

	mdelay(1);		

	lastLCC = lastLCC & 0xfd;
	outb(lastLCC, wEEPROMAddress);	

	return 0;
}

static DWORD PCIEE_GetSerialNumber(WORD wAddress)
{
	WORD wLo, wHi;
	if (PCIEE_ReadWord(wAddress, 62, &wLo))
		return 0;
	if (PCIEE_ReadWord(wAddress, 63, &wHi))
		return 0;
	return (((DWORD) wHi << 16) | wLo);
}

static int dspio[IXJMAX + 1] =
{
	0,
};
static int xio[IXJMAX + 1] =
{
	0,
};

module_param_array(dspio, int, NULL, 0);
module_param_array(xio, int, NULL, 0);
MODULE_DESCRIPTION("Quicknet VoIP Telephony card module - www.quicknet.net");
MODULE_AUTHOR("Ed Okerson <eokerson@quicknet.net>");
MODULE_LICENSE("GPL");

static void __exit ixj_exit(void)
{
        cleanup();
}

static IXJ *new_ixj(unsigned long port)
{
	IXJ *res;
	if (!request_region(port, 16, "ixj DSP")) {
		printk(KERN_INFO "ixj: can't get I/O address 0x%lx\n", port);
		return NULL;
	}
	res = ixj_alloc();
	if (!res) {
		release_region(port, 16);
		printk(KERN_INFO "ixj: out of memory\n");
		return NULL;
	}
	res->DSPbase = port;
	return res;
}

static int __init ixj_probe_isapnp(int *cnt)
{               
	int probe = 0;
	int func = 0x110;
        struct pnp_dev *dev = NULL, *old_dev = NULL;

	while (1) {
		do {
			IXJ *j;
			int result;

			old_dev = dev;
			dev = pnp_find_dev(NULL, ISAPNP_VENDOR('Q', 'T', 'I'),
					 ISAPNP_FUNCTION(func), old_dev);
			if (!dev || !dev->card)
				break;
			result = pnp_device_attach(dev);
			if (result < 0) {
				printk("pnp attach failed %d \n", result);
				break;
			}
			if (pnp_activate_dev(dev) < 0) {
				printk("pnp activate failed (out of resources?)\n");
				pnp_device_detach(dev);
				return -ENOMEM;
			}

			if (!pnp_port_valid(dev, 0)) {
				pnp_device_detach(dev);
				return -ENODEV;
			}

			j = new_ixj(pnp_port_start(dev, 0));
			if (!j)
				break;

			if (func != 0x110)
				j->XILINXbase = pnp_port_start(dev, 1);	

			switch (func) {
			case (0x110):
				j->cardtype = QTI_PHONEJACK;
				break;
			case (0x310):
				j->cardtype = QTI_LINEJACK;
				break;
			case (0x410):
				j->cardtype = QTI_PHONEJACK_LITE;
				break;
			}
			j->board = *cnt;
			probe = ixj_selfprobe(j);
			if(!probe) {
				j->serial = dev->card->serial;
				j->dev = dev;
				switch (func) {
				case 0x110:
					printk(KERN_INFO "ixj: found Internet PhoneJACK at 0x%x\n", j->DSPbase);
					break;
				case 0x310:
					printk(KERN_INFO "ixj: found Internet LineJACK at 0x%x\n", j->DSPbase);
					break;
				case 0x410:
					printk(KERN_INFO "ixj: found Internet PhoneJACK Lite at 0x%x\n", j->DSPbase);
					break;
				}
			}
			++*cnt;
		} while (dev);
		if (func == 0x410)
			break;
		if (func == 0x310)
			func = 0x410;
		if (func == 0x110)
			func = 0x310;
		dev = NULL;
	}
	return probe;
}
                        
static int __init ixj_probe_isa(int *cnt)
{
	int i, probe;

	
	for (i = 0; i < IXJMAX; i++) {
		if (dspio[i]) {
			IXJ *j = new_ixj(dspio[i]);

			if (!j)
				break;

			j->XILINXbase = xio[i];
			j->cardtype = 0;

			j->board = *cnt;
			probe = ixj_selfprobe(j);
			j->dev = NULL;
			++*cnt;
		}
	}
	return 0;
}

static int __init ixj_probe_pci(int *cnt)
{
	struct pci_dev *pci = NULL;   
	int i, probe = 0;
	IXJ *j = NULL;

	for (i = 0; i < IXJMAX - *cnt; i++) {
		pci = pci_get_device(PCI_VENDOR_ID_QUICKNET,
				      PCI_DEVICE_ID_QUICKNET_XJ, pci);
		if (!pci)
			break;

		if (pci_enable_device(pci))
			break;
		j = new_ixj(pci_resource_start(pci, 0));
		if (!j)
			break;

		j->serial = (PCIEE_GetSerialNumber)pci_resource_start(pci, 2);
		j->XILINXbase = j->DSPbase + 0x10;
		j->cardtype = QTI_PHONEJACK_PCI;
		j->board = *cnt;
		probe = ixj_selfprobe(j);
		if (!probe)
			printk(KERN_INFO "ixj: found Internet PhoneJACK PCI at 0x%x\n", j->DSPbase);
		++*cnt;
	}
	pci_dev_put(pci);
	return probe;
}

static int __init ixj_init(void)
{
	int cnt = 0;
	int probe = 0;   

	cnt = 0;

	
	if ((probe = ixj_probe_isapnp(&cnt)) < 0) {
		return probe;
	}
	if ((probe = ixj_probe_isa(&cnt)) < 0) {
		return probe;
	}
	if ((probe = ixj_probe_pci(&cnt)) < 0) {
		return probe;
	}
	printk(KERN_INFO "ixj driver initialized.\n");
	create_proc_read_entry ("ixj", 0, NULL, ixj_read_proc, NULL);
	return probe;
}

module_init(ixj_init);
module_exit(ixj_exit);

static void DAA_Coeff_US(IXJ *j)
{
	int i;

	j->daa_country = DAA_US;
	
	
	for (i = 0; i < ALISDAA_CALLERID_SIZE; i++) {
		j->m_DAAShadowRegs.CAO_REGS.CAO.CallerID[i] = 0;
	}

	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[7] = 0x03;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[6] = 0x4B;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[5] = 0x5D;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[4] = 0xCD;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[3] = 0x24;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[2] = 0xC5;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[1] = 0xA0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[0] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[7] = 0x71;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[6] = 0x1A;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[5] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[4] = 0x0A;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[3] = 0xB5;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[2] = 0x33;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[1] = 0xE0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[7] = 0x05;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[6] = 0xA3;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[5] = 0x72;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[4] = 0x34;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[3] = 0x3F;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[2] = 0x3B;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[1] = 0x30;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[7] = 0x05;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[6] = 0x87;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[5] = 0xF9;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[4] = 0x3E;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[3] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[2] = 0xDA;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[1] = 0xB0;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[3] = 0x41;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[2] = 0xB5;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[1] = 0xDD;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[0] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[3] = 0x25;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[2] = 0xC7;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[1] = 0x10;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[0] = 0xD6;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[6] = 0x42;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[5] = 0x48;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[4] = 0x81;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[3] = 0xA5;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[2] = 0x80;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[0] = 0x98;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[7] = 0x02;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[6] = 0xA2;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[5] = 0x2B;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[4] = 0xB0;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[3] = 0xE8;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[2] = 0xAB;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[1] = 0x81;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[0] = 0xCC;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[6] = 0x88;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[5] = 0xD2;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[4] = 0x24;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[3] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[2] = 0xA9;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[1] = 0x3B;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[0] = 0xA6;
	
	
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[7] = 0x1B;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[6] = 0x3C;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[5] = 0x93;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[4] = 0x3A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[2] = 0x12;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[1] = 0xA3;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[0] = 0x23;
	
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[7] = 0x12;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[6] = 0xA2;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[5] = 0xA6;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[4] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[2] = 0x7A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[1] = 0x0A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[0] = 0xD5;

	
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[3] = 0xAA;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[2] = 0x35;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[1] = 0x0F;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[0] = 0x8E;

	
	 
	

	
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[7] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[6] = 0x0E;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[5] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[4] = 0x09;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[3] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[2] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[1] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[0] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[7] = 0xFD;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[6] = 0xB5;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[5] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[4] = 0x07;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[3] = 0xDA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[2] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[0] = 0x00;
	
	
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg = 0xFF;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.reg = 0x05;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr2.reg = 0x04;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr3.reg = 0x00;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.reg = 0x02;
	
	
	
	
	
	

	j->m_DAAShadowRegs.XOP_xr0_W.reg = 0x02;	
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr1.reg = 0x3C;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr2.reg = 0x7D;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr3.reg = 0x3B;		
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr4.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr5.reg = 0x22;
	j->m_DAAShadowRegs.XOP_xr6_W.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr7.reg = 0x40;		
	
	
	
	
	

	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[3] = 0x11;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[2] = 0xB3;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[1] = 0x5A;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[0] = 0x2C;
	
	
	
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[3] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[2] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[1] = 0x52;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[0] = 0xB3;
}

static void DAA_Coeff_UK(IXJ *j)
{
	int i;

	j->daa_country = DAA_UK;
	
	
	for (i = 0; i < ALISDAA_CALLERID_SIZE; i++) {
		j->m_DAAShadowRegs.CAO_REGS.CAO.CallerID[i] = 0;
	}

	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[6] = 0xC2;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[5] = 0xBB;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[4] = 0xA8;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[3] = 0xCB;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[2] = 0x81;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[1] = 0xA0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[0] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[7] = 0x40;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[6] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[5] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[4] = 0x0A;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[3] = 0xA4;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[2] = 0x33;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[1] = 0xE0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[7] = 0x07;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[6] = 0x9B;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[5] = 0xED;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[4] = 0x24;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[3] = 0xB2;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[2] = 0xA2;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[1] = 0xA0;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[7] = 0x0F;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[6] = 0x92;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[5] = 0xF2;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[4] = 0xB2;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[3] = 0x87;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[2] = 0xD2;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[1] = 0x30;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[3] = 0x1B;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[2] = 0xA5;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[1] = 0xDD;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[0] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[3] = 0xE2;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[2] = 0x27;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[1] = 0x10;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[0] = 0xD6;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[7] = 0x80;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[6] = 0x2D;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[5] = 0x38;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[4] = 0x8B;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[3] = 0xD0;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[2] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[0] = 0x98;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[7] = 0x02;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[6] = 0x5A;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[5] = 0x53;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[4] = 0xF0;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[3] = 0x0B;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[2] = 0x5F;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[1] = 0x84;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[0] = 0xD4;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[6] = 0x88;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[5] = 0x6A;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[4] = 0xA4;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[3] = 0x8F;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[2] = 0x52;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[1] = 0xF5;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[0] = 0x32;
	
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[7] = 0x1B;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[6] = 0x3C;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[5] = 0x93;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[4] = 0x3A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[2] = 0x12;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[1] = 0xA3;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[0] = 0x23;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[7] = 0x12;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[6] = 0xA2;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[5] = 0xA6;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[4] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[2] = 0x7A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[1] = 0x0A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[0] = 0xD5;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[3] = 0xAA;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[2] = 0x35;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[1] = 0x0F;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[0] = 0x8E;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[7] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[6] = 0x0E;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[5] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[4] = 0x09;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[3] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[2] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[1] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[0] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[7] = 0xFD;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[6] = 0xB5;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[5] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[4] = 0x07;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[3] = 0xDA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[2] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[0] = 0x00;
	
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg = 0xFF;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.reg = 0x05;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr2.reg = 0x04;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr3.reg = 0x00;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.reg = 0x02;
	
	
	
	
	

	j->m_DAAShadowRegs.XOP_xr0_W.reg = 0x02;	
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr1.reg = 0x1C;		
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr2.reg = 0x7D;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr3.reg = 0x36;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr4.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr5.reg = 0x22;
	j->m_DAAShadowRegs.XOP_xr6_W.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr7.reg = 0x46;		
	
	
	
	

	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[3] = 0x11;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[2] = 0xB3;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[1] = 0x5A;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[0] = 0x2C;
	
	
	
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[3] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[2] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[1] = 0x52;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[0] = 0xB3;
}


static void DAA_Coeff_France(IXJ *j)
{
	int i;

	j->daa_country = DAA_FRANCE;
	
	
	for (i = 0; i < ALISDAA_CALLERID_SIZE; i++) {
		j->m_DAAShadowRegs.CAO_REGS.CAO.CallerID[i] = 0;
	}

	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[7] = 0x02;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[6] = 0xA2;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[5] = 0x43;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[4] = 0x2C;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[2] = 0xAF;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[1] = 0xA0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[0] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[7] = 0x67;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[6] = 0xCE;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[5] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[4] = 0x2C;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[2] = 0x33;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[1] = 0xE0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[7] = 0x07;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[6] = 0x9A;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[5] = 0x28;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[4] = 0xF6;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[3] = 0x23;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[2] = 0x4A;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[1] = 0xB0;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[7] = 0x03;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[6] = 0x8F;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[5] = 0xF9;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[4] = 0x2F;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[3] = 0x9E;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[2] = 0xFA;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[1] = 0x20;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[3] = 0x16;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[2] = 0xB5;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[1] = 0xDD;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[0] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[3] = 0xE2;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[2] = 0xC7;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[1] = 0x10;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[0] = 0xD6;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[6] = 0x42;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[5] = 0x48;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[4] = 0x81;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[3] = 0xA6;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[2] = 0x80;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[0] = 0x98;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[7] = 0x02;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[6] = 0xAC;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[5] = 0x2A;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[4] = 0x30;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[3] = 0x78;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[2] = 0xAC;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[1] = 0x8A;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[0] = 0x2C;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[6] = 0x88;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[5] = 0xDA;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[4] = 0xA5;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[2] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[1] = 0x2C;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[0] = 0x45;
	
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[7] = 0x1B;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[6] = 0x3C;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[5] = 0x93;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[4] = 0x3A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[2] = 0x12;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[1] = 0xA3;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[0] = 0x23;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[7] = 0x12;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[6] = 0xA2;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[5] = 0xA6;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[4] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[2] = 0x7A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[1] = 0x0A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[0] = 0xD5;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[3] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[2] = 0x45;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[1] = 0xB5;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[0] = 0x84;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[7] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[6] = 0x0E;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[5] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[4] = 0x09;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[3] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[2] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[1] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[0] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[7] = 0xFD;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[6] = 0xB5;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[5] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[4] = 0x07;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[3] = 0xDA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[2] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[0] = 0x00;
	
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg = 0xFF;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.reg = 0x05;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr2.reg = 0x04;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr3.reg = 0x00;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.reg = 0x02;
	
	
	
	
	

	j->m_DAAShadowRegs.XOP_xr0_W.reg = 0x02;	
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr1.reg = 0x1C;		
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr2.reg = 0x7D;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr3.reg = 0x36;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr4.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr5.reg = 0x22;
	j->m_DAAShadowRegs.XOP_xr6_W.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr7.reg = 0x46;		
	
	
	
	

	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[3] = 0x11;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[2] = 0xB3;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[1] = 0x5A;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[0] = 0x2C;
	
	
	
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[3] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[2] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[1] = 0x52;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[0] = 0xB3;
}


static void DAA_Coeff_Germany(IXJ *j)
{
	int i;

	j->daa_country = DAA_GERMANY;
	
	
	for (i = 0; i < ALISDAA_CALLERID_SIZE; i++) {
		j->m_DAAShadowRegs.CAO_REGS.CAO.CallerID[i] = 0;
	}

	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[6] = 0xCE;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[5] = 0xBB;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[4] = 0xB8;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[3] = 0xD2;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[2] = 0x81;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[1] = 0xB0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[0] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[7] = 0x45;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[6] = 0x8F;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[5] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[4] = 0x0C;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[3] = 0xD2;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[2] = 0x3A;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[1] = 0xD0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[7] = 0x07;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[6] = 0xAA;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[5] = 0xE2;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[4] = 0x34;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[3] = 0x24;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[2] = 0x89;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[1] = 0x20;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[7] = 0x02;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[6] = 0x87;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[5] = 0xFA;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[4] = 0x37;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[3] = 0x9A;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[2] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[1] = 0xB0;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[3] = 0x72;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[2] = 0xD5;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[1] = 0xDD;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[0] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[3] = 0x72;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[2] = 0x42;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[1] = 0x13;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[0] = 0x4B;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[7] = 0x80;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[6] = 0x52;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[5] = 0x48;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[4] = 0x81;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[3] = 0xAD;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[2] = 0x80;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[0] = 0x98;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[7] = 0x02;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[6] = 0x42;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[5] = 0x5A;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[4] = 0x20;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[3] = 0xE8;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[2] = 0x1A;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[1] = 0x81;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[0] = 0x27;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[6] = 0x88;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[5] = 0x63;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[4] = 0x26;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[3] = 0xBD;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[2] = 0x4B;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[1] = 0xA3;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[0] = 0xC2;
	
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[7] = 0x1B;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[6] = 0x3B;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[5] = 0x9B;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[4] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[3] = 0xD4;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[2] = 0x1C;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[1] = 0xB3;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[0] = 0x23;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[7] = 0x13;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[6] = 0x42;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[5] = 0xA6;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[4] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[3] = 0xD4;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[2] = 0x73;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[1] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[0] = 0xD5;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[3] = 0xB2;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[2] = 0x45;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[1] = 0x0F;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[0] = 0x8E;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[7] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[6] = 0x0E;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[5] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[4] = 0x09;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[3] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[2] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[1] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[0] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[7] = 0xFD;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[6] = 0xB5;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[5] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[4] = 0x07;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[3] = 0xDA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[2] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[0] = 0x00;
	
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg = 0xFF;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.reg = 0x05;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr2.reg = 0x04;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr3.reg = 0x00;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.reg = 0x02;
	
	
	
	
	

	j->m_DAAShadowRegs.XOP_xr0_W.reg = 0x02;	
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr1.reg = 0x1C;		
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr2.reg = 0x7D;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr3.reg = 0x32;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr4.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr5.reg = 0x22;
	j->m_DAAShadowRegs.XOP_xr6_W.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr7.reg = 0x40;		
	
	
	
	

	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[3] = 0x11;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[2] = 0xB3;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[1] = 0x5A;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[0] = 0x2C;
	
	
	
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[3] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[2] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[1] = 0x52;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[0] = 0xB3;
}


static void DAA_Coeff_Australia(IXJ *j)
{
	int i;

	j->daa_country = DAA_AUSTRALIA;
	
	
	for (i = 0; i < ALISDAA_CALLERID_SIZE; i++) {
		j->m_DAAShadowRegs.CAO_REGS.CAO.CallerID[i] = 0;
	}

	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[6] = 0xA3;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[5] = 0xAA;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[4] = 0x28;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[3] = 0xB3;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[2] = 0x82;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[1] = 0xD0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[0] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[7] = 0x70;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[6] = 0x96;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[5] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[4] = 0x09;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[3] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[2] = 0x6B;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[1] = 0xC0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[7] = 0x07;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[6] = 0x96;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[5] = 0xE2;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[4] = 0x34;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[3] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[2] = 0x9B;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[1] = 0x30;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[7] = 0x0F;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[6] = 0x9A;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[5] = 0xE9;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[4] = 0x2F;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[2] = 0xCC;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[1] = 0xA0;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[3] = 0xCB;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[2] = 0x45;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[1] = 0xDD;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[0] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[3] = 0x1B;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[2] = 0x67;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[1] = 0x10;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[0] = 0xD6;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[7] = 0x80;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[6] = 0x52;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[5] = 0x48;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[4] = 0x81;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[3] = 0xAF;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[2] = 0x80;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[0] = 0x98;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[7] = 0x02;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[6] = 0xDB;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[5] = 0x52;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[4] = 0xB0;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[3] = 0x38;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[2] = 0x01;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[1] = 0x82;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[0] = 0xAC;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[6] = 0x88;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[5] = 0x4A;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[4] = 0x3E;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[3] = 0x2C;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[2] = 0x3B;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[1] = 0x24;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[0] = 0x46;
	
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[7] = 0x1B;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[6] = 0x3C;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[5] = 0x93;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[4] = 0x3A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[2] = 0x12;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[1] = 0xA3;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[0] = 0x23;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[7] = 0x12;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[6] = 0xA2;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[5] = 0xA6;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[4] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[2] = 0x7A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[1] = 0x0A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[0] = 0xD5;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[3] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[2] = 0x45;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[1] = 0xB5;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[0] = 0x84;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[7] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[6] = 0x0E;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[5] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[4] = 0x09;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[3] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[2] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[1] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[0] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[7] = 0xFD;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[6] = 0xB5;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[5] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[4] = 0x07;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[3] = 0xDA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[2] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[0] = 0x00;
	
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg = 0xFF;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.reg = 0x05;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr2.reg = 0x04;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr3.reg = 0x00;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.reg = 0x02;
	
	
	
	
	

	j->m_DAAShadowRegs.XOP_xr0_W.reg = 0x02;	
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr1.reg = 0x1C;		
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr2.reg = 0x7D;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr3.reg = 0x2B;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr4.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr5.reg = 0x22;
	j->m_DAAShadowRegs.XOP_xr6_W.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr7.reg = 0x40;		

	
	
	
	
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[3] = 0x11;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[2] = 0xB3;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[1] = 0x5A;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[0] = 0x2C;

	
	
	
	
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[3] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[2] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[1] = 0x52;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[0] = 0xB3;
}

static void DAA_Coeff_Japan(IXJ *j)
{
	int i;

	j->daa_country = DAA_JAPAN;
	
	
	for (i = 0; i < ALISDAA_CALLERID_SIZE; i++) {
		j->m_DAAShadowRegs.CAO_REGS.CAO.CallerID[i] = 0;
	}

	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[7] = 0x06;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[6] = 0xBD;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[5] = 0xE2;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[4] = 0x2D;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[3] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[2] = 0xF9;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[1] = 0xA0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_1[0] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[7] = 0x6F;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[6] = 0xF7;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[5] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[4] = 0x0E;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[3] = 0x34;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[2] = 0x33;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[1] = 0xE0;
	j->m_DAAShadowRegs.COP_REGS.COP.IMFilterCoeff_2[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[7] = 0x02;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[6] = 0x8F;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[5] = 0x68;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[4] = 0x77;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[3] = 0x9C;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[2] = 0x58;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[1] = 0xF0;
	j->m_DAAShadowRegs.COP_REGS.COP.FRXFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[7] = 0x03;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[6] = 0x8F;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[5] = 0x38;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[4] = 0x73;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[3] = 0x87;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[2] = 0xEA;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[1] = 0x20;
	j->m_DAAShadowRegs.COP_REGS.COP.FRRFilterCoeff[0] = 0x08;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[3] = 0x51;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[2] = 0xC5;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[1] = 0xDD;
	j->m_DAAShadowRegs.COP_REGS.COP.AXFilterCoeff[0] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[3] = 0x25;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[2] = 0xA7;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[1] = 0x10;
	j->m_DAAShadowRegs.COP_REGS.COP.ARFilterCoeff[0] = 0xD6;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[6] = 0x42;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[5] = 0x48;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[4] = 0x81;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[3] = 0xAE;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[2] = 0x80;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_1[0] = 0x98;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[7] = 0x02;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[6] = 0xAB;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[5] = 0x2A;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[4] = 0x20;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[3] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[2] = 0x5B;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[1] = 0x89;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_2[0] = 0x28;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[7] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[6] = 0x88;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[5] = 0xDA;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[4] = 0x25;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[3] = 0x34;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[2] = 0xC5;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[1] = 0x4C;
	j->m_DAAShadowRegs.COP_REGS.COP.THFilterCoeff_3[0] = 0xBA;
	
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[7] = 0x1B;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[6] = 0x3C;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[5] = 0x93;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[4] = 0x3A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[2] = 0x12;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[1] = 0xA3;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_1[0] = 0x23;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[7] = 0x12;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[6] = 0xA2;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[5] = 0xA6;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[4] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[3] = 0x22;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[2] = 0x7A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[1] = 0x0A;
	j->m_DAAShadowRegs.COP_REGS.COP.RingerImpendance_2[0] = 0xD5;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[3] = 0xAA;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[2] = 0x35;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[1] = 0x0F;
	j->m_DAAShadowRegs.COP_REGS.COP.LevelmeteringRinging[0] = 0x8E;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[7] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[6] = 0x0E;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[5] = 0xCA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[4] = 0x09;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[3] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[2] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[1] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID1stTone[0] = 0x99;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[7] = 0xFD;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[6] = 0xB5;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[5] = 0xBA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[4] = 0x07;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[3] = 0xDA;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[2] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[1] = 0x00;
	j->m_DAAShadowRegs.COP_REGS.COP.CallerID2ndTone[0] = 0x00;
	
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr0.reg = 0xFF;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr1.reg = 0x05;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr2.reg = 0x04;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr3.reg = 0x00;
	j->m_DAAShadowRegs.SOP_REGS.SOP.cr4.reg = 0x02;
	
	
	
	
	

	j->m_DAAShadowRegs.XOP_xr0_W.reg = 0x02;	
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr1.reg = 0x1C;		
	

	j->m_DAAShadowRegs.XOP_REGS.XOP.xr2.reg = 0x7D;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr3.reg = 0x22;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr4.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr5.reg = 0x22;
	j->m_DAAShadowRegs.XOP_xr6_W.reg = 0x00;
	j->m_DAAShadowRegs.XOP_REGS.XOP.xr7.reg = 0x40;		
	
	
	
	

	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[3] = 0x11;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[2] = 0xB3;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[1] = 0x5A;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone1Coeff[0] = 0x2C;
	
	
	
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[3] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[2] = 0x32;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[1] = 0x52;
	j->m_DAAShadowRegs.COP_REGS.COP.Tone2Coeff[0] = 0xB3;
}

static s16 tone_table[][19] =
{
	{			
		32538,		
		 -32325,	
		 -343,		
		 0,		
		 343,		
		 32619,		
		 -32520,	
		 19179,		
		 -19178,	
		 19179,		
		 32723,		
		 -32686,	
		 9973,		
		 -9955,		
		 9973,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		32072,		
		 -31896,	
		 -435,		
		 0,		
		 435,		
		 32188,		
		 -32400,	
		 15139,		
		 -14882,	
		 15139,		
		 32473,		
		 -32524,	
		 23200,		
		 -23113,	
		 23200,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		31769,		
		 -32584,	
		 -475,		
		 0,		
		 475,		
		 31789,		
		 -32679,	
		 17280,		
		 -16865,	
		 17280,		
		 31841,		
		 -32681,	
		 543,		
		 -525,		
		 543,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30750,		
		 -31212,	
		 -804,		
		 0,		
		 804,		
		 30686,		
		 -32145,	
		 14747,		
		 -13703,	
		 14747,		
		 31651,		
		 -32321,	
		 24425,		
		 -23914,	
		 24427,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		31613,		
		 -32646,	
		 -185,		
		 0,		
		 185,		
		 31620,		
		 -32713,	
		 19253,		
		 -18566,	
		 19253,		
		 31674,		
		 -32715,	
		 2575,		
		 -2495,		
		 2575,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30741,		
		 -31475,	
		 -703,		
		 0,		
		 703,		
		 30688,		
		 -32248,	
		 14542,		
		 -13523,	
		 14542,		
		 31494,		
		 -32366,	
		 21577,		
		 -21013,	
		 21577,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30627,		
		 -31338,	
		 -843,		
		 0,		
		 843,		
		 30550,		
		 -32221,	
		 13594,		
		 -12589,	
		 13594,		
		 31488,		
		 -32358,	
		 24684,		
		 -24029,	
		 24684,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		31546,		
		 -32646,	
		 -445,		
		 0,		
		 445,		
		 31551,		
		 -32713,	
		 23884,		
		 -22979,	
		 23884,		
		 31606,		
		 -32715,	
		 863,		
		 -835,		
		 863,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		31006,		
		 -32029,	
		 -461,		
		 0,		
		 461,		
		 30999,		
		 -32487,	
		 11325,		
		 -10682,	
		 11325,		
		 31441,		
		 -32526,	
		 24324,		
		 -23535,	
		 24324,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30634,		
		 -31533,	
		 -680,		
		 0,		
		 680,		
		 30571,		
		 -32277,	
		 12894,		
		 -11945,	
		 12894,		
		 31367,		
		 -32379,	
		 23820,		
		 -23104,	
		 23820,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30552,		
		 -31434,	
		 -690,		
		 0,		
		 690,		
		 30472,		
		 -32248,	
		 13385,		
		 -12357,	
		 13385,		
		 31358,		
		 -32366,	
		 26488,		
		 -25692,	
		 26490,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		31397,		
		 -32623,	
		 -117,		
		 0,		
		 117,		
		 31403,		
		 -32700,	
		 3388,		
		 -3240,		
		 3388,		
		 31463,		
		 -32702,	
		 13346,		
		 -12863,	
		 13346,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30831,		
		 -32064,	
		 -367,		
		 0,		
		 367,		
		 30813,		
		 -32456,	
		 11068,		
		 -10338,	
		 11068,		
		 31214,		
		 -32491,	
		 16374,		
		 -15781,	
		 16374,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		31152,		
		 -32613,	
		 -314,		
		 0,		
		 314,		
		 31156,		
		 -32694,	
		 28847,		
		 -2734,		
		 28847,		
		 31225,		
		 -32696,	
		 462,		
		 -442,		
		 462,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30836,		
		 -32296,	
		 -324,		
		 0,		
		 324,		
		 30825,		
		 -32570,	
		 16847,		
		 -15792,	
		 16847,		
		 31106,		
		 -32584,	
		 9579,		
		 -9164,		
		 9579,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30702,		
		 -32134,	
		 -517,		
		 0,		
		 517,		
		 30676,		
		 -32520,	
		 8144,		
		 -7596,		
		 8144,		
		 31084,		
		 -32547,	
		 22713,		
		 -21734,	
		 22713,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30613,		
		 -32031,	
		 -618,		
		 0,		
		 618,		
		 30577,		
		 -32491,	
		 9612,		
		 -8935,		
		 9612,		
		 31071,		
		 -32524,	
		 21596,		
		 -20667,	
		 21596,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30914,		
		 -32584,	
		 -426,		
		 0,		
		 426,		
		 30914,		
		 -32679,	
		 17520,		
		 -16471,	
		 17520,		
		 31004,		
		 -32683,	
		 819,		
		 -780,		
		 819,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
#if 0
	{			
		30881,		
		 -32603,	
		 -496,		
		 0,		
		 496,		
		 30880,		
		 -32692,	
		 24767,		
		 -23290,	
		 24767,		
		 30967,		
		 -32694,	
		 728,		
		 -691,		
		 728,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
#else
	{
		30850,
		-32534,
		-504,
		0,
		504,
		30831,
		-32669,
		24303,
		-22080,
		24303,
		30994,
		-32673,
		1905,
		-1811,
		1905,
		5,
		129,
		17,
		0xff5
	},
#endif
	{			
		30646,		
		 -32327,	
		 -287,		
		 0,		
		 287,		
		 30627,		
		 -32607,	
		 13269,		
		 -12376,	
		 13269,		
		 30924,		
		 -32619,	
		 19950,		
		 -18940,	
		 19950,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30396,		
		 -32014,	
		 -395,		
		 0,		
		 395,		
		 30343,		
		 -32482,	
		 17823,		
		 -16431,	
		 17823,		
		 30872,		
		 -32516,	
		 18124,		
		 -17246,	
		 18124,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30796,		
		 -32603,	
		 -254,		
		 0,		
		 254,		
		 30793,		
		 -32692,	
		 18934,		
		 -17751,	
		 18934,		
		 30882,		
		 -32694,	
		 1858,		
		 -1758,		
		 1858,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30641,		
		 -32458,	
		 -155,		
		 0,		
		 155,		
		 30631,		
		 -32630,	
		 11453,		
		 -10666,	
		 11453,		
		 30810,		
		 -32634,	
		 12237,		
		 -11588,	
		 12237,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30367,		
		 -32147,	
		 -495,		
		 0,		
		 495,		
		 30322,		
		 -32543,	
		 10031,		
		 -9252,		
		 10031,		
		 30770,		
		 -32563,	
		 22674,		
		 -21465,	
		 22674,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30709,		
		 -32603,	
		 -83,		
		 0,		
		 83,		
		 30704,		
		 -32692,	
		 10641,		
		 -9947,		
		 10641,		
		 30796,		
		 -32694,	
		 10079,		
		 9513,		
		 10079,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30664,		
		 -32603,	
		 -164,		
		 0,		
		 164,		
		 30661,		
		 -32692,	
		 15294,		
		 -14275,	
		 15294,		
		 30751,		
		 -32694,	
		 3548,		
		 -3344,		
		 3548,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30653,		
		 -32615,	
		 -209,		
		 0,		
		 209,		
		 30647,		
		 -32702,	
		 18971,		
		 -17716,	
		 18971,		
		 30738,		
		 -32702,	
		 2967,		
		 -2793,		
		 2967,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30437,		
		 -32603,	
		 -264,		
		 0,		
		 264,		
		 30430,		
		 -32692,	
		 21681,		
		 -20082,	
		 21681,		
		 30526,		
		 -32694,	
		 1559,		
		 -1459,		
		 1559,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		28975,		
		 -30955,	
		 -1026,		
		 0,		
		 1026,		
		 28613,		
		 -32089,	
		 14214,		
		 -12202,	
		 14214,		
		 30243,		
		 -32238,	
		 24825,		
		 -23402,	
		 24825,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30257,		
		 -32605,	
		 -249,		
		 0,		
		 249,		
		 30247,		
		 -32694,	
		 18088,		
		 -16652,	
		 18088,		
		 30348,		
		 -32696,	
		 2099,		
		 -1953,		
		 2099,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30202,		
		 -32624,	
		 -413,		
		 0,		
		 413,		
		 30191,		
		 -32714,	
		 25954,		
		 -23890,	
		 25954,		
		 30296,		
		 -32715,	
		 2007,		
		 -1860,		
		 2007,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		30001,		
		 -32613,	
		 -155,		
		 0,		
		 155,		
		 29985,		
		 -32710,	
		 6584,		
		 -6018,		
		 6584,		
		 30105,		
		 -32712,	
		 23812,		
		 -21936,	
		 23812,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		29964,		
		 -32601,	
		 -101,		
		 0,		
		 101,		
		 29949,		
		 -32700,	
		 11041,		
		 -10075,	
		 11041,		
		 30070,		
		 -32702,	
		 16762,		
		 -15437,	
		 16762,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		29936,		
		 -32584,	
		 -91,		
		 0,		
		 91,		
		 29921,		
		 -32688,	
		 11449,		
		 -10426,	
		 11449,		
		 30045,		
		 -32688,	
		 13055,		
		 -12028,	
		 13055,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		28499,		
		 -31129,	
		 -849,		
		 0,		
		 849,		
		 28128,		
		 -32130,	
		 14556,		
		 -12251,	
		 14556,		
		 29667,		
		 -32244,	
		 23038,		
		 -21358,	
		 23040,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		29271,		
		 -32599,	
		 -490,		
		 0,		
		 490,		
		 29246,		
		 -32700,	
		 28961,		
		 -25796,	
		 28961,		
		 29383,		
		 -32700,	
		 1299,		
		 -1169,		
		 1299,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		29230,		
		 -32584,	
		 -418,		
		 0,		
		 418,		
		 29206,		
		 -32688,	
		 36556,		
		 -32478,	
		 36556,		
		 29345,		
		 -32688,	
		 897,		
		 -808,		
		 897,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		29116,		
		 -32603,	
		 -165,		
		 0,		
		 165,		
		 29089,		
		 -32708,	
		 6963,		
		 -6172,		
		 6963,		
		 29237,		
		 -32710,	
		 24197,		
		 -21657,	
		 24197,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		28376,		
		 -32567,	
		 -363,		
		 0,		
		 363,		
		 28337,		
		 -32683,	
		 21766,		
		 -18761,	
		 21766,		
		 28513,		
		 -32686,	
		 2509,		
		 -2196,		
		 2509,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		27844,		
		 -32563,	
		 -366,		
		 0,		
		 366,		
		 27797,		
		 -32686,	
		 22748,		
		 -19235,	
		 22748,		
		 27995,		
		 -32688,	
		 2964,		
		 -2546,		
		 2964,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		27297,		
		 -32551,	
		 -345,		
		 0,		
		 345,		
		 27240,		
		 -32683,	
		 22560,		
		 -18688,	
		 22560,		
		 27461,		
		 -32684,	
		 3541,		
		 -2985,		
		 3541,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		27155,		
		 -32551,	
		 -462,		
		 0,		
		 462,		
		 27097,		
		 -32683,	
		 32495,		
		 -26776,	
		 32495,		
		 27321,		
		 -32684,	
		 1835,		
		 -1539,		
		 1835,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		19298,		
		 -24471,	
		 -4152,		
		 0,		
		 4152,		
		 12902,		
		 -29091,	
		 12491,		
		 -1794,		
		 12494,		
		 26291,		
		 -30470,	
		 28859,		
		 -26084,	
		 28861,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		26867,		
		 -32551,	
		 -123,		
		 0,		
		 123,		
		 26805,		
		 -32683,	
		 17297,		
		 -14096,	
		 17297,		
		 27034,		
		 -32684,	
		 12958,		
		 -10756,	
		 12958,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		26413,		
		 -32547,	
		 -223,		
		 0,		
		 223,		
		 26342,		
		 -32686,	
		 6391,		
		 -5120,		
		 6391,		
		 26593,		
		 -32688,	
		 23681,		
		 -19328,	
		 23681,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		26168,		
		 -32528,	
		 -235,		
		 0,		
		 235,		
		 26092,		
		 -32675,	
		 20823,		
		 -16510,	
		 20823,		
		 26363,		
		 -32677,	
		 6739,		
		 -5459,		
		 6739,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		25641,		
		 -32536,	
		 -121,		
		 0,		
		 121,		
		 25560,		
		 -32684,	
		 18341,		
		 -14252,	
		 18341,		
		 25837,		
		 -32684,	
		 16679,		
		 -13232,	
		 16679,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		16415,		
		 -23669,	
		 -4549,		
		 0,		
		 4549,		
		 8456,		
		 -28996,	
		 13753,		
		 -12,		
		 13757,		
		 24632,		
		 -30271,	
		 29070,		
		 -25265,	
		 29073,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		24806,		
		 -32501,	
		 -326,		
		 0,		
		 326,		
		 24709,		
		 -32659,	
		 20277,		
		 -15182,	
		 20277,		
		 25022,		
		 -32661,	
		 4320,		
		 -3331,		
		 4320,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		19776,		
		 -27437,	
		 -2666,		
		 0,		
		 2666,		
		 16302,		
		 -30354,	
		 10389,		
		 -3327,		
		 10389,		
		 24299,		
		 -30930,	
		 25016,		
		 -21171,	
		 25016,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		20554,		
		 -28764,	
		 -2048,		
		 0,		
		 2048,		
		 18209,		
		 -30951,	
		 9390,		
		 -3955,		
		 9390,		
		 23902,		
		 -31286,	
		 23252,		
		 -19132,	
		 23252,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		17543,		
		 -26220,	
		 -3298,		
		 0,		
		 3298,		
		 12423,		
		 -30036,	
		 12651,		
		 -2444,		
		 12653,		
		 23518,		
		 -30745,	
		 27282,		
		 -22529,	
		 27286,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		24104,		
		 -32507,	
		 -351,		
		 0,		
		 351,		
		 23996,		
		 -32671,	
		 22848,		
		 -16639,	
		 22848,		
		 24332,		
		 -32673,	
		 4906,		
		 -3672,		
		 4906,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		23967,		
		 -32507,	
		 -518,		
		 0,		
		 518,		
		 23856,		
		 -32671,	
		 26287,		
		 -19031,	
		 26287,		
		 24195,		
		 -32673,	
		 2890,		
		 -2151,		
		 2890,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		18294,		
		 -26962,	
		 -2914,		
		 0,		
		 2914,		
		 14119,		
		 -30227,	
		 11466,		
		 -2833,		
		 11466,		
		 23431,		
		 -30828,	
		 25331,		
		 -20911,	
		 25331,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		23521,		
		 -32489,	
		 -193,		
		 0,		
		 193,		
		 23404,		
		 -32655,	
		 17740,		
		 -12567,	
		 17740,		
		 23753,		
		 -32657,	
		 9090,		
		 -6662,		
		 9090,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		23071,		
		 -32489,	
		 -293,		
		 0,		
		 293,		
		 22951,		
		 -32655,	
		 5689,		
		 -3951,		
		 5689,		
		 23307,		
		 -32657,	
		 18692,		
		 -13447,	
		 18692,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		22701,		
		 -32474,	
		 -292,		
		 0,		
		 292,		
		 22564,		
		 -32655,	
		 20756,		
		 -14176,	
		 20756,		
		 22960,		
		 -32657,	
		 6520,		
		 -4619,		
		 6520,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		22142,		
		 -32474,	
		 -147,		
		 0,		
		 147,		
		 22000,		
		 -32655,	
		 15379,		
		 -10237,	
		 15379,		
		 22406,		
		 -32657,	
		 17491,		
		 -12096,	
		 17491,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		12973,		
		 -24916,	
		 6655,		
		 367,		
		 6657,		
		 5915,		
		 -29560,	
		 -7777,		
		 0,		
		 7777,		
		 20510,		
		 -30260,	
		 26662,		
		 -20573,	
		 26668,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		20392,		
		 -32460,	
		 -270,		
		 0,		
		 270,		
		 20218,		
		 -32655,	
		 21337,		
		 -13044,	
		 21337,		
		 20684,		
		 -32657,	
		 8572,		
		 -5476,		
		 8572,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		19159,		
		 -32456,	
		 -335,		
		 0,		
		 335,		
		 18966,		
		 -32661,	
		 6802,		
		 -3900,		
		 6802,		
		 19467,		
		 -32661,	
		 25035,		
		 -15049,	
		 25035,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		18976,		
		 -32439,	
		 -183,		
		 0,		
		 183,		
		 18774,		
		 -32650,	
		 15468,		
		 -8768,		
		 15468,		
		 19300,		
		 -32652,	
		 19840,		
		 -11842,	
		 19840,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		16357,		
		 -32368,	
		 -217,		
		 0,		
		 217,		
		 16107,		
		 -32601,	
		 11602,		
		 -5555,		
		 11602,		
		 16722,		
		 -32603,	
		 15574,		
		 -8176,		
		 15574,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		16234,		
		 32404,		
		 -193,		
		 0,		
		 193,		
		 15986,		
		 -32632,	
		 18051,		
		 -8658,		
		 18051,		
		 16591,		
		 -32634,	
		 15736,		
		 -8125,		
		 15736,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		15564,		
		 -32404,	
		 -269,		
		 0,		
		 269,		
		 15310,		
		 -32632,	
		 10815,		
		 -4962,		
		 10815,		
		 15924,		
		 -32634,	
		 18880,		
		 -9364,		
		 18880,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		15247,		
		 -32397,	
		 -244,		
		 0,		
		 244,		
		 14989,		
		 -32627,	
		 18961,		
		 -8498,		
		 18961,		
		 15608,		
		 -32628,	
		 11145,		
		 -5430,		
		 11145,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		14780,		
		 -32393,	
		 -396,		
		 0,		
		 396,		
		 14510,		
		 -32630,	
		 6326,		
		 -2747,		
		 6326,		
		 15154,		
		 -32632,	
		 23235,		
		 -10983,	
		 23235,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		13005,		
		 -32368,	
		 -500,		
		 0,		
		 500,		
		 12708,		
		 -32615,	
		 11420,		
		 -4306,		
		 11420,		
		 13397,		
		 -32615,	
		 9454,		
		 -3981,		
		 9454,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		10046,		
		 -32331,	
		 -455,		
		 0,		
		 455,		
		 9694,		
		 -32601,	
		 6023,		
		 -1708,		
		 6023,		
		 10478,		
		 -32603,	
		 22031,		
		 -7342,		
		 22031,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		9181,		
		 -32256,	
		 -556,		
		 0,		
		 556,		
		 8757,		
		 -32574,	
		 8443,		
		 -2135,		
		 8443,		
		 9691,		
		 -32574,	
		 15446,		
		 -4809,		
		 15446,		
		 7,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		5076,		
		 -32304,	
		 -508,		
		 0,		
		 508,		
		 4646,		
		 -32605,	
		 6742,		
		 -878,		
		 6742,		
		 5552,		
		 -32605,	
		 23667,		
		 -4297,		
		 23667,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
	{			
		3569,		
		 -32292,	
		 -239,		
		 0,		
		 239,		
		 3117,		
		 -32603,	
		 18658,		
		 -1557,		
		 18658,		
		 4054,		
		 -32603,	
		 18886,		
		 -2566,		
		 18886,		
		 5,		
		 159,		
		 21,		
		 0x0FF5		
	},
};
static int ixj_init_filter(IXJ *j, IXJ_FILTER * jf)
{
	unsigned short cmd;
	int cnt, max;

	if (jf->filter > 3) {
		return -1;
	}
	if (ixj_WriteDSPCommand(0x5154 + jf->filter, j))	

		return -1;
	if (!jf->enable) {
		if (ixj_WriteDSPCommand(0x5152, j))		

			return -1;
		else
			return 0;
	} else {
		if (ixj_WriteDSPCommand(0x5153, j))		

			return -1;
		
		if (ixj_WriteDSPCommand(0x5154 + jf->filter, j))
			return -1;
	}
	if (jf->freq < 12 && jf->freq > 3) {
		
		if (ixj_WriteDSPCommand(0x5170 + jf->freq, j))
			return -1;
	} else if (jf->freq > 11) {
		
		
		
		
		
		if (ixj_WriteDSPCommand(0x5170 + jf->filter, j))
			return -1;
		if (j->ver.low != 0x12) {
			cmd = 0x515B;
			max = 19;
		} else {
			cmd = 0x515E;
			max = 15;
		}
		if (ixj_WriteDSPCommand(cmd, j))
			return -1;
		for (cnt = 0; cnt < max; cnt++) {
			if (ixj_WriteDSPCommand(tone_table[jf->freq - 12][cnt], j))
				return -1;
		}
	}
	j->filter_en[jf->filter] = jf->enable;
	return 0;
}

static int ixj_init_filter_raw(IXJ *j, IXJ_FILTER_RAW * jfr)
{
	unsigned short cmd;
	int cnt, max;
	if (jfr->filter > 3) {
		return -1;
	}
	if (ixj_WriteDSPCommand(0x5154 + jfr->filter, j))	
		return -1;

	if (!jfr->enable) {
		if (ixj_WriteDSPCommand(0x5152, j))		
			return -1;
		else
			return 0;
	} else {
		if (ixj_WriteDSPCommand(0x5153, j))		
			return -1;
		
		if (ixj_WriteDSPCommand(0x5154 + jfr->filter, j))
			return -1;
	}
	
	
	
	
	
	if (ixj_WriteDSPCommand(0x5170 + jfr->filter, j))
		return -1;
	if (j->ver.low != 0x12) {
		cmd = 0x515B;
		max = 19;
	} else {
		cmd = 0x515E;
		max = 15;
	}
	if (ixj_WriteDSPCommand(cmd, j))
		return -1;
	for (cnt = 0; cnt < max; cnt++) {
		if (ixj_WriteDSPCommand(jfr->coeff[cnt], j))
			return -1;
	}
	j->filter_en[jfr->filter] = jfr->enable;
	return 0;
}

static int ixj_init_tone(IXJ *j, IXJ_TONE * ti)
{
	int freq0, freq1;
	unsigned short data;
	if (ti->freq0) {
		freq0 = ti->freq0;
	} else {
		freq0 = 0x7FFF;
	}

	if (ti->freq1) {
		freq1 = ti->freq1;
	} else {
		freq1 = 0x7FFF;
	}

	if(ti->tone_index > 12 && ti->tone_index < 28)
	{
		if (ixj_WriteDSPCommand(0x6800 + ti->tone_index, j))
			return -1;
		if (ixj_WriteDSPCommand(0x6000 + (ti->gain1 << 4) + ti->gain0, j))
			return -1;
		data = freq0;
		if (ixj_WriteDSPCommand(data, j))
			return -1;
		data = freq1;
		if (ixj_WriteDSPCommand(data, j))
			return -1;
	}
	return freq0;
}

