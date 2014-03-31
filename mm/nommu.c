/*
 *  linux/mm/nommu.c
 *
 *  Replacement code for mm functions to support CPU's that don't
 *  have any form of memory management unit (thus no virtual memory).
 *
 *  See Documentation/nommu-mmap.txt
 *
 *  Copyright (c) 2004-2008 David Howells <dhowells@redhat.com>
 *  Copyright (c) 2000-2003 David McCullough <davidm@snapgear.com>
 *  Copyright (c) 2000-2001 D Jeff Dionne <jeff@uClinux.org>
 *  Copyright (c) 2002      Greg Ungerer <gerg@snapgear.com>
 *  Copyright (c) 2007-2010 Paul Mundt <lethal@linux-sh.org>
 */

#include <linux/export.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/swap.h>
#include <linux/file.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/mount.h>
#include <linux/personality.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/audit.h>

#include <asm/uaccess.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>
#include <asm/mmu_context.h>
#include "internal.h"

#if 0
#define kenter(FMT, ...) \
	printk(KERN_DEBUG "==> %s("FMT")\n", __func__, ##__VA_ARGS__)
#define kleave(FMT, ...) \
	printk(KERN_DEBUG "<== %s()"FMT"\n", __func__, ##__VA_ARGS__)
#define kdebug(FMT, ...) \
	printk(KERN_DEBUG "xxx" FMT"yyy\n", ##__VA_ARGS__)
#else
#define kenter(FMT, ...) \
	no_printk(KERN_DEBUG "==> %s("FMT")\n", __func__, ##__VA_ARGS__)
#define kleave(FMT, ...) \
	no_printk(KERN_DEBUG "<== %s()"FMT"\n", __func__, ##__VA_ARGS__)
#define kdebug(FMT, ...) \
	no_printk(KERN_DEBUG FMT"\n", ##__VA_ARGS__)
#endif

void *high_memory;
struct page *mem_map;
unsigned long max_mapnr;
unsigned long num_physpages;
unsigned long highest_memmap_pfn;
struct percpu_counter vm_committed_as;
int sysctl_overcommit_memory = OVERCOMMIT_GUESS; 
int sysctl_overcommit_ratio = 50; 
int sysctl_max_map_count = DEFAULT_MAX_MAP_COUNT;
int sysctl_nr_trim_pages = CONFIG_NOMMU_INITIAL_TRIM_EXCESS;
int heap_stack_gap = 0;

atomic_long_t mmap_pages_allocated;

EXPORT_SYMBOL(mem_map);
EXPORT_SYMBOL(num_physpages);

static struct kmem_cache *vm_region_jar;
struct rb_root nommu_region_tree = RB_ROOT;
DECLARE_RWSEM(nommu_region_sem);

const struct vm_operations_struct generic_file_vm_ops = {
};

unsigned int kobjsize(const void *objp)
{
	struct page *page;

	if (!objp || !virt_addr_valid(objp))
		return 0;

	page = virt_to_head_page(objp);

	if (PageSlab(page))
		return ksize(objp);

	if (!PageCompound(page)) {
		struct vm_area_struct *vma;

		vma = find_vma(current->mm, (unsigned long)objp);
		if (vma)
			return vma->vm_end - vma->vm_start;
	}

	return PAGE_SIZE << compound_order(page);
}

int __get_user_pages(struct task_struct *tsk, struct mm_struct *mm,
		     unsigned long start, int nr_pages, unsigned int foll_flags,
		     struct page **pages, struct vm_area_struct **vmas,
		     int *retry)
{
	struct vm_area_struct *vma;
	unsigned long vm_flags;
	int i;

	vm_flags  = (foll_flags & FOLL_WRITE) ?
			(VM_WRITE | VM_MAYWRITE) : (VM_READ | VM_MAYREAD);
	vm_flags &= (foll_flags & FOLL_FORCE) ?
			(VM_MAYREAD | VM_MAYWRITE) : (VM_READ | VM_WRITE);

	for (i = 0; i < nr_pages; i++) {
		vma = find_vma(mm, start);
		if (!vma)
			goto finish_or_fault;

		
		if ((vma->vm_flags & (VM_IO | VM_PFNMAP)) ||
		    !(vm_flags & vma->vm_flags))
			goto finish_or_fault;

		if (pages) {
			pages[i] = virt_to_page(start);
			if (pages[i])
				page_cache_get(pages[i]);
		}
		if (vmas)
			vmas[i] = vma;
		start = (start + PAGE_SIZE) & PAGE_MASK;
	}

	return i;

finish_or_fault:
	return i ? : -EFAULT;
}

int get_user_pages(struct task_struct *tsk, struct mm_struct *mm,
	unsigned long start, int nr_pages, int write, int force,
	struct page **pages, struct vm_area_struct **vmas)
{
	int flags = 0;

	if (write)
		flags |= FOLL_WRITE;
	if (force)
		flags |= FOLL_FORCE;

	return __get_user_pages(tsk, mm, start, nr_pages, flags, pages, vmas,
				NULL);
}
EXPORT_SYMBOL(get_user_pages);

int follow_pfn(struct vm_area_struct *vma, unsigned long address,
	unsigned long *pfn)
{
	if (!(vma->vm_flags & (VM_IO | VM_PFNMAP)))
		return -EINVAL;

	*pfn = address >> PAGE_SHIFT;
	return 0;
}
EXPORT_SYMBOL(follow_pfn);

DEFINE_RWLOCK(vmlist_lock);
struct vm_struct *vmlist;

void vfree(const void *addr)
{
	kfree(addr);
}
EXPORT_SYMBOL(vfree);

void *__vmalloc(unsigned long size, gfp_t gfp_mask, pgprot_t prot)
{
	return kmalloc(size, (gfp_mask | __GFP_COMP) & ~__GFP_HIGHMEM);
}
EXPORT_SYMBOL(__vmalloc);

void *vmalloc_user(unsigned long size)
{
	void *ret;

	ret = __vmalloc(size, GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO,
			PAGE_KERNEL);
	if (ret) {
		struct vm_area_struct *vma;

		down_write(&current->mm->mmap_sem);
		vma = find_vma(current->mm, (unsigned long)ret);
		if (vma)
			vma->vm_flags |= VM_USERMAP;
		up_write(&current->mm->mmap_sem);
	}

	return ret;
}
EXPORT_SYMBOL(vmalloc_user);

struct page *vmalloc_to_page(const void *addr)
{
	return virt_to_page(addr);
}
EXPORT_SYMBOL(vmalloc_to_page);

unsigned long vmalloc_to_pfn(const void *addr)
{
	return page_to_pfn(virt_to_page(addr));
}
EXPORT_SYMBOL(vmalloc_to_pfn);

long vread(char *buf, char *addr, unsigned long count)
{
	memcpy(buf, addr, count);
	return count;
}

long vwrite(char *buf, char *addr, unsigned long count)
{
	
	if ((unsigned long) addr + count < count)
		count = -(unsigned long) addr;

	memcpy(addr, buf, count);
	return(count);
}

void *vmalloc(unsigned long size)
{
       return __vmalloc(size, GFP_KERNEL | __GFP_HIGHMEM, PAGE_KERNEL);
}
EXPORT_SYMBOL(vmalloc);

void *vzalloc(unsigned long size)
{
	return __vmalloc(size, GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO,
			PAGE_KERNEL);
}
EXPORT_SYMBOL(vzalloc);

void *vmalloc_node(unsigned long size, int node)
{
	return vmalloc(size);
}
EXPORT_SYMBOL(vmalloc_node);

void *vzalloc_node(unsigned long size, int node)
{
	return vzalloc(size);
}
EXPORT_SYMBOL(vzalloc_node);

#ifndef PAGE_KERNEL_EXEC
# define PAGE_KERNEL_EXEC PAGE_KERNEL
#endif


void *vmalloc_exec(unsigned long size)
{
	return __vmalloc(size, GFP_KERNEL | __GFP_HIGHMEM, PAGE_KERNEL_EXEC);
}

void *vmalloc_32(unsigned long size)
{
	return __vmalloc(size, GFP_KERNEL, PAGE_KERNEL);
}
EXPORT_SYMBOL(vmalloc_32);

void *vmalloc_32_user(unsigned long size)
{
	return vmalloc_user(size);
}
EXPORT_SYMBOL(vmalloc_32_user);

void *vmap(struct page **pages, unsigned int count, unsigned long flags, pgprot_t prot)
{
	BUG();
	return NULL;
}
EXPORT_SYMBOL(vmap);

void vunmap(const void *addr)
{
	BUG();
}
EXPORT_SYMBOL(vunmap);

void *vm_map_ram(struct page **pages, unsigned int count, int node, pgprot_t prot)
{
	BUG();
	return NULL;
}
EXPORT_SYMBOL(vm_map_ram);

void vm_unmap_ram(const void *mem, unsigned int count)
{
	BUG();
}
EXPORT_SYMBOL(vm_unmap_ram);

void vm_unmap_aliases(void)
{
}
EXPORT_SYMBOL_GPL(vm_unmap_aliases);

void  __attribute__((weak)) vmalloc_sync_all(void)
{
}

struct vm_struct *alloc_vm_area(size_t size, pte_t **ptes)
{
	BUG();
	return NULL;
}
EXPORT_SYMBOL_GPL(alloc_vm_area);

void free_vm_area(struct vm_struct *area)
{
	BUG();
}
EXPORT_SYMBOL_GPL(free_vm_area);

int vm_insert_page(struct vm_area_struct *vma, unsigned long addr,
		   struct page *page)
{
	return -EINVAL;
}
EXPORT_SYMBOL(vm_insert_page);

SYSCALL_DEFINE1(brk, unsigned long, brk)
{
	struct mm_struct *mm = current->mm;

	if (brk < mm->start_brk || brk > mm->context.end_brk)
		return mm->brk;

	if (mm->brk == brk)
		return mm->brk;

	if (brk <= mm->brk) {
		mm->brk = brk;
		return brk;
	}

	flush_icache_range(mm->brk, brk);
	return mm->brk = brk;
}

void __init mmap_init(void)
{
	int ret;

	ret = percpu_counter_init(&vm_committed_as, 0);
	VM_BUG_ON(ret);
	vm_region_jar = KMEM_CACHE(vm_region, SLAB_PANIC);
}

#ifdef CONFIG_DEBUG_NOMMU_REGIONS
static noinline void validate_nommu_regions(void)
{
	struct vm_region *region, *last;
	struct rb_node *p, *lastp;

	lastp = rb_first(&nommu_region_tree);
	if (!lastp)
		return;

	last = rb_entry(lastp, struct vm_region, vm_rb);
	BUG_ON(unlikely(last->vm_end <= last->vm_start));
	BUG_ON(unlikely(last->vm_top < last->vm_end));

	while ((p = rb_next(lastp))) {
		region = rb_entry(p, struct vm_region, vm_rb);
		last = rb_entry(lastp, struct vm_region, vm_rb);

		BUG_ON(unlikely(region->vm_end <= region->vm_start));
		BUG_ON(unlikely(region->vm_top < region->vm_end));
		BUG_ON(unlikely(region->vm_start < last->vm_top));

		lastp = p;
	}
}
#else
static void validate_nommu_regions(void)
{
}
#endif

static void add_nommu_region(struct vm_region *region)
{
	struct vm_region *pregion;
	struct rb_node **p, *parent;

	validate_nommu_regions();

	parent = NULL;
	p = &nommu_region_tree.rb_node;
	while (*p) {
		parent = *p;
		pregion = rb_entry(parent, struct vm_region, vm_rb);
		if (region->vm_start < pregion->vm_start)
			p = &(*p)->rb_left;
		else if (region->vm_start > pregion->vm_start)
			p = &(*p)->rb_right;
		else if (pregion == region)
			return;
		else
			BUG();
	}

	rb_link_node(&region->vm_rb, parent, p);
	rb_insert_color(&region->vm_rb, &nommu_region_tree);

	validate_nommu_regions();
}

static void delete_nommu_region(struct vm_region *region)
{
	BUG_ON(!nommu_region_tree.rb_node);

	validate_nommu_regions();
	rb_erase(&region->vm_rb, &nommu_region_tree);
	validate_nommu_regions();
}

static void free_page_series(unsigned long from, unsigned long to)
{
	for (; from < to; from += PAGE_SIZE) {
		struct page *page = virt_to_page(from);

		kdebug("- free %lx", from);
		atomic_long_dec(&mmap_pages_allocated);
		if (page_count(page) != 1)
			kdebug("free page %p: refcount not one: %d",
			       page, page_count(page));
		put_page(page);
	}
}

static void __put_nommu_region(struct vm_region *region)
	__releases(nommu_region_sem)
{
	kenter("%p{%d}", region, region->vm_usage);

	BUG_ON(!nommu_region_tree.rb_node);

	if (--region->vm_usage == 0) {
		if (region->vm_top > region->vm_start)
			delete_nommu_region(region);
		up_write(&nommu_region_sem);

		if (region->vm_file)
			fput(region->vm_file);

		if (region->vm_flags & VM_MAPPED_COPY) {
			kdebug("free series");
			free_page_series(region->vm_start, region->vm_top);
		}
		kmem_cache_free(vm_region_jar, region);
	} else {
		up_write(&nommu_region_sem);
	}
}

static void put_nommu_region(struct vm_region *region)
{
	down_write(&nommu_region_sem);
	__put_nommu_region(region);
}

static void protect_vma(struct vm_area_struct *vma, unsigned long flags)
{
#ifdef CONFIG_MPU
	struct mm_struct *mm = vma->vm_mm;
	long start = vma->vm_start & PAGE_MASK;
	while (start < vma->vm_end) {
		protect_page(mm, start, flags);
		start += PAGE_SIZE;
	}
	update_protections(mm);
#endif
}

static void add_vma_to_mm(struct mm_struct *mm, struct vm_area_struct *vma)
{
	struct vm_area_struct *pvma, *prev;
	struct address_space *mapping;
	struct rb_node **p, *parent, *rb_prev;

	kenter(",%p", vma);

	BUG_ON(!vma->vm_region);

	mm->map_count++;
	vma->vm_mm = mm;

	protect_vma(vma, vma->vm_flags);

	
	if (vma->vm_file) {
		mapping = vma->vm_file->f_mapping;

		mutex_lock(&mapping->i_mmap_mutex);
		flush_dcache_mmap_lock(mapping);
		vma_prio_tree_insert(vma, &mapping->i_mmap);
		flush_dcache_mmap_unlock(mapping);
		mutex_unlock(&mapping->i_mmap_mutex);
	}

	
	parent = rb_prev = NULL;
	p = &mm->mm_rb.rb_node;
	while (*p) {
		parent = *p;
		pvma = rb_entry(parent, struct vm_area_struct, vm_rb);

		if (vma->vm_start < pvma->vm_start)
			p = &(*p)->rb_left;
		else if (vma->vm_start > pvma->vm_start) {
			rb_prev = parent;
			p = &(*p)->rb_right;
		} else if (vma->vm_end < pvma->vm_end)
			p = &(*p)->rb_left;
		else if (vma->vm_end > pvma->vm_end) {
			rb_prev = parent;
			p = &(*p)->rb_right;
		} else if (vma < pvma)
			p = &(*p)->rb_left;
		else if (vma > pvma) {
			rb_prev = parent;
			p = &(*p)->rb_right;
		} else
			BUG();
	}

	rb_link_node(&vma->vm_rb, parent, p);
	rb_insert_color(&vma->vm_rb, &mm->mm_rb);

	
	prev = NULL;
	if (rb_prev)
		prev = rb_entry(rb_prev, struct vm_area_struct, vm_rb);

	__vma_link_list(mm, vma, prev, parent);
}

static void delete_vma_from_mm(struct vm_area_struct *vma)
{
	struct address_space *mapping;
	struct mm_struct *mm = vma->vm_mm;

	kenter("%p", vma);

	protect_vma(vma, 0);

	mm->map_count--;
	if (mm->mmap_cache == vma)
		mm->mmap_cache = NULL;

	
	if (vma->vm_file) {
		mapping = vma->vm_file->f_mapping;

		mutex_lock(&mapping->i_mmap_mutex);
		flush_dcache_mmap_lock(mapping);
		vma_prio_tree_remove(vma, &mapping->i_mmap);
		flush_dcache_mmap_unlock(mapping);
		mutex_unlock(&mapping->i_mmap_mutex);
	}

	
	rb_erase(&vma->vm_rb, &mm->mm_rb);

	if (vma->vm_prev)
		vma->vm_prev->vm_next = vma->vm_next;
	else
		mm->mmap = vma->vm_next;

	if (vma->vm_next)
		vma->vm_next->vm_prev = vma->vm_prev;
}

static void delete_vma(struct mm_struct *mm, struct vm_area_struct *vma)
{
	kenter("%p", vma);
	if (vma->vm_ops && vma->vm_ops->close)
		vma->vm_ops->close(vma);
	if (vma->vm_file) {
		fput(vma->vm_file);
		if (vma->vm_flags & VM_EXECUTABLE)
			removed_exe_file_vma(mm);
	}
	put_nommu_region(vma->vm_region);
	kmem_cache_free(vm_area_cachep, vma);
}

struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr)
{
	struct vm_area_struct *vma;

	
	vma = mm->mmap_cache;
	if (vma && vma->vm_start <= addr && vma->vm_end > addr)
		return vma;

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (vma->vm_start > addr)
			return NULL;
		if (vma->vm_end > addr) {
			mm->mmap_cache = vma;
			return vma;
		}
	}

	return NULL;
}
EXPORT_SYMBOL(find_vma);

struct vm_area_struct *find_extend_vma(struct mm_struct *mm, unsigned long addr)
{
	return find_vma(mm, addr);
}

int expand_stack(struct vm_area_struct *vma, unsigned long address)
{
	return -ENOMEM;
}

static struct vm_area_struct *find_vma_exact(struct mm_struct *mm,
					     unsigned long addr,
					     unsigned long len)
{
	struct vm_area_struct *vma;
	unsigned long end = addr + len;

	
	vma = mm->mmap_cache;
	if (vma && vma->vm_start == addr && vma->vm_end == end)
		return vma;

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (vma->vm_start < addr)
			continue;
		if (vma->vm_start > addr)
			return NULL;
		if (vma->vm_end == end) {
			mm->mmap_cache = vma;
			return vma;
		}
	}

	return NULL;
}

static int validate_mmap_request(struct file *file,
				 unsigned long addr,
				 unsigned long len,
				 unsigned long prot,
				 unsigned long flags,
				 unsigned long pgoff,
				 unsigned long *_capabilities)
{
	unsigned long capabilities, rlen;
	unsigned long reqprot = prot;
	int ret;

	
	if (flags & MAP_FIXED) {
		printk(KERN_DEBUG
		       "%d: Can't do fixed-address/overlay mmap of RAM\n",
		       current->pid);
		return -EINVAL;
	}

	if ((flags & MAP_TYPE) != MAP_PRIVATE &&
	    (flags & MAP_TYPE) != MAP_SHARED)
		return -EINVAL;

	if (!len)
		return -EINVAL;

	
	rlen = PAGE_ALIGN(len);
	if (!rlen || rlen > TASK_SIZE)
		return -ENOMEM;

	
	if ((pgoff + (rlen >> PAGE_SHIFT)) < pgoff)
		return -EOVERFLOW;

	if (file) {
		
		struct address_space *mapping;

		
		if (!file->f_op || !file->f_op->mmap)
			return -ENODEV;

		mapping = file->f_mapping;
		if (!mapping)
			mapping = file->f_path.dentry->d_inode->i_mapping;

		capabilities = 0;
		if (mapping && mapping->backing_dev_info)
			capabilities = mapping->backing_dev_info->capabilities;

		if (!capabilities) {
			switch (file->f_path.dentry->d_inode->i_mode & S_IFMT) {
			case S_IFREG:
			case S_IFBLK:
				capabilities = BDI_CAP_MAP_COPY;
				break;

			case S_IFCHR:
				capabilities =
					BDI_CAP_MAP_DIRECT |
					BDI_CAP_READ_MAP |
					BDI_CAP_WRITE_MAP;
				break;

			default:
				return -EINVAL;
			}
		}

		if (!file->f_op->get_unmapped_area)
			capabilities &= ~BDI_CAP_MAP_DIRECT;
		if (!file->f_op->read)
			capabilities &= ~BDI_CAP_MAP_COPY;

		
		if (!(file->f_mode & FMODE_READ))
			return -EACCES;

		if (flags & MAP_SHARED) {
			
			if ((prot & PROT_WRITE) &&
			    !(file->f_mode & FMODE_WRITE))
				return -EACCES;

			if (IS_APPEND(file->f_path.dentry->d_inode) &&
			    (file->f_mode & FMODE_WRITE))
				return -EACCES;

			if (locks_verify_locked(file->f_path.dentry->d_inode))
				return -EAGAIN;

			if (!(capabilities & BDI_CAP_MAP_DIRECT))
				return -ENODEV;

			
			capabilities &= ~BDI_CAP_MAP_COPY;
		}
		else {
			if (!(capabilities & BDI_CAP_MAP_COPY))
				return -ENODEV;

			if (prot & PROT_WRITE)
				capabilities &= ~BDI_CAP_MAP_DIRECT;
		}

		if (capabilities & BDI_CAP_MAP_DIRECT) {
			if (((prot & PROT_READ)  && !(capabilities & BDI_CAP_READ_MAP))  ||
			    ((prot & PROT_WRITE) && !(capabilities & BDI_CAP_WRITE_MAP)) ||
			    ((prot & PROT_EXEC)  && !(capabilities & BDI_CAP_EXEC_MAP))
			    ) {
				capabilities &= ~BDI_CAP_MAP_DIRECT;
				if (flags & MAP_SHARED) {
					printk(KERN_WARNING
					       "MAP_SHARED not completely supported on !MMU\n");
					return -EINVAL;
				}
			}
		}

		if (file->f_path.mnt->mnt_flags & MNT_NOEXEC) {
			if (prot & PROT_EXEC)
				return -EPERM;
		}
		else if ((prot & PROT_READ) && !(prot & PROT_EXEC)) {
			
			if (current->personality & READ_IMPLIES_EXEC) {
				if (capabilities & BDI_CAP_EXEC_MAP)
					prot |= PROT_EXEC;
			}
		}
		else if ((prot & PROT_READ) &&
			 (prot & PROT_EXEC) &&
			 !(capabilities & BDI_CAP_EXEC_MAP)
			 ) {
			
			capabilities &= ~BDI_CAP_MAP_DIRECT;
		}
	}
	else {
		capabilities = BDI_CAP_MAP_COPY;

		
		if ((prot & PROT_READ) &&
		    (current->personality & READ_IMPLIES_EXEC))
			prot |= PROT_EXEC;
	}

	
	ret = security_file_mmap(file, reqprot, prot, flags, addr, 0);
	if (ret < 0)
		return ret;

	
	*_capabilities = capabilities;
	return 0;
}

static unsigned long determine_vm_flags(struct file *file,
					unsigned long prot,
					unsigned long flags,
					unsigned long capabilities)
{
	unsigned long vm_flags;

	vm_flags = calc_vm_prot_bits(prot) | calc_vm_flag_bits(flags);
	

	if (!(capabilities & BDI_CAP_MAP_DIRECT)) {
		
		vm_flags |= VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC;
		if (file && !(prot & PROT_WRITE))
			vm_flags |= VM_MAYSHARE;
	} else {
		vm_flags |= VM_MAYSHARE | (capabilities & BDI_CAP_VMFLAGS);
		if (flags & MAP_SHARED)
			vm_flags |= VM_SHARED;
	}

	if ((flags & MAP_PRIVATE) && current->ptrace)
		vm_flags &= ~VM_MAYSHARE;

	return vm_flags;
}

static int do_mmap_shared_file(struct vm_area_struct *vma)
{
	int ret;

	ret = vma->vm_file->f_op->mmap(vma->vm_file, vma);
	if (ret == 0) {
		vma->vm_region->vm_top = vma->vm_region->vm_end;
		return 0;
	}
	if (ret != -ENOSYS)
		return ret;

	return -ENODEV;
}

static int do_mmap_private(struct vm_area_struct *vma,
			   struct vm_region *region,
			   unsigned long len,
			   unsigned long capabilities)
{
	struct page *pages;
	unsigned long total, point, n;
	void *base;
	int ret, order;

	if (capabilities & BDI_CAP_MAP_DIRECT) {
		ret = vma->vm_file->f_op->mmap(vma->vm_file, vma);
		if (ret == 0) {
			
			BUG_ON(!(vma->vm_flags & VM_MAYSHARE));
			vma->vm_region->vm_top = vma->vm_region->vm_end;
			return 0;
		}
		if (ret != -ENOSYS)
			return ret;

	}


	order = get_order(len);
	kdebug("alloc order %d for %lx", order, len);

	pages = alloc_pages(GFP_KERNEL, order);
	if (!pages)
		goto enomem;

	total = 1 << order;
	atomic_long_add(total, &mmap_pages_allocated);

	point = len >> PAGE_SHIFT;

	if (sysctl_nr_trim_pages && total - point >= sysctl_nr_trim_pages) {
		while (total > point) {
			order = ilog2(total - point);
			n = 1 << order;
			kdebug("shave %lu/%lu @%lu", n, total - point, total);
			atomic_long_sub(n, &mmap_pages_allocated);
			total -= n;
			set_page_refcounted(pages + total);
			__free_pages(pages + total, order);
		}
	}

	for (point = 1; point < total; point++)
		set_page_refcounted(&pages[point]);

	base = page_address(pages);
	region->vm_flags = vma->vm_flags |= VM_MAPPED_COPY;
	region->vm_start = (unsigned long) base;
	region->vm_end   = region->vm_start + len;
	region->vm_top   = region->vm_start + (total << PAGE_SHIFT);

	vma->vm_start = region->vm_start;
	vma->vm_end   = region->vm_start + len;

	if (vma->vm_file) {
		
		mm_segment_t old_fs;
		loff_t fpos;

		fpos = vma->vm_pgoff;
		fpos <<= PAGE_SHIFT;

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		ret = vma->vm_file->f_op->read(vma->vm_file, base, len, &fpos);
		set_fs(old_fs);

		if (ret < 0)
			goto error_free;

		
		if (ret < len)
			memset(base + ret, 0, len - ret);

	}

	return 0;

error_free:
	free_page_series(region->vm_start, region->vm_top);
	region->vm_start = vma->vm_start = 0;
	region->vm_end   = vma->vm_end = 0;
	region->vm_top   = 0;
	return ret;

enomem:
	printk("Allocation of length %lu from process %d (%s) failed\n",
	       len, current->pid, current->comm);
	show_free_areas(0);
	return -ENOMEM;
}

static unsigned long do_mmap_pgoff(struct file *file,
			    unsigned long addr,
			    unsigned long len,
			    unsigned long prot,
			    unsigned long flags,
			    unsigned long pgoff)
{
	struct vm_area_struct *vma;
	struct vm_region *region;
	struct rb_node *rb;
	unsigned long capabilities, vm_flags, result;
	int ret;

	kenter(",%lx,%lx,%lx,%lx,%lx", addr, len, prot, flags, pgoff);

	ret = validate_mmap_request(file, addr, len, prot, flags, pgoff,
				    &capabilities);
	if (ret < 0) {
		kleave(" = %d [val]", ret);
		return ret;
	}

	
	addr = 0;
	len = PAGE_ALIGN(len);

	vm_flags = determine_vm_flags(file, prot, flags, capabilities);

	
	region = kmem_cache_zalloc(vm_region_jar, GFP_KERNEL);
	if (!region)
		goto error_getting_region;

	vma = kmem_cache_zalloc(vm_area_cachep, GFP_KERNEL);
	if (!vma)
		goto error_getting_vma;

	region->vm_usage = 1;
	region->vm_flags = vm_flags;
	region->vm_pgoff = pgoff;

	INIT_LIST_HEAD(&vma->anon_vma_chain);
	vma->vm_flags = vm_flags;
	vma->vm_pgoff = pgoff;

	if (file) {
		region->vm_file = file;
		get_file(file);
		vma->vm_file = file;
		get_file(file);
		if (vm_flags & VM_EXECUTABLE) {
			added_exe_file_vma(current->mm);
			vma->vm_mm = current->mm;
		}
	}

	down_write(&nommu_region_sem);

	if (vm_flags & VM_MAYSHARE) {
		struct vm_region *pregion;
		unsigned long pglen, rpglen, pgend, rpgend, start;

		pglen = (len + PAGE_SIZE - 1) >> PAGE_SHIFT;
		pgend = pgoff + pglen;

		for (rb = rb_first(&nommu_region_tree); rb; rb = rb_next(rb)) {
			pregion = rb_entry(rb, struct vm_region, vm_rb);

			if (!(pregion->vm_flags & VM_MAYSHARE))
				continue;

			
			if (pregion->vm_file->f_path.dentry->d_inode !=
			    file->f_path.dentry->d_inode)
				continue;

			if (pregion->vm_pgoff >= pgend)
				continue;

			rpglen = pregion->vm_end - pregion->vm_start;
			rpglen = (rpglen + PAGE_SIZE - 1) >> PAGE_SHIFT;
			rpgend = pregion->vm_pgoff + rpglen;
			if (pgoff >= rpgend)
				continue;

			if ((pregion->vm_pgoff != pgoff || rpglen != pglen) &&
			    !(pgoff >= pregion->vm_pgoff && pgend <= rpgend)) {
				
				if (!(capabilities & BDI_CAP_MAP_DIRECT))
					goto sharing_violation;
				continue;
			}

			
			pregion->vm_usage++;
			vma->vm_region = pregion;
			start = pregion->vm_start;
			start += (pgoff - pregion->vm_pgoff) << PAGE_SHIFT;
			vma->vm_start = start;
			vma->vm_end = start + len;

			if (pregion->vm_flags & VM_MAPPED_COPY) {
				kdebug("share copy");
				vma->vm_flags |= VM_MAPPED_COPY;
			} else {
				kdebug("share mmap");
				ret = do_mmap_shared_file(vma);
				if (ret < 0) {
					vma->vm_region = NULL;
					vma->vm_start = 0;
					vma->vm_end = 0;
					pregion->vm_usage--;
					pregion = NULL;
					goto error_just_free;
				}
			}
			fput(region->vm_file);
			kmem_cache_free(vm_region_jar, region);
			region = pregion;
			result = start;
			goto share;
		}

		if (capabilities & BDI_CAP_MAP_DIRECT) {
			addr = file->f_op->get_unmapped_area(file, addr, len,
							     pgoff, flags);
			if (IS_ERR_VALUE(addr)) {
				ret = addr;
				if (ret != -ENOSYS)
					goto error_just_free;

				ret = -ENODEV;
				if (!(capabilities & BDI_CAP_MAP_COPY))
					goto error_just_free;

				capabilities &= ~BDI_CAP_MAP_DIRECT;
			} else {
				vma->vm_start = region->vm_start = addr;
				vma->vm_end = region->vm_end = addr + len;
			}
		}
	}

	vma->vm_region = region;

	if (file && vma->vm_flags & VM_SHARED)
		ret = do_mmap_shared_file(vma);
	else
		ret = do_mmap_private(vma, region, len, capabilities);
	if (ret < 0)
		goto error_just_free;
	add_nommu_region(region);

	
	if (!vma->vm_file && !(flags & MAP_UNINITIALIZED))
		memset((void *)region->vm_start, 0,
		       region->vm_end - region->vm_start);

	
	result = vma->vm_start;

	current->mm->total_vm += len >> PAGE_SHIFT;

share:
	add_vma_to_mm(current->mm, vma);

	if (vma->vm_flags & VM_EXEC && !region->vm_icache_flushed) {
		flush_icache_range(region->vm_start, region->vm_end);
		region->vm_icache_flushed = true;
	}

	up_write(&nommu_region_sem);

	kleave(" = %lx", result);
	return result;

error_just_free:
	up_write(&nommu_region_sem);
error:
	if (region->vm_file)
		fput(region->vm_file);
	kmem_cache_free(vm_region_jar, region);
	if (vma->vm_file)
		fput(vma->vm_file);
	if (vma->vm_flags & VM_EXECUTABLE)
		removed_exe_file_vma(vma->vm_mm);
	kmem_cache_free(vm_area_cachep, vma);
	kleave(" = %d", ret);
	return ret;

sharing_violation:
	up_write(&nommu_region_sem);
	printk(KERN_WARNING "Attempt to share mismatched mappings\n");
	ret = -EINVAL;
	goto error;

error_getting_vma:
	kmem_cache_free(vm_region_jar, region);
	printk(KERN_WARNING "Allocation of vma for %lu byte allocation"
	       " from process %d failed\n",
	       len, current->pid);
	show_free_areas(0);
	return -ENOMEM;

error_getting_region:
	printk(KERN_WARNING "Allocation of vm region for %lu byte allocation"
	       " from process %d failed\n",
	       len, current->pid);
	show_free_areas(0);
	return -ENOMEM;
}

unsigned long do_mmap(struct file *file, unsigned long addr,
	unsigned long len, unsigned long prot,
	unsigned long flag, unsigned long offset)
{
	if (unlikely(offset + PAGE_ALIGN(len) < offset))
		return -EINVAL;
	if (unlikely(offset & ~PAGE_MASK))
		return -EINVAL;
	return do_mmap_pgoff(file, addr, len, prot, flag, offset >> PAGE_SHIFT);
}
EXPORT_SYMBOL(do_mmap);

unsigned long vm_mmap(struct file *file, unsigned long addr,
	unsigned long len, unsigned long prot,
	unsigned long flag, unsigned long offset)
{
	unsigned long ret;
	struct mm_struct *mm = current->mm;

	down_write(&mm->mmap_sem);
	ret = do_mmap(file, addr, len, prot, flag, offset);
	up_write(&mm->mmap_sem);
	return ret;
}
EXPORT_SYMBOL(vm_mmap);

SYSCALL_DEFINE6(mmap_pgoff, unsigned long, addr, unsigned long, len,
		unsigned long, prot, unsigned long, flags,
		unsigned long, fd, unsigned long, pgoff)
{
	struct file *file = NULL;
	unsigned long retval = -EBADF;

	audit_mmap_fd(fd, flags);
	if (!(flags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			goto out;
	}

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);

	down_write(&current->mm->mmap_sem);
	retval = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);

	if (file)
		fput(file);
out:
	return retval;
}

#ifdef __ARCH_WANT_SYS_OLD_MMAP
struct mmap_arg_struct {
	unsigned long addr;
	unsigned long len;
	unsigned long prot;
	unsigned long flags;
	unsigned long fd;
	unsigned long offset;
};

SYSCALL_DEFINE1(old_mmap, struct mmap_arg_struct __user *, arg)
{
	struct mmap_arg_struct a;

	if (copy_from_user(&a, arg, sizeof(a)))
		return -EFAULT;
	if (a.offset & ~PAGE_MASK)
		return -EINVAL;

	return sys_mmap_pgoff(a.addr, a.len, a.prot, a.flags, a.fd,
			      a.offset >> PAGE_SHIFT);
}
#endif 

int split_vma(struct mm_struct *mm, struct vm_area_struct *vma,
	      unsigned long addr, int new_below)
{
	struct vm_area_struct *new;
	struct vm_region *region;
	unsigned long npages;

	kenter("");

	if (vma->vm_file)
		return -ENOMEM;

	if (mm->map_count >= sysctl_max_map_count)
		return -ENOMEM;

	region = kmem_cache_alloc(vm_region_jar, GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	new = kmem_cache_alloc(vm_area_cachep, GFP_KERNEL);
	if (!new) {
		kmem_cache_free(vm_region_jar, region);
		return -ENOMEM;
	}

	
	*new = *vma;
	*region = *vma->vm_region;
	new->vm_region = region;

	npages = (addr - vma->vm_start) >> PAGE_SHIFT;

	if (new_below) {
		region->vm_top = region->vm_end = new->vm_end = addr;
	} else {
		region->vm_start = new->vm_start = addr;
		region->vm_pgoff = new->vm_pgoff += npages;
	}

	if (new->vm_ops && new->vm_ops->open)
		new->vm_ops->open(new);

	delete_vma_from_mm(vma);
	down_write(&nommu_region_sem);
	delete_nommu_region(vma->vm_region);
	if (new_below) {
		vma->vm_region->vm_start = vma->vm_start = addr;
		vma->vm_region->vm_pgoff = vma->vm_pgoff += npages;
	} else {
		vma->vm_region->vm_end = vma->vm_end = addr;
		vma->vm_region->vm_top = addr;
	}
	add_nommu_region(vma->vm_region);
	add_nommu_region(new->vm_region);
	up_write(&nommu_region_sem);
	add_vma_to_mm(mm, vma);
	add_vma_to_mm(mm, new);
	return 0;
}

static int shrink_vma(struct mm_struct *mm,
		      struct vm_area_struct *vma,
		      unsigned long from, unsigned long to)
{
	struct vm_region *region;

	kenter("");

	delete_vma_from_mm(vma);
	if (from > vma->vm_start)
		vma->vm_end = from;
	else
		vma->vm_start = to;
	add_vma_to_mm(mm, vma);

	
	region = vma->vm_region;
	BUG_ON(region->vm_usage != 1);

	down_write(&nommu_region_sem);
	delete_nommu_region(region);
	if (from > region->vm_start) {
		to = region->vm_top;
		region->vm_top = region->vm_end = from;
	} else {
		region->vm_start = to;
	}
	add_nommu_region(region);
	up_write(&nommu_region_sem);

	free_page_series(from, to);
	return 0;
}

int do_munmap(struct mm_struct *mm, unsigned long start, size_t len)
{
	struct vm_area_struct *vma;
	unsigned long end;
	int ret;

	kenter(",%lx,%zx", start, len);

	len = PAGE_ALIGN(len);
	if (len == 0)
		return -EINVAL;

	end = start + len;

	
	vma = find_vma(mm, start);
	if (!vma) {
		static int limit = 0;
		if (limit < 5) {
			printk(KERN_WARNING
			       "munmap of memory not mmapped by process %d"
			       " (%s): 0x%lx-0x%lx\n",
			       current->pid, current->comm,
			       start, start + len - 1);
			limit++;
		}
		return -EINVAL;
	}

	
	if (vma->vm_file) {
		do {
			if (start > vma->vm_start) {
				kleave(" = -EINVAL [miss]");
				return -EINVAL;
			}
			if (end == vma->vm_end)
				goto erase_whole_vma;
			vma = vma->vm_next;
		} while (vma);
		kleave(" = -EINVAL [split file]");
		return -EINVAL;
	} else {
		
		if (start == vma->vm_start && end == vma->vm_end)
			goto erase_whole_vma;
		if (start < vma->vm_start || end > vma->vm_end) {
			kleave(" = -EINVAL [superset]");
			return -EINVAL;
		}
		if (start & ~PAGE_MASK) {
			kleave(" = -EINVAL [unaligned start]");
			return -EINVAL;
		}
		if (end != vma->vm_end && end & ~PAGE_MASK) {
			kleave(" = -EINVAL [unaligned split]");
			return -EINVAL;
		}
		if (start != vma->vm_start && end != vma->vm_end) {
			ret = split_vma(mm, vma, start, 1);
			if (ret < 0) {
				kleave(" = %d [split]", ret);
				return ret;
			}
		}
		return shrink_vma(mm, vma, start, end);
	}

erase_whole_vma:
	delete_vma_from_mm(vma);
	delete_vma(mm, vma);
	kleave(" = 0");
	return 0;
}
EXPORT_SYMBOL(do_munmap);

int vm_munmap(unsigned long addr, size_t len)
{
	struct mm_struct *mm = current->mm;
	int ret;

	down_write(&mm->mmap_sem);
	ret = do_munmap(mm, addr, len);
	up_write(&mm->mmap_sem);
	return ret;
}
EXPORT_SYMBOL(vm_munmap);

SYSCALL_DEFINE2(munmap, unsigned long, addr, size_t, len)
{
	return vm_munmap(addr, len);
}

void exit_mmap(struct mm_struct *mm)
{
	struct vm_area_struct *vma;

	if (!mm)
		return;

	kenter("");

	mm->total_vm = 0;

	while ((vma = mm->mmap)) {
		mm->mmap = vma->vm_next;
		delete_vma_from_mm(vma);
		delete_vma(mm, vma);
		cond_resched();
	}

	kleave("");
}

unsigned long vm_brk(unsigned long addr, unsigned long len)
{
	return -ENOMEM;
}

unsigned long do_mremap(unsigned long addr,
			unsigned long old_len, unsigned long new_len,
			unsigned long flags, unsigned long new_addr)
{
	struct vm_area_struct *vma;

	
	old_len = PAGE_ALIGN(old_len);
	new_len = PAGE_ALIGN(new_len);
	if (old_len == 0 || new_len == 0)
		return (unsigned long) -EINVAL;

	if (addr & ~PAGE_MASK)
		return -EINVAL;

	if (flags & MREMAP_FIXED && new_addr != addr)
		return (unsigned long) -EINVAL;

	vma = find_vma_exact(current->mm, addr, old_len);
	if (!vma)
		return (unsigned long) -EINVAL;

	if (vma->vm_end != vma->vm_start + old_len)
		return (unsigned long) -EFAULT;

	if (vma->vm_flags & VM_MAYSHARE)
		return (unsigned long) -EPERM;

	if (new_len > vma->vm_region->vm_end - vma->vm_region->vm_start)
		return (unsigned long) -ENOMEM;

	
	vma->vm_end = vma->vm_start + new_len;
	return vma->vm_start;
}
EXPORT_SYMBOL(do_mremap);

SYSCALL_DEFINE5(mremap, unsigned long, addr, unsigned long, old_len,
		unsigned long, new_len, unsigned long, flags,
		unsigned long, new_addr)
{
	unsigned long ret;

	down_write(&current->mm->mmap_sem);
	ret = do_mremap(addr, old_len, new_len, flags, new_addr);
	up_write(&current->mm->mmap_sem);
	return ret;
}

struct page *follow_page(struct vm_area_struct *vma, unsigned long address,
			unsigned int foll_flags)
{
	return NULL;
}

int remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
		unsigned long pfn, unsigned long size, pgprot_t prot)
{
	if (addr != (pfn << PAGE_SHIFT))
		return -EINVAL;

	vma->vm_flags |= VM_IO | VM_RESERVED | VM_PFNMAP;
	return 0;
}
EXPORT_SYMBOL(remap_pfn_range);

int remap_vmalloc_range(struct vm_area_struct *vma, void *addr,
			unsigned long pgoff)
{
	unsigned int size = vma->vm_end - vma->vm_start;

	if (!(vma->vm_flags & VM_USERMAP))
		return -EINVAL;

	vma->vm_start = (unsigned long)(addr + (pgoff << PAGE_SHIFT));
	vma->vm_end = vma->vm_start + size;

	return 0;
}
EXPORT_SYMBOL(remap_vmalloc_range);

unsigned long arch_get_unmapped_area(struct file *file, unsigned long addr,
	unsigned long len, unsigned long pgoff, unsigned long flags)
{
	return -ENOMEM;
}

void arch_unmap_area(struct mm_struct *mm, unsigned long addr)
{
}

void unmap_mapping_range(struct address_space *mapping,
			 loff_t const holebegin, loff_t const holelen,
			 int even_cows)
{
}
EXPORT_SYMBOL(unmap_mapping_range);

int __vm_enough_memory(struct mm_struct *mm, long pages, int cap_sys_admin)
{
	unsigned long free, allowed;

	vm_acct_memory(pages);

	if (sysctl_overcommit_memory == OVERCOMMIT_ALWAYS)
		return 0;

	if (sysctl_overcommit_memory == OVERCOMMIT_GUESS) {
		free = global_page_state(NR_FREE_PAGES);
		free += global_page_state(NR_FILE_PAGES);

		free -= global_page_state(NR_SHMEM);

		free += get_nr_swap_pages();

		free += global_page_state(NR_SLAB_RECLAIMABLE);

		if (free <= totalreserve_pages)
			goto error;
		else
			free -= totalreserve_pages;

		if (!cap_sys_admin)
			free -= free / 32;

		if (free > pages)
			return 0;

		goto error;
	}

	allowed = totalram_pages * sysctl_overcommit_ratio / 100;
	if (!cap_sys_admin)
		allowed -= allowed / 32;
	allowed += total_swap_pages;

	if (mm)
		allowed -= mm->total_vm / 32;

	if (percpu_counter_read_positive(&vm_committed_as) < allowed)
		return 0;

error:
	vm_unacct_memory(pages);

	return -ENOMEM;
}

int in_gate_area_no_mm(unsigned long addr)
{
	return 0;
}

int filemap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	BUG();
	return 0;
}
EXPORT_SYMBOL(filemap_fault);

static int __access_remote_vm(struct task_struct *tsk, struct mm_struct *mm,
		unsigned long addr, void *buf, int len, int write)
{
	struct vm_area_struct *vma;

	down_read(&mm->mmap_sem);

	
	vma = find_vma(mm, addr);
	if (vma) {
		
		if (addr + len >= vma->vm_end)
			len = vma->vm_end - addr;

		
		if (write && vma->vm_flags & VM_MAYWRITE)
			copy_to_user_page(vma, NULL, addr,
					 (void *) addr, buf, len);
		else if (!write && vma->vm_flags & VM_MAYREAD)
			copy_from_user_page(vma, NULL, addr,
					    buf, (void *) addr, len);
		else
			len = 0;
	} else {
		len = 0;
	}

	up_read(&mm->mmap_sem);

	return len;
}

int access_remote_vm(struct mm_struct *mm, unsigned long addr,
		void *buf, int len, int write)
{
	return __access_remote_vm(NULL, mm, addr, buf, len, write);
}

int access_process_vm(struct task_struct *tsk, unsigned long addr, void *buf, int len, int write)
{
	struct mm_struct *mm;

	if (addr + len < addr)
		return 0;

	mm = get_task_mm(tsk);
	if (!mm)
		return 0;

	len = __access_remote_vm(tsk, mm, addr, buf, len, write);

	mmput(mm);
	return len;
}

int nommu_shrink_inode_mappings(struct inode *inode, size_t size,
				size_t newsize)
{
	struct vm_area_struct *vma;
	struct prio_tree_iter iter;
	struct vm_region *region;
	pgoff_t low, high;
	size_t r_size, r_top;

	low = newsize >> PAGE_SHIFT;
	high = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

	down_write(&nommu_region_sem);
	mutex_lock(&inode->i_mapping->i_mmap_mutex);

	
	vma_prio_tree_foreach(vma, &iter, &inode->i_mapping->i_mmap,
			      low, high) {
		if (vma->vm_flags & VM_SHARED) {
			mutex_unlock(&inode->i_mapping->i_mmap_mutex);
			up_write(&nommu_region_sem);
			return -ETXTBSY; 
		}
	}

	vma_prio_tree_foreach(vma, &iter, &inode->i_mapping->i_mmap,
			      0, ULONG_MAX) {
		if (!(vma->vm_flags & VM_SHARED))
			continue;

		region = vma->vm_region;
		r_size = region->vm_top - region->vm_start;
		r_top = (region->vm_pgoff << PAGE_SHIFT) + r_size;

		if (r_top > newsize) {
			region->vm_top -= r_top - newsize;
			if (region->vm_end > region->vm_top)
				region->vm_end = region->vm_top;
		}
	}

	mutex_unlock(&inode->i_mapping->i_mmap_mutex);
	up_write(&nommu_region_sem);
	return 0;
}
