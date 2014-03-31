/*******************************************************************************

  Intel(R) 82576 Virtual Function Linux driver
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

#ifndef _E1000_MBX_H_
#define _E1000_MBX_H_

#include "vf.h"

#define E1000_V2PMAILBOX_REQ   0x00000001 
#define E1000_V2PMAILBOX_ACK   0x00000002 
#define E1000_V2PMAILBOX_VFU   0x00000004 
#define E1000_V2PMAILBOX_PFU   0x00000008 
#define E1000_V2PMAILBOX_PFSTS 0x00000010 
#define E1000_V2PMAILBOX_PFACK 0x00000020 
#define E1000_V2PMAILBOX_RSTI  0x00000040 
#define E1000_V2PMAILBOX_RSTD  0x00000080 
#define E1000_V2PMAILBOX_R2C_BITS 0x000000B0 

#define E1000_VFMAILBOX_SIZE   16 

#define E1000_VT_MSGTYPE_ACK      0x80000000  
#define E1000_VT_MSGTYPE_NACK     0x40000000  
#define E1000_VT_MSGTYPE_CTS      0x20000000  

#define E1000_VF_MBX_INIT_TIMEOUT 2000 
#define E1000_VF_MBX_INIT_DELAY   500  

#define E1000_VT_MSGINFO_SHIFT    16
#define E1000_VT_MSGINFO_MASK     (0xFF << E1000_VT_MSGINFO_SHIFT)

#define E1000_VF_RESET            0x01 
#define E1000_VF_SET_MAC_ADDR     0x02 
#define E1000_VF_SET_MULTICAST    0x03 
#define E1000_VF_SET_VLAN         0x04 
#define E1000_VF_SET_LPE          0x05 

#define E1000_PF_CONTROL_MSG      0x0100 

void e1000_init_mbx_ops_generic(struct e1000_hw *hw);
s32 e1000_init_mbx_params_vf(struct e1000_hw *);

#endif 
