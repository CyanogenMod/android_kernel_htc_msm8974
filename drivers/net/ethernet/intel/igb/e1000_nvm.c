/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2012 Intel Corporation.

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

#include <linux/if_ether.h>
#include <linux/delay.h>

#include "e1000_mac.h"
#include "e1000_nvm.h"

static void igb_raise_eec_clk(struct e1000_hw *hw, u32 *eecd)
{
	*eecd = *eecd | E1000_EECD_SK;
	wr32(E1000_EECD, *eecd);
	wrfl();
	udelay(hw->nvm.delay_usec);
}

static void igb_lower_eec_clk(struct e1000_hw *hw, u32 *eecd)
{
	*eecd = *eecd & ~E1000_EECD_SK;
	wr32(E1000_EECD, *eecd);
	wrfl();
	udelay(hw->nvm.delay_usec);
}

static void igb_shift_out_eec_bits(struct e1000_hw *hw, u16 data, u16 count)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = rd32(E1000_EECD);
	u32 mask;

	mask = 0x01 << (count - 1);
	if (nvm->type == e1000_nvm_eeprom_spi)
		eecd |= E1000_EECD_DO;

	do {
		eecd &= ~E1000_EECD_DI;

		if (data & mask)
			eecd |= E1000_EECD_DI;

		wr32(E1000_EECD, eecd);
		wrfl();

		udelay(nvm->delay_usec);

		igb_raise_eec_clk(hw, &eecd);
		igb_lower_eec_clk(hw, &eecd);

		mask >>= 1;
	} while (mask);

	eecd &= ~E1000_EECD_DI;
	wr32(E1000_EECD, eecd);
}

static u16 igb_shift_in_eec_bits(struct e1000_hw *hw, u16 count)
{
	u32 eecd;
	u32 i;
	u16 data;

	eecd = rd32(E1000_EECD);

	eecd &= ~(E1000_EECD_DO | E1000_EECD_DI);
	data = 0;

	for (i = 0; i < count; i++) {
		data <<= 1;
		igb_raise_eec_clk(hw, &eecd);

		eecd = rd32(E1000_EECD);

		eecd &= ~E1000_EECD_DI;
		if (eecd & E1000_EECD_DO)
			data |= 1;

		igb_lower_eec_clk(hw, &eecd);
	}

	return data;
}

static s32 igb_poll_eerd_eewr_done(struct e1000_hw *hw, int ee_reg)
{
	u32 attempts = 100000;
	u32 i, reg = 0;
	s32 ret_val = -E1000_ERR_NVM;

	for (i = 0; i < attempts; i++) {
		if (ee_reg == E1000_NVM_POLL_READ)
			reg = rd32(E1000_EERD);
		else
			reg = rd32(E1000_EEWR);

		if (reg & E1000_NVM_RW_REG_DONE) {
			ret_val = 0;
			break;
		}

		udelay(5);
	}

	return ret_val;
}

s32 igb_acquire_nvm(struct e1000_hw *hw)
{
	u32 eecd = rd32(E1000_EECD);
	s32 timeout = E1000_NVM_GRANT_ATTEMPTS;
	s32 ret_val = 0;


	wr32(E1000_EECD, eecd | E1000_EECD_REQ);
	eecd = rd32(E1000_EECD);

	while (timeout) {
		if (eecd & E1000_EECD_GNT)
			break;
		udelay(5);
		eecd = rd32(E1000_EECD);
		timeout--;
	}

	if (!timeout) {
		eecd &= ~E1000_EECD_REQ;
		wr32(E1000_EECD, eecd);
		hw_dbg("Could not acquire NVM grant\n");
		ret_val = -E1000_ERR_NVM;
	}

	return ret_val;
}

static void igb_standby_nvm(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = rd32(E1000_EECD);

	if (nvm->type == e1000_nvm_eeprom_spi) {
		
		eecd |= E1000_EECD_CS;
		wr32(E1000_EECD, eecd);
		wrfl();
		udelay(nvm->delay_usec);
		eecd &= ~E1000_EECD_CS;
		wr32(E1000_EECD, eecd);
		wrfl();
		udelay(nvm->delay_usec);
	}
}

static void e1000_stop_nvm(struct e1000_hw *hw)
{
	u32 eecd;

	eecd = rd32(E1000_EECD);
	if (hw->nvm.type == e1000_nvm_eeprom_spi) {
		
		eecd |= E1000_EECD_CS;
		igb_lower_eec_clk(hw, &eecd);
	}
}

void igb_release_nvm(struct e1000_hw *hw)
{
	u32 eecd;

	e1000_stop_nvm(hw);

	eecd = rd32(E1000_EECD);
	eecd &= ~E1000_EECD_REQ;
	wr32(E1000_EECD, eecd);
}

static s32 igb_ready_nvm_eeprom(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = rd32(E1000_EECD);
	s32 ret_val = 0;
	u16 timeout = 0;
	u8 spi_stat_reg;


	if (nvm->type == e1000_nvm_eeprom_spi) {
		
		eecd &= ~(E1000_EECD_CS | E1000_EECD_SK);
		wr32(E1000_EECD, eecd);
		wrfl();
		udelay(1);
		timeout = NVM_MAX_RETRY_SPI;

		while (timeout) {
			igb_shift_out_eec_bits(hw, NVM_RDSR_OPCODE_SPI,
						 hw->nvm.opcode_bits);
			spi_stat_reg = (u8)igb_shift_in_eec_bits(hw, 8);
			if (!(spi_stat_reg & NVM_STATUS_RDY_SPI))
				break;

			udelay(5);
			igb_standby_nvm(hw);
			timeout--;
		}

		if (!timeout) {
			hw_dbg("SPI NVM Status error\n");
			ret_val = -E1000_ERR_NVM;
			goto out;
		}
	}

out:
	return ret_val;
}

s32 igb_read_nvm_spi(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 i = 0;
	s32 ret_val;
	u16 word_in;
	u8 read_opcode = NVM_READ_OPCODE_SPI;

	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		hw_dbg("nvm parameter(s) out of bounds\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	ret_val = nvm->ops.acquire(hw);
	if (ret_val)
		goto out;

	ret_val = igb_ready_nvm_eeprom(hw);
	if (ret_val)
		goto release;

	igb_standby_nvm(hw);

	if ((nvm->address_bits == 8) && (offset >= 128))
		read_opcode |= NVM_A8_OPCODE_SPI;

	
	igb_shift_out_eec_bits(hw, read_opcode, nvm->opcode_bits);
	igb_shift_out_eec_bits(hw, (u16)(offset*2), nvm->address_bits);

	for (i = 0; i < words; i++) {
		word_in = igb_shift_in_eec_bits(hw, 16);
		data[i] = (word_in >> 8) | (word_in << 8);
	}

release:
	nvm->ops.release(hw);

out:
	return ret_val;
}

s32 igb_read_nvm_eerd(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 i, eerd = 0;
	s32 ret_val = 0;

	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		hw_dbg("nvm parameter(s) out of bounds\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	for (i = 0; i < words; i++) {
		eerd = ((offset+i) << E1000_NVM_RW_ADDR_SHIFT) +
		       E1000_NVM_RW_REG_START;

		wr32(E1000_EERD, eerd);
		ret_val = igb_poll_eerd_eewr_done(hw, E1000_NVM_POLL_READ);
		if (ret_val)
			break;

		data[i] = (rd32(E1000_EERD) >>
			E1000_NVM_RW_REG_DATA);
	}

out:
	return ret_val;
}

/**
 *  igb_write_nvm_spi - Write to EEPROM using SPI
 *  @hw: pointer to the HW structure
 *  @offset: offset within the EEPROM to be written to
 *  @words: number of words to write
 *  @data: 16 bit word(s) to be written to the EEPROM
 *
 *  Writes data to EEPROM at offset using SPI interface.
 *
 *  If e1000_update_nvm_checksum is not called after this function , the
 *  EEPROM will most likley contain an invalid checksum.
 **/
s32 igb_write_nvm_spi(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	s32 ret_val;
	u16 widx = 0;

	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		hw_dbg("nvm parameter(s) out of bounds\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	ret_val = hw->nvm.ops.acquire(hw);
	if (ret_val)
		goto out;

	msleep(10);

	while (widx < words) {
		u8 write_opcode = NVM_WRITE_OPCODE_SPI;

		ret_val = igb_ready_nvm_eeprom(hw);
		if (ret_val)
			goto release;

		igb_standby_nvm(hw);

		
		igb_shift_out_eec_bits(hw, NVM_WREN_OPCODE_SPI,
					 nvm->opcode_bits);

		igb_standby_nvm(hw);

		if ((nvm->address_bits == 8) && (offset >= 128))
			write_opcode |= NVM_A8_OPCODE_SPI;

		
		igb_shift_out_eec_bits(hw, write_opcode, nvm->opcode_bits);
		igb_shift_out_eec_bits(hw, (u16)((offset + widx) * 2),
					 nvm->address_bits);

		
		while (widx < words) {
			u16 word_out = data[widx];
			word_out = (word_out >> 8) | (word_out << 8);
			igb_shift_out_eec_bits(hw, word_out, 16);
			widx++;

			if ((((offset + widx) * 2) % nvm->page_size) == 0) {
				igb_standby_nvm(hw);
				break;
			}
		}
	}

	msleep(10);
release:
	hw->nvm.ops.release(hw);

out:
	return ret_val;
}

s32 igb_read_part_string(struct e1000_hw *hw, u8 *part_num, u32 part_num_size)
{
	s32 ret_val;
	u16 nvm_data;
	u16 pointer;
	u16 offset;
	u16 length;

	if (part_num == NULL) {
		hw_dbg("PBA string buffer was null\n");
		ret_val = E1000_ERR_INVALID_ARGUMENT;
		goto out;
	}

	ret_val = hw->nvm.ops.read(hw, NVM_PBA_OFFSET_0, 1, &nvm_data);
	if (ret_val) {
		hw_dbg("NVM Read Error\n");
		goto out;
	}

	ret_val = hw->nvm.ops.read(hw, NVM_PBA_OFFSET_1, 1, &pointer);
	if (ret_val) {
		hw_dbg("NVM Read Error\n");
		goto out;
	}

	if (nvm_data != NVM_PBA_PTR_GUARD) {
		hw_dbg("NVM PBA number is not stored as string\n");

		
		if (part_num_size < 11) {
			hw_dbg("PBA string buffer too small\n");
			return E1000_ERR_NO_SPACE;
		}

		
		part_num[0] = (nvm_data >> 12) & 0xF;
		part_num[1] = (nvm_data >> 8) & 0xF;
		part_num[2] = (nvm_data >> 4) & 0xF;
		part_num[3] = nvm_data & 0xF;
		part_num[4] = (pointer >> 12) & 0xF;
		part_num[5] = (pointer >> 8) & 0xF;
		part_num[6] = '-';
		part_num[7] = 0;
		part_num[8] = (pointer >> 4) & 0xF;
		part_num[9] = pointer & 0xF;

		
		part_num[10] = '\0';

		
		for (offset = 0; offset < 10; offset++) {
			if (part_num[offset] < 0xA)
				part_num[offset] += '0';
			else if (part_num[offset] < 0x10)
				part_num[offset] += 'A' - 0xA;
		}

		goto out;
	}

	ret_val = hw->nvm.ops.read(hw, pointer, 1, &length);
	if (ret_val) {
		hw_dbg("NVM Read Error\n");
		goto out;
	}

	if (length == 0xFFFF || length == 0) {
		hw_dbg("NVM PBA number section invalid length\n");
		ret_val = E1000_ERR_NVM_PBA_SECTION;
		goto out;
	}
	
	if (part_num_size < (((u32)length * 2) - 1)) {
		hw_dbg("PBA string buffer too small\n");
		ret_val = E1000_ERR_NO_SPACE;
		goto out;
	}

	
	pointer++;
	length--;

	for (offset = 0; offset < length; offset++) {
		ret_val = hw->nvm.ops.read(hw, pointer + offset, 1, &nvm_data);
		if (ret_val) {
			hw_dbg("NVM Read Error\n");
			goto out;
		}
		part_num[offset * 2] = (u8)(nvm_data >> 8);
		part_num[(offset * 2) + 1] = (u8)(nvm_data & 0xFF);
	}
	part_num[offset * 2] = '\0';

out:
	return ret_val;
}

s32 igb_read_mac_addr(struct e1000_hw *hw)
{
	u32 rar_high;
	u32 rar_low;
	u16 i;

	rar_high = rd32(E1000_RAH(0));
	rar_low = rd32(E1000_RAL(0));

	for (i = 0; i < E1000_RAL_MAC_ADDR_LEN; i++)
		hw->mac.perm_addr[i] = (u8)(rar_low >> (i*8));

	for (i = 0; i < E1000_RAH_MAC_ADDR_LEN; i++)
		hw->mac.perm_addr[i+4] = (u8)(rar_high >> (i*8));

	for (i = 0; i < ETH_ALEN; i++)
		hw->mac.addr[i] = hw->mac.perm_addr[i];

	return 0;
}

s32 igb_validate_nvm_checksum(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u16 checksum = 0;
	u16 i, nvm_data;

	for (i = 0; i < (NVM_CHECKSUM_REG + 1); i++) {
		ret_val = hw->nvm.ops.read(hw, i, 1, &nvm_data);
		if (ret_val) {
			hw_dbg("NVM Read Error\n");
			goto out;
		}
		checksum += nvm_data;
	}

	if (checksum != (u16) NVM_SUM) {
		hw_dbg("NVM Checksum Invalid\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

out:
	return ret_val;
}

s32 igb_update_nvm_checksum(struct e1000_hw *hw)
{
	s32  ret_val;
	u16 checksum = 0;
	u16 i, nvm_data;

	for (i = 0; i < NVM_CHECKSUM_REG; i++) {
		ret_val = hw->nvm.ops.read(hw, i, 1, &nvm_data);
		if (ret_val) {
			hw_dbg("NVM Read Error while updating checksum.\n");
			goto out;
		}
		checksum += nvm_data;
	}
	checksum = (u16) NVM_SUM - checksum;
	ret_val = hw->nvm.ops.write(hw, NVM_CHECKSUM_REG, 1, &checksum);
	if (ret_val)
		hw_dbg("NVM Write Error while updating checksum.\n");

out:
	return ret_val;
}

