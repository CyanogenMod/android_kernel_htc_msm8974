/*
 * EDMA3 support for DaVinci
 *
 * Copyright (C) 2006-2009 Texas Instruments.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <mach/edma.h>

#define PARM_OPT		0x00
#define PARM_SRC		0x04
#define PARM_A_B_CNT		0x08
#define PARM_DST		0x0c
#define PARM_SRC_DST_BIDX	0x10
#define PARM_LINK_BCNTRLD	0x14
#define PARM_SRC_DST_CIDX	0x18
#define PARM_CCNT		0x1c

#define PARM_SIZE		0x20

#define SH_ER		0x00	
#define SH_ECR		0x08	
#define SH_ESR		0x10	
#define SH_CER		0x18	
#define SH_EER		0x20	
#define SH_EECR		0x28	
#define SH_EESR		0x30	
#define SH_SER		0x38	
#define SH_SECR		0x40	
#define SH_IER		0x50	
#define SH_IECR		0x58	
#define SH_IESR		0x60	
#define SH_IPR		0x68	
#define SH_ICR		0x70	
#define SH_IEVAL	0x78
#define SH_QER		0x80
#define SH_QEER		0x84
#define SH_QEECR	0x88
#define SH_QEESR	0x8c
#define SH_QSER		0x90
#define SH_QSECR	0x94
#define SH_SIZE		0x200

#define EDMA_REV	0x0000
#define EDMA_CCCFG	0x0004
#define EDMA_QCHMAP	0x0200	
#define EDMA_DMAQNUM	0x0240	
#define EDMA_QDMAQNUM	0x0260
#define EDMA_QUETCMAP	0x0280
#define EDMA_QUEPRI	0x0284
#define EDMA_EMR	0x0300	
#define EDMA_EMCR	0x0308	
#define EDMA_QEMR	0x0310
#define EDMA_QEMCR	0x0314
#define EDMA_CCERR	0x0318
#define EDMA_CCERRCLR	0x031c
#define EDMA_EEVAL	0x0320
#define EDMA_DRAE	0x0340	
#define EDMA_QRAE	0x0380	
#define EDMA_QUEEVTENTRY	0x0400	
#define EDMA_QSTAT	0x0600	
#define EDMA_QWMTHRA	0x0620
#define EDMA_QWMTHRB	0x0624
#define EDMA_CCSTAT	0x0640

#define EDMA_M		0x1000	
#define EDMA_ECR	0x1008
#define EDMA_ECRH	0x100C
#define EDMA_SHADOW0	0x2000	
#define EDMA_PARM	0x4000	

#define PARM_OFFSET(param_no)	(EDMA_PARM + ((param_no) << 5))

#define EDMA_DCHMAP	0x0100  
#define CHMAP_EXIST	BIT(24)

#define EDMA_MAX_DMACH           64
#define EDMA_MAX_PARAMENTRY     512


static void __iomem *edmacc_regs_base[EDMA_MAX_CC];

static inline unsigned int edma_read(unsigned ctlr, int offset)
{
	return (unsigned int)__raw_readl(edmacc_regs_base[ctlr] + offset);
}

static inline void edma_write(unsigned ctlr, int offset, int val)
{
	__raw_writel(val, edmacc_regs_base[ctlr] + offset);
}
static inline void edma_modify(unsigned ctlr, int offset, unsigned and,
		unsigned or)
{
	unsigned val = edma_read(ctlr, offset);
	val &= and;
	val |= or;
	edma_write(ctlr, offset, val);
}
static inline void edma_and(unsigned ctlr, int offset, unsigned and)
{
	unsigned val = edma_read(ctlr, offset);
	val &= and;
	edma_write(ctlr, offset, val);
}
static inline void edma_or(unsigned ctlr, int offset, unsigned or)
{
	unsigned val = edma_read(ctlr, offset);
	val |= or;
	edma_write(ctlr, offset, val);
}
static inline unsigned int edma_read_array(unsigned ctlr, int offset, int i)
{
	return edma_read(ctlr, offset + (i << 2));
}
static inline void edma_write_array(unsigned ctlr, int offset, int i,
		unsigned val)
{
	edma_write(ctlr, offset + (i << 2), val);
}
static inline void edma_modify_array(unsigned ctlr, int offset, int i,
		unsigned and, unsigned or)
{
	edma_modify(ctlr, offset + (i << 2), and, or);
}
static inline void edma_or_array(unsigned ctlr, int offset, int i, unsigned or)
{
	edma_or(ctlr, offset + (i << 2), or);
}
static inline void edma_or_array2(unsigned ctlr, int offset, int i, int j,
		unsigned or)
{
	edma_or(ctlr, offset + ((i*2 + j) << 2), or);
}
static inline void edma_write_array2(unsigned ctlr, int offset, int i, int j,
		unsigned val)
{
	edma_write(ctlr, offset + ((i*2 + j) << 2), val);
}
static inline unsigned int edma_shadow0_read(unsigned ctlr, int offset)
{
	return edma_read(ctlr, EDMA_SHADOW0 + offset);
}
static inline unsigned int edma_shadow0_read_array(unsigned ctlr, int offset,
		int i)
{
	return edma_read(ctlr, EDMA_SHADOW0 + offset + (i << 2));
}
static inline void edma_shadow0_write(unsigned ctlr, int offset, unsigned val)
{
	edma_write(ctlr, EDMA_SHADOW0 + offset, val);
}
static inline void edma_shadow0_write_array(unsigned ctlr, int offset, int i,
		unsigned val)
{
	edma_write(ctlr, EDMA_SHADOW0 + offset + (i << 2), val);
}
static inline unsigned int edma_parm_read(unsigned ctlr, int offset,
		int param_no)
{
	return edma_read(ctlr, EDMA_PARM + offset + (param_no << 5));
}
static inline void edma_parm_write(unsigned ctlr, int offset, int param_no,
		unsigned val)
{
	edma_write(ctlr, EDMA_PARM + offset + (param_no << 5), val);
}
static inline void edma_parm_modify(unsigned ctlr, int offset, int param_no,
		unsigned and, unsigned or)
{
	edma_modify(ctlr, EDMA_PARM + offset + (param_no << 5), and, or);
}
static inline void edma_parm_and(unsigned ctlr, int offset, int param_no,
		unsigned and)
{
	edma_and(ctlr, EDMA_PARM + offset + (param_no << 5), and);
}
static inline void edma_parm_or(unsigned ctlr, int offset, int param_no,
		unsigned or)
{
	edma_or(ctlr, EDMA_PARM + offset + (param_no << 5), or);
}

static inline void set_bits(int offset, int len, unsigned long *p)
{
	for (; len > 0; len--)
		set_bit(offset + (len - 1), p);
}

static inline void clear_bits(int offset, int len, unsigned long *p)
{
	for (; len > 0; len--)
		clear_bit(offset + (len - 1), p);
}


struct edma {
	
	unsigned	num_channels;
	unsigned	num_region;
	unsigned	num_slots;
	unsigned	num_tc;
	unsigned	num_cc;
	enum dma_event_q 	default_queue;

	
	const s8	*noevent;

	DECLARE_BITMAP(edma_inuse, EDMA_MAX_PARAMENTRY);

	DECLARE_BITMAP(edma_unused, EDMA_MAX_DMACH);

	unsigned	irq_res_start;
	unsigned	irq_res_end;

	struct dma_interrupt_data {
		void (*callback)(unsigned channel, unsigned short ch_status,
				void *data);
		void *data;
	} intr_data[EDMA_MAX_DMACH];
};

static struct edma *edma_cc[EDMA_MAX_CC];
static int arch_num_cc;

static const struct edmacc_param dummy_paramset = {
	.link_bcntrld = 0xffff,
	.ccnt = 1,
};


static void map_dmach_queue(unsigned ctlr, unsigned ch_no,
		enum dma_event_q queue_no)
{
	int bit = (ch_no & 0x7) * 4;

	
	if (queue_no == EVENTQ_DEFAULT)
		queue_no = edma_cc[ctlr]->default_queue;

	queue_no &= 7;
	edma_modify_array(ctlr, EDMA_DMAQNUM, (ch_no >> 3),
			~(0x7 << bit), queue_no << bit);
}

static void __init map_queue_tc(unsigned ctlr, int queue_no, int tc_no)
{
	int bit = queue_no * 4;
	edma_modify(ctlr, EDMA_QUETCMAP, ~(0x7 << bit), ((tc_no & 0x7) << bit));
}

static void __init assign_priority_to_queue(unsigned ctlr, int queue_no,
		int priority)
{
	int bit = queue_no * 4;
	edma_modify(ctlr, EDMA_QUEPRI, ~(0x7 << bit),
			((priority & 0x7) << bit));
}

static void __init map_dmach_param(unsigned ctlr)
{
	int i;
	for (i = 0; i < EDMA_MAX_DMACH; i++)
		edma_write_array(ctlr, EDMA_DCHMAP , i , (i << 5));
}

static inline void
setup_dma_interrupt(unsigned lch,
	void (*callback)(unsigned channel, u16 ch_status, void *data),
	void *data)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(lch);
	lch = EDMA_CHAN_SLOT(lch);

	if (!callback)
		edma_shadow0_write_array(ctlr, SH_IECR, lch >> 5,
				BIT(lch & 0x1f));

	edma_cc[ctlr]->intr_data[lch].callback = callback;
	edma_cc[ctlr]->intr_data[lch].data = data;

	if (callback) {
		edma_shadow0_write_array(ctlr, SH_ICR, lch >> 5,
				BIT(lch & 0x1f));
		edma_shadow0_write_array(ctlr, SH_IESR, lch >> 5,
				BIT(lch & 0x1f));
	}
}

static int irq2ctlr(int irq)
{
	if (irq >= edma_cc[0]->irq_res_start && irq <= edma_cc[0]->irq_res_end)
		return 0;
	else if (irq >= edma_cc[1]->irq_res_start &&
		irq <= edma_cc[1]->irq_res_end)
		return 1;

	return -1;
}

static irqreturn_t dma_irq_handler(int irq, void *data)
{
	int i;
	int ctlr;
	unsigned int cnt = 0;

	ctlr = irq2ctlr(irq);
	if (ctlr < 0)
		return IRQ_NONE;

	dev_dbg(data, "dma_irq_handler\n");

	if ((edma_shadow0_read_array(ctlr, SH_IPR, 0) == 0) &&
	    (edma_shadow0_read_array(ctlr, SH_IPR, 1) == 0))
		return IRQ_NONE;

	while (1) {
		int j;
		if (edma_shadow0_read_array(ctlr, SH_IPR, 0) &
				edma_shadow0_read_array(ctlr, SH_IER, 0))
			j = 0;
		else if (edma_shadow0_read_array(ctlr, SH_IPR, 1) &
				edma_shadow0_read_array(ctlr, SH_IER, 1))
			j = 1;
		else
			break;
		dev_dbg(data, "IPR%d %08x\n", j,
				edma_shadow0_read_array(ctlr, SH_IPR, j));
		for (i = 0; i < 32; i++) {
			int k = (j << 5) + i;
			if ((edma_shadow0_read_array(ctlr, SH_IPR, j) & BIT(i))
					&& (edma_shadow0_read_array(ctlr,
							SH_IER, j) & BIT(i))) {
				
				edma_shadow0_write_array(ctlr, SH_ICR, j,
							BIT(i));
				if (edma_cc[ctlr]->intr_data[k].callback)
					edma_cc[ctlr]->intr_data[k].callback(
						k, DMA_COMPLETE,
						edma_cc[ctlr]->intr_data[k].
						data);
			}
		}
		cnt++;
		if (cnt > 10)
			break;
	}
	edma_shadow0_write(ctlr, SH_IEVAL, 1);
	return IRQ_HANDLED;
}

static irqreturn_t dma_ccerr_handler(int irq, void *data)
{
	int i;
	int ctlr;
	unsigned int cnt = 0;

	ctlr = irq2ctlr(irq);
	if (ctlr < 0)
		return IRQ_NONE;

	dev_dbg(data, "dma_ccerr_handler\n");

	if ((edma_read_array(ctlr, EDMA_EMR, 0) == 0) &&
	    (edma_read_array(ctlr, EDMA_EMR, 1) == 0) &&
	    (edma_read(ctlr, EDMA_QEMR) == 0) &&
	    (edma_read(ctlr, EDMA_CCERR) == 0))
		return IRQ_NONE;

	while (1) {
		int j = -1;
		if (edma_read_array(ctlr, EDMA_EMR, 0))
			j = 0;
		else if (edma_read_array(ctlr, EDMA_EMR, 1))
			j = 1;
		if (j >= 0) {
			dev_dbg(data, "EMR%d %08x\n", j,
					edma_read_array(ctlr, EDMA_EMR, j));
			for (i = 0; i < 32; i++) {
				int k = (j << 5) + i;
				if (edma_read_array(ctlr, EDMA_EMR, j) &
							BIT(i)) {
					
					edma_write_array(ctlr, EDMA_EMCR, j,
							BIT(i));
					
					edma_shadow0_write_array(ctlr, SH_SECR,
								j, BIT(i));
					if (edma_cc[ctlr]->intr_data[k].
								callback) {
						edma_cc[ctlr]->intr_data[k].
						callback(k,
						DMA_CC_ERROR,
						edma_cc[ctlr]->intr_data
						[k].data);
					}
				}
			}
		} else if (edma_read(ctlr, EDMA_QEMR)) {
			dev_dbg(data, "QEMR %02x\n",
				edma_read(ctlr, EDMA_QEMR));
			for (i = 0; i < 8; i++) {
				if (edma_read(ctlr, EDMA_QEMR) & BIT(i)) {
					
					edma_write(ctlr, EDMA_QEMCR, BIT(i));
					edma_shadow0_write(ctlr, SH_QSECR,
								BIT(i));

					
				}
			}
		} else if (edma_read(ctlr, EDMA_CCERR)) {
			dev_dbg(data, "CCERR %08x\n",
				edma_read(ctlr, EDMA_CCERR));
			for (i = 0; i < 8; i++) {
				if (edma_read(ctlr, EDMA_CCERR) & BIT(i)) {
					
					edma_write(ctlr, EDMA_CCERRCLR, BIT(i));

					
				}
			}
		}
		if ((edma_read_array(ctlr, EDMA_EMR, 0) == 0) &&
		    (edma_read_array(ctlr, EDMA_EMR, 1) == 0) &&
		    (edma_read(ctlr, EDMA_QEMR) == 0) &&
		    (edma_read(ctlr, EDMA_CCERR) == 0))
			break;
		cnt++;
		if (cnt > 10)
			break;
	}
	edma_write(ctlr, EDMA_EEVAL, 1);
	return IRQ_HANDLED;
}


#define tc_errs_handled	false	

static irqreturn_t dma_tc0err_handler(int irq, void *data)
{
	dev_dbg(data, "dma_tc0err_handler\n");
	return IRQ_HANDLED;
}

static irqreturn_t dma_tc1err_handler(int irq, void *data)
{
	dev_dbg(data, "dma_tc1err_handler\n");
	return IRQ_HANDLED;
}

static int reserve_contiguous_slots(int ctlr, unsigned int id,
				     unsigned int num_slots,
				     unsigned int start_slot)
{
	int i, j;
	unsigned int count = num_slots;
	int stop_slot = start_slot;
	DECLARE_BITMAP(tmp_inuse, EDMA_MAX_PARAMENTRY);

	for (i = start_slot; i < edma_cc[ctlr]->num_slots; ++i) {
		j = EDMA_CHAN_SLOT(i);
		if (!test_and_set_bit(j, edma_cc[ctlr]->edma_inuse)) {
			
			if (count == num_slots)
				stop_slot = i;

			count--;
			set_bit(j, tmp_inuse);

			if (count == 0)
				break;
		} else {
			clear_bit(j, tmp_inuse);

			if (id == EDMA_CONT_PARAMS_FIXED_EXACT) {
				stop_slot = i;
				break;
			} else {
				count = num_slots;
			}
		}
	}

	if (i == edma_cc[ctlr]->num_slots)
		stop_slot = i;

	for (j = start_slot; j < stop_slot; j++)
		if (test_bit(j, tmp_inuse))
			clear_bit(j, edma_cc[ctlr]->edma_inuse);

	if (count)
		return -EBUSY;

	for (j = i - num_slots + 1; j <= i; ++j)
		memcpy_toio(edmacc_regs_base[ctlr] + PARM_OFFSET(j),
			&dummy_paramset, PARM_SIZE);

	return EDMA_CTLR_CHAN(ctlr, i - num_slots + 1);
}

static int prepare_unused_channel_list(struct device *dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	int i, ctlr;

	for (i = 0; i < pdev->num_resources; i++) {
		if ((pdev->resource[i].flags & IORESOURCE_DMA) &&
				(int)pdev->resource[i].start >= 0) {
			ctlr = EDMA_CTLR(pdev->resource[i].start);
			clear_bit(EDMA_CHAN_SLOT(pdev->resource[i].start),
					edma_cc[ctlr]->edma_unused);
		}
	}

	return 0;
}


static bool unused_chan_list_done;


int edma_alloc_channel(int channel,
		void (*callback)(unsigned channel, u16 ch_status, void *data),
		void *data,
		enum dma_event_q eventq_no)
{
	unsigned i, done = 0, ctlr = 0;
	int ret = 0;

	if (!unused_chan_list_done) {
		ret = bus_for_each_dev(&platform_bus_type, NULL, NULL,
				prepare_unused_channel_list);
		if (ret < 0)
			return ret;

		unused_chan_list_done = true;
	}

	if (channel >= 0) {
		ctlr = EDMA_CTLR(channel);
		channel = EDMA_CHAN_SLOT(channel);
	}

	if (channel < 0) {
		for (i = 0; i < arch_num_cc; i++) {
			channel = 0;
			for (;;) {
				channel = find_next_bit(edma_cc[i]->edma_unused,
						edma_cc[i]->num_channels,
						channel);
				if (channel == edma_cc[i]->num_channels)
					break;
				if (!test_and_set_bit(channel,
						edma_cc[i]->edma_inuse)) {
					done = 1;
					ctlr = i;
					break;
				}
				channel++;
			}
			if (done)
				break;
		}
		if (!done)
			return -ENOMEM;
	} else if (channel >= edma_cc[ctlr]->num_channels) {
		return -EINVAL;
	} else if (test_and_set_bit(channel, edma_cc[ctlr]->edma_inuse)) {
		return -EBUSY;
	}

	
	edma_or_array2(ctlr, EDMA_DRAE, 0, channel >> 5, BIT(channel & 0x1f));

	
	edma_stop(EDMA_CTLR_CHAN(ctlr, channel));
	memcpy_toio(edmacc_regs_base[ctlr] + PARM_OFFSET(channel),
			&dummy_paramset, PARM_SIZE);

	if (callback)
		setup_dma_interrupt(EDMA_CTLR_CHAN(ctlr, channel),
					callback, data);

	map_dmach_queue(ctlr, channel, eventq_no);

	return EDMA_CTLR_CHAN(ctlr, channel);
}
EXPORT_SYMBOL(edma_alloc_channel);


void edma_free_channel(unsigned channel)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(channel);
	channel = EDMA_CHAN_SLOT(channel);

	if (channel >= edma_cc[ctlr]->num_channels)
		return;

	setup_dma_interrupt(channel, NULL, NULL);
	

	memcpy_toio(edmacc_regs_base[ctlr] + PARM_OFFSET(channel),
			&dummy_paramset, PARM_SIZE);
	clear_bit(channel, edma_cc[ctlr]->edma_inuse);
}
EXPORT_SYMBOL(edma_free_channel);

int edma_alloc_slot(unsigned ctlr, int slot)
{
	if (slot >= 0)
		slot = EDMA_CHAN_SLOT(slot);

	if (slot < 0) {
		slot = edma_cc[ctlr]->num_channels;
		for (;;) {
			slot = find_next_zero_bit(edma_cc[ctlr]->edma_inuse,
					edma_cc[ctlr]->num_slots, slot);
			if (slot == edma_cc[ctlr]->num_slots)
				return -ENOMEM;
			if (!test_and_set_bit(slot, edma_cc[ctlr]->edma_inuse))
				break;
		}
	} else if (slot < edma_cc[ctlr]->num_channels ||
			slot >= edma_cc[ctlr]->num_slots) {
		return -EINVAL;
	} else if (test_and_set_bit(slot, edma_cc[ctlr]->edma_inuse)) {
		return -EBUSY;
	}

	memcpy_toio(edmacc_regs_base[ctlr] + PARM_OFFSET(slot),
			&dummy_paramset, PARM_SIZE);

	return EDMA_CTLR_CHAN(ctlr, slot);
}
EXPORT_SYMBOL(edma_alloc_slot);

void edma_free_slot(unsigned slot)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(slot);
	slot = EDMA_CHAN_SLOT(slot);

	if (slot < edma_cc[ctlr]->num_channels ||
		slot >= edma_cc[ctlr]->num_slots)
		return;

	memcpy_toio(edmacc_regs_base[ctlr] + PARM_OFFSET(slot),
			&dummy_paramset, PARM_SIZE);
	clear_bit(slot, edma_cc[ctlr]->edma_inuse);
}
EXPORT_SYMBOL(edma_free_slot);


int edma_alloc_cont_slots(unsigned ctlr, unsigned int id, int slot, int count)
{
	if ((id != EDMA_CONT_PARAMS_ANY) &&
		(slot < edma_cc[ctlr]->num_channels ||
		slot >= edma_cc[ctlr]->num_slots))
		return -EINVAL;

	if (count < 1 || count >
		(edma_cc[ctlr]->num_slots - edma_cc[ctlr]->num_channels))
		return -EINVAL;

	switch (id) {
	case EDMA_CONT_PARAMS_ANY:
		return reserve_contiguous_slots(ctlr, id, count,
						 edma_cc[ctlr]->num_channels);
	case EDMA_CONT_PARAMS_FIXED_EXACT:
	case EDMA_CONT_PARAMS_FIXED_NOT_EXACT:
		return reserve_contiguous_slots(ctlr, id, count, slot);
	default:
		return -EINVAL;
	}

}
EXPORT_SYMBOL(edma_alloc_cont_slots);

int edma_free_cont_slots(unsigned slot, int count)
{
	unsigned ctlr, slot_to_free;
	int i;

	ctlr = EDMA_CTLR(slot);
	slot = EDMA_CHAN_SLOT(slot);

	if (slot < edma_cc[ctlr]->num_channels ||
		slot >= edma_cc[ctlr]->num_slots ||
		count < 1)
		return -EINVAL;

	for (i = slot; i < slot + count; ++i) {
		ctlr = EDMA_CTLR(i);
		slot_to_free = EDMA_CHAN_SLOT(i);

		memcpy_toio(edmacc_regs_base[ctlr] + PARM_OFFSET(slot_to_free),
			&dummy_paramset, PARM_SIZE);
		clear_bit(slot_to_free, edma_cc[ctlr]->edma_inuse);
	}

	return 0;
}
EXPORT_SYMBOL(edma_free_cont_slots);



void edma_set_src(unsigned slot, dma_addr_t src_port,
				enum address_mode mode, enum fifo_width width)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(slot);
	slot = EDMA_CHAN_SLOT(slot);

	if (slot < edma_cc[ctlr]->num_slots) {
		unsigned int i = edma_parm_read(ctlr, PARM_OPT, slot);

		if (mode) {
			
			i = (i & ~(EDMA_FWID)) | (SAM | ((width & 0x7) << 8));
		} else {
			
			i &= ~SAM;
		}
		edma_parm_write(ctlr, PARM_OPT, slot, i);

		edma_parm_write(ctlr, PARM_SRC, slot, src_port);
	}
}
EXPORT_SYMBOL(edma_set_src);

void edma_set_dest(unsigned slot, dma_addr_t dest_port,
				 enum address_mode mode, enum fifo_width width)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(slot);
	slot = EDMA_CHAN_SLOT(slot);

	if (slot < edma_cc[ctlr]->num_slots) {
		unsigned int i = edma_parm_read(ctlr, PARM_OPT, slot);

		if (mode) {
			
			i = (i & ~(EDMA_FWID)) | (DAM | ((width & 0x7) << 8));
		} else {
			
			i &= ~DAM;
		}
		edma_parm_write(ctlr, PARM_OPT, slot, i);
		edma_parm_write(ctlr, PARM_DST, slot, dest_port);
	}
}
EXPORT_SYMBOL(edma_set_dest);

void edma_get_position(unsigned slot, dma_addr_t *src, dma_addr_t *dst)
{
	struct edmacc_param temp;
	unsigned ctlr;

	ctlr = EDMA_CTLR(slot);
	slot = EDMA_CHAN_SLOT(slot);

	edma_read_slot(EDMA_CTLR_CHAN(ctlr, slot), &temp);
	if (src != NULL)
		*src = temp.src;
	if (dst != NULL)
		*dst = temp.dst;
}
EXPORT_SYMBOL(edma_get_position);

void edma_set_src_index(unsigned slot, s16 src_bidx, s16 src_cidx)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(slot);
	slot = EDMA_CHAN_SLOT(slot);

	if (slot < edma_cc[ctlr]->num_slots) {
		edma_parm_modify(ctlr, PARM_SRC_DST_BIDX, slot,
				0xffff0000, src_bidx);
		edma_parm_modify(ctlr, PARM_SRC_DST_CIDX, slot,
				0xffff0000, src_cidx);
	}
}
EXPORT_SYMBOL(edma_set_src_index);

void edma_set_dest_index(unsigned slot, s16 dest_bidx, s16 dest_cidx)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(slot);
	slot = EDMA_CHAN_SLOT(slot);

	if (slot < edma_cc[ctlr]->num_slots) {
		edma_parm_modify(ctlr, PARM_SRC_DST_BIDX, slot,
				0x0000ffff, dest_bidx << 16);
		edma_parm_modify(ctlr, PARM_SRC_DST_CIDX, slot,
				0x0000ffff, dest_cidx << 16);
	}
}
EXPORT_SYMBOL(edma_set_dest_index);

void edma_set_transfer_params(unsigned slot,
		u16 acnt, u16 bcnt, u16 ccnt,
		u16 bcnt_rld, enum sync_dimension sync_mode)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(slot);
	slot = EDMA_CHAN_SLOT(slot);

	if (slot < edma_cc[ctlr]->num_slots) {
		edma_parm_modify(ctlr, PARM_LINK_BCNTRLD, slot,
				0x0000ffff, bcnt_rld << 16);
		if (sync_mode == ASYNC)
			edma_parm_and(ctlr, PARM_OPT, slot, ~SYNCDIM);
		else
			edma_parm_or(ctlr, PARM_OPT, slot, SYNCDIM);
		
		edma_parm_write(ctlr, PARM_A_B_CNT, slot, (bcnt << 16) | acnt);
		edma_parm_write(ctlr, PARM_CCNT, slot, ccnt);
	}
}
EXPORT_SYMBOL(edma_set_transfer_params);

void edma_link(unsigned from, unsigned to)
{
	unsigned ctlr_from, ctlr_to;

	ctlr_from = EDMA_CTLR(from);
	from = EDMA_CHAN_SLOT(from);
	ctlr_to = EDMA_CTLR(to);
	to = EDMA_CHAN_SLOT(to);

	if (from >= edma_cc[ctlr_from]->num_slots)
		return;
	if (to >= edma_cc[ctlr_to]->num_slots)
		return;
	edma_parm_modify(ctlr_from, PARM_LINK_BCNTRLD, from, 0xffff0000,
				PARM_OFFSET(to));
}
EXPORT_SYMBOL(edma_link);

void edma_unlink(unsigned from)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(from);
	from = EDMA_CHAN_SLOT(from);

	if (from >= edma_cc[ctlr]->num_slots)
		return;
	edma_parm_or(ctlr, PARM_LINK_BCNTRLD, from, 0xffff);
}
EXPORT_SYMBOL(edma_unlink);



/**
 * edma_write_slot - write parameter RAM data for slot
 * @slot: number of parameter RAM slot being modified
 * @param: data to be written into parameter RAM slot
 *
 * Use this to assign all parameters of a transfer at once.  This
 * allows more efficient setup of transfers than issuing multiple
 * calls to set up those parameters in small pieces, and provides
 * complete control over all transfer options.
 */
void edma_write_slot(unsigned slot, const struct edmacc_param *param)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(slot);
	slot = EDMA_CHAN_SLOT(slot);

	if (slot >= edma_cc[ctlr]->num_slots)
		return;
	memcpy_toio(edmacc_regs_base[ctlr] + PARM_OFFSET(slot), param,
			PARM_SIZE);
}
EXPORT_SYMBOL(edma_write_slot);

void edma_read_slot(unsigned slot, struct edmacc_param *param)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(slot);
	slot = EDMA_CHAN_SLOT(slot);

	if (slot >= edma_cc[ctlr]->num_slots)
		return;
	memcpy_fromio(param, edmacc_regs_base[ctlr] + PARM_OFFSET(slot),
			PARM_SIZE);
}
EXPORT_SYMBOL(edma_read_slot);



void edma_pause(unsigned channel)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(channel);
	channel = EDMA_CHAN_SLOT(channel);

	if (channel < edma_cc[ctlr]->num_channels) {
		unsigned int mask = BIT(channel & 0x1f);

		edma_shadow0_write_array(ctlr, SH_EECR, channel >> 5, mask);
	}
}
EXPORT_SYMBOL(edma_pause);

void edma_resume(unsigned channel)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(channel);
	channel = EDMA_CHAN_SLOT(channel);

	if (channel < edma_cc[ctlr]->num_channels) {
		unsigned int mask = BIT(channel & 0x1f);

		edma_shadow0_write_array(ctlr, SH_EESR, channel >> 5, mask);
	}
}
EXPORT_SYMBOL(edma_resume);

int edma_start(unsigned channel)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(channel);
	channel = EDMA_CHAN_SLOT(channel);

	if (channel < edma_cc[ctlr]->num_channels) {
		int j = channel >> 5;
		unsigned int mask = BIT(channel & 0x1f);

		
		if (test_bit(channel, edma_cc[ctlr]->edma_unused)) {
			pr_debug("EDMA: ESR%d %08x\n", j,
				edma_shadow0_read_array(ctlr, SH_ESR, j));
			edma_shadow0_write_array(ctlr, SH_ESR, j, mask);
			return 0;
		}

		
		pr_debug("EDMA: ER%d %08x\n", j,
			edma_shadow0_read_array(ctlr, SH_ER, j));
		
		edma_write_array(ctlr, EDMA_ECR, j, mask);
		edma_write_array(ctlr, EDMA_EMCR, j, mask);
		
		edma_shadow0_write_array(ctlr, SH_SECR, j, mask);
		edma_shadow0_write_array(ctlr, SH_EESR, j, mask);
		pr_debug("EDMA: EER%d %08x\n", j,
			edma_shadow0_read_array(ctlr, SH_EER, j));
		return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(edma_start);

void edma_stop(unsigned channel)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(channel);
	channel = EDMA_CHAN_SLOT(channel);

	if (channel < edma_cc[ctlr]->num_channels) {
		int j = channel >> 5;
		unsigned int mask = BIT(channel & 0x1f);

		edma_shadow0_write_array(ctlr, SH_EECR, j, mask);
		edma_shadow0_write_array(ctlr, SH_ECR, j, mask);
		edma_shadow0_write_array(ctlr, SH_SECR, j, mask);
		edma_write_array(ctlr, EDMA_EMCR, j, mask);

		pr_debug("EDMA: EER%d %08x\n", j,
				edma_shadow0_read_array(ctlr, SH_EER, j));

	}
}
EXPORT_SYMBOL(edma_stop);


void edma_clean_channel(unsigned channel)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(channel);
	channel = EDMA_CHAN_SLOT(channel);

	if (channel < edma_cc[ctlr]->num_channels) {
		int j = (channel >> 5);
		unsigned int mask = BIT(channel & 0x1f);

		pr_debug("EDMA: EMR%d %08x\n", j,
				edma_read_array(ctlr, EDMA_EMR, j));
		edma_shadow0_write_array(ctlr, SH_ECR, j, mask);
		
		edma_write_array(ctlr, EDMA_EMCR, j, mask);
		
		edma_shadow0_write_array(ctlr, SH_SECR, j, mask);
		edma_write(ctlr, EDMA_CCERRCLR, BIT(16) | BIT(1) | BIT(0));
	}
}
EXPORT_SYMBOL(edma_clean_channel);

void edma_clear_event(unsigned channel)
{
	unsigned ctlr;

	ctlr = EDMA_CTLR(channel);
	channel = EDMA_CHAN_SLOT(channel);

	if (channel >= edma_cc[ctlr]->num_channels)
		return;
	if (channel < 32)
		edma_write(ctlr, EDMA_ECR, BIT(channel));
	else
		edma_write(ctlr, EDMA_ECRH, BIT(channel - 32));
}
EXPORT_SYMBOL(edma_clear_event);


static int __init edma_probe(struct platform_device *pdev)
{
	struct edma_soc_info	**info = pdev->dev.platform_data;
	const s8		(*queue_priority_mapping)[2];
	const s8		(*queue_tc_mapping)[2];
	int			i, j, off, ln, found = 0;
	int			status = -1;
	const s16		(*rsv_chans)[2];
	const s16		(*rsv_slots)[2];
	int			irq[EDMA_MAX_CC] = {0, 0};
	int			err_irq[EDMA_MAX_CC] = {0, 0};
	struct resource		*r[EDMA_MAX_CC] = {NULL};
	resource_size_t		len[EDMA_MAX_CC];
	char			res_name[10];
	char			irq_name[10];

	if (!info)
		return -ENODEV;

	for (j = 0; j < EDMA_MAX_CC; j++) {
		sprintf(res_name, "edma_cc%d", j);
		r[j] = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						res_name);
		if (!r[j] || !info[j]) {
			if (found)
				break;
			else
				return -ENODEV;
		} else {
			found = 1;
		}

		len[j] = resource_size(r[j]);

		r[j] = request_mem_region(r[j]->start, len[j],
			dev_name(&pdev->dev));
		if (!r[j]) {
			status = -EBUSY;
			goto fail1;
		}

		edmacc_regs_base[j] = ioremap(r[j]->start, len[j]);
		if (!edmacc_regs_base[j]) {
			status = -EBUSY;
			goto fail1;
		}

		edma_cc[j] = kzalloc(sizeof(struct edma), GFP_KERNEL);
		if (!edma_cc[j]) {
			status = -ENOMEM;
			goto fail1;
		}

		edma_cc[j]->num_channels = min_t(unsigned, info[j]->n_channel,
							EDMA_MAX_DMACH);
		edma_cc[j]->num_slots = min_t(unsigned, info[j]->n_slot,
							EDMA_MAX_PARAMENTRY);
		edma_cc[j]->num_cc = min_t(unsigned, info[j]->n_cc,
							EDMA_MAX_CC);

		edma_cc[j]->default_queue = info[j]->default_queue;

		dev_dbg(&pdev->dev, "DMA REG BASE ADDR=%p\n",
			edmacc_regs_base[j]);

		for (i = 0; i < edma_cc[j]->num_slots; i++)
			memcpy_toio(edmacc_regs_base[j] + PARM_OFFSET(i),
					&dummy_paramset, PARM_SIZE);

		
		memset(edma_cc[j]->edma_unused, 0xff,
			sizeof(edma_cc[j]->edma_unused));

		if (info[j]->rsv) {

			
			rsv_chans = info[j]->rsv->rsv_chans;
			if (rsv_chans) {
				for (i = 0; rsv_chans[i][0] != -1; i++) {
					off = rsv_chans[i][0];
					ln = rsv_chans[i][1];
					clear_bits(off, ln,
						edma_cc[j]->edma_unused);
				}
			}

			
			rsv_slots = info[j]->rsv->rsv_slots;
			if (rsv_slots) {
				for (i = 0; rsv_slots[i][0] != -1; i++) {
					off = rsv_slots[i][0];
					ln = rsv_slots[i][1];
					set_bits(off, ln,
						edma_cc[j]->edma_inuse);
				}
			}
		}

		sprintf(irq_name, "edma%d", j);
		irq[j] = platform_get_irq_byname(pdev, irq_name);
		edma_cc[j]->irq_res_start = irq[j];
		status = request_irq(irq[j], dma_irq_handler, 0, "edma",
					&pdev->dev);
		if (status < 0) {
			dev_dbg(&pdev->dev, "request_irq %d failed --> %d\n",
				irq[j], status);
			goto fail;
		}

		sprintf(irq_name, "edma%d_err", j);
		err_irq[j] = platform_get_irq_byname(pdev, irq_name);
		edma_cc[j]->irq_res_end = err_irq[j];
		status = request_irq(err_irq[j], dma_ccerr_handler, 0,
					"edma_error", &pdev->dev);
		if (status < 0) {
			dev_dbg(&pdev->dev, "request_irq %d failed --> %d\n",
				err_irq[j], status);
			goto fail;
		}

		for (i = 0; i < edma_cc[j]->num_channels; i++)
			map_dmach_queue(j, i, info[j]->default_queue);

		queue_tc_mapping = info[j]->queue_tc_mapping;
		queue_priority_mapping = info[j]->queue_priority_mapping;

		
		for (i = 0; queue_tc_mapping[i][0] != -1; i++)
			map_queue_tc(j, queue_tc_mapping[i][0],
					queue_tc_mapping[i][1]);

		
		for (i = 0; queue_priority_mapping[i][0] != -1; i++)
			assign_priority_to_queue(j,
						queue_priority_mapping[i][0],
						queue_priority_mapping[i][1]);

		if (edma_read(j, EDMA_CCCFG) & CHMAP_EXIST)
			map_dmach_param(j);

		for (i = 0; i < info[j]->n_region; i++) {
			edma_write_array2(j, EDMA_DRAE, i, 0, 0x0);
			edma_write_array2(j, EDMA_DRAE, i, 1, 0x0);
			edma_write_array(j, EDMA_QRAE, i, 0x0);
		}
		arch_num_cc++;
	}

	if (tc_errs_handled) {
		status = request_irq(IRQ_TCERRINT0, dma_tc0err_handler, 0,
					"edma_tc0", &pdev->dev);
		if (status < 0) {
			dev_dbg(&pdev->dev, "request_irq %d failed --> %d\n",
				IRQ_TCERRINT0, status);
			return status;
		}
		status = request_irq(IRQ_TCERRINT, dma_tc1err_handler, 0,
					"edma_tc1", &pdev->dev);
		if (status < 0) {
			dev_dbg(&pdev->dev, "request_irq %d --> %d\n",
				IRQ_TCERRINT, status);
			return status;
		}
	}

	return 0;

fail:
	for (i = 0; i < EDMA_MAX_CC; i++) {
		if (err_irq[i])
			free_irq(err_irq[i], &pdev->dev);
		if (irq[i])
			free_irq(irq[i], &pdev->dev);
	}
fail1:
	for (i = 0; i < EDMA_MAX_CC; i++) {
		if (r[i])
			release_mem_region(r[i]->start, len[i]);
		if (edmacc_regs_base[i])
			iounmap(edmacc_regs_base[i]);
		kfree(edma_cc[i]);
	}
	return status;
}


static struct platform_driver edma_driver = {
	.driver.name	= "edma",
};

static int __init edma_init(void)
{
	return platform_driver_probe(&edma_driver, edma_probe);
}
arch_initcall(edma_init);

