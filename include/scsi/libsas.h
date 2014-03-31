/*
 * SAS host prototypes and structures header file
 *
 * Copyright (C) 2005 Adaptec, Inc.  All rights reserved.
 * Copyright (C) 2005 Luben Tuikov <luben_tuikov@adaptec.com>
 *
 * This file is licensed under GPLv2.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#ifndef _LIBSAS_H_
#define _LIBSAS_H_


#include <linux/timer.h>
#include <linux/pci.h>
#include <scsi/sas.h>
#include <linux/libata.h>
#include <linux/list.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_transport_sas.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

struct block_device;

enum sas_class {
	SAS,
	EXPANDER
};

enum sas_phy_role {
	PHY_ROLE_NONE = 0,
	PHY_ROLE_TARGET = 0x40,
	PHY_ROLE_INITIATOR = 0x80,
};

enum sas_phy_type {
        PHY_TYPE_PHYSICAL,
        PHY_TYPE_VIRTUAL
};

enum ha_event {
	HAE_RESET             = 0U,
	HA_NUM_EVENTS         = 1,
};

enum port_event {
	PORTE_BYTES_DMAED     = 0U,
	PORTE_BROADCAST_RCVD  = 1,
	PORTE_LINK_RESET_ERR  = 2,
	PORTE_TIMER_EVENT     = 3,
	PORTE_HARD_RESET      = 4,
	PORT_NUM_EVENTS       = 5,
};

enum phy_event {
	PHYE_LOSS_OF_SIGNAL   = 0U,
	PHYE_OOB_DONE         = 1,
	PHYE_OOB_ERROR        = 2,
	PHYE_SPINUP_HOLD      = 3, 
	PHY_NUM_EVENTS        = 4,
};

enum discover_event {
	DISCE_DISCOVER_DOMAIN   = 0U,
	DISCE_REVALIDATE_DOMAIN = 1,
	DISCE_PORT_GONE         = 2,
	DISCE_PROBE		= 3,
	DISCE_DESTRUCT		= 4,
	DISC_NUM_EVENTS		= 5,
};


#define to_dom_device(_obj) container_of(_obj, struct domain_device, dev_obj)
#define to_dev_attr(_attr)  container_of(_attr, struct domain_dev_attribute,\
                                         attr)

enum routing_attribute {
	DIRECT_ROUTING,
	SUBTRACTIVE_ROUTING,
	TABLE_ROUTING,
};

enum ex_phy_state {
	PHY_EMPTY,
	PHY_VACANT,
	PHY_NOT_PRESENT,
	PHY_DEVICE_DISCOVERED
};

struct ex_phy {
	int    phy_id;

	enum ex_phy_state phy_state;

	enum sas_dev_type attached_dev_type;
	enum sas_linkrate linkrate;

	u8   attached_sata_host:1;
	u8   attached_sata_dev:1;
	u8   attached_sata_ps:1;

	enum sas_protocol attached_tproto;
	enum sas_protocol attached_iproto;

	u8   attached_sas_addr[SAS_ADDR_SIZE];
	u8   attached_phy_id;

	u8   phy_change_count;
	enum routing_attribute routing_attr;
	u8   virtual:1;

	int  last_da_index;

	struct sas_phy *phy;
	struct sas_port *port;
};

struct expander_device {
	struct list_head children;

	u16    ex_change_count;
	u16    max_route_indexes;
	u8     num_phys;

	u8     t2t_supp:1;
	u8     configuring:1;
	u8     conf_route_table:1;

	u8     enclosure_logical_id[8];

	struct ex_phy *ex_phy;
	struct sas_port *parent_port;

	struct mutex cmd_mutex;
};

enum ata_command_set {
        ATA_COMMAND_SET   = 0,
        ATAPI_COMMAND_SET = 1,
};

struct sata_device {
        enum   ata_command_set command_set;
        struct smp_resp        rps_resp; 
        u8     port_no;        
        struct list_head children; 

	struct ata_port *ap;
	struct ata_host ata_host;
	struct ata_taskfile tf;
};

enum {
	SAS_DEV_GONE,
	SAS_DEV_DESTROY,
};

struct domain_device {
	spinlock_t done_lock;
        enum sas_dev_type dev_type;

        enum sas_linkrate linkrate;
        enum sas_linkrate min_linkrate;
        enum sas_linkrate max_linkrate;

        int  pathways;

        struct domain_device *parent;
        struct list_head siblings; 
        struct asd_sas_port *port;        
	struct sas_phy *phy;

        struct list_head dev_list_node;
	struct list_head disco_list_node; 

        enum sas_protocol    iproto;
        enum sas_protocol    tproto;

        struct sas_rphy *rphy;

        u8  sas_addr[SAS_ADDR_SIZE];
        u8  hashed_sas_addr[HASHED_SAS_ADDR_SIZE];

        u8  frame_rcvd[32];

        union {
                struct expander_device ex_dev;
                struct sata_device     sata_dev; 
        };

        void *lldd_dev;
	unsigned long state;
	struct kref kref;
};

struct sas_work {
	struct list_head drain_node;
	struct work_struct work;
};

static inline void INIT_SAS_WORK(struct sas_work *sw, void (*fn)(struct work_struct *))
{
	INIT_WORK(&sw->work, fn);
	INIT_LIST_HEAD(&sw->drain_node);
}

struct sas_discovery_event {
	struct sas_work work;
	struct asd_sas_port *port;
};

static inline struct sas_discovery_event *to_sas_discovery_event(struct work_struct *work)
{
	struct sas_discovery_event *ev = container_of(work, typeof(*ev), work.work);

	return ev;
}

struct sas_discovery {
	struct sas_discovery_event disc_work[DISC_NUM_EVENTS];
	unsigned long    pending;
	u8     fanout_sas_addr[8];
	u8     eeds_a[8];
	u8     eeds_b[8];
	int    max_level;
};

struct asd_sas_port {
	struct completion port_gone_completion;

	struct sas_discovery disc;
	struct domain_device *port_dev;
	spinlock_t dev_list_lock;
	struct list_head dev_list;
	struct list_head disco_list;
	struct list_head destroy_list;
	enum   sas_linkrate linkrate;

	struct sas_work work;

	int id;

	enum sas_class   class;
	u8               sas_addr[SAS_ADDR_SIZE];
	u8               attached_sas_addr[SAS_ADDR_SIZE];
	enum sas_protocol   iproto;
	enum sas_protocol   tproto;

	enum sas_oob_mode oob_mode;

	spinlock_t       phy_list_lock;
	struct list_head phy_list;
	int              num_phys;
	u32              phy_mask;

	struct sas_ha_struct *ha;

	struct sas_port	*port;

	void *lldd_port;	  
};

struct asd_sas_event {
	struct sas_work work;
	struct asd_sas_phy *phy;
};

static inline struct asd_sas_event *to_asd_sas_event(struct work_struct *work)
{
	struct asd_sas_event *ev = container_of(work, typeof(*ev), work.work);

	return ev;
}

struct asd_sas_phy {
	struct asd_sas_event   port_events[PORT_NUM_EVENTS];
	struct asd_sas_event   phy_events[PHY_NUM_EVENTS];

	unsigned long port_events_pending;
	unsigned long phy_events_pending;

	int error;

	struct sas_phy *phy;

	
	int            enabled;	  

	int            id;	  
	enum sas_class class;
	enum sas_protocol iproto;
	enum sas_protocol tproto;

	enum sas_phy_type  type;
	enum sas_phy_role  role;
	enum sas_oob_mode  oob_mode;
	enum sas_linkrate linkrate;

	u8   *sas_addr;		  
	u8   attached_sas_addr[SAS_ADDR_SIZE]; 

	spinlock_t     frame_rcvd_lock;
	u8             *frame_rcvd; 
	int            frame_rcvd_size;

	spinlock_t     sas_prim_lock;
	u32            sas_prim;

	struct list_head port_phy_el; 
	struct asd_sas_port      *port; 

	struct sas_ha_struct *ha; 

	void *lldd_phy;		  
};

struct scsi_core {
	struct Scsi_Host *shost;

	struct mutex	  task_queue_flush;
	spinlock_t        task_queue_lock;
	struct list_head  task_queue;
	int               task_queue_size;

	struct task_struct *queue_thread;
};

struct sas_ha_event {
	struct sas_work work;
	struct sas_ha_struct *ha;
};

static inline struct sas_ha_event *to_sas_ha_event(struct work_struct *work)
{
	struct sas_ha_event *ev = container_of(work, typeof(*ev), work.work);

	return ev;
}

enum sas_ha_state {
	SAS_HA_REGISTERED,
	SAS_HA_DRAINING,
	SAS_HA_ATA_EH_ACTIVE,
	SAS_HA_FROZEN,
};

struct sas_ha_struct {
	struct sas_ha_event ha_events[HA_NUM_EVENTS];
	unsigned long	 pending;

	struct list_head  defer_q; 
	struct mutex	  drain_mutex;
	unsigned long	  state;
	spinlock_t 	  state_lock;

	struct mutex disco_mutex;

	struct scsi_core core;

	char *sas_ha_name;
	struct device *dev;	  
	struct module *lldd_module; 

	u8 *sas_addr;		  
	u8 hashed_sas_addr[HASHED_SAS_ADDR_SIZE];

	spinlock_t      phy_port_lock;
	struct asd_sas_phy  **sas_phy; 
	struct asd_sas_port **sas_port; 
	int             num_phys; 

	
	int lldd_max_execute_num;
	int lldd_queue_size;
	int strict_wide_ports; 

	
	void (*notify_ha_event)(struct sas_ha_struct *, enum ha_event);
	void (*notify_port_event)(struct asd_sas_phy *, enum port_event);
	void (*notify_phy_event)(struct asd_sas_phy *, enum phy_event);

	void *lldd_ha;		  

	struct list_head eh_done_q;  
	struct list_head eh_ata_q; 
};

#define SHOST_TO_SAS_HA(_shost) (*(struct sas_ha_struct **)(_shost)->hostdata)

static inline struct domain_device *
starget_to_domain_dev(struct scsi_target *starget) {
	return starget->hostdata;
}

static inline struct domain_device *
sdev_to_domain_dev(struct scsi_device *sdev) {
	return starget_to_domain_dev(sdev->sdev_target);
}

static inline struct ata_device *sas_to_ata_dev(struct domain_device *dev)
{
	return &dev->sata_dev.ap->link.device[0];
}

static inline struct domain_device *
cmd_to_domain_dev(struct scsi_cmnd *cmd)
{
	return sdev_to_domain_dev(cmd->device);
}

void sas_hash_addr(u8 *hashed, const u8 *sas_addr);

static inline void sas_phy_disconnected(struct asd_sas_phy *phy)
{
	phy->oob_mode = OOB_NOT_CONNECTED;
	phy->linkrate = SAS_LINK_RATE_UNKNOWN;
}

static inline unsigned int to_sas_gpio_od(int device, int bit)
{
	return 3 * device + bit;
}

static inline void sas_put_local_phy(struct sas_phy *phy)
{
	put_device(&phy->dev);
}

#ifdef CONFIG_SCSI_SAS_HOST_SMP
int try_test_sas_gpio_gp_bit(unsigned int od, u8 *data, u8 index, u8 count);
#else
static inline int try_test_sas_gpio_gp_bit(unsigned int od, u8 *data, u8 index, u8 count)
{
	return -1;
}
#endif


enum service_response {
	SAS_TASK_COMPLETE,
	SAS_TASK_UNDELIVERED = -1,
};

enum exec_status {
	__SAM_STAT_CHECK_CONDITION = SAM_STAT_CHECK_CONDITION,

	SAS_DEV_NO_RESPONSE = 0x80,
	SAS_DATA_UNDERRUN,
	SAS_DATA_OVERRUN,
	SAS_INTERRUPTED,
	SAS_QUEUE_FULL,
	SAS_DEVICE_UNKNOWN,
	SAS_SG_ERR,
	SAS_OPEN_REJECT,
	SAS_OPEN_TO,
	SAS_PROTO_RESPONSE,
	SAS_PHY_DOWN,
	SAS_NAK_R_ERR,
	SAS_PENDING,
	SAS_ABORTED_TASK,
};

struct ata_task_resp {
	u16  frame_len;
	u8   ending_fis[24];	  
};

#define SAS_STATUS_BUF_SIZE 96

struct task_status_struct {
	enum service_response resp;
	enum exec_status      stat;
	int  buf_valid_size;

	u8   buf[SAS_STATUS_BUF_SIZE];

	u32  residual;
	enum sas_open_rej_reason open_rej_reason;
};

struct sas_ata_task {
	struct host_to_dev_fis fis;
	u8     atapi_packet[16];  

	u8     retry_count;	  

	u8     dma_xfer:1;	  
	u8     use_ncq:1;
	u8     set_affil_pol:1;
	u8     stp_affil_pol:1;

	u8     device_control_reg_update:1;
};

struct sas_smp_task {
	struct scatterlist smp_req;
	struct scatterlist smp_resp;
};

enum task_attribute {
	TASK_ATTR_SIMPLE = 0,
	TASK_ATTR_HOQ    = 1,
	TASK_ATTR_ORDERED= 2,
	TASK_ATTR_ACA    = 4,
};

struct sas_ssp_task {
	u8     retry_count;	  

	u8     LUN[8];
	u8     enable_first_burst:1;
	enum   task_attribute task_attr;
	u8     task_prio;
	u8     cdb[16];
};

struct sas_task {
	struct domain_device *dev;
	struct list_head      list;

	spinlock_t   task_state_lock;
	unsigned     task_state_flags;

	enum   sas_protocol      task_proto;

	
	struct timer_list     timer;
	struct completion     completion;

	union {
		struct sas_ata_task ata_task;
		struct sas_smp_task smp_task;
		struct sas_ssp_task ssp_task;
	};

	struct scatterlist *scatter;
	int    num_scatter;
	u32    total_xfer_len;
	u8     data_dir:2;	  

	struct task_status_struct task_status;
	void   (*task_done)(struct sas_task *);

	void   *lldd_task;	  
	void   *uldd_task;

	struct work_struct abort_work;
};

#define SAS_TASK_STATE_PENDING      1
#define SAS_TASK_STATE_DONE         2
#define SAS_TASK_STATE_ABORTED      4
#define SAS_TASK_NEED_DEV_RESET     8
#define SAS_TASK_AT_INITIATOR       16

extern struct sas_task *sas_alloc_task(gfp_t flags);
extern void sas_free_task(struct sas_task *task);

struct sas_domain_function_template {
	
	void (*lldd_port_formed)(struct asd_sas_phy *);
	void (*lldd_port_deformed)(struct asd_sas_phy *);

	
	int  (*lldd_dev_found)(struct domain_device *);
	void (*lldd_dev_gone)(struct domain_device *);

	int (*lldd_execute_task)(struct sas_task *, int num,
				 gfp_t gfp_flags);

	
	int (*lldd_abort_task)(struct sas_task *);
	int (*lldd_abort_task_set)(struct domain_device *, u8 *lun);
	int (*lldd_clear_aca)(struct domain_device *, u8 *lun);
	int (*lldd_clear_task_set)(struct domain_device *, u8 *lun);
	int (*lldd_I_T_nexus_reset)(struct domain_device *);
	int (*lldd_ata_check_ready)(struct domain_device *);
	void (*lldd_ata_set_dmamode)(struct domain_device *);
	int (*lldd_lu_reset)(struct domain_device *, u8 *lun);
	int (*lldd_query_task)(struct sas_task *);

	
	int (*lldd_clear_nexus_port)(struct asd_sas_port *);
	int (*lldd_clear_nexus_ha)(struct sas_ha_struct *);

	
	int (*lldd_control_phy)(struct asd_sas_phy *, enum phy_func, void *);

	
	int (*lldd_write_gpio)(struct sas_ha_struct *, u8 reg_type,
			       u8 reg_index, u8 reg_count, u8 *write_data);
};

extern int sas_register_ha(struct sas_ha_struct *);
extern int sas_unregister_ha(struct sas_ha_struct *);

int sas_set_phy_speed(struct sas_phy *phy,
		      struct sas_phy_linkrates *rates);
int sas_phy_reset(struct sas_phy *phy, int hard_reset);
int sas_queue_up(struct sas_task *task);
extern int sas_queuecommand(struct Scsi_Host * ,struct scsi_cmnd *);
extern int sas_target_alloc(struct scsi_target *);
extern int sas_slave_configure(struct scsi_device *);
extern int sas_change_queue_depth(struct scsi_device *, int new_depth,
				  int reason);
extern int sas_change_queue_type(struct scsi_device *, int qt);
extern int sas_bios_param(struct scsi_device *,
			  struct block_device *,
			  sector_t capacity, int *hsc);
extern struct scsi_transport_template *
sas_domain_attach_transport(struct sas_domain_function_template *);
extern void sas_domain_release_transport(struct scsi_transport_template *);

int  sas_discover_root_expander(struct domain_device *);

void sas_init_ex_attr(void);

int  sas_ex_revalidate_domain(struct domain_device *);

void sas_unregister_domain_devices(struct asd_sas_port *port, int gone);
void sas_init_disc(struct sas_discovery *disc, struct asd_sas_port *);
int  sas_discover_event(struct asd_sas_port *, enum discover_event ev);

int  sas_discover_sata(struct domain_device *);
int  sas_discover_end_dev(struct domain_device *);

void sas_unregister_dev(struct asd_sas_port *port, struct domain_device *);

void sas_init_dev(struct domain_device *);

void sas_task_abort(struct sas_task *);
int sas_eh_device_reset_handler(struct scsi_cmnd *cmd);
int sas_eh_bus_reset_handler(struct scsi_cmnd *cmd);

extern void sas_target_destroy(struct scsi_target *);
extern int sas_slave_alloc(struct scsi_device *);
extern int sas_ioctl(struct scsi_device *sdev, int cmd, void __user *arg);
extern int sas_drain_work(struct sas_ha_struct *ha);

extern int sas_smp_handler(struct Scsi_Host *shost, struct sas_rphy *rphy,
			   struct request *req);

extern void sas_ssp_task_response(struct device *dev, struct sas_task *task,
				  struct ssp_response_iu *iu);
struct sas_phy *sas_get_local_phy(struct domain_device *dev);

int sas_request_addr(struct Scsi_Host *shost, u8 *addr);

#endif 
