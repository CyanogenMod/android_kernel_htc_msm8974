/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

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

#ifndef _IXGBE_MBX_H_
#define _IXGBE_MBX_H_

#include "ixgbe_type.h"

#define IXGBE_VFMAILBOX_SIZE        16 
#define IXGBE_ERR_MBX               -100

#define IXGBE_VFMAILBOX             0x002FC
#define IXGBE_VFMBMEM               0x00200

#define IXGBE_PFMAILBOX_STS   0x00000001 
#define IXGBE_PFMAILBOX_ACK   0x00000002 
#define IXGBE_PFMAILBOX_VFU   0x00000004 
#define IXGBE_PFMAILBOX_PFU   0x00000008 
#define IXGBE_PFMAILBOX_RVFU  0x00000010 

#define IXGBE_MBVFICR_VFREQ_MASK 0x0000FFFF 
#define IXGBE_MBVFICR_VFREQ_VF1  0x00000001 
#define IXGBE_MBVFICR_VFACK_MASK 0xFFFF0000 
#define IXGBE_MBVFICR_VFACK_VF1  0x00010000 


#define IXGBE_VT_MSGTYPE_ACK      0x80000000  
#define IXGBE_VT_MSGTYPE_NACK     0x40000000  
#define IXGBE_VT_MSGTYPE_CTS      0x20000000  
#define IXGBE_VT_MSGINFO_SHIFT    16
#define IXGBE_VT_MSGINFO_MASK     (0xFF << IXGBE_VT_MSGINFO_SHIFT)

#define IXGBE_VF_RESET            0x01 
#define IXGBE_VF_SET_MAC_ADDR     0x02 
#define IXGBE_VF_SET_MULTICAST    0x03 
#define IXGBE_VF_SET_VLAN         0x04 
#define IXGBE_VF_SET_LPE          0x05 
#define IXGBE_VF_SET_MACVLAN      0x06 

#define IXGBE_VF_PERMADDR_MSG_LEN 4
#define IXGBE_VF_MC_TYPE_WORD     3

#define IXGBE_PF_CONTROL_MSG      0x0100 

#define IXGBE_VF_MBX_INIT_TIMEOUT 2000 
#define IXGBE_VF_MBX_INIT_DELAY   500  

s32 ixgbe_read_mbx(struct ixgbe_hw *, u32 *, u16, u16);
s32 ixgbe_write_mbx(struct ixgbe_hw *, u32 *, u16, u16);
s32 ixgbe_check_for_msg(struct ixgbe_hw *, u16);
s32 ixgbe_check_for_ack(struct ixgbe_hw *, u16);
s32 ixgbe_check_for_rst(struct ixgbe_hw *, u16);
#ifdef CONFIG_PCI_IOV
void ixgbe_init_mbx_params_pf(struct ixgbe_hw *);
#endif 

extern struct ixgbe_mbx_operations mbx_ops_generic;

#endif 
