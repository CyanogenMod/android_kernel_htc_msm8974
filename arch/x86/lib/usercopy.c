
#include <linux/highmem.h>
#include <linux/module.h>

#include <asm/word-at-a-time.h>

unsigned long
copy_from_user_nmi(void *to, const void __user *from, unsigned long n)
{
	unsigned long offset, addr = (unsigned long)from;
	unsigned long size, len = 0;
	struct page *page;
	void *map;
	int ret;

	do {
		ret = __get_user_pages_fast(addr, 1, 0, &page);
		if (!ret)
			break;

		offset = addr & (PAGE_SIZE - 1);
		size = min(PAGE_SIZE - offset, n - len);

		map = kmap_atomic(page);
		memcpy(to, map+offset, size);
		kunmap_atomic(map);
		put_page(page);

		len  += size;
		to   += size;
		addr += size;

	} while (len < n);

	return len;
}
EXPORT_SYMBOL_GPL(copy_from_user_nmi);

static inline unsigned long count_bytes(unsigned long mask)
{
	mask = (mask - 1) & ~mask;
	mask >>= 7;
	return count_masked_bytes(mask);
}

static inline long do_strncpy_from_user(char *dst, const char __user *src, long count, unsigned long max)
{
	long res = 0;

	if (max > count)
		max = count;

	while (max >= sizeof(unsigned long)) {
		unsigned long c;

		
		if (unlikely(__get_user(c,(unsigned long __user *)(src+res))))
			break;
		
		*(unsigned long *)(dst+res) = c;
		c = has_zero(c);
		if (c)
			return res + count_bytes(c);
		res += sizeof(unsigned long);
		max -= sizeof(unsigned long);
	}

	while (max) {
		char c;

		if (unlikely(__get_user(c,src+res)))
			return -EFAULT;
		dst[res] = c;
		if (!c)
			return res;
		res++;
		max--;
	}

	if (res >= count)
		return res;

	return -EFAULT;
}

long
strncpy_from_user(char *dst, const char __user *src, long count)
{
	unsigned long max_addr, src_addr;

	if (unlikely(count <= 0))
		return 0;

	max_addr = current_thread_info()->addr_limit.seg;
	src_addr = (unsigned long)src;
	if (likely(src_addr < max_addr)) {
		unsigned long max = max_addr - src_addr;
		return do_strncpy_from_user(dst, src, count, max);
	}
	return -EFAULT;
}
EXPORT_SYMBOL(strncpy_from_user);
