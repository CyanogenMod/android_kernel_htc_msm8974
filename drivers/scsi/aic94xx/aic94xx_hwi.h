/*
 * Aic94xx SAS/SATA driver hardware interface header file.
 *
 * Copyright (C) 2005 Adaptec, Inc.  All rights reserved.
 * Copyright (C) 2005 Luben Tuikov <luben_tuikov@adaptec.com>
 *
 * This file is licensed under GPLv2.
 *
 * This file is part of the aic94xx driver.
 *
 * The aic94xx driver is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * The aic94xx driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the aic94xx driver; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _AIC94XX_HWI_H_
#define _AIC94XX_HWI_H_

#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>

#include <scsi/libsas.h>

#include "aic94xx.h"
#include "aic94xx_sas.h"

#define ASD_MAX_PHYS       8
#define ASD_PCBA_SN_SIZE   12

struct asd_ha_addrspace {
	void __iomem  *addr;
	unsigned long  start;       
	unsigned long  len;         
	unsigned long  flags;       

	
	u32 swa_base; 
	u32 swb_base;
	u32 swc_base;
};

struct bios_struct {
	int    present;
	u8     maj;
	u8     min;
	u32    bld;
};

struct unit_element_struct {
	u16    num;
	u16    size;
	void   *area;
};

struct flash_struct {
	u32    bar;
	int    present;
	int    wide;
	u8     manuf;
	u8     dev_id;
	u8     sec_prot;
	u8     method;

	u32    dir_offs;
};

struct asd_phy_desc {
	
	u8     sas_addr[SAS_ADDR_SIZE];
	u8     max_sas_lrate;
	u8     min_sas_lrate;
	u8     max_sata_lrate;
	u8     min_sata_lrate;
	u8     flags;
#define ASD_CRC_DIS  1
#define ASD_SATA_SPINUP_HOLD 2

	u8     phy_control_0; 
	u8     phy_control_1; 
	u8     phy_control_2; 
	u8     phy_control_3; 
};

struct asd_dma_tok {
	void *vaddr;
	dma_addr_t dma_handle;
	size_t size;
};

struct hw_profile {
	struct bios_struct bios;
	struct unit_element_struct ue;
	struct flash_struct flash;

	u8     sas_addr[SAS_ADDR_SIZE];
	char   pcba_sn[ASD_PCBA_SN_SIZE+1];

	u8     enabled_phys;	  
	struct asd_phy_desc phy_desc[ASD_MAX_PHYS];
	u32    max_scbs;	  
	struct asd_dma_tok *scb_ext;
	u32    max_ddbs;
	struct asd_dma_tok *ddb_ext;

	spinlock_t ddb_lock;
	void  *ddb_bitmap;

	int    num_phys;	  
	int    max_phys;	  

	unsigned addr_range;	  
	unsigned port_name_base;
	unsigned dev_name_base;
	unsigned sata_name_base;
};

struct asd_ascb {
	struct list_head list;
	struct asd_ha_struct *ha;

	struct scb *scb;	  
	struct asd_dma_tok dma_scb;
	struct asd_dma_tok *sg_arr;

	void (*tasklet_complete)(struct asd_ascb *, struct done_list_struct *);
	u8     uldd_timer:1;

	
	struct timer_list timer;
	struct completion *completion;
	u8        tag_valid:1;
	__be16    tag;		  

	
	int    edb_index;

	
	int    tc_index;

	void   *uldd_task;
};

#define ASD_DL_SIZE_BITS   0x8
#define ASD_DL_SIZE        (1<<(2+ASD_DL_SIZE_BITS))
#define ASD_DEF_DL_TOGGLE  0x01

struct asd_seq_data {
	spinlock_t pend_q_lock;
	u16    scbpro;
	int    pending;
	struct list_head pend_q;
	int    can_queue;	  
	struct asd_dma_tok next_scb; 

	spinlock_t tc_index_lock;
	void **tc_index_array;
	void *tc_index_bitmap;
	int   tc_index_bitmap_bits;

	struct tasklet_struct dl_tasklet;
	struct done_list_struct *dl; 
	struct asd_dma_tok *actual_dl; 
	int    dl_toggle;
	int    dl_next;

	int    num_edbs;
	struct asd_dma_tok **edb_arr;
	int    num_escbs;
	struct asd_ascb **escb_arr; 
};

struct asd_port {
	u8  sas_addr[SAS_ADDR_SIZE];
	u8  attached_sas_addr[SAS_ADDR_SIZE];
	u32 phy_mask;
	int num_phys;
};

struct asd_ha_struct {
	struct pci_dev   *pcidev;
	const char       *name;

	struct sas_ha_struct sas_ha;

	u8                revision_id;

	int               iospace;
	spinlock_t        iolock;
	struct asd_ha_addrspace io_handle[2];

	struct hw_profile hw_prof;

	struct asd_phy    phys[ASD_MAX_PHYS];
	spinlock_t        asd_ports_lock;
	struct asd_port   asd_ports[ASD_MAX_PHYS];
	struct asd_sas_port   ports[ASD_MAX_PHYS];

	struct dma_pool  *scb_pool;

	struct asd_seq_data  seq; 
	u32    bios_status;
	const struct firmware *bios_image;
};


#define ASD_BUSADDR_LO(__dma_handle) ((u32)(__dma_handle))
#define ASD_BUSADDR_HI(__dma_handle) (((sizeof(dma_addr_t))==8)     \
                                    ? ((u32)((__dma_handle) >> 32)) \
                                    : ((u32)0))

#define dev_to_asd_ha(__dev)  pci_get_drvdata(to_pci_dev(__dev))
#define SCB_SITE_VALID(__site_no) (((__site_no) & 0xF0FF) != 0x00FF   \
				 && ((__site_no) & 0xF0FF) > 0x001F)
#define for_each_sequencer(__lseq_mask, __mc, __lseq)                        \
	for ((__mc)=(__lseq_mask),(__lseq)=0;(__mc)!=0;(__lseq++),(__mc)>>=1)\
		if (((__mc) & 1))
#define for_each_phy(__lseq_mask, __mc, __lseq)                              \
	for ((__mc)=(__lseq_mask),(__lseq)=0;(__mc)!=0;(__lseq++),(__mc)>>=1)\
		if (((__mc) & 1))

#define PHY_ENABLED(_HA, _I) ((_HA)->hw_prof.enabled_phys & (1<<(_I)))


static inline struct asd_dma_tok *asd_dmatok_alloc(gfp_t flags)
{
	return kmem_cache_alloc(asd_dma_token_cache, flags);
}

static inline void asd_dmatok_free(struct asd_dma_tok *token)
{
	kmem_cache_free(asd_dma_token_cache, token);
}

static inline struct asd_dma_tok *asd_alloc_coherent(struct asd_ha_struct *
						     asd_ha, size_t size,
						     gfp_t flags)
{
	struct asd_dma_tok *token = asd_dmatok_alloc(flags);
	if (token) {
		token->size = size;
		token->vaddr = dma_alloc_coherent(&asd_ha->pcidev->dev,
						  token->size,
						  &token->dma_handle,
						  flags);
		if (!token->vaddr) {
			asd_dmatok_free(token);
			token = NULL;
		}
	}
	return token;
}

static inline void asd_free_coherent(struct asd_ha_struct *asd_ha,
				     struct asd_dma_tok *token)
{
	if (token) {
		dma_free_coherent(&asd_ha->pcidev->dev, token->size,
				  token->vaddr, token->dma_handle);
		asd_dmatok_free(token);
	}
}

static inline void asd_init_ascb(struct asd_ha_struct *asd_ha,
				 struct asd_ascb *ascb)
{
	INIT_LIST_HEAD(&ascb->list);
	ascb->scb = ascb->dma_scb.vaddr;
	ascb->ha = asd_ha;
	ascb->timer.function = NULL;
	init_timer(&ascb->timer);
	ascb->tc_index = -1;
}

static inline void asd_tc_index_release(struct asd_seq_data *seq, int index)
{
	seq->tc_index_array[index] = NULL;
	clear_bit(index, seq->tc_index_bitmap);
}

static inline int asd_tc_index_get(struct asd_seq_data *seq, void *ptr)
{
	int index;

	index = find_first_zero_bit(seq->tc_index_bitmap,
				    seq->tc_index_bitmap_bits);
	if (index == seq->tc_index_bitmap_bits)
		return -1;

	seq->tc_index_array[index] = ptr;
	set_bit(index, seq->tc_index_bitmap);

	return index;
}

static inline void *asd_tc_index_find(struct asd_seq_data *seq, int index)
{
	return seq->tc_index_array[index];
}

static inline void asd_ascb_free(struct asd_ascb *ascb)
{
	if (ascb) {
		struct asd_ha_struct *asd_ha = ascb->ha;
		unsigned long flags;

		BUG_ON(!list_empty(&ascb->list));
		spin_lock_irqsave(&ascb->ha->seq.tc_index_lock, flags);
		asd_tc_index_release(&ascb->ha->seq, ascb->tc_index);
		spin_unlock_irqrestore(&ascb->ha->seq.tc_index_lock, flags);
		dma_pool_free(asd_ha->scb_pool, ascb->dma_scb.vaddr,
			      ascb->dma_scb.dma_handle);
		kmem_cache_free(asd_ascb_cache, ascb);
	}
}

static inline void asd_ascb_free_list(struct asd_ascb *ascb_list)
{
	LIST_HEAD(list);
	struct list_head *n, *pos;

	__list_add(&list, ascb_list->list.prev, &ascb_list->list);
	list_for_each_safe(pos, n, &list) {
		list_del_init(pos);
		asd_ascb_free(list_entry(pos, struct asd_ascb, list));
	}
}


int  asd_init_hw(struct asd_ha_struct *asd_ha);
irqreturn_t asd_hw_isr(int irq, void *dev_id);


struct asd_ascb *asd_ascb_alloc_list(struct asd_ha_struct
				     *asd_ha, int *num,
				     gfp_t gfp_mask);

int  asd_post_ascb_list(struct asd_ha_struct *asd_ha, struct asd_ascb *ascb,
			int num);
int  asd_post_escb_list(struct asd_ha_struct *asd_ha, struct asd_ascb *ascb,
			int num);

int  asd_init_post_escbs(struct asd_ha_struct *asd_ha);
void asd_build_control_phy(struct asd_ascb *ascb, int phy_id, u8 subfunc);
void asd_control_led(struct asd_ha_struct *asd_ha, int phy_id, int op);
void asd_turn_led(struct asd_ha_struct *asd_ha, int phy_id, int op);
int  asd_enable_phys(struct asd_ha_struct *asd_ha, const u8 phy_mask);

void asd_ascb_timedout(unsigned long data);
int  asd_chip_hardrst(struct asd_ha_struct *asd_ha);

#endif
