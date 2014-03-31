/*
 * Bestcomm ATA task driver
 *
 *
 * Patterned after bestcomm/fec.c by Dale Farnsworth <dfarnsworth@mvista.com>
 *                                   2003-2004 (c) MontaVista, Software, Inc.
 *
 * Copyright (C) 2006-2007 Sylvain Munaut <tnt@246tNt.com>
 * Copyright (C) 2006      Freescale - John Rigby
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/io.h>

#include "bestcomm.h"
#include "bestcomm_priv.h"
#include "ata.h"



extern u32 bcom_ata_task[];

struct bcom_ata_var {
	u32 enable;		
	u32 bd_base;		
	u32 bd_last;		
	u32 bd_start;		
	u32 buffer_size;	
};

struct bcom_ata_inc {
	u16 pad0;
	s16 incr_bytes;
	u16 pad1;
	s16 incr_dst;
	u16 pad2;
	s16 incr_src;
};



struct bcom_task *
bcom_ata_init(int queue_len, int maxbufsize)
{
	struct bcom_task *tsk;
	struct bcom_ata_var *var;
	struct bcom_ata_inc *inc;

	
	bcom_disable_prefetch();

	tsk = bcom_task_alloc(queue_len, sizeof(struct bcom_ata_bd), 0);
	if (!tsk)
		return NULL;

	tsk->flags = BCOM_FLAGS_NONE;

	bcom_ata_reset_bd(tsk);

	var = (struct bcom_ata_var *) bcom_task_var(tsk->tasknum);
	inc = (struct bcom_ata_inc *) bcom_task_inc(tsk->tasknum);

	if (bcom_load_image(tsk->tasknum, bcom_ata_task)) {
		bcom_task_free(tsk);
		return NULL;
	}

	var->enable	= bcom_eng->regs_base +
				offsetof(struct mpc52xx_sdma, tcr[tsk->tasknum]);
	var->bd_base	= tsk->bd_pa;
	var->bd_last	= tsk->bd_pa + ((tsk->num_bd-1) * tsk->bd_size);
	var->bd_start	= tsk->bd_pa;
	var->buffer_size = maxbufsize;

	
	bcom_set_task_pragma(tsk->tasknum, BCOM_ATA_PRAGMA);
	bcom_set_task_auto_start(tsk->tasknum, tsk->tasknum);

	out_8(&bcom_eng->regs->ipr[BCOM_INITIATOR_ATA_RX], BCOM_IPR_ATA_RX);
	out_8(&bcom_eng->regs->ipr[BCOM_INITIATOR_ATA_TX], BCOM_IPR_ATA_TX);

	out_be32(&bcom_eng->regs->IntPend, 1<<tsk->tasknum); 

	return tsk;
}
EXPORT_SYMBOL_GPL(bcom_ata_init);

void bcom_ata_rx_prepare(struct bcom_task *tsk)
{
	struct bcom_ata_inc *inc;

	inc = (struct bcom_ata_inc *) bcom_task_inc(tsk->tasknum);

	inc->incr_bytes	= -(s16)sizeof(u32);
	inc->incr_src	= 0;
	inc->incr_dst	= sizeof(u32);

	bcom_set_initiator(tsk->tasknum, BCOM_INITIATOR_ATA_RX);
}
EXPORT_SYMBOL_GPL(bcom_ata_rx_prepare);

void bcom_ata_tx_prepare(struct bcom_task *tsk)
{
	struct bcom_ata_inc *inc;

	inc = (struct bcom_ata_inc *) bcom_task_inc(tsk->tasknum);

	inc->incr_bytes	= -(s16)sizeof(u32);
	inc->incr_src	= sizeof(u32);
	inc->incr_dst	= 0;

	bcom_set_initiator(tsk->tasknum, BCOM_INITIATOR_ATA_TX);
}
EXPORT_SYMBOL_GPL(bcom_ata_tx_prepare);

void bcom_ata_reset_bd(struct bcom_task *tsk)
{
	struct bcom_ata_var *var;

	
	memset(tsk->bd, 0x00, tsk->num_bd * tsk->bd_size);

	tsk->index = 0;
	tsk->outdex = 0;

	var = (struct bcom_ata_var *) bcom_task_var(tsk->tasknum);
	var->bd_start = var->bd_base;
}
EXPORT_SYMBOL_GPL(bcom_ata_reset_bd);

void bcom_ata_release(struct bcom_task *tsk)
{
	
	bcom_task_free(tsk);
}
EXPORT_SYMBOL_GPL(bcom_ata_release);


MODULE_DESCRIPTION("BestComm ATA task driver");
MODULE_AUTHOR("John Rigby");
MODULE_LICENSE("GPL v2");

