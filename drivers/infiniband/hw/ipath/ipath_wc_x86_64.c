/*
 * Copyright (c) 2006, 2007 QLogic Corporation. All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc. All rights reserved.
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
#include <asm/mtrr.h>
#include <asm/processor.h>

#include "ipath_kernel.h"

int ipath_enable_wc(struct ipath_devdata *dd)
{
	int ret = 0;
	u64 pioaddr, piolen;
	unsigned bits;
	const unsigned long addr = pci_resource_start(dd->pcidev, 0);
	const size_t len = pci_resource_len(dd->pcidev, 0);

	if (dd->ipath_piobcnt2k && dd->ipath_piobcnt4k) { 
		unsigned long pio2kbase, pio4kbase;
		pio2kbase = dd->ipath_piobufbase & 0xffffffffUL;
		pio4kbase = (dd->ipath_piobufbase >> 32) & 0xffffffffUL;
		if (pio2kbase < pio4kbase) { 
			pioaddr = addr + pio2kbase;
			piolen = pio4kbase - pio2kbase +
				dd->ipath_piobcnt4k * dd->ipath_4kalign;
		} else {
			pioaddr = addr + pio4kbase;
			piolen = pio2kbase - pio4kbase +
				dd->ipath_piobcnt2k * dd->ipath_palign;
		}
	} else {  
		pioaddr = addr + dd->ipath_piobufbase;
		piolen = dd->ipath_piobcnt2k * dd->ipath_palign +
			dd->ipath_piobcnt4k * dd->ipath_4kalign;
	}

	for (bits = 0; !(piolen & (1ULL << bits)); bits++)
		 ;

	if (piolen != (1ULL << bits)) {
		piolen >>= bits;
		while (piolen >>= 1)
			bits++;
		piolen = 1ULL << (bits + 1);
	}
	if (pioaddr & (piolen - 1)) {
		u64 atmp;
		ipath_dbg("pioaddr %llx not on right boundary for size "
			  "%llx, fixing\n",
			  (unsigned long long) pioaddr,
			  (unsigned long long) piolen);
		atmp = pioaddr & ~(piolen - 1);
		if (atmp < addr || (atmp + piolen) > (addr + len)) {
			ipath_dev_err(dd, "No way to align address/size "
				      "(%llx/%llx), no WC mtrr\n",
				      (unsigned long long) atmp,
				      (unsigned long long) piolen << 1);
			ret = -ENODEV;
		} else {
			ipath_dbg("changing WC base from %llx to %llx, "
				  "len from %llx to %llx\n",
				  (unsigned long long) pioaddr,
				  (unsigned long long) atmp,
				  (unsigned long long) piolen,
				  (unsigned long long) piolen << 1);
			pioaddr = atmp;
			piolen <<= 1;
		}
	}

	if (!ret) {
		int cookie;
		ipath_cdbg(VERBOSE, "Setting mtrr for chip to WC "
			   "(addr %llx, len=0x%llx)\n",
			   (unsigned long long) pioaddr,
			   (unsigned long long) piolen);
		cookie = mtrr_add(pioaddr, piolen, MTRR_TYPE_WRCOMB, 0);
		if (cookie < 0) {
			{
				dev_info(&dd->pcidev->dev,
					 "mtrr_add()  WC for PIO bufs "
					 "failed (%d)\n",
					 cookie);
				ret = -EINVAL;
			}
		} else {
			ipath_cdbg(VERBOSE, "Set mtrr for chip to WC, "
				   "cookie is %d\n", cookie);
			dd->ipath_wc_cookie = cookie;
			dd->ipath_wc_base = (unsigned long) pioaddr;
			dd->ipath_wc_len = (unsigned long) piolen;
		}
	}

	return ret;
}

void ipath_disable_wc(struct ipath_devdata *dd)
{
	if (dd->ipath_wc_cookie) {
		int r;
		ipath_cdbg(VERBOSE, "undoing WCCOMB on pio buffers\n");
		r = mtrr_del(dd->ipath_wc_cookie, dd->ipath_wc_base,
			     dd->ipath_wc_len);
		if (r < 0)
			dev_info(&dd->pcidev->dev,
				 "mtrr_del(%lx, %lx, %lx) failed: %d\n",
				 dd->ipath_wc_cookie, dd->ipath_wc_base,
				 dd->ipath_wc_len, r);
		dd->ipath_wc_cookie = 0; 
	}
}

int ipath_unordered_wc(void)
{
	return boot_cpu_data.x86_vendor != X86_VENDOR_AMD;
}
