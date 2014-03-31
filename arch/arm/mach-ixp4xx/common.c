/*
 * arch/arm/mach-ixp4xx/common.c
 *
 * Generic code shared across all IXP4XX platforms
 *
 * Maintainer: Deepak Saxena <dsaxena@plexity.net>
 *
 * Copyright 2002 (c) Intel Corporation
 * Copyright 2003-2004 (c) MontaVista, Software, Inc. 
 * 
 * This file is licensed under  the terms of the GNU General Public 
 * License version 2. This program is licensed "as is" without any 
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/io.h>
#include <linux/export.h>

#include <mach/udc.h>
#include <mach/hardware.h>
#include <mach/io.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/sched_clock.h>
#include <asm/system_misc.h>

#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>

static void __init ixp4xx_clocksource_init(void);
static void __init ixp4xx_clockevent_init(void);
static struct clock_event_device clockevent_ixp4xx;

static struct map_desc ixp4xx_io_desc[] __initdata = {
	{	
		.virtual	= IXP4XX_PERIPHERAL_BASE_VIRT,
		.pfn		= __phys_to_pfn(IXP4XX_PERIPHERAL_BASE_PHYS),
		.length		= IXP4XX_PERIPHERAL_REGION_SIZE,
		.type		= MT_DEVICE
	}, {	
		.virtual	= IXP4XX_EXP_CFG_BASE_VIRT,
		.pfn		= __phys_to_pfn(IXP4XX_EXP_CFG_BASE_PHYS),
		.length		= IXP4XX_EXP_CFG_REGION_SIZE,
		.type		= MT_DEVICE
	}, {	
		.virtual	= IXP4XX_PCI_CFG_BASE_VIRT,
		.pfn		= __phys_to_pfn(IXP4XX_PCI_CFG_BASE_PHYS),
		.length		= IXP4XX_PCI_CFG_REGION_SIZE,
		.type		= MT_DEVICE
	},
#ifdef CONFIG_DEBUG_LL
	{	
		.virtual	= IXP4XX_DEBUG_UART_BASE_VIRT,
		.pfn		= __phys_to_pfn(IXP4XX_DEBUG_UART_BASE_PHYS),
		.length		= IXP4XX_DEBUG_UART_REGION_SIZE,
		.type		= MT_DEVICE
	}
#endif
};

void __init ixp4xx_map_io(void)
{
  	iotable_init(ixp4xx_io_desc, ARRAY_SIZE(ixp4xx_io_desc));
}


enum ixp4xx_irq_type {
	IXP4XX_IRQ_LEVEL, IXP4XX_IRQ_EDGE
};

static unsigned long long ixp4xx_irq_edge = 0;

static signed char irq2gpio[32] = {
	-1, -1, -1, -1, -1, -1,  0,  1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1,  2,  3,  4,  5,  6,
	 7,  8,  9, 10, 11, 12, -1, -1,
};

int gpio_to_irq(int gpio)
{
	int irq;

	for (irq = 0; irq < 32; irq++) {
		if (irq2gpio[irq] == gpio)
			return irq;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(gpio_to_irq);

int irq_to_gpio(unsigned int irq)
{
	int gpio = (irq < 32) ? irq2gpio[irq] : -EINVAL;

	if (gpio == -1)
		return -EINVAL;

	return gpio;
}
EXPORT_SYMBOL(irq_to_gpio);

static int ixp4xx_set_irq_type(struct irq_data *d, unsigned int type)
{
	int line = irq2gpio[d->irq];
	u32 int_style;
	enum ixp4xx_irq_type irq_type;
	volatile u32 *int_reg;

	if (line < 0)
		return -EINVAL;

	switch (type){
	case IRQ_TYPE_EDGE_BOTH:
		int_style = IXP4XX_GPIO_STYLE_TRANSITIONAL;
		irq_type = IXP4XX_IRQ_EDGE;
		break;
	case IRQ_TYPE_EDGE_RISING:
		int_style = IXP4XX_GPIO_STYLE_RISING_EDGE;
		irq_type = IXP4XX_IRQ_EDGE;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		int_style = IXP4XX_GPIO_STYLE_FALLING_EDGE;
		irq_type = IXP4XX_IRQ_EDGE;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		int_style = IXP4XX_GPIO_STYLE_ACTIVE_HIGH;
		irq_type = IXP4XX_IRQ_LEVEL;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		int_style = IXP4XX_GPIO_STYLE_ACTIVE_LOW;
		irq_type = IXP4XX_IRQ_LEVEL;
		break;
	default:
		return -EINVAL;
	}

	if (irq_type == IXP4XX_IRQ_EDGE)
		ixp4xx_irq_edge |= (1 << d->irq);
	else
		ixp4xx_irq_edge &= ~(1 << d->irq);

	if (line >= 8) {	
		line -= 8;
		int_reg = IXP4XX_GPIO_GPIT2R;
	} else {		
		int_reg = IXP4XX_GPIO_GPIT1R;
	}

	
	*int_reg &= ~(IXP4XX_GPIO_STYLE_CLEAR <<
	    		(line * IXP4XX_GPIO_STYLE_SIZE));

	*IXP4XX_GPIO_GPISR = (1 << line);

	
	*int_reg |= (int_style << (line * IXP4XX_GPIO_STYLE_SIZE));

	
	gpio_line_config(irq2gpio[d->irq], IXP4XX_GPIO_IN);

	return 0;
}

static void ixp4xx_irq_mask(struct irq_data *d)
{
	if ((cpu_is_ixp46x() || cpu_is_ixp43x()) && d->irq >= 32)
		*IXP4XX_ICMR2 &= ~(1 << (d->irq - 32));
	else
		*IXP4XX_ICMR &= ~(1 << d->irq);
}

static void ixp4xx_irq_ack(struct irq_data *d)
{
	int line = (d->irq < 32) ? irq2gpio[d->irq] : -1;

	if (line >= 0)
		*IXP4XX_GPIO_GPISR = (1 << line);
}

static void ixp4xx_irq_unmask(struct irq_data *d)
{
	if (!(ixp4xx_irq_edge & (1 << d->irq)))
		ixp4xx_irq_ack(d);

	if ((cpu_is_ixp46x() || cpu_is_ixp43x()) && d->irq >= 32)
		*IXP4XX_ICMR2 |= (1 << (d->irq - 32));
	else
		*IXP4XX_ICMR |= (1 << d->irq);
}

static struct irq_chip ixp4xx_irq_chip = {
	.name		= "IXP4xx",
	.irq_ack	= ixp4xx_irq_ack,
	.irq_mask	= ixp4xx_irq_mask,
	.irq_unmask	= ixp4xx_irq_unmask,
	.irq_set_type	= ixp4xx_set_irq_type,
};

void __init ixp4xx_init_irq(void)
{
	int i = 0;

	disable_hlt();

	
	*IXP4XX_ICLR = 0x0;

	
	*IXP4XX_ICMR = 0x0; 

	if (cpu_is_ixp46x() || cpu_is_ixp43x()) {
		
		*IXP4XX_ICLR2 = 0x00;

		
		*IXP4XX_ICMR2 = 0x00;
	}

        
	for(i = 0; i < NR_IRQS; i++) {
		irq_set_chip_and_handler(i, &ixp4xx_irq_chip,
					 handle_level_irq);
		set_irq_flags(i, IRQF_VALID);
	}
}



static irqreturn_t ixp4xx_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	
	*IXP4XX_OSST = IXP4XX_OSST_TIMER_1_PEND;

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct irqaction ixp4xx_timer_irq = {
	.name		= "timer1",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= ixp4xx_timer_interrupt,
	.dev_id		= &clockevent_ixp4xx,
};

void __init ixp4xx_timer_init(void)
{
	
	*IXP4XX_OSRT1 = 0;

	
	*IXP4XX_OSST = IXP4XX_OSST_TIMER_1_PEND;

	
	*IXP4XX_OSTS = 0;

	
	setup_irq(IRQ_IXP4XX_TIMER1, &ixp4xx_timer_irq);

	ixp4xx_clocksource_init();
	ixp4xx_clockevent_init();
}

struct sys_timer ixp4xx_timer = {
	.init		= ixp4xx_timer_init,
};

static struct pxa2xx_udc_mach_info ixp4xx_udc_info;

void __init ixp4xx_set_udc_info(struct pxa2xx_udc_mach_info *info)
{
	memcpy(&ixp4xx_udc_info, info, sizeof *info);
}

static struct resource ixp4xx_udc_resources[] = {
	[0] = {
		.start  = 0xc800b000,
		.end    = 0xc800bfff,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_IXP4XX_USB,
		.end    = IRQ_IXP4XX_USB,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device ixp4xx_udc_device = {
	.name           = "pxa25x-udc",
	.id             = -1,
	.num_resources  = 2,
	.resource       = ixp4xx_udc_resources,
	.dev            = {
		.platform_data = &ixp4xx_udc_info,
	},
};

static struct platform_device *ixp4xx_devices[] __initdata = {
	&ixp4xx_udc_device,
};

static struct resource ixp46x_i2c_resources[] = {
	[0] = {
		.start 	= 0xc8011000,
		.end	= 0xc801101c,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start 	= IRQ_IXP4XX_I2C,
		.end	= IRQ_IXP4XX_I2C,
		.flags	= IORESOURCE_IRQ
	}
};

static struct platform_device ixp46x_i2c_controller = {
	.name		= "IOP3xx-I2C",
	.id		= 0,
	.num_resources	= 2,
	.resource	= ixp46x_i2c_resources
};

static struct platform_device *ixp46x_devices[] __initdata = {
	&ixp46x_i2c_controller
};

unsigned long ixp4xx_exp_bus_size;
EXPORT_SYMBOL(ixp4xx_exp_bus_size);

void __init ixp4xx_sys_init(void)
{
	ixp4xx_exp_bus_size = SZ_16M;

	platform_add_devices(ixp4xx_devices, ARRAY_SIZE(ixp4xx_devices));

	if (cpu_is_ixp46x()) {
		int region;

		platform_add_devices(ixp46x_devices,
				ARRAY_SIZE(ixp46x_devices));

		for (region = 0; region < 7; region++) {
			if((*(IXP4XX_EXP_REG(0x4 * region)) & 0x200)) {
				ixp4xx_exp_bus_size = SZ_32M;
				break;
			}
		}
	}

	printk("IXP4xx: Using %luMiB expansion bus window size\n",
			ixp4xx_exp_bus_size >> 20);
}

static u32 notrace ixp4xx_read_sched_clock(void)
{
	return *IXP4XX_OSTS;
}


static cycle_t ixp4xx_clocksource_read(struct clocksource *c)
{
	return *IXP4XX_OSTS;
}

unsigned long ixp4xx_timer_freq = IXP4XX_TIMER_FREQ;
EXPORT_SYMBOL(ixp4xx_timer_freq);
static void __init ixp4xx_clocksource_init(void)
{
	setup_sched_clock(ixp4xx_read_sched_clock, 32, ixp4xx_timer_freq);

	clocksource_mmio_init(NULL, "OSTS", ixp4xx_timer_freq, 200, 32,
			ixp4xx_clocksource_read);
}

static int ixp4xx_set_next_event(unsigned long evt,
				 struct clock_event_device *unused)
{
	unsigned long opts = *IXP4XX_OSRT1 & IXP4XX_OST_RELOAD_MASK;

	*IXP4XX_OSRT1 = (evt & ~IXP4XX_OST_RELOAD_MASK) | opts;

	return 0;
}

static void ixp4xx_set_mode(enum clock_event_mode mode,
			    struct clock_event_device *evt)
{
	unsigned long opts = *IXP4XX_OSRT1 & IXP4XX_OST_RELOAD_MASK;
	unsigned long osrt = *IXP4XX_OSRT1 & ~IXP4XX_OST_RELOAD_MASK;

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		osrt = LATCH & ~IXP4XX_OST_RELOAD_MASK;
 		opts = IXP4XX_OST_ENABLE;
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		
		osrt = 0;
		opts = IXP4XX_OST_ENABLE | IXP4XX_OST_ONE_SHOT;
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
		opts &= ~IXP4XX_OST_ENABLE;
		break;
	case CLOCK_EVT_MODE_RESUME:
		opts |= IXP4XX_OST_ENABLE;
		break;
	case CLOCK_EVT_MODE_UNUSED:
	default:
		osrt = opts = 0;
		break;
	}

	*IXP4XX_OSRT1 = osrt | opts;
}

static struct clock_event_device clockevent_ixp4xx = {
	.name		= "ixp4xx timer1",
	.features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.rating         = 200,
	.shift		= 24,
	.set_mode	= ixp4xx_set_mode,
	.set_next_event	= ixp4xx_set_next_event,
};

static void __init ixp4xx_clockevent_init(void)
{
	clockevent_ixp4xx.mult = div_sc(IXP4XX_TIMER_FREQ, NSEC_PER_SEC,
					clockevent_ixp4xx.shift);
	clockevent_ixp4xx.max_delta_ns =
		clockevent_delta2ns(0xfffffffe, &clockevent_ixp4xx);
	clockevent_ixp4xx.min_delta_ns =
		clockevent_delta2ns(0xf, &clockevent_ixp4xx);
	clockevent_ixp4xx.cpumask = cpumask_of(0);

	clockevents_register_device(&clockevent_ixp4xx);
}

void ixp4xx_restart(char mode, const char *cmd)
{
	if ( 1 && mode == 's') {
		
		soft_restart(0);
	} else {
		

		*IXP4XX_OSWK = IXP4XX_WDT_KEY;

		
		*IXP4XX_OSWT = 0;

		*IXP4XX_OSWE = IXP4XX_WDT_RESET_ENABLE | IXP4XX_WDT_COUNT_ENABLE;
	}
}

#ifdef CONFIG_IXP4XX_INDIRECT_PCI

static void __iomem *ixp4xx_ioremap_caller(unsigned long addr, size_t size,
					   unsigned int mtype, void *caller)
{
	if (!is_pci_memory(addr))
		return __arm_ioremap_caller(addr, size, mtype, caller);

	return (void __iomem *)addr;
}

static void ixp4xx_iounmap(void __iomem *addr)
{
	if (!is_pci_memory((__force u32)addr))
		__iounmap(addr);
}

void __init ixp4xx_init_early(void)
{
	arch_ioremap_caller = ixp4xx_ioremap_caller;
	arch_iounmap = ixp4xx_iounmap;
}
#else
void __init ixp4xx_init_early(void) {}
#endif
