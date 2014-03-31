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

#include "ixgbe.h"
#include "ixgbe_type.h"
#include "ixgbe_dcb.h"
#include "ixgbe_dcb_82598.h"

s32 ixgbe_dcb_config_rx_arbiter_82598(struct ixgbe_hw *hw,
					u16 *refill,
					u16 *max,
					u8 *prio_type)
{
	u32    reg           = 0;
	u32    credit_refill = 0;
	u32    credit_max    = 0;
	u8     i             = 0;

	reg = IXGBE_READ_REG(hw, IXGBE_RUPPBMR) | IXGBE_RUPPBMR_MQA;
	IXGBE_WRITE_REG(hw, IXGBE_RUPPBMR, reg);

	reg = IXGBE_READ_REG(hw, IXGBE_RMCS);
	
	reg &= ~IXGBE_RMCS_ARBDIS;
	
	reg |= IXGBE_RMCS_RRM;
	
	reg |= IXGBE_RMCS_DFP;

	IXGBE_WRITE_REG(hw, IXGBE_RMCS, reg);

	
	for (i = 0; i < MAX_TRAFFIC_CLASS; i++) {
		credit_refill = refill[i];
		credit_max    = max[i];

		reg = credit_refill | (credit_max << IXGBE_RT2CR_MCL_SHIFT);

		if (prio_type[i] == prio_link)
			reg |= IXGBE_RT2CR_LSP;

		IXGBE_WRITE_REG(hw, IXGBE_RT2CR(i), reg);
	}

	reg = IXGBE_READ_REG(hw, IXGBE_RDRXCTL);
	reg |= IXGBE_RDRXCTL_RDMTS_1_2;
	reg |= IXGBE_RDRXCTL_MPBEN;
	reg |= IXGBE_RDRXCTL_MCEN;
	IXGBE_WRITE_REG(hw, IXGBE_RDRXCTL, reg);

	reg = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	
	reg &= ~IXGBE_RXCTRL_DMBYPS;
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, reg);

	return 0;
}

s32 ixgbe_dcb_config_tx_desc_arbiter_82598(struct ixgbe_hw *hw,
						u16 *refill,
						u16 *max,
						u8 *bwg_id,
						u8 *prio_type)
{
	u32    reg, max_credits;
	u8     i;

	reg = IXGBE_READ_REG(hw, IXGBE_DPMCS);

	
	reg &= ~IXGBE_DPMCS_ARBDIS;
	
	reg |= (IXGBE_DPMCS_TDPAC | IXGBE_DPMCS_TRM);
	reg |= IXGBE_DPMCS_TSOEF;
	
	reg |= (0x4 << IXGBE_DPMCS_MTSOS_SHIFT);

	IXGBE_WRITE_REG(hw, IXGBE_DPMCS, reg);

	
	for (i = 0; i < MAX_TRAFFIC_CLASS; i++) {
		max_credits = max[i];
		reg = max_credits << IXGBE_TDTQ2TCCR_MCL_SHIFT;
		reg |= refill[i];
		reg |= (u32)(bwg_id[i]) << IXGBE_TDTQ2TCCR_BWG_SHIFT;

		if (prio_type[i] == prio_group)
			reg |= IXGBE_TDTQ2TCCR_GSP;

		if (prio_type[i] == prio_link)
			reg |= IXGBE_TDTQ2TCCR_LSP;

		IXGBE_WRITE_REG(hw, IXGBE_TDTQ2TCCR(i), reg);
	}

	return 0;
}

s32 ixgbe_dcb_config_tx_data_arbiter_82598(struct ixgbe_hw *hw,
						u16 *refill,
						u16 *max,
						u8 *bwg_id,
						u8 *prio_type)
{
	u32 reg;
	u8 i;

	reg = IXGBE_READ_REG(hw, IXGBE_PDPMCS);
	
	reg &= ~IXGBE_PDPMCS_ARBDIS;
	
	reg |= (IXGBE_PDPMCS_TPPAC | IXGBE_PDPMCS_TRM);

	IXGBE_WRITE_REG(hw, IXGBE_PDPMCS, reg);

	
	for (i = 0; i < MAX_TRAFFIC_CLASS; i++) {
		reg = refill[i];
		reg |= (u32)(max[i]) << IXGBE_TDPT2TCCR_MCL_SHIFT;
		reg |= (u32)(bwg_id[i]) << IXGBE_TDPT2TCCR_BWG_SHIFT;

		if (prio_type[i] == prio_group)
			reg |= IXGBE_TDPT2TCCR_GSP;

		if (prio_type[i] == prio_link)
			reg |= IXGBE_TDPT2TCCR_LSP;

		IXGBE_WRITE_REG(hw, IXGBE_TDPT2TCCR(i), reg);
	}

	
	reg = IXGBE_READ_REG(hw, IXGBE_DTXCTL);
	reg |= IXGBE_DTXCTL_ENDBUBD;
	IXGBE_WRITE_REG(hw, IXGBE_DTXCTL, reg);

	return 0;
}

s32 ixgbe_dcb_config_pfc_82598(struct ixgbe_hw *hw, u8 pfc_en)
{
	u32 reg;
	u8  i;

	if (pfc_en) {
		
		reg = IXGBE_READ_REG(hw, IXGBE_RMCS);
		reg &= ~IXGBE_RMCS_TFCE_802_3X;
		
		reg |= IXGBE_RMCS_TFCE_PRIORITY;
		IXGBE_WRITE_REG(hw, IXGBE_RMCS, reg);

		
		reg = IXGBE_READ_REG(hw, IXGBE_FCTRL);
		reg &= ~IXGBE_FCTRL_RFCE;
		reg |= IXGBE_FCTRL_RPFCE;
		IXGBE_WRITE_REG(hw, IXGBE_FCTRL, reg);

		
		for (i = 0; i < (MAX_TRAFFIC_CLASS >> 1); i++)
			IXGBE_WRITE_REG(hw, IXGBE_FCTTV(i), 0x68006800);

		
		IXGBE_WRITE_REG(hw, IXGBE_FCRTV, 0x3400);
	}

	for (i = 0; i < MAX_TRAFFIC_CLASS; i++) {
		int enabled = pfc_en & (1 << i);

		reg = hw->fc.low_water << 10;

		if (enabled == pfc_enabled_tx ||
		    enabled == pfc_enabled_full)
			reg |= IXGBE_FCRTL_XONE;

		IXGBE_WRITE_REG(hw, IXGBE_FCRTL(i), reg);

		reg = hw->fc.high_water[i] << 10;
		if (enabled == pfc_enabled_tx ||
		    enabled == pfc_enabled_full)
			reg |= IXGBE_FCRTH_FCEN;

		IXGBE_WRITE_REG(hw, IXGBE_FCRTH(i), reg);
	}

	return 0;
}

static s32 ixgbe_dcb_config_tc_stats_82598(struct ixgbe_hw *hw)
{
	u32 reg = 0;
	u8  i   = 0;
	u8  j   = 0;

	
	for (i = 0, j = 0; i < 15 && j < 8; i = i + 2, j++) {
		reg = IXGBE_READ_REG(hw, IXGBE_RQSMR(i));
		reg |= ((0x1010101) * j);
		IXGBE_WRITE_REG(hw, IXGBE_RQSMR(i), reg);
		reg = IXGBE_READ_REG(hw, IXGBE_RQSMR(i + 1));
		reg |= ((0x1010101) * j);
		IXGBE_WRITE_REG(hw, IXGBE_RQSMR(i + 1), reg);
	}
	
	for (i = 0; i < 8; i++) {
		reg = IXGBE_READ_REG(hw, IXGBE_TQSMR(i));
		reg |= ((0x1010101) * i);
		IXGBE_WRITE_REG(hw, IXGBE_TQSMR(i), reg);
	}

	return 0;
}

s32 ixgbe_dcb_hw_config_82598(struct ixgbe_hw *hw, u8 pfc_en, u16 *refill,
			      u16 *max, u8 *bwg_id, u8 *prio_type)
{
	ixgbe_dcb_config_rx_arbiter_82598(hw, refill, max, prio_type);
	ixgbe_dcb_config_tx_desc_arbiter_82598(hw, refill, max,
					       bwg_id, prio_type);
	ixgbe_dcb_config_tx_data_arbiter_82598(hw, refill, max,
					       bwg_id, prio_type);
	ixgbe_dcb_config_pfc_82598(hw, pfc_en);
	ixgbe_dcb_config_tc_stats_82598(hw);

	return 0;
}
