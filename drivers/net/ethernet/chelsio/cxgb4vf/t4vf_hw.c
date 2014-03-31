/*
 * This file is part of the Chelsio T4 PCI-E SR-IOV Virtual Function Ethernet
 * driver for Linux.
 *
 * Copyright (c) 2009-2010 Chelsio Communications, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/pci.h>

#include "t4vf_common.h"
#include "t4vf_defs.h"

#include "../cxgb4/t4_regs.h"
#include "../cxgb4/t4fw_api.h"

int __devinit t4vf_wait_dev_ready(struct adapter *adapter)
{
	const u32 whoami = T4VF_PL_BASE_ADDR + PL_VF_WHOAMI;
	const u32 notready1 = 0xffffffff;
	const u32 notready2 = 0xeeeeeeee;
	u32 val;

	val = t4_read_reg(adapter, whoami);
	if (val != notready1 && val != notready2)
		return 0;
	msleep(500);
	val = t4_read_reg(adapter, whoami);
	if (val != notready1 && val != notready2)
		return 0;
	else
		return -EIO;
}

static void get_mbox_rpl(struct adapter *adapter, __be64 *rpl, int size,
			 u32 mbox_data)
{
	for ( ; size; size -= 8, mbox_data += 8)
		*rpl++ = cpu_to_be64(t4_read_reg64(adapter, mbox_data));
}

static void dump_mbox(struct adapter *adapter, const char *tag, u32 mbox_data)
{
	dev_err(adapter->pdev_dev,
		"mbox %s: %llx %llx %llx %llx %llx %llx %llx %llx\n", tag,
		(unsigned long long)t4_read_reg64(adapter, mbox_data +  0),
		(unsigned long long)t4_read_reg64(adapter, mbox_data +  8),
		(unsigned long long)t4_read_reg64(adapter, mbox_data + 16),
		(unsigned long long)t4_read_reg64(adapter, mbox_data + 24),
		(unsigned long long)t4_read_reg64(adapter, mbox_data + 32),
		(unsigned long long)t4_read_reg64(adapter, mbox_data + 40),
		(unsigned long long)t4_read_reg64(adapter, mbox_data + 48),
		(unsigned long long)t4_read_reg64(adapter, mbox_data + 56));
}

int t4vf_wr_mbox_core(struct adapter *adapter, const void *cmd, int size,
		      void *rpl, bool sleep_ok)
{
	static const int delay[] = {
		1, 1, 3, 5, 10, 10, 20, 50, 100
	};

	u32 v;
	int i, ms, delay_idx;
	const __be64 *p;
	u32 mbox_data = T4VF_MBDATA_BASE_ADDR;
	u32 mbox_ctl = T4VF_CIM_BASE_ADDR + CIM_VF_EXT_MAILBOX_CTRL;

	if ((size % 16) != 0 ||
	    size > NUM_CIM_VF_MAILBOX_DATA_INSTANCES * 4)
		return -EINVAL;

	v = MBOWNER_GET(t4_read_reg(adapter, mbox_ctl));
	for (i = 0; v == MBOX_OWNER_NONE && i < 3; i++)
		v = MBOWNER_GET(t4_read_reg(adapter, mbox_ctl));
	if (v != MBOX_OWNER_DRV)
		return v == MBOX_OWNER_FW ? -EBUSY : -ETIMEDOUT;

	for (i = 0, p = cmd; i < size; i += 8)
		t4_write_reg64(adapter, mbox_data + i, be64_to_cpu(*p++));
	t4_read_reg(adapter, mbox_data);         

	t4_write_reg(adapter, mbox_ctl,
		     MBMSGVALID | MBOWNER(MBOX_OWNER_FW));
	t4_read_reg(adapter, mbox_ctl);          

	delay_idx = 0;
	ms = delay[0];

	for (i = 0; i < FW_CMD_MAX_TIMEOUT; i += ms) {
		if (sleep_ok) {
			ms = delay[delay_idx];
			if (delay_idx < ARRAY_SIZE(delay) - 1)
				delay_idx++;
			msleep(ms);
		} else
			mdelay(ms);

		v = t4_read_reg(adapter, mbox_ctl);
		if (MBOWNER_GET(v) == MBOX_OWNER_DRV) {
			if ((v & MBMSGVALID) == 0) {
				t4_write_reg(adapter, mbox_ctl,
					     MBOWNER(MBOX_OWNER_NONE));
				continue;
			}


			
			v = t4_read_reg(adapter, mbox_data);
			if (FW_CMD_RETVAL_GET(v))
				dump_mbox(adapter, "FW Error", mbox_data);

			if (rpl) {
				
				WARN_ON((be32_to_cpu(*(const u32 *)cmd)
					 & FW_CMD_REQUEST) == 0);
				get_mbox_rpl(adapter, rpl, size, mbox_data);
				WARN_ON((be32_to_cpu(*(u32 *)rpl)
					 & FW_CMD_REQUEST) != 0);
			}
			t4_write_reg(adapter, mbox_ctl,
				     MBOWNER(MBOX_OWNER_NONE));
			return -FW_CMD_RETVAL_GET(v);
		}
	}

	dump_mbox(adapter, "FW Timeout", mbox_data);
	return -ETIMEDOUT;
}

static int hash_mac_addr(const u8 *addr)
{
	u32 a = ((u32)addr[0] << 16) | ((u32)addr[1] << 8) | addr[2];
	u32 b = ((u32)addr[3] << 16) | ((u32)addr[4] << 8) | addr[5];
	a ^= b;
	a ^= (a >> 12);
	a ^= (a >> 6);
	return a & 0x3f;
}

static void __devinit init_link_config(struct link_config *lc,
				       unsigned int caps)
{
	lc->supported = caps;
	lc->requested_speed = 0;
	lc->speed = 0;
	lc->requested_fc = lc->fc = PAUSE_RX | PAUSE_TX;
	if (lc->supported & SUPPORTED_Autoneg) {
		lc->advertising = lc->supported;
		lc->autoneg = AUTONEG_ENABLE;
		lc->requested_fc |= PAUSE_AUTONEG;
	} else {
		lc->advertising = 0;
		lc->autoneg = AUTONEG_DISABLE;
	}
}

int __devinit t4vf_port_init(struct adapter *adapter, int pidx)
{
	struct port_info *pi = adap2pinfo(adapter, pidx);
	struct fw_vi_cmd vi_cmd, vi_rpl;
	struct fw_port_cmd port_cmd, port_rpl;
	int v;
	u32 word;

	memset(&vi_cmd, 0, sizeof(vi_cmd));
	vi_cmd.op_to_vfn = cpu_to_be32(FW_CMD_OP(FW_VI_CMD) |
				       FW_CMD_REQUEST |
				       FW_CMD_READ);
	vi_cmd.alloc_to_len16 = cpu_to_be32(FW_LEN16(vi_cmd));
	vi_cmd.type_viid = cpu_to_be16(FW_VI_CMD_VIID(pi->viid));
	v = t4vf_wr_mbox(adapter, &vi_cmd, sizeof(vi_cmd), &vi_rpl);
	if (v)
		return v;

	BUG_ON(pi->port_id != FW_VI_CMD_PORTID_GET(vi_rpl.portid_pkd));
	pi->rss_size = FW_VI_CMD_RSSSIZE_GET(be16_to_cpu(vi_rpl.rsssize_pkd));
	t4_os_set_hw_addr(adapter, pidx, vi_rpl.mac);

	if (!(adapter->params.vfres.r_caps & FW_CMD_CAP_PORT))
		return 0;

	memset(&port_cmd, 0, sizeof(port_cmd));
	port_cmd.op_to_portid = cpu_to_be32(FW_CMD_OP(FW_PORT_CMD) |
					    FW_CMD_REQUEST |
					    FW_CMD_READ |
					    FW_PORT_CMD_PORTID(pi->port_id));
	port_cmd.action_to_len16 =
		cpu_to_be32(FW_PORT_CMD_ACTION(FW_PORT_ACTION_GET_PORT_INFO) |
			    FW_LEN16(port_cmd));
	v = t4vf_wr_mbox(adapter, &port_cmd, sizeof(port_cmd), &port_rpl);
	if (v)
		return v;

	v = 0;
	word = be16_to_cpu(port_rpl.u.info.pcap);
	if (word & FW_PORT_CAP_SPEED_100M)
		v |= SUPPORTED_100baseT_Full;
	if (word & FW_PORT_CAP_SPEED_1G)
		v |= SUPPORTED_1000baseT_Full;
	if (word & FW_PORT_CAP_SPEED_10G)
		v |= SUPPORTED_10000baseT_Full;
	if (word & FW_PORT_CAP_ANEG)
		v |= SUPPORTED_Autoneg;
	init_link_config(&pi->link_cfg, v);

	return 0;
}

int t4vf_fw_reset(struct adapter *adapter)
{
	struct fw_reset_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_write = cpu_to_be32(FW_CMD_OP(FW_RESET_CMD) |
				      FW_CMD_WRITE);
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));
	return t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), NULL);
}

int t4vf_query_params(struct adapter *adapter, unsigned int nparams,
		      const u32 *params, u32 *vals)
{
	int i, ret;
	struct fw_params_cmd cmd, rpl;
	struct fw_params_param *p;
	size_t len16;

	if (nparams > 7)
		return -EINVAL;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(FW_CMD_OP(FW_PARAMS_CMD) |
				    FW_CMD_REQUEST |
				    FW_CMD_READ);
	len16 = DIV_ROUND_UP(offsetof(struct fw_params_cmd,
				      param[nparams].mnem), 16);
	cmd.retval_len16 = cpu_to_be32(FW_CMD_LEN16(len16));
	for (i = 0, p = &cmd.param[0]; i < nparams; i++, p++)
		p->mnem = htonl(*params++);

	ret = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (ret == 0)
		for (i = 0, p = &rpl.param[0]; i < nparams; i++, p++)
			*vals++ = be32_to_cpu(p->val);
	return ret;
}

int t4vf_set_params(struct adapter *adapter, unsigned int nparams,
		    const u32 *params, const u32 *vals)
{
	int i;
	struct fw_params_cmd cmd;
	struct fw_params_param *p;
	size_t len16;

	if (nparams > 7)
		return -EINVAL;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(FW_CMD_OP(FW_PARAMS_CMD) |
				    FW_CMD_REQUEST |
				    FW_CMD_WRITE);
	len16 = DIV_ROUND_UP(offsetof(struct fw_params_cmd,
				      param[nparams]), 16);
	cmd.retval_len16 = cpu_to_be32(FW_CMD_LEN16(len16));
	for (i = 0, p = &cmd.param[0]; i < nparams; i++, p++) {
		p->mnem = cpu_to_be32(*params++);
		p->val = cpu_to_be32(*vals++);
	}

	return t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), NULL);
}

int t4vf_get_sge_params(struct adapter *adapter)
{
	struct sge_params *sge_params = &adapter->params.sge;
	u32 params[7], vals[7];
	int v;

	params[0] = (FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     FW_PARAMS_PARAM_XYZ(SGE_CONTROL));
	params[1] = (FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     FW_PARAMS_PARAM_XYZ(SGE_HOST_PAGE_SIZE));
	params[2] = (FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     FW_PARAMS_PARAM_XYZ(SGE_FL_BUFFER_SIZE0));
	params[3] = (FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     FW_PARAMS_PARAM_XYZ(SGE_FL_BUFFER_SIZE1));
	params[4] = (FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     FW_PARAMS_PARAM_XYZ(SGE_TIMER_VALUE_0_AND_1));
	params[5] = (FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     FW_PARAMS_PARAM_XYZ(SGE_TIMER_VALUE_2_AND_3));
	params[6] = (FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     FW_PARAMS_PARAM_XYZ(SGE_TIMER_VALUE_4_AND_5));
	v = t4vf_query_params(adapter, 7, params, vals);
	if (v)
		return v;
	sge_params->sge_control = vals[0];
	sge_params->sge_host_page_size = vals[1];
	sge_params->sge_fl_buffer_size[0] = vals[2];
	sge_params->sge_fl_buffer_size[1] = vals[3];
	sge_params->sge_timer_value_0_and_1 = vals[4];
	sge_params->sge_timer_value_2_and_3 = vals[5];
	sge_params->sge_timer_value_4_and_5 = vals[6];

	params[0] = (FW_PARAMS_MNEM(FW_PARAMS_MNEM_REG) |
		     FW_PARAMS_PARAM_XYZ(SGE_INGRESS_RX_THRESHOLD));
	v = t4vf_query_params(adapter, 1, params, vals);
	if (v)
		return v;
	sge_params->sge_ingress_rx_threshold = vals[0];

	return 0;
}

int t4vf_get_vpd_params(struct adapter *adapter)
{
	struct vpd_params *vpd_params = &adapter->params.vpd;
	u32 params[7], vals[7];
	int v;

	params[0] = (FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
		     FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_CCLK));
	v = t4vf_query_params(adapter, 1, params, vals);
	if (v)
		return v;
	vpd_params->cclk = vals[0];

	return 0;
}

int t4vf_get_dev_params(struct adapter *adapter)
{
	struct dev_params *dev_params = &adapter->params.dev;
	u32 params[7], vals[7];
	int v;

	params[0] = (FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
		     FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_FWREV));
	params[1] = (FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
		     FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_TPREV));
	v = t4vf_query_params(adapter, 2, params, vals);
	if (v)
		return v;
	dev_params->fwrev = vals[0];
	dev_params->tprev = vals[1];

	return 0;
}

int t4vf_get_rss_glb_config(struct adapter *adapter)
{
	struct rss_params *rss = &adapter->params.rss;
	struct fw_rss_glb_config_cmd cmd, rpl;
	int v;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_write = cpu_to_be32(FW_CMD_OP(FW_RSS_GLB_CONFIG_CMD) |
				      FW_CMD_REQUEST |
				      FW_CMD_READ);
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));
	v = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (v)
		return v;

	rss->mode = FW_RSS_GLB_CONFIG_CMD_MODE_GET(
			be32_to_cpu(rpl.u.manual.mode_pkd));
	switch (rss->mode) {
	case FW_RSS_GLB_CONFIG_CMD_MODE_BASICVIRTUAL: {
		u32 word = be32_to_cpu(
				rpl.u.basicvirtual.synmapen_to_hashtoeplitz);

		rss->u.basicvirtual.synmapen =
			((word & FW_RSS_GLB_CONFIG_CMD_SYNMAPEN) != 0);
		rss->u.basicvirtual.syn4tupenipv6 =
			((word & FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6) != 0);
		rss->u.basicvirtual.syn2tupenipv6 =
			((word & FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6) != 0);
		rss->u.basicvirtual.syn4tupenipv4 =
			((word & FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4) != 0);
		rss->u.basicvirtual.syn2tupenipv4 =
			((word & FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4) != 0);

		rss->u.basicvirtual.ofdmapen =
			((word & FW_RSS_GLB_CONFIG_CMD_OFDMAPEN) != 0);

		rss->u.basicvirtual.tnlmapen =
			((word & FW_RSS_GLB_CONFIG_CMD_TNLMAPEN) != 0);
		rss->u.basicvirtual.tnlalllookup =
			((word  & FW_RSS_GLB_CONFIG_CMD_TNLALLLKP) != 0);

		rss->u.basicvirtual.hashtoeplitz =
			((word & FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ) != 0);

		
		if (!rss->u.basicvirtual.tnlmapen)
			return -EINVAL;
		break;
	}

	default:
		
		return -EINVAL;
	}

	return 0;
}

int t4vf_get_vfres(struct adapter *adapter)
{
	struct vf_resources *vfres = &adapter->params.vfres;
	struct fw_pfvf_cmd cmd, rpl;
	int v;
	u32 word;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(FW_CMD_OP(FW_PFVF_CMD) |
				    FW_CMD_REQUEST |
				    FW_CMD_READ);
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));
	v = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (v)
		return v;

	word = be32_to_cpu(rpl.niqflint_niq);
	vfres->niqflint = FW_PFVF_CMD_NIQFLINT_GET(word);
	vfres->niq = FW_PFVF_CMD_NIQ_GET(word);

	word = be32_to_cpu(rpl.type_to_neq);
	vfres->neq = FW_PFVF_CMD_NEQ_GET(word);
	vfres->pmask = FW_PFVF_CMD_PMASK_GET(word);

	word = be32_to_cpu(rpl.tc_to_nexactf);
	vfres->tc = FW_PFVF_CMD_TC_GET(word);
	vfres->nvi = FW_PFVF_CMD_NVI_GET(word);
	vfres->nexactf = FW_PFVF_CMD_NEXACTF_GET(word);

	word = be32_to_cpu(rpl.r_caps_to_nethctrl);
	vfres->r_caps = FW_PFVF_CMD_R_CAPS_GET(word);
	vfres->wx_caps = FW_PFVF_CMD_WX_CAPS_GET(word);
	vfres->nethctrl = FW_PFVF_CMD_NETHCTRL_GET(word);

	return 0;
}

int t4vf_read_rss_vi_config(struct adapter *adapter, unsigned int viid,
			    union rss_vi_config *config)
{
	struct fw_rss_vi_config_cmd cmd, rpl;
	int v;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_viid = cpu_to_be32(FW_CMD_OP(FW_RSS_VI_CONFIG_CMD) |
				     FW_CMD_REQUEST |
				     FW_CMD_READ |
				     FW_RSS_VI_CONFIG_CMD_VIID(viid));
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));
	v = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (v)
		return v;

	switch (adapter->params.rss.mode) {
	case FW_RSS_GLB_CONFIG_CMD_MODE_BASICVIRTUAL: {
		u32 word = be32_to_cpu(rpl.u.basicvirtual.defaultq_to_udpen);

		config->basicvirtual.ip6fourtupen =
			((word & FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN) != 0);
		config->basicvirtual.ip6twotupen =
			((word & FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN) != 0);
		config->basicvirtual.ip4fourtupen =
			((word & FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN) != 0);
		config->basicvirtual.ip4twotupen =
			((word & FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN) != 0);
		config->basicvirtual.udpen =
			((word & FW_RSS_VI_CONFIG_CMD_UDPEN) != 0);
		config->basicvirtual.defaultq =
			FW_RSS_VI_CONFIG_CMD_DEFAULTQ_GET(word);
		break;
	}

	default:
		return -EINVAL;
	}

	return 0;
}

int t4vf_write_rss_vi_config(struct adapter *adapter, unsigned int viid,
			     union rss_vi_config *config)
{
	struct fw_rss_vi_config_cmd cmd, rpl;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_viid = cpu_to_be32(FW_CMD_OP(FW_RSS_VI_CONFIG_CMD) |
				     FW_CMD_REQUEST |
				     FW_CMD_WRITE |
				     FW_RSS_VI_CONFIG_CMD_VIID(viid));
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));
	switch (adapter->params.rss.mode) {
	case FW_RSS_GLB_CONFIG_CMD_MODE_BASICVIRTUAL: {
		u32 word = 0;

		if (config->basicvirtual.ip6fourtupen)
			word |= FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN;
		if (config->basicvirtual.ip6twotupen)
			word |= FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN;
		if (config->basicvirtual.ip4fourtupen)
			word |= FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN;
		if (config->basicvirtual.ip4twotupen)
			word |= FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN;
		if (config->basicvirtual.udpen)
			word |= FW_RSS_VI_CONFIG_CMD_UDPEN;
		word |= FW_RSS_VI_CONFIG_CMD_DEFAULTQ(
				config->basicvirtual.defaultq);
		cmd.u.basicvirtual.defaultq_to_udpen = cpu_to_be32(word);
		break;
	}

	default:
		return -EINVAL;
	}

	return t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
}

int t4vf_config_rss_range(struct adapter *adapter, unsigned int viid,
			  int start, int n, const u16 *rspq, int nrspq)
{
	const u16 *rsp = rspq;
	const u16 *rsp_end = rspq+nrspq;
	struct fw_rss_ind_tbl_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_viid = cpu_to_be32(FW_CMD_OP(FW_RSS_IND_TBL_CMD) |
				     FW_CMD_REQUEST |
				     FW_CMD_WRITE |
				     FW_RSS_IND_TBL_CMD_VIID(viid));
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));

	while (n > 0) {
		__be32 *qp = &cmd.iq0_to_iq2;
		int nq = min(n, 32);
		int ret;

		cmd.niqid = cpu_to_be16(nq);
		cmd.startidx = cpu_to_be16(start);

		start += nq;
		n -= nq;

		while (nq > 0) {
			u16 qbuf[3];
			u16 *qbp = qbuf;
			int nqbuf = min(3, nq);

			nq -= nqbuf;
			qbuf[0] = qbuf[1] = qbuf[2] = 0;
			while (nqbuf) {
				nqbuf--;
				*qbp++ = *rsp++;
				if (rsp >= rsp_end)
					rsp = rspq;
			}
			*qp++ = cpu_to_be32(FW_RSS_IND_TBL_CMD_IQ0(qbuf[0]) |
					    FW_RSS_IND_TBL_CMD_IQ1(qbuf[1]) |
					    FW_RSS_IND_TBL_CMD_IQ2(qbuf[2]));
		}

		ret = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), NULL);
		if (ret)
			return ret;
	}
	return 0;
}

int t4vf_alloc_vi(struct adapter *adapter, int port_id)
{
	struct fw_vi_cmd cmd, rpl;
	int v;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(FW_CMD_OP(FW_VI_CMD) |
				    FW_CMD_REQUEST |
				    FW_CMD_WRITE |
				    FW_CMD_EXEC);
	cmd.alloc_to_len16 = cpu_to_be32(FW_LEN16(cmd) |
					 FW_VI_CMD_ALLOC);
	cmd.portid_pkd = FW_VI_CMD_PORTID(port_id);
	v = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (v)
		return v;

	return FW_VI_CMD_VIID_GET(be16_to_cpu(rpl.type_viid));
}

int t4vf_free_vi(struct adapter *adapter, int viid)
{
	struct fw_vi_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(FW_CMD_OP(FW_VI_CMD) |
				    FW_CMD_REQUEST |
				    FW_CMD_EXEC);
	cmd.alloc_to_len16 = cpu_to_be32(FW_LEN16(cmd) |
					 FW_VI_CMD_FREE);
	cmd.type_viid = cpu_to_be16(FW_VI_CMD_VIID(viid));
	return t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), NULL);
}

int t4vf_enable_vi(struct adapter *adapter, unsigned int viid,
		   bool rx_en, bool tx_en)
{
	struct fw_vi_enable_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_viid = cpu_to_be32(FW_CMD_OP(FW_VI_ENABLE_CMD) |
				     FW_CMD_REQUEST |
				     FW_CMD_EXEC |
				     FW_VI_ENABLE_CMD_VIID(viid));
	cmd.ien_to_len16 = cpu_to_be32(FW_VI_ENABLE_CMD_IEN(rx_en) |
				       FW_VI_ENABLE_CMD_EEN(tx_en) |
				       FW_LEN16(cmd));
	return t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), NULL);
}

int t4vf_identify_port(struct adapter *adapter, unsigned int viid,
		       unsigned int nblinks)
{
	struct fw_vi_enable_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_viid = cpu_to_be32(FW_CMD_OP(FW_VI_ENABLE_CMD) |
				     FW_CMD_REQUEST |
				     FW_CMD_EXEC |
				     FW_VI_ENABLE_CMD_VIID(viid));
	cmd.ien_to_len16 = cpu_to_be32(FW_VI_ENABLE_CMD_LED |
				       FW_LEN16(cmd));
	cmd.blinkdur = cpu_to_be16(nblinks);
	return t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), NULL);
}

int t4vf_set_rxmode(struct adapter *adapter, unsigned int viid,
		    int mtu, int promisc, int all_multi, int bcast, int vlanex,
		    bool sleep_ok)
{
	struct fw_vi_rxmode_cmd cmd;

	
	if (mtu < 0)
		mtu = FW_VI_RXMODE_CMD_MTU_MASK;
	if (promisc < 0)
		promisc = FW_VI_RXMODE_CMD_PROMISCEN_MASK;
	if (all_multi < 0)
		all_multi = FW_VI_RXMODE_CMD_ALLMULTIEN_MASK;
	if (bcast < 0)
		bcast = FW_VI_RXMODE_CMD_BROADCASTEN_MASK;
	if (vlanex < 0)
		vlanex = FW_VI_RXMODE_CMD_VLANEXEN_MASK;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_viid = cpu_to_be32(FW_CMD_OP(FW_VI_RXMODE_CMD) |
				     FW_CMD_REQUEST |
				     FW_CMD_WRITE |
				     FW_VI_RXMODE_CMD_VIID(viid));
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));
	cmd.mtu_to_vlanexen =
		cpu_to_be32(FW_VI_RXMODE_CMD_MTU(mtu) |
			    FW_VI_RXMODE_CMD_PROMISCEN(promisc) |
			    FW_VI_RXMODE_CMD_ALLMULTIEN(all_multi) |
			    FW_VI_RXMODE_CMD_BROADCASTEN(bcast) |
			    FW_VI_RXMODE_CMD_VLANEXEN(vlanex));
	return t4vf_wr_mbox_core(adapter, &cmd, sizeof(cmd), NULL, sleep_ok);
}

int t4vf_alloc_mac_filt(struct adapter *adapter, unsigned int viid, bool free,
			unsigned int naddr, const u8 **addr, u16 *idx,
			u64 *hash, bool sleep_ok)
{
	int offset, ret = 0;
	unsigned nfilters = 0;
	unsigned int rem = naddr;
	struct fw_vi_mac_cmd cmd, rpl;

	if (naddr > FW_CLS_TCAM_NUM_ENTRIES)
		return -EINVAL;

	for (offset = 0; offset < naddr; ) {
		unsigned int fw_naddr = (rem < ARRAY_SIZE(cmd.u.exact)
					 ? rem
					 : ARRAY_SIZE(cmd.u.exact));
		size_t len16 = DIV_ROUND_UP(offsetof(struct fw_vi_mac_cmd,
						     u.exact[fw_naddr]), 16);
		struct fw_vi_mac_exact *p;
		int i;

		memset(&cmd, 0, sizeof(cmd));
		cmd.op_to_viid = cpu_to_be32(FW_CMD_OP(FW_VI_MAC_CMD) |
					     FW_CMD_REQUEST |
					     FW_CMD_WRITE |
					     (free ? FW_CMD_EXEC : 0) |
					     FW_VI_MAC_CMD_VIID(viid));
		cmd.freemacs_to_len16 =
			cpu_to_be32(FW_VI_MAC_CMD_FREEMACS(free) |
				    FW_CMD_LEN16(len16));

		for (i = 0, p = cmd.u.exact; i < fw_naddr; i++, p++) {
			p->valid_to_idx = cpu_to_be16(
				FW_VI_MAC_CMD_VALID |
				FW_VI_MAC_CMD_IDX(FW_VI_MAC_ADD_MAC));
			memcpy(p->macaddr, addr[offset+i], sizeof(p->macaddr));
		}


		ret = t4vf_wr_mbox_core(adapter, &cmd, sizeof(cmd), &rpl,
					sleep_ok);
		if (ret && ret != -ENOMEM)
			break;

		for (i = 0, p = rpl.u.exact; i < fw_naddr; i++, p++) {
			u16 index = FW_VI_MAC_CMD_IDX_GET(
				be16_to_cpu(p->valid_to_idx));

			if (idx)
				idx[offset+i] =
					(index >= FW_CLS_TCAM_NUM_ENTRIES
					 ? 0xffff
					 : index);
			if (index < FW_CLS_TCAM_NUM_ENTRIES)
				nfilters++;
			else if (hash)
				*hash |= (1ULL << hash_mac_addr(addr[offset+i]));
		}

		free = false;
		offset += fw_naddr;
		rem -= fw_naddr;
	}

	/*
	 * If there were no errors or we merely ran out of room in our MAC
	 * address arena, return the number of filters actually written.
	 */
	if (ret == 0 || ret == -ENOMEM)
		ret = nfilters;
	return ret;
}

int t4vf_change_mac(struct adapter *adapter, unsigned int viid,
		    int idx, const u8 *addr, bool persist)
{
	int ret;
	struct fw_vi_mac_cmd cmd, rpl;
	struct fw_vi_mac_exact *p = &cmd.u.exact[0];
	size_t len16 = DIV_ROUND_UP(offsetof(struct fw_vi_mac_cmd,
					     u.exact[1]), 16);

	if (idx < 0)
		idx = persist ? FW_VI_MAC_ADD_PERSIST_MAC : FW_VI_MAC_ADD_MAC;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_viid = cpu_to_be32(FW_CMD_OP(FW_VI_MAC_CMD) |
				     FW_CMD_REQUEST |
				     FW_CMD_WRITE |
				     FW_VI_MAC_CMD_VIID(viid));
	cmd.freemacs_to_len16 = cpu_to_be32(FW_CMD_LEN16(len16));
	p->valid_to_idx = cpu_to_be16(FW_VI_MAC_CMD_VALID |
				      FW_VI_MAC_CMD_IDX(idx));
	memcpy(p->macaddr, addr, sizeof(p->macaddr));

	ret = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), &rpl);
	if (ret == 0) {
		p = &rpl.u.exact[0];
		ret = FW_VI_MAC_CMD_IDX_GET(be16_to_cpu(p->valid_to_idx));
		if (ret >= FW_CLS_TCAM_NUM_ENTRIES)
			ret = -ENOMEM;
	}
	return ret;
}

/**
 *	t4vf_set_addr_hash - program the MAC inexact-match hash filter
 *	@adapter: the adapter
 *	@viid: the Virtual Interface Identifier
 *	@ucast: whether the hash filter should also match unicast addresses
 *	@vec: the value to be written to the hash filter
 *	@sleep_ok: call is allowed to sleep
 *
 *	Sets the 64-bit inexact-match hash filter for a virtual interface.
 */
int t4vf_set_addr_hash(struct adapter *adapter, unsigned int viid,
		       bool ucast, u64 vec, bool sleep_ok)
{
	struct fw_vi_mac_cmd cmd;
	size_t len16 = DIV_ROUND_UP(offsetof(struct fw_vi_mac_cmd,
					     u.exact[0]), 16);

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_viid = cpu_to_be32(FW_CMD_OP(FW_VI_MAC_CMD) |
				     FW_CMD_REQUEST |
				     FW_CMD_WRITE |
				     FW_VI_ENABLE_CMD_VIID(viid));
	cmd.freemacs_to_len16 = cpu_to_be32(FW_VI_MAC_CMD_HASHVECEN |
					    FW_VI_MAC_CMD_HASHUNIEN(ucast) |
					    FW_CMD_LEN16(len16));
	cmd.u.hash.hashvec = cpu_to_be64(vec);
	return t4vf_wr_mbox_core(adapter, &cmd, sizeof(cmd), NULL, sleep_ok);
}

int t4vf_get_port_stats(struct adapter *adapter, int pidx,
			struct t4vf_port_stats *s)
{
	struct port_info *pi = adap2pinfo(adapter, pidx);
	struct fw_vi_stats_vf fwstats;
	unsigned int rem = VI_VF_NUM_STATS;
	__be64 *fwsp = (__be64 *)&fwstats;

	while (rem) {
		unsigned int ix = VI_VF_NUM_STATS - rem;
		unsigned int nstats = min(6U, rem);
		struct fw_vi_stats_cmd cmd, rpl;
		size_t len = (offsetof(struct fw_vi_stats_cmd, u) +
			      sizeof(struct fw_vi_stats_ctl));
		size_t len16 = DIV_ROUND_UP(len, 16);
		int ret;

		memset(&cmd, 0, sizeof(cmd));
		cmd.op_to_viid = cpu_to_be32(FW_CMD_OP(FW_VI_STATS_CMD) |
					     FW_VI_STATS_CMD_VIID(pi->viid) |
					     FW_CMD_REQUEST |
					     FW_CMD_READ);
		cmd.retval_len16 = cpu_to_be32(FW_CMD_LEN16(len16));
		cmd.u.ctl.nstats_ix =
			cpu_to_be16(FW_VI_STATS_CMD_IX(ix) |
				    FW_VI_STATS_CMD_NSTATS(nstats));
		ret = t4vf_wr_mbox_ns(adapter, &cmd, len, &rpl);
		if (ret)
			return ret;

		memcpy(fwsp, &rpl.u.ctl.stat0, sizeof(__be64) * nstats);

		rem -= nstats;
		fwsp += nstats;
	}

	s->tx_bcast_bytes = be64_to_cpu(fwstats.tx_bcast_bytes);
	s->tx_bcast_frames = be64_to_cpu(fwstats.tx_bcast_frames);
	s->tx_mcast_bytes = be64_to_cpu(fwstats.tx_mcast_bytes);
	s->tx_mcast_frames = be64_to_cpu(fwstats.tx_mcast_frames);
	s->tx_ucast_bytes = be64_to_cpu(fwstats.tx_ucast_bytes);
	s->tx_ucast_frames = be64_to_cpu(fwstats.tx_ucast_frames);
	s->tx_drop_frames = be64_to_cpu(fwstats.tx_drop_frames);
	s->tx_offload_bytes = be64_to_cpu(fwstats.tx_offload_bytes);
	s->tx_offload_frames = be64_to_cpu(fwstats.tx_offload_frames);

	s->rx_bcast_bytes = be64_to_cpu(fwstats.rx_bcast_bytes);
	s->rx_bcast_frames = be64_to_cpu(fwstats.rx_bcast_frames);
	s->rx_mcast_bytes = be64_to_cpu(fwstats.rx_mcast_bytes);
	s->rx_mcast_frames = be64_to_cpu(fwstats.rx_mcast_frames);
	s->rx_ucast_bytes = be64_to_cpu(fwstats.rx_ucast_bytes);
	s->rx_ucast_frames = be64_to_cpu(fwstats.rx_ucast_frames);

	s->rx_err_frames = be64_to_cpu(fwstats.rx_err_frames);

	return 0;
}

int t4vf_iq_free(struct adapter *adapter, unsigned int iqtype,
		 unsigned int iqid, unsigned int fl0id, unsigned int fl1id)
{
	struct fw_iq_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(FW_CMD_OP(FW_IQ_CMD) |
				    FW_CMD_REQUEST |
				    FW_CMD_EXEC);
	cmd.alloc_to_len16 = cpu_to_be32(FW_IQ_CMD_FREE |
					 FW_LEN16(cmd));
	cmd.type_to_iqandstindex =
		cpu_to_be32(FW_IQ_CMD_TYPE(iqtype));

	cmd.iqid = cpu_to_be16(iqid);
	cmd.fl0id = cpu_to_be16(fl0id);
	cmd.fl1id = cpu_to_be16(fl1id);
	return t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), NULL);
}

int t4vf_eth_eq_free(struct adapter *adapter, unsigned int eqid)
{
	struct fw_eq_eth_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(FW_CMD_OP(FW_EQ_ETH_CMD) |
				    FW_CMD_REQUEST |
				    FW_CMD_EXEC);
	cmd.alloc_to_len16 = cpu_to_be32(FW_EQ_ETH_CMD_FREE |
					 FW_LEN16(cmd));
	cmd.eqid_pkd = cpu_to_be32(FW_EQ_ETH_CMD_EQID(eqid));
	return t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), NULL);
}

int t4vf_handle_fw_rpl(struct adapter *adapter, const __be64 *rpl)
{
	const struct fw_cmd_hdr *cmd_hdr = (const struct fw_cmd_hdr *)rpl;
	u8 opcode = FW_CMD_OP_GET(be32_to_cpu(cmd_hdr->hi));

	switch (opcode) {
	case FW_PORT_CMD: {
		const struct fw_port_cmd *port_cmd =
			(const struct fw_port_cmd *)rpl;
		u32 word;
		int action, port_id, link_ok, speed, fc, pidx;

		action = FW_PORT_CMD_ACTION_GET(
			be32_to_cpu(port_cmd->action_to_len16));
		if (action != FW_PORT_ACTION_GET_PORT_INFO) {
			dev_err(adapter->pdev_dev,
				"Unknown firmware PORT reply action %x\n",
				action);
			break;
		}

		port_id = FW_PORT_CMD_PORTID_GET(
			be32_to_cpu(port_cmd->op_to_portid));

		word = be32_to_cpu(port_cmd->u.info.lstatus_to_modtype);
		link_ok = (word & FW_PORT_CMD_LSTATUS) != 0;
		speed = 0;
		fc = 0;
		if (word & FW_PORT_CMD_RXPAUSE)
			fc |= PAUSE_RX;
		if (word & FW_PORT_CMD_TXPAUSE)
			fc |= PAUSE_TX;
		if (word & FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_100M))
			speed = SPEED_100;
		else if (word & FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_1G))
			speed = SPEED_1000;
		else if (word & FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_10G))
			speed = SPEED_10000;

		for_each_port(adapter, pidx) {
			struct port_info *pi = adap2pinfo(adapter, pidx);
			struct link_config *lc;

			if (pi->port_id != port_id)
				continue;

			lc = &pi->link_cfg;
			if (link_ok != lc->link_ok || speed != lc->speed ||
			    fc != lc->fc) {
				
				lc->link_ok = link_ok;
				lc->speed = speed;
				lc->fc = fc;
				t4vf_os_link_changed(adapter, pidx, link_ok);
			}
		}
		break;
	}

	default:
		dev_err(adapter->pdev_dev, "Unknown firmware reply %X\n",
			opcode);
	}
	return 0;
}
