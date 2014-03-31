/*
 * kexec.c - kexec system call
 * Copyright (C) 2002-2004 Eric Biederman  <ebiederm@xmission.com>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#include <linux/capability.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kexec.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/highmem.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/ioport.h>
#include <linux/hardirq.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <generated/utsrelease.h>
#include <linux/utsname.h>
#include <linux/numa.h>
#include <linux/suspend.h>
#include <linux/device.h>
#include <linux/freezer.h>
#include <linux/pm.h>
#include <linux/cpu.h>
#include <linux/console.h>
#include <linux/vmalloc.h>
#include <linux/swap.h>
#include <linux/syscore_ops.h>

#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/sections.h>

note_buf_t __percpu *crash_notes;

static unsigned char vmcoreinfo_data[VMCOREINFO_BYTES];
u32 vmcoreinfo_note[VMCOREINFO_NOTE_SIZE/4];
size_t vmcoreinfo_size;
size_t vmcoreinfo_max_size = sizeof(vmcoreinfo_data);

struct resource crashk_res = {
	.name  = "Crash kernel",
	.start = 0,
	.end   = 0,
	.flags = IORESOURCE_BUSY | IORESOURCE_MEM
};

int kexec_should_crash(struct task_struct *p)
{
	if (in_interrupt() || !p->pid || is_global_init(p) || panic_on_oops)
		return 1;
	return 0;
}


#define KIMAGE_NO_DEST (-1UL)

static int kimage_is_destination_range(struct kimage *image,
				       unsigned long start, unsigned long end);
static struct page *kimage_alloc_page(struct kimage *image,
				       gfp_t gfp_mask,
				       unsigned long dest);

static int do_kimage_alloc(struct kimage **rimage, unsigned long entry,
	                    unsigned long nr_segments,
                            struct kexec_segment __user *segments)
{
	size_t segment_bytes;
	struct kimage *image;
	unsigned long i;
	int result;

	
	result = -ENOMEM;
	image = kzalloc(sizeof(*image), GFP_KERNEL);
	if (!image)
		goto out;

	image->head = 0;
	image->entry = &image->head;
	image->last_entry = &image->head;
	image->control_page = ~0; 
	image->start = entry;
	image->type = KEXEC_TYPE_DEFAULT;

	
	INIT_LIST_HEAD(&image->control_pages);

	
	INIT_LIST_HEAD(&image->dest_pages);

	
	INIT_LIST_HEAD(&image->unuseable_pages);

	
	image->nr_segments = nr_segments;
	segment_bytes = nr_segments * sizeof(*segments);
	result = copy_from_user(image->segment, segments, segment_bytes);
	if (result) {
		result = -EFAULT;
		goto out;
	}

	result = -EADDRNOTAVAIL;
	for (i = 0; i < nr_segments; i++) {
		unsigned long mstart, mend;

		mstart = image->segment[i].mem;
		mend   = mstart + image->segment[i].memsz;
		if ((mstart & ~PAGE_MASK) || (mend & ~PAGE_MASK))
			goto out;
		if (mend >= KEXEC_DESTINATION_MEMORY_LIMIT)
			goto out;
	}

	result = -EINVAL;
	for (i = 0; i < nr_segments; i++) {
		unsigned long mstart, mend;
		unsigned long j;

		mstart = image->segment[i].mem;
		mend   = mstart + image->segment[i].memsz;
		for (j = 0; j < i; j++) {
			unsigned long pstart, pend;
			pstart = image->segment[j].mem;
			pend   = pstart + image->segment[j].memsz;
			
			if ((mend > pstart) && (mstart < pend))
				goto out;
		}
	}

	result = -EINVAL;
	for (i = 0; i < nr_segments; i++) {
		if (image->segment[i].bufsz > image->segment[i].memsz)
			goto out;
	}

	result = 0;
out:
	if (result == 0)
		*rimage = image;
	else
		kfree(image);

	return result;

}

static int kimage_normal_alloc(struct kimage **rimage, unsigned long entry,
				unsigned long nr_segments,
				struct kexec_segment __user *segments)
{
	int result;
	struct kimage *image;

	
	image = NULL;
	result = do_kimage_alloc(&image, entry, nr_segments, segments);
	if (result)
		goto out;

	*rimage = image;

	result = -ENOMEM;
	image->control_code_page = kimage_alloc_control_pages(image,
					   get_order(KEXEC_CONTROL_PAGE_SIZE));
	if (!image->control_code_page) {
		printk(KERN_ERR "Could not allocate control_code_buffer\n");
		goto out;
	}

	image->swap_page = kimage_alloc_control_pages(image, 0);
	if (!image->swap_page) {
		printk(KERN_ERR "Could not allocate swap buffer\n");
		goto out;
	}

	result = 0;
 out:
	if (result == 0)
		*rimage = image;
	else
		kfree(image);

	return result;
}

static int kimage_crash_alloc(struct kimage **rimage, unsigned long entry,
				unsigned long nr_segments,
				struct kexec_segment __user *segments)
{
	int result;
	struct kimage *image;
	unsigned long i;

	image = NULL;
	
	if ((entry < crashk_res.start) || (entry > crashk_res.end)) {
		result = -EADDRNOTAVAIL;
		goto out;
	}

	
	result = do_kimage_alloc(&image, entry, nr_segments, segments);
	if (result)
		goto out;

	image->control_page = crashk_res.start;
	image->type = KEXEC_TYPE_CRASH;

	result = -EADDRNOTAVAIL;
	for (i = 0; i < nr_segments; i++) {
		unsigned long mstart, mend;

		mstart = image->segment[i].mem;
		mend = mstart + image->segment[i].memsz - 1;
		
		if ((mstart < crashk_res.start) || (mend > crashk_res.end))
			goto out;
	}

	result = -ENOMEM;
	image->control_code_page = kimage_alloc_control_pages(image,
					   get_order(KEXEC_CONTROL_PAGE_SIZE));
	if (!image->control_code_page) {
		printk(KERN_ERR "Could not allocate control_code_buffer\n");
		goto out;
	}

	result = 0;
out:
	if (result == 0)
		*rimage = image;
	else
		kfree(image);

	return result;
}

static int kimage_is_destination_range(struct kimage *image,
					unsigned long start,
					unsigned long end)
{
	unsigned long i;

	for (i = 0; i < image->nr_segments; i++) {
		unsigned long mstart, mend;

		mstart = image->segment[i].mem;
		mend = mstart + image->segment[i].memsz;
		if ((end > mstart) && (start < mend))
			return 1;
	}

	return 0;
}

static struct page *kimage_alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *pages;

	pages = alloc_pages(gfp_mask, order);
	if (pages) {
		unsigned int count, i;
		pages->mapping = NULL;
		set_page_private(pages, order);
		count = 1 << order;
		for (i = 0; i < count; i++)
			SetPageReserved(pages + i);
	}

	return pages;
}

static void kimage_free_pages(struct page *page)
{
	unsigned int order, count, i;

	order = page_private(page);
	count = 1 << order;
	for (i = 0; i < count; i++)
		ClearPageReserved(page + i);
	__free_pages(page, order);
}

static void kimage_free_page_list(struct list_head *list)
{
	struct list_head *pos, *next;

	list_for_each_safe(pos, next, list) {
		struct page *page;

		page = list_entry(pos, struct page, lru);
		list_del(&page->lru);
		kimage_free_pages(page);
	}
}

static struct page *kimage_alloc_normal_control_pages(struct kimage *image,
							unsigned int order)
{
	struct list_head extra_pages;
	struct page *pages;
	unsigned int count;

	count = 1 << order;
	INIT_LIST_HEAD(&extra_pages);

	do {
		unsigned long pfn, epfn, addr, eaddr;

		pages = kimage_alloc_pages(GFP_KERNEL, order);
		if (!pages)
			break;
		pfn   = page_to_pfn(pages);
		epfn  = pfn + count;
		addr  = pfn << PAGE_SHIFT;
		eaddr = epfn << PAGE_SHIFT;
		if ((epfn >= (KEXEC_CONTROL_MEMORY_LIMIT >> PAGE_SHIFT)) ||
			      kimage_is_destination_range(image, addr, eaddr)) {
			list_add(&pages->lru, &extra_pages);
			pages = NULL;
		}
	} while (!pages);

	if (pages) {
		
		list_add(&pages->lru, &image->control_pages);

	}
	kimage_free_page_list(&extra_pages);

	return pages;
}

static struct page *kimage_alloc_crash_control_pages(struct kimage *image,
						      unsigned int order)
{
	unsigned long hole_start, hole_end, size;
	struct page *pages;

	pages = NULL;
	size = (1 << order) << PAGE_SHIFT;
	hole_start = (image->control_page + (size - 1)) & ~(size - 1);
	hole_end   = hole_start + size - 1;
	while (hole_end <= crashk_res.end) {
		unsigned long i;

		if (hole_end > KEXEC_CRASH_CONTROL_MEMORY_LIMIT)
			break;
		if (hole_end > crashk_res.end)
			break;
		
		for (i = 0; i < image->nr_segments; i++) {
			unsigned long mstart, mend;

			mstart = image->segment[i].mem;
			mend   = mstart + image->segment[i].memsz - 1;
			if ((hole_end >= mstart) && (hole_start <= mend)) {
				
				hole_start = (mend + (size - 1)) & ~(size - 1);
				hole_end   = hole_start + size - 1;
				break;
			}
		}
		
		if (i == image->nr_segments) {
			pages = pfn_to_page(hole_start >> PAGE_SHIFT);
			break;
		}
	}
	if (pages)
		image->control_page = hole_end;

	return pages;
}


struct page *kimage_alloc_control_pages(struct kimage *image,
					 unsigned int order)
{
	struct page *pages = NULL;

	switch (image->type) {
	case KEXEC_TYPE_DEFAULT:
		pages = kimage_alloc_normal_control_pages(image, order);
		break;
	case KEXEC_TYPE_CRASH:
		pages = kimage_alloc_crash_control_pages(image, order);
		break;
	}

	return pages;
}

static int kimage_add_entry(struct kimage *image, kimage_entry_t entry)
{
	if (*image->entry != 0)
		image->entry++;

	if (image->entry == image->last_entry) {
		kimage_entry_t *ind_page;
		struct page *page;

		page = kimage_alloc_page(image, GFP_KERNEL, KIMAGE_NO_DEST);
		if (!page)
			return -ENOMEM;

		ind_page = page_address(page);
		*image->entry = virt_to_phys(ind_page) | IND_INDIRECTION;
		image->entry = ind_page;
		image->last_entry = ind_page +
				      ((PAGE_SIZE/sizeof(kimage_entry_t)) - 1);
	}
	*image->entry = entry;
	image->entry++;
	*image->entry = 0;

	return 0;
}

static int kimage_set_destination(struct kimage *image,
				   unsigned long destination)
{
	int result;

	destination &= PAGE_MASK;
	result = kimage_add_entry(image, destination | IND_DESTINATION);
	if (result == 0)
		image->destination = destination;

	return result;
}


static int kimage_add_page(struct kimage *image, unsigned long page)
{
	int result;

	page &= PAGE_MASK;
	result = kimage_add_entry(image, page | IND_SOURCE);
	if (result == 0)
		image->destination += PAGE_SIZE;

	return result;
}


static void kimage_free_extra_pages(struct kimage *image)
{
	
	kimage_free_page_list(&image->dest_pages);

	
	kimage_free_page_list(&image->unuseable_pages);

}
static void kimage_terminate(struct kimage *image)
{
	if (*image->entry != 0)
		image->entry++;

	*image->entry = IND_DONE;
}

#define for_each_kimage_entry(image, ptr, entry) \
	for (ptr = &image->head; (entry = *ptr) && !(entry & IND_DONE); \
		ptr = (entry & IND_INDIRECTION)? \
			phys_to_virt((entry & PAGE_MASK)): ptr +1)

static void kimage_free_entry(kimage_entry_t entry)
{
	struct page *page;

	page = pfn_to_page(entry >> PAGE_SHIFT);
	kimage_free_pages(page);
}

static void kimage_free(struct kimage *image)
{
	kimage_entry_t *ptr, entry;
	kimage_entry_t ind = 0;

	if (!image)
		return;

	kimage_free_extra_pages(image);
	for_each_kimage_entry(image, ptr, entry) {
		if (entry & IND_INDIRECTION) {
			
			if (ind & IND_INDIRECTION)
				kimage_free_entry(ind);
			ind = entry;
		}
		else if (entry & IND_SOURCE)
			kimage_free_entry(entry);
	}
	
	if (ind & IND_INDIRECTION)
		kimage_free_entry(ind);

	
	machine_kexec_cleanup(image);

	
	kimage_free_page_list(&image->control_pages);
	kfree(image);
}

static kimage_entry_t *kimage_dst_used(struct kimage *image,
					unsigned long page)
{
	kimage_entry_t *ptr, entry;
	unsigned long destination = 0;

	for_each_kimage_entry(image, ptr, entry) {
		if (entry & IND_DESTINATION)
			destination = entry & PAGE_MASK;
		else if (entry & IND_SOURCE) {
			if (page == destination)
				return ptr;
			destination += PAGE_SIZE;
		}
	}

	return NULL;
}

static struct page *kimage_alloc_page(struct kimage *image,
					gfp_t gfp_mask,
					unsigned long destination)
{
	struct page *page;
	unsigned long addr;

	list_for_each_entry(page, &image->dest_pages, lru) {
		addr = page_to_pfn(page) << PAGE_SHIFT;
		if (addr == destination) {
			list_del(&page->lru);
			return page;
		}
	}
	page = NULL;
	while (1) {
		kimage_entry_t *old;

		
		page = kimage_alloc_pages(gfp_mask, 0);
		if (!page)
			return NULL;
		
		if (page_to_pfn(page) >
				(KEXEC_SOURCE_MEMORY_LIMIT >> PAGE_SHIFT)) {
			list_add(&page->lru, &image->unuseable_pages);
			continue;
		}
		addr = page_to_pfn(page) << PAGE_SHIFT;

		
		if (addr == destination)
			break;

		
		if (!kimage_is_destination_range(image, addr,
						  addr + PAGE_SIZE))
			break;

		old = kimage_dst_used(image, addr);
		if (old) {
			
			unsigned long old_addr;
			struct page *old_page;

			old_addr = *old & PAGE_MASK;
			old_page = pfn_to_page(old_addr >> PAGE_SHIFT);
			copy_highpage(page, old_page);
			*old = addr | (*old & ~PAGE_MASK);

			if (!(gfp_mask & __GFP_HIGHMEM) &&
			    PageHighMem(old_page)) {
				kimage_free_pages(old_page);
				continue;
			}
			addr = old_addr;
			page = old_page;
			break;
		}
		else {
			list_add(&page->lru, &image->dest_pages);
		}
	}

	return page;
}

static int kimage_load_normal_segment(struct kimage *image,
					 struct kexec_segment *segment)
{
	unsigned long maddr;
	unsigned long ubytes, mbytes;
	int result;
	unsigned char __user *buf;

	result = 0;
	buf = segment->buf;
	ubytes = segment->bufsz;
	mbytes = segment->memsz;
	maddr = segment->mem;

	result = kimage_set_destination(image, maddr);
	if (result < 0)
		goto out;

	while (mbytes) {
		struct page *page;
		char *ptr;
		size_t uchunk, mchunk;

		page = kimage_alloc_page(image, GFP_HIGHUSER, maddr);
		if (!page) {
			result  = -ENOMEM;
			goto out;
		}
		result = kimage_add_page(image, page_to_pfn(page)
								<< PAGE_SHIFT);
		if (result < 0)
			goto out;

		ptr = kmap(page);
		
		clear_page(ptr);
		ptr += maddr & ~PAGE_MASK;
		mchunk = PAGE_SIZE - (maddr & ~PAGE_MASK);
		if (mchunk > mbytes)
			mchunk = mbytes;

		uchunk = mchunk;
		if (uchunk > ubytes)
			uchunk = ubytes;

		result = copy_from_user(ptr, buf, uchunk);
		kunmap(page);
		if (result) {
			result = -EFAULT;
			goto out;
		}
		ubytes -= uchunk;
		maddr  += mchunk;
		buf    += mchunk;
		mbytes -= mchunk;
	}
out:
	return result;
}

static int kimage_load_crash_segment(struct kimage *image,
					struct kexec_segment *segment)
{
	unsigned long maddr;
	unsigned long ubytes, mbytes;
	int result;
	unsigned char __user *buf;

	result = 0;
	buf = segment->buf;
	ubytes = segment->bufsz;
	mbytes = segment->memsz;
	maddr = segment->mem;
	while (mbytes) {
		struct page *page;
		char *ptr;
		size_t uchunk, mchunk;

		page = pfn_to_page(maddr >> PAGE_SHIFT);
		if (!page) {
			result  = -ENOMEM;
			goto out;
		}
		ptr = kmap(page);
		ptr += maddr & ~PAGE_MASK;
		mchunk = PAGE_SIZE - (maddr & ~PAGE_MASK);
		if (mchunk > mbytes)
			mchunk = mbytes;

		uchunk = mchunk;
		if (uchunk > ubytes) {
			uchunk = ubytes;
			
			memset(ptr + uchunk, 0, mchunk - uchunk);
		}
		result = copy_from_user(ptr, buf, uchunk);
		kexec_flush_icache_page(page);
		kunmap(page);
		if (result) {
			result = -EFAULT;
			goto out;
		}
		ubytes -= uchunk;
		maddr  += mchunk;
		buf    += mchunk;
		mbytes -= mchunk;
	}
out:
	return result;
}

static int kimage_load_segment(struct kimage *image,
				struct kexec_segment *segment)
{
	int result = -ENOMEM;

	switch (image->type) {
	case KEXEC_TYPE_DEFAULT:
		result = kimage_load_normal_segment(image, segment);
		break;
	case KEXEC_TYPE_CRASH:
		result = kimage_load_crash_segment(image, segment);
		break;
	}

	return result;
}

struct kimage *kexec_image;
struct kimage *kexec_crash_image;

static DEFINE_MUTEX(kexec_mutex);

SYSCALL_DEFINE4(kexec_load, unsigned long, entry, unsigned long, nr_segments,
		struct kexec_segment __user *, segments, unsigned long, flags)
{
	struct kimage **dest_image, *image;
	int result;

	
	if (!capable(CAP_SYS_BOOT))
		return -EPERM;

	if ((flags & KEXEC_FLAGS) != (flags & ~KEXEC_ARCH_MASK))
		return -EINVAL;

	
	if (((flags & KEXEC_ARCH_MASK) != KEXEC_ARCH) &&
		((flags & KEXEC_ARCH_MASK) != KEXEC_ARCH_DEFAULT))
		return -EINVAL;

	if (nr_segments > KEXEC_SEGMENT_MAX)
		return -EINVAL;

	image = NULL;
	result = 0;

	if (!mutex_trylock(&kexec_mutex))
		return -EBUSY;

	dest_image = &kexec_image;
	if (flags & KEXEC_ON_CRASH)
		dest_image = &kexec_crash_image;
	if (nr_segments > 0) {
		unsigned long i;

		
		if ((flags & KEXEC_ON_CRASH) == 0)
			result = kimage_normal_alloc(&image, entry,
							nr_segments, segments);
		
		else if (flags & KEXEC_ON_CRASH) {
			kimage_free(xchg(&kexec_crash_image, NULL));
			result = kimage_crash_alloc(&image, entry,
						     nr_segments, segments);
			crash_map_reserved_pages();
		}
		if (result)
			goto out;

		if (flags & KEXEC_PRESERVE_CONTEXT)
			image->preserve_context = 1;
		result = machine_kexec_prepare(image);
		if (result)
			goto out;

		for (i = 0; i < nr_segments; i++) {
			result = kimage_load_segment(image, &image->segment[i]);
			if (result)
				goto out;
		}
		kimage_terminate(image);
		if (flags & KEXEC_ON_CRASH)
			crash_unmap_reserved_pages();
	}
	
	image = xchg(dest_image, image);

out:
	mutex_unlock(&kexec_mutex);
	kimage_free(image);

	return result;
}

void __weak crash_map_reserved_pages(void)
{}

void __weak crash_unmap_reserved_pages(void)
{}

#ifdef CONFIG_COMPAT
asmlinkage long compat_sys_kexec_load(unsigned long entry,
				unsigned long nr_segments,
				struct compat_kexec_segment __user *segments,
				unsigned long flags)
{
	struct compat_kexec_segment in;
	struct kexec_segment out, __user *ksegments;
	unsigned long i, result;

	if ((flags & KEXEC_ARCH_MASK) == KEXEC_ARCH_DEFAULT)
		return -EINVAL;

	if (nr_segments > KEXEC_SEGMENT_MAX)
		return -EINVAL;

	ksegments = compat_alloc_user_space(nr_segments * sizeof(out));
	for (i=0; i < nr_segments; i++) {
		result = copy_from_user(&in, &segments[i], sizeof(in));
		if (result)
			return -EFAULT;

		out.buf   = compat_ptr(in.buf);
		out.bufsz = in.bufsz;
		out.mem   = in.mem;
		out.memsz = in.memsz;

		result = copy_to_user(&ksegments[i], &out, sizeof(out));
		if (result)
			return -EFAULT;
	}

	return sys_kexec_load(entry, nr_segments, ksegments, flags);
}
#endif

void crash_kexec(struct pt_regs *regs)
{
	if (mutex_trylock(&kexec_mutex)) {
		if (kexec_crash_image) {
			struct pt_regs fixed_regs;

			crash_setup_regs(&fixed_regs, regs);
			crash_save_vmcoreinfo();
			machine_crash_shutdown(&fixed_regs);
			machine_kexec(kexec_crash_image);
		}
		mutex_unlock(&kexec_mutex);
	}
}

size_t crash_get_memory_size(void)
{
	size_t size = 0;
	mutex_lock(&kexec_mutex);
	if (crashk_res.end != crashk_res.start)
		size = resource_size(&crashk_res);
	mutex_unlock(&kexec_mutex);
	return size;
}

void __weak crash_free_reserved_phys_range(unsigned long begin,
					   unsigned long end)
{
	unsigned long addr;

	for (addr = begin; addr < end; addr += PAGE_SIZE) {
		ClearPageReserved(pfn_to_page(addr >> PAGE_SHIFT));
		init_page_count(pfn_to_page(addr >> PAGE_SHIFT));
		free_page((unsigned long)__va(addr));
		totalram_pages++;
	}
}

int crash_shrink_memory(unsigned long new_size)
{
	int ret = 0;
	unsigned long start, end;
	unsigned long old_size;
	struct resource *ram_res;

	mutex_lock(&kexec_mutex);

	if (kexec_crash_image) {
		ret = -ENOENT;
		goto unlock;
	}
	start = crashk_res.start;
	end = crashk_res.end;
	old_size = (end == 0) ? 0 : end - start + 1;
	if (new_size >= old_size) {
		ret = (new_size == old_size) ? 0 : -EINVAL;
		goto unlock;
	}

	ram_res = kzalloc(sizeof(*ram_res), GFP_KERNEL);
	if (!ram_res) {
		ret = -ENOMEM;
		goto unlock;
	}

	start = roundup(start, KEXEC_CRASH_MEM_ALIGN);
	end = roundup(start + new_size, KEXEC_CRASH_MEM_ALIGN);

	crash_map_reserved_pages();
	crash_free_reserved_phys_range(end, crashk_res.end);

	if ((start == end) && (crashk_res.parent != NULL))
		release_resource(&crashk_res);

	ram_res->start = end;
	ram_res->end = crashk_res.end;
	ram_res->flags = IORESOURCE_BUSY | IORESOURCE_MEM;
	ram_res->name = "System RAM";

	crashk_res.end = end - 1;

	insert_resource(&iomem_resource, ram_res);
	crash_unmap_reserved_pages();

unlock:
	mutex_unlock(&kexec_mutex);
	return ret;
}

static u32 *append_elf_note(u32 *buf, char *name, unsigned type, void *data,
			    size_t data_len)
{
	struct elf_note note;

	note.n_namesz = strlen(name) + 1;
	note.n_descsz = data_len;
	note.n_type   = type;
	memcpy(buf, &note, sizeof(note));
	buf += (sizeof(note) + 3)/4;
	memcpy(buf, name, note.n_namesz);
	buf += (note.n_namesz + 3)/4;
	memcpy(buf, data, note.n_descsz);
	buf += (note.n_descsz + 3)/4;

	return buf;
}

static void final_note(u32 *buf)
{
	struct elf_note note;

	note.n_namesz = 0;
	note.n_descsz = 0;
	note.n_type   = 0;
	memcpy(buf, &note, sizeof(note));
}

void crash_save_cpu(struct pt_regs *regs, int cpu)
{
	struct elf_prstatus prstatus;
	u32 *buf;

	if ((cpu < 0) || (cpu >= nr_cpu_ids))
		return;

	buf = (u32*)per_cpu_ptr(crash_notes, cpu);
	if (!buf)
		return;
	memset(&prstatus, 0, sizeof(prstatus));
	prstatus.pr_pid = current->pid;
	elf_core_copy_kernel_regs(&prstatus.pr_reg, regs);
	buf = append_elf_note(buf, KEXEC_CORE_NOTE_NAME, NT_PRSTATUS,
		      	      &prstatus, sizeof(prstatus));
	final_note(buf);
}

static int __init crash_notes_memory_init(void)
{
	
	crash_notes = alloc_percpu(note_buf_t);
	if (!crash_notes) {
		printk("Kexec: Memory allocation for saving cpu register"
		" states failed\n");
		return -ENOMEM;
	}
	return 0;
}
module_init(crash_notes_memory_init)




static int __init parse_crashkernel_mem(char 			*cmdline,
					unsigned long long	system_ram,
					unsigned long long	*crash_size,
					unsigned long long	*crash_base)
{
	char *cur = cmdline, *tmp;

	
	do {
		unsigned long long start, end = ULLONG_MAX, size;

		
		start = memparse(cur, &tmp);
		if (cur == tmp) {
			pr_warning("crashkernel: Memory value expected\n");
			return -EINVAL;
		}
		cur = tmp;
		if (*cur != '-') {
			pr_warning("crashkernel: '-' expected\n");
			return -EINVAL;
		}
		cur++;

		
		if (*cur != ':') {
			end = memparse(cur, &tmp);
			if (cur == tmp) {
				pr_warning("crashkernel: Memory "
						"value expected\n");
				return -EINVAL;
			}
			cur = tmp;
			if (end <= start) {
				pr_warning("crashkernel: end <= start\n");
				return -EINVAL;
			}
		}

		if (*cur != ':') {
			pr_warning("crashkernel: ':' expected\n");
			return -EINVAL;
		}
		cur++;

		size = memparse(cur, &tmp);
		if (cur == tmp) {
			pr_warning("Memory value expected\n");
			return -EINVAL;
		}
		cur = tmp;
		if (size >= system_ram) {
			pr_warning("crashkernel: invalid size\n");
			return -EINVAL;
		}

		
		if (system_ram >= start && system_ram < end) {
			*crash_size = size;
			break;
		}
	} while (*cur++ == ',');

	if (*crash_size > 0) {
		while (*cur && *cur != ' ' && *cur != '@')
			cur++;
		if (*cur == '@') {
			cur++;
			*crash_base = memparse(cur, &tmp);
			if (cur == tmp) {
				pr_warning("Memory value expected "
						"after '@'\n");
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int __init parse_crashkernel_simple(char 		*cmdline,
					   unsigned long long 	*crash_size,
					   unsigned long long 	*crash_base)
{
	char *cur = cmdline;

	*crash_size = memparse(cmdline, &cur);
	if (cmdline == cur) {
		pr_warning("crashkernel: memory value expected\n");
		return -EINVAL;
	}

	if (*cur == '@')
		*crash_base = memparse(cur+1, &cur);
	else if (*cur != ' ' && *cur != '\0') {
		pr_warning("crashkernel: unrecognized char\n");
		return -EINVAL;
	}

	return 0;
}

int __init parse_crashkernel(char 		 *cmdline,
			     unsigned long long system_ram,
			     unsigned long long *crash_size,
			     unsigned long long *crash_base)
{
	char 	*p = cmdline, *ck_cmdline = NULL;
	char	*first_colon, *first_space;

	BUG_ON(!crash_size || !crash_base);
	*crash_size = 0;
	*crash_base = 0;

	
	p = strstr(p, "crashkernel=");
	while (p) {
		ck_cmdline = p;
		p = strstr(p+1, "crashkernel=");
	}

	if (!ck_cmdline)
		return -EINVAL;

	ck_cmdline += 12; 

	first_colon = strchr(ck_cmdline, ':');
	first_space = strchr(ck_cmdline, ' ');
	if (first_colon && (!first_space || first_colon < first_space))
		return parse_crashkernel_mem(ck_cmdline, system_ram,
				crash_size, crash_base);
	else
		return parse_crashkernel_simple(ck_cmdline, crash_size,
				crash_base);

	return 0;
}


static void update_vmcoreinfo_note(void)
{
	u32 *buf = vmcoreinfo_note;

	if (!vmcoreinfo_size)
		return;
	buf = append_elf_note(buf, VMCOREINFO_NOTE_NAME, 0, vmcoreinfo_data,
			      vmcoreinfo_size);
	final_note(buf);
}

void crash_save_vmcoreinfo(void)
{
	vmcoreinfo_append_str("CRASHTIME=%ld", get_seconds());
	update_vmcoreinfo_note();
}

void vmcoreinfo_append_str(const char *fmt, ...)
{
	va_list args;
	char buf[0x50];
	int r;

	va_start(args, fmt);
	r = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (r + vmcoreinfo_size > vmcoreinfo_max_size)
		r = vmcoreinfo_max_size - vmcoreinfo_size;

	memcpy(&vmcoreinfo_data[vmcoreinfo_size], buf, r);

	vmcoreinfo_size += r;
}

void __attribute__ ((weak)) arch_crash_save_vmcoreinfo(void)
{}

unsigned long __attribute__ ((weak)) paddr_vmcoreinfo_note(void)
{
	return __pa((unsigned long)(char *)&vmcoreinfo_note);
}

static int __init crash_save_vmcoreinfo_init(void)
{
	VMCOREINFO_OSRELEASE(init_uts_ns.name.release);
	VMCOREINFO_PAGESIZE(PAGE_SIZE);

	VMCOREINFO_SYMBOL(init_uts_ns);
	VMCOREINFO_SYMBOL(node_online_map);
#ifdef CONFIG_MMU
	VMCOREINFO_SYMBOL(swapper_pg_dir);
#endif
	VMCOREINFO_SYMBOL(_stext);
	VMCOREINFO_SYMBOL(vmlist);

#ifndef CONFIG_NEED_MULTIPLE_NODES
	VMCOREINFO_SYMBOL(mem_map);
	VMCOREINFO_SYMBOL(contig_page_data);
#endif
#ifdef CONFIG_SPARSEMEM
	VMCOREINFO_SYMBOL(mem_section);
	VMCOREINFO_LENGTH(mem_section, NR_SECTION_ROOTS);
	VMCOREINFO_STRUCT_SIZE(mem_section);
	VMCOREINFO_OFFSET(mem_section, section_mem_map);
#endif
	VMCOREINFO_STRUCT_SIZE(page);
	VMCOREINFO_STRUCT_SIZE(pglist_data);
	VMCOREINFO_STRUCT_SIZE(zone);
	VMCOREINFO_STRUCT_SIZE(free_area);
	VMCOREINFO_STRUCT_SIZE(list_head);
	VMCOREINFO_SIZE(nodemask_t);
	VMCOREINFO_OFFSET(page, flags);
	VMCOREINFO_OFFSET(page, _count);
	VMCOREINFO_OFFSET(page, mapping);
	VMCOREINFO_OFFSET(page, lru);
	VMCOREINFO_OFFSET(pglist_data, node_zones);
	VMCOREINFO_OFFSET(pglist_data, nr_zones);
#ifdef CONFIG_FLAT_NODE_MEM_MAP
	VMCOREINFO_OFFSET(pglist_data, node_mem_map);
#endif
	VMCOREINFO_OFFSET(pglist_data, node_start_pfn);
	VMCOREINFO_OFFSET(pglist_data, node_spanned_pages);
	VMCOREINFO_OFFSET(pglist_data, node_id);
	VMCOREINFO_OFFSET(zone, free_area);
	VMCOREINFO_OFFSET(zone, vm_stat);
	VMCOREINFO_OFFSET(zone, spanned_pages);
	VMCOREINFO_OFFSET(free_area, free_list);
	VMCOREINFO_OFFSET(list_head, next);
	VMCOREINFO_OFFSET(list_head, prev);
	VMCOREINFO_OFFSET(vm_struct, addr);
	VMCOREINFO_LENGTH(zone.free_area, MAX_ORDER);
	log_buf_kexec_setup();
	VMCOREINFO_LENGTH(free_area.free_list, MIGRATE_TYPES);
	VMCOREINFO_NUMBER(NR_FREE_PAGES);
	VMCOREINFO_NUMBER(PG_lru);
	VMCOREINFO_NUMBER(PG_private);
	VMCOREINFO_NUMBER(PG_swapcache);

	arch_crash_save_vmcoreinfo();
	update_vmcoreinfo_note();

	return 0;
}

module_init(crash_save_vmcoreinfo_init)

int kernel_kexec(void)
{
	int error = 0;

	if (!mutex_trylock(&kexec_mutex))
		return -EBUSY;
	if (!kexec_image) {
		error = -EINVAL;
		goto Unlock;
	}

#ifdef CONFIG_KEXEC_JUMP
	if (kexec_image->preserve_context) {
		lock_system_sleep();
		pm_prepare_console();
		error = freeze_processes();
		if (error) {
			error = -EBUSY;
			goto Restore_console;
		}
		suspend_console();
		error = dpm_suspend_start(PMSG_FREEZE);
		if (error)
			goto Resume_console;
		error = dpm_suspend_end(PMSG_FREEZE);
		if (error)
			goto Resume_devices;
		error = disable_nonboot_cpus();
		if (error)
			goto Enable_cpus;
		local_irq_disable();
		error = syscore_suspend();
		if (error)
			goto Enable_irqs;
	} else
#endif
	{
		kernel_restart_prepare(NULL);
		printk(KERN_EMERG "Starting new kernel\n");
		machine_shutdown();
	}

	machine_kexec(kexec_image);

#ifdef CONFIG_KEXEC_JUMP
	if (kexec_image->preserve_context) {
		syscore_resume();
 Enable_irqs:
		local_irq_enable();
 Enable_cpus:
		enable_nonboot_cpus();
		dpm_resume_start(PMSG_RESTORE);
 Resume_devices:
		dpm_resume_end(PMSG_RESTORE);
 Resume_console:
		resume_console();
		thaw_processes();
 Restore_console:
		pm_restore_console();
		unlock_system_sleep();
	}
#endif

 Unlock:
	mutex_unlock(&kexec_mutex);
	return error;
}
