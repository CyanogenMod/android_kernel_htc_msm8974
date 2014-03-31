/*
 * arch/arm/kernel/crash_dump.c
 *
 * Copyright (C) 2010 Nokia Corporation.
 * Author: Mika Westerberg
 *
 * This code is taken from arch/x86/kernel/crash_dump_64.c
 *   Created by: Hariprasad Nellitheertha (hari@in.ibm.com)
 *   Copyright (C) IBM Corporation, 2004. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/crash_dump.h>
#include <linux/uaccess.h>
#include <linux/io.h>

ssize_t copy_oldmem_page(unsigned long pfn, char *buf,
			 size_t csize, unsigned long offset,
			 int userbuf)
{
	void *vaddr;

	if (!csize)
		return 0;

	vaddr = ioremap(pfn << PAGE_SHIFT, PAGE_SIZE);
	if (!vaddr)
		return -ENOMEM;

	if (userbuf) {
		if (copy_to_user(buf, vaddr + offset, csize)) {
			iounmap(vaddr);
			return -EFAULT;
		}
	} else {
		memcpy(buf, vaddr + offset, csize);
	}

	iounmap(vaddr);
	return csize;
}
