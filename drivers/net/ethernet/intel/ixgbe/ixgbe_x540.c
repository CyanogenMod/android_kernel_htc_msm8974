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

#define IXGBE_X540_MAX_TX_QUEUES 128
#define IXGBE_X540_MAX_RX_QUEUES 128
#define IXGBE_X540_RAR_ENTRIES   128
#define IXGBE_X540_MC_TBL_SIZE   128
#define IXGBE_X540_VFT_TBL_SIZE  128
#define IXGBE_X540_RX_PB_SIZE	 384

static s32 ixgbe_update_flash_X540(struct ixgbe_hw *hw);
static s32 ixgbe_poll_flash_update_done_X540(struct ixgbe_hw *hw);
static s32 ixgbe_acquire_swfw_sync_X540(struct ixgbe_hw *hw, u16 mask);
static void ixgbe_release_swfw_sync_X540(struct ixgbe_hw *hw, u16 mask);
static s32 ixgbe_get_swfw_sync_semaphore(struct ixgbe_hw *hw);
static void ixgbe_release_swfw_sync_semaphore(struct ixgbe_hw *hw);

static enum ixgbe_media_type ixgbe_get_media_type_X540(struct ixgbe_hw *hw)
{
	return ixgbe_media_type_copper;
}

static s32 ixgbe_get_invariants_X540(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;

	
	ixgbe_identify_phy_generic(hw);

	mac->mcft_size = IXGBE_X540_MC_TBL_SIZE;
	mac->vft_size = IXGBE_X540_VFT_TBL_SIZE;
	mac->num_rar_entries = IXGBE_X540_RAR_ENTRIES;
	mac->max_rx_queues = IXGBE_X540_MAX_RX_QUEUES;
	mac->max_tx_queues = IXGBE_X540_MAX_TX_QUEUES;
	mac->max_msix_vectors = ixgbe_get_pcie_msix_count_generic(hw);

	return 0;
}

static s32 ixgbe_setup_mac_link_X540(struct ixgbe_hw *hw,
                                     ixgbe_link_speed speed, bool autoneg,
                                     bool autoneg_wait_to_complete)
{
	return hw->phy.ops.setup_link_speed(hw, speed, autoneg,
	                                    autoneg_wait_to_complete);
}

static s32 ixgbe_reset_hw_X540(struct ixgbe_hw *hw)
{
	s32 status;
	u32 ctrl, i;

	
	status = hw->mac.ops.stop_adapter(hw);
	if (status != 0)
		goto reset_hw_out;

	
	ixgbe_clear_tx_pending(hw);

mac_reset_top:
	ctrl = IXGBE_CTRL_RST;
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
	msleep(100);

	if (hw->mac.flags & IXGBE_FLAGS_DOUBLE_RESET_REQUIRED) {
		hw->mac.flags &= ~IXGBE_FLAGS_DOUBLE_RESET_REQUIRED;
		goto mac_reset_top;
	}

	
	IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(0), 384 << IXGBE_RXPBSIZE_SHIFT);

	
	hw->mac.ops.get_mac_addr(hw, hw->mac.perm_addr);

	hw->mac.num_rar_entries = IXGBE_X540_MAX_TX_QUEUES;
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

static s32 ixgbe_start_hw_X540(struct ixgbe_hw *hw)
{
	s32 ret_val = 0;

	ret_val = ixgbe_start_hw_generic(hw);
	if (ret_val != 0)
		goto out;

	ret_val = ixgbe_start_hw_gen2(hw);
	hw->mac.rx_pb_size = IXGBE_X540_RX_PB_SIZE;
out:
	return ret_val;
}

static u32 ixgbe_get_supported_physical_layer_X540(struct ixgbe_hw *hw)
{
	u32 physical_layer = IXGBE_PHYSICAL_LAYER_UNKNOWN;
	u16 ext_ability = 0;

	hw->phy.ops.identify(hw);

	hw->phy.ops.read_reg(hw, MDIO_PMA_EXTABLE, MDIO_MMD_PMAPMD,
			     &ext_ability);
	if (ext_ability & MDIO_PMA_EXTABLE_10GBT)
		physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_T;
	if (ext_ability & MDIO_PMA_EXTABLE_1000BT)
		physical_layer |= IXGBE_PHYSICAL_LAYER_1000BASE_T;
	if (ext_ability & MDIO_PMA_EXTABLE_100BTX)
		physical_layer |= IXGBE_PHYSICAL_LAYER_100BASE_TX;

	return physical_layer;
}

static s32 ixgbe_init_eeprom_params_X540(struct ixgbe_hw *hw)
{
	struct ixgbe_eeprom_info *eeprom = &hw->eeprom;
	u32 eec;
	u16 eeprom_size;

	if (eeprom->type == ixgbe_eeprom_uninitialized) {
		eeprom->semaphore_delay = 10;
		eeprom->type = ixgbe_flash;

		eec = IXGBE_READ_REG(hw, IXGBE_EEC);
		eeprom_size = (u16)((eec & IXGBE_EEC_SIZE) >>
		                    IXGBE_EEC_SIZE_SHIFT);
		eeprom->word_size = 1 << (eeprom_size +
		                          IXGBE_EEPROM_WORD_SIZE_SHIFT);

		hw_dbg(hw, "Eeprom params: type = %d, size = %d\n",
		       eeprom->type, eeprom->word_size);
	}

	return 0;
}

static s32 ixgbe_read_eerd_X540(struct ixgbe_hw *hw, u16 offset, u16 *data)
{
	s32 status = 0;

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_EEP_SM) ==
	    0)
		status = ixgbe_read_eerd_generic(hw, offset, data);
	else
		status = IXGBE_ERR_SWFW_SYNC;

	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);
	return status;
}

static s32 ixgbe_read_eerd_buffer_X540(struct ixgbe_hw *hw,
				       u16 offset, u16 words, u16 *data)
{
	s32 status = 0;

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_EEP_SM) ==
	    0)
		status = ixgbe_read_eerd_buffer_generic(hw, offset,
							words, data);
	else
		status = IXGBE_ERR_SWFW_SYNC;

	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);
	return status;
}

static s32 ixgbe_write_eewr_X540(struct ixgbe_hw *hw, u16 offset, u16 data)
{
	s32 status = 0;

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_EEP_SM) == 0)
		status = ixgbe_write_eewr_generic(hw, offset, data);
	else
		status = IXGBE_ERR_SWFW_SYNC;

	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);
	return status;
}

static s32 ixgbe_write_eewr_buffer_X540(struct ixgbe_hw *hw,
					u16 offset, u16 words, u16 *data)
{
	s32 status = 0;

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_EEP_SM) ==
	    0)
		status = ixgbe_write_eewr_buffer_generic(hw, offset,
							 words, data);
	else
		status = IXGBE_ERR_SWFW_SYNC;

	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);
	return status;
}

static u16 ixgbe_calc_eeprom_checksum_X540(struct ixgbe_hw *hw)
{
	u16 i;
	u16 j;
	u16 checksum = 0;
	u16 length = 0;
	u16 pointer = 0;
	u16 word = 0;


	
	for (i = 0; i < IXGBE_EEPROM_CHECKSUM; i++) {
		if (ixgbe_read_eerd_generic(hw, i, &word) != 0) {
			hw_dbg(hw, "EEPROM read failed\n");
			break;
		}
		checksum += word;
	}

	for (i = IXGBE_PCIE_ANALOG_PTR; i < IXGBE_FW_PTR; i++) {
		if (i == IXGBE_PHY_PTR || i == IXGBE_OPTION_ROM_PTR)
			continue;

		if (ixgbe_read_eerd_generic(hw, i, &pointer) != 0) {
			hw_dbg(hw, "EEPROM read failed\n");
			break;
		}

		
		if (pointer == 0xFFFF || pointer == 0 ||
		    pointer >= hw->eeprom.word_size)
			continue;

		if (ixgbe_read_eerd_generic(hw, pointer, &length) != 0) {
			hw_dbg(hw, "EEPROM read failed\n");
			break;
		}

		
		if (length == 0xFFFF || length == 0 ||
		    (pointer + length) >= hw->eeprom.word_size)
			continue;

		for (j = pointer+1; j <= pointer+length; j++) {
			if (ixgbe_read_eerd_generic(hw, j, &word) != 0) {
				hw_dbg(hw, "EEPROM read failed\n");
				break;
			}
			checksum += word;
		}
	}

	checksum = (u16)IXGBE_EEPROM_SUM - checksum;

	return checksum;
}

static s32 ixgbe_validate_eeprom_checksum_X540(struct ixgbe_hw *hw,
					       u16 *checksum_val)
{
	s32 status;
	u16 checksum;
	u16 read_checksum = 0;

	status = hw->eeprom.ops.read(hw, 0, &checksum);

	if (status != 0) {
		hw_dbg(hw, "EEPROM read failed\n");
		goto out;
	}

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_EEP_SM) == 0) {
		checksum = hw->eeprom.ops.calc_checksum(hw);

		ixgbe_read_eerd_generic(hw, IXGBE_EEPROM_CHECKSUM,
					&read_checksum);

		if (read_checksum != checksum)
			status = IXGBE_ERR_EEPROM_CHECKSUM;

		
		if (checksum_val)
			*checksum_val = checksum;
	} else {
		status = IXGBE_ERR_SWFW_SYNC;
	}

	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);
out:
	return status;
}

static s32 ixgbe_update_eeprom_checksum_X540(struct ixgbe_hw *hw)
{
	s32 status;
	u16 checksum;

	status = hw->eeprom.ops.read(hw, 0, &checksum);

	if (status != 0)
		hw_dbg(hw, "EEPROM read failed\n");

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_EEP_SM) == 0) {
		checksum = hw->eeprom.ops.calc_checksum(hw);

		status = ixgbe_write_eewr_generic(hw, IXGBE_EEPROM_CHECKSUM,
						  checksum);

	if (status == 0)
		status = ixgbe_update_flash_X540(hw);
	else
		status = IXGBE_ERR_SWFW_SYNC;
	}

	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);

	return status;
}

static s32 ixgbe_update_flash_X540(struct ixgbe_hw *hw)
{
	u32 flup;
	s32 status = IXGBE_ERR_EEPROM;

	status = ixgbe_poll_flash_update_done_X540(hw);
	if (status == IXGBE_ERR_EEPROM) {
		hw_dbg(hw, "Flash update time out\n");
		goto out;
	}

	flup = IXGBE_READ_REG(hw, IXGBE_EEC) | IXGBE_EEC_FLUP;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, flup);

	status = ixgbe_poll_flash_update_done_X540(hw);
	if (status == 0)
		hw_dbg(hw, "Flash update complete\n");
	else
		hw_dbg(hw, "Flash update time out\n");

	if (hw->revision_id == 0) {
		flup = IXGBE_READ_REG(hw, IXGBE_EEC);

		if (flup & IXGBE_EEC_SEC1VAL) {
			flup |= IXGBE_EEC_FLUP;
			IXGBE_WRITE_REG(hw, IXGBE_EEC, flup);
		}

		status = ixgbe_poll_flash_update_done_X540(hw);
		if (status == 0)
			hw_dbg(hw, "Flash update complete\n");
		else
			hw_dbg(hw, "Flash update time out\n");
	}
out:
	return status;
}

static s32 ixgbe_poll_flash_update_done_X540(struct ixgbe_hw *hw)
{
	u32 i;
	u32 reg;
	s32 status = IXGBE_ERR_EEPROM;

	for (i = 0; i < IXGBE_FLUDONE_ATTEMPTS; i++) {
		reg = IXGBE_READ_REG(hw, IXGBE_EEC);
		if (reg & IXGBE_EEC_FLUDONE) {
			status = 0;
			break;
		}
		udelay(5);
	}
	return status;
}

static s32 ixgbe_acquire_swfw_sync_X540(struct ixgbe_hw *hw, u16 mask)
{
	u32 swfw_sync;
	u32 swmask = mask;
	u32 fwmask = mask << 5;
	u32 hwmask = 0;
	u32 timeout = 200;
	u32 i;

	if (swmask == IXGBE_GSSR_EEP_SM)
		hwmask = IXGBE_GSSR_FLASH_SM;

	for (i = 0; i < timeout; i++) {
		if (ixgbe_get_swfw_sync_semaphore(hw))
			return IXGBE_ERR_SWFW_SYNC;

		swfw_sync = IXGBE_READ_REG(hw, IXGBE_SWFW_SYNC);
		if (!(swfw_sync & (fwmask | swmask | hwmask))) {
			swfw_sync |= swmask;
			IXGBE_WRITE_REG(hw, IXGBE_SWFW_SYNC, swfw_sync);
			ixgbe_release_swfw_sync_semaphore(hw);
			break;
		} else {
			ixgbe_release_swfw_sync_semaphore(hw);
			usleep_range(5000, 10000);
		}
	}

	if (i >= timeout) {
		swfw_sync = IXGBE_READ_REG(hw, IXGBE_SWFW_SYNC);
		if (swfw_sync & (fwmask | hwmask)) {
			if (ixgbe_get_swfw_sync_semaphore(hw))
				return IXGBE_ERR_SWFW_SYNC;

			swfw_sync |= swmask;
			IXGBE_WRITE_REG(hw, IXGBE_SWFW_SYNC, swfw_sync);
			ixgbe_release_swfw_sync_semaphore(hw);
		}
	}

	usleep_range(5000, 10000);
	return 0;
}

static void ixgbe_release_swfw_sync_X540(struct ixgbe_hw *hw, u16 mask)
{
	u32 swfw_sync;
	u32 swmask = mask;

	ixgbe_get_swfw_sync_semaphore(hw);

	swfw_sync = IXGBE_READ_REG(hw, IXGBE_SWFW_SYNC);
	swfw_sync &= ~swmask;
	IXGBE_WRITE_REG(hw, IXGBE_SWFW_SYNC, swfw_sync);

	ixgbe_release_swfw_sync_semaphore(hw);
	usleep_range(5000, 10000);
}

static s32 ixgbe_get_swfw_sync_semaphore(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_ERR_EEPROM;
	u32 timeout = 2000;
	u32 i;
	u32 swsm;

	
	for (i = 0; i < timeout; i++) {
		swsm = IXGBE_READ_REG(hw, IXGBE_SWSM);
		if (!(swsm & IXGBE_SWSM_SMBI)) {
			status = 0;
			break;
		}
		udelay(50);
	}

	
	if (status) {
		for (i = 0; i < timeout; i++) {
			swsm = IXGBE_READ_REG(hw, IXGBE_SWFW_SYNC);
			if (!(swsm & IXGBE_SWFW_REGSMP))
				break;

			udelay(50);
		}
	} else {
		hw_dbg(hw, "Software semaphore SMBI between device drivers "
		           "not granted.\n");
	}

	return status;
}

static void ixgbe_release_swfw_sync_semaphore(struct ixgbe_hw *hw)
{
	 u32 swsm;

	

	swsm = IXGBE_READ_REG(hw, IXGBE_SWSM);
	swsm &= ~IXGBE_SWSM_SMBI;
	IXGBE_WRITE_REG(hw, IXGBE_SWSM, swsm);

	swsm = IXGBE_READ_REG(hw, IXGBE_SWFW_SYNC);
	swsm &= ~IXGBE_SWFW_REGSMP;
	IXGBE_WRITE_REG(hw, IXGBE_SWFW_SYNC, swsm);

	IXGBE_WRITE_FLUSH(hw);
}

static s32 ixgbe_blink_led_start_X540(struct ixgbe_hw *hw, u32 index)
{
	u32 macc_reg;
	u32 ledctl_reg;
	ixgbe_link_speed speed;
	bool link_up;

	hw->mac.ops.check_link(hw, &speed, &link_up, false);
	if (!link_up) {
		macc_reg = IXGBE_READ_REG(hw, IXGBE_MACC);
		macc_reg |= IXGBE_MACC_FLU | IXGBE_MACC_FSV_10G | IXGBE_MACC_FS;
		IXGBE_WRITE_REG(hw, IXGBE_MACC, macc_reg);
	}
	
	ledctl_reg = IXGBE_READ_REG(hw, IXGBE_LEDCTL);
	ledctl_reg &= ~IXGBE_LED_MODE_MASK(index);
	ledctl_reg |= IXGBE_LED_BLINK(index);
	IXGBE_WRITE_REG(hw, IXGBE_LEDCTL, ledctl_reg);
	IXGBE_WRITE_FLUSH(hw);

	return 0;
}

static s32 ixgbe_blink_led_stop_X540(struct ixgbe_hw *hw, u32 index)
{
	u32 macc_reg;
	u32 ledctl_reg;

	
	ledctl_reg = IXGBE_READ_REG(hw, IXGBE_LEDCTL);
	ledctl_reg &= ~IXGBE_LED_MODE_MASK(index);
	ledctl_reg |= IXGBE_LED_LINK_ACTIVE << IXGBE_LED_MODE_SHIFT(index);
	ledctl_reg &= ~IXGBE_LED_BLINK(index);
	IXGBE_WRITE_REG(hw, IXGBE_LEDCTL, ledctl_reg);

	
	macc_reg = IXGBE_READ_REG(hw, IXGBE_MACC);
	macc_reg &= ~(IXGBE_MACC_FLU | IXGBE_MACC_FSV_10G | IXGBE_MACC_FS);
	IXGBE_WRITE_REG(hw, IXGBE_MACC, macc_reg);
	IXGBE_WRITE_FLUSH(hw);

	return 0;
}
static struct ixgbe_mac_operations mac_ops_X540 = {
	.init_hw                = &ixgbe_init_hw_generic,
	.reset_hw               = &ixgbe_reset_hw_X540,
	.start_hw               = &ixgbe_start_hw_X540,
	.clear_hw_cntrs         = &ixgbe_clear_hw_cntrs_generic,
	.get_media_type         = &ixgbe_get_media_type_X540,
	.get_supported_physical_layer =
                                  &ixgbe_get_supported_physical_layer_X540,
	.enable_rx_dma          = &ixgbe_enable_rx_dma_generic,
	.get_mac_addr           = &ixgbe_get_mac_addr_generic,
	.get_san_mac_addr       = &ixgbe_get_san_mac_addr_generic,
	.get_device_caps        = &ixgbe_get_device_caps_generic,
	.get_wwn_prefix         = &ixgbe_get_wwn_prefix_generic,
	.stop_adapter           = &ixgbe_stop_adapter_generic,
	.get_bus_info           = &ixgbe_get_bus_info_generic,
	.set_lan_id             = &ixgbe_set_lan_id_multi_port_pcie,
	.read_analog_reg8       = NULL,
	.write_analog_reg8      = NULL,
	.setup_link             = &ixgbe_setup_mac_link_X540,
	.set_rxpba		= &ixgbe_set_rxpba_generic,
	.check_link             = &ixgbe_check_mac_link_generic,
	.get_link_capabilities  = &ixgbe_get_copper_link_capabilities_generic,
	.led_on                 = &ixgbe_led_on_generic,
	.led_off                = &ixgbe_led_off_generic,
	.blink_led_start        = &ixgbe_blink_led_start_X540,
	.blink_led_stop         = &ixgbe_blink_led_stop_X540,
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
	.setup_sfp              = NULL,
	.set_mac_anti_spoofing  = &ixgbe_set_mac_anti_spoofing,
	.set_vlan_anti_spoofing = &ixgbe_set_vlan_anti_spoofing,
	.acquire_swfw_sync      = &ixgbe_acquire_swfw_sync_X540,
	.release_swfw_sync      = &ixgbe_release_swfw_sync_X540,
	.disable_rx_buff	= &ixgbe_disable_rx_buff_generic,
	.enable_rx_buff		= &ixgbe_enable_rx_buff_generic,
};

static struct ixgbe_eeprom_operations eeprom_ops_X540 = {
	.init_params            = &ixgbe_init_eeprom_params_X540,
	.read                   = &ixgbe_read_eerd_X540,
	.read_buffer		= &ixgbe_read_eerd_buffer_X540,
	.write                  = &ixgbe_write_eewr_X540,
	.write_buffer		= &ixgbe_write_eewr_buffer_X540,
	.calc_checksum		= &ixgbe_calc_eeprom_checksum_X540,
	.validate_checksum      = &ixgbe_validate_eeprom_checksum_X540,
	.update_checksum        = &ixgbe_update_eeprom_checksum_X540,
};

static struct ixgbe_phy_operations phy_ops_X540 = {
	.identify               = &ixgbe_identify_phy_generic,
	.identify_sfp           = &ixgbe_identify_sfp_module_generic,
	.init			= NULL,
	.reset                  = NULL,
	.read_reg               = &ixgbe_read_phy_reg_generic,
	.write_reg              = &ixgbe_write_phy_reg_generic,
	.setup_link             = &ixgbe_setup_phy_link_generic,
	.setup_link_speed       = &ixgbe_setup_phy_link_speed_generic,
	.read_i2c_byte          = &ixgbe_read_i2c_byte_generic,
	.write_i2c_byte         = &ixgbe_write_i2c_byte_generic,
	.read_i2c_eeprom        = &ixgbe_read_i2c_eeprom_generic,
	.write_i2c_eeprom       = &ixgbe_write_i2c_eeprom_generic,
	.check_overtemp         = &ixgbe_tn_check_overtemp,
	.get_firmware_version   = &ixgbe_get_phy_firmware_version_generic,
};

struct ixgbe_info ixgbe_X540_info = {
	.mac                    = ixgbe_mac_X540,
	.get_invariants         = &ixgbe_get_invariants_X540,
	.mac_ops                = &mac_ops_X540,
	.eeprom_ops             = &eeprom_ops_X540,
	.phy_ops                = &phy_ops_X540,
	.mbx_ops                = &mbx_ops_generic,
};
