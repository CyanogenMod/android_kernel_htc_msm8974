
#define __EXTERN_INLINE inline
#include <asm/io.h>
#include <asm/core_polaris.h>
#undef __EXTERN_INLINE

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/ptrace.h>

#include "proto.h"
#include "pci_impl.h"


#define DEBUG_CONFIG 0

#if DEBUG_CONFIG
# define DBG_CFG(args)	printk args
#else
# define DBG_CFG(args)
#endif



static int
mk_conf_addr(struct pci_bus *pbus, unsigned int device_fn, int where,
	     unsigned long *pci_addr, u8 *type1)
{
	u8 bus = pbus->number;

	*type1 = (bus == 0) ? 0 : 1;
	*pci_addr = (bus << 16) | (device_fn << 8) | (where) |
		    POLARIS_DENSE_CONFIG_BASE;

        DBG_CFG(("mk_conf_addr(bus=%d ,device_fn=0x%x, where=0x%x,"
                 " returning address 0x%p\n"
                 bus, device_fn, where, *pci_addr));

	return 0;
}

static int
polaris_read_config(struct pci_bus *bus, unsigned int devfn, int where,
		    int size, u32 *value)
{
	unsigned long addr;
	unsigned char type1;

	if (mk_conf_addr(bus, devfn, where, &addr, &type1))
                return PCIBIOS_DEVICE_NOT_FOUND;

	switch (size) {
	case 1:
		*value = __kernel_ldbu(*(vucp)addr);
		break;
	case 2:
		*value = __kernel_ldwu(*(vusp)addr);
		break;
	case 4:
		*value = *(vuip)addr;
		break;
	}

	return PCIBIOS_SUCCESSFUL;
}


static int 
polaris_write_config(struct pci_bus *bus, unsigned int devfn, int where,
		     int size, u32 value)
{
	unsigned long addr;
	unsigned char type1;

	if (mk_conf_addr(bus, devfn, where, &addr, &type1))
                return PCIBIOS_DEVICE_NOT_FOUND;

	switch (size) {
	case 1:
		__kernel_stb(value, *(vucp)addr);
		mb();
		__kernel_ldbu(*(vucp)addr);
		break;
	case 2:
		__kernel_stw(value, *(vusp)addr);
		mb();
		__kernel_ldwu(*(vusp)addr);
		break;
	case 4:
		*(vuip)addr = value;
		mb();
		*(vuip)addr;
		break;
	}

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops polaris_pci_ops = 
{
	.read =		polaris_read_config,
	.write =	polaris_write_config,
};

void __init
polaris_init_arch(void)
{
	struct pci_controller *hose;

#if 0
	printk("polaris_init_arch(): trusting firmware for setup\n");
#endif


	pci_isa_hose = hose = alloc_pci_controller();
	hose->io_space = &ioport_resource;
	hose->mem_space = &iomem_resource;
	hose->index = 0;

	hose->sparse_mem_base = 0;
	hose->dense_mem_base = POLARIS_DENSE_MEM_BASE - IDENT_ADDR;
	hose->sparse_io_base = 0;
	hose->dense_io_base = POLARIS_DENSE_IO_BASE - IDENT_ADDR;

	hose->sg_isa = hose->sg_pci = NULL;

	
	__direct_map_base = 0x80000000;
	__direct_map_size = 0x80000000;
}

static inline void
polaris_pci_clr_err(void)
{
	*(vusp)POLARIS_W_STATUS;
	
	*(vusp)POLARIS_W_STATUS = 0x7800;
	mb();
	*(vusp)POLARIS_W_STATUS;
}

void
polaris_machine_check(unsigned long vector, unsigned long la_ptr)
{
	
	mb();
	mb();
	draina();
	polaris_pci_clr_err();
	wrmces(0x7);
	mb();

	process_mcheck_info(vector, la_ptr, "POLARIS",
			    mcheck_expected(0));
}
