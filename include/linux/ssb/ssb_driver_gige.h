#ifndef LINUX_SSB_DRIVER_GIGE_H_
#define LINUX_SSB_DRIVER_GIGE_H_

#include <linux/ssb/ssb.h>
#include <linux/bug.h>
#include <linux/pci.h>
#include <linux/spinlock.h>


#ifdef CONFIG_SSB_DRIVER_GIGE


#define SSB_GIGE_PCIIO			0x0000 
#define SSB_GIGE_RESERVED		0x0400 
#define SSB_GIGE_PCICFG			0x0800 
#define SSB_GIGE_SHIM_FLUSHSTAT		0x0C00 
#define SSB_GIGE_SHIM_FLUSHRDA		0x0C04 
#define SSB_GIGE_SHIM_FLUSHTO		0x0C08 
#define SSB_GIGE_SHIM_BARRIER		0x0C0C 
#define SSB_GIGE_SHIM_MAOCPSI		0x0C10 
#define SSB_GIGE_SHIM_SIOCPMA		0x0C14 

#define SSB_GIGE_TMSHIGH_RGMII		0x00010000 
#define SSB_GIGE_TMSLOW_TXBYPASS	0x00080000 
#define SSB_GIGE_TMSLOW_RXBYPASS	0x00100000 
#define SSB_GIGE_TMSLOW_DLLEN		0x01000000 

#define SSB_GIGE_BFL_ROBOSWITCH		0x0010


#define SSB_GIGE_MEM_RES_NAME		"SSB Broadcom 47xx GigE memory"
#define SSB_GIGE_IO_RES_NAME		"SSB Broadcom 47xx GigE I/O"

struct ssb_gige {
	struct ssb_device *dev;

	spinlock_t lock;

	bool has_rgmii;

	
	struct pci_controller pci_controller;
	struct pci_ops pci_ops;
	struct resource mem_resource;
	struct resource io_resource;
};

extern bool pdev_is_ssb_gige_core(struct pci_dev *pdev);

static inline struct ssb_gige * pdev_to_ssb_gige(struct pci_dev *pdev)
{
	if (!pdev_is_ssb_gige_core(pdev))
		return NULL;
	return container_of(pdev->bus->ops, struct ssb_gige, pci_ops);
}

static inline bool ssb_gige_is_rgmii(struct pci_dev *pdev)
{
	struct ssb_gige *dev = pdev_to_ssb_gige(pdev);
	return (dev ? dev->has_rgmii : 0);
}

static inline bool ssb_gige_have_roboswitch(struct pci_dev *pdev)
{
	struct ssb_gige *dev = pdev_to_ssb_gige(pdev);
	if (dev)
		return !!(dev->dev->bus->sprom.boardflags_lo &
			  SSB_GIGE_BFL_ROBOSWITCH);
	return 0;
}

static inline bool ssb_gige_one_dma_at_once(struct pci_dev *pdev)
{
	struct ssb_gige *dev = pdev_to_ssb_gige(pdev);
	if (dev)
		return ((dev->dev->bus->chip_id == 0x4785) &&
			(dev->dev->bus->chip_rev < 2));
	return 0;
}

static inline bool ssb_gige_must_flush_posted_writes(struct pci_dev *pdev)
{
	struct ssb_gige *dev = pdev_to_ssb_gige(pdev);
	if (dev)
		return (dev->dev->bus->chip_id == 0x4785);
	return 0;
}

#ifdef CONFIG_BCM47XX
#include <asm/mach-bcm47xx/nvram.h>
static inline void ssb_gige_get_macaddr(struct pci_dev *pdev, u8 *macaddr)
{
	char buf[20];
	if (nvram_getenv("et0macaddr", buf, sizeof(buf)) < 0)
		return;
	nvram_parse_macaddr(buf, macaddr);
}
#else
static inline void ssb_gige_get_macaddr(struct pci_dev *pdev, u8 *macaddr)
{
}
#endif

extern int ssb_gige_pcibios_plat_dev_init(struct ssb_device *sdev,
					  struct pci_dev *pdev);
extern int ssb_gige_map_irq(struct ssb_device *sdev,
			    const struct pci_dev *pdev);

extern int ssb_gige_init(void);
static inline void ssb_gige_exit(void)
{
	BUG();
}


#else 


static inline int ssb_gige_pcibios_plat_dev_init(struct ssb_device *sdev,
						 struct pci_dev *pdev)
{
	return -ENOSYS;
}
static inline int ssb_gige_map_irq(struct ssb_device *sdev,
				   const struct pci_dev *pdev)
{
	return -ENOSYS;
}
static inline int ssb_gige_init(void)
{
	return 0;
}
static inline void ssb_gige_exit(void)
{
}

static inline bool pdev_is_ssb_gige_core(struct pci_dev *pdev)
{
	return 0;
}
static inline struct ssb_gige * pdev_to_ssb_gige(struct pci_dev *pdev)
{
	return NULL;
}
static inline bool ssb_gige_is_rgmii(struct pci_dev *pdev)
{
	return 0;
}
static inline bool ssb_gige_have_roboswitch(struct pci_dev *pdev)
{
	return 0;
}
static inline bool ssb_gige_one_dma_at_once(struct pci_dev *pdev)
{
	return 0;
}
static inline bool ssb_gige_must_flush_posted_writes(struct pci_dev *pdev)
{
	return 0;
}

#endif 
#endif 
