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
* This is a host controller driver file. Isochronous event processing is handled here.
* 
* Author : wired support <wired.support@stericsson.com>
*
*/
#ifdef CONFIG_ISO_SUPPORT
void phcd_clean_periodic_ep(void);
#endif

#ifdef CONFIG_ISO_SUPPORT

#define MAX_URBS		8
#define MAX_EPS			2
#define NUMMICROFRAME		8
struct urb *gstUrb_pending[MAX_URBS] = { 0, 0, 0, 0, 0, 0, 0, 0 };

struct usb_host_endpoint *periodic_ep[MAX_EPS];

int giUrbCount = 0;		
int giUrbIndex = 0;		
void *
phcd_iso_sitd_to_ptd(phci_hcd * hcd,
	struct ehci_sitd *sitd, struct urb *urb, void *ptd)
{
	struct _isp1763_isoptd *iso_ptd;
	struct isp1763_mem_addr *mem_addr;

	unsigned long max_packet, mult, length, td_info1, td_info3;
	unsigned long token, port_num, hub_num, data_addr;
	unsigned long frame_number;

	iso_dbg(ISO_DBG_ENTRY, "phcd_iso_sitd_to_ptd entry\n");

	
	iso_ptd = (struct _isp1763_isoptd *) ptd;
	mem_addr = &sitd->mem_addr;

	max_packet = usb_maxpacket(urb->dev, urb->pipe,usb_pipeout(urb->pipe));

	mult = 1 + ((max_packet >> 11) & 0x3);
	max_packet &= 0x7ff;

	
	length = sitd->length;

	td_info1 = QHA_VALID;

	td_info1 |= (length << 3);

	if (urb->dev->speed != USB_SPEED_HIGH) {
		if (usb_pipein(urb->pipe) && (max_packet > 192)) {
			iso_dbg(ISO_DBG_INFO,
				"IN Max packet over maximum\n");
			max_packet = 192;
		}

		if ((!usb_pipein(urb->pipe)) && (max_packet > 188)) {
			iso_dbg(ISO_DBG_INFO,
				"OUT Max packet over maximum\n");
			max_packet = 188;
		}
	}
	td_info1 |= (max_packet << 18);

	td_info1 |= (usb_pipeendpoint(urb->pipe) << 31);

	if (urb->dev->speed == USB_SPEED_HIGH) {
		td_info1 |= MULTI(mult);
	}

	
	iso_ptd->td_info1 = td_info1;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD0 = 0x%08x\n",
		iso_ptd->td_info1);

	token = (usb_pipeendpoint(urb->pipe) & 0xE) >> 1;

	token |= usb_pipedevice(urb->pipe) << 3;

	
	if (urb->dev->speed != USB_SPEED_HIGH) {
		token |= 1 << 14;

		port_num = urb->dev->ttport;
		hub_num = urb->dev->tt->hub->devnum;

		
		token |= port_num << 18;

		token |= hub_num << 25;
	}

	
	if (usb_pipein(urb->pipe)) {
		token |= (IN_PID << 10);
	}

	
	token |= EPTYPE_ISO;

	
	iso_ptd->td_info2 = token;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD1 = 0x%08x\n",
		iso_ptd->td_info2);

	data_addr = ((unsigned long) (mem_addr->phy_addr) & 0xffff) - 0x400;
	data_addr >>= 3;

	
	td_info3 =( 0xffff&data_addr) << 8;

	frame_number = sitd->framenumber;
	frame_number = sitd->start_frame;
	td_info3 |= (0xff& ((frame_number) << 3));

	
	iso_ptd->td_info3 = td_info3;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD2 = 0x%08x\n",
		iso_ptd->td_info3);

	iso_ptd->td_info4 = QHA_ACTIVE;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD3 = 0x%08x\n",
		iso_ptd->td_info4);

	
	if (usb_pipein(urb->pipe)){
		iso_ptd->td_info5 = (sitd->ssplit);
	}else{
		iso_ptd->td_info5 = (sitd->ssplit << 2);
	}
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD4 = 0x%08x\n",
		iso_ptd->td_info5);

	iso_ptd->td_info6 = sitd->csplit;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD5 = 0x%08x\n",
		iso_ptd->td_info6);

	iso_dbg(ISO_DBG_EXIT, "phcd_iso_itd_to_ptd exit\n");
	return iso_ptd;
}


void *
phcd_iso_itd_to_ptd(phci_hcd * hcd,
	struct ehci_itd *itd, struct urb *urb, void *ptd)
{
	struct _isp1763_isoptd *iso_ptd;
	struct isp1763_mem_addr *mem_addr;

	unsigned long max_packet, mult, length, td_info1, td_info3;
	unsigned long token, port_num, hub_num, data_addr;
	unsigned long frame_number;
	int maxpacket;
	iso_dbg(ISO_DBG_ENTRY, "phcd_iso_itd_to_ptd entry\n");

	
	iso_ptd = (struct _isp1763_isoptd *) ptd;
	mem_addr = &itd->mem_addr;

	max_packet = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));

	maxpacket = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));	

	maxpacket &= 0x7ff;
	mult = 1 + ((max_packet >> 11) & 0x3);


	max_packet &= 0x7ff;

	
	length = itd->length;

	td_info1 = QHA_VALID;

	td_info1 |= (length << 3);

	if (urb->dev->speed != USB_SPEED_HIGH) {
		if (usb_pipein(urb->pipe) && (max_packet > 192)) {
			iso_dbg(ISO_DBG_INFO,
				"[phcd_iso_itd_to_ptd]: IN Max packet over maximum\n");
			max_packet = 192;
		}

		if ((!usb_pipein(urb->pipe)) && (max_packet > 188)) {
			iso_dbg(ISO_DBG_INFO,
				"[phcd_iso_itd_to_ptd]: OUT Max packet over maximum\n");
			max_packet = 188;
		}
	} else {		

		if (max_packet > 1024){
			max_packet = 1024;
		}
	}
	td_info1 |= (max_packet << 18);

	td_info1 |= (usb_pipeendpoint(urb->pipe) << 31);

	if (urb->dev->speed == USB_SPEED_HIGH) {
		td_info1 |= MULTI(mult);
	}

	
	iso_ptd->td_info1 = td_info1;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD0 = 0x%08x\n",
		iso_ptd->td_info1);

	token = (usb_pipeendpoint(urb->pipe) & 0xE) >> 1;

	token |= usb_pipedevice(urb->pipe) << 3;

	
	if (urb->dev->speed != USB_SPEED_HIGH) {
		token |= 1 << 14;

		port_num = urb->dev->ttport;
		hub_num = urb->dev->tt->hub->devnum;

		
		token |= port_num << 18;

		token |= hub_num << 25;
	}

	
	if (usb_pipein(urb->pipe)){
		token |= (IN_PID << 10);
	}

	
	token |= EPTYPE_ISO;

	
	iso_ptd->td_info2 = token;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD1 = 0x%08x\n",
		iso_ptd->td_info2);

	data_addr = ((unsigned long) (mem_addr->phy_addr) & 0xffff) - 0x400;
	data_addr >>= 3;

	
	td_info3 = (data_addr&0xffff) << 8;

	frame_number = itd->framenumber;
	td_info3 |= (0xff&(frame_number << 3));

	
	iso_ptd->td_info3 = td_info3;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD2 = 0x%08x\n",
		iso_ptd->td_info3);

	iso_ptd->td_info4 = QHA_ACTIVE;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD3 = 0x%08x\n",
		iso_ptd->td_info4);

	
	iso_ptd->td_info5 = itd->ssplit;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD4 = 0x%08x\n",
		iso_ptd->td_info5);

	iso_ptd->td_info6 = itd->csplit;
	iso_dbg(ISO_DBG_DATA, "[phcd_iso_itd_to_ptd]: DWORD5 = 0x%08x\n",
		iso_ptd->td_info6);

	iso_dbg(ISO_DBG_EXIT, "phcd_iso_itd_to_ptd exit\n");
	return iso_ptd;
}				

unsigned long
phcd_iso_scheduling_info(phci_hcd * hcd,
	struct ehci_qh *qhead,
	unsigned long max_pkt,
	unsigned long high_speed, unsigned long ep_in)
{
	unsigned long count, usof, temp;

	
	usof = 0x1;

	if (high_speed) {
		qhead->csplit = 0;

		
		qhead->ssplit = 0x1;
		return 0;
	}

	
	count = max_pkt / 188;

	if (max_pkt % 188){
		count += 1;
	}

	for (temp = 0; temp < count; temp++){
		usof |= (0x1 << temp);
	}

	if (ep_in) {
		qhead->ssplit = 0x1;

		qhead->csplit = (usof << 2);
	} else {
		qhead->ssplit = usof;
		qhead->csplit = 0;
	}	
	return 0;
}				

unsigned long
phcd_iso_sitd_fill(phci_hcd * hcd,
	struct ehci_sitd *sitd,
	struct urb *urb, unsigned long packets)
{
	unsigned long length, offset, pipe;
	unsigned long max_pkt;
	dma_addr_t buff_dma;
	struct isp1763_mem_addr *mem_addr;

#ifdef COMMON_MEMORY
	struct ehci_qh *qhead = NULL;
#endif

	iso_dbg(ISO_DBG_ENTRY, "phcd_iso_itd_fill entry\n");
	length = urb->iso_frame_desc[packets].length;
	offset = urb->iso_frame_desc[packets].offset;

	
	urb->iso_frame_desc[packets].actual_length = 0;
	urb->iso_frame_desc[packets].status = -EXDEV;

	
	buff_dma = (u32) ((unsigned char *) urb->transfer_buffer + offset);

	
	mem_addr = &sitd->mem_addr;

	pipe = urb->pipe;
	max_pkt = usb_maxpacket(urb->dev, pipe, usb_pipeout(pipe));

	max_pkt = max_pkt & 0x7FF;

	if ((length < 0) || (max_pkt < length)) {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No available memory.\n");
		return -ENOSPC;
	}
	sitd->buf_dma = buff_dma;


#ifndef COMMON_MEMORY
	phci_hcd_mem_alloc(length, mem_addr, 0);
	if (length && ((mem_addr->phy_addr == 0) || (mem_addr->virt_addr == 0))) {
		mem_addr = 0;
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No payload memory available\n");
		return -ENOMEM;
	}
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	qhead=urb->hcpriv;
#else
	qhead = urb->ep->hcpriv;
#endif
	if (qhead) {

		mem_addr->phy_addr = qhead->memory_addr.phy_addr + offset;

		mem_addr->virt_addr = qhead->memory_addr.phy_addr + offset;
	} else {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No payload memory available\n");
		return -ENOMEM;
	}


#endif
	
	sitd->length = length;

	
	sitd->hw_bufp[0] = buff_dma;

	iso_dbg(ISO_DBG_EXIT, "phcd_iso_sitd_fill exit\n");
	return 0;
}

unsigned long
phcd_iso_itd_fill(phci_hcd * hcd,
	struct ehci_itd *itd,
	struct urb *urb,
	unsigned long packets, unsigned char numofPkts)
{
	unsigned long length, offset, pipe;
	unsigned long max_pkt, mult;
	dma_addr_t buff_dma;
	struct isp1763_mem_addr *mem_addr;
#ifdef COMMON_MEMORY
	struct ehci_qh *qhead = NULL;
#endif
	int i = 0;

	iso_dbg(ISO_DBG_ENTRY, "phcd_iso_itd_fill entry\n");
	for (i = 0; i < 8; i++){
		itd->hw_transaction[i] = 0;
	}
	length = urb->iso_frame_desc[packets].length;
	offset = urb->iso_frame_desc[packets].offset;

	
	urb->iso_frame_desc[packets].actual_length = 0;
	urb->iso_frame_desc[packets].status = -EXDEV;

	
	buff_dma = cpu_to_le32((unsigned char *) urb->transfer_buffer + offset);

	
	mem_addr = &itd->mem_addr;

	pipe = urb->pipe;
	max_pkt = usb_maxpacket(urb->dev, pipe, usb_pipeout(pipe));

	mult = 1 + ((max_pkt >> 11) & 0x3);
	max_pkt = max_pkt & 0x7FF;
	max_pkt *= mult;

	if ((length < 0) || (max_pkt < length)) {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No available memory.\n");
		return -ENOSPC;
	}
	itd->buf_dma = buff_dma;
	for (i = packets + 1; i < numofPkts + packets; i++)
		length += urb->iso_frame_desc[i].length;

#ifndef COMMON_MEMORY

	phci_hcd_mem_alloc(length, mem_addr, 0);
	if (length && ((mem_addr->phy_addr == 0) || (mem_addr->virt_addr == 0))) {
		mem_addr = 0;
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No payload memory available\n");
		return -ENOMEM;
	}
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	qhead = urb->ep->hcpriv;
#else
	qhead=urb->hcpriv;
#endif
	if (qhead) {

		mem_addr->phy_addr = qhead->memory_addr.phy_addr + offset;

		mem_addr->virt_addr = qhead->memory_addr.phy_addr + offset;
	} else {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_fill Error]: No payload memory available\n");
		return -ENOMEM;
	}


#endif
	
	itd->length = length;

	
	itd->multi = mult;

	
	itd->hw_bufp[0] = buff_dma;

	iso_dbg(ISO_DBG_EXIT, "phcd_iso_itd_fill exit\n");
	return 0;
}				

void
phcd_iso_get_sitd_ptd_index(phci_hcd * hcd, struct ehci_sitd *sitd)
{
	td_ptd_map_buff_t *ptd_map_buff;
	unsigned long buff_type, max_ptds;
	unsigned char sitd_index, bitmap;

	
	bitmap = 0x1;
	buff_type = td_ptd_pipe_x_buff_type[TD_PTD_BUFF_TYPE_ISTL];
	ptd_map_buff = (td_ptd_map_buff_t *) & (td_ptd_map_buff[buff_type]);
	max_ptds = ptd_map_buff->max_ptds;
	sitd->sitd_index = TD_PTD_INV_PTD_INDEX;

	for (sitd_index = 0; sitd_index < max_ptds; sitd_index++) {
		if (ptd_map_buff->map_list[sitd_index].state == TD_PTD_NEW) {
			iso_dbg(ISO_DBG_INFO,
				"[phcd_iso_get_itd_ptd_index] There's a free PTD No. %d\n",
				sitd_index);
			if (sitd->sitd_index == TD_PTD_INV_PTD_INDEX) {
				sitd->sitd_index = sitd_index;
			}

			
			ptd_map_buff->map_list[sitd_index].datatoggle = 0;
			ptd_map_buff->map_list[sitd_index].state =
				TD_PTD_ACTIVE;
			ptd_map_buff->map_list[sitd_index].qtd = NULL;

			
			ptd_map_buff->map_list[sitd_index].sitd = sitd;
			ptd_map_buff->map_list[sitd_index].itd = NULL;
			ptd_map_buff->map_list[sitd_index].qh = NULL;

			
			ptd_map_buff->map_list[sitd_index].ptd_bitmap =
				bitmap << sitd_index;

			phci_hcd_fill_ptd_addresses(&ptd_map_buff->
				map_list[sitd_index], sitd->sitd_index,
				buff_type);

			ptd_map_buff->map_list[sitd_index].lasttd = 0;
			ptd_map_buff->total_ptds++;


			ptd_map_buff->active_ptd_bitmap |=
				(bitmap << sitd_index);
			ptd_map_buff->pending_ptd_bitmap |= (bitmap << sitd_index);	
			break;
		}		
	}			
	return;
}

void
phcd_iso_get_itd_ptd_index(phci_hcd * hcd, struct ehci_itd *itd)
{
	td_ptd_map_buff_t *ptd_map_buff;
	unsigned long buff_type, max_ptds;
	unsigned char itd_index, bitmap;

	
	bitmap = 0x1;
	buff_type = td_ptd_pipe_x_buff_type[TD_PTD_BUFF_TYPE_ISTL];
	ptd_map_buff = (td_ptd_map_buff_t *) & (td_ptd_map_buff[buff_type]);
	max_ptds = ptd_map_buff->max_ptds;

	itd->itd_index = TD_PTD_INV_PTD_INDEX;

	for (itd_index = 0; itd_index < max_ptds; itd_index++) {
		if (ptd_map_buff->map_list[itd_index].state == TD_PTD_NEW) {
			if (itd->itd_index == TD_PTD_INV_PTD_INDEX) {
				itd->itd_index = itd_index;
			}

			
			ptd_map_buff->map_list[itd_index].datatoggle = 0;
			ptd_map_buff->map_list[itd_index].state = TD_PTD_ACTIVE;
			ptd_map_buff->map_list[itd_index].qtd = NULL;

			
			ptd_map_buff->map_list[itd_index].itd = itd;
			ptd_map_buff->map_list[itd_index].qh = NULL;

			
			ptd_map_buff->map_list[itd_index].ptd_bitmap =
				bitmap << itd_index;

			phci_hcd_fill_ptd_addresses(&ptd_map_buff->
				map_list[itd_index],
				itd->itd_index, buff_type);

			ptd_map_buff->map_list[itd_index].lasttd = 0;
			ptd_map_buff->total_ptds++;

			ptd_map_buff->active_ptd_bitmap |=
				(bitmap << itd_index);
			ptd_map_buff->pending_ptd_bitmap |= (bitmap << itd_index);	
			break;
		}		
	}			
	return;
}				

void
phcd_iso_sitd_free_list(phci_hcd * hcd, struct urb *urb, unsigned long status)
{
	td_ptd_map_buff_t *ptd_map_buff;
	struct ehci_sitd *first_sitd, *next_sitd, *sitd;
	td_ptd_map_t *td_ptd_map;

	
	ptd_map_buff = &(td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL]);
	first_sitd = (struct ehci_sitd *) urb->hcpriv;
	sitd = first_sitd;

	if (sitd->hw_next == EHCI_LIST_END) {
		if (sitd->sitd_index != TD_PTD_INV_PTD_INDEX) {
			td_ptd_map = &ptd_map_buff->map_list[sitd->sitd_index];
			td_ptd_map->state = TD_PTD_NEW;
		}

		if (status != -ENOMEM) {
			phci_hcd_mem_free(&sitd->mem_addr);
		}

		list_del(&sitd->sitd_list);
		qha_free(qha_cache, sitd);

		urb->hcpriv = 0;
		return;
	}
	
	while (1) {
		
		next_sitd = (struct ehci_sitd *) (sitd->hw_next);
		if (next_sitd->hw_next == EHCI_LIST_END) {
			if (next_sitd->sitd_index != TD_PTD_INV_PTD_INDEX) {
				
				td_ptd_map =
					&ptd_map_buff->map_list[next_sitd->
					sitd_index];
				td_ptd_map->state = TD_PTD_NEW;
			}

			if (status != -ENOMEM) {
				iso_dbg(ISO_DBG_ERR,
					"[phcd_iso_itd_free_list Error]: Memory not available\n");
				phci_hcd_mem_free(&next_sitd->mem_addr);
			}

			
			list_del(&next_sitd->sitd_list);
			qha_free(qha_cache, next_sitd);
			break;
		}

		
		sitd->hw_next = next_sitd->hw_next;

		td_ptd_map = &ptd_map_buff->map_list[next_sitd->sitd_index];
		td_ptd_map->state = TD_PTD_NEW;
		phci_hcd_mem_free(&next_sitd->mem_addr);
		list_del(&next_sitd->sitd_list);
		qha_free(qha_cache, next_sitd);
	}			

	
	if (first_sitd->sitd_index != TD_PTD_INV_PTD_INDEX) {
		td_ptd_map = &ptd_map_buff->map_list[first_sitd->sitd_index];
		td_ptd_map->state = TD_PTD_NEW;
	}

	if (status != -ENOMEM) {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_free_list Error]: No memory\n");
		phci_hcd_mem_free(&first_sitd->mem_addr);
	}

	list_del(&first_sitd->sitd_list);
	qha_free(qha_cache, first_sitd);
	urb->hcpriv = 0;
	return;
}

void
phcd_iso_itd_free_list(phci_hcd * hcd, struct urb *urb, unsigned long status)
{
	td_ptd_map_buff_t *ptd_map_buff;
	struct ehci_itd *first_itd, *next_itd, *itd;
	td_ptd_map_t *td_ptd_map;

	
	ptd_map_buff = &(td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL]);
	first_itd = (struct ehci_itd *) urb->hcpriv;
	itd = first_itd;

	if (itd->hw_next == EHCI_LIST_END) {
		if (itd->itd_index != TD_PTD_INV_PTD_INDEX) {
			td_ptd_map = &ptd_map_buff->map_list[itd->itd_index];
			td_ptd_map->state = TD_PTD_NEW;
		}

		if (status != -ENOMEM) {
			phci_hcd_mem_free(&itd->mem_addr);
		}

		list_del(&itd->itd_list);
		qha_free(qha_cache, itd);

		urb->hcpriv = 0;
		return;
	}
	
	while (1) {
		
		next_itd = (struct ehci_itd *) le32_to_cpu(itd->hw_next);
		if (next_itd->hw_next == EHCI_LIST_END) {
			if (next_itd->itd_index != TD_PTD_INV_PTD_INDEX) {
				
				td_ptd_map =
					&ptd_map_buff->map_list[next_itd->
					itd_index];
				td_ptd_map->state = TD_PTD_NEW;
			}

			if (status != -ENOMEM) {
				iso_dbg(ISO_DBG_ERR,
					"[phcd_iso_itd_free_list Error]: Memory not available\n");
				phci_hcd_mem_free(&next_itd->mem_addr);
			}

			
			list_del(&next_itd->itd_list);
			qha_free(qha_cache, next_itd);
			break;
		}

		
		itd->hw_next = next_itd->hw_next;

		td_ptd_map = &ptd_map_buff->map_list[next_itd->itd_index];
		td_ptd_map->state = TD_PTD_NEW;
		phci_hcd_mem_free(&next_itd->mem_addr);
		list_del(&next_itd->itd_list);
		qha_free(qha_cache, next_itd);
	}			

	
	if (first_itd->itd_index != TD_PTD_INV_PTD_INDEX) {
		td_ptd_map = &ptd_map_buff->map_list[first_itd->itd_index];
		td_ptd_map->state = TD_PTD_NEW;
	}

	if (status != -ENOMEM) {
		iso_dbg(ISO_DBG_ERR,
			"[phcd_iso_itd_free_list Error]: No memory\n");
		phci_hcd_mem_free(&first_itd->mem_addr);
	}

	list_del(&first_itd->itd_list);
	qha_free(qha_cache, first_itd);
	urb->hcpriv = 0;
	return;
}				

void
phcd_clean_iso_qh(phci_hcd * hcd, struct ehci_qh *qh)
{
	unsigned int i = 0;
	u16 skipmap=0;
	struct ehci_sitd *sitd;
	struct ehci_itd *itd;

	iso_dbg(ISO_DBG_ERR, "phcd_clean_iso_qh \n");
	if (!qh){
		return;
	}
	skipmap = isp1763_reg_read16(hcd->dev, hcd->regs.isotdskipmap, skipmap);
	skipmap |= qh->periodic_list.ptdlocation;
	isp1763_reg_write16(hcd->dev, hcd->regs.isotdskipmap, skipmap);
#ifdef COMMON_MEMORY
	phci_hcd_mem_free(&qh->memory_addr);
#endif
	for (i = 0; i < 16 && qh->periodic_list.ptdlocation; i++) {
		if (qh->periodic_list.ptdlocation & (0x1 << i)) {
			printk("[phcd_clean_iso_qh] : %x \n",
				qh->periodic_list.high_speed);

			qh->periodic_list.ptdlocation &= ~(0x1 << i);

			if (qh->periodic_list.high_speed == 0) {
				if (td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
					map_list[i].sitd) {

					printk("SITD found \n");
					sitd = td_ptd_map_buff
						[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].sitd;
#ifndef COMMON_MEMORY
					phci_hcd_mem_free(&sitd->mem_addr);
#endif
					sitd->urb = NULL;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].state = TD_PTD_NEW;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].sitd = NULL;
					qha_free(qha_cache, sitd);
				}
			} else {
				if (td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
					map_list[i].itd) {

					printk("ITD found \n");
					itd = td_ptd_map_buff
						[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].itd;
#ifdef COMMON_MEMORY
					phci_hcd_mem_free(&itd->mem_addr);
#endif

					itd->urb = NULL;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].state = TD_PTD_NEW;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].itd = NULL;
					qha_free(qha_cache, itd);
				}
			}

		}
	}


}


void phcd_clean_periodic_ep(void){
	periodic_ep[0] = NULL;
	periodic_ep[1] = NULL;
}

int
phcd_clean_urb_pending(phci_hcd * hcd, struct urb *urb)
{
	unsigned int i = 0;
	struct ehci_qh *qhead;
	struct ehci_sitd *sitd;
	struct ehci_itd *itd;
	u16 skipmap=0;;

	iso_dbg(ISO_DBG_ENTRY, "[phcd_clean_urb_pending] : Enter\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	qhead=urb->hcpriv;
	if (periodic_ep[0] == qhead->ep) {
		periodic_ep[0] = NULL;

	}

	if (periodic_ep[1] == qhead->ep) {
		periodic_ep[1] = NULL;
	}
#else	
	qhead = urb->ep->hcpriv;
	if (periodic_ep[0] == urb->ep) {
		periodic_ep[0] = NULL;

	}

	if (periodic_ep[1] == urb->ep) {
		periodic_ep[1] = NULL;
	}
#endif	
	if (!qhead) {
		return 0;
	}
	skipmap = isp1763_reg_read16(hcd->dev, hcd->regs.isotdskipmap, skipmap);
	skipmap |= qhead->periodic_list.ptdlocation;
	isp1763_reg_write16(hcd->dev, hcd->regs.isotdskipmap, skipmap);
#ifdef COMMON_MEMORY
	phci_hcd_mem_free(&qhead->memory_addr);
#endif

	for (i = 0; i < 16 && qhead->periodic_list.ptdlocation; i++) {

		qhead->periodic_list.ptdlocation &= ~(0x1 << i);

		if (qhead->periodic_list.ptdlocation & (0x1 << i)) {

			printk("[phcd_clean_urb_pending] : %x \n",
				qhead->periodic_list.high_speed);

			if (qhead->periodic_list.high_speed == 0) {

				if (td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
					map_list[i].sitd) {

					sitd = td_ptd_map_buff
						[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].sitd;
#ifndef COMMON_MEMORY
					phci_hcd_mem_free(&sitd->mem_addr);
#endif
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].state = TD_PTD_NEW;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].sitd = NULL;
					qha_free(qha_cache, sitd);
				}
			} else {

				if (td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
					map_list[i].itd) {

					itd = td_ptd_map_buff
						[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].itd;
#ifdef COMMON_MEMORY
					phci_hcd_mem_free(&itd->mem_addr);
#endif
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].state = TD_PTD_NEW;
					td_ptd_map_buff[TD_PTD_BUFF_TYPE_ISTL].
						map_list[i].itd = NULL;
					qha_free(qha_cache, itd);
				}
			}

		}

	}
	INIT_LIST_HEAD(&qhead->periodic_list.sitd_itd_head);
	iso_dbg(ISO_DBG_ENTRY, "[phcd_clean_urb_pending] : Exit\n");
	return 0;
}



int
phcd_store_urb_pending(phci_hcd * hcd, int index, struct urb *urb, int *status)
{
	unsigned int uiNumofPTDs = 0;
	unsigned int uiNumofSlots = 0;
	unsigned int uiMult = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	iso_dbg(ISO_DBG_ENTRY, "[phcd_store_urb_pending] : Enter\n");
	if (urb != NULL) {
		if (periodic_ep[0] != urb->ep && periodic_ep[1] != urb->ep) {
			if (periodic_ep[0] == NULL) {
			
				periodic_ep[0] = urb->ep;
			} else if (periodic_ep[1] == NULL) {
				printk("storing in 1\n");
				periodic_ep[1] = urb->ep;
				usb_hcd_link_urb_to_ep(&(hcd->usb_hcd), urb);
				return -1;
			} else {
				iso_dbg(ISO_DBG_ERR,
					"Support only 2 ISO endpoints simultaneously \n");
				*status = -1;
				return -1;
			}
		}
		usb_hcd_link_urb_to_ep(&(hcd->usb_hcd), urb);
		iso_dbg(ISO_DBG_DATA,
			"[phcd_store_urb_pending] : Add an urb into gstUrb_pending array at index : %d\n",
			giUrbCount);
		giUrbCount++;
	} else {

		iso_dbg(ISO_DBG_ENTRY,
			"[phcd_store_urb_pending] : getting urb from list \n");
		if (index > 0 && index < 2) {
			if (periodic_ep[index - 1]){
				urb = container_of(periodic_ep[index - 1]->
					urb_list.next, struct urb,
					urb_list);
			}
		} else {
			iso_dbg(ISO_DBG_ERR, " Unknown enpoints Error \n");
			*status = -1;
			return -1;
		}

	}


	if ((urb != NULL && (urb->ep->urb_list.next == &urb->urb_list))){
		iso_dbg(ISO_DBG_DATA,
			"[phcd_store_urb_pending] : periodic_sched : %d\n",
			hcd->periodic_sched);
		iso_dbg(ISO_DBG_DATA,
			"[phcd_store_urb_pending] : number_of_packets : %d\n",
			urb->number_of_packets);
		iso_dbg(ISO_DBG_DATA,
			"[phcd_store_urb_pending] : Maximum PacketSize : %d\n",
			usb_maxpacket(urb->dev,urb->pipe, usb_pipeout(urb->pipe)));
		
		if (urb->dev->speed == USB_SPEED_FULL) {	
	
		
			if(1){
				if (phcd_submit_iso(hcd, 
					#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
						struct usb_host_endpoint *ep,
					#endif
						urb,
						( unsigned long *) &status) == 0) {
					pehci_hcd_iso_schedule(hcd, urb);
				} else{
				
				}
			}
		} else if (urb->dev->speed == USB_SPEED_HIGH) {	
			
			uiNumofSlots = NUMMICROFRAME / urb->interval;
			
			uiMult = usb_maxpacket(urb->dev, urb->pipe,
					usb_pipeout(urb->pipe));
			
			uiMult = 1 + ((uiMult >> 11) & 0x3);
			
			uiNumofPTDs =
				(urb->number_of_packets / uiMult) /
				uiNumofSlots;
			if ((urb->number_of_packets / uiMult) % uiNumofSlots != 0){
				uiNumofPTDs += 1;
			}

			iso_dbg(ISO_DBG_DATA,
				"[phcd_store_urb_pending] : interval : %d\n",
				urb->interval);
			iso_dbg(ISO_DBG_DATA,
				"[phcd_store_urb_pending] : uiMult : %d\n",
				uiMult);
			iso_dbg(ISO_DBG_DATA,
				"[phcd_store_urb_pending] : uiNumofPTDs : %d\n",
				uiNumofPTDs);

			if (hcd->periodic_sched <=
				MAX_PERIODIC_SIZE - uiNumofPTDs) {

				if (phcd_submit_iso(hcd,
					#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
						struct usb_host_endpoint *ep,
					#endif
					urb, (unsigned long *) &status)== 0) {

					pehci_hcd_iso_schedule(hcd, urb);
				}
			} else{
				*status = 0;
			}
		}
	} else{
		iso_dbg(ISO_DBG_DATA,
			"[phcd_store_urb_pending] : nextUrb is NULL\n");
	}
#endif
	iso_dbg(ISO_DBG_ENTRY, "[phcd_store_urb_pending] : Exit\n");
	return 0;
}

unsigned long
phcd_submit_iso(phci_hcd * hcd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	struct usb_host_endpoint *ep,
#else
#endif
		struct urb *urb, unsigned long *status)
{
	struct _periodic_list *periodic_list;
	struct hcd_dev *dev;
	struct ehci_qh *qhead;
	struct ehci_itd *itd, *prev_itd;
	struct ehci_sitd *sitd, *prev_sitd;
	struct list_head *sitd_itd_list;
	unsigned long ep_in, max_pkt, mult;
	unsigned long bus_time, high_speed, start_frame;
	unsigned long temp;
	unsigned long packets;
	
	unsigned int iMicroIndex = 0;
	unsigned int iNumofSlots = 0;
	unsigned int iNumofPTDs = 0;
	unsigned int iPTDIndex = 0;
	unsigned int iNumofPks = 0;
	int iPG = 0;
	dma_addr_t buff_dma;
	unsigned long length, offset;
	int i = 0;

	iso_dbg(ISO_DBG_ENTRY, "phcd_submit_iso Entry\n");

	*status = 0;
	
	high_speed = 0;
	periodic_list = &hcd->periodic_list[0];
	dev = (struct hcd_dev *) urb->hcpriv;
	urb->hcpriv = (void *) 0;
	prev_itd = (struct ehci_itd *) 0;
	itd = (struct ehci_itd *) 0;
	prev_sitd = (struct ehci_sitd *) 0;
	sitd = (struct ehci_sitd *) 0;
	start_frame = 0;

	ep_in = usb_pipein(urb->pipe);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	qhead = ep->hcpriv;
#else
	qhead = urb->ep->hcpriv;
#endif
	if (!qhead) {

		qhead = phci_hcd_qh_alloc(hcd);
		if (qhead == 0) {
			iso_dbg(ISO_DBG_ERR,
				"[phcd_submit_iso Error]: Not enough memory\n");
			return -ENOMEM;
		}

		qhead->type = TD_PTD_BUFF_TYPE_ISTL;
		INIT_LIST_HEAD(&qhead->periodic_list.sitd_itd_head);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		qhead->ep=ep;
		ep->hcpriv = qhead;
		urb->hcpriv=qhead;
#else
		urb->ep->hcpriv = qhead;
#endif
	}

		urb->hcpriv=qhead;

	
	max_pkt = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));

	mult = 1 + ((max_pkt >> 11) & 0x3);

	
	max_pkt *= mult;

	
	bus_time = 0;

	if (urb->dev->speed == USB_SPEED_FULL) {

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		if (urb->bandwidth == 0) {
			bus_time = usb_check_bandwidth(urb->dev, urb);
			if (bus_time < 0) {
				usb_dec_dev_use(urb->dev);
				*status = bus_time;
				return *status;
			}
		}
#else
#endif
	} else {			

		high_speed = 1;

		bus_time = 633232L;
		bus_time +=
			(2083L * ((3167L + BitTime(max_pkt) * 1000L) / 1000L));
		bus_time = bus_time / 1000L;
		bus_time += BW_HOST_DELAY;
		bus_time = NS_TO_US(bus_time);
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	usb_claim_bandwidth(urb->dev, urb, bus_time, 1);
#else
#endif

	qhead->periodic_list.ptdlocation = 0;
	
	if (phcd_iso_scheduling_info(hcd, qhead, max_pkt, high_speed, ep_in) <
		0) {

		iso_dbg(ISO_DBG_ERR,
			"[phcd_submit_iso Error]: No space available\n");
		return -ENOSPC;
	}

	if (urb->dev->speed == USB_SPEED_HIGH) {
		iNumofSlots = NUMMICROFRAME / urb->interval;
		
		iNumofPTDs = (urb->number_of_packets / mult) / iNumofSlots;
		if ((urb->number_of_packets / mult) % iNumofSlots != 0){	
			
			iNumofPTDs += 1;
		}
	}
	if (urb->iso_frame_desc[0].offset != 0) {
		*status = -EINVAL;
		iso_dbg(ISO_DBG_ERR,
			"[phcd_submit_iso Error]: Invalid value\n");
		return *status;
	}
	if (1) {
		
		if (0){
			if (urb->transfer_flags & URB_ISO_ASAP){
				start_frame =
					isp1763_reg_read16(hcd->dev,
						hcd->regs.frameindex,
						start_frame);
			} else {
				start_frame = urb->start_frame;
			}
		}

		start_frame =
			isp1763_reg_read16(hcd->dev, hcd->regs.frameindex,
				start_frame);

		

		start_frame >>= 3;
		if (urb->dev->speed != USB_SPEED_HIGH){
			start_frame += 1;
		}else{
			start_frame += 2;
		}
		start_frame = start_frame & PTD_FRAME_MASK;
		temp = start_frame;
		if (urb->dev->speed != USB_SPEED_HIGH) {
			qhead->next_uframe =
				start_frame + urb->number_of_packets;
		} else {
			qhead->next_uframe = start_frame + iNumofPTDs;
		}
		qhead->next_uframe %= PTD_FRAME_MASK;
		iso_dbg(ISO_DBG_DATA, "[phcd_submit_iso]: startframe = %ld\n",
			start_frame);
	} else {
		start_frame = (qhead->next_uframe) % PTD_FRAME_MASK;
		if (urb->dev->speed != USB_SPEED_HIGH){
			qhead->next_uframe =
				start_frame + urb->number_of_packets;
				iNumofPTDs=urb->number_of_packets;
		} else {
			qhead->next_uframe = start_frame + iNumofPTDs;
		}

		qhead->next_uframe %= PTD_FRAME_MASK;
	}


	iso_dbg(ISO_DBG_DATA, "[phcd_submit_iso]: Start frame index: %ld\n",
		start_frame);
	iso_dbg(ISO_DBG_DATA, "[phcd_submit_iso]: Max packet: %d\n",
		(int) max_pkt);

#ifdef COMMON_MEMORY
	if(urb->number_of_packets>8 && urb->dev->speed!=USB_SPEED_HIGH)
		phci_hcd_mem_alloc(8*max_pkt, &qhead->memory_addr, 0);
	else
	phci_hcd_mem_alloc(urb->transfer_buffer_length, &qhead->memory_addr, 0);
	if (urb->transfer_buffer_length && ((qhead->memory_addr.phy_addr == 0)
		|| (qhead->memory_addr.virt_addr ==0))) {
		iso_dbg(ISO_DBG_ERR,
			"[URB FILL MEMORY Error]: No payload memory available\n");
		return -ENOMEM;
	}
#endif

	if (urb->dev->speed != USB_SPEED_HIGH) {
		iNumofPks = urb->number_of_packets;
		qhead->totalptds=urb->number_of_packets;
		qhead->actualptds=0;

		
		for (packets = 0; packets < urb->number_of_packets; packets++) {
			sitd = kmalloc(sizeof(*sitd), GFP_ATOMIC);
			if (!sitd) {
				*status = -ENOMEM;
				if (((int)(qhead->next_uframe -
					urb->number_of_packets)) < 0){
					
					qhead->next_uframe = qhead->next_uframe + PTD_PERIODIC_SIZE;	
					
				}
				qhead->next_uframe -= urb->number_of_packets;

				
				if (urb->hcpriv) {
					phcd_iso_sitd_free_list(hcd, urb, 
						*status);
				}
				iso_dbg(ISO_DBG_ERR,
					"[phcd_submit_iso Error]: No memory available\n");
				return *status;
			}

			memset(sitd, 0, sizeof(struct ehci_sitd));

			INIT_LIST_HEAD(&sitd->sitd_list);

			sitd->sitd_dma = (u32) (sitd);
			sitd->urb = urb;


			sitd->hw_next = EHCI_LIST_END;
			sitd->sitd_index = TD_PTD_INV_PTD_INDEX;

			
			sitd->framenumber = start_frame + packets;
			sitd->start_frame = temp + packets;

			
			sitd->index = packets;

			sitd->framenumber = sitd->framenumber & PTD_FRAME_MASK;
			sitd->ssplit = qhead->ssplit;
			sitd->csplit = qhead->csplit;

			*status = phcd_iso_sitd_fill(hcd, sitd, urb, packets);

			if (*status != 0) {
				if (((int)(qhead->next_uframe - 
					urb->number_of_packets)) < 0){
					
					qhead->next_uframe = qhead->next_uframe + 
						PTD_PERIODIC_SIZE;	
				}
				qhead->next_uframe -= urb->number_of_packets;

				
				if (urb->hcpriv) {
					phcd_iso_sitd_free_list(hcd, urb,
						*status);
				}
				iso_dbg(ISO_DBG_ERR,
					"[phcd_submit_iso Error]: Error in filling up SITD\n");
				return *status;
			}

			if (prev_sitd) {
				prev_sitd->hw_next = (u32) (sitd);
			}

			prev_sitd = sitd;

			if(packets<8){  
			phcd_iso_get_sitd_ptd_index(hcd, sitd);
			iso_dbg(ISO_DBG_DATA,
				"[phcd_submit_iso]: SITD index %d\n",
				sitd->sitd_index);

			
			if (sitd->sitd_index == TD_PTD_INV_PTD_INDEX) {
				*status = -ENOSPC;
				if (((int) (qhead->next_uframe -
					urb->number_of_packets)) < 0){
					
					qhead->next_uframe = qhead->next_uframe + PTD_PERIODIC_SIZE;	
				}
				qhead->next_uframe -= urb->number_of_packets;

				
				if (urb->hcpriv) {
					phcd_iso_sitd_free_list(hcd, urb,
						*status);
				}
				return *status;
			}
					qhead->actualptds++;
			}
			

			sitd_itd_list = &qhead->periodic_list.sitd_itd_head;
			list_add_tail(&sitd->sitd_list, sitd_itd_list);
			qhead->periodic_list.high_speed = 0;
			if(sitd->sitd_index!=TD_PTD_INV_PTD_INDEX)
			qhead->periodic_list.ptdlocation |=
				0x1 << sitd->sitd_index;
			
			hcd->periodic_sched++;

			
			if (urb->hcpriv == 0){
				urb->hcpriv = sitd;
			}
		}	
	} else if (urb->dev->speed == USB_SPEED_HIGH) {	
		iNumofPks = iNumofPTDs;

		packets = 0;
		iPTDIndex = 0;
		while (packets < urb->number_of_packets) {
			iNumofSlots = NUMMICROFRAME / urb->interval;
			itd = kmalloc(sizeof(*itd), GFP_ATOMIC);
			if (!itd) {
				*status = -ENOMEM;
				if(((int) (qhead->next_uframe - iNumofPTDs))<0){
					
					qhead->next_uframe = qhead->next_uframe + 
						PTD_PERIODIC_SIZE;	
				}
				qhead->next_uframe -= iNumofPTDs;

				
				if (urb->hcpriv) {
					phcd_iso_itd_free_list(hcd, urb,
							       *status);
				}
				iso_dbg(ISO_DBG_ERR,
					"[phcd_submit_iso Error]: No memory available\n");
				return *status;
			}
			memset(itd, 0, sizeof(struct ehci_itd));

			INIT_LIST_HEAD(&itd->itd_list);

			itd->itd_dma = (u32) (itd);
			itd->urb = urb;

			itd->hw_next = EHCI_LIST_END;
			itd->itd_index = TD_PTD_INV_PTD_INDEX;
			
			itd->framenumber = start_frame + iPTDIndex;
			
			itd->index = packets;

			itd->framenumber = itd->framenumber & 0x1F;

			itd->ssplit = qhead->ssplit;
			itd->csplit = qhead->csplit;

			
			itd->num_of_pkts = iNumofSlots * mult;
			
			if (itd->num_of_pkts >= urb->number_of_packets)
			{
				itd->num_of_pkts = urb->number_of_packets;
			}
			else {
				if (itd->num_of_pkts >
					urb->number_of_packets - packets){
					itd->num_of_pkts =
						urb->number_of_packets -
						packets;
				}
			}

			iso_dbg(ISO_DBG_DATA,
				"[phcd_submit_iso] packets index = %ld itd->num_of_pkts = %d\n",
				packets, itd->num_of_pkts);
			*status =
				phcd_iso_itd_fill(hcd, itd, urb, packets,
						itd->num_of_pkts);
			if (*status != 0) {
				if (((int) (qhead->next_uframe - iNumofPTDs)) <
					0) {
					qhead->next_uframe = qhead->next_uframe + PTD_PERIODIC_SIZE;	
				}
				qhead->next_uframe -= iNumofPTDs;

				
				if (urb->hcpriv) {
					phcd_iso_itd_free_list(hcd, urb,
						*status);
				}
				iso_dbg(ISO_DBG_ERR,
					"[phcd_submit_iso Error]: Error in filling up ITD\n");
				return *status;
			}

			iPG = 0;
			iMicroIndex = 0;
			while (iNumofSlots > 0) {
				offset = urb->iso_frame_desc[packets].offset;
				
				buff_dma =
					(u32) ((unsigned char *) urb->
						transfer_buffer + offset);

				
				length = 0;
				for (i = packets; i < packets + mult; i++) {
					length += urb->iso_frame_desc[i].length;
				}
				itd->hw_transaction[iMicroIndex] =
					EHCI_ISOC_ACTIVE | (length & 
					EHCI_ITD_TRANLENGTH)
					<< 16 | iPG << 12 | buff_dma;
					
				if (itd->hw_bufp[iPG] != buff_dma){
					itd->hw_bufp[++iPG] = buff_dma;
				}

				iso_dbg(ISO_DBG_DATA,
					"[%s] offset : %ld buff_dma : 0x%08x length : %ld\n",
					__FUNCTION__, offset,
					(unsigned int) buff_dma, length);

				itd->ssplit |= 1 << iMicroIndex;
				packets++;
				iMicroIndex += urb->interval;
				iNumofSlots--;

				
				if (packets == urb->number_of_packets
					|| iNumofSlots == 0) {

					itd->hw_transaction[iMicroIndex] |=
						EHCI_ITD_IOC;

					break;
					
				}
			}

			if (prev_itd) {
				prev_itd->hw_next = (u32) (itd);
			}

			prev_itd = itd;



			iso_dbg(ISO_DBG_DATA,
				"[phcd_submit_iso]: ITD index %d\n",
				itd->framenumber);
			phcd_iso_get_itd_ptd_index(hcd, itd);
			iso_dbg(ISO_DBG_DATA,
				"[phcd_submit_iso]: ITD index %d\n",
				itd->itd_index);

			
			if (itd->itd_index == TD_PTD_INV_PTD_INDEX) {
				*status = -ENOSPC;
				if (((int) (qhead->next_uframe - iNumofPTDs)) <
					0){
					
					qhead->next_uframe = qhead->next_uframe + PTD_PERIODIC_SIZE;	
				}
				qhead->next_uframe -= iNumofPTDs;

				
				if (urb->hcpriv) {
					phcd_iso_itd_free_list(hcd, urb,
							       *status);
				}
				return *status;
			}

			sitd_itd_list = &qhead->periodic_list.sitd_itd_head;
			list_add_tail(&itd->itd_list, sitd_itd_list);
			qhead->periodic_list.high_speed = 1;
			qhead->periodic_list.ptdlocation |=
				0x1 << itd->itd_index;

			
			hcd->periodic_sched++;

			
			if (urb->hcpriv == 0){
				urb->hcpriv = itd;
			}
			iPTDIndex++;

		}		
	}

	
	
	if (high_speed == 0){
		sitd->hw_next = EHCI_LIST_END;
	}
	urb->error_count = 0;
	return *status;
}				
#endif 
