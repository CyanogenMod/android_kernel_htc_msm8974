/*
 * Copyright (c) 2010 Cisco Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <generated/utsrelease.h>
#include <linux/utsname.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/configfs.h>
#include <linux/ctype.h>
#include <linux/hash.h>
#include <linux/rcupdate.h>
#include <linux/rculist.h>
#include <linux/kref.h>
#include <asm/unaligned.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/libfc.h>

#include <target/target_core_base.h>
#include <target/target_core_fabric.h>
#include <target/target_core_configfs.h>
#include <target/configfs_macros.h>

#include "tcm_fc.h"

static void ft_sess_delete_all(struct ft_tport *);

static struct ft_tport *ft_tport_create(struct fc_lport *lport)
{
	struct ft_tpg *tpg;
	struct ft_tport *tport;
	int i;

	tport = rcu_dereference(lport->prov[FC_TYPE_FCP]);
	if (tport && tport->tpg)
		return tport;

	tpg = ft_lport_find_tpg(lport);
	if (!tpg)
		return NULL;

	if (tport) {
		tport->tpg = tpg;
		return tport;
	}

	tport = kzalloc(sizeof(*tport), GFP_KERNEL);
	if (!tport)
		return NULL;

	tport->lport = lport;
	tport->tpg = tpg;
	tpg->tport = tport;
	for (i = 0; i < FT_SESS_HASH_SIZE; i++)
		INIT_HLIST_HEAD(&tport->hash[i]);

	rcu_assign_pointer(lport->prov[FC_TYPE_FCP], tport);
	return tport;
}

static void ft_tport_delete(struct ft_tport *tport)
{
	struct fc_lport *lport;
	struct ft_tpg *tpg;

	ft_sess_delete_all(tport);
	lport = tport->lport;
	BUG_ON(tport != lport->prov[FC_TYPE_FCP]);
	rcu_assign_pointer(lport->prov[FC_TYPE_FCP], NULL);

	tpg = tport->tpg;
	if (tpg) {
		tpg->tport = NULL;
		tport->tpg = NULL;
	}
	kfree_rcu(tport, rcu);
}

void ft_lport_add(struct fc_lport *lport, void *arg)
{
	mutex_lock(&ft_lport_lock);
	ft_tport_create(lport);
	mutex_unlock(&ft_lport_lock);
}

void ft_lport_del(struct fc_lport *lport, void *arg)
{
	struct ft_tport *tport;

	mutex_lock(&ft_lport_lock);
	tport = lport->prov[FC_TYPE_FCP];
	if (tport)
		ft_tport_delete(tport);
	mutex_unlock(&ft_lport_lock);
}

int ft_lport_notify(struct notifier_block *nb, unsigned long event, void *arg)
{
	struct fc_lport *lport = arg;

	switch (event) {
	case FC_LPORT_EV_ADD:
		ft_lport_add(lport, NULL);
		break;
	case FC_LPORT_EV_DEL:
		ft_lport_del(lport, NULL);
		break;
	}
	return NOTIFY_DONE;
}

static u32 ft_sess_hash(u32 port_id)
{
	return hash_32(port_id, FT_SESS_HASH_BITS);
}

static struct ft_sess *ft_sess_get(struct fc_lport *lport, u32 port_id)
{
	struct ft_tport *tport;
	struct hlist_head *head;
	struct hlist_node *pos;
	struct ft_sess *sess;

	rcu_read_lock();
	tport = rcu_dereference(lport->prov[FC_TYPE_FCP]);
	if (!tport)
		goto out;

	head = &tport->hash[ft_sess_hash(port_id)];
	hlist_for_each_entry_rcu(sess, pos, head, hash) {
		if (sess->port_id == port_id) {
			kref_get(&sess->kref);
			rcu_read_unlock();
			pr_debug("port_id %x found %p\n", port_id, sess);
			return sess;
		}
	}
out:
	rcu_read_unlock();
	pr_debug("port_id %x not found\n", port_id);
	return NULL;
}

static struct ft_sess *ft_sess_create(struct ft_tport *tport, u32 port_id,
				      struct ft_node_acl *acl)
{
	struct ft_sess *sess;
	struct hlist_head *head;
	struct hlist_node *pos;

	head = &tport->hash[ft_sess_hash(port_id)];
	hlist_for_each_entry_rcu(sess, pos, head, hash)
		if (sess->port_id == port_id)
			return sess;

	sess = kzalloc(sizeof(*sess), GFP_KERNEL);
	if (!sess)
		return NULL;

	sess->se_sess = transport_init_session();
	if (IS_ERR(sess->se_sess)) {
		kfree(sess);
		return NULL;
	}
	sess->se_sess->se_node_acl = &acl->se_node_acl;
	sess->tport = tport;
	sess->port_id = port_id;
	kref_init(&sess->kref);	
	hlist_add_head_rcu(&sess->hash, head);
	tport->sess_count++;

	pr_debug("port_id %x sess %p\n", port_id, sess);

	transport_register_session(&tport->tpg->se_tpg, &acl->se_node_acl,
				   sess->se_sess, sess);
	return sess;
}

static void ft_sess_unhash(struct ft_sess *sess)
{
	struct ft_tport *tport = sess->tport;

	hlist_del_rcu(&sess->hash);
	BUG_ON(!tport->sess_count);
	tport->sess_count--;
	sess->port_id = -1;
	sess->params = 0;
}

static struct ft_sess *ft_sess_delete(struct ft_tport *tport, u32 port_id)
{
	struct hlist_head *head;
	struct hlist_node *pos;
	struct ft_sess *sess;

	head = &tport->hash[ft_sess_hash(port_id)];
	hlist_for_each_entry_rcu(sess, pos, head, hash) {
		if (sess->port_id == port_id) {
			ft_sess_unhash(sess);
			return sess;
		}
	}
	return NULL;
}

static void ft_sess_delete_all(struct ft_tport *tport)
{
	struct hlist_head *head;
	struct hlist_node *pos;
	struct ft_sess *sess;

	for (head = tport->hash;
	     head < &tport->hash[FT_SESS_HASH_SIZE]; head++) {
		hlist_for_each_entry_rcu(sess, pos, head, hash) {
			ft_sess_unhash(sess);
			transport_deregister_session_configfs(sess->se_sess);
			ft_sess_put(sess);	
		}
	}
}


int ft_sess_shutdown(struct se_session *se_sess)
{
	struct ft_sess *sess = se_sess->fabric_sess_ptr;

	pr_debug("port_id %x\n", sess->port_id);
	return 1;
}

void ft_sess_close(struct se_session *se_sess)
{
	struct ft_sess *sess = se_sess->fabric_sess_ptr;
	u32 port_id;

	mutex_lock(&ft_lport_lock);
	port_id = sess->port_id;
	if (port_id == -1) {
		mutex_unlock(&ft_lport_lock);
		return;
	}
	pr_debug("port_id %x\n", port_id);
	ft_sess_unhash(sess);
	mutex_unlock(&ft_lport_lock);
	transport_deregister_session_configfs(se_sess);
	ft_sess_put(sess);
	
	synchronize_rcu();		
}

u32 ft_sess_get_index(struct se_session *se_sess)
{
	struct ft_sess *sess = se_sess->fabric_sess_ptr;

	return sess->port_id;	
}

u32 ft_sess_get_port_name(struct se_session *se_sess,
			  unsigned char *buf, u32 len)
{
	struct ft_sess *sess = se_sess->fabric_sess_ptr;

	return ft_format_wwn(buf, len, sess->port_name);
}


static int ft_prli_locked(struct fc_rport_priv *rdata, u32 spp_len,
			  const struct fc_els_spp *rspp, struct fc_els_spp *spp)
{
	struct ft_tport *tport;
	struct ft_sess *sess;
	struct ft_node_acl *acl;
	u32 fcp_parm;

	tport = ft_tport_create(rdata->local_port);
	if (!tport)
		return 0;	

	acl = ft_acl_get(tport->tpg, rdata);
	if (!acl)
		return 0;

	if (!rspp)
		goto fill;

	if (rspp->spp_flags & (FC_SPP_OPA_VAL | FC_SPP_RPA_VAL))
		return FC_SPP_RESP_NO_PA;

	fcp_parm = ntohl(rspp->spp_params);
	if (!(fcp_parm & (FCP_SPPF_INIT_FCN | FCP_SPPF_TARG_FCN)))
		return FC_SPP_RESP_INVL;

	if (rspp->spp_flags & FC_SPP_EST_IMG_PAIR) {
		spp->spp_flags |= FC_SPP_EST_IMG_PAIR;
		if (!(fcp_parm & FCP_SPPF_INIT_FCN))
			return FC_SPP_RESP_CONF;
		sess = ft_sess_create(tport, rdata->ids.port_id, acl);
		if (!sess)
			return FC_SPP_RESP_RES;
		if (!sess->params)
			rdata->prli_count++;
		sess->params = fcp_parm;
		sess->port_name = rdata->ids.port_name;
		sess->max_frame = rdata->maxframe_size;

		
	}

fill:
	fcp_parm = ntohl(spp->spp_params);
	spp->spp_params = htonl(fcp_parm | FCP_SPPF_TARG_FCN);
	return FC_SPP_RESP_ACK;
}

static int ft_prli(struct fc_rport_priv *rdata, u32 spp_len,
		   const struct fc_els_spp *rspp, struct fc_els_spp *spp)
{
	int ret;

	mutex_lock(&ft_lport_lock);
	ret = ft_prli_locked(rdata, spp_len, rspp, spp);
	mutex_unlock(&ft_lport_lock);
	pr_debug("port_id %x flags %x ret %x\n",
	       rdata->ids.port_id, rspp ? rspp->spp_flags : 0, ret);
	return ret;
}

static void ft_sess_rcu_free(struct rcu_head *rcu)
{
	struct ft_sess *sess = container_of(rcu, struct ft_sess, rcu);

	transport_deregister_session(sess->se_sess);
	kfree(sess);
}

static void ft_sess_free(struct kref *kref)
{
	struct ft_sess *sess = container_of(kref, struct ft_sess, kref);

	call_rcu(&sess->rcu, ft_sess_rcu_free);
}

void ft_sess_put(struct ft_sess *sess)
{
	int sess_held = atomic_read(&sess->kref.refcount);

	BUG_ON(!sess_held);
	kref_put(&sess->kref, ft_sess_free);
}

static void ft_prlo(struct fc_rport_priv *rdata)
{
	struct ft_sess *sess;
	struct ft_tport *tport;

	mutex_lock(&ft_lport_lock);
	tport = rcu_dereference(rdata->local_port->prov[FC_TYPE_FCP]);
	if (!tport) {
		mutex_unlock(&ft_lport_lock);
		return;
	}
	sess = ft_sess_delete(tport, rdata->ids.port_id);
	if (!sess) {
		mutex_unlock(&ft_lport_lock);
		return;
	}
	mutex_unlock(&ft_lport_lock);
	transport_deregister_session_configfs(sess->se_sess);
	ft_sess_put(sess);		
	rdata->prli_count--;
	
}

static void ft_recv(struct fc_lport *lport, struct fc_frame *fp)
{
	struct ft_sess *sess;
	u32 sid = fc_frame_sid(fp);

	pr_debug("sid %x\n", sid);

	sess = ft_sess_get(lport, sid);
	if (!sess) {
		pr_debug("sid %x sess lookup failed\n", sid);
		
		fc_frame_free(fp);
		return;
	}
	ft_recv_req(sess, fp);	
}

struct fc4_prov ft_prov = {
	.prli = ft_prli,
	.prlo = ft_prlo,
	.recv = ft_recv,
	.module = THIS_MODULE,
};
