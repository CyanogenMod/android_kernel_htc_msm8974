#include <linux/linkage.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/syscore_ops.h>
#include <linux/bitops.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <linux/atomic.h>
#include <asm/timer.h>
#include <asm/hw_irq.h>
#include <asm/pgtable.h>
#include <asm/desc.h>
#include <asm/apic.h>
#include <asm/i8259.h>

static void init_8259A(int auto_eoi);

static int i8259A_auto_eoi;
DEFINE_RAW_SPINLOCK(i8259A_lock);


unsigned int cached_irq_mask = 0xffff;

unsigned long io_apic_irqs;

static void mask_8259A_irq(unsigned int irq)
{
	unsigned int mask = 1 << irq;
	unsigned long flags;

	raw_spin_lock_irqsave(&i8259A_lock, flags);
	cached_irq_mask |= mask;
	if (irq & 8)
		outb(cached_slave_mask, PIC_SLAVE_IMR);
	else
		outb(cached_master_mask, PIC_MASTER_IMR);
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
}

static void disable_8259A_irq(struct irq_data *data)
{
	mask_8259A_irq(data->irq);
}

static void unmask_8259A_irq(unsigned int irq)
{
	unsigned int mask = ~(1 << irq);
	unsigned long flags;

	raw_spin_lock_irqsave(&i8259A_lock, flags);
	cached_irq_mask &= mask;
	if (irq & 8)
		outb(cached_slave_mask, PIC_SLAVE_IMR);
	else
		outb(cached_master_mask, PIC_MASTER_IMR);
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
}

static void enable_8259A_irq(struct irq_data *data)
{
	unmask_8259A_irq(data->irq);
}

static int i8259A_irq_pending(unsigned int irq)
{
	unsigned int mask = 1<<irq;
	unsigned long flags;
	int ret;

	raw_spin_lock_irqsave(&i8259A_lock, flags);
	if (irq < 8)
		ret = inb(PIC_MASTER_CMD) & mask;
	else
		ret = inb(PIC_SLAVE_CMD) & (mask >> 8);
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);

	return ret;
}

static void make_8259A_irq(unsigned int irq)
{
	disable_irq_nosync(irq);
	io_apic_irqs &= ~(1<<irq);
	irq_set_chip_and_handler_name(irq, &i8259A_chip, handle_level_irq,
				      i8259A_chip.name);
	enable_irq(irq);
}

static inline int i8259A_irq_real(unsigned int irq)
{
	int value;
	int irqmask = 1<<irq;

	if (irq < 8) {
		outb(0x0B, PIC_MASTER_CMD);	
		value = inb(PIC_MASTER_CMD) & irqmask;
		outb(0x0A, PIC_MASTER_CMD);	
		return value;
	}
	outb(0x0B, PIC_SLAVE_CMD);	
	value = inb(PIC_SLAVE_CMD) & (irqmask >> 8);
	outb(0x0A, PIC_SLAVE_CMD);	
	return value;
}

static void mask_and_ack_8259A(struct irq_data *data)
{
	unsigned int irq = data->irq;
	unsigned int irqmask = 1 << irq;
	unsigned long flags;

	raw_spin_lock_irqsave(&i8259A_lock, flags);
	if (cached_irq_mask & irqmask)
		goto spurious_8259A_irq;
	cached_irq_mask |= irqmask;

handle_real_irq:
	if (irq & 8) {
		inb(PIC_SLAVE_IMR);	
		outb(cached_slave_mask, PIC_SLAVE_IMR);
		
		outb(0x60+(irq&7), PIC_SLAVE_CMD);
		 
		outb(0x60+PIC_CASCADE_IR, PIC_MASTER_CMD);
	} else {
		inb(PIC_MASTER_IMR);	
		outb(cached_master_mask, PIC_MASTER_IMR);
		outb(0x60+irq, PIC_MASTER_CMD);	
	}
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
	return;

spurious_8259A_irq:
	if (i8259A_irq_real(irq))
		goto handle_real_irq;

	{
		static int spurious_irq_mask;
		if (!(spurious_irq_mask & irqmask)) {
			printk(KERN_DEBUG
			       "spurious 8259A interrupt: IRQ%d.\n", irq);
			spurious_irq_mask |= irqmask;
		}
		atomic_inc(&irq_err_count);
		goto handle_real_irq;
	}
}

struct irq_chip i8259A_chip = {
	.name		= "XT-PIC",
	.irq_mask	= disable_8259A_irq,
	.irq_disable	= disable_8259A_irq,
	.irq_unmask	= enable_8259A_irq,
	.irq_mask_ack	= mask_and_ack_8259A,
};

static char irq_trigger[2];
static void restore_ELCR(char *trigger)
{
	outb(trigger[0], 0x4d0);
	outb(trigger[1], 0x4d1);
}

static void save_ELCR(char *trigger)
{
	
	trigger[0] = inb(0x4d0) & 0xF8;
	trigger[1] = inb(0x4d1) & 0xDE;
}

static void i8259A_resume(void)
{
	init_8259A(i8259A_auto_eoi);
	restore_ELCR(irq_trigger);
}

static int i8259A_suspend(void)
{
	save_ELCR(irq_trigger);
	return 0;
}

static void i8259A_shutdown(void)
{
	outb(0xff, PIC_MASTER_IMR);	
	outb(0xff, PIC_SLAVE_IMR);	
}

static struct syscore_ops i8259_syscore_ops = {
	.suspend = i8259A_suspend,
	.resume = i8259A_resume,
	.shutdown = i8259A_shutdown,
};

static void mask_8259A(void)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&i8259A_lock, flags);

	outb(0xff, PIC_MASTER_IMR);	
	outb(0xff, PIC_SLAVE_IMR);	

	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
}

static void unmask_8259A(void)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&i8259A_lock, flags);

	outb(cached_master_mask, PIC_MASTER_IMR); 
	outb(cached_slave_mask, PIC_SLAVE_IMR);	  

	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
}

static void init_8259A(int auto_eoi)
{
	unsigned long flags;

	i8259A_auto_eoi = auto_eoi;

	raw_spin_lock_irqsave(&i8259A_lock, flags);

	outb(0xff, PIC_MASTER_IMR);	
	outb(0xff, PIC_SLAVE_IMR);	

	outb_pic(0x11, PIC_MASTER_CMD);	

	outb_pic(IRQ0_VECTOR, PIC_MASTER_IMR);

	
	outb_pic(1U << PIC_CASCADE_IR, PIC_MASTER_IMR);

	if (auto_eoi)	
		outb_pic(MASTER_ICW4_DEFAULT | PIC_ICW4_AEOI, PIC_MASTER_IMR);
	else		
		outb_pic(MASTER_ICW4_DEFAULT, PIC_MASTER_IMR);

	outb_pic(0x11, PIC_SLAVE_CMD);	

	
	outb_pic(IRQ8_VECTOR, PIC_SLAVE_IMR);
	
	outb_pic(PIC_CASCADE_IR, PIC_SLAVE_IMR);
	
	outb_pic(SLAVE_ICW4_DEFAULT, PIC_SLAVE_IMR);

	if (auto_eoi)
		i8259A_chip.irq_mask_ack = disable_8259A_irq;
	else
		i8259A_chip.irq_mask_ack = mask_and_ack_8259A;

	udelay(100);		

	outb(cached_master_mask, PIC_MASTER_IMR); 
	outb(cached_slave_mask, PIC_SLAVE_IMR);	  

	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
}


static void legacy_pic_noop(void) { };
static void legacy_pic_uint_noop(unsigned int unused) { };
static void legacy_pic_int_noop(int unused) { };
static int legacy_pic_irq_pending_noop(unsigned int irq)
{
	return 0;
}

struct legacy_pic null_legacy_pic = {
	.nr_legacy_irqs = 0,
	.chip = &dummy_irq_chip,
	.mask = legacy_pic_uint_noop,
	.unmask = legacy_pic_uint_noop,
	.mask_all = legacy_pic_noop,
	.restore_mask = legacy_pic_noop,
	.init = legacy_pic_int_noop,
	.irq_pending = legacy_pic_irq_pending_noop,
	.make_irq = legacy_pic_uint_noop,
};

struct legacy_pic default_legacy_pic = {
	.nr_legacy_irqs = NR_IRQS_LEGACY,
	.chip  = &i8259A_chip,
	.mask = mask_8259A_irq,
	.unmask = unmask_8259A_irq,
	.mask_all = mask_8259A,
	.restore_mask = unmask_8259A,
	.init = init_8259A,
	.irq_pending = i8259A_irq_pending,
	.make_irq = make_8259A_irq,
};

struct legacy_pic *legacy_pic = &default_legacy_pic;

static int __init i8259A_init_ops(void)
{
	if (legacy_pic == &default_legacy_pic)
		register_syscore_ops(&i8259_syscore_ops);

	return 0;
}

device_initcall(i8259A_init_ops);
