#include <linux/clocksource.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/timex.h>
#include <linux/init.h>

#include <asm/pgtable.h>
#include <asm/io.h>

#include <asm/mach_timer.h>

#define CYCLONE_CBAR_ADDR	0xFEB00CD0	
#define CYCLONE_PMCC_OFFSET	0x51A0		
#define CYCLONE_MPCS_OFFSET	0x51A8		
#define CYCLONE_MPMC_OFFSET	0x51D0		
#define CYCLONE_TIMER_FREQ	99780000	
#define CYCLONE_TIMER_MASK	CLOCKSOURCE_MASK(32) 

int use_cyclone = 0;
static void __iomem *cyclone_ptr;

static cycle_t read_cyclone(struct clocksource *cs)
{
	return (cycle_t)readl(cyclone_ptr);
}

static struct clocksource clocksource_cyclone = {
	.name		= "cyclone",
	.rating		= 250,
	.read		= read_cyclone,
	.mask		= CYCLONE_TIMER_MASK,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static int __init init_cyclone_clocksource(void)
{
	unsigned long base;	
	unsigned long offset;
	u32 __iomem* volatile cyclone_timer;	
	u32 __iomem* reg;
	int i;

	
	if (!use_cyclone)
		return -ENODEV;

	printk(KERN_INFO "Summit chipset: Starting Cyclone Counter.\n");

	
	offset = CYCLONE_CBAR_ADDR;
	reg = ioremap_nocache(offset, sizeof(reg));
	if (!reg) {
		printk(KERN_ERR "Summit chipset: Could not find valid CBAR register.\n");
		return -ENODEV;
	}
	
	base = readl(reg);
	iounmap(reg);
	if (!base) {
		printk(KERN_ERR "Summit chipset: Could not find valid CBAR value.\n");
		return -ENODEV;
	}

	
	offset = base + CYCLONE_PMCC_OFFSET;
	reg = ioremap_nocache(offset, sizeof(reg));
	if (!reg) {
		printk(KERN_ERR "Summit chipset: Could not find valid PMCC register.\n");
		return -ENODEV;
	}
	writel(0x00000001,reg);
	iounmap(reg);

	
	offset = base + CYCLONE_MPCS_OFFSET;
	reg = ioremap_nocache(offset, sizeof(reg));
	if (!reg) {
		printk(KERN_ERR "Summit chipset: Could not find valid MPCS register.\n");
		return -ENODEV;
	}
	writel(0x00000001,reg);
	iounmap(reg);

	
	offset = base + CYCLONE_MPMC_OFFSET;
	cyclone_timer = ioremap_nocache(offset, sizeof(u64));
	if (!cyclone_timer) {
		printk(KERN_ERR "Summit chipset: Could not find valid MPMC register.\n");
		return -ENODEV;
	}

	
	for (i = 0; i < 3; i++){
		u32 old = readl(cyclone_timer);
		int stall = 100;

		while (stall--)
			barrier();

		if (readl(cyclone_timer) == old) {
			printk(KERN_ERR "Summit chipset: Counter not counting! DISABLED\n");
			iounmap(cyclone_timer);
			cyclone_timer = NULL;
			return -ENODEV;
		}
	}
	cyclone_ptr = cyclone_timer;

	return clocksource_register_hz(&clocksource_cyclone,
					CYCLONE_TIMER_FREQ);
}

arch_initcall(init_cyclone_clocksource);
