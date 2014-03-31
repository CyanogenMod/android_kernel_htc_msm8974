/*
 * io_sm.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * IO dispatcher for a shared memory channel driver.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/types.h>
#include <linux/list.h>

#include <dspbridge/host_os.h>
#include <linux/workqueue.h>

#include <dspbridge/dbdefs.h>

#include <dspbridge/ntfy.h>
#include <dspbridge/sync.h>

#include <hw_defs.h>
#include <hw_mmu.h>

#include <dspbridge/dspdeh.h>
#include <dspbridge/dspio.h>
#include <dspbridge/dspioctl.h>
#include <dspbridge/wdt.h>
#include <_tiomap.h>
#include <tiomap_io.h>
#include <_tiomap_pwr.h>

#include <dspbridge/cod.h>
#include <dspbridge/node.h>
#include <dspbridge/dev.h>

#include <dspbridge/rms_sh.h>
#include <dspbridge/mgr.h>
#include <dspbridge/drv.h>
#include "_cmm.h"
#include "module_list.h"

#include <dspbridge/io_sm.h>
#include "_msg_sm.h"

#define OUTPUTNOTREADY  0xffff
#define NOTENABLED      0xffff	

#define EXTEND      "_EXT_END"

#define SWAP_WORD(x)     (x)
#define UL_PAGE_ALIGN_SIZE 0x10000	

#define MAX_PM_REQS 32

#define MMU_FAULT_HEAD1 0xa5a5a5a5
#define MMU_FAULT_HEAD2 0x96969696
#define POLL_MAX 1000
#define MAX_MMU_DBGBUFF 10240

struct io_mgr {
	
	
	struct bridge_dev_context *bridge_context;
	
	struct bridge_drv_interface *intf_fxns;
	struct dev_object *dev_obj;	

	
	struct chnl_mgr *chnl_mgr;
	struct shm *shared_mem;	
	u8 *input;		
	u8 *output;		
	struct msg_mgr *msg_mgr;	
	
	struct msg_ctrl *msg_input_ctrl;
	
	struct msg_ctrl *msg_output_ctrl;
	u8 *msg_input;		
	u8 *msg_output;		
	u32 sm_buf_size;	
	bool shared_irq;	
	u32 word_size;		
	u16 intr_val;		
	
	struct mgr_processorextinfo ext_proc_info;
	struct cmm_object *cmm_mgr;	
	struct work_struct io_workq;	
#if defined(CONFIG_TIDSPBRIDGE_BACKTRACE)
	u32 trace_buffer_begin;	
	u32 trace_buffer_end;	
	u32 trace_buffer_current;	
	u32 gpp_read_pointer;		
	u8 *msg;
	u32 gpp_va;
	u32 dsp_va;
#endif
	
	u32 dpc_req;		
	u32 dpc_sched;		
	struct tasklet_struct dpc_tasklet;
	spinlock_t dpc_lock;

};

static void io_dispatch_pm(struct io_mgr *pio_mgr);
static void notify_chnl_complete(struct chnl_object *pchnl,
				 struct chnl_irp *chnl_packet_obj);
static void input_chnl(struct io_mgr *pio_mgr, struct chnl_object *pchnl,
			u8 io_mode);
static void output_chnl(struct io_mgr *pio_mgr, struct chnl_object *pchnl,
			u8 io_mode);
static void input_msg(struct io_mgr *pio_mgr, struct msg_mgr *hmsg_mgr);
static void output_msg(struct io_mgr *pio_mgr, struct msg_mgr *hmsg_mgr);
static u32 find_ready_output(struct chnl_mgr *chnl_mgr_obj,
			     struct chnl_object *pchnl, u32 mask);

static int register_shm_segs(struct io_mgr *hio_mgr,
				    struct cod_manager *cod_man,
				    u32 dw_gpp_base_pa);

static inline void set_chnl_free(struct shm *sm, u32 chnl)
{
	sm->host_free_mask &= ~(1 << chnl);
}

static inline void set_chnl_busy(struct shm *sm, u32 chnl)
{
	sm->host_free_mask |= 1 << chnl;
}


int bridge_io_create(struct io_mgr **io_man,
			    struct dev_object *hdev_obj,
			    const struct io_attrs *mgr_attrts)
{
	struct io_mgr *pio_mgr = NULL;
	struct bridge_dev_context *hbridge_context = NULL;
	struct cfg_devnode *dev_node_obj;
	struct chnl_mgr *hchnl_mgr;
	u8 dev_type;

	
	if (!io_man || !mgr_attrts || mgr_attrts->word_size == 0)
		return -EFAULT;

	*io_man = NULL;

	dev_get_chnl_mgr(hdev_obj, &hchnl_mgr);
	if (!hchnl_mgr || hchnl_mgr->iomgr)
		return -EFAULT;

	dev_get_bridge_context(hdev_obj, &hbridge_context);
	if (!hbridge_context)
		return -EFAULT;

	dev_get_dev_type(hdev_obj, &dev_type);

	
	pio_mgr = kzalloc(sizeof(struct io_mgr), GFP_KERNEL);
	if (!pio_mgr)
		return -ENOMEM;

	
	pio_mgr->chnl_mgr = hchnl_mgr;
	pio_mgr->word_size = mgr_attrts->word_size;

	if (dev_type == DSP_UNIT) {
		
		tasklet_init(&pio_mgr->dpc_tasklet, io_dpc, (u32) pio_mgr);

		
		pio_mgr->dpc_req = 0;
		pio_mgr->dpc_sched = 0;

		spin_lock_init(&pio_mgr->dpc_lock);

		if (dev_get_dev_node(hdev_obj, &dev_node_obj)) {
			bridge_io_destroy(pio_mgr);
			return -EIO;
		}
	}

	pio_mgr->bridge_context = hbridge_context;
	pio_mgr->shared_irq = mgr_attrts->irq_shared;
	if (dsp_wdt_init()) {
		bridge_io_destroy(pio_mgr);
		return -EPERM;
	}

	
	hchnl_mgr->iomgr = pio_mgr;
	*io_man = pio_mgr;

	return 0;
}

int bridge_io_destroy(struct io_mgr *hio_mgr)
{
	int status = 0;
	if (hio_mgr) {
		
		tasklet_kill(&hio_mgr->dpc_tasklet);

#if defined(CONFIG_TIDSPBRIDGE_BACKTRACE)
		kfree(hio_mgr->msg);
#endif
		dsp_wdt_exit();
		
		kfree(hio_mgr);
	} else {
		status = -EFAULT;
	}

	return status;
}

int bridge_io_on_loaded(struct io_mgr *hio_mgr)
{
	struct cod_manager *cod_man;
	struct chnl_mgr *hchnl_mgr;
	struct msg_mgr *hmsg_mgr;
	u32 ul_shm_base;
	u32 ul_shm_base_offset;
	u32 ul_shm_limit;
	u32 ul_shm_length = -1;
	u32 ul_mem_length = -1;
	u32 ul_msg_base;
	u32 ul_msg_limit;
	u32 ul_msg_length = -1;
	u32 ul_ext_end;
	u32 ul_gpp_pa = 0;
	u32 ul_gpp_va = 0;
	u32 ul_dsp_va = 0;
	u32 ul_seg_size = 0;
	u32 ul_pad_size = 0;
	u32 i;
	int status = 0;
	u8 num_procs = 0;
	s32 ndx = 0;
	
	struct bridge_ioctl_extproc ae_proc[BRDIOCTL_NUMOFMMUTLB];
	struct cfg_hostres *host_res;
	struct bridge_dev_context *pbridge_context;
	u32 map_attrs;
	u32 shm0_end;
	u32 ul_dyn_ext_base;
	u32 ul_seg1_size = 0;
	u32 pa_curr = 0;
	u32 va_curr = 0;
	u32 gpp_va_curr = 0;
	u32 num_bytes = 0;
	u32 all_bits = 0;
	u32 page_size[] = { HW_PAGE_SIZE16MB, HW_PAGE_SIZE1MB,
		HW_PAGE_SIZE64KB, HW_PAGE_SIZE4KB
	};

	status = dev_get_bridge_context(hio_mgr->dev_obj, &pbridge_context);
	if (!pbridge_context) {
		status = -EFAULT;
		goto func_end;
	}

	host_res = pbridge_context->resources;
	if (!host_res) {
		status = -EFAULT;
		goto func_end;
	}
	status = dev_get_cod_mgr(hio_mgr->dev_obj, &cod_man);
	if (!cod_man) {
		status = -EFAULT;
		goto func_end;
	}
	hchnl_mgr = hio_mgr->chnl_mgr;
	
	dev_get_msg_mgr(hio_mgr->dev_obj, &hio_mgr->msg_mgr);
	hmsg_mgr = hio_mgr->msg_mgr;
	if (!hchnl_mgr || !hmsg_mgr) {
		status = -EFAULT;
		goto func_end;
	}
	if (hio_mgr->shared_mem)
		hio_mgr->shared_mem = NULL;

	
	status = cod_get_sym_value(cod_man, CHNL_SHARED_BUFFER_BASE_SYM,
				   &ul_shm_base);
	if (status) {
		status = -EFAULT;
		goto func_end;
	}
	status = cod_get_sym_value(cod_man, CHNL_SHARED_BUFFER_LIMIT_SYM,
				   &ul_shm_limit);
	if (status) {
		status = -EFAULT;
		goto func_end;
	}
	if (ul_shm_limit <= ul_shm_base) {
		status = -EINVAL;
		goto func_end;
	}
	
	ul_shm_length = (ul_shm_limit - ul_shm_base + 1) * hio_mgr->word_size;
	
	dev_dbg(bridge, "%s: (proc)proccopy shmmem size: 0x%x bytes\n",
		__func__, (ul_shm_length - sizeof(struct shm)));

	
	status = cod_get_sym_value(cod_man, MSG_SHARED_BUFFER_BASE_SYM,
					   &ul_msg_base);
	if (!status) {
		status = cod_get_sym_value(cod_man, MSG_SHARED_BUFFER_LIMIT_SYM,
					   &ul_msg_limit);
		if (!status) {
			if (ul_msg_limit <= ul_msg_base) {
				status = -EINVAL;
			} else {
				ul_msg_length =
				    (ul_msg_limit - ul_msg_base +
				     1) * hio_mgr->word_size;
				ul_mem_length = ul_shm_length + ul_msg_length;
			}
		} else {
			status = -EFAULT;
		}
	} else {
		status = -EFAULT;
	}
	if (!status) {
#if defined(CONFIG_TIDSPBRIDGE_BACKTRACE)
		status =
		    cod_get_sym_value(cod_man, DSP_TRACESEC_END, &shm0_end);
#else
		status = cod_get_sym_value(cod_man, SHM0_SHARED_END_SYM,
					   &shm0_end);
#endif
		if (status)
			status = -EFAULT;
	}
	if (!status) {
		status =
		    cod_get_sym_value(cod_man, DYNEXTBASE, &ul_dyn_ext_base);
		if (status)
			status = -EFAULT;
	}
	if (!status) {
		status = cod_get_sym_value(cod_man, EXTEND, &ul_ext_end);
		if (status)
			status = -EFAULT;
	}
	if (!status) {
		
		(void)mgr_enum_processor_info(0, (struct dsp_processorinfo *)
					      &hio_mgr->ext_proc_info,
					      sizeof(struct
						     mgr_processorextinfo),
					      &num_procs);

		
		ndx = 0;
		ul_gpp_pa = host_res->mem_phys[1];
		ul_gpp_va = host_res->mem_base[1];
		
		
		ul_dsp_va = hio_mgr->ext_proc_info.ty_tlb[0].dsp_virt;
		ul_seg_size = (shm0_end - ul_dsp_va) * hio_mgr->word_size;
		ul_seg1_size =
		    (ul_ext_end - ul_dyn_ext_base) * hio_mgr->word_size;
		
		ul_seg1_size = (ul_seg1_size + 0xFFF) & (~0xFFFUL);
		
		ul_seg_size = (ul_seg_size + 0xFFFF) & (~0xFFFFUL);
		ul_pad_size = UL_PAGE_ALIGN_SIZE - ((ul_gpp_pa + ul_seg1_size) %
						    UL_PAGE_ALIGN_SIZE);
		if (ul_pad_size == UL_PAGE_ALIGN_SIZE)
			ul_pad_size = 0x0;

		dev_dbg(bridge, "%s: ul_gpp_pa %x, ul_gpp_va %x, ul_dsp_va %x, "
			"shm0_end %x, ul_dyn_ext_base %x, ul_ext_end %x, "
			"ul_seg_size %x ul_seg1_size %x \n", __func__,
			ul_gpp_pa, ul_gpp_va, ul_dsp_va, shm0_end,
			ul_dyn_ext_base, ul_ext_end, ul_seg_size, ul_seg1_size);

		if ((ul_seg_size + ul_seg1_size + ul_pad_size) >
		    host_res->mem_length[1]) {
			pr_err("%s: shm Error, reserved 0x%x required 0x%x\n",
			       __func__, host_res->mem_length[1],
			       ul_seg_size + ul_seg1_size + ul_pad_size);
			status = -ENOMEM;
		}
	}
	if (status)
		goto func_end;

	pa_curr = ul_gpp_pa;
	va_curr = ul_dyn_ext_base * hio_mgr->word_size;
	gpp_va_curr = ul_gpp_va;
	num_bytes = ul_seg1_size;

	map_attrs = 0x00000000;
	map_attrs = DSP_MAPLITTLEENDIAN;
	map_attrs |= DSP_MAPPHYSICALADDR;
	map_attrs |= DSP_MAPELEMSIZE32;
	map_attrs |= DSP_MAPDONOTLOCK;

	while (num_bytes) {
		all_bits = pa_curr | va_curr;
		dev_dbg(bridge, "all_bits %x, pa_curr %x, va_curr %x, "
			"num_bytes %x\n", all_bits, pa_curr, va_curr,
			num_bytes);
		for (i = 0; i < 4; i++) {
			if ((num_bytes >= page_size[i]) && ((all_bits &
							     (page_size[i] -
							      1)) == 0)) {
				status =
				    hio_mgr->intf_fxns->
				    brd_mem_map(hio_mgr->bridge_context,
						    pa_curr, va_curr,
						    page_size[i], map_attrs,
						    NULL);
				if (status)
					goto func_end;
				pa_curr += page_size[i];
				va_curr += page_size[i];
				gpp_va_curr += page_size[i];
				num_bytes -= page_size[i];
				break;
			}
		}
	}
	pa_curr += ul_pad_size;
	va_curr += ul_pad_size;
	gpp_va_curr += ul_pad_size;

	
	num_bytes = ul_seg_size;
	va_curr = ul_dsp_va * hio_mgr->word_size;
	while (num_bytes) {
		all_bits = pa_curr | va_curr;
		dev_dbg(bridge, "all_bits for Seg1 %x, pa_curr %x, "
			"va_curr %x, num_bytes %x\n", all_bits, pa_curr,
			va_curr, num_bytes);
		for (i = 0; i < 4; i++) {
			if (!(num_bytes >= page_size[i]) ||
			    !((all_bits & (page_size[i] - 1)) == 0))
				continue;
			if (ndx < MAX_LOCK_TLB_ENTRIES) {
				/*
				 * This is the physical address written to
				 * DSP MMU.
				 */
				ae_proc[ndx].gpp_pa = pa_curr;
				ae_proc[ndx].gpp_va = gpp_va_curr;
				ae_proc[ndx].dsp_va =
				    va_curr / hio_mgr->word_size;
				ae_proc[ndx].size = page_size[i];
				ae_proc[ndx].endianism = HW_LITTLE_ENDIAN;
				ae_proc[ndx].elem_size = HW_ELEM_SIZE16BIT;
				ae_proc[ndx].mixed_mode = HW_MMU_CPUES;
				dev_dbg(bridge, "shm MMU TLB entry PA %x"
					" VA %x DSP_VA %x Size %x\n",
					ae_proc[ndx].gpp_pa,
					ae_proc[ndx].gpp_va,
					ae_proc[ndx].dsp_va *
					hio_mgr->word_size, page_size[i]);
				ndx++;
			} else {
				status =
				    hio_mgr->intf_fxns->
				    brd_mem_map(hio_mgr->bridge_context,
						    pa_curr, va_curr,
						    page_size[i], map_attrs,
						    NULL);
				dev_dbg(bridge,
					"shm MMU PTE entry PA %x"
					" VA %x DSP_VA %x Size %x\n",
					ae_proc[ndx].gpp_pa,
					ae_proc[ndx].gpp_va,
					ae_proc[ndx].dsp_va *
					hio_mgr->word_size, page_size[i]);
				if (status)
					goto func_end;
			}
			pa_curr += page_size[i];
			va_curr += page_size[i];
			gpp_va_curr += page_size[i];
			num_bytes -= page_size[i];
			break;
		}
	}

	for (i = 3; i < 7 && ndx < BRDIOCTL_NUMOFMMUTLB; i++) {
		if (hio_mgr->ext_proc_info.ty_tlb[i].gpp_phys == 0)
			continue;

		if ((hio_mgr->ext_proc_info.ty_tlb[i].gpp_phys >
		     ul_gpp_pa - 0x100000
		     && hio_mgr->ext_proc_info.ty_tlb[i].gpp_phys <=
		     ul_gpp_pa + ul_seg_size)
		    || (hio_mgr->ext_proc_info.ty_tlb[i].dsp_virt >
			ul_dsp_va - 0x100000 / hio_mgr->word_size
			&& hio_mgr->ext_proc_info.ty_tlb[i].dsp_virt <=
			ul_dsp_va + ul_seg_size / hio_mgr->word_size)) {
			dev_dbg(bridge,
				"CDB MMU entry %d conflicts with "
				"shm.\n\tCDB: GppPa %x, DspVa %x.\n\tSHM: "
				"GppPa %x, DspVa %x, Bytes %x.\n", i,
				hio_mgr->ext_proc_info.ty_tlb[i].gpp_phys,
				hio_mgr->ext_proc_info.ty_tlb[i].dsp_virt,
				ul_gpp_pa, ul_dsp_va, ul_seg_size);
			status = -EPERM;
		} else {
			if (ndx < MAX_LOCK_TLB_ENTRIES) {
				ae_proc[ndx].dsp_va =
				    hio_mgr->ext_proc_info.ty_tlb[i].
				    dsp_virt;
				ae_proc[ndx].gpp_pa =
				    hio_mgr->ext_proc_info.ty_tlb[i].
				    gpp_phys;
				ae_proc[ndx].gpp_va = 0;
				
				ae_proc[ndx].size = 0x100000;
				dev_dbg(bridge, "shm MMU entry PA %x "
					"DSP_VA 0x%x\n", ae_proc[ndx].gpp_pa,
					ae_proc[ndx].dsp_va);
				ndx++;
			} else {
				status = hio_mgr->intf_fxns->brd_mem_map
				    (hio_mgr->bridge_context,
				     hio_mgr->ext_proc_info.ty_tlb[i].
				     gpp_phys,
				     hio_mgr->ext_proc_info.ty_tlb[i].
				     dsp_virt, 0x100000, map_attrs,
				     NULL);
			}
		}
		if (status)
			goto func_end;
	}

	map_attrs = 0x00000000;
	map_attrs = DSP_MAPLITTLEENDIAN;
	map_attrs |= DSP_MAPPHYSICALADDR;
	map_attrs |= DSP_MAPELEMSIZE32;
	map_attrs |= DSP_MAPDONOTLOCK;

	
	i = 0;
	while (l4_peripheral_table[i].phys_addr) {
		status = hio_mgr->intf_fxns->brd_mem_map
		    (hio_mgr->bridge_context, l4_peripheral_table[i].phys_addr,
		     l4_peripheral_table[i].dsp_virt_addr, HW_PAGE_SIZE4KB,
		     map_attrs, NULL);
		if (status)
			goto func_end;
		i++;
	}

	for (i = ndx; i < BRDIOCTL_NUMOFMMUTLB; i++) {
		ae_proc[i].dsp_va = 0;
		ae_proc[i].gpp_pa = 0;
		ae_proc[i].gpp_va = 0;
		ae_proc[i].size = 0;
	}
	hio_mgr->ext_proc_info.ty_tlb[0].gpp_phys =
	    (ul_gpp_va + ul_seg1_size + ul_pad_size);

	if (!hio_mgr->ext_proc_info.ty_tlb[0].gpp_phys || num_procs != 1) {
		status = -EFAULT;
		goto func_end;
	} else {
		if (ae_proc[0].dsp_va > ul_shm_base) {
			status = -EPERM;
			goto func_end;
		}
		
		ul_shm_base_offset = (ul_shm_base - ae_proc[0].dsp_va) *
		    hio_mgr->word_size;

		status =
		    hio_mgr->intf_fxns->dev_cntrl(hio_mgr->bridge_context,
						      BRDIOCTL_SETMMUCONFIG,
						      ae_proc);
		if (status)
			goto func_end;
		ul_shm_base = hio_mgr->ext_proc_info.ty_tlb[0].gpp_phys;
		ul_shm_base += ul_shm_base_offset;
		ul_shm_base = (u32) MEM_LINEAR_ADDRESS((void *)ul_shm_base,
						       ul_mem_length);
		if (ul_shm_base == 0) {
			status = -EFAULT;
			goto func_end;
		}
		
		status =
		    register_shm_segs(hio_mgr, cod_man, ae_proc[0].gpp_pa);
	}

	hio_mgr->shared_mem = (struct shm *)ul_shm_base;
	hio_mgr->input = (u8 *) hio_mgr->shared_mem + sizeof(struct shm);
	hio_mgr->output = hio_mgr->input + (ul_shm_length -
					    sizeof(struct shm)) / 2;
	hio_mgr->sm_buf_size = hio_mgr->output - hio_mgr->input;

	
	hio_mgr->msg_input_ctrl = (struct msg_ctrl *)((u8 *) hio_mgr->shared_mem
						      + ul_shm_length);
	hio_mgr->msg_input =
	    (u8 *) hio_mgr->msg_input_ctrl + sizeof(struct msg_ctrl);
	hio_mgr->msg_output_ctrl =
	    (struct msg_ctrl *)((u8 *) hio_mgr->msg_input_ctrl +
				ul_msg_length / 2);
	hio_mgr->msg_output =
	    (u8 *) hio_mgr->msg_output_ctrl + sizeof(struct msg_ctrl);
	hmsg_mgr->max_msgs =
	    ((u8 *) hio_mgr->msg_output_ctrl - hio_mgr->msg_input)
	    / sizeof(struct msg_dspmsg);
	dev_dbg(bridge, "IO MGR shm details: shared_mem %p, input %p, "
		"output %p, msg_input_ctrl %p, msg_input %p, "
		"msg_output_ctrl %p, msg_output %p\n",
		(u8 *) hio_mgr->shared_mem, hio_mgr->input,
		hio_mgr->output, (u8 *) hio_mgr->msg_input_ctrl,
		hio_mgr->msg_input, (u8 *) hio_mgr->msg_output_ctrl,
		hio_mgr->msg_output);
	dev_dbg(bridge, "(proc) Mas msgs in shared memory: 0x%x\n",
		hmsg_mgr->max_msgs);
	memset((void *)hio_mgr->shared_mem, 0, sizeof(struct shm));

#if defined(CONFIG_TIDSPBRIDGE_BACKTRACE)
	
	status = cod_get_sym_value(cod_man, SYS_PUTCBEG,
				   &hio_mgr->trace_buffer_begin);
	if (status) {
		status = -EFAULT;
		goto func_end;
	}

	hio_mgr->gpp_read_pointer = hio_mgr->trace_buffer_begin =
	    (ul_gpp_va + ul_seg1_size + ul_pad_size) +
	    (hio_mgr->trace_buffer_begin - ul_dsp_va);
	
	status = cod_get_sym_value(cod_man, SYS_PUTCEND,
				   &hio_mgr->trace_buffer_end);
	if (status) {
		status = -EFAULT;
		goto func_end;
	}
	hio_mgr->trace_buffer_end =
	    (ul_gpp_va + ul_seg1_size + ul_pad_size) +
	    (hio_mgr->trace_buffer_end - ul_dsp_va);
	
	status = cod_get_sym_value(cod_man, BRIDGE_SYS_PUTC_CURRENT,
				   &hio_mgr->trace_buffer_current);
	if (status) {
		status = -EFAULT;
		goto func_end;
	}
	hio_mgr->trace_buffer_current =
	    (ul_gpp_va + ul_seg1_size + ul_pad_size) +
	    (hio_mgr->trace_buffer_current - ul_dsp_va);
	
	kfree(hio_mgr->msg);
	hio_mgr->msg = kmalloc(((hio_mgr->trace_buffer_end -
				hio_mgr->trace_buffer_begin) *
				hio_mgr->word_size) + 2, GFP_KERNEL);
	if (!hio_mgr->msg)
		status = -ENOMEM;

	hio_mgr->dsp_va = ul_dsp_va;
	hio_mgr->gpp_va = (ul_gpp_va + ul_seg1_size + ul_pad_size);

#endif
func_end:
	return status;
}

u32 io_buf_size(struct io_mgr *hio_mgr)
{
	if (hio_mgr)
		return hio_mgr->sm_buf_size;
	else
		return 0;
}

void io_cancel_chnl(struct io_mgr *hio_mgr, u32 chnl)
{
	struct io_mgr *pio_mgr = (struct io_mgr *)hio_mgr;
	struct shm *sm;

	if (!hio_mgr)
		goto func_end;
	sm = hio_mgr->shared_mem;

	
	set_chnl_free(sm, chnl);

	sm_interrupt_dsp(pio_mgr->bridge_context, MBX_PCPY_CLASS);
func_end:
	return;
}


static void io_dispatch_pm(struct io_mgr *pio_mgr)
{
	int status;
	u32 parg[2];

	
	parg[0] = pio_mgr->intr_val;

	
	if (parg[0] == MBX_PM_HIBERNATE_EN) {
		dev_dbg(bridge, "PM: Hibernate command\n");
		status = pio_mgr->intf_fxns->
				dev_cntrl(pio_mgr->bridge_context,
					      BRDIOCTL_PWR_HIBERNATE, parg);
		if (status)
			pr_err("%s: hibernate cmd failed 0x%x\n",
				       __func__, status);
	} else if (parg[0] == MBX_PM_OPP_REQ) {
		parg[1] = pio_mgr->shared_mem->opp_request.rqst_opp_pt;
		dev_dbg(bridge, "PM: Requested OPP = 0x%x\n", parg[1]);
		status = pio_mgr->intf_fxns->
				dev_cntrl(pio_mgr->bridge_context,
					BRDIOCTL_CONSTRAINT_REQUEST, parg);
		if (status)
			dev_dbg(bridge, "PM: Failed to set constraint "
				"= 0x%x\n", parg[1]);
	} else {
		dev_dbg(bridge, "PM: clk control value of msg = 0x%x\n",
			parg[0]);
		status = pio_mgr->intf_fxns->
				dev_cntrl(pio_mgr->bridge_context,
					      BRDIOCTL_CLK_CTRL, parg);
		if (status)
			dev_dbg(bridge, "PM: Failed to ctrl the DSP clk"
				"= 0x%x\n", *parg);
	}
}

void io_dpc(unsigned long ref_data)
{
	struct io_mgr *pio_mgr = (struct io_mgr *)ref_data;
	struct chnl_mgr *chnl_mgr_obj;
	struct msg_mgr *msg_mgr_obj;
	struct deh_mgr *hdeh_mgr;
	u32 requested;
	u32 serviced;

	if (!pio_mgr)
		goto func_end;
	chnl_mgr_obj = pio_mgr->chnl_mgr;
	dev_get_msg_mgr(pio_mgr->dev_obj, &msg_mgr_obj);
	dev_get_deh_mgr(pio_mgr->dev_obj, &hdeh_mgr);
	if (!chnl_mgr_obj)
		goto func_end;

	requested = pio_mgr->dpc_req;
	serviced = pio_mgr->dpc_sched;

	if (serviced == requested)
		goto func_end;

	
	do {
		
		if ((pio_mgr->intr_val > DEH_BASE) &&
		    (pio_mgr->intr_val < DEH_LIMIT)) {
			
			if (hdeh_mgr) {
#ifdef CONFIG_TIDSPBRIDGE_BACKTRACE
				print_dsp_debug_trace(pio_mgr);
#endif
				bridge_deh_notify(hdeh_mgr, DSP_SYSERROR,
						  pio_mgr->intr_val);
			}
		}
		
		input_chnl(pio_mgr, NULL, IO_SERVICE);
		output_chnl(pio_mgr, NULL, IO_SERVICE);

#ifdef CHNL_MESSAGES
		if (msg_mgr_obj) {
			
			input_msg(pio_mgr, msg_mgr_obj);
			output_msg(pio_mgr, msg_mgr_obj);
		}

#endif
#ifdef CONFIG_TIDSPBRIDGE_BACKTRACE
		if (pio_mgr->intr_val & MBX_DBG_SYSPRINTF) {
			
			print_dsp_debug_trace(pio_mgr);
		}
#endif
		serviced++;
	} while (serviced != requested);
	pio_mgr->dpc_sched = requested;
func_end:
	return;
}

int io_mbox_msg(struct notifier_block *self, unsigned long len, void *msg)
{
	struct io_mgr *pio_mgr;
	struct dev_object *dev_obj;
	unsigned long flags;

	dev_obj = dev_get_first();
	dev_get_io_mgr(dev_obj, &pio_mgr);

	if (!pio_mgr)
		return NOTIFY_BAD;

	pio_mgr->intr_val = (u16)((u32)msg);
	if (pio_mgr->intr_val & MBX_PM_CLASS)
		io_dispatch_pm(pio_mgr);

	if (pio_mgr->intr_val == MBX_DEH_RESET) {
		pio_mgr->intr_val = 0;
	} else {
		spin_lock_irqsave(&pio_mgr->dpc_lock, flags);
		pio_mgr->dpc_req++;
		spin_unlock_irqrestore(&pio_mgr->dpc_lock, flags);
		tasklet_schedule(&pio_mgr->dpc_tasklet);
	}
	return NOTIFY_OK;
}

void io_request_chnl(struct io_mgr *io_manager, struct chnl_object *pchnl,
			u8 io_mode, u16 *mbx_val)
{
	struct chnl_mgr *chnl_mgr_obj;
	struct shm *sm;

	if (!pchnl || !mbx_val)
		goto func_end;
	chnl_mgr_obj = io_manager->chnl_mgr;
	sm = io_manager->shared_mem;
	if (io_mode == IO_INPUT) {
		
		set_chnl_busy(sm, pchnl->chnl_id);
		*mbx_val = MBX_PCPY_CLASS;
	} else if (io_mode == IO_OUTPUT) {
		chnl_mgr_obj->output_mask |= (1 << pchnl->chnl_id);
	} else {
	}
func_end:
	return;
}

void iosm_schedule(struct io_mgr *io_manager)
{
	unsigned long flags;

	if (!io_manager)
		return;

	
	spin_lock_irqsave(&io_manager->dpc_lock, flags);
	io_manager->dpc_req++;
	spin_unlock_irqrestore(&io_manager->dpc_lock, flags);

	
	tasklet_schedule(&io_manager->dpc_tasklet);
}

static u32 find_ready_output(struct chnl_mgr *chnl_mgr_obj,
			     struct chnl_object *pchnl, u32 mask)
{
	u32 ret = OUTPUTNOTREADY;
	u32 id, start_id;
	u32 shift;

	id = (pchnl !=
	      NULL ? pchnl->chnl_id : (chnl_mgr_obj->last_output + 1));
	id = ((id == CHNL_MAXCHANNELS) ? 0 : id);
	if (id >= CHNL_MAXCHANNELS)
		goto func_end;
	if (mask) {
		shift = (1 << id);
		start_id = id;
		do {
			if (mask & shift) {
				ret = id;
				if (pchnl == NULL)
					chnl_mgr_obj->last_output = id;
				break;
			}
			id = id + 1;
			id = ((id == CHNL_MAXCHANNELS) ? 0 : id);
			shift = (1 << id);
		} while (id != start_id);
	}
func_end:
	return ret;
}

static void input_chnl(struct io_mgr *pio_mgr, struct chnl_object *pchnl,
			u8 io_mode)
{
	struct chnl_mgr *chnl_mgr_obj;
	struct shm *sm;
	u32 chnl_id;
	u32 bytes;
	struct chnl_irp *chnl_packet_obj = NULL;
	u32 dw_arg;
	bool clear_chnl = false;
	bool notify_client = false;

	sm = pio_mgr->shared_mem;
	chnl_mgr_obj = pio_mgr->chnl_mgr;

	
	if (!sm->input_full)
		goto func_end;

	bytes = sm->input_size * chnl_mgr_obj->word_size;
	chnl_id = sm->input_id;
	dw_arg = sm->arg;
	if (chnl_id >= CHNL_MAXCHANNELS) {
		
		goto func_end;
	}
	pchnl = chnl_mgr_obj->channels[chnl_id];
	if ((pchnl != NULL) && CHNL_IS_INPUT(pchnl->chnl_mode)) {
		if ((pchnl->state & ~CHNL_STATEEOS) == CHNL_STATEREADY) {
			
			if (!list_empty(&pchnl->io_requests)) {
				if (!pchnl->cio_reqs)
					goto func_end;

				chnl_packet_obj = list_first_entry(
						&pchnl->io_requests,
						struct chnl_irp, link);
				list_del(&chnl_packet_obj->link);
				pchnl->cio_reqs--;

				bytes = min(bytes, chnl_packet_obj->byte_size);
				memcpy(chnl_packet_obj->host_sys_buf,
						pio_mgr->input, bytes);
				pchnl->bytes_moved += bytes;
				chnl_packet_obj->byte_size = bytes;
				chnl_packet_obj->arg = dw_arg;
				chnl_packet_obj->status = CHNL_IOCSTATCOMPLETE;

				if (bytes == 0) {
					if (pchnl->state & CHNL_STATEEOS)
						goto func_end;
					chnl_packet_obj->status |=
						CHNL_IOCSTATEOS;
					pchnl->state |= CHNL_STATEEOS;
					ntfy_notify(pchnl->ntfy_obj,
							DSP_STREAMDONE);
				}
				
				if (list_empty(&pchnl->io_requests))
					set_chnl_free(sm, pchnl->chnl_id);
				clear_chnl = true;
				notify_client = true;
			} else {
				clear_chnl = true;
			}
		} else {
			
			clear_chnl = true;
		}
	} else {
		
		clear_chnl = true;
	}
	if (clear_chnl) {
		
		sm->input_full = 0;
		sm_interrupt_dsp(pio_mgr->bridge_context, MBX_PCPY_CLASS);
	}
	if (notify_client) {
		
		notify_chnl_complete(pchnl, chnl_packet_obj);
	}
func_end:
	return;
}

static void input_msg(struct io_mgr *pio_mgr, struct msg_mgr *hmsg_mgr)
{
	u32 num_msgs;
	u32 i;
	u8 *msg_input;
	struct msg_queue *msg_queue_obj;
	struct msg_frame *pmsg;
	struct msg_dspmsg msg;
	struct msg_ctrl *msg_ctr_obj;
	u32 input_empty;
	u32 addr;

	msg_ctr_obj = pio_mgr->msg_input_ctrl;
	
	input_empty = msg_ctr_obj->buf_empty;
	num_msgs = msg_ctr_obj->size;
	if (input_empty)
		return;

	msg_input = pio_mgr->msg_input;
	for (i = 0; i < num_msgs; i++) {
		
		addr = (u32) &(((struct msg_dspmsg *)msg_input)->msg.cmd);
		msg.msg.cmd =
			read_ext32_bit_dsp_data(pio_mgr->bridge_context, addr);
		addr = (u32) &(((struct msg_dspmsg *)msg_input)->msg.arg1);
		msg.msg.arg1 =
			read_ext32_bit_dsp_data(pio_mgr->bridge_context, addr);
		addr = (u32) &(((struct msg_dspmsg *)msg_input)->msg.arg2);
		msg.msg.arg2 =
			read_ext32_bit_dsp_data(pio_mgr->bridge_context, addr);
		addr = (u32) &(((struct msg_dspmsg *)msg_input)->msgq_id);
		msg.msgq_id =
			read_ext32_bit_dsp_data(pio_mgr->bridge_context, addr);
		msg_input += sizeof(struct msg_dspmsg);

		
		dev_dbg(bridge,	"input msg: cmd=0x%x arg1=0x%x "
				"arg2=0x%x msgq_id=0x%x\n", msg.msg.cmd,
				msg.msg.arg1, msg.msg.arg2, msg.msgq_id);
		list_for_each_entry(msg_queue_obj, &hmsg_mgr->queue_list,
				list_elem) {
			if (msg.msgq_id != msg_queue_obj->msgq_id)
				continue;
			
			if (msg.msg.cmd == RMS_EXITACK) {
				(*hmsg_mgr->on_exit)(msg_queue_obj->arg,
						msg.msg.arg1);
				break;
			}
			if (list_empty(&msg_queue_obj->msg_free_list)) {
				pr_err("%s: no free msg frames,"
						" discarding msg\n",
						__func__);
				break;
			}

			pmsg = list_first_entry(&msg_queue_obj->msg_free_list,
					struct msg_frame, list_elem);
			list_del(&pmsg->list_elem);
			pmsg->msg_data = msg;
			list_add_tail(&pmsg->list_elem,
					&msg_queue_obj->msg_used_list);
			ntfy_notify(msg_queue_obj->ntfy_obj,
					DSP_NODEMESSAGEREADY);
			sync_set_event(msg_queue_obj->sync_event);
		}
	}
	
	if (num_msgs > 0) {
		
		msg_ctr_obj->buf_empty = true;
		msg_ctr_obj->post_swi = true;
		sm_interrupt_dsp(pio_mgr->bridge_context, MBX_PCPY_CLASS);
	}
}

static void notify_chnl_complete(struct chnl_object *pchnl,
				 struct chnl_irp *chnl_packet_obj)
{
	bool signal_event;

	if (!pchnl || !pchnl->sync_event || !chnl_packet_obj)
		goto func_end;

	signal_event = list_empty(&pchnl->io_completions);
	
	list_add_tail(&chnl_packet_obj->link, &pchnl->io_completions);
	pchnl->cio_cs++;

	if (pchnl->cio_cs > pchnl->chnl_packets)
		goto func_end;
	
	if (signal_event)
		sync_set_event(pchnl->sync_event);

	
	ntfy_notify(pchnl->ntfy_obj, DSP_STREAMIOCOMPLETION);
func_end:
	return;
}

static void output_chnl(struct io_mgr *pio_mgr, struct chnl_object *pchnl,
			u8 io_mode)
{
	struct chnl_mgr *chnl_mgr_obj;
	struct shm *sm;
	u32 chnl_id;
	struct chnl_irp *chnl_packet_obj;
	u32 dw_dsp_f_mask;

	chnl_mgr_obj = pio_mgr->chnl_mgr;
	sm = pio_mgr->shared_mem;
	
	if (sm->output_full)
		goto func_end;

	if (pchnl && !((pchnl->state & ~CHNL_STATEEOS) == CHNL_STATEREADY))
		goto func_end;

	
	dw_dsp_f_mask = sm->dsp_free_mask;
	chnl_id =
	    find_ready_output(chnl_mgr_obj, pchnl,
			      (chnl_mgr_obj->output_mask & dw_dsp_f_mask));
	if (chnl_id == OUTPUTNOTREADY)
		goto func_end;

	pchnl = chnl_mgr_obj->channels[chnl_id];
	if (!pchnl || list_empty(&pchnl->io_requests)) {
		
		goto func_end;
	}

	if (!pchnl->cio_reqs)
		goto func_end;

	
	chnl_packet_obj = list_first_entry(&pchnl->io_requests,
			struct chnl_irp, link);
	list_del(&chnl_packet_obj->link);

	pchnl->cio_reqs--;

	
	if (list_empty(&pchnl->io_requests))
		chnl_mgr_obj->output_mask &= ~(1 << chnl_id);

	
	chnl_packet_obj->byte_size = min(pio_mgr->sm_buf_size,
					chnl_packet_obj->byte_size);
	memcpy(pio_mgr->output,	chnl_packet_obj->host_sys_buf,
					chnl_packet_obj->byte_size);
	pchnl->bytes_moved += chnl_packet_obj->byte_size;
	
	sm->arg = chnl_packet_obj->arg;
#if _CHNL_WORDSIZE == 2
	
	sm->output_id = (u16) chnl_id;
	sm->output_size = (u16) (chnl_packet_obj->byte_size +
				chnl_mgr_obj->word_size - 1) /
				(u16) chnl_mgr_obj->word_size;
#else
	sm->output_id = chnl_id;
	sm->output_size = (chnl_packet_obj->byte_size +
			chnl_mgr_obj->word_size - 1) / chnl_mgr_obj->word_size;
#endif
	sm->output_full =  1;
	/* Indicate to the DSP we have written the output */
	sm_interrupt_dsp(pio_mgr->bridge_context, MBX_PCPY_CLASS);
	
	chnl_packet_obj->status &= CHNL_IOCSTATEOS;
	notify_chnl_complete(pchnl, chnl_packet_obj);
	
	if (chnl_packet_obj->status & CHNL_IOCSTATEOS)
		ntfy_notify(pchnl->ntfy_obj, DSP_STREAMDONE);

func_end:
	return;
}

static void output_msg(struct io_mgr *pio_mgr, struct msg_mgr *hmsg_mgr)
{
	u32 num_msgs = 0;
	u32 i;
	struct msg_dspmsg *msg_output;
	struct msg_frame *pmsg;
	struct msg_ctrl *msg_ctr_obj;
	u32 val;
	u32 addr;

	msg_ctr_obj = pio_mgr->msg_output_ctrl;

	
	if (!msg_ctr_obj->buf_empty)
		return;

	num_msgs = (hmsg_mgr->msgs_pending > hmsg_mgr->max_msgs) ?
		hmsg_mgr->max_msgs : hmsg_mgr->msgs_pending;
	msg_output = (struct msg_dspmsg *) pio_mgr->msg_output;

	
	for (i = 0; i < num_msgs; i++) {
		if (list_empty(&hmsg_mgr->msg_used_list))
			continue;

		pmsg = list_first_entry(&hmsg_mgr->msg_used_list,
				struct msg_frame, list_elem);
		list_del(&pmsg->list_elem);

		val = (pmsg->msg_data).msgq_id;
		addr = (u32) &msg_output->msgq_id;
		write_ext32_bit_dsp_data(pio_mgr->bridge_context, addr, val);

		val = (pmsg->msg_data).msg.cmd;
		addr = (u32) &msg_output->msg.cmd;
		write_ext32_bit_dsp_data(pio_mgr->bridge_context, addr, val);

		val = (pmsg->msg_data).msg.arg1;
		addr = (u32) &msg_output->msg.arg1;
		write_ext32_bit_dsp_data(pio_mgr->bridge_context, addr, val);

		val = (pmsg->msg_data).msg.arg2;
		addr = (u32) &msg_output->msg.arg2;
		write_ext32_bit_dsp_data(pio_mgr->bridge_context, addr, val);

		msg_output++;
		list_add_tail(&pmsg->list_elem, &hmsg_mgr->msg_free_list);
		sync_set_event(hmsg_mgr->sync_event);
	}

	if (num_msgs > 0) {
		hmsg_mgr->msgs_pending -= num_msgs;
#if _CHNL_WORDSIZE == 2
		msg_ctr_obj->size = (u16) num_msgs;
#else
		msg_ctr_obj->size = num_msgs;
#endif
		msg_ctr_obj->buf_empty = false;
		
		msg_ctr_obj->post_swi = true;
		/* Tell the DSP we have written the output. */
		sm_interrupt_dsp(pio_mgr->bridge_context, MBX_PCPY_CLASS);
	}
}

static int register_shm_segs(struct io_mgr *hio_mgr,
				    struct cod_manager *cod_man,
				    u32 dw_gpp_base_pa)
{
	int status = 0;
	u32 ul_shm0_base = 0;
	u32 shm0_end = 0;
	u32 ul_shm0_rsrvd_start = 0;
	u32 ul_rsrvd_size = 0;
	u32 ul_gpp_phys;
	u32 ul_dsp_virt;
	u32 ul_shm_seg_id0 = 0;
	u32 dw_offset, dw_gpp_base_va, ul_dsp_size;

	status =
	    cod_get_sym_value(cod_man, SHM0_SHARED_BASE_SYM, &ul_shm0_base);
	if (ul_shm0_base == 0) {
		status = -EPERM;
		goto func_end;
	}
	
	if (!status) {
		
		status = cod_get_sym_value(cod_man, SHM0_SHARED_END_SYM,
					   &shm0_end);
		if (shm0_end == 0) {
			status = -EPERM;
			goto func_end;
		}
	}
	
	if (!status) {
		
		status =
		    cod_get_sym_value(cod_man, SHM0_SHARED_RESERVED_BASE_SYM,
				      &ul_shm0_rsrvd_start);
		if (ul_shm0_rsrvd_start == 0) {
			status = -EPERM;
			goto func_end;
		}
	}
	
	if (!status) {
		status = dev_get_cmm_mgr(hio_mgr->dev_obj, &hio_mgr->cmm_mgr);
		if (!status) {
			status = cmm_un_register_gppsm_seg(hio_mgr->cmm_mgr,
							   CMM_ALLSEGMENTS);
		}
	}
	
	if (!status && (shm0_end - ul_shm0_base) > 0) {
		
		ul_rsrvd_size =
		    (shm0_end - ul_shm0_rsrvd_start + 1) * hio_mgr->word_size;
		if (ul_rsrvd_size <= 0) {
			status = -EPERM;
			goto func_end;
		}
		
		ul_dsp_size =
		    (ul_shm0_rsrvd_start - ul_shm0_base) * hio_mgr->word_size;
		if (ul_dsp_size <= 0) {
			status = -EPERM;
			goto func_end;
		}
		
		ul_gpp_phys = hio_mgr->ext_proc_info.ty_tlb[0].gpp_phys;
		
		ul_dsp_virt =
		    hio_mgr->ext_proc_info.ty_tlb[0].dsp_virt *
		    hio_mgr->word_size;
		if (dw_gpp_base_pa > ul_dsp_virt)
			dw_offset = dw_gpp_base_pa - ul_dsp_virt;
		else
			dw_offset = ul_dsp_virt - dw_gpp_base_pa;

		if (ul_shm0_rsrvd_start * hio_mgr->word_size < ul_dsp_virt) {
			status = -EPERM;
			goto func_end;
		}
		dw_gpp_base_va =
		    ul_gpp_phys + ul_shm0_rsrvd_start * hio_mgr->word_size -
		    ul_dsp_virt;
		dw_gpp_base_pa =
		    dw_gpp_base_pa + ul_shm0_rsrvd_start * hio_mgr->word_size -
		    ul_dsp_virt;
		
		status =
		    cmm_register_gppsm_seg(hio_mgr->cmm_mgr, dw_gpp_base_pa,
					   ul_rsrvd_size, dw_offset,
					   (dw_gpp_base_pa >
					    ul_dsp_virt) ? CMM_ADDTODSPPA :
					   CMM_SUBFROMDSPPA,
					   (u32) (ul_shm0_base *
						  hio_mgr->word_size),
					   ul_dsp_size, &ul_shm_seg_id0,
					   dw_gpp_base_va);
		
		if (ul_shm_seg_id0 != 1)
			status = -EPERM;
	}
func_end:
	return status;
}

int io_sh_msetting(struct io_mgr *hio_mgr, u8 desc, void *pargs)
{
#ifdef CONFIG_TIDSPBRIDGE_DVFS
	u32 i;
	struct dspbridge_platform_data *pdata =
	    omap_dspbridge_dev->dev.platform_data;

	switch (desc) {
	case SHM_CURROPP:
		
		if (pargs != NULL)
			hio_mgr->shared_mem->opp_table_struct.curr_opp_pt =
			    *(u32 *) pargs;
		else
			return -EPERM;
		break;
	case SHM_OPPINFO:
		for (i = 0; i <= dsp_max_opps; i++) {
			hio_mgr->shared_mem->opp_table_struct.opp_point[i].
			    voltage = vdd1_dsp_freq[i][0];
			dev_dbg(bridge, "OPP-shm: voltage: %d\n",
				vdd1_dsp_freq[i][0]);
			hio_mgr->shared_mem->opp_table_struct.
			    opp_point[i].frequency = vdd1_dsp_freq[i][1];
			dev_dbg(bridge, "OPP-shm: frequency: %d\n",
				vdd1_dsp_freq[i][1]);
			hio_mgr->shared_mem->opp_table_struct.opp_point[i].
			    min_freq = vdd1_dsp_freq[i][2];
			dev_dbg(bridge, "OPP-shm: min freq: %d\n",
				vdd1_dsp_freq[i][2]);
			hio_mgr->shared_mem->opp_table_struct.opp_point[i].
			    max_freq = vdd1_dsp_freq[i][3];
			dev_dbg(bridge, "OPP-shm: max freq: %d\n",
				vdd1_dsp_freq[i][3]);
		}
		hio_mgr->shared_mem->opp_table_struct.num_opp_pts =
		    dsp_max_opps;
		dev_dbg(bridge, "OPP-shm: max OPP number: %d\n", dsp_max_opps);
		
		if (pdata->dsp_get_opp)
			i = (*pdata->dsp_get_opp) ();
		hio_mgr->shared_mem->opp_table_struct.curr_opp_pt = i;
		dev_dbg(bridge, "OPP-shm: value programmed = %d\n", i);
		break;
	case SHM_GETOPP:
		
		*(u32 *) pargs = hio_mgr->shared_mem->opp_request.rqst_opp_pt;
		break;
	default:
		break;
	}
#endif
	return 0;
}

int bridge_io_get_proc_load(struct io_mgr *hio_mgr,
				struct dsp_procloadstat *proc_lstat)
{
	if (!hio_mgr->shared_mem)
		return -EFAULT;

	proc_lstat->curr_load =
			hio_mgr->shared_mem->load_mon_info.curr_dsp_load;
	proc_lstat->predicted_load =
	    hio_mgr->shared_mem->load_mon_info.pred_dsp_load;
	proc_lstat->curr_dsp_freq =
	    hio_mgr->shared_mem->load_mon_info.curr_dsp_freq;
	proc_lstat->predicted_freq =
	    hio_mgr->shared_mem->load_mon_info.pred_dsp_freq;

	dev_dbg(bridge, "Curr Load = %d, Pred Load = %d, Curr Freq = %d, "
		"Pred Freq = %d\n", proc_lstat->curr_load,
		proc_lstat->predicted_load, proc_lstat->curr_dsp_freq,
		proc_lstat->predicted_freq);
	return 0;
}


#if defined(CONFIG_TIDSPBRIDGE_BACKTRACE)
void print_dsp_debug_trace(struct io_mgr *hio_mgr)
{
	u32 ul_new_message_length = 0, ul_gpp_cur_pointer;

	while (true) {
		
		ul_gpp_cur_pointer =
		    *(u32 *) (hio_mgr->trace_buffer_current);
		ul_gpp_cur_pointer =
		    hio_mgr->gpp_va + (ul_gpp_cur_pointer -
					  hio_mgr->dsp_va);

		
		if (ul_gpp_cur_pointer == hio_mgr->gpp_read_pointer) {
			break;
		} else if (ul_gpp_cur_pointer > hio_mgr->gpp_read_pointer) {
			
			ul_new_message_length =
			    ul_gpp_cur_pointer - hio_mgr->gpp_read_pointer;

			memcpy(hio_mgr->msg,
			       (char *)hio_mgr->gpp_read_pointer,
			       ul_new_message_length);
			hio_mgr->msg[ul_new_message_length] = '\0';
			hio_mgr->gpp_read_pointer += ul_new_message_length;
			
			pr_info("DSPTrace: %s\n", hio_mgr->msg);
		} else if (ul_gpp_cur_pointer < hio_mgr->gpp_read_pointer) {
			
			memcpy(hio_mgr->msg,
			       (char *)hio_mgr->gpp_read_pointer,
			       hio_mgr->trace_buffer_end -
			       hio_mgr->gpp_read_pointer);
			ul_new_message_length =
			    ul_gpp_cur_pointer - hio_mgr->trace_buffer_begin;
			memcpy(&hio_mgr->msg[hio_mgr->trace_buffer_end -
					      hio_mgr->gpp_read_pointer],
			       (char *)hio_mgr->trace_buffer_begin,
			       ul_new_message_length);
			hio_mgr->msg[hio_mgr->trace_buffer_end -
				      hio_mgr->gpp_read_pointer +
				      ul_new_message_length] = '\0';
			hio_mgr->gpp_read_pointer =
			    hio_mgr->trace_buffer_begin +
			    ul_new_message_length;
			
			pr_info("DSPTrace: %s\n", hio_mgr->msg);
		}
	}
}
#endif

#ifdef CONFIG_TIDSPBRIDGE_BACKTRACE
int print_dsp_trace_buffer(struct bridge_dev_context *hbridge_context)
{
	int status = 0;
	struct cod_manager *cod_mgr;
	u32 ul_trace_end;
	u32 ul_trace_begin;
	u32 trace_cur_pos;
	u32 ul_num_bytes = 0;
	u32 ul_num_words = 0;
	u32 ul_word_size = 2;
	char *psz_buf;
	char *str_beg;
	char *trace_end;
	char *buf_end;
	char *new_line;

	struct bridge_dev_context *pbridge_context = hbridge_context;
	struct bridge_drv_interface *intf_fxns;
	struct dev_object *dev_obj = (struct dev_object *)
	    pbridge_context->dev_obj;

	status = dev_get_cod_mgr(dev_obj, &cod_mgr);

	if (cod_mgr) {
		
		status =
		    cod_get_sym_value(cod_mgr, COD_TRACEBEG, &ul_trace_begin);
	} else {
		status = -EFAULT;
	}
	if (!status)
		status =
		    cod_get_sym_value(cod_mgr, COD_TRACEEND, &ul_trace_end);

	if (!status)
		
		status = cod_get_sym_value(cod_mgr, COD_TRACECURPOS,
							&trace_cur_pos);

	if (status)
		goto func_end;

	ul_num_bytes = (ul_trace_end - ul_trace_begin);

	ul_num_words = ul_num_bytes * ul_word_size;
	status = dev_get_intf_fxns(dev_obj, &intf_fxns);

	if (status)
		goto func_end;

	psz_buf = kzalloc(ul_num_bytes + 2, GFP_ATOMIC);
	if (psz_buf != NULL) {
		
		status = (*intf_fxns->brd_read)(pbridge_context,
			(u8 *)psz_buf, (u32)ul_trace_begin,
			ul_num_bytes, 0);

		if (status)
			goto func_end;

		
		pr_debug("PrintDspTraceBuffer: "
			"before pack and unpack.\n");
		pr_debug("%s: DSP Trace Buffer Begin:\n"
			"=======================\n%s\n",
			__func__, psz_buf);

		
		status = (*intf_fxns->brd_read)(pbridge_context,
				(u8 *)&trace_cur_pos, (u32)trace_cur_pos,
				4, 0);
		if (status)
			goto func_end;
		
		pr_info("DSP Trace Buffer Begin:\n"
			"=======================\n%s\n",
			psz_buf);


		
		trace_cur_pos = trace_cur_pos - ul_trace_begin;

		if (ul_num_bytes) {
			buf_end = &psz_buf[ul_num_bytes+1];
			
			trace_end = &psz_buf[trace_cur_pos];

			str_beg = trace_end;
			ul_num_bytes = buf_end - str_beg;

			while (str_beg < buf_end) {
				new_line = strnchr(str_beg, ul_num_bytes,
								'\n');
				if (new_line && new_line < buf_end) {
					*new_line = 0;
					pr_debug("%s\n", str_beg);
					str_beg = ++new_line;
					ul_num_bytes = buf_end - str_beg;
				} else {
					if (*str_beg != '\0') {
						str_beg[ul_num_bytes] = 0;
						pr_debug("%s\n", str_beg);
					}
					str_beg = buf_end;
					ul_num_bytes = 0;
				}
			}
			str_beg = psz_buf;
			ul_num_bytes = trace_end - str_beg;

			while (str_beg < trace_end) {
				new_line = strnchr(str_beg, ul_num_bytes, '\n');
				if (new_line != NULL && new_line < trace_end) {
					*new_line = 0;
					pr_debug("%s\n", str_beg);
					str_beg = ++new_line;
					ul_num_bytes = trace_end - str_beg;
				} else {
					if (*str_beg != '\0') {
						str_beg[ul_num_bytes] = 0;
						pr_debug("%s\n", str_beg);
					}
					str_beg = trace_end;
					ul_num_bytes = 0;
				}
			}
		}
		pr_info("\n=======================\n"
			"DSP Trace Buffer End:\n");
		kfree(psz_buf);
	} else {
		status = -ENOMEM;
	}
func_end:
	if (status)
		dev_dbg(bridge, "%s Failed, status 0x%x\n", __func__, status);
	return status;
}

int dump_dsp_stack(struct bridge_dev_context *bridge_context)
{
	int status = 0;
	struct cod_manager *code_mgr;
	struct node_mgr *node_mgr;
	u32 trace_begin;
	char name[256];
	struct {
		u32 head[2];
		u32 size;
	} mmu_fault_dbg_info;
	u32 *buffer;
	u32 *buffer_beg;
	u32 *buffer_end;
	u32 exc_type;
	u32 dyn_ext_base;
	u32 i;
	u32 offset_output;
	u32 total_size;
	u32 poll_cnt;
	const char *dsp_regs[] = {"EFR", "IERR", "ITSR", "NTSR",
				"IRP", "NRP", "AMR", "SSR",
				"ILC", "RILC", "IER", "CSR"};
	const char *exec_ctxt[] = {"Task", "SWI", "HWI", "Unknown"};
	struct bridge_drv_interface *intf_fxns;
	struct dev_object *dev_object = bridge_context->dev_obj;

	status = dev_get_cod_mgr(dev_object, &code_mgr);
	if (!code_mgr) {
		pr_debug("%s: Failed on dev_get_cod_mgr.\n", __func__);
		status = -EFAULT;
	}

	if (!status) {
		status = dev_get_node_manager(dev_object, &node_mgr);
		if (!node_mgr) {
			pr_debug("%s: Failed on dev_get_node_manager.\n",
								__func__);
			status = -EFAULT;
		}
	}

	if (!status) {
		
		status =
			cod_get_sym_value(code_mgr, COD_TRACEBEG, &trace_begin);
		pr_debug("%s: trace_begin Value 0x%x\n",
			__func__, trace_begin);
		if (status)
			pr_debug("%s: Failed on cod_get_sym_value.\n",
								__func__);
	}
	if (!status)
		status = dev_get_intf_fxns(dev_object, &intf_fxns);
	mmu_fault_dbg_info.head[0] = 0;
	mmu_fault_dbg_info.head[1] = 0;
	if (!status) {
		poll_cnt = 0;
		while ((mmu_fault_dbg_info.head[0] != MMU_FAULT_HEAD1 ||
			mmu_fault_dbg_info.head[1] != MMU_FAULT_HEAD2) &&
			poll_cnt < POLL_MAX) {

			
			status = (*intf_fxns->brd_read)(bridge_context,
				(u8 *)&mmu_fault_dbg_info, (u32)trace_begin,
				sizeof(mmu_fault_dbg_info), 0);

			if (status)
				break;

			poll_cnt++;
		}

		if (mmu_fault_dbg_info.head[0] != MMU_FAULT_HEAD1 &&
			mmu_fault_dbg_info.head[1] != MMU_FAULT_HEAD2) {
			status = -ETIME;
			pr_err("%s:No DSP MMU-Fault information available.\n",
							__func__);
		}
	}

	if (!status) {
		total_size = mmu_fault_dbg_info.size;
		
		if (total_size > MAX_MMU_DBGBUFF)
			total_size = MAX_MMU_DBGBUFF;

		buffer = kzalloc(total_size, GFP_ATOMIC);
		if (!buffer) {
			status = -ENOMEM;
			pr_debug("%s: Failed to "
				"allocate stack dump buffer.\n", __func__);
			goto func_end;
		}

		buffer_beg = buffer;
		buffer_end =  buffer + total_size / 4;

		
		status = (*intf_fxns->brd_read)(bridge_context,
				(u8 *)buffer, (u32)trace_begin,
				total_size, 0);
		if (status) {
			pr_debug("%s: Failed to Read Trace Buffer.\n",
								__func__);
			goto func_end;
		}

		pr_err("\nAproximate Crash Position:\n"
			"--------------------------\n");

		exc_type = buffer[3];
		if (!exc_type)
			i = buffer[79];         
		else
			i = buffer[80];         

		status =
		    cod_get_sym_value(code_mgr, DYNEXTBASE, &dyn_ext_base);
		if (status) {
			status = -EFAULT;
			goto func_end;
		}

		if ((i > dyn_ext_base) && (node_find_addr(node_mgr, i,
			0x1000, &offset_output, name) == 0))
			pr_err("0x%-8x [\"%s\" + 0x%x]\n", i, name,
							i - offset_output);
		else
			pr_err("0x%-8x [Unable to match to a symbol.]\n", i);

		buffer += 4;

		pr_err("\nExecution Info:\n"
			"---------------\n");

		if (*buffer < ARRAY_SIZE(exec_ctxt)) {
			pr_err("Execution context \t%s\n",
				exec_ctxt[*buffer++]);
		} else {
			pr_err("Execution context corrupt\n");
			kfree(buffer_beg);
			return -EFAULT;
		}
		pr_err("Task Handle\t\t0x%x\n", *buffer++);
		pr_err("Stack Pointer\t\t0x%x\n", *buffer++);
		pr_err("Stack Top\t\t0x%x\n", *buffer++);
		pr_err("Stack Bottom\t\t0x%x\n", *buffer++);
		pr_err("Stack Size\t\t0x%x\n", *buffer++);
		pr_err("Stack Size In Use\t0x%x\n", *buffer++);

		pr_err("\nCPU Registers\n"
			"---------------\n");

		for (i = 0; i < 32; i++) {
			if (i == 4 || i == 6 || i == 8)
				pr_err("A%d 0x%-8x [Function Argument %d]\n",
							i, *buffer++, i-3);
			else if (i == 15)
				pr_err("A15 0x%-8x [Frame Pointer]\n",
								*buffer++);
			else
				pr_err("A%d 0x%x\n", i, *buffer++);
		}

		pr_err("\nB0 0x%x\n", *buffer++);
		pr_err("B1 0x%x\n", *buffer++);
		pr_err("B2 0x%x\n", *buffer++);

		if ((*buffer > dyn_ext_base) && (node_find_addr(node_mgr,
			*buffer, 0x1000, &offset_output, name) == 0))

			pr_err("B3 0x%-8x [Function Return Pointer:"
				" \"%s\" + 0x%x]\n", *buffer, name,
				*buffer - offset_output);
		else
			pr_err("B3 0x%-8x [Function Return Pointer:"
				"Unable to match to a symbol.]\n", *buffer);

		buffer++;

		for (i = 4; i < 32; i++) {
			if (i == 4 || i == 6 || i == 8)
				pr_err("B%d 0x%-8x [Function Argument %d]\n",
							i, *buffer++, i-2);
			else if (i == 14)
				pr_err("B14 0x%-8x [Data Page Pointer]\n",
								*buffer++);
			else
				pr_err("B%d 0x%x\n", i, *buffer++);
		}

		pr_err("\n");

		for (i = 0; i < ARRAY_SIZE(dsp_regs); i++)
			pr_err("%s 0x%x\n", dsp_regs[i], *buffer++);

		pr_err("\nStack:\n"
			"------\n");

		for (i = 0; buffer < buffer_end; i++, buffer++) {
			if ((*buffer > dyn_ext_base) && (
				node_find_addr(node_mgr, *buffer , 0x600,
				&offset_output, name) == 0))
				pr_err("[%d] 0x%-8x [\"%s\" + 0x%x]\n",
					i, *buffer, name,
					*buffer - offset_output);
			else
				pr_err("[%d] 0x%x\n", i, *buffer);
		}
		kfree(buffer_beg);
	}
func_end:
	return status;
}

void dump_dl_modules(struct bridge_dev_context *bridge_context)
{
	struct cod_manager *code_mgr;
	struct bridge_drv_interface *intf_fxns;
	struct bridge_dev_context *bridge_ctxt = bridge_context;
	struct dev_object *dev_object = bridge_ctxt->dev_obj;
	struct modules_header modules_hdr;
	struct dll_module *module_struct = NULL;
	u32 module_dsp_addr;
	u32 module_size;
	u32 module_struct_size = 0;
	u32 sect_ndx;
	char *sect_str ;
	int status = 0;

	status = dev_get_intf_fxns(dev_object, &intf_fxns);
	if (status) {
		pr_debug("%s: Failed on dev_get_intf_fxns.\n", __func__);
		goto func_end;
	}

	status = dev_get_cod_mgr(dev_object, &code_mgr);
	if (!code_mgr) {
		pr_debug("%s: Failed on dev_get_cod_mgr.\n", __func__);
		status = -EFAULT;
		goto func_end;
	}

	
	status = cod_get_sym_value(code_mgr, "_DLModules", &module_dsp_addr);
	if (status) {
		pr_debug("%s: Failed on cod_get_sym_value for _DLModules.\n",
			__func__);
		goto func_end;
	}

	pr_debug("%s: _DLModules at 0x%x\n", __func__, module_dsp_addr);

	
	status = (*intf_fxns->brd_read)(bridge_context, (u8 *) &modules_hdr,
				(u32) module_dsp_addr, sizeof(modules_hdr), 0);

	if (status) {
		pr_debug("%s: Failed failed to read modules header.\n",
								__func__);
		goto func_end;
	}

	module_dsp_addr = modules_hdr.first_module;
	module_size = modules_hdr.first_module_size;

	pr_debug("%s: dll_module_header 0x%x %d\n", __func__, module_dsp_addr,
								module_size);

	pr_err("\nDynamically Loaded Modules:\n"
		"---------------------------\n");

	
	while (module_size) {
		if (module_size > module_struct_size) {
			kfree(module_struct);
			module_struct = kzalloc(module_size+128, GFP_ATOMIC);
			module_struct_size = module_size+128;
			pr_debug("%s: allocated module struct %p %d\n",
				__func__, module_struct, module_struct_size);
			if (!module_struct)
				goto func_end;
		}
		
		status = (*intf_fxns->brd_read)(bridge_context,
			(u8 *)module_struct, module_dsp_addr, module_size, 0);

		if (status) {
			pr_debug(
			"%s: Failed to read dll_module stuct for 0x%x.\n",
			__func__, module_dsp_addr);
			break;
		}

		
		module_dsp_addr = module_struct->next_module;
		module_size = module_struct->next_module_size;

		pr_debug("%s: next module 0x%x %d, this module num sects %d\n",
			__func__, module_dsp_addr, module_size,
			module_struct->num_sects);

		sect_str = (char *) &module_struct->
					sects[module_struct->num_sects];
		pr_err("%s\n", sect_str);

		sect_str += strlen(sect_str) + 1;

		
		for (sect_ndx = 0;
			sect_ndx < module_struct->num_sects; sect_ndx++) {
			pr_err("    Section: 0x%x ",
				module_struct->sects[sect_ndx].sect_load_adr);

			if (((u32) sect_str - (u32) module_struct) <
				module_struct_size) {
				pr_err("%s\n", sect_str);
				
				sect_str += strlen(sect_str)+1;
			} else {
				pr_err("<string error>\n");
				pr_debug("%s: section name sting address "
					"is invalid %p\n", __func__, sect_str);
			}
		}
	}
func_end:
	kfree(module_struct);
}
#endif
