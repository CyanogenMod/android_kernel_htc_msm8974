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
#include <linux/netdevice.h>

#include "ixgbe.h"
#include "ixgbe_common.h"
#include "ixgbe_phy.h"

static s32 ixgbe_acquire_eeprom(struct ixgbe_hw *hw);
static s32 ixgbe_get_eeprom_semaphore(struct ixgbe_hw *hw);
static void ixgbe_release_eeprom_semaphore(struct ixgbe_hw *hw);
static s32 ixgbe_ready_eeprom(struct ixgbe_hw *hw);
static void ixgbe_standby_eeprom(struct ixgbe_hw *hw);
static void ixgbe_shift_out_eeprom_bits(struct ixgbe_hw *hw, u16 data,
                                        u16 count);
static u16 ixgbe_shift_in_eeprom_bits(struct ixgbe_hw *hw, u16 count);
static void ixgbe_raise_eeprom_clk(struct ixgbe_hw *hw, u32 *eec);
static void ixgbe_lower_eeprom_clk(struct ixgbe_hw *hw, u32 *eec);
static void ixgbe_release_eeprom(struct ixgbe_hw *hw);

static s32 ixgbe_mta_vector(struct ixgbe_hw *hw, u8 *mc_addr);
static s32 ixgbe_fc_autoneg_fiber(struct ixgbe_hw *hw);
static s32 ixgbe_fc_autoneg_backplane(struct ixgbe_hw *hw);
static s32 ixgbe_fc_autoneg_copper(struct ixgbe_hw *hw);
static s32 ixgbe_device_supports_autoneg_fc(struct ixgbe_hw *hw);
static s32 ixgbe_negotiate_fc(struct ixgbe_hw *hw, u32 adv_reg, u32 lp_reg,
			      u32 adv_sym, u32 adv_asm, u32 lp_sym, u32 lp_asm);
static s32 ixgbe_setup_fc(struct ixgbe_hw *hw, s32 packetbuf_num);
static s32 ixgbe_poll_eerd_eewr_done(struct ixgbe_hw *hw, u32 ee_reg);
static s32 ixgbe_read_eeprom_buffer_bit_bang(struct ixgbe_hw *hw, u16 offset,
					     u16 words, u16 *data);
static s32 ixgbe_write_eeprom_buffer_bit_bang(struct ixgbe_hw *hw, u16 offset,
					     u16 words, u16 *data);
static s32 ixgbe_detect_eeprom_page_size_generic(struct ixgbe_hw *hw,
						 u16 offset);
static s32 ixgbe_disable_pcie_master(struct ixgbe_hw *hw);

s32 ixgbe_start_hw_generic(struct ixgbe_hw *hw)
{
	u32 ctrl_ext;

	
	hw->phy.media_type = hw->mac.ops.get_media_type(hw);

	
	hw->phy.ops.identify(hw);

	
	hw->mac.ops.clear_vfta(hw);

	
	hw->mac.ops.clear_hw_cntrs(hw);

	
	ctrl_ext = IXGBE_READ_REG(hw, IXGBE_CTRL_EXT);
	ctrl_ext |= IXGBE_CTRL_EXT_NS_DIS;
	IXGBE_WRITE_REG(hw, IXGBE_CTRL_EXT, ctrl_ext);
	IXGBE_WRITE_FLUSH(hw);

	
	ixgbe_setup_fc(hw, 0);

	
	hw->adapter_stopped = false;

	return 0;
}

s32 ixgbe_start_hw_gen2(struct ixgbe_hw *hw)
{
	u32 i;
	u32 regval;

	
	for (i = 0; i < hw->mac.max_tx_queues; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_RTTDQSEL, i);
		IXGBE_WRITE_REG(hw, IXGBE_RTTBCNRC, 0);
	}
	IXGBE_WRITE_FLUSH(hw);

	
	for (i = 0; i < hw->mac.max_tx_queues; i++) {
		regval = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL_82599(i));
		regval &= ~IXGBE_DCA_TXCTRL_DESC_WRO_EN;
		IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL_82599(i), regval);
	}

	for (i = 0; i < hw->mac.max_rx_queues; i++) {
		regval = IXGBE_READ_REG(hw, IXGBE_DCA_RXCTRL(i));
		regval &= ~(IXGBE_DCA_RXCTRL_DATA_WRO_EN |
			    IXGBE_DCA_RXCTRL_HEAD_WRO_EN);
		IXGBE_WRITE_REG(hw, IXGBE_DCA_RXCTRL(i), regval);
	}

	return 0;
}

s32 ixgbe_init_hw_generic(struct ixgbe_hw *hw)
{
	s32 status;

	
	status = hw->mac.ops.reset_hw(hw);

	if (status == 0) {
		
		status = hw->mac.ops.start_hw(hw);
	}

	return status;
}

s32 ixgbe_clear_hw_cntrs_generic(struct ixgbe_hw *hw)
{
	u16 i = 0;

	IXGBE_READ_REG(hw, IXGBE_CRCERRS);
	IXGBE_READ_REG(hw, IXGBE_ILLERRC);
	IXGBE_READ_REG(hw, IXGBE_ERRBC);
	IXGBE_READ_REG(hw, IXGBE_MSPDC);
	for (i = 0; i < 8; i++)
		IXGBE_READ_REG(hw, IXGBE_MPC(i));

	IXGBE_READ_REG(hw, IXGBE_MLFC);
	IXGBE_READ_REG(hw, IXGBE_MRFC);
	IXGBE_READ_REG(hw, IXGBE_RLEC);
	IXGBE_READ_REG(hw, IXGBE_LXONTXC);
	IXGBE_READ_REG(hw, IXGBE_LXOFFTXC);
	if (hw->mac.type >= ixgbe_mac_82599EB) {
		IXGBE_READ_REG(hw, IXGBE_LXONRXCNT);
		IXGBE_READ_REG(hw, IXGBE_LXOFFRXCNT);
	} else {
		IXGBE_READ_REG(hw, IXGBE_LXONRXC);
		IXGBE_READ_REG(hw, IXGBE_LXOFFRXC);
	}

	for (i = 0; i < 8; i++) {
		IXGBE_READ_REG(hw, IXGBE_PXONTXC(i));
		IXGBE_READ_REG(hw, IXGBE_PXOFFTXC(i));
		if (hw->mac.type >= ixgbe_mac_82599EB) {
			IXGBE_READ_REG(hw, IXGBE_PXONRXCNT(i));
			IXGBE_READ_REG(hw, IXGBE_PXOFFRXCNT(i));
		} else {
			IXGBE_READ_REG(hw, IXGBE_PXONRXC(i));
			IXGBE_READ_REG(hw, IXGBE_PXOFFRXC(i));
		}
	}
	if (hw->mac.type >= ixgbe_mac_82599EB)
		for (i = 0; i < 8; i++)
			IXGBE_READ_REG(hw, IXGBE_PXON2OFFCNT(i));
	IXGBE_READ_REG(hw, IXGBE_PRC64);
	IXGBE_READ_REG(hw, IXGBE_PRC127);
	IXGBE_READ_REG(hw, IXGBE_PRC255);
	IXGBE_READ_REG(hw, IXGBE_PRC511);
	IXGBE_READ_REG(hw, IXGBE_PRC1023);
	IXGBE_READ_REG(hw, IXGBE_PRC1522);
	IXGBE_READ_REG(hw, IXGBE_GPRC);
	IXGBE_READ_REG(hw, IXGBE_BPRC);
	IXGBE_READ_REG(hw, IXGBE_MPRC);
	IXGBE_READ_REG(hw, IXGBE_GPTC);
	IXGBE_READ_REG(hw, IXGBE_GORCL);
	IXGBE_READ_REG(hw, IXGBE_GORCH);
	IXGBE_READ_REG(hw, IXGBE_GOTCL);
	IXGBE_READ_REG(hw, IXGBE_GOTCH);
	if (hw->mac.type == ixgbe_mac_82598EB)
		for (i = 0; i < 8; i++)
			IXGBE_READ_REG(hw, IXGBE_RNBC(i));
	IXGBE_READ_REG(hw, IXGBE_RUC);
	IXGBE_READ_REG(hw, IXGBE_RFC);
	IXGBE_READ_REG(hw, IXGBE_ROC);
	IXGBE_READ_REG(hw, IXGBE_RJC);
	IXGBE_READ_REG(hw, IXGBE_MNGPRC);
	IXGBE_READ_REG(hw, IXGBE_MNGPDC);
	IXGBE_READ_REG(hw, IXGBE_MNGPTC);
	IXGBE_READ_REG(hw, IXGBE_TORL);
	IXGBE_READ_REG(hw, IXGBE_TORH);
	IXGBE_READ_REG(hw, IXGBE_TPR);
	IXGBE_READ_REG(hw, IXGBE_TPT);
	IXGBE_READ_REG(hw, IXGBE_PTC64);
	IXGBE_READ_REG(hw, IXGBE_PTC127);
	IXGBE_READ_REG(hw, IXGBE_PTC255);
	IXGBE_READ_REG(hw, IXGBE_PTC511);
	IXGBE_READ_REG(hw, IXGBE_PTC1023);
	IXGBE_READ_REG(hw, IXGBE_PTC1522);
	IXGBE_READ_REG(hw, IXGBE_MPTC);
	IXGBE_READ_REG(hw, IXGBE_BPTC);
	for (i = 0; i < 16; i++) {
		IXGBE_READ_REG(hw, IXGBE_QPRC(i));
		IXGBE_READ_REG(hw, IXGBE_QPTC(i));
		if (hw->mac.type >= ixgbe_mac_82599EB) {
			IXGBE_READ_REG(hw, IXGBE_QBRC_L(i));
			IXGBE_READ_REG(hw, IXGBE_QBRC_H(i));
			IXGBE_READ_REG(hw, IXGBE_QBTC_L(i));
			IXGBE_READ_REG(hw, IXGBE_QBTC_H(i));
			IXGBE_READ_REG(hw, IXGBE_QPRDC(i));
		} else {
			IXGBE_READ_REG(hw, IXGBE_QBRC(i));
			IXGBE_READ_REG(hw, IXGBE_QBTC(i));
		}
	}

	if (hw->mac.type == ixgbe_mac_X540) {
		if (hw->phy.id == 0)
			hw->phy.ops.identify(hw);
		hw->phy.ops.read_reg(hw, IXGBE_PCRC8ECL, MDIO_MMD_PCS, &i);
		hw->phy.ops.read_reg(hw, IXGBE_PCRC8ECH, MDIO_MMD_PCS, &i);
		hw->phy.ops.read_reg(hw, IXGBE_LDPCECL, MDIO_MMD_PCS, &i);
		hw->phy.ops.read_reg(hw, IXGBE_LDPCECH, MDIO_MMD_PCS, &i);
	}

	return 0;
}

s32 ixgbe_read_pba_string_generic(struct ixgbe_hw *hw, u8 *pba_num,
                                  u32 pba_num_size)
{
	s32 ret_val;
	u16 data;
	u16 pba_ptr;
	u16 offset;
	u16 length;

	if (pba_num == NULL) {
		hw_dbg(hw, "PBA string buffer was null\n");
		return IXGBE_ERR_INVALID_ARGUMENT;
	}

	ret_val = hw->eeprom.ops.read(hw, IXGBE_PBANUM0_PTR, &data);
	if (ret_val) {
		hw_dbg(hw, "NVM Read Error\n");
		return ret_val;
	}

	ret_val = hw->eeprom.ops.read(hw, IXGBE_PBANUM1_PTR, &pba_ptr);
	if (ret_val) {
		hw_dbg(hw, "NVM Read Error\n");
		return ret_val;
	}

	if (data != IXGBE_PBANUM_PTR_GUARD) {
		hw_dbg(hw, "NVM PBA number is not stored as string\n");

		
		if (pba_num_size < 11) {
			hw_dbg(hw, "PBA string buffer too small\n");
			return IXGBE_ERR_NO_SPACE;
		}

		
		pba_num[0] = (data >> 12) & 0xF;
		pba_num[1] = (data >> 8) & 0xF;
		pba_num[2] = (data >> 4) & 0xF;
		pba_num[3] = data & 0xF;
		pba_num[4] = (pba_ptr >> 12) & 0xF;
		pba_num[5] = (pba_ptr >> 8) & 0xF;
		pba_num[6] = '-';
		pba_num[7] = 0;
		pba_num[8] = (pba_ptr >> 4) & 0xF;
		pba_num[9] = pba_ptr & 0xF;

		
		pba_num[10] = '\0';

		
		for (offset = 0; offset < 10; offset++) {
			if (pba_num[offset] < 0xA)
				pba_num[offset] += '0';
			else if (pba_num[offset] < 0x10)
				pba_num[offset] += 'A' - 0xA;
		}

		return 0;
	}

	ret_val = hw->eeprom.ops.read(hw, pba_ptr, &length);
	if (ret_val) {
		hw_dbg(hw, "NVM Read Error\n");
		return ret_val;
	}

	if (length == 0xFFFF || length == 0) {
		hw_dbg(hw, "NVM PBA number section invalid length\n");
		return IXGBE_ERR_PBA_SECTION;
	}

	
	if (pba_num_size  < (((u32)length * 2) - 1)) {
		hw_dbg(hw, "PBA string buffer too small\n");
		return IXGBE_ERR_NO_SPACE;
	}

	
	pba_ptr++;
	length--;

	for (offset = 0; offset < length; offset++) {
		ret_val = hw->eeprom.ops.read(hw, pba_ptr + offset, &data);
		if (ret_val) {
			hw_dbg(hw, "NVM Read Error\n");
			return ret_val;
		}
		pba_num[offset * 2] = (u8)(data >> 8);
		pba_num[(offset * 2) + 1] = (u8)(data & 0xFF);
	}
	pba_num[offset * 2] = '\0';

	return 0;
}

s32 ixgbe_get_mac_addr_generic(struct ixgbe_hw *hw, u8 *mac_addr)
{
	u32 rar_high;
	u32 rar_low;
	u16 i;

	rar_high = IXGBE_READ_REG(hw, IXGBE_RAH(0));
	rar_low = IXGBE_READ_REG(hw, IXGBE_RAL(0));

	for (i = 0; i < 4; i++)
		mac_addr[i] = (u8)(rar_low >> (i*8));

	for (i = 0; i < 2; i++)
		mac_addr[i+4] = (u8)(rar_high >> (i*8));

	return 0;
}

s32 ixgbe_get_bus_info_generic(struct ixgbe_hw *hw)
{
	struct ixgbe_adapter *adapter = hw->back;
	struct ixgbe_mac_info *mac = &hw->mac;
	u16 link_status;

	hw->bus.type = ixgbe_bus_type_pci_express;

	
	pci_read_config_word(adapter->pdev, IXGBE_PCI_LINK_STATUS,
	                     &link_status);

	switch (link_status & IXGBE_PCI_LINK_WIDTH) {
	case IXGBE_PCI_LINK_WIDTH_1:
		hw->bus.width = ixgbe_bus_width_pcie_x1;
		break;
	case IXGBE_PCI_LINK_WIDTH_2:
		hw->bus.width = ixgbe_bus_width_pcie_x2;
		break;
	case IXGBE_PCI_LINK_WIDTH_4:
		hw->bus.width = ixgbe_bus_width_pcie_x4;
		break;
	case IXGBE_PCI_LINK_WIDTH_8:
		hw->bus.width = ixgbe_bus_width_pcie_x8;
		break;
	default:
		hw->bus.width = ixgbe_bus_width_unknown;
		break;
	}

	switch (link_status & IXGBE_PCI_LINK_SPEED) {
	case IXGBE_PCI_LINK_SPEED_2500:
		hw->bus.speed = ixgbe_bus_speed_2500;
		break;
	case IXGBE_PCI_LINK_SPEED_5000:
		hw->bus.speed = ixgbe_bus_speed_5000;
		break;
	default:
		hw->bus.speed = ixgbe_bus_speed_unknown;
		break;
	}

	mac->ops.set_lan_id(hw);

	return 0;
}

void ixgbe_set_lan_id_multi_port_pcie(struct ixgbe_hw *hw)
{
	struct ixgbe_bus_info *bus = &hw->bus;
	u32 reg;

	reg = IXGBE_READ_REG(hw, IXGBE_STATUS);
	bus->func = (reg & IXGBE_STATUS_LAN_ID) >> IXGBE_STATUS_LAN_ID_SHIFT;
	bus->lan_id = bus->func;

	
	reg = IXGBE_READ_REG(hw, IXGBE_FACTPS);
	if (reg & IXGBE_FACTPS_LFS)
		bus->func ^= 0x1;
}

s32 ixgbe_stop_adapter_generic(struct ixgbe_hw *hw)
{
	u32 reg_val;
	u16 i;

	hw->adapter_stopped = true;

	
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, 0);

	
	IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_IRQ_CLEAR_MASK);

	
	IXGBE_READ_REG(hw, IXGBE_EICR);

	
	for (i = 0; i < hw->mac.max_tx_queues; i++)
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(i), IXGBE_TXDCTL_SWFLSH);

	
	for (i = 0; i < hw->mac.max_rx_queues; i++) {
		reg_val = IXGBE_READ_REG(hw, IXGBE_RXDCTL(i));
		reg_val &= ~IXGBE_RXDCTL_ENABLE;
		reg_val |= IXGBE_RXDCTL_SWFLSH;
		IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(i), reg_val);
	}

	
	IXGBE_WRITE_FLUSH(hw);
	usleep_range(1000, 2000);

	return ixgbe_disable_pcie_master(hw);
}

s32 ixgbe_led_on_generic(struct ixgbe_hw *hw, u32 index)
{
	u32 led_reg = IXGBE_READ_REG(hw, IXGBE_LEDCTL);

	
	led_reg &= ~IXGBE_LED_MODE_MASK(index);
	led_reg |= IXGBE_LED_ON << IXGBE_LED_MODE_SHIFT(index);
	IXGBE_WRITE_REG(hw, IXGBE_LEDCTL, led_reg);
	IXGBE_WRITE_FLUSH(hw);

	return 0;
}

s32 ixgbe_led_off_generic(struct ixgbe_hw *hw, u32 index)
{
	u32 led_reg = IXGBE_READ_REG(hw, IXGBE_LEDCTL);

	
	led_reg &= ~IXGBE_LED_MODE_MASK(index);
	led_reg |= IXGBE_LED_OFF << IXGBE_LED_MODE_SHIFT(index);
	IXGBE_WRITE_REG(hw, IXGBE_LEDCTL, led_reg);
	IXGBE_WRITE_FLUSH(hw);

	return 0;
}

s32 ixgbe_init_eeprom_params_generic(struct ixgbe_hw *hw)
{
	struct ixgbe_eeprom_info *eeprom = &hw->eeprom;
	u32 eec;
	u16 eeprom_size;

	if (eeprom->type == ixgbe_eeprom_uninitialized) {
		eeprom->type = ixgbe_eeprom_none;
		eeprom->semaphore_delay = 10;
		
		eeprom->word_page_size = 0;

		eec = IXGBE_READ_REG(hw, IXGBE_EEC);
		if (eec & IXGBE_EEC_PRES) {
			eeprom->type = ixgbe_eeprom_spi;

			eeprom_size = (u16)((eec & IXGBE_EEC_SIZE) >>
					    IXGBE_EEC_SIZE_SHIFT);
			eeprom->word_size = 1 << (eeprom_size +
						  IXGBE_EEPROM_WORD_SIZE_SHIFT);
		}

		if (eec & IXGBE_EEC_ADDR_SIZE)
			eeprom->address_bits = 16;
		else
			eeprom->address_bits = 8;
		hw_dbg(hw, "Eeprom params: type = %d, size = %d, address bits: "
			  "%d\n", eeprom->type, eeprom->word_size,
			  eeprom->address_bits);
	}

	return 0;
}

s32 ixgbe_write_eeprom_buffer_bit_bang_generic(struct ixgbe_hw *hw, u16 offset,
					       u16 words, u16 *data)
{
	s32 status = 0;
	u16 i, count;

	hw->eeprom.ops.init_params(hw);

	if (words == 0) {
		status = IXGBE_ERR_INVALID_ARGUMENT;
		goto out;
	}

	if (offset + words > hw->eeprom.word_size) {
		status = IXGBE_ERR_EEPROM;
		goto out;
	}

	if ((hw->eeprom.word_page_size == 0) &&
	    (words > IXGBE_EEPROM_PAGE_SIZE_MAX))
		ixgbe_detect_eeprom_page_size_generic(hw, offset);

	for (i = 0; i < words; i += IXGBE_EEPROM_RD_BUFFER_MAX_COUNT) {
		count = (words - i) / IXGBE_EEPROM_RD_BUFFER_MAX_COUNT > 0 ?
			 IXGBE_EEPROM_RD_BUFFER_MAX_COUNT : (words - i);
		status = ixgbe_write_eeprom_buffer_bit_bang(hw, offset + i,
							    count, &data[i]);

		if (status != 0)
			break;
	}

out:
	return status;
}

/**
 *  ixgbe_write_eeprom_buffer_bit_bang - Writes 16 bit word(s) to EEPROM
 *  @hw: pointer to hardware structure
 *  @offset: offset within the EEPROM to be written to
 *  @words: number of word(s)
 *  @data: 16 bit word(s) to be written to the EEPROM
 *
 *  If ixgbe_eeprom_update_checksum is not called after this function, the
 *  EEPROM will most likely contain an invalid checksum.
 **/
static s32 ixgbe_write_eeprom_buffer_bit_bang(struct ixgbe_hw *hw, u16 offset,
					      u16 words, u16 *data)
{
	s32 status;
	u16 word;
	u16 page_size;
	u16 i;
	u8 write_opcode = IXGBE_EEPROM_WRITE_OPCODE_SPI;

	
	status = ixgbe_acquire_eeprom(hw);

	if (status == 0) {
		if (ixgbe_ready_eeprom(hw) != 0) {
			ixgbe_release_eeprom(hw);
			status = IXGBE_ERR_EEPROM;
		}
	}

	if (status == 0) {
		for (i = 0; i < words; i++) {
			ixgbe_standby_eeprom(hw);

			
			ixgbe_shift_out_eeprom_bits(hw,
						  IXGBE_EEPROM_WREN_OPCODE_SPI,
						  IXGBE_EEPROM_OPCODE_BITS);

			ixgbe_standby_eeprom(hw);

			if ((hw->eeprom.address_bits == 8) &&
			    ((offset + i) >= 128))
				write_opcode |= IXGBE_EEPROM_A8_OPCODE_SPI;

			
			ixgbe_shift_out_eeprom_bits(hw, write_opcode,
						    IXGBE_EEPROM_OPCODE_BITS);
			ixgbe_shift_out_eeprom_bits(hw, (u16)((offset + i) * 2),
						    hw->eeprom.address_bits);

			page_size = hw->eeprom.word_page_size;

			
			do {
				word = data[i];
				word = (word >> 8) | (word << 8);
				ixgbe_shift_out_eeprom_bits(hw, word, 16);

				if (page_size == 0)
					break;

				
				if (((offset + i) & (page_size - 1)) ==
				    (page_size - 1))
					break;
			} while (++i < words);

			ixgbe_standby_eeprom(hw);
			usleep_range(10000, 20000);
		}
		
		ixgbe_release_eeprom(hw);
	}

	return status;
}

/**
 *  ixgbe_write_eeprom_generic - Writes 16 bit value to EEPROM
 *  @hw: pointer to hardware structure
 *  @offset: offset within the EEPROM to be written to
 *  @data: 16 bit word to be written to the EEPROM
 *
 *  If ixgbe_eeprom_update_checksum is not called after this function, the
 *  EEPROM will most likely contain an invalid checksum.
 **/
s32 ixgbe_write_eeprom_generic(struct ixgbe_hw *hw, u16 offset, u16 data)
{
	s32 status;

	hw->eeprom.ops.init_params(hw);

	if (offset >= hw->eeprom.word_size) {
		status = IXGBE_ERR_EEPROM;
		goto out;
	}

	status = ixgbe_write_eeprom_buffer_bit_bang(hw, offset, 1, &data);

out:
	return status;
}

s32 ixgbe_read_eeprom_buffer_bit_bang_generic(struct ixgbe_hw *hw, u16 offset,
					      u16 words, u16 *data)
{
	s32 status = 0;
	u16 i, count;

	hw->eeprom.ops.init_params(hw);

	if (words == 0) {
		status = IXGBE_ERR_INVALID_ARGUMENT;
		goto out;
	}

	if (offset + words > hw->eeprom.word_size) {
		status = IXGBE_ERR_EEPROM;
		goto out;
	}

	for (i = 0; i < words; i += IXGBE_EEPROM_RD_BUFFER_MAX_COUNT) {
		count = (words - i) / IXGBE_EEPROM_RD_BUFFER_MAX_COUNT > 0 ?
			 IXGBE_EEPROM_RD_BUFFER_MAX_COUNT : (words - i);

		status = ixgbe_read_eeprom_buffer_bit_bang(hw, offset + i,
							   count, &data[i]);

		if (status != 0)
			break;
	}

out:
	return status;
}

static s32 ixgbe_read_eeprom_buffer_bit_bang(struct ixgbe_hw *hw, u16 offset,
					     u16 words, u16 *data)
{
	s32 status;
	u16 word_in;
	u8 read_opcode = IXGBE_EEPROM_READ_OPCODE_SPI;
	u16 i;

	
	status = ixgbe_acquire_eeprom(hw);

	if (status == 0) {
		if (ixgbe_ready_eeprom(hw) != 0) {
			ixgbe_release_eeprom(hw);
			status = IXGBE_ERR_EEPROM;
		}
	}

	if (status == 0) {
		for (i = 0; i < words; i++) {
			ixgbe_standby_eeprom(hw);
			if ((hw->eeprom.address_bits == 8) &&
			    ((offset + i) >= 128))
				read_opcode |= IXGBE_EEPROM_A8_OPCODE_SPI;

			
			ixgbe_shift_out_eeprom_bits(hw, read_opcode,
						    IXGBE_EEPROM_OPCODE_BITS);
			ixgbe_shift_out_eeprom_bits(hw, (u16)((offset + i) * 2),
						    hw->eeprom.address_bits);

			
			word_in = ixgbe_shift_in_eeprom_bits(hw, 16);
			data[i] = (word_in >> 8) | (word_in << 8);
		}

		
		ixgbe_release_eeprom(hw);
	}

	return status;
}

s32 ixgbe_read_eeprom_bit_bang_generic(struct ixgbe_hw *hw, u16 offset,
				       u16 *data)
{
	s32 status;

	hw->eeprom.ops.init_params(hw);

	if (offset >= hw->eeprom.word_size) {
		status = IXGBE_ERR_EEPROM;
		goto out;
	}

	status = ixgbe_read_eeprom_buffer_bit_bang(hw, offset, 1, data);

out:
	return status;
}

s32 ixgbe_read_eerd_buffer_generic(struct ixgbe_hw *hw, u16 offset,
				   u16 words, u16 *data)
{
	u32 eerd;
	s32 status = 0;
	u32 i;

	hw->eeprom.ops.init_params(hw);

	if (words == 0) {
		status = IXGBE_ERR_INVALID_ARGUMENT;
		goto out;
	}

	if (offset >= hw->eeprom.word_size) {
		status = IXGBE_ERR_EEPROM;
		goto out;
	}

	for (i = 0; i < words; i++) {
		eerd = ((offset + i) << IXGBE_EEPROM_RW_ADDR_SHIFT) +
		       IXGBE_EEPROM_RW_REG_START;

		IXGBE_WRITE_REG(hw, IXGBE_EERD, eerd);
		status = ixgbe_poll_eerd_eewr_done(hw, IXGBE_NVM_POLL_READ);

		if (status == 0) {
			data[i] = (IXGBE_READ_REG(hw, IXGBE_EERD) >>
				   IXGBE_EEPROM_RW_REG_DATA);
		} else {
			hw_dbg(hw, "Eeprom read timed out\n");
			goto out;
		}
	}
out:
	return status;
}

/**
 *  ixgbe_detect_eeprom_page_size_generic - Detect EEPROM page size
 *  @hw: pointer to hardware structure
 *  @offset: offset within the EEPROM to be used as a scratch pad
 *
 *  Discover EEPROM page size by writing marching data at given offset.
 *  This function is called only when we are writing a new large buffer
 *  at given offset so the data would be overwritten anyway.
 **/
static s32 ixgbe_detect_eeprom_page_size_generic(struct ixgbe_hw *hw,
						 u16 offset)
{
	u16 data[IXGBE_EEPROM_PAGE_SIZE_MAX];
	s32 status = 0;
	u16 i;

	for (i = 0; i < IXGBE_EEPROM_PAGE_SIZE_MAX; i++)
		data[i] = i;

	hw->eeprom.word_page_size = IXGBE_EEPROM_PAGE_SIZE_MAX;
	status = ixgbe_write_eeprom_buffer_bit_bang(hw, offset,
					     IXGBE_EEPROM_PAGE_SIZE_MAX, data);
	hw->eeprom.word_page_size = 0;
	if (status != 0)
		goto out;

	status = ixgbe_read_eeprom_buffer_bit_bang(hw, offset, 1, data);
	if (status != 0)
		goto out;

	hw->eeprom.word_page_size = IXGBE_EEPROM_PAGE_SIZE_MAX - data[0];

	hw_dbg(hw, "Detected EEPROM page size = %d words.",
	       hw->eeprom.word_page_size);
out:
	return status;
}

s32 ixgbe_read_eerd_generic(struct ixgbe_hw *hw, u16 offset, u16 *data)
{
	return ixgbe_read_eerd_buffer_generic(hw, offset, 1, data);
}

s32 ixgbe_write_eewr_buffer_generic(struct ixgbe_hw *hw, u16 offset,
				    u16 words, u16 *data)
{
	u32 eewr;
	s32 status = 0;
	u16 i;

	hw->eeprom.ops.init_params(hw);

	if (words == 0) {
		status = IXGBE_ERR_INVALID_ARGUMENT;
		goto out;
	}

	if (offset >= hw->eeprom.word_size) {
		status = IXGBE_ERR_EEPROM;
		goto out;
	}

	for (i = 0; i < words; i++) {
		eewr = ((offset + i) << IXGBE_EEPROM_RW_ADDR_SHIFT) |
		       (data[i] << IXGBE_EEPROM_RW_REG_DATA) |
		       IXGBE_EEPROM_RW_REG_START;

		status = ixgbe_poll_eerd_eewr_done(hw, IXGBE_NVM_POLL_WRITE);
		if (status != 0) {
			hw_dbg(hw, "Eeprom write EEWR timed out\n");
			goto out;
		}

		IXGBE_WRITE_REG(hw, IXGBE_EEWR, eewr);

		status = ixgbe_poll_eerd_eewr_done(hw, IXGBE_NVM_POLL_WRITE);
		if (status != 0) {
			hw_dbg(hw, "Eeprom write EEWR timed out\n");
			goto out;
		}
	}

out:
	return status;
}

s32 ixgbe_write_eewr_generic(struct ixgbe_hw *hw, u16 offset, u16 data)
{
	return ixgbe_write_eewr_buffer_generic(hw, offset, 1, &data);
}

static s32 ixgbe_poll_eerd_eewr_done(struct ixgbe_hw *hw, u32 ee_reg)
{
	u32 i;
	u32 reg;
	s32 status = IXGBE_ERR_EEPROM;

	for (i = 0; i < IXGBE_EERD_EEWR_ATTEMPTS; i++) {
		if (ee_reg == IXGBE_NVM_POLL_READ)
			reg = IXGBE_READ_REG(hw, IXGBE_EERD);
		else
			reg = IXGBE_READ_REG(hw, IXGBE_EEWR);

		if (reg & IXGBE_EEPROM_RW_REG_DONE) {
			status = 0;
			break;
		}
		udelay(5);
	}
	return status;
}

static s32 ixgbe_acquire_eeprom(struct ixgbe_hw *hw)
{
	s32 status = 0;
	u32 eec;
	u32 i;

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_EEP_SM) != 0)
		status = IXGBE_ERR_SWFW_SYNC;

	if (status == 0) {
		eec = IXGBE_READ_REG(hw, IXGBE_EEC);

		
		eec |= IXGBE_EEC_REQ;
		IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);

		for (i = 0; i < IXGBE_EEPROM_GRANT_ATTEMPTS; i++) {
			eec = IXGBE_READ_REG(hw, IXGBE_EEC);
			if (eec & IXGBE_EEC_GNT)
				break;
			udelay(5);
		}

		
		if (!(eec & IXGBE_EEC_GNT)) {
			eec &= ~IXGBE_EEC_REQ;
			IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
			hw_dbg(hw, "Could not acquire EEPROM grant\n");

			hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);
			status = IXGBE_ERR_EEPROM;
		}

		
		if (status == 0) {
			
			eec &= ~(IXGBE_EEC_CS | IXGBE_EEC_SK);
			IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
			IXGBE_WRITE_FLUSH(hw);
			udelay(1);
		}
	}
	return status;
}

static s32 ixgbe_get_eeprom_semaphore(struct ixgbe_hw *hw)
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

	if (i == timeout) {
		hw_dbg(hw, "Driver can't access the Eeprom - SMBI Semaphore "
		       "not granted.\n");
		ixgbe_release_eeprom_semaphore(hw);

		udelay(50);
		swsm = IXGBE_READ_REG(hw, IXGBE_SWSM);
		if (!(swsm & IXGBE_SWSM_SMBI))
			status = 0;
	}

	
	if (status == 0) {
		for (i = 0; i < timeout; i++) {
			swsm = IXGBE_READ_REG(hw, IXGBE_SWSM);

			
			swsm |= IXGBE_SWSM_SWESMBI;
			IXGBE_WRITE_REG(hw, IXGBE_SWSM, swsm);

			swsm = IXGBE_READ_REG(hw, IXGBE_SWSM);
			if (swsm & IXGBE_SWSM_SWESMBI)
				break;

			udelay(50);
		}

		if (i >= timeout) {
			hw_dbg(hw, "SWESMBI Software EEPROM semaphore "
			       "not granted.\n");
			ixgbe_release_eeprom_semaphore(hw);
			status = IXGBE_ERR_EEPROM;
		}
	} else {
		hw_dbg(hw, "Software semaphore SMBI between device drivers "
		       "not granted.\n");
	}

	return status;
}

static void ixgbe_release_eeprom_semaphore(struct ixgbe_hw *hw)
{
	u32 swsm;

	swsm = IXGBE_READ_REG(hw, IXGBE_SWSM);

	
	swsm &= ~(IXGBE_SWSM_SWESMBI | IXGBE_SWSM_SMBI);
	IXGBE_WRITE_REG(hw, IXGBE_SWSM, swsm);
	IXGBE_WRITE_FLUSH(hw);
}

static s32 ixgbe_ready_eeprom(struct ixgbe_hw *hw)
{
	s32 status = 0;
	u16 i;
	u8 spi_stat_reg;

	for (i = 0; i < IXGBE_EEPROM_MAX_RETRY_SPI; i += 5) {
		ixgbe_shift_out_eeprom_bits(hw, IXGBE_EEPROM_RDSR_OPCODE_SPI,
		                            IXGBE_EEPROM_OPCODE_BITS);
		spi_stat_reg = (u8)ixgbe_shift_in_eeprom_bits(hw, 8);
		if (!(spi_stat_reg & IXGBE_EEPROM_STATUS_RDY_SPI))
			break;

		udelay(5);
		ixgbe_standby_eeprom(hw);
	}

	if (i >= IXGBE_EEPROM_MAX_RETRY_SPI) {
		hw_dbg(hw, "SPI EEPROM Status error\n");
		status = IXGBE_ERR_EEPROM;
	}

	return status;
}

static void ixgbe_standby_eeprom(struct ixgbe_hw *hw)
{
	u32 eec;

	eec = IXGBE_READ_REG(hw, IXGBE_EEC);

	
	eec |= IXGBE_EEC_CS;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
	IXGBE_WRITE_FLUSH(hw);
	udelay(1);
	eec &= ~IXGBE_EEC_CS;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
	IXGBE_WRITE_FLUSH(hw);
	udelay(1);
}

static void ixgbe_shift_out_eeprom_bits(struct ixgbe_hw *hw, u16 data,
                                        u16 count)
{
	u32 eec;
	u32 mask;
	u32 i;

	eec = IXGBE_READ_REG(hw, IXGBE_EEC);

	mask = 0x01 << (count - 1);

	for (i = 0; i < count; i++) {
		if (data & mask)
			eec |= IXGBE_EEC_DI;
		else
			eec &= ~IXGBE_EEC_DI;

		IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
		IXGBE_WRITE_FLUSH(hw);

		udelay(1);

		ixgbe_raise_eeprom_clk(hw, &eec);
		ixgbe_lower_eeprom_clk(hw, &eec);

		mask = mask >> 1;
	}

	
	eec &= ~IXGBE_EEC_DI;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
	IXGBE_WRITE_FLUSH(hw);
}

static u16 ixgbe_shift_in_eeprom_bits(struct ixgbe_hw *hw, u16 count)
{
	u32 eec;
	u32 i;
	u16 data = 0;

	eec = IXGBE_READ_REG(hw, IXGBE_EEC);

	eec &= ~(IXGBE_EEC_DO | IXGBE_EEC_DI);

	for (i = 0; i < count; i++) {
		data = data << 1;
		ixgbe_raise_eeprom_clk(hw, &eec);

		eec = IXGBE_READ_REG(hw, IXGBE_EEC);

		eec &= ~(IXGBE_EEC_DI);
		if (eec & IXGBE_EEC_DO)
			data |= 1;

		ixgbe_lower_eeprom_clk(hw, &eec);
	}

	return data;
}

static void ixgbe_raise_eeprom_clk(struct ixgbe_hw *hw, u32 *eec)
{
	*eec = *eec | IXGBE_EEC_SK;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, *eec);
	IXGBE_WRITE_FLUSH(hw);
	udelay(1);
}

static void ixgbe_lower_eeprom_clk(struct ixgbe_hw *hw, u32 *eec)
{
	*eec = *eec & ~IXGBE_EEC_SK;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, *eec);
	IXGBE_WRITE_FLUSH(hw);
	udelay(1);
}

static void ixgbe_release_eeprom(struct ixgbe_hw *hw)
{
	u32 eec;

	eec = IXGBE_READ_REG(hw, IXGBE_EEC);

	eec |= IXGBE_EEC_CS;  
	eec &= ~IXGBE_EEC_SK; 

	IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);
	IXGBE_WRITE_FLUSH(hw);

	udelay(1);

	
	eec &= ~IXGBE_EEC_REQ;
	IXGBE_WRITE_REG(hw, IXGBE_EEC, eec);

	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_EEP_SM);

	usleep_range(hw->eeprom.semaphore_delay * 1000,
		     hw->eeprom.semaphore_delay * 2000);
}

u16 ixgbe_calc_eeprom_checksum_generic(struct ixgbe_hw *hw)
{
	u16 i;
	u16 j;
	u16 checksum = 0;
	u16 length = 0;
	u16 pointer = 0;
	u16 word = 0;

	
	for (i = 0; i < IXGBE_EEPROM_CHECKSUM; i++) {
		if (hw->eeprom.ops.read(hw, i, &word) != 0) {
			hw_dbg(hw, "EEPROM read failed\n");
			break;
		}
		checksum += word;
	}

	
	for (i = IXGBE_PCIE_ANALOG_PTR; i < IXGBE_FW_PTR; i++) {
		hw->eeprom.ops.read(hw, i, &pointer);

		
		if (pointer != 0xFFFF && pointer != 0) {
			hw->eeprom.ops.read(hw, pointer, &length);

			if (length != 0xFFFF && length != 0) {
				for (j = pointer+1; j <= pointer+length; j++) {
					hw->eeprom.ops.read(hw, j, &word);
					checksum += word;
				}
			}
		}
	}

	checksum = (u16)IXGBE_EEPROM_SUM - checksum;

	return checksum;
}

s32 ixgbe_validate_eeprom_checksum_generic(struct ixgbe_hw *hw,
                                           u16 *checksum_val)
{
	s32 status;
	u16 checksum;
	u16 read_checksum = 0;

	status = hw->eeprom.ops.read(hw, 0, &checksum);

	if (status == 0) {
		checksum = hw->eeprom.ops.calc_checksum(hw);

		hw->eeprom.ops.read(hw, IXGBE_EEPROM_CHECKSUM, &read_checksum);

		if (read_checksum != checksum)
			status = IXGBE_ERR_EEPROM_CHECKSUM;

		
		if (checksum_val)
			*checksum_val = checksum;
	} else {
		hw_dbg(hw, "EEPROM read failed\n");
	}

	return status;
}

s32 ixgbe_update_eeprom_checksum_generic(struct ixgbe_hw *hw)
{
	s32 status;
	u16 checksum;

	status = hw->eeprom.ops.read(hw, 0, &checksum);

	if (status == 0) {
		checksum = hw->eeprom.ops.calc_checksum(hw);
		status = hw->eeprom.ops.write(hw, IXGBE_EEPROM_CHECKSUM,
					      checksum);
	} else {
		hw_dbg(hw, "EEPROM read failed\n");
	}

	return status;
}

s32 ixgbe_validate_mac_addr(u8 *mac_addr)
{
	s32 status = 0;

	
	if (IXGBE_IS_MULTICAST(mac_addr))
		status = IXGBE_ERR_INVALID_MAC_ADDR;
	
	else if (IXGBE_IS_BROADCAST(mac_addr))
		status = IXGBE_ERR_INVALID_MAC_ADDR;
	
	else if (mac_addr[0] == 0 && mac_addr[1] == 0 && mac_addr[2] == 0 &&
	         mac_addr[3] == 0 && mac_addr[4] == 0 && mac_addr[5] == 0)
		status = IXGBE_ERR_INVALID_MAC_ADDR;

	return status;
}

s32 ixgbe_set_rar_generic(struct ixgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
                          u32 enable_addr)
{
	u32 rar_low, rar_high;
	u32 rar_entries = hw->mac.num_rar_entries;

	
	if (index >= rar_entries) {
		hw_dbg(hw, "RAR index %d is out of range.\n", index);
		return IXGBE_ERR_INVALID_ARGUMENT;
	}

	
	hw->mac.ops.set_vmdq(hw, index, vmdq);

	rar_low = ((u32)addr[0] |
		   ((u32)addr[1] << 8) |
		   ((u32)addr[2] << 16) |
		   ((u32)addr[3] << 24));
	rar_high = IXGBE_READ_REG(hw, IXGBE_RAH(index));
	rar_high &= ~(0x0000FFFF | IXGBE_RAH_AV);
	rar_high |= ((u32)addr[4] | ((u32)addr[5] << 8));

	if (enable_addr != 0)
		rar_high |= IXGBE_RAH_AV;

	IXGBE_WRITE_REG(hw, IXGBE_RAL(index), rar_low);
	IXGBE_WRITE_REG(hw, IXGBE_RAH(index), rar_high);

	return 0;
}

s32 ixgbe_clear_rar_generic(struct ixgbe_hw *hw, u32 index)
{
	u32 rar_high;
	u32 rar_entries = hw->mac.num_rar_entries;

	
	if (index >= rar_entries) {
		hw_dbg(hw, "RAR index %d is out of range.\n", index);
		return IXGBE_ERR_INVALID_ARGUMENT;
	}

	rar_high = IXGBE_READ_REG(hw, IXGBE_RAH(index));
	rar_high &= ~(0x0000FFFF | IXGBE_RAH_AV);

	IXGBE_WRITE_REG(hw, IXGBE_RAL(index), 0);
	IXGBE_WRITE_REG(hw, IXGBE_RAH(index), rar_high);

	
	hw->mac.ops.clear_vmdq(hw, index, IXGBE_CLEAR_VMDQ_ALL);

	return 0;
}

s32 ixgbe_init_rx_addrs_generic(struct ixgbe_hw *hw)
{
	u32 i;
	u32 rar_entries = hw->mac.num_rar_entries;

	if (ixgbe_validate_mac_addr(hw->mac.addr) ==
	    IXGBE_ERR_INVALID_MAC_ADDR) {
		
		hw->mac.ops.get_mac_addr(hw, hw->mac.addr);

		hw_dbg(hw, " Keeping Current RAR0 Addr =%pM\n", hw->mac.addr);
	} else {
		
		hw_dbg(hw, "Overriding MAC Address in RAR[0]\n");
		hw_dbg(hw, " New MAC Addr =%pM\n", hw->mac.addr);

		hw->mac.ops.set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);

		
		hw->mac.ops.clear_vmdq(hw, 0, IXGBE_CLEAR_VMDQ_ALL);
	}
	hw->addr_ctrl.overflow_promisc = 0;

	hw->addr_ctrl.rar_used_count = 1;

	
	hw_dbg(hw, "Clearing RAR[1-%d]\n", rar_entries - 1);
	for (i = 1; i < rar_entries; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_RAL(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_RAH(i), 0);
	}

	
	hw->addr_ctrl.mta_in_use = 0;
	IXGBE_WRITE_REG(hw, IXGBE_MCSTCTRL, hw->mac.mc_filter_type);

	hw_dbg(hw, " Clearing MTA\n");
	for (i = 0; i < hw->mac.mcft_size; i++)
		IXGBE_WRITE_REG(hw, IXGBE_MTA(i), 0);

	if (hw->mac.ops.init_uta_tables)
		hw->mac.ops.init_uta_tables(hw);

	return 0;
}

static s32 ixgbe_mta_vector(struct ixgbe_hw *hw, u8 *mc_addr)
{
	u32 vector = 0;

	switch (hw->mac.mc_filter_type) {
	case 0:   
		vector = ((mc_addr[4] >> 4) | (((u16)mc_addr[5]) << 4));
		break;
	case 1:   
		vector = ((mc_addr[4] >> 3) | (((u16)mc_addr[5]) << 5));
		break;
	case 2:   
		vector = ((mc_addr[4] >> 2) | (((u16)mc_addr[5]) << 6));
		break;
	case 3:   
		vector = ((mc_addr[4]) | (((u16)mc_addr[5]) << 8));
		break;
	default:  
		hw_dbg(hw, "MC filter type param set incorrectly\n");
		break;
	}

	
	vector &= 0xFFF;
	return vector;
}

static void ixgbe_set_mta(struct ixgbe_hw *hw, u8 *mc_addr)
{
	u32 vector;
	u32 vector_bit;
	u32 vector_reg;

	hw->addr_ctrl.mta_in_use++;

	vector = ixgbe_mta_vector(hw, mc_addr);
	hw_dbg(hw, " bit-vector = 0x%03X\n", vector);

	vector_reg = (vector >> 5) & 0x7F;
	vector_bit = vector & 0x1F;
	hw->mac.mta_shadow[vector_reg] |= (1 << vector_bit);
}

s32 ixgbe_update_mc_addr_list_generic(struct ixgbe_hw *hw,
				      struct net_device *netdev)
{
	struct netdev_hw_addr *ha;
	u32 i;

	hw->addr_ctrl.num_mc_addrs = netdev_mc_count(netdev);
	hw->addr_ctrl.mta_in_use = 0;

	
	hw_dbg(hw, " Clearing MTA\n");
	memset(&hw->mac.mta_shadow, 0, sizeof(hw->mac.mta_shadow));

	
	netdev_for_each_mc_addr(ha, netdev) {
		hw_dbg(hw, " Adding the multicast addresses:\n");
		ixgbe_set_mta(hw, ha->addr);
	}

	
	for (i = 0; i < hw->mac.mcft_size; i++)
		IXGBE_WRITE_REG_ARRAY(hw, IXGBE_MTA(0), i,
				      hw->mac.mta_shadow[i]);

	if (hw->addr_ctrl.mta_in_use > 0)
		IXGBE_WRITE_REG(hw, IXGBE_MCSTCTRL,
		                IXGBE_MCSTCTRL_MFE | hw->mac.mc_filter_type);

	hw_dbg(hw, "ixgbe_update_mc_addr_list_generic Complete\n");
	return 0;
}

s32 ixgbe_enable_mc_generic(struct ixgbe_hw *hw)
{
	struct ixgbe_addr_filter_info *a = &hw->addr_ctrl;

	if (a->mta_in_use > 0)
		IXGBE_WRITE_REG(hw, IXGBE_MCSTCTRL, IXGBE_MCSTCTRL_MFE |
		                hw->mac.mc_filter_type);

	return 0;
}

s32 ixgbe_disable_mc_generic(struct ixgbe_hw *hw)
{
	struct ixgbe_addr_filter_info *a = &hw->addr_ctrl;

	if (a->mta_in_use > 0)
		IXGBE_WRITE_REG(hw, IXGBE_MCSTCTRL, hw->mac.mc_filter_type);

	return 0;
}

s32 ixgbe_fc_enable_generic(struct ixgbe_hw *hw, s32 packetbuf_num)
{
	s32 ret_val = 0;
	u32 mflcn_reg, fccfg_reg;
	u32 reg;
	u32 fcrtl, fcrth;

#ifdef CONFIG_DCB
	if (hw->fc.requested_mode == ixgbe_fc_pfc)
		goto out;

#endif 
	
	ret_val = ixgbe_fc_autoneg(hw);
	if (ret_val == IXGBE_ERR_FLOW_CONTROL)
		goto out;

	
	mflcn_reg = IXGBE_READ_REG(hw, IXGBE_MFLCN);
	mflcn_reg &= ~(IXGBE_MFLCN_RFCE | IXGBE_MFLCN_RPFCE);

	fccfg_reg = IXGBE_READ_REG(hw, IXGBE_FCCFG);
	fccfg_reg &= ~(IXGBE_FCCFG_TFCE_802_3X | IXGBE_FCCFG_TFCE_PRIORITY);

	switch (hw->fc.current_mode) {
	case ixgbe_fc_none:
		break;
	case ixgbe_fc_rx_pause:
		mflcn_reg |= IXGBE_MFLCN_RFCE;
		break;
	case ixgbe_fc_tx_pause:
		fccfg_reg |= IXGBE_FCCFG_TFCE_802_3X;
		break;
	case ixgbe_fc_full:
		
		mflcn_reg |= IXGBE_MFLCN_RFCE;
		fccfg_reg |= IXGBE_FCCFG_TFCE_802_3X;
		break;
#ifdef CONFIG_DCB
	case ixgbe_fc_pfc:
		goto out;
		break;
#endif 
	default:
		hw_dbg(hw, "Flow control param set incorrectly\n");
		ret_val = IXGBE_ERR_CONFIG;
		goto out;
		break;
	}

	
	mflcn_reg |= IXGBE_MFLCN_DPF;
	IXGBE_WRITE_REG(hw, IXGBE_MFLCN, mflcn_reg);
	IXGBE_WRITE_REG(hw, IXGBE_FCCFG, fccfg_reg);

	fcrtl = hw->fc.low_water << 10;

	if (hw->fc.current_mode & ixgbe_fc_tx_pause) {
		fcrth = hw->fc.high_water[packetbuf_num] << 10;
		fcrth |= IXGBE_FCRTH_FCEN;
		if (hw->fc.send_xon)
			fcrtl |= IXGBE_FCRTL_XONE;
	} else {
		fcrth = IXGBE_READ_REG(hw, IXGBE_RXPBSIZE(packetbuf_num)) - 32;
	}

	IXGBE_WRITE_REG(hw, IXGBE_FCRTH_82599(packetbuf_num), fcrth);
	IXGBE_WRITE_REG(hw, IXGBE_FCRTL_82599(packetbuf_num), fcrtl);

	
	reg = IXGBE_READ_REG(hw, IXGBE_FCTTV(packetbuf_num / 2));
	if ((packetbuf_num & 1) == 0)
		reg = (reg & 0xFFFF0000) | hw->fc.pause_time;
	else
		reg = (reg & 0x0000FFFF) | (hw->fc.pause_time << 16);
	IXGBE_WRITE_REG(hw, IXGBE_FCTTV(packetbuf_num / 2), reg);

	IXGBE_WRITE_REG(hw, IXGBE_FCRTV, (hw->fc.pause_time >> 1));

out:
	return ret_val;
}

s32 ixgbe_fc_autoneg(struct ixgbe_hw *hw)
{
	s32 ret_val = IXGBE_ERR_FC_NOT_NEGOTIATED;
	ixgbe_link_speed speed;
	bool link_up;

	if (hw->fc.disable_fc_autoneg)
		goto out;

	hw->mac.ops.check_link(hw, &speed, &link_up, false);
	if (!link_up) {
		ret_val = IXGBE_ERR_FLOW_CONTROL;
		goto out;
	}

	switch (hw->phy.media_type) {
	
	case ixgbe_media_type_fiber:
		if (speed == IXGBE_LINK_SPEED_1GB_FULL)
			ret_val = ixgbe_fc_autoneg_fiber(hw);
		break;

	
	case ixgbe_media_type_backplane:
		ret_val = ixgbe_fc_autoneg_backplane(hw);
		break;

	
	case ixgbe_media_type_copper:
		if (ixgbe_device_supports_autoneg_fc(hw) == 0)
			ret_val = ixgbe_fc_autoneg_copper(hw);
		break;

	default:
		break;
	}

out:
	if (ret_val == 0) {
		hw->fc.fc_was_autonegged = true;
	} else {
		hw->fc.fc_was_autonegged = false;
		hw->fc.current_mode = hw->fc.requested_mode;
	}
	return ret_val;
}

static s32 ixgbe_fc_autoneg_fiber(struct ixgbe_hw *hw)
{
	u32 pcs_anadv_reg, pcs_lpab_reg, linkstat;
	s32 ret_val;


	linkstat = IXGBE_READ_REG(hw, IXGBE_PCS1GLSTA);
	if ((!!(linkstat & IXGBE_PCS1GLSTA_AN_COMPLETE) == 0) ||
	    (!!(linkstat & IXGBE_PCS1GLSTA_AN_TIMED_OUT) == 1)) {
		ret_val = IXGBE_ERR_FC_NOT_NEGOTIATED;
		goto out;
	}

	pcs_anadv_reg = IXGBE_READ_REG(hw, IXGBE_PCS1GANA);
	pcs_lpab_reg = IXGBE_READ_REG(hw, IXGBE_PCS1GANLP);

	ret_val =  ixgbe_negotiate_fc(hw, pcs_anadv_reg,
			       pcs_lpab_reg, IXGBE_PCS1GANA_SYM_PAUSE,
			       IXGBE_PCS1GANA_ASM_PAUSE,
			       IXGBE_PCS1GANA_SYM_PAUSE,
			       IXGBE_PCS1GANA_ASM_PAUSE);

out:
	return ret_val;
}

static s32 ixgbe_fc_autoneg_backplane(struct ixgbe_hw *hw)
{
	u32 links2, anlp1_reg, autoc_reg, links;
	s32 ret_val;

	links = IXGBE_READ_REG(hw, IXGBE_LINKS);
	if ((links & IXGBE_LINKS_KX_AN_COMP) == 0) {
		hw->fc.fc_was_autonegged = false;
		hw->fc.current_mode = hw->fc.requested_mode;
		ret_val = IXGBE_ERR_FC_NOT_NEGOTIATED;
		goto out;
	}

	if (hw->mac.type == ixgbe_mac_82599EB) {
		links2 = IXGBE_READ_REG(hw, IXGBE_LINKS2);
		if ((links2 & IXGBE_LINKS2_AN_SUPPORTED) == 0) {
			hw->fc.fc_was_autonegged = false;
			hw->fc.current_mode = hw->fc.requested_mode;
			ret_val = IXGBE_ERR_FC_NOT_NEGOTIATED;
			goto out;
		}
	}
	autoc_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	anlp1_reg = IXGBE_READ_REG(hw, IXGBE_ANLP1);

	ret_val = ixgbe_negotiate_fc(hw, autoc_reg,
		anlp1_reg, IXGBE_AUTOC_SYM_PAUSE, IXGBE_AUTOC_ASM_PAUSE,
		IXGBE_ANLP1_SYM_PAUSE, IXGBE_ANLP1_ASM_PAUSE);

out:
	return ret_val;
}

static s32 ixgbe_fc_autoneg_copper(struct ixgbe_hw *hw)
{
	u16 technology_ability_reg = 0;
	u16 lp_technology_ability_reg = 0;

	hw->phy.ops.read_reg(hw, MDIO_AN_ADVERTISE,
			     MDIO_MMD_AN,
			     &technology_ability_reg);
	hw->phy.ops.read_reg(hw, MDIO_AN_LPA,
			     MDIO_MMD_AN,
			     &lp_technology_ability_reg);

	return ixgbe_negotiate_fc(hw, (u32)technology_ability_reg,
				  (u32)lp_technology_ability_reg,
				  IXGBE_TAF_SYM_PAUSE, IXGBE_TAF_ASM_PAUSE,
				  IXGBE_TAF_SYM_PAUSE, IXGBE_TAF_ASM_PAUSE);
}

static s32 ixgbe_negotiate_fc(struct ixgbe_hw *hw, u32 adv_reg, u32 lp_reg,
			      u32 adv_sym, u32 adv_asm, u32 lp_sym, u32 lp_asm)
{
	if ((!(adv_reg)) ||  (!(lp_reg)))
		return IXGBE_ERR_FC_NOT_NEGOTIATED;

	if ((adv_reg & adv_sym) && (lp_reg & lp_sym)) {
		if (hw->fc.requested_mode == ixgbe_fc_full) {
			hw->fc.current_mode = ixgbe_fc_full;
			hw_dbg(hw, "Flow Control = FULL.\n");
		} else {
			hw->fc.current_mode = ixgbe_fc_rx_pause;
			hw_dbg(hw, "Flow Control=RX PAUSE frames only\n");
		}
	} else if (!(adv_reg & adv_sym) && (adv_reg & adv_asm) &&
		   (lp_reg & lp_sym) && (lp_reg & lp_asm)) {
		hw->fc.current_mode = ixgbe_fc_tx_pause;
		hw_dbg(hw, "Flow Control = TX PAUSE frames only.\n");
	} else if ((adv_reg & adv_sym) && (adv_reg & adv_asm) &&
		   !(lp_reg & lp_sym) && (lp_reg & lp_asm)) {
		hw->fc.current_mode = ixgbe_fc_rx_pause;
		hw_dbg(hw, "Flow Control = RX PAUSE frames only.\n");
	} else {
		hw->fc.current_mode = ixgbe_fc_none;
		hw_dbg(hw, "Flow Control = NONE.\n");
	}
	return 0;
}

static s32 ixgbe_setup_fc(struct ixgbe_hw *hw, s32 packetbuf_num)
{
	s32 ret_val = 0;
	u32 reg = 0, reg_bp = 0;
	u16 reg_cu = 0;

#ifdef CONFIG_DCB
	if (hw->fc.requested_mode == ixgbe_fc_pfc) {
		hw->fc.current_mode = hw->fc.requested_mode;
		goto out;
	}

#endif 
	
	if (packetbuf_num < 0 || packetbuf_num > 7) {
		hw_dbg(hw, "Invalid packet buffer number [%d], expected range "
		       "is 0-7\n", packetbuf_num);
		ret_val = IXGBE_ERR_INVALID_LINK_SETTINGS;
		goto out;
	}

	if (!hw->fc.low_water ||
	    !hw->fc.high_water[packetbuf_num] ||
	    !hw->fc.pause_time) {
		hw_dbg(hw, "Invalid water mark configuration\n");
		ret_val = IXGBE_ERR_INVALID_LINK_SETTINGS;
		goto out;
	}

	if (hw->fc.strict_ieee && hw->fc.requested_mode == ixgbe_fc_rx_pause) {
		hw_dbg(hw, "ixgbe_fc_rx_pause not valid in strict "
		       "IEEE mode\n");
		ret_val = IXGBE_ERR_INVALID_LINK_SETTINGS;
		goto out;
	}

	if (hw->fc.requested_mode == ixgbe_fc_default)
		hw->fc.requested_mode = ixgbe_fc_full;


	switch (hw->phy.media_type) {
	case ixgbe_media_type_fiber:
	case ixgbe_media_type_backplane:
		reg = IXGBE_READ_REG(hw, IXGBE_PCS1GANA);
		reg_bp = IXGBE_READ_REG(hw, IXGBE_AUTOC);
		break;

	case ixgbe_media_type_copper:
		hw->phy.ops.read_reg(hw, MDIO_AN_ADVERTISE,
					MDIO_MMD_AN, &reg_cu);
		break;

	default:
		;
	}

	switch (hw->fc.requested_mode) {
	case ixgbe_fc_none:
		
		reg &= ~(IXGBE_PCS1GANA_SYM_PAUSE | IXGBE_PCS1GANA_ASM_PAUSE);
		if (hw->phy.media_type == ixgbe_media_type_backplane)
			reg_bp &= ~(IXGBE_AUTOC_SYM_PAUSE |
				    IXGBE_AUTOC_ASM_PAUSE);
		else if (hw->phy.media_type == ixgbe_media_type_copper)
			reg_cu &= ~(IXGBE_TAF_SYM_PAUSE | IXGBE_TAF_ASM_PAUSE);
		break;
	case ixgbe_fc_rx_pause:
		reg |= (IXGBE_PCS1GANA_SYM_PAUSE | IXGBE_PCS1GANA_ASM_PAUSE);
		if (hw->phy.media_type == ixgbe_media_type_backplane)
			reg_bp |= (IXGBE_AUTOC_SYM_PAUSE |
				   IXGBE_AUTOC_ASM_PAUSE);
		else if (hw->phy.media_type == ixgbe_media_type_copper)
			reg_cu |= (IXGBE_TAF_SYM_PAUSE | IXGBE_TAF_ASM_PAUSE);
		break;
	case ixgbe_fc_tx_pause:
		reg |= (IXGBE_PCS1GANA_ASM_PAUSE);
		reg &= ~(IXGBE_PCS1GANA_SYM_PAUSE);
		if (hw->phy.media_type == ixgbe_media_type_backplane) {
			reg_bp |= (IXGBE_AUTOC_ASM_PAUSE);
			reg_bp &= ~(IXGBE_AUTOC_SYM_PAUSE);
		} else if (hw->phy.media_type == ixgbe_media_type_copper) {
			reg_cu |= (IXGBE_TAF_ASM_PAUSE);
			reg_cu &= ~(IXGBE_TAF_SYM_PAUSE);
		}
		break;
	case ixgbe_fc_full:
		
		reg |= (IXGBE_PCS1GANA_SYM_PAUSE | IXGBE_PCS1GANA_ASM_PAUSE);
		if (hw->phy.media_type == ixgbe_media_type_backplane)
			reg_bp |= (IXGBE_AUTOC_SYM_PAUSE |
				   IXGBE_AUTOC_ASM_PAUSE);
		else if (hw->phy.media_type == ixgbe_media_type_copper)
			reg_cu |= (IXGBE_TAF_SYM_PAUSE | IXGBE_TAF_ASM_PAUSE);
		break;
#ifdef CONFIG_DCB
	case ixgbe_fc_pfc:
		goto out;
		break;
#endif 
	default:
		hw_dbg(hw, "Flow control param set incorrectly\n");
		ret_val = IXGBE_ERR_CONFIG;
		goto out;
		break;
	}

	if (hw->mac.type != ixgbe_mac_X540) {
		IXGBE_WRITE_REG(hw, IXGBE_PCS1GANA, reg);
		reg = IXGBE_READ_REG(hw, IXGBE_PCS1GLCTL);

		
		if (hw->fc.strict_ieee)
			reg &= ~IXGBE_PCS1GLCTL_AN_1G_TIMEOUT_EN;

		IXGBE_WRITE_REG(hw, IXGBE_PCS1GLCTL, reg);
		hw_dbg(hw, "Set up FC; PCS1GLCTL = 0x%08X\n", reg);
	}

	if (hw->phy.media_type == ixgbe_media_type_backplane) {
		reg_bp |= IXGBE_AUTOC_AN_RESTART;
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, reg_bp);
	} else if ((hw->phy.media_type == ixgbe_media_type_copper) &&
		    (ixgbe_device_supports_autoneg_fc(hw) == 0)) {
		hw->phy.ops.write_reg(hw, MDIO_AN_ADVERTISE,
				      MDIO_MMD_AN, reg_cu);
	}

	hw_dbg(hw, "Set up FC; IXGBE_AUTOC = 0x%08X\n", reg);
out:
	return ret_val;
}

static s32 ixgbe_disable_pcie_master(struct ixgbe_hw *hw)
{
	struct ixgbe_adapter *adapter = hw->back;
	s32 status = 0;
	u32 i;
	u16 value;

	
	IXGBE_WRITE_REG(hw, IXGBE_CTRL, IXGBE_CTRL_GIO_DIS);

	
	if (!(IXGBE_READ_REG(hw, IXGBE_STATUS) & IXGBE_STATUS_GIO))
		goto out;

	
	for (i = 0; i < IXGBE_PCI_MASTER_DISABLE_TIMEOUT; i++) {
		udelay(100);
		if (!(IXGBE_READ_REG(hw, IXGBE_STATUS) & IXGBE_STATUS_GIO))
			goto out;
	}

	hw_dbg(hw, "GIO Master Disable bit didn't clear - requesting resets\n");
	hw->mac.flags |= IXGBE_FLAGS_DOUBLE_RESET_REQUIRED;

	for (i = 0; i < IXGBE_PCI_MASTER_DISABLE_TIMEOUT; i++) {
		udelay(100);
		pci_read_config_word(adapter->pdev, IXGBE_PCI_DEVICE_STATUS,
							 &value);
		if (!(value & IXGBE_PCI_DEVICE_STATUS_TRANSACTION_PENDING))
			goto out;
	}

	hw_dbg(hw, "PCIe transaction pending bit also did not clear.\n");
	status = IXGBE_ERR_MASTER_REQUESTS_PENDING;

out:
	return status;
}

s32 ixgbe_acquire_swfw_sync(struct ixgbe_hw *hw, u16 mask)
{
	u32 gssr;
	u32 swmask = mask;
	u32 fwmask = mask << 5;
	s32 timeout = 200;

	while (timeout) {
		if (ixgbe_get_eeprom_semaphore(hw))
			return IXGBE_ERR_SWFW_SYNC;

		gssr = IXGBE_READ_REG(hw, IXGBE_GSSR);
		if (!(gssr & (fwmask | swmask)))
			break;

		ixgbe_release_eeprom_semaphore(hw);
		usleep_range(5000, 10000);
		timeout--;
	}

	if (!timeout) {
		hw_dbg(hw, "Driver can't access resource, SW_FW_SYNC timeout.\n");
		return IXGBE_ERR_SWFW_SYNC;
	}

	gssr |= swmask;
	IXGBE_WRITE_REG(hw, IXGBE_GSSR, gssr);

	ixgbe_release_eeprom_semaphore(hw);
	return 0;
}

void ixgbe_release_swfw_sync(struct ixgbe_hw *hw, u16 mask)
{
	u32 gssr;
	u32 swmask = mask;

	ixgbe_get_eeprom_semaphore(hw);

	gssr = IXGBE_READ_REG(hw, IXGBE_GSSR);
	gssr &= ~swmask;
	IXGBE_WRITE_REG(hw, IXGBE_GSSR, gssr);

	ixgbe_release_eeprom_semaphore(hw);
}

s32 ixgbe_disable_rx_buff_generic(struct ixgbe_hw *hw)
{
#define IXGBE_MAX_SECRX_POLL 40
	int i;
	int secrxreg;

	secrxreg = IXGBE_READ_REG(hw, IXGBE_SECRXCTRL);
	secrxreg |= IXGBE_SECRXCTRL_RX_DIS;
	IXGBE_WRITE_REG(hw, IXGBE_SECRXCTRL, secrxreg);
	for (i = 0; i < IXGBE_MAX_SECRX_POLL; i++) {
		secrxreg = IXGBE_READ_REG(hw, IXGBE_SECRXSTAT);
		if (secrxreg & IXGBE_SECRXSTAT_SECRX_RDY)
			break;
		else
			
			udelay(10);
	}

	
	if (i >= IXGBE_MAX_SECRX_POLL)
		hw_dbg(hw, "Rx unit being enabled before security "
		       "path fully disabled.  Continuing with init.\n");

	return 0;

}

s32 ixgbe_enable_rx_buff_generic(struct ixgbe_hw *hw)
{
	int secrxreg;

	secrxreg = IXGBE_READ_REG(hw, IXGBE_SECRXCTRL);
	secrxreg &= ~IXGBE_SECRXCTRL_RX_DIS;
	IXGBE_WRITE_REG(hw, IXGBE_SECRXCTRL, secrxreg);
	IXGBE_WRITE_FLUSH(hw);

	return 0;
}

s32 ixgbe_enable_rx_dma_generic(struct ixgbe_hw *hw, u32 regval)
{
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, regval);

	return 0;
}

s32 ixgbe_blink_led_start_generic(struct ixgbe_hw *hw, u32 index)
{
	ixgbe_link_speed speed = 0;
	bool link_up = false;
	u32 autoc_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	u32 led_reg = IXGBE_READ_REG(hw, IXGBE_LEDCTL);

	hw->mac.ops.check_link(hw, &speed, &link_up, false);

	if (!link_up) {
		autoc_reg |= IXGBE_AUTOC_AN_RESTART;
		autoc_reg |= IXGBE_AUTOC_FLU;
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc_reg);
		IXGBE_WRITE_FLUSH(hw);
		usleep_range(10000, 20000);
	}

	led_reg &= ~IXGBE_LED_MODE_MASK(index);
	led_reg |= IXGBE_LED_BLINK(index);
	IXGBE_WRITE_REG(hw, IXGBE_LEDCTL, led_reg);
	IXGBE_WRITE_FLUSH(hw);

	return 0;
}

s32 ixgbe_blink_led_stop_generic(struct ixgbe_hw *hw, u32 index)
{
	u32 autoc_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	u32 led_reg = IXGBE_READ_REG(hw, IXGBE_LEDCTL);

	autoc_reg &= ~IXGBE_AUTOC_FLU;
	autoc_reg |= IXGBE_AUTOC_AN_RESTART;
	IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc_reg);

	led_reg &= ~IXGBE_LED_MODE_MASK(index);
	led_reg &= ~IXGBE_LED_BLINK(index);
	led_reg |= IXGBE_LED_LINK_ACTIVE << IXGBE_LED_MODE_SHIFT(index);
	IXGBE_WRITE_REG(hw, IXGBE_LEDCTL, led_reg);
	IXGBE_WRITE_FLUSH(hw);

	return 0;
}

static s32 ixgbe_get_san_mac_addr_offset(struct ixgbe_hw *hw,
                                        u16 *san_mac_offset)
{
	hw->eeprom.ops.read(hw, IXGBE_SAN_MAC_ADDR_PTR, san_mac_offset);

	return 0;
}

s32 ixgbe_get_san_mac_addr_generic(struct ixgbe_hw *hw, u8 *san_mac_addr)
{
	u16 san_mac_data, san_mac_offset;
	u8 i;

	ixgbe_get_san_mac_addr_offset(hw, &san_mac_offset);

	if ((san_mac_offset == 0) || (san_mac_offset == 0xFFFF)) {
		for (i = 0; i < 6; i++)
			san_mac_addr[i] = 0xFF;

		goto san_mac_addr_out;
	}

	
	hw->mac.ops.set_lan_id(hw);
	
	(hw->bus.func) ? (san_mac_offset += IXGBE_SAN_MAC_ADDR_PORT1_OFFSET) :
	                 (san_mac_offset += IXGBE_SAN_MAC_ADDR_PORT0_OFFSET);
	for (i = 0; i < 3; i++) {
		hw->eeprom.ops.read(hw, san_mac_offset, &san_mac_data);
		san_mac_addr[i * 2] = (u8)(san_mac_data);
		san_mac_addr[i * 2 + 1] = (u8)(san_mac_data >> 8);
		san_mac_offset++;
	}

san_mac_addr_out:
	return 0;
}

u32 ixgbe_get_pcie_msix_count_generic(struct ixgbe_hw *hw)
{
	struct ixgbe_adapter *adapter = hw->back;
	u16 msix_count;
	pci_read_config_word(adapter->pdev, IXGBE_PCIE_MSIX_82599_CAPS,
	                     &msix_count);
	msix_count &= IXGBE_PCIE_MSIX_TBL_SZ_MASK;

	
	msix_count++;

	return msix_count;
}

s32 ixgbe_clear_vmdq_generic(struct ixgbe_hw *hw, u32 rar, u32 vmdq)
{
	u32 mpsar_lo, mpsar_hi;
	u32 rar_entries = hw->mac.num_rar_entries;

	
	if (rar >= rar_entries) {
		hw_dbg(hw, "RAR index %d is out of range.\n", rar);
		return IXGBE_ERR_INVALID_ARGUMENT;
	}

	mpsar_lo = IXGBE_READ_REG(hw, IXGBE_MPSAR_LO(rar));
	mpsar_hi = IXGBE_READ_REG(hw, IXGBE_MPSAR_HI(rar));

	if (!mpsar_lo && !mpsar_hi)
		goto done;

	if (vmdq == IXGBE_CLEAR_VMDQ_ALL) {
		if (mpsar_lo) {
			IXGBE_WRITE_REG(hw, IXGBE_MPSAR_LO(rar), 0);
			mpsar_lo = 0;
		}
		if (mpsar_hi) {
			IXGBE_WRITE_REG(hw, IXGBE_MPSAR_HI(rar), 0);
			mpsar_hi = 0;
		}
	} else if (vmdq < 32) {
		mpsar_lo &= ~(1 << vmdq);
		IXGBE_WRITE_REG(hw, IXGBE_MPSAR_LO(rar), mpsar_lo);
	} else {
		mpsar_hi &= ~(1 << (vmdq - 32));
		IXGBE_WRITE_REG(hw, IXGBE_MPSAR_HI(rar), mpsar_hi);
	}

	
	if (mpsar_lo == 0 && mpsar_hi == 0 && rar != 0)
		hw->mac.ops.clear_rar(hw, rar);
done:
	return 0;
}

s32 ixgbe_set_vmdq_generic(struct ixgbe_hw *hw, u32 rar, u32 vmdq)
{
	u32 mpsar;
	u32 rar_entries = hw->mac.num_rar_entries;

	
	if (rar >= rar_entries) {
		hw_dbg(hw, "RAR index %d is out of range.\n", rar);
		return IXGBE_ERR_INVALID_ARGUMENT;
	}

	if (vmdq < 32) {
		mpsar = IXGBE_READ_REG(hw, IXGBE_MPSAR_LO(rar));
		mpsar |= 1 << vmdq;
		IXGBE_WRITE_REG(hw, IXGBE_MPSAR_LO(rar), mpsar);
	} else {
		mpsar = IXGBE_READ_REG(hw, IXGBE_MPSAR_HI(rar));
		mpsar |= 1 << (vmdq - 32);
		IXGBE_WRITE_REG(hw, IXGBE_MPSAR_HI(rar), mpsar);
	}
	return 0;
}

s32 ixgbe_init_uta_tables_generic(struct ixgbe_hw *hw)
{
	int i;

	for (i = 0; i < 128; i++)
		IXGBE_WRITE_REG(hw, IXGBE_UTA(i), 0);

	return 0;
}

static s32 ixgbe_find_vlvf_slot(struct ixgbe_hw *hw, u32 vlan)
{
	u32 bits = 0;
	u32 first_empty_slot = 0;
	s32 regindex;

	
	if (vlan == 0)
		return 0;

	for (regindex = 1; regindex < IXGBE_VLVF_ENTRIES; regindex++) {
		bits = IXGBE_READ_REG(hw, IXGBE_VLVF(regindex));
		if (!bits && !(first_empty_slot))
			first_empty_slot = regindex;
		else if ((bits & 0x0FFF) == vlan)
			break;
	}

	if (regindex >= IXGBE_VLVF_ENTRIES) {
		if (first_empty_slot)
			regindex = first_empty_slot;
		else {
			hw_dbg(hw, "No space in VLVF.\n");
			regindex = IXGBE_ERR_NO_SPACE;
		}
	}

	return regindex;
}

s32 ixgbe_set_vfta_generic(struct ixgbe_hw *hw, u32 vlan, u32 vind,
                           bool vlan_on)
{
	s32 regindex;
	u32 bitindex;
	u32 vfta;
	u32 bits;
	u32 vt;
	u32 targetbit;
	bool vfta_changed = false;

	if (vlan > 4095)
		return IXGBE_ERR_PARAM;


	regindex = (vlan >> 5) & 0x7F;
	bitindex = vlan & 0x1F;
	targetbit = (1 << bitindex);
	vfta = IXGBE_READ_REG(hw, IXGBE_VFTA(regindex));

	if (vlan_on) {
		if (!(vfta & targetbit)) {
			vfta |= targetbit;
			vfta_changed = true;
		}
	} else {
		if ((vfta & targetbit)) {
			vfta &= ~targetbit;
			vfta_changed = true;
		}
	}

	vt = IXGBE_READ_REG(hw, IXGBE_VT_CTL);
	if (vt & IXGBE_VT_CTL_VT_ENABLE) {
		s32 vlvf_index;

		vlvf_index = ixgbe_find_vlvf_slot(hw, vlan);
		if (vlvf_index < 0)
			return vlvf_index;

		if (vlan_on) {
			
			if (vind < 32) {
				bits = IXGBE_READ_REG(hw,
						IXGBE_VLVFB(vlvf_index*2));
				bits |= (1 << vind);
				IXGBE_WRITE_REG(hw,
						IXGBE_VLVFB(vlvf_index*2),
						bits);
			} else {
				bits = IXGBE_READ_REG(hw,
						IXGBE_VLVFB((vlvf_index*2)+1));
				bits |= (1 << (vind-32));
				IXGBE_WRITE_REG(hw,
						IXGBE_VLVFB((vlvf_index*2)+1),
						bits);
			}
		} else {
			
			if (vind < 32) {
				bits = IXGBE_READ_REG(hw,
						IXGBE_VLVFB(vlvf_index*2));
				bits &= ~(1 << vind);
				IXGBE_WRITE_REG(hw,
						IXGBE_VLVFB(vlvf_index*2),
						bits);
				bits |= IXGBE_READ_REG(hw,
						IXGBE_VLVFB((vlvf_index*2)+1));
			} else {
				bits = IXGBE_READ_REG(hw,
						IXGBE_VLVFB((vlvf_index*2)+1));
				bits &= ~(1 << (vind-32));
				IXGBE_WRITE_REG(hw,
						IXGBE_VLVFB((vlvf_index*2)+1),
						bits);
				bits |= IXGBE_READ_REG(hw,
						IXGBE_VLVFB(vlvf_index*2));
			}
		}

		if (bits) {
			IXGBE_WRITE_REG(hw, IXGBE_VLVF(vlvf_index),
					(IXGBE_VLVF_VIEN | vlan));
			if (!vlan_on) {
				vfta_changed = false;
			}
		}
		else
			IXGBE_WRITE_REG(hw, IXGBE_VLVF(vlvf_index), 0);
	}

	if (vfta_changed)
		IXGBE_WRITE_REG(hw, IXGBE_VFTA(regindex), vfta);

	return 0;
}

s32 ixgbe_clear_vfta_generic(struct ixgbe_hw *hw)
{
	u32 offset;

	for (offset = 0; offset < hw->mac.vft_size; offset++)
		IXGBE_WRITE_REG(hw, IXGBE_VFTA(offset), 0);

	for (offset = 0; offset < IXGBE_VLVF_ENTRIES; offset++) {
		IXGBE_WRITE_REG(hw, IXGBE_VLVF(offset), 0);
		IXGBE_WRITE_REG(hw, IXGBE_VLVFB(offset*2), 0);
		IXGBE_WRITE_REG(hw, IXGBE_VLVFB((offset*2)+1), 0);
	}

	return 0;
}

s32 ixgbe_check_mac_link_generic(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
				 bool *link_up, bool link_up_wait_to_complete)
{
	u32 links_reg, links_orig;
	u32 i;

	
	links_orig = IXGBE_READ_REG(hw, IXGBE_LINKS);

	links_reg = IXGBE_READ_REG(hw, IXGBE_LINKS);

	if (links_orig != links_reg) {
		hw_dbg(hw, "LINKS changed from %08X to %08X\n",
		       links_orig, links_reg);
	}

	if (link_up_wait_to_complete) {
		for (i = 0; i < IXGBE_LINK_UP_TIME; i++) {
			if (links_reg & IXGBE_LINKS_UP) {
				*link_up = true;
				break;
			} else {
				*link_up = false;
			}
			msleep(100);
			links_reg = IXGBE_READ_REG(hw, IXGBE_LINKS);
		}
	} else {
		if (links_reg & IXGBE_LINKS_UP)
			*link_up = true;
		else
			*link_up = false;
	}

	if ((links_reg & IXGBE_LINKS_SPEED_82599) ==
	    IXGBE_LINKS_SPEED_10G_82599)
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
	else if ((links_reg & IXGBE_LINKS_SPEED_82599) ==
		 IXGBE_LINKS_SPEED_1G_82599)
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
	else if ((links_reg & IXGBE_LINKS_SPEED_82599) ==
		 IXGBE_LINKS_SPEED_100_82599)
		*speed = IXGBE_LINK_SPEED_100_FULL;
	else
		*speed = IXGBE_LINK_SPEED_UNKNOWN;

	return 0;
}

s32 ixgbe_get_wwn_prefix_generic(struct ixgbe_hw *hw, u16 *wwnn_prefix,
                                        u16 *wwpn_prefix)
{
	u16 offset, caps;
	u16 alt_san_mac_blk_offset;

	
	*wwnn_prefix = 0xFFFF;
	*wwpn_prefix = 0xFFFF;

	
	hw->eeprom.ops.read(hw, IXGBE_ALT_SAN_MAC_ADDR_BLK_PTR,
	                    &alt_san_mac_blk_offset);

	if ((alt_san_mac_blk_offset == 0) ||
	    (alt_san_mac_blk_offset == 0xFFFF))
		goto wwn_prefix_out;

	
	offset = alt_san_mac_blk_offset + IXGBE_ALT_SAN_MAC_ADDR_CAPS_OFFSET;
	hw->eeprom.ops.read(hw, offset, &caps);
	if (!(caps & IXGBE_ALT_SAN_MAC_ADDR_CAPS_ALTWWN))
		goto wwn_prefix_out;

	
	offset = alt_san_mac_blk_offset + IXGBE_ALT_SAN_MAC_ADDR_WWNN_OFFSET;
	hw->eeprom.ops.read(hw, offset, wwnn_prefix);

	offset = alt_san_mac_blk_offset + IXGBE_ALT_SAN_MAC_ADDR_WWPN_OFFSET;
	hw->eeprom.ops.read(hw, offset, wwpn_prefix);

wwn_prefix_out:
	return 0;
}

static s32 ixgbe_device_supports_autoneg_fc(struct ixgbe_hw *hw)
{

	switch (hw->device_id) {
	case IXGBE_DEV_ID_X540T:
		return 0;
	case IXGBE_DEV_ID_82599_T3_LOM:
		return 0;
	default:
		return IXGBE_ERR_FC_NOT_SUPPORTED;
	}
}

void ixgbe_set_mac_anti_spoofing(struct ixgbe_hw *hw, bool enable, int pf)
{
	int j;
	int pf_target_reg = pf >> 3;
	int pf_target_shift = pf % 8;
	u32 pfvfspoof = 0;

	if (hw->mac.type == ixgbe_mac_82598EB)
		return;

	if (enable)
		pfvfspoof = IXGBE_SPOOF_MACAS_MASK;

	for (j = 0; j < IXGBE_PFVFSPOOF_REG_COUNT; j++)
		IXGBE_WRITE_REG(hw, IXGBE_PFVFSPOOF(j), pfvfspoof);

	
	if (!enable)
		return;

	pfvfspoof = IXGBE_READ_REG(hw, IXGBE_PFVFSPOOF(pf_target_reg));
	pfvfspoof ^= (1 << pf_target_shift);
	IXGBE_WRITE_REG(hw, IXGBE_PFVFSPOOF(pf_target_reg), pfvfspoof);
}

void ixgbe_set_vlan_anti_spoofing(struct ixgbe_hw *hw, bool enable, int vf)
{
	int vf_target_reg = vf >> 3;
	int vf_target_shift = vf % 8 + IXGBE_SPOOF_VLANAS_SHIFT;
	u32 pfvfspoof;

	if (hw->mac.type == ixgbe_mac_82598EB)
		return;

	pfvfspoof = IXGBE_READ_REG(hw, IXGBE_PFVFSPOOF(vf_target_reg));
	if (enable)
		pfvfspoof |= (1 << vf_target_shift);
	else
		pfvfspoof &= ~(1 << vf_target_shift);
	IXGBE_WRITE_REG(hw, IXGBE_PFVFSPOOF(vf_target_reg), pfvfspoof);
}

s32 ixgbe_get_device_caps_generic(struct ixgbe_hw *hw, u16 *device_caps)
{
	hw->eeprom.ops.read(hw, IXGBE_DEVICE_CAPS, device_caps);

	return 0;
}

void ixgbe_set_rxpba_generic(struct ixgbe_hw *hw,
			     int num_pb,
			     u32 headroom,
			     int strategy)
{
	u32 pbsize = hw->mac.rx_pb_size;
	int i = 0;
	u32 rxpktsize, txpktsize, txpbthresh;

	
	pbsize -= headroom;

	if (!num_pb)
		num_pb = 1;

	switch (strategy) {
	case (PBA_STRATEGY_WEIGHTED):
		rxpktsize = ((pbsize * 5 * 2) / (num_pb * 8));
		pbsize -= rxpktsize * (num_pb / 2);
		rxpktsize <<= IXGBE_RXPBSIZE_SHIFT;
		for (; i < (num_pb / 2); i++)
			IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(i), rxpktsize);
		
	case (PBA_STRATEGY_EQUAL):
		
		rxpktsize = (pbsize / (num_pb - i)) << IXGBE_RXPBSIZE_SHIFT;
		for (; i < num_pb; i++)
			IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(i), rxpktsize);
		break;
	default:
		break;
	}

	txpktsize = IXGBE_TXPBSIZE_MAX / num_pb;
	txpbthresh = (txpktsize / 1024) - IXGBE_TXPKT_SIZE_MAX;
	for (i = 0; i < num_pb; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_TXPBSIZE(i), txpktsize);
		IXGBE_WRITE_REG(hw, IXGBE_TXPBTHRESH(i), txpbthresh);
	}

	
	for (; i < IXGBE_MAX_PB; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TXPBSIZE(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TXPBTHRESH(i), 0);
	}
}

static u8 ixgbe_calculate_checksum(u8 *buffer, u32 length)
{
	u32 i;
	u8 sum = 0;

	if (!buffer)
		return 0;

	for (i = 0; i < length; i++)
		sum += buffer[i];

	return (u8) (0 - sum);
}

static s32 ixgbe_host_interface_command(struct ixgbe_hw *hw, u32 *buffer,
					u32 length)
{
	u32 hicr, i, bi;
	u32 hdr_size = sizeof(struct ixgbe_hic_hdr);
	u8 buf_len, dword_len;

	s32 ret_val = 0;

	if (length == 0 || length & 0x3 ||
	    length > IXGBE_HI_MAX_BLOCK_BYTE_LENGTH) {
		hw_dbg(hw, "Buffer length failure.\n");
		ret_val = IXGBE_ERR_HOST_INTERFACE_COMMAND;
		goto out;
	}

	
	hicr = IXGBE_READ_REG(hw, IXGBE_HICR);
	if ((hicr & IXGBE_HICR_EN) == 0) {
		hw_dbg(hw, "IXGBE_HOST_EN bit disabled.\n");
		ret_val = IXGBE_ERR_HOST_INTERFACE_COMMAND;
		goto out;
	}

	
	dword_len = length >> 2;

	for (i = 0; i < dword_len; i++)
		IXGBE_WRITE_REG_ARRAY(hw, IXGBE_FLEX_MNG,
				      i, cpu_to_le32(buffer[i]));

	
	IXGBE_WRITE_REG(hw, IXGBE_HICR, hicr | IXGBE_HICR_C);

	for (i = 0; i < IXGBE_HI_COMMAND_TIMEOUT; i++) {
		hicr = IXGBE_READ_REG(hw, IXGBE_HICR);
		if (!(hicr & IXGBE_HICR_C))
			break;
		usleep_range(1000, 2000);
	}

	
	if (i == IXGBE_HI_COMMAND_TIMEOUT ||
	    (!(IXGBE_READ_REG(hw, IXGBE_HICR) & IXGBE_HICR_SV))) {
		hw_dbg(hw, "Command has failed with no status valid.\n");
		ret_val = IXGBE_ERR_HOST_INTERFACE_COMMAND;
		goto out;
	}

	
	dword_len = hdr_size >> 2;

	
	for (bi = 0; bi < dword_len; bi++) {
		buffer[bi] = IXGBE_READ_REG_ARRAY(hw, IXGBE_FLEX_MNG, bi);
		le32_to_cpus(&buffer[bi]);
	}

	
	buf_len = ((struct ixgbe_hic_hdr *)buffer)->buf_len;
	if (buf_len == 0)
		goto out;

	if (length < (buf_len + hdr_size)) {
		hw_dbg(hw, "Buffer not large enough for reply message.\n");
		ret_val = IXGBE_ERR_HOST_INTERFACE_COMMAND;
		goto out;
	}

	
	dword_len = (buf_len + 3) >> 2;

	
	for (; bi <= dword_len; bi++) {
		buffer[bi] = IXGBE_READ_REG_ARRAY(hw, IXGBE_FLEX_MNG, bi);
		le32_to_cpus(&buffer[bi]);
	}

out:
	return ret_val;
}

s32 ixgbe_set_fw_drv_ver_generic(struct ixgbe_hw *hw, u8 maj, u8 min,
				 u8 build, u8 sub)
{
	struct ixgbe_hic_drv_info fw_cmd;
	int i;
	s32 ret_val = 0;

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_SW_MNG_SM) != 0) {
		ret_val = IXGBE_ERR_SWFW_SYNC;
		goto out;
	}

	fw_cmd.hdr.cmd = FW_CEM_CMD_DRIVER_INFO;
	fw_cmd.hdr.buf_len = FW_CEM_CMD_DRIVER_INFO_LEN;
	fw_cmd.hdr.cmd_or_resp.cmd_resv = FW_CEM_CMD_RESERVED;
	fw_cmd.port_num = (u8)hw->bus.func;
	fw_cmd.ver_maj = maj;
	fw_cmd.ver_min = min;
	fw_cmd.ver_build = build;
	fw_cmd.ver_sub = sub;
	fw_cmd.hdr.checksum = 0;
	fw_cmd.hdr.checksum = ixgbe_calculate_checksum((u8 *)&fw_cmd,
				(FW_CEM_HDR_LEN + fw_cmd.hdr.buf_len));
	fw_cmd.pad = 0;
	fw_cmd.pad2 = 0;

	for (i = 0; i <= FW_CEM_MAX_RETRIES; i++) {
		ret_val = ixgbe_host_interface_command(hw, (u32 *)&fw_cmd,
						       sizeof(fw_cmd));
		if (ret_val != 0)
			continue;

		if (fw_cmd.hdr.cmd_or_resp.ret_status ==
		    FW_CEM_RESP_STATUS_SUCCESS)
			ret_val = 0;
		else
			ret_val = IXGBE_ERR_HOST_INTERFACE_COMMAND;

		break;
	}

	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_SW_MNG_SM);
out:
	return ret_val;
}

void ixgbe_clear_tx_pending(struct ixgbe_hw *hw)
{
	u32 gcr_ext, hlreg0;

	if (!(hw->mac.flags & IXGBE_FLAGS_DOUBLE_RESET_REQUIRED))
		return;

	hlreg0 = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg0 | IXGBE_HLREG0_LPBK);

	
	gcr_ext = IXGBE_READ_REG(hw, IXGBE_GCR_EXT);
	IXGBE_WRITE_REG(hw, IXGBE_GCR_EXT,
			gcr_ext | IXGBE_GCR_EXT_BUFFERS_CLEAR);

	
	IXGBE_WRITE_FLUSH(hw);
	udelay(20);

	
	IXGBE_WRITE_REG(hw, IXGBE_GCR_EXT, gcr_ext);
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg0);
}
