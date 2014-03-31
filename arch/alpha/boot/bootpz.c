/*
 * arch/alpha/boot/bootpz.c
 *
 * Copyright (C) 1997 Jay Estabrook
 *
 * This file is used for creating a compressed BOOTP file for the
 * Linux/AXP kernel
 *
 * based significantly on the arch/alpha/boot/main.c of Linus Torvalds
 * and the decompression code from MILO.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <generated/utsrelease.h>
#include <linux/mm.h>

#include <asm/console.h>
#include <asm/hwrpb.h>
#include <asm/pgtable.h>
#include <asm/io.h>

#include <stdarg.h>

#include "kzsize.h"

#define MALLOC_AREA_SIZE 0x200000 



#undef DEBUG_CHECK_RANGE
#undef DEBUG_ADDRESSES
#undef DEBUG_LAST_STEPS

extern unsigned long switch_to_osf_pal(unsigned long nr,
	struct pcb_struct * pcb_va, struct pcb_struct * pcb_pa,
	unsigned long *vptb);

extern int decompress_kernel(void* destination, void *source,
			     size_t ksize, size_t kzsize);

extern void move_stack(unsigned long new_stack);

struct hwrpb_struct *hwrpb = INIT_HWRPB;
static struct pcb_struct pcb_va[1];

#define VPTB	((unsigned long *) 0x200000000)

static inline unsigned long
find_pa(unsigned long address)
{
	unsigned long result;

	result = VPTB[address >> 13];
	result >>= 32;
	result <<= 13;
	result |= address & 0x1fff;
	return result;
}	

int
check_range(unsigned long vstart, unsigned long vend,
	    unsigned long kstart, unsigned long kend)
{
	unsigned long vaddr, kaddr;

#ifdef DEBUG_CHECK_RANGE
	srm_printk("check_range: V[0x%lx:0x%lx] K[0x%lx:0x%lx]\n",
		   vstart, vend, kstart, kend);
#endif
	
	for (vaddr = vstart; vaddr <= vend; vaddr += PAGE_SIZE)
	{
		kaddr = (find_pa(vaddr) | PAGE_OFFSET);
		if (kaddr >= kstart && kaddr <= kend)
		{
#ifdef DEBUG_CHECK_RANGE
			srm_printk("OVERLAP: vaddr 0x%lx kaddr 0x%lx"
				   " [0x%lx:0x%lx]\n",
				   vaddr, kaddr, kstart, kend);
#endif
			return 1;
		}
	}
	return 0;
}


#define L1	((unsigned long *) 0x200802000)

void
pal_init(void)
{
	unsigned long i, rev;
	struct percpu_struct * percpu;
	struct pcb_struct * pcb_pa;

	
	pcb_va->ksp = 0;
	pcb_va->usp = 0;
	pcb_va->ptbr = L1[1] >> 32;
	pcb_va->asn = 0;
	pcb_va->pcc = 0;
	pcb_va->unique = 0;
	pcb_va->flags = 1;
	pcb_va->res1 = 0;
	pcb_va->res2 = 0;
	pcb_pa = (struct pcb_struct *)find_pa((unsigned long)pcb_va);

	srm_printk("Switching to OSF PAL-code... ");

	i = switch_to_osf_pal(2, pcb_va, pcb_pa, VPTB);
	if (i) {
		srm_printk("failed, code %ld\n", i);
		__halt();
	}

	percpu = (struct percpu_struct *)
		(INIT_HWRPB->processor_offset + (unsigned long) INIT_HWRPB);
	rev = percpu->pal_revision = percpu->palcode_avail[2];

	srm_printk("OK (rev %lx)\n", rev);

	tbia(); 
}

static inline void
runkernel(void)
{
	__asm__ __volatile__(
		"bis %0,%0,$27\n\t"
		"jmp ($27)"
		: 
		: "r" (START_ADDR));
}

unsigned long SP_on_entry;

extern char _end;
#define KERNEL_ORIGIN \
	((((unsigned long)&_end) + 511) & ~511)

#define NEXT_PAGE(a)	(((a) | (PAGE_SIZE - 1)) + 1)

#ifdef INITRD_IMAGE_SIZE
# define REAL_INITRD_SIZE INITRD_IMAGE_SIZE
#else
# define REAL_INITRD_SIZE 0
#endif

  

#define V_BOOT_IMAGE_START	BOOT_ADDR
#define V_BOOT_IMAGE_END	SP_on_entry

#define V_BOOTSTRAPPER_START	BOOT_ADDR
#define V_BOOTSTRAPPER_END	KERNEL_ORIGIN

#define V_DATA_START		KERNEL_ORIGIN
#define V_INITRD_START		(KERNEL_ORIGIN + KERNEL_Z_SIZE)
#define V_INTRD_END		(V_INITRD_START + REAL_INITRD_SIZE)
#define V_DATA_END	 	V_BOOT_IMAGE_END

#define K_KERNEL_DATA_START	ZERO_PGE
#define K_KERNEL_IMAGE_START	START_ADDR
#define K_KERNEL_IMAGE_END	(START_ADDR + KERNEL_SIZE)

#define K_COPY_IMAGE_START	NEXT_PAGE(K_KERNEL_IMAGE_END)
#define K_INITRD_START \
    NEXT_PAGE(K_COPY_IMAGE_START + KERNEL_SIZE + PAGE_SIZE)
#define K_COPY_IMAGE_END \
    (K_INITRD_START + REAL_INITRD_SIZE + MALLOC_AREA_SIZE)
#define K_COPY_IMAGE_SIZE \
    NEXT_PAGE(K_COPY_IMAGE_END - K_COPY_IMAGE_START)

void
start_kernel(void)
{
	int must_move = 0;

	unsigned long uncompressed_image_start = K_KERNEL_IMAGE_START;
	unsigned long uncompressed_image_end = K_KERNEL_IMAGE_END;

	unsigned long initrd_image_start = K_INITRD_START;

	static long nbytes;
	static char envval[256] __attribute__((aligned(8)));
	register unsigned long asm_sp asm("30");

	SP_on_entry = asm_sp;

	srm_printk("Linux/Alpha BOOTPZ Loader for Linux " UTS_RELEASE "\n");

	
	if (INIT_HWRPB->pagesize != 8192) {
		srm_printk("Expected 8kB pages, got %ldkB\n",
		           INIT_HWRPB->pagesize >> 10);
		return;
	}
	if (INIT_HWRPB->vptb != (unsigned long) VPTB) {
		srm_printk("Expected vptb at %p, got %p\n",
			   VPTB, (void *)INIT_HWRPB->vptb);
		return;
	}

	
	pal_init();

	
	nbytes = callback_getenv(ENV_BOOTED_OSFLAGS, envval, sizeof(envval));
	if (nbytes < 0 || nbytes >= sizeof(envval)) {
		nbytes = 0;
	}
	envval[nbytes] = '\0';

#ifdef DEBUG_ADDRESSES
	srm_printk("START_ADDR 0x%lx\n", START_ADDR);
	srm_printk("KERNEL_ORIGIN 0x%lx\n", KERNEL_ORIGIN);
	srm_printk("KERNEL_SIZE 0x%x\n", KERNEL_SIZE);
	srm_printk("KERNEL_Z_SIZE 0x%x\n", KERNEL_Z_SIZE);
#endif


	if (check_range(V_BOOTSTRAPPER_START, V_BOOTSTRAPPER_END,
			K_KERNEL_DATA_START, K_KERNEL_IMAGE_END))
	{
		srm_printk("FATAL ERROR: overlap of bootstrapper code\n");
		__halt();
	}

	if (check_range(V_DATA_START, V_DATA_END,
			K_KERNEL_IMAGE_START, K_COPY_IMAGE_END))
	{
#ifdef DEBUG_ADDRESSES
		srm_printk("OVERLAP: cannot decompress in place\n");
#endif
		uncompressed_image_start = K_COPY_IMAGE_START;
		uncompressed_image_end = K_COPY_IMAGE_END;
		must_move = 1;

		while (check_range(V_DATA_START, V_DATA_END,
				   uncompressed_image_start,
				   uncompressed_image_end))
		{
#if 0
			uncompressed_image_start += K_COPY_IMAGE_SIZE;
			uncompressed_image_end += K_COPY_IMAGE_SIZE;
			initrd_image_start += K_COPY_IMAGE_SIZE;
#else
			
			uncompressed_image_start += PAGE_SIZE;
			uncompressed_image_end += PAGE_SIZE;
			initrd_image_start += PAGE_SIZE;
#endif
		}
	}

	srm_printk("Starting to load the kernel with args '%s'\n", envval);

#ifdef DEBUG_ADDRESSES
	srm_printk("Decompressing the kernel...\n"
		   "...from 0x%lx to 0x%lx size 0x%x\n",
		   V_DATA_START,
		   uncompressed_image_start,
		   KERNEL_SIZE);
#endif
        decompress_kernel((void *)uncompressed_image_start,
			  (void *)V_DATA_START,
			  KERNEL_SIZE, KERNEL_Z_SIZE);


#ifdef INITRD_IMAGE_SIZE

	
#ifdef DEBUG_ADDRESSES
	srm_printk("Moving the INITRD image...\n"
		   " from 0x%lx to 0x%lx size 0x%x\n",
		   V_INITRD_START,
		   initrd_image_start,
		   INITRD_IMAGE_SIZE);
#endif
	memcpy((void *)initrd_image_start, (void *)V_INITRD_START,
	       INITRD_IMAGE_SIZE);

#endif 

	if (must_move) {
#ifdef DEBUG_ADDRESSES
		srm_printk("Moving the uncompressed kernel...\n"
			   "...from 0x%lx to 0x%lx size 0x%x\n",
			   uncompressed_image_start,
			   K_KERNEL_IMAGE_START,
			   (unsigned)KERNEL_SIZE);
#endif
		/*
		 * Move the stack to a safe place to ensure it won't be
		 * overwritten by kernel image.
		 */
		move_stack(initrd_image_start - PAGE_SIZE);

		memcpy((void *)K_KERNEL_IMAGE_START,
		       (void *)uncompressed_image_start, KERNEL_SIZE);
	}
	
	
#ifdef DEBUG_LAST_STEPS
	srm_printk("Preparing ZERO_PGE...\n");
#endif
	memset((char*)ZERO_PGE, 0, PAGE_SIZE);
	strcpy((char*)ZERO_PGE, envval);

#ifdef INITRD_IMAGE_SIZE

#ifdef DEBUG_LAST_STEPS
	srm_printk("Preparing INITRD info...\n");
#endif
	
	((long *)(ZERO_PGE+256))[0] = initrd_image_start;
	((long *)(ZERO_PGE+256))[1] = INITRD_IMAGE_SIZE;

#endif 

#ifdef DEBUG_LAST_STEPS
	srm_printk("Doing 'runkernel()'...\n");
#endif
	runkernel();
}

 
void *__kmalloc(size_t size, gfp_t flags)
{
	return (void *)NULL;
}
