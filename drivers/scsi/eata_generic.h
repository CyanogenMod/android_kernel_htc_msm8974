

#ifndef _EATA_GENERIC_H
#define _EATA_GENERIC_H




#define R_LIMIT 0x20000

#define MAXISA	   4
#define MAXEISA	  16  
#define MAXPCI	  16
#define MAXIRQ	  16 
#define MAXTARGET 16
#define MAXCHANNEL 3

#define IS_ISA	   'I'
#define IS_EISA	   'E'
#define IS_PCI	   'P'

#define BROKEN_INQUIRY	1

#define BUSMASTER       0xff
#define PIO             0xfe

#define EATA_SIGNATURE	0x45415441     

#define DPT_ID1         0x12
#define DPT_ID2         0x14

#define ATT_ID1         0x06
#define ATT_ID2         0x94
#define ATT_ID3         0x0

#define NEC_ID1         0x38
#define NEC_ID2         0xa3
#define NEC_ID3         0x82

 
#define EATA_CP_SIZE	 44

#define MAX_PCI_DEVICES  32	       
#define MAX_METHOD_2	 16	       
#define MAX_PCI_BUS	 16	       

#define SG_SIZE		 64 
#define SG_SIZE_BIG	 252	       

#define UPPER_DEVICE_QUEUE_LIMIT 64    

#define TYPE_DISK_QUEUE  16
#define TYPE_TAPE_QUEUE  4
#define TYPE_ROM_QUEUE   4
#define TYPE_OTHER_QUEUE 2

#define FREE	         0
#define OK	         0
#define NO_TIMEOUT       0
#define USED	         1
#define TIMEOUT	         2
#define RESET	         4
#define LOCKED	         8
#define ABORTED          16

#define READ             0
#define WRITE            1
#define OTHER            2

#define HD(cmd)	 ((hostdata *)&(cmd->device->host->hostdata))
#define CD(cmd)	 ((struct eata_ccb *)(cmd->host_scribble))
#define SD(host) ((hostdata *)&(host->hostdata))

#define PCI_REG_DPTconfig	 0x40	 
#define PCI_REG_PumpModeAddress	 0x44	 
#define PCI_REG_PumpModeData	 0x48	 
#define PCI_REG_ConfigParam1	 0x50	 
#define PCI_REG_ConfigParam2	 0x54	 


#define EATA_CMD_PIO_SETUPTEST	 0xc6
#define EATA_CMD_PIO_READ_CONFIG 0xf0
#define EATA_CMD_PIO_SET_CONFIG	 0xf1
#define EATA_CMD_PIO_SEND_CP	 0xf2
#define EATA_CMD_PIO_RECEIVE_SP	 0xf3
#define EATA_CMD_PIO_TRUNC	 0xf4

#define EATA_CMD_RESET		 0xf9
#define EATA_CMD_IMMEDIATE	 0xfa

#define EATA_CMD_DMA_READ_CONFIG 0xfd
#define EATA_CMD_DMA_SET_CONFIG	 0xfe
#define EATA_CMD_DMA_SEND_CP	 0xff

#define ECS_EMULATE_SENSE	 0xd4

#define EATA_GENERIC_ABORT       0x00 
#define EATA_SPECIFIC_RESET      0x01
#define EATA_BUS_RESET           0x02
#define EATA_SPECIFIC_ABORT      0x03
#define EATA_QUIET_INTR          0x04
#define EATA_COLD_BOOT_HBA       0x06	   
#define EATA_FORCE_IO            0x07

#define HA_CTRLREG     0x206       
#define HA_CTRL_DISINT 0x02        
#define HA_CTRL_RESCPU 0x04        
#define HA_CTRL_8HEADS 0x08        

#define HA_WCOMMAND    0x07	   
#define HA_WIFC        0x06	   
#define HA_WCODE       0x05 
#define HA_WCODE2      0x04 
#define HA_WDMAADDR    0x02	     
#define HA_RAUXSTAT    0x08	   
#define HA_RSTATUS     0x07	   
#define HA_RDATA       0x00	   
#define HA_WDATA       0x00	   

#define HA_ABUSY       0x01	   
#define HA_AIRQ	       0x02	   
#define HA_SERROR      0x01	   
#define HA_SMORE       0x02	   
#define HA_SCORR       0x04	   
#define HA_SDRQ	       0x08	   
#define HA_SSC	       0x10	   
#define HA_SFAULT      0x20	   
#define HA_SREADY      0x40	   
#define HA_SBUSY       0x80	   
#define HA_SDRDY       HA_SSC+HA_SREADY+HA_SDRQ 


#define HA_NO_ERROR	 0x00	
#define HA_ERR_SEL_TO	 0x01	
#define HA_ERR_CMD_TO	 0x02	
#define HA_BUS_RESET	 0x03	
#define HA_INIT_POWERUP	 0x04	
#define HA_UNX_BUSPHASE	 0x05	
#define HA_UNX_BUS_FREE	 0x06	
#define HA_BUS_PARITY	 0x07	
#define HA_SCSI_HUNG	 0x08	
#define HA_UNX_MSGRJCT	 0x09	
#define HA_RESET_STUCK	 0x0a	
#define HA_RSENSE_FAIL	 0x0b	
#define HA_PARITY_ERR	 0x0c	
#define HA_CP_ABORT_NA	 0x0d	
#define HA_CP_ABORTED	 0x0e	
#define HA_CP_RESET_NA	 0x0f	
#define HA_CP_RESET	 0x10	
#define HA_ECC_ERR	 0x11	
#define HA_PCI_PARITY	 0x12	
#define HA_PCI_MABORT	 0x13	
#define HA_PCI_TABORT	 0x14	
#define HA_PCI_STABORT	 0x15	


struct reg_bit {      
    __u8 error:1;     
    __u8 more:1;      
    __u8 corr:1;      
    __u8 drq:1;	           
    __u8 sc:1;	      
    __u8 fault:1;     
    __u8 ready:1;     
    __u8 busy:1;      
};

struct reg_abit {     
    __u8 abusy:1;     
    __u8 irq:1;	      
    __u8 dummy:6;
};

struct eata_register {	    
    __u8 data_reg[2];	    
    __u8 cp_addr[4];	    
    union { 
	__u8 command;	    
	struct reg_bit status;	
	__u8 statusbyte;
    } ovr;   
    struct reg_abit aux_stat; 
};

struct get_conf {	      
    __u32  len;		      
    __u32 signature;	      
    __u8    version2:4,
	     version:4;	      
    __u8 OCS_enabled:1,	      
	 TAR_support:1,	      
	      TRNXFR:1,	      
	MORE_support:1,	      
	 DMA_support:1,	      
	   DMA_valid:1,	      
		 ATA:1,	      
	   HAA_valid:1;	      

    __u16 cppadlen;	      
    __u8 scsi_id[4];	      
    __u32  cplen;	      
    __u32  splen;	      
    __u16 queuesiz;	      
    __u16 dummy;
    __u16 SGsiz;	      
    __u8    IRQ:4,	      
	 IRQ_TR:1,	      
	 SECOND:1,	      
    DMA_channel:2;	      
    __u8 sync;		      
    __u8   DSBLE:1,	      
	 FORCADR:1,	      
	  SG_64K:1,
	  SG_UAE:1,
		:4;
    __u8  MAX_ID:5,	      
	MAX_CHAN:3;	      
    __u8 MAX_LUN;	      
    __u8	:3,
	 AUTOTRM:1,
	 M1_inst:1,
	 ID_qest:1,	      
	  is_PCI:1,	      
	 is_EISA:1;	      
    __u8 RAIDNUM;             
    __u8 unused[474]; 
};

struct eata_sg_list
{
    __u32 data;
    __u32 len;
};

struct eata_ccb {	      
 
    __u8 SCSI_Reset:1,	      
	   HBA_Init:1,	      
       Auto_Req_Sen:1,	      
	    scatter:1,	      
	     Resrvd:1,	      
	  Interpret:1,	      
	    DataOut:1,	      
	     DataIn:1;	      
    __u8 reqlen;	      
    __u8 unused[3];
    __u8  FWNEST:1,	      
	 unused2:7;
    __u8 Phsunit:1,	      
	    I_AT:1,	      
	 I_HBA_C:1,	      
	 unused3:5;

    __u8     cp_id:5,	       
	cp_channel:3;	      
    __u8    cp_lun:3,
		  :2,
	 cp_luntar:1,	      
	 cp_dispri:1,	      
       cp_identify:1;	      
    __u8 cp_msg1;	      
    __u8 cp_msg2;
    __u8 cp_msg3;
    __u8 cp_cdb[12];	      
    __u32 cp_datalen;	      
    void *cp_viraddr;	      
    __u32 cp_dataDMA;	      
    __u32 cp_statDMA;	       
    __u32 cp_reqDMA;	      
    
    __u32 timestamp;	      
    __u32 timeout;
    __u8 sizeindex;
    __u8 rw_latency;
    __u8 retries;
    __u8 status;	      
    struct scsi_cmnd *cmd;    
    struct eata_sg_list *sg_list;
};


struct eata_sp {
    __u8 hba_stat:7,	      
	      EOC:1;	      
    __u8 scsi_stat;	      
    __u8 reserved[2];
    __u32  residue_len;	      
    struct eata_ccb *ccb;     
    __u8 msg[12];
};

typedef struct hstd {
    __u8   vendor[9];
    __u8   name[18];
    __u8   revision[6];
    __u8   EATA_revision;
    __u32  firmware_revision;
    __u8   HBA_number;
    __u8   bustype;		 
    __u8   channel;		 
    __u8   state;		 
    __u8   primary;		 
    __u8        more_support:1,  
           immediate_support:1,  
              broken_INQUIRY:1;	 
    __u8   do_latency;		 
    __u32  reads[13];
    __u32  writes[13];
    __u32  reads_lat[12][4];
    __u32  writes_lat[12][4];
    __u32  all_lat[4];
    __u8   resetlevel[MAXCHANNEL]; 
    __u32  last_ccb;		 
    __u32  cplen;		 
    __u16  cppadlen;		 
    __u16  queuesize;
    __u16  sgsize;               
    __u16  devflags;		 
    __u8   hostid;		 
    __u8   moresupport;		 
    struct Scsi_Host *next;	    
    struct Scsi_Host *prev;
    struct pci_dev *pdev;	
    struct eata_sp sp;		  
    struct eata_ccb ccb[0];	 
}hostdata;

struct drive_geom_emul {
    __u8  trans;		 
    __u8  channel;		 
    __u8  HBA;			 
    __u8  id;			 
    __u8  lun;			 
    __u32 heads;		 
    __u32 sectors;		 
    __u32 cylinder;		 
};

struct geom_emul {
    __u8 bios_drives;		 
    struct drive_geom_emul drv[2]; 
};

#endif 

