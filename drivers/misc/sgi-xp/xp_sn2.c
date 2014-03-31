/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2008 Silicon Graphics, Inc.  All Rights Reserved.
 */


#include <linux/module.h>
#include <linux/device.h>
#include <asm/sn/bte.h>
#include <asm/sn/sn_sal.h>
#include "xp.h"

EXPORT_SYMBOL_GPL(xp_nofault_PIOR);

u64 xp_nofault_PIOR_target;
EXPORT_SYMBOL_GPL(xp_nofault_PIOR_target);

static enum xp_retval
xp_register_nofault_code_sn2(void)
{
	int ret;
	u64 func_addr;
	u64 err_func_addr;

	func_addr = *(u64 *)xp_nofault_PIOR;
	err_func_addr = *(u64 *)xp_error_PIOR;
	ret = sn_register_nofault_code(func_addr, err_func_addr, err_func_addr,
				       1, 1);
	if (ret != 0) {
		dev_err(xp, "can't register nofault code, error=%d\n", ret);
		return xpSalError;
	}
	if (is_shub1())
		xp_nofault_PIOR_target = SH1_IPI_ACCESS;
	else if (is_shub2())
		xp_nofault_PIOR_target = SH2_IPI_ACCESS0;

	return xpSuccess;
}

static void
xp_unregister_nofault_code_sn2(void)
{
	u64 func_addr = *(u64 *)xp_nofault_PIOR;
	u64 err_func_addr = *(u64 *)xp_error_PIOR;

	
	(void)sn_register_nofault_code(func_addr, err_func_addr,
				       err_func_addr, 1, 0);
}

static unsigned long
xp_pa_sn2(void *addr)
{
	return __pa(addr);
}

static unsigned long
xp_socket_pa_sn2(unsigned long gpa)
{
	return gpa;
}

static enum xp_retval
xp_remote_memcpy_sn2(unsigned long dst_pa, const unsigned long src_pa,
		     size_t len)
{
	bte_result_t ret;

	ret = bte_copy(src_pa, dst_pa, len, (BTE_NOTIFY | BTE_WACQUIRE), NULL);
	if (ret == BTE_SUCCESS)
		return xpSuccess;

	if (is_shub2()) {
		dev_err(xp, "bte_copy() on shub2 failed, error=0x%x dst_pa="
			"0x%016lx src_pa=0x%016lx len=%ld\\n", ret, dst_pa,
			src_pa, len);
	} else {
		dev_err(xp, "bte_copy() failed, error=%d dst_pa=0x%016lx "
			"src_pa=0x%016lx len=%ld\\n", ret, dst_pa, src_pa, len);
	}

	return xpBteCopyError;
}

static int
xp_cpu_to_nasid_sn2(int cpuid)
{
	return cpuid_to_nasid(cpuid);
}

static enum xp_retval
xp_expand_memprotect_sn2(unsigned long phys_addr, unsigned long size)
{
	u64 nasid_array = 0;
	int ret;

	ret = sn_change_memprotect(phys_addr, size, SN_MEMPROT_ACCESS_CLASS_1,
				   &nasid_array);
	if (ret != 0) {
		dev_err(xp, "sn_change_memprotect(,, "
			"SN_MEMPROT_ACCESS_CLASS_1,) failed ret=%d\n", ret);
		return xpSalError;
	}
	return xpSuccess;
}

static enum xp_retval
xp_restrict_memprotect_sn2(unsigned long phys_addr, unsigned long size)
{
	u64 nasid_array = 0;
	int ret;

	ret = sn_change_memprotect(phys_addr, size, SN_MEMPROT_ACCESS_CLASS_0,
				   &nasid_array);
	if (ret != 0) {
		dev_err(xp, "sn_change_memprotect(,, "
			"SN_MEMPROT_ACCESS_CLASS_0,) failed ret=%d\n", ret);
		return xpSalError;
	}
	return xpSuccess;
}

enum xp_retval
xp_init_sn2(void)
{
	BUG_ON(!is_shub());

	xp_max_npartitions = XP_MAX_NPARTITIONS_SN2;
	xp_partition_id = sn_partition_id;
	xp_region_size = sn_region_size;

	xp_pa = xp_pa_sn2;
	xp_socket_pa = xp_socket_pa_sn2;
	xp_remote_memcpy = xp_remote_memcpy_sn2;
	xp_cpu_to_nasid = xp_cpu_to_nasid_sn2;
	xp_expand_memprotect = xp_expand_memprotect_sn2;
	xp_restrict_memprotect = xp_restrict_memprotect_sn2;

	return xp_register_nofault_code_sn2();
}

void
xp_exit_sn2(void)
{
	BUG_ON(!is_shub());

	xp_unregister_nofault_code_sn2();
}

