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

#ifndef _DCB_82599_CONFIG_H_
#define _DCB_82599_CONFIG_H_

#define IXGBE_RTTDCS_TDPAC      0x00000001 
#define IXGBE_RTTDCS_VMPAC      0x00000002 
#define IXGBE_RTTDCS_TDRM       0x00000010 
#define IXGBE_RTTDCS_ARBDIS     0x00000040 
#define IXGBE_RTTDCS_BDPM       0x00400000 
#define IXGBE_RTTDCS_BPBFSM     0x00800000 
#define IXGBE_RTTDCS_SPEED_CHG  0x80000000 

#define IXGBE_RTRUP2TC_UP_SHIFT 3
#define IXGBE_RTTUP2TC_UP_SHIFT 3

#define IXGBE_RTRPT4C_MCL_SHIFT 12 
#define IXGBE_RTRPT4C_BWG_SHIFT 9  
#define IXGBE_RTRPT4C_GSP       0x40000000 
#define IXGBE_RTRPT4C_LSP       0x80000000 

#define IXGBE_RDRXCTL_MPBEN     0x00000010 
#define IXGBE_RDRXCTL_MCEN      0x00000040 

#define IXGBE_RTRPCS_RRM        0x00000002 
#define IXGBE_RTRPCS_RAC        0x00000004
#define IXGBE_RTRPCS_ARBDIS     0x00000040 

#define IXGBE_RTTDT2C_MCL_SHIFT 12
#define IXGBE_RTTDT2C_BWG_SHIFT 9
#define IXGBE_RTTDT2C_GSP       0x40000000
#define IXGBE_RTTDT2C_LSP       0x80000000

#define IXGBE_RTTPT2C_MCL_SHIFT 12
#define IXGBE_RTTPT2C_BWG_SHIFT 9
#define IXGBE_RTTPT2C_GSP       0x40000000
#define IXGBE_RTTPT2C_LSP       0x80000000

#define IXGBE_RTTPCS_TPPAC      0x00000020 
#define IXGBE_RTTPCS_ARBDIS     0x00000040 
#define IXGBE_RTTPCS_TPRM       0x00000100 
#define IXGBE_RTTPCS_ARBD_SHIFT 22
#define IXGBE_RTTPCS_ARBD_DCB   0x4        

#define IXGBE_SECTX_DCB		0x00001F00 



s32 ixgbe_dcb_config_pfc_82599(struct ixgbe_hw *hw, u8 pfc_en, u8 *prio_tc);

s32 ixgbe_dcb_config_rx_arbiter_82599(struct ixgbe_hw *hw,
					u16 *refill,
					u16 *max,
					u8 *bwg_id,
					u8 *prio_type,
					u8 *prio_tc);

s32 ixgbe_dcb_config_tx_desc_arbiter_82599(struct ixgbe_hw *hw,
						u16 *refill,
						u16 *max,
						u8 *bwg_id,
						u8 *prio_type);

s32 ixgbe_dcb_config_tx_data_arbiter_82599(struct ixgbe_hw *hw,
						u16 *refill,
						u16 *max,
						u8 *bwg_id,
						u8 *prio_type,
						u8 *prio_tc);

s32 ixgbe_dcb_hw_config_82599(struct ixgbe_hw *hw, u8 pfc_en, u16 *refill,
			      u16 *max, u8 *bwg_id, u8 *prio_type,
			      u8 *prio_tc);

#endif 
