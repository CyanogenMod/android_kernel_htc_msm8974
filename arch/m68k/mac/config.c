/*
 *  linux/arch/m68k/mac/config.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */


#include <linux/module.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/vt_kern.h>
#include <linux/platform_device.h>
#include <linux/adb.h>
#include <linux/cuda.h>

#define BOOTINFO_COMPAT_1_0
#include <asm/setup.h>
#include <asm/bootinfo.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/rtc.h>
#include <asm/machdep.h>

#include <asm/macintosh.h>
#include <asm/macints.h>
#include <asm/machw.h>

#include <asm/mac_iop.h>
#include <asm/mac_via.h>
#include <asm/mac_oss.h>
#include <asm/mac_psc.h>

struct mac_booter_data mac_bi_data;

static unsigned long mac_orig_videoaddr;

extern unsigned long mac_gettimeoffset(void);
extern int mac_hwclk(int, struct rtc_time *);
extern int mac_set_clock_mmss(unsigned long);
extern void iop_preinit(void);
extern void iop_init(void);
extern void via_init(void);
extern void via_init_clock(irq_handler_t func);
extern void via_flush_cache(void);
extern void oss_init(void);
extern void psc_init(void);
extern void baboon_init(void);

extern void mac_mksound(unsigned int, unsigned int);

static void mac_get_model(char *str);
static void mac_identify(void);
static void mac_report_hardware(void);

#ifdef CONFIG_EARLY_PRINTK
asmlinkage void __init mac_early_print(const char *s, unsigned n);

static void __init mac_early_cons_write(struct console *con,
                                 const char *s, unsigned n)
{
	mac_early_print(s, n);
}

static struct console __initdata mac_early_cons = {
	.name  = "early",
	.write = mac_early_cons_write,
	.flags = CON_PRINTBUFFER | CON_BOOT,
	.index = -1
};

int __init mac_unregister_early_cons(void)
{
	
	return unregister_console(&mac_early_cons);
}

late_initcall(mac_unregister_early_cons);
#endif

static void __init mac_sched_init(irq_handler_t vector)
{
	via_init_clock(vector);
}


int __init mac_parse_bootinfo(const struct bi_record *record)
{
	int unknown = 0;
	const u_long *data = record->data;

	switch (record->tag) {
	case BI_MAC_MODEL:
		mac_bi_data.id = *data;
		break;
	case BI_MAC_VADDR:
		mac_bi_data.videoaddr = *data;
		break;
	case BI_MAC_VDEPTH:
		mac_bi_data.videodepth = *data;
		break;
	case BI_MAC_VROW:
		mac_bi_data.videorow = *data;
		break;
	case BI_MAC_VDIM:
		mac_bi_data.dimensions = *data;
		break;
	case BI_MAC_VLOGICAL:
		mac_bi_data.videological = VIDEOMEMBASE + (*data & ~VIDEOMEMMASK);
		mac_orig_videoaddr = *data;
		break;
	case BI_MAC_SCCBASE:
		mac_bi_data.sccbase = *data;
		break;
	case BI_MAC_BTIME:
		mac_bi_data.boottime = *data;
		break;
	case BI_MAC_GMTBIAS:
		mac_bi_data.gmtbias = *data;
		break;
	case BI_MAC_MEMSIZE:
		mac_bi_data.memsize = *data;
		break;
	case BI_MAC_CPUID:
		mac_bi_data.cpuid = *data;
		break;
	case BI_MAC_ROMBASE:
		mac_bi_data.rombase = *data;
		break;
	default:
		unknown = 1;
		break;
	}
	return unknown;
}


static void mac_cache_card_flush(int writeback)
{
	unsigned long flags;

	local_irq_save(flags);
	via_flush_cache();
	local_irq_restore(flags);
}

void __init config_mac(void)
{
	if (!MACH_IS_MAC)
		printk(KERN_ERR "ERROR: no Mac, but config_mac() called!!\n");

	mach_sched_init = mac_sched_init;
	mach_init_IRQ = mac_init_IRQ;
	mach_get_model = mac_get_model;
	mach_gettimeoffset = mac_gettimeoffset;
	mach_hwclk = mac_hwclk;
	mach_set_clock_mmss = mac_set_clock_mmss;
	mach_reset = mac_reset;
	mach_halt = mac_poweroff;
	mach_power_off = mac_poweroff;
	mach_max_dma_address = 0xffffffff;
#if defined(CONFIG_INPUT_M68K_BEEP) || defined(CONFIG_INPUT_M68K_BEEP_MODULE)
	mach_beep = mac_mksound;
#endif

#ifdef CONFIG_EARLY_PRINTK
	register_console(&mac_early_cons);
#endif


	mac_identify();
	mac_report_hardware();


	if (macintosh_config->ident == MAC_MODEL_IICI
	    || macintosh_config->ident == MAC_MODEL_IIFX)
		mach_l2_flush = mac_cache_card_flush;
}



struct mac_model *macintosh_config;
EXPORT_SYMBOL(macintosh_config);

static struct mac_model mac_data_table[] = {

	{
		.ident		= MAC_MODEL_II,
		.name		= "Unknown",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_II,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_IWM,
	},


	{
		.ident		= MAC_MODEL_II,
		.name		= "II",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_II,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_IWM,
	}, {
		.ident		= MAC_MODEL_IIX,
		.name		= "IIx",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_II,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_IICX,
		.name		= "IIcx",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_II,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_SE30,
		.name		= "SE/30",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_II,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	},


	{
		.ident		= MAC_MODEL_IICI,
		.name		= "IIci",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_IIFX,
		.name		= "IIfx",
		.adb_type	= MAC_ADB_IOP,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_IOP,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_IOP,
	}, {
		.ident		= MAC_MODEL_IISI,
		.name		= "IIsi",
		.adb_type	= MAC_ADB_IISI,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_IIVI,
		.name		= "IIvi",
		.adb_type	= MAC_ADB_IISI,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_IIVX,
		.name		= "IIvx",
		.adb_type	= MAC_ADB_IISI,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	},


	{
		.ident		= MAC_MODEL_CLII,
		.name		= "Classic II",
		.adb_type	= MAC_ADB_IISI,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_CCL,
		.name		= "Color Classic",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_CCLII,
		.name		= "Color Classic II",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	},


	{
		.ident		= MAC_MODEL_LC,
		.name		= "LC",
		.adb_type	= MAC_ADB_IISI,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_LCII,
		.name		= "LC II",
		.adb_type	= MAC_ADB_IISI,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_LCIII,
		.name		= "LC III",
		.adb_type	= MAC_ADB_IISI,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	},


	{
		.ident		= MAC_MODEL_Q605,
		.name		= "Quadra 605",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_Q605_ACC,
		.name		= "Quadra 605",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_Q610,
		.name		= "Quadra 610",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.scc_type	= MAC_SCC_QUADRA,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_Q630,
		.name		= "Quadra 630",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.ide_type	= MAC_IDE_QUADRA,
		.scc_type	= MAC_SCC_QUADRA,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_Q650,
		.name		= "Quadra 650",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.scc_type	= MAC_SCC_QUADRA,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	},
	
	{
		.ident		= MAC_MODEL_Q700,
		.name		= "Quadra 700",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA2,
		.scc_type	= MAC_SCC_QUADRA,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_Q800,
		.name		= "Quadra 800",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.scc_type	= MAC_SCC_QUADRA,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_Q840,
		.name		= "Quadra 840AV",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA3,
		.scc_type	= MAC_SCC_PSC,
		.ether_type	= MAC_ETHER_MACE,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_AV,
	}, {
		.ident		= MAC_MODEL_Q900,
		.name		= "Quadra 900",
		.adb_type	= MAC_ADB_IOP,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA2,
		.scc_type	= MAC_SCC_IOP,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_IOP,
	}, {
		.ident		= MAC_MODEL_Q950,
		.name		= "Quadra 950",
		.adb_type	= MAC_ADB_IOP,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA2,
		.scc_type	= MAC_SCC_IOP,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_IOP,
	},


	{
		.ident		= MAC_MODEL_P460,
		.name		= "Performa 460",
		.adb_type	= MAC_ADB_IISI,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_P475,
		.name		= "Performa 475",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_P475F,
		.name		= "Performa 475",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_P520,
		.name		= "Performa 520",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_P550,
		.name		= "Performa 550",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	},
	
	{
		.ident		= MAC_MODEL_P575,
		.name		= "Performa 575",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.scc_type	= MAC_SCC_II,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_P588,
		.name		= "Performa 588",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.ide_type	= MAC_IDE_QUADRA,
		.scc_type	= MAC_SCC_II,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_TV,
		.name		= "TV",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_P600,
		.name		= "Performa 600",
		.adb_type	= MAC_ADB_IISI,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_II,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	},


	{
		.ident		= MAC_MODEL_C610,
		.name		= "Centris 610",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.scc_type	= MAC_SCC_QUADRA,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_C650,
		.name		= "Centris 650",
		.adb_type	= MAC_ADB_II,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA,
		.scc_type	= MAC_SCC_QUADRA,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR1,
	}, {
		.ident		= MAC_MODEL_C660,
		.name		= "Centris 660AV",
		.adb_type	= MAC_ADB_CUDA,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_QUADRA3,
		.scc_type	= MAC_SCC_PSC,
		.ether_type	= MAC_ETHER_MACE,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_AV,
	},


	{
		.ident		= MAC_MODEL_PB140,
		.name		= "PowerBook 140",
		.adb_type	= MAC_ADB_PB1,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB145,
		.name		= "PowerBook 145",
		.adb_type	= MAC_ADB_PB1,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB150,
		.name		= "PowerBook 150",
		.adb_type	= MAC_ADB_PB2,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.ide_type	= MAC_IDE_PB,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB160,
		.name		= "PowerBook 160",
		.adb_type	= MAC_ADB_PB1,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB165,
		.name		= "PowerBook 165",
		.adb_type	= MAC_ADB_PB1,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB165C,
		.name		= "PowerBook 165c",
		.adb_type	= MAC_ADB_PB1,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB170,
		.name		= "PowerBook 170",
		.adb_type	= MAC_ADB_PB1,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB180,
		.name		= "PowerBook 180",
		.adb_type	= MAC_ADB_PB1,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB180C,
		.name		= "PowerBook 180c",
		.adb_type	= MAC_ADB_PB1,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB190,
		.name		= "PowerBook 190",
		.adb_type	= MAC_ADB_PB2,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_OLD,
		.ide_type	= MAC_IDE_BABOON,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB520,
		.name		= "PowerBook 520",
		.adb_type	= MAC_ADB_PB2,
		.via_type	= MAC_VIA_QUADRA,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.ether_type	= MAC_ETHER_SONIC,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	},


	{
		.ident		= MAC_MODEL_PB210,
		.name		= "PowerBook Duo 210",
		.adb_type	= MAC_ADB_PB2,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB230,
		.name		= "PowerBook Duo 230",
		.adb_type	= MAC_ADB_PB2,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB250,
		.name		= "PowerBook Duo 250",
		.adb_type	= MAC_ADB_PB2,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB270C,
		.name		= "PowerBook Duo 270c",
		.adb_type	= MAC_ADB_PB2,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB280,
		.name		= "PowerBook Duo 280",
		.adb_type	= MAC_ADB_PB2,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	}, {
		.ident		= MAC_MODEL_PB280C,
		.name		= "PowerBook Duo 280c",
		.adb_type	= MAC_ADB_PB2,
		.via_type	= MAC_VIA_IICI,
		.scsi_type	= MAC_SCSI_OLD,
		.scc_type	= MAC_SCC_QUADRA,
		.nubus_type	= MAC_NUBUS,
		.floppy_type	= MAC_FLOPPY_SWIM_ADDR2,
	},


	{
		.ident		= -1
	}
};

static struct resource scc_a_rsrcs[] = {
	{ .flags = IORESOURCE_MEM },
	{ .flags = IORESOURCE_IRQ },
};

static struct resource scc_b_rsrcs[] = {
	{ .flags = IORESOURCE_MEM },
	{ .flags = IORESOURCE_IRQ },
};

struct platform_device scc_a_pdev = {
	.name           = "scc",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(scc_a_rsrcs),
	.resource       = scc_a_rsrcs,
};
EXPORT_SYMBOL(scc_a_pdev);

struct platform_device scc_b_pdev = {
	.name           = "scc",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(scc_b_rsrcs),
	.resource       = scc_b_rsrcs,
};
EXPORT_SYMBOL(scc_b_pdev);

static void __init mac_identify(void)
{
	struct mac_model *m;

	
	int model = mac_bi_data.id;
	if (!model) {
		
		
		model = (mac_bi_data.cpuid >> 2) & 63;
		printk(KERN_WARNING "No bootinfo model ID, using cpuid instead "
		       "(obsolete bootloader?)\n");
	}

	macintosh_config = mac_data_table;
	for (m = macintosh_config; m->ident != -1; m++) {
		if (m->ident == model) {
			macintosh_config = m;
			break;
		}
	}

	

	scc_a_rsrcs[0].start = (resource_size_t) mac_bi_data.sccbase + 2;
	scc_a_rsrcs[0].end   = scc_a_rsrcs[0].start;
	scc_b_rsrcs[0].start = (resource_size_t) mac_bi_data.sccbase;
	scc_b_rsrcs[0].end   = scc_b_rsrcs[0].start;

	switch (macintosh_config->scc_type) {
	case MAC_SCC_PSC:
		scc_a_rsrcs[1].start = scc_a_rsrcs[1].end = IRQ_MAC_SCC_A;
		scc_b_rsrcs[1].start = scc_b_rsrcs[1].end = IRQ_MAC_SCC_B;
		break;
	default:
		
		if (macintosh_config->ident == MAC_MODEL_IIFX) {
			scc_a_rsrcs[1].start = scc_a_rsrcs[1].end = IRQ_MAC_SCC;
			scc_b_rsrcs[1].start = scc_b_rsrcs[1].end = IRQ_MAC_SCC;
		} else {
			scc_a_rsrcs[1].start = scc_a_rsrcs[1].end = IRQ_AUTO_4;
			scc_b_rsrcs[1].start = scc_b_rsrcs[1].end = IRQ_AUTO_4;
		}
		break;
	}

	iop_preinit();

	printk(KERN_INFO "Detected Macintosh model: %d\n", model);

	printk(KERN_DEBUG " Penguin bootinfo data:\n");
	printk(KERN_DEBUG " Video: addr 0x%lx "
		"row 0x%lx depth %lx dimensions %ld x %ld\n",
		mac_bi_data.videoaddr, mac_bi_data.videorow,
		mac_bi_data.videodepth, mac_bi_data.dimensions & 0xFFFF,
		mac_bi_data.dimensions >> 16);
	printk(KERN_DEBUG " Videological 0x%lx phys. 0x%lx, SCC at 0x%lx\n",
		mac_bi_data.videological, mac_orig_videoaddr,
		mac_bi_data.sccbase);
	printk(KERN_DEBUG " Boottime: 0x%lx GMTBias: 0x%lx\n",
		mac_bi_data.boottime, mac_bi_data.gmtbias);
	printk(KERN_DEBUG " Machine ID: %ld CPUid: 0x%lx memory size: 0x%lx\n",
		mac_bi_data.id, mac_bi_data.cpuid, mac_bi_data.memsize);

	iop_init();
	via_init();
	oss_init();
	psc_init();
	baboon_init();

#ifdef CONFIG_ADB_CUDA
	find_via_cuda();
#endif
}

static void __init mac_report_hardware(void)
{
	printk(KERN_INFO "Apple Macintosh %s\n", macintosh_config->name);
}

static void mac_get_model(char *str)
{
	strcpy(str, "Macintosh ");
	strcat(str, macintosh_config->name);
}

static struct resource swim_rsrc = { .flags = IORESOURCE_MEM };

static struct platform_device swim_pdev = {
	.name		= "swim",
	.id		= -1,
	.num_resources	= 1,
	.resource	= &swim_rsrc,
};

static struct platform_device esp_0_pdev = {
	.name		= "mac_esp",
	.id		= 0,
};

static struct platform_device esp_1_pdev = {
	.name		= "mac_esp",
	.id		= 1,
};

static struct platform_device sonic_pdev = {
	.name		= "macsonic",
	.id		= -1,
};

static struct platform_device mace_pdev = {
	.name		= "macmace",
	.id		= -1,
};

int __init mac_platform_init(void)
{
	u8 *swim_base;

	if (!MACH_IS_MAC)
		return -ENODEV;


	platform_device_register(&scc_a_pdev);
	platform_device_register(&scc_b_pdev);


	switch (macintosh_config->floppy_type) {
	case MAC_FLOPPY_SWIM_ADDR1:
		swim_base = (u8 *)(VIA1_BASE + 0x1E000);
		break;
	case MAC_FLOPPY_SWIM_ADDR2:
		swim_base = (u8 *)(VIA1_BASE + 0x16000);
		break;
	default:
		swim_base = NULL;
		break;
	}

	if (swim_base) {
		swim_rsrc.start = (resource_size_t) swim_base,
		swim_rsrc.end   = (resource_size_t) swim_base + 0x2000,
		platform_device_register(&swim_pdev);
	}


	switch (macintosh_config->scsi_type) {
	case MAC_SCSI_QUADRA:
	case MAC_SCSI_QUADRA3:
		platform_device_register(&esp_0_pdev);
		break;
	case MAC_SCSI_QUADRA2:
		platform_device_register(&esp_0_pdev);
		if ((macintosh_config->ident == MAC_MODEL_Q900) ||
		    (macintosh_config->ident == MAC_MODEL_Q950))
			platform_device_register(&esp_1_pdev);
		break;
	}


	switch (macintosh_config->ether_type) {
	case MAC_ETHER_SONIC:
		platform_device_register(&sonic_pdev);
		break;
	case MAC_ETHER_MACE:
		platform_device_register(&mace_pdev);
		break;
	}

	return 0;
}

arch_initcall(mac_platform_init);
