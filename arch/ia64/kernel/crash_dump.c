
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/crash_dump.h>

#include <asm/page.h>
#include <asm/uaccess.h>

ssize_t
copy_oldmem_page(unsigned long pfn, char *buf,
		size_t csize, unsigned long offset, int userbuf)
{
	void  *vaddr;

	if (!csize)
		return 0;
	vaddr = __va(pfn<<PAGE_SHIFT);
	if (userbuf) {
		if (copy_to_user(buf, (vaddr + offset), csize)) {
			return -EFAULT;
		}
	} else
		memcpy(buf, (vaddr + offset), csize);
	return csize;
}

