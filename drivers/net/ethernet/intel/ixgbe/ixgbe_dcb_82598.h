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
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _DCB_82598_CONFIG_H_
#define _DCB_82598_CONFIG_H_


#define IXGBE_DPMCS_MTSOS_SHIFT 16
#define IXGBE_DPMCS_TDPAC       0x00000001 
#define IXGBE_DPMCS_TRM         0x00000010 
#define IXGBE_DPMCS_ARBDIS      0x00000040 
#define IXGBE_DPMCS_TSOEF       0x00080000 

#define IXGBE_RUPPBMR_MQA       0x80000000 

#define IXGBE_RT2CR_MCL_SHIFT   12 
#define IXGBE_RT2CR_LSP         0x80000000 

#define IXGBE_RDRXCTL_MPBEN     0x00000010 
#define IXGBE_RDRXCTL_MCEN      0x00000040 

#define IXGBE_TDTQ2TCCR_MCL_SHIFT   12
#define IXGBE_TDTQ2TCCR_BWG_SHIFT   9
#define IXGBE_TDTQ2TCCR_GSP     0x40000000
#define IXGBE_TDTQ2TCCR_LSP     0x80000000

#define IXGBE_TDPT2TCCR_MCL_SHIFT   12
#define IXGBE_TDPT2TCCR_BWG_SHIFT   9
#define IXGBE_TDPT2TCCR_GSP     0x40000000
#define IXGBE_TDPT2TCCR_LSP     0x80000000

#define IXGBE_PDPMCS_TPPAC      0x00000020 
#define IXGBE_PDPMCS_ARBDIS     0x00000040 
#define IXGBE_PDPMCS_TRM        0x00000100 

#define IXGBE_DTXCTL_ENDBUBD    0x00000004 

#define IXGBE_TXPBSIZE_40KB     0x0000A000 
#define IXGBE_RXPBSIZE_48KB     0x0000C000 
#define IXGBE_RXPBSIZE_64KB     0x00010000 
#define IXGBE_RXPBSIZE_80KB     0x00014000 

#define IXGBE_RDRXCTL_RDMTS_1_2 0x00000000


s32 ixgbe_dcb_config_pfc_82598(struct ixgbe_hw *, u8 pfc_en);

s32 ixgbe_dcb_config_rx_arbiter_82598(struct ixgbe_hw *hw,
					u16 *refill,
					u16 *max,
					u8 *prio_type);

s32 ixgbe_dcb_config_tx_desc_arbiter_82598(struct ixgbe_hw *hw,
						u16 *refill,
						u16 *max,
						u8 *bwg_id,
						u8 *prio_type);

s32 ixgbe_dcb_config_tx_data_arbiter_82598(struct ixgbe_hw *hw,
						u16 *refill,
						u16 *max,
						u8 *bwg_id,
						u8 *prio_type);

s32 ixgbe_dcb_hw_config_82598(struct ixgbe_hw *hw, u8 pfc_en, u16 *refill,
			      u16 *max, u8 *bwg_id, u8 *prio_type);

#endif 
