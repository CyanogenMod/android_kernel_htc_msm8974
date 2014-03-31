/*
 * Common Flash Interface support:
 *   AMD & Fujitsu Standard Vendor Command Set (ID 0x0002)
 *
 * Copyright (C) 2000 Crossnet Co. <info@crossnet.co.jp>
 * Copyright (C) 2004 Arcom Control Systems Ltd <linux@arcom.com>
 * Copyright (C) 2005 MontaVista Software Inc. <source@mvista.com>
 *
 * 2_by_8 routines added by Simon Munton
 *
 * 4_by_16 work by Carolyn J. Smith
 *
 * XIP support hooks by Vitaly Wool (based on code for Intel flash
 * by Nicolas Pitre)
 *
 * 25/09/2008 Christopher Moore: TopBottom fixup for many Macronix with CFI V1.0
 *
 * Occasionally maintained by Thayne Harbaugh tharbaugh at lnxi dot com
 *
 * This code is GPL
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/byteorder.h>

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/mtd/map.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/xip.h>

#define AMD_BOOTLOC_BUG
#define FORCE_WORD_WRITE 0

#define MAX_WORD_RETRIES 3

#define SST49LF004B	        0x0060
#define SST49LF040B	        0x0050
#define SST49LF008A		0x005a
#define AT49BV6416		0x00d6

static int cfi_amdstd_read (struct mtd_info *, loff_t, size_t, size_t *, u_char *);
static int cfi_amdstd_write_words(struct mtd_info *, loff_t, size_t, size_t *, const u_char *);
static int cfi_amdstd_write_buffers(struct mtd_info *, loff_t, size_t, size_t *, const u_char *);
static int cfi_amdstd_erase_chip(struct mtd_info *, struct erase_info *);
static int cfi_amdstd_erase_varsize(struct mtd_info *, struct erase_info *);
static void cfi_amdstd_sync (struct mtd_info *);
static int cfi_amdstd_suspend (struct mtd_info *);
static void cfi_amdstd_resume (struct mtd_info *);
static int cfi_amdstd_reboot(struct notifier_block *, unsigned long, void *);
static int cfi_amdstd_secsi_read (struct mtd_info *, loff_t, size_t, size_t *, u_char *);

static int cfi_amdstd_panic_write(struct mtd_info *mtd, loff_t to, size_t len,
				  size_t *retlen, const u_char *buf);

static void cfi_amdstd_destroy(struct mtd_info *);

struct mtd_info *cfi_cmdset_0002(struct map_info *, int);
static struct mtd_info *cfi_amdstd_setup (struct mtd_info *);

static int get_chip(struct map_info *map, struct flchip *chip, unsigned long adr, int mode);
static void put_chip(struct map_info *map, struct flchip *chip, unsigned long adr);
#include "fwh_lock.h"

static int cfi_atmel_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len);
static int cfi_atmel_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len);

static struct mtd_chip_driver cfi_amdstd_chipdrv = {
	.probe		= NULL, 
	.destroy	= cfi_amdstd_destroy,
	.name		= "cfi_cmdset_0002",
	.module		= THIS_MODULE
};




#ifdef DEBUG_CFI_FEATURES
static void cfi_tell_features(struct cfi_pri_amdstd *extp)
{
	const char* erase_suspend[3] = {
		"Not supported", "Read only", "Read/write"
	};
	const char* top_bottom[6] = {
		"No WP", "8x8KiB sectors at top & bottom, no WP",
		"Bottom boot", "Top boot",
		"Uniform, Bottom WP", "Uniform, Top WP"
	};

	printk("  Silicon revision: %d\n", extp->SiliconRevision >> 1);
	printk("  Address sensitive unlock: %s\n",
	       (extp->SiliconRevision & 1) ? "Not required" : "Required");

	if (extp->EraseSuspend < ARRAY_SIZE(erase_suspend))
		printk("  Erase Suspend: %s\n", erase_suspend[extp->EraseSuspend]);
	else
		printk("  Erase Suspend: Unknown value %d\n", extp->EraseSuspend);

	if (extp->BlkProt == 0)
		printk("  Block protection: Not supported\n");
	else
		printk("  Block protection: %d sectors per group\n", extp->BlkProt);


	printk("  Temporary block unprotect: %s\n",
	       extp->TmpBlkUnprotect ? "Supported" : "Not supported");
	printk("  Block protect/unprotect scheme: %d\n", extp->BlkProtUnprot);
	printk("  Number of simultaneous operations: %d\n", extp->SimultaneousOps);
	printk("  Burst mode: %s\n",
	       extp->BurstMode ? "Supported" : "Not supported");
	if (extp->PageMode == 0)
		printk("  Page mode: Not supported\n");
	else
		printk("  Page mode: %d word page\n", extp->PageMode << 2);

	printk("  Vpp Supply Minimum Program/Erase Voltage: %d.%d V\n",
	       extp->VppMin >> 4, extp->VppMin & 0xf);
	printk("  Vpp Supply Maximum Program/Erase Voltage: %d.%d V\n",
	       extp->VppMax >> 4, extp->VppMax & 0xf);

	if (extp->TopBottom < ARRAY_SIZE(top_bottom))
		printk("  Top/Bottom Boot Block: %s\n", top_bottom[extp->TopBottom]);
	else
		printk("  Top/Bottom Boot Block: Unknown value %d\n", extp->TopBottom);
}
#endif

#ifdef AMD_BOOTLOC_BUG
static void fixup_amd_bootblock(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	struct cfi_pri_amdstd *extp = cfi->cmdset_priv;
	__u8 major = extp->MajorVersion;
	__u8 minor = extp->MinorVersion;

	if (((major << 8) | minor) < 0x3131) {
		

		pr_debug("%s: JEDEC Vendor ID is 0x%02X Device ID is 0x%02X\n",
			map->name, cfi->mfr, cfi->id);

		if (((cfi->id == 0xBA) || (cfi->id == 0x22BA)) &&

			(cfi->mfr == CFI_MFR_MACRONIX)) {
			pr_debug("%s: Macronix MX29LV400C with bottom boot block"
				" detected\n", map->name);
			extp->TopBottom = 2;	
		} else
		if (cfi->id & 0x80) {
			printk(KERN_WARNING "%s: JEDEC Device ID is 0x%02X. Assuming broken CFI table.\n", map->name, cfi->id);
			extp->TopBottom = 3;	
		} else {
			extp->TopBottom = 2;	
		}

		pr_debug("%s: AMD CFI PRI V%c.%c has no boot block field;"
			" deduced %s from Device ID\n", map->name, major, minor,
			extp->TopBottom == 2 ? "bottom" : "top");
	}
}
#endif

static void fixup_use_write_buffers(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	if (cfi->cfiq->BufWriteTimeoutTyp) {
		pr_debug("Using buffer write method\n" );
		mtd->_write = cfi_amdstd_write_buffers;
	}
}

static void fixup_convert_atmel_pri(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	struct cfi_pri_amdstd *extp = cfi->cmdset_priv;
	struct cfi_pri_atmel atmel_pri;

	memcpy(&atmel_pri, extp, sizeof(atmel_pri));
	memset((char *)extp + 5, 0, sizeof(*extp) - 5);

	if (atmel_pri.Features & 0x02)
		extp->EraseSuspend = 2;

	
	if (cfi->id == AT49BV6416) {
		if (atmel_pri.BottomBoot)
			extp->TopBottom = 3;
		else
			extp->TopBottom = 2;
	} else {
		if (atmel_pri.BottomBoot)
			extp->TopBottom = 2;
		else
			extp->TopBottom = 3;
	}

	
	cfi->cfiq->BufWriteTimeoutTyp = 0;
	cfi->cfiq->BufWriteTimeoutMax = 0;
}

static void fixup_use_secsi(struct mtd_info *mtd)
{
	
	mtd->_read_user_prot_reg = cfi_amdstd_secsi_read;
	mtd->_read_fact_prot_reg = cfi_amdstd_secsi_read;
}

static void fixup_use_erase_chip(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	if ((cfi->cfiq->NumEraseRegions == 1) &&
		((cfi->cfiq->EraseRegionInfo[0] & 0xffff) == 0)) {
		mtd->_erase = cfi_amdstd_erase_chip;
	}

}

static void fixup_use_atmel_lock(struct mtd_info *mtd)
{
	mtd->_lock = cfi_atmel_lock;
	mtd->_unlock = cfi_atmel_unlock;
	mtd->flags |= MTD_POWERUP_LOCK;
}

static void fixup_old_sst_eraseregion(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	cfi->cfiq->NumEraseRegions = 1;
}

static void fixup_sst39vf(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	fixup_old_sst_eraseregion(mtd);

	cfi->addr_unlock1 = 0x5555;
	cfi->addr_unlock2 = 0x2AAA;
}

static void fixup_sst39vf_rev_b(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	fixup_old_sst_eraseregion(mtd);

	cfi->addr_unlock1 = 0x555;
	cfi->addr_unlock2 = 0x2AA;

	cfi->sector_erase_cmd = CMD(0x50);
}

static void fixup_sst38vf640x_sectorsize(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	fixup_sst39vf_rev_b(mtd);

	cfi->cfiq->EraseRegionInfo[0] = 0x002003ff;
	pr_warning("%s: Bad 38VF640x CFI data; adjusting sector size from 64 to 8KiB\n", mtd->name);
}

static void fixup_s29gl064n_sectors(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	if ((cfi->cfiq->EraseRegionInfo[0] & 0xffff) == 0x003f) {
		cfi->cfiq->EraseRegionInfo[0] |= 0x0040;
		pr_warning("%s: Bad S29GL064N CFI data, adjust from 64 to 128 sectors\n", mtd->name);
	}
}

static void fixup_s29gl032n_sectors(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	if ((cfi->cfiq->EraseRegionInfo[1] & 0xffff) == 0x007e) {
		cfi->cfiq->EraseRegionInfo[1] &= ~0x0040;
		pr_warning("%s: Bad S29GL032N CFI data, adjust from 127 to 63 sectors\n", mtd->name);
	}
}

static struct cfi_fixup cfi_nopri_fixup_table[] = {
	{ CFI_MFR_SST, 0x234a, fixup_sst39vf }, 
	{ CFI_MFR_SST, 0x234b, fixup_sst39vf }, 
	{ CFI_MFR_SST, 0x235a, fixup_sst39vf }, 
	{ CFI_MFR_SST, 0x235b, fixup_sst39vf }, 
	{ CFI_MFR_SST, 0x235c, fixup_sst39vf_rev_b }, 
	{ CFI_MFR_SST, 0x235d, fixup_sst39vf_rev_b }, 
	{ CFI_MFR_SST, 0x236c, fixup_sst39vf_rev_b }, 
	{ CFI_MFR_SST, 0x236d, fixup_sst39vf_rev_b }, 
	{ 0, 0, NULL }
};

static struct cfi_fixup cfi_fixup_table[] = {
	{ CFI_MFR_ATMEL, CFI_ID_ANY, fixup_convert_atmel_pri },
#ifdef AMD_BOOTLOC_BUG
	{ CFI_MFR_AMD, CFI_ID_ANY, fixup_amd_bootblock },
	{ CFI_MFR_AMIC, CFI_ID_ANY, fixup_amd_bootblock },
	{ CFI_MFR_MACRONIX, CFI_ID_ANY, fixup_amd_bootblock },
#endif
	{ CFI_MFR_AMD, 0x0050, fixup_use_secsi },
	{ CFI_MFR_AMD, 0x0053, fixup_use_secsi },
	{ CFI_MFR_AMD, 0x0055, fixup_use_secsi },
	{ CFI_MFR_AMD, 0x0056, fixup_use_secsi },
	{ CFI_MFR_AMD, 0x005C, fixup_use_secsi },
	{ CFI_MFR_AMD, 0x005F, fixup_use_secsi },
	{ CFI_MFR_AMD, 0x0c01, fixup_s29gl064n_sectors },
	{ CFI_MFR_AMD, 0x1301, fixup_s29gl064n_sectors },
	{ CFI_MFR_AMD, 0x1a00, fixup_s29gl032n_sectors },
	{ CFI_MFR_AMD, 0x1a01, fixup_s29gl032n_sectors },
	{ CFI_MFR_SST, 0x536a, fixup_sst38vf640x_sectorsize }, 
	{ CFI_MFR_SST, 0x536b, fixup_sst38vf640x_sectorsize }, 
	{ CFI_MFR_SST, 0x536c, fixup_sst38vf640x_sectorsize }, 
	{ CFI_MFR_SST, 0x536d, fixup_sst38vf640x_sectorsize }, 
#if !FORCE_WORD_WRITE
	{ CFI_MFR_ANY, CFI_ID_ANY, fixup_use_write_buffers },
#endif
	{ 0, 0, NULL }
};
static struct cfi_fixup jedec_fixup_table[] = {
	{ CFI_MFR_SST, SST49LF004B, fixup_use_fwh_lock },
	{ CFI_MFR_SST, SST49LF040B, fixup_use_fwh_lock },
	{ CFI_MFR_SST, SST49LF008A, fixup_use_fwh_lock },
	{ 0, 0, NULL }
};

static struct cfi_fixup fixup_table[] = {
	{ CFI_MFR_ANY, CFI_ID_ANY, fixup_use_erase_chip },
	{ CFI_MFR_ATMEL, AT49BV6416, fixup_use_atmel_lock },
	{ 0, 0, NULL }
};


static void cfi_fixup_major_minor(struct cfi_private *cfi,
				  struct cfi_pri_amdstd *extp)
{
	if (cfi->mfr == CFI_MFR_SAMSUNG) {
		if ((extp->MajorVersion == '0' && extp->MinorVersion == '0') ||
		    (extp->MajorVersion == '3' && extp->MinorVersion == '3')) {
			printk(KERN_NOTICE "  Fixing Samsung's Amd/Fujitsu"
			       " Extended Query version to 1.%c\n",
			       extp->MinorVersion);
			extp->MajorVersion = '1';
		}
	}

	if (cfi->mfr == CFI_MFR_SST && (cfi->id >> 4) == 0x0536) {
		extp->MajorVersion = '1';
		extp->MinorVersion = '0';
	}
}

struct mtd_info *cfi_cmdset_0002(struct map_info *map, int primary)
{
	struct cfi_private *cfi = map->fldrv_priv;
	struct mtd_info *mtd;
	int i;

	mtd = kzalloc(sizeof(*mtd), GFP_KERNEL);
	if (!mtd) {
		printk(KERN_WARNING "Failed to allocate memory for MTD device\n");
		return NULL;
	}
	mtd->priv = map;
	mtd->type = MTD_NORFLASH;

	
	mtd->_erase   = cfi_amdstd_erase_varsize;
	mtd->_write   = cfi_amdstd_write_words;
	mtd->_read    = cfi_amdstd_read;
	mtd->_sync    = cfi_amdstd_sync;
	mtd->_suspend = cfi_amdstd_suspend;
	mtd->_resume  = cfi_amdstd_resume;
	mtd->flags   = MTD_CAP_NORFLASH;
	mtd->name    = map->name;
	mtd->writesize = 1;
	mtd->writebufsize = cfi_interleave(cfi) << cfi->cfiq->MaxBufWriteSize;

	pr_debug("MTD %s(): write buffer size %d\n", __func__,
			mtd->writebufsize);

	mtd->_panic_write = cfi_amdstd_panic_write;
	mtd->reboot_notifier.notifier_call = cfi_amdstd_reboot;

	if (cfi->cfi_mode==CFI_MODE_CFI){
		unsigned char bootloc;
		__u16 adr = primary?cfi->cfiq->P_ADR:cfi->cfiq->A_ADR;
		struct cfi_pri_amdstd *extp;

		extp = (struct cfi_pri_amdstd*)cfi_read_pri(map, adr, sizeof(*extp), "Amd/Fujitsu");
		if (extp) {
			cfi_fixup_major_minor(cfi, extp);

			if (extp->MajorVersion != '1' ||
			    (extp->MajorVersion == '1' && (extp->MinorVersion < '0' || extp->MinorVersion > '5'))) {
				printk(KERN_ERR "  Unknown Amd/Fujitsu Extended Query "
				       "version %c.%c (%#02x/%#02x).\n",
				       extp->MajorVersion, extp->MinorVersion,
				       extp->MajorVersion, extp->MinorVersion);
				kfree(extp);
				kfree(mtd);
				return NULL;
			}

			printk(KERN_INFO "  Amd/Fujitsu Extended Query version %c.%c.\n",
			       extp->MajorVersion, extp->MinorVersion);

			
			cfi->cmdset_priv = extp;

			
			cfi_fixup(mtd, cfi_fixup_table);

#ifdef DEBUG_CFI_FEATURES
			
			cfi_tell_features(extp);
#endif

			bootloc = extp->TopBottom;
			if ((bootloc < 2) || (bootloc > 5)) {
				printk(KERN_WARNING "%s: CFI contains unrecognised boot "
				       "bank location (%d). Assuming bottom.\n",
				       map->name, bootloc);
				bootloc = 2;
			}

			if (bootloc == 3 && cfi->cfiq->NumEraseRegions > 1) {
				printk(KERN_WARNING "%s: Swapping erase regions for top-boot CFI table.\n", map->name);

				for (i=0; i<cfi->cfiq->NumEraseRegions / 2; i++) {
					int j = (cfi->cfiq->NumEraseRegions-1)-i;
					__u32 swap;

					swap = cfi->cfiq->EraseRegionInfo[i];
					cfi->cfiq->EraseRegionInfo[i] = cfi->cfiq->EraseRegionInfo[j];
					cfi->cfiq->EraseRegionInfo[j] = swap;
				}
			}
			
			cfi->addr_unlock1 = 0x555;
			cfi->addr_unlock2 = 0x2aa;
		}
		cfi_fixup(mtd, cfi_nopri_fixup_table);

		if (!cfi->addr_unlock1 || !cfi->addr_unlock2) {
			kfree(mtd);
			return NULL;
		}

	} 
	else if (cfi->cfi_mode == CFI_MODE_JEDEC) {
		
		cfi_fixup(mtd, jedec_fixup_table);
	}
	
	cfi_fixup(mtd, fixup_table);

	for (i=0; i< cfi->numchips; i++) {
		cfi->chips[i].word_write_time = 1<<cfi->cfiq->WordWriteTimeoutTyp;
		cfi->chips[i].buffer_write_time = 1<<cfi->cfiq->BufWriteTimeoutTyp;
		cfi->chips[i].erase_time = 1<<cfi->cfiq->BlockEraseTimeoutTyp;
		cfi->chips[i].ref_point_counter = 0;
		init_waitqueue_head(&(cfi->chips[i].wq));
	}

	map->fldrv = &cfi_amdstd_chipdrv;

	return cfi_amdstd_setup(mtd);
}
struct mtd_info *cfi_cmdset_0006(struct map_info *map, int primary) __attribute__((alias("cfi_cmdset_0002")));
struct mtd_info *cfi_cmdset_0701(struct map_info *map, int primary) __attribute__((alias("cfi_cmdset_0002")));
EXPORT_SYMBOL_GPL(cfi_cmdset_0002);
EXPORT_SYMBOL_GPL(cfi_cmdset_0006);
EXPORT_SYMBOL_GPL(cfi_cmdset_0701);

static struct mtd_info *cfi_amdstd_setup(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long devsize = (1<<cfi->cfiq->DevSize) * cfi->interleave;
	unsigned long offset = 0;
	int i,j;

	printk(KERN_NOTICE "number of %s chips: %d\n",
	       (cfi->cfi_mode == CFI_MODE_CFI)?"CFI":"JEDEC",cfi->numchips);
	
	mtd->size = devsize * cfi->numchips;

	mtd->numeraseregions = cfi->cfiq->NumEraseRegions * cfi->numchips;
	mtd->eraseregions = kmalloc(sizeof(struct mtd_erase_region_info)
				    * mtd->numeraseregions, GFP_KERNEL);
	if (!mtd->eraseregions) {
		printk(KERN_WARNING "Failed to allocate memory for MTD erase region info\n");
		goto setup_err;
	}

	for (i=0; i<cfi->cfiq->NumEraseRegions; i++) {
		unsigned long ernum, ersize;
		ersize = ((cfi->cfiq->EraseRegionInfo[i] >> 8) & ~0xff) * cfi->interleave;
		ernum = (cfi->cfiq->EraseRegionInfo[i] & 0xffff) + 1;

		if (mtd->erasesize < ersize) {
			mtd->erasesize = ersize;
		}
		for (j=0; j<cfi->numchips; j++) {
			mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].offset = (j*devsize)+offset;
			mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].erasesize = ersize;
			mtd->eraseregions[(j*cfi->cfiq->NumEraseRegions)+i].numblocks = ernum;
		}
		offset += (ersize * ernum);
	}
	if (offset != devsize) {
		
		printk(KERN_WARNING "Sum of regions (%lx) != total size of set of interleaved chips (%lx)\n", offset, devsize);
		goto setup_err;
	}

	__module_get(THIS_MODULE);
	register_reboot_notifier(&mtd->reboot_notifier);
	return mtd;

 setup_err:
	kfree(mtd->eraseregions);
	kfree(mtd);
	kfree(cfi->cmdset_priv);
	kfree(cfi->cfiq);
	return NULL;
}

static int __xipram chip_ready(struct map_info *map, unsigned long addr)
{
	map_word d, t;

	d = map_read(map, addr);
	t = map_read(map, addr);

	return map_word_equal(map, d, t);
}

static int __xipram chip_good(struct map_info *map, unsigned long addr, map_word expected)
{
	map_word oldd, curd;

	oldd = map_read(map, addr);
	curd = map_read(map, addr);

	return	map_word_equal(map, oldd, curd) &&
		map_word_equal(map, curd, expected);
}

static int get_chip(struct map_info *map, struct flchip *chip, unsigned long adr, int mode)
{
	DECLARE_WAITQUEUE(wait, current);
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long timeo;
	struct cfi_pri_amdstd *cfip = (struct cfi_pri_amdstd *)cfi->cmdset_priv;

 resettime:
	timeo = jiffies + HZ;
 retry:
	switch (chip->state) {

	case FL_STATUS:
		for (;;) {
			if (chip_ready(map, adr))
				break;

			if (time_after(jiffies, timeo)) {
				printk(KERN_ERR "Waiting for chip to be ready timed out.\n");
				return -EIO;
			}
			mutex_unlock(&chip->mutex);
			cfi_udelay(1);
			mutex_lock(&chip->mutex);
			
			goto retry;
		}

	case FL_READY:
	case FL_CFI_QUERY:
	case FL_JEDEC_QUERY:
		return 0;

	case FL_ERASING:
		if (!cfip || !(cfip->EraseSuspend & (0x1|0x2)) ||
		    !(mode == FL_READY || mode == FL_POINT ||
		    (mode == FL_WRITING && (cfip->EraseSuspend & 0x2))))
			goto sleep;


		
		map_write(map, CMD(0xB0), chip->in_progress_block_addr);
		chip->oldstate = FL_ERASING;
		chip->state = FL_ERASE_SUSPENDING;
		chip->erase_suspended = 1;
		for (;;) {
			if (chip_ready(map, adr))
				break;

			if (time_after(jiffies, timeo)) {
				put_chip(map, chip, adr);
				printk(KERN_ERR "MTD %s(): chip not ready after erase suspend\n", __func__);
				return -EIO;
			}

			mutex_unlock(&chip->mutex);
			cfi_udelay(1);
			mutex_lock(&chip->mutex);
		}
		chip->state = FL_READY;
		return 0;

	case FL_XIP_WHILE_ERASING:
		if (mode != FL_READY && mode != FL_POINT &&
		    (!cfip || !(cfip->EraseSuspend&2)))
			goto sleep;
		chip->oldstate = chip->state;
		chip->state = FL_READY;
		return 0;

	case FL_SHUTDOWN:
		
		return -EIO;

	case FL_POINT:
		
		if (mode == FL_READY && chip->oldstate == FL_READY)
			return 0;

	default:
	sleep:
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
		mutex_unlock(&chip->mutex);
		schedule();
		remove_wait_queue(&chip->wq, &wait);
		mutex_lock(&chip->mutex);
		goto resettime;
	}
}


static void put_chip(struct map_info *map, struct flchip *chip, unsigned long adr)
{
	struct cfi_private *cfi = map->fldrv_priv;

	switch(chip->oldstate) {
	case FL_ERASING:
		map_write(map, cfi->sector_erase_cmd, chip->in_progress_block_addr);
		chip->oldstate = FL_READY;
		chip->state = FL_ERASING;
		break;

	case FL_XIP_WHILE_ERASING:
		chip->state = chip->oldstate;
		chip->oldstate = FL_READY;
		break;

	case FL_READY:
	case FL_STATUS:
		break;
	default:
		printk(KERN_ERR "MTD: put_chip() called with oldstate %d!!\n", chip->oldstate);
	}
	wake_up(&chip->wq);
}

#ifdef CONFIG_MTD_XIP


static void xip_disable(struct map_info *map, struct flchip *chip,
			unsigned long adr)
{
	
	(void) map_read(map, adr); 
	local_irq_disable();
}

static void __xipram xip_enable(struct map_info *map, struct flchip *chip,
				unsigned long adr)
{
	struct cfi_private *cfi = map->fldrv_priv;

	if (chip->state != FL_POINT && chip->state != FL_READY) {
		map_write(map, CMD(0xf0), adr);
		chip->state = FL_READY;
	}
	(void) map_read(map, adr);
	xip_iprefetch();
	local_irq_enable();
}


static void __xipram xip_udelay(struct map_info *map, struct flchip *chip,
				unsigned long adr, int usec)
{
	struct cfi_private *cfi = map->fldrv_priv;
	struct cfi_pri_amdstd *extp = cfi->cmdset_priv;
	map_word status, OK = CMD(0x80);
	unsigned long suspended, start = xip_currtime();
	flstate_t oldstate;

	do {
		cpu_relax();
		if (xip_irqpending() && extp &&
		    ((chip->state == FL_ERASING && (extp->EraseSuspend & 2))) &&
		    (cfi_interleave_is_1(cfi) || chip->oldstate == FL_READY)) {
			map_write(map, CMD(0xb0), adr);
			usec -= xip_elapsed_since(start);
			suspended = xip_currtime();
			do {
				if (xip_elapsed_since(suspended) > 100000) {
					return;
				}
				status = map_read(map, adr);
			} while (!map_word_andequal(map, status, OK, OK));

			
			oldstate = chip->state;
			if (!map_word_bitsset(map, status, CMD(0x40)))
				break;
			chip->state = FL_XIP_WHILE_ERASING;
			chip->erase_suspended = 1;
			map_write(map, CMD(0xf0), adr);
			(void) map_read(map, adr);
			xip_iprefetch();
			local_irq_enable();
			mutex_unlock(&chip->mutex);
			xip_iprefetch();
			cond_resched();

			mutex_lock(&chip->mutex);
			while (chip->state != FL_XIP_WHILE_ERASING) {
				DECLARE_WAITQUEUE(wait, current);
				set_current_state(TASK_UNINTERRUPTIBLE);
				add_wait_queue(&chip->wq, &wait);
				mutex_unlock(&chip->mutex);
				schedule();
				remove_wait_queue(&chip->wq, &wait);
				mutex_lock(&chip->mutex);
			}
			
			local_irq_disable();

			
			map_write(map, cfi->sector_erase_cmd, adr);
			chip->state = oldstate;
			start = xip_currtime();
		} else if (usec >= 1000000/HZ) {
			xip_cpu_idle();
		}
		status = map_read(map, adr);
	} while (!map_word_andequal(map, status, OK, OK)
		 && xip_elapsed_since(start) < usec);
}

#define UDELAY(map, chip, adr, usec)  xip_udelay(map, chip, adr, usec)

#define XIP_INVAL_CACHED_RANGE(map, from, size)  \
	INVALIDATE_CACHED_RANGE(map, from, size)

#define INVALIDATE_CACHE_UDELAY(map, chip, adr, len, usec)  \
	UDELAY(map, chip, adr, usec)


#else

#define xip_disable(map, chip, adr)
#define xip_enable(map, chip, adr)
#define XIP_INVAL_CACHED_RANGE(x...)

#define UDELAY(map, chip, adr, usec)  \
do {  \
	mutex_unlock(&chip->mutex);  \
	cfi_udelay(usec);  \
	mutex_lock(&chip->mutex);  \
} while (0)

#define INVALIDATE_CACHE_UDELAY(map, chip, adr, len, usec)  \
do {  \
	mutex_unlock(&chip->mutex);  \
	INVALIDATE_CACHED_RANGE(map, adr, len);  \
	cfi_udelay(usec);  \
	mutex_lock(&chip->mutex);  \
} while (0)

#endif

static inline int do_read_onechip(struct map_info *map, struct flchip *chip, loff_t adr, size_t len, u_char *buf)
{
	unsigned long cmd_addr;
	struct cfi_private *cfi = map->fldrv_priv;
	int ret;

	adr += chip->start;

	
	cmd_addr = adr & ~(map_bankwidth(map)-1);

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, cmd_addr, FL_READY);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	if (chip->state != FL_POINT && chip->state != FL_READY) {
		map_write(map, CMD(0xf0), cmd_addr);
		chip->state = FL_READY;
	}

	map_copy_from(map, buf, adr, len);

	put_chip(map, chip, cmd_addr);

	mutex_unlock(&chip->mutex);
	return 0;
}


static int cfi_amdstd_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long ofs;
	int chipnum;
	int ret = 0;

	
	chipnum = (from >> cfi->chipshift);
	ofs = from - (chipnum <<  cfi->chipshift);

	while (len) {
		unsigned long thislen;

		if (chipnum >= cfi->numchips)
			break;

		if ((len + ofs -1) >> cfi->chipshift)
			thislen = (1<<cfi->chipshift) - ofs;
		else
			thislen = len;

		ret = do_read_onechip(map, &cfi->chips[chipnum], ofs, thislen, buf);
		if (ret)
			break;

		*retlen += thislen;
		len -= thislen;
		buf += thislen;

		ofs = 0;
		chipnum++;
	}
	return ret;
}


static inline int do_read_secsi_onechip(struct map_info *map, struct flchip *chip, loff_t adr, size_t len, u_char *buf)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long timeo = jiffies + HZ;
	struct cfi_private *cfi = map->fldrv_priv;

 retry:
	mutex_lock(&chip->mutex);

	if (chip->state != FL_READY){
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);

		mutex_unlock(&chip->mutex);

		schedule();
		remove_wait_queue(&chip->wq, &wait);
		timeo = jiffies + HZ;

		goto retry;
	}

	adr += chip->start;

	chip->state = FL_READY;

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x88, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);

	map_copy_from(map, buf, adr, len);

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x90, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x00, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);

	wake_up(&chip->wq);
	mutex_unlock(&chip->mutex);

	return 0;
}

static int cfi_amdstd_secsi_read (struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long ofs;
	int chipnum;
	int ret = 0;

	
	
	chipnum=from>>3;
	ofs=from & 7;

	while (len) {
		unsigned long thislen;

		if (chipnum >= cfi->numchips)
			break;

		if ((len + ofs -1) >> 3)
			thislen = (1<<3) - ofs;
		else
			thislen = len;

		ret = do_read_secsi_onechip(map, &cfi->chips[chipnum], ofs, thislen, buf);
		if (ret)
			break;

		*retlen += thislen;
		len -= thislen;
		buf += thislen;

		ofs = 0;
		chipnum++;
	}
	return ret;
}


static int __xipram do_write_oneword(struct map_info *map, struct flchip *chip, unsigned long adr, map_word datum)
{
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long timeo = jiffies + HZ;
	unsigned long uWriteTimeout = ( HZ / 1000 ) + 1;
	int ret = 0;
	map_word oldd;
	int retry_cnt = 0;

	adr += chip->start;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr, FL_WRITING);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	pr_debug("MTD %s(): WRITE 0x%.8lx(0x%.8lx)\n",
	       __func__, adr, datum.x[0] );

	/*
	 * Check for a NOP for the case when the datum to write is already
	 * present - it saves time and works around buggy chips that corrupt
	 * data at other locations when 0xff is written to a location that
	 * already contains 0xff.
	 */
	oldd = map_read(map, adr);
	if (map_word_equal(map, oldd, datum)) {
		pr_debug("MTD %s(): NOP\n",
		       __func__);
		goto op_done;
	}

	XIP_INVAL_CACHED_RANGE(map, adr, map_bankwidth(map));
	ENABLE_VPP(map);
	xip_disable(map, chip, adr);
 retry:
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0xA0, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	map_write(map, datum, adr);
	chip->state = FL_WRITING;

	INVALIDATE_CACHE_UDELAY(map, chip,
				adr, map_bankwidth(map),
				chip->word_write_time);

	
	timeo = jiffies + uWriteTimeout;
	for (;;) {
		if (chip->state != FL_WRITING) {
			
			DECLARE_WAITQUEUE(wait, current);

			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			mutex_unlock(&chip->mutex);
			schedule();
			remove_wait_queue(&chip->wq, &wait);
			timeo = jiffies + (HZ / 2); 
			mutex_lock(&chip->mutex);
			continue;
		}

		if (time_after(jiffies, timeo) && !chip_ready(map, adr)){
			xip_enable(map, chip, adr);
			printk(KERN_WARNING "MTD %s(): software timeout\n", __func__);
			xip_disable(map, chip, adr);
			break;
		}

		if (chip_ready(map, adr))
			break;

		
		UDELAY(map, chip, adr, 1);
	}
	
	if (!chip_good(map, adr, datum)) {
		
		map_write( map, CMD(0xF0), chip->start );
		

		if (++retry_cnt <= MAX_WORD_RETRIES)
			goto retry;

		ret = -EIO;
	}
	xip_enable(map, chip, adr);
 op_done:
	chip->state = FL_READY;
	DISABLE_VPP(map);
	put_chip(map, chip, adr);
	mutex_unlock(&chip->mutex);

	return ret;
}


static int cfi_amdstd_write_words(struct mtd_info *mtd, loff_t to, size_t len,
				  size_t *retlen, const u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int ret = 0;
	int chipnum;
	unsigned long ofs, chipstart;
	DECLARE_WAITQUEUE(wait, current);

	chipnum = to >> cfi->chipshift;
	ofs = to  - (chipnum << cfi->chipshift);
	chipstart = cfi->chips[chipnum].start;

	
	if (ofs & (map_bankwidth(map)-1)) {
		unsigned long bus_ofs = ofs & ~(map_bankwidth(map)-1);
		int i = ofs - bus_ofs;
		int n = 0;
		map_word tmp_buf;

 retry:
		mutex_lock(&cfi->chips[chipnum].mutex);

		if (cfi->chips[chipnum].state != FL_READY) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&cfi->chips[chipnum].wq, &wait);

			mutex_unlock(&cfi->chips[chipnum].mutex);

			schedule();
			remove_wait_queue(&cfi->chips[chipnum].wq, &wait);
			goto retry;
		}

		
		tmp_buf = map_read(map, bus_ofs+chipstart);

		mutex_unlock(&cfi->chips[chipnum].mutex);

		
		n = min_t(int, len, map_bankwidth(map)-i);

		tmp_buf = map_word_load_partial(map, tmp_buf, buf, i, n);

		ret = do_write_oneword(map, &cfi->chips[chipnum],
				       bus_ofs, tmp_buf);
		if (ret)
			return ret;

		ofs += n;
		buf += n;
		(*retlen) += n;
		len -= n;

		if (ofs >> cfi->chipshift) {
			chipnum ++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
		}
	}

	
	while(len >= map_bankwidth(map)) {
		map_word datum;

		datum = map_word_load(map, buf);

		ret = do_write_oneword(map, &cfi->chips[chipnum],
				       ofs, datum);
		if (ret)
			return ret;

		ofs += map_bankwidth(map);
		buf += map_bankwidth(map);
		(*retlen) += map_bankwidth(map);
		len -= map_bankwidth(map);

		if (ofs >> cfi->chipshift) {
			chipnum ++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
			chipstart = cfi->chips[chipnum].start;
		}
	}

	
	if (len & (map_bankwidth(map)-1)) {
		map_word tmp_buf;

 retry1:
		mutex_lock(&cfi->chips[chipnum].mutex);

		if (cfi->chips[chipnum].state != FL_READY) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&cfi->chips[chipnum].wq, &wait);

			mutex_unlock(&cfi->chips[chipnum].mutex);

			schedule();
			remove_wait_queue(&cfi->chips[chipnum].wq, &wait);
			goto retry1;
		}

		tmp_buf = map_read(map, ofs + chipstart);

		mutex_unlock(&cfi->chips[chipnum].mutex);

		tmp_buf = map_word_load_partial(map, tmp_buf, buf, 0, len);

		ret = do_write_oneword(map, &cfi->chips[chipnum],
				ofs, tmp_buf);
		if (ret)
			return ret;

		(*retlen) += len;
	}

	return 0;
}


static int __xipram do_write_buffer(struct map_info *map, struct flchip *chip,
				    unsigned long adr, const u_char *buf,
				    int len)
{
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long timeo = jiffies + HZ;
	
	unsigned long uWriteTimeout = ( HZ / 1000 ) + 1;
	int ret = -EIO;
	unsigned long cmd_adr;
	int z, words;
	map_word datum;

	adr += chip->start;
	cmd_adr = adr;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr, FL_WRITING);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	datum = map_word_load(map, buf);

	pr_debug("MTD %s(): WRITE 0x%.8lx(0x%.8lx)\n",
	       __func__, adr, datum.x[0] );

	XIP_INVAL_CACHED_RANGE(map, adr, len);
	ENABLE_VPP(map);
	xip_disable(map, chip, cmd_adr);

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);

	
	map_write(map, CMD(0x25), cmd_adr);

	chip->state = FL_WRITING_TO_BUFFER;

	
	words = len / map_bankwidth(map);
	map_write(map, CMD(words - 1), cmd_adr);
	
	z = 0;
	while(z < words * map_bankwidth(map)) {
		datum = map_word_load(map, buf);
		map_write(map, datum, adr + z);

		z += map_bankwidth(map);
		buf += map_bankwidth(map);
	}
	z -= map_bankwidth(map);

	adr += z;

	
	map_write(map, CMD(0x29), cmd_adr);
	chip->state = FL_WRITING;

	INVALIDATE_CACHE_UDELAY(map, chip,
				adr, map_bankwidth(map),
				chip->word_write_time);

	timeo = jiffies + uWriteTimeout;

	for (;;) {
		if (chip->state != FL_WRITING) {
			
			DECLARE_WAITQUEUE(wait, current);

			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			mutex_unlock(&chip->mutex);
			schedule();
			remove_wait_queue(&chip->wq, &wait);
			timeo = jiffies + (HZ / 2); 
			mutex_lock(&chip->mutex);
			continue;
		}

		if (time_after(jiffies, timeo) && !chip_ready(map, adr))
			break;

		if (chip_ready(map, adr)) {
			xip_enable(map, chip, adr);
			goto op_done;
		}

		
		UDELAY(map, chip, adr, 1);
	}

	
	map_write( map, CMD(0xF0), chip->start );
	xip_enable(map, chip, adr);
	

	printk(KERN_WARNING "MTD %s(): software timeout\n",
	       __func__ );

	ret = -EIO;
 op_done:
	chip->state = FL_READY;
	DISABLE_VPP(map);
	put_chip(map, chip, adr);
	mutex_unlock(&chip->mutex);

	return ret;
}


static int cfi_amdstd_write_buffers(struct mtd_info *mtd, loff_t to, size_t len,
				    size_t *retlen, const u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int wbufsize = cfi_interleave(cfi) << cfi->cfiq->MaxBufWriteSize;
	int ret = 0;
	int chipnum;
	unsigned long ofs;

	chipnum = to >> cfi->chipshift;
	ofs = to  - (chipnum << cfi->chipshift);

	
	if (ofs & (map_bankwidth(map)-1)) {
		size_t local_len = (-ofs)&(map_bankwidth(map)-1);
		if (local_len > len)
			local_len = len;
		ret = cfi_amdstd_write_words(mtd, ofs + (chipnum<<cfi->chipshift),
					     local_len, retlen, buf);
		if (ret)
			return ret;
		ofs += local_len;
		buf += local_len;
		len -= local_len;

		if (ofs >> cfi->chipshift) {
			chipnum ++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
		}
	}

	
	while (len >= map_bankwidth(map) * 2) {
		
		int size = wbufsize - (ofs & (wbufsize-1));

		if (size > len)
			size = len;
		if (size % map_bankwidth(map))
			size -= size % map_bankwidth(map);

		ret = do_write_buffer(map, &cfi->chips[chipnum],
				      ofs, buf, size);
		if (ret)
			return ret;

		ofs += size;
		buf += size;
		(*retlen) += size;
		len -= size;

		if (ofs >> cfi->chipshift) {
			chipnum ++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
		}
	}

	if (len) {
		size_t retlen_dregs = 0;

		ret = cfi_amdstd_write_words(mtd, ofs + (chipnum<<cfi->chipshift),
					     len, &retlen_dregs, buf);

		*retlen += retlen_dregs;
		return ret;
	}

	return 0;
}

static int cfi_amdstd_panic_wait(struct map_info *map, struct flchip *chip,
				 unsigned long adr)
{
	struct cfi_private *cfi = map->fldrv_priv;
	int retries = 10;
	int i;

	if (chip->state == FL_READY && chip_ready(map, adr))
		return 0;

	while (retries > 0) {
		const unsigned long timeo = (HZ / 1000) + 1;

		
		map_write(map, CMD(0xF0), chip->start);

		
		for (i = 0; i < jiffies_to_usecs(timeo); i++) {
			if (chip_ready(map, adr))
				return 0;

			udelay(1);
		}
	}

	
	return -EBUSY;
}

static int do_panic_write_oneword(struct map_info *map, struct flchip *chip,
				  unsigned long adr, map_word datum)
{
	const unsigned long uWriteTimeout = (HZ / 1000) + 1;
	struct cfi_private *cfi = map->fldrv_priv;
	int retry_cnt = 0;
	map_word oldd;
	int ret = 0;
	int i;

	adr += chip->start;

	ret = cfi_amdstd_panic_wait(map, chip, adr);
	if (ret)
		return ret;

	pr_debug("MTD %s(): PANIC WRITE 0x%.8lx(0x%.8lx)\n",
			__func__, adr, datum.x[0]);

	/*
	 * Check for a NOP for the case when the datum to write is already
	 * present - it saves time and works around buggy chips that corrupt
	 * data at other locations when 0xff is written to a location that
	 * already contains 0xff.
	 */
	oldd = map_read(map, adr);
	if (map_word_equal(map, oldd, datum)) {
		pr_debug("MTD %s(): NOP\n", __func__);
		goto op_done;
	}

	ENABLE_VPP(map);

retry:
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0xA0, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	map_write(map, datum, adr);

	for (i = 0; i < jiffies_to_usecs(uWriteTimeout); i++) {
		if (chip_ready(map, adr))
			break;

		udelay(1);
	}

	if (!chip_good(map, adr, datum)) {
		
		map_write(map, CMD(0xF0), chip->start);
		

		if (++retry_cnt <= MAX_WORD_RETRIES)
			goto retry;

		ret = -EIO;
	}

op_done:
	DISABLE_VPP(map);
	return ret;
}

/*
 * Write out some data during a kernel panic
 *
 * This is used by the mtdoops driver to save the dying messages from a
 * kernel which has panic'd.
 *
 * This routine ignores all of the locking used throughout the rest of the
 * driver, in order to ensure that the data gets written out no matter what
 * state this driver (and the flash chip itself) was in when the kernel crashed.
 *
 * The implementation of this routine is intentionally similar to
 * cfi_amdstd_write_words(), in order to ease code maintenance.
 */
static int cfi_amdstd_panic_write(struct mtd_info *mtd, loff_t to, size_t len,
				  size_t *retlen, const u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long ofs, chipstart;
	int ret = 0;
	int chipnum;

	chipnum = to >> cfi->chipshift;
	ofs = to - (chipnum << cfi->chipshift);
	chipstart = cfi->chips[chipnum].start;

	
	if (ofs & (map_bankwidth(map) - 1)) {
		unsigned long bus_ofs = ofs & ~(map_bankwidth(map) - 1);
		int i = ofs - bus_ofs;
		int n = 0;
		map_word tmp_buf;

		ret = cfi_amdstd_panic_wait(map, &cfi->chips[chipnum], bus_ofs);
		if (ret)
			return ret;

		
		tmp_buf = map_read(map, bus_ofs + chipstart);

		
		n = min_t(int, len, map_bankwidth(map) - i);

		tmp_buf = map_word_load_partial(map, tmp_buf, buf, i, n);

		ret = do_panic_write_oneword(map, &cfi->chips[chipnum],
					     bus_ofs, tmp_buf);
		if (ret)
			return ret;

		ofs += n;
		buf += n;
		(*retlen) += n;
		len -= n;

		if (ofs >> cfi->chipshift) {
			chipnum++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;
		}
	}

	
	while (len >= map_bankwidth(map)) {
		map_word datum;

		datum = map_word_load(map, buf);

		ret = do_panic_write_oneword(map, &cfi->chips[chipnum],
					     ofs, datum);
		if (ret)
			return ret;

		ofs += map_bankwidth(map);
		buf += map_bankwidth(map);
		(*retlen) += map_bankwidth(map);
		len -= map_bankwidth(map);

		if (ofs >> cfi->chipshift) {
			chipnum++;
			ofs = 0;
			if (chipnum == cfi->numchips)
				return 0;

			chipstart = cfi->chips[chipnum].start;
		}
	}

	
	if (len & (map_bankwidth(map) - 1)) {
		map_word tmp_buf;

		ret = cfi_amdstd_panic_wait(map, &cfi->chips[chipnum], ofs);
		if (ret)
			return ret;

		tmp_buf = map_read(map, ofs + chipstart);

		tmp_buf = map_word_load_partial(map, tmp_buf, buf, 0, len);

		ret = do_panic_write_oneword(map, &cfi->chips[chipnum],
					     ofs, tmp_buf);
		if (ret)
			return ret;

		(*retlen) += len;
	}

	return 0;
}


static int __xipram do_erase_chip(struct map_info *map, struct flchip *chip)
{
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long timeo = jiffies + HZ;
	unsigned long int adr;
	DECLARE_WAITQUEUE(wait, current);
	int ret = 0;

	adr = cfi->addr_unlock1;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr, FL_WRITING);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	pr_debug("MTD %s(): ERASE 0x%.8lx\n",
	       __func__, chip->start );

	XIP_INVAL_CACHED_RANGE(map, adr, map->size);
	ENABLE_VPP(map);
	xip_disable(map, chip, adr);

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x80, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x10, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);

	chip->state = FL_ERASING;
	chip->erase_suspended = 0;
	chip->in_progress_block_addr = adr;

	INVALIDATE_CACHE_UDELAY(map, chip,
				adr, map->size,
				chip->erase_time*500);

	timeo = jiffies + (HZ*20);

	for (;;) {
		if (chip->state != FL_ERASING) {
			
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			mutex_unlock(&chip->mutex);
			schedule();
			remove_wait_queue(&chip->wq, &wait);
			mutex_lock(&chip->mutex);
			continue;
		}
		if (chip->erase_suspended) {
			timeo = jiffies + (HZ*20); 
			chip->erase_suspended = 0;
		}

		if (chip_ready(map, adr))
			break;

		if (time_after(jiffies, timeo)) {
			printk(KERN_WARNING "MTD %s(): software timeout\n",
				__func__ );
			break;
		}

		
		UDELAY(map, chip, adr, 1000000/HZ);
	}
	
	if (!chip_good(map, adr, map_word_ff(map))) {
		
		map_write( map, CMD(0xF0), chip->start );
		

		ret = -EIO;
	}

	chip->state = FL_READY;
	xip_enable(map, chip, adr);
	DISABLE_VPP(map);
	put_chip(map, chip, adr);
	mutex_unlock(&chip->mutex);

	return ret;
}


static int __xipram do_erase_oneblock(struct map_info *map, struct flchip *chip, unsigned long adr, int len, void *thunk)
{
	struct cfi_private *cfi = map->fldrv_priv;
	unsigned long timeo = jiffies + HZ;
	DECLARE_WAITQUEUE(wait, current);
	int ret = 0;

	adr += chip->start;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr, FL_ERASING);
	if (ret) {
		mutex_unlock(&chip->mutex);
		return ret;
	}

	pr_debug("MTD %s(): ERASE 0x%.8lx\n",
	       __func__, adr );

	XIP_INVAL_CACHED_RANGE(map, adr, len);
	ENABLE_VPP(map);
	xip_disable(map, chip, adr);

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x80, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi, cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi, cfi->device_type, NULL);
	map_write(map, cfi->sector_erase_cmd, adr);

	chip->state = FL_ERASING;
	chip->erase_suspended = 0;
	chip->in_progress_block_addr = adr;

	INVALIDATE_CACHE_UDELAY(map, chip,
				adr, len,
				chip->erase_time*500);

	timeo = jiffies + (HZ*20);

	for (;;) {
		if (chip->state != FL_ERASING) {
			
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);
			mutex_unlock(&chip->mutex);
			schedule();
			remove_wait_queue(&chip->wq, &wait);
			mutex_lock(&chip->mutex);
			continue;
		}
		if (chip->erase_suspended) {
			timeo = jiffies + (HZ*20); 
			chip->erase_suspended = 0;
		}

		if (chip_ready(map, adr)) {
			xip_enable(map, chip, adr);
			break;
		}

		if (time_after(jiffies, timeo)) {
			xip_enable(map, chip, adr);
			printk(KERN_WARNING "MTD %s(): software timeout\n",
				__func__ );
			break;
		}

		
		UDELAY(map, chip, adr, 1000000/HZ);
	}
	
	if (!chip_good(map, adr, map_word_ff(map))) {
		
		map_write( map, CMD(0xF0), chip->start );
		

		ret = -EIO;
	}

	chip->state = FL_READY;
	DISABLE_VPP(map);
	put_chip(map, chip, adr);
	mutex_unlock(&chip->mutex);
	return ret;
}


static int cfi_amdstd_erase_varsize(struct mtd_info *mtd, struct erase_info *instr)
{
	unsigned long ofs, len;
	int ret;

	ofs = instr->addr;
	len = instr->len;

	ret = cfi_varsize_frob(mtd, do_erase_oneblock, ofs, len, NULL);
	if (ret)
		return ret;

	instr->state = MTD_ERASE_DONE;
	mtd_erase_callback(instr);

	return 0;
}


static int cfi_amdstd_erase_chip(struct mtd_info *mtd, struct erase_info *instr)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int ret = 0;

	if (instr->addr != 0)
		return -EINVAL;

	if (instr->len != mtd->size)
		return -EINVAL;

	ret = do_erase_chip(map, &cfi->chips[0]);
	if (ret)
		return ret;

	instr->state = MTD_ERASE_DONE;
	mtd_erase_callback(instr);

	return 0;
}

static int do_atmel_lock(struct map_info *map, struct flchip *chip,
			 unsigned long adr, int len, void *thunk)
{
	struct cfi_private *cfi = map->fldrv_priv;
	int ret;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr + chip->start, FL_LOCKING);
	if (ret)
		goto out_unlock;
	chip->state = FL_LOCKING;

	pr_debug("MTD %s(): LOCK 0x%08lx len %d\n", __func__, adr, len);

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x80, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	cfi_send_gen_cmd(0x55, cfi->addr_unlock2, chip->start, map, cfi,
			 cfi->device_type, NULL);
	map_write(map, CMD(0x40), chip->start + adr);

	chip->state = FL_READY;
	put_chip(map, chip, adr + chip->start);
	ret = 0;

out_unlock:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int do_atmel_unlock(struct map_info *map, struct flchip *chip,
			   unsigned long adr, int len, void *thunk)
{
	struct cfi_private *cfi = map->fldrv_priv;
	int ret;

	mutex_lock(&chip->mutex);
	ret = get_chip(map, chip, adr + chip->start, FL_UNLOCKING);
	if (ret)
		goto out_unlock;
	chip->state = FL_UNLOCKING;

	pr_debug("MTD %s(): LOCK 0x%08lx len %d\n", __func__, adr, len);

	cfi_send_gen_cmd(0xAA, cfi->addr_unlock1, chip->start, map, cfi,
			 cfi->device_type, NULL);
	map_write(map, CMD(0x70), adr);

	chip->state = FL_READY;
	put_chip(map, chip, adr + chip->start);
	ret = 0;

out_unlock:
	mutex_unlock(&chip->mutex);
	return ret;
}

static int cfi_atmel_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	return cfi_varsize_frob(mtd, do_atmel_lock, ofs, len, NULL);
}

static int cfi_atmel_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	return cfi_varsize_frob(mtd, do_atmel_unlock, ofs, len, NULL);
}


static void cfi_amdstd_sync (struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;
	int ret = 0;
	DECLARE_WAITQUEUE(wait, current);

	for (i=0; !ret && i<cfi->numchips; i++) {
		chip = &cfi->chips[i];

	retry:
		mutex_lock(&chip->mutex);

		switch(chip->state) {
		case FL_READY:
		case FL_STATUS:
		case FL_CFI_QUERY:
		case FL_JEDEC_QUERY:
			chip->oldstate = chip->state;
			chip->state = FL_SYNCING;
		case FL_SYNCING:
			mutex_unlock(&chip->mutex);
			break;

		default:
			
			set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&chip->wq, &wait);

			mutex_unlock(&chip->mutex);

			schedule();

			remove_wait_queue(&chip->wq, &wait);

			goto retry;
		}
	}

	

	for (i--; i >=0; i--) {
		chip = &cfi->chips[i];

		mutex_lock(&chip->mutex);

		if (chip->state == FL_SYNCING) {
			chip->state = chip->oldstate;
			wake_up(&chip->wq);
		}
		mutex_unlock(&chip->mutex);
	}
}


static int cfi_amdstd_suspend(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;
	int ret = 0;

	for (i=0; !ret && i<cfi->numchips; i++) {
		chip = &cfi->chips[i];

		mutex_lock(&chip->mutex);

		switch(chip->state) {
		case FL_READY:
		case FL_STATUS:
		case FL_CFI_QUERY:
		case FL_JEDEC_QUERY:
			chip->oldstate = chip->state;
			chip->state = FL_PM_SUSPENDED;
		case FL_PM_SUSPENDED:
			break;

		default:
			ret = -EAGAIN;
			break;
		}
		mutex_unlock(&chip->mutex);
	}

	

	if (ret) {
		for (i--; i >=0; i--) {
			chip = &cfi->chips[i];

			mutex_lock(&chip->mutex);

			if (chip->state == FL_PM_SUSPENDED) {
				chip->state = chip->oldstate;
				wake_up(&chip->wq);
			}
			mutex_unlock(&chip->mutex);
		}
	}

	return ret;
}


static void cfi_amdstd_resume(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i;
	struct flchip *chip;

	for (i=0; i<cfi->numchips; i++) {

		chip = &cfi->chips[i];

		mutex_lock(&chip->mutex);

		if (chip->state == FL_PM_SUSPENDED) {
			chip->state = FL_READY;
			map_write(map, CMD(0xF0), chip->start);
			wake_up(&chip->wq);
		}
		else
			printk(KERN_ERR "Argh. Chip not in PM_SUSPENDED state upon resume()\n");

		mutex_unlock(&chip->mutex);
	}
}


static int cfi_amdstd_reset(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;
	int i, ret;
	struct flchip *chip;

	for (i = 0; i < cfi->numchips; i++) {

		chip = &cfi->chips[i];

		mutex_lock(&chip->mutex);

		ret = get_chip(map, chip, chip->start, FL_SHUTDOWN);
		if (!ret) {
			map_write(map, CMD(0xF0), chip->start);
			chip->state = FL_SHUTDOWN;
			put_chip(map, chip, chip->start);
		}

		mutex_unlock(&chip->mutex);
	}

	return 0;
}


static int cfi_amdstd_reboot(struct notifier_block *nb, unsigned long val,
			       void *v)
{
	struct mtd_info *mtd;

	mtd = container_of(nb, struct mtd_info, reboot_notifier);
	cfi_amdstd_reset(mtd);
	return NOTIFY_DONE;
}


static void cfi_amdstd_destroy(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct cfi_private *cfi = map->fldrv_priv;

	cfi_amdstd_reset(mtd);
	unregister_reboot_notifier(&mtd->reboot_notifier);
	kfree(cfi->cmdset_priv);
	kfree(cfi->cfiq);
	kfree(cfi);
	kfree(mtd->eraseregions);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Crossnet Co. <info@crossnet.co.jp> et al.");
MODULE_DESCRIPTION("MTD chip driver for AMD/Fujitsu flash chips");
MODULE_ALIAS("cfi_cmdset_0006");
MODULE_ALIAS("cfi_cmdset_0701");
