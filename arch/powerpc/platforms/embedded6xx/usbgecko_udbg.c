/*
 * arch/powerpc/platforms/embedded6xx/usbgecko_udbg.c
 *
 * udbg serial input/output routines for the USB Gecko adapter.
 * Copyright (C) 2008-2009 The GameCube Linux Team
 * Copyright (C) 2008,2009 Albert Herranz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 */

#include <mm/mmu_decl.h>

#include <asm/io.h>
#include <asm/prom.h>
#include <asm/udbg.h>
#include <asm/fixmap.h>

#include "usbgecko_udbg.h"


#define EXI_CLK_32MHZ           5

#define EXI_CSR                 0x00
#define   EXI_CSR_CLKMASK       (0x7<<4)
#define     EXI_CSR_CLK_32MHZ   (EXI_CLK_32MHZ<<4)
#define   EXI_CSR_CSMASK        (0x7<<7)
#define     EXI_CSR_CS_0        (0x1<<7)  

#define EXI_CR                  0x0c
#define   EXI_CR_TSTART         (1<<0)
#define   EXI_CR_WRITE		(1<<2)
#define   EXI_CR_READ_WRITE     (2<<2)
#define   EXI_CR_TLEN(len)      (((len)-1)<<4)

#define EXI_DATA                0x10

#define UG_READ_ATTEMPTS	100
#define UG_WRITE_ATTEMPTS	100


static void __iomem *ug_io_base;

static u32 ug_io_transaction(u32 in)
{
	u32 __iomem *csr_reg = ug_io_base + EXI_CSR;
	u32 __iomem *data_reg = ug_io_base + EXI_DATA;
	u32 __iomem *cr_reg = ug_io_base + EXI_CR;
	u32 csr, data, cr;

	
	csr = EXI_CSR_CLK_32MHZ | EXI_CSR_CS_0;
	out_be32(csr_reg, csr);

	
	data = in;
	out_be32(data_reg, data);
	cr = EXI_CR_TLEN(2) | EXI_CR_READ_WRITE | EXI_CR_TSTART;
	out_be32(cr_reg, cr);

	while (in_be32(cr_reg) & EXI_CR_TSTART)
		barrier();

	
	out_be32(csr_reg, 0);

	
	data = in_be32(data_reg);

	return data;
}

static int ug_is_adapter_present(void)
{
	if (!ug_io_base)
		return 0;

	return ug_io_transaction(0x90000000) == 0x04700000;
}

static int ug_is_txfifo_ready(void)
{
	return ug_io_transaction(0xc0000000) & 0x04000000;
}

static void ug_raw_putc(char ch)
{
	ug_io_transaction(0xb0000000 | (ch << 20));
}

static void ug_putc(char ch)
{
	int count = UG_WRITE_ATTEMPTS;

	if (!ug_io_base)
		return;

	if (ch == '\n')
		ug_putc('\r');

	while (!ug_is_txfifo_ready() && count--)
		barrier();
	if (count >= 0)
		ug_raw_putc(ch);
}

static int ug_is_rxfifo_ready(void)
{
	return ug_io_transaction(0xd0000000) & 0x04000000;
}

static int ug_raw_getc(void)
{
	u32 data = ug_io_transaction(0xa0000000);
	if (data & 0x08000000)
		return (data >> 16) & 0xff;
	else
		return -1;
}

static int ug_getc(void)
{
	int count = UG_READ_ATTEMPTS;

	if (!ug_io_base)
		return -1;

	while (!ug_is_rxfifo_ready() && count--)
		barrier();
	return ug_raw_getc();
}


void ug_udbg_putc(char ch)
{
	ug_putc(ch);
}

static int ug_udbg_getc(void)
{
	int ch;

	while ((ch = ug_getc()) == -1)
		barrier();
	return ch;
}

static int ug_udbg_getc_poll(void)
{
	if (!ug_is_rxfifo_ready())
		return -1;
	return ug_getc();
}

static void __iomem *ug_udbg_setup_exi_io_base(struct device_node *np)
{
	void __iomem *exi_io_base = NULL;
	phys_addr_t paddr;
	const unsigned int *reg;

	reg = of_get_property(np, "reg", NULL);
	if (reg) {
		paddr = of_translate_address(np, reg);
		if (paddr)
			exi_io_base = ioremap(paddr, reg[1]);
	}
	return exi_io_base;
}

static void __iomem *ug_udbg_probe(void __iomem *exi_io_base)
{
	int i;

	
	for (i = 0; i < 2; i++) {
		ug_io_base = exi_io_base + 0x14 * i;
		if (ug_is_adapter_present())
			break;
	}
	if (i == 2)
		ug_io_base = NULL;
	return ug_io_base;

}

void __init ug_udbg_init(void)
{
	struct device_node *np;
	void __iomem *exi_io_base;

	if (ug_io_base)
		udbg_printf("%s: early -> final\n", __func__);

	np = of_find_compatible_node(NULL, NULL, "nintendo,flipper-exi");
	if (!np) {
		udbg_printf("%s: EXI node not found\n", __func__);
		goto done;
	}

	exi_io_base = ug_udbg_setup_exi_io_base(np);
	if (!exi_io_base) {
		udbg_printf("%s: failed to setup EXI io base\n", __func__);
		goto done;
	}

	if (!ug_udbg_probe(exi_io_base)) {
		udbg_printf("usbgecko_udbg: not found\n");
		iounmap(exi_io_base);
	} else {
		udbg_putc = ug_udbg_putc;
		udbg_getc = ug_udbg_getc;
		udbg_getc_poll = ug_udbg_getc_poll;
		udbg_printf("usbgecko_udbg: ready\n");
	}

done:
	if (np)
		of_node_put(np);
	return;
}

#ifdef CONFIG_PPC_EARLY_DEBUG_USBGECKO

static phys_addr_t __init ug_early_grab_io_addr(void)
{
#if defined(CONFIG_GAMECUBE)
	return 0x0c000000;
#elif defined(CONFIG_WII)
	return 0x0d000000;
#else
#error Invalid platform for USB Gecko based early debugging.
#endif
}

void __init udbg_init_usbgecko(void)
{
	void __iomem *early_debug_area;
	void __iomem *exi_io_base;

	early_debug_area = (void __iomem *)__fix_to_virt(FIX_EARLY_DEBUG_BASE);
	exi_io_base = early_debug_area + 0x00006800;

	
	if (!ug_udbg_probe(exi_io_base))
		return;

	
	udbg_putc = ug_udbg_putc;
	udbg_getc = ug_udbg_getc;
	udbg_getc_poll = ug_udbg_getc_poll;

	setbat(1, (unsigned long)early_debug_area,
	       ug_early_grab_io_addr(), 128*1024, PAGE_KERNEL_NCG);
}

#endif 

