/*
 * Copyright © 2009 - Maxim Levitsky
 * driver for Ricoh xD readers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pci.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/mtd/nand.h>
#include <linux/spinlock.h>


#define	R852_DATALINE		0x00

#define R852_CTL		0x04
#define R852_CTL_COMMAND 	0x01	
#define R852_CTL_DATA		0x02	
#define R852_CTL_ON		0x04	
					
#define R852_CTL_RESET		0x08	
#define R852_CTL_CARDENABLE	0x10	
#define R852_CTL_ECC_ENABLE	0x20	
#define R852_CTL_ECC_ACCESS	0x40	
#define R852_CTL_WRITE		0x80	

#define R852_CARD_STA		0x05

#define R852_CARD_STA_CD	0x01	
#define R852_CARD_STA_RO	0x02	
#define R852_CARD_STA_PRESENT	0x04	
#define R852_CARD_STA_ABSENT	0x08	
#define R852_CARD_STA_BUSY	0x80	

#define R852_CARD_IRQ_STA	0x06	
#define R852_CARD_IRQ_ENABLE	0x07	

#define R852_CARD_IRQ_CD	0x01	
#define R852_CARD_IRQ_REMOVE	0x04	
#define R852_CARD_IRQ_INSERT	0x08	
#define R852_CARD_IRQ_UNK1	0x10	
#define R852_CARD_IRQ_GENABLE	0x80	
#define R852_CARD_IRQ_MASK	0x1D



#define R852_HW			0x08
#define R852_HW_ENABLED		0x01	
#define R852_HW_UNKNOWN		0x80


#define R852_DMA_CAP		0x09
#define R852_SMBIT		0x20	
					
#define R852_DMA1		0x40	
#define R852_DMA2		0x80	


#define R852_DMA_ADDR		0x0C


#define R852_DMA_SETTINGS	0x10
#define R852_DMA_MEMORY		0x01	
#define R852_DMA_READ		0x02	
#define R852_DMA_INTERNAL	0x04	

#define R852_DMA_IRQ_STA		0x14

#define R852_DMA_IRQ_ENABLE	0x18

#define R852_DMA_IRQ_MEMORY	0x01	
#define R852_DMA_IRQ_ERROR	0x02	
#define R852_DMA_IRQ_INTERNAL	0x04	
#define R852_DMA_IRQ_MASK	0x07	


#define R852_ECC_ERR_BIT_MSK	0x07	
#define R852_ECC_CORRECT		0x10	
#define R852_ECC_CORRECTABLE	0x20	
#define R852_ECC_FAIL		0x40	

#define R852_DMA_LEN		512

#define DMA_INTERNAL	0
#define DMA_MEMORY	1

struct r852_device {
	void __iomem *mmio;		
	struct mtd_info *mtd;		
	struct nand_chip *chip;		
	struct pci_dev *pci_dev;	

	
	dma_addr_t phys_dma_addr;	
	struct completion dma_done;	

	dma_addr_t phys_bounce_buffer;	
	uint8_t *bounce_buffer;		

	int dma_dir;			
	int dma_stage;			

	int dma_state;			
	int dma_error;			
	int dma_usable;			

	
	struct delayed_work card_detect_work;
	struct workqueue_struct *card_workqueue;
	int card_registred;		
	int card_detected;		
	int card_unstable;		
	int readonly;			
	int sm;				

	
	spinlock_t irqlock;		
	int irq;			
	
	void *tmp_buffer;		
	uint8_t ctlreg;			
};

#define DRV_NAME "r852"


#define dbg(format, ...) \
	if (debug) \
		printk(KERN_DEBUG DRV_NAME ": " format "\n", ## __VA_ARGS__)

#define dbg_verbose(format, ...) \
	if (debug > 1) \
		printk(KERN_DEBUG DRV_NAME ": " format "\n", ## __VA_ARGS__)


#define message(format, ...) \
	printk(KERN_INFO DRV_NAME ": " format "\n", ## __VA_ARGS__)
