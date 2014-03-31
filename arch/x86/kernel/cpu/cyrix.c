#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <asm/dma.h>
#include <linux/io.h>
#include <asm/processor-cyrix.h>
#include <asm/processor-flags.h>
#include <linux/timer.h>
#include <asm/pci-direct.h>
#include <asm/tsc.h>

#include "cpu.h"

static void __cpuinit __do_cyrix_devid(unsigned char *dir0, unsigned char *dir1)
{
	unsigned char ccr2, ccr3;

	
	ccr3 = getCx86(CX86_CCR3);
	setCx86(CX86_CCR3, ccr3 ^ 0x80);
	getCx86(0xc0);   

	if (getCx86(CX86_CCR3) == ccr3) {       
		ccr2 = getCx86(CX86_CCR2);
		setCx86(CX86_CCR2, ccr2 ^ 0x04);
		getCx86(0xc0);  

		if (getCx86(CX86_CCR2) == ccr2) 
			*dir0 = 0xfd;
		else {                          
			setCx86(CX86_CCR2, ccr2);
			*dir0 = 0xfe;
		}
	} else {
		setCx86(CX86_CCR3, ccr3);  

		
		*dir0 = getCx86(CX86_DIR0);
		*dir1 = getCx86(CX86_DIR1);
	}
}

static void __cpuinit do_cyrix_devid(unsigned char *dir0, unsigned char *dir1)
{
	unsigned long flags;

	local_irq_save(flags);
	__do_cyrix_devid(dir0, dir1);
	local_irq_restore(flags);
}
static unsigned char Cx86_dir0_msb __cpuinitdata = 0;

static const char __cpuinitconst Cx86_model[][9] = {
	"Cx486", "Cx486", "5x86 ", "6x86", "MediaGX ", "6x86MX ",
	"M II ", "Unknown"
};
static const char __cpuinitconst Cx486_name[][5] = {
	"SLC", "DLC", "SLC2", "DLC2", "SRx", "DRx",
	"SRx2", "DRx2"
};
static const char __cpuinitconst Cx486S_name[][4] = {
	"S", "S2", "Se", "S2e"
};
static const char __cpuinitconst Cx486D_name[][4] = {
	"DX", "DX2", "?", "?", "?", "DX4"
};
static char Cx86_cb[] __cpuinitdata = "?.5x Core/Bus Clock";
static const char __cpuinitconst cyrix_model_mult1[] = "12??43";
static const char __cpuinitconst cyrix_model_mult2[] = "12233445";


static void __cpuinit check_cx686_slop(struct cpuinfo_x86 *c)
{
	unsigned long flags;

	if (Cx86_dir0_msb == 3) {
		unsigned char ccr3, ccr5;

		local_irq_save(flags);
		ccr3 = getCx86(CX86_CCR3);
		setCx86(CX86_CCR3, (ccr3 & 0x0f) | 0x10); 
		ccr5 = getCx86(CX86_CCR5);
		if (ccr5 & 2)
			setCx86(CX86_CCR5, ccr5 & 0xfd);  
		setCx86(CX86_CCR3, ccr3);                 
		local_irq_restore(flags);

		if (ccr5 & 2) { 
			printk(KERN_INFO "Recalibrating delay loop with SLOP bit reset\n");
			calibrate_delay();
			c->loops_per_jiffy = loops_per_jiffy;
		}
	}
}


static void __cpuinit set_cx86_reorder(void)
{
	u8 ccr3;

	printk(KERN_INFO "Enable Memory access reorder on Cyrix/NSC processor.\n");
	ccr3 = getCx86(CX86_CCR3);
	setCx86(CX86_CCR3, (ccr3 & 0x0f) | 0x10); 

	
	setCx86_old(CX86_PCR0, getCx86_old(CX86_PCR0) & ~0x80);
	
	ccr3 |= 0xe0;
	setCx86(CX86_CCR3, ccr3);
}

static void __cpuinit set_cx86_memwb(void)
{
	printk(KERN_INFO "Enable Memory-Write-back mode on Cyrix/NSC processor.\n");

	
	setCx86_old(CX86_CCR2, getCx86_old(CX86_CCR2) & ~0x04);
	
	write_cr0(read_cr0() | X86_CR0_NW);
	
	setCx86_old(CX86_CCR2, getCx86_old(CX86_CCR2) | 0x14);
}


static void __cpuinit geode_configure(void)
{
	unsigned long flags;
	u8 ccr3;
	local_irq_save(flags);

	
	setCx86_old(CX86_CCR2, getCx86_old(CX86_CCR2) | 0x88);

	ccr3 = getCx86(CX86_CCR3);
	setCx86(CX86_CCR3, (ccr3 & 0x0f) | 0x10);	


	
	setCx86_old(CX86_CCR4, getCx86_old(CX86_CCR4) | 0x38);
	setCx86(CX86_CCR3, ccr3);			

	set_cx86_memwb();
	set_cx86_reorder();

	local_irq_restore(flags);
}

static void __cpuinit early_init_cyrix(struct cpuinfo_x86 *c)
{
	unsigned char dir0, dir0_msn, dir1 = 0;

	__do_cyrix_devid(&dir0, &dir1);
	dir0_msn = dir0 >> 4; 

	switch (dir0_msn) {
	case 3: 
		
		set_cpu_cap(c, X86_FEATURE_CYRIX_ARR);
		break;
	case 5: 
		
		set_cpu_cap(c, X86_FEATURE_CYRIX_ARR);
		break;
	}
}

static void __cpuinit init_cyrix(struct cpuinfo_x86 *c)
{
	unsigned char dir0, dir0_msn, dir0_lsn, dir1 = 0;
	char *buf = c->x86_model_id;
	const char *p = NULL;

	clear_cpu_cap(c, 0*32+31);

	
	if (test_cpu_cap(c, 1*32+24)) {
		clear_cpu_cap(c, 1*32+24);
		set_cpu_cap(c, X86_FEATURE_CXMMX);
	}

	do_cyrix_devid(&dir0, &dir1);

	check_cx686_slop(c);

	Cx86_dir0_msb = dir0_msn = dir0 >> 4; 
	dir0_lsn = dir0 & 0xf;                

	
	c->x86_model = (dir1 >> 4) + 1;
	c->x86_mask = dir1 & 0xf;


	switch (dir0_msn) {
		unsigned char tmp;

	case 0: 
		p = Cx486_name[dir0_lsn & 7];
		break;

	case 1: 
		p = (dir0_lsn & 8) ? Cx486D_name[dir0_lsn & 5]
			: Cx486S_name[dir0_lsn & 3];
		break;

	case 2: 
		Cx86_cb[2] = cyrix_model_mult1[dir0_lsn & 5];
		p = Cx86_cb+2;
		break;

	case 3: 
		Cx86_cb[1] = ' ';
		Cx86_cb[2] = cyrix_model_mult1[dir0_lsn & 5];
		if (dir1 > 0x21) { 
			Cx86_cb[0] = 'L';
			p = Cx86_cb;
			(c->x86_model)++;
		} else             
			p = Cx86_cb+1;
		
		set_cpu_cap(c, X86_FEATURE_CYRIX_ARR);
		
		c->coma_bug = 1;
		break;

	case 4: 
#ifdef CONFIG_PCI
	{
		u32 vendor, device;

		printk(KERN_INFO "Working around Cyrix MediaGX virtual DMA bugs.\n");
		isa_dma_bridge_buggy = 2;

		vendor = read_pci_config_16(0, 0, 0x12, PCI_VENDOR_ID);
		device = read_pci_config_16(0, 0, 0x12, PCI_DEVICE_ID);

		if (vendor == PCI_VENDOR_ID_CYRIX &&
			(device == PCI_DEVICE_ID_CYRIX_5510 ||
					device == PCI_DEVICE_ID_CYRIX_5520))
			mark_tsc_unstable("cyrix 5510/5520 detected");
	}
#endif
		c->x86_cache_size = 16;	

		
		if (c->cpuid_level == 2) {
			
			setCx86_old(CX86_CCR7, getCx86_old(CX86_CCR7) | 1);

			if ((0x30 <= dir1 && dir1 <= 0x6f) ||
					(0x80 <= dir1 && dir1 <= 0x8f))
				geode_configure();
			return;
		} else { 
			Cx86_cb[2] = (dir0_lsn & 1) ? '3' : '4';
			p = Cx86_cb+2;
			c->x86_model = (dir1 & 0x20) ? 1 : 2;
		}
		break;

	case 5: 
		if (dir1 > 7) {
			dir0_msn++;  
			
			setCx86_old(CX86_CCR7, getCx86_old(CX86_CCR7)|1);
		} else {
			c->coma_bug = 1;      
		}
		tmp = (!(dir0_lsn & 7) || dir0_lsn & 1) ? 2 : 0;
		Cx86_cb[tmp] = cyrix_model_mult2[dir0_lsn & 7];
		p = Cx86_cb+tmp;
		if (((dir1 & 0x0f) > 4) || ((dir1 & 0xf0) == 0x20))
			(c->x86_model)++;
		
		set_cpu_cap(c, X86_FEATURE_CYRIX_ARR);
		break;

	case 0xf:  
		switch (dir0_lsn) {
		case 0xd:  
			dir0_msn = 0;
			p = Cx486_name[(c->hard_math) ? 1 : 0];
			break;

		case 0xe:  
			dir0_msn = 0;
			p = Cx486S_name[0];
			break;
		}
		break;

	default:  
		dir0_msn = 7;
		break;
	}
	strcpy(buf, Cx86_model[dir0_msn & 7]);
	if (p)
		strcat(buf, p);
	return;
}

static void __cpuinit init_nsc(struct cpuinfo_x86 *c)
{

	

	if (c->x86 == 5 && c->x86_model == 5)
		cpu_detect_cache_sizes(c);
	else
		init_cyrix(c);
}


static inline int test_cyrix_52div(void)
{
	unsigned int test;

	__asm__ __volatile__(
	     "sahf\n\t"		
	     "div %b2\n\t"	
	     "lahf"		
	     : "=a" (test)
	     : "0" (5), "q" (2)
	     : "cc");

	
	return (unsigned char) (test >> 8) == 0x02;
}

static void __cpuinit cyrix_identify(struct cpuinfo_x86 *c)
{
	
	if (c->x86 == 4 && test_cyrix_52div()) {
		unsigned char dir0, dir1;

		strcpy(c->x86_vendor_id, "CyrixInstead");
		c->x86_vendor = X86_VENDOR_CYRIX;

		

		

		do_cyrix_devid(&dir0, &dir1);

		dir0 >>= 4;

		

		if (dir0 == 5 || dir0 == 3) {
			unsigned char ccr3;
			unsigned long flags;
			printk(KERN_INFO "Enabling CPUID on Cyrix processor.\n");
			local_irq_save(flags);
			ccr3 = getCx86(CX86_CCR3);
			
			setCx86(CX86_CCR3, (ccr3 & 0x0f) | 0x10);
			
			setCx86_old(CX86_CCR4, getCx86_old(CX86_CCR4) | 0x80);
			
			setCx86(CX86_CCR3, ccr3);
			local_irq_restore(flags);
		}
	}
}

static const struct cpu_dev __cpuinitconst cyrix_cpu_dev = {
	.c_vendor	= "Cyrix",
	.c_ident	= { "CyrixInstead" },
	.c_early_init	= early_init_cyrix,
	.c_init		= init_cyrix,
	.c_identify	= cyrix_identify,
	.c_x86_vendor	= X86_VENDOR_CYRIX,
};

cpu_dev_register(cyrix_cpu_dev);

static const struct cpu_dev __cpuinitconst nsc_cpu_dev = {
	.c_vendor	= "NSC",
	.c_ident	= { "Geode by NSC" },
	.c_init		= init_nsc,
	.c_x86_vendor	= X86_VENDOR_NSC,
};

cpu_dev_register(nsc_cpu_dev);
