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

#include "isci.h"
#include "host.h"
#include "phy.h"
#include "scu_event_codes.h"
#include "probe_roms.h"

#undef C
#define C(a) (#a)
static const char *phy_state_name(enum sci_phy_states state)
{
	static const char * const strings[] = PHY_STATES;

	return strings[state];
}
#undef C

#define SCIC_SDS_PHY_MAX_ARBITRATION_WAIT_TIME  (700)

enum sas_linkrate sci_phy_linkrate(struct isci_phy *iphy)
{
	return iphy->max_negotiated_speed;
}

static struct isci_host *phy_to_host(struct isci_phy *iphy)
{
	struct isci_phy *table = iphy - iphy->phy_index;
	struct isci_host *ihost = container_of(table, typeof(*ihost), phys[0]);

	return ihost;
}

static struct device *sciphy_to_dev(struct isci_phy *iphy)
{
	return &phy_to_host(iphy)->pdev->dev;
}

static enum sci_status
sci_phy_transport_layer_initialization(struct isci_phy *iphy,
				       struct scu_transport_layer_registers __iomem *reg)
{
	u32 tl_control;

	iphy->transport_layer_registers = reg;

	writel(SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX,
		&iphy->transport_layer_registers->stp_rni);

	tl_control = readl(&iphy->transport_layer_registers->control);
	tl_control |= SCU_TLCR_GEN_BIT(STP_WRITE_DATA_PREFETCH);
	writel(tl_control, &iphy->transport_layer_registers->control);

	return SCI_SUCCESS;
}

static enum sci_status
sci_phy_link_layer_initialization(struct isci_phy *iphy,
				  struct scu_link_layer_registers __iomem *llr)
{
	struct isci_host *ihost = iphy->owning_port->owning_controller;
	struct sci_phy_user_params *phy_user;
	struct sci_phy_oem_params *phy_oem;
	int phy_idx = iphy->phy_index;
	struct sci_phy_cap phy_cap;
	u32 phy_configuration;
	u32 parity_check = 0;
	u32 parity_count = 0;
	u32 llctl, link_rate;
	u32 clksm_value = 0;
	u32 sp_timeouts = 0;

	phy_user = &ihost->user_parameters.phys[phy_idx];
	phy_oem = &ihost->oem_parameters.phys[phy_idx];
	iphy->link_layer_registers = llr;

	
	#define SCI_END_DEVICE 0x01

	writel(SCU_SAS_TIID_GEN_BIT(SMP_INITIATOR) |
	       SCU_SAS_TIID_GEN_BIT(SSP_INITIATOR) |
	       SCU_SAS_TIID_GEN_BIT(STP_INITIATOR) |
	       SCU_SAS_TIID_GEN_BIT(DA_SATA_HOST) |
	       SCU_SAS_TIID_GEN_VAL(DEVICE_TYPE, SCI_END_DEVICE),
	       &llr->transmit_identification);

	
	writel(0xFEDCBA98, &llr->sas_device_name_high);
	writel(phy_idx, &llr->sas_device_name_low);

	
	writel(phy_oem->sas_address.high, &llr->source_sas_address_high);
	writel(phy_oem->sas_address.low, &llr->source_sas_address_low);

	
	writel(0, &llr->identify_frame_phy_id);
	writel(SCU_SAS_TIPID_GEN_VALUE(ID, phy_idx), &llr->identify_frame_phy_id);

	
	phy_configuration = readl(&llr->phy_configuration);

	
	phy_configuration |=  SCU_SAS_PCFG_GEN_BIT(OOB_RESET);
	writel(phy_configuration, &llr->phy_configuration);

	
	phy_cap.all = 0;
	phy_cap.start = 1;
	phy_cap.gen3_no_ssc = 1;
	phy_cap.gen2_no_ssc = 1;
	phy_cap.gen1_no_ssc = 1;
	if (ihost->oem_parameters.controller.do_enable_ssc) {
		struct scu_afe_registers __iomem *afe = &ihost->scu_registers->afe;
		struct scu_afe_transceiver *xcvr = &afe->scu_afe_xcvr[phy_idx];
		struct isci_pci_info *pci_info = to_pci_info(ihost->pdev);
		bool en_sas = false;
		bool en_sata = false;
		u32 sas_type = 0;
		u32 sata_spread = 0x2;
		u32 sas_spread = 0x2;

		phy_cap.gen3_ssc = 1;
		phy_cap.gen2_ssc = 1;
		phy_cap.gen1_ssc = 1;

		if (pci_info->orom->hdr.version < ISCI_ROM_VER_1_1)
			en_sas = en_sata = true;
		else {
			sata_spread = ihost->oem_parameters.controller.ssc_sata_tx_spread_level;
			sas_spread = ihost->oem_parameters.controller.ssc_sas_tx_spread_level;

			if (sata_spread)
				en_sata = true;

			if (sas_spread) {
				en_sas = true;
				sas_type = ihost->oem_parameters.controller.ssc_sas_tx_type;
			}

		}

		if (en_sas) {
			u32 reg;

			reg = readl(&xcvr->afe_xcvr_control0);
			reg |= (0x00100000 | (sas_type << 19));
			writel(reg, &xcvr->afe_xcvr_control0);

			reg = readl(&xcvr->afe_tx_ssc_control);
			reg |= sas_spread << 8;
			writel(reg, &xcvr->afe_tx_ssc_control);
		}

		if (en_sata) {
			u32 reg;

			reg = readl(&xcvr->afe_tx_ssc_control);
			reg |= sata_spread;
			writel(reg, &xcvr->afe_tx_ssc_control);

			reg = readl(&llr->stp_control);
			reg |= 1 << 12;
			writel(reg, &llr->stp_control);
		}
	}

	parity_check = phy_cap.all;
	while (parity_check != 0) {
		if (parity_check & 0x1)
			parity_count++;
		parity_check >>= 1;
	}

	if ((parity_count % 2) != 0)
		phy_cap.parity = 1;

	writel(phy_cap.all, &llr->phy_capabilities);

	writel(SCU_ENSPINUP_GEN_VAL(COUNT,
			phy_user->notify_enable_spin_up_insertion_frequency),
		&llr->notify_enable_spinup_control);

	clksm_value = SCU_ALIGN_INSERTION_FREQUENCY_GEN_VAL(CONNECTED,
			phy_user->in_connection_align_insertion_frequency);

	clksm_value |= SCU_ALIGN_INSERTION_FREQUENCY_GEN_VAL(GENERAL,
			phy_user->align_insertion_frequency);

	writel(clksm_value, &llr->clock_skew_management);

	if (is_c0(ihost->pdev) || is_c1(ihost->pdev)) {
		writel(0x04210400, &llr->afe_lookup_table_control);
		writel(0x020A7C05, &llr->sas_primitive_timeout);
	} else
		writel(0x02108421, &llr->afe_lookup_table_control);

	llctl = SCU_SAS_LLCTL_GEN_VAL(NO_OUTBOUND_TASK_TIMEOUT,
		(u8)ihost->user_parameters.no_outbound_task_timeout);

	switch (phy_user->max_speed_generation) {
	case SCIC_SDS_PARM_GEN3_SPEED:
		link_rate = SCU_SAS_LINK_LAYER_CONTROL_MAX_LINK_RATE_GEN3;
		break;
	case SCIC_SDS_PARM_GEN2_SPEED:
		link_rate = SCU_SAS_LINK_LAYER_CONTROL_MAX_LINK_RATE_GEN2;
		break;
	default:
		link_rate = SCU_SAS_LINK_LAYER_CONTROL_MAX_LINK_RATE_GEN1;
		break;
	}
	llctl |= SCU_SAS_LLCTL_GEN_VAL(MAX_LINK_RATE, link_rate);
	writel(llctl, &llr->link_layer_control);

	sp_timeouts = readl(&llr->sas_phy_timeouts);

	
	sp_timeouts &= ~SCU_SAS_PHYTOV_GEN_VAL(RATE_CHANGE, 0xFF);

	sp_timeouts |= SCU_SAS_PHYTOV_GEN_VAL(RATE_CHANGE, 0x3B);

	writel(sp_timeouts, &llr->sas_phy_timeouts);

	if (is_a2(ihost->pdev)) {
		writel(SCIC_SDS_PHY_MAX_ARBITRATION_WAIT_TIME,
		       &llr->maximum_arbitration_wait_timer_timeout);
	}

	writel(0, &llr->link_layer_hang_detection_timeout);

	
	sci_change_state(&iphy->sm, SCI_PHY_STOPPED);

	return SCI_SUCCESS;
}

static void phy_sata_timeout(unsigned long data)
{
	struct sci_timer *tmr = (struct sci_timer *)data;
	struct isci_phy *iphy = container_of(tmr, typeof(*iphy), sata_timer);
	struct isci_host *ihost = iphy->owning_port->owning_controller;
	unsigned long flags;

	spin_lock_irqsave(&ihost->scic_lock, flags);

	if (tmr->cancel)
		goto done;

	dev_dbg(sciphy_to_dev(iphy),
		 "%s: SCIC SDS Phy 0x%p did not receive signature fis before "
		 "timeout.\n",
		 __func__,
		 iphy);

	sci_change_state(&iphy->sm, SCI_PHY_STARTING);
done:
	spin_unlock_irqrestore(&ihost->scic_lock, flags);
}

struct isci_port *phy_get_non_dummy_port(struct isci_phy *iphy)
{
	struct isci_port *iport = iphy->owning_port;

	if (iport->physical_port_index == SCIC_SDS_DUMMY_PORT)
		return NULL;

	return iphy->owning_port;
}

void sci_phy_set_port(
	struct isci_phy *iphy,
	struct isci_port *iport)
{
	iphy->owning_port = iport;

	if (iphy->bcn_received_while_port_unassigned) {
		iphy->bcn_received_while_port_unassigned = false;
		sci_port_broadcast_change_received(iphy->owning_port, iphy);
	}
}

enum sci_status sci_phy_initialize(struct isci_phy *iphy,
				   struct scu_transport_layer_registers __iomem *tl,
				   struct scu_link_layer_registers __iomem *ll)
{
	
	sci_phy_transport_layer_initialization(iphy, tl);

	
	sci_phy_link_layer_initialization(iphy, ll);

	sci_change_state(&iphy->sm, SCI_PHY_STOPPED);

	return SCI_SUCCESS;
}

void sci_phy_setup_transport(struct isci_phy *iphy, u32 device_id)
{
	u32 tl_control;

	writel(device_id, &iphy->transport_layer_registers->stp_rni);

	tl_control = readl(&iphy->transport_layer_registers->control);
	tl_control |= SCU_TLCR_GEN_BIT(CLEAR_TCI_NCQ_MAPPING_TABLE);
	writel(tl_control, &iphy->transport_layer_registers->control);
}

static void sci_phy_suspend(struct isci_phy *iphy)
{
	u32 scu_sas_pcfg_value;

	scu_sas_pcfg_value =
		readl(&iphy->link_layer_registers->phy_configuration);
	scu_sas_pcfg_value |= SCU_SAS_PCFG_GEN_BIT(SUSPEND_PROTOCOL_ENGINE);
	writel(scu_sas_pcfg_value,
		&iphy->link_layer_registers->phy_configuration);

	sci_phy_setup_transport(iphy, SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX);
}

void sci_phy_resume(struct isci_phy *iphy)
{
	u32 scu_sas_pcfg_value;

	scu_sas_pcfg_value =
		readl(&iphy->link_layer_registers->phy_configuration);
	scu_sas_pcfg_value &= ~SCU_SAS_PCFG_GEN_BIT(SUSPEND_PROTOCOL_ENGINE);
	writel(scu_sas_pcfg_value,
		&iphy->link_layer_registers->phy_configuration);
}

void sci_phy_get_sas_address(struct isci_phy *iphy, struct sci_sas_address *sas)
{
	sas->high = readl(&iphy->link_layer_registers->source_sas_address_high);
	sas->low = readl(&iphy->link_layer_registers->source_sas_address_low);
}

void sci_phy_get_attached_sas_address(struct isci_phy *iphy, struct sci_sas_address *sas)
{
	struct sas_identify_frame *iaf;

	iaf = &iphy->frame_rcvd.iaf;
	memcpy(sas, iaf->sas_addr, SAS_ADDR_SIZE);
}

void sci_phy_get_protocols(struct isci_phy *iphy, struct sci_phy_proto *proto)
{
	proto->all = readl(&iphy->link_layer_registers->transmit_identification);
}

enum sci_status sci_phy_start(struct isci_phy *iphy)
{
	enum sci_phy_states state = iphy->sm.current_state_id;

	if (state != SCI_PHY_STOPPED) {
		dev_dbg(sciphy_to_dev(iphy), "%s: in wrong state: %s\n",
			__func__, phy_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	}

	sci_change_state(&iphy->sm, SCI_PHY_STARTING);
	return SCI_SUCCESS;
}

enum sci_status sci_phy_stop(struct isci_phy *iphy)
{
	enum sci_phy_states state = iphy->sm.current_state_id;

	switch (state) {
	case SCI_PHY_SUB_INITIAL:
	case SCI_PHY_SUB_AWAIT_OSSP_EN:
	case SCI_PHY_SUB_AWAIT_SAS_SPEED_EN:
	case SCI_PHY_SUB_AWAIT_SAS_POWER:
	case SCI_PHY_SUB_AWAIT_SATA_POWER:
	case SCI_PHY_SUB_AWAIT_SATA_PHY_EN:
	case SCI_PHY_SUB_AWAIT_SATA_SPEED_EN:
	case SCI_PHY_SUB_AWAIT_SIG_FIS_UF:
	case SCI_PHY_SUB_FINAL:
	case SCI_PHY_READY:
		break;
	default:
		dev_dbg(sciphy_to_dev(iphy), "%s: in wrong state: %s\n",
			__func__, phy_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	}

	sci_change_state(&iphy->sm, SCI_PHY_STOPPED);
	return SCI_SUCCESS;
}

enum sci_status sci_phy_reset(struct isci_phy *iphy)
{
	enum sci_phy_states state = iphy->sm.current_state_id;

	if (state != SCI_PHY_READY) {
		dev_dbg(sciphy_to_dev(iphy), "%s: in wrong state: %s\n",
			__func__, phy_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	}

	sci_change_state(&iphy->sm, SCI_PHY_RESETTING);
	return SCI_SUCCESS;
}

enum sci_status sci_phy_consume_power_handler(struct isci_phy *iphy)
{
	enum sci_phy_states state = iphy->sm.current_state_id;

	switch (state) {
	case SCI_PHY_SUB_AWAIT_SAS_POWER: {
		u32 enable_spinup;

		enable_spinup = readl(&iphy->link_layer_registers->notify_enable_spinup_control);
		enable_spinup |= SCU_ENSPINUP_GEN_BIT(ENABLE);
		writel(enable_spinup, &iphy->link_layer_registers->notify_enable_spinup_control);

		
		sci_change_state(&iphy->sm, SCI_PHY_SUB_FINAL);

		return SCI_SUCCESS;
	}
	case SCI_PHY_SUB_AWAIT_SATA_POWER: {
		u32 scu_sas_pcfg_value;

		
		scu_sas_pcfg_value =
			readl(&iphy->link_layer_registers->phy_configuration);
		scu_sas_pcfg_value &=
			~(SCU_SAS_PCFG_GEN_BIT(SATA_SPINUP_HOLD) | SCU_SAS_PCFG_GEN_BIT(OOB_ENABLE));
		scu_sas_pcfg_value |= SCU_SAS_PCFG_GEN_BIT(OOB_RESET);
		writel(scu_sas_pcfg_value,
			&iphy->link_layer_registers->phy_configuration);

		
		scu_sas_pcfg_value &= ~SCU_SAS_PCFG_GEN_BIT(OOB_RESET);
		scu_sas_pcfg_value |= SCU_SAS_PCFG_GEN_BIT(OOB_ENABLE);
		writel(scu_sas_pcfg_value,
			&iphy->link_layer_registers->phy_configuration);

		
		sci_change_state(&iphy->sm, SCI_PHY_SUB_AWAIT_SATA_PHY_EN);

		return SCI_SUCCESS;
	}
	default:
		dev_dbg(sciphy_to_dev(iphy), "%s: in wrong state: %s\n",
			__func__, phy_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	}
}

static void sci_phy_start_sas_link_training(struct isci_phy *iphy)
{
	u32 phy_control;

	phy_control = readl(&iphy->link_layer_registers->phy_configuration);
	phy_control |= SCU_SAS_PCFG_GEN_BIT(SATA_SPINUP_HOLD);
	writel(phy_control,
	       &iphy->link_layer_registers->phy_configuration);

	sci_change_state(&iphy->sm, SCI_PHY_SUB_AWAIT_SAS_SPEED_EN);

	iphy->protocol = SCIC_SDS_PHY_PROTOCOL_SAS;
}

static void sci_phy_start_sata_link_training(struct isci_phy *iphy)
{
	sci_change_state(&iphy->sm, SCI_PHY_SUB_AWAIT_SATA_POWER);

	iphy->protocol = SCIC_SDS_PHY_PROTOCOL_SATA;
}

static void sci_phy_complete_link_training(struct isci_phy *iphy,
					   enum sas_linkrate max_link_rate,
					   u32 next_state)
{
	iphy->max_negotiated_speed = max_link_rate;

	sci_change_state(&iphy->sm, next_state);
}

static const char *phy_event_name(u32 event_code)
{
	switch (scu_get_event_code(event_code)) {
	case SCU_EVENT_PORT_SELECTOR_DETECTED:
		return "port selector";
	case SCU_EVENT_SENT_PORT_SELECTION:
		return "port selection";
	case SCU_EVENT_HARD_RESET_TRANSMITTED:
		return "tx hard reset";
	case SCU_EVENT_HARD_RESET_RECEIVED:
		return "rx hard reset";
	case SCU_EVENT_RECEIVED_IDENTIFY_TIMEOUT:
		return "identify timeout";
	case SCU_EVENT_LINK_FAILURE:
		return "link fail";
	case SCU_EVENT_SATA_SPINUP_HOLD:
		return "sata spinup hold";
	case SCU_EVENT_SAS_15_SSC:
	case SCU_EVENT_SAS_15:
		return "sas 1.5";
	case SCU_EVENT_SAS_30_SSC:
	case SCU_EVENT_SAS_30:
		return "sas 3.0";
	case SCU_EVENT_SAS_60_SSC:
	case SCU_EVENT_SAS_60:
		return "sas 6.0";
	case SCU_EVENT_SATA_15_SSC:
	case SCU_EVENT_SATA_15:
		return "sata 1.5";
	case SCU_EVENT_SATA_30_SSC:
	case SCU_EVENT_SATA_30:
		return "sata 3.0";
	case SCU_EVENT_SATA_60_SSC:
	case SCU_EVENT_SATA_60:
		return "sata 6.0";
	case SCU_EVENT_SAS_PHY_DETECTED:
		return "sas detect";
	case SCU_EVENT_SATA_PHY_DETECTED:
		return "sata detect";
	default:
		return "unknown";
	}
}

#define phy_event_dbg(iphy, state, code) \
	dev_dbg(sciphy_to_dev(iphy), "phy-%d:%d: %s event: %s (%x)\n", \
		phy_to_host(iphy)->id, iphy->phy_index, \
		phy_state_name(state), phy_event_name(code), code)

#define phy_event_warn(iphy, state, code) \
	dev_warn(sciphy_to_dev(iphy), "phy-%d:%d: %s event: %s (%x)\n", \
		phy_to_host(iphy)->id, iphy->phy_index, \
		phy_state_name(state), phy_event_name(code), code)

enum sci_status sci_phy_event_handler(struct isci_phy *iphy, u32 event_code)
{
	enum sci_phy_states state = iphy->sm.current_state_id;

	switch (state) {
	case SCI_PHY_SUB_AWAIT_OSSP_EN:
		switch (scu_get_event_code(event_code)) {
		case SCU_EVENT_SAS_PHY_DETECTED:
			sci_phy_start_sas_link_training(iphy);
			iphy->is_in_link_training = true;
			break;
		case SCU_EVENT_SATA_SPINUP_HOLD:
			sci_phy_start_sata_link_training(iphy);
			iphy->is_in_link_training = true;
			break;
		default:
			phy_event_dbg(iphy, state, event_code);
			return SCI_FAILURE;
		}
		return SCI_SUCCESS;
	case SCI_PHY_SUB_AWAIT_SAS_SPEED_EN:
		switch (scu_get_event_code(event_code)) {
		case SCU_EVENT_SAS_PHY_DETECTED:
			break;
		case SCU_EVENT_SAS_15:
		case SCU_EVENT_SAS_15_SSC:
			sci_phy_complete_link_training(iphy, SAS_LINK_RATE_1_5_GBPS,
						       SCI_PHY_SUB_AWAIT_IAF_UF);
			break;
		case SCU_EVENT_SAS_30:
		case SCU_EVENT_SAS_30_SSC:
			sci_phy_complete_link_training(iphy, SAS_LINK_RATE_3_0_GBPS,
						       SCI_PHY_SUB_AWAIT_IAF_UF);
			break;
		case SCU_EVENT_SAS_60:
		case SCU_EVENT_SAS_60_SSC:
			sci_phy_complete_link_training(iphy, SAS_LINK_RATE_6_0_GBPS,
						       SCI_PHY_SUB_AWAIT_IAF_UF);
			break;
		case SCU_EVENT_SATA_SPINUP_HOLD:
			sci_phy_start_sata_link_training(iphy);
			break;
		case SCU_EVENT_LINK_FAILURE:
			
			sci_change_state(&iphy->sm, SCI_PHY_STARTING);
			break;
		default:
			phy_event_warn(iphy, state, event_code);
			return SCI_FAILURE;
			break;
		}
		return SCI_SUCCESS;
	case SCI_PHY_SUB_AWAIT_IAF_UF:
		switch (scu_get_event_code(event_code)) {
		case SCU_EVENT_SAS_PHY_DETECTED:
			
			sci_phy_start_sas_link_training(iphy);
			break;
		case SCU_EVENT_SATA_SPINUP_HOLD:
			sci_phy_start_sata_link_training(iphy);
			break;
		case SCU_EVENT_RECEIVED_IDENTIFY_TIMEOUT:
		case SCU_EVENT_LINK_FAILURE:
		case SCU_EVENT_HARD_RESET_RECEIVED:
			
			sci_change_state(&iphy->sm, SCI_PHY_STARTING);
			break;
		default:
			phy_event_warn(iphy, state, event_code);
			return SCI_FAILURE;
		}
		return SCI_SUCCESS;
	case SCI_PHY_SUB_AWAIT_SAS_POWER:
		switch (scu_get_event_code(event_code)) {
		case SCU_EVENT_LINK_FAILURE:
			
			sci_change_state(&iphy->sm, SCI_PHY_STARTING);
			break;
		default:
			phy_event_warn(iphy, state, event_code);
			return SCI_FAILURE;
		}
		return SCI_SUCCESS;
	case SCI_PHY_SUB_AWAIT_SATA_POWER:
		switch (scu_get_event_code(event_code)) {
		case SCU_EVENT_LINK_FAILURE:
			
			sci_change_state(&iphy->sm, SCI_PHY_STARTING);
			break;
		case SCU_EVENT_SATA_SPINUP_HOLD:
			break;

		case SCU_EVENT_SAS_PHY_DETECTED:
			sci_phy_start_sas_link_training(iphy);
			break;

		default:
			phy_event_warn(iphy, state, event_code);
			return SCI_FAILURE;
		}
		return SCI_SUCCESS;
	case SCI_PHY_SUB_AWAIT_SATA_PHY_EN:
		switch (scu_get_event_code(event_code)) {
		case SCU_EVENT_LINK_FAILURE:
			
			sci_change_state(&iphy->sm, SCI_PHY_STARTING);
			break;
		case SCU_EVENT_SATA_SPINUP_HOLD:
			break;
		case SCU_EVENT_SATA_PHY_DETECTED:
			iphy->protocol = SCIC_SDS_PHY_PROTOCOL_SATA;

			
			sci_change_state(&iphy->sm, SCI_PHY_SUB_AWAIT_SATA_SPEED_EN);
			break;
		case SCU_EVENT_SAS_PHY_DETECTED:
			sci_phy_start_sas_link_training(iphy);
			break;
		default:
			phy_event_warn(iphy, state, event_code);
			return SCI_FAILURE;
		}
		return SCI_SUCCESS;
	case SCI_PHY_SUB_AWAIT_SATA_SPEED_EN:
		switch (scu_get_event_code(event_code)) {
		case SCU_EVENT_SATA_PHY_DETECTED:
			break;
		case SCU_EVENT_SATA_15:
		case SCU_EVENT_SATA_15_SSC:
			sci_phy_complete_link_training(iphy, SAS_LINK_RATE_1_5_GBPS,
						       SCI_PHY_SUB_AWAIT_SIG_FIS_UF);
			break;
		case SCU_EVENT_SATA_30:
		case SCU_EVENT_SATA_30_SSC:
			sci_phy_complete_link_training(iphy, SAS_LINK_RATE_3_0_GBPS,
						       SCI_PHY_SUB_AWAIT_SIG_FIS_UF);
			break;
		case SCU_EVENT_SATA_60:
		case SCU_EVENT_SATA_60_SSC:
			sci_phy_complete_link_training(iphy, SAS_LINK_RATE_6_0_GBPS,
						       SCI_PHY_SUB_AWAIT_SIG_FIS_UF);
			break;
		case SCU_EVENT_LINK_FAILURE:
			
			sci_change_state(&iphy->sm, SCI_PHY_STARTING);
			break;
		case SCU_EVENT_SAS_PHY_DETECTED:
			sci_phy_start_sas_link_training(iphy);
			break;
		default:
			phy_event_warn(iphy, state, event_code);
			return SCI_FAILURE;
		}

		return SCI_SUCCESS;
	case SCI_PHY_SUB_AWAIT_SIG_FIS_UF:
		switch (scu_get_event_code(event_code)) {
		case SCU_EVENT_SATA_PHY_DETECTED:
			
			sci_change_state(&iphy->sm, SCI_PHY_SUB_AWAIT_SATA_SPEED_EN);
			break;

		case SCU_EVENT_LINK_FAILURE:
			
			sci_change_state(&iphy->sm, SCI_PHY_STARTING);
			break;

		default:
			phy_event_warn(iphy, state, event_code);
			return SCI_FAILURE;
		}
		return SCI_SUCCESS;
	case SCI_PHY_READY:
		switch (scu_get_event_code(event_code)) {
		case SCU_EVENT_LINK_FAILURE:
			
			sci_change_state(&iphy->sm, SCI_PHY_STARTING);
			break;
		case SCU_EVENT_BROADCAST_CHANGE:
			
			if (phy_get_non_dummy_port(iphy) != NULL)
				sci_port_broadcast_change_received(iphy->owning_port, iphy);
			else
				iphy->bcn_received_while_port_unassigned = true;
			break;
		default:
			phy_event_warn(iphy, state, event_code);
			return SCI_FAILURE_INVALID_STATE;
		}
		return SCI_SUCCESS;
	case SCI_PHY_RESETTING:
		switch (scu_get_event_code(event_code)) {
		case SCU_EVENT_HARD_RESET_TRANSMITTED:
			
			sci_change_state(&iphy->sm, SCI_PHY_STARTING);
			break;
		default:
			phy_event_warn(iphy, state, event_code);
			return SCI_FAILURE_INVALID_STATE;
			break;
		}
		return SCI_SUCCESS;
	default:
		dev_dbg(sciphy_to_dev(iphy), "%s: in wrong state: %s\n",
			__func__, phy_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	}
}

enum sci_status sci_phy_frame_handler(struct isci_phy *iphy, u32 frame_index)
{
	enum sci_phy_states state = iphy->sm.current_state_id;
	struct isci_host *ihost = iphy->owning_port->owning_controller;
	enum sci_status result;
	unsigned long flags;

	switch (state) {
	case SCI_PHY_SUB_AWAIT_IAF_UF: {
		u32 *frame_words;
		struct sas_identify_frame iaf;

		result = sci_unsolicited_frame_control_get_header(&ihost->uf_control,
								  frame_index,
								  (void **)&frame_words);

		if (result != SCI_SUCCESS)
			return result;

		sci_swab32_cpy(&iaf, frame_words, sizeof(iaf) / sizeof(u32));
		if (iaf.frame_type == 0) {
			u32 state;

			spin_lock_irqsave(&iphy->sas_phy.frame_rcvd_lock, flags);
			memcpy(&iphy->frame_rcvd.iaf, &iaf, sizeof(iaf));
			spin_unlock_irqrestore(&iphy->sas_phy.frame_rcvd_lock, flags);
			if (iaf.smp_tport) {
				state = SCI_PHY_SUB_FINAL;
			} else {
				state = SCI_PHY_SUB_AWAIT_SAS_POWER;
			}
			sci_change_state(&iphy->sm, state);
			result = SCI_SUCCESS;
		} else
			dev_warn(sciphy_to_dev(iphy),
				"%s: PHY starting substate machine received "
				"unexpected frame id %x\n",
				__func__, frame_index);

		sci_controller_release_frame(ihost, frame_index);
		return result;
	}
	case SCI_PHY_SUB_AWAIT_SIG_FIS_UF: {
		struct dev_to_host_fis *frame_header;
		u32 *fis_frame_data;

		result = sci_unsolicited_frame_control_get_header(&ihost->uf_control,
								  frame_index,
								  (void **)&frame_header);

		if (result != SCI_SUCCESS)
			return result;

		if ((frame_header->fis_type == FIS_REGD2H) &&
		    !(frame_header->status & ATA_BUSY)) {
			sci_unsolicited_frame_control_get_buffer(&ihost->uf_control,
								 frame_index,
								 (void **)&fis_frame_data);

			spin_lock_irqsave(&iphy->sas_phy.frame_rcvd_lock, flags);
			sci_controller_copy_sata_response(&iphy->frame_rcvd.fis,
							  frame_header,
							  fis_frame_data);
			spin_unlock_irqrestore(&iphy->sas_phy.frame_rcvd_lock, flags);

			
			sci_change_state(&iphy->sm, SCI_PHY_SUB_FINAL);

			result = SCI_SUCCESS;
		} else
			dev_warn(sciphy_to_dev(iphy),
				 "%s: PHY starting substate machine received "
				 "unexpected frame id %x\n",
				 __func__, frame_index);

		
		sci_controller_release_frame(ihost, frame_index);

		return result;
	}
	default:
		dev_dbg(sciphy_to_dev(iphy), "%s: in wrong state: %s\n",
			__func__, phy_state_name(state));
		return SCI_FAILURE_INVALID_STATE;
	}

}

static void sci_phy_starting_initial_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);

	
	sci_change_state(&iphy->sm, SCI_PHY_SUB_AWAIT_OSSP_EN);
}

static void sci_phy_starting_await_sas_power_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);
	struct isci_host *ihost = iphy->owning_port->owning_controller;

	sci_controller_power_control_queue_insert(ihost, iphy);
}

static void sci_phy_starting_await_sas_power_substate_exit(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);
	struct isci_host *ihost = iphy->owning_port->owning_controller;

	sci_controller_power_control_queue_remove(ihost, iphy);
}

static void sci_phy_starting_await_sata_power_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);
	struct isci_host *ihost = iphy->owning_port->owning_controller;

	sci_controller_power_control_queue_insert(ihost, iphy);
}

static void sci_phy_starting_await_sata_power_substate_exit(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);
	struct isci_host *ihost = iphy->owning_port->owning_controller;

	sci_controller_power_control_queue_remove(ihost, iphy);
}

static void sci_phy_starting_await_sata_phy_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);

	sci_mod_timer(&iphy->sata_timer, SCIC_SDS_SATA_LINK_TRAINING_TIMEOUT);
}

static void sci_phy_starting_await_sata_phy_substate_exit(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);

	sci_del_timer(&iphy->sata_timer);
}

static void sci_phy_starting_await_sata_speed_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);

	sci_mod_timer(&iphy->sata_timer, SCIC_SDS_SATA_LINK_TRAINING_TIMEOUT);
}

static void sci_phy_starting_await_sata_speed_substate_exit(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);

	sci_del_timer(&iphy->sata_timer);
}

static void sci_phy_starting_await_sig_fis_uf_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);

	if (sci_port_link_detected(iphy->owning_port, iphy)) {

		sci_phy_resume(iphy);

		sci_mod_timer(&iphy->sata_timer,
			      SCIC_SDS_SIGNATURE_FIS_TIMEOUT);
	} else
		iphy->is_in_link_training = false;
}

static void sci_phy_starting_await_sig_fis_uf_substate_exit(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);

	sci_del_timer(&iphy->sata_timer);
}

static void sci_phy_starting_final_substate_enter(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);

	sci_change_state(&iphy->sm, SCI_PHY_READY);
}

static void scu_link_layer_stop_protocol_engine(
	struct isci_phy *iphy)
{
	u32 scu_sas_pcfg_value;
	u32 enable_spinup_value;

	
	scu_sas_pcfg_value =
		readl(&iphy->link_layer_registers->phy_configuration);
	scu_sas_pcfg_value |=
		(SCU_SAS_PCFG_GEN_BIT(OOB_RESET) |
		 SCU_SAS_PCFG_GEN_BIT(SUSPEND_PROTOCOL_ENGINE) |
		 SCU_SAS_PCFG_GEN_BIT(SATA_SPINUP_HOLD));
	writel(scu_sas_pcfg_value,
	       &iphy->link_layer_registers->phy_configuration);

	
	enable_spinup_value = readl(&iphy->link_layer_registers->notify_enable_spinup_control);
	enable_spinup_value &= ~SCU_ENSPINUP_GEN_BIT(ENABLE);
	writel(enable_spinup_value, &iphy->link_layer_registers->notify_enable_spinup_control);
}

static void scu_link_layer_start_oob(struct isci_phy *iphy)
{
	struct scu_link_layer_registers __iomem *ll = iphy->link_layer_registers;
	u32 val;

	
	val = readl(&ll->phy_configuration);
	val &= ~(SCU_SAS_PCFG_GEN_BIT(OOB_RESET) |
		 SCU_SAS_PCFG_GEN_BIT(HARD_RESET));
	writel(val, &ll->phy_configuration);
	readl(&ll->phy_configuration); 
	

	
	val = readl(&ll->phy_configuration);
	val |= SCU_SAS_PCFG_GEN_BIT(OOB_ENABLE);
	writel(val, &ll->phy_configuration);
	readl(&ll->phy_configuration); 
	
}

static void scu_link_layer_tx_hard_reset(
	struct isci_phy *iphy)
{
	u32 phy_configuration_value;

	phy_configuration_value =
		readl(&iphy->link_layer_registers->phy_configuration);
	phy_configuration_value |=
		(SCU_SAS_PCFG_GEN_BIT(HARD_RESET) |
		 SCU_SAS_PCFG_GEN_BIT(OOB_RESET));
	writel(phy_configuration_value,
	       &iphy->link_layer_registers->phy_configuration);

	
	phy_configuration_value |= SCU_SAS_PCFG_GEN_BIT(OOB_ENABLE);
	phy_configuration_value &= ~SCU_SAS_PCFG_GEN_BIT(OOB_RESET);
	writel(phy_configuration_value,
	       &iphy->link_layer_registers->phy_configuration);
}

static void sci_phy_stopped_state_enter(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);
	struct isci_port *iport = iphy->owning_port;
	struct isci_host *ihost = iport->owning_controller;

	sci_del_timer(&iphy->sata_timer);

	scu_link_layer_stop_protocol_engine(iphy);

	if (iphy->sm.previous_state_id != SCI_PHY_INITIAL)
		sci_controller_link_down(ihost, phy_get_non_dummy_port(iphy), iphy);
}

static void sci_phy_starting_state_enter(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);
	struct isci_port *iport = iphy->owning_port;
	struct isci_host *ihost = iport->owning_controller;

	scu_link_layer_stop_protocol_engine(iphy);
	scu_link_layer_start_oob(iphy);

	
	iphy->protocol = SCIC_SDS_PHY_PROTOCOL_UNKNOWN;
	iphy->bcn_received_while_port_unassigned = false;

	if (iphy->sm.previous_state_id == SCI_PHY_READY)
		sci_controller_link_down(ihost, phy_get_non_dummy_port(iphy), iphy);

	sci_change_state(&iphy->sm, SCI_PHY_SUB_INITIAL);
}

static void sci_phy_ready_state_enter(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);
	struct isci_port *iport = iphy->owning_port;
	struct isci_host *ihost = iport->owning_controller;

	sci_controller_link_up(ihost, phy_get_non_dummy_port(iphy), iphy);
}

static void sci_phy_ready_state_exit(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);

	sci_phy_suspend(iphy);
}

static void sci_phy_resetting_state_enter(struct sci_base_state_machine *sm)
{
	struct isci_phy *iphy = container_of(sm, typeof(*iphy), sm);

	sci_port_deactivate_phy(iphy->owning_port, iphy, false);

	if (iphy->protocol == SCIC_SDS_PHY_PROTOCOL_SAS) {
		scu_link_layer_tx_hard_reset(iphy);
	} else {
		sci_change_state(&iphy->sm, SCI_PHY_STARTING);
	}
}

static const struct sci_base_state sci_phy_state_table[] = {
	[SCI_PHY_INITIAL] = { },
	[SCI_PHY_STOPPED] = {
		.enter_state = sci_phy_stopped_state_enter,
	},
	[SCI_PHY_STARTING] = {
		.enter_state = sci_phy_starting_state_enter,
	},
	[SCI_PHY_SUB_INITIAL] = {
		.enter_state = sci_phy_starting_initial_substate_enter,
	},
	[SCI_PHY_SUB_AWAIT_OSSP_EN] = { },
	[SCI_PHY_SUB_AWAIT_SAS_SPEED_EN] = { },
	[SCI_PHY_SUB_AWAIT_IAF_UF] = { },
	[SCI_PHY_SUB_AWAIT_SAS_POWER] = {
		.enter_state = sci_phy_starting_await_sas_power_substate_enter,
		.exit_state  = sci_phy_starting_await_sas_power_substate_exit,
	},
	[SCI_PHY_SUB_AWAIT_SATA_POWER] = {
		.enter_state = sci_phy_starting_await_sata_power_substate_enter,
		.exit_state  = sci_phy_starting_await_sata_power_substate_exit
	},
	[SCI_PHY_SUB_AWAIT_SATA_PHY_EN] = {
		.enter_state = sci_phy_starting_await_sata_phy_substate_enter,
		.exit_state  = sci_phy_starting_await_sata_phy_substate_exit
	},
	[SCI_PHY_SUB_AWAIT_SATA_SPEED_EN] = {
		.enter_state = sci_phy_starting_await_sata_speed_substate_enter,
		.exit_state  = sci_phy_starting_await_sata_speed_substate_exit
	},
	[SCI_PHY_SUB_AWAIT_SIG_FIS_UF] = {
		.enter_state = sci_phy_starting_await_sig_fis_uf_substate_enter,
		.exit_state  = sci_phy_starting_await_sig_fis_uf_substate_exit
	},
	[SCI_PHY_SUB_FINAL] = {
		.enter_state = sci_phy_starting_final_substate_enter,
	},
	[SCI_PHY_READY] = {
		.enter_state = sci_phy_ready_state_enter,
		.exit_state = sci_phy_ready_state_exit,
	},
	[SCI_PHY_RESETTING] = {
		.enter_state = sci_phy_resetting_state_enter,
	},
	[SCI_PHY_FINAL] = { },
};

void sci_phy_construct(struct isci_phy *iphy,
			    struct isci_port *iport, u8 phy_index)
{
	sci_init_sm(&iphy->sm, sci_phy_state_table, SCI_PHY_INITIAL);

	
	iphy->owning_port = iport;
	iphy->phy_index = phy_index;
	iphy->bcn_received_while_port_unassigned = false;
	iphy->protocol = SCIC_SDS_PHY_PROTOCOL_UNKNOWN;
	iphy->link_layer_registers = NULL;
	iphy->max_negotiated_speed = SAS_LINK_RATE_UNKNOWN;

	
	sci_init_timer(&iphy->sata_timer, phy_sata_timeout);
}

void isci_phy_init(struct isci_phy *iphy, struct isci_host *ihost, int index)
{
	struct sci_oem_params *oem = &ihost->oem_parameters;
	u64 sci_sas_addr;
	__be64 sas_addr;

	sci_sas_addr = oem->phys[index].sas_address.high;
	sci_sas_addr <<= 32;
	sci_sas_addr |= oem->phys[index].sas_address.low;
	sas_addr = cpu_to_be64(sci_sas_addr);
	memcpy(iphy->sas_addr, &sas_addr, sizeof(sas_addr));

	iphy->sas_phy.enabled = 0;
	iphy->sas_phy.id = index;
	iphy->sas_phy.sas_addr = &iphy->sas_addr[0];
	iphy->sas_phy.frame_rcvd = (u8 *)&iphy->frame_rcvd;
	iphy->sas_phy.ha = &ihost->sas_ha;
	iphy->sas_phy.lldd_phy = iphy;
	iphy->sas_phy.enabled = 1;
	iphy->sas_phy.class = SAS;
	iphy->sas_phy.iproto = SAS_PROTOCOL_ALL;
	iphy->sas_phy.tproto = 0;
	iphy->sas_phy.type = PHY_TYPE_PHYSICAL;
	iphy->sas_phy.role = PHY_ROLE_INITIATOR;
	iphy->sas_phy.oob_mode = OOB_NOT_CONNECTED;
	iphy->sas_phy.linkrate = SAS_LINK_RATE_UNKNOWN;
	memset(&iphy->frame_rcvd, 0, sizeof(iphy->frame_rcvd));
}


int isci_phy_control(struct asd_sas_phy *sas_phy,
		     enum phy_func func,
		     void *buf)
{
	int ret = 0;
	struct isci_phy *iphy = sas_phy->lldd_phy;
	struct asd_sas_port *port = sas_phy->port;
	struct isci_host *ihost = sas_phy->ha->lldd_ha;
	unsigned long flags;

	dev_dbg(&ihost->pdev->dev,
		"%s: phy %p; func %d; buf %p; isci phy %p, port %p\n",
		__func__, sas_phy, func, buf, iphy, port);

	switch (func) {
	case PHY_FUNC_DISABLE:
		spin_lock_irqsave(&ihost->scic_lock, flags);
		sci_phy_stop(iphy);
		spin_unlock_irqrestore(&ihost->scic_lock, flags);
		break;

	case PHY_FUNC_LINK_RESET:
		spin_lock_irqsave(&ihost->scic_lock, flags);
		sci_phy_stop(iphy);
		sci_phy_start(iphy);
		spin_unlock_irqrestore(&ihost->scic_lock, flags);
		break;

	case PHY_FUNC_HARD_RESET:
		if (!port)
			return -ENODEV;

		ret = isci_port_perform_hard_reset(ihost, port->lldd_port, iphy);

		break;
	case PHY_FUNC_GET_EVENTS: {
		struct scu_link_layer_registers __iomem *r;
		struct sas_phy *phy = sas_phy->phy;

		r = iphy->link_layer_registers;
		phy->running_disparity_error_count = readl(&r->running_disparity_error_count);
		phy->loss_of_dword_sync_count = readl(&r->loss_of_sync_error_count);
		phy->phy_reset_problem_count = readl(&r->phy_reset_problem_count);
		phy->invalid_dword_count = readl(&r->invalid_dword_counter);
		break;
	}

	default:
		dev_dbg(&ihost->pdev->dev,
			   "%s: phy %p; func %d NOT IMPLEMENTED!\n",
			   __func__, sas_phy, func);
		ret = -ENOSYS;
		break;
	}
	return ret;
}
