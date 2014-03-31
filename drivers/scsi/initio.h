/**************************************************************************
 * Initio 9100 device driver for Linux.
 *
 * Copyright (c) 1994-1998 Initio Corporation
 * All rights reserved.
 *
 * Cleanups (c) Copyright 2007 Red Hat <alan@lxorguk.ukuu.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 **************************************************************************/


#include <linux/types.h>

#define TOTAL_SG_ENTRY		32
#define MAX_SUPPORTED_ADAPTERS  8
#define MAX_OFFSET		15
#define MAX_TARGETS		16

typedef struct {
	unsigned short base;
	unsigned short vec;
} i91u_config;

#define TUL_PVID        0x00	
#define TUL_PDID        0x02	
#define TUL_PCMD        0x04	
#define TUL_PSTUS       0x06	
#define TUL_PRID        0x08	
#define TUL_PPI         0x09	
#define TUL_PSC         0x0A	
#define TUL_PBC         0x0B	
#define TUL_PCLS        0x0C	
#define TUL_PLTR        0x0D	
#define TUL_PHDT        0x0E	
#define TUL_PBIST       0x0F	
#define TUL_PBAD        0x10	
#define TUL_PBAD1       0x14	
#define TUL_PBAD2       0x18	
#define TUL_PBAD3       0x1C	
#define TUL_PBAD4       0x20	
#define TUL_PBAD5       0x24	
#define TUL_PRSVD       0x28	
#define TUL_PRSVD1      0x2C	
#define TUL_PRAD        0x30	
#define TUL_PRSVD2      0x34	
#define TUL_PRSVD3      0x38	
#define TUL_PINTL       0x3C	
#define TUL_PINTP       0x3D	
#define TUL_PIGNT       0x3E	
#define TUL_PMGNT       0x3F	

#define TUL_HACFG0      0x40	
#define TUL_HACFG1      0x41	
#define TUL_HACFG2      0x42	

#define TUL_SDCFG0      0x44	
#define TUL_SDCFG1      0x45	
#define TUL_SDCFG2      0x46	
#define TUL_SDCFG3      0x47	

#define TUL_GINTS       0x50	
#define TUL_GIMSK       0x52	
#define TUL_GCTRL       0x54	
#define TUL_GCTRL_EEPROM_BIT    0x04
#define TUL_GCTRL1      0x55	
#define TUL_DMACFG      0x5B	
#define TUL_NVRAM       0x5D	

#define TUL_SCnt0       0x80	
#define TUL_SCnt1       0x81	
#define TUL_SCnt2       0x82	
#define TUL_SFifoCnt    0x83	
#define TUL_SIntEnable  0x84	
#define TUL_SInt        0x84	
#define TUL_SCtrl0      0x85	
#define TUL_SStatus0    0x85	
#define TUL_SCtrl1      0x86	
#define TUL_SStatus1    0x86	
#define TUL_SConfig     0x87	
#define TUL_SStatus2    0x87	
#define TUL_SPeriod     0x88	
#define TUL_SOffset     0x88	
#define TUL_SScsiId     0x89	
#define TUL_SBusId      0x89	
#define TUL_STimeOut    0x8A	
#define TUL_SIdent      0x8A	
#define TUL_SAvail      0x8A	
#define TUL_SData       0x8B	
#define TUL_SFifo       0x8C	
#define TUL_SSignal     0x90	
#define TUL_SCmd        0x91	
#define TUL_STest0      0x92	
#define TUL_STest1      0x93	
#define TUL_SCFG1	0x94	

#define TUL_XAddH       0xC0	
#define TUL_XAddW       0xC8	
#define TUL_XCntH       0xD0	
#define TUL_XCntW       0xD4	
#define TUL_XCmd        0xD8	
#define TUL_Int         0xDC	
#define TUL_XStatus     0xDD	
#define TUL_Mask        0xE0	
#define TUL_XCtrl       0xE4	
#define TUL_XCtrl1      0xE5	
#define TUL_XFifo       0xE8	

#define TUL_WCtrl       0xF7	
#define TUL_DCtrl       0xFB	

#define BUSMS           0x04	
#define IOSPA           0x01	

#define TSC_EN_RESEL    0x80	
#define TSC_CMD_COMP    0x84	
#define TSC_SEL         0x01	
#define TSC_SEL_ATN     0x11	
#define TSC_SEL_ATN_DMA 0x51	
#define TSC_SEL_ATN3    0x31	
#define TSC_SEL_ATNSTOP 0x12	
#define TSC_SELATNSTOP  0x1E	

#define TSC_SEL_ATN_DIRECT_IN   0x95	
#define TSC_SEL_ATN_DIRECT_OUT  0x15	
#define TSC_SEL_ATN3_DIRECT_IN  0xB5	
#define TSC_SEL_ATN3_DIRECT_OUT 0x35	
#define TSC_XF_DMA_OUT_DIRECT   0x06	
#define TSC_XF_DMA_IN_DIRECT    0x86	

#define TSC_XF_DMA_OUT  0x43	
#define TSC_XF_DMA_IN   0xC3	
#define TSC_XF_FIFO_OUT 0x03	
#define TSC_XF_FIFO_IN  0x83	

#define TSC_MSG_ACCEPT  0x0F	

#define TSC_RST_SEQ     0x20	
#define TSC_FLUSH_FIFO  0x10	
#define TSC_ABT_CMD     0x04	
#define TSC_RST_CHIP    0x02	
#define TSC_RST_BUS     0x01	

#define TSC_EN_SCAM     0x80	
#define TSC_TIMER       0x40	
#define TSC_EN_SCSI2    0x20	
#define TSC_PWDN        0x10	
#define TSC_WIDE_CPU    0x08	
#define TSC_HW_RESELECT 0x04	
#define TSC_EN_BUS_OUT  0x02	
#define TSC_EN_BUS_IN   0x01	

#define TSC_EN_LATCH    0x80	
#define TSC_INITIATOR   0x40	
#define TSC_EN_SCSI_PAR 0x20	
#define TSC_DMA_8BIT    0x10	
#define TSC_DMA_16BIT   0x08	
#define TSC_EN_WDACK    0x04	
#define TSC_ALT_PERIOD  0x02	
#define TSC_DIS_SCSIRST 0x01	

#define TSC_INITDEFAULT (TSC_INITIATOR | TSC_EN_LATCH | TSC_ALT_PERIOD | TSC_DIS_SCSIRST)

#define TSC_WIDE_SCSI   0x80	

#define TSC_RST_ACK     0x00	
#define TSC_RST_ATN     0x00	
#define TSC_RST_BSY     0x00	

#define TSC_SET_ACK     0x40	
#define TSC_SET_ATN     0x08	

#define TSC_REQI        0x80	
#define TSC_ACKI        0x40	
#define TSC_BSYI        0x20	
#define TSC_SELI        0x10	
#define TSC_ATNI        0x08	
#define TSC_MSGI        0x04	
#define TSC_CDI         0x02	
#define TSC_IOI         0x01	


#define TSS_INT_PENDING 0x80	
#define TSS_SEQ_ACTIVE  0x40	
#define TSS_XFER_CNT    0x20	
#define TSS_FIFO_EMPTY  0x10	
#define TSS_PAR_ERROR   0x08	
#define TSS_PH_MASK     0x07	

#define TSS_STATUS_RCV  0x08	
#define TSS_MSG_SEND    0x40	
#define TSS_CMD_PH_CMP  0x20	
#define TSS_DATA_PH_CMP 0x10	
#define TSS_STATUS_SEND 0x08	
#define TSS_XFER_CMP    0x04	
#define TSS_SEL_CMP     0x02	
#define TSS_ARB_CMP     0x01	

#define TSS_CMD_ABTED   0x80	
#define TSS_OFFSET_0    0x40	
#define TSS_FIFO_FULL   0x20	
#define TSS_TIMEOUT_0   0x10	
#define TSS_BUSY_RLS    0x08	
#define TSS_PH_MISMATCH 0x04	
#define TSS_SCSI_BUS_EN 0x02	
#define TSS_SCSIRST     0x01	

#define TSS_RESEL_INT   0x80	
#define TSS_SEL_TIMEOUT 0x40	
#define TSS_BUS_SERV    0x20
#define TSS_SCSIRST_INT 0x10	
#define TSS_DISC_INT    0x08	
#define TSS_SEL_INT     0x04	
#define TSS_SCAM_SEL    0x02	
#define TSS_FUNC_COMP   0x01

#define DATA_OUT        0
#define DATA_IN         1	
#define CMD_OUT         2
#define STATUS_IN       3	
#define MSG_OUT         6	
#define MSG_IN          7



#define TAX_X_FORC      0x02
#define TAX_X_ABT       0x04
#define TAX_X_CLR_FIFO  0x08

#define TAX_X_IN        0x21
#define TAX_X_OUT       0x01
#define TAX_SG_IN       0xA1
#define TAX_SG_OUT      0x81

#define XCMP            0x01
#define FCMP            0x02
#define XABT            0x04
#define XERR            0x08
#define SCMP            0x10
#define IPEND           0x80

#define XPEND           0x01	
#define FEMPTY          0x02	



#define EXTSG           0x80
#define EXTAD           0x60
#define SEG4K           0x08
#define EEPRG           0x04
#define MRMUL           0x02

#define SE2CS           0x08
#define SE2CLK          0x04
#define SE2DO           0x02
#define SE2DI           0x01


struct sg_entry {
	u32 data;		
	u32 len;		
};

struct scsi_ctrl_blk {
	struct scsi_ctrl_blk *next;
	u8 status;	
	u8 next_state;	
	u8 mode;		
	u8 msgin;	
	u16 sgidx;	
	u16 sgmax;	
#ifdef ALPHA
	u32 reserved[2];	
#else
	u32 reserved[3];	
#endif

	u32 xferlen;	
	u32 totxlen;	
	u32 paddr;		

	u8 opcode;	
	u8 flags;	
	u8 target;	
	u8 lun;		
	u32 bufptr;		
	u32 buflen;		
	u8 sglen;	
	u8 senselen;	
	u8 hastat;	
	u8 tastat;	
	u8 cdblen;	
	u8 ident;	
	u8 tagmsg;	
	u8 tagid;	
	u8 cdb[12];	
	u32 sgpaddr;	
	u32 senseptr;	
	void (*post) (u8 *, u8 *);	
	struct scsi_cmnd *srb;	
	struct sg_entry sglist[TOTAL_SG_ENTRY];	
};

#define SCB_RENT        0x01
#define SCB_PEND        0x02
#define SCB_CONTIG      0x04	
#define SCB_SELECT      0x08
#define SCB_BUSY        0x10
#define SCB_DONE        0x20


#define ExecSCSI        0x1
#define BusDevRst       0x2
#define AbortCmd        0x3


#define SCM_RSENS       0x01	


#define SCF_DONE        0x01
#define SCF_POST        0x02
#define SCF_SENSE       0x04
#define SCF_DIR         0x18
#define SCF_NO_DCHK     0x00
#define SCF_DIN         0x08
#define SCF_DOUT        0x10
#define SCF_NO_XF       0x18
#define SCF_WR_VF       0x20	
#define SCF_POLL        0x40
#define SCF_SG          0x80

#define HOST_SEL_TOUT   0x11
#define HOST_DO_DU      0x12
#define HOST_BUS_FREE   0x13
#define HOST_BAD_PHAS   0x14
#define HOST_INV_CMD    0x16
#define HOST_ABORTED    0x1A	
#define HOST_SCSI_RST   0x1B
#define HOST_DEV_RST    0x1C

#define TARGET_CHKCOND  0x02
#define TARGET_BUSY     0x08
#define INI_QUEUE_FULL	0x28

#define MSG_COMP        0x00
#define MSG_EXTEND      0x01
#define MSG_SDP         0x02
#define MSG_RESTORE     0x03
#define MSG_DISC        0x04
#define MSG_IDE         0x05
#define MSG_ABORT       0x06
#define MSG_REJ         0x07
#define MSG_NOP         0x08
#define MSG_PARITY      0x09
#define MSG_LINK_COMP   0x0A
#define MSG_LINK_FLAG   0x0B
#define MSG_DEVRST      0x0C
#define MSG_ABORT_TAG   0x0D

#define MSG_STAG        0x20
#define MSG_HTAG        0x21
#define MSG_OTAG        0x22

#define MSG_IGNOREWIDE  0x23

#define MSG_IDENT   0x80


struct target_control {
	u16 flags;
	u8 js_period;
	u8 sconfig0;
	u16 drv_flags;
	u8 heads;
	u8 sectors;
};


#define TCF_SCSI_RATE           0x0007
#define TCF_EN_DISC             0x0008
#define TCF_NO_SYNC_NEGO        0x0010
#define TCF_NO_WDTR             0x0020
#define TCF_EN_255              0x0040
#define TCF_EN_START            0x0080
#define TCF_WDTR_DONE           0x0100
#define TCF_SYNC_DONE           0x0200
#define TCF_BUSY                0x0400


#define TCF_DRV_BUSY            0x01	
#define TCF_DRV_EN_TAG          0x0800
#define TCF_DRV_255_63          0x0400

struct initio_host {
	u16 addr;		
	u16 bios_addr;		
	u8 irq;			
	u8 scsi_id;		
	u8 max_tar;		
	u8 num_scbs;		

	u8 flags;		
	u8 index;		
	u8 ha_id;		
	u8 config;		
	u16 idmask;		
	u8 semaph;		
	u8 phase;		
	u8 jsstatus0;		
	u8 jsint;		
	u8 jsstatus1;		
	u8 sconf1;		

	u8 msg[8];		
	struct scsi_ctrl_blk *next_avail;	
	struct scsi_ctrl_blk *scb;		
	struct scsi_ctrl_blk *scb_end;		 
	struct scsi_ctrl_blk *next_pending;	
	struct scsi_ctrl_blk *next_contig;	 
	struct scsi_ctrl_blk *active;		
	struct target_control *active_tc;	

	struct scsi_ctrl_blk *first_avail;	
	struct scsi_ctrl_blk *last_avail;	
	struct scsi_ctrl_blk *first_pending;	
	struct scsi_ctrl_blk *last_pending;	
	struct scsi_ctrl_blk *first_busy;	
	struct scsi_ctrl_blk *last_busy;	
	struct scsi_ctrl_blk *first_done;	
	struct scsi_ctrl_blk *last_done;	
	u8 max_tags[16];	
	u8 act_tags[16];	
	struct target_control targets[MAX_TARGETS];	
	spinlock_t avail_lock;
	spinlock_t semaph_lock;
	struct pci_dev *pci_dev;
};

#define HCC_SCSI_RESET          0x01
#define HCC_EN_PAR              0x02
#define HCC_ACT_TERM1           0x04
#define HCC_ACT_TERM2           0x08
#define HCC_AUTO_TERM           0x10
#define HCC_EN_PWR              0x80

#define HCF_EXPECT_DISC         0x01
#define HCF_EXPECT_SELECT       0x02
#define HCF_EXPECT_RESET        0x10
#define HCF_EXPECT_DONE_DISC    0x20


typedef struct _NVRAM_SCSI {	
	u8 NVM_ChSCSIID;	
	u8 NVM_ChConfig1;	
	u8 NVM_ChConfig2;	
	u8 NVM_NumOfTarg;	
	
	u8 NVM_Targ0Config;	
	u8 NVM_Targ1Config;	
	u8 NVM_Targ2Config;	
	u8 NVM_Targ3Config;	
	u8 NVM_Targ4Config;	
	u8 NVM_Targ5Config;	
	u8 NVM_Targ6Config;	
	u8 NVM_Targ7Config;	
	u8 NVM_Targ8Config;	
	u8 NVM_Targ9Config;	
	u8 NVM_TargAConfig;	
	u8 NVM_TargBConfig;	
	u8 NVM_TargCConfig;	
	u8 NVM_TargDConfig;	
	u8 NVM_TargEConfig;	
	u8 NVM_TargFConfig;	
} NVRAM_SCSI;

typedef struct _NVRAM {
	u16 NVM_Signature;	
	u8 NVM_Size;		
	u8 NVM_Revision;	
	
	u8 NVM_ModelByte0;	
	u8 NVM_ModelByte1;	
	u8 NVM_ModelInfo;	
	u8 NVM_NumOfCh;	
	u8 NVM_BIOSConfig1;	
	u8 NVM_BIOSConfig2;	
	u8 NVM_HAConfig1;	
	u8 NVM_HAConfig2;	
	NVRAM_SCSI NVM_SCSIInfo[2];
	u8 NVM_reserved[10];
	
	u16 NVM_CheckSum;	
} NVRAM, *PNVRAM;

#define NBC1_ENABLE             0x01	
#define NBC1_8DRIVE             0x02	
#define NBC1_REMOVABLE          0x04	
#define NBC1_INT19              0x08	
#define NBC1_BIOSSCAN           0x10	
#define NBC1_LUNSUPPORT         0x40	

#define NHC1_BOOTIDMASK 0x0F	
#define NHC1_LUNMASK    0x70	
#define NHC1_CHANMASK   0x80	

#define NCC1_BUSRESET           0x01	
#define NCC1_PARITYCHK          0x02	
#define NCC1_ACTTERM1           0x04	
#define NCC1_ACTTERM2           0x08	
#define NCC1_AUTOTERM           0x10	
#define NCC1_PWRMGR             0x80	

#define NTC_DISCONNECT          0x08	
#define NTC_SYNC                0x10	
#define NTC_NO_WDTR             0x20	
#define NTC_1GIGA               0x40	
#define NTC_SPINUP              0x80	

#define INI_SIGNATURE           0xC925
#define NBC1_DEFAULT            (NBC1_ENABLE)
#define NCC1_DEFAULT            (NCC1_BUSRESET | NCC1_AUTOTERM | NCC1_PARITYCHK)
#define NTC_DEFAULT             (NTC_NO_WDTR | NTC_1GIGA | NTC_DISCONNECT)

#define DISC_NOT_ALLOW          0x80	
#define DISC_ALLOW              0xC0	
#define SCSICMD_RequestSense    0x03

#define SCSI_ABORT_SNOOZE 0
#define SCSI_ABORT_SUCCESS 1
#define SCSI_ABORT_PENDING 2
#define SCSI_ABORT_BUSY 3
#define SCSI_ABORT_NOT_RUNNING 4
#define SCSI_ABORT_ERROR 5

#define SCSI_RESET_SNOOZE 0
#define SCSI_RESET_PUNT 1
#define SCSI_RESET_SUCCESS 2
#define SCSI_RESET_PENDING 3
#define SCSI_RESET_WAKEUP 4
#define SCSI_RESET_NOT_RUNNING 5
#define SCSI_RESET_ERROR 6

#define SCSI_RESET_SYNCHRONOUS		0x01
#define SCSI_RESET_ASYNCHRONOUS		0x02
#define SCSI_RESET_SUGGEST_BUS_RESET	0x04
#define SCSI_RESET_SUGGEST_HOST_RESET	0x08

#define SCSI_RESET_BUS_RESET 0x100
#define SCSI_RESET_HOST_RESET 0x200
#define SCSI_RESET_ACTION   0xff

