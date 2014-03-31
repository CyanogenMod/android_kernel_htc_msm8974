/*
 * ipmi_smi.h
 *
 * MontaVista IPMI system management interface
 *
 * Author: MontaVista Software, Inc.
 *         Corey Minyard <minyard@mvista.com>
 *         source@mvista.com
 *
 * Copyright 2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __LINUX_IPMI_SMI_H
#define __LINUX_IPMI_SMI_H

#include <linux/ipmi_msgdefs.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/ipmi.h>

struct device;


typedef struct ipmi_smi *ipmi_smi_t;

struct ipmi_smi_msg {
	struct list_head link;

	long    msgid;
	void    *user_data;

	int           data_size;
	unsigned char data[IPMI_MAX_MSG_LENGTH];

	int           rsp_size;
	unsigned char rsp[IPMI_MAX_MSG_LENGTH];

	void (*done)(struct ipmi_smi_msg *msg);
};

struct ipmi_smi_handlers {
	struct module *owner;

	int (*start_processing)(void       *send_info,
				ipmi_smi_t new_intf);

	int (*get_smi_info)(void *send_info, struct ipmi_smi_info *data);

	void (*sender)(void                *send_info,
		       struct ipmi_smi_msg *msg,
		       int                 priority);

	void (*request_events)(void *send_info);

	void (*set_run_to_completion)(void *send_info, int run_to_completion);

	void (*poll)(void *send_info);

	void (*set_maintenance_mode)(void *send_info, int enable);

	int (*inc_usecount)(void *send_info);
	void (*dec_usecount)(void *send_info);
};

struct ipmi_device_id {
	unsigned char device_id;
	unsigned char device_revision;
	unsigned char firmware_revision_1;
	unsigned char firmware_revision_2;
	unsigned char ipmi_version;
	unsigned char additional_device_support;
	unsigned int  manufacturer_id;
	unsigned int  product_id;
	unsigned char aux_firmware_revision[4];
	unsigned int  aux_firmware_revision_set : 1;
};

#define ipmi_version_major(v) ((v)->ipmi_version & 0xf)
#define ipmi_version_minor(v) ((v)->ipmi_version >> 4)

static inline int ipmi_demangle_device_id(const unsigned char *data,
					  unsigned int data_len,
					  struct ipmi_device_id *id)
{
	if (data_len < 9)
		return -EINVAL;
	if (data[0] != IPMI_NETFN_APP_RESPONSE << 2 ||
	    data[1] != IPMI_GET_DEVICE_ID_CMD)
		
		return -EINVAL;
	if (data[2] != 0)
		
		return -EINVAL;

	data += 3;
	data_len -= 3;
	id->device_id = data[0];
	id->device_revision = data[1];
	id->firmware_revision_1 = data[2];
	id->firmware_revision_2 = data[3];
	id->ipmi_version = data[4];
	id->additional_device_support = data[5];
	if (data_len >= 11) {
		id->manufacturer_id = (data[6] | (data[7] << 8) |
				       (data[8] << 16));
		id->product_id = data[9] | (data[10] << 8);
	} else {
		id->manufacturer_id = 0;
		id->product_id = 0;
	}
	if (data_len >= 15) {
		memcpy(id->aux_firmware_revision, data+11, 4);
		id->aux_firmware_revision_set = 1;
	} else
		id->aux_firmware_revision_set = 0;

	return 0;
}

int ipmi_register_smi(struct ipmi_smi_handlers *handlers,
		      void                     *send_info,
		      struct ipmi_device_id    *device_id,
		      struct device            *dev,
		      const char               *sysfs_name,
		      unsigned char            slave_addr);

int ipmi_unregister_smi(ipmi_smi_t intf);

void ipmi_smi_msg_received(ipmi_smi_t          intf,
			   struct ipmi_smi_msg *msg);

void ipmi_smi_watchdog_pretimeout(ipmi_smi_t intf);

struct ipmi_smi_msg *ipmi_alloc_smi_msg(void);
static inline void ipmi_free_smi_msg(struct ipmi_smi_msg *msg)
{
	msg->done(msg);
}

int ipmi_smi_add_proc_entry(ipmi_smi_t smi, char *name,
			    const struct file_operations *proc_ops,
			    void *data);

#endif 
