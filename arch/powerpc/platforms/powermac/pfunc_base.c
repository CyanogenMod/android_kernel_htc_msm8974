#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#include <asm/pmac_feature.h>
#include <asm/pmac_pfunc.h>

#undef DEBUG
#ifdef DEBUG
#define DBG(fmt...)	printk(fmt)
#else
#define DBG(fmt...)
#endif

static irqreturn_t macio_gpio_irq(int irq, void *data)
{
	pmf_do_irq(data);

	return IRQ_HANDLED;
}

static int macio_do_gpio_irq_enable(struct pmf_function *func)
{
	unsigned int irq = irq_of_parse_and_map(func->node, 0);
	if (irq == NO_IRQ)
		return -EINVAL;
	return request_irq(irq, macio_gpio_irq, 0, func->node->name, func);
}

static int macio_do_gpio_irq_disable(struct pmf_function *func)
{
	unsigned int irq = irq_of_parse_and_map(func->node, 0);
	if (irq == NO_IRQ)
		return -EINVAL;
	free_irq(irq, func);
	return 0;
}

static int macio_do_gpio_write(PMF_STD_ARGS, u8 value, u8 mask)
{
	u8 __iomem *addr = (u8 __iomem *)func->driver_data;
	unsigned long flags;
	u8 tmp;

	
	if (args && args->count && !args->u[0].v)
		value = ~value;

	
	raw_spin_lock_irqsave(&feature_lock, flags);
	tmp = readb(addr);
	tmp = (tmp & ~mask) | (value & mask);
	DBG("Do write 0x%02x to GPIO %s (%p)\n",
	    tmp, func->node->full_name, addr);
	writeb(tmp, addr);
	raw_spin_unlock_irqrestore(&feature_lock, flags);

	return 0;
}

static int macio_do_gpio_read(PMF_STD_ARGS, u8 mask, int rshift, u8 xor)
{
	u8 __iomem *addr = (u8 __iomem *)func->driver_data;
	u32 value;

	
	if (args == NULL || args->count == 0 || args->u[0].p == NULL)
		return -EINVAL;

	value = readb(addr);
	*args->u[0].p = ((value & mask) >> rshift) ^ xor;

	return 0;
}

static int macio_do_delay(PMF_STD_ARGS, u32 duration)
{
	
	msleep((duration + 999) / 1000);
	return 0;
}

static struct pmf_handlers macio_gpio_handlers = {
	.irq_enable	= macio_do_gpio_irq_enable,
	.irq_disable	= macio_do_gpio_irq_disable,
	.write_gpio	= macio_do_gpio_write,
	.read_gpio	= macio_do_gpio_read,
	.delay		= macio_do_delay,
};

static void macio_gpio_init_one(struct macio_chip *macio)
{
	struct device_node *gparent, *gp;


	for (gparent = NULL;
	     (gparent = of_get_next_child(macio->of_node, gparent)) != NULL;)
		if (strcmp(gparent->name, "gpio") == 0)
			break;
	if (gparent == NULL)
		return;

	DBG("Installing GPIO functions for macio %s\n",
	    macio->of_node->full_name);

	for (gp = NULL; (gp = of_get_next_child(gparent, gp)) != NULL;) {
		const u32 *reg = of_get_property(gp, "reg", NULL);
		unsigned long offset;
		if (reg == NULL)
			continue;
		offset = *reg;
		if (offset < 0x50)
			offset += 0x50;
		offset += (unsigned long)macio->base;
		pmf_register_driver(gp, &macio_gpio_handlers, (void *)offset);
	}

	DBG("Calling initial GPIO functions for macio %s\n",
	    macio->of_node->full_name);

	
	for (gp = NULL; (gp = of_get_next_child(gparent, gp)) != NULL;)
		pmf_do_functions(gp, NULL, 0, PMF_FLAGS_ON_INIT, NULL);

}

static int macio_do_write_reg32(PMF_STD_ARGS, u32 offset, u32 value, u32 mask)
{
	struct macio_chip *macio = func->driver_data;
	unsigned long flags;

	raw_spin_lock_irqsave(&feature_lock, flags);
	MACIO_OUT32(offset, (MACIO_IN32(offset) & ~mask) | (value & mask));
	raw_spin_unlock_irqrestore(&feature_lock, flags);
	return 0;
}

static int macio_do_read_reg32(PMF_STD_ARGS, u32 offset)
{
	struct macio_chip *macio = func->driver_data;

	
	if (args == NULL || args->count == 0 || args->u[0].p == NULL)
		return -EINVAL;

	*args->u[0].p = MACIO_IN32(offset);
	return 0;
}

static int macio_do_write_reg8(PMF_STD_ARGS, u32 offset, u8 value, u8 mask)
{
	struct macio_chip *macio = func->driver_data;
	unsigned long flags;

	raw_spin_lock_irqsave(&feature_lock, flags);
	MACIO_OUT8(offset, (MACIO_IN8(offset) & ~mask) | (value & mask));
	raw_spin_unlock_irqrestore(&feature_lock, flags);
	return 0;
}

static int macio_do_read_reg8(PMF_STD_ARGS, u32 offset)
{
	struct macio_chip *macio = func->driver_data;

	
	if (args == NULL || args->count == 0 || args->u[0].p == NULL)
		return -EINVAL;

	*((u8 *)(args->u[0].p)) = MACIO_IN8(offset);
	return 0;
}

static int macio_do_read_reg32_msrx(PMF_STD_ARGS, u32 offset, u32 mask,
				    u32 shift, u32 xor)
{
	struct macio_chip *macio = func->driver_data;

	
	if (args == NULL || args->count == 0 || args->u[0].p == NULL)
		return -EINVAL;

	*args->u[0].p = ((MACIO_IN32(offset) & mask) >> shift) ^ xor;
	return 0;
}

static int macio_do_read_reg8_msrx(PMF_STD_ARGS, u32 offset, u32 mask,
				   u32 shift, u32 xor)
{
	struct macio_chip *macio = func->driver_data;

	
	if (args == NULL || args->count == 0 || args->u[0].p == NULL)
		return -EINVAL;

	*((u8 *)(args->u[0].p)) = ((MACIO_IN8(offset) & mask) >> shift) ^ xor;
	return 0;
}

static int macio_do_write_reg32_slm(PMF_STD_ARGS, u32 offset, u32 shift,
				    u32 mask)
{
	struct macio_chip *macio = func->driver_data;
	unsigned long flags;
	u32 tmp, val;

	
	if (args == NULL || args->count == 0)
		return -EINVAL;

	raw_spin_lock_irqsave(&feature_lock, flags);
	tmp = MACIO_IN32(offset);
	val = args->u[0].v << shift;
	tmp = (tmp & ~mask) | (val & mask);
	MACIO_OUT32(offset, tmp);
	raw_spin_unlock_irqrestore(&feature_lock, flags);
	return 0;
}

static int macio_do_write_reg8_slm(PMF_STD_ARGS, u32 offset, u32 shift,
				   u32 mask)
{
	struct macio_chip *macio = func->driver_data;
	unsigned long flags;
	u32 tmp, val;

	
	if (args == NULL || args->count == 0)
		return -EINVAL;

	raw_spin_lock_irqsave(&feature_lock, flags);
	tmp = MACIO_IN8(offset);
	val = args->u[0].v << shift;
	tmp = (tmp & ~mask) | (val & mask);
	MACIO_OUT8(offset, tmp);
	raw_spin_unlock_irqrestore(&feature_lock, flags);
	return 0;
}

static struct pmf_handlers macio_mmio_handlers = {
	.write_reg32		= macio_do_write_reg32,
	.read_reg32		= macio_do_read_reg32,
	.write_reg8		= macio_do_write_reg8,
	.read_reg8		= macio_do_read_reg8,
	.read_reg32_msrx	= macio_do_read_reg32_msrx,
	.read_reg8_msrx		= macio_do_read_reg8_msrx,
	.write_reg32_slm	= macio_do_write_reg32_slm,
	.write_reg8_slm		= macio_do_write_reg8_slm,
	.delay			= macio_do_delay,
};

static void macio_mmio_init_one(struct macio_chip *macio)
{
	DBG("Installing MMIO functions for macio %s\n",
	    macio->of_node->full_name);

	pmf_register_driver(macio->of_node, &macio_mmio_handlers, macio);
}

static struct device_node *unin_hwclock;

static int unin_do_write_reg32(PMF_STD_ARGS, u32 offset, u32 value, u32 mask)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&feature_lock, flags);
	UN_OUT(offset, (UN_IN(offset) & ~mask) | (value & mask));
	raw_spin_unlock_irqrestore(&feature_lock, flags);
	return 0;
}


static struct pmf_handlers unin_mmio_handlers = {
	.write_reg32		= unin_do_write_reg32,
	.delay			= macio_do_delay,
};

static void uninorth_install_pfunc(void)
{
	struct device_node *np;

	DBG("Installing functions for UniN %s\n",
	    uninorth_node->full_name);

	pmf_register_driver(uninorth_node, &unin_mmio_handlers, NULL);
	pmf_do_functions(uninorth_node, NULL, 0, PMF_FLAGS_ON_INIT, NULL);


	for (np = NULL; (np = of_get_next_child(uninorth_node, np)) != NULL;)
		if (strcmp(np->name, "hw-clock") == 0) {
			unin_hwclock = np;
			break;
		}
	if (unin_hwclock) {
		DBG("Installing functions for UniN clock %s\n",
		    unin_hwclock->full_name);
		pmf_register_driver(unin_hwclock, &unin_mmio_handlers, NULL);
		pmf_do_functions(unin_hwclock, NULL, 0, PMF_FLAGS_ON_INIT,
				 NULL);
	}
}

int __init pmac_pfunc_base_install(void)
{
	static int pfbase_inited;
	int i;

	if (pfbase_inited)
		return 0;
	pfbase_inited = 1;

	if (!machine_is(powermac))
		return 0;

	DBG("Installing base platform functions...\n");

	for (i = 0 ; i < MAX_MACIO_CHIPS; i++) {
		if (macio_chips[i].of_node) {
			macio_mmio_init_one(&macio_chips[i]);
			macio_gpio_init_one(&macio_chips[i]);
		}
	}

	if (uninorth_node && uninorth_base)
		uninorth_install_pfunc();

	DBG("All base functions installed\n");

	return 0;
}
machine_arch_initcall(powermac, pmac_pfunc_base_install);

#ifdef CONFIG_PM

void pmac_pfunc_base_suspend(void)
{
	int i;

	for (i = 0 ; i < MAX_MACIO_CHIPS; i++) {
		if (macio_chips[i].of_node)
			pmf_do_functions(macio_chips[i].of_node, NULL, 0,
					 PMF_FLAGS_ON_SLEEP, NULL);
	}
	if (uninorth_node)
		pmf_do_functions(uninorth_node, NULL, 0,
				 PMF_FLAGS_ON_SLEEP, NULL);
	if (unin_hwclock)
		pmf_do_functions(unin_hwclock, NULL, 0,
				 PMF_FLAGS_ON_SLEEP, NULL);
}

void pmac_pfunc_base_resume(void)
{
	int i;

	if (unin_hwclock)
		pmf_do_functions(unin_hwclock, NULL, 0,
				 PMF_FLAGS_ON_WAKE, NULL);
	if (uninorth_node)
		pmf_do_functions(uninorth_node, NULL, 0,
				 PMF_FLAGS_ON_WAKE, NULL);
	for (i = 0 ; i < MAX_MACIO_CHIPS; i++) {
		if (macio_chips[i].of_node)
			pmf_do_functions(macio_chips[i].of_node, NULL, 0,
					 PMF_FLAGS_ON_WAKE, NULL);
	}
}

#endif 
