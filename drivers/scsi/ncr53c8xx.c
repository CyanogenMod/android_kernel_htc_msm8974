/******************************************************************************
**  Device driver for the PCI-SCSI NCR538XX controller family.
**
**  Copyright (C) 1994  Wolfgang Stanglmeier
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
**
**  This driver has been ported to Linux from the FreeBSD NCR53C8XX driver
**  and is currently maintained by
**
**          Gerard Roudier              <groudier@free.fr>
**
**  Being given that this driver originates from the FreeBSD version, and
**  in order to keep synergy on both, any suggested enhancements and corrections
**  received on Linux are automatically a potential candidate for the FreeBSD 
**  version.
**
**  The original driver has been written for 386bsd and FreeBSD by
**          Wolfgang Stanglmeier        <wolf@cologne.de>
**          Stefan Esser                <se@mi.Uni-Koeln.de>
**
**  And has been ported to NetBSD by
**          Charles M. Hannum           <mycroft@gnu.ai.mit.edu>
**
**-----------------------------------------------------------------------------
**
**                     Brief history
**
**  December 10 1995 by Gerard Roudier:
**     Initial port to Linux.
**
**  June 23 1996 by Gerard Roudier:
**     Support for 64 bits architectures (Alpha).
**
**  November 30 1996 by Gerard Roudier:
**     Support for Fast-20 scsi.
**     Support for large DMA fifo and 128 dwords bursting.
**
**  February 27 1997 by Gerard Roudier:
**     Support for Fast-40 scsi.
**     Support for on-Board RAM.
**
**  May 3 1997 by Gerard Roudier:
**     Full support for scsi scripts instructions pre-fetching.
**
**  May 19 1997 by Richard Waltham <dormouse@farsrobt.demon.co.uk>:
**     Support for NvRAM detection and reading.
**
**  August 18 1997 by Cort <cort@cs.nmt.edu>:
**     Support for Power/PC (Big Endian).
**
**  June 20 1998 by Gerard Roudier
**     Support for up to 64 tags per lun.
**     O(1) everywhere (C and SCRIPTS) for normal cases.
**     Low PCI traffic for command handling when on-chip RAM is present.
**     Aggressive SCSI SCRIPTS optimizations.
**
**  2005 by Matthew Wilcox and James Bottomley
**     PCI-ectomy.  This driver now supports only the 720 chip (see the
**     NCR_Q720 and zalon drivers for the bus probe logic).
**
*******************************************************************************
*/


#define SCSI_NCR_DRIVER_NAME	"ncr53c8xx-3.4.3g"

#define SCSI_NCR_DEBUG_FLAGS	(0)

#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/types.h>

#include <asm/dma.h>
#include <asm/io.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_spi.h>

#include "ncr53c8xx.h"

#define NAME53C8XX		"ncr53c8xx"


#define DEBUG_ALLOC    (0x0001)
#define DEBUG_PHASE    (0x0002)
#define DEBUG_QUEUE    (0x0008)
#define DEBUG_RESULT   (0x0010)
#define DEBUG_POINTER  (0x0020)
#define DEBUG_SCRIPT   (0x0040)
#define DEBUG_TINY     (0x0080)
#define DEBUG_TIMING   (0x0100)
#define DEBUG_NEGO     (0x0200)
#define DEBUG_TAGS     (0x0400)
#define DEBUG_SCATTER  (0x0800)
#define DEBUG_IC        (0x1000)


#ifdef SCSI_NCR_DEBUG_INFO_SUPPORT
static int ncr_debug = SCSI_NCR_DEBUG_FLAGS;
	#define DEBUG_FLAGS ncr_debug
#else
	#define DEBUG_FLAGS	SCSI_NCR_DEBUG_FLAGS
#endif

static inline struct list_head *ncr_list_pop(struct list_head *head)
{
	if (!list_empty(head)) {
		struct list_head *elem = head->next;

		list_del(elem);
		return elem;
	}

	return NULL;
}


#define MEMO_SHIFT	4	
#if PAGE_SIZE >= 8192
#define MEMO_PAGE_ORDER	0	
#else
#define MEMO_PAGE_ORDER	1	
#endif
#define MEMO_FREE_UNUSED	
#define MEMO_WARN	1
#define MEMO_GFP_FLAGS	GFP_ATOMIC
#define MEMO_CLUSTER_SHIFT	(PAGE_SHIFT+MEMO_PAGE_ORDER)
#define MEMO_CLUSTER_SIZE	(1UL << MEMO_CLUSTER_SHIFT)
#define MEMO_CLUSTER_MASK	(MEMO_CLUSTER_SIZE-1)

typedef u_long m_addr_t;	
typedef struct device *m_bush_t;	

typedef struct m_link {		
	struct m_link *next;
} m_link_s;

typedef struct m_vtob {		
	struct m_vtob *next;
	m_addr_t vaddr;
	m_addr_t baddr;
} m_vtob_s;
#define VTOB_HASH_SHIFT		5
#define VTOB_HASH_SIZE		(1UL << VTOB_HASH_SHIFT)
#define VTOB_HASH_MASK		(VTOB_HASH_SIZE-1)
#define VTOB_HASH_CODE(m)	\
	((((m_addr_t) (m)) >> MEMO_CLUSTER_SHIFT) & VTOB_HASH_MASK)

typedef struct m_pool {		
	m_bush_t bush;
	m_addr_t (*getp)(struct m_pool *);
	void (*freep)(struct m_pool *, m_addr_t);
	int nump;
	m_vtob_s *(vtob[VTOB_HASH_SIZE]);
	struct m_pool *next;
	struct m_link h[PAGE_SHIFT-MEMO_SHIFT+MEMO_PAGE_ORDER+1];
} m_pool_s;

static void *___m_alloc(m_pool_s *mp, int size)
{
	int i = 0;
	int s = (1 << MEMO_SHIFT);
	int j;
	m_addr_t a;
	m_link_s *h = mp->h;

	if (size > (PAGE_SIZE << MEMO_PAGE_ORDER))
		return NULL;

	while (size > s) {
		s <<= 1;
		++i;
	}

	j = i;
	while (!h[j].next) {
		if (s == (PAGE_SIZE << MEMO_PAGE_ORDER)) {
			h[j].next = (m_link_s *)mp->getp(mp);
			if (h[j].next)
				h[j].next->next = NULL;
			break;
		}
		++j;
		s <<= 1;
	}
	a = (m_addr_t) h[j].next;
	if (a) {
		h[j].next = h[j].next->next;
		while (j > i) {
			j -= 1;
			s >>= 1;
			h[j].next = (m_link_s *) (a+s);
			h[j].next->next = NULL;
		}
	}
#ifdef DEBUG
	printk("___m_alloc(%d) = %p\n", size, (void *) a);
#endif
	return (void *) a;
}

static void ___m_free(m_pool_s *mp, void *ptr, int size)
{
	int i = 0;
	int s = (1 << MEMO_SHIFT);
	m_link_s *q;
	m_addr_t a, b;
	m_link_s *h = mp->h;

#ifdef DEBUG
	printk("___m_free(%p, %d)\n", ptr, size);
#endif

	if (size > (PAGE_SIZE << MEMO_PAGE_ORDER))
		return;

	while (size > s) {
		s <<= 1;
		++i;
	}

	a = (m_addr_t) ptr;

	while (1) {
#ifdef MEMO_FREE_UNUSED
		if (s == (PAGE_SIZE << MEMO_PAGE_ORDER)) {
			mp->freep(mp, a);
			break;
		}
#endif
		b = a ^ s;
		q = &h[i];
		while (q->next && q->next != (m_link_s *) b) {
			q = q->next;
		}
		if (!q->next) {
			((m_link_s *) a)->next = h[i].next;
			h[i].next = (m_link_s *) a;
			break;
		}
		q->next = q->next->next;
		a = a & b;
		s <<= 1;
		++i;
	}
}

static DEFINE_SPINLOCK(ncr53c8xx_lock);

static void *__m_calloc2(m_pool_s *mp, int size, char *name, int uflags)
{
	void *p;

	p = ___m_alloc(mp, size);

	if (DEBUG_FLAGS & DEBUG_ALLOC)
		printk ("new %-10s[%4d] @%p.\n", name, size, p);

	if (p)
		memset(p, 0, size);
	else if (uflags & MEMO_WARN)
		printk (NAME53C8XX ": failed to allocate %s[%d]\n", name, size);

	return p;
}

#define __m_calloc(mp, s, n)	__m_calloc2(mp, s, n, MEMO_WARN)

static void __m_free(m_pool_s *mp, void *ptr, int size, char *name)
{
	if (DEBUG_FLAGS & DEBUG_ALLOC)
		printk ("freeing %-10s[%4d] @%p.\n", name, size, ptr);

	___m_free(mp, ptr, size);

}


static m_addr_t ___mp0_getp(m_pool_s *mp)
{
	m_addr_t m = __get_free_pages(MEMO_GFP_FLAGS, MEMO_PAGE_ORDER);
	if (m)
		++mp->nump;
	return m;
}

static void ___mp0_freep(m_pool_s *mp, m_addr_t m)
{
	free_pages(m, MEMO_PAGE_ORDER);
	--mp->nump;
}

static m_pool_s mp0 = {NULL, ___mp0_getp, ___mp0_freep};


static m_addr_t ___dma_getp(m_pool_s *mp)
{
	m_addr_t vp;
	m_vtob_s *vbp;

	vbp = __m_calloc(&mp0, sizeof(*vbp), "VTOB");
	if (vbp) {
		dma_addr_t daddr;
		vp = (m_addr_t) dma_alloc_coherent(mp->bush,
						PAGE_SIZE<<MEMO_PAGE_ORDER,
						&daddr, GFP_ATOMIC);
		if (vp) {
			int hc = VTOB_HASH_CODE(vp);
			vbp->vaddr = vp;
			vbp->baddr = daddr;
			vbp->next = mp->vtob[hc];
			mp->vtob[hc] = vbp;
			++mp->nump;
			return vp;
		}
	}
	if (vbp)
		__m_free(&mp0, vbp, sizeof(*vbp), "VTOB");
	return 0;
}

static void ___dma_freep(m_pool_s *mp, m_addr_t m)
{
	m_vtob_s **vbpp, *vbp;
	int hc = VTOB_HASH_CODE(m);

	vbpp = &mp->vtob[hc];
	while (*vbpp && (*vbpp)->vaddr != m)
		vbpp = &(*vbpp)->next;
	if (*vbpp) {
		vbp = *vbpp;
		*vbpp = (*vbpp)->next;
		dma_free_coherent(mp->bush, PAGE_SIZE<<MEMO_PAGE_ORDER,
				  (void *)vbp->vaddr, (dma_addr_t)vbp->baddr);
		__m_free(&mp0, vbp, sizeof(*vbp), "VTOB");
		--mp->nump;
	}
}

static inline m_pool_s *___get_dma_pool(m_bush_t bush)
{
	m_pool_s *mp;
	for (mp = mp0.next; mp && mp->bush != bush; mp = mp->next);
	return mp;
}

static m_pool_s *___cre_dma_pool(m_bush_t bush)
{
	m_pool_s *mp;
	mp = __m_calloc(&mp0, sizeof(*mp), "MPOOL");
	if (mp) {
		memset(mp, 0, sizeof(*mp));
		mp->bush = bush;
		mp->getp = ___dma_getp;
		mp->freep = ___dma_freep;
		mp->next = mp0.next;
		mp0.next = mp;
	}
	return mp;
}

static void ___del_dma_pool(m_pool_s *p)
{
	struct m_pool **pp = &mp0.next;

	while (*pp && *pp != p)
		pp = &(*pp)->next;
	if (*pp) {
		*pp = (*pp)->next;
		__m_free(&mp0, p, sizeof(*p), "MPOOL");
	}
}

static void *__m_calloc_dma(m_bush_t bush, int size, char *name)
{
	u_long flags;
	struct m_pool *mp;
	void *m = NULL;

	spin_lock_irqsave(&ncr53c8xx_lock, flags);
	mp = ___get_dma_pool(bush);
	if (!mp)
		mp = ___cre_dma_pool(bush);
	if (mp)
		m = __m_calloc(mp, size, name);
	if (mp && !mp->nump)
		___del_dma_pool(mp);
	spin_unlock_irqrestore(&ncr53c8xx_lock, flags);

	return m;
}

static void __m_free_dma(m_bush_t bush, void *m, int size, char *name)
{
	u_long flags;
	struct m_pool *mp;

	spin_lock_irqsave(&ncr53c8xx_lock, flags);
	mp = ___get_dma_pool(bush);
	if (mp)
		__m_free(mp, m, size, name);
	if (mp && !mp->nump)
		___del_dma_pool(mp);
	spin_unlock_irqrestore(&ncr53c8xx_lock, flags);
}

static m_addr_t __vtobus(m_bush_t bush, void *m)
{
	u_long flags;
	m_pool_s *mp;
	int hc = VTOB_HASH_CODE(m);
	m_vtob_s *vp = NULL;
	m_addr_t a = ((m_addr_t) m) & ~MEMO_CLUSTER_MASK;

	spin_lock_irqsave(&ncr53c8xx_lock, flags);
	mp = ___get_dma_pool(bush);
	if (mp) {
		vp = mp->vtob[hc];
		while (vp && (m_addr_t) vp->vaddr != a)
			vp = vp->next;
	}
	spin_unlock_irqrestore(&ncr53c8xx_lock, flags);
	return vp ? vp->baddr + (((m_addr_t) m) - a) : 0;
}

#define _m_calloc_dma(np, s, n)		__m_calloc_dma(np->dev, s, n)
#define _m_free_dma(np, p, s, n)	__m_free_dma(np->dev, p, s, n)
#define m_calloc_dma(s, n)		_m_calloc_dma(np, s, n)
#define m_free_dma(p, s, n)		_m_free_dma(np, p, s, n)
#define _vtobus(np, p)			__vtobus(np->dev, p)
#define vtobus(p)			_vtobus(np, p)


#define __data_mapped	SCp.phase
#define __data_mapping	SCp.have_data_in

static void __unmap_scsi_data(struct device *dev, struct scsi_cmnd *cmd)
{
	switch(cmd->__data_mapped) {
	case 2:
		scsi_dma_unmap(cmd);
		break;
	}
	cmd->__data_mapped = 0;
}

static int __map_scsi_sg_data(struct device *dev, struct scsi_cmnd *cmd)
{
	int use_sg;

	use_sg = scsi_dma_map(cmd);
	if (!use_sg)
		return 0;

	cmd->__data_mapped = 2;
	cmd->__data_mapping = use_sg;

	return use_sg;
}

#define unmap_scsi_data(np, cmd)	__unmap_scsi_data(np->dev, cmd)
#define map_scsi_sg_data(np, cmd)	__map_scsi_sg_data(np->dev, cmd)

static struct ncr_driver_setup
	driver_setup			= SCSI_NCR_DRIVER_SETUP;

#ifndef MODULE
#ifdef	SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT
static struct ncr_driver_setup
	driver_safe_setup __initdata	= SCSI_NCR_DRIVER_SAFE_SETUP;
#endif
#endif 

#define initverbose (driver_setup.verbose)
#define bootverbose (np->verbose)



#ifdef MODULE
#define	ARG_SEP	' '
#else
#define	ARG_SEP	','
#endif

#define OPT_TAGS		1
#define OPT_MASTER_PARITY	2
#define OPT_SCSI_PARITY		3
#define OPT_DISCONNECTION	4
#define OPT_SPECIAL_FEATURES	5
#define OPT_UNUSED_1		6
#define OPT_FORCE_SYNC_NEGO	7
#define OPT_REVERSE_PROBE	8
#define OPT_DEFAULT_SYNC	9
#define OPT_VERBOSE		10
#define OPT_DEBUG		11
#define OPT_BURST_MAX		12
#define OPT_LED_PIN		13
#define OPT_MAX_WIDE		14
#define OPT_SETTLE_DELAY	15
#define OPT_DIFF_SUPPORT	16
#define OPT_IRQM		17
#define OPT_PCI_FIX_UP		18
#define OPT_BUS_CHECK		19
#define OPT_OPTIMIZE		20
#define OPT_RECOVERY		21
#define OPT_SAFE_SETUP		22
#define OPT_USE_NVRAM		23
#define OPT_EXCLUDE		24
#define OPT_HOST_ID		25

#ifdef SCSI_NCR_IARB_SUPPORT
#define OPT_IARB		26
#endif

#ifdef MODULE
#define	ARG_SEP	' '
#else
#define	ARG_SEP	','
#endif

#ifndef MODULE
static char setup_token[] __initdata = 
	"tags:"   "mpar:"
	"spar:"   "disc:"
	"specf:"  "ultra:"
	"fsn:"    "revprob:"
	"sync:"   "verb:"
	"debug:"  "burst:"
	"led:"    "wide:"
	"settle:" "diff:"
	"irqm:"   "pcifix:"
	"buschk:" "optim:"
	"recovery:"
	"safe:"   "nvram:"
	"excl:"   "hostid:"
#ifdef SCSI_NCR_IARB_SUPPORT
	"iarb:"
#endif
	;	

static int __init get_setup_token(char *p)
{
	char *cur = setup_token;
	char *pc;
	int i = 0;

	while (cur != NULL && (pc = strchr(cur, ':')) != NULL) {
		++pc;
		++i;
		if (!strncmp(p, cur, pc - cur))
			return i;
		cur = pc;
	}
	return 0;
}

static int __init sym53c8xx__setup(char *str)
{
#ifdef SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT
	char *cur = str;
	char *pc, *pv;
	int i, val, c;
	int xi = 0;

	while (cur != NULL && (pc = strchr(cur, ':')) != NULL) {
		char *pe;

		val = 0;
		pv = pc;
		c = *++pv;

		if	(c == 'n')
			val = 0;
		else if	(c == 'y')
			val = 1;
		else
			val = (int) simple_strtoul(pv, &pe, 0);

		switch (get_setup_token(cur)) {
		case OPT_TAGS:
			driver_setup.default_tags = val;
			if (pe && *pe == '/') {
				i = 0;
				while (*pe && *pe != ARG_SEP && 
					i < sizeof(driver_setup.tag_ctrl)-1) {
					driver_setup.tag_ctrl[i++] = *pe++;
				}
				driver_setup.tag_ctrl[i] = '\0';
			}
			break;
		case OPT_MASTER_PARITY:
			driver_setup.master_parity = val;
			break;
		case OPT_SCSI_PARITY:
			driver_setup.scsi_parity = val;
			break;
		case OPT_DISCONNECTION:
			driver_setup.disconnection = val;
			break;
		case OPT_SPECIAL_FEATURES:
			driver_setup.special_features = val;
			break;
		case OPT_FORCE_SYNC_NEGO:
			driver_setup.force_sync_nego = val;
			break;
		case OPT_REVERSE_PROBE:
			driver_setup.reverse_probe = val;
			break;
		case OPT_DEFAULT_SYNC:
			driver_setup.default_sync = val;
			break;
		case OPT_VERBOSE:
			driver_setup.verbose = val;
			break;
		case OPT_DEBUG:
			driver_setup.debug = val;
			break;
		case OPT_BURST_MAX:
			driver_setup.burst_max = val;
			break;
		case OPT_LED_PIN:
			driver_setup.led_pin = val;
			break;
		case OPT_MAX_WIDE:
			driver_setup.max_wide = val? 1:0;
			break;
		case OPT_SETTLE_DELAY:
			driver_setup.settle_delay = val;
			break;
		case OPT_DIFF_SUPPORT:
			driver_setup.diff_support = val;
			break;
		case OPT_IRQM:
			driver_setup.irqm = val;
			break;
		case OPT_PCI_FIX_UP:
			driver_setup.pci_fix_up	= val;
			break;
		case OPT_BUS_CHECK:
			driver_setup.bus_check = val;
			break;
		case OPT_OPTIMIZE:
			driver_setup.optimize = val;
			break;
		case OPT_RECOVERY:
			driver_setup.recovery = val;
			break;
		case OPT_USE_NVRAM:
			driver_setup.use_nvram = val;
			break;
		case OPT_SAFE_SETUP:
			memcpy(&driver_setup, &driver_safe_setup,
				sizeof(driver_setup));
			break;
		case OPT_EXCLUDE:
			if (xi < SCSI_NCR_MAX_EXCLUDES)
				driver_setup.excludes[xi++] = val;
			break;
		case OPT_HOST_ID:
			driver_setup.host_id = val;
			break;
#ifdef SCSI_NCR_IARB_SUPPORT
		case OPT_IARB:
			driver_setup.iarb = val;
			break;
#endif
		default:
			printk("sym53c8xx_setup: unexpected boot option '%.*s' ignored\n", (int)(pc-cur+1), cur);
			break;
		}

		if ((cur = strchr(cur, ARG_SEP)) != NULL)
			++cur;
	}
#endif 
	return 1;
}
#endif 

#define DEF_DEPTH	(driver_setup.default_tags)
#define ALL_TARGETS	-2
#define NO_TARGET	-1
#define ALL_LUNS	-2
#define NO_LUN		-1

static int device_queue_depth(int unit, int target, int lun)
{
	int c, h, t, u, v;
	char *p = driver_setup.tag_ctrl;
	char *ep;

	h = -1;
	t = NO_TARGET;
	u = NO_LUN;
	while ((c = *p++) != 0) {
		v = simple_strtoul(p, &ep, 0);
		switch(c) {
		case '/':
			++h;
			t = ALL_TARGETS;
			u = ALL_LUNS;
			break;
		case 't':
			if (t != target)
				t = (target == v) ? v : NO_TARGET;
			u = ALL_LUNS;
			break;
		case 'u':
			if (u != lun)
				u = (lun == v) ? v : NO_LUN;
			break;
		case 'q':
			if (h == unit &&
				(t == ALL_TARGETS || t == target) &&
				(u == ALL_LUNS    || u == lun))
				return v;
			break;
		case '-':
			t = ALL_TARGETS;
			u = ALL_LUNS;
			break;
		default:
			break;
		}
		p = ep;
	}
	return DEF_DEPTH;
}


 
#define SCSI_NCR_CCB_DONE_SUPPORT
#ifdef  SCSI_NCR_CCB_DONE_SUPPORT

#define MAX_DONE 24
#define CCB_DONE_EMPTY 0xffffffffUL

#if BITS_PER_LONG == 32
#define CCB_DONE_VALID(cp)  (((u_long) cp) != CCB_DONE_EMPTY)

#else
#define CCB_DONE_VALID(cp)  \
	((((u_long) cp) & 0xffffffff00000000ul) && 	\
	 (((u_long) cp) & 0xfffffffful) != CCB_DONE_EMPTY)
#endif

#endif 



#ifndef SCSI_NCR_MYADDR
#define SCSI_NCR_MYADDR      (7)
#endif


#ifndef SCSI_NCR_MAX_TAGS
#define SCSI_NCR_MAX_TAGS    (8)
#endif

#if	SCSI_NCR_MAX_TAGS > 64
#define	MAX_TAGS (64)
#else
#define	MAX_TAGS SCSI_NCR_MAX_TAGS
#endif

#define NO_TAG	(255)

#if	MAX_TAGS > 32
typedef u64 tagmap_t;
#else
typedef u32 tagmap_t;
#endif


#ifdef SCSI_NCR_MAX_TARGET
#define MAX_TARGET  (SCSI_NCR_MAX_TARGET)
#else
#define MAX_TARGET  (16)
#endif


#ifdef SCSI_NCR_MAX_LUN
#define MAX_LUN    SCSI_NCR_MAX_LUN
#else
#define MAX_LUN    (1)
#endif

 
#ifndef SCSI_NCR_MIN_ASYNC
#define SCSI_NCR_MIN_ASYNC (40)
#endif


#ifdef SCSI_NCR_CAN_QUEUE
#define MAX_START   (SCSI_NCR_CAN_QUEUE + 4)
#else
#define MAX_START   (MAX_TARGET + 7 * MAX_TAGS)
#endif

#if	MAX_START > 250
#undef	MAX_START
#define	MAX_START 250
#endif


#define MAX_SCATTER (SCSI_NCR_MAX_SCATTER)

#if (MAX_SCATTER > 80)
#define MAX_SCATTERL	80
#define	MAX_SCATTERH	(MAX_SCATTER - MAX_SCATTERL)
#else
#define MAX_SCATTERL	(MAX_SCATTER-1)
#define	MAX_SCATTERH	1
#endif


#define NCR_SNOOP_TIMEOUT (1000000)


#define ScsiResult(host_code, scsi_code) (((host_code) << 16) + ((scsi_code) & 0x7f))

#define initverbose (driver_setup.verbose)
#define bootverbose (np->verbose)


#define HS_IDLE		(0)
#define HS_BUSY		(1)
#define HS_NEGOTIATE	(2)	
#define HS_DISCONNECT	(3)	

#define HS_DONEMASK	(0x80)
#define HS_COMPLETE	(4|HS_DONEMASK)
#define HS_SEL_TIMEOUT	(5|HS_DONEMASK)	
#define HS_RESET	(6|HS_DONEMASK)	
#define HS_ABORTED	(7|HS_DONEMASK)	
#define HS_TIMEOUT	(8|HS_DONEMASK)	
#define HS_FAIL		(9|HS_DONEMASK)	
#define HS_UNEXPECTED	(10|HS_DONEMASK)


#define HS_INVALMASK	(0x40)
#define	HS_SELECTING	(0|HS_INVALMASK)
#define	HS_IN_RESELECT	(1|HS_INVALMASK)
#define	HS_STARTING	(2|HS_INVALMASK)

#define HS_SKIPMASK	(0x20)


#define	SIR_BAD_STATUS		(1)
#define	SIR_XXXXXXXXXX		(2)
#define	SIR_NEGO_SYNC		(3)
#define	SIR_NEGO_WIDE		(4)
#define	SIR_NEGO_FAILED		(5)
#define	SIR_NEGO_PROTO		(6)
#define	SIR_REJECT_RECEIVED	(7)
#define	SIR_REJECT_SENT		(8)
#define	SIR_IGN_RESIDUE		(9)
#define	SIR_MISSING_SAVE	(10)
#define	SIR_RESEL_NO_MSG_IN	(11)
#define	SIR_RESEL_NO_IDENTIFY	(12)
#define	SIR_RESEL_BAD_LUN	(13)
#define	SIR_RESEL_BAD_TARGET	(14)
#define	SIR_RESEL_BAD_I_T_L	(15)
#define	SIR_RESEL_BAD_I_T_L_Q	(16)
#define	SIR_DONE_OVERFLOW	(17)
#define	SIR_INTFLY		(18)
#define	SIR_MAX			(18)


#define	XE_OK		(0)
#define	XE_EXTRA_DATA	(1)	
#define	XE_BAD_PHASE	(2)	


#define NS_NOCHANGE	(0)
#define NS_SYNC		(1)
#define NS_WIDE		(2)
#define NS_PPR		(4)


#define CCB_MAGIC	(0xf2691ad2)


static struct scsi_transport_template *ncr53c8xx_transport_template = NULL;

struct tcb;
struct lcb;
struct ccb;
struct ncb;
struct script;

struct link {
	ncrcmd	l_cmd;
	ncrcmd	l_paddr;
};

struct	usrcmd {
	u_long	target;
	u_long	lun;
	u_long	data;
	u_long	cmd;
};

#define UC_SETSYNC      10
#define UC_SETTAGS	11
#define UC_SETDEBUG	12
#define UC_SETORDER	13
#define UC_SETWIDE	14
#define UC_SETFLAG	15
#define UC_SETVERBOSE	17

#define	UF_TRACE	(0x01)
#define	UF_NODISC	(0x02)
#define	UF_NOSCAN	(0x04)

struct tcb {
	struct link   jump_tcb;

	ncrcmd	getscr[6];

	struct link   call_lun;

	struct link     jump_lcb[4];	
	struct lcb *	lp[MAX_LUN];	

	struct ccb *   nego_cp;

	u_long	transfers;
	u_long	bytes;

#ifdef SCSI_NCR_BIG_ENDIAN
	u16	period;
	u_char	sval;
	u_char	minsync;
	u_char	wval;
	u_char	widedone;
	u_char	quirks;
	u_char	maxoffs;
#else
	u_char	minsync;
	u_char	sval;
	u16	period;
	u_char	maxoffs;
	u_char	quirks;
	u_char	widedone;
	u_char	wval;
#endif

	
	u_char	usrsync;
	u_char	usrwide;
	u_char	usrtags;
	u_char	usrflag;
	struct scsi_target *starget;
};

struct lcb {
	struct link	jump_lcb;
	ncrcmd		load_jump_ccb[3];
	struct link	jump_tag;
	ncrcmd		p_jump_ccb;	

	u32		jump_ccb_0;	
	u32		*jump_ccb;	

	struct list_head free_ccbq;	
	struct list_head busy_ccbq;	
	struct list_head wait_ccbq;	
	struct list_head skip_ccbq;	
	u_char		actccbs;	
	u_char		busyccbs;	
	u_char		queuedccbs;	
	u_char		queuedepth;	
	u_char		scdev_depth;	
	u_char		maxnxs;		

	u_char		ia_tag;		
	u_char		if_tag;		
	u_char cb_tags[MAX_TAGS];	
	u_char		usetags;	
	u_char		maxtags;	
	u_char		numtags;	

	u16		num_good;	
	tagmap_t	tags_umap;	
	tagmap_t	tags_smap;	
	u_long		tags_stime;	
	struct ccb *	held_ccb;	
};

struct launch {
	ncrcmd		setup_dsa[3];	
	struct link	schedule;	
	ncrcmd		p_phys;		
};


struct head {
	/*----------------------------------------------------------------
	**	Saved data pointer.
	**	Points to the position in the script responsible for the
	**	actual transfer transfer of data.
	**	It's written after reception of a SAVE_DATA_POINTER message.
	**	The goalpointer points after the last transfer command.
	**----------------------------------------------------------------
	*/
	u32		savep;
	u32		lastp;
	u32		goalp;

	u32		wlastp;
	u32		wgoalp;

	struct ccb *	cp;

	u_char		scr_st[4];	
	u_char		status[4];	
					
};


#define  QU_REG	scr0
#define  HS_REG	scr1
#define  HS_PRT	nc_scr1
#define  SS_REG	scr2
#define  SS_PRT	nc_scr2
#define  PS_REG	scr3

#ifdef SCSI_NCR_BIG_ENDIAN
#define  actualquirks  phys.header.status[3]
#define  host_status   phys.header.status[2]
#define  scsi_status   phys.header.status[1]
#define  parity_status phys.header.status[0]
#else
#define  actualquirks  phys.header.status[0]
#define  host_status   phys.header.status[1]
#define  scsi_status   phys.header.status[2]
#define  parity_status phys.header.status[3]
#endif

#define  xerr_st       header.scr_st[0]
#define  sync_st       header.scr_st[1]
#define  nego_st       header.scr_st[2]
#define  wide_st       header.scr_st[3]

#define  xerr_status   phys.xerr_st
#define  nego_status   phys.nego_st

#if 0
#define  sync_status   phys.sync_st
#define  wide_status   phys.wide_st
#endif


struct dsb {


	struct head	header;


	struct scr_tblsel  select;
	struct scr_tblmove smsg  ;
	struct scr_tblmove cmd   ;
	struct scr_tblmove sense ;
	struct scr_tblmove data[MAX_SCATTER];
};


struct ccb {
	struct dsb	phys;

	struct launch	start;

	struct launch	restart;

	ncrcmd		patch[8];

	struct scsi_cmnd	*cmd;		
	u_char		cdb_buf[16];	
	u_char		sense_buf[64];
	int		data_len;	

	u_char		scsi_smsg [8];
	u_char		scsi_smsg2[8];

	u_long		p_ccb;		
	u_char		sensecmd[6];	
	u_char		tag;		
					
	u_char		target;
	u_char		lun;
	u_char		queued;
	u_char		auto_sense;
	struct ccb *	link_ccb;	
	struct list_head link_ccbq;	
	u32		startp;		
	u_long		magic;		
};

#define CCB_PHYS(cp,lbl)	(cp->p_ccb + offsetof(struct ccb, lbl))


struct ncb {
	struct head     header;

	struct scsi_cmnd	*waiting_list;	
					
	struct scsi_cmnd	*done_list;	
					 
	spinlock_t	smp_lock;	

	int		unit;		
	char		inst_name[16];	

	u_char	sv_scntl0, sv_scntl3, sv_dmode, sv_dcntl, sv_ctest0, sv_ctest3,
		sv_ctest4, sv_ctest5, sv_gpcntl, sv_stest2, sv_stest4;

	u_char	rv_scntl0, rv_scntl3, rv_dmode, rv_dcntl, rv_ctest0, rv_ctest3,
		rv_ctest4, rv_ctest5, rv_stest2;

	struct link     jump_tcb[4];	
	struct tcb  target[MAX_TARGET];	

	void __iomem *vaddr;		
	unsigned long	paddr;		
	unsigned long	paddr2;		
	volatile			
	struct ncr_reg	__iomem *reg;	

	struct script	*script0;	
	struct scripth	*scripth0;	
	struct scripth	*scripth;	
	u_long		p_script;	
	u_long		p_scripth;	

	struct device	*dev;
	u_char		revision_id;	
	u32		irq;		
	u32		features;	
	u_char		myaddr;		
	u_char		maxburst;	
	u_char		maxwide;	
	u_char		minsync;	
	u_char		maxsync;	
	u_char		maxoffs;	
	u_char		multiplier;	
	u_char		clock_divn;	
	u_long		clock_khz;	

	u16		squeueput;	
	u16		actccbs;	
	u16		queuedccbs;	
	u16		queuedepth;	

	struct timer_list timer;	
	u_long		lasttime;
	u_long		settle_time;	

	struct ncr_reg	regdump;	
	u_long		regtime;	

	/*----------------------------------------------------------------
	**	Miscellaneous buffers accessed by the scripts-processor.
	**	They shall be DWORD aligned, because they may be read or 
	**	written with a SCR_COPY script command.
	**----------------------------------------------------------------
	*/
	u_char		msgout[8];	
	u_char		msgin [8];	
	u32		lastmsg;	
	u_char		scratch;	

	u_char		disc;		
	u_char		scsi_mode;	
	u_char		order;		
	u_char		verbose;	
	int		ncr_cache;	
	u_long		p_ncb;		

#ifdef SCSI_NCR_CCB_DONE_SUPPORT
	struct ccb	*(ccb_done[MAX_DONE]);
	int		ccb_done_ic;
#endif
	struct ccb	*ccb;		
	struct usrcmd	user;		
	volatile u_char	release_stage;	
};

#define NCB_SCRIPT_PHYS(np,lbl)	 (np->p_script  + offsetof (struct script, lbl))
#define NCB_SCRIPTH_PHYS(np,lbl) (np->p_scripth + offsetof (struct scripth,lbl))



#ifdef CONFIG_NCR53C8XX_PREFETCH
#define PREFETCH_FLUSH_CNT	2
#define PREFETCH_FLUSH		SCR_CALL, PADDRH (wait_dma),
#else
#define PREFETCH_FLUSH_CNT	0
#define PREFETCH_FLUSH
#endif

struct script {
	ncrcmd	start		[  5];
	ncrcmd  startpos	[  1];
	ncrcmd	select		[  6];
	ncrcmd	select2		[  9 + PREFETCH_FLUSH_CNT];
	ncrcmd	loadpos		[  4];
	ncrcmd	send_ident	[  9];
	ncrcmd	prepare		[  6];
	ncrcmd	prepare2	[  7];
	ncrcmd  command		[  6];
	ncrcmd  dispatch	[ 32];
	ncrcmd  clrack		[  4];
	ncrcmd	no_data		[ 17];
	ncrcmd  status		[  8];
	ncrcmd  msg_in		[  2];
	ncrcmd  msg_in2		[ 16];
	ncrcmd  msg_bad		[  4];
	ncrcmd	setmsg		[  7];
	ncrcmd	cleanup		[  6];
	ncrcmd  complete	[  9];
	ncrcmd	cleanup_ok	[  8 + PREFETCH_FLUSH_CNT];
	ncrcmd	cleanup0	[  1];
#ifndef SCSI_NCR_CCB_DONE_SUPPORT
	ncrcmd	signal		[ 12];
#else
	ncrcmd	signal		[  9];
	ncrcmd	done_pos	[  1];
	ncrcmd	done_plug	[  2];
	ncrcmd	done_end	[  7];
#endif
	ncrcmd  save_dp		[  7];
	ncrcmd  restore_dp	[  5];
	ncrcmd  disconnect	[ 10];
	ncrcmd	msg_out		[  9];
	ncrcmd	msg_out_done	[  7];
	ncrcmd  idle		[  2];
	ncrcmd	reselect	[  8];
	ncrcmd	reselected	[  8];
	ncrcmd	resel_dsa	[  6 + PREFETCH_FLUSH_CNT];
	ncrcmd	loadpos1	[  4];
	ncrcmd  resel_lun	[  6];
	ncrcmd	resel_tag	[  6];
	ncrcmd	jump_to_nexus	[  4 + PREFETCH_FLUSH_CNT];
	ncrcmd	nexus_indirect	[  4];
	ncrcmd	resel_notag	[  4];
	ncrcmd  data_in		[MAX_SCATTERL * 4];
	ncrcmd  data_in2	[  4];
	ncrcmd  data_out	[MAX_SCATTERL * 4];
	ncrcmd  data_out2	[  4];
};

struct scripth {
	ncrcmd  tryloop		[MAX_START*2];
	ncrcmd  tryloop2	[  2];
#ifdef SCSI_NCR_CCB_DONE_SUPPORT
	ncrcmd  done_queue	[MAX_DONE*5];
	ncrcmd  done_queue2	[  2];
#endif
	ncrcmd	select_no_atn	[  8];
	ncrcmd	cancel		[  4];
	ncrcmd	skip		[  9 + PREFETCH_FLUSH_CNT];
	ncrcmd	skip2		[ 19];
	ncrcmd	par_err_data_in	[  6];
	ncrcmd	par_err_other	[  4];
	ncrcmd	msg_reject	[  8];
	ncrcmd	msg_ign_residue	[ 24];
	ncrcmd  msg_extended	[ 10];
	ncrcmd  msg_ext_2	[ 10];
	ncrcmd	msg_wdtr	[ 14];
	ncrcmd	send_wdtr	[  7];
	ncrcmd  msg_ext_3	[ 10];
	ncrcmd	msg_sdtr	[ 14];
	ncrcmd	send_sdtr	[  7];
	ncrcmd	nego_bad_phase	[  4];
	ncrcmd	msg_out_abort	[ 10];
	ncrcmd  hdata_in	[MAX_SCATTERH * 4];
	ncrcmd  hdata_in2	[  2];
	ncrcmd  hdata_out	[MAX_SCATTERH * 4];
	ncrcmd  hdata_out2	[  2];
	ncrcmd	reset		[  4];
	ncrcmd	aborttag	[  4];
	ncrcmd	abort		[  2];
	ncrcmd	abort_resel	[ 20];
	ncrcmd	resend_ident	[  4];
	ncrcmd	clratn_go_on	[  3];
	ncrcmd	nxtdsp_go_on	[  1];
	ncrcmd	sdata_in	[  8];
	ncrcmd  data_io		[ 18];
	ncrcmd	bad_identify	[ 12];
	ncrcmd	bad_i_t_l	[  4];
	ncrcmd	bad_i_t_l_q	[  4];
	ncrcmd	bad_target	[  8];
	ncrcmd	bad_status	[  8];
	ncrcmd	start_ram	[  4 + PREFETCH_FLUSH_CNT];
	ncrcmd	start_ram0	[  4];
	ncrcmd	sto_restart	[  5];
	ncrcmd	wait_dma	[  2];
	ncrcmd	snooptest	[  9];
	ncrcmd	snoopend	[  2];
};


static	void	ncr_alloc_ccb	(struct ncb *np, u_char tn, u_char ln);
static	void	ncr_complete	(struct ncb *np, struct ccb *cp);
static	void	ncr_exception	(struct ncb *np);
static	void	ncr_free_ccb	(struct ncb *np, struct ccb *cp);
static	void	ncr_init_ccb	(struct ncb *np, struct ccb *cp);
static	void	ncr_init_tcb	(struct ncb *np, u_char tn);
static	struct lcb *	ncr_alloc_lcb	(struct ncb *np, u_char tn, u_char ln);
static	struct lcb *	ncr_setup_lcb	(struct ncb *np, struct scsi_device *sdev);
static	void	ncr_getclock	(struct ncb *np, int mult);
static	void	ncr_selectclock	(struct ncb *np, u_char scntl3);
static	struct ccb *ncr_get_ccb	(struct ncb *np, struct scsi_cmnd *cmd);
static	void	ncr_chip_reset	(struct ncb *np, int delay);
static	void	ncr_init	(struct ncb *np, int reset, char * msg, u_long code);
static	int	ncr_int_sbmc	(struct ncb *np);
static	int	ncr_int_par	(struct ncb *np);
static	void	ncr_int_ma	(struct ncb *np);
static	void	ncr_int_sir	(struct ncb *np);
static  void    ncr_int_sto     (struct ncb *np);
static	void	ncr_negotiate	(struct ncb* np, struct tcb* tp);
static	int	ncr_prepare_nego(struct ncb *np, struct ccb *cp, u_char *msgptr);

static	void	ncr_script_copy_and_bind
				(struct ncb *np, ncrcmd *src, ncrcmd *dst, int len);
static  void    ncr_script_fill (struct script * scr, struct scripth * scripth);
static	int	ncr_scatter	(struct ncb *np, struct ccb *cp, struct scsi_cmnd *cmd);
static	void	ncr_getsync	(struct ncb *np, u_char sfac, u_char *fakp, u_char *scntl3p);
static	void	ncr_setsync	(struct ncb *np, struct ccb *cp, u_char scntl3, u_char sxfer);
static	void	ncr_setup_tags	(struct ncb *np, struct scsi_device *sdev);
static	void	ncr_setwide	(struct ncb *np, struct ccb *cp, u_char wide, u_char ack);
static	int	ncr_snooptest	(struct ncb *np);
static	void	ncr_timeout	(struct ncb *np);
static  void    ncr_wakeup      (struct ncb *np, u_long code);
static  void    ncr_wakeup_done (struct ncb *np);
static	void	ncr_start_next_ccb (struct ncb *np, struct lcb * lp, int maxn);
static	void	ncr_put_start_queue(struct ncb *np, struct ccb *cp);

static void insert_into_waiting_list(struct ncb *np, struct scsi_cmnd *cmd);
static struct scsi_cmnd *retrieve_from_waiting_list(int to_remove, struct ncb *np, struct scsi_cmnd *cmd);
static void process_waiting_list(struct ncb *np, int sts);

#define remove_from_waiting_list(np, cmd) \
		retrieve_from_waiting_list(1, (np), (cmd))
#define requeue_waiting_list(np) process_waiting_list((np), DID_OK)
#define reset_waiting_list(np) process_waiting_list((np), DID_RESET)

static inline char *ncr_name (struct ncb *np)
{
	return np->inst_name;
}



#define	RELOC_SOFTC	0x40000000
#define	RELOC_LABEL	0x50000000
#define	RELOC_REGISTER	0x60000000
#if 0
#define	RELOC_KVAR	0x70000000
#endif
#define	RELOC_LABELH	0x80000000
#define	RELOC_MASK	0xf0000000

#define	NADDR(label)	(RELOC_SOFTC | offsetof(struct ncb, label))
#define PADDR(label)    (RELOC_LABEL | offsetof(struct script, label))
#define PADDRH(label)   (RELOC_LABELH | offsetof(struct scripth, label))
#define	RADDR(label)	(RELOC_REGISTER | REG(label))
#define	FADDR(label,ofs)(RELOC_REGISTER | ((REG(label))+(ofs)))
#if 0
#define	KVAR(which)	(RELOC_KVAR | (which))
#endif

#if 0
#define	SCRIPT_KVAR_JIFFIES	(0)
#define	SCRIPT_KVAR_FIRST		SCRIPT_KVAR_JIFFIES
#define	SCRIPT_KVAR_LAST		SCRIPT_KVAR_JIFFIES
static void *script_kvars[] __initdata =
	{ (void *)&jiffies };
#endif

static	struct script script0 __initdata = {
 {
	SCR_NO_OP,
		0,
	SCR_FROM_REG (ctest2),
		0,
	SCR_JUMP,
},{
		PADDRH(tryloop),

},{

	SCR_CLR (SCR_TRG),
		0,
	SCR_LOAD_REG (HS_REG, HS_SELECTING),
		0,

	SCR_SEL_TBL_ATN ^ offsetof (struct dsb, select),
		PADDR (reselect),

},{

	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		0,

	SCR_COPY (4),
		RADDR (temp),
		PADDR (startpos),
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (loadpos),
	PREFETCH_FLUSH
	SCR_COPY (sizeof (struct head)),
},{
		0,
		NADDR (header),
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDR (prepare),

},{
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct dsb, smsg),
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDRH (resend_ident),
	SCR_LOAD_REG (scratcha, 0x80),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (lastmsg),
},{
	SCR_COPY (4),
		NADDR (header.savep),
		RADDR (temp),
	SCR_COPY (4),
		NADDR (header.status),
		RADDR (scr0),
},{
	SCR_LOAD_REG (scratcha, NOP),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
#if 0
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgin),
#endif
	SCR_JUMP ^ IFFALSE (WHEN (SCR_COMMAND)),
		PADDR (dispatch),

},{
	SCR_MOVE_TBL ^ SCR_COMMAND,
		offsetof (struct dsb, cmd),
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,

},{
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR (msg_in),

	SCR_RETURN ^ IFTRUE (IF (SCR_DATA_OUT)),
		0,
	SCR_JUMPR ^ IFFALSE (IF (SCR_DATA_IN)),
		20,
	SCR_COPY (4),
		RADDR (scratcha),
		RADDR (scratcha),
	SCR_RETURN,
 		0,
	SCR_JUMP ^ IFTRUE (IF (SCR_STATUS)),
		PADDR (status),
	SCR_JUMP ^ IFTRUE (IF (SCR_COMMAND)),
		PADDR (command),
	SCR_JUMP ^ IFTRUE (IF (SCR_MSG_OUT)),
		PADDR (msg_out),
	SCR_LOAD_REG (scratcha, XE_BAD_PHASE),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (xerr_st),
	SCR_JUMPR ^ IFFALSE (IF (SCR_ILG_OUT)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_ILG_OUT,
		NADDR (scratch),
	SCR_JUMPR ^ IFFALSE (IF (SCR_ILG_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_ILG_IN,
		NADDR (scratch),
	SCR_JUMP,
		PADDR (dispatch),

},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR (dispatch),

},{
	SCR_LOAD_REG (scratcha, XE_EXTRA_DATA),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (xerr_st),
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_DATA_OUT)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_DATA_OUT,
		NADDR (scratch),
	SCR_JUMPR ^ IFFALSE (IF (SCR_DATA_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_DATA_IN,
		NADDR (scratch),
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),

},{
	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		NADDR (scratch),
	SCR_TO_REG (SS_REG),
		0,
	SCR_LOAD_REG (HS_REG, HS_COMPLETE),
		0,
	SCR_JUMP,
		PADDR (dispatch),
},{
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[0]),
},{
	SCR_JUMP ^ IFTRUE (DATA (COMMAND_COMPLETE)),
		PADDR (complete),
	SCR_JUMP ^ IFTRUE (DATA (DISCONNECT)),
		PADDR (disconnect),
	SCR_JUMP ^ IFTRUE (DATA (SAVE_POINTERS)),
		PADDR (save_dp),
	SCR_JUMP ^ IFTRUE (DATA (RESTORE_POINTERS)),
		PADDR (restore_dp),
	SCR_JUMP ^ IFTRUE (DATA (EXTENDED_MESSAGE)),
		PADDRH (msg_extended),
	SCR_JUMP ^ IFTRUE (DATA (NOP)),
		PADDR (clrack),
	SCR_JUMP ^ IFTRUE (DATA (MESSAGE_REJECT)),
		PADDRH (msg_reject),
	SCR_JUMP ^ IFTRUE (DATA (IGNORE_WIDE_RESIDUE)),
		PADDRH (msg_ign_residue),
},{
	SCR_INT,
		SIR_REJECT_SENT,
	SCR_LOAD_REG (scratcha, MESSAGE_REJECT),
		0,
},{
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_SET (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR (clrack),
},{
	SCR_FROM_REG (dsa),
		0,
	SCR_JUMP ^ IFTRUE (DATA (0xff)),
		PADDR (start),
	SCR_JUMP,
		PADDR (cleanup_ok),

},{
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.lastp),
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
},{
	SCR_COPY (4),
		RADDR (scr0),
		NADDR (header.status),
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (cleanup0),
	PREFETCH_FLUSH
	SCR_COPY (sizeof (struct head)),
		NADDR (header),
},{
		0,
},{
	SCR_FROM_REG (HS_REG),
		0,
	SCR_JUMP ^ IFTRUE (MASK (0, (HS_DONEMASK|HS_SKIPMASK))),
		PADDR(start),
	SCR_FROM_REG (SS_REG),
		0,
	SCR_CALL ^ IFFALSE (DATA (S_GOOD)),
		PADDRH (bad_status),

#ifndef	SCSI_NCR_CCB_DONE_SUPPORT

	SCR_INT,
		SIR_INTFLY,
	SCR_JUMP,
		PADDR(start),

#else	

	SCR_JUMP,
},{
		PADDRH (done_queue),
},{
	SCR_INT,
		SIR_DONE_OVERFLOW,
},{
	SCR_INT,
		SIR_INTFLY,
	SCR_COPY (4),
		RADDR (temp),
		PADDR (done_pos),
	SCR_JUMP,
		PADDR (start),

#endif	

},{
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.savep),
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR (dispatch),
},{
	SCR_COPY (4),
		NADDR (header.savep),
		RADDR (temp),
	SCR_JUMP,
		PADDR (clrack),

},{
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),
		0,
	SCR_JUMP,
		PADDR (cleanup_ok),

},{
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	SCR_JUMP ^ IFTRUE (DATA (ABORT_TASK_SET)),
		PADDRH (msg_out_abort),
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDR (msg_out),
},{
	SCR_LOAD_REG (scratcha, NOP),
		0,
	SCR_COPY (4),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_JUMP,
		PADDR (dispatch),
},{
	SCR_NO_OP,
		0,
},{
	SCR_LOAD_REG (dsa, 0xff),
		0,
	SCR_CLR (SCR_TRG),
		0,
	SCR_LOAD_REG (HS_REG, HS_IN_RESELECT),
		0,
	SCR_WAIT_RESEL,
		PADDR(start),
},{
	SCR_NO_OP,
		0,
	SCR_REG_SFBR (ssid, SCR_AND, 0x8F),
		0,
	SCR_TO_REG (sdid),
		0,
	SCR_JUMP,
		NADDR (jump_tcb),

},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (loadpos1),
	PREFETCH_FLUSH
	SCR_COPY (sizeof (struct head)),

},{
		0,
		NADDR (header),
	SCR_JUMP,
		PADDR (prepare),

},{
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_IN)),
		SIR_RESEL_NO_MSG_IN,
	SCR_FROM_REG (sbdl),
		0,
	SCR_RETURN,
		0,
},{
	SCR_MOVE_ABS (3) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_REG_SFBR (sidl, SCR_SHL, 0),
		0,
	SCR_SFBR_REG (temp, SCR_AND, 0xfc),
		0,
},{
	SCR_COPY_F (4),
		RADDR (temp),
		PADDR (nexus_indirect),
	PREFETCH_FLUSH
	SCR_COPY (4),
},{
		0,
		RADDR (temp),
	SCR_RETURN,
		0,
},{
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_JUMP,
		PADDR (jump_to_nexus),
},{
0
},{
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
},{
0
},{
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
}
};

static	struct scripth scripth0 __initdata = {
{
0
},{
	SCR_JUMP,
		PADDRH(tryloop),

#ifdef SCSI_NCR_CCB_DONE_SUPPORT

},{
0
},{
	SCR_JUMP,
		PADDRH (done_queue),

#endif 
},{

	SCR_CLR (SCR_TRG),
		0,
	SCR_LOAD_REG (HS_REG, HS_SELECTING),
		0,
	SCR_SEL_TBL ^ offsetof (struct dsb, select),
		PADDR (reselect),
	SCR_JUMP,
		PADDR (select2),

},{

	SCR_LOAD_REG (scratcha, HS_ABORTED),
		0,
	SCR_JUMPR,
		8,
},{
	SCR_LOAD_REG (scratcha, 0),
		0,
	SCR_COPY (4),
		RADDR (temp),
		PADDR (startpos),
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDRH (skip2),
	PREFETCH_FLUSH
	SCR_COPY (sizeof (struct head)),
},{
		0,
		NADDR (header),
	SCR_COPY (4),
		NADDR (header.status),
		RADDR (scr0),
	SCR_FROM_REG (scratcha),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (0, HS_DONEMASK)),
		16,
	SCR_REG_REG (HS_REG, SCR_OR, HS_SKIPMASK),
		0,
	SCR_JUMPR,
		8,
	SCR_TO_REG (HS_REG),
		0,
	SCR_LOAD_REG (SS_REG, S_GOOD),
		0,
	SCR_JUMP,
		PADDR (cleanup_ok),

},{
	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN)),
		PADDRH (par_err_other),
	SCR_MOVE_ABS (1) ^ SCR_DATA_IN,
		NADDR (scratch),
	SCR_JUMPR,
		-24,
},{
	SCR_REG_REG (PS_REG, SCR_ADD, 0x01),
		0,
	SCR_JUMP,
		PADDR (dispatch),
},{
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFFALSE (DATA (HS_NEGOTIATE)),
		SIR_REJECT_RECEIVED,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,
	SCR_JUMP,
		PADDR (clrack),

},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	SCR_JUMP ^ IFTRUE (DATA (0)),
		PADDR (clrack),
	SCR_JUMPR ^ IFFALSE (DATA (1)),
		40,
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (WSR, WSR)),
		16,
	SCR_REG_REG (scntl2, SCR_OR, WSR),
		0,
	SCR_JUMP,
		PADDR (clrack),
	SCR_FROM_REG (scratcha),
		0,
	SCR_INT,
		SIR_IGN_RESIDUE,
	SCR_JUMP,
		PADDR (clrack),

},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	SCR_JUMP ^ IFTRUE (DATA (3)),
		PADDRH (msg_ext_3),
	SCR_JUMP ^ IFFALSE (DATA (2)),
		PADDR (msg_bad),
},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[2]),
	SCR_JUMP ^ IFTRUE (DATA (EXTENDED_WDTR)),
		PADDRH (msg_wdtr),
	SCR_JUMP,
		PADDR (msg_bad)
},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[3]),
	SCR_INT,
		SIR_NEGO_WIDE,
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),

},{
	SCR_MOVE_ABS (4) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	SCR_JUMP,
		PADDR (msg_out_done),

},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[2]),
	SCR_JUMP ^ IFTRUE (DATA (EXTENDED_SDTR)),
		PADDRH (msg_sdtr),
	SCR_JUMP,
		PADDR (msg_bad)

},{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	SCR_MOVE_ABS (2) ^ SCR_MSG_IN,
		NADDR (msgin[3]),
	SCR_INT,
		SIR_NEGO_SYNC,
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),

},{
	SCR_MOVE_ABS (5) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	SCR_JUMP,
		PADDR (msg_out_done),

},{
	SCR_INT,
		SIR_NEGO_PROTO,
	SCR_JUMP,
		PADDR (dispatch),

},{
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_LOAD_REG (HS_REG, HS_ABORTED),
		0,
	SCR_JUMP,
		PADDR (cleanup),

},{
0
},{
	SCR_JUMP,
		PADDR (data_in),

},{
0
},{
	SCR_JUMP,
		PADDR (data_out),

},{
	SCR_LOAD_REG (scratcha, ABORT_TASK),
		0,
	SCR_JUMP,
		PADDRH (abort_resel),
},{
	SCR_LOAD_REG (scratcha, ABORT_TASK),
		0,
	SCR_JUMP,
		PADDRH (abort_resel),
},{
	SCR_LOAD_REG (scratcha, ABORT_TASK_SET),
		0,
},{
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_JUMP,
		PADDR (start),
},{
	SCR_SET (SCR_ATN), 
		0,         
	SCR_JUMP,
		PADDR (send_ident),
},{
	SCR_CLR (SCR_ATN),
		0,
	SCR_JUMP,
},{
		0,
},{
	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN)),
		PADDR (dispatch),
	SCR_MOVE_TBL ^ SCR_DATA_IN,
		offsetof (struct dsb, sense),
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
},{
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_DATA_OUT)),
		32,
	SCR_COPY (4),
		NADDR (header.lastp),
		NADDR (header.savep),

	SCR_COPY (4),
		NADDR (header.savep),
		RADDR (temp),
	SCR_RETURN,
		0,
	SCR_COPY (4),
		NADDR (header.wlastp),
		NADDR (header.lastp),
	SCR_COPY (4),
		NADDR (header.wgoalp),
		NADDR (header.goalp),
	SCR_JUMPR,
		-64,
},{
	SCR_JUMPR ^ IFTRUE (MASK (0x80, 0x80)),
		16,
	SCR_INT,
		SIR_RESEL_NO_IDENTIFY,
	SCR_JUMP,
		PADDRH (reset),
	SCR_INT,
		SIR_RESEL_BAD_LUN,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_JUMP,
		PADDRH (abort),
},{
	SCR_INT,
		SIR_RESEL_BAD_I_T_L,
	SCR_JUMP,
		PADDRH (abort),
},{
	SCR_INT,
		SIR_RESEL_BAD_I_T_L_Q,
	SCR_JUMP,
		PADDRH (aborttag),
},{
	SCR_INT,
		SIR_RESEL_BAD_TARGET,
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_JUMP,
		PADDRH (reset),
},{
	SCR_INT ^ IFTRUE (DATA (S_QUEUE_FULL)),
		SIR_BAD_STATUS,
	SCR_INT ^ IFTRUE (DATA (S_CHECK_COND)),
		SIR_BAD_STATUS,
	SCR_INT ^ IFTRUE (DATA (S_TERMINATED)),
		SIR_BAD_STATUS,
	SCR_RETURN,
		0,
},{
	SCR_COPY_F (4),
		RADDR (scratcha),
		PADDRH (start_ram0),
	PREFETCH_FLUSH
	SCR_COPY (sizeof (struct script)),
},{
		0,
		PADDR (start),
	SCR_JUMP,
		PADDR (start),
},{
	SCR_COPY (4),
		RADDR (temp),
		PADDR (startpos),
	SCR_JUMP,
		PADDR (start),
},{
	SCR_RETURN,
		0,
},{
	SCR_COPY (4),
		NADDR(ncr_cache),
		RADDR (scratcha),
	SCR_COPY (4),
		RADDR (temp),
		NADDR(ncr_cache),
	SCR_COPY (4),
		NADDR(ncr_cache),
		RADDR (temp),
},{
	SCR_INT,
		99,
}
};


void __init ncr_script_fill (struct script * scr, struct scripth * scrh)
{
	int	i;
	ncrcmd	*p;

	p = scrh->tryloop;
	for (i=0; i<MAX_START; i++) {
		*p++ =SCR_CALL;
		*p++ =PADDR (idle);
	}

	BUG_ON((u_long)p != (u_long)&scrh->tryloop + sizeof (scrh->tryloop));

#ifdef SCSI_NCR_CCB_DONE_SUPPORT

	p = scrh->done_queue;
	for (i = 0; i<MAX_DONE; i++) {
		*p++ =SCR_COPY (sizeof(struct ccb *));
		*p++ =NADDR (header.cp);
		*p++ =NADDR (ccb_done[i]);
		*p++ =SCR_CALL;
		*p++ =PADDR (done_end);
	}

	BUG_ON((u_long)p != (u_long)&scrh->done_queue+sizeof(scrh->done_queue));

#endif 

	p = scrh->hdata_in;
	for (i=0; i<MAX_SCATTERH; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_IN;
		*p++ =offsetof (struct dsb, data[i]);
	}

	BUG_ON((u_long)p != (u_long)&scrh->hdata_in + sizeof (scrh->hdata_in));

	p = scr->data_in;
	for (i=MAX_SCATTERH; i<MAX_SCATTERH+MAX_SCATTERL; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_IN;
		*p++ =offsetof (struct dsb, data[i]);
	}

	BUG_ON((u_long)p != (u_long)&scr->data_in + sizeof (scr->data_in));

	p = scrh->hdata_out;
	for (i=0; i<MAX_SCATTERH; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_OUT;
		*p++ =offsetof (struct dsb, data[i]);
	}

	BUG_ON((u_long)p != (u_long)&scrh->hdata_out + sizeof (scrh->hdata_out));

	p = scr->data_out;
	for (i=MAX_SCATTERH; i<MAX_SCATTERH+MAX_SCATTERL; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_OUT;
		*p++ =offsetof (struct dsb, data[i]);
	}

	BUG_ON((u_long) p != (u_long)&scr->data_out + sizeof (scr->data_out));
}


static void __init 
ncr_script_copy_and_bind (struct ncb *np, ncrcmd *src, ncrcmd *dst, int len)
{
	ncrcmd  opcode, new, old, tmp1, tmp2;
	ncrcmd	*start, *end;
	int relocs;
	int opchanged = 0;

	start = src;
	end = src + len/4;

	while (src < end) {

		opcode = *src++;
		*dst++ = cpu_to_scr(opcode);


		if (opcode == 0) {
			printk (KERN_ERR "%s: ERROR0 IN SCRIPT at %d.\n",
				ncr_name(np), (int) (src-start-1));
			mdelay(1000);
		}

		if (DEBUG_FLAGS & DEBUG_SCRIPT)
			printk (KERN_DEBUG "%p:  <%x>\n",
				(src-1), (unsigned)opcode);

		switch (opcode >> 28) {

		case 0xc:
			relocs = 2;
			tmp1 = src[0];
#ifdef	RELOC_KVAR
			if ((tmp1 & RELOC_MASK) == RELOC_KVAR)
				tmp1 = 0;
#endif
			tmp2 = src[1];
#ifdef	RELOC_KVAR
			if ((tmp2 & RELOC_MASK) == RELOC_KVAR)
				tmp2 = 0;
#endif
			if ((tmp1 ^ tmp2) & 3) {
				printk (KERN_ERR"%s: ERROR1 IN SCRIPT at %d.\n",
					ncr_name(np), (int) (src-start-1));
				mdelay(1000);
			}
			if ((opcode & SCR_NO_FLUSH) && !(np->features & FE_PFEN)) {
				dst[-1] = cpu_to_scr(opcode & ~SCR_NO_FLUSH);
				++opchanged;
			}
			break;

		case 0x0:
			relocs = 1;
			break;

		case 0x8:
			if (opcode & 0x00800000)
				relocs = 0;
			else
				relocs = 1;
			break;

		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
			relocs = 1;
			break;

		default:
			relocs = 0;
			break;
		}

		if (relocs) {
			while (relocs--) {
				old = *src++;

				switch (old & RELOC_MASK) {
				case RELOC_REGISTER:
					new = (old & ~RELOC_MASK) + np->paddr;
					break;
				case RELOC_LABEL:
					new = (old & ~RELOC_MASK) + np->p_script;
					break;
				case RELOC_LABELH:
					new = (old & ~RELOC_MASK) + np->p_scripth;
					break;
				case RELOC_SOFTC:
					new = (old & ~RELOC_MASK) + np->p_ncb;
					break;
#ifdef	RELOC_KVAR
				case RELOC_KVAR:
					if (((old & ~RELOC_MASK) <
					     SCRIPT_KVAR_FIRST) ||
					    ((old & ~RELOC_MASK) >
					     SCRIPT_KVAR_LAST))
						panic("ncr KVAR out of range");
					new = vtophys(script_kvars[old &
					    ~RELOC_MASK]);
					break;
#endif
				case 0:
					
					if (old == 0) {
						new = old;
						break;
					}
					
				default:
					panic("ncr_script_copy_and_bind: weird relocation %x\n", old);
					break;
				}

				*dst++ = cpu_to_scr(new);
			}
		} else
			*dst++ = cpu_to_scr(*src++);

	}
}


struct host_data {
     struct ncb *ncb;
};

#define PRINT_ADDR(cmd, arg...) dev_info(&cmd->device->sdev_gendev , ## arg)

static void ncr_print_msg(struct ccb *cp, char *label, u_char *msg)
{
	PRINT_ADDR(cp->cmd, "%s: ", label);

	spi_print_msg(msg);
	printk("\n");
}


#define _5M 5000000
static u_long div_10M[] =
	{2*_5M, 3*_5M, 4*_5M, 6*_5M, 8*_5M, 12*_5M, 16*_5M};



#define burst_length(bc) (!(bc))? 0 : 1 << (bc)

#define burst_code(dmode, ctest0) \
	(ctest0) & 0x80 ? 0 : (((dmode) & 0xc0) >> 6) + 1

static inline void ncr_init_burst(struct ncb *np, u_char bc)
{
	u_char *be = &np->rv_ctest0;
	*be		&= ~0x80;
	np->rv_dmode	&= ~(0x3 << 6);
	np->rv_ctest5	&= ~0x4;

	if (!bc) {
		*be		|= 0x80;
	} else {
		--bc;
		np->rv_dmode	|= ((bc & 0x3) << 6);
		np->rv_ctest5	|= (bc & 0x4);
	}
}

static void __init ncr_prepare_setting(struct ncb *np)
{
	u_char	burst_max;
	u_long	period;
	int i;


	np->sv_scntl0	= INB(nc_scntl0) & 0x0a;
	np->sv_scntl3	= INB(nc_scntl3) & 0x07;
	np->sv_dmode	= INB(nc_dmode)  & 0xce;
	np->sv_dcntl	= INB(nc_dcntl)  & 0xa8;
	np->sv_ctest0	= INB(nc_ctest0) & 0x84;
	np->sv_ctest3	= INB(nc_ctest3) & 0x01;
	np->sv_ctest4	= INB(nc_ctest4) & 0x80;
	np->sv_ctest5	= INB(nc_ctest5) & 0x24;
	np->sv_gpcntl	= INB(nc_gpcntl);
	np->sv_stest2	= INB(nc_stest2) & 0x20;
	np->sv_stest4	= INB(nc_stest4);


	np->maxwide	= (np->features & FE_WIDE)? 1 : 0;

	if (np->features & FE_ULTRA)
		np->clock_khz = 80000;
	else
		np->clock_khz = 40000;

	if	(np->features & FE_QUAD)
		np->multiplier	= 4;
	else if	(np->features & FE_DBLR)
		np->multiplier	= 2;
	else
		np->multiplier	= 1;

	if (np->features & FE_VARCLK)
		ncr_getclock(np, np->multiplier);

	i = np->clock_divn - 1;
	while (--i >= 0) {
		if (10ul * SCSI_NCR_MIN_ASYNC * np->clock_khz > div_10M[i]) {
			++i;
			break;
		}
	}
	np->rv_scntl3 = i+1;


	period = (4 * div_10M[0] + np->clock_khz - 1) / np->clock_khz;
	if	(period <= 250)		np->minsync = 10;
	else if	(period <= 303)		np->minsync = 11;
	else if	(period <= 500)		np->minsync = 12;
	else				np->minsync = (period + 40 - 1) / 40;


	if	(np->minsync < 25 && !(np->features & FE_ULTRA))
		np->minsync = 25;


	period = (11 * div_10M[np->clock_divn - 1]) / (4 * np->clock_khz);
	np->maxsync = period > 2540 ? 254 : period / 10;

#if defined SCSI_NCR_TRUST_BIOS_SETTING
	np->rv_scntl0	= np->sv_scntl0;
	np->rv_dmode	= np->sv_dmode;
	np->rv_dcntl	= np->sv_dcntl;
	np->rv_ctest0	= np->sv_ctest0;
	np->rv_ctest3	= np->sv_ctest3;
	np->rv_ctest4	= np->sv_ctest4;
	np->rv_ctest5	= np->sv_ctest5;
	burst_max	= burst_code(np->sv_dmode, np->sv_ctest0);
#else

	burst_max	= driver_setup.burst_max;
	if (burst_max == 255)
		burst_max = burst_code(np->sv_dmode, np->sv_ctest0);
	if (burst_max > 7)
		burst_max = 7;
	if (burst_max > np->maxburst)
		burst_max = np->maxburst;

	if (np->features & FE_ERL)
		np->rv_dmode	|= ERL;		
	if (np->features & FE_BOF)
		np->rv_dmode	|= BOF;		
	if (np->features & FE_ERMP)
		np->rv_dmode	|= ERMP;	
	if (np->features & FE_PFEN)
		np->rv_dcntl	|= PFEN;	
	if (np->features & FE_CLSE)
		np->rv_dcntl	|= CLSE;	
	if (np->features & FE_WRIE)
		np->rv_ctest3	|= WRIE;	
	if (np->features & FE_DFS)
		np->rv_ctest5	|= DFS;		
	if (np->features & FE_MUX)
		np->rv_ctest4	|= MUX;		
	if (np->features & FE_EA)
		np->rv_dcntl	|= EA;		
	if (np->features & FE_EHP)
		np->rv_ctest0	|= EHP;		

	if (driver_setup.master_parity)
		np->rv_ctest4	|= MPEE;	
	if (driver_setup.scsi_parity)
		np->rv_scntl0	|= 0x0a;	

	if (np->myaddr == 255) {
		np->myaddr = INB(nc_scid) & 0x07;
		if (!np->myaddr)
			np->myaddr = SCSI_NCR_MYADDR;
	}

#endif 

	ncr_init_burst(np, burst_max);

	np->scsi_mode = SMODE_SE;
	if (np->features & FE_DIFF) {
		switch(driver_setup.diff_support) {
		case 4:	
			if (np->sv_scntl3) {
				if (np->sv_stest2 & 0x20)
					np->scsi_mode = SMODE_HVD;
				break;
			}
		case 3:	
			if (INB(nc_gpreg) & 0x08)
				break;
		case 2:	
			np->scsi_mode = SMODE_HVD;
		case 1:	
			if (np->sv_stest2 & 0x20)
				np->scsi_mode = SMODE_HVD;
			break;
		default:	
			break;
		}
	}
	if (np->scsi_mode == SMODE_HVD)
		np->rv_stest2 |= 0x20;

	if ((driver_setup.led_pin) &&
	    !(np->features & FE_LEDC) && !(np->sv_gpcntl & 0x01))
		np->features |= FE_LED0;

	switch(driver_setup.irqm & 3) {
	case 2:
		np->rv_dcntl	|= IRQM;
		break;
	case 1:
		np->rv_dcntl	|= (np->sv_dcntl & IRQM);
		break;
	default:
		break;
	}

	for (i = 0 ; i < MAX_TARGET ; i++) {
		struct tcb *tp = &np->target[i];

		tp->usrsync = driver_setup.default_sync;
		tp->usrwide = driver_setup.max_wide;
		tp->usrtags = MAX_TAGS;
		tp->period = 0xffff;
		if (!driver_setup.disconnection)
			np->target[i].usrflag = UF_NODISC;
	}


	printk(KERN_INFO "%s: ID %d, Fast-%d%s%s\n", ncr_name(np),
		np->myaddr,
		np->minsync < 12 ? 40 : (np->minsync < 25 ? 20 : 10),
		(np->rv_scntl0 & 0xa)	? ", Parity Checking"	: ", NO Parity",
		(np->rv_stest2 & 0x20)	? ", Differential"	: "");

	if (bootverbose > 1) {
		printk (KERN_INFO "%s: initial SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			ncr_name(np), np->sv_scntl3, np->sv_dmode, np->sv_dcntl,
			np->sv_ctest3, np->sv_ctest4, np->sv_ctest5);

		printk (KERN_INFO "%s: final   SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			ncr_name(np), np->rv_scntl3, np->rv_dmode, np->rv_dcntl,
			np->rv_ctest3, np->rv_ctest4, np->rv_ctest5);
	}

	if (bootverbose && np->paddr2)
		printk (KERN_INFO "%s: on-chip RAM at 0x%lx\n",
			ncr_name(np), np->paddr2);
}

static inline void ncr_queue_done_cmd(struct ncb *np, struct scsi_cmnd *cmd)
{
	unmap_scsi_data(np, cmd);
	cmd->host_scribble = (char *) np->done_list;
	np->done_list = cmd;
}

static inline void ncr_flush_done_cmds(struct scsi_cmnd *lcmd)
{
	struct scsi_cmnd *cmd;

	while (lcmd) {
		cmd = lcmd;
		lcmd = (struct scsi_cmnd *) cmd->host_scribble;
		cmd->scsi_done(cmd);
	}
}



static int ncr_prepare_nego(struct ncb *np, struct ccb *cp, u_char *msgptr)
{
	struct tcb *tp = &np->target[cp->target];
	int msglen = 0;
	int nego = 0;
	struct scsi_target *starget = tp->starget;

	
	if (!tp->widedone) {
		if (spi_support_wide(starget)) {
			nego = NS_WIDE;
		} else
			tp->widedone=1;
	}

	
	if (!nego && !tp->period) {
		if (spi_support_sync(starget)) {
			nego = NS_SYNC;
		} else {
			tp->period  =0xffff;
			dev_info(&starget->dev, "target did not report SYNC.\n");
		}
	}

	switch (nego) {
	case NS_SYNC:
		msglen += spi_populate_sync_msg(msgptr + msglen,
				tp->maxoffs ? tp->minsync : 0, tp->maxoffs);
		break;
	case NS_WIDE:
		msglen += spi_populate_width_msg(msgptr + msglen, tp->usrwide);
		break;
	}

	cp->nego_status = nego;

	if (nego) {
		tp->nego_cp = cp;
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			ncr_print_msg(cp, nego == NS_WIDE ?
					  "wide msgout":"sync_msgout", msgptr);
		}
	}

	return msglen;
}



static int ncr_queue_command (struct ncb *np, struct scsi_cmnd *cmd)
{
	struct scsi_device *sdev = cmd->device;
	struct tcb *tp = &np->target[sdev->id];
	struct lcb *lp = tp->lp[sdev->lun];
	struct ccb *cp;

	int	segments;
	u_char	idmsg, *msgptr;
	u32	msglen;
	int	direction;
	u32	lastp, goalp;

	if ((sdev->id == np->myaddr	  ) ||
		(sdev->id >= MAX_TARGET) ||
		(sdev->lun    >= MAX_LUN   )) {
		return(DID_BAD_TARGET);
	}

	if ((cmd->cmnd[0] == 0 || cmd->cmnd[0] == 0x12) && 
	    (tp->usrflag & UF_NOSCAN)) {
		tp->usrflag &= ~UF_NOSCAN;
		return DID_BAD_TARGET;
	}

	if (DEBUG_FLAGS & DEBUG_TINY) {
		PRINT_ADDR(cmd, "CMD=%x ", cmd->cmnd[0]);
	}

	if (np->settle_time && cmd->request->timeout >= HZ) {
		u_long tlimit = jiffies + cmd->request->timeout - HZ;
		if (time_after(np->settle_time, tlimit))
			np->settle_time = tlimit;
	}

	if (np->settle_time || !(cp=ncr_get_ccb (np, cmd))) {
		insert_into_waiting_list(np, cmd);
		return(DID_OK);
	}
	cp->cmd = cmd;


	idmsg = IDENTIFY(0, sdev->lun);

	if (cp ->tag != NO_TAG ||
		(cp != np->ccb && np->disc && !(tp->usrflag & UF_NODISC)))
		idmsg |= 0x40;

	msgptr = cp->scsi_smsg;
	msglen = 0;
	msgptr[msglen++] = idmsg;

	if (cp->tag != NO_TAG) {
		char order = np->order;

		if (lp && time_after(jiffies, lp->tags_stime)) {
			if (lp->tags_smap) {
				order = ORDERED_QUEUE_TAG;
				if ((DEBUG_FLAGS & DEBUG_TAGS)||bootverbose>2){ 
					PRINT_ADDR(cmd,
						"ordered tag forced.\n");
				}
			}
			lp->tags_stime = jiffies + 3*HZ;
			lp->tags_smap = lp->tags_umap;
		}

		if (order == 0) {
			switch (cmd->cmnd[0]) {
			case 0x08:  
			case 0x28:  
			case 0xa8:  
				order = SIMPLE_QUEUE_TAG;
				break;
			default:
				order = ORDERED_QUEUE_TAG;
			}
		}
		msgptr[msglen++] = order;
		msgptr[msglen++] = (cp->tag << 1) + 1;
	}


	direction = cmd->sc_data_direction;
	if (direction != DMA_NONE) {
		segments = ncr_scatter(np, cp, cp->cmd);
		if (segments < 0) {
			ncr_free_ccb(np, cp);
			return(DID_ERROR);
		}
	}
	else {
		cp->data_len = 0;
		segments = 0;
	}


	cp->nego_status = 0;

	if ((!tp->widedone || !tp->period) && !tp->nego_cp && lp) {
		msglen += ncr_prepare_nego (np, cp, msgptr + msglen);
	}

	if (!cp->data_len)
		direction = DMA_NONE;

	switch(direction) {
	case DMA_BIDIRECTIONAL:
	case DMA_TO_DEVICE:
		goalp = NCB_SCRIPT_PHYS (np, data_out2) + 8;
		if (segments <= MAX_SCATTERL)
			lastp = goalp - 8 - (segments * 16);
		else {
			lastp = NCB_SCRIPTH_PHYS (np, hdata_out2);
			lastp -= (segments - MAX_SCATTERL) * 16;
		}
		if (direction != DMA_BIDIRECTIONAL)
			break;
		cp->phys.header.wgoalp	= cpu_to_scr(goalp);
		cp->phys.header.wlastp	= cpu_to_scr(lastp);
		
	case DMA_FROM_DEVICE:
		goalp = NCB_SCRIPT_PHYS (np, data_in2) + 8;
		if (segments <= MAX_SCATTERL)
			lastp = goalp - 8 - (segments * 16);
		else {
			lastp = NCB_SCRIPTH_PHYS (np, hdata_in2);
			lastp -= (segments - MAX_SCATTERL) * 16;
		}
		break;
	default:
	case DMA_NONE:
		lastp = goalp = NCB_SCRIPT_PHYS (np, no_data);
		break;
	}

	cp->phys.header.lastp = cpu_to_scr(lastp);
	cp->phys.header.goalp = cpu_to_scr(goalp);

	if (direction == DMA_BIDIRECTIONAL)
		cp->phys.header.savep = 
			cpu_to_scr(NCB_SCRIPTH_PHYS (np, data_io));
	else
		cp->phys.header.savep= cpu_to_scr(lastp);

	cp->startp = cp->phys.header.savep;


	cp->start.schedule.l_paddr   = cpu_to_scr(NCB_SCRIPT_PHYS (np, select));
	cp->restart.schedule.l_paddr = cpu_to_scr(NCB_SCRIPT_PHYS (np, resel_dsa));
	cp->phys.select.sel_id		= sdev_id(sdev);
	cp->phys.select.sel_scntl3	= tp->wval;
	cp->phys.select.sel_sxfer	= tp->sval;
	cp->phys.smsg.addr		= cpu_to_scr(CCB_PHYS (cp, scsi_smsg));
	cp->phys.smsg.size		= cpu_to_scr(msglen);

	memcpy(cp->cdb_buf, cmd->cmnd, min_t(int, cmd->cmd_len, sizeof(cp->cdb_buf)));
	cp->phys.cmd.addr		= cpu_to_scr(CCB_PHYS (cp, cdb_buf[0]));
	cp->phys.cmd.size		= cpu_to_scr(cmd->cmd_len);

	cp->actualquirks		= 0;
	cp->host_status			= cp->nego_status ? HS_NEGOTIATE : HS_BUSY;
	cp->scsi_status			= S_ILLEGAL;
	cp->parity_status		= 0;

	cp->xerr_status			= XE_OK;
#if 0
	cp->sync_status			= tp->sval;
	cp->wide_status			= tp->wval;
#endif


	
	cp->magic		= CCB_MAGIC;

	cp->auto_sense = 0;
	if (lp)
		ncr_start_next_ccb(np, lp, 2);
	else
		ncr_put_start_queue(np, cp);

	

	return DID_OK;
}



static void ncr_start_next_ccb(struct ncb *np, struct lcb *lp, int maxn)
{
	struct list_head *qp;
	struct ccb *cp;

	if (lp->held_ccb)
		return;

	while (maxn-- && lp->queuedccbs < lp->queuedepth) {
		qp = ncr_list_pop(&lp->wait_ccbq);
		if (!qp)
			break;
		++lp->queuedccbs;
		cp = list_entry(qp, struct ccb, link_ccbq);
		list_add_tail(qp, &lp->busy_ccbq);
		lp->jump_ccb[cp->tag == NO_TAG ? 0 : cp->tag] =
			cpu_to_scr(CCB_PHYS (cp, restart));
		ncr_put_start_queue(np, cp);
	}
}

static void ncr_put_start_queue(struct ncb *np, struct ccb *cp)
{
	u16	qidx;

	if (!np->squeueput) np->squeueput = 1;
	qidx = np->squeueput + 2;
	if (qidx >= MAX_START + MAX_START) qidx = 1;

	np->scripth->tryloop [qidx] = cpu_to_scr(NCB_SCRIPT_PHYS (np, idle));
	MEMORY_BARRIER();
	np->scripth->tryloop [np->squeueput] = cpu_to_scr(CCB_PHYS (cp, start));

	np->squeueput = qidx;
	++np->queuedccbs;
	cp->queued = 1;

	if (DEBUG_FLAGS & DEBUG_QUEUE)
		printk ("%s: queuepos=%d.\n", ncr_name (np), np->squeueput);

	MEMORY_BARRIER();
	OUTB (nc_istat, SIGP);
}


static int ncr_reset_scsi_bus(struct ncb *np, int enab_int, int settle_delay)
{
	u32 term;
	int retv = 0;

	np->settle_time	= jiffies + settle_delay * HZ;

	if (bootverbose > 1)
		printk("%s: resetting, "
			"command processing suspended for %d seconds\n",
			ncr_name(np), settle_delay);

	ncr_chip_reset(np, 100);
	udelay(2000);	
	if (enab_int)
		OUTW (nc_sien, RST);
	OUTB (nc_stest3, TE);
	OUTB (nc_scntl1, CRST);
	udelay(200);

	if (!driver_setup.bus_check)
		goto out;

	term =	INB(nc_sstat0);
	term =	((term & 2) << 7) + ((term & 1) << 17);	
	term |= ((INB(nc_sstat2) & 0x01) << 26) |	
		((INW(nc_sbdl) & 0xff)   << 9)  |	
		((INW(nc_sbdl) & 0xff00) << 10) |	
		INB(nc_sbcl);	

	if (!(np->features & FE_WIDE))
		term &= 0x3ffff;

	if (term != (2<<7)) {
		printk("%s: suspicious SCSI data while resetting the BUS.\n",
			ncr_name(np));
		printk("%s: %sdp0,d7-0,rst,req,ack,bsy,sel,atn,msg,c/d,i/o = "
			"0x%lx, expecting 0x%lx\n",
			ncr_name(np),
			(np->features & FE_WIDE) ? "dp1,d15-8," : "",
			(u_long)term, (u_long)(2<<7));
		if (driver_setup.bus_check == 1)
			retv = 1;
	}
out:
	OUTB (nc_scntl1, 0);
	return retv;
}

static void ncr_start_reset(struct ncb *np)
{
	if (!np->settle_time) {
		ncr_reset_scsi_bus(np, 1, driver_setup.settle_delay);
 	}
}
 
static int ncr_reset_bus (struct ncb *np, struct scsi_cmnd *cmd, int sync_reset)
{
	struct ccb *cp;
	int found;

	if (np->settle_time) {
		return FAILED;
	}
	ncr_start_reset(np);
	for (found=0, cp=np->ccb; cp; cp=cp->link_ccb) {
		if (cp->host_status == HS_IDLE) continue;
		if (cp->cmd == cmd) {
			found = 1;
			break;
		}
	}
	if (!found && retrieve_from_waiting_list(0, np, cmd))
		found = 1;
	reset_waiting_list(np);
	ncr_wakeup(np, HS_RESET);
	if (!found && sync_reset && !retrieve_from_waiting_list(0, np, cmd)) {
		cmd->result = ScsiResult(DID_RESET, 0);
		ncr_queue_done_cmd(np, cmd);
	}

	return SUCCESS;
}

#if 0 
static int ncr_abort_command (struct ncb *np, struct scsi_cmnd *cmd)
{
	struct ccb *cp;
	int found;
	int retv;

	if (remove_from_waiting_list(np, cmd)) {
		cmd->result = ScsiResult(DID_ABORT, 0);
		ncr_queue_done_cmd(np, cmd);
		return SCSI_ABORT_SUCCESS;
	}

	for (found=0, cp=np->ccb; cp; cp=cp->link_ccb) {
		if (cp->host_status == HS_IDLE) continue;
		if (cp->cmd == cmd) {
			found = 1;
			break;
		}
	}

	if (!found) {
		return SCSI_ABORT_NOT_RUNNING;
	}

	if (np->settle_time) {
		return SCSI_ABORT_SNOOZE;
	}


	switch(cp->host_status) {
	case HS_BUSY:
	case HS_NEGOTIATE:
		printk ("%s: abort ccb=%p (cancel)\n", ncr_name (np), cp);
			cp->start.schedule.l_paddr =
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, cancel));
		retv = SCSI_ABORT_PENDING;
		break;
	case HS_DISCONNECT:
		cp->restart.schedule.l_paddr =
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, abort));
		retv = SCSI_ABORT_PENDING;
		break;
	default:
		retv = SCSI_ABORT_NOT_RUNNING;
		break;

	}

	OUTB (nc_istat, SIGP);

	return retv;
}
#endif

static void ncr_detach(struct ncb *np)
{
	struct ccb *cp;
	struct tcb *tp;
	struct lcb *lp;
	int target, lun;
	int i;
	char inst_name[16];

	
	strlcpy(inst_name, ncr_name(np), sizeof(inst_name));

	printk("%s: releasing host resources\n", ncr_name(np));


#ifdef DEBUG_NCR53C8XX
	printk("%s: stopping the timer\n", ncr_name(np));
#endif
	np->release_stage = 1;
	for (i = 50 ; i && np->release_stage != 2 ; i--)
		mdelay(100);
	if (np->release_stage != 2)
		printk("%s: the timer seems to be already stopped\n", ncr_name(np));
	else np->release_stage = 2;


#ifdef DEBUG_NCR53C8XX
	printk("%s: disabling chip interrupts\n", ncr_name(np));
#endif
	OUTW (nc_sien , 0);
	OUTB (nc_dien , 0);


	printk("%s: resetting chip\n", ncr_name(np));
	ncr_chip_reset(np, 100);

	OUTB(nc_dmode,	np->sv_dmode);
	OUTB(nc_dcntl,	np->sv_dcntl);
	OUTB(nc_ctest0,	np->sv_ctest0);
	OUTB(nc_ctest3,	np->sv_ctest3);
	OUTB(nc_ctest4,	np->sv_ctest4);
	OUTB(nc_ctest5,	np->sv_ctest5);
	OUTB(nc_gpcntl,	np->sv_gpcntl);
	OUTB(nc_stest2,	np->sv_stest2);

	ncr_selectclock(np, np->sv_scntl3);


	while ((cp=np->ccb->link_ccb) != NULL) {
		np->ccb->link_ccb = cp->link_ccb;
		if (cp->host_status) {
		printk("%s: shall free an active ccb (host_status=%d)\n",
			ncr_name(np), cp->host_status);
		}
#ifdef DEBUG_NCR53C8XX
	printk("%s: freeing ccb (%lx)\n", ncr_name(np), (u_long) cp);
#endif
		m_free_dma(cp, sizeof(*cp), "CCB");
	}

	

	for (target = 0; target < MAX_TARGET ; target++) {
		tp=&np->target[target];
		for (lun = 0 ; lun < MAX_LUN ; lun++) {
			lp = tp->lp[lun];
			if (lp) {
#ifdef DEBUG_NCR53C8XX
	printk("%s: freeing lp (%lx)\n", ncr_name(np), (u_long) lp);
#endif
				if (lp->jump_ccb != &lp->jump_ccb_0)
					m_free_dma(lp->jump_ccb,256,"JUMP_CCB");
				m_free_dma(lp, sizeof(*lp), "LCB");
			}
		}
	}

	if (np->scripth0)
		m_free_dma(np->scripth0, sizeof(struct scripth), "SCRIPTH");
	if (np->script0)
		m_free_dma(np->script0, sizeof(struct script), "SCRIPT");
	if (np->ccb)
		m_free_dma(np->ccb, sizeof(struct ccb), "CCB");
	m_free_dma(np, sizeof(struct ncb), "NCB");

	printk("%s: host resources successfully released\n", inst_name);
}


void ncr_complete (struct ncb *np, struct ccb *cp)
{
	struct scsi_cmnd *cmd;
	struct tcb *tp;
	struct lcb *lp;


	if (!cp || cp->magic != CCB_MAGIC || !cp->cmd)
		return;


	if (DEBUG_FLAGS & DEBUG_TINY)
		printk ("CCB=%lx STAT=%x/%x\n", (unsigned long)cp,
			cp->host_status,cp->scsi_status);


	cmd = cp->cmd;
	cp->cmd = NULL;
	tp = &np->target[cmd->device->id];
	lp = tp->lp[cmd->device->lun];


	if (cp == tp->nego_cp)
		tp->nego_cp = NULL;

	if (cp->auto_sense) {
		cp->scsi_status = cp->auto_sense;
	}


	if (lp && lp->held_ccb) {
		if (cp == lp->held_ccb) {
			list_splice_init(&lp->skip_ccbq, &lp->wait_ccbq);
			lp->held_ccb = NULL;
		}
	}


	if (cp->parity_status > 1) {
		PRINT_ADDR(cmd, "%d parity error(s).\n",cp->parity_status);
	}


	if (cp->xerr_status != XE_OK) {
		switch (cp->xerr_status) {
		case XE_EXTRA_DATA:
			PRINT_ADDR(cmd, "extraneous data discarded.\n");
			break;
		case XE_BAD_PHASE:
			PRINT_ADDR(cmd, "invalid scsi phase (4/5).\n");
			break;
		default:
			PRINT_ADDR(cmd, "extended error %d.\n",
					cp->xerr_status);
			break;
		}
		if (cp->host_status==HS_COMPLETE)
			cp->host_status = HS_FAIL;
	}

	if (DEBUG_FLAGS & (DEBUG_RESULT|DEBUG_TINY)) {
		if (cp->host_status!=HS_COMPLETE || cp->scsi_status!=S_GOOD) {
			PRINT_ADDR(cmd, "ERROR: cmd=%x host_status=%x "
					"scsi_status=%x\n", cmd->cmnd[0],
					cp->host_status, cp->scsi_status);
		}
	}

	if (   (cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_GOOD ||
		    cp->scsi_status == S_COND_MET)) {
		cmd->result = ScsiResult(DID_OK, cp->scsi_status);

		

		if (!lp)
			ncr_alloc_lcb (np, cmd->device->id, cmd->device->lun);

		tp->bytes     += cp->data_len;
		tp->transfers ++;

		if (lp && lp->usetags && lp->numtags < lp->maxtags) {
			++lp->num_good;
			if (lp->num_good >= 1000) {
				lp->num_good = 0;
				++lp->numtags;
				ncr_setup_tags (np, cmd->device);
			}
		}
	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_CHECK_COND)) {
		cmd->result = ScsiResult(DID_OK, S_CHECK_COND);

		memcpy(cmd->sense_buffer, cp->sense_buf,
		       min_t(size_t, SCSI_SENSE_BUFFERSIZE,
			     sizeof(cp->sense_buf)));

		if (DEBUG_FLAGS & (DEBUG_RESULT|DEBUG_TINY)) {
			u_char *p = cmd->sense_buffer;
			int i;
			PRINT_ADDR(cmd, "sense data:");
			for (i=0; i<14; i++) printk (" %x", *p++);
			printk (".\n");
		}
	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_CONFLICT)) {
		cmd->result = ScsiResult(DID_OK, S_CONFLICT);
	
	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_BUSY ||
		    cp->scsi_status == S_QUEUE_FULL)) {

		cmd->result = ScsiResult(DID_OK, cp->scsi_status);

	} else if ((cp->host_status == HS_SEL_TIMEOUT)
		|| (cp->host_status == HS_TIMEOUT)) {

		cmd->result = ScsiResult(DID_TIME_OUT, cp->scsi_status);

	} else if (cp->host_status == HS_RESET) {

		cmd->result = ScsiResult(DID_RESET, cp->scsi_status);

	} else if (cp->host_status == HS_ABORTED) {

		cmd->result = ScsiResult(DID_ABORT, cp->scsi_status);

	} else {

		PRINT_ADDR(cmd, "COMMAND FAILED (%x %x) @%p.\n",
			cp->host_status, cp->scsi_status, cp);

		cmd->result = ScsiResult(DID_ERROR, cp->scsi_status);
	}


	if (tp->usrflag & UF_TRACE) {
		u_char * p;
		int i;
		PRINT_ADDR(cmd, " CMD:");
		p = (u_char*) &cmd->cmnd[0];
		for (i=0; i<cmd->cmd_len; i++) printk (" %x", *p++);

		if (cp->host_status==HS_COMPLETE) {
			switch (cp->scsi_status) {
			case S_GOOD:
				printk ("  GOOD");
				break;
			case S_CHECK_COND:
				printk ("  SENSE:");
				p = (u_char*) &cmd->sense_buffer;
				for (i=0; i<14; i++)
					printk (" %x", *p++);
				break;
			default:
				printk ("  STAT: %x\n", cp->scsi_status);
				break;
			}
		} else printk ("  HOSTERROR: %x", cp->host_status);
		printk ("\n");
	}

	ncr_free_ccb (np, cp);

	if (lp && lp->queuedccbs < lp->queuedepth &&
	    !list_empty(&lp->wait_ccbq))
		ncr_start_next_ccb(np, lp, 2);

	if (np->waiting_list)
		requeue_waiting_list(np);

	ncr_queue_done_cmd(np, cmd);
}


static void ncr_ccb_skipped(struct ncb *np, struct ccb *cp)
{
	struct tcb *tp = &np->target[cp->target];
	struct lcb *lp = tp->lp[cp->lun];

	if (lp && cp != np->ccb) {
		cp->host_status &= ~HS_SKIPMASK;
		cp->start.schedule.l_paddr = 
			cpu_to_scr(NCB_SCRIPT_PHYS (np, select));
		list_move_tail(&cp->link_ccbq, &lp->skip_ccbq);
		if (cp->queued) {
			--lp->queuedccbs;
		}
	}
	if (cp->queued) {
		--np->queuedccbs;
		cp->queued = 0;
	}
}

void ncr_wakeup_done (struct ncb *np)
{
	struct ccb *cp;
#ifdef SCSI_NCR_CCB_DONE_SUPPORT
	int i, j;

	i = np->ccb_done_ic;
	while (1) {
		j = i+1;
		if (j >= MAX_DONE)
			j = 0;

		cp = np->ccb_done[j];
		if (!CCB_DONE_VALID(cp))
			break;

		np->ccb_done[j] = (struct ccb *)CCB_DONE_EMPTY;
		np->scripth->done_queue[5*j + 4] =
				cpu_to_scr(NCB_SCRIPT_PHYS (np, done_plug));
		MEMORY_BARRIER();
		np->scripth->done_queue[5*i + 4] =
				cpu_to_scr(NCB_SCRIPT_PHYS (np, done_end));

		if (cp->host_status & HS_DONEMASK)
			ncr_complete (np, cp);
		else if (cp->host_status & HS_SKIPMASK)
			ncr_ccb_skipped (np, cp);

		i = j;
	}
	np->ccb_done_ic = i;
#else
	cp = np->ccb;
	while (cp) {
		if (cp->host_status & HS_DONEMASK)
			ncr_complete (np, cp);
		else if (cp->host_status & HS_SKIPMASK)
			ncr_ccb_skipped (np, cp);
		cp = cp->link_ccb;
	}
#endif
}

void ncr_wakeup (struct ncb *np, u_long code)
{
	struct ccb *cp = np->ccb;

	while (cp) {
		if (cp->host_status != HS_IDLE) {
			cp->host_status = code;
			ncr_complete (np, cp);
		}
		cp = cp->link_ccb;
	}
}


static void ncr_chip_reset(struct ncb *np, int delay)
{
	OUTB (nc_istat,  SRST);
	udelay(delay);
	OUTB (nc_istat,  0   );

	if (np->features & FE_EHP)
		OUTB (nc_ctest0, EHP);
	if (np->features & FE_MUX)
		OUTB (nc_ctest4, MUX);
}



void ncr_init (struct ncb *np, int reset, char * msg, u_long code)
{
 	int	i;


	if (reset) {
		OUTB (nc_istat,  SRST);
		udelay(100);
	}
	else {
		OUTB (nc_stest3, TE|CSF);
		OUTONB (nc_ctest3, CLF);
	}
 

	if (msg) printk (KERN_INFO "%s: restart (%s).\n", ncr_name (np), msg);

	np->queuedepth = MAX_START - 1;	
	for (i = 1; i < MAX_START + MAX_START; i += 2)
		np->scripth0->tryloop[i] =
				cpu_to_scr(NCB_SCRIPT_PHYS (np, idle));

	np->squeueput = 0;
	np->script0->startpos[0] = cpu_to_scr(NCB_SCRIPTH_PHYS (np, tryloop));

#ifdef SCSI_NCR_CCB_DONE_SUPPORT
	for (i = 0; i < MAX_DONE; i++) {
		np->ccb_done[i] = (struct ccb *)CCB_DONE_EMPTY;
		np->scripth0->done_queue[5*i + 4] =
			cpu_to_scr(NCB_SCRIPT_PHYS (np, done_end));
	}
#endif

	np->script0->done_pos[0] = cpu_to_scr(NCB_SCRIPTH_PHYS (np,done_queue));
	np->ccb_done_ic = MAX_DONE-1;
	np->scripth0->done_queue[5*(MAX_DONE-1) + 4] =
			cpu_to_scr(NCB_SCRIPT_PHYS (np, done_plug));

	ncr_wakeup (np, code);


	ncr_chip_reset(np, 2000);

	OUTB (nc_scntl0, np->rv_scntl0 | 0xc0);
					
	OUTB (nc_scntl1, 0x00);		

	ncr_selectclock(np, np->rv_scntl3);	

	OUTB (nc_scid  , RRE|np->myaddr);	
	OUTW (nc_respid, 1ul<<np->myaddr);	
	OUTB (nc_istat , SIGP	);		
	OUTB (nc_dmode , np->rv_dmode);		
	OUTB (nc_ctest5, np->rv_ctest5);	

	OUTB (nc_dcntl , NOCOM|np->rv_dcntl);	
	OUTB (nc_ctest0, np->rv_ctest0);	
	OUTB (nc_ctest3, np->rv_ctest3);	
	OUTB (nc_ctest4, np->rv_ctest4);	

	OUTB (nc_stest2, EXT|np->rv_stest2);	
	OUTB (nc_stest3, TE);			
	OUTB (nc_stime0, 0x0c	);		


	np->disc = 0;


	if (np->features & FE_LED0) {
		OUTOFFB (nc_gpcntl, 0x01);
	}


	OUTW (nc_sien , STO|HTH|MA|SGE|UDC|RST|PAR);
	OUTB (nc_dien , MDPE|BF|ABRT|SSI|SIR|IID);


	for (i=0;i<MAX_TARGET;i++) {
		struct tcb *tp = &np->target[i];

		tp->sval    = 0;
		tp->wval    = np->rv_scntl3;

		if (tp->usrsync != 255) {
			if (tp->usrsync <= np->maxsync) {
				if (tp->usrsync < np->minsync) {
					tp->usrsync = np->minsync;
				}
			}
			else
				tp->usrsync = 255;
		}

		if (tp->usrwide > np->maxwide)
			tp->usrwide = np->maxwide;

	}

	if (np->paddr2) {
		if (bootverbose)
			printk ("%s: Downloading SCSI SCRIPTS.\n",
				ncr_name(np));
		OUTL (nc_scratcha, vtobus(np->script0));
		OUTL_DSP (NCB_SCRIPTH_PHYS (np, start_ram));
	}
	else
		OUTL_DSP (NCB_SCRIPT_PHYS (np, start));
}


static void ncr_negotiate (struct ncb* np, struct tcb* tp)
{

	u_long minsync = tp->usrsync;


	if (np->scsi_mode && np->scsi_mode == SMODE_SE) {
		if (minsync < 12) minsync = 12;
	}


	if (minsync < np->minsync)
		minsync = np->minsync;


	if (minsync > np->maxsync)
		minsync = 255;

	if (tp->maxoffs > np->maxoffs)
		tp->maxoffs = np->maxoffs;

	tp->minsync = minsync;
	tp->maxoffs = (minsync<255 ? tp->maxoffs : 0);


	tp->period=0;

	tp->widedone=0;
}


static void ncr_getsync(struct ncb *np, u_char sfac, u_char *fakp, u_char *scntl3p)
{
	u_long	clk = np->clock_khz;	
	int	div = np->clock_divn;	
	u_long	fak;			
	u_long	per;			
	u_long	kpc;			

	if	(sfac <= 10)	per = 250;
	else if	(sfac == 11)	per = 303;
	else if	(sfac == 12)	per = 500;
	else			per = 40 * sfac;

	kpc = per * clk;
	while (--div > 0)
		if (kpc >= (div_10M[div] << 2)) break;

	fak = (kpc - 1) / div_10M[div] + 1;

#if 0	

	per = (fak * div_10M[div]) / clk;

	if (div >= 1 && fak < 8) {
		u_long fak2, per2;
		fak2 = (kpc - 1) / div_10M[div-1] + 1;
		per2 = (fak2 * div_10M[div-1]) / clk;
		if (per2 < per && fak2 <= 8) {
			fak = fak2;
			per = per2;
			--div;
		}
	}
#endif

	if (fak < 4) fak = 4;	

	*fakp		= fak - 4;
	*scntl3p	= ((div+1) << 4) + (sfac < 25 ? 0x80 : 0);
}



static void ncr_set_sync_wide_status (struct ncb *np, u_char target)
{
	struct ccb *cp;
	struct tcb *tp = &np->target[target];

	OUTB (nc_sxfer, tp->sval);
	np->sync_st = tp->sval;
	OUTB (nc_scntl3, tp->wval);
	np->wide_st = tp->wval;

	for (cp = np->ccb; cp; cp = cp->link_ccb) {
		if (!cp->cmd) continue;
		if (scmd_id(cp->cmd) != target) continue;
#if 0
		cp->sync_status = tp->sval;
		cp->wide_status = tp->wval;
#endif
		cp->phys.select.sel_scntl3 = tp->wval;
		cp->phys.select.sel_sxfer  = tp->sval;
	}
}


static void ncr_setsync (struct ncb *np, struct ccb *cp, u_char scntl3, u_char sxfer)
{
	struct scsi_cmnd *cmd = cp->cmd;
	struct tcb *tp;
	u_char target = INB (nc_sdid) & 0x0f;
	u_char idiv;

	BUG_ON(target != (scmd_id(cmd) & 0xf));

	tp = &np->target[target];

	if (!scntl3 || !(sxfer & 0x1f))
		scntl3 = np->rv_scntl3;
	scntl3 = (scntl3 & 0xf0) | (tp->wval & EWS) | (np->rv_scntl3 & 0x07);


	idiv = ((scntl3 >> 4) & 0x7);
	if ((sxfer & 0x1f) && idiv)
		tp->period = (((sxfer>>5)+4)*div_10M[idiv-1])/np->clock_khz;
	else
		tp->period = 0xffff;

	
	if (tp->sval == sxfer && tp->wval == scntl3)
		return;
	tp->sval = sxfer;
	tp->wval = scntl3;

	if (sxfer & 0x01f) {
		
		if (tp->period <= 2000)
			OUTOFFB(nc_stest2, EXT);
	}
 
	spi_display_xfer_agreement(tp->starget);

	ncr_set_sync_wide_status(np, target);
}


static void ncr_setwide (struct ncb *np, struct ccb *cp, u_char wide, u_char ack)
{
	struct scsi_cmnd *cmd = cp->cmd;
	u16 target = INB (nc_sdid) & 0x0f;
	struct tcb *tp;
	u_char	scntl3;
	u_char	sxfer;

	BUG_ON(target != (scmd_id(cmd) & 0xf));

	tp = &np->target[target];
	tp->widedone  =  wide+1;
	scntl3 = (tp->wval & (~EWS)) | (wide ? EWS : 0);

	sxfer = ack ? 0 : tp->sval;

	if (tp->sval == sxfer && tp->wval == scntl3) return;
	tp->sval = sxfer;
	tp->wval = scntl3;

	if (bootverbose >= 2) {
		dev_info(&cmd->device->sdev_target->dev, "WIDE SCSI %sabled.\n",
				(scntl3 & EWS) ? "en" : "dis");
	}

	ncr_set_sync_wide_status(np, target);
}


static void ncr_setup_tags (struct ncb *np, struct scsi_device *sdev)
{
	unsigned char tn = sdev->id, ln = sdev->lun;
	struct tcb *tp = &np->target[tn];
	struct lcb *lp = tp->lp[ln];
	u_char   reqtags, maxdepth;

	if ((!tp) || (!lp) || !sdev)
		return;

	if (!lp->scdev_depth)
		return;

	maxdepth = lp->scdev_depth;
	if (maxdepth > lp->maxnxs)	maxdepth    = lp->maxnxs;
	if (lp->maxtags > maxdepth)	lp->maxtags = maxdepth;
	if (lp->numtags > maxdepth)	lp->numtags = maxdepth;

	if (sdev->tagged_supported && lp->numtags > 1) {
		reqtags = lp->numtags;
	} else {
		reqtags = 1;
	}

	lp->numtags = reqtags;
	if (lp->numtags > lp->maxtags)
		lp->maxtags = lp->numtags;

	if	(reqtags > 1 && lp->usetags) {	 
		if (lp->queuedepth == reqtags)	 
			return;
		lp->queuedepth	= reqtags;
	}
	else if	(reqtags <= 1 && !lp->usetags) { 
		lp->queuedepth	= reqtags;
		return;
	}
	else {					 
		if (lp->busyccbs)		 
			return;
		lp->queuedepth	= reqtags;
		lp->usetags	= reqtags > 1 ? 1 : 0;
	}

	lp->jump_tag.l_paddr = lp->usetags?
			cpu_to_scr(NCB_SCRIPT_PHYS(np, resel_tag)) :
			cpu_to_scr(NCB_SCRIPT_PHYS(np, resel_notag));

	if (bootverbose) {
		if (lp->usetags) {
			dev_info(&sdev->sdev_gendev,
				"tagged command queue depth set to %d\n",
				reqtags);
		} else {
			dev_info(&sdev->sdev_gendev,
					"tagged command queueing disabled\n");
		}
	}
}


static void ncr_timeout (struct ncb *np)
{
	u_long	thistime = jiffies;


	if (np->release_stage) {
		if (np->release_stage == 1) np->release_stage = 2;
		return;
	}

	np->timer.expires = jiffies + SCSI_NCR_TIMER_INTERVAL;
	add_timer(&np->timer);

	if (np->settle_time) {
		if (np->settle_time <= thistime) {
			if (bootverbose > 1)
				printk("%s: command processing resumed\n", ncr_name(np));
			np->settle_time	= 0;
			np->disc	= 1;
			requeue_waiting_list(np);
		}
		return;
	}

	if (np->lasttime + 4*HZ < thistime) {
		np->lasttime = thistime;
	}

#ifdef SCSI_NCR_BROKEN_INTR
	if (INB(nc_istat) & (INTF|SIP|DIP)) {

		if (DEBUG_FLAGS & DEBUG_TINY) printk ("{");
		ncr_exception (np);
		if (DEBUG_FLAGS & DEBUG_TINY) printk ("}");
	}
#endif 
}


static void ncr_log_hard_error(struct ncb *np, u16 sist, u_char dstat)
{
	u32	dsp;
	int	script_ofs;
	int	script_size;
	char	*script_name;
	u_char	*script_base;
	int	i;

	dsp	= INL (nc_dsp);

	if (dsp > np->p_script && dsp <= np->p_script + sizeof(struct script)) {
		script_ofs	= dsp - np->p_script;
		script_size	= sizeof(struct script);
		script_base	= (u_char *) np->script0;
		script_name	= "script";
	}
	else if (np->p_scripth < dsp && 
		 dsp <= np->p_scripth + sizeof(struct scripth)) {
		script_ofs	= dsp - np->p_scripth;
		script_size	= sizeof(struct scripth);
		script_base	= (u_char *) np->scripth0;
		script_name	= "scripth";
	} else {
		script_ofs	= dsp;
		script_size	= 0;
		script_base	= NULL;
		script_name	= "mem";
	}

	printk ("%s:%d: ERROR (%x:%x) (%x-%x-%x) (%x/%x) @ (%s %x:%08x).\n",
		ncr_name (np), (unsigned)INB (nc_sdid)&0x0f, dstat, sist,
		(unsigned)INB (nc_socl), (unsigned)INB (nc_sbcl), (unsigned)INB (nc_sbdl),
		(unsigned)INB (nc_sxfer),(unsigned)INB (nc_scntl3), script_name, script_ofs,
		(unsigned)INL (nc_dbc));

	if (((script_ofs & 3) == 0) &&
	    (unsigned)script_ofs < script_size) {
		printk ("%s: script cmd = %08x\n", ncr_name(np),
			scr_to_cpu((int) *(ncrcmd *)(script_base + script_ofs)));
	}

	printk ("%s: regdump:", ncr_name(np));
	for (i=0; i<16;i++)
            printk (" %02x", (unsigned)INB_OFF(i));
	printk (".\n");
}


void ncr_exception (struct ncb *np)
{
	u_char	istat, dstat;
	u16	sist;
	int	i;

	istat = INB (nc_istat);
	if (istat & INTF) {
		OUTB (nc_istat, (istat & SIGP) | INTF);
		istat = INB (nc_istat);
		if (DEBUG_FLAGS & DEBUG_TINY) printk ("F ");
		ncr_wakeup_done (np);
	}

	if (!(istat & (SIP|DIP)))
		return;

	if (istat & CABRT)
		OUTB (nc_istat, CABRT);


	sist  = (istat & SIP) ? INW (nc_sist)  : 0;
	dstat = (istat & DIP) ? INB (nc_dstat) : 0;

	if (DEBUG_FLAGS & DEBUG_TINY)
		printk ("<%d|%x:%x|%x:%x>",
			(int)INB(nc_scr0),
			dstat,sist,
			(unsigned)INL(nc_dsp),
			(unsigned)INL(nc_dbc));


	if (!(sist  & (STO|GEN|HTH|SGE|UDC|RST)) &&
	    !(dstat & (MDPE|BF|ABRT|IID))) {
		if ((sist & SBMC) && ncr_int_sbmc (np))
			return;
		if ((sist & PAR)  && ncr_int_par  (np))
			return;
		if (sist & MA) {
			ncr_int_ma (np);
			return;
		}
		if (dstat & SIR) {
			ncr_int_sir (np);
			return;
		}
		if (!(sist & (SBMC|PAR)) && !(dstat & SSI)) {
			printk(	"%s: unknown interrupt(s) ignored, "
				"ISTAT=%x DSTAT=%x SIST=%x\n",
				ncr_name(np), istat, dstat, sist);
			return;
		}
		OUTONB_STD ();
		return;
	}


	if (sist & RST) {
		ncr_init (np, 1, bootverbose ? "scsi reset" : NULL, HS_RESET);
		return;
	}

	if ((sist & STO) &&
		!(dstat & (MDPE|BF|ABRT))) {
		OUTONB (nc_ctest3, CLF);

		ncr_int_sto (np);
		return;
	}


	if (time_after(jiffies, np->regtime)) {
		np->regtime = jiffies + 10*HZ;
		for (i = 0; i<sizeof(np->regdump); i++)
			((char*)&np->regdump)[i] = INB_OFF(i);
		np->regdump.nc_dstat = dstat;
		np->regdump.nc_sist  = sist;
	}

	ncr_log_hard_error(np, sist, dstat);

	printk ("%s: have to clear fifos.\n", ncr_name (np));
	OUTB (nc_stest3, TE|CSF);
	OUTONB (nc_ctest3, CLF);

	if ((sist & (SGE)) ||
		(dstat & (MDPE|BF|ABRT|IID))) {
		ncr_start_reset(np);
		return;
	}

	if (sist & HTH) {
		printk ("%s: handshake timeout\n", ncr_name(np));
		ncr_start_reset(np);
		return;
	}

	if (sist & UDC) {
		printk ("%s: unexpected disconnect\n", ncr_name(np));
		OUTB (HS_PRT, HS_UNEXPECTED);
		OUTL_DSP (NCB_SCRIPT_PHYS (np, cleanup));
		return;
	}

	printk ("%s: unknown interrupt\n", ncr_name(np));
}


void ncr_int_sto (struct ncb *np)
{
	u_long dsa;
	struct ccb *cp;
	if (DEBUG_FLAGS & DEBUG_TINY) printk ("T");


	dsa = INL (nc_dsa);
	cp = np->ccb;
	while (cp && (CCB_PHYS (cp, phys) != dsa))
		cp = cp->link_ccb;

	if (cp) {
		cp-> host_status = HS_SEL_TIMEOUT;
		ncr_complete (np, cp);
	}


	OUTL_DSP (NCB_SCRIPTH_PHYS (np, sto_restart));
	return;
}


static int ncr_int_sbmc (struct ncb *np)
{
	u_char scsi_mode = INB (nc_stest4) & SMODE;

	if (scsi_mode != np->scsi_mode) {
		printk("%s: SCSI bus mode change from %x to %x.\n",
			ncr_name(np), np->scsi_mode, scsi_mode);

		np->scsi_mode = scsi_mode;


		np->settle_time	= jiffies + HZ;
		ncr_init (np, 0, bootverbose ? "scsi mode change" : NULL, HS_RESET);
		return 1;
	}
	return 0;
}


static int ncr_int_par (struct ncb *np)
{
	u_char	hsts	= INB (HS_PRT);
	u32	dbc	= INL (nc_dbc);
	u_char	sstat1	= INB (nc_sstat1);
	int phase	= -1;
	int msg		= -1;
	u32 jmp;

	printk("%s: SCSI parity error detected: SCR1=%d DBC=%x SSTAT1=%x\n",
		ncr_name(np), hsts, dbc, sstat1);

	if (!(INB (nc_scntl1) & ISCON))
		return 0;

	if (hsts & HS_INVALMASK)
		goto reset_all;

	if (!(dbc & 0xc0000000))
		phase = (dbc >> 24) & 7;
	if (phase == 7)
		msg = MSG_PARITY_ERROR;
	else
		msg = INITIATOR_ERROR;


	if (phase == 1)
		jmp = NCB_SCRIPTH_PHYS (np, par_err_data_in);
	else
		jmp = NCB_SCRIPTH_PHYS (np, par_err_other);

	OUTONB (nc_ctest3, CLF );	
	OUTB (nc_stest3, TE|CSF);	

	np->msgout[0] = msg;
	OUTL_DSP (jmp);
	return 1;

reset_all:
	ncr_start_reset(np);
	return 1;
}


static void ncr_int_ma (struct ncb *np)
{
	u32	dbc;
	u32	rest;
	u32	dsp;
	u32	dsa;
	u32	nxtdsp;
	u32	newtmp;
	u32	*vdsp;
	u32	oadr, olen;
	u32	*tblp;
	ncrcmd *newcmd;
	u_char	cmd, sbcl;
	struct ccb *cp;

	dsp	= INL (nc_dsp);
	dbc	= INL (nc_dbc);
	sbcl	= INB (nc_sbcl);

	cmd	= dbc >> 24;
	rest	= dbc & 0xffffff;


	if ((cmd & 1) == 0) {
		u_char	ctest5, ss0, ss2;
		u16	delta;

		ctest5 = (np->rv_ctest5 & DFS) ? INB (nc_ctest5) : 0;
		if (ctest5 & DFS)
			delta=(((ctest5 << 8) | (INB (nc_dfifo) & 0xff)) - rest) & 0x3ff;
		else
			delta=(INB (nc_dfifo) - rest) & 0x7f;


		rest += delta;
		ss0  = INB (nc_sstat0);
		if (ss0 & OLF) rest++;
		if (ss0 & ORF) rest++;
		if (INB(nc_scntl3) & EWS) {
			ss2 = INB (nc_sstat2);
			if (ss2 & OLF1) rest++;
			if (ss2 & ORF1) rest++;
		}

		if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_PHASE))
			printk ("P%x%x RL=%d D=%d SS0=%x ", cmd&7, sbcl&7,
				(unsigned) rest, (unsigned) delta, ss0);

	} else	{
		if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_PHASE))
			printk ("P%x%x RL=%d ", cmd&7, sbcl&7, rest);
	}

	OUTONB (nc_ctest3, CLF );	
	OUTB (nc_stest3, TE|CSF);	

	dsa = INL (nc_dsa);
	if (!(cmd & 6)) {
		cp = np->header.cp;
		if (CCB_PHYS(cp, phys) != dsa)
			cp = NULL;
	} else {
		cp  = np->ccb;
		while (cp && (CCB_PHYS (cp, phys) != dsa))
			cp = cp->link_ccb;
	}

	vdsp	= NULL;
	nxtdsp	= 0;
	if	(dsp >  np->p_script &&
		 dsp <= np->p_script + sizeof(struct script)) {
		vdsp = (u32 *)((char*)np->script0 + (dsp-np->p_script-8));
		nxtdsp = dsp;
	}
	else if	(dsp >  np->p_scripth &&
		 dsp <= np->p_scripth + sizeof(struct scripth)) {
		vdsp = (u32 *)((char*)np->scripth0 + (dsp-np->p_scripth-8));
		nxtdsp = dsp;
	}
	else if (cp) {
		if	(dsp == CCB_PHYS (cp, patch[2])) {
			vdsp = &cp->patch[0];
			nxtdsp = scr_to_cpu(vdsp[3]);
		}
		else if (dsp == CCB_PHYS (cp, patch[6])) {
			vdsp = &cp->patch[4];
			nxtdsp = scr_to_cpu(vdsp[3]);
		}
	}


	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printk ("\nCP=%p CP2=%p DSP=%x NXT=%x VDSP=%p CMD=%x ",
			cp, np->header.cp,
			(unsigned)dsp,
			(unsigned)nxtdsp, vdsp, cmd);
	}

	if (!cp) {
		printk ("%s: SCSI phase error fixup: "
			"CCB already dequeued (0x%08lx)\n", 
			ncr_name (np), (u_long) np->header.cp);
		goto reset_all;
	}


	oadr = scr_to_cpu(vdsp[1]);

	if (cmd & 0x10) {	
		tblp = (u32 *) ((char*) &cp->phys + oadr);
		olen = scr_to_cpu(tblp[0]);
		oadr = scr_to_cpu(tblp[1]);
	} else {
		tblp = (u32 *) 0;
		olen = scr_to_cpu(vdsp[0]) & 0xffffff;
	}

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printk ("OCMD=%x\nTBLP=%p OLEN=%x OADR=%x\n",
			(unsigned) (scr_to_cpu(vdsp[0]) >> 24),
			tblp,
			(unsigned) olen,
			(unsigned) oadr);
	}


	if (cmd != (scr_to_cpu(vdsp[0]) >> 24)) {
		PRINT_ADDR(cp->cmd, "internal error: cmd=%02x != %02x=(vdsp[0] "
				">> 24)\n", cmd, scr_to_cpu(vdsp[0]) >> 24);

		goto reset_all;
	}

	if (cp != np->header.cp) {
		printk ("%s: SCSI phase error fixup: "
			"CCB address mismatch (0x%08lx != 0x%08lx)\n", 
			ncr_name (np), (u_long) cp, (u_long) np->header.cp);
	}


	if (cmd & 0x06) {
		PRINT_ADDR(cp->cmd, "phase change %x-%x %d@%08x resid=%d.\n",
			cmd&7, sbcl&7, (unsigned)olen,
			(unsigned)oadr, (unsigned)rest);
		goto unexpected_phase;
	}


	newcmd = cp->patch;
	newtmp = CCB_PHYS (cp, patch);
	if (newtmp == scr_to_cpu(cp->phys.header.savep)) {
		newcmd = &cp->patch[4];
		newtmp = CCB_PHYS (cp, patch[4]);
	}


	newcmd[0] = cpu_to_scr(((cmd & 0x0f) << 24) | rest);
	newcmd[1] = cpu_to_scr(oadr + olen - rest);
	newcmd[2] = cpu_to_scr(SCR_JUMP);
	newcmd[3] = cpu_to_scr(nxtdsp);

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		PRINT_ADDR(cp->cmd, "newcmd[%d] %x %x %x %x.\n",
			(int) (newcmd - cp->patch),
			(unsigned)scr_to_cpu(newcmd[0]),
			(unsigned)scr_to_cpu(newcmd[1]),
			(unsigned)scr_to_cpu(newcmd[2]),
			(unsigned)scr_to_cpu(newcmd[3]));
	}
	OUTL (nc_temp, newtmp);
	OUTL_DSP (NCB_SCRIPT_PHYS (np, dispatch));
	return;

unexpected_phase:
	dsp -= 8;
	nxtdsp = 0;

	switch (cmd & 7) {
	case 2:	
		nxtdsp = NCB_SCRIPT_PHYS (np, dispatch);
		break;
#if 0
	case 3:	
		nxtdsp = NCB_SCRIPT_PHYS (np, dispatch);
		break;
#endif
	case 6:	
		np->scripth->nxtdsp_go_on[0] = cpu_to_scr(dsp + 8);
		if	(dsp == NCB_SCRIPT_PHYS (np, send_ident)) {
			cp->host_status = HS_BUSY;
			nxtdsp = NCB_SCRIPTH_PHYS (np, clratn_go_on);
		}
		else if	(dsp == NCB_SCRIPTH_PHYS (np, send_wdtr) ||
			 dsp == NCB_SCRIPTH_PHYS (np, send_sdtr)) {
			nxtdsp = NCB_SCRIPTH_PHYS (np, nego_bad_phase);
		}
		break;
#if 0
	case 7:	
		nxtdsp = NCB_SCRIPT_PHYS (np, clrack);
		break;
#endif
	}

	if (nxtdsp) {
		OUTL_DSP (nxtdsp);
		return;
	}

reset_all:
	ncr_start_reset(np);
}


static void ncr_sir_to_redo(struct ncb *np, int num, struct ccb *cp)
{
	struct scsi_cmnd *cmd	= cp->cmd;
	struct tcb *tp	= &np->target[cmd->device->id];
	struct lcb *lp	= tp->lp[cmd->device->lun];
	struct list_head *qp;
	struct ccb *	cp2;
	int		disc_cnt = 0;
	int		busy_cnt = 0;
	u32		startp;
	u_char		s_status = INB (SS_PRT);

	if (lp) {
		qp = lp->busy_ccbq.prev;
		while (qp != &lp->busy_ccbq) {
			cp2 = list_entry(qp, struct ccb, link_ccbq);
			qp  = qp->prev;
			++busy_cnt;
			if (cp2 == cp)
				break;
			cp2->start.schedule.l_paddr =
			cpu_to_scr(NCB_SCRIPTH_PHYS (np, skip));
		}
		lp->held_ccb = cp;	
		disc_cnt = lp->queuedccbs - busy_cnt;
	}

	switch(s_status) {
	default:	
	case S_QUEUE_FULL:
		if (!lp)
			goto out;
		if (bootverbose >= 1) {
			PRINT_ADDR(cmd, "QUEUE FULL! %d busy, %d disconnected "
					"CCBs\n", busy_cnt, disc_cnt);
		}
		if (disc_cnt < lp->numtags) {
			lp->numtags	= disc_cnt > 2 ? disc_cnt : 2;
			lp->num_good	= 0;
			ncr_setup_tags (np, cmd->device);
		}
		cp->phys.header.savep = cp->startp;
		cp->host_status = HS_BUSY;
		cp->scsi_status = S_ILLEGAL;

		ncr_put_start_queue(np, cp);
		if (disc_cnt)
			INB (nc_ctest2);		
		OUTL_DSP (NCB_SCRIPT_PHYS (np, reselect));
		return;
	case S_TERMINATED:
	case S_CHECK_COND:
		if (cp->auto_sense)
			goto out;

		cp->scsi_smsg2[0]	= IDENTIFY(0, cmd->device->lun);
		cp->phys.smsg.addr	= cpu_to_scr(CCB_PHYS (cp, scsi_smsg2));
		cp->phys.smsg.size	= cpu_to_scr(1);

		cp->phys.cmd.addr	= cpu_to_scr(CCB_PHYS (cp, sensecmd));
		cp->phys.cmd.size	= cpu_to_scr(6);

		cp->sensecmd[0]		= 0x03;
		cp->sensecmd[1]		= cmd->device->lun << 5;
		cp->sensecmd[4]		= sizeof(cp->sense_buf);

		memset(cp->sense_buf, 0, sizeof(cp->sense_buf));
		cp->phys.sense.addr	= cpu_to_scr(CCB_PHYS(cp,sense_buf[0]));
		cp->phys.sense.size	= cpu_to_scr(sizeof(cp->sense_buf));

		startp = cpu_to_scr(NCB_SCRIPTH_PHYS (np, sdata_in));

		cp->phys.header.savep	= startp;
		cp->phys.header.goalp	= startp + 24;
		cp->phys.header.lastp	= startp;
		cp->phys.header.wgoalp	= startp + 24;
		cp->phys.header.wlastp	= startp;

		cp->host_status = HS_BUSY;
		cp->scsi_status = S_ILLEGAL;
		cp->auto_sense	= s_status;

		cp->start.schedule.l_paddr =
			cpu_to_scr(NCB_SCRIPT_PHYS (np, select));

		if (cmd->device->select_no_atn)
			cp->start.schedule.l_paddr =
			cpu_to_scr(NCB_SCRIPTH_PHYS (np, select_no_atn));

		ncr_put_start_queue(np, cp);

		OUTL_DSP (NCB_SCRIPT_PHYS (np, start));
		return;
	}

out:
	OUTONB_STD ();
	return;
}



void ncr_int_sir (struct ncb *np)
{
	u_char scntl3;
	u_char chg, ofs, per, fak, wide;
	u_char num = INB (nc_dsps);
	struct ccb *cp=NULL;
	u_long	dsa    = INL (nc_dsa);
	u_char	target = INB (nc_sdid) & 0x0f;
	struct tcb *tp     = &np->target[target];
	struct scsi_target *starget = tp->starget;

	if (DEBUG_FLAGS & DEBUG_TINY) printk ("I#%d", num);

	switch (num) {
	case SIR_INTFLY:
		ncr_wakeup_done(np);
#ifdef SCSI_NCR_CCB_DONE_SUPPORT
		OUTL(nc_dsp, NCB_SCRIPT_PHYS (np, done_end) + 8);
#else
		OUTL(nc_dsp, NCB_SCRIPT_PHYS (np, start));
#endif
		return;
	case SIR_RESEL_NO_MSG_IN:
	case SIR_RESEL_NO_IDENTIFY:
		if (tp->lp[0]) { 
			OUTL_DSP (scr_to_cpu(tp->lp[0]->jump_ccb[0]));
			return;
		}
	case SIR_RESEL_BAD_TARGET:	
	case SIR_RESEL_BAD_LUN:		
	case SIR_RESEL_BAD_I_T_L_Q:	
	case SIR_RESEL_BAD_I_T_L:	
		printk ("%s:%d: SIR %d, "
			"incorrect nexus identification on reselection\n",
			ncr_name (np), target, num);
		goto out;
	case SIR_DONE_OVERFLOW:
		printk ("%s:%d: SIR %d, "
			"CCB done queue overflow\n",
			ncr_name (np), target, num);
		goto out;
	case SIR_BAD_STATUS:
		cp = np->header.cp;
		if (!cp || CCB_PHYS (cp, phys) != dsa)
			goto out;
		ncr_sir_to_redo(np, num, cp);
		return;
	default:
		cp = np->ccb;
		while (cp && (CCB_PHYS (cp, phys) != dsa))
			cp = cp->link_ccb;

		BUG_ON(!cp);
		BUG_ON(cp != np->header.cp);

		if (!cp || cp != np->header.cp)
			goto out;
	}

	switch (num) {

	case SIR_NEGO_FAILED:
		OUTB (HS_PRT, HS_BUSY);

		

	case SIR_NEGO_PROTO:

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->cmd, "negotiation failed sir=%x "
					"status=%x.\n", num, cp->nego_status);
		}

		switch (cp->nego_status) {

		case NS_SYNC:
			spi_period(starget) = 0;
			spi_offset(starget) = 0;
			ncr_setsync (np, cp, 0, 0xe0);
			break;

		case NS_WIDE:
			spi_width(starget) = 0;
			ncr_setwide (np, cp, 0, 0);
			break;

		}
		np->msgin [0] = NOP;
		np->msgout[0] = NOP;
		cp->nego_status = 0;
		break;

	case SIR_NEGO_SYNC:
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			ncr_print_msg(cp, "sync msgin", np->msgin);
		}

		chg = 0;
		per = np->msgin[3];
		ofs = np->msgin[4];
		if (ofs==0) per=255;


		if (ofs && starget)
			spi_support_sync(starget) = 1;


		if (per < np->minsync)
			{chg = 1; per = np->minsync;}
		if (per < tp->minsync)
			{chg = 1; per = tp->minsync;}
		if (ofs > tp->maxoffs)
			{chg = 1; ofs = tp->maxoffs;}

		fak	= 7;
		scntl3	= 0;
		if (ofs != 0) {
			ncr_getsync(np, per, &fak, &scntl3);
			if (fak > 7) {
				chg = 1;
				ofs = 0;
			}
		}
		if (ofs == 0) {
			fak	= 7;
			per	= 0;
			scntl3	= 0;
			tp->minsync = 0;
		}

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->cmd, "sync: per=%d scntl3=0x%x ofs=%d "
				"fak=%d chg=%d.\n", per, scntl3, ofs, fak, chg);
		}

		if (INB (HS_PRT) == HS_NEGOTIATE) {
			OUTB (HS_PRT, HS_BUSY);
			switch (cp->nego_status) {

			case NS_SYNC:
				
				if (chg) {
					
					spi_period(starget) = 0;
					spi_offset(starget) = 0;
					ncr_setsync(np, cp, 0, 0xe0);
					OUTL_DSP(NCB_SCRIPT_PHYS (np, msg_bad));
				} else {
					
					spi_period(starget) = per;
					spi_offset(starget) = ofs;
					ncr_setsync(np, cp, scntl3, (fak<<5)|ofs);
					OUTL_DSP(NCB_SCRIPT_PHYS (np, clrack));
				}
				return;

			case NS_WIDE:
				spi_width(starget) = 0;
				ncr_setwide(np, cp, 0, 0);
				break;
			}
		}


		spi_period(starget) = per;
		spi_offset(starget) = ofs;
		ncr_setsync(np, cp, scntl3, (fak<<5)|ofs);

		spi_populate_sync_msg(np->msgout, per, ofs);
		cp->nego_status = NS_SYNC;

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			ncr_print_msg(cp, "sync msgout", np->msgout);
		}

		if (!ofs) {
			OUTL_DSP (NCB_SCRIPT_PHYS (np, msg_bad));
			return;
		}
		np->msgin [0] = NOP;

		break;

	case SIR_NEGO_WIDE:
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			ncr_print_msg(cp, "wide msgin", np->msgin);
		}


		chg  = 0;
		wide = np->msgin[3];


		if (wide && starget)
			spi_support_wide(starget) = 1;


		if (wide > tp->usrwide)
			{chg = 1; wide = tp->usrwide;}

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->cmd, "wide: wide=%d chg=%d.\n", wide,
					chg);
		}

		if (INB (HS_PRT) == HS_NEGOTIATE) {
			OUTB (HS_PRT, HS_BUSY);
			switch (cp->nego_status) {

			case NS_WIDE:
				if (chg) {
					
					spi_width(starget) = 0;
					ncr_setwide(np, cp, 0, 1);
					OUTL_DSP (NCB_SCRIPT_PHYS (np, msg_bad));
				} else {
					
					spi_width(starget) = wide;
					ncr_setwide(np, cp, wide, 1);
					OUTL_DSP (NCB_SCRIPT_PHYS (np, clrack));
				}
				return;

			case NS_SYNC:
				spi_period(starget) = 0;
				spi_offset(starget) = 0;
				ncr_setsync(np, cp, 0, 0xe0);
				break;
			}
		}


		spi_width(starget) = wide;
		ncr_setwide(np, cp, wide, 1);
		spi_populate_width_msg(np->msgout, wide);

		np->msgin [0] = NOP;

		cp->nego_status = NS_WIDE;

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			ncr_print_msg(cp, "wide msgout", np->msgin);
		}
		break;


	case SIR_REJECT_RECEIVED:

		PRINT_ADDR(cp->cmd, "MESSAGE_REJECT received (%x:%x).\n",
			(unsigned)scr_to_cpu(np->lastmsg), np->msgout[0]);
		break;

	case SIR_REJECT_SENT:

		ncr_print_msg(cp, "MESSAGE_REJECT sent for", np->msgin);
		break;


	case SIR_IGN_RESIDUE:

		PRINT_ADDR(cp->cmd, "IGNORE_WIDE_RESIDUE received, but not yet "
				"implemented.\n");
		break;
#if 0
	case SIR_MISSING_SAVE:

		PRINT_ADDR(cp->cmd, "DISCONNECT received, but datapointer "
				"not saved: data=%x save=%x goal=%x.\n",
			(unsigned) INL (nc_temp),
			(unsigned) scr_to_cpu(np->header.savep),
			(unsigned) scr_to_cpu(np->header.goalp));
		break;
#endif
	}

out:
	OUTONB_STD ();
}


static struct ccb *ncr_get_ccb(struct ncb *np, struct scsi_cmnd *cmd)
{
	u_char tn = cmd->device->id;
	u_char ln = cmd->device->lun;
	struct tcb *tp = &np->target[tn];
	struct lcb *lp = tp->lp[ln];
	u_char tag = NO_TAG;
	struct ccb *cp = NULL;

	if (lp) {
		struct list_head *qp;
		if (lp->usetags && lp->busyccbs >= lp->maxnxs)
			return NULL;

		if (list_empty(&lp->free_ccbq))
			ncr_alloc_ccb(np, tn, ln);

		qp = ncr_list_pop(&lp->free_ccbq);
		if (qp) {
			cp = list_entry(qp, struct ccb, link_ccbq);
			if (cp->magic) {
				PRINT_ADDR(cmd, "ccb free list corrupted "
						"(@%p)\n", cp);
				cp = NULL;
			} else {
				list_add_tail(qp, &lp->wait_ccbq);
				++lp->busyccbs;
			}
		}

		if (cp) {
			if (lp->usetags)
				tag = lp->cb_tags[lp->ia_tag];
		}
		else if (lp->actccbs > 0)
			return NULL;
	}

	if (!cp)
		cp = np->ccb;

#if 0
	while (cp->magic) {
		if (flags & SCSI_NOSLEEP) break;
		if (tsleep ((caddr_t)cp, PRIBIO|PCATCH, "ncr", 0))
			break;
	}
#endif

	if (cp->magic)
		return NULL;

	cp->magic = 1;

	if (lp) {
		if (tag != NO_TAG) {
			++lp->ia_tag;
			if (lp->ia_tag == MAX_TAGS)
				lp->ia_tag = 0;
			lp->tags_umap |= (((tagmap_t) 1) << tag);
		}
	}

	cp->tag	   = tag;
	cp->target = tn;
	cp->lun    = ln;

	if (DEBUG_FLAGS & DEBUG_TAGS) {
		PRINT_ADDR(cmd, "ccb @%p using tag %d.\n", cp, tag);
	}

	return cp;
}


static void ncr_free_ccb (struct ncb *np, struct ccb *cp)
{
	struct tcb *tp = &np->target[cp->target];
	struct lcb *lp = tp->lp[cp->lun];

	if (DEBUG_FLAGS & DEBUG_TAGS) {
		PRINT_ADDR(cp->cmd, "ccb @%p freeing tag %d.\n", cp, cp->tag);
	}

	if (lp) {
		if (cp->tag != NO_TAG) {
			lp->cb_tags[lp->if_tag++] = cp->tag;
			if (lp->if_tag == MAX_TAGS)
				lp->if_tag = 0;
			lp->tags_umap &= ~(((tagmap_t) 1) << cp->tag);
			lp->tags_smap &= lp->tags_umap;
			lp->jump_ccb[cp->tag] =
				cpu_to_scr(NCB_SCRIPTH_PHYS(np, bad_i_t_l_q));
		} else {
			lp->jump_ccb[0] =
				cpu_to_scr(NCB_SCRIPTH_PHYS(np, bad_i_t_l));
		}
	}


	if (lp) {
		if (cp != np->ccb)
			list_move(&cp->link_ccbq, &lp->free_ccbq);
		--lp->busyccbs;
		if (cp->queued) {
			--lp->queuedccbs;
		}
	}
	cp -> host_status = HS_IDLE;
	cp -> magic = 0;
	if (cp->queued) {
		--np->queuedccbs;
		cp->queued = 0;
	}

#if 0
	if (cp == np->ccb)
		wakeup ((caddr_t) cp);
#endif
}


#define ncr_reg_bus_addr(r) (np->paddr + offsetof (struct ncr_reg, r))

static void ncr_init_ccb(struct ncb *np, struct ccb *cp)
{
	ncrcmd copy_4 = np->features & FE_PFEN ? SCR_COPY(4) : SCR_COPY_F(4);

	cp->p_ccb 	   = vtobus(cp);
	cp->phys.header.cp = cp;

	INIT_LIST_HEAD(&cp->link_ccbq);

	cp->start.setup_dsa[0]	 = cpu_to_scr(copy_4);
	cp->start.setup_dsa[1]	 = cpu_to_scr(CCB_PHYS(cp, start.p_phys));
	cp->start.setup_dsa[2]	 = cpu_to_scr(ncr_reg_bus_addr(nc_dsa));
	cp->start.schedule.l_cmd = cpu_to_scr(SCR_JUMP);
	cp->start.p_phys	 = cpu_to_scr(CCB_PHYS(cp, phys));

	memcpy(&cp->restart, &cp->start, sizeof(cp->restart));

	cp->start.schedule.l_paddr   = cpu_to_scr(NCB_SCRIPT_PHYS (np, idle));
	cp->restart.schedule.l_paddr = cpu_to_scr(NCB_SCRIPTH_PHYS (np, abort));
}


static void ncr_alloc_ccb(struct ncb *np, u_char tn, u_char ln)
{
	struct tcb *tp = &np->target[tn];
	struct lcb *lp = tp->lp[ln];
	struct ccb *cp = NULL;

	cp = m_calloc_dma(sizeof(struct ccb), "CCB");
	if (!cp)
		return;

	lp->actccbs++;
	np->actccbs++;
	memset(cp, 0, sizeof (*cp));
	ncr_init_ccb(np, cp);

	cp->link_ccb      = np->ccb->link_ccb;
	np->ccb->link_ccb = cp;

	list_add(&cp->link_ccbq, &lp->free_ccbq);
}



static void ncr_init_tcb (struct ncb *np, u_char tn)
{
	struct tcb *tp = &np->target[tn];
	ncrcmd copy_1 = np->features & FE_PFEN ? SCR_COPY(1) : SCR_COPY_F(1);
	int th = tn & 3;
	int i;

	tp->jump_tcb.l_cmd   =
		cpu_to_scr((SCR_JUMP ^ IFFALSE (DATA (0x80 + tn))));
	tp->jump_tcb.l_paddr = np->jump_tcb[th].l_paddr;

	tp->getscr[0] =	cpu_to_scr(copy_1);
	tp->getscr[1] = cpu_to_scr(vtobus (&tp->sval));
#ifdef SCSI_NCR_BIG_ENDIAN
	tp->getscr[2] = cpu_to_scr(ncr_reg_bus_addr(nc_sxfer) ^ 3);
#else
	tp->getscr[2] = cpu_to_scr(ncr_reg_bus_addr(nc_sxfer));
#endif

	tp->getscr[3] =	cpu_to_scr(copy_1);
	tp->getscr[4] = cpu_to_scr(vtobus (&tp->wval));
#ifdef SCSI_NCR_BIG_ENDIAN
	tp->getscr[5] = cpu_to_scr(ncr_reg_bus_addr(nc_scntl3) ^ 3);
#else
	tp->getscr[5] = cpu_to_scr(ncr_reg_bus_addr(nc_scntl3));
#endif

	tp->call_lun.l_cmd   = cpu_to_scr(SCR_CALL);
	tp->call_lun.l_paddr = cpu_to_scr(NCB_SCRIPT_PHYS (np, resel_lun));

	for (i = 0 ; i < 4 ; i++) {
		tp->jump_lcb[i].l_cmd   =
				cpu_to_scr((SCR_JUMP ^ IFTRUE (MASK (i, 3))));
		tp->jump_lcb[i].l_paddr =
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, bad_identify));
	}

	np->jump_tcb[th].l_paddr = cpu_to_scr(vtobus (&tp->jump_tcb));

#ifdef SCSI_NCR_BIG_ENDIAN
	BUG_ON(((offsetof(struct ncr_reg, nc_sxfer) ^
		 offsetof(struct tcb    , sval    )) &3) != 3);
	BUG_ON(((offsetof(struct ncr_reg, nc_scntl3) ^
		 offsetof(struct tcb    , wval    )) &3) != 3);
#else
	BUG_ON(((offsetof(struct ncr_reg, nc_sxfer) ^
		 offsetof(struct tcb    , sval    )) &3) != 0);
	BUG_ON(((offsetof(struct ncr_reg, nc_scntl3) ^
		 offsetof(struct tcb    , wval    )) &3) != 0);
#endif
}


static struct lcb *ncr_alloc_lcb (struct ncb *np, u_char tn, u_char ln)
{
	struct tcb *tp = &np->target[tn];
	struct lcb *lp = tp->lp[ln];
	ncrcmd copy_4 = np->features & FE_PFEN ? SCR_COPY(4) : SCR_COPY_F(4);
	int lh = ln & 3;

	if (lp)
		return lp;

	lp = m_calloc_dma(sizeof(struct lcb), "LCB");
	if (!lp)
		goto fail;
	memset(lp, 0, sizeof(*lp));
	tp->lp[ln] = lp;

	if (!tp->jump_tcb.l_cmd)
		ncr_init_tcb(np, tn);

	INIT_LIST_HEAD(&lp->free_ccbq);
	INIT_LIST_HEAD(&lp->busy_ccbq);
	INIT_LIST_HEAD(&lp->wait_ccbq);
	INIT_LIST_HEAD(&lp->skip_ccbq);

	lp->maxnxs	= 1;
	lp->jump_ccb	= &lp->jump_ccb_0;
	lp->p_jump_ccb	= cpu_to_scr(vtobus(lp->jump_ccb));

	lp->jump_lcb.l_cmd   =
		cpu_to_scr((SCR_JUMP ^ IFFALSE (MASK (0x80+ln, 0xff))));
	lp->jump_lcb.l_paddr = tp->jump_lcb[lh].l_paddr;

	lp->load_jump_ccb[0] = cpu_to_scr(copy_4);
	lp->load_jump_ccb[1] = cpu_to_scr(vtobus (&lp->p_jump_ccb));
	lp->load_jump_ccb[2] = cpu_to_scr(ncr_reg_bus_addr(nc_temp));

	lp->jump_tag.l_cmd   = cpu_to_scr(SCR_JUMP);
	lp->jump_tag.l_paddr = cpu_to_scr(NCB_SCRIPT_PHYS (np, resel_notag));

	tp->jump_lcb[lh].l_paddr = cpu_to_scr(vtobus (&lp->jump_lcb));

	lp->busyccbs	= 1;
	lp->queuedccbs	= 1;
	lp->queuedepth	= 1;
fail:
	return lp;
}


static struct lcb *ncr_setup_lcb (struct ncb *np, struct scsi_device *sdev)
{
	unsigned char tn = sdev->id, ln = sdev->lun;
	struct tcb *tp = &np->target[tn];
	struct lcb *lp = tp->lp[ln];

	
	if (!lp && !(lp = ncr_alloc_lcb(np, tn, ln)))
		goto fail;

	if (sdev->tagged_supported && lp->jump_ccb == &lp->jump_ccb_0) {
		int i;
		lp->jump_ccb = m_calloc_dma(256, "JUMP_CCB");
		if (!lp->jump_ccb) {
			lp->jump_ccb = &lp->jump_ccb_0;
			goto fail;
		}
		lp->p_jump_ccb = cpu_to_scr(vtobus(lp->jump_ccb));
		for (i = 0 ; i < 64 ; i++)
			lp->jump_ccb[i] =
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, bad_i_t_l_q));
		for (i = 0 ; i < MAX_TAGS ; i++)
			lp->cb_tags[i] = i;
		lp->maxnxs = MAX_TAGS;
		lp->tags_stime = jiffies + 3*HZ;
		ncr_setup_tags (np, sdev);
	}


fail:
	return lp;
}



static int ncr_scatter(struct ncb *np, struct ccb *cp, struct scsi_cmnd *cmd)
{
	int segment	= 0;
	int use_sg	= scsi_sg_count(cmd);

	cp->data_len	= 0;

	use_sg = map_scsi_sg_data(np, cmd);
	if (use_sg > 0) {
		struct scatterlist *sg;
		struct scr_tblmove *data;

		if (use_sg > MAX_SCATTER) {
			unmap_scsi_data(np, cmd);
			return -1;
		}

		data = &cp->phys.data[MAX_SCATTER - use_sg];

		scsi_for_each_sg(cmd, sg, use_sg, segment) {
			dma_addr_t baddr = sg_dma_address(sg);
			unsigned int len = sg_dma_len(sg);

			ncr_build_sge(np, &data[segment], baddr, len);
			cp->data_len += len;
		}
	} else
		segment = -2;

	return segment;
}


static int __init ncr_regtest (struct ncb* np)
{
	register volatile u32 data;
	data = 0xffffffff;
	OUTL_OFF(offsetof(struct ncr_reg, nc_dstat), data);
	data = INL_OFF(offsetof(struct ncr_reg, nc_dstat));
#if 1
	if (data == 0xffffffff) {
#else
	if ((data & 0xe2f0fffd) != 0x02000080) {
#endif
		printk ("CACHE TEST FAILED: reg dstat-sstat2 readback %x.\n",
			(unsigned) data);
		return (0x10);
	}
	return (0);
}

static int __init ncr_snooptest (struct ncb* np)
{
	u32	ncr_rd, ncr_wr, ncr_bk, host_rd, host_wr, pc;
	int	i, err=0;
	if (np->reg) {
		err |= ncr_regtest (np);
		if (err)
			return (err);
	}

	
	pc  = NCB_SCRIPTH_PHYS (np, snooptest);
	host_wr = 1;
	ncr_wr  = 2;
	np->ncr_cache = cpu_to_scr(host_wr);
	OUTL (nc_temp, ncr_wr);
	OUTL_DSP (pc);
	for (i=0; i<NCR_SNOOP_TIMEOUT; i++)
		if (INB(nc_istat) & (INTF|SIP|DIP))
			break;
	pc = INL (nc_dsp);
	host_rd = scr_to_cpu(np->ncr_cache);
	ncr_rd  = INL (nc_scratcha);
	ncr_bk  = INL (nc_temp);
	ncr_chip_reset(np, 100);
	if (i>=NCR_SNOOP_TIMEOUT) {
		printk ("CACHE TEST FAILED: timeout.\n");
		return (0x20);
	}
	if (pc != NCB_SCRIPTH_PHYS (np, snoopend)+8) {
		printk ("CACHE TEST FAILED: script execution failed.\n");
		printk ("start=%08lx, pc=%08lx, end=%08lx\n", 
			(u_long) NCB_SCRIPTH_PHYS (np, snooptest), (u_long) pc,
			(u_long) NCB_SCRIPTH_PHYS (np, snoopend) +8);
		return (0x40);
	}
	if (host_wr != ncr_rd) {
		printk ("CACHE TEST FAILED: host wrote %d, ncr read %d.\n",
			(int) host_wr, (int) ncr_rd);
		err |= 1;
	}
	if (host_rd != ncr_wr) {
		printk ("CACHE TEST FAILED: ncr wrote %d, host read %d.\n",
			(int) ncr_wr, (int) host_rd);
		err |= 2;
	}
	if (ncr_bk != ncr_wr) {
		printk ("CACHE TEST FAILED: ncr wrote %d, read back %d.\n",
			(int) ncr_wr, (int) ncr_bk);
		err |= 4;
	}
	return (err);
}


static void ncr_selectclock(struct ncb *np, u_char scntl3)
{
	if (np->multiplier < 2) {
		OUTB(nc_scntl3,	scntl3);
		return;
	}

	if (bootverbose >= 2)
		printk ("%s: enabling clock multiplier\n", ncr_name(np));

	OUTB(nc_stest1, DBLEN);	   
	if (np->multiplier > 2) {  
		int i = 20;
		while (!(INB(nc_stest4) & LCKFRQ) && --i > 0)
			udelay(20);
		if (!i)
			printk("%s: the chip cannot lock the frequency\n", ncr_name(np));
	} else			
		udelay(20);
	OUTB(nc_stest3, HSC);		
	OUTB(nc_scntl3,	scntl3);
	OUTB(nc_stest1, (DBLEN|DBLSEL));
	OUTB(nc_stest3, 0x00);		
}


static unsigned __init ncrgetfreq (struct ncb *np, int gen)
{
	unsigned ms = 0;
	char count = 0;

	OUTB (nc_stest1, 0);	
	OUTW (nc_sien , 0);	
	(void) INW (nc_sist);	
	OUTB (nc_dien , 0);	
	(void) INW (nc_sist);	
	OUTB (nc_scntl3, 4);	
	OUTB (nc_stime1, 0);	
	OUTB (nc_stime1, gen);	
	while (!(INW(nc_sist) & GEN) && ms++ < 100000) {
		for (count = 0; count < 10; count ++)
			udelay(100);	
	}
	OUTB (nc_stime1, 0);	
 	OUTB (nc_scntl3, 0);

	if (bootverbose >= 2)
		printk ("%s: Delay (GEN=%d): %u msec\n", ncr_name(np), gen, ms);
	return ms ? ((1 << gen) * 4340) / ms : 0;
}

static void __init ncr_getclock (struct ncb *np, int mult)
{
	unsigned char scntl3 = INB(nc_scntl3);
	unsigned char stest1 = INB(nc_stest1);
	unsigned f1;

	np->multiplier = 1;
	f1 = 40000;

	if (mult > 1 && (stest1 & (DBLEN+DBLSEL)) == DBLEN+DBLSEL) {
		if (bootverbose >= 2)
			printk ("%s: clock multiplier found\n", ncr_name(np));
		np->multiplier = mult;
	}

	if (np->multiplier != mult || (scntl3 & 7) < 3 || !(scntl3 & 1)) {
		unsigned f2;

		ncr_chip_reset(np, 5);

		(void) ncrgetfreq (np, 11);	
		f1 = ncrgetfreq (np, 11);
		f2 = ncrgetfreq (np, 11);

		if(bootverbose)
			printk ("%s: NCR clock is %uKHz, %uKHz\n", ncr_name(np), f1, f2);

		if (f1 > f2) f1 = f2;		

		if	(f1 <	45000)		f1 =  40000;
		else if (f1 <	55000)		f1 =  50000;
		else				f1 =  80000;

		if (f1 < 80000 && mult > 1) {
			if (bootverbose >= 2)
				printk ("%s: clock multiplier assumed\n", ncr_name(np));
			np->multiplier	= mult;
		}
	} else {
		if	((scntl3 & 7) == 3)	f1 =  40000;
		else if	((scntl3 & 7) == 5)	f1 =  80000;
		else 				f1 = 160000;

		f1 /= np->multiplier;
	}

	f1		*= np->multiplier;
	np->clock_khz	= f1;
}


static int ncr53c8xx_slave_alloc(struct scsi_device *device)
{
	struct Scsi_Host *host = device->host;
	struct ncb *np = ((struct host_data *) host->hostdata)->ncb;
	struct tcb *tp = &np->target[device->id];
	tp->starget = device->sdev_target;

	return 0;
}

static int ncr53c8xx_slave_configure(struct scsi_device *device)
{
	struct Scsi_Host *host = device->host;
	struct ncb *np = ((struct host_data *) host->hostdata)->ncb;
	struct tcb *tp = &np->target[device->id];
	struct lcb *lp = tp->lp[device->lun];
	int numtags, depth_to_use;

	ncr_setup_lcb(np, device);

	numtags = device_queue_depth(np->unit, device->id, device->lun);
	if (numtags > tp->usrtags)
		numtags = tp->usrtags;
	if (!device->tagged_supported)
		numtags = 1;
	depth_to_use = numtags;
	if (depth_to_use < 2)
		depth_to_use = 2;
	if (depth_to_use > MAX_TAGS)
		depth_to_use = MAX_TAGS;

	scsi_adjust_queue_depth(device,
				(device->tagged_supported ?
				 MSG_SIMPLE_TAG : 0),
				depth_to_use);

	if (lp) {
		lp->numtags = lp->maxtags = numtags;
		lp->scdev_depth = depth_to_use;
	}
	ncr_setup_tags (np, device);

#ifdef DEBUG_NCR53C8XX
	printk("ncr53c8xx_select_queue_depth: host=%d, id=%d, lun=%d, depth=%d\n",
	       np->unit, device->id, device->lun, depth_to_use);
#endif

	if (spi_support_sync(device->sdev_target) &&
	    !spi_initial_dv(device->sdev_target))
		spi_dv_device(device);
	return 0;
}

static int ncr53c8xx_queue_command_lck (struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
{
     struct ncb *np = ((struct host_data *) cmd->device->host->hostdata)->ncb;
     unsigned long flags;
     int sts;

#ifdef DEBUG_NCR53C8XX
printk("ncr53c8xx_queue_command\n");
#endif

     cmd->scsi_done     = done;
     cmd->host_scribble = NULL;
     cmd->__data_mapped = 0;
     cmd->__data_mapping = 0;

     spin_lock_irqsave(&np->smp_lock, flags);

     if ((sts = ncr_queue_command(np, cmd)) != DID_OK) {
	  cmd->result = ScsiResult(sts, 0);
#ifdef DEBUG_NCR53C8XX
printk("ncr53c8xx : command not queued - result=%d\n", sts);
#endif
     }
#ifdef DEBUG_NCR53C8XX
     else
printk("ncr53c8xx : command successfully queued\n");
#endif

     spin_unlock_irqrestore(&np->smp_lock, flags);

     if (sts != DID_OK) {
          unmap_scsi_data(np, cmd);
          done(cmd);
	  sts = 0;
     }

     return sts;
}

static DEF_SCSI_QCMD(ncr53c8xx_queue_command)

irqreturn_t ncr53c8xx_intr(int irq, void *dev_id)
{
     unsigned long flags;
     struct Scsi_Host *shost = (struct Scsi_Host *)dev_id;
     struct host_data *host_data = (struct host_data *)shost->hostdata;
     struct ncb *np = host_data->ncb;
     struct scsi_cmnd *done_list;

#ifdef DEBUG_NCR53C8XX
     printk("ncr53c8xx : interrupt received\n");
#endif

     if (DEBUG_FLAGS & DEBUG_TINY) printk ("[");

     spin_lock_irqsave(&np->smp_lock, flags);
     ncr_exception(np);
     done_list     = np->done_list;
     np->done_list = NULL;
     spin_unlock_irqrestore(&np->smp_lock, flags);

     if (DEBUG_FLAGS & DEBUG_TINY) printk ("]\n");

     if (done_list)
	     ncr_flush_done_cmds(done_list);
     return IRQ_HANDLED;
}

static void ncr53c8xx_timeout(unsigned long npref)
{
	struct ncb *np = (struct ncb *) npref;
	unsigned long flags;
	struct scsi_cmnd *done_list;

	spin_lock_irqsave(&np->smp_lock, flags);
	ncr_timeout(np);
	done_list     = np->done_list;
	np->done_list = NULL;
	spin_unlock_irqrestore(&np->smp_lock, flags);

	if (done_list)
		ncr_flush_done_cmds(done_list);
}

static int ncr53c8xx_bus_reset(struct scsi_cmnd *cmd)
{
	struct ncb *np = ((struct host_data *) cmd->device->host->hostdata)->ncb;
	int sts;
	unsigned long flags;
	struct scsi_cmnd *done_list;


	spin_lock_irqsave(&np->smp_lock, flags);
	sts = ncr_reset_bus(np, cmd, 1);

	done_list     = np->done_list;
	np->done_list = NULL;
	spin_unlock_irqrestore(&np->smp_lock, flags);

	ncr_flush_done_cmds(done_list);

	return sts;
}

#if 0 
static int ncr53c8xx_abort(struct scsi_cmnd *cmd)
{
	struct ncb *np = ((struct host_data *) cmd->device->host->hostdata)->ncb;
	int sts;
	unsigned long flags;
	struct scsi_cmnd *done_list;

	printk("ncr53c8xx_abort\n");

	NCR_LOCK_NCB(np, flags);

	sts = ncr_abort_command(np, cmd);
out:
	done_list     = np->done_list;
	np->done_list = NULL;
	NCR_UNLOCK_NCB(np, flags);

	ncr_flush_done_cmds(done_list);

	return sts;
}
#endif



#define next_wcmd host_scribble

static void insert_into_waiting_list(struct ncb *np, struct scsi_cmnd *cmd)
{
	struct scsi_cmnd *wcmd;

#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx inserted into waiting list\n", ncr_name(np), (u_long) cmd);
#endif
	cmd->next_wcmd = NULL;
	if (!(wcmd = np->waiting_list)) np->waiting_list = cmd;
	else {
		while (wcmd->next_wcmd)
			wcmd = (struct scsi_cmnd *) wcmd->next_wcmd;
		wcmd->next_wcmd = (char *) cmd;
	}
}

static struct scsi_cmnd *retrieve_from_waiting_list(int to_remove, struct ncb *np, struct scsi_cmnd *cmd)
{
	struct scsi_cmnd **pcmd = &np->waiting_list;

	while (*pcmd) {
		if (cmd == *pcmd) {
			if (to_remove) {
				*pcmd = (struct scsi_cmnd *) cmd->next_wcmd;
				cmd->next_wcmd = NULL;
			}
#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx retrieved from waiting list\n", ncr_name(np), (u_long) cmd);
#endif
			return cmd;
		}
		pcmd = (struct scsi_cmnd **) &(*pcmd)->next_wcmd;
	}
	return NULL;
}

static void process_waiting_list(struct ncb *np, int sts)
{
	struct scsi_cmnd *waiting_list, *wcmd;

	waiting_list = np->waiting_list;
	np->waiting_list = NULL;

#ifdef DEBUG_WAITING_LIST
	if (waiting_list) printk("%s: waiting_list=%lx processing sts=%d\n", ncr_name(np), (u_long) waiting_list, sts);
#endif
	while ((wcmd = waiting_list) != NULL) {
		waiting_list = (struct scsi_cmnd *) wcmd->next_wcmd;
		wcmd->next_wcmd = NULL;
		if (sts == DID_OK) {
#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx trying to requeue\n", ncr_name(np), (u_long) wcmd);
#endif
			sts = ncr_queue_command(np, wcmd);
		}
		if (sts != DID_OK) {
#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx done forced sts=%d\n", ncr_name(np), (u_long) wcmd, sts);
#endif
			wcmd->result = ScsiResult(sts, 0);
			ncr_queue_done_cmd(np, wcmd);
		}
	}
}

#undef next_wcmd

static ssize_t show_ncr53c8xx_revision(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *host = class_to_shost(dev);
	struct host_data *host_data = (struct host_data *)host->hostdata;
  
	return snprintf(buf, 20, "0x%x\n", host_data->ncb->revision_id);
}
  
static struct device_attribute ncr53c8xx_revision_attr = {
	.attr	= { .name = "revision", .mode = S_IRUGO, },
	.show	= show_ncr53c8xx_revision,
};
  
static struct device_attribute *ncr53c8xx_host_attrs[] = {
	&ncr53c8xx_revision_attr,
	NULL
};

#ifdef	MODULE
char *ncr53c8xx;	
module_param(ncr53c8xx, charp, 0);
#endif

#ifndef MODULE
static int __init ncr53c8xx_setup(char *str)
{
	return sym53c8xx__setup(str);
}

__setup("ncr53c8xx=", ncr53c8xx_setup);
#endif


struct Scsi_Host * __init ncr_attach(struct scsi_host_template *tpnt,
					int unit, struct ncr_device *device)
{
	struct host_data *host_data;
	struct ncb *np = NULL;
	struct Scsi_Host *instance = NULL;
	u_long flags = 0;
	int i;

	if (!tpnt->name)
		tpnt->name	= SCSI_NCR_DRIVER_NAME;
	if (!tpnt->shost_attrs)
		tpnt->shost_attrs = ncr53c8xx_host_attrs;

	tpnt->queuecommand	= ncr53c8xx_queue_command;
	tpnt->slave_configure	= ncr53c8xx_slave_configure;
	tpnt->slave_alloc	= ncr53c8xx_slave_alloc;
	tpnt->eh_bus_reset_handler = ncr53c8xx_bus_reset;
	tpnt->can_queue		= SCSI_NCR_CAN_QUEUE;
	tpnt->this_id		= 7;
	tpnt->sg_tablesize	= SCSI_NCR_SG_TABLESIZE;
	tpnt->cmd_per_lun	= SCSI_NCR_CMD_PER_LUN;
	tpnt->use_clustering	= ENABLE_CLUSTERING;

	if (device->differential)
		driver_setup.diff_support = device->differential;

	printk(KERN_INFO "ncr53c720-%d: rev 0x%x irq %d\n",
		unit, device->chip.revision_id, device->slot.irq);

	instance = scsi_host_alloc(tpnt, sizeof(*host_data));
	if (!instance)
	        goto attach_error;
	host_data = (struct host_data *) instance->hostdata;

	np = __m_calloc_dma(device->dev, sizeof(struct ncb), "NCB");
	if (!np)
		goto attach_error;
	spin_lock_init(&np->smp_lock);
	np->dev = device->dev;
	np->p_ncb = vtobus(np);
	host_data->ncb = np;

	np->ccb = m_calloc_dma(sizeof(struct ccb), "CCB");
	if (!np->ccb)
		goto attach_error;

	
	np->unit	= unit;
	np->verbose	= driver_setup.verbose;
	sprintf(np->inst_name, "ncr53c720-%d", np->unit);
	np->revision_id	= device->chip.revision_id;
	np->features	= device->chip.features;
	np->clock_divn	= device->chip.nr_divisor;
	np->maxoffs	= device->chip.offset_max;
	np->maxburst	= device->chip.burst_max;
	np->myaddr	= device->host_id;

	
	np->script0 = m_calloc_dma(sizeof(struct script), "SCRIPT");
	if (!np->script0)
		goto attach_error;
	np->scripth0 = m_calloc_dma(sizeof(struct scripth), "SCRIPTH");
	if (!np->scripth0)
		goto attach_error;

	init_timer(&np->timer);
	np->timer.data     = (unsigned long) np;
	np->timer.function = ncr53c8xx_timeout;

	

	np->paddr	= device->slot.base;
	np->paddr2	= (np->features & FE_RAM) ? device->slot.base_2 : 0;

	if (device->slot.base_v)
		np->vaddr = device->slot.base_v;
	else
		np->vaddr = ioremap(device->slot.base_c, 128);

	if (!np->vaddr) {
		printk(KERN_ERR
			"%s: can't map memory mapped IO region\n",ncr_name(np));
		goto attach_error;
	} else {
		if (bootverbose > 1)
			printk(KERN_INFO
				"%s: using memory mapped IO at virtual address 0x%lx\n", ncr_name(np), (u_long) np->vaddr);
	}


	np->reg = (struct ncr_reg __iomem *)np->vaddr;

	
	ncr_prepare_setting(np);

	if (np->paddr2 && sizeof(struct script) > 4096) {
		np->paddr2 = 0;
		printk(KERN_WARNING "%s: script too large, NOT using on chip RAM.\n",
			ncr_name(np));
	}

	instance->max_channel	= 0;
	instance->this_id       = np->myaddr;
	instance->max_id	= np->maxwide ? 16 : 8;
	instance->max_lun	= SCSI_NCR_MAX_LUN;
	instance->base		= (unsigned long) np->reg;
	instance->irq		= device->slot.irq;
	instance->unique_id	= device->slot.base;
	instance->dma_channel	= 0;
	instance->cmd_per_lun	= MAX_TAGS;
	instance->can_queue	= (MAX_START-4);
	BUG_ON(!ncr53c8xx_transport_template);
	instance->transportt	= ncr53c8xx_transport_template;

	
	ncr_script_fill(&script0, &scripth0);

	np->scripth	= np->scripth0;
	np->p_scripth	= vtobus(np->scripth);
	np->p_script	= (np->paddr2) ?  np->paddr2 : vtobus(np->script0);

	ncr_script_copy_and_bind(np, (ncrcmd *) &script0,
			(ncrcmd *) np->script0, sizeof(struct script));
	ncr_script_copy_and_bind(np, (ncrcmd *) &scripth0,
			(ncrcmd *) np->scripth0, sizeof(struct scripth));
	np->ccb->p_ccb	= vtobus (np->ccb);

	

	if (np->features & FE_LED0) {
		np->script0->idle[0]  =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_OR,  0x01));
		np->script0->reselected[0] =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_AND, 0xfe));
		np->script0->start[0] =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_AND, 0xfe));
	}

	for (i = 0 ; i < 4 ; i++) {
		np->jump_tcb[i].l_cmd   =
				cpu_to_scr((SCR_JUMP ^ IFTRUE (MASK (i, 3))));
		np->jump_tcb[i].l_paddr =
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, bad_target));
	}

	ncr_chip_reset(np, 100);

	

	if (ncr_snooptest(np)) {
		printk(KERN_ERR "CACHE INCORRECTLY CONFIGURED.\n");
		goto attach_error;
	}

	
	np->irq = device->slot.irq;

	
	ncr_init_ccb(np, np->ccb);

	spin_lock_irqsave(&np->smp_lock, flags);
	if (ncr_reset_scsi_bus(np, 0, driver_setup.settle_delay) != 0) {
		printk(KERN_ERR "%s: FATAL ERROR: CHECK SCSI BUS - CABLES, TERMINATION, DEVICE POWER etc.!\n", ncr_name(np));

		spin_unlock_irqrestore(&np->smp_lock, flags);
		goto attach_error;
	}
	ncr_exception(np);

	np->disc = 1;

	if (driver_setup.settle_delay > 2) {
		printk(KERN_INFO "%s: waiting %d seconds for scsi devices to settle...\n",
			ncr_name(np), driver_setup.settle_delay);
		mdelay(1000 * driver_setup.settle_delay);
	}

	
	np->lasttime=0;
	ncr_timeout (np);

	
#ifdef SCSI_NCR_ALWAYS_SIMPLE_TAG
	np->order = SIMPLE_QUEUE_TAG;
#endif

	spin_unlock_irqrestore(&np->smp_lock, flags);

	return instance;

 attach_error:
	if (!instance)
		return NULL;
	printk(KERN_INFO "%s: detaching...\n", ncr_name(np));
	if (!np)
		goto unregister;
	if (np->scripth0)
		m_free_dma(np->scripth0, sizeof(struct scripth), "SCRIPTH");
	if (np->script0)
		m_free_dma(np->script0, sizeof(struct script), "SCRIPT");
	if (np->ccb)
		m_free_dma(np->ccb, sizeof(struct ccb), "CCB");
	m_free_dma(np, sizeof(struct ncb), "NCB");
	host_data->ncb = NULL;

 unregister:
	scsi_host_put(instance);

	return NULL;
}


void ncr53c8xx_release(struct Scsi_Host *host)
{
	struct host_data *host_data = shost_priv(host);
#ifdef DEBUG_NCR53C8XX
	printk("ncr53c8xx: release\n");
#endif
	if (host_data->ncb)
		ncr_detach(host_data->ncb);
	scsi_host_put(host);
}

static void ncr53c8xx_set_period(struct scsi_target *starget, int period)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ncb *np = ((struct host_data *)shost->hostdata)->ncb;
	struct tcb *tp = &np->target[starget->id];

	if (period > np->maxsync)
		period = np->maxsync;
	else if (period < np->minsync)
		period = np->minsync;

	tp->usrsync = period;

	ncr_negotiate(np, tp);
}

static void ncr53c8xx_set_offset(struct scsi_target *starget, int offset)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ncb *np = ((struct host_data *)shost->hostdata)->ncb;
	struct tcb *tp = &np->target[starget->id];

	if (offset > np->maxoffs)
		offset = np->maxoffs;
	else if (offset < 0)
		offset = 0;

	tp->maxoffs = offset;

	ncr_negotiate(np, tp);
}

static void ncr53c8xx_set_width(struct scsi_target *starget, int width)
{
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct ncb *np = ((struct host_data *)shost->hostdata)->ncb;
	struct tcb *tp = &np->target[starget->id];

	if (width > np->maxwide)
		width = np->maxwide;
	else if (width < 0)
		width = 0;

	tp->usrwide = width;

	ncr_negotiate(np, tp);
}

static void ncr53c8xx_get_signalling(struct Scsi_Host *shost)
{
	struct ncb *np = ((struct host_data *)shost->hostdata)->ncb;
	enum spi_signal_type type;

	switch (np->scsi_mode) {
	case SMODE_SE:
		type = SPI_SIGNAL_SE;
		break;
	case SMODE_HVD:
		type = SPI_SIGNAL_HVD;
		break;
	default:
		type = SPI_SIGNAL_UNKNOWN;
		break;
	}
	spi_signalling(shost) = type;
}

static struct spi_function_template ncr53c8xx_transport_functions =  {
	.set_period	= ncr53c8xx_set_period,
	.show_period	= 1,
	.set_offset	= ncr53c8xx_set_offset,
	.show_offset	= 1,
	.set_width	= ncr53c8xx_set_width,
	.show_width	= 1,
	.get_signalling	= ncr53c8xx_get_signalling,
};

int __init ncr53c8xx_init(void)
{
	ncr53c8xx_transport_template = spi_attach_transport(&ncr53c8xx_transport_functions);
	if (!ncr53c8xx_transport_template)
		return -ENODEV;
	return 0;
}

void ncr53c8xx_exit(void)
{
	spi_release_transport(ncr53c8xx_transport_template);
}
