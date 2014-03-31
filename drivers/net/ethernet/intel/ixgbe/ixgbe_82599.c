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

#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include "ixgbe.h"
#include "ixgbe_phy.h"
#include "ixgbe_mbx.h"

#define IXGBE_82599_MAX_TX_QUEUES 128
#define IXGBE_82599_MAX_RX_QUEUES 128
#define IXGBE_82599_RAR_ENTRIES   128
#define IXGBE_82599_MC_TBL_SIZE   128
#define IXGBE_82599_VFT_TBL_SIZE  128
#define IXGBE_82599_RX_PB_SIZE	  512

static void ixgbe_disable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
static void ixgbe_enable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
static void ixgbe_flap_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
static s32 ixgbe_setup_mac_link_multispeed_fiber(struct ixgbe_hw *hw,
						 ixgbe_link_speed speed,
						 bool autoneg,
						 bool autoneg_wait_to_complete);
static s32 ixgbe_setup_mac_link_smartspeed(struct ixgbe_hw *hw,
                                           ixgbe_link_speed speed,
                                           bool autoneg,
                                           bool autoneg_wait_to_complete);
static s32 ixgbe_start_mac_link_82599(struct ixgbe_hw *hw,
				      bool autoneg_wait_to_complete);
static s32 ixgbe_setup_mac_link_82599(struct ixgbe_hw *hw,
                               ixgbe_link_speed speed,
                               bool autoneg,
                               bool autoneg_wait_to_complete);
static s32 ixgbe_setup_copper_link_82599(struct ixgbe_hw *hw,
                                         ixgbe_link_speed speed,
                                         bool autoneg,
                                         bool autoneg_wait_to_complete);
static s32 ixgbe_verify_fw_version_82599(struct ixgbe_hw *hw);
static bool ixgbe_verify_lesm_fw_enabled_82599(struct ixgbe_hw *hw);

static void ixgbe_init_mac_link_ops_82599(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;

	
	if (mac->ops.get_media_type(hw) == ixgbe_media_type_fiber) {
		mac->ops.disable_tx_laser =
		                       &ixgbe_disable_tx_laser_multispeed_fiber;
		mac->ops.enable_tx_laser =
		                        &ixgbe_enable_tx_laser_multispeed_fiber;
		mac->ops.flap_tx_laser = &ixgbe_flap_tx_laser_multispeed_fiber;
	} else {
		mac->ops.disable_tx_laser = NULL;
		mac->ops.enable_tx_laser = NULL;
		mac->ops.flap_tx_laser = NULL;
	}

	if (hw->phy.multispeed_fiber) {
		
		mac->ops.setup_link = &ixgbe_setup_mac_link_multispeed_fiber;
	} else {
		if ((mac->ops.get_media_type(hw) ==
		     ixgbe_media_type_backplane) &&
		    (hw->phy.smart_speed == ixgbe_smart_speed_auto ||
		     hw->phy.smart_speed == ixgbe_smart_speed_on) &&
		     !ixgbe_verify_lesm_fw_enabled_82599(hw))
			mac->ops.setup_link = &ixgbe_setup_mac_link_smartspeed;
		else
			mac->ops.setup_link = &ixgbe_setup_mac_link_82599;
	}
}

static s32 ixgbe_setup_sfp_modules_82599(struct ixgbe_hw *hw)
{
	s32 ret_val = 0;
	u32 reg_anlp1 = 0;
	u32 i = 0;
	u16 list_offset, data_offset, data_value;

	if (hw->phy.sfp_type != ixgbe_sfp_type_unknown) {
		ixgbe_init_mac_link_ops_82599(hw);

		hw->phy.ops.reset = NULL;

		ret_val = ixgbe_get_sfp_init_sequence_offsets(hw, &list_offset,
		                                              &data_offset);
		if (ret_val != 0)
			goto setup_sfp_out;

		
		ret_val = hw->mac.ops.acquire_swfw_sync(hw,
		                                        IXGBE_GSSR_MAC_CSR_SM);
		if (ret_val != 0) {
			ret_val = IXGBE_ERR_SWFW_SYNC;
			goto setup_sfp_out;
		}

		hw->eeprom.ops.read(hw, ++data_offset, &data_value);
		while (data_value != 0xffff) {
			IXGBE_WRITE_REG(hw, IXGBE_CORECTL, data_value);
			IXGBE_WRITE_FLUSH(hw);
			hw->eeprom.ops.read(hw, ++data_offset, &data_value);
		}

		
		hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_MAC_CSR_SM);
		usleep_range(hw->eeprom.semaphore_delay * 1000,
			     hw->eeprom.semaphore_delay * 2000);

		
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, ((IXGBE_READ_REG(hw,
		                IXGBE_AUTOC) & ~IXGBE_AUTOC_LMS_MASK) |
		                IXGBE_AUTOC_AN_RESTART));

		
		for (i = 0; i < 10; i++) {
			usleep_range(4000, 8000);
			reg_anlp1 = IXGBE_READ_REG(hw, IXGBE_ANLP1);
			if (reg_anlp1 & IXGBE_ANLP1_AN_STATE_MASK)
				break;
		}
		if (!(reg_anlp1 & IXGBE_ANLP1_AN_STATE_MASK)) {
			hw_dbg(hw, "sfp module setup not complete\n");
			ret_val = IXGBE_ERR_SFP_SETUP_NOT_COMPLETE;
			goto setup_sfp_out;
		}

		
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, (IXGBE_READ_REG(hw,
		                IXGBE_AUTOC) | IXGBE_AUTOC_LMS_10G_SERIAL |
		                IXGBE_AUTOC_AN_RESTART));
	}

setup_sfp_out:
	return ret_val;
}

static s32 ixgbe_get_invariants_82599(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;

	ixgbe_init_mac_link_ops_82599(hw);

	mac->mcft_size = IXGBE_82599_MC_TBL_SIZE;
	mac->vft_size = IXGBE_82599_VFT_TBL_SIZE;
	mac->num_rar_entries = IXGBE_82599_RAR_ENTRIES;
	mac->max_rx_queues = IXGBE_82599_MAX_RX_QUEUES;
	mac->max_tx_queues = IXGBE_82599_MAX_TX_QUEUES;
	mac->max_msix_vectors = ixgbe_get_pcie_msix_count_generic(hw);

	return 0;
}

static s32 ixgbe_init_phy_ops_82599(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;
	struct ixgbe_phy_info *phy = &hw->phy;
	s32 ret_val = 0;

	
	ret_val = phy->ops.identify(hw);

	
	ixgbe_init_mac_link_ops_82599(hw);

	
	if (mac->ops.get_media_type(hw) == ixgbe_media_type_copper) {
		mac->ops.setup_link = &ixgbe_setup_copper_link_82599;
		mac->ops.get_link_capabilities =
			&ixgbe_get_copper_link_capabilities_generic;
	}

	
	switch (hw->phy.type) {
	case ixgbe_phy_tn:
		phy->ops.check_link = &ixgbe_check_phy_link_tnx;
		phy->ops.setup_link = &ixgbe_setup_phy_link_tnx;
		phy->ops.get_firmware_version =
		             &ixgbe_get_phy_firmware_version_tnx;
		break;
	default:
		break;
	}

	return ret_val;
}

static s32 ixgbe_get_link_capabilities_82599(struct ixgbe_hw *hw,
                                             ixgbe_link_speed *speed,
                                             bool *negotiation)
{
	s32 status = 0;
	u32 autoc = 0;

	
	if (hw->phy.sfp_type == ixgbe_sfp_type_1g_cu_core0 ||
	    hw->phy.sfp_type == ixgbe_sfp_type_1g_cu_core1) {
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = true;
		goto out;
	}

	if (hw->mac.orig_link_settings_stored)
		autoc = hw->mac.orig_autoc;
	else
		autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);

	switch (autoc & IXGBE_AUTOC_LMS_MASK) {
	case IXGBE_AUTOC_LMS_1G_LINK_NO_AN:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = false;
		break;

	case IXGBE_AUTOC_LMS_10G_LINK_NO_AN:
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
		*negotiation = false;
		break;

	case IXGBE_AUTOC_LMS_1G_AN:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = true;
		break;

	case IXGBE_AUTOC_LMS_10G_SERIAL:
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
		*negotiation = false;
		break;

	case IXGBE_AUTOC_LMS_KX4_KX_KR:
	case IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN:
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
		if (autoc & IXGBE_AUTOC_KR_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX4_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX_SUPP)
			*speed |= IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = true;
		break;

	case IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII:
		*speed = IXGBE_LINK_SPEED_100_FULL;
		if (autoc & IXGBE_AUTOC_KR_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX4_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX_SUPP)
			*speed |= IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = true;
		break;

	case IXGBE_AUTOC_LMS_SGMII_1G_100M:
		*speed = IXGBE_LINK_SPEED_1GB_FULL | IXGBE_LINK_SPEED_100_FULL;
		*negotiation = false;
		break;

	default:
		status = IXGBE_ERR_LINK_SETUP;
		goto out;
		break;
	}

	if (hw->phy.multispeed_fiber) {
		*speed |= IXGBE_LINK_SPEED_10GB_FULL |
		          IXGBE_LINK_SPEED_1GB_FULL;
		*negotiation = true;
	}

out:
	return status;
}

static enum ixgbe_media_type ixgbe_get_media_type_82599(struct ixgbe_hw *hw)
{
	enum ixgbe_media_type media_type;

	
	switch (hw->phy.type) {
	case ixgbe_phy_cu_unknown:
	case ixgbe_phy_tn:
		media_type = ixgbe_media_type_copper;
		goto out;
	default:
		break;
	}

	switch (hw->device_id) {
	case IXGBE_DEV_ID_82599_KX4:
	case IXGBE_DEV_ID_82599_KX4_MEZZ:
	case IXGBE_DEV_ID_82599_COMBO_BACKPLANE:
	case IXGBE_DEV_ID_82599_KR:
	case IXGBE_DEV_ID_82599_BACKPLANE_FCOE:
	case IXGBE_DEV_ID_82599_XAUI_LOM:
		
		media_type = ixgbe_media_type_backplane;
		break;
	case IXGBE_DEV_ID_82599_SFP:
	case IXGBE_DEV_ID_82599_SFP_FCOE:
	case IXGBE_DEV_ID_82599_SFP_EM:
	case IXGBE_DEV_ID_82599_SFP_SF2:
	case IXGBE_DEV_ID_82599_SFP_SF_QP:
	case IXGBE_DEV_ID_82599EN_SFP:
		media_type = ixgbe_media_type_fiber;
		break;
	case IXGBE_DEV_ID_82599_CX4:
		media_type = ixgbe_media_type_cx4;
		break;
	case IXGBE_DEV_ID_82599_T3_LOM:
		media_type = ixgbe_media_type_copper;
		break;
	case IXGBE_DEV_ID_82599_LS:
		media_type = ixgbe_media_type_fiber_lco;
		break;
	default:
		media_type = ixgbe_media_type_unknown;
		break;
	}
out:
	return media_type;
}

static s32 ixgbe_start_mac_link_82599(struct ixgbe_hw *hw,
                               bool autoneg_wait_to_complete)
{
	u32 autoc_reg;
	u32 links_reg;
	u32 i;
	s32 status = 0;

	
	autoc_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	autoc_reg |= IXGBE_AUTOC_AN_RESTART;
	IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc_reg);

	
	if (autoneg_wait_to_complete) {
		if ((autoc_reg & IXGBE_AUTOC_LMS_MASK) ==
		     IXGBE_AUTOC_LMS_KX4_KX_KR ||
		    (autoc_reg & IXGBE_AUTOC_LMS_MASK) ==
		     IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN ||
		    (autoc_reg & IXGBE_AUTOC_LMS_MASK) ==
		     IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII) {
			links_reg = 0; 
			for (i = 0; i < IXGBE_AUTO_NEG_TIME; i++) {
				links_reg = IXGBE_READ_REG(hw, IXGBE_LINKS);
				if (links_reg & IXGBE_LINKS_KX_AN_COMP)
					break;
				msleep(100);
			}
			if (!(links_reg & IXGBE_LINKS_KX_AN_COMP)) {
				status = IXGBE_ERR_AUTONEG_NOT_COMPLETE;
				hw_dbg(hw, "Autoneg did not complete.\n");
			}
		}
	}

	
	msleep(50);

	return status;
}

static void ixgbe_disable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw)
{
	u32 esdp_reg = IXGBE_READ_REG(hw, IXGBE_ESDP);

	
	esdp_reg |= IXGBE_ESDP_SDP3;
	IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
	IXGBE_WRITE_FLUSH(hw);
	udelay(100);
}

static void ixgbe_enable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw)
{
	u32 esdp_reg = IXGBE_READ_REG(hw, IXGBE_ESDP);

	
	esdp_reg &= ~IXGBE_ESDP_SDP3;
	IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
	IXGBE_WRITE_FLUSH(hw);
	msleep(100);
}

static void ixgbe_flap_tx_laser_multispeed_fiber(struct ixgbe_hw *hw)
{
	if (hw->mac.autotry_restart) {
		ixgbe_disable_tx_laser_multispeed_fiber(hw);
		ixgbe_enable_tx_laser_multispeed_fiber(hw);
		hw->mac.autotry_restart = false;
	}
}

static s32 ixgbe_setup_mac_link_multispeed_fiber(struct ixgbe_hw *hw,
                                          ixgbe_link_speed speed,
                                          bool autoneg,
                                          bool autoneg_wait_to_complete)
{
	s32 status = 0;
	ixgbe_link_speed link_speed = IXGBE_LINK_SPEED_UNKNOWN;
	ixgbe_link_speed highest_link_speed = IXGBE_LINK_SPEED_UNKNOWN;
	u32 speedcnt = 0;
	u32 esdp_reg = IXGBE_READ_REG(hw, IXGBE_ESDP);
	u32 i = 0;
	bool link_up = false;
	bool negotiation;

	
	status = hw->mac.ops.get_link_capabilities(hw, &link_speed,
						   &negotiation);
	if (status != 0)
		return status;

	speed &= link_speed;

	if (speed & IXGBE_LINK_SPEED_10GB_FULL) {
		speedcnt++;
		highest_link_speed = IXGBE_LINK_SPEED_10GB_FULL;

		
		status = hw->mac.ops.check_link(hw, &link_speed, &link_up,
						false);
		if (status != 0)
			return status;

		if ((link_speed == IXGBE_LINK_SPEED_10GB_FULL) && link_up)
			goto out;

		
		esdp_reg |= (IXGBE_ESDP_SDP5_DIR | IXGBE_ESDP_SDP5);
		IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
		IXGBE_WRITE_FLUSH(hw);

		
		msleep(40);

		status = ixgbe_setup_mac_link_82599(hw,
						    IXGBE_LINK_SPEED_10GB_FULL,
						    autoneg,
						    autoneg_wait_to_complete);
		if (status != 0)
			return status;

		
		hw->mac.ops.flap_tx_laser(hw);

		for (i = 0; i < 5; i++) {
			
			msleep(100);

			
			status = hw->mac.ops.check_link(hw, &link_speed,
							&link_up, false);
			if (status != 0)
				return status;

			if (link_up)
				goto out;
		}
	}

	if (speed & IXGBE_LINK_SPEED_1GB_FULL) {
		speedcnt++;
		if (highest_link_speed == IXGBE_LINK_SPEED_UNKNOWN)
			highest_link_speed = IXGBE_LINK_SPEED_1GB_FULL;

		
		status = hw->mac.ops.check_link(hw, &link_speed, &link_up,
						false);
		if (status != 0)
			return status;

		if ((link_speed == IXGBE_LINK_SPEED_1GB_FULL) && link_up)
			goto out;

		
		esdp_reg &= ~IXGBE_ESDP_SDP5;
		esdp_reg |= IXGBE_ESDP_SDP5_DIR;
		IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
		IXGBE_WRITE_FLUSH(hw);

		
		msleep(40);

		status = ixgbe_setup_mac_link_82599(hw,
						    IXGBE_LINK_SPEED_1GB_FULL,
						    autoneg,
						    autoneg_wait_to_complete);
		if (status != 0)
			return status;

		
		hw->mac.ops.flap_tx_laser(hw);

		
		msleep(100);

		
		status = hw->mac.ops.check_link(hw, &link_speed, &link_up,
						false);
		if (status != 0)
			return status;

		if (link_up)
			goto out;
	}

	if (speedcnt > 1)
		status = ixgbe_setup_mac_link_multispeed_fiber(hw,
		                                               highest_link_speed,
		                                               autoneg,
		                                               autoneg_wait_to_complete);

out:
	
	hw->phy.autoneg_advertised = 0;

	if (speed & IXGBE_LINK_SPEED_10GB_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_10GB_FULL;

	if (speed & IXGBE_LINK_SPEED_1GB_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_1GB_FULL;

	return status;
}

static s32 ixgbe_setup_mac_link_smartspeed(struct ixgbe_hw *hw,
				     ixgbe_link_speed speed, bool autoneg,
				     bool autoneg_wait_to_complete)
{
	s32 status = 0;
	ixgbe_link_speed link_speed = IXGBE_LINK_SPEED_UNKNOWN;
	s32 i, j;
	bool link_up = false;
	u32 autoc_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC);

	 
	hw->phy.autoneg_advertised = 0;

	if (speed & IXGBE_LINK_SPEED_10GB_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_10GB_FULL;

	if (speed & IXGBE_LINK_SPEED_1GB_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_1GB_FULL;

	if (speed & IXGBE_LINK_SPEED_100_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_100_FULL;


	
	hw->phy.smart_speed_active = false;
	for (j = 0; j < IXGBE_SMARTSPEED_MAX_RETRIES; j++) {
		status = ixgbe_setup_mac_link_82599(hw, speed, autoneg,
						    autoneg_wait_to_complete);
		if (status != 0)
			goto out;

		for (i = 0; i < 5; i++) {
			mdelay(100);

			
			status = hw->mac.ops.check_link(hw, &link_speed,
							&link_up, false);
			if (status != 0)
				goto out;

			if (link_up)
				goto out;
		}
	}

	if (((autoc_reg & IXGBE_AUTOC_KR_SUPP) == 0) ||
	    ((autoc_reg & IXGBE_AUTOC_KX4_KX_SUPP_MASK) == 0))
		goto out;

	
	hw->phy.smart_speed_active = true;
	status = ixgbe_setup_mac_link_82599(hw, speed, autoneg,
					    autoneg_wait_to_complete);
	if (status != 0)
		goto out;

	for (i = 0; i < 6; i++) {
		mdelay(100);

		
		status = hw->mac.ops.check_link(hw, &link_speed,
						&link_up, false);
		if (status != 0)
			goto out;

		if (link_up)
			goto out;
	}

	
	hw->phy.smart_speed_active = false;
	status = ixgbe_setup_mac_link_82599(hw, speed, autoneg,
					    autoneg_wait_to_complete);

out:
	if (link_up && (link_speed == IXGBE_LINK_SPEED_1GB_FULL))
		hw_dbg(hw, "Smartspeed has downgraded the link speed from "
		       "the maximum advertised\n");
	return status;
}

static s32 ixgbe_setup_mac_link_82599(struct ixgbe_hw *hw,
                               ixgbe_link_speed speed, bool autoneg,
                               bool autoneg_wait_to_complete)
{
	s32 status = 0;
	u32 autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	u32 autoc2 = IXGBE_READ_REG(hw, IXGBE_AUTOC2);
	u32 start_autoc = autoc;
	u32 orig_autoc = 0;
	u32 link_mode = autoc & IXGBE_AUTOC_LMS_MASK;
	u32 pma_pmd_1g = autoc & IXGBE_AUTOC_1G_PMA_PMD_MASK;
	u32 pma_pmd_10g_serial = autoc2 & IXGBE_AUTOC2_10G_SERIAL_PMA_PMD_MASK;
	u32 links_reg;
	u32 i;
	ixgbe_link_speed link_capabilities = IXGBE_LINK_SPEED_UNKNOWN;

	
	status = hw->mac.ops.get_link_capabilities(hw, &link_capabilities,
						   &autoneg);
	if (status != 0)
		goto out;

	speed &= link_capabilities;

	if (speed == IXGBE_LINK_SPEED_UNKNOWN) {
		status = IXGBE_ERR_LINK_SETUP;
		goto out;
	}

	
	if (hw->mac.orig_link_settings_stored)
		orig_autoc = hw->mac.orig_autoc;
	else
		orig_autoc = autoc;

	if (link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR ||
	    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN ||
	    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII) {
		
		autoc &= ~(IXGBE_AUTOC_KX4_KX_SUPP_MASK | IXGBE_AUTOC_KR_SUPP);
		if (speed & IXGBE_LINK_SPEED_10GB_FULL)
			if (orig_autoc & IXGBE_AUTOC_KX4_SUPP)
				autoc |= IXGBE_AUTOC_KX4_SUPP;
			if ((orig_autoc & IXGBE_AUTOC_KR_SUPP) &&
			    (hw->phy.smart_speed_active == false))
				autoc |= IXGBE_AUTOC_KR_SUPP;
		if (speed & IXGBE_LINK_SPEED_1GB_FULL)
			autoc |= IXGBE_AUTOC_KX_SUPP;
	} else if ((pma_pmd_1g == IXGBE_AUTOC_1G_SFI) &&
	           (link_mode == IXGBE_AUTOC_LMS_1G_LINK_NO_AN ||
	            link_mode == IXGBE_AUTOC_LMS_1G_AN)) {
		
		if ((speed == IXGBE_LINK_SPEED_10GB_FULL) &&
		    (pma_pmd_10g_serial == IXGBE_AUTOC2_10G_SFI)) {
			autoc &= ~IXGBE_AUTOC_LMS_MASK;
			autoc |= IXGBE_AUTOC_LMS_10G_SERIAL;
		}
	} else if ((pma_pmd_10g_serial == IXGBE_AUTOC2_10G_SFI) &&
	           (link_mode == IXGBE_AUTOC_LMS_10G_SERIAL)) {
		
		if ((speed == IXGBE_LINK_SPEED_1GB_FULL) &&
		    (pma_pmd_1g == IXGBE_AUTOC_1G_SFI)) {
			autoc &= ~IXGBE_AUTOC_LMS_MASK;
			if (autoneg)
				autoc |= IXGBE_AUTOC_LMS_1G_AN;
			else
				autoc |= IXGBE_AUTOC_LMS_1G_LINK_NO_AN;
		}
	}

	if (autoc != start_autoc) {
		
		autoc |= IXGBE_AUTOC_AN_RESTART;
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc);

		
		if (autoneg_wait_to_complete) {
			if (link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR ||
			    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN ||
			    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII) {
				links_reg = 0; 
				for (i = 0; i < IXGBE_AUTO_NEG_TIME; i++) {
					links_reg =
					       IXGBE_READ_REG(hw, IXGBE_LINKS);
					if (links_reg & IXGBE_LINKS_KX_AN_COMP)
						break;
					msleep(100);
				}
				if (!(links_reg & IXGBE_LINKS_KX_AN_COMP)) {
					status =
					        IXGBE_ERR_AUTONEG_NOT_COMPLETE;
					hw_dbg(hw, "Autoneg did not "
					       "complete.\n");
				}
			}
		}

		
		msleep(50);
	}

out:
	return status;
}

static s32 ixgbe_setup_copper_link_82599(struct ixgbe_hw *hw,
                                         ixgbe_link_speed speed,
                                         bool autoneg,
                                         bool autoneg_wait_to_complete)
{
	s32 status;

	
	status = hw->phy.ops.setup_link_speed(hw, speed, autoneg,
	                                      autoneg_wait_to_complete);
	
	ixgbe_start_mac_link_82599(hw, autoneg_wait_to_complete);

	return status;
}

static s32 ixgbe_reset_hw_82599(struct ixgbe_hw *hw)
{
	ixgbe_link_speed link_speed;
	s32 status;
	u32 ctrl, i, autoc, autoc2;
	bool link_up = false;

	
	status = hw->mac.ops.stop_adapter(hw);
	if (status != 0)
		goto reset_hw_out;

	
	ixgbe_clear_tx_pending(hw);

	

	
	status = hw->phy.ops.init(hw);

	if (status == IXGBE_ERR_SFP_NOT_SUPPORTED)
		goto reset_hw_out;

	
	if (hw->phy.sfp_setup_needed) {
		status = hw->mac.ops.setup_sfp(hw);
		hw->phy.sfp_setup_needed = false;
	}

	if (status == IXGBE_ERR_SFP_NOT_SUPPORTED)
		goto reset_hw_out;

	
	if (hw->phy.reset_disable == false && hw->phy.ops.reset != NULL)
		hw->phy.ops.reset(hw);

mac_reset_top:
	ctrl = IXGBE_CTRL_LNK_RST;
	if (!hw->force_full_reset) {
		hw->mac.ops.check_link(hw, &link_speed, &link_up, false);
		if (link_up)
			ctrl = IXGBE_CTRL_RST;
	}

	ctrl |= IXGBE_READ_REG(hw, IXGBE_CTRL);
	IXGBE_WRITE_REG(hw, IXGBE_CTRL, ctrl);
	IXGBE_WRITE_FLUSH(hw);

	
	for (i = 0; i < 10; i++) {
		udelay(1);
		ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
		if (!(ctrl & IXGBE_CTRL_RST_MASK))
			break;
	}

	if (ctrl & IXGBE_CTRL_RST_MASK) {
		status = IXGBE_ERR_RESET_FAILED;
		hw_dbg(hw, "Reset polling failed to complete.\n");
	}

	msleep(50);

	if (hw->mac.flags & IXGBE_FLAGS_DOUBLE_RESET_REQUIRED) {
		hw->mac.flags &= ~IXGBE_FLAGS_DOUBLE_RESET_REQUIRED;
		goto mac_reset_top;
	}

	autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	autoc2 = IXGBE_READ_REG(hw, IXGBE_AUTOC2);
	if (hw->mac.orig_link_settings_stored == false) {
		hw->mac.orig_autoc = autoc;
		hw->mac.orig_autoc2 = autoc2;
		hw->mac.orig_link_settings_stored = true;
	} else {
		if (autoc != hw->mac.orig_autoc)
			IXGBE_WRITE_REG(hw, IXGBE_AUTOC, (hw->mac.orig_autoc |
			                IXGBE_AUTOC_AN_RESTART));

		if ((autoc2 & IXGBE_AUTOC2_UPPER_MASK) !=
		    (hw->mac.orig_autoc2 & IXGBE_AUTOC2_UPPER_MASK)) {
			autoc2 &= ~IXGBE_AUTOC2_UPPER_MASK;
			autoc2 |= (hw->mac.orig_autoc2 &
			           IXGBE_AUTOC2_UPPER_MASK);
			IXGBE_WRITE_REG(hw, IXGBE_AUTOC2, autoc2);
		}
	}

	
	hw->mac.ops.get_mac_addr(hw, hw->mac.perm_addr);

	hw->mac.num_rar_entries = 128;
	hw->mac.ops.init_rx_addrs(hw);

	
	hw->mac.ops.get_san_mac_addr(hw, hw->mac.san_addr);

	
	if (ixgbe_validate_mac_addr(hw->mac.san_addr) == 0) {
		hw->mac.ops.set_rar(hw, hw->mac.num_rar_entries - 1,
		                    hw->mac.san_addr, 0, IXGBE_RAH_AV);

		
		hw->mac.num_rar_entries--;
	}

	
	hw->mac.ops.get_wwn_prefix(hw, &hw->mac.wwnn_prefix,
	                               &hw->mac.wwpn_prefix);

reset_hw_out:
	return status;
}

s32 ixgbe_reinit_fdir_tables_82599(struct ixgbe_hw *hw)
{
	int i;
	u32 fdirctrl = IXGBE_READ_REG(hw, IXGBE_FDIRCTRL);
	fdirctrl &= ~IXGBE_FDIRCTRL_INIT_DONE;

	for (i = 0; i < IXGBE_FDIRCMD_CMD_POLL; i++) {
		if (!(IXGBE_READ_REG(hw, IXGBE_FDIRCMD) &
		      IXGBE_FDIRCMD_CMD_MASK))
			break;
		udelay(10);
	}
	if (i >= IXGBE_FDIRCMD_CMD_POLL) {
		hw_dbg(hw, "Flow Director previous command isn't complete, "
		       "aborting table re-initialization.\n");
		return IXGBE_ERR_FDIR_REINIT_FAILED;
	}

	IXGBE_WRITE_REG(hw, IXGBE_FDIRFREE, 0);
	IXGBE_WRITE_FLUSH(hw);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD,
	                (IXGBE_READ_REG(hw, IXGBE_FDIRCMD) |
	                 IXGBE_FDIRCMD_CLEARHT));
	IXGBE_WRITE_FLUSH(hw);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD,
	                (IXGBE_READ_REG(hw, IXGBE_FDIRCMD) &
	                 ~IXGBE_FDIRCMD_CLEARHT));
	IXGBE_WRITE_FLUSH(hw);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRHASH, 0x00);
	IXGBE_WRITE_FLUSH(hw);

	IXGBE_WRITE_REG(hw, IXGBE_FDIRCTRL, fdirctrl);
	IXGBE_WRITE_FLUSH(hw);

	
	for (i = 0; i < IXGBE_FDIR_INIT_DONE_POLL; i++) {
		if (IXGBE_READ_REG(hw, IXGBE_FDIRCTRL) &
		                   IXGBE_FDIRCTRL_INIT_DONE)
			break;
		udelay(10);
	}
	if (i >= IXGBE_FDIR_INIT_DONE_POLL) {
		hw_dbg(hw, "Flow Director Signature poll time exceeded!\n");
		return IXGBE_ERR_FDIR_REINIT_FAILED;
	}

	
	IXGBE_READ_REG(hw, IXGBE_FDIRUSTAT);
	IXGBE_READ_REG(hw, IXGBE_FDIRFSTAT);
	IXGBE_READ_REG(hw, IXGBE_FDIRMATCH);
	IXGBE_READ_REG(hw, IXGBE_FDIRMISS);
	IXGBE_READ_REG(hw, IXGBE_FDIRLEN);

	return 0;
}

static void ixgbe_fdir_enable_82599(struct ixgbe_hw *hw, u32 fdirctrl)
{
	int i;

	
	IXGBE_WRITE_REG(hw, IXGBE_FDIRHKEY, IXGBE_ATR_BUCKET_HASH_KEY);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRSKEY, IXGBE_ATR_SIGNATURE_HASH_KEY);

	IXGBE_WRITE_REG(hw, IXGBE_FDIRCTRL, fdirctrl);
	IXGBE_WRITE_FLUSH(hw);
	for (i = 0; i < IXGBE_FDIR_INIT_DONE_POLL; i++) {
		if (IXGBE_READ_REG(hw, IXGBE_FDIRCTRL) &
		                   IXGBE_FDIRCTRL_INIT_DONE)
			break;
		usleep_range(1000, 2000);
	}

	if (i >= IXGBE_FDIR_INIT_DONE_POLL)
		hw_dbg(hw, "Flow Director poll time exceeded!\n");
}

s32 ixgbe_init_fdir_signature_82599(struct ixgbe_hw *hw, u32 fdirctrl)
{
	fdirctrl |= (0x6 << IXGBE_FDIRCTRL_FLEX_SHIFT) |
		    (0xA << IXGBE_FDIRCTRL_MAX_LENGTH_SHIFT) |
		    (4 << IXGBE_FDIRCTRL_FULL_THRESH_SHIFT);

	
	ixgbe_fdir_enable_82599(hw, fdirctrl);

	return 0;
}

s32 ixgbe_init_fdir_perfect_82599(struct ixgbe_hw *hw, u32 fdirctrl)
{
	fdirctrl |= IXGBE_FDIRCTRL_PERFECT_MATCH |
		    IXGBE_FDIRCTRL_REPORT_STATUS |
		    (IXGBE_FDIR_DROP_QUEUE << IXGBE_FDIRCTRL_DROP_Q_SHIFT) |
		    (0x6 << IXGBE_FDIRCTRL_FLEX_SHIFT) |
		    (0xA << IXGBE_FDIRCTRL_MAX_LENGTH_SHIFT) |
		    (4 << IXGBE_FDIRCTRL_FULL_THRESH_SHIFT);

	
	ixgbe_fdir_enable_82599(hw, fdirctrl);

	return 0;
}

#define IXGBE_ATR_COMMON_HASH_KEY \
		(IXGBE_ATR_BUCKET_HASH_KEY & IXGBE_ATR_SIGNATURE_HASH_KEY)
#define IXGBE_COMPUTE_SIG_HASH_ITERATION(_n) \
do { \
	u32 n = (_n); \
	if (IXGBE_ATR_COMMON_HASH_KEY & (0x01 << n)) \
		common_hash ^= lo_hash_dword >> n; \
	else if (IXGBE_ATR_BUCKET_HASH_KEY & (0x01 << n)) \
		bucket_hash ^= lo_hash_dword >> n; \
	else if (IXGBE_ATR_SIGNATURE_HASH_KEY & (0x01 << n)) \
		sig_hash ^= lo_hash_dword << (16 - n); \
	if (IXGBE_ATR_COMMON_HASH_KEY & (0x01 << (n + 16))) \
		common_hash ^= hi_hash_dword >> n; \
	else if (IXGBE_ATR_BUCKET_HASH_KEY & (0x01 << (n + 16))) \
		bucket_hash ^= hi_hash_dword >> n; \
	else if (IXGBE_ATR_SIGNATURE_HASH_KEY & (0x01 << (n + 16))) \
		sig_hash ^= hi_hash_dword << (16 - n); \
} while (0);

static u32 ixgbe_atr_compute_sig_hash_82599(union ixgbe_atr_hash_dword input,
					    union ixgbe_atr_hash_dword common)
{
	u32 hi_hash_dword, lo_hash_dword, flow_vm_vlan;
	u32 sig_hash = 0, bucket_hash = 0, common_hash = 0;

	
	flow_vm_vlan = ntohl(input.dword);

	
	hi_hash_dword = ntohl(common.dword);

	
	lo_hash_dword = (hi_hash_dword >> 16) | (hi_hash_dword << 16);

	
	hi_hash_dword ^= flow_vm_vlan ^ (flow_vm_vlan >> 16);

	
	IXGBE_COMPUTE_SIG_HASH_ITERATION(0);

	lo_hash_dword ^= flow_vm_vlan ^ (flow_vm_vlan << 16);

	
	IXGBE_COMPUTE_SIG_HASH_ITERATION(1);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(2);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(3);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(4);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(5);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(6);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(7);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(8);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(9);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(10);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(11);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(12);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(13);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(14);
	IXGBE_COMPUTE_SIG_HASH_ITERATION(15);

	
	bucket_hash ^= common_hash;
	bucket_hash &= IXGBE_ATR_HASH_MASK;

	sig_hash ^= common_hash << 16;
	sig_hash &= IXGBE_ATR_HASH_MASK << 16;

	
	return sig_hash ^ bucket_hash;
}

s32 ixgbe_fdir_add_signature_filter_82599(struct ixgbe_hw *hw,
                                          union ixgbe_atr_hash_dword input,
                                          union ixgbe_atr_hash_dword common,
                                          u8 queue)
{
	u64  fdirhashcmd;
	u32  fdircmd;

	switch (input.formatted.flow_type) {
	case IXGBE_ATR_FLOW_TYPE_TCPV4:
	case IXGBE_ATR_FLOW_TYPE_UDPV4:
	case IXGBE_ATR_FLOW_TYPE_SCTPV4:
	case IXGBE_ATR_FLOW_TYPE_TCPV6:
	case IXGBE_ATR_FLOW_TYPE_UDPV6:
	case IXGBE_ATR_FLOW_TYPE_SCTPV6:
		break;
	default:
		hw_dbg(hw, " Error on flow type input\n");
		return IXGBE_ERR_CONFIG;
	}

	
	fdircmd = IXGBE_FDIRCMD_CMD_ADD_FLOW | IXGBE_FDIRCMD_FILTER_UPDATE |
	          IXGBE_FDIRCMD_LAST | IXGBE_FDIRCMD_QUEUE_EN;
	fdircmd |= input.formatted.flow_type << IXGBE_FDIRCMD_FLOW_TYPE_SHIFT;
	fdircmd |= (u32)queue << IXGBE_FDIRCMD_RX_QUEUE_SHIFT;

	fdirhashcmd = (u64)fdircmd << 32;
	fdirhashcmd |= ixgbe_atr_compute_sig_hash_82599(input, common);
	IXGBE_WRITE_REG64(hw, IXGBE_FDIRHASH, fdirhashcmd);

	hw_dbg(hw, "Tx Queue=%x hash=%x\n", queue, (u32)fdirhashcmd);

	return 0;
}

#define IXGBE_COMPUTE_BKT_HASH_ITERATION(_n) \
do { \
	u32 n = (_n); \
	if (IXGBE_ATR_BUCKET_HASH_KEY & (0x01 << n)) \
		bucket_hash ^= lo_hash_dword >> n; \
	if (IXGBE_ATR_BUCKET_HASH_KEY & (0x01 << (n + 16))) \
		bucket_hash ^= hi_hash_dword >> n; \
} while (0);

void ixgbe_atr_compute_perfect_hash_82599(union ixgbe_atr_input *input,
					  union ixgbe_atr_input *input_mask)
{

	u32 hi_hash_dword, lo_hash_dword, flow_vm_vlan;
	u32 bucket_hash = 0;

	
	input->dword_stream[0]  &= input_mask->dword_stream[0];
	input->dword_stream[1]  &= input_mask->dword_stream[1];
	input->dword_stream[2]  &= input_mask->dword_stream[2];
	input->dword_stream[3]  &= input_mask->dword_stream[3];
	input->dword_stream[4]  &= input_mask->dword_stream[4];
	input->dword_stream[5]  &= input_mask->dword_stream[5];
	input->dword_stream[6]  &= input_mask->dword_stream[6];
	input->dword_stream[7]  &= input_mask->dword_stream[7];
	input->dword_stream[8]  &= input_mask->dword_stream[8];
	input->dword_stream[9]  &= input_mask->dword_stream[9];
	input->dword_stream[10] &= input_mask->dword_stream[10];

	
	flow_vm_vlan = ntohl(input->dword_stream[0]);

	
	hi_hash_dword = ntohl(input->dword_stream[1] ^
				    input->dword_stream[2] ^
				    input->dword_stream[3] ^
				    input->dword_stream[4] ^
				    input->dword_stream[5] ^
				    input->dword_stream[6] ^
				    input->dword_stream[7] ^
				    input->dword_stream[8] ^
				    input->dword_stream[9] ^
				    input->dword_stream[10]);

	
	lo_hash_dword = (hi_hash_dword >> 16) | (hi_hash_dword << 16);

	
	hi_hash_dword ^= flow_vm_vlan ^ (flow_vm_vlan >> 16);

	
	IXGBE_COMPUTE_BKT_HASH_ITERATION(0);

	lo_hash_dword ^= flow_vm_vlan ^ (flow_vm_vlan << 16);

	
	IXGBE_COMPUTE_BKT_HASH_ITERATION(1);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(2);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(3);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(4);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(5);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(6);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(7);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(8);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(9);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(10);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(11);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(12);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(13);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(14);
	IXGBE_COMPUTE_BKT_HASH_ITERATION(15);

	input->formatted.bkt_hash = bucket_hash & 0x1FFF;
}

static u32 ixgbe_get_fdirtcpm_82599(union ixgbe_atr_input *input_mask)
{
	u32 mask = ntohs(input_mask->formatted.dst_port);
	mask <<= IXGBE_FDIRTCPM_DPORTM_SHIFT;
	mask |= ntohs(input_mask->formatted.src_port);
	mask = ((mask & 0x55555555) << 1) | ((mask & 0xAAAAAAAA) >> 1);
	mask = ((mask & 0x33333333) << 2) | ((mask & 0xCCCCCCCC) >> 2);
	mask = ((mask & 0x0F0F0F0F) << 4) | ((mask & 0xF0F0F0F0) >> 4);
	return ((mask & 0x00FF00FF) << 8) | ((mask & 0xFF00FF00) >> 8);
}

/*
 * These two macros are meant to address the fact that we have registers
 * that are either all or in part big-endian.  As a result on big-endian
 * systems we will end up byte swapping the value to little-endian before
 * it is byte swapped again and written to the hardware in the original
 * big-endian format.
 */
#define IXGBE_STORE_AS_BE32(_value) \
	(((u32)(_value) >> 24) | (((u32)(_value) & 0x00FF0000) >> 8) | \
	 (((u32)(_value) & 0x0000FF00) << 8) | ((u32)(_value) << 24))

#define IXGBE_WRITE_REG_BE32(a, reg, value) \
	IXGBE_WRITE_REG((a), (reg), IXGBE_STORE_AS_BE32(ntohl(value)))

#define IXGBE_STORE_AS_BE16(_value) \
	ntohs(((u16)(_value) >> 8) | ((u16)(_value) << 8))

s32 ixgbe_fdir_set_input_mask_82599(struct ixgbe_hw *hw,
				    union ixgbe_atr_input *input_mask)
{
	
	u32 fdirm = IXGBE_FDIRM_DIPv6;
	u32 fdirtcpm;


	
	if (input_mask->formatted.bkt_hash)
		hw_dbg(hw, " bucket hash should always be 0 in mask\n");

	
	switch (input_mask->formatted.vm_pool & 0x7F) {
	case 0x0:
		fdirm |= IXGBE_FDIRM_POOL;
	case 0x7F:
		break;
	default:
		hw_dbg(hw, " Error on vm pool mask\n");
		return IXGBE_ERR_CONFIG;
	}

	switch (input_mask->formatted.flow_type & IXGBE_ATR_L4TYPE_MASK) {
	case 0x0:
		fdirm |= IXGBE_FDIRM_L4P;
		if (input_mask->formatted.dst_port ||
		    input_mask->formatted.src_port) {
			hw_dbg(hw, " Error on src/dst port mask\n");
			return IXGBE_ERR_CONFIG;
		}
	case IXGBE_ATR_L4TYPE_MASK:
		break;
	default:
		hw_dbg(hw, " Error on flow type mask\n");
		return IXGBE_ERR_CONFIG;
	}

	switch (ntohs(input_mask->formatted.vlan_id) & 0xEFFF) {
	case 0x0000:
		
		fdirm |= IXGBE_FDIRM_VLANID;
	case 0x0FFF:
		
		fdirm |= IXGBE_FDIRM_VLANP;
		break;
	case 0xE000:
		
		fdirm |= IXGBE_FDIRM_VLANID;
	case 0xEFFF:
		
		break;
	default:
		hw_dbg(hw, " Error on VLAN mask\n");
		return IXGBE_ERR_CONFIG;
	}

	switch (input_mask->formatted.flex_bytes & 0xFFFF) {
	case 0x0000:
		
		fdirm |= IXGBE_FDIRM_FLEX;
	case 0xFFFF:
		break;
	default:
		hw_dbg(hw, " Error on flexible byte mask\n");
		return IXGBE_ERR_CONFIG;
	}

	
	IXGBE_WRITE_REG(hw, IXGBE_FDIRM, fdirm);

	
	fdirtcpm = ixgbe_get_fdirtcpm_82599(input_mask);

	
	IXGBE_WRITE_REG(hw, IXGBE_FDIRTCPM, ~fdirtcpm);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRUDPM, ~fdirtcpm);

	
	IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRSIP4M,
			     ~input_mask->formatted.src_ip[0]);
	IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRDIP4M,
			     ~input_mask->formatted.dst_ip[0]);

	return 0;
}

s32 ixgbe_fdir_write_perfect_filter_82599(struct ixgbe_hw *hw,
					  union ixgbe_atr_input *input,
					  u16 soft_id, u8 queue)
{
	u32 fdirport, fdirvlan, fdirhash, fdircmd;

	
	IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRSIPv6(0),
			     input->formatted.src_ip[0]);
	IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRSIPv6(1),
			     input->formatted.src_ip[1]);
	IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRSIPv6(2),
			     input->formatted.src_ip[2]);

	
	IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRIPSA, input->formatted.src_ip[0]);

	
	IXGBE_WRITE_REG_BE32(hw, IXGBE_FDIRIPDA, input->formatted.dst_ip[0]);

	
	fdirport = ntohs(input->formatted.dst_port);
	fdirport <<= IXGBE_FDIRPORT_DESTINATION_SHIFT;
	fdirport |= ntohs(input->formatted.src_port);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRPORT, fdirport);

	
	fdirvlan = IXGBE_STORE_AS_BE16(input->formatted.flex_bytes);
	fdirvlan <<= IXGBE_FDIRVLAN_FLEX_SHIFT;
	fdirvlan |= ntohs(input->formatted.vlan_id);
	IXGBE_WRITE_REG(hw, IXGBE_FDIRVLAN, fdirvlan);

	
	fdirhash = input->formatted.bkt_hash;
	fdirhash |= soft_id << IXGBE_FDIRHASH_SIG_SW_INDEX_SHIFT;
	IXGBE_WRITE_REG(hw, IXGBE_FDIRHASH, fdirhash);

	IXGBE_WRITE_FLUSH(hw);

	
	fdircmd = IXGBE_FDIRCMD_CMD_ADD_FLOW | IXGBE_FDIRCMD_FILTER_UPDATE |
		  IXGBE_FDIRCMD_LAST | IXGBE_FDIRCMD_QUEUE_EN;
	if (queue == IXGBE_FDIR_DROP_QUEUE)
		fdircmd |= IXGBE_FDIRCMD_DROP;
	fdircmd |= input->formatted.flow_type << IXGBE_FDIRCMD_FLOW_TYPE_SHIFT;
	fdircmd |= (u32)queue << IXGBE_FDIRCMD_RX_QUEUE_SHIFT;
	fdircmd |= (u32)input->formatted.vm_pool << IXGBE_FDIRCMD_VT_POOL_SHIFT;

	IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD, fdircmd);

	return 0;
}

s32 ixgbe_fdir_erase_perfect_filter_82599(struct ixgbe_hw *hw,
					  union ixgbe_atr_input *input,
					  u16 soft_id)
{
	u32 fdirhash;
	u32 fdircmd = 0;
	u32 retry_count;
	s32 err = 0;

	
	fdirhash = input->formatted.bkt_hash;
	fdirhash |= soft_id << IXGBE_FDIRHASH_SIG_SW_INDEX_SHIFT;
	IXGBE_WRITE_REG(hw, IXGBE_FDIRHASH, fdirhash);

	
	IXGBE_WRITE_FLUSH(hw);

	
	IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD, IXGBE_FDIRCMD_CMD_QUERY_REM_FILT);

	for (retry_count = 10; retry_count; retry_count--) {
		
		udelay(10);
		
		fdircmd = IXGBE_READ_REG(hw, IXGBE_FDIRCMD);
		if (!(fdircmd & IXGBE_FDIRCMD_CMD_MASK))
			break;
	}

	if (!retry_count)
		err = IXGBE_ERR_FDIR_REINIT_FAILED;

	
	if (fdircmd & IXGBE_FDIRCMD_FILTER_VALID) {
		IXGBE_WRITE_REG(hw, IXGBE_FDIRHASH, fdirhash);
		IXGBE_WRITE_FLUSH(hw);
		IXGBE_WRITE_REG(hw, IXGBE_FDIRCMD,
				IXGBE_FDIRCMD_CMD_REMOVE_FLOW);
	}

	return err;
}

static s32 ixgbe_read_analog_reg8_82599(struct ixgbe_hw *hw, u32 reg, u8 *val)
{
	u32  core_ctl;

	IXGBE_WRITE_REG(hw, IXGBE_CORECTL, IXGBE_CORECTL_WRITE_CMD |
	                (reg << 8));
	IXGBE_WRITE_FLUSH(hw);
	udelay(10);
	core_ctl = IXGBE_READ_REG(hw, IXGBE_CORECTL);
	*val = (u8)core_ctl;

	return 0;
}

static s32 ixgbe_write_analog_reg8_82599(struct ixgbe_hw *hw, u32 reg, u8 val)
{
	u32  core_ctl;

	core_ctl = (reg << 8) | val;
	IXGBE_WRITE_REG(hw, IXGBE_CORECTL, core_ctl);
	IXGBE_WRITE_FLUSH(hw);
	udelay(10);

	return 0;
}

static s32 ixgbe_start_hw_82599(struct ixgbe_hw *hw)
{
	s32 ret_val = 0;

	ret_val = ixgbe_start_hw_generic(hw);
	if (ret_val != 0)
		goto out;

	ret_val = ixgbe_start_hw_gen2(hw);
	if (ret_val != 0)
		goto out;

	
	hw->mac.autotry_restart = true;
	hw->mac.rx_pb_size = IXGBE_82599_RX_PB_SIZE;

	if (ret_val == 0)
		ret_val = ixgbe_verify_fw_version_82599(hw);
out:
	return ret_val;
}

static s32 ixgbe_identify_phy_82599(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_ERR_PHY_ADDR_INVALID;

	
	status = ixgbe_identify_phy_generic(hw);
	if (status != 0) {
		
		if (hw->mac.ops.get_media_type(hw) == ixgbe_media_type_copper)
			goto out;
		else
			status = ixgbe_identify_sfp_module_generic(hw);
	}

	
	if (hw->phy.type == ixgbe_phy_unknown) {
		hw->phy.type = ixgbe_phy_none;
		status = 0;
	}

	
	if (hw->phy.type == ixgbe_phy_sfp_unsupported)
		status = IXGBE_ERR_SFP_NOT_SUPPORTED;

out:
	return status;
}

static u32 ixgbe_get_supported_physical_layer_82599(struct ixgbe_hw *hw)
{
	u32 physical_layer = IXGBE_PHYSICAL_LAYER_UNKNOWN;
	u32 autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	u32 autoc2 = IXGBE_READ_REG(hw, IXGBE_AUTOC2);
	u32 pma_pmd_10g_serial = autoc2 & IXGBE_AUTOC2_10G_SERIAL_PMA_PMD_MASK;
	u32 pma_pmd_10g_parallel = autoc & IXGBE_AUTOC_10G_PMA_PMD_MASK;
	u32 pma_pmd_1g = autoc & IXGBE_AUTOC_1G_PMA_PMD_MASK;
	u16 ext_ability = 0;
	u8 comp_codes_10g = 0;
	u8 comp_codes_1g = 0;

	hw->phy.ops.identify(hw);

	switch (hw->phy.type) {
	case ixgbe_phy_tn:
	case ixgbe_phy_cu_unknown:
		hw->phy.ops.read_reg(hw, MDIO_PMA_EXTABLE, MDIO_MMD_PMAPMD,
							 &ext_ability);
		if (ext_ability & MDIO_PMA_EXTABLE_10GBT)
			physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_T;
		if (ext_ability & MDIO_PMA_EXTABLE_1000BT)
			physical_layer |= IXGBE_PHYSICAL_LAYER_1000BASE_T;
		if (ext_ability & MDIO_PMA_EXTABLE_100BTX)
			physical_layer |= IXGBE_PHYSICAL_LAYER_100BASE_TX;
		goto out;
	default:
		break;
	}

	switch (autoc & IXGBE_AUTOC_LMS_MASK) {
	case IXGBE_AUTOC_LMS_1G_AN:
	case IXGBE_AUTOC_LMS_1G_LINK_NO_AN:
		if (pma_pmd_1g == IXGBE_AUTOC_1G_KX_BX) {
			physical_layer = IXGBE_PHYSICAL_LAYER_1000BASE_KX |
			    IXGBE_PHYSICAL_LAYER_1000BASE_BX;
			goto out;
		} else
			
			goto sfp_check;
		break;
	case IXGBE_AUTOC_LMS_10G_LINK_NO_AN:
		if (pma_pmd_10g_parallel == IXGBE_AUTOC_10G_CX4)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_CX4;
		else if (pma_pmd_10g_parallel == IXGBE_AUTOC_10G_KX4)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_KX4;
		else if (pma_pmd_10g_parallel == IXGBE_AUTOC_10G_XAUI)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_XAUI;
		goto out;
		break;
	case IXGBE_AUTOC_LMS_10G_SERIAL:
		if (pma_pmd_10g_serial == IXGBE_AUTOC2_10G_KR) {
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_KR;
			goto out;
		} else if (pma_pmd_10g_serial == IXGBE_AUTOC2_10G_SFI)
			goto sfp_check;
		break;
	case IXGBE_AUTOC_LMS_KX4_KX_KR:
	case IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN:
		if (autoc & IXGBE_AUTOC_KX_SUPP)
			physical_layer |= IXGBE_PHYSICAL_LAYER_1000BASE_KX;
		if (autoc & IXGBE_AUTOC_KX4_SUPP)
			physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_KX4;
		if (autoc & IXGBE_AUTOC_KR_SUPP)
			physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_KR;
		goto out;
		break;
	default:
		goto out;
		break;
	}

sfp_check:
	hw->phy.ops.identify_sfp(hw);
	if (hw->phy.sfp_type == ixgbe_sfp_type_not_present)
		goto out;

	switch (hw->phy.type) {
	case ixgbe_phy_sfp_passive_tyco:
	case ixgbe_phy_sfp_passive_unknown:
		physical_layer = IXGBE_PHYSICAL_LAYER_SFP_PLUS_CU;
		break;
	case ixgbe_phy_sfp_ftl_active:
	case ixgbe_phy_sfp_active_unknown:
		physical_layer = IXGBE_PHYSICAL_LAYER_SFP_ACTIVE_DA;
		break;
	case ixgbe_phy_sfp_avago:
	case ixgbe_phy_sfp_ftl:
	case ixgbe_phy_sfp_intel:
	case ixgbe_phy_sfp_unknown:
		hw->phy.ops.read_i2c_eeprom(hw,
		      IXGBE_SFF_1GBE_COMP_CODES, &comp_codes_1g);
		hw->phy.ops.read_i2c_eeprom(hw,
		      IXGBE_SFF_10GBE_COMP_CODES, &comp_codes_10g);
		if (comp_codes_10g & IXGBE_SFF_10GBASESR_CAPABLE)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_SR;
		else if (comp_codes_10g & IXGBE_SFF_10GBASELR_CAPABLE)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_LR;
		else if (comp_codes_1g & IXGBE_SFF_1GBASET_CAPABLE)
			physical_layer = IXGBE_PHYSICAL_LAYER_1000BASE_T;
		break;
	default:
		break;
	}

out:
	return physical_layer;
}

static s32 ixgbe_enable_rx_dma_82599(struct ixgbe_hw *hw, u32 regval)
{
	hw->mac.ops.disable_rx_buff(hw);

	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, regval);

	hw->mac.ops.enable_rx_buff(hw);

	return 0;
}

static s32 ixgbe_verify_fw_version_82599(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_ERR_EEPROM_VERSION;
	u16 fw_offset, fw_ptp_cfg_offset;
	u16 fw_version = 0;

	
	if (hw->phy.media_type != ixgbe_media_type_fiber) {
		status = 0;
		goto fw_version_out;
	}

	
	hw->eeprom.ops.read(hw, IXGBE_FW_PTR, &fw_offset);

	if ((fw_offset == 0) || (fw_offset == 0xFFFF))
		goto fw_version_out;

	
	hw->eeprom.ops.read(hw, (fw_offset +
	                         IXGBE_FW_PASSTHROUGH_PATCH_CONFIG_PTR),
	                         &fw_ptp_cfg_offset);

	if ((fw_ptp_cfg_offset == 0) || (fw_ptp_cfg_offset == 0xFFFF))
		goto fw_version_out;

	
	hw->eeprom.ops.read(hw, (fw_ptp_cfg_offset +
	                         IXGBE_FW_PATCH_VERSION_4),
	                         &fw_version);

	if (fw_version > 0x5)
		status = 0;

fw_version_out:
	return status;
}

static bool ixgbe_verify_lesm_fw_enabled_82599(struct ixgbe_hw *hw)
{
	bool lesm_enabled = false;
	u16 fw_offset, fw_lesm_param_offset, fw_lesm_state;
	s32 status;

	
	status = hw->eeprom.ops.read(hw, IXGBE_FW_PTR, &fw_offset);

	if ((status != 0) ||
	    (fw_offset == 0) || (fw_offset == 0xFFFF))
		goto out;

	
	status = hw->eeprom.ops.read(hw, (fw_offset +
				     IXGBE_FW_LESM_PARAMETERS_PTR),
				     &fw_lesm_param_offset);

	if ((status != 0) ||
	    (fw_lesm_param_offset == 0) || (fw_lesm_param_offset == 0xFFFF))
		goto out;

	
	status = hw->eeprom.ops.read(hw, (fw_lesm_param_offset +
				     IXGBE_FW_LESM_STATE_1),
				     &fw_lesm_state);

	if ((status == 0) &&
	    (fw_lesm_state & IXGBE_FW_LESM_STATE_ENABLED))
		lesm_enabled = true;

out:
	return lesm_enabled;
}

static s32 ixgbe_read_eeprom_buffer_82599(struct ixgbe_hw *hw, u16 offset,
					  u16 words, u16 *data)
{
	struct ixgbe_eeprom_info *eeprom = &hw->eeprom;
	s32 ret_val = IXGBE_ERR_CONFIG;

	if ((eeprom->type == ixgbe_eeprom_spi) &&
	    (offset + (words - 1) <= IXGBE_EERD_MAX_ADDR))
		ret_val = ixgbe_read_eerd_buffer_generic(hw, offset, words,
							 data);
	else
		ret_val = ixgbe_read_eeprom_buffer_bit_bang_generic(hw, offset,
								    words,
								    data);

	return ret_val;
}

static s32 ixgbe_read_eeprom_82599(struct ixgbe_hw *hw,
				   u16 offset, u16 *data)
{
	struct ixgbe_eeprom_info *eeprom = &hw->eeprom;
	s32 ret_val = IXGBE_ERR_CONFIG;

	if ((eeprom->type == ixgbe_eeprom_spi) &&
	    (offset <= IXGBE_EERD_MAX_ADDR))
		ret_val = ixgbe_read_eerd_generic(hw, offset, data);
	else
		ret_val = ixgbe_read_eeprom_bit_bang_generic(hw, offset, data);

	return ret_val;
}

static struct ixgbe_mac_operations mac_ops_82599 = {
	.init_hw                = &ixgbe_init_hw_generic,
	.reset_hw               = &ixgbe_reset_hw_82599,
	.start_hw               = &ixgbe_start_hw_82599,
	.clear_hw_cntrs         = &ixgbe_clear_hw_cntrs_generic,
	.get_media_type         = &ixgbe_get_media_type_82599,
	.get_supported_physical_layer = &ixgbe_get_supported_physical_layer_82599,
	.enable_rx_dma          = &ixgbe_enable_rx_dma_82599,
	.disable_rx_buff	= &ixgbe_disable_rx_buff_generic,
	.enable_rx_buff		= &ixgbe_enable_rx_buff_generic,
	.get_mac_addr           = &ixgbe_get_mac_addr_generic,
	.get_san_mac_addr       = &ixgbe_get_san_mac_addr_generic,
	.get_device_caps        = &ixgbe_get_device_caps_generic,
	.get_wwn_prefix         = &ixgbe_get_wwn_prefix_generic,
	.stop_adapter           = &ixgbe_stop_adapter_generic,
	.get_bus_info           = &ixgbe_get_bus_info_generic,
	.set_lan_id             = &ixgbe_set_lan_id_multi_port_pcie,
	.read_analog_reg8       = &ixgbe_read_analog_reg8_82599,
	.write_analog_reg8      = &ixgbe_write_analog_reg8_82599,
	.setup_link             = &ixgbe_setup_mac_link_82599,
	.set_rxpba		= &ixgbe_set_rxpba_generic,
	.check_link             = &ixgbe_check_mac_link_generic,
	.get_link_capabilities  = &ixgbe_get_link_capabilities_82599,
	.led_on                 = &ixgbe_led_on_generic,
	.led_off                = &ixgbe_led_off_generic,
	.blink_led_start        = &ixgbe_blink_led_start_generic,
	.blink_led_stop         = &ixgbe_blink_led_stop_generic,
	.set_rar                = &ixgbe_set_rar_generic,
	.clear_rar              = &ixgbe_clear_rar_generic,
	.set_vmdq               = &ixgbe_set_vmdq_generic,
	.clear_vmdq             = &ixgbe_clear_vmdq_generic,
	.init_rx_addrs          = &ixgbe_init_rx_addrs_generic,
	.update_mc_addr_list    = &ixgbe_update_mc_addr_list_generic,
	.enable_mc              = &ixgbe_enable_mc_generic,
	.disable_mc             = &ixgbe_disable_mc_generic,
	.clear_vfta             = &ixgbe_clear_vfta_generic,
	.set_vfta               = &ixgbe_set_vfta_generic,
	.fc_enable              = &ixgbe_fc_enable_generic,
	.set_fw_drv_ver         = &ixgbe_set_fw_drv_ver_generic,
	.init_uta_tables        = &ixgbe_init_uta_tables_generic,
	.setup_sfp              = &ixgbe_setup_sfp_modules_82599,
	.set_mac_anti_spoofing  = &ixgbe_set_mac_anti_spoofing,
	.set_vlan_anti_spoofing = &ixgbe_set_vlan_anti_spoofing,
	.acquire_swfw_sync      = &ixgbe_acquire_swfw_sync,
	.release_swfw_sync      = &ixgbe_release_swfw_sync,

};

static struct ixgbe_eeprom_operations eeprom_ops_82599 = {
	.init_params		= &ixgbe_init_eeprom_params_generic,
	.read			= &ixgbe_read_eeprom_82599,
	.read_buffer		= &ixgbe_read_eeprom_buffer_82599,
	.write			= &ixgbe_write_eeprom_generic,
	.write_buffer		= &ixgbe_write_eeprom_buffer_bit_bang_generic,
	.calc_checksum		= &ixgbe_calc_eeprom_checksum_generic,
	.validate_checksum	= &ixgbe_validate_eeprom_checksum_generic,
	.update_checksum	= &ixgbe_update_eeprom_checksum_generic,
};

static struct ixgbe_phy_operations phy_ops_82599 = {
	.identify		= &ixgbe_identify_phy_82599,
	.identify_sfp		= &ixgbe_identify_sfp_module_generic,
	.init			= &ixgbe_init_phy_ops_82599,
	.reset			= &ixgbe_reset_phy_generic,
	.read_reg		= &ixgbe_read_phy_reg_generic,
	.write_reg		= &ixgbe_write_phy_reg_generic,
	.setup_link		= &ixgbe_setup_phy_link_generic,
	.setup_link_speed	= &ixgbe_setup_phy_link_speed_generic,
	.read_i2c_byte		= &ixgbe_read_i2c_byte_generic,
	.write_i2c_byte		= &ixgbe_write_i2c_byte_generic,
	.read_i2c_eeprom	= &ixgbe_read_i2c_eeprom_generic,
	.write_i2c_eeprom	= &ixgbe_write_i2c_eeprom_generic,
	.check_overtemp		= &ixgbe_tn_check_overtemp,
};

struct ixgbe_info ixgbe_82599_info = {
	.mac                    = ixgbe_mac_82599EB,
	.get_invariants         = &ixgbe_get_invariants_82599,
	.mac_ops                = &mac_ops_82599,
	.eeprom_ops             = &eeprom_ops_82599,
	.phy_ops                = &phy_ops_82599,
	.mbx_ops                = &mbx_ops_generic,
};
