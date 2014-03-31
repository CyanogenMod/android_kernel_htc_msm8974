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

#include "e1000_mbx.h"

s32 igb_read_mbx(struct e1000_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	
	if (size > mbx->size)
		size = mbx->size;

	if (mbx->ops.read)
		ret_val = mbx->ops.read(hw, msg, size, mbx_id);

	return ret_val;
}

s32 igb_write_mbx(struct e1000_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = 0;

	if (size > mbx->size)
		ret_val = -E1000_ERR_MBX;

	else if (mbx->ops.write)
		ret_val = mbx->ops.write(hw, msg, size, mbx_id);

	return ret_val;
}

s32 igb_check_for_msg(struct e1000_hw *hw, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	if (mbx->ops.check_for_msg)
		ret_val = mbx->ops.check_for_msg(hw, mbx_id);

	return ret_val;
}

s32 igb_check_for_ack(struct e1000_hw *hw, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	if (mbx->ops.check_for_ack)
		ret_val = mbx->ops.check_for_ack(hw, mbx_id);

	return ret_val;
}

s32 igb_check_for_rst(struct e1000_hw *hw, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	if (mbx->ops.check_for_rst)
		ret_val = mbx->ops.check_for_rst(hw, mbx_id);

	return ret_val;
}

static s32 igb_poll_for_msg(struct e1000_hw *hw, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	if (!countdown || !mbx->ops.check_for_msg)
		goto out;

	while (countdown && mbx->ops.check_for_msg(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		udelay(mbx->usec_delay);
	}

	
	if (!countdown)
		mbx->timeout = 0;
out:
	return countdown ? 0 : -E1000_ERR_MBX;
}

static s32 igb_poll_for_ack(struct e1000_hw *hw, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	if (!countdown || !mbx->ops.check_for_ack)
		goto out;

	while (countdown && mbx->ops.check_for_ack(hw, mbx_id)) {
		countdown--;
		if (!countdown)
			break;
		udelay(mbx->usec_delay);
	}

	
	if (!countdown)
		mbx->timeout = 0;
out:
	return countdown ? 0 : -E1000_ERR_MBX;
}

static s32 igb_read_posted_mbx(struct e1000_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	if (!mbx->ops.read)
		goto out;

	ret_val = igb_poll_for_msg(hw, mbx_id);

	if (!ret_val)
		ret_val = mbx->ops.read(hw, msg, size, mbx_id);
out:
	return ret_val;
}

static s32 igb_write_posted_mbx(struct e1000_hw *hw, u32 *msg, u16 size, u16 mbx_id)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	
	if (!mbx->ops.write || !mbx->timeout)
		goto out;

	
	ret_val = mbx->ops.write(hw, msg, size, mbx_id);

	
	if (!ret_val)
		ret_val = igb_poll_for_ack(hw, mbx_id);
out:
	return ret_val;
}

static s32 igb_check_for_bit_pf(struct e1000_hw *hw, u32 mask)
{
	u32 mbvficr = rd32(E1000_MBVFICR);
	s32 ret_val = -E1000_ERR_MBX;

	if (mbvficr & mask) {
		ret_val = 0;
		wr32(E1000_MBVFICR, mask);
	}

	return ret_val;
}

static s32 igb_check_for_msg_pf(struct e1000_hw *hw, u16 vf_number)
{
	s32 ret_val = -E1000_ERR_MBX;

	if (!igb_check_for_bit_pf(hw, E1000_MBVFICR_VFREQ_VF1 << vf_number)) {
		ret_val = 0;
		hw->mbx.stats.reqs++;
	}

	return ret_val;
}

static s32 igb_check_for_ack_pf(struct e1000_hw *hw, u16 vf_number)
{
	s32 ret_val = -E1000_ERR_MBX;

	if (!igb_check_for_bit_pf(hw, E1000_MBVFICR_VFACK_VF1 << vf_number)) {
		ret_val = 0;
		hw->mbx.stats.acks++;
	}

	return ret_val;
}

static s32 igb_check_for_rst_pf(struct e1000_hw *hw, u16 vf_number)
{
	u32 vflre = rd32(E1000_VFLRE);
	s32 ret_val = -E1000_ERR_MBX;

	if (vflre & (1 << vf_number)) {
		ret_val = 0;
		wr32(E1000_VFLRE, (1 << vf_number));
		hw->mbx.stats.rsts++;
	}

	return ret_val;
}

static s32 igb_obtain_mbx_lock_pf(struct e1000_hw *hw, u16 vf_number)
{
	s32 ret_val = -E1000_ERR_MBX;
	u32 p2v_mailbox;


	
	wr32(E1000_P2VMAILBOX(vf_number), E1000_P2VMAILBOX_PFU);

	
	p2v_mailbox = rd32(E1000_P2VMAILBOX(vf_number));
	if (p2v_mailbox & E1000_P2VMAILBOX_PFU)
		ret_val = 0;

	return ret_val;
}

static s32 igb_write_mbx_pf(struct e1000_hw *hw, u32 *msg, u16 size,
                              u16 vf_number)
{
	s32 ret_val;
	u16 i;

	
	ret_val = igb_obtain_mbx_lock_pf(hw, vf_number);
	if (ret_val)
		goto out_no_write;

	
	igb_check_for_msg_pf(hw, vf_number);
	igb_check_for_ack_pf(hw, vf_number);

	
	for (i = 0; i < size; i++)
		array_wr32(E1000_VMBMEM(vf_number), i, msg[i]);

	
	wr32(E1000_P2VMAILBOX(vf_number), E1000_P2VMAILBOX_STS);

	
	hw->mbx.stats.msgs_tx++;

out_no_write:
	return ret_val;

}

static s32 igb_read_mbx_pf(struct e1000_hw *hw, u32 *msg, u16 size,
                             u16 vf_number)
{
	s32 ret_val;
	u16 i;

	
	ret_val = igb_obtain_mbx_lock_pf(hw, vf_number);
	if (ret_val)
		goto out_no_read;

	
	for (i = 0; i < size; i++)
		msg[i] = array_rd32(E1000_VMBMEM(vf_number), i);

	
	wr32(E1000_P2VMAILBOX(vf_number), E1000_P2VMAILBOX_ACK);

	
	hw->mbx.stats.msgs_rx++;

out_no_read:
	return ret_val;
}

s32 igb_init_mbx_params_pf(struct e1000_hw *hw)
{
	struct e1000_mbx_info *mbx = &hw->mbx;

	mbx->timeout = 0;
	mbx->usec_delay = 0;

	mbx->size = E1000_VFMAILBOX_SIZE;

	mbx->ops.read = igb_read_mbx_pf;
	mbx->ops.write = igb_write_mbx_pf;
	mbx->ops.read_posted = igb_read_posted_mbx;
	mbx->ops.write_posted = igb_write_posted_mbx;
	mbx->ops.check_for_msg = igb_check_for_msg_pf;
	mbx->ops.check_for_ack = igb_check_for_ack_pf;
	mbx->ops.check_for_rst = igb_check_for_rst_pf;

	mbx->stats.msgs_tx = 0;
	mbx->stats.msgs_rx = 0;
	mbx->stats.reqs = 0;
	mbx->stats.acks = 0;
	mbx->stats.rsts = 0;

	return 0;
}

