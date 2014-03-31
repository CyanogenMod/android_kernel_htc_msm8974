/*******************************************************************************
 * Filename:  target_core_fabric_lib.c
 *
 * This file contains generic high level protocol identifier and PR
 * handlers for TCM fabric modules
 *
 * Copyright (c) 2010 Rising Tide Systems, Inc.
 * Copyright (c) 2010 Linux-iSCSI.org
 *
 * Nicholas A. Bellinger <nab@linux-iscsi.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 ******************************************************************************/

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/spinlock.h>
#include <linux/export.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>

#include <target/target_core_base.h>
#include <target/target_core_fabric.h>
#include <target/target_core_configfs.h>

#include "target_core_internal.h"
#include "target_core_pr.h"

u8 sas_get_fabric_proto_ident(struct se_portal_group *se_tpg)
{
	return 0x6;
}
EXPORT_SYMBOL(sas_get_fabric_proto_ident);

u32 sas_get_pr_transport_id(
	struct se_portal_group *se_tpg,
	struct se_node_acl *se_nacl,
	struct t10_pr_registration *pr_reg,
	int *format_code,
	unsigned char *buf)
{
	unsigned char *ptr;
	int ret;

	buf[0] = 0x06;
	ptr = &se_nacl->initiatorname[4]; 

	ret = hex2bin(&buf[4], ptr, 8);
	if (ret < 0)
		pr_debug("sas transport_id: invalid hex string\n");

	return 24;
}
EXPORT_SYMBOL(sas_get_pr_transport_id);

u32 sas_get_pr_transport_id_len(
	struct se_portal_group *se_tpg,
	struct se_node_acl *se_nacl,
	struct t10_pr_registration *pr_reg,
	int *format_code)
{
	*format_code = 0;
	return 24;
}
EXPORT_SYMBOL(sas_get_pr_transport_id_len);

char *sas_parse_pr_out_transport_id(
	struct se_portal_group *se_tpg,
	const char *buf,
	u32 *out_tid_len,
	char **port_nexus_ptr)
{
	*port_nexus_ptr = NULL;
	*out_tid_len = 24;

	return (char *)&buf[4];
}
EXPORT_SYMBOL(sas_parse_pr_out_transport_id);

u8 fc_get_fabric_proto_ident(struct se_portal_group *se_tpg)
{
	return 0x0;	
}
EXPORT_SYMBOL(fc_get_fabric_proto_ident);

u32 fc_get_pr_transport_id_len(
	struct se_portal_group *se_tpg,
	struct se_node_acl *se_nacl,
	struct t10_pr_registration *pr_reg,
	int *format_code)
{
	*format_code = 0;
	return 24;
}
EXPORT_SYMBOL(fc_get_pr_transport_id_len);

u32 fc_get_pr_transport_id(
	struct se_portal_group *se_tpg,
	struct se_node_acl *se_nacl,
	struct t10_pr_registration *pr_reg,
	int *format_code,
	unsigned char *buf)
{
	unsigned char *ptr;
	int i, ret;
	u32 off = 8;

	ptr = &se_nacl->initiatorname[0];

	for (i = 0; i < 24; ) {
		if (!strncmp(&ptr[i], ":", 1)) {
			i++;
			continue;
		}
		ret = hex2bin(&buf[off++], &ptr[i], 1);
		if (ret < 0)
			pr_debug("fc transport_id: invalid hex string\n");
		i += 2;
	}
	return 24;
}
EXPORT_SYMBOL(fc_get_pr_transport_id);

char *fc_parse_pr_out_transport_id(
	struct se_portal_group *se_tpg,
	const char *buf,
	u32 *out_tid_len,
	char **port_nexus_ptr)
{
	*port_nexus_ptr = NULL;
	*out_tid_len = 24;

	 return (char *)&buf[8];
}
EXPORT_SYMBOL(fc_parse_pr_out_transport_id);


u8 iscsi_get_fabric_proto_ident(struct se_portal_group *se_tpg)
{
	return 0x5;
}
EXPORT_SYMBOL(iscsi_get_fabric_proto_ident);

u32 iscsi_get_pr_transport_id(
	struct se_portal_group *se_tpg,
	struct se_node_acl *se_nacl,
	struct t10_pr_registration *pr_reg,
	int *format_code,
	unsigned char *buf)
{
	u32 off = 4, padding = 0;
	u16 len = 0;

	spin_lock_irq(&se_nacl->nacl_sess_lock);
	buf[0] = 0x05;
	len = sprintf(&buf[off], "%s", se_nacl->initiatorname);
	len++;
	if ((*format_code == 1) && (pr_reg->isid_present_at_reg)) {
		buf[0] |= 0x40;
		buf[off+len] = 0x2c; off++; 
		buf[off+len] = 0x69; off++; 
		buf[off+len] = 0x2c; off++; 
		buf[off+len] = 0x30; off++; 
		buf[off+len] = 0x78; off++; 
		len += 5;
		buf[off+len] = pr_reg->pr_reg_isid[0]; off++;
		buf[off+len] = pr_reg->pr_reg_isid[1]; off++;
		buf[off+len] = pr_reg->pr_reg_isid[2]; off++;
		buf[off+len] = pr_reg->pr_reg_isid[3]; off++;
		buf[off+len] = pr_reg->pr_reg_isid[4]; off++;
		buf[off+len] = pr_reg->pr_reg_isid[5]; off++;
		buf[off+len] = '\0'; off++;
		len += 7;
	}
	spin_unlock_irq(&se_nacl->nacl_sess_lock);
	padding = ((-len) & 3);
	if (padding != 0)
		len += padding;

	buf[2] = ((len >> 8) & 0xff);
	buf[3] = (len & 0xff);
	len += 4;

	return len;
}
EXPORT_SYMBOL(iscsi_get_pr_transport_id);

u32 iscsi_get_pr_transport_id_len(
	struct se_portal_group *se_tpg,
	struct se_node_acl *se_nacl,
	struct t10_pr_registration *pr_reg,
	int *format_code)
{
	u32 len = 0, padding = 0;

	spin_lock_irq(&se_nacl->nacl_sess_lock);
	len = strlen(se_nacl->initiatorname);
	len++;
	if (pr_reg->isid_present_at_reg) {
		len += 5; 
		len += 7; 
		*format_code = 1;
	} else
		*format_code = 0;
	spin_unlock_irq(&se_nacl->nacl_sess_lock);
	padding = ((-len) & 3);
	if (padding != 0)
		len += padding;
	len += 4;

	return len;
}
EXPORT_SYMBOL(iscsi_get_pr_transport_id_len);

char *iscsi_parse_pr_out_transport_id(
	struct se_portal_group *se_tpg,
	const char *buf,
	u32 *out_tid_len,
	char **port_nexus_ptr)
{
	char *p;
	u32 tid_len, padding;
	int i;
	u16 add_len;
	u8 format_code = (buf[0] & 0xc0);
	if ((format_code != 0x00) && (format_code != 0x40)) {
		pr_err("Illegal format code: 0x%02x for iSCSI"
			" Initiator Transport ID\n", format_code);
		return NULL;
	}
	 if (out_tid_len != NULL) {
		add_len = ((buf[2] >> 8) & 0xff);
		add_len |= (buf[3] & 0xff);

		tid_len = strlen(&buf[4]);
		tid_len += 4; 
		tid_len += 1; 
		padding = ((-tid_len) & 3);
		if (padding != 0)
			tid_len += padding;

		if ((add_len + 4) != tid_len) {
			pr_debug("LIO-Target Extracted add_len: %hu "
				"does not match calculated tid_len: %u,"
				" using tid_len instead\n", add_len+4, tid_len);
			*out_tid_len = tid_len;
		} else
			*out_tid_len = (add_len + 4);
	}
	if (format_code == 0x40) {
		p = strstr(&buf[4], ",i,0x");
		if (!p) {
			pr_err("Unable to locate \",i,0x\" seperator"
				" for Initiator port identifier: %s\n",
				&buf[4]);
			return NULL;
		}
		*p = '\0'; 
		p += 5; 

		*port_nexus_ptr = p;
		for (i = 0; i < 12; i++) {
			if (isdigit(*p)) {
				p++;
				continue;
			}
			*p = tolower(*p);
			p++;
		}
	}

	return (char *)&buf[4];
}
EXPORT_SYMBOL(iscsi_parse_pr_out_transport_id);
