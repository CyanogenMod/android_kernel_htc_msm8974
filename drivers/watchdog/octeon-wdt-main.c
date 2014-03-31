/*
 * Octeon Watchdog driver
 *
 * Copyright (C) 2007, 2008, 2009, 2010 Cavium Networks
 *
 * Some parts derived from wdt.c
 *
 *	(c) Copyright 1996-1997 Alan Cox <alan@lxorguk.ukuu.org.uk>,
 *						All Rights Reserved.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Neither Alan Cox nor CymruNet Ltd. admit liability nor provide
 *	warranty for any of this software. This material is provided
 *	"AS-IS" and at no charge.
 *
 *	(c) Copyright 1995    Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 *
 * The OCTEON watchdog has a maximum timeout of 2^32 * io_clock.
 * For most systems this is less than 10 seconds, so to allow for
 * software to request longer watchdog heartbeats, we maintain software
 * counters to count multiples of the base rate.  If the system locks
 * up in such a manner that we can not run the software counters, the
 * only result is a watchdog reset sooner than was requested.  But
 * that is OK, because in this case userspace would likely not be able
 * to do anything anyhow.
 *
 * The hardware watchdog interval we call the period.  The OCTEON
 * watchdog goes through several stages, after the first period an
 * irq is asserted, then if it is not reset, after the next period NMI
 * is asserted, then after an additional period a chip wide soft reset.
 * So for the software counters, we reset watchdog after each period
 * and decrement the counter.  But for the last two periods we need to
 * let the watchdog progress to the NMI stage so we disable the irq
 * and let it proceed.  Once in the NMI, we print the register state
 * to the serial port and then wait for the reset.
 *
 * A watchdog is maintained for each CPU in the system, that way if
 * one CPU suffers a lockup, we also get a register dump and reset.
 * The userspace ping resets the watchdog on all CPUs.
 *
 * Before userspace opens the watchdog device, we still run the
 * watchdogs to catch any lockups that may be kernel related.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/watchdog.h>
#include <linux/cpumask.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/fs.h>
#include <linux/irq.h>

#include <asm/mipsregs.h>
#include <asm/uasm.h>

#include <asm/octeon/octeon.h>

static unsigned int timeout_cnt;

static unsigned int max_timeout_sec;

static unsigned int timeout_sec;

static int do_coundown;
static unsigned int countdown_reset;
static unsigned int per_cpu_countdown[NR_CPUS];

static cpumask_t irq_enabled_cpus;

#define WD_TIMO 60			

static int heartbeat = WD_TIMO;
module_param(heartbeat, int, S_IRUGO);
MODULE_PARM_DESC(heartbeat,
	"Watchdog heartbeat in seconds. (0 < heartbeat, default="
				__MODULE_STRING(WD_TIMO) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, S_IRUGO);
MODULE_PARM_DESC(nowayout,
	"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static unsigned long octeon_wdt_is_open;
static char expect_close;

static u32 __initdata nmi_stage1_insns[64];
static struct uasm_label __initdata labels[5];
static struct uasm_reloc __initdata relocs[5];

enum lable_id {
	label_enter_bootloader = 1
};

#define K0		26
#define C0_CVMMEMCTL 11, 7
#define C0_STATUS 12, 0
#define C0_EBASE 15, 1
#define C0_DESAVE 31, 0

void octeon_wdt_nmi_stage2(void);

static void __init octeon_wdt_build_stage1(void)
{
	int i;
	int len;
	u32 *p = nmi_stage1_insns;
#ifdef CONFIG_HOTPLUG_CPU
	struct uasm_label *l = labels;
	struct uasm_reloc *r = relocs;
#endif

	uasm_i_dmtc0(&p, K0, C0_DESAVE);

	uasm_i_mfc0(&p, K0, C0_STATUS);
#ifdef CONFIG_HOTPLUG_CPU
	uasm_il_bbit0(&p, &r, K0, ilog2(ST0_NMI), label_enter_bootloader);
#endif
	
	uasm_i_ori(&p, K0, K0, ST0_UX | ST0_SX | ST0_KX);
	uasm_i_mtc0(&p, K0, C0_STATUS);

#ifdef CONFIG_HOTPLUG_CPU
	uasm_i_mfc0(&p, K0, C0_EBASE);
	
	uasm_i_andi(&p, K0, K0, 0xf);
	
	uasm_i_dsll_safe(&p, K0, K0, 3 + 16);
	uasm_i_ori(&p, K0, K0, 0x8001);
	uasm_i_dsll_safe(&p, K0, K0, 16);
	uasm_i_ori(&p, K0, K0, 0x0700);
	uasm_i_drotr_safe(&p, K0, K0, 32);
	uasm_i_ld(&p, K0, 0x500, K0);
	uasm_il_bbit0(&p, &r, K0, 1, label_enter_bootloader);
	uasm_i_nop(&p);
#endif

	
	uasm_i_cache(&p, 1, 0, 0);

	
	uasm_i_dmfc0(&p, K0, C0_CVMMEMCTL);
	
	uasm_i_dins(&p, K0, 0, 0, 6);
	
	uasm_i_ori(&p, K0, K0, 0x1c0 | 54);
	
	uasm_i_dmtc0(&p, K0, C0_CVMMEMCTL);

	
	UASM_i_LA(&p, K0, (long)octeon_wdt_nmi_stage2);
	uasm_i_jr(&p, K0);
	uasm_i_dmfc0(&p, K0, C0_DESAVE);

#ifdef CONFIG_HOTPLUG_CPU
	uasm_build_label(&l, p, label_enter_bootloader);
	
	UASM_i_LA(&p, K0, (long)octeon_bootloader_entry_addr);
	uasm_i_jr(&p, K0);
	uasm_i_dmfc0(&p, K0, C0_DESAVE);
#endif
	uasm_resolve_relocs(relocs, labels);

	len = (int)(p - nmi_stage1_insns);
	pr_debug("Synthesized NMI stage 1 handler (%d instructions)\n", len);

	pr_debug("\t.set push\n");
	pr_debug("\t.set noreorder\n");
	for (i = 0; i < len; i++)
		pr_debug("\t.word 0x%08x\n", nmi_stage1_insns[i]);
	pr_debug("\t.set pop\n");

	if (len > 32)
		panic("NMI stage 1 handler exceeds 32 instructions, was %d\n", len);
}

static int cpu2core(int cpu)
{
#ifdef CONFIG_SMP
	return cpu_logical_map(cpu);
#else
	return cvmx_get_core_num();
#endif
}

static int core2cpu(int coreid)
{
#ifdef CONFIG_SMP
	return cpu_number_map(coreid);
#else
	return 0;
#endif
}

static irqreturn_t octeon_wdt_poke_irq(int cpl, void *dev_id)
{
	unsigned int core = cvmx_get_core_num();
	int cpu = core2cpu(core);

	if (do_coundown) {
		if (per_cpu_countdown[cpu] > 0) {
			
			cvmx_write_csr(CVMX_CIU_PP_POKEX(core), 1);
			per_cpu_countdown[cpu]--;
		} else {
			
			disable_irq_nosync(cpl);
			cpumask_clear_cpu(cpu, &irq_enabled_cpus);
		}
	} else {
		
		cvmx_write_csr(CVMX_CIU_PP_POKEX(core), 1);
	}
	return IRQ_HANDLED;
}

extern int prom_putchar(char c);

static void octeon_wdt_write_string(const char *str)
{
	
	while (*str)
		prom_putchar(*str++);
}

static void octeon_wdt_write_hex(u64 value, int digits)
{
	int d;
	int v;
	for (d = 0; d < digits; d++) {
		v = (value >> ((digits - d - 1) * 4)) & 0xf;
		if (v >= 10)
			prom_putchar('a' + v - 10);
		else
			prom_putchar('0' + v);
	}
}

const char *reg_name[] = {
	"$0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
	"a4", "a5", "a6", "a7", "t0", "t1", "t2", "t3",
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra"
};

void octeon_wdt_nmi_stage3(u64 reg[32])
{
	u64 i;

	unsigned int coreid = cvmx_get_core_num();
	u64 cp0_cause = read_c0_cause();
	u64 cp0_status = read_c0_status();
	u64 cp0_error_epc = read_c0_errorepc();
	u64 cp0_epc = read_c0_epc();

	
	__delay(100000000ull * coreid);

	octeon_wdt_write_string("\r\n*** NMI Watchdog interrupt on Core 0x");
	octeon_wdt_write_hex(coreid, 1);
	octeon_wdt_write_string(" ***\r\n");
	for (i = 0; i < 32; i++) {
		octeon_wdt_write_string("\t");
		octeon_wdt_write_string(reg_name[i]);
		octeon_wdt_write_string("\t0x");
		octeon_wdt_write_hex(reg[i], 16);
		if (i & 1)
			octeon_wdt_write_string("\r\n");
	}
	octeon_wdt_write_string("\terr_epc\t0x");
	octeon_wdt_write_hex(cp0_error_epc, 16);

	octeon_wdt_write_string("\tepc\t0x");
	octeon_wdt_write_hex(cp0_epc, 16);
	octeon_wdt_write_string("\r\n");

	octeon_wdt_write_string("\tstatus\t0x");
	octeon_wdt_write_hex(cp0_status, 16);
	octeon_wdt_write_string("\tcause\t0x");
	octeon_wdt_write_hex(cp0_cause, 16);
	octeon_wdt_write_string("\r\n");

	octeon_wdt_write_string("\tsum0\t0x");
	octeon_wdt_write_hex(cvmx_read_csr(CVMX_CIU_INTX_SUM0(coreid * 2)), 16);
	octeon_wdt_write_string("\ten0\t0x");
	octeon_wdt_write_hex(cvmx_read_csr(CVMX_CIU_INTX_EN0(coreid * 2)), 16);
	octeon_wdt_write_string("\r\n");

	octeon_wdt_write_string("*** Chip soft reset soon ***\r\n");
}

static void octeon_wdt_disable_interrupt(int cpu)
{
	unsigned int core;
	unsigned int irq;
	union cvmx_ciu_wdogx ciu_wdog;

	core = cpu2core(cpu);

	irq = OCTEON_IRQ_WDOG0 + core;

	
	cvmx_write_csr(CVMX_CIU_PP_POKEX(core), 1);

	
	ciu_wdog.u64 = 0;
	cvmx_write_csr(CVMX_CIU_WDOGX(core), ciu_wdog.u64);

	free_irq(irq, octeon_wdt_poke_irq);
}

static void octeon_wdt_setup_interrupt(int cpu)
{
	unsigned int core;
	unsigned int irq;
	union cvmx_ciu_wdogx ciu_wdog;

	core = cpu2core(cpu);

	
	ciu_wdog.u64 = 0;
	cvmx_write_csr(CVMX_CIU_WDOGX(core), ciu_wdog.u64);

	per_cpu_countdown[cpu] = countdown_reset;

	irq = OCTEON_IRQ_WDOG0 + core;

	if (request_irq(irq, octeon_wdt_poke_irq,
			IRQF_NO_THREAD, "octeon_wdt", octeon_wdt_poke_irq))
		panic("octeon_wdt: Couldn't obtain irq %d", irq);

	cpumask_set_cpu(cpu, &irq_enabled_cpus);

	
	cvmx_write_csr(CVMX_CIU_PP_POKEX(core), 1);

	
	ciu_wdog.u64 = 0;
	ciu_wdog.s.len = timeout_cnt;
	ciu_wdog.s.mode = 3;	
	cvmx_write_csr(CVMX_CIU_WDOGX(core), ciu_wdog.u64);
}

static int octeon_wdt_cpu_callback(struct notifier_block *nfb,
					   unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_DOWN_PREPARE:
		octeon_wdt_disable_interrupt(cpu);
		break;
	case CPU_ONLINE:
	case CPU_DOWN_FAILED:
		octeon_wdt_setup_interrupt(cpu);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static void octeon_wdt_ping(void)
{
	int cpu;
	int coreid;

	for_each_online_cpu(cpu) {
		coreid = cpu2core(cpu);
		cvmx_write_csr(CVMX_CIU_PP_POKEX(coreid), 1);
		per_cpu_countdown[cpu] = countdown_reset;
		if ((countdown_reset || !do_coundown) &&
		    !cpumask_test_cpu(cpu, &irq_enabled_cpus)) {
			
			int irq = OCTEON_IRQ_WDOG0 + coreid;
			enable_irq(irq);
			cpumask_set_cpu(cpu, &irq_enabled_cpus);
		}
	}
}

static void octeon_wdt_calc_parameters(int t)
{
	unsigned int periods;

	timeout_sec = max_timeout_sec;


	while ((t % timeout_sec) != 0)
		timeout_sec--;

	periods = t / timeout_sec;


	countdown_reset = periods > 2 ? periods - 2 : 0;
	heartbeat = t;
	timeout_cnt = ((octeon_get_io_clock_rate() >> 8) * timeout_sec) >> 8;
}

static int octeon_wdt_set_heartbeat(int t)
{
	int cpu;
	int coreid;
	union cvmx_ciu_wdogx ciu_wdog;

	if (t <= 0)
		return -1;

	octeon_wdt_calc_parameters(t);

	for_each_online_cpu(cpu) {
		coreid = cpu2core(cpu);
		cvmx_write_csr(CVMX_CIU_PP_POKEX(coreid), 1);
		ciu_wdog.u64 = 0;
		ciu_wdog.s.len = timeout_cnt;
		ciu_wdog.s.mode = 3;	
		cvmx_write_csr(CVMX_CIU_WDOGX(coreid), ciu_wdog.u64);
		cvmx_write_csr(CVMX_CIU_PP_POKEX(coreid), 1);
	}
	octeon_wdt_ping(); 
	return 0;
}


static ssize_t octeon_wdt_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	if (count) {
		if (!nowayout) {
			size_t i;

			
			expect_close = 0;

			for (i = 0; i != count; i++) {
				char c;
				if (get_user(c, buf + i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 1;
			}
		}
		octeon_wdt_ping();
	}
	return count;
}


static long octeon_wdt_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	int new_heartbeat;

	static struct watchdog_info ident = {
		.options =		WDIOF_SETTIMEOUT|
					WDIOF_MAGICCLOSE|
					WDIOF_KEEPALIVEPING,
		.firmware_version =	1,
		.identity =		"OCTEON",
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(argp, &ident, sizeof(ident)) ? -EFAULT : 0;
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, p);
	case WDIOC_KEEPALIVE:
		octeon_wdt_ping();
		return 0;
	case WDIOC_SETTIMEOUT:
		if (get_user(new_heartbeat, p))
			return -EFAULT;
		if (octeon_wdt_set_heartbeat(new_heartbeat))
			return -EINVAL;
		
	case WDIOC_GETTIMEOUT:
		return put_user(heartbeat, p);
	default:
		return -ENOTTY;
	}
}


static int octeon_wdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &octeon_wdt_is_open))
		return -EBUSY;
	octeon_wdt_ping();
	do_coundown = 1;
	return nonseekable_open(inode, file);
}


static int octeon_wdt_release(struct inode *inode, struct file *file)
{
	if (expect_close) {
		do_coundown = 0;
		octeon_wdt_ping();
	} else {
		pr_crit("WDT device closed unexpectedly.  WDT will not stop!\n");
	}
	clear_bit(0, &octeon_wdt_is_open);
	expect_close = 0;
	return 0;
}

static const struct file_operations octeon_wdt_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= octeon_wdt_write,
	.unlocked_ioctl	= octeon_wdt_ioctl,
	.open		= octeon_wdt_open,
	.release	= octeon_wdt_release,
};

static struct miscdevice octeon_wdt_miscdev = {
	.minor	= WATCHDOG_MINOR,
	.name	= "watchdog",
	.fops	= &octeon_wdt_fops,
};

static struct notifier_block octeon_wdt_cpu_notifier = {
	.notifier_call = octeon_wdt_cpu_callback,
};


static int __init octeon_wdt_init(void)
{
	int i;
	int ret;
	int cpu;
	u64 *ptr;

	max_timeout_sec = 6;
	do {
		max_timeout_sec--;
		timeout_cnt = ((octeon_get_io_clock_rate() >> 8) * max_timeout_sec) >> 8;
	} while (timeout_cnt > 65535);

	BUG_ON(timeout_cnt == 0);

	octeon_wdt_calc_parameters(heartbeat);

	pr_info("Initial granularity %d Sec\n", timeout_sec);

	ret = misc_register(&octeon_wdt_miscdev);
	if (ret) {
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
		       WATCHDOG_MINOR, ret);
		goto out;
	}

	
	octeon_wdt_build_stage1();

	
	ptr = (u64 *) nmi_stage1_insns;
	for (i = 0; i < 16; i++) {
		cvmx_write_csr(CVMX_MIO_BOOT_LOC_ADR, i * 8);
		cvmx_write_csr(CVMX_MIO_BOOT_LOC_DAT, ptr[i]);
	}
	cvmx_write_csr(CVMX_MIO_BOOT_LOC_CFGX(0), 0x81fc0000);

	cpumask_clear(&irq_enabled_cpus);

	for_each_online_cpu(cpu)
		octeon_wdt_setup_interrupt(cpu);

	register_hotcpu_notifier(&octeon_wdt_cpu_notifier);
out:
	return ret;
}

static void __exit octeon_wdt_cleanup(void)
{
	int cpu;

	misc_deregister(&octeon_wdt_miscdev);

	unregister_hotcpu_notifier(&octeon_wdt_cpu_notifier);

	for_each_online_cpu(cpu) {
		int core = cpu2core(cpu);
		
		cvmx_write_csr(CVMX_CIU_WDOGX(core), 0);
		
		free_irq(OCTEON_IRQ_WDOG0 + core, octeon_wdt_poke_irq);
	}
	cvmx_write_csr(CVMX_MIO_BOOT_LOC_CFGX(0), 0);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cavium Networks <support@caviumnetworks.com>");
MODULE_DESCRIPTION("Cavium Networks Octeon Watchdog driver.");
module_init(octeon_wdt_init);
module_exit(octeon_wdt_cleanup);
