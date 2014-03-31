/*
 * Driver for MPC52xx processor BestComm peripheral controller
 *
 *
 * Copyright (C) 2006-2007 Sylvain Munaut <tnt@246tNt.com>
 * Copyright (C) 2005      Varma Electronics Oy,
 *                         ( by Andrey Volkov <avolkov@varma-el.com> )
 * Copyright (C) 2003-2004 MontaVista, Software, Inc.
 *                         ( by Dale Farnsworth <dfarnsworth@mvista.com> )
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mpc52xx.h>

#include "sram.h"
#include "bestcomm_priv.h"
#include "bestcomm.h"

#define DRIVER_NAME "bestcomm-core"

static struct of_device_id mpc52xx_sram_ids[] __devinitdata = {
	{ .compatible = "fsl,mpc5200-sram", },
	{ .compatible = "mpc5200-sram", },
	{}
};


struct bcom_engine *bcom_eng = NULL;
EXPORT_SYMBOL_GPL(bcom_eng);	



struct bcom_task *
bcom_task_alloc(int bd_count, int bd_size, int priv_size)
{
	int i, tasknum = -1;
	struct bcom_task *tsk;

	
	if (!bcom_eng)
		return NULL;

	
	spin_lock(&bcom_eng->lock);

	for (i=0; i<BCOM_MAX_TASKS; i++)
		if (!bcom_eng->tdt[i].stop) {	
			bcom_eng->tdt[i].stop = 0xfffffffful; 
			tasknum = i;
			break;
		}

	spin_unlock(&bcom_eng->lock);

	if (tasknum < 0)
		return NULL;

	
	tsk = kzalloc(sizeof(struct bcom_task) + priv_size, GFP_KERNEL);
	if (!tsk)
		goto error;

	tsk->tasknum = tasknum;
	if (priv_size)
		tsk->priv = (void*)tsk + sizeof(struct bcom_task);

	
	tsk->irq = irq_of_parse_and_map(bcom_eng->ofnode, tsk->tasknum);
	if (tsk->irq == NO_IRQ)
		goto error;

	
	if (bd_count) {
		tsk->cookie = kmalloc(sizeof(void*) * bd_count, GFP_KERNEL);
		if (!tsk->cookie)
			goto error;

		tsk->bd = bcom_sram_alloc(bd_count * bd_size, 4, &tsk->bd_pa);
		if (!tsk->bd)
			goto error;
		memset(tsk->bd, 0x00, bd_count * bd_size);

		tsk->num_bd = bd_count;
		tsk->bd_size = bd_size;
	}

	return tsk;

error:
	if (tsk) {
		if (tsk->irq != NO_IRQ)
			irq_dispose_mapping(tsk->irq);
		bcom_sram_free(tsk->bd);
		kfree(tsk->cookie);
		kfree(tsk);
	}

	bcom_eng->tdt[tasknum].stop = 0;

	return NULL;
}
EXPORT_SYMBOL_GPL(bcom_task_alloc);

void
bcom_task_free(struct bcom_task *tsk)
{
	
	bcom_disable_task(tsk->tasknum);

	
	bcom_eng->tdt[tsk->tasknum].start = 0;
	bcom_eng->tdt[tsk->tasknum].stop  = 0;

	
	irq_dispose_mapping(tsk->irq);
	bcom_sram_free(tsk->bd);
	kfree(tsk->cookie);
	kfree(tsk);
}
EXPORT_SYMBOL_GPL(bcom_task_free);

int
bcom_load_image(int task, u32 *task_image)
{
	struct bcom_task_header *hdr = (struct bcom_task_header *)task_image;
	struct bcom_tdt *tdt;
	u32 *desc, *var, *inc;
	u32 *desc_src, *var_src, *inc_src;

	
	if (hdr->magic != BCOM_TASK_MAGIC) {
		printk(KERN_ERR DRIVER_NAME
			": Trying to load invalid microcode\n");
		return -EINVAL;
	}

	if ((task < 0) || (task >= BCOM_MAX_TASKS)) {
		printk(KERN_ERR DRIVER_NAME
			": Trying to load invalid task %d\n", task);
		return -EINVAL;
	}

	
	tdt = &bcom_eng->tdt[task];

	if (tdt->start) {
		desc = bcom_task_desc(task);
		if (hdr->desc_size != bcom_task_num_descs(task)) {
			printk(KERN_ERR DRIVER_NAME
				": Trying to reload wrong task image "
				"(%d size %d/%d)!\n",
				task,
				hdr->desc_size,
				bcom_task_num_descs(task));
			return -EINVAL;
		}
	} else {
		phys_addr_t start_pa;

		desc = bcom_sram_alloc(hdr->desc_size * sizeof(u32), 4, &start_pa);
		if (!desc)
			return -ENOMEM;

		tdt->start = start_pa;
		tdt->stop = start_pa + ((hdr->desc_size-1) * sizeof(u32));
	}

	var = bcom_task_var(task);
	inc = bcom_task_inc(task);

	
	memset(var, 0x00, BCOM_VAR_SIZE);
	memset(inc, 0x00, BCOM_INC_SIZE);

	desc_src = (u32 *)(hdr + 1);
	var_src = desc_src + hdr->desc_size;
	inc_src = var_src + hdr->var_size;

	memcpy(desc, desc_src, hdr->desc_size * sizeof(u32));
	memcpy(var + hdr->first_var, var_src, hdr->var_size * sizeof(u32));
	memcpy(inc, inc_src, hdr->inc_size * sizeof(u32));

	return 0;
}
EXPORT_SYMBOL_GPL(bcom_load_image);

void
bcom_set_initiator(int task, int initiator)
{
	int i;
	int num_descs;
	u32 *desc;
	int next_drd_has_initiator;

	bcom_set_tcr_initiator(task, initiator);

	
	
	

	desc = bcom_task_desc(task);
	next_drd_has_initiator = 1;
	num_descs = bcom_task_num_descs(task);

	for (i=0; i<num_descs; i++, desc++) {
		if (!bcom_desc_is_drd(*desc))
			continue;
		if (next_drd_has_initiator)
			if (bcom_desc_initiator(*desc) != BCOM_INITIATOR_ALWAYS)
				bcom_set_desc_initiator(desc, initiator);
		next_drd_has_initiator = !bcom_drd_is_extended(*desc);
	}
}
EXPORT_SYMBOL_GPL(bcom_set_initiator);



void
bcom_enable(struct bcom_task *tsk)
{
	bcom_enable_task(tsk->tasknum);
}
EXPORT_SYMBOL_GPL(bcom_enable);

void
bcom_disable(struct bcom_task *tsk)
{
	bcom_disable_task(tsk->tasknum);
}
EXPORT_SYMBOL_GPL(bcom_disable);



static u32 fdt_ops[] = {
	0xa0045670,	
	0x80045670,	
	0x21800000,	
	0x21e00000,	
	0x21500000,	
	0x21400000,	
	0x21500000,	
	0x20400000,	
	0x20500000,	
	0x20800000,	
	0x20a00000,	
	0xc0170000,	
	0xc0145670,	
	0xc0345670,	
	0xa0076540,	
	0xa0000760,	
};


static int __devinit
bcom_engine_init(void)
{
	int task;
	phys_addr_t tdt_pa, ctx_pa, var_pa, fdt_pa;
	unsigned int tdt_size, ctx_size, var_size, fdt_size;

	
	tdt_size = BCOM_MAX_TASKS * sizeof(struct bcom_tdt);
	ctx_size = BCOM_MAX_TASKS * BCOM_CTX_SIZE;
	var_size = BCOM_MAX_TASKS * (BCOM_VAR_SIZE + BCOM_INC_SIZE);
	fdt_size = BCOM_FDT_SIZE;

	bcom_eng->tdt = bcom_sram_alloc(tdt_size, sizeof(u32), &tdt_pa);
	bcom_eng->ctx = bcom_sram_alloc(ctx_size, BCOM_CTX_ALIGN, &ctx_pa);
	bcom_eng->var = bcom_sram_alloc(var_size, BCOM_VAR_ALIGN, &var_pa);
	bcom_eng->fdt = bcom_sram_alloc(fdt_size, BCOM_FDT_ALIGN, &fdt_pa);

	if (!bcom_eng->tdt || !bcom_eng->ctx || !bcom_eng->var || !bcom_eng->fdt) {
		printk(KERN_ERR "DMA: SRAM alloc failed in engine init !\n");

		bcom_sram_free(bcom_eng->tdt);
		bcom_sram_free(bcom_eng->ctx);
		bcom_sram_free(bcom_eng->var);
		bcom_sram_free(bcom_eng->fdt);

		return -ENOMEM;
	}

	memset(bcom_eng->tdt, 0x00, tdt_size);
	memset(bcom_eng->ctx, 0x00, ctx_size);
	memset(bcom_eng->var, 0x00, var_size);
	memset(bcom_eng->fdt, 0x00, fdt_size);

	
	memcpy(&bcom_eng->fdt[48], fdt_ops, sizeof(fdt_ops));

	
	for (task=0; task<BCOM_MAX_TASKS; task++)
	{
		out_be16(&bcom_eng->regs->tcr[task], 0);
		out_8(&bcom_eng->regs->ipr[task], 0);

		bcom_eng->tdt[task].context	= ctx_pa;
		bcom_eng->tdt[task].var	= var_pa;
		bcom_eng->tdt[task].fdt	= fdt_pa;

		var_pa += BCOM_VAR_SIZE + BCOM_INC_SIZE;
		ctx_pa += BCOM_CTX_SIZE;
	}

	out_be32(&bcom_eng->regs->taskBar, tdt_pa);

	
	out_8(&bcom_eng->regs->ipr[BCOM_INITIATOR_ALWAYS], BCOM_IPR_ALWAYS);

	
	if ((mfspr(SPRN_SVR) & MPC5200_SVR_MASK) == MPC5200_SVR)
		bcom_disable_prefetch();

	
	spin_lock_init(&bcom_eng->lock);

	return 0;
}

static void
bcom_engine_cleanup(void)
{
	int task;

	
	for (task=0; task<BCOM_MAX_TASKS; task++)
	{
		out_be16(&bcom_eng->regs->tcr[task], 0);
		out_8(&bcom_eng->regs->ipr[task], 0);
	}

	out_be32(&bcom_eng->regs->taskBar, 0ul);

	
	bcom_sram_free(bcom_eng->tdt);
	bcom_sram_free(bcom_eng->ctx);
	bcom_sram_free(bcom_eng->var);
	bcom_sram_free(bcom_eng->fdt);
}



static int __devinit mpc52xx_bcom_probe(struct platform_device *op)
{
	struct device_node *ofn_sram;
	struct resource res_bcom;

	int rv;

	
	printk(KERN_INFO "DMA: MPC52xx BestComm driver\n");

	
	of_node_get(op->dev.of_node);

	
	ofn_sram = of_find_matching_node(NULL, mpc52xx_sram_ids);
	if (!ofn_sram) {
		printk(KERN_ERR DRIVER_NAME ": "
			"No SRAM found in device tree\n");
		rv = -ENODEV;
		goto error_ofput;
	}
	rv = bcom_sram_init(ofn_sram, DRIVER_NAME);
	of_node_put(ofn_sram);

	if (rv) {
		printk(KERN_ERR DRIVER_NAME ": "
			"Error in SRAM init\n");
		goto error_ofput;
	}

	
	bcom_eng = kzalloc(sizeof(struct bcom_engine), GFP_KERNEL);
	if (!bcom_eng) {
		printk(KERN_ERR DRIVER_NAME ": "
			"Can't allocate state structure\n");
		rv = -ENOMEM;
		goto error_sramclean;
	}

	
	bcom_eng->ofnode = op->dev.of_node;

	
	if (of_address_to_resource(op->dev.of_node, 0, &res_bcom)) {
		printk(KERN_ERR DRIVER_NAME ": "
			"Can't get resource\n");
		rv = -EINVAL;
		goto error_sramclean;
	}

	if (!request_mem_region(res_bcom.start, sizeof(struct mpc52xx_sdma),
				DRIVER_NAME)) {
		printk(KERN_ERR DRIVER_NAME ": "
			"Can't request registers region\n");
		rv = -EBUSY;
		goto error_sramclean;
	}

	bcom_eng->regs_base = res_bcom.start;
	bcom_eng->regs = ioremap(res_bcom.start, sizeof(struct mpc52xx_sdma));
	if (!bcom_eng->regs) {
		printk(KERN_ERR DRIVER_NAME ": "
			"Can't map registers\n");
		rv = -ENOMEM;
		goto error_release;
	}

	
	rv = bcom_engine_init();
	if (rv)
		goto error_unmap;

	
	printk(KERN_INFO "DMA: MPC52xx BestComm engine @%08lx ok !\n",
		(long)bcom_eng->regs_base);

	return 0;

	
error_unmap:
	iounmap(bcom_eng->regs);
error_release:
	release_mem_region(res_bcom.start, sizeof(struct mpc52xx_sdma));
error_sramclean:
	kfree(bcom_eng);
	bcom_sram_cleanup();
error_ofput:
	of_node_put(op->dev.of_node);

	printk(KERN_ERR "DMA: MPC52xx BestComm init failed !\n");

	return rv;
}


static int mpc52xx_bcom_remove(struct platform_device *op)
{
	
	bcom_engine_cleanup();

	
	bcom_sram_cleanup();

	
	iounmap(bcom_eng->regs);
	release_mem_region(bcom_eng->regs_base, sizeof(struct mpc52xx_sdma));

	
	of_node_put(bcom_eng->ofnode);

	
	kfree(bcom_eng);
	bcom_eng = NULL;

	return 0;
}

static struct of_device_id mpc52xx_bcom_of_match[] = {
	{ .compatible = "fsl,mpc5200-bestcomm", },
	{ .compatible = "mpc5200-bestcomm", },
	{},
};

MODULE_DEVICE_TABLE(of, mpc52xx_bcom_of_match);


static struct platform_driver mpc52xx_bcom_of_platform_driver = {
	.probe		= mpc52xx_bcom_probe,
	.remove		= mpc52xx_bcom_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mpc52xx_bcom_of_match,
	},
};



static int __init
mpc52xx_bcom_init(void)
{
	return platform_driver_register(&mpc52xx_bcom_of_platform_driver);
}

static void __exit
mpc52xx_bcom_exit(void)
{
	platform_driver_unregister(&mpc52xx_bcom_of_platform_driver);
}

subsys_initcall(mpc52xx_bcom_init);
module_exit(mpc52xx_bcom_exit);

MODULE_DESCRIPTION("Freescale MPC52xx BestComm DMA");
MODULE_AUTHOR("Sylvain Munaut <tnt@246tNt.com>");
MODULE_AUTHOR("Andrey Volkov <avolkov@varma-el.com>");
MODULE_AUTHOR("Dale Farnsworth <dfarnsworth@mvista.com>");
MODULE_LICENSE("GPL v2");

