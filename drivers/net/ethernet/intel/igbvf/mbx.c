/*******************************************************************************

  Intel(R) 82576 Virtual Function Linux driver
  Copyright(c) 2009 - 2012 Intel Corporation.

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

#include "mbx.h"

static s32 e1000_poll_for_msg(struct e1000_hw *hw)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	if (!mbx->ops.check_for_msg)
		goto out;

	while (countdown && mbx->ops.check_for_msg(hw)) {
		countdown--;
		udelay(mbx->usec_delay);
	}

	
	if (!countdown)
		mbx->timeout = 0;
out:
	return countdown ? E1000_SUCCESS : -E1000_ERR_MBX;
}

static s32 e1000_poll_for_ack(struct e1000_hw *hw)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	if (!mbx->ops.check_for_ack)
		goto out;

	while (countdown && mbx->ops.check_for_ack(hw)) {
		countdown--;
		udelay(mbx->usec_delay);
	}

	
	if (!countdown)
		mbx->timeout = 0;
out:
	return countdown ? E1000_SUCCESS : -E1000_ERR_MBX;
}

static s32 e1000_read_posted_mbx(struct e1000_hw *hw, u32 *msg, u16 size)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	if (!mbx->ops.read)
		goto out;

	ret_val = e1000_poll_for_msg(hw);

	
	if (!ret_val)
		ret_val = mbx->ops.read(hw, msg, size);
out:
	return ret_val;
}

static s32 e1000_write_posted_mbx(struct e1000_hw *hw, u32 *msg, u16 size)
{
	struct e1000_mbx_info *mbx = &hw->mbx;
	s32 ret_val = -E1000_ERR_MBX;

	
	if (!mbx->ops.write || !mbx->timeout)
		goto out;

	
	ret_val = mbx->ops.write(hw, msg, size);

	
	if (!ret_val)
		ret_val = e1000_poll_for_ack(hw);
out:
	return ret_val;
}

static u32 e1000_read_v2p_mailbox(struct e1000_hw *hw)
{
	u32 v2p_mailbox = er32(V2PMAILBOX(0));

	v2p_mailbox |= hw->dev_spec.vf.v2p_mailbox;
	hw->dev_spec.vf.v2p_mailbox |= v2p_mailbox & E1000_V2PMAILBOX_R2C_BITS;

	return v2p_mailbox;
}

static s32 e1000_check_for_bit_vf(struct e1000_hw *hw, u32 mask)
{
	u32 v2p_mailbox = e1000_read_v2p_mailbox(hw);
	s32 ret_val = -E1000_ERR_MBX;

	if (v2p_mailbox & mask)
		ret_val = E1000_SUCCESS;

	hw->dev_spec.vf.v2p_mailbox &= ~mask;

	return ret_val;
}

static s32 e1000_check_for_msg_vf(struct e1000_hw *hw)
{
	s32 ret_val = -E1000_ERR_MBX;

	if (!e1000_check_for_bit_vf(hw, E1000_V2PMAILBOX_PFSTS)) {
		ret_val = E1000_SUCCESS;
		hw->mbx.stats.reqs++;
	}

	return ret_val;
}

static s32 e1000_check_for_ack_vf(struct e1000_hw *hw)
{
	s32 ret_val = -E1000_ERR_MBX;

	if (!e1000_check_for_bit_vf(hw, E1000_V2PMAILBOX_PFACK)) {
		ret_val = E1000_SUCCESS;
		hw->mbx.stats.acks++;
	}

	return ret_val;
}

static s32 e1000_check_for_rst_vf(struct e1000_hw *hw)
{
	s32 ret_val = -E1000_ERR_MBX;

	if (!e1000_check_for_bit_vf(hw, (E1000_V2PMAILBOX_RSTD |
	                                 E1000_V2PMAILBOX_RSTI))) {
		ret_val = E1000_SUCCESS;
		hw->mbx.stats.rsts++;
	}

	return ret_val;
}

static s32 e1000_obtain_mbx_lock_vf(struct e1000_hw *hw)
{
	s32 ret_val = -E1000_ERR_MBX;

	
	ew32(V2PMAILBOX(0), E1000_V2PMAILBOX_VFU);

	
	if (e1000_read_v2p_mailbox(hw) & E1000_V2PMAILBOX_VFU)
		ret_val = E1000_SUCCESS;

	return ret_val;
}

static s32 e1000_write_mbx_vf(struct e1000_hw *hw, u32 *msg, u16 size)
{
	s32 err;
	u16 i;

	
	err = e1000_obtain_mbx_lock_vf(hw);
	if (err)
		goto out_no_write;

	
	e1000_check_for_ack_vf(hw);
	e1000_check_for_msg_vf(hw);

	
	for (i = 0; i < size; i++)
		array_ew32(VMBMEM(0), i, msg[i]);

	
	hw->mbx.stats.msgs_tx++;

	
	ew32(V2PMAILBOX(0), E1000_V2PMAILBOX_REQ);

out_no_write:
	return err;
}

static s32 e1000_read_mbx_vf(struct e1000_hw *hw, u32 *msg, u16 size)
{
	s32 err;
	u16 i;

	
	err = e1000_obtain_mbx_lock_vf(hw);
	if (err)
		goto out_no_read;

	
	for (i = 0; i < size; i++)
		msg[i] = array_er32(VMBMEM(0), i);

	
	ew32(V2PMAILBOX(0), E1000_V2PMAILBOX_ACK);

	
	hw->mbx.stats.msgs_rx++;

out_no_read:
	return err;
}

s32 e1000_init_mbx_params_vf(struct e1000_hw *hw)
{
	struct e1000_mbx_info *mbx = &hw->mbx;

	mbx->timeout = 0;
	mbx->usec_delay = E1000_VF_MBX_INIT_DELAY;

	mbx->size = E1000_VFMAILBOX_SIZE;

	mbx->ops.read = e1000_read_mbx_vf;
	mbx->ops.write = e1000_write_mbx_vf;
	mbx->ops.read_posted = e1000_read_posted_mbx;
	mbx->ops.write_posted = e1000_write_posted_mbx;
	mbx->ops.check_for_msg = e1000_check_for_msg_vf;
	mbx->ops.check_for_ack = e1000_check_for_ack_vf;
	mbx->ops.check_for_rst = e1000_check_for_rst_vf;

	mbx->stats.msgs_tx = 0;
	mbx->stats.msgs_rx = 0;
	mbx->stats.reqs = 0;
	mbx->stats.acks = 0;
	mbx->stats.rsts = 0;

	return E1000_SUCCESS;
}

