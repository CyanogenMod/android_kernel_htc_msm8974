/*
 * Intel 5400 class Memory Controllers kernel module (Seaburg)
 *
 * This file may be distributed under the terms of the
 * GNU General Public License.
 *
 * Copyright (c) 2008 by:
 *	 Ben Woodard <woodard@redhat.com>
 *	 Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * Red Hat Inc. http://www.redhat.com
 *
 * Forked and adapted from the i5000_edac driver which was
 * written by Douglas Thompson Linux Networx <norsk5@xmission.com>
 *
 * This module is based on the following document:
 *
 * Intel 5400 Chipset Memory Controller Hub (MCH) - Datasheet
 * 	http://developer.intel.com/design/chipsets/datashts/313070.htm
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/slab.h>
#include <linux/edac.h>
#include <linux/mmzone.h>

#include "edac_core.h"

#define I5400_REVISION    " Ver: 1.0.0"

#define EDAC_MOD_STR      "i5400_edac"

#define i5400_printk(level, fmt, arg...) \
	edac_printk(level, "i5400", fmt, ##arg)

#define i5400_mc_printk(mci, level, fmt, arg...) \
	edac_mc_chipset_printk(mci, level, "i5400", fmt, ##arg)

#define NUM_MTRS_PER_BRANCH	4
#define CHANNELS_PER_BRANCH	2
#define MAX_DIMMS_PER_CHANNEL	NUM_MTRS_PER_BRANCH
#define	MAX_CHANNELS		4
#define MAX_CSROWS		(MAX_DIMMS_PER_CHANNEL)


	
#define		AMBASE			0x48 
#define		MAXCH			0x56 
#define		MAXDIMMPERCH		0x57 

	
#define		TOLM			0x6C
#define		REDMEMB			0x7C
#define			REC_ECC_LOCATOR_ODD(x)	((x) & 0x3fe00) 
#define		MIR0			0x80
#define		MIR1			0x84
#define		AMIR0			0x8c
#define		AMIR1			0x90

	
#define		FERR_FAT_FBD		0x98	
#define			FERR_FAT_FBDCHAN (3<<28)	

#define		NERR_FAT_FBD		0x9c
#define		FERR_NF_FBD		0xa0	

	
#define		NERR_NF_FBD		0xa4

	
#define		EMASK_FBD		0xa8

#define		ERR0_FBD		0xac
#define		ERR1_FBD		0xb0
#define		ERR2_FBD		0xb4
#define		MCERR_FBD		0xb8

	


	
#define AMBPRESENT_0	0x64
#define AMBPRESENT_1	0x66
#define MTR0		0x80
#define MTR1		0x82
#define MTR2		0x84
#define MTR3		0x86

	
#define NRECFGLOG		0x74
#define RECFGLOG		0x78
#define NRECMEMA		0xbe
#define NRECMEMB		0xc0
#define NRECFB_DIMMA		0xc4
#define NRECFB_DIMMB		0xc8
#define NRECFB_DIMMC		0xcc
#define NRECFB_DIMMD		0xd0
#define NRECFB_DIMME		0xd4
#define NRECFB_DIMMF		0xd8
#define REDMEMA			0xdC
#define RECMEMA			0xf0
#define RECMEMB			0xf4
#define RECFB_DIMMA		0xf8
#define RECFB_DIMMB		0xec
#define RECFB_DIMMC		0xf0
#define RECFB_DIMMD		0xf4
#define RECFB_DIMME		0xf8
#define RECFB_DIMMF		0xfC


enum error_mask {
	EMASK_M1  = 1<<0,  
	EMASK_M2  = 1<<1,  
	EMASK_M3  = 1<<2,  
	EMASK_M4  = 1<<3,  
	EMASK_M5  = 1<<4,  
	EMASK_M6  = 1<<5,  
	EMASK_M7  = 1<<6,  
	EMASK_M8  = 1<<7,  
	EMASK_M9  = 1<<8,  
	EMASK_M10 = 1<<9,  
	EMASK_M11 = 1<<10, 
	EMASK_M12 = 1<<11, 
	EMASK_M13 = 1<<12, 
	EMASK_M14 = 1<<13, 
	EMASK_M15 = 1<<14, 
	EMASK_M16 = 1<<15, 
	EMASK_M17 = 1<<16, 
	EMASK_M18 = 1<<17, 
	EMASK_M19 = 1<<18, 
	EMASK_M20 = 1<<19, 
	EMASK_M21 = 1<<20, 
	EMASK_M22 = 1<<21, 
	EMASK_M23 = 1<<22, 
	EMASK_M24 = 1<<23, 
	EMASK_M25 = 1<<24, 
	EMASK_M26 = 1<<25, 
	EMASK_M27 = 1<<26, 
	EMASK_M28 = 1<<27, 
	EMASK_M29 = 1<<28, 
};

static const char *error_name[] = {
	[0]  = "Memory Write error on non-redundant retry",
	[1]  = "Memory or FB-DIMM configuration CRC read error",
	
	[3]  = "Uncorrectable Data ECC on Replay",
	[4]  = "Aliased Uncorrectable Non-Mirrored Demand Data ECC",
	
	[6]  = "Aliased Uncorrectable Resilver- or Spare-Copy Data ECC",
	[7]  = "Aliased Uncorrectable Patrol Data ECC",
	[8]  = "Non-Aliased Uncorrectable Non-Mirrored Demand Data ECC",
	
	[10] = "Non-Aliased Uncorrectable Resilver- or Spare-Copy Data ECC",
	[11] = "Non-Aliased Uncorrectable Patrol Data ECC",
	[12] = "Memory Write error on first attempt",
	[13] = "FB-DIMM Configuration Write error on first attempt",
	[14] = "Memory or FB-DIMM configuration CRC read error",
	[15] = "Channel Failed-Over Occurred",
	[16] = "Correctable Non-Mirrored Demand Data ECC",
	
	[18] = "Correctable Resilver- or Spare-Copy Data ECC",
	[19] = "Correctable Patrol Data ECC",
	[20] = "FB-DIMM Northbound parity error on FB-DIMM Sync Status",
	[21] = "SPD protocol Error",
	[22] = "Non-Redundant Fast Reset Timeout",
	[23] = "Refresh error",
	[24] = "Memory Write error on redundant retry",
	[25] = "Redundant Fast Reset Timeout",
	[26] = "Correctable Counter Threshold Exceeded",
	[27] = "DIMM-Spare Copy Completed",
	[28] = "DIMM-Isolation Completed",
};

#define ERROR_FAT_MASK		(EMASK_M1 | \
				 EMASK_M2 | \
				 EMASK_M23)

#define ERROR_NF_CORRECTABLE	(EMASK_M27 | \
				 EMASK_M20 | \
				 EMASK_M19 | \
				 EMASK_M18 | \
				 EMASK_M17 | \
				 EMASK_M16)
#define ERROR_NF_DIMM_SPARE	(EMASK_M29 | \
				 EMASK_M28)
#define ERROR_NF_SPD_PROTOCOL	(EMASK_M22)
#define ERROR_NF_NORTH_CRC	(EMASK_M21)

#define ERROR_NF_RECOVERABLE	(EMASK_M26 | \
				 EMASK_M25 | \
				 EMASK_M24 | \
				 EMASK_M15 | \
				 EMASK_M14 | \
				 EMASK_M13 | \
				 EMASK_M12 | \
				 EMASK_M11 | \
				 EMASK_M9  | \
				 EMASK_M8  | \
				 EMASK_M7  | \
				 EMASK_M5)

#define ERROR_NF_UNCORRECTABLE	(EMASK_M4)

#define ERROR_NF_MASK		(ERROR_NF_CORRECTABLE   | \
				 ERROR_NF_UNCORRECTABLE | \
				 ERROR_NF_RECOVERABLE   | \
				 ERROR_NF_DIMM_SPARE    | \
				 ERROR_NF_SPD_PROTOCOL  | \
				 ERROR_NF_NORTH_CRC)


#define ENABLE_EMASK_ALL	(ERROR_FAT_MASK | ERROR_NF_MASK)

#define FERR_FAT_MASK ERROR_FAT_MASK

static inline int to_nf_mask(unsigned int mask)
{
	return (mask & EMASK_M29) | (mask >> 3);
};

static inline int from_nf_ferr(unsigned int mask)
{
	return (mask & EMASK_M29) |		
	       (mask & ((1 << 28) - 1) << 3);	
};

#define FERR_NF_MASK		to_nf_mask(ERROR_NF_MASK)
#define FERR_NF_CORRECTABLE	to_nf_mask(ERROR_NF_CORRECTABLE)
#define FERR_NF_DIMM_SPARE	to_nf_mask(ERROR_NF_DIMM_SPARE)
#define FERR_NF_SPD_PROTOCOL	to_nf_mask(ERROR_NF_SPD_PROTOCOL)
#define FERR_NF_NORTH_CRC	to_nf_mask(ERROR_NF_NORTH_CRC)
#define FERR_NF_RECOVERABLE	to_nf_mask(ERROR_NF_RECOVERABLE)
#define FERR_NF_UNCORRECTABLE	to_nf_mask(ERROR_NF_UNCORRECTABLE)

#define MTR_DIMMS_PRESENT(mtr)		((mtr) & (1 << 10))
#define MTR_DIMMS_ETHROTTLE(mtr)	((mtr) & (1 << 9))
#define MTR_DRAM_WIDTH(mtr)		(((mtr) & (1 << 8)) ? 8 : 4)
#define MTR_DRAM_BANKS(mtr)		(((mtr) & (1 << 6)) ? 8 : 4)
#define MTR_DRAM_BANKS_ADDR_BITS(mtr)	((MTR_DRAM_BANKS(mtr) == 8) ? 3 : 2)
#define MTR_DIMM_RANK(mtr)		(((mtr) >> 5) & 0x1)
#define MTR_DIMM_RANK_ADDR_BITS(mtr)	(MTR_DIMM_RANK(mtr) ? 2 : 1)
#define MTR_DIMM_ROWS(mtr)		(((mtr) >> 2) & 0x3)
#define MTR_DIMM_ROWS_ADDR_BITS(mtr)	(MTR_DIMM_ROWS(mtr) + 13)
#define MTR_DIMM_COLS(mtr)		((mtr) & 0x3)
#define MTR_DIMM_COLS_ADDR_BITS(mtr)	(MTR_DIMM_COLS(mtr) + 10)

static inline int extract_fbdchan_indx(u32 x)
{
	return (x>>28) & 0x3;
}

#ifdef CONFIG_EDAC_DEBUG
static const char *numrow_toString[] = {
	"8,192 - 13 rows",
	"16,384 - 14 rows",
	"32,768 - 15 rows",
	"65,536 - 16 rows"
};

static const char *numcol_toString[] = {
	"1,024 - 10 columns",
	"2,048 - 11 columns",
	"4,096 - 12 columns",
	"reserved"
};
#endif

struct i5400_dev_info {
	const char *ctl_name;	
	u16 fsb_mapping_errors;	
};

static const struct i5400_dev_info i5400_devs[] = {
	{
		.ctl_name = "I5400",
		.fsb_mapping_errors = PCI_DEVICE_ID_INTEL_5400_ERR,
	},
};

struct i5400_dimm_info {
	int megabytes;		
};

struct i5400_pvt {
	struct pci_dev *system_address;		
	struct pci_dev *branchmap_werrors;	
	struct pci_dev *fsb_error_regs;		
	struct pci_dev *branch_0;		
	struct pci_dev *branch_1;		

	u16 tolm;				
	u64 ambase;				

	u16 mir0, mir1;

	u16 b0_mtr[NUM_MTRS_PER_BRANCH];	
	u16 b0_ambpresent0;			
	u16 b0_ambpresent1;			

	u16 b1_mtr[NUM_MTRS_PER_BRANCH];	
	u16 b1_ambpresent0;			
	u16 b1_ambpresent1;			

	
	struct i5400_dimm_info dimm_info[MAX_CSROWS][MAX_CHANNELS];

	
	int maxch;				
	int maxdimmperch;			
};

struct i5400_error_info {
	
	u32 ferr_fat_fbd;	
	u32 nerr_fat_fbd;	
	u32 ferr_nf_fbd;	
	u32 nerr_nf_fbd;	

	
	u32 redmemb;		
	u16 recmema;		
	u32 recmemb;		

	
	u16 nrecmema;		
	u16 nrecmemb;		

};

static inline int nrec_bank(struct i5400_error_info *info)
{
	return ((info->nrecmema) >> 12) & 0x7;
}
static inline int nrec_rank(struct i5400_error_info *info)
{
	return ((info->nrecmema) >> 8) & 0xf;
}
static inline int nrec_buf_id(struct i5400_error_info *info)
{
	return ((info->nrecmema)) & 0xff;
}
static inline int nrec_rdwr(struct i5400_error_info *info)
{
	return (info->nrecmemb) >> 31;
}
static inline const char *rdwr_str(int rdwr)
{
	return rdwr ? "Write" : "Read";
}
static inline int nrec_cas(struct i5400_error_info *info)
{
	return ((info->nrecmemb) >> 16) & 0x1fff;
}
static inline int nrec_ras(struct i5400_error_info *info)
{
	return (info->nrecmemb) & 0xffff;
}
static inline int rec_bank(struct i5400_error_info *info)
{
	return ((info->recmema) >> 12) & 0x7;
}
static inline int rec_rank(struct i5400_error_info *info)
{
	return ((info->recmema) >> 8) & 0xf;
}
static inline int rec_rdwr(struct i5400_error_info *info)
{
	return (info->recmemb) >> 31;
}
static inline int rec_cas(struct i5400_error_info *info)
{
	return ((info->recmemb) >> 16) & 0x1fff;
}
static inline int rec_ras(struct i5400_error_info *info)
{
	return (info->recmemb) & 0xffff;
}

static struct edac_pci_ctl_info *i5400_pci;

static void i5400_get_error_info(struct mem_ctl_info *mci,
				 struct i5400_error_info *info)
{
	struct i5400_pvt *pvt;
	u32 value;

	pvt = mci->pvt_info;

	
	pci_read_config_dword(pvt->branchmap_werrors, FERR_FAT_FBD, &value);

	value &= (FERR_FAT_FBDCHAN | FERR_FAT_MASK);

	if (value & FERR_FAT_MASK) {
		info->ferr_fat_fbd = value;

		
		pci_read_config_dword(pvt->branchmap_werrors,
				NERR_FAT_FBD, &info->nerr_fat_fbd);
		pci_read_config_word(pvt->branchmap_werrors,
				NRECMEMA, &info->nrecmema);
		pci_read_config_word(pvt->branchmap_werrors,
				NRECMEMB, &info->nrecmemb);

		
		pci_write_config_dword(pvt->branchmap_werrors,
				FERR_FAT_FBD, value);
	} else {
		info->ferr_fat_fbd = 0;
		info->nerr_fat_fbd = 0;
		info->nrecmema = 0;
		info->nrecmemb = 0;
	}

	
	pci_read_config_dword(pvt->branchmap_werrors, FERR_NF_FBD, &value);

	if (value & FERR_NF_MASK) {
		info->ferr_nf_fbd = value;

		
		pci_read_config_dword(pvt->branchmap_werrors,
				NERR_NF_FBD, &info->nerr_nf_fbd);
		pci_read_config_word(pvt->branchmap_werrors,
				RECMEMA, &info->recmema);
		pci_read_config_dword(pvt->branchmap_werrors,
				RECMEMB, &info->recmemb);
		pci_read_config_dword(pvt->branchmap_werrors,
				REDMEMB, &info->redmemb);

		
		pci_write_config_dword(pvt->branchmap_werrors,
				FERR_NF_FBD, value);
	} else {
		info->ferr_nf_fbd = 0;
		info->nerr_nf_fbd = 0;
		info->recmema = 0;
		info->recmemb = 0;
		info->redmemb = 0;
	}
}

static void i5400_proccess_non_recoverable_info(struct mem_ctl_info *mci,
				    struct i5400_error_info *info,
				    unsigned long allErrors)
{
	char msg[EDAC_MC_LABEL_LEN + 1 + 90 + 80];
	int branch;
	int channel;
	int bank;
	int buf_id;
	int rank;
	int rdwr;
	int ras, cas;
	int errnum;
	char *type = NULL;

	if (!allErrors)
		return;		

	if (allErrors &  ERROR_FAT_MASK)
		type = "FATAL";
	else if (allErrors & FERR_NF_UNCORRECTABLE)
		type = "NON-FATAL uncorrected";
	else
		type = "NON-FATAL recoverable";

	

	branch = extract_fbdchan_indx(info->ferr_fat_fbd);
	channel = branch;

	
	bank = nrec_bank(info);
	rank = nrec_rank(info);
	buf_id = nrec_buf_id(info);
	rdwr = nrec_rdwr(info);
	ras = nrec_ras(info);
	cas = nrec_cas(info);

	debugf0("\t\tCSROW= %d  Channels= %d,%d  (Branch= %d "
		"DRAM Bank= %d Buffer ID = %d rdwr= %s ras= %d cas= %d)\n",
		rank, channel, channel + 1, branch >> 1, bank,
		buf_id, rdwr_str(rdwr), ras, cas);

	
	errnum = find_first_bit(&allErrors, ARRAY_SIZE(error_name));

	
	snprintf(msg, sizeof(msg),
		 "%s (Branch=%d DRAM-Bank=%d Buffer ID = %d RDWR=%s "
		 "RAS=%d CAS=%d %s Err=0x%lx (%s))",
		 type, branch >> 1, bank, buf_id, rdwr_str(rdwr), ras, cas,
		 type, allErrors, error_name[errnum]);

	
	edac_mc_handle_fbd_ue(mci, rank, channel, channel + 1, msg);
}

static void i5400_process_nonfatal_error_info(struct mem_ctl_info *mci,
					struct i5400_error_info *info)
{
	char msg[EDAC_MC_LABEL_LEN + 1 + 90 + 80];
	unsigned long allErrors;
	int branch;
	int channel;
	int bank;
	int rank;
	int rdwr;
	int ras, cas;
	int errnum;

	
	allErrors = from_nf_ferr(info->ferr_nf_fbd & FERR_NF_MASK);
	if (!allErrors)
		return;		

	

	if (allErrors & (ERROR_NF_UNCORRECTABLE | ERROR_NF_RECOVERABLE)) {
		i5400_proccess_non_recoverable_info(mci, info, allErrors);
		return;
	}

	
	if (allErrors & ERROR_NF_CORRECTABLE) {
		debugf0("\tCorrected bits= 0x%lx\n", allErrors);

		branch = extract_fbdchan_indx(info->ferr_nf_fbd);

		channel = 0;
		if (REC_ECC_LOCATOR_ODD(info->redmemb))
			channel = 1;

		channel += branch;

		bank = rec_bank(info);
		rank = rec_rank(info);
		rdwr = rec_rdwr(info);
		ras = rec_ras(info);
		cas = rec_cas(info);

		
		errnum = find_first_bit(&allErrors, ARRAY_SIZE(error_name));

		debugf0("\t\tCSROW= %d Channel= %d  (Branch %d "
			"DRAM Bank= %d rdwr= %s ras= %d cas= %d)\n",
			rank, channel, branch >> 1, bank,
			rdwr_str(rdwr), ras, cas);

		
		snprintf(msg, sizeof(msg),
			 "Corrected error (Branch=%d DRAM-Bank=%d RDWR=%s "
			 "RAS=%d CAS=%d, CE Err=0x%lx (%s))",
			 branch >> 1, bank, rdwr_str(rdwr), ras, cas,
			 allErrors, error_name[errnum]);

		
		edac_mc_handle_fbd_ce(mci, rank, channel, msg);

		return;
	}

	
	errnum = find_first_bit(&allErrors, ARRAY_SIZE(error_name));

	branch = extract_fbdchan_indx(info->ferr_nf_fbd);

	i5400_mc_printk(mci, KERN_EMERG,
			"Non-Fatal misc error (Branch=%d Err=%#lx (%s))",
			branch >> 1, allErrors, error_name[errnum]);
}

static void i5400_process_error_info(struct mem_ctl_info *mci,
				struct i5400_error_info *info)
{	u32 allErrors;

	
	allErrors = (info->ferr_fat_fbd & FERR_FAT_MASK);
	i5400_proccess_non_recoverable_info(mci, info, allErrors);

	
	i5400_process_nonfatal_error_info(mci, info);
}

static void i5400_clear_error(struct mem_ctl_info *mci)
{
	struct i5400_error_info info;

	i5400_get_error_info(mci, &info);
}

static void i5400_check_error(struct mem_ctl_info *mci)
{
	struct i5400_error_info info;
	debugf4("MC%d: %s: %s()\n", mci->mc_idx, __FILE__, __func__);
	i5400_get_error_info(mci, &info);
	i5400_process_error_info(mci, &info);
}

static void i5400_put_devices(struct mem_ctl_info *mci)
{
	struct i5400_pvt *pvt;

	pvt = mci->pvt_info;

	
	pci_dev_put(pvt->branch_1);
	pci_dev_put(pvt->branch_0);
	pci_dev_put(pvt->fsb_error_regs);
	pci_dev_put(pvt->branchmap_werrors);
}

static int i5400_get_devices(struct mem_ctl_info *mci, int dev_idx)
{
	struct i5400_pvt *pvt;
	struct pci_dev *pdev;

	pvt = mci->pvt_info;
	pvt->branchmap_werrors = NULL;
	pvt->fsb_error_regs = NULL;
	pvt->branch_0 = NULL;
	pvt->branch_1 = NULL;

	
	pdev = NULL;
	while (1) {
		pdev = pci_get_device(PCI_VENDOR_ID_INTEL,
				      PCI_DEVICE_ID_INTEL_5400_ERR, pdev);
		if (!pdev) {
			
			i5400_printk(KERN_ERR,
				"'system address,Process Bus' "
				"device not found:"
				"vendor 0x%x device 0x%x ERR func 1 "
				"(broken BIOS?)\n",
				PCI_VENDOR_ID_INTEL,
				PCI_DEVICE_ID_INTEL_5400_ERR);
			return -ENODEV;
		}

		
		if (PCI_FUNC(pdev->devfn) == 1)
			break;
	}
	pvt->branchmap_werrors = pdev;

	pdev = NULL;
	while (1) {
		pdev = pci_get_device(PCI_VENDOR_ID_INTEL,
				      PCI_DEVICE_ID_INTEL_5400_ERR, pdev);
		if (!pdev) {
			
			i5400_printk(KERN_ERR,
				"'system address,Process Bus' "
				"device not found:"
				"vendor 0x%x device 0x%x ERR func 2 "
				"(broken BIOS?)\n",
				PCI_VENDOR_ID_INTEL,
				PCI_DEVICE_ID_INTEL_5400_ERR);

			pci_dev_put(pvt->branchmap_werrors);
			return -ENODEV;
		}

		
		if (PCI_FUNC(pdev->devfn) == 2)
			break;
	}
	pvt->fsb_error_regs = pdev;

	debugf1("System Address, processor bus- PCI Bus ID: %s  %x:%x\n",
		pci_name(pvt->system_address),
		pvt->system_address->vendor, pvt->system_address->device);
	debugf1("Branchmap, control and errors - PCI Bus ID: %s  %x:%x\n",
		pci_name(pvt->branchmap_werrors),
		pvt->branchmap_werrors->vendor, pvt->branchmap_werrors->device);
	debugf1("FSB Error Regs - PCI Bus ID: %s  %x:%x\n",
		pci_name(pvt->fsb_error_regs),
		pvt->fsb_error_regs->vendor, pvt->fsb_error_regs->device);

	pvt->branch_0 = pci_get_device(PCI_VENDOR_ID_INTEL,
				       PCI_DEVICE_ID_INTEL_5400_FBD0, NULL);
	if (!pvt->branch_0) {
		i5400_printk(KERN_ERR,
			"MC: 'BRANCH 0' device not found:"
			"vendor 0x%x device 0x%x Func 0 (broken BIOS?)\n",
			PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_5400_FBD0);

		pci_dev_put(pvt->fsb_error_regs);
		pci_dev_put(pvt->branchmap_werrors);
		return -ENODEV;
	}

	if (pvt->maxch < CHANNELS_PER_BRANCH)
		return 0;

	pvt->branch_1 = pci_get_device(PCI_VENDOR_ID_INTEL,
				       PCI_DEVICE_ID_INTEL_5400_FBD1, NULL);
	if (!pvt->branch_1) {
		i5400_printk(KERN_ERR,
			"MC: 'BRANCH 1' device not found:"
			"vendor 0x%x device 0x%x Func 0 "
			"(broken BIOS?)\n",
			PCI_VENDOR_ID_INTEL,
			PCI_DEVICE_ID_INTEL_5400_FBD1);

		pci_dev_put(pvt->branch_0);
		pci_dev_put(pvt->fsb_error_regs);
		pci_dev_put(pvt->branchmap_werrors);
		return -ENODEV;
	}

	return 0;
}

static int determine_amb_present_reg(struct i5400_pvt *pvt, int channel)
{
	int amb_present;

	if (channel < CHANNELS_PER_BRANCH) {
		if (channel & 0x1)
			amb_present = pvt->b0_ambpresent1;
		else
			amb_present = pvt->b0_ambpresent0;
	} else {
		if (channel & 0x1)
			amb_present = pvt->b1_ambpresent1;
		else
			amb_present = pvt->b1_ambpresent0;
	}

	return amb_present;
}

static int determine_mtr(struct i5400_pvt *pvt, int csrow, int channel)
{
	int mtr;
	int n;

	n = csrow;

	if (n >= NUM_MTRS_PER_BRANCH) {
		debugf0("ERROR: trying to access an invalid csrow: %d\n",
			csrow);
		return 0;
	}

	if (channel < CHANNELS_PER_BRANCH)
		mtr = pvt->b0_mtr[n];
	else
		mtr = pvt->b1_mtr[n];

	return mtr;
}

static void decode_mtr(int slot_row, u16 mtr)
{
	int ans;

	ans = MTR_DIMMS_PRESENT(mtr);

	debugf2("\tMTR%d=0x%x:  DIMMs are %s\n", slot_row, mtr,
		ans ? "Present" : "NOT Present");
	if (!ans)
		return;

	debugf2("\t\tWIDTH: x%d\n", MTR_DRAM_WIDTH(mtr));

	debugf2("\t\tELECTRICAL THROTTLING is %s\n",
		MTR_DIMMS_ETHROTTLE(mtr) ? "enabled" : "disabled");

	debugf2("\t\tNUMBANK: %d bank(s)\n", MTR_DRAM_BANKS(mtr));
	debugf2("\t\tNUMRANK: %s\n", MTR_DIMM_RANK(mtr) ? "double" : "single");
	debugf2("\t\tNUMROW: %s\n", numrow_toString[MTR_DIMM_ROWS(mtr)]);
	debugf2("\t\tNUMCOL: %s\n", numcol_toString[MTR_DIMM_COLS(mtr)]);
}

static void handle_channel(struct i5400_pvt *pvt, int csrow, int channel,
			struct i5400_dimm_info *dinfo)
{
	int mtr;
	int amb_present_reg;
	int addrBits;

	mtr = determine_mtr(pvt, csrow, channel);
	if (MTR_DIMMS_PRESENT(mtr)) {
		amb_present_reg = determine_amb_present_reg(pvt, channel);

		
		if (amb_present_reg & (1 << csrow)) {
			addrBits = MTR_DRAM_BANKS_ADDR_BITS(mtr);
			
			addrBits += MTR_DIMM_ROWS_ADDR_BITS(mtr);
			
			addrBits += MTR_DIMM_COLS_ADDR_BITS(mtr);
			
			addrBits += MTR_DIMM_RANK(mtr);

			addrBits += 6;	
			addrBits -= 20;	
			addrBits -= 3;	

			dinfo->megabytes = 1 << addrBits;
		}
	}
}

static void calculate_dimm_size(struct i5400_pvt *pvt)
{
	struct i5400_dimm_info *dinfo;
	int csrow, max_csrows;
	char *p, *mem_buffer;
	int space, n;
	int channel;

	
	space = PAGE_SIZE;
	mem_buffer = p = kmalloc(space, GFP_KERNEL);
	if (p == NULL) {
		i5400_printk(KERN_ERR, "MC: %s:%s() kmalloc() failed\n",
			__FILE__, __func__);
		return;
	}

	max_csrows = pvt->maxdimmperch;
	for (csrow = max_csrows - 1; csrow >= 0; csrow--) {

		if (csrow & 0x1) {
			n = snprintf(p, space, "---------------------------"
					"--------------------------------");
			p += n;
			space -= n;
			debugf2("%s\n", mem_buffer);
			p = mem_buffer;
			space = PAGE_SIZE;
		}
		n = snprintf(p, space, "csrow %2d    ", csrow);
		p += n;
		space -= n;

		for (channel = 0; channel < pvt->maxch; channel++) {
			dinfo = &pvt->dimm_info[csrow][channel];
			handle_channel(pvt, csrow, channel, dinfo);
			n = snprintf(p, space, "%4d MB   | ", dinfo->megabytes);
			p += n;
			space -= n;
		}
		debugf2("%s\n", mem_buffer);
		p = mem_buffer;
		space = PAGE_SIZE;
	}

	
	n = snprintf(p, space, "---------------------------"
			"--------------------------------");
	p += n;
	space -= n;
	debugf2("%s\n", mem_buffer);
	p = mem_buffer;
	space = PAGE_SIZE;

	
	n = snprintf(p, space, "            ");
	p += n;
	space -= n;
	for (channel = 0; channel < pvt->maxch; channel++) {
		n = snprintf(p, space, "channel %d | ", channel);
		p += n;
		space -= n;
	}

	
	debugf2("%s\n", mem_buffer);
	kfree(mem_buffer);
}

static void i5400_get_mc_regs(struct mem_ctl_info *mci)
{
	struct i5400_pvt *pvt;
	u32 actual_tolm;
	u16 limit;
	int slot_row;
	int maxch;
	int maxdimmperch;
	int way0, way1;

	pvt = mci->pvt_info;

	pci_read_config_dword(pvt->system_address, AMBASE,
			(u32 *) &pvt->ambase);
	pci_read_config_dword(pvt->system_address, AMBASE + sizeof(u32),
			((u32 *) &pvt->ambase) + sizeof(u32));

	maxdimmperch = pvt->maxdimmperch;
	maxch = pvt->maxch;

	debugf2("AMBASE= 0x%lx  MAXCH= %d  MAX-DIMM-Per-CH= %d\n",
		(long unsigned int)pvt->ambase, pvt->maxch, pvt->maxdimmperch);

	
	pci_read_config_word(pvt->branchmap_werrors, TOLM, &pvt->tolm);
	pvt->tolm >>= 12;
	debugf2("\nTOLM (number of 256M regions) =%u (0x%x)\n", pvt->tolm,
		pvt->tolm);

	actual_tolm = (u32) ((1000l * pvt->tolm) >> (30 - 28));
	debugf2("Actual TOLM byte addr=%u.%03u GB (0x%x)\n",
		actual_tolm/1000, actual_tolm % 1000, pvt->tolm << 28);

	pci_read_config_word(pvt->branchmap_werrors, MIR0, &pvt->mir0);
	pci_read_config_word(pvt->branchmap_werrors, MIR1, &pvt->mir1);

	
	limit = (pvt->mir0 >> 4) & 0x0fff;
	way0 = pvt->mir0 & 0x1;
	way1 = pvt->mir0 & 0x2;
	debugf2("MIR0: limit= 0x%x  WAY1= %u  WAY0= %x\n", limit, way1, way0);
	limit = (pvt->mir1 >> 4) & 0xfff;
	way0 = pvt->mir1 & 0x1;
	way1 = pvt->mir1 & 0x2;
	debugf2("MIR1: limit= 0x%x  WAY1= %u  WAY0= %x\n", limit, way1, way0);

	
	for (slot_row = 0; slot_row < NUM_MTRS_PER_BRANCH; slot_row++) {
		int where = MTR0 + (slot_row * sizeof(u16));

		
		pci_read_config_word(pvt->branch_0, where,
				&pvt->b0_mtr[slot_row]);

		debugf2("MTR%d where=0x%x B0 value=0x%x\n", slot_row, where,
			pvt->b0_mtr[slot_row]);

		if (pvt->maxch < CHANNELS_PER_BRANCH) {
			pvt->b1_mtr[slot_row] = 0;
			continue;
		}

		
		pci_read_config_word(pvt->branch_1, where,
				&pvt->b1_mtr[slot_row]);
		debugf2("MTR%d where=0x%x B1 value=0x%x\n", slot_row, where,
			pvt->b1_mtr[slot_row]);
	}

	
	debugf2("\nMemory Technology Registers:\n");
	debugf2("   Branch 0:\n");
	for (slot_row = 0; slot_row < NUM_MTRS_PER_BRANCH; slot_row++)
		decode_mtr(slot_row, pvt->b0_mtr[slot_row]);

	pci_read_config_word(pvt->branch_0, AMBPRESENT_0,
			&pvt->b0_ambpresent0);
	debugf2("\t\tAMB-Branch 0-present0 0x%x:\n", pvt->b0_ambpresent0);
	pci_read_config_word(pvt->branch_0, AMBPRESENT_1,
			&pvt->b0_ambpresent1);
	debugf2("\t\tAMB-Branch 0-present1 0x%x:\n", pvt->b0_ambpresent1);

	
	if (pvt->maxch < CHANNELS_PER_BRANCH) {
		pvt->b1_ambpresent0 = 0;
		pvt->b1_ambpresent1 = 0;
	} else {
		
		debugf2("   Branch 1:\n");
		for (slot_row = 0; slot_row < NUM_MTRS_PER_BRANCH; slot_row++)
			decode_mtr(slot_row, pvt->b1_mtr[slot_row]);

		pci_read_config_word(pvt->branch_1, AMBPRESENT_0,
				&pvt->b1_ambpresent0);
		debugf2("\t\tAMB-Branch 1-present0 0x%x:\n",
			pvt->b1_ambpresent0);
		pci_read_config_word(pvt->branch_1, AMBPRESENT_1,
				&pvt->b1_ambpresent1);
		debugf2("\t\tAMB-Branch 1-present1 0x%x:\n",
			pvt->b1_ambpresent1);
	}

	calculate_dimm_size(pvt);
}

static int i5400_init_csrows(struct mem_ctl_info *mci)
{
	struct i5400_pvt *pvt;
	struct csrow_info *p_csrow;
	int empty, channel_count;
	int max_csrows;
	int mtr;
	int csrow_megs;
	int channel;
	int csrow;

	pvt = mci->pvt_info;

	channel_count = pvt->maxch;
	max_csrows = pvt->maxdimmperch;

	empty = 1;		

	for (csrow = 0; csrow < max_csrows; csrow++) {
		p_csrow = &mci->csrows[csrow];

		p_csrow->csrow_idx = csrow;

		
		mtr = determine_mtr(pvt, csrow, 0);

		
		if (!MTR_DIMMS_PRESENT(mtr))
			continue;

		
		p_csrow->first_page = 0 + csrow * 20;
		p_csrow->last_page = 9 + csrow * 20;
		p_csrow->page_mask = 0xFFF;

		p_csrow->grain = 8;

		csrow_megs = 0;
		for (channel = 0; channel < pvt->maxch; channel++)
			csrow_megs += pvt->dimm_info[csrow][channel].megabytes;

		p_csrow->nr_pages = csrow_megs << 8;

		
		p_csrow->mtype = MEM_FB_DDR2;

		
		if (MTR_DRAM_WIDTH(mtr))
			p_csrow->dtype = DEV_X8;
		else
			p_csrow->dtype = DEV_X4;

		p_csrow->edac_mode = EDAC_S8ECD8ED;

		empty = 0;
	}

	return empty;
}

static void i5400_enable_error_reporting(struct mem_ctl_info *mci)
{
	struct i5400_pvt *pvt;
	u32 fbd_error_mask;

	pvt = mci->pvt_info;

	
	pci_read_config_dword(pvt->branchmap_werrors, EMASK_FBD,
			&fbd_error_mask);

	
	fbd_error_mask &= ~(ENABLE_EMASK_ALL);

	pci_write_config_dword(pvt->branchmap_werrors, EMASK_FBD,
			fbd_error_mask);
}

static int i5400_probe1(struct pci_dev *pdev, int dev_idx)
{
	struct mem_ctl_info *mci;
	struct i5400_pvt *pvt;
	int num_channels;
	int num_dimms_per_channel;
	int num_csrows;

	if (dev_idx >= ARRAY_SIZE(i5400_devs))
		return -EINVAL;

	debugf0("MC: %s: %s(), pdev bus %u dev=0x%x fn=0x%x\n",
		__FILE__, __func__,
		pdev->bus->number,
		PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

	
	if (PCI_FUNC(pdev->devfn) != 0)
		return -ENODEV;

	num_dimms_per_channel = MAX_DIMMS_PER_CHANNEL;
	num_channels = MAX_CHANNELS;
	num_csrows = num_dimms_per_channel;

	debugf0("MC: %s(): Number of - Channels= %d  DIMMS= %d  CSROWS= %d\n",
		__func__, num_channels, num_dimms_per_channel, num_csrows);

	
	mci = edac_mc_alloc(sizeof(*pvt), num_csrows, num_channels, 0);

	if (mci == NULL)
		return -ENOMEM;

	debugf0("MC: %s: %s(): mci = %p\n", __FILE__, __func__, mci);

	mci->dev = &pdev->dev;	

	pvt = mci->pvt_info;
	pvt->system_address = pdev;	
	pvt->maxch = num_channels;
	pvt->maxdimmperch = num_dimms_per_channel;

	
	if (i5400_get_devices(mci, dev_idx))
		goto fail0;

	
	i5400_get_mc_regs(mci);	

	mci->mc_idx = 0;
	mci->mtype_cap = MEM_FLAG_FB_DDR2;
	mci->edac_ctl_cap = EDAC_FLAG_NONE;
	mci->edac_cap = EDAC_FLAG_NONE;
	mci->mod_name = "i5400_edac.c";
	mci->mod_ver = I5400_REVISION;
	mci->ctl_name = i5400_devs[dev_idx].ctl_name;
	mci->dev_name = pci_name(pdev);
	mci->ctl_page_to_phys = NULL;

	
	mci->edac_check = i5400_check_error;

	if (i5400_init_csrows(mci)) {
		debugf0("MC: Setting mci->edac_cap to EDAC_FLAG_NONE\n"
			"    because i5400_init_csrows() returned nonzero "
			"value\n");
		mci->edac_cap = EDAC_FLAG_NONE;	
	} else {
		debugf1("MC: Enable error reporting now\n");
		i5400_enable_error_reporting(mci);
	}

	
	if (edac_mc_add_mc(mci)) {
		debugf0("MC: %s: %s(): failed edac_mc_add_mc()\n",
			__FILE__, __func__);
		goto fail1;
	}

	i5400_clear_error(mci);

	
	i5400_pci = edac_pci_create_generic_ctl(&pdev->dev, EDAC_MOD_STR);
	if (!i5400_pci) {
		printk(KERN_WARNING
			"%s(): Unable to create PCI control\n",
			__func__);
		printk(KERN_WARNING
			"%s(): PCI error report via EDAC not setup\n",
			__func__);
	}

	return 0;

	
fail1:

	i5400_put_devices(mci);

fail0:
	edac_mc_free(mci);
	return -ENODEV;
}

static int __devinit i5400_init_one(struct pci_dev *pdev,
				const struct pci_device_id *id)
{
	int rc;

	debugf0("MC: %s: %s()\n", __FILE__, __func__);

	
	rc = pci_enable_device(pdev);
	if (rc)
		return rc;

	
	return i5400_probe1(pdev, id->driver_data);
}

static void __devexit i5400_remove_one(struct pci_dev *pdev)
{
	struct mem_ctl_info *mci;

	debugf0("%s: %s()\n", __FILE__, __func__);

	if (i5400_pci)
		edac_pci_release_generic_ctl(i5400_pci);

	mci = edac_mc_del_mc(&pdev->dev);
	if (!mci)
		return;

	
	i5400_put_devices(mci);

	edac_mc_free(mci);
}

static DEFINE_PCI_DEVICE_TABLE(i5400_pci_tbl) = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_5400_ERR)},
	{0,}			
};

MODULE_DEVICE_TABLE(pci, i5400_pci_tbl);

static struct pci_driver i5400_driver = {
	.name = "i5400_edac",
	.probe = i5400_init_one,
	.remove = __devexit_p(i5400_remove_one),
	.id_table = i5400_pci_tbl,
};

static int __init i5400_init(void)
{
	int pci_rc;

	debugf2("MC: %s: %s()\n", __FILE__, __func__);

	
	opstate_init();

	pci_rc = pci_register_driver(&i5400_driver);

	return (pci_rc < 0) ? pci_rc : 0;
}

static void __exit i5400_exit(void)
{
	debugf2("MC: %s: %s()\n", __FILE__, __func__);
	pci_unregister_driver(&i5400_driver);
}

module_init(i5400_init);
module_exit(i5400_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ben Woodard <woodard@redhat.com>");
MODULE_AUTHOR("Mauro Carvalho Chehab <mchehab@redhat.com>");
MODULE_AUTHOR("Red Hat Inc. (http://www.redhat.com)");
MODULE_DESCRIPTION("MC Driver for Intel I5400 memory controllers - "
		   I5400_REVISION);

module_param(edac_op_state, int, 0444);
MODULE_PARM_DESC(edac_op_state, "EDAC Error Reporting state: 0=Poll,1=NMI");
