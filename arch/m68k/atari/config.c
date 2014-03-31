/*
 *  linux/arch/m68k/atari/config.c
 *
 *  Copyright (C) 1994 Bjoern Brauel
 *
 *  5/2/94 Roman Hodek:
 *    Added setting of time_adj to get a better clock.
 *
 *  5/14/94 Roman Hodek:
 *    gettod() for TT
 *
 *  5/15/94 Roman Hodek:
 *    hard_reset_now() for Atari (and others?)
 *
 *  94/12/30 Andreas Schwab:
 *    atari_sched_init fixed to get precise clock.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */


#include <linux/types.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/vt_kern.h>
#include <linux/module.h>

#include <asm/bootinfo.h>
#include <asm/setup.h>
#include <asm/atarihw.h>
#include <asm/atariints.h>
#include <asm/atari_stram.h>
#include <asm/machdep.h>
#include <asm/hwtest.h>
#include <asm/io.h>

u_long atari_mch_cookie;
EXPORT_SYMBOL(atari_mch_cookie);

u_long atari_mch_type;
EXPORT_SYMBOL(atari_mch_type);

struct atari_hw_present atari_hw_present;
EXPORT_SYMBOL(atari_hw_present);

u_long atari_switches;
EXPORT_SYMBOL(atari_switches);

int atari_dont_touch_floppy_select;
EXPORT_SYMBOL(atari_dont_touch_floppy_select);

int atari_rtc_year_offset;

static void atari_reset(void);
static void atari_get_model(char *model);
static void atari_get_hardware_list(struct seq_file *m);

extern void atari_init_IRQ (void);
extern void atari_mksound(unsigned int count, unsigned int ticks);
#ifdef CONFIG_HEARTBEAT
static void atari_heartbeat(int on);
#endif

extern void atari_sched_init(irq_handler_t);
extern unsigned long atari_gettimeoffset (void);
extern int atari_mste_hwclk (int, struct rtc_time *);
extern int atari_tt_hwclk (int, struct rtc_time *);
extern int atari_mste_set_clock_mmss (unsigned long);
extern int atari_tt_set_clock_mmss (unsigned long);



static int __init scc_test(volatile char *ctla)
{
	if (!hwreg_present(ctla))
		return 0;
	MFPDELAY();

	*ctla = 2;
	MFPDELAY();
	*ctla = 0x40;
	MFPDELAY();

	*ctla = 2;
	MFPDELAY();
	if (*ctla != 0x40)
		return 0;
	MFPDELAY();

	*ctla = 2;
	MFPDELAY();
	*ctla = 0x60;
	MFPDELAY();

	*ctla = 2;
	MFPDELAY();
	if (*ctla != 0x60)
		return 0;

	return 1;
}



int __init atari_parse_bootinfo(const struct bi_record *record)
{
	int unknown = 0;
	const u_long *data = record->data;

	switch (record->tag) {
	case BI_ATARI_MCH_COOKIE:
		atari_mch_cookie = *data;
		break;
	case BI_ATARI_MCH_TYPE:
		atari_mch_type = *data;
		break;
	default:
		unknown = 1;
		break;
	}
	return unknown;
}


static int __init atari_switches_setup(char *str)
{
	char switches[strlen(str) + 1];
	char *p;
	int ovsc_shift;
	char *args = switches;

	if (!MACH_IS_ATARI)
		return 0;

	
	strcpy(switches, str);
	atari_switches = 0;

	
	while ((p = strsep(&args, ",")) != NULL) {
		if (!*p)
			continue;
		ovsc_shift = 0;
		if (strncmp(p, "ov_", 3) == 0) {
			p += 3;
			ovsc_shift = ATARI_SWITCH_OVSC_SHIFT;
		}

		if (strcmp(p, "ikbd") == 0) {
			
			atari_switches |= ATARI_SWITCH_IKBD << ovsc_shift;
		} else if (strcmp(p, "midi") == 0) {
			
			atari_switches |= ATARI_SWITCH_MIDI << ovsc_shift;
		} else if (strcmp(p, "snd6") == 0) {
			atari_switches |= ATARI_SWITCH_SND6 << ovsc_shift;
		} else if (strcmp(p, "snd7") == 0) {
			atari_switches |= ATARI_SWITCH_SND7 << ovsc_shift;
		}
	}
	return 0;
}

early_param("switches", atari_switches_setup);



void __init config_atari(void)
{
	unsigned short tos_version;

	memset(&atari_hw_present, 0, sizeof(atari_hw_present));

	
	ioport_resource.end  = 0xFFFFFFFF;

	mach_sched_init      = atari_sched_init;
	mach_init_IRQ        = atari_init_IRQ;
	mach_get_model	 = atari_get_model;
	mach_get_hardware_list = atari_get_hardware_list;
	mach_gettimeoffset   = atari_gettimeoffset;
	mach_reset           = atari_reset;
	mach_max_dma_address = 0xffffff;
#if defined(CONFIG_INPUT_M68K_BEEP) || defined(CONFIG_INPUT_M68K_BEEP_MODULE)
	mach_beep          = atari_mksound;
#endif
#ifdef CONFIG_HEARTBEAT
	mach_heartbeat = atari_heartbeat;
#endif

	
	if (atari_switches & ATARI_SWITCH_IKBD)
		acia.key_ctrl = ACIA_DIV64 | ACIA_D8N1S | ACIA_RHTID;
	if (atari_switches & ATARI_SWITCH_MIDI)
		acia.mid_ctrl = ACIA_DIV16 | ACIA_D8N1S | ACIA_RHTID;
	if (atari_switches & (ATARI_SWITCH_SND6|ATARI_SWITCH_SND7)) {
		sound_ym.rd_data_reg_sel = 14;
		sound_ym.wd_data = sound_ym.rd_data_reg_sel |
				   ((atari_switches&ATARI_SWITCH_SND6) ? 0x40 : 0) |
				   ((atari_switches&ATARI_SWITCH_SND7) ? 0x80 : 0);
	}


	printk("Atari hardware found: ");
	if (MACH_IS_MEDUSA) {
	} else if (hwreg_present(f030_xreg)) {
		ATARIHW_SET(VIDEL_SHIFTER);
		printk("VIDEL ");
		ATARIHW_SET(ST_SCSI);
		printk("STDMA-SCSI ");
	} else if (hwreg_present(tt_palette)) {
		ATARIHW_SET(TT_SHIFTER);
		printk("TT_SHIFTER ");
	} else if (hwreg_present(&shifter.bas_hi)) {
		if (hwreg_present(&shifter.bas_lo) &&
		    (shifter.bas_lo = 0x0aau, shifter.bas_lo == 0x0aau)) {
			ATARIHW_SET(EXTD_SHIFTER);
			printk("EXTD_SHIFTER ");
		} else {
			ATARIHW_SET(STND_SHIFTER);
			printk("STND_SHIFTER ");
		}
	}
	if (hwreg_present(&st_mfp.par_dt_reg)) {
		ATARIHW_SET(ST_MFP);
		printk("ST_MFP ");
	}
	if (hwreg_present(&tt_mfp.par_dt_reg)) {
		ATARIHW_SET(TT_MFP);
		printk("TT_MFP ");
	}
	if (hwreg_present(&tt_scsi_dma.dma_addr_hi)) {
		ATARIHW_SET(SCSI_DMA);
		printk("TT_SCSI_DMA ");
	}
	if (MACH_IS_MEDUSA ||
	    (hwreg_present(&st_dma.dma_vhi) &&
	     (st_dma.dma_vhi = 0x55) && (st_dma.dma_hi = 0xaa) &&
	     st_dma.dma_vhi == 0x55 && st_dma.dma_hi == 0xaa &&
	     (st_dma.dma_vhi = 0xaa) && (st_dma.dma_hi = 0x55) &&
	     st_dma.dma_vhi == 0xaa && st_dma.dma_hi == 0x55)) {
		ATARIHW_SET(EXTD_DMA);
		printk("EXTD_DMA ");
	}
	if (hwreg_present(&tt_scsi.scsi_data)) {
		ATARIHW_SET(TT_SCSI);
		printk("TT_SCSI ");
	}
	if (hwreg_present(&sound_ym.rd_data_reg_sel)) {
		ATARIHW_SET(YM_2149);
		printk("YM2149 ");
	}
	if (!MACH_IS_MEDUSA && hwreg_present(&tt_dmasnd.ctrl)) {
		ATARIHW_SET(PCM_8BIT);
		printk("PCM ");
	}
	if (hwreg_present(&falcon_codec.unused5)) {
		ATARIHW_SET(CODEC);
		printk("CODEC ");
	}
	if (hwreg_present(&dsp56k_host_interface.icr)) {
		ATARIHW_SET(DSP56K);
		printk("DSP56K ");
	}
	if (hwreg_present(&tt_scc_dma.dma_ctrl) &&
#if 0
	    
	    (tt_scc_dma.dma_ctrl = 0x01, (tt_scc_dma.dma_ctrl & 1) == 1) &&
	    (tt_scc_dma.dma_ctrl = 0x00, (tt_scc_dma.dma_ctrl & 1) == 0)
#else
	    !MACH_IS_MEDUSA
#endif
	    ) {
		ATARIHW_SET(SCC_DMA);
		printk("SCC_DMA ");
	}
	if (scc_test(&atari_scc.cha_a_ctrl)) {
		ATARIHW_SET(SCC);
		printk("SCC ");
	}
	if (scc_test(&st_escc.cha_b_ctrl)) {
		ATARIHW_SET(ST_ESCC);
		printk("ST_ESCC ");
	}
	if (hwreg_present(&tt_scu.sys_mask)) {
		ATARIHW_SET(SCU);
		
		ATARIHW_SET(VME);
		printk("VME SCU ");
	}
	if (hwreg_present((void *)(0xffff9210))) {
		ATARIHW_SET(ANALOG_JOY);
		printk("ANALOG_JOY ");
	}
	if (hwreg_present(blitter.halftone)) {
		ATARIHW_SET(BLITTER);
		printk("BLITTER ");
	}
	if (hwreg_present((void *)0xfff00039)) {
		ATARIHW_SET(IDE);
		printk("IDE ");
	}
#if 1 
	if (!MACH_IS_MEDUSA && hwreg_present(&tt_microwire.data) &&
	    hwreg_present(&tt_microwire.mask) &&
	    (tt_microwire.mask = 0x7ff,
	     udelay(1),
	     tt_microwire.data = MW_LM1992_PSG_HIGH | MW_LM1992_ADDR,
	     udelay(1),
	     tt_microwire.data != 0)) {
		ATARIHW_SET(MICROWIRE);
		while (tt_microwire.mask != 0x7ff)
			;
		printk("MICROWIRE ");
	}
#endif
	if (hwreg_present(&tt_rtc.regsel)) {
		ATARIHW_SET(TT_CLK);
		printk("TT_CLK ");
		mach_hwclk = atari_tt_hwclk;
		mach_set_clock_mmss = atari_tt_set_clock_mmss;
	}
	if (hwreg_present(&mste_rtc.sec_ones)) {
		ATARIHW_SET(MSTE_CLK);
		printk("MSTE_CLK ");
		mach_hwclk = atari_mste_hwclk;
		mach_set_clock_mmss = atari_mste_set_clock_mmss;
	}
	if (!MACH_IS_MEDUSA && hwreg_present(&dma_wd.fdc_speed) &&
	    hwreg_write(&dma_wd.fdc_speed, 0)) {
		ATARIHW_SET(FDCSPEED);
		printk("FDC_SPEED ");
	}
	if (!ATARIHW_PRESENT(ST_SCSI)) {
		ATARIHW_SET(ACSI);
		printk("ACSI ");
	}
	printk("\n");

	if (CPU_IS_040_OR_060)
		asm volatile ("\n"
			"	moveq	#0,%%d0\n"
			"	.chip	68040\n"
			"	movec	%%d0,%%itt0\n"
			"	movec	%%d0,%%dtt0\n"
			"	.chip	68k"
			: 
			: 
			: "d0");

	
	atari_stram_init();


	if (CPU_IS_020_OR_030) {
		unsigned long tt1_val;
		tt1_val = 0xfe008543;	
		asm volatile ("\n"
			"	.chip	68030\n"
			"	pmove	%0,%/tt1\n"
			"	.chip	68k"
			: : "m" (tt1_val));
	} else {
	        asm volatile ("\n"
			"	.chip	68040\n"
			"	movec	%0,%%itt1\n"
			"	movec	%0,%%dtt1\n"
			"	.chip	68k"
			:
			: "d" (0xfe00a040));	

	}

	
	tos_version = (MACH_IS_MEDUSA) ?
			0xfff : *(unsigned short *)0xff000002;
	atari_rtc_year_offset = (tos_version < 0x306) ? 70 : 68;
}

#ifdef CONFIG_HEARTBEAT
static void atari_heartbeat(int on)
{
	unsigned char tmp;
	unsigned long flags;

	if (atari_dont_touch_floppy_select)
		return;

	local_irq_save(flags);
	sound_ym.rd_data_reg_sel = 14;	
	tmp = sound_ym.rd_data_reg_sel;
	sound_ym.wd_data = on ? (tmp & ~0x02) : (tmp | 0x02);
	local_irq_restore(flags);
}
#endif



static void atari_reset(void)
{
	long tc_val = 0;
	long reset_addr;

	reset_addr = MACH_IS_MEDUSA || MACH_IS_AB40 ? 0xe00030 :
		     *(unsigned long *) 0xff000004;

	
	if (atari_switches & ATARI_SWITCH_OVSC_IKBD)
		acia.key_ctrl = ACIA_RESET;
	if (atari_switches & ATARI_SWITCH_OVSC_MIDI)
		acia.mid_ctrl = ACIA_RESET;

	local_irq_disable();
	asm volatile ("movec	%0,%%vbr"
			: : "d" (0));

	if (CPU_IS_040_OR_060) {
		unsigned long jmp_addr040 = virt_to_phys(&&jmp_addr_label040);
		if (CPU_IS_060) {
			
			asm volatile ("\n"
				"	.chip 68060\n"
				"	movec %0,%%pcr\n"
				"	.chip 68k"
				: : "d" (0));
		}

		asm volatile ("\n"
			"	move.l	%0,%%d0\n"
			"	and.l	#0xff000000,%%d0\n"
			"	or.w	#0xe020,%%d0\n"   
			"	.chip	68040\n"
			"	movec	%%d0,%%itt0\n"
			"	movec	%%d0,%%dtt0\n"
			"	.chip	68k\n"
			"	jmp	%0@"
			: : "a" (jmp_addr040)
			: "d0");
	jmp_addr_label040:
		asm volatile ("\n"
			"	moveq	#0,%%d0\n"
			"	nop\n"
			"	.chip	68040\n"
			"	cinva	%%bc\n"
			"	nop\n"
			"	pflusha\n"
			"	nop\n"
			"	movec	%%d0,%%tc\n"
			"	nop\n"
			"	move.l	#0xffc000,%%d0\n" 
			"	movec	%%d0,%%itt0\n"
			"	movec	%%d0,%%itt1\n"
			"	or.w	#0x40,%/d0\n" 
			"	movec	%%d0,%%dtt0\n"
			"	movec	%%d0,%%dtt1\n"
			"	.chip	68k\n"
			"	jmp	%0@"
			: 
			: "a" (reset_addr)
			: "d0");
	} else
		asm volatile ("\n"
			"	pmove	%0,%%tc\n"
			"	jmp	%1@"
			: 
			: "m" (tc_val), "a" (reset_addr));
}


static void atari_get_model(char *model)
{
	strcpy(model, "Atari ");
	switch (atari_mch_cookie >> 16) {
	case ATARI_MCH_ST:
		if (ATARIHW_PRESENT(MSTE_CLK))
			strcat(model, "Mega ST");
		else
			strcat(model, "ST");
		break;
	case ATARI_MCH_STE:
		if (MACH_IS_MSTE)
			strcat(model, "Mega STE");
		else
			strcat(model, "STE");
		break;
	case ATARI_MCH_TT:
		if (MACH_IS_MEDUSA)
			
			strcat(model, "Medusa");
		else
			strcat(model, "TT");
		break;
	case ATARI_MCH_FALCON:
		strcat(model, "Falcon");
		if (MACH_IS_AB40)
			strcat(model, " (with Afterburner040)");
		break;
	default:
		sprintf(model + strlen(model), "(unknown mach cookie 0x%lx)",
			atari_mch_cookie);
		break;
	}
}


static void atari_get_hardware_list(struct seq_file *m)
{
	int i;

	for (i = 0; i < m68k_num_memory; i++)
		seq_printf(m, "\t%3ld MB at 0x%08lx (%s)\n",
				m68k_memory[i].size >> 20, m68k_memory[i].addr,
				(m68k_memory[i].addr & 0xff000000 ?
				 "alternate RAM" : "ST-RAM"));

#define ATARIHW_ANNOUNCE(name, str)			\
	if (ATARIHW_PRESENT(name))			\
		seq_printf(m, "\t%s\n", str)

	seq_printf(m, "Detected hardware:\n");
	ATARIHW_ANNOUNCE(STND_SHIFTER, "ST Shifter");
	ATARIHW_ANNOUNCE(EXTD_SHIFTER, "STe Shifter");
	ATARIHW_ANNOUNCE(TT_SHIFTER, "TT Shifter");
	ATARIHW_ANNOUNCE(VIDEL_SHIFTER, "Falcon Shifter");
	ATARIHW_ANNOUNCE(YM_2149, "Programmable Sound Generator");
	ATARIHW_ANNOUNCE(PCM_8BIT, "PCM 8 Bit Sound");
	ATARIHW_ANNOUNCE(CODEC, "CODEC Sound");
	ATARIHW_ANNOUNCE(TT_SCSI, "SCSI Controller NCR5380 (TT style)");
	ATARIHW_ANNOUNCE(ST_SCSI, "SCSI Controller NCR5380 (Falcon style)");
	ATARIHW_ANNOUNCE(ACSI, "ACSI Interface");
	ATARIHW_ANNOUNCE(IDE, "IDE Interface");
	ATARIHW_ANNOUNCE(FDCSPEED, "8/16 Mhz Switch for FDC");
	ATARIHW_ANNOUNCE(ST_MFP, "Multi Function Peripheral MFP 68901");
	ATARIHW_ANNOUNCE(TT_MFP, "Second Multi Function Peripheral MFP 68901");
	ATARIHW_ANNOUNCE(SCC, "Serial Communications Controller SCC 8530");
	ATARIHW_ANNOUNCE(ST_ESCC, "Extended Serial Communications Controller SCC 85230");
	ATARIHW_ANNOUNCE(ANALOG_JOY, "Paddle Interface");
	ATARIHW_ANNOUNCE(MICROWIRE, "MICROWIRE(tm) Interface");
	ATARIHW_ANNOUNCE(STND_DMA, "DMA Controller (24 bit)");
	ATARIHW_ANNOUNCE(EXTD_DMA, "DMA Controller (32 bit)");
	ATARIHW_ANNOUNCE(SCSI_DMA, "DMA Controller for NCR5380");
	ATARIHW_ANNOUNCE(SCC_DMA, "DMA Controller for SCC");
	ATARIHW_ANNOUNCE(TT_CLK, "Clock Chip MC146818A");
	ATARIHW_ANNOUNCE(MSTE_CLK, "Clock Chip RP5C15");
	ATARIHW_ANNOUNCE(SCU, "System Control Unit");
	ATARIHW_ANNOUNCE(BLITTER, "Blitter");
	ATARIHW_ANNOUNCE(VME, "VME Bus");
	ATARIHW_ANNOUNCE(DSP56K, "DSP56001 processor");
}
