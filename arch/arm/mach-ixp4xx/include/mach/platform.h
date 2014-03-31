/*
 * arch/arm/mach-ixp4xx/include/mach/platform.h
 *
 * Constants and functions that are useful to IXP4xx platform-specific code
 * and device drivers.
 *
 * Copyright (C) 2004 MontaVista Software, Inc.
 */

#ifndef __ASM_ARCH_HARDWARE_H__
#error "Do not include this directly, instead #include <mach/hardware.h>"
#endif

#ifndef __ASSEMBLY__

#include <asm/types.h>

#ifndef	__ARMEB__
#define	REG_OFFSET	0
#else
#define	REG_OFFSET	3
#endif

#define IXP4XX_EXP_BUS_BASE_PHYS	(0x50000000)

extern unsigned long ixp4xx_exp_bus_size;

#define	IXP4XX_EXP_BUS_BASE(region)\
		(IXP4XX_EXP_BUS_BASE_PHYS + ((region) * ixp4xx_exp_bus_size))

#define IXP4XX_EXP_BUS_END(region)\
		(IXP4XX_EXP_BUS_BASE(region) + ixp4xx_exp_bus_size - 1)


#define IXP4XX_EXP_BUS_RECOVERY_T(x)	(((x) & 0x0f) << 16)
#define IXP4XX_EXP_BUS_HOLD_T(x)	(((x) & 0x03) << 20)
#define IXP4XX_EXP_BUS_STROBE_T(x)	(((x) & 0x0f) << 22)
#define IXP4XX_EXP_BUS_SETUP_T(x)	(((x) & 0x03) << 26)
#define IXP4XX_EXP_BUS_ADDR_T(x)	(((x) & 0x03) << 28)
#define IXP4XX_EXP_BUS_SIZE(x)		(((x) & 0x0f) << 10)
#define IXP4XX_EXP_BUS_CYCLES(x)	(((x) & 0x03) << 14)

#define IXP4XX_EXP_BUS_CS_EN		(1L << 31)
#define IXP4XX_EXP_BUS_BYTE_RD16	(1L << 6)
#define IXP4XX_EXP_BUS_HRDY_POL		(1L << 5)
#define IXP4XX_EXP_BUS_MUX_EN		(1L << 4)
#define IXP4XX_EXP_BUS_SPLT_EN		(1L << 3)
#define IXP4XX_EXP_BUS_WR_EN		(1L << 1)
#define IXP4XX_EXP_BUS_BYTE_EN		(1L << 0)

#define IXP4XX_EXP_BUS_CYCLES_INTEL	0x00
#define IXP4XX_EXP_BUS_CYCLES_MOTOROLA	0x01
#define IXP4XX_EXP_BUS_CYCLES_HPI	0x02

#define IXP4XX_FLASH_WRITABLE	(0x2)
#define IXP4XX_FLASH_DEFAULT	(0xbcd23c40)
#define IXP4XX_FLASH_WRITE	(0xbcd23c42)

#define IXP4XX_PERIPHERAL_BUS_CLOCK 	(66)  
#define IXP4XX_UART_XTAL        	14745600

struct ixp4xx_pata_data {
	volatile u32	*cs0_cfg;
	volatile u32	*cs1_cfg;
	unsigned long	cs0_bits;
	unsigned long	cs1_bits;
	void __iomem	*cs0;
	void __iomem	*cs1;
};

struct sys_timer;

#define IXP4XX_ETH_NPEA		0x00
#define IXP4XX_ETH_NPEB		0x10
#define IXP4XX_ETH_NPEC		0x20

struct eth_plat_info {
	u8 phy;		
	u8 rxq;		
	u8 txreadyq;
	u8 hwaddr[6];
};

struct hss_plat_info {
	int (*set_clock)(int port, unsigned int clock_type);
	int (*open)(int port, void *pdev,
		    void (*set_carrier_cb)(void *pdev, int carrier));
	void (*close)(int port, void *pdev);
	u8 txreadyq;
};

extern unsigned long ixp4xx_timer_freq;

extern void ixp4xx_map_io(void);
extern void ixp4xx_init_early(void);
extern void ixp4xx_init_irq(void);
extern void ixp4xx_sys_init(void);
extern void ixp4xx_timer_init(void);
extern struct sys_timer ixp4xx_timer;
extern void ixp4xx_restart(char, const char *);
extern void ixp4xx_pci_preinit(void);
struct pci_sys_data;
extern int ixp4xx_setup(int nr, struct pci_sys_data *sys);
extern struct pci_bus *ixp4xx_scan_bus(int nr, struct pci_sys_data *sys);

#define IXP4XX_GPIO_OUT 		0x1
#define IXP4XX_GPIO_IN  		0x2

#define IXP4XX_GPIO_LOW			0
#define IXP4XX_GPIO_HIGH		1

#define IXP4XX_GPIO_CLK_0		14
#define IXP4XX_GPIO_CLK_1		15

static inline void gpio_line_config(u8 line, u32 direction)
{
	if (direction == IXP4XX_GPIO_IN)
		*IXP4XX_GPIO_GPOER |= (1 << line);
	else
		*IXP4XX_GPIO_GPOER &= ~(1 << line);
}

static inline void gpio_line_get(u8 line, int *value)
{
	*value = (*IXP4XX_GPIO_GPINR >> line) & 0x1;
}

static inline void gpio_line_set(u8 line, int value)
{
	if (value == IXP4XX_GPIO_HIGH)
	    *IXP4XX_GPIO_GPOUTR |= (1 << line);
	else if (value == IXP4XX_GPIO_LOW)
	    *IXP4XX_GPIO_GPOUTR &= ~(1 << line);
}

#endif 

