/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/interrupt.h>
#include <linux/types.h>
#include <asm/sn/io.h>
#include <asm/sn/pcibr_provider.h>
#include <asm/sn/pcibus_provider_defs.h>
#include <asm/sn/pcidev.h>
#include <asm/sn/pic.h>
#include <asm/sn/tiocp.h>

union br_ptr {
	struct tiocp tio;
	struct pic pic;
};

void pcireg_control_bit_clr(struct pcibus_info *pcibus_info, u64 bits)
{
	union br_ptr __iomem *ptr = (union br_ptr __iomem *)pcibus_info->pbi_buscommon.bs_base;

	if (pcibus_info) {
		switch (pcibus_info->pbi_bridge_type) {
		case PCIBR_BRIDGETYPE_TIOCP:
			__sn_clrq_relaxed(&ptr->tio.cp_control, bits);
			break;
		case PCIBR_BRIDGETYPE_PIC:
			__sn_clrq_relaxed(&ptr->pic.p_wid_control, bits);
			break;
		default:
			panic
			    ("pcireg_control_bit_clr: unknown bridgetype bridge 0x%p",
			     ptr);
		}
	}
}

void pcireg_control_bit_set(struct pcibus_info *pcibus_info, u64 bits)
{
	union br_ptr __iomem *ptr = (union br_ptr __iomem *)pcibus_info->pbi_buscommon.bs_base;

	if (pcibus_info) {
		switch (pcibus_info->pbi_bridge_type) {
		case PCIBR_BRIDGETYPE_TIOCP:
			__sn_setq_relaxed(&ptr->tio.cp_control, bits);
			break;
		case PCIBR_BRIDGETYPE_PIC:
			__sn_setq_relaxed(&ptr->pic.p_wid_control, bits);
			break;
		default:
			panic
			    ("pcireg_control_bit_set: unknown bridgetype bridge 0x%p",
			     ptr);
		}
	}
}

u64 pcireg_tflush_get(struct pcibus_info *pcibus_info)
{
	union br_ptr __iomem *ptr = (union br_ptr __iomem *)pcibus_info->pbi_buscommon.bs_base;
	u64 ret = 0;

	if (pcibus_info) {
		switch (pcibus_info->pbi_bridge_type) {
		case PCIBR_BRIDGETYPE_TIOCP:
			ret = __sn_readq_relaxed(&ptr->tio.cp_tflush);
			break;
		case PCIBR_BRIDGETYPE_PIC:
			ret = __sn_readq_relaxed(&ptr->pic.p_wid_tflush);
			break;
		default:
			panic
			    ("pcireg_tflush_get: unknown bridgetype bridge 0x%p",
			     ptr);
		}
	}

	
	if (ret != 0)
		panic("pcireg_tflush_get:Target Flush failed\n");

	return ret;
}

u64 pcireg_intr_status_get(struct pcibus_info * pcibus_info)
{
	union br_ptr __iomem *ptr = (union br_ptr __iomem *)pcibus_info->pbi_buscommon.bs_base;
	u64 ret = 0;

	if (pcibus_info) {
		switch (pcibus_info->pbi_bridge_type) {
		case PCIBR_BRIDGETYPE_TIOCP:
			ret = __sn_readq_relaxed(&ptr->tio.cp_int_status);
			break;
		case PCIBR_BRIDGETYPE_PIC:
			ret = __sn_readq_relaxed(&ptr->pic.p_int_status);
			break;
		default:
			panic
			    ("pcireg_intr_status_get: unknown bridgetype bridge 0x%p",
			     ptr);
		}
	}
	return ret;
}

void pcireg_intr_enable_bit_clr(struct pcibus_info *pcibus_info, u64 bits)
{
	union br_ptr __iomem *ptr = (union br_ptr __iomem *)pcibus_info->pbi_buscommon.bs_base;

	if (pcibus_info) {
		switch (pcibus_info->pbi_bridge_type) {
		case PCIBR_BRIDGETYPE_TIOCP:
			__sn_clrq_relaxed(&ptr->tio.cp_int_enable, bits);
			break;
		case PCIBR_BRIDGETYPE_PIC:
			__sn_clrq_relaxed(&ptr->pic.p_int_enable, bits);
			break;
		default:
			panic
			    ("pcireg_intr_enable_bit_clr: unknown bridgetype bridge 0x%p",
			     ptr);
		}
	}
}

void pcireg_intr_enable_bit_set(struct pcibus_info *pcibus_info, u64 bits)
{
	union br_ptr __iomem *ptr = (union br_ptr __iomem *)pcibus_info->pbi_buscommon.bs_base;

	if (pcibus_info) {
		switch (pcibus_info->pbi_bridge_type) {
		case PCIBR_BRIDGETYPE_TIOCP:
			__sn_setq_relaxed(&ptr->tio.cp_int_enable, bits);
			break;
		case PCIBR_BRIDGETYPE_PIC:
			__sn_setq_relaxed(&ptr->pic.p_int_enable, bits);
			break;
		default:
			panic
			    ("pcireg_intr_enable_bit_set: unknown bridgetype bridge 0x%p",
			     ptr);
		}
	}
}

void pcireg_intr_addr_addr_set(struct pcibus_info *pcibus_info, int int_n,
			       u64 addr)
{
	union br_ptr __iomem *ptr = (union br_ptr __iomem *)pcibus_info->pbi_buscommon.bs_base;

	if (pcibus_info) {
		switch (pcibus_info->pbi_bridge_type) {
		case PCIBR_BRIDGETYPE_TIOCP:
			__sn_clrq_relaxed(&ptr->tio.cp_int_addr[int_n],
			    TIOCP_HOST_INTR_ADDR);
			__sn_setq_relaxed(&ptr->tio.cp_int_addr[int_n],
			    (addr & TIOCP_HOST_INTR_ADDR));
			break;
		case PCIBR_BRIDGETYPE_PIC:
			__sn_clrq_relaxed(&ptr->pic.p_int_addr[int_n],
			    PIC_HOST_INTR_ADDR);
			__sn_setq_relaxed(&ptr->pic.p_int_addr[int_n],
			    (addr & PIC_HOST_INTR_ADDR));
			break;
		default:
			panic
			    ("pcireg_intr_addr_addr_get: unknown bridgetype bridge 0x%p",
			     ptr);
		}
	}
}

void pcireg_force_intr_set(struct pcibus_info *pcibus_info, int int_n)
{
	union br_ptr __iomem *ptr = (union br_ptr __iomem *)pcibus_info->pbi_buscommon.bs_base;

	if (pcibus_info) {
		switch (pcibus_info->pbi_bridge_type) {
		case PCIBR_BRIDGETYPE_TIOCP:
			writeq(1, &ptr->tio.cp_force_pin[int_n]);
			break;
		case PCIBR_BRIDGETYPE_PIC:
			writeq(1, &ptr->pic.p_force_pin[int_n]);
			break;
		default:
			panic
			    ("pcireg_force_intr_set: unknown bridgetype bridge 0x%p",
			     ptr);
		}
	}
}

u64 pcireg_wrb_flush_get(struct pcibus_info *pcibus_info, int device)
{
	union br_ptr __iomem *ptr = (union br_ptr __iomem *)pcibus_info->pbi_buscommon.bs_base;
	u64 ret = 0;

	if (pcibus_info) {
		switch (pcibus_info->pbi_bridge_type) {
		case PCIBR_BRIDGETYPE_TIOCP:
			ret =
			    __sn_readq_relaxed(&ptr->tio.cp_wr_req_buf[device]);
			break;
		case PCIBR_BRIDGETYPE_PIC:
			ret =
			    __sn_readq_relaxed(&ptr->pic.p_wr_req_buf[device]);
			break;
		default:
		      panic("pcireg_wrb_flush_get: unknown bridgetype bridge 0x%p", ptr);
		}

	}
	
	return ret;
}

void pcireg_int_ate_set(struct pcibus_info *pcibus_info, int ate_index,
			u64 val)
{
	union br_ptr __iomem *ptr = (union br_ptr __iomem *)pcibus_info->pbi_buscommon.bs_base;

	if (pcibus_info) {
		switch (pcibus_info->pbi_bridge_type) {
		case PCIBR_BRIDGETYPE_TIOCP:
			writeq(val, &ptr->tio.cp_int_ate_ram[ate_index]);
			break;
		case PCIBR_BRIDGETYPE_PIC:
			writeq(val, &ptr->pic.p_int_ate_ram[ate_index]);
			break;
		default:
			panic
			    ("pcireg_int_ate_set: unknown bridgetype bridge 0x%p",
			     ptr);
		}
	}
}

u64 __iomem *pcireg_int_ate_addr(struct pcibus_info *pcibus_info, int ate_index)
{
	union br_ptr __iomem *ptr = (union br_ptr __iomem *)pcibus_info->pbi_buscommon.bs_base;
	u64 __iomem *ret = NULL;

	if (pcibus_info) {
		switch (pcibus_info->pbi_bridge_type) {
		case PCIBR_BRIDGETYPE_TIOCP:
			ret = &ptr->tio.cp_int_ate_ram[ate_index];
			break;
		case PCIBR_BRIDGETYPE_PIC:
			ret = &ptr->pic.p_int_ate_ram[ate_index];
			break;
		default:
			panic
			    ("pcireg_int_ate_addr: unknown bridgetype bridge 0x%p",
			     ptr);
		}
	}
	return ret;
}
