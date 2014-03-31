/*
 * Initio A100 device driver for Linux.
 *
 * Copyright (c) 1994-1998 Initio Corporation
 * All rights reserved.
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
 * Revision History:
 * 06/18/98 HL, Initial production Version 1.02
 * 12/19/98 bv, Use spinlocks for 2.1.95 and up
 * 06/25/02 Doug Ledford <dledford@redhat.com>
 *	 - This and the i60uscsi.h file are almost identical,
 *	   merged them into a single header used by both .c files.
 * 14/06/07 Alan Cox <alan@redhat.com>
 *	 - Grand cleanup and Linuxisation
 */

#define inia100_REVID "Initio INI-A100U2W SCSI device driver; Revision: 1.02d"

#if 1
#define ORC_MAXQUEUE		245
#define ORC_MAXTAGS		64
#else
#define ORC_MAXQUEUE		25
#define ORC_MAXTAGS		8
#endif

#define TOTAL_SG_ENTRY		32
#define MAX_TARGETS		16
#define IMAX_CDB			15
#define SENSE_SIZE		14

struct orc_sgent {
	u32 base;		
	u32 length;		
};

#define DISC_NOT_ALLOW          0x80	
#define DISC_ALLOW              0xC0	


#define ORC_OFFSET_SCB			16
#define ORC_MAX_SCBS		    250
#define MAX_CHANNELS       2
#define MAX_ESCB_ELE				64
#define TCF_DRV_255_63     0x0400

#define ORC_CMD_NOP		0x00	
#define ORC_CMD_VERSION		0x01	
#define ORC_CMD_ECHO		0x02	
#define ORC_CMD_SET_NVM		0x03	
#define ORC_CMD_GET_NVM		0x04	
#define ORC_CMD_GET_BUS_STATUS	0x05	
#define ORC_CMD_ABORT_SCB	0x06	
#define ORC_CMD_ISSUE_SCB	0x07	

#define ORC_GINTS	0xA0	
#define QINT		0x04	
#define ORC_GIMSK	0xA1	
#define MQINT		0x04	
#define	ORC_GCFG	0xA2	
#define EEPRG		0x01	
#define	ORC_GSTAT	0xA3	
#define WIDEBUS		0x10	
#define ORC_HDATA	0xA4	
#define ORC_HCTRL	0xA5	
#define SCSIRST		0x80	
#define HDO			0x40	
#define HOSTSTOP		0x02	
#define DEVRST		0x01	
#define ORC_HSTUS	0xA6	
#define HDI			0x02	
#define RREADY		0x01	
#define	ORC_NVRAM	0xA7	
#define SE2CS		0x008
#define SE2CLK		0x004
#define SE2DO		0x002
#define SE2DI		0x001
#define ORC_PQUEUE	0xA8	
#define ORC_PQCNT	0xA9	
#define ORC_RQUEUE	0xAA	
#define ORC_RQUEUECNT	0xAB	
#define	ORC_FWBASEADR	0xAC	

#define	ORC_EBIOSADR0 0xB0	
#define	ORC_EBIOSADR1 0xB1	
#define	ORC_EBIOSADR2 0xB2	
#define	ORC_EBIOSDATA 0xB3	

#define	ORC_SCBSIZE	0xB7	
#define	ORC_SCBBASE0	0xB8	
#define	ORC_SCBBASE1	0xBC	

#define	ORC_RISCCTL	0xE0	
#define PRGMRST		0x002
#define DOWNLOAD		0x001
#define	ORC_PRGMCTR0	0xE2	
#define	ORC_PRGMCTR1	0xE3	
#define	ORC_RISCRAM	0xEC	

struct orc_extended_scb {	
	struct orc_sgent sglist[TOTAL_SG_ENTRY];	
	struct scsi_cmnd *srb;	
};

struct orc_scb {	
	u8 opcode;	
	u8 flags;	
	u8 target;	
	u8 lun;		
	u32 reserved0;	
	u32 xferlen;	
	u32 reserved1;	
	u32 sg_len;		
	u32 sg_addr;	
	u32 sg_addrhigh;	
	u8 hastat;	
	u8 tastat;	
	u8 status;	
	u8 link;		
	u8 sense_len;	
	u8 cdb_len;	
	u8 ident;	
	u8 tag_msg;	
	u8 cdb[IMAX_CDB];	
	u8 scbidx;	
	u32 sense_addr;	

	struct orc_extended_scb *escb; 
        
#ifndef CONFIG_64BIT
	u8 reserved2[4];	
#endif
};

#define ORC_EXECSCSI	0x00	
#define ORC_BUSDEVRST	0x01	

#define ORCSCB_COMPLETE	0x00	
#define ORCSCB_POST	0x01	

#define SCF_DISINT	0x01	
#define SCF_DIR		0x18	
#define SCF_NO_DCHK	0x00	
#define SCF_DIN		0x08	
#define SCF_DOUT	0x10	
#define SCF_NO_XF	0x18	
#define SCF_POLL   0x40

#define HOST_SEL_TOUT	0x11
#define HOST_DO_DU	0x12
#define HOST_BUS_FREE	0x13
#define HOST_BAD_PHAS	0x14
#define HOST_INV_CMD	0x16
#define HOST_SCSI_RST	0x1B
#define HOST_DEV_RST	0x1C


#define TARGET_CHK_COND	0x02
#define TARGET_BUSY	0x08
#define TARGET_TAG_FULL	0x28



struct orc_target {
	u8 TCS_DrvDASD;	
	u8 TCS_DrvSCSI;	
	u8 TCS_DrvHead;	
	u16 TCS_DrvFlags;	
	u8 TCS_DrvSector;	
};

#define	TCS_DF_NODASD_SUPT	0x20	
#define	TCS_DF_NOSCSI_SUPT	0x40	


struct orc_host {
	unsigned long base;	
	u8 index;		
	u8 scsi_id;		
	u8 BIOScfg;		
	u8 flags;
	u8 max_targets;		
	struct orc_scb *scb_virt;	
	dma_addr_t scb_phys;	
	struct orc_extended_scb *escb_virt; 
	dma_addr_t escb_phys;	
	u8 target_flag[16];	
	u8 max_tags[16];	
	u32 allocation_map[MAX_CHANNELS][8];	
	spinlock_t allocation_lock;
	struct pci_dev *pdev;
};


#define HCF_SCSI_RESET	0x01	
#define HCF_PARITY    	0x02	
#define HCF_LVDS     	0x10	


#define TCF_EN_255	    0x08
#define TCF_EN_TAG	    0x10
#define TCF_BUSY	      0x20
#define TCF_DISCONNECT	0x40
#define TCF_SPIN_UP	  0x80

#define	HCS_AF_IGNORE		0x01	
#define	HCS_AF_DISABLE_RESET	0x10	
#define	HCS_AF_DISABLE_ADPT	0x80	

struct orc_nvram {
        u8 SubVendorID0;     
        u8 SubVendorID1;     
        u8 SubSysID0;        
        u8 SubSysID1;        
        u8 SubClass;         
        u8 VendorID0;        
        u8 VendorID1;        
        u8 DeviceID0;        
        u8 DeviceID1;        
        u8 Reserved0[2];     
        u8 revision;         
        
        u8 NumOfCh;          
        u8 BIOSConfig1;      
        u8 BIOSConfig2;      
        u8 BIOSConfig3;      
        
        
        u8 scsi_id;          
        u8 SCSI0Config;      
        u8 SCSI0MaxTags;     
        u8 SCSI0ResetTime;   
        u8 ReservedforChannel0[2];   

        
        
        u8 Target00Config;   
        u8 Target01Config;   
        u8 Target02Config;   
        u8 Target03Config;   
        u8 Target04Config;   
        u8 Target05Config;   
        u8 Target06Config;   
        u8 Target07Config;   
        u8 Target08Config;   
        u8 Target09Config;   
        u8 Target0AConfig;   
        u8 Target0BConfig;   
        u8 Target0CConfig;   
        u8 Target0DConfig;   
        u8 Target0EConfig;   
        u8 Target0FConfig;   

        u8 SCSI1Id;          
        u8 SCSI1Config;      
        u8 SCSI1MaxTags;     
        u8 SCSI1ResetTime;   
        u8 ReservedforChannel1[2];   

        
        
        u8 Target10Config;   
        u8 Target11Config;   
        u8 Target12Config;   
        u8 Target13Config;   
        u8 Target14Config;   
        u8 Target15Config;   
        u8 Target16Config;   
        u8 Target17Config;   
        u8 Target18Config;   
        u8 Target19Config;   
        u8 Target1AConfig;   
        u8 Target1BConfig;   
        u8 Target1CConfig;   
        u8 Target1DConfig;   
        u8 Target1EConfig;   
        u8 Target1FConfig;   
        u8 reserved[3];      
        
        u8 CheckSum;         
};

#define NBC_BIOSENABLE  0x01    
#define NBC_CDROM       0x02    
#define NBC_REMOVABLE   0x04    

#define NBB_TARGET_MASK 0x0F    
#define NBB_CHANL_MASK  0xF0    

#define NCC_BUSRESET    0x01    
#define NCC_PARITYCHK   0x02    
#define NCC_LVDS        0x10    
#define NCC_ACTTERM1    0x20    
#define NCC_ACTTERM2    0x40    
#define NCC_AUTOTERM    0x80    

#define NTC_PERIOD      0x07    
#define NTC_1GIGA       0x08    
#define NTC_NO_SYNC     0x10    
#define NTC_NO_WIDESYNC 0x20    
#define NTC_DISC_ENABLE 0x40    
#define NTC_SPINUP      0x80    

#define NBC_DEFAULT     (NBC_ENABLE)
#define NCC_DEFAULT     (NCC_BUSRESET | NCC_AUTOTERM | NCC_PARITYCHK)
#define NCC_MAX_TAGS    0x20    
#define NCC_RESET_TIME  0x0A    
#define NTC_DEFAULT     (NTC_1GIGA | NTC_NO_WIDESYNC | NTC_DISC_ENABLE)

