/*
 * dc395x.c
 *
 * Device Driver for Tekram DC395(U/UW/F), DC315(U)
 * PCI SCSI Bus Master Host Adapter
 * (SCSI chip set used Tekram ASIC TRM-S1040)
 *
 * Authors:
 *  C.L. Huang <ching@tekram.com.tw>
 *  Erich Chen <erich@tekram.com.tw>
 *  (C) Copyright 1995-1999 Tekram Technology Co., Ltd.
 *
 *  Kurt Garloff <garloff@suse.de>
 *  (C) 1999-2000 Kurt Garloff
 *
 *  Oliver Neukum <oliver@neukum.name>
 *  Ali Akcaagac <aliakc@web.de>
 *  Jamie Lenehan <lenehan@twibble.org>
 *  (C) 2003
 *
 * License: GNU GPL
 *
 *************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ************************************************************************
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/blkdev.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <asm/io.h>

#include <scsi/scsi.h>
#include <scsi/scsicam.h>	
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>

#include "dc395x.h"

#define DC395X_NAME	"dc395x"
#define DC395X_BANNER	"Tekram DC395(U/UW/F), DC315(U) - ASIC TRM-S1040"
#define DC395X_VERSION	"v2.05, 2004/03/08"


#define DBG_KG		0x0001
#define DBG_0		0x0002
#define DBG_1		0x0004
#define DBG_SG		0x0020
#define DBG_FIFO	0x0040
#define DBG_PIO		0x0080




#define dprintkl(level, format, arg...)  \
    printk(level DC395X_NAME ": " format , ## arg)


#ifdef DEBUG_MASK
#define dprintkdbg(type, format, arg...) \
	do { \
		if ((type) & (DEBUG_MASK)) \
			dprintkl(KERN_DEBUG , format , ## arg); \
	} while (0)

#define debug_enabled(type)	((DEBUG_MASK) & (type))

#else
#define dprintkdbg(type, format, arg...) \
	do {} while (0)
#define debug_enabled(type)	(0)

#endif


#ifndef PCI_VENDOR_ID_TEKRAM
#define PCI_VENDOR_ID_TEKRAM                    0x1DE1	
#endif
#ifndef PCI_DEVICE_ID_TEKRAM_TRMS1040
#define PCI_DEVICE_ID_TEKRAM_TRMS1040           0x0391	
#endif


#define DC395x_LOCK_IO(dev,flags)		spin_lock_irqsave(((struct Scsi_Host *)dev)->host_lock, flags)
#define DC395x_UNLOCK_IO(dev,flags)		spin_unlock_irqrestore(((struct Scsi_Host *)dev)->host_lock, flags)

#define DC395x_read8(acb,address)		(u8)(inb(acb->io_port_base + (address)))
#define DC395x_read16(acb,address)		(u16)(inw(acb->io_port_base + (address)))
#define DC395x_read32(acb,address)		(u32)(inl(acb->io_port_base + (address)))
#define DC395x_write8(acb,address,value)	outb((value), acb->io_port_base + (address))
#define DC395x_write16(acb,address,value)	outw((value), acb->io_port_base + (address))
#define DC395x_write32(acb,address,value)	outl((value), acb->io_port_base + (address))

#define RES_TARGET		0x000000FF	
#define RES_TARGET_LNX  STATUS_MASK	
#define RES_ENDMSG		0x0000FF00	
#define RES_DID			0x00FF0000	
#define RES_DRV			0xFF000000	

#define MK_RES(drv,did,msg,tgt) ((int)(drv)<<24 | (int)(did)<<16 | (int)(msg)<<8 | (int)(tgt))
#define MK_RES_LNX(drv,did,msg,tgt) ((int)(drv)<<24 | (int)(did)<<16 | (int)(msg)<<8 | (int)(tgt)<<1)

#define SET_RES_TARGET(who,tgt) { who &= ~RES_TARGET; who |= (int)(tgt); }
#define SET_RES_TARGET_LNX(who,tgt) { who &= ~RES_TARGET_LNX; who |= (int)(tgt) << 1; }
#define SET_RES_MSG(who,msg) { who &= ~RES_ENDMSG; who |= (int)(msg) << 8; }
#define SET_RES_DID(who,did) { who &= ~RES_DID; who |= (int)(did) << 16; }
#define SET_RES_DRV(who,drv) { who &= ~RES_DRV; who |= (int)(drv) << 24; }

#define TAG_NONE 255

#define SEGMENTX_LEN	(sizeof(struct SGentry)*DC395x_MAX_SG_LISTENTRY)


struct SGentry {
	u32 address;		
	u32 length;
};

struct NVRamTarget {
	u8 cfg0;		
	u8 period;		
	u8 cfg2;		
	u8 cfg3;		
};

struct NvRamType {
	u8 sub_vendor_id[2];	
	u8 sub_sys_id[2];	
	u8 sub_class;		
	u8 vendor_id[2];	
	u8 device_id[2];	
	u8 reserved;		
	struct NVRamTarget target[DC395x_MAX_SCSI_ID];
	u8 scsi_id;		
	u8 channel_cfg;		
	u8 delay_time;		
	u8 max_tag;		
	u8 reserved0;		
	u8 boot_target;		
	u8 boot_lun;		
	u8 reserved1;		
	u16 reserved2[22];	
	u16 cksum;		
};

struct ScsiReqBlk {
	struct list_head list;		
	struct DeviceCtlBlk *dcb;
	struct scsi_cmnd *cmd;

	struct SGentry *segment_x;	
	dma_addr_t sg_bus_addr;	        

	u8 sg_count;			
	u8 sg_index;			
	size_t total_xfer_length;	
	size_t request_length;		
	size_t xferred;		        

	u16 state;

	u8 msgin_buf[6];
	u8 msgout_buf[6];

	u8 adapter_status;
	u8 target_status;
	u8 msg_count;
	u8 end_message;

	u8 tag_number;
	u8 status;
	u8 retry_count;
	u8 flag;

	u8 scsi_phase;
};

struct DeviceCtlBlk {
	struct list_head list;		
	struct AdapterCtlBlk *acb;
	struct list_head srb_going_list;	
	struct list_head srb_waiting_list;	

	struct ScsiReqBlk *active_srb;
	u32 tag_mask;

	u16 max_command;

	u8 target_id;		
	u8 target_lun;		
	u8 identify_msg;
	u8 dev_mode;

	u8 inquiry7;		
	u8 sync_mode;		
	u8 min_nego_period;	
	u8 sync_period;		

	u8 sync_offset;		
	u8 flag;
	u8 dev_type;
	u8 init_tcq_flag;
};

struct AdapterCtlBlk {
	struct Scsi_Host *scsi_host;

	unsigned long io_port_base;
	unsigned long io_port_len;

	struct list_head dcb_list;		
	struct DeviceCtlBlk *dcb_run_robin;
	struct DeviceCtlBlk *active_dcb;

	struct list_head srb_free_list;		
	struct ScsiReqBlk *tmp_srb;
	struct timer_list waiting_timer;
	struct timer_list selto_timer;

	u16 srb_count;

	u8 sel_timeout;

	unsigned int irq_level;
	u8 tag_max_num;
	u8 acb_flag;
	u8 gmode2;

	u8 config;
	u8 lun_chk;
	u8 scan_devices;
	u8 hostid_bit;

	u8 dcb_map[DC395x_MAX_SCSI_ID];
	struct DeviceCtlBlk *children[DC395x_MAX_SCSI_ID][32];

	struct pci_dev *dev;

	u8 msg_len;

	struct ScsiReqBlk srb_array[DC395x_MAX_SRB_CNT];
	struct ScsiReqBlk srb;

	struct NvRamType eeprom;	
};


static void data_out_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void data_in_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void command_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void status_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void msgout_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void msgin_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void data_out_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void data_in_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void command_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void status_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void msgout_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void msgin_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void nop0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status);
static void nop1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb, 
		u16 *pscsi_status);
static void set_basic_config(struct AdapterCtlBlk *acb);
static void cleanup_after_transfer(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb);
static void reset_scsi_bus(struct AdapterCtlBlk *acb);
static void data_io_transfer(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb, u16 io_dir);
static void disconnect(struct AdapterCtlBlk *acb);
static void reselect(struct AdapterCtlBlk *acb);
static u8 start_scsi(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb);
static inline void enable_msgout_abort(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb);
static void build_srb(struct scsi_cmnd *cmd, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb);
static void doing_srb_done(struct AdapterCtlBlk *acb, u8 did_code,
		struct scsi_cmnd *cmd, u8 force);
static void scsi_reset_detect(struct AdapterCtlBlk *acb);
static void pci_unmap_srb(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb);
static void pci_unmap_srb_sense(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb);
static void srb_done(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb);
static void request_sense(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb);
static void set_xfer_rate(struct AdapterCtlBlk *acb,
		struct DeviceCtlBlk *dcb);
static void waiting_timeout(unsigned long ptr);


static u16 current_sync_offset = 0;

static void *dc395x_scsi_phase0[] = {
	data_out_phase0,
	data_in_phase0,	
	command_phase0,	
	status_phase0,	
	nop0,		
	nop0,		
	msgout_phase0,	
	msgin_phase0,	
};

static void *dc395x_scsi_phase1[] = {
	data_out_phase1,
	data_in_phase1,	
	command_phase1,	
	status_phase1,	
	nop1,		
	nop1,		
	msgout_phase1,	
	msgin_phase1,	
};


static u8 clock_period[] = { 12, 18, 25, 31, 37, 43, 50, 62 };
static u16 clock_speed[] = { 200, 133, 100, 80, 67, 58, 50, 40 };



#define CFG_ADAPTER_ID		0
#define CFG_MAX_SPEED		1
#define CFG_DEV_MODE		2
#define CFG_ADAPTER_MODE	3
#define CFG_TAGS		4
#define CFG_RESET_DELAY		5

#define CFG_NUM			6	


#define CFG_PARAM_UNSET -1


struct ParameterData {
	int value;		
	int min;		
	int max;		
	int def;		
	int safe;		
};
static struct ParameterData __devinitdata cfg_data[] = {
	{ 
		CFG_PARAM_UNSET,
		0,
		15,
		7,
		7
	},
	{ 
		CFG_PARAM_UNSET,
		  0,
		  7,
		  1,	
		  4,	
	},
	{ 
		CFG_PARAM_UNSET,
		0,
		0x3f,
		NTC_DO_PARITY_CHK | NTC_DO_DISCONNECT | NTC_DO_SYNC_NEGO |
			NTC_DO_WIDE_NEGO | NTC_DO_TAG_QUEUEING |
			NTC_DO_SEND_START,
		NTC_DO_PARITY_CHK | NTC_DO_SEND_START
	},
	{ 
		CFG_PARAM_UNSET,
		0,
		0x2f,
#ifdef CONFIG_SCSI_MULTI_LUN
			NAC_SCANLUN |
#endif
		NAC_GT2DRIVES | NAC_GREATER_1G | NAC_POWERON_SCSI_RESET
			,
		NAC_GT2DRIVES | NAC_GREATER_1G | NAC_POWERON_SCSI_RESET | 0x08
	},
	{ 
		CFG_PARAM_UNSET,
		0,
		5,
		3,	
		2,
	},
	{ 
		CFG_PARAM_UNSET,
		0,
		180,
		1,	
		10,	
	}
};


static bool use_safe_settings = 0;
module_param_named(safe, use_safe_settings, bool, 0);
MODULE_PARM_DESC(safe, "Use safe and slow settings only. Default: false");


module_param_named(adapter_id, cfg_data[CFG_ADAPTER_ID].value, int, 0);
MODULE_PARM_DESC(adapter_id, "Adapter SCSI ID. Default 7 (0-15)");

module_param_named(max_speed, cfg_data[CFG_MAX_SPEED].value, int, 0);
MODULE_PARM_DESC(max_speed, "Maximum bus speed. Default 1 (0-7) Speeds: 0=20, 1=13.3, 2=10, 3=8, 4=6.7, 5=5.8, 6=5, 7=4 Mhz");

module_param_named(dev_mode, cfg_data[CFG_DEV_MODE].value, int, 0);
MODULE_PARM_DESC(dev_mode, "Device mode.");

module_param_named(adapter_mode, cfg_data[CFG_ADAPTER_MODE].value, int, 0);
MODULE_PARM_DESC(adapter_mode, "Adapter mode.");

module_param_named(tags, cfg_data[CFG_TAGS].value, int, 0);
MODULE_PARM_DESC(tags, "Number of tags (1<<x). Default 3 (0-5)");

module_param_named(reset_delay, cfg_data[CFG_RESET_DELAY].value, int, 0);
MODULE_PARM_DESC(reset_delay, "Reset delay in seconds. Default 1 (0-180)");


static void __devinit set_safe_settings(void)
{
	if (use_safe_settings)
	{
		int i;

		dprintkl(KERN_INFO, "Using safe settings.\n");
		for (i = 0; i < CFG_NUM; i++)
		{
			cfg_data[i].value = cfg_data[i].safe;
		}
	}
}


static void __devinit fix_settings(void)
{
	int i;

	dprintkdbg(DBG_1,
		"setup: AdapterId=%08x MaxSpeed=%08x DevMode=%08x "
		"AdapterMode=%08x Tags=%08x ResetDelay=%08x\n",
		cfg_data[CFG_ADAPTER_ID].value,
		cfg_data[CFG_MAX_SPEED].value,
		cfg_data[CFG_DEV_MODE].value,
		cfg_data[CFG_ADAPTER_MODE].value,
		cfg_data[CFG_TAGS].value,
		cfg_data[CFG_RESET_DELAY].value);
	for (i = 0; i < CFG_NUM; i++)
	{
		if (cfg_data[i].value < cfg_data[i].min
		    || cfg_data[i].value > cfg_data[i].max)
			cfg_data[i].value = cfg_data[i].def;
	}
}



static char __devinitdata eeprom_index_to_delay_map[] = 
	{ 1, 3, 5, 10, 16, 30, 60, 120 };


static void __devinit eeprom_index_to_delay(struct NvRamType *eeprom)
{
	eeprom->delay_time = eeprom_index_to_delay_map[eeprom->delay_time];
}


static int __devinit delay_to_eeprom_index(int delay)
{
	u8 idx = 0;
	while (idx < 7 && eeprom_index_to_delay_map[idx] < delay)
		idx++;
	return idx;
}


static void __devinit eeprom_override(struct NvRamType *eeprom)
{
	u8 id;

	
	if (cfg_data[CFG_ADAPTER_ID].value != CFG_PARAM_UNSET)
		eeprom->scsi_id = (u8)cfg_data[CFG_ADAPTER_ID].value;

	if (cfg_data[CFG_ADAPTER_MODE].value != CFG_PARAM_UNSET)
		eeprom->channel_cfg = (u8)cfg_data[CFG_ADAPTER_MODE].value;

	if (cfg_data[CFG_RESET_DELAY].value != CFG_PARAM_UNSET)
		eeprom->delay_time = delay_to_eeprom_index(
					cfg_data[CFG_RESET_DELAY].value);

	if (cfg_data[CFG_TAGS].value != CFG_PARAM_UNSET)
		eeprom->max_tag = (u8)cfg_data[CFG_TAGS].value;

	
	for (id = 0; id < DC395x_MAX_SCSI_ID; id++) {
		if (cfg_data[CFG_DEV_MODE].value != CFG_PARAM_UNSET)
			eeprom->target[id].cfg0 =
				(u8)cfg_data[CFG_DEV_MODE].value;

		if (cfg_data[CFG_MAX_SPEED].value != CFG_PARAM_UNSET)
			eeprom->target[id].period =
				(u8)cfg_data[CFG_MAX_SPEED].value;

	}
}



static unsigned int list_size(struct list_head *head)
{
	unsigned int count = 0;
	struct list_head *pos;
	list_for_each(pos, head)
		count++;
	return count;
}


static struct DeviceCtlBlk *dcb_get_next(struct list_head *head,
		struct DeviceCtlBlk *pos)
{
	int use_next = 0;
	struct DeviceCtlBlk* next = NULL;
	struct DeviceCtlBlk* i;

	if (list_empty(head))
		return NULL;

	
	list_for_each_entry(i, head, list)
		if (use_next) {
			next = i;
			break;
		} else if (i == pos) {
			use_next = 1;
		}
	
	if (!next)
        	list_for_each_entry(i, head, list) {
        		next = i;
        		break;
        	}

	return next;
}


static void free_tag(struct DeviceCtlBlk *dcb, struct ScsiReqBlk *srb)
{
	if (srb->tag_number < 255) {
		dcb->tag_mask &= ~(1 << srb->tag_number);	
		srb->tag_number = 255;
	}
}


static inline struct ScsiReqBlk *find_cmd(struct scsi_cmnd *cmd,
		struct list_head *head)
{
	struct ScsiReqBlk *i;
	list_for_each_entry(i, head, list)
		if (i->cmd == cmd)
			return i;
	return NULL;
}


static struct ScsiReqBlk *srb_get_free(struct AdapterCtlBlk *acb)
{
	struct list_head *head = &acb->srb_free_list;
	struct ScsiReqBlk *srb = NULL;

	if (!list_empty(head)) {
		srb = list_entry(head->next, struct ScsiReqBlk, list);
		list_del(head->next);
		dprintkdbg(DBG_0, "srb_get_free: srb=%p\n", srb);
	}
	return srb;
}


static void srb_free_insert(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0, "srb_free_insert: srb=%p\n", srb);
	list_add_tail(&srb->list, &acb->srb_free_list);
}


static void srb_waiting_insert(struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0, "srb_waiting_insert: (0x%p) <%02i-%i> srb=%p\n",
		srb->cmd, dcb->target_id, dcb->target_lun, srb);
	list_add(&srb->list, &dcb->srb_waiting_list);
}


static void srb_waiting_append(struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0, "srb_waiting_append: (0x%p) <%02i-%i> srb=%p\n",
		 srb->cmd, dcb->target_id, dcb->target_lun, srb);
	list_add_tail(&srb->list, &dcb->srb_waiting_list);
}


static void srb_going_append(struct DeviceCtlBlk *dcb, struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0, "srb_going_append: (0x%p) <%02i-%i> srb=%p\n",
		srb->cmd, dcb->target_id, dcb->target_lun, srb);
	list_add_tail(&srb->list, &dcb->srb_going_list);
}


static void srb_going_remove(struct DeviceCtlBlk *dcb, struct ScsiReqBlk *srb)
{
	struct ScsiReqBlk *i;
	struct ScsiReqBlk *tmp;
	dprintkdbg(DBG_0, "srb_going_remove: (0x%p) <%02i-%i> srb=%p\n",
		srb->cmd, dcb->target_id, dcb->target_lun, srb);

	list_for_each_entry_safe(i, tmp, &dcb->srb_going_list, list)
		if (i == srb) {
			list_del(&srb->list);
			break;
		}
}


static void srb_waiting_remove(struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	struct ScsiReqBlk *i;
	struct ScsiReqBlk *tmp;
	dprintkdbg(DBG_0, "srb_waiting_remove: (0x%p) <%02i-%i> srb=%p\n",
		srb->cmd, dcb->target_id, dcb->target_lun, srb);

	list_for_each_entry_safe(i, tmp, &dcb->srb_waiting_list, list)
		if (i == srb) {
			list_del(&srb->list);
			break;
		}
}


static void srb_going_to_waiting_move(struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0,
		"srb_going_to_waiting_move: (0x%p) <%02i-%i> srb=%p\n",
		srb->cmd, dcb->target_id, dcb->target_lun, srb);
	list_move(&srb->list, &dcb->srb_waiting_list);
}


static void srb_waiting_to_going_move(struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	dprintkdbg(DBG_0,
		"srb_waiting_to_going_move: (0x%p) <%02i-%i> srb=%p\n",
		srb->cmd, dcb->target_id, dcb->target_lun, srb);
	list_move(&srb->list, &dcb->srb_going_list);
}


static void waiting_set_timer(struct AdapterCtlBlk *acb, unsigned long to)
{
	if (timer_pending(&acb->waiting_timer))
		return;
	init_timer(&acb->waiting_timer);
	acb->waiting_timer.function = waiting_timeout;
	acb->waiting_timer.data = (unsigned long) acb;
	if (time_before(jiffies + to, acb->scsi_host->last_reset - HZ / 2))
		acb->waiting_timer.expires =
		    acb->scsi_host->last_reset - HZ / 2 + 1;
	else
		acb->waiting_timer.expires = jiffies + to + 1;
	add_timer(&acb->waiting_timer);
}


static void waiting_process_next(struct AdapterCtlBlk *acb)
{
	struct DeviceCtlBlk *start = NULL;
	struct DeviceCtlBlk *pos;
	struct DeviceCtlBlk *dcb;
	struct ScsiReqBlk *srb;
	struct list_head *dcb_list_head = &acb->dcb_list;

	if (acb->active_dcb
	    || (acb->acb_flag & (RESET_DETECT + RESET_DONE + RESET_DEV)))
		return;

	if (timer_pending(&acb->waiting_timer))
		del_timer(&acb->waiting_timer);

	if (list_empty(dcb_list_head))
		return;

	list_for_each_entry(dcb, dcb_list_head, list)
		if (dcb == acb->dcb_run_robin) {
			start = dcb;
			break;
		}
	if (!start) {
		
		start = list_entry(dcb_list_head->next, typeof(*start), list);
		acb->dcb_run_robin = start;
	}


	pos = start;
	do {
		struct list_head *waiting_list_head = &pos->srb_waiting_list;

		
		acb->dcb_run_robin = dcb_get_next(dcb_list_head,
						  acb->dcb_run_robin);

		if (list_empty(waiting_list_head) ||
		    pos->max_command <= list_size(&pos->srb_going_list)) {
			
			pos = dcb_get_next(dcb_list_head, pos);
		} else {
			srb = list_entry(waiting_list_head->next,
					 struct ScsiReqBlk, list);

			
			if (!start_scsi(acb, pos, srb))
				srb_waiting_to_going_move(pos, srb);
			else
				waiting_set_timer(acb, HZ/50);
			break;
		}
	} while (pos != start);
}


static void waiting_timeout(unsigned long ptr)
{
	unsigned long flags;
	struct AdapterCtlBlk *acb = (struct AdapterCtlBlk *)ptr;
	dprintkdbg(DBG_1,
		"waiting_timeout: Queue woken up by timer. acb=%p\n", acb);
	DC395x_LOCK_IO(acb->scsi_host, flags);
	waiting_process_next(acb);
	DC395x_UNLOCK_IO(acb->scsi_host, flags);
}


static struct DeviceCtlBlk *find_dcb(struct AdapterCtlBlk *acb, u8 id, u8 lun)
{
	return acb->children[id][lun];
}


static void send_srb(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	struct DeviceCtlBlk *dcb = srb->dcb;

	if (dcb->max_command <= list_size(&dcb->srb_going_list) ||
	    acb->active_dcb ||
	    (acb->acb_flag & (RESET_DETECT + RESET_DONE + RESET_DEV))) {
		srb_waiting_append(dcb, srb);
		waiting_process_next(acb);
		return;
	}

	if (!start_scsi(acb, dcb, srb))
		srb_going_append(dcb, srb);
	else {
		srb_waiting_insert(dcb, srb);
		waiting_set_timer(acb, HZ / 50);
	}
}

static void build_srb(struct scsi_cmnd *cmd, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	int nseg;
	enum dma_data_direction dir = cmd->sc_data_direction;
	dprintkdbg(DBG_0, "build_srb: (0x%p) <%02i-%i>\n",
		cmd, dcb->target_id, dcb->target_lun);

	srb->dcb = dcb;
	srb->cmd = cmd;
	srb->sg_count = 0;
	srb->total_xfer_length = 0;
	srb->sg_bus_addr = 0;
	srb->sg_index = 0;
	srb->adapter_status = 0;
	srb->target_status = 0;
	srb->msg_count = 0;
	srb->status = 0;
	srb->flag = 0;
	srb->state = 0;
	srb->retry_count = 0;
	srb->tag_number = TAG_NONE;
	srb->scsi_phase = PH_BUS_FREE;	
	srb->end_message = 0;

	nseg = scsi_dma_map(cmd);
	BUG_ON(nseg < 0);

	if (dir == PCI_DMA_NONE || !nseg) {
		dprintkdbg(DBG_0,
			"build_srb: [0] len=%d buf=%p use_sg=%d !MAP=%08x\n",
			   cmd->bufflen, scsi_sglist(cmd), scsi_sg_count(cmd),
			   srb->segment_x[0].address);
	} else {
		int i;
		u32 reqlen = scsi_bufflen(cmd);
		struct scatterlist *sg;
		struct SGentry *sgp = srb->segment_x;

		srb->sg_count = nseg;

		dprintkdbg(DBG_0,
			   "build_srb: [n] len=%d buf=%p use_sg=%d segs=%d\n",
			   reqlen, scsi_sglist(cmd), scsi_sg_count(cmd),
			   srb->sg_count);

		scsi_for_each_sg(cmd, sg, srb->sg_count, i) {
			u32 busaddr = (u32)sg_dma_address(sg);
			u32 seglen = (u32)sg->length;
			sgp[i].address = busaddr;
			sgp[i].length = seglen;
			srb->total_xfer_length += seglen;
		}
		sgp += srb->sg_count - 1;

		if (srb->total_xfer_length > reqlen) {
			sgp->length -= (srb->total_xfer_length - reqlen);
			srb->total_xfer_length = reqlen;
		}

		
		if (dcb->sync_period & WIDE_SYNC &&
		    srb->total_xfer_length % 2) {
			srb->total_xfer_length++;
			sgp->length++;
		}

		srb->sg_bus_addr = pci_map_single(dcb->acb->dev,
						srb->segment_x,
				            	SEGMENTX_LEN,
				            	PCI_DMA_TODEVICE);

		dprintkdbg(DBG_SG, "build_srb: [n] map sg %p->%08x(%05x)\n",
			srb->segment_x, srb->sg_bus_addr, SEGMENTX_LEN);
	}

	srb->request_length = srb->total_xfer_length;
}


static int dc395x_queue_command_lck(struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
{
	struct DeviceCtlBlk *dcb;
	struct ScsiReqBlk *srb;
	struct AdapterCtlBlk *acb =
	    (struct AdapterCtlBlk *)cmd->device->host->hostdata;
	dprintkdbg(DBG_0, "queue_command: (0x%p) <%02i-%i> cmnd=0x%02x\n",
		cmd, cmd->device->id, cmd->device->lun, cmd->cmnd[0]);

	
	cmd->result = DID_BAD_TARGET << 16;

	
	if (cmd->device->id >= acb->scsi_host->max_id ||
	    cmd->device->lun >= acb->scsi_host->max_lun ||
	    cmd->device->lun >31) {
		goto complete;
	}

	
	if (!(acb->dcb_map[cmd->device->id] & (1 << cmd->device->lun))) {
		dprintkl(KERN_INFO, "queue_command: Ignore target <%02i-%i>\n",
			cmd->device->id, cmd->device->lun);
		goto complete;
	}

	
	dcb = find_dcb(acb, cmd->device->id, cmd->device->lun);
	if (!dcb) {
		
		dprintkl(KERN_ERR, "queue_command: No such device <%02i-%i>",
			cmd->device->id, cmd->device->lun);
		goto complete;
	}

	
	cmd->scsi_done = done;
	cmd->result = 0;

	srb = srb_get_free(acb);
	if (!srb)
	{
		dprintkdbg(DBG_0, "queue_command: No free srb's\n");
		return 1;
	}

	build_srb(cmd, dcb, srb);

	if (!list_empty(&dcb->srb_waiting_list)) {
		
		srb_waiting_append(dcb, srb);
		waiting_process_next(acb);
	} else {
		
		send_srb(acb, srb);
	}
	dprintkdbg(DBG_1, "queue_command: (0x%p) done\n", cmd);
	return 0;

complete:
	done(cmd);
	return 0;
}

static DEF_SCSI_QCMD(dc395x_queue_command)

static int dc395x_bios_param(struct scsi_device *sdev,
		struct block_device *bdev, sector_t capacity, int *info)
{
#ifdef CONFIG_SCSI_DC395x_TRMS1040_TRADMAP
	int heads, sectors, cylinders;
	struct AdapterCtlBlk *acb;
	int size = capacity;

	dprintkdbg(DBG_0, "dc395x_bios_param..............\n");
	acb = (struct AdapterCtlBlk *)sdev->host->hostdata;
	heads = 64;
	sectors = 32;
	cylinders = size / (heads * sectors);

	if ((acb->gmode2 & NAC_GREATER_1G) && (cylinders > 1024)) {
		heads = 255;
		sectors = 63;
		cylinders = size / (heads * sectors);
	}
	geom[0] = heads;
	geom[1] = sectors;
	geom[2] = cylinders;
	return 0;
#else
	return scsicam_bios_param(bdev, capacity, info);
#endif
}


static void dump_register_info(struct AdapterCtlBlk *acb,
		struct DeviceCtlBlk *dcb, struct ScsiReqBlk *srb)
{
	u16 pstat;
	struct pci_dev *dev = acb->dev;
	pci_read_config_word(dev, PCI_STATUS, &pstat);
	if (!dcb)
		dcb = acb->active_dcb;
	if (!srb && dcb)
		srb = dcb->active_srb;
	if (srb) {
		if (!srb->cmd)
			dprintkl(KERN_INFO, "dump: srb=%p cmd=%p OOOPS!\n",
				srb, srb->cmd);
		else
			dprintkl(KERN_INFO, "dump: srb=%p cmd=%p "
				 "cmnd=0x%02x <%02i-%i>\n",
				srb, srb->cmd,
				srb->cmd->cmnd[0], srb->cmd->device->id,
			       	srb->cmd->device->lun);
		printk("  sglist=%p cnt=%i idx=%i len=%zu\n",
		       srb->segment_x, srb->sg_count, srb->sg_index,
		       srb->total_xfer_length);
		printk("  state=0x%04x status=0x%02x phase=0x%02x (%sconn.)\n",
		       srb->state, srb->status, srb->scsi_phase,
		       (acb->active_dcb) ? "" : "not");
	}
	dprintkl(KERN_INFO, "dump: SCSI{status=0x%04x fifocnt=0x%02x "
		"signals=0x%02x irqstat=0x%02x sync=0x%02x target=0x%02x "
		"rselid=0x%02x ctr=0x%08x irqen=0x%02x config=0x%04x "
		"config2=0x%02x cmd=0x%02x selto=0x%02x}\n",
		DC395x_read16(acb, TRM_S1040_SCSI_STATUS),
		DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT),
		DC395x_read8(acb, TRM_S1040_SCSI_SIGNAL),
		DC395x_read8(acb, TRM_S1040_SCSI_INTSTATUS),
		DC395x_read8(acb, TRM_S1040_SCSI_SYNC),
		DC395x_read8(acb, TRM_S1040_SCSI_TARGETID),
		DC395x_read8(acb, TRM_S1040_SCSI_IDMSG),
		DC395x_read32(acb, TRM_S1040_SCSI_COUNTER),
		DC395x_read8(acb, TRM_S1040_SCSI_INTEN),
		DC395x_read16(acb, TRM_S1040_SCSI_CONFIG0),
		DC395x_read8(acb, TRM_S1040_SCSI_CONFIG2),
		DC395x_read8(acb, TRM_S1040_SCSI_COMMAND),
		DC395x_read8(acb, TRM_S1040_SCSI_TIMEOUT));
	dprintkl(KERN_INFO, "dump: DMA{cmd=0x%04x fifocnt=0x%02x fstat=0x%02x "
		"irqstat=0x%02x irqen=0x%02x cfg=0x%04x tctr=0x%08x "
		"ctctr=0x%08x addr=0x%08x:0x%08x}\n",
		DC395x_read16(acb, TRM_S1040_DMA_COMMAND),
		DC395x_read8(acb, TRM_S1040_DMA_FIFOCNT),
		DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT),
		DC395x_read8(acb, TRM_S1040_DMA_STATUS),
		DC395x_read8(acb, TRM_S1040_DMA_INTEN),
		DC395x_read16(acb, TRM_S1040_DMA_CONFIG),
		DC395x_read32(acb, TRM_S1040_DMA_XCNT),
		DC395x_read32(acb, TRM_S1040_DMA_CXCNT),
		DC395x_read32(acb, TRM_S1040_DMA_XHIGHADDR),
		DC395x_read32(acb, TRM_S1040_DMA_XLOWADDR));
	dprintkl(KERN_INFO, "dump: gen{gctrl=0x%02x gstat=0x%02x gtmr=0x%02x} "
		"pci{status=0x%04x}\n",
		DC395x_read8(acb, TRM_S1040_GEN_CONTROL),
		DC395x_read8(acb, TRM_S1040_GEN_STATUS),
		DC395x_read8(acb, TRM_S1040_GEN_TIMER),
		pstat);
}


static inline void clear_fifo(struct AdapterCtlBlk *acb, char *txt)
{
#if debug_enabled(DBG_FIFO)
	u8 lines = DC395x_read8(acb, TRM_S1040_SCSI_SIGNAL);
	u8 fifocnt = DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT);
	if (!(fifocnt & 0x40))
		dprintkdbg(DBG_FIFO,
			"clear_fifo: (%i bytes) on phase %02x in %s\n",
			fifocnt & 0x3f, lines, txt);
#endif
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_CLRFIFO);
}


static void reset_dev_param(struct AdapterCtlBlk *acb)
{
	struct DeviceCtlBlk *dcb;
	struct NvRamType *eeprom = &acb->eeprom;
	dprintkdbg(DBG_0, "reset_dev_param: acb=%p\n", acb);

	list_for_each_entry(dcb, &acb->dcb_list, list) {
		u8 period_index;

		dcb->sync_mode &= ~(SYNC_NEGO_DONE + WIDE_NEGO_DONE);
		dcb->sync_period = 0;
		dcb->sync_offset = 0;

		dcb->dev_mode = eeprom->target[dcb->target_id].cfg0;
		period_index = eeprom->target[dcb->target_id].period & 0x07;
		dcb->min_nego_period = clock_period[period_index];
		if (!(dcb->dev_mode & NTC_DO_WIDE_NEGO)
		    || !(acb->config & HCC_WIDE_CARD))
			dcb->sync_mode &= ~WIDE_NEGO_ENABLE;
	}
}


static int __dc395x_eh_bus_reset(struct scsi_cmnd *cmd)
{
	struct AdapterCtlBlk *acb =
		(struct AdapterCtlBlk *)cmd->device->host->hostdata;
	dprintkl(KERN_INFO,
		"eh_bus_reset: (0%p) target=<%02i-%i> cmd=%p\n",
		cmd, cmd->device->id, cmd->device->lun, cmd);

	if (timer_pending(&acb->waiting_timer))
		del_timer(&acb->waiting_timer);

	DC395x_write8(acb, TRM_S1040_DMA_INTEN, 0x00);
	DC395x_write8(acb, TRM_S1040_SCSI_INTEN, 0x00);
	DC395x_write8(acb, TRM_S1040_SCSI_CONTROL, DO_RSTMODULE);
	DC395x_write8(acb, TRM_S1040_DMA_CONTROL, DMARESETMODULE);

	reset_scsi_bus(acb);
	udelay(500);

	
	acb->scsi_host->last_reset =
	    jiffies + 3 * HZ / 2 +
	    HZ * acb->eeprom.delay_time;

	
	DC395x_write8(acb, TRM_S1040_DMA_CONTROL, CLRXFIFO);
	clear_fifo(acb, "eh_bus_reset");
	
	DC395x_read8(acb, TRM_S1040_SCSI_INTSTATUS);
	set_basic_config(acb);

	reset_dev_param(acb);
	doing_srb_done(acb, DID_RESET, cmd, 0);
	acb->active_dcb = NULL;
	acb->acb_flag = 0;	
	waiting_process_next(acb);

	return SUCCESS;
}

static int dc395x_eh_bus_reset(struct scsi_cmnd *cmd)
{
	int rc;

	spin_lock_irq(cmd->device->host->host_lock);
	rc = __dc395x_eh_bus_reset(cmd);
	spin_unlock_irq(cmd->device->host->host_lock);

	return rc;
}

static int dc395x_eh_abort(struct scsi_cmnd *cmd)
{
	struct AdapterCtlBlk *acb =
	    (struct AdapterCtlBlk *)cmd->device->host->hostdata;
	struct DeviceCtlBlk *dcb;
	struct ScsiReqBlk *srb;
	dprintkl(KERN_INFO, "eh_abort: (0x%p) target=<%02i-%i> cmd=%p\n",
		cmd, cmd->device->id, cmd->device->lun, cmd);

	dcb = find_dcb(acb, cmd->device->id, cmd->device->lun);
	if (!dcb) {
		dprintkl(KERN_DEBUG, "eh_abort: No such device\n");
		return FAILED;
	}

	srb = find_cmd(cmd, &dcb->srb_waiting_list);
	if (srb) {
		srb_waiting_remove(dcb, srb);
		pci_unmap_srb_sense(acb, srb);
		pci_unmap_srb(acb, srb);
		free_tag(dcb, srb);
		srb_free_insert(acb, srb);
		dprintkl(KERN_DEBUG, "eh_abort: Command was waiting\n");
		cmd->result = DID_ABORT << 16;
		return SUCCESS;
	}
	srb = find_cmd(cmd, &dcb->srb_going_list);
	if (srb) {
		dprintkl(KERN_DEBUG, "eh_abort: Command in progress\n");
		
	} else {
		dprintkl(KERN_DEBUG, "eh_abort: Command not found\n");
	}
	return FAILED;
}


static void build_sdtr(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	u8 *ptr = srb->msgout_buf + srb->msg_count;
	if (srb->msg_count > 1) {
		dprintkl(KERN_INFO,
			"build_sdtr: msgout_buf BUSY (%i: %02x %02x)\n",
			srb->msg_count, srb->msgout_buf[0],
			srb->msgout_buf[1]);
		return;
	}
	if (!(dcb->dev_mode & NTC_DO_SYNC_NEGO)) {
		dcb->sync_offset = 0;
		dcb->min_nego_period = 200 >> 2;
	} else if (dcb->sync_offset == 0)
		dcb->sync_offset = SYNC_NEGO_OFFSET;

	*ptr++ = MSG_EXTENDED;	
	*ptr++ = 3;		
	*ptr++ = EXTENDED_SDTR;	
	*ptr++ = dcb->min_nego_period;	
	*ptr++ = dcb->sync_offset;	
	srb->msg_count += 5;
	srb->state |= SRB_DO_SYNC_NEGO;
}


static void build_wdtr(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	u8 wide = ((dcb->dev_mode & NTC_DO_WIDE_NEGO) &
		   (acb->config & HCC_WIDE_CARD)) ? 1 : 0;
	u8 *ptr = srb->msgout_buf + srb->msg_count;
	if (srb->msg_count > 1) {
		dprintkl(KERN_INFO,
			"build_wdtr: msgout_buf BUSY (%i: %02x %02x)\n",
			srb->msg_count, srb->msgout_buf[0],
			srb->msgout_buf[1]);
		return;
	}
	*ptr++ = MSG_EXTENDED;	
	*ptr++ = 2;		
	*ptr++ = EXTENDED_WDTR;	
	*ptr++ = wide;
	srb->msg_count += 4;
	srb->state |= SRB_DO_WIDE_NEGO;
}


#if 0
void selection_timeout_missed(unsigned long ptr);
static void selto_timer(struct AdapterCtlBlk *acb)
{
	if (timer_pending(&acb->selto_timer))
		return;
	acb->selto_timer.function = selection_timeout_missed;
	acb->selto_timer.data = (unsigned long) acb;
	if (time_before
	    (jiffies + HZ, acb->scsi_host->last_reset + HZ / 2))
		acb->selto_timer.expires =
		    acb->scsi_host->last_reset + HZ / 2 + 1;
	else
		acb->selto_timer.expires = jiffies + HZ + 1;
	add_timer(&acb->selto_timer);
}


void selection_timeout_missed(unsigned long ptr)
{
	unsigned long flags;
	struct AdapterCtlBlk *acb = (struct AdapterCtlBlk *)ptr;
	struct ScsiReqBlk *srb;
	dprintkl(KERN_DEBUG, "Chip forgot to produce SelTO IRQ!\n");
	if (!acb->active_dcb || !acb->active_dcb->active_srb) {
		dprintkl(KERN_DEBUG, "... but no cmd pending? Oops!\n");
		return;
	}
	DC395x_LOCK_IO(acb->scsi_host, flags);
	srb = acb->active_dcb->active_srb;
	disconnect(acb);
	DC395x_UNLOCK_IO(acb->scsi_host, flags);
}
#endif


static u8 start_scsi(struct AdapterCtlBlk* acb, struct DeviceCtlBlk* dcb,
		struct ScsiReqBlk* srb)
{
	u16 s_stat2, return_code;
	u8 s_stat, scsicommand, i, identify_message;
	u8 *ptr;
	dprintkdbg(DBG_0, "start_scsi: (0x%p) <%02i-%i> srb=%p\n",
		dcb->target_id, dcb->target_lun, srb);

	srb->tag_number = TAG_NONE;	

	s_stat = DC395x_read8(acb, TRM_S1040_SCSI_SIGNAL);
	s_stat2 = 0;
	s_stat2 = DC395x_read16(acb, TRM_S1040_SCSI_STATUS);
#if 1
	if (s_stat & 0x20  ) {
		dprintkdbg(DBG_KG, "start_scsi: (0x%p) BUSY %02x %04x\n",
			s_stat, s_stat2);
		
		return 1;
	}
#endif
	if (acb->active_dcb) {
		dprintkl(KERN_DEBUG, "start_scsi: (0x%p) Attempt to start a"
			"command while another command (0x%p) is active.",
			srb->cmd,
			acb->active_dcb->active_srb ?
			    acb->active_dcb->active_srb->cmd : 0);
		return 1;
	}
	if (DC395x_read16(acb, TRM_S1040_SCSI_STATUS) & SCSIINTERRUPT) {
		dprintkdbg(DBG_KG, "start_scsi: (0x%p) Failed (busy)\n", srb->cmd);
		return 1;
	}
	if (time_before(jiffies, acb->scsi_host->last_reset - HZ / 2)) {
		dprintkdbg(DBG_KG, "start_scsi: Refuse cmds (reset wait)\n");
		return 1;
	}

	
	clear_fifo(acb, "start_scsi");
	DC395x_write8(acb, TRM_S1040_SCSI_HOSTID, acb->scsi_host->this_id);
	DC395x_write8(acb, TRM_S1040_SCSI_TARGETID, dcb->target_id);
	DC395x_write8(acb, TRM_S1040_SCSI_SYNC, dcb->sync_period);
	DC395x_write8(acb, TRM_S1040_SCSI_OFFSET, dcb->sync_offset);
	srb->scsi_phase = PH_BUS_FREE;	

	identify_message = dcb->identify_msg;
	
	
	if (srb->flag & AUTO_REQSENSE)
		identify_message &= 0xBF;

	if (((srb->cmd->cmnd[0] == INQUIRY)
	     || (srb->cmd->cmnd[0] == REQUEST_SENSE)
	     || (srb->flag & AUTO_REQSENSE))
	    && (((dcb->sync_mode & WIDE_NEGO_ENABLE)
		 && !(dcb->sync_mode & WIDE_NEGO_DONE))
		|| ((dcb->sync_mode & SYNC_NEGO_ENABLE)
		    && !(dcb->sync_mode & SYNC_NEGO_DONE)))
	    && (dcb->target_lun == 0)) {
		srb->msgout_buf[0] = identify_message;
		srb->msg_count = 1;
		scsicommand = SCMD_SEL_ATNSTOP;
		srb->state = SRB_MSGOUT;
#ifndef SYNC_FIRST
		if (dcb->sync_mode & WIDE_NEGO_ENABLE
		    && dcb->inquiry7 & SCSI_INQ_WBUS16) {
			build_wdtr(acb, dcb, srb);
			goto no_cmd;
		}
#endif
		if (dcb->sync_mode & SYNC_NEGO_ENABLE
		    && dcb->inquiry7 & SCSI_INQ_SYNC) {
			build_sdtr(acb, dcb, srb);
			goto no_cmd;
		}
		if (dcb->sync_mode & WIDE_NEGO_ENABLE
		    && dcb->inquiry7 & SCSI_INQ_WBUS16) {
			build_wdtr(acb, dcb, srb);
			goto no_cmd;
		}
		srb->msg_count = 0;
	}
	
	DC395x_write8(acb, TRM_S1040_SCSI_FIFO, identify_message);

	scsicommand = SCMD_SEL_ATN;
	srb->state = SRB_START_;
#ifndef DC395x_NO_TAGQ
	if ((dcb->sync_mode & EN_TAG_QUEUEING)
	    && (identify_message & 0xC0)) {
		
		u32 tag_mask = 1;
		u8 tag_number = 0;
		while (tag_mask & dcb->tag_mask
		       && tag_number < dcb->max_command) {
			tag_mask = tag_mask << 1;
			tag_number++;
		}
		if (tag_number >= dcb->max_command) {
			dprintkl(KERN_WARNING, "start_scsi: (0x%p) "
				"Out of tags target=<%02i-%i>)\n",
				srb->cmd, srb->cmd->device->id,
				srb->cmd->device->lun);
			srb->state = SRB_READY;
			DC395x_write16(acb, TRM_S1040_SCSI_CONTROL,
				       DO_HWRESELECT);
			return 1;
		}
		
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, MSG_SIMPLE_QTAG);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, tag_number);
		dcb->tag_mask |= tag_mask;
		srb->tag_number = tag_number;
		scsicommand = SCMD_SEL_ATN3;
		srb->state = SRB_START_;
	}
#endif
	
	dprintkdbg(DBG_KG, "start_scsi: (0x%p) <%02i-%i> cmnd=0x%02x tag=%i\n",
		srb->cmd, srb->cmd->device->id, srb->cmd->device->lun,
		srb->cmd->cmnd[0], srb->tag_number);
	if (srb->flag & AUTO_REQSENSE) {
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, REQUEST_SENSE);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, (dcb->target_lun << 5));
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, SCSI_SENSE_BUFFERSIZE);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
	} else {
		ptr = (u8 *)srb->cmd->cmnd;
		for (i = 0; i < srb->cmd->cmd_len; i++)
			DC395x_write8(acb, TRM_S1040_SCSI_FIFO, *ptr++);
	}
      no_cmd:
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL,
		       DO_HWRESELECT | DO_DATALATCH);
	if (DC395x_read16(acb, TRM_S1040_SCSI_STATUS) & SCSIINTERRUPT) {
		dprintkdbg(DBG_0, "start_scsi: (0x%p) <%02i-%i> Failed - busy\n",
			srb->cmd, dcb->target_id, dcb->target_lun);
		srb->state = SRB_READY;
		free_tag(dcb, srb);
		srb->msg_count = 0;
		return_code = 1;
		
	} else {
		srb->scsi_phase = PH_BUS_FREE;	
		dcb->active_srb = srb;
		acb->active_dcb = dcb;
		return_code = 0;
		
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL,
			       DO_DATALATCH | DO_HWRESELECT);
		
		DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, scsicommand);
	}
	return return_code;
}


#define DC395x_ENABLE_MSGOUT \
 DC395x_write16 (acb, TRM_S1040_SCSI_CONTROL, DO_SETATN); \
 srb->state |= SRB_MSGOUT


static inline void enable_msgout_abort(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb)
{
	srb->msgout_buf[0] = ABORT;
	srb->msg_count = 1;
	DC395x_ENABLE_MSGOUT;
	srb->state &= ~SRB_MSGIN;
	srb->state |= SRB_MSGOUT;
}


static void dc395x_handle_interrupt(struct AdapterCtlBlk *acb,
		u16 scsi_status)
{
	struct DeviceCtlBlk *dcb;
	struct ScsiReqBlk *srb;
	u16 phase;
	u8 scsi_intstatus;
	unsigned long flags;
	void (*dc395x_statev)(struct AdapterCtlBlk *, struct ScsiReqBlk *, 
			      u16 *);

	DC395x_LOCK_IO(acb->scsi_host, flags);

	
	scsi_intstatus = DC395x_read8(acb, TRM_S1040_SCSI_INTSTATUS);
	if ((scsi_status & 0x2007) == 0x2002)
		dprintkl(KERN_DEBUG,
			"COP after COP completed? %04x\n", scsi_status);
	if (debug_enabled(DBG_KG)) {
		if (scsi_intstatus & INT_SELTIMEOUT)
			dprintkdbg(DBG_KG, "handle_interrupt: Selection timeout\n");
	}
	

	if (timer_pending(&acb->selto_timer))
		del_timer(&acb->selto_timer);

	if (scsi_intstatus & (INT_SELTIMEOUT | INT_DISCONNECT)) {
		disconnect(acb);	
		goto out_unlock;
	}
	if (scsi_intstatus & INT_RESELECTED) {
		reselect(acb);
		goto out_unlock;
	}
	if (scsi_intstatus & INT_SELECT) {
		dprintkl(KERN_INFO, "Host does not support target mode!\n");
		goto out_unlock;
	}
	if (scsi_intstatus & INT_SCSIRESET) {
		scsi_reset_detect(acb);
		goto out_unlock;
	}
	if (scsi_intstatus & (INT_BUSSERVICE | INT_CMDDONE)) {
		dcb = acb->active_dcb;
		if (!dcb) {
			dprintkl(KERN_DEBUG,
				"Oops: BusService (%04x %02x) w/o ActiveDCB!\n",
				scsi_status, scsi_intstatus);
			goto out_unlock;
		}
		srb = dcb->active_srb;
		if (dcb->flag & ABORT_DEV_) {
			dprintkdbg(DBG_0, "MsgOut Abort Device.....\n");
			enable_msgout_abort(acb, srb);
		}

		
		phase = (u16)srb->scsi_phase;

		
		
		
		
		
		
		
		
		dc395x_statev = dc395x_scsi_phase0[phase];
		dc395x_statev(acb, srb, &scsi_status);

		srb->scsi_phase = scsi_status & PHASEMASK;
		phase = (u16)scsi_status & PHASEMASK;

		
		
		
		
		
		
		
		
		dc395x_statev = dc395x_scsi_phase1[phase];
		dc395x_statev(acb, srb, &scsi_status);
	}
      out_unlock:
	DC395x_UNLOCK_IO(acb->scsi_host, flags);
}


static irqreturn_t dc395x_interrupt(int irq, void *dev_id)
{
	struct AdapterCtlBlk *acb = dev_id;
	u16 scsi_status;
	u8 dma_status;
	irqreturn_t handled = IRQ_NONE;

	scsi_status = DC395x_read16(acb, TRM_S1040_SCSI_STATUS);
	dma_status = DC395x_read8(acb, TRM_S1040_DMA_STATUS);
	if (scsi_status & SCSIINTERRUPT) {
		
		dc395x_handle_interrupt(acb, scsi_status);
		handled = IRQ_HANDLED;
	}
	else if (dma_status & 0x20) {
		
		dprintkl(KERN_INFO, "Interrupt from DMA engine: 0x%02x!\n", dma_status);
#if 0
		dprintkl(KERN_INFO, "This means DMA error! Try to handle ...\n");
		if (acb->active_dcb) {
			acb->active_dcb-> flag |= ABORT_DEV_;
			if (acb->active_dcb->active_srb)
				enable_msgout_abort(acb, acb->active_dcb->active_srb);
		}
		DC395x_write8(acb, TRM_S1040_DMA_CONTROL, ABORTXFER | CLRXFIFO);
#else
		dprintkl(KERN_INFO, "Ignoring DMA error (probably a bad thing) ...\n");
		acb = NULL;
#endif
		handled = IRQ_HANDLED;
	}

	return handled;
}


static void msgout_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "msgout_phase0: (0x%p)\n", srb->cmd);
	if (srb->state & (SRB_UNEXPECT_RESEL + SRB_ABORT_SENT))
		*pscsi_status = PH_BUS_FREE;	

	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	
	srb->state &= ~SRB_MSGOUT;
}


static void msgout_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	u16 i;
	u8 *ptr;
	dprintkdbg(DBG_0, "msgout_phase1: (0x%p)\n", srb->cmd);

	clear_fifo(acb, "msgout_phase1");
	if (!(srb->state & SRB_MSGOUT)) {
		srb->state |= SRB_MSGOUT;
		dprintkl(KERN_DEBUG,
			"msgout_phase1: (0x%p) Phase unexpected\n",
			srb->cmd);	
	}
	if (!srb->msg_count) {
		dprintkdbg(DBG_0, "msgout_phase1: (0x%p) NOP msg\n",
			srb->cmd);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, MSG_NOP);
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	
		DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_FIFO_OUT);
		return;
	}
	ptr = (u8 *)srb->msgout_buf;
	for (i = 0; i < srb->msg_count; i++)
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, *ptr++);
	srb->msg_count = 0;
	if (srb->msgout_buf[0] == MSG_ABORT)
		srb->state = SRB_ABORT_SENT;

	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_FIFO_OUT);
}


static void command_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "command_phase0: (0x%p)\n", srb->cmd);
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);
}


static void command_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	struct DeviceCtlBlk *dcb;
	u8 *ptr;
	u16 i;
	dprintkdbg(DBG_0, "command_phase1: (0x%p)\n", srb->cmd);

	clear_fifo(acb, "command_phase1");
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_CLRATN);
	if (!(srb->flag & AUTO_REQSENSE)) {
		ptr = (u8 *)srb->cmd->cmnd;
		for (i = 0; i < srb->cmd->cmd_len; i++) {
			DC395x_write8(acb, TRM_S1040_SCSI_FIFO, *ptr);
			ptr++;
		}
	} else {
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, REQUEST_SENSE);
		dcb = acb->active_dcb;
		
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, (dcb->target_lun << 5));
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, SCSI_SENSE_BUFFERSIZE);
		DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
	}
	srb->state |= SRB_COMMAND;
	
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);
	
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_FIFO_OUT);
}


static void sg_verify_length(struct ScsiReqBlk *srb)
{
	if (debug_enabled(DBG_SG)) {
		unsigned len = 0;
		unsigned idx = srb->sg_index;
		struct SGentry *psge = srb->segment_x + idx;
		for (; idx < srb->sg_count; psge++, idx++)
			len += psge->length;
		if (len != srb->total_xfer_length)
			dprintkdbg(DBG_SG,
			       "Inconsistent SRB S/G lengths (Tot=%i, Count=%i) !!\n",
			       srb->total_xfer_length, len);
	}			       
}


static void sg_update_list(struct ScsiReqBlk *srb, u32 left)
{
	u8 idx;
	u32 xferred = srb->total_xfer_length - left; 
	struct SGentry *psge = srb->segment_x + srb->sg_index;

	dprintkdbg(DBG_0,
		"sg_update_list: Transferred %i of %i bytes, %i remain\n",
		xferred, srb->total_xfer_length, left);
	if (xferred == 0) {
		
		return;
	}

	sg_verify_length(srb);
	srb->total_xfer_length = left;	
	for (idx = srb->sg_index; idx < srb->sg_count; idx++) {
		if (xferred >= psge->length) {
			
			xferred -= psge->length;
		} else {
			
			psge->length -= xferred;
			psge->address += xferred;
			srb->sg_index = idx;
			pci_dma_sync_single_for_device(srb->dcb->
					    acb->dev,
					    srb->sg_bus_addr,
					    SEGMENTX_LEN,
					    PCI_DMA_TODEVICE);
			break;
		}
		psge++;
	}
	sg_verify_length(srb);
}


static void sg_subtract_one(struct ScsiReqBlk *srb)
{
	sg_update_list(srb, srb->total_xfer_length - 1);
}


static void cleanup_after_transfer(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb)
{
	
	if (DC395x_read16(acb, TRM_S1040_DMA_COMMAND) & 0x0001) {	
		if (!(DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT) & 0x40))
			clear_fifo(acb, "cleanup/in");
		if (!(DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT) & 0x80))
			DC395x_write8(acb, TRM_S1040_DMA_CONTROL, CLRXFIFO);
	} else {		
		if (!(DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT) & 0x80))
			DC395x_write8(acb, TRM_S1040_DMA_CONTROL, CLRXFIFO);
		if (!(DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT) & 0x40))
			clear_fifo(acb, "cleanup/out");
	}
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);
}


#define DC395x_LASTPIO 4


static void data_out_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	u16 scsi_status = *pscsi_status;
	u32 d_left_counter = 0;
	dprintkdbg(DBG_0, "data_out_phase0: (0x%p) <%02i-%i>\n",
		srb->cmd, srb->cmd->device->id, srb->cmd->device->lun);

	dprintkdbg(DBG_PIO, "data_out_phase0: "
		"DMA{fifocnt=0x%02x fifostat=0x%02x} "
		"SCSI{fifocnt=0x%02x cnt=0x%06x status=0x%04x} total=0x%06x\n",
		DC395x_read8(acb, TRM_S1040_DMA_FIFOCNT),
		DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT),
		DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT),
		DC395x_read32(acb, TRM_S1040_SCSI_COUNTER), scsi_status,
		srb->total_xfer_length);
	DC395x_write8(acb, TRM_S1040_DMA_CONTROL, STOPDMAXFER | CLRXFIFO);

	if (!(srb->state & SRB_XFERPAD)) {
		if (scsi_status & PARITYERROR)
			srb->status |= PARITY_ERROR;

		if (!(scsi_status & SCSIXFERDONE)) {
			d_left_counter =
			    (u32)(DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT) &
				  0x1F);
			if (dcb->sync_period & WIDE_SYNC)
				d_left_counter <<= 1;

			dprintkdbg(DBG_KG, "data_out_phase0: FIFO contains %i %s\n"
				"SCSI{fifocnt=0x%02x cnt=0x%08x} "
				"DMA{fifocnt=0x%04x cnt=0x%02x ctr=0x%08x}\n",
				DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT),
				(dcb->sync_period & WIDE_SYNC) ? "words" : "bytes",
				DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT),
				DC395x_read32(acb, TRM_S1040_SCSI_COUNTER),
				DC395x_read8(acb, TRM_S1040_DMA_FIFOCNT),
				DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT),
				DC395x_read32(acb, TRM_S1040_DMA_CXCNT));
		}
		if (srb->total_xfer_length > DC395x_LASTPIO)
			d_left_counter +=
			    DC395x_read32(acb, TRM_S1040_SCSI_COUNTER);

		
		
		
		if (d_left_counter == 1 && dcb->sync_period & WIDE_SYNC
		    && scsi_bufflen(srb->cmd) % 2) {
			d_left_counter = 0;
			dprintkl(KERN_INFO,
				"data_out_phase0: Discard 1 byte (0x%02x)\n",
				scsi_status);
		}
		if (d_left_counter == 0) {
			srb->total_xfer_length = 0;
		} else {
			long oldxferred =
			    srb->total_xfer_length - d_left_counter;
			const int diff =
			    (dcb->sync_period & WIDE_SYNC) ? 2 : 1;
			sg_update_list(srb, d_left_counter);
			
			if ((srb->segment_x[srb->sg_index].length ==
			     diff && scsi_sg_count(srb->cmd))
			    || ((oldxferred & ~PAGE_MASK) ==
				(PAGE_SIZE - diff))
			    ) {
				dprintkl(KERN_INFO, "data_out_phase0: "
					"Work around chip bug (%i)?\n", diff);
				d_left_counter =
				    srb->total_xfer_length - diff;
				sg_update_list(srb, d_left_counter);
				
				
				
				
			}
		}
	}
	if ((*pscsi_status & PHASEMASK) != PH_DATA_OUT) {
		cleanup_after_transfer(acb, srb);
	}
}


static void data_out_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "data_out_phase1: (0x%p) <%02i-%i>\n",
		srb->cmd, srb->cmd->device->id, srb->cmd->device->lun);
	clear_fifo(acb, "data_out_phase1");
	
	data_io_transfer(acb, srb, XFERDATAOUT);
}

static void data_in_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	u16 scsi_status = *pscsi_status;

	dprintkdbg(DBG_0, "data_in_phase0: (0x%p) <%02i-%i>\n",
		srb->cmd, srb->cmd->device->id, srb->cmd->device->lun);

	if (!(srb->state & SRB_XFERPAD)) {
		u32 d_left_counter;
		unsigned int sc, fc;

		if (scsi_status & PARITYERROR) {
			dprintkl(KERN_INFO, "data_in_phase0: (0x%p) "
				"Parity Error\n", srb->cmd);
			srb->status |= PARITY_ERROR;
		}
		if (!(DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT) & 0x80)) {
#if 0
			int ctr = 6000000;
			dprintkl(KERN_DEBUG,
				"DIP0: Wait for DMA FIFO to flush ...\n");
			
			
			
			while (!
			       (DC395x_read16(acb, TRM_S1040_DMA_FIFOSTAT) &
				0x80) && --ctr);
			if (ctr < 6000000 - 1)
				dprintkl(KERN_DEBUG
				       "DIP0: Had to wait for DMA ...\n");
			if (!ctr)
				dprintkl(KERN_ERR,
				       "Deadlock in DIP0 waiting for DMA FIFO empty!!\n");
			
#endif
			dprintkdbg(DBG_KG, "data_in_phase0: "
				"DMA{fifocnt=0x%02x fifostat=0x%02x}\n",
				DC395x_read8(acb, TRM_S1040_DMA_FIFOCNT),
				DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT));
		}
		
		sc = DC395x_read32(acb, TRM_S1040_SCSI_COUNTER);
		fc = DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT);
		d_left_counter = sc + ((fc & 0x1f)
		       << ((srb->dcb->sync_period & WIDE_SYNC) ? 1 :
			   0));
		dprintkdbg(DBG_KG, "data_in_phase0: "
			"SCSI{fifocnt=0x%02x%s ctr=0x%08x} "
			"DMA{fifocnt=0x%02x fifostat=0x%02x ctr=0x%08x} "
			"Remain{totxfer=%i scsi_fifo+ctr=%i}\n",
			fc,
			(srb->dcb->sync_period & WIDE_SYNC) ? "words" : "bytes",
			sc,
			fc,
			DC395x_read8(acb, TRM_S1040_DMA_FIFOSTAT),
			DC395x_read32(acb, TRM_S1040_DMA_CXCNT),
			srb->total_xfer_length, d_left_counter);
#if DC395x_LASTPIO
		
		if (d_left_counter
		    && srb->total_xfer_length <= DC395x_LASTPIO) {
			size_t left_io = srb->total_xfer_length;

			
			
			dprintkdbg(DBG_PIO, "data_in_phase0: PIO (%i %s) "
				   "for remaining %i bytes:",
				fc & 0x1f,
				(srb->dcb->sync_period & WIDE_SYNC) ?
				    "words" : "bytes",
				srb->total_xfer_length);
			if (srb->dcb->sync_period & WIDE_SYNC)
				DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2,
					      CFG2_WIDEFIFO);
			while (left_io) {
				unsigned char *virt, *base = NULL;
				unsigned long flags = 0;
				size_t len = left_io;
				size_t offset = srb->request_length - left_io;

				local_irq_save(flags);
				base = scsi_kmap_atomic_sg(scsi_sglist(srb->cmd),
							   srb->sg_count, &offset, &len);
				virt = base + offset;

				left_io -= len;

				while (len) {
					u8 byte;
					byte = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
					*virt++ = byte;

					if (debug_enabled(DBG_PIO))
						printk(" %02x", byte);

					d_left_counter--;
					sg_subtract_one(srb);

					len--;

					fc = DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT);

					if (fc == 0x40) {
						left_io = 0;
						break;
					}
				}

				WARN_ON((fc != 0x40) == !d_left_counter);

				if (fc == 0x40 && (srb->dcb->sync_period & WIDE_SYNC)) {
					
					if (srb->total_xfer_length > 0) {
						u8 byte = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);

						*virt++ = byte;
						srb->total_xfer_length--;
						if (debug_enabled(DBG_PIO))
							printk(" %02x", byte);
					}

					DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2, 0);
				}

				scsi_kunmap_atomic_sg(base);
				local_irq_restore(flags);
			}
			
			
			if (debug_enabled(DBG_PIO))
				printk("\n");
		}
#endif				

#if 0
		if (!(scsi_status & SCSIXFERDONE)) {
			d_left_counter =
			    (u32)(DC395x_read8(acb, TRM_S1040_SCSI_FIFOCNT) &
				  0x1F);
			if (srb->dcb->sync_period & WIDE_SYNC)
				d_left_counter <<= 1;
		}
#endif
		
		if (d_left_counter == 0
		    || (scsi_status & SCSIXFERCNT_2_ZERO)) {
#if 0
			int ctr = 6000000;
			u8 TempDMAstatus;
			do {
				TempDMAstatus =
				    DC395x_read8(acb, TRM_S1040_DMA_STATUS);
			} while (!(TempDMAstatus & DMAXFERCOMP) && --ctr);
			if (!ctr)
				dprintkl(KERN_ERR,
				       "Deadlock in DataInPhase0 waiting for DMA!!\n");
			srb->total_xfer_length = 0;
#endif
			srb->total_xfer_length = d_left_counter;
		} else {	
			sg_update_list(srb, d_left_counter);
		}
	}
	
	if ((*pscsi_status & PHASEMASK) != PH_DATA_IN) {
		cleanup_after_transfer(acb, srb);
	}
}


static void data_in_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "data_in_phase1: (0x%p) <%02i-%i>\n",
		srb->cmd, srb->cmd->device->id, srb->cmd->device->lun);
	data_io_transfer(acb, srb, XFERDATAIN);
}


static void data_io_transfer(struct AdapterCtlBlk *acb, 
		struct ScsiReqBlk *srb, u16 io_dir)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	u8 bval;
	dprintkdbg(DBG_0,
		"data_io_transfer: (0x%p) <%02i-%i> %c len=%i, sg=(%i/%i)\n",
		srb->cmd, srb->cmd->device->id, srb->cmd->device->lun,
		((io_dir & DMACMD_DIR) ? 'r' : 'w'),
		srb->total_xfer_length, srb->sg_index, srb->sg_count);
	if (srb == acb->tmp_srb)
		dprintkl(KERN_ERR, "data_io_transfer: Using tmp_srb!\n");
	if (srb->sg_index >= srb->sg_count) {
		
		return;
	}

	if (srb->total_xfer_length > DC395x_LASTPIO) {
		u8 dma_status = DC395x_read8(acb, TRM_S1040_DMA_STATUS);
		if (dma_status & XFERPENDING) {
			dprintkl(KERN_DEBUG, "data_io_transfer: Xfer pending! "
				"Expect trouble!\n");
			dump_register_info(acb, dcb, srb);
			DC395x_write8(acb, TRM_S1040_DMA_CONTROL, CLRXFIFO);
		}
		
		srb->state |= SRB_DATA_XFER;
		DC395x_write32(acb, TRM_S1040_DMA_XHIGHADDR, 0);
		if (scsi_sg_count(srb->cmd)) {	
			io_dir |= DMACMD_SG;
			DC395x_write32(acb, TRM_S1040_DMA_XLOWADDR,
				       srb->sg_bus_addr +
				       sizeof(struct SGentry) *
				       srb->sg_index);
			
			DC395x_write32(acb, TRM_S1040_DMA_XCNT,
				       ((u32)(srb->sg_count -
					      srb->sg_index) << 3));
		} else {	
			io_dir &= ~DMACMD_SG;
			DC395x_write32(acb, TRM_S1040_DMA_XLOWADDR,
				       srb->segment_x[0].address);
			DC395x_write32(acb, TRM_S1040_DMA_XCNT,
				       srb->segment_x[0].length);
		}
		
		DC395x_write32(acb, TRM_S1040_SCSI_COUNTER,
			       srb->total_xfer_length);
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	
		if (io_dir & DMACMD_DIR) {	
			DC395x_write8(acb, TRM_S1040_SCSI_COMMAND,
				      SCMD_DMA_IN);
			DC395x_write16(acb, TRM_S1040_DMA_COMMAND, io_dir);
		} else {
			DC395x_write16(acb, TRM_S1040_DMA_COMMAND, io_dir);
			DC395x_write8(acb, TRM_S1040_SCSI_COMMAND,
				      SCMD_DMA_OUT);
		}

	}
#if DC395x_LASTPIO
	else if (srb->total_xfer_length > 0) {	
		srb->state |= SRB_DATA_XFER;
		
		DC395x_write32(acb, TRM_S1040_SCSI_COUNTER,
			       srb->total_xfer_length);
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	
		if (io_dir & DMACMD_DIR) {	
			DC395x_write8(acb, TRM_S1040_SCSI_COMMAND,
				      SCMD_FIFO_IN);
		} else {	
			int ln = srb->total_xfer_length;
			size_t left_io = srb->total_xfer_length;

			if (srb->dcb->sync_period & WIDE_SYNC)
				DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2,
				     CFG2_WIDEFIFO);

			while (left_io) {
				unsigned char *virt, *base = NULL;
				unsigned long flags = 0;
				size_t len = left_io;
				size_t offset = srb->request_length - left_io;

				local_irq_save(flags);
				
				base = scsi_kmap_atomic_sg(scsi_sglist(srb->cmd),
							   srb->sg_count, &offset, &len);
				virt = base + offset;

				left_io -= len;

				while (len--) {
					if (debug_enabled(DBG_PIO))
						printk(" %02x", *virt);

					DC395x_write8(acb, TRM_S1040_SCSI_FIFO, *virt++);

					sg_subtract_one(srb);
				}

				scsi_kunmap_atomic_sg(base);
				local_irq_restore(flags);
			}
			if (srb->dcb->sync_period & WIDE_SYNC) {
				if (ln % 2) {
					DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 0);
					if (debug_enabled(DBG_PIO))
						printk(" |00");
				}
				DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2, 0);
			}
			
			if (debug_enabled(DBG_PIO))
				printk("\n");
			DC395x_write8(acb, TRM_S1040_SCSI_COMMAND,
					  SCMD_FIFO_OUT);
		}
	}
#endif				
	else {		
		u8 data = 0, data2 = 0;
		if (srb->sg_count) {
			srb->adapter_status = H_OVER_UNDER_RUN;
			srb->status |= OVER_RUN;
		}
		if (dcb->sync_period & WIDE_SYNC) {
			DC395x_write32(acb, TRM_S1040_SCSI_COUNTER, 2);
			DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2,
				      CFG2_WIDEFIFO);
			if (io_dir & DMACMD_DIR) {
				data = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
				data2 = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
			} else {
				DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 'K');
				DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 'G');
			}
			DC395x_write8(acb, TRM_S1040_SCSI_CONFIG2, 0);
		} else {
			DC395x_write32(acb, TRM_S1040_SCSI_COUNTER, 1);
			if (io_dir & DMACMD_DIR)
				data = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
			else
				DC395x_write8(acb, TRM_S1040_SCSI_FIFO, 'K');
		}
		srb->state |= SRB_XFERPAD;
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	
		
		bval = (io_dir & DMACMD_DIR) ? SCMD_FIFO_IN : SCMD_FIFO_OUT;
		DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, bval);
	}
}


static void status_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "status_phase0: (0x%p) <%02i-%i>\n",
		srb->cmd, srb->cmd->device->id, srb->cmd->device->lun);
	srb->target_status = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
	srb->end_message = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);	
	srb->state = SRB_COMPLETED;
	*pscsi_status = PH_BUS_FREE;	
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_MSGACCEPT);
}


static void status_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "status_phase1: (0x%p) <%02i-%i>\n",
		srb->cmd, srb->cmd->device->id, srb->cmd->device->lun);
	srb->state = SRB_STATUS;
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_COMP);
}


static inline u8 msgin_completed(u8 * msgbuf, u32 len)
{
	if (*msgbuf == EXTENDED_MESSAGE) {
		if (len < 2)
			return 0;
		if (len < msgbuf[1] + 2)
			return 0;
	} else if (*msgbuf >= 0x20 && *msgbuf <= 0x2f)	
		if (len < 2)
			return 0;
	return 1;
}

static inline void msgin_reject(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb)
{
	srb->msgout_buf[0] = MESSAGE_REJECT;
	srb->msg_count = 1;
	DC395x_ENABLE_MSGOUT;
	srb->state &= ~SRB_MSGIN;
	srb->state |= SRB_MSGOUT;
	dprintkl(KERN_INFO, "msgin_reject: 0x%02x <%02i-%i>\n",
		srb->msgin_buf[0],
		srb->dcb->target_id, srb->dcb->target_lun);
}


static struct ScsiReqBlk *msgin_qtag(struct AdapterCtlBlk *acb,
		struct DeviceCtlBlk *dcb, u8 tag)
{
	struct ScsiReqBlk *srb = NULL;
	struct ScsiReqBlk *i;
	dprintkdbg(DBG_0, "msgin_qtag: (0x%p) tag=%i srb=%p\n",
		   srb->cmd, tag, srb);

	if (!(dcb->tag_mask & (1 << tag)))
		dprintkl(KERN_DEBUG,
			"msgin_qtag: tag_mask=0x%08x does not reserve tag %i!\n",
			dcb->tag_mask, tag);

	if (list_empty(&dcb->srb_going_list))
		goto mingx0;
	list_for_each_entry(i, &dcb->srb_going_list, list) {
		if (i->tag_number == tag) {
			srb = i;
			break;
		}
	}
	if (!srb)
		goto mingx0;

	dprintkdbg(DBG_0, "msgin_qtag: (0x%p) <%02i-%i>\n",
		srb->cmd, srb->dcb->target_id, srb->dcb->target_lun);
	if (dcb->flag & ABORT_DEV_) {
		
		enable_msgout_abort(acb, srb);
	}

	if (!(srb->state & SRB_DISCONNECT))
		goto mingx0;

	memcpy(srb->msgin_buf, dcb->active_srb->msgin_buf, acb->msg_len);
	srb->state |= dcb->active_srb->state;
	srb->state |= SRB_DATA_XFER;
	dcb->active_srb = srb;
	
	return srb;

      mingx0:
	srb = acb->tmp_srb;
	srb->state = SRB_UNEXPECT_RESEL;
	dcb->active_srb = srb;
	srb->msgout_buf[0] = MSG_ABORT_TAG;
	srb->msg_count = 1;
	DC395x_ENABLE_MSGOUT;
	dprintkl(KERN_DEBUG, "msgin_qtag: Unknown tag %i - abort\n", tag);
	return srb;
}


static inline void reprogram_regs(struct AdapterCtlBlk *acb,
		struct DeviceCtlBlk *dcb)
{
	DC395x_write8(acb, TRM_S1040_SCSI_TARGETID, dcb->target_id);
	DC395x_write8(acb, TRM_S1040_SCSI_SYNC, dcb->sync_period);
	DC395x_write8(acb, TRM_S1040_SCSI_OFFSET, dcb->sync_offset);
	set_xfer_rate(acb, dcb);
}


static void msgin_set_async(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	dprintkl(KERN_DEBUG, "msgin_set_async: No sync transfers <%02i-%i>\n",
		dcb->target_id, dcb->target_lun);

	dcb->sync_mode &= ~(SYNC_NEGO_ENABLE);
	dcb->sync_mode |= SYNC_NEGO_DONE;
	
	dcb->sync_offset = 0;
	dcb->min_nego_period = 200 >> 2;	
	srb->state &= ~SRB_DO_SYNC_NEGO;
	reprogram_regs(acb, dcb);
	if ((dcb->sync_mode & WIDE_NEGO_ENABLE)
	    && !(dcb->sync_mode & WIDE_NEGO_DONE)) {
		build_wdtr(acb, dcb, srb);
		DC395x_ENABLE_MSGOUT;
		dprintkdbg(DBG_0, "msgin_set_async(rej): Try WDTR anyway\n");
	}
}


static void msgin_set_sync(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	u8 bval;
	int fact;
	dprintkdbg(DBG_1, "msgin_set_sync: <%02i> Sync: %ins "
		"(%02i.%01i MHz) Offset %i\n",
		dcb->target_id, srb->msgin_buf[3] << 2,
		(250 / srb->msgin_buf[3]),
		((250 % srb->msgin_buf[3]) * 10) / srb->msgin_buf[3],
		srb->msgin_buf[4]);

	if (srb->msgin_buf[4] > 15)
		srb->msgin_buf[4] = 15;
	if (!(dcb->dev_mode & NTC_DO_SYNC_NEGO))
		dcb->sync_offset = 0;
	else if (dcb->sync_offset == 0)
		dcb->sync_offset = srb->msgin_buf[4];
	if (srb->msgin_buf[4] > dcb->sync_offset)
		srb->msgin_buf[4] = dcb->sync_offset;
	else
		dcb->sync_offset = srb->msgin_buf[4];
	bval = 0;
	while (bval < 7 && (srb->msgin_buf[3] > clock_period[bval]
			    || dcb->min_nego_period >
			    clock_period[bval]))
		bval++;
	if (srb->msgin_buf[3] < clock_period[bval])
		dprintkl(KERN_INFO,
			"msgin_set_sync: Increase sync nego period to %ins\n",
			clock_period[bval] << 2);
	srb->msgin_buf[3] = clock_period[bval];
	dcb->sync_period &= 0xf0;
	dcb->sync_period |= ALT_SYNC | bval;
	dcb->min_nego_period = srb->msgin_buf[3];

	if (dcb->sync_period & WIDE_SYNC)
		fact = 500;
	else
		fact = 250;

	dprintkl(KERN_INFO,
		"Target %02i: %s Sync: %ins Offset %i (%02i.%01i MB/s)\n",
		dcb->target_id, (fact == 500) ? "Wide16" : "",
		dcb->min_nego_period << 2, dcb->sync_offset,
		(fact / dcb->min_nego_period),
		((fact % dcb->min_nego_period) * 10 +
		dcb->min_nego_period / 2) / dcb->min_nego_period);

	if (!(srb->state & SRB_DO_SYNC_NEGO)) {
		
		dprintkl(KERN_DEBUG, "msgin_set_sync: answer w/%ins %i\n",
			srb->msgin_buf[3] << 2, srb->msgin_buf[4]);

		memcpy(srb->msgout_buf, srb->msgin_buf, 5);
		srb->msg_count = 5;
		DC395x_ENABLE_MSGOUT;
		dcb->sync_mode |= SYNC_NEGO_DONE;
	} else {
		if ((dcb->sync_mode & WIDE_NEGO_ENABLE)
		    && !(dcb->sync_mode & WIDE_NEGO_DONE)) {
			build_wdtr(acb, dcb, srb);
			DC395x_ENABLE_MSGOUT;
			dprintkdbg(DBG_0, "msgin_set_sync: Also try WDTR\n");
		}
	}
	srb->state &= ~SRB_DO_SYNC_NEGO;
	dcb->sync_mode |= SYNC_NEGO_DONE | SYNC_NEGO_ENABLE;

	reprogram_regs(acb, dcb);
}


static inline void msgin_set_nowide(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	dprintkdbg(DBG_1, "msgin_set_nowide: <%02i>\n", dcb->target_id);

	dcb->sync_period &= ~WIDE_SYNC;
	dcb->sync_mode &= ~(WIDE_NEGO_ENABLE);
	dcb->sync_mode |= WIDE_NEGO_DONE;
	srb->state &= ~SRB_DO_WIDE_NEGO;
	reprogram_regs(acb, dcb);
	if ((dcb->sync_mode & SYNC_NEGO_ENABLE)
	    && !(dcb->sync_mode & SYNC_NEGO_DONE)) {
		build_sdtr(acb, dcb, srb);
		DC395x_ENABLE_MSGOUT;
		dprintkdbg(DBG_0, "msgin_set_nowide: Rejected. Try SDTR anyway\n");
	}
}

static void msgin_set_wide(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	struct DeviceCtlBlk *dcb = srb->dcb;
	u8 wide = (dcb->dev_mode & NTC_DO_WIDE_NEGO
		   && acb->config & HCC_WIDE_CARD) ? 1 : 0;
	dprintkdbg(DBG_1, "msgin_set_wide: <%02i>\n", dcb->target_id);

	if (srb->msgin_buf[3] > wide)
		srb->msgin_buf[3] = wide;
	
	if (!(srb->state & SRB_DO_WIDE_NEGO)) {
		dprintkl(KERN_DEBUG,
			"msgin_set_wide: Wide nego initiated <%02i>\n",
			dcb->target_id);
		memcpy(srb->msgout_buf, srb->msgin_buf, 4);
		srb->msg_count = 4;
		srb->state |= SRB_DO_WIDE_NEGO;
		DC395x_ENABLE_MSGOUT;
	}

	dcb->sync_mode |= (WIDE_NEGO_ENABLE | WIDE_NEGO_DONE);
	if (srb->msgin_buf[3] > 0)
		dcb->sync_period |= WIDE_SYNC;
	else
		dcb->sync_period &= ~WIDE_SYNC;
	srb->state &= ~SRB_DO_WIDE_NEGO;
	
	dprintkdbg(DBG_1,
		"msgin_set_wide: Wide (%i bit) negotiated <%02i>\n",
		(8 << srb->msgin_buf[3]), dcb->target_id);
	reprogram_regs(acb, dcb);
	if ((dcb->sync_mode & SYNC_NEGO_ENABLE)
	    && !(dcb->sync_mode & SYNC_NEGO_DONE)) {
		build_sdtr(acb, dcb, srb);
		DC395x_ENABLE_MSGOUT;
		dprintkdbg(DBG_0, "msgin_set_wide: Also try SDTR.\n");
	}
}


static void msgin_phase0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	struct DeviceCtlBlk *dcb = acb->active_dcb;
	dprintkdbg(DBG_0, "msgin_phase0: (0x%p)\n", srb->cmd);

	srb->msgin_buf[acb->msg_len++] = DC395x_read8(acb, TRM_S1040_SCSI_FIFO);
	if (msgin_completed(srb->msgin_buf, acb->msg_len)) {
		
		switch (srb->msgin_buf[0]) {
		case DISCONNECT:
			srb->state = SRB_DISCONNECT;
			break;

		case SIMPLE_QUEUE_TAG:
		case HEAD_OF_QUEUE_TAG:
		case ORDERED_QUEUE_TAG:
			srb =
			    msgin_qtag(acb, dcb,
					      srb->msgin_buf[1]);
			break;

		case MESSAGE_REJECT:
			DC395x_write16(acb, TRM_S1040_SCSI_CONTROL,
				       DO_CLRATN | DO_DATALATCH);
			
			if (srb->state & SRB_DO_SYNC_NEGO) {
				msgin_set_async(acb, srb);
				break;
			}
			
			if (srb->state & SRB_DO_WIDE_NEGO) {
				msgin_set_nowide(acb, srb);
				break;
			}
			enable_msgout_abort(acb, srb);
			
			break;

		case EXTENDED_MESSAGE:
			
			if (srb->msgin_buf[1] == 3
			    && srb->msgin_buf[2] == EXTENDED_SDTR) {
				msgin_set_sync(acb, srb);
				break;
			}
			
			if (srb->msgin_buf[1] == 2
			    && srb->msgin_buf[2] == EXTENDED_WDTR
			    && srb->msgin_buf[3] <= 2) { 
				msgin_set_wide(acb, srb);
				break;
			}
			msgin_reject(acb, srb);
			break;

		case MSG_IGNOREWIDE:
			
			dprintkdbg(DBG_0, "msgin_phase0: Ignore Wide Residual!\n");
			break;

		case COMMAND_COMPLETE:
			
			break;

		case SAVE_POINTERS:
			dprintkdbg(DBG_0, "msgin_phase0: (0x%p) "
				"SAVE POINTER rem=%i Ignore\n",
				srb->cmd, srb->total_xfer_length);
			break;

		case RESTORE_POINTERS:
			dprintkdbg(DBG_0, "msgin_phase0: RESTORE POINTER. Ignore\n");
			break;

		case ABORT:
			dprintkdbg(DBG_0, "msgin_phase0: (0x%p) "
				"<%02i-%i> ABORT msg\n",
				srb->cmd, dcb->target_id,
				dcb->target_lun);
			dcb->flag |= ABORT_DEV_;
			enable_msgout_abort(acb, srb);
			break;

		default:
			
			if (srb->msgin_buf[0] & IDENTIFY_BASE) {
				dprintkdbg(DBG_0, "msgin_phase0: Identify msg\n");
				srb->msg_count = 1;
				srb->msgout_buf[0] = dcb->identify_msg;
				DC395x_ENABLE_MSGOUT;
				srb->state |= SRB_MSGOUT;
				
			}
			msgin_reject(acb, srb);
		}

		
		srb->state &= ~SRB_MSGIN;
		acb->msg_len = 0;
	}
	*pscsi_status = PH_BUS_FREE;
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_MSGACCEPT);
}


static void msgin_phase1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
	dprintkdbg(DBG_0, "msgin_phase1: (0x%p)\n", srb->cmd);
	clear_fifo(acb, "msgin_phase1");
	DC395x_write32(acb, TRM_S1040_SCSI_COUNTER, 1);
	if (!(srb->state & SRB_MSGIN)) {
		srb->state &= ~SRB_DISCONNECT;
		srb->state |= SRB_MSGIN;
	}
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	
	
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_FIFO_IN);
}


static void nop0(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
}


static void nop1(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb,
		u16 *pscsi_status)
{
}


static void set_xfer_rate(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb)
{
	struct DeviceCtlBlk *i;

	
	if (dcb->identify_msg & 0x07)
		return;

	if (acb->scan_devices) {
		current_sync_offset = dcb->sync_offset;
		return;
	}

	list_for_each_entry(i, &acb->dcb_list, list)
		if (i->target_id == dcb->target_id) {
			i->sync_period = dcb->sync_period;
			i->sync_offset = dcb->sync_offset;
			i->sync_mode = dcb->sync_mode;
			i->min_nego_period = dcb->min_nego_period;
		}
}


static void disconnect(struct AdapterCtlBlk *acb)
{
	struct DeviceCtlBlk *dcb = acb->active_dcb;
	struct ScsiReqBlk *srb;

	if (!dcb) {
		dprintkl(KERN_ERR, "disconnect: No such device\n");
		udelay(500);
		
		acb->scsi_host->last_reset =
		    jiffies + HZ / 2 +
		    HZ * acb->eeprom.delay_time;
		clear_fifo(acb, "disconnectEx");
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_HWRESELECT);
		return;
	}
	srb = dcb->active_srb;
	acb->active_dcb = NULL;
	dprintkdbg(DBG_0, "disconnect: (0x%p)\n", srb->cmd);

	srb->scsi_phase = PH_BUS_FREE;	
	clear_fifo(acb, "disconnect");
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_HWRESELECT);
	if (srb->state & SRB_UNEXPECT_RESEL) {
		dprintkl(KERN_ERR,
			"disconnect: Unexpected reselection <%02i-%i>\n",
			dcb->target_id, dcb->target_lun);
		srb->state = 0;
		waiting_process_next(acb);
	} else if (srb->state & SRB_ABORT_SENT) {
		dcb->flag &= ~ABORT_DEV_;
		acb->scsi_host->last_reset = jiffies + HZ / 2 + 1;
		dprintkl(KERN_ERR, "disconnect: SRB_ABORT_SENT\n");
		doing_srb_done(acb, DID_ABORT, srb->cmd, 1);
		waiting_process_next(acb);
	} else {
		if ((srb->state & (SRB_START_ + SRB_MSGOUT))
		    || !(srb->
			 state & (SRB_DISCONNECT + SRB_COMPLETED))) {
			
			if (srb->state != SRB_START_
			    && srb->state != SRB_MSGOUT) {
				srb->state = SRB_READY;
				dprintkl(KERN_DEBUG,
					"disconnect: (0x%p) Unexpected\n",
					srb->cmd);
				srb->target_status = SCSI_STAT_SEL_TIMEOUT;
				goto disc1;
			} else {
				
				dprintkdbg(DBG_KG, "disconnect: (0x%p) "
					"<%02i-%i> SelTO\n", srb->cmd,
					dcb->target_id, dcb->target_lun);
				if (srb->retry_count++ > DC395x_MAX_RETRIES
				    || acb->scan_devices) {
					srb->target_status =
					    SCSI_STAT_SEL_TIMEOUT;
					goto disc1;
				}
				free_tag(dcb, srb);
				srb_going_to_waiting_move(dcb, srb);
				dprintkdbg(DBG_KG,
					"disconnect: (0x%p) Retry\n",
					srb->cmd);
				waiting_set_timer(acb, HZ / 20);
			}
		} else if (srb->state & SRB_DISCONNECT) {
			u8 bval = DC395x_read8(acb, TRM_S1040_SCSI_SIGNAL);
			if (bval & 0x40) {
				dprintkdbg(DBG_0, "disconnect: SCSI bus stat "
					" 0x%02x: ACK set! Other controllers?\n",
					bval);
				
			} else
				waiting_process_next(acb);
		} else if (srb->state & SRB_COMPLETED) {
		      disc1:
			free_tag(dcb, srb);
			dcb->active_srb = NULL;
			srb->state = SRB_FREE;
			srb_done(acb, dcb, srb);
		}
	}
}


static void reselect(struct AdapterCtlBlk *acb)
{
	struct DeviceCtlBlk *dcb = acb->active_dcb;
	struct ScsiReqBlk *srb = NULL;
	u16 rsel_tar_lun_id;
	u8 id, lun;
	u8 arblostflag = 0;
	dprintkdbg(DBG_0, "reselect: acb=%p\n", acb);

	clear_fifo(acb, "reselect");
	
	
	rsel_tar_lun_id = DC395x_read16(acb, TRM_S1040_SCSI_TARGETID);
	if (dcb) {		
		srb = dcb->active_srb;
		if (!srb) {
			dprintkl(KERN_DEBUG, "reselect: Arb lost Resel won, "
				"but active_srb == NULL\n");
			DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	
			return;
		}
		
		if (!acb->scan_devices) {
			dprintkdbg(DBG_KG, "reselect: (0x%p) <%02i-%i> "
				"Arb lost but Resel win rsel=%i stat=0x%04x\n",
				srb->cmd, dcb->target_id,
				dcb->target_lun, rsel_tar_lun_id,
				DC395x_read16(acb, TRM_S1040_SCSI_STATUS));
			arblostflag = 1;
			

			srb->state = SRB_READY;
			free_tag(dcb, srb);
			srb_going_to_waiting_move(dcb, srb);
			waiting_set_timer(acb, HZ / 20);

			
		}
	}
	
	if (!(rsel_tar_lun_id & (IDENTIFY_BASE << 8)))
		dprintkl(KERN_DEBUG, "reselect: Expects identify msg. "
			"Got %i!\n", rsel_tar_lun_id);
	id = rsel_tar_lun_id & 0xff;
	lun = (rsel_tar_lun_id >> 8) & 7;
	dcb = find_dcb(acb, id, lun);
	if (!dcb) {
		dprintkl(KERN_ERR, "reselect: From non existent device "
			"<%02i-%i>\n", id, lun);
		DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);	
		return;
	}
	acb->active_dcb = dcb;

	if (!(dcb->dev_mode & NTC_DO_DISCONNECT))
		dprintkl(KERN_DEBUG, "reselect: in spite of forbidden "
			"disconnection? <%02i-%i>\n",
			dcb->target_id, dcb->target_lun);

	if (dcb->sync_mode & EN_TAG_QUEUEING ) {
		srb = acb->tmp_srb;
		dcb->active_srb = srb;
	} else {
		
		srb = dcb->active_srb;
		if (!srb || !(srb->state & SRB_DISCONNECT)) {
			dprintkl(KERN_DEBUG,
				"reselect: w/o disconnected cmds <%02i-%i>\n",
				dcb->target_id, dcb->target_lun);
			srb = acb->tmp_srb;
			srb->state = SRB_UNEXPECT_RESEL;
			dcb->active_srb = srb;
			enable_msgout_abort(acb, srb);
		} else {
			if (dcb->flag & ABORT_DEV_) {
				
				enable_msgout_abort(acb, srb);
			} else
				srb->state = SRB_DATA_XFER;

		}
	}
	srb->scsi_phase = PH_BUS_FREE;	

	
	dprintkdbg(DBG_0, "reselect: select <%i>\n", dcb->target_id);
	DC395x_write8(acb, TRM_S1040_SCSI_HOSTID, acb->scsi_host->this_id);	
	DC395x_write8(acb, TRM_S1040_SCSI_TARGETID, dcb->target_id);		
	DC395x_write8(acb, TRM_S1040_SCSI_OFFSET, dcb->sync_offset);		
	DC395x_write8(acb, TRM_S1040_SCSI_SYNC, dcb->sync_period);		
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_DATALATCH);		
	
	DC395x_write8(acb, TRM_S1040_SCSI_COMMAND, SCMD_MSGACCEPT);
}


static inline u8 tagq_blacklist(char *name)
{
#ifndef DC395x_NO_TAGQ
#if 0
	u8 i;
	for (i = 0; i < BADDEVCNT; i++)
		if (memcmp(name, DC395x_baddevname1[i], 28) == 0)
			return 1;
#endif
	return 0;
#else
	return 1;
#endif
}


static void disc_tagq_set(struct DeviceCtlBlk *dcb, struct ScsiInqData *ptr)
{
	
	if ((ptr->Vers & 0x07) >= 2 || (ptr->RDF & 0x0F) == 2) {
		if ((ptr->Flags & SCSI_INQ_CMDQUEUE)
		    && (dcb->dev_mode & NTC_DO_TAG_QUEUEING) &&
		    
		    !tagq_blacklist(((char *)ptr) + 8)) {
			if (dcb->max_command == 1)
				dcb->max_command =
				    dcb->acb->tag_max_num;
			dcb->sync_mode |= EN_TAG_QUEUEING;
			
		} else
			dcb->max_command = 1;
	}
}


static void add_dev(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiInqData *ptr)
{
	u8 bval1 = ptr->DevType & SCSI_DEVTYPE;
	dcb->dev_type = bval1;
	
	disc_tagq_set(dcb, ptr);
}


static void pci_unmap_srb(struct AdapterCtlBlk *acb, struct ScsiReqBlk *srb)
{
	struct scsi_cmnd *cmd = srb->cmd;
	enum dma_data_direction dir = cmd->sc_data_direction;

	if (scsi_sg_count(cmd) && dir != PCI_DMA_NONE) {
		
		dprintkdbg(DBG_SG, "pci_unmap_srb: list=%08x(%05x)\n",
			srb->sg_bus_addr, SEGMENTX_LEN);
		pci_unmap_single(acb->dev, srb->sg_bus_addr,
				 SEGMENTX_LEN,
				 PCI_DMA_TODEVICE);
		dprintkdbg(DBG_SG, "pci_unmap_srb: segs=%i buffer=%p\n",
			   scsi_sg_count(cmd), scsi_bufflen(cmd));
		
		scsi_dma_unmap(cmd);
	}
}


static void pci_unmap_srb_sense(struct AdapterCtlBlk *acb,
		struct ScsiReqBlk *srb)
{
	if (!(srb->flag & AUTO_REQSENSE))
		return;
	
	dprintkdbg(DBG_SG, "pci_unmap_srb_sense: buffer=%08x\n",
	       srb->segment_x[0].address);
	pci_unmap_single(acb->dev, srb->segment_x[0].address,
			 srb->segment_x[0].length, PCI_DMA_FROMDEVICE);
	
	srb->total_xfer_length = srb->xferred;
	srb->segment_x[0].address =
	    srb->segment_x[DC395x_MAX_SG_LISTENTRY - 1].address;
	srb->segment_x[0].length =
	    srb->segment_x[DC395x_MAX_SG_LISTENTRY - 1].length;
}


static void srb_done(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	u8 tempcnt, status;
	struct scsi_cmnd *cmd = srb->cmd;
	enum dma_data_direction dir = cmd->sc_data_direction;
	int ckc_only = 1;

	dprintkdbg(DBG_1, "srb_done: (0x%p) <%02i-%i>\n", srb->cmd,
		srb->cmd->device->id, srb->cmd->device->lun);
	dprintkdbg(DBG_SG, "srb_done: srb=%p sg=%i(%i/%i) buf=%p\n",
		   srb, scsi_sg_count(cmd), srb->sg_index, srb->sg_count,
		   scsi_sgtalbe(cmd));
	status = srb->target_status;
	if (srb->flag & AUTO_REQSENSE) {
		dprintkdbg(DBG_0, "srb_done: AUTO_REQSENSE1\n");
		pci_unmap_srb_sense(acb, srb);
		srb->flag &= ~AUTO_REQSENSE;
		srb->adapter_status = 0;
		srb->target_status = CHECK_CONDITION << 1;
		if (debug_enabled(DBG_1)) {
			switch (cmd->sense_buffer[2] & 0x0f) {
			case NOT_READY:
				dprintkl(KERN_DEBUG,
				     "ReqSense: NOT_READY cmnd=0x%02x <%02i-%i> stat=%i scan=%i ",
				     cmd->cmnd[0], dcb->target_id,
				     dcb->target_lun, status, acb->scan_devices);
				break;
			case UNIT_ATTENTION:
				dprintkl(KERN_DEBUG,
				     "ReqSense: UNIT_ATTENTION cmnd=0x%02x <%02i-%i> stat=%i scan=%i ",
				     cmd->cmnd[0], dcb->target_id,
				     dcb->target_lun, status, acb->scan_devices);
				break;
			case ILLEGAL_REQUEST:
				dprintkl(KERN_DEBUG,
				     "ReqSense: ILLEGAL_REQUEST cmnd=0x%02x <%02i-%i> stat=%i scan=%i ",
				     cmd->cmnd[0], dcb->target_id,
				     dcb->target_lun, status, acb->scan_devices);
				break;
			case MEDIUM_ERROR:
				dprintkl(KERN_DEBUG,
				     "ReqSense: MEDIUM_ERROR cmnd=0x%02x <%02i-%i> stat=%i scan=%i ",
				     cmd->cmnd[0], dcb->target_id,
				     dcb->target_lun, status, acb->scan_devices);
				break;
			case HARDWARE_ERROR:
				dprintkl(KERN_DEBUG,
				     "ReqSense: HARDWARE_ERROR cmnd=0x%02x <%02i-%i> stat=%i scan=%i ",
				     cmd->cmnd[0], dcb->target_id,
				     dcb->target_lun, status, acb->scan_devices);
				break;
			}
			if (cmd->sense_buffer[7] >= 6)
				printk("sense=0x%02x ASC=0x%02x ASCQ=0x%02x "
					"(0x%08x 0x%08x)\n",
					cmd->sense_buffer[2], cmd->sense_buffer[12],
					cmd->sense_buffer[13],
					*((unsigned int *)(cmd->sense_buffer + 3)),
					*((unsigned int *)(cmd->sense_buffer + 8)));
			else
				printk("sense=0x%02x No ASC/ASCQ (0x%08x)\n",
					cmd->sense_buffer[2],
					*((unsigned int *)(cmd->sense_buffer + 3)));
		}

		if (status == (CHECK_CONDITION << 1)) {
			cmd->result = DID_BAD_TARGET << 16;
			goto ckc_e;
		}
		dprintkdbg(DBG_0, "srb_done: AUTO_REQSENSE2\n");

		if (srb->total_xfer_length
		    && srb->total_xfer_length >= cmd->underflow)
			cmd->result =
			    MK_RES_LNX(DRIVER_SENSE, DID_OK,
				       srb->end_message, CHECK_CONDITION);
		
		else
			cmd->result =
			    MK_RES_LNX(DRIVER_SENSE, DID_OK,
				       srb->end_message, CHECK_CONDITION);

		goto ckc_e;
	}

	if (status) {
		if (status_byte(status) == CHECK_CONDITION) {
			request_sense(acb, dcb, srb);
			return;
		} else if (status_byte(status) == QUEUE_FULL) {
			tempcnt = (u8)list_size(&dcb->srb_going_list);
			dprintkl(KERN_INFO, "QUEUE_FULL for dev <%02i-%i> with %i cmnds\n",
			     dcb->target_id, dcb->target_lun, tempcnt);
			if (tempcnt > 1)
				tempcnt--;
			dcb->max_command = tempcnt;
			free_tag(dcb, srb);
			srb_going_to_waiting_move(dcb, srb);
			waiting_set_timer(acb, HZ / 20);
			srb->adapter_status = 0;
			srb->target_status = 0;
			return;
		} else if (status == SCSI_STAT_SEL_TIMEOUT) {
			srb->adapter_status = H_SEL_TIMEOUT;
			srb->target_status = 0;
			cmd->result = DID_NO_CONNECT << 16;
		} else {
			srb->adapter_status = 0;
			SET_RES_DID(cmd->result, DID_ERROR);
			SET_RES_MSG(cmd->result, srb->end_message);
			SET_RES_TARGET(cmd->result, status);

		}
	} else {
		status = srb->adapter_status;
		if (status & H_OVER_UNDER_RUN) {
			srb->target_status = 0;
			SET_RES_DID(cmd->result, DID_OK);
			SET_RES_MSG(cmd->result, srb->end_message);
		} else if (srb->status & PARITY_ERROR) {
			SET_RES_DID(cmd->result, DID_PARITY);
			SET_RES_MSG(cmd->result, srb->end_message);
		} else {	

			srb->adapter_status = 0;
			srb->target_status = 0;
			SET_RES_DID(cmd->result, DID_OK);
		}
	}

	if (dir != PCI_DMA_NONE && scsi_sg_count(cmd))
		pci_dma_sync_sg_for_cpu(acb->dev, scsi_sglist(cmd),
					scsi_sg_count(cmd), dir);

	ckc_only = 0;
      ckc_e:

	if (cmd->cmnd[0] == INQUIRY) {
		unsigned char *base = NULL;
		struct ScsiInqData *ptr;
		unsigned long flags = 0;
		struct scatterlist* sg = scsi_sglist(cmd);
		size_t offset = 0, len = sizeof(struct ScsiInqData);

		local_irq_save(flags);
		base = scsi_kmap_atomic_sg(sg, scsi_sg_count(cmd), &offset, &len);
		ptr = (struct ScsiInqData *)(base + offset);

		if (!ckc_only && (cmd->result & RES_DID) == 0
		    && cmd->cmnd[2] == 0 && scsi_bufflen(cmd) >= 8
		    && dir != PCI_DMA_NONE && ptr && (ptr->Vers & 0x07) >= 2)
			dcb->inquiry7 = ptr->Flags;

	
	
		if ((cmd->result == (DID_OK << 16)
		     || status_byte(cmd->result) &
		     CHECK_CONDITION)) {
			if (!dcb->init_tcq_flag) {
				add_dev(acb, dcb, ptr);
				dcb->init_tcq_flag = 1;
			}
		}

		scsi_kunmap_atomic_sg(base);
		local_irq_restore(flags);
	}

	
	scsi_set_resid(cmd, srb->total_xfer_length);
	
	cmd->SCp.this_residual = srb->total_xfer_length;
	cmd->SCp.buffers_residual = 0;
	if (debug_enabled(DBG_KG)) {
		if (srb->total_xfer_length)
			dprintkdbg(DBG_KG, "srb_done: (0x%p) <%02i-%i> "
				"cmnd=0x%02x Missed %i bytes\n",
				cmd, cmd->device->id, cmd->device->lun,
				cmd->cmnd[0], srb->total_xfer_length);
	}

	srb_going_remove(dcb, srb);
	
	if (srb == acb->tmp_srb)
		dprintkl(KERN_ERR, "srb_done: ERROR! Completed cmd with tmp_srb\n");
	else {
		dprintkdbg(DBG_0, "srb_done: (0x%p) done result=0x%08x\n",
			cmd, cmd->result);
		srb_free_insert(acb, srb);
	}
	pci_unmap_srb(acb, srb);

	cmd->scsi_done(cmd);
	waiting_process_next(acb);
}


static void doing_srb_done(struct AdapterCtlBlk *acb, u8 did_flag,
		struct scsi_cmnd *cmd, u8 force)
{
	struct DeviceCtlBlk *dcb;
	dprintkl(KERN_INFO, "doing_srb_done: pids ");

	list_for_each_entry(dcb, &acb->dcb_list, list) {
		struct ScsiReqBlk *srb;
		struct ScsiReqBlk *tmp;
		struct scsi_cmnd *p;

		list_for_each_entry_safe(srb, tmp, &dcb->srb_going_list, list) {
			enum dma_data_direction dir;
			int result;

			p = srb->cmd;
			dir = p->sc_data_direction;
			result = MK_RES(0, did_flag, 0, 0);
			printk("G:%p(%02i-%i) ", p,
			       p->device->id, p->device->lun);
			srb_going_remove(dcb, srb);
			free_tag(dcb, srb);
			srb_free_insert(acb, srb);
			p->result = result;
			pci_unmap_srb_sense(acb, srb);
			pci_unmap_srb(acb, srb);
			if (force) {
				p->scsi_done(p);
			}
		}
		if (!list_empty(&dcb->srb_going_list))
			dprintkl(KERN_DEBUG, 
			       "How could the ML send cmnds to the Going queue? <%02i-%i>\n",
			       dcb->target_id, dcb->target_lun);
		if (dcb->tag_mask)
			dprintkl(KERN_DEBUG,
			       "tag_mask for <%02i-%i> should be empty, is %08x!\n",
			       dcb->target_id, dcb->target_lun,
			       dcb->tag_mask);

		
		list_for_each_entry_safe(srb, tmp, &dcb->srb_waiting_list, list) {
			int result;
			p = srb->cmd;

			result = MK_RES(0, did_flag, 0, 0);
			printk("W:%p<%02i-%i>", p, p->device->id,
			       p->device->lun);
			srb_waiting_remove(dcb, srb);
			srb_free_insert(acb, srb);
			p->result = result;
			pci_unmap_srb_sense(acb, srb);
			pci_unmap_srb(acb, srb);
			if (force) {
				cmd->scsi_done(cmd);
			}
		}
		if (!list_empty(&dcb->srb_waiting_list))
			dprintkl(KERN_DEBUG, "ML queued %i cmnds again to <%02i-%i>\n",
			     list_size(&dcb->srb_waiting_list), dcb->target_id,
			     dcb->target_lun);
		dcb->flag &= ~ABORT_DEV_;
	}
	printk("\n");
}


static void reset_scsi_bus(struct AdapterCtlBlk *acb)
{
	dprintkdbg(DBG_0, "reset_scsi_bus: acb=%p\n", acb);
	acb->acb_flag |= RESET_DEV;	
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_RSTSCSI);

	while (!(DC395x_read8(acb, TRM_S1040_SCSI_INTSTATUS) & INT_SCSIRESET))
		;
}


static void set_basic_config(struct AdapterCtlBlk *acb)
{
	u8 bval;
	u16 wval;
	DC395x_write8(acb, TRM_S1040_SCSI_TIMEOUT, acb->sel_timeout);
	if (acb->config & HCC_PARITY)
		bval = PHASELATCH | INITIATOR | BLOCKRST | PARITYCHECK;
	else
		bval = PHASELATCH | INITIATOR | BLOCKRST;

	DC395x_write8(acb, TRM_S1040_SCSI_CONFIG0, bval);

	
	DC395x_write8(acb, TRM_S1040_SCSI_CONFIG1, 0x03);	
	
	DC395x_write8(acb, TRM_S1040_SCSI_HOSTID, acb->scsi_host->this_id);
	
	DC395x_write8(acb, TRM_S1040_SCSI_OFFSET, 0x00);
	
	wval = DC395x_read16(acb, TRM_S1040_GEN_CONTROL) & 0x7F;
	DC395x_write16(acb, TRM_S1040_GEN_CONTROL, wval);
	
	wval = DC395x_read16(acb, TRM_S1040_DMA_CONFIG) & ~DMA_FIFO_CTRL;
	wval |=
	    DMA_FIFO_HALF_HALF | DMA_ENHANCE  ;
	DC395x_write16(acb, TRM_S1040_DMA_CONFIG, wval);
	
	DC395x_read8(acb, TRM_S1040_SCSI_INTSTATUS);
	
	DC395x_write8(acb, TRM_S1040_SCSI_INTEN, 0x7F);
	DC395x_write8(acb, TRM_S1040_DMA_INTEN, EN_SCSIINTR | EN_DMAXFERERROR
		      
		      );
}


static void scsi_reset_detect(struct AdapterCtlBlk *acb)
{
	dprintkl(KERN_INFO, "scsi_reset_detect: acb=%p\n", acb);
	
	if (timer_pending(&acb->waiting_timer))
		del_timer(&acb->waiting_timer);

	DC395x_write8(acb, TRM_S1040_SCSI_CONTROL, DO_RSTMODULE);
	DC395x_write8(acb, TRM_S1040_DMA_CONTROL, DMARESETMODULE);
	
	udelay(500);
	
	acb->scsi_host->last_reset =
	    jiffies + 5 * HZ / 2 +
	    HZ * acb->eeprom.delay_time;

	clear_fifo(acb, "scsi_reset_detect");
	set_basic_config(acb);
	
	

	if (acb->acb_flag & RESET_DEV) {	
		acb->acb_flag |= RESET_DONE;
	} else {
		acb->acb_flag |= RESET_DETECT;
		reset_dev_param(acb);
		doing_srb_done(acb, DID_RESET, NULL, 1);
		
		acb->active_dcb = NULL;
		acb->acb_flag = 0;
		waiting_process_next(acb);
	}
}


static void request_sense(struct AdapterCtlBlk *acb, struct DeviceCtlBlk *dcb,
		struct ScsiReqBlk *srb)
{
	struct scsi_cmnd *cmd = srb->cmd;
	dprintkdbg(DBG_1, "request_sense: (0x%p) <%02i-%i>\n",
		cmd, cmd->device->id, cmd->device->lun);

	srb->flag |= AUTO_REQSENSE;
	srb->adapter_status = 0;
	srb->target_status = 0;

	
	memset(cmd->sense_buffer, 0, SCSI_SENSE_BUFFERSIZE);

	
	srb->segment_x[DC395x_MAX_SG_LISTENTRY - 1].address =
	    srb->segment_x[0].address;
	srb->segment_x[DC395x_MAX_SG_LISTENTRY - 1].length =
	    srb->segment_x[0].length;
	srb->xferred = srb->total_xfer_length;
	
	srb->total_xfer_length = SCSI_SENSE_BUFFERSIZE;
	srb->segment_x[0].length = SCSI_SENSE_BUFFERSIZE;
	
	srb->segment_x[0].address =
	    pci_map_single(acb->dev, cmd->sense_buffer,
			   SCSI_SENSE_BUFFERSIZE, PCI_DMA_FROMDEVICE);
	dprintkdbg(DBG_SG, "request_sense: map buffer %p->%08x(%05x)\n",
	       cmd->sense_buffer, srb->segment_x[0].address,
	       SCSI_SENSE_BUFFERSIZE);
	srb->sg_count = 1;
	srb->sg_index = 0;

	if (start_scsi(acb, dcb, srb)) {	
		dprintkl(KERN_DEBUG,
			"request_sense: (0x%p) failed <%02i-%i>\n",
			srb->cmd, dcb->target_id, dcb->target_lun);
		srb_going_to_waiting_move(dcb, srb);
		waiting_set_timer(acb, HZ / 100);
	}
}


static struct DeviceCtlBlk *device_alloc(struct AdapterCtlBlk *acb,
		u8 target, u8 lun)
{
	struct NvRamType *eeprom = &acb->eeprom;
	u8 period_index = eeprom->target[target].period & 0x07;
	struct DeviceCtlBlk *dcb;

	dcb = kmalloc(sizeof(struct DeviceCtlBlk), GFP_ATOMIC);
	dprintkdbg(DBG_0, "device_alloc: <%02i-%i>\n", target, lun);
	if (!dcb)
		return NULL;
	dcb->acb = NULL;
	INIT_LIST_HEAD(&dcb->srb_going_list);
	INIT_LIST_HEAD(&dcb->srb_waiting_list);
	dcb->active_srb = NULL;
	dcb->tag_mask = 0;
	dcb->max_command = 1;
	dcb->target_id = target;
	dcb->target_lun = lun;
#ifndef DC395x_NO_DISCONNECT
	dcb->identify_msg =
	    IDENTIFY(dcb->dev_mode & NTC_DO_DISCONNECT, lun);
#else
	dcb->identify_msg = IDENTIFY(0, lun);
#endif
	dcb->dev_mode = eeprom->target[target].cfg0;
	dcb->inquiry7 = 0;
	dcb->sync_mode = 0;
	dcb->min_nego_period = clock_period[period_index];
	dcb->sync_period = 0;
	dcb->sync_offset = 0;
	dcb->flag = 0;

#ifndef DC395x_NO_WIDE
	if ((dcb->dev_mode & NTC_DO_WIDE_NEGO)
	    && (acb->config & HCC_WIDE_CARD))
		dcb->sync_mode |= WIDE_NEGO_ENABLE;
#endif
#ifndef DC395x_NO_SYNC
	if (dcb->dev_mode & NTC_DO_SYNC_NEGO)
		if (!(lun) || current_sync_offset)
			dcb->sync_mode |= SYNC_NEGO_ENABLE;
#endif
	if (dcb->target_lun != 0) {
		
		struct DeviceCtlBlk *p;
		list_for_each_entry(p, &acb->dcb_list, list)
			if (p->target_id == dcb->target_id)
				break;
		dprintkdbg(DBG_1, 
		       "device_alloc: <%02i-%i> copy from <%02i-%i>\n",
		       dcb->target_id, dcb->target_lun,
		       p->target_id, p->target_lun);
		dcb->sync_mode = p->sync_mode;
		dcb->sync_period = p->sync_period;
		dcb->min_nego_period = p->min_nego_period;
		dcb->sync_offset = p->sync_offset;
		dcb->inquiry7 = p->inquiry7;
	}
	return dcb;
}


static void adapter_add_device(struct AdapterCtlBlk *acb,
		struct DeviceCtlBlk *dcb)
{
	
	dcb->acb = acb;
	
	
	if (list_empty(&acb->dcb_list))
		acb->dcb_run_robin = dcb;

	
	list_add_tail(&dcb->list, &acb->dcb_list);

	
	acb->dcb_map[dcb->target_id] |= (1 << dcb->target_lun);
	acb->children[dcb->target_id][dcb->target_lun] = dcb;
}


static void adapter_remove_device(struct AdapterCtlBlk *acb,
		struct DeviceCtlBlk *dcb)
{
	struct DeviceCtlBlk *i;
	struct DeviceCtlBlk *tmp;
	dprintkdbg(DBG_0, "adapter_remove_device: <%02i-%i>\n",
		dcb->target_id, dcb->target_lun);

	
	if (acb->active_dcb == dcb)
		acb->active_dcb = NULL;
	if (acb->dcb_run_robin == dcb)
		acb->dcb_run_robin = dcb_get_next(&acb->dcb_list, dcb);

	
	list_for_each_entry_safe(i, tmp, &acb->dcb_list, list)
		if (dcb == i) {
			list_del(&i->list);
			break;
		}

		
	acb->dcb_map[dcb->target_id] &= ~(1 << dcb->target_lun);
	acb->children[dcb->target_id][dcb->target_lun] = NULL;
	dcb->acb = NULL;
}


static void adapter_remove_and_free_device(struct AdapterCtlBlk *acb,
		struct DeviceCtlBlk *dcb)
{
	if (list_size(&dcb->srb_going_list) > 1) {
		dprintkdbg(DBG_1, "adapter_remove_and_free_device: <%02i-%i> "
		           "Won't remove because of %i active requests.\n",
			   dcb->target_id, dcb->target_lun,
			   list_size(&dcb->srb_going_list));
		return;
	}
	adapter_remove_device(acb, dcb);
	kfree(dcb);
}


static void adapter_remove_and_free_all_devices(struct AdapterCtlBlk* acb)
{
	struct DeviceCtlBlk *dcb;
	struct DeviceCtlBlk *tmp;
	dprintkdbg(DBG_1, "adapter_remove_and_free_all_devices: num=%i\n",
		   list_size(&acb->dcb_list));

	list_for_each_entry_safe(dcb, tmp, &acb->dcb_list, list)
		adapter_remove_and_free_device(acb, dcb);
}


static int dc395x_slave_alloc(struct scsi_device *scsi_device)
{
	struct AdapterCtlBlk *acb = (struct AdapterCtlBlk *)scsi_device->host->hostdata;
	struct DeviceCtlBlk *dcb;

	dcb = device_alloc(acb, scsi_device->id, scsi_device->lun);
	if (!dcb)
		return -ENOMEM;
	adapter_add_device(acb, dcb);

	return 0;
}


static void dc395x_slave_destroy(struct scsi_device *scsi_device)
{
	struct AdapterCtlBlk *acb = (struct AdapterCtlBlk *)scsi_device->host->hostdata;
	struct DeviceCtlBlk *dcb = find_dcb(acb, scsi_device->id, scsi_device->lun);
	if (dcb)
		adapter_remove_and_free_device(acb, dcb);
}




static void __devinit trms1040_wait_30us(unsigned long io_port)
{
	
	outb(5, io_port + TRM_S1040_GEN_TIMER);
	while (!(inb(io_port + TRM_S1040_GEN_STATUS) & GTIMEOUT))
		 ;
}


static void __devinit trms1040_write_cmd(unsigned long io_port, u8 cmd, u8 addr)
{
	int i;
	u8 send_data;

	
	for (i = 0; i < 3; i++, cmd <<= 1) {
		send_data = NVR_SELECT;
		if (cmd & 0x04)	
			send_data |= NVR_BITOUT;

		outb(send_data, io_port + TRM_S1040_GEN_NVRAM);
		trms1040_wait_30us(io_port);
		outb((send_data | NVR_CLOCK),
		     io_port + TRM_S1040_GEN_NVRAM);
		trms1040_wait_30us(io_port);
	}

	
	for (i = 0; i < 7; i++, addr <<= 1) {
		send_data = NVR_SELECT;
		if (addr & 0x40)	
			send_data |= NVR_BITOUT;

		outb(send_data, io_port + TRM_S1040_GEN_NVRAM);
		trms1040_wait_30us(io_port);
		outb((send_data | NVR_CLOCK),
		     io_port + TRM_S1040_GEN_NVRAM);
		trms1040_wait_30us(io_port);
	}
	outb(NVR_SELECT, io_port + TRM_S1040_GEN_NVRAM);
	trms1040_wait_30us(io_port);
}


static void __devinit trms1040_set_data(unsigned long io_port, u8 addr, u8 byte)
{
	int i;
	u8 send_data;

	
	trms1040_write_cmd(io_port, 0x05, addr);

	
	for (i = 0; i < 8; i++, byte <<= 1) {
		send_data = NVR_SELECT;
		if (byte & 0x80)	
			send_data |= NVR_BITOUT;

		outb(send_data, io_port + TRM_S1040_GEN_NVRAM);
		trms1040_wait_30us(io_port);
		outb((send_data | NVR_CLOCK), io_port + TRM_S1040_GEN_NVRAM);
		trms1040_wait_30us(io_port);
	}
	outb(NVR_SELECT, io_port + TRM_S1040_GEN_NVRAM);
	trms1040_wait_30us(io_port);

	
	outb(0, io_port + TRM_S1040_GEN_NVRAM);
	trms1040_wait_30us(io_port);

	outb(NVR_SELECT, io_port + TRM_S1040_GEN_NVRAM);
	trms1040_wait_30us(io_port);

	
	while (1) {
		outb((NVR_SELECT | NVR_CLOCK), io_port + TRM_S1040_GEN_NVRAM);
		trms1040_wait_30us(io_port);

		outb(NVR_SELECT, io_port + TRM_S1040_GEN_NVRAM);
		trms1040_wait_30us(io_port);

		if (inb(io_port + TRM_S1040_GEN_NVRAM) & NVR_BITIN)
			break;
	}

	
	outb(0, io_port + TRM_S1040_GEN_NVRAM);
}


static void __devinit trms1040_write_all(struct NvRamType *eeprom, unsigned long io_port)
{
	u8 *b_eeprom = (u8 *)eeprom;
	u8 addr;

	
	outb((inb(io_port + TRM_S1040_GEN_CONTROL) | EN_EEPROM),
	     io_port + TRM_S1040_GEN_CONTROL);

	
	trms1040_write_cmd(io_port, 0x04, 0xFF);
	outb(0, io_port + TRM_S1040_GEN_NVRAM);
	trms1040_wait_30us(io_port);

	
	for (addr = 0; addr < 128; addr++, b_eeprom++)
		trms1040_set_data(io_port, addr, *b_eeprom);

	
	trms1040_write_cmd(io_port, 0x04, 0x00);
	outb(0, io_port + TRM_S1040_GEN_NVRAM);
	trms1040_wait_30us(io_port);

	
	outb((inb(io_port + TRM_S1040_GEN_CONTROL) & ~EN_EEPROM),
	     io_port + TRM_S1040_GEN_CONTROL);
}


static u8 __devinit trms1040_get_data(unsigned long io_port, u8 addr)
{
	int i;
	u8 read_byte;
	u8 result = 0;

	
	trms1040_write_cmd(io_port, 0x06, addr);

	
	for (i = 0; i < 8; i++) {
		outb((NVR_SELECT | NVR_CLOCK), io_port + TRM_S1040_GEN_NVRAM);
		trms1040_wait_30us(io_port);
		outb(NVR_SELECT, io_port + TRM_S1040_GEN_NVRAM);

		
		read_byte = inb(io_port + TRM_S1040_GEN_NVRAM);
		result <<= 1;
		if (read_byte & NVR_BITIN)
			result |= 1;

		trms1040_wait_30us(io_port);
	}

	
	outb(0, io_port + TRM_S1040_GEN_NVRAM);
	return result;
}


static void __devinit trms1040_read_all(struct NvRamType *eeprom, unsigned long io_port)
{
	u8 *b_eeprom = (u8 *)eeprom;
	u8 addr;

	
	outb((inb(io_port + TRM_S1040_GEN_CONTROL) | EN_EEPROM),
	     io_port + TRM_S1040_GEN_CONTROL);

	
	for (addr = 0; addr < 128; addr++, b_eeprom++)
		*b_eeprom = trms1040_get_data(io_port, addr);

	
	outb((inb(io_port + TRM_S1040_GEN_CONTROL) & ~EN_EEPROM),
	     io_port + TRM_S1040_GEN_CONTROL);
}



static void __devinit check_eeprom(struct NvRamType *eeprom, unsigned long io_port)
{
	u16 *w_eeprom = (u16 *)eeprom;
	u16 w_addr;
	u16 cksum;
	u32 d_addr;
	u32 *d_eeprom;

	trms1040_read_all(eeprom, io_port);	

	cksum = 0;
	for (w_addr = 0, w_eeprom = (u16 *)eeprom; w_addr < 64;
	     w_addr++, w_eeprom++)
		cksum += *w_eeprom;
	if (cksum != 0x1234) {
		dprintkl(KERN_WARNING,
			"EEProm checksum error: using default values and options.\n");
		eeprom->sub_vendor_id[0] = (u8)PCI_VENDOR_ID_TEKRAM;
		eeprom->sub_vendor_id[1] = (u8)(PCI_VENDOR_ID_TEKRAM >> 8);
		eeprom->sub_sys_id[0] = (u8)PCI_DEVICE_ID_TEKRAM_TRMS1040;
		eeprom->sub_sys_id[1] =
		    (u8)(PCI_DEVICE_ID_TEKRAM_TRMS1040 >> 8);
		eeprom->sub_class = 0x00;
		eeprom->vendor_id[0] = (u8)PCI_VENDOR_ID_TEKRAM;
		eeprom->vendor_id[1] = (u8)(PCI_VENDOR_ID_TEKRAM >> 8);
		eeprom->device_id[0] = (u8)PCI_DEVICE_ID_TEKRAM_TRMS1040;
		eeprom->device_id[1] =
		    (u8)(PCI_DEVICE_ID_TEKRAM_TRMS1040 >> 8);
		eeprom->reserved = 0x00;

		for (d_addr = 0, d_eeprom = (u32 *)eeprom->target;
		     d_addr < 16; d_addr++, d_eeprom++)
			*d_eeprom = 0x00000077;	

		*d_eeprom++ = 0x04000F07;	
		*d_eeprom++ = 0x00000015;	
		for (d_addr = 0; d_addr < 12; d_addr++, d_eeprom++)
			*d_eeprom = 0x00;

		
		set_safe_settings();
		fix_settings();
		eeprom_override(eeprom);

		eeprom->cksum = 0x00;
		for (w_addr = 0, cksum = 0, w_eeprom = (u16 *)eeprom;
		     w_addr < 63; w_addr++, w_eeprom++)
			cksum += *w_eeprom;

		*w_eeprom = 0x1234 - cksum;
		trms1040_write_all(eeprom, io_port);
		eeprom->delay_time = cfg_data[CFG_RESET_DELAY].value;
	} else {
		set_safe_settings();
		eeprom_index_to_delay(eeprom);
		eeprom_override(eeprom);
	}
}


static void __devinit print_eeprom_settings(struct NvRamType *eeprom)
{
	dprintkl(KERN_INFO, "Used settings: AdapterID=%02i, Speed=%i(%02i.%01iMHz), dev_mode=0x%02x\n",
		eeprom->scsi_id,
		eeprom->target[0].period,
		clock_speed[eeprom->target[0].period] / 10,
		clock_speed[eeprom->target[0].period] % 10,
		eeprom->target[0].cfg0);
	dprintkl(KERN_INFO, "               AdaptMode=0x%02x, Tags=%i(%02i), DelayReset=%is\n",
		eeprom->channel_cfg, eeprom->max_tag,
		1 << eeprom->max_tag, eeprom->delay_time);
}


static void adapter_sg_tables_free(struct AdapterCtlBlk *acb)
{
	int i;
	const unsigned srbs_per_page = PAGE_SIZE/SEGMENTX_LEN;

	for (i = 0; i < DC395x_MAX_SRB_CNT; i += srbs_per_page)
		kfree(acb->srb_array[i].segment_x);
}


static int __devinit adapter_sg_tables_alloc(struct AdapterCtlBlk *acb)
{
	const unsigned mem_needed = (DC395x_MAX_SRB_CNT+1)
	                            *SEGMENTX_LEN;
	int pages = (mem_needed+(PAGE_SIZE-1))/PAGE_SIZE;
	const unsigned srbs_per_page = PAGE_SIZE/SEGMENTX_LEN;
	int srb_idx = 0;
	unsigned i = 0;
	struct SGentry *uninitialized_var(ptr);

	for (i = 0; i < DC395x_MAX_SRB_CNT; i++)
		acb->srb_array[i].segment_x = NULL;

	dprintkdbg(DBG_1, "Allocate %i pages for SG tables\n", pages);
	while (pages--) {
		ptr = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!ptr) {
			adapter_sg_tables_free(acb);
			return 1;
		}
		dprintkdbg(DBG_1, "Allocate %li bytes at %p for SG segments %i\n",
			PAGE_SIZE, ptr, srb_idx);
		i = 0;
		while (i < srbs_per_page && srb_idx < DC395x_MAX_SRB_CNT)
			acb->srb_array[srb_idx++].segment_x =
			    ptr + (i++ * DC395x_MAX_SG_LISTENTRY);
	}
	if (i < srbs_per_page)
		acb->srb.segment_x =
		    ptr + (i * DC395x_MAX_SG_LISTENTRY);
	else
		dprintkl(KERN_DEBUG, "No space for tmsrb SG table reserved?!\n");
	return 0;
}



static void __devinit adapter_print_config(struct AdapterCtlBlk *acb)
{
	u8 bval;

	bval = DC395x_read8(acb, TRM_S1040_GEN_STATUS);
	dprintkl(KERN_INFO, "%sConnectors: ",
		((bval & WIDESCSI) ? "(Wide) " : ""));
	if (!(bval & CON5068))
		printk("ext%s ", !(bval & EXT68HIGH) ? "68" : "50");
	if (!(bval & CON68))
		printk("int68%s ", !(bval & INT68HIGH) ? "" : "(50)");
	if (!(bval & CON50))
		printk("int50 ");
	if ((bval & (CON5068 | CON50 | CON68)) ==
	    0  )
		printk(" Oops! (All 3?) ");
	bval = DC395x_read8(acb, TRM_S1040_GEN_CONTROL);
	printk(" Termination: ");
	if (bval & DIS_TERM)
		printk("Disabled\n");
	else {
		if (bval & AUTOTERM)
			printk("Auto ");
		if (bval & LOW8TERM)
			printk("Low ");
		if (bval & UP8TERM)
			printk("High ");
		printk("\n");
	}
}


static void __devinit adapter_init_params(struct AdapterCtlBlk *acb)
{
	struct NvRamType *eeprom = &acb->eeprom;
	int i;

	
	
	

	INIT_LIST_HEAD(&acb->dcb_list);
	acb->dcb_run_robin = NULL;
	acb->active_dcb = NULL;

	INIT_LIST_HEAD(&acb->srb_free_list);
	
	acb->tmp_srb = &acb->srb;
	init_timer(&acb->waiting_timer);
	init_timer(&acb->selto_timer);

	acb->srb_count = DC395x_MAX_SRB_CNT;

	acb->sel_timeout = DC395x_SEL_TIMEOUT;	
	

	acb->tag_max_num = 1 << eeprom->max_tag;
	if (acb->tag_max_num > 30)
		acb->tag_max_num = 30;

	acb->acb_flag = 0;	
	acb->gmode2 = eeprom->channel_cfg;
	acb->config = 0;	

	if (eeprom->channel_cfg & NAC_SCANLUN)
		acb->lun_chk = 1;
	acb->scan_devices = 1;

	acb->scsi_host->this_id = eeprom->scsi_id;
	acb->hostid_bit = (1 << acb->scsi_host->this_id);

	for (i = 0; i < DC395x_MAX_SCSI_ID; i++)
		acb->dcb_map[i] = 0;

	acb->msg_len = 0;
	
	
	for (i = 0; i < acb->srb_count - 1; i++)
		srb_free_insert(acb, &acb->srb_array[i]);
}


static void __devinit adapter_init_scsi_host(struct Scsi_Host *host)
{
        struct AdapterCtlBlk *acb = (struct AdapterCtlBlk *)host->hostdata;
	struct NvRamType *eeprom = &acb->eeprom;
        
	host->max_cmd_len = 24;
	host->can_queue = DC395x_MAX_CMD_QUEUE;
	host->cmd_per_lun = DC395x_MAX_CMD_PER_LUN;
	host->this_id = (int)eeprom->scsi_id;
	host->io_port = acb->io_port_base;
	host->n_io_port = acb->io_port_len;
	host->dma_channel = -1;
	host->unique_id = acb->io_port_base;
	host->irq = acb->irq_level;
	host->last_reset = jiffies;

	host->max_id = 16;
	if (host->max_id - 1 == eeprom->scsi_id)
		host->max_id--;

#ifdef CONFIG_SCSI_MULTI_LUN
	if (eeprom->channel_cfg & NAC_SCANLUN)
		host->max_lun = 8;
	else
		host->max_lun = 1;
#else
	host->max_lun = 1;
#endif

}


static void __devinit adapter_init_chip(struct AdapterCtlBlk *acb)
{
        struct NvRamType *eeprom = &acb->eeprom;
        
        
	DC395x_write8(acb, TRM_S1040_DMA_INTEN, 0x00);
	DC395x_write8(acb, TRM_S1040_SCSI_INTEN, 0x00);

	
	DC395x_write16(acb, TRM_S1040_SCSI_CONTROL, DO_RSTMODULE);

	
	DC395x_write8(acb, TRM_S1040_DMA_CONTROL, DMARESETMODULE);
	udelay(20);

	
	acb->config = HCC_AUTOTERM | HCC_PARITY;
	if (DC395x_read8(acb, TRM_S1040_GEN_STATUS) & WIDESCSI)
		acb->config |= HCC_WIDE_CARD;

	if (eeprom->channel_cfg & NAC_POWERON_SCSI_RESET)
		acb->config |= HCC_SCSI_RESET;

	if (acb->config & HCC_SCSI_RESET) {
		dprintkl(KERN_INFO, "Performing initial SCSI bus reset\n");
		DC395x_write8(acb, TRM_S1040_SCSI_CONTROL, DO_RSTSCSI);

		
		
		udelay(500);

		acb->scsi_host->last_reset =
		    jiffies + HZ / 2 +
		    HZ * acb->eeprom.delay_time;

		
	}
}


static int __devinit adapter_init(struct AdapterCtlBlk *acb,
	unsigned long io_port, u32 io_port_len, unsigned int irq)
{
	if (!request_region(io_port, io_port_len, DC395X_NAME)) {
		dprintkl(KERN_ERR, "Failed to reserve IO region 0x%lx\n", io_port);
		goto failed;
	}
	
	acb->io_port_base = io_port;
	acb->io_port_len = io_port_len;
	
	if (request_irq(irq, dc395x_interrupt, IRQF_SHARED, DC395X_NAME, acb)) {
	    	
		dprintkl(KERN_INFO, "Failed to register IRQ\n");
		goto failed;
	}
	
	acb->irq_level = irq;

	
	check_eeprom(&acb->eeprom, io_port);
 	print_eeprom_settings(&acb->eeprom);

		
	adapter_init_params(acb);
	
	
 	adapter_print_config(acb);

	if (adapter_sg_tables_alloc(acb)) {
		dprintkl(KERN_DEBUG, "Memory allocation for SG tables failed\n");
		goto failed;
	}
	adapter_init_scsi_host(acb->scsi_host);
	adapter_init_chip(acb);
	set_basic_config(acb);

	dprintkdbg(DBG_0,
		"adapter_init: acb=%p, pdcb_map=%p psrb_array=%p "
		"size{acb=0x%04x dcb=0x%04x srb=0x%04x}\n",
		acb, acb->dcb_map, acb->srb_array, sizeof(struct AdapterCtlBlk),
		sizeof(struct DeviceCtlBlk), sizeof(struct ScsiReqBlk));
	return 0;

failed:
	if (acb->irq_level)
		free_irq(acb->irq_level, acb);
	if (acb->io_port_base)
		release_region(acb->io_port_base, acb->io_port_len);
	adapter_sg_tables_free(acb);

	return 1;
}


static void adapter_uninit_chip(struct AdapterCtlBlk *acb)
{
	
	DC395x_write8(acb, TRM_S1040_DMA_INTEN, 0);
	DC395x_write8(acb, TRM_S1040_SCSI_INTEN, 0);

	
	if (acb->config & HCC_SCSI_RESET)
		reset_scsi_bus(acb);

	
	DC395x_read8(acb, TRM_S1040_SCSI_INTSTATUS);
}



static void adapter_uninit(struct AdapterCtlBlk *acb)
{
	unsigned long flags;
	DC395x_LOCK_IO(acb->scsi_host, flags);

	
	if (timer_pending(&acb->waiting_timer))
		del_timer(&acb->waiting_timer);
	if (timer_pending(&acb->selto_timer))
		del_timer(&acb->selto_timer);

	adapter_uninit_chip(acb);
	adapter_remove_and_free_all_devices(acb);
	DC395x_UNLOCK_IO(acb->scsi_host, flags);

	if (acb->irq_level)
		free_irq(acb->irq_level, acb);
	if (acb->io_port_base)
		release_region(acb->io_port_base, acb->io_port_len);

	adapter_sg_tables_free(acb);
}


#undef SPRINTF
#define SPRINTF(args...) pos += sprintf(pos, args)

#undef YESNO
#define YESNO(YN) \
 if (YN) SPRINTF(" Yes ");\
 else SPRINTF(" No  ")

static int dc395x_proc_info(struct Scsi_Host *host, char *buffer,
		char **start, off_t offset, int length, int inout)
{
	struct AdapterCtlBlk *acb = (struct AdapterCtlBlk *)host->hostdata;
	int spd, spd1;
	char *pos = buffer;
	struct DeviceCtlBlk *dcb;
	unsigned long flags;
	int dev;

	if (inout)		/* Has data been written to the file ? */
		return -EPERM;

	SPRINTF(DC395X_BANNER " PCI SCSI Host Adapter\n");
	SPRINTF(" Driver Version " DC395X_VERSION "\n");

	DC395x_LOCK_IO(acb->scsi_host, flags);

	SPRINTF("SCSI Host Nr %i, ", host->host_no);
	SPRINTF("DC395U/UW/F DC315/U %s\n",
		(acb->config & HCC_WIDE_CARD) ? "Wide" : "");
	SPRINTF("io_port_base 0x%04lx, ", acb->io_port_base);
	SPRINTF("irq_level 0x%04x, ", acb->irq_level);
	SPRINTF(" SelTimeout %ims\n", (1638 * acb->sel_timeout) / 1000);

	SPRINTF("MaxID %i, MaxLUN %i, ", host->max_id, host->max_lun);
	SPRINTF("AdapterID %i\n", host->this_id);

	SPRINTF("tag_max_num %i", acb->tag_max_num);
	
	SPRINTF(", FilterCfg 0x%02x",
		DC395x_read8(acb, TRM_S1040_SCSI_CONFIG1));
	SPRINTF(", DelayReset %is\n", acb->eeprom.delay_time);
	

	SPRINTF("Nr of DCBs: %i\n", list_size(&acb->dcb_list));
	SPRINTF
	    ("Map of attached LUNs: %02x %02x %02x %02x %02x %02x %02x %02x\n",
	     acb->dcb_map[0], acb->dcb_map[1], acb->dcb_map[2],
	     acb->dcb_map[3], acb->dcb_map[4], acb->dcb_map[5],
	     acb->dcb_map[6], acb->dcb_map[7]);
	SPRINTF
	    ("                      %02x %02x %02x %02x %02x %02x %02x %02x\n",
	     acb->dcb_map[8], acb->dcb_map[9], acb->dcb_map[10],
	     acb->dcb_map[11], acb->dcb_map[12], acb->dcb_map[13],
	     acb->dcb_map[14], acb->dcb_map[15]);

	SPRINTF
	    ("Un ID LUN Prty Sync Wide DsCn SndS TagQ nego_period SyncFreq SyncOffs MaxCmd\n");

	dev = 0;
	list_for_each_entry(dcb, &acb->dcb_list, list) {
		int nego_period;
		SPRINTF("%02i %02i  %02i ", dev, dcb->target_id,
			dcb->target_lun);
		YESNO(dcb->dev_mode & NTC_DO_PARITY_CHK);
		YESNO(dcb->sync_offset);
		YESNO(dcb->sync_period & WIDE_SYNC);
		YESNO(dcb->dev_mode & NTC_DO_DISCONNECT);
		YESNO(dcb->dev_mode & NTC_DO_SEND_START);
		YESNO(dcb->sync_mode & EN_TAG_QUEUEING);
		nego_period = clock_period[dcb->sync_period & 0x07] << 2;
		if (dcb->sync_offset)
			SPRINTF("  %03i ns ", nego_period);
		else
			SPRINTF(" (%03i ns)", (dcb->min_nego_period << 2));

		if (dcb->sync_offset & 0x0f) {
			spd = 1000 / (nego_period);
			spd1 = 1000 % (nego_period);
			spd1 = (spd1 * 10 + nego_period / 2) / (nego_period);
			SPRINTF("   %2i.%1i M     %02i ", spd, spd1,
				(dcb->sync_offset & 0x0f));
		} else
			SPRINTF("                 ");

		
		SPRINTF("     %02i\n", dcb->max_command);
		dev++;
	}

	if (timer_pending(&acb->waiting_timer))
		SPRINTF("Waiting queue timer running\n");
	else
		SPRINTF("\n");

	list_for_each_entry(dcb, &acb->dcb_list, list) {
		struct ScsiReqBlk *srb;
		if (!list_empty(&dcb->srb_waiting_list))
			SPRINTF("DCB (%02i-%i): Waiting: %i:",
				dcb->target_id, dcb->target_lun,
				list_size(&dcb->srb_waiting_list));
                list_for_each_entry(srb, &dcb->srb_waiting_list, list)
			SPRINTF(" %p", srb->cmd);
		if (!list_empty(&dcb->srb_going_list))
			SPRINTF("\nDCB (%02i-%i): Going  : %i:",
				dcb->target_id, dcb->target_lun,
				list_size(&dcb->srb_going_list));
		list_for_each_entry(srb, &dcb->srb_going_list, list)
			SPRINTF(" %p", srb->cmd);
		if (!list_empty(&dcb->srb_waiting_list) || !list_empty(&dcb->srb_going_list))
			SPRINTF("\n");
	}

	if (debug_enabled(DBG_1)) {
		SPRINTF("DCB list for ACB %p:\n", acb);
		list_for_each_entry(dcb, &acb->dcb_list, list) {
			SPRINTF("%p -> ", dcb);
		}
		SPRINTF("END\n");
	}

	*start = buffer + offset;
	DC395x_UNLOCK_IO(acb->scsi_host, flags);

	if (pos - buffer < offset)
		return 0;
	else if (pos - buffer - offset < length)
		return pos - buffer - offset;
	else
		return length;
}


static struct scsi_host_template dc395x_driver_template = {
	.module                 = THIS_MODULE,
	.proc_name              = DC395X_NAME,
	.proc_info              = dc395x_proc_info,
	.name                   = DC395X_BANNER " " DC395X_VERSION,
	.queuecommand           = dc395x_queue_command,
	.bios_param             = dc395x_bios_param,
	.slave_alloc            = dc395x_slave_alloc,
	.slave_destroy          = dc395x_slave_destroy,
	.can_queue              = DC395x_MAX_CAN_QUEUE,
	.this_id                = 7,
	.sg_tablesize           = DC395x_MAX_SG_TABLESIZE,
	.cmd_per_lun            = DC395x_MAX_CMD_PER_LUN,
	.eh_abort_handler       = dc395x_eh_abort,
	.eh_bus_reset_handler   = dc395x_eh_bus_reset,
	.use_clustering         = DISABLE_CLUSTERING,
};


static void banner_display(void)
{
	static int banner_done = 0;
	if (!banner_done)
	{
		dprintkl(KERN_INFO, "%s %s\n", DC395X_BANNER, DC395X_VERSION);
		banner_done = 1;
	}
}


static int __devinit dc395x_init_one(struct pci_dev *dev,
		const struct pci_device_id *id)
{
	struct Scsi_Host *scsi_host = NULL;
	struct AdapterCtlBlk *acb = NULL;
	unsigned long io_port_base;
	unsigned int io_port_len;
	unsigned int irq;
	
	dprintkdbg(DBG_0, "Init one instance (%s)\n", pci_name(dev));
	banner_display();

	if (pci_enable_device(dev))
	{
		dprintkl(KERN_INFO, "PCI Enable device failed.\n");
		return -ENODEV;
	}
	io_port_base = pci_resource_start(dev, 0) & PCI_BASE_ADDRESS_IO_MASK;
	io_port_len = pci_resource_len(dev, 0);
	irq = dev->irq;
	dprintkdbg(DBG_0, "IO_PORT=0x%04lx, IRQ=0x%x\n", io_port_base, dev->irq);

	
	scsi_host = scsi_host_alloc(&dc395x_driver_template,
				    sizeof(struct AdapterCtlBlk));
	if (!scsi_host) {
		dprintkl(KERN_INFO, "scsi_host_alloc failed\n");
		goto fail;
	}
 	acb = (struct AdapterCtlBlk*)scsi_host->hostdata;
 	acb->scsi_host = scsi_host;
 	acb->dev = dev;

	
 	if (adapter_init(acb, io_port_base, io_port_len, irq)) {
		dprintkl(KERN_INFO, "adapter init failed\n");
		goto fail;
	}

	pci_set_master(dev);

	
	if (scsi_add_host(scsi_host, &dev->dev)) {
		dprintkl(KERN_ERR, "scsi_add_host failed\n");
		goto fail;
	}
	pci_set_drvdata(dev, scsi_host);
	scsi_scan_host(scsi_host);
        	
	return 0;

fail:
	if (acb != NULL)
		adapter_uninit(acb);
	if (scsi_host != NULL)
		scsi_host_put(scsi_host);
	pci_disable_device(dev);
	return -ENODEV;
}


static void __devexit dc395x_remove_one(struct pci_dev *dev)
{
	struct Scsi_Host *scsi_host = pci_get_drvdata(dev);
	struct AdapterCtlBlk *acb = (struct AdapterCtlBlk *)(scsi_host->hostdata);

	dprintkdbg(DBG_0, "dc395x_remove_one: acb=%p\n", acb);

	scsi_remove_host(scsi_host);
	adapter_uninit(acb);
	pci_disable_device(dev);
	scsi_host_put(scsi_host);
	pci_set_drvdata(dev, NULL);
}


static struct pci_device_id dc395x_pci_table[] = {
	{
		.vendor		= PCI_VENDOR_ID_TEKRAM,
		.device		= PCI_DEVICE_ID_TEKRAM_TRMS1040,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
	 },
	{}			
};
MODULE_DEVICE_TABLE(pci, dc395x_pci_table);


static struct pci_driver dc395x_driver = {
	.name           = DC395X_NAME,
	.id_table       = dc395x_pci_table,
	.probe          = dc395x_init_one,
	.remove         = __devexit_p(dc395x_remove_one),
};


static int __init dc395x_module_init(void)
{
	return pci_register_driver(&dc395x_driver);
}


static void __exit dc395x_module_exit(void)
{
	pci_unregister_driver(&dc395x_driver);
}


module_init(dc395x_module_init);
module_exit(dc395x_module_exit);

MODULE_AUTHOR("C.L. Huang / Erich Chen / Kurt Garloff");
MODULE_DESCRIPTION("SCSI host adapter driver for Tekram TRM-S1040 based adapters: Tekram DC395 and DC315 series");
MODULE_LICENSE("GPL");
