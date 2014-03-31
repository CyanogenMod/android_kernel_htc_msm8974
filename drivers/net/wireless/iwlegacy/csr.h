/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2005 - 2011 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2011 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#ifndef __il_csr_h__
#define __il_csr_h__
#define CSR_BASE    (0x000)

#define CSR_HW_IF_CONFIG_REG    (CSR_BASE+0x000)	
#define CSR_INT_COALESCING      (CSR_BASE+0x004)	
#define CSR_INT                 (CSR_BASE+0x008)	
#define CSR_INT_MASK            (CSR_BASE+0x00c)	
#define CSR_FH_INT_STATUS       (CSR_BASE+0x010)	
#define CSR_GPIO_IN             (CSR_BASE+0x018)	
#define CSR_RESET               (CSR_BASE+0x020)	
#define CSR_GP_CNTRL            (CSR_BASE+0x024)

#define CSR_INT_PERIODIC_REG	(CSR_BASE+0x005)

#define CSR_HW_REV              (CSR_BASE+0x028)

#define CSR_EEPROM_REG          (CSR_BASE+0x02c)
#define CSR_EEPROM_GP           (CSR_BASE+0x030)

#define CSR_GIO_REG		(CSR_BASE+0x03C)
#define CSR_GP_UCODE_REG	(CSR_BASE+0x048)
#define CSR_GP_DRIVER_REG	(CSR_BASE+0x050)

/*
 * UCODE-DRIVER GP (general purpose) mailbox registers.
 * SET/CLR registers set/clear bit(s) if "1" is written.
 */
#define CSR_UCODE_DRV_GP1       (CSR_BASE+0x054)
#define CSR_UCODE_DRV_GP1_SET   (CSR_BASE+0x058)
#define CSR_UCODE_DRV_GP1_CLR   (CSR_BASE+0x05c)
#define CSR_UCODE_DRV_GP2       (CSR_BASE+0x060)

#define CSR_LED_REG             (CSR_BASE+0x094)
#define CSR_DRAM_INT_TBL_REG	(CSR_BASE+0x0A0)

#define CSR_GIO_CHICKEN_BITS    (CSR_BASE+0x100)

#define CSR_ANA_PLL_CFG         (CSR_BASE+0x20c)

#define CSR_HW_REV_WA_REG		(CSR_BASE+0x22C)

#define CSR_DBG_HPET_MEM_REG		(CSR_BASE+0x240)
#define CSR_DBG_LINK_PWR_MGMT_REG	(CSR_BASE+0x250)

#define CSR49_HW_IF_CONFIG_REG_BIT_4965_R	(0x00000010)
#define CSR_HW_IF_CONFIG_REG_MSK_BOARD_VER	(0x00000C00)
#define CSR_HW_IF_CONFIG_REG_BIT_MAC_SI 	(0x00000100)
#define CSR_HW_IF_CONFIG_REG_BIT_RADIO_SI	(0x00000200)

#define CSR39_HW_IF_CONFIG_REG_BIT_3945_MB         (0x00000100)
#define CSR39_HW_IF_CONFIG_REG_BIT_3945_MM         (0x00000200)
#define CSR39_HW_IF_CONFIG_REG_BIT_SKU_MRC            (0x00000400)
#define CSR39_HW_IF_CONFIG_REG_BIT_BOARD_TYPE         (0x00000800)
#define CSR39_HW_IF_CONFIG_REG_BITS_SILICON_TYPE_A    (0x00000000)
#define CSR39_HW_IF_CONFIG_REG_BITS_SILICON_TYPE_B    (0x00001000)

#define CSR_HW_IF_CONFIG_REG_BIT_HAP_WAKE_L1A	(0x00080000)
#define CSR_HW_IF_CONFIG_REG_BIT_EEPROM_OWN_SEM	(0x00200000)
#define CSR_HW_IF_CONFIG_REG_BIT_NIC_READY	(0x00400000)	
#define CSR_HW_IF_CONFIG_REG_BIT_NIC_PREPARE_DONE (0x02000000)	
#define CSR_HW_IF_CONFIG_REG_PREPARE		  (0x08000000)	

#define CSR_INT_PERIODIC_DIS			(0x00)	
#define CSR_INT_PERIODIC_ENA			(0xFF)	

#define CSR_INT_BIT_FH_RX        (1 << 31)	
#define CSR_INT_BIT_HW_ERR       (1 << 29)	
#define CSR_INT_BIT_RX_PERIODIC	 (1 << 28)	
#define CSR_INT_BIT_FH_TX        (1 << 27)	
#define CSR_INT_BIT_SCD          (1 << 26)	
#define CSR_INT_BIT_SW_ERR       (1 << 25)	
#define CSR_INT_BIT_RF_KILL      (1 << 7)	
#define CSR_INT_BIT_CT_KILL      (1 << 6)	
#define CSR_INT_BIT_SW_RX        (1 << 3)	
#define CSR_INT_BIT_WAKEUP       (1 << 1)	
#define CSR_INT_BIT_ALIVE        (1 << 0)	

#define CSR_INI_SET_MASK	(CSR_INT_BIT_FH_RX   | \
				 CSR_INT_BIT_HW_ERR  | \
				 CSR_INT_BIT_FH_TX   | \
				 CSR_INT_BIT_SW_ERR  | \
				 CSR_INT_BIT_RF_KILL | \
				 CSR_INT_BIT_SW_RX   | \
				 CSR_INT_BIT_WAKEUP  | \
				 CSR_INT_BIT_ALIVE)

#define CSR_FH_INT_BIT_ERR       (1 << 31)	
#define CSR_FH_INT_BIT_HI_PRIOR  (1 << 30)	
#define CSR39_FH_INT_BIT_RX_CHNL2  (1 << 18)	
#define CSR_FH_INT_BIT_RX_CHNL1  (1 << 17)	
#define CSR_FH_INT_BIT_RX_CHNL0  (1 << 16)	
#define CSR39_FH_INT_BIT_TX_CHNL6  (1 << 6)	
#define CSR_FH_INT_BIT_TX_CHNL1  (1 << 1)	
#define CSR_FH_INT_BIT_TX_CHNL0  (1 << 0)	

#define CSR39_FH_INT_RX_MASK	(CSR_FH_INT_BIT_HI_PRIOR | \
				 CSR39_FH_INT_BIT_RX_CHNL2 | \
				 CSR_FH_INT_BIT_RX_CHNL1 | \
				 CSR_FH_INT_BIT_RX_CHNL0)

#define CSR39_FH_INT_TX_MASK	(CSR39_FH_INT_BIT_TX_CHNL6 | \
				 CSR_FH_INT_BIT_TX_CHNL1 | \
				 CSR_FH_INT_BIT_TX_CHNL0)

#define CSR49_FH_INT_RX_MASK	(CSR_FH_INT_BIT_HI_PRIOR | \
				 CSR_FH_INT_BIT_RX_CHNL1 | \
				 CSR_FH_INT_BIT_RX_CHNL0)

#define CSR49_FH_INT_TX_MASK	(CSR_FH_INT_BIT_TX_CHNL1 | \
				 CSR_FH_INT_BIT_TX_CHNL0)

#define CSR_GPIO_IN_BIT_AUX_POWER                   (0x00000200)
#define CSR_GPIO_IN_VAL_VAUX_PWR_SRC                (0x00000000)
#define CSR_GPIO_IN_VAL_VMAIN_PWR_SRC               (0x00000200)

#define CSR_RESET_REG_FLAG_NEVO_RESET                (0x00000001)
#define CSR_RESET_REG_FLAG_FORCE_NMI                 (0x00000002)
#define CSR_RESET_REG_FLAG_SW_RESET                  (0x00000080)
#define CSR_RESET_REG_FLAG_MASTER_DISABLED           (0x00000100)
#define CSR_RESET_REG_FLAG_STOP_MASTER               (0x00000200)
#define CSR_RESET_LINK_PWR_MGMT_DISABLED             (0x80000000)

#define CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY        (0x00000001)
#define CSR_GP_CNTRL_REG_FLAG_INIT_DONE              (0x00000004)
#define CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ         (0x00000008)
#define CSR_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP         (0x00000010)

#define CSR_GP_CNTRL_REG_VAL_MAC_ACCESS_EN           (0x00000001)

#define CSR_GP_CNTRL_REG_MSK_POWER_SAVE_TYPE         (0x07000000)
#define CSR_GP_CNTRL_REG_FLAG_MAC_POWER_SAVE         (0x04000000)
#define CSR_GP_CNTRL_REG_FLAG_HW_RF_KILL_SW          (0x08000000)

#define CSR_EEPROM_REG_READ_VALID_MSK	(0x00000001)
#define CSR_EEPROM_REG_BIT_CMD		(0x00000002)
#define CSR_EEPROM_REG_MSK_ADDR		(0x0000FFFC)
#define CSR_EEPROM_REG_MSK_DATA		(0xFFFF0000)

#define CSR_EEPROM_GP_VALID_MSK		(0x00000007)	
#define CSR_EEPROM_GP_IF_OWNER_MSK	(0x00000180)
#define CSR_EEPROM_GP_GOOD_SIG_EEP_LESS_THAN_4K		(0x00000002)
#define CSR_EEPROM_GP_GOOD_SIG_EEP_MORE_THAN_4K		(0x00000004)

#define CSR_GP_REG_POWER_SAVE_STATUS_MSK            (0x03000000)	
#define CSR_GP_REG_NO_POWER_SAVE            (0x00000000)
#define CSR_GP_REG_MAC_POWER_SAVE           (0x01000000)
#define CSR_GP_REG_PHY_POWER_SAVE           (0x02000000)
#define CSR_GP_REG_POWER_SAVE_ERROR         (0x03000000)

#define CSR_GIO_REG_VAL_L0S_ENABLED	(0x00000002)

#define CSR_UCODE_DRV_GP1_BIT_MAC_SLEEP             (0x00000001)
#define CSR_UCODE_SW_BIT_RFKILL                     (0x00000002)
#define CSR_UCODE_DRV_GP1_BIT_CMD_BLOCKED           (0x00000004)
#define CSR_UCODE_DRV_GP1_REG_BIT_CT_KILL_EXIT      (0x00000008)

#define CSR_GIO_CHICKEN_BITS_REG_BIT_L1A_NO_L0S_RX  (0x00800000)
#define CSR_GIO_CHICKEN_BITS_REG_BIT_DIS_L0S_EXIT_TIMER  (0x20000000)

#define CSR_LED_BSM_CTRL_MSK (0xFFFFFFDF)
#define CSR_LED_REG_TRUN_ON (0x78)
#define CSR_LED_REG_TRUN_OFF (0x38)

#define CSR39_ANA_PLL_CFG_VAL        (0x01000000)

#define CSR_DBG_HPET_MEM_REG_VAL	(0xFFFF0000)

#define CSR_DRAM_INT_TBL_ENABLE		(1 << 31)
#define CSR_DRAM_INIT_TBL_WRAP_CHECK	(1 << 27)

#define HBUS_BASE	(0x400)

#define HBUS_TARG_MEM_RADDR     (HBUS_BASE+0x00c)
#define HBUS_TARG_MEM_WADDR     (HBUS_BASE+0x010)
#define HBUS_TARG_MEM_WDAT      (HBUS_BASE+0x018)
#define HBUS_TARG_MEM_RDAT      (HBUS_BASE+0x01c)

#define HBUS_TARG_MBX_C         (HBUS_BASE+0x030)
#define HBUS_TARG_MBX_C_REG_BIT_CMD_BLOCKED         (0x00000004)

#define HBUS_TARG_PRPH_WADDR    (HBUS_BASE+0x044)
#define HBUS_TARG_PRPH_RADDR    (HBUS_BASE+0x048)
#define HBUS_TARG_PRPH_WDAT     (HBUS_BASE+0x04c)
#define HBUS_TARG_PRPH_RDAT     (HBUS_BASE+0x050)

#define HBUS_TARG_WRPTR         (HBUS_BASE+0x060)

#endif 
