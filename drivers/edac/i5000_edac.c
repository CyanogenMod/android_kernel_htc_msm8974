/*
 * Intel 5000(P/V/X) class Memory Controllers kernel module
 *
 * This file may be distributed under the terms of the
 * GNU General Public License.
 *
 * Written by Douglas Thompson Linux Networx (http://lnxi.com)
 *	norsk5@xmission.com
 *
 * This module is based on the following document:
 *
 * Intel 5000X Chipset Memory Controller Hub (MCH) - Datasheet
 * 	http://developer.intel.com/design/chipsets/datashts/313070.htm
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/slab.h>
#include <linux/edac.h>
#include <asm/mmzone.h>

#include "edac_core.h"

#define I5000_REVISION    " Ver: 2.0.12"
#define EDAC_MOD_STR      "i5000_edac"

#define i5000_printk(level, fmt, arg...) \
        edac_printk(level, "i5000", fmt, ##arg)

#define i5000_mc_printk(mci, level, fmt, arg...) \
        edac_mc_chipset_printk(mci, level, "i5000", fmt, ##arg)

#ifndef PCI_DEVICE_ID_INTEL_FBD_0
#define PCI_DEVICE_ID_INTEL_FBD_0	0x25F5
#endif
#ifndef PCI_DEVICE_ID_INTEL_FBD_1
#define PCI_DEVICE_ID_INTEL_FBD_1	0x25F6
#endif

#define	PCI_DEVICE_ID_INTEL_I5000_DEV16	0x25F0


#define		AMBASE			0x48
#define		MAXCH			0x56
#define		MAXDIMMPERCH		0x57
#define		TOLM			0x6C
#define		REDMEMB			0x7C
#define			RED_ECC_LOCATOR(x)	((x) & 0x3FFFF)
#define			REC_ECC_LOCATOR_EVEN(x)	((x) & 0x001FF)
#define			REC_ECC_LOCATOR_ODD(x)	((x) & 0x3FE00)
#define		MIR0			0x80
#define		MIR1			0x84
#define		MIR2			0x88
#define		AMIR0			0x8C
#define		AMIR1			0x90
#define		AMIR2			0x94

#define		FERR_FAT_FBD		0x98
#define		NERR_FAT_FBD		0x9C
#define			EXTRACT_FBDCHAN_INDX(x)	(((x)>>28) & 0x3)
#define			FERR_FAT_FBDCHAN 0x30000000
#define			FERR_FAT_M3ERR	0x00000004
#define			FERR_FAT_M2ERR	0x00000002
#define			FERR_FAT_M1ERR	0x00000001
#define			FERR_FAT_MASK	(FERR_FAT_M1ERR | \
						FERR_FAT_M2ERR | \
						FERR_FAT_M3ERR)

#define		FERR_NF_FBD		0xA0

#define			FERR_NF_M28ERR	0x01000000
#define			FERR_NF_M27ERR	0x00800000
#define			FERR_NF_M26ERR	0x00400000
#define			FERR_NF_M25ERR	0x00200000
#define			FERR_NF_M24ERR	0x00100000
#define			FERR_NF_M23ERR	0x00080000
#define			FERR_NF_M22ERR	0x00040000
#define			FERR_NF_M21ERR	0x00020000

#define			FERR_NF_M20ERR	0x00010000
#define			FERR_NF_M19ERR	0x00008000
#define			FERR_NF_M18ERR	0x00004000
#define			FERR_NF_M17ERR	0x00002000

#define			FERR_NF_M16ERR	0x00001000
#define			FERR_NF_M15ERR	0x00000800
#define			FERR_NF_M14ERR	0x00000400
#define			FERR_NF_M13ERR	0x00000200

#define			FERR_NF_M12ERR	0x00000100
#define			FERR_NF_M11ERR	0x00000080
#define			FERR_NF_M10ERR	0x00000040
#define			FERR_NF_M9ERR	0x00000020
#define			FERR_NF_M8ERR	0x00000010
#define			FERR_NF_M7ERR	0x00000008
#define			FERR_NF_M6ERR	0x00000004
#define			FERR_NF_M5ERR	0x00000002
#define			FERR_NF_M4ERR	0x00000001

#define			FERR_NF_UNCORRECTABLE	(FERR_NF_M12ERR | \
							FERR_NF_M11ERR | \
							FERR_NF_M10ERR | \
							FERR_NF_M9ERR | \
							FERR_NF_M8ERR | \
							FERR_NF_M7ERR | \
							FERR_NF_M6ERR | \
							FERR_NF_M5ERR | \
							FERR_NF_M4ERR)
#define			FERR_NF_CORRECTABLE	(FERR_NF_M20ERR | \
							FERR_NF_M19ERR | \
							FERR_NF_M18ERR | \
							FERR_NF_M17ERR)
#define			FERR_NF_DIMM_SPARE	(FERR_NF_M27ERR | \
							FERR_NF_M28ERR)
#define			FERR_NF_THERMAL		(FERR_NF_M26ERR | \
							FERR_NF_M25ERR | \
							FERR_NF_M24ERR | \
							FERR_NF_M23ERR)
#define			FERR_NF_SPD_PROTOCOL	(FERR_NF_M22ERR)
#define			FERR_NF_NORTH_CRC	(FERR_NF_M21ERR)
#define			FERR_NF_NON_RETRY	(FERR_NF_M13ERR | \
							FERR_NF_M14ERR | \
							FERR_NF_M15ERR)

#define		NERR_NF_FBD		0xA4
#define			FERR_NF_MASK		(FERR_NF_UNCORRECTABLE | \
							FERR_NF_CORRECTABLE | \
							FERR_NF_DIMM_SPARE | \
							FERR_NF_THERMAL | \
							FERR_NF_SPD_PROTOCOL | \
							FERR_NF_NORTH_CRC | \
							FERR_NF_NON_RETRY)

#define		EMASK_FBD		0xA8
#define			EMASK_FBD_M28ERR	0x08000000
#define			EMASK_FBD_M27ERR	0x04000000
#define			EMASK_FBD_M26ERR	0x02000000
#define			EMASK_FBD_M25ERR	0x01000000
#define			EMASK_FBD_M24ERR	0x00800000
#define			EMASK_FBD_M23ERR	0x00400000
#define			EMASK_FBD_M22ERR	0x00200000
#define			EMASK_FBD_M21ERR	0x00100000
#define			EMASK_FBD_M20ERR	0x00080000
#define			EMASK_FBD_M19ERR	0x00040000
#define			EMASK_FBD_M18ERR	0x00020000
#define			EMASK_FBD_M17ERR	0x00010000

#define			EMASK_FBD_M15ERR	0x00004000
#define			EMASK_FBD_M14ERR	0x00002000
#define			EMASK_FBD_M13ERR	0x00001000
#define			EMASK_FBD_M12ERR	0x00000800
#define			EMASK_FBD_M11ERR	0x00000400
#define			EMASK_FBD_M10ERR	0x00000200
#define			EMASK_FBD_M9ERR		0x00000100
#define			EMASK_FBD_M8ERR		0x00000080
#define			EMASK_FBD_M7ERR		0x00000040
#define			EMASK_FBD_M6ERR		0x00000020
#define			EMASK_FBD_M5ERR		0x00000010
#define			EMASK_FBD_M4ERR		0x00000008
#define			EMASK_FBD_M3ERR		0x00000004
#define			EMASK_FBD_M2ERR		0x00000002
#define			EMASK_FBD_M1ERR		0x00000001

#define			ENABLE_EMASK_FBD_FATAL_ERRORS	(EMASK_FBD_M1ERR | \
							EMASK_FBD_M2ERR | \
							EMASK_FBD_M3ERR)

#define 		ENABLE_EMASK_FBD_UNCORRECTABLE	(EMASK_FBD_M4ERR | \
							EMASK_FBD_M5ERR | \
							EMASK_FBD_M6ERR | \
							EMASK_FBD_M7ERR | \
							EMASK_FBD_M8ERR | \
							EMASK_FBD_M9ERR | \
							EMASK_FBD_M10ERR | \
							EMASK_FBD_M11ERR | \
							EMASK_FBD_M12ERR)
#define 		ENABLE_EMASK_FBD_CORRECTABLE	(EMASK_FBD_M17ERR | \
							EMASK_FBD_M18ERR | \
							EMASK_FBD_M19ERR | \
							EMASK_FBD_M20ERR)
#define			ENABLE_EMASK_FBD_DIMM_SPARE	(EMASK_FBD_M27ERR | \
							EMASK_FBD_M28ERR)
#define			ENABLE_EMASK_FBD_THERMALS	(EMASK_FBD_M26ERR | \
							EMASK_FBD_M25ERR | \
							EMASK_FBD_M24ERR | \
							EMASK_FBD_M23ERR)
#define			ENABLE_EMASK_FBD_SPD_PROTOCOL	(EMASK_FBD_M22ERR)
#define			ENABLE_EMASK_FBD_NORTH_CRC	(EMASK_FBD_M21ERR)
#define			ENABLE_EMASK_FBD_NON_RETRY	(EMASK_FBD_M15ERR | \
							EMASK_FBD_M14ERR | \
							EMASK_FBD_M13ERR)

#define		ENABLE_EMASK_ALL	(ENABLE_EMASK_FBD_NON_RETRY | \
					ENABLE_EMASK_FBD_NORTH_CRC | \
					ENABLE_EMASK_FBD_SPD_PROTOCOL | \
					ENABLE_EMASK_FBD_THERMALS | \
					ENABLE_EMASK_FBD_DIMM_SPARE | \
					ENABLE_EMASK_FBD_FATAL_ERRORS | \
					ENABLE_EMASK_FBD_CORRECTABLE | \
					ENABLE_EMASK_FBD_UNCORRECTABLE)

#define		ERR0_FBD		0xAC
#define		ERR1_FBD		0xB0
#define		ERR2_FBD		0xB4
#define		MCERR_FBD		0xB8
#define		NRECMEMA		0xBE
#define			NREC_BANK(x)		(((x)>>12) & 0x7)
#define			NREC_RDWR(x)		(((x)>>11) & 1)
#define			NREC_RANK(x)		(((x)>>8) & 0x7)
#define		NRECMEMB		0xC0
#define			NREC_CAS(x)		(((x)>>16) & 0xFFFFFF)
#define			NREC_RAS(x)		((x) & 0x7FFF)
#define		NRECFGLOG		0xC4
#define		NREEECFBDA		0xC8
#define		NREEECFBDB		0xCC
#define		NREEECFBDC		0xD0
#define		NREEECFBDD		0xD4
#define		NREEECFBDE		0xD8
#define		REDMEMA			0xDC
#define		RECMEMA			0xE2
#define			REC_BANK(x)		(((x)>>12) & 0x7)
#define			REC_RDWR(x)		(((x)>>11) & 1)
#define			REC_RANK(x)		(((x)>>8) & 0x7)
#define		RECMEMB			0xE4
#define			REC_CAS(x)		(((x)>>16) & 0xFFFFFF)
#define			REC_RAS(x)		((x) & 0x7FFF)
#define		RECFGLOG		0xE8
#define		RECFBDA			0xEC
#define		RECFBDB			0xF0
#define		RECFBDC			0xF4
#define		RECFBDD			0xF8
#define		RECFBDE			0xFC


#define PCI_DEVICE_ID_I5000_BRANCH_0	0x25F5
#define PCI_DEVICE_ID_I5000_BRANCH_1	0x25F6

#define AMB_PRESENT_0	0x64
#define AMB_PRESENT_1	0x66
#define MTR0		0x80
#define MTR1		0x84
#define MTR2		0x88
#define MTR3		0x8C

#define NUM_MTRS		4
#define CHANNELS_PER_BRANCH	(2)

#define MTR_DIMMS_PRESENT(mtr)		((mtr) & (0x1 << 8))
#define MTR_DRAM_WIDTH(mtr)		((((mtr) >> 6) & 0x1) ? 8 : 4)
#define MTR_DRAM_BANKS(mtr)		((((mtr) >> 5) & 0x1) ? 8 : 4)
#define MTR_DRAM_BANKS_ADDR_BITS(mtr)	((MTR_DRAM_BANKS(mtr) == 8) ? 3 : 2)
#define MTR_DIMM_RANK(mtr)		(((mtr) >> 4) & 0x1)
#define MTR_DIMM_RANK_ADDR_BITS(mtr)	(MTR_DIMM_RANK(mtr) ? 2 : 1)
#define MTR_DIMM_ROWS(mtr)		(((mtr) >> 2) & 0x3)
#define MTR_DIMM_ROWS_ADDR_BITS(mtr)	(MTR_DIMM_ROWS(mtr) + 13)
#define MTR_DIMM_COLS(mtr)		((mtr) & 0x3)
#define MTR_DIMM_COLS_ADDR_BITS(mtr)	(MTR_DIMM_COLS(mtr) + 10)

#ifdef CONFIG_EDAC_DEBUG
static char *numrow_toString[] = {
	"8,192 - 13 rows",
	"16,384 - 14 rows",
	"32,768 - 15 rows",
	"reserved"
};

static char *numcol_toString[] = {
	"1,024 - 10 columns",
	"2,048 - 11 columns",
	"4,096 - 12 columns",
	"reserved"
};
#endif

static int misc_messages;

enum i5000_chips {
	I5000P = 0,
	I5000V = 1,		
	I5000X = 2		
};

struct i5000_dev_info {
	const char *ctl_name;	
	u16 fsb_mapping_errors;	
};

static const struct i5000_dev_info i5000_devs[] = {
	[I5000P] = {
		.ctl_name = "I5000",
		.fsb_mapping_errors = PCI_DEVICE_ID_INTEL_I5000_DEV16,
	},
};

struct i5000_dimm_info {
	int megabytes;		
	int dual_rank;
};

#define	MAX_CHANNELS	6	
#define MAX_CSROWS	(8*2)	

struct i5000_pvt {
	struct pci_dev *system_address;	
	struct pci_dev *branchmap_werrors;	
	struct pci_dev *fsb_error_regs;	
	struct pci_dev *branch_0;	
	struct pci_dev *branch_1;	

	u16 tolm;		
	u64 ambase;		

	u16 mir0, mir1, mir2;

	u16 b0_mtr[NUM_MTRS];	
	u16 b0_ambpresent0;	
	u16 b0_ambpresent1;	

	u16 b1_mtr[NUM_MTRS];	
	u16 b1_ambpresent0;	
	u16 b1_ambpresent1;	

	
	struct i5000_dimm_info dimm_info[MAX_CSROWS][MAX_CHANNELS];

	
	int maxch;		
	int maxdimmperch;	
};

struct i5000_error_info {

	
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

static struct edac_pci_ctl_info *i5000_pci;

static void i5000_get_error_info(struct mem_ctl_info *mci,
				 struct i5000_error_info *info)
{
	struct i5000_pvt *pvt;
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

static void i5000_process_fatal_error_info(struct mem_ctl_info *mci,
					struct i5000_error_info *info,
					int handle_errors)
{
	char msg[EDAC_MC_LABEL_LEN + 1 + 160];
	char *specific = NULL;
	u32 allErrors;
	int branch;
	int channel;
	int bank;
	int rank;
	int rdwr;
	int ras, cas;

	
	allErrors = (info->ferr_fat_fbd & FERR_FAT_MASK);
	if (!allErrors)
		return;		

	branch = EXTRACT_FBDCHAN_INDX(info->ferr_fat_fbd);
	channel = branch;

	
	bank = NREC_BANK(info->nrecmema);
	rank = NREC_RANK(info->nrecmema);
	rdwr = NREC_RDWR(info->nrecmema);
	ras = NREC_RAS(info->nrecmemb);
	cas = NREC_CAS(info->nrecmemb);

	debugf0("\t\tCSROW= %d  Channels= %d,%d  (Branch= %d "
		"DRAM Bank= %d rdwr= %s ras= %d cas= %d)\n",
		rank, channel, channel + 1, branch >> 1, bank,
		rdwr ? "Write" : "Read", ras, cas);

	
	switch (allErrors) {
	case FERR_FAT_M1ERR:
		specific = "Alert on non-redundant retry or fast "
				"reset timeout";
		break;
	case FERR_FAT_M2ERR:
		specific = "Northbound CRC error on non-redundant "
				"retry";
		break;
	case FERR_FAT_M3ERR:
		{
		static int done;

		if (done)
			return;
		done++;

		specific = ">Tmid Thermal event with intelligent "
			   "throttling disabled";
		}
		break;
	}

	
	snprintf(msg, sizeof(msg),
		 "(Branch=%d DRAM-Bank=%d RDWR=%s RAS=%d CAS=%d "
		 "FATAL Err=0x%x (%s))",
		 branch >> 1, bank, rdwr ? "Write" : "Read", ras, cas,
		 allErrors, specific);

	
	edac_mc_handle_fbd_ue(mci, rank, channel, channel + 1, msg);
}

static void i5000_process_nonfatal_error_info(struct mem_ctl_info *mci,
					struct i5000_error_info *info,
					int handle_errors)
{
	char msg[EDAC_MC_LABEL_LEN + 1 + 170];
	char *specific = NULL;
	u32 allErrors;
	u32 ue_errors;
	u32 ce_errors;
	u32 misc_errors;
	int branch;
	int channel;
	int bank;
	int rank;
	int rdwr;
	int ras, cas;

	
	allErrors = (info->ferr_nf_fbd & FERR_NF_MASK);
	if (!allErrors)
		return;		

	
	ue_errors = allErrors & FERR_NF_UNCORRECTABLE;
	if (ue_errors) {
		debugf0("\tUncorrected bits= 0x%x\n", ue_errors);

		branch = EXTRACT_FBDCHAN_INDX(info->ferr_nf_fbd);

		channel = branch & 2;

		bank = NREC_BANK(info->nrecmema);
		rank = NREC_RANK(info->nrecmema);
		rdwr = NREC_RDWR(info->nrecmema);
		ras = NREC_RAS(info->nrecmemb);
		cas = NREC_CAS(info->nrecmemb);

		debugf0
			("\t\tCSROW= %d  Channels= %d,%d  (Branch= %d "
			"DRAM Bank= %d rdwr= %s ras= %d cas= %d)\n",
			rank, channel, channel + 1, branch >> 1, bank,
			rdwr ? "Write" : "Read", ras, cas);

		switch (ue_errors) {
		case FERR_NF_M12ERR:
			specific = "Non-Aliased Uncorrectable Patrol Data ECC";
			break;
		case FERR_NF_M11ERR:
			specific = "Non-Aliased Uncorrectable Spare-Copy "
					"Data ECC";
			break;
		case FERR_NF_M10ERR:
			specific = "Non-Aliased Uncorrectable Mirrored Demand "
					"Data ECC";
			break;
		case FERR_NF_M9ERR:
			specific = "Non-Aliased Uncorrectable Non-Mirrored "
					"Demand Data ECC";
			break;
		case FERR_NF_M8ERR:
			specific = "Aliased Uncorrectable Patrol Data ECC";
			break;
		case FERR_NF_M7ERR:
			specific = "Aliased Uncorrectable Spare-Copy Data ECC";
			break;
		case FERR_NF_M6ERR:
			specific = "Aliased Uncorrectable Mirrored Demand "
					"Data ECC";
			break;
		case FERR_NF_M5ERR:
			specific = "Aliased Uncorrectable Non-Mirrored Demand "
					"Data ECC";
			break;
		case FERR_NF_M4ERR:
			specific = "Uncorrectable Data ECC on Replay";
			break;
		}

		
		snprintf(msg, sizeof(msg),
			 "(Branch=%d DRAM-Bank=%d RDWR=%s RAS=%d "
			 "CAS=%d, UE Err=0x%x (%s))",
			 branch >> 1, bank, rdwr ? "Write" : "Read", ras, cas,
			 ue_errors, specific);

		
		edac_mc_handle_fbd_ue(mci, rank, channel, channel + 1, msg);
	}

	
	ce_errors = allErrors & FERR_NF_CORRECTABLE;
	if (ce_errors) {
		debugf0("\tCorrected bits= 0x%x\n", ce_errors);

		branch = EXTRACT_FBDCHAN_INDX(info->ferr_nf_fbd);

		channel = 0;
		if (REC_ECC_LOCATOR_ODD(info->redmemb))
			channel = 1;

		channel += branch;

		bank = REC_BANK(info->recmema);
		rank = REC_RANK(info->recmema);
		rdwr = REC_RDWR(info->recmema);
		ras = REC_RAS(info->recmemb);
		cas = REC_CAS(info->recmemb);

		debugf0("\t\tCSROW= %d Channel= %d  (Branch %d "
			"DRAM Bank= %d rdwr= %s ras= %d cas= %d)\n",
			rank, channel, branch >> 1, bank,
			rdwr ? "Write" : "Read", ras, cas);

		switch (ce_errors) {
		case FERR_NF_M17ERR:
			specific = "Correctable Non-Mirrored Demand Data ECC";
			break;
		case FERR_NF_M18ERR:
			specific = "Correctable Mirrored Demand Data ECC";
			break;
		case FERR_NF_M19ERR:
			specific = "Correctable Spare-Copy Data ECC";
			break;
		case FERR_NF_M20ERR:
			specific = "Correctable Patrol Data ECC";
			break;
		}

		
		snprintf(msg, sizeof(msg),
			 "(Branch=%d DRAM-Bank=%d RDWR=%s RAS=%d "
			 "CAS=%d, CE Err=0x%x (%s))", branch >> 1, bank,
			 rdwr ? "Write" : "Read", ras, cas, ce_errors,
			 specific);

		
		edac_mc_handle_fbd_ce(mci, rank, channel, msg);
	}

	if (!misc_messages)
		return;

	misc_errors = allErrors & (FERR_NF_NON_RETRY | FERR_NF_NORTH_CRC |
				   FERR_NF_SPD_PROTOCOL | FERR_NF_DIMM_SPARE);
	if (misc_errors) {
		switch (misc_errors) {
		case FERR_NF_M13ERR:
			specific = "Non-Retry or Redundant Retry FBD Memory "
					"Alert or Redundant Fast Reset Timeout";
			break;
		case FERR_NF_M14ERR:
			specific = "Non-Retry or Redundant Retry FBD "
					"Configuration Alert";
			break;
		case FERR_NF_M15ERR:
			specific = "Non-Retry or Redundant Retry FBD "
					"Northbound CRC error on read data";
			break;
		case FERR_NF_M21ERR:
			specific = "FBD Northbound CRC error on "
					"FBD Sync Status";
			break;
		case FERR_NF_M22ERR:
			specific = "SPD protocol error";
			break;
		case FERR_NF_M27ERR:
			specific = "DIMM-spare copy started";
			break;
		case FERR_NF_M28ERR:
			specific = "DIMM-spare copy completed";
			break;
		}
		branch = EXTRACT_FBDCHAN_INDX(info->ferr_nf_fbd);

		
		snprintf(msg, sizeof(msg),
			 "(Branch=%d Err=%#x (%s))", branch >> 1,
			 misc_errors, specific);

		
		edac_mc_handle_fbd_ce(mci, 0, 0, msg);
	}
}

static void i5000_process_error_info(struct mem_ctl_info *mci,
				struct i5000_error_info *info,
				int handle_errors)
{
	
	i5000_process_fatal_error_info(mci, info, handle_errors);

	
	i5000_process_nonfatal_error_info(mci, info, handle_errors);
}

static void i5000_clear_error(struct mem_ctl_info *mci)
{
	struct i5000_error_info info;

	i5000_get_error_info(mci, &info);
}

static void i5000_check_error(struct mem_ctl_info *mci)
{
	struct i5000_error_info info;
	debugf4("MC%d: %s: %s()\n", mci->mc_idx, __FILE__, __func__);
	i5000_get_error_info(mci, &info);
	i5000_process_error_info(mci, &info, 1);
}

static int i5000_get_devices(struct mem_ctl_info *mci, int dev_idx)
{
	
	struct i5000_pvt *pvt;
	struct pci_dev *pdev;

	pvt = mci->pvt_info;

	
	pdev = NULL;
	while (1) {
		pdev = pci_get_device(PCI_VENDOR_ID_INTEL,
				PCI_DEVICE_ID_INTEL_I5000_DEV16, pdev);

		
		if (pdev == NULL) {
			i5000_printk(KERN_ERR,
				"'system address,Process Bus' "
				"device not found:"
				"vendor 0x%x device 0x%x FUNC 1 "
				"(broken BIOS?)\n",
				PCI_VENDOR_ID_INTEL,
				PCI_DEVICE_ID_INTEL_I5000_DEV16);

			return 1;
		}

		
		if (PCI_FUNC(pdev->devfn) == 1)
			break;
	}

	pvt->branchmap_werrors = pdev;

	
	pdev = NULL;
	while (1) {
		pdev = pci_get_device(PCI_VENDOR_ID_INTEL,
				PCI_DEVICE_ID_INTEL_I5000_DEV16, pdev);

		if (pdev == NULL) {
			i5000_printk(KERN_ERR,
				"MC: 'branchmap,control,errors' "
				"device not found:"
				"vendor 0x%x device 0x%x Func 2 "
				"(broken BIOS?)\n",
				PCI_VENDOR_ID_INTEL,
				PCI_DEVICE_ID_INTEL_I5000_DEV16);

			pci_dev_put(pvt->branchmap_werrors);
			return 1;
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

	pdev = NULL;
	pdev = pci_get_device(PCI_VENDOR_ID_INTEL,
			PCI_DEVICE_ID_I5000_BRANCH_0, pdev);

	if (pdev == NULL) {
		i5000_printk(KERN_ERR,
			"MC: 'BRANCH 0' device not found:"
			"vendor 0x%x device 0x%x Func 0 (broken BIOS?)\n",
			PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_I5000_BRANCH_0);

		pci_dev_put(pvt->branchmap_werrors);
		pci_dev_put(pvt->fsb_error_regs);
		return 1;
	}

	pvt->branch_0 = pdev;

	if (pvt->maxch >= CHANNELS_PER_BRANCH) {
		pdev = NULL;
		pdev = pci_get_device(PCI_VENDOR_ID_INTEL,
				PCI_DEVICE_ID_I5000_BRANCH_1, pdev);

		if (pdev == NULL) {
			i5000_printk(KERN_ERR,
				"MC: 'BRANCH 1' device not found:"
				"vendor 0x%x device 0x%x Func 0 "
				"(broken BIOS?)\n",
				PCI_VENDOR_ID_INTEL,
				PCI_DEVICE_ID_I5000_BRANCH_1);

			pci_dev_put(pvt->branchmap_werrors);
			pci_dev_put(pvt->fsb_error_regs);
			pci_dev_put(pvt->branch_0);
			return 1;
		}

		pvt->branch_1 = pdev;
	}

	return 0;
}

static void i5000_put_devices(struct mem_ctl_info *mci)
{
	struct i5000_pvt *pvt;

	pvt = mci->pvt_info;

	pci_dev_put(pvt->branchmap_werrors);	
	pci_dev_put(pvt->fsb_error_regs);	
	pci_dev_put(pvt->branch_0);	

	
	if (pvt->maxch >= CHANNELS_PER_BRANCH)
		pci_dev_put(pvt->branch_1);	
}

static int determine_amb_present_reg(struct i5000_pvt *pvt, int channel)
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

static int determine_mtr(struct i5000_pvt *pvt, int csrow, int channel)
{
	int mtr;

	if (channel < CHANNELS_PER_BRANCH)
		mtr = pvt->b0_mtr[csrow >> 1];
	else
		mtr = pvt->b1_mtr[csrow >> 1];

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
	debugf2("\t\tNUMBANK: %d bank(s)\n", MTR_DRAM_BANKS(mtr));
	debugf2("\t\tNUMRANK: %s\n", MTR_DIMM_RANK(mtr) ? "double" : "single");
	debugf2("\t\tNUMROW: %s\n", numrow_toString[MTR_DIMM_ROWS(mtr)]);
	debugf2("\t\tNUMCOL: %s\n", numcol_toString[MTR_DIMM_COLS(mtr)]);
}

static void handle_channel(struct i5000_pvt *pvt, int csrow, int channel,
			struct i5000_dimm_info *dinfo)
{
	int mtr;
	int amb_present_reg;
	int addrBits;

	mtr = determine_mtr(pvt, csrow, channel);
	if (MTR_DIMMS_PRESENT(mtr)) {
		amb_present_reg = determine_amb_present_reg(pvt, channel);

		
		if (amb_present_reg & (1 << (csrow >> 1))) {
			dinfo->dual_rank = MTR_DIMM_RANK(mtr);

			if (!((dinfo->dual_rank == 0) &&
				((csrow & 0x1) == 0x1))) {
				addrBits = MTR_DRAM_BANKS_ADDR_BITS(mtr);
				
				addrBits += MTR_DIMM_ROWS_ADDR_BITS(mtr);
				
				addrBits += MTR_DIMM_COLS_ADDR_BITS(mtr);

				addrBits += 6;	
				addrBits -= 20;	
				addrBits -= 3;	

				dinfo->megabytes = 1 << addrBits;
			}
		}
	}
}

static void calculate_dimm_size(struct i5000_pvt *pvt)
{
	struct i5000_dimm_info *dinfo;
	int csrow, max_csrows;
	char *p, *mem_buffer;
	int space, n;
	int channel;

	
	space = PAGE_SIZE;
	mem_buffer = p = kmalloc(space, GFP_KERNEL);
	if (p == NULL) {
		i5000_printk(KERN_ERR, "MC: %s:%s() kmalloc() failed\n",
			__FILE__, __func__);
		return;
	}

	n = snprintf(p, space, "\n");
	p += n;
	space -= n;

	max_csrows = pvt->maxdimmperch * 2;
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
		n = snprintf(p, space, "\n");
		p += n;
		space -= n;
	}

	
	n = snprintf(p, space, "---------------------------"
		"--------------------------------\n");
	p += n;
	space -= n;

	
	n = snprintf(p, space, "            ");
	p += n;
	space -= n;
	for (channel = 0; channel < pvt->maxch; channel++) {
		n = snprintf(p, space, "channel %d | ", channel);
		p += n;
		space -= n;
	}
	n = snprintf(p, space, "\n");
	p += n;
	space -= n;

	
	debugf2("%s\n", mem_buffer);
	kfree(mem_buffer);
}

static void i5000_get_mc_regs(struct mem_ctl_info *mci)
{
	struct i5000_pvt *pvt;
	u32 actual_tolm;
	u16 limit;
	int slot_row;
	int maxch;
	int maxdimmperch;
	int way0, way1;

	pvt = mci->pvt_info;

	pci_read_config_dword(pvt->system_address, AMBASE,
			(u32 *) & pvt->ambase);
	pci_read_config_dword(pvt->system_address, AMBASE + sizeof(u32),
			((u32 *) & pvt->ambase) + sizeof(u32));

	maxdimmperch = pvt->maxdimmperch;
	maxch = pvt->maxch;

	debugf2("AMBASE= 0x%lx  MAXCH= %d  MAX-DIMM-Per-CH= %d\n",
		(long unsigned int)pvt->ambase, pvt->maxch, pvt->maxdimmperch);

	
	pci_read_config_word(pvt->branchmap_werrors, TOLM, &pvt->tolm);
	pvt->tolm >>= 12;
	debugf2("\nTOLM (number of 256M regions) =%u (0x%x)\n", pvt->tolm,
		pvt->tolm);

	actual_tolm = pvt->tolm << 28;
	debugf2("Actual TOLM byte addr=%u (0x%x)\n", actual_tolm, actual_tolm);

	pci_read_config_word(pvt->branchmap_werrors, MIR0, &pvt->mir0);
	pci_read_config_word(pvt->branchmap_werrors, MIR1, &pvt->mir1);
	pci_read_config_word(pvt->branchmap_werrors, MIR2, &pvt->mir2);

	
	limit = (pvt->mir0 >> 4) & 0x0FFF;
	way0 = pvt->mir0 & 0x1;
	way1 = pvt->mir0 & 0x2;
	debugf2("MIR0: limit= 0x%x  WAY1= %u  WAY0= %x\n", limit, way1, way0);
	limit = (pvt->mir1 >> 4) & 0x0FFF;
	way0 = pvt->mir1 & 0x1;
	way1 = pvt->mir1 & 0x2;
	debugf2("MIR1: limit= 0x%x  WAY1= %u  WAY0= %x\n", limit, way1, way0);
	limit = (pvt->mir2 >> 4) & 0x0FFF;
	way0 = pvt->mir2 & 0x1;
	way1 = pvt->mir2 & 0x2;
	debugf2("MIR2: limit= 0x%x  WAY1= %u  WAY0= %x\n", limit, way1, way0);

	
	for (slot_row = 0; slot_row < NUM_MTRS; slot_row++) {
		int where = MTR0 + (slot_row * sizeof(u32));

		pci_read_config_word(pvt->branch_0, where,
				&pvt->b0_mtr[slot_row]);

		debugf2("MTR%d where=0x%x B0 value=0x%x\n", slot_row, where,
			pvt->b0_mtr[slot_row]);

		if (pvt->maxch >= CHANNELS_PER_BRANCH) {
			pci_read_config_word(pvt->branch_1, where,
					&pvt->b1_mtr[slot_row]);
			debugf2("MTR%d where=0x%x B1 value=0x%x\n", slot_row,
				where, pvt->b1_mtr[slot_row]);
		} else {
			pvt->b1_mtr[slot_row] = 0;
		}
	}

	
	debugf2("\nMemory Technology Registers:\n");
	debugf2("   Branch 0:\n");
	for (slot_row = 0; slot_row < NUM_MTRS; slot_row++) {
		decode_mtr(slot_row, pvt->b0_mtr[slot_row]);
	}
	pci_read_config_word(pvt->branch_0, AMB_PRESENT_0,
			&pvt->b0_ambpresent0);
	debugf2("\t\tAMB-Branch 0-present0 0x%x:\n", pvt->b0_ambpresent0);
	pci_read_config_word(pvt->branch_0, AMB_PRESENT_1,
			&pvt->b0_ambpresent1);
	debugf2("\t\tAMB-Branch 0-present1 0x%x:\n", pvt->b0_ambpresent1);

	
	if (pvt->maxch < CHANNELS_PER_BRANCH) {
		pvt->b1_ambpresent0 = 0;
		pvt->b1_ambpresent1 = 0;
	} else {
		
		debugf2("   Branch 1:\n");
		for (slot_row = 0; slot_row < NUM_MTRS; slot_row++) {
			decode_mtr(slot_row, pvt->b1_mtr[slot_row]);
		}
		pci_read_config_word(pvt->branch_1, AMB_PRESENT_0,
				&pvt->b1_ambpresent0);
		debugf2("\t\tAMB-Branch 1-present0 0x%x:\n",
			pvt->b1_ambpresent0);
		pci_read_config_word(pvt->branch_1, AMB_PRESENT_1,
				&pvt->b1_ambpresent1);
		debugf2("\t\tAMB-Branch 1-present1 0x%x:\n",
			pvt->b1_ambpresent1);
	}

	calculate_dimm_size(pvt);
}

static int i5000_init_csrows(struct mem_ctl_info *mci)
{
	struct i5000_pvt *pvt;
	struct csrow_info *p_csrow;
	int empty, channel_count;
	int max_csrows;
	int mtr, mtr1;
	int csrow_megs;
	int channel;
	int csrow;

	pvt = mci->pvt_info;

	channel_count = pvt->maxch;
	max_csrows = pvt->maxdimmperch * 2;

	empty = 1;		

	for (csrow = 0; csrow < max_csrows; csrow++) {
		p_csrow = &mci->csrows[csrow];

		p_csrow->csrow_idx = csrow;

		
		mtr = pvt->b0_mtr[csrow >> 1];
		mtr1 = pvt->b1_mtr[csrow >> 1];

		
		if (!MTR_DIMMS_PRESENT(mtr) && !MTR_DIMMS_PRESENT(mtr1))
			continue;

		
		p_csrow->first_page = 0 + csrow * 20;
		p_csrow->last_page = 9 + csrow * 20;
		p_csrow->page_mask = 0xFFF;

		p_csrow->grain = 8;

		csrow_megs = 0;
		for (channel = 0; channel < pvt->maxch; channel++) {
			csrow_megs += pvt->dimm_info[csrow][channel].megabytes;
		}

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

static void i5000_enable_error_reporting(struct mem_ctl_info *mci)
{
	struct i5000_pvt *pvt;
	u32 fbd_error_mask;

	pvt = mci->pvt_info;

	
	pci_read_config_dword(pvt->branchmap_werrors, EMASK_FBD,
			&fbd_error_mask);

	
	fbd_error_mask &= ~(ENABLE_EMASK_ALL);

	pci_write_config_dword(pvt->branchmap_werrors, EMASK_FBD,
			fbd_error_mask);
}

static void i5000_get_dimm_and_channel_counts(struct pci_dev *pdev,
					int *num_dimms_per_channel,
					int *num_channels)
{
	u8 value;

	pci_read_config_byte(pdev, MAXDIMMPERCH, &value);
	*num_dimms_per_channel = (int)value *2;

	pci_read_config_byte(pdev, MAXCH, &value);
	*num_channels = (int)value;
}

static int i5000_probe1(struct pci_dev *pdev, int dev_idx)
{
	struct mem_ctl_info *mci;
	struct i5000_pvt *pvt;
	int num_channels;
	int num_dimms_per_channel;
	int num_csrows;

	debugf0("MC: %s: %s(), pdev bus %u dev=0x%x fn=0x%x\n",
		__FILE__, __func__,
		pdev->bus->number,
		PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

	
	if (PCI_FUNC(pdev->devfn) != 0)
		return -ENODEV;

	i5000_get_dimm_and_channel_counts(pdev, &num_dimms_per_channel,
					&num_channels);
	num_csrows = num_dimms_per_channel * 2;

	debugf0("MC: %s(): Number of - Channels= %d  DIMMS= %d  CSROWS= %d\n",
		__func__, num_channels, num_dimms_per_channel, num_csrows);

	
	mci = edac_mc_alloc(sizeof(*pvt), num_csrows, num_channels, 0);

	if (mci == NULL)
		return -ENOMEM;

	kobject_get(&mci->edac_mci_kobj);
	debugf0("MC: %s: %s(): mci = %p\n", __FILE__, __func__, mci);

	mci->dev = &pdev->dev;	

	pvt = mci->pvt_info;
	pvt->system_address = pdev;	
	pvt->maxch = num_channels;
	pvt->maxdimmperch = num_dimms_per_channel;

	
	if (i5000_get_devices(mci, dev_idx))
		goto fail0;

	
	i5000_get_mc_regs(mci);	

	mci->mc_idx = 0;
	mci->mtype_cap = MEM_FLAG_FB_DDR2;
	mci->edac_ctl_cap = EDAC_FLAG_NONE;
	mci->edac_cap = EDAC_FLAG_NONE;
	mci->mod_name = "i5000_edac.c";
	mci->mod_ver = I5000_REVISION;
	mci->ctl_name = i5000_devs[dev_idx].ctl_name;
	mci->dev_name = pci_name(pdev);
	mci->ctl_page_to_phys = NULL;

	
	mci->edac_check = i5000_check_error;

	if (i5000_init_csrows(mci)) {
		debugf0("MC: Setting mci->edac_cap to EDAC_FLAG_NONE\n"
			"    because i5000_init_csrows() returned nonzero "
			"value\n");
		mci->edac_cap = EDAC_FLAG_NONE;	
	} else {
		debugf1("MC: Enable error reporting now\n");
		i5000_enable_error_reporting(mci);
	}

	
	if (edac_mc_add_mc(mci)) {
		debugf0("MC: %s: %s(): failed edac_mc_add_mc()\n",
			__FILE__, __func__);
		goto fail1;
	}

	i5000_clear_error(mci);

	
	i5000_pci = edac_pci_create_generic_ctl(&pdev->dev, EDAC_MOD_STR);
	if (!i5000_pci) {
		printk(KERN_WARNING
			"%s(): Unable to create PCI control\n",
			__func__);
		printk(KERN_WARNING
			"%s(): PCI error report via EDAC not setup\n",
			__func__);
	}

	return 0;

	
fail1:

	i5000_put_devices(mci);

fail0:
	kobject_put(&mci->edac_mci_kobj);
	edac_mc_free(mci);
	return -ENODEV;
}

static int __devinit i5000_init_one(struct pci_dev *pdev,
				const struct pci_device_id *id)
{
	int rc;

	debugf0("MC: %s: %s()\n", __FILE__, __func__);

	
	rc = pci_enable_device(pdev);
	if (rc)
		return rc;

	
	return i5000_probe1(pdev, id->driver_data);
}

static void __devexit i5000_remove_one(struct pci_dev *pdev)
{
	struct mem_ctl_info *mci;

	debugf0("%s: %s()\n", __FILE__, __func__);

	if (i5000_pci)
		edac_pci_release_generic_ctl(i5000_pci);

	if ((mci = edac_mc_del_mc(&pdev->dev)) == NULL)
		return;

	
	i5000_put_devices(mci);
	kobject_put(&mci->edac_mci_kobj);
	edac_mc_free(mci);
}

static DEFINE_PCI_DEVICE_TABLE(i5000_pci_tbl) = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_I5000_DEV16),
	 .driver_data = I5000P},

	{0,}			
};

MODULE_DEVICE_TABLE(pci, i5000_pci_tbl);

static struct pci_driver i5000_driver = {
	.name = KBUILD_BASENAME,
	.probe = i5000_init_one,
	.remove = __devexit_p(i5000_remove_one),
	.id_table = i5000_pci_tbl,
};

static int __init i5000_init(void)
{
	int pci_rc;

	debugf2("MC: %s: %s()\n", __FILE__, __func__);

       
       opstate_init();

	pci_rc = pci_register_driver(&i5000_driver);

	return (pci_rc < 0) ? pci_rc : 0;
}

static void __exit i5000_exit(void)
{
	debugf2("MC: %s: %s()\n", __FILE__, __func__);
	pci_unregister_driver(&i5000_driver);
}

module_init(i5000_init);
module_exit(i5000_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR
    ("Linux Networx (http://lnxi.com) Doug Thompson <norsk5@xmission.com>");
MODULE_DESCRIPTION("MC Driver for Intel I5000 memory controllers - "
		I5000_REVISION);

module_param(edac_op_state, int, 0444);
MODULE_PARM_DESC(edac_op_state, "EDAC Error Reporting state: 0=Poll,1=NMI");
module_param(misc_messages, int, 0444);
MODULE_PARM_DESC(misc_messages, "Log miscellaneous non fatal messages");

