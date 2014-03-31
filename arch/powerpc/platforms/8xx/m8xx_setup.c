/*
 *  Copyright (C) 1995  Linus Torvalds
 *  Adapted from 'alpha' version by Gary Thomas
 *  Modified by Cort Dougan (cort@cs.nmt.edu)
 *  Modified for MBX using prep/chrp/pmac functions by Dan (dmalek@jlc.net)
 *  Further modified for generic 8xx by Dan.
 */


#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/fsl_devices.h>

#include <asm/io.h>
#include <asm/mpc8xx.h>
#include <asm/8xx_immap.h>
#include <asm/prom.h>
#include <asm/fs_pd.h>
#include <mm/mmu_decl.h>

#include <sysdev/mpc8xx_pic.h>

#include "mpc8xx.h"

struct mpc8xx_pcmcia_ops m8xx_pcmcia_ops;

extern int cpm_pic_init(void);
extern int cpm_get_irq(void);

static irqreturn_t timebase_interrupt(int irq, void *dev)
{
	printk ("timebase_interrupt()\n");

	return IRQ_HANDLED;
}

static struct irqaction tbint_irqaction = {
	.handler = timebase_interrupt,
	.name = "tbint",
};

void __init __attribute__ ((weak))
init_internal_rtc(void)
{
	sit8xx_t __iomem *sys_tmr = immr_map(im_sit);

	
	clrbits16(&sys_tmr->sit_rtcsc, (RTCSC_SIE | RTCSC_ALE));

	
	setbits16(&sys_tmr->sit_rtcsc, (RTCSC_RTF | RTCSC_RTE));
	immr_unmap(sys_tmr);
}

static int __init get_freq(char *name, unsigned long *val)
{
	struct device_node *cpu;
	const unsigned int *fp;
	int found = 0;

	
	cpu = of_find_node_by_type(NULL, "cpu");

	if (cpu) {
		fp = of_get_property(cpu, name, NULL);
		if (fp) {
			found = 1;
			*val = *fp;
		}

		of_node_put(cpu);
	}

	return found;
}

void __init mpc8xx_calibrate_decr(void)
{
	struct device_node *cpu;
	cark8xx_t __iomem *clk_r1;
	car8xx_t __iomem *clk_r2;
	sitk8xx_t __iomem *sys_tmr1;
	sit8xx_t __iomem *sys_tmr2;
	int irq, virq;

	clk_r1 = immr_map(im_clkrstk);

	
	out_be32(&clk_r1->cark_sccrk, ~KAPWR_KEY);
	out_be32(&clk_r1->cark_sccrk, KAPWR_KEY);
	immr_unmap(clk_r1);

	
	clk_r2 = immr_map(im_clkrst);
	setbits32(&clk_r2->car_sccr, 0x02000000);
	immr_unmap(clk_r2);

	ppc_proc_freq = 50000000;
	if (!get_freq("clock-frequency", &ppc_proc_freq))
		printk(KERN_ERR "WARNING: Estimating processor frequency "
		                "(not found)\n");

	ppc_tb_freq = ppc_proc_freq / 16;
	printk("Decrementer Frequency = 0x%lx\n", ppc_tb_freq);

	sys_tmr1 = immr_map(im_sitk);
	out_be32(&sys_tmr1->sitk_tbscrk, ~KAPWR_KEY);
	out_be32(&sys_tmr1->sitk_rtcsck, ~KAPWR_KEY);
	out_be32(&sys_tmr1->sitk_tbk, ~KAPWR_KEY);
	out_be32(&sys_tmr1->sitk_tbscrk, KAPWR_KEY);
	out_be32(&sys_tmr1->sitk_rtcsck, KAPWR_KEY);
	out_be32(&sys_tmr1->sitk_tbk, KAPWR_KEY);
	immr_unmap(sys_tmr1);

	init_internal_rtc();

	cpu = of_find_node_by_type(NULL, "cpu");
	virq= irq_of_parse_and_map(cpu, 0);
	irq = virq_to_hw(virq);

	sys_tmr2 = immr_map(im_sit);
	out_be16(&sys_tmr2->sit_tbscr, ((1 << (7 - (irq/2))) << 8) |
					(TBSCR_TBF | TBSCR_TBE));
	immr_unmap(sys_tmr2);

	if (setup_irq(virq, &tbint_irqaction))
		panic("Could not allocate timer IRQ!");
}


int mpc8xx_set_rtc_time(struct rtc_time *tm)
{
	sitk8xx_t __iomem *sys_tmr1;
	sit8xx_t __iomem *sys_tmr2;
	int time;

	sys_tmr1 = immr_map(im_sitk);
	sys_tmr2 = immr_map(im_sit);
	time = mktime(tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
	              tm->tm_hour, tm->tm_min, tm->tm_sec);

	out_be32(&sys_tmr1->sitk_rtck, KAPWR_KEY);
	out_be32(&sys_tmr2->sit_rtc, time);
	out_be32(&sys_tmr1->sitk_rtck, ~KAPWR_KEY);

	immr_unmap(sys_tmr2);
	immr_unmap(sys_tmr1);
	return 0;
}

void mpc8xx_get_rtc_time(struct rtc_time *tm)
{
	unsigned long data;
	sit8xx_t __iomem *sys_tmr = immr_map(im_sit);

	
	data = in_be32(&sys_tmr->sit_rtc);
	to_tm(data, tm);
	tm->tm_year -= 1900;
	tm->tm_mon -= 1;
	immr_unmap(sys_tmr);
	return;
}

void mpc8xx_restart(char *cmd)
{
	car8xx_t __iomem *clk_r = immr_map(im_clkrst);


	local_irq_disable();

	setbits32(&clk_r->car_plprcr, 0x00000080);
	mtmsr(mfmsr() & ~0x1000);

	in_8(&clk_r->res[0]);
	panic("Restart failed\n");
}

static void cpm_cascade(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip;
	int cascade_irq;

	if ((cascade_irq = cpm_get_irq()) >= 0) {
		struct irq_desc *cdesc = irq_to_desc(cascade_irq);

		generic_handle_irq(cascade_irq);

		chip = irq_desc_get_chip(cdesc);
		chip->irq_eoi(&cdesc->irq_data);
	}

	chip = irq_desc_get_chip(desc);
	chip->irq_eoi(&desc->irq_data);
}

void __init mpc8xx_pics_init(void)
{
	int irq;

	if (mpc8xx_pic_init()) {
		printk(KERN_ERR "Failed interrupt 8xx controller  initialization\n");
		return;
	}

	irq = cpm_pic_init();
	if (irq != NO_IRQ)
		irq_set_chained_handler(irq, cpm_cascade);
}
