/*******************************************************************************

  Intel PRO/10GbE Linux driver
  Copyright(c) 1999 - 2008 Intel Corporation.

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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "ixgb_hw.h"
#include "ixgb_ee.h"
static u16 ixgb_shift_in_bits(struct ixgb_hw *hw);

static void ixgb_shift_out_bits(struct ixgb_hw *hw,
				u16 data,
				u16 count);
static void ixgb_standby_eeprom(struct ixgb_hw *hw);

static bool ixgb_wait_eeprom_command(struct ixgb_hw *hw);

static void ixgb_cleanup_eeprom(struct ixgb_hw *hw);

static void
ixgb_raise_clock(struct ixgb_hw *hw,
		  u32 *eecd_reg)
{
	*eecd_reg = *eecd_reg | IXGB_EECD_SK;
	IXGB_WRITE_REG(hw, EECD, *eecd_reg);
	IXGB_WRITE_FLUSH(hw);
	udelay(50);
}

static void
ixgb_lower_clock(struct ixgb_hw *hw,
		  u32 *eecd_reg)
{
	*eecd_reg = *eecd_reg & ~IXGB_EECD_SK;
	IXGB_WRITE_REG(hw, EECD, *eecd_reg);
	IXGB_WRITE_FLUSH(hw);
	udelay(50);
}

static void
ixgb_shift_out_bits(struct ixgb_hw *hw,
					 u16 data,
					 u16 count)
{
	u32 eecd_reg;
	u32 mask;

	mask = 0x01 << (count - 1);
	eecd_reg = IXGB_READ_REG(hw, EECD);
	eecd_reg &= ~(IXGB_EECD_DO | IXGB_EECD_DI);
	do {
		eecd_reg &= ~IXGB_EECD_DI;

		if (data & mask)
			eecd_reg |= IXGB_EECD_DI;

		IXGB_WRITE_REG(hw, EECD, eecd_reg);
		IXGB_WRITE_FLUSH(hw);

		udelay(50);

		ixgb_raise_clock(hw, &eecd_reg);
		ixgb_lower_clock(hw, &eecd_reg);

		mask = mask >> 1;

	} while (mask);

	
	eecd_reg &= ~IXGB_EECD_DI;
	IXGB_WRITE_REG(hw, EECD, eecd_reg);
}

static u16
ixgb_shift_in_bits(struct ixgb_hw *hw)
{
	u32 eecd_reg;
	u32 i;
	u16 data;


	eecd_reg = IXGB_READ_REG(hw, EECD);

	eecd_reg &= ~(IXGB_EECD_DO | IXGB_EECD_DI);
	data = 0;

	for (i = 0; i < 16; i++) {
		data = data << 1;
		ixgb_raise_clock(hw, &eecd_reg);

		eecd_reg = IXGB_READ_REG(hw, EECD);

		eecd_reg &= ~(IXGB_EECD_DI);
		if (eecd_reg & IXGB_EECD_DO)
			data |= 1;

		ixgb_lower_clock(hw, &eecd_reg);
	}

	return data;
}

static void
ixgb_setup_eeprom(struct ixgb_hw *hw)
{
	u32 eecd_reg;

	eecd_reg = IXGB_READ_REG(hw, EECD);

	
	eecd_reg &= ~(IXGB_EECD_SK | IXGB_EECD_DI);
	IXGB_WRITE_REG(hw, EECD, eecd_reg);

	
	eecd_reg |= IXGB_EECD_CS;
	IXGB_WRITE_REG(hw, EECD, eecd_reg);
}

static void
ixgb_standby_eeprom(struct ixgb_hw *hw)
{
	u32 eecd_reg;

	eecd_reg = IXGB_READ_REG(hw, EECD);

	
	eecd_reg &= ~(IXGB_EECD_CS | IXGB_EECD_SK);
	IXGB_WRITE_REG(hw, EECD, eecd_reg);
	IXGB_WRITE_FLUSH(hw);
	udelay(50);

	
	eecd_reg |= IXGB_EECD_SK;
	IXGB_WRITE_REG(hw, EECD, eecd_reg);
	IXGB_WRITE_FLUSH(hw);
	udelay(50);

	
	eecd_reg |= IXGB_EECD_CS;
	IXGB_WRITE_REG(hw, EECD, eecd_reg);
	IXGB_WRITE_FLUSH(hw);
	udelay(50);

	
	eecd_reg &= ~IXGB_EECD_SK;
	IXGB_WRITE_REG(hw, EECD, eecd_reg);
	IXGB_WRITE_FLUSH(hw);
	udelay(50);
}

static void
ixgb_clock_eeprom(struct ixgb_hw *hw)
{
	u32 eecd_reg;

	eecd_reg = IXGB_READ_REG(hw, EECD);

	
	eecd_reg |= IXGB_EECD_SK;
	IXGB_WRITE_REG(hw, EECD, eecd_reg);
	IXGB_WRITE_FLUSH(hw);
	udelay(50);

	
	eecd_reg &= ~IXGB_EECD_SK;
	IXGB_WRITE_REG(hw, EECD, eecd_reg);
	IXGB_WRITE_FLUSH(hw);
	udelay(50);
}

static void
ixgb_cleanup_eeprom(struct ixgb_hw *hw)
{
	u32 eecd_reg;

	eecd_reg = IXGB_READ_REG(hw, EECD);

	eecd_reg &= ~(IXGB_EECD_CS | IXGB_EECD_DI);

	IXGB_WRITE_REG(hw, EECD, eecd_reg);

	ixgb_clock_eeprom(hw);
}

static bool
ixgb_wait_eeprom_command(struct ixgb_hw *hw)
{
	u32 eecd_reg;
	u32 i;

	ixgb_standby_eeprom(hw);

	for (i = 0; i < 200; i++) {
		eecd_reg = IXGB_READ_REG(hw, EECD);

		if (eecd_reg & IXGB_EECD_DO)
			return true;

		udelay(50);
	}
	ASSERT(0);
	return false;
}

bool
ixgb_validate_eeprom_checksum(struct ixgb_hw *hw)
{
	u16 checksum = 0;
	u16 i;

	for (i = 0; i < (EEPROM_CHECKSUM_REG + 1); i++)
		checksum += ixgb_read_eeprom(hw, i);

	if (checksum == (u16) EEPROM_SUM)
		return true;
	else
		return false;
}

void
ixgb_update_eeprom_checksum(struct ixgb_hw *hw)
{
	u16 checksum = 0;
	u16 i;

	for (i = 0; i < EEPROM_CHECKSUM_REG; i++)
		checksum += ixgb_read_eeprom(hw, i);

	checksum = (u16) EEPROM_SUM - checksum;

	ixgb_write_eeprom(hw, EEPROM_CHECKSUM_REG, checksum);
}

/******************************************************************************
 * Writes a 16 bit word to a given offset in the EEPROM.
 *
 * hw - Struct containing variables accessed by shared code
 * reg - offset within the EEPROM to be written to
 * data - 16 bit word to be written to the EEPROM
 *
 * If ixgb_update_eeprom_checksum is not called after this function, the
 * EEPROM will most likely contain an invalid checksum.
 *
 *****************************************************************************/
void
ixgb_write_eeprom(struct ixgb_hw *hw, u16 offset, u16 data)
{
	struct ixgb_ee_map_type *ee_map = (struct ixgb_ee_map_type *)hw->eeprom;

	
	ixgb_setup_eeprom(hw);

	ixgb_shift_out_bits(hw, EEPROM_EWEN_OPCODE, 5);
	ixgb_shift_out_bits(hw, 0, 4);

	
	ixgb_standby_eeprom(hw);

	
	ixgb_shift_out_bits(hw, EEPROM_WRITE_OPCODE, 3);
	ixgb_shift_out_bits(hw, offset, 6);

	
	ixgb_shift_out_bits(hw, data, 16);

	ixgb_wait_eeprom_command(hw);

	
	ixgb_standby_eeprom(hw);

	ixgb_shift_out_bits(hw, EEPROM_EWDS_OPCODE, 5);
	ixgb_shift_out_bits(hw, 0, 4);

	
	ixgb_cleanup_eeprom(hw);

	
	ee_map->init_ctrl_reg_1 = cpu_to_le16(EEPROM_ICW1_SIGNATURE_CLEAR);
}

u16
ixgb_read_eeprom(struct ixgb_hw *hw,
		  u16 offset)
{
	u16 data;

	
	ixgb_setup_eeprom(hw);

	
	ixgb_shift_out_bits(hw, EEPROM_READ_OPCODE, 3);
	ixgb_shift_out_bits(hw, offset, 6);

	
	data = ixgb_shift_in_bits(hw);

	
	ixgb_standby_eeprom(hw);

	return data;
}

bool
ixgb_get_eeprom_data(struct ixgb_hw *hw)
{
	u16 i;
	u16 checksum = 0;
	struct ixgb_ee_map_type *ee_map;

	ENTER();

	ee_map = (struct ixgb_ee_map_type *)hw->eeprom;

	pr_debug("Reading eeprom data\n");
	for (i = 0; i < IXGB_EEPROM_SIZE ; i++) {
		u16 ee_data;
		ee_data = ixgb_read_eeprom(hw, i);
		checksum += ee_data;
		hw->eeprom[i] = cpu_to_le16(ee_data);
	}

	if (checksum != (u16) EEPROM_SUM) {
		pr_debug("Checksum invalid\n");
		ee_map->init_ctrl_reg_1 = cpu_to_le16(EEPROM_ICW1_SIGNATURE_CLEAR);
		return false;
	}

	if ((ee_map->init_ctrl_reg_1 & cpu_to_le16(EEPROM_ICW1_SIGNATURE_MASK))
		 != cpu_to_le16(EEPROM_ICW1_SIGNATURE_VALID)) {
		pr_debug("Signature invalid\n");
		return false;
	}

	return true;
}

static bool
ixgb_check_and_get_eeprom_data (struct ixgb_hw* hw)
{
	struct ixgb_ee_map_type *ee_map = (struct ixgb_ee_map_type *)hw->eeprom;

	if ((ee_map->init_ctrl_reg_1 & cpu_to_le16(EEPROM_ICW1_SIGNATURE_MASK))
	    == cpu_to_le16(EEPROM_ICW1_SIGNATURE_VALID)) {
		return true;
	} else {
		return ixgb_get_eeprom_data(hw);
	}
}

__le16
ixgb_get_eeprom_word(struct ixgb_hw *hw, u16 index)
{

	if (index < IXGB_EEPROM_SIZE && ixgb_check_and_get_eeprom_data(hw))
		return hw->eeprom[index];

	return 0;
}

void
ixgb_get_ee_mac_addr(struct ixgb_hw *hw,
			u8 *mac_addr)
{
	int i;
	struct ixgb_ee_map_type *ee_map = (struct ixgb_ee_map_type *)hw->eeprom;

	ENTER();

	if (ixgb_check_and_get_eeprom_data(hw)) {
		for (i = 0; i < ETH_ALEN; i++) {
			mac_addr[i] = ee_map->mac_addr[i];
		}
		pr_debug("eeprom mac address = %pM\n", mac_addr);
	}
}


u32
ixgb_get_ee_pba_number(struct ixgb_hw *hw)
{
	if (ixgb_check_and_get_eeprom_data(hw))
		return le16_to_cpu(hw->eeprom[EEPROM_PBA_1_2_REG])
			| (le16_to_cpu(hw->eeprom[EEPROM_PBA_3_4_REG])<<16);

	return 0;
}


u16
ixgb_get_ee_device_id(struct ixgb_hw *hw)
{
	struct ixgb_ee_map_type *ee_map = (struct ixgb_ee_map_type *)hw->eeprom;

	if (ixgb_check_and_get_eeprom_data(hw))
		return le16_to_cpu(ee_map->device_id);

	return 0;
}

