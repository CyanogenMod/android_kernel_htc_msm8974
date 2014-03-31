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
#ifndef _ISCI_PHY_H_
#define _ISCI_PHY_H_

#include <scsi/sas.h>
#include <scsi/libsas.h>
#include "isci.h"
#include "sas.h"

#define SCIC_SDS_SIGNATURE_FIS_TIMEOUT    25000

#define SCIC_SDS_SATA_LINK_TRAINING_TIMEOUT  250

enum sci_phy_protocol {
	SCIC_SDS_PHY_PROTOCOL_UNKNOWN,
	SCIC_SDS_PHY_PROTOCOL_SAS,
	SCIC_SDS_PHY_PROTOCOL_SATA,
	SCIC_SDS_MAX_PHY_PROTOCOLS
};

struct isci_phy {
	struct sci_base_state_machine sm;
	struct isci_port *owning_port;
	enum sas_linkrate max_negotiated_speed;
	enum sci_phy_protocol protocol;
	u8 phy_index;
	bool bcn_received_while_port_unassigned;
	bool is_in_link_training;
	struct sci_timer sata_timer;
	struct scu_transport_layer_registers __iomem *transport_layer_registers;
	struct scu_link_layer_registers __iomem *link_layer_registers;
	struct asd_sas_phy sas_phy;
	u8 sas_addr[SAS_ADDR_SIZE];
	union {
		struct sas_identify_frame iaf;
		struct dev_to_host_fis fis;
	} frame_rcvd;
};

static inline struct isci_phy *to_iphy(struct asd_sas_phy *sas_phy)
{
	struct isci_phy *iphy = container_of(sas_phy, typeof(*iphy), sas_phy);

	return iphy;
}

struct sci_phy_cap {
	union {
		struct {
			u8 start:1;
			u8 tx_ssc_type:1;
			u8 res1:2;
			u8 req_logical_linkrate:4;

			u32 gen1_no_ssc:1;
			u32 gen1_ssc:1;
			u32 gen2_no_ssc:1;
			u32 gen2_ssc:1;
			u32 gen3_no_ssc:1;
			u32 gen3_ssc:1;
			u32 res2:17;
			u32 parity:1;
		};
		u32 all;
	};
}  __packed;

struct sci_phy_proto {
	union {
		struct {
			u16 _r_a:1;
			u16 smp_iport:1;
			u16 stp_iport:1;
			u16 ssp_iport:1;
			u16 _r_b:4;
			u16 _r_c:1;
			u16 smp_tport:1;
			u16 stp_tport:1;
			u16 ssp_tport:1;
			u16 _r_d:4;
		};
		u16 all;
	};
} __packed;


struct sci_phy_properties {
	struct isci_port *iport;

	enum sas_linkrate negotiated_link_rate;

	u8 index;
};

struct sci_sas_phy_properties {
	struct sas_identify_frame rcvd_iaf;

	struct sci_phy_cap rcvd_cap;

};

struct sci_sata_phy_properties {
	struct dev_to_host_fis signature_fis;

	bool is_port_selector_present;

};

enum sci_phy_counter_id {
	SCIC_PHY_COUNTER_RECEIVED_FRAME,

	SCIC_PHY_COUNTER_TRANSMITTED_FRAME,

	SCIC_PHY_COUNTER_RECEIVED_FRAME_WORD,

	SCIC_PHY_COUNTER_TRANSMITTED_FRAME_DWORD,

	SCIC_PHY_COUNTER_LOSS_OF_SYNC_ERROR,

	SCIC_PHY_COUNTER_RECEIVED_DISPARITY_ERROR,

	SCIC_PHY_COUNTER_RECEIVED_FRAME_CRC_ERROR,

	SCIC_PHY_COUNTER_RECEIVED_DONE_ACK_NAK_TIMEOUT,

	SCIC_PHY_COUNTER_TRANSMITTED_DONE_ACK_NAK_TIMEOUT,

	SCIC_PHY_COUNTER_INACTIVITY_TIMER_EXPIRED,

	SCIC_PHY_COUNTER_RECEIVED_DONE_CREDIT_TIMEOUT,

	SCIC_PHY_COUNTER_TRANSMITTED_DONE_CREDIT_TIMEOUT,

	SCIC_PHY_COUNTER_RECEIVED_CREDIT_BLOCKED,

	SCIC_PHY_COUNTER_RECEIVED_SHORT_FRAME,

	SCIC_PHY_COUNTER_RECEIVED_FRAME_WITHOUT_CREDIT,

	SCIC_PHY_COUNTER_RECEIVED_FRAME_AFTER_DONE,

	SCIC_PHY_COUNTER_SN_DWORD_SYNC_ERROR
};

#define PHY_STATES {\
	C(PHY_INITIAL),\
	C(PHY_STOPPED),\
	C(PHY_STARTING),\
	C(PHY_SUB_INITIAL),\
	C(PHY_SUB_AWAIT_OSSP_EN),\
	C(PHY_SUB_AWAIT_SAS_SPEED_EN),\
	C(PHY_SUB_AWAIT_IAF_UF),\
	C(PHY_SUB_AWAIT_SAS_POWER),\
	C(PHY_SUB_AWAIT_SATA_POWER),\
	C(PHY_SUB_AWAIT_SATA_PHY_EN),\
	C(PHY_SUB_AWAIT_SATA_SPEED_EN),\
	C(PHY_SUB_AWAIT_SIG_FIS_UF),\
	C(PHY_SUB_FINAL),\
	C(PHY_READY),\
	C(PHY_RESETTING),\
	C(PHY_FINAL),\
	}
#undef C
#define C(a) SCI_##a
enum sci_phy_states PHY_STATES;
#undef C

void sci_phy_construct(
	struct isci_phy *iphy,
	struct isci_port *iport,
	u8 phy_index);

struct isci_port *phy_get_non_dummy_port(struct isci_phy *iphy);

void sci_phy_set_port(
	struct isci_phy *iphy,
	struct isci_port *iport);

enum sci_status sci_phy_initialize(
	struct isci_phy *iphy,
	struct scu_transport_layer_registers __iomem *transport_layer_registers,
	struct scu_link_layer_registers __iomem *link_layer_registers);

enum sci_status sci_phy_start(
	struct isci_phy *iphy);

enum sci_status sci_phy_stop(
	struct isci_phy *iphy);

enum sci_status sci_phy_reset(
	struct isci_phy *iphy);

void sci_phy_resume(
	struct isci_phy *iphy);

void sci_phy_setup_transport(
	struct isci_phy *iphy,
	u32 device_id);

enum sci_status sci_phy_event_handler(
	struct isci_phy *iphy,
	u32 event_code);

enum sci_status sci_phy_frame_handler(
	struct isci_phy *iphy,
	u32 frame_index);

enum sci_status sci_phy_consume_power_handler(
	struct isci_phy *iphy);

void sci_phy_get_sas_address(
	struct isci_phy *iphy,
	struct sci_sas_address *sas_address);

void sci_phy_get_attached_sas_address(
	struct isci_phy *iphy,
	struct sci_sas_address *sas_address);

struct sci_phy_proto;
void sci_phy_get_protocols(
	struct isci_phy *iphy,
	struct sci_phy_proto *protocols);
enum sas_linkrate sci_phy_linkrate(struct isci_phy *iphy);

struct isci_host;
void isci_phy_init(struct isci_phy *iphy, struct isci_host *ihost, int index);
int isci_phy_control(struct asd_sas_phy *phy, enum phy_func func, void *buf);

#endif 
