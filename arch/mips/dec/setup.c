/*
 * System-specific setup, especially interrupts.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1998 Harald Koerfgen
 * Copyright (C) 2000, 2001, 2002, 2003, 2005  Maciej W. Rozycki
 */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/param.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/pm.h>
#include <linux/irq.h>

#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/cpu-features.h>
#include <asm/irq.h>
#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <asm/reboot.h>
#include <asm/time.h>
#include <asm/traps.h>
#include <asm/wbflush.h>

#include <asm/dec/interrupts.h>
#include <asm/dec/ioasic.h>
#include <asm/dec/ioasic_addrs.h>
#include <asm/dec/ioasic_ints.h>
#include <asm/dec/kn01.h>
#include <asm/dec/kn02.h>
#include <asm/dec/kn02ba.h>
#include <asm/dec/kn02ca.h>
#include <asm/dec/kn03.h>
#include <asm/dec/kn230.h>
#include <asm/dec/system.h>


extern void dec_machine_restart(char *command);
extern void dec_machine_halt(void);
extern void dec_machine_power_off(void);
extern irqreturn_t dec_intr_halt(int irq, void *dev_id);

unsigned long dec_kn_slot_base, dec_kn_slot_size;

EXPORT_SYMBOL(dec_kn_slot_base);
EXPORT_SYMBOL(dec_kn_slot_size);

int dec_tc_bus;

DEFINE_SPINLOCK(ioasic_ssr_lock);

volatile u32 *ioasic_base;

EXPORT_SYMBOL(ioasic_base);


int dec_interrupt[DEC_NR_INTS] = {
	[0 ... DEC_NR_INTS - 1] = -1
};

EXPORT_SYMBOL(dec_interrupt);

int_ptr cpu_mask_nr_tbl[DEC_MAX_CPU_INTS][2] = {
	{ { .i = ~0 }, { .p = dec_intr_unimplemented } },
};
int_ptr asic_mask_nr_tbl[DEC_MAX_ASIC_INTS][2] = {
	{ { .i = ~0 }, { .p = asic_intr_unimplemented } },
};
int cpu_fpu_mask = DEC_CPU_IRQ_MASK(DEC_CPU_INR_FPU);

static struct irqaction ioirq = {
	.handler = no_action,
	.name = "cascade",
	.flags = IRQF_NO_THREAD,
};
static struct irqaction fpuirq = {
	.handler = no_action,
	.name = "fpu",
	.flags = IRQF_NO_THREAD,
};

static struct irqaction busirq = {
	.name = "bus error",
	.flags = IRQF_NO_THREAD,
};

static struct irqaction haltirq = {
	.handler = dec_intr_halt,
	.name = "halt",
	.flags = IRQF_NO_THREAD,
};


static void __init dec_be_init(void)
{
	switch (mips_machtype) {
	case MACH_DS23100:	
		board_be_handler = dec_kn01_be_handler;
		busirq.handler = dec_kn01_be_interrupt;
		busirq.flags |= IRQF_SHARED;
		dec_kn01_be_init();
		break;
	case MACH_DS5000_1XX:	
	case MACH_DS5000_XX:	
		board_be_handler = dec_kn02xa_be_handler;
		busirq.handler = dec_kn02xa_be_interrupt;
		dec_kn02xa_be_init();
		break;
	case MACH_DS5000_200:	
	case MACH_DS5000_2X0:	
	case MACH_DS5900:	
		board_be_handler = dec_ecc_be_handler;
		busirq.handler = dec_ecc_be_interrupt;
		dec_ecc_be_init();
		break;
	}
}

void __init plat_mem_setup(void)
{
	board_be_init = dec_be_init;

	wbflush_setup();

	_machine_restart = dec_machine_restart;
	_machine_halt = dec_machine_halt;
	pm_power_off = dec_machine_power_off;

	ioport_resource.start = ~0UL;
	ioport_resource.end = 0UL;
}

static int kn01_interrupt[DEC_NR_INTS] __initdata = {
	[DEC_IRQ_CASCADE]	= -1,
	[DEC_IRQ_AB_RECV]	= -1,
	[DEC_IRQ_AB_XMIT]	= -1,
	[DEC_IRQ_DZ11]		= DEC_CPU_IRQ_NR(KN01_CPU_INR_DZ11),
	[DEC_IRQ_ASC]		= -1,
	[DEC_IRQ_FLOPPY]	= -1,
	[DEC_IRQ_FPU]		= DEC_CPU_IRQ_NR(DEC_CPU_INR_FPU),
	[DEC_IRQ_HALT]		= -1,
	[DEC_IRQ_ISDN]		= -1,
	[DEC_IRQ_LANCE]		= DEC_CPU_IRQ_NR(KN01_CPU_INR_LANCE),
	[DEC_IRQ_BUS]		= DEC_CPU_IRQ_NR(KN01_CPU_INR_BUS),
	[DEC_IRQ_PSU]		= -1,
	[DEC_IRQ_RTC]		= DEC_CPU_IRQ_NR(KN01_CPU_INR_RTC),
	[DEC_IRQ_SCC0]		= -1,
	[DEC_IRQ_SCC1]		= -1,
	[DEC_IRQ_SII]		= DEC_CPU_IRQ_NR(KN01_CPU_INR_SII),
	[DEC_IRQ_TC0]		= -1,
	[DEC_IRQ_TC1]		= -1,
	[DEC_IRQ_TC2]		= -1,
	[DEC_IRQ_TIMER]		= -1,
	[DEC_IRQ_VIDEO]		= DEC_CPU_IRQ_NR(KN01_CPU_INR_VIDEO),
	[DEC_IRQ_ASC_MERR]	= -1,
	[DEC_IRQ_ASC_ERR]	= -1,
	[DEC_IRQ_ASC_DMA]	= -1,
	[DEC_IRQ_FLOPPY_ERR]	= -1,
	[DEC_IRQ_ISDN_ERR]	= -1,
	[DEC_IRQ_ISDN_RXDMA]	= -1,
	[DEC_IRQ_ISDN_TXDMA]	= -1,
	[DEC_IRQ_LANCE_MERR]	= -1,
	[DEC_IRQ_SCC0A_RXERR]	= -1,
	[DEC_IRQ_SCC0A_RXDMA]	= -1,
	[DEC_IRQ_SCC0A_TXERR]	= -1,
	[DEC_IRQ_SCC0A_TXDMA]	= -1,
	[DEC_IRQ_AB_RXERR]	= -1,
	[DEC_IRQ_AB_RXDMA]	= -1,
	[DEC_IRQ_AB_TXERR]	= -1,
	[DEC_IRQ_AB_TXDMA]	= -1,
	[DEC_IRQ_SCC1A_RXERR]	= -1,
	[DEC_IRQ_SCC1A_RXDMA]	= -1,
	[DEC_IRQ_SCC1A_TXERR]	= -1,
	[DEC_IRQ_SCC1A_TXDMA]	= -1,
};

static int_ptr kn01_cpu_mask_nr_tbl[][2] __initdata = {
	{ { .i = DEC_CPU_IRQ_MASK(KN01_CPU_INR_BUS) },
		{ .i = DEC_CPU_IRQ_NR(KN01_CPU_INR_BUS) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN01_CPU_INR_RTC) },
		{ .i = DEC_CPU_IRQ_NR(KN01_CPU_INR_RTC) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN01_CPU_INR_DZ11) },
		{ .i = DEC_CPU_IRQ_NR(KN01_CPU_INR_DZ11) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN01_CPU_INR_SII) },
		{ .i = DEC_CPU_IRQ_NR(KN01_CPU_INR_SII) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN01_CPU_INR_LANCE) },
		{ .i = DEC_CPU_IRQ_NR(KN01_CPU_INR_LANCE) } },
	{ { .i = DEC_CPU_IRQ_ALL },
		{ .p = cpu_all_int } },
};

static void __init dec_init_kn01(void)
{
	
	memcpy(&dec_interrupt, &kn01_interrupt,
		sizeof(kn01_interrupt));

	
	memcpy(&cpu_mask_nr_tbl, &kn01_cpu_mask_nr_tbl,
		sizeof(kn01_cpu_mask_nr_tbl));

	mips_cpu_irq_init();

}				


static int kn230_interrupt[DEC_NR_INTS] __initdata = {
	[DEC_IRQ_CASCADE]	= -1,
	[DEC_IRQ_AB_RECV]	= -1,
	[DEC_IRQ_AB_XMIT]	= -1,
	[DEC_IRQ_DZ11]		= DEC_CPU_IRQ_NR(KN230_CPU_INR_DZ11),
	[DEC_IRQ_ASC]		= -1,
	[DEC_IRQ_FLOPPY]	= -1,
	[DEC_IRQ_FPU]		= DEC_CPU_IRQ_NR(DEC_CPU_INR_FPU),
	[DEC_IRQ_HALT]		= DEC_CPU_IRQ_NR(KN230_CPU_INR_HALT),
	[DEC_IRQ_ISDN]		= -1,
	[DEC_IRQ_LANCE]		= DEC_CPU_IRQ_NR(KN230_CPU_INR_LANCE),
	[DEC_IRQ_BUS]		= DEC_CPU_IRQ_NR(KN230_CPU_INR_BUS),
	[DEC_IRQ_PSU]		= -1,
	[DEC_IRQ_RTC]		= DEC_CPU_IRQ_NR(KN230_CPU_INR_RTC),
	[DEC_IRQ_SCC0]		= -1,
	[DEC_IRQ_SCC1]		= -1,
	[DEC_IRQ_SII]		= DEC_CPU_IRQ_NR(KN230_CPU_INR_SII),
	[DEC_IRQ_TC0]		= -1,
	[DEC_IRQ_TC1]		= -1,
	[DEC_IRQ_TC2]		= -1,
	[DEC_IRQ_TIMER]		= -1,
	[DEC_IRQ_VIDEO]		= -1,
	[DEC_IRQ_ASC_MERR]	= -1,
	[DEC_IRQ_ASC_ERR]	= -1,
	[DEC_IRQ_ASC_DMA]	= -1,
	[DEC_IRQ_FLOPPY_ERR]	= -1,
	[DEC_IRQ_ISDN_ERR]	= -1,
	[DEC_IRQ_ISDN_RXDMA]	= -1,
	[DEC_IRQ_ISDN_TXDMA]	= -1,
	[DEC_IRQ_LANCE_MERR]	= -1,
	[DEC_IRQ_SCC0A_RXERR]	= -1,
	[DEC_IRQ_SCC0A_RXDMA]	= -1,
	[DEC_IRQ_SCC0A_TXERR]	= -1,
	[DEC_IRQ_SCC0A_TXDMA]	= -1,
	[DEC_IRQ_AB_RXERR]	= -1,
	[DEC_IRQ_AB_RXDMA]	= -1,
	[DEC_IRQ_AB_TXERR]	= -1,
	[DEC_IRQ_AB_TXDMA]	= -1,
	[DEC_IRQ_SCC1A_RXERR]	= -1,
	[DEC_IRQ_SCC1A_RXDMA]	= -1,
	[DEC_IRQ_SCC1A_TXERR]	= -1,
	[DEC_IRQ_SCC1A_TXDMA]	= -1,
};

static int_ptr kn230_cpu_mask_nr_tbl[][2] __initdata = {
	{ { .i = DEC_CPU_IRQ_MASK(KN230_CPU_INR_BUS) },
		{ .i = DEC_CPU_IRQ_NR(KN230_CPU_INR_BUS) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN230_CPU_INR_RTC) },
		{ .i = DEC_CPU_IRQ_NR(KN230_CPU_INR_RTC) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN230_CPU_INR_DZ11) },
		{ .i = DEC_CPU_IRQ_NR(KN230_CPU_INR_DZ11) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN230_CPU_INR_SII) },
		{ .i = DEC_CPU_IRQ_NR(KN230_CPU_INR_SII) } },
	{ { .i = DEC_CPU_IRQ_ALL },
		{ .p = cpu_all_int } },
};

static void __init dec_init_kn230(void)
{
	
	memcpy(&dec_interrupt, &kn230_interrupt,
		sizeof(kn230_interrupt));

	
	memcpy(&cpu_mask_nr_tbl, &kn230_cpu_mask_nr_tbl,
		sizeof(kn230_cpu_mask_nr_tbl));

	mips_cpu_irq_init();

}				


static int kn02_interrupt[DEC_NR_INTS] __initdata = {
	[DEC_IRQ_CASCADE]	= DEC_CPU_IRQ_NR(KN02_CPU_INR_CASCADE),
	[DEC_IRQ_AB_RECV]	= -1,
	[DEC_IRQ_AB_XMIT]	= -1,
	[DEC_IRQ_DZ11]		= KN02_IRQ_NR(KN02_CSR_INR_DZ11),
	[DEC_IRQ_ASC]		= KN02_IRQ_NR(KN02_CSR_INR_ASC),
	[DEC_IRQ_FLOPPY]	= -1,
	[DEC_IRQ_FPU]		= DEC_CPU_IRQ_NR(DEC_CPU_INR_FPU),
	[DEC_IRQ_HALT]		= -1,
	[DEC_IRQ_ISDN]		= -1,
	[DEC_IRQ_LANCE]		= KN02_IRQ_NR(KN02_CSR_INR_LANCE),
	[DEC_IRQ_BUS]		= DEC_CPU_IRQ_NR(KN02_CPU_INR_BUS),
	[DEC_IRQ_PSU]		= -1,
	[DEC_IRQ_RTC]		= DEC_CPU_IRQ_NR(KN02_CPU_INR_RTC),
	[DEC_IRQ_SCC0]		= -1,
	[DEC_IRQ_SCC1]		= -1,
	[DEC_IRQ_SII]		= -1,
	[DEC_IRQ_TC0]		= KN02_IRQ_NR(KN02_CSR_INR_TC0),
	[DEC_IRQ_TC1]		= KN02_IRQ_NR(KN02_CSR_INR_TC1),
	[DEC_IRQ_TC2]		= KN02_IRQ_NR(KN02_CSR_INR_TC2),
	[DEC_IRQ_TIMER]		= -1,
	[DEC_IRQ_VIDEO]		= -1,
	[DEC_IRQ_ASC_MERR]	= -1,
	[DEC_IRQ_ASC_ERR]	= -1,
	[DEC_IRQ_ASC_DMA]	= -1,
	[DEC_IRQ_FLOPPY_ERR]	= -1,
	[DEC_IRQ_ISDN_ERR]	= -1,
	[DEC_IRQ_ISDN_RXDMA]	= -1,
	[DEC_IRQ_ISDN_TXDMA]	= -1,
	[DEC_IRQ_LANCE_MERR]	= -1,
	[DEC_IRQ_SCC0A_RXERR]	= -1,
	[DEC_IRQ_SCC0A_RXDMA]	= -1,
	[DEC_IRQ_SCC0A_TXERR]	= -1,
	[DEC_IRQ_SCC0A_TXDMA]	= -1,
	[DEC_IRQ_AB_RXERR]	= -1,
	[DEC_IRQ_AB_RXDMA]	= -1,
	[DEC_IRQ_AB_TXERR]	= -1,
	[DEC_IRQ_AB_TXDMA]	= -1,
	[DEC_IRQ_SCC1A_RXERR]	= -1,
	[DEC_IRQ_SCC1A_RXDMA]	= -1,
	[DEC_IRQ_SCC1A_TXERR]	= -1,
	[DEC_IRQ_SCC1A_TXDMA]	= -1,
};

static int_ptr kn02_cpu_mask_nr_tbl[][2] __initdata = {
	{ { .i = DEC_CPU_IRQ_MASK(KN02_CPU_INR_BUS) },
		{ .i = DEC_CPU_IRQ_NR(KN02_CPU_INR_BUS) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN02_CPU_INR_RTC) },
		{ .i = DEC_CPU_IRQ_NR(KN02_CPU_INR_RTC) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN02_CPU_INR_CASCADE) },
		{ .p = kn02_io_int } },
	{ { .i = DEC_CPU_IRQ_ALL },
		{ .p = cpu_all_int } },
};

static int_ptr kn02_asic_mask_nr_tbl[][2] __initdata = {
	{ { .i = KN02_IRQ_MASK(KN02_CSR_INR_DZ11) },
		{ .i = KN02_IRQ_NR(KN02_CSR_INR_DZ11) } },
	{ { .i = KN02_IRQ_MASK(KN02_CSR_INR_ASC) },
		{ .i = KN02_IRQ_NR(KN02_CSR_INR_ASC) } },
	{ { .i = KN02_IRQ_MASK(KN02_CSR_INR_LANCE) },
		{ .i = KN02_IRQ_NR(KN02_CSR_INR_LANCE) } },
	{ { .i = KN02_IRQ_MASK(KN02_CSR_INR_TC2) },
		{ .i = KN02_IRQ_NR(KN02_CSR_INR_TC2) } },
	{ { .i = KN02_IRQ_MASK(KN02_CSR_INR_TC1) },
		{ .i = KN02_IRQ_NR(KN02_CSR_INR_TC1) } },
	{ { .i = KN02_IRQ_MASK(KN02_CSR_INR_TC0) },
		{ .i = KN02_IRQ_NR(KN02_CSR_INR_TC0) } },
	{ { .i = KN02_IRQ_ALL },
		{ .p = kn02_all_int } },
};

static void __init dec_init_kn02(void)
{
	
	memcpy(&dec_interrupt, &kn02_interrupt,
		sizeof(kn02_interrupt));

	
	memcpy(&cpu_mask_nr_tbl, &kn02_cpu_mask_nr_tbl,
		sizeof(kn02_cpu_mask_nr_tbl));

	
	memcpy(&asic_mask_nr_tbl, &kn02_asic_mask_nr_tbl,
		sizeof(kn02_asic_mask_nr_tbl));

	mips_cpu_irq_init();
	init_kn02_irqs(KN02_IRQ_BASE);

}				


static int kn02ba_interrupt[DEC_NR_INTS] __initdata = {
	[DEC_IRQ_CASCADE]	= DEC_CPU_IRQ_NR(KN02BA_CPU_INR_CASCADE),
	[DEC_IRQ_AB_RECV]	= -1,
	[DEC_IRQ_AB_XMIT]	= -1,
	[DEC_IRQ_DZ11]		= -1,
	[DEC_IRQ_ASC]		= IO_IRQ_NR(KN02BA_IO_INR_ASC),
	[DEC_IRQ_FLOPPY]	= -1,
	[DEC_IRQ_FPU]		= DEC_CPU_IRQ_NR(DEC_CPU_INR_FPU),
	[DEC_IRQ_HALT]		= DEC_CPU_IRQ_NR(KN02BA_CPU_INR_HALT),
	[DEC_IRQ_ISDN]		= -1,
	[DEC_IRQ_LANCE]		= IO_IRQ_NR(KN02BA_IO_INR_LANCE),
	[DEC_IRQ_BUS]		= IO_IRQ_NR(KN02BA_IO_INR_BUS),
	[DEC_IRQ_PSU]		= IO_IRQ_NR(KN02BA_IO_INR_PSU),
	[DEC_IRQ_RTC]		= IO_IRQ_NR(KN02BA_IO_INR_RTC),
	[DEC_IRQ_SCC0]		= IO_IRQ_NR(KN02BA_IO_INR_SCC0),
	[DEC_IRQ_SCC1]		= IO_IRQ_NR(KN02BA_IO_INR_SCC1),
	[DEC_IRQ_SII]		= -1,
	[DEC_IRQ_TC0]		= DEC_CPU_IRQ_NR(KN02BA_CPU_INR_TC0),
	[DEC_IRQ_TC1]		= DEC_CPU_IRQ_NR(KN02BA_CPU_INR_TC1),
	[DEC_IRQ_TC2]		= DEC_CPU_IRQ_NR(KN02BA_CPU_INR_TC2),
	[DEC_IRQ_TIMER]		= -1,
	[DEC_IRQ_VIDEO]		= -1,
	[DEC_IRQ_ASC_MERR]	= IO_IRQ_NR(IO_INR_ASC_MERR),
	[DEC_IRQ_ASC_ERR]	= IO_IRQ_NR(IO_INR_ASC_ERR),
	[DEC_IRQ_ASC_DMA]	= IO_IRQ_NR(IO_INR_ASC_DMA),
	[DEC_IRQ_FLOPPY_ERR]	= -1,
	[DEC_IRQ_ISDN_ERR]	= -1,
	[DEC_IRQ_ISDN_RXDMA]	= -1,
	[DEC_IRQ_ISDN_TXDMA]	= -1,
	[DEC_IRQ_LANCE_MERR]	= IO_IRQ_NR(IO_INR_LANCE_MERR),
	[DEC_IRQ_SCC0A_RXERR]	= IO_IRQ_NR(IO_INR_SCC0A_RXERR),
	[DEC_IRQ_SCC0A_RXDMA]	= IO_IRQ_NR(IO_INR_SCC0A_RXDMA),
	[DEC_IRQ_SCC0A_TXERR]	= IO_IRQ_NR(IO_INR_SCC0A_TXERR),
	[DEC_IRQ_SCC0A_TXDMA]	= IO_IRQ_NR(IO_INR_SCC0A_TXDMA),
	[DEC_IRQ_AB_RXERR]	= -1,
	[DEC_IRQ_AB_RXDMA]	= -1,
	[DEC_IRQ_AB_TXERR]	= -1,
	[DEC_IRQ_AB_TXDMA]	= -1,
	[DEC_IRQ_SCC1A_RXERR]	= IO_IRQ_NR(IO_INR_SCC1A_RXERR),
	[DEC_IRQ_SCC1A_RXDMA]	= IO_IRQ_NR(IO_INR_SCC1A_RXDMA),
	[DEC_IRQ_SCC1A_TXERR]	= IO_IRQ_NR(IO_INR_SCC1A_TXERR),
	[DEC_IRQ_SCC1A_TXDMA]	= IO_IRQ_NR(IO_INR_SCC1A_TXDMA),
};

static int_ptr kn02ba_cpu_mask_nr_tbl[][2] __initdata = {
	{ { .i = DEC_CPU_IRQ_MASK(KN02BA_CPU_INR_CASCADE) },
		{ .p = kn02xa_io_int } },
	{ { .i = DEC_CPU_IRQ_MASK(KN02BA_CPU_INR_TC2) },
		{ .i = DEC_CPU_IRQ_NR(KN02BA_CPU_INR_TC2) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN02BA_CPU_INR_TC1) },
		{ .i = DEC_CPU_IRQ_NR(KN02BA_CPU_INR_TC1) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN02BA_CPU_INR_TC0) },
		{ .i = DEC_CPU_IRQ_NR(KN02BA_CPU_INR_TC0) } },
	{ { .i = DEC_CPU_IRQ_ALL },
		{ .p = cpu_all_int } },
};

static int_ptr kn02ba_asic_mask_nr_tbl[][2] __initdata = {
	{ { .i = IO_IRQ_MASK(KN02BA_IO_INR_BUS) },
		{ .i = IO_IRQ_NR(KN02BA_IO_INR_BUS) } },
	{ { .i = IO_IRQ_MASK(KN02BA_IO_INR_RTC) },
		{ .i = IO_IRQ_NR(KN02BA_IO_INR_RTC) } },
	{ { .i = IO_IRQ_DMA },
		{ .p = asic_dma_int } },
	{ { .i = IO_IRQ_MASK(KN02BA_IO_INR_SCC0) },
		{ .i = IO_IRQ_NR(KN02BA_IO_INR_SCC0) } },
	{ { .i = IO_IRQ_MASK(KN02BA_IO_INR_SCC1) },
		{ .i = IO_IRQ_NR(KN02BA_IO_INR_SCC1) } },
	{ { .i = IO_IRQ_MASK(KN02BA_IO_INR_ASC) },
		{ .i = IO_IRQ_NR(KN02BA_IO_INR_ASC) } },
	{ { .i = IO_IRQ_MASK(KN02BA_IO_INR_LANCE) },
		{ .i = IO_IRQ_NR(KN02BA_IO_INR_LANCE) } },
	{ { .i = IO_IRQ_ALL },
		{ .p = asic_all_int } },
};

static void __init dec_init_kn02ba(void)
{
	
	memcpy(&dec_interrupt, &kn02ba_interrupt,
		sizeof(kn02ba_interrupt));

	
	memcpy(&cpu_mask_nr_tbl, &kn02ba_cpu_mask_nr_tbl,
		sizeof(kn02ba_cpu_mask_nr_tbl));

	
	memcpy(&asic_mask_nr_tbl, &kn02ba_asic_mask_nr_tbl,
		sizeof(kn02ba_asic_mask_nr_tbl));

	mips_cpu_irq_init();
	init_ioasic_irqs(IO_IRQ_BASE);

}				


static int kn02ca_interrupt[DEC_NR_INTS] __initdata = {
	[DEC_IRQ_CASCADE]	= DEC_CPU_IRQ_NR(KN02CA_CPU_INR_CASCADE),
	[DEC_IRQ_AB_RECV]	= IO_IRQ_NR(KN02CA_IO_INR_AB_RECV),
	[DEC_IRQ_AB_XMIT]	= IO_IRQ_NR(KN02CA_IO_INR_AB_XMIT),
	[DEC_IRQ_DZ11]		= -1,
	[DEC_IRQ_ASC]		= IO_IRQ_NR(KN02CA_IO_INR_ASC),
	[DEC_IRQ_FLOPPY]	= IO_IRQ_NR(KN02CA_IO_INR_FLOPPY),
	[DEC_IRQ_FPU]		= DEC_CPU_IRQ_NR(DEC_CPU_INR_FPU),
	[DEC_IRQ_HALT]		= DEC_CPU_IRQ_NR(KN02CA_CPU_INR_HALT),
	[DEC_IRQ_ISDN]		= IO_IRQ_NR(KN02CA_IO_INR_ISDN),
	[DEC_IRQ_LANCE]		= IO_IRQ_NR(KN02CA_IO_INR_LANCE),
	[DEC_IRQ_BUS]		= DEC_CPU_IRQ_NR(KN02CA_CPU_INR_BUS),
	[DEC_IRQ_PSU]		= -1,
	[DEC_IRQ_RTC]		= DEC_CPU_IRQ_NR(KN02CA_CPU_INR_RTC),
	[DEC_IRQ_SCC0]		= IO_IRQ_NR(KN02CA_IO_INR_SCC0),
	[DEC_IRQ_SCC1]		= -1,
	[DEC_IRQ_SII]		= -1,
	[DEC_IRQ_TC0]		= IO_IRQ_NR(KN02CA_IO_INR_TC0),
	[DEC_IRQ_TC1]		= IO_IRQ_NR(KN02CA_IO_INR_TC1),
	[DEC_IRQ_TC2]		= -1,
	[DEC_IRQ_TIMER]		= DEC_CPU_IRQ_NR(KN02CA_CPU_INR_TIMER),
	[DEC_IRQ_VIDEO]		= IO_IRQ_NR(KN02CA_IO_INR_VIDEO),
	[DEC_IRQ_ASC_MERR]	= IO_IRQ_NR(IO_INR_ASC_MERR),
	[DEC_IRQ_ASC_ERR]	= IO_IRQ_NR(IO_INR_ASC_ERR),
	[DEC_IRQ_ASC_DMA]	= IO_IRQ_NR(IO_INR_ASC_DMA),
	[DEC_IRQ_FLOPPY_ERR]	= IO_IRQ_NR(IO_INR_FLOPPY_ERR),
	[DEC_IRQ_ISDN_ERR]	= IO_IRQ_NR(IO_INR_ISDN_ERR),
	[DEC_IRQ_ISDN_RXDMA]	= IO_IRQ_NR(IO_INR_ISDN_RXDMA),
	[DEC_IRQ_ISDN_TXDMA]	= IO_IRQ_NR(IO_INR_ISDN_TXDMA),
	[DEC_IRQ_LANCE_MERR]	= IO_IRQ_NR(IO_INR_LANCE_MERR),
	[DEC_IRQ_SCC0A_RXERR]	= IO_IRQ_NR(IO_INR_SCC0A_RXERR),
	[DEC_IRQ_SCC0A_RXDMA]	= IO_IRQ_NR(IO_INR_SCC0A_RXDMA),
	[DEC_IRQ_SCC0A_TXERR]	= IO_IRQ_NR(IO_INR_SCC0A_TXERR),
	[DEC_IRQ_SCC0A_TXDMA]	= IO_IRQ_NR(IO_INR_SCC0A_TXDMA),
	[DEC_IRQ_AB_RXERR]	= IO_IRQ_NR(IO_INR_AB_RXERR),
	[DEC_IRQ_AB_RXDMA]	= IO_IRQ_NR(IO_INR_AB_RXDMA),
	[DEC_IRQ_AB_TXERR]	= IO_IRQ_NR(IO_INR_AB_TXERR),
	[DEC_IRQ_AB_TXDMA]	= IO_IRQ_NR(IO_INR_AB_TXDMA),
	[DEC_IRQ_SCC1A_RXERR]	= -1,
	[DEC_IRQ_SCC1A_RXDMA]	= -1,
	[DEC_IRQ_SCC1A_TXERR]	= -1,
	[DEC_IRQ_SCC1A_TXDMA]	= -1,
};

static int_ptr kn02ca_cpu_mask_nr_tbl[][2] __initdata = {
	{ { .i = DEC_CPU_IRQ_MASK(KN02CA_CPU_INR_BUS) },
		{ .i = DEC_CPU_IRQ_NR(KN02CA_CPU_INR_BUS) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN02CA_CPU_INR_RTC) },
		{ .i = DEC_CPU_IRQ_NR(KN02CA_CPU_INR_RTC) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN02CA_CPU_INR_CASCADE) },
		{ .p = kn02xa_io_int } },
	{ { .i = DEC_CPU_IRQ_ALL },
		{ .p = cpu_all_int } },
};

static int_ptr kn02ca_asic_mask_nr_tbl[][2] __initdata = {
	{ { .i = IO_IRQ_DMA },
		{ .p = asic_dma_int } },
	{ { .i = IO_IRQ_MASK(KN02CA_IO_INR_SCC0) },
		{ .i = IO_IRQ_NR(KN02CA_IO_INR_SCC0) } },
	{ { .i = IO_IRQ_MASK(KN02CA_IO_INR_ASC) },
		{ .i = IO_IRQ_NR(KN02CA_IO_INR_ASC) } },
	{ { .i = IO_IRQ_MASK(KN02CA_IO_INR_LANCE) },
		{ .i = IO_IRQ_NR(KN02CA_IO_INR_LANCE) } },
	{ { .i = IO_IRQ_MASK(KN02CA_IO_INR_TC1) },
		{ .i = IO_IRQ_NR(KN02CA_IO_INR_TC1) } },
	{ { .i = IO_IRQ_MASK(KN02CA_IO_INR_TC0) },
		{ .i = IO_IRQ_NR(KN02CA_IO_INR_TC0) } },
	{ { .i = IO_IRQ_ALL },
		{ .p = asic_all_int } },
};

static void __init dec_init_kn02ca(void)
{
	
	memcpy(&dec_interrupt, &kn02ca_interrupt,
		sizeof(kn02ca_interrupt));

	
	memcpy(&cpu_mask_nr_tbl, &kn02ca_cpu_mask_nr_tbl,
		sizeof(kn02ca_cpu_mask_nr_tbl));

	
	memcpy(&asic_mask_nr_tbl, &kn02ca_asic_mask_nr_tbl,
		sizeof(kn02ca_asic_mask_nr_tbl));

	mips_cpu_irq_init();
	init_ioasic_irqs(IO_IRQ_BASE);

}				


static int kn03_interrupt[DEC_NR_INTS] __initdata = {
	[DEC_IRQ_CASCADE]	= DEC_CPU_IRQ_NR(KN03_CPU_INR_CASCADE),
	[DEC_IRQ_AB_RECV]	= -1,
	[DEC_IRQ_AB_XMIT]	= -1,
	[DEC_IRQ_DZ11]		= -1,
	[DEC_IRQ_ASC]		= IO_IRQ_NR(KN03_IO_INR_ASC),
	[DEC_IRQ_FLOPPY]	= -1,
	[DEC_IRQ_FPU]		= DEC_CPU_IRQ_NR(DEC_CPU_INR_FPU),
	[DEC_IRQ_HALT]		= DEC_CPU_IRQ_NR(KN03_CPU_INR_HALT),
	[DEC_IRQ_ISDN]		= -1,
	[DEC_IRQ_LANCE]		= IO_IRQ_NR(KN03_IO_INR_LANCE),
	[DEC_IRQ_BUS]		= DEC_CPU_IRQ_NR(KN03_CPU_INR_BUS),
	[DEC_IRQ_PSU]		= IO_IRQ_NR(KN03_IO_INR_PSU),
	[DEC_IRQ_RTC]		= DEC_CPU_IRQ_NR(KN03_CPU_INR_RTC),
	[DEC_IRQ_SCC0]		= IO_IRQ_NR(KN03_IO_INR_SCC0),
	[DEC_IRQ_SCC1]		= IO_IRQ_NR(KN03_IO_INR_SCC1),
	[DEC_IRQ_SII]		= -1,
	[DEC_IRQ_TC0]		= IO_IRQ_NR(KN03_IO_INR_TC0),
	[DEC_IRQ_TC1]		= IO_IRQ_NR(KN03_IO_INR_TC1),
	[DEC_IRQ_TC2]		= IO_IRQ_NR(KN03_IO_INR_TC2),
	[DEC_IRQ_TIMER]		= -1,
	[DEC_IRQ_VIDEO]		= -1,
	[DEC_IRQ_ASC_MERR]	= IO_IRQ_NR(IO_INR_ASC_MERR),
	[DEC_IRQ_ASC_ERR]	= IO_IRQ_NR(IO_INR_ASC_ERR),
	[DEC_IRQ_ASC_DMA]	= IO_IRQ_NR(IO_INR_ASC_DMA),
	[DEC_IRQ_FLOPPY_ERR]	= -1,
	[DEC_IRQ_ISDN_ERR]	= -1,
	[DEC_IRQ_ISDN_RXDMA]	= -1,
	[DEC_IRQ_ISDN_TXDMA]	= -1,
	[DEC_IRQ_LANCE_MERR]	= IO_IRQ_NR(IO_INR_LANCE_MERR),
	[DEC_IRQ_SCC0A_RXERR]	= IO_IRQ_NR(IO_INR_SCC0A_RXERR),
	[DEC_IRQ_SCC0A_RXDMA]	= IO_IRQ_NR(IO_INR_SCC0A_RXDMA),
	[DEC_IRQ_SCC0A_TXERR]	= IO_IRQ_NR(IO_INR_SCC0A_TXERR),
	[DEC_IRQ_SCC0A_TXDMA]	= IO_IRQ_NR(IO_INR_SCC0A_TXDMA),
	[DEC_IRQ_AB_RXERR]	= -1,
	[DEC_IRQ_AB_RXDMA]	= -1,
	[DEC_IRQ_AB_TXERR]	= -1,
	[DEC_IRQ_AB_TXDMA]	= -1,
	[DEC_IRQ_SCC1A_RXERR]	= IO_IRQ_NR(IO_INR_SCC1A_RXERR),
	[DEC_IRQ_SCC1A_RXDMA]	= IO_IRQ_NR(IO_INR_SCC1A_RXDMA),
	[DEC_IRQ_SCC1A_TXERR]	= IO_IRQ_NR(IO_INR_SCC1A_TXERR),
	[DEC_IRQ_SCC1A_TXDMA]	= IO_IRQ_NR(IO_INR_SCC1A_TXDMA),
};

static int_ptr kn03_cpu_mask_nr_tbl[][2] __initdata = {
	{ { .i = DEC_CPU_IRQ_MASK(KN03_CPU_INR_BUS) },
		{ .i = DEC_CPU_IRQ_NR(KN03_CPU_INR_BUS) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN03_CPU_INR_RTC) },
		{ .i = DEC_CPU_IRQ_NR(KN03_CPU_INR_RTC) } },
	{ { .i = DEC_CPU_IRQ_MASK(KN03_CPU_INR_CASCADE) },
		{ .p = kn03_io_int } },
	{ { .i = DEC_CPU_IRQ_ALL },
		{ .p = cpu_all_int } },
};

static int_ptr kn03_asic_mask_nr_tbl[][2] __initdata = {
	{ { .i = IO_IRQ_DMA },
		{ .p = asic_dma_int } },
	{ { .i = IO_IRQ_MASK(KN03_IO_INR_SCC0) },
		{ .i = IO_IRQ_NR(KN03_IO_INR_SCC0) } },
	{ { .i = IO_IRQ_MASK(KN03_IO_INR_SCC1) },
		{ .i = IO_IRQ_NR(KN03_IO_INR_SCC1) } },
	{ { .i = IO_IRQ_MASK(KN03_IO_INR_ASC) },
		{ .i = IO_IRQ_NR(KN03_IO_INR_ASC) } },
	{ { .i = IO_IRQ_MASK(KN03_IO_INR_LANCE) },
		{ .i = IO_IRQ_NR(KN03_IO_INR_LANCE) } },
	{ { .i = IO_IRQ_MASK(KN03_IO_INR_TC2) },
		{ .i = IO_IRQ_NR(KN03_IO_INR_TC2) } },
	{ { .i = IO_IRQ_MASK(KN03_IO_INR_TC1) },
		{ .i = IO_IRQ_NR(KN03_IO_INR_TC1) } },
	{ { .i = IO_IRQ_MASK(KN03_IO_INR_TC0) },
		{ .i = IO_IRQ_NR(KN03_IO_INR_TC0) } },
	{ { .i = IO_IRQ_ALL },
		{ .p = asic_all_int } },
};

static void __init dec_init_kn03(void)
{
	
	memcpy(&dec_interrupt, &kn03_interrupt,
		sizeof(kn03_interrupt));

	
	memcpy(&cpu_mask_nr_tbl, &kn03_cpu_mask_nr_tbl,
		sizeof(kn03_cpu_mask_nr_tbl));

	
	memcpy(&asic_mask_nr_tbl, &kn03_asic_mask_nr_tbl,
		sizeof(kn03_asic_mask_nr_tbl));

	mips_cpu_irq_init();
	init_ioasic_irqs(IO_IRQ_BASE);

}				


void __init arch_init_irq(void)
{
	switch (mips_machtype) {
	case MACH_DS23100:	
		dec_init_kn01();
		break;
	case MACH_DS5100:	
		dec_init_kn230();
		break;
	case MACH_DS5000_200:	
		dec_init_kn02();
		break;
	case MACH_DS5000_1XX:	
		dec_init_kn02ba();
		break;
	case MACH_DS5000_2X0:	
	case MACH_DS5900:	
		dec_init_kn03();
		break;
	case MACH_DS5000_XX:	
		dec_init_kn02ca();
		break;
	case MACH_DS5800:	
		panic("Don't know how to set this up!");
		break;
	case MACH_DS5400:	
		panic("Don't know how to set this up!");
		break;
	case MACH_DS5500:	
		panic("Don't know how to set this up!");
		break;
	}

	
	if (!cpu_has_nofpuex) {
		cpu_fpu_mask = 0;
		dec_interrupt[DEC_IRQ_FPU] = -1;
	}

	
	if (dec_interrupt[DEC_IRQ_FPU] >= 0)
		setup_irq(dec_interrupt[DEC_IRQ_FPU], &fpuirq);
	if (dec_interrupt[DEC_IRQ_CASCADE] >= 0)
		setup_irq(dec_interrupt[DEC_IRQ_CASCADE], &ioirq);

	
	if (dec_interrupt[DEC_IRQ_BUS] >= 0 && busirq.handler)
		setup_irq(dec_interrupt[DEC_IRQ_BUS], &busirq);

	
	if (dec_interrupt[DEC_IRQ_HALT] >= 0)
		setup_irq(dec_interrupt[DEC_IRQ_HALT], &haltirq);
}

asmlinkage unsigned int dec_irq_dispatch(unsigned int irq)
{
	do_IRQ(irq);
	return 0;
}
