/*
 * Copyright(c) 2009 Intel Corporation. All rights reserved.
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
 *
 * Maintained at www.Open-FCoE.org
 */


#include <scsi/libfc.h>
#include <linux/export.h>


struct fc_lport *libfc_vport_create(struct fc_vport *vport, int privsize)
{
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_lport *n_port = shost_priv(shost);
	struct fc_lport *vn_port;

	vn_port = libfc_host_alloc(shost->hostt, privsize);
	if (!vn_port)
		return vn_port;

	vn_port->vport = vport;
	vport->dd_data = vn_port;

	mutex_lock(&n_port->lp_mutex);
	list_add_tail(&vn_port->list, &n_port->vports);
	mutex_unlock(&n_port->lp_mutex);

	return vn_port;
}
EXPORT_SYMBOL(libfc_vport_create);

struct fc_lport *fc_vport_id_lookup(struct fc_lport *n_port, u32 port_id)
{
	struct fc_lport *lport = NULL;
	struct fc_lport *vn_port;

	if (n_port->port_id == port_id)
		return n_port;

	if (port_id == FC_FID_FLOGI)
		return n_port;		

	mutex_lock(&n_port->lp_mutex);
	list_for_each_entry(vn_port, &n_port->vports, list) {
		if (vn_port->port_id == port_id) {
			lport = vn_port;
			break;
		}
	}
	mutex_unlock(&n_port->lp_mutex);

	return lport;
}
EXPORT_SYMBOL(fc_vport_id_lookup);

enum libfc_lport_mutex_class {
	LPORT_MUTEX_NORMAL = 0,
	LPORT_MUTEX_VN_PORT = 1,
};

static void __fc_vport_setlink(struct fc_lport *n_port,
			       struct fc_lport *vn_port)
{
	struct fc_vport *vport = vn_port->vport;

	if (vn_port->state == LPORT_ST_DISABLED)
		return;

	if (n_port->state == LPORT_ST_READY) {
		if (n_port->npiv_enabled) {
			fc_vport_set_state(vport, FC_VPORT_INITIALIZING);
			__fc_linkup(vn_port);
		} else {
			fc_vport_set_state(vport, FC_VPORT_NO_FABRIC_SUPP);
			__fc_linkdown(vn_port);
		}
	} else {
		fc_vport_set_state(vport, FC_VPORT_LINKDOWN);
		__fc_linkdown(vn_port);
	}
}

void fc_vport_setlink(struct fc_lport *vn_port)
{
	struct fc_vport *vport = vn_port->vport;
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_lport *n_port = shost_priv(shost);

	mutex_lock(&n_port->lp_mutex);
	mutex_lock_nested(&vn_port->lp_mutex, LPORT_MUTEX_VN_PORT);
	__fc_vport_setlink(n_port, vn_port);
	mutex_unlock(&vn_port->lp_mutex);
	mutex_unlock(&n_port->lp_mutex);
}
EXPORT_SYMBOL(fc_vport_setlink);

void fc_vports_linkchange(struct fc_lport *n_port)
{
	struct fc_lport *vn_port;

	list_for_each_entry(vn_port, &n_port->vports, list) {
		mutex_lock_nested(&vn_port->lp_mutex, LPORT_MUTEX_VN_PORT);
		__fc_vport_setlink(n_port, vn_port);
		mutex_unlock(&vn_port->lp_mutex);
	}
}

