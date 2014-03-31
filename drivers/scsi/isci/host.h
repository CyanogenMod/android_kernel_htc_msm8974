/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _SCI_HOST_H_
#define _SCI_HOST_H_

#include "remote_device.h"
#include "phy.h"
#include "isci.h"
#include "remote_node_table.h"
#include "registers.h"
#include "unsolicited_frame_control.h"
#include "probe_roms.h"

struct isci_request;
struct scu_task_context;


struct sci_power_control {
	bool timer_started;

	struct sci_timer timer;

	u8 phys_waiting;

	u8 phys_granted_power;

	struct isci_phy *requesters[SCI_MAX_PHYS];

};

struct sci_port_configuration_agent;
typedef void (*port_config_fn)(struct isci_host *,
			       struct sci_port_configuration_agent *,
			       struct isci_port *, struct isci_phy *);

struct sci_port_configuration_agent {
	u16 phy_configured_mask;
	u16 phy_ready_mask;
	struct {
		u8 min_index;
		u8 max_index;
	} phy_valid_port_range[SCI_MAX_PHYS];
	bool timer_pending;
	port_config_fn link_up_handler;
	port_config_fn link_down_handler;
	struct sci_timer	timer;
};

struct isci_host {
	struct sci_base_state_machine sm;
	
	struct sci_timer timer;
	
	struct sci_user_parameters user_parameters;
	
	struct sci_oem_params oem_parameters;
	struct sci_port_configuration_agent port_agent;
	struct isci_remote_device *device_table[SCI_MAX_REMOTE_DEVICES];
	struct sci_remote_node_table available_remote_nodes;
	struct sci_power_control power_control;
	u8 io_request_sequence[SCI_MAX_IO_REQUESTS];
	struct scu_task_context *task_context_table;
	dma_addr_t task_context_dma;
	union scu_remote_node_context *remote_node_context_table;
	u32 *completion_queue;
	u32 completion_queue_get;
	u32 logical_port_entries;
	u32 remote_node_entries;
	u32 task_context_entries;
	struct sci_unsolicited_frame_control uf_control;

	
	struct sci_timer phy_timer;
	
	bool phy_startup_timer_pending;
	u32 next_phy_to_start;
	
	u8 invalid_phy_mask;

	
	u16 interrupt_coalesce_number;
	u32 interrupt_coalesce_timeout;
	struct smu_registers __iomem *smu_registers;
	struct scu_registers __iomem *scu_registers;

	u16 tci_head;
	u16 tci_tail;
	u16 tci_pool[SCI_MAX_IO_REQUESTS];

	int id; 
	struct isci_phy phys[SCI_MAX_PHYS];
	struct isci_port ports[SCI_MAX_PORTS + 1]; 
	struct asd_sas_port sas_ports[SCI_MAX_PORTS];
	struct sas_ha_struct sas_ha;

	spinlock_t state_lock;
	struct pci_dev *pdev;
	enum isci_status status;
	#define IHOST_START_PENDING 0
	#define IHOST_STOP_PENDING 1
	unsigned long flags;
	wait_queue_head_t eventq;
	struct Scsi_Host *shost;
	struct tasklet_struct completion_tasklet;
	struct list_head requests_to_complete;
	struct list_head requests_to_errorback;
	spinlock_t scic_lock;
	struct isci_request *reqs[SCI_MAX_IO_REQUESTS];
	struct isci_remote_device devices[SCI_MAX_REMOTE_DEVICES];
};

enum sci_controller_states {
	SCIC_INITIAL = 0,

	SCIC_RESET,

	SCIC_INITIALIZING,

	SCIC_INITIALIZED,

	SCIC_STARTING,

	SCIC_READY,

	SCIC_RESETTING,

	SCIC_STOPPING,

	SCIC_STOPPED,

	SCIC_FAILED,
};

#define SCI_MAX_MSIX_INT (SCI_NUM_MSI_X_INT*SCI_MAX_CONTROLLERS)

struct isci_pci_info {
	struct msix_entry msix_entries[SCI_MAX_MSIX_INT];
	struct isci_host *hosts[SCI_MAX_CONTROLLERS];
	struct isci_orom *orom;
};

static inline struct isci_pci_info *to_pci_info(struct pci_dev *pdev)
{
	return pci_get_drvdata(pdev);
}

#define for_each_isci_host(id, ihost, pdev) \
	for (id = 0, ihost = to_pci_info(pdev)->hosts[id]; \
	     id < ARRAY_SIZE(to_pci_info(pdev)->hosts) && ihost; \
	     ihost = to_pci_info(pdev)->hosts[++id])

static inline enum isci_status isci_host_get_state(struct isci_host *isci_host)
{
	return isci_host->status;
}

static inline void isci_host_change_state(struct isci_host *isci_host,
					  enum isci_status status)
{
	unsigned long flags;

	dev_dbg(&isci_host->pdev->dev,
		"%s: isci_host = %p, state = 0x%x",
		__func__,
		isci_host,
		status);
	spin_lock_irqsave(&isci_host->state_lock, flags);
	isci_host->status = status;
	spin_unlock_irqrestore(&isci_host->state_lock, flags);

}

static inline void wait_for_start(struct isci_host *ihost)
{
	wait_event(ihost->eventq, !test_bit(IHOST_START_PENDING, &ihost->flags));
}

static inline void wait_for_stop(struct isci_host *ihost)
{
	wait_event(ihost->eventq, !test_bit(IHOST_STOP_PENDING, &ihost->flags));
}

static inline void wait_for_device_start(struct isci_host *ihost, struct isci_remote_device *idev)
{
	wait_event(ihost->eventq, !test_bit(IDEV_START_PENDING, &idev->flags));
}

static inline void wait_for_device_stop(struct isci_host *ihost, struct isci_remote_device *idev)
{
	wait_event(ihost->eventq, !test_bit(IDEV_STOP_PENDING, &idev->flags));
}

static inline struct isci_host *dev_to_ihost(struct domain_device *dev)
{
	return dev->port->ha->lldd_ha;
}

#define ISCI_PEG 0

#define ISCI_TAG(seq, tci) (((u16) (seq)) << 12 | tci)

#define ISCI_TAG_SEQ(tag) (((tag) >> 12) & (SCI_MAX_SEQ-1))
#define ISCI_TAG_TCI(tag) ((tag) & (SCI_MAX_IO_REQUESTS-1))

#define ISCI_COALESCE_BASE 9

static inline int sci_remote_device_node_count(struct isci_remote_device *idev)
{
	struct domain_device *dev = idev->domain_dev;

	if ((dev->dev_type == SATA_DEV || (dev->tproto & SAS_PROTOCOL_STP)) &&
	    !idev->is_direct_attached)
		return SCU_STP_REMOTE_NODE_COUNT;
	return SCU_SSP_REMOTE_NODE_COUNT;
}

#define sci_controller_clear_invalid_phy(controller, phy) \
	((controller)->invalid_phy_mask &= ~(1 << (phy)->phy_index))

static inline struct device *scirdev_to_dev(struct isci_remote_device *idev)
{
	if (!idev || !idev->isci_port || !idev->isci_port->isci_host)
		return NULL;

	return &idev->isci_port->isci_host->pdev->dev;
}

static inline bool is_a2(struct pci_dev *pdev)
{
	if (pdev->revision < 4)
		return true;
	return false;
}

static inline bool is_b0(struct pci_dev *pdev)
{
	if (pdev->revision == 4)
		return true;
	return false;
}

static inline bool is_c0(struct pci_dev *pdev)
{
	if (pdev->revision == 5)
		return true;
	return false;
}

static inline bool is_c1(struct pci_dev *pdev)
{
	if (pdev->revision >= 6)
		return true;
	return false;
}

enum cable_selections {
	short_cable     = 0,
	long_cable      = 1,
	medium_cable    = 2,
	undefined_cable = 3
};

#define CABLE_OVERRIDE_DISABLED (0x10000)

static inline int is_cable_select_overridden(void)
{
	return cable_selection_override < CABLE_OVERRIDE_DISABLED;
}

enum cable_selections decode_cable_selection(struct isci_host *ihost, int phy);
void validate_cable_selections(struct isci_host *ihost);
char *lookup_cable_names(enum cable_selections);

#define SGPIO_HW_CONTROL 0x00000443

static inline int isci_gpio_count(struct isci_host *ihost)
{
	return ARRAY_SIZE(ihost->scu_registers->peg0.sgpio.output_data_select);
}

void sci_controller_post_request(struct isci_host *ihost,
				      u32 request);
void sci_controller_release_frame(struct isci_host *ihost,
				       u32 frame_index);
void sci_controller_copy_sata_response(void *response_buffer,
					    void *frame_header,
					    void *frame_buffer);
enum sci_status sci_controller_allocate_remote_node_context(struct isci_host *ihost,
								 struct isci_remote_device *idev,
								 u16 *node_id);
void sci_controller_free_remote_node_context(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	u16 node_id);

struct isci_request *sci_request_by_tag(struct isci_host *ihost,
					     u16 io_tag);

void sci_controller_power_control_queue_insert(
	struct isci_host *ihost,
	struct isci_phy *iphy);

void sci_controller_power_control_queue_remove(
	struct isci_host *ihost,
	struct isci_phy *iphy);

void sci_controller_link_up(
	struct isci_host *ihost,
	struct isci_port *iport,
	struct isci_phy *iphy);

void sci_controller_link_down(
	struct isci_host *ihost,
	struct isci_port *iport,
	struct isci_phy *iphy);

void sci_controller_remote_device_stopped(
	struct isci_host *ihost,
	struct isci_remote_device *idev);

void sci_controller_copy_task_context(
	struct isci_host *ihost,
	struct isci_request *ireq);

void sci_controller_register_setup(struct isci_host *ihost);

enum sci_status sci_controller_continue_io(struct isci_request *ireq);
int isci_host_scan_finished(struct Scsi_Host *, unsigned long);
void isci_host_scan_start(struct Scsi_Host *);
u16 isci_alloc_tag(struct isci_host *ihost);
enum sci_status isci_free_tag(struct isci_host *ihost, u16 io_tag);
void isci_tci_free(struct isci_host *ihost, u16 tci);

int isci_host_init(struct isci_host *);

void isci_host_init_controller_names(
	struct isci_host *isci_host,
	unsigned int controller_idx);

void isci_host_deinit(
	struct isci_host *);

void isci_host_port_link_up(
	struct isci_host *,
	struct isci_port *,
	struct isci_phy *);
int isci_host_dev_found(struct domain_device *);

void isci_host_remote_device_start_complete(
	struct isci_host *,
	struct isci_remote_device *,
	enum sci_status);

void sci_controller_disable_interrupts(
	struct isci_host *ihost);

enum sci_status sci_controller_start_io(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);

enum sci_task_status sci_controller_start_task(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);

enum sci_status sci_controller_terminate_request(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);

enum sci_status sci_controller_complete_io(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);

void sci_port_configuration_agent_construct(
	struct sci_port_configuration_agent *port_agent);

enum sci_status sci_port_configuration_agent_initialize(
	struct isci_host *ihost,
	struct sci_port_configuration_agent *port_agent);

int isci_gpio_write(struct sas_ha_struct *, u8 reg_type, u8 reg_index,
		    u8 reg_count, u8 *write_data);
#endif
