/*
 * Copyright 2008-2010 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef _VNIC_ENIC_H_
#define _VNIC_ENIC_H_

struct vnic_enet_config {
	u32 flags;
	u32 wq_desc_count;
	u32 rq_desc_count;
	u16 mtu;
	u16 intr_timer_deprecated;
	u8 intr_timer_type;
	u8 intr_mode;
	char devname[16];
	u32 intr_timer_usec;
	u16 loop_tag;
};

#define VENETF_TSO		0x1	
#define VENETF_LRO		0x2	
#define VENETF_RXCSUM		0x4	
#define VENETF_TXCSUM		0x8	
#define VENETF_RSS		0x10	
#define VENETF_RSSHASH_IPV4	0x20	
#define VENETF_RSSHASH_TCPIPV4	0x40	
#define VENETF_RSSHASH_IPV6	0x80	
#define VENETF_RSSHASH_TCPIPV6	0x100	
#define VENETF_RSSHASH_IPV6_EX	0x200	
#define VENETF_RSSHASH_TCPIPV6_EX 0x400	
#define VENETF_LOOP		0x800	

#define VENET_INTR_TYPE_MIN	0	
#define VENET_INTR_TYPE_IDLE	1	

#define VENET_INTR_MODE_ANY	0	
#define VENET_INTR_MODE_MSI	1	
#define VENET_INTR_MODE_INTX	2	

#endif 
