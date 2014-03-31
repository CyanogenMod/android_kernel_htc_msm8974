
#undef DEBUG

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

#define PCI_PROBE_BIOS		0x0001
#define PCI_PROBE_CONF1		0x0002
#define PCI_PROBE_CONF2		0x0004
#define PCI_PROBE_MMCONF	0x0008
#define PCI_PROBE_MASK		0x000f
#define PCI_PROBE_NOEARLY	0x0010

#define PCI_NO_CHECKS		0x0400
#define PCI_USE_PIRQ_MASK	0x0800
#define PCI_ASSIGN_ROMS		0x1000
#define PCI_BIOS_IRQ_SCAN	0x2000
#define PCI_ASSIGN_ALL_BUSSES	0x4000
#define PCI_CAN_SKIP_ISA_ALIGN	0x8000
#define PCI_USE__CRS		0x10000
#define PCI_CHECK_ENABLE_AMD_MMCONF	0x20000
#define PCI_HAS_IO_ECS		0x40000
#define PCI_NOASSIGN_ROMS	0x80000
#define PCI_ROOT_NO_CRS		0x100000
#define PCI_NOASSIGN_BARS	0x200000

extern unsigned int pci_probe;
extern unsigned long pirq_table_addr;

enum pci_bf_sort_state {
	pci_bf_sort_default,
	pci_force_nobf,
	pci_force_bf,
	pci_dmi_bf,
};


void pcibios_resource_survey(void);
void pcibios_set_cache_line_size(void);


extern int pcibios_last_bus;
extern struct pci_bus *pci_root_bus;
extern struct pci_ops pci_root_ops;

void pcibios_scan_specific_bus(int busn);


struct irq_info {
	u8 bus, devfn;			
	struct {
		u8 link;		
		u16 bitmap;		
	} __attribute__((packed)) irq[4];
	u8 slot;			
	u8 rfu;
} __attribute__((packed));

struct irq_routing_table {
	u32 signature;			
	u16 version;			
	u16 size;			
	u8 rtr_bus, rtr_devfn;		
	u16 exclusive_irqs;		
	u16 rtr_vendor, rtr_device;	
	u32 miniport_data;		
	u8 rfu[11];
	u8 checksum;			
	struct irq_info slots[0];
} __attribute__((packed));

extern unsigned int pcibios_irq_mask;

extern raw_spinlock_t pci_config_lock;

extern int (*pcibios_enable_irq)(struct pci_dev *dev);
extern void (*pcibios_disable_irq)(struct pci_dev *dev);

struct pci_raw_ops {
	int (*read)(unsigned int domain, unsigned int bus, unsigned int devfn,
						int reg, int len, u32 *val);
	int (*write)(unsigned int domain, unsigned int bus, unsigned int devfn,
						int reg, int len, u32 val);
};

extern const struct pci_raw_ops *raw_pci_ops;
extern const struct pci_raw_ops *raw_pci_ext_ops;

extern const struct pci_raw_ops pci_direct_conf1;
extern bool port_cf9_safe;

extern int pci_direct_probe(void);
extern void pci_direct_init(int type);
extern void pci_pcbios_init(void);
extern void __init dmi_check_pciprobe(void);
extern void __init dmi_check_skip_isa_align(void);

extern int __init pci_acpi_init(void);
extern void __init pcibios_irq_init(void);
extern int __init pcibios_init(void);
extern int pci_legacy_init(void);
extern void pcibios_fixup_irqs(void);


#define PCI_MMCFG_RESOURCE_NAME_LEN (22 + 4 + 2 + 2)

struct pci_mmcfg_region {
	struct list_head list;
	struct resource res;
	u64 address;
	char __iomem *virt;
	u16 segment;
	u8 start_bus;
	u8 end_bus;
	char name[PCI_MMCFG_RESOURCE_NAME_LEN];
};

extern int __init pci_mmcfg_arch_init(void);
extern void __init pci_mmcfg_arch_free(void);
extern struct pci_mmcfg_region *pci_mmconfig_lookup(int segment, int bus);

extern struct list_head pci_mmcfg_list;

#define PCI_MMCFG_BUS_OFFSET(bus)      ((bus) << 20)

static inline unsigned char mmio_config_readb(void __iomem *pos)
{
	u8 val;
	asm volatile("movb (%1),%%al" : "=a" (val) : "r" (pos));
	return val;
}

static inline unsigned short mmio_config_readw(void __iomem *pos)
{
	u16 val;
	asm volatile("movw (%1),%%ax" : "=a" (val) : "r" (pos));
	return val;
}

static inline unsigned int mmio_config_readl(void __iomem *pos)
{
	u32 val;
	asm volatile("movl (%1),%%eax" : "=a" (val) : "r" (pos));
	return val;
}

static inline void mmio_config_writeb(void __iomem *pos, u8 val)
{
	asm volatile("movb %%al,(%1)" : : "a" (val), "r" (pos) : "memory");
}

static inline void mmio_config_writew(void __iomem *pos, u16 val)
{
	asm volatile("movw %%ax,(%1)" : : "a" (val), "r" (pos) : "memory");
}

static inline void mmio_config_writel(void __iomem *pos, u32 val)
{
	asm volatile("movl %%eax,(%1)" : : "a" (val), "r" (pos) : "memory");
}

#ifdef CONFIG_PCI
# ifdef CONFIG_ACPI
#  define x86_default_pci_init		pci_acpi_init
# else
#  define x86_default_pci_init		pci_legacy_init
# endif
# define x86_default_pci_init_irq	pcibios_irq_init
# define x86_default_pci_fixup_irqs	pcibios_fixup_irqs
#else
# define x86_default_pci_init		NULL
# define x86_default_pci_init_irq	NULL
# define x86_default_pci_fixup_irqs	NULL
#endif
