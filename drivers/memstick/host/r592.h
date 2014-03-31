/*
 * Copyright (C) 2010 - Maxim Levitsky
 * driver for Ricoh memstick readers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef R592_H

#include <linux/memstick.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kfifo.h>
#include <linux/ctype.h>

#define R592_TPC_EXEC			0x00
#define R592_TPC_EXEC_LEN_SHIFT		16		
#define R592_TPC_EXEC_BIG_FIFO		(1 << 26)	
#define R592_TPC_EXEC_TPC_SHIFT		28		


#define R592_SFIFO			0x08


#define R592_STATUS			0x10
							
#define R592_STATUS_P_CMDNACK		(1 << 16)	
#define R592_STATUS_P_BREQ		(1 << 17)	
#define R592_STATUS_P_INTERR		(1 << 18)	
#define R592_STATUS_P_CED		(1 << 19)	

							
#define R592_STATUS_SFIFO_FULL		(1 << 20)	/* Small Fifo almost full (last chunk is written) */
#define R592_STATUS_SFIFO_EMPTY		(1 << 21)	

							
#define R592_STATUS_SEND_ERR		(1 << 24)	
#define R592_STATUS_RECV_ERR		(1 << 25)	

							
#define R592_STATUS_RDY			(1 << 28)	
#define R592_STATUS_CED			(1 << 29)	
#define R592_STATUS_SFIFO_INPUT		(1 << 30)	

#define R592_SFIFO_SIZE			32		
#define R592_SFIFO_PACKET		8		

#define R592_IO				0x18
#define	R592_IO_16			(1 << 16)	
#define	R592_IO_18			(1 << 18)	
#define	R592_IO_SERIAL1			(1 << 20)	
#define	R592_IO_22			(1 << 22)	
#define R592_IO_DIRECTION		(1 << 24)	
#define	R592_IO_26			(1 << 26)	
#define	R592_IO_SERIAL2			(1 << 30)	
#define R592_IO_RESET			(1 << 31)	


#define R592_POWER			0x20		
#define R592_POWER_0			(1 << 0)	
#define R592_POWER_1			(1 << 1)	
#define R592_POWER_3			(1 << 3)	
#define R592_POWER_20			(1 << 5)	

#define R592_IO_MODE			0x24
#define R592_IO_MODE_SERIAL		1
#define R592_IO_MODE_PARALLEL		3


#define R592_REG_MSC			0x28
#define R592_REG_MSC_PRSNT		(1 << 1)	
#define R592_REG_MSC_IRQ_INSERT		(1 << 8)	
#define R592_REG_MSC_IRQ_REMOVE		(1 << 9)	
#define R592_REG_MSC_FIFO_EMPTY		(1 << 10)	
#define R592_REG_MSC_FIFO_DMA_DONE	(1 << 11)	

#define R592_REG_MSC_FIFO_USER_ORN	(1 << 12)	
#define R592_REG_MSC_FIFO_MISMATH	(1 << 13)	
#define R592_REG_MSC_FIFO_DMA_ERR	(1 << 14)	
#define R592_REG_MSC_LED		(1 << 15)	

#define DMA_IRQ_ACK_MASK \
	(R592_REG_MSC_FIFO_DMA_DONE | R592_REG_MSC_FIFO_DMA_ERR)

#define DMA_IRQ_EN_MASK (DMA_IRQ_ACK_MASK << 16)

#define IRQ_ALL_ACK_MASK 0x00007F00
#define IRQ_ALL_EN_MASK (IRQ_ALL_ACK_MASK << 16)

#define R592_FIFO_DMA			0x2C

#define R592_FIFO_PIO			0x30
#define R592_LFIFO_SIZE			512		


#define R592_FIFO_DMA_SETTINGS		0x34
#define R592_FIFO_DMA_SETTINGS_EN	(1 << 0)	
#define R592_FIFO_DMA_SETTINGS_DIR	(1 << 1)	
#define R592_FIFO_DMA_SETTINGS_CAP	(1 << 24)	

#define R592_REG38			0x38
#define R592_REG38_CHANGE		(1 << 16)	
#define R592_REG38_DONE			(1 << 20)	
#define R592_REG38_SHIFT		17

/* Debug register, written (0xABCDEF00) when error happens - not used*/
#define R592_REG_3C			0x3C

struct r592_device {
	struct pci_dev *pci_dev;
	struct memstick_host	*host;		
	struct memstick_request *req;		

	
	void __iomem *mmio;
	int irq;
	spinlock_t irq_lock;
	spinlock_t io_thread_lock;
	struct timer_list detect_timer;

	struct task_struct *io_thread;
	bool parallel_mode;

	DECLARE_KFIFO(pio_fifo, u8, sizeof(u32));

	
	int dma_capable;
	int dma_error;
	struct completion dma_done;
	void *dummy_dma_page;
	dma_addr_t dummy_dma_page_physical_address;

};

#define DRV_NAME "r592"


#define message(format, ...) \
	printk(KERN_INFO DRV_NAME ": " format "\n", ## __VA_ARGS__)

#define __dbg(level, format, ...) \
	do { \
		if (debug >= level) \
			printk(KERN_DEBUG DRV_NAME \
				": " format "\n", ## __VA_ARGS__); \
	} while (0)


#define dbg(format, ...)		__dbg(1, format, ## __VA_ARGS__)
#define dbg_verbose(format, ...)	__dbg(2, format, ## __VA_ARGS__)
#define dbg_reg(format, ...)		__dbg(3, format, ## __VA_ARGS__)

#endif
