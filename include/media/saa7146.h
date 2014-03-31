#ifndef __SAA7146__
#define __SAA7146__

#include <linux/delay.h>	
#include <linux/slab.h>		
#include <linux/pci.h>		
#include <linux/init.h>		
#include <linux/interrupt.h>	
#include <linux/kmod.h>		
#include <linux/i2c.h>		
#include <asm/io.h>		
#include <linux/stringify.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <media/v4l2-device.h>

#include <linux/vmalloc.h>	
#include <linux/mm.h>		

#define SAA7146_VERSION_CODE 0x000600	

#define saa7146_write(sxy,adr,dat)    writel((dat),(sxy->mem+(adr)))
#define saa7146_read(sxy,adr)         readl(sxy->mem+(adr))

extern unsigned int saa7146_debug;

#ifndef DEBUG_VARIABLE
	#define DEBUG_VARIABLE saa7146_debug
#endif

#define ERR(fmt, ...)	pr_err("%s: " fmt, __func__, ##__VA_ARGS__)

#define _DBG(mask, fmt, ...)						\
do {									\
	if (DEBUG_VARIABLE & mask)					\
		pr_debug("%s(): " fmt, __func__, ##__VA_ARGS__);	\
} while (0)

#define DEB_S(fmt, ...)		_DBG(0x01, fmt, ##__VA_ARGS__)
#define DEB_D(fmt, ...)		_DBG(0x02, fmt, ##__VA_ARGS__)
#define DEB_EE(fmt, ...)	_DBG(0x04, fmt, ##__VA_ARGS__)
#define DEB_I2C(fmt, ...)	_DBG(0x08, fmt, ##__VA_ARGS__)
#define DEB_VBI(fmt, ...)	_DBG(0x10, fmt, ##__VA_ARGS__)
#define DEB_INT(fmt, ...)	_DBG(0x20, fmt, ##__VA_ARGS__)
#define DEB_CAP(fmt, ...)	_DBG(0x40, fmt, ##__VA_ARGS__)

#define SAA7146_ISR_CLEAR(x,y) \
	saa7146_write(x, ISR, (y));

struct module;

struct saa7146_dev;
struct saa7146_extension;
struct saa7146_vv;

struct saa7146_pgtable {
	unsigned int	size;
	__le32		*cpu;
	dma_addr_t	dma;
	
	unsigned long	offset;
	
	struct scatterlist *slist;
	int		nents;
};

struct saa7146_pci_extension_data {
	struct saa7146_extension *ext;
	void *ext_priv;			
};

#define MAKE_EXTENSION_PCI(x_var, x_vendor, x_device)		\
	{							\
		.vendor    = PCI_VENDOR_ID_PHILIPS,		\
		.device	   = PCI_DEVICE_ID_PHILIPS_SAA7146,	\
		.subvendor = x_vendor,				\
		.subdevice = x_device,				\
		.driver_data = (unsigned long)& x_var,		\
	}

struct saa7146_extension
{
	char	name[32];		
#define SAA7146_USE_I2C_IRQ	0x1
#define SAA7146_I2C_SHORT_DELAY	0x2
	int	flags;

	struct module *module;
	struct pci_driver driver;
	struct pci_device_id *pci_tbl;

	
	int (*probe)(struct saa7146_dev *);
	int (*attach)(struct saa7146_dev *, struct saa7146_pci_extension_data *);
	int (*detach)(struct saa7146_dev*);

	u32	irq_mask;	
	void	(*irq_func)(struct saa7146_dev*, u32* irq_mask);
};

struct saa7146_dma
{
	dma_addr_t	dma_handle;
	__le32		*cpu_addr;
};

struct saa7146_dev
{
	struct module			*module;

	struct list_head		item;

	struct v4l2_device 		v4l2_dev;

	
	spinlock_t			slock;
	struct mutex			v4l2_lock;

	unsigned char			__iomem *mem;		
	u32				revision;	

	
	char				name[32];
	struct pci_dev			*pci;
	u32				int_todo;
	spinlock_t			int_slock;

	
	struct saa7146_extension	*ext;		
	void				*ext_priv;	
	struct saa7146_ext_vv		*ext_vv_data;

	
	struct saa7146_vv	*vv_data;
	void (*vv_callback)(struct saa7146_dev *dev, unsigned long status);

	
	struct mutex			i2c_lock;

	u32				i2c_bitrate;
	struct saa7146_dma		d_i2c;	
	wait_queue_head_t		i2c_wq;
	int				i2c_op;

	
	struct saa7146_dma		d_rps0;
	struct saa7146_dma		d_rps1;
};

static inline struct saa7146_dev *to_saa7146_dev(struct v4l2_device *v4l2_dev)
{
	return container_of(v4l2_dev, struct saa7146_dev, v4l2_dev);
}

int saa7146_i2c_adapter_prepare(struct saa7146_dev *dev, struct i2c_adapter *i2c_adapter, u32 bitrate);

extern struct list_head saa7146_devices;
extern struct mutex saa7146_devices_lock;
int saa7146_register_extension(struct saa7146_extension*);
int saa7146_unregister_extension(struct saa7146_extension*);
struct saa7146_format* saa7146_format_by_fourcc(struct saa7146_dev *dev, int fourcc);
int saa7146_pgtable_alloc(struct pci_dev *pci, struct saa7146_pgtable *pt);
void saa7146_pgtable_free(struct pci_dev *pci, struct saa7146_pgtable *pt);
int saa7146_pgtable_build_single(struct pci_dev *pci, struct saa7146_pgtable *pt, struct scatterlist *list, int length );
void *saa7146_vmalloc_build_pgtable(struct pci_dev *pci, long length, struct saa7146_pgtable *pt);
void saa7146_vfree_destroy_pgtable(struct pci_dev *pci, void *mem, struct saa7146_pgtable *pt);
void saa7146_setgpio(struct saa7146_dev *dev, int port, u32 data);
int saa7146_wait_for_debi_done(struct saa7146_dev *dev, int nobusyloop);

#define SAA7146_I2C_MEM		( 1*PAGE_SIZE)
#define SAA7146_RPS_MEM		( 1*PAGE_SIZE)

#define SAA7146_I2C_TIMEOUT	100	
#define SAA7146_I2C_RETRIES	3	
#define SAA7146_I2C_DELAY	5	

#define ME1    0x0000000800
#define PV1    0x0000000008

#define SAA7146_GPIO_INPUT 0x00
#define SAA7146_GPIO_IRQHI 0x10
#define SAA7146_GPIO_IRQLO 0x20
#define SAA7146_GPIO_IRQHL 0x30
#define SAA7146_GPIO_OUTLO 0x40
#define SAA7146_GPIO_OUTHI 0x50

#define DEBINOSWAP 0x000e0000

#define CMD_NOP		0x00000000  
#define CMD_CLR_EVENT	0x00000000  
#define CMD_SET_EVENT	0x10000000  
#define CMD_PAUSE	0x20000000  
#define CMD_CHECK_LATE	0x30000000  
#define CMD_UPLOAD	0x40000000  
#define CMD_STOP	0x50000000  
#define CMD_INTERRUPT	0x60000000  
#define CMD_JUMP	0x80000000  
#define CMD_WR_REG	0x90000000  
#define CMD_RD_REG	0xa0000000  
#define CMD_WR_REG_MASK	0xc0000000  

#define CMD_OAN		MASK_27
#define CMD_INV		MASK_26
#define CMD_SIG4	MASK_25
#define CMD_SIG3	MASK_24
#define CMD_SIG2	MASK_23
#define CMD_SIG1	MASK_22
#define CMD_SIG0	MASK_21
#define CMD_O_FID_B	MASK_14
#define CMD_E_FID_B	MASK_13
#define CMD_O_FID_A	MASK_12
#define CMD_E_FID_A	MASK_11

#define EVT_HS          (1<<15)     
#define EVT_VBI_B       (1<<9)      
#define RPS_OAN         (1<<27)     
#define RPS_INV         (1<<26)     
#define GPIO3_MSK       0xFF000000  

#define MASK_00   0x00000001    
#define MASK_01   0x00000002    
#define MASK_02   0x00000004    
#define MASK_03   0x00000008    
#define MASK_04   0x00000010    
#define MASK_05   0x00000020    
#define MASK_06   0x00000040    
#define MASK_07   0x00000080    
#define MASK_08   0x00000100    
#define MASK_09   0x00000200    
#define MASK_10   0x00000400    
#define MASK_11   0x00000800    
#define MASK_12   0x00001000    
#define MASK_13   0x00002000    
#define MASK_14   0x00004000    
#define MASK_15   0x00008000    
#define MASK_16   0x00010000    
#define MASK_17   0x00020000    
#define MASK_18   0x00040000    
#define MASK_19   0x00080000    
#define MASK_20   0x00100000    
#define MASK_21   0x00200000    
#define MASK_22   0x00400000    
#define MASK_23   0x00800000    
#define MASK_24   0x01000000    
#define MASK_25   0x02000000    
#define MASK_26   0x04000000    
#define MASK_27   0x08000000    
#define MASK_28   0x10000000    
#define MASK_29   0x20000000    
#define MASK_30   0x40000000    
#define MASK_31   0x80000000    

#define MASK_B0   0x000000ff    
#define MASK_B1   0x0000ff00    
#define MASK_B2   0x00ff0000    
#define MASK_B3   0xff000000    

#define MASK_W0   0x0000ffff    
#define MASK_W1   0xffff0000    

#define MASK_PA   0xfffffffc    
#define MASK_PR   0xfffffffe	
#define MASK_ER   0xffffffff    

#define MASK_NONE 0x00000000    

#define BASE_ODD1         0x00  
#define BASE_EVEN1        0x04
#define PROT_ADDR1        0x08
#define PITCH1            0x0C
#define BASE_PAGE1        0x10  
#define NUM_LINE_BYTE1    0x14

#define BASE_ODD2         0x18  
#define BASE_EVEN2        0x1C
#define PROT_ADDR2        0x20
#define PITCH2            0x24
#define BASE_PAGE2        0x28  
#define NUM_LINE_BYTE2    0x2C

#define BASE_ODD3         0x30  
#define BASE_EVEN3        0x34
#define PROT_ADDR3        0x38
#define PITCH3            0x3C
#define BASE_PAGE3        0x40  
#define NUM_LINE_BYTE3    0x44

#define PCI_BT_V1         0x48  
#define PCI_BT_V2         0x49  
#define PCI_BT_V3         0x4A  
#define PCI_BT_DEBI       0x4B  
#define PCI_BT_A          0x4C  

#define DD1_INIT          0x50  

#define DD1_STREAM_B      0x54  
#define DD1_STREAM_A      0x56  

#define BRS_CTRL          0x58  
#define HPS_CTRL          0x5C  
#define HPS_V_SCALE       0x60  
#define HPS_V_GAIN        0x64  
#define HPS_H_PRESCALE    0x68  
#define HPS_H_SCALE       0x6C  
#define BCS_CTRL          0x70  
#define CHROMA_KEY_RANGE  0x74
#define CLIP_FORMAT_CTRL  0x78  

#define DEBI_CONFIG       0x7C
#define DEBI_COMMAND      0x80
#define DEBI_PAGE         0x84
#define DEBI_AD           0x88

#define I2C_TRANSFER      0x8C
#define I2C_STATUS        0x90

#define BASE_A1_IN        0x94	
#define PROT_A1_IN        0x98
#define PAGE_A1_IN        0x9C

#define BASE_A1_OUT       0xA0  
#define PROT_A1_OUT       0xA4
#define PAGE_A1_OUT       0xA8

#define BASE_A2_IN        0xAC  
#define PROT_A2_IN        0xB0
#define PAGE_A2_IN        0xB4

#define BASE_A2_OUT       0xB8  
#define PROT_A2_OUT       0xBC
#define PAGE_A2_OUT       0xC0

#define RPS_PAGE0         0xC4  
#define RPS_PAGE1         0xC8  

#define RPS_THRESH0       0xCC  
#define RPS_THRESH1       0xD0  

#define RPS_TOV0          0xD4  
#define RPS_TOV1          0xD8  

#define IER               0xDC  

#define GPIO_CTRL         0xE0  

#define EC1SSR            0xE4  
#define EC2SSR            0xE8  
#define ECT1R             0xEC  
#define ECT2R             0xF0  

#define ACON1             0xF4
#define ACON2             0xF8

#define MC1               0xFC   
#define MC2               0x100  

#define RPS_ADDR0         0x104  
#define RPS_ADDR1         0x108  

#define ISR               0x10C  
#define PSR               0x110  
#define SSR               0x114  

#define EC1R              0x118  
#define EC2R              0x11C  

#define PCI_VDP1          0x120  
#define PCI_VDP2          0x124  
#define PCI_VDP3          0x128  
#define PCI_ADP1          0x12C  
#define PCI_ADP2          0x130  
#define PCI_ADP3          0x134  
#define PCI_ADP4          0x138  
#define PCI_DMA_DDP       0x13C  

#define LEVEL_REP         0x140,
#define A_TIME_SLOT1      0x180,  
#define A_TIME_SLOT2      0x1C0,  

#define SPCI_PPEF       0x80000000  
#define SPCI_PABO       0x40000000  
#define SPCI_PPED       0x20000000  
#define SPCI_RPS_I1     0x10000000  
#define SPCI_RPS_I0     0x08000000  
#define SPCI_RPS_LATE1  0x04000000  
#define SPCI_RPS_LATE0  0x02000000  
#define SPCI_RPS_E1     0x01000000  
#define SPCI_RPS_E0     0x00800000  
#define SPCI_RPS_TO1    0x00400000  
#define SPCI_RPS_TO0    0x00200000  
#define SPCI_UPLD       0x00100000  
#define SPCI_DEBI_S     0x00080000  
#define SPCI_DEBI_E     0x00040000  
#define SPCI_IIC_S      0x00020000  
#define SPCI_IIC_E      0x00010000  
#define SPCI_A2_IN      0x00008000  
#define SPCI_A2_OUT     0x00004000  
#define SPCI_A1_IN      0x00002000  
#define SPCI_A1_OUT     0x00001000  
#define SPCI_AFOU       0x00000800  
#define SPCI_V_PE       0x00000400  
#define SPCI_VFOU       0x00000200  
#define SPCI_FIDA       0x00000100  
#define SPCI_FIDB       0x00000080  
#define SPCI_PIN3       0x00000040  
#define SPCI_PIN2       0x00000020  
#define SPCI_PIN1       0x00000010  
#define SPCI_PIN0       0x00000008  
#define SPCI_ECS        0x00000004  
#define SPCI_EC3S       0x00000002  
#define SPCI_EC0S       0x00000001  

#define	SAA7146_I2C_ABORT	(1<<7)
#define	SAA7146_I2C_SPERR	(1<<6)
#define	SAA7146_I2C_APERR	(1<<5)
#define	SAA7146_I2C_DTERR	(1<<4)
#define	SAA7146_I2C_DRERR	(1<<3)
#define	SAA7146_I2C_AL		(1<<2)
#define	SAA7146_I2C_ERR		(1<<1)
#define	SAA7146_I2C_BUSY	(1<<0)

#define	SAA7146_I2C_START	(0x3)
#define	SAA7146_I2C_CONT	(0x2)
#define	SAA7146_I2C_STOP	(0x1)
#define	SAA7146_I2C_NOP		(0x0)

#define SAA7146_I2C_BUS_BIT_RATE_6400	(0x500)
#define SAA7146_I2C_BUS_BIT_RATE_3200	(0x100)
#define SAA7146_I2C_BUS_BIT_RATE_480	(0x400)
#define SAA7146_I2C_BUS_BIT_RATE_320	(0x600)
#define SAA7146_I2C_BUS_BIT_RATE_240	(0x700)
#define SAA7146_I2C_BUS_BIT_RATE_120	(0x000)
#define SAA7146_I2C_BUS_BIT_RATE_80	(0x200)
#define SAA7146_I2C_BUS_BIT_RATE_60	(0x300)

static inline void SAA7146_IER_DISABLE(struct saa7146_dev *x, unsigned y)
{
	unsigned long flags;
	spin_lock_irqsave(&x->int_slock, flags);
	saa7146_write(x, IER, saa7146_read(x, IER) & ~y);
	spin_unlock_irqrestore(&x->int_slock, flags);
}

static inline void SAA7146_IER_ENABLE(struct saa7146_dev *x, unsigned y)
{
	unsigned long flags;
	spin_lock_irqsave(&x->int_slock, flags);
	saa7146_write(x, IER, saa7146_read(x, IER) | y);
	spin_unlock_irqrestore(&x->int_slock, flags);
}

#endif
