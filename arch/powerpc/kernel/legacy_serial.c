#include <linux/kernel.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>
#include <linux/serial_core.h>
#include <linux/console.h>
#include <linux/pci.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/serial_reg.h>
#include <asm/io.h>
#include <asm/mmu.h>
#include <asm/prom.h>
#include <asm/serial.h>
#include <asm/udbg.h>
#include <asm/pci-bridge.h>
#include <asm/ppc-pci.h>

#undef DEBUG

#ifdef DEBUG
#define DBG(fmt...) do { printk(fmt); } while(0)
#else
#define DBG(fmt...) do { } while(0)
#endif

#define MAX_LEGACY_SERIAL_PORTS	8

static struct plat_serial8250_port
legacy_serial_ports[MAX_LEGACY_SERIAL_PORTS+1];
static struct legacy_serial_info {
	struct device_node		*np;
	unsigned int			speed;
	unsigned int			clock;
	int				irq_check_parent;
	phys_addr_t			taddr;
} legacy_serial_infos[MAX_LEGACY_SERIAL_PORTS];

static struct __initdata of_device_id legacy_serial_parents[] = {
	{.type = "soc",},
	{.type = "tsi-bridge",},
	{.type = "opb", },
	{.compatible = "ibm,opb",},
	{.compatible = "simple-bus",},
	{.compatible = "wrs,epld-localbus",},
	{},
};

static unsigned int legacy_serial_count;
static int legacy_serial_console = -1;

static unsigned int tsi_serial_in(struct uart_port *p, int offset)
{
	unsigned int tmp;
	offset = offset << p->regshift;
	if (offset == UART_IIR) {
		tmp = readl(p->membase + (UART_IIR & ~3));
		return (tmp >> 16) & 0xff; 
	} else
		return readb(p->membase + offset);
}

static void tsi_serial_out(struct uart_port *p, int offset, int value)
{
	offset = offset << p->regshift;
	if (!((offset == UART_IER) && (value & UART_IER_UUE)))
		writeb(value, p->membase + offset);
}

static int __init add_legacy_port(struct device_node *np, int want_index,
				  int iotype, phys_addr_t base,
				  phys_addr_t taddr, unsigned long irq,
				  upf_t flags, int irq_check_parent)
{
	const __be32 *clk, *spd;
	u32 clock = BASE_BAUD * 16;
	int index;

	
	clk = of_get_property(np, "clock-frequency", NULL);
	if (clk && *clk)
		clock = be32_to_cpup(clk);

	
	spd = of_get_property(np, "current-speed", NULL);

	
	if (want_index >= 0 && want_index < MAX_LEGACY_SERIAL_PORTS)
		index = want_index;
	else
		index = legacy_serial_count;

	if (index >= MAX_LEGACY_SERIAL_PORTS)
		return -1;
	if (index >= legacy_serial_count)
		legacy_serial_count = index + 1;

	
	if (legacy_serial_infos[index].np != 0) {
		
		if (legacy_serial_count < MAX_LEGACY_SERIAL_PORTS) {
			printk(KERN_DEBUG "Moved legacy port %d -> %d\n",
			       index, legacy_serial_count);
			legacy_serial_ports[legacy_serial_count] =
				legacy_serial_ports[index];
			legacy_serial_infos[legacy_serial_count] =
				legacy_serial_infos[index];
			legacy_serial_count++;
		} else {
			printk(KERN_DEBUG "Replacing legacy port %d\n", index);
		}
	}

	
	memset(&legacy_serial_ports[index], 0,
	       sizeof(struct plat_serial8250_port));
	if (iotype == UPIO_PORT)
		legacy_serial_ports[index].iobase = base;
	else
		legacy_serial_ports[index].mapbase = base;

	legacy_serial_ports[index].iotype = iotype;
	legacy_serial_ports[index].uartclk = clock;
	legacy_serial_ports[index].irq = irq;
	legacy_serial_ports[index].flags = flags;
	legacy_serial_infos[index].taddr = taddr;
	legacy_serial_infos[index].np = of_node_get(np);
	legacy_serial_infos[index].clock = clock;
	legacy_serial_infos[index].speed = spd ? be32_to_cpup(spd) : 0;
	legacy_serial_infos[index].irq_check_parent = irq_check_parent;

	if (iotype == UPIO_TSI) {
		legacy_serial_ports[index].serial_in = tsi_serial_in;
		legacy_serial_ports[index].serial_out = tsi_serial_out;
	}

	printk(KERN_DEBUG "Found legacy serial port %d for %s\n",
	       index, np->full_name);
	printk(KERN_DEBUG "  %s=%llx, taddr=%llx, irq=%lx, clk=%d, speed=%d\n",
	       (iotype == UPIO_PORT) ? "port" : "mem",
	       (unsigned long long)base, (unsigned long long)taddr, irq,
	       legacy_serial_ports[index].uartclk,
	       legacy_serial_infos[index].speed);

	return index;
}

static int __init add_legacy_soc_port(struct device_node *np,
				      struct device_node *soc_dev)
{
	u64 addr;
	const u32 *addrp;
	upf_t flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_SHARE_IRQ
		| UPF_FIXED_PORT;
	struct device_node *tsi = of_get_parent(np);

	if (of_get_property(np, "clock-frequency", NULL) == NULL)
		return -1;

	
	if ((of_get_property(np, "reg-shift", NULL) != NULL) ||
		(of_get_property(np, "reg-offset", NULL) != NULL))
		return -1;

	
	if (of_get_property(np, "used-by-rtas", NULL) != NULL)
		return -1;

	
	addrp = of_get_address(soc_dev, 0, NULL, NULL);
	if (addrp == NULL)
		return -1;

	addr = of_translate_address(soc_dev, addrp);
	if (addr == OF_BAD_ADDR)
		return -1;

	if (tsi && !strcmp(tsi->type, "tsi-bridge"))
		return add_legacy_port(np, -1, UPIO_TSI, addr, addr, NO_IRQ, flags, 0);
	else
		return add_legacy_port(np, -1, UPIO_MEM, addr, addr, NO_IRQ, flags, 0);
}

static int __init add_legacy_isa_port(struct device_node *np,
				      struct device_node *isa_brg)
{
	const __be32 *reg;
	const char *typep;
	int index = -1;
	u64 taddr;

	DBG(" -> add_legacy_isa_port(%s)\n", np->full_name);

	
	reg = of_get_property(np, "reg", NULL);
	if (reg == NULL)
		return -1;

	
	if (!(be32_to_cpu(reg[0]) & 0x00000001))
		return -1;

	typep = of_get_property(np, "ibm,aix-loc", NULL);

	
	if (typep && *typep == 'S')
		index = simple_strtol(typep+1, NULL, 0) - 1;

	taddr = of_translate_address(np, reg);
	if (taddr == OF_BAD_ADDR)
		taddr = 0;

	
	return add_legacy_port(np, index, UPIO_PORT, be32_to_cpu(reg[1]), taddr,
			       NO_IRQ, UPF_BOOT_AUTOCONF, 0);

}

#ifdef CONFIG_PCI
static int __init add_legacy_pci_port(struct device_node *np,
				      struct device_node *pci_dev)
{
	u64 addr, base;
	const u32 *addrp;
	unsigned int flags;
	int iotype, index = -1, lindex = 0;

	DBG(" -> add_legacy_pci_port(%s)\n", np->full_name);

	if (of_get_property(np, "clock-frequency", NULL) == NULL)
		return -1;

	
	addrp = of_get_pci_address(pci_dev, 0, NULL, &flags);
	if (addrp == NULL)
		return -1;

	
	iotype = (flags & IORESOURCE_MEM) ? UPIO_MEM : UPIO_PORT;
	addr = of_translate_address(pci_dev, addrp);
	if (addr == OF_BAD_ADDR)
		return -1;

	if (iotype == UPIO_MEM)
		base = addr;
	else
		base = addrp[2];

	if (np != pci_dev) {
		const __be32 *reg = of_get_property(np, "reg", NULL);
		if (reg && (be32_to_cpup(reg) < 4))
			index = lindex = be32_to_cpup(reg);
	}

	if (of_device_is_compatible(pci_dev, "pci13a8,152") ||
	    of_device_is_compatible(pci_dev, "pci13a8,154") ||
	    of_device_is_compatible(pci_dev, "pci13a8,158")) {
		addr += 0x200 * lindex;
		base += 0x200 * lindex;
	} else {
		addr += 8 * lindex;
		base += 8 * lindex;
	}

	return add_legacy_port(np, index, iotype, base, addr, NO_IRQ,
			       UPF_BOOT_AUTOCONF, np != pci_dev);
}
#endif

static void __init setup_legacy_serial_console(int console)
{
	struct legacy_serial_info *info =
		&legacy_serial_infos[console];
	void __iomem *addr;

	if (info->taddr == 0)
		return;
	addr = ioremap(info->taddr, 0x1000);
	if (addr == NULL)
		return;
	if (info->speed == 0)
		info->speed = udbg_probe_uart_speed(addr, info->clock);
	DBG("default console speed = %d\n", info->speed);
	udbg_init_uart(addr, info->speed, info->clock);
}

void __init find_legacy_serial_ports(void)
{
	struct device_node *np, *stdout = NULL;
	const char *path;
	int index;

	DBG(" -> find_legacy_serial_port()\n");

	
	path = of_get_property(of_chosen, "linux,stdout-path", NULL);
	if (path != NULL) {
		stdout = of_find_node_by_path(path);
		if (stdout)
			DBG("stdout is %s\n", stdout->full_name);
	} else {
		DBG(" no linux,stdout-path !\n");
	}

	
	for_each_compatible_node(np, "serial", "ns16550") {
		struct device_node *parent = of_get_parent(np);
		if (!parent)
			continue;
		if (of_match_node(legacy_serial_parents, parent) != NULL) {
			if (of_device_is_available(np)) {
				index = add_legacy_soc_port(np, np);
				if (index >= 0 && np == stdout)
					legacy_serial_console = index;
			}
		}
		of_node_put(parent);
	}

	
	for_each_node_by_type(np, "serial") {
		struct device_node *isa = of_get_parent(np);
		if (isa && !strcmp(isa->name, "isa")) {
			index = add_legacy_isa_port(np, isa);
			if (index >= 0 && np == stdout)
				legacy_serial_console = index;
		}
		of_node_put(isa);
	}

#ifdef CONFIG_PCI
	
	for (np = NULL; (np = of_find_all_nodes(np));) {
		struct device_node *pci, *parent = of_get_parent(np);
		if (parent && !strcmp(parent->name, "isa")) {
			of_node_put(parent);
			continue;
		}
		if (strcmp(np->name, "serial") && strcmp(np->type, "serial")) {
			of_node_put(parent);
			continue;
		}
		if (of_device_is_compatible(np, "pciclass,0700") ||
		    of_device_is_compatible(np, "pciclass,070002"))
			pci = np;
		else if (of_device_is_compatible(parent, "pciclass,0700") ||
			 of_device_is_compatible(parent, "pciclass,070002"))
			pci = parent;
		else {
			of_node_put(parent);
			continue;
		}
		index = add_legacy_pci_port(np, pci);
		if (index >= 0 && np == stdout)
			legacy_serial_console = index;
		of_node_put(parent);
	}
#endif

	DBG("legacy_serial_console = %d\n", legacy_serial_console);
	if (legacy_serial_console >= 0)
		setup_legacy_serial_console(legacy_serial_console);
	DBG(" <- find_legacy_serial_port()\n");
}

static struct platform_device serial_device = {
	.name	= "serial8250",
	.id	= PLAT8250_DEV_PLATFORM,
	.dev	= {
		.platform_data = legacy_serial_ports,
	},
};

static void __init fixup_port_irq(int index,
				  struct device_node *np,
				  struct plat_serial8250_port *port)
{
	unsigned int virq;

	DBG("fixup_port_irq(%d)\n", index);

	virq = irq_of_parse_and_map(np, 0);
	if (virq == NO_IRQ && legacy_serial_infos[index].irq_check_parent) {
		np = of_get_parent(np);
		if (np == NULL)
			return;
		virq = irq_of_parse_and_map(np, 0);
		of_node_put(np);
	}
	if (virq == NO_IRQ)
		return;

	port->irq = virq;

#ifdef CONFIG_SERIAL_8250_FSL
	if (of_device_is_compatible(np, "fsl,ns16550"))
		port->handle_irq = fsl8250_handle_irq;
#endif
}

static void __init fixup_port_pio(int index,
				  struct device_node *np,
				  struct plat_serial8250_port *port)
{
#ifdef CONFIG_PCI
	struct pci_controller *hose;

	DBG("fixup_port_pio(%d)\n", index);

	hose = pci_find_hose_for_OF_device(np);
	if (hose) {
		unsigned long offset = (unsigned long)hose->io_base_virt -
#ifdef CONFIG_PPC64
			pci_io_base;
#else
			isa_io_base;
#endif
		DBG("port %d, IO %lx -> %lx\n",
		    index, port->iobase, port->iobase + offset);
		port->iobase += offset;
	}
#endif
}

static void __init fixup_port_mmio(int index,
				   struct device_node *np,
				   struct plat_serial8250_port *port)
{
	DBG("fixup_port_mmio(%d)\n", index);

	port->membase = ioremap(port->mapbase, 0x100);
}

static int __init serial_dev_init(void)
{
	int i;

	if (legacy_serial_count == 0)
		return -ENODEV;

	DBG("Fixing serial ports interrupts and IO ports ...\n");

	for (i = 0; i < legacy_serial_count; i++) {
		struct plat_serial8250_port *port = &legacy_serial_ports[i];
		struct device_node *np = legacy_serial_infos[i].np;

		if (port->irq == NO_IRQ)
			fixup_port_irq(i, np, port);
		if (port->iotype == UPIO_PORT)
			fixup_port_pio(i, np, port);
		if ((port->iotype == UPIO_MEM) || (port->iotype == UPIO_TSI))
			fixup_port_mmio(i, np, port);
	}

	DBG("Registering platform serial ports\n");

	return platform_device_register(&serial_device);
}
device_initcall(serial_dev_init);


#ifdef CONFIG_SERIAL_8250_CONSOLE
static int __init check_legacy_serial_console(void)
{
	struct device_node *prom_stdout = NULL;
	int i, speed = 0, offset = 0;
	const char *name;
	const __be32 *spd;

	DBG(" -> check_legacy_serial_console()\n");

	
	if (strstr(boot_command_line, "console=")) {
		DBG(" console was specified !\n");
		return -EBUSY;
	}

	if (!of_chosen) {
		DBG(" of_chosen is NULL !\n");
		return -ENODEV;
	}

	if (legacy_serial_console < 0) {
		DBG(" legacy_serial_console not found !\n");
		return -ENODEV;
	}
	
	
	name = of_get_property(of_chosen, "linux,stdout-path", NULL);
	if (name == NULL) {
		DBG(" no linux,stdout-path !\n");
		return -ENODEV;
	}
	prom_stdout = of_find_node_by_path(name);
	if (!prom_stdout) {
		DBG(" can't find stdout package %s !\n", name);
		return -ENODEV;
	}
	DBG("stdout is %s\n", prom_stdout->full_name);

	name = of_get_property(prom_stdout, "name", NULL);
	if (!name) {
		DBG(" stdout package has no name !\n");
		goto not_found;
	}
	spd = of_get_property(prom_stdout, "current-speed", NULL);
	if (spd)
		speed = be32_to_cpup(spd);

	if (strcmp(name, "serial") != 0)
		goto not_found;

	
	for (i = 0; i < legacy_serial_count; i++) {
		if (prom_stdout != legacy_serial_infos[i].np)
			continue;
		offset = i;
		speed = legacy_serial_infos[i].speed;
		break;
	}
	if (i >= legacy_serial_count)
		goto not_found;

	of_node_put(prom_stdout);

	DBG("Found serial console at ttyS%d\n", offset);

	if (speed) {
		static char __initdata opt[16];
		sprintf(opt, "%d", speed);
		return add_preferred_console("ttyS", offset, opt);
	} else
		return add_preferred_console("ttyS", offset, NULL);

 not_found:
	DBG("No preferred console found !\n");
	of_node_put(prom_stdout);
	return -ENODEV;
}
console_initcall(check_legacy_serial_console);

#endif 
