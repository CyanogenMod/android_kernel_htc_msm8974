/*
 * Common structures and definitions for FT1000 Flarion Flash OFDM PCMCIA and USB devices
 *
 * Originally copyright (c) 2002 Flarion Technologies
 *
 */

#define DSPVERSZ	4
#define HWSERNUMSZ	16
#define SKUSZ		20
#define EUISZ		8
#define MODESZ		2
#define CALVERSZ	2
#define CALDATESZ	6

#define ELECTRABUZZ_ID	0	
#define MAGNEMITE_ID	0x1a01	

#define	FT1000_REG_DPRAM_ADDR	0x000E	
#define	FT1000_REG_SUP_CTRL	0x0020	
#define	FT1000_REG_SUP_STAT	0x0022	
#define	FT1000_REG_RESET	0x0024	
#define	FT1000_REG_SUP_ISR	0x0026	
#define	FT1000_REG_SUP_IMASK	0x0028	
#define	FT1000_REG_DOORBELL	0x002a	
#define FT1000_REG_ASIC_ID	0x002e	

#define FT1000_REG_UFIFO_STAT	0x0000	
#define FT1000_REG_UFIFO_BEG	0x0002	
#define	FT1000_REG_UFIFO_MID	0x0004	
#define	FT1000_REG_UFIFO_END	0x0006	
#define	FT1000_REG_DFIFO_STAT	0x0008	
#define	FT1000_REG_DFIFO	0x000A	
#define	FT1000_REG_DPRAM_DATA	0x000C	
#define	FT1000_REG_WATERMARK	0x0010	

#define FT1000_REG_MAG_UFDR	0x0000	
#define FT1000_REG_MAG_UFDRL	0x0000	
#define FT1000_REG_MAG_UFDRH	0x0002	
#define FT1000_REG_MAG_UFER	0x0004	
#define FT1000_REG_MAG_UFSR	0x0006	
#define FT1000_REG_MAG_DFR	0x0008	
#define FT1000_REG_MAG_DFRL	0x0008	
#define FT1000_REG_MAG_DFRH	0x000a	
#define FT1000_REG_MAG_DFSR	0x000c	
#define FT1000_REG_MAG_DPDATA	0x0010	
#define FT1000_REG_MAG_DPDATAL	0x0010	
#define FT1000_REG_MAG_DPDATAH	0x0012	
#define	FT1000_REG_MAG_WATERMARK 0x002c	
#define FT1000_REG_MAG_VERSION	0x0030	

#define FT1000_DPRAM_TX_BASE	0x0002	
#define FT1000_DPRAM_RX_BASE	0x0800	
#define FT1000_FIFO_LEN		0x07FC	
#define FT1000_HI_HO		0x07FE	
#define FT1000_DSP_STATUS	0x0FFE	
#define FT1000_DSP_LED		0x0FFA	
#define FT1000_DSP_CON_STATE	0x0FF8	
#define FT1000_DPRAM_FEFE	0x0002	
#define FT1000_DSP_TIMER0	0x1FF0	
#define FT1000_DSP_TIMER1	0x1FF2	
#define FT1000_DSP_TIMER2	0x1FF4	
#define FT1000_DSP_TIMER3	0x1FF6	

#define FT1000_DPRAM_MAG_TX_BASE	0x0000	
#define FT1000_DPRAM_MAG_RX_BASE	0x0200	

#define FT1000_MAG_FIFO_LEN		0x1FF	
#define FT1000_MAG_FIFO_LEN_INDX	0x1	
#define FT1000_MAG_HI_HO		0x1FF	
#define FT1000_MAG_HI_HO_INDX		0x0	
#define FT1000_MAG_DSP_LED		0x3FE	
#define FT1000_MAG_DSP_LED_INDX		0x0	
#define FT1000_MAG_DSP_CON_STATE	0x3FE	
#define FT1000_MAG_DSP_CON_STATE_INDX	0x1	
#define FT1000_MAG_DPRAM_FEFE		0x000	
#define FT1000_MAG_DPRAM_FEFE_INDX	0x0	
#define FT1000_MAG_DSP_TIMER0		0x3FC	
#define FT1000_MAG_DSP_TIMER0_INDX	0x1
#define FT1000_MAG_DSP_TIMER1		0x3FC	
#define FT1000_MAG_DSP_TIMER1_INDX	0x0
#define FT1000_MAG_DSP_TIMER2		0x3FD	
#define FT1000_MAG_DSP_TIMER2_INDX	0x1
#define FT1000_MAG_DSP_TIMER3		0x3FD	
#define FT1000_MAG_DSP_TIMER3_INDX	0x0
#define FT1000_MAG_TOTAL_LEN		0x200
#define FT1000_MAG_TOTAL_LEN_INDX	0x1
#define FT1000_MAG_PH_LEN		0x200
#define FT1000_MAG_PH_LEN_INDX		0x0
#define FT1000_MAG_PORT_ID		0x201
#define FT1000_MAG_PORT_ID_INDX		0x0

#define HOST_INTF_LE	0x0	
#define HOST_INTF_BE	0x1	

#define FT1000_DB_DPRAM_RX	0x0001	
#define FT1000_DB_DNLD_RX	0x0002	
#define FT1000_ASIC_RESET_REQ	0x0004	
#define FT1000_DSP_ASIC_RESET	0x0008	
#define FT1000_DB_COND_RESET	0x0010	

#define FT1000_DB_DPRAM_TX	0x0100	
#define FT1000_DB_DNLD_TX	0x0200	
#define FT1000_ASIC_RESET_DSP	0x0400	
#define FT1000_DB_HB		0x1000	

#define hi			0x6869	
#define ho			0x686f	

#define hi_mag			0x6968	
#define ho_mag			0x6f68	

#define ISR_EMPTY		0x00	
#define ISR_DOORBELL_ACK	0x01	
#define ISR_DOORBELL_PEND	0x02	
#define ISR_RCV			0x04	
#define ISR_WATERMARK		0x08	

#define ISR_MASK_NONE		0x0000	
#define ISR_MASK_DOORBELL_ACK	0x0001	
#define ISR_MASK_DOORBELL_PEND	0x0002	
#define ISR_MASK_RCV		0x0004	
#define ISR_MASK_WATERMARK	0x0008	
#define ISR_MASK_ALL		0xffff	
#define ISR_DEFAULT_MASK	0x7ff9

#define DSP_RESET_BIT		0x0001	
					
#define ASIC_RESET_BIT		0x0002	
					
#define DSP_UNENCRYPTED		0x0004
#define DSP_ENCRYPTED		0x0008
#define EFUSE_MEM_DISABLE	0x0040

#define DSPID		0x20
#define HOSTID		0x10
#define DSPAIRID	0x90
#define DRIVERID	0x00
#define NETWORKID	0x20

#define MAX_CMD_SQSIZE	1780

#define ENET_MAX_SIZE		1514
#define ENET_HEADER_SIZE	14

#define SLOWQ_TYPE	0
#define FASTQ_TYPE	1

#define MAX_DSP_SESS_REC	1024

#define DSP_QID_OFFSET	4

#define MEDIA_STATE		0x0010
#define TIME_UPDATE		0x0020
#define DSP_PROVISION		0x0030
#define DSP_INIT_MSG		0x0050
#define DSP_HIBERNATE		0x0060
#define DSP_STORE_INFO		0x0070
#define DSP_GET_INFO		0x0071
#define GET_DRV_ERR_RPT_MSG	0x0073
#define RSP_DRV_ERR_RPT_MSG	0x0074

#define DSP_HB_INFO		0x7ef0
#define DSP_FIFO_INFO		0x7ef1
#define DSP_CONDRESET_INFO	0x7ef2
#define DSP_CMDLEN_INFO		0x7ef3
#define DSP_CMDPHCKSUM_INFO	0x7ef4
#define DSP_PKTPHCKSUM_INFO	0x7ef5
#define DSP_PKTLEN_INFO		0x7ef6
#define DSP_USER_RESET		0x7ef7
#define FIFO_FLUSH_MAXLIMIT	0x7ef8
#define FIFO_FLUSH_BADCNT	0x7ef9
#define FIFO_ZERO_LEN		0x7efa

struct pseudo_hdr {
	unsigned short	length;		
	unsigned char	source;		
					
					
	unsigned char	destination;	
	unsigned char	portdest;	
					
					
					
					
					
					
					
	unsigned char	portsrc;	
	unsigned short	sh_str_id;	
	unsigned char	control;	
	unsigned char	rsvd1;
	unsigned char	seq_num;	
	unsigned char	rsvd2;
	unsigned short	qos_class;	
	unsigned short	checksum;	
} __packed;

struct drv_msg {
	struct pseudo_hdr pseudo;
	u16 type;
	u16 length;
	u8  data[0];
} __packed;

struct media_msg {
	struct pseudo_hdr pseudo;
	u16 type;
	u16 length;
	u16 state;
	u32 ip_addr;
	u32 net_mask;
	u32 gateway;
	u32 dns_1;
	u32 dns_2;
} __packed;

struct dsp_init_msg {
	struct pseudo_hdr pseudo;
	u16 type;
	u16 length;
	u8 DspVer[DSPVERSZ];		
	u8 HwSerNum[HWSERNUMSZ];	
	u8 Sku[SKUSZ];			
	u8 eui64[EUISZ];		
	u8 ProductMode[MODESZ];		
	u8 RfCalVer[CALVERSZ];		
	u8 RfCalDate[CALDATESZ];	
} __packed;

struct prov_record {
	struct list_head list;
	u8 *pprov_data;
};
