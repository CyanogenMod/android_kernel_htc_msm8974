/* 
* Copyright (C) ST-Ericsson AP Pte Ltd 2010 
*
* ISP1763 Linux OTG Controller driver : host
* 
* This program is free software; you can redistribute it and/or modify it under the terms of 
* the GNU General Public License as published by the Free Software Foundation; version 
* 2 of the License. 
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY  
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more  
* details. 
* 
* You should have received a copy of the GNU General Public License 
* along with this program; if not, write to the Free Software 
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
* 
* Refer to the follwing files in ~/drivers/usb/host for copyright owners:
* ehci-dbg.c, ehci-hcd.c, ehci-hub.c, ehci-mem.c, ehci-q.c and ehic-sched.c (kernel version 2.6.9)
* Code is modified for ST-Ericsson product 
* 
* Author : wired support <wired.support@stericsson.com>
*
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/usb.h>
#include <linux/version.h>
#include <stdarg.h>
#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/unaligned.h>
#include <linux/version.h>

#include "../hal/isp1763.h"
#include "pehci.h"
#include "../hal/hal_intf.h"
#include <linux/platform_device.h>
#include <linux/wakelock.h>

extern int HostComplianceTest;
extern int HostTest;
extern int No_Data_Phase;
extern int No_Status_Phase;
#define	EHCI_TUNE_CERR		3
#define	URB_NO_INTERRUPT	0x0080
#define	EHCI_TUNE_RL_TT		0
#define	EHCI_TUNE_MULT_TT	1
#define	EHCI_TUNE_RL_HS		0
#define	EHCI_TUNE_MULT_HS	1


#define POWER_DOWN_CTRL_NORMAL_VALUE	0xffff1ba0
#define POWER_DOWN_CTRL_SUSPEND_VALUE	0xffff08b0


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
#define	USB_PORT_FEAT_HIGHSPEED 10
#endif

#ifdef CONFIG_ISO_SUPPORT

#define	FALSE 0
#define	TRUE (!FALSE)
extern void *phcd_iso_sitd_to_ptd(phci_hcd * hcd,
	struct ehci_sitd *sitd,
	struct urb *urb, void *ptd);
extern void *phcd_iso_itd_to_ptd(phci_hcd * hcd,
	struct	ehci_itd *itd,
	struct	urb *urb, void *ptd);

extern unsigned	long phcd_submit_iso(phci_hcd *	hcd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	struct usb_host_endpoint *ep,
#else
#endif
	struct urb *urb, unsigned long *status);
void pehci_hcd_iso_schedule(phci_hcd * hcd, struct urb *);
unsigned long lgFrameIndex = 0;
unsigned long lgScheduledPTDIndex = 0;
int igNumOfPkts = 0;
#endif 

struct isp1763_dev *isp1763_hcd;

#ifdef HCD_PACKAGE
struct fasync_struct *fasync_q;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static void
pehci_hcd_urb_complete(phci_hcd * hcd, struct ehci_qh *qh, struct urb *urb,
	td_ptd_map_t * td_ptd_map, struct pt_regs *regs);
#else
static void
pehci_hcd_urb_complete(phci_hcd * hcd, struct ehci_qh *qh, struct urb *urb,
	td_ptd_map_t * td_ptd_map);
#endif

#include "otg.c"  


int hcdpowerdown = 0;
int portchange=0; 
EXPORT_SYMBOL(hcdpowerdown);
unsigned char otg_se0_enable;
EXPORT_SYMBOL(otg_se0_enable);



#ifdef MSEC_INT_BASED
#ifdef THREAD_BASED
#define INTR_ENABLE_MASK ( HC_INTL_INT | HC_ATL_INT| HC_ISO_INT )
#else
#define	INTR_ENABLE_MASK (HC_MSEC_INT|HC_OPR_REG_INT|HC_CLK_RDY_INT )
#endif
#else
#define	INTR_ENABLE_MASK ( HC_INTL_INT | HC_ATL_INT |HC_ISO_INT| HC_EOT_INT|HC_OPR_REG_INT|HC_CLK_RDY_INT)
#endif



#ifdef THREAD_BASED

#define NO_SOF_REQ_IN_TSK 		0x1
#define NO_SOF_REQ_IN_ISR 		0x2
#define NO_SOF_REQ_IN_REQ 	0x3
#define MSEC_INTERVAL_CHECKING 5

typedef struct _st_UsbIt_Msg_Struc {
	struct usb_hcd 		*usb_hcd;
	u8				uIntStatus;
	struct list_head 		list;
} st_UsbIt_Msg_Struc, *pst_UsbIt_Msg_Struc ;

typedef struct _st_UsbIt_Thread {
    wait_queue_head_t       	ulThrdWaitQhead;
    int                           		lThrdWakeUpNeeded;
    struct task_struct           	*phThreadTask;
    spinlock_t              lock;
} st_UsbIt_Thread, *pst_UsbIt_Thread;

st_UsbIt_Thread g_stUsbItThreadHandler;

st_UsbIt_Msg_Struc 	g_messList;
st_UsbIt_Msg_Struc 	g_enqueueMessList;
spinlock_t              	enqueue_lock;

int pehci_hcd_process_irq_it_handle(struct usb_hcd* usb_hcd_);
int pehci_hcd_process_irq_in_thread(struct usb_hcd *usb_hcd_);

#endif 

#ifdef THREAD_BASED
phci_hcd *g_pehci_hcd;
#endif


struct wake_lock pehci_wake_lock;


static spinlock_t hcd_data_lock	= SPIN_LOCK_UNLOCKED;

static const char hcd_name[] = "ST-Ericsson ISP1763";
static td_ptd_map_buff_t td_ptd_map_buff[TD_PTD_TOTAL_BUFF_TYPES];	

static u8 td_ptd_pipe_x_buff_type[TD_PTD_TOTAL_BUFF_TYPES] = {
	TD_PTD_BUFF_TYPE_ATL,
	TD_PTD_BUFF_TYPE_INTL,
	TD_PTD_BUFF_TYPE_ISTL
};


isp1763_mem_addr_t memalloc[BLK_TOTAL];
#include "mem.c"
#include "qtdptd.c"

#ifdef CONFIG_ISO_SUPPORT
#include "itdptd.c"
#endif 

static int
pehci_rh_control(struct	usb_hcd	*usb_hcd, u16 typeReq,
		 u16 wValue, u16 wIndex, char *buf, u16	wLength);

static int pehci_bus_suspend(struct usb_hcd *usb_hcd);
static int pehci_bus_resume(struct usb_hcd *usb_hcd);
static void
pehci_complete_device_removal(phci_hcd * hcd, struct ehci_qh *qh)
{
	td_ptd_map_t *td_ptd_map;
	td_ptd_map_buff_t *td_ptd_buff;
	struct urb * urb;
	urb_priv_t *urb_priv;
	struct ehci_qtd	*qtd = 0;
	u16 skipmap=0;

	if (qh->type ==	TD_PTD_BUFF_TYPE_ISTL) {
#ifdef COMMON_MEMORY
		phci_hcd_mem_free(&qh->memory_addr);
#endif
		return;
	}

	td_ptd_buff = &td_ptd_map_buff[qh->type];
	td_ptd_map = &td_ptd_buff->map_list[qh->qtd_ptd_index];

	
	td_ptd_map->state = TD_PTD_REMOVE;
	
	if (list_empty(&qh->qtd_list)) {
		if (td_ptd_map->state != TD_PTD_NEW) {
			phci_hcd_release_td_ptd_index(qh);
		}
		qha_free(qha_cache, qh);
		qh = 0;
		return;
	} else {
	
		if(!list_empty(&qh->qtd_list)){
				qtd=NULL;
				qtd = list_entry(qh->qtd_list.next, struct ehci_qtd, qtd_list);
				if(qtd){
					urb=qtd->urb;
					urb_priv= urb->hcpriv;
					
					if(urb)
					switch (usb_pipetype(urb->pipe)) {
						case PIPE_CONTROL:
						case PIPE_BULK:
							break;
						case PIPE_INTERRUPT:
							td_ptd_buff = &td_ptd_map_buff[TD_PTD_BUFF_TYPE_INTL];
							td_ptd_map = &td_ptd_buff->map_list[qh->qtd_ptd_index];

							
						
						
						
						

							
							td_ptd_buff->pending_ptd_bitmap &= ~td_ptd_map->ptd_bitmap;

							td_ptd_map->state = TD_PTD_REMOVE;
							urb_priv->state	|= DELETE_URB;

							
							skipmap	=
							isp1763_reg_read16(hcd->dev, hcd->regs.inttdskipmap,
							skipmap);

							isp1763_reg_write16(hcd->dev, hcd->regs.inttdskipmap,
							skipmap | td_ptd_map->ptd_bitmap);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
							pehci_hcd_urb_complete(hcd, qh, urb, td_ptd_map, NULL);
#else
							pehci_hcd_urb_complete(hcd, qh, urb, td_ptd_map);
#endif
							break;
					}

					
				}else{
					
				}
		}
		qha_free(qha_cache, qh);
		qh = 0;
		return;
	}
	
	err("Never Error: Should not come to this portion of code\n");

	return;
}

static int
pehci_hcd_handshake(phci_hcd * hcd, u32	ptr, u32 mask, u32 done, int usec)
{
	u32 result = 0;
	do {
		result = isp1763_reg_read16(hcd->dev, ptr, result);
		printk(KERN_NOTICE "Registr %x val is %x\n", ptr, result);
		if (result == ~(u32) 0)	{
			return -ENODEV;
		}
		result &= mask;
		if (result == done) {
			return 0;
		}
		udelay(1);
		usec--;
	} while	(usec >	0);

	return -ETIMEDOUT;
}

#ifndef	MSEC_INT_BASED
static void
pehci_hcd_td_ptd_submit_urb(phci_hcd * hcd, struct ehci_qh *qh,	u8 bufftype)
{
	unsigned long flags=0;
	struct ehci_qtd	*qtd = 0;
	struct urb *urb	= 0;
	struct _isp1763_qha *qha = 0;
	u16 location = 0;
	u16 skipmap = 0;
	u16 buffstatus = 0;
	u16 ormask = 0;
	u16 intormask =	0;
	u32 length = 0;
	struct list_head *head;

	td_ptd_map_t *td_ptd_map;
	td_ptd_map_buff_t *ptd_map_buff;
	struct isp1763_mem_addr	*mem_addr = 0;

	pehci_entry("++	%s: Entered\n",	__FUNCTION__);
	pehci_print("Buuffer type %d\n", bufftype);

	spin_lock_irqsave(&hcd->lock, flags);
	ptd_map_buff = &td_ptd_map_buff[bufftype];

	qha = &hcd->qha;

	switch (bufftype) {
	case TD_PTD_BUFF_TYPE_ATL:

		skipmap	=
			isp1763_reg_read16(hcd->dev, hcd->regs.atltdskipmap,
					   skipmap);

		ormask = isp1763_reg_read16(hcd->dev, hcd->regs.atl_irq_mask_or,
					    ormask);
		break;
	case TD_PTD_BUFF_TYPE_INTL:

		skipmap	=
			isp1763_reg_read16(hcd->dev, hcd->regs.inttdskipmap,
					   skipmap);

		intormask =
			isp1763_reg_read16(hcd->dev, hcd->regs.int_irq_mask_or,
					   intormask);
		break;
	default:

		skipmap	=
			isp1763_reg_read16(hcd->dev, hcd->regs.isotdskipmap,
					   skipmap);
		break;

	}


	buffstatus =
		isp1763_reg_read16(hcd->dev, hcd->regs.buffer_status,
				   buffstatus);

	
	location = qh->qtd_ptd_index;
	td_ptd_map = &ptd_map_buff->map_list[location];

	if (!(qh->qh_state & QH_STATE_TAKE_NEXT)) {
		pehci_check("qh	will schdule from interrupt routine,map	%x\n",
			    td_ptd_map->ptd_bitmap);
		spin_unlock_irqrestore(&hcd->lock, flags);
		return;
	}
	head = &qh->qtd_list;
	qtd = list_entry(head->next, struct ehci_qtd, qtd_list);

	
	if (!(qtd->state & QTD_STATE_NEW)) {
		pehci_check("qtd already in, state %x\n", qtd->state);
		spin_unlock_irqrestore(&hcd->lock, flags);
		return;
	}

	qtd->state &= ~QTD_STATE_NEW;
	qtd->state |= QTD_STATE_SCHEDULED;

	qh->qh_state &=	~QH_STATE_TAKE_NEXT;
	
	td_ptd_map->qtd	= qtd;
	
	urb = qtd->urb;
	ptd_map_buff->active_ptds++;

	
	
	if (qtd->state & QTD_STATE_LAST) {
		qh->hw_current = cpu_to_le32(0);
		
	} else {
		qh->hw_current = qtd->hw_next;
	}
	memset(qha, 0, sizeof(isp1763_qha));

	pehci_check("td	being scheduled	: length: %d, device: %d, map: %x\n",
		    qtd->length, urb->dev->devnum, td_ptd_map->ptd_bitmap);
	
	length = qtd->length;
	mem_addr = &qtd->mem_addr;
	phci_hcd_mem_alloc(length, mem_addr, 0);
	if (length && ((mem_addr->phy_addr == 0) || (mem_addr->virt_addr == 0))) {
		err("Never Error: Can not allocate memory for the current td,length %d\n", length);
		
	}
	phci_hcd_qha_from_qtd(hcd, qtd, qtd->urb, (void *) qha,
		td_ptd_map->ptd_ram_data_addr, qh);
	if (qh->type ==	TD_PTD_BUFF_TYPE_INTL) {
		phci_hcd_qhint_schedule(hcd, qh, qtd, (isp1763_qhint *)	qha,
					qtd->urb);
	}
	
	isp1763_mem_write(hcd->dev, td_ptd_map->ptd_header_addr, 0,
			  (u32 *) (qha), PHCI_QHA_LENGTH, 0);

	
	
	if (qtd->length && (qtd->length <= HC_ATL_PL_SIZE)){
		switch (PTD_PID(qha->td_info2))	{
		case OUT_PID:
		case SETUP_PID:

			isp1763_mem_write(hcd->dev, (u32) mem_addr->phy_addr, 0,
					  (void	*) qtd->hw_buf[0], length, 0);


#if 0
					int i=0;
					int *data_addr= qtd->hw_buf[0];
					printk("\n");
					for(i=0;i<length;i+=4) printk("[0x%X] ",*data_addr++);
					printk("\n");
#endif

			

			break;
		}
	}

	
	switch (bufftype) {
	case TD_PTD_BUFF_TYPE_ATL:
		skipmap	&= ~td_ptd_map->ptd_bitmap;
		
		ormask |= td_ptd_map->ptd_bitmap;

		isp1763_reg_write16(hcd->dev, hcd->regs.atl_irq_mask_or,
				    ormask);
		break;

	case TD_PTD_BUFF_TYPE_INTL:
		skipmap	&= ~td_ptd_map->ptd_bitmap;
		intormask |= td_ptd_map->ptd_bitmap;

		isp1763_reg_write16(hcd->dev, hcd->regs.int_irq_mask_or,
				    intormask);
		break;

	case TD_PTD_BUFF_TYPE_ISTL:
		skipmap	&= ~td_ptd_map->ptd_bitmap;

		isp1763_reg_write16(hcd->dev, hcd->regs.isotdskipmap, skipmap);
		break;
	}

	
	switch (bufftype) {
	case TD_PTD_BUFF_TYPE_ATL:

		isp1763_reg_write16(hcd->dev, hcd->regs.buffer_status,
				    buffstatus | ATL_BUFFER);

		isp1763_reg_write16(hcd->dev, hcd->regs.atltdskipmap, skipmap);
		buffstatus |= ATL_BUFFER;
		break;
	case TD_PTD_BUFF_TYPE_INTL:

		isp1763_reg_write16(hcd->dev, hcd->regs.buffer_status,
				    buffstatus | INT_BUFFER);

		isp1763_reg_write16(hcd->dev, hcd->regs.inttdskipmap, skipmap);
		break;
	case TD_PTD_BUFF_TYPE_ISTL:
		

		isp1763_reg_write16(hcd->dev, hcd->regs.buffer_status,
				    buffstatus | ISO_BUFFER);
		break;
	}
	spin_unlock_irqrestore(&hcd->lock, flags);
	pehci_entry("--	%s: Exit\n", __FUNCTION__);
	return;

}
#endif



#ifdef MSEC_INT_BASED
static void
pehci_hcd_schedule_pending_ptds(phci_hcd * hcd, u16 donemap, u8 bufftype,
				u16 only)
{
	struct ehci_qtd	*qtd = 0;
	struct ehci_qh *qh = 0;
	struct list_head *qtd_list = 0;
	struct _isp1763_qha allqha;
	struct _isp1763_qha *qha = 0;
	u16 mask = 0x1,	index =	0;
	u16 location = 0;
	u16 skipmap = 0;
	u32 newschedule	= 0;
	u16 buffstatus = 0;
	u16 schedulemap	= 0;
#ifndef	CONFIG_ISO_SUPPORT
	u16 lasttd = 1;
#endif
	u16 lastmap = 0;
	struct urb *urb	= 0;
	urb_priv_t *urbpriv = 0;
	int length = 0;
	u16 ormask = 0,	andmask	= 0;
	u16 intormask =	0;
	td_ptd_map_t *td_ptd_map;
	td_ptd_map_buff_t *ptd_map_buff;
	struct isp1763_mem_addr	*mem_addr = 0;

	pehci_entry("++	%s: Entered\n",	__FUNCTION__);
	pehci_print("Buffer type %d\n",	bufftype);

	spin_lock(&hcd_data_lock);
	ptd_map_buff = &td_ptd_map_buff[bufftype];
	qha = &allqha;
	switch (bufftype) {
	case TD_PTD_BUFF_TYPE_ATL:

		skipmap	=
			isp1763_reg_read16(hcd->dev, hcd->regs.atltdskipmap,
					   skipmap);
		rmb();

		ormask = isp1763_reg_read16(hcd->dev, hcd->regs.atl_irq_mask_or,
					    ormask);

		andmask	=
			isp1763_reg_read16(hcd->dev, hcd->regs.atl_irq_mask_and,
					   andmask);
		break;
	case TD_PTD_BUFF_TYPE_INTL:

		skipmap	=
			isp1763_reg_read16(hcd->dev, hcd->regs.inttdskipmap,
					   skipmap);
		

		intormask =
			isp1763_reg_read16(hcd->dev, hcd->regs.int_irq_mask_or,
					   intormask);
		break;
	default:
		err("Never Error: Bogus	type of	bufer\n");
		return;
	}

	buffstatus =
		isp1763_reg_read16(hcd->dev, hcd->regs.buffer_status,
				   buffstatus);
	
	schedulemap = donemap;
	while (schedulemap) {
		index =	schedulemap & mask;
		schedulemap &= ~mask;
		mask <<= 1;

		if (!index) {
			location++;
			continue;
		}

		td_ptd_map = &ptd_map_buff->map_list[location];
		if ((td_ptd_map->state == TD_PTD_NEW) ||
			(td_ptd_map->state == TD_PTD_REMOVE)) {
			qh = td_ptd_map->qh;
			pehci_check
				("should not come here,	map %x,pending map %x\n",
				 td_ptd_map->ptd_bitmap,
				 ptd_map_buff->pending_ptd_bitmap);

			pehci_check("buffer type %s\n",
				(bufftype == 0) ? "ATL" : "INTL");
			donemap	&= ~td_ptd_map->ptd_bitmap;
			
			ptd_map_buff->pending_ptd_bitmap &=
				~td_ptd_map->ptd_bitmap;
			location++;
			continue;
		}

		
		if (!(td_ptd_map->qh)) {
			err("queue head	can not	be null	here\n");
			
			ptd_map_buff->pending_ptd_bitmap &=
				~td_ptd_map->ptd_bitmap;
			location++;
			continue;
		}

		
		qh = td_ptd_map->qh;
		if (!(skipmap &	td_ptd_map->ptd_bitmap)) {
			
			pehci_check("buffertype	%d,td_ptd_map %x,skipnap %x\n",
				    bufftype, td_ptd_map->ptd_bitmap, skipmap);
			lastmap	= td_ptd_map->ptd_bitmap;
			donemap	&= ~td_ptd_map->ptd_bitmap;
			ptd_map_buff->pending_ptd_bitmap &=
				~td_ptd_map->ptd_bitmap;
			location++;
			continue;
		}

		
		if (td_ptd_map->lasttd)	{
			err("should not	show  map %x,qtd %p\n",
			td_ptd_map->ptd_bitmap, td_ptd_map->qtd);
			qh->hw_current = cpu_to_le32(td_ptd_map->qtd);
			ptd_map_buff->pending_ptd_bitmap &=
				~td_ptd_map->ptd_bitmap;
			location++;
			continue;
		}

		
		if ((td_ptd_map->qtd) && (td_ptd_map->state & TD_PTD_RELOAD)) {
			warn("%s: reload td\n",	__FUNCTION__);
			td_ptd_map->state &= ~TD_PTD_RELOAD;
			qtd = td_ptd_map->qtd;
			goto loadtd;
		}

		
		if ((td_ptd_map->qh) &&	!(td_ptd_map->qtd)) {
			if (list_empty(&qh->qtd_list)) {
				pehci_check
					("must not come	here any more, td map %x\n",
					 td_ptd_map->ptd_bitmap);
				donemap	&= ~td_ptd_map->ptd_bitmap;
				td_ptd_map->state |= TD_PTD_IDLE;
				ptd_map_buff->pending_ptd_bitmap &=
					~td_ptd_map->ptd_bitmap;
				location++;
				continue;
			}
			qtd_list = &qh->qtd_list;
			qtd = td_ptd_map->qtd =
				list_entry(qtd_list->next, struct ehci_qtd,
					   qtd_list);
			
			goto loadtd;
		}

		
		if (td_ptd_map->qtd) {
			
			qtd = td_ptd_map->qtd;
		}
		loadtd:
		
		if (!qtd) {
			err("this piece	of code	should not be executed\n");
			ptd_map_buff->pending_ptd_bitmap &=
				~td_ptd_map->ptd_bitmap;
			location++;
			continue;
		}

		ptd_map_buff->active_ptds++;
		
		ptd_map_buff->pending_ptd_bitmap &= ~td_ptd_map->ptd_bitmap;



		
		if (qtd->state & QTD_STATE_LAST) {
			
			qh->hw_current = cpu_to_le32(0);

			
		} else {
			qh->hw_current = qtd->hw_next;
		}

		if (location !=	qh->qtd_ptd_index) {
			err("Never Error: Endpoint header location and scheduling information are not same\n");
		}

		
		location++;
		
		newschedule = 1;
		
		urb = qtd->urb;
		if (!(qtd->state & QTD_STATE_NEW)) {
			err("Never Error: We should not	put the	same stuff\n");
			continue;
		}

		urbpriv	= (urb_priv_t *) urb->hcpriv;
		urbpriv->timeout = 0;

		
		qtd->state &= ~QTD_STATE_NEW;
		qtd->state |= QTD_STATE_SCHEDULED;



		
		length = qtd->length;
		mem_addr = &qtd->mem_addr;
		phci_hcd_mem_alloc(length, mem_addr, 0);
		if (length && ((mem_addr->phy_addr == 0)
			       || (mem_addr->virt_addr == 0))) {

			err("Never Error: Can not allocate memory for the current td,length %d\n", length);
			location++;
			continue;
		}

		pehci_check("qtd being scheduled %p, device %d,map %x\n", qtd,
			    urb->dev->devnum, td_ptd_map->ptd_bitmap);


		memset(qha, 0, sizeof(isp1763_qha));
		
		phci_hcd_qha_from_qtd(hcd, qtd,	qtd->urb, (void	*) qha,
			td_ptd_map->ptd_ram_data_addr, qh);

		if (qh->type ==	TD_PTD_BUFF_TYPE_INTL) {
			phci_hcd_qhint_schedule(hcd, qh, qtd,
				(isp1763_qhint *) qha,
				qtd->urb);

		}


		length = PTD_XFERRED_LENGTH(qha->td_info1 >> 3);
		if (length > HC_ATL_PL_SIZE) {
			err("Never Error: Bogus	length,length %d(max %d)\n",
			qtd->length, HC_ATL_PL_SIZE);
		}

		
		isp1763_mem_write(hcd->dev, td_ptd_map->ptd_header_addr, 0,
			(u32 *) (qha), PHCI_QHA_LENGTH, 0);

#ifdef PTD_DUMP_SCHEDULE
		printk("SCHEDULE next (atl/int)tds PTD header\n");
		printk("DW0: 0x%08X\n", qha->td_info1);
		printk("DW1: 0x%08X\n", qha->td_info2);
		printk("DW2: 0x%08X\n", qha->td_info3);
		printk("DW3: 0x%08X\n", qha->td_info4);
#endif
		
		
		
		if (qtd->length && (length <= HC_ATL_PL_SIZE)){
			switch (PTD_PID(qha->td_info2))	{
			case OUT_PID:
			case SETUP_PID:

				isp1763_mem_write(hcd->dev,
					(u32)	mem_addr->phy_addr, 0,
					(void	*) qtd->hw_buf[0],
					length, 0);
#if 0
					int i=0;
					int *data_addr= qtd->hw_buf[0];
					printk("\n");
					for(i=0;i<length;i+=4) printk("[0x%X] ",*data_addr++);
					printk("\n");
#endif



				break;
			}
		}

		
		switch (bufftype) {
		case TD_PTD_BUFF_TYPE_ATL:
			skipmap	&= ~td_ptd_map->ptd_bitmap;
			lastmap	= td_ptd_map->ptd_bitmap;
			
			ormask |= td_ptd_map->ptd_bitmap;

			isp1763_reg_write16(hcd->dev, hcd->regs.atl_irq_mask_or,
					    ormask);
			break;

		case TD_PTD_BUFF_TYPE_INTL:
			skipmap	&= ~td_ptd_map->ptd_bitmap;
			lastmap	= td_ptd_map->ptd_bitmap;
			intormask |= td_ptd_map->ptd_bitmap;
			;
			isp1763_reg_write16(hcd->dev, hcd->regs.int_irq_mask_or,
					    intormask);
			break;

		case TD_PTD_BUFF_TYPE_ISTL:
#ifdef CONFIG_ISO_SUPPORT
			iso_dbg(ISO_DBG_INFO,
				"Never Error: Should not come here\n");
#else
			skipmap	&= ~td_ptd_map->ptd_bitmap;

			isp1763_reg_write16(hcd->dev, hcd->regs.isotdskipmap,
					    skipmap);

			isp1763_reg_write16(hcd->dev, hcd->regs.isotdlastmap,
				lasttd);
#endif 
			break;
		}


	}
	

	if (newschedule) {
		switch (bufftype) {
		case TD_PTD_BUFF_TYPE_ATL:

			isp1763_reg_write16(hcd->dev, hcd->regs.buffer_status,
					    buffstatus | ATL_BUFFER);
			
			
			if (skipmap & donemap) {
				pehci_check
					("must be both ones compliment of each other\n");
				pehci_check
					("problem, skipmap %x, donemap %x,\n",
					 skipmap, donemap);

			}
			skipmap	&= ~donemap;

			isp1763_reg_write16(hcd->dev, hcd->regs.atltdskipmap,
					    skipmap);

			break;
		case TD_PTD_BUFF_TYPE_INTL:

			isp1763_reg_write16(hcd->dev, hcd->regs.buffer_status,
					    buffstatus | INT_BUFFER);
			skipmap	&= ~donemap;

			isp1763_reg_write16(hcd->dev, hcd->regs.inttdskipmap,
					    skipmap);
			break;
		case TD_PTD_BUFF_TYPE_ISTL:
#ifndef	CONFIG_ISO_SUPPORT

			isp1763_reg_write16(hcd->dev, hcd->regs.buffer_status,
					    buffstatus | ISO_BUFFER);
#endif
			break;
		}
	}
	spin_unlock(&hcd_data_lock);
	pehci_entry("--	%s: Exit\n", __FUNCTION__);
}
#endif



static void
pehci_hcd_qtd_schedule(phci_hcd	* hcd, struct ehci_qtd *qtd,
		       struct ehci_qh *qh, td_ptd_map_t	* td_ptd_map)
{
	struct urb *urb;
	urb_priv_t *urbpriv = 0;
	u32 length=0;
	struct isp1763_mem_addr	*mem_addr = 0;
	struct _isp1763_qha *qha, qhtemp;

	pehci_entry("++	%s: Entered\n",	__FUNCTION__);

	if (qtd->state & QTD_STATE_SCHEDULED) {
		return;
	}
	
	qha = &qhtemp;

	
	if (qtd->state & QTD_STATE_LAST) {
		
		qh->hw_current = cpu_to_le32(0);

		
	} else {
		qh->hw_current = qtd->hw_next;
	}

	urb = qtd->urb;
	urbpriv	= (urb_priv_t *) urb->hcpriv;
	urbpriv->timeout = 0;

	
	length = qtd->length;
	mem_addr = &qtd->mem_addr;
	phci_hcd_mem_alloc(length, mem_addr, 0);
	if (length && ((mem_addr->phy_addr == 0) || (mem_addr->virt_addr == 0))) {
		err("Never Error: Cannot allocate memory for the current td,length %d\n", length);
		return;
	}

	pehci_check("newqtd being scheduled, device: %d,map: %x\n",
		    urb->dev->devnum, td_ptd_map->ptd_bitmap);

	

	memset(qha, 0, sizeof(isp1763_qha));
	
	phci_hcd_qha_from_qtd(hcd, qtd,	qtd->urb, (void	*) qha,
			      td_ptd_map->ptd_ram_data_addr, qh
			       );

	if (qh->type ==	TD_PTD_BUFF_TYPE_INTL) {
		phci_hcd_qhint_schedule(hcd, qh, qtd, (isp1763_qhint *)	qha,
					qtd->urb);
	}


	length = PTD_XFERRED_LENGTH(qha->td_info1 >> 3);
	if (length > HC_ATL_PL_SIZE) {
		err("Never Error: Bogus	length,length %d(max %d)\n",
		qtd->length, HC_ATL_PL_SIZE);
	}

	
	isp1763_mem_write(hcd->dev, td_ptd_map->ptd_header_addr, 0,
			  (u32 *) (qha), PHCI_QHA_LENGTH, 0);
	
#if 0 
		printk("SCHEDULE Next qtd\n");
		printk("DW0: 0x%08X\n", qha->td_info1);
		printk("DW1: 0x%08X\n", qha->td_info2);
		printk("DW2: 0x%08X\n", qha->td_info3);
		printk("DW3: 0x%08X\n", qha->td_info4);
#endif
	
	
	
	if (qtd->length && (length <= HC_ATL_PL_SIZE)){
		switch (PTD_PID(qha->td_info2))	{
		case OUT_PID:
		case SETUP_PID:

			isp1763_mem_write(hcd->dev, (u32) mem_addr->phy_addr, 0,
				(void	*) qtd->hw_buf[0], length, 0);

#if 0
					int i=0;
					int *data_addr= qtd->hw_buf[0];
					printk("\n");
					for(i=0;i<length;i+=4) printk("[0x%X] ",*data_addr++);
					printk("\n");
#endif


			break;
		}
	}
	
	qtd->state &= ~QTD_STATE_NEW;
	qtd->state |= QTD_STATE_SCHEDULED;

	pehci_entry("--	%s: Exit\n", __FUNCTION__);
	return;
}
#ifdef USBNET 

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static void
pehci_hcd_urb_delayed_complete(phci_hcd * hcd, struct ehci_qh *qh, struct urb *urb,
	td_ptd_map_t * td_ptd_map, struct pt_regs *regs)
#else
static void
pehci_hcd_urb_delayed_complete(phci_hcd * hcd, struct ehci_qh *qh, struct urb *urb,
	td_ptd_map_t * td_ptd_map)
#endif
{
	static u32 remove = 0;
	static u32 qh_state = 0;

	urb_priv_t *urb_priv = (urb_priv_t *) urb->hcpriv;

#ifdef USBNET 
	struct isp1763_async_cleanup_urb *urb_st = 0;
#endif



	urb_priv->timeout = 0;

	if((td_ptd_map->state == TD_PTD_REMOVE	) ||
		  (urb_priv->state == DELETE_URB) ||
		     !HCD_IS_RUNNING(hcd->state)){
	remove=1;
	}
	qh_state=qh->qh_state;
	qh->qh_state = QH_STATE_COMPLETING;
	
	spin_lock(&hcd_data_lock);
	phci_hcd_urb_free_priv(hcd, urb_priv, qh);
	spin_unlock(&hcd_data_lock);

	urb_priv->timeout = 0;
	kfree(urb_priv);
	urb->hcpriv = 0;


	
	if (urb->status	== -EINPROGRESS) {
		urb->status = 0;
	}

	if(remove)
	if (list_empty(&qh->qtd_list)) {
		phci_hcd_release_td_ptd_index(qh);
	}
	remove=0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
		if(!usb_hcd_check_unlink_urb(&hcd->usb_hcd, urb,0))
					usb_hcd_unlink_urb_from_ep(&hcd->usb_hcd,urb);
#endif

{
	
	urb_st = (struct isp1763_async_cleanup_urb *)kmalloc(sizeof(struct isp1763_async_cleanup_urb), GFP_ATOMIC);
	urb_st->urb = urb;
	list_add_tail(&urb_st->urb_list, &(hcd->cleanup_urb.urb_list));

	isp1763_reg_write16(hcd->dev, hcd->regs.interruptenable, HC_MSOF_INT);
}

	pehci_entry("--	%s: Exit\n", __FUNCTION__);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static void
pehci_hcd_urb_complete(phci_hcd * hcd, struct ehci_qh *qh, struct urb *urb,
	td_ptd_map_t * td_ptd_map, struct pt_regs *regs)
#else
static void
pehci_hcd_urb_complete(phci_hcd * hcd, struct ehci_qh *qh, struct urb *urb,
	td_ptd_map_t * td_ptd_map)
#endif
{
	static u32 remove = 0;
	static u32 qh_state = 0;
	urb_priv_t *urb_priv = (urb_priv_t *) urb->hcpriv;
	
	if(urb_priv==NULL){
	printk("***************urb_priv is NULL ************ %s: Entered\n",	__FUNCTION__);
	goto exit;
	}
	pehci_check("complete the td , length: %d\n", td_ptd_map->qtd->length);
	urb_priv->timeout = 0;

	if((td_ptd_map->state == TD_PTD_REMOVE	) ||
		  (urb_priv->state == DELETE_URB) ||
		     !HCD_IS_RUNNING(hcd->state)){
	remove=1;
	}


	qh_state=qh->qh_state;

	qh->qh_state = QH_STATE_COMPLETING;
	
	spin_lock(&hcd_data_lock);
	phci_hcd_urb_free_priv(hcd, urb_priv, qh);
	spin_unlock(&hcd_data_lock);

	urb_priv->timeout = 0;
	kfree(urb_priv);
	urb->hcpriv = 0;


	
	if (urb->status	== -EINPROGRESS) {
		urb->status = 0;
	}

	if(remove)
	if (list_empty(&qh->qtd_list)) {
		phci_hcd_release_td_ptd_index(qh);
	}
	remove=0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	if(!usb_hcd_check_unlink_urb(&hcd->usb_hcd, urb,0))
	{
		usb_hcd_unlink_urb_from_ep(&hcd->usb_hcd,urb);
	}
#endif
	spin_unlock(&hcd->lock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	usb_hcd_giveback_urb(&hcd->usb_hcd, urb);
#else
	usb_hcd_giveback_urb(&hcd->usb_hcd, urb, urb->status);
#endif
	spin_lock(&hcd->lock);
exit:
	pehci_entry("--	%s: Exit\n", __FUNCTION__);
	
}

static void
pehci_hcd_update_error_status(u32 ptdstatus, struct urb	*urb)
{
	
	if (ptdstatus &	PTD_STATUS_HALTED) {
		if (ptdstatus &	PTD_XACT_ERROR)	{
			
			if (PTD_RETRY(ptdstatus)) {
				
				printk("transaction error , retries %d\n",
					PTD_RETRY(ptdstatus));
				urb->status = -EPIPE;
			} else {
				printk("transaction error , retries %d\n",
					PTD_RETRY(ptdstatus));
				
				urb->status = -EPROTO;
			}
		} else if (ptdstatus & PTD_BABBLE) {
			printk("babble error, qha %x\n", ptdstatus);
			
			urb->status = -EOVERFLOW;
		} else if (PTD_RETRY(ptdstatus)) {
			printk("endpoint halted with retrie remaining %d\n",
				PTD_RETRY(ptdstatus));
			urb->status = -EPIPE;
		} else {	
			printk("protocol error, qha %x\n", ptdstatus);
			urb->status = -EPIPE;
		}

		
		if (urb->status	== -EPIPE) {
		}
	}
}

#ifdef CONFIG_ISO_SUPPORT	

void 
pehci_hcd_iso_sitd_schedule(phci_hcd *hcd,struct urb* urb,struct ehci_sitd* sitd){
		td_ptd_map_t *td_ptd_map;
		td_ptd_map_buff_t *ptd_map_buff;
		struct _isp1763_isoptd *iso_ptd;
		u32 ormask = 0, skip_map = 0,last_map=0,buff_stat=0;
		struct isp1763_mem_addr *mem_addr;
		ptd_map_buff = &(td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL]);
		
		
		td_ptd_map =
				&ptd_map_buff->map_list[sitd->
					sitd_index];
		iso_ptd = &hcd->isotd;
		
		memset(iso_ptd, 0,	sizeof(struct _isp1763_isoptd));
		buff_stat =
			isp1763_reg_read16(hcd->dev, hcd->regs.buffer_status,buff_stat);

		
		skip_map =
			isp1763_reg_read16(hcd->dev, hcd->regs.isotdskipmap,
				skip_map);
		iso_dbg(ISO_DBG_DATA,
			"[pehci_hcd_iso_sitd_schedule]: Read skip map: 0x%08x\n",
			(unsigned int) skip_map);

		
		last_map =
			isp1763_reg_read16(hcd->dev, hcd->regs.isotdlastmap,
			last_map);

		
		ormask = isp1763_reg_read16(hcd->dev, hcd->regs.iso_irq_mask_or,
			ormask);
		
		
		phcd_iso_sitd_to_ptd(hcd, sitd, sitd->urb,
				(void *) iso_ptd);	
		ptd_map_buff->pending_ptd_bitmap &=
			~td_ptd_map->ptd_bitmap;		

#ifdef SWAP
				isp1763_mem_write(hcd->dev,
					td_ptd_map->ptd_header_addr, 0,
					(u32 *) iso_ptd, PHCI_QHA_LENGTH, 0,
					PTD_HED);
#else 
				isp1763_mem_write(hcd->dev,
					td_ptd_map->ptd_header_addr, 0,
					(u32 *) iso_ptd,PHCI_QHA_LENGTH, 0);
#endif

				td_ptd_map->state = TD_PTD_IN_SCHEDULE;

				if (sitd->length) {
					switch (PTD_PID(iso_ptd->td_info2)) {
					case OUT_PID:
						mem_addr = &sitd->mem_addr;
#ifdef SWAP
						isp1763_mem_write(hcd->dev,
							(unsigned long)
							mem_addr-> phy_addr,
							0, (u32*)
							((sitd->hw_bufp[0])),
							sitd->length, 0,
							PTD_PAY);
#else 
						isp1763_mem_write(hcd->dev,
							(unsigned long)
							mem_addr->phy_addr,
							0, (u32 *)
							sitd->hw_bufp[0],
							sitd->length, 0);
#endif
						break;
					}
					
				}

				
				if (sitd->hw_next == EHCI_LIST_END) {
					td_ptd_map->lasttd = 1;
				}

				skip_map &= ~td_ptd_map->ptd_bitmap;
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_sitd_schedule]: Skip map:0x%08x\n",(unsigned int) skip_map);

				if (last_map < td_ptd_map->ptd_bitmap) {
					isp1763_reg_write16(hcd->dev,
						hcd->regs.isotdlastmap,
						td_ptd_map->ptd_bitmap);
					iso_dbg(ISO_DBG_DATA,
						"[pehci_hcd_iso_sitd_schedule]:Last Map: 0x%08x\n",
						td_ptd_map->ptd_bitmap);
				}

				isp1763_reg_write16(hcd->dev,
					hcd->regs.buffer_status,
					(buff_stat | ISO_BUFFER));
				
				isp1763_reg_write16(hcd->dev, hcd->regs.isotdskipmap,skip_map);
		
}

void
pehci_hcd_iso_schedule(phci_hcd * hcd, struct urb *urb)
{
	struct list_head *sitd_itd_sched, *position;
	struct ehci_itd *itd;
	struct ehci_sitd *sitd;
	td_ptd_map_t *td_ptd_map;
	unsigned long last_map;
	td_ptd_map_buff_t *ptd_map_buff;
	struct _isp1763_isoptd *iso_ptd;
	unsigned long buff_stat;
	struct isp1763_mem_addr *mem_addr;
	u32 ormask = 0, skip_map = 0;
	u32 iNumofPkts;
	unsigned int iNumofSlots = 0, mult = 0;
	struct ehci_qh *qhead;

	buff_stat = 0;
	iso_dbg(ISO_DBG_ENTRY, "[pehci_hcd_iso_schedule]: Enter\n");
	iso_ptd = &hcd->isotd;

	last_map = 0;
	
	if (hcd->periodic_sched == 0) {
		return;
	}
	if (urb->dev->speed == USB_SPEED_HIGH) {
		mult = usb_maxpacket(urb->dev, urb->pipe,
				usb_pipeout(urb->pipe));
		mult = 1 + ((mult >> 11) & 0x3);
		iNumofSlots = NUMMICROFRAME / urb->interval;
		
		iNumofPkts = (urb->number_of_packets / mult) / iNumofSlots;
		if ((urb->number_of_packets / mult) % iNumofSlots != 0){
			
			iNumofPkts += 1;
		}
	} else{
		iNumofPkts = urb->number_of_packets;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	qhead = urb->hcpriv;
#else
	qhead = urb->ep->hcpriv;
#endif
	if (!qhead) {
		iso_dbg(ISO_DBG_ENTRY,
			"[pehci_hcd_iso_schedule]: Qhead==NULL\n");
		return ;
	}
	ptd_map_buff = &(td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL]);

	while (iNumofPkts > 0) {
	buff_stat =
		isp1763_reg_read16(hcd->dev, hcd->regs.buffer_status,buff_stat);

		
		skip_map =
			isp1763_reg_read16(hcd->dev, hcd->regs.isotdskipmap,
				skip_map);
		iso_dbg(ISO_DBG_DATA,
			"[pehci_hcd_iso_schedule]: Read skip map: 0x%08x\n",
			(unsigned int) skip_map);

		
		last_map =
			isp1763_reg_read16(hcd->dev, hcd->regs.isotdlastmap,
			last_map);

		
		ormask = isp1763_reg_read16(hcd->dev, hcd->regs.iso_irq_mask_or,
			ormask);

		sitd_itd_sched = &qhead->periodic_list.sitd_itd_head;
		if (list_empty(sitd_itd_sched)) {
			iso_dbg(ISO_DBG_INFO,
				"[pehci_hcd_iso_schedule]: ISO schedule list's empty. Nothing to schedule.\n");
			return;
		}

		list_for_each(position, sitd_itd_sched) {
			if (qhead->periodic_list.high_speed == 0){
				
				sitd = list_entry(position, struct ehci_sitd,
					sitd_list);
				iNumofPkts--;
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_schedule]: SITD Index:%d\n", sitd->sitd_index);
				if(sitd->sitd_index==TD_PTD_INV_PTD_INDEX)
					continue;
				
				td_ptd_map =
					&ptd_map_buff->map_list[sitd->
					sitd_index];
				memset(iso_ptd, 0,
					sizeof(struct _isp1763_isoptd));

				
				phcd_iso_sitd_to_ptd(hcd, sitd, sitd->urb,
					(void *) iso_ptd);

				ptd_map_buff->pending_ptd_bitmap &=
					~td_ptd_map->ptd_bitmap;

#ifdef SWAP
				isp1763_mem_write(hcd->dev,
					td_ptd_map->ptd_header_addr, 0,
					(u32 *) iso_ptd, PHCI_QHA_LENGTH, 0,
					PTD_HED);
#else 
				isp1763_mem_write(hcd->dev,
					td_ptd_map->ptd_header_addr, 0,
					(u32 *) iso_ptd,PHCI_QHA_LENGTH, 0);
#endif

				td_ptd_map->state = TD_PTD_IN_SCHEDULE;

				if (sitd->length) {
					switch (PTD_PID(iso_ptd->td_info2)) {
					case OUT_PID:
						mem_addr = &sitd->mem_addr;
#ifdef SWAP
						isp1763_mem_write(hcd->dev,
							(unsigned long)
							mem_addr-> phy_addr,
							0, (u32*)
							((sitd->hw_bufp[0])),
							sitd->length, 0,
							PTD_PAY);
#else 
						isp1763_mem_write(hcd->dev,
							(unsigned long)
							mem_addr->phy_addr,
							0, (u32 *)
							sitd->hw_bufp[0],
							sitd->length, 0);
#endif
						break;
					}
					
				}

				
				if (sitd->hw_next == EHCI_LIST_END) {
					td_ptd_map->lasttd = 1;
				}

				skip_map &= ~td_ptd_map->ptd_bitmap;
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_schedule]: Skip map:0x%08x\n",(unsigned int) skip_map);

				if (last_map < td_ptd_map->ptd_bitmap) {
					isp1763_reg_write16(hcd->dev,
						hcd->regs.isotdlastmap,
						td_ptd_map->ptd_bitmap);
					iso_dbg(ISO_DBG_DATA,
						"[pehci_hcd_iso_schedule]:Last Map: 0x%08x\n",
						td_ptd_map->ptd_bitmap);
				}

				isp1763_reg_write16(hcd->dev,
					hcd->regs.buffer_status,
					(buff_stat | ISO_BUFFER));

			} else {	

				
				itd = list_entry(position, struct ehci_itd,
					itd_list);
				iNumofPkts--;

				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_schedule]: ITD Index: %d\n",	itd->itd_index);
				
				td_ptd_map =
					&ptd_map_buff->map_list[itd->itd_index];
				memset(iso_ptd, 0,
					sizeof(struct _isp1763_isoptd));

				
				phcd_iso_itd_to_ptd(hcd, itd, itd->urb,
					(void *) iso_ptd);

				ptd_map_buff->pending_ptd_bitmap &=
					~td_ptd_map->ptd_bitmap;

#ifdef SWAP
				isp1763_mem_write(hcd->dev,
					td_ptd_map->ptd_header_addr, 0,
					(u32 *) iso_ptd,PHCI_QHA_LENGTH, 0,
					PTD_HED);
#else 
				isp1763_mem_write(hcd->dev,
					td_ptd_map->ptd_header_addr, 0,
					(u32 *) iso_ptd,PHCI_QHA_LENGTH, 0);
#endif
				td_ptd_map->state = TD_PTD_IN_SCHEDULE;

				if (itd->length) {
					switch (PTD_PID(iso_ptd->td_info2)) {
					case OUT_PID:
						mem_addr = &itd->mem_addr;
#ifdef SWAP
						isp1763_mem_write(hcd->dev,
							(unsigned long)
							mem_addr->phy_addr, 0,
							(u32*)
							((itd->hw_bufp[0])),
							itd->length, 0,
							PTD_PAY);
#else 
						isp1763_mem_write(hcd->dev,
							(unsigned long)
							mem_addr->phy_addr, 0,
							(u32 *)itd->hw_bufp[0],
							itd->length, 0);
#endif
						break;
					}
					
				}

				
				if (itd->hw_next == EHCI_LIST_END) {
					td_ptd_map->lasttd = 1;
				}

				skip_map &= ~td_ptd_map->ptd_bitmap;
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_schedule]: Skip map:0x%08x\n",(unsigned int) skip_map);
				isp1763_reg_write16(hcd->dev,
					hcd->regs.isotdskipmap,
					skip_map);

				if (last_map < td_ptd_map->ptd_bitmap) {
					isp1763_reg_write16(hcd->dev,
						hcd->regs.isotdlastmap,
						td_ptd_map->ptd_bitmap);
					iso_dbg(ISO_DBG_DATA,
						"[pehci_hcd_iso_schedule]:Last Map: 0x%08x\n",
						td_ptd_map->ptd_bitmap);
				}

				isp1763_reg_write16(hcd->dev,
					hcd->regs.buffer_status,
					(buff_stat | ISO_BUFFER));
			}
		}		
		isp1763_reg_write16(hcd->dev, hcd->regs.isotdskipmap,skip_map);
	}

	iso_dbg(ISO_DBG_INFO,
		"[pehci_hcd_iso_schedule]: ISO-Frame scheduling done\n");
	iso_dbg(ISO_DBG_ENTRY, "[pehci_hcd_iso_schedule]: Exit\n");
}


int debugiso = 0;

void
pehci_hcd_iso_worker(phci_hcd * hcd)
{
	u32 donemap = 0, skipmap = 0; 
	u32 pendingmap = 0;
	u32 mask = 0x1, index = 0, donetoclear = 0;
	u32 uFrIndex = 0;
	unsigned char last_td = FALSE, iReject = 0;
	struct isp1763_mem_addr *mem_addr;
	struct _isp1763_isoptd *iso_ptd;
	unsigned long length = 0, uframe_cnt, usof_stat;
	struct ehci_qh *qhead;
	struct ehci_itd *itd, *current_itd;
	struct ehci_sitd *sitd=0, *current_sitd=0;
	td_ptd_map_t *td_ptd_map;
	td_ptd_map_buff_t *ptd_map_buff;
	struct list_head *sitd_itd_remove, *position;
	struct urb *urb;
	u8 i = 0;
	unsigned long startAdd = 0;
	int ret = 0;


	iso_ptd = &hcd->isotd;

	
	if (hcd->periodic_sched == 0) {
		goto exit;
	}
	ptd_map_buff = &(td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL]);
	pendingmap = ptd_map_buff->pending_ptd_bitmap;


	
	donemap = isp1763_reg_read16(hcd->dev, hcd->regs.isotddonemap, donemap);

	iso_dbg(ISO_DBG_ENTRY, "[pehci_hcd_iso_worker]: Enter %x \n", donemap);
	if (!donemap) {		
		goto exit;
	}
	donetoclear = donemap;
	uFrIndex = 0;
	while (donetoclear) {
		mask = 0x1 << uFrIndex;
		index = uFrIndex;
		uFrIndex++;
		if (!(donetoclear & mask))
			continue;
		donetoclear &= ~mask;
		iso_dbg(ISO_DBG_DATA, "[pehci_hcd_iso_worker]: uFrIndex = %d\n", index);
		iso_dbg(ISO_DBG_DATA,
			"[pehci_hcd_iso_worker]:donetoclear = 0x%x mask = 0x%x\n",
			donetoclear, mask);


		if (ptd_map_buff->map_list[index].sitd) {
			urb = ptd_map_buff->map_list[index].sitd->urb;
			if (!urb) {
				printk("ERROR : URB is NULL \n");
				continue;
			}
			sitd = ptd_map_buff->map_list[index].sitd;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
			qhead=urb->hcpriv;
#else
			qhead = urb->ep->hcpriv;
#endif
			if (!qhead) {
				printk("ERROR : Qhead is NULL \n");
				continue;
			}

			sitd_itd_remove = &qhead->periodic_list.sitd_itd_head;
		} else if (ptd_map_buff->map_list[index].itd) {
			urb = ptd_map_buff->map_list[index].itd->urb;
			if (!urb) {
				printk("ERROR : URB is NULL \n");
				continue;
			}
			itd = ptd_map_buff->map_list[index].itd;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
			qhead=urb->hcpriv;
#else
			qhead = urb->ep->hcpriv;
#endif
			if (!qhead) {
				printk("ERROR : Qhead is NULL \n");
				continue;
			}

			sitd_itd_remove = &qhead->periodic_list.sitd_itd_head;

		} else {
			printk("ERROR : NO sitd in that PTD location : \n");
			continue;
		}
		
		iso_dbg(ISO_DBG_DATA,
			"[pehci_hcd_iso_worker]: Removal Frame number: %d\n",
			(int) index);
		if (list_empty(sitd_itd_remove)) {
			continue;
		}

		if (urb) {
			last_td = FALSE;
			if (qhead->periodic_list.high_speed == 0)
			{

				td_ptd_map =
					&ptd_map_buff->map_list[sitd->
								sitd_index];

				iso_dbg(ISO_DBG_INFO,
					"[pehci_hcd_iso_worker]: PTD is done,%d\n",index);
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_worker]: SITD Index: %d\n",sitd->sitd_index);
				urb = sitd->urb;

				mem_addr = &sitd->mem_addr;
				memset(iso_ptd, 0,
					sizeof(struct _isp1763_isoptd));


				isp1763_mem_read(hcd->dev,
					td_ptd_map->ptd_header_addr,
					0, (u32 *) iso_ptd,
					PHCI_QHA_LENGTH, 0);
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_worker]: DWORD0 = 0x%08x\n", iso_ptd->td_info1);
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_worker]: DWORD1 = 0x%08x\n", iso_ptd->td_info2);
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_worker]: DWORD2 = 0x%08x\n", iso_ptd->td_info3);
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_worker]: DWORD3 = 0x%08x\n", iso_ptd->td_info4);
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_worker]: DWORD4 = 0x%08x\n", iso_ptd->td_info5);
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_worker]: DWORD5 = 0x%08x\n", iso_ptd->td_info6);
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_worker]: DWORD6 = 0x%08x\n", iso_ptd->td_info7);
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_worker]: DWORD7 = 0x%08x\n", iso_ptd->td_info8);

				
				for (uframe_cnt = 0; uframe_cnt < 8;
					uframe_cnt++) {
					usof_stat =
						iso_ptd->td_info5 >> (8 +
						(uframe_cnt * 3));

					switch (usof_stat & 0x7) {
					case INT_UNDERRUN:
						iso_dbg(ISO_DBG_ERR,
							"[pehci_hcd_iso_worker Error]: Buffer underrun\n");
							urb->error_count++;
						break;
					case INT_EXACT:
						iso_dbg(ISO_DBG_ERR,
							"[pehci_hcd_iso_worker Error]: Transaction error\n");
							printk("[pehci_hcd_iso_worker Error]: Transaction error\n");
							urb->error_count++;
						break;
					case INT_BABBLE:
						iso_dbg(ISO_DBG_ERR,
							"[pehci_hcd_iso_worker Error]: Babble error\n");
							printk("[pehci_hcd_iso_worker Error]: Babble error\n");
						urb->iso_frame_desc[sitd->sitd_index].status
							= -EOVERFLOW;
						urb->error_count++;
						break;
					}	
				}	

				if (urb->dev->speed != USB_SPEED_HIGH) {
					
					length = PTD_XFERRED_NONHSLENGTH
						(iso_ptd->td_info4);
				} else {
					
					length = PTD_XFERRED_LENGTH(iso_ptd->
						td_info4);
				}

				
				if (iso_ptd->td_info4 & PTD_STATUS_HALTED) {
					iso_dbg(ISO_DBG_ERR,
						"[pehci_hcd_iso_worker Error] PTD Halted\n");
						printk("[pehci_hcd_iso_worker Error] PTD Halted\n");
					td_ptd_map->lasttd = 1;

					td_ptd_map->datatoggle = 0;
				}

				
				
				urb->iso_frame_desc[sitd->index].actual_length =
					length;

				
				if (iso_ptd->td_info1 & QHA_VALID) {
					iso_dbg(ISO_DBG_ERR,
						"[pehci_hcd_iso_worker Error]: Valid bit not cleared\n");
						printk("[pehci_hcd_iso_worker Error]: Valid bit not cleared\n");
					urb->iso_frame_desc[sitd->index].
						status = -ENOSPC;
				} else {
					urb->iso_frame_desc[sitd->index].
						status = 0;
				}

				
				if ((td_ptd_map->lasttd)
					|| (sitd->hw_next == EHCI_LIST_END)) {
					last_td = TRUE;
				}

				
				if (length && (length <= MAX_PTD_BUFFER_SIZE)) {
					switch (PTD_PID(iso_ptd->td_info2)) {
					case IN_PID:

						isp1763_mem_read(hcd->dev,
							(unsigned long)mem_addr->
							phy_addr, 0,(u32 *)
							sitd->hw_bufp[0],
							length, 0);

					case OUT_PID:
						urb->actual_length += length;
						break;
					}
				}
				
				
				skipmap =
					isp1763_reg_read16(hcd->dev,
						hcd->regs.isotdskipmap,
						skipmap);
				iso_dbg(ISO_DBG_DATA,
					"[%s] : read skipmap =0x%x\n",
					__FUNCTION__, skipmap);
				if (last_td == TRUE) {
					
					while (1) {
						if (sitd->hw_next == EHCI_LIST_END) {
							td_ptd_map =
								&ptd_map_buff->
								map_list[sitd->
								sitd_index];

#ifndef COMMON_MEMORY
							phci_hcd_mem_free
								(&sitd->
								mem_addr);
#endif
							
							list_del(&sitd->
								sitd_list);

							
							qha_free(qha_cache,
								sitd);

							
							td_ptd_map->state =
								TD_PTD_NEW;
							td_ptd_map->sitd = NULL;
							td_ptd_map->itd = NULL;

							
							hcd->periodic_sched--;

							
							skipmap |=
								td_ptd_map->ptd_bitmap;
							isp1763_reg_write16
								(hcd->dev,
								hcd->regs.
								isotdskipmap,
								skipmap);

							
							break;
						} else {
							current_sitd = sitd;

							td_ptd_map =
								&ptd_map_buff->
								map_list[sitd->
								sitd_index];

							sitd = (struct ehci_sitd
								*)
								(current_sitd->
								hw_next);
							
#ifndef COMMON_MEMORY
							phci_hcd_mem_free
								(&current_sitd->
								 mem_addr);
#endif
							
							list_del(&current_sitd->
								sitd_list);

							
							qha_free(qha_cache,
								current_sitd);

							
							td_ptd_map->state =
								TD_PTD_NEW;
							td_ptd_map->sitd = NULL;
							td_ptd_map->itd = NULL;

							
							hcd->periodic_sched--;

							
							skipmap |=
								td_ptd_map->
								ptd_bitmap;
							isp1763_reg_write16
								(hcd->dev,
								hcd->regs.
								isotdskipmap,
								skipmap);
							continue;
						}	

						
						break;
					}	

					
					if (urb->status == -EINPROGRESS) {
						if ((urb->actual_length !=
							urb->transfer_buffer_length)
							&& (urb->transfer_flags &
							URB_SHORT_NOT_OK)) {
							iso_dbg(ISO_DBG_ERR,
								"[pehci_hcd_iso_worker Error]: Short Packet\n");
							urb->status =
								-EREMOTEIO;
						} else {
							urb->status = 0;
						}
					}

					urb->hcpriv = 0;
					iso_dbg(ISO_DBG_DATA,
						"[%s] : remain skipmap =0x%x\n",
						__FUNCTION__, skipmap);
#ifdef COMMON_MEMORY
					phci_hcd_mem_free(&qhead->memory_addr);
#endif
					spin_unlock(&hcd->lock);
					
					iso_dbg(ISO_DBG_INFO,
						"[pehci_hcd_iso_worker] Complete a URB\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
		if(!usb_hcd_check_unlink_urb(&hcd->usb_hcd, urb,0))
					usb_hcd_unlink_urb_from_ep(&hcd->usb_hcd,
						urb);
#endif
					hcd->periodic_more_urb = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
						qhead=urb->hcpriv;
					if (!list_empty(&qhead->ep->urb_list))
#else
					if (!list_empty(&urb->ep->urb_list))
#endif
						hcd->periodic_more_urb = 1;
					

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
				usb_hcd_giveback_urb(&hcd->usb_hcd, urb);
#else
				usb_hcd_giveback_urb(&hcd->usb_hcd, urb, urb->status);
#endif

					spin_lock(&hcd->lock);
					continue;
				}

				
				iso_dbg(ISO_DBG_INFO,
					"[pehci_hcd_iso_worker]: last_td is not set\n");
				
				skipmap |= td_ptd_map->ptd_bitmap;
				isp1763_reg_write16(hcd->dev,
					hcd->regs.isotdskipmap,
					skipmap);
				iso_dbg(ISO_DBG_DATA,
					"%s : remain skipmap =0x%x\n",
					__FUNCTION__, skipmap);

				
				hcd->periodic_sched--;
				
				if(qhead->actualptds<qhead->totalptds)
				{
					sitd_itd_remove = &qhead->periodic_list.sitd_itd_head;
					
					list_for_each(position, sitd_itd_remove) {
						
						if (qhead->periodic_list.high_speed == 0){
						
							current_sitd= list_entry(position, struct ehci_sitd,
									sitd_list);		
							if(current_sitd->sitd_index==TD_PTD_INV_PTD_INDEX)
								break;
						}	
					}
				      if(current_sitd->sitd_index==TD_PTD_INV_PTD_INDEX){
					  	qhead->actualptds++;
					
						memcpy(&current_sitd->mem_addr,&sitd->mem_addr,sizeof(struct isp1763_mem_addr));
						current_sitd->sitd_index=sitd->sitd_index;
					
						td_ptd_map->sitd = current_sitd;
						hcd->periodic_sched++;
						pehci_hcd_iso_sitd_schedule(hcd, urb,current_sitd);
				      }

				
				list_del(&sitd->sitd_list);

				
				qha_free(qha_cache, sitd);

					
				}else{
#ifndef COMMON_MEMORY
				phci_hcd_mem_free(&sitd->mem_addr);
#endif
				
				list_del(&sitd->sitd_list);

				
				qha_free(qha_cache, sitd);

				td_ptd_map->state = TD_PTD_NEW;
				td_ptd_map->sitd = NULL;
				td_ptd_map->itd = NULL;

				}		

				
				
			}	else {	

				
				itd = ptd_map_buff->map_list[index].itd;

				
				td_ptd_map =
					&ptd_map_buff->map_list[itd->itd_index];

				iso_dbg(ISO_DBG_INFO,
					"[pehci_hcd_iso_worker]: PTD is done , %d\n",
					index);
				iso_dbg(ISO_DBG_DATA,
					"[pehci_hcd_iso_worker]: ITD Index: %d\n",
					itd->itd_index);

				urb = itd->urb;

				mem_addr = &itd->mem_addr;
				memset(iso_ptd, 0,
					sizeof(struct _isp1763_isoptd));


				isp1763_mem_read(hcd->dev,
					td_ptd_map->ptd_header_addr,
					0, (u32 *) iso_ptd,
					PHCI_QHA_LENGTH, 0);



				if (iso_ptd->td_info1 & QHA_VALID) {
					iso_dbg(ISO_DBG_ERR,
						"[pehci_hcd_iso_worker Error]: Valid bit not cleared\n");
					for(i = 0; i<itd->num_of_pkts; i++){
						urb->iso_frame_desc[itd->index
							+ i].status = -ENOSPC;
					}
				} else {
					for (i = 0; i<itd->num_of_pkts; i++){
						urb->iso_frame_desc[itd->index
							+i].status = 0;
					}
				}

				
				for (uframe_cnt = 0; (uframe_cnt < 8)
					&& (uframe_cnt < itd->num_of_pkts);
					uframe_cnt++) {
					usof_stat =
						iso_ptd->td_info5 >> (8 +
						(uframe_cnt * 3));

					switch (usof_stat & 0x7) {
					case INT_UNDERRUN:
						iso_dbg(ISO_DBG_ERR,
							"[pehci_hcd_iso_worker Error]: Buffer underrun\n");
						urb->iso_frame_desc[itd->index +
							uframe_cnt].
						status = -ECOMM;
						urb->error_count++;
						break;
					case INT_EXACT:
						iso_dbg(ISO_DBG_ERR,
							"[pehci_hcd_iso_worker Error]: %p Transaction error\n",
							urb);
						urb->iso_frame_desc[itd->index +
							uframe_cnt].
							status = -EPROTO;
						urb->error_count++;
						debugiso = 25;
						break;
					case INT_BABBLE:
						iso_dbg(ISO_DBG_ERR,
							"[pehci_hcd_iso_worker Error]: Babble error\n");
						urb->iso_frame_desc[itd->index +
							uframe_cnt].
							status = -EOVERFLOW;
						urb->error_count++;
						break;
					}
				}


				
				length = PTD_XFERRED_LENGTH(iso_ptd->td_info4);

				
				if (iso_ptd->td_info4 & PTD_STATUS_HALTED) {

					iso_dbg(ISO_DBG_ERR,
						"[pehci_hcd_iso_worker Error] PTD Halted\n");
					printk("[pehci_hcd_iso_worker Error] PTD Halted===============\n");
					td_ptd_map->lasttd = 1;

					td_ptd_map->datatoggle = 0;
				}
				
				
				if (PTD_PID(iso_ptd->td_info2) == OUT_PID) {
					for (i = 0; i < itd->num_of_pkts; i++){
						urb->iso_frame_desc[itd->index +
						i].actual_length =(unsigned int)
						length / itd->num_of_pkts;
					}
				} else{
					iso_dbg(ISO_DBG_DATA,
						"itd->num_of_pkts = %d, itd->ssplit = %x\n",
						itd->num_of_pkts, itd->ssplit);
					urb->iso_frame_desc[itd->index +
						0].actual_length =
						iso_ptd->td_info6 & 0x00000FFF;
					iso_dbg(ISO_DBG_DATA,
						"actual length[0] = %d\n",
						urb->iso_frame_desc[itd->index +0].
						actual_length);

					if((itd->num_of_pkts > 1)
						&& ((itd->ssplit & 0x2) == 0x2)
						&& (urb->iso_frame_desc[itd->index +
						1].status ==0)) {
						
						urb->iso_frame_desc[itd->index +1].
							actual_length =	(iso_ptd->
							td_info6 & 0x00FFF000)>> 12;

						iso_dbg(ISO_DBG_DATA,
							"actual length[1] = %d\n",
							urb->
							iso_frame_desc[itd->
							index + 1].
							actual_length);
					}else{
						urb->iso_frame_desc[itd->index +1].
							actual_length = 0;
					}

					if ((itd->num_of_pkts > 2)
						&& ((itd->ssplit & 0x4) == 0x4)
						&& (urb->
						iso_frame_desc[itd->index +
						2].status ==0)) {
						
						urb->iso_frame_desc[itd->index +
							2].actual_length =
							((iso_ptd->td_info6 &
							0xFF000000 )>> 24)
							| ((iso_ptd->td_info7
							& 0x0000000F)<< 8);
						
						iso_dbg(ISO_DBG_DATA,
							"actual length[2] = %d\n",
							urb->iso_frame_desc[itd->
							index + 2].actual_length);
					} else{
						urb->iso_frame_desc[itd->index +2].
							actual_length = 0;
					}

					if ((itd->num_of_pkts > 3)
						&& ((itd->ssplit & 0x8) == 0x8)
						&& (urb->iso_frame_desc[itd->index +
						3].status == 0)) {

						urb->iso_frame_desc[itd->index + 3].
							actual_length =(iso_ptd->
							td_info7 & 0x0000FFF0)>> 4;

						iso_dbg(ISO_DBG_DATA,
							"actual length[3] = %d\n",
							urb->iso_frame_desc[itd->
							index + 3].actual_length);
					} else {
						urb->iso_frame_desc[itd->index +3].
							actual_length = 0;
					}

					if ((itd->num_of_pkts > 4)
						&& ((itd->ssplit & 0x10) == 0x10)
						&& (urb->
						iso_frame_desc[itd->index +
						4].status ==0)) {

						urb->iso_frame_desc[itd->index +
							4].actual_length =
							(iso_ptd->
							td_info7 & 0x0FFF0000) >> 16;

						iso_dbg(ISO_DBG_DATA,
							"actual length[4] = %d\n",
							urb->iso_frame_desc[itd->index +
							4].actual_length);
					} else {
						urb->iso_frame_desc[itd->index +
							4].actual_length = 0;
					}

					if ((itd->num_of_pkts > 5)
						&& ((itd->ssplit & 0x20) == 0x20)
						&& (urb->
						iso_frame_desc[itd->index +
						5].status ==
						0)) {

						urb->iso_frame_desc[itd->index +
							5].actual_length =
							((iso_ptd->
							td_info7 & 0xF0000000) >> 28) | 
							((iso_ptd->td_info8 &
							0x000000FF)
							<< 4);

						iso_dbg(ISO_DBG_DATA,
							"actual length[5] = %d\n",
							urb->
							iso_frame_desc[itd->
							index +
							5].actual_length);
					} else {
						urb->iso_frame_desc[itd->index +
							5].actual_length = 0;
					}

					if ((itd->num_of_pkts > 6)
						&& ((itd->ssplit & 0x40) == 0x40)
						&& (urb->
						iso_frame_desc[itd->index +
						6].status ==0)) {

						urb->iso_frame_desc[itd->index +
							6].actual_length =
							(iso_ptd->
							td_info8 & 0x000FFF00)
							>> 8;
						
						iso_dbg(ISO_DBG_DATA,
							"actual length[6] = %d\n",
							urb->
							iso_frame_desc[itd->
							index +
							6].actual_length);
					} else {
						urb->iso_frame_desc[itd->index +
							6].actual_length = 0;
					}

					if ((itd->num_of_pkts > 7)
						&& ((itd->ssplit & 0x80) == 0x80)
						&& (urb->
						iso_frame_desc[itd->index +
						7].status ==
						0)) {

						urb->iso_frame_desc[itd->index +
							7].actual_length =
							(iso_ptd->
							td_info8 & 0xFFF00000) >> 20;

						iso_dbg(ISO_DBG_DATA,
							"actual length[7] = %d\n",
							urb->
							iso_frame_desc[itd->
							index +
							7].actual_length);
					} else {
						urb->iso_frame_desc[itd->index +
							7].actual_length = 0;
					}
				}
				
				if ((td_ptd_map->lasttd)
					|| (itd->hw_next == EHCI_LIST_END)) {

					last_td = TRUE;

				}

				
				if (length && (length <= MAX_PTD_BUFFER_SIZE)) {
					switch (PTD_PID(iso_ptd->td_info2)) {
					case IN_PID:
						
						startAdd = mem_addr->phy_addr;
						iso_dbg(ISO_DBG_DATA,
							"start add = %ld hw_bufp[0] = 0x%08x length = %d\n",
							startAdd,
							itd->hw_bufp[0],
							urb->
							iso_frame_desc[itd->
							index].actual_length);
						if (urb->
							iso_frame_desc[itd->index].
							status == 0) {

							if (itd->hw_bufp[0] ==0) {
								dma_addr_t
									buff_dma;

								buff_dma =
									(u32) ((unsigned char *) urb->transfer_buffer +
									urb->iso_frame_desc[itd->index].offset);
								itd->buf_dma =
									buff_dma;
								itd->hw_bufp[0]
									=
									buff_dma;
							}
							if (itd->hw_bufp[0] !=0) {

								ret = isp1763_mem_read(hcd->dev, (unsigned long)
									startAdd,
									0,(u32*)itd->
									hw_bufp[0],
									urb->
									iso_frame_desc
									[itd->
									index].
									actual_length,
									0);

							} else {
								printk("isp1763_mem_read data payload fail\n");
								printk("start add = %ld hw_bufp[0] = 0x%08x length = %d\n",
									startAdd, itd->hw_bufp[0],
									urb->iso_frame_desc[itd->index].actual_length);
								urb->iso_frame_desc[itd->index].status = -EPROTO;
								urb->error_count++;
							}
						}


						for (i = 1;
							i < itd->num_of_pkts;
							i++) {
							startAdd +=
								(unsigned
								long) (urb->
								iso_frame_desc
								[itd->
								index +
								i - 1].
								actual_length);

							iso_dbg(ISO_DBG_DATA,
								"start add = %ld hw_bufp[%d] = 0x%08x length = %d\n",
								startAdd, i,
								itd->hw_bufp[i],
								urb->
								iso_frame_desc
								[itd->index +
								i].
								actual_length);
							if (urb->
								iso_frame_desc[itd->
								index + i].
								status == 0) {

								isp1763_mem_read
									(hcd->dev,
									startAdd,
									0,(u32*)
									itd->
									hw_bufp
									[i],urb->
									iso_frame_desc
									[itd->
									index + i].
									actual_length,
									0);

								if (ret == -EINVAL){
									printk("isp1763_mem_read data payload fail %d\n", i);
								}
							}
						}

					case OUT_PID:
						urb->actual_length += length;
						break;
					}	
				}

				
				
				skipmap =
					isp1763_reg_read16(hcd->dev,
						hcd->regs.isotdskipmap,
						skipmap);

				iso_dbg(ISO_DBG_DATA,
					"[%s] : read skipmap =0x%x\n",
					__FUNCTION__, skipmap);
				if (last_td == TRUE) {
					
					while (1) {
						if (itd->hw_next ==
							EHCI_LIST_END) {
							td_ptd_map =
							&ptd_map_buff->
							map_list[itd->
							itd_index];

#ifndef COMMON_MEMORY
							phci_hcd_mem_free(&itd->
								mem_addr);
#endif

							
							list_del(&itd->
								itd_list);

							
							qha_free(qha_cache,
								itd);

							
							td_ptd_map->state =
								TD_PTD_NEW;
							td_ptd_map->sitd = NULL;
							td_ptd_map->itd = NULL;

							
							hcd->periodic_sched--;

							
							skipmap |=
								td_ptd_map->
								ptd_bitmap;

							isp1763_reg_write16
								(hcd->dev,
								hcd->regs.
								isotdskipmap,
								skipmap);

							
							break;
						}
						
						else {
							current_itd = itd;

							td_ptd_map =
								&ptd_map_buff->
								map_list[itd->
								itd_index];

							itd = (struct ehci_itd
								*) (current_itd->
								hw_next);
#ifndef COMMON_MEMORY
							
							phci_hcd_mem_free
								(&current_itd->
								mem_addr);
#endif

							
							list_del(&current_itd->
								itd_list);

							
							qha_free(qha_cache,
								current_itd);

							
							td_ptd_map->state =
								TD_PTD_NEW;
							td_ptd_map->sitd = NULL;
							td_ptd_map->itd = NULL;

							
							hcd->periodic_sched--;

							
							skipmap |=
								td_ptd_map->
								ptd_bitmap;
							isp1763_reg_write16
								(hcd->dev,
								hcd->regs.
								isotdskipmap,
								skipmap);
							continue;
						}
						
						break;
					}	
					
					if (urb->status == -EINPROGRESS) {
						if ((urb->actual_length !=
							urb->transfer_buffer_length)
							&& (urb->
							transfer_flags &
							URB_SHORT_NOT_OK)) {

							iso_dbg(ISO_DBG_ERR,
							"[pehci_hcd_iso_worker Error]: Short Packet\n");

							urb->status =
								-EREMOTEIO;
						} else {
							urb->status = 0;
						}
					}

					urb->hcpriv = 0;
					iso_dbg(ISO_DBG_DATA,
						"[%s] : remain skipmap =0x%x\n",
						__FUNCTION__, skipmap);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
					if (unlikely(atomic_read(&urb->reject))) {
						iso_dbg("ISO_DBG_INFO, [%s] urb reject\n", __FUNCTION__);
						iReject = 1;
					}
#else
					if (unlikely(urb->reject)) {
						iso_dbg("ISO_DBG_INFO, [%s] urb reject\n", __FUNCTION__);
						iReject = 1;
					}
#endif


#ifdef COMMON_MEMORY
					phci_hcd_mem_free(&qhead->memory_addr);
#endif
					
					
					spin_unlock(&hcd->lock);
					
					iso_dbg(ISO_DBG_INFO,
						"[pehci_hcd_iso_worker] Complete a URB\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
		if(!usb_hcd_check_unlink_urb(&hcd->usb_hcd, urb,0))
					usb_hcd_unlink_urb_from_ep(&hcd->usb_hcd, urb);
#endif
					hcd->periodic_more_urb = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
						qhead=urb->hcpriv;
					if (!list_empty(&qhead->ep->urb_list)){

#else
					if (!list_empty(&urb->ep->urb_list)){
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
						if (urb->hcpriv== periodic_ep[0]){
#else
						if (urb->ep == periodic_ep[0]){
#endif
							hcd->periodic_more_urb =
							1;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
						} else if (urb->hcpriv==
							 periodic_ep[1]){
#else
						} else if (urb->ep ==
							 periodic_ep[1]){
#endif							 
							hcd->periodic_more_urb =
							2;
						} else {
							hcd->periodic_more_urb =
							0;
						}


					}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
				usb_hcd_giveback_urb(&hcd->usb_hcd, urb);
#else
				usb_hcd_giveback_urb(&hcd->usb_hcd, urb, 
										urb->status);
#endif

					spin_lock(&hcd->lock);
					continue;
				}
				
				iso_dbg(ISO_DBG_INFO,
					"[pehci_hcd_iso_worker]: last_td is not set\n");
				
				skipmap |= td_ptd_map->ptd_bitmap;
				isp1763_reg_write16(hcd->dev,
					hcd->regs.isotdskipmap,
					skipmap);
				iso_dbg(ISO_DBG_DATA,
					"%s : remain skipmap =0x%x\n",
					__FUNCTION__, skipmap);

				
				hcd->periodic_sched--;
#ifndef COMMON_MEMORY
				
				phci_hcd_mem_free(&itd->mem_addr);
#endif
				
				list_del(&itd->itd_list);

				
				qha_free(qha_cache, itd);
				td_ptd_map->state = TD_PTD_NEW;
				td_ptd_map->sitd = NULL;
				td_ptd_map->itd = NULL;
			}	
		}		
		iso_dbg(ISO_DBG_INFO,
			"[pehci_hcd_iso_worker]: ISO-Frame removal done\n");


	}			


	if (iReject) {
		spin_unlock(&hcd->lock);
		if (hcd->periodic_more_urb) {

			if(periodic_ep[hcd->periodic_more_urb])
			while (&periodic_ep[hcd->periodic_more_urb - 1]->
				urb_list) {

				urb = container_of(periodic_ep
					[hcd->periodic_more_urb -
					1]->urb_list.next,
					struct urb, urb_list);
				
				if (urb) {
					urb->status = -ENOENT;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
		if(!usb_hcd_check_unlink_urb(&hcd->usb_hcd, urb,0))
					usb_hcd_unlink_urb_from_ep(&hcd->
					usb_hcd,urb);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
					usb_hcd_giveback_urb(&hcd->usb_hcd, urb);
#else
					usb_hcd_giveback_urb(&hcd->usb_hcd, urb,
						urb->status);
#endif
				}
			}
		}

		spin_lock(&hcd->lock);
	}


	if (hcd->periodic_more_urb) {
		int status = 0;
		iso_dbg(ISO_DBG_INFO,
			"[phcd_iso_handler]: No more PTDs queued\n");
		hcd->periodic_sched = 0;
		phcd_store_urb_pending(hcd, hcd->periodic_more_urb, NULL,
				       &status);
		hcd->periodic_more_urb = 0;
	}
exit:
	iso_dbg(ISO_DBG_ENTRY, "-- %s: Exit\n", __FUNCTION__);
}				

#endif 

static void
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
pehci_hcd_intl_worker(phci_hcd * hcd, struct pt_regs *regs)
#else
pehci_hcd_intl_worker(phci_hcd * hcd)
#endif
{
	int i =	0;
	u16 donemap = 0, donetoclear;
	u16 mask = 0x1,	index =	0;
	u16 pendingmap = 0;
	u16 location = 0;
	u32 length = 0;
	u16 skipmap = 0;
	u16 ormask = 0;
	u32 usofstatus = 0;
	struct urb *urb;
	struct ehci_qtd	*qtd = 0;
	struct ehci_qh *qh = 0;

	struct _isp1763_qhint *qhint = &hcd->qhint;

	td_ptd_map_t *td_ptd_map;
	td_ptd_map_buff_t *ptd_map_buff;
	struct isp1763_mem_addr	*mem_addr = 0;
	u16 dontschedule = 0;

	ptd_map_buff = &(td_ptd_map_buff[TD_PTD_BUFF_TYPE_INTL]);
	pendingmap = ptd_map_buff->pending_ptd_bitmap;

	
	donetoclear = donemap =
		isp1763_reg_read16(hcd->dev, hcd->regs.inttddonemap, donemap);
	if (donemap) {
		
		skipmap	=
			isp1763_reg_read16(hcd->dev, hcd->regs.inttdskipmap,
			skipmap);
		skipmap	|= donemap;
		isp1763_reg_write16(hcd->dev, hcd->regs.inttdskipmap, skipmap);
		donemap	|= pendingmap;
	}
	
#ifdef MSEC_INT_BASED
	else {
		
		if (ptd_map_buff->pending_ptd_bitmap) {
			pehci_hcd_schedule_pending_ptds(hcd, pendingmap, (u8)
				TD_PTD_BUFF_TYPE_INTL,
				1);
		}
		
		goto exit;
	}
#else
	else {
	goto exit;	
	
	}

#endif


	ormask = isp1763_reg_read16(hcd->dev, hcd->regs.int_irq_mask_or,
		ormask);
	
	donetoclear = donemap;
	while (donetoclear) {
		
		index =	donetoclear & mask;
		donetoclear &= ~mask;
		mask <<= 1;
		if (!index) {
			location++;
			continue;
		}

		
		td_ptd_map = &ptd_map_buff->map_list[location];

		
		if (td_ptd_map->state == TD_PTD_REMOVE ||
			td_ptd_map->state == TD_PTD_NEW) {
			pehci_check("interrupt td is being removed\n");
			
			
			donemap	&= ~td_ptd_map->ptd_bitmap;
			
			ptd_map_buff->pending_ptd_bitmap &=
				~td_ptd_map->ptd_bitmap;
			continue;
		}


		
		if (!(skipmap &	td_ptd_map->ptd_bitmap)) {
			pehci_check("intr td_ptd_map %x,skipnap	%x\n",
			td_ptd_map->ptd_bitmap, skipmap);
			donemap	&= ~td_ptd_map->ptd_bitmap;
			
			ptd_map_buff->pending_ptd_bitmap &=
				~td_ptd_map->ptd_bitmap;;
			location++;
			continue;
		}


		if (td_ptd_map->state == TD_PTD_NEW) {
			pehci_check
				("interrupt not	come here, map %x,location %d\n",
				 td_ptd_map->ptd_bitmap, location);
			donemap	&= ~td_ptd_map->ptd_bitmap;
			
			ptd_map_buff->pending_ptd_bitmap &=
				~td_ptd_map->ptd_bitmap;
			donemap	&= ~td_ptd_map->ptd_bitmap;
			location++;
			continue;
		}

		
		location++;
		qh = td_ptd_map->qh;
		qtd = td_ptd_map->qtd;
		if (qtd->state & QTD_STATE_NEW)	{
			
			goto schedule;
		}
		urb = qtd->urb;
		mem_addr = &qtd->mem_addr;

		
		ormask &= ~td_ptd_map->ptd_bitmap;
		isp1763_reg_write16(hcd->dev, hcd->regs.int_irq_mask_or,
			ormask);

		ptd_map_buff->active_ptds--;
		memset(qhint, 0, sizeof(struct _isp1763_qhint));

		isp1763_mem_read(hcd->dev, td_ptd_map->ptd_header_addr,	0,
				 (u32 *) (qhint), PHCI_QHA_LENGTH, 0);

#ifdef PTD_DUMP_COMPLETE
		printk("INTL PTD header after COMPLETION\n");
		printk("CDW0: 0x%08X\n", qhint->td_info1);
		printk("CDW1: 0x%08X\n", qhint->td_info2);
		printk("CDW2: 0x%08X\n", qhint->td_info3);
		printk("CDW3: 0x%08X\n", qhint->td_info4);
#endif

		
		for (i = 0; i <	8; i++)	{
			
			usofstatus = qhint->td_info5 >>	(8 + i * 3);
			switch (usofstatus & 0x7) {
			case INT_UNDERRUN:
				pehci_print("under run , %x\n",	usofstatus);
				break;
			case INT_EXACT:
				pehci_print("transaction error,	%x\n",
					    usofstatus);
				break;
			case INT_BABBLE:
				pehci_print("babble error, %x\n", usofstatus);
				break;
			}
		}

		if (urb->dev->speed != USB_SPEED_HIGH) {
			
			length = PTD_XFERRED_NONHSLENGTH(qhint->td_info4);
		} else {
			
			length = PTD_XFERRED_LENGTH(qhint->td_info4);
		}

		pehci_hcd_update_error_status(qhint->td_info4, urb);
		
		if (qhint->td_info4 & PTD_STATUS_HALTED) {
			qtd->state |= QTD_STATE_LAST;
			qh->datatoggle = td_ptd_map->datatoggle	= 0;
			donemap	&= ~td_ptd_map->ptd_bitmap;
			ptd_map_buff->pending_ptd_bitmap &=
				~td_ptd_map->ptd_bitmap;
			dontschedule = 1;
			goto copylength;
		}


		copylength:
		
		qh->datatoggle = td_ptd_map->datatoggle	=
			PTD_NEXTTOGGLE(qhint->td_info4);
		
		switch (PTD_PID(qhint->td_info2)) {
		case IN_PID:
			if (length && (length <= MAX_PTD_BUFFER_SIZE))
				
				isp1763_mem_read(hcd->dev,
					(u32) mem_addr->phy_addr, 0,
					urb->transfer_buffer +
					urb->actual_length, length, 0);

		case OUT_PID:
			urb->actual_length += length;
			qh->hw_current = qtd->hw_next;
			phci_hcd_mem_free(&qtd->mem_addr);
			qtd->state &= ~QTD_STATE_NEW;
			qtd->state |= QTD_STATE_DONE;
			break;
		}

		if (qtd->state & QTD_STATE_LAST) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
			pehci_hcd_urb_complete(hcd, qh, urb, td_ptd_map, regs);
#else
			pehci_hcd_urb_complete(hcd, qh, urb, td_ptd_map);
#endif
			if (dontschedule) {	
				dontschedule = 0;
				continue;
			}

			
			if (!list_empty(&qh->qtd_list))	{
				struct list_head *head;
				
				head = &qh->qtd_list;
				qtd = list_entry(head->next, struct ehci_qtd,
					qtd_list);
				td_ptd_map->qtd	= qtd;
				qh->hw_current = cpu_to_le32(qtd);
				qh->qh_state = QH_STATE_LINKED;

			} else {
				td_ptd_map->qtd	=
						 (struct ehci_qtd *) le32_to_cpu(0);
				qh->hw_current = cpu_to_le32(0);
				qh->qh_state = QH_STATE_IDLE;
				donemap	&= ~td_ptd_map->ptd_bitmap;
				ptd_map_buff->pending_ptd_bitmap &= 
						~td_ptd_map->ptd_bitmap;
	       			td_ptd_map->state=TD_PTD_NEW;
				continue;
			}

		}

		schedule:
		{
			
			ptd_map_buff->pending_ptd_bitmap &=
				~td_ptd_map->ptd_bitmap;
			ormask |= td_ptd_map->ptd_bitmap;
			ptd_map_buff->active_ptds++;
			pehci_check
				("inter	schedule next qtd %p, active tds %d\n",
				 qtd, ptd_map_buff->active_ptds);
			pehci_hcd_qtd_schedule(hcd, qtd, qh, td_ptd_map);
		}

	}			


	
	skipmap	&= ~donemap;
	isp1763_reg_write16(hcd->dev, hcd->regs.inttdskipmap, skipmap);
	ormask |= donemap;
	isp1763_reg_write16(hcd->dev, hcd->regs.int_irq_mask_or, ormask);
exit:
	pehci_entry("--	%s: Exit\n", __FUNCTION__);
	
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static void
pehci_hcd_atl_worker(phci_hcd * hcd, struct pt_regs *regs)
#else
static void
pehci_hcd_atl_worker(phci_hcd * hcd)
#endif
{
	u16 donemap = 0, donetoclear = 0;
	u16 pendingmap = 0;
	u32 rl = 0;
	u16 mask = 0x1,	index =	0;
	u16 location = 0;
	u32 nakcount = 0;
	u32 active = 0;
	u32 length = 0;
	u16 skipmap = 0;
	u16 tempskipmap	= 0;
	u16 ormask = 0;
	struct urb *urb;
	struct ehci_qtd	*qtd = 0;
	struct ehci_qh *qh;
	struct _isp1763_qha atlqha;
	struct _isp1763_qha *qha;
	td_ptd_map_t *td_ptd_map;
	td_ptd_map_buff_t *ptd_map_buff;
	urb_priv_t *urbpriv = 0;
	struct isp1763_mem_addr	*mem_addr = 0;
	u16 dontschedule = 0;
	ptd_map_buff = &(td_ptd_map_buff[TD_PTD_BUFF_TYPE_ATL]);
	pendingmap = ptd_map_buff->pending_ptd_bitmap;

#ifdef MSEC_INT_BASED
	skipmap	= isp1763_reg_read16(hcd->dev, hcd->regs.atltdskipmap, skipmap);
	tempskipmap = ~skipmap;
	tempskipmap &= 0xffff;

	if (tempskipmap) {
		donemap	=
			isp1763_reg_read16(hcd->dev, hcd->regs.atltddonemap,
					   donemap);
		skipmap	|= donemap;
		isp1763_reg_write16(hcd->dev, hcd->regs.atltdskipmap, skipmap);
		qha = &atlqha;
		donemap	|= pendingmap;
		tempskipmap &= ~donemap;
	}  else {

	

		
		if (pendingmap)	{
			pehci_hcd_schedule_pending_ptds(hcd, pendingmap, (u8)
				TD_PTD_BUFF_TYPE_ATL,
				1);
		}
		goto exit;
	}
#else

	donemap	= isp1763_reg_read16(hcd->dev, hcd->regs.atltddonemap, donemap);
	if (donemap) {


		pehci_info("DoneMap Value in ATL Worker	%x\n", donemap);
		skipmap	=
			isp1763_reg_read16(hcd->dev, hcd->regs.atltdskipmap,
					   skipmap);
		skipmap	|= donemap;
		isp1763_reg_write16(hcd->dev, hcd->regs.atltdskipmap, skipmap);
		qha = &atlqha;
	} else {
		pehci_info("Done Map Value is 0x%X \n",	donemap);
		pehci_entry("--	%s: Exit abnormally with DoneMap all zero \n",
			    __FUNCTION__);
		goto exit;

	}
#endif

	
	ormask = isp1763_reg_read16(hcd->dev, hcd->regs.atl_irq_mask_or,
				    ormask);


	donetoclear = donemap;
	
	donetoclear |= tempskipmap;
	
	while (donetoclear) {
		
		index =	donetoclear & mask;
		donetoclear &= ~mask;
		mask <<= 1;
		if (!index) {
			location++;
			continue;
		}

		
		td_ptd_map = &ptd_map_buff->map_list[location];

		
		if (td_ptd_map->state == TD_PTD_NEW ||
			td_ptd_map->state == TD_PTD_REMOVE)	{
			pehci_check
				("atl td is being removed,map %x, skipmap %x\n",
				 td_ptd_map->ptd_bitmap, skipmap);
			pehci_check("temp skipmap %x, pendign map %x,done %x\n",
				    tempskipmap, pendingmap, donemap);

			
			donemap	&= ((~td_ptd_map->ptd_bitmap) &	0xffff);
			
			ptd_map_buff->pending_ptd_bitmap &=
				((~td_ptd_map->ptd_bitmap) & 0xffff);
			location++;
			continue;
		}


		
		location++;
		qh = td_ptd_map->qh;
		qtd = td_ptd_map->qtd;
		if (!qh	|| !qtd) {
			donemap	&= ((~td_ptd_map->ptd_bitmap) &	0xffff);
			
			ptd_map_buff->pending_ptd_bitmap &=
				((~td_ptd_map->ptd_bitmap) & 0xffff);
			continue;
		}
#ifdef MSEC_INT_BASED
		
		if ((qtd->state	& QTD_STATE_NEW)	
 ) {
			qh->hw_current = QTD_NEXT(qtd->qtd_dma);
			goto schedule;
		}
#endif
		urb = qtd->urb;
		if (urb	== NULL) {
			donemap	&= ((~td_ptd_map->ptd_bitmap) &	0xffff);
			
			ptd_map_buff->pending_ptd_bitmap &=
				((~td_ptd_map->ptd_bitmap) & 0xffff);
			continue;
		}
		urbpriv	= (urb_priv_t *) urb->hcpriv;
		mem_addr = &qtd->mem_addr;

#ifdef MSEC_INT_BASED
		
		if (donemap & td_ptd_map->ptd_bitmap) {
			
			;
		} else {
			if (tempskipmap	& td_ptd_map->ptd_bitmap) {
				
				if (urbpriv->timeout < 20) {
					urbpriv->timeout++;
					continue;
				}
				urbpriv->timeout++;
				
			}

		}
#endif
		memset(qha, 0, sizeof(struct _isp1763_qha));

		isp1763_mem_read(hcd->dev, td_ptd_map->ptd_header_addr,	0,
				 (u32 *) (qha),	PHCI_QHA_LENGTH, 0);

#ifdef PTD_DUMP_COMPLETE
		printk("ATL PTD header after COMPLETION\n");
		printk("CDW0: 0x%08X\n", qha->td_info1);
		printk("CDW1: 0x%08X\n", qha->td_info2);
		printk("CDW2: 0x%08X\n", qha->td_info3);
		printk("CDW3: 0x%08X\n", qha->td_info4);
#endif

#ifdef MSEC_INT_BASED
		if ((qha->td_info1 & QHA_VALID)) {

			pehci_check
				("pendign map %x, donemap %x, tempskipmap %x\n",
				 pendingmap, donemap, tempskipmap);
			
			ptd_map_buff->pending_ptd_bitmap &=
				((~td_ptd_map->ptd_bitmap) & 0xffff);
			
			urbpriv->timeout++;
			continue;
		} else {

			skipmap	|= td_ptd_map->ptd_bitmap;
			isp1763_reg_write16(hcd->dev, hcd->regs.atltdskipmap,
					    skipmap);

			donemap	|= td_ptd_map->ptd_bitmap;
		}
#endif
		
		ormask &= ((~td_ptd_map->ptd_bitmap) & 0xffff);
		isp1763_reg_write16(hcd->dev, hcd->regs.atl_irq_mask_or,
			ormask);

		ptd_map_buff->active_ptds--;

		urbpriv->timeout = 0;

		
		pehci_hcd_update_error_status(qha->td_info4, urb);
		
		if (qha->td_info4 & PTD_STATUS_HALTED) {

			printk(KERN_NOTICE "Endpoint is	halted\n");
			qtd->state |= QTD_STATE_LAST;

			donemap	&= ((~td_ptd_map->ptd_bitmap) &	0xffff);
			
			ptd_map_buff->pending_ptd_bitmap &=
				((~td_ptd_map->ptd_bitmap) & 0xffff);
			qh->datatoggle = td_ptd_map->datatoggle	= 0;
			
			qh->ping = 0;
			
			dontschedule = 1;
			goto copylength;
		}



		
		rl = (qha->td_info3 >> 23);
		rl &= 0xf;



		if ((qha->td_info4 & PTD_XACT_ERROR) &&
			!(qha->td_info4 & PTD_STATUS_HALTED) &&
			(qha->td_info4 & QHA_ACTIVE)) {

			if (PTD_XFERRED_LENGTH(qha->td_info4) == qtd->length) {
				;	
			} else {

				pehci_print
					("xact error, info1 0x%08x,info4 0x%08x\n",
					 qha->td_info1,	qha->td_info4);

				qha->td_info1 |= QHA_VALID;
				skipmap	&= ~td_ptd_map->ptd_bitmap;
				ormask |= td_ptd_map->ptd_bitmap;
				donemap	&= ((~td_ptd_map->ptd_bitmap) &	0xffff);

				
				qha->td_info4 |= (rl <<	19);
				
				qha->td_info4 |= QHA_ACTIVE;

				
				qha->td_info4 &= ~PTD_XACT_ERROR;
				isp1763_reg_write16(hcd->dev,
						    hcd->regs.atl_irq_mask_or,
						    ormask);

				isp1763_mem_write(hcd->dev,
						  td_ptd_map->ptd_header_addr,
						  0, (u32 *) (qha),
						  PHCI_QHA_LENGTH, 0);
				
				isp1763_reg_write16(hcd->dev,
						    hcd->regs.atltdskipmap,
						    skipmap);
				continue;
			}
			goto copylength;
		}

		nakcount = qha->td_info4 >> 19;
		nakcount &= 0xf;
		active = qha->td_info4 & QHA_ACTIVE;
		if (!nakcount && active) {
			pehci_info("%s:	ptd is going for reload,length %d\n",
				   __FUNCTION__, length);
			
			qha->td_info1 |= QHA_VALID;
			donemap	&= ((~td_ptd_map->ptd_bitmap & 0xffff));
			

			
			qha->td_info4 |= (rl <<	19);
			qha->td_info4 &= ~0x3;
			qha->td_info4 |= (0x2 << 23);
			ptd_map_buff->active_ptds++;
			skipmap	&= ((~td_ptd_map->ptd_bitmap) &	0xffff);
			ormask |= td_ptd_map->ptd_bitmap;
			isp1763_reg_write16(hcd->dev, hcd->regs.atl_irq_mask_or,
					    ormask);
			isp1763_mem_write(hcd->dev, td_ptd_map->ptd_header_addr,
					  0, (u32 *) (qha), PHCI_QHA_LENGTH, 0);
			
			isp1763_reg_write16(hcd->dev, hcd->regs.atltdskipmap,
					    skipmap);
			continue;
		}

		copylength:
		
		length = PTD_XFERRED_LENGTH(qha->td_info4);


		
		if ((length < qtd->length) && usb_pipebulk(urb->pipe)) {

			
			if ((urb->transfer_flags & URB_SHORT_NOT_OK)) {
				pehci_check
					("short	read, length %d(expected %d)\n",
					 length, qtd->length);
				urb->status = -EREMOTEIO;
				donemap	&= ((~td_ptd_map->ptd_bitmap) &	0xffff);
				ptd_map_buff->pending_ptd_bitmap &=
					((~td_ptd_map->ptd_bitmap) & 0xffff);
				
				dontschedule = 1;
			}

			
			
			qtd->state |= QTD_STATE_LAST;

		}
		
		qh->datatoggle = td_ptd_map->datatoggle	=
			PTD_NEXTTOGGLE(qha->td_info4);
		qh->ping = PTD_PING_STATE(qha->td_info4);
		
		switch (PTD_PID(qha->td_info2))	{
		case IN_PID:
			qh->ping = 0;
			
			if (length && (length <= HC_ATL_PL_SIZE)) {
				isp1763_mem_read(hcd->dev,
						 (u32) mem_addr->phy_addr, 0,
						 (u32*) (le32_to_cpu(qtd->hw_buf[0])), length, 0);
#if 0
			
			if(length<=4)	{
					int i=0;
					int *data_addr= qtd->hw_buf[0];
					printk("\n");
					for(i=0;i<length;i+=4) printk("[0x%X] ",*data_addr++);
					printk("\n");
				}
#endif
			}

		case OUT_PID:
			urb->actual_length += length;
			qh->hw_current = qtd->hw_next;
			phci_hcd_mem_free(&qtd->mem_addr);
			qtd->state |= QTD_STATE_DONE;

			break;
		case SETUP_PID:
			qh->hw_current = qtd->hw_next;
			phci_hcd_mem_free(&qtd->mem_addr);
			qtd->state |= QTD_STATE_DONE;
			break;
		}

		if (qtd->state & QTD_STATE_LAST) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
			pehci_hcd_urb_complete(hcd, qh, urb, td_ptd_map, regs);
#else
			pehci_hcd_urb_complete(hcd, qh, urb, td_ptd_map);
#endif
			if (dontschedule) {	
				dontschedule = 0;
				
				qh->qh_state = QH_STATE_TAKE_NEXT;
				continue;
			}
			
			if (!list_empty(&qh->qtd_list))	{
				struct list_head *head;
				
				head = &qh->qtd_list;
				qtd = list_entry(head->next, struct ehci_qtd,
						 qtd_list);
				td_ptd_map->qtd	= qtd;
				qh->hw_current = cpu_to_le32(qtd);
				qh->qh_state = QH_STATE_LINKED;

			} else {
				td_ptd_map->qtd	=
					(struct	ehci_qtd *) le32_to_cpu(0);
				qh->hw_current = cpu_to_le32(0);
				qh->qh_state = QH_STATE_TAKE_NEXT;
				donemap	&= ((~td_ptd_map->ptd_bitmap & 0xffff));
				ptd_map_buff->pending_ptd_bitmap &=
					((~td_ptd_map->ptd_bitmap) & 0xffff);
				continue;
			}
		}

#ifdef MSEC_INT_BASED
		schedule:
#endif
		{
			
			ptd_map_buff->pending_ptd_bitmap &=
				((~td_ptd_map->ptd_bitmap) & 0xffff);
			td_ptd_map->qtd	=
				(struct	ehci_qtd
				 *) (le32_to_cpu(qh->hw_current));
			qtd = td_ptd_map->qtd;
			ormask |= td_ptd_map->ptd_bitmap;
			ptd_map_buff->active_ptds++;
			pehci_hcd_qtd_schedule(hcd, qtd, qh, td_ptd_map);
		}

	}			

	skipmap	&= ((~donemap) & 0xffff);
	isp1763_reg_write16(hcd->dev, hcd->regs.atltdskipmap, skipmap);
	ormask |= donemap;
	isp1763_reg_write16(hcd->dev, hcd->regs.atl_irq_mask_or, ormask);
exit:
	pehci_entry("--	%s: Exit\n", __FUNCTION__);
}


static void
pehci_hub_descriptor(phci_hcd *	hcd, struct usb_hub_descriptor *desc)
{
	u32 ports = 0;
	u16 temp = 0;

	pehci_entry("++	%s: Entered\n",	__FUNCTION__);

	ports =	0x11;
	ports =	ports &	0xf;

	pehci_info("%s:	number of ports	%d\n", __FUNCTION__, ports);

	desc->bDescriptorType =	0x29;
	desc->bPwrOn2PwrGood = 10;

	desc->bHubContrCurrent = 0;

	desc->bNbrPorts	= ports;
	temp = 1 + (ports / 8);
	desc->bDescLength = 7 +	2 * temp;
	

	memset(&desc->DeviceRemovable[0], 0, temp);
	memset(&desc->PortPwrCtrlMask[temp], 0xff, temp);

	temp = 0x0008;		
	temp |=	0x0001;		
	temp |=	0x0080;		
	desc->wHubCharacteristics = cpu_to_le16(temp);
	pehci_entry("--	%s: Exit\n", __FUNCTION__);
}

static int
phci_check_reset_complete(phci_hcd * hcd, int index, int port_status)
{
	pehci_print("check reset complete\n");
	if (!(port_status & PORT_CONNECT)) {
		hcd->reset_done[index] = 0;
		return port_status;
	}

	
	if (!(port_status & PORT_PE)) {
		printk("port %d	full speed --> companion\n", index + 1);
		port_status |= PORT_OWNER;
		isp1763_reg_write32(hcd->dev, hcd->regs.ports[index],
				    port_status);

	} else {
		pehci_print("port %d high speed\n", index + 1);
	}

	return port_status;

}



static void
pehci_hcd_init_map_buffers(phci_hcd * phci)
{
	td_ptd_map_buff_t *ptd_map_buff;
	u8 buff_type, ptd_index;
	u32 bitmap;

	pehci_entry("++	%s: Entered\n",	__FUNCTION__);
	pehci_print("phci_init_map_buffers(phci	= 0x%p)\n", phci);
	
	for (buff_type = 0; buff_type <	TD_PTD_TOTAL_BUFF_TYPES; buff_type++) {
		ptd_map_buff = &(td_ptd_map_buff[buff_type]);
		ptd_map_buff->buffer_type = buff_type;
		ptd_map_buff->active_ptds = 0;
		ptd_map_buff->total_ptds = 0;
		
		ptd_map_buff->max_ptds = 16;
		ptd_map_buff->active_ptd_bitmap	= 0;
		
		
		ptd_map_buff->pending_ptd_bitmap = 0x00000000;

		
		bitmap = 0x00000001;
		for (ptd_index = 0; ptd_index <	TD_PTD_MAX_BUFF_TDS;
			ptd_index++) {
			
			ptd_map_buff->map_list[ptd_index].datatoggle = 0;
			
			ptd_map_buff->map_list[ptd_index].state	= TD_PTD_NEW;
			
			ptd_map_buff->map_list[ptd_index].qh = NULL;
			ptd_map_buff->map_list[ptd_index].qtd =	NULL;
			ptd_map_buff->map_list[ptd_index].ptd_header_addr =
				0xFFFF;
		}		
	}			
	pehci_entry("--	%s: Exit\n", __FUNCTION__);
}				



static int
pehci_hcd_start_controller(phci_hcd * hcd)
{
	u32 temp = 0;
	u32 command = 0;
	int retval = 0;
	pehci_entry("++	%s: Entered\n",	__FUNCTION__);
	printk(KERN_NOTICE "++ %s: Entered\n", __FUNCTION__);


	command	= isp1763_reg_read16(hcd->dev, hcd->regs.command, command);
	printk(KERN_NOTICE "HC Command Reg val ...1 %x\n", command);

	
	command	|= CMD_RUN;

	isp1763_reg_write16(hcd->dev, hcd->regs.command, command);


	command	&= 0;

	command	= isp1763_reg_read16(hcd->dev, hcd->regs.command, command);
	printk(KERN_NOTICE "HC Command Reg val ...2 %x\n", command);

	
	if ((retval =
		pehci_hcd_handshake(hcd, hcd->regs.command, CMD_RUN, CMD_RUN,
		100000))) {
		err("Host is not up(CMD_RUN) in	1000 usecs\n");
		return retval;
	}

	printk(KERN_NOTICE "ISP1763 HC is running \n");


	
	command	&= 0;
	command	|= 1;

	isp1763_reg_write16(hcd->dev, hcd->regs.configflag, command);
	mdelay(5);

	temp = isp1763_reg_read16(hcd->dev, hcd->regs.configflag, temp);
	pehci_print("%s: Config	Flag reg value:	0x%08x\n", __FUNCTION__, temp);

	
	if ((retval =
		pehci_hcd_handshake(hcd, hcd->regs.configflag, 1, 1, 100))) {
		err("Host is not into ehci mode	in 100 usecs\n");
		return retval;
	}

	mdelay(5);

	pehci_entry("--	%s: Exit\n", __FUNCTION__);
	printk(KERN_NOTICE "-- %s: Exit\n", __FUNCTION__);
	return retval;
}


static void
pehci_hcd_enable_interrupts(phci_hcd * hcd)
{
	u32 temp = 0;
	pehci_entry("++	%s: Entered\n",	__FUNCTION__);
	printk(KERN_NOTICE "++ %s: Entered\n", __FUNCTION__);
	
	temp &=	0;
	
	temp |=	INTR_ENABLE_MASK;
	isp1763_reg_write16(hcd->dev, hcd->regs.interrupt, temp);

	
	temp = 0;
	
#ifdef OTG_PACKAGE
	temp |= INTR_ENABLE_MASK | HC_OTG_INT;
#else
	temp |= INTR_ENABLE_MASK;
#endif	
	pehci_print("%s: enabled mask 0x%08x\n", __FUNCTION__, temp);
	isp1763_reg_write16(hcd->dev, hcd->regs.interruptenable, temp);

	temp = isp1763_reg_read16(hcd->dev, hcd->regs.interruptenable, temp);
	pehci_print("%s: Intr enable reg value:	0x%08x\n", __FUNCTION__, temp);
	
#ifdef HCD_PACKAGE
	temp = 0;
	temp = isp1763_reg_read32(hcd->dev, HC_INT_THRESHOLD_REG, temp);
	temp |= 0x0100000F;
	
	isp1763_reg_write32(hcd->dev, HC_INT_THRESHOLD_REG, temp);
#endif
	
	temp &=	0;
	temp = isp1763_reg_read16(hcd->dev, hcd->regs.hwmodecontrol, temp);
	temp |=	0x01;		
#ifdef EDGE_INTERRUPT
	temp |=	0x02;		
#endif

#ifdef POL_HIGH_INTERRUPT
	temp |=	0x04;		
#endif

	isp1763_reg_write16(hcd->dev, hcd->regs.hwmodecontrol, temp);

	
	
	temp = 0;
	isp1763_reg_write16(hcd->dev, hcd->regs.atl_irq_mask_and, temp);
	temp = 0;
	isp1763_reg_write16(hcd->dev, hcd->regs.atl_irq_mask_or, temp);
	temp = 0;
	isp1763_reg_write16(hcd->dev, hcd->regs.int_irq_mask_and, temp);
	temp = 0x0;
	isp1763_reg_write16(hcd->dev, hcd->regs.int_irq_mask_or, temp);
	temp = 0;
	isp1763_reg_write16(hcd->dev, hcd->regs.iso_irq_mask_and, temp);
	temp = 0xffff;
	isp1763_reg_write16(hcd->dev, hcd->regs.iso_irq_mask_or, temp);

	temp = isp1763_reg_read16(hcd->dev, hcd->regs.iso_irq_mask_or, temp);
	pehci_print("%s:Iso irq	mask reg value:	0x%08x\n", __FUNCTION__, temp);

	pehci_entry("--	%s: Exit\n", __FUNCTION__);
}

static void
pehci_hcd_init_reg(phci_hcd * hcd)
{
	pehci_entry("++	%s: Entered\n",	__FUNCTION__);
	
	hcd->regs.scratch = HC_SCRATCH_REG;

	
	hcd->regs.command = HC_USBCMD_REG;
	hcd->regs.usbstatus = HC_USBSTS_REG;
	hcd->regs.usbinterrupt = HC_INTERRUPT_REG_EHCI;

	hcd->regs.hcsparams = HC_SPARAMS_REG;
	hcd->regs.frameindex = HC_FRINDEX_REG;

	
	hcd->regs.hwmodecontrol	= HC_HWMODECTRL_REG;
	hcd->regs.interrupt = HC_INTERRUPT_REG;
	hcd->regs.interruptenable = HC_INTENABLE_REG;
	hcd->regs.atl_irq_mask_and = HC_ATL_IRQ_MASK_AND_REG;
	hcd->regs.atl_irq_mask_or = HC_ATL_IRQ_MASK_OR_REG;

	hcd->regs.int_irq_mask_and = HC_INT_IRQ_MASK_AND_REG;
	hcd->regs.int_irq_mask_or = HC_INT_IRQ_MASK_OR_REG;
	hcd->regs.iso_irq_mask_and = HC_ISO_IRQ_MASK_AND_REG;
	hcd->regs.iso_irq_mask_or = HC_ISO_IRQ_MASK_OR_REG;
	hcd->regs.buffer_status	= HC_BUFFER_STATUS_REG;
	hcd->regs.interruptthreshold = HC_INT_THRESHOLD_REG;
	
	hcd->regs.reset	= HC_RESET_REG;
	hcd->regs.configflag = HC_CONFIGFLAG_REG;
	hcd->regs.ports[0] = HC_PORTSC1_REG;
	hcd->regs.ports[1] = 0;	
	hcd->regs.ports[2] = 0;
	hcd->regs.ports[3] = 0;
	hcd->regs.pwrdwn_ctrl =	HC_POWER_DOWN_CONTROL_REG;
	
	hcd->regs.isotddonemap = HC_ISO_PTD_DONEMAP_REG;
	hcd->regs.isotdskipmap = HC_ISO_PTD_SKIPMAP_REG;
	hcd->regs.isotdlastmap = HC_ISO_PTD_LASTPTD_REG;

	hcd->regs.inttddonemap = HC_INT_PTD_DONEMAP_REG;

	hcd->regs.inttdskipmap = HC_INT_PTD_SKIPMAP_REG;
	hcd->regs.inttdlastmap = HC_INT_PTD_LASTPTD_REG;

	hcd->regs.atltddonemap = HC_ATL_PTD_DONEMAP_REG;
	hcd->regs.atltdskipmap = HC_ATL_PTD_SKIPMAP_REG;
	hcd->regs.atltdlastmap = HC_ATL_PTD_LASTPTD_REG;

	pehci_entry("--	%s: Exit\n", __FUNCTION__);
}




#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static void
pehci_interrupt_handler(phci_hcd * hcd, struct pt_regs *regs)
{
	spin_lock(&hcd->lock);
#ifdef CONFIG_ISO_SUPPORT
	phcd_iso_handler(hcd, regs);
#endif
	pehci_hcd_intl_worker(hcd, regs);
	pehci_hcd_atl_worker(hcd, regs);
	spin_unlock(&hcd->lock);
	return;
}
#else
static void
pehci_interrupt_handler(phci_hcd * hcd)
{
	spin_lock(&hcd->lock);
#ifdef CONFIG_ISO_SUPPORT
	pehci_hcd_iso_worker(hcd);
#endif
	pehci_hcd_intl_worker(hcd);
	pehci_hcd_atl_worker(hcd);
	spin_unlock(&hcd->lock);
	return;
}
#endif
irqreturn_t pehci_hcd_irq(struct usb_hcd *usb_hcd)
{

	int work = 0;
	phci_hcd *pehci_hcd;
	struct isp1763_dev *dev;
	u32 intr = 0;
	u32 resume=0;
	u32 temp=0;
	u32 irq_mask = 0;

	if (!(usb_hcd->state & USB_STATE_READY)) {
		info("interrupt	handler	state not ready	yet\n");
	usb_hcd->state=USB_STATE_READY;
	
	}

	
	pehci_hcd = usb_hcd_to_pehci_hcd(usb_hcd);
	dev = pehci_hcd->dev;

	spin_lock(&pehci_hcd->lock);
	dev->int_reg = isp1763_reg_read16(dev, HC_INTERRUPT_REG, dev->int_reg);
	
	isp1763_reg_write16(dev, HC_INTERRUPT_REG, dev->int_reg);

	irq_mask = isp1763_reg_read16(dev, HC_INTENABLE_REG, irq_mask);
	dev->int_reg &= irq_mask;

	intr = dev->int_reg;


	if (atomic_read(&pehci_hcd->nuofsofs)) {
		spin_unlock(&pehci_hcd->lock);
		return IRQ_HANDLED;
	}
	atomic_inc(&pehci_hcd->nuofsofs);

	irq_mask=isp1763_reg_read32(dev,HC_USBSTS_REG,0);
	isp1763_reg_write32(dev,HC_USBSTS_REG,irq_mask);
	if(irq_mask & 0x4){  
		if(intr & 0x50) {   
			temp=isp1763_reg_read32(dev,HC_PORTSC1_REG,0);
			if(temp & 0x4){   
				if (dev) {
					if (dev->driver) {
						if (dev->driver->resume) {
						dev->driver->resume(dev);
							resume=1;
						}
					}
				}
			}
		}
	}

	set_bit(HCD_FLAG_SAW_IRQ, &usb_hcd->flags);

#ifndef THREAD_BASED
#ifdef MSEC_INT_BASED
	work = 1;
#else
	if (intr & (HC_MSEC_INT	& INTR_ENABLE_MASK)) {
		work = 1;	
	}

#ifdef USBNET 
	if (intr & HC_MSOF_INT ) {
		struct list_head *pos, *q;
	
		list_for_each_safe(pos, q, &pehci_hcd->cleanup_urb.urb_list) {
		struct isp1763_async_cleanup_urb *tmp;
		
			tmp = list_entry(pos, struct isp1763_async_cleanup_urb, urb_list);
			if (tmp) {
				spin_unlock(&pehci_hcd->lock);
				usb_hcd_giveback_urb(usb_hcd, tmp->urb, tmp->urb->status);
				spin_lock(&pehci_hcd->lock);

				list_del(pos);
				if(tmp)
				kfree(tmp);
			}
		}
		isp1763_reg_write16(dev, HC_INTENABLE_REG, INTR_ENABLE_MASK );
	}
#endif


	if (intr & (HC_INTL_INT	& INTR_ENABLE_MASK)) {
	
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		pehci_hcd_intl_worker(pehci_hcd, regs);
#else
		pehci_hcd_intl_worker(pehci_hcd);
#endif
	
		work = 0;	
	}
	
	if (intr & (HC_ATL_INT & INTR_ENABLE_MASK)) {
	
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		pehci_hcd_atl_worker(pehci_hcd, regs);
#else
		pehci_hcd_atl_worker(pehci_hcd);
#endif
	
		work = 0;	
	}
#ifdef CONFIG_ISO_SUPPORT
	if (intr & (HC_ISO_INT & INTR_ENABLE_MASK)) {
	
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		pehci_hcd_iso_worker(pehci_hcd);
#else
		pehci_hcd_iso_worker(pehci_hcd);
#endif
	
		work = 0;	
	}
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	if (work){
		spin_unlock(&pehci_hcd->lock);
		pehci_interrupt_handler(pehci_hcd, regs);
		spin_lock(&pehci_hcd->lock);
	}
#else
	if (work){
		spin_unlock(&pehci_hcd->lock);
		pehci_interrupt_handler(pehci_hcd);
		spin_lock(&pehci_hcd->lock);
	}
#endif

#else
	if ((intr & (HC_INTL_INT & INTR_ENABLE_MASK)) ||(intr & (HC_ATL_INT & INTR_ENABLE_MASK)))
	{ 
		st_UsbIt_Msg_Struc *stUsbItMsgSnd ;
		
		stUsbItMsgSnd = (st_UsbIt_Msg_Struc *)kmalloc(sizeof(st_UsbIt_Msg_Struc), GFP_ATOMIC);
		if (!stUsbItMsgSnd) return -ENOMEM;
		
		memset(stUsbItMsgSnd, 0, sizeof(stUsbItMsgSnd));
		
		stUsbItMsgSnd->usb_hcd = usb_hcd;
		stUsbItMsgSnd->uIntStatus = NO_SOF_REQ_IN_ISR;
		list_add_tail(&(stUsbItMsgSnd->list), &(g_messList.list));

		pehci_print("\n------------- send mess : %d------------\n",stUsbItMsgSnd->uIntStatus);
		if ((g_stUsbItThreadHandler.phThreadTask != NULL) && (g_stUsbItThreadHandler.lThrdWakeUpNeeded == 0))
		{
			pehci_print("\n------- wake up thread : %d-----\n",stUsbItMsgSnd->uIntStatus);
			g_stUsbItThreadHandler.lThrdWakeUpNeeded = 1;
			wake_up(&(g_stUsbItThreadHandler.ulThrdWaitQhead));
		}
	}
#endif

	atomic_dec(&pehci_hcd->nuofsofs);
	spin_unlock(&pehci_hcd->lock);
		if(resume){
			usb_hcd_poll_rh_status(usb_hcd);
	}
	return IRQ_HANDLED;
}

static int
pehci_hcd_reset(struct usb_hcd *usb_hcd)
{
	u32 command = 0;
	u32 temp = 0;
	phci_hcd *hcd =	usb_hcd_to_pehci_hcd(usb_hcd);
	printk(KERN_NOTICE "++ %s: Entered\n", __FUNCTION__);
	pehci_hcd_init_reg(hcd);
	printk("chipid %x \n", isp1763_reg_read32(hcd->dev, HC_CHIP_ID_REG, temp)); 

	
	temp &=	0;
	temp |=	8;
	isp1763_reg_write16(hcd->dev, hcd->regs.reset, temp);
	mdelay(10);
	
	
	temp &=	0;
	temp |=	1;
	isp1763_reg_write16(hcd->dev, hcd->regs.reset, temp);

	command	= 0;
	do {

		temp = isp1763_reg_read16(hcd->dev, hcd->regs.reset, temp);
		mdelay(10);
		command++;
		if (command > 100) {
			printk("not able to reset\n");
			break;
		}
	} while	(temp &	0x01);


	
	temp = 0;
	temp |=	(1 << 1);
	isp1763_reg_write16(hcd->dev, hcd->regs.reset, temp);
	command	= 0;
	do {
		temp = isp1763_reg_read16(hcd->dev, hcd->regs.reset, temp);
		mdelay(10);
		command++;
		if (command > 100) {
			printk("not able to reset\n");
			break;
		}
	} while	(temp &	0x02);

	
	command	= isp1763_reg_read16(hcd->dev, hcd->regs.command, command);

	command	|= CMD_RESET;
	
	isp1763_reg_write16(hcd->dev, hcd->regs.command, command);
	
	mdelay(200);
	printk("command	%x\n",
		isp1763_reg_read16(hcd->dev, hcd->regs.command, command));
	printk(KERN_NOTICE "-- %s: Exit	\n", __FUNCTION__);
	return 0;
}

static int
pehci_hcd_start(struct usb_hcd *usb_hcd)
{

	int retval;
	int count = 0;
	phci_hcd *pehci_hcd = NULL;
	u32 temp = 0;
	u32 hwmodectrl = 0;
	u32 ul_scratchval = 0;

	pehci_entry("++	%s: Entered\n",	__FUNCTION__);
	pehci_hcd = usb_hcd_to_pehci_hcd(usb_hcd);

	spin_lock_init(&pehci_hcd->lock);
	atomic_set(&pehci_hcd->nuofsofs, 0);
	atomic_set(&pehci_hcd->missedsofs, 0);

	
	pehci_hcd_init_reg(pehci_hcd);

	
	retval = pehci_hcd_reset(usb_hcd);
	if (retval) {
		err("phci_1763_start: error failing with status	%x\n", retval);
		return retval;
	}

	hwmodectrl =
		isp1763_reg_read16(pehci_hcd->dev,
				   pehci_hcd->regs.hwmodecontrol, hwmodectrl);
#ifdef DATABUS_WIDTH_16
	printk(KERN_NOTICE "Mode Ctrl Value before 16width: %x\n", hwmodectrl);
	hwmodectrl &= 0xFFEF;	
	hwmodectrl |= 0x0400;	
#else
	printk(KERN_NOTICE "Mode Ctrl Value before 8width : %x\n", hwmodectrl);
	hwmodectrl |= 0x0010;	
	hwmodectrl |= 0x0400;	
#endif
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.hwmodecontrol,
			    hwmodectrl);

	hwmodectrl =
		isp1763_reg_read16(pehci_hcd->dev,
				   pehci_hcd->regs.hwmodecontrol, hwmodectrl);
	hwmodectrl |=0x9;  
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.hwmodecontrol,
		hwmodectrl);	
	printk(KERN_NOTICE "Mode Ctrl Value after buswidth: %x\n", hwmodectrl);

	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.scratch, 0x3344);

	ul_scratchval =
		isp1763_reg_read16(pehci_hcd->dev, pehci_hcd->regs.scratch,
				   ul_scratchval);
	printk(KERN_NOTICE "Scratch Reg	Value :	%x\n", ul_scratchval);
	if (ul_scratchval != 0x3344) {
		printk(KERN_NOTICE "Scratch Reg	Value Mismatch:	%x\n",
		       ul_scratchval);

	}


	
	
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.buffer_status, 0);
	
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.atltdskipmap,
			    NO_TRANSFER_ACTIVE);
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.inttdskipmap,
			    NO_TRANSFER_ACTIVE);
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.isotdskipmap,
			    NO_TRANSFER_ACTIVE);
	
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.atltddonemap,
			    NO_TRANSFER_DONE);
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.inttddonemap,
			    NO_TRANSFER_DONE);
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.isotddonemap,
			    NO_TRANSFER_DONE);
	
#ifdef HCD_PACKAGE
	
	isp1763_reg_write16(pehci_hcd->dev, OTG_CTRL_SET_REG, 0x0400);
	isp1763_reg_write16(pehci_hcd->dev, OTG_CTRL_CLEAR_REG, 0x0080);
	
	isp1763_reg_write16(pehci_hcd->dev, OTG_CTRL_SET_REG, 0x0000);
	isp1763_reg_write16(pehci_hcd->dev, OTG_CTRL_CLEAR_REG, 0x8000);
	
	#if 0 
	ul_scratchval =	isp1763_reg_read32(pehci_hcd->dev, HC_POWER_DOWN_CONTROL_REG,0);
	ul_scratchval |= 0x006;	
	isp1763_reg_write32(pehci_hcd->dev, HC_POWER_DOWN_CONTROL_REG,ul_scratchval);
	#endif
	
#elif defined(HCD_DCD_PACKAGE)

	
	isp1763_reg_write16(pehci_hcd->dev,OTG_CTRL_SET_REG, 
			OTG_CTRL_DMPULLDOWN |OTG_CTRL_DPPULLDOWN | 
			OTG_CTRL_SW_SEL_HC_DC |OTG_CTRL_OTG_DISABLE);	

	isp1763_reg_write16(pehci_hcd->dev, OTG_CTRL_SET_REG, 0x0480);
	
	isp1763_reg_write16(pehci_hcd->dev, OTG_CTRL_SET_REG, 0x0000);
	isp1763_reg_write16(pehci_hcd->dev, OTG_CTRL_CLEAR_REG, 0x8000);
	ul_scratchval =
		isp1763_reg_read32(pehci_hcd->dev, HC_POWER_DOWN_CONTROL_REG,
		0);
#endif

	
	pehci_hcd_enable_interrupts(pehci_hcd);

	
	retval = pehci_hcd_start_controller(pehci_hcd);
	if (retval) {
		err("phci_1763_start: error failing with status	%x\n", retval);
		return retval;
	}

	
	pehci_hcd_init_map_buffers(pehci_hcd);

	
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.atltdlastmap,
			    0x8000);
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.inttdlastmap, 0x80);
	isp1763_reg_write16(pehci_hcd->dev, pehci_hcd->regs.isotdlastmap, 0x01);
	
	pehci_hcd->next_uframe = -1;
	pehci_hcd->periodic_sched = 0;
	hwmodectrl =
		isp1763_reg_read16(pehci_hcd->dev,
				   pehci_hcd->regs.hwmodecontrol, hwmodectrl);

	
	for (count = 0; count < PTD_PERIODIC_SIZE; count++) {
		pehci_hcd->periodic_list[count].framenumber = 0;
		INIT_LIST_HEAD(&pehci_hcd->periodic_list[count].sitd_itd_head);
	}



	usb_hcd->state = HC_STATE_RUNNING;
	pehci_hcd->state = HC_STATE_RUNNING;


	
	init_timer(&pehci_hcd->rh_timer);
	
	init_timer(&pehci_hcd->watchdog);

	temp = isp1763_reg_read32(pehci_hcd->dev, HC_POWER_DOWN_CONTROL_REG,
				  temp);
	
	temp = 0x3e81bA0;
#if 0
	temp |=	0x306;
#endif
	isp1763_reg_write32(pehci_hcd->dev, HC_POWER_DOWN_CONTROL_REG, temp);
	temp = isp1763_reg_read32(pehci_hcd->dev, HC_POWER_DOWN_CONTROL_REG,
				  temp);
	printk(" Powerdown Reg Val: %x\n", temp);

	pehci_entry("--	%s: Exit\n", __FUNCTION__);

	return 0;
}

static void
pehci_hcd_stop(struct usb_hcd *usb_hcd)
{

	pehci_entry("++	%s: Entered\n",	__FUNCTION__);

	
	if (usb_hcd->state == USB_STATE_RUNNING) {
		mdelay(2);
	}
	if (in_interrupt()) {	
		pehci_info("stopped in_interrupt!\n");

		return;
	}

	
	pehci_rh_control(usb_hcd, ClearPortFeature, USB_PORT_FEAT_POWER,
			 1, NULL, 0);

	
	mdelay(20);
	pehci_entry("--	%s: Exit\n", __FUNCTION__);

	return;
}


static int
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
pehci_hcd_urb_enqueue(struct usb_hcd *usb_hcd, struct usb_host_endpoint *ep,
		struct urb *urb, gfp_t mem_flags)
#else
pehci_hcd_urb_enqueue(struct usb_hcd *usb_hcd, struct urb *urb, gfp_t mem_flags)
#endif
{

	struct list_head qtd_list;
	struct ehci_qh *qh = 0;
	phci_hcd *pehci_hcd = usb_hcd_to_pehci_hcd(usb_hcd);
	int status = 0;
	int temp = 0, max = 0, num_tds = 0, mult = 0;
	urb_priv_t *urb_priv = NULL;
	unsigned long  flags;
	
	pehci_entry("++	%s: Entered\n",	__FUNCTION__);

	
	if (unlikely(atomic_read(&urb->reject))) 
		return -EINVAL;
	
	INIT_LIST_HEAD(&qtd_list);
	urb->transfer_flags &= ~EHCI_STATE_UNLINK;


	temp = usb_pipetype(urb->pipe);
	max = usb_maxpacket(urb->dev, urb->pipe, !usb_pipein(urb->pipe));
	

	if (hcdpowerdown == 1) {
		printk("Enqueue	hcd power down\n");
		return -EINVAL;
	}


	
	if (!hubdev || 
		(urb->dev->parent==usb_hcd->self.root_hub && 
		hubdev!=urb->dev)) {
		if(urb->dev->parent== usb_hcd->self.root_hub) {
			hubdev = urb->dev;
		}
	}

	switch (temp) {
	case PIPE_INTERRUPT:
		
		num_tds	= 1;
		mult = 1 + ((max >> 11)	& 0x03);
		max &= 0x07ff;
		max *= mult;

		if (urb->transfer_buffer_length	> max) {
			err("interrupt urb length is greater then %d\n", max);
			return -EINVAL;
		}

		if (hubdev && urb->dev->parent == usb_hcd->self.root_hub) {
			huburb = urb;
		}

		break;

	case PIPE_CONTROL:
		
		if (No_Data_Phase && No_Status_Phase) {
			printk("Only SetUP Phase\n");
			num_tds	= (urb->transfer_buffer_length == 0) ? 1 :
				((urb->transfer_buffer_length -
				  1) / HC_ATL_PL_SIZE +	1);
		} else if (!No_Data_Phase && No_Status_Phase) {
			printk("SetUP Phase and	Data Phase\n");
			num_tds	= (urb->transfer_buffer_length == 0) ? 2 :
				((urb->transfer_buffer_length -
				  1) / HC_ATL_PL_SIZE +	3);
		} else if (!No_Data_Phase && !No_Status_Phase) {
			num_tds	= (urb->transfer_buffer_length == 0) ? 2 :
				((urb->transfer_buffer_length -
				  1) / HC_ATL_PL_SIZE +	3);
		}
		
		break;
		
	case PIPE_BULK:
		num_tds	=
			(urb->transfer_buffer_length - 1) / HC_ATL_PL_SIZE + 1;
		if ((urb->transfer_flags & URB_ZERO_PACKET)
			&& !(urb->transfer_buffer_length % max)) {
			num_tds++;
		}
		
		break;
		
#ifdef CONFIG_ISO_SUPPORT
	case PIPE_ISOCHRONOUS:
		
		break;
#endif
	default:
		return -EINVAL;	


	}

#ifdef CONFIG_ISO_SUPPORT
	if (temp != PIPE_ISOCHRONOUS) {
#endif
		
		urb_priv = kmalloc(sizeof(urb_priv_t) +
				   num_tds * sizeof(struct ehci_qtd),
				   mem_flags);
		if (!urb_priv) {
			err("memory   allocation error\n");
			return -ENOMEM;
		}

		memset(urb_priv, 0, sizeof(urb_priv_t) +
			num_tds * sizeof(struct ehci_qtd));
		INIT_LIST_HEAD(&urb_priv->qtd_list);
		urb_priv->qtd[0] = NULL;
		urb_priv->length = num_tds;
		{
			int i =	0;
			
			for (i = 0; i <	num_tds; i++) {
				urb_priv->qtd[i] =
					phci_hcd_qtd_allocate(mem_flags);
				if (!urb_priv->qtd[i]) {
					phci_hcd_urb_free_priv(pehci_hcd,
							       urb_priv, NULL);
					return -ENOMEM;
				}
			}
		}
		
		urb->hcpriv = urb_priv;
#ifdef CONFIG_ISO_SUPPORT
	}
#endif

	switch (temp) {
	case PIPE_INTERRUPT:
		phci_hcd_make_qtd(pehci_hcd, &urb_priv->qtd_list,	urb, &status);
		if (status < 0)	{
			return status;
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		qh = phci_hcd_submit_interrupt(pehci_hcd, ep, &urb_priv->qtd_list, urb,
			&status);
#else
		qh = phci_hcd_submit_interrupt(pehci_hcd, &urb_priv->qtd_list, urb,
			&status);
#endif
		if (status < 0)
			return status;
		break;

	case PIPE_CONTROL:
	case PIPE_BULK:

#ifdef THREAD_BASED
	spin_lock_irqsave (&pehci_hcd->lock, flags);
#endif
		phci_hcd_make_qtd(pehci_hcd, &qtd_list,	urb, &status);
		if (status < 0) {
			return status;
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		qh = phci_hcd_submit_async(pehci_hcd, ep, &qtd_list, urb,
			&status);
#else
		qh = phci_hcd_submit_async(pehci_hcd, &qtd_list, urb, &status);
#endif

#ifdef THREAD_BASED
	spin_unlock_irqrestore (&pehci_hcd->lock, flags);
#endif

		if (status < 0) {
			return status;
		}
		break;
#ifdef CONFIG_ISO_SUPPORT
	case PIPE_ISOCHRONOUS:
		iso_dbg(ISO_DBG_DATA,
			"[pehci_hcd_urb_enqueue]: URB Transfer buffer: 0x%08x\n",
			(long) urb->transfer_buffer);
		iso_dbg(ISO_DBG_DATA,
			"[pehci_hcd_urb_enqueue]: URB Buffer Length: %d\n",
			(long) urb->transfer_buffer_length);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		phcd_submit_iso(pehci_hcd, ep, urb, (unsigned long *) &status);
#else
		spin_lock_irqsave(&pehci_hcd->lock, flags);
		phcd_store_urb_pending(pehci_hcd, 0, urb, (int *) &status);
		spin_unlock_irqrestore(&pehci_hcd->lock, flags);
#endif

		return status;

		break;
#endif
	default:
		return -ENODEV;
	}			

#if (defined MSEC_INT_BASED)
	return 0;
#elif (defined THREAD_BASED)
{ 
		st_UsbIt_Msg_Struc *stUsbItMsgSnd ;
		unsigned long flags;
		spin_lock_irqsave(&pehci_hcd->lock,flags);
	
		
		stUsbItMsgSnd = (st_UsbIt_Msg_Struc *)kmalloc(sizeof(st_UsbIt_Msg_Struc), GFP_ATOMIC);
		if (!stUsbItMsgSnd)
		{
			return -ENOMEM;
		}
		
		memset(stUsbItMsgSnd, 0, sizeof(stUsbItMsgSnd));
		
		stUsbItMsgSnd->usb_hcd = usb_hcd;
		stUsbItMsgSnd->uIntStatus = NO_SOF_REQ_IN_REQ;
		spin_lock(&enqueue_lock);
		if(list_empty(&g_enqueueMessList.list))
			list_add_tail(&(stUsbItMsgSnd->list), &(g_enqueueMessList.list));
		spin_unlock(&enqueue_lock);
		
		pehci_print("\n------------- send mess : %d------------\n",stUsbItMsgSnd->uIntStatus);

		
		
		spin_lock(&g_stUsbItThreadHandler.lock);
		if ((g_stUsbItThreadHandler.phThreadTask != NULL) && (g_stUsbItThreadHandler.lThrdWakeUpNeeded == 0))
		{
			pehci_print("\n------- wake up thread : %d-----\n",stUsbItMsgSnd->uIntStatus);
			g_stUsbItThreadHandler.lThrdWakeUpNeeded = 1;
			wake_up(&(g_stUsbItThreadHandler.ulThrdWaitQhead));
		}
		spin_unlock(&g_stUsbItThreadHandler.lock);

		spin_unlock_irqrestore(&pehci_hcd->lock,flags);
	}
	pehci_entry("-- %s: Exit\n",__FUNCTION__);
    return 0;
#else
	
    if (temp != PIPE_ISOCHRONOUS)
	pehci_hcd_td_ptd_submit_urb(pehci_hcd, qh, qh->type);
#endif
	pehci_entry("--	%s: Exit\n", __FUNCTION__);
	return 0;

}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static int
pehci_hcd_urb_dequeue(struct usb_hcd *usb_hcd, struct urb *urb)
#else
static int
pehci_hcd_urb_dequeue(struct usb_hcd *usb_hcd, struct urb *urb, int status)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	int status = 0;
#endif
	int retval = 0;
	td_ptd_map_buff_t *td_ptd_buf;
	td_ptd_map_t *td_ptd_map;
	struct ehci_qh *qh = 0;
	u32 skipmap = 0;
	u32 buffstatus = 0;
	unsigned long flags;
	struct ehci_qtd	*qtd = 0;
	struct usb_host_endpoint *ep;

	struct ehci_qtd	*cancel_qtd = 0;	
	struct urb *cancel_urb = 0;	
	urb_priv_t *cancel_urb_priv = 0;	
	struct _isp1763_qha atlqha;
	struct _isp1763_qha *qha;
	struct isp1763_mem_addr	*mem_addr = 0;
	u32 ormask = 0;
	struct list_head *qtd_list = 0;
	urb_priv_t *urb_priv = (urb_priv_t *) urb->hcpriv;
	phci_hcd *hcd =	usb_hcd_to_pehci_hcd(usb_hcd);
	pehci_entry("++	%s: Entered\n",	__FUNCTION__);

	pehci_info("device %d\n", urb->dev->devnum);

	if(urb_priv==NULL){
		printk("*******urb_priv is NULL*******	%s: Entered\n",	__FUNCTION__);
		return 0;
		}
	spin_lock_irqsave(&hcd->lock, flags);


	switch (usb_pipetype(urb->pipe)) {
	case PIPE_CONTROL:
	case PIPE_BULK:
	
		qh = urb_priv->qh;
		if(qh==NULL)
			break;

		td_ptd_buf = &td_ptd_map_buff[TD_PTD_BUFF_TYPE_ATL];
		td_ptd_map = &td_ptd_buf->map_list[qh->qtd_ptd_index];

		
		if (td_ptd_map->state == TD_PTD_NEW) {
			break;
		}
		if (urb->dev->speed != USB_SPEED_HIGH) {

			cancel_qtd = td_ptd_map->qtd;
			if (!qh	|| !cancel_qtd)	{
				err("Never Error:QH and	QTD must not be	zero\n");
			} else {
				cancel_urb = cancel_qtd->urb;
				cancel_urb_priv	=
					(urb_priv_t *) cancel_urb->hcpriv;
				mem_addr = &cancel_qtd->mem_addr;
				qha = &atlqha;
				memset(qha, 0, sizeof(struct _isp1763_qha));

				skipmap	=
					isp1763_reg_read16(hcd->dev,
							   hcd->regs.
							   atltdskipmap,
							   skipmap);
				skipmap	|= td_ptd_map->ptd_bitmap;
				isp1763_reg_write16(hcd->dev,
						    hcd->regs.atltdskipmap,
						    skipmap);

				isp1763_mem_read(hcd->dev,
						 td_ptd_map->ptd_header_addr, 0,
						 (u32 *) (qha),	PHCI_QHA_LENGTH,
						 0);
				if ((qha->td_info1 & QHA_VALID)
					|| (qha->td_info4 &	QHA_ACTIVE)) {

					qha->td_info2 |= 0x00008000;
					qha->td_info1 |= QHA_VALID;
					qha->td_info4 |= QHA_ACTIVE;
					skipmap	&= ~td_ptd_map->ptd_bitmap;
					ormask |= td_ptd_map->ptd_bitmap;
					isp1763_reg_write16(hcd->dev,
						hcd->regs.
						atl_irq_mask_or,
						ormask);
					isp1763_mem_write(hcd->dev,
						td_ptd_map->
						ptd_header_addr, 0,
						(u32 *) (qha),
						PHCI_QHA_LENGTH, 0);
					
					isp1763_reg_write16(hcd->dev,
						hcd->regs.
						atltdskipmap,
						skipmap);
					udelay(100);
				}

				isp1763_mem_read(hcd->dev,
					td_ptd_map->ptd_header_addr, 0,
					(u32 *) (qha),	PHCI_QHA_LENGTH,
					0);
				if (!(qha->td_info1 & QHA_VALID)
					&& !(qha->td_info4 & QHA_ACTIVE)) {
					printk(KERN_NOTICE
					"ptd has	been retired \n");
				}

			}
		}

		
		td_ptd_buf->pending_ptd_bitmap &= ~td_ptd_map->ptd_bitmap;

		
		td_ptd_map->state = TD_PTD_REMOVE;
		
		td_ptd_buf->pending_ptd_bitmap &= ~td_ptd_map->ptd_bitmap;
		
		td_ptd_map->state = TD_PTD_REMOVE;
		urb_priv->state	|= DELETE_URB;

		
		skipmap	=
			isp1763_reg_read16(hcd->dev, hcd->regs.atltdskipmap,
			skipmap);
		pehci_check("remove skip map %x, ptd map %x\n",	skipmap,
			td_ptd_map->ptd_bitmap);

		buffstatus =
			isp1763_reg_read16(hcd->dev, hcd->regs.buffer_status,
			buffstatus);


		isp1763_reg_write16(hcd->dev, hcd->regs.atltdskipmap,
			skipmap | td_ptd_map->ptd_bitmap);

		while (!(skipmap & td_ptd_map->ptd_bitmap)) {
			udelay(125);

			skipmap	= isp1763_reg_read16(hcd->dev,
				hcd->regs.atltdskipmap,
				skipmap);
		}

		if (skipmap == NO_TRANSFER_ACTIVE) {
			
			pehci_info("disable the	atl buffer\n");
			buffstatus &= ~ATL_BUFFER;
			isp1763_reg_write16(hcd->dev, hcd->regs.buffer_status,
				buffstatus);
		}

		qtd_list = &qh->qtd_list;
		
		pehci_check("num tds %d, urb length %d,device %d\n",
			urb_priv->length, urb->transfer_buffer_length,
			urb->dev->devnum);

		pehci_check("remove first qtd address %p\n", urb_priv->qtd[0]);
		pehci_check("length of the urb %d, completed %d\n",
			urb->transfer_buffer_length, urb->actual_length);
		qtd = urb_priv->qtd[urb_priv->length - 1];
		pehci_check("qtd state is %x\n", qtd->state);


		urb->status=status;
		status = 0;
#ifdef USBNET 
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		pehci_hcd_urb_delayed_complete(hcd, qh, urb, td_ptd_map, NULL);
#else
		pehci_hcd_urb_delayed_complete(hcd, qh, urb, td_ptd_map);
#endif
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		pehci_hcd_urb_complete(hcd, qh, urb, td_ptd_map, NULL);
#else
		pehci_hcd_urb_complete(hcd, qh, urb, td_ptd_map);
#endif

#endif
		break;

	case PIPE_INTERRUPT:
		pehci_check("phci_1763_urb_dequeue: INTR needs to be done\n");
		urb->status = status; 
		status = 0;
		qh = urb_priv->qh;
		if(qh==NULL)
			break;

		td_ptd_buf = &td_ptd_map_buff[TD_PTD_BUFF_TYPE_INTL];
		td_ptd_map = &td_ptd_buf->map_list[qh->qtd_ptd_index];

		
		if (td_ptd_map->state == TD_PTD_NEW) {
			kfree(urb_priv);
			break;
		}

		
		td_ptd_buf->pending_ptd_bitmap &= ~td_ptd_map->ptd_bitmap;

		td_ptd_map->state = TD_PTD_REMOVE;
		urb_priv->state	|= DELETE_URB;

		
		skipmap	=
			isp1763_reg_read16(hcd->dev, hcd->regs.inttdskipmap,
			skipmap);

		isp1763_reg_write16(hcd->dev, hcd->regs.inttdskipmap,
			skipmap | td_ptd_map->ptd_bitmap);
		qtd_list = &qh->qtd_list;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		pehci_hcd_urb_complete(hcd, qh, urb, td_ptd_map, NULL);
#else
		pehci_hcd_urb_complete(hcd, qh, urb, td_ptd_map);
#endif
		break;
#ifdef CONFIG_ISO_SUPPORT
	case PIPE_ISOCHRONOUS:
		pehci_info("urb dequeue %x %x\n", urb,urb->pipe);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	if(urb->dev->speed==USB_SPEED_HIGH){
		retval = usb_hcd_check_unlink_urb(usb_hcd, urb, status);
		if (!retval) {
			pehci_info("[pehci_hcd_urb_dequeue] usb_hcd_unlink_urb_from_ep with status = %d\n", status);
			usb_hcd_unlink_urb_from_ep(usb_hcd, urb);


		}
	}
#endif

		
		status = 0;
		ep=urb->ep;
		spin_unlock_irqrestore(&hcd->lock, flags);
		mdelay(100);


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
						if (urb->hcpriv!= periodic_ep[0]){
#else
						if (urb->ep != periodic_ep[0]){
#endif
	if(!list_empty(&ep->urb_list)){	
		while(!list_empty(&ep->urb_list)){
			urb=container_of(ep->urb_list.next,struct urb,urb_list);
			pehci_info("list is not empty %x %x\n",urb,urb->dev->state);
			if(urb){
		retval = usb_hcd_check_unlink_urb(usb_hcd, urb,0);
		if (!retval) {
			pehci_info("[pehci_hcd_urb_dequeue] usb_hcd_unlink_urb_from_ep with status = %d\n", status);
			usb_hcd_unlink_urb_from_ep(usb_hcd, urb);
		}
			urb->status=-ESHUTDOWN;
	#if LINUX_VERSION_CODE <KERNEL_VERSION(2,6,24)
			usb_hcd_giveback_urb(usb_hcd,urb);
	#else
			usb_hcd_giveback_urb(usb_hcd,urb,urb->status);
	#endif
				
			}
		}
		}else{
	if(urb){
		pehci_info("list empty %x\n",urb->dev->state);
		phcd_clean_urb_pending(hcd, urb);
		retval = usb_hcd_check_unlink_urb(usb_hcd, urb,0);
		if (!retval) {
			pehci_info("[pehci_hcd_urb_dequeue] usb_hcd_unlink_urb_from_ep with status = %d\n", status);
			usb_hcd_unlink_urb_from_ep(usb_hcd, urb);
		}
			urb->status=-ESHUTDOWN;
	#if LINUX_VERSION_CODE <KERNEL_VERSION(2,6,24)
			usb_hcd_giveback_urb(usb_hcd,urb);
	#else
			usb_hcd_giveback_urb(usb_hcd,urb,urb->status);
	#endif
				
			}
			
		}
	}	
#endif
		return 0;
		
		break;
	}

	spin_unlock_irqrestore(&hcd->lock, flags);
	pehci_info("status %d\n", status);
	pehci_entry("--	%s: Exit\n", __FUNCTION__);
	return status;
}


static void
pehci_hcd_endpoint_disable(struct usb_hcd *usb_hcd,
			   struct usb_host_endpoint *ep)
{
	phci_hcd *ehci = usb_hcd_to_pehci_hcd(usb_hcd);
	struct urb *urb;

	unsigned long flags;
	struct ehci_qh *qh;

	pehci_entry("++	%s: Entered\n",	__FUNCTION__);
	
	
	
#ifdef CONFIG_ISO_SUPPORT
	mdelay(100);  
#endif
	spin_lock_irqsave(&ehci->lock, flags);

	qh = ep->hcpriv;

	if (!qh) {
		goto done;
	} else {
#ifdef CONFIG_ISO_SUPPORT
		pehci_info("disable endpoint %x %x\n", ep->desc.bEndpointAddress,qh->type);

		
		if (qh->type == TD_PTD_BUFF_TYPE_ISTL) {

			
			pehci_info("disable %x \n", list_empty(&ep->urb_list));
			while (!list_empty(&ep->urb_list)) {
			
				urb = container_of(ep->urb_list.next,
					struct urb, urb_list);
				if (urb) {
					phcd_clean_urb_pending(ehci, urb);
					spin_unlock_irqrestore(&ehci->lock,
						flags);

					urb->status = -ESHUTDOWN;
					
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
					usb_hcd_giveback_urb(usb_hcd, urb);
#else
					usb_hcd_giveback_urb(usb_hcd, urb,
						urb->status);
#endif
					spin_lock_irqsave(&ehci->lock, flags);

				}

			}
		}
#endif
		
		pehci_complete_device_removal(ehci, qh);
#ifdef CONFIG_ISO_SUPPORT
		phcd_clean_periodic_ep();
#endif
		ep->hcpriv = NULL;

		goto done;
	}
	done:

	ep->hcpriv = NULL;

	spin_unlock_irqrestore(&ehci->lock, flags);
	printk("disable endpoint exit\n");
	pehci_entry("--	%s: Exit\n", __FUNCTION__);
	return;
}

static int
pehci_hcd_get_frame_number(struct usb_hcd *usb_hcd)
{
	u32 framenumber	= 0;
	phci_hcd *pehci_hcd = usb_hcd_to_pehci_hcd(usb_hcd);
	framenumber =
		isp1763_reg_read16(pehci_hcd->dev, pehci_hcd->regs.frameindex,
		framenumber);
	return framenumber;
}

static int
pehci_rh_status_data(struct usb_hcd *usb_hcd, char *buf)
{

	u32 temp = 0, status = 0;
	u32 ports = 0, i, retval = 1;
	unsigned long flags;
	phci_hcd *hcd =	usb_hcd_to_pehci_hcd(usb_hcd);

	if (hcdpowerdown == 1)
		return 0;

	buf[0] = 0;
	if(portchange==1){
		printk("Remotewakeup-enumerate again \n");
		buf[0] |= 2;
		hcd->reset_done[0] = 0;
		return 1;
	}
	
	buf[0] = 0;
	
	ports =	0x1;
	spin_lock_irqsave(&hcd->lock, flags);
	
	for (i = 0; i <	ports; i++) {
		temp = isp1763_reg_read32(hcd->dev, hcd->regs.ports[i],	temp);
		if (temp & PORT_OWNER) {
			if (temp & PORT_CSC) {
				temp &=	~PORT_CSC;
				isp1763_reg_write32(hcd->dev,
						    hcd->regs.ports[i],	temp);
				continue;
			}
		}

		if (!(temp & PORT_CONNECT)) {
			hcd->reset_done[i] = 0;
		}
		if ((temp & (PORT_CSC |	PORT_PEC | PORT_OCC)) != 0) {
			if (i <	7) {
				buf[0] |= 1 << (i + 1);
			} else {
				buf[1] |= 1 << (i - 7);
			}
			status = STS_PCD;
		}
	}

	spin_unlock_irqrestore(&hcd->lock, flags);
	return status ?	retval : 0;
}

static int
pehci_rh_control(struct	usb_hcd	*usb_hcd, u16 typeReq, u16 wValue,
		 u16 wIndex, char *buf,	u16 wLength)
{
	u32 ports = 0;
	u32 temp = 0, status;
	unsigned long flags;
	int retval = 0;
	phci_hcd *hcd =	usb_hcd_to_pehci_hcd(usb_hcd);

	ports =	0x11;

	printk("%s: request %x,wValuse:0x%x, wIndex:0x%x \n",__func__, typeReq,wValue,wIndex);
	
	spin_lock_irqsave(&hcd->lock, flags);
	switch (typeReq) {
	case ClearHubFeature:
		switch (wValue)	{
		case C_HUB_LOCAL_POWER:
		case C_HUB_OVER_CURRENT:
			
			break;
		default:
			goto error;
		}
		break;
	case ClearPortFeature:
		pehci_print("ClearPortFeature:0x%x\n", ClearPortFeature);
		if (!wIndex || wIndex >	(ports & 0xf)) {
			pehci_info
				("ClearPortFeature not valid port number %d, should be %d\n",
				 wIndex, (ports	& 0xf));
			goto error;
		}
		wIndex--;
		temp = isp1763_reg_read32(hcd->dev, hcd->regs.ports[wIndex],
					  temp);
		if (temp & PORT_OWNER) {
			printk("port is	owned by the CC	host\n");
			break;
		}

		switch (wValue)	{
		case USB_PORT_FEAT_ENABLE:
			pehci_print("enable the	port\n");
			isp1763_reg_write32(hcd->dev, hcd->regs.ports[wIndex],
					    temp & ~PORT_PE);

			break;
		case USB_PORT_FEAT_C_ENABLE:
			printk("disable	the port\n");
			isp1763_reg_write32(hcd->dev, hcd->regs.ports[wIndex],
					    temp | PORT_PEC);
			break;
		case USB_PORT_FEAT_SUSPEND:
		case USB_PORT_FEAT_C_SUSPEND:
			printk("clear feature suspend  \n");
			break;
		case USB_PORT_FEAT_POWER:
			if (ports & 0x10) {	
				isp1763_reg_write32(hcd->dev,
						    hcd->regs.ports[wIndex],
						    temp & ~PORT_POWER);
			}
			break;
		case USB_PORT_FEAT_C_CONNECTION:
			pehci_print("connect change, status is 0x%08x\n", temp);
			isp1763_reg_write32(hcd->dev, hcd->regs.ports[wIndex],
					    temp | PORT_CSC);
			break;
		case USB_PORT_FEAT_C_OVER_CURRENT:
			isp1763_reg_write32(hcd->dev, hcd->regs.ports[wIndex],
					    temp | PORT_OCC);
			break;
		default:
			goto error;

		}
		break;

	case GetHubDescriptor:
		pehci_hub_descriptor(hcd, (struct usb_hub_descriptor *)	buf);
		break;

	case GetHubStatus:
		pehci_print("GetHubStatus:0x%x\n", GetHubStatus);
		
		memset(buf, 0, 4);
		break;
	case GetPortStatus:
		pehci_print("GetPortStatus:0x%x\n", GetPortStatus);
		if (!wIndex || wIndex >	(ports & 0xf)) {
			pehci_info
				("GetPortStatus,not valid port number %d, should be %d\n",
				 wIndex, (ports	& 0xf));
			goto error;
		}
		wIndex--;
		status = 0;
		temp = isp1763_reg_read32(hcd->dev, hcd->regs.ports[wIndex],
					  temp);
		printk("root port status:0x%x\n", temp);
		
		if (temp & PORT_CSC) {
			status |= 1 << USB_PORT_FEAT_C_CONNECTION;
			pehci_print("feature CSC 0x%08x	and status 0x%08x  \n",
				    temp, status);
		}
		if(portchange){
			portchange=0;
			status |= 1 << USB_PORT_FEAT_C_CONNECTION;
		}
		
		if (temp & PORT_PEC) {
			status |= 1 << USB_PORT_FEAT_C_ENABLE;
			pehci_print("feature PEC  0x%08x and status 0x%08x  \n",
				    temp, status);
		}
		
		if (temp & PORT_OCC) {
			status |= 1 << USB_PORT_FEAT_C_OVER_CURRENT;
			pehci_print("feature OCC 0x%08x	and status 0x%08x  \n",
				    temp, status);
		}

		
		if ((temp & PORT_RESET)	&& jiffies > hcd->reset_done[wIndex]) {
			status |= 1 << USB_PORT_FEAT_C_RESET;
			pehci_print("feature reset 0x%08x and status 0x%08x\n",
				temp, status);
			printk(KERN_NOTICE
				"feature	reset 0x%08x and status	0x%08x\n", temp,
				status);
			
			isp1763_reg_write32(hcd->dev, hcd->regs.ports[wIndex],
					    temp & ~PORT_RESET);
			do {
				mdelay(20);
				temp = isp1763_reg_read32(hcd->dev,
							  hcd->regs.
							  ports[wIndex], temp);
			} while	(temp &	PORT_RESET);

			
			printk(KERN_NOTICE "after portreset: %x\n", temp);

			temp = phci_check_reset_complete(hcd, wIndex, temp);
			printk(KERN_NOTICE "after checkportreset: %x\n", temp);
		}

		

		if (!(temp & PORT_OWNER)) {

			if (temp & PORT_CONNECT) {
				status |= 1 << USB_PORT_FEAT_CONNECTION;
				status |= 1 << USB_PORT_FEAT_HIGHSPEED;
			}
			if (temp & PORT_PE) {
				status |= 1 << USB_PORT_FEAT_ENABLE;
			}
			if (temp & PORT_SUSPEND) {
				status |= 1 << USB_PORT_FEAT_SUSPEND;
			}
			if (temp & PORT_OC) {
				status |= 1 << USB_PORT_FEAT_OVER_CURRENT;
			}
			if (temp & PORT_RESET) {
				status |= 1 << USB_PORT_FEAT_RESET;
			}
			if (temp & PORT_POWER) {
				status |= 1 << USB_PORT_FEAT_POWER;
			}
		}

		
		*((u32 *) buf) = cpu_to_le32(status);
		break;

	case SetHubFeature:
		pehci_print("SetHubFeature:0x%x\n", SetHubFeature);
		switch (wValue)	{
		case C_HUB_LOCAL_POWER:
		case C_HUB_OVER_CURRENT:
			
			break;
		default:
			goto error;
		}
		break;
	case SetPortFeature:
		pehci_print("SetPortFeature:%x\n", SetPortFeature);
		if (!wIndex || wIndex >	(ports & 0xf)) {
			pehci_info
				("SetPortFeature not valid port	number %d, should be %d\n",
				 wIndex, (ports	& 0xf));
			goto error;
		}
		wIndex--;
		temp = isp1763_reg_read32(hcd->dev, hcd->regs.ports[wIndex],
					  temp);
		pehci_print("SetPortFeature:PortSc Val 0x%x\n",	temp);
		if (temp & PORT_OWNER) {
			break;
		}
		switch (wValue)	{
		case USB_PORT_FEAT_ENABLE:
			
			isp1763_reg_write32(hcd->dev, hcd->regs.ports[wIndex],
				temp | PORT_PE);
			break;
		case USB_PORT_FEAT_SUSPEND:
			
			#if 0 
			isp1763_reg_write32(hcd->dev, hcd->regs.ports[wIndex],
				temp | PORT_SUSPEND);
			#endif
			
			break;
		case USB_PORT_FEAT_POWER:
			pehci_print("Set Port Power 0x%x and Ports %x\n",
				USB_PORT_FEAT_POWER, ports);
			if (ports & 0x10) {
				printk(KERN_NOTICE
					"PortSc Reg %x an Value %x\n",
					hcd->regs.ports[wIndex],
					(temp | PORT_POWER));

				isp1763_reg_write32(hcd->dev,
					hcd->regs.ports[wIndex],
					temp | PORT_POWER);
			}
			break;
		case USB_PORT_FEAT_RESET:
			pehci_print("Set Port Reset 0x%x\n",
				USB_PORT_FEAT_RESET);
			if ((temp & (PORT_PE | PORT_CONNECT)) == PORT_CONNECT
				&& PORT_USB11(temp)) {
				printk("error:port %d low speed	--> companion\n", wIndex + 1);
				temp |=	PORT_OWNER;
			} else {
				temp |=	PORT_RESET;
				temp &=	~PORT_PE;

				hcd->reset_done[wIndex]	= jiffies
					+ ((50   * HZ) / 1000);
			}
			isp1763_reg_write32(hcd->dev, hcd->regs.ports[wIndex],
				temp);
			break;
		default:
			goto error;
		}
		break;
	default:
		pehci_print("this request doesnt fit anywhere\n");
	error:
		
		pehci_info
			("unhandled root hub request: typereq 0x%08x, wValue %d, wIndex	%d\n",
			 typeReq, wValue, wIndex);
		retval = -EPIPE;
	}

	pehci_info("rh_control:exit\n");
	spin_unlock_irqrestore(&hcd->lock, flags);
	return retval;
}




static const struct hc_driver pehci_driver = {
	.description = hcd_name,
	.product_desc =	"ST-ERICSSON ISP1763",
	.hcd_priv_size = sizeof(phci_hcd),
#ifdef LINUX_2620
	.irq = NULL,
#else
	.irq = pehci_hcd_irq,
#endif
	.flags = HCD_USB2 | HCD_MEMORY,

	.reset = pehci_hcd_reset,
	.start = pehci_hcd_start,
	.bus_suspend = pehci_bus_suspend,
	.bus_resume  = pehci_bus_resume,
	.stop =	pehci_hcd_stop,
	.urb_enqueue = pehci_hcd_urb_enqueue,
	.urb_dequeue = pehci_hcd_urb_dequeue,
	.endpoint_disable = pehci_hcd_endpoint_disable,

	.get_frame_number = pehci_hcd_get_frame_number,

	.hub_status_data = pehci_rh_status_data,
	.hub_control = pehci_rh_control,
};


#ifdef THREAD_BASED
int pehci_hcd_process_irq_it_handle(struct usb_hcd* usb_hcd_)
{
	int istatus;
	
	struct usb_hcd 		*usb_hcd;
	char					uIntStatus;
	phci_hcd    *pehci_hcd;

	struct list_head *pos, *lst_tmp;
	st_UsbIt_Msg_Struc *mess;
	unsigned long flags;
	
	g_stUsbItThreadHandler.phThreadTask = current;
	siginitsetinv(&((g_stUsbItThreadHandler.phThreadTask)->blocked), sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM));		
	pehci_info("pehci_hcd_process_irq_it_thread ID : %d\n", g_stUsbItThreadHandler.phThreadTask->pid);
	
	while (1)
	{
		if (signal_pending(g_stUsbItThreadHandler.phThreadTask))
		{
	       	printk("thread handler:  Thread received signal\n");
	       	break;
		}

		spin_lock(&g_stUsbItThreadHandler.lock);
		g_stUsbItThreadHandler.lThrdWakeUpNeeded = 0;
		spin_unlock(&g_stUsbItThreadHandler.lock);
		
		
		istatus = wait_event_interruptible_timeout(g_stUsbItThreadHandler.ulThrdWaitQhead, (g_stUsbItThreadHandler.lThrdWakeUpNeeded== 1), msecs_to_jiffies(MSEC_INTERVAL_CHECKING));

		local_irq_save(flags); 
		spin_lock(&g_stUsbItThreadHandler.lock);
		g_stUsbItThreadHandler.lThrdWakeUpNeeded = 1;
		spin_unlock(&g_stUsbItThreadHandler.lock);
		
		if (!list_empty(&g_messList.list)) 
		{

			list_for_each_safe(pos, lst_tmp, &(g_messList.list))
			{
				mess = list_entry(pos, st_UsbIt_Msg_Struc, list);

				usb_hcd = mess->usb_hcd;
				uIntStatus = mess->uIntStatus;
				
				pehci_hcd = usb_hcd_to_pehci_hcd(usb_hcd);
				if((uIntStatus & NO_SOF_REQ_IN_TSK)  || (uIntStatus & NO_SOF_REQ_IN_ISR) || (uIntStatus & NO_SOF_REQ_IN_REQ))
					pehci_interrupt_handler(pehci_hcd);
				spin_lock(&g_stUsbItThreadHandler.lock);
				list_del(pos);
				kfree(mess);
				spin_unlock(&g_stUsbItThreadHandler.lock);				
			}
		}
		else if(!list_empty(&g_enqueueMessList.list))
		{
			mess = list_first_entry(&(g_enqueueMessList.list), st_UsbIt_Msg_Struc, list);
			usb_hcd = mess->usb_hcd;
			uIntStatus = mess->uIntStatus;

			pehci_print("-------------receive mess : %d------------\n",uIntStatus);
			pehci_hcd = usb_hcd_to_pehci_hcd(usb_hcd);
			if((uIntStatus & NO_SOF_REQ_IN_REQ))
			{
				pehci_interrupt_handler(pehci_hcd);
			}	

			{
				spin_lock(&enqueue_lock);
				list_del((g_enqueueMessList.list).next);
				kfree(mess);
				spin_unlock(&enqueue_lock);
			}	
		}
		else if(istatus == 0) 
		{
			pehci_hcd = NULL;
			pehci_hcd = usb_hcd_to_pehci_hcd(usb_hcd_);
			pehci_interrupt_handler(pehci_hcd);

		}
		local_irq_restore(flags);  
	}

	flush_signals(g_stUsbItThreadHandler.phThreadTask);
	g_stUsbItThreadHandler.phThreadTask = NULL;
	return 0;
	
}

int pehci_hcd_process_irq_in_thread(struct usb_hcd* usb_hcd_)
{
	
	
	INIT_LIST_HEAD(&g_messList.list);
	INIT_LIST_HEAD(&g_enqueueMessList.list);
	spin_lock_init(&enqueue_lock);

	memset(&g_stUsbItThreadHandler, 0, sizeof(st_UsbIt_Thread));
	init_waitqueue_head(&(g_stUsbItThreadHandler.ulThrdWaitQhead));
	g_stUsbItThreadHandler.lThrdWakeUpNeeded = 0;
	spin_lock_init(&g_stUsbItThreadHandler.lock);
	kernel_thread(pehci_hcd_process_irq_it_handle, usb_hcd_, 0);
	
    return 0;
}
#endif


int
pehci_hcd_probe(struct isp1763_dev *isp1763_dev, isp1763_id * ids)
{
#ifdef NON_PCI
    struct platform_device *dev = isp1763_dev->dev;
#else 
	struct pci_dev *dev = isp1763_dev->pcidev;
#endif
	struct usb_hcd *usb_hcd;
	phci_hcd *pehci_hcd;
	int status = 0;

#ifndef NON_PCI
	u32 intcsr=0;
#endif
	pehci_entry("++	%s: Entered\n",	__FUNCTION__);
	if (usb_disabled()) {
		return -ENODEV;
	}

	usb_hcd	= usb_create_hcd(&pehci_driver,&dev->dev, "ISP1763");

	if (usb_hcd == NULL) {
		status = -ENOMEM;
		goto clean;
	}

	
	pehci_hcd = usb_hcd_to_pehci_hcd(usb_hcd);
	pehci_hcd->dev = isp1763_dev;
	pehci_hcd->iobase = (u8	*) isp1763_dev->baseaddress;
	pehci_hcd->iolength = isp1763_dev->length;


	
	isp1763_dev->driver_data = usb_hcd;
#ifdef NON_PCI
#else
	
	
#ifdef DATABUS_WIDTH_16
	wvalue1	= readw(pehci_hcd->plxiobase + 0x68);
	wvalue2	= readw(pehci_hcd->plxiobase + 0x68 + 2);
	intcsr |= wvalue2;
	intcsr <<= 16;
	intcsr |= wvalue1;
	printk(KERN_NOTICE "Enable PCI Intr: %x	\n", intcsr);
	intcsr |= 0x900;
	writew((u16) intcsr, pehci_hcd->plxiobase + 0x68);
	writew((u16) (intcsr >>	16), pehci_hcd->plxiobase + 0x68 + 2);
#else
	bvalue1	= readb(pehci_hcd->plxiobase + 0x68);
	bvalue2	= readb(pehci_hcd->plxiobase + 0x68 + 1);
	bvalue3	= readb(pehci_hcd->plxiobase + 0x68 + 2);
	bvalue4	= readb(pehci_hcd->plxiobase + 0x68 + 3);
	intcsr |= bvalue4;
	intcsr <<= 8;
	intcsr |= bvalue3;
	intcsr <<= 8;
	intcsr |= bvalue2;
	intcsr <<= 8;
	intcsr |= bvalue1;
	writeb((u8) intcsr, pehci_hcd->plxiobase + 0x68);
	writeb((u8) (intcsr >> 8), pehci_hcd->plxiobase	+ 0x68 + 1);
	writeb((u8) (intcsr >> 16), pehci_hcd->plxiobase + 0x68	+ 2);
	writeb((u8) (intcsr >> 24), pehci_hcd->plxiobase + 0x68	+ 3);
#endif
#endif

	No_Data_Phase =	0;
	No_Status_Phase	= 0;
	usb_hcd->self.controller->dma_mask = 0;
	usb_hcd->self.otg_port = 1;
#if 0
#ifndef THREAD_BASED 	
	status = isp1763_request_irq(pehci_hcd_irq, isp1763_dev, usb_hcd);
#endif
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	if (status == 0) {
		status = usb_add_hcd(usb_hcd, isp1763_dev->irq, SA_SHIRQ);
	}
#else 
	usb_hcd->self.uses_dma = 0;
	if (status == 0){
		status = usb_add_hcd(usb_hcd, isp1763_dev->irq,
		IRQF_SHARED | IRQF_DISABLED | IRQF_TRIGGER_LOW);
	}
#endif

#ifdef THREAD_BASED 	
	g_pehci_hcd = pehci_hcd;
#endif

#ifdef USBNET 
	
	INIT_LIST_HEAD(&(pehci_hcd->cleanup_urb.urb_list));
#endif
	enable_irq_wake(isp1763_dev->irq);
	wake_lock_init(&pehci_wake_lock, WAKE_LOCK_SUSPEND,
						dev_name(&dev->dev));
	wake_lock(&pehci_wake_lock);

	pehci_entry("--	%s: Exit\n", __FUNCTION__);
	isp1763_hcd=isp1763_dev;
	return status;

	clean:
	return status;

}
void 
pehci_hcd_powerup(struct	isp1763_dev *dev)
{
	printk("%s\n", __FUNCTION__);
	hcdpowerdown = 0;
	dev->driver->probe(dev,dev->driver->id);

	
}
void
pehci_hcd_powerdown(struct	isp1763_dev *dev)
{
	struct usb_hcd *usb_hcd;

	phci_hcd *hcd = NULL;
	u32 temp;
	usb_hcd = (struct usb_hcd *) dev->driver_data;
	if (!usb_hcd) {
		return;
	}
	
	printk("%s\n", __FUNCTION__);
	hcd = usb_hcd_to_pehci_hcd(usb_hcd);

	temp = isp1763_reg_read16(dev, HC_USBCMD_REG, 0);
	temp &= ~0x01;		
	isp1763_reg_write16(dev, HC_USBCMD_REG, temp);
	printk("++ %s: Entered\n", __FUNCTION__);

	usb_remove_hcd(usb_hcd);
	dev->driver_data = NULL;
	

	temp = isp1763_reg_read16(dev, HC_INTENABLE_REG, temp); 
	temp &= ~0x400;		
	isp1763_reg_write16(dev, HC_INTENABLE_REG, temp); 

	isp1763_reg_write16(dev, HC_UNLOCK_DEVICE, 0xAA37);	
	mdelay(1);
	temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);


	if ((temp & 0x1005) == 0x1005) {
		isp1763_reg_write32(dev, HC_PORTSC1_REG, 0x1000);
		temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
		mdelay(10);
		isp1763_reg_write32(dev, HC_PORTSC1_REG, 0x1104);
		mdelay(10);
		isp1763_reg_write32(dev, HC_PORTSC1_REG, 0x1007);
		temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
		mdelay(10);
		isp1763_reg_write32(dev, HC_PORTSC1_REG, 0x1005);

		temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
	}
	
	printk("port status %x\n ", temp);
	temp &= ~0x2;
	temp &= ~0x40;		
	temp |= 0x80;		

	isp1763_reg_write32(dev, HC_PORTSC1_REG, temp);
	printk("port status %x\n ", temp);
	mdelay(200);

	temp = isp1763_reg_read16(dev, HC_HW_MODE_REG, 0);	
	temp |= 0x2c;
	isp1763_reg_write16(dev, HC_HW_MODE_REG, temp); 
	mdelay(20);

	temp = isp1763_reg_read16(dev, HC_HW_MODE_REG, 0); 
	temp = 0xc;
	isp1763_reg_write16(dev, HC_HW_MODE_REG, temp); 

	isp1763_reg_write32(dev, HC_POWER_DOWN_CONTROL_REG, 0xffff0800);

	wake_unlock(&pehci_wake_lock);
	wake_lock_destroy(&pehci_wake_lock);

	hcdpowerdown = 1;
	
}

static int pehci_bus_suspend(struct usb_hcd *usb_hcd)
{
	u32 temp=0;
	unsigned long flags;
	phci_hcd *pehci_hcd = NULL;
	struct isp1763_dev *dev = NULL;

	
	if (!usb_hcd) {
		return -EBUSY;
	}
	
	printk("++ %s \n",__FUNCTION__);
	pehci_hcd = usb_hcd_to_pehci_hcd(usb_hcd);

	dev = pehci_hcd->dev;
	
	spin_lock_irqsave(&pehci_hcd->lock, flags);
	if(hcdpowerdown){
		spin_unlock_irqrestore(&pehci_hcd->lock, flags);
		return 0;
	}


	isp1763_reg_write32(dev, HC_USBSTS_REG, 0x4); 
	isp1763_reg_write32(dev, HC_INTERRUPT_REG_EHCI, 0x4); 
	isp1763_reg_write16(dev, HC_INTERRUPT_REG, INTR_ENABLE_MASK); 

	temp=isp1763_reg_read16(dev, HC_INTERRUPT_REG, 0); 

	isp1763_reg_write16(dev,HC_INTENABLE_REG,INTR_ENABLE_MASK);
	temp=isp1763_reg_read16(dev,HC_INTENABLE_REG,0);

	hcdpowerdown = 1;
	
	
	temp = isp1763_reg_read16(dev, HC_USBCMD_REG, 0);
	temp &= ~0x01;		
	isp1763_reg_write16(dev, HC_USBCMD_REG, temp);

	
	temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
	temp |= (PORT_SUSPEND);
	isp1763_reg_write32(dev, HC_PORTSC1_REG, temp);
	
	
	temp = isp1763_reg_read16(dev, HC_HW_MODE_REG, 0);
	temp |= 0x20;
	isp1763_reg_write16(dev, HC_HW_MODE_REG, temp);
	mdelay(1); 
	temp &= ~0x20;
	isp1763_reg_write16(dev, HC_HW_MODE_REG, temp);
	
	isp1763_reg_write32(dev, HC_POWER_DOWN_CONTROL_REG, POWER_DOWN_CTRL_SUSPEND_VALUE);


	spin_unlock_irqrestore(&pehci_hcd->lock, flags);

	printk("-- %s \n",__FUNCTION__);

	wake_unlock(&pehci_wake_lock);

	return 0;
	

}

static int pehci_bus_resume(struct usb_hcd *usb_hcd)
{
	u32 temp,i;
	phci_hcd *pehci_hcd = NULL;
	struct isp1763_dev *dev = NULL;
	unsigned long flags;
	u32 portsc1;

	printk("%s Enter \n",__func__);

	if (!usb_hcd) {
		return -EBUSY;
	}

	if(hcdpowerdown ==0){
		printk("%s already executed\n ",__func__);
		return 0;
	}

	pehci_hcd = usb_hcd_to_pehci_hcd(usb_hcd);
	dev = pehci_hcd->dev;
	spin_lock_irqsave(&pehci_hcd->lock, flags);

	for (temp = 0; temp < 100; temp++)
	{
		i = isp1763_reg_read32(dev, HC_CHIP_ID_REG, 0);
		if(i==0x176320)
			break;
		mdelay(2);
	}
	printk("temp=%d, chipid:0x%x \n",temp,i);
	mdelay(10);
	isp1763_reg_write16(dev, HC_UNLOCK_DEVICE, 0xAA37);	
	i = isp1763_reg_read32(dev, HC_POWER_DOWN_CONTROL_REG, 0);
	printk("POWER DOWN CTRL REG value during suspend =0x%x\n", i);
	for (temp = 0; temp < 100; temp++) {
		mdelay(1);
		isp1763_reg_write32(dev, HC_POWER_DOWN_CONTROL_REG, POWER_DOWN_CTRL_NORMAL_VALUE);
		mdelay(1);
		i = isp1763_reg_read32(dev, HC_POWER_DOWN_CONTROL_REG, 0);
		if(i==POWER_DOWN_CTRL_NORMAL_VALUE)
			break;
	}
	if (temp == 100) {
		spin_unlock_irqrestore(&pehci_hcd->lock, flags);
		pr_err("%s:isp1763a failed to resume\n", __func__);
		return -1;
	}

	wake_lock(&pehci_wake_lock);

	printk("%s: Powerdown Reg Val: 0x%08x -- %d\n", __func__, i, temp);

	isp1763_reg_write32(dev, HC_USBSTS_REG,0x0); 
	isp1763_reg_write32(dev, HC_INTERRUPT_REG_EHCI, 0x0); 
	isp1763_reg_write16(dev, HC_INTENABLE_REG,0); 

	portsc1 = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
	printk("%s PORTSC1: 0x%x\n", __func__, portsc1);

	temp = isp1763_reg_read16(dev, HC_USBCMD_REG, 0);
	temp |= 0x01;		
	isp1763_reg_write16(dev, HC_USBCMD_REG, temp);
	mdelay(10);

	temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
	if (temp & PORT_SUSPEND)
		pr_err("%s: HC_PORTSC1_REG: 0x%08x\n", __func__, temp);
	temp |= PORT_SUSPEND;    
	isp1763_reg_write32(dev, HC_PORTSC1_REG, temp);
	mdelay(50);
	temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
	temp |= PORT_RESUME;     
	temp &= ~(PORT_SUSPEND); 
	isp1763_reg_write32(dev, HC_PORTSC1_REG, temp);
	temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
	temp &= ~(PORT_RESUME);  
	isp1763_reg_write32(dev, HC_PORTSC1_REG, temp);

	temp = INTR_ENABLE_MASK;
	isp1763_reg_write16(dev, HC_INTENABLE_REG, temp); 
	temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
	printk("%s resume port status: 0x%x\n", __func__, temp);
	if(!(temp & 0x4)){ 
		isp1763_reg_write16(dev, HC_INTENABLE_REG, 0x1005); 
		mdelay(10);
	}

	hcdpowerdown = 0;
	if(hubdev){
		hubdev->hcd_priv    = NULL;
		hubdev->hcd_suspend = NULL;
	}

	spin_unlock_irqrestore(&pehci_hcd->lock, flags);
	printk("%s Leave\n",__func__);

	return 0;
}

void
pehci_hcd_resume(struct	isp1763_dev *dev)
{
	struct usb_hcd *usb_hcd;
	u32 temp,i;
	usb_hcd = (struct usb_hcd *) dev->driver_data;
	if (!usb_hcd) {
		return;
	}

	if(hcdpowerdown ==0){
		return ;
	}

	printk("%s \n",__FUNCTION__);

	for (temp = 0; temp < 10; temp++)
	{
	i = isp1763_reg_read32(dev, HC_CHIP_ID_REG, 0);
	printk("temp=%d, chipid:0x%x \n",temp,i);
	if(i==0x176320)
	break;
	mdelay(1);
	}

	
	temp = 0x01;		
	isp1763_reg_write16(dev, HC_USBCMD_REG, temp);

	
	for (temp = 0; temp < 100; temp++) {
		isp1763_reg_write32(dev, HC_POWER_DOWN_CONTROL_REG, POWER_DOWN_CTRL_NORMAL_VALUE);
		i = isp1763_reg_read32(dev, HC_POWER_DOWN_CONTROL_REG, 0);
		if(i==POWER_DOWN_CTRL_NORMAL_VALUE)
		break;
	}
	
	if (temp == 100) {
		pr_err("%s:isp1763a failed to resume\n", __func__);
		return;
	}

	wake_lock(&pehci_wake_lock);

	isp1763_reg_write16(dev, HC_INTENABLE_REG,0); 
	isp1763_reg_write32(dev,HC_INTERRUPT_REG_EHCI,0x4); 
	isp1763_reg_write32(dev,HC_INTERRUPT_REG,0xFFFF); 
		
	temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
	temp &= ~(PORT_SUSPEND); 
	temp &= ~(PORT_RESUME);  
	isp1763_reg_write32(dev, HC_PORTSC1_REG, temp);
	
	isp1763_reg_write16(dev, HC_INTENABLE_REG, INTR_ENABLE_MASK); 
		
	mdelay(1);
	temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
	printk("after hcd resume :port status %x\n ", temp);
	
	hcdpowerdown = 0;	

	phci_resume_wakeup(dev);

	if(hubdev){
		hubdev->hcd_priv=NULL;
		hubdev->hcd_suspend=NULL;
	}

}


void
pehci_hcd_suspend(struct isp1763_dev *dev)
{
	struct usb_hcd *usb_hcd;
	u32 temp;
	usb_hcd = (struct usb_hcd *) dev->driver_data;
	if (!usb_hcd) {
		return;
	}
	printk("%s \n",__FUNCTION__);
	if(hcdpowerdown){
		return ;
	}

	temp = isp1763_reg_read16(dev, HC_USBCMD_REG, 0);
	temp &= ~0x01;		
	isp1763_reg_write16(dev, HC_USBCMD_REG, temp);

	isp1763_reg_write32(dev, HC_USBSTS_REG, 0x4); 
	isp1763_reg_write32(dev, HC_INTERRUPT_REG_EHCI, 0x4); 
	isp1763_reg_write16(dev, HC_INTERRUPT_REG, INTR_ENABLE_MASK); 
	
	temp=isp1763_reg_read16(dev, HC_INTERRUPT_REG, 0); 

	printk("suspend :Interrupt Status %x\n",temp);
	isp1763_reg_write16(dev,HC_INTENABLE_REG,INTR_ENABLE_MASK);
	temp=isp1763_reg_read16(dev,HC_INTENABLE_REG,0);
	printk("suspend :Interrupt Enable %x\n",temp);
	temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
	
	printk("suspend :port status %x\n ", temp);
	temp &= ~0x2;
	temp &= ~0x40;		
	temp |= 0x80;		
	isp1763_reg_write32(dev, HC_PORTSC1_REG, temp);
  
	temp = isp1763_reg_read32(dev, HC_PORTSC1_REG, 0);
	printk("suspend :port status %x\n ", temp);
	hcdpowerdown = 1;


	temp = isp1763_reg_read16(dev,HC_HW_MODE_REG, 0);	
	temp&=0xff7b;
	isp1763_reg_write16(dev, HC_HW_MODE_REG, temp); 


	temp = isp1763_reg_read16(dev, HC_HW_MODE_REG, 0);	
	temp |= 0x20;
	isp1763_reg_write16(dev, HC_HW_MODE_REG, temp);
	mdelay(2);
	temp = isp1763_reg_read16(dev, HC_HW_MODE_REG, 0);
	temp &= 0xffdf;
	temp &= ~0x20;
	isp1763_reg_write16(dev, HC_HW_MODE_REG, temp);

	isp1763_reg_write32(dev, HC_POWER_DOWN_CONTROL_REG, 0xffff0830);

	wake_unlock(&pehci_wake_lock);
	
}

void 
pehci_hcd_remotewakeup(struct isp1763_dev *dev){
	if(hubdev){
		hubdev->hcd_priv=dev;
		hubdev->hcd_suspend=(void *)pehci_hcd_suspend;
		}
	phci_remotewakeup(dev);
}

static void
pehci_hcd_remove(struct	isp1763_dev *isp1763_dev)
{

	struct usb_hcd *usb_hcd;
	
#ifdef NON_PCI
#else	
#endif

	phci_hcd *hcd =	NULL;
	u32 temp;
	usb_hcd	= (struct usb_hcd *) isp1763_dev->driver_data;
	if (!usb_hcd) {
		return;
	}
	hcd=usb_hcd_to_pehci_hcd(usb_hcd);
	isp1763_reg_write32(hcd->dev,hcd->regs.hwmodecontrol,0);
	isp1763_reg_write32(hcd->dev,hcd->regs.interruptenable,0);
	hubdev=0;
	huburb=0;
	temp = isp1763_reg_read16(hcd->dev, HC_USBCMD_REG, 0);
	temp &= ~0x01;		
	isp1763_reg_write16(hcd->dev, HC_USBCMD_REG, temp);
	usb_remove_hcd(usb_hcd);

	wake_unlock(&pehci_wake_lock);
	wake_lock_destroy(&pehci_wake_lock);

	return ;
}


static isp1763_id ids =	{
	.idVendor = 0x04CC,	
	.idProduct = 0x1A64,	
	.driver_info = (unsigned long) &pehci_driver,
};

static struct isp1763_driver pehci_hcd_pci_driver = {
	.name =	(char *) hcd_name,
	.index = 0,
	.id = &ids,
	.probe = pehci_hcd_probe,
	.remove	= pehci_hcd_remove,
	.suspend = pehci_hcd_suspend,
	.resume	= pehci_hcd_resume,
	.remotewakeup=pehci_hcd_remotewakeup,
	.powerup	=	pehci_hcd_powerup,
	.powerdown	=	pehci_hcd_powerdown,
};

#ifdef HCD_PACKAGE
int
usb_hcddev_open(struct inode *inode, struct file *fp)
{

	return 0;
}

int
usb_hcddev_close(struct inode *inode, struct file *fp)
{

	return 0;
}

int
usb_hcddev_fasync(int fd, struct file *fp, int mode)
{

	return fasync_helper(fd, fp, mode, &fasync_q);
}

long
usb_hcddev_ioctl(struct file *fp,
		 unsigned int cmd, unsigned long arg)
{

	switch (cmd) {
	case HCD_IOC_POWERDOWN:	
		printk("HCD IOC POWERDOWN MODE\n");
		if(isp1763_hcd->driver->powerdown)
			isp1763_hcd->driver->powerdown(isp1763_hcd);

		break;

	case HCD_IOC_POWERUP:	
		printk("HCD IOC POWERUP MODE\n");
		if(isp1763_hcd->driver->powerup)
			isp1763_hcd->driver->powerup(isp1763_hcd);

		break;
	case HCD_IOC_TESTSE0_NACK:
		HostComplianceTest = HOST_COMPILANCE_TEST_ENABLE;
		HostTest = HOST_COMP_TEST_SE0_NAK;
		break;
	case   HCD_IOC_TEST_J:		
		HostComplianceTest = HOST_COMPILANCE_TEST_ENABLE;
		HostTest = HOST_COMP_TEST_J;
		break;
	case    HCD_IOC_TEST_K:
		HostComplianceTest = HOST_COMPILANCE_TEST_ENABLE;
		HostTest = HOST_COMP_TEST_K;
		break;
		
	case   HCD_IOC_TEST_TESTPACKET:
		HostComplianceTest = HOST_COMPILANCE_TEST_ENABLE;
		HostTest = HOST_COMP_TEST_PACKET;
		break;
	case HCD_IOC_TEST_FORCE_ENABLE:
		HostComplianceTest = HOST_COMPILANCE_TEST_ENABLE;
		HostTest = HOST_COMP_TEST_FORCE_ENABLE;
		break;
	case	HCD_IOC_TEST_SUSPEND_RESUME:
		HostComplianceTest = HOST_COMPILANCE_TEST_ENABLE;
		HostTest = HOST_COMP_HS_HOST_PORT_SUSPEND_RESUME;
		break;
	case HCD_IOC_TEST_SINGLE_STEP_GET_DEV_DESC:
		HostComplianceTest = HOST_COMPILANCE_TEST_ENABLE;
		HostTest = HOST_COMP_SINGLE_STEP_GET_DEV_DESC;		
		break;
	case HCD_IOC_TEST_SINGLE_STEP_SET_FEATURE:
		HostComplianceTest = HOST_COMPILANCE_TEST_ENABLE;
		HostTest = HOST_COMP_SINGLE_STEP_SET_FEATURE;		
		break;
	case HCD_IOC_TEST_STOP:
		HostComplianceTest = 0;
		HostTest = 0;		
		break;
	case     HCD_IOC_SUSPEND_BUS:
		printk("isp1763:SUSPEND bus\n");
		if(isp1763_hcd->driver->suspend)
			isp1763_hcd->driver->suspend(isp1763_hcd);
		break;
	case	HCD_IOC_RESUME_BUS:
		printk("isp1763:RESUME bus\n");
		if(isp1763_hcd->driver->resume)
			isp1763_hcd->driver->resume(isp1763_hcd);		
		break;
	case     HCD_IOC_REMOTEWAKEUP_BUS:
		printk("isp1763:SUSPEND bus\n");
		if(isp1763_hcd->driver->remotewakeup)
			isp1763_hcd->driver->remotewakeup(isp1763_hcd);
		break;		
	default:

		break;

	}
	return 0;
}


static struct file_operations usb_hcddev_fops = {
	owner:THIS_MODULE,
	read:NULL,
	write:NULL,
	poll:NULL,
	unlocked_ioctl:usb_hcddev_ioctl,
	open:usb_hcddev_open,
	release:usb_hcddev_close,
	fasync:usb_hcddev_fasync,
};

#endif


static int __init
pehci_module_init(void)
{
	int result = 0;
	phci_hcd_mem_init();

	
	result = isp1763_register_driver(&pehci_hcd_pci_driver);
	if (!result) {
		info("Host Driver has been Registered");
	} else {
		err("Host Driver has not been Registered with errors : %x",
			result);
	}

#ifdef THREAD_BASED 	
	pehci_hcd_process_irq_in_thread(&(g_pehci_hcd->usb_hcd));
   	printk("kernel_thread() Enter\n"); 
#endif
	
#ifdef HCD_PACKAGE
	printk("Register Char Driver for HCD\n");
	result = register_chrdev(USB_HCD_MAJOR, USB_HCD_MODULE_NAME,
		&usb_hcddev_fops);
	
#endif
	return result;

}

static void __exit
pehci_module_cleanup(void)
{
#ifdef THREAD_BASED	
	printk("module exit:  Sending signal to stop thread\n");
	if (g_stUsbItThreadHandler.phThreadTask != NULL)
	{
		send_sig(SIGKILL, g_stUsbItThreadHandler.phThreadTask, 1);
		mdelay(6);
	}
#endif

#ifdef HCD_PACKAGE
	unregister_chrdev(USB_HCD_MAJOR, USB_HCD_MODULE_NAME);
#endif
	isp1763_unregister_driver(&pehci_hcd_pci_driver);
}

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
module_init(pehci_module_init);
module_exit(pehci_module_cleanup);
