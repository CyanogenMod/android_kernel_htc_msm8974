/*
 * Copyright (c) 2006, 2007, 2008, 2009 QLogic Corporation. All rights reserved.
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

#include "qib.h"

int qib_enable_wc(struct qib_devdata *dd)
{
	int ret = 0;
	u64 pioaddr, piolen;
	unsigned bits;
	const unsigned long addr = pci_resource_start(dd->pcidev, 0);
	const size_t len = pci_resource_len(dd->pcidev, 0);

	if (dd->piobcnt2k && dd->piobcnt4k) {
		
		unsigned long pio2kbase, pio4kbase;
		pio2kbase = dd->piobufbase & 0xffffffffUL;
		pio4kbase = (dd->piobufbase >> 32) & 0xffffffffUL;
		if (pio2kbase < pio4kbase) {
			
			pioaddr = addr + pio2kbase;
			piolen = pio4kbase - pio2kbase +
				dd->piobcnt4k * dd->align4k;
		} else {
			pioaddr = addr + pio4kbase;
			piolen = pio2kbase - pio4kbase +
				dd->piobcnt2k * dd->palign;
		}
	} else {  
		pioaddr = addr + dd->piobufbase;
		piolen = dd->piobcnt2k * dd->palign +
			dd->piobcnt4k * dd->align4k;
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
		atmp = pioaddr & ~(piolen - 1);
		if (atmp < addr || (atmp + piolen) > (addr + len)) {
			qib_dev_err(dd, "No way to align address/size "
				    "(%llx/%llx), no WC mtrr\n",
				    (unsigned long long) atmp,
				    (unsigned long long) piolen << 1);
			ret = -ENODEV;
		} else {
			pioaddr = atmp;
			piolen <<= 1;
		}
	}

	if (!ret) {
		int cookie;

		cookie = mtrr_add(pioaddr, piolen, MTRR_TYPE_WRCOMB, 0);
		if (cookie < 0) {
			{
				qib_devinfo(dd->pcidev,
					 "mtrr_add()  WC for PIO bufs "
					 "failed (%d)\n",
					 cookie);
				ret = -EINVAL;
			}
		} else {
			dd->wc_cookie = cookie;
			dd->wc_base = (unsigned long) pioaddr;
			dd->wc_len = (unsigned long) piolen;
		}
	}

	return ret;
}

void qib_disable_wc(struct qib_devdata *dd)
{
	if (dd->wc_cookie) {
		int r;

		r = mtrr_del(dd->wc_cookie, dd->wc_base,
			     dd->wc_len);
		if (r < 0)
			qib_devinfo(dd->pcidev,
				 "mtrr_del(%lx, %lx, %lx) failed: %d\n",
				 dd->wc_cookie, dd->wc_base,
				 dd->wc_len, r);
		dd->wc_cookie = 0; 
	}
}

int qib_unordered_wc(void)
{
	return boot_cpu_data.x86_vendor != X86_VENDOR_AMD;
}
