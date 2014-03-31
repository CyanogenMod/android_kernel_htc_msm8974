#ifndef DC395x_H
#define DC395x_H

#define DC395x_MAX_CMD_QUEUE		32
#define DC395x_MAX_QTAGS		16
#define DC395x_MAX_SCSI_ID		16
#define DC395x_MAX_CMD_PER_LUN		DC395x_MAX_QTAGS
#define DC395x_MAX_SG_TABLESIZE		64	
#define DC395x_MAX_SG_LISTENTRY		64	
						
#define DC395x_MAX_SRB_CNT		63
#define DC395x_MAX_CAN_QUEUE		DC395x_MAX_SRB_CNT
#define DC395x_END_SCAN			2
#define DC395x_SEL_TIMEOUT		153	
#define DC395x_MAX_RETRIES		3

#if 0
#define SYNC_FIRST
#endif

#define NORM_REC_LVL			0

#define BIT31				0x80000000
#define BIT30				0x40000000
#define BIT29				0x20000000
#define BIT28				0x10000000
#define BIT27				0x08000000
#define BIT26				0x04000000
#define BIT25				0x02000000
#define BIT24				0x01000000
#define BIT23				0x00800000
#define BIT22				0x00400000
#define BIT21				0x00200000
#define BIT20				0x00100000
#define BIT19				0x00080000
#define BIT18				0x00040000
#define BIT17				0x00020000
#define BIT16				0x00010000
#define BIT15				0x00008000
#define BIT14				0x00004000
#define BIT13				0x00002000
#define BIT12				0x00001000
#define BIT11				0x00000800
#define BIT10				0x00000400
#define BIT9				0x00000200
#define BIT8				0x00000100
#define BIT7				0x00000080
#define BIT6				0x00000040
#define BIT5				0x00000020
#define BIT4				0x00000010
#define BIT3				0x00000008
#define BIT2				0x00000004
#define BIT1				0x00000002
#define BIT0				0x00000001

#define UNIT_ALLOCATED			BIT0
#define UNIT_INFO_CHANGED		BIT1
#define FORMATING_MEDIA			BIT2
#define UNIT_RETRY			BIT3

#define DASD_SUPPORT			BIT0
#define SCSI_SUPPORT			BIT1
#define ASPI_SUPPORT			BIT2

#define SRB_FREE			0x0000
#define SRB_WAIT			0x0001
#define SRB_READY			0x0002
#define SRB_MSGOUT			0x0004	
#define SRB_MSGIN			0x0008
#define SRB_EXTEND_MSGIN		0x0010
#define SRB_COMMAND			0x0020
#define SRB_START_			0x0040	
#define SRB_DISCONNECT			0x0080
#define SRB_DATA_XFER			0x0100
#define SRB_XFERPAD			0x0200
#define SRB_STATUS			0x0400
#define SRB_COMPLETED			0x0800
#define SRB_ABORT_SENT			0x1000
#define SRB_DO_SYNC_NEGO		0x2000
#define SRB_DO_WIDE_NEGO		0x4000
#define SRB_UNEXPECT_RESEL		0x8000

#define HCC_WIDE_CARD			0x20
#define HCC_SCSI_RESET			0x10
#define HCC_PARITY			0x08
#define HCC_AUTOTERM			0x04
#define HCC_LOW8TERM			0x02
#define HCC_UP8TERM			0x01

#define RESET_DEV			BIT0
#define RESET_DETECT			BIT1
#define RESET_DONE			BIT2

#define ABORT_DEV_			BIT0

#define SRB_OK				BIT0
#define ABORTION			BIT1
#define OVER_RUN			BIT2
#define UNDER_RUN			BIT3
#define PARITY_ERROR			BIT4
#define SRB_ERROR			BIT5

#define DATAOUT				BIT7
#define DATAIN				BIT6
#define RESIDUAL_VALID			BIT5
#define ENABLE_TIMER			BIT4
#define RESET_DEV0			BIT2
#define ABORT_DEV			BIT1
#define AUTO_REQSENSE			BIT0

#define H_STATUS_GOOD			0
#define H_SEL_TIMEOUT			0x11
#define H_OVER_UNDER_RUN		0x12
#define H_UNEXP_BUS_FREE		0x13
#define H_TARGET_PHASE_F		0x14
#define H_INVALID_CCB_OP		0x16
#define H_LINK_CCB_BAD			0x17
#define H_BAD_TARGET_DIR		0x18
#define H_DUPLICATE_CCB			0x19
#define H_BAD_CCB_OR_SG			0x1A
#define H_ABORT				0x0FF

#define SCSI_STAT_GOOD			0x0	
#define SCSI_STAT_CHECKCOND		0x02	
#define SCSI_STAT_CONDMET		0x04	
#define SCSI_STAT_BUSY			0x08	
#define SCSI_STAT_INTER			0x10	
#define SCSI_STAT_INTERCONDMET		0x14	
#define SCSI_STAT_RESCONFLICT		0x18	
#define SCSI_STAT_CMDTERM		0x22	
#define SCSI_STAT_QUEUEFULL		0x28	
#define SCSI_STAT_UNEXP_BUS_F		0xFD	
#define SCSI_STAT_BUS_RST_DETECT	0xFE	
#define SCSI_STAT_SEL_TIMEOUT		0xFF	

#define SYNC_WIDE_TAG_ATNT_DISABLE	0
#define SYNC_NEGO_ENABLE		BIT0
#define SYNC_NEGO_DONE			BIT1
#define WIDE_NEGO_ENABLE		BIT2
#define WIDE_NEGO_DONE			BIT3
#define WIDE_NEGO_STATE			BIT4
#define EN_TAG_QUEUEING			BIT5
#define EN_ATN_STOP			BIT6

#define SYNC_NEGO_OFFSET		15

#define MSG_COMPLETE			0x00
#define MSG_EXTENDED			0x01
#define MSG_SAVE_PTR			0x02
#define MSG_RESTORE_PTR			0x03
#define MSG_DISCONNECT			0x04
#define MSG_INITIATOR_ERROR		0x05
#define MSG_ABORT			0x06
#define MSG_REJECT_			0x07
#define MSG_NOP				0x08
#define MSG_PARITY_ERROR		0x09
#define MSG_LINK_CMD_COMPL		0x0A
#define MSG_LINK_CMD_COMPL_FLG		0x0B
#define MSG_BUS_RESET			0x0C
#define MSG_ABORT_TAG			0x0D
#define MSG_SIMPLE_QTAG			0x20
#define MSG_HEAD_QTAG			0x21
#define MSG_ORDER_QTAG			0x22
#define MSG_IGNOREWIDE			0x23
#define MSG_IDENTIFY			0x80
#define MSG_HOST_ID			0xC0

#define STATUS_GOOD			0x00
#define CHECK_CONDITION_		0x02
#define STATUS_BUSY			0x08
#define STATUS_INTERMEDIATE		0x10
#define RESERVE_CONFLICT		0x18

#define STATUS_MASK_			0xFF
#define MSG_MASK			0xFF00
#define RETURN_MASK			0xFF0000

struct ScsiInqData
{						
	u8 DevType;				
	u8 RMB_TypeMod;				
	u8 Vers;				
	u8 RDF;					
	u8 AddLen;				
	u8 Res1;				
	u8 Res2;				
	u8 Flags;				
	u8 VendorID[8];				
	u8 ProductID[16];			
	u8 ProductRev[4];			
};

						
#define SCSI_DEVTYPE			0x1F	
#define SCSI_PERIPHQUAL			0xE0	
						
#define SCSI_REMOVABLE_MEDIA		0x80	
						
						
#define TYPE_NODEV		SCSI_DEVTYPE	
#ifndef TYPE_PRINTER				
# define TYPE_PRINTER			0x02	
#endif						
#ifndef TYPE_COMM				
# define TYPE_COMM			0x09	
#endif

#define SCSI_INQ_RELADR			0x80	
#define SCSI_INQ_WBUS32			0x40	
#define SCSI_INQ_WBUS16			0x20	
#define SCSI_INQ_SYNC			0x10	
#define SCSI_INQ_LINKED			0x08	
#define SCSI_INQ_CMDQUEUE		0x02	
#define SCSI_INQ_SFTRE			0x01	

#define ENABLE_CE			1
#define DISABLE_CE			0
#define EEPROM_READ			0x80

#define TRM_S1040_ID			0x00	
#define TRM_S1040_COMMAND		0x04	
#define TRM_S1040_IOBASE		0x10	
#define TRM_S1040_ROMBASE		0x30	
#define TRM_S1040_INTLINE		0x3C	

#define TRM_S1040_SCSI_STATUS		0x80	
#define COMMANDPHASEDONE		0x2000	
#define SCSIXFERDONE			0x0800	
#define SCSIXFERCNT_2_ZERO		0x0100	
#define SCSIINTERRUPT			0x0080	
#define COMMANDABORT			0x0040	
#define SEQUENCERACTIVE			0x0020	
#define PHASEMISMATCH			0x0010	
#define PARITYERROR			0x0008	

#define PHASEMASK			0x0007	
#define PH_DATA_OUT			0x00	
#define PH_DATA_IN			0x01	
#define PH_COMMAND			0x02	
#define PH_STATUS			0x03	
#define PH_BUS_FREE			0x05	
#define PH_MSG_OUT			0x06	
#define PH_MSG_IN			0x07	

#define TRM_S1040_SCSI_CONTROL		0x80	
#define DO_CLRATN			0x0400	
#define DO_SETATN			0x0200	
#define DO_CMDABORT			0x0100	
#define DO_RSTMODULE			0x0010	
#define DO_RSTSCSI			0x0008	
#define DO_CLRFIFO			0x0004	
#define DO_DATALATCH			0x0002	
	
#define DO_HWRESELECT			0x0001	

#define TRM_S1040_SCSI_FIFOCNT		0x82	
#define TRM_S1040_SCSI_SIGNAL		0x83	

#define TRM_S1040_SCSI_INTSTATUS	0x84	
#define INT_SCAM			0x80	
#define INT_SELECT			0x40	
#define INT_SELTIMEOUT			0x20	
#define INT_DISCONNECT			0x10	
#define INT_RESELECTED			0x08	
#define INT_SCSIRESET			0x04	
#define INT_BUSSERVICE			0x02	
#define INT_CMDDONE			0x01	

#define TRM_S1040_SCSI_OFFSET		0x84	


#define TRM_S1040_SCSI_SYNC		0x85	
#define LVDS_SYNC			0x20	
#define WIDE_SYNC			0x10	
#define ALT_SYNC			0x08	


#define TRM_S1040_SCSI_TARGETID		0x86	
#define TRM_S1040_SCSI_IDMSG		0x87	
#define TRM_S1040_SCSI_HOSTID		0x87	
#define TRM_S1040_SCSI_COUNTER		0x88	

#define TRM_S1040_SCSI_INTEN		0x8C	
#define EN_SCAM				0x80	
#define EN_SELECT			0x40	
#define EN_SELTIMEOUT			0x20	
#define EN_DISCONNECT			0x10	
#define EN_RESELECTED			0x08	
#define EN_SCSIRESET			0x04	
#define EN_BUSSERVICE			0x02	
#define EN_CMDDONE			0x01	

#define TRM_S1040_SCSI_CONFIG0		0x8D	
#define PHASELATCH			0x40	
#define INITIATOR			0x20	
#define PARITYCHECK			0x10	
#define BLOCKRST			0x01	

#define TRM_S1040_SCSI_CONFIG1		0x8E	
#define ACTIVE_NEGPLUS			0x10	
#define FILTER_DISABLE			0x08	
#define FAST_FILTER			0x04	
#define ACTIVE_NEG			0x02	

#define TRM_S1040_SCSI_CONFIG2		0x8F	
#define CFG2_WIDEFIFO			0x02	

#define TRM_S1040_SCSI_COMMAND		0x90	
#define SCMD_COMP			0x12	
#define SCMD_SEL_ATN			0x60	
#define SCMD_SEL_ATN3			0x64	
#define SCMD_SEL_ATNSTOP		0xB8	
#define SCMD_FIFO_OUT			0xC0	
#define SCMD_DMA_OUT			0xC1	
#define SCMD_FIFO_IN			0xC2	
#define SCMD_DMA_IN			0xC3	
#define SCMD_MSGACCEPT			0xD8	


#define TRM_S1040_SCSI_TIMEOUT		0x91	
#define TRM_S1040_SCSI_FIFO		0x98	

#define TRM_S1040_SCSI_TCR0		0x9C	
#define TCR0_WIDE_NEGO_DONE		0x8000	
#define TCR0_SYNC_NEGO_DONE		0x4000	
#define TCR0_ENABLE_LVDS		0x2000	
#define TCR0_ENABLE_WIDE		0x1000	
#define TCR0_ENABLE_ALT			0x0800	
#define TCR0_PERIOD_MASK		0x0700	

#define TCR0_DO_WIDE_NEGO		0x0080	
#define TCR0_DO_SYNC_NEGO		0x0040	
#define TCR0_DISCONNECT_EN		0x0020	
#define TCR0_OFFSET_MASK		0x001F	

#define TRM_S1040_SCSI_TCR1		0x9E	
#define MAXTAG_MASK			0x7F00	
#define NON_TAG_BUSY			0x0080	
#define ACTTAG_MASK			0x007F	

#define TRM_S1040_DMA_COMMAND		0xA0	
#define DMACMD_SG			0x02	
#define DMACMD_DIR			0x01	
#define XFERDATAIN_SG			0x0103	
#define XFERDATAOUT_SG			0x0102	
#define XFERDATAIN			0x0101	
#define XFERDATAOUT			0x0100	

#define TRM_S1040_DMA_FIFOCNT		0xA1	

#define TRM_S1040_DMA_CONTROL		0xA1	
#define DMARESETMODULE			0x10	
#define STOPDMAXFER			0x08	
#define ABORTXFER			0x04	
#define CLRXFIFO			0x02	
#define STARTDMAXFER			0x01	

#define TRM_S1040_DMA_FIFOSTAT		0xA2	

#define TRM_S1040_DMA_STATUS		0xA3	
#define XFERPENDING			0x80	
#define SCSIBUSY			0x40	
#define GLOBALINT			0x20	
#define FORCEDMACOMP			0x10	
#define DMAXFERERROR			0x08	
#define DMAXFERABORT			0x04	
#define DMAXFERCOMP			0x02	
#define SCSICOMP			0x01	

#define TRM_S1040_DMA_INTEN		0xA4	
#define EN_FORCEDMACOMP			0x10	
#define EN_DMAXFERERROR			0x08	
#define EN_DMAXFERABORT			0x04	
#define EN_DMAXFERCOMP			0x02	
#define EN_SCSIINTR			0x01	

#define TRM_S1040_DMA_CONFIG		0xA6	
#define DMA_ENHANCE			0x8000	
#define DMA_PCI_DUAL_ADDR		0x4000	
#define DMA_CFG_RES			0x2000	
#define DMA_AUTO_CLR_FIFO		0x1000	
#define DMA_MEM_MULTI_READ		0x0800	
#define DMA_MEM_WRITE_INVAL		0x0400	
#define DMA_FIFO_CTRL			0x0300	
#define DMA_FIFO_HALF_HALF		0x0200	

#define TRM_S1040_DMA_XCNT		0xA8	
#define TRM_S1040_DMA_CXCNT		0xAC	
#define TRM_S1040_DMA_XLOWADDR		0xB0	
#define TRM_S1040_DMA_XHIGHADDR		0xB4	

#define TRM_S1040_GEN_CONTROL		0xD4	
#define CTRL_LED			0x80	
#define EN_EEPROM			0x10	
#define DIS_TERM			0x08	
#define AUTOTERM			0x04	
#define LOW8TERM			0x02	
#define UP8TERM				0x01	

#define TRM_S1040_GEN_STATUS		0xD5	
#define GTIMEOUT			0x80	
#define EXT68HIGH			0x40	
#define INT68HIGH			0x20	
#define CON5068				0x10	
#define CON68				0x08	
#define CON50				0x04	
#define WIDESCSI			0x02	
#define STATUS_LOAD_DEFAULT		0x01	

#define TRM_S1040_GEN_NVRAM		0xD6	
#define NVR_BITOUT			0x08	
#define NVR_BITIN			0x04	
#define NVR_CLOCK			0x02	
#define NVR_SELECT			0x01	

#define TRM_S1040_GEN_EDATA		0xD7	
#define TRM_S1040_GEN_EADDRESS		0xD8	
#define TRM_S1040_GEN_TIMER		0xDB	

#define NTC_DO_WIDE_NEGO		0x20	
#define NTC_DO_TAG_QUEUEING		0x10	
#define NTC_DO_SEND_START		0x08	
#define NTC_DO_DISCONNECT		0x04	
#define NTC_DO_SYNC_NEGO		0x02	
#define NTC_DO_PARITY_CHK		0x01	
						

#if 0
#define MORE2_DRV			BIT0
#define GREATER_1G			BIT1
#define RST_SCSI_BUS			BIT2
#define ACTIVE_NEGATION			BIT3
#define NO_SEEK				BIT4
#define LUN_CHECK			BIT5
#endif

#define NAC_SCANLUN			0x20	
#define NAC_POWERON_SCSI_RESET		0x04	
#define NAC_GREATER_1G			0x02	
#define NAC_GT2DRIVES			0x01	
	

#endif
