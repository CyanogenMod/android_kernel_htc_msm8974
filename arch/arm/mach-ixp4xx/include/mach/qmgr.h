/*
 * Copyright (C) 2007 Krzysztof Halasa <khc@pm.waw.pl>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */

#ifndef IXP4XX_QMGR_H
#define IXP4XX_QMGR_H

#include <linux/io.h>
#include <linux/kernel.h>

#define DEBUG_QMGR	0

#define HALF_QUEUES	32
#define QUEUES		64
#define MAX_QUEUE_LENGTH 4	

#define QUEUE_STAT1_EMPTY		1 
#define QUEUE_STAT1_NEARLY_EMPTY	2
#define QUEUE_STAT1_NEARLY_FULL		4
#define QUEUE_STAT1_FULL		8
#define QUEUE_STAT2_UNDERFLOW		1
#define QUEUE_STAT2_OVERFLOW		2

#define QUEUE_WATERMARK_0_ENTRIES	0
#define QUEUE_WATERMARK_1_ENTRY		1
#define QUEUE_WATERMARK_2_ENTRIES	2
#define QUEUE_WATERMARK_4_ENTRIES	3
#define QUEUE_WATERMARK_8_ENTRIES	4
#define QUEUE_WATERMARK_16_ENTRIES	5
#define QUEUE_WATERMARK_32_ENTRIES	6
#define QUEUE_WATERMARK_64_ENTRIES	7

#define QUEUE_IRQ_SRC_EMPTY		0
#define QUEUE_IRQ_SRC_NEARLY_EMPTY	1
#define QUEUE_IRQ_SRC_NEARLY_FULL	2
#define QUEUE_IRQ_SRC_FULL		3
#define QUEUE_IRQ_SRC_NOT_EMPTY		4
#define QUEUE_IRQ_SRC_NOT_NEARLY_EMPTY	5
#define QUEUE_IRQ_SRC_NOT_NEARLY_FULL	6
#define QUEUE_IRQ_SRC_NOT_FULL		7

struct qmgr_regs {
	u32 acc[QUEUES][MAX_QUEUE_LENGTH]; 
	u32 stat1[4];		
	u32 stat2[2];		
	u32 statne_h;		
	u32 statf_h;		
	u32 irqsrc[4];		
	u32 irqen[2];		
	u32 irqstat[2];		
	u32 reserved[1776];
	u32 sram[2048];		
};

void qmgr_set_irq(unsigned int queue, int src,
		  void (*handler)(void *pdev), void *pdev);
void qmgr_enable_irq(unsigned int queue);
void qmgr_disable_irq(unsigned int queue);


#if DEBUG_QMGR
extern char qmgr_queue_descs[QUEUES][32];

int qmgr_request_queue(unsigned int queue, unsigned int len ,
		       unsigned int nearly_empty_watermark,
		       unsigned int nearly_full_watermark,
		       const char *desc_format, const char* name);
#else
int __qmgr_request_queue(unsigned int queue, unsigned int len ,
			 unsigned int nearly_empty_watermark,
			 unsigned int nearly_full_watermark);
#define qmgr_request_queue(queue, len, nearly_empty_watermark,		\
			   nearly_full_watermark, desc_format, name)	\
	__qmgr_request_queue(queue, len, nearly_empty_watermark,	\
			     nearly_full_watermark)
#endif

void qmgr_release_queue(unsigned int queue);


static inline void qmgr_put_entry(unsigned int queue, u32 val)
{
	extern struct qmgr_regs __iomem *qmgr_regs;
#if DEBUG_QMGR
	BUG_ON(!qmgr_queue_descs[queue]); 

	printk(KERN_DEBUG "Queue %s(%i) put %X\n",
	       qmgr_queue_descs[queue], queue, val);
#endif
	__raw_writel(val, &qmgr_regs->acc[queue][0]);
}

static inline u32 qmgr_get_entry(unsigned int queue)
{
	u32 val;
	extern struct qmgr_regs __iomem *qmgr_regs;
	val = __raw_readl(&qmgr_regs->acc[queue][0]);
#if DEBUG_QMGR
	BUG_ON(!qmgr_queue_descs[queue]); 

	printk(KERN_DEBUG "Queue %s(%i) get %X\n",
	       qmgr_queue_descs[queue], queue, val);
#endif
	return val;
}

static inline int __qmgr_get_stat1(unsigned int queue)
{
	extern struct qmgr_regs __iomem *qmgr_regs;
	return (__raw_readl(&qmgr_regs->stat1[queue >> 3])
		>> ((queue & 7) << 2)) & 0xF;
}

static inline int __qmgr_get_stat2(unsigned int queue)
{
	extern struct qmgr_regs __iomem *qmgr_regs;
	BUG_ON(queue >= HALF_QUEUES);
	return (__raw_readl(&qmgr_regs->stat2[queue >> 4])
		>> ((queue & 0xF) << 1)) & 0x3;
}

static inline int qmgr_stat_empty(unsigned int queue)
{
	BUG_ON(queue >= HALF_QUEUES);
	return __qmgr_get_stat1(queue) & QUEUE_STAT1_EMPTY;
}

static inline int qmgr_stat_below_low_watermark(unsigned int queue)
{
	extern struct qmgr_regs __iomem *qmgr_regs;
	if (queue >= HALF_QUEUES)
		return (__raw_readl(&qmgr_regs->statne_h) >>
			(queue - HALF_QUEUES)) & 0x01;
	return __qmgr_get_stat1(queue) & QUEUE_STAT1_NEARLY_EMPTY;
}

static inline int qmgr_stat_above_high_watermark(unsigned int queue)
{
	BUG_ON(queue >= HALF_QUEUES);
	return __qmgr_get_stat1(queue) & QUEUE_STAT1_NEARLY_FULL;
}

static inline int qmgr_stat_full(unsigned int queue)
{
	extern struct qmgr_regs __iomem *qmgr_regs;
	if (queue >= HALF_QUEUES)
		return (__raw_readl(&qmgr_regs->statf_h) >>
			(queue - HALF_QUEUES)) & 0x01;
	return __qmgr_get_stat1(queue) & QUEUE_STAT1_FULL;
}

static inline int qmgr_stat_underflow(unsigned int queue)
{
	return __qmgr_get_stat2(queue) & QUEUE_STAT2_UNDERFLOW;
}

static inline int qmgr_stat_overflow(unsigned int queue)
{
	return __qmgr_get_stat2(queue) & QUEUE_STAT2_OVERFLOW;
}

#endif
