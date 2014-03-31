/*
 * Copyright (c) 2008 Nuovation System Designs, LLC
 *   Grant Erickson <gerickson@nuovations.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 */

#include <linux/edac.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/types.h>

#include <asm/dcr.h>

#include "edac_core.h"
#include "ppc4xx_edac.h"



#define EDAC_OPSTATE_INT_STR		"interrupt"
#define EDAC_OPSTATE_POLL_STR		"polled"
#define EDAC_OPSTATE_UNKNOWN_STR	"unknown"

#define PPC4XX_EDAC_MODULE_NAME		"ppc4xx_edac"
#define PPC4XX_EDAC_MODULE_REVISION	"v1.0.0"

#define PPC4XX_EDAC_MESSAGE_SIZE	256

#define ppc4xx_edac_printk(level, fmt, arg...) \
	edac_printk(level, "PPC4xx MC", fmt, ##arg)

#define ppc4xx_edac_mc_printk(level, mci, fmt, arg...) \
	edac_mc_chipset_printk(mci, level, "PPC4xx", fmt, ##arg)

#define SDRAM_MBCF_SZ_MiB_MIN		4
#define SDRAM_MBCF_SZ_TO_MiB(n)		(SDRAM_MBCF_SZ_MiB_MIN \
					 << (SDRAM_MBCF_SZ_DECODE(n)))
#define SDRAM_MBCF_SZ_TO_PAGES(n)	(SDRAM_MBCF_SZ_MiB_MIN \
					 << (20 - PAGE_SHIFT + \
					     SDRAM_MBCF_SZ_DECODE(n)))

#define SDRAM_DCR_RESOURCE_LEN		2
#define SDRAM_DCR_ADDR_OFFSET		0
#define SDRAM_DCR_DATA_OFFSET		1

#define INTMAP_ECCDED_INDEX		0	
#define INTMAP_ECCSEC_INDEX		1	


struct ppc4xx_edac_pdata {
	dcr_host_t dcr_host;	
	struct {
		int sec;	
		int ded;	
	} irqs;
};

struct ppc4xx_ecc_status {
	u32 ecces;
	u32 besr;
	u32 bearh;
	u32 bearl;
	u32 wmirq;
};


static int ppc4xx_edac_probe(struct platform_device *device);
static int ppc4xx_edac_remove(struct platform_device *device);


static struct of_device_id ppc4xx_edac_match[] = {
	{
		.compatible	= "ibm,sdram-4xx-ddr2"
	},
	{ }
};

static struct platform_driver ppc4xx_edac_driver = {
	.probe			= ppc4xx_edac_probe,
	.remove			= ppc4xx_edac_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = PPC4XX_EDAC_MODULE_NAME,
		.of_match_table = ppc4xx_edac_match,
	},
};

static const unsigned ppc4xx_edac_nr_csrows = 2;
static const unsigned ppc4xx_edac_nr_chans = 1;

static const char * const ppc4xx_plb_masters[9] = {
	[SDRAM_PLB_M0ID_ICU]	= "ICU",
	[SDRAM_PLB_M0ID_PCIE0]	= "PCI-E 0",
	[SDRAM_PLB_M0ID_PCIE1]	= "PCI-E 1",
	[SDRAM_PLB_M0ID_DMA]	= "DMA",
	[SDRAM_PLB_M0ID_DCU]	= "DCU",
	[SDRAM_PLB_M0ID_OPB]	= "OPB",
	[SDRAM_PLB_M0ID_MAL]	= "MAL",
	[SDRAM_PLB_M0ID_SEC]	= "SEC",
	[SDRAM_PLB_M0ID_AHB]	= "AHB"
};

static inline u32
mfsdram(const dcr_host_t *dcr_host, unsigned int idcr_n)
{
	return __mfdcri(dcr_host->base + SDRAM_DCR_ADDR_OFFSET,
			dcr_host->base + SDRAM_DCR_DATA_OFFSET,
			idcr_n);
}

static inline void
mtsdram(const dcr_host_t *dcr_host, unsigned int idcr_n, u32 value)
{
	return __mtdcri(dcr_host->base + SDRAM_DCR_ADDR_OFFSET,
			dcr_host->base + SDRAM_DCR_DATA_OFFSET,
			idcr_n,
			value);
}

static bool
ppc4xx_edac_check_bank_error(const struct ppc4xx_ecc_status *status,
			     unsigned int bank)
{
	switch (bank) {
	case 0:
		return status->ecces & SDRAM_ECCES_BK0ER;
	case 1:
		return status->ecces & SDRAM_ECCES_BK1ER;
	default:
		return false;
	}
}

static int
ppc4xx_edac_generate_bank_message(const struct mem_ctl_info *mci,
				  const struct ppc4xx_ecc_status *status,
				  char *buffer,
				  size_t size)
{
	int n, total = 0;
	unsigned int row, rows;

	n = snprintf(buffer, size, "%s: Banks: ", mci->dev_name);

	if (n < 0 || n >= size)
		goto fail;

	buffer += n;
	size -= n;
	total += n;

	for (rows = 0, row = 0; row < mci->nr_csrows; row++) {
		if (ppc4xx_edac_check_bank_error(status, row)) {
			n = snprintf(buffer, size, "%s%u",
					(rows++ ? ", " : ""), row);

			if (n < 0 || n >= size)
				goto fail;

			buffer += n;
			size -= n;
			total += n;
		}
	}

	n = snprintf(buffer, size, "%s; ", rows ? "" : "None");

	if (n < 0 || n >= size)
		goto fail;

	buffer += n;
	size -= n;
	total += n;

 fail:
	return total;
}

static int
ppc4xx_edac_generate_checkbit_message(const struct mem_ctl_info *mci,
				      const struct ppc4xx_ecc_status *status,
				      char *buffer,
				      size_t size)
{
	const struct ppc4xx_edac_pdata *pdata = mci->pvt_info;
	const char *ckber = NULL;

	switch (status->ecces & SDRAM_ECCES_CKBER_MASK) {
	case SDRAM_ECCES_CKBER_NONE:
		ckber = "None";
		break;
	case SDRAM_ECCES_CKBER_32_ECC_0_3:
		ckber = "ECC0:3";
		break;
	case SDRAM_ECCES_CKBER_32_ECC_4_8:
		switch (mfsdram(&pdata->dcr_host, SDRAM_MCOPT1) &
			SDRAM_MCOPT1_WDTH_MASK) {
		case SDRAM_MCOPT1_WDTH_16:
			ckber = "ECC0:3";
			break;
		case SDRAM_MCOPT1_WDTH_32:
			ckber = "ECC4:8";
			break;
		default:
			ckber = "Unknown";
			break;
		}
		break;
	case SDRAM_ECCES_CKBER_32_ECC_0_8:
		ckber = "ECC0:8";
		break;
	default:
		ckber = "Unknown";
		break;
	}

	return snprintf(buffer, size, "Checkbit Error: %s", ckber);
}

static int
ppc4xx_edac_generate_lane_message(const struct mem_ctl_info *mci,
				  const struct ppc4xx_ecc_status *status,
				  char *buffer,
				  size_t size)
{
	int n, total = 0;
	unsigned int lane, lanes;
	const unsigned int first_lane = 0;
	const unsigned int lane_count = 16;

	n = snprintf(buffer, size, "; Byte Lane Errors: ");

	if (n < 0 || n >= size)
		goto fail;

	buffer += n;
	size -= n;
	total += n;

	for (lanes = 0, lane = first_lane; lane < lane_count; lane++) {
		if ((status->ecces & SDRAM_ECCES_BNCE_ENCODE(lane)) != 0) {
			n = snprintf(buffer, size,
				     "%s%u",
				     (lanes++ ? ", " : ""), lane);

			if (n < 0 || n >= size)
				goto fail;

			buffer += n;
			size -= n;
			total += n;
		}
	}

	n = snprintf(buffer, size, "%s; ", lanes ? "" : "None");

	if (n < 0 || n >= size)
		goto fail;

	buffer += n;
	size -= n;
	total += n;

 fail:
	return total;
}

static int
ppc4xx_edac_generate_ecc_message(const struct mem_ctl_info *mci,
				 const struct ppc4xx_ecc_status *status,
				 char *buffer,
				 size_t size)
{
	int n, total = 0;

	n = ppc4xx_edac_generate_bank_message(mci, status, buffer, size);

	if (n < 0 || n >= size)
		goto fail;

	buffer += n;
	size -= n;
	total += n;

	n = ppc4xx_edac_generate_checkbit_message(mci, status, buffer, size);

	if (n < 0 || n >= size)
		goto fail;

	buffer += n;
	size -= n;
	total += n;

	n = ppc4xx_edac_generate_lane_message(mci, status, buffer, size);

	if (n < 0 || n >= size)
		goto fail;

	buffer += n;
	size -= n;
	total += n;

 fail:
	return total;
}

static int
ppc4xx_edac_generate_plb_message(const struct mem_ctl_info *mci,
				 const struct ppc4xx_ecc_status *status,
				 char *buffer,
				 size_t size)
{
	unsigned int master;
	bool read;

	if ((status->besr & SDRAM_BESR_MASK) == 0)
		return 0;

	if ((status->besr & SDRAM_BESR_M0ET_MASK) == SDRAM_BESR_M0ET_NONE)
		return 0;

	read = ((status->besr & SDRAM_BESR_M0RW_MASK) == SDRAM_BESR_M0RW_READ);

	master = SDRAM_BESR_M0ID_DECODE(status->besr);

	return snprintf(buffer, size,
			"%s error w/ PLB master %u \"%s\"; ",
			(read ? "Read" : "Write"),
			master,
			(((master >= SDRAM_PLB_M0ID_FIRST) &&
			  (master <= SDRAM_PLB_M0ID_LAST)) ?
			 ppc4xx_plb_masters[master] : "UNKNOWN"));
}

static void
ppc4xx_edac_generate_message(const struct mem_ctl_info *mci,
			     const struct ppc4xx_ecc_status *status,
			     char *buffer,
			     size_t size)
{
	int n;

	if (buffer == NULL || size == 0)
		return;

	n = ppc4xx_edac_generate_ecc_message(mci, status, buffer, size);

	if (n < 0 || n >= size)
		return;

	buffer += n;
	size -= n;

	ppc4xx_edac_generate_plb_message(mci, status, buffer, size);
}

#ifdef DEBUG
static void
ppc4xx_ecc_dump_status(const struct mem_ctl_info *mci,
		       const struct ppc4xx_ecc_status *status)
{
	char message[PPC4XX_EDAC_MESSAGE_SIZE];

	ppc4xx_edac_generate_message(mci, status, message, sizeof(message));

	ppc4xx_edac_mc_printk(KERN_INFO, mci,
			      "\n"
			      "\tECCES: 0x%08x\n"
			      "\tWMIRQ: 0x%08x\n"
			      "\tBESR:  0x%08x\n"
			      "\tBEAR:  0x%08x%08x\n"
			      "\t%s\n",
			      status->ecces,
			      status->wmirq,
			      status->besr,
			      status->bearh,
			      status->bearl,
			      message);
}
#endif 

static void
ppc4xx_ecc_get_status(const struct mem_ctl_info *mci,
		      struct ppc4xx_ecc_status *status)
{
	const struct ppc4xx_edac_pdata *pdata = mci->pvt_info;
	const dcr_host_t *dcr_host = &pdata->dcr_host;

	status->ecces = mfsdram(dcr_host, SDRAM_ECCES) & SDRAM_ECCES_MASK;
	status->wmirq = mfsdram(dcr_host, SDRAM_WMIRQ) & SDRAM_WMIRQ_MASK;
	status->besr  = mfsdram(dcr_host, SDRAM_BESR)  & SDRAM_BESR_MASK;
	status->bearl = mfsdram(dcr_host, SDRAM_BEARL);
	status->bearh = mfsdram(dcr_host, SDRAM_BEARH);
}

static void
ppc4xx_ecc_clear_status(const struct mem_ctl_info *mci,
			const struct ppc4xx_ecc_status *status)
{
	const struct ppc4xx_edac_pdata *pdata = mci->pvt_info;
	const dcr_host_t *dcr_host = &pdata->dcr_host;

	mtsdram(dcr_host, SDRAM_ECCES,	status->ecces & SDRAM_ECCES_MASK);
	mtsdram(dcr_host, SDRAM_WMIRQ,	status->wmirq & SDRAM_WMIRQ_MASK);
	mtsdram(dcr_host, SDRAM_BESR,	status->besr & SDRAM_BESR_MASK);
	mtsdram(dcr_host, SDRAM_BEARL,	0);
	mtsdram(dcr_host, SDRAM_BEARH,	0);
}

static void
ppc4xx_edac_handle_ce(struct mem_ctl_info *mci,
		      const struct ppc4xx_ecc_status *status)
{
	int row;
	char message[PPC4XX_EDAC_MESSAGE_SIZE];

	ppc4xx_edac_generate_message(mci, status, message, sizeof(message));

	for (row = 0; row < mci->nr_csrows; row++)
		if (ppc4xx_edac_check_bank_error(status, row))
			edac_mc_handle_ce_no_info(mci, message);
}

static void
ppc4xx_edac_handle_ue(struct mem_ctl_info *mci,
		      const struct ppc4xx_ecc_status *status)
{
	const u64 bear = ((u64)status->bearh << 32 | status->bearl);
	const unsigned long page = bear >> PAGE_SHIFT;
	const unsigned long offset = bear & ~PAGE_MASK;
	int row;
	char message[PPC4XX_EDAC_MESSAGE_SIZE];

	ppc4xx_edac_generate_message(mci, status, message, sizeof(message));

	for (row = 0; row < mci->nr_csrows; row++)
		if (ppc4xx_edac_check_bank_error(status, row))
			edac_mc_handle_ue(mci, page, offset, row, message);
}

static void
ppc4xx_edac_check(struct mem_ctl_info *mci)
{
#ifdef DEBUG
	static unsigned int count;
#endif
	struct ppc4xx_ecc_status status;

	ppc4xx_ecc_get_status(mci, &status);

#ifdef DEBUG
	if (count++ % 30 == 0)
		ppc4xx_ecc_dump_status(mci, &status);
#endif

	if (status.ecces & SDRAM_ECCES_UE)
		ppc4xx_edac_handle_ue(mci, &status);

	if (status.ecces & SDRAM_ECCES_CE)
		ppc4xx_edac_handle_ce(mci, &status);

	ppc4xx_ecc_clear_status(mci, &status);
}

static irqreturn_t
ppc4xx_edac_isr(int irq, void *dev_id)
{
	struct mem_ctl_info *mci = dev_id;

	ppc4xx_edac_check(mci);

	return IRQ_HANDLED;
}

static enum dev_type __devinit
ppc4xx_edac_get_dtype(u32 mcopt1)
{
	switch (mcopt1 & SDRAM_MCOPT1_WDTH_MASK) {
	case SDRAM_MCOPT1_WDTH_16:
		return DEV_X2;
	case SDRAM_MCOPT1_WDTH_32:
		return DEV_X4;
	default:
		return DEV_UNKNOWN;
	}
}

static enum mem_type __devinit
ppc4xx_edac_get_mtype(u32 mcopt1)
{
	bool rden = ((mcopt1 & SDRAM_MCOPT1_RDEN_MASK) == SDRAM_MCOPT1_RDEN);

	switch (mcopt1 & SDRAM_MCOPT1_DDR_TYPE_MASK) {
	case SDRAM_MCOPT1_DDR2_TYPE:
		return rden ? MEM_RDDR2 : MEM_DDR2;
	case SDRAM_MCOPT1_DDR1_TYPE:
		return rden ? MEM_RDDR : MEM_DDR;
	default:
		return MEM_UNKNOWN;
	}
}

static int __devinit
ppc4xx_edac_init_csrows(struct mem_ctl_info *mci, u32 mcopt1)
{
	const struct ppc4xx_edac_pdata *pdata = mci->pvt_info;
	int status = 0;
	enum mem_type mtype;
	enum dev_type dtype;
	enum edac_type edac_mode;
	int row;
	u32 mbxcf, size;
	static u32 ppc4xx_last_page;

	

	mtype = ppc4xx_edac_get_mtype(mcopt1);
	dtype = ppc4xx_edac_get_dtype(mcopt1);

	

	if (mci->edac_cap & EDAC_FLAG_SECDED)
		edac_mode = EDAC_SECDED;
	else if (mci->edac_cap & EDAC_FLAG_EC)
		edac_mode = EDAC_EC;
	else
		edac_mode = EDAC_NONE;


	for (row = 0; row < mci->nr_csrows; row++) {
		struct csrow_info *csi = &mci->csrows[row];


		mbxcf = mfsdram(&pdata->dcr_host, SDRAM_MBXCF(row));

		if ((mbxcf & SDRAM_MBCF_BE_MASK) != SDRAM_MBCF_BE_ENABLE)
			continue;

		

		size = mbxcf & SDRAM_MBCF_SZ_MASK;

		switch (size) {
		case SDRAM_MBCF_SZ_4MB:
		case SDRAM_MBCF_SZ_8MB:
		case SDRAM_MBCF_SZ_16MB:
		case SDRAM_MBCF_SZ_32MB:
		case SDRAM_MBCF_SZ_64MB:
		case SDRAM_MBCF_SZ_128MB:
		case SDRAM_MBCF_SZ_256MB:
		case SDRAM_MBCF_SZ_512MB:
		case SDRAM_MBCF_SZ_1GB:
		case SDRAM_MBCF_SZ_2GB:
		case SDRAM_MBCF_SZ_4GB:
		case SDRAM_MBCF_SZ_8GB:
			csi->nr_pages = SDRAM_MBCF_SZ_TO_PAGES(size);
			break;
		default:
			ppc4xx_edac_mc_printk(KERN_ERR, mci,
					      "Unrecognized memory bank %d "
					      "size 0x%08x\n",
					      row, SDRAM_MBCF_SZ_DECODE(size));
			status = -EINVAL;
			goto done;
		}

		csi->first_page = ppc4xx_last_page;
		csi->last_page	= csi->first_page + csi->nr_pages - 1;
		csi->page_mask	= 0;


		csi->grain	= 1;

		csi->mtype	= mtype;
		csi->dtype	= dtype;

		csi->edac_mode	= edac_mode;

		ppc4xx_last_page += csi->nr_pages;
	}

 done:
	return status;
}

static int __devinit
ppc4xx_edac_mc_init(struct mem_ctl_info *mci,
		    struct platform_device *op,
		    const dcr_host_t *dcr_host,
		    u32 mcopt1)
{
	int status = 0;
	const u32 memcheck = (mcopt1 & SDRAM_MCOPT1_MCHK_MASK);
	struct ppc4xx_edac_pdata *pdata = NULL;
	const struct device_node *np = op->dev.of_node;

	if (of_match_device(ppc4xx_edac_match, &op->dev) == NULL)
		return -EINVAL;

	

	mci->dev		= &op->dev;

	dev_set_drvdata(mci->dev, mci);

	pdata			= mci->pvt_info;

	pdata->dcr_host		= *dcr_host;
	pdata->irqs.sec		= NO_IRQ;
	pdata->irqs.ded		= NO_IRQ;

	

	mci->mtype_cap		= (MEM_FLAG_DDR | MEM_FLAG_RDDR |
				   MEM_FLAG_DDR2 | MEM_FLAG_RDDR2);

	mci->edac_ctl_cap	= (EDAC_FLAG_NONE |
				   EDAC_FLAG_EC |
				   EDAC_FLAG_SECDED);

	mci->scrub_cap		= SCRUB_NONE;
	mci->scrub_mode		= SCRUB_NONE;


	switch (memcheck) {
	case SDRAM_MCOPT1_MCHK_CHK:
		mci->edac_cap	= EDAC_FLAG_EC;
		break;
	case SDRAM_MCOPT1_MCHK_CHK_REP:
		mci->edac_cap	= (EDAC_FLAG_EC | EDAC_FLAG_SECDED);
		mci->scrub_mode	= SCRUB_SW_SRC;
		break;
	default:
		mci->edac_cap	= EDAC_FLAG_NONE;
		break;
	}

	

	mci->mod_name		= PPC4XX_EDAC_MODULE_NAME;
	mci->mod_ver		= PPC4XX_EDAC_MODULE_REVISION;
	mci->ctl_name		= ppc4xx_edac_match->compatible,
	mci->dev_name		= np->full_name;

	

	mci->edac_check		= ppc4xx_edac_check;
	mci->ctl_page_to_phys	= NULL;

	

	status = ppc4xx_edac_init_csrows(mci, mcopt1);

	if (status)
		ppc4xx_edac_mc_printk(KERN_ERR, mci,
				      "Failed to initialize rows!\n");

	return status;
}

static int __devinit
ppc4xx_edac_register_irq(struct platform_device *op, struct mem_ctl_info *mci)
{
	int status = 0;
	int ded_irq, sec_irq;
	struct ppc4xx_edac_pdata *pdata = mci->pvt_info;
	struct device_node *np = op->dev.of_node;

	ded_irq = irq_of_parse_and_map(np, INTMAP_ECCDED_INDEX);
	sec_irq = irq_of_parse_and_map(np, INTMAP_ECCSEC_INDEX);

	if (ded_irq == NO_IRQ || sec_irq == NO_IRQ) {
		ppc4xx_edac_mc_printk(KERN_ERR, mci,
				      "Unable to map interrupts.\n");
		status = -ENODEV;
		goto fail;
	}

	status = request_irq(ded_irq,
			     ppc4xx_edac_isr,
			     IRQF_DISABLED,
			     "[EDAC] MC ECCDED",
			     mci);

	if (status < 0) {
		ppc4xx_edac_mc_printk(KERN_ERR, mci,
				      "Unable to request irq %d for ECC DED",
				      ded_irq);
		status = -ENODEV;
		goto fail1;
	}

	status = request_irq(sec_irq,
			     ppc4xx_edac_isr,
			     IRQF_DISABLED,
			     "[EDAC] MC ECCSEC",
			     mci);

	if (status < 0) {
		ppc4xx_edac_mc_printk(KERN_ERR, mci,
				      "Unable to request irq %d for ECC SEC",
				      sec_irq);
		status = -ENODEV;
		goto fail2;
	}

	ppc4xx_edac_mc_printk(KERN_INFO, mci, "ECCDED irq is %d\n", ded_irq);
	ppc4xx_edac_mc_printk(KERN_INFO, mci, "ECCSEC irq is %d\n", sec_irq);

	pdata->irqs.ded = ded_irq;
	pdata->irqs.sec = sec_irq;

	return 0;

 fail2:
	free_irq(sec_irq, mci);

 fail1:
	free_irq(ded_irq, mci);

 fail:
	return status;
}

static int __devinit
ppc4xx_edac_map_dcrs(const struct device_node *np, dcr_host_t *dcr_host)
{
	unsigned int dcr_base, dcr_len;

	if (np == NULL || dcr_host == NULL)
		return -EINVAL;

	

	dcr_base = dcr_resource_start(np, 0);
	dcr_len = dcr_resource_len(np, 0);

	if (dcr_base == 0 || dcr_len == 0) {
		ppc4xx_edac_printk(KERN_ERR,
				   "Failed to obtain DCR property.\n");
		return -ENODEV;
	}

	if (dcr_len != SDRAM_DCR_RESOURCE_LEN) {
		ppc4xx_edac_printk(KERN_ERR,
				   "Unexpected DCR length %d, expected %d.\n",
				   dcr_len, SDRAM_DCR_RESOURCE_LEN);
		return -ENODEV;
	}

	

	*dcr_host = dcr_map(np, dcr_base, dcr_len);

	if (!DCR_MAP_OK(*dcr_host)) {
		ppc4xx_edac_printk(KERN_INFO, "Failed to map DCRs.\n");
		    return -ENODEV;
	}

	return 0;
}

static int __devinit ppc4xx_edac_probe(struct platform_device *op)
{
	int status = 0;
	u32 mcopt1, memcheck;
	dcr_host_t dcr_host;
	const struct device_node *np = op->dev.of_node;
	struct mem_ctl_info *mci = NULL;
	static int ppc4xx_edac_instance;


	if (!of_device_is_compatible(np, "ibm,sdram-405ex") &&
	    !of_device_is_compatible(np, "ibm,sdram-405exr")) {
		ppc4xx_edac_printk(KERN_NOTICE,
				   "Only the PPC405EX[r] is supported.\n");
		return -ENODEV;
	}


	status = ppc4xx_edac_map_dcrs(np, &dcr_host);

	if (status)
		return status;


	mcopt1 = mfsdram(&dcr_host, SDRAM_MCOPT1);
	memcheck = (mcopt1 & SDRAM_MCOPT1_MCHK_MASK);

	if (memcheck == SDRAM_MCOPT1_MCHK_NON) {
		ppc4xx_edac_printk(KERN_INFO, "%s: No ECC memory detected or "
				   "ECC is disabled.\n", np->full_name);
		status = -ENODEV;
		goto done;
	}


	mci = edac_mc_alloc(sizeof(struct ppc4xx_edac_pdata),
			    ppc4xx_edac_nr_csrows,
			    ppc4xx_edac_nr_chans,
			    ppc4xx_edac_instance);

	if (mci == NULL) {
		ppc4xx_edac_printk(KERN_ERR, "%s: "
				   "Failed to allocate EDAC MC instance!\n",
				   np->full_name);
		status = -ENOMEM;
		goto done;
	}

	status = ppc4xx_edac_mc_init(mci, op, &dcr_host, mcopt1);

	if (status) {
		ppc4xx_edac_mc_printk(KERN_ERR, mci,
				      "Failed to initialize instance!\n");
		goto fail;
	}


	if (edac_mc_add_mc(mci)) {
		ppc4xx_edac_mc_printk(KERN_ERR, mci,
				      "Failed to add instance!\n");
		status = -ENODEV;
		goto fail;
	}

	if (edac_op_state == EDAC_OPSTATE_INT) {
		status = ppc4xx_edac_register_irq(op, mci);

		if (status)
			goto fail1;
	}

	ppc4xx_edac_instance++;

	return 0;

 fail1:
	edac_mc_del_mc(mci->dev);

 fail:
	edac_mc_free(mci);

 done:
	return status;
}

static int
ppc4xx_edac_remove(struct platform_device *op)
{
	struct mem_ctl_info *mci = dev_get_drvdata(&op->dev);
	struct ppc4xx_edac_pdata *pdata = mci->pvt_info;

	if (edac_op_state == EDAC_OPSTATE_INT) {
		free_irq(pdata->irqs.sec, mci);
		free_irq(pdata->irqs.ded, mci);
	}

	dcr_unmap(pdata->dcr_host, SDRAM_DCR_RESOURCE_LEN);

	edac_mc_del_mc(mci->dev);
	edac_mc_free(mci);

	return 0;
}

static inline void __init
ppc4xx_edac_opstate_init(void)
{
	switch (edac_op_state) {
	case EDAC_OPSTATE_POLL:
	case EDAC_OPSTATE_INT:
		break;
	default:
		edac_op_state = EDAC_OPSTATE_INT;
		break;
	}

	ppc4xx_edac_printk(KERN_INFO, "Reporting type: %s\n",
			   ((edac_op_state == EDAC_OPSTATE_POLL) ?
			    EDAC_OPSTATE_POLL_STR :
			    ((edac_op_state == EDAC_OPSTATE_INT) ?
			     EDAC_OPSTATE_INT_STR :
			     EDAC_OPSTATE_UNKNOWN_STR)));
}

static int __init
ppc4xx_edac_init(void)
{
	ppc4xx_edac_printk(KERN_INFO, PPC4XX_EDAC_MODULE_REVISION "\n");

	ppc4xx_edac_opstate_init();

	return platform_driver_register(&ppc4xx_edac_driver);
}

static void __exit
ppc4xx_edac_exit(void)
{
	platform_driver_unregister(&ppc4xx_edac_driver);
}

module_init(ppc4xx_edac_init);
module_exit(ppc4xx_edac_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Grant Erickson <gerickson@nuovations.com>");
MODULE_DESCRIPTION("EDAC MC Driver for the PPC4xx IBM DDR2 Memory Controller");
module_param(edac_op_state, int, 0444);
MODULE_PARM_DESC(edac_op_state, "EDAC Error Reporting State: "
		 "0=" EDAC_OPSTATE_POLL_STR ", 2=" EDAC_OPSTATE_INT_STR);
