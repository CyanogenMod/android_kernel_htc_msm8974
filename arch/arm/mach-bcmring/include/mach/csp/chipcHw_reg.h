/*****************************************************************************
* Copyright 2004 - 2008 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/

#ifndef CHIPCHW_REG_H
#define CHIPCHW_REG_H

#include <mach/csp/mm_io.h>
#include <csp/reg.h>
#include <mach/csp/ddrcReg.h>

#define chipcHw_BASE_ADDRESS    MM_IO_BASE_CHIPC

typedef struct {
	uint32_t ChipId;	
	uint32_t DDRClock;	
	uint32_t ARMClock;	
	uint32_t ESWClock;	
	uint32_t VPMClock;	
	uint32_t ESW125Clock;	
	uint32_t UARTClock;	
	uint32_t SDIO0Clock;	
	uint32_t SDIO1Clock;	
	uint32_t SPIClock;	
	uint32_t ETMClock;	

	uint32_t ACLKClock;	
	uint32_t OTPClock;	
	uint32_t I2CClock;	
	uint32_t I2S0Clock;	
	uint32_t RTBUSClock;	
	uint32_t pad1;
	uint32_t APM100Clock;	
	uint32_t TSCClock;	
	uint32_t LEDClock;	

	uint32_t USBClock;	
	uint32_t LCDClock;	
	uint32_t APMClock;	

	uint32_t BusIntfClock;	

	uint32_t PLLStatus;	
	uint32_t PLLConfig;	
	uint32_t PLLPreDivider;	
	uint32_t PLLDivider;	
	uint32_t PLLControl1;	
	uint32_t PLLControl2;	

	uint32_t I2S1Clock;	
	uint32_t AudioEnable;	
	uint32_t SoftReset1;	
	uint32_t SoftReset2;	
	uint32_t Spare1;	
	uint32_t Sticky;	
	uint32_t MiscCtrl;	
	uint32_t pad3[3];

	uint32_t PLLStatus2;	
	uint32_t PLLConfig2;	
	uint32_t PLLPreDivider2;	
	uint32_t PLLDivider2;	
	uint32_t PLLControl12;	
	uint32_t PLLControl22;	

	uint32_t DDRPhaseCtrl1;	
	uint32_t VPMPhaseCtrl1;	
	uint32_t PhaseAlignStatus;	
	uint32_t PhaseCtrlStatus;	
	uint32_t DDRPhaseCtrl2;	
	uint32_t VPMPhaseCtrl2;	
	uint32_t pad4[9];

	uint32_t SoftOTP1;	
	uint32_t SoftOTP2;	
	uint32_t SoftStraps;	
	uint32_t PinStraps;	
	uint32_t DiffOscCtrl;	
	uint32_t DiagsCtrl;	
	uint32_t DiagsOutputCtrl;	
	uint32_t DiagsReadBackCtrl;	

	uint32_t LcdPifMode;	

	uint32_t GpioMux_0_7;	
	uint32_t GpioMux_8_15;	
	uint32_t GpioMux_16_23;	
	uint32_t GpioMux_24_31;	
	uint32_t GpioMux_32_39;	
	uint32_t GpioMux_40_47;	
	uint32_t GpioMux_48_55;	
	uint32_t GpioMux_56_63;	

	uint32_t GpioSR_0_7;	
	uint32_t GpioSR_8_15;	
	uint32_t GpioSR_16_23;	
	uint32_t GpioSR_24_31;	
	uint32_t GpioSR_32_39;	
	uint32_t GpioSR_40_47;	
	uint32_t GpioSR_48_55;	
	uint32_t GpioSR_56_63;	
	uint32_t MiscSR_0_7;	
	uint32_t MiscSR_8_15;	

	uint32_t GpioPull_0_15;	
	uint32_t GpioPull_16_31;	
	uint32_t GpioPull_32_47;	
	uint32_t GpioPull_48_63;	
	uint32_t MiscPull_0_15;	

	uint32_t GpioInput_0_31;	
	uint32_t GpioInput_32_63;	
	uint32_t MiscInput_0_15;	
} chipcHw_REG_t;

#define pChipcHw  ((volatile chipcHw_REG_t *) chipcHw_BASE_ADDRESS)
#define pChipcPhysical  ((volatile chipcHw_REG_t *) MM_ADDR_IO_CHIPC)

#define chipcHw_REG_CHIPID_BASE_MASK                    0xFFFFF000
#define chipcHw_REG_CHIPID_BASE_SHIFT                   12
#define chipcHw_REG_CHIPID_REV_MASK                     0x00000FFF
#define chipcHw_REG_REV_A0                              0xA00
#define chipcHw_REG_REV_B0                              0x0B0

#define chipcHw_REG_PLL_STATUS_CONTROL_ENABLE           0x80000000	
#define chipcHw_REG_PLL_STATUS_LOCKED                   0x00000001	
#define chipcHw_REG_PLL_CONFIG_D_RESET                  0x00000008	
#define chipcHw_REG_PLL_CONFIG_A_RESET                  0x00000004	
#define chipcHw_REG_PLL_CONFIG_BYPASS_ENABLE            0x00000020	
#define chipcHw_REG_PLL_CONFIG_OUTPUT_ENABLE            0x00000010	
#define chipcHw_REG_PLL_CONFIG_POWER_DOWN               0x00000001	
#define chipcHw_REG_PLL_CONFIG_VCO_SPLIT_FREQ           1600000000	
#define chipcHw_REG_PLL_CONFIG_VCO_800_1600             0x00000000	
#define chipcHw_REG_PLL_CONFIG_VCO_1601_3200            0x00000080	
#define chipcHw_REG_PLL_CONFIG_TEST_ENABLE              0x00010000	
#define chipcHw_REG_PLL_CONFIG_TEST_SELECT_MASK         0x003E0000	
#define chipcHw_REG_PLL_CONFIG_TEST_SELECT_SHIFT        17

#define chipcHw_REG_PLL_CLOCK_PHASE_COMP                0x00800000	
#define chipcHw_REG_PLL_CLOCK_TO_BUS_RATIO_MASK         0x00300000	
#define chipcHw_REG_PLL_CLOCK_TO_BUS_RATIO_SHIFT        20	
#define chipcHw_REG_PLL_CLOCK_POWER_DOWN                0x00080000	
#define chipcHw_REG_PLL_CLOCK_SOURCE_GPIO               0x00040000	
#define chipcHw_REG_PLL_CLOCK_BYPASS_SELECT             0x00020000	
#define chipcHw_REG_PLL_CLOCK_OUTPUT_ENABLE             0x00010000	
#define chipcHw_REG_PLL_CLOCK_PHASE_UPDATE_ENABLE       0x00008000	
#define chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_SHIFT       8	
#define chipcHw_REG_PLL_CLOCK_PHASE_CONTROL_MASK        0x00003F00	
#define chipcHw_REG_PLL_CLOCK_MDIV_MASK                 0x000000FF	

#define chipcHw_REG_DIV_CLOCK_SOURCE_OTHER              0x00040000	
#define chipcHw_REG_DIV_CLOCK_BYPASS_SELECT             0x00020000	
#define chipcHw_REG_DIV_CLOCK_OUTPUT_ENABLE             0x00010000	
#define chipcHw_REG_DIV_CLOCK_DIV_MASK                  0x000000FF	
#define chipcHw_REG_DIV_CLOCK_DIV_256                   0x00000000	

#define chipcHw_REG_PLL_PREDIVIDER_P1_SHIFT             0
#define chipcHw_REG_PLL_PREDIVIDER_P2_SHIFT             4
#define chipcHw_REG_PLL_PREDIVIDER_NDIV_SHIFT           8
#define chipcHw_REG_PLL_PREDIVIDER_NDIV_MASK            0x0001FF00
#define chipcHw_REG_PLL_PREDIVIDER_POWER_DOWN           0x02000000
#define chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_MASK       0x00700000	
#define chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_INTEGER    0x00000000	
#define chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_MASH_UNIT  0x00100000	
#define chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_MFB_UNIT   0x00200000	
#define chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_MASH_1_8   0x00300000	
#define chipcHw_REG_PLL_PREDIVIDER_NDIV_MODE_MFB_1_8    0x00400000	

#define chipcHw_REG_PLL_PREDIVIDER_NDIV_i(vco)          ((vco) / chipcHw_XTAL_FREQ_Hz)
#define chipcHw_REG_PLL_PREDIVIDER_P1                   1
#define chipcHw_REG_PLL_PREDIVIDER_P2                   1

#define chipcHw_REG_PLL_DIVIDER_M1DIV                   0x03000000
#define chipcHw_REG_PLL_DIVIDER_FRAC                    0x00FFFFFF	

#define chipcHw_REG_PLL_DIVIDER_NDIV_f_SS               (0x00FFFFFF)	

#define chipcHw_REG_PLL_DIVIDER_NDIV_f                  0	

#define chipcHw_REG_PLL_DIVIDER_MDIV(vco, Hz)           ((chipcHw_divide((vco), (Hz)) > 255) ? 0 : chipcHw_divide((vco), (Hz)))

#define chipcHw_REG_ACLKClock_CLK_DIV_MASK              0x3

#define chipcHw_STRAPS_SOFT_OVERRIDE                    0x00000001	

#define chipcHw_STRAPS_BOOT_DEVICE_NAND_FLASH_8         0x00000000	
#define chipcHw_STRAPS_BOOT_DEVICE_NOR_FLASH_16         0x00000002	
#define chipcHw_STRAPS_BOOT_DEVICE_SERIAL_FLASH         0x00000004	
#define chipcHw_STRAPS_BOOT_DEVICE_NAND_FLASH_16        0x00000006	
#define chipcHw_STRAPS_BOOT_DEVICE_UART                 0x00000008	
#define chipcHw_STRAPS_BOOT_DEVICE_MASK                 0x0000000E	

#define chipcHw_STRAPS_BOOT_OPTION_BROM                 0x00000000	
#define chipcHw_STRAPS_BOOT_OPTION_ARAM                 0x00000020	
#define chipcHw_STRAPS_BOOT_OPTION_NOR                  0x00000030	

#define chipcHw_STRAPS_NAND_PAGESIZE_512                0x00000000	
#define chipcHw_STRAPS_NAND_PAGESIZE_2048               0x00000040	
#define chipcHw_STRAPS_NAND_PAGESIZE_4096               0x00000080	
#define chipcHw_STRAPS_NAND_PAGESIZE_EXT                0x000000C0	
#define chipcHw_STRAPS_NAND_PAGESIZE_MASK               0x000000C0	

#define chipcHw_STRAPS_NAND_EXTRA_CYCLE                 0x00000400	
#define chipcHw_STRAPS_REBOOT_TO_UART                   0x00000800	

#define chipcHw_STRAPS_BOOT_MODE_NORMAL                 0x00000000	
#define chipcHw_STRAPS_BOOT_MODE_DBG_SW                 0x00000100	
#define chipcHw_STRAPS_BOOT_MODE_DBG_BOOT               0x00000200	
#define chipcHw_STRAPS_BOOT_MODE_NORMAL_QUIET           0x00000300	
#define chipcHw_STRAPS_BOOT_MODE_MASK                   0x00000300	

#define chipcHw_STRAPS_I2CS                             0x02000000	
#define chipcHw_STRAPS_SPIS                             0x01000000	

#define chipcHw_REG_SW_STRAPS                           ((pChipcHw->PinStraps & 0x0000FC00) >> 10)

#define chipcHw_REG_LCD_PIN_ENABLE                      0x00000001	
#define chipcHw_REG_PIF_PIN_ENABLE                      0x00000002	

#define chipcHw_GPIO_COUNT                              61	

#define chipcHw_REG_GPIO_MUX_KEYPAD                     0x00000001	
#define chipcHw_REG_GPIO_MUX_I2CH                       0x00000002	
#define chipcHw_REG_GPIO_MUX_SPI                        0x00000003	
#define chipcHw_REG_GPIO_MUX_UART                       0x00000004	
#define chipcHw_REG_GPIO_MUX_LEDMTXP                    0x00000005	
#define chipcHw_REG_GPIO_MUX_LEDMTXS                    0x00000006	
#define chipcHw_REG_GPIO_MUX_SDIO0                      0x00000007	
#define chipcHw_REG_GPIO_MUX_SDIO1                      0x00000008	
#define chipcHw_REG_GPIO_MUX_PCM                        0x00000009	
#define chipcHw_REG_GPIO_MUX_I2S                        0x0000000A	
#define chipcHw_REG_GPIO_MUX_ETM                        0x0000000B	
#define chipcHw_REG_GPIO_MUX_DEBUG                      0x0000000C	
#define chipcHw_REG_GPIO_MUX_MISC                       0x0000000D	
#define chipcHw_REG_GPIO_MUX_GPIO                       0x00000000	
#define chipcHw_REG_GPIO_MUX(pin)                       (&pChipcHw->GpioMux_0_7 + ((pin) >> 3))
#define chipcHw_REG_GPIO_MUX_POSITION(pin)              (((pin) & 0x00000007) << 2)
#define chipcHw_REG_GPIO_MUX_MASK                       0x0000000F	

#define chipcHw_REG_SLEW_RATE_HIGH                      0x00000000	
#define chipcHw_REG_SLEW_RATE_NORMAL                    0x00000008	
							
#define chipcHw_REG_SLEW_RATE(pin)                      (((pin) > 42) ? (&pChipcHw->GpioSR_0_7 + (((pin) + 2) >> 3)) : (&pChipcHw->GpioSR_0_7 + ((pin) >> 3)))
#define chipcHw_REG_SLEW_RATE_POSITION(pin)             (((pin) > 42) ? ((((pin) + 2) & 0x00000007) << 2) : (((pin) & 0x00000007) << 2))
#define chipcHw_REG_SLEW_RATE_MASK                      0x00000008	

#define chipcHw_REG_CURRENT_STRENGTH_2mA                0x00000001	
#define chipcHw_REG_CURRENT_STRENGTH_4mA                0x00000002	
#define chipcHw_REG_CURRENT_STRENGTH_6mA                0x00000004	
#define chipcHw_REG_CURRENT_STRENGTH_8mA                0x00000005	
#define chipcHw_REG_CURRENT_STRENGTH_10mA               0x00000006	
#define chipcHw_REG_CURRENT_STRENGTH_12mA               0x00000007	
#define chipcHw_REG_CURRENT_MASK                        0x00000007	
							
#define chipcHw_REG_CURRENT(pin)                        (((pin) > 42) ? (&pChipcHw->GpioSR_0_7 + (((pin) + 2) >> 3)) : (&pChipcHw->GpioSR_0_7 + ((pin) >> 3)))
#define chipcHw_REG_CURRENT_POSITION(pin)               (((pin) > 42) ? ((((pin) + 2) & 0x00000007) << 2) : (((pin) & 0x00000007) << 2))

#define chipcHw_REG_PULL_NONE                           0x00000000	
#define chipcHw_REG_PULL_UP                             0x00000001	
#define chipcHw_REG_PULL_DOWN                           0x00000002	
#define chipcHw_REG_PULLUP_MASK                         0x00000003	
							
#define chipcHw_REG_PULLUP(pin)                         (((pin) > 42) ? (&pChipcHw->GpioPull_0_15 + (((pin) + 2) >> 4)) : (&pChipcHw->GpioPull_0_15 + ((pin) >> 4)))
#define chipcHw_REG_PULLUP_POSITION(pin)                (((pin) > 42) ? ((((pin) + 2) & 0x0000000F) << 1) : (((pin) & 0x0000000F) << 1))

#define chipcHw_REG_INPUTTYPE_CMOS                      0x00000000	
#define chipcHw_REG_INPUTTYPE_ST                        0x00000001	
#define chipcHw_REG_INPUTTYPE_MASK                      0x00000001	
							
#define chipcHw_REG_INPUTTYPE(pin)                      (((pin) > 42) ? (&pChipcHw->GpioInput_0_31 + (((pin) + 2) >> 5)) : (&pChipcHw->GpioInput_0_31 + ((pin) >> 5)))
#define chipcHw_REG_INPUTTYPE_POSITION(pin)             (((pin) > 42) ? ((((pin) + 2) & 0x0000001F)) : (((pin) & 0x0000001F)))

#define chipcHw_REG_BUS_CLOCK_ARM                       0x00000001	
#define chipcHw_REG_BUS_CLOCK_VDEC                      0x00000002	
#define chipcHw_REG_BUS_CLOCK_ARAM                      0x00000004	
#define chipcHw_REG_BUS_CLOCK_HPM                       0x00000008	
#define chipcHw_REG_BUS_CLOCK_DDRC                      0x00000010	
#define chipcHw_REG_BUS_CLOCK_DMAC0                     0x00000020	
#define chipcHw_REG_BUS_CLOCK_DMAC1                     0x00000040	
#define chipcHw_REG_BUS_CLOCK_NVI                       0x00000080	
#define chipcHw_REG_BUS_CLOCK_ESW                       0x00000100	
#define chipcHw_REG_BUS_CLOCK_GE                        0x00000200	
#define chipcHw_REG_BUS_CLOCK_I2CH                      0x00000400	
#define chipcHw_REG_BUS_CLOCK_I2S0                      0x00000800	
#define chipcHw_REG_BUS_CLOCK_I2S1                      0x00001000	
#define chipcHw_REG_BUS_CLOCK_VRAM                      0x00002000	
#define chipcHw_REG_BUS_CLOCK_CLCD                      0x00004000	
#define chipcHw_REG_BUS_CLOCK_LDK                       0x00008000	
#define chipcHw_REG_BUS_CLOCK_LED                       0x00010000	
#define chipcHw_REG_BUS_CLOCK_OTP                       0x00020000	
#define chipcHw_REG_BUS_CLOCK_PIF                       0x00040000	
#define chipcHw_REG_BUS_CLOCK_SPU                       0x00080000	
#define chipcHw_REG_BUS_CLOCK_SDIO0                     0x00100000	
#define chipcHw_REG_BUS_CLOCK_SDIO1                     0x00200000	
#define chipcHw_REG_BUS_CLOCK_SPIH                      0x00400000	
#define chipcHw_REG_BUS_CLOCK_SPIS                      0x00800000	
#define chipcHw_REG_BUS_CLOCK_UART0                     0x01000000	
#define chipcHw_REG_BUS_CLOCK_UART1                     0x02000000	
#define chipcHw_REG_BUS_CLOCK_BBL                       0x04000000	
#define chipcHw_REG_BUS_CLOCK_I2CS                      0x08000000	
#define chipcHw_REG_BUS_CLOCK_USBH                      0x10000000	
#define chipcHw_REG_BUS_CLOCK_USBD                      0x20000000	
#define chipcHw_REG_BUS_CLOCK_BROM                      0x40000000	
#define chipcHw_REG_BUS_CLOCK_TSC                       0x80000000	

#define chipcHw_REG_SOFT_RESET_VPM_GLOBAL_HOLD          0x0000000080000000ULL	
#define chipcHw_REG_SOFT_RESET_VPM_HOLD                 0x0000000040000000ULL	
#define chipcHw_REG_SOFT_RESET_VPM_GLOBAL               0x0000000020000000ULL	
#define chipcHw_REG_SOFT_RESET_VPM                      0x0000000010000000ULL	
#define chipcHw_REG_SOFT_RESET_KEYPAD                   0x0000000008000000ULL	
#define chipcHw_REG_SOFT_RESET_LED                      0x0000000004000000ULL	
#define chipcHw_REG_SOFT_RESET_SPU                      0x0000000002000000ULL	
#define chipcHw_REG_SOFT_RESET_RNG                      0x0000000001000000ULL	
#define chipcHw_REG_SOFT_RESET_PKA                      0x0000000000800000ULL	
#define chipcHw_REG_SOFT_RESET_LCD                      0x0000000000400000ULL	
#define chipcHw_REG_SOFT_RESET_PIF                      0x0000000000200000ULL	
#define chipcHw_REG_SOFT_RESET_I2CS                     0x0000000000100000ULL	
#define chipcHw_REG_SOFT_RESET_I2CH                     0x0000000000080000ULL	
#define chipcHw_REG_SOFT_RESET_SDIO1                    0x0000000000040000ULL	
#define chipcHw_REG_SOFT_RESET_SDIO0                    0x0000000000020000ULL	
#define chipcHw_REG_SOFT_RESET_BBL                      0x0000000000010000ULL	
#define chipcHw_REG_SOFT_RESET_I2S1                     0x0000000000008000ULL	
#define chipcHw_REG_SOFT_RESET_I2S0                     0x0000000000004000ULL	
#define chipcHw_REG_SOFT_RESET_SPIS                     0x0000000000002000ULL	
#define chipcHw_REG_SOFT_RESET_SPIH                     0x0000000000001000ULL	
#define chipcHw_REG_SOFT_RESET_GPIO1                    0x0000000000000800ULL	
#define chipcHw_REG_SOFT_RESET_GPIO0                    0x0000000000000400ULL	
#define chipcHw_REG_SOFT_RESET_UART1                    0x0000000000000200ULL	
#define chipcHw_REG_SOFT_RESET_UART0                    0x0000000000000100ULL	
#define chipcHw_REG_SOFT_RESET_NVI                      0x0000000000000080ULL	
#define chipcHw_REG_SOFT_RESET_WDOG                     0x0000000000000040ULL	
#define chipcHw_REG_SOFT_RESET_TMR                      0x0000000000000020ULL	
#define chipcHw_REG_SOFT_RESET_ETM                      0x0000000000000010ULL	
#define chipcHw_REG_SOFT_RESET_ARM_HOLD                 0x0000000000000008ULL	
#define chipcHw_REG_SOFT_RESET_ARM                      0x0000000000000004ULL	
#define chipcHw_REG_SOFT_RESET_CHIP_WARM                0x0000000000000002ULL	
#define chipcHw_REG_SOFT_RESET_CHIP_SOFT                0x0000000000000001ULL	
#define chipcHw_REG_SOFT_RESET_VDEC                     0x0000100000000000ULL	
#define chipcHw_REG_SOFT_RESET_GE                       0x0000080000000000ULL	
#define chipcHw_REG_SOFT_RESET_OTP                      0x0000040000000000ULL	
#define chipcHw_REG_SOFT_RESET_USB2                     0x0000020000000000ULL	
#define chipcHw_REG_SOFT_RESET_USB1                     0x0000010000000000ULL	
#define chipcHw_REG_SOFT_RESET_USB                      0x0000008000000000ULL	
#define chipcHw_REG_SOFT_RESET_ESW                      0x0000004000000000ULL	
#define chipcHw_REG_SOFT_RESET_ESWCLK                   0x0000002000000000ULL	
#define chipcHw_REG_SOFT_RESET_DDRPHY                   0x0000001000000000ULL	
#define chipcHw_REG_SOFT_RESET_DDR                      0x0000000800000000ULL	
#define chipcHw_REG_SOFT_RESET_TSC                      0x0000000400000000ULL	
#define chipcHw_REG_SOFT_RESET_PCM                      0x0000000200000000ULL	
#define chipcHw_REG_SOFT_RESET_APM                      0x0000200100000000ULL	

#define chipcHw_REG_SOFT_RESET_VPM_GLOBAL_UNHOLD        0x8000000000000000ULL	
#define chipcHw_REG_SOFT_RESET_VPM_UNHOLD               0x4000000000000000ULL	
#define chipcHw_REG_SOFT_RESET_ARM_UNHOLD               0x2000000000000000ULL	
#define chipcHw_REG_SOFT_RESET_UNHOLD_MASK              0xF000000000000000ULL	

#define chipcHw_REG_AUDIO_CHANNEL_ENABLE_ALL            0x00000001	
#define chipcHw_REG_AUDIO_CHANNEL_ENABLE_A              0x00000002	
#define chipcHw_REG_AUDIO_CHANNEL_ENABLE_B              0x00000004	
#define chipcHw_REG_AUDIO_CHANNEL_ENABLE_C              0x00000008	
#define chipcHw_REG_AUDIO_CHANNEL_ENABLE_NTP_CLOCK      0x00000010	
#define chipcHw_REG_AUDIO_CHANNEL_ENABLE_PCM0_CLOCK     0x00000020	
#define chipcHw_REG_AUDIO_CHANNEL_ENABLE_PCM1_CLOCK     0x00000040	
#define chipcHw_REG_AUDIO_CHANNEL_ENABLE_APM_CLOCK      0x00000080	

#define chipcHw_REG_MISC_CTRL_GE_SEL                    0x00040000	
#define chipcHw_REG_MISC_CTRL_I2S1_CLOCK_ONCHIP         0x00000000	
#define chipcHw_REG_MISC_CTRL_I2S1_CLOCK_GPIO           0x00020000	
#define chipcHw_REG_MISC_CTRL_I2S0_CLOCK_ONCHIP         0x00000000	
#define chipcHw_REG_MISC_CTRL_I2S0_CLOCK_GPIO           0x00010000	
#define chipcHw_REG_MISC_CTRL_ARM_CP15_DISABLE          0x00008000	
#define chipcHw_REG_MISC_CTRL_RTC_DISABLE               0x00000008	
#define chipcHw_REG_MISC_CTRL_BBRAM_DISABLE             0x00000004	
#define chipcHw_REG_MISC_CTRL_USB_MODE_HOST             0x00000002	
#define chipcHw_REG_MISC_CTRL_USB_MODE_DEVICE           0xFFFFFFFD	
#define chipcHw_REG_MISC_CTRL_USB_POWERON               0xFFFFFFFE	
#define chipcHw_REG_MISC_CTRL_USB_POWEROFF              0x00000001	

#define chipcHw_REG_OTP_SECURITY_OFF                    0x0000020000000000ULL	
#define chipcHw_REG_OTP_SPU_SLOW                        0x0000010000000000ULL	
#define chipcHw_REG_OTP_LCD_SPEED                       0x0000000600000000ULL	
#define chipcHw_REG_OTP_VPM_SPEED_1                     0x0000000100000000ULL	
#define chipcHw_REG_OTP_VPM_SPEED_0                     0x0000000080000000ULL	
#define chipcHw_REG_OTP_AXI_SPEED                       0x0000000060000000ULL	
#define chipcHw_REG_OTP_APM_DISABLE                     0x000000001F000000ULL	
#define chipcHw_REG_OTP_PIF_DISABLE                     0x0000000000200000ULL	
#define chipcHw_REG_OTP_VDEC_DISABLE                    0x0000000000100000ULL	
#define chipcHw_REG_OTP_BBL_DISABLE                     0x0000000000080000ULL	
#define chipcHw_REG_OTP_LED_DISABLE                     0x0000000000040000ULL	
#define chipcHw_REG_OTP_GE_DISABLE                      0x0000000000020000ULL	
#define chipcHw_REG_OTP_LCD_DISABLE                     0x0000000000010000ULL	
#define chipcHw_REG_OTP_KEYPAD_DISABLE                  0x0000000000008000ULL	
#define chipcHw_REG_OTP_UART_DISABLE                    0x0000000000004000ULL	
#define chipcHw_REG_OTP_SDIOH_DISABLE                   0x0000000000003000ULL	
#define chipcHw_REG_OTP_HSS_DISABLE                     0x0000000000000C00ULL	
#define chipcHw_REG_OTP_TSC_DISABLE                     0x0000000000000200ULL	
#define chipcHw_REG_OTP_USB_DISABLE                     0x0000000000000180ULL	
#define chipcHw_REG_OTP_SGMII_DISABLE                   0x0000000000000060ULL	
#define chipcHw_REG_OTP_ETH_DISABLE                     0x0000000000000018ULL	
#define chipcHw_REG_OTP_ETH_PHY_DISABLE                 0x0000000000000006ULL	
#define chipcHw_REG_OTP_VPM_DISABLE                     0x0000000000000001ULL	

#define chipcHw_REG_STICKY_BOOT_DONE                    0x00000001	
#define chipcHw_REG_STICKY_SOFT_RESET                   0x00000002	
#define chipcHw_REG_STICKY_GENERAL_1                    0x00000004	
#define chipcHw_REG_STICKY_GENERAL_2                    0x00000008	
#define chipcHw_REG_STICKY_GENERAL_3                    0x00000010	
#define chipcHw_REG_STICKY_GENERAL_4                    0x00000020	
#define chipcHw_REG_STICKY_GENERAL_5                    0x00000040	
#define chipcHw_REG_STICKY_POR_BROM                     0x00000080	
#define chipcHw_REG_STICKY_ARM_RESET                    0x00000100	
#define chipcHw_REG_STICKY_CHIP_SOFT_RESET              0x00000200	
#define chipcHw_REG_STICKY_CHIP_WARM_RESET              0x00000400	
#define chipcHw_REG_STICKY_WDOG_RESET                   0x00000800	
#define chipcHw_REG_STICKY_OTP_RESET                    0x00001000	

							
#define chipcHw_REG_SPARE1_DDR_PHASE_INTR_ENABLE        0x80000000	
#define chipcHw_REG_SPARE1_VPM_PHASE_INTR_ENABLE        0x40000000	
#define chipcHw_REG_SPARE1_VPM_BUS_ACCESS_ENABLE        0x00000002	
#define chipcHw_REG_SPARE1_DDR_BUS_ACCESS_ENABLE        0x00000001	
							
#define chipcHw_REG_DDR_SW_PHASE_CTRL_ENABLE            0x80000000	
#define chipcHw_REG_DDR_HW_PHASE_CTRL_ENABLE            0x40000000	
#define chipcHw_REG_DDR_PHASE_VALUE_GE_MASK             0x0000007F	
#define chipcHw_REG_DDR_PHASE_VALUE_GE_SHIFT            23
#define chipcHw_REG_DDR_PHASE_VALUE_LE_MASK             0x0000007F	
#define chipcHw_REG_DDR_PHASE_VALUE_LE_SHIFT            16
#define chipcHw_REG_DDR_PHASE_ALIGN_WAIT_CYCLE_MASK     0x0000FFFF	
#define chipcHw_REG_DDR_PHASE_ALIGN_WAIT_CYCLE_SHIFT    0
							
#define chipcHw_REG_VPM_SW_PHASE_CTRL_ENABLE            0x80000000	
#define chipcHw_REG_VPM_HW_PHASE_CTRL_ENABLE            0x40000000	
#define chipcHw_REG_VPM_PHASE_VALUE_GE_MASK             0x0000007F	
#define chipcHw_REG_VPM_PHASE_VALUE_GE_SHIFT            23
#define chipcHw_REG_VPM_PHASE_VALUE_LE_MASK             0x0000007F	
#define chipcHw_REG_VPM_PHASE_VALUE_LE_SHIFT            16
#define chipcHw_REG_VPM_PHASE_ALIGN_WAIT_CYCLE_MASK     0x0000FFFF	
#define chipcHw_REG_VPM_PHASE_ALIGN_WAIT_CYCLE_SHIFT    0
							
#define chipcHw_REG_DDR_TIMEOUT_INTR_STATUS             0x80000000	
#define chipcHw_REG_DDR_PHASE_STATUS_MASK               0x0000007F	
#define chipcHw_REG_DDR_PHASE_STATUS_SHIFT              24
#define chipcHw_REG_DDR_PHASE_ALIGNED                   0x00800000	
#define chipcHw_REG_DDR_LOAD                            0x00400000	
#define chipcHw_REG_DDR_PHASE_CTRL_MASK                 0x0000003F	
#define chipcHw_REG_DDR_PHASE_CTRL_SHIFT                16
#define chipcHw_REG_VPM_TIMEOUT_INTR_STATUS             0x80000000	
#define chipcHw_REG_VPM_PHASE_STATUS_MASK               0x0000007F	
#define chipcHw_REG_VPM_PHASE_STATUS_SHIFT              8
#define chipcHw_REG_VPM_PHASE_ALIGNED                   0x00000080	
#define chipcHw_REG_VPM_LOAD                            0x00000040	
#define chipcHw_REG_VPM_PHASE_CTRL_MASK                 0x0000003F	
#define chipcHw_REG_VPM_PHASE_CTRL_SHIFT                0
							
#define chipcHw_REG_DDR_INTR_SERVICED                   0x02000000	
#define chipcHw_REG_DDR_TIMEOUT_INTR_ENABLE             0x01000000	
#define chipcHw_REG_DDR_LOAD_COUNT_PHASE_CTRL_MASK      0x0000000F	
#define chipcHw_REG_DDR_LOAD_COUNT_PHASE_CTRL_SHIFT     20
#define chipcHw_REG_DDR_TOTAL_LOAD_COUNT_CTRL_MASK      0x0000000F	
#define chipcHw_REG_DDR_TOTAL_LOAD_COUNT_CTRL_SHIFT     16
#define chipcHw_REG_DDR_PHASE_TIMEOUT_COUNT_MASK        0x0000FFFF	
#define chipcHw_REG_DDR_PHASE_TIMEOUT_COUNT_SHIFT       0
							
#define chipcHw_REG_VPM_INTR_SELECT_MASK                0x00000003	
#define chipcHw_REG_VPM_INTR_SELECT_SHIFT               26
#define chipcHw_REG_VPM_INTR_DISABLE                    0x00000000
#define chipcHw_REG_VPM_INTR_FAST                       (0x1 << chipcHw_REG_VPM_INTR_SELECT_SHIFT)
#define chipcHw_REG_VPM_INTR_MEDIUM                     (0x2 << chipcHw_REG_VPM_INTR_SELECT_SHIFT)
#define chipcHw_REG_VPM_INTR_SLOW                       (0x3 << chipcHw_REG_VPM_INTR_SELECT_SHIFT)
#define chipcHw_REG_VPM_INTR_SERVICED                   0x02000000	
#define chipcHw_REG_VPM_TIMEOUT_INTR_ENABLE             0x01000000	
#define chipcHw_REG_VPM_LOAD_COUNT_PHASE_CTRL_MASK      0x0000000F	
#define chipcHw_REG_VPM_LOAD_COUNT_PHASE_CTRL_SHIFT     20
#define chipcHw_REG_VPM_TOTAL_LOAD_COUNT_CTRL_MASK      0x0000000F	
#define chipcHw_REG_VPM_TOTAL_LOAD_COUNT_CTRL_SHIFT     16
#define chipcHw_REG_VPM_PHASE_TIMEOUT_COUNT_MASK        0x0000FFFF	
#define chipcHw_REG_VPM_PHASE_TIMEOUT_COUNT_SHIFT       0

#endif 
