#ifndef LINUX_SSB_H_
#define LINUX_SSB_H_

#include <linux/device.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/mod_devicetable.h>
#include <linux/dma-mapping.h>

#include <linux/ssb/ssb_regs.h>


struct pcmcia_device;
struct ssb_bus;
struct ssb_driver;

struct ssb_sprom_core_pwr_info {
	u8 itssi_2g, itssi_5g;
	u8 maxpwr_2g, maxpwr_5gl, maxpwr_5g, maxpwr_5gh;
	u16 pa_2g[4], pa_5gl[4], pa_5g[4], pa_5gh[4];
};

struct ssb_sprom {
	u8 revision;
	u8 il0mac[6];		
	u8 et0mac[6];		
	u8 et1mac[6];		
	u8 et0phyaddr;		
	u8 et1phyaddr;		
	u8 et0mdcport;		
	u8 et1mdcport;		
	u16 board_rev;		
	u16 board_num;		
	u16 board_type;		
	u8 country_code;	
	char alpha2[2];		
	u8 leddc_on_time;	
	u8 leddc_off_time;	
	u8 ant_available_a;	
	u8 ant_available_bg;	
	u16 pa0b0;
	u16 pa0b1;
	u16 pa0b2;
	u16 pa1b0;
	u16 pa1b1;
	u16 pa1b2;
	u16 pa1lob0;
	u16 pa1lob1;
	u16 pa1lob2;
	u16 pa1hib0;
	u16 pa1hib1;
	u16 pa1hib2;
	u8 gpio0;		
	u8 gpio1;		
	u8 gpio2;		
	u8 gpio3;		
	u8 maxpwr_bg;		
	u8 maxpwr_al;		
	u8 maxpwr_a;		
	u8 maxpwr_ah;		
	u8 itssi_a;		
	u8 itssi_bg;		
	u8 tri2g;		
	u8 tri5gl;		
	u8 tri5g;		
	u8 tri5gh;		
	u8 txpid2g[4];		
	u8 txpid5gl[4];		
	u8 txpid5g[4];		
	u8 txpid5gh[4];		
	s8 rxpo2g;		
	s8 rxpo5g;		
	u8 rssisav2g;		
	u8 rssismc2g;
	u8 rssismf2g;
	u8 bxa2g;		
	u8 rssisav5g;		
	u8 rssismc5g;
	u8 rssismf5g;
	u8 bxa5g;		
	u16 cck2gpo;		
	u32 ofdm2gpo;		
	u32 ofdm5glpo;		
	u32 ofdm5gpo;		
	u32 ofdm5ghpo;		
	u16 boardflags_lo;	
	u16 boardflags_hi;	
	u16 boardflags2_lo;	
	u16 boardflags2_hi;	
	

	struct ssb_sprom_core_pwr_info core_pwr_info[4];

	struct {
		s8 a0, a1, a2, a3;
	} antenna_gain;

	struct {
		struct {
			u8 tssipos, extpa_gain, pdet_range, tr_iso, antswlut;
		} ghz2;
		struct {
			u8 tssipos, extpa_gain, pdet_range, tr_iso, antswlut;
		} ghz5;
	} fem;

	u16 mcs2gpo[8];
	u16 mcs5gpo[8];
	u16 mcs5glpo[8];
	u16 mcs5ghpo[8];
	u8 opo;

	u8 rxgainerr2ga[3];
	u8 rxgainerr5gla[3];
	u8 rxgainerr5gma[3];
	u8 rxgainerr5gha[3];
	u8 rxgainerr5gua[3];

	u8 noiselvl2ga[3];
	u8 noiselvl5gla[3];
	u8 noiselvl5gma[3];
	u8 noiselvl5gha[3];
	u8 noiselvl5gua[3];

	u8 regrev;
	u8 txchain;
	u8 rxchain;
	u8 antswitch;
	u16 cddpo;
	u16 stbcpo;
	u16 bw40po;
	u16 bwduppo;

	u8 tempthresh;
	u8 tempoffset;
	u16 rawtempsense;
	u8 measpower;
	u8 tempsense_slope;
	u8 tempcorrx;
	u8 tempsense_option;
	u8 freqoffset_corr;
	u8 iqcal_swp_dis;
	u8 hw_iqcal_en;
	u8 elna2g;
	u8 elna5g;
	u8 phycal_tempdelta;
	u8 temps_period;
	u8 temps_hysteresis;
	u8 measpower1;
	u8 measpower2;
	u8 pcieingress_war;

	
	u16 cckbw202gpo;
	u16 cckbw20ul2gpo;
	u32 legofdmbw202gpo;
	u32 legofdmbw20ul2gpo;
	u32 legofdmbw205glpo;
	u32 legofdmbw20ul5glpo;
	u32 legofdmbw205gmpo;
	u32 legofdmbw20ul5gmpo;
	u32 legofdmbw205ghpo;
	u32 legofdmbw20ul5ghpo;
	u32 mcsbw202gpo;
	u32 mcsbw20ul2gpo;
	u32 mcsbw402gpo;
	u32 mcsbw205glpo;
	u32 mcsbw20ul5glpo;
	u32 mcsbw405glpo;
	u32 mcsbw205gmpo;
	u32 mcsbw20ul5gmpo;
	u32 mcsbw405gmpo;
	u32 mcsbw205ghpo;
	u32 mcsbw20ul5ghpo;
	u32 mcsbw405ghpo;
	u16 mcs32po;
	u16 legofdm40duppo;
	u8 sar2g;
	u8 sar5g;
};

struct ssb_boardinfo {
	u16 vendor;
	u16 type;
	u8  rev;
};


struct ssb_device;
struct ssb_bus_ops {
	u8 (*read8)(struct ssb_device *dev, u16 offset);
	u16 (*read16)(struct ssb_device *dev, u16 offset);
	u32 (*read32)(struct ssb_device *dev, u16 offset);
	void (*write8)(struct ssb_device *dev, u16 offset, u8 value);
	void (*write16)(struct ssb_device *dev, u16 offset, u16 value);
	void (*write32)(struct ssb_device *dev, u16 offset, u32 value);
#ifdef CONFIG_SSB_BLOCKIO
	void (*block_read)(struct ssb_device *dev, void *buffer,
			   size_t count, u16 offset, u8 reg_width);
	void (*block_write)(struct ssb_device *dev, const void *buffer,
			    size_t count, u16 offset, u8 reg_width);
#endif
};


#define SSB_DEV_CHIPCOMMON	0x800
#define SSB_DEV_ILINE20		0x801
#define SSB_DEV_SDRAM		0x803
#define SSB_DEV_PCI		0x804
#define SSB_DEV_MIPS		0x805
#define SSB_DEV_ETHERNET	0x806
#define SSB_DEV_V90		0x807
#define SSB_DEV_USB11_HOSTDEV	0x808
#define SSB_DEV_ADSL		0x809
#define SSB_DEV_ILINE100	0x80A
#define SSB_DEV_IPSEC		0x80B
#define SSB_DEV_PCMCIA		0x80D
#define SSB_DEV_INTERNAL_MEM	0x80E
#define SSB_DEV_MEMC_SDRAM	0x80F
#define SSB_DEV_EXTIF		0x811
#define SSB_DEV_80211		0x812
#define SSB_DEV_MIPS_3302	0x816
#define SSB_DEV_USB11_HOST	0x817
#define SSB_DEV_USB11_DEV	0x818
#define SSB_DEV_USB20_HOST	0x819
#define SSB_DEV_USB20_DEV	0x81A
#define SSB_DEV_SDIO_HOST	0x81B
#define SSB_DEV_ROBOSWITCH	0x81C
#define SSB_DEV_PARA_ATA	0x81D
#define SSB_DEV_SATA_XORDMA	0x81E
#define SSB_DEV_ETHERNET_GBIT	0x81F
#define SSB_DEV_PCIE		0x820
#define SSB_DEV_MIMO_PHY	0x821
#define SSB_DEV_SRAM_CTRLR	0x822
#define SSB_DEV_MINI_MACPHY	0x823
#define SSB_DEV_ARM_1176	0x824
#define SSB_DEV_ARM_7TDMI	0x825

#define SSB_VENDOR_BROADCOM	0x4243

struct __ssb_dev_wrapper {
	struct device dev;
	struct ssb_device *sdev;
};

struct ssb_device {
	const struct ssb_bus_ops *ops;

	struct device *dev, *dma_dev;

	struct ssb_bus *bus;
	struct ssb_device_id id;

	u8 core_index;
	unsigned int irq;

	
	void *drvdata;		
	void *devtypedata;	
};

static inline
struct ssb_device * dev_to_ssb_dev(struct device *dev)
{
	struct __ssb_dev_wrapper *wrap;
	wrap = container_of(dev, struct __ssb_dev_wrapper, dev);
	return wrap->sdev;
}

static inline
void ssb_set_drvdata(struct ssb_device *dev, void *data)
{
	dev->drvdata = data;
}
static inline
void * ssb_get_drvdata(struct ssb_device *dev)
{
	return dev->drvdata;
}

void ssb_set_devtypedata(struct ssb_device *dev, void *data);
static inline
void * ssb_get_devtypedata(struct ssb_device *dev)
{
	return dev->devtypedata;
}


struct ssb_driver {
	const char *name;
	const struct ssb_device_id *id_table;

	int (*probe)(struct ssb_device *dev, const struct ssb_device_id *id);
	void (*remove)(struct ssb_device *dev);
	int (*suspend)(struct ssb_device *dev, pm_message_t state);
	int (*resume)(struct ssb_device *dev);
	void (*shutdown)(struct ssb_device *dev);

	struct device_driver drv;
};
#define drv_to_ssb_drv(_drv) container_of(_drv, struct ssb_driver, drv)

extern int __ssb_driver_register(struct ssb_driver *drv, struct module *owner);
#define ssb_driver_register(drv) \
	__ssb_driver_register(drv, THIS_MODULE)

extern void ssb_driver_unregister(struct ssb_driver *drv);




enum ssb_bustype {
	SSB_BUSTYPE_SSB,	
	SSB_BUSTYPE_PCI,	
	SSB_BUSTYPE_PCMCIA,	
	SSB_BUSTYPE_SDIO,	
};

#define SSB_BOARDVENDOR_BCM	0x14E4	
#define SSB_BOARDVENDOR_DELL	0x1028	
#define SSB_BOARDVENDOR_HP	0x0E11	
#define SSB_BOARD_BCM94306MP	0x0418
#define SSB_BOARD_BCM4309G	0x0421
#define SSB_BOARD_BCM4306CB	0x0417
#define SSB_BOARD_BCM4309MP	0x040C
#define SSB_BOARD_MP4318	0x044A
#define SSB_BOARD_BU4306	0x0416
#define SSB_BOARD_BU4309	0x040A
#define SSB_CHIPPACK_BCM4712S	1	
#define SSB_CHIPPACK_BCM4712M	2	
#define SSB_CHIPPACK_BCM4712L	0	

#include <linux/ssb/ssb_driver_chipcommon.h>
#include <linux/ssb/ssb_driver_mips.h>
#include <linux/ssb/ssb_driver_extif.h>
#include <linux/ssb/ssb_driver_pci.h>

struct ssb_bus {
	
	void __iomem *mmio;

	const struct ssb_bus_ops *ops;

	struct ssb_device *mapped_device;
	union {
		
		u8 mapped_pcmcia_seg;
		
		u32 sdio_sbaddr;
	};
	spinlock_t bar_lock;

	
	enum ssb_bustype bustype;
	
	union {
		
		struct pci_dev *host_pci;
		
		struct pcmcia_device *host_pcmcia;
		
		struct sdio_func *host_sdio;
	};

	
	unsigned int quirks;

#ifdef CONFIG_SSB_SPROM
	
	struct mutex sprom_mutex;
#endif

	
	u16 chip_id;
	u8 chip_rev;
	u16 sprom_offset;
	u16 sprom_size;		
	u8 chip_package;

	
	struct ssb_device devices[SSB_MAX_NR_CORES];
	u8 nr_devices;

	
	unsigned int busnumber;

	
	struct ssb_chipcommon chipco;
	
	struct ssb_pcicore pcicore;
	
	struct ssb_mipscore mipscore;
	
	struct ssb_extif extif;


	
	struct ssb_boardinfo boardinfo;
	
	struct ssb_sprom sprom;
	
	bool has_cardbus_slot;

#ifdef CONFIG_SSB_EMBEDDED
	
	spinlock_t gpio_lock;
#endif 

	
	struct list_head list;
#ifdef CONFIG_SSB_DEBUG
	
	bool powered_up;
	int power_warn_count;
#endif 
};

enum ssb_quirks {
	
	SSB_QUIRK_SDIO_READ_AFTER_WRITE32	= (1 << 0),
};

struct ssb_init_invariants {
	
	struct ssb_boardinfo boardinfo;
	struct ssb_sprom sprom;
	
	bool has_cardbus_slot;
};
typedef int (*ssb_invariants_func_t)(struct ssb_bus *bus,
				     struct ssb_init_invariants *iv);

extern int ssb_bus_ssbbus_register(struct ssb_bus *bus,
				   unsigned long baseaddr,
				   ssb_invariants_func_t get_invariants);
#ifdef CONFIG_SSB_PCIHOST
extern int ssb_bus_pcibus_register(struct ssb_bus *bus,
				   struct pci_dev *host_pci);
#endif 
#ifdef CONFIG_SSB_PCMCIAHOST
extern int ssb_bus_pcmciabus_register(struct ssb_bus *bus,
				      struct pcmcia_device *pcmcia_dev,
				      unsigned long baseaddr);
#endif 
#ifdef CONFIG_SSB_SDIOHOST
extern int ssb_bus_sdiobus_register(struct ssb_bus *bus,
				    struct sdio_func *sdio_func,
				    unsigned int quirks);
#endif 


extern void ssb_bus_unregister(struct ssb_bus *bus);

extern bool ssb_is_sprom_available(struct ssb_bus *bus);

extern int ssb_arch_register_fallback_sprom(
		int (*sprom_callback)(struct ssb_bus *bus,
		struct ssb_sprom *out));

extern int ssb_bus_suspend(struct ssb_bus *bus);
extern int ssb_bus_resume(struct ssb_bus *bus);

extern u32 ssb_clockspeed(struct ssb_bus *bus);

int ssb_device_is_enabled(struct ssb_device *dev);
void ssb_device_enable(struct ssb_device *dev, u32 core_specific_flags);
void ssb_device_disable(struct ssb_device *dev, u32 core_specific_flags);


static inline u8 ssb_read8(struct ssb_device *dev, u16 offset)
{
	return dev->ops->read8(dev, offset);
}
static inline u16 ssb_read16(struct ssb_device *dev, u16 offset)
{
	return dev->ops->read16(dev, offset);
}
static inline u32 ssb_read32(struct ssb_device *dev, u16 offset)
{
	return dev->ops->read32(dev, offset);
}
static inline void ssb_write8(struct ssb_device *dev, u16 offset, u8 value)
{
	dev->ops->write8(dev, offset, value);
}
static inline void ssb_write16(struct ssb_device *dev, u16 offset, u16 value)
{
	dev->ops->write16(dev, offset, value);
}
static inline void ssb_write32(struct ssb_device *dev, u16 offset, u32 value)
{
	dev->ops->write32(dev, offset, value);
}
#ifdef CONFIG_SSB_BLOCKIO
static inline void ssb_block_read(struct ssb_device *dev, void *buffer,
				  size_t count, u16 offset, u8 reg_width)
{
	dev->ops->block_read(dev, buffer, count, offset, reg_width);
}

static inline void ssb_block_write(struct ssb_device *dev, const void *buffer,
				   size_t count, u16 offset, u8 reg_width)
{
	dev->ops->block_write(dev, buffer, count, offset, reg_width);
}
#endif 



extern u32 ssb_dma_translation(struct ssb_device *dev);
#define SSB_DMA_TRANSLATION_MASK	0xC0000000
#define SSB_DMA_TRANSLATION_SHIFT	30

static inline void __cold __ssb_dma_not_implemented(struct ssb_device *dev)
{
#ifdef CONFIG_SSB_DEBUG
	printk(KERN_ERR "SSB: BUG! Calling DMA API for "
	       "unsupported bustype %d\n", dev->bus->bustype);
#endif 
}

#ifdef CONFIG_SSB_PCIHOST
extern int ssb_pcihost_register(struct pci_driver *driver);
static inline void ssb_pcihost_unregister(struct pci_driver *driver)
{
	pci_unregister_driver(driver);
}

static inline
void ssb_pcihost_set_power_state(struct ssb_device *sdev, pci_power_t state)
{
	if (sdev->bus->bustype == SSB_BUSTYPE_PCI)
		pci_set_power_state(sdev->bus->host_pci, state);
}
#else
static inline void ssb_pcihost_unregister(struct pci_driver *driver)
{
}

static inline
void ssb_pcihost_set_power_state(struct ssb_device *sdev, pci_power_t state)
{
}
#endif 


extern int ssb_bus_may_powerdown(struct ssb_bus *bus);
extern int ssb_bus_powerup(struct ssb_bus *bus, bool dynamic_pctl);

extern void ssb_commit_settings(struct ssb_bus *bus);

extern u32 ssb_admatch_base(u32 adm);
extern u32 ssb_admatch_size(u32 adm);

#ifdef CONFIG_SSB_EMBEDDED
int ssb_pcibios_plat_dev_init(struct pci_dev *dev);
int ssb_pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin);
#endif 

#endif 
