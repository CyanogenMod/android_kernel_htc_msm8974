#include <linux/atmel_tc.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/of.h>


#if defined(CONFIG_AVR32)
const u8 atmel_tc_divisors[5] = { 0, 4, 8, 16, 32, };
EXPORT_SYMBOL(atmel_tc_divisors);

#elif defined(CONFIG_ARCH_AT91)
const u8 atmel_tc_divisors[5] = { 2, 8, 32, 128, 0, };
EXPORT_SYMBOL(atmel_tc_divisors);

#endif

static DEFINE_SPINLOCK(tc_list_lock);
static LIST_HEAD(tc_list);

struct atmel_tc *atmel_tc_alloc(unsigned block, const char *name)
{
	struct atmel_tc		*tc;
	struct platform_device	*pdev = NULL;
	struct resource		*r;
	size_t			size;

	spin_lock(&tc_list_lock);
	list_for_each_entry(tc, &tc_list, node) {
		if (tc->pdev->dev.of_node) {
			if (of_alias_get_id(tc->pdev->dev.of_node, "tcb")
					== block) {
				pdev = tc->pdev;
				break;
			}
		} else if (tc->pdev->id == block) {
			pdev = tc->pdev;
			break;
		}
	}

	if (!pdev || tc->iomem)
		goto fail;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r)
		goto fail;

	size = resource_size(r);
	r = request_mem_region(r->start, size, name);
	if (!r)
		goto fail;

	tc->regs = ioremap(r->start, size);
	if (!tc->regs)
		goto fail_ioremap;

	tc->iomem = r;

out:
	spin_unlock(&tc_list_lock);
	return tc;

fail_ioremap:
	release_mem_region(r->start, size);
fail:
	tc = NULL;
	goto out;
}
EXPORT_SYMBOL_GPL(atmel_tc_alloc);

void atmel_tc_free(struct atmel_tc *tc)
{
	spin_lock(&tc_list_lock);
	if (tc->regs) {
		iounmap(tc->regs);
		release_mem_region(tc->iomem->start, resource_size(tc->iomem));
		tc->regs = NULL;
		tc->iomem = NULL;
	}
	spin_unlock(&tc_list_lock);
}
EXPORT_SYMBOL_GPL(atmel_tc_free);

#if defined(CONFIG_OF)
static struct atmel_tcb_config tcb_rm9200_config = {
	.counter_width = 16,
};

static struct atmel_tcb_config tcb_sam9x5_config = {
	.counter_width = 32,
};

static const struct of_device_id atmel_tcb_dt_ids[] = {
	{
		.compatible = "atmel,at91rm9200-tcb",
		.data = &tcb_rm9200_config,
	}, {
		.compatible = "atmel,at91sam9x5-tcb",
		.data = &tcb_sam9x5_config,
	}, {
		
	}
};

MODULE_DEVICE_TABLE(of, atmel_tcb_dt_ids);
#endif

static int __init tc_probe(struct platform_device *pdev)
{
	struct atmel_tc *tc;
	struct clk	*clk;
	int		irq;

	if (!platform_get_resource(pdev, IORESOURCE_MEM, 0))
		return -EINVAL;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return -EINVAL;

	tc = kzalloc(sizeof(struct atmel_tc), GFP_KERNEL);
	if (!tc)
		return -ENOMEM;

	tc->pdev = pdev;

	clk = clk_get(&pdev->dev, "t0_clk");
	if (IS_ERR(clk)) {
		kfree(tc);
		return -EINVAL;
	}

	
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(atmel_tcb_dt_ids, pdev->dev.of_node);
		if (match)
			tc->tcb_config = match->data;
	}

	tc->clk[0] = clk;
	tc->clk[1] = clk_get(&pdev->dev, "t1_clk");
	if (IS_ERR(tc->clk[1]))
		tc->clk[1] = clk;
	tc->clk[2] = clk_get(&pdev->dev, "t2_clk");
	if (IS_ERR(tc->clk[2]))
		tc->clk[2] = clk;

	tc->irq[0] = irq;
	tc->irq[1] = platform_get_irq(pdev, 1);
	if (tc->irq[1] < 0)
		tc->irq[1] = irq;
	tc->irq[2] = platform_get_irq(pdev, 2);
	if (tc->irq[2] < 0)
		tc->irq[2] = irq;

	spin_lock(&tc_list_lock);
	list_add_tail(&tc->node, &tc_list);
	spin_unlock(&tc_list_lock);

	return 0;
}

static struct platform_driver tc_driver = {
	.driver = {
		.name	= "atmel_tcb",
		.of_match_table	= of_match_ptr(atmel_tcb_dt_ids),
	},
};

static int __init tc_init(void)
{
	return platform_driver_probe(&tc_driver, tc_probe);
}
arch_initcall(tc_init);
