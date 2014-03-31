/*
 * arch/arm/mach-pxa/include/mach/pcm027.h
 *
 * (c) 2003 Phytec Messtechnik GmbH <armlinux@phytec.de>
 * (c) 2007 Juergen Beisert <j.beisert@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#define PCM027_IRQ(x)          (IRQ_BOARD_START + (x))
#define PCM027_BTDET_IRQ       PCM027_IRQ(0)
#define PCM027_FF_RI_IRQ       PCM027_IRQ(1)
#define PCM027_MMCDET_IRQ      PCM027_IRQ(2)
#define PCM027_PM_5V_IRQ       PCM027_IRQ(3)

#define PCM027_NR_IRQS		(IRQ_BOARD_START + 32)

#define PCM027_RTC_IRQ_GPIO	0
#define PCM027_RTC_IRQ		PXA_GPIO_TO_IRQ(PCM027_RTC_IRQ_GPIO)
#define PCM027_RTC_IRQ_EDGE	IRQ_TYPE_EDGE_FALLING
#define ADR_PCM027_RTC		0x51	

#define ADR_PCM027_EEPROM	0x54	

#define PCM027_ETH_IRQ_GPIO	52
#define PCM027_ETH_IRQ		PXA_GPIO_TO_IRQ(PCM027_ETH_IRQ_GPIO)
#define PCM027_ETH_IRQ_EDGE	IRQ_TYPE_EDGE_RISING
#define PCM027_ETH_PHYS		PXA_CS5_PHYS
#define PCM027_ETH_SIZE		(1*1024*1024)

#define PCM027_CAN_IRQ_GPIO	114
#define PCM027_CAN_IRQ		PXA_GPIO_TO_IRQ(PCM027_CAN_IRQ_GPIO)
#define PCM027_CAN_IRQ_EDGE	IRQ_TYPE_EDGE_FALLING
#define PCM027_CAN_PHYS		0x22000000
#define PCM027_CAN_SIZE		0x100

#define PCM027_EGPIO_IRQ_GPIO	27
#define PCM027_EGPIO_IRQ	PXA_GPIO_TO_IRQ(PCM027_EGPIO_IRQ_GPIO)
#define PCM027_EGPIO_IRQ_EDGE	IRQ_TYPE_EDGE_FALLING
#define PCM027_EGPIO_CS		24
#define PCM027_EGPIO_CS_MODE	GPIO24_SFRM_MD

#define PCM027_FLASH_PHYS	0x00000000
#define PCM027_FLASH_SIZE	0x02000000

#define PCM027_LED_CPU		90
#define PCM027_LED_HEARD_BEAT	91

extern void pcm990_baseboard_init(void);
