/*
 * Intel 7300 class Memory Controllers kernel module (Clarksboro)
 *
 * This file may be distributed under the terms of the
 * GNU General Public License version 2 only.
 *
 * Copyright (c) 2010 by:
 *	 Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * Red Hat Inc. http://www.redhat.com
 *
 * Intel 7300 Chipset Memory Controller Hub (MCH) - Datasheet
 *	http://www.intel.com/Assets/PDF/datasheet/318082.pdf
 *
 * TODO: The chipset allow checking for PCI Express errors also. Currently,
 *	 the driver covers only memory error errors
 *
 * This driver uses "csrows" EDAC attribute to represent DIMM slot#
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/slab.h>
#include <linux/edac.h>
#include <linux/mmzone.h>

#include "edac_core.h"

#define I7300_REVISION    " Ver: 1.0.0"

#define EDAC_MOD_STR      "i7300_edac"

#define i7300_printk(level, fmt, arg...) \
	edac_printk(level, "i7300", fmt, ##arg)

#define i7300_mc_printk(mci, level, fmt, arg...) \
	edac_mc_chipset_printk(mci, level, "i7300", fmt, ##arg)



#define MAX_SLOTS		8
#define MAX_BRANCHES		2
#define MAX_CH_PER_BRANCH	2
#define MAX_CHANNELS		(MAX_CH_PER_BRANCH * MAX_BRANCHES)
#define MAX_MIR			3

#define to_channel(ch, branch)	((((branch)) << 1) | (ch))

#define to_csrow(slot, ch, branch)					\
		(to_channel(ch, branch) | ((slot) << 2))

struct i7300_dev_info {
	const char *ctl_name;	
	u16 fsb_mapping_errors;	
};

static const struct i7300_dev_info i7300_devs[] = {
	{
		.ctl_name = "I7300",
		.fsb_mapping_errors = PCI_DEVICE_ID_INTEL_I7300_MCH_ERR,
	},
};

struct i7300_dimm_info {
	int megabytes;		
};

struct i7300_pvt {
	struct pci_dev *pci_dev_16_0_fsb_ctlr;		
	struct pci_dev *pci_dev_16_1_fsb_addr_map;	
	struct pci_dev *pci_dev_16_2_fsb_err_regs;	
	struct pci_dev *pci_dev_2x_0_fbd_branch[MAX_BRANCHES];	

	u16 tolm;				
	u64 ambase;				

	u32 mc_settings;			
	u32 mc_settings_a;

	u16 mir[MAX_MIR];			

	u16 mtr[MAX_SLOTS][MAX_BRANCHES];	
	u16 ambpresent[MAX_CHANNELS];		

	
	struct i7300_dimm_info dimm_info[MAX_SLOTS][MAX_CHANNELS];

	
	char *tmp_prt_buffer;
};

static struct edac_pci_ctl_info *i7300_pci;



	
#define AMBASE			0x48 
#define MAXCH			0x56 
#define MAXDIMMPERCH		0x57 

	
#define MC_SETTINGS		0x40
  #define IS_MIRRORED(mc)		((mc) & (1 << 16))
  #define IS_ECC_ENABLED(mc)		((mc) & (1 << 5))
  #define IS_RETRY_ENABLED(mc)		((mc) & (1 << 31))
  #define IS_SCRBALGO_ENHANCED(mc)	((mc) & (1 << 8))

#define MC_SETTINGS_A		0x58
  #define IS_SINGLE_MODE(mca)		((mca) & (1 << 14))

#define TOLM			0x6C

#define MIR0			0x80
#define MIR1			0x84
#define MIR2			0x88

#define AMBPRESENT_0	0x64
#define AMBPRESENT_1	0x66

static const u16 mtr_regs[MAX_SLOTS] = {
	0x80, 0x84, 0x88, 0x8c,
	0x82, 0x86, 0x8a, 0x8e
};

#define MTR_DIMMS_PRESENT(mtr)		((mtr) & (1 << 8))
#define MTR_DIMMS_ETHROTTLE(mtr)	((mtr) & (1 << 7))
#define MTR_DRAM_WIDTH(mtr)		(((mtr) & (1 << 6)) ? 8 : 4)
#define MTR_DRAM_BANKS(mtr)		(((mtr) & (1 << 5)) ? 8 : 4)
#define MTR_DIMM_RANKS(mtr)		(((mtr) & (1 << 4)) ? 1 : 0)
#define MTR_DIMM_ROWS(mtr)		(((mtr) >> 2) & 0x3)
#define MTR_DRAM_BANKS_ADDR_BITS	2
#define MTR_DIMM_ROWS_ADDR_BITS(mtr)	(MTR_DIMM_ROWS(mtr) + 13)
#define MTR_DIMM_COLS(mtr)		((mtr) & 0x3)
#define MTR_DIMM_COLS_ADDR_BITS(mtr)	(MTR_DIMM_COLS(mtr) + 10)

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


#define FERR_FAT_FBD	0x98
static const char *ferr_fat_fbd_name[] = {
	[22] = "Non-Redundant Fast Reset Timeout",
	[2]  = ">Tmid Thermal event with intelligent throttling disabled",
	[1]  = "Memory or FBD configuration CRC read error",
	[0]  = "Memory Write error on non-redundant retry or "
	       "FBD configuration Write error on retry",
};
#define GET_FBD_FAT_IDX(fbderr)	(fbderr & (3 << 28))
#define FERR_FAT_FBD_ERR_MASK ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3))

#define FERR_NF_FBD	0xa0
static const char *ferr_nf_fbd_name[] = {
	[24] = "DIMM-Spare Copy Completed",
	[23] = "DIMM-Spare Copy Initiated",
	[22] = "Redundant Fast Reset Timeout",
	[21] = "Memory Write error on redundant retry",
	[18] = "SPD protocol Error",
	[17] = "FBD Northbound parity error on FBD Sync Status",
	[16] = "Correctable Patrol Data ECC",
	[15] = "Correctable Resilver- or Spare-Copy Data ECC",
	[14] = "Correctable Mirrored Demand Data ECC",
	[13] = "Correctable Non-Mirrored Demand Data ECC",
	[11] = "Memory or FBD configuration CRC read error",
	[10] = "FBD Configuration Write error on first attempt",
	[9]  = "Memory Write error on first attempt",
	[8]  = "Non-Aliased Uncorrectable Patrol Data ECC",
	[7]  = "Non-Aliased Uncorrectable Resilver- or Spare-Copy Data ECC",
	[6]  = "Non-Aliased Uncorrectable Mirrored Demand Data ECC",
	[5]  = "Non-Aliased Uncorrectable Non-Mirrored Demand Data ECC",
	[4]  = "Aliased Uncorrectable Patrol Data ECC",
	[3]  = "Aliased Uncorrectable Resilver- or Spare-Copy Data ECC",
	[2]  = "Aliased Uncorrectable Mirrored Demand Data ECC",
	[1]  = "Aliased Uncorrectable Non-Mirrored Demand Data ECC",
	[0]  = "Uncorrectable Data ECC on Replay",
};
#define GET_FBD_NF_IDX(fbderr)	(fbderr & (3 << 28))
#define FERR_NF_FBD_ERR_MASK ((1 << 24) | (1 << 23) | (1 << 22) | (1 << 21) |\
			      (1 << 18) | (1 << 17) | (1 << 16) | (1 << 15) |\
			      (1 << 14) | (1 << 13) | (1 << 11) | (1 << 10) |\
			      (1 << 9)  | (1 << 8)  | (1 << 7)  | (1 << 6)  |\
			      (1 << 5)  | (1 << 4)  | (1 << 3)  | (1 << 2)  |\
			      (1 << 1)  | (1 << 0))

#define EMASK_FBD	0xa8
#define EMASK_FBD_ERR_MASK ((1 << 27) | (1 << 26) | (1 << 25) | (1 << 24) |\
			    (1 << 22) | (1 << 21) | (1 << 20) | (1 << 19) |\
			    (1 << 18) | (1 << 17) | (1 << 16) | (1 << 14) |\
			    (1 << 13) | (1 << 12) | (1 << 11) | (1 << 10) |\
			    (1 << 9)  | (1 << 8)  | (1 << 7)  | (1 << 6)  |\
			    (1 << 5)  | (1 << 4)  | (1 << 3)  | (1 << 2)  |\
			    (1 << 1)  | (1 << 0))


#define FERR_GLOBAL_HI	0x48
static const char *ferr_global_hi_name[] = {
	[3] = "FSB 3 Fatal Error",
	[2] = "FSB 2 Fatal Error",
	[1] = "FSB 1 Fatal Error",
	[0] = "FSB 0 Fatal Error",
};
#define ferr_global_hi_is_fatal(errno)	1

#define FERR_GLOBAL_LO	0x40
static const char *ferr_global_lo_name[] = {
	[31] = "Internal MCH Fatal Error",
	[30] = "Intel QuickData Technology Device Fatal Error",
	[29] = "FSB1 Fatal Error",
	[28] = "FSB0 Fatal Error",
	[27] = "FBD Channel 3 Fatal Error",
	[26] = "FBD Channel 2 Fatal Error",
	[25] = "FBD Channel 1 Fatal Error",
	[24] = "FBD Channel 0 Fatal Error",
	[23] = "PCI Express Device 7Fatal Error",
	[22] = "PCI Express Device 6 Fatal Error",
	[21] = "PCI Express Device 5 Fatal Error",
	[20] = "PCI Express Device 4 Fatal Error",
	[19] = "PCI Express Device 3 Fatal Error",
	[18] = "PCI Express Device 2 Fatal Error",
	[17] = "PCI Express Device 1 Fatal Error",
	[16] = "ESI Fatal Error",
	[15] = "Internal MCH Non-Fatal Error",
	[14] = "Intel QuickData Technology Device Non Fatal Error",
	[13] = "FSB1 Non-Fatal Error",
	[12] = "FSB 0 Non-Fatal Error",
	[11] = "FBD Channel 3 Non-Fatal Error",
	[10] = "FBD Channel 2 Non-Fatal Error",
	[9]  = "FBD Channel 1 Non-Fatal Error",
	[8]  = "FBD Channel 0 Non-Fatal Error",
	[7]  = "PCI Express Device 7 Non-Fatal Error",
	[6]  = "PCI Express Device 6 Non-Fatal Error",
	[5]  = "PCI Express Device 5 Non-Fatal Error",
	[4]  = "PCI Express Device 4 Non-Fatal Error",
	[3]  = "PCI Express Device 3 Non-Fatal Error",
	[2]  = "PCI Express Device 2 Non-Fatal Error",
	[1]  = "PCI Express Device 1 Non-Fatal Error",
	[0]  = "ESI Non-Fatal Error",
};
#define ferr_global_lo_is_fatal(errno)	((errno < 16) ? 0 : 1)

#define NRECMEMA	0xbe
  #define NRECMEMA_BANK(v)	(((v) >> 12) & 7)
  #define NRECMEMA_RANK(v)	(((v) >> 8) & 15)

#define NRECMEMB	0xc0
  #define NRECMEMB_IS_WR(v)	((v) & (1 << 31))
  #define NRECMEMB_CAS(v)	(((v) >> 16) & 0x1fff)
  #define NRECMEMB_RAS(v)	((v) & 0xffff)

#define REDMEMA		0xdc

#define REDMEMB		0x7c
  #define IS_SECOND_CH(v)	((v) * (1 << 17))

#define RECMEMA		0xe0
  #define RECMEMA_BANK(v)	(((v) >> 12) & 7)
  #define RECMEMA_RANK(v)	(((v) >> 8) & 15)

#define RECMEMB		0xe4
  #define RECMEMB_IS_WR(v)	((v) & (1 << 31))
  #define RECMEMB_CAS(v)	(((v) >> 16) & 0x1fff)
  #define RECMEMB_RAS(v)	((v) & 0xffff)


static const char *get_err_from_table(const char *table[], int size, int pos)
{
	if (unlikely(pos >= size))
		return "Reserved";

	if (unlikely(!table[pos]))
		return "Reserved";

	return table[pos];
}

#define GET_ERR_FROM_TABLE(table, pos)				\
	get_err_from_table(table, ARRAY_SIZE(table), pos)

static void i7300_process_error_global(struct mem_ctl_info *mci)
{
	struct i7300_pvt *pvt;
	u32 errnum, error_reg;
	unsigned long errors;
	const char *specific;
	bool is_fatal;

	pvt = mci->pvt_info;

	
	pci_read_config_dword(pvt->pci_dev_16_2_fsb_err_regs,
			      FERR_GLOBAL_HI, &error_reg);
	if (unlikely(error_reg)) {
		errors = error_reg;
		errnum = find_first_bit(&errors,
					ARRAY_SIZE(ferr_global_hi_name));
		specific = GET_ERR_FROM_TABLE(ferr_global_hi_name, errnum);
		is_fatal = ferr_global_hi_is_fatal(errnum);

		
		pci_write_config_dword(pvt->pci_dev_16_2_fsb_err_regs,
				       FERR_GLOBAL_HI, error_reg);

		goto error_global;
	}

	pci_read_config_dword(pvt->pci_dev_16_2_fsb_err_regs,
			      FERR_GLOBAL_LO, &error_reg);
	if (unlikely(error_reg)) {
		errors = error_reg;
		errnum = find_first_bit(&errors,
					ARRAY_SIZE(ferr_global_lo_name));
		specific = GET_ERR_FROM_TABLE(ferr_global_lo_name, errnum);
		is_fatal = ferr_global_lo_is_fatal(errnum);

		
		pci_write_config_dword(pvt->pci_dev_16_2_fsb_err_regs,
				       FERR_GLOBAL_LO, error_reg);

		goto error_global;
	}
	return;

error_global:
	i7300_mc_printk(mci, KERN_EMERG, "%s misc error: %s\n",
			is_fatal ? "Fatal" : "NOT fatal", specific);
}

static void i7300_process_fbd_error(struct mem_ctl_info *mci)
{
	struct i7300_pvt *pvt;
	u32 errnum, value, error_reg;
	u16 val16;
	unsigned branch, channel, bank, rank, cas, ras;
	u32 syndrome;

	unsigned long errors;
	const char *specific;
	bool is_wr;

	pvt = mci->pvt_info;

	
	pci_read_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
			      FERR_FAT_FBD, &error_reg);
	if (unlikely(error_reg & FERR_FAT_FBD_ERR_MASK)) {
		errors = error_reg & FERR_FAT_FBD_ERR_MASK ;
		errnum = find_first_bit(&errors,
					ARRAY_SIZE(ferr_fat_fbd_name));
		specific = GET_ERR_FROM_TABLE(ferr_fat_fbd_name, errnum);
		branch = (GET_FBD_FAT_IDX(error_reg) == 2) ? 1 : 0;

		pci_read_config_word(pvt->pci_dev_16_1_fsb_addr_map,
				     NRECMEMA, &val16);
		bank = NRECMEMA_BANK(val16);
		rank = NRECMEMA_RANK(val16);

		pci_read_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
				NRECMEMB, &value);
		is_wr = NRECMEMB_IS_WR(value);
		cas = NRECMEMB_CAS(value);
		ras = NRECMEMB_RAS(value);

		
		pci_write_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
				FERR_FAT_FBD, error_reg);

		snprintf(pvt->tmp_prt_buffer, PAGE_SIZE,
			"FATAL (Branch=%d DRAM-Bank=%d %s "
			"RAS=%d CAS=%d Err=0x%lx (%s))",
			branch, bank,
			is_wr ? "RDWR" : "RD",
			ras, cas,
			errors, specific);

		
		edac_mc_handle_fbd_ue(mci, rank, branch << 1,
				      (branch << 1) + 1,
				      pvt->tmp_prt_buffer);
	}

	
	pci_read_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
			      FERR_NF_FBD, &error_reg);
	if (unlikely(error_reg & FERR_NF_FBD_ERR_MASK)) {
		errors = error_reg & FERR_NF_FBD_ERR_MASK;
		errnum = find_first_bit(&errors,
					ARRAY_SIZE(ferr_nf_fbd_name));
		specific = GET_ERR_FROM_TABLE(ferr_nf_fbd_name, errnum);
		branch = (GET_FBD_FAT_IDX(error_reg) == 2) ? 1 : 0;

		pci_read_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
			REDMEMA, &syndrome);

		pci_read_config_word(pvt->pci_dev_16_1_fsb_addr_map,
				     RECMEMA, &val16);
		bank = RECMEMA_BANK(val16);
		rank = RECMEMA_RANK(val16);

		pci_read_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
				RECMEMB, &value);
		is_wr = RECMEMB_IS_WR(value);
		cas = RECMEMB_CAS(value);
		ras = RECMEMB_RAS(value);

		pci_read_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
				     REDMEMB, &value);
		channel = (branch << 1);
		if (IS_SECOND_CH(value))
			channel++;

		
		pci_write_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
				FERR_NF_FBD, error_reg);

		
		snprintf(pvt->tmp_prt_buffer, PAGE_SIZE,
			"Corrected error (Branch=%d, Channel %d), "
			" DRAM-Bank=%d %s "
			"RAS=%d CAS=%d, CE Err=0x%lx, Syndrome=0x%08x(%s))",
			branch, channel,
			bank,
			is_wr ? "RDWR" : "RD",
			ras, cas,
			errors, syndrome, specific);

		edac_mc_handle_fbd_ce(mci, rank, channel,
				      pvt->tmp_prt_buffer);
	}
	return;
}

static void i7300_check_error(struct mem_ctl_info *mci)
{
	i7300_process_error_global(mci);
	i7300_process_fbd_error(mci);
};

static void i7300_clear_error(struct mem_ctl_info *mci)
{
	struct i7300_pvt *pvt = mci->pvt_info;
	u32 value;

	
	pci_read_config_dword(pvt->pci_dev_16_2_fsb_err_regs,
			      FERR_GLOBAL_HI, &value);
	pci_write_config_dword(pvt->pci_dev_16_2_fsb_err_regs,
			      FERR_GLOBAL_HI, value);

	pci_read_config_dword(pvt->pci_dev_16_2_fsb_err_regs,
			      FERR_GLOBAL_LO, &value);
	pci_write_config_dword(pvt->pci_dev_16_2_fsb_err_regs,
			      FERR_GLOBAL_LO, value);

	
	pci_read_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
			      FERR_FAT_FBD, &value);
	pci_write_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
			      FERR_FAT_FBD, value);

	pci_read_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
			      FERR_NF_FBD, &value);
	pci_write_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
			      FERR_NF_FBD, value);
}

static void i7300_enable_error_reporting(struct mem_ctl_info *mci)
{
	struct i7300_pvt *pvt = mci->pvt_info;
	u32 fbd_error_mask;

	
	pci_read_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
			      EMASK_FBD, &fbd_error_mask);

	
	fbd_error_mask &= ~(EMASK_FBD_ERR_MASK);

	pci_write_config_dword(pvt->pci_dev_16_1_fsb_addr_map,
			       EMASK_FBD, fbd_error_mask);
}


static int decode_mtr(struct i7300_pvt *pvt,
		      int slot, int ch, int branch,
		      struct i7300_dimm_info *dinfo,
		      struct csrow_info *p_csrow,
		      u32 *nr_pages)
{
	int mtr, ans, addrBits, channel;

	channel = to_channel(ch, branch);

	mtr = pvt->mtr[slot][branch];
	ans = MTR_DIMMS_PRESENT(mtr) ? 1 : 0;

	debugf2("\tMTR%d CH%d: DIMMs are %s (mtr)\n",
		slot, channel,
		ans ? "Present" : "NOT Present");

	
	if (!ans)
		return 0;

	addrBits = MTR_DRAM_BANKS_ADDR_BITS;
	
	addrBits += MTR_DIMM_ROWS_ADDR_BITS(mtr);
	
	addrBits += MTR_DIMM_COLS_ADDR_BITS(mtr);
	
	addrBits += MTR_DIMM_RANKS(mtr);

	addrBits += 6;	
	addrBits -= 20;	
	addrBits -= 3;	

	dinfo->megabytes = 1 << addrBits;
	*nr_pages = dinfo->megabytes << 8;

	debugf2("\t\tWIDTH: x%d\n", MTR_DRAM_WIDTH(mtr));

	debugf2("\t\tELECTRICAL THROTTLING is %s\n",
		MTR_DIMMS_ETHROTTLE(mtr) ? "enabled" : "disabled");

	debugf2("\t\tNUMBANK: %d bank(s)\n", MTR_DRAM_BANKS(mtr));
	debugf2("\t\tNUMRANK: %s\n", MTR_DIMM_RANKS(mtr) ? "double" : "single");
	debugf2("\t\tNUMROW: %s\n", numrow_toString[MTR_DIMM_ROWS(mtr)]);
	debugf2("\t\tNUMCOL: %s\n", numcol_toString[MTR_DIMM_COLS(mtr)]);
	debugf2("\t\tSIZE: %d MB\n", dinfo->megabytes);

	p_csrow->grain = 8;
	p_csrow->mtype = MEM_FB_DDR2;
	p_csrow->csrow_idx = slot;
	p_csrow->page_mask = 0;


	if (IS_SINGLE_MODE(pvt->mc_settings_a)) {
		p_csrow->edac_mode = EDAC_SECDED;
		debugf2("\t\tECC code is 8-byte-over-32-byte SECDED+ code\n");
	} else {
		debugf2("\t\tECC code is on Lockstep mode\n");
		if (MTR_DRAM_WIDTH(mtr) == 8)
			p_csrow->edac_mode = EDAC_S8ECD8ED;
		else
			p_csrow->edac_mode = EDAC_S4ECD4ED;
	}

	
	if (MTR_DRAM_WIDTH(mtr) == 8) {
		debugf2("\t\tScrub algorithm for x8 is on %s mode\n",
			IS_SCRBALGO_ENHANCED(pvt->mc_settings) ?
					    "enhanced" : "normal");

		p_csrow->dtype = DEV_X8;
	} else
		p_csrow->dtype = DEV_X4;

	return mtr;
}

static void print_dimm_size(struct i7300_pvt *pvt)
{
#ifdef CONFIG_EDAC_DEBUG
	struct i7300_dimm_info *dinfo;
	char *p;
	int space, n;
	int channel, slot;

	space = PAGE_SIZE;
	p = pvt->tmp_prt_buffer;

	n = snprintf(p, space, "              ");
	p += n;
	space -= n;
	for (channel = 0; channel < MAX_CHANNELS; channel++) {
		n = snprintf(p, space, "channel %d | ", channel);
		p += n;
		space -= n;
	}
	debugf2("%s\n", pvt->tmp_prt_buffer);
	p = pvt->tmp_prt_buffer;
	space = PAGE_SIZE;
	n = snprintf(p, space, "-------------------------------"
			       "------------------------------");
	p += n;
	space -= n;
	debugf2("%s\n", pvt->tmp_prt_buffer);
	p = pvt->tmp_prt_buffer;
	space = PAGE_SIZE;

	for (slot = 0; slot < MAX_SLOTS; slot++) {
		n = snprintf(p, space, "csrow/SLOT %d  ", slot);
		p += n;
		space -= n;

		for (channel = 0; channel < MAX_CHANNELS; channel++) {
			dinfo = &pvt->dimm_info[slot][channel];
			n = snprintf(p, space, "%4d MB   | ", dinfo->megabytes);
			p += n;
			space -= n;
		}

		debugf2("%s\n", pvt->tmp_prt_buffer);
		p = pvt->tmp_prt_buffer;
		space = PAGE_SIZE;
	}

	n = snprintf(p, space, "-------------------------------"
			       "------------------------------");
	p += n;
	space -= n;
	debugf2("%s\n", pvt->tmp_prt_buffer);
	p = pvt->tmp_prt_buffer;
	space = PAGE_SIZE;
#endif
}

static int i7300_init_csrows(struct mem_ctl_info *mci)
{
	struct i7300_pvt *pvt;
	struct i7300_dimm_info *dinfo;
	struct csrow_info *p_csrow;
	int rc = -ENODEV;
	int mtr;
	int ch, branch, slot, channel;
	u32 last_page = 0, nr_pages;

	pvt = mci->pvt_info;

	debugf2("Memory Technology Registers:\n");

	
	for (branch = 0; branch < MAX_BRANCHES; branch++) {
		
		channel = to_channel(0, branch);
		pci_read_config_word(pvt->pci_dev_2x_0_fbd_branch[branch],
				     AMBPRESENT_0,
				&pvt->ambpresent[channel]);
		debugf2("\t\tAMB-present CH%d = 0x%x:\n",
			channel, pvt->ambpresent[channel]);

		channel = to_channel(1, branch);
		pci_read_config_word(pvt->pci_dev_2x_0_fbd_branch[branch],
				     AMBPRESENT_1,
				&pvt->ambpresent[channel]);
		debugf2("\t\tAMB-present CH%d = 0x%x:\n",
			channel, pvt->ambpresent[channel]);
	}

	
	for (slot = 0; slot < MAX_SLOTS; slot++) {
		int where = mtr_regs[slot];
		for (branch = 0; branch < MAX_BRANCHES; branch++) {
			pci_read_config_word(pvt->pci_dev_2x_0_fbd_branch[branch],
					where,
					&pvt->mtr[slot][branch]);
			for (ch = 0; ch < MAX_BRANCHES; ch++) {
				int channel = to_channel(ch, branch);

				dinfo = &pvt->dimm_info[slot][channel];
				p_csrow = &mci->csrows[slot];

				mtr = decode_mtr(pvt, slot, ch, branch,
						 dinfo, p_csrow, &nr_pages);
				
				if (!MTR_DIMMS_PRESENT(mtr))
					continue;

				
				p_csrow->nr_pages += nr_pages;
				p_csrow->first_page = last_page;
				last_page += nr_pages;
				p_csrow->last_page = last_page;

				rc = 0;
			}
		}
	}

	return rc;
}

static void decode_mir(int mir_no, u16 mir[MAX_MIR])
{
	if (mir[mir_no] & 3)
		debugf2("MIR%d: limit= 0x%x Branch(es) that participate:"
			" %s %s\n",
			mir_no,
			(mir[mir_no] >> 4) & 0xfff,
			(mir[mir_no] & 1) ? "B0" : "",
			(mir[mir_no] & 2) ? "B1" : "");
}

static int i7300_get_mc_regs(struct mem_ctl_info *mci)
{
	struct i7300_pvt *pvt;
	u32 actual_tolm;
	int i, rc;

	pvt = mci->pvt_info;

	pci_read_config_dword(pvt->pci_dev_16_0_fsb_ctlr, AMBASE,
			(u32 *) &pvt->ambase);

	debugf2("AMBASE= 0x%lx\n", (long unsigned int)pvt->ambase);

	
	pci_read_config_word(pvt->pci_dev_16_1_fsb_addr_map, TOLM, &pvt->tolm);
	pvt->tolm >>= 12;
	debugf2("TOLM (number of 256M regions) =%u (0x%x)\n", pvt->tolm,
		pvt->tolm);

	actual_tolm = (u32) ((1000l * pvt->tolm) >> (30 - 28));
	debugf2("Actual TOLM byte addr=%u.%03u GB (0x%x)\n",
		actual_tolm/1000, actual_tolm % 1000, pvt->tolm << 28);

	
	pci_read_config_dword(pvt->pci_dev_16_1_fsb_addr_map, MC_SETTINGS,
			     &pvt->mc_settings);
	pci_read_config_dword(pvt->pci_dev_16_1_fsb_addr_map, MC_SETTINGS_A,
			     &pvt->mc_settings_a);

	if (IS_SINGLE_MODE(pvt->mc_settings_a))
		debugf0("Memory controller operating on single mode\n");
	else
		debugf0("Memory controller operating on %s mode\n",
		IS_MIRRORED(pvt->mc_settings) ? "mirrored" : "non-mirrored");

	debugf0("Error detection is %s\n",
		IS_ECC_ENABLED(pvt->mc_settings) ? "enabled" : "disabled");
	debugf0("Retry is %s\n",
		IS_RETRY_ENABLED(pvt->mc_settings) ? "enabled" : "disabled");

	
	pci_read_config_word(pvt->pci_dev_16_1_fsb_addr_map, MIR0,
			     &pvt->mir[0]);
	pci_read_config_word(pvt->pci_dev_16_1_fsb_addr_map, MIR1,
			     &pvt->mir[1]);
	pci_read_config_word(pvt->pci_dev_16_1_fsb_addr_map, MIR2,
			     &pvt->mir[2]);

	
	for (i = 0; i < MAX_MIR; i++)
		decode_mir(i, pvt->mir);

	rc = i7300_init_csrows(mci);
	if (rc < 0)
		return rc;

	print_dimm_size(pvt);

	return 0;
}


static void i7300_put_devices(struct mem_ctl_info *mci)
{
	struct i7300_pvt *pvt;
	int branch;

	pvt = mci->pvt_info;

	
	for (branch = 0; branch < MAX_CH_PER_BRANCH; branch++)
		pci_dev_put(pvt->pci_dev_2x_0_fbd_branch[branch]);
	pci_dev_put(pvt->pci_dev_16_2_fsb_err_regs);
	pci_dev_put(pvt->pci_dev_16_1_fsb_addr_map);
}

static int __devinit i7300_get_devices(struct mem_ctl_info *mci)
{
	struct i7300_pvt *pvt;
	struct pci_dev *pdev;

	pvt = mci->pvt_info;

	
	pdev = NULL;
	while (!pvt->pci_dev_16_1_fsb_addr_map ||
	       !pvt->pci_dev_16_2_fsb_err_regs) {
		pdev = pci_get_device(PCI_VENDOR_ID_INTEL,
				      PCI_DEVICE_ID_INTEL_I7300_MCH_ERR, pdev);
		if (!pdev) {
			
			i7300_printk(KERN_ERR,
				"'system address,Process Bus' "
				"device not found:"
				"vendor 0x%x device 0x%x ERR funcs "
				"(broken BIOS?)\n",
				PCI_VENDOR_ID_INTEL,
				PCI_DEVICE_ID_INTEL_I7300_MCH_ERR);
			goto error;
		}

		
		switch (PCI_FUNC(pdev->devfn)) {
		case 1:
			pvt->pci_dev_16_1_fsb_addr_map = pdev;
			break;
		case 2:
			pvt->pci_dev_16_2_fsb_err_regs = pdev;
			break;
		}
	}

	debugf1("System Address, processor bus- PCI Bus ID: %s  %x:%x\n",
		pci_name(pvt->pci_dev_16_0_fsb_ctlr),
		pvt->pci_dev_16_0_fsb_ctlr->vendor,
		pvt->pci_dev_16_0_fsb_ctlr->device);
	debugf1("Branchmap, control and errors - PCI Bus ID: %s  %x:%x\n",
		pci_name(pvt->pci_dev_16_1_fsb_addr_map),
		pvt->pci_dev_16_1_fsb_addr_map->vendor,
		pvt->pci_dev_16_1_fsb_addr_map->device);
	debugf1("FSB Error Regs - PCI Bus ID: %s  %x:%x\n",
		pci_name(pvt->pci_dev_16_2_fsb_err_regs),
		pvt->pci_dev_16_2_fsb_err_regs->vendor,
		pvt->pci_dev_16_2_fsb_err_regs->device);

	pvt->pci_dev_2x_0_fbd_branch[0] = pci_get_device(PCI_VENDOR_ID_INTEL,
					    PCI_DEVICE_ID_INTEL_I7300_MCH_FB0,
					    NULL);
	if (!pvt->pci_dev_2x_0_fbd_branch[0]) {
		i7300_printk(KERN_ERR,
			"MC: 'BRANCH 0' device not found:"
			"vendor 0x%x device 0x%x Func 0 (broken BIOS?)\n",
			PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_I7300_MCH_FB0);
		goto error;
	}

	pvt->pci_dev_2x_0_fbd_branch[1] = pci_get_device(PCI_VENDOR_ID_INTEL,
					    PCI_DEVICE_ID_INTEL_I7300_MCH_FB1,
					    NULL);
	if (!pvt->pci_dev_2x_0_fbd_branch[1]) {
		i7300_printk(KERN_ERR,
			"MC: 'BRANCH 1' device not found:"
			"vendor 0x%x device 0x%x Func 0 "
			"(broken BIOS?)\n",
			PCI_VENDOR_ID_INTEL,
			PCI_DEVICE_ID_INTEL_I7300_MCH_FB1);
		goto error;
	}

	return 0;

error:
	i7300_put_devices(mci);
	return -ENODEV;
}

static int __devinit i7300_init_one(struct pci_dev *pdev,
				    const struct pci_device_id *id)
{
	struct mem_ctl_info *mci;
	struct i7300_pvt *pvt;
	int num_channels;
	int num_dimms_per_channel;
	int num_csrows;
	int rc;

	
	rc = pci_enable_device(pdev);
	if (rc == -EIO)
		return rc;

	debugf0("MC: " __FILE__ ": %s(), pdev bus %u dev=0x%x fn=0x%x\n",
		__func__,
		pdev->bus->number,
		PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

	
	if (PCI_FUNC(pdev->devfn) != 0)
		return -ENODEV;

	num_dimms_per_channel = MAX_SLOTS;
	num_channels = MAX_CHANNELS;
	num_csrows = MAX_SLOTS * MAX_CHANNELS;

	debugf0("MC: %s(): Number of - Channels= %d  DIMMS= %d  CSROWS= %d\n",
		__func__, num_channels, num_dimms_per_channel, num_csrows);

	
	mci = edac_mc_alloc(sizeof(*pvt), num_csrows, num_channels, 0);

	if (mci == NULL)
		return -ENOMEM;

	debugf0("MC: " __FILE__ ": %s(): mci = %p\n", __func__, mci);

	mci->dev = &pdev->dev;	

	pvt = mci->pvt_info;
	pvt->pci_dev_16_0_fsb_ctlr = pdev;	

	pvt->tmp_prt_buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!pvt->tmp_prt_buffer) {
		edac_mc_free(mci);
		return -ENOMEM;
	}

	
	if (i7300_get_devices(mci))
		goto fail0;

	mci->mc_idx = 0;
	mci->mtype_cap = MEM_FLAG_FB_DDR2;
	mci->edac_ctl_cap = EDAC_FLAG_NONE;
	mci->edac_cap = EDAC_FLAG_NONE;
	mci->mod_name = "i7300_edac.c";
	mci->mod_ver = I7300_REVISION;
	mci->ctl_name = i7300_devs[0].ctl_name;
	mci->dev_name = pci_name(pdev);
	mci->ctl_page_to_phys = NULL;

	
	mci->edac_check = i7300_check_error;

	if (i7300_get_mc_regs(mci)) {
		debugf0("MC: Setting mci->edac_cap to EDAC_FLAG_NONE\n"
			"    because i7300_init_csrows() returned nonzero "
			"value\n");
		mci->edac_cap = EDAC_FLAG_NONE;	
	} else {
		debugf1("MC: Enable error reporting now\n");
		i7300_enable_error_reporting(mci);
	}

	
	if (edac_mc_add_mc(mci)) {
		debugf0("MC: " __FILE__
			": %s(): failed edac_mc_add_mc()\n", __func__);
		goto fail1;
	}

	i7300_clear_error(mci);

	
	i7300_pci = edac_pci_create_generic_ctl(&pdev->dev, EDAC_MOD_STR);
	if (!i7300_pci) {
		printk(KERN_WARNING
			"%s(): Unable to create PCI control\n",
			__func__);
		printk(KERN_WARNING
			"%s(): PCI error report via EDAC not setup\n",
			__func__);
	}

	return 0;

	
fail1:

	i7300_put_devices(mci);

fail0:
	kfree(pvt->tmp_prt_buffer);
	edac_mc_free(mci);
	return -ENODEV;
}

static void __devexit i7300_remove_one(struct pci_dev *pdev)
{
	struct mem_ctl_info *mci;
	char *tmp;

	debugf0(__FILE__ ": %s()\n", __func__);

	if (i7300_pci)
		edac_pci_release_generic_ctl(i7300_pci);

	mci = edac_mc_del_mc(&pdev->dev);
	if (!mci)
		return;

	tmp = ((struct i7300_pvt *)mci->pvt_info)->tmp_prt_buffer;

	
	i7300_put_devices(mci);

	kfree(tmp);
	edac_mc_free(mci);
}

static DEFINE_PCI_DEVICE_TABLE(i7300_pci_tbl) = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_I7300_MCH_ERR)},
	{0,}			
};

MODULE_DEVICE_TABLE(pci, i7300_pci_tbl);

static struct pci_driver i7300_driver = {
	.name = "i7300_edac",
	.probe = i7300_init_one,
	.remove = __devexit_p(i7300_remove_one),
	.id_table = i7300_pci_tbl,
};

static int __init i7300_init(void)
{
	int pci_rc;

	debugf2("MC: " __FILE__ ": %s()\n", __func__);

	
	opstate_init();

	pci_rc = pci_register_driver(&i7300_driver);

	return (pci_rc < 0) ? pci_rc : 0;
}

static void __exit i7300_exit(void)
{
	debugf2("MC: " __FILE__ ": %s()\n", __func__);
	pci_unregister_driver(&i7300_driver);
}

module_init(i7300_init);
module_exit(i7300_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab <mchehab@redhat.com>");
MODULE_AUTHOR("Red Hat Inc. (http://www.redhat.com)");
MODULE_DESCRIPTION("MC Driver for Intel I7300 memory controllers - "
		   I7300_REVISION);

module_param(edac_op_state, int, 0444);
MODULE_PARM_DESC(edac_op_state, "EDAC Error Reporting state: 0=Poll,1=NMI");
