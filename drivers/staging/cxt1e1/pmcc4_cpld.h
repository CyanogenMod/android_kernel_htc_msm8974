#ifndef _INC_PMCC4_CPLD_H_
#define _INC_PMCC4_CPLD_H_

/*-----------------------------------------------------------------------------
 * pmcc4_cpld.h -
 *
 * Copyright (C) 2005  SBE, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 * For further information, contact via email: support@sbei.com
 * SBE, Inc.  San Ramon, California  U.S.A.
 *-----------------------------------------------------------------------------
 */

#include <linux/types.h>


#if 0
#define CPLD_MCSR    0x0
#define CPLD_MCLK    0x1
#define CPLD_LEDS    0x2
#define CPLD_INTR    0x3
#endif

    struct c4_cpld
    {
        volatile u_int32_t mcsr;
        volatile u_int32_t mclk;
        volatile u_int32_t leds;
        volatile u_int32_t intr;
    };

    typedef struct c4_cpld c4cpld_t;

#define PMCC4_CPLD_MCSR_IND     0       
#define PMCC4_CPLD_MCSR_CMT_1   1       
#define PMCC4_CPLD_MCSR_CMT_2   2       
#define PMCC4_CPLD_MCSR_CMT_3   3       
#define PMCC4_CPLD_MCSR_CMT_4   4       

#define PMCC4_CPLD_MCLK_MASK    0x0f
#define PMCC4_CPLD_MCLK_P1      0x1
#define PMCC4_CPLD_MCLK_P2      0x2
#define PMCC4_CPLD_MCLK_P3      0x4
#define PMCC4_CPLD_MCLK_P4      0x8
#define PMCC4_CPLD_MCLK_T1      0x00
#define PMCC4_CPLD_MCLK_P1_E1   0x01
#define PMCC4_CPLD_MCLK_P2_E1   0x02
#define PMCC4_CPLD_MCLK_P3_E1   0x04
#define PMCC4_CPLD_MCLK_P4_E1   0x08

#define PMCC4_CPLD_LED_OFF      0
#define PMCC4_CPLD_LED_ON       1
#define PMCC4_CPLD_LED_GP0      0x01    
#define PMCC4_CPLD_LED_YP0      0x02    
#define PMCC4_CPLD_LED_GP1      0x04    
#define PMCC4_CPLD_LED_YP1      0x08    
#define PMCC4_CPLD_LED_GP2      0x10    
#define PMCC4_CPLD_LED_YP2      0x20    
#define PMCC4_CPLD_LED_GP3      0x40    
#define PMCC4_CPLD_LED_YP3      0x80    
#define PMCC4_CPLD_LED_GREEN   (PMCC4_CPLD_LED_GP0 | PMCC4_CPLD_LED_GP1 | \
                                PMCC4_CPLD_LED_GP2 | PMCC4_CPLD_LED_GP3 )
#define PMCC4_CPLD_LED_YELLOW  (PMCC4_CPLD_LED_YP0 | PMCC4_CPLD_LED_YP1 | \
                                PMCC4_CPLD_LED_YP2 | PMCC4_CPLD_LED_YP3)

#define PMCC4_CPLD_INTR_MASK    0x0f
#define PMCC4_CPLD_INTR_CMT_1   0x01
#define PMCC4_CPLD_INTR_CMT_2   0x02
#define PMCC4_CPLD_INTR_CMT_3   0x04
#define PMCC4_CPLD_INTR_CMT_4   0x08

#endif                          
