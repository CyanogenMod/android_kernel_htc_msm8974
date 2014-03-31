
#include <linux/pci.h>
#include <linux/acpi.h>
#include <acpi/reboot.h>

void acpi_reboot(void)
{
	struct acpi_generic_address *rr;
	struct pci_bus *bus0;
	u8 reset_value;
	unsigned int devfn;

	if (acpi_disabled)
		return;

	rr = &acpi_gbl_FADT.reset_register;

	

	if (acpi_gbl_FADT.header.revision < 2)
		return;

	if (!(acpi_gbl_FADT.flags & ACPI_FADT_RESET_REGISTER))
		return;

	reset_value = acpi_gbl_FADT.reset_value;

	switch (rr->space_id) {
	case ACPI_ADR_SPACE_PCI_CONFIG:
		
		bus0 = pci_find_bus(0, 0);
		if (!bus0)
			return;
		
		devfn = PCI_DEVFN((rr->address >> 32) & 0xffff,
				  (rr->address >> 16) & 0xffff);
		printk(KERN_DEBUG "Resetting with ACPI PCI RESET_REG.");
		
		pci_bus_write_config_byte(bus0, devfn,
				(rr->address & 0xffff), reset_value);
		break;

	case ACPI_ADR_SPACE_SYSTEM_MEMORY:
	case ACPI_ADR_SPACE_SYSTEM_IO:
		printk(KERN_DEBUG "ACPI MEMORY or I/O RESET_REG.\n");
		acpi_reset();
		break;
	}
}
