/* asm/floppy.h: Sparc specific parts of the Floppy driver.
 *
 * Copyright (C) 1995 David S. Miller (davem@davemloft.net)
 */

#ifndef __ASM_SPARC_FLOPPY_H
#define __ASM_SPARC_FLOPPY_H

#include <linux/of.h>
#include <linux/of_device.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/idprom.h>
#include <asm/machines.h>
#include <asm/oplib.h>
#include <asm/auxio.h>
#include <asm/irq.h>

#undef release_region
#undef request_region
#define release_region(X, Y)	do { } while(0)
#define request_region(X, Y, Z)	(1)

struct sun_flpy_controller {
	volatile unsigned char status_82072;  
#define dcr_82072              status_82072   
#define status1_82077          status_82072   

	volatile unsigned char data_82072;    
#define status2_82077          data_82072     

	volatile unsigned char dor_82077;     
	volatile unsigned char tapectl_82077; 

	volatile unsigned char status_82077;  
#define drs_82077              status_82077   

	volatile unsigned char data_82077;    
	volatile unsigned char ___unused;
	volatile unsigned char dir_82077;     
#define dcr_82077              dir_82077      
};

static struct sun_flpy_controller *sun_fdc = NULL;
extern volatile unsigned char *fdc_status;

struct sun_floppy_ops {
	unsigned char (*fd_inb)(int port);
	void (*fd_outb)(unsigned char value, int port);
};

static struct sun_floppy_ops sun_fdops;

#define fd_inb(port)              sun_fdops.fd_inb(port)
#define fd_outb(value,port)       sun_fdops.fd_outb(value,port)
#define fd_enable_dma()           sun_fd_enable_dma()
#define fd_disable_dma()          sun_fd_disable_dma()
#define fd_request_dma()          (0) 
#define fd_free_dma()             
#define fd_clear_dma_ff()         
#define fd_set_dma_mode(mode)     sun_fd_set_dma_mode(mode)
#define fd_set_dma_addr(addr)     sun_fd_set_dma_addr(addr)
#define fd_set_dma_count(count)   sun_fd_set_dma_count(count)
#define fd_enable_irq()           
#define fd_disable_irq()          
#define fd_cacheflush(addr, size) 
#define fd_request_irq()          sun_fd_request_irq()
#define fd_free_irq()             
#if 0  
#define fd_dma_mem_alloc(size)    ((unsigned long) vmalloc(size))
#define fd_dma_mem_free(addr,size) (vfree((void *)(addr)))
#endif

#define get_dma_residue(x)        (0)

#define FLOPPY0_TYPE  4
#define FLOPPY1_TYPE  0

#undef HAVE_DISABLE_HLT

#define FDC1                      sun_floppy_init()

#define N_FDC    1
#define N_DRIVE  8

#define CROSS_64KB(a,s) (0)

static void sun_set_dor(unsigned char value, int fdc_82077)
{
	if (sparc_cpu_model == sun4c) {
		unsigned int bits = 0;
		if (value & 0x10)
			bits |= AUXIO_FLPY_DSEL;
		if ((value & 0x80) == 0)
			bits |= AUXIO_FLPY_EJCT;
		set_auxio(bits, (~bits) & (AUXIO_FLPY_DSEL|AUXIO_FLPY_EJCT));
	}
	if (fdc_82077) {
		sun_fdc->dor_82077 = value;
	}
}

static unsigned char sun_read_dir(void)
{
	if (sparc_cpu_model == sun4c)
		return (get_auxio() & AUXIO_FLPY_DCHG) ? 0x80 : 0;
	else
		return sun_fdc->dir_82077;
}

static unsigned char sun_82072_fd_inb(int port)
{
	udelay(5);
	switch(port & 7) {
	default:
		printk("floppy: Asked to read unknown port %d\n", port);
		panic("floppy: Port bolixed.");
	case 4: 
		return sun_fdc->status_82072 & ~STATUS_DMA;
	case 5: 
		return sun_fdc->data_82072;
	case 7: 
		return sun_read_dir();
	}
	panic("sun_82072_fd_inb: How did I get here?");
}

static void sun_82072_fd_outb(unsigned char value, int port)
{
	udelay(5);
	switch(port & 7) {
	default:
		printk("floppy: Asked to write to unknown port %d\n", port);
		panic("floppy: Port bolixed.");
	case 2: 
		sun_set_dor(value, 0);
		break;
	case 5: 
		sun_fdc->data_82072 = value;
		break;
	case 7: 
		sun_fdc->dcr_82072 = value;
		break;
	case 4: 
		sun_fdc->status_82072 = value;
		break;
	}
	return;
}

static unsigned char sun_82077_fd_inb(int port)
{
	udelay(5);
	switch(port & 7) {
	default:
		printk("floppy: Asked to read unknown port %d\n", port);
		panic("floppy: Port bolixed.");
	case 0: 
		return sun_fdc->status1_82077;
	case 1: 
		return sun_fdc->status2_82077;
	case 2: 
		return sun_fdc->dor_82077;
	case 3: 
		return sun_fdc->tapectl_82077;
	case 4: 
		return sun_fdc->status_82077 & ~STATUS_DMA;
	case 5: 
		return sun_fdc->data_82077;
	case 7: 
		return sun_read_dir();
	}
	panic("sun_82077_fd_inb: How did I get here?");
}

static void sun_82077_fd_outb(unsigned char value, int port)
{
	udelay(5);
	switch(port & 7) {
	default:
		printk("floppy: Asked to write to unknown port %d\n", port);
		panic("floppy: Port bolixed.");
	case 2: 
		sun_set_dor(value, 1);
		break;
	case 5: 
		sun_fdc->data_82077 = value;
		break;
	case 7: 
		sun_fdc->dcr_82077 = value;
		break;
	case 4: 
		sun_fdc->status_82077 = value;
		break;
	case 3: 
		sun_fdc->tapectl_82077 = value;
		break;
	}
	return;
}

extern char *pdma_vaddr;
extern unsigned long pdma_size;
extern volatile int doing_pdma;

extern char *pdma_base;
extern unsigned long pdma_areasize;

static inline void virtual_dma_init(void)
{
	
}

static inline void sun_fd_disable_dma(void)
{
	doing_pdma = 0;
	if (pdma_base) {
		mmu_unlockarea(pdma_base, pdma_areasize);
		pdma_base = NULL;
	}
}

static inline void sun_fd_set_dma_mode(int mode)
{
	switch(mode) {
	case DMA_MODE_READ:
		doing_pdma = 1;
		break;
	case DMA_MODE_WRITE:
		doing_pdma = 2;
		break;
	default:
		printk("Unknown dma mode %d\n", mode);
		panic("floppy: Giving up...");
	}
}

static inline void sun_fd_set_dma_addr(char *buffer)
{
	pdma_vaddr = buffer;
}

static inline void sun_fd_set_dma_count(int length)
{
	pdma_size = length;
}

static inline void sun_fd_enable_dma(void)
{
	pdma_vaddr = mmu_lockarea(pdma_vaddr, pdma_size);
	pdma_base = pdma_vaddr;
	pdma_areasize = pdma_size;
}

extern int sparc_floppy_request_irq(unsigned int irq,
                                    irq_handler_t irq_handler);

static int sun_fd_request_irq(void)
{
	static int once = 0;

	if (!once) {
		once = 1;
		return sparc_floppy_request_irq(FLOPPY_IRQ, floppy_interrupt);
	} else {
		return 0;
	}
}

static struct linux_prom_registers fd_regs[2];

static int sun_floppy_init(void)
{
	struct platform_device *op;
	struct device_node *dp;
	char state[128];
	phandle tnode, fd_node;
	int num_regs;
	struct resource r;

	use_virtual_dma = 1;

	if((sparc_cpu_model != sun4c && sparc_cpu_model != sun4m) ||
	   ((idprom->id_machtype == (SM_SUN4C | SM_4C_SLC)) ||
	    (idprom->id_machtype == (SM_SUN4C | SM_4C_ELC)))) {
		
		goto no_sun_fdc;
	}
	
	tnode = prom_getchild(prom_root_node);
	fd_node = prom_searchsiblings(tnode, "obio");
	if(fd_node != 0) {
		tnode = prom_getchild(fd_node);
		fd_node = prom_searchsiblings(tnode, "SUNW,fdtwo");
	} else {
		fd_node = prom_searchsiblings(tnode, "fd");
	}
	if(fd_node == 0) {
		goto no_sun_fdc;
	}

	
	if(sparc_cpu_model == sun4m &&
	   prom_getproperty(fd_node, "status", state, sizeof(state)) != -1) {
		if(!strcmp(state, "disabled")) {
			goto no_sun_fdc;
		}
	}
	num_regs = prom_getproperty(fd_node, "reg", (char *) fd_regs, sizeof(fd_regs));
	num_regs = (num_regs / sizeof(fd_regs[0]));
	prom_apply_obio_ranges(fd_regs, num_regs);
	memset(&r, 0, sizeof(r));
	r.flags = fd_regs[0].which_io;
	r.start = fd_regs[0].phys_addr;
	sun_fdc = (struct sun_flpy_controller *)
	    of_ioremap(&r, 0, fd_regs[0].reg_size, "floppy");

	for_each_node_by_name(dp, "SUNW,fdtwo") {
		op = of_find_device_by_node(dp);
		if (op)
			break;
	}
	if (!op) {
		for_each_node_by_name(dp, "fd") {
			op = of_find_device_by_node(dp);
			if (op)
				break;
		}
	}
	if (!op)
		goto no_sun_fdc;

	FLOPPY_IRQ = op->archdata.irqs[0];

	
	if(sun_fdc->status_82072 == 0xff) {
		sun_fdc = NULL;
		goto no_sun_fdc;
	}

	sun_fdops.fd_inb = sun_82077_fd_inb;
	sun_fdops.fd_outb = sun_82077_fd_outb;
	fdc_status = &sun_fdc->status_82077;

	if (sun_fdc->dor_82077 == 0x80) {
		sun_fdc->dor_82077 = 0x02;
		if (sun_fdc->dor_82077 == 0x80) {
			sun_fdops.fd_inb = sun_82072_fd_inb;
			sun_fdops.fd_outb = sun_82072_fd_outb;
			fdc_status = &sun_fdc->status_82072;
		}
	}

	
	allowed_drive_mask = 0x01;
	return (int) sun_fdc;

no_sun_fdc:
	return -1;
}

static int sparc_eject(void)
{
	set_dor(0x00, 0xff, 0x90);
	udelay(500);
	set_dor(0x00, 0x6f, 0x00);
	udelay(500);
	return 0;
}

#define fd_eject(drive) sparc_eject()

#define EXTRA_FLOPPY_PARAMS

static DEFINE_SPINLOCK(dma_spin_lock);

#define claim_dma_lock() \
({	unsigned long flags; \
	spin_lock_irqsave(&dma_spin_lock, flags); \
	flags; \
})

#define release_dma_lock(__flags) \
	spin_unlock_irqrestore(&dma_spin_lock, __flags);

#endif 
