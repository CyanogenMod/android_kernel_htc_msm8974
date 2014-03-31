/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/genalloc.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <mach/ocmem_priv.h>

#define RDM_MAX_ENTRIES 32
#define RDM_MAX_CLIENTS 2

#define DM_BLOCK_128 0x0
#define DM_BLOCK_256 0x1
#define DM_BR_ID_LPASS 0x0
#define DM_BR_ID_GPS 0x1

#define DM_INTR_CLR (0x8)
#define DM_INTR_MASK (0xC)
#define DM_INT_STATUS (0x10)
#define DM_GEN_STATUS (0x14)
#define DM_CLR_OFFSET (0x18)
#define DM_CLR_SIZE (0x1C)
#define DM_CLR_PATTERN (0x20)
#define DM_CLR_TRIGGER (0x24)
#define DM_CTRL (0x1000)
#define DM_TBL_BASE (0x1010)
#define DM_TBL_IDX(x) ((x) * 0x18)
#define DM_TBL_n(x) (DM_TBL_BASE + (DM_TBL_IDX(x)))
#define DM_TBL_n_offset(x) DM_TBL_n(x)
#define DM_TBL_n_size(x) (DM_TBL_n(x)+0x4)
#define DM_TBL_n_paddr(x) (DM_TBL_n(x)+0x8)
#define DM_TBL_n_ctrl(x) (DM_TBL_n(x)+0x10)

#define BR_CTRL (0x0)
#define BR_CLIENT_BASE (0x4)
#define BR_CLIENT_n_IDX(x) ((x) * 0x4)
#define BR_CLIENT_n_ctrl(x) (BR_CLIENT_BASE + (BR_CLIENT_n_IDX(x)))
#define BR_STATUS (0x14)
#define BR_LAST_ADDR (0x18)
#define BR_CLIENT0_MASK	(0x1000)
#define BR_CLIENT1_MASK	(0x2010)

#define BR_TBL_BASE (0x40)
#define BR_TBL_IDX(x) ((x) * 0x18)
#define BR_TBL_n(x) (BR_TBL_BASE + (BR_TBL_IDX(x)))
#define BR_TBL_n_offset(x) BR_TBL_n(x)
#define BR_TBL_n_size(x) (BR_TBL_n(x)+0x4)
#define BR_TBL_n_paddr(x) (BR_TBL_n(x)+0x8)
#define BR_TBL_n_ctrl(x) (BR_TBL_n(x)+0x10)

#define BR_TBL_ENTRY_ENABLE 0x1
#define BR_TBL_START 0x0
#define BR_TBL_END 0x8
#define BR_RW_SHIFT 0x1

#define DM_TBL_START 0x10
#define DM_TBL_END 0x18
#define DM_CLIENT_SHIFT 0x8
#define DM_BR_ID_SHIFT 0x4
#define DM_BR_BLK_SHIFT 0x1
#define DM_DIR_SHIFT 0x0

#define DM_DONE 0x1
#define DM_MASK_RESET 0x0
#define DM_INTR_RESET 0x20003
#define DM_CLR_ENABLE 0x1

static void *br_base;
static void *dm_base;

struct completion dm_clear_event;
struct completion dm_transfer_event;
struct ocmem_br_table {
	unsigned int offset;
	unsigned int size;
	unsigned int ddr_low;
	unsigned int ddr_high;
	unsigned int ctrl;
} br_table[RDM_MAX_ENTRIES];

struct ocmem_dm_table {
	unsigned int offset;
	unsigned int size;
	unsigned int ddr_low;
	unsigned int ddr_high;
	unsigned int ctrl;
} dm_table[RDM_MAX_ENTRIES];

static inline int client_ctrl_id(int id)
{
	return (id == OCMEM_SENSORS) ? 1 : 0;
}

static inline int client_slot_start(int id)
{

	return client_ctrl_id(id) * 16;
}

static irqreturn_t ocmem_dm_irq_handler(int irq, void *dev_id)
{
	unsigned status;
	unsigned irq_status;
	status = ocmem_read(dm_base + DM_GEN_STATUS);
	irq_status = ocmem_read(dm_base + DM_INT_STATUS);
	pr_debug("irq:dm_status %x irq_status %x\n", status, irq_status);
	if (irq_status & BIT(0)) {
		pr_debug("Data mover completed\n");
		ocmem_write(BIT(0), dm_base + DM_INTR_CLR);
		pr_debug("Last re-mapped address block %x\n",
				ocmem_read(br_base + BR_LAST_ADDR));
		complete(&dm_transfer_event);
	} else if (irq_status & BIT(1)) {
		pr_debug("Data clear engine completed\n");
		ocmem_write(BIT(1), dm_base + DM_INTR_CLR);
		complete(&dm_clear_event);
	} else {
		BUG_ON(1);
	}
	return IRQ_HANDLED;
}

#ifdef CONFIG_MSM_OCMEM_NONSECURE
int ocmem_clear(unsigned long start, unsigned long size)
{
	INIT_COMPLETION(dm_clear_event);
	
	ocmem_write(DM_MASK_RESET, dm_base + DM_INTR_MASK);
	
	ocmem_write(DM_INTR_RESET, dm_base + DM_INTR_CLR);
	
	ocmem_write(start, dm_base + DM_CLR_OFFSET);
	
	ocmem_write(size, dm_base + DM_CLR_SIZE);
	
	ocmem_write(0x4D4D434F, dm_base + DM_CLR_PATTERN);
	mb();
	
	ocmem_write(DM_CLR_ENABLE, dm_base + DM_CLR_TRIGGER);

	wait_for_completion(&dm_clear_event);

	return 0;
}
#else
int ocmem_clear(unsigned long start, unsigned long size)
{
	return 0;
}
#endif

int ocmem_rdm_transfer(int id, struct ocmem_map_list *clist,
			unsigned long start, int direction)
{
	int num_chunks = clist->num_chunks;
	int slot = client_slot_start(id);
	int table_start = 0;
	int table_end = 0;
	int br_ctrl = 0;
	int br_id = 0;
	int client_id = 0;
	int dm_ctrl = 0;
	int i = 0;
	int j = 0;
	int status = 0;
	int rc = 0;

	rc = ocmem_enable_core_clock();

	if (rc < 0) {
		pr_err("RDM transfer failed for client %s (id: %d)\n",
				get_name(id), id);
		return rc;
	}

	
	ocmem_write(DM_MASK_RESET, dm_base + DM_INTR_MASK);
	
	ocmem_write(DM_INTR_RESET, dm_base + DM_INTR_CLR);

	for (i = 0, j = slot; i < num_chunks; i++, j++) {

		struct ocmem_chunk *chunk = &clist->chunks[i];
		int sz = chunk->size;
		int paddr = chunk->ddr_paddr;
		int tbl_n_ctrl = 0;

		tbl_n_ctrl |= BR_TBL_ENTRY_ENABLE;
		if (chunk->ro)
			tbl_n_ctrl |= (1 << BR_RW_SHIFT);

		
		ocmem_write(start, br_base + BR_TBL_n_offset(j));
		ocmem_write(sz, br_base + BR_TBL_n_size(j));
		ocmem_write(paddr, br_base + BR_TBL_n_paddr(j));
		ocmem_write(tbl_n_ctrl, br_base + BR_TBL_n_ctrl(j));

		ocmem_write(start, dm_base + DM_TBL_n_offset(j));
		ocmem_write(sz, dm_base + DM_TBL_n_size(j));
		ocmem_write(paddr, dm_base + DM_TBL_n_paddr(j));
		ocmem_write(tbl_n_ctrl, dm_base + DM_TBL_n_ctrl(j));

		start += sz;
	}

	br_id = client_ctrl_id(id);
	table_start = slot;
	table_end = slot + num_chunks - 1;
	br_ctrl |= (table_start << BR_TBL_START);
	br_ctrl |= (table_end << BR_TBL_END);

	ocmem_write(br_ctrl, (br_base + BR_CLIENT_n_ctrl(br_id)));
	
	ocmem_write(0x1, br_base + BR_CTRL);

	
	dm_ctrl |= (table_start << DM_TBL_START);
	dm_ctrl |= (table_end << DM_TBL_END);

	client_id = client_ctrl_id(id);
	dm_ctrl |= (client_id << DM_CLIENT_SHIFT);
	dm_ctrl |= (DM_BR_ID_LPASS << DM_BR_ID_SHIFT);
	dm_ctrl |= (DM_BLOCK_256 << DM_BR_BLK_SHIFT);
	dm_ctrl |= (direction << DM_DIR_SHIFT);

	status = ocmem_read(dm_base + DM_GEN_STATUS);
	pr_debug("Transfer status before %x\n", status);
	INIT_COMPLETION(dm_transfer_event);
	mb();
	
	ocmem_write(dm_ctrl, dm_base + DM_CTRL);
	pr_debug("ocmem: rdm: dm_ctrl %x br_ctrl %x\n", dm_ctrl, br_ctrl);

	wait_for_completion(&dm_transfer_event);
	pr_debug("Completed transferring %d segments\n", num_chunks);
	ocmem_disable_core_clock();
	return 0;
}

int ocmem_rdm_init(struct platform_device *pdev)
{

	struct ocmem_plat_data *pdata = NULL;
	int rc = 0;

	pdata = platform_get_drvdata(pdev);

	br_base = pdata->br_base;
	dm_base = pdata->dm_base;

	rc = devm_request_irq(&pdev->dev, pdata->dm_irq, ocmem_dm_irq_handler,
				IRQF_TRIGGER_RISING, "ocmem_dm_irq", pdata);

	if (rc) {
		dev_err(&pdev->dev, "Failed to request dm irq");
		return -EINVAL;
	}

	rc = ocmem_enable_core_clock();

	if (rc < 0) {
		pr_err("RDM initialization failed\n");
		return rc;
	}

	init_completion(&dm_clear_event);
	init_completion(&dm_transfer_event);
	ocmem_disable_core_clock();
	return 0;
}
