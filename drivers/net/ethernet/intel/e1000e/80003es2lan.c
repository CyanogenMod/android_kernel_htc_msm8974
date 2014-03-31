/*******************************************************************************

  Intel PRO/1000 Linux driver
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


#include "e1000.h"

#define E1000_KMRNCTRLSTA_OFFSET_FIFO_CTRL	 0x00
#define E1000_KMRNCTRLSTA_OFFSET_INB_CTRL	 0x02
#define E1000_KMRNCTRLSTA_OFFSET_HD_CTRL	 0x10
#define E1000_KMRNCTRLSTA_OFFSET_MAC2PHY_OPMODE	 0x1F

#define E1000_KMRNCTRLSTA_FIFO_CTRL_RX_BYPASS	 0x0008
#define E1000_KMRNCTRLSTA_FIFO_CTRL_TX_BYPASS	 0x0800
#define E1000_KMRNCTRLSTA_INB_CTRL_DIS_PADDING	 0x0010

#define E1000_KMRNCTRLSTA_HD_CTRL_10_100_DEFAULT 0x0004
#define E1000_KMRNCTRLSTA_HD_CTRL_1000_DEFAULT	 0x0000
#define E1000_KMRNCTRLSTA_OPMODE_E_IDLE		 0x2000

#define E1000_KMRNCTRLSTA_OPMODE_MASK		 0x000C
#define E1000_KMRNCTRLSTA_OPMODE_INBAND_MDIO	 0x0004

#define E1000_TCTL_EXT_GCEX_MASK 0x000FFC00 
#define DEFAULT_TCTL_EXT_GCEX_80003ES2LAN	 0x00010000

#define DEFAULT_TIPG_IPGT_1000_80003ES2LAN	 0x8
#define DEFAULT_TIPG_IPGT_10_100_80003ES2LAN	 0x9

#define GG82563_PSCR_POLARITY_REVERSAL_DISABLE	 0x0002 
#define GG82563_PSCR_CROSSOVER_MODE_MASK	 0x0060
#define GG82563_PSCR_CROSSOVER_MODE_MDI		 0x0000 
#define GG82563_PSCR_CROSSOVER_MODE_MDIX	 0x0020 
#define GG82563_PSCR_CROSSOVER_MODE_AUTO	 0x0060 

#define GG82563_PSCR2_REVERSE_AUTO_NEG		 0x2000
						

#define GG82563_MSCR_TX_CLK_MASK		 0x0007
#define GG82563_MSCR_TX_CLK_10MBPS_2_5		 0x0004
#define GG82563_MSCR_TX_CLK_100MBPS_25		 0x0005
#define GG82563_MSCR_TX_CLK_1000MBPS_25		 0x0007

#define GG82563_MSCR_ASSERT_CRS_ON_TX		 0x0010 

#define GG82563_DSPD_CABLE_LENGTH		 0x0007 

#define GG82563_KMCR_PASS_FALSE_CARRIER		 0x0800

#define GG82563_MAX_KMRN_RETRY  0x5

#define GG82563_PMCR_ENABLE_ELECTRICAL_IDLE	 0x0001
					   

#define GG82563_ICR_DIS_PADDING			 0x0010 

static const u16 e1000_gg82563_cable_length_table[] = {
	 0, 60, 115, 150, 150, 60, 115, 150, 180, 180, 0xFF };
#define GG82563_CABLE_LENGTH_TABLE_SIZE \
		ARRAY_SIZE(e1000_gg82563_cable_length_table)

static s32 e1000_setup_copper_link_80003es2lan(struct e1000_hw *hw);
static s32 e1000_acquire_swfw_sync_80003es2lan(struct e1000_hw *hw, u16 mask);
static void e1000_release_swfw_sync_80003es2lan(struct e1000_hw *hw, u16 mask);
static void e1000_initialize_hw_bits_80003es2lan(struct e1000_hw *hw);
static void e1000_clear_hw_cntrs_80003es2lan(struct e1000_hw *hw);
static s32 e1000_cfg_kmrn_1000_80003es2lan(struct e1000_hw *hw);
static s32 e1000_cfg_kmrn_10_100_80003es2lan(struct e1000_hw *hw, u16 duplex);
static s32 e1000_cfg_on_link_up_80003es2lan(struct e1000_hw *hw);
static s32  e1000_read_kmrn_reg_80003es2lan(struct e1000_hw *hw, u32 offset,
                                            u16 *data);
static s32  e1000_write_kmrn_reg_80003es2lan(struct e1000_hw *hw, u32 offset,
                                             u16 data);
static void e1000_power_down_phy_copper_80003es2lan(struct e1000_hw *hw);

static s32 e1000_init_phy_params_80003es2lan(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;

	if (hw->phy.media_type != e1000_media_type_copper) {
		phy->type	= e1000_phy_none;
		return 0;
	} else {
		phy->ops.power_up = e1000_power_up_phy_copper;
		phy->ops.power_down = e1000_power_down_phy_copper_80003es2lan;
	}

	phy->addr		= 1;
	phy->autoneg_mask	= AUTONEG_ADVERTISE_SPEED_DEFAULT;
	phy->reset_delay_us      = 100;
	phy->type		= e1000_phy_gg82563;

	
	ret_val = e1000e_get_phy_id(hw);

	
	if (phy->id != GG82563_E_PHY_ID)
		return -E1000_ERR_PHY;

	return ret_val;
}

static s32 e1000_init_nvm_params_80003es2lan(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = er32(EECD);
	u16 size;

	nvm->opcode_bits	= 8;
	nvm->delay_usec	 = 1;
	switch (nvm->override) {
	case e1000_nvm_override_spi_large:
		nvm->page_size    = 32;
		nvm->address_bits = 16;
		break;
	case e1000_nvm_override_spi_small:
		nvm->page_size    = 8;
		nvm->address_bits = 8;
		break;
	default:
		nvm->page_size    = eecd & E1000_EECD_ADDR_BITS ? 32 : 8;
		nvm->address_bits = eecd & E1000_EECD_ADDR_BITS ? 16 : 8;
		break;
	}

	nvm->type = e1000_nvm_eeprom_spi;

	size = (u16)((eecd & E1000_EECD_SIZE_EX_MASK) >>
			  E1000_EECD_SIZE_EX_SHIFT);

	size += NVM_WORD_SIZE_BASE_SHIFT;

	
	if (size > 14)
		size = 14;
	nvm->word_size	= 1 << size;

	return 0;
}

static s32 e1000_init_mac_params_80003es2lan(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;

	
	switch (hw->adapter->pdev->device) {
	case E1000_DEV_ID_80003ES2LAN_SERDES_DPT:
		hw->phy.media_type = e1000_media_type_internal_serdes;
		mac->ops.check_for_link = e1000e_check_for_serdes_link;
		mac->ops.setup_physical_interface =
		    e1000e_setup_fiber_serdes_link;
		break;
	default:
		hw->phy.media_type = e1000_media_type_copper;
		mac->ops.check_for_link = e1000e_check_for_copper_link;
		mac->ops.setup_physical_interface =
		    e1000_setup_copper_link_80003es2lan;
		break;
	}

	
	mac->mta_reg_count = 128;
	
	mac->rar_entry_count = E1000_RAR_ENTRIES;
	
	mac->has_fwsm = true;
	
	mac->arc_subsystem_valid =
	        (er32(FWSM) & E1000_FWSM_MODE_MASK)
	                ? true : false;
	
	mac->adaptive_ifs = false;

	
	hw->mac.ops.set_lan_id(hw);

	return 0;
}

static s32 e1000_get_variants_80003es2lan(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	s32 rc;

	rc = e1000_init_mac_params_80003es2lan(hw);
	if (rc)
		return rc;

	rc = e1000_init_nvm_params_80003es2lan(hw);
	if (rc)
		return rc;

	rc = e1000_init_phy_params_80003es2lan(hw);
	if (rc)
		return rc;

	return 0;
}

static s32 e1000_acquire_phy_80003es2lan(struct e1000_hw *hw)
{
	u16 mask;

	mask = hw->bus.func ? E1000_SWFW_PHY1_SM : E1000_SWFW_PHY0_SM;
	return e1000_acquire_swfw_sync_80003es2lan(hw, mask);
}

static void e1000_release_phy_80003es2lan(struct e1000_hw *hw)
{
	u16 mask;

	mask = hw->bus.func ? E1000_SWFW_PHY1_SM : E1000_SWFW_PHY0_SM;
	e1000_release_swfw_sync_80003es2lan(hw, mask);
}

static s32 e1000_acquire_mac_csr_80003es2lan(struct e1000_hw *hw)
{
	u16 mask;

	mask = E1000_SWFW_CSR_SM;

	return e1000_acquire_swfw_sync_80003es2lan(hw, mask);
}

static void e1000_release_mac_csr_80003es2lan(struct e1000_hw *hw)
{
	u16 mask;

	mask = E1000_SWFW_CSR_SM;

	e1000_release_swfw_sync_80003es2lan(hw, mask);
}

static s32 e1000_acquire_nvm_80003es2lan(struct e1000_hw *hw)
{
	s32 ret_val;

	ret_val = e1000_acquire_swfw_sync_80003es2lan(hw, E1000_SWFW_EEP_SM);
	if (ret_val)
		return ret_val;

	ret_val = e1000e_acquire_nvm(hw);

	if (ret_val)
		e1000_release_swfw_sync_80003es2lan(hw, E1000_SWFW_EEP_SM);

	return ret_val;
}

static void e1000_release_nvm_80003es2lan(struct e1000_hw *hw)
{
	e1000e_release_nvm(hw);
	e1000_release_swfw_sync_80003es2lan(hw, E1000_SWFW_EEP_SM);
}

static s32 e1000_acquire_swfw_sync_80003es2lan(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;
	u32 swmask = mask;
	u32 fwmask = mask << 16;
	s32 i = 0;
	s32 timeout = 50;

	while (i < timeout) {
		if (e1000e_get_hw_semaphore(hw))
			return -E1000_ERR_SWFW_SYNC;

		swfw_sync = er32(SW_FW_SYNC);
		if (!(swfw_sync & (fwmask | swmask)))
			break;

		e1000e_put_hw_semaphore(hw);
		mdelay(5);
		i++;
	}

	if (i == timeout) {
		e_dbg("Driver can't access resource, SW_FW_SYNC timeout.\n");
		return -E1000_ERR_SWFW_SYNC;
	}

	swfw_sync |= swmask;
	ew32(SW_FW_SYNC, swfw_sync);

	e1000e_put_hw_semaphore(hw);

	return 0;
}

static void e1000_release_swfw_sync_80003es2lan(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;

	while (e1000e_get_hw_semaphore(hw) != 0)
		; 

	swfw_sync = er32(SW_FW_SYNC);
	swfw_sync &= ~mask;
	ew32(SW_FW_SYNC, swfw_sync);

	e1000e_put_hw_semaphore(hw);
}

static s32 e1000_read_phy_reg_gg82563_80003es2lan(struct e1000_hw *hw,
						  u32 offset, u16 *data)
{
	s32 ret_val;
	u32 page_select;
	u16 temp;

	ret_val = e1000_acquire_phy_80003es2lan(hw);
	if (ret_val)
		return ret_val;

	
	if ((offset & MAX_PHY_REG_ADDRESS) < GG82563_MIN_ALT_REG) {
		page_select = GG82563_PHY_PAGE_SELECT;
	} else {
		page_select = GG82563_PHY_PAGE_SELECT_ALT;
	}

	temp = (u16)((u16)offset >> GG82563_PAGE_SHIFT);
	ret_val = e1000e_write_phy_reg_mdic(hw, page_select, temp);
	if (ret_val) {
		e1000_release_phy_80003es2lan(hw);
		return ret_val;
	}

	if (hw->dev_spec.e80003es2lan.mdic_wa_enable) {
		udelay(200);

		
		ret_val = e1000e_read_phy_reg_mdic(hw, page_select, &temp);

		if (((u16)offset >> GG82563_PAGE_SHIFT) != temp) {
			e1000_release_phy_80003es2lan(hw);
			return -E1000_ERR_PHY;
		}

		udelay(200);

		ret_val = e1000e_read_phy_reg_mdic(hw,
		                                  MAX_PHY_REG_ADDRESS & offset,
		                                  data);

		udelay(200);
	} else {
		ret_val = e1000e_read_phy_reg_mdic(hw,
		                                  MAX_PHY_REG_ADDRESS & offset,
		                                  data);
	}

	e1000_release_phy_80003es2lan(hw);

	return ret_val;
}

static s32 e1000_write_phy_reg_gg82563_80003es2lan(struct e1000_hw *hw,
						   u32 offset, u16 data)
{
	s32 ret_val;
	u32 page_select;
	u16 temp;

	ret_val = e1000_acquire_phy_80003es2lan(hw);
	if (ret_val)
		return ret_val;

	
	if ((offset & MAX_PHY_REG_ADDRESS) < GG82563_MIN_ALT_REG) {
		page_select = GG82563_PHY_PAGE_SELECT;
	} else {
		page_select = GG82563_PHY_PAGE_SELECT_ALT;
	}

	temp = (u16)((u16)offset >> GG82563_PAGE_SHIFT);
	ret_val = e1000e_write_phy_reg_mdic(hw, page_select, temp);
	if (ret_val) {
		e1000_release_phy_80003es2lan(hw);
		return ret_val;
	}

	if (hw->dev_spec.e80003es2lan.mdic_wa_enable) {
		udelay(200);

		
		ret_val = e1000e_read_phy_reg_mdic(hw, page_select, &temp);

		if (((u16)offset >> GG82563_PAGE_SHIFT) != temp) {
			e1000_release_phy_80003es2lan(hw);
			return -E1000_ERR_PHY;
		}

		udelay(200);

		ret_val = e1000e_write_phy_reg_mdic(hw,
		                                  MAX_PHY_REG_ADDRESS & offset,
		                                  data);

		udelay(200);
	} else {
		ret_val = e1000e_write_phy_reg_mdic(hw,
		                                  MAX_PHY_REG_ADDRESS & offset,
		                                  data);
	}

	e1000_release_phy_80003es2lan(hw);

	return ret_val;
}

static s32 e1000_write_nvm_80003es2lan(struct e1000_hw *hw, u16 offset,
				       u16 words, u16 *data)
{
	return e1000e_write_nvm_spi(hw, offset, words, data);
}

static s32 e1000_get_cfg_done_80003es2lan(struct e1000_hw *hw)
{
	s32 timeout = PHY_CFG_TIMEOUT;
	u32 mask = E1000_NVM_CFG_DONE_PORT_0;

	if (hw->bus.func == 1)
		mask = E1000_NVM_CFG_DONE_PORT_1;

	while (timeout) {
		if (er32(EEMNGCTL) & mask)
			break;
		usleep_range(1000, 2000);
		timeout--;
	}
	if (!timeout) {
		e_dbg("MNG configuration cycle has not completed.\n");
		return -E1000_ERR_RESET;
	}

	return 0;
}

static s32 e1000_phy_force_speed_duplex_80003es2lan(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_data;
	bool link;

	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data &= ~GG82563_PSCR_CROSSOVER_MODE_AUTO;
	ret_val = e1e_wphy(hw, GG82563_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	e_dbg("GG82563 PSCR: %X\n", phy_data);

	ret_val = e1e_rphy(hw, PHY_CONTROL, &phy_data);
	if (ret_val)
		return ret_val;

	e1000e_phy_force_speed_duplex_setup(hw, &phy_data);

	
	phy_data |= MII_CR_RESET;

	ret_val = e1e_wphy(hw, PHY_CONTROL, phy_data);
	if (ret_val)
		return ret_val;

	udelay(1);

	if (hw->phy.autoneg_wait_to_complete) {
		e_dbg("Waiting for forced speed/duplex link on GG82563 phy.\n");

		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						     100000, &link);
		if (ret_val)
			return ret_val;

		if (!link) {
			ret_val = e1000e_phy_reset_dsp(hw);
			if (ret_val)
				return ret_val;
		}

		
		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						     100000, &link);
		if (ret_val)
			return ret_val;
	}

	ret_val = e1e_rphy(hw, GG82563_PHY_MAC_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data &= ~GG82563_MSCR_TX_CLK_MASK;
	if (hw->mac.forced_speed_duplex & E1000_ALL_10_SPEED)
		phy_data |= GG82563_MSCR_TX_CLK_10MBPS_2_5;
	else
		phy_data |= GG82563_MSCR_TX_CLK_100MBPS_25;

	phy_data |= GG82563_MSCR_ASSERT_CRS_ON_TX;
	ret_val = e1e_wphy(hw, GG82563_PHY_MAC_SPEC_CTRL, phy_data);

	return ret_val;
}

static s32 e1000_get_cable_length_80003es2lan(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val = 0;
	u16 phy_data, index;

	ret_val = e1e_rphy(hw, GG82563_PHY_DSP_DISTANCE, &phy_data);
	if (ret_val)
		return ret_val;

	index = phy_data & GG82563_DSPD_CABLE_LENGTH;

	if (index >= GG82563_CABLE_LENGTH_TABLE_SIZE - 5)
		return -E1000_ERR_PHY;

	phy->min_cable_length = e1000_gg82563_cable_length_table[index];
	phy->max_cable_length = e1000_gg82563_cable_length_table[index + 5];

	phy->cable_length = (phy->min_cable_length + phy->max_cable_length) / 2;

	return 0;
}

static s32 e1000_get_link_up_info_80003es2lan(struct e1000_hw *hw, u16 *speed,
					      u16 *duplex)
{
	s32 ret_val;

	if (hw->phy.media_type == e1000_media_type_copper) {
		ret_val = e1000e_get_speed_and_duplex_copper(hw,
								    speed,
								    duplex);
		hw->phy.ops.cfg_on_link_up(hw);
	} else {
		ret_val = e1000e_get_speed_and_duplex_fiber_serdes(hw,
								  speed,
								  duplex);
	}

	return ret_val;
}

static s32 e1000_reset_hw_80003es2lan(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;

	ret_val = e1000e_disable_pcie_master(hw);
	if (ret_val)
		e_dbg("PCI-E Master disable polling has failed.\n");

	e_dbg("Masking off all interrupts\n");
	ew32(IMC, 0xffffffff);

	ew32(RCTL, 0);
	ew32(TCTL, E1000_TCTL_PSP);
	e1e_flush();

	usleep_range(10000, 20000);

	ctrl = er32(CTRL);

	ret_val = e1000_acquire_phy_80003es2lan(hw);
	e_dbg("Issuing a global reset to MAC\n");
	ew32(CTRL, ctrl | E1000_CTRL_RST);
	e1000_release_phy_80003es2lan(hw);

	ret_val = e1000e_get_auto_rd_done(hw);
	if (ret_val)
		
		return ret_val;

	
	ew32(IMC, 0xffffffff);
	er32(ICR);

	return e1000_check_alt_mac_addr_generic(hw);
}

static s32 e1000_init_hw_80003es2lan(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 reg_data;
	s32 ret_val;
	u16 kum_reg_data;
	u16 i;

	e1000_initialize_hw_bits_80003es2lan(hw);

	
	ret_val = mac->ops.id_led_init(hw);
	if (ret_val)
		e_dbg("Error initializing identification LED\n");
		

	
	e_dbg("Initializing the IEEE VLAN\n");
	mac->ops.clear_vfta(hw);

	
	e1000e_init_rx_addrs(hw, mac->rar_entry_count);

	
	e_dbg("Zeroing the MTA\n");
	for (i = 0; i < mac->mta_reg_count; i++)
		E1000_WRITE_REG_ARRAY(hw, E1000_MTA, i, 0);

	
	ret_val = mac->ops.setup_link(hw);

	
	e1000_read_kmrn_reg_80003es2lan(hw, E1000_KMRNCTRLSTA_INBAND_PARAM,
					&kum_reg_data);
	kum_reg_data |= E1000_KMRNCTRLSTA_IBIST_DISABLE;
	e1000_write_kmrn_reg_80003es2lan(hw, E1000_KMRNCTRLSTA_INBAND_PARAM,
					 kum_reg_data);

	
	reg_data = er32(TXDCTL(0));
	reg_data = (reg_data & ~E1000_TXDCTL_WTHRESH) |
		   E1000_TXDCTL_FULL_TX_DESC_WB | E1000_TXDCTL_COUNT_DESC;
	ew32(TXDCTL(0), reg_data);

	
	reg_data = er32(TXDCTL(1));
	reg_data = (reg_data & ~E1000_TXDCTL_WTHRESH) |
		   E1000_TXDCTL_FULL_TX_DESC_WB | E1000_TXDCTL_COUNT_DESC;
	ew32(TXDCTL(1), reg_data);

	
	reg_data = er32(TCTL);
	reg_data |= E1000_TCTL_RTLC;
	ew32(TCTL, reg_data);

	
	reg_data = er32(TCTL_EXT);
	reg_data &= ~E1000_TCTL_EXT_GCEX_MASK;
	reg_data |= DEFAULT_TCTL_EXT_GCEX_80003ES2LAN;
	ew32(TCTL_EXT, reg_data);

	
	reg_data = er32(TIPG);
	reg_data &= ~E1000_TIPG_IPGT_MASK;
	reg_data |= DEFAULT_TIPG_IPGT_1000_80003ES2LAN;
	ew32(TIPG, reg_data);

	reg_data = E1000_READ_REG_ARRAY(hw, E1000_FFLT, 0x0001);
	reg_data &= ~0x00100000;
	E1000_WRITE_REG_ARRAY(hw, E1000_FFLT, 0x0001, reg_data);

	
	hw->dev_spec.e80003es2lan.mdic_wa_enable = true;

	ret_val = e1000_read_kmrn_reg_80003es2lan(hw,
	                              E1000_KMRNCTRLSTA_OFFSET >>
	                              E1000_KMRNCTRLSTA_OFFSET_SHIFT,
	                              &i);
	if (!ret_val) {
		if ((i & E1000_KMRNCTRLSTA_OPMODE_MASK) ==
		     E1000_KMRNCTRLSTA_OPMODE_INBAND_MDIO)
			hw->dev_spec.e80003es2lan.mdic_wa_enable = false;
	}

	e1000_clear_hw_cntrs_80003es2lan(hw);

	return ret_val;
}

static void e1000_initialize_hw_bits_80003es2lan(struct e1000_hw *hw)
{
	u32 reg;

	
	reg = er32(TXDCTL(0));
	reg |= (1 << 22);
	ew32(TXDCTL(0), reg);

	
	reg = er32(TXDCTL(1));
	reg |= (1 << 22);
	ew32(TXDCTL(1), reg);

	
	reg = er32(TARC(0));
	reg &= ~(0xF << 27); 
	if (hw->phy.media_type != e1000_media_type_copper)
		reg &= ~(1 << 20);
	ew32(TARC(0), reg);

	
	reg = er32(TARC(1));
	if (er32(TCTL) & E1000_TCTL_MULR)
		reg &= ~(1 << 28);
	else
		reg |= (1 << 28);
	ew32(TARC(1), reg);
}

static s32 e1000_copper_link_setup_gg82563_80003es2lan(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u32 ctrl_ext;
	u16 data;

	ret_val = e1e_rphy(hw, GG82563_PHY_MAC_SPEC_CTRL, &data);
	if (ret_val)
		return ret_val;

	data |= GG82563_MSCR_ASSERT_CRS_ON_TX;
	
	data |= GG82563_MSCR_TX_CLK_1000MBPS_25;

	ret_val = e1e_wphy(hw, GG82563_PHY_MAC_SPEC_CTRL, data);
	if (ret_val)
		return ret_val;

	ret_val = e1e_rphy(hw, GG82563_PHY_SPEC_CTRL, &data);
	if (ret_val)
		return ret_val;

	data &= ~GG82563_PSCR_CROSSOVER_MODE_MASK;

	switch (phy->mdix) {
	case 1:
		data |= GG82563_PSCR_CROSSOVER_MODE_MDI;
		break;
	case 2:
		data |= GG82563_PSCR_CROSSOVER_MODE_MDIX;
		break;
	case 0:
	default:
		data |= GG82563_PSCR_CROSSOVER_MODE_AUTO;
		break;
	}

	data &= ~GG82563_PSCR_POLARITY_REVERSAL_DISABLE;
	if (phy->disable_polarity_correction)
		data |= GG82563_PSCR_POLARITY_REVERSAL_DISABLE;

	ret_val = e1e_wphy(hw, GG82563_PHY_SPEC_CTRL, data);
	if (ret_val)
		return ret_val;

	
	ret_val = e1000e_commit_phy(hw);
	if (ret_val) {
		e_dbg("Error Resetting the PHY\n");
		return ret_val;
	}

	
	ret_val = e1000_write_kmrn_reg_80003es2lan(hw,
					E1000_KMRNCTRLSTA_OFFSET_FIFO_CTRL,
					E1000_KMRNCTRLSTA_FIFO_CTRL_RX_BYPASS |
					E1000_KMRNCTRLSTA_FIFO_CTRL_TX_BYPASS);
	if (ret_val)
		return ret_val;

	ret_val = e1000_read_kmrn_reg_80003es2lan(hw,
				       E1000_KMRNCTRLSTA_OFFSET_MAC2PHY_OPMODE,
				       &data);
	if (ret_val)
		return ret_val;
	data |= E1000_KMRNCTRLSTA_OPMODE_E_IDLE;
	ret_val = e1000_write_kmrn_reg_80003es2lan(hw,
					E1000_KMRNCTRLSTA_OFFSET_MAC2PHY_OPMODE,
					data);
	if (ret_val)
		return ret_val;

	ret_val = e1e_rphy(hw, GG82563_PHY_SPEC_CTRL_2, &data);
	if (ret_val)
		return ret_val;

	data &= ~GG82563_PSCR2_REVERSE_AUTO_NEG;
	ret_val = e1e_wphy(hw, GG82563_PHY_SPEC_CTRL_2, data);
	if (ret_val)
		return ret_val;

	ctrl_ext = er32(CTRL_EXT);
	ctrl_ext &= ~(E1000_CTRL_EXT_LINK_MODE_MASK);
	ew32(CTRL_EXT, ctrl_ext);

	ret_val = e1e_rphy(hw, GG82563_PHY_PWR_MGMT_CTRL, &data);
	if (ret_val)
		return ret_val;

	if (!hw->mac.ops.check_mng_mode(hw)) {
		
		data |= GG82563_PMCR_ENABLE_ELECTRICAL_IDLE;
		ret_val = e1e_wphy(hw, GG82563_PHY_PWR_MGMT_CTRL, data);
		if (ret_val)
			return ret_val;

		ret_val = e1e_rphy(hw, GG82563_PHY_KMRN_MODE_CTRL, &data);
		if (ret_val)
			return ret_val;

		data &= ~GG82563_KMCR_PASS_FALSE_CARRIER;
		ret_val = e1e_wphy(hw, GG82563_PHY_KMRN_MODE_CTRL, data);
		if (ret_val)
			return ret_val;
	}

	ret_val = e1e_rphy(hw, GG82563_PHY_INBAND_CTRL, &data);
	if (ret_val)
		return ret_val;

	data |= GG82563_ICR_DIS_PADDING;
	ret_val = e1e_wphy(hw, GG82563_PHY_INBAND_CTRL, data);
	if (ret_val)
		return ret_val;

	return 0;
}

static s32 e1000_setup_copper_link_80003es2lan(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;
	u16 reg_data;

	ctrl = er32(CTRL);
	ctrl |= E1000_CTRL_SLU;
	ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	ew32(CTRL, ctrl);

	ret_val = e1000_write_kmrn_reg_80003es2lan(hw, GG82563_REG(0x34, 4),
	                                           0xFFFF);
	if (ret_val)
		return ret_val;
	ret_val = e1000_read_kmrn_reg_80003es2lan(hw, GG82563_REG(0x34, 9),
	                                          &reg_data);
	if (ret_val)
		return ret_val;
	reg_data |= 0x3F;
	ret_val = e1000_write_kmrn_reg_80003es2lan(hw, GG82563_REG(0x34, 9),
	                                           reg_data);
	if (ret_val)
		return ret_val;
	ret_val = e1000_read_kmrn_reg_80003es2lan(hw,
				      E1000_KMRNCTRLSTA_OFFSET_INB_CTRL,
				      &reg_data);
	if (ret_val)
		return ret_val;
	reg_data |= E1000_KMRNCTRLSTA_INB_CTRL_DIS_PADDING;
	ret_val = e1000_write_kmrn_reg_80003es2lan(hw,
					E1000_KMRNCTRLSTA_OFFSET_INB_CTRL,
					reg_data);
	if (ret_val)
		return ret_val;

	ret_val = e1000_copper_link_setup_gg82563_80003es2lan(hw);
	if (ret_val)
		return ret_val;

	return e1000e_setup_copper_link(hw);
}

static s32 e1000_cfg_on_link_up_80003es2lan(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u16 speed;
	u16 duplex;

	if (hw->phy.media_type == e1000_media_type_copper) {
		ret_val = e1000e_get_speed_and_duplex_copper(hw, &speed,
		                                             &duplex);
		if (ret_val)
			return ret_val;

		if (speed == SPEED_1000)
			ret_val = e1000_cfg_kmrn_1000_80003es2lan(hw);
		else
			ret_val = e1000_cfg_kmrn_10_100_80003es2lan(hw, duplex);
	}

	return ret_val;
}

static s32 e1000_cfg_kmrn_10_100_80003es2lan(struct e1000_hw *hw, u16 duplex)
{
	s32 ret_val;
	u32 tipg;
	u32 i = 0;
	u16 reg_data, reg_data2;

	reg_data = E1000_KMRNCTRLSTA_HD_CTRL_10_100_DEFAULT;
	ret_val = e1000_write_kmrn_reg_80003es2lan(hw,
	                               E1000_KMRNCTRLSTA_OFFSET_HD_CTRL,
	                               reg_data);
	if (ret_val)
		return ret_val;

	
	tipg = er32(TIPG);
	tipg &= ~E1000_TIPG_IPGT_MASK;
	tipg |= DEFAULT_TIPG_IPGT_10_100_80003ES2LAN;
	ew32(TIPG, tipg);

	do {
		ret_val = e1e_rphy(hw, GG82563_PHY_KMRN_MODE_CTRL, &reg_data);
		if (ret_val)
			return ret_val;

		ret_val = e1e_rphy(hw, GG82563_PHY_KMRN_MODE_CTRL, &reg_data2);
		if (ret_val)
			return ret_val;
		i++;
	} while ((reg_data != reg_data2) && (i < GG82563_MAX_KMRN_RETRY));

	if (duplex == HALF_DUPLEX)
		reg_data |= GG82563_KMCR_PASS_FALSE_CARRIER;
	else
		reg_data &= ~GG82563_KMCR_PASS_FALSE_CARRIER;

	return e1e_wphy(hw, GG82563_PHY_KMRN_MODE_CTRL, reg_data);
}

static s32 e1000_cfg_kmrn_1000_80003es2lan(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 reg_data, reg_data2;
	u32 tipg;
	u32 i = 0;

	reg_data = E1000_KMRNCTRLSTA_HD_CTRL_1000_DEFAULT;
	ret_val = e1000_write_kmrn_reg_80003es2lan(hw,
	                               E1000_KMRNCTRLSTA_OFFSET_HD_CTRL,
	                               reg_data);
	if (ret_val)
		return ret_val;

	
	tipg = er32(TIPG);
	tipg &= ~E1000_TIPG_IPGT_MASK;
	tipg |= DEFAULT_TIPG_IPGT_1000_80003ES2LAN;
	ew32(TIPG, tipg);

	do {
		ret_val = e1e_rphy(hw, GG82563_PHY_KMRN_MODE_CTRL, &reg_data);
		if (ret_val)
			return ret_val;

		ret_val = e1e_rphy(hw, GG82563_PHY_KMRN_MODE_CTRL, &reg_data2);
		if (ret_val)
			return ret_val;
		i++;
	} while ((reg_data != reg_data2) && (i < GG82563_MAX_KMRN_RETRY));

	reg_data &= ~GG82563_KMCR_PASS_FALSE_CARRIER;

	return e1e_wphy(hw, GG82563_PHY_KMRN_MODE_CTRL, reg_data);
}

static s32 e1000_read_kmrn_reg_80003es2lan(struct e1000_hw *hw, u32 offset,
					   u16 *data)
{
	u32 kmrnctrlsta;
	s32 ret_val = 0;

	ret_val = e1000_acquire_mac_csr_80003es2lan(hw);
	if (ret_val)
		return ret_val;

	kmrnctrlsta = ((offset << E1000_KMRNCTRLSTA_OFFSET_SHIFT) &
	               E1000_KMRNCTRLSTA_OFFSET) | E1000_KMRNCTRLSTA_REN;
	ew32(KMRNCTRLSTA, kmrnctrlsta);
	e1e_flush();

	udelay(2);

	kmrnctrlsta = er32(KMRNCTRLSTA);
	*data = (u16)kmrnctrlsta;

	e1000_release_mac_csr_80003es2lan(hw);

	return ret_val;
}

static s32 e1000_write_kmrn_reg_80003es2lan(struct e1000_hw *hw, u32 offset,
					    u16 data)
{
	u32 kmrnctrlsta;
	s32 ret_val = 0;

	ret_val = e1000_acquire_mac_csr_80003es2lan(hw);
	if (ret_val)
		return ret_val;

	kmrnctrlsta = ((offset << E1000_KMRNCTRLSTA_OFFSET_SHIFT) &
	               E1000_KMRNCTRLSTA_OFFSET) | data;
	ew32(KMRNCTRLSTA, kmrnctrlsta);
	e1e_flush();

	udelay(2);

	e1000_release_mac_csr_80003es2lan(hw);

	return ret_val;
}

static s32 e1000_read_mac_addr_80003es2lan(struct e1000_hw *hw)
{
	s32 ret_val = 0;

	ret_val = e1000_check_alt_mac_addr_generic(hw);
	if (ret_val)
		return ret_val;

	return e1000_read_mac_addr_generic(hw);
}

static void e1000_power_down_phy_copper_80003es2lan(struct e1000_hw *hw)
{
	
	if (!(hw->mac.ops.check_mng_mode(hw) ||
	      hw->phy.ops.check_reset_block(hw)))
		e1000_power_down_phy_copper(hw);
}

static void e1000_clear_hw_cntrs_80003es2lan(struct e1000_hw *hw)
{
	e1000e_clear_hw_cntrs_base(hw);

	er32(PRC64);
	er32(PRC127);
	er32(PRC255);
	er32(PRC511);
	er32(PRC1023);
	er32(PRC1522);
	er32(PTC64);
	er32(PTC127);
	er32(PTC255);
	er32(PTC511);
	er32(PTC1023);
	er32(PTC1522);

	er32(ALGNERRC);
	er32(RXERRC);
	er32(TNCRS);
	er32(CEXTERR);
	er32(TSCTC);
	er32(TSCTFC);

	er32(MGTPRC);
	er32(MGTPDC);
	er32(MGTPTC);

	er32(IAC);
	er32(ICRXOC);

	er32(ICRXPTC);
	er32(ICRXATC);
	er32(ICTXPTC);
	er32(ICTXATC);
	er32(ICTXQEC);
	er32(ICTXQMTC);
	er32(ICRXDMTC);
}

static const struct e1000_mac_operations es2_mac_ops = {
	.read_mac_addr		= e1000_read_mac_addr_80003es2lan,
	.id_led_init		= e1000e_id_led_init_generic,
	.blink_led		= e1000e_blink_led_generic,
	.check_mng_mode		= e1000e_check_mng_mode_generic,
	
	.cleanup_led		= e1000e_cleanup_led_generic,
	.clear_hw_cntrs		= e1000_clear_hw_cntrs_80003es2lan,
	.get_bus_info		= e1000e_get_bus_info_pcie,
	.set_lan_id		= e1000_set_lan_id_multi_port_pcie,
	.get_link_up_info	= e1000_get_link_up_info_80003es2lan,
	.led_on			= e1000e_led_on_generic,
	.led_off		= e1000e_led_off_generic,
	.update_mc_addr_list	= e1000e_update_mc_addr_list_generic,
	.write_vfta		= e1000_write_vfta_generic,
	.clear_vfta		= e1000_clear_vfta_generic,
	.reset_hw		= e1000_reset_hw_80003es2lan,
	.init_hw		= e1000_init_hw_80003es2lan,
	.setup_link		= e1000e_setup_link_generic,
	
	.setup_led		= e1000e_setup_led_generic,
	.config_collision_dist	= e1000e_config_collision_dist_generic,
};

static const struct e1000_phy_operations es2_phy_ops = {
	.acquire		= e1000_acquire_phy_80003es2lan,
	.check_polarity		= e1000_check_polarity_m88,
	.check_reset_block	= e1000e_check_reset_block_generic,
	.commit		 	= e1000e_phy_sw_reset,
	.force_speed_duplex 	= e1000_phy_force_speed_duplex_80003es2lan,
	.get_cfg_done       	= e1000_get_cfg_done_80003es2lan,
	.get_cable_length   	= e1000_get_cable_length_80003es2lan,
	.get_info       	= e1000e_get_phy_info_m88,
	.read_reg       	= e1000_read_phy_reg_gg82563_80003es2lan,
	.release		= e1000_release_phy_80003es2lan,
	.reset		  	= e1000e_phy_hw_reset_generic,
	.set_d0_lplu_state  	= NULL,
	.set_d3_lplu_state  	= e1000e_set_d3_lplu_state,
	.write_reg      	= e1000_write_phy_reg_gg82563_80003es2lan,
	.cfg_on_link_up      	= e1000_cfg_on_link_up_80003es2lan,
};

static const struct e1000_nvm_operations es2_nvm_ops = {
	.acquire		= e1000_acquire_nvm_80003es2lan,
	.read			= e1000e_read_nvm_eerd,
	.release		= e1000_release_nvm_80003es2lan,
	.reload			= e1000e_reload_nvm_generic,
	.update			= e1000e_update_nvm_checksum_generic,
	.valid_led_default	= e1000e_valid_led_default,
	.validate		= e1000e_validate_nvm_checksum_generic,
	.write			= e1000_write_nvm_80003es2lan,
};

const struct e1000_info e1000_es2_info = {
	.mac			= e1000_80003es2lan,
	.flags			= FLAG_HAS_HW_VLAN_FILTER
				  | FLAG_HAS_JUMBO_FRAMES
				  | FLAG_HAS_WOL
				  | FLAG_APME_IN_CTRL3
				  | FLAG_HAS_CTRLEXT_ON_LOAD
				  | FLAG_RX_NEEDS_RESTART 
				  | FLAG_TARC_SET_BIT_ZERO 
				  | FLAG_APME_CHECK_PORT_B
				  | FLAG_DISABLE_FC_PAUSE_TIME, 
	.flags2			= FLAG2_DMA_BURST,
	.pba			= 38,
	.max_hw_frame_size	= DEFAULT_JUMBO,
	.get_variants		= e1000_get_variants_80003es2lan,
	.mac_ops		= &es2_mac_ops,
	.phy_ops		= &es2_phy_ops,
	.nvm_ops		= &es2_nvm_ops,
};

