/*
 * Core definitions and data structures shareable across OS platforms.
 *
 * Copyright (c) 1994-2001 Justin T. Gibbs.
 * Copyright (c) 2000-2001 Adaptec Inc.
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
 * $Id: //depot/aic7xxx/aic7xxx/aic7xxx.h#85 $
 *
 * $FreeBSD$
 */

#ifndef _AIC7XXX_H_
#define _AIC7XXX_H_

#include "aic7xxx_reg.h"

struct ahc_platform_data;
struct scb_platform_data;
struct seeprom_descriptor;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define ALL_CHANNELS '\0'
#define ALL_TARGETS_MASK 0xFFFF
#define INITIATOR_WILDCARD	(~0)

#define SCSIID_TARGET(ahc, scsiid) \
	(((scsiid) & ((((ahc)->features & AHC_TWIN) != 0) ? TWIN_TID : TID)) \
	>> TID_SHIFT)
#define SCSIID_OUR_ID(scsiid) \
	((scsiid) & OID)
#define SCSIID_CHANNEL(ahc, scsiid) \
	((((ahc)->features & AHC_TWIN) != 0) \
        ? ((((scsiid) & TWIN_CHNLB) != 0) ? 'B' : 'A') \
       : 'A')
#define	SCB_IS_SCSIBUS_B(ahc, scb) \
	(SCSIID_CHANNEL(ahc, (scb)->hscb->scsiid) == 'B')
#define	SCB_GET_OUR_ID(scb) \
	SCSIID_OUR_ID((scb)->hscb->scsiid)
#define	SCB_GET_TARGET(ahc, scb) \
	SCSIID_TARGET((ahc), (scb)->hscb->scsiid)
#define	SCB_GET_CHANNEL(ahc, scb) \
	SCSIID_CHANNEL(ahc, (scb)->hscb->scsiid)
#define	SCB_GET_LUN(scb) \
	((scb)->hscb->lun & LID)
#define SCB_GET_TARGET_OFFSET(ahc, scb)	\
	(SCB_GET_TARGET(ahc, scb) + (SCB_IS_SCSIBUS_B(ahc, scb) ? 8 : 0))
#define SCB_GET_TARGET_MASK(ahc, scb) \
	(0x01 << (SCB_GET_TARGET_OFFSET(ahc, scb)))
#ifdef AHC_DEBUG
#define SCB_IS_SILENT(scb)					\
	((ahc_debug & AHC_SHOW_MASKED_ERRORS) == 0		\
      && (((scb)->flags & SCB_SILENT) != 0))
#else
#define SCB_IS_SILENT(scb)					\
	(((scb)->flags & SCB_SILENT) != 0)
#endif
#define TCL_TARGET_OFFSET(tcl) \
	((((tcl) >> 4) & TID) >> 4)
#define TCL_LUN(tcl) \
	(tcl & (AHC_NUM_LUNS - 1))
#define BUILD_TCL(scsiid, lun) \
	((lun) | (((scsiid) & TID) << 4))

#ifndef	AHC_TARGET_MODE
#undef	AHC_TMODE_ENABLE
#define	AHC_TMODE_ENABLE 0
#endif

#define AHC_NUM_TARGETS 16

#define AHC_NUM_LUNS 64

#define AHC_MAXTRANSFER_SIZE	 0x00ffffff	

#define AHC_SCB_MAX	255

#define AHC_MAX_QUEUE	253

#define AHC_SCB_MAX_ALLOC (AHC_MAX_QUEUE+1)

#define AHC_TMODE_CMDS	256

#define AHC_BUSRESET_DELAY	25

typedef enum {
	AHC_NONE	= 0x0000,
	AHC_CHIPID_MASK	= 0x00FF,
	AHC_AIC7770	= 0x0001,
	AHC_AIC7850	= 0x0002,
	AHC_AIC7855	= 0x0003,
	AHC_AIC7859	= 0x0004,
	AHC_AIC7860	= 0x0005,
	AHC_AIC7870	= 0x0006,
	AHC_AIC7880	= 0x0007,
	AHC_AIC7895	= 0x0008,
	AHC_AIC7895C	= 0x0009,
	AHC_AIC7890	= 0x000a,
	AHC_AIC7896	= 0x000b,
	AHC_AIC7892	= 0x000c,
	AHC_AIC7899	= 0x000d,
	AHC_VL		= 0x0100,	
	AHC_EISA	= 0x0200,	
	AHC_PCI		= 0x0400,	
	AHC_BUS_MASK	= 0x0F00
} ahc_chip;

typedef enum {
	AHC_FENONE	= 0x00000,
	AHC_ULTRA	= 0x00001,	
	AHC_ULTRA2	= 0x00002,	
	AHC_WIDE  	= 0x00004,	
	AHC_TWIN	= 0x00008,	
	AHC_MORE_SRAM	= 0x00010,	
	AHC_CMD_CHAN	= 0x00020,	
	AHC_QUEUE_REGS	= 0x00040,	
	AHC_SG_PRELOAD	= 0x00080,	
	AHC_SPIOCAP	= 0x00100,	
	AHC_MULTI_TID	= 0x00200,	
	AHC_HS_MAILBOX	= 0x00400,	
	AHC_DT		= 0x00800,	
	AHC_NEW_TERMCTL	= 0x01000,	
	AHC_MULTI_FUNC	= 0x02000,	
	AHC_LARGE_SCBS	= 0x04000,	
	AHC_AUTORATE	= 0x08000,	
	AHC_AUTOPAUSE	= 0x10000,	
	AHC_TARGETMODE	= 0x20000,	
	AHC_MULTIROLE	= 0x40000,	
	AHC_REMOVABLE	= 0x80000,	
	AHC_HVD		= 0x100000,	
	AHC_AIC7770_FE	= AHC_FENONE,
	AHC_AIC7850_FE	= AHC_SPIOCAP|AHC_AUTOPAUSE|AHC_TARGETMODE|AHC_ULTRA,
	AHC_AIC7860_FE	= AHC_AIC7850_FE,
	AHC_AIC7870_FE	= AHC_TARGETMODE|AHC_AUTOPAUSE,
	AHC_AIC7880_FE	= AHC_AIC7870_FE|AHC_ULTRA,
	AHC_AIC7890_FE	= AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA2
			  |AHC_QUEUE_REGS|AHC_SG_PRELOAD|AHC_MULTI_TID
			  |AHC_HS_MAILBOX|AHC_NEW_TERMCTL|AHC_LARGE_SCBS
			  |AHC_TARGETMODE,
	AHC_AIC7892_FE	= AHC_AIC7890_FE|AHC_DT|AHC_AUTORATE|AHC_AUTOPAUSE,
	AHC_AIC7895_FE	= AHC_AIC7880_FE|AHC_MORE_SRAM|AHC_AUTOPAUSE
			  |AHC_CMD_CHAN|AHC_MULTI_FUNC|AHC_LARGE_SCBS,
	AHC_AIC7895C_FE	= AHC_AIC7895_FE|AHC_MULTI_TID,
	AHC_AIC7896_FE	= AHC_AIC7890_FE|AHC_MULTI_FUNC,
	AHC_AIC7899_FE	= AHC_AIC7892_FE|AHC_MULTI_FUNC
} ahc_feature;

typedef enum {
	AHC_BUGNONE		= 0x00,
	AHC_TMODE_WIDEODD_BUG	= 0x01,
	AHC_AUTOFLUSH_BUG	= 0x02,
	AHC_CACHETHEN_BUG	= 0x04,
	AHC_CACHETHEN_DIS_BUG	= 0x08,
	AHC_PCI_2_1_RETRY_BUG	= 0x10,
	AHC_PCI_MWI_BUG		= 0x20,
	AHC_SCBCHAN_UPLOAD_BUG	= 0x40
} ahc_bug;

typedef enum {
	AHC_FNONE	      = 0x000,
	AHC_PRIMARY_CHANNEL   = 0x003,  
	AHC_USEDEFAULTS	      = 0x004,  
	AHC_SEQUENCER_DEBUG   = 0x008,
	AHC_SHARED_SRAM	      = 0x010,
	AHC_LARGE_SEEPROM     = 0x020,  
	AHC_RESET_BUS_A	      = 0x040,
	AHC_RESET_BUS_B	      = 0x080,
	AHC_EXTENDED_TRANS_A  = 0x100,
	AHC_EXTENDED_TRANS_B  = 0x200,
	AHC_TERM_ENB_A	      = 0x400,
	AHC_TERM_ENB_B	      = 0x800,
	AHC_INITIATORROLE     = 0x1000,  
	AHC_TARGETROLE	      = 0x2000,  
	AHC_NEWEEPROM_FMT     = 0x4000,
	AHC_TQINFIFO_BLOCKED  = 0x10000,  
	AHC_INT50_SPEEDFLEX   = 0x20000,  
	AHC_SCB_BTT	      = 0x40000,  
	AHC_BIOS_ENABLED      = 0x80000,
	AHC_ALL_INTERRUPTS    = 0x100000,
	AHC_PAGESCBS	      = 0x400000,  
	AHC_EDGE_INTERRUPT    = 0x800000,  
	AHC_39BIT_ADDRESSING  = 0x1000000, 
	AHC_LSCBS_ENABLED     = 0x2000000, 
	AHC_SCB_CONFIG_USED   = 0x4000000, 
	AHC_NO_BIOS_INIT      = 0x8000000, 
	AHC_DISABLE_PCI_PERR  = 0x10000000,
	AHC_HAS_TERM_LOGIC    = 0x20000000
} ahc_flag;



struct status_pkt {
	uint32_t residual_datacnt;	
	uint32_t residual_sg_ptr;	
	uint8_t	 scsi_status;		
};

struct target_data {
	uint32_t residual_datacnt;	
	uint32_t residual_sg_ptr;	
	uint8_t  scsi_status;		
	uint8_t  target_phases;		
	uint8_t  data_phase;		
	uint8_t  initiator_tag;		
};

struct hardware_scb {
	union {
		uint8_t	 cdb[12];
		uint32_t cdb_ptr;
		struct	 status_pkt status;
		struct	 target_data tdata;
	} shared_data;
 
	uint32_t dataptr;
	uint32_t datacnt;		
	uint32_t sgptr;
#define SG_PTR_MASK	0xFFFFFFF8
	uint8_t  control;	
	uint8_t  scsiid;	
	uint8_t  lun;
	uint8_t  tag;			
	uint8_t  cdb_len;
	uint8_t  scsirate;		
	uint8_t  scsioffset;		
	uint8_t  next;			
	uint8_t  cdb32[32];		
};


struct ahc_dma_seg {
	uint32_t	addr;
	uint32_t	len;
#define	AHC_DMA_LAST_SEG	0x80000000
#define	AHC_SG_HIGH_ADDR_MASK	0x7F000000
#define	AHC_SG_LEN_MASK		0x00FFFFFF
};

struct sg_map_node {
	bus_dmamap_t		 sg_dmamap;
	dma_addr_t		 sg_physaddr;
	struct ahc_dma_seg*	 sg_vaddr;
	SLIST_ENTRY(sg_map_node) links;
};

typedef enum {
	SCB_FREE		= 0x0000,
	SCB_OTHERTCL_TIMEOUT	= 0x0002,
	SCB_DEVICE_RESET	= 0x0004,
	SCB_SENSE		= 0x0008,
	SCB_CDB32_PTR		= 0x0010,
	SCB_RECOVERY_SCB	= 0x0020,
	SCB_AUTO_NEGOTIATE	= 0x0040,
	SCB_NEGOTIATE		= 0x0080,
	SCB_ABORT		= 0x0100,
	SCB_UNTAGGEDQ		= 0x0200,
	SCB_ACTIVE		= 0x0400,
	SCB_TARGET_IMMEDIATE	= 0x0800,
	SCB_TRANSMISSION_ERROR	= 0x1000,
	SCB_TARGET_SCB		= 0x2000,
	SCB_SILENT		= 0x4000 
} scb_flag;

struct scb {
	struct	hardware_scb	 *hscb;
	union {
		SLIST_ENTRY(scb)  sle;
		TAILQ_ENTRY(scb)  tqe;
	} links;
	LIST_ENTRY(scb)		  pending_links;
	ahc_io_ctx_t		  io_ctx;
	struct ahc_softc	 *ahc_softc;
	scb_flag		  flags;
#ifndef __linux__
	bus_dmamap_t		  dmamap;
#endif
	struct scb_platform_data *platform_data;
	struct sg_map_node	 *sg_map;
	struct ahc_dma_seg 	 *sg_list;
	dma_addr_t		  sg_list_phys;
	u_int			  sg_count;
};

struct scb_data {
	SLIST_HEAD(, scb) free_scbs;	
	struct	scb *scbindex[256];	
	struct	hardware_scb	*hscbs;	
	struct	scb *scbarray;		
	struct	scsi_sense_data *sense; 

	bus_dma_tag_t	 hscb_dmat;	
	bus_dmamap_t	 hscb_dmamap;
	dma_addr_t	 hscb_busaddr;
	bus_dma_tag_t	 sense_dmat;
	bus_dmamap_t	 sense_dmamap;
	dma_addr_t	 sense_busaddr;
	bus_dma_tag_t	 sg_dmat;	
	SLIST_HEAD(, sg_map_node) sg_maps;
	uint8_t	numscbs;
	uint8_t	maxhscbs;		
	uint8_t	init_level;		
};


struct target_cmd {
	uint8_t scsiid;		
	uint8_t identify;	
	uint8_t bytes[22];	
	uint8_t cmd_valid;	
	uint8_t pad[7];
};

#define AHC_TMODE_EVENT_BUFFER_SIZE 8
struct ahc_tmode_event {
	uint8_t initiator_id;
	uint8_t event_type;	
#define	EVENT_TYPE_BUS_RESET 0xFF
	uint8_t event_arg;
};

#ifdef AHC_TARGET_MODE 
struct ahc_tmode_lstate {
	struct cam_path *path;
	struct ccb_hdr_slist accept_tios;
	struct ccb_hdr_slist immed_notifies;
	struct ahc_tmode_event event_buffer[AHC_TMODE_EVENT_BUFFER_SIZE];
	uint8_t event_r_idx;
	uint8_t event_w_idx;
};
#else
struct ahc_tmode_lstate;
#endif

#define AHC_TRANS_CUR		0x01	
#define AHC_TRANS_ACTIVE	0x03	
#define AHC_TRANS_GOAL		0x04	
#define AHC_TRANS_USER		0x08	

#define AHC_WIDTH_UNKNOWN	0xFF
#define AHC_PERIOD_UNKNOWN	0xFF
#define AHC_OFFSET_UNKNOWN	0xFF
#define AHC_PPR_OPTS_UNKNOWN	0xFF

struct ahc_transinfo {
	uint8_t protocol_version;	
	uint8_t transport_version;	
	uint8_t width;			
	uint8_t period;			
	uint8_t offset;			
	uint8_t ppr_options;		
};

struct ahc_initiator_tinfo {
	uint8_t scsirate;		
	struct ahc_transinfo curr;
	struct ahc_transinfo goal;
	struct ahc_transinfo user;
};

struct ahc_tmode_tstate {
	struct ahc_tmode_lstate*	enabled_luns[AHC_NUM_LUNS];
	struct ahc_initiator_tinfo	transinfo[AHC_NUM_TARGETS];

	uint16_t	 auto_negotiate;
	uint16_t	 ultraenb;	
	uint16_t	 discenable;	
	uint16_t	 tagenable;	
};

struct ahc_syncrate {
	u_int sxfr_u2;	
	u_int sxfr;	
#define		ULTRA_SXFR 0x100	
#define		ST_SXFR	   0x010	
#define		DT_SXFR	   0x040	
	uint8_t period; 
	const char *rate;
};

#define	AHC_ASYNC_XFER_PERIOD 0x45
#define	AHC_ULTRA2_XFER_PERIOD 0x0a

#define AHC_SYNCRATE_DT		0
#define AHC_SYNCRATE_ULTRA2	1
#define AHC_SYNCRATE_ULTRA	3
#define AHC_SYNCRATE_FAST	6
#define AHC_SYNCRATE_MAX	AHC_SYNCRATE_DT
#define	AHC_SYNCRATE_MIN	13

struct ahc_phase_table_entry {
        uint8_t phase;
        uint8_t mesg_out; 
	char *phasemsg;
};


struct seeprom_config {
	uint16_t device_flags[16];	
#define		CFXFER		0x0007	
#define		CFSYNCH		0x0008	
#define		CFDISC		0x0010	
#define		CFWIDEB		0x0020	
#define		CFSYNCHISULTRA	0x0040	
#define		CFSYNCSINGLE	0x0080	
#define		CFSTART		0x0100	
#define		CFINCBIOS	0x0200	
#define		CFRNFOUND	0x0400	
#define		CFMULTILUNDEV	0x0800	
#define		CFWBCACHEENB	0x4000	
#define		CFWBCACHENOP	0xc000	

	uint16_t bios_control;		
#define		CFSUPREM	0x0001	
#define		CFSUPREMB	0x0002	
#define		CFBIOSEN	0x0004	
#define		CFBIOS_BUSSCAN	0x0008	
#define		CFSM2DRV	0x0010	
#define		CFSTPWLEVEL	0x0010	
#define		CF284XEXTEND	0x0020		
#define		CFCTRL_A	0x0020		
#define		CFTERM_MENU	0x0040		
#define		CFEXTEND	0x0080	
#define		CFSCAMEN	0x0100	
#define		CFMSG_LEVEL	0x0600	
#define			CFMSG_VERBOSE	0x0000
#define			CFMSG_SILENT	0x0200
#define			CFMSG_DIAG	0x0400
#define		CFBOOTCD	0x0800  

	uint16_t adapter_control;		
#define		CFAUTOTERM	0x0001	
#define		CFULTRAEN	0x0002	
#define		CF284XSELTO     0x0003	
#define		CF284XFIFO      0x000C	
#define		CFSTERM		0x0004	
#define		CFWSTERM	0x0008	
#define		CFSPARITY	0x0010	
#define		CF284XSTERM     0x0020		
#define		CFMULTILUN	0x0020
#define		CFRESETB	0x0040	
#define		CFCLUSTERENB	0x0080	
#define		CFBOOTCHAN	0x0300	
#define		CFBOOTCHANSHIFT 8
#define		CFSEAUTOTERM	0x0400	
#define		CFSELOWTERM	0x0800	
#define		CFSEHIGHTERM	0x1000	
#define		CFENABLEDV	0x4000	

	uint16_t brtime_id;		
#define		CFSCSIID	0x000f	
#define		CFBRTIME	0xff00	

	uint16_t max_targets;			
#define		CFMAXTARG	0x00ff	
#define		CFBOOTLUN	0x0f00	
#define		CFBOOTID	0xf000	
	uint16_t res_1[10];		
	uint16_t signature;		
#define		CFSIGNATURE	0x250
#define		CFSIGNATURE2	0x300
	uint16_t checksum;		
};

typedef enum {
	MSG_TYPE_NONE			= 0x00,
	MSG_TYPE_INITIATOR_MSGOUT	= 0x01,
	MSG_TYPE_INITIATOR_MSGIN	= 0x02,
	MSG_TYPE_TARGET_MSGOUT		= 0x03,
	MSG_TYPE_TARGET_MSGIN		= 0x04
} ahc_msg_type;

typedef enum {
	MSGLOOP_IN_PROG,
	MSGLOOP_MSGCOMPLETE,
	MSGLOOP_TERMINATED
} msg_loop_stat;

TAILQ_HEAD(scb_tailq, scb);

struct ahc_aic7770_softc {
	uint8_t busspd;
	uint8_t bustime;
};

struct ahc_pci_softc {
	uint32_t  devconfig;
	uint16_t  targcrccnt;
	uint8_t   command;
	uint8_t   csize_lattime;
	uint8_t   optionmode;
	uint8_t   crccontrol1;
	uint8_t   dscommand0;
	uint8_t   dspcistatus;
	uint8_t   scbbaddr;
	uint8_t   dff_thrsh;
};

union ahc_bus_softc {
	struct ahc_aic7770_softc aic7770_softc;
	struct ahc_pci_softc pci_softc;
};

typedef void (*ahc_bus_intr_t)(struct ahc_softc *);
typedef int (*ahc_bus_chip_init_t)(struct ahc_softc *);
typedef int (*ahc_bus_suspend_t)(struct ahc_softc *);
typedef int (*ahc_bus_resume_t)(struct ahc_softc *);
typedef void ahc_callback_t (void *);

struct ahc_softc {
	bus_space_tag_t           tag;
	bus_space_handle_t        bsh;
#ifndef __linux__
	bus_dma_tag_t		  buffer_dmat;   
#endif
	struct scb_data		 *scb_data;

	struct scb		 *next_queued_scb;

	LIST_HEAD(, scb)	  pending_scbs;

	u_int			  untagged_queue_lock;

	struct scb_tailq	  untagged_queues[AHC_NUM_TARGETS];

	union ahc_bus_softc	  bus_softc;

	struct ahc_platform_data *platform_data;

	ahc_dev_softc_t		  dev_softc;

	ahc_bus_intr_t		  bus_intr;

	ahc_bus_chip_init_t	  bus_chip_init;

	struct ahc_tmode_tstate  *enabled_targets[AHC_NUM_TARGETS];

	struct ahc_tmode_lstate  *black_hole;

	struct ahc_tmode_lstate  *pending_device;

	ahc_chip		  chip;
	ahc_feature		  features;
	ahc_bug			  bugs;
	ahc_flag		  flags;
	struct seeprom_config	 *seep_config;

	
	uint8_t			  unpause;
	uint8_t			  pause;

	
	uint8_t			  qoutfifonext;
	uint8_t			  qinfifonext;
	uint8_t			 *qoutfifo;
	uint8_t			 *qinfifo;

	
	struct cs		 *critical_sections;
	u_int			  num_critical_sections;

	
	char			  channel;
	char			  channel_b;

	
	uint8_t			  our_id;
	uint8_t			  our_id_b;

	int			  unsolicited_ints;

	struct target_cmd	 *targetcmds;
	uint8_t			  tqinfifonext;

	uint8_t			  seqctl;

	uint8_t			  send_msg_perror;
	ahc_msg_type		  msg_type;
	uint8_t			  msgout_buf[12];
	uint8_t			  msgin_buf[12];
	u_int			  msgout_len;	
	u_int			  msgout_index;	
	u_int			  msgin_index;	

	bus_dma_tag_t		  parent_dmat;
	bus_dma_tag_t		  shared_data_dmat;
	bus_dmamap_t		  shared_data_dmamap;
	dma_addr_t		  shared_data_busaddr;

	dma_addr_t		  dma_bug_buf;

	
	u_int			  enabled_luns;

	
	u_int			  init_level;

	
	u_int			  pci_cachesize;

	u_int			  pci_target_perr_count;
#define		AHC_PCI_TARGET_PERR_THRESH	10

	
	u_int			  instruction_ram_size;

	
	const char		 *description;
	char			 *name;
	int			  unit;

	
	int			  seltime;
	int			  seltime_b;

	uint16_t	 	  user_discenable;
	uint16_t		  user_tagenable;
};

typedef enum {
	ROLE_UNKNOWN,
	ROLE_INITIATOR,
	ROLE_TARGET
} role_t;

struct ahc_devinfo {
	int	 our_scsiid;
	int	 target_offset;
	uint16_t target_mask;
	u_int	 target;
	u_int	 lun;
	char	 channel;
	role_t	 role;		
};

typedef int (ahc_device_setup_t)(struct ahc_softc *);

struct ahc_pci_identity {
	uint64_t		 full_id;
	uint64_t		 id_mask;
	const char		*name;
	ahc_device_setup_t	*setup;
};

struct aic7770_identity {
	uint32_t		 full_id;
	uint32_t		 id_mask;
	const char		*name;
	ahc_device_setup_t	*setup;
};
extern struct aic7770_identity aic7770_ident_table[];
extern const int ahc_num_aic7770_devs;

#define AHC_EISA_SLOT_OFFSET	0xc00
#define AHC_EISA_IOSIZE		0x100


const struct ahc_pci_identity	*ahc_find_pci_device(ahc_dev_softc_t);
int			 ahc_pci_config(struct ahc_softc *,
					const struct ahc_pci_identity *);
int			 ahc_pci_test_register_access(struct ahc_softc *);
#ifdef CONFIG_PM
void			 ahc_pci_resume(struct ahc_softc *ahc);
#endif

struct aic7770_identity *aic7770_find_device(uint32_t);
int			 aic7770_config(struct ahc_softc *ahc,
					struct aic7770_identity *,
					u_int port);

int		ahc_probe_scbs(struct ahc_softc *);
void		ahc_qinfifo_requeue_tail(struct ahc_softc *ahc,
					 struct scb *scb);
int		ahc_match_scb(struct ahc_softc *ahc, struct scb *scb,
			      int target, char channel, int lun,
			      u_int tag, role_t role);

struct ahc_softc	*ahc_alloc(void *platform_arg, char *name);
int			 ahc_softc_init(struct ahc_softc *);
void			 ahc_controller_info(struct ahc_softc *ahc, char *buf);
int			 ahc_chip_init(struct ahc_softc *ahc);
int			 ahc_init(struct ahc_softc *ahc);
void			 ahc_intr_enable(struct ahc_softc *ahc, int enable);
void			 ahc_pause_and_flushwork(struct ahc_softc *ahc);
#ifdef CONFIG_PM
int			 ahc_suspend(struct ahc_softc *ahc); 
int			 ahc_resume(struct ahc_softc *ahc);
#endif
void			 ahc_set_unit(struct ahc_softc *, int);
void			 ahc_set_name(struct ahc_softc *, char *);
void			 ahc_free(struct ahc_softc *ahc);
int			 ahc_reset(struct ahc_softc *ahc, int reinit);

typedef enum {
	SEARCH_COMPLETE,
	SEARCH_COUNT,
	SEARCH_REMOVE
} ahc_search_action;
int			ahc_search_qinfifo(struct ahc_softc *ahc, int target,
					   char channel, int lun, u_int tag,
					   role_t role, uint32_t status,
					   ahc_search_action action);
int			ahc_search_untagged_queues(struct ahc_softc *ahc,
						   ahc_io_ctx_t ctx,
						   int target, char channel,
						   int lun, uint32_t status,
						   ahc_search_action action);
int			ahc_search_disc_list(struct ahc_softc *ahc, int target,
					     char channel, int lun, u_int tag,
					     int stop_on_first, int remove,
					     int save_state);
int			ahc_reset_channel(struct ahc_softc *ahc, char channel,
					  int initiate_reset);

void			ahc_compile_devinfo(struct ahc_devinfo *devinfo,
					    u_int our_id, u_int target,
					    u_int lun, char channel,
					    role_t role);
const struct ahc_syncrate*	ahc_find_syncrate(struct ahc_softc *ahc, u_int *period,
					  u_int *ppr_options, u_int maxsync);
u_int			ahc_find_period(struct ahc_softc *ahc,
					u_int scsirate, u_int maxsync);
typedef enum {
	AHC_NEG_TO_GOAL,	
	AHC_NEG_IF_NON_ASYNC,	
	AHC_NEG_ALWAYS		
} ahc_neg_type;
int			ahc_update_neg_request(struct ahc_softc*,
					       struct ahc_devinfo*,
					       struct ahc_tmode_tstate*,
					       struct ahc_initiator_tinfo*,
					       ahc_neg_type);
void			ahc_set_width(struct ahc_softc *ahc,
				      struct ahc_devinfo *devinfo,
				      u_int width, u_int type, int paused);
void			ahc_set_syncrate(struct ahc_softc *ahc,
					 struct ahc_devinfo *devinfo,
					 const struct ahc_syncrate *syncrate,
					 u_int period, u_int offset,
					 u_int ppr_options,
					 u_int type, int paused);
typedef enum {
	AHC_QUEUE_NONE,
	AHC_QUEUE_BASIC,
	AHC_QUEUE_TAGGED
} ahc_queue_alg;

#ifdef AHC_TARGET_MODE
void		ahc_send_lstate_events(struct ahc_softc *,
				       struct ahc_tmode_lstate *);
void		ahc_handle_en_lun(struct ahc_softc *ahc,
				  struct cam_sim *sim, union ccb *ccb);
cam_status	ahc_find_tmode_devs(struct ahc_softc *ahc,
				    struct cam_sim *sim, union ccb *ccb,
				    struct ahc_tmode_tstate **tstate,
				    struct ahc_tmode_lstate **lstate,
				    int notfound_failure);
#ifndef AHC_TMODE_ENABLE
#define AHC_TMODE_ENABLE 0
#endif
#endif
#ifdef AHC_DEBUG
extern uint32_t ahc_debug;
#define	AHC_SHOW_MISC		0x0001
#define	AHC_SHOW_SENSE		0x0002
#define AHC_DUMP_SEEPROM	0x0004
#define AHC_SHOW_TERMCTL	0x0008
#define AHC_SHOW_MEMORY		0x0010
#define AHC_SHOW_MESSAGES	0x0020
#define	AHC_SHOW_DV		0x0040
#define AHC_SHOW_SELTO		0x0080
#define AHC_SHOW_QFULL		0x0200
#define AHC_SHOW_QUEUE		0x0400
#define AHC_SHOW_TQIN		0x0800
#define AHC_SHOW_MASKED_ERRORS	0x1000
#define AHC_DEBUG_SEQUENCER	0x2000
#endif
void			ahc_print_devinfo(struct ahc_softc *ahc,
					  struct ahc_devinfo *dev);
void			ahc_dump_card_state(struct ahc_softc *ahc);
int			ahc_print_register(const ahc_reg_parse_entry_t *table,
					   u_int num_entries,
					   const char *name,
					   u_int address,
					   u_int value,
					   u_int *cur_column,
					   u_int wrap_point);
int		ahc_acquire_seeprom(struct ahc_softc *ahc,
				    struct seeprom_descriptor *sd);
void		ahc_release_seeprom(struct seeprom_descriptor *sd);
#endif 
