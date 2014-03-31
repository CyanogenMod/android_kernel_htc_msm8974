#ifndef __ARCH_ASM_MACH_OMAP2_PRCM_COMMON_H
#define __ARCH_ASM_MACH_OMAP2_PRCM_COMMON_H

/*
 * OMAP2/3 PRCM base and module definitions
 *
 * Copyright (C) 2007-2009, 2011 Texas Instruments, Inc.
 * Copyright (C) 2007-2009 Nokia Corporation
 *
 * Written by Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#define OCP_MOD						0x000
#define MPU_MOD						0x100
#define CORE_MOD					0x200
#define GFX_MOD						0x300
#define WKUP_MOD					0x400
#define PLL_MOD						0x500


#define OMAP24XX_GR_MOD					OCP_MOD
#define OMAP24XX_DSP_MOD				0x800

#define OMAP2430_MDM_MOD				0xc00

#define OMAP3430_IVA2_MOD				-0x800
#define OMAP3430ES2_SGX_MOD				GFX_MOD
#define OMAP3430_CCR_MOD				PLL_MOD
#define OMAP3430_DSS_MOD				0x600
#define OMAP3430_CAM_MOD				0x700
#define OMAP3430_PER_MOD				0x800
#define OMAP3430_EMU_MOD				0x900
#define OMAP3430_GR_MOD					0xa00
#define OMAP3430_NEON_MOD				0xb00
#define OMAP3430ES2_USBHOST_MOD				0xc00


#define OMAP2420_EN_MMC_SHIFT				26
#define OMAP2420_EN_MMC_MASK				(1 << 26)
#define OMAP24XX_EN_UART2_SHIFT				22
#define OMAP24XX_EN_UART2_MASK				(1 << 22)
#define OMAP24XX_EN_UART1_SHIFT				21
#define OMAP24XX_EN_UART1_MASK				(1 << 21)
#define OMAP24XX_EN_MCSPI2_SHIFT			18
#define OMAP24XX_EN_MCSPI2_MASK				(1 << 18)
#define OMAP24XX_EN_MCSPI1_SHIFT			17
#define OMAP24XX_EN_MCSPI1_MASK				(1 << 17)
#define OMAP24XX_EN_MCBSP2_SHIFT			16
#define OMAP24XX_EN_MCBSP2_MASK				(1 << 16)
#define OMAP24XX_EN_MCBSP1_SHIFT			15
#define OMAP24XX_EN_MCBSP1_MASK				(1 << 15)
#define OMAP24XX_EN_GPT12_SHIFT				14
#define OMAP24XX_EN_GPT12_MASK				(1 << 14)
#define OMAP24XX_EN_GPT11_SHIFT				13
#define OMAP24XX_EN_GPT11_MASK				(1 << 13)
#define OMAP24XX_EN_GPT10_SHIFT				12
#define OMAP24XX_EN_GPT10_MASK				(1 << 12)
#define OMAP24XX_EN_GPT9_SHIFT				11
#define OMAP24XX_EN_GPT9_MASK				(1 << 11)
#define OMAP24XX_EN_GPT8_SHIFT				10
#define OMAP24XX_EN_GPT8_MASK				(1 << 10)
#define OMAP24XX_EN_GPT7_SHIFT				9
#define OMAP24XX_EN_GPT7_MASK				(1 << 9)
#define OMAP24XX_EN_GPT6_SHIFT				8
#define OMAP24XX_EN_GPT6_MASK				(1 << 8)
#define OMAP24XX_EN_GPT5_SHIFT				7
#define OMAP24XX_EN_GPT5_MASK				(1 << 7)
#define OMAP24XX_EN_GPT4_SHIFT				6
#define OMAP24XX_EN_GPT4_MASK				(1 << 6)
#define OMAP24XX_EN_GPT3_SHIFT				5
#define OMAP24XX_EN_GPT3_MASK				(1 << 5)
#define OMAP24XX_EN_GPT2_SHIFT				4
#define OMAP24XX_EN_GPT2_MASK				(1 << 4)
#define OMAP2420_EN_VLYNQ_SHIFT				3
#define OMAP2420_EN_VLYNQ_MASK				(1 << 3)

#define OMAP2430_EN_GPIO5_SHIFT				10
#define OMAP2430_EN_GPIO5_MASK				(1 << 10)
#define OMAP2430_EN_MCSPI3_SHIFT			9
#define OMAP2430_EN_MCSPI3_MASK				(1 << 9)
#define OMAP2430_EN_MMCHS2_SHIFT			8
#define OMAP2430_EN_MMCHS2_MASK				(1 << 8)
#define OMAP2430_EN_MMCHS1_SHIFT			7
#define OMAP2430_EN_MMCHS1_MASK				(1 << 7)
#define OMAP24XX_EN_UART3_SHIFT				2
#define OMAP24XX_EN_UART3_MASK				(1 << 2)
#define OMAP24XX_EN_USB_SHIFT				0
#define OMAP24XX_EN_USB_MASK				(1 << 0)

#define OMAP2430_EN_MDM_INTC_SHIFT			11
#define OMAP2430_EN_MDM_INTC_MASK			(1 << 11)
#define OMAP2430_EN_USBHS_SHIFT				6
#define OMAP2430_EN_USBHS_MASK				(1 << 6)

#define OMAP2420_ST_MMC_SHIFT				26
#define OMAP2420_ST_MMC_MASK				(1 << 26)
#define OMAP24XX_ST_UART2_SHIFT				22
#define OMAP24XX_ST_UART2_MASK				(1 << 22)
#define OMAP24XX_ST_UART1_SHIFT				21
#define OMAP24XX_ST_UART1_MASK				(1 << 21)
#define OMAP24XX_ST_MCSPI2_SHIFT			18
#define OMAP24XX_ST_MCSPI2_MASK				(1 << 18)
#define OMAP24XX_ST_MCSPI1_SHIFT			17
#define OMAP24XX_ST_MCSPI1_MASK				(1 << 17)
#define OMAP24XX_ST_MCBSP2_SHIFT			16
#define OMAP24XX_ST_MCBSP2_MASK				(1 << 16)
#define OMAP24XX_ST_MCBSP1_SHIFT			15
#define OMAP24XX_ST_MCBSP1_MASK				(1 << 15)
#define OMAP24XX_ST_GPT12_SHIFT				14
#define OMAP24XX_ST_GPT12_MASK				(1 << 14)
#define OMAP24XX_ST_GPT11_SHIFT				13
#define OMAP24XX_ST_GPT11_MASK				(1 << 13)
#define OMAP24XX_ST_GPT10_SHIFT				12
#define OMAP24XX_ST_GPT10_MASK				(1 << 12)
#define OMAP24XX_ST_GPT9_SHIFT				11
#define OMAP24XX_ST_GPT9_MASK				(1 << 11)
#define OMAP24XX_ST_GPT8_SHIFT				10
#define OMAP24XX_ST_GPT8_MASK				(1 << 10)
#define OMAP24XX_ST_GPT7_SHIFT				9
#define OMAP24XX_ST_GPT7_MASK				(1 << 9)
#define OMAP24XX_ST_GPT6_SHIFT				8
#define OMAP24XX_ST_GPT6_MASK				(1 << 8)
#define OMAP24XX_ST_GPT5_SHIFT				7
#define OMAP24XX_ST_GPT5_MASK				(1 << 7)
#define OMAP24XX_ST_GPT4_SHIFT				6
#define OMAP24XX_ST_GPT4_MASK				(1 << 6)
#define OMAP24XX_ST_GPT3_SHIFT				5
#define OMAP24XX_ST_GPT3_MASK				(1 << 5)
#define OMAP24XX_ST_GPT2_SHIFT				4
#define OMAP24XX_ST_GPT2_MASK				(1 << 4)
#define OMAP2420_ST_VLYNQ_SHIFT				3
#define OMAP2420_ST_VLYNQ_MASK				(1 << 3)

#define OMAP2430_ST_MDM_INTC_SHIFT			11
#define OMAP2430_ST_MDM_INTC_MASK			(1 << 11)
#define OMAP2430_ST_GPIO5_SHIFT				10
#define OMAP2430_ST_GPIO5_MASK				(1 << 10)
#define OMAP2430_ST_MCSPI3_SHIFT			9
#define OMAP2430_ST_MCSPI3_MASK				(1 << 9)
#define OMAP2430_ST_MMCHS2_SHIFT			8
#define OMAP2430_ST_MMCHS2_MASK				(1 << 8)
#define OMAP2430_ST_MMCHS1_SHIFT			7
#define OMAP2430_ST_MMCHS1_MASK				(1 << 7)
#define OMAP2430_ST_USBHS_SHIFT				6
#define OMAP2430_ST_USBHS_MASK				(1 << 6)
#define OMAP24XX_ST_UART3_SHIFT				2
#define OMAP24XX_ST_UART3_MASK				(1 << 2)
#define OMAP24XX_ST_USB_SHIFT				0
#define OMAP24XX_ST_USB_MASK				(1 << 0)

#define OMAP24XX_EN_GPIOS_SHIFT				2
#define OMAP24XX_EN_GPIOS_MASK				(1 << 2)
#define OMAP24XX_EN_GPT1_SHIFT				0
#define OMAP24XX_EN_GPT1_MASK				(1 << 0)

#define OMAP24XX_ST_GPIOS_SHIFT				2
#define OMAP24XX_ST_GPIOS_MASK				(1 << 2)
#define OMAP24XX_ST_GPT1_SHIFT				0
#define OMAP24XX_ST_GPT1_MASK				(1 << 0)

#define OMAP2430_ST_MDM_SHIFT				0
#define OMAP2430_ST_MDM_MASK				(1 << 0)



#define OMAP3430_REV_SHIFT				0
#define OMAP3430_REV_MASK				(0xff << 0)

#define OMAP3430_AUTOIDLE_MASK				(1 << 0)

#define OMAP3430_EN_MMC3_MASK				(1 << 30)
#define OMAP3430_EN_MMC3_SHIFT				30
#define OMAP3430_EN_MMC2_MASK				(1 << 25)
#define OMAP3430_EN_MMC2_SHIFT				25
#define OMAP3430_EN_MMC1_MASK				(1 << 24)
#define OMAP3430_EN_MMC1_SHIFT				24
#define OMAP3430_EN_UART4_MASK				(1 << 23)
#define OMAP3430_EN_UART4_SHIFT				23
#define OMAP3430_EN_MCSPI4_MASK				(1 << 21)
#define OMAP3430_EN_MCSPI4_SHIFT			21
#define OMAP3430_EN_MCSPI3_MASK				(1 << 20)
#define OMAP3430_EN_MCSPI3_SHIFT			20
#define OMAP3430_EN_MCSPI2_MASK				(1 << 19)
#define OMAP3430_EN_MCSPI2_SHIFT			19
#define OMAP3430_EN_MCSPI1_MASK				(1 << 18)
#define OMAP3430_EN_MCSPI1_SHIFT			18
#define OMAP3430_EN_I2C3_MASK				(1 << 17)
#define OMAP3430_EN_I2C3_SHIFT				17
#define OMAP3430_EN_I2C2_MASK				(1 << 16)
#define OMAP3430_EN_I2C2_SHIFT				16
#define OMAP3430_EN_I2C1_MASK				(1 << 15)
#define OMAP3430_EN_I2C1_SHIFT				15
#define OMAP3430_EN_UART2_MASK				(1 << 14)
#define OMAP3430_EN_UART2_SHIFT				14
#define OMAP3430_EN_UART1_MASK				(1 << 13)
#define OMAP3430_EN_UART1_SHIFT				13
#define OMAP3430_EN_GPT11_MASK				(1 << 12)
#define OMAP3430_EN_GPT11_SHIFT				12
#define OMAP3430_EN_GPT10_MASK				(1 << 11)
#define OMAP3430_EN_GPT10_SHIFT				11
#define OMAP3430_EN_MCBSP5_MASK				(1 << 10)
#define OMAP3430_EN_MCBSP5_SHIFT			10
#define OMAP3430_EN_MCBSP1_MASK				(1 << 9)
#define OMAP3430_EN_MCBSP1_SHIFT			9
#define OMAP3430_EN_FSHOSTUSB_MASK			(1 << 5)
#define OMAP3430_EN_FSHOSTUSB_SHIFT			5
#define OMAP3430_EN_D2D_MASK				(1 << 3)
#define OMAP3430_EN_D2D_SHIFT				3

#define OMAP3430_EN_HSOTGUSB_MASK			(1 << 4)
#define OMAP3430_EN_HSOTGUSB_SHIFT			4

#define OMAP3430_ST_MMC3_SHIFT				30
#define OMAP3430_ST_MMC3_MASK				(1 << 30)
#define OMAP3430_ST_MMC2_SHIFT				25
#define OMAP3430_ST_MMC2_MASK				(1 << 25)
#define OMAP3430_ST_MMC1_SHIFT				24
#define OMAP3430_ST_MMC1_MASK				(1 << 24)
#define OMAP3430_ST_MCSPI4_SHIFT			21
#define OMAP3430_ST_MCSPI4_MASK				(1 << 21)
#define OMAP3430_ST_MCSPI3_SHIFT			20
#define OMAP3430_ST_MCSPI3_MASK				(1 << 20)
#define OMAP3430_ST_MCSPI2_SHIFT			19
#define OMAP3430_ST_MCSPI2_MASK				(1 << 19)
#define OMAP3430_ST_MCSPI1_SHIFT			18
#define OMAP3430_ST_MCSPI1_MASK				(1 << 18)
#define OMAP3430_ST_I2C3_SHIFT				17
#define OMAP3430_ST_I2C3_MASK				(1 << 17)
#define OMAP3430_ST_I2C2_SHIFT				16
#define OMAP3430_ST_I2C2_MASK				(1 << 16)
#define OMAP3430_ST_I2C1_SHIFT				15
#define OMAP3430_ST_I2C1_MASK				(1 << 15)
#define OMAP3430_ST_UART2_SHIFT				14
#define OMAP3430_ST_UART2_MASK				(1 << 14)
#define OMAP3430_ST_UART1_SHIFT				13
#define OMAP3430_ST_UART1_MASK				(1 << 13)
#define OMAP3430_ST_GPT11_SHIFT				12
#define OMAP3430_ST_GPT11_MASK				(1 << 12)
#define OMAP3430_ST_GPT10_SHIFT				11
#define OMAP3430_ST_GPT10_MASK				(1 << 11)
#define OMAP3430_ST_MCBSP5_SHIFT			10
#define OMAP3430_ST_MCBSP5_MASK				(1 << 10)
#define OMAP3430_ST_MCBSP1_SHIFT			9
#define OMAP3430_ST_MCBSP1_MASK				(1 << 9)
#define OMAP3430ES1_ST_FSHOSTUSB_SHIFT			5
#define OMAP3430ES1_ST_FSHOSTUSB_MASK			(1 << 5)
#define OMAP3430ES1_ST_HSOTGUSB_SHIFT			4
#define OMAP3430ES1_ST_HSOTGUSB_MASK			(1 << 4)
#define OMAP3430ES2_ST_HSOTGUSB_IDLE_SHIFT		5
#define OMAP3430ES2_ST_HSOTGUSB_IDLE_MASK		(1 << 5)
#define OMAP3430ES2_ST_HSOTGUSB_STDBY_SHIFT		4
#define OMAP3430ES2_ST_HSOTGUSB_STDBY_MASK		(1 << 4)
#define OMAP3430_ST_D2D_SHIFT				3
#define OMAP3430_ST_D2D_MASK				(1 << 3)

#define OMAP3430_EN_GPIO1_MASK				(1 << 3)
#define OMAP3430_EN_GPIO1_SHIFT				3
#define OMAP3430_EN_GPT12_MASK				(1 << 1)
#define OMAP3430_EN_GPT12_SHIFT				1
#define OMAP3430_EN_GPT1_MASK				(1 << 0)
#define OMAP3430_EN_GPT1_SHIFT				0

#define OMAP3430_EN_SR2_MASK				(1 << 7)
#define OMAP3430_EN_SR2_SHIFT				7
#define OMAP3430_EN_SR1_MASK				(1 << 6)
#define OMAP3430_EN_SR1_SHIFT				6

#define OMAP3430_EN_GPT12_MASK				(1 << 1)
#define OMAP3430_EN_GPT12_SHIFT				1

#define OMAP3430_ST_SR2_SHIFT				7
#define OMAP3430_ST_SR2_MASK				(1 << 7)
#define OMAP3430_ST_SR1_SHIFT				6
#define OMAP3430_ST_SR1_MASK				(1 << 6)
#define OMAP3430_ST_GPIO1_SHIFT				3
#define OMAP3430_ST_GPIO1_MASK				(1 << 3)
#define OMAP3430_ST_GPT12_SHIFT				1
#define OMAP3430_ST_GPT12_MASK				(1 << 1)
#define OMAP3430_ST_GPT1_SHIFT				0
#define OMAP3430_ST_GPT1_MASK				(1 << 0)

#define OMAP3430_EN_MPU_MASK				(1 << 1)
#define OMAP3430_EN_MPU_SHIFT				1


#define OMAP3630_EN_UART4_MASK				(1 << 18)
#define OMAP3630_EN_UART4_SHIFT				18
#define OMAP3430_EN_GPIO6_MASK				(1 << 17)
#define OMAP3430_EN_GPIO6_SHIFT				17
#define OMAP3430_EN_GPIO5_MASK				(1 << 16)
#define OMAP3430_EN_GPIO5_SHIFT				16
#define OMAP3430_EN_GPIO4_MASK				(1 << 15)
#define OMAP3430_EN_GPIO4_SHIFT				15
#define OMAP3430_EN_GPIO3_MASK				(1 << 14)
#define OMAP3430_EN_GPIO3_SHIFT				14
#define OMAP3430_EN_GPIO2_MASK				(1 << 13)
#define OMAP3430_EN_GPIO2_SHIFT				13
#define OMAP3430_EN_UART3_MASK				(1 << 11)
#define OMAP3430_EN_UART3_SHIFT				11
#define OMAP3430_EN_GPT9_MASK				(1 << 10)
#define OMAP3430_EN_GPT9_SHIFT				10
#define OMAP3430_EN_GPT8_MASK				(1 << 9)
#define OMAP3430_EN_GPT8_SHIFT				9
#define OMAP3430_EN_GPT7_MASK				(1 << 8)
#define OMAP3430_EN_GPT7_SHIFT				8
#define OMAP3430_EN_GPT6_MASK				(1 << 7)
#define OMAP3430_EN_GPT6_SHIFT				7
#define OMAP3430_EN_GPT5_MASK				(1 << 6)
#define OMAP3430_EN_GPT5_SHIFT				6
#define OMAP3430_EN_GPT4_MASK				(1 << 5)
#define OMAP3430_EN_GPT4_SHIFT				5
#define OMAP3430_EN_GPT3_MASK				(1 << 4)
#define OMAP3430_EN_GPT3_SHIFT				4
#define OMAP3430_EN_GPT2_MASK				(1 << 3)
#define OMAP3430_EN_GPT2_SHIFT				3

#define OMAP3430_EN_MCBSP4_MASK				(1 << 2)
#define OMAP3430_EN_MCBSP4_SHIFT			2
#define OMAP3430_EN_MCBSP3_MASK				(1 << 1)
#define OMAP3430_EN_MCBSP3_SHIFT			1
#define OMAP3430_EN_MCBSP2_MASK				(1 << 0)
#define OMAP3430_EN_MCBSP2_SHIFT			0

#define OMAP3630_ST_UART4_SHIFT				18
#define OMAP3630_ST_UART4_MASK				(1 << 18)
#define OMAP3430_ST_GPIO6_SHIFT				17
#define OMAP3430_ST_GPIO6_MASK				(1 << 17)
#define OMAP3430_ST_GPIO5_SHIFT				16
#define OMAP3430_ST_GPIO5_MASK				(1 << 16)
#define OMAP3430_ST_GPIO4_SHIFT				15
#define OMAP3430_ST_GPIO4_MASK				(1 << 15)
#define OMAP3430_ST_GPIO3_SHIFT				14
#define OMAP3430_ST_GPIO3_MASK				(1 << 14)
#define OMAP3430_ST_GPIO2_SHIFT				13
#define OMAP3430_ST_GPIO2_MASK				(1 << 13)
#define OMAP3430_ST_UART3_SHIFT				11
#define OMAP3430_ST_UART3_MASK				(1 << 11)
#define OMAP3430_ST_GPT9_SHIFT				10
#define OMAP3430_ST_GPT9_MASK				(1 << 10)
#define OMAP3430_ST_GPT8_SHIFT				9
#define OMAP3430_ST_GPT8_MASK				(1 << 9)
#define OMAP3430_ST_GPT7_SHIFT				8
#define OMAP3430_ST_GPT7_MASK				(1 << 8)
#define OMAP3430_ST_GPT6_SHIFT				7
#define OMAP3430_ST_GPT6_MASK				(1 << 7)
#define OMAP3430_ST_GPT5_SHIFT				6
#define OMAP3430_ST_GPT5_MASK				(1 << 6)
#define OMAP3430_ST_GPT4_SHIFT				5
#define OMAP3430_ST_GPT4_MASK				(1 << 5)
#define OMAP3430_ST_GPT3_SHIFT				4
#define OMAP3430_ST_GPT3_MASK				(1 << 4)
#define OMAP3430_ST_GPT2_SHIFT				3
#define OMAP3430_ST_GPT2_MASK				(1 << 3)

#define OMAP3430_EN_CORE_SHIFT				0
#define OMAP3430_EN_CORE_MASK				(1 << 0)


#define MAX_MODULE_HARDRESET_WAIT		10000

# ifndef __ASSEMBLER__
extern void __iomem *prm_base;
extern void __iomem *cm_base;
extern void __iomem *cm2_base;

struct omap_prcm_irq {
	const char *name;
	unsigned int offset;
	bool priority;
};

struct omap_prcm_irq_setup {
	u16 ack;
	u16 mask;
	u8 nr_regs;
	u8 nr_irqs;
	const struct omap_prcm_irq *irqs;
	int irq;
	void (*read_pending_irqs)(unsigned long *events);
	void (*ocp_barrier)(void);
	void (*save_and_clear_irqen)(u32 *saved_mask);
	void (*restore_irqen)(u32 *saved_mask);
	u32 *saved_mask;
	u32 *priority_mask;
	int base_irq;
	bool suspended;
	bool suspend_save_flag;
};

#define OMAP_PRCM_IRQ(_name, _offset, _priority) {	\
	.name = _name,					\
	.offset = _offset,				\
	.priority = _priority				\
	}

extern void omap_prcm_irq_cleanup(void);
extern int omap_prcm_register_chain_handler(
	struct omap_prcm_irq_setup *irq_setup);
extern int omap_prcm_event_to_irq(const char *event);
extern void omap_prcm_irq_prepare(void);
extern void omap_prcm_irq_complete(void);

# endif

#endif

