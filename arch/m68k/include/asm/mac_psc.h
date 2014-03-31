
#define PSC_BASE	(0x50F31000)


#define pIFRbase	0x100
#define pIERbase	0x104


#define PSC_MYSTERY	0x804

#define PSC_CTL_BASE	0xC00

#define PSC_SCSI_CTL	0xC00
#define PSC_ENETRD_CTL  0xC10
#define PSC_ENETWR_CTL  0xC20
#define PSC_FDC_CTL	0xC30
#define PSC_SCCA_CTL	0xC40
#define PSC_SCCB_CTL	0xC50
#define PSC_SCCATX_CTL	0xC60


#define PSC_ADDR_BASE	0x1000
#define PSC_LEN_BASE	0x1004
#define PSC_CMD_BASE	0x1008

#define PSC_SET0	0x00
#define PSC_SET1	0x10

#define PSC_SCSI_ADDR	0x1000	
#define PSC_SCSI_LEN	0x1004	
#define PSC_SCSI_CMD	0x1008	
#define PSC_ENETRD_ADDR 0x1020	
#define PSC_ENETRD_LEN  0x1024	
#define PSC_ENETRD_CMD  0x1028	
#define PSC_ENETWR_ADDR 0x1040	
#define PSC_ENETWR_LEN  0x1044	
#define PSC_ENETWR_CMD  0x1048	
#define PSC_FDC_ADDR	0x1060	
#define PSC_FDC_LEN	0x1064	
#define PSC_FDC_CMD	0x1068	
#define PSC_SCCA_ADDR	0x1080	
#define PSC_SCCA_LEN	0x1084	
#define PSC_SCCA_CMD	0x1088	
#define PSC_SCCB_ADDR	0x10A0	
#define PSC_SCCB_LEN	0x10A4	
#define PSC_SCCB_CMD	0x10A8	
#define PSC_SCCATX_ADDR	0x10C0	
#define PSC_SCCATX_LEN	0x10C4	
#define PSC_SCCATX_CMD	0x10C8	


#define PSC_SND_CTL	0x200	


#define PSC_SND_SOURCE	0x204	
#define PSC_SND_STATUS1	0x208	
#define PSC_SND_HUH3	0x20C	
#define PSC_SND_BITS2GO	0x20E	
#define PSC_SND_INADDR	0x210	
#define PSC_SND_OUTADDR	0x214	
#define PSC_SND_LEN	0x218	
#define PSC_SND_HUH4	0x21A	
#define PSC_SND_STATUS2	0x21C	
#define PSC_SND_HUH5	0x21E	

#ifndef __ASSEMBLY__

extern volatile __u8 *psc;
extern int psc_present;

extern void psc_register_interrupts(void);
extern void psc_irq_enable(int);
extern void psc_irq_disable(int);


static inline void psc_write_byte(int offset, __u8 data)
{
	*((volatile __u8 *)(psc + offset)) = data;
}

static inline void psc_write_word(int offset, __u16 data)
{
	*((volatile __u16 *)(psc + offset)) = data;
}

static inline void psc_write_long(int offset, __u32 data)
{
	*((volatile __u32 *)(psc + offset)) = data;
}

static inline u8 psc_read_byte(int offset)
{
	return *((volatile __u8 *)(psc + offset));
}

static inline u16 psc_read_word(int offset)
{
	return *((volatile __u16 *)(psc + offset));
}

static inline u32 psc_read_long(int offset)
{
	return *((volatile __u32 *)(psc + offset));
}

#endif 
