#include <linux/intel-iommu.h>

struct ioapic_scope {
	struct intel_iommu *iommu;
	unsigned int id;
	unsigned int bus;	
	unsigned int devfn;	
};

struct hpet_scope {
	struct intel_iommu *iommu;
	u8 id;
	unsigned int bus;
	unsigned int devfn;
};

#define IR_X2APIC_MODE(mode) (mode ? (1 << 11) : 0)
