/*****************************************************************************
* Copyright 2003 - 2008 Broadcom Corporation.  All rights reserved.
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


#ifndef CSP_HW_CFG_H
#define CSP_HW_CFG_H


#include <cfg_global.h>
#include <mach/csp/cap_inline.h>

#if defined(__KERNEL__)
#include <mach/memory_settings.h>
#else
#include <hw_cfg.h>
#endif


#if defined(CFG_GLOBAL_CHIP)
  #if (CFG_GLOBAL_CHIP == FPGA11107)
     #define HW_CFG_BUS_CLK_HZ            5000000
     #define HW_CFG_DDR_CTLR_CLK_HZ      10000000
     #define HW_CFG_DDR_PHY_OMIT
     #define HW_CFG_UART_CLK_HZ           7500000
  #else
     #define HW_CFG_PLL_VCO_HZ           2000000000
     #define HW_CFG_PLL2_VCO_HZ          1800000000
     #define HW_CFG_ARM_CLK_HZ            CAP_HW_CFG_ARM_CLK_HZ
     #define HW_CFG_BUS_CLK_HZ            166666666
     #define HW_CFG_DDR_CTLR_CLK_HZ       333333333
     #define HW_CFG_DDR_PHY_CLK_HZ        (2 * HW_CFG_DDR_CTLR_CLK_HZ)
     #define HW_CFG_UART_CLK_HZ           142857142
     #define HW_CFG_VPM_CLK_HZ            CAP_HW_CFG_VPM_CLK_HZ
  #endif
#else
   #define HW_CFG_PLL_VCO_HZ           1800000000
   #define HW_CFG_PLL2_VCO_HZ          1800000000
   #define HW_CFG_ARM_CLK_HZ            450000000
   #define HW_CFG_BUS_CLK_HZ            150000000
   #define HW_CFG_DDR_CTLR_CLK_HZ       300000000
   #define HW_CFG_DDR_PHY_CLK_HZ        (2 * HW_CFG_DDR_CTLR_CLK_HZ)
   #define HW_CFG_UART_CLK_HZ           150000000
   #define HW_CFG_VPM_CLK_HZ            300000000
#endif



#endif 

