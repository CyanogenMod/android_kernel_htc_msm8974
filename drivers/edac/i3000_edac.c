/*
 * Intel 3000/3010 Memory Controller kernel module
 * Copyright (C) 2007 Akamai Technologies, Inc.
 * Shamelessly copied from:
 * 	Intel D82875P Memory Controller kernel module
 * 	(C) 2003 Linux Networx (http://lnxi.com)
 *
 * This file may be distributed under the terms of the
 * GNU General Public License.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/edac.h>
#include "edac_core.h"

#define I3000_REVISION		"1.1"

#define EDAC_MOD_STR		"i3000_edac"

#define I3000_RANKS		8
#define I3000_RANKS_PER_CHANNEL	4
#define I3000_CHANNELS		2


#define I3000_MCHBAR		0x44	
#define I3000_MCHBAR_MASK	0xffffc000
#define I3000_MMR_WINDOW_SIZE	16384

#define I3000_EDEAP	0x70	
#define I3000_DEAP	0x58	
#define I3000_DEAP_GRAIN 		(1 << 7)


static inline unsigned long deap_pfn(u8 edeap, u32 deap)
{
	deap >>= PAGE_SHIFT;
	deap |= (edeap & 1) << (32 - PAGE_SHIFT);
	return deap;
}

static inline unsigned long deap_offset(u32 deap)
{
	return deap & ~(I3000_DEAP_GRAIN - 1) & ~PAGE_MASK;
}

static inline int deap_channel(u32 deap)
{
	return deap & 1;
}

#define I3000_DERRSYN	0x5c	

#define I3000_ERRSTS	0xc8	
#define I3000_ERRSTS_BITS	0x0b03	
#define I3000_ERRSTS_UE		0x0002
#define I3000_ERRSTS_CE		0x0001

#define I3000_ERRCMD	0xca	


#define I3000_DRB_SHIFT 25	

#define I3000_C0DRB	0x100	
#define I3000_C1DRB	0x180	

#define I3000_C0DRA	0x108	
#define I3000_C1DRA	0x188	

static inline unsigned char odd_rank_attrib(unsigned char dra)
{
	return (dra & 0x70) >> 4;
}

static inline unsigned char even_rank_attrib(unsigned char dra)
{
	return dra & 0x07;
}

#define I3000_C0DRC0	0x120	

#define I3000_C0DRC1	0x124	

enum i3000p_chips {
	I3000 = 0,
};

struct i3000_dev_info {
	const char *ctl_name;
};

struct i3000_error_info {
	u16 errsts;
	u8 derrsyn;
	u8 edeap;
	u32 deap;
	u16 errsts2;
};

static const struct i3000_dev_info i3000_devs[] = {
	[I3000] = {
		.ctl_name = "i3000"},
};

static struct pci_dev *mci_pdev;
static int i3000_registered = 1;
static struct edac_pci_ctl_info *i3000_pci;

static void i3000_get_error_info(struct mem_ctl_info *mci,
				 struct i3000_error_info *info)
{
	struct pci_dev *pdev;

	pdev = to_pci_dev(mci->dev);

	/*
	 * This is a mess because there is no atomic way to read all the
	 * registers at once and the registers can transition from CE being
	 * overwritten by UE.
	 */
	pci_read_config_word(pdev, I3000_ERRSTS, &info->errsts);
	if (!(info->errsts & I3000_ERRSTS_BITS))
		return;
	pci_read_config_byte(pdev, I3000_EDEAP, &info->edeap);
	pci_read_config_dword(pdev, I3000_DEAP, &info->deap);
	pci_read_config_byte(pdev, I3000_DERRSYN, &info->derrsyn);
	pci_read_config_word(pdev, I3000_ERRSTS, &info->errsts2);

	if ((info->errsts ^ info->errsts2) & I3000_ERRSTS_BITS) {
		pci_read_config_byte(pdev, I3000_EDEAP, &info->edeap);
		pci_read_config_dword(pdev, I3000_DEAP, &info->deap);
		pci_read_config_byte(pdev, I3000_DERRSYN, &info->derrsyn);
	}

	pci_write_bits16(pdev, I3000_ERRSTS, I3000_ERRSTS_BITS,
			 I3000_ERRSTS_BITS);
}

static int i3000_process_error_info(struct mem_ctl_info *mci,
				struct i3000_error_info *info,
				int handle_errors)
{
	int row, multi_chan, channel;
	unsigned long pfn, offset;

	multi_chan = mci->csrows[0].nr_channels - 1;

	if (!(info->errsts & I3000_ERRSTS_BITS))
		return 0;

	if (!handle_errors)
		return 1;

	if ((info->errsts ^ info->errsts2) & I3000_ERRSTS_BITS) {
		edac_mc_handle_ce_no_info(mci, "UE overwrote CE");
		info->errsts = info->errsts2;
	}

	pfn = deap_pfn(info->edeap, info->deap);
	offset = deap_offset(info->deap);
	channel = deap_channel(info->deap);

	row = edac_mc_find_csrow_by_page(mci, pfn);

	if (info->errsts & I3000_ERRSTS_UE)
		edac_mc_handle_ue(mci, pfn, offset, row, "i3000 UE");
	else
		edac_mc_handle_ce(mci, pfn, offset, info->derrsyn, row,
				multi_chan ? channel : 0, "i3000 CE");

	return 1;
}

static void i3000_check(struct mem_ctl_info *mci)
{
	struct i3000_error_info info;

	debugf1("MC%d: %s()\n", mci->mc_idx, __func__);
	i3000_get_error_info(mci, &info);
	i3000_process_error_info(mci, &info, 1);
}

static int i3000_is_interleaved(const unsigned char *c0dra,
				const unsigned char *c1dra,
				const unsigned char *c0drb,
				const unsigned char *c1drb)
{
	int i;

	for (i = 0; i < I3000_RANKS_PER_CHANNEL / 2; i++)
		if (odd_rank_attrib(c0dra[i]) != odd_rank_attrib(c1dra[i]) ||
			even_rank_attrib(c0dra[i]) !=
						even_rank_attrib(c1dra[i]))
			return 0;

	for (i = 0; i < I3000_RANKS_PER_CHANNEL; i++)
		if (c0drb[i] != c1drb[i])
			return 0;

	return 1;
}

static int i3000_probe1(struct pci_dev *pdev, int dev_idx)
{
	int rc;
	int i;
	struct mem_ctl_info *mci = NULL;
	unsigned long last_cumul_size;
	int interleaved, nr_channels;
	unsigned char dra[I3000_RANKS / 2], drb[I3000_RANKS];
	unsigned char *c0dra = dra, *c1dra = &dra[I3000_RANKS_PER_CHANNEL / 2];
	unsigned char *c0drb = drb, *c1drb = &drb[I3000_RANKS_PER_CHANNEL];
	unsigned long mchbar;
	void __iomem *window;

	debugf0("MC: %s()\n", __func__);

	pci_read_config_dword(pdev, I3000_MCHBAR, (u32 *) & mchbar);
	mchbar &= I3000_MCHBAR_MASK;
	window = ioremap_nocache(mchbar, I3000_MMR_WINDOW_SIZE);
	if (!window) {
		printk(KERN_ERR "i3000: cannot map mmio space at 0x%lx\n",
			mchbar);
		return -ENODEV;
	}

	c0dra[0] = readb(window + I3000_C0DRA + 0);	
	c0dra[1] = readb(window + I3000_C0DRA + 1);	
	c1dra[0] = readb(window + I3000_C1DRA + 0);	
	c1dra[1] = readb(window + I3000_C1DRA + 1);	

	for (i = 0; i < I3000_RANKS_PER_CHANNEL; i++) {
		c0drb[i] = readb(window + I3000_C0DRB + i);
		c1drb[i] = readb(window + I3000_C1DRB + i);
	}

	iounmap(window);

	interleaved = i3000_is_interleaved(c0dra, c1dra, c0drb, c1drb);
	nr_channels = interleaved ? 2 : 1;
	mci = edac_mc_alloc(0, I3000_RANKS / nr_channels, nr_channels, 0);
	if (!mci)
		return -ENOMEM;

	debugf3("MC: %s(): init mci\n", __func__);

	mci->dev = &pdev->dev;
	mci->mtype_cap = MEM_FLAG_DDR2;

	mci->edac_ctl_cap = EDAC_FLAG_SECDED;
	mci->edac_cap = EDAC_FLAG_SECDED;

	mci->mod_name = EDAC_MOD_STR;
	mci->mod_ver = I3000_REVISION;
	mci->ctl_name = i3000_devs[dev_idx].ctl_name;
	mci->dev_name = pci_name(pdev);
	mci->edac_check = i3000_check;
	mci->ctl_page_to_phys = NULL;

	for (last_cumul_size = i = 0; i < mci->nr_csrows; i++) {
		u8 value;
		u32 cumul_size;
		struct csrow_info *csrow = &mci->csrows[i];

		value = drb[i];
		cumul_size = value << (I3000_DRB_SHIFT - PAGE_SHIFT);
		if (interleaved)
			cumul_size <<= 1;
		debugf3("MC: %s(): (%d) cumul_size 0x%x\n",
			__func__, i, cumul_size);
		if (cumul_size == last_cumul_size) {
			csrow->mtype = MEM_EMPTY;
			continue;
		}

		csrow->first_page = last_cumul_size;
		csrow->last_page = cumul_size - 1;
		csrow->nr_pages = cumul_size - last_cumul_size;
		last_cumul_size = cumul_size;
		csrow->grain = I3000_DEAP_GRAIN;
		csrow->mtype = MEM_DDR2;
		csrow->dtype = DEV_UNKNOWN;
		csrow->edac_mode = EDAC_UNKNOWN;
	}

	pci_write_bits16(pdev, I3000_ERRSTS, I3000_ERRSTS_BITS,
			 I3000_ERRSTS_BITS);

	rc = -ENODEV;
	if (edac_mc_add_mc(mci)) {
		debugf3("MC: %s(): failed edac_mc_add_mc()\n", __func__);
		goto fail;
	}

	
	i3000_pci = edac_pci_create_generic_ctl(&pdev->dev, EDAC_MOD_STR);
	if (!i3000_pci) {
		printk(KERN_WARNING
			"%s(): Unable to create PCI control\n",
			__func__);
		printk(KERN_WARNING
			"%s(): PCI error report via EDAC not setup\n",
			__func__);
	}

	
	debugf3("MC: %s(): success\n", __func__);
	return 0;

fail:
	if (mci)
		edac_mc_free(mci);

	return rc;
}

static int __devinit i3000_init_one(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	int rc;

	debugf0("MC: %s()\n", __func__);

	if (pci_enable_device(pdev) < 0)
		return -EIO;

	rc = i3000_probe1(pdev, ent->driver_data);
	if (!mci_pdev)
		mci_pdev = pci_dev_get(pdev);

	return rc;
}

static void __devexit i3000_remove_one(struct pci_dev *pdev)
{
	struct mem_ctl_info *mci;

	debugf0("%s()\n", __func__);

	if (i3000_pci)
		edac_pci_release_generic_ctl(i3000_pci);

	mci = edac_mc_del_mc(&pdev->dev);
	if (!mci)
		return;

	edac_mc_free(mci);
}

static DEFINE_PCI_DEVICE_TABLE(i3000_pci_tbl) = {
	{
	 PCI_VEND_DEV(INTEL, 3000_HB), PCI_ANY_ID, PCI_ANY_ID, 0, 0,
	 I3000},
	{
	 0,
	 }			
};

MODULE_DEVICE_TABLE(pci, i3000_pci_tbl);

static struct pci_driver i3000_driver = {
	.name = EDAC_MOD_STR,
	.probe = i3000_init_one,
	.remove = __devexit_p(i3000_remove_one),
	.id_table = i3000_pci_tbl,
};

static int __init i3000_init(void)
{
	int pci_rc;

	debugf3("MC: %s()\n", __func__);

       
       opstate_init();

	pci_rc = pci_register_driver(&i3000_driver);
	if (pci_rc < 0)
		goto fail0;

	if (!mci_pdev) {
		i3000_registered = 0;
		mci_pdev = pci_get_device(PCI_VENDOR_ID_INTEL,
					PCI_DEVICE_ID_INTEL_3000_HB, NULL);
		if (!mci_pdev) {
			debugf0("i3000 pci_get_device fail\n");
			pci_rc = -ENODEV;
			goto fail1;
		}

		pci_rc = i3000_init_one(mci_pdev, i3000_pci_tbl);
		if (pci_rc < 0) {
			debugf0("i3000 init fail\n");
			pci_rc = -ENODEV;
			goto fail1;
		}
	}

	return 0;

fail1:
	pci_unregister_driver(&i3000_driver);

fail0:
	if (mci_pdev)
		pci_dev_put(mci_pdev);

	return pci_rc;
}

static void __exit i3000_exit(void)
{
	debugf3("MC: %s()\n", __func__);

	pci_unregister_driver(&i3000_driver);
	if (!i3000_registered) {
		i3000_remove_one(mci_pdev);
		pci_dev_put(mci_pdev);
	}
}

module_init(i3000_init);
module_exit(i3000_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Akamai Technologies Arthur Ulfeldt/Jason Uhlenkott");
MODULE_DESCRIPTION("MC support for Intel 3000 memory hub controllers");

module_param(edac_op_state, int, 0444);
MODULE_PARM_DESC(edac_op_state, "EDAC Error Reporting state: 0=Poll,1=NMI");
