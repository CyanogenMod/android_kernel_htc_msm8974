/*
 *  Miscellaneous functions for IDT EB434 board
 *
 *  Copyright 2004 IDT Inc. (rischelp@idt.com)
 *  Copyright 2006 Phil Sutter <n0-1@freewrt.org>
 *  Copyright 2007 Florian Fainelli <florian@openwrt.org>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#include <asm/mach-rc32434/rb.h>
#include <asm/mach-rc32434/gpio.h>

struct rb532_gpio_chip {
	struct gpio_chip chip;
	void __iomem	 *regbase;
};

static struct resource rb532_gpio_reg0_res[] = {
	{
		.name 	= "gpio_reg0",
		.start 	= REGBASE + GPIOBASE,
		.end 	= REGBASE + GPIOBASE + sizeof(struct rb532_gpio_reg) - 1,
		.flags 	= IORESOURCE_MEM,
	}
};

static inline void rb532_set_bit(unsigned bitval,
		unsigned offset, void __iomem *ioaddr)
{
	unsigned long flags;
	u32 val;

	local_irq_save(flags);

	val = readl(ioaddr);
	val &= ~(!bitval << offset);   
	val |= (!!bitval << offset);   
	writel(val, ioaddr);

	local_irq_restore(flags);
}

static inline int rb532_get_bit(unsigned offset, void __iomem *ioaddr)
{
	return (readl(ioaddr) & (1 << offset));
}

static int rb532_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct rb532_gpio_chip	*gpch;

	gpch = container_of(chip, struct rb532_gpio_chip, chip);
	return rb532_get_bit(offset, gpch->regbase + GPIOD);
}

static void rb532_gpio_set(struct gpio_chip *chip,
				unsigned offset, int value)
{
	struct rb532_gpio_chip	*gpch;

	gpch = container_of(chip, struct rb532_gpio_chip, chip);
	rb532_set_bit(value, offset, gpch->regbase + GPIOD);
}

static int rb532_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct rb532_gpio_chip	*gpch;

	gpch = container_of(chip, struct rb532_gpio_chip, chip);

	
	rb532_set_bit(0, offset, gpch->regbase + GPIOFUNC);

	rb532_set_bit(0, offset, gpch->regbase + GPIOCFG);
	return 0;
}

static int rb532_gpio_direction_output(struct gpio_chip *chip,
					unsigned offset, int value)
{
	struct rb532_gpio_chip	*gpch;

	gpch = container_of(chip, struct rb532_gpio_chip, chip);

	
	rb532_set_bit(0, offset, gpch->regbase + GPIOFUNC);

	
	rb532_set_bit(value, offset, gpch->regbase + GPIOD);

	rb532_set_bit(1, offset, gpch->regbase + GPIOCFG);
	return 0;
}

static struct rb532_gpio_chip rb532_gpio_chip[] = {
	[0] = {
		.chip = {
			.label			= "gpio0",
			.direction_input	= rb532_gpio_direction_input,
			.direction_output	= rb532_gpio_direction_output,
			.get			= rb532_gpio_get,
			.set			= rb532_gpio_set,
			.base			= 0,
			.ngpio			= 32,
		},
	},
};

void rb532_gpio_set_ilevel(int bit, unsigned gpio)
{
	rb532_set_bit(bit, gpio, rb532_gpio_chip->regbase + GPIOILEVEL);
}
EXPORT_SYMBOL(rb532_gpio_set_ilevel);

void rb532_gpio_set_istat(int bit, unsigned gpio)
{
	rb532_set_bit(bit, gpio, rb532_gpio_chip->regbase + GPIOISTAT);
}
EXPORT_SYMBOL(rb532_gpio_set_istat);

void rb532_gpio_set_func(unsigned gpio)
{
       rb532_set_bit(1, gpio, rb532_gpio_chip->regbase + GPIOFUNC);
}
EXPORT_SYMBOL(rb532_gpio_set_func);

int __init rb532_gpio_init(void)
{
	struct resource *r;

	r = rb532_gpio_reg0_res;
	rb532_gpio_chip->regbase = ioremap_nocache(r->start, resource_size(r));

	if (!rb532_gpio_chip->regbase) {
		printk(KERN_ERR "rb532: cannot remap GPIO register 0\n");
		return -ENXIO;
	}

	
	gpiochip_add(&rb532_gpio_chip->chip);

	return 0;
}
arch_initcall(rb532_gpio_init);
