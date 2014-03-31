/*
 * Pb1100 board platform device registration
 *
 * Copyright (C) 2009 Manuel Lauss
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-db1x00/bcsr.h>
#include <prom.h>
#include "platform.h"

const char *get_system_type(void)
{
	return "PB1100";
}

void __init board_setup(void)
{
	volatile void __iomem *base = (volatile void __iomem *)0xac000000UL;

	bcsr_init(DB1000_BCSR_PHYS_ADDR,
		  DB1000_BCSR_PHYS_ADDR + DB1000_BCSR_HEXLED_OFS);

	
	au_writel(8, SYS_AUXPLL);
	alchemy_gpio1_input_enable();
	udelay(100);

#if defined(CONFIG_USB_OHCI_HCD) || defined(CONFIG_USB_OHCI_HCD_MODULE)
	{
		u32 pin_func, sys_freqctrl, sys_clksrc;

		
		pin_func = au_readl(SYS_PINFUNC) & ~SYS_PF_UR3;

		
		sys_freqctrl = au_readl(SYS_FREQCTRL0);
		sys_freqctrl &= ~0xFFF00000;
		au_writel(sys_freqctrl, SYS_FREQCTRL0);

		
		sys_clksrc = au_readl(SYS_CLKSRC);
		sys_clksrc &= ~(SYS_CS_CIR | SYS_CS_DIR | SYS_CS_MIR_MASK);
		au_writel(sys_clksrc, SYS_CLKSRC);

		sys_freqctrl = au_readl(SYS_FREQCTRL0);
		sys_freqctrl &= ~0xFFF00000;

		sys_clksrc = au_readl(SYS_CLKSRC);
		sys_clksrc &= ~(SYS_CS_CIR | SYS_CS_DIR | SYS_CS_MIR_MASK);

		
		sys_freqctrl |= (0 << SYS_FC_FRDIV2_BIT) |
				SYS_FC_FE2 | SYS_FC_FS2;
		au_writel(sys_freqctrl, SYS_FREQCTRL0);

		sys_clksrc |= SYS_CS_MUX_FQ2 << SYS_CS_MIR_BIT;
		au_writel(sys_clksrc, SYS_CLKSRC);

		
		au_writel(0x00000002, MEM_STCFG3);  
		au_writel(0x280E3D07, MEM_STTIME3); 
		au_writel(0x10000000, MEM_STADDR3); 

		pin_func = au_readl(SYS_PINFUNC) & ~SYS_PF_USB;
		
		pin_func |= SYS_PF_USB;
		au_writel(pin_func, SYS_PINFUNC);
	}
#endif 

	
	au_writel(au_readl(SYS_POWERCTRL) | (0x3 << 5), SYS_POWERCTRL);

	
	if (!(readb(base + 0x28) & 0x20)) {
		writeb(readb(base + 0x28) | 0x20, base + 0x28);
		au_sync();
	}
	
	if (readb(base + 0x2C) & 0x4) { 
		writeb(readb(base + 0x2c) & ~0x4, base + 0x2c);
		au_sync();
	}
}


static struct resource au1100_lcd_resources[] = {
	[0] = {
		.start	= AU1100_LCD_PHYS_ADDR,
		.end	= AU1100_LCD_PHYS_ADDR + 0x800 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AU1100_LCD_INT,
		.end	= AU1100_LCD_INT,
		.flags	= IORESOURCE_IRQ,
	}
};

static u64 au1100_lcd_dmamask = DMA_BIT_MASK(32);

static struct platform_device au1100_lcd_device = {
	.name		= "au1100-lcd",
	.id		= 0,
	.dev = {
		.dma_mask		= &au1100_lcd_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.num_resources	= ARRAY_SIZE(au1100_lcd_resources),
	.resource	= au1100_lcd_resources,
};

static int __init pb1100_dev_init(void)
{
	int swapped;

	irq_set_irq_type(AU1100_GPIO9_INT, IRQF_TRIGGER_LOW); 
	irq_set_irq_type(AU1100_GPIO10_INT, IRQF_TRIGGER_LOW); 
	irq_set_irq_type(AU1100_GPIO11_INT, IRQF_TRIGGER_LOW); 
	irq_set_irq_type(AU1100_GPIO13_INT, IRQF_TRIGGER_LOW); 

	
	db1x_register_pcmcia_socket(
		AU1000_PCMCIA_ATTR_PHYS_ADDR,
		AU1000_PCMCIA_ATTR_PHYS_ADDR + 0x000400000 - 1,
		AU1000_PCMCIA_MEM_PHYS_ADDR,
		AU1000_PCMCIA_MEM_PHYS_ADDR  + 0x000400000 - 1,
		AU1000_PCMCIA_IO_PHYS_ADDR,
		AU1000_PCMCIA_IO_PHYS_ADDR   + 0x000010000 - 1,
		AU1100_GPIO11_INT, AU1100_GPIO9_INT,	 
		0, 0, 0); 

	swapped = bcsr_read(BCSR_STATUS) &  BCSR_STATUS_DB1000_SWAPBOOT;
	db1x_register_norflash(64 * 1024 * 1024, 4, swapped);
	platform_device_register(&au1100_lcd_device);

	return 0;
}
device_initcall(pb1100_dev_init);
