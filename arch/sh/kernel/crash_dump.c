/*
 *	crash_dump.c - Memory preserving reboot related code.
 *
 *	Created by: Hariprasad Nellitheertha (hari@in.ibm.com)
 *	Copyright (C) IBM Corporation, 2004. All rights reserved
 */
#include <linux/errno.h>
#include <linux/crash_dump.h>
#include <linux/io.h>
#include <asm/uaccess.h>

ssize_t copy_oldmem_page(unsigned long pfn, char *buf,
                               size_t csize, unsigned long offset, int userbuf)
{
	void  *vaddr;

	if (!csize)
		return 0;

	vaddr = ioremap(pfn << PAGE_SHIFT, PAGE_SIZE);

	if (userbuf) {
		if (copy_to_user(buf, (vaddr + offset), csize)) {
			iounmap(vaddr);
			return -EFAULT;
		}
	} else
	memcpy(buf, (vaddr + offset), csize);

	iounmap(vaddr);
	return csize;
}
