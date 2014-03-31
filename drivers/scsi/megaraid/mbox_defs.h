/*
 *
 *			Linux MegaRAID Unified device driver
 *
 * Copyright (c) 2003-2004  LSI Logic Corporation.
 *
 *	   This program is free software; you can redistribute it and/or
 *	   modify it under the terms of the GNU General Public License
 *	   as published by the Free Software Foundation; either version
 *	   2 of the License, or (at your option) any later version.
 *
 * FILE		: mbox_defs.h
 *
 */
#ifndef _MRAID_MBOX_DEFS_H_
#define _MRAID_MBOX_DEFS_H_

#include <linux/types.h>


#define MBOXCMD_LREAD		0x01
#define MBOXCMD_LWRITE		0x02
#define MBOXCMD_PASSTHRU	0x03
#define MBOXCMD_ADPEXTINQ	0x04
#define MBOXCMD_ADAPTERINQ	0x05
#define MBOXCMD_LREAD64		0xA7
#define MBOXCMD_LWRITE64	0xA8
#define MBOXCMD_PASSTHRU64	0xC3
#define MBOXCMD_EXTPTHRU	0xE3

#define MAIN_MISC_OPCODE	0xA4
#define GET_MAX_SG_SUPPORT	0x01
#define SUPPORT_EXT_CDB		0x16

#define FC_NEW_CONFIG		0xA1
#define NC_SUBOP_PRODUCT_INFO	0x0E
#define NC_SUBOP_ENQUIRY3	0x0F
#define ENQ3_GET_SOLICITED_FULL	0x02
#define OP_DCMD_READ_CONFIG	0x04
#define NEW_READ_CONFIG_8LD	0x67
#define READ_CONFIG_8LD		0x07
#define FLUSH_ADAPTER		0x0A
#define FLUSH_SYSTEM		0xFE

#define	FC_DEL_LOGDRV		0xA4
#define	OP_SUP_DEL_LOGDRV	0x2A
#define OP_GET_LDID_MAP		0x18
#define OP_DEL_LOGDRV		0x1C

#define IS_BIOS_ENABLED		0x62
#define GET_BIOS		0x01
#define CHNL_CLASS		0xA9
#define GET_CHNL_CLASS		0x00
#define SET_CHNL_CLASS		0x01
#define CH_RAID			0x01
#define CH_SCSI			0x00
#define BIOS_PVT_DATA		0x40
#define GET_BIOS_PVT_DATA	0x00


#define GET_TARGET_ID		0x7D
#define CLUSTER_OP		0x70
#define GET_CLUSTER_MODE	0x02
#define CLUSTER_CMD		0x6E
#define RESERVE_LD		0x01
#define RELEASE_LD		0x02
#define RESET_RESERVATIONS	0x03
#define RESERVATION_STATUS	0x04
#define RESERVE_PD		0x05
#define RELEASE_PD		0x06


#define BATTERY_MODULE_MISSING		0x01
#define BATTERY_LOW_VOLTAGE		0x02
#define BATTERY_TEMP_HIGH		0x04
#define BATTERY_PACK_MISSING		0x08
#define BATTERY_CHARGE_MASK		0x30
#define BATTERY_CHARGE_DONE		0x00
#define BATTERY_CHARGE_INPROG		0x10
#define BATTERY_CHARGE_FAIL		0x20
#define BATTERY_CYCLES_EXCEEDED		0x40

#define PDRV_UNCNF	0
#define PDRV_ONLINE	3
#define PDRV_FAILED	4
#define PDRV_RBLD	5
#define PDRV_HOTSPARE	6


#define RDRV_OFFLINE	0
#define RDRV_DEGRADED	1
#define RDRV_OPTIMAL	2
#define RDRV_DELETED	3

#define NO_READ_AHEAD		0
#define READ_AHEAD		1
#define ADAP_READ_AHEAD		2
#define WRMODE_WRITE_THRU	0
#define WRMODE_WRITE_BACK	1
#define CACHED_IO		0
#define DIRECT_IO		1

#define MAX_LOGICAL_DRIVES_8LD		8
#define MAX_LOGICAL_DRIVES_40LD		40
#define FC_MAX_PHYSICAL_DEVICES		256
#define MAX_MBOX_CHANNELS		5
#define MAX_MBOX_TARGET			15
#define MBOX_MAX_PHYSICAL_DRIVES	MAX_MBOX_CHANNELS*MAX_MBOX_TARGET
#define MAX_ROW_SIZE_40LD		32
#define MAX_ROW_SIZE_8LD		8
#define SPAN_DEPTH_8_SPANS		8
#define SPAN_DEPTH_4_SPANS		4
#define MAX_REQ_SENSE_LEN		0x20



#define MBOX_MAX_FIRMWARE_STATUS	46
typedef struct {
	uint8_t		cmd;
	uint8_t		cmdid;
	uint16_t	numsectors;
	uint32_t	lba;
	uint32_t	xferaddr;
	uint8_t		logdrv;
	uint8_t		numsge;
	uint8_t		resvd;
	uint8_t		busy;
	uint8_t		numstatus;
	uint8_t		status;
	uint8_t		completed[MBOX_MAX_FIRMWARE_STATUS];
	uint8_t		poll;
	uint8_t		ack;
} __attribute__ ((packed)) mbox_t;


typedef struct {
	uint32_t	xferaddr_lo;
	uint32_t	xferaddr_hi;
	mbox_t		mbox32;
} __attribute__ ((packed)) mbox64_t;

typedef struct {
	u8	cmd;
	u8	cmdid;
	u8	opcode;
	u8	subopcode;
	u32	lba;
	u32	xferaddr;
	u8	logdrv;
	u8	rsvd[3];
	u8	numstatus;
	u8	status;
} __attribute__ ((packed)) int_mbox_t;

typedef struct {
	uint8_t		timeout		:3;
	uint8_t		ars		:1;
	uint8_t		reserved	:3;
	uint8_t		islogical	:1;
	uint8_t		logdrv;
	uint8_t		channel;
	uint8_t		target;
	uint8_t		queuetag;
	uint8_t		queueaction;
	uint8_t		cdb[10];
	uint8_t		cdblen;
	uint8_t		reqsenselen;
	uint8_t		reqsensearea[MAX_REQ_SENSE_LEN];
	uint8_t		numsge;
	uint8_t		scsistatus;
	uint32_t	dataxferaddr;
	uint32_t	dataxferlen;
} __attribute__ ((packed)) mraid_passthru_t;

typedef struct {

	uint32_t		dataxferaddr_lo;
	uint32_t		dataxferaddr_hi;
	mraid_passthru_t	pthru32;

} __attribute__ ((packed)) mega_passthru64_t;

typedef struct {
	uint8_t		timeout		:3;
	uint8_t		ars		:1;
	uint8_t		rsvd1		:1;
	uint8_t		cd_rom		:1;
	uint8_t		rsvd2		:1;
	uint8_t		islogical	:1;
	uint8_t		logdrv;
	uint8_t		channel;
	uint8_t		target;
	uint8_t		queuetag;
	uint8_t		queueaction;
	uint8_t		cdblen;
	uint8_t		rsvd3;
	uint8_t		cdb[16];
	uint8_t		numsge;
	uint8_t		status;
	uint8_t		reqsenselen;
	uint8_t		reqsensearea[MAX_REQ_SENSE_LEN];
	uint8_t		rsvd4;
	uint32_t	dataxferaddr;
	uint32_t	dataxferlen;
} __attribute__ ((packed)) mraid_epassthru_t;


typedef struct {
	uint32_t	data_size;
	uint32_t	config_signature;
	uint8_t		fw_version[16];
	uint8_t		bios_version[16];
	uint8_t		product_name[80];
	uint8_t		max_commands;
	uint8_t		nchannels;
	uint8_t		fc_loop_present;
	uint8_t		mem_type;
	uint32_t	signature;
	uint16_t	dram_size;
	uint16_t	subsysid;
	uint16_t	subsysvid;
	uint8_t		notify_counters;
	uint8_t		pad1k[889];
} __attribute__ ((packed)) mraid_pinfo_t;


typedef struct {
	uint32_t	global_counter;
	uint8_t		param_counter;
	uint8_t		param_id;
	uint16_t	param_val;
	uint8_t		write_config_counter;
	uint8_t		write_config_rsvd[3];
	uint8_t		ldrv_op_counter;
	uint8_t		ldrv_opid;
	uint8_t		ldrv_opcmd;
	uint8_t		ldrv_opstatus;
	uint8_t		ldrv_state_counter;
	uint8_t		ldrv_state_id;
	uint8_t		ldrv_state_new;
	uint8_t		ldrv_state_old;
	uint8_t		pdrv_state_counter;
	uint8_t		pdrv_state_id;
	uint8_t		pdrv_state_new;
	uint8_t		pdrv_state_old;
	uint8_t		pdrv_fmt_counter;
	uint8_t		pdrv_fmt_id;
	uint8_t		pdrv_fmt_val;
	uint8_t		pdrv_fmt_rsvd;
	uint8_t		targ_xfer_counter;
	uint8_t		targ_xfer_id;
	uint8_t		targ_xfer_val;
	uint8_t		targ_xfer_rsvd;
	uint8_t		fcloop_id_chg_counter;
	uint8_t		fcloopid_pdrvid;
	uint8_t		fcloop_id0;
	uint8_t		fcloop_id1;
	uint8_t		fcloop_state_counter;
	uint8_t		fcloop_state0;
	uint8_t		fcloop_state1;
	uint8_t		fcloop_state_rsvd;
} __attribute__ ((packed)) mraid_notify_t;


#define MAX_NOTIFY_SIZE		0x80
#define CUR_NOTIFY_SIZE		sizeof(mraid_notify_t)

typedef struct {
	uint32_t	data_size;

	mraid_notify_t	notify;

	uint8_t		notify_rsvd[MAX_NOTIFY_SIZE - CUR_NOTIFY_SIZE];

	uint8_t		rebuild_rate;
	uint8_t		cache_flush_int;
	uint8_t		sense_alert;
	uint8_t		drive_insert_count;

	uint8_t		battery_status;
	uint8_t		num_ldrv;
	uint8_t		recon_state[MAX_LOGICAL_DRIVES_40LD / 8];
	uint16_t	ldrv_op_status[MAX_LOGICAL_DRIVES_40LD / 8];

	uint32_t	ldrv_size[MAX_LOGICAL_DRIVES_40LD];
	uint8_t		ldrv_prop[MAX_LOGICAL_DRIVES_40LD];
	uint8_t		ldrv_state[MAX_LOGICAL_DRIVES_40LD];
	uint8_t		pdrv_state[FC_MAX_PHYSICAL_DEVICES];
	uint16_t	pdrv_format[FC_MAX_PHYSICAL_DEVICES / 16];

	uint8_t		targ_xfer[80];
	uint8_t		pad1k[263];
} __attribute__ ((packed)) mraid_inquiry3_t;


typedef struct {
	uint8_t		max_commands;
	uint8_t		rebuild_rate;
	uint8_t		max_targ_per_chan;
	uint8_t		nchannels;
	uint8_t		fw_version[4];
	uint16_t	age_of_flash;
	uint8_t		chip_set_value;
	uint8_t		dram_size;
	uint8_t		cache_flush_interval;
	uint8_t		bios_version[4];
	uint8_t		board_type;
	uint8_t		sense_alert;
	uint8_t		write_config_count;
	uint8_t		battery_status;
	uint8_t		dec_fault_bus_info;
} __attribute__ ((packed)) mraid_adapinfo_t;


typedef struct {
	uint8_t		nldrv;
	uint8_t		rsvd[3];
	uint32_t	size[MAX_LOGICAL_DRIVES_8LD];
	uint8_t		prop[MAX_LOGICAL_DRIVES_8LD];
	uint8_t		state[MAX_LOGICAL_DRIVES_8LD];
} __attribute__ ((packed)) mraid_ldrv_info_t;


typedef struct {
	uint8_t		pdrv_state[MBOX_MAX_PHYSICAL_DRIVES];
	uint8_t		rsvd;
} __attribute__ ((packed)) mraid_pdrv_info_t;


typedef struct {
	mraid_adapinfo_t	adapter_info;
	mraid_ldrv_info_t	logdrv_info;
	mraid_pdrv_info_t	pdrv_info;
} __attribute__ ((packed)) mraid_inquiry_t;


typedef struct {
	mraid_inquiry_t	raid_inq;
	uint16_t	phys_drv_format[MAX_MBOX_CHANNELS];
	uint8_t		stack_attn;
	uint8_t		modem_status;
	uint8_t		rsvd[2];
} __attribute__ ((packed)) mraid_extinq_t;


typedef struct {
	uint8_t		channel;
	uint8_t		target;
}__attribute__ ((packed)) adap_device_t;


typedef struct {
	uint32_t	start_blk;
	uint32_t	num_blks;
	adap_device_t	device[MAX_ROW_SIZE_40LD];
}__attribute__ ((packed)) adap_span_40ld_t;


typedef struct {
	uint32_t	start_blk;
	uint32_t	num_blks;
	adap_device_t	device[MAX_ROW_SIZE_8LD];
}__attribute__ ((packed)) adap_span_8ld_t;


typedef struct {
	uint8_t		span_depth;
	uint8_t		level;
	uint8_t		read_ahead;
	uint8_t		stripe_sz;
	uint8_t		status;
	uint8_t		write_mode;
	uint8_t		direct_io;
	uint8_t		row_size;
} __attribute__ ((packed)) logdrv_param_t;


typedef struct {
	logdrv_param_t		lparam;
	adap_span_40ld_t	span[SPAN_DEPTH_8_SPANS];
}__attribute__ ((packed)) logdrv_40ld_t;


typedef struct {
	logdrv_param_t	lparam;
	adap_span_8ld_t	span[SPAN_DEPTH_8_SPANS];
}__attribute__ ((packed)) logdrv_8ld_span8_t;


typedef struct {
	logdrv_param_t	lparam;
	adap_span_8ld_t	span[SPAN_DEPTH_4_SPANS];
}__attribute__ ((packed)) logdrv_8ld_span4_t;


typedef struct {
	uint8_t		type;
	uint8_t		cur_status;
	uint8_t		tag_depth;
	uint8_t		sync_neg;
	uint32_t	size;
}__attribute__ ((packed)) phys_drive_t;


typedef struct {
	uint8_t		numldrv;
	uint8_t		resvd[3];
	logdrv_40ld_t	ldrv[MAX_LOGICAL_DRIVES_40LD];
	phys_drive_t	pdrv[MBOX_MAX_PHYSICAL_DRIVES];
}__attribute__ ((packed)) disk_array_40ld_t;


typedef struct {
	uint8_t			numldrv;
	uint8_t			resvd[3];
	logdrv_8ld_span8_t	ldrv[MAX_LOGICAL_DRIVES_8LD];
	phys_drive_t		pdrv[MBOX_MAX_PHYSICAL_DRIVES];
}__attribute__ ((packed)) disk_array_8ld_span8_t;


typedef struct {
	uint8_t			numldrv;
	uint8_t			resvd[3];
	logdrv_8ld_span4_t	ldrv[MAX_LOGICAL_DRIVES_8LD];
	phys_drive_t		pdrv[MBOX_MAX_PHYSICAL_DRIVES];
}__attribute__ ((packed)) disk_array_8ld_span4_t;


struct private_bios_data {
	uint8_t		geometry	:4;
	uint8_t		unused		:4;
	uint8_t		boot_drv;
	uint8_t		rsvd[12];
	uint16_t	cksum;
} __attribute__ ((packed));


typedef struct {
	uint64_t	address;
	uint32_t	length;
} __attribute__ ((packed)) mbox_sgl64;

typedef struct {
	uint32_t	address;
	uint32_t	length;
} __attribute__ ((packed)) mbox_sgl32;

#endif		

