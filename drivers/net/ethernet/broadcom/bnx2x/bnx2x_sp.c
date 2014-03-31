/* bnx2x_sp.c: Broadcom Everest network driver.
 *
 * Copyright (c) 2011-2012 Broadcom Corporation
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 * Maintained by: Eilon Greenstein <eilong@broadcom.com>
 * Written by: Vladislav Zolotarov
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/crc32.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/crc32c.h>
#include "bnx2x.h"
#include "bnx2x_cmn.h"
#include "bnx2x_sp.h"

#define BNX2X_MAX_EMUL_MULTI		16

#define MAC_LEADING_ZERO_CNT (ALIGN(ETH_ALEN, sizeof(u32)) - ETH_ALEN)


static inline void bnx2x_exe_queue_init(struct bnx2x *bp,
					struct bnx2x_exe_queue_obj *o,
					int exe_len,
					union bnx2x_qable_obj *owner,
					exe_q_validate validate,
					exe_q_remove remove,
					exe_q_optimize optimize,
					exe_q_execute exec,
					exe_q_get get)
{
	memset(o, 0, sizeof(*o));

	INIT_LIST_HEAD(&o->exe_queue);
	INIT_LIST_HEAD(&o->pending_comp);

	spin_lock_init(&o->lock);

	o->exe_chunk_len = exe_len;
	o->owner         = owner;

	
	o->validate      = validate;
	o->remove        = remove;
	o->optimize      = optimize;
	o->execute       = exec;
	o->get           = get;

	DP(BNX2X_MSG_SP, "Setup the execution queue with the chunk length of %d\n",
	   exe_len);
}

static inline void bnx2x_exe_queue_free_elem(struct bnx2x *bp,
					     struct bnx2x_exeq_elem *elem)
{
	DP(BNX2X_MSG_SP, "Deleting an exe_queue element\n");
	kfree(elem);
}

static inline int bnx2x_exe_queue_length(struct bnx2x_exe_queue_obj *o)
{
	struct bnx2x_exeq_elem *elem;
	int cnt = 0;

	spin_lock_bh(&o->lock);

	list_for_each_entry(elem, &o->exe_queue, link)
		cnt++;

	spin_unlock_bh(&o->lock);

	return cnt;
}

static inline int bnx2x_exe_queue_add(struct bnx2x *bp,
				      struct bnx2x_exe_queue_obj *o,
				      struct bnx2x_exeq_elem *elem,
				      bool restore)
{
	int rc;

	spin_lock_bh(&o->lock);

	if (!restore) {
		
		rc = o->optimize(bp, o->owner, elem);
		if (rc)
			goto free_and_exit;

		
		rc = o->validate(bp, o->owner, elem);
		if (rc) {
			BNX2X_ERR("Preamble failed: %d\n", rc);
			goto free_and_exit;
		}
	}

	
	list_add_tail(&elem->link, &o->exe_queue);

	spin_unlock_bh(&o->lock);

	return 0;

free_and_exit:
	bnx2x_exe_queue_free_elem(bp, elem);

	spin_unlock_bh(&o->lock);

	return rc;

}

static inline void __bnx2x_exe_queue_reset_pending(
	struct bnx2x *bp,
	struct bnx2x_exe_queue_obj *o)
{
	struct bnx2x_exeq_elem *elem;

	while (!list_empty(&o->pending_comp)) {
		elem = list_first_entry(&o->pending_comp,
					struct bnx2x_exeq_elem, link);

		list_del(&elem->link);
		bnx2x_exe_queue_free_elem(bp, elem);
	}
}

static inline void bnx2x_exe_queue_reset_pending(struct bnx2x *bp,
						 struct bnx2x_exe_queue_obj *o)
{

	spin_lock_bh(&o->lock);

	__bnx2x_exe_queue_reset_pending(bp, o);

	spin_unlock_bh(&o->lock);

}

static inline int bnx2x_exe_queue_step(struct bnx2x *bp,
				       struct bnx2x_exe_queue_obj *o,
				       unsigned long *ramrod_flags)
{
	struct bnx2x_exeq_elem *elem, spacer;
	int cur_len = 0, rc;

	memset(&spacer, 0, sizeof(spacer));

	spin_lock_bh(&o->lock);

	if (!list_empty(&o->pending_comp)) {
		if (test_bit(RAMROD_DRV_CLR_ONLY, ramrod_flags)) {
			DP(BNX2X_MSG_SP, "RAMROD_DRV_CLR_ONLY requested: resetting a pending_comp list\n");
			__bnx2x_exe_queue_reset_pending(bp, o);
		} else {
			spin_unlock_bh(&o->lock);
			return 1;
		}
	}

	while (!list_empty(&o->exe_queue)) {
		elem = list_first_entry(&o->exe_queue, struct bnx2x_exeq_elem,
					link);
		WARN_ON(!elem->cmd_len);

		if (cur_len + elem->cmd_len <= o->exe_chunk_len) {
			cur_len += elem->cmd_len;
			list_add_tail(&spacer.link, &o->pending_comp);
			mb();
			list_del(&elem->link);
			list_add_tail(&elem->link, &o->pending_comp);
			list_del(&spacer.link);
		} else
			break;
	}

	
	if (!cur_len) {
		spin_unlock_bh(&o->lock);
		return 0;
	}

	rc = o->execute(bp, o->owner, &o->pending_comp, ramrod_flags);
	if (rc < 0)
		list_splice_init(&o->pending_comp, &o->exe_queue);
	else if (!rc)
		__bnx2x_exe_queue_reset_pending(bp, o);

	spin_unlock_bh(&o->lock);
	return rc;
}

static inline bool bnx2x_exe_queue_empty(struct bnx2x_exe_queue_obj *o)
{
	bool empty = list_empty(&o->exe_queue);

	
	mb();

	return empty && list_empty(&o->pending_comp);
}

static inline struct bnx2x_exeq_elem *bnx2x_exe_queue_alloc_elem(
	struct bnx2x *bp)
{
	DP(BNX2X_MSG_SP, "Allocating a new exe_queue element\n");
	return kzalloc(sizeof(struct bnx2x_exeq_elem), GFP_ATOMIC);
}

static bool bnx2x_raw_check_pending(struct bnx2x_raw_obj *o)
{
	return !!test_bit(o->state, o->pstate);
}

static void bnx2x_raw_clear_pending(struct bnx2x_raw_obj *o)
{
	smp_mb__before_clear_bit();
	clear_bit(o->state, o->pstate);
	smp_mb__after_clear_bit();
}

static void bnx2x_raw_set_pending(struct bnx2x_raw_obj *o)
{
	smp_mb__before_clear_bit();
	set_bit(o->state, o->pstate);
	smp_mb__after_clear_bit();
}

static inline int bnx2x_state_wait(struct bnx2x *bp, int state,
				   unsigned long *pstate)
{
	
	int cnt = 5000;


	if (CHIP_REV_IS_EMUL(bp))
		cnt *= 20;

	DP(BNX2X_MSG_SP, "waiting for state to become %d\n", state);

	might_sleep();
	while (cnt--) {
		if (!test_bit(state, pstate)) {
#ifdef BNX2X_STOP_ON_ERROR
			DP(BNX2X_MSG_SP, "exit  (cnt %d)\n", 5000 - cnt);
#endif
			return 0;
		}

		usleep_range(1000, 1000);

		if (bp->panic)
			return -EIO;
	}

	
	BNX2X_ERR("timeout waiting for state %d\n", state);
#ifdef BNX2X_STOP_ON_ERROR
	bnx2x_panic();
#endif

	return -EBUSY;
}

static int bnx2x_raw_wait(struct bnx2x *bp, struct bnx2x_raw_obj *raw)
{
	return bnx2x_state_wait(bp, raw->state, raw->pstate);
}

static bool bnx2x_get_cam_offset_mac(struct bnx2x_vlan_mac_obj *o, int *offset)
{
	struct bnx2x_credit_pool_obj *mp = o->macs_pool;

	WARN_ON(!mp);

	return mp->get_entry(mp, offset);
}

static bool bnx2x_get_credit_mac(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *mp = o->macs_pool;

	WARN_ON(!mp);

	return mp->get(mp, 1);
}

static bool bnx2x_get_cam_offset_vlan(struct bnx2x_vlan_mac_obj *o, int *offset)
{
	struct bnx2x_credit_pool_obj *vp = o->vlans_pool;

	WARN_ON(!vp);

	return vp->get_entry(vp, offset);
}

static bool bnx2x_get_credit_vlan(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *vp = o->vlans_pool;

	WARN_ON(!vp);

	return vp->get(vp, 1);
}

static bool bnx2x_get_credit_vlan_mac(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *mp = o->macs_pool;
	struct bnx2x_credit_pool_obj *vp = o->vlans_pool;

	if (!mp->get(mp, 1))
		return false;

	if (!vp->get(vp, 1)) {
		mp->put(mp, 1);
		return false;
	}

	return true;
}

static bool bnx2x_put_cam_offset_mac(struct bnx2x_vlan_mac_obj *o, int offset)
{
	struct bnx2x_credit_pool_obj *mp = o->macs_pool;

	return mp->put_entry(mp, offset);
}

static bool bnx2x_put_credit_mac(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *mp = o->macs_pool;

	return mp->put(mp, 1);
}

static bool bnx2x_put_cam_offset_vlan(struct bnx2x_vlan_mac_obj *o, int offset)
{
	struct bnx2x_credit_pool_obj *vp = o->vlans_pool;

	return vp->put_entry(vp, offset);
}

static bool bnx2x_put_credit_vlan(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *vp = o->vlans_pool;

	return vp->put(vp, 1);
}

static bool bnx2x_put_credit_vlan_mac(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_credit_pool_obj *mp = o->macs_pool;
	struct bnx2x_credit_pool_obj *vp = o->vlans_pool;

	if (!mp->put(mp, 1))
		return false;

	if (!vp->put(vp, 1)) {
		mp->get(mp, 1);
		return false;
	}

	return true;
}

static int bnx2x_get_n_elements(struct bnx2x *bp, struct bnx2x_vlan_mac_obj *o,
				int n, u8 *buf)
{
	struct bnx2x_vlan_mac_registry_elem *pos;
	u8 *next = buf;
	int counter = 0;

	
	list_for_each_entry(pos, &o->head, link) {
		if (counter < n) {
			
			memset(next, 0, MAC_LEADING_ZERO_CNT);

			
			memcpy(next + MAC_LEADING_ZERO_CNT, pos->u.mac.mac,
			       ETH_ALEN);

			counter++;
			next = buf + counter * ALIGN(ETH_ALEN, sizeof(u32));

			DP(BNX2X_MSG_SP, "copied element number %d to address %p element was %pM\n",
			   counter, next, pos->u.mac.mac);
		}
	}
	return counter * ETH_ALEN;
}

static int bnx2x_check_mac_add(struct bnx2x *bp,
			       struct bnx2x_vlan_mac_obj *o,
			       union bnx2x_classification_ramrod_data *data)
{
	struct bnx2x_vlan_mac_registry_elem *pos;

	DP(BNX2X_MSG_SP, "Checking MAC %pM for ADD command\n", data->mac.mac);

	if (!is_valid_ether_addr(data->mac.mac))
		return -EINVAL;

	
	list_for_each_entry(pos, &o->head, link)
		if (!memcmp(data->mac.mac, pos->u.mac.mac, ETH_ALEN))
			return -EEXIST;

	return 0;
}

static int bnx2x_check_vlan_add(struct bnx2x *bp,
				struct bnx2x_vlan_mac_obj *o,
				union bnx2x_classification_ramrod_data *data)
{
	struct bnx2x_vlan_mac_registry_elem *pos;

	DP(BNX2X_MSG_SP, "Checking VLAN %d for ADD command\n", data->vlan.vlan);

	list_for_each_entry(pos, &o->head, link)
		if (data->vlan.vlan == pos->u.vlan.vlan)
			return -EEXIST;

	return 0;
}

static int bnx2x_check_vlan_mac_add(struct bnx2x *bp,
				    struct bnx2x_vlan_mac_obj *o,
				   union bnx2x_classification_ramrod_data *data)
{
	struct bnx2x_vlan_mac_registry_elem *pos;

	DP(BNX2X_MSG_SP, "Checking VLAN_MAC (%pM, %d) for ADD command\n",
	   data->vlan_mac.mac, data->vlan_mac.vlan);

	list_for_each_entry(pos, &o->head, link)
		if ((data->vlan_mac.vlan == pos->u.vlan_mac.vlan) &&
		    (!memcmp(data->vlan_mac.mac, pos->u.vlan_mac.mac,
			     ETH_ALEN)))
			return -EEXIST;

	return 0;
}


static struct bnx2x_vlan_mac_registry_elem *
	bnx2x_check_mac_del(struct bnx2x *bp,
			    struct bnx2x_vlan_mac_obj *o,
			    union bnx2x_classification_ramrod_data *data)
{
	struct bnx2x_vlan_mac_registry_elem *pos;

	DP(BNX2X_MSG_SP, "Checking MAC %pM for DEL command\n", data->mac.mac);

	list_for_each_entry(pos, &o->head, link)
		if (!memcmp(data->mac.mac, pos->u.mac.mac, ETH_ALEN))
			return pos;

	return NULL;
}

static struct bnx2x_vlan_mac_registry_elem *
	bnx2x_check_vlan_del(struct bnx2x *bp,
			     struct bnx2x_vlan_mac_obj *o,
			     union bnx2x_classification_ramrod_data *data)
{
	struct bnx2x_vlan_mac_registry_elem *pos;

	DP(BNX2X_MSG_SP, "Checking VLAN %d for DEL command\n", data->vlan.vlan);

	list_for_each_entry(pos, &o->head, link)
		if (data->vlan.vlan == pos->u.vlan.vlan)
			return pos;

	return NULL;
}

static struct bnx2x_vlan_mac_registry_elem *
	bnx2x_check_vlan_mac_del(struct bnx2x *bp,
				 struct bnx2x_vlan_mac_obj *o,
				 union bnx2x_classification_ramrod_data *data)
{
	struct bnx2x_vlan_mac_registry_elem *pos;

	DP(BNX2X_MSG_SP, "Checking VLAN_MAC (%pM, %d) for DEL command\n",
	   data->vlan_mac.mac, data->vlan_mac.vlan);

	list_for_each_entry(pos, &o->head, link)
		if ((data->vlan_mac.vlan == pos->u.vlan_mac.vlan) &&
		    (!memcmp(data->vlan_mac.mac, pos->u.vlan_mac.mac,
			     ETH_ALEN)))
			return pos;

	return NULL;
}

static bool bnx2x_check_move(struct bnx2x *bp,
			     struct bnx2x_vlan_mac_obj *src_o,
			     struct bnx2x_vlan_mac_obj *dst_o,
			     union bnx2x_classification_ramrod_data *data)
{
	struct bnx2x_vlan_mac_registry_elem *pos;
	int rc;

	pos = src_o->check_del(bp, src_o, data);

	
	rc = dst_o->check_add(bp, dst_o, data);

	if (rc || !pos)
		return false;

	return true;
}

static bool bnx2x_check_move_always_err(
	struct bnx2x *bp,
	struct bnx2x_vlan_mac_obj *src_o,
	struct bnx2x_vlan_mac_obj *dst_o,
	union bnx2x_classification_ramrod_data *data)
{
	return false;
}


static inline u8 bnx2x_vlan_mac_get_rx_tx_flag(struct bnx2x_vlan_mac_obj *o)
{
	struct bnx2x_raw_obj *raw = &o->raw;
	u8 rx_tx_flag = 0;

	if ((raw->obj_type == BNX2X_OBJ_TYPE_TX) ||
	    (raw->obj_type == BNX2X_OBJ_TYPE_RX_TX))
		rx_tx_flag |= ETH_CLASSIFY_CMD_HEADER_TX_CMD;

	if ((raw->obj_type == BNX2X_OBJ_TYPE_RX) ||
	    (raw->obj_type == BNX2X_OBJ_TYPE_RX_TX))
		rx_tx_flag |= ETH_CLASSIFY_CMD_HEADER_RX_CMD;

	return rx_tx_flag;
}


static inline void bnx2x_set_mac_in_nig(struct bnx2x *bp,
				 bool add, unsigned char *dev_addr, int index)
{
	u32 wb_data[2];
	u32 reg_offset = BP_PORT(bp) ? NIG_REG_LLH1_FUNC_MEM :
			 NIG_REG_LLH0_FUNC_MEM;

	if (!IS_MF_SI(bp) || index > BNX2X_LLH_CAM_MAX_PF_LINE)
		return;

	DP(BNX2X_MSG_SP, "Going to %s LLH configuration at entry %d\n",
			 (add ? "ADD" : "DELETE"), index);

	if (add) {
		
		reg_offset += 8*index;

		wb_data[0] = ((dev_addr[2] << 24) | (dev_addr[3] << 16) |
			      (dev_addr[4] <<  8) |  dev_addr[5]);
		wb_data[1] = ((dev_addr[0] <<  8) |  dev_addr[1]);

		REG_WR_DMAE(bp, reg_offset, wb_data, 2);
	}

	REG_WR(bp, (BP_PORT(bp) ? NIG_REG_LLH1_FUNC_MEM_ENABLE :
				  NIG_REG_LLH0_FUNC_MEM_ENABLE) + 4*index, add);
}

static inline void bnx2x_vlan_mac_set_cmd_hdr_e2(struct bnx2x *bp,
	struct bnx2x_vlan_mac_obj *o, bool add, int opcode,
	struct eth_classify_cmd_header *hdr)
{
	struct bnx2x_raw_obj *raw = &o->raw;

	hdr->client_id = raw->cl_id;
	hdr->func_id = raw->func_id;

	
	hdr->cmd_general_data |=
		bnx2x_vlan_mac_get_rx_tx_flag(o);

	if (add)
		hdr->cmd_general_data |= ETH_CLASSIFY_CMD_HEADER_IS_ADD;

	hdr->cmd_general_data |=
		(opcode << ETH_CLASSIFY_CMD_HEADER_OPCODE_SHIFT);
}

static inline void bnx2x_vlan_mac_set_rdata_hdr_e2(u32 cid, int type,
				struct eth_classify_header *hdr, int rule_cnt)
{
	hdr->echo = (cid & BNX2X_SWCID_MASK) | (type << BNX2X_SWCID_SHIFT);
	hdr->rule_cnt = (u8)rule_cnt;
}


static void bnx2x_set_one_mac_e2(struct bnx2x *bp,
				 struct bnx2x_vlan_mac_obj *o,
				 struct bnx2x_exeq_elem *elem, int rule_idx,
				 int cam_offset)
{
	struct bnx2x_raw_obj *raw = &o->raw;
	struct eth_classify_rules_ramrod_data *data =
		(struct eth_classify_rules_ramrod_data *)(raw->rdata);
	int rule_cnt = rule_idx + 1, cmd = elem->cmd_data.vlan_mac.cmd;
	union eth_classify_rule_cmd *rule_entry = &data->rules[rule_idx];
	bool add = (cmd == BNX2X_VLAN_MAC_ADD) ? true : false;
	unsigned long *vlan_mac_flags = &elem->cmd_data.vlan_mac.vlan_mac_flags;
	u8 *mac = elem->cmd_data.vlan_mac.u.mac.mac;

	if (cmd != BNX2X_VLAN_MAC_MOVE) {
		if (test_bit(BNX2X_ISCSI_ETH_MAC, vlan_mac_flags))
			bnx2x_set_mac_in_nig(bp, add, mac,
					     BNX2X_LLH_CAM_ISCSI_ETH_LINE);
		else if (test_bit(BNX2X_ETH_MAC, vlan_mac_flags))
			bnx2x_set_mac_in_nig(bp, add, mac,
					     BNX2X_LLH_CAM_ETH_LINE);
	}

	
	if (rule_idx == 0)
		memset(data, 0, sizeof(*data));

	
	bnx2x_vlan_mac_set_cmd_hdr_e2(bp, o, add, CLASSIFY_RULE_OPCODE_MAC,
				      &rule_entry->mac.header);

	DP(BNX2X_MSG_SP, "About to %s MAC %pM for Queue %d\n",
	   (add ? "add" : "delete"), mac, raw->cl_id);

	
	bnx2x_set_fw_mac_addr(&rule_entry->mac.mac_msb,
			      &rule_entry->mac.mac_mid,
			      &rule_entry->mac.mac_lsb, mac);

	
	if (cmd == BNX2X_VLAN_MAC_MOVE) {
		rule_entry++;
		rule_cnt++;

		
		bnx2x_vlan_mac_set_cmd_hdr_e2(bp,
					elem->cmd_data.vlan_mac.target_obj,
					      true, CLASSIFY_RULE_OPCODE_MAC,
					      &rule_entry->mac.header);

		
		bnx2x_set_fw_mac_addr(&rule_entry->mac.mac_msb,
				      &rule_entry->mac.mac_mid,
				      &rule_entry->mac.mac_lsb, mac);
	}

	
	bnx2x_vlan_mac_set_rdata_hdr_e2(raw->cid, raw->state, &data->header,
					rule_cnt);
}

static inline void bnx2x_vlan_mac_set_rdata_hdr_e1x(struct bnx2x *bp,
	struct bnx2x_vlan_mac_obj *o, int type, int cam_offset,
	struct mac_configuration_hdr *hdr)
{
	struct bnx2x_raw_obj *r = &o->raw;

	hdr->length = 1;
	hdr->offset = (u8)cam_offset;
	hdr->client_id = 0xff;
	hdr->echo = ((r->cid & BNX2X_SWCID_MASK) | (type << BNX2X_SWCID_SHIFT));
}

static inline void bnx2x_vlan_mac_set_cfg_entry_e1x(struct bnx2x *bp,
	struct bnx2x_vlan_mac_obj *o, bool add, int opcode, u8 *mac,
	u16 vlan_id, struct mac_configuration_entry *cfg_entry)
{
	struct bnx2x_raw_obj *r = &o->raw;
	u32 cl_bit_vec = (1 << r->cl_id);

	cfg_entry->clients_bit_vector = cpu_to_le32(cl_bit_vec);
	cfg_entry->pf_id = r->func_id;
	cfg_entry->vlan_id = cpu_to_le16(vlan_id);

	if (add) {
		SET_FLAG(cfg_entry->flags, MAC_CONFIGURATION_ENTRY_ACTION_TYPE,
			 T_ETH_MAC_COMMAND_SET);
		SET_FLAG(cfg_entry->flags,
			 MAC_CONFIGURATION_ENTRY_VLAN_FILTERING_MODE, opcode);

		
		bnx2x_set_fw_mac_addr(&cfg_entry->msb_mac_addr,
				      &cfg_entry->middle_mac_addr,
				      &cfg_entry->lsb_mac_addr, mac);
	} else
		SET_FLAG(cfg_entry->flags, MAC_CONFIGURATION_ENTRY_ACTION_TYPE,
			 T_ETH_MAC_COMMAND_INVALIDATE);
}

static inline void bnx2x_vlan_mac_set_rdata_e1x(struct bnx2x *bp,
	struct bnx2x_vlan_mac_obj *o, int type, int cam_offset, bool add,
	u8 *mac, u16 vlan_id, int opcode, struct mac_configuration_cmd *config)
{
	struct mac_configuration_entry *cfg_entry = &config->config_table[0];
	struct bnx2x_raw_obj *raw = &o->raw;

	bnx2x_vlan_mac_set_rdata_hdr_e1x(bp, o, type, cam_offset,
					 &config->hdr);
	bnx2x_vlan_mac_set_cfg_entry_e1x(bp, o, add, opcode, mac, vlan_id,
					 cfg_entry);

	DP(BNX2X_MSG_SP, "%s MAC %pM CLID %d CAM offset %d\n",
			 (add ? "setting" : "clearing"),
			 mac, raw->cl_id, cam_offset);
}

static void bnx2x_set_one_mac_e1x(struct bnx2x *bp,
				  struct bnx2x_vlan_mac_obj *o,
				  struct bnx2x_exeq_elem *elem, int rule_idx,
				  int cam_offset)
{
	struct bnx2x_raw_obj *raw = &o->raw;
	struct mac_configuration_cmd *config =
		(struct mac_configuration_cmd *)(raw->rdata);
	bool add = (elem->cmd_data.vlan_mac.cmd == BNX2X_VLAN_MAC_ADD) ?
		true : false;

	
	memset(config, 0, sizeof(*config));

	bnx2x_vlan_mac_set_rdata_e1x(bp, o, raw->state,
				     cam_offset, add,
				     elem->cmd_data.vlan_mac.u.mac.mac, 0,
				     ETH_VLAN_FILTER_ANY_VLAN, config);
}

static void bnx2x_set_one_vlan_e2(struct bnx2x *bp,
				  struct bnx2x_vlan_mac_obj *o,
				  struct bnx2x_exeq_elem *elem, int rule_idx,
				  int cam_offset)
{
	struct bnx2x_raw_obj *raw = &o->raw;
	struct eth_classify_rules_ramrod_data *data =
		(struct eth_classify_rules_ramrod_data *)(raw->rdata);
	int rule_cnt = rule_idx + 1;
	union eth_classify_rule_cmd *rule_entry = &data->rules[rule_idx];
	int cmd = elem->cmd_data.vlan_mac.cmd;
	bool add = (cmd == BNX2X_VLAN_MAC_ADD) ? true : false;
	u16 vlan = elem->cmd_data.vlan_mac.u.vlan.vlan;

	
	if (rule_idx == 0)
		memset(data, 0, sizeof(*data));

	
	bnx2x_vlan_mac_set_cmd_hdr_e2(bp, o, add, CLASSIFY_RULE_OPCODE_VLAN,
				      &rule_entry->vlan.header);

	DP(BNX2X_MSG_SP, "About to %s VLAN %d\n", (add ? "add" : "delete"),
			 vlan);

	
	rule_entry->vlan.vlan = cpu_to_le16(vlan);

	
	if (cmd == BNX2X_VLAN_MAC_MOVE) {
		rule_entry++;
		rule_cnt++;

		
		bnx2x_vlan_mac_set_cmd_hdr_e2(bp,
					elem->cmd_data.vlan_mac.target_obj,
					      true, CLASSIFY_RULE_OPCODE_VLAN,
					      &rule_entry->vlan.header);

		
		rule_entry->vlan.vlan = cpu_to_le16(vlan);
	}

	
	bnx2x_vlan_mac_set_rdata_hdr_e2(raw->cid, raw->state, &data->header,
					rule_cnt);
}

static void bnx2x_set_one_vlan_mac_e2(struct bnx2x *bp,
				      struct bnx2x_vlan_mac_obj *o,
				      struct bnx2x_exeq_elem *elem,
				      int rule_idx, int cam_offset)
{
	struct bnx2x_raw_obj *raw = &o->raw;
	struct eth_classify_rules_ramrod_data *data =
		(struct eth_classify_rules_ramrod_data *)(raw->rdata);
	int rule_cnt = rule_idx + 1;
	union eth_classify_rule_cmd *rule_entry = &data->rules[rule_idx];
	int cmd = elem->cmd_data.vlan_mac.cmd;
	bool add = (cmd == BNX2X_VLAN_MAC_ADD) ? true : false;
	u16 vlan = elem->cmd_data.vlan_mac.u.vlan_mac.vlan;
	u8 *mac = elem->cmd_data.vlan_mac.u.vlan_mac.mac;


	
	if (rule_idx == 0)
		memset(data, 0, sizeof(*data));

	
	bnx2x_vlan_mac_set_cmd_hdr_e2(bp, o, add, CLASSIFY_RULE_OPCODE_PAIR,
				      &rule_entry->pair.header);

	
	rule_entry->pair.vlan = cpu_to_le16(vlan);
	bnx2x_set_fw_mac_addr(&rule_entry->pair.mac_msb,
			      &rule_entry->pair.mac_mid,
			      &rule_entry->pair.mac_lsb, mac);

	
	if (cmd == BNX2X_VLAN_MAC_MOVE) {
		rule_entry++;
		rule_cnt++;

		
		bnx2x_vlan_mac_set_cmd_hdr_e2(bp,
					elem->cmd_data.vlan_mac.target_obj,
					      true, CLASSIFY_RULE_OPCODE_PAIR,
					      &rule_entry->pair.header);

		
		rule_entry->pair.vlan = cpu_to_le16(vlan);
		bnx2x_set_fw_mac_addr(&rule_entry->pair.mac_msb,
				      &rule_entry->pair.mac_mid,
				      &rule_entry->pair.mac_lsb, mac);
	}

	
	bnx2x_vlan_mac_set_rdata_hdr_e2(raw->cid, raw->state, &data->header,
					rule_cnt);
}

static void bnx2x_set_one_vlan_mac_e1h(struct bnx2x *bp,
				       struct bnx2x_vlan_mac_obj *o,
				       struct bnx2x_exeq_elem *elem,
				       int rule_idx, int cam_offset)
{
	struct bnx2x_raw_obj *raw = &o->raw;
	struct mac_configuration_cmd *config =
		(struct mac_configuration_cmd *)(raw->rdata);
	bool add = (elem->cmd_data.vlan_mac.cmd == BNX2X_VLAN_MAC_ADD) ?
		true : false;

	
	memset(config, 0, sizeof(*config));

	bnx2x_vlan_mac_set_rdata_e1x(bp, o, BNX2X_FILTER_VLAN_MAC_PENDING,
				     cam_offset, add,
				     elem->cmd_data.vlan_mac.u.vlan_mac.mac,
				     elem->cmd_data.vlan_mac.u.vlan_mac.vlan,
				     ETH_VLAN_FILTER_CLASSIFY, config);
}

#define list_next_entry(pos, member) \
	list_entry((pos)->member.next, typeof(*(pos)), member)

static int bnx2x_vlan_mac_restore(struct bnx2x *bp,
			   struct bnx2x_vlan_mac_ramrod_params *p,
			   struct bnx2x_vlan_mac_registry_elem **ppos)
{
	struct bnx2x_vlan_mac_registry_elem *pos;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;

	
	if (list_empty(&o->head)) {
		*ppos = NULL;
		return 0;
	}

	
	if (*ppos == NULL)
		*ppos = list_first_entry(&o->head,
					 struct bnx2x_vlan_mac_registry_elem,
					 link);
	else
		*ppos = list_next_entry(*ppos, link);

	pos = *ppos;

	
	if (list_is_last(&pos->link, &o->head))
		*ppos = NULL;

	
	memcpy(&p->user_req.u, &pos->u, sizeof(pos->u));

	
	p->user_req.cmd = BNX2X_VLAN_MAC_ADD;

	
	p->user_req.vlan_mac_flags = pos->vlan_mac_flags;

	
	__set_bit(RAMROD_RESTORE, &p->ramrod_flags);

	return bnx2x_config_vlan_mac(bp, p);
}

static struct bnx2x_exeq_elem *bnx2x_exeq_get_mac(
	struct bnx2x_exe_queue_obj *o,
	struct bnx2x_exeq_elem *elem)
{
	struct bnx2x_exeq_elem *pos;
	struct bnx2x_mac_ramrod_data *data = &elem->cmd_data.vlan_mac.u.mac;

	
	list_for_each_entry(pos, &o->exe_queue, link)
		if (!memcmp(&pos->cmd_data.vlan_mac.u.mac, data,
			      sizeof(*data)) &&
		    (pos->cmd_data.vlan_mac.cmd == elem->cmd_data.vlan_mac.cmd))
			return pos;

	return NULL;
}

static struct bnx2x_exeq_elem *bnx2x_exeq_get_vlan(
	struct bnx2x_exe_queue_obj *o,
	struct bnx2x_exeq_elem *elem)
{
	struct bnx2x_exeq_elem *pos;
	struct bnx2x_vlan_ramrod_data *data = &elem->cmd_data.vlan_mac.u.vlan;

	
	list_for_each_entry(pos, &o->exe_queue, link)
		if (!memcmp(&pos->cmd_data.vlan_mac.u.vlan, data,
			      sizeof(*data)) &&
		    (pos->cmd_data.vlan_mac.cmd == elem->cmd_data.vlan_mac.cmd))
			return pos;

	return NULL;
}

static struct bnx2x_exeq_elem *bnx2x_exeq_get_vlan_mac(
	struct bnx2x_exe_queue_obj *o,
	struct bnx2x_exeq_elem *elem)
{
	struct bnx2x_exeq_elem *pos;
	struct bnx2x_vlan_mac_ramrod_data *data =
		&elem->cmd_data.vlan_mac.u.vlan_mac;

	
	list_for_each_entry(pos, &o->exe_queue, link)
		if (!memcmp(&pos->cmd_data.vlan_mac.u.vlan_mac, data,
			      sizeof(*data)) &&
		    (pos->cmd_data.vlan_mac.cmd == elem->cmd_data.vlan_mac.cmd))
			return pos;

	return NULL;
}

static inline int bnx2x_validate_vlan_mac_add(struct bnx2x *bp,
					      union bnx2x_qable_obj *qo,
					      struct bnx2x_exeq_elem *elem)
{
	struct bnx2x_vlan_mac_obj *o = &qo->vlan_mac;
	struct bnx2x_exe_queue_obj *exeq = &o->exe_queue;
	int rc;

	
	rc = o->check_add(bp, o, &elem->cmd_data.vlan_mac.u);
	if (rc) {
		DP(BNX2X_MSG_SP, "ADD command is not allowed considering current registry state.\n");
		return rc;
	}

	if (exeq->get(exeq, elem)) {
		DP(BNX2X_MSG_SP, "There is a pending ADD command already\n");
		return -EEXIST;
	}


	
	if (!(test_bit(BNX2X_DONT_CONSUME_CAM_CREDIT,
		       &elem->cmd_data.vlan_mac.vlan_mac_flags) ||
	    o->get_credit(o)))
		return -EINVAL;

	return 0;
}

static inline int bnx2x_validate_vlan_mac_del(struct bnx2x *bp,
					      union bnx2x_qable_obj *qo,
					      struct bnx2x_exeq_elem *elem)
{
	struct bnx2x_vlan_mac_obj *o = &qo->vlan_mac;
	struct bnx2x_vlan_mac_registry_elem *pos;
	struct bnx2x_exe_queue_obj *exeq = &o->exe_queue;
	struct bnx2x_exeq_elem query_elem;

	pos = o->check_del(bp, o, &elem->cmd_data.vlan_mac.u);
	if (!pos) {
		DP(BNX2X_MSG_SP, "DEL command is not allowed considering current registry state\n");
		return -EEXIST;
	}

	memcpy(&query_elem, elem, sizeof(query_elem));

	
	query_elem.cmd_data.vlan_mac.cmd = BNX2X_VLAN_MAC_MOVE;
	if (exeq->get(exeq, &query_elem)) {
		BNX2X_ERR("There is a pending MOVE command already\n");
		return -EINVAL;
	}

	
	if (exeq->get(exeq, elem)) {
		DP(BNX2X_MSG_SP, "There is a pending DEL command already\n");
		return -EEXIST;
	}

	
	if (!(test_bit(BNX2X_DONT_CONSUME_CAM_CREDIT,
		       &elem->cmd_data.vlan_mac.vlan_mac_flags) ||
	    o->put_credit(o))) {
		BNX2X_ERR("Failed to return a credit\n");
		return -EINVAL;
	}

	return 0;
}

static inline int bnx2x_validate_vlan_mac_move(struct bnx2x *bp,
					       union bnx2x_qable_obj *qo,
					       struct bnx2x_exeq_elem *elem)
{
	struct bnx2x_vlan_mac_obj *src_o = &qo->vlan_mac;
	struct bnx2x_vlan_mac_obj *dest_o = elem->cmd_data.vlan_mac.target_obj;
	struct bnx2x_exeq_elem query_elem;
	struct bnx2x_exe_queue_obj *src_exeq = &src_o->exe_queue;
	struct bnx2x_exe_queue_obj *dest_exeq = &dest_o->exe_queue;

	if (!src_o->check_move(bp, src_o, dest_o,
			       &elem->cmd_data.vlan_mac.u)) {
		DP(BNX2X_MSG_SP, "MOVE command is not allowed considering current registry state\n");
		return -EINVAL;
	}

	memcpy(&query_elem, elem, sizeof(query_elem));

	
	query_elem.cmd_data.vlan_mac.cmd = BNX2X_VLAN_MAC_DEL;
	if (src_exeq->get(src_exeq, &query_elem)) {
		BNX2X_ERR("There is a pending DEL command on the source queue already\n");
		return -EINVAL;
	}

	
	if (src_exeq->get(src_exeq, elem)) {
		DP(BNX2X_MSG_SP, "There is a pending MOVE command already\n");
		return -EEXIST;
	}

	
	query_elem.cmd_data.vlan_mac.cmd = BNX2X_VLAN_MAC_ADD;
	if (dest_exeq->get(dest_exeq, &query_elem)) {
		BNX2X_ERR("There is a pending ADD command on the destination queue already\n");
		return -EINVAL;
	}

	
	if (!(test_bit(BNX2X_DONT_CONSUME_CAM_CREDIT_DEST,
		       &elem->cmd_data.vlan_mac.vlan_mac_flags) ||
	    dest_o->get_credit(dest_o)))
		return -EINVAL;

	if (!(test_bit(BNX2X_DONT_CONSUME_CAM_CREDIT,
		       &elem->cmd_data.vlan_mac.vlan_mac_flags) ||
	    src_o->put_credit(src_o))) {
		
		dest_o->put_credit(dest_o);
		return -EINVAL;
	}

	return 0;
}

static int bnx2x_validate_vlan_mac(struct bnx2x *bp,
				   union bnx2x_qable_obj *qo,
				   struct bnx2x_exeq_elem *elem)
{
	switch (elem->cmd_data.vlan_mac.cmd) {
	case BNX2X_VLAN_MAC_ADD:
		return bnx2x_validate_vlan_mac_add(bp, qo, elem);
	case BNX2X_VLAN_MAC_DEL:
		return bnx2x_validate_vlan_mac_del(bp, qo, elem);
	case BNX2X_VLAN_MAC_MOVE:
		return bnx2x_validate_vlan_mac_move(bp, qo, elem);
	default:
		return -EINVAL;
	}
}

static int bnx2x_remove_vlan_mac(struct bnx2x *bp,
				  union bnx2x_qable_obj *qo,
				  struct bnx2x_exeq_elem *elem)
{
	int rc = 0;

	
	if (test_bit(BNX2X_DONT_CONSUME_CAM_CREDIT,
		     &elem->cmd_data.vlan_mac.vlan_mac_flags))
		return 0;

	switch (elem->cmd_data.vlan_mac.cmd) {
	case BNX2X_VLAN_MAC_ADD:
	case BNX2X_VLAN_MAC_MOVE:
		rc = qo->vlan_mac.put_credit(&qo->vlan_mac);
		break;
	case BNX2X_VLAN_MAC_DEL:
		rc = qo->vlan_mac.get_credit(&qo->vlan_mac);
		break;
	default:
		return -EINVAL;
	}

	if (rc != true)
		return -EINVAL;

	return 0;
}

static int bnx2x_wait_vlan_mac(struct bnx2x *bp,
			       struct bnx2x_vlan_mac_obj *o)
{
	int cnt = 5000, rc;
	struct bnx2x_exe_queue_obj *exeq = &o->exe_queue;
	struct bnx2x_raw_obj *raw = &o->raw;

	while (cnt--) {
		
		rc = raw->wait_comp(bp, raw);
		if (rc)
			return rc;

		
		if (!bnx2x_exe_queue_empty(exeq))
			usleep_range(1000, 1000);
		else
			return 0;
	}

	return -EBUSY;
}

static int bnx2x_complete_vlan_mac(struct bnx2x *bp,
				   struct bnx2x_vlan_mac_obj *o,
				   union event_ring_elem *cqe,
				   unsigned long *ramrod_flags)
{
	struct bnx2x_raw_obj *r = &o->raw;
	int rc;

	
	bnx2x_exe_queue_reset_pending(bp, &o->exe_queue);

	
	r->clear_pending(r);

	
	if (cqe->message.error)
		return -EINVAL;

	
	if (test_bit(RAMROD_CONT, ramrod_flags)) {
		rc = bnx2x_exe_queue_step(bp, &o->exe_queue, ramrod_flags);
		if (rc < 0)
			return rc;
	}

	
	if (!bnx2x_exe_queue_empty(&o->exe_queue))
		return 1;

	return 0;
}

static int bnx2x_optimize_vlan_mac(struct bnx2x *bp,
				   union bnx2x_qable_obj *qo,
				   struct bnx2x_exeq_elem *elem)
{
	struct bnx2x_exeq_elem query, *pos;
	struct bnx2x_vlan_mac_obj *o = &qo->vlan_mac;
	struct bnx2x_exe_queue_obj *exeq = &o->exe_queue;

	memcpy(&query, elem, sizeof(query));

	switch (elem->cmd_data.vlan_mac.cmd) {
	case BNX2X_VLAN_MAC_ADD:
		query.cmd_data.vlan_mac.cmd = BNX2X_VLAN_MAC_DEL;
		break;
	case BNX2X_VLAN_MAC_DEL:
		query.cmd_data.vlan_mac.cmd = BNX2X_VLAN_MAC_ADD;
		break;
	default:
		
		return 0;
	}

	
	pos = exeq->get(exeq, &query);
	if (pos) {

		
		if (!test_bit(BNX2X_DONT_CONSUME_CAM_CREDIT,
			      &pos->cmd_data.vlan_mac.vlan_mac_flags)) {
			if ((query.cmd_data.vlan_mac.cmd ==
			     BNX2X_VLAN_MAC_ADD) && !o->put_credit(o)) {
				BNX2X_ERR("Failed to return the credit for the optimized ADD command\n");
				return -EINVAL;
			} else if (!o->get_credit(o)) { 
				BNX2X_ERR("Failed to recover the credit from the optimized DEL command\n");
				return -EINVAL;
			}
		}

		DP(BNX2X_MSG_SP, "Optimizing %s command\n",
			   (elem->cmd_data.vlan_mac.cmd == BNX2X_VLAN_MAC_ADD) ?
			   "ADD" : "DEL");

		list_del(&pos->link);
		bnx2x_exe_queue_free_elem(bp, pos);
		return 1;
	}

	return 0;
}

static inline int bnx2x_vlan_mac_get_registry_elem(
	struct bnx2x *bp,
	struct bnx2x_vlan_mac_obj *o,
	struct bnx2x_exeq_elem *elem,
	bool restore,
	struct bnx2x_vlan_mac_registry_elem **re)
{
	int cmd = elem->cmd_data.vlan_mac.cmd;
	struct bnx2x_vlan_mac_registry_elem *reg_elem;

	
	if (!restore &&
	    ((cmd == BNX2X_VLAN_MAC_ADD) || (cmd == BNX2X_VLAN_MAC_MOVE))) {
		reg_elem = kzalloc(sizeof(*reg_elem), GFP_ATOMIC);
		if (!reg_elem)
			return -ENOMEM;

		
		if (!o->get_cam_offset(o, &reg_elem->cam_offset)) {
			WARN_ON(1);
			kfree(reg_elem);
			return -EINVAL;
		}

		DP(BNX2X_MSG_SP, "Got cam offset %d\n", reg_elem->cam_offset);

		
		memcpy(&reg_elem->u, &elem->cmd_data.vlan_mac.u,
			  sizeof(reg_elem->u));

		
		reg_elem->vlan_mac_flags =
			elem->cmd_data.vlan_mac.vlan_mac_flags;
	} else 
		reg_elem = o->check_del(bp, o, &elem->cmd_data.vlan_mac.u);

	*re = reg_elem;
	return 0;
}

static int bnx2x_execute_vlan_mac(struct bnx2x *bp,
				  union bnx2x_qable_obj *qo,
				  struct list_head *exe_chunk,
				  unsigned long *ramrod_flags)
{
	struct bnx2x_exeq_elem *elem;
	struct bnx2x_vlan_mac_obj *o = &qo->vlan_mac, *cam_obj;
	struct bnx2x_raw_obj *r = &o->raw;
	int rc, idx = 0;
	bool restore = test_bit(RAMROD_RESTORE, ramrod_flags);
	bool drv_only = test_bit(RAMROD_DRV_CLR_ONLY, ramrod_flags);
	struct bnx2x_vlan_mac_registry_elem *reg_elem;
	int cmd;

	if (!drv_only) {
		WARN_ON(r->check_pending(r));

		
		r->set_pending(r);

		
		list_for_each_entry(elem, exe_chunk, link) {
			cmd = elem->cmd_data.vlan_mac.cmd;
			if (cmd == BNX2X_VLAN_MAC_MOVE)
				cam_obj = elem->cmd_data.vlan_mac.target_obj;
			else
				cam_obj = o;

			rc = bnx2x_vlan_mac_get_registry_elem(bp, cam_obj,
							      elem, restore,
							      &reg_elem);
			if (rc)
				goto error_exit;

			WARN_ON(!reg_elem);

			
			if (!restore &&
			    ((cmd == BNX2X_VLAN_MAC_ADD) ||
			    (cmd == BNX2X_VLAN_MAC_MOVE)))
				list_add(&reg_elem->link, &cam_obj->head);

			
			o->set_one_rule(bp, o, elem, idx,
					reg_elem->cam_offset);

			
			if (cmd == BNX2X_VLAN_MAC_MOVE)
				idx += 2;
			else
				idx++;
		}


		rc = bnx2x_sp_post(bp, o->ramrod_cmd, r->cid,
				   U64_HI(r->rdata_mapping),
				   U64_LO(r->rdata_mapping),
				   ETH_CONNECTION_TYPE);
		if (rc)
			goto error_exit;
	}

	
	list_for_each_entry(elem, exe_chunk, link) {
		cmd = elem->cmd_data.vlan_mac.cmd;
		if ((cmd == BNX2X_VLAN_MAC_DEL) ||
		    (cmd == BNX2X_VLAN_MAC_MOVE)) {
			reg_elem = o->check_del(bp, o,
						&elem->cmd_data.vlan_mac.u);

			WARN_ON(!reg_elem);

			o->put_cam_offset(o, reg_elem->cam_offset);
			list_del(&reg_elem->link);
			kfree(reg_elem);
		}
	}

	if (!drv_only)
		return 1;
	else
		return 0;

error_exit:
	r->clear_pending(r);

	
	list_for_each_entry(elem, exe_chunk, link) {
		cmd = elem->cmd_data.vlan_mac.cmd;

		if (cmd == BNX2X_VLAN_MAC_MOVE)
			cam_obj = elem->cmd_data.vlan_mac.target_obj;
		else
			cam_obj = o;

		
		if (!restore &&
		    ((cmd == BNX2X_VLAN_MAC_ADD) ||
		    (cmd == BNX2X_VLAN_MAC_MOVE))) {
			reg_elem = o->check_del(bp, cam_obj,
						&elem->cmd_data.vlan_mac.u);
			if (reg_elem) {
				list_del(&reg_elem->link);
				kfree(reg_elem);
			}
		}
	}

	return rc;
}

static inline int bnx2x_vlan_mac_push_new_cmd(
	struct bnx2x *bp,
	struct bnx2x_vlan_mac_ramrod_params *p)
{
	struct bnx2x_exeq_elem *elem;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;
	bool restore = test_bit(RAMROD_RESTORE, &p->ramrod_flags);

	
	elem = bnx2x_exe_queue_alloc_elem(bp);
	if (!elem)
		return -ENOMEM;

	
	switch (p->user_req.cmd) {
	case BNX2X_VLAN_MAC_MOVE:
		elem->cmd_len = 2;
		break;
	default:
		elem->cmd_len = 1;
	}

	
	memcpy(&elem->cmd_data.vlan_mac, &p->user_req, sizeof(p->user_req));

	
	return bnx2x_exe_queue_add(bp, &o->exe_queue, elem, restore);
}

int bnx2x_config_vlan_mac(
	struct bnx2x *bp,
	struct bnx2x_vlan_mac_ramrod_params *p)
{
	int rc = 0;
	struct bnx2x_vlan_mac_obj *o = p->vlan_mac_obj;
	unsigned long *ramrod_flags = &p->ramrod_flags;
	bool cont = test_bit(RAMROD_CONT, ramrod_flags);
	struct bnx2x_raw_obj *raw = &o->raw;

	if (!cont) {
		rc = bnx2x_vlan_mac_push_new_cmd(bp, p);
		if (rc)
			return rc;
	}

	if (!bnx2x_exe_queue_empty(&o->exe_queue))
		rc = 1;

	if (test_bit(RAMROD_DRV_CLR_ONLY, ramrod_flags))  {
		DP(BNX2X_MSG_SP, "RAMROD_DRV_CLR_ONLY requested: clearing a pending bit.\n");
		raw->clear_pending(raw);
	}

	
	if (cont || test_bit(RAMROD_EXEC, ramrod_flags) ||
	    test_bit(RAMROD_COMP_WAIT, ramrod_flags)) {
		rc = bnx2x_exe_queue_step(bp, &o->exe_queue, ramrod_flags);
		if (rc < 0)
			return rc;
	}

	if (test_bit(RAMROD_COMP_WAIT, &p->ramrod_flags)) {
		int max_iterations = bnx2x_exe_queue_length(&o->exe_queue) + 1;

		while (!bnx2x_exe_queue_empty(&o->exe_queue) &&
		       max_iterations--) {

			
			rc = raw->wait_comp(bp, raw);
			if (rc)
				return rc;

			
			rc = bnx2x_exe_queue_step(bp, &o->exe_queue,
						  ramrod_flags);
			if (rc < 0)
				return rc;
		}

		return 0;
	}

	return rc;
}



static int bnx2x_vlan_mac_del_all(struct bnx2x *bp,
				  struct bnx2x_vlan_mac_obj *o,
				  unsigned long *vlan_mac_flags,
				  unsigned long *ramrod_flags)
{
	struct bnx2x_vlan_mac_registry_elem *pos = NULL;
	int rc = 0;
	struct bnx2x_vlan_mac_ramrod_params p;
	struct bnx2x_exe_queue_obj *exeq = &o->exe_queue;
	struct bnx2x_exeq_elem *exeq_pos, *exeq_pos_n;

	

	spin_lock_bh(&exeq->lock);

	list_for_each_entry_safe(exeq_pos, exeq_pos_n, &exeq->exe_queue, link) {
		if (exeq_pos->cmd_data.vlan_mac.vlan_mac_flags ==
		    *vlan_mac_flags) {
			rc = exeq->remove(bp, exeq->owner, exeq_pos);
			if (rc) {
				BNX2X_ERR("Failed to remove command\n");
				spin_unlock_bh(&exeq->lock);
				return rc;
			}
			list_del(&exeq_pos->link);
		}
	}

	spin_unlock_bh(&exeq->lock);

	
	memset(&p, 0, sizeof(p));
	p.vlan_mac_obj = o;
	p.ramrod_flags = *ramrod_flags;
	p.user_req.cmd = BNX2X_VLAN_MAC_DEL;

	__clear_bit(RAMROD_COMP_WAIT, &p.ramrod_flags);
	__clear_bit(RAMROD_EXEC, &p.ramrod_flags);
	__clear_bit(RAMROD_CONT, &p.ramrod_flags);

	list_for_each_entry(pos, &o->head, link) {
		if (pos->vlan_mac_flags == *vlan_mac_flags) {
			p.user_req.vlan_mac_flags = pos->vlan_mac_flags;
			memcpy(&p.user_req.u, &pos->u, sizeof(pos->u));
			rc = bnx2x_config_vlan_mac(bp, &p);
			if (rc < 0) {
				BNX2X_ERR("Failed to add a new DEL command\n");
				return rc;
			}
		}
	}

	p.ramrod_flags = *ramrod_flags;
	__set_bit(RAMROD_CONT, &p.ramrod_flags);

	return bnx2x_config_vlan_mac(bp, &p);
}

static inline void bnx2x_init_raw_obj(struct bnx2x_raw_obj *raw, u8 cl_id,
	u32 cid, u8 func_id, void *rdata, dma_addr_t rdata_mapping, int state,
	unsigned long *pstate, bnx2x_obj_type type)
{
	raw->func_id = func_id;
	raw->cid = cid;
	raw->cl_id = cl_id;
	raw->rdata = rdata;
	raw->rdata_mapping = rdata_mapping;
	raw->state = state;
	raw->pstate = pstate;
	raw->obj_type = type;
	raw->check_pending = bnx2x_raw_check_pending;
	raw->clear_pending = bnx2x_raw_clear_pending;
	raw->set_pending = bnx2x_raw_set_pending;
	raw->wait_comp = bnx2x_raw_wait;
}

static inline void bnx2x_init_vlan_mac_common(struct bnx2x_vlan_mac_obj *o,
	u8 cl_id, u32 cid, u8 func_id, void *rdata, dma_addr_t rdata_mapping,
	int state, unsigned long *pstate, bnx2x_obj_type type,
	struct bnx2x_credit_pool_obj *macs_pool,
	struct bnx2x_credit_pool_obj *vlans_pool)
{
	INIT_LIST_HEAD(&o->head);

	o->macs_pool = macs_pool;
	o->vlans_pool = vlans_pool;

	o->delete_all = bnx2x_vlan_mac_del_all;
	o->restore = bnx2x_vlan_mac_restore;
	o->complete = bnx2x_complete_vlan_mac;
	o->wait = bnx2x_wait_vlan_mac;

	bnx2x_init_raw_obj(&o->raw, cl_id, cid, func_id, rdata, rdata_mapping,
			   state, pstate, type);
}


void bnx2x_init_mac_obj(struct bnx2x *bp,
			struct bnx2x_vlan_mac_obj *mac_obj,
			u8 cl_id, u32 cid, u8 func_id, void *rdata,
			dma_addr_t rdata_mapping, int state,
			unsigned long *pstate, bnx2x_obj_type type,
			struct bnx2x_credit_pool_obj *macs_pool)
{
	union bnx2x_qable_obj *qable_obj = (union bnx2x_qable_obj *)mac_obj;

	bnx2x_init_vlan_mac_common(mac_obj, cl_id, cid, func_id, rdata,
				   rdata_mapping, state, pstate, type,
				   macs_pool, NULL);

	
	mac_obj->get_credit = bnx2x_get_credit_mac;
	mac_obj->put_credit = bnx2x_put_credit_mac;
	mac_obj->get_cam_offset = bnx2x_get_cam_offset_mac;
	mac_obj->put_cam_offset = bnx2x_put_cam_offset_mac;

	if (CHIP_IS_E1x(bp)) {
		mac_obj->set_one_rule      = bnx2x_set_one_mac_e1x;
		mac_obj->check_del         = bnx2x_check_mac_del;
		mac_obj->check_add         = bnx2x_check_mac_add;
		mac_obj->check_move        = bnx2x_check_move_always_err;
		mac_obj->ramrod_cmd        = RAMROD_CMD_ID_ETH_SET_MAC;

		
		bnx2x_exe_queue_init(bp,
				     &mac_obj->exe_queue, 1, qable_obj,
				     bnx2x_validate_vlan_mac,
				     bnx2x_remove_vlan_mac,
				     bnx2x_optimize_vlan_mac,
				     bnx2x_execute_vlan_mac,
				     bnx2x_exeq_get_mac);
	} else {
		mac_obj->set_one_rule      = bnx2x_set_one_mac_e2;
		mac_obj->check_del         = bnx2x_check_mac_del;
		mac_obj->check_add         = bnx2x_check_mac_add;
		mac_obj->check_move        = bnx2x_check_move;
		mac_obj->ramrod_cmd        =
			RAMROD_CMD_ID_ETH_CLASSIFICATION_RULES;
		mac_obj->get_n_elements    = bnx2x_get_n_elements;

		
		bnx2x_exe_queue_init(bp,
				     &mac_obj->exe_queue, CLASSIFY_RULES_COUNT,
				     qable_obj, bnx2x_validate_vlan_mac,
				     bnx2x_remove_vlan_mac,
				     bnx2x_optimize_vlan_mac,
				     bnx2x_execute_vlan_mac,
				     bnx2x_exeq_get_mac);
	}
}

void bnx2x_init_vlan_obj(struct bnx2x *bp,
			 struct bnx2x_vlan_mac_obj *vlan_obj,
			 u8 cl_id, u32 cid, u8 func_id, void *rdata,
			 dma_addr_t rdata_mapping, int state,
			 unsigned long *pstate, bnx2x_obj_type type,
			 struct bnx2x_credit_pool_obj *vlans_pool)
{
	union bnx2x_qable_obj *qable_obj = (union bnx2x_qable_obj *)vlan_obj;

	bnx2x_init_vlan_mac_common(vlan_obj, cl_id, cid, func_id, rdata,
				   rdata_mapping, state, pstate, type, NULL,
				   vlans_pool);

	vlan_obj->get_credit = bnx2x_get_credit_vlan;
	vlan_obj->put_credit = bnx2x_put_credit_vlan;
	vlan_obj->get_cam_offset = bnx2x_get_cam_offset_vlan;
	vlan_obj->put_cam_offset = bnx2x_put_cam_offset_vlan;

	if (CHIP_IS_E1x(bp)) {
		BNX2X_ERR("Do not support chips others than E2 and newer\n");
		BUG();
	} else {
		vlan_obj->set_one_rule      = bnx2x_set_one_vlan_e2;
		vlan_obj->check_del         = bnx2x_check_vlan_del;
		vlan_obj->check_add         = bnx2x_check_vlan_add;
		vlan_obj->check_move        = bnx2x_check_move;
		vlan_obj->ramrod_cmd        =
			RAMROD_CMD_ID_ETH_CLASSIFICATION_RULES;

		
		bnx2x_exe_queue_init(bp,
				     &vlan_obj->exe_queue, CLASSIFY_RULES_COUNT,
				     qable_obj, bnx2x_validate_vlan_mac,
				     bnx2x_remove_vlan_mac,
				     bnx2x_optimize_vlan_mac,
				     bnx2x_execute_vlan_mac,
				     bnx2x_exeq_get_vlan);
	}
}

void bnx2x_init_vlan_mac_obj(struct bnx2x *bp,
			     struct bnx2x_vlan_mac_obj *vlan_mac_obj,
			     u8 cl_id, u32 cid, u8 func_id, void *rdata,
			     dma_addr_t rdata_mapping, int state,
			     unsigned long *pstate, bnx2x_obj_type type,
			     struct bnx2x_credit_pool_obj *macs_pool,
			     struct bnx2x_credit_pool_obj *vlans_pool)
{
	union bnx2x_qable_obj *qable_obj =
		(union bnx2x_qable_obj *)vlan_mac_obj;

	bnx2x_init_vlan_mac_common(vlan_mac_obj, cl_id, cid, func_id, rdata,
				   rdata_mapping, state, pstate, type,
				   macs_pool, vlans_pool);

	
	vlan_mac_obj->get_credit = bnx2x_get_credit_vlan_mac;
	vlan_mac_obj->put_credit = bnx2x_put_credit_vlan_mac;
	vlan_mac_obj->get_cam_offset = bnx2x_get_cam_offset_mac;
	vlan_mac_obj->put_cam_offset = bnx2x_put_cam_offset_mac;

	if (CHIP_IS_E1(bp)) {
		BNX2X_ERR("Do not support chips others than E2\n");
		BUG();
	} else if (CHIP_IS_E1H(bp)) {
		vlan_mac_obj->set_one_rule      = bnx2x_set_one_vlan_mac_e1h;
		vlan_mac_obj->check_del         = bnx2x_check_vlan_mac_del;
		vlan_mac_obj->check_add         = bnx2x_check_vlan_mac_add;
		vlan_mac_obj->check_move        = bnx2x_check_move_always_err;
		vlan_mac_obj->ramrod_cmd        = RAMROD_CMD_ID_ETH_SET_MAC;

		
		bnx2x_exe_queue_init(bp,
				     &vlan_mac_obj->exe_queue, 1, qable_obj,
				     bnx2x_validate_vlan_mac,
				     bnx2x_remove_vlan_mac,
				     bnx2x_optimize_vlan_mac,
				     bnx2x_execute_vlan_mac,
				     bnx2x_exeq_get_vlan_mac);
	} else {
		vlan_mac_obj->set_one_rule      = bnx2x_set_one_vlan_mac_e2;
		vlan_mac_obj->check_del         = bnx2x_check_vlan_mac_del;
		vlan_mac_obj->check_add         = bnx2x_check_vlan_mac_add;
		vlan_mac_obj->check_move        = bnx2x_check_move;
		vlan_mac_obj->ramrod_cmd        =
			RAMROD_CMD_ID_ETH_CLASSIFICATION_RULES;

		
		bnx2x_exe_queue_init(bp,
				     &vlan_mac_obj->exe_queue,
				     CLASSIFY_RULES_COUNT,
				     qable_obj, bnx2x_validate_vlan_mac,
				     bnx2x_remove_vlan_mac,
				     bnx2x_optimize_vlan_mac,
				     bnx2x_execute_vlan_mac,
				     bnx2x_exeq_get_vlan_mac);
	}

}

static inline void __storm_memset_mac_filters(struct bnx2x *bp,
			struct tstorm_eth_mac_filter_config *mac_filters,
			u16 pf_id)
{
	size_t size = sizeof(struct tstorm_eth_mac_filter_config);

	u32 addr = BAR_TSTRORM_INTMEM +
			TSTORM_MAC_FILTER_CONFIG_OFFSET(pf_id);

	__storm_memset_struct(bp, addr, size, (u32 *)mac_filters);
}

static int bnx2x_set_rx_mode_e1x(struct bnx2x *bp,
				 struct bnx2x_rx_mode_ramrod_params *p)
{
	
	u32 mask = (1 << p->cl_id);

	struct tstorm_eth_mac_filter_config *mac_filters =
		(struct tstorm_eth_mac_filter_config *)p->rdata;

	
	u8 drop_all_ucast = 1, drop_all_mcast = 1;
	u8 accp_all_ucast = 0, accp_all_bcast = 0, accp_all_mcast = 0;
	u8 unmatched_unicast = 0;

	if (test_bit(BNX2X_ACCEPT_UNICAST, &p->rx_accept_flags))
		
		drop_all_ucast = 0;

	if (test_bit(BNX2X_ACCEPT_MULTICAST, &p->rx_accept_flags))
		
		drop_all_mcast = 0;

	if (test_bit(BNX2X_ACCEPT_ALL_UNICAST, &p->rx_accept_flags)) {
		
		drop_all_ucast = 0;
		accp_all_ucast = 1;
	}
	if (test_bit(BNX2X_ACCEPT_ALL_MULTICAST, &p->rx_accept_flags)) {
		
		drop_all_mcast = 0;
		accp_all_mcast = 1;
	}
	if (test_bit(BNX2X_ACCEPT_BROADCAST, &p->rx_accept_flags))
		
		accp_all_bcast = 1;
	if (test_bit(BNX2X_ACCEPT_UNMATCHED, &p->rx_accept_flags))
		
		unmatched_unicast = 1;

	mac_filters->ucast_drop_all = drop_all_ucast ?
		mac_filters->ucast_drop_all | mask :
		mac_filters->ucast_drop_all & ~mask;

	mac_filters->mcast_drop_all = drop_all_mcast ?
		mac_filters->mcast_drop_all | mask :
		mac_filters->mcast_drop_all & ~mask;

	mac_filters->ucast_accept_all = accp_all_ucast ?
		mac_filters->ucast_accept_all | mask :
		mac_filters->ucast_accept_all & ~mask;

	mac_filters->mcast_accept_all = accp_all_mcast ?
		mac_filters->mcast_accept_all | mask :
		mac_filters->mcast_accept_all & ~mask;

	mac_filters->bcast_accept_all = accp_all_bcast ?
		mac_filters->bcast_accept_all | mask :
		mac_filters->bcast_accept_all & ~mask;

	mac_filters->unmatched_unicast = unmatched_unicast ?
		mac_filters->unmatched_unicast | mask :
		mac_filters->unmatched_unicast & ~mask;

	DP(BNX2X_MSG_SP, "drop_ucast 0x%x\ndrop_mcast 0x%x\n accp_ucast 0x%x\n"
					 "accp_mcast 0x%x\naccp_bcast 0x%x\n",
	   mac_filters->ucast_drop_all, mac_filters->mcast_drop_all,
	   mac_filters->ucast_accept_all, mac_filters->mcast_accept_all,
	   mac_filters->bcast_accept_all);

	
	__storm_memset_mac_filters(bp, mac_filters, p->func_id);

	
	clear_bit(p->state, p->pstate);
	smp_mb__after_clear_bit();

	return 0;
}

static inline void bnx2x_rx_mode_set_rdata_hdr_e2(u32 cid,
				struct eth_classify_header *hdr,
				u8 rule_cnt)
{
	hdr->echo = cid;
	hdr->rule_cnt = rule_cnt;
}

static inline void bnx2x_rx_mode_set_cmd_state_e2(struct bnx2x *bp,
				unsigned long accept_flags,
				struct eth_filter_rules_cmd *cmd,
				bool clear_accept_all)
{
	u16 state;

	
	state = ETH_FILTER_RULES_CMD_UCAST_DROP_ALL |
		ETH_FILTER_RULES_CMD_MCAST_DROP_ALL;

	if (accept_flags) {
		if (test_bit(BNX2X_ACCEPT_UNICAST, &accept_flags))
			state &= ~ETH_FILTER_RULES_CMD_UCAST_DROP_ALL;

		if (test_bit(BNX2X_ACCEPT_MULTICAST, &accept_flags))
			state &= ~ETH_FILTER_RULES_CMD_MCAST_DROP_ALL;

		if (test_bit(BNX2X_ACCEPT_ALL_UNICAST, &accept_flags)) {
			state &= ~ETH_FILTER_RULES_CMD_UCAST_DROP_ALL;
			state |= ETH_FILTER_RULES_CMD_UCAST_ACCEPT_ALL;
		}

		if (test_bit(BNX2X_ACCEPT_ALL_MULTICAST, &accept_flags)) {
			state |= ETH_FILTER_RULES_CMD_MCAST_ACCEPT_ALL;
			state &= ~ETH_FILTER_RULES_CMD_MCAST_DROP_ALL;
		}
		if (test_bit(BNX2X_ACCEPT_BROADCAST, &accept_flags))
			state |= ETH_FILTER_RULES_CMD_BCAST_ACCEPT_ALL;

		if (test_bit(BNX2X_ACCEPT_UNMATCHED, &accept_flags)) {
			state &= ~ETH_FILTER_RULES_CMD_UCAST_DROP_ALL;
			state |= ETH_FILTER_RULES_CMD_UCAST_ACCEPT_UNMATCHED;
		}
		if (test_bit(BNX2X_ACCEPT_ANY_VLAN, &accept_flags))
			state |= ETH_FILTER_RULES_CMD_ACCEPT_ANY_VLAN;
	}

	
	if (clear_accept_all) {
		state &= ~ETH_FILTER_RULES_CMD_MCAST_ACCEPT_ALL;
		state &= ~ETH_FILTER_RULES_CMD_BCAST_ACCEPT_ALL;
		state &= ~ETH_FILTER_RULES_CMD_UCAST_ACCEPT_ALL;
		state &= ~ETH_FILTER_RULES_CMD_UCAST_ACCEPT_UNMATCHED;
	}

	cmd->state = cpu_to_le16(state);

}

static int bnx2x_set_rx_mode_e2(struct bnx2x *bp,
				struct bnx2x_rx_mode_ramrod_params *p)
{
	struct eth_filter_rules_ramrod_data *data = p->rdata;
	int rc;
	u8 rule_idx = 0;

	
	memset(data, 0, sizeof(*data));

	

	
	if (test_bit(RAMROD_TX, &p->ramrod_flags)) {
		data->rules[rule_idx].client_id = p->cl_id;
		data->rules[rule_idx].func_id = p->func_id;

		data->rules[rule_idx].cmd_general_data =
			ETH_FILTER_RULES_CMD_TX_CMD;

		bnx2x_rx_mode_set_cmd_state_e2(bp, p->tx_accept_flags,
			&(data->rules[rule_idx++]), false);
	}

	
	if (test_bit(RAMROD_RX, &p->ramrod_flags)) {
		data->rules[rule_idx].client_id = p->cl_id;
		data->rules[rule_idx].func_id = p->func_id;

		data->rules[rule_idx].cmd_general_data =
			ETH_FILTER_RULES_CMD_RX_CMD;

		bnx2x_rx_mode_set_cmd_state_e2(bp, p->rx_accept_flags,
			&(data->rules[rule_idx++]), false);
	}


	if (test_bit(BNX2X_RX_MODE_FCOE_ETH, &p->rx_mode_flags)) {
		
		if (test_bit(RAMROD_TX, &p->ramrod_flags)) {
			data->rules[rule_idx].client_id = bnx2x_fcoe(bp, cl_id);
			data->rules[rule_idx].func_id = p->func_id;

			data->rules[rule_idx].cmd_general_data =
						ETH_FILTER_RULES_CMD_TX_CMD;

			bnx2x_rx_mode_set_cmd_state_e2(bp, p->tx_accept_flags,
						     &(data->rules[rule_idx++]),
						       true);
		}

		
		if (test_bit(RAMROD_RX, &p->ramrod_flags)) {
			data->rules[rule_idx].client_id = bnx2x_fcoe(bp, cl_id);
			data->rules[rule_idx].func_id = p->func_id;

			data->rules[rule_idx].cmd_general_data =
						ETH_FILTER_RULES_CMD_RX_CMD;

			bnx2x_rx_mode_set_cmd_state_e2(bp, p->rx_accept_flags,
						     &(data->rules[rule_idx++]),
						       true);
		}
	}

	bnx2x_rx_mode_set_rdata_hdr_e2(p->cid, &data->header, rule_idx);

	DP(BNX2X_MSG_SP, "About to configure %d rules, rx_accept_flags 0x%lx, tx_accept_flags 0x%lx\n",
			 data->header.rule_cnt, p->rx_accept_flags,
			 p->tx_accept_flags);


	
	rc = bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_FILTER_RULES, p->cid,
			   U64_HI(p->rdata_mapping),
			   U64_LO(p->rdata_mapping),
			   ETH_CONNECTION_TYPE);
	if (rc)
		return rc;

	
	return 1;
}

static int bnx2x_wait_rx_mode_comp_e2(struct bnx2x *bp,
				      struct bnx2x_rx_mode_ramrod_params *p)
{
	return bnx2x_state_wait(bp, p->state, p->pstate);
}

static int bnx2x_empty_rx_mode_wait(struct bnx2x *bp,
				    struct bnx2x_rx_mode_ramrod_params *p)
{
	
	return 0;
}

int bnx2x_config_rx_mode(struct bnx2x *bp,
			 struct bnx2x_rx_mode_ramrod_params *p)
{
	int rc;

	
	rc = p->rx_mode_obj->config_rx_mode(bp, p);
	if (rc < 0)
		return rc;

	
	if (test_bit(RAMROD_COMP_WAIT, &p->ramrod_flags)) {
		rc = p->rx_mode_obj->wait_comp(bp, p);
		if (rc)
			return rc;
	}

	return rc;
}

void bnx2x_init_rx_mode_obj(struct bnx2x *bp,
			    struct bnx2x_rx_mode_obj *o)
{
	if (CHIP_IS_E1x(bp)) {
		o->wait_comp      = bnx2x_empty_rx_mode_wait;
		o->config_rx_mode = bnx2x_set_rx_mode_e1x;
	} else {
		o->wait_comp      = bnx2x_wait_rx_mode_comp_e2;
		o->config_rx_mode = bnx2x_set_rx_mode_e2;
	}
}

static inline u8 bnx2x_mcast_bin_from_mac(u8 *mac)
{
	return (crc32c_le(0, mac, ETH_ALEN) >> 24) & 0xff;
}

struct bnx2x_mcast_mac_elem {
	struct list_head link;
	u8 mac[ETH_ALEN];
	u8 pad[2]; 
};

struct bnx2x_pending_mcast_cmd {
	struct list_head link;
	int type; 
	union {
		struct list_head macs_head;
		u32 macs_num; 
		int next_bin; 
	} data;

	bool done; 
};

static int bnx2x_mcast_wait(struct bnx2x *bp,
			    struct bnx2x_mcast_obj *o)
{
	if (bnx2x_state_wait(bp, o->sched_state, o->raw.pstate) ||
			o->raw.wait_comp(bp, &o->raw))
		return -EBUSY;

	return 0;
}

static int bnx2x_mcast_enqueue_cmd(struct bnx2x *bp,
				   struct bnx2x_mcast_obj *o,
				   struct bnx2x_mcast_ramrod_params *p,
				   int cmd)
{
	int total_sz;
	struct bnx2x_pending_mcast_cmd *new_cmd;
	struct bnx2x_mcast_mac_elem *cur_mac = NULL;
	struct bnx2x_mcast_list_elem *pos;
	int macs_list_len = ((cmd == BNX2X_MCAST_CMD_ADD) ?
			     p->mcast_list_len : 0);

	
	if (!p->mcast_list_len)
		return 0;

	total_sz = sizeof(*new_cmd) +
		macs_list_len * sizeof(struct bnx2x_mcast_mac_elem);

	
	new_cmd = kzalloc(total_sz, GFP_ATOMIC);

	if (!new_cmd)
		return -ENOMEM;

	DP(BNX2X_MSG_SP, "About to enqueue a new %d command. macs_list_len=%d\n",
	   cmd, macs_list_len);

	INIT_LIST_HEAD(&new_cmd->data.macs_head);

	new_cmd->type = cmd;
	new_cmd->done = false;

	switch (cmd) {
	case BNX2X_MCAST_CMD_ADD:
		cur_mac = (struct bnx2x_mcast_mac_elem *)
			  ((u8 *)new_cmd + sizeof(*new_cmd));

		list_for_each_entry(pos, &p->mcast_list, link) {
			memcpy(cur_mac->mac, pos->mac, ETH_ALEN);
			list_add_tail(&cur_mac->link, &new_cmd->data.macs_head);
			cur_mac++;
		}

		break;

	case BNX2X_MCAST_CMD_DEL:
		new_cmd->data.macs_num = p->mcast_list_len;
		break;

	case BNX2X_MCAST_CMD_RESTORE:
		new_cmd->data.next_bin = 0;
		break;

	default:
		BNX2X_ERR("Unknown command: %d\n", cmd);
		return -EINVAL;
	}

	
	list_add_tail(&new_cmd->link, &o->pending_cmds_head);

	o->set_sched(o);

	return 1;
}

static inline int bnx2x_mcast_get_next_bin(struct bnx2x_mcast_obj *o, int last)
{
	int i, j, inner_start = last % BIT_VEC64_ELEM_SZ;

	for (i = last / BIT_VEC64_ELEM_SZ; i < BNX2X_MCAST_VEC_SZ; i++) {
		if (o->registry.aprox_match.vec[i])
			for (j = inner_start; j < BIT_VEC64_ELEM_SZ; j++) {
				int cur_bit = j + BIT_VEC64_ELEM_SZ * i;
				if (BIT_VEC64_TEST_BIT(o->registry.aprox_match.
						       vec, cur_bit)) {
					return cur_bit;
				}
			}
		inner_start = 0;
	}

	
	return -1;
}

static inline int bnx2x_mcast_clear_first_bin(struct bnx2x_mcast_obj *o)
{
	int cur_bit = bnx2x_mcast_get_next_bin(o, 0);

	if (cur_bit >= 0)
		BIT_VEC64_CLEAR_BIT(o->registry.aprox_match.vec, cur_bit);

	return cur_bit;
}

static inline u8 bnx2x_mcast_get_rx_tx_flag(struct bnx2x_mcast_obj *o)
{
	struct bnx2x_raw_obj *raw = &o->raw;
	u8 rx_tx_flag = 0;

	if ((raw->obj_type == BNX2X_OBJ_TYPE_TX) ||
	    (raw->obj_type == BNX2X_OBJ_TYPE_RX_TX))
		rx_tx_flag |= ETH_MULTICAST_RULES_CMD_TX_CMD;

	if ((raw->obj_type == BNX2X_OBJ_TYPE_RX) ||
	    (raw->obj_type == BNX2X_OBJ_TYPE_RX_TX))
		rx_tx_flag |= ETH_MULTICAST_RULES_CMD_RX_CMD;

	return rx_tx_flag;
}

static void bnx2x_mcast_set_one_rule_e2(struct bnx2x *bp,
					struct bnx2x_mcast_obj *o, int idx,
					union bnx2x_mcast_config_data *cfg_data,
					int cmd)
{
	struct bnx2x_raw_obj *r = &o->raw;
	struct eth_multicast_rules_ramrod_data *data =
		(struct eth_multicast_rules_ramrod_data *)(r->rdata);
	u8 func_id = r->func_id;
	u8 rx_tx_add_flag = bnx2x_mcast_get_rx_tx_flag(o);
	int bin;

	if ((cmd == BNX2X_MCAST_CMD_ADD) || (cmd == BNX2X_MCAST_CMD_RESTORE))
		rx_tx_add_flag |= ETH_MULTICAST_RULES_CMD_IS_ADD;

	data->rules[idx].cmd_general_data |= rx_tx_add_flag;

	
	switch (cmd) {
	case BNX2X_MCAST_CMD_ADD:
		bin = bnx2x_mcast_bin_from_mac(cfg_data->mac);
		BIT_VEC64_SET_BIT(o->registry.aprox_match.vec, bin);
		break;

	case BNX2X_MCAST_CMD_DEL:
		bin = bnx2x_mcast_clear_first_bin(o);
		break;

	case BNX2X_MCAST_CMD_RESTORE:
		bin = cfg_data->bin;
		break;

	default:
		BNX2X_ERR("Unknown command: %d\n", cmd);
		return;
	}

	DP(BNX2X_MSG_SP, "%s bin %d\n",
			 ((rx_tx_add_flag & ETH_MULTICAST_RULES_CMD_IS_ADD) ?
			 "Setting"  : "Clearing"), bin);

	data->rules[idx].bin_id    = (u8)bin;
	data->rules[idx].func_id   = func_id;
	data->rules[idx].engine_id = o->engine_id;
}

static inline int bnx2x_mcast_handle_restore_cmd_e2(
	struct bnx2x *bp, struct bnx2x_mcast_obj *o , int start_bin,
	int *rdata_idx)
{
	int cur_bin, cnt = *rdata_idx;
	union bnx2x_mcast_config_data cfg_data = {0};

	
	for (cur_bin = bnx2x_mcast_get_next_bin(o, start_bin); cur_bin >= 0;
	    cur_bin = bnx2x_mcast_get_next_bin(o, cur_bin + 1)) {

		cfg_data.bin = (u8)cur_bin;
		o->set_one_rule(bp, o, cnt, &cfg_data,
				BNX2X_MCAST_CMD_RESTORE);

		cnt++;

		DP(BNX2X_MSG_SP, "About to configure a bin %d\n", cur_bin);

		if (cnt >= o->max_cmd_len)
			break;
	}

	*rdata_idx = cnt;

	return cur_bin;
}

static inline void bnx2x_mcast_hdl_pending_add_e2(struct bnx2x *bp,
	struct bnx2x_mcast_obj *o, struct bnx2x_pending_mcast_cmd *cmd_pos,
	int *line_idx)
{
	struct bnx2x_mcast_mac_elem *pmac_pos, *pmac_pos_n;
	int cnt = *line_idx;
	union bnx2x_mcast_config_data cfg_data = {0};

	list_for_each_entry_safe(pmac_pos, pmac_pos_n, &cmd_pos->data.macs_head,
				 link) {

		cfg_data.mac = &pmac_pos->mac[0];
		o->set_one_rule(bp, o, cnt, &cfg_data, cmd_pos->type);

		cnt++;

		DP(BNX2X_MSG_SP, "About to configure %pM mcast MAC\n",
		   pmac_pos->mac);

		list_del(&pmac_pos->link);

		if (cnt >= o->max_cmd_len)
			break;
	}

	*line_idx = cnt;

	
	if (list_empty(&cmd_pos->data.macs_head))
		cmd_pos->done = true;
}

static inline void bnx2x_mcast_hdl_pending_del_e2(struct bnx2x *bp,
	struct bnx2x_mcast_obj *o, struct bnx2x_pending_mcast_cmd *cmd_pos,
	int *line_idx)
{
	int cnt = *line_idx;

	while (cmd_pos->data.macs_num) {
		o->set_one_rule(bp, o, cnt, NULL, cmd_pos->type);

		cnt++;

		cmd_pos->data.macs_num--;

		  DP(BNX2X_MSG_SP, "Deleting MAC. %d left,cnt is %d\n",
				   cmd_pos->data.macs_num, cnt);

		if (cnt >= o->max_cmd_len)
			break;
	}

	*line_idx = cnt;

	
	if (!cmd_pos->data.macs_num)
		cmd_pos->done = true;
}

static inline void bnx2x_mcast_hdl_pending_restore_e2(struct bnx2x *bp,
	struct bnx2x_mcast_obj *o, struct bnx2x_pending_mcast_cmd *cmd_pos,
	int *line_idx)
{
	cmd_pos->data.next_bin = o->hdl_restore(bp, o, cmd_pos->data.next_bin,
						line_idx);

	if (cmd_pos->data.next_bin < 0)
		
		cmd_pos->done = true;
	else
		
		cmd_pos->data.next_bin++;
}

static inline int bnx2x_mcast_handle_pending_cmds_e2(struct bnx2x *bp,
				struct bnx2x_mcast_ramrod_params *p)
{
	struct bnx2x_pending_mcast_cmd *cmd_pos, *cmd_pos_n;
	int cnt = 0;
	struct bnx2x_mcast_obj *o = p->mcast_obj;

	list_for_each_entry_safe(cmd_pos, cmd_pos_n, &o->pending_cmds_head,
				 link) {
		switch (cmd_pos->type) {
		case BNX2X_MCAST_CMD_ADD:
			bnx2x_mcast_hdl_pending_add_e2(bp, o, cmd_pos, &cnt);
			break;

		case BNX2X_MCAST_CMD_DEL:
			bnx2x_mcast_hdl_pending_del_e2(bp, o, cmd_pos, &cnt);
			break;

		case BNX2X_MCAST_CMD_RESTORE:
			bnx2x_mcast_hdl_pending_restore_e2(bp, o, cmd_pos,
							   &cnt);
			break;

		default:
			BNX2X_ERR("Unknown command: %d\n", cmd_pos->type);
			return -EINVAL;
		}

		if (cmd_pos->done) {
			list_del(&cmd_pos->link);
			kfree(cmd_pos);
		}

		
		if (cnt >= o->max_cmd_len)
			break;
	}

	return cnt;
}

static inline void bnx2x_mcast_hdl_add(struct bnx2x *bp,
	struct bnx2x_mcast_obj *o, struct bnx2x_mcast_ramrod_params *p,
	int *line_idx)
{
	struct bnx2x_mcast_list_elem *mlist_pos;
	union bnx2x_mcast_config_data cfg_data = {0};
	int cnt = *line_idx;

	list_for_each_entry(mlist_pos, &p->mcast_list, link) {
		cfg_data.mac = mlist_pos->mac;
		o->set_one_rule(bp, o, cnt, &cfg_data, BNX2X_MCAST_CMD_ADD);

		cnt++;

		DP(BNX2X_MSG_SP, "About to configure %pM mcast MAC\n",
				 mlist_pos->mac);
	}

	*line_idx = cnt;
}

static inline void bnx2x_mcast_hdl_del(struct bnx2x *bp,
	struct bnx2x_mcast_obj *o, struct bnx2x_mcast_ramrod_params *p,
	int *line_idx)
{
	int cnt = *line_idx, i;

	for (i = 0; i < p->mcast_list_len; i++) {
		o->set_one_rule(bp, o, cnt, NULL, BNX2X_MCAST_CMD_DEL);

		cnt++;

		DP(BNX2X_MSG_SP, "Deleting MAC. %d left\n",
				 p->mcast_list_len - i - 1);
	}

	*line_idx = cnt;
}

static inline int bnx2x_mcast_handle_current_cmd(struct bnx2x *bp,
			struct bnx2x_mcast_ramrod_params *p, int cmd,
			int start_cnt)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	int cnt = start_cnt;

	DP(BNX2X_MSG_SP, "p->mcast_list_len=%d\n", p->mcast_list_len);

	switch (cmd) {
	case BNX2X_MCAST_CMD_ADD:
		bnx2x_mcast_hdl_add(bp, o, p, &cnt);
		break;

	case BNX2X_MCAST_CMD_DEL:
		bnx2x_mcast_hdl_del(bp, o, p, &cnt);
		break;

	case BNX2X_MCAST_CMD_RESTORE:
		o->hdl_restore(bp, o, 0, &cnt);
		break;

	default:
		BNX2X_ERR("Unknown command: %d\n", cmd);
		return -EINVAL;
	}

	
	p->mcast_list_len = 0;

	return cnt;
}

static int bnx2x_mcast_validate_e2(struct bnx2x *bp,
				   struct bnx2x_mcast_ramrod_params *p,
				   int cmd)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	int reg_sz = o->get_registry_size(o);

	switch (cmd) {
	
	case BNX2X_MCAST_CMD_DEL:
		o->set_registry_size(o, 0);
		

	
	case BNX2X_MCAST_CMD_RESTORE:
		p->mcast_list_len = reg_sz;
		break;

	case BNX2X_MCAST_CMD_ADD:
	case BNX2X_MCAST_CMD_CONT:
		o->set_registry_size(o, reg_sz + p->mcast_list_len);
		break;

	default:
		BNX2X_ERR("Unknown command: %d\n", cmd);
		return -EINVAL;

	}

	
	o->total_pending_num += p->mcast_list_len;

	return 0;
}

static void bnx2x_mcast_revert_e2(struct bnx2x *bp,
				      struct bnx2x_mcast_ramrod_params *p,
				      int old_num_bins)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;

	o->set_registry_size(o, old_num_bins);
	o->total_pending_num -= p->mcast_list_len;
}

static inline void bnx2x_mcast_set_rdata_hdr_e2(struct bnx2x *bp,
					struct bnx2x_mcast_ramrod_params *p,
					u8 len)
{
	struct bnx2x_raw_obj *r = &p->mcast_obj->raw;
	struct eth_multicast_rules_ramrod_data *data =
		(struct eth_multicast_rules_ramrod_data *)(r->rdata);

	data->header.echo = ((r->cid & BNX2X_SWCID_MASK) |
			  (BNX2X_FILTER_MCAST_PENDING << BNX2X_SWCID_SHIFT));
	data->header.rule_cnt = len;
}

static inline int bnx2x_mcast_refresh_registry_e2(struct bnx2x *bp,
						  struct bnx2x_mcast_obj *o)
{
	int i, cnt = 0;
	u64 elem;

	for (i = 0; i < BNX2X_MCAST_VEC_SZ; i++) {
		elem = o->registry.aprox_match.vec[i];
		for (; elem; cnt++)
			elem &= elem - 1;
	}

	o->set_registry_size(o, cnt);

	return 0;
}

static int bnx2x_mcast_setup_e2(struct bnx2x *bp,
				struct bnx2x_mcast_ramrod_params *p,
				int cmd)
{
	struct bnx2x_raw_obj *raw = &p->mcast_obj->raw;
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	struct eth_multicast_rules_ramrod_data *data =
		(struct eth_multicast_rules_ramrod_data *)(raw->rdata);
	int cnt = 0, rc;

	
	memset(data, 0, sizeof(*data));

	cnt = bnx2x_mcast_handle_pending_cmds_e2(bp, p);

	
	if (list_empty(&o->pending_cmds_head))
		o->clear_sched(o);

	if (p->mcast_list_len > 0)
		cnt = bnx2x_mcast_handle_current_cmd(bp, p, cmd, cnt);

	o->total_pending_num -= cnt;

	
	WARN_ON(o->total_pending_num < 0);
	WARN_ON(cnt > o->max_cmd_len);

	bnx2x_mcast_set_rdata_hdr_e2(bp, p, (u8)cnt);

	if (!o->total_pending_num)
		bnx2x_mcast_refresh_registry_e2(bp, o);

	if (test_bit(RAMROD_DRV_CLR_ONLY, &p->ramrod_flags)) {
		raw->clear_pending(raw);
		return 0;
	} else {

		
		rc = bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_MULTICAST_RULES,
				   raw->cid, U64_HI(raw->rdata_mapping),
				   U64_LO(raw->rdata_mapping),
				   ETH_CONNECTION_TYPE);
		if (rc)
			return rc;

		
		return 1;
	}
}

static int bnx2x_mcast_validate_e1h(struct bnx2x *bp,
				    struct bnx2x_mcast_ramrod_params *p,
				    int cmd)
{
	
	if ((cmd == BNX2X_MCAST_CMD_DEL) || (cmd == BNX2X_MCAST_CMD_RESTORE))
		p->mcast_list_len = 1;

	return 0;
}

static void bnx2x_mcast_revert_e1h(struct bnx2x *bp,
				       struct bnx2x_mcast_ramrod_params *p,
				       int old_num_bins)
{
	
}

#define BNX2X_57711_SET_MC_FILTER(filter, bit) \
do { \
	(filter)[(bit) >> 5] |= (1 << ((bit) & 0x1f)); \
} while (0)

static inline void bnx2x_mcast_hdl_add_e1h(struct bnx2x *bp,
					   struct bnx2x_mcast_obj *o,
					   struct bnx2x_mcast_ramrod_params *p,
					   u32 *mc_filter)
{
	struct bnx2x_mcast_list_elem *mlist_pos;
	int bit;

	list_for_each_entry(mlist_pos, &p->mcast_list, link) {
		bit = bnx2x_mcast_bin_from_mac(mlist_pos->mac);
		BNX2X_57711_SET_MC_FILTER(mc_filter, bit);

		DP(BNX2X_MSG_SP, "About to configure %pM mcast MAC, bin %d\n",
				 mlist_pos->mac, bit);

		
		BIT_VEC64_SET_BIT(o->registry.aprox_match.vec,
				  bit);
	}
}

static inline void bnx2x_mcast_hdl_restore_e1h(struct bnx2x *bp,
	struct bnx2x_mcast_obj *o, struct bnx2x_mcast_ramrod_params *p,
	u32 *mc_filter)
{
	int bit;

	for (bit = bnx2x_mcast_get_next_bin(o, 0);
	     bit >= 0;
	     bit = bnx2x_mcast_get_next_bin(o, bit + 1)) {
		BNX2X_57711_SET_MC_FILTER(mc_filter, bit);
		DP(BNX2X_MSG_SP, "About to set bin %d\n", bit);
	}
}

static int bnx2x_mcast_setup_e1h(struct bnx2x *bp,
				 struct bnx2x_mcast_ramrod_params *p,
				 int cmd)
{
	int i;
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	struct bnx2x_raw_obj *r = &o->raw;

	if (!test_bit(RAMROD_DRV_CLR_ONLY, &p->ramrod_flags)) {
		u32 mc_filter[MC_HASH_SIZE] = {0};

		switch (cmd) {
		case BNX2X_MCAST_CMD_ADD:
			bnx2x_mcast_hdl_add_e1h(bp, o, p, mc_filter);
			break;

		case BNX2X_MCAST_CMD_DEL:
			DP(BNX2X_MSG_SP,
			   "Invalidating multicast MACs configuration\n");

			
			memset(o->registry.aprox_match.vec, 0,
			       sizeof(o->registry.aprox_match.vec));
			break;

		case BNX2X_MCAST_CMD_RESTORE:
			bnx2x_mcast_hdl_restore_e1h(bp, o, p, mc_filter);
			break;

		default:
			BNX2X_ERR("Unknown command: %d\n", cmd);
			return -EINVAL;
		}

		
		for (i = 0; i < MC_HASH_SIZE; i++)
			REG_WR(bp, MC_HASH_OFFSET(bp, i), mc_filter[i]);
	} else
		
		memset(o->registry.aprox_match.vec, 0,
		       sizeof(o->registry.aprox_match.vec));

	
	r->clear_pending(r);

	return 0;
}

static int bnx2x_mcast_validate_e1(struct bnx2x *bp,
				   struct bnx2x_mcast_ramrod_params *p,
				   int cmd)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	int reg_sz = o->get_registry_size(o);

	switch (cmd) {
	
	case BNX2X_MCAST_CMD_DEL:
		o->set_registry_size(o, 0);
		

	
	case BNX2X_MCAST_CMD_RESTORE:
		p->mcast_list_len = reg_sz;
		  DP(BNX2X_MSG_SP, "Command %d, p->mcast_list_len=%d\n",
				   cmd, p->mcast_list_len);
		break;

	case BNX2X_MCAST_CMD_ADD:
	case BNX2X_MCAST_CMD_CONT:
		if (p->mcast_list_len > o->max_cmd_len) {
			BNX2X_ERR("Can't configure more than %d multicast MACs on 57710\n",
				  o->max_cmd_len);
			return -EINVAL;
		}
		DP(BNX2X_MSG_SP, "p->mcast_list_len=%d\n", p->mcast_list_len);
		if (p->mcast_list_len > 0)
			o->set_registry_size(o, p->mcast_list_len);

		break;

	default:
		BNX2X_ERR("Unknown command: %d\n", cmd);
		return -EINVAL;

	}

	if (p->mcast_list_len)
		o->total_pending_num += o->max_cmd_len;

	return 0;
}

static void bnx2x_mcast_revert_e1(struct bnx2x *bp,
				      struct bnx2x_mcast_ramrod_params *p,
				      int old_num_macs)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;

	o->set_registry_size(o, old_num_macs);

	if (p->mcast_list_len)
		o->total_pending_num -= o->max_cmd_len;
}

static void bnx2x_mcast_set_one_rule_e1(struct bnx2x *bp,
					struct bnx2x_mcast_obj *o, int idx,
					union bnx2x_mcast_config_data *cfg_data,
					int cmd)
{
	struct bnx2x_raw_obj *r = &o->raw;
	struct mac_configuration_cmd *data =
		(struct mac_configuration_cmd *)(r->rdata);

	
	if ((cmd == BNX2X_MCAST_CMD_ADD) || (cmd == BNX2X_MCAST_CMD_RESTORE)) {
		bnx2x_set_fw_mac_addr(&data->config_table[idx].msb_mac_addr,
				      &data->config_table[idx].middle_mac_addr,
				      &data->config_table[idx].lsb_mac_addr,
				      cfg_data->mac);

		data->config_table[idx].vlan_id = 0;
		data->config_table[idx].pf_id = r->func_id;
		data->config_table[idx].clients_bit_vector =
			cpu_to_le32(1 << r->cl_id);

		SET_FLAG(data->config_table[idx].flags,
			 MAC_CONFIGURATION_ENTRY_ACTION_TYPE,
			 T_ETH_MAC_COMMAND_SET);
	}
}

static inline void bnx2x_mcast_set_rdata_hdr_e1(struct bnx2x *bp,
					struct bnx2x_mcast_ramrod_params *p,
					u8 len)
{
	struct bnx2x_raw_obj *r = &p->mcast_obj->raw;
	struct mac_configuration_cmd *data =
		(struct mac_configuration_cmd *)(r->rdata);

	u8 offset = (CHIP_REV_IS_SLOW(bp) ?
		     BNX2X_MAX_EMUL_MULTI*(1 + r->func_id) :
		     BNX2X_MAX_MULTICAST*(1 + r->func_id));

	data->hdr.offset = offset;
	data->hdr.client_id = 0xff;
	data->hdr.echo = ((r->cid & BNX2X_SWCID_MASK) |
			  (BNX2X_FILTER_MCAST_PENDING << BNX2X_SWCID_SHIFT));
	data->hdr.length = len;
}

static inline int bnx2x_mcast_handle_restore_cmd_e1(
	struct bnx2x *bp, struct bnx2x_mcast_obj *o , int start_idx,
	int *rdata_idx)
{
	struct bnx2x_mcast_mac_elem *elem;
	int i = 0;
	union bnx2x_mcast_config_data cfg_data = {0};

	
	list_for_each_entry(elem, &o->registry.exact_match.macs, link) {
		cfg_data.mac = &elem->mac[0];
		o->set_one_rule(bp, o, i, &cfg_data, BNX2X_MCAST_CMD_RESTORE);

		i++;

		  DP(BNX2X_MSG_SP, "About to configure %pM mcast MAC\n",
				   cfg_data.mac);
	}

	*rdata_idx = i;

	return -1;
}


static inline int bnx2x_mcast_handle_pending_cmds_e1(
	struct bnx2x *bp, struct bnx2x_mcast_ramrod_params *p)
{
	struct bnx2x_pending_mcast_cmd *cmd_pos;
	struct bnx2x_mcast_mac_elem *pmac_pos;
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	union bnx2x_mcast_config_data cfg_data = {0};
	int cnt = 0;


	
	if (list_empty(&o->pending_cmds_head))
		return 0;

	
	cmd_pos = list_first_entry(&o->pending_cmds_head,
				   struct bnx2x_pending_mcast_cmd, link);

	switch (cmd_pos->type) {
	case BNX2X_MCAST_CMD_ADD:
		list_for_each_entry(pmac_pos, &cmd_pos->data.macs_head, link) {
			cfg_data.mac = &pmac_pos->mac[0];
			o->set_one_rule(bp, o, cnt, &cfg_data, cmd_pos->type);

			cnt++;

			DP(BNX2X_MSG_SP, "About to configure %pM mcast MAC\n",
					 pmac_pos->mac);
		}
		break;

	case BNX2X_MCAST_CMD_DEL:
		cnt = cmd_pos->data.macs_num;
		DP(BNX2X_MSG_SP, "About to delete %d multicast MACs\n", cnt);
		break;

	case BNX2X_MCAST_CMD_RESTORE:
		o->hdl_restore(bp, o, 0, &cnt);
		break;

	default:
		BNX2X_ERR("Unknown command: %d\n", cmd_pos->type);
		return -EINVAL;
	}

	list_del(&cmd_pos->link);
	kfree(cmd_pos);

	return cnt;
}

static inline void bnx2x_get_fw_mac_addr(__le16 *fw_hi, __le16 *fw_mid,
					 __le16 *fw_lo, u8 *mac)
{
	mac[1] = ((u8 *)fw_hi)[0];
	mac[0] = ((u8 *)fw_hi)[1];
	mac[3] = ((u8 *)fw_mid)[0];
	mac[2] = ((u8 *)fw_mid)[1];
	mac[5] = ((u8 *)fw_lo)[0];
	mac[4] = ((u8 *)fw_lo)[1];
}

static inline int bnx2x_mcast_refresh_registry_e1(struct bnx2x *bp,
						  struct bnx2x_mcast_obj *o)
{
	struct bnx2x_raw_obj *raw = &o->raw;
	struct bnx2x_mcast_mac_elem *elem;
	struct mac_configuration_cmd *data =
			(struct mac_configuration_cmd *)(raw->rdata);

	if (GET_FLAG(data->config_table[0].flags,
			MAC_CONFIGURATION_ENTRY_ACTION_TYPE)) {
		int i, len = data->hdr.length;

		
		if (!list_empty(&o->registry.exact_match.macs))
			return 0;

		elem = kcalloc(len, sizeof(*elem), GFP_ATOMIC);
		if (!elem) {
			BNX2X_ERR("Failed to allocate registry memory\n");
			return -ENOMEM;
		}

		for (i = 0; i < len; i++, elem++) {
			bnx2x_get_fw_mac_addr(
				&data->config_table[i].msb_mac_addr,
				&data->config_table[i].middle_mac_addr,
				&data->config_table[i].lsb_mac_addr,
				elem->mac);
			DP(BNX2X_MSG_SP, "Adding registry entry for [%pM]\n",
			   elem->mac);
			list_add_tail(&elem->link,
				      &o->registry.exact_match.macs);
		}
	} else {
		elem = list_first_entry(&o->registry.exact_match.macs,
					struct bnx2x_mcast_mac_elem, link);
		DP(BNX2X_MSG_SP, "Deleting a registry\n");
		kfree(elem);
		INIT_LIST_HEAD(&o->registry.exact_match.macs);
	}

	return 0;
}

static int bnx2x_mcast_setup_e1(struct bnx2x *bp,
				struct bnx2x_mcast_ramrod_params *p,
				int cmd)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	struct bnx2x_raw_obj *raw = &o->raw;
	struct mac_configuration_cmd *data =
		(struct mac_configuration_cmd *)(raw->rdata);
	int cnt = 0, i, rc;

	
	memset(data, 0, sizeof(*data));

	
	for (i = 0; i < o->max_cmd_len ; i++)
		SET_FLAG(data->config_table[i].flags,
			 MAC_CONFIGURATION_ENTRY_ACTION_TYPE,
			 T_ETH_MAC_COMMAND_INVALIDATE);

	
	cnt = bnx2x_mcast_handle_pending_cmds_e1(bp, p);

	
	if (list_empty(&o->pending_cmds_head))
		o->clear_sched(o);

	
	if (!cnt)
		cnt = bnx2x_mcast_handle_current_cmd(bp, p, cmd, 0);

	o->total_pending_num -= o->max_cmd_len;

	

	WARN_ON(cnt > o->max_cmd_len);

	
	bnx2x_mcast_set_rdata_hdr_e1(bp, p, (u8)cnt);

	rc = bnx2x_mcast_refresh_registry_e1(bp, o);
	if (rc)
		return rc;

	if (test_bit(RAMROD_DRV_CLR_ONLY, &p->ramrod_flags)) {
		raw->clear_pending(raw);
		return 0;
	} else {

		
		rc = bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_SET_MAC, raw->cid,
				   U64_HI(raw->rdata_mapping),
				   U64_LO(raw->rdata_mapping),
				   ETH_CONNECTION_TYPE);
		if (rc)
			return rc;

		
		return 1;
	}

}

static int bnx2x_mcast_get_registry_size_exact(struct bnx2x_mcast_obj *o)
{
	return o->registry.exact_match.num_macs_set;
}

static int bnx2x_mcast_get_registry_size_aprox(struct bnx2x_mcast_obj *o)
{
	return o->registry.aprox_match.num_bins_set;
}

static void bnx2x_mcast_set_registry_size_exact(struct bnx2x_mcast_obj *o,
						int n)
{
	o->registry.exact_match.num_macs_set = n;
}

static void bnx2x_mcast_set_registry_size_aprox(struct bnx2x_mcast_obj *o,
						int n)
{
	o->registry.aprox_match.num_bins_set = n;
}

int bnx2x_config_mcast(struct bnx2x *bp,
		       struct bnx2x_mcast_ramrod_params *p,
		       int cmd)
{
	struct bnx2x_mcast_obj *o = p->mcast_obj;
	struct bnx2x_raw_obj *r = &o->raw;
	int rc = 0, old_reg_size;

	old_reg_size = o->get_registry_size(o);

	
	rc = o->validate(bp, p, cmd);
	if (rc)
		return rc;

	
	if ((!p->mcast_list_len) && (!o->check_sched(o)))
		return 0;

	DP(BNX2X_MSG_SP, "o->total_pending_num=%d p->mcast_list_len=%d o->max_cmd_len=%d\n",
	   o->total_pending_num, p->mcast_list_len, o->max_cmd_len);

	if (r->check_pending(r) ||
	    ((o->max_cmd_len > 0) && (o->total_pending_num > o->max_cmd_len))) {
		rc = o->enqueue_cmd(bp, p->mcast_obj, p, cmd);
		if (rc < 0)
			goto error_exit1;

		p->mcast_list_len = 0;
	}

	if (!r->check_pending(r)) {

		
		r->set_pending(r);

		
		rc = o->config_mcast(bp, p, cmd);
		if (rc < 0)
			goto error_exit2;

		
		if (test_bit(RAMROD_COMP_WAIT, &p->ramrod_flags))
			rc = o->wait_comp(bp, o);
	}

	return rc;

error_exit2:
	r->clear_pending(r);

error_exit1:
	o->revert(bp, p, old_reg_size);

	return rc;
}

static void bnx2x_mcast_clear_sched(struct bnx2x_mcast_obj *o)
{
	smp_mb__before_clear_bit();
	clear_bit(o->sched_state, o->raw.pstate);
	smp_mb__after_clear_bit();
}

static void bnx2x_mcast_set_sched(struct bnx2x_mcast_obj *o)
{
	smp_mb__before_clear_bit();
	set_bit(o->sched_state, o->raw.pstate);
	smp_mb__after_clear_bit();
}

static bool bnx2x_mcast_check_sched(struct bnx2x_mcast_obj *o)
{
	return !!test_bit(o->sched_state, o->raw.pstate);
}

static bool bnx2x_mcast_check_pending(struct bnx2x_mcast_obj *o)
{
	return o->raw.check_pending(&o->raw) || o->check_sched(o);
}

void bnx2x_init_mcast_obj(struct bnx2x *bp,
			  struct bnx2x_mcast_obj *mcast_obj,
			  u8 mcast_cl_id, u32 mcast_cid, u8 func_id,
			  u8 engine_id, void *rdata, dma_addr_t rdata_mapping,
			  int state, unsigned long *pstate, bnx2x_obj_type type)
{
	memset(mcast_obj, 0, sizeof(*mcast_obj));

	bnx2x_init_raw_obj(&mcast_obj->raw, mcast_cl_id, mcast_cid, func_id,
			   rdata, rdata_mapping, state, pstate, type);

	mcast_obj->engine_id = engine_id;

	INIT_LIST_HEAD(&mcast_obj->pending_cmds_head);

	mcast_obj->sched_state = BNX2X_FILTER_MCAST_SCHED;
	mcast_obj->check_sched = bnx2x_mcast_check_sched;
	mcast_obj->set_sched = bnx2x_mcast_set_sched;
	mcast_obj->clear_sched = bnx2x_mcast_clear_sched;

	if (CHIP_IS_E1(bp)) {
		mcast_obj->config_mcast      = bnx2x_mcast_setup_e1;
		mcast_obj->enqueue_cmd       = bnx2x_mcast_enqueue_cmd;
		mcast_obj->hdl_restore       =
			bnx2x_mcast_handle_restore_cmd_e1;
		mcast_obj->check_pending     = bnx2x_mcast_check_pending;

		if (CHIP_REV_IS_SLOW(bp))
			mcast_obj->max_cmd_len = BNX2X_MAX_EMUL_MULTI;
		else
			mcast_obj->max_cmd_len = BNX2X_MAX_MULTICAST;

		mcast_obj->wait_comp         = bnx2x_mcast_wait;
		mcast_obj->set_one_rule      = bnx2x_mcast_set_one_rule_e1;
		mcast_obj->validate          = bnx2x_mcast_validate_e1;
		mcast_obj->revert            = bnx2x_mcast_revert_e1;
		mcast_obj->get_registry_size =
			bnx2x_mcast_get_registry_size_exact;
		mcast_obj->set_registry_size =
			bnx2x_mcast_set_registry_size_exact;

		INIT_LIST_HEAD(&mcast_obj->registry.exact_match.macs);

	} else if (CHIP_IS_E1H(bp)) {
		mcast_obj->config_mcast  = bnx2x_mcast_setup_e1h;
		mcast_obj->enqueue_cmd   = NULL;
		mcast_obj->hdl_restore   = NULL;
		mcast_obj->check_pending = bnx2x_mcast_check_pending;

		mcast_obj->max_cmd_len       = -1;
		mcast_obj->wait_comp         = bnx2x_mcast_wait;
		mcast_obj->set_one_rule      = NULL;
		mcast_obj->validate          = bnx2x_mcast_validate_e1h;
		mcast_obj->revert            = bnx2x_mcast_revert_e1h;
		mcast_obj->get_registry_size =
			bnx2x_mcast_get_registry_size_aprox;
		mcast_obj->set_registry_size =
			bnx2x_mcast_set_registry_size_aprox;
	} else {
		mcast_obj->config_mcast      = bnx2x_mcast_setup_e2;
		mcast_obj->enqueue_cmd       = bnx2x_mcast_enqueue_cmd;
		mcast_obj->hdl_restore       =
			bnx2x_mcast_handle_restore_cmd_e2;
		mcast_obj->check_pending     = bnx2x_mcast_check_pending;
		mcast_obj->max_cmd_len       = 16;
		mcast_obj->wait_comp         = bnx2x_mcast_wait;
		mcast_obj->set_one_rule      = bnx2x_mcast_set_one_rule_e2;
		mcast_obj->validate          = bnx2x_mcast_validate_e2;
		mcast_obj->revert            = bnx2x_mcast_revert_e2;
		mcast_obj->get_registry_size =
			bnx2x_mcast_get_registry_size_aprox;
		mcast_obj->set_registry_size =
			bnx2x_mcast_set_registry_size_aprox;
	}
}


static inline bool __atomic_add_ifless(atomic_t *v, int a, int u)
{
	int c, old;

	c = atomic_read(v);
	for (;;) {
		if (unlikely(c + a >= u))
			return false;

		old = atomic_cmpxchg((v), c, c + a);
		if (likely(old == c))
			break;
		c = old;
	}

	return true;
}

static inline bool __atomic_dec_ifmoe(atomic_t *v, int a, int u)
{
	int c, old;

	c = atomic_read(v);
	for (;;) {
		if (unlikely(c - a < u))
			return false;

		old = atomic_cmpxchg((v), c, c - a);
		if (likely(old == c))
			break;
		c = old;
	}

	return true;
}

static bool bnx2x_credit_pool_get(struct bnx2x_credit_pool_obj *o, int cnt)
{
	bool rc;

	smp_mb();
	rc = __atomic_dec_ifmoe(&o->credit, cnt, 0);
	smp_mb();

	return rc;
}

static bool bnx2x_credit_pool_put(struct bnx2x_credit_pool_obj *o, int cnt)
{
	bool rc;

	smp_mb();

	
	rc = __atomic_add_ifless(&o->credit, cnt, o->pool_sz + 1);

	smp_mb();

	return rc;
}

static int bnx2x_credit_pool_check(struct bnx2x_credit_pool_obj *o)
{
	int cur_credit;

	smp_mb();
	cur_credit = atomic_read(&o->credit);

	return cur_credit;
}

static bool bnx2x_credit_pool_always_true(struct bnx2x_credit_pool_obj *o,
					  int cnt)
{
	return true;
}


static bool bnx2x_credit_pool_get_entry(
	struct bnx2x_credit_pool_obj *o,
	int *offset)
{
	int idx, vec, i;

	*offset = -1;

	
	for (vec = 0; vec < BNX2X_POOL_VEC_SIZE; vec++) {

		
		if (!o->pool_mirror[vec])
			continue;

		
		for (idx = vec * BIT_VEC64_ELEM_SZ, i = 0;
		      i < BIT_VEC64_ELEM_SZ; idx++, i++)

			if (BIT_VEC64_TEST_BIT(o->pool_mirror, idx)) {
				
				BIT_VEC64_CLEAR_BIT(o->pool_mirror, idx);
				*offset = o->base_pool_offset + idx;
				return true;
			}
	}

	return false;
}

static bool bnx2x_credit_pool_put_entry(
	struct bnx2x_credit_pool_obj *o,
	int offset)
{
	if (offset < o->base_pool_offset)
		return false;

	offset -= o->base_pool_offset;

	if (offset >= o->pool_sz)
		return false;

	
	BIT_VEC64_SET_BIT(o->pool_mirror, offset);

	return true;
}

static bool bnx2x_credit_pool_put_entry_always_true(
	struct bnx2x_credit_pool_obj *o,
	int offset)
{
	return true;
}

static bool bnx2x_credit_pool_get_entry_always_true(
	struct bnx2x_credit_pool_obj *o,
	int *offset)
{
	*offset = -1;
	return true;
}
static inline void bnx2x_init_credit_pool(struct bnx2x_credit_pool_obj *p,
					  int base, int credit)
{
	
	memset(p, 0, sizeof(*p));

	
	memset(&p->pool_mirror, 0xff, sizeof(p->pool_mirror));

	
	atomic_set(&p->credit, credit);

	
	p->pool_sz = credit;

	p->base_pool_offset = base;

	
	smp_mb();

	p->check = bnx2x_credit_pool_check;

	
	if (credit >= 0) {
		p->put      = bnx2x_credit_pool_put;
		p->get      = bnx2x_credit_pool_get;
		p->put_entry = bnx2x_credit_pool_put_entry;
		p->get_entry = bnx2x_credit_pool_get_entry;
	} else {
		p->put      = bnx2x_credit_pool_always_true;
		p->get      = bnx2x_credit_pool_always_true;
		p->put_entry = bnx2x_credit_pool_put_entry_always_true;
		p->get_entry = bnx2x_credit_pool_get_entry_always_true;
	}

	
	if (base < 0) {
		p->put_entry = bnx2x_credit_pool_put_entry_always_true;
		p->get_entry = bnx2x_credit_pool_get_entry_always_true;
	}
}

void bnx2x_init_mac_credit_pool(struct bnx2x *bp,
				struct bnx2x_credit_pool_obj *p, u8 func_id,
				u8 func_num)
{
#define BNX2X_CAM_SIZE_EMUL 5

	int cam_sz;

	if (CHIP_IS_E1(bp)) {
		
		if (!CHIP_REV_IS_SLOW(bp))
			cam_sz = (MAX_MAC_CREDIT_E1 / 2) - BNX2X_MAX_MULTICAST;
		else
			cam_sz = BNX2X_CAM_SIZE_EMUL - BNX2X_MAX_EMUL_MULTI;

		bnx2x_init_credit_pool(p, func_id * cam_sz, cam_sz);

	} else if (CHIP_IS_E1H(bp)) {
		if ((func_num > 0)) {
			if (!CHIP_REV_IS_SLOW(bp))
				cam_sz = (MAX_MAC_CREDIT_E1H / (2*func_num));
			else
				cam_sz = BNX2X_CAM_SIZE_EMUL;
			bnx2x_init_credit_pool(p, func_id * cam_sz, cam_sz);
		} else {
			
			bnx2x_init_credit_pool(p, 0, 0);
		}

	} else {

		if ((func_num > 0)) {
			if (!CHIP_REV_IS_SLOW(bp))
				cam_sz = (MAX_MAC_CREDIT_E2 / func_num);
			else
				cam_sz = BNX2X_CAM_SIZE_EMUL;

			bnx2x_init_credit_pool(p, -1, cam_sz);
		} else {
			
			bnx2x_init_credit_pool(p, 0, 0);
		}

	}
}

void bnx2x_init_vlan_credit_pool(struct bnx2x *bp,
				 struct bnx2x_credit_pool_obj *p,
				 u8 func_id,
				 u8 func_num)
{
	if (CHIP_IS_E1x(bp)) {
		bnx2x_init_credit_pool(p, 0, -1);
	} else {
		if (func_num > 0) {
			int credit = MAX_VLAN_CREDIT_E2 / func_num;
			bnx2x_init_credit_pool(p, func_id * credit, credit);
		} else
			
			bnx2x_init_credit_pool(p, 0, 0);
	}
}

static inline void bnx2x_debug_print_ind_table(struct bnx2x *bp,
					struct bnx2x_config_rss_params *p)
{
	int i;

	DP(BNX2X_MSG_SP, "Setting indirection table to:\n");
	DP(BNX2X_MSG_SP, "0x0000: ");
	for (i = 0; i < T_ETH_INDIRECTION_TABLE_SIZE; i++) {
		DP_CONT(BNX2X_MSG_SP, "0x%02x ", p->ind_table[i]);

		
		if ((i + 1 < T_ETH_INDIRECTION_TABLE_SIZE) &&
		    (((i + 1) & 0x3) == 0)) {
			DP_CONT(BNX2X_MSG_SP, "\n");
			DP(BNX2X_MSG_SP, "0x%04x: ", i + 1);
		}
	}

	DP_CONT(BNX2X_MSG_SP, "\n");
}

static int bnx2x_setup_rss(struct bnx2x *bp,
			   struct bnx2x_config_rss_params *p)
{
	struct bnx2x_rss_config_obj *o = p->rss_obj;
	struct bnx2x_raw_obj *r = &o->raw;
	struct eth_rss_update_ramrod_data *data =
		(struct eth_rss_update_ramrod_data *)(r->rdata);
	u8 rss_mode = 0;
	int rc;

	memset(data, 0, sizeof(*data));

	DP(BNX2X_MSG_SP, "Configuring RSS\n");

	
	data->echo = (r->cid & BNX2X_SWCID_MASK) |
		     (r->state << BNX2X_SWCID_SHIFT);

	
	if (test_bit(BNX2X_RSS_MODE_DISABLED, &p->rss_flags))
		rss_mode = ETH_RSS_MODE_DISABLED;
	else if (test_bit(BNX2X_RSS_MODE_REGULAR, &p->rss_flags))
		rss_mode = ETH_RSS_MODE_REGULAR;
	else if (test_bit(BNX2X_RSS_MODE_VLAN_PRI, &p->rss_flags))
		rss_mode = ETH_RSS_MODE_VLAN_PRI;
	else if (test_bit(BNX2X_RSS_MODE_E1HOV_PRI, &p->rss_flags))
		rss_mode = ETH_RSS_MODE_E1HOV_PRI;
	else if (test_bit(BNX2X_RSS_MODE_IP_DSCP, &p->rss_flags))
		rss_mode = ETH_RSS_MODE_IP_DSCP;

	data->rss_mode = rss_mode;

	DP(BNX2X_MSG_SP, "rss_mode=%d\n", rss_mode);

	
	if (test_bit(BNX2X_RSS_IPV4, &p->rss_flags))
		data->capabilities |=
			ETH_RSS_UPDATE_RAMROD_DATA_IPV4_CAPABILITY;

	if (test_bit(BNX2X_RSS_IPV4_TCP, &p->rss_flags))
		data->capabilities |=
			ETH_RSS_UPDATE_RAMROD_DATA_IPV4_TCP_CAPABILITY;

	if (test_bit(BNX2X_RSS_IPV6, &p->rss_flags))
		data->capabilities |=
			ETH_RSS_UPDATE_RAMROD_DATA_IPV6_CAPABILITY;

	if (test_bit(BNX2X_RSS_IPV6_TCP, &p->rss_flags))
		data->capabilities |=
			ETH_RSS_UPDATE_RAMROD_DATA_IPV6_TCP_CAPABILITY;

	
	data->rss_result_mask = p->rss_result_mask;

	
	data->rss_engine_id = o->engine_id;

	DP(BNX2X_MSG_SP, "rss_engine_id=%d\n", data->rss_engine_id);

	
	memcpy(data->indirection_table, p->ind_table,
		  T_ETH_INDIRECTION_TABLE_SIZE);

	
	memcpy(o->ind_table, p->ind_table, T_ETH_INDIRECTION_TABLE_SIZE);

	
	if (netif_msg_ifup(bp))
		bnx2x_debug_print_ind_table(bp, p);

	
	if (test_bit(BNX2X_RSS_SET_SRCH, &p->rss_flags)) {
		memcpy(&data->rss_key[0], &p->rss_key[0],
		       sizeof(data->rss_key));
		data->capabilities |= ETH_RSS_UPDATE_RAMROD_DATA_UPDATE_RSS_KEY;
	}


	
	rc = bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_RSS_UPDATE, r->cid,
			   U64_HI(r->rdata_mapping),
			   U64_LO(r->rdata_mapping),
			   ETH_CONNECTION_TYPE);

	if (rc < 0)
		return rc;

	return 1;
}

void bnx2x_get_rss_ind_table(struct bnx2x_rss_config_obj *rss_obj,
			     u8 *ind_table)
{
	memcpy(ind_table, rss_obj->ind_table, sizeof(rss_obj->ind_table));
}

int bnx2x_config_rss(struct bnx2x *bp,
		     struct bnx2x_config_rss_params *p)
{
	int rc;
	struct bnx2x_rss_config_obj *o = p->rss_obj;
	struct bnx2x_raw_obj *r = &o->raw;

	
	if (test_bit(RAMROD_DRV_CLR_ONLY, &p->ramrod_flags))
		return 0;

	r->set_pending(r);

	rc = o->config_rss(bp, p);
	if (rc < 0) {
		r->clear_pending(r);
		return rc;
	}

	if (test_bit(RAMROD_COMP_WAIT, &p->ramrod_flags))
		rc = r->wait_comp(bp, r);

	return rc;
}


void bnx2x_init_rss_config_obj(struct bnx2x *bp,
			       struct bnx2x_rss_config_obj *rss_obj,
			       u8 cl_id, u32 cid, u8 func_id, u8 engine_id,
			       void *rdata, dma_addr_t rdata_mapping,
			       int state, unsigned long *pstate,
			       bnx2x_obj_type type)
{
	bnx2x_init_raw_obj(&rss_obj->raw, cl_id, cid, func_id, rdata,
			   rdata_mapping, state, pstate, type);

	rss_obj->engine_id  = engine_id;
	rss_obj->config_rss = bnx2x_setup_rss;
}


int bnx2x_queue_state_change(struct bnx2x *bp,
			     struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_sp_obj *o = params->q_obj;
	int rc, pending_bit;
	unsigned long *pending = &o->pending;

	
	if (o->check_transition(bp, o, params))
		return -EINVAL;

	
	pending_bit = o->set_pending(o, params);

	
	if (test_bit(RAMROD_DRV_CLR_ONLY, &params->ramrod_flags))
		o->complete_cmd(bp, o, pending_bit);
	else {
		
		rc = o->send_cmd(bp, params);
		if (rc) {
			o->next_state = BNX2X_Q_STATE_MAX;
			clear_bit(pending_bit, pending);
			smp_mb__after_clear_bit();
			return rc;
		}

		if (test_bit(RAMROD_COMP_WAIT, &params->ramrod_flags)) {
			rc = o->wait_comp(bp, o, pending_bit);
			if (rc)
				return rc;

			return 0;
		}
	}

	return !!test_bit(pending_bit, pending);
}


static int bnx2x_queue_set_pending(struct bnx2x_queue_sp_obj *obj,
				   struct bnx2x_queue_state_params *params)
{
	enum bnx2x_queue_cmd cmd = params->cmd, bit;

	if ((cmd == BNX2X_Q_CMD_ACTIVATE) ||
	    (cmd == BNX2X_Q_CMD_DEACTIVATE))
		bit = BNX2X_Q_CMD_UPDATE;
	else
		bit = cmd;

	set_bit(bit, &obj->pending);
	return bit;
}

static int bnx2x_queue_wait_comp(struct bnx2x *bp,
				 struct bnx2x_queue_sp_obj *o,
				 enum bnx2x_queue_cmd cmd)
{
	return bnx2x_state_wait(bp, cmd, &o->pending);
}

static int bnx2x_queue_comp_cmd(struct bnx2x *bp,
				struct bnx2x_queue_sp_obj *o,
				enum bnx2x_queue_cmd cmd)
{
	unsigned long cur_pending = o->pending;

	if (!test_and_clear_bit(cmd, &cur_pending)) {
		BNX2X_ERR("Bad MC reply %d for queue %d in state %d pending 0x%lx, next_state %d\n",
			  cmd, o->cids[BNX2X_PRIMARY_CID_INDEX],
			  o->state, cur_pending, o->next_state);
		return -EINVAL;
	}

	if (o->next_tx_only >= o->max_cos)
		BNX2X_ERR("illegal value for next tx_only: %d. max cos was %d",
			   o->next_tx_only, o->max_cos);

	DP(BNX2X_MSG_SP,
	   "Completing command %d for queue %d, setting state to %d\n",
	   cmd, o->cids[BNX2X_PRIMARY_CID_INDEX], o->next_state);

	if (o->next_tx_only)  
		DP(BNX2X_MSG_SP, "primary cid %d: num tx-only cons %d\n",
		   o->cids[BNX2X_PRIMARY_CID_INDEX], o->next_tx_only);

	o->state = o->next_state;
	o->num_tx_only = o->next_tx_only;
	o->next_state = BNX2X_Q_STATE_MAX;

	wmb();

	clear_bit(cmd, &o->pending);
	smp_mb__after_clear_bit();

	return 0;
}

static void bnx2x_q_fill_setup_data_e2(struct bnx2x *bp,
				struct bnx2x_queue_state_params *cmd_params,
				struct client_init_ramrod_data *data)
{
	struct bnx2x_queue_setup_params *params = &cmd_params->params.setup;

	

	
	data->rx.tpa_en |= test_bit(BNX2X_Q_FLG_TPA_IPV6, &params->flags) *
				CLIENT_INIT_RX_DATA_TPA_EN_IPV6;
}

static void bnx2x_q_fill_init_general_data(struct bnx2x *bp,
				struct bnx2x_queue_sp_obj *o,
				struct bnx2x_general_setup_params *params,
				struct client_init_general_data *gen_data,
				unsigned long *flags)
{
	gen_data->client_id = o->cl_id;

	if (test_bit(BNX2X_Q_FLG_STATS, flags)) {
		gen_data->statistics_counter_id =
					params->stat_id;
		gen_data->statistics_en_flg = 1;
		gen_data->statistics_zero_flg =
			test_bit(BNX2X_Q_FLG_ZERO_STATS, flags);
	} else
		gen_data->statistics_counter_id =
					DISABLE_STATISTIC_COUNTER_ID_VALUE;

	gen_data->is_fcoe_flg = test_bit(BNX2X_Q_FLG_FCOE, flags);
	gen_data->activate_flg = test_bit(BNX2X_Q_FLG_ACTIVE, flags);
	gen_data->sp_client_id = params->spcl_id;
	gen_data->mtu = cpu_to_le16(params->mtu);
	gen_data->func_id = o->func_id;


	gen_data->cos = params->cos;

	gen_data->traffic_type =
		test_bit(BNX2X_Q_FLG_FCOE, flags) ?
		LLFC_TRAFFIC_TYPE_FCOE : LLFC_TRAFFIC_TYPE_NW;

	DP(BNX2X_MSG_SP, "flags: active %d, cos %d, stats en %d\n",
	   gen_data->activate_flg, gen_data->cos, gen_data->statistics_en_flg);
}

static void bnx2x_q_fill_init_tx_data(struct bnx2x_queue_sp_obj *o,
				struct bnx2x_txq_setup_params *params,
				struct client_init_tx_data *tx_data,
				unsigned long *flags)
{
	tx_data->enforce_security_flg =
		test_bit(BNX2X_Q_FLG_TX_SEC, flags);
	tx_data->default_vlan =
		cpu_to_le16(params->default_vlan);
	tx_data->default_vlan_flg =
		test_bit(BNX2X_Q_FLG_DEF_VLAN, flags);
	tx_data->tx_switching_flg =
		test_bit(BNX2X_Q_FLG_TX_SWITCH, flags);
	tx_data->anti_spoofing_flg =
		test_bit(BNX2X_Q_FLG_ANTI_SPOOF, flags);
	tx_data->tx_status_block_id = params->fw_sb_id;
	tx_data->tx_sb_index_number = params->sb_cq_index;
	tx_data->tss_leading_client_id = params->tss_leading_cl_id;

	tx_data->tx_bd_page_base.lo =
		cpu_to_le32(U64_LO(params->dscr_map));
	tx_data->tx_bd_page_base.hi =
		cpu_to_le32(U64_HI(params->dscr_map));

	
	tx_data->state = 0;
}

static void bnx2x_q_fill_init_pause_data(struct bnx2x_queue_sp_obj *o,
				struct rxq_pause_params *params,
				struct client_init_rx_data *rx_data)
{
	
	rx_data->cqe_pause_thr_low = cpu_to_le16(params->rcq_th_lo);
	rx_data->cqe_pause_thr_high = cpu_to_le16(params->rcq_th_hi);
	rx_data->bd_pause_thr_low = cpu_to_le16(params->bd_th_lo);
	rx_data->bd_pause_thr_high = cpu_to_le16(params->bd_th_hi);
	rx_data->sge_pause_thr_low = cpu_to_le16(params->sge_th_lo);
	rx_data->sge_pause_thr_high = cpu_to_le16(params->sge_th_hi);
	rx_data->rx_cos_mask = cpu_to_le16(params->pri_map);
}

static void bnx2x_q_fill_init_rx_data(struct bnx2x_queue_sp_obj *o,
				struct bnx2x_rxq_setup_params *params,
				struct client_init_rx_data *rx_data,
				unsigned long *flags)
{
	rx_data->tpa_en = test_bit(BNX2X_Q_FLG_TPA, flags) *
				CLIENT_INIT_RX_DATA_TPA_EN_IPV4;
	rx_data->tpa_en |= test_bit(BNX2X_Q_FLG_TPA_GRO, flags) *
				CLIENT_INIT_RX_DATA_TPA_MODE;
	rx_data->vmqueue_mode_en_flg = 0;

	rx_data->cache_line_alignment_log_size =
		params->cache_line_log;
	rx_data->enable_dynamic_hc =
		test_bit(BNX2X_Q_FLG_DHC, flags);
	rx_data->max_sges_for_packet = params->max_sges_pkt;
	rx_data->client_qzone_id = params->cl_qzone_id;
	rx_data->max_agg_size = cpu_to_le16(params->tpa_agg_sz);

	
	rx_data->state = cpu_to_le16(CLIENT_INIT_RX_DATA_UCAST_DROP_ALL |
				     CLIENT_INIT_RX_DATA_MCAST_DROP_ALL);

	
	rx_data->drop_ip_cs_err_flg = 0;
	rx_data->drop_tcp_cs_err_flg = 0;
	rx_data->drop_ttl0_flg = 0;
	rx_data->drop_udp_cs_err_flg = 0;
	rx_data->inner_vlan_removal_enable_flg =
		test_bit(BNX2X_Q_FLG_VLAN, flags);
	rx_data->outer_vlan_removal_enable_flg =
		test_bit(BNX2X_Q_FLG_OV, flags);
	rx_data->status_block_id = params->fw_sb_id;
	rx_data->rx_sb_index_number = params->sb_cq_index;
	rx_data->max_tpa_queues = params->max_tpa_queues;
	rx_data->max_bytes_on_bd = cpu_to_le16(params->buf_sz);
	rx_data->sge_buff_size = cpu_to_le16(params->sge_buf_sz);
	rx_data->bd_page_base.lo =
		cpu_to_le32(U64_LO(params->dscr_map));
	rx_data->bd_page_base.hi =
		cpu_to_le32(U64_HI(params->dscr_map));
	rx_data->sge_page_base.lo =
		cpu_to_le32(U64_LO(params->sge_map));
	rx_data->sge_page_base.hi =
		cpu_to_le32(U64_HI(params->sge_map));
	rx_data->cqe_page_base.lo =
		cpu_to_le32(U64_LO(params->rcq_map));
	rx_data->cqe_page_base.hi =
		cpu_to_le32(U64_HI(params->rcq_map));
	rx_data->is_leading_rss = test_bit(BNX2X_Q_FLG_LEADING_RSS, flags);

	if (test_bit(BNX2X_Q_FLG_MCAST, flags)) {
		rx_data->approx_mcast_engine_id = params->mcast_engine_id;
		rx_data->is_approx_mcast = 1;
	}

	rx_data->rss_engine_id = params->rss_engine_id;

	
	rx_data->silent_vlan_removal_flg =
		test_bit(BNX2X_Q_FLG_SILENT_VLAN_REM, flags);
	rx_data->silent_vlan_value =
		cpu_to_le16(params->silent_removal_value);
	rx_data->silent_vlan_mask =
		cpu_to_le16(params->silent_removal_mask);

}

static void bnx2x_q_fill_setup_data_cmn(struct bnx2x *bp,
				struct bnx2x_queue_state_params *cmd_params,
				struct client_init_ramrod_data *data)
{
	bnx2x_q_fill_init_general_data(bp, cmd_params->q_obj,
				       &cmd_params->params.setup.gen_params,
				       &data->general,
				       &cmd_params->params.setup.flags);

	bnx2x_q_fill_init_tx_data(cmd_params->q_obj,
				  &cmd_params->params.setup.txq_params,
				  &data->tx,
				  &cmd_params->params.setup.flags);

	bnx2x_q_fill_init_rx_data(cmd_params->q_obj,
				  &cmd_params->params.setup.rxq_params,
				  &data->rx,
				  &cmd_params->params.setup.flags);

	bnx2x_q_fill_init_pause_data(cmd_params->q_obj,
				     &cmd_params->params.setup.pause_params,
				     &data->rx);
}

static void bnx2x_q_fill_setup_tx_only(struct bnx2x *bp,
				struct bnx2x_queue_state_params *cmd_params,
				struct tx_queue_init_ramrod_data *data)
{
	bnx2x_q_fill_init_general_data(bp, cmd_params->q_obj,
				       &cmd_params->params.tx_only.gen_params,
				       &data->general,
				       &cmd_params->params.tx_only.flags);

	bnx2x_q_fill_init_tx_data(cmd_params->q_obj,
				  &cmd_params->params.tx_only.txq_params,
				  &data->tx,
				  &cmd_params->params.tx_only.flags);

	DP(BNX2X_MSG_SP, "cid %d, tx bd page lo %x hi %x",
			 cmd_params->q_obj->cids[0],
			 data->tx.tx_bd_page_base.lo,
			 data->tx.tx_bd_page_base.hi);
}

static inline int bnx2x_q_init(struct bnx2x *bp,
			       struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_sp_obj *o = params->q_obj;
	struct bnx2x_queue_init_params *init = &params->params.init;
	u16 hc_usec;
	u8 cos;

	
	if (test_bit(BNX2X_Q_TYPE_HAS_TX, &o->type) &&
	    test_bit(BNX2X_Q_FLG_HC, &init->tx.flags)) {
		hc_usec = init->tx.hc_rate ? 1000000 / init->tx.hc_rate : 0;

		bnx2x_update_coalesce_sb_index(bp, init->tx.fw_sb_id,
			init->tx.sb_cq_index,
			!test_bit(BNX2X_Q_FLG_HC_EN, &init->tx.flags),
			hc_usec);
	}

	
	if (test_bit(BNX2X_Q_TYPE_HAS_RX, &o->type) &&
	    test_bit(BNX2X_Q_FLG_HC, &init->rx.flags)) {
		hc_usec = init->rx.hc_rate ? 1000000 / init->rx.hc_rate : 0;

		bnx2x_update_coalesce_sb_index(bp, init->rx.fw_sb_id,
			init->rx.sb_cq_index,
			!test_bit(BNX2X_Q_FLG_HC_EN, &init->rx.flags),
			hc_usec);
	}

	
	for (cos = 0; cos < o->max_cos; cos++) {
		DP(BNX2X_MSG_SP, "setting context validation. cid %d, cos %d\n",
				 o->cids[cos], cos);
		DP(BNX2X_MSG_SP, "context pointer %p\n", init->cxts[cos]);
		bnx2x_set_ctx_validation(bp, init->cxts[cos], o->cids[cos]);
	}

	
	o->complete_cmd(bp, o, BNX2X_Q_CMD_INIT);

	mmiowb();
	smp_mb();

	return 0;
}

static inline int bnx2x_q_send_setup_e1x(struct bnx2x *bp,
					struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_sp_obj *o = params->q_obj;
	struct client_init_ramrod_data *rdata =
		(struct client_init_ramrod_data *)o->rdata;
	dma_addr_t data_mapping = o->rdata_mapping;
	int ramrod = RAMROD_CMD_ID_ETH_CLIENT_SETUP;

	
	memset(rdata, 0, sizeof(*rdata));

	
	bnx2x_q_fill_setup_data_cmn(bp, params, rdata);


	return bnx2x_sp_post(bp, ramrod, o->cids[BNX2X_PRIMARY_CID_INDEX],
			     U64_HI(data_mapping),
			     U64_LO(data_mapping), ETH_CONNECTION_TYPE);
}

static inline int bnx2x_q_send_setup_e2(struct bnx2x *bp,
					struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_sp_obj *o = params->q_obj;
	struct client_init_ramrod_data *rdata =
		(struct client_init_ramrod_data *)o->rdata;
	dma_addr_t data_mapping = o->rdata_mapping;
	int ramrod = RAMROD_CMD_ID_ETH_CLIENT_SETUP;

	
	memset(rdata, 0, sizeof(*rdata));

	
	bnx2x_q_fill_setup_data_cmn(bp, params, rdata);
	bnx2x_q_fill_setup_data_e2(bp, params, rdata);


	return bnx2x_sp_post(bp, ramrod, o->cids[BNX2X_PRIMARY_CID_INDEX],
			     U64_HI(data_mapping),
			     U64_LO(data_mapping), ETH_CONNECTION_TYPE);
}

static inline int bnx2x_q_send_setup_tx_only(struct bnx2x *bp,
				  struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_sp_obj *o = params->q_obj;
	struct tx_queue_init_ramrod_data *rdata =
		(struct tx_queue_init_ramrod_data *)o->rdata;
	dma_addr_t data_mapping = o->rdata_mapping;
	int ramrod = RAMROD_CMD_ID_ETH_TX_QUEUE_SETUP;
	struct bnx2x_queue_setup_tx_only_params *tx_only_params =
		&params->params.tx_only;
	u8 cid_index = tx_only_params->cid_index;


	if (cid_index >= o->max_cos) {
		BNX2X_ERR("queue[%d]: cid_index (%d) is out of range\n",
			  o->cl_id, cid_index);
		return -EINVAL;
	}

	DP(BNX2X_MSG_SP, "parameters received: cos: %d sp-id: %d\n",
			 tx_only_params->gen_params.cos,
			 tx_only_params->gen_params.spcl_id);

	
	memset(rdata, 0, sizeof(*rdata));

	
	bnx2x_q_fill_setup_tx_only(bp, params, rdata);

	DP(BNX2X_MSG_SP, "sending tx-only ramrod: cid %d, client-id %d, sp-client id %d, cos %d\n",
			 o->cids[cid_index], rdata->general.client_id,
			 rdata->general.sp_client_id, rdata->general.cos);


	return bnx2x_sp_post(bp, ramrod, o->cids[cid_index],
			     U64_HI(data_mapping),
			     U64_LO(data_mapping), ETH_CONNECTION_TYPE);
}

static void bnx2x_q_fill_update_data(struct bnx2x *bp,
				     struct bnx2x_queue_sp_obj *obj,
				     struct bnx2x_queue_update_params *params,
				     struct client_update_ramrod_data *data)
{
	
	data->client_id = obj->cl_id;

	
	data->func_id = obj->func_id;

	
	data->default_vlan = cpu_to_le16(params->def_vlan);

	
	data->inner_vlan_removal_enable_flg =
		test_bit(BNX2X_Q_UPDATE_IN_VLAN_REM, &params->update_flags);
	data->inner_vlan_removal_change_flg =
		test_bit(BNX2X_Q_UPDATE_IN_VLAN_REM_CHNG,
			 &params->update_flags);

	
	data->outer_vlan_removal_enable_flg =
		test_bit(BNX2X_Q_UPDATE_OUT_VLAN_REM, &params->update_flags);
	data->outer_vlan_removal_change_flg =
		test_bit(BNX2X_Q_UPDATE_OUT_VLAN_REM_CHNG,
			 &params->update_flags);

	data->anti_spoofing_enable_flg =
		test_bit(BNX2X_Q_UPDATE_ANTI_SPOOF, &params->update_flags);
	data->anti_spoofing_change_flg =
		test_bit(BNX2X_Q_UPDATE_ANTI_SPOOF_CHNG, &params->update_flags);

	
	data->activate_flg =
		test_bit(BNX2X_Q_UPDATE_ACTIVATE, &params->update_flags);
	data->activate_change_flg =
		test_bit(BNX2X_Q_UPDATE_ACTIVATE_CHNG, &params->update_flags);

	
	data->default_vlan_enable_flg =
		test_bit(BNX2X_Q_UPDATE_DEF_VLAN_EN, &params->update_flags);
	data->default_vlan_change_flg =
		test_bit(BNX2X_Q_UPDATE_DEF_VLAN_EN_CHNG,
			 &params->update_flags);

	
	data->silent_vlan_change_flg =
		test_bit(BNX2X_Q_UPDATE_SILENT_VLAN_REM_CHNG,
			 &params->update_flags);
	data->silent_vlan_removal_flg =
		test_bit(BNX2X_Q_UPDATE_SILENT_VLAN_REM, &params->update_flags);
	data->silent_vlan_value = cpu_to_le16(params->silent_removal_value);
	data->silent_vlan_mask = cpu_to_le16(params->silent_removal_mask);
}

static inline int bnx2x_q_send_update(struct bnx2x *bp,
				      struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_sp_obj *o = params->q_obj;
	struct client_update_ramrod_data *rdata =
		(struct client_update_ramrod_data *)o->rdata;
	dma_addr_t data_mapping = o->rdata_mapping;
	struct bnx2x_queue_update_params *update_params =
		&params->params.update;
	u8 cid_index = update_params->cid_index;

	if (cid_index >= o->max_cos) {
		BNX2X_ERR("queue[%d]: cid_index (%d) is out of range\n",
			  o->cl_id, cid_index);
		return -EINVAL;
	}


	
	memset(rdata, 0, sizeof(*rdata));

	
	bnx2x_q_fill_update_data(bp, o, update_params, rdata);


	return bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_CLIENT_UPDATE,
			     o->cids[cid_index], U64_HI(data_mapping),
			     U64_LO(data_mapping), ETH_CONNECTION_TYPE);
}

static inline int bnx2x_q_send_deactivate(struct bnx2x *bp,
					struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_update_params *update = &params->params.update;

	memset(update, 0, sizeof(*update));

	__set_bit(BNX2X_Q_UPDATE_ACTIVATE_CHNG, &update->update_flags);

	return bnx2x_q_send_update(bp, params);
}

static inline int bnx2x_q_send_activate(struct bnx2x *bp,
					struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_update_params *update = &params->params.update;

	memset(update, 0, sizeof(*update));

	__set_bit(BNX2X_Q_UPDATE_ACTIVATE, &update->update_flags);
	__set_bit(BNX2X_Q_UPDATE_ACTIVATE_CHNG, &update->update_flags);

	return bnx2x_q_send_update(bp, params);
}

static inline int bnx2x_q_send_update_tpa(struct bnx2x *bp,
					struct bnx2x_queue_state_params *params)
{
	
	return -1;
}

static inline int bnx2x_q_send_halt(struct bnx2x *bp,
				    struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_sp_obj *o = params->q_obj;

	return bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_HALT,
			     o->cids[BNX2X_PRIMARY_CID_INDEX], 0, o->cl_id,
			     ETH_CONNECTION_TYPE);
}

static inline int bnx2x_q_send_cfc_del(struct bnx2x *bp,
				       struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_sp_obj *o = params->q_obj;
	u8 cid_idx = params->params.cfc_del.cid_index;

	if (cid_idx >= o->max_cos) {
		BNX2X_ERR("queue[%d]: cid_index (%d) is out of range\n",
			  o->cl_id, cid_idx);
		return -EINVAL;
	}

	return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_CFC_DEL,
			     o->cids[cid_idx], 0, 0, NONE_CONNECTION_TYPE);
}

static inline int bnx2x_q_send_terminate(struct bnx2x *bp,
					struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_sp_obj *o = params->q_obj;
	u8 cid_index = params->params.terminate.cid_index;

	if (cid_index >= o->max_cos) {
		BNX2X_ERR("queue[%d]: cid_index (%d) is out of range\n",
			  o->cl_id, cid_index);
		return -EINVAL;
	}

	return bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_TERMINATE,
			     o->cids[cid_index], 0, 0, ETH_CONNECTION_TYPE);
}

static inline int bnx2x_q_send_empty(struct bnx2x *bp,
				     struct bnx2x_queue_state_params *params)
{
	struct bnx2x_queue_sp_obj *o = params->q_obj;

	return bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_EMPTY,
			     o->cids[BNX2X_PRIMARY_CID_INDEX], 0, 0,
			     ETH_CONNECTION_TYPE);
}

static inline int bnx2x_queue_send_cmd_cmn(struct bnx2x *bp,
					struct bnx2x_queue_state_params *params)
{
	switch (params->cmd) {
	case BNX2X_Q_CMD_INIT:
		return bnx2x_q_init(bp, params);
	case BNX2X_Q_CMD_SETUP_TX_ONLY:
		return bnx2x_q_send_setup_tx_only(bp, params);
	case BNX2X_Q_CMD_DEACTIVATE:
		return bnx2x_q_send_deactivate(bp, params);
	case BNX2X_Q_CMD_ACTIVATE:
		return bnx2x_q_send_activate(bp, params);
	case BNX2X_Q_CMD_UPDATE:
		return bnx2x_q_send_update(bp, params);
	case BNX2X_Q_CMD_UPDATE_TPA:
		return bnx2x_q_send_update_tpa(bp, params);
	case BNX2X_Q_CMD_HALT:
		return bnx2x_q_send_halt(bp, params);
	case BNX2X_Q_CMD_CFC_DEL:
		return bnx2x_q_send_cfc_del(bp, params);
	case BNX2X_Q_CMD_TERMINATE:
		return bnx2x_q_send_terminate(bp, params);
	case BNX2X_Q_CMD_EMPTY:
		return bnx2x_q_send_empty(bp, params);
	default:
		BNX2X_ERR("Unknown command: %d\n", params->cmd);
		return -EINVAL;
	}
}

static int bnx2x_queue_send_cmd_e1x(struct bnx2x *bp,
				    struct bnx2x_queue_state_params *params)
{
	switch (params->cmd) {
	case BNX2X_Q_CMD_SETUP:
		return bnx2x_q_send_setup_e1x(bp, params);
	case BNX2X_Q_CMD_INIT:
	case BNX2X_Q_CMD_SETUP_TX_ONLY:
	case BNX2X_Q_CMD_DEACTIVATE:
	case BNX2X_Q_CMD_ACTIVATE:
	case BNX2X_Q_CMD_UPDATE:
	case BNX2X_Q_CMD_UPDATE_TPA:
	case BNX2X_Q_CMD_HALT:
	case BNX2X_Q_CMD_CFC_DEL:
	case BNX2X_Q_CMD_TERMINATE:
	case BNX2X_Q_CMD_EMPTY:
		return bnx2x_queue_send_cmd_cmn(bp, params);
	default:
		BNX2X_ERR("Unknown command: %d\n", params->cmd);
		return -EINVAL;
	}
}

static int bnx2x_queue_send_cmd_e2(struct bnx2x *bp,
				   struct bnx2x_queue_state_params *params)
{
	switch (params->cmd) {
	case BNX2X_Q_CMD_SETUP:
		return bnx2x_q_send_setup_e2(bp, params);
	case BNX2X_Q_CMD_INIT:
	case BNX2X_Q_CMD_SETUP_TX_ONLY:
	case BNX2X_Q_CMD_DEACTIVATE:
	case BNX2X_Q_CMD_ACTIVATE:
	case BNX2X_Q_CMD_UPDATE:
	case BNX2X_Q_CMD_UPDATE_TPA:
	case BNX2X_Q_CMD_HALT:
	case BNX2X_Q_CMD_CFC_DEL:
	case BNX2X_Q_CMD_TERMINATE:
	case BNX2X_Q_CMD_EMPTY:
		return bnx2x_queue_send_cmd_cmn(bp, params);
	default:
		BNX2X_ERR("Unknown command: %d\n", params->cmd);
		return -EINVAL;
	}
}

static int bnx2x_queue_chk_transition(struct bnx2x *bp,
				      struct bnx2x_queue_sp_obj *o,
				      struct bnx2x_queue_state_params *params)
{
	enum bnx2x_q_state state = o->state, next_state = BNX2X_Q_STATE_MAX;
	enum bnx2x_queue_cmd cmd = params->cmd;
	struct bnx2x_queue_update_params *update_params =
		 &params->params.update;
	u8 next_tx_only = o->num_tx_only;

	if (test_bit(RAMROD_DRV_CLR_ONLY, &params->ramrod_flags)) {
		o->pending = 0;
		o->next_state = BNX2X_Q_STATE_MAX;
	}

	if (o->pending)
		return -EBUSY;

	switch (state) {
	case BNX2X_Q_STATE_RESET:
		if (cmd == BNX2X_Q_CMD_INIT)
			next_state = BNX2X_Q_STATE_INITIALIZED;

		break;
	case BNX2X_Q_STATE_INITIALIZED:
		if (cmd == BNX2X_Q_CMD_SETUP) {
			if (test_bit(BNX2X_Q_FLG_ACTIVE,
				     &params->params.setup.flags))
				next_state = BNX2X_Q_STATE_ACTIVE;
			else
				next_state = BNX2X_Q_STATE_INACTIVE;
		}

		break;
	case BNX2X_Q_STATE_ACTIVE:
		if (cmd == BNX2X_Q_CMD_DEACTIVATE)
			next_state = BNX2X_Q_STATE_INACTIVE;

		else if ((cmd == BNX2X_Q_CMD_EMPTY) ||
			 (cmd == BNX2X_Q_CMD_UPDATE_TPA))
			next_state = BNX2X_Q_STATE_ACTIVE;

		else if (cmd == BNX2X_Q_CMD_SETUP_TX_ONLY) {
			next_state = BNX2X_Q_STATE_MULTI_COS;
			next_tx_only = 1;
		}

		else if (cmd == BNX2X_Q_CMD_HALT)
			next_state = BNX2X_Q_STATE_STOPPED;

		else if (cmd == BNX2X_Q_CMD_UPDATE) {
			if (test_bit(BNX2X_Q_UPDATE_ACTIVATE_CHNG,
				     &update_params->update_flags) &&
			    !test_bit(BNX2X_Q_UPDATE_ACTIVATE,
				      &update_params->update_flags))
				next_state = BNX2X_Q_STATE_INACTIVE;
			else
				next_state = BNX2X_Q_STATE_ACTIVE;
		}

		break;
	case BNX2X_Q_STATE_MULTI_COS:
		if (cmd == BNX2X_Q_CMD_TERMINATE)
			next_state = BNX2X_Q_STATE_MCOS_TERMINATED;

		else if (cmd == BNX2X_Q_CMD_SETUP_TX_ONLY) {
			next_state = BNX2X_Q_STATE_MULTI_COS;
			next_tx_only = o->num_tx_only + 1;
		}

		else if ((cmd == BNX2X_Q_CMD_EMPTY) ||
			 (cmd == BNX2X_Q_CMD_UPDATE_TPA))
			next_state = BNX2X_Q_STATE_MULTI_COS;

		else if (cmd == BNX2X_Q_CMD_UPDATE) {
			if (test_bit(BNX2X_Q_UPDATE_ACTIVATE_CHNG,
				     &update_params->update_flags) &&
			    !test_bit(BNX2X_Q_UPDATE_ACTIVATE,
				      &update_params->update_flags))
				next_state = BNX2X_Q_STATE_INACTIVE;
			else
				next_state = BNX2X_Q_STATE_MULTI_COS;
		}

		break;
	case BNX2X_Q_STATE_MCOS_TERMINATED:
		if (cmd == BNX2X_Q_CMD_CFC_DEL) {
			next_tx_only = o->num_tx_only - 1;
			if (next_tx_only == 0)
				next_state = BNX2X_Q_STATE_ACTIVE;
			else
				next_state = BNX2X_Q_STATE_MULTI_COS;
		}

		break;
	case BNX2X_Q_STATE_INACTIVE:
		if (cmd == BNX2X_Q_CMD_ACTIVATE)
			next_state = BNX2X_Q_STATE_ACTIVE;

		else if ((cmd == BNX2X_Q_CMD_EMPTY) ||
			 (cmd == BNX2X_Q_CMD_UPDATE_TPA))
			next_state = BNX2X_Q_STATE_INACTIVE;

		else if (cmd == BNX2X_Q_CMD_HALT)
			next_state = BNX2X_Q_STATE_STOPPED;

		else if (cmd == BNX2X_Q_CMD_UPDATE) {
			if (test_bit(BNX2X_Q_UPDATE_ACTIVATE_CHNG,
				     &update_params->update_flags) &&
			    test_bit(BNX2X_Q_UPDATE_ACTIVATE,
				     &update_params->update_flags)){
				if (o->num_tx_only == 0)
					next_state = BNX2X_Q_STATE_ACTIVE;
				else 
					next_state = BNX2X_Q_STATE_MULTI_COS;
			} else
				next_state = BNX2X_Q_STATE_INACTIVE;
		}

		break;
	case BNX2X_Q_STATE_STOPPED:
		if (cmd == BNX2X_Q_CMD_TERMINATE)
			next_state = BNX2X_Q_STATE_TERMINATED;

		break;
	case BNX2X_Q_STATE_TERMINATED:
		if (cmd == BNX2X_Q_CMD_CFC_DEL)
			next_state = BNX2X_Q_STATE_RESET;

		break;
	default:
		BNX2X_ERR("Illegal state: %d\n", state);
	}

	
	if (next_state != BNX2X_Q_STATE_MAX) {
		DP(BNX2X_MSG_SP, "Good state transition: %d(%d)->%d\n",
				 state, cmd, next_state);
		o->next_state = next_state;
		o->next_tx_only = next_tx_only;
		return 0;
	}

	DP(BNX2X_MSG_SP, "Bad state transition request: %d %d\n", state, cmd);

	return -EINVAL;
}

void bnx2x_init_queue_obj(struct bnx2x *bp,
			  struct bnx2x_queue_sp_obj *obj,
			  u8 cl_id, u32 *cids, u8 cid_cnt, u8 func_id,
			  void *rdata,
			  dma_addr_t rdata_mapping, unsigned long type)
{
	memset(obj, 0, sizeof(*obj));

	
	BUG_ON(BNX2X_MULTI_TX_COS < cid_cnt);

	memcpy(obj->cids, cids, sizeof(obj->cids[0]) * cid_cnt);
	obj->max_cos = cid_cnt;
	obj->cl_id = cl_id;
	obj->func_id = func_id;
	obj->rdata = rdata;
	obj->rdata_mapping = rdata_mapping;
	obj->type = type;
	obj->next_state = BNX2X_Q_STATE_MAX;

	if (CHIP_IS_E1x(bp))
		obj->send_cmd = bnx2x_queue_send_cmd_e1x;
	else
		obj->send_cmd = bnx2x_queue_send_cmd_e2;

	obj->check_transition = bnx2x_queue_chk_transition;

	obj->complete_cmd = bnx2x_queue_comp_cmd;
	obj->wait_comp = bnx2x_queue_wait_comp;
	obj->set_pending = bnx2x_queue_set_pending;
}

enum bnx2x_func_state bnx2x_func_get_state(struct bnx2x *bp,
					   struct bnx2x_func_sp_obj *o)
{
	
	if (o->pending)
		return BNX2X_F_STATE_MAX;

	rmb();

	return o->state;
}

static int bnx2x_func_wait_comp(struct bnx2x *bp,
				struct bnx2x_func_sp_obj *o,
				enum bnx2x_func_cmd cmd)
{
	return bnx2x_state_wait(bp, cmd, &o->pending);
}

static inline int bnx2x_func_state_change_comp(struct bnx2x *bp,
					       struct bnx2x_func_sp_obj *o,
					       enum bnx2x_func_cmd cmd)
{
	unsigned long cur_pending = o->pending;

	if (!test_and_clear_bit(cmd, &cur_pending)) {
		BNX2X_ERR("Bad MC reply %d for func %d in state %d pending 0x%lx, next_state %d\n",
			  cmd, BP_FUNC(bp), o->state,
			  cur_pending, o->next_state);
		return -EINVAL;
	}

	DP(BNX2X_MSG_SP,
	   "Completing command %d for func %d, setting state to %d\n",
	   cmd, BP_FUNC(bp), o->next_state);

	o->state = o->next_state;
	o->next_state = BNX2X_F_STATE_MAX;

	wmb();

	clear_bit(cmd, &o->pending);
	smp_mb__after_clear_bit();

	return 0;
}

static int bnx2x_func_comp_cmd(struct bnx2x *bp,
			       struct bnx2x_func_sp_obj *o,
			       enum bnx2x_func_cmd cmd)
{
	int rc = bnx2x_func_state_change_comp(bp, o, cmd);
	return rc;
}

static int bnx2x_func_chk_transition(struct bnx2x *bp,
				     struct bnx2x_func_sp_obj *o,
				     struct bnx2x_func_state_params *params)
{
	enum bnx2x_func_state state = o->state, next_state = BNX2X_F_STATE_MAX;
	enum bnx2x_func_cmd cmd = params->cmd;

	if (test_bit(RAMROD_DRV_CLR_ONLY, &params->ramrod_flags)) {
		o->pending = 0;
		o->next_state = BNX2X_F_STATE_MAX;
	}

	if (o->pending)
		return -EBUSY;

	switch (state) {
	case BNX2X_F_STATE_RESET:
		if (cmd == BNX2X_F_CMD_HW_INIT)
			next_state = BNX2X_F_STATE_INITIALIZED;

		break;
	case BNX2X_F_STATE_INITIALIZED:
		if (cmd == BNX2X_F_CMD_START)
			next_state = BNX2X_F_STATE_STARTED;

		else if (cmd == BNX2X_F_CMD_HW_RESET)
			next_state = BNX2X_F_STATE_RESET;

		break;
	case BNX2X_F_STATE_STARTED:
		if (cmd == BNX2X_F_CMD_STOP)
			next_state = BNX2X_F_STATE_INITIALIZED;
		else if (cmd == BNX2X_F_CMD_TX_STOP)
			next_state = BNX2X_F_STATE_TX_STOPPED;

		break;
	case BNX2X_F_STATE_TX_STOPPED:
		if (cmd == BNX2X_F_CMD_TX_START)
			next_state = BNX2X_F_STATE_STARTED;

		break;
	default:
		BNX2X_ERR("Unknown state: %d\n", state);
	}

	
	if (next_state != BNX2X_F_STATE_MAX) {
		DP(BNX2X_MSG_SP, "Good function state transition: %d(%d)->%d\n",
				 state, cmd, next_state);
		o->next_state = next_state;
		return 0;
	}

	DP(BNX2X_MSG_SP, "Bad function state transition request: %d %d\n",
			 state, cmd);

	return -EINVAL;
}

static inline int bnx2x_func_init_func(struct bnx2x *bp,
				       const struct bnx2x_func_sp_drv_ops *drv)
{
	return drv->init_hw_func(bp);
}

static inline int bnx2x_func_init_port(struct bnx2x *bp,
				       const struct bnx2x_func_sp_drv_ops *drv)
{
	int rc = drv->init_hw_port(bp);
	if (rc)
		return rc;

	return bnx2x_func_init_func(bp, drv);
}

static inline int bnx2x_func_init_cmn_chip(struct bnx2x *bp,
					const struct bnx2x_func_sp_drv_ops *drv)
{
	int rc = drv->init_hw_cmn_chip(bp);
	if (rc)
		return rc;

	return bnx2x_func_init_port(bp, drv);
}

static inline int bnx2x_func_init_cmn(struct bnx2x *bp,
				      const struct bnx2x_func_sp_drv_ops *drv)
{
	int rc = drv->init_hw_cmn(bp);
	if (rc)
		return rc;

	return bnx2x_func_init_port(bp, drv);
}

static int bnx2x_func_hw_init(struct bnx2x *bp,
			      struct bnx2x_func_state_params *params)
{
	u32 load_code = params->params.hw_init.load_phase;
	struct bnx2x_func_sp_obj *o = params->f_obj;
	const struct bnx2x_func_sp_drv_ops *drv = o->drv;
	int rc = 0;

	DP(BNX2X_MSG_SP, "function %d  load_code %x\n",
			 BP_ABS_FUNC(bp), load_code);

	
	rc = drv->gunzip_init(bp);
	if (rc)
		return rc;

	
	rc = drv->init_fw(bp);
	if (rc) {
		BNX2X_ERR("Error loading firmware\n");
		goto init_err;
	}

	
	switch (load_code) {
	case FW_MSG_CODE_DRV_LOAD_COMMON_CHIP:
		rc = bnx2x_func_init_cmn_chip(bp, drv);
		if (rc)
			goto init_err;

		break;
	case FW_MSG_CODE_DRV_LOAD_COMMON:
		rc = bnx2x_func_init_cmn(bp, drv);
		if (rc)
			goto init_err;

		break;
	case FW_MSG_CODE_DRV_LOAD_PORT:
		rc = bnx2x_func_init_port(bp, drv);
		if (rc)
			goto init_err;

		break;
	case FW_MSG_CODE_DRV_LOAD_FUNCTION:
		rc = bnx2x_func_init_func(bp, drv);
		if (rc)
			goto init_err;

		break;
	default:
		BNX2X_ERR("Unknown load_code (0x%x) from MCP\n", load_code);
		rc = -EINVAL;
	}

init_err:
	drv->gunzip_end(bp);

	if (!rc)
		o->complete_cmd(bp, o, BNX2X_F_CMD_HW_INIT);

	return rc;
}

static inline void bnx2x_func_reset_func(struct bnx2x *bp,
					const struct bnx2x_func_sp_drv_ops *drv)
{
	drv->reset_hw_func(bp);
}

static inline void bnx2x_func_reset_port(struct bnx2x *bp,
					const struct bnx2x_func_sp_drv_ops *drv)
{
	drv->reset_hw_port(bp);
	bnx2x_func_reset_func(bp, drv);
}

static inline void bnx2x_func_reset_cmn(struct bnx2x *bp,
					const struct bnx2x_func_sp_drv_ops *drv)
{
	bnx2x_func_reset_port(bp, drv);
	drv->reset_hw_cmn(bp);
}


static inline int bnx2x_func_hw_reset(struct bnx2x *bp,
				      struct bnx2x_func_state_params *params)
{
	u32 reset_phase = params->params.hw_reset.reset_phase;
	struct bnx2x_func_sp_obj *o = params->f_obj;
	const struct bnx2x_func_sp_drv_ops *drv = o->drv;

	DP(BNX2X_MSG_SP, "function %d  reset_phase %x\n", BP_ABS_FUNC(bp),
			 reset_phase);

	switch (reset_phase) {
	case FW_MSG_CODE_DRV_UNLOAD_COMMON:
		bnx2x_func_reset_cmn(bp, drv);
		break;
	case FW_MSG_CODE_DRV_UNLOAD_PORT:
		bnx2x_func_reset_port(bp, drv);
		break;
	case FW_MSG_CODE_DRV_UNLOAD_FUNCTION:
		bnx2x_func_reset_func(bp, drv);
		break;
	default:
		BNX2X_ERR("Unknown reset_phase (0x%x) from MCP\n",
			   reset_phase);
		break;
	}

	
	o->complete_cmd(bp, o, BNX2X_F_CMD_HW_RESET);

	return 0;
}

static inline int bnx2x_func_send_start(struct bnx2x *bp,
					struct bnx2x_func_state_params *params)
{
	struct bnx2x_func_sp_obj *o = params->f_obj;
	struct function_start_data *rdata =
		(struct function_start_data *)o->rdata;
	dma_addr_t data_mapping = o->rdata_mapping;
	struct bnx2x_func_start_params *start_params = &params->params.start;

	memset(rdata, 0, sizeof(*rdata));

	
	rdata->function_mode = cpu_to_le16(start_params->mf_mode);
	rdata->sd_vlan_tag   = cpu_to_le16(start_params->sd_vlan_tag);
	rdata->path_id       = BP_PATH(bp);
	rdata->network_cos_mode = start_params->network_cos_mode;


	return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_FUNCTION_START, 0,
			     U64_HI(data_mapping),
			     U64_LO(data_mapping), NONE_CONNECTION_TYPE);
}

static inline int bnx2x_func_send_stop(struct bnx2x *bp,
				       struct bnx2x_func_state_params *params)
{
	return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_FUNCTION_STOP, 0, 0, 0,
			     NONE_CONNECTION_TYPE);
}

static inline int bnx2x_func_send_tx_stop(struct bnx2x *bp,
				       struct bnx2x_func_state_params *params)
{
	return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_STOP_TRAFFIC, 0, 0, 0,
			     NONE_CONNECTION_TYPE);
}
static inline int bnx2x_func_send_tx_start(struct bnx2x *bp,
				       struct bnx2x_func_state_params *params)
{
	struct bnx2x_func_sp_obj *o = params->f_obj;
	struct flow_control_configuration *rdata =
		(struct flow_control_configuration *)o->rdata;
	dma_addr_t data_mapping = o->rdata_mapping;
	struct bnx2x_func_tx_start_params *tx_start_params =
		&params->params.tx_start;
	int i;

	memset(rdata, 0, sizeof(*rdata));

	rdata->dcb_enabled = tx_start_params->dcb_enabled;
	rdata->dcb_version = tx_start_params->dcb_version;
	rdata->dont_add_pri_0_en = tx_start_params->dont_add_pri_0_en;

	for (i = 0; i < ARRAY_SIZE(rdata->traffic_type_to_priority_cos); i++)
		rdata->traffic_type_to_priority_cos[i] =
			tx_start_params->traffic_type_to_priority_cos[i];

	return bnx2x_sp_post(bp, RAMROD_CMD_ID_COMMON_START_TRAFFIC, 0,
			     U64_HI(data_mapping),
			     U64_LO(data_mapping), NONE_CONNECTION_TYPE);
}

static int bnx2x_func_send_cmd(struct bnx2x *bp,
			       struct bnx2x_func_state_params *params)
{
	switch (params->cmd) {
	case BNX2X_F_CMD_HW_INIT:
		return bnx2x_func_hw_init(bp, params);
	case BNX2X_F_CMD_START:
		return bnx2x_func_send_start(bp, params);
	case BNX2X_F_CMD_STOP:
		return bnx2x_func_send_stop(bp, params);
	case BNX2X_F_CMD_HW_RESET:
		return bnx2x_func_hw_reset(bp, params);
	case BNX2X_F_CMD_TX_STOP:
		return bnx2x_func_send_tx_stop(bp, params);
	case BNX2X_F_CMD_TX_START:
		return bnx2x_func_send_tx_start(bp, params);
	default:
		BNX2X_ERR("Unknown command: %d\n", params->cmd);
		return -EINVAL;
	}
}

void bnx2x_init_func_obj(struct bnx2x *bp,
			 struct bnx2x_func_sp_obj *obj,
			 void *rdata, dma_addr_t rdata_mapping,
			 struct bnx2x_func_sp_drv_ops *drv_iface)
{
	memset(obj, 0, sizeof(*obj));

	mutex_init(&obj->one_pending_mutex);

	obj->rdata = rdata;
	obj->rdata_mapping = rdata_mapping;

	obj->send_cmd = bnx2x_func_send_cmd;
	obj->check_transition = bnx2x_func_chk_transition;
	obj->complete_cmd = bnx2x_func_comp_cmd;
	obj->wait_comp = bnx2x_func_wait_comp;

	obj->drv = drv_iface;
}

int bnx2x_func_state_change(struct bnx2x *bp,
			    struct bnx2x_func_state_params *params)
{
	struct bnx2x_func_sp_obj *o = params->f_obj;
	int rc;
	enum bnx2x_func_cmd cmd = params->cmd;
	unsigned long *pending = &o->pending;

	mutex_lock(&o->one_pending_mutex);

	
	if (o->check_transition(bp, o, params)) {
		mutex_unlock(&o->one_pending_mutex);
		return -EINVAL;
	}

	
	set_bit(cmd, pending);

	
	if (test_bit(RAMROD_DRV_CLR_ONLY, &params->ramrod_flags)) {
		bnx2x_func_state_change_comp(bp, o, cmd);
		mutex_unlock(&o->one_pending_mutex);
	} else {
		
		rc = o->send_cmd(bp, params);

		mutex_unlock(&o->one_pending_mutex);

		if (rc) {
			o->next_state = BNX2X_F_STATE_MAX;
			clear_bit(cmd, pending);
			smp_mb__after_clear_bit();
			return rc;
		}

		if (test_bit(RAMROD_COMP_WAIT, &params->ramrod_flags)) {
			rc = o->wait_comp(bp, o, cmd);
			if (rc)
				return rc;

			return 0;
		}
	}

	return !!test_bit(cmd, pending);
}
