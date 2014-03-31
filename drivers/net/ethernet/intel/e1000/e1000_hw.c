/*******************************************************************************

  Intel PRO/1000 Linux driver
  Copyright(c) 1999 - 2006 Intel Corporation.

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

 */


#include "e1000.h"

static s32 e1000_check_downshift(struct e1000_hw *hw);
static s32 e1000_check_polarity(struct e1000_hw *hw,
				e1000_rev_polarity *polarity);
static void e1000_clear_hw_cntrs(struct e1000_hw *hw);
static void e1000_clear_vfta(struct e1000_hw *hw);
static s32 e1000_config_dsp_after_link_change(struct e1000_hw *hw,
					      bool link_up);
static s32 e1000_config_fc_after_link_up(struct e1000_hw *hw);
static s32 e1000_detect_gig_phy(struct e1000_hw *hw);
static s32 e1000_get_auto_rd_done(struct e1000_hw *hw);
static s32 e1000_get_cable_length(struct e1000_hw *hw, u16 *min_length,
				  u16 *max_length);
static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw);
static s32 e1000_id_led_init(struct e1000_hw *hw);
static void e1000_init_rx_addrs(struct e1000_hw *hw);
static s32 e1000_phy_igp_get_info(struct e1000_hw *hw,
				  struct e1000_phy_info *phy_info);
static s32 e1000_phy_m88_get_info(struct e1000_hw *hw,
				  struct e1000_phy_info *phy_info);
static s32 e1000_set_d3_lplu_state(struct e1000_hw *hw, bool active);
static s32 e1000_wait_autoneg(struct e1000_hw *hw);
static void e1000_write_reg_io(struct e1000_hw *hw, u32 offset, u32 value);
static s32 e1000_set_phy_type(struct e1000_hw *hw);
static void e1000_phy_init_script(struct e1000_hw *hw);
static s32 e1000_setup_copper_link(struct e1000_hw *hw);
static s32 e1000_setup_fiber_serdes_link(struct e1000_hw *hw);
static s32 e1000_adjust_serdes_amplitude(struct e1000_hw *hw);
static s32 e1000_phy_force_speed_duplex(struct e1000_hw *hw);
static s32 e1000_config_mac_to_phy(struct e1000_hw *hw);
static void e1000_raise_mdi_clk(struct e1000_hw *hw, u32 *ctrl);
static void e1000_lower_mdi_clk(struct e1000_hw *hw, u32 *ctrl);
static void e1000_shift_out_mdi_bits(struct e1000_hw *hw, u32 data, u16 count);
static u16 e1000_shift_in_mdi_bits(struct e1000_hw *hw);
static s32 e1000_phy_reset_dsp(struct e1000_hw *hw);
static s32 e1000_write_eeprom_spi(struct e1000_hw *hw, u16 offset,
				  u16 words, u16 *data);
static s32 e1000_write_eeprom_microwire(struct e1000_hw *hw, u16 offset,
					u16 words, u16 *data);
static s32 e1000_spi_eeprom_ready(struct e1000_hw *hw);
static void e1000_raise_ee_clk(struct e1000_hw *hw, u32 *eecd);
static void e1000_lower_ee_clk(struct e1000_hw *hw, u32 *eecd);
static void e1000_shift_out_ee_bits(struct e1000_hw *hw, u16 data, u16 count);
static s32 e1000_write_phy_reg_ex(struct e1000_hw *hw, u32 reg_addr,
				  u16 phy_data);
static s32 e1000_read_phy_reg_ex(struct e1000_hw *hw, u32 reg_addr,
				 u16 *phy_data);
static u16 e1000_shift_in_ee_bits(struct e1000_hw *hw, u16 count);
static s32 e1000_acquire_eeprom(struct e1000_hw *hw);
static void e1000_release_eeprom(struct e1000_hw *hw);
static void e1000_standby_eeprom(struct e1000_hw *hw);
static s32 e1000_set_vco_speed(struct e1000_hw *hw);
static s32 e1000_polarity_reversal_workaround(struct e1000_hw *hw);
static s32 e1000_set_phy_mode(struct e1000_hw *hw);
static s32 e1000_do_read_eeprom(struct e1000_hw *hw, u16 offset, u16 words,
				u16 *data);
static s32 e1000_do_write_eeprom(struct e1000_hw *hw, u16 offset, u16 words,
				 u16 *data);

static const
u16 e1000_igp_cable_length_table[IGP01E1000_AGC_LENGTH_TABLE_SIZE] = {
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 10, 10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 25, 25, 25,
	25, 25, 25, 25, 30, 30, 30, 30, 40, 40, 40, 40, 40, 40, 40, 40,
	40, 50, 50, 50, 50, 50, 50, 50, 60, 60, 60, 60, 60, 60, 60, 60,
	60, 70, 70, 70, 70, 70, 70, 80, 80, 80, 80, 80, 80, 90, 90, 90,
	90, 90, 90, 90, 90, 90, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	    100,
	100, 100, 100, 100, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,
	    110, 110,
	110, 110, 110, 110, 110, 110, 120, 120, 120, 120, 120, 120, 120, 120,
	    120, 120
};

static DEFINE_SPINLOCK(e1000_eeprom_lock);

static s32 e1000_set_phy_type(struct e1000_hw *hw)
{
	e_dbg("e1000_set_phy_type");

	if (hw->mac_type == e1000_undefined)
		return -E1000_ERR_PHY_TYPE;

	switch (hw->phy_id) {
	case M88E1000_E_PHY_ID:
	case M88E1000_I_PHY_ID:
	case M88E1011_I_PHY_ID:
	case M88E1111_I_PHY_ID:
	case M88E1118_E_PHY_ID:
		hw->phy_type = e1000_phy_m88;
		break;
	case IGP01E1000_I_PHY_ID:
		if (hw->mac_type == e1000_82541 ||
		    hw->mac_type == e1000_82541_rev_2 ||
		    hw->mac_type == e1000_82547 ||
		    hw->mac_type == e1000_82547_rev_2)
			hw->phy_type = e1000_phy_igp;
		break;
	case RTL8211B_PHY_ID:
		hw->phy_type = e1000_phy_8211;
		break;
	case RTL8201N_PHY_ID:
		hw->phy_type = e1000_phy_8201;
		break;
	default:
		
		hw->phy_type = e1000_phy_undefined;
		return -E1000_ERR_PHY_TYPE;
	}

	return E1000_SUCCESS;
}

static void e1000_phy_init_script(struct e1000_hw *hw)
{
	u32 ret_val;
	u16 phy_saved_data;

	e_dbg("e1000_phy_init_script");

	if (hw->phy_init_script) {
		msleep(20);

		ret_val = e1000_read_phy_reg(hw, 0x2F5B, &phy_saved_data);

		
		e1000_write_phy_reg(hw, 0x2F5B, 0x0003);
		msleep(20);

		e1000_write_phy_reg(hw, 0x0000, 0x0140);
		msleep(5);

		switch (hw->mac_type) {
		case e1000_82541:
		case e1000_82547:
			e1000_write_phy_reg(hw, 0x1F95, 0x0001);
			e1000_write_phy_reg(hw, 0x1F71, 0xBD21);
			e1000_write_phy_reg(hw, 0x1F79, 0x0018);
			e1000_write_phy_reg(hw, 0x1F30, 0x1600);
			e1000_write_phy_reg(hw, 0x1F31, 0x0014);
			e1000_write_phy_reg(hw, 0x1F32, 0x161C);
			e1000_write_phy_reg(hw, 0x1F94, 0x0003);
			e1000_write_phy_reg(hw, 0x1F96, 0x003F);
			e1000_write_phy_reg(hw, 0x2010, 0x0008);
			break;

		case e1000_82541_rev_2:
		case e1000_82547_rev_2:
			e1000_write_phy_reg(hw, 0x1F73, 0x0099);
			break;
		default:
			break;
		}

		e1000_write_phy_reg(hw, 0x0000, 0x3300);
		msleep(20);

		
		e1000_write_phy_reg(hw, 0x2F5B, phy_saved_data);

		if (hw->mac_type == e1000_82547) {
			u16 fused, fine, coarse;

			
			e1000_read_phy_reg(hw,
					   IGP01E1000_ANALOG_SPARE_FUSE_STATUS,
					   &fused);

			if (!(fused & IGP01E1000_ANALOG_SPARE_FUSE_ENABLED)) {
				e1000_read_phy_reg(hw,
						   IGP01E1000_ANALOG_FUSE_STATUS,
						   &fused);

				fine = fused & IGP01E1000_ANALOG_FUSE_FINE_MASK;
				coarse =
				    fused & IGP01E1000_ANALOG_FUSE_COARSE_MASK;

				if (coarse >
				    IGP01E1000_ANALOG_FUSE_COARSE_THRESH) {
					coarse -=
					    IGP01E1000_ANALOG_FUSE_COARSE_10;
					fine -= IGP01E1000_ANALOG_FUSE_FINE_1;
				} else if (coarse ==
					   IGP01E1000_ANALOG_FUSE_COARSE_THRESH)
					fine -= IGP01E1000_ANALOG_FUSE_FINE_10;

				fused =
				    (fused & IGP01E1000_ANALOG_FUSE_POLY_MASK) |
				    (fine & IGP01E1000_ANALOG_FUSE_FINE_MASK) |
				    (coarse &
				     IGP01E1000_ANALOG_FUSE_COARSE_MASK);

				e1000_write_phy_reg(hw,
						    IGP01E1000_ANALOG_FUSE_CONTROL,
						    fused);
				e1000_write_phy_reg(hw,
						    IGP01E1000_ANALOG_FUSE_BYPASS,
						    IGP01E1000_ANALOG_FUSE_ENABLE_SW_CONTROL);
			}
		}
	}
}

s32 e1000_set_mac_type(struct e1000_hw *hw)
{
	e_dbg("e1000_set_mac_type");

	switch (hw->device_id) {
	case E1000_DEV_ID_82542:
		switch (hw->revision_id) {
		case E1000_82542_2_0_REV_ID:
			hw->mac_type = e1000_82542_rev2_0;
			break;
		case E1000_82542_2_1_REV_ID:
			hw->mac_type = e1000_82542_rev2_1;
			break;
		default:
			
			return -E1000_ERR_MAC_TYPE;
		}
		break;
	case E1000_DEV_ID_82543GC_FIBER:
	case E1000_DEV_ID_82543GC_COPPER:
		hw->mac_type = e1000_82543;
		break;
	case E1000_DEV_ID_82544EI_COPPER:
	case E1000_DEV_ID_82544EI_FIBER:
	case E1000_DEV_ID_82544GC_COPPER:
	case E1000_DEV_ID_82544GC_LOM:
		hw->mac_type = e1000_82544;
		break;
	case E1000_DEV_ID_82540EM:
	case E1000_DEV_ID_82540EM_LOM:
	case E1000_DEV_ID_82540EP:
	case E1000_DEV_ID_82540EP_LOM:
	case E1000_DEV_ID_82540EP_LP:
		hw->mac_type = e1000_82540;
		break;
	case E1000_DEV_ID_82545EM_COPPER:
	case E1000_DEV_ID_82545EM_FIBER:
		hw->mac_type = e1000_82545;
		break;
	case E1000_DEV_ID_82545GM_COPPER:
	case E1000_DEV_ID_82545GM_FIBER:
	case E1000_DEV_ID_82545GM_SERDES:
		hw->mac_type = e1000_82545_rev_3;
		break;
	case E1000_DEV_ID_82546EB_COPPER:
	case E1000_DEV_ID_82546EB_FIBER:
	case E1000_DEV_ID_82546EB_QUAD_COPPER:
		hw->mac_type = e1000_82546;
		break;
	case E1000_DEV_ID_82546GB_COPPER:
	case E1000_DEV_ID_82546GB_FIBER:
	case E1000_DEV_ID_82546GB_SERDES:
	case E1000_DEV_ID_82546GB_PCIE:
	case E1000_DEV_ID_82546GB_QUAD_COPPER:
	case E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3:
		hw->mac_type = e1000_82546_rev_3;
		break;
	case E1000_DEV_ID_82541EI:
	case E1000_DEV_ID_82541EI_MOBILE:
	case E1000_DEV_ID_82541ER_LOM:
		hw->mac_type = e1000_82541;
		break;
	case E1000_DEV_ID_82541ER:
	case E1000_DEV_ID_82541GI:
	case E1000_DEV_ID_82541GI_LF:
	case E1000_DEV_ID_82541GI_MOBILE:
		hw->mac_type = e1000_82541_rev_2;
		break;
	case E1000_DEV_ID_82547EI:
	case E1000_DEV_ID_82547EI_MOBILE:
		hw->mac_type = e1000_82547;
		break;
	case E1000_DEV_ID_82547GI:
		hw->mac_type = e1000_82547_rev_2;
		break;
	case E1000_DEV_ID_INTEL_CE4100_GBE:
		hw->mac_type = e1000_ce4100;
		break;
	default:
		
		return -E1000_ERR_MAC_TYPE;
	}

	switch (hw->mac_type) {
	case e1000_82541:
	case e1000_82547:
	case e1000_82541_rev_2:
	case e1000_82547_rev_2:
		hw->asf_firmware_present = true;
		break;
	default:
		break;
	}

	if (hw->mac_type == e1000_82543)
		hw->bad_tx_carr_stats_fd = true;

	if (hw->mac_type > e1000_82544)
		hw->has_smbus = true;

	return E1000_SUCCESS;
}

void e1000_set_media_type(struct e1000_hw *hw)
{
	u32 status;

	e_dbg("e1000_set_media_type");

	if (hw->mac_type != e1000_82543) {
		
		hw->tbi_compatibility_en = false;
	}

	switch (hw->device_id) {
	case E1000_DEV_ID_82545GM_SERDES:
	case E1000_DEV_ID_82546GB_SERDES:
		hw->media_type = e1000_media_type_internal_serdes;
		break;
	default:
		switch (hw->mac_type) {
		case e1000_82542_rev2_0:
		case e1000_82542_rev2_1:
			hw->media_type = e1000_media_type_fiber;
			break;
		case e1000_ce4100:
			hw->media_type = e1000_media_type_copper;
			break;
		default:
			status = er32(STATUS);
			if (status & E1000_STATUS_TBIMODE) {
				hw->media_type = e1000_media_type_fiber;
				
				hw->tbi_compatibility_en = false;
			} else {
				hw->media_type = e1000_media_type_copper;
			}
			break;
		}
	}
}

s32 e1000_reset_hw(struct e1000_hw *hw)
{
	u32 ctrl;
	u32 ctrl_ext;
	u32 icr;
	u32 manc;
	u32 led_ctrl;
	s32 ret_val;

	e_dbg("e1000_reset_hw");

	
	if (hw->mac_type == e1000_82542_rev2_0) {
		e_dbg("Disabling MWI on 82542 rev 2.0\n");
		e1000_pci_clear_mwi(hw);
	}

	
	e_dbg("Masking off all interrupts\n");
	ew32(IMC, 0xffffffff);

	ew32(RCTL, 0);
	ew32(TCTL, E1000_TCTL_PSP);
	E1000_WRITE_FLUSH();

	
	hw->tbi_compatibility_on = false;

	msleep(10);

	ctrl = er32(CTRL);

	
	if ((hw->mac_type == e1000_82541) || (hw->mac_type == e1000_82547)) {
		ew32(CTRL, (ctrl | E1000_CTRL_PHY_RST));
		E1000_WRITE_FLUSH();
		msleep(5);
	}

	e_dbg("Issuing a global reset to MAC\n");

	switch (hw->mac_type) {
	case e1000_82544:
	case e1000_82540:
	case e1000_82545:
	case e1000_82546:
	case e1000_82541:
	case e1000_82541_rev_2:
		E1000_WRITE_REG_IO(hw, CTRL, (ctrl | E1000_CTRL_RST));
		break;
	case e1000_82545_rev_3:
	case e1000_82546_rev_3:
		
		ew32(CTRL_DUP, (ctrl | E1000_CTRL_RST));
		break;
	case e1000_ce4100:
	default:
		ew32(CTRL, (ctrl | E1000_CTRL_RST));
		break;
	}

	switch (hw->mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
	case e1000_82543:
	case e1000_82544:
		
		udelay(10);
		ctrl_ext = er32(CTRL_EXT);
		ctrl_ext |= E1000_CTRL_EXT_EE_RST;
		ew32(CTRL_EXT, ctrl_ext);
		E1000_WRITE_FLUSH();
		
		msleep(2);
		break;
	case e1000_82541:
	case e1000_82541_rev_2:
	case e1000_82547:
	case e1000_82547_rev_2:
		
		msleep(20);
		break;
	default:
		
		ret_val = e1000_get_auto_rd_done(hw);
		if (ret_val)
			return ret_val;
		break;
	}

	
	if (hw->mac_type >= e1000_82540) {
		manc = er32(MANC);
		manc &= ~(E1000_MANC_ARP_EN);
		ew32(MANC, manc);
	}

	if ((hw->mac_type == e1000_82541) || (hw->mac_type == e1000_82547)) {
		e1000_phy_init_script(hw);

		
		led_ctrl = er32(LEDCTL);
		led_ctrl &= IGP_ACTIVITY_LED_MASK;
		led_ctrl |= (IGP_ACTIVITY_LED_ENABLE | IGP_LED3_MODE);
		ew32(LEDCTL, led_ctrl);
	}

	
	e_dbg("Masking off all interrupts\n");
	ew32(IMC, 0xffffffff);

	
	icr = er32(ICR);

	
	if (hw->mac_type == e1000_82542_rev2_0) {
		if (hw->pci_cmd_word & PCI_COMMAND_INVALIDATE)
			e1000_pci_set_mwi(hw);
	}

	return E1000_SUCCESS;
}

s32 e1000_init_hw(struct e1000_hw *hw)
{
	u32 ctrl;
	u32 i;
	s32 ret_val;
	u32 mta_size;
	u32 ctrl_ext;

	e_dbg("e1000_init_hw");

	
	ret_val = e1000_id_led_init(hw);
	if (ret_val) {
		e_dbg("Error Initializing Identification LED\n");
		return ret_val;
	}

	
	e1000_set_media_type(hw);

	
	e_dbg("Initializing the IEEE VLAN\n");
	if (hw->mac_type < e1000_82545_rev_3)
		ew32(VET, 0);
	e1000_clear_vfta(hw);

	
	if (hw->mac_type == e1000_82542_rev2_0) {
		e_dbg("Disabling MWI on 82542 rev 2.0\n");
		e1000_pci_clear_mwi(hw);
		ew32(RCTL, E1000_RCTL_RST);
		E1000_WRITE_FLUSH();
		msleep(5);
	}

	e1000_init_rx_addrs(hw);

	
	if (hw->mac_type == e1000_82542_rev2_0) {
		ew32(RCTL, 0);
		E1000_WRITE_FLUSH();
		msleep(1);
		if (hw->pci_cmd_word & PCI_COMMAND_INVALIDATE)
			e1000_pci_set_mwi(hw);
	}

	
	e_dbg("Zeroing the MTA\n");
	mta_size = E1000_MC_TBL_SIZE;
	for (i = 0; i < mta_size; i++) {
		E1000_WRITE_REG_ARRAY(hw, MTA, i, 0);
		E1000_WRITE_FLUSH();
	}

	if (hw->dma_fairness && hw->mac_type <= e1000_82543) {
		ctrl = er32(CTRL);
		ew32(CTRL, ctrl | E1000_CTRL_PRIOR);
	}

	switch (hw->mac_type) {
	case e1000_82545_rev_3:
	case e1000_82546_rev_3:
		break;
	default:
		
		if (hw->bus_type == e1000_bus_type_pcix
		    && e1000_pcix_get_mmrbc(hw) > 2048)
			e1000_pcix_set_mmrbc(hw, 2048);
		break;
	}

	
	ret_val = e1000_setup_link(hw);

	
	if (hw->mac_type > e1000_82544) {
		ctrl = er32(TXDCTL);
		ctrl =
		    (ctrl & ~E1000_TXDCTL_WTHRESH) |
		    E1000_TXDCTL_FULL_TX_DESC_WB;
		ew32(TXDCTL, ctrl);
	}

	e1000_clear_hw_cntrs(hw);

	if (hw->device_id == E1000_DEV_ID_82546GB_QUAD_COPPER ||
	    hw->device_id == E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3) {
		ctrl_ext = er32(CTRL_EXT);
		ctrl_ext |= E1000_CTRL_EXT_RO_DIS;
		ew32(CTRL_EXT, ctrl_ext);
	}

	return ret_val;
}

static s32 e1000_adjust_serdes_amplitude(struct e1000_hw *hw)
{
	u16 eeprom_data;
	s32 ret_val;

	e_dbg("e1000_adjust_serdes_amplitude");

	if (hw->media_type != e1000_media_type_internal_serdes)
		return E1000_SUCCESS;

	switch (hw->mac_type) {
	case e1000_82545_rev_3:
	case e1000_82546_rev_3:
		break;
	default:
		return E1000_SUCCESS;
	}

	ret_val = e1000_read_eeprom(hw, EEPROM_SERDES_AMPLITUDE, 1,
	                            &eeprom_data);
	if (ret_val) {
		return ret_val;
	}

	if (eeprom_data != EEPROM_RESERVED_WORD) {
		
		eeprom_data &= EEPROM_SERDES_AMPLITUDE_MASK;
		ret_val =
		    e1000_write_phy_reg(hw, M88E1000_PHY_EXT_CTRL, eeprom_data);
		if (ret_val)
			return ret_val;
	}

	return E1000_SUCCESS;
}

s32 e1000_setup_link(struct e1000_hw *hw)
{
	u32 ctrl_ext;
	s32 ret_val;
	u16 eeprom_data;

	e_dbg("e1000_setup_link");

	if (hw->fc == E1000_FC_DEFAULT) {
		ret_val = e1000_read_eeprom(hw, EEPROM_INIT_CONTROL2_REG,
					    1, &eeprom_data);
		if (ret_val) {
			e_dbg("EEPROM Read Error\n");
			return -E1000_ERR_EEPROM;
		}
		if ((eeprom_data & EEPROM_WORD0F_PAUSE_MASK) == 0)
			hw->fc = E1000_FC_NONE;
		else if ((eeprom_data & EEPROM_WORD0F_PAUSE_MASK) ==
			 EEPROM_WORD0F_ASM_DIR)
			hw->fc = E1000_FC_TX_PAUSE;
		else
			hw->fc = E1000_FC_FULL;
	}

	if (hw->mac_type == e1000_82542_rev2_0)
		hw->fc &= (~E1000_FC_TX_PAUSE);

	if ((hw->mac_type < e1000_82543) && (hw->report_tx_early == 1))
		hw->fc &= (~E1000_FC_RX_PAUSE);

	hw->original_fc = hw->fc;

	e_dbg("After fix-ups FlowControl is now = %x\n", hw->fc);

	if (hw->mac_type == e1000_82543) {
		ret_val = e1000_read_eeprom(hw, EEPROM_INIT_CONTROL2_REG,
					    1, &eeprom_data);
		if (ret_val) {
			e_dbg("EEPROM Read Error\n");
			return -E1000_ERR_EEPROM;
		}
		ctrl_ext = ((eeprom_data & EEPROM_WORD0F_SWPDIO_EXT) <<
			    SWDPIO__EXT_SHIFT);
		ew32(CTRL_EXT, ctrl_ext);
	}

	
	ret_val = (hw->media_type == e1000_media_type_copper) ?
	    e1000_setup_copper_link(hw) : e1000_setup_fiber_serdes_link(hw);

	e_dbg("Initializing the Flow Control address, type and timer regs\n");

	ew32(FCT, FLOW_CONTROL_TYPE);
	ew32(FCAH, FLOW_CONTROL_ADDRESS_HIGH);
	ew32(FCAL, FLOW_CONTROL_ADDRESS_LOW);

	ew32(FCTTV, hw->fc_pause_time);

	if (!(hw->fc & E1000_FC_TX_PAUSE)) {
		ew32(FCRTL, 0);
		ew32(FCRTH, 0);
	} else {
		if (hw->fc_send_xon) {
			ew32(FCRTL, (hw->fc_low_water | E1000_FCRTL_XONE));
			ew32(FCRTH, hw->fc_high_water);
		} else {
			ew32(FCRTL, hw->fc_low_water);
			ew32(FCRTH, hw->fc_high_water);
		}
	}
	return ret_val;
}

static s32 e1000_setup_fiber_serdes_link(struct e1000_hw *hw)
{
	u32 ctrl;
	u32 status;
	u32 txcw = 0;
	u32 i;
	u32 signal = 0;
	s32 ret_val;

	e_dbg("e1000_setup_fiber_serdes_link");

	ctrl = er32(CTRL);
	if (hw->media_type == e1000_media_type_fiber)
		signal = (hw->mac_type > e1000_82544) ? E1000_CTRL_SWDPIN1 : 0;

	ret_val = e1000_adjust_serdes_amplitude(hw);
	if (ret_val)
		return ret_val;

	
	ctrl &= ~(E1000_CTRL_LRST);

	
	ret_val = e1000_set_vco_speed(hw);
	if (ret_val)
		return ret_val;

	e1000_config_collision_dist(hw);

	switch (hw->fc) {
	case E1000_FC_NONE:
		
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD);
		break;
	case E1000_FC_RX_PAUSE:
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	case E1000_FC_TX_PAUSE:
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_ASM_DIR);
		break;
	case E1000_FC_FULL:
		
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	default:
		e_dbg("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
		break;
	}

	e_dbg("Auto-negotiation enabled\n");

	ew32(TXCW, txcw);
	ew32(CTRL, ctrl);
	E1000_WRITE_FLUSH();

	hw->txcw = txcw;
	msleep(1);

	if (hw->media_type == e1000_media_type_internal_serdes ||
	    (er32(CTRL) & E1000_CTRL_SWDPIN1) == signal) {
		e_dbg("Looking for Link\n");
		for (i = 0; i < (LINK_UP_TIMEOUT / 10); i++) {
			msleep(10);
			status = er32(STATUS);
			if (status & E1000_STATUS_LU)
				break;
		}
		if (i == (LINK_UP_TIMEOUT / 10)) {
			e_dbg("Never got a valid link from auto-neg!!!\n");
			hw->autoneg_failed = 1;
			ret_val = e1000_check_for_link(hw);
			if (ret_val) {
				e_dbg("Error while checking for link\n");
				return ret_val;
			}
			hw->autoneg_failed = 0;
		} else {
			hw->autoneg_failed = 0;
			e_dbg("Valid Link Found\n");
		}
	} else {
		e_dbg("No Signal Detected\n");
	}
	return E1000_SUCCESS;
}

static s32 e1000_copper_link_rtl_setup(struct e1000_hw *hw)
{
	s32 ret_val;

	
	ret_val = e1000_phy_reset(hw);
	if (ret_val) {
		e_dbg("Error Resetting the PHY\n");
		return ret_val;
	}

	return E1000_SUCCESS;
}

static s32 gbe_dhg_phy_setup(struct e1000_hw *hw)
{
	s32 ret_val;
	u32 ctrl_aux;

	switch (hw->phy_type) {
	case e1000_phy_8211:
		ret_val = e1000_copper_link_rtl_setup(hw);
		if (ret_val) {
			e_dbg("e1000_copper_link_rtl_setup failed!\n");
			return ret_val;
		}
		break;
	case e1000_phy_8201:
		
		ctrl_aux = er32(CTL_AUX);
		ctrl_aux |= E1000_CTL_AUX_RMII;
		ew32(CTL_AUX, ctrl_aux);
		E1000_WRITE_FLUSH();

		
		ctrl_aux = er32(CTL_AUX);
		ctrl_aux |= 0x4;
		ctrl_aux &= ~0x2;
		ew32(CTL_AUX, ctrl_aux);
		E1000_WRITE_FLUSH();
		ret_val = e1000_copper_link_rtl_setup(hw);

		if (ret_val) {
			e_dbg("e1000_copper_link_rtl_setup failed!\n");
			return ret_val;
		}
		break;
	default:
		e_dbg("Error Resetting the PHY\n");
		return E1000_ERR_PHY_TYPE;
	}

	return E1000_SUCCESS;
}

static s32 e1000_copper_link_preconfig(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;
	u16 phy_data;

	e_dbg("e1000_copper_link_preconfig");

	ctrl = er32(CTRL);
	if (hw->mac_type > e1000_82543) {
		ctrl |= E1000_CTRL_SLU;
		ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
		ew32(CTRL, ctrl);
	} else {
		ctrl |=
		    (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX | E1000_CTRL_SLU);
		ew32(CTRL, ctrl);
		ret_val = e1000_phy_hw_reset(hw);
		if (ret_val)
			return ret_val;
	}

	
	ret_val = e1000_detect_gig_phy(hw);
	if (ret_val) {
		e_dbg("Error, did not detect valid phy.\n");
		return ret_val;
	}
	e_dbg("Phy ID = %x\n", hw->phy_id);

	
	ret_val = e1000_set_phy_mode(hw);
	if (ret_val)
		return ret_val;

	if ((hw->mac_type == e1000_82545_rev_3) ||
	    (hw->mac_type == e1000_82546_rev_3)) {
		ret_val =
		    e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
		phy_data |= 0x00000008;
		ret_val =
		    e1000_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
	}

	if (hw->mac_type <= e1000_82543 ||
	    hw->mac_type == e1000_82541 || hw->mac_type == e1000_82547 ||
	    hw->mac_type == e1000_82541_rev_2
	    || hw->mac_type == e1000_82547_rev_2)
		hw->phy_reset_disable = false;

	return E1000_SUCCESS;
}

static s32 e1000_copper_link_igp_setup(struct e1000_hw *hw)
{
	u32 led_ctrl;
	s32 ret_val;
	u16 phy_data;

	e_dbg("e1000_copper_link_igp_setup");

	if (hw->phy_reset_disable)
		return E1000_SUCCESS;

	ret_val = e1000_phy_reset(hw);
	if (ret_val) {
		e_dbg("Error Resetting the PHY\n");
		return ret_val;
	}

	
	msleep(15);
	
	led_ctrl = er32(LEDCTL);
	led_ctrl &= IGP_ACTIVITY_LED_MASK;
	led_ctrl |= (IGP_ACTIVITY_LED_ENABLE | IGP_LED3_MODE);
	ew32(LEDCTL, led_ctrl);

	
	if (hw->phy_type == e1000_phy_igp) {
		
		ret_val = e1000_set_d3_lplu_state(hw, false);
		if (ret_val) {
			e_dbg("Error Disabling LPLU D3\n");
			return ret_val;
		}
	}

	
	ret_val = e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	if ((hw->mac_type == e1000_82541) || (hw->mac_type == e1000_82547)) {
		hw->dsp_config_state = e1000_dsp_config_disabled;
		
		phy_data &=
		    ~(IGP01E1000_PSCR_AUTO_MDIX |
		      IGP01E1000_PSCR_FORCE_MDI_MDIX);
		hw->mdix = 1;

	} else {
		hw->dsp_config_state = e1000_dsp_config_enabled;
		phy_data &= ~IGP01E1000_PSCR_AUTO_MDIX;

		switch (hw->mdix) {
		case 1:
			phy_data &= ~IGP01E1000_PSCR_FORCE_MDI_MDIX;
			break;
		case 2:
			phy_data |= IGP01E1000_PSCR_FORCE_MDI_MDIX;
			break;
		case 0:
		default:
			phy_data |= IGP01E1000_PSCR_AUTO_MDIX;
			break;
		}
	}
	ret_val = e1000_write_phy_reg(hw, IGP01E1000_PHY_PORT_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	
	if (hw->autoneg) {
		e1000_ms_type phy_ms_setting = hw->master_slave;

		if (hw->ffe_config_state == e1000_ffe_config_active)
			hw->ffe_config_state = e1000_ffe_config_enabled;

		if (hw->dsp_config_state == e1000_dsp_config_activated)
			hw->dsp_config_state = e1000_dsp_config_enabled;

		if (hw->autoneg_advertised == ADVERTISE_1000_FULL) {
			
			ret_val =
			    e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
					       &phy_data);
			if (ret_val)
				return ret_val;
			phy_data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val =
			    e1000_write_phy_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
						phy_data);
			if (ret_val)
				return ret_val;
			
			ret_val =
			    e1000_read_phy_reg(hw, PHY_1000T_CTRL, &phy_data);
			if (ret_val)
				return ret_val;
			phy_data &= ~CR_1000T_MS_ENABLE;
			ret_val =
			    e1000_write_phy_reg(hw, PHY_1000T_CTRL, phy_data);
			if (ret_val)
				return ret_val;
		}

		ret_val = e1000_read_phy_reg(hw, PHY_1000T_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		
		hw->original_master_slave = (phy_data & CR_1000T_MS_ENABLE) ?
		    ((phy_data & CR_1000T_MS_VALUE) ?
		     e1000_ms_force_master :
		     e1000_ms_force_slave) : e1000_ms_auto;

		switch (phy_ms_setting) {
		case e1000_ms_force_master:
			phy_data |= (CR_1000T_MS_ENABLE | CR_1000T_MS_VALUE);
			break;
		case e1000_ms_force_slave:
			phy_data |= CR_1000T_MS_ENABLE;
			phy_data &= ~(CR_1000T_MS_VALUE);
			break;
		case e1000_ms_auto:
			phy_data &= ~CR_1000T_MS_ENABLE;
		default:
			break;
		}
		ret_val = e1000_write_phy_reg(hw, PHY_1000T_CTRL, phy_data);
		if (ret_val)
			return ret_val;
	}

	return E1000_SUCCESS;
}

static s32 e1000_copper_link_mgp_setup(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_data;

	e_dbg("e1000_copper_link_mgp_setup");

	if (hw->phy_reset_disable)
		return E1000_SUCCESS;

	
	ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;

	phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;

	switch (hw->mdix) {
	case 1:
		phy_data |= M88E1000_PSCR_MDI_MANUAL_MODE;
		break;
	case 2:
		phy_data |= M88E1000_PSCR_MDIX_MANUAL_MODE;
		break;
	case 3:
		phy_data |= M88E1000_PSCR_AUTO_X_1000T;
		break;
	case 0:
	default:
		phy_data |= M88E1000_PSCR_AUTO_X_MODE;
		break;
	}

	phy_data &= ~M88E1000_PSCR_POLARITY_REVERSAL;
	if (hw->disable_polarity_correction == 1)
		phy_data |= M88E1000_PSCR_POLARITY_REVERSAL;
	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	if (hw->phy_revision < M88E1011_I_REV_4) {
		ret_val =
		    e1000_read_phy_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL,
				       &phy_data);
		if (ret_val)
			return ret_val;

		phy_data |= M88E1000_EPSCR_TX_CLK_25;

		if ((hw->phy_revision == E1000_REVISION_2) &&
		    (hw->phy_id == M88E1111_I_PHY_ID)) {
			
			phy_data &= ~(M88EC018_EPSCR_DOWNSHIFT_COUNTER_MASK);
			phy_data |= M88EC018_EPSCR_DOWNSHIFT_COUNTER_5X;
			ret_val = e1000_write_phy_reg(hw,
						      M88E1000_EXT_PHY_SPEC_CTRL,
						      phy_data);
			if (ret_val)
				return ret_val;
		} else {
			
			phy_data &= ~(M88E1000_EPSCR_MASTER_DOWNSHIFT_MASK |
				      M88E1000_EPSCR_SLAVE_DOWNSHIFT_MASK);
			phy_data |= (M88E1000_EPSCR_MASTER_DOWNSHIFT_1X |
				     M88E1000_EPSCR_SLAVE_DOWNSHIFT_1X);
			ret_val = e1000_write_phy_reg(hw,
						      M88E1000_EXT_PHY_SPEC_CTRL,
						      phy_data);
			if (ret_val)
				return ret_val;
		}
	}

	
	ret_val = e1000_phy_reset(hw);
	if (ret_val) {
		e_dbg("Error Resetting the PHY\n");
		return ret_val;
	}

	return E1000_SUCCESS;
}

static s32 e1000_copper_link_autoneg(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_data;

	e_dbg("e1000_copper_link_autoneg");

	hw->autoneg_advertised &= AUTONEG_ADVERTISE_SPEED_DEFAULT;

	if (hw->autoneg_advertised == 0)
		hw->autoneg_advertised = AUTONEG_ADVERTISE_SPEED_DEFAULT;

	
	if (hw->phy_type == e1000_phy_8201)
		hw->autoneg_advertised &= AUTONEG_ADVERTISE_10_100_ALL;

	e_dbg("Reconfiguring auto-neg advertisement params\n");
	ret_val = e1000_phy_setup_autoneg(hw);
	if (ret_val) {
		e_dbg("Error Setting up Auto-Negotiation\n");
		return ret_val;
	}
	e_dbg("Restarting Auto-Neg\n");

	ret_val = e1000_read_phy_reg(hw, PHY_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data |= (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);
	ret_val = e1000_write_phy_reg(hw, PHY_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	if (hw->wait_autoneg_complete) {
		ret_val = e1000_wait_autoneg(hw);
		if (ret_val) {
			e_dbg
			    ("Error while waiting for autoneg to complete\n");
			return ret_val;
		}
	}

	hw->get_link_status = true;

	return E1000_SUCCESS;
}

static s32 e1000_copper_link_postconfig(struct e1000_hw *hw)
{
	s32 ret_val;
	e_dbg("e1000_copper_link_postconfig");

	if ((hw->mac_type >= e1000_82544) && (hw->mac_type != e1000_ce4100)) {
		e1000_config_collision_dist(hw);
	} else {
		ret_val = e1000_config_mac_to_phy(hw);
		if (ret_val) {
			e_dbg("Error configuring MAC to PHY settings\n");
			return ret_val;
		}
	}
	ret_val = e1000_config_fc_after_link_up(hw);
	if (ret_val) {
		e_dbg("Error Configuring Flow Control\n");
		return ret_val;
	}

	
	if (hw->phy_type == e1000_phy_igp) {
		ret_val = e1000_config_dsp_after_link_change(hw, true);
		if (ret_val) {
			e_dbg("Error Configuring DSP after link up\n");
			return ret_val;
		}
	}

	return E1000_SUCCESS;
}

static s32 e1000_setup_copper_link(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 i;
	u16 phy_data;

	e_dbg("e1000_setup_copper_link");

	
	ret_val = e1000_copper_link_preconfig(hw);
	if (ret_val)
		return ret_val;

	if (hw->phy_type == e1000_phy_igp) {
		ret_val = e1000_copper_link_igp_setup(hw);
		if (ret_val)
			return ret_val;
	} else if (hw->phy_type == e1000_phy_m88) {
		ret_val = e1000_copper_link_mgp_setup(hw);
		if (ret_val)
			return ret_val;
	} else {
		ret_val = gbe_dhg_phy_setup(hw);
		if (ret_val) {
			e_dbg("gbe_dhg_phy_setup failed!\n");
			return ret_val;
		}
	}

	if (hw->autoneg) {
		ret_val = e1000_copper_link_autoneg(hw);
		if (ret_val)
			return ret_val;
	} else {
		e_dbg("Forcing speed and duplex\n");
		ret_val = e1000_phy_force_speed_duplex(hw);
		if (ret_val) {
			e_dbg("Error Forcing Speed and Duplex\n");
			return ret_val;
		}
	}

	for (i = 0; i < 10; i++) {
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;

		if (phy_data & MII_SR_LINK_STATUS) {
			
			ret_val = e1000_copper_link_postconfig(hw);
			if (ret_val)
				return ret_val;

			e_dbg("Valid link established!!!\n");
			return E1000_SUCCESS;
		}
		udelay(10);
	}

	e_dbg("Unable to establish link!!!\n");
	return E1000_SUCCESS;
}

s32 e1000_phy_setup_autoneg(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 mii_autoneg_adv_reg;
	u16 mii_1000t_ctrl_reg;

	e_dbg("e1000_phy_setup_autoneg");

	
	ret_val = e1000_read_phy_reg(hw, PHY_AUTONEG_ADV, &mii_autoneg_adv_reg);
	if (ret_val)
		return ret_val;

	
	ret_val = e1000_read_phy_reg(hw, PHY_1000T_CTRL, &mii_1000t_ctrl_reg);
	if (ret_val)
		return ret_val;
	else if (hw->phy_type == e1000_phy_8201)
		mii_1000t_ctrl_reg &= ~REG9_SPEED_MASK;


	mii_autoneg_adv_reg &= ~REG4_SPEED_MASK;
	mii_1000t_ctrl_reg &= ~REG9_SPEED_MASK;

	e_dbg("autoneg_advertised %x\n", hw->autoneg_advertised);

	
	if (hw->autoneg_advertised & ADVERTISE_10_HALF) {
		e_dbg("Advertise 10mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_HD_CAPS;
	}

	
	if (hw->autoneg_advertised & ADVERTISE_10_FULL) {
		e_dbg("Advertise 10mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_FD_CAPS;
	}

	
	if (hw->autoneg_advertised & ADVERTISE_100_HALF) {
		e_dbg("Advertise 100mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_HD_CAPS;
	}

	
	if (hw->autoneg_advertised & ADVERTISE_100_FULL) {
		e_dbg("Advertise 100mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_FD_CAPS;
	}

	
	if (hw->autoneg_advertised & ADVERTISE_1000_HALF) {
		e_dbg
		    ("Advertise 1000mb Half duplex requested, request denied!\n");
	}

	
	if (hw->autoneg_advertised & ADVERTISE_1000_FULL) {
		e_dbg("Advertise 1000mb Full duplex\n");
		mii_1000t_ctrl_reg |= CR_1000T_FD_CAPS;
	}

	switch (hw->fc) {
	case E1000_FC_NONE:	
		mii_autoneg_adv_reg &= ~(NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case E1000_FC_RX_PAUSE:	
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case E1000_FC_TX_PAUSE:	
		mii_autoneg_adv_reg |= NWAY_AR_ASM_DIR;
		mii_autoneg_adv_reg &= ~NWAY_AR_PAUSE;
		break;
	case E1000_FC_FULL:	
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	default:
		e_dbg("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
	}

	ret_val = e1000_write_phy_reg(hw, PHY_AUTONEG_ADV, mii_autoneg_adv_reg);
	if (ret_val)
		return ret_val;

	e_dbg("Auto-Neg Advertising %x\n", mii_autoneg_adv_reg);

	if (hw->phy_type == e1000_phy_8201) {
		mii_1000t_ctrl_reg = 0;
	} else {
		ret_val = e1000_write_phy_reg(hw, PHY_1000T_CTRL,
		                              mii_1000t_ctrl_reg);
		if (ret_val)
			return ret_val;
	}

	return E1000_SUCCESS;
}

static s32 e1000_phy_force_speed_duplex(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;
	u16 mii_ctrl_reg;
	u16 mii_status_reg;
	u16 phy_data;
	u16 i;

	e_dbg("e1000_phy_force_speed_duplex");

	
	hw->fc = E1000_FC_NONE;

	e_dbg("hw->fc = %d\n", hw->fc);

	
	ctrl = er32(CTRL);

	
	ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	ctrl &= ~(DEVICE_SPEED_MASK);

	
	ctrl &= ~E1000_CTRL_ASDE;

	
	ret_val = e1000_read_phy_reg(hw, PHY_CTRL, &mii_ctrl_reg);
	if (ret_val)
		return ret_val;

	

	mii_ctrl_reg &= ~MII_CR_AUTO_NEG_EN;

	
	if (hw->forced_speed_duplex == e1000_100_full ||
	    hw->forced_speed_duplex == e1000_10_full) {
		ctrl |= E1000_CTRL_FD;
		mii_ctrl_reg |= MII_CR_FULL_DUPLEX;
		e_dbg("Full Duplex\n");
	} else {
		ctrl &= ~E1000_CTRL_FD;
		mii_ctrl_reg &= ~MII_CR_FULL_DUPLEX;
		e_dbg("Half Duplex\n");
	}

	
	if (hw->forced_speed_duplex == e1000_100_full ||
	    hw->forced_speed_duplex == e1000_100_half) {
		
		ctrl |= E1000_CTRL_SPD_100;
		mii_ctrl_reg |= MII_CR_SPEED_100;
		mii_ctrl_reg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_10);
		e_dbg("Forcing 100mb ");
	} else {
		
		ctrl &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
		mii_ctrl_reg |= MII_CR_SPEED_10;
		mii_ctrl_reg &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_100);
		e_dbg("Forcing 10mb ");
	}

	e1000_config_collision_dist(hw);

	
	ew32(CTRL, ctrl);

	if (hw->phy_type == e1000_phy_m88) {
		ret_val =
		    e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;
		ret_val =
		    e1000_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
		if (ret_val)
			return ret_val;

		e_dbg("M88E1000 PSCR: %x\n", phy_data);

		
		mii_ctrl_reg |= MII_CR_RESET;

		
	} else {
		ret_val =
		    e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		phy_data &= ~IGP01E1000_PSCR_AUTO_MDIX;
		phy_data &= ~IGP01E1000_PSCR_FORCE_MDI_MDIX;

		ret_val =
		    e1000_write_phy_reg(hw, IGP01E1000_PHY_PORT_CTRL, phy_data);
		if (ret_val)
			return ret_val;
	}

	
	ret_val = e1000_write_phy_reg(hw, PHY_CTRL, mii_ctrl_reg);
	if (ret_val)
		return ret_val;

	udelay(1);

	if (hw->wait_autoneg_complete) {
		
		e_dbg("Waiting for forced speed/duplex link.\n");
		mii_status_reg = 0;

		
		for (i = PHY_FORCE_TIME; i > 0; i--) {
			ret_val =
			    e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
			if (ret_val)
				return ret_val;

			ret_val =
			    e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
			if (ret_val)
				return ret_val;

			if (mii_status_reg & MII_SR_LINK_STATUS)
				break;
			msleep(100);
		}
		if ((i == 0) && (hw->phy_type == e1000_phy_m88)) {
			
			ret_val = e1000_phy_reset_dsp(hw);
			if (ret_val) {
				e_dbg("Error Resetting PHY DSP\n");
				return ret_val;
			}
		}
		
		for (i = PHY_FORCE_TIME; i > 0; i--) {
			if (mii_status_reg & MII_SR_LINK_STATUS)
				break;
			msleep(100);
			ret_val =
			    e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
			if (ret_val)
				return ret_val;

			ret_val =
			    e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
			if (ret_val)
				return ret_val;
		}
	}

	if (hw->phy_type == e1000_phy_m88) {
		ret_val =
		    e1000_read_phy_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL,
				       &phy_data);
		if (ret_val)
			return ret_val;

		phy_data |= M88E1000_EPSCR_TX_CLK_25;
		ret_val =
		    e1000_write_phy_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL,
					phy_data);
		if (ret_val)
			return ret_val;

		ret_val =
		    e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;
		ret_val =
		    e1000_write_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
		if (ret_val)
			return ret_val;

		if ((hw->mac_type == e1000_82544 || hw->mac_type == e1000_82543)
		    && (!hw->autoneg)
		    && (hw->forced_speed_duplex == e1000_10_full
			|| hw->forced_speed_duplex == e1000_10_half)) {
			ret_val = e1000_polarity_reversal_workaround(hw);
			if (ret_val)
				return ret_val;
		}
	}
	return E1000_SUCCESS;
}

void e1000_config_collision_dist(struct e1000_hw *hw)
{
	u32 tctl, coll_dist;

	e_dbg("e1000_config_collision_dist");

	if (hw->mac_type < e1000_82543)
		coll_dist = E1000_COLLISION_DISTANCE_82542;
	else
		coll_dist = E1000_COLLISION_DISTANCE;

	tctl = er32(TCTL);

	tctl &= ~E1000_TCTL_COLD;
	tctl |= coll_dist << E1000_COLD_SHIFT;

	ew32(TCTL, tctl);
	E1000_WRITE_FLUSH();
}

static s32 e1000_config_mac_to_phy(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;
	u16 phy_data;

	e_dbg("e1000_config_mac_to_phy");

	if ((hw->mac_type >= e1000_82544) && (hw->mac_type != e1000_ce4100))
		return E1000_SUCCESS;

	ctrl = er32(CTRL);
	ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	ctrl &= ~(E1000_CTRL_SPD_SEL | E1000_CTRL_ILOS);

	switch (hw->phy_type) {
	case e1000_phy_8201:
		ret_val = e1000_read_phy_reg(hw, PHY_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		if (phy_data & RTL_PHY_CTRL_FD)
			ctrl |= E1000_CTRL_FD;
		else
			ctrl &= ~E1000_CTRL_FD;

		if (phy_data & RTL_PHY_CTRL_SPD_100)
			ctrl |= E1000_CTRL_SPD_100;
		else
			ctrl |= E1000_CTRL_SPD_10;

		e1000_config_collision_dist(hw);
		break;
	default:
		ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_STATUS,
		                             &phy_data);
		if (ret_val)
			return ret_val;

		if (phy_data & M88E1000_PSSR_DPLX)
			ctrl |= E1000_CTRL_FD;
		else
			ctrl &= ~E1000_CTRL_FD;

		e1000_config_collision_dist(hw);

		if ((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS)
			ctrl |= E1000_CTRL_SPD_1000;
		else if ((phy_data & M88E1000_PSSR_SPEED) ==
		         M88E1000_PSSR_100MBS)
			ctrl |= E1000_CTRL_SPD_100;
	}

	
	ew32(CTRL, ctrl);
	return E1000_SUCCESS;
}

s32 e1000_force_mac_fc(struct e1000_hw *hw)
{
	u32 ctrl;

	e_dbg("e1000_force_mac_fc");

	
	ctrl = er32(CTRL);


	switch (hw->fc) {
	case E1000_FC_NONE:
		ctrl &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
		break;
	case E1000_FC_RX_PAUSE:
		ctrl &= (~E1000_CTRL_TFCE);
		ctrl |= E1000_CTRL_RFCE;
		break;
	case E1000_FC_TX_PAUSE:
		ctrl &= (~E1000_CTRL_RFCE);
		ctrl |= E1000_CTRL_TFCE;
		break;
	case E1000_FC_FULL:
		ctrl |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
		break;
	default:
		e_dbg("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
	}

	
	if (hw->mac_type == e1000_82542_rev2_0)
		ctrl &= (~E1000_CTRL_TFCE);

	ew32(CTRL, ctrl);
	return E1000_SUCCESS;
}

static s32 e1000_config_fc_after_link_up(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 mii_status_reg;
	u16 mii_nway_adv_reg;
	u16 mii_nway_lp_ability_reg;
	u16 speed;
	u16 duplex;

	e_dbg("e1000_config_fc_after_link_up");

	if (((hw->media_type == e1000_media_type_fiber) && (hw->autoneg_failed))
	    || ((hw->media_type == e1000_media_type_internal_serdes)
		&& (hw->autoneg_failed))
	    || ((hw->media_type == e1000_media_type_copper)
		&& (!hw->autoneg))) {
		ret_val = e1000_force_mac_fc(hw);
		if (ret_val) {
			e_dbg("Error forcing flow control settings\n");
			return ret_val;
		}
	}

	if ((hw->media_type == e1000_media_type_copper) && hw->autoneg) {
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;

		if (mii_status_reg & MII_SR_AUTONEG_COMPLETE) {
			ret_val = e1000_read_phy_reg(hw, PHY_AUTONEG_ADV,
						     &mii_nway_adv_reg);
			if (ret_val)
				return ret_val;
			ret_val = e1000_read_phy_reg(hw, PHY_LP_ABILITY,
						     &mii_nway_lp_ability_reg);
			if (ret_val)
				return ret_val;

			if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			    (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE)) {
				if (hw->original_fc == E1000_FC_FULL) {
					hw->fc = E1000_FC_FULL;
					e_dbg("Flow Control = FULL.\n");
				} else {
					hw->fc = E1000_FC_RX_PAUSE;
					e_dbg
					    ("Flow Control = RX PAUSE frames only.\n");
				}
			}
			else if (!(mii_nway_adv_reg & NWAY_AR_PAUSE) &&
				 (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
				 (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
				 (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR))
			{
				hw->fc = E1000_FC_TX_PAUSE;
				e_dbg
				    ("Flow Control = TX PAUSE frames only.\n");
			}
			else if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
				 (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
				 !(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
				 (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR))
			{
				hw->fc = E1000_FC_RX_PAUSE;
				e_dbg
				    ("Flow Control = RX PAUSE frames only.\n");
			}
			else if ((hw->original_fc == E1000_FC_NONE ||
				  hw->original_fc == E1000_FC_TX_PAUSE) ||
				 hw->fc_strict_ieee) {
				hw->fc = E1000_FC_NONE;
				e_dbg("Flow Control = NONE.\n");
			} else {
				hw->fc = E1000_FC_RX_PAUSE;
				e_dbg
				    ("Flow Control = RX PAUSE frames only.\n");
			}

			ret_val =
			    e1000_get_speed_and_duplex(hw, &speed, &duplex);
			if (ret_val) {
				e_dbg
				    ("Error getting link speed and duplex\n");
				return ret_val;
			}

			if (duplex == HALF_DUPLEX)
				hw->fc = E1000_FC_NONE;

			ret_val = e1000_force_mac_fc(hw);
			if (ret_val) {
				e_dbg
				    ("Error forcing flow control settings\n");
				return ret_val;
			}
		} else {
			e_dbg
			    ("Copper PHY and Auto Neg has not completed.\n");
		}
	}
	return E1000_SUCCESS;
}

static s32 e1000_check_for_serdes_link_generic(struct e1000_hw *hw)
{
	u32 rxcw;
	u32 ctrl;
	u32 status;
	s32 ret_val = E1000_SUCCESS;

	e_dbg("e1000_check_for_serdes_link_generic");

	ctrl = er32(CTRL);
	status = er32(STATUS);
	rxcw = er32(RXCW);

	
	if ((!(status & E1000_STATUS_LU)) && (!(rxcw & E1000_RXCW_C))) {
		if (hw->autoneg_failed == 0) {
			hw->autoneg_failed = 1;
			goto out;
		}
		e_dbg("NOT RXing /C/, disable AutoNeg and force link.\n");

		
		ew32(TXCW, (hw->txcw & ~E1000_TXCW_ANE));

		
		ctrl = er32(CTRL);
		ctrl |= (E1000_CTRL_SLU | E1000_CTRL_FD);
		ew32(CTRL, ctrl);

		
		ret_val = e1000_config_fc_after_link_up(hw);
		if (ret_val) {
			e_dbg("Error configuring flow control\n");
			goto out;
		}
	} else if ((ctrl & E1000_CTRL_SLU) && (rxcw & E1000_RXCW_C)) {
		e_dbg("RXing /C/, enable AutoNeg and stop forcing link.\n");
		ew32(TXCW, hw->txcw);
		ew32(CTRL, (ctrl & ~E1000_CTRL_SLU));

		hw->serdes_has_link = true;
	} else if (!(E1000_TXCW_ANE & er32(TXCW))) {
		
		udelay(10);
		rxcw = er32(RXCW);
		if (rxcw & E1000_RXCW_SYNCH) {
			if (!(rxcw & E1000_RXCW_IV)) {
				hw->serdes_has_link = true;
				e_dbg("SERDES: Link up - forced.\n");
			}
		} else {
			hw->serdes_has_link = false;
			e_dbg("SERDES: Link down - force failed.\n");
		}
	}

	if (E1000_TXCW_ANE & er32(TXCW)) {
		status = er32(STATUS);
		if (status & E1000_STATUS_LU) {
			
			udelay(10);
			rxcw = er32(RXCW);
			if (rxcw & E1000_RXCW_SYNCH) {
				if (!(rxcw & E1000_RXCW_IV)) {
					hw->serdes_has_link = true;
					e_dbg("SERDES: Link up - autoneg "
						 "completed successfully.\n");
				} else {
					hw->serdes_has_link = false;
					e_dbg("SERDES: Link down - invalid"
						 "codewords detected in autoneg.\n");
				}
			} else {
				hw->serdes_has_link = false;
				e_dbg("SERDES: Link down - no sync.\n");
			}
		} else {
			hw->serdes_has_link = false;
			e_dbg("SERDES: Link down - autoneg failed\n");
		}
	}

      out:
	return ret_val;
}

s32 e1000_check_for_link(struct e1000_hw *hw)
{
	u32 rxcw = 0;
	u32 ctrl;
	u32 status;
	u32 rctl;
	u32 icr;
	u32 signal = 0;
	s32 ret_val;
	u16 phy_data;

	e_dbg("e1000_check_for_link");

	ctrl = er32(CTRL);
	status = er32(STATUS);

	if ((hw->media_type == e1000_media_type_fiber) ||
	    (hw->media_type == e1000_media_type_internal_serdes)) {
		rxcw = er32(RXCW);

		if (hw->media_type == e1000_media_type_fiber) {
			signal =
			    (hw->mac_type >
			     e1000_82544) ? E1000_CTRL_SWDPIN1 : 0;
			if (status & E1000_STATUS_LU)
				hw->get_link_status = false;
		}
	}

	if ((hw->media_type == e1000_media_type_copper) && hw->get_link_status) {
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;

		if (phy_data & MII_SR_LINK_STATUS) {
			hw->get_link_status = false;
			e1000_check_downshift(hw);


			if ((hw->mac_type == e1000_82544
			     || hw->mac_type == e1000_82543) && (!hw->autoneg)
			    && (hw->forced_speed_duplex == e1000_10_full
				|| hw->forced_speed_duplex == e1000_10_half)) {
				ew32(IMC, 0xffffffff);
				ret_val =
				    e1000_polarity_reversal_workaround(hw);
				icr = er32(ICR);
				ew32(ICS, (icr & ~E1000_ICS_LSC));
				ew32(IMS, IMS_ENABLE_MASK);
			}

		} else {
			
			e1000_config_dsp_after_link_change(hw, false);
			return 0;
		}

		if (!hw->autoneg)
			return -E1000_ERR_CONFIG;

		
		e1000_config_dsp_after_link_change(hw, true);

		if ((hw->mac_type >= e1000_82544) &&
		    (hw->mac_type != e1000_ce4100))
			e1000_config_collision_dist(hw);
		else {
			ret_val = e1000_config_mac_to_phy(hw);
			if (ret_val) {
				e_dbg
				    ("Error configuring MAC to PHY settings\n");
				return ret_val;
			}
		}

		ret_val = e1000_config_fc_after_link_up(hw);
		if (ret_val) {
			e_dbg("Error configuring flow control\n");
			return ret_val;
		}

		if (hw->tbi_compatibility_en) {
			u16 speed, duplex;
			ret_val =
			    e1000_get_speed_and_duplex(hw, &speed, &duplex);
			if (ret_val) {
				e_dbg
				    ("Error getting link speed and duplex\n");
				return ret_val;
			}
			if (speed != SPEED_1000) {
				if (hw->tbi_compatibility_on) {
					
					rctl = er32(RCTL);
					rctl &= ~E1000_RCTL_SBP;
					ew32(RCTL, rctl);
					hw->tbi_compatibility_on = false;
				}
			} else {
				if (!hw->tbi_compatibility_on) {
					hw->tbi_compatibility_on = true;
					rctl = er32(RCTL);
					rctl |= E1000_RCTL_SBP;
					ew32(RCTL, rctl);
				}
			}
		}
	}

	if ((hw->media_type == e1000_media_type_fiber) ||
	    (hw->media_type == e1000_media_type_internal_serdes))
		e1000_check_for_serdes_link_generic(hw);

	return E1000_SUCCESS;
}

s32 e1000_get_speed_and_duplex(struct e1000_hw *hw, u16 *speed, u16 *duplex)
{
	u32 status;
	s32 ret_val;
	u16 phy_data;

	e_dbg("e1000_get_speed_and_duplex");

	if (hw->mac_type >= e1000_82543) {
		status = er32(STATUS);
		if (status & E1000_STATUS_SPEED_1000) {
			*speed = SPEED_1000;
			e_dbg("1000 Mbs, ");
		} else if (status & E1000_STATUS_SPEED_100) {
			*speed = SPEED_100;
			e_dbg("100 Mbs, ");
		} else {
			*speed = SPEED_10;
			e_dbg("10 Mbs, ");
		}

		if (status & E1000_STATUS_FD) {
			*duplex = FULL_DUPLEX;
			e_dbg("Full Duplex\n");
		} else {
			*duplex = HALF_DUPLEX;
			e_dbg(" Half Duplex\n");
		}
	} else {
		e_dbg("1000 Mbs, Full Duplex\n");
		*speed = SPEED_1000;
		*duplex = FULL_DUPLEX;
	}

	if (hw->phy_type == e1000_phy_igp && hw->speed_downgraded) {
		ret_val = e1000_read_phy_reg(hw, PHY_AUTONEG_EXP, &phy_data);
		if (ret_val)
			return ret_val;

		if (!(phy_data & NWAY_ER_LP_NWAY_CAPS))
			*duplex = HALF_DUPLEX;
		else {
			ret_val =
			    e1000_read_phy_reg(hw, PHY_LP_ABILITY, &phy_data);
			if (ret_val)
				return ret_val;
			if ((*speed == SPEED_100
			     && !(phy_data & NWAY_LPAR_100TX_FD_CAPS))
			    || (*speed == SPEED_10
				&& !(phy_data & NWAY_LPAR_10T_FD_CAPS)))
				*duplex = HALF_DUPLEX;
		}
	}

	return E1000_SUCCESS;
}

static s32 e1000_wait_autoneg(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 i;
	u16 phy_data;

	e_dbg("e1000_wait_autoneg");
	e_dbg("Waiting for Auto-Neg to complete.\n");

	
	for (i = PHY_AUTO_NEG_TIME; i > 0; i--) {
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;
		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
		if (ret_val)
			return ret_val;
		if (phy_data & MII_SR_AUTONEG_COMPLETE) {
			return E1000_SUCCESS;
		}
		msleep(100);
	}
	return E1000_SUCCESS;
}

static void e1000_raise_mdi_clk(struct e1000_hw *hw, u32 *ctrl)
{
	ew32(CTRL, (*ctrl | E1000_CTRL_MDC));
	E1000_WRITE_FLUSH();
	udelay(10);
}

static void e1000_lower_mdi_clk(struct e1000_hw *hw, u32 *ctrl)
{
	ew32(CTRL, (*ctrl & ~E1000_CTRL_MDC));
	E1000_WRITE_FLUSH();
	udelay(10);
}

static void e1000_shift_out_mdi_bits(struct e1000_hw *hw, u32 data, u16 count)
{
	u32 ctrl;
	u32 mask;

	mask = 0x01;
	mask <<= (count - 1);

	ctrl = er32(CTRL);

	
	ctrl |= (E1000_CTRL_MDIO_DIR | E1000_CTRL_MDC_DIR);

	while (mask) {
		if (data & mask)
			ctrl |= E1000_CTRL_MDIO;
		else
			ctrl &= ~E1000_CTRL_MDIO;

		ew32(CTRL, ctrl);
		E1000_WRITE_FLUSH();

		udelay(10);

		e1000_raise_mdi_clk(hw, &ctrl);
		e1000_lower_mdi_clk(hw, &ctrl);

		mask = mask >> 1;
	}
}

static u16 e1000_shift_in_mdi_bits(struct e1000_hw *hw)
{
	u32 ctrl;
	u16 data = 0;
	u8 i;

	ctrl = er32(CTRL);

	
	ctrl &= ~E1000_CTRL_MDIO_DIR;
	ctrl &= ~E1000_CTRL_MDIO;

	ew32(CTRL, ctrl);
	E1000_WRITE_FLUSH();

	e1000_raise_mdi_clk(hw, &ctrl);
	e1000_lower_mdi_clk(hw, &ctrl);

	for (data = 0, i = 0; i < 16; i++) {
		data = data << 1;
		e1000_raise_mdi_clk(hw, &ctrl);
		ctrl = er32(CTRL);
		
		if (ctrl & E1000_CTRL_MDIO)
			data |= 1;
		e1000_lower_mdi_clk(hw, &ctrl);
	}

	e1000_raise_mdi_clk(hw, &ctrl);
	e1000_lower_mdi_clk(hw, &ctrl);

	return data;
}


s32 e1000_read_phy_reg(struct e1000_hw *hw, u32 reg_addr, u16 *phy_data)
{
	u32 ret_val;

	e_dbg("e1000_read_phy_reg");

	if ((hw->phy_type == e1000_phy_igp) &&
	    (reg_addr > MAX_PHY_MULTI_PAGE_REG)) {
		ret_val = e1000_write_phy_reg_ex(hw, IGP01E1000_PHY_PAGE_SELECT,
						 (u16) reg_addr);
		if (ret_val)
			return ret_val;
	}

	ret_val = e1000_read_phy_reg_ex(hw, MAX_PHY_REG_ADDRESS & reg_addr,
					phy_data);

	return ret_val;
}

static s32 e1000_read_phy_reg_ex(struct e1000_hw *hw, u32 reg_addr,
				 u16 *phy_data)
{
	u32 i;
	u32 mdic = 0;
	const u32 phy_addr = (hw->mac_type == e1000_ce4100) ? hw->phy_addr : 1;

	e_dbg("e1000_read_phy_reg_ex");

	if (reg_addr > MAX_PHY_REG_ADDRESS) {
		e_dbg("PHY Address %d is out of range\n", reg_addr);
		return -E1000_ERR_PARAM;
	}

	if (hw->mac_type > e1000_82543) {
		if (hw->mac_type == e1000_ce4100) {
			mdic = ((reg_addr << E1000_MDIC_REG_SHIFT) |
				(phy_addr << E1000_MDIC_PHY_SHIFT) |
				(INTEL_CE_GBE_MDIC_OP_READ) |
				(INTEL_CE_GBE_MDIC_GO));

			writel(mdic, E1000_MDIO_CMD);

			for (i = 0; i < 64; i++) {
				udelay(50);
				mdic = readl(E1000_MDIO_CMD);
				if (!(mdic & INTEL_CE_GBE_MDIC_GO))
					break;
			}

			if (mdic & INTEL_CE_GBE_MDIC_GO) {
				e_dbg("MDI Read did not complete\n");
				return -E1000_ERR_PHY;
			}

			mdic = readl(E1000_MDIO_STS);
			if (mdic & INTEL_CE_GBE_MDIC_READ_ERROR) {
				e_dbg("MDI Read Error\n");
				return -E1000_ERR_PHY;
			}
			*phy_data = (u16) mdic;
		} else {
			mdic = ((reg_addr << E1000_MDIC_REG_SHIFT) |
				(phy_addr << E1000_MDIC_PHY_SHIFT) |
				(E1000_MDIC_OP_READ));

			ew32(MDIC, mdic);

			for (i = 0; i < 64; i++) {
				udelay(50);
				mdic = er32(MDIC);
				if (mdic & E1000_MDIC_READY)
					break;
			}
			if (!(mdic & E1000_MDIC_READY)) {
				e_dbg("MDI Read did not complete\n");
				return -E1000_ERR_PHY;
			}
			if (mdic & E1000_MDIC_ERROR) {
				e_dbg("MDI Error\n");
				return -E1000_ERR_PHY;
			}
			*phy_data = (u16) mdic;
		}
	} else {
		e1000_shift_out_mdi_bits(hw, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

		mdic = ((reg_addr) | (phy_addr << 5) |
			(PHY_OP_READ << 10) | (PHY_SOF << 12));

		e1000_shift_out_mdi_bits(hw, mdic, 14);

		*phy_data = e1000_shift_in_mdi_bits(hw);
	}
	return E1000_SUCCESS;
}

s32 e1000_write_phy_reg(struct e1000_hw *hw, u32 reg_addr, u16 phy_data)
{
	u32 ret_val;

	e_dbg("e1000_write_phy_reg");

	if ((hw->phy_type == e1000_phy_igp) &&
	    (reg_addr > MAX_PHY_MULTI_PAGE_REG)) {
		ret_val = e1000_write_phy_reg_ex(hw, IGP01E1000_PHY_PAGE_SELECT,
						 (u16) reg_addr);
		if (ret_val)
			return ret_val;
	}

	ret_val = e1000_write_phy_reg_ex(hw, MAX_PHY_REG_ADDRESS & reg_addr,
					 phy_data);

	return ret_val;
}

static s32 e1000_write_phy_reg_ex(struct e1000_hw *hw, u32 reg_addr,
				  u16 phy_data)
{
	u32 i;
	u32 mdic = 0;
	const u32 phy_addr = (hw->mac_type == e1000_ce4100) ? hw->phy_addr : 1;

	e_dbg("e1000_write_phy_reg_ex");

	if (reg_addr > MAX_PHY_REG_ADDRESS) {
		e_dbg("PHY Address %d is out of range\n", reg_addr);
		return -E1000_ERR_PARAM;
	}

	if (hw->mac_type > e1000_82543) {
		if (hw->mac_type == e1000_ce4100) {
			mdic = (((u32) phy_data) |
				(reg_addr << E1000_MDIC_REG_SHIFT) |
				(phy_addr << E1000_MDIC_PHY_SHIFT) |
				(INTEL_CE_GBE_MDIC_OP_WRITE) |
				(INTEL_CE_GBE_MDIC_GO));

			writel(mdic, E1000_MDIO_CMD);

			for (i = 0; i < 640; i++) {
				udelay(5);
				mdic = readl(E1000_MDIO_CMD);
				if (!(mdic & INTEL_CE_GBE_MDIC_GO))
					break;
			}
			if (mdic & INTEL_CE_GBE_MDIC_GO) {
				e_dbg("MDI Write did not complete\n");
				return -E1000_ERR_PHY;
			}
		} else {
			mdic = (((u32) phy_data) |
				(reg_addr << E1000_MDIC_REG_SHIFT) |
				(phy_addr << E1000_MDIC_PHY_SHIFT) |
				(E1000_MDIC_OP_WRITE));

			ew32(MDIC, mdic);

			for (i = 0; i < 641; i++) {
				udelay(5);
				mdic = er32(MDIC);
				if (mdic & E1000_MDIC_READY)
					break;
			}
			if (!(mdic & E1000_MDIC_READY)) {
				e_dbg("MDI Write did not complete\n");
				return -E1000_ERR_PHY;
			}
		}
	} else {
		e1000_shift_out_mdi_bits(hw, PHY_PREAMBLE, PHY_PREAMBLE_SIZE);

		mdic = ((PHY_TURNAROUND) | (reg_addr << 2) | (phy_addr << 7) |
			(PHY_OP_WRITE << 12) | (PHY_SOF << 14));
		mdic <<= 16;
		mdic |= (u32) phy_data;

		e1000_shift_out_mdi_bits(hw, mdic, 32);
	}

	return E1000_SUCCESS;
}

s32 e1000_phy_hw_reset(struct e1000_hw *hw)
{
	u32 ctrl, ctrl_ext;
	u32 led_ctrl;

	e_dbg("e1000_phy_hw_reset");

	e_dbg("Resetting Phy...\n");

	if (hw->mac_type > e1000_82543) {
		ctrl = er32(CTRL);
		ew32(CTRL, ctrl | E1000_CTRL_PHY_RST);
		E1000_WRITE_FLUSH();

		msleep(10);

		ew32(CTRL, ctrl);
		E1000_WRITE_FLUSH();

	} else {
		ctrl_ext = er32(CTRL_EXT);
		ctrl_ext |= E1000_CTRL_EXT_SDP4_DIR;
		ctrl_ext &= ~E1000_CTRL_EXT_SDP4_DATA;
		ew32(CTRL_EXT, ctrl_ext);
		E1000_WRITE_FLUSH();
		msleep(10);
		ctrl_ext |= E1000_CTRL_EXT_SDP4_DATA;
		ew32(CTRL_EXT, ctrl_ext);
		E1000_WRITE_FLUSH();
	}
	udelay(150);

	if ((hw->mac_type == e1000_82541) || (hw->mac_type == e1000_82547)) {
		
		led_ctrl = er32(LEDCTL);
		led_ctrl &= IGP_ACTIVITY_LED_MASK;
		led_ctrl |= (IGP_ACTIVITY_LED_ENABLE | IGP_LED3_MODE);
		ew32(LEDCTL, led_ctrl);
	}

	
	return e1000_get_phy_cfg_done(hw);
}

s32 e1000_phy_reset(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_data;

	e_dbg("e1000_phy_reset");

	switch (hw->phy_type) {
	case e1000_phy_igp:
		ret_val = e1000_phy_hw_reset(hw);
		if (ret_val)
			return ret_val;
		break;
	default:
		ret_val = e1000_read_phy_reg(hw, PHY_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		phy_data |= MII_CR_RESET;
		ret_val = e1000_write_phy_reg(hw, PHY_CTRL, phy_data);
		if (ret_val)
			return ret_val;

		udelay(1);
		break;
	}

	if (hw->phy_type == e1000_phy_igp)
		e1000_phy_init_script(hw);

	return E1000_SUCCESS;
}

static s32 e1000_detect_gig_phy(struct e1000_hw *hw)
{
	s32 phy_init_status, ret_val;
	u16 phy_id_high, phy_id_low;
	bool match = false;

	e_dbg("e1000_detect_gig_phy");

	if (hw->phy_id != 0)
		return E1000_SUCCESS;

	
	ret_val = e1000_read_phy_reg(hw, PHY_ID1, &phy_id_high);
	if (ret_val)
		return ret_val;

	hw->phy_id = (u32) (phy_id_high << 16);
	udelay(20);
	ret_val = e1000_read_phy_reg(hw, PHY_ID2, &phy_id_low);
	if (ret_val)
		return ret_val;

	hw->phy_id |= (u32) (phy_id_low & PHY_REVISION_MASK);
	hw->phy_revision = (u32) phy_id_low & ~PHY_REVISION_MASK;

	switch (hw->mac_type) {
	case e1000_82543:
		if (hw->phy_id == M88E1000_E_PHY_ID)
			match = true;
		break;
	case e1000_82544:
		if (hw->phy_id == M88E1000_I_PHY_ID)
			match = true;
		break;
	case e1000_82540:
	case e1000_82545:
	case e1000_82545_rev_3:
	case e1000_82546:
	case e1000_82546_rev_3:
		if (hw->phy_id == M88E1011_I_PHY_ID)
			match = true;
		break;
	case e1000_ce4100:
		if ((hw->phy_id == RTL8211B_PHY_ID) ||
		    (hw->phy_id == RTL8201N_PHY_ID) ||
		    (hw->phy_id == M88E1118_E_PHY_ID))
			match = true;
		break;
	case e1000_82541:
	case e1000_82541_rev_2:
	case e1000_82547:
	case e1000_82547_rev_2:
		if (hw->phy_id == IGP01E1000_I_PHY_ID)
			match = true;
		break;
	default:
		e_dbg("Invalid MAC type %d\n", hw->mac_type);
		return -E1000_ERR_CONFIG;
	}
	phy_init_status = e1000_set_phy_type(hw);

	if ((match) && (phy_init_status == E1000_SUCCESS)) {
		e_dbg("PHY ID 0x%X detected\n", hw->phy_id);
		return E1000_SUCCESS;
	}
	e_dbg("Invalid PHY ID 0x%X\n", hw->phy_id);
	return -E1000_ERR_PHY;
}

static s32 e1000_phy_reset_dsp(struct e1000_hw *hw)
{
	s32 ret_val;
	e_dbg("e1000_phy_reset_dsp");

	do {
		ret_val = e1000_write_phy_reg(hw, 29, 0x001d);
		if (ret_val)
			break;
		ret_val = e1000_write_phy_reg(hw, 30, 0x00c1);
		if (ret_val)
			break;
		ret_val = e1000_write_phy_reg(hw, 30, 0x0000);
		if (ret_val)
			break;
		ret_val = E1000_SUCCESS;
	} while (0);

	return ret_val;
}

static s32 e1000_phy_igp_get_info(struct e1000_hw *hw,
				  struct e1000_phy_info *phy_info)
{
	s32 ret_val;
	u16 phy_data, min_length, max_length, average;
	e1000_rev_polarity polarity;

	e_dbg("e1000_phy_igp_get_info");

	phy_info->downshift = (e1000_downshift) hw->speed_downgraded;

	
	phy_info->extended_10bt_distance = e1000_10bt_ext_dist_enable_normal;

	
	phy_info->polarity_correction = e1000_polarity_reversal_enabled;

	
	ret_val = e1000_check_polarity(hw, &polarity);
	if (ret_val)
		return ret_val;

	phy_info->cable_polarity = polarity;

	ret_val = e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	phy_info->mdix_mode =
	    (e1000_auto_x_mode) ((phy_data & IGP01E1000_PSSR_MDIX) >>
				 IGP01E1000_PSSR_MDIX_SHIFT);

	if ((phy_data & IGP01E1000_PSSR_SPEED_MASK) ==
	    IGP01E1000_PSSR_SPEED_1000MBPS) {
		
		ret_val = e1000_read_phy_reg(hw, PHY_1000T_STATUS, &phy_data);
		if (ret_val)
			return ret_val;

		phy_info->local_rx = ((phy_data & SR_1000T_LOCAL_RX_STATUS) >>
				      SR_1000T_LOCAL_RX_STATUS_SHIFT) ?
		    e1000_1000t_rx_status_ok : e1000_1000t_rx_status_not_ok;
		phy_info->remote_rx = ((phy_data & SR_1000T_REMOTE_RX_STATUS) >>
				       SR_1000T_REMOTE_RX_STATUS_SHIFT) ?
		    e1000_1000t_rx_status_ok : e1000_1000t_rx_status_not_ok;

		
		ret_val = e1000_get_cable_length(hw, &min_length, &max_length);
		if (ret_val)
			return ret_val;

		
		average = (max_length + min_length) / 2;

		if (average <= e1000_igp_cable_length_50)
			phy_info->cable_length = e1000_cable_length_50;
		else if (average <= e1000_igp_cable_length_80)
			phy_info->cable_length = e1000_cable_length_50_80;
		else if (average <= e1000_igp_cable_length_110)
			phy_info->cable_length = e1000_cable_length_80_110;
		else if (average <= e1000_igp_cable_length_140)
			phy_info->cable_length = e1000_cable_length_110_140;
		else
			phy_info->cable_length = e1000_cable_length_140;
	}

	return E1000_SUCCESS;
}

static s32 e1000_phy_m88_get_info(struct e1000_hw *hw,
				  struct e1000_phy_info *phy_info)
{
	s32 ret_val;
	u16 phy_data;
	e1000_rev_polarity polarity;

	e_dbg("e1000_phy_m88_get_info");

	phy_info->downshift = (e1000_downshift) hw->speed_downgraded;

	ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_info->extended_10bt_distance =
	    ((phy_data & M88E1000_PSCR_10BT_EXT_DIST_ENABLE) >>
	     M88E1000_PSCR_10BT_EXT_DIST_ENABLE_SHIFT) ?
	    e1000_10bt_ext_dist_enable_lower :
	    e1000_10bt_ext_dist_enable_normal;

	phy_info->polarity_correction =
	    ((phy_data & M88E1000_PSCR_POLARITY_REVERSAL) >>
	     M88E1000_PSCR_POLARITY_REVERSAL_SHIFT) ?
	    e1000_polarity_reversal_disabled : e1000_polarity_reversal_enabled;

	
	ret_val = e1000_check_polarity(hw, &polarity);
	if (ret_val)
		return ret_val;
	phy_info->cable_polarity = polarity;

	ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	phy_info->mdix_mode =
	    (e1000_auto_x_mode) ((phy_data & M88E1000_PSSR_MDIX) >>
				 M88E1000_PSSR_MDIX_SHIFT);

	if ((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS) {
		phy_info->cable_length =
		    (e1000_cable_length) ((phy_data &
					   M88E1000_PSSR_CABLE_LENGTH) >>
					  M88E1000_PSSR_CABLE_LENGTH_SHIFT);

		ret_val = e1000_read_phy_reg(hw, PHY_1000T_STATUS, &phy_data);
		if (ret_val)
			return ret_val;

		phy_info->local_rx = ((phy_data & SR_1000T_LOCAL_RX_STATUS) >>
				      SR_1000T_LOCAL_RX_STATUS_SHIFT) ?
		    e1000_1000t_rx_status_ok : e1000_1000t_rx_status_not_ok;
		phy_info->remote_rx = ((phy_data & SR_1000T_REMOTE_RX_STATUS) >>
				       SR_1000T_REMOTE_RX_STATUS_SHIFT) ?
		    e1000_1000t_rx_status_ok : e1000_1000t_rx_status_not_ok;

	}

	return E1000_SUCCESS;
}

s32 e1000_phy_get_info(struct e1000_hw *hw, struct e1000_phy_info *phy_info)
{
	s32 ret_val;
	u16 phy_data;

	e_dbg("e1000_phy_get_info");

	phy_info->cable_length = e1000_cable_length_undefined;
	phy_info->extended_10bt_distance = e1000_10bt_ext_dist_enable_undefined;
	phy_info->cable_polarity = e1000_rev_polarity_undefined;
	phy_info->downshift = e1000_downshift_undefined;
	phy_info->polarity_correction = e1000_polarity_reversal_undefined;
	phy_info->mdix_mode = e1000_auto_x_mode_undefined;
	phy_info->local_rx = e1000_1000t_rx_status_undefined;
	phy_info->remote_rx = e1000_1000t_rx_status_undefined;

	if (hw->media_type != e1000_media_type_copper) {
		e_dbg("PHY info is only valid for copper media\n");
		return -E1000_ERR_CONFIG;
	}

	ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	if ((phy_data & MII_SR_LINK_STATUS) != MII_SR_LINK_STATUS) {
		e_dbg("PHY info is only valid if link is up\n");
		return -E1000_ERR_CONFIG;
	}

	if (hw->phy_type == e1000_phy_igp)
		return e1000_phy_igp_get_info(hw, phy_info);
	else if ((hw->phy_type == e1000_phy_8211) ||
	         (hw->phy_type == e1000_phy_8201))
		return E1000_SUCCESS;
	else
		return e1000_phy_m88_get_info(hw, phy_info);
}

s32 e1000_validate_mdi_setting(struct e1000_hw *hw)
{
	e_dbg("e1000_validate_mdi_settings");

	if (!hw->autoneg && (hw->mdix == 0 || hw->mdix == 3)) {
		e_dbg("Invalid MDI setting detected\n");
		hw->mdix = 1;
		return -E1000_ERR_CONFIG;
	}
	return E1000_SUCCESS;
}

s32 e1000_init_eeprom_params(struct e1000_hw *hw)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	u32 eecd = er32(EECD);
	s32 ret_val = E1000_SUCCESS;
	u16 eeprom_size;

	e_dbg("e1000_init_eeprom_params");

	switch (hw->mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
	case e1000_82543:
	case e1000_82544:
		eeprom->type = e1000_eeprom_microwire;
		eeprom->word_size = 64;
		eeprom->opcode_bits = 3;
		eeprom->address_bits = 6;
		eeprom->delay_usec = 50;
		break;
	case e1000_82540:
	case e1000_82545:
	case e1000_82545_rev_3:
	case e1000_82546:
	case e1000_82546_rev_3:
		eeprom->type = e1000_eeprom_microwire;
		eeprom->opcode_bits = 3;
		eeprom->delay_usec = 50;
		if (eecd & E1000_EECD_SIZE) {
			eeprom->word_size = 256;
			eeprom->address_bits = 8;
		} else {
			eeprom->word_size = 64;
			eeprom->address_bits = 6;
		}
		break;
	case e1000_82541:
	case e1000_82541_rev_2:
	case e1000_82547:
	case e1000_82547_rev_2:
		if (eecd & E1000_EECD_TYPE) {
			eeprom->type = e1000_eeprom_spi;
			eeprom->opcode_bits = 8;
			eeprom->delay_usec = 1;
			if (eecd & E1000_EECD_ADDR_BITS) {
				eeprom->page_size = 32;
				eeprom->address_bits = 16;
			} else {
				eeprom->page_size = 8;
				eeprom->address_bits = 8;
			}
		} else {
			eeprom->type = e1000_eeprom_microwire;
			eeprom->opcode_bits = 3;
			eeprom->delay_usec = 50;
			if (eecd & E1000_EECD_ADDR_BITS) {
				eeprom->word_size = 256;
				eeprom->address_bits = 8;
			} else {
				eeprom->word_size = 64;
				eeprom->address_bits = 6;
			}
		}
		break;
	default:
		break;
	}

	if (eeprom->type == e1000_eeprom_spi) {
		
		eeprom->word_size = 64;
		ret_val = e1000_read_eeprom(hw, EEPROM_CFG, 1, &eeprom_size);
		if (ret_val)
			return ret_val;
		eeprom_size =
		    (eeprom_size & EEPROM_SIZE_MASK) >> EEPROM_SIZE_SHIFT;
		if (eeprom_size)
			eeprom_size++;

		eeprom->word_size = 1 << (eeprom_size + EEPROM_WORD_SIZE_SHIFT);
	}
	return ret_val;
}

static void e1000_raise_ee_clk(struct e1000_hw *hw, u32 *eecd)
{
	*eecd = *eecd | E1000_EECD_SK;
	ew32(EECD, *eecd);
	E1000_WRITE_FLUSH();
	udelay(hw->eeprom.delay_usec);
}

static void e1000_lower_ee_clk(struct e1000_hw *hw, u32 *eecd)
{
	*eecd = *eecd & ~E1000_EECD_SK;
	ew32(EECD, *eecd);
	E1000_WRITE_FLUSH();
	udelay(hw->eeprom.delay_usec);
}

static void e1000_shift_out_ee_bits(struct e1000_hw *hw, u16 data, u16 count)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	u32 eecd;
	u32 mask;

	mask = 0x01 << (count - 1);
	eecd = er32(EECD);
	if (eeprom->type == e1000_eeprom_microwire) {
		eecd &= ~E1000_EECD_DO;
	} else if (eeprom->type == e1000_eeprom_spi) {
		eecd |= E1000_EECD_DO;
	}
	do {
		eecd &= ~E1000_EECD_DI;

		if (data & mask)
			eecd |= E1000_EECD_DI;

		ew32(EECD, eecd);
		E1000_WRITE_FLUSH();

		udelay(eeprom->delay_usec);

		e1000_raise_ee_clk(hw, &eecd);
		e1000_lower_ee_clk(hw, &eecd);

		mask = mask >> 1;

	} while (mask);

	
	eecd &= ~E1000_EECD_DI;
	ew32(EECD, eecd);
}

static u16 e1000_shift_in_ee_bits(struct e1000_hw *hw, u16 count)
{
	u32 eecd;
	u32 i;
	u16 data;


	eecd = er32(EECD);

	eecd &= ~(E1000_EECD_DO | E1000_EECD_DI);
	data = 0;

	for (i = 0; i < count; i++) {
		data = data << 1;
		e1000_raise_ee_clk(hw, &eecd);

		eecd = er32(EECD);

		eecd &= ~(E1000_EECD_DI);
		if (eecd & E1000_EECD_DO)
			data |= 1;

		e1000_lower_ee_clk(hw, &eecd);
	}

	return data;
}

static s32 e1000_acquire_eeprom(struct e1000_hw *hw)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	u32 eecd, i = 0;

	e_dbg("e1000_acquire_eeprom");

	eecd = er32(EECD);

	
	if (hw->mac_type > e1000_82544) {
		eecd |= E1000_EECD_REQ;
		ew32(EECD, eecd);
		eecd = er32(EECD);
		while ((!(eecd & E1000_EECD_GNT)) &&
		       (i < E1000_EEPROM_GRANT_ATTEMPTS)) {
			i++;
			udelay(5);
			eecd = er32(EECD);
		}
		if (!(eecd & E1000_EECD_GNT)) {
			eecd &= ~E1000_EECD_REQ;
			ew32(EECD, eecd);
			e_dbg("Could not acquire EEPROM grant\n");
			return -E1000_ERR_EEPROM;
		}
	}

	

	if (eeprom->type == e1000_eeprom_microwire) {
		
		eecd &= ~(E1000_EECD_DI | E1000_EECD_SK);
		ew32(EECD, eecd);

		
		eecd |= E1000_EECD_CS;
		ew32(EECD, eecd);
	} else if (eeprom->type == e1000_eeprom_spi) {
		
		eecd &= ~(E1000_EECD_CS | E1000_EECD_SK);
		ew32(EECD, eecd);
		E1000_WRITE_FLUSH();
		udelay(1);
	}

	return E1000_SUCCESS;
}

static void e1000_standby_eeprom(struct e1000_hw *hw)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	u32 eecd;

	eecd = er32(EECD);

	if (eeprom->type == e1000_eeprom_microwire) {
		eecd &= ~(E1000_EECD_CS | E1000_EECD_SK);
		ew32(EECD, eecd);
		E1000_WRITE_FLUSH();
		udelay(eeprom->delay_usec);

		
		eecd |= E1000_EECD_SK;
		ew32(EECD, eecd);
		E1000_WRITE_FLUSH();
		udelay(eeprom->delay_usec);

		
		eecd |= E1000_EECD_CS;
		ew32(EECD, eecd);
		E1000_WRITE_FLUSH();
		udelay(eeprom->delay_usec);

		
		eecd &= ~E1000_EECD_SK;
		ew32(EECD, eecd);
		E1000_WRITE_FLUSH();
		udelay(eeprom->delay_usec);
	} else if (eeprom->type == e1000_eeprom_spi) {
		
		eecd |= E1000_EECD_CS;
		ew32(EECD, eecd);
		E1000_WRITE_FLUSH();
		udelay(eeprom->delay_usec);
		eecd &= ~E1000_EECD_CS;
		ew32(EECD, eecd);
		E1000_WRITE_FLUSH();
		udelay(eeprom->delay_usec);
	}
}

static void e1000_release_eeprom(struct e1000_hw *hw)
{
	u32 eecd;

	e_dbg("e1000_release_eeprom");

	eecd = er32(EECD);

	if (hw->eeprom.type == e1000_eeprom_spi) {
		eecd |= E1000_EECD_CS;	
		eecd &= ~E1000_EECD_SK;	

		ew32(EECD, eecd);
		E1000_WRITE_FLUSH();

		udelay(hw->eeprom.delay_usec);
	} else if (hw->eeprom.type == e1000_eeprom_microwire) {
		

		
		eecd &= ~(E1000_EECD_CS | E1000_EECD_DI);

		ew32(EECD, eecd);

		
		eecd |= E1000_EECD_SK;
		ew32(EECD, eecd);
		E1000_WRITE_FLUSH();
		udelay(hw->eeprom.delay_usec);

		
		eecd &= ~E1000_EECD_SK;
		ew32(EECD, eecd);
		E1000_WRITE_FLUSH();
		udelay(hw->eeprom.delay_usec);
	}

	
	if (hw->mac_type > e1000_82544) {
		eecd &= ~E1000_EECD_REQ;
		ew32(EECD, eecd);
	}
}

static s32 e1000_spi_eeprom_ready(struct e1000_hw *hw)
{
	u16 retry_count = 0;
	u8 spi_stat_reg;

	e_dbg("e1000_spi_eeprom_ready");

	retry_count = 0;
	do {
		e1000_shift_out_ee_bits(hw, EEPROM_RDSR_OPCODE_SPI,
					hw->eeprom.opcode_bits);
		spi_stat_reg = (u8) e1000_shift_in_ee_bits(hw, 8);
		if (!(spi_stat_reg & EEPROM_STATUS_RDY_SPI))
			break;

		udelay(5);
		retry_count += 5;

		e1000_standby_eeprom(hw);
	} while (retry_count < EEPROM_MAX_RETRY_SPI);

	if (retry_count >= EEPROM_MAX_RETRY_SPI) {
		e_dbg("SPI EEPROM Status error\n");
		return -E1000_ERR_EEPROM;
	}

	return E1000_SUCCESS;
}

s32 e1000_read_eeprom(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	s32 ret;
	spin_lock(&e1000_eeprom_lock);
	ret = e1000_do_read_eeprom(hw, offset, words, data);
	spin_unlock(&e1000_eeprom_lock);
	return ret;
}

static s32 e1000_do_read_eeprom(struct e1000_hw *hw, u16 offset, u16 words,
				u16 *data)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	u32 i = 0;

	e_dbg("e1000_read_eeprom");

	if (hw->mac_type == e1000_ce4100) {
		GBE_CONFIG_FLASH_READ(GBE_CONFIG_BASE_VIRT, offset, words,
		                      data);
		return E1000_SUCCESS;
	}

	
	if (eeprom->word_size == 0)
		e1000_init_eeprom_params(hw);

	if ((offset >= eeprom->word_size)
	    || (words > eeprom->word_size - offset) || (words == 0)) {
		e_dbg("\"words\" parameter out of bounds. Words = %d,"
		      "size = %d\n", offset, eeprom->word_size);
		return -E1000_ERR_EEPROM;
	}

	
	if (e1000_acquire_eeprom(hw) != E1000_SUCCESS)
		return -E1000_ERR_EEPROM;

	if (eeprom->type == e1000_eeprom_spi) {
		u16 word_in;
		u8 read_opcode = EEPROM_READ_OPCODE_SPI;

		if (e1000_spi_eeprom_ready(hw)) {
			e1000_release_eeprom(hw);
			return -E1000_ERR_EEPROM;
		}

		e1000_standby_eeprom(hw);

		
		if ((eeprom->address_bits == 8) && (offset >= 128))
			read_opcode |= EEPROM_A8_OPCODE_SPI;

		
		e1000_shift_out_ee_bits(hw, read_opcode, eeprom->opcode_bits);
		e1000_shift_out_ee_bits(hw, (u16) (offset * 2),
					eeprom->address_bits);

		for (i = 0; i < words; i++) {
			word_in = e1000_shift_in_ee_bits(hw, 16);
			data[i] = (word_in >> 8) | (word_in << 8);
		}
	} else if (eeprom->type == e1000_eeprom_microwire) {
		for (i = 0; i < words; i++) {
			
			e1000_shift_out_ee_bits(hw,
						EEPROM_READ_OPCODE_MICROWIRE,
						eeprom->opcode_bits);
			e1000_shift_out_ee_bits(hw, (u16) (offset + i),
						eeprom->address_bits);

			data[i] = e1000_shift_in_ee_bits(hw, 16);
			e1000_standby_eeprom(hw);
		}
	}

	
	e1000_release_eeprom(hw);

	return E1000_SUCCESS;
}

s32 e1000_validate_eeprom_checksum(struct e1000_hw *hw)
{
	u16 checksum = 0;
	u16 i, eeprom_data;

	e_dbg("e1000_validate_eeprom_checksum");

	for (i = 0; i < (EEPROM_CHECKSUM_REG + 1); i++) {
		if (e1000_read_eeprom(hw, i, 1, &eeprom_data) < 0) {
			e_dbg("EEPROM Read Error\n");
			return -E1000_ERR_EEPROM;
		}
		checksum += eeprom_data;
	}

#ifdef CONFIG_PARISC
	
	if ((hw->subsystem_vendor_id == 0x103C) && (eeprom_data == 0x16d6))
		return E1000_SUCCESS;

#endif
	if (checksum == (u16) EEPROM_SUM)
		return E1000_SUCCESS;
	else {
		e_dbg("EEPROM Checksum Invalid\n");
		return -E1000_ERR_EEPROM;
	}
}

s32 e1000_update_eeprom_checksum(struct e1000_hw *hw)
{
	u16 checksum = 0;
	u16 i, eeprom_data;

	e_dbg("e1000_update_eeprom_checksum");

	for (i = 0; i < EEPROM_CHECKSUM_REG; i++) {
		if (e1000_read_eeprom(hw, i, 1, &eeprom_data) < 0) {
			e_dbg("EEPROM Read Error\n");
			return -E1000_ERR_EEPROM;
		}
		checksum += eeprom_data;
	}
	checksum = (u16) EEPROM_SUM - checksum;
	if (e1000_write_eeprom(hw, EEPROM_CHECKSUM_REG, 1, &checksum) < 0) {
		e_dbg("EEPROM Write Error\n");
		return -E1000_ERR_EEPROM;
	}
	return E1000_SUCCESS;
}

/**
 * e1000_write_eeprom - write words to the different EEPROM types.
 * @hw: Struct containing variables accessed by shared code
 * @offset: offset within the EEPROM to be written to
 * @words: number of words to write
 * @data: 16 bit word to be written to the EEPROM
 *
 * If e1000_update_eeprom_checksum is not called after this function, the
 * EEPROM will most likely contain an invalid checksum.
 */
s32 e1000_write_eeprom(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	s32 ret;
	spin_lock(&e1000_eeprom_lock);
	ret = e1000_do_write_eeprom(hw, offset, words, data);
	spin_unlock(&e1000_eeprom_lock);
	return ret;
}

static s32 e1000_do_write_eeprom(struct e1000_hw *hw, u16 offset, u16 words,
				 u16 *data)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	s32 status = 0;

	e_dbg("e1000_write_eeprom");

	if (hw->mac_type == e1000_ce4100) {
		GBE_CONFIG_FLASH_WRITE(GBE_CONFIG_BASE_VIRT, offset, words,
		                       data);
		return E1000_SUCCESS;
	}

	
	if (eeprom->word_size == 0)
		e1000_init_eeprom_params(hw);

	if ((offset >= eeprom->word_size)
	    || (words > eeprom->word_size - offset) || (words == 0)) {
		e_dbg("\"words\" parameter out of bounds\n");
		return -E1000_ERR_EEPROM;
	}

	
	if (e1000_acquire_eeprom(hw) != E1000_SUCCESS)
		return -E1000_ERR_EEPROM;

	if (eeprom->type == e1000_eeprom_microwire) {
		status = e1000_write_eeprom_microwire(hw, offset, words, data);
	} else {
		status = e1000_write_eeprom_spi(hw, offset, words, data);
		msleep(10);
	}

	
	e1000_release_eeprom(hw);

	return status;
}

/**
 * e1000_write_eeprom_spi - Writes a 16 bit word to a given offset in an SPI EEPROM.
 * @hw: Struct containing variables accessed by shared code
 * @offset: offset within the EEPROM to be written to
 * @words: number of words to write
 * @data: pointer to array of 8 bit words to be written to the EEPROM
 */
static s32 e1000_write_eeprom_spi(struct e1000_hw *hw, u16 offset, u16 words,
				  u16 *data)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	u16 widx = 0;

	e_dbg("e1000_write_eeprom_spi");

	while (widx < words) {
		u8 write_opcode = EEPROM_WRITE_OPCODE_SPI;

		if (e1000_spi_eeprom_ready(hw))
			return -E1000_ERR_EEPROM;

		e1000_standby_eeprom(hw);

		
		e1000_shift_out_ee_bits(hw, EEPROM_WREN_OPCODE_SPI,
					eeprom->opcode_bits);

		e1000_standby_eeprom(hw);

		
		if ((eeprom->address_bits == 8) && (offset >= 128))
			write_opcode |= EEPROM_A8_OPCODE_SPI;

		
		e1000_shift_out_ee_bits(hw, write_opcode, eeprom->opcode_bits);

		e1000_shift_out_ee_bits(hw, (u16) ((offset + widx) * 2),
					eeprom->address_bits);

		

		
		while (widx < words) {
			u16 word_out = data[widx];
			word_out = (word_out >> 8) | (word_out << 8);
			e1000_shift_out_ee_bits(hw, word_out, 16);
			widx++;

			if ((((offset + widx) * 2) % eeprom->page_size) == 0) {
				e1000_standby_eeprom(hw);
				break;
			}
		}
	}

	return E1000_SUCCESS;
}

/**
 * e1000_write_eeprom_microwire - Writes a 16 bit word to a given offset in a Microwire EEPROM.
 * @hw: Struct containing variables accessed by shared code
 * @offset: offset within the EEPROM to be written to
 * @words: number of words to write
 * @data: pointer to array of 8 bit words to be written to the EEPROM
 */
static s32 e1000_write_eeprom_microwire(struct e1000_hw *hw, u16 offset,
					u16 words, u16 *data)
{
	struct e1000_eeprom_info *eeprom = &hw->eeprom;
	u32 eecd;
	u16 words_written = 0;
	u16 i = 0;

	e_dbg("e1000_write_eeprom_microwire");

	e1000_shift_out_ee_bits(hw, EEPROM_EWEN_OPCODE_MICROWIRE,
				(u16) (eeprom->opcode_bits + 2));

	e1000_shift_out_ee_bits(hw, 0, (u16) (eeprom->address_bits - 2));

	
	e1000_standby_eeprom(hw);

	while (words_written < words) {
		
		e1000_shift_out_ee_bits(hw, EEPROM_WRITE_OPCODE_MICROWIRE,
					eeprom->opcode_bits);

		e1000_shift_out_ee_bits(hw, (u16) (offset + words_written),
					eeprom->address_bits);

		
		e1000_shift_out_ee_bits(hw, data[words_written], 16);

		e1000_standby_eeprom(hw);

		for (i = 0; i < 200; i++) {
			eecd = er32(EECD);
			if (eecd & E1000_EECD_DO)
				break;
			udelay(50);
		}
		if (i == 200) {
			e_dbg("EEPROM Write did not complete\n");
			return -E1000_ERR_EEPROM;
		}

		
		e1000_standby_eeprom(hw);

		words_written++;
	}

	e1000_shift_out_ee_bits(hw, EEPROM_EWDS_OPCODE_MICROWIRE,
				(u16) (eeprom->opcode_bits + 2));

	e1000_shift_out_ee_bits(hw, 0, (u16) (eeprom->address_bits - 2));

	return E1000_SUCCESS;
}

s32 e1000_read_mac_addr(struct e1000_hw *hw)
{
	u16 offset;
	u16 eeprom_data, i;

	e_dbg("e1000_read_mac_addr");

	for (i = 0; i < NODE_ADDRESS_SIZE; i += 2) {
		offset = i >> 1;
		if (e1000_read_eeprom(hw, offset, 1, &eeprom_data) < 0) {
			e_dbg("EEPROM Read Error\n");
			return -E1000_ERR_EEPROM;
		}
		hw->perm_mac_addr[i] = (u8) (eeprom_data & 0x00FF);
		hw->perm_mac_addr[i + 1] = (u8) (eeprom_data >> 8);
	}

	switch (hw->mac_type) {
	default:
		break;
	case e1000_82546:
	case e1000_82546_rev_3:
		if (er32(STATUS) & E1000_STATUS_FUNC_1)
			hw->perm_mac_addr[5] ^= 0x01;
		break;
	}

	for (i = 0; i < NODE_ADDRESS_SIZE; i++)
		hw->mac_addr[i] = hw->perm_mac_addr[i];
	return E1000_SUCCESS;
}

static void e1000_init_rx_addrs(struct e1000_hw *hw)
{
	u32 i;
	u32 rar_num;

	e_dbg("e1000_init_rx_addrs");

	
	e_dbg("Programming MAC Address into RAR[0]\n");

	e1000_rar_set(hw, hw->mac_addr, 0);

	rar_num = E1000_RAR_ENTRIES;

	
	e_dbg("Clearing RAR[1-15]\n");
	for (i = 1; i < rar_num; i++) {
		E1000_WRITE_REG_ARRAY(hw, RA, (i << 1), 0);
		E1000_WRITE_FLUSH();
		E1000_WRITE_REG_ARRAY(hw, RA, ((i << 1) + 1), 0);
		E1000_WRITE_FLUSH();
	}
}

u32 e1000_hash_mc_addr(struct e1000_hw *hw, u8 *mc_addr)
{
	u32 hash_value = 0;

	switch (hw->mc_filter_type) {
	case 0:
		
		hash_value = ((mc_addr[4] >> 4) | (((u16) mc_addr[5]) << 4));
		break;
	case 1:
		
		hash_value = ((mc_addr[4] >> 3) | (((u16) mc_addr[5]) << 5));
		break;
	case 2:
		
		hash_value = ((mc_addr[4] >> 2) | (((u16) mc_addr[5]) << 6));
		break;
	case 3:
		
		hash_value = ((mc_addr[4]) | (((u16) mc_addr[5]) << 8));
		break;
	}

	hash_value &= 0xFFF;
	return hash_value;
}

void e1000_rar_set(struct e1000_hw *hw, u8 *addr, u32 index)
{
	u32 rar_low, rar_high;

	rar_low = ((u32) addr[0] | ((u32) addr[1] << 8) |
		   ((u32) addr[2] << 16) | ((u32) addr[3] << 24));
	rar_high = ((u32) addr[4] | ((u32) addr[5] << 8));

	switch (hw->mac_type) {
	default:
		
		rar_high |= E1000_RAH_AV;
		break;
	}

	E1000_WRITE_REG_ARRAY(hw, RA, (index << 1), rar_low);
	E1000_WRITE_FLUSH();
	E1000_WRITE_REG_ARRAY(hw, RA, ((index << 1) + 1), rar_high);
	E1000_WRITE_FLUSH();
}

void e1000_write_vfta(struct e1000_hw *hw, u32 offset, u32 value)
{
	u32 temp;

	if ((hw->mac_type == e1000_82544) && ((offset & 0x1) == 1)) {
		temp = E1000_READ_REG_ARRAY(hw, VFTA, (offset - 1));
		E1000_WRITE_REG_ARRAY(hw, VFTA, offset, value);
		E1000_WRITE_FLUSH();
		E1000_WRITE_REG_ARRAY(hw, VFTA, (offset - 1), temp);
		E1000_WRITE_FLUSH();
	} else {
		E1000_WRITE_REG_ARRAY(hw, VFTA, offset, value);
		E1000_WRITE_FLUSH();
	}
}

static void e1000_clear_vfta(struct e1000_hw *hw)
{
	u32 offset;
	u32 vfta_value = 0;
	u32 vfta_offset = 0;
	u32 vfta_bit_in_reg = 0;

	for (offset = 0; offset < E1000_VLAN_FILTER_TBL_SIZE; offset++) {
		vfta_value = (offset == vfta_offset) ? vfta_bit_in_reg : 0;
		E1000_WRITE_REG_ARRAY(hw, VFTA, offset, vfta_value);
		E1000_WRITE_FLUSH();
	}
}

static s32 e1000_id_led_init(struct e1000_hw *hw)
{
	u32 ledctl;
	const u32 ledctl_mask = 0x000000FF;
	const u32 ledctl_on = E1000_LEDCTL_MODE_LED_ON;
	const u32 ledctl_off = E1000_LEDCTL_MODE_LED_OFF;
	u16 eeprom_data, i, temp;
	const u16 led_mask = 0x0F;

	e_dbg("e1000_id_led_init");

	if (hw->mac_type < e1000_82540) {
		
		return E1000_SUCCESS;
	}

	ledctl = er32(LEDCTL);
	hw->ledctl_default = ledctl;
	hw->ledctl_mode1 = hw->ledctl_default;
	hw->ledctl_mode2 = hw->ledctl_default;

	if (e1000_read_eeprom(hw, EEPROM_ID_LED_SETTINGS, 1, &eeprom_data) < 0) {
		e_dbg("EEPROM Read Error\n");
		return -E1000_ERR_EEPROM;
	}

	if ((eeprom_data == ID_LED_RESERVED_0000) ||
	    (eeprom_data == ID_LED_RESERVED_FFFF)) {
		eeprom_data = ID_LED_DEFAULT;
	}

	for (i = 0; i < 4; i++) {
		temp = (eeprom_data >> (i << 2)) & led_mask;
		switch (temp) {
		case ID_LED_ON1_DEF2:
		case ID_LED_ON1_ON2:
		case ID_LED_ON1_OFF2:
			hw->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			hw->ledctl_mode1 |= ledctl_on << (i << 3);
			break;
		case ID_LED_OFF1_DEF2:
		case ID_LED_OFF1_ON2:
		case ID_LED_OFF1_OFF2:
			hw->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			hw->ledctl_mode1 |= ledctl_off << (i << 3);
			break;
		default:
			
			break;
		}
		switch (temp) {
		case ID_LED_DEF1_ON2:
		case ID_LED_ON1_ON2:
		case ID_LED_OFF1_ON2:
			hw->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			hw->ledctl_mode2 |= ledctl_on << (i << 3);
			break;
		case ID_LED_DEF1_OFF2:
		case ID_LED_ON1_OFF2:
		case ID_LED_OFF1_OFF2:
			hw->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			hw->ledctl_mode2 |= ledctl_off << (i << 3);
			break;
		default:
			
			break;
		}
	}
	return E1000_SUCCESS;
}

s32 e1000_setup_led(struct e1000_hw *hw)
{
	u32 ledctl;
	s32 ret_val = E1000_SUCCESS;

	e_dbg("e1000_setup_led");

	switch (hw->mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
	case e1000_82543:
	case e1000_82544:
		
		break;
	case e1000_82541:
	case e1000_82547:
	case e1000_82541_rev_2:
	case e1000_82547_rev_2:
		
		ret_val = e1000_read_phy_reg(hw, IGP01E1000_GMII_FIFO,
					     &hw->phy_spd_default);
		if (ret_val)
			return ret_val;
		ret_val = e1000_write_phy_reg(hw, IGP01E1000_GMII_FIFO,
					      (u16) (hw->phy_spd_default &
						     ~IGP01E1000_GMII_SPD));
		if (ret_val)
			return ret_val;
		
	default:
		if (hw->media_type == e1000_media_type_fiber) {
			ledctl = er32(LEDCTL);
			
			hw->ledctl_default = ledctl;
			
			ledctl &= ~(E1000_LEDCTL_LED0_IVRT |
				    E1000_LEDCTL_LED0_BLINK |
				    E1000_LEDCTL_LED0_MODE_MASK);
			ledctl |= (E1000_LEDCTL_MODE_LED_OFF <<
				   E1000_LEDCTL_LED0_MODE_SHIFT);
			ew32(LEDCTL, ledctl);
		} else if (hw->media_type == e1000_media_type_copper)
			ew32(LEDCTL, hw->ledctl_mode1);
		break;
	}

	return E1000_SUCCESS;
}

s32 e1000_cleanup_led(struct e1000_hw *hw)
{
	s32 ret_val = E1000_SUCCESS;

	e_dbg("e1000_cleanup_led");

	switch (hw->mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
	case e1000_82543:
	case e1000_82544:
		
		break;
	case e1000_82541:
	case e1000_82547:
	case e1000_82541_rev_2:
	case e1000_82547_rev_2:
		
		ret_val = e1000_write_phy_reg(hw, IGP01E1000_GMII_FIFO,
					      hw->phy_spd_default);
		if (ret_val)
			return ret_val;
		
	default:
		
		ew32(LEDCTL, hw->ledctl_default);
		break;
	}

	return E1000_SUCCESS;
}

s32 e1000_led_on(struct e1000_hw *hw)
{
	u32 ctrl = er32(CTRL);

	e_dbg("e1000_led_on");

	switch (hw->mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
	case e1000_82543:
		
		ctrl |= E1000_CTRL_SWDPIN0;
		ctrl |= E1000_CTRL_SWDPIO0;
		break;
	case e1000_82544:
		if (hw->media_type == e1000_media_type_fiber) {
			
			ctrl |= E1000_CTRL_SWDPIN0;
			ctrl |= E1000_CTRL_SWDPIO0;
		} else {
			
			ctrl &= ~E1000_CTRL_SWDPIN0;
			ctrl |= E1000_CTRL_SWDPIO0;
		}
		break;
	default:
		if (hw->media_type == e1000_media_type_fiber) {
			
			ctrl &= ~E1000_CTRL_SWDPIN0;
			ctrl |= E1000_CTRL_SWDPIO0;
		} else if (hw->media_type == e1000_media_type_copper) {
			ew32(LEDCTL, hw->ledctl_mode2);
			return E1000_SUCCESS;
		}
		break;
	}

	ew32(CTRL, ctrl);

	return E1000_SUCCESS;
}

s32 e1000_led_off(struct e1000_hw *hw)
{
	u32 ctrl = er32(CTRL);

	e_dbg("e1000_led_off");

	switch (hw->mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
	case e1000_82543:
		
		ctrl &= ~E1000_CTRL_SWDPIN0;
		ctrl |= E1000_CTRL_SWDPIO0;
		break;
	case e1000_82544:
		if (hw->media_type == e1000_media_type_fiber) {
			
			ctrl &= ~E1000_CTRL_SWDPIN0;
			ctrl |= E1000_CTRL_SWDPIO0;
		} else {
			
			ctrl |= E1000_CTRL_SWDPIN0;
			ctrl |= E1000_CTRL_SWDPIO0;
		}
		break;
	default:
		if (hw->media_type == e1000_media_type_fiber) {
			
			ctrl |= E1000_CTRL_SWDPIN0;
			ctrl |= E1000_CTRL_SWDPIO0;
		} else if (hw->media_type == e1000_media_type_copper) {
			ew32(LEDCTL, hw->ledctl_mode1);
			return E1000_SUCCESS;
		}
		break;
	}

	ew32(CTRL, ctrl);

	return E1000_SUCCESS;
}

static void e1000_clear_hw_cntrs(struct e1000_hw *hw)
{
	volatile u32 temp;

	temp = er32(CRCERRS);
	temp = er32(SYMERRS);
	temp = er32(MPC);
	temp = er32(SCC);
	temp = er32(ECOL);
	temp = er32(MCC);
	temp = er32(LATECOL);
	temp = er32(COLC);
	temp = er32(DC);
	temp = er32(SEC);
	temp = er32(RLEC);
	temp = er32(XONRXC);
	temp = er32(XONTXC);
	temp = er32(XOFFRXC);
	temp = er32(XOFFTXC);
	temp = er32(FCRUC);

	temp = er32(PRC64);
	temp = er32(PRC127);
	temp = er32(PRC255);
	temp = er32(PRC511);
	temp = er32(PRC1023);
	temp = er32(PRC1522);

	temp = er32(GPRC);
	temp = er32(BPRC);
	temp = er32(MPRC);
	temp = er32(GPTC);
	temp = er32(GORCL);
	temp = er32(GORCH);
	temp = er32(GOTCL);
	temp = er32(GOTCH);
	temp = er32(RNBC);
	temp = er32(RUC);
	temp = er32(RFC);
	temp = er32(ROC);
	temp = er32(RJC);
	temp = er32(TORL);
	temp = er32(TORH);
	temp = er32(TOTL);
	temp = er32(TOTH);
	temp = er32(TPR);
	temp = er32(TPT);

	temp = er32(PTC64);
	temp = er32(PTC127);
	temp = er32(PTC255);
	temp = er32(PTC511);
	temp = er32(PTC1023);
	temp = er32(PTC1522);

	temp = er32(MPTC);
	temp = er32(BPTC);

	if (hw->mac_type < e1000_82543)
		return;

	temp = er32(ALGNERRC);
	temp = er32(RXERRC);
	temp = er32(TNCRS);
	temp = er32(CEXTERR);
	temp = er32(TSCTC);
	temp = er32(TSCTFC);

	if (hw->mac_type <= e1000_82544)
		return;

	temp = er32(MGTPRC);
	temp = er32(MGTPDC);
	temp = er32(MGTPTC);
}

void e1000_reset_adaptive(struct e1000_hw *hw)
{
	e_dbg("e1000_reset_adaptive");

	if (hw->adaptive_ifs) {
		if (!hw->ifs_params_forced) {
			hw->current_ifs_val = 0;
			hw->ifs_min_val = IFS_MIN;
			hw->ifs_max_val = IFS_MAX;
			hw->ifs_step_size = IFS_STEP;
			hw->ifs_ratio = IFS_RATIO;
		}
		hw->in_ifs_mode = false;
		ew32(AIT, 0);
	} else {
		e_dbg("Not in Adaptive IFS mode!\n");
	}
}

void e1000_update_adaptive(struct e1000_hw *hw)
{
	e_dbg("e1000_update_adaptive");

	if (hw->adaptive_ifs) {
		if ((hw->collision_delta *hw->ifs_ratio) > hw->tx_packet_delta) {
			if (hw->tx_packet_delta > MIN_NUM_XMITS) {
				hw->in_ifs_mode = true;
				if (hw->current_ifs_val < hw->ifs_max_val) {
					if (hw->current_ifs_val == 0)
						hw->current_ifs_val =
						    hw->ifs_min_val;
					else
						hw->current_ifs_val +=
						    hw->ifs_step_size;
					ew32(AIT, hw->current_ifs_val);
				}
			}
		} else {
			if (hw->in_ifs_mode
			    && (hw->tx_packet_delta <= MIN_NUM_XMITS)) {
				hw->current_ifs_val = 0;
				hw->in_ifs_mode = false;
				ew32(AIT, 0);
			}
		}
	} else {
		e_dbg("Not in Adaptive IFS mode!\n");
	}
}

void e1000_tbi_adjust_stats(struct e1000_hw *hw, struct e1000_hw_stats *stats,
			    u32 frame_len, u8 *mac_addr)
{
	u64 carry_bit;

	
	frame_len--;
	
	stats->crcerrs--;
	
	stats->gprc++;

	
	carry_bit = 0x80000000 & stats->gorcl;
	stats->gorcl += frame_len;
	if (carry_bit && ((stats->gorcl & 0x80000000) == 0))
		stats->gorch++;
	if ((mac_addr[0] == (u8) 0xff) && (mac_addr[1] == (u8) 0xff))
		
		stats->bprc++;
	else if (*mac_addr & 0x01)
		
		stats->mprc++;

	if (frame_len == hw->max_frame_size) {
		if (stats->roc > 0)
			stats->roc--;
	}

	if (frame_len == 64) {
		stats->prc64++;
		stats->prc127--;
	} else if (frame_len == 127) {
		stats->prc127++;
		stats->prc255--;
	} else if (frame_len == 255) {
		stats->prc255++;
		stats->prc511--;
	} else if (frame_len == 511) {
		stats->prc511++;
		stats->prc1023--;
	} else if (frame_len == 1023) {
		stats->prc1023++;
		stats->prc1522--;
	} else if (frame_len == 1522) {
		stats->prc1522++;
	}
}

void e1000_get_bus_info(struct e1000_hw *hw)
{
	u32 status;

	switch (hw->mac_type) {
	case e1000_82542_rev2_0:
	case e1000_82542_rev2_1:
		hw->bus_type = e1000_bus_type_pci;
		hw->bus_speed = e1000_bus_speed_unknown;
		hw->bus_width = e1000_bus_width_unknown;
		break;
	default:
		status = er32(STATUS);
		hw->bus_type = (status & E1000_STATUS_PCIX_MODE) ?
		    e1000_bus_type_pcix : e1000_bus_type_pci;

		if (hw->device_id == E1000_DEV_ID_82546EB_QUAD_COPPER) {
			hw->bus_speed = (hw->bus_type == e1000_bus_type_pci) ?
			    e1000_bus_speed_66 : e1000_bus_speed_120;
		} else if (hw->bus_type == e1000_bus_type_pci) {
			hw->bus_speed = (status & E1000_STATUS_PCI66) ?
			    e1000_bus_speed_66 : e1000_bus_speed_33;
		} else {
			switch (status & E1000_STATUS_PCIX_SPEED) {
			case E1000_STATUS_PCIX_SPEED_66:
				hw->bus_speed = e1000_bus_speed_66;
				break;
			case E1000_STATUS_PCIX_SPEED_100:
				hw->bus_speed = e1000_bus_speed_100;
				break;
			case E1000_STATUS_PCIX_SPEED_133:
				hw->bus_speed = e1000_bus_speed_133;
				break;
			default:
				hw->bus_speed = e1000_bus_speed_reserved;
				break;
			}
		}
		hw->bus_width = (status & E1000_STATUS_BUS64) ?
		    e1000_bus_width_64 : e1000_bus_width_32;
		break;
	}
}

static void e1000_write_reg_io(struct e1000_hw *hw, u32 offset, u32 value)
{
	unsigned long io_addr = hw->io_base;
	unsigned long io_data = hw->io_base + 4;

	e1000_io_write(hw, io_addr, offset);
	e1000_io_write(hw, io_data, value);
}

static s32 e1000_get_cable_length(struct e1000_hw *hw, u16 *min_length,
				  u16 *max_length)
{
	s32 ret_val;
	u16 agc_value = 0;
	u16 i, phy_data;
	u16 cable_length;

	e_dbg("e1000_get_cable_length");

	*min_length = *max_length = 0;

	
	if (hw->phy_type == e1000_phy_m88) {

		ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_STATUS,
					     &phy_data);
		if (ret_val)
			return ret_val;
		cable_length = (phy_data & M88E1000_PSSR_CABLE_LENGTH) >>
		    M88E1000_PSSR_CABLE_LENGTH_SHIFT;

		
		switch (cable_length) {
		case e1000_cable_length_50:
			*min_length = 0;
			*max_length = e1000_igp_cable_length_50;
			break;
		case e1000_cable_length_50_80:
			*min_length = e1000_igp_cable_length_50;
			*max_length = e1000_igp_cable_length_80;
			break;
		case e1000_cable_length_80_110:
			*min_length = e1000_igp_cable_length_80;
			*max_length = e1000_igp_cable_length_110;
			break;
		case e1000_cable_length_110_140:
			*min_length = e1000_igp_cable_length_110;
			*max_length = e1000_igp_cable_length_140;
			break;
		case e1000_cable_length_140:
			*min_length = e1000_igp_cable_length_140;
			*max_length = e1000_igp_cable_length_170;
			break;
		default:
			return -E1000_ERR_PHY;
			break;
		}
	} else if (hw->phy_type == e1000_phy_igp) {	
		u16 cur_agc_value;
		u16 min_agc_value = IGP01E1000_AGC_LENGTH_TABLE_SIZE;
		static const u16 agc_reg_array[IGP01E1000_PHY_CHANNEL_NUM] = {
		       IGP01E1000_PHY_AGC_A,
		       IGP01E1000_PHY_AGC_B,
		       IGP01E1000_PHY_AGC_C,
		       IGP01E1000_PHY_AGC_D
		};
		
		for (i = 0; i < IGP01E1000_PHY_CHANNEL_NUM; i++) {

			ret_val =
			    e1000_read_phy_reg(hw, agc_reg_array[i], &phy_data);
			if (ret_val)
				return ret_val;

			cur_agc_value = phy_data >> IGP01E1000_AGC_LENGTH_SHIFT;

			
			if ((cur_agc_value >=
			     IGP01E1000_AGC_LENGTH_TABLE_SIZE - 1)
			    || (cur_agc_value == 0))
				return -E1000_ERR_PHY;

			agc_value += cur_agc_value;

			
			if (min_agc_value > cur_agc_value)
				min_agc_value = cur_agc_value;
		}

		
		if (agc_value <
		    IGP01E1000_PHY_CHANNEL_NUM * e1000_igp_cable_length_50) {
			agc_value -= min_agc_value;

			
			agc_value /= (IGP01E1000_PHY_CHANNEL_NUM - 1);
		} else {
			
			agc_value /= IGP01E1000_PHY_CHANNEL_NUM;
		}

		
		*min_length = ((e1000_igp_cable_length_table[agc_value] -
				IGP01E1000_AGC_RANGE) > 0) ?
		    (e1000_igp_cable_length_table[agc_value] -
		     IGP01E1000_AGC_RANGE) : 0;
		*max_length = e1000_igp_cable_length_table[agc_value] +
		    IGP01E1000_AGC_RANGE;
	}

	return E1000_SUCCESS;
}

static s32 e1000_check_polarity(struct e1000_hw *hw,
				e1000_rev_polarity *polarity)
{
	s32 ret_val;
	u16 phy_data;

	e_dbg("e1000_check_polarity");

	if (hw->phy_type == e1000_phy_m88) {
		
		ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_STATUS,
					     &phy_data);
		if (ret_val)
			return ret_val;
		*polarity = ((phy_data & M88E1000_PSSR_REV_POLARITY) >>
			     M88E1000_PSSR_REV_POLARITY_SHIFT) ?
		    e1000_rev_polarity_reversed : e1000_rev_polarity_normal;

	} else if (hw->phy_type == e1000_phy_igp) {
		
		ret_val = e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_STATUS,
					     &phy_data);
		if (ret_val)
			return ret_val;

		if ((phy_data & IGP01E1000_PSSR_SPEED_MASK) ==
		    IGP01E1000_PSSR_SPEED_1000MBPS) {

			
			ret_val =
			    e1000_read_phy_reg(hw, IGP01E1000_PHY_PCS_INIT_REG,
					       &phy_data);
			if (ret_val)
				return ret_val;

			
			*polarity = (phy_data & IGP01E1000_PHY_POLARITY_MASK) ?
			    e1000_rev_polarity_reversed :
			    e1000_rev_polarity_normal;
		} else {
			*polarity =
			    (phy_data & IGP01E1000_PSSR_POLARITY_REVERSED) ?
			    e1000_rev_polarity_reversed :
			    e1000_rev_polarity_normal;
		}
	}
	return E1000_SUCCESS;
}

static s32 e1000_check_downshift(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_data;

	e_dbg("e1000_check_downshift");

	if (hw->phy_type == e1000_phy_igp) {
		ret_val = e1000_read_phy_reg(hw, IGP01E1000_PHY_LINK_HEALTH,
					     &phy_data);
		if (ret_val)
			return ret_val;

		hw->speed_downgraded =
		    (phy_data & IGP01E1000_PLHR_SS_DOWNGRADE) ? 1 : 0;
	} else if (hw->phy_type == e1000_phy_m88) {
		ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_SPEC_STATUS,
					     &phy_data);
		if (ret_val)
			return ret_val;

		hw->speed_downgraded = (phy_data & M88E1000_PSSR_DOWNSHIFT) >>
		    M88E1000_PSSR_DOWNSHIFT_SHIFT;
	}

	return E1000_SUCCESS;
}

static const u16 dsp_reg_array[IGP01E1000_PHY_CHANNEL_NUM] = {
	IGP01E1000_PHY_AGC_PARAM_A,
	IGP01E1000_PHY_AGC_PARAM_B,
	IGP01E1000_PHY_AGC_PARAM_C,
	IGP01E1000_PHY_AGC_PARAM_D
};

static s32 e1000_1000Mb_check_cable_length(struct e1000_hw *hw)
{
	u16 min_length, max_length;
	u16 phy_data, i;
	s32 ret_val;

	ret_val = e1000_get_cable_length(hw, &min_length, &max_length);
	if (ret_val)
		return ret_val;

	if (hw->dsp_config_state != e1000_dsp_config_enabled)
		return 0;

	if (min_length >= e1000_igp_cable_length_50) {
		for (i = 0; i < IGP01E1000_PHY_CHANNEL_NUM; i++) {
			ret_val = e1000_read_phy_reg(hw, dsp_reg_array[i],
						     &phy_data);
			if (ret_val)
				return ret_val;

			phy_data &= ~IGP01E1000_PHY_EDAC_MU_INDEX;

			ret_val = e1000_write_phy_reg(hw, dsp_reg_array[i],
						      phy_data);
			if (ret_val)
				return ret_val;
		}
		hw->dsp_config_state = e1000_dsp_config_activated;
	} else {
		u16 ffe_idle_err_timeout = FFE_IDLE_ERR_COUNT_TIMEOUT_20;
		u32 idle_errs = 0;

		
		ret_val = e1000_read_phy_reg(hw, PHY_1000T_STATUS, &phy_data);
		if (ret_val)
			return ret_val;

		for (i = 0; i < ffe_idle_err_timeout; i++) {
			udelay(1000);
			ret_val = e1000_read_phy_reg(hw, PHY_1000T_STATUS,
						     &phy_data);
			if (ret_val)
				return ret_val;

			idle_errs += (phy_data & SR_1000T_IDLE_ERROR_CNT);
			if (idle_errs > SR_1000T_PHY_EXCESSIVE_IDLE_ERR_COUNT) {
				hw->ffe_config_state = e1000_ffe_config_active;

				ret_val = e1000_write_phy_reg(hw,
					      IGP01E1000_PHY_DSP_FFE,
					      IGP01E1000_PHY_DSP_FFE_CM_CP);
				if (ret_val)
					return ret_val;
				break;
			}

			if (idle_errs)
				ffe_idle_err_timeout =
					    FFE_IDLE_ERR_COUNT_TIMEOUT_100;
		}
	}

	return 0;
}


static s32 e1000_config_dsp_after_link_change(struct e1000_hw *hw, bool link_up)
{
	s32 ret_val;
	u16 phy_data, phy_saved_data, speed, duplex, i;

	e_dbg("e1000_config_dsp_after_link_change");

	if (hw->phy_type != e1000_phy_igp)
		return E1000_SUCCESS;

	if (link_up) {
		ret_val = e1000_get_speed_and_duplex(hw, &speed, &duplex);
		if (ret_val) {
			e_dbg("Error getting link speed and duplex\n");
			return ret_val;
		}

		if (speed == SPEED_1000) {
			ret_val = e1000_1000Mb_check_cable_length(hw);
			if (ret_val)
				return ret_val;
		}
	} else {
		if (hw->dsp_config_state == e1000_dsp_config_activated) {
			ret_val =
			    e1000_read_phy_reg(hw, 0x2F5B, &phy_saved_data);

			if (ret_val)
				return ret_val;

			
			ret_val = e1000_write_phy_reg(hw, 0x2F5B, 0x0003);

			if (ret_val)
				return ret_val;

			msleep(20);

			ret_val = e1000_write_phy_reg(hw, 0x0000,
						      IGP01E1000_IEEE_FORCE_GIGA);
			if (ret_val)
				return ret_val;
			for (i = 0; i < IGP01E1000_PHY_CHANNEL_NUM; i++) {
				ret_val =
				    e1000_read_phy_reg(hw, dsp_reg_array[i],
						       &phy_data);
				if (ret_val)
					return ret_val;

				phy_data &= ~IGP01E1000_PHY_EDAC_MU_INDEX;
				phy_data |= IGP01E1000_PHY_EDAC_SIGN_EXT_9_BITS;

				ret_val =
				    e1000_write_phy_reg(hw, dsp_reg_array[i],
							phy_data);
				if (ret_val)
					return ret_val;
			}

			ret_val = e1000_write_phy_reg(hw, 0x0000,
						      IGP01E1000_IEEE_RESTART_AUTONEG);
			if (ret_val)
				return ret_val;

			msleep(20);

			
			ret_val =
			    e1000_write_phy_reg(hw, 0x2F5B, phy_saved_data);

			if (ret_val)
				return ret_val;

			hw->dsp_config_state = e1000_dsp_config_enabled;
		}

		if (hw->ffe_config_state == e1000_ffe_config_active) {
			ret_val =
			    e1000_read_phy_reg(hw, 0x2F5B, &phy_saved_data);

			if (ret_val)
				return ret_val;

			
			ret_val = e1000_write_phy_reg(hw, 0x2F5B, 0x0003);

			if (ret_val)
				return ret_val;

			msleep(20);

			ret_val = e1000_write_phy_reg(hw, 0x0000,
						      IGP01E1000_IEEE_FORCE_GIGA);
			if (ret_val)
				return ret_val;
			ret_val =
			    e1000_write_phy_reg(hw, IGP01E1000_PHY_DSP_FFE,
						IGP01E1000_PHY_DSP_FFE_DEFAULT);
			if (ret_val)
				return ret_val;

			ret_val = e1000_write_phy_reg(hw, 0x0000,
						      IGP01E1000_IEEE_RESTART_AUTONEG);
			if (ret_val)
				return ret_val;

			msleep(20);

			
			ret_val =
			    e1000_write_phy_reg(hw, 0x2F5B, phy_saved_data);

			if (ret_val)
				return ret_val;

			hw->ffe_config_state = e1000_ffe_config_enabled;
		}
	}
	return E1000_SUCCESS;
}

static s32 e1000_set_phy_mode(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 eeprom_data;

	e_dbg("e1000_set_phy_mode");

	if ((hw->mac_type == e1000_82545_rev_3) &&
	    (hw->media_type == e1000_media_type_copper)) {
		ret_val =
		    e1000_read_eeprom(hw, EEPROM_PHY_CLASS_WORD, 1,
				      &eeprom_data);
		if (ret_val) {
			return ret_val;
		}

		if ((eeprom_data != EEPROM_RESERVED_WORD) &&
		    (eeprom_data & EEPROM_PHY_CLASS_A)) {
			ret_val =
			    e1000_write_phy_reg(hw, M88E1000_PHY_PAGE_SELECT,
						0x000B);
			if (ret_val)
				return ret_val;
			ret_val =
			    e1000_write_phy_reg(hw, M88E1000_PHY_GEN_CONTROL,
						0x8104);
			if (ret_val)
				return ret_val;

			hw->phy_reset_disable = false;
		}
	}

	return E1000_SUCCESS;
}

static s32 e1000_set_d3_lplu_state(struct e1000_hw *hw, bool active)
{
	s32 ret_val;
	u16 phy_data;
	e_dbg("e1000_set_d3_lplu_state");

	if (hw->phy_type != e1000_phy_igp)
		return E1000_SUCCESS;

	if (hw->mac_type == e1000_82541_rev_2
	    || hw->mac_type == e1000_82547_rev_2) {
		ret_val =
		    e1000_read_phy_reg(hw, IGP01E1000_GMII_FIFO, &phy_data);
		if (ret_val)
			return ret_val;
	}

	if (!active) {
		if (hw->mac_type == e1000_82541_rev_2 ||
		    hw->mac_type == e1000_82547_rev_2) {
			phy_data &= ~IGP01E1000_GMII_FLEX_SPD;
			ret_val =
			    e1000_write_phy_reg(hw, IGP01E1000_GMII_FIFO,
						phy_data);
			if (ret_val)
				return ret_val;
		}

		if (hw->smart_speed == e1000_smart_speed_on) {
			ret_val =
			    e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
					       &phy_data);
			if (ret_val)
				return ret_val;

			phy_data |= IGP01E1000_PSCFR_SMART_SPEED;
			ret_val =
			    e1000_write_phy_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
						phy_data);
			if (ret_val)
				return ret_val;
		} else if (hw->smart_speed == e1000_smart_speed_off) {
			ret_val =
			    e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
					       &phy_data);
			if (ret_val)
				return ret_val;

			phy_data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val =
			    e1000_write_phy_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
						phy_data);
			if (ret_val)
				return ret_val;
		}
	} else if ((hw->autoneg_advertised == AUTONEG_ADVERTISE_SPEED_DEFAULT)
		   || (hw->autoneg_advertised == AUTONEG_ADVERTISE_10_ALL)
		   || (hw->autoneg_advertised ==
		       AUTONEG_ADVERTISE_10_100_ALL)) {

		if (hw->mac_type == e1000_82541_rev_2 ||
		    hw->mac_type == e1000_82547_rev_2) {
			phy_data |= IGP01E1000_GMII_FLEX_SPD;
			ret_val =
			    e1000_write_phy_reg(hw, IGP01E1000_GMII_FIFO,
						phy_data);
			if (ret_val)
				return ret_val;
		}

		
		ret_val =
		    e1000_read_phy_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
				       &phy_data);
		if (ret_val)
			return ret_val;

		phy_data &= ~IGP01E1000_PSCFR_SMART_SPEED;
		ret_val =
		    e1000_write_phy_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
					phy_data);
		if (ret_val)
			return ret_val;

	}
	return E1000_SUCCESS;
}

static s32 e1000_set_vco_speed(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 default_page = 0;
	u16 phy_data;

	e_dbg("e1000_set_vco_speed");

	switch (hw->mac_type) {
	case e1000_82545_rev_3:
	case e1000_82546_rev_3:
		break;
	default:
		return E1000_SUCCESS;
	}

	

	ret_val =
	    e1000_read_phy_reg(hw, M88E1000_PHY_PAGE_SELECT, &default_page);
	if (ret_val)
		return ret_val;

	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_PAGE_SELECT, 0x0005);
	if (ret_val)
		return ret_val;

	ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_GEN_CONTROL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data &= ~M88E1000_PHY_VCO_REG_BIT8;
	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_GEN_CONTROL, phy_data);
	if (ret_val)
		return ret_val;

	

	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_PAGE_SELECT, 0x0004);
	if (ret_val)
		return ret_val;

	ret_val = e1000_read_phy_reg(hw, M88E1000_PHY_GEN_CONTROL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data |= M88E1000_PHY_VCO_REG_BIT11;
	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_GEN_CONTROL, phy_data);
	if (ret_val)
		return ret_val;

	ret_val =
	    e1000_write_phy_reg(hw, M88E1000_PHY_PAGE_SELECT, default_page);
	if (ret_val)
		return ret_val;

	return E1000_SUCCESS;
}


u32 e1000_enable_mng_pass_thru(struct e1000_hw *hw)
{
	u32 manc;

	if (hw->asf_firmware_present) {
		manc = er32(MANC);

		if (!(manc & E1000_MANC_RCV_TCO_EN) ||
		    !(manc & E1000_MANC_EN_MAC_ADDR_FILTER))
			return false;
		if ((manc & E1000_MANC_SMBUS_EN) && !(manc & E1000_MANC_ASF_EN))
			return true;
	}
	return false;
}

static s32 e1000_polarity_reversal_workaround(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 mii_status_reg;
	u16 i;

	

	

	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_PAGE_SELECT, 0x0019);
	if (ret_val)
		return ret_val;
	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_GEN_CONTROL, 0xFFFF);
	if (ret_val)
		return ret_val;

	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_PAGE_SELECT, 0x0000);
	if (ret_val)
		return ret_val;

	
	for (i = PHY_FORCE_TIME; i > 0; i--) {

		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;

		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;

		if ((mii_status_reg & ~MII_SR_LINK_STATUS) == 0)
			break;
		msleep(100);
	}

	
	msleep(1000);

	

	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_PAGE_SELECT, 0x0019);
	if (ret_val)
		return ret_val;
	msleep(50);
	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_GEN_CONTROL, 0xFFF0);
	if (ret_val)
		return ret_val;
	msleep(50);
	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_GEN_CONTROL, 0xFF00);
	if (ret_val)
		return ret_val;
	msleep(50);
	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_GEN_CONTROL, 0x0000);
	if (ret_val)
		return ret_val;

	ret_val = e1000_write_phy_reg(hw, M88E1000_PHY_PAGE_SELECT, 0x0000);
	if (ret_val)
		return ret_val;

	
	for (i = PHY_FORCE_TIME; i > 0; i--) {

		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;

		ret_val = e1000_read_phy_reg(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;

		if (mii_status_reg & MII_SR_LINK_STATUS)
			break;
		msleep(100);
	}
	return E1000_SUCCESS;
}

static s32 e1000_get_auto_rd_done(struct e1000_hw *hw)
{
	e_dbg("e1000_get_auto_rd_done");
	msleep(5);
	return E1000_SUCCESS;
}

static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw)
{
	e_dbg("e1000_get_phy_cfg_done");
	msleep(10);
	return E1000_SUCCESS;
}
