/*
 *  interrupts.c: Interrupt mappings for PNX833X.
 *
 *  Copyright 2008 NXP Semiconductors
 *	  Chris Steel <chris.steel@nxp.com>
 *    Daniel Laird <daniel.j.laird@nxp.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/hardirq.h>
#include <linux/interrupt.h>
#include <asm/mipsregs.h>
#include <asm/irq_cpu.h>
#include <asm/setup.h>
#include <irq.h>
#include <irq-mapping.h>
#include <gpio.h>

static int mips_cpu_timer_irq;

static const unsigned int irq_prio[PNX833X_PIC_NUM_IRQ] =
{
    0, 
    4, 
    4, 
    1, 
    1, 
    6, 
    6, 
    7, 
    4, 
    5, 
    4, 
    4, 
    9, 
    9, 
    4, 
    9, 
    4, 
    9, 
    4, 
    4, 
    9, 
    9, 
    6, 
    6, 
    4, 
    4, 
    4, 
    3, 
    3, 
    4, 
    4, 
    4, 
    5, 
    4, 
    4, 
    4, 
    4, 
#if defined(CONFIG_SOC_PNX8335)
    4, 
    4, 
    9, 
    9, 
    9, 
    4, 
    4, 
    4, 
    4, 
    4, 
    4, 
    4, 
    4, 
    4, 
    4, 
   12, 
    3, 
    3, 
    4, 
    4, 
    4, 
#endif
};

static void pnx833x_timer_dispatch(void)
{
	do_IRQ(mips_cpu_timer_irq);
}

static void pic_dispatch(void)
{
	unsigned int irq = PNX833X_REGFIELD(PIC_INT_SRC, INT_SRC);

	if ((irq >= 1) && (irq < (PNX833X_PIC_NUM_IRQ))) {
		unsigned long priority = PNX833X_PIC_INT_PRIORITY;
		PNX833X_PIC_INT_PRIORITY = irq_prio[irq];

		if (irq == PNX833X_PIC_GPIO_INT) {
			unsigned long mask = PNX833X_PIO_INT_STATUS & PNX833X_PIO_INT_ENABLE;
			int pin;
			while ((pin = ffs(mask & 0xffff))) {
				pin -= 1;
				do_IRQ(PNX833X_GPIO_IRQ_BASE + pin);
				mask &= ~(1 << pin);
			}
		} else {
			do_IRQ(irq + PNX833X_PIC_IRQ_BASE);
		}

		PNX833X_PIC_INT_PRIORITY = priority;
	} else {
		printk(KERN_ERR "plat_irq_dispatch: unexpected irq %u\n", irq);
	}
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending = read_c0_status() & read_c0_cause();

	if (pending & STATUSF_IP4)
		pic_dispatch();
	else if (pending & STATUSF_IP7)
		do_IRQ(PNX833X_TIMER_IRQ);
	else
		spurious_interrupt();
}

static inline void pnx833x_hard_enable_pic_irq(unsigned int irq)
{
	PNX833X_PIC_INT_REG(irq) = irq_prio[irq];
}

static inline void pnx833x_hard_disable_pic_irq(unsigned int irq)
{
	
	PNX833X_PIC_INT_REG(irq) = 0;
}

static DEFINE_RAW_SPINLOCK(pnx833x_irq_lock);

static unsigned int pnx833x_startup_pic_irq(unsigned int irq)
{
	unsigned long flags;
	unsigned int pic_irq = irq - PNX833X_PIC_IRQ_BASE;

	raw_spin_lock_irqsave(&pnx833x_irq_lock, flags);
	pnx833x_hard_enable_pic_irq(pic_irq);
	raw_spin_unlock_irqrestore(&pnx833x_irq_lock, flags);
	return 0;
}

static void pnx833x_enable_pic_irq(struct irq_data *d)
{
	unsigned long flags;
	unsigned int pic_irq = d->irq - PNX833X_PIC_IRQ_BASE;

	raw_spin_lock_irqsave(&pnx833x_irq_lock, flags);
	pnx833x_hard_enable_pic_irq(pic_irq);
	raw_spin_unlock_irqrestore(&pnx833x_irq_lock, flags);
}

static void pnx833x_disable_pic_irq(struct irq_data *d)
{
	unsigned long flags;
	unsigned int pic_irq = d->irq - PNX833X_PIC_IRQ_BASE;

	raw_spin_lock_irqsave(&pnx833x_irq_lock, flags);
	pnx833x_hard_disable_pic_irq(pic_irq);
	raw_spin_unlock_irqrestore(&pnx833x_irq_lock, flags);
}

static DEFINE_RAW_SPINLOCK(pnx833x_gpio_pnx833x_irq_lock);

static void pnx833x_enable_gpio_irq(struct irq_data *d)
{
	int pin = d->irq - PNX833X_GPIO_IRQ_BASE;
	unsigned long flags;
	raw_spin_lock_irqsave(&pnx833x_gpio_pnx833x_irq_lock, flags);
	pnx833x_gpio_enable_irq(pin);
	raw_spin_unlock_irqrestore(&pnx833x_gpio_pnx833x_irq_lock, flags);
}

static void pnx833x_disable_gpio_irq(struct irq_data *d)
{
	int pin = d->irq - PNX833X_GPIO_IRQ_BASE;
	unsigned long flags;
	raw_spin_lock_irqsave(&pnx833x_gpio_pnx833x_irq_lock, flags);
	pnx833x_gpio_disable_irq(pin);
	raw_spin_unlock_irqrestore(&pnx833x_gpio_pnx833x_irq_lock, flags);
}

static int pnx833x_set_type_gpio_irq(struct irq_data *d, unsigned int flow_type)
{
	int pin = d->irq - PNX833X_GPIO_IRQ_BASE;
	int gpio_mode;

	switch (flow_type) {
	case IRQ_TYPE_EDGE_RISING:
		gpio_mode = GPIO_INT_EDGE_RISING;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		gpio_mode = GPIO_INT_EDGE_FALLING;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		gpio_mode = GPIO_INT_EDGE_BOTH;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		gpio_mode = GPIO_INT_LEVEL_HIGH;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		gpio_mode = GPIO_INT_LEVEL_LOW;
		break;
	default:
		gpio_mode = GPIO_INT_NONE;
		break;
	}

	pnx833x_gpio_setup_irq(gpio_mode, pin);

	return 0;
}

static struct irq_chip pnx833x_pic_irq_type = {
	.name = "PNX-PIC",
	.irq_enable = pnx833x_enable_pic_irq,
	.irq_disable = pnx833x_disable_pic_irq,
};

static struct irq_chip pnx833x_gpio_irq_type = {
	.name = "PNX-GPIO",
	.irq_enable = pnx833x_enable_gpio_irq,
	.irq_disable = pnx833x_disable_gpio_irq,
	.irq_set_type = pnx833x_set_type_gpio_irq,
};

void __init arch_init_irq(void)
{
	unsigned int irq;

	
	mips_cpu_irq_init();

	
	for (irq = PNX833X_PIC_IRQ_BASE; irq < (PNX833X_PIC_IRQ_BASE + PNX833X_PIC_NUM_IRQ); irq++) {
		pnx833x_hard_disable_pic_irq(irq);
		irq_set_chip_and_handler(irq, &pnx833x_pic_irq_type,
					 handle_simple_irq);
	}

	for (irq = PNX833X_GPIO_IRQ_BASE; irq < (PNX833X_GPIO_IRQ_BASE + PNX833X_GPIO_NUM_IRQ); irq++)
		irq_set_chip_and_handler(irq, &pnx833x_gpio_irq_type,
					 handle_simple_irq);

	
	PNX833X_PIC_INT_PRIORITY = 0;

	
	pnx833x_startup_pic_irq(PNX833X_PIC_GPIO_INT);

	
	if (cpu_has_vint)
		set_vi_handler(4, pic_dispatch);

	write_c0_status(read_c0_status() | IE_IRQ2);
}

unsigned int __cpuinit get_c0_compare_int(void)
{
	if (cpu_has_vint)
		set_vi_handler(cp0_compare_irq, pnx833x_timer_dispatch);

	mips_cpu_timer_irq = MIPS_CPU_IRQ_BASE + cp0_compare_irq;
	return mips_cpu_timer_irq;
}

void __init plat_time_init(void)
{
	

	extern unsigned long mips_hpt_frequency;
	unsigned long reg = PNX833X_CLOCK_CPUCP_CTL;

	if (!(PNX833X_BIT(reg, CLOCK_CPUCP_CTL, EXIT_RESET))) {
		
		mips_hpt_frequency = 25;
	} else {
#if defined(CONFIG_SOC_PNX8335)
		
		mips_hpt_frequency = 90 + (10 * PNX8335_REGFIELD(CLOCK_PLL_CPU_CTL, FREQ));
#else
		static const unsigned long int freq[4] = {240, 160, 120, 80};
		mips_hpt_frequency = freq[PNX833X_FIELD(reg, CLOCK_CPUCP_CTL, DIV_CLOCK)];
#endif
	}

	printk(KERN_INFO "CPU clock is %ld MHz\n", mips_hpt_frequency);

	mips_hpt_frequency *= 500000;
}
