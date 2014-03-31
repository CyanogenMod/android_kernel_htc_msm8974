/*
 * Core definitions and data structures shareable across OS platforms.
 *
 * Copyright (c) 1994-2002 Justin T. Gibbs.
 * Copyright (c) 2000-2002 Adaptec Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/aic7xxx/aic7xxx/aic79xx.h#109 $
 *
 * $FreeBSD$
 */

#ifndef _AIC79XX_H_
#define _AIC79XX_H_

#include "aic79xx_reg.h"

struct ahd_platform_data;
struct scb_platform_data;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define ALL_CHANNELS '\0'
#define ALL_TARGETS_MASK 0xFFFF
#define INITIATOR_WILDCARD	(~0)
#define	SCB_LIST_NULL		0xFF00
#define	SCB_LIST_NULL_LE	(ahd_htole16(SCB_LIST_NULL))
#define QOUTFIFO_ENTRY_VALID 0x80
#define SCBID_IS_NULL(scbid) (((scbid) & 0xFF00 ) == SCB_LIST_NULL)

#define SCSIID_TARGET(ahd, scsiid)	\
	(((scsiid) & TID) >> TID_SHIFT)
#define SCSIID_OUR_ID(scsiid)		\
	((scsiid) & OID)
#define SCSIID_CHANNEL(ahd, scsiid) ('A')
#define	SCB_IS_SCSIBUS_B(ahd, scb) (0)
#define	SCB_GET_OUR_ID(scb) \
	SCSIID_OUR_ID((scb)->hscb->scsiid)
#define	SCB_GET_TARGET(ahd, scb) \
	SCSIID_TARGET((ahd), (scb)->hscb->scsiid)
#define	SCB_GET_CHANNEL(ahd, scb) \
	SCSIID_CHANNEL(ahd, (scb)->hscb->scsiid)
#define	SCB_GET_LUN(scb) \
	((scb)->hscb->lun)
#define SCB_GET_TARGET_OFFSET(ahd, scb)	\
	SCB_GET_TARGET(ahd, scb)
#define SCB_GET_TARGET_MASK(ahd, scb) \
	(0x01 << (SCB_GET_TARGET_OFFSET(ahd, scb)))
#ifdef AHD_DEBUG
#define SCB_IS_SILENT(scb)					\
	((ahd_debug & AHD_SHOW_MASKED_ERRORS) == 0		\
      && (((scb)->flags & SCB_SILENT) != 0))
#else
#define SCB_IS_SILENT(scb)					\
	(((scb)->flags & SCB_SILENT) != 0)
#endif
#define TCL_TARGET_OFFSET(tcl) \
	((((tcl) >> 4) & TID) >> 4)
#define TCL_LUN(tcl) \
	(tcl & (AHD_NUM_LUNS - 1))
#define BUILD_TCL(scsiid, lun) \
	((lun) | (((scsiid) & TID) << 4))
#define BUILD_TCL_RAW(target, channel, lun) \
	((lun) | ((target) << 8))

#define SCB_GET_TAG(scb) \
	ahd_le16toh(scb->hscb->tag)

#ifndef	AHD_TARGET_MODE
#undef	AHD_TMODE_ENABLE
#define	AHD_TMODE_ENABLE 0
#endif

#define AHD_BUILD_COL_IDX(target, lun)				\
	(((lun) << 4) | target)

#define AHD_GET_SCB_COL_IDX(ahd, scb)				\
	((SCB_GET_LUN(scb) << 4) | SCB_GET_TARGET(ahd, scb))

#define AHD_SET_SCB_COL_IDX(scb, col_idx)				\
do {									\
	(scb)->hscb->scsiid = ((col_idx) << TID_SHIFT) & TID;		\
	(scb)->hscb->lun = ((col_idx) >> 4) & (AHD_NUM_LUNS_NONPKT-1);	\
} while (0)

#define AHD_COPY_SCB_COL_IDX(dst, src)				\
do {								\
	dst->hscb->scsiid = src->hscb->scsiid;			\
	dst->hscb->lun = src->hscb->lun;			\
} while (0)

#define	AHD_NEVER_COL_IDX 0xFFFF

#define AHD_NUM_TARGETS 16

#define AHD_NUM_LUNS_NONPKT 64
#define AHD_NUM_LUNS 256

#define AHD_MAXTRANSFER_SIZE	 0x00ffffff	

#define AHD_SCB_MAX	512

#define AHD_MAX_QUEUE	AHD_SCB_MAX

#define	AHD_QIN_SIZE	AHD_MAX_QUEUE
#define	AHD_QOUT_SIZE	AHD_MAX_QUEUE

#define AHD_QIN_WRAP(x) ((x) & (AHD_QIN_SIZE-1))
#define AHD_SCB_MAX_ALLOC AHD_MAX_QUEUE

#define AHD_TMODE_CMDS	256

#define AHD_BUSRESET_DELAY	25

typedef enum {
	AHD_NONE	= 0x0000,
	AHD_CHIPID_MASK	= 0x00FF,
	AHD_AIC7901	= 0x0001,
	AHD_AIC7902	= 0x0002,
	AHD_AIC7901A	= 0x0003,
	AHD_PCI		= 0x0100,	
	AHD_PCIX	= 0x0200,	
	AHD_BUS_MASK	= 0x0F00
} ahd_chip;

typedef enum {
	AHD_FENONE		= 0x00000,
	AHD_WIDE  		= 0x00001,
	AHD_AIC79XXB_SLOWCRC    = 0x00002,
	AHD_MULTI_FUNC		= 0x00100,
	AHD_TARGETMODE		= 0x01000,
	AHD_MULTIROLE		= 0x02000,
	AHD_RTI			= 0x04000,
	AHD_NEW_IOCELL_OPTS	= 0x08000,
	AHD_NEW_DFCNTRL_OPTS	= 0x10000,
	AHD_FAST_CDB_DELIVERY	= 0x20000,
	AHD_REMOVABLE		= 0x00000,
	AHD_AIC7901_FE		= AHD_FENONE,
	AHD_AIC7901A_FE		= AHD_FENONE,
	AHD_AIC7902_FE		= AHD_MULTI_FUNC
} ahd_feature;

typedef enum {
	AHD_BUGNONE		= 0x0000,
	AHD_SENT_SCB_UPDATE_BUG	= 0x0001,
	
	AHD_ABORT_LQI_BUG	= 0x0002,
	
	AHD_PKT_BITBUCKET_BUG	= 0x0004,
	
	AHD_LONG_SETIMO_BUG	= 0x0008,
	
	AHD_NLQICRC_DELAYED_BUG	= 0x0010,
	
	AHD_SCSIRST_BUG		= 0x0020,
	
	AHD_PCIX_CHIPRST_BUG	= 0x0040,
	
	AHD_PCIX_MMAPIO_BUG	= 0x0080,
	
	AHD_PCIX_SCBRAM_RD_BUG  = 0x0100,
	
	AHD_PCIX_BUG_MASK	= AHD_PCIX_CHIPRST_BUG
				| AHD_PCIX_MMAPIO_BUG
				| AHD_PCIX_SCBRAM_RD_BUG,
	AHD_LQO_ATNO_BUG	= 0x0200,
	
	AHD_AUTOFLUSH_BUG	= 0x0400,
	
	AHD_CLRLQO_AUTOCLR_BUG	= 0x0800,
	
	AHD_PKTIZED_STATUS_BUG  = 0x1000,
	
	AHD_PKT_LUN_BUG		= 0x2000,
	AHD_NONPACKFIFO_BUG	= 0x4000,
	AHD_MDFF_WSCBPTR_BUG	= 0x8000,
	
	AHD_REG_SLOW_SETTLE_BUG	= 0x10000,
	/*
	 * Changing the MODE_PTR coincident with an interrupt that
	 * switches to a different mode will cause the interrupt to
	 * be in the mode written outside of interrupt context.
	 */
	AHD_SET_MODE_BUG	= 0x20000,
	
	AHD_BUSFREEREV_BUG	= 0x40000,
	AHD_PACED_NEGTABLE_BUG	= 0x80000,
	
	AHD_LQOOVERRUN_BUG	= 0x100000,
	AHD_INTCOLLISION_BUG	= 0x200000,
	AHD_EARLY_REQ_BUG	= 0x400000,
	AHD_FAINT_LED_BUG	= 0x800000
} ahd_bug;

typedef enum {
	AHD_FNONE	      = 0x00000,
	AHD_BOOT_CHANNEL      = 0x00001,
	AHD_USEDEFAULTS	      = 0x00004,
	AHD_SEQUENCER_DEBUG   = 0x00008,
	AHD_RESET_BUS_A	      = 0x00010,
	AHD_EXTENDED_TRANS_A  = 0x00020,
	AHD_TERM_ENB_A	      = 0x00040,
	AHD_SPCHK_ENB_A	      = 0x00080,
	AHD_STPWLEVEL_A	      = 0x00100,
	AHD_INITIATORROLE     = 0x00200,
	AHD_TARGETROLE	      = 0x00400,
	AHD_RESOURCE_SHORTAGE = 0x00800,
	AHD_TQINFIFO_BLOCKED  = 0x01000,
	AHD_INT50_SPEEDFLEX   = 0x02000,
	AHD_BIOS_ENABLED      = 0x04000,
	AHD_ALL_INTERRUPTS    = 0x08000,
	AHD_39BIT_ADDRESSING  = 0x10000,
	AHD_64BIT_ADDRESSING  = 0x20000,
	AHD_CURRENT_SENSING   = 0x40000,
	AHD_SCB_CONFIG_USED   = 0x80000,
	AHD_HP_BOARD	      = 0x100000,
	AHD_BUS_RESET_ACTIVE  = 0x200000,
	AHD_UPDATE_PEND_CMDS  = 0x400000,
	AHD_RUNNING_QOUTFIFO  = 0x800000,
	AHD_HAD_FIRST_SEL     = 0x1000000
} ahd_flag;



struct initiator_status {
	uint32_t residual_datacnt;	
	uint32_t residual_sgptr;	
	uint8_t	 scsi_status;		
};

struct target_status {
	uint32_t residual_datacnt;	
	uint32_t residual_sgptr;	
	uint8_t  scsi_status;		
	uint8_t  target_phases;		
	uint8_t  data_phase;		
	uint8_t  initiator_tag;		
};

typedef uint32_t sense_addr_t;
#define MAX_CDB_LEN 16
#define MAX_CDB_LEN_WITH_SENSE_ADDR (MAX_CDB_LEN - sizeof(sense_addr_t))
union initiator_data {
	struct {
		uint64_t cdbptr;
		uint8_t  cdblen;
	} cdb_from_host;
	uint8_t	 cdb[MAX_CDB_LEN];
	struct {
		uint8_t	 cdb[MAX_CDB_LEN_WITH_SENSE_ADDR];
		sense_addr_t sense_addr;
	} cdb_plus_saddr;
};

struct target_data {
	uint32_t spare[2];	
	uint8_t  scsi_status;		
	uint8_t  target_phases;		
	uint8_t  data_phase;		
	uint8_t  initiator_tag;		
};

struct hardware_scb {
	union {
		union	initiator_data idata;
		struct	target_data tdata;
		struct	initiator_status istatus;
		struct	target_status tstatus;
	} shared_data;
 
#define SG_PTR_MASK	0xFFFFFFF8
	uint16_t tag;		
	uint8_t  control;	
	uint8_t	 scsiid;	
	uint8_t  lun;
	uint8_t  task_attribute;
	uint8_t  cdb_len;
	uint8_t  task_management;
	uint64_t dataptr;
	uint32_t datacnt;	
	uint32_t sgptr;
	uint32_t hscb_busaddr;
	uint32_t next_hscb_busaddr;
  uint8_t	 pkt_long_lun[8];
  uint8_t	 spare[8];
};


struct ahd_dma_seg {
	uint32_t	addr;
	uint32_t	len;
#define	AHD_DMA_LAST_SEG	0x80000000
#define	AHD_SG_HIGH_ADDR_MASK	0x7F000000
#define	AHD_SG_LEN_MASK		0x00FFFFFF
};

struct ahd_dma64_seg {
	uint64_t	addr;
	uint32_t	len;
	uint32_t	pad;
};

struct map_node {
	bus_dmamap_t		 dmamap;
	dma_addr_t		 physaddr;
	uint8_t			*vaddr;
	SLIST_ENTRY(map_node)	 links;
};

typedef enum {
	SCB_FLAG_NONE		= 0x00000,
	SCB_TRANSMISSION_ERROR	= 0x00001,
	SCB_OTHERTCL_TIMEOUT	= 0x00002,
	SCB_DEVICE_RESET	= 0x00004,
	SCB_SENSE		= 0x00008,
	SCB_CDB32_PTR		= 0x00010,
	SCB_RECOVERY_SCB	= 0x00020,
	SCB_AUTO_NEGOTIATE	= 0x00040,
	SCB_NEGOTIATE		= 0x00080,
	SCB_ABORT		= 0x00100,
	SCB_ACTIVE		= 0x00200,
	SCB_TARGET_IMMEDIATE	= 0x00400,
	SCB_PACKETIZED		= 0x00800,
	SCB_EXPECT_PPR_BUSFREE	= 0x01000,
	SCB_PKT_SENSE		= 0x02000,
	SCB_EXTERNAL_RESET	= 0x04000,
	SCB_ON_COL_LIST		= 0x08000,
	SCB_SILENT		= 0x10000 
} scb_flag;

struct scb {
	struct	hardware_scb	 *hscb;
	union {
		SLIST_ENTRY(scb)  sle;
		LIST_ENTRY(scb)	  le;
		TAILQ_ENTRY(scb)  tqe;
	} links;
	union {
		SLIST_ENTRY(scb)  sle;
		LIST_ENTRY(scb)	  le;
		TAILQ_ENTRY(scb)  tqe;
	} links2;
#define pending_links links2.le
#define collision_links links2.le
	struct scb		 *col_scb;
	ahd_io_ctx_t		  io_ctx;
	struct ahd_softc	 *ahd_softc;
	scb_flag		  flags;
#ifndef __linux__
	bus_dmamap_t		  dmamap;
#endif
	struct scb_platform_data *platform_data;
	struct map_node	 	 *hscb_map;
	struct map_node	 	 *sg_map;
	struct map_node	 	 *sense_map;
	void			 *sg_list;
	uint8_t			 *sense_data;
	dma_addr_t		  sg_list_busaddr;
	dma_addr_t		  sense_busaddr;
	u_int			  sg_count;
#define	AHD_MAX_LQ_CRC_ERRORS 5
	u_int			  crc_retry_count;
};

TAILQ_HEAD(scb_tailq, scb);
LIST_HEAD(scb_list, scb);

struct scb_data {
	struct scb_tailq free_scbs;

	struct scb_list free_scb_lists[AHD_NUM_TARGETS * AHD_NUM_LUNS_NONPKT];

	struct scb_list any_dev_free_scb_list;

	struct	scb *scbindex[AHD_SCB_MAX];

	bus_dma_tag_t	 hscb_dmat;	
	bus_dma_tag_t	 sg_dmat;	
	bus_dma_tag_t	 sense_dmat;	
	SLIST_HEAD(, map_node) hscb_maps;
	SLIST_HEAD(, map_node) sg_maps;
	SLIST_HEAD(, map_node) sense_maps;
	int		 scbs_left;	
	int		 sgs_left;	
	int		 sense_left;	
	uint16_t	 numscbs;
	uint16_t	 maxhscbs;	
	uint8_t		 init_level;	
};


struct target_cmd {
	uint8_t scsiid;		
	uint8_t identify;	
	uint8_t bytes[22];	
	uint8_t cmd_valid;	
	uint8_t pad[7];
};

#define AHD_TMODE_EVENT_BUFFER_SIZE 8
struct ahd_tmode_event {
	uint8_t initiator_id;
	uint8_t event_type;	
#define	EVENT_TYPE_BUS_RESET 0xFF
	uint8_t event_arg;
};

#ifdef AHD_TARGET_MODE 
struct ahd_tmode_lstate {
	struct cam_path *path;
	struct ccb_hdr_slist accept_tios;
	struct ccb_hdr_slist immed_notifies;
	struct ahd_tmode_event event_buffer[AHD_TMODE_EVENT_BUFFER_SIZE];
	uint8_t event_r_idx;
	uint8_t event_w_idx;
};
#else
struct ahd_tmode_lstate;
#endif

#define AHD_TRANS_CUR		0x01	
#define AHD_TRANS_ACTIVE	0x03	
#define AHD_TRANS_GOAL		0x04	
#define AHD_TRANS_USER		0x08	
#define AHD_PERIOD_10MHz	0x19

#define AHD_WIDTH_UNKNOWN	0xFF
#define AHD_PERIOD_UNKNOWN	0xFF
#define AHD_OFFSET_UNKNOWN	0xFF
#define AHD_PPR_OPTS_UNKNOWN	0xFF

struct ahd_transinfo {
	uint8_t protocol_version;	
	uint8_t transport_version;	
	uint8_t width;			
	uint8_t period;			
	uint8_t offset;			
	uint8_t ppr_options;		
};

struct ahd_initiator_tinfo {
	struct ahd_transinfo curr;
	struct ahd_transinfo goal;
	struct ahd_transinfo user;
};

struct ahd_tmode_tstate {
	struct ahd_tmode_lstate*	enabled_luns[AHD_NUM_LUNS];
	struct ahd_initiator_tinfo	transinfo[AHD_NUM_TARGETS];

	uint16_t	 auto_negotiate;
	uint16_t	 discenable;	
	uint16_t	 tagenable;	
};

#define AHD_SYNCRATE_160	0x8
#define AHD_SYNCRATE_PACED	0x8
#define AHD_SYNCRATE_DT		0x9
#define AHD_SYNCRATE_ULTRA2	0xa
#define AHD_SYNCRATE_ULTRA	0xc
#define AHD_SYNCRATE_FAST	0x19
#define AHD_SYNCRATE_MIN_DT	AHD_SYNCRATE_FAST
#define AHD_SYNCRATE_SYNC	0x32
#define AHD_SYNCRATE_MIN	0x60
#define	AHD_SYNCRATE_ASYNC	0xFF
#define AHD_SYNCRATE_MAX	AHD_SYNCRATE_160

#define	AHD_ASYNC_XFER_PERIOD	0x44

#define AHD_SYNCRATE_REVA_120	0x8
#define AHD_SYNCRATE_REVA_160	0x7

struct ahd_phase_table_entry {
        uint8_t phase;
        uint8_t mesg_out; 
	const char *phasemsg;
};


struct seeprom_config {
	uint16_t device_flags[16];	
#define		CFXFER		0x003F	
#define			CFXFER_ASYNC	0x3F
#define		CFQAS		0x0040	
#define		CFPACKETIZED	0x0080	
#define		CFSTART		0x0100	
#define		CFINCBIOS	0x0200	
#define		CFDISC		0x0400	
#define		CFMULTILUNDEV	0x0800	
#define		CFWIDEB		0x1000	
#define		CFHOSTMANAGED	0x8000	

	uint16_t bios_control;		
#define		CFSUPREM	0x0001	
#define		CFSUPREMB	0x0002	
#define		CFBIOSSTATE	0x000C	
#define		    CFBS_DISABLED	0x00
#define		    CFBS_ENABLED	0x04
#define		    CFBS_DISABLED_SCAN	0x08
#define		CFENABLEDV	0x0010	
#define		CFCTRL_A	0x0020		
#define		CFSPARITY	0x0040	
#define		CFEXTEND	0x0080	
#define		CFBOOTCD	0x0100  
#define		CFMSG_LEVEL	0x0600	
#define			CFMSG_VERBOSE	0x0000
#define			CFMSG_SILENT	0x0200
#define			CFMSG_DIAG	0x0400
#define		CFRESETB	0x0800	

	uint16_t adapter_control;		
#define		CFAUTOTERM	0x0001	
#define		CFSTERM		0x0002	
#define		CFWSTERM	0x0004	
#define		CFSEAUTOTERM	0x0008	
#define		CFSELOWTERM	0x0010	
#define		CFSEHIGHTERM	0x0020	
#define		CFSTPWLEVEL	0x0040	
#define		CFBIOSAUTOTERM	0x0080	
#define		CFTERM_MENU	0x0100		
#define		CFCLUSTERENB	0x8000	

	uint16_t brtime_id;		
#define		CFSCSIID	0x000f	
#define		CFBRTIME	0xff00	

	uint16_t max_targets;			
#define		CFMAXTARG	0x00ff	
#define		CFBOOTLUN	0x0f00	
#define		CFBOOTID	0xf000	
	uint16_t res_1[10];		
	uint16_t signature;		
#define		CFSIGNATURE	0x400
	uint16_t checksum;		
};

struct vpd_config {
	uint8_t  bios_flags;
#define		VPDMASTERBIOS	0x0001
#define		VPDBOOTHOST	0x0002
	uint8_t  reserved_1[21];
	uint8_t  resource_type;
	uint8_t  resource_len[2];
	uint8_t  resource_data[8];
	uint8_t  vpd_tag;
	uint16_t vpd_len;
	uint8_t  vpd_keyword[2];
	uint8_t  length;
	uint8_t  revision;
	uint8_t  device_flags;
	uint8_t  termnation_menus[2];
	uint8_t  fifo_threshold;
	uint8_t  end_tag;
	uint8_t  vpd_checksum;
	uint16_t default_target_flags;
	uint16_t default_bios_flags;
	uint16_t default_ctrl_flags;
	uint8_t  default_irq;
	uint8_t  pci_lattime;
	uint8_t  max_target;
	uint8_t  boot_lun;
	uint16_t signature;
	uint8_t  reserved_2;
	uint8_t  checksum;
	uint8_t	 reserved_3[4];
};

#define FLXADDR_TERMCTL			0x0
#define		FLX_TERMCTL_ENSECHIGH	0x8
#define		FLX_TERMCTL_ENSECLOW	0x4
#define		FLX_TERMCTL_ENPRIHIGH	0x2
#define		FLX_TERMCTL_ENPRILOW	0x1
#define FLXADDR_ROMSTAT_CURSENSECTL	0x1
#define		FLX_ROMSTAT_SEECFG	0xF0
#define		FLX_ROMSTAT_EECFG	0x0F
#define		FLX_ROMSTAT_SEE_93C66	0x00
#define		FLX_ROMSTAT_SEE_NONE	0xF0
#define		FLX_ROMSTAT_EE_512x8	0x0
#define		FLX_ROMSTAT_EE_1MBx8	0x1
#define		FLX_ROMSTAT_EE_2MBx8	0x2
#define		FLX_ROMSTAT_EE_4MBx8	0x3
#define		FLX_ROMSTAT_EE_16MBx8	0x4
#define 		CURSENSE_ENB	0x1
#define	FLXADDR_FLEXSTAT		0x2
#define		FLX_FSTAT_BUSY		0x1
#define FLXADDR_CURRENT_STAT		0x4
#define		FLX_CSTAT_SEC_HIGH	0xC0
#define		FLX_CSTAT_SEC_LOW	0x30
#define		FLX_CSTAT_PRI_HIGH	0x0C
#define		FLX_CSTAT_PRI_LOW	0x03
#define		FLX_CSTAT_MASK		0x03
#define		FLX_CSTAT_SHIFT		2
#define		FLX_CSTAT_OKAY		0x0
#define		FLX_CSTAT_OVER		0x1
#define		FLX_CSTAT_UNDER		0x2
#define		FLX_CSTAT_INVALID	0x3

int		ahd_read_seeprom(struct ahd_softc *ahd, uint16_t *buf,
				 u_int start_addr, u_int count, int bstream);

int		ahd_write_seeprom(struct ahd_softc *ahd, uint16_t *buf,
				  u_int start_addr, u_int count);
int		ahd_verify_cksum(struct seeprom_config *sc);
int		ahd_acquire_seeprom(struct ahd_softc *ahd);
void		ahd_release_seeprom(struct ahd_softc *ahd);

typedef enum {
	MSG_FLAG_NONE			= 0x00,
	MSG_FLAG_EXPECT_PPR_BUSFREE	= 0x01,
	MSG_FLAG_IU_REQ_CHANGED		= 0x02,
	MSG_FLAG_EXPECT_IDE_BUSFREE	= 0x04,
	MSG_FLAG_EXPECT_QASREJ_BUSFREE	= 0x08,
	MSG_FLAG_PACKETIZED		= 0x10
} ahd_msg_flags;

typedef enum {
	MSG_TYPE_NONE			= 0x00,
	MSG_TYPE_INITIATOR_MSGOUT	= 0x01,
	MSG_TYPE_INITIATOR_MSGIN	= 0x02,
	MSG_TYPE_TARGET_MSGOUT		= 0x03,
	MSG_TYPE_TARGET_MSGIN		= 0x04
} ahd_msg_type;

typedef enum {
	MSGLOOP_IN_PROG,
	MSGLOOP_MSGCOMPLETE,
	MSGLOOP_TERMINATED
} msg_loop_stat;

struct ahd_suspend_channel_state {
	uint8_t	scsiseq;
	uint8_t	sxfrctl0;
	uint8_t	sxfrctl1;
	uint8_t	simode0;
	uint8_t	simode1;
	uint8_t	seltimer;
	uint8_t	seqctl;
};

struct ahd_suspend_pci_state {
	uint32_t  devconfig;
	uint8_t   command;
	uint8_t   csize_lattime;
};

struct ahd_suspend_state {
	struct	ahd_suspend_channel_state channel[2];
	struct  ahd_suspend_pci_state pci_state;
	uint8_t	optionmode;
	uint8_t	dscommand0;
	uint8_t	dspcistatus;
	
	uint8_t	crccontrol1;
	uint8_t	scbbaddr;
	
	uint8_t	dff_thrsh;
	uint8_t	*scratch_ram;
	uint8_t	*btt;
};

typedef void (*ahd_bus_intr_t)(struct ahd_softc *);

typedef enum {
	AHD_MODE_DFF0,
	AHD_MODE_DFF1,
	AHD_MODE_CCHAN,
	AHD_MODE_SCSI,
	AHD_MODE_CFG,
	AHD_MODE_UNKNOWN
} ahd_mode;

#define AHD_MK_MSK(x) (0x01 << (x))
#define AHD_MODE_DFF0_MSK	AHD_MK_MSK(AHD_MODE_DFF0)
#define AHD_MODE_DFF1_MSK	AHD_MK_MSK(AHD_MODE_DFF1)
#define AHD_MODE_CCHAN_MSK	AHD_MK_MSK(AHD_MODE_CCHAN)
#define AHD_MODE_SCSI_MSK	AHD_MK_MSK(AHD_MODE_SCSI)
#define AHD_MODE_CFG_MSK	AHD_MK_MSK(AHD_MODE_CFG)
#define AHD_MODE_UNKNOWN_MSK	AHD_MK_MSK(AHD_MODE_UNKNOWN)
#define AHD_MODE_ANY_MSK (~0)

typedef uint8_t ahd_mode_state;

typedef void ahd_callback_t (void *);

struct ahd_completion
{
	uint16_t	tag;
	uint8_t		sg_status;
	uint8_t		valid_tag;
};

struct ahd_softc {
	bus_space_tag_t           tags[2];
	bus_space_handle_t        bshs[2];
#ifndef __linux__
	bus_dma_tag_t		  buffer_dmat;   
#endif
	struct scb_data		  scb_data;

	struct hardware_scb	 *next_queued_hscb;
	struct map_node		 *next_queued_hscb_map;

	LIST_HEAD(, scb)	  pending_scbs;

	ahd_mode		  dst_mode;
	ahd_mode		  src_mode;

	ahd_mode		  saved_dst_mode;
	ahd_mode		  saved_src_mode;

	struct ahd_platform_data *platform_data;

	ahd_dev_softc_t		  dev_softc;

	ahd_bus_intr_t		  bus_intr;

	struct ahd_tmode_tstate  *enabled_targets[AHD_NUM_TARGETS];

	struct ahd_tmode_lstate  *black_hole;

	struct ahd_tmode_lstate  *pending_device;

	ahd_timer_t		  reset_timer;
	ahd_timer_t		  stat_timer;

#define	AHD_STAT_UPDATE_US	250000 
#define	AHD_STAT_BUCKETS	4
	u_int			  cmdcmplt_bucket;
	uint32_t		  cmdcmplt_counts[AHD_STAT_BUCKETS];
	uint32_t		  cmdcmplt_total;

	ahd_chip		  chip;
	ahd_feature		  features;
	ahd_bug			  bugs;
	ahd_flag		  flags;
	struct seeprom_config	 *seep_config;

	
	struct ahd_completion	  *qoutfifo;
	uint16_t		  qoutfifonext;
	uint16_t		  qoutfifonext_valid_tag;
	uint16_t		  qinfifonext;
	uint16_t		  qinfifo[AHD_SCB_MAX];

	uint16_t		  qfreeze_cnt;

	
	uint8_t			  unpause;
	uint8_t			  pause;

	
	struct cs		 *critical_sections;
	u_int			  num_critical_sections;

	
	uint8_t			 *overrun_buf;

	
	TAILQ_ENTRY(ahd_softc)	  links;

	
	char			  channel;

	
	uint8_t			  our_id;

	struct target_cmd	 *targetcmds;
	uint8_t			  tqinfifonext;

	uint8_t			  hs_mailbox;

	uint8_t			  send_msg_perror;
	ahd_msg_flags		  msg_flags;
	ahd_msg_type		  msg_type;
	uint8_t			  msgout_buf[12];
	uint8_t			  msgin_buf[12];
	u_int			  msgout_len;	
	u_int			  msgout_index;	
	u_int			  msgin_index;	

	bus_dma_tag_t		  parent_dmat;
	bus_dma_tag_t		  shared_data_dmat;
	struct map_node		  shared_data_map;

	
	struct ahd_suspend_state  suspend_state;

	
	u_int			  enabled_luns;

	
	u_int			  init_level;

	
	u_int			  pci_cachesize;

	
	uint8_t			  iocell_opts[AHD_NUM_PER_DEV_ANNEXCOLS];

	u_int			  stack_size;
	uint16_t		 *saved_stack;

	
	const char		 *description;
	const char		 *bus_description;
	char			 *name;
	int			  unit;

	
	int			  seltime;

#define	AHD_INT_COALESCING_TIMER_DEFAULT		250 
#define	AHD_INT_COALESCING_MAXCMDS_DEFAULT		10
#define	AHD_INT_COALESCING_MAXCMDS_MAX			127
#define	AHD_INT_COALESCING_MINCMDS_DEFAULT		5
#define	AHD_INT_COALESCING_MINCMDS_MAX			127
#define	AHD_INT_COALESCING_THRESHOLD_DEFAULT		2000
#define	AHD_INT_COALESCING_STOP_THRESHOLD_DEFAULT	1000
	u_int			  int_coalescing_timer;
	u_int			  int_coalescing_maxcmds;
	u_int			  int_coalescing_mincmds;
	u_int			  int_coalescing_threshold;
	u_int			  int_coalescing_stop_threshold;

	uint16_t	 	  user_discenable;
	uint16_t		  user_tagenable;
};

#define	AHD_PRECOMP_SLEW_INDEX						\
    (AHD_ANNEXCOL_PRECOMP_SLEW - AHD_ANNEXCOL_PER_DEV0)

#define	AHD_AMPLITUDE_INDEX						\
    (AHD_ANNEXCOL_AMPLITUDE - AHD_ANNEXCOL_PER_DEV0)

#define AHD_SET_SLEWRATE(ahd, new_slew)					\
do {									\
    (ahd)->iocell_opts[AHD_PRECOMP_SLEW_INDEX] &= ~AHD_SLEWRATE_MASK;	\
    (ahd)->iocell_opts[AHD_PRECOMP_SLEW_INDEX] |=			\
	(((new_slew) << AHD_SLEWRATE_SHIFT) & AHD_SLEWRATE_MASK);	\
} while (0)

#define AHD_SET_PRECOMP(ahd, new_pcomp)					\
do {									\
    (ahd)->iocell_opts[AHD_PRECOMP_SLEW_INDEX] &= ~AHD_PRECOMP_MASK;	\
    (ahd)->iocell_opts[AHD_PRECOMP_SLEW_INDEX] |=			\
	(((new_pcomp) << AHD_PRECOMP_SHIFT) & AHD_PRECOMP_MASK);	\
} while (0)

#define AHD_SET_AMPLITUDE(ahd, new_amp)					\
do {									\
    (ahd)->iocell_opts[AHD_AMPLITUDE_INDEX] &= ~AHD_AMPLITUDE_MASK;	\
    (ahd)->iocell_opts[AHD_AMPLITUDE_INDEX] |=				\
	(((new_amp) << AHD_AMPLITUDE_SHIFT) & AHD_AMPLITUDE_MASK);	\
} while (0)

typedef enum {
	ROLE_UNKNOWN,
	ROLE_INITIATOR,
	ROLE_TARGET
} role_t;

struct ahd_devinfo {
	int	 our_scsiid;
	int	 target_offset;
	uint16_t target_mask;
	u_int	 target;
	u_int	 lun;
	char	 channel;
	role_t	 role;		
};

#define AHD_PCI_IOADDR0	PCIR_BAR(0)	
#define AHD_PCI_MEMADDR	PCIR_BAR(1)	
#define AHD_PCI_IOADDR1	PCIR_BAR(3)	

typedef int (ahd_device_setup_t)(struct ahd_softc *);

struct ahd_pci_identity {
	uint64_t		 full_id;
	uint64_t		 id_mask;
	const char		*name;
	ahd_device_setup_t	*setup;
};

struct aic7770_identity {
	uint32_t		 full_id;
	uint32_t		 id_mask;
	const char		*name;
	ahd_device_setup_t	*setup;
};
extern struct aic7770_identity aic7770_ident_table [];
extern const int ahd_num_aic7770_devs;

#define AHD_EISA_SLOT_OFFSET	0xc00
#define AHD_EISA_IOSIZE		0x100


const struct	ahd_pci_identity *ahd_find_pci_device(ahd_dev_softc_t);
int			  ahd_pci_config(struct ahd_softc *,
					 const struct ahd_pci_identity *);
int	ahd_pci_test_register_access(struct ahd_softc *);
#ifdef CONFIG_PM
void	ahd_pci_suspend(struct ahd_softc *);
void	ahd_pci_resume(struct ahd_softc *);
#endif

void		ahd_qinfifo_requeue_tail(struct ahd_softc *ahd,
					 struct scb *scb);

struct ahd_softc	*ahd_alloc(void *platform_arg, char *name);
int			 ahd_softc_init(struct ahd_softc *);
void			 ahd_controller_info(struct ahd_softc *ahd, char *buf);
int			 ahd_init(struct ahd_softc *ahd);
#ifdef CONFIG_PM
int			 ahd_suspend(struct ahd_softc *ahd);
void			 ahd_resume(struct ahd_softc *ahd);
#endif
int			 ahd_default_config(struct ahd_softc *ahd);
int			 ahd_parse_vpddata(struct ahd_softc *ahd,
					   struct vpd_config *vpd);
int			 ahd_parse_cfgdata(struct ahd_softc *ahd,
					   struct seeprom_config *sc);
void			 ahd_intr_enable(struct ahd_softc *ahd, int enable);
void			 ahd_pause_and_flushwork(struct ahd_softc *ahd);
void			 ahd_set_unit(struct ahd_softc *, int);
void			 ahd_set_name(struct ahd_softc *, char *);
struct scb		*ahd_get_scb(struct ahd_softc *ahd, u_int col_idx);
void			 ahd_free_scb(struct ahd_softc *ahd, struct scb *scb);
void			 ahd_free(struct ahd_softc *ahd);
int			 ahd_reset(struct ahd_softc *ahd, int reinit);
int			 ahd_write_flexport(struct ahd_softc *ahd,
					    u_int addr, u_int value);
int			 ahd_read_flexport(struct ahd_softc *ahd, u_int addr,
					   uint8_t *value);

typedef enum {
	SEARCH_COMPLETE,
	SEARCH_COUNT,
	SEARCH_REMOVE,
	SEARCH_PRINT
} ahd_search_action;
int			ahd_search_qinfifo(struct ahd_softc *ahd, int target,
					   char channel, int lun, u_int tag,
					   role_t role, uint32_t status,
					   ahd_search_action action);
int			ahd_search_disc_list(struct ahd_softc *ahd, int target,
					     char channel, int lun, u_int tag,
					     int stop_on_first, int remove,
					     int save_state);
int			ahd_reset_channel(struct ahd_softc *ahd, char channel,
					  int initiate_reset);
void			ahd_compile_devinfo(struct ahd_devinfo *devinfo,
					    u_int our_id, u_int target,
					    u_int lun, char channel,
					    role_t role);
void			ahd_find_syncrate(struct ahd_softc *ahd, u_int *period,
					  u_int *ppr_options, u_int maxsync);
typedef enum {
	AHD_NEG_TO_GOAL,	
	AHD_NEG_IF_NON_ASYNC,	
	AHD_NEG_ALWAYS		
} ahd_neg_type;
int			ahd_update_neg_request(struct ahd_softc*,
					       struct ahd_devinfo*,
					       struct ahd_tmode_tstate*,
					       struct ahd_initiator_tinfo*,
					       ahd_neg_type);
void			ahd_set_width(struct ahd_softc *ahd,
				      struct ahd_devinfo *devinfo,
				      u_int width, u_int type, int paused);
void			ahd_set_syncrate(struct ahd_softc *ahd,
					 struct ahd_devinfo *devinfo,
					 u_int period, u_int offset,
					 u_int ppr_options,
					 u_int type, int paused);
typedef enum {
	AHD_QUEUE_NONE,
	AHD_QUEUE_BASIC,
	AHD_QUEUE_TAGGED
} ahd_queue_alg;

#ifdef AHD_TARGET_MODE
void		ahd_send_lstate_events(struct ahd_softc *,
				       struct ahd_tmode_lstate *);
void		ahd_handle_en_lun(struct ahd_softc *ahd,
				  struct cam_sim *sim, union ccb *ccb);
cam_status	ahd_find_tmode_devs(struct ahd_softc *ahd,
				    struct cam_sim *sim, union ccb *ccb,
				    struct ahd_tmode_tstate **tstate,
				    struct ahd_tmode_lstate **lstate,
				    int notfound_failure);
#ifndef AHD_TMODE_ENABLE
#define AHD_TMODE_ENABLE 0
#endif
#endif
#ifdef AHD_DEBUG
extern uint32_t ahd_debug;
#define AHD_SHOW_MISC		0x00001
#define AHD_SHOW_SENSE		0x00002
#define AHD_SHOW_RECOVERY	0x00004
#define AHD_DUMP_SEEPROM	0x00008
#define AHD_SHOW_TERMCTL	0x00010
#define AHD_SHOW_MEMORY		0x00020
#define AHD_SHOW_MESSAGES	0x00040
#define AHD_SHOW_MODEPTR	0x00080
#define AHD_SHOW_SELTO		0x00100
#define AHD_SHOW_FIFOS		0x00200
#define AHD_SHOW_QFULL		0x00400
#define	AHD_SHOW_DV		0x00800
#define AHD_SHOW_MASKED_ERRORS	0x01000
#define AHD_SHOW_QUEUE		0x02000
#define AHD_SHOW_TQIN		0x04000
#define AHD_SHOW_SG		0x08000
#define AHD_SHOW_INT_COALESCING	0x10000
#define AHD_DEBUG_SEQUENCER	0x20000
#endif
void			ahd_print_devinfo(struct ahd_softc *ahd,
					  struct ahd_devinfo *devinfo);
void			ahd_dump_card_state(struct ahd_softc *ahd);
int			ahd_print_register(const ahd_reg_parse_entry_t *table,
					   u_int num_entries,
					   const char *name,
					   u_int address,
					   u_int value,
					   u_int *cur_column,
					   u_int wrap_point);
#endif 
