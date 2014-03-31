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

#include "ixgbe.h"
#include "ixgbe_type.h"
#include "ixgbe_dcb.h"
#include "ixgbe_dcb_82599.h"

s32 ixgbe_dcb_config_rx_arbiter_82599(struct ixgbe_hw *hw,
				      u16 *refill,
				      u16 *max,
				      u8 *bwg_id,
				      u8 *prio_type,
				      u8 *prio_tc)
{
	u32    reg           = 0;
	u32    credit_refill = 0;
	u32    credit_max    = 0;
	u8     i             = 0;

	reg = IXGBE_RTRPCS_RRM | IXGBE_RTRPCS_RAC | IXGBE_RTRPCS_ARBDIS;
	IXGBE_WRITE_REG(hw, IXGBE_RTRPCS, reg);

	
	reg = 0;
	for (i = 0; i < MAX_USER_PRIORITY; i++)
		reg |= (prio_tc[i] << (i * IXGBE_RTRUP2TC_UP_SHIFT));
	IXGBE_WRITE_REG(hw, IXGBE_RTRUP2TC, reg);

	
	for (i = 0; i < MAX_TRAFFIC_CLASS; i++) {
		credit_refill = refill[i];
		credit_max    = max[i];
		reg = credit_refill | (credit_max << IXGBE_RTRPT4C_MCL_SHIFT);

		reg |= (u32)(bwg_id[i]) << IXGBE_RTRPT4C_BWG_SHIFT;

		if (prio_type[i] == prio_link)
			reg |= IXGBE_RTRPT4C_LSP;

		IXGBE_WRITE_REG(hw, IXGBE_RTRPT4C(i), reg);
	}

	reg = IXGBE_RTRPCS_RRM | IXGBE_RTRPCS_RAC;
	IXGBE_WRITE_REG(hw, IXGBE_RTRPCS, reg);

	return 0;
}

s32 ixgbe_dcb_config_tx_desc_arbiter_82599(struct ixgbe_hw *hw,
					   u16 *refill,
					   u16 *max,
					   u8 *bwg_id,
					   u8 *prio_type)
{
	u32    reg, max_credits;
	u8     i;

	
	for (i = 0; i < 128; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_RTTDQSEL, i);
		IXGBE_WRITE_REG(hw, IXGBE_RTTDT1C, 0);
	}

	
	for (i = 0; i < MAX_TRAFFIC_CLASS; i++) {
		max_credits = max[i];
		reg = max_credits << IXGBE_RTTDT2C_MCL_SHIFT;
		reg |= refill[i];
		reg |= (u32)(bwg_id[i]) << IXGBE_RTTDT2C_BWG_SHIFT;

		if (prio_type[i] == prio_group)
			reg |= IXGBE_RTTDT2C_GSP;

		if (prio_type[i] == prio_link)
			reg |= IXGBE_RTTDT2C_LSP;

		IXGBE_WRITE_REG(hw, IXGBE_RTTDT2C(i), reg);
	}

	reg = IXGBE_RTTDCS_TDPAC | IXGBE_RTTDCS_TDRM;
	IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, reg);

	return 0;
}

s32 ixgbe_dcb_config_tx_data_arbiter_82599(struct ixgbe_hw *hw,
					   u16 *refill,
					   u16 *max,
					   u8 *bwg_id,
					   u8 *prio_type,
					   u8 *prio_tc)
{
	u32 reg;
	u8 i;

	reg = IXGBE_RTTPCS_TPPAC | IXGBE_RTTPCS_TPRM |
	      (IXGBE_RTTPCS_ARBD_DCB << IXGBE_RTTPCS_ARBD_SHIFT) |
	      IXGBE_RTTPCS_ARBDIS;
	IXGBE_WRITE_REG(hw, IXGBE_RTTPCS, reg);

	
	reg = 0;
	for (i = 0; i < MAX_USER_PRIORITY; i++)
		reg |= (prio_tc[i] << (i * IXGBE_RTTUP2TC_UP_SHIFT));
	IXGBE_WRITE_REG(hw, IXGBE_RTTUP2TC, reg);

	
	for (i = 0; i < MAX_TRAFFIC_CLASS; i++) {
		reg = refill[i];
		reg |= (u32)(max[i]) << IXGBE_RTTPT2C_MCL_SHIFT;
		reg |= (u32)(bwg_id[i]) << IXGBE_RTTPT2C_BWG_SHIFT;

		if (prio_type[i] == prio_group)
			reg |= IXGBE_RTTPT2C_GSP;

		if (prio_type[i] == prio_link)
			reg |= IXGBE_RTTPT2C_LSP;

		IXGBE_WRITE_REG(hw, IXGBE_RTTPT2C(i), reg);
	}

	reg = IXGBE_RTTPCS_TPPAC | IXGBE_RTTPCS_TPRM |
	      (IXGBE_RTTPCS_ARBD_DCB << IXGBE_RTTPCS_ARBD_SHIFT);
	IXGBE_WRITE_REG(hw, IXGBE_RTTPCS, reg);

	return 0;
}

s32 ixgbe_dcb_config_pfc_82599(struct ixgbe_hw *hw, u8 pfc_en, u8 *prio_tc)
{
	u32 i, j, reg;
	u8 max_tc = 0;

	for (i = 0; i < MAX_USER_PRIORITY; i++)
		if (prio_tc[i] > max_tc)
			max_tc = prio_tc[i];

	
	for (i = 0; i < MAX_TRAFFIC_CLASS; i++) {
		int enabled = 0;

		if (i > max_tc) {
			reg = 0;
			IXGBE_WRITE_REG(hw, IXGBE_FCRTL_82599(i), reg);
			IXGBE_WRITE_REG(hw, IXGBE_FCRTH_82599(i), reg);
			continue;
		}

		for (j = 0; j < MAX_USER_PRIORITY; j++) {
			if ((prio_tc[j] == i) && (pfc_en & (1 << j))) {
				enabled = 1;
				break;
			}
		}

		reg = hw->fc.low_water << 10;

		if (enabled)
			reg |= IXGBE_FCRTL_XONE;
		IXGBE_WRITE_REG(hw, IXGBE_FCRTL_82599(i), reg);

		reg = hw->fc.high_water[i] << 10;
		if (enabled)
			reg |= IXGBE_FCRTH_FCEN;
		IXGBE_WRITE_REG(hw, IXGBE_FCRTH_82599(i), reg);
	}

	if (pfc_en) {
		
		reg = hw->fc.pause_time | (hw->fc.pause_time << 16);
		for (i = 0; i < (MAX_TRAFFIC_CLASS / 2); i++)
			IXGBE_WRITE_REG(hw, IXGBE_FCTTV(i), reg);

		
		IXGBE_WRITE_REG(hw, IXGBE_FCRTV, hw->fc.pause_time / 2);


		reg = IXGBE_FCCFG_TFCE_PRIORITY;
		IXGBE_WRITE_REG(hw, IXGBE_FCCFG, reg);
		reg = IXGBE_READ_REG(hw, IXGBE_MFLCN);
		reg &= ~IXGBE_MFLCN_RFCE;
		reg |= IXGBE_MFLCN_RPFCE | IXGBE_MFLCN_DPF;

		if (hw->mac.type == ixgbe_mac_X540) {
			reg &= ~IXGBE_MFLCN_RPFCE_MASK;
			reg |= pfc_en << IXGBE_MFLCN_RPFCE_SHIFT;
		}

		IXGBE_WRITE_REG(hw, IXGBE_MFLCN, reg);

	} else {
		if (hw->mac.type == ixgbe_mac_X540) {
			reg = IXGBE_READ_REG(hw, IXGBE_MFLCN);
			reg &= ~IXGBE_MFLCN_RPFCE_MASK;
			IXGBE_WRITE_REG(hw, IXGBE_MFLCN, reg);
		}

		for (i = 0; i < MAX_TRAFFIC_CLASS; i++)
			hw->mac.ops.fc_enable(hw, i);
	}

	return 0;
}

static s32 ixgbe_dcb_config_tc_stats_82599(struct ixgbe_hw *hw)
{
	u32 reg = 0;
	u8  i   = 0;

	for (i = 0; i < 32; i++) {
		reg = 0x01010101 * (i / 4);
		IXGBE_WRITE_REG(hw, IXGBE_RQSMR(i), reg);
	}
	for (i = 0; i < 32; i++) {
		if (i < 8)
			reg = 0x00000000;
		else if (i < 16)
			reg = 0x01010101;
		else if (i < 20)
			reg = 0x02020202;
		else if (i < 24)
			reg = 0x03030303;
		else if (i < 26)
			reg = 0x04040404;
		else if (i < 28)
			reg = 0x05050505;
		else if (i < 30)
			reg = 0x06060606;
		else
			reg = 0x07070707;
		IXGBE_WRITE_REG(hw, IXGBE_TQSM(i), reg);
	}

	return 0;
}

s32 ixgbe_dcb_hw_config_82599(struct ixgbe_hw *hw, u8 pfc_en, u16 *refill,
			      u16 *max, u8 *bwg_id, u8 *prio_type, u8 *prio_tc)
{
	ixgbe_dcb_config_rx_arbiter_82599(hw, refill, max, bwg_id,
					  prio_type, prio_tc);
	ixgbe_dcb_config_tx_desc_arbiter_82599(hw, refill, max,
					       bwg_id, prio_type);
	ixgbe_dcb_config_tx_data_arbiter_82599(hw, refill, max,
					       bwg_id, prio_type, prio_tc);
	ixgbe_dcb_config_pfc_82599(hw, pfc_en, prio_tc);
	ixgbe_dcb_config_tc_stats_82599(hw);

	return 0;
}

