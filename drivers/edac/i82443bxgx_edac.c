/*
 * Intel 82443BX/GX (440BX/GX chipset) Memory Controller EDAC kernel
 * module (C) 2006 Tim Small
 *
 * This file may be distributed under the terms of the GNU General
 * Public License.
 *
 * Written by Tim Small <tim@buttersideup.com>, based on work by Linux
 * Networx, Thayne Harbaugh, Dan Hollis <goemon at anime dot net> and
 * others.
 *
 * 440GX fix by Jason Uhlenkott <juhlenko@akamai.com>.
 *
 * Written with reference to 82443BX Host Bridge Datasheet:
 * http://download.intel.com/design/chipsets/datashts/29063301.pdf 
 * references to this document given in [].
 *
 * This module doesn't support the 440LX, but it may be possible to
 * make it do so (the 440LX's register definitions are different, but
 * not completely so - I haven't studied them in enough detail to know
 * how easy this would be).
 */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/pci.h>
#include <linux/pci_ids.h>


#include <linux/edac.h>
#include "edac_core.h"

#define I82443_REVISION	"0.1"

#define EDAC_MOD_STR    "i82443bxgx_edac"




#define I82443BXGX_NR_CSROWS 8
#define I82443BXGX_NR_CHANS  1
#define I82443BXGX_NR_DIMMS  4

#define I82443BXGX_NBXCFG 0x50	
#define I82443BXGX_NBXCFG_OFFSET_NON_ECCROW 24	
#define I82443BXGX_NBXCFG_OFFSET_DRAM_FREQ 12	

#define I82443BXGX_NBXCFG_OFFSET_DRAM_INTEGRITY 7	
#define I82443BXGX_NBXCFG_INTEGRITY_NONE   0x0	
#define I82443BXGX_NBXCFG_INTEGRITY_EC     0x1	
#define I82443BXGX_NBXCFG_INTEGRITY_ECC    0x2	
#define I82443BXGX_NBXCFG_INTEGRITY_SCRUB  0x3	

#define I82443BXGX_NBXCFG_OFFSET_ECC_DIAG_ENABLE  6

#define I82443BXGX_EAP   0x80	
#define I82443BXGX_EAP_OFFSET_EAP  12	
#define I82443BXGX_EAP_OFFSET_MBE  BIT(1)	
#define I82443BXGX_EAP_OFFSET_SBE  BIT(0)	

#define I82443BXGX_ERRCMD  0x90	
#define I82443BXGX_ERRCMD_OFFSET_SERR_ON_MBE BIT(1)	
#define I82443BXGX_ERRCMD_OFFSET_SERR_ON_SBE BIT(0)	

#define I82443BXGX_ERRSTS  0x91	
#define I82443BXGX_ERRSTS_OFFSET_MBFRE 5	
#define I82443BXGX_ERRSTS_OFFSET_MEF   BIT(4)	
#define I82443BXGX_ERRSTS_OFFSET_SBFRE 1	
#define I82443BXGX_ERRSTS_OFFSET_SEF   BIT(0)	

#define I82443BXGX_DRAMC 0x57	
#define I82443BXGX_DRAMC_OFFSET_DT 3	
#define I82443BXGX_DRAMC_DRAM_IS_EDO 0	
#define I82443BXGX_DRAMC_DRAM_IS_SDRAM 1	
#define I82443BXGX_DRAMC_DRAM_IS_RSDRAM 2	

#define I82443BXGX_DRB 0x60	


struct i82443bxgx_edacmc_error_info {
	u32 eap;
};

static struct edac_pci_ctl_info *i82443bxgx_pci;

static struct pci_dev *mci_pdev;	

static int i82443bxgx_registered = 1;

static void i82443bxgx_edacmc_get_error_info(struct mem_ctl_info *mci,
				struct i82443bxgx_edacmc_error_info
				*info)
{
	struct pci_dev *pdev;
	pdev = to_pci_dev(mci->dev);
	pci_read_config_dword(pdev, I82443BXGX_EAP, &info->eap);
	if (info->eap & I82443BXGX_EAP_OFFSET_SBE)
		
		pci_write_bits32(pdev, I82443BXGX_EAP,
				 I82443BXGX_EAP_OFFSET_SBE,
				 I82443BXGX_EAP_OFFSET_SBE);

	if (info->eap & I82443BXGX_EAP_OFFSET_MBE)
		
		pci_write_bits32(pdev, I82443BXGX_EAP,
				 I82443BXGX_EAP_OFFSET_MBE,
				 I82443BXGX_EAP_OFFSET_MBE);
}

static int i82443bxgx_edacmc_process_error_info(struct mem_ctl_info *mci,
						struct
						i82443bxgx_edacmc_error_info
						*info, int handle_errors)
{
	int error_found = 0;
	u32 eapaddr, page, pageoffset;

	eapaddr = (info->eap & 0xfffff000);
	page = eapaddr >> PAGE_SHIFT;
	pageoffset = eapaddr - (page << PAGE_SHIFT);

	if (info->eap & I82443BXGX_EAP_OFFSET_SBE) {
		error_found = 1;
		if (handle_errors)
			edac_mc_handle_ce(mci, page, pageoffset,
				0, edac_mc_find_csrow_by_page(mci, page), 0,
				mci->ctl_name);
	}

	if (info->eap & I82443BXGX_EAP_OFFSET_MBE) {
		error_found = 1;
		if (handle_errors)
			edac_mc_handle_ue(mci, page, pageoffset,
					edac_mc_find_csrow_by_page(mci, page),
					mci->ctl_name);
	}

	return error_found;
}

static void i82443bxgx_edacmc_check(struct mem_ctl_info *mci)
{
	struct i82443bxgx_edacmc_error_info info;

	debugf1("MC%d: %s: %s()\n", mci->mc_idx, __FILE__, __func__);
	i82443bxgx_edacmc_get_error_info(mci, &info);
	i82443bxgx_edacmc_process_error_info(mci, &info, 1);
}

static void i82443bxgx_init_csrows(struct mem_ctl_info *mci,
				struct pci_dev *pdev,
				enum edac_type edac_mode,
				enum mem_type mtype)
{
	struct csrow_info *csrow;
	int index;
	u8 drbar, dramc;
	u32 row_base, row_high_limit, row_high_limit_last;

	pci_read_config_byte(pdev, I82443BXGX_DRAMC, &dramc);
	row_high_limit_last = 0;
	for (index = 0; index < mci->nr_csrows; index++) {
		csrow = &mci->csrows[index];
		pci_read_config_byte(pdev, I82443BXGX_DRB + index, &drbar);
		debugf1("MC%d: %s: %s() Row=%d DRB = %#0x\n",
			mci->mc_idx, __FILE__, __func__, index, drbar);
		row_high_limit = ((u32) drbar << 23);
		
		debugf1("MC%d: %s: %s() Row=%d, "
			"Boundary Address=%#0x, Last = %#0x\n",
			mci->mc_idx, __FILE__, __func__, index, row_high_limit,
			row_high_limit_last);

		
		if (row_high_limit_last && !row_high_limit)
			row_high_limit = 1UL << 31;

		
		if (row_high_limit == row_high_limit_last)
			continue;
		row_base = row_high_limit_last;
		csrow->first_page = row_base >> PAGE_SHIFT;
		csrow->last_page = (row_high_limit >> PAGE_SHIFT) - 1;
		csrow->nr_pages = csrow->last_page - csrow->first_page + 1;
		
		csrow->grain = 1 << 12;
		csrow->mtype = mtype;
		
		csrow->dtype = DEV_UNKNOWN;
		
		csrow->edac_mode = edac_mode;
		row_high_limit_last = row_high_limit;
	}
}

static int i82443bxgx_edacmc_probe1(struct pci_dev *pdev, int dev_idx)
{
	struct mem_ctl_info *mci;
	u8 dramc;
	u32 nbxcfg, ecc_mode;
	enum mem_type mtype;
	enum edac_type edac_mode;

	debugf0("MC: %s: %s()\n", __FILE__, __func__);

	if (pci_read_config_dword(pdev, I82443BXGX_NBXCFG, &nbxcfg))
		return -EIO;

	mci = edac_mc_alloc(0, I82443BXGX_NR_CSROWS, I82443BXGX_NR_CHANS, 0);

	if (mci == NULL)
		return -ENOMEM;

	debugf0("MC: %s: %s(): mci = %p\n", __FILE__, __func__, mci);
	mci->dev = &pdev->dev;
	mci->mtype_cap = MEM_FLAG_EDO | MEM_FLAG_SDR | MEM_FLAG_RDR;
	mci->edac_ctl_cap = EDAC_FLAG_NONE | EDAC_FLAG_EC | EDAC_FLAG_SECDED;
	pci_read_config_byte(pdev, I82443BXGX_DRAMC, &dramc);
	switch ((dramc >> I82443BXGX_DRAMC_OFFSET_DT) & (BIT(0) | BIT(1))) {
	case I82443BXGX_DRAMC_DRAM_IS_EDO:
		mtype = MEM_EDO;
		break;
	case I82443BXGX_DRAMC_DRAM_IS_SDRAM:
		mtype = MEM_SDR;
		break;
	case I82443BXGX_DRAMC_DRAM_IS_RSDRAM:
		mtype = MEM_RDR;
		break;
	default:
		debugf0("Unknown/reserved DRAM type value "
			"in DRAMC register!\n");
		mtype = -MEM_UNKNOWN;
	}

	if ((mtype == MEM_SDR) || (mtype == MEM_RDR))
		mci->edac_cap = mci->edac_ctl_cap;
	else
		mci->edac_cap = EDAC_FLAG_NONE;

	mci->scrub_cap = SCRUB_FLAG_HW_SRC;
	pci_read_config_dword(pdev, I82443BXGX_NBXCFG, &nbxcfg);
	ecc_mode = ((nbxcfg >> I82443BXGX_NBXCFG_OFFSET_DRAM_INTEGRITY) &
		(BIT(0) | BIT(1)));

	mci->scrub_mode = (ecc_mode == I82443BXGX_NBXCFG_INTEGRITY_SCRUB)
		? SCRUB_HW_SRC : SCRUB_NONE;

	switch (ecc_mode) {
	case I82443BXGX_NBXCFG_INTEGRITY_NONE:
		edac_mode = EDAC_NONE;
		break;
	case I82443BXGX_NBXCFG_INTEGRITY_EC:
		edac_mode = EDAC_EC;
		break;
	case I82443BXGX_NBXCFG_INTEGRITY_ECC:
	case I82443BXGX_NBXCFG_INTEGRITY_SCRUB:
		edac_mode = EDAC_SECDED;
		break;
	default:
		debugf0("%s(): Unknown/reserved ECC state "
			"in NBXCFG register!\n", __func__);
		edac_mode = EDAC_UNKNOWN;
		break;
	}

	i82443bxgx_init_csrows(mci, pdev, edac_mode, mtype);

	pci_write_bits32(pdev, I82443BXGX_EAP,
			(I82443BXGX_EAP_OFFSET_SBE |
				I82443BXGX_EAP_OFFSET_MBE),
			(I82443BXGX_EAP_OFFSET_SBE |
				I82443BXGX_EAP_OFFSET_MBE));

	mci->mod_name = EDAC_MOD_STR;
	mci->mod_ver = I82443_REVISION;
	mci->ctl_name = "I82443BXGX";
	mci->dev_name = pci_name(pdev);
	mci->edac_check = i82443bxgx_edacmc_check;
	mci->ctl_page_to_phys = NULL;

	if (edac_mc_add_mc(mci)) {
		debugf3("%s(): failed edac_mc_add_mc()\n", __func__);
		goto fail;
	}

	
	i82443bxgx_pci = edac_pci_create_generic_ctl(&pdev->dev, EDAC_MOD_STR);
	if (!i82443bxgx_pci) {
		printk(KERN_WARNING
			"%s(): Unable to create PCI control\n",
			__func__);
		printk(KERN_WARNING
			"%s(): PCI error report via EDAC not setup\n",
			__func__);
	}

	debugf3("MC: %s: %s(): success\n", __FILE__, __func__);
	return 0;

fail:
	edac_mc_free(mci);
	return -ENODEV;
}

EXPORT_SYMBOL_GPL(i82443bxgx_edacmc_probe1);

static int __devinit i82443bxgx_edacmc_init_one(struct pci_dev *pdev,
						const struct pci_device_id *ent)
{
	int rc;

	debugf0("MC: %s: %s()\n", __FILE__, __func__);

	
	rc = i82443bxgx_edacmc_probe1(pdev, ent->driver_data);

	if (mci_pdev == NULL)
		mci_pdev = pci_dev_get(pdev);

	return rc;
}

static void __devexit i82443bxgx_edacmc_remove_one(struct pci_dev *pdev)
{
	struct mem_ctl_info *mci;

	debugf0("%s: %s()\n", __FILE__, __func__);

	if (i82443bxgx_pci)
		edac_pci_release_generic_ctl(i82443bxgx_pci);

	if ((mci = edac_mc_del_mc(&pdev->dev)) == NULL)
		return;

	edac_mc_free(mci);
}

EXPORT_SYMBOL_GPL(i82443bxgx_edacmc_remove_one);

static DEFINE_PCI_DEVICE_TABLE(i82443bxgx_pci_tbl) = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82443BX_0)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82443BX_2)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82443GX_0)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82443GX_2)},
	{0,}			
};

MODULE_DEVICE_TABLE(pci, i82443bxgx_pci_tbl);

static struct pci_driver i82443bxgx_edacmc_driver = {
	.name = EDAC_MOD_STR,
	.probe = i82443bxgx_edacmc_init_one,
	.remove = __devexit_p(i82443bxgx_edacmc_remove_one),
	.id_table = i82443bxgx_pci_tbl,
};

static int __init i82443bxgx_edacmc_init(void)
{
	int pci_rc;
       
       opstate_init();

	pci_rc = pci_register_driver(&i82443bxgx_edacmc_driver);
	if (pci_rc < 0)
		goto fail0;

	if (mci_pdev == NULL) {
		const struct pci_device_id *id = &i82443bxgx_pci_tbl[0];
		int i = 0;
		i82443bxgx_registered = 0;

		while (mci_pdev == NULL && id->vendor != 0) {
			mci_pdev = pci_get_device(id->vendor,
					id->device, NULL);
			i++;
			id = &i82443bxgx_pci_tbl[i];
		}
		if (!mci_pdev) {
			debugf0("i82443bxgx pci_get_device fail\n");
			pci_rc = -ENODEV;
			goto fail1;
		}

		pci_rc = i82443bxgx_edacmc_init_one(mci_pdev, i82443bxgx_pci_tbl);

		if (pci_rc < 0) {
			debugf0("i82443bxgx init fail\n");
			pci_rc = -ENODEV;
			goto fail1;
		}
	}

	return 0;

fail1:
	pci_unregister_driver(&i82443bxgx_edacmc_driver);

fail0:
	if (mci_pdev != NULL)
		pci_dev_put(mci_pdev);

	return pci_rc;
}

static void __exit i82443bxgx_edacmc_exit(void)
{
	pci_unregister_driver(&i82443bxgx_edacmc_driver);

	if (!i82443bxgx_registered)
		i82443bxgx_edacmc_remove_one(mci_pdev);

	if (mci_pdev)
		pci_dev_put(mci_pdev);
}

module_init(i82443bxgx_edacmc_init);
module_exit(i82443bxgx_edacmc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Small <tim@buttersideup.com> - WPAD");
MODULE_DESCRIPTION("EDAC MC support for Intel 82443BX/GX memory controllers");

module_param(edac_op_state, int, 0444);
MODULE_PARM_DESC(edac_op_state, "EDAC Error Reporting state: 0=Poll,1=NMI");
