/*
 * Definition of platform feature hooks for PowerMacs
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1998 Paul Mackerras &
 *                    Ben. Herrenschmidt.
 *
 *
 * Note: I removed media-bay details from the feature stuff, I believe it's
 *       not worth it, the media-bay driver can directly use the mac-io
 *       ASIC registers.
 *
 * Implementation note: Currently, none of these functions will block.
 * However, they may internally protect themselves with a spinlock
 * for way too long. Be prepared for at least some of these to block
 * in the future.
 *
 * Unless specifically defined, the result code is assumed to be an
 * error when negative, 0 is the default success result. Some functions
 * may return additional positive result values.
 *
 * To keep implementation simple, all feature calls are assumed to have
 * the prototype parameters (struct device_node* node, int value).
 * When either is not used, pass 0.
 */

#ifdef __KERNEL__
#ifndef __ASM_POWERPC_PMAC_FEATURE_H
#define __ASM_POWERPC_PMAC_FEATURE_H

#include <asm/macio.h>
#include <asm/machdep.h>


#define PMAC_TYPE_PSURGE		0x10	
#define PMAC_TYPE_ANS			0x11	

#define PMAC_TYPE_COMET			0x20	
#define PMAC_TYPE_HOOPER		0x21	
#define PMAC_TYPE_KANGA			0x22	
#define PMAC_TYPE_ALCHEMY		0x23	
#define PMAC_TYPE_GAZELLE		0x24	
#define PMAC_TYPE_UNKNOWN_OHARE		0x2f	

#define PMAC_TYPE_GOSSAMER		0x30	
#define PMAC_TYPE_SILK			0x31	
#define PMAC_TYPE_WALLSTREET		0x32	
#define PMAC_TYPE_UNKNOWN_HEATHROW	0x3f	

#define PMAC_TYPE_101_PBOOK		0x40	
#define PMAC_TYPE_ORIG_IMAC		0x41	
#define PMAC_TYPE_YOSEMITE		0x42	
#define PMAC_TYPE_YIKES			0x43	
#define PMAC_TYPE_UNKNOWN_PADDINGTON	0x4f	

#define PMAC_TYPE_ORIG_IBOOK		0x40	
#define PMAC_TYPE_SAWTOOTH		0x41	
#define PMAC_TYPE_FW_IMAC		0x42	
#define PMAC_TYPE_FW_IBOOK		0x43	
#define PMAC_TYPE_CUBE			0x44	
#define PMAC_TYPE_QUICKSILVER		0x45	
#define PMAC_TYPE_PISMO			0x46	
#define PMAC_TYPE_TITANIUM		0x47	
#define PMAC_TYPE_TITANIUM2		0x48	
#define PMAC_TYPE_TITANIUM3		0x49	
#define PMAC_TYPE_TITANIUM4		0x50	
#define PMAC_TYPE_EMAC			0x50	
#define PMAC_TYPE_UNKNOWN_CORE99	0x5f

#define PMAC_TYPE_RACKMAC		0x80	
#define PMAC_TYPE_WINDTUNNEL		0x81

#define PMAC_TYPE_PANGEA_IMAC		0x100	
#define PMAC_TYPE_IBOOK2		0x101	
#define PMAC_TYPE_FLAT_PANEL_IMAC	0x102	
#define PMAC_TYPE_UNKNOWN_PANGEA	0x10f

#define PMAC_TYPE_UNKNOWN_INTREPID	0x11f	

#define PMAC_TYPE_POWERMAC_G5		0x150	
#define PMAC_TYPE_POWERMAC_G5_U3L	0x151	
#define PMAC_TYPE_IMAC_G5		0x152	
#define PMAC_TYPE_XSERVE_G5		0x153	
#define PMAC_TYPE_UNKNOWN_K2		0x19f	
#define PMAC_TYPE_UNKNOWN_SHASTA       	0x19e	


#define PMAC_MB_CAN_SLEEP		0x00000001
#define PMAC_MB_HAS_FW_POWER		0x00000002
#define PMAC_MB_OLD_CORE99		0x00000004
#define PMAC_MB_MOBILE			0x00000008
#define PMAC_MB_MAY_SLEEP		0x00000010


struct device_node;

static inline long pmac_call_feature(int selector, struct device_node* node,
					long param, long value)
{
	if (!ppc_md.feature_call || !machine_is(powermac))
		return -ENODEV;
	return ppc_md.feature_call(selector, node, param, value);
}

#define PMAC_FTR_SCC_ENABLE		PMAC_FTR_DEF(0)
	#define PMAC_SCC_ASYNC		0
	#define PMAC_SCC_IRDA		1
	#define PMAC_SCC_I2S1		2
	#define PMAC_SCC_FLAG_XMON	0x00001000

#define PMAC_FTR_MODEM_ENABLE		PMAC_FTR_DEF(1)

#define PMAC_FTR_SWIM3_ENABLE		PMAC_FTR_DEF(2)

#define PMAC_FTR_MESH_ENABLE		PMAC_FTR_DEF(3)

#define PMAC_FTR_IDE_ENABLE		PMAC_FTR_DEF(4)

#define PMAC_FTR_IDE_RESET		PMAC_FTR_DEF(5)

#define PMAC_FTR_BMAC_ENABLE		PMAC_FTR_DEF(6)

#define PMAC_FTR_GMAC_ENABLE		PMAC_FTR_DEF(7)

#define PMAC_FTR_GMAC_PHY_RESET		PMAC_FTR_DEF(8)

#define PMAC_FTR_SOUND_CHIP_ENABLE	PMAC_FTR_DEF(9)


#define PMAC_FTR_AIRPORT_ENABLE		PMAC_FTR_DEF(10)

#define PMAC_FTR_RESET_CPU		PMAC_FTR_DEF(11)

#define PMAC_FTR_USB_ENABLE		PMAC_FTR_DEF(12)

#define PMAC_FTR_1394_ENABLE		PMAC_FTR_DEF(13)

#define PMAC_FTR_1394_CABLE_POWER	PMAC_FTR_DEF(14)

#define PMAC_FTR_SLEEP_STATE		PMAC_FTR_DEF(15)

#define PMAC_FTR_GET_MB_INFO		PMAC_FTR_DEF(16)
#define   PMAC_MB_INFO_MODEL	0
#define   PMAC_MB_INFO_FLAGS	1
#define   PMAC_MB_INFO_NAME	2

#define PMAC_FTR_READ_GPIO		PMAC_FTR_DEF(17)

#define PMAC_FTR_WRITE_GPIO		PMAC_FTR_DEF(18)

#define PMAC_FTR_ENABLE_MPIC		PMAC_FTR_DEF(19)

#define PMAC_FTR_AACK_DELAY_ENABLE     	PMAC_FTR_DEF(20)

#define PMAC_FTR_DEVICE_CAN_WAKE	PMAC_FTR_DEF(22)


extern long pmac_do_feature_call(unsigned int selector, ...);
extern void pmac_feature_init(void);

extern void pmac_set_early_video_resume(void (*proc)(void *data), void *data);
extern void pmac_call_early_video_resume(void);

#define PMAC_FTR_DEF(x) ((0x6660000) | (x))

extern void pmac_register_agp_pm(struct pci_dev *bridge,
				 int (*suspend)(struct pci_dev *bridge),
				 int (*resume)(struct pci_dev *bridge));

extern void pmac_suspend_agp_for_card(struct pci_dev *dev);
extern void pmac_resume_agp_for_card(struct pci_dev *dev);


#define MAX_MACIO_CHIPS		2

enum {
	macio_unknown = 0,
	macio_grand_central,
	macio_ohare,
	macio_ohareII,
	macio_heathrow,
	macio_gatwick,
	macio_paddington,
	macio_keylargo,
	macio_pangea,
	macio_intrepid,
	macio_keylargo2,
	macio_shasta,
};

struct macio_chip
{
	struct device_node	*of_node;
	int			type;
	const char		*name;
	int			rev;
	volatile u32		__iomem *base;
	unsigned long		flags;

	
	struct macio_bus	lbus;
};

extern struct macio_chip macio_chips[MAX_MACIO_CHIPS];

#define MACIO_FLAG_SCCA_ON	0x00000001
#define MACIO_FLAG_SCCB_ON	0x00000002
#define MACIO_FLAG_SCC_LOCKED	0x00000004
#define MACIO_FLAG_AIRPORT_ON	0x00000010
#define MACIO_FLAG_FW_SUPPORTED	0x00000020

extern struct macio_chip* macio_find(struct device_node* child, int type);

#define MACIO_FCR32(macio, r)	((macio)->base + ((r) >> 2))
#define MACIO_FCR8(macio, r)	(((volatile u8 __iomem *)((macio)->base)) + (r))

#define MACIO_IN32(r)		(in_le32(MACIO_FCR32(macio,r)))
#define MACIO_OUT32(r,v)	(out_le32(MACIO_FCR32(macio,r), (v)))
#define MACIO_BIS(r,v)		(MACIO_OUT32((r), MACIO_IN32(r) | (v)))
#define MACIO_BIC(r,v)		(MACIO_OUT32((r), MACIO_IN32(r) & ~(v)))
#define MACIO_IN8(r)		(in_8(MACIO_FCR8(macio,r)))
#define MACIO_OUT8(r,v)		(out_8(MACIO_FCR8(macio,r), (v)))

extern raw_spinlock_t feature_lock;
extern struct device_node *uninorth_node;
extern u32 __iomem *uninorth_base;


#define UN_REG(r)	(uninorth_base + ((r) >> 2))
#define UN_IN(r)	(in_be32(UN_REG(r)))
#define UN_OUT(r,v)	(out_be32(UN_REG(r), (v)))
#define UN_BIS(r,v)	(UN_OUT((r), UN_IN(r) | (v)))
#define UN_BIC(r,v)	(UN_OUT((r), UN_IN(r) & ~(v)))

extern int pmac_get_uninorth_variant(void);

#endif 
#endif 
