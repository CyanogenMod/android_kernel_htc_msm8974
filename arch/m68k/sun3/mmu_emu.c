
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/bitops.h>
#include <linux/module.h>

#include <asm/setup.h>
#include <asm/traps.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/sun3mmu.h>
#include <asm/segment.h>
#include <asm/oplib.h>
#include <asm/mmu_context.h>
#include <asm/dvma.h>


#undef DEBUG_MMU_EMU
#define DEBUG_PROM_MAPS


#define CONTEXTS_NUM		8
#define SEGMAPS_PER_CONTEXT_NUM 2048
#define PAGES_PER_SEGMENT	16
#define PMEGS_NUM		256
#define PMEG_MASK		0xFF


unsigned long m68k_vmalloc_end;
EXPORT_SYMBOL(m68k_vmalloc_end);

unsigned long pmeg_vaddr[PMEGS_NUM];
unsigned char pmeg_alloc[PMEGS_NUM];
unsigned char pmeg_ctx[PMEGS_NUM];

static struct mm_struct *ctx_alloc[CONTEXTS_NUM] = {
    [0] = (struct mm_struct *)0xffffffff
};

static unsigned char ctx_avail = CONTEXTS_NUM-1;

unsigned long rom_pages[256];

void print_pte (pte_t pte)
{
#if 0
	
	unsigned long val = pte_val (pte);
	printk (" pte=%lx [addr=%lx",
		val, (val & SUN3_PAGE_PGNUM_MASK) << PAGE_SHIFT);
	if (val & SUN3_PAGE_VALID)	printk (" valid");
	if (val & SUN3_PAGE_WRITEABLE)	printk (" write");
	if (val & SUN3_PAGE_SYSTEM)	printk (" sys");
	if (val & SUN3_PAGE_NOCACHE)	printk (" nocache");
	if (val & SUN3_PAGE_ACCESSED)	printk (" accessed");
	if (val & SUN3_PAGE_MODIFIED)	printk (" modified");
	switch (val & SUN3_PAGE_TYPE_MASK) {
		case SUN3_PAGE_TYPE_MEMORY: printk (" memory"); break;
		case SUN3_PAGE_TYPE_IO:     printk (" io");     break;
		case SUN3_PAGE_TYPE_VME16:  printk (" vme16");  break;
		case SUN3_PAGE_TYPE_VME32:  printk (" vme32");  break;
	}
	printk ("]\n");
#else
	
	unsigned long val = pte_val (pte);
	char flags[7], *type;

	flags[0] = (val & SUN3_PAGE_VALID)     ? 'v' : '-';
	flags[1] = (val & SUN3_PAGE_WRITEABLE) ? 'w' : '-';
	flags[2] = (val & SUN3_PAGE_SYSTEM)    ? 's' : '-';
	flags[3] = (val & SUN3_PAGE_NOCACHE)   ? 'x' : '-';
	flags[4] = (val & SUN3_PAGE_ACCESSED)  ? 'a' : '-';
	flags[5] = (val & SUN3_PAGE_MODIFIED)  ? 'm' : '-';
	flags[6] = '\0';

	switch (val & SUN3_PAGE_TYPE_MASK) {
		case SUN3_PAGE_TYPE_MEMORY: type = "memory"; break;
		case SUN3_PAGE_TYPE_IO:     type = "io"    ; break;
		case SUN3_PAGE_TYPE_VME16:  type = "vme16" ; break;
		case SUN3_PAGE_TYPE_VME32:  type = "vme32" ; break;
		default: type = "unknown?"; break;
	}

	printk (" pte=%08lx [%07lx %s %s]\n",
		val, (val & SUN3_PAGE_PGNUM_MASK) << PAGE_SHIFT, flags, type);
#endif
}

void print_pte_vaddr (unsigned long vaddr)
{
	printk (" vaddr=%lx [%02lx]", vaddr, sun3_get_segmap (vaddr));
	print_pte (__pte (sun3_get_pte (vaddr)));
}

void mmu_emu_init(unsigned long bootmem_end)
{
	unsigned long seg, num;
	int i,j;

	memset(rom_pages, 0, sizeof(rom_pages));
	memset(pmeg_vaddr, 0, sizeof(pmeg_vaddr));
	memset(pmeg_alloc, 0, sizeof(pmeg_alloc));
	memset(pmeg_ctx, 0, sizeof(pmeg_ctx));

	bootmem_end = (bootmem_end + (2 * SUN3_PMEG_SIZE)) & ~SUN3_PMEG_MASK;

	
	for (i=0; i < __pa(bootmem_end) / SUN3_PMEG_SIZE ; ++i)
		pmeg_alloc[i] = 2;


	for(num = 0xf0; num <= 0xff; num++)
		pmeg_alloc[num] = 2;

	
	for(seg = bootmem_end; seg < 0x0f800000; seg += SUN3_PMEG_SIZE) {
		i = sun3_get_segmap(seg);

		if(!pmeg_alloc[i]) {
#ifdef DEBUG_MMU_EMU
			printk("freed: ");
			print_pte_vaddr (seg);
#endif
			sun3_put_segmap(seg, SUN3_INVALID_PMEG);
		}
	}

	j = 0;
	for (num=0, seg=0x0F800000; seg<0x10000000; seg+=16*PAGE_SIZE) {
		if (sun3_get_segmap (seg) != SUN3_INVALID_PMEG) {
#ifdef DEBUG_PROM_MAPS
			for(i = 0; i < 16; i++) {
				printk ("mapped:");
				print_pte_vaddr (seg + (i*PAGE_SIZE));
				break;
			}
#endif
			
			
			if (!m68k_vmalloc_end)
				m68k_vmalloc_end = seg;

			
			
			
			pmeg_alloc[sun3_get_segmap(seg)] = 2;
		}
	}

	dvma_init();


	for(seg = 0; seg < PAGE_OFFSET; seg += SUN3_PMEG_SIZE)
		sun3_put_segmap(seg, SUN3_INVALID_PMEG);

	set_fs(MAKE_MM_SEG(3));
	for(seg = 0; seg < 0x10000000; seg += SUN3_PMEG_SIZE) {
		i = sun3_get_segmap(seg);
		for(j = 1; j < CONTEXTS_NUM; j++)
			(*(romvec->pv_setctxt))(j, (void *)seg, i);
	}
	set_fs(KERNEL_DS);

}

void clear_context(unsigned long context)
{
     unsigned char oldctx;
     unsigned long i;

     if(context) {
	     if(!ctx_alloc[context])
		     panic("clear_context: context not allocated\n");

	     ctx_alloc[context]->context = SUN3_INVALID_CONTEXT;
	     ctx_alloc[context] = (struct mm_struct *)0;
	     ctx_avail++;
     }

     oldctx = sun3_get_context();

     sun3_put_context(context);

     for(i = 0; i < SUN3_INVALID_PMEG; i++) {
	     if((pmeg_ctx[i] == context) && (pmeg_alloc[i] == 1)) {
		     sun3_put_segmap(pmeg_vaddr[i], SUN3_INVALID_PMEG);
		     pmeg_ctx[i] = 0;
		     pmeg_alloc[i] = 0;
		     pmeg_vaddr[i] = 0;
	     }
     }

     sun3_put_context(oldctx);
}

unsigned long get_free_context(struct mm_struct *mm)
{
	unsigned long new = 1;
	static unsigned char next_to_die = 1;

	if(!ctx_avail) {
		
		new = next_to_die;
		clear_context(new);
		next_to_die = (next_to_die + 1) & 0x7;
		if(!next_to_die)
			next_to_die++;
	} else {
		while(new < CONTEXTS_NUM) {
			if(ctx_alloc[new])
				new++;
			else
				break;
		}
		
		if(new == CONTEXTS_NUM)
			panic("get_free_context: failed to find free context");
	}

	ctx_alloc[new] = mm;
	ctx_avail--;

	return new;
}


inline void mmu_emu_map_pmeg (int context, int vaddr)
{
	static unsigned char curr_pmeg = 128;
	int i;

	
	vaddr &= ~SUN3_PMEG_MASK;

	
	while (pmeg_alloc[curr_pmeg] == 2)
		++curr_pmeg;


#ifdef DEBUG_MMU_EMU
printk("mmu_emu_map_pmeg: pmeg %x to context %d vaddr %x\n",
       curr_pmeg, context, vaddr);
#endif

	
	if (pmeg_alloc[curr_pmeg] == 1) {
		sun3_put_context(pmeg_ctx[curr_pmeg]);
		sun3_put_segmap (pmeg_vaddr[curr_pmeg], SUN3_INVALID_PMEG);
		sun3_put_context(context);
	}

	
	
	if(vaddr >= PAGE_OFFSET) {
		
		unsigned char i;

		for(i = 0; i < CONTEXTS_NUM; i++) {
			sun3_put_context(i);
			sun3_put_segmap (vaddr, curr_pmeg);
		}
		sun3_put_context(context);
		pmeg_alloc[curr_pmeg] = 2;
		pmeg_ctx[curr_pmeg] = 0;

	}
	else {
		pmeg_alloc[curr_pmeg] = 1;
		pmeg_ctx[curr_pmeg] = context;
		sun3_put_segmap (vaddr, curr_pmeg);

	}
	pmeg_vaddr[curr_pmeg] = vaddr;

	
	for (i=0; i<SUN3_PMEG_SIZE; i+=SUN3_PTE_SIZE)
		sun3_put_pte (vaddr + i, SUN3_PAGE_SYSTEM);

	
	++curr_pmeg;
}



int mmu_emu_handle_fault (unsigned long vaddr, int read_flag, int kernel_fault)
{
	unsigned long segment, offset;
	unsigned char context;
	pte_t *pte;
	pgd_t * crp;

	if(current->mm == NULL) {
		crp = swapper_pg_dir;
		context = 0;
	} else {
		context = current->mm->context;
		if(kernel_fault)
			crp = swapper_pg_dir;
		else
			crp = current->mm->pgd;
	}

#ifdef DEBUG_MMU_EMU
	printk ("mmu_emu_handle_fault: vaddr=%lx type=%s crp=%p\n",
		vaddr, read_flag ? "read" : "write", crp);
#endif

	segment = (vaddr >> SUN3_PMEG_SIZE_BITS) & 0x7FF;
	offset  = (vaddr >> SUN3_PTE_SIZE_BITS) & 0xF;

#ifdef DEBUG_MMU_EMU
	printk ("mmu_emu_handle_fault: segment=%lx offset=%lx\n", segment, offset);
#endif

	pte = (pte_t *) pgd_val (*(crp + segment));

	if (!pte) {
                return 0;
        }

	pte = (pte_t *) __va ((unsigned long)(pte + offset));

	
	if (!(pte_val (*pte) & SUN3_PAGE_VALID))
		return 0;

	
	if (sun3_get_segmap (vaddr&~SUN3_PMEG_MASK) == SUN3_INVALID_PMEG)
		mmu_emu_map_pmeg (context, vaddr);

	
	sun3_put_pte (vaddr&PAGE_MASK, pte_val (*pte));

	
	if (!read_flag) {
		if (pte_val (*pte) & SUN3_PAGE_WRITEABLE)
			pte_val (*pte) |= (SUN3_PAGE_ACCESSED
					   | SUN3_PAGE_MODIFIED);
		else
			return 0;	
	} else
		pte_val (*pte) |= SUN3_PAGE_ACCESSED;

#ifdef DEBUG_MMU_EMU
	printk ("seg:%d crp:%p ->", get_fs().seg, crp);
	print_pte_vaddr (vaddr);
	printk ("\n");
#endif

	return 1;
}
