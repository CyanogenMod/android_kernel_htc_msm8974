/*
 *  SGI Visual Workstation support and quirks, unmaintained.
 *
 *  Split out from setup.c by davej@suse.de
 *
 *	Copyright (C) 1999 Bent Hagemark, Ingo Molnar
 *
 *  SGI Visual Workstation interrupt controller
 *
 *  The Cobalt system ASIC in the Visual Workstation contains a "Cobalt" APIC
 *  which serves as the main interrupt controller in the system.  Non-legacy
 *  hardware in the system uses this controller directly.  Legacy devices
 *  are connected to the PIIX4 which in turn has its 8259(s) connected to
 *  a of the Cobalt APIC entry.
 *
 *  09/02/2000 - Updated for 2.4 by jbarnes@sgi.com
 *
 *  25/11/2002 - Updated for 2.5 by Andrey Panin <pazke@orbita1.ru>
 */
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/visws/cobalt.h>
#include <asm/visws/piix4.h>
#include <asm/io_apic.h>
#include <asm/fixmap.h>
#include <asm/reboot.h>
#include <asm/setup.h>
#include <asm/apic.h>
#include <asm/e820.h>
#include <asm/time.h>
#include <asm/io.h>

#include <linux/kernel_stat.h>

#include <asm/i8259.h>
#include <asm/irq_vectors.h>
#include <asm/visws/lithium.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>

extern int no_broadcast;

char visws_board_type	= -1;
char visws_board_rev	= -1;

static void __init visws_time_init(void)
{
	printk(KERN_INFO "Starting Cobalt Timer system clock\n");

	
	co_cpu_write(CO_CPU_TIMEVAL, CO_TIME_HZ/HZ);

	
	co_cpu_write(CO_CPU_CTRL, co_cpu_read(CO_CPU_CTRL) | CO_CTRL_TIMERUN);

	
	co_cpu_write(CO_CPU_CTRL, co_cpu_read(CO_CPU_CTRL) & ~CO_CTRL_TIMEMASK);

	setup_default_timer_irq();
}

static void __init visws_pre_intr_init(void);


#define MB (1024 * 1024)

unsigned long sgivwfb_mem_phys;
unsigned long sgivwfb_mem_size;
EXPORT_SYMBOL(sgivwfb_mem_phys);
EXPORT_SYMBOL(sgivwfb_mem_size);

long long mem_size __initdata = 0;

static char * __init visws_memory_setup(void)
{
	long long gfx_mem_size = 8 * MB;

	mem_size = boot_params.alt_mem_k;

	if (!mem_size) {
		printk(KERN_WARNING "Bootloader didn't set memory size, upgrade it !\n");
		mem_size = 128 * MB;
	}

	if (!sgivwfb_mem_size) {
		printk(KERN_WARNING "Defaulting to 8 MB framebuffer size\n");
		sgivwfb_mem_size = 8 * MB;
	}

	sgivwfb_mem_size &= ~((1 << 20) - 1);
	sgivwfb_mem_phys = mem_size - gfx_mem_size;

	e820_add_region(0, LOWMEMSIZE(), E820_RAM);
	e820_add_region(HIGH_MEMORY, mem_size - sgivwfb_mem_size - HIGH_MEMORY, E820_RAM);
	e820_add_region(sgivwfb_mem_phys, sgivwfb_mem_size, E820_RESERVED);

	return "PROM";
}

static void visws_machine_emergency_restart(void)
{
	outb(PIIX4_RESET_VAL, PIIX4_RESET_PORT);
}

static void visws_machine_power_off(void)
{
	unsigned short pm_status;

	while ((pm_status = inw(PMSTS_PORT)) & 0x100)
		outw(pm_status, PMSTS_PORT);

	outw(PM_SUSPEND_ENABLE, PMCNTRL_PORT);

	mdelay(10);

#define PCI_CONF1_ADDRESS(bus, devfn, reg) \
	(0x80000000 | (bus << 16) | (devfn << 8) | (reg & ~3))

	outl(PIIX_SPECIAL_STOP, 0xCFC);
}

static void __init visws_get_smp_config(unsigned int early)
{
}


static void __init MP_processor_info(struct mpc_cpu *m)
{
	int ver, logical_apicid;
	physid_mask_t apic_cpus;

	if (!(m->cpuflag & CPU_ENABLED))
		return;

	logical_apicid = m->apicid;
	printk(KERN_INFO "%sCPU #%d %u:%u APIC version %d\n",
	       m->cpuflag & CPU_BOOTPROCESSOR ? "Bootup " : "",
	       m->apicid, (m->cpufeature & CPU_FAMILY_MASK) >> 8,
	       (m->cpufeature & CPU_MODEL_MASK) >> 4, m->apicver);

	if (m->cpuflag & CPU_BOOTPROCESSOR)
		boot_cpu_physical_apicid = m->apicid;

	ver = m->apicver;
	if ((ver >= 0x14 && m->apicid >= 0xff) || m->apicid >= 0xf) {
		printk(KERN_ERR "Processor #%d INVALID. (Max ID: %d).\n",
			m->apicid, MAX_LOCAL_APIC);
		return;
	}

	apic->apicid_to_cpu_present(m->apicid, &apic_cpus);
	physids_or(phys_cpu_present_map, phys_cpu_present_map, apic_cpus);
	if (ver == 0x0) {
		printk(KERN_ERR "BIOS bug, APIC version is 0 for CPU#%d! "
			"fixing up to 0x10. (tell your hw vendor)\n",
			m->apicid);
		ver = 0x10;
	}
	apic_version[m->apicid] = ver;
}

static void __init visws_find_smp_config(void)
{
	struct mpc_cpu *mp = phys_to_virt(CO_CPU_TAB_PHYS);
	unsigned short ncpus = readw(phys_to_virt(CO_CPU_NUM_PHYS));

	if (ncpus > CO_CPU_MAX) {
		printk(KERN_WARNING "find_visws_smp: got cpu count of %d at %p\n",
			ncpus, mp);

		ncpus = CO_CPU_MAX;
	}

	if (ncpus > setup_max_cpus)
		ncpus = setup_max_cpus;

#ifdef CONFIG_X86_LOCAL_APIC
	smp_found_config = 1;
#endif
	while (ncpus--)
		MP_processor_info(mp++);

	mp_lapic_addr = APIC_DEFAULT_PHYS_BASE;
}

static void visws_trap_init(void);

void __init visws_early_detect(void)
{
	int raw;

	visws_board_type = (char)(inb_p(PIIX_GPI_BD_REG) & PIIX_GPI_BD_REG)
							 >> PIIX_GPI_BD_SHIFT;

	if (visws_board_type < 0)
		return;

	x86_init.resources.memory_setup = visws_memory_setup;
	x86_init.mpparse.get_smp_config = visws_get_smp_config;
	x86_init.mpparse.find_smp_config = visws_find_smp_config;
	x86_init.irqs.pre_vector_init = visws_pre_intr_init;
	x86_init.irqs.trap_init = visws_trap_init;
	x86_init.timers.timer_init = visws_time_init;
	x86_init.pci.init = pci_visws_init;
	x86_init.pci.init_irq = x86_init_noop;

	pm_power_off			= visws_machine_power_off;
	machine_ops.emergency_restart	= visws_machine_emergency_restart;

	no_broadcast = 0;

#ifdef CONFIG_X86_IO_APIC
	skip_ioapic_setup		= 1;
#endif

	outb_p(SIO_DEV_SEL, SIO_INDEX);
	outb_p(SIO_GP_DEV, SIO_DATA);	

	outb_p(SIO_DEV_MSB, SIO_INDEX);
	outb_p(SIO_GP_MSB, SIO_DATA);	

	outb_p(SIO_DEV_LSB, SIO_INDEX);
	outb_p(SIO_GP_LSB, SIO_DATA);	

	outb_p(SIO_DEV_ENB, SIO_INDEX);
	outb_p(1, SIO_DATA);		

	outb_p(SIO_DEV_SEL, SIO_INDEX);
	outb_p(SIO_PM_DEV, SIO_DATA);	

	outb_p(SIO_DEV_MSB, SIO_INDEX);
	outb_p(SIO_PM_MSB, SIO_DATA);	

	outb_p(SIO_DEV_LSB, SIO_INDEX);
	outb_p(SIO_PM_LSB, SIO_DATA);	

	outb_p(SIO_DEV_ENB, SIO_INDEX);
	outb_p(1, SIO_DATA);		

	outb_p(SIO_PM_FER2, SIO_PM_INDEX);
	outb_p(SIO_PM_GP_EN, SIO_PM_DATA);

	raw = inb_p(SIO_GP_DATA1);
	raw &= 0x7f;	

	if (visws_board_type == VISWS_320) {
		if (raw < 0x6) {
			visws_board_rev = 4;
		} else if (raw < 0xc) {
			visws_board_rev = 5;
		} else {
			visws_board_rev = 6;
		}
	} else if (visws_board_type == VISWS_540) {
			visws_board_rev = 2;
		} else {
			visws_board_rev = raw;
		}

	printk(KERN_INFO "Silicon Graphics Visual Workstation %s (rev %d) detected\n",
	       (visws_board_type == VISWS_320 ? "320" :
	       (visws_board_type == VISWS_540 ? "540" :
		"unknown")), visws_board_rev);
}

#define A01234 (LI_INTA_0 | LI_INTA_1 | LI_INTA_2 | LI_INTA_3 | LI_INTA_4)
#define BCD (LI_INTB | LI_INTC | LI_INTD)
#define ALLDEVS (A01234 | BCD)

static __init void lithium_init(void)
{
	set_fixmap(FIX_LI_PCIA, LI_PCI_A_PHYS);
	set_fixmap(FIX_LI_PCIB, LI_PCI_B_PHYS);

	if ((li_pcia_read16(PCI_VENDOR_ID) != PCI_VENDOR_ID_SGI) ||
	    (li_pcia_read16(PCI_DEVICE_ID) != PCI_DEVICE_ID_SGI_LITHIUM)) {
		printk(KERN_EMERG "Lithium hostbridge %c not found\n", 'A');
	}

	if ((li_pcib_read16(PCI_VENDOR_ID) != PCI_VENDOR_ID_SGI) ||
	    (li_pcib_read16(PCI_DEVICE_ID) != PCI_DEVICE_ID_SGI_LITHIUM)) {
		printk(KERN_EMERG "Lithium hostbridge %c not found\n", 'B');
	}

	li_pcia_write16(LI_PCI_INTEN, ALLDEVS);
	li_pcib_write16(LI_PCI_INTEN, ALLDEVS);
}

static __init void cobalt_init(void)
{
	set_fixmap(FIX_APIC_BASE, APIC_DEFAULT_PHYS_BASE);
	setup_local_APIC();
	printk(KERN_INFO "Local APIC Version %#x, ID %#x\n",
		(unsigned int)apic_read(APIC_LVR),
		(unsigned int)apic_read(APIC_ID));

	set_fixmap(FIX_CO_CPU, CO_CPU_PHYS);
	set_fixmap(FIX_CO_APIC, CO_APIC_PHYS);
	printk(KERN_INFO "Cobalt Revision %#lx, APIC ID %#lx\n",
		co_cpu_read(CO_CPU_REV), co_apic_read(CO_APIC_ID));

	
	co_apic_write(CO_APIC_ID, co_apic_read(CO_APIC_ID) | CO_APIC_ENABLE);

	printk(KERN_INFO "Cobalt APIC enabled: ID reg %#lx\n",
		co_apic_read(CO_APIC_ID));
}

static void __init visws_trap_init(void)
{
	lithium_init();
	cobalt_init();
}


static DEFINE_SPINLOCK(cobalt_lock);

static inline void co_apic_set(int entry, int irq)
{
	co_apic_write(CO_APIC_LO(entry), CO_APIC_LEVEL | (irq + FIRST_EXTERNAL_VECTOR));
	co_apic_write(CO_APIC_HI(entry), 0);
}

static inline int co_apic_ide0_hack(void)
{
	extern char visws_board_type;
	extern char visws_board_rev;

	if (visws_board_type == VISWS_320 && visws_board_rev == 5)
		return 5;
	return CO_APIC_IDE0;
}

static int is_co_apic(unsigned int irq)
{
	if (IS_CO_APIC(irq))
		return CO_APIC(irq);

	switch (irq) {
		case 0: return CO_APIC_CPU;
		case CO_IRQ_IDE0: return co_apic_ide0_hack();
		case CO_IRQ_IDE1: return CO_APIC_IDE1;
		default: return -1;
	}
}


static void enable_cobalt_irq(struct irq_data *data)
{
	co_apic_set(is_co_apic(data->irq), data->irq);
}

static void disable_cobalt_irq(struct irq_data *data)
{
	int entry = is_co_apic(data->irq);

	co_apic_write(CO_APIC_LO(entry), CO_APIC_MASK);
	co_apic_read(CO_APIC_LO(entry));
}

static void ack_cobalt_irq(struct irq_data *data)
{
	unsigned long flags;

	spin_lock_irqsave(&cobalt_lock, flags);
	disable_cobalt_irq(data);
	apic_write(APIC_EOI, APIC_EIO_ACK);
	spin_unlock_irqrestore(&cobalt_lock, flags);
}

static struct irq_chip cobalt_irq_type = {
	.name		= "Cobalt-APIC",
	.irq_enable	= enable_cobalt_irq,
	.irq_disable	= disable_cobalt_irq,
	.irq_ack	= ack_cobalt_irq,
};


static unsigned int startup_piix4_master_irq(struct irq_data *data)
{
	legacy_pic->init(0);
	enable_cobalt_irq(data);
	return 0;
}

static struct irq_chip piix4_master_irq_type = {
	.name		= "PIIX4-master",
	.irq_startup	= startup_piix4_master_irq,
	.irq_ack	= ack_cobalt_irq,
};

static void pii4_mask(struct irq_data *data) { }

static struct irq_chip piix4_virtual_irq_type = {
	.name		= "PIIX4-virtual",
	.irq_mask	= pii4_mask,
};

static irqreturn_t piix4_master_intr(int irq, void *dev_id)
{
	unsigned long flags;
	int realirq;

	raw_spin_lock_irqsave(&i8259A_lock, flags);

	
	outb(0x0c, 0x20);		
	realirq = inb(0x20);

	if (unlikely(!(realirq & 0x80)))
		goto out_unlock;

	realirq &= 7;

	if (unlikely(realirq == 2)) {
		outb(0x0c, 0xa0);
		realirq = inb(0xa0);

		if (unlikely(!(realirq & 0x80)))
			goto out_unlock;

		realirq = (realirq & 7) + 8;
	}

	
	cached_irq_mask |= 1 << realirq;
	if (unlikely(realirq > 7)) {
		inb(0xa1);
		outb(cached_slave_mask, 0xa1);
		outb(0x60 + (realirq & 7), 0xa0);
		outb(0x60 + 2, 0x20);
	} else {
		inb(0x21);
		outb(cached_master_mask, 0x21);
		outb(0x60 + realirq, 0x20);
	}

	raw_spin_unlock_irqrestore(&i8259A_lock, flags);

	generic_handle_irq(realirq);

	return IRQ_HANDLED;

out_unlock:
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
	return IRQ_NONE;
}

static struct irqaction master_action = {
	.handler =	piix4_master_intr,
	.name =		"PIIX4-8259",
	.flags =	IRQF_NO_THREAD,
};

static struct irqaction cascade_action = {
	.handler = 	no_action,
	.name =		"cascade",
	.flags =	IRQF_NO_THREAD,
};

static inline void set_piix4_virtual_irq_type(void)
{
	piix4_virtual_irq_type.irq_enable = i8259A_chip.irq_unmask;
	piix4_virtual_irq_type.irq_disable = i8259A_chip.irq_mask;
	piix4_virtual_irq_type.irq_unmask = i8259A_chip.irq_unmask;
}

static void __init visws_pre_intr_init(void)
{
	int i;

	set_piix4_virtual_irq_type();

	for (i = 0; i < CO_IRQ_APIC0 + CO_APIC_LAST + 1; i++) {
		struct irq_chip *chip = NULL;

		if (i == 0)
			chip = &cobalt_irq_type;
		else if (i == CO_IRQ_IDE0)
			chip = &cobalt_irq_type;
		else if (i == CO_IRQ_IDE1)
			chip = &cobalt_irq_type;
		else if (i == CO_IRQ_8259)
			chip = &piix4_master_irq_type;
		else if (i < CO_IRQ_APIC0)
			chip = &piix4_virtual_irq_type;
		else if (IS_CO_APIC(i))
			chip = &cobalt_irq_type;

		if (chip)
			irq_set_chip(i, chip);
	}

	setup_irq(CO_IRQ_8259, &master_action);
	setup_irq(2, &cascade_action);
}
