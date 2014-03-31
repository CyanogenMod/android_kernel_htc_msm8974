/*
 * Device driver for the SYMBIOS/LSILOGIC 53C8XX and 53C1010 family 
 * of PCI-SCSI IO processors.
 *
 * Copyright (C) 1999-2001  Gerard Roudier <groudier@free.fr>
 * Copyright (c) 2003-2005  Matthew Wilcox <matthew@wil.cx>
 *
 * This driver is derived from the Linux sym53c8xx driver.
 * Copyright (C) 1998-2000  Gerard Roudier
 *
 * The sym53c8xx driver is derived from the ncr53c8xx driver that had been 
 * a port of the FreeBSD ncr driver to Linux-1.2.13.
 *
 * The original ncr driver has been written for 386bsd and FreeBSD by
 *         Wolfgang Stanglmeier        <wolf@cologne.de>
 *         Stefan Esser                <se@mi.Uni-Koeln.de>
 * Copyright (C) 1994  Wolfgang Stanglmeier
 *
 * Other major contributions:
 *
 * NVRAM detection and reading.
 * Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
 *
 *-----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/slab.h>
#include <asm/param.h>		

#include "sym_glue.h"
#include "sym_nvram.h"

#if 0
#define SYM_DEBUG_GENERIC_SUPPORT
#endif

static void sym_int_ma (struct sym_hcb *np);
static void sym_int_sir(struct sym_hcb *);
static struct sym_ccb *sym_alloc_ccb(struct sym_hcb *np);
static struct sym_ccb *sym_ccb_from_dsa(struct sym_hcb *np, u32 dsa);
static void sym_alloc_lcb_tags (struct sym_hcb *np, u_char tn, u_char ln);
static void sym_complete_error (struct sym_hcb *np, struct sym_ccb *cp);
static void sym_complete_ok (struct sym_hcb *np, struct sym_ccb *cp);
static int sym_compute_residual(struct sym_hcb *np, struct sym_ccb *cp);

static void sym_printl_hex(u_char *p, int n)
{
	while (n-- > 0)
		printf (" %x", *p++);
	printf (".\n");
}

static void sym_print_msg(struct sym_ccb *cp, char *label, u_char *msg)
{
	sym_print_addr(cp->cmd, "%s: ", label);

	spi_print_msg(msg);
	printf("\n");
}

static void sym_print_nego_msg(struct sym_hcb *np, int target, char *label, u_char *msg)
{
	struct sym_tcb *tp = &np->target[target];
	dev_info(&tp->starget->dev, "%s: ", label);

	spi_print_msg(msg);
	printf("\n");
}

void sym_print_xerr(struct scsi_cmnd *cmd, int x_status)
{
	if (x_status & XE_PARITY_ERR) {
		sym_print_addr(cmd, "unrecovered SCSI parity error.\n");
	}
	if (x_status & XE_EXTRA_DATA) {
		sym_print_addr(cmd, "extraneous data discarded.\n");
	}
	if (x_status & XE_BAD_PHASE) {
		sym_print_addr(cmd, "illegal scsi phase (4/5).\n");
	}
	if (x_status & XE_SODL_UNRUN) {
		sym_print_addr(cmd, "ODD transfer in DATA OUT phase.\n");
	}
	if (x_status & XE_SWIDE_OVRUN) {
		sym_print_addr(cmd, "ODD transfer in DATA IN phase.\n");
	}
}

static char *sym_scsi_bus_mode(int mode)
{
	switch(mode) {
	case SMODE_HVD:	return "HVD";
	case SMODE_SE:	return "SE";
	case SMODE_LVD: return "LVD";
	}
	return "??";
}

static void sym_chip_reset (struct sym_hcb *np)
{
	OUTB(np, nc_istat, SRST);
	INB(np, nc_mbox1);
	udelay(10);
	OUTB(np, nc_istat, 0);
	INB(np, nc_mbox1);
	udelay(2000);	
}

static void sym_soft_reset (struct sym_hcb *np)
{
	u_char istat = 0;
	int i;

	if (!(np->features & FE_ISTAT1) || !(INB(np, nc_istat1) & SCRUN))
		goto do_chip_reset;

	OUTB(np, nc_istat, CABRT);
	for (i = 100000 ; i ; --i) {
		istat = INB(np, nc_istat);
		if (istat & SIP) {
			INW(np, nc_sist);
		}
		else if (istat & DIP) {
			if (INB(np, nc_dstat) & ABRT)
				break;
		}
		udelay(5);
	}
	OUTB(np, nc_istat, 0);
	if (!i)
		printf("%s: unable to abort current chip operation, "
		       "ISTAT=0x%02x.\n", sym_name(np), istat);
do_chip_reset:
	sym_chip_reset(np);
}

static void sym_start_reset(struct sym_hcb *np)
{
	sym_reset_scsi_bus(np, 1);
}
 
int sym_reset_scsi_bus(struct sym_hcb *np, int enab_int)
{
	u32 term;
	int retv = 0;

	sym_soft_reset(np);	
	if (enab_int)
		OUTW(np, nc_sien, RST);
	OUTB(np, nc_stest3, TE);
	OUTB(np, nc_dcntl, (np->rv_dcntl & IRQM));
	OUTB(np, nc_scntl1, CRST);
	INB(np, nc_mbox1);
	udelay(200);

	if (!SYM_SETUP_SCSI_BUS_CHECK)
		goto out;
	term =	INB(np, nc_sstat0);
	term =	((term & 2) << 7) + ((term & 1) << 17);	
	term |= ((INB(np, nc_sstat2) & 0x01) << 26) |	
		((INW(np, nc_sbdl) & 0xff)   << 9)  |	
		((INW(np, nc_sbdl) & 0xff00) << 10) |	
		INB(np, nc_sbcl);	

	if (!np->maxwide)
		term &= 0x3ffff;

	if (term != (2<<7)) {
		printf("%s: suspicious SCSI data while resetting the BUS.\n",
			sym_name(np));
		printf("%s: %sdp0,d7-0,rst,req,ack,bsy,sel,atn,msg,c/d,i/o = "
			"0x%lx, expecting 0x%lx\n",
			sym_name(np),
			(np->features & FE_WIDE) ? "dp1,d15-8," : "",
			(u_long)term, (u_long)(2<<7));
		if (SYM_SETUP_SCSI_BUS_CHECK == 1)
			retv = 1;
	}
out:
	OUTB(np, nc_scntl1, 0);
	return retv;
}

static void sym_selectclock(struct sym_hcb *np, u_char scntl3)
{
	if (np->multiplier <= 1) {
		OUTB(np, nc_scntl3, scntl3);
		return;
	}

	if (sym_verbose >= 2)
		printf ("%s: enabling clock multiplier\n", sym_name(np));

	OUTB(np, nc_stest1, DBLEN);	   
	if (np->features & FE_LCKFRQ) {
		int i = 20;
		while (!(INB(np, nc_stest4) & LCKFRQ) && --i > 0)
			udelay(20);
		if (!i)
			printf("%s: the chip cannot lock the frequency\n",
				sym_name(np));
	} else {
		INB(np, nc_mbox1);
		udelay(50+10);
	}
	OUTB(np, nc_stest3, HSC);		
	OUTB(np, nc_scntl3, scntl3);
	OUTB(np, nc_stest1, (DBLEN|DBLSEL));
	OUTB(np, nc_stest3, 0x00);		
}



static unsigned getfreq (struct sym_hcb *np, int gen)
{
	unsigned int ms = 0;
	unsigned int f;

	OUTW(np, nc_sien, 0);	
	INW(np, nc_sist);	
	OUTB(np, nc_dien, 0);	
	INW(np, nc_sist);	
	if (np->features & FE_C10) {
		OUTW(np, nc_sien, GEN);
		OUTB(np, nc_istat1, SIRQD);
	}
	OUTB(np, nc_scntl3, 4);	   
	OUTB(np, nc_stime1, 0);	   
	OUTB(np, nc_stime1, gen);  
	while (!(INW(np, nc_sist) & GEN) && ms++ < 100000)
		udelay(1000/4);    
	OUTB(np, nc_stime1, 0);    
	if (np->features & FE_C10) {
		OUTW(np, nc_sien, 0);
		OUTB(np, nc_istat1, 0);
	}
 	OUTB(np, nc_scntl3, 0);

	f = ms ? ((1 << gen) * (4340*4)) / ms : 0;

	if (np->features & FE_C10)
		f = (f * 2) / 3;

	if (sym_verbose >= 2)
		printf ("%s: Delay (GEN=%d): %u msec, %u KHz\n",
			sym_name(np), gen, ms/4, f);

	return f;
}

static unsigned sym_getfreq (struct sym_hcb *np)
{
	u_int f1, f2;
	int gen = 8;

	getfreq (np, gen);	
	f1 = getfreq (np, gen);
	f2 = getfreq (np, gen);
	if (f1 > f2) f1 = f2;		
	return f1;
}

static void sym_getclock (struct sym_hcb *np, int mult)
{
	unsigned char scntl3 = np->sv_scntl3;
	unsigned char stest1 = np->sv_stest1;
	unsigned f1;

	np->multiplier = 1;
	f1 = 40000;
	if (mult > 1 && (stest1 & (DBLEN+DBLSEL)) == DBLEN+DBLSEL) {
		if (sym_verbose >= 2)
			printf ("%s: clock multiplier found\n", sym_name(np));
		np->multiplier = mult;
	}

	if (np->multiplier != mult || (scntl3 & 7) < 3 || !(scntl3 & 1)) {
		OUTB(np, nc_stest1, 0);		
		f1 = sym_getfreq (np);

		if (sym_verbose)
			printf ("%s: chip clock is %uKHz\n", sym_name(np), f1);

		if	(f1 <	45000)		f1 =  40000;
		else if (f1 <	55000)		f1 =  50000;
		else				f1 =  80000;

		if (f1 < 80000 && mult > 1) {
			if (sym_verbose >= 2)
				printf ("%s: clock multiplier assumed\n",
					sym_name(np));
			np->multiplier	= mult;
		}
	} else {
		if	((scntl3 & 7) == 3)	f1 =  40000;
		else if	((scntl3 & 7) == 5)	f1 =  80000;
		else 				f1 = 160000;

		f1 /= np->multiplier;
	}

	f1		*= np->multiplier;
	np->clock_khz	= f1;
}

static int sym_getpciclock (struct sym_hcb *np)
{
	int f = 0;

#if 1
	if (np->features & FE_66MHZ) {
#else
	if (1) {
#endif
		OUTB(np, nc_stest1, SCLK); 
		f = sym_getfreq(np);
		OUTB(np, nc_stest1, 0);
	}
	np->pciclk_khz = f;

	return f;
}

#define _5M 5000000
static const u32 div_10M[] = {2*_5M, 3*_5M, 4*_5M, 6*_5M, 8*_5M, 12*_5M, 16*_5M};

static int 
sym_getsync(struct sym_hcb *np, u_char dt, u_char sfac, u_char *divp, u_char *fakp)
{
	u32	clk = np->clock_khz;	
	int	div = np->clock_divn;	
	u32	fak;			
	u32	per;			
	u32	kpc;			
	int	ret;

	if (dt && sfac <= 9)	per = 125;
	else if	(sfac <= 10)	per = 250;
	else if	(sfac == 11)	per = 303;
	else if	(sfac == 12)	per = 500;
	else			per = 40 * sfac;
	ret = per;

	kpc = per * clk;
	if (dt)
		kpc <<= 1;

#if 1
	if ((np->features & (FE_C10|FE_U3EN)) == FE_C10) {
		while (div > 0) {
			--div;
			if (kpc > (div_10M[div] << 2)) {
				++div;
				break;
			}
		}
		fak = 0;			
		if (div == np->clock_divn) {	
			ret = -1;
		}
		*divp = div;
		*fakp = fak;
		return ret;
	}
#endif

	while (div-- > 0)
		if (kpc >= (div_10M[div] << 2)) break;

	if (dt) {
		fak = (kpc - 1) / (div_10M[div] << 1) + 1 - 2;
		
	} else {
		fak = (kpc - 1) / div_10M[div] + 1 - 4;
		
	}

	if (fak > 2) {
		fak = 2;
		ret = -1;
	}

	*divp = div;
	*fakp = fak;

	return ret;
}


#define burst_length(bc) (!(bc))? 0 : 1 << (bc)

#define burst_code(dmode, ctest4, ctest5) \
	(ctest4) & 0x80? 0 : (((dmode) & 0xc0) >> 6) + ((ctest5) & 0x04) + 1

static inline void sym_init_burst(struct sym_hcb *np, u_char bc)
{
	np->rv_ctest4	&= ~0x80;
	np->rv_dmode	&= ~(0x3 << 6);
	np->rv_ctest5	&= ~0x4;

	if (!bc) {
		np->rv_ctest4	|= 0x80;
	}
	else {
		--bc;
		np->rv_dmode	|= ((bc & 0x3) << 6);
		np->rv_ctest5	|= (bc & 0x4);
	}
}

static void sym_save_initial_setting (struct sym_hcb *np)
{
	np->sv_scntl0	= INB(np, nc_scntl0) & 0x0a;
	np->sv_scntl3	= INB(np, nc_scntl3) & 0x07;
	np->sv_dmode	= INB(np, nc_dmode)  & 0xce;
	np->sv_dcntl	= INB(np, nc_dcntl)  & 0xa8;
	np->sv_ctest3	= INB(np, nc_ctest3) & 0x01;
	np->sv_ctest4	= INB(np, nc_ctest4) & 0x80;
	np->sv_gpcntl	= INB(np, nc_gpcntl);
	np->sv_stest1	= INB(np, nc_stest1);
	np->sv_stest2	= INB(np, nc_stest2) & 0x20;
	np->sv_stest4	= INB(np, nc_stest4);
	if (np->features & FE_C10) {	
		np->sv_scntl4	= INB(np, nc_scntl4);
		np->sv_ctest5	= INB(np, nc_ctest5) & 0x04;
	}
	else
		np->sv_ctest5	= INB(np, nc_ctest5) & 0x24;
}

static void sym_set_bus_mode(struct sym_hcb *np, struct sym_nvram *nvram)
{
	if (np->scsi_mode)
		return;

	np->scsi_mode = SMODE_SE;
	if (np->features & (FE_ULTRA2|FE_ULTRA3))
		np->scsi_mode = (np->sv_stest4 & SMODE);
	else if	(np->features & FE_DIFF) {
		if (SYM_SETUP_SCSI_DIFF == 1) {
			if (np->sv_scntl3) {
				if (np->sv_stest2 & 0x20)
					np->scsi_mode = SMODE_HVD;
			} else if (nvram->type == SYM_SYMBIOS_NVRAM) {
				if (!(INB(np, nc_gpreg) & 0x08))
					np->scsi_mode = SMODE_HVD;
			}
		} else if (SYM_SETUP_SCSI_DIFF == 2)
			np->scsi_mode = SMODE_HVD;
	}
	if (np->scsi_mode == SMODE_HVD)
		np->rv_stest2 |= 0x20;
}

static int sym_prepare_setting(struct Scsi_Host *shost, struct sym_hcb *np, struct sym_nvram *nvram)
{
	struct sym_data *sym_data = shost_priv(shost);
	struct pci_dev *pdev = sym_data->pdev;
	u_char	burst_max;
	u32	period;
	int i;

	np->maxwide = (np->features & FE_WIDE) ? 1 : 0;

	if	(np->features & (FE_ULTRA3 | FE_ULTRA2))
		np->clock_khz = 160000;
	else if	(np->features & FE_ULTRA)
		np->clock_khz = 80000;
	else
		np->clock_khz = 40000;

	if	(np->features & FE_QUAD)
		np->multiplier	= 4;
	else if	(np->features & FE_DBLR)
		np->multiplier	= 2;
	else
		np->multiplier	= 1;

	if (np->features & FE_VARCLK)
		sym_getclock(np, np->multiplier);

	i = np->clock_divn - 1;
	while (--i >= 0) {
		if (10ul * SYM_CONF_MIN_ASYNC * np->clock_khz > div_10M[i]) {
			++i;
			break;
		}
	}
	np->rv_scntl3 = i+1;

	if (np->features & FE_C10)
		np->rv_scntl3 = 0;

	period = (4 * div_10M[0] + np->clock_khz - 1) / np->clock_khz;

	if	(period <= 250)		np->minsync = 10;
	else if	(period <= 303)		np->minsync = 11;
	else if	(period <= 500)		np->minsync = 12;
	else				np->minsync = (period + 40 - 1) / 40;

	if	(np->minsync < 25 &&
		 !(np->features & (FE_ULTRA|FE_ULTRA2|FE_ULTRA3)))
		np->minsync = 25;
	else if	(np->minsync < 12 &&
		 !(np->features & (FE_ULTRA2|FE_ULTRA3)))
		np->minsync = 12;

	period = (11 * div_10M[np->clock_divn - 1]) / (4 * np->clock_khz);
	np->maxsync = period > 2540 ? 254 : period / 10;

	if ((np->features & (FE_C10|FE_ULTRA3)) == (FE_C10|FE_ULTRA3)) {
		if (np->clock_khz == 160000) {
			np->minsync_dt = 9;
			np->maxsync_dt = 50;
			np->maxoffs_dt = nvram->type ? 62 : 31;
		}
	}
	
	if (np->features & FE_DAC) {
		if (!use_dac(np))
			np->rv_ccntl1 |= (DDAC);
		else if (SYM_CONF_DMA_ADDRESSING_MODE == 1)
			np->rv_ccntl1 |= (XTIMOD | EXTIBMV);
		else if (SYM_CONF_DMA_ADDRESSING_MODE == 2)
			np->rv_ccntl1 |= (0 | EXTIBMV);
	}

	if (np->features & FE_NOPM)
		np->rv_ccntl0	|= (ENPMJ);

	if (pdev->device == PCI_DEVICE_ID_LSI_53C1010_33 &&
	    pdev->revision < 0x1)
		np->rv_ccntl0	|=  DILS;

	burst_max	= SYM_SETUP_BURST_ORDER;
	if (burst_max == 255)
		burst_max = burst_code(np->sv_dmode, np->sv_ctest4,
				       np->sv_ctest5);
	if (burst_max > 7)
		burst_max = 7;
	if (burst_max > np->maxburst)
		burst_max = np->maxburst;

	if ((pdev->device == PCI_DEVICE_ID_NCR_53C810 &&
	     pdev->revision >= 0x10 && pdev->revision <= 0x11) ||
	    (pdev->device == PCI_DEVICE_ID_NCR_53C860 &&
	     pdev->revision <= 0x1))
		np->features &= ~(FE_WRIE|FE_ERL|FE_ERMP);

	if (np->features & FE_ERL)
		np->rv_dmode	|= ERL;		
	if (np->features & FE_BOF)
		np->rv_dmode	|= BOF;		
	if (np->features & FE_ERMP)
		np->rv_dmode	|= ERMP;	
#if 1
	if ((np->features & FE_PFEN) && !np->ram_ba)
#else
	if (np->features & FE_PFEN)
#endif
		np->rv_dcntl	|= PFEN;	
	if (np->features & FE_CLSE)
		np->rv_dcntl	|= CLSE;	
	if (np->features & FE_WRIE)
		np->rv_ctest3	|= WRIE;	
	if (np->features & FE_DFS)
		np->rv_ctest5	|= DFS;		

	np->rv_ctest4	|= MPEE; 
	np->rv_scntl0	|= 0x0a; 

	np->myaddr = 255;
	np->scsi_mode = 0;
	sym_nvram_setup_host(shost, np, nvram);

	if (np->myaddr == 255) {
		np->myaddr = INB(np, nc_scid) & 0x07;
		if (!np->myaddr)
			np->myaddr = SYM_SETUP_HOST_ID;
	}

	sym_init_burst(np, burst_max);

	sym_set_bus_mode(np, nvram);

	if ((SYM_SETUP_SCSI_LED || 
	     (nvram->type == SYM_SYMBIOS_NVRAM ||
	      (nvram->type == SYM_TEKRAM_NVRAM &&
	       pdev->device == PCI_DEVICE_ID_NCR_53C895))) &&
	    !(np->features & FE_LEDC) && !(np->sv_gpcntl & 0x01))
		np->features |= FE_LED0;

	switch(SYM_SETUP_IRQ_MODE & 3) {
	case 2:
		np->rv_dcntl	|= IRQM;
		break;
	case 1:
		np->rv_dcntl	|= (np->sv_dcntl & IRQM);
		break;
	default:
		break;
	}

	for (i = 0 ; i < SYM_CONF_MAX_TARGET ; i++) {
		struct sym_tcb *tp = &np->target[i];

		tp->usrflags |= (SYM_DISC_ENABLED | SYM_TAGS_ENABLED);
		tp->usrtags = SYM_SETUP_MAX_TAG;
		tp->usr_width = np->maxwide;
		tp->usr_period = 9;

		sym_nvram_setup_target(tp, i, nvram);

		if (!tp->usrtags)
			tp->usrflags &= ~SYM_TAGS_ENABLED;
	}

	printf("%s: %s, ID %d, Fast-%d, %s, %s\n", sym_name(np),
		sym_nvram_type(nvram), np->myaddr,
		(np->features & FE_ULTRA3) ? 80 : 
		(np->features & FE_ULTRA2) ? 40 : 
		(np->features & FE_ULTRA)  ? 20 : 10,
		sym_scsi_bus_mode(np->scsi_mode),
		(np->rv_scntl0 & 0xa)	? "parity checking" : "NO parity");
	if (sym_verbose) {
		printf("%s: %s IRQ line driver%s\n",
			sym_name(np),
			np->rv_dcntl & IRQM ? "totem pole" : "open drain",
			np->ram_ba ? ", using on-chip SRAM" : "");
		printf("%s: using %s firmware.\n", sym_name(np), np->fw_name);
		if (np->features & FE_NOPM)
			printf("%s: handling phase mismatch from SCRIPTS.\n", 
			       sym_name(np));
	}
	if (sym_verbose >= 2) {
		printf ("%s: initial SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			sym_name(np), np->sv_scntl3, np->sv_dmode, np->sv_dcntl,
			np->sv_ctest3, np->sv_ctest4, np->sv_ctest5);

		printf ("%s: final   SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			sym_name(np), np->rv_scntl3, np->rv_dmode, np->rv_dcntl,
			np->rv_ctest3, np->rv_ctest4, np->rv_ctest5);
	}

	return 0;
}

#ifdef CONFIG_SCSI_SYM53C8XX_MMIO
static int sym_regtest(struct sym_hcb *np)
{
	register volatile u32 data;
	data = 0xffffffff;
	OUTL(np, nc_dstat, data);
	data = INL(np, nc_dstat);
#if 1
	if (data == 0xffffffff) {
#else
	if ((data & 0xe2f0fffd) != 0x02000080) {
#endif
		printf ("CACHE TEST FAILED: reg dstat-sstat2 readback %x.\n",
			(unsigned) data);
		return 0x10;
	}
	return 0;
}
#else
static inline int sym_regtest(struct sym_hcb *np)
{
	return 0;
}
#endif

static int sym_snooptest(struct sym_hcb *np)
{
	u32 sym_rd, sym_wr, sym_bk, host_rd, host_wr, pc, dstat;
	int i, err;

	err = sym_regtest(np);
	if (err)
		return err;
restart_test:
	OUTB(np, nc_ctest4, (np->rv_ctest4 & MPEE));
	pc  = SCRIPTZ_BA(np, snooptest);
	host_wr = 1;
	sym_wr  = 2;
	np->scratch = cpu_to_scr(host_wr);
	OUTL(np, nc_temp, sym_wr);
	OUTL(np, nc_dsa, np->hcb_ba);
	OUTL_DSP(np, pc);
	for (i=0; i<SYM_SNOOP_TIMEOUT; i++)
		if (INB(np, nc_istat) & (INTF|SIP|DIP))
			break;
	if (i>=SYM_SNOOP_TIMEOUT) {
		printf ("CACHE TEST FAILED: timeout.\n");
		return (0x20);
	}
	dstat = INB(np, nc_dstat);
#if 1	
	if ((dstat & MDPE) && (np->rv_ctest4 & MPEE)) {
		printf ("%s: PCI DATA PARITY ERROR DETECTED - "
			"DISABLING MASTER DATA PARITY CHECKING.\n",
			sym_name(np));
		np->rv_ctest4 &= ~MPEE;
		goto restart_test;
	}
#endif
	if (dstat & (MDPE|BF|IID)) {
		printf ("CACHE TEST FAILED: DMA error (dstat=0x%02x).", dstat);
		return (0x80);
	}
	pc = INL(np, nc_dsp);
	host_rd = scr_to_cpu(np->scratch);
	sym_rd  = INL(np, nc_scratcha);
	sym_bk  = INL(np, nc_temp);
	if (pc != SCRIPTZ_BA(np, snoopend)+8) {
		printf ("CACHE TEST FAILED: script execution failed.\n");
		printf ("start=%08lx, pc=%08lx, end=%08lx\n", 
			(u_long) SCRIPTZ_BA(np, snooptest), (u_long) pc,
			(u_long) SCRIPTZ_BA(np, snoopend) +8);
		return (0x40);
	}
	if (host_wr != sym_rd) {
		printf ("CACHE TEST FAILED: host wrote %d, chip read %d.\n",
			(int) host_wr, (int) sym_rd);
		err |= 1;
	}
	if (host_rd != sym_wr) {
		printf ("CACHE TEST FAILED: chip wrote %d, host read %d.\n",
			(int) sym_wr, (int) host_rd);
		err |= 2;
	}
	if (sym_bk != sym_wr) {
		printf ("CACHE TEST FAILED: chip wrote %d, read back %d.\n",
			(int) sym_wr, (int) sym_bk);
		err |= 4;
	}

	return err;
}

static void sym_log_hard_error(struct Scsi_Host *shost, u_short sist, u_char dstat)
{
	struct sym_hcb *np = sym_get_hcb(shost);
	u32	dsp;
	int	script_ofs;
	int	script_size;
	char	*script_name;
	u_char	*script_base;
	int	i;

	dsp	= INL(np, nc_dsp);

	if	(dsp > np->scripta_ba &&
		 dsp <= np->scripta_ba + np->scripta_sz) {
		script_ofs	= dsp - np->scripta_ba;
		script_size	= np->scripta_sz;
		script_base	= (u_char *) np->scripta0;
		script_name	= "scripta";
	}
	else if (np->scriptb_ba < dsp && 
		 dsp <= np->scriptb_ba + np->scriptb_sz) {
		script_ofs	= dsp - np->scriptb_ba;
		script_size	= np->scriptb_sz;
		script_base	= (u_char *) np->scriptb0;
		script_name	= "scriptb";
	} else {
		script_ofs	= dsp;
		script_size	= 0;
		script_base	= NULL;
		script_name	= "mem";
	}

	printf ("%s:%d: ERROR (%x:%x) (%x-%x-%x) (%x/%x/%x) @ (%s %x:%08x).\n",
		sym_name(np), (unsigned)INB(np, nc_sdid)&0x0f, dstat, sist,
		(unsigned)INB(np, nc_socl), (unsigned)INB(np, nc_sbcl),
		(unsigned)INB(np, nc_sbdl), (unsigned)INB(np, nc_sxfer),
		(unsigned)INB(np, nc_scntl3),
		(np->features & FE_C10) ?  (unsigned)INB(np, nc_scntl4) : 0,
		script_name, script_ofs,   (unsigned)INL(np, nc_dbc));

	if (((script_ofs & 3) == 0) &&
	    (unsigned)script_ofs < script_size) {
		printf ("%s: script cmd = %08x\n", sym_name(np),
			scr_to_cpu((int) *(u32 *)(script_base + script_ofs)));
	}

	printf("%s: regdump:", sym_name(np));
	for (i = 0; i < 24; i++)
		printf(" %02x", (unsigned)INB_OFF(np, i));
	printf(".\n");

	if (dstat & (MDPE|BF))
		sym_log_bus_error(shost);
}

void sym_dump_registers(struct Scsi_Host *shost)
{
	struct sym_hcb *np = sym_get_hcb(shost);
	u_short sist;
	u_char dstat;

	sist = INW(np, nc_sist);
	dstat = INB(np, nc_dstat);
	sym_log_hard_error(shost, sist, dstat);
}

static struct sym_chip sym_dev_table[] = {
 {PCI_DEVICE_ID_NCR_53C810, 0x0f, "810", 4, 8, 4, 64,
 FE_ERL}
 ,
#ifdef SYM_DEBUG_GENERIC_SUPPORT
 {PCI_DEVICE_ID_NCR_53C810, 0xff, "810a", 4,  8, 4, 1,
 FE_BOF}
 ,
#else
 {PCI_DEVICE_ID_NCR_53C810, 0xff, "810a", 4,  8, 4, 1,
 FE_CACHE_SET|FE_LDSTR|FE_PFEN|FE_BOF}
 ,
#endif
 {PCI_DEVICE_ID_NCR_53C815, 0xff, "815", 4,  8, 4, 64,
 FE_BOF|FE_ERL}
 ,
 {PCI_DEVICE_ID_NCR_53C825, 0x0f, "825", 6,  8, 4, 64,
 FE_WIDE|FE_BOF|FE_ERL|FE_DIFF}
 ,
 {PCI_DEVICE_ID_NCR_53C825, 0xff, "825a", 6,  8, 4, 2,
 FE_WIDE|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM|FE_DIFF}
 ,
 {PCI_DEVICE_ID_NCR_53C860, 0xff, "860", 4,  8, 5, 1,
 FE_ULTRA|FE_CACHE_SET|FE_BOF|FE_LDSTR|FE_PFEN}
 ,
 {PCI_DEVICE_ID_NCR_53C875, 0x01, "875", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF|FE_VARCLK}
 ,
 {PCI_DEVICE_ID_NCR_53C875, 0xff, "875", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF|FE_VARCLK}
 ,
 {PCI_DEVICE_ID_NCR_53C875J, 0xff, "875J", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF|FE_VARCLK}
 ,
 {PCI_DEVICE_ID_NCR_53C885, 0xff, "885", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF|FE_VARCLK}
 ,
#ifdef SYM_DEBUG_GENERIC_SUPPORT
 {PCI_DEVICE_ID_NCR_53C895, 0xff, "895", 6, 31, 7, 2,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|
 FE_RAM|FE_LCKFRQ}
 ,
#else
 {PCI_DEVICE_ID_NCR_53C895, 0xff, "895", 6, 31, 7, 2,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_LCKFRQ}
 ,
#endif
 {PCI_DEVICE_ID_NCR_53C896, 0xff, "896", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_LCKFRQ}
 ,
 {PCI_DEVICE_ID_LSI_53C895A, 0xff, "895a", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_LCKFRQ}
 ,
 {PCI_DEVICE_ID_LSI_53C875A, 0xff, "875a", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_LCKFRQ}
 ,
 {PCI_DEVICE_ID_LSI_53C1010_33, 0x00, "1010-33", 6, 31, 7, 8,
 FE_WIDE|FE_ULTRA3|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFBC|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_CRC|
 FE_C10}
 ,
 {PCI_DEVICE_ID_LSI_53C1010_33, 0xff, "1010-33", 6, 31, 7, 8,
 FE_WIDE|FE_ULTRA3|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFBC|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_CRC|
 FE_C10|FE_U3EN}
 ,
 {PCI_DEVICE_ID_LSI_53C1010_66, 0xff, "1010-66", 6, 31, 7, 8,
 FE_WIDE|FE_ULTRA3|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFBC|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_DAC|FE_IO256|FE_NOPM|FE_LEDC|FE_66MHZ|FE_CRC|
 FE_C10|FE_U3EN}
 ,
 {PCI_DEVICE_ID_LSI_53C1510, 0xff, "1510d", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_IO256|FE_LEDC}
};

#define sym_num_devs (ARRAY_SIZE(sym_dev_table))

struct sym_chip *
sym_lookup_chip_table (u_short device_id, u_char revision)
{
	struct	sym_chip *chip;
	int	i;

	for (i = 0; i < sym_num_devs; i++) {
		chip = &sym_dev_table[i];
		if (device_id != chip->device_id)
			continue;
		if (revision > chip->revision_id)
			continue;
		return chip;
	}

	return NULL;
}

#if SYM_CONF_DMA_ADDRESSING_MODE == 2
int sym_lookup_dmap(struct sym_hcb *np, u32 h, int s)
{
	int i;

	if (!use_dac(np))
		goto weird;

	
	for (i = SYM_DMAP_SIZE-1; i > 0; i--) {
		if (h == np->dmap_bah[i])
			return i;
	}
	
	if (!np->dmap_bah[s])
		goto new;
	
	for (s = SYM_DMAP_SIZE-1; s > 0; s--) {
		if (!np->dmap_bah[s])
			goto new;
	}
weird:
	panic("sym: ran out of 64 bit DMA segment registers");
	return -1;
new:
	np->dmap_bah[s] = h;
	np->dmap_dirty = 1;
	return s;
}

static void sym_update_dmap_regs(struct sym_hcb *np)
{
	int o, i;

	if (!np->dmap_dirty)
		return;
	o = offsetof(struct sym_reg, nc_scrx[0]);
	for (i = 0; i < SYM_DMAP_SIZE; i++) {
		OUTL_OFF(np, o, np->dmap_bah[i]);
		o += 4;
	}
	np->dmap_dirty = 0;
}
#endif

static void sym_check_goals(struct sym_hcb *np, struct scsi_target *starget,
		struct sym_trans *goal)
{
	if (!spi_support_wide(starget))
		goal->width = 0;

	if (!spi_support_sync(starget)) {
		goal->iu = 0;
		goal->dt = 0;
		goal->qas = 0;
		goal->offset = 0;
		return;
	}

	if (spi_support_dt(starget)) {
		if (spi_support_dt_only(starget))
			goal->dt = 1;

		if (goal->offset == 0)
			goal->dt = 0;
	} else {
		goal->dt = 0;
	}

	
	if ((np->scsi_mode != SMODE_LVD) || !(np->features & FE_U3EN))
		goal->dt = 0;

	if (goal->dt) {
		
		goal->width = 1;
		if (goal->offset > np->maxoffs_dt)
			goal->offset = np->maxoffs_dt;
		if (goal->period < np->minsync_dt)
			goal->period = np->minsync_dt;
		if (goal->period > np->maxsync_dt)
			goal->period = np->maxsync_dt;
	} else {
		goal->iu = goal->qas = 0;
		if (goal->offset > np->maxoffs)
			goal->offset = np->maxoffs;
		if (goal->period < np->minsync)
			goal->period = np->minsync;
		if (goal->period > np->maxsync)
			goal->period = np->maxsync;
	}
}

static int sym_prepare_nego(struct sym_hcb *np, struct sym_ccb *cp, u_char *msgptr)
{
	struct sym_tcb *tp = &np->target[cp->target];
	struct scsi_target *starget = tp->starget;
	struct sym_trans *goal = &tp->tgoal;
	int msglen = 0;
	int nego;

	sym_check_goals(np, starget, goal);

	if (goal->renego == NS_PPR || (goal->offset &&
	    (goal->iu || goal->dt || goal->qas || (goal->period < 0xa)))) {
		nego = NS_PPR;
	} else if (goal->renego == NS_WIDE || goal->width) {
		nego = NS_WIDE;
	} else if (goal->renego == NS_SYNC || goal->offset) {
		nego = NS_SYNC;
	} else {
		goal->check_nego = 0;
		nego = 0;
	}

	switch (nego) {
	case NS_SYNC:
		msglen += spi_populate_sync_msg(msgptr + msglen, goal->period,
				goal->offset);
		break;
	case NS_WIDE:
		msglen += spi_populate_width_msg(msgptr + msglen, goal->width);
		break;
	case NS_PPR:
		msglen += spi_populate_ppr_msg(msgptr + msglen, goal->period,
				goal->offset, goal->width,
				(goal->iu ? PPR_OPT_IU : 0) |
					(goal->dt ? PPR_OPT_DT : 0) |
					(goal->qas ? PPR_OPT_QAS : 0));
		break;
	}

	cp->nego_status = nego;

	if (nego) {
		tp->nego_cp = cp; 
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			sym_print_nego_msg(np, cp->target, 
					  nego == NS_SYNC ? "sync msgout" :
					  nego == NS_WIDE ? "wide msgout" :
					  "ppr msgout", msgptr);
		}
	}

	return msglen;
}

void sym_put_start_queue(struct sym_hcb *np, struct sym_ccb *cp)
{
	u_short	qidx;

#ifdef SYM_CONF_IARB_SUPPORT
	if (np->last_cp && np->iarb_count < np->iarb_max) {
		np->last_cp->host_flags |= HF_HINT_IARB;
		++np->iarb_count;
	}
	else
		np->iarb_count = 0;
	np->last_cp = cp;
#endif

#if   SYM_CONF_DMA_ADDRESSING_MODE == 2
	if (np->dmap_dirty)
		cp->host_xflags |= HX_DMAP_DIRTY;
#endif

	qidx = np->squeueput + 2;
	if (qidx >= MAX_QUEUE*2) qidx = 0;

	np->squeue [qidx]	   = cpu_to_scr(np->idletask_ba);
	MEMORY_WRITE_BARRIER();
	np->squeue [np->squeueput] = cpu_to_scr(cp->ccb_ba);

	np->squeueput = qidx;

	if (DEBUG_FLAGS & DEBUG_QUEUE)
		scmd_printk(KERN_DEBUG, cp->cmd, "queuepos=%d\n",
							np->squeueput);

	MEMORY_WRITE_BARRIER();
	OUTB(np, nc_istat, SIGP|np->istat_sem);
}

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
void sym_start_next_ccbs(struct sym_hcb *np, struct sym_lcb *lp, int maxn)
{
	SYM_QUEHEAD *qp;
	struct sym_ccb *cp;

	assert(!lp->started_tags || !lp->started_no_tag);

	while (maxn--) {
		qp = sym_remque_head(&lp->waiting_ccbq);
		if (!qp)
			break;
		cp = sym_que_entry(qp, struct sym_ccb, link2_ccbq);
		if (cp->tag != NO_TAG) {
			if (lp->started_no_tag ||
			    lp->started_tags >= lp->started_max) {
				sym_insque_head(qp, &lp->waiting_ccbq);
				break;
			}
			lp->itlq_tbl[cp->tag] = cpu_to_scr(cp->ccb_ba);
			lp->head.resel_sa =
				cpu_to_scr(SCRIPTA_BA(np, resel_tag));
			++lp->started_tags;
		} else {
			if (lp->started_no_tag || lp->started_tags) {
				sym_insque_head(qp, &lp->waiting_ccbq);
				break;
			}
			lp->head.itl_task_sa = cpu_to_scr(cp->ccb_ba);
			lp->head.resel_sa =
			      cpu_to_scr(SCRIPTA_BA(np, resel_no_tag));
			++lp->started_no_tag;
		}
		cp->started = 1;
		sym_insque_tail(qp, &lp->started_ccbq);
		sym_put_start_queue(np, cp);
	}
}
#endif 

static int sym_wakeup_done (struct sym_hcb *np)
{
	struct sym_ccb *cp;
	int i, n;
	u32 dsa;

	n = 0;
	i = np->dqueueget;

	
	while (1) {
		dsa = scr_to_cpu(np->dqueue[i]);
		if (!dsa)
			break;
		np->dqueue[i] = 0;
		if ((i = i+2) >= MAX_QUEUE*2)
			i = 0;

		cp = sym_ccb_from_dsa(np, dsa);
		if (cp) {
			MEMORY_READ_BARRIER();
			sym_complete_ok (np, cp);
			++n;
		}
		else
			printf ("%s: bad DSA (%x) in done queue.\n",
				sym_name(np), (u_int) dsa);
	}
	np->dqueueget = i;

	return n;
}

static void sym_flush_comp_queue(struct sym_hcb *np, int cam_status)
{
	SYM_QUEHEAD *qp;
	struct sym_ccb *cp;

	while ((qp = sym_remque_head(&np->comp_ccbq)) != NULL) {
		struct scsi_cmnd *cmd;
		cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		sym_insque_tail(&cp->link_ccbq, &np->busy_ccbq);
		
		if (cp->host_status == HS_WAIT)
			continue;
		cmd = cp->cmd;
		if (cam_status)
			sym_set_cam_status(cmd, cam_status);
#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
		if (sym_get_cam_status(cmd) == DID_SOFT_ERROR) {
			struct sym_tcb *tp = &np->target[cp->target];
			struct sym_lcb *lp = sym_lp(tp, cp->lun);
			if (lp) {
				sym_remque(&cp->link2_ccbq);
				sym_insque_tail(&cp->link2_ccbq,
				                &lp->waiting_ccbq);
				if (cp->started) {
					if (cp->tag != NO_TAG)
						--lp->started_tags;
					else
						--lp->started_no_tag;
				}
			}
			cp->started = 0;
			continue;
		}
#endif
		sym_free_ccb(np, cp);
		sym_xpt_done(np, cmd);
	}
}

static void sym_flush_busy_queue (struct sym_hcb *np, int cam_status)
{
	sym_que_splice(&np->busy_ccbq, &np->comp_ccbq);
	sym_que_init(&np->busy_ccbq);
	sym_flush_comp_queue(np, cam_status);
}

void sym_start_up(struct Scsi_Host *shost, int reason)
{
	struct sym_data *sym_data = shost_priv(shost);
	struct pci_dev *pdev = sym_data->pdev;
	struct sym_hcb *np = sym_data->ncb;
 	int	i;
	u32	phys;

	if (reason == 1)
		sym_soft_reset(np);
	else {
		OUTB(np, nc_stest3, TE|CSF);
		OUTONB(np, nc_ctest3, CLF);
	}
 
	phys = np->squeue_ba;
	for (i = 0; i < MAX_QUEUE*2; i += 2) {
		np->squeue[i]   = cpu_to_scr(np->idletask_ba);
		np->squeue[i+1] = cpu_to_scr(phys + (i+2)*4);
	}
	np->squeue[MAX_QUEUE*2-1] = cpu_to_scr(phys);

	np->squeueput = 0;

	phys = np->dqueue_ba;
	for (i = 0; i < MAX_QUEUE*2; i += 2) {
		np->dqueue[i]   = 0;
		np->dqueue[i+1] = cpu_to_scr(phys + (i+2)*4);
	}
	np->dqueue[MAX_QUEUE*2-1] = cpu_to_scr(phys);

	np->dqueueget = 0;

	np->fw_patch(shost);

	sym_flush_busy_queue(np, DID_RESET);

	OUTB(np, nc_istat,  0x00);			
	INB(np, nc_mbox1);
	udelay(2000); 

	OUTB(np, nc_scntl0, np->rv_scntl0 | 0xc0);
					
	OUTB(np, nc_scntl1, 0x00);		

	sym_selectclock(np, np->rv_scntl3);	

	OUTB(np, nc_scid  , RRE|np->myaddr);	
	OUTW(np, nc_respid, 1ul<<np->myaddr);	
	OUTB(np, nc_istat , SIGP	);		
	OUTB(np, nc_dmode , np->rv_dmode);		
	OUTB(np, nc_ctest5, np->rv_ctest5);	

	OUTB(np, nc_dcntl , NOCOM|np->rv_dcntl);	
	OUTB(np, nc_ctest3, np->rv_ctest3);	
	OUTB(np, nc_ctest4, np->rv_ctest4);	

	
	if (np->features & FE_C10)
		OUTB(np, nc_stest2, np->rv_stest2);
	else
		OUTB(np, nc_stest2, EXT|np->rv_stest2);

	OUTB(np, nc_stest3, TE);			
	OUTB(np, nc_stime0, 0x0c);			

	if (pdev->device == PCI_DEVICE_ID_LSI_53C1010_66)
		OUTB(np, nc_aipcntl1, DISAIP);

	if (pdev->device == PCI_DEVICE_ID_LSI_53C1010_33 &&
	    pdev->revision < 1)
		OUTB(np, nc_stest1, INB(np, nc_stest1) | 0x30);

	if (pdev->device == PCI_DEVICE_ID_NCR_53C875)
		OUTB(np, nc_ctest0, (1<<5));
	else if (pdev->device == PCI_DEVICE_ID_NCR_53C896)
		np->rv_ccntl0 |= DPR;

	if (np->features & (FE_DAC|FE_NOPM)) {
		OUTB(np, nc_ccntl0, np->rv_ccntl0);
		OUTB(np, nc_ccntl1, np->rv_ccntl1);
	}

#if	SYM_CONF_DMA_ADDRESSING_MODE == 2
	if (use_dac(np)) {
		np->dmap_bah[0] = 0;	
		OUTL(np, nc_scrx[0], np->dmap_bah[0]);
		OUTL(np, nc_drs, np->dmap_bah[0]);
	}
#endif

	if (np->features & FE_NOPM) {
		OUTL(np, nc_pmjad1, SCRIPTB_BA(np, pm_handle));
		OUTL(np, nc_pmjad2, SCRIPTB_BA(np, pm_handle));
	}

	if (np->features & FE_LED0)
		OUTB(np, nc_gpcntl, INB(np, nc_gpcntl) & ~0x01);
	else if (np->features & FE_LEDC)
		OUTB(np, nc_gpcntl, (INB(np, nc_gpcntl) & ~0x41) | 0x20);

	OUTW(np, nc_sien , STO|HTH|MA|SGE|UDC|RST|PAR);
	OUTB(np, nc_dien , MDPE|BF|SSI|SIR|IID);

	if (np->features & (FE_ULTRA2|FE_ULTRA3)) {
		OUTONW(np, nc_sien, SBMC);
		if (reason == 0) {
			INB(np, nc_mbox1);
			mdelay(100);
			INW(np, nc_sist);
		}
		np->scsi_mode = INB(np, nc_stest4) & SMODE;
	}

	for (i=0;i<SYM_CONF_MAX_TARGET;i++) {
		struct sym_tcb *tp = &np->target[i];

		tp->to_reset  = 0;
		tp->head.sval = 0;
		tp->head.wval = np->rv_scntl3;
		tp->head.uval = 0;
		if (tp->lun0p)
			tp->lun0p->to_clear = 0;
		if (tp->lunmp) {
			int ln;

			for (ln = 1; ln < SYM_CONF_MAX_LUN; ln++)
				if (tp->lunmp[ln])
					tp->lunmp[ln]->to_clear = 0;
		}
	}

	phys = SCRIPTA_BA(np, init);
	if (np->ram_ba) {
		if (sym_verbose >= 2)
			printf("%s: Downloading SCSI SCRIPTS.\n", sym_name(np));
		memcpy_toio(np->s.ramaddr, np->scripta0, np->scripta_sz);
		if (np->features & FE_RAM8K) {
			memcpy_toio(np->s.ramaddr + 4096, np->scriptb0, np->scriptb_sz);
			phys = scr_to_cpu(np->scr_ram_seg);
			OUTL(np, nc_mmws, phys);
			OUTL(np, nc_mmrs, phys);
			OUTL(np, nc_sfs,  phys);
			phys = SCRIPTB_BA(np, start64);
		}
	}

	np->istat_sem = 0;

	OUTL(np, nc_dsa, np->hcb_ba);
	OUTL_DSP(np, phys);

	if (reason != 0)
		sym_xpt_async_bus_reset(np);
}

static void sym_settrans(struct sym_hcb *np, int target, u_char opts, u_char ofs,
			 u_char per, u_char wide, u_char div, u_char fak)
{
	SYM_QUEHEAD *qp;
	u_char sval, wval, uval;
	struct sym_tcb *tp = &np->target[target];

	assert(target == (INB(np, nc_sdid) & 0x0f));

	sval = tp->head.sval;
	wval = tp->head.wval;
	uval = tp->head.uval;

#if 0
	printf("XXXX sval=%x wval=%x uval=%x (%x)\n", 
		sval, wval, uval, np->rv_scntl3);
#endif
	if (!(np->features & FE_C10))
		sval = (sval & ~0x1f) | ofs;
	else
		sval = (sval & ~0x3f) | ofs;

	if (ofs != 0) {
		wval = (wval & ~0x70) | ((div+1) << 4);
		if (!(np->features & FE_C10))
			sval = (sval & ~0xe0) | (fak << 5);
		else {
			uval = uval & ~(XCLKH_ST|XCLKH_DT|XCLKS_ST|XCLKS_DT);
			if (fak >= 1) uval |= (XCLKH_ST|XCLKH_DT);
			if (fak >= 2) uval |= (XCLKS_ST|XCLKS_DT);
		}
	}

	wval = wval & ~EWS;
	if (wide != 0)
		wval |= EWS;

	if (np->features & FE_C10) {
		uval = uval & ~(U3EN|AIPCKEN);
		if (opts)	{
			assert(np->features & FE_U3EN);
			uval |= U3EN;
		}
	} else {
		wval = wval & ~ULTRA;
		if (per <= 12)	wval |= ULTRA;
	}

	if (tp->head.sval == sval && 
	    tp->head.wval == wval &&
	    tp->head.uval == uval)
		return;
	tp->head.sval = sval;
	tp->head.wval = wval;
	tp->head.uval = uval;

	if (per < 50 && !(np->features & FE_C10))
		OUTOFFB(np, nc_stest2, EXT);

	OUTB(np, nc_sxfer,  tp->head.sval);
	OUTB(np, nc_scntl3, tp->head.wval);

	if (np->features & FE_C10) {
		OUTB(np, nc_scntl4, tp->head.uval);
	}

	FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
		struct sym_ccb *cp;
		cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		if (cp->target != target)
			continue;
		cp->phys.select.sel_scntl3 = tp->head.wval;
		cp->phys.select.sel_sxfer  = tp->head.sval;
		if (np->features & FE_C10) {
			cp->phys.select.sel_scntl4 = tp->head.uval;
		}
	}
}

static void sym_announce_transfer_rate(struct sym_tcb *tp)
{
	struct scsi_target *starget = tp->starget;

	if (tp->tprint.period != spi_period(starget) ||
	    tp->tprint.offset != spi_offset(starget) ||
	    tp->tprint.width != spi_width(starget) ||
	    tp->tprint.iu != spi_iu(starget) ||
	    tp->tprint.dt != spi_dt(starget) ||
	    tp->tprint.qas != spi_qas(starget) ||
	    !tp->tprint.check_nego) {
		tp->tprint.period = spi_period(starget);
		tp->tprint.offset = spi_offset(starget);
		tp->tprint.width = spi_width(starget);
		tp->tprint.iu = spi_iu(starget);
		tp->tprint.dt = spi_dt(starget);
		tp->tprint.qas = spi_qas(starget);
		tp->tprint.check_nego = 1;

		spi_display_xfer_agreement(starget);
	}
}

static void sym_setwide(struct sym_hcb *np, int target, u_char wide)
{
	struct sym_tcb *tp = &np->target[target];
	struct scsi_target *starget = tp->starget;

	sym_settrans(np, target, 0, 0, 0, wide, 0, 0);

	if (wide)
		tp->tgoal.renego = NS_WIDE;
	else
		tp->tgoal.renego = 0;
	tp->tgoal.check_nego = 0;
	tp->tgoal.width = wide;
	spi_offset(starget) = 0;
	spi_period(starget) = 0;
	spi_width(starget) = wide;
	spi_iu(starget) = 0;
	spi_dt(starget) = 0;
	spi_qas(starget) = 0;

	if (sym_verbose >= 3)
		sym_announce_transfer_rate(tp);
}

static void
sym_setsync(struct sym_hcb *np, int target,
            u_char ofs, u_char per, u_char div, u_char fak)
{
	struct sym_tcb *tp = &np->target[target];
	struct scsi_target *starget = tp->starget;
	u_char wide = (tp->head.wval & EWS) ? BUS_16_BIT : BUS_8_BIT;

	sym_settrans(np, target, 0, ofs, per, wide, div, fak);

	if (wide)
		tp->tgoal.renego = NS_WIDE;
	else if (ofs)
		tp->tgoal.renego = NS_SYNC;
	else
		tp->tgoal.renego = 0;
	spi_period(starget) = per;
	spi_offset(starget) = ofs;
	spi_iu(starget) = spi_dt(starget) = spi_qas(starget) = 0;

	if (!tp->tgoal.dt && !tp->tgoal.iu && !tp->tgoal.qas) {
		tp->tgoal.period = per;
		tp->tgoal.offset = ofs;
		tp->tgoal.check_nego = 0;
	}

	sym_announce_transfer_rate(tp);
}

static void 
sym_setpprot(struct sym_hcb *np, int target, u_char opts, u_char ofs,
             u_char per, u_char wide, u_char div, u_char fak)
{
	struct sym_tcb *tp = &np->target[target];
	struct scsi_target *starget = tp->starget;

	sym_settrans(np, target, opts, ofs, per, wide, div, fak);

	if (wide || ofs)
		tp->tgoal.renego = NS_PPR;
	else
		tp->tgoal.renego = 0;
	spi_width(starget) = tp->tgoal.width = wide;
	spi_period(starget) = tp->tgoal.period = per;
	spi_offset(starget) = tp->tgoal.offset = ofs;
	spi_iu(starget) = tp->tgoal.iu = !!(opts & PPR_OPT_IU);
	spi_dt(starget) = tp->tgoal.dt = !!(opts & PPR_OPT_DT);
	spi_qas(starget) = tp->tgoal.qas = !!(opts & PPR_OPT_QAS);
	tp->tgoal.check_nego = 0;

	sym_announce_transfer_rate(tp);
}

static void sym_recover_scsi_int (struct sym_hcb *np, u_char hsts)
{
	u32	dsp	= INL(np, nc_dsp);
	u32	dsa	= INL(np, nc_dsa);
	struct sym_ccb *cp	= sym_ccb_from_dsa(np, dsa);

	if ((!(dsp > SCRIPTA_BA(np, getjob_begin) &&
	       dsp < SCRIPTA_BA(np, getjob_end) + 1)) &&
	    (!(dsp > SCRIPTA_BA(np, ungetjob) &&
	       dsp < SCRIPTA_BA(np, reselect) + 1)) &&
	    (!(dsp > SCRIPTB_BA(np, sel_for_abort) &&
	       dsp < SCRIPTB_BA(np, sel_for_abort_1) + 1)) &&
	    (!(dsp > SCRIPTA_BA(np, done) &&
	       dsp < SCRIPTA_BA(np, done_end) + 1))) {
		OUTB(np, nc_ctest3, np->rv_ctest3 | CLF); 
		OUTB(np, nc_stest3, TE|CSF);		
		if (cp) {
			cp->host_status = hsts;
			OUTL_DSP(np, SCRIPTA_BA(np, complete_error));
		}
		else {
			OUTL(np, nc_dsa, 0xffffff);
			OUTL_DSP(np, SCRIPTA_BA(np, start));
		}
	}
	else
		goto reset_all;

	return;

reset_all:
	sym_start_reset(np);
}

static void sym_int_sto (struct sym_hcb *np)
{
	u32 dsp	= INL(np, nc_dsp);

	if (DEBUG_FLAGS & DEBUG_TINY) printf ("T");

	if (dsp == SCRIPTA_BA(np, wf_sel_done) + 8)
		sym_recover_scsi_int(np, HS_SEL_TIMEOUT);
	else
		sym_start_reset(np);
}

static void sym_int_udc (struct sym_hcb *np)
{
	printf ("%s: unexpected disconnect\n", sym_name(np));
	sym_recover_scsi_int(np, HS_UNEXPECTED);
}

static void sym_int_sbmc(struct Scsi_Host *shost)
{
	struct sym_hcb *np = sym_get_hcb(shost);
	u_char scsi_mode = INB(np, nc_stest4) & SMODE;

	printf("%s: SCSI BUS mode change from %s to %s.\n", sym_name(np),
		sym_scsi_bus_mode(np->scsi_mode), sym_scsi_bus_mode(scsi_mode));

	sym_start_up(shost, 2);
}

static void sym_int_par (struct sym_hcb *np, u_short sist)
{
	u_char	hsts	= INB(np, HS_PRT);
	u32	dsp	= INL(np, nc_dsp);
	u32	dbc	= INL(np, nc_dbc);
	u32	dsa	= INL(np, nc_dsa);
	u_char	sbcl	= INB(np, nc_sbcl);
	u_char	cmd	= dbc >> 24;
	int phase	= cmd & 7;
	struct sym_ccb *cp	= sym_ccb_from_dsa(np, dsa);

	if (printk_ratelimit())
		printf("%s: SCSI parity error detected: SCR1=%d DBC=%x SBCL=%x\n",
			sym_name(np), hsts, dbc, sbcl);

	if (!(INB(np, nc_scntl1) & ISCON)) {
		sym_recover_scsi_int(np, HS_UNEXPECTED);
		return;
	}

	if (!cp)
		goto reset_all;

	if ((cmd & 0xc0) || !(phase & 1) || !(sbcl & 0x8))
		goto reset_all;

	OUTONB(np, HF_PRT, HF_EXT_ERR);
	cp->xerr_status |= XE_PARITY_ERR;

	np->msgout[0] = (phase == 7) ? M_PARITY : M_ID_ERROR;

	if (phase == 1 || phase == 5) {
		
		if (dsp == SCRIPTB_BA(np, pm_handle))
			OUTL_DSP(np, dsp);
		
		else if (sist & MA)
			sym_int_ma (np);
		
		else {
			sym_set_script_dp (np, cp, dsp);
			OUTL_DSP(np, SCRIPTA_BA(np, dispatch));
		}
	}
	else if (phase == 7)	
#if 1				
		goto reset_all; 
#else
		OUTL_DSP(np, SCRIPTA_BA(np, clrack));
#endif
	else
		OUTL_DSP(np, SCRIPTA_BA(np, dispatch));
	return;

reset_all:
	sym_start_reset(np);
	return;
}

static void sym_int_ma (struct sym_hcb *np)
{
	u32	dbc;
	u32	rest;
	u32	dsp;
	u32	dsa;
	u32	nxtdsp;
	u32	*vdsp;
	u32	oadr, olen;
	u32	*tblp;
        u32	newcmd;
	u_int	delta;
	u_char	cmd;
	u_char	hflags, hflags0;
	struct	sym_pmc *pm;
	struct sym_ccb *cp;

	dsp	= INL(np, nc_dsp);
	dbc	= INL(np, nc_dbc);
	dsa	= INL(np, nc_dsa);

	cmd	= dbc >> 24;
	rest	= dbc & 0xffffff;
	delta	= 0;

	cp = sym_ccb_from_dsa(np, dsa);

	if ((cmd & 7) != 1 && (cmd & 7) != 5) {
		u_char ss0, ss2;

		if (np->features & FE_DFBC)
			delta = INW(np, nc_dfbc);
		else {
			u32 dfifo;

			dfifo = INL(np, nc_dfifo);

			if (dfifo & (DFS << 16))
				delta = ((((dfifo >> 8) & 0x300) |
				          (dfifo & 0xff)) - rest) & 0x3ff;
			else
				delta = ((dfifo & 0xff) - rest) & 0x7f;
		}

		rest += delta;
		ss0  = INB(np, nc_sstat0);
		if (ss0 & OLF) rest++;
		if (!(np->features & FE_C10))
			if (ss0 & ORF) rest++;
		if (cp && (cp->phys.select.sel_scntl3 & EWS)) {
			ss2 = INB(np, nc_sstat2);
			if (ss2 & OLF1) rest++;
			if (!(np->features & FE_C10))
				if (ss2 & ORF1) rest++;
		}

		OUTB(np, nc_ctest3, np->rv_ctest3 | CLF);	
		OUTB(np, nc_stest3, TE|CSF);		
	}

	if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_PHASE))
		printf ("P%x%x RL=%d D=%d ", cmd&7, INB(np, nc_sbcl)&7,
			(unsigned) rest, (unsigned) delta);

	vdsp	= NULL;
	nxtdsp	= 0;
	if	(dsp >  np->scripta_ba &&
		 dsp <= np->scripta_ba + np->scripta_sz) {
		vdsp = (u32 *)((char*)np->scripta0 + (dsp-np->scripta_ba-8));
		nxtdsp = dsp;
	}
	else if	(dsp >  np->scriptb_ba &&
		 dsp <= np->scriptb_ba + np->scriptb_sz) {
		vdsp = (u32 *)((char*)np->scriptb0 + (dsp-np->scriptb_ba-8));
		nxtdsp = dsp;
	}

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printf ("\nCP=%p DSP=%x NXT=%x VDSP=%p CMD=%x ",
			cp, (unsigned)dsp, (unsigned)nxtdsp, vdsp, cmd);
	}

	if (!vdsp) {
		printf ("%s: interrupted SCRIPT address not found.\n", 
			sym_name (np));
		goto reset_all;
	}

	if (!cp) {
		printf ("%s: SCSI phase error fixup: CCB already dequeued.\n", 
			sym_name (np));
		goto reset_all;
	}

	oadr = scr_to_cpu(vdsp[1]);

	if (cmd & 0x10) {	
		tblp = (u32 *) ((char*) &cp->phys + oadr);
		olen = scr_to_cpu(tblp[0]);
		oadr = scr_to_cpu(tblp[1]);
	} else {
		tblp = (u32 *) 0;
		olen = scr_to_cpu(vdsp[0]) & 0xffffff;
	}

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printf ("OCMD=%x\nTBLP=%p OLEN=%x OADR=%x\n",
			(unsigned) (scr_to_cpu(vdsp[0]) >> 24),
			tblp,
			(unsigned) olen,
			(unsigned) oadr);
	}

	if (((cmd & 2) ? cmd : (cmd & ~4)) != (scr_to_cpu(vdsp[0]) >> 24)) {
		sym_print_addr(cp->cmd,
			"internal error: cmd=%02x != %02x=(vdsp[0] >> 24)\n",
			cmd, scr_to_cpu(vdsp[0]) >> 24);

		goto reset_all;
	}

	if (cmd & 2) {
		sym_print_addr(cp->cmd,
			"phase change %x-%x %d@%08x resid=%d.\n",
			cmd&7, INB(np, nc_sbcl)&7, (unsigned)olen,
			(unsigned)oadr, (unsigned)rest);
		goto unexpected_phase;
	}

	hflags0 = INB(np, HF_PRT);
	hflags = hflags0;

	if (hflags & (HF_IN_PM0 | HF_IN_PM1 | HF_DP_SAVED)) {
		if (hflags & HF_IN_PM0)
			nxtdsp = scr_to_cpu(cp->phys.pm0.ret);
		else if	(hflags & HF_IN_PM1)
			nxtdsp = scr_to_cpu(cp->phys.pm1.ret);

		if (hflags & HF_DP_SAVED)
			hflags ^= HF_ACT_PM;
	}

	if (!(hflags & HF_ACT_PM)) {
		pm = &cp->phys.pm0;
		newcmd = SCRIPTA_BA(np, pm0_data);
	}
	else {
		pm = &cp->phys.pm1;
		newcmd = SCRIPTA_BA(np, pm1_data);
	}

	hflags &= ~(HF_IN_PM0 | HF_IN_PM1 | HF_DP_SAVED);
	if (hflags != hflags0)
		OUTB(np, HF_PRT, hflags);

	pm->sg.addr = cpu_to_scr(oadr + olen - rest);
	pm->sg.size = cpu_to_scr(rest);
	pm->ret     = cpu_to_scr(nxtdsp);

	nxtdsp = SCRIPTA_BA(np, dispatch);
	if ((cmd & 7) == 1 && cp && (cp->phys.select.sel_scntl3 & EWS) &&
	    (INB(np, nc_scntl2) & WSR)) {
		u32 tmp;

		tmp = scr_to_cpu(pm->sg.addr);
		cp->phys.wresid.addr = cpu_to_scr(tmp);
		pm->sg.addr = cpu_to_scr(tmp + 1);
		tmp = scr_to_cpu(pm->sg.size);
		cp->phys.wresid.size = cpu_to_scr((tmp&0xff000000) | 1);
		pm->sg.size = cpu_to_scr(tmp - 1);

		if ((tmp&0xffffff) == 1)
			newcmd = pm->ret;

		nxtdsp = SCRIPTB_BA(np, wsr_ma_helper);
	}

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		sym_print_addr(cp->cmd, "PM %x %x %x / %x %x %x.\n",
			hflags0, hflags, newcmd,
			(unsigned)scr_to_cpu(pm->sg.addr),
			(unsigned)scr_to_cpu(pm->sg.size),
			(unsigned)scr_to_cpu(pm->ret));
	}

	sym_set_script_dp (np, cp, newcmd);
	OUTL_DSP(np, nxtdsp);
	return;

unexpected_phase:
	dsp -= 8;
	nxtdsp = 0;

	switch (cmd & 7) {
	case 2:	
		nxtdsp = SCRIPTA_BA(np, dispatch);
		break;
#if 0
	case 3:	
		nxtdsp = SCRIPTA_BA(np, dispatch);
		break;
#endif
	case 6:	
		if	(dsp == SCRIPTA_BA(np, send_ident)) {
			if (cp->tag != NO_TAG && olen - rest <= 3) {
				cp->host_status = HS_BUSY;
				np->msgout[0] = IDENTIFY(0, cp->lun);
				nxtdsp = SCRIPTB_BA(np, ident_break_atn);
			}
			else
				nxtdsp = SCRIPTB_BA(np, ident_break);
		}
		else if	(dsp == SCRIPTB_BA(np, send_wdtr) ||
			 dsp == SCRIPTB_BA(np, send_sdtr) ||
			 dsp == SCRIPTB_BA(np, send_ppr)) {
			nxtdsp = SCRIPTB_BA(np, nego_bad_phase);
			if (dsp == SCRIPTB_BA(np, send_ppr)) {
				struct scsi_device *dev = cp->cmd->device;
				dev->ppr = 0;
			}
		}
		break;
#if 0
	case 7:	
		nxtdsp = SCRIPTA_BA(np, clrack);
		break;
#endif
	}

	if (nxtdsp) {
		OUTL_DSP(np, nxtdsp);
		return;
	}

reset_all:
	sym_start_reset(np);
}


irqreturn_t sym_interrupt(struct Scsi_Host *shost)
{
	struct sym_data *sym_data = shost_priv(shost);
	struct sym_hcb *np = sym_data->ncb;
	struct pci_dev *pdev = sym_data->pdev;
	u_char	istat, istatc;
	u_char	dstat;
	u_short	sist;

	istat = INB(np, nc_istat);
	if (istat & INTF) {
		OUTB(np, nc_istat, (istat & SIGP) | INTF | np->istat_sem);
		istat |= INB(np, nc_istat);		
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("F ");
		sym_wakeup_done(np);
	}

	if (!(istat & (SIP|DIP)))
		return (istat & INTF) ? IRQ_HANDLED : IRQ_NONE;

#if 0	
	if (istat & CABRT)
		OUTB(np, nc_istat, CABRT);
#endif

	sist	= 0;
	dstat	= 0;
	istatc	= istat;
	do {
		if (istatc & SIP)
			sist  |= INW(np, nc_sist);
		if (istatc & DIP)
			dstat |= INB(np, nc_dstat);
		istatc = INB(np, nc_istat);
		istat |= istatc;

		if (unlikely(sist == 0xffff && dstat == 0xff)) {
			if (pci_channel_offline(pdev))
				return IRQ_NONE;
		}
	} while (istatc & (SIP|DIP));

	if (DEBUG_FLAGS & DEBUG_TINY)
		printf ("<%d|%x:%x|%x:%x>",
			(int)INB(np, nc_scr0),
			dstat,sist,
			(unsigned)INL(np, nc_dsp),
			(unsigned)INL(np, nc_dbc));
	MEMORY_READ_BARRIER();

	if (!(sist  & (STO|GEN|HTH|SGE|UDC|SBMC|RST)) &&
	    !(dstat & (MDPE|BF|ABRT|IID))) {
		if	(sist & PAR)	sym_int_par (np, sist);
		else if (sist & MA)	sym_int_ma (np);
		else if (dstat & SIR)	sym_int_sir(np);
		else if (dstat & SSI)	OUTONB_STD();
		else			goto unknown_int;
		return IRQ_HANDLED;
	}

	if (sist & RST) {
		printf("%s: SCSI BUS reset detected.\n", sym_name(np));
		sym_start_up(shost, 1);
		return IRQ_HANDLED;
	}

	OUTB(np, nc_ctest3, np->rv_ctest3 | CLF);	
	OUTB(np, nc_stest3, TE|CSF);		

	if (!(sist  & (GEN|HTH|SGE)) &&
	    !(dstat & (MDPE|BF|ABRT|IID))) {
		if	(sist & SBMC)	sym_int_sbmc(shost);
		else if (sist & STO)	sym_int_sto (np);
		else if (sist & UDC)	sym_int_udc (np);
		else			goto unknown_int;
		return IRQ_HANDLED;
	}


	sym_log_hard_error(shost, sist, dstat);

	if ((sist & (GEN|HTH|SGE)) ||
		(dstat & (MDPE|BF|ABRT|IID))) {
		sym_start_reset(np);
		return IRQ_HANDLED;
	}

unknown_int:
	printf(	"%s: unknown interrupt(s) ignored, "
		"ISTAT=0x%x DSTAT=0x%x SIST=0x%x\n",
		sym_name(np), istat, dstat, sist);
	return IRQ_NONE;
}

static int 
sym_dequeue_from_squeue(struct sym_hcb *np, int i, int target, int lun, int task)
{
	int j;
	struct sym_ccb *cp;

	assert((i >= 0) && (i < 2*MAX_QUEUE));

	j = i;
	while (i != np->squeueput) {
		cp = sym_ccb_from_dsa(np, scr_to_cpu(np->squeue[i]));
		assert(cp);
#ifdef SYM_CONF_IARB_SUPPORT
		
		cp->host_flags &= ~HF_HINT_IARB;
#endif
		if ((target == -1 || cp->target == target) &&
		    (lun    == -1 || cp->lun    == lun)    &&
		    (task   == -1 || cp->tag    == task)) {
			sym_set_cam_status(cp->cmd, DID_SOFT_ERROR);
			sym_remque(&cp->link_ccbq);
			sym_insque_tail(&cp->link_ccbq, &np->comp_ccbq);
		}
		else {
			if (i != j)
				np->squeue[j] = np->squeue[i];
			if ((j += 2) >= MAX_QUEUE*2) j = 0;
		}
		if ((i += 2) >= MAX_QUEUE*2) i = 0;
	}
	if (i != j)		
		np->squeue[j] = np->squeue[i];
	np->squeueput = j;	

	return (i - j) / 2;
}

static void sym_sir_bad_scsi_status(struct sym_hcb *np, int num, struct sym_ccb *cp)
{
	u32		startp;
	u_char		s_status = cp->ssss_status;
	u_char		h_flags  = cp->host_flags;
	int		msglen;
	int		i;

	i = (INL(np, nc_scratcha) - np->squeue_ba) / 4;

#ifdef SYM_CONF_IARB_SUPPORT
	if (np->last_cp)
		np->last_cp = 0;
#endif

	switch(s_status) {
	case S_BUSY:
	case S_QUEUE_FULL:
		if (sym_verbose >= 2) {
			sym_print_addr(cp->cmd, "%s\n",
			        s_status == S_BUSY ? "BUSY" : "QUEUE FULL\n");
		}
	default:	
		sym_complete_error (np, cp);
		break;
	case S_TERMINATED:
	case S_CHECK_COND:
		if (h_flags & HF_SENSE) {
			sym_complete_error (np, cp);
			break;
		}

		sym_dequeue_from_squeue(np, i, cp->target, cp->lun, -1);
		OUTL_DSP(np, SCRIPTA_BA(np, start));

		cp->sv_scsi_status = cp->ssss_status;
		cp->sv_xerr_status = cp->xerr_status;
		cp->sv_resid = sym_compute_residual(np, cp);


		cp->scsi_smsg2[0] = IDENTIFY(0, cp->lun);
		msglen = 1;

		cp->nego_status = 0;
		msglen += sym_prepare_nego(np, cp, &cp->scsi_smsg2[msglen]);
		cp->phys.smsg.addr	= CCB_BA(cp, scsi_smsg2);
		cp->phys.smsg.size	= cpu_to_scr(msglen);

		cp->phys.cmd.addr	= CCB_BA(cp, sensecmd);
		cp->phys.cmd.size	= cpu_to_scr(6);

		cp->sensecmd[0]		= REQUEST_SENSE;
		cp->sensecmd[1]		= 0;
		if (cp->cmd->device->scsi_level <= SCSI_2 && cp->lun <= 7)
			cp->sensecmd[1]	= cp->lun << 5;
		cp->sensecmd[4]		= SYM_SNS_BBUF_LEN;
		cp->data_len		= SYM_SNS_BBUF_LEN;

		memset(cp->sns_bbuf, 0, SYM_SNS_BBUF_LEN);
		cp->phys.sense.addr	= CCB_BA(cp, sns_bbuf);
		cp->phys.sense.size	= cpu_to_scr(SYM_SNS_BBUF_LEN);

		startp = SCRIPTB_BA(np, sdata_in);

		cp->phys.head.savep	= cpu_to_scr(startp);
		cp->phys.head.lastp	= cpu_to_scr(startp);
		cp->startp		= cpu_to_scr(startp);
		cp->goalp		= cpu_to_scr(startp + 16);

		cp->host_xflags = 0;
		cp->host_status	= cp->nego_status ? HS_NEGOTIATE : HS_BUSY;
		cp->ssss_status = S_ILLEGAL;
		cp->host_flags	= (HF_SENSE|HF_DATA_IN);
		cp->xerr_status = 0;
		cp->extra_bytes = 0;

		cp->phys.head.go.start = cpu_to_scr(SCRIPTA_BA(np, select));

		sym_put_start_queue(np, cp);

		sym_flush_comp_queue(np, 0);
		break;
	}
}

int sym_clear_tasks(struct sym_hcb *np, int cam_status, int target, int lun, int task)
{
	SYM_QUEHEAD qtmp, *qp;
	int i = 0;
	struct sym_ccb *cp;

	sym_que_init(&qtmp);
	sym_que_splice(&np->busy_ccbq, &qtmp);
	sym_que_init(&np->busy_ccbq);

	while ((qp = sym_remque_head(&qtmp)) != NULL) {
		struct scsi_cmnd *cmd;
		cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		cmd = cp->cmd;
		if (cp->host_status != HS_DISCONNECT ||
		    cp->target != target	     ||
		    (lun  != -1 && cp->lun != lun)   ||
		    (task != -1 && 
			(cp->tag != NO_TAG && cp->scsi_smsg[2] != task))) {
			sym_insque_tail(&cp->link_ccbq, &np->busy_ccbq);
			continue;
		}
		sym_insque_tail(&cp->link_ccbq, &np->comp_ccbq);

		
		if (sym_get_cam_status(cmd) != DID_TIME_OUT)
			sym_set_cam_status(cmd, cam_status);
		++i;
#if 0
printf("XXXX TASK @%p CLEARED\n", cp);
#endif
	}
	return i;
}

static void sym_sir_task_recovery(struct sym_hcb *np, int num)
{
	SYM_QUEHEAD *qp;
	struct sym_ccb *cp;
	struct sym_tcb *tp = NULL; 
	struct scsi_target *starget;
	int target=-1, lun=-1, task;
	int i, k;

	switch(num) {
	case SIR_SCRIPT_STOPPED:
		for (i = 0 ; i < SYM_CONF_MAX_TARGET ; i++) {
			tp = &np->target[i];
			if (tp->to_reset || 
			    (tp->lun0p && tp->lun0p->to_clear)) {
				target = i;
				break;
			}
			if (!tp->lunmp)
				continue;
			for (k = 1 ; k < SYM_CONF_MAX_LUN ; k++) {
				if (tp->lunmp[k] && tp->lunmp[k]->to_clear) {
					target	= i;
					break;
				}
			}
			if (target != -1)
				break;
		}

		if (target == -1) {
			FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
				cp = sym_que_entry(qp,struct sym_ccb,link_ccbq);
				if (cp->host_status != HS_DISCONNECT)
					continue;
				if (cp->to_abort) {
					target = cp->target;
					break;
				}
			}
		}

		if (target != -1) {
			tp = &np->target[target];
			np->abrt_sel.sel_id	= target;
			np->abrt_sel.sel_scntl3 = tp->head.wval;
			np->abrt_sel.sel_sxfer  = tp->head.sval;
			OUTL(np, nc_dsa, np->hcb_ba);
			OUTL_DSP(np, SCRIPTB_BA(np, sel_for_abort));
			return;
		}

		i = 0;
		cp = NULL;
		FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
			cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
			if (cp->host_status != HS_BUSY &&
			    cp->host_status != HS_NEGOTIATE)
				continue;
			if (!cp->to_abort)
				continue;
#ifdef SYM_CONF_IARB_SUPPORT
			if (cp == np->last_cp) {
				cp->to_abort = 0;
				continue;
			}
#endif
			i = 1;	
			break;
		}
		if (!i) {
			np->istat_sem = 0;
			OUTB(np, nc_istat, SIGP);
			break;
		}
		i = (INL(np, nc_scratcha) - np->squeue_ba) / 4;
		i = sym_dequeue_from_squeue(np, i, cp->target, cp->lun, -1);

#ifndef SYM_OPT_HANDLE_DEVICE_QUEUEING
		assert(i && sym_get_cam_status(cp->cmd) == DID_SOFT_ERROR);
#else
		sym_remque(&cp->link_ccbq);
		sym_insque_tail(&cp->link_ccbq, &np->comp_ccbq);
#endif
		if (cp->to_abort == 2)
			sym_set_cam_status(cp->cmd, DID_TIME_OUT);
		else
			sym_set_cam_status(cp->cmd, DID_ABORT);

		sym_flush_comp_queue(np, 0);
		break;
	case SIR_TARGET_SELECTED:
		target = INB(np, nc_sdid) & 0xf;
		tp = &np->target[target];

		np->abrt_tbl.addr = cpu_to_scr(vtobus(np->abrt_msg));

		if (tp->to_reset) {
			np->abrt_msg[0] = M_RESET;
			np->abrt_tbl.size = 1;
			tp->to_reset = 0;
			break;
		}

		if (tp->lun0p && tp->lun0p->to_clear)
			lun = 0;
		else if (tp->lunmp) {
			for (k = 1 ; k < SYM_CONF_MAX_LUN ; k++) {
				if (tp->lunmp[k] && tp->lunmp[k]->to_clear) {
					lun = k;
					break;
				}
			}
		}

		if (lun != -1) {
			struct sym_lcb *lp = sym_lp(tp, lun);
			lp->to_clear = 0; 
			np->abrt_msg[0] = IDENTIFY(0, lun);
			np->abrt_msg[1] = M_ABORT;
			np->abrt_tbl.size = 2;
			break;
		}

		i = 0;
		cp = NULL;
		FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
			cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
			if (cp->host_status != HS_DISCONNECT)
				continue;
			if (cp->target != target)
				continue;
			if (!cp->to_abort)
				continue;
			i = 1;	
			break;
		}

		if (!i) {
			np->abrt_msg[0] = M_ABORT;
			np->abrt_tbl.size = 1;
			break;
		}

		np->abrt_msg[0] = IDENTIFY(0, cp->lun);

		if (cp->tag == NO_TAG) {
			np->abrt_msg[1] = M_ABORT;
			np->abrt_tbl.size = 2;
		} else {
			np->abrt_msg[1] = cp->scsi_smsg[1];
			np->abrt_msg[2] = cp->scsi_smsg[2];
			np->abrt_msg[3] = M_ABORT_TAG;
			np->abrt_tbl.size = 4;
		}
		if (cp->to_abort == 2)
			sym_set_cam_status(cp->cmd, DID_TIME_OUT);
		cp->to_abort = 0; 
		break;

	case SIR_ABORT_SENT:
		target = INB(np, nc_sdid) & 0xf;
		tp = &np->target[target];
		starget = tp->starget;
		
		if (np->abrt_msg[0] == M_ABORT)
			break;

		lun = -1;
		task = -1;
		if (np->abrt_msg[0] == M_RESET) {
			tp->head.sval = 0;
			tp->head.wval = np->rv_scntl3;
			tp->head.uval = 0;
			spi_period(starget) = 0;
			spi_offset(starget) = 0;
			spi_width(starget) = 0;
			spi_iu(starget) = 0;
			spi_dt(starget) = 0;
			spi_qas(starget) = 0;
			tp->tgoal.check_nego = 1;
			tp->tgoal.renego = 0;
		}

		else {
			lun = np->abrt_msg[0] & 0x3f;
			if (np->abrt_msg[1] == M_ABORT_TAG)
				task = np->abrt_msg[2];
		}

		i = (INL(np, nc_scratcha) - np->squeue_ba) / 4;
		sym_dequeue_from_squeue(np, i, target, lun, -1);
		sym_clear_tasks(np, DID_ABORT, target, lun, task);
		sym_flush_comp_queue(np, 0);

		if (np->abrt_msg[0] == M_RESET)
			starget_printk(KERN_NOTICE, starget,
							"has been reset\n");
		break;
	}

	if (num == SIR_TARGET_SELECTED) {
		dev_info(&tp->starget->dev, "control msgout:");
		sym_printl_hex(np->abrt_msg, np->abrt_tbl.size);
		np->abrt_tbl.size = cpu_to_scr(np->abrt_tbl.size);
	}

	OUTONB_STD();
}


static int sym_evaluate_dp(struct sym_hcb *np, struct sym_ccb *cp, u32 scr, int *ofs)
{
	u32	dp_scr;
	int	dp_ofs, dp_sg, dp_sgmin;
	int	tmp;
	struct sym_pmc *pm;

	dp_scr = scr;
	dp_ofs = *ofs;
	if	(dp_scr == SCRIPTA_BA(np, pm0_data))
		pm = &cp->phys.pm0;
	else if (dp_scr == SCRIPTA_BA(np, pm1_data))
		pm = &cp->phys.pm1;
	else
		pm = NULL;

	if (pm) {
		dp_scr  = scr_to_cpu(pm->ret);
		dp_ofs -= scr_to_cpu(pm->sg.size) & 0x00ffffff;
	}

	if (cp->host_flags & HF_SENSE) {
		*ofs = dp_ofs;
		return 0;
	}

	tmp = scr_to_cpu(cp->goalp);
	dp_sg = SYM_CONF_MAX_SG;
	if (dp_scr != tmp)
		dp_sg -= (tmp - 8 - (int)dp_scr) / (2*4);
	dp_sgmin = SYM_CONF_MAX_SG - cp->segments;

	if (dp_ofs < 0) {
		int n;
		while (dp_sg > dp_sgmin) {
			--dp_sg;
			tmp = scr_to_cpu(cp->phys.data[dp_sg].size);
			n = dp_ofs + (tmp & 0xffffff);
			if (n > 0) {
				++dp_sg;
				break;
			}
			dp_ofs = n;
		}
	}
	else if (dp_ofs > 0) {
		while (dp_sg < SYM_CONF_MAX_SG) {
			tmp = scr_to_cpu(cp->phys.data[dp_sg].size);
			dp_ofs -= (tmp & 0xffffff);
			++dp_sg;
			if (dp_ofs <= 0)
				break;
		}
	}

	if	(dp_sg < dp_sgmin || (dp_sg == dp_sgmin && dp_ofs < 0))
		goto out_err;
	else if	(dp_sg > SYM_CONF_MAX_SG ||
		 (dp_sg == SYM_CONF_MAX_SG && dp_ofs > 0))
		goto out_err;

	if (dp_sg > cp->ext_sg ||
            (dp_sg == cp->ext_sg && dp_ofs > cp->ext_ofs)) {
		cp->ext_sg  = dp_sg;
		cp->ext_ofs = dp_ofs;
	}

	*ofs = dp_ofs;
	return dp_sg;

out_err:
	return -1;
}


static void sym_modify_dp(struct sym_hcb *np, struct sym_tcb *tp, struct sym_ccb *cp, int ofs)
{
	int dp_ofs	= ofs;
	u32	dp_scr	= sym_get_script_dp (np, cp);
	u32	dp_ret;
	u32	tmp;
	u_char	hflags;
	int	dp_sg;
	struct	sym_pmc *pm;

	if (cp->host_flags & HF_SENSE)
		goto out_reject;

	dp_sg = sym_evaluate_dp(np, cp, dp_scr, &dp_ofs);
	if (dp_sg < 0)
		goto out_reject;

	dp_ret = cpu_to_scr(cp->goalp);
	dp_ret = dp_ret - 8 - (SYM_CONF_MAX_SG - dp_sg) * (2*4);

	if (dp_ofs == 0) {
		dp_scr = dp_ret;
		goto out_ok;
	}

	hflags = INB(np, HF_PRT);

	if (hflags & HF_DP_SAVED)
		hflags ^= HF_ACT_PM;

	if (!(hflags & HF_ACT_PM)) {
		pm  = &cp->phys.pm0;
		dp_scr = SCRIPTA_BA(np, pm0_data);
	}
	else {
		pm = &cp->phys.pm1;
		dp_scr = SCRIPTA_BA(np, pm1_data);
	}

	hflags &= ~(HF_DP_SAVED);

	OUTB(np, HF_PRT, hflags);

	pm->ret = cpu_to_scr(dp_ret);
	tmp  = scr_to_cpu(cp->phys.data[dp_sg-1].addr);
	tmp += scr_to_cpu(cp->phys.data[dp_sg-1].size) + dp_ofs;
	pm->sg.addr = cpu_to_scr(tmp);
	pm->sg.size = cpu_to_scr(-dp_ofs);

out_ok:
	sym_set_script_dp (np, cp, dp_scr);
	OUTL_DSP(np, SCRIPTA_BA(np, clrack));
	return;

out_reject:
	OUTL_DSP(np, SCRIPTB_BA(np, msg_bad));
}



int sym_compute_residual(struct sym_hcb *np, struct sym_ccb *cp)
{
	int dp_sg, dp_sgmin, resid = 0;
	int dp_ofs = 0;

	if (cp->xerr_status & (XE_EXTRA_DATA|XE_SODL_UNRUN|XE_SWIDE_OVRUN)) {
		if (cp->xerr_status & XE_EXTRA_DATA)
			resid -= cp->extra_bytes;
		if (cp->xerr_status & XE_SODL_UNRUN)
			++resid;
		if (cp->xerr_status & XE_SWIDE_OVRUN)
			--resid;
	}

	if (cp->phys.head.lastp == cp->goalp)
		return resid;

	if (cp->startp == cp->phys.head.lastp ||
	    sym_evaluate_dp(np, cp, scr_to_cpu(cp->phys.head.lastp),
			    &dp_ofs) < 0) {
		return cp->data_len - cp->odd_byte_adjustment;
	}

	if (cp->host_flags & HF_SENSE) {
		return -dp_ofs;
	}

	dp_sgmin = SYM_CONF_MAX_SG - cp->segments;
	resid = -cp->ext_ofs;
	for (dp_sg = cp->ext_sg; dp_sg < SYM_CONF_MAX_SG; ++dp_sg) {
		u_int tmp = scr_to_cpu(cp->phys.data[dp_sg].size);
		resid += (tmp & 0xffffff);
	}

	resid -= cp->odd_byte_adjustment;

	return resid;
}


static int  
sym_sync_nego_check(struct sym_hcb *np, int req, struct sym_ccb *cp)
{
	int target = cp->target;
	u_char	chg, ofs, per, fak, div;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_nego_msg(np, target, "sync msgin", np->msgin);
	}

	chg = 0;
	per = np->msgin[3];
	ofs = np->msgin[4];

	if (ofs) {
		if (ofs > np->maxoffs)
			{chg = 1; ofs = np->maxoffs;}
	}

	if (ofs) {
		if (per < np->minsync)
			{chg = 1; per = np->minsync;}
	}

	div = fak = 0;
	if (ofs && sym_getsync(np, 0, per, &div, &fak) < 0)
		goto reject_it;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_addr(cp->cmd,
				"sdtr: ofs=%d per=%d div=%d fak=%d chg=%d.\n",
				ofs, per, div, fak, chg);
	}

	if (!req && chg)
		goto reject_it;

	sym_setsync (np, target, ofs, per, div, fak);

	if (!req)
		return 0;

	spi_populate_sync_msg(np->msgout, per, ofs);

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_nego_msg(np, target, "sync msgout", np->msgout);
	}

	np->msgin [0] = M_NOOP;

	return 0;

reject_it:
	sym_setsync (np, target, 0, 0, 0, 0);
	return -1;
}

static void sym_sync_nego(struct sym_hcb *np, struct sym_tcb *tp, struct sym_ccb *cp)
{
	int req = 1;
	int result;

	if (INB(np, HS_PRT) == HS_NEGOTIATE) {
		OUTB(np, HS_PRT, HS_BUSY);
		if (cp->nego_status && cp->nego_status != NS_SYNC)
			goto reject_it;
		req = 0;
	}

	result = sym_sync_nego_check(np, req, cp);
	if (result)	
		goto reject_it;
	if (req) {	
		cp->nego_status = NS_SYNC;
		OUTL_DSP(np, SCRIPTB_BA(np, sdtr_resp));
	}
	else		
		OUTL_DSP(np, SCRIPTA_BA(np, clrack));
	return;

reject_it:
	OUTL_DSP(np, SCRIPTB_BA(np, msg_bad));
}

static int 
sym_ppr_nego_check(struct sym_hcb *np, int req, int target)
{
	struct sym_tcb *tp = &np->target[target];
	unsigned char fak, div;
	int dt, chg = 0;

	unsigned char per = np->msgin[3];
	unsigned char ofs = np->msgin[5];
	unsigned char wide = np->msgin[6];
	unsigned char opts = np->msgin[7] & PPR_OPT_MASK;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_nego_msg(np, target, "ppr msgin", np->msgin);
	}

	if (wide > np->maxwide) {
		chg = 1;
		wide = np->maxwide;
	}
	if (!wide || !(np->features & FE_U3EN))
		opts = 0;

	if (opts != (np->msgin[7] & PPR_OPT_MASK))
		chg = 1;

	dt = opts & PPR_OPT_DT;

	if (ofs) {
		unsigned char maxoffs = dt ? np->maxoffs_dt : np->maxoffs;
		if (ofs > maxoffs) {
			chg = 1;
			ofs = maxoffs;
		}
	}

	if (ofs) {
		unsigned char minsync = dt ? np->minsync_dt : np->minsync;
		if (per < minsync) {
			chg = 1;
			per = minsync;
		}
	}

	div = fak = 0;
	if (ofs && sym_getsync(np, dt, per, &div, &fak) < 0)
		goto reject_it;

	if (!req && chg)
		goto reject_it;

	sym_setpprot(np, target, opts, ofs, per, wide, div, fak);

	if (!req)
		return 0;

	spi_populate_ppr_msg(np->msgout, per, ofs, wide, opts);

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_nego_msg(np, target, "ppr msgout", np->msgout);
	}

	np->msgin [0] = M_NOOP;

	return 0;

reject_it:
	sym_setpprot (np, target, 0, 0, 0, 0, 0, 0);
	if (!req && !opts) {
		tp->tgoal.period = per;
		tp->tgoal.offset = ofs;
		tp->tgoal.width = wide;
		tp->tgoal.iu = tp->tgoal.dt = tp->tgoal.qas = 0;
		tp->tgoal.check_nego = 1;
	}
	return -1;
}

static void sym_ppr_nego(struct sym_hcb *np, struct sym_tcb *tp, struct sym_ccb *cp)
{
	int req = 1;
	int result;

	if (INB(np, HS_PRT) == HS_NEGOTIATE) {
		OUTB(np, HS_PRT, HS_BUSY);
		if (cp->nego_status && cp->nego_status != NS_PPR)
			goto reject_it;
		req = 0;
	}

	result = sym_ppr_nego_check(np, req, cp->target);
	if (result)	
		goto reject_it;
	if (req) {	
		cp->nego_status = NS_PPR;
		OUTL_DSP(np, SCRIPTB_BA(np, ppr_resp));
	}
	else		
		OUTL_DSP(np, SCRIPTA_BA(np, clrack));
	return;

reject_it:
	OUTL_DSP(np, SCRIPTB_BA(np, msg_bad));
}

static int  
sym_wide_nego_check(struct sym_hcb *np, int req, struct sym_ccb *cp)
{
	int target = cp->target;
	u_char	chg, wide;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_nego_msg(np, target, "wide msgin", np->msgin);
	}

	chg  = 0;
	wide = np->msgin[3];

	if (wide > np->maxwide) {
		chg = 1;
		wide = np->maxwide;
	}

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_addr(cp->cmd, "wdtr: wide=%d chg=%d.\n",
				wide, chg);
	}

	if (!req && chg)
		goto reject_it;

	sym_setwide (np, target, wide);

	if (!req)
		return 0;

	spi_populate_width_msg(np->msgout, wide);

	np->msgin [0] = M_NOOP;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_nego_msg(np, target, "wide msgout", np->msgout);
	}

	return 0;

reject_it:
	return -1;
}

static void sym_wide_nego(struct sym_hcb *np, struct sym_tcb *tp, struct sym_ccb *cp)
{
	int req = 1;
	int result;

	if (INB(np, HS_PRT) == HS_NEGOTIATE) {
		OUTB(np, HS_PRT, HS_BUSY);
		if (cp->nego_status && cp->nego_status != NS_WIDE)
			goto reject_it;
		req = 0;
	}

	result = sym_wide_nego_check(np, req, cp);
	if (result)	
		goto reject_it;
	if (req) {	
		cp->nego_status = NS_WIDE;
		OUTL_DSP(np, SCRIPTB_BA(np, wdtr_resp));
	} else {		
		if (tp->tgoal.offset) {
			spi_populate_sync_msg(np->msgout, tp->tgoal.period,
					tp->tgoal.offset);

			if (DEBUG_FLAGS & DEBUG_NEGO) {
				sym_print_nego_msg(np, cp->target,
				                   "sync msgout", np->msgout);
			}

			cp->nego_status = NS_SYNC;
			OUTB(np, HS_PRT, HS_NEGOTIATE);
			OUTL_DSP(np, SCRIPTB_BA(np, sdtr_resp));
			return;
		} else
			OUTL_DSP(np, SCRIPTA_BA(np, clrack));
	}

	return;

reject_it:
	OUTL_DSP(np, SCRIPTB_BA(np, msg_bad));
}

static void sym_nego_default(struct sym_hcb *np, struct sym_tcb *tp, struct sym_ccb *cp)
{
	switch (cp->nego_status) {
	case NS_PPR:
#if 0
		sym_setpprot (np, cp->target, 0, 0, 0, 0, 0, 0);
#else
		if (tp->tgoal.period < np->minsync)
			tp->tgoal.period = np->minsync;
		if (tp->tgoal.offset > np->maxoffs)
			tp->tgoal.offset = np->maxoffs;
		tp->tgoal.iu = tp->tgoal.dt = tp->tgoal.qas = 0;
		tp->tgoal.check_nego = 1;
#endif
		break;
	case NS_SYNC:
		sym_setsync (np, cp->target, 0, 0, 0, 0);
		break;
	case NS_WIDE:
		sym_setwide (np, cp->target, 0);
		break;
	}
	np->msgin [0] = M_NOOP;
	np->msgout[0] = M_NOOP;
	cp->nego_status = 0;
}

static void sym_nego_rejected(struct sym_hcb *np, struct sym_tcb *tp, struct sym_ccb *cp)
{
	sym_nego_default(np, tp, cp);
	OUTB(np, HS_PRT, HS_BUSY);
}

static void sym_int_sir(struct sym_hcb *np)
{
	u_char	num	= INB(np, nc_dsps);
	u32	dsa	= INL(np, nc_dsa);
	struct sym_ccb *cp	= sym_ccb_from_dsa(np, dsa);
	u_char	target	= INB(np, nc_sdid) & 0x0f;
	struct sym_tcb *tp	= &np->target[target];
	int	tmp;

	if (DEBUG_FLAGS & DEBUG_TINY) printf ("I#%d", num);

	switch (num) {
#if   SYM_CONF_DMA_ADDRESSING_MODE == 2
	case SIR_DMAP_DIRTY:
		sym_update_dmap_regs(np);
		goto out;
#endif
	case SIR_COMPLETE_ERROR:
		sym_complete_error(np, cp);
		return;
	case SIR_SCRIPT_STOPPED:
	case SIR_TARGET_SELECTED:
	case SIR_ABORT_SENT:
		sym_sir_task_recovery(np, num);
		return;
	case SIR_SEL_ATN_NO_MSG_OUT:
		scmd_printk(KERN_WARNING, cp->cmd,
				"No MSG OUT phase after selection with ATN\n");
		goto out_stuck;
	case SIR_RESEL_NO_MSG_IN:
		scmd_printk(KERN_WARNING, cp->cmd,
				"No MSG IN phase after reselection\n");
		goto out_stuck;
	case SIR_RESEL_NO_IDENTIFY:
		scmd_printk(KERN_WARNING, cp->cmd,
				"No IDENTIFY after reselection\n");
		goto out_stuck;
	case SIR_RESEL_BAD_LUN:
		np->msgout[0] = M_RESET;
		goto out;
	case SIR_RESEL_BAD_I_T_L:
		np->msgout[0] = M_ABORT;
		goto out;
	case SIR_RESEL_BAD_I_T_L_Q:
		np->msgout[0] = M_ABORT_TAG;
		goto out;
	case SIR_RESEL_ABORTED:
		np->lastmsg = np->msgout[0];
		np->msgout[0] = M_NOOP;
		scmd_printk(KERN_WARNING, cp->cmd,
			"message %x sent on bad reselection\n", np->lastmsg);
		goto out;
	case SIR_MSG_OUT_DONE:
		np->lastmsg = np->msgout[0];
		np->msgout[0] = M_NOOP;
		
		if (np->lastmsg == M_PARITY || np->lastmsg == M_ID_ERROR) {
			if (cp) {
				cp->xerr_status &= ~XE_PARITY_ERR;
				if (!cp->xerr_status)
					OUTOFFB(np, HF_PRT, HF_EXT_ERR);
			}
		}
		goto out;
	case SIR_BAD_SCSI_STATUS:
		if (!cp)
			goto out;
		sym_sir_bad_scsi_status(np, num, cp);
		return;
	case SIR_REJECT_TO_SEND:
		sym_print_msg(cp, "M_REJECT to send for ", np->msgin);
		np->msgout[0] = M_REJECT;
		goto out;
	case SIR_SWIDE_OVERRUN:
		if (cp) {
			OUTONB(np, HF_PRT, HF_EXT_ERR);
			cp->xerr_status |= XE_SWIDE_OVRUN;
		}
		goto out;
	case SIR_SODL_UNDERRUN:
		if (cp) {
			OUTONB(np, HF_PRT, HF_EXT_ERR);
			cp->xerr_status |= XE_SODL_UNRUN;
		}
		goto out;
	case SIR_DATA_OVERRUN:
		if (cp) {
			OUTONB(np, HF_PRT, HF_EXT_ERR);
			cp->xerr_status |= XE_EXTRA_DATA;
			cp->extra_bytes += INL(np, nc_scratcha);
		}
		goto out;
	case SIR_BAD_PHASE:
		if (cp) {
			OUTONB(np, HF_PRT, HF_EXT_ERR);
			cp->xerr_status |= XE_BAD_PHASE;
		}
		goto out;
	case SIR_MSG_RECEIVED:
		if (!cp)
			goto out_stuck;
		switch (np->msgin [0]) {
		case M_EXTENDED:
			switch (np->msgin [2]) {
			case M_X_MODIFY_DP:
				if (DEBUG_FLAGS & DEBUG_POINTER)
					sym_print_msg(cp, "extended msg ",
						      np->msgin);
				tmp = (np->msgin[3]<<24) + (np->msgin[4]<<16) + 
				      (np->msgin[5]<<8)  + (np->msgin[6]);
				sym_modify_dp(np, tp, cp, tmp);
				return;
			case M_X_SYNC_REQ:
				sym_sync_nego(np, tp, cp);
				return;
			case M_X_PPR_REQ:
				sym_ppr_nego(np, tp, cp);
				return;
			case M_X_WIDE_REQ:
				sym_wide_nego(np, tp, cp);
				return;
			default:
				goto out_reject;
			}
			break;
		case M_IGN_RESIDUE:
			if (DEBUG_FLAGS & DEBUG_POINTER)
				sym_print_msg(cp, "1 or 2 byte ", np->msgin);
			if (cp->host_flags & HF_SENSE)
				OUTL_DSP(np, SCRIPTA_BA(np, clrack));
			else
				sym_modify_dp(np, tp, cp, -1);
			return;
		case M_REJECT:
			if (INB(np, HS_PRT) == HS_NEGOTIATE)
				sym_nego_rejected(np, tp, cp);
			else {
				sym_print_addr(cp->cmd,
					"M_REJECT received (%x:%x).\n",
					scr_to_cpu(np->lastmsg), np->msgout[0]);
			}
			goto out_clrack;
			break;
		default:
			goto out_reject;
		}
		break;
	case SIR_MSG_WEIRD:
		sym_print_msg(cp, "WEIRD message received", np->msgin);
		OUTL_DSP(np, SCRIPTB_BA(np, msg_weird));
		return;
	case SIR_NEGO_FAILED:
		OUTB(np, HS_PRT, HS_BUSY);
	case SIR_NEGO_PROTO:
		sym_nego_default(np, tp, cp);
		goto out;
	}

out:
	OUTONB_STD();
	return;
out_reject:
	OUTL_DSP(np, SCRIPTB_BA(np, msg_bad));
	return;
out_clrack:
	OUTL_DSP(np, SCRIPTA_BA(np, clrack));
	return;
out_stuck:
	return;
}

struct sym_ccb *sym_get_ccb (struct sym_hcb *np, struct scsi_cmnd *cmd, u_char tag_order)
{
	u_char tn = cmd->device->id;
	u_char ln = cmd->device->lun;
	struct sym_tcb *tp = &np->target[tn];
	struct sym_lcb *lp = sym_lp(tp, ln);
	u_short tag = NO_TAG;
	SYM_QUEHEAD *qp;
	struct sym_ccb *cp = NULL;

	if (sym_que_empty(&np->free_ccbq))
		sym_alloc_ccb(np);
	qp = sym_remque_head(&np->free_ccbq);
	if (!qp)
		goto out;
	cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);

	{
		if (tag_order) {
#ifndef SYM_OPT_HANDLE_DEVICE_QUEUEING
			if (lp->busy_itl != 0)
				goto out_free;
#endif
			if (!lp->cb_tags) {
				sym_alloc_lcb_tags(np, tn, ln);
				if (!lp->cb_tags)
					goto out_free;
			}
			if (lp->busy_itlq < SYM_CONF_MAX_TASK) {
				tag = lp->cb_tags[lp->ia_tag];
				if (++lp->ia_tag == SYM_CONF_MAX_TASK)
					lp->ia_tag = 0;
				++lp->busy_itlq;
#ifndef SYM_OPT_HANDLE_DEVICE_QUEUEING
				lp->itlq_tbl[tag] = cpu_to_scr(cp->ccb_ba);
				lp->head.resel_sa =
					cpu_to_scr(SCRIPTA_BA(np, resel_tag));
#endif
#ifdef SYM_OPT_LIMIT_COMMAND_REORDERING
				cp->tags_si = lp->tags_si;
				++lp->tags_sum[cp->tags_si];
				++lp->tags_since;
#endif
			}
			else
				goto out_free;
		}
		else {
#ifndef SYM_OPT_HANDLE_DEVICE_QUEUEING
			if (lp->busy_itl != 0 || lp->busy_itlq != 0)
				goto out_free;
#endif
			++lp->busy_itl;
#ifndef SYM_OPT_HANDLE_DEVICE_QUEUEING
			if (lp->busy_itl == 1) {
				lp->head.itl_task_sa = cpu_to_scr(cp->ccb_ba);
				lp->head.resel_sa =
				      cpu_to_scr(SCRIPTA_BA(np, resel_no_tag));
			}
			else
				goto out_free;
#endif
		}
	}
	sym_insque_tail(&cp->link_ccbq, &np->busy_ccbq);
#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	if (lp) {
		sym_remque(&cp->link2_ccbq);
		sym_insque_tail(&cp->link2_ccbq, &lp->waiting_ccbq);
	}

#endif
	cp->to_abort = 0;
	cp->odd_byte_adjustment = 0;
	cp->tag	   = tag;
	cp->order  = tag_order;
	cp->target = tn;
	cp->lun    = ln;

	if (DEBUG_FLAGS & DEBUG_TAGS) {
		sym_print_addr(cmd, "ccb @%p using tag %d.\n", cp, tag);
	}

out:
	return cp;
out_free:
	sym_insque_head(&cp->link_ccbq, &np->free_ccbq);
	return NULL;
}

void sym_free_ccb (struct sym_hcb *np, struct sym_ccb *cp)
{
	struct sym_tcb *tp = &np->target[cp->target];
	struct sym_lcb *lp = sym_lp(tp, cp->lun);

	if (DEBUG_FLAGS & DEBUG_TAGS) {
		sym_print_addr(cp->cmd, "ccb @%p freeing tag %d.\n",
				cp, cp->tag);
	}

	if (lp) {
		if (cp->tag != NO_TAG) {
#ifdef SYM_OPT_LIMIT_COMMAND_REORDERING
			--lp->tags_sum[cp->tags_si];
#endif
			lp->cb_tags[lp->if_tag] = cp->tag;
			if (++lp->if_tag == SYM_CONF_MAX_TASK)
				lp->if_tag = 0;
			lp->itlq_tbl[cp->tag] = cpu_to_scr(np->bad_itlq_ba);
			--lp->busy_itlq;
		} else {	
			lp->head.itl_task_sa = cpu_to_scr(np->bad_itl_ba);
			--lp->busy_itl;
		}
		if (lp->busy_itlq == 0 && lp->busy_itl == 0)
			lp->head.resel_sa =
				cpu_to_scr(SCRIPTB_BA(np, resel_bad_lun));
	}

	if (cp == tp->nego_cp)
		tp->nego_cp = NULL;

#ifdef SYM_CONF_IARB_SUPPORT
	if (cp == np->last_cp)
		np->last_cp = 0;
#endif

	cp->cmd = NULL;
	cp->host_status = HS_IDLE;
	sym_remque(&cp->link_ccbq);
	sym_insque_head(&cp->link_ccbq, &np->free_ccbq);

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	if (lp) {
		sym_remque(&cp->link2_ccbq);
		sym_insque_tail(&cp->link2_ccbq, &np->dummy_ccbq);
		if (cp->started) {
			if (cp->tag != NO_TAG)
				--lp->started_tags;
			else
				--lp->started_no_tag;
		}
	}
	cp->started = 0;
#endif
}

static struct sym_ccb *sym_alloc_ccb(struct sym_hcb *np)
{
	struct sym_ccb *cp = NULL;
	int hcode;

	if (np->actccbs >= SYM_CONF_MAX_START)
		return NULL;

	cp = sym_calloc_dma(sizeof(struct sym_ccb), "CCB");
	if (!cp)
		goto out_free;

	np->actccbs++;

	cp->ccb_ba = vtobus(cp);

	hcode = CCB_HASH_CODE(cp->ccb_ba);
	cp->link_ccbh = np->ccbh[hcode];
	np->ccbh[hcode] = cp;

	cp->phys.head.go.start   = cpu_to_scr(SCRIPTA_BA(np, idle));
	cp->phys.head.go.restart = cpu_to_scr(SCRIPTB_BA(np, bad_i_t_l));

	cp->phys.smsg_ext.addr = cpu_to_scr(HCB_BA(np, msgin[2]));

	sym_insque_head(&cp->link_ccbq, &np->free_ccbq);

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	sym_insque_head(&cp->link2_ccbq, &np->dummy_ccbq);
#endif
	return cp;
out_free:
	if (cp)
		sym_mfree_dma(cp, sizeof(*cp), "CCB");
	return NULL;
}

static struct sym_ccb *sym_ccb_from_dsa(struct sym_hcb *np, u32 dsa)
{
	int hcode;
	struct sym_ccb *cp;

	hcode = CCB_HASH_CODE(dsa);
	cp = np->ccbh[hcode];
	while (cp) {
		if (cp->ccb_ba == dsa)
			break;
		cp = cp->link_ccbh;
	}

	return cp;
}

static void sym_init_tcb (struct sym_hcb *np, u_char tn)
{
#if 0	
	
	assert (((offsetof(struct sym_reg, nc_sxfer) ^
		offsetof(struct sym_tcb, head.sval)) &3) == 0);
	assert (((offsetof(struct sym_reg, nc_scntl3) ^
		offsetof(struct sym_tcb, head.wval)) &3) == 0);
#endif
}

struct sym_lcb *sym_alloc_lcb (struct sym_hcb *np, u_char tn, u_char ln)
{
	struct sym_tcb *tp = &np->target[tn];
	struct sym_lcb *lp = NULL;

	sym_init_tcb (np, tn);

	if (ln && !tp->luntbl) {
		int i;

		tp->luntbl = sym_calloc_dma(256, "LUNTBL");
		if (!tp->luntbl)
			goto fail;
		for (i = 0 ; i < 64 ; i++)
			tp->luntbl[i] = cpu_to_scr(vtobus(&np->badlun_sa));
		tp->head.luntbl_sa = cpu_to_scr(vtobus(tp->luntbl));
	}

	if (ln && !tp->lunmp) {
		tp->lunmp = kcalloc(SYM_CONF_MAX_LUN, sizeof(struct sym_lcb *),
				GFP_ATOMIC);
		if (!tp->lunmp)
			goto fail;
	}

	lp = sym_calloc_dma(sizeof(struct sym_lcb), "LCB");
	if (!lp)
		goto fail;
	if (ln) {
		tp->lunmp[ln] = lp;
		tp->luntbl[ln] = cpu_to_scr(vtobus(lp));
	}
	else {
		tp->lun0p = lp;
		tp->head.lun0_sa = cpu_to_scr(vtobus(lp));
	}
	tp->nlcb++;

	lp->head.itl_task_sa = cpu_to_scr(np->bad_itl_ba);

	lp->head.resel_sa = cpu_to_scr(SCRIPTB_BA(np, resel_bad_lun));

	lp->user_flags = tp->usrflags & (SYM_DISC_ENABLED | SYM_TAGS_ENABLED);

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	sym_que_init(&lp->waiting_ccbq);
	sym_que_init(&lp->started_ccbq);
	lp->started_max   = SYM_CONF_MAX_TASK;
	lp->started_limit = SYM_CONF_MAX_TASK;
#endif

fail:
	return lp;
}

static void sym_alloc_lcb_tags (struct sym_hcb *np, u_char tn, u_char ln)
{
	struct sym_tcb *tp = &np->target[tn];
	struct sym_lcb *lp = sym_lp(tp, ln);
	int i;

	lp->itlq_tbl = sym_calloc_dma(SYM_CONF_MAX_TASK*4, "ITLQ_TBL");
	if (!lp->itlq_tbl)
		goto fail;
	lp->cb_tags = kcalloc(SYM_CONF_MAX_TASK, 1, GFP_ATOMIC);
	if (!lp->cb_tags) {
		sym_mfree_dma(lp->itlq_tbl, SYM_CONF_MAX_TASK*4, "ITLQ_TBL");
		lp->itlq_tbl = NULL;
		goto fail;
	}

	for (i = 0 ; i < SYM_CONF_MAX_TASK ; i++)
		lp->itlq_tbl[i] = cpu_to_scr(np->notask_ba);

	for (i = 0 ; i < SYM_CONF_MAX_TASK ; i++)
		lp->cb_tags[i] = i;

	lp->head.itlq_tbl_sa = cpu_to_scr(vtobus(lp->itlq_tbl));

	return;
fail:
	return;
}

int sym_free_lcb(struct sym_hcb *np, u_char tn, u_char ln)
{
	struct sym_tcb *tp = &np->target[tn];
	struct sym_lcb *lp = sym_lp(tp, ln);

	tp->nlcb--;

	if (ln) {
		if (!tp->nlcb) {
			kfree(tp->lunmp);
			sym_mfree_dma(tp->luntbl, 256, "LUNTBL");
			tp->lunmp = NULL;
			tp->luntbl = NULL;
			tp->head.luntbl_sa = cpu_to_scr(vtobus(np->badluntbl));
		} else {
			tp->luntbl[ln] = cpu_to_scr(vtobus(&np->badlun_sa));
			tp->lunmp[ln] = NULL;
		}
	} else {
		tp->lun0p = NULL;
		tp->head.lun0_sa = cpu_to_scr(vtobus(&np->badlun_sa));
	}

	if (lp->itlq_tbl) {
		sym_mfree_dma(lp->itlq_tbl, SYM_CONF_MAX_TASK*4, "ITLQ_TBL");
		kfree(lp->cb_tags);
	}

	sym_mfree_dma(lp, sizeof(*lp), "LCB");

	return tp->nlcb;
}

int sym_queue_scsiio(struct sym_hcb *np, struct scsi_cmnd *cmd, struct sym_ccb *cp)
{
	struct scsi_device *sdev = cmd->device;
	struct sym_tcb *tp;
	struct sym_lcb *lp;
	u_char	*msgptr;
	u_int   msglen;
	int can_disconnect;

	cp->cmd = cmd;

	tp = &np->target[cp->target];

	lp = sym_lp(tp, sdev->lun);

	can_disconnect = (cp->tag != NO_TAG) ||
		(lp && (lp->curr_flags & SYM_DISC_ENABLED));

	msgptr = cp->scsi_smsg;
	msglen = 0;
	msgptr[msglen++] = IDENTIFY(can_disconnect, sdev->lun);

	if (cp->tag != NO_TAG) {
		u_char order = cp->order;

		switch(order) {
		case M_ORDERED_TAG:
			break;
		case M_HEAD_TAG:
			break;
		default:
			order = M_SIMPLE_TAG;
		}
#ifdef SYM_OPT_LIMIT_COMMAND_REORDERING
		if (lp && lp->tags_since > 3*SYM_CONF_MAX_TAG) {
			lp->tags_si = !(lp->tags_si);
			if (lp->tags_sum[lp->tags_si]) {
				order = M_ORDERED_TAG;
				if ((DEBUG_FLAGS & DEBUG_TAGS)||sym_verbose>1) {
					sym_print_addr(cmd,
						"ordered tag forced.\n");
				}
			}
			lp->tags_since = 0;
		}
#endif
		msgptr[msglen++] = order;

#if SYM_CONF_MAX_TASK > (512/4)
		msgptr[msglen++] = cp->tag;
#else
		msgptr[msglen++] = (cp->tag << 1) + 1;
#endif
	}

	cp->nego_status = 0;
	if ((tp->tgoal.check_nego ||
	     cmd->cmnd[0] == INQUIRY || cmd->cmnd[0] == REQUEST_SENSE) &&
	    !tp->nego_cp && lp) {
		msglen += sym_prepare_nego(np, cp, msgptr + msglen);
	}

	cp->phys.head.go.start   = cpu_to_scr(SCRIPTA_BA(np, select));
	cp->phys.head.go.restart = cpu_to_scr(SCRIPTA_BA(np, resel_dsa));

	cp->phys.select.sel_id		= cp->target;
	cp->phys.select.sel_scntl3	= tp->head.wval;
	cp->phys.select.sel_sxfer	= tp->head.sval;
	cp->phys.select.sel_scntl4	= tp->head.uval;

	cp->phys.smsg.addr	= CCB_BA(cp, scsi_smsg);
	cp->phys.smsg.size	= cpu_to_scr(msglen);

	cp->host_xflags		= 0;
	cp->host_status		= cp->nego_status ? HS_NEGOTIATE : HS_BUSY;
	cp->ssss_status		= S_ILLEGAL;
	cp->xerr_status		= 0;
	cp->host_flags		= 0;
	cp->extra_bytes		= 0;

	cp->ext_sg  = -1;
	cp->ext_ofs = 0;

	return sym_setup_data_and_start(np, cmd, cp);
}

int sym_reset_scsi_target(struct sym_hcb *np, int target)
{
	struct sym_tcb *tp;

	if (target == np->myaddr || (u_int)target >= SYM_CONF_MAX_TARGET)
		return -1;

	tp = &np->target[target];
	tp->to_reset = 1;

	np->istat_sem = SEM;
	OUTB(np, nc_istat, SIGP|SEM);

	return 0;
}

static int sym_abort_ccb(struct sym_hcb *np, struct sym_ccb *cp, int timed_out)
{
	if (!cp || !cp->host_status || cp->host_status == HS_WAIT)
		return -1;

	if (cp->to_abort) {
		sym_reset_scsi_bus(np, 1);
		return 0;
	}

	cp->to_abort = timed_out ? 2 : 1;

	np->istat_sem = SEM;
	OUTB(np, nc_istat, SIGP|SEM);
	return 0;
}

int sym_abort_scsiio(struct sym_hcb *np, struct scsi_cmnd *cmd, int timed_out)
{
	struct sym_ccb *cp;
	SYM_QUEHEAD *qp;

	cp = NULL;
	FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
		struct sym_ccb *cp2 = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		if (cp2->cmd == cmd) {
			cp = cp2;
			break;
		}
	}

	return sym_abort_ccb(np, cp, timed_out);
}

void sym_complete_error(struct sym_hcb *np, struct sym_ccb *cp)
{
	struct scsi_device *sdev;
	struct scsi_cmnd *cmd;
	struct sym_tcb *tp;
	struct sym_lcb *lp;
	int resid;
	int i;

	if (!cp || !cp->cmd)
		return;

	cmd = cp->cmd;
	sdev = cmd->device;
	if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_RESULT)) {
		dev_info(&sdev->sdev_gendev, "CCB=%p STAT=%x/%x/%x\n", cp,
			cp->host_status, cp->ssss_status, cp->host_flags);
	}

	tp = &np->target[cp->target];
	lp = sym_lp(tp, sdev->lun);

	if (cp->xerr_status) {
		if (sym_verbose)
			sym_print_xerr(cmd, cp->xerr_status);
		if (cp->host_status == HS_COMPLETE)
			cp->host_status = HS_COMP_ERR;
	}

	resid = sym_compute_residual(np, cp);

	if (!SYM_SETUP_RESIDUAL_SUPPORT) {
		resid  = 0;		 
		cp->sv_resid = 0;
	}
#ifdef DEBUG_2_0_X
if (resid)
	printf("XXXX RESID= %d - 0x%x\n", resid, resid);
#endif

	i = (INL(np, nc_scratcha) - np->squeue_ba) / 4;
	i = sym_dequeue_from_squeue(np, i, cp->target, sdev->lun, -1);

	OUTL_DSP(np, SCRIPTA_BA(np, start));

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	if (cp->host_status == HS_COMPLETE &&
	    cp->ssss_status == S_QUEUE_FULL) {
		if (!lp || lp->started_tags - i < 2)
			goto weirdness;
		lp->started_max = lp->started_tags - i - 1;
		lp->num_sgood = 0;

		if (sym_verbose >= 2) {
			sym_print_addr(cmd, " queue depth is now %d\n",
					lp->started_max);
		}

		cp->host_status = HS_BUSY;
		cp->ssss_status = S_ILLEGAL;

		sym_set_cam_status(cmd, DID_SOFT_ERROR);
		goto finish;
	}
weirdness:
#endif
	sym_set_cam_result_error(np, cp, resid);

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
finish:
#endif
	sym_remque(&cp->link_ccbq);
	sym_insque_head(&cp->link_ccbq, &np->comp_ccbq);

	sym_flush_comp_queue(np, 0);

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	sym_start_next_ccbs(np, lp, 1);
#endif
}

void sym_complete_ok (struct sym_hcb *np, struct sym_ccb *cp)
{
	struct sym_tcb *tp;
	struct sym_lcb *lp;
	struct scsi_cmnd *cmd;
	int resid;

	if (!cp || !cp->cmd)
		return;
	assert (cp->host_status == HS_COMPLETE);

	cmd = cp->cmd;

	tp = &np->target[cp->target];
	lp = sym_lp(tp, cp->lun);

	resid = 0;
	if (cp->phys.head.lastp != cp->goalp)
		resid = sym_compute_residual(np, cp);

	if (!SYM_SETUP_RESIDUAL_SUPPORT)
		resid  = 0;
#ifdef DEBUG_2_0_X
if (resid)
	printf("XXXX RESID= %d - 0x%x\n", resid, resid);
#endif

	sym_set_cam_result_ok(cp, cmd, resid);

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	if (lp && lp->started_max < lp->started_limit) {
		++lp->num_sgood;
		if (lp->num_sgood >= 200) {
			lp->num_sgood = 0;
			++lp->started_max;
			if (sym_verbose >= 2) {
				sym_print_addr(cmd, " queue depth is now %d\n",
				       lp->started_max);
			}
		}
	}
#endif

	sym_free_ccb (np, cp);

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	if (!sym_que_empty(&lp->waiting_ccbq))
		sym_start_next_ccbs(np, lp, 2);
#endif
	sym_xpt_done(np, cmd);
}

int sym_hcb_attach(struct Scsi_Host *shost, struct sym_fw *fw, struct sym_nvram *nvram)
{
	struct sym_hcb *np = sym_get_hcb(shost);
	int i;

	np->scripta_sz	 = fw->a_size;
	np->scriptb_sz	 = fw->b_size;
	np->scriptz_sz	 = fw->z_size;
	np->fw_setup	 = fw->setup;
	np->fw_patch	 = fw->patch;
	np->fw_name	 = fw->name;

	sym_save_initial_setting (np);

	sym_chip_reset(np);

	sym_prepare_setting(shost, np, nvram);

	i = sym_getpciclock(np);
	if (i > 37000 && !(np->features & FE_66MHZ))
		printf("%s: PCI BUS clock seems too high: %u KHz.\n",
			sym_name(np), i);

	np->squeue = sym_calloc_dma(sizeof(u32)*(MAX_QUEUE*2),"SQUEUE");
	if (!np->squeue)
		goto attach_failed;
	np->squeue_ba = vtobus(np->squeue);

	np->dqueue = sym_calloc_dma(sizeof(u32)*(MAX_QUEUE*2),"DQUEUE");
	if (!np->dqueue)
		goto attach_failed;
	np->dqueue_ba = vtobus(np->dqueue);

	np->targtbl = sym_calloc_dma(256, "TARGTBL");
	if (!np->targtbl)
		goto attach_failed;
	np->targtbl_ba = vtobus(np->targtbl);

	np->scripta0 = sym_calloc_dma(np->scripta_sz, "SCRIPTA0");
	np->scriptb0 = sym_calloc_dma(np->scriptb_sz, "SCRIPTB0");
	np->scriptz0 = sym_calloc_dma(np->scriptz_sz, "SCRIPTZ0");
	if (!np->scripta0 || !np->scriptb0 || !np->scriptz0)
		goto attach_failed;

	np->ccbh = kcalloc(CCB_HASH_SIZE, sizeof(struct sym_ccb **), GFP_KERNEL);
	if (!np->ccbh)
		goto attach_failed;

	sym_que_init(&np->free_ccbq);
	sym_que_init(&np->busy_ccbq);
	sym_que_init(&np->comp_ccbq);

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	sym_que_init(&np->dummy_ccbq);
#endif
	if (!sym_alloc_ccb(np))
		goto attach_failed;

	np->scripta_ba	= vtobus(np->scripta0);
	np->scriptb_ba	= vtobus(np->scriptb0);
	np->scriptz_ba	= vtobus(np->scriptz0);

	if (np->ram_ba) {
		np->scripta_ba = np->ram_ba;
		if (np->features & FE_RAM8K) {
			np->scriptb_ba = np->scripta_ba + 4096;
#if 0	
			np->scr_ram_seg = cpu_to_scr(np->scripta_ba >> 32);
#endif
		}
	}

	memcpy(np->scripta0, fw->a_base, np->scripta_sz);
	memcpy(np->scriptb0, fw->b_base, np->scriptb_sz);
	memcpy(np->scriptz0, fw->z_base, np->scriptz_sz);

	np->fw_setup(np, fw);

	sym_fw_bind_script(np, (u32 *) np->scripta0, np->scripta_sz);
	sym_fw_bind_script(np, (u32 *) np->scriptb0, np->scriptb_sz);
	sym_fw_bind_script(np, (u32 *) np->scriptz0, np->scriptz_sz);

#ifdef SYM_CONF_IARB_SUPPORT
#ifdef	SYM_SETUP_IARB_MAX
	np->iarb_max = SYM_SETUP_IARB_MAX;
#else
	np->iarb_max = 4;
#endif
#endif

	np->idletask.start	= cpu_to_scr(SCRIPTA_BA(np, idle));
	np->idletask.restart	= cpu_to_scr(SCRIPTB_BA(np, bad_i_t_l));
	np->idletask_ba		= vtobus(&np->idletask);

	np->notask.start	= cpu_to_scr(SCRIPTA_BA(np, idle));
	np->notask.restart	= cpu_to_scr(SCRIPTB_BA(np, bad_i_t_l));
	np->notask_ba		= vtobus(&np->notask);

	np->bad_itl.start	= cpu_to_scr(SCRIPTA_BA(np, idle));
	np->bad_itl.restart	= cpu_to_scr(SCRIPTB_BA(np, bad_i_t_l));
	np->bad_itl_ba		= vtobus(&np->bad_itl);

	np->bad_itlq.start	= cpu_to_scr(SCRIPTA_BA(np, idle));
	np->bad_itlq.restart	= cpu_to_scr(SCRIPTB_BA(np,bad_i_t_l_q));
	np->bad_itlq_ba		= vtobus(&np->bad_itlq);

	np->badluntbl = sym_calloc_dma(256, "BADLUNTBL");
	if (!np->badluntbl)
		goto attach_failed;

	np->badlun_sa = cpu_to_scr(SCRIPTB_BA(np, resel_bad_lun));
	for (i = 0 ; i < 64 ; i++)	
		np->badluntbl[i] = cpu_to_scr(vtobus(&np->badlun_sa));

	for (i = 0 ; i < SYM_CONF_MAX_TARGET ; i++) {
		np->targtbl[i] = cpu_to_scr(vtobus(&np->target[i]));
		np->target[i].head.luntbl_sa =
				cpu_to_scr(vtobus(np->badluntbl));
		np->target[i].head.lun0_sa =
				cpu_to_scr(vtobus(&np->badlun_sa));
	}

	if (sym_snooptest (np)) {
		printf("%s: CACHE INCORRECTLY CONFIGURED.\n", sym_name(np));
		goto attach_failed;
	}

	return 0;

attach_failed:
	return -ENXIO;
}

void sym_hcb_free(struct sym_hcb *np)
{
	SYM_QUEHEAD *qp;
	struct sym_ccb *cp;
	struct sym_tcb *tp;
	int target;

	if (np->scriptz0)
		sym_mfree_dma(np->scriptz0, np->scriptz_sz, "SCRIPTZ0");
	if (np->scriptb0)
		sym_mfree_dma(np->scriptb0, np->scriptb_sz, "SCRIPTB0");
	if (np->scripta0)
		sym_mfree_dma(np->scripta0, np->scripta_sz, "SCRIPTA0");
	if (np->squeue)
		sym_mfree_dma(np->squeue, sizeof(u32)*(MAX_QUEUE*2), "SQUEUE");
	if (np->dqueue)
		sym_mfree_dma(np->dqueue, sizeof(u32)*(MAX_QUEUE*2), "DQUEUE");

	if (np->actccbs) {
		while ((qp = sym_remque_head(&np->free_ccbq)) != NULL) {
			cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
			sym_mfree_dma(cp, sizeof(*cp), "CCB");
		}
	}
	kfree(np->ccbh);

	if (np->badluntbl)
		sym_mfree_dma(np->badluntbl, 256,"BADLUNTBL");

	for (target = 0; target < SYM_CONF_MAX_TARGET ; target++) {
		tp = &np->target[target];
		if (tp->luntbl)
			sym_mfree_dma(tp->luntbl, 256, "LUNTBL");
#if SYM_CONF_MAX_LUN > 1
		kfree(tp->lunmp);
#endif 
	}
	if (np->targtbl)
		sym_mfree_dma(np->targtbl, 256, "TARGTBL");
}
