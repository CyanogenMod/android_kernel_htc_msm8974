/*
 * ipmi.h
 *
 * MontaVista IPMI interface
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

#ifndef __LINUX_IPMI_H
#define __LINUX_IPMI_H

#include <linux/ipmi_msgdefs.h>
#include <linux/compiler.h>




#define IPMI_MAX_ADDR_SIZE 32
struct ipmi_addr {
	int   addr_type;
	short channel;
	char  data[IPMI_MAX_ADDR_SIZE];
};

#define IPMI_SYSTEM_INTERFACE_ADDR_TYPE	0x0c
struct ipmi_system_interface_addr {
	int           addr_type;
	short         channel;
	unsigned char lun;
};

#define IPMI_IPMB_ADDR_TYPE		0x01
#define IPMI_IPMB_BROADCAST_ADDR_TYPE	0x41
struct ipmi_ipmb_addr {
	int           addr_type;
	short         channel;
	unsigned char slave_addr;
	unsigned char lun;
};

#define IPMI_LAN_ADDR_TYPE		0x04
struct ipmi_lan_addr {
	int           addr_type;
	short         channel;
	unsigned char privilege;
	unsigned char session_handle;
	unsigned char remote_SWID;
	unsigned char local_SWID;
	unsigned char lun;
};


#define IPMI_BMC_CHANNEL  0xf
#define IPMI_NUM_CHANNELS 0x10

#define IPMI_CHAN_ALL     (~0)


struct ipmi_msg {
	unsigned char  netfn;
	unsigned char  cmd;
	unsigned short data_len;
	unsigned char  __user *data;
};

struct kernel_ipmi_msg {
	unsigned char  netfn;
	unsigned char  cmd;
	unsigned short data_len;
	unsigned char  *data;
};

#define IPMI_INVALID_CMD_COMPLETION_CODE	0xC1
#define IPMI_TIMEOUT_COMPLETION_CODE		0xC3
#define IPMI_UNKNOWN_ERR_COMPLETION_CODE	0xff


#define IPMI_RESPONSE_RECV_TYPE		1 
#define IPMI_ASYNC_EVENT_RECV_TYPE	2 
#define IPMI_CMD_RECV_TYPE		3 
#define IPMI_RESPONSE_RESPONSE_TYPE	4 
#define IPMI_OEM_RECV_TYPE		5 



#define IPMI_MAINTENANCE_MODE_AUTO	0
#define IPMI_MAINTENANCE_MODE_OFF	1
#define IPMI_MAINTENANCE_MODE_ON	2

#ifdef __KERNEL__

#include <linux/list.h>
#include <linux/proc_fs.h>

struct module;
struct device;

typedef struct ipmi_user *ipmi_user_t;

struct ipmi_recv_msg {
	struct list_head link;

	int              recv_type;

	ipmi_user_t      user;
	struct ipmi_addr addr;
	long             msgid;
	struct kernel_ipmi_msg  msg;

	void             *user_msg_data;

	void (*done)(struct ipmi_recv_msg *msg);

	unsigned char   msg_data[IPMI_MAX_MSG_LENGTH];
};

void ipmi_free_recv_msg(struct ipmi_recv_msg *msg);

struct ipmi_user_hndl {
	void (*ipmi_recv_hndl)(struct ipmi_recv_msg *msg,
			       void                 *user_msg_data);

	void (*ipmi_watchdog_pretimeout)(void *handler_data);
};

int ipmi_create_user(unsigned int          if_num,
		     struct ipmi_user_hndl *handler,
		     void                  *handler_data,
		     ipmi_user_t           *user);

int ipmi_destroy_user(ipmi_user_t user);

void ipmi_get_version(ipmi_user_t   user,
		      unsigned char *major,
		      unsigned char *minor);

int ipmi_set_my_address(ipmi_user_t   user,
			unsigned int  channel,
			unsigned char address);
int ipmi_get_my_address(ipmi_user_t   user,
			unsigned int  channel,
			unsigned char *address);
int ipmi_set_my_LUN(ipmi_user_t   user,
		    unsigned int  channel,
		    unsigned char LUN);
int ipmi_get_my_LUN(ipmi_user_t   user,
		    unsigned int  channel,
		    unsigned char *LUN);

int ipmi_request_settime(ipmi_user_t      user,
			 struct ipmi_addr *addr,
			 long             msgid,
			 struct kernel_ipmi_msg  *msg,
			 void             *user_msg_data,
			 int              priority,
			 int              max_retries,
			 unsigned int     retry_time_ms);

int ipmi_request_supply_msgs(ipmi_user_t          user,
			     struct ipmi_addr     *addr,
			     long                 msgid,
			     struct kernel_ipmi_msg *msg,
			     void                 *user_msg_data,
			     void                 *supplied_smi,
			     struct ipmi_recv_msg *supplied_recv,
			     int                  priority);

void ipmi_poll_interface(ipmi_user_t user);

int ipmi_register_for_cmd(ipmi_user_t   user,
			  unsigned char netfn,
			  unsigned char cmd,
			  unsigned int  chans);
int ipmi_unregister_for_cmd(ipmi_user_t   user,
			    unsigned char netfn,
			    unsigned char cmd,
			    unsigned int  chans);

int ipmi_get_maintenance_mode(ipmi_user_t user);
int ipmi_set_maintenance_mode(ipmi_user_t user, int mode);

int ipmi_set_gets_events(ipmi_user_t user, int val);

struct ipmi_smi_watcher {
	struct list_head link;

	struct module *owner;

	void (*new_smi)(int if_num, struct device *dev);
	void (*smi_gone)(int if_num);
};

int ipmi_smi_watcher_register(struct ipmi_smi_watcher *watcher);
int ipmi_smi_watcher_unregister(struct ipmi_smi_watcher *watcher);


unsigned int ipmi_addr_length(int addr_type);

int ipmi_validate_addr(struct ipmi_addr *addr, int len);

enum ipmi_addr_src {
	SI_INVALID = 0, SI_HOTMOD, SI_HARDCODED, SI_SPMI, SI_ACPI, SI_SMBIOS,
	SI_PCI,	SI_DEVICETREE, SI_DEFAULT
};

union ipmi_smi_info_union {
	struct {
		void *acpi_handle;
	} acpi_info;
};

struct ipmi_smi_info {
	enum ipmi_addr_src addr_src;

	struct device *dev;

	union ipmi_smi_info_union addr_info;
};

extern int ipmi_get_smi_info(int if_num, struct ipmi_smi_info *data);

#endif 





#define IPMI_IOC_MAGIC 'i'


struct ipmi_req {
	unsigned char __user *addr; 
	unsigned int  addr_len;

	long    msgid; 

	struct ipmi_msg msg;
};
#define IPMICTL_SEND_COMMAND		_IOR(IPMI_IOC_MAGIC, 13,	\
					     struct ipmi_req)

struct ipmi_req_settime {
	struct ipmi_req req;

	int          retries;
	unsigned int retry_time_ms;
};
#define IPMICTL_SEND_COMMAND_SETTIME	_IOR(IPMI_IOC_MAGIC, 21,	\
					     struct ipmi_req_settime)

struct ipmi_recv {
	int     recv_type; 

	unsigned char __user *addr;    
	unsigned int  addr_len; 

	long    msgid; 

	struct ipmi_msg msg; 
};

#define IPMICTL_RECEIVE_MSG		_IOWR(IPMI_IOC_MAGIC, 12,	\
					      struct ipmi_recv)

#define IPMICTL_RECEIVE_MSG_TRUNC	_IOWR(IPMI_IOC_MAGIC, 11,	\
					      struct ipmi_recv)

struct ipmi_cmdspec {
	unsigned char netfn;
	unsigned char cmd;
};

#define IPMICTL_REGISTER_FOR_CMD	_IOR(IPMI_IOC_MAGIC, 14,	\
					     struct ipmi_cmdspec)
#define IPMICTL_UNREGISTER_FOR_CMD	_IOR(IPMI_IOC_MAGIC, 15,	\
					     struct ipmi_cmdspec)

struct ipmi_cmdspec_chans {
	unsigned int netfn;
	unsigned int cmd;
	unsigned int chans;
};

#define IPMICTL_REGISTER_FOR_CMD_CHANS	_IOR(IPMI_IOC_MAGIC, 28,	\
					     struct ipmi_cmdspec_chans)
#define IPMICTL_UNREGISTER_FOR_CMD_CHANS _IOR(IPMI_IOC_MAGIC, 29,	\
					     struct ipmi_cmdspec_chans)

#define IPMICTL_SET_GETS_EVENTS_CMD	_IOR(IPMI_IOC_MAGIC, 16, int)

struct ipmi_channel_lun_address_set {
	unsigned short channel;
	unsigned char  value;
};
#define IPMICTL_SET_MY_CHANNEL_ADDRESS_CMD \
	_IOR(IPMI_IOC_MAGIC, 24, struct ipmi_channel_lun_address_set)
#define IPMICTL_GET_MY_CHANNEL_ADDRESS_CMD \
	_IOR(IPMI_IOC_MAGIC, 25, struct ipmi_channel_lun_address_set)
#define IPMICTL_SET_MY_CHANNEL_LUN_CMD \
	_IOR(IPMI_IOC_MAGIC, 26, struct ipmi_channel_lun_address_set)
#define IPMICTL_GET_MY_CHANNEL_LUN_CMD \
	_IOR(IPMI_IOC_MAGIC, 27, struct ipmi_channel_lun_address_set)
#define IPMICTL_SET_MY_ADDRESS_CMD	_IOR(IPMI_IOC_MAGIC, 17, unsigned int)
#define IPMICTL_GET_MY_ADDRESS_CMD	_IOR(IPMI_IOC_MAGIC, 18, unsigned int)
#define IPMICTL_SET_MY_LUN_CMD		_IOR(IPMI_IOC_MAGIC, 19, unsigned int)
#define IPMICTL_GET_MY_LUN_CMD		_IOR(IPMI_IOC_MAGIC, 20, unsigned int)

struct ipmi_timing_parms {
	int          retries;
	unsigned int retry_time_ms;
};
#define IPMICTL_SET_TIMING_PARMS_CMD	_IOR(IPMI_IOC_MAGIC, 22, \
					     struct ipmi_timing_parms)
#define IPMICTL_GET_TIMING_PARMS_CMD	_IOR(IPMI_IOC_MAGIC, 23, \
					     struct ipmi_timing_parms)

#define IPMICTL_GET_MAINTENANCE_MODE_CMD	_IOR(IPMI_IOC_MAGIC, 30, int)
#define IPMICTL_SET_MAINTENANCE_MODE_CMD	_IOW(IPMI_IOC_MAGIC, 31, int)

#endif 
