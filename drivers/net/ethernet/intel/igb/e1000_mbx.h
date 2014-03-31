/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2012 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _E1000_MBX_H_
#define _E1000_MBX_H_

#include "e1000_hw.h"

#define E1000_P2VMAILBOX_STS   0x00000001 
#define E1000_P2VMAILBOX_ACK   0x00000002 
#define E1000_P2VMAILBOX_VFU   0x00000004 
#define E1000_P2VMAILBOX_PFU   0x00000008 
#define E1000_P2VMAILBOX_RVFU  0x00000010 

#define E1000_MBVFICR_VFREQ_MASK 0x000000FF 
#define E1000_MBVFICR_VFREQ_VF1  0x00000001 
#define E1000_MBVFICR_VFACK_MASK 0x00FF0000 
#define E1000_MBVFICR_VFACK_VF1  0x00010000 

#define E1000_VFMAILBOX_SIZE   16 

#define E1000_VT_MSGTYPE_ACK      0x80000000  
#define E1000_VT_MSGTYPE_NACK     0x40000000  
#define E1000_VT_MSGTYPE_CTS      0x20000000  
#define E1000_VT_MSGINFO_SHIFT    16
#define E1000_VT_MSGINFO_MASK     (0xFF << E1000_VT_MSGINFO_SHIFT)

#define E1000_VF_RESET            0x01 
#define E1000_VF_SET_MAC_ADDR     0x02 
#define E1000_VF_SET_MULTICAST    0x03 
#define E1000_VF_SET_VLAN         0x04 
#define E1000_VF_SET_LPE          0x05 
#define E1000_VF_SET_PROMISC      0x06 
#define E1000_VF_SET_PROMISC_MULTICAST    (0x02 << E1000_VT_MSGINFO_SHIFT)

#define E1000_PF_CONTROL_MSG      0x0100 

s32 igb_read_mbx(struct e1000_hw *, u32 *, u16, u16);
s32 igb_write_mbx(struct e1000_hw *, u32 *, u16, u16);
s32 igb_check_for_msg(struct e1000_hw *, u16);
s32 igb_check_for_ack(struct e1000_hw *, u16);
s32 igb_check_for_rst(struct e1000_hw *, u16);
s32 igb_init_mbx_params_pf(struct e1000_hw *);

#endif 
