/*
 * early_printk_mrst.c - early consoles for Intel MID platforms
 *
 * Copyright (c) 2008-2010, Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */


#include <linux/serial_reg.h>
#include <linux/serial_mfd.h>
#include <linux/kmsg_dump.h>
#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>

#include <asm/fixmap.h>
#include <asm/pgtable.h>
#include <asm/mrst.h>

#define MRST_SPI_TIMEOUT		0x200000
#define MRST_REGBASE_SPI0		0xff128000
#define MRST_REGBASE_SPI1		0xff128400
#define MRST_CLK_SPI0_REG		0xff11d86c

#define SPI_DFS_OFFSET			0

#define SPI_FRF_OFFSET			4
#define SPI_FRF_SPI			0x0
#define SPI_FRF_SSP			0x1
#define SPI_FRF_MICROWIRE		0x2
#define SPI_FRF_RESV			0x3

#define SPI_MODE_OFFSET			6
#define SPI_SCPH_OFFSET			6
#define SPI_SCOL_OFFSET			7
#define SPI_TMOD_OFFSET			8
#define	SPI_TMOD_TR			0x0		
#define SPI_TMOD_TO			0x1		
#define SPI_TMOD_RO			0x2		
#define SPI_TMOD_EPROMREAD		0x3		

#define SPI_SLVOE_OFFSET		10
#define SPI_SRL_OFFSET			11
#define SPI_CFS_OFFSET			12

#define SR_MASK				0x7f		
#define SR_BUSY				(1 << 0)
#define SR_TF_NOT_FULL			(1 << 1)
#define SR_TF_EMPT			(1 << 2)
#define SR_RF_NOT_EMPT			(1 << 3)
#define SR_RF_FULL			(1 << 4)
#define SR_TX_ERR			(1 << 5)
#define SR_DCOL				(1 << 6)

struct dw_spi_reg {
	u32	ctrl0;
	u32	ctrl1;
	u32	ssienr;
	u32	mwcr;
	u32	ser;
	u32	baudr;
	u32	txfltr;
	u32	rxfltr;
	u32	txflr;
	u32	rxflr;
	u32	sr;
	u32	imr;
	u32	isr;
	u32	risr;
	u32	txoicr;
	u32	rxoicr;
	u32	rxuicr;
	u32	msticr;
	u32	icr;
	u32	dmacr;
	u32	dmatdlr;
	u32	dmardlr;
	u32	idr;
	u32	version;

	
	u32	dr;
} __packed;

#define dw_readl(dw, name)		__raw_readl(&(dw)->name)
#define dw_writel(dw, name, val)	__raw_writel((val), &(dw)->name)

static unsigned long mrst_spi_paddr = MRST_REGBASE_SPI0;

static u32 *pclk_spi0;
static struct dw_spi_reg *pspi;

static struct kmsg_dumper dw_dumper;
static int dumper_registered;

static void dw_kmsg_dump(struct kmsg_dumper *dumper,
			enum kmsg_dump_reason reason,
			const char *s1, unsigned long l1,
			const char *s2, unsigned long l2)
{
	int i;

	
	mrst_early_console_init();

	for (i = 0; i < l1; i++)
		early_mrst_console.write(&early_mrst_console, s1 + i, 1);
	for (i = 0; i < l2; i++)
		early_mrst_console.write(&early_mrst_console, s2 + i, 1);
}

static void max3110_write_config(void)
{
	u16 config;

	config = 0xc001;
	dw_writel(pspi, dr, config);
}

static void max3110_write_data(char c)
{
	u16 data;

	data = 0x8000 | c;
	dw_writel(pspi, dr, data);
}

void mrst_early_console_init(void)
{
	u32 ctrlr0 = 0;
	u32 spi0_cdiv;
	u32 freq; 

	
	pclk_spi0 = (void *)set_fixmap_offset_nocache(FIX_EARLYCON_MEM_BASE,
							MRST_CLK_SPI0_REG);
	spi0_cdiv = ((*pclk_spi0) & 0xe00) >> 9;
	freq = 100000000 / (spi0_cdiv + 1);

	if (mrst_identify_cpu() == MRST_CPU_CHIP_PENWELL)
		mrst_spi_paddr = MRST_REGBASE_SPI1;

	pspi = (void *)set_fixmap_offset_nocache(FIX_EARLYCON_MEM_BASE,
						mrst_spi_paddr);

	
	dw_writel(pspi, ssienr, 0);

	
	ctrlr0 = dw_readl(pspi, ctrl0);

	ctrlr0 &= 0xfcc0;
	ctrlr0 |= 0xf | (SPI_FRF_SPI << SPI_FRF_OFFSET)
		      | (SPI_TMOD_TO << SPI_TMOD_OFFSET);
	dw_writel(pspi, ctrl0, ctrlr0);

	dw_writel(pspi, baudr, freq/100000);

	
	dw_writel(pspi, imr, 0x0);

	
	dw_writel(pspi, ser, 0x2);

	
	dw_writel(pspi, ssienr, 0x1);

	
	max3110_write_config();

	
	if (!dumper_registered) {
		dw_dumper.dump = dw_kmsg_dump;
		kmsg_dump_register(&dw_dumper);
		dumper_registered = 1;
	}
}

static void early_mrst_spi_putc(char c)
{
	unsigned int timeout;
	u32 sr;

	timeout = MRST_SPI_TIMEOUT;
	
	while (--timeout) {
		sr = dw_readl(pspi, sr);
		if (!(sr & SR_TF_NOT_FULL))
			cpu_relax();
		else
			break;
	}

	if (!timeout)
		pr_warning("MRST earlycon: timed out\n");
	else
		max3110_write_data(c);
}

static void early_mrst_spi_write(struct console *con, const char *str, unsigned n)
{
	int i;

	for (i = 0; i < n && *str; i++) {
		if (*str == '\n')
			early_mrst_spi_putc('\r');
		early_mrst_spi_putc(*str);
		str++;
	}
}

struct console early_mrst_console = {
	.name =		"earlymrst",
	.write =	early_mrst_spi_write,
	.flags =	CON_PRINTBUFFER,
	.index =	-1,
};

#define HSU_PORT_BASE		0xffa28080

static void __iomem *phsu;

void hsu_early_console_init(const char *s)
{
	unsigned long paddr, port = 0;
	u8 lcr;

	if (*s && !kstrtoul(s, 10, &port))
		port = clamp_val(port, 0, 2);

	paddr = HSU_PORT_BASE + port * 0x80;
	phsu = (void *)set_fixmap_offset_nocache(FIX_EARLYCON_MEM_BASE, paddr);

	
	writeb(0x0, phsu + UART_FCR);

	
	lcr = readb(phsu + UART_LCR);
	writeb((0x80 | lcr), phsu + UART_LCR);
	writeb(0x18, phsu + UART_DLL);
	writeb(lcr,  phsu + UART_LCR);
	writel(0x3600, phsu + UART_MUL*4);

	writeb(0x8, phsu + UART_MCR);
	writeb(0x7, phsu + UART_FCR);
	writeb(0x3, phsu + UART_LCR);

	
	readb(phsu + UART_LSR);
	readb(phsu + UART_RX);
	readb(phsu + UART_IIR);
	readb(phsu + UART_MSR);

	
	writeb(0x7, phsu + UART_FCR);
}

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

static void early_hsu_putc(char ch)
{
	unsigned int timeout = 10000; 
	u8 status;

	while (--timeout) {
		status = readb(phsu + UART_LSR);
		if (status & BOTH_EMPTY)
			break;
		udelay(1);
	}

	
	if (timeout)
		writeb(ch, phsu + UART_TX);
}

static void early_hsu_write(struct console *con, const char *str, unsigned n)
{
	int i;

	for (i = 0; i < n && *str; i++) {
		if (*str == '\n')
			early_hsu_putc('\r');
		early_hsu_putc(*str);
		str++;
	}
}

struct console early_hsu_console = {
	.name =		"earlyhsu",
	.write =	early_hsu_write,
	.flags =	CON_PRINTBUFFER,
	.index =	-1,
};
