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
 *****************************************************************************/

#ifndef	__il_prph_h__
#define __il_prph_h__

#define PRPH_BASE	(0x00000)
#define PRPH_END	(0xFFFFF)

#define APMG_BASE			(PRPH_BASE + 0x3000)
#define APMG_CLK_CTRL_REG		(APMG_BASE + 0x0000)
#define APMG_CLK_EN_REG			(APMG_BASE + 0x0004)
#define APMG_CLK_DIS_REG		(APMG_BASE + 0x0008)
#define APMG_PS_CTRL_REG		(APMG_BASE + 0x000c)
#define APMG_PCIDEV_STT_REG		(APMG_BASE + 0x0010)
#define APMG_RFKILL_REG			(APMG_BASE + 0x0014)
#define APMG_RTC_INT_STT_REG		(APMG_BASE + 0x001c)
#define APMG_RTC_INT_MSK_REG		(APMG_BASE + 0x0020)
#define APMG_DIGITAL_SVR_REG		(APMG_BASE + 0x0058)
#define APMG_ANALOG_SVR_REG		(APMG_BASE + 0x006C)

#define APMS_CLK_VAL_MRB_FUNC_MODE	(0x00000001)
#define APMG_CLK_VAL_DMA_CLK_RQT	(0x00000200)
#define APMG_CLK_VAL_BSM_CLK_RQT	(0x00000800)

#define APMG_PS_CTRL_EARLY_PWR_OFF_RESET_DIS	(0x00400000)
#define APMG_PS_CTRL_VAL_RESET_REQ		(0x04000000)
#define APMG_PS_CTRL_MSK_PWR_SRC		(0x03000000)
#define APMG_PS_CTRL_VAL_PWR_SRC_VMAIN		(0x00000000)
#define APMG_PS_CTRL_VAL_PWR_SRC_MAX		(0x01000000)	
#define APMG_PS_CTRL_VAL_PWR_SRC_VAUX		(0x02000000)
#define APMG_SVR_VOLTAGE_CONFIG_BIT_MSK	(0x000001E0)	
#define APMG_SVR_DIGITAL_VOLTAGE_1_32		(0x00000060)

#define APMG_PCIDEV_STT_VAL_L1_ACT_DIS		(0x00000800)


#define BSM_WR_CTRL_REG_BIT_START     (0x80000000)	
#define BSM_WR_CTRL_REG_BIT_START_EN  (0x40000000)	
#define BSM_DRAM_INST_LOAD            (0x80000000)	

#define BSM_BASE                     (PRPH_BASE + 0x3400)
#define BSM_END                      (PRPH_BASE + 0x3800)

#define BSM_WR_CTRL_REG              (BSM_BASE + 0x000)	
#define BSM_WR_MEM_SRC_REG           (BSM_BASE + 0x004)	
#define BSM_WR_MEM_DST_REG           (BSM_BASE + 0x008)	
#define BSM_WR_DWCOUNT_REG           (BSM_BASE + 0x00C)	
#define BSM_WR_STATUS_REG            (BSM_BASE + 0x010)	

#define BSM_DRAM_INST_PTR_REG        (BSM_BASE + 0x090)
#define BSM_DRAM_INST_BYTECOUNT_REG  (BSM_BASE + 0x094)
#define BSM_DRAM_DATA_PTR_REG        (BSM_BASE + 0x098)
#define BSM_DRAM_DATA_BYTECOUNT_REG  (BSM_BASE + 0x09C)

#define BSM_SRAM_LOWER_BOUND         (PRPH_BASE + 0x3800)
#define BSM_SRAM_SIZE			(1024)	

#define ALM_SCD_BASE                        (PRPH_BASE + 0x2E00)
#define ALM_SCD_MODE_REG                    (ALM_SCD_BASE + 0x000)
#define ALM_SCD_ARASTAT_REG                 (ALM_SCD_BASE + 0x004)
#define ALM_SCD_TXFACT_REG                  (ALM_SCD_BASE + 0x010)
#define ALM_SCD_TXF4MF_REG                  (ALM_SCD_BASE + 0x014)
#define ALM_SCD_TXF5MF_REG                  (ALM_SCD_BASE + 0x020)
#define ALM_SCD_SBYP_MODE_1_REG             (ALM_SCD_BASE + 0x02C)
#define ALM_SCD_SBYP_MODE_2_REG             (ALM_SCD_BASE + 0x030)


#define SCD_WIN_SIZE				64
#define SCD_FRAME_LIMIT				64

#define IL49_SCD_START_OFFSET		0xa02c00

#define IL49_SCD_SRAM_BASE_ADDR           (IL49_SCD_START_OFFSET + 0x0)

#define IL49_SCD_EMPTY_BITS               (IL49_SCD_START_OFFSET + 0x4)

#define IL49_SCD_DRAM_BASE_ADDR           (IL49_SCD_START_OFFSET + 0x10)

#define IL49_SCD_TXFACT                   (IL49_SCD_START_OFFSET + 0x1c)
#define IL49_SCD_QUEUE_WRPTR(x)  (IL49_SCD_START_OFFSET + 0x24 + (x) * 4)

#define IL49_SCD_QUEUE_RDPTR(x)  (IL49_SCD_START_OFFSET + 0x64 + (x) * 4)

#define IL49_SCD_QUEUECHAIN_SEL  (IL49_SCD_START_OFFSET + 0xd0)

#define IL49_SCD_INTERRUPT_MASK  (IL49_SCD_START_OFFSET + 0xe4)

/*
 * Queue search status registers.  One for each queue.
 * Sets up queue mode and assigns queue to Tx DMA channel.
 * Bit fields:
 * 19-10: Write mask/enable bits for bits 0-9
 *     9: Driver should init to "0"
 *     8: Scheduler-ACK mode (1), non-Scheduler-ACK (i.e. FIFO) mode (0).
 *        Driver should init to "1" for aggregation mode, or "0" otherwise.
 *   7-6: Driver should init to "0"
 *     5: Window Size Left; indicates whether scheduler can request
 *        another TFD, based on win size, etc.  Driver should init
 *        this bit to "1" for aggregation mode, or "0" for non-agg.
 *   4-1: Tx FIFO to use (range 0-7).
 *     0: Queue is active (1), not active (0).
 * Other bits should be written as "0"
 *
 * NOTE:  If enabling Scheduler-ACK mode, chain mode should also be enabled
 *        via SCD_QUEUECHAIN_SEL.
 */
#define IL49_SCD_QUEUE_STATUS_BITS(x)\
	(IL49_SCD_START_OFFSET + 0x104 + (x) * 4)

#define IL49_SCD_QUEUE_STTS_REG_POS_ACTIVE	(0)
#define IL49_SCD_QUEUE_STTS_REG_POS_TXF	(1)
#define IL49_SCD_QUEUE_STTS_REG_POS_WSL	(5)
#define IL49_SCD_QUEUE_STTS_REG_POS_SCD_ACK	(8)

#define IL49_SCD_QUEUE_STTS_REG_POS_SCD_ACT_EN	(10)
#define IL49_SCD_QUEUE_STTS_REG_MSK		(0x0007FC00)


#define IL49_SCD_CONTEXT_DATA_OFFSET			0x380
#define IL49_SCD_CONTEXT_QUEUE_OFFSET(x) \
			(IL49_SCD_CONTEXT_DATA_OFFSET + ((x) * 8))

#define IL49_SCD_QUEUE_CTX_REG1_WIN_SIZE_POS		(0)
#define IL49_SCD_QUEUE_CTX_REG1_WIN_SIZE_MSK		(0x0000007F)
#define IL49_SCD_QUEUE_CTX_REG2_FRAME_LIMIT_POS	(16)
#define IL49_SCD_QUEUE_CTX_REG2_FRAME_LIMIT_MSK	(0x007F0000)

#define IL49_SCD_TX_STTS_BITMAP_OFFSET		0x400

#define IL49_SCD_TRANSLATE_TBL_OFFSET		0x500

#define IL49_SCD_TRANSLATE_TBL_OFFSET_QUEUE(x) \
	((IL49_SCD_TRANSLATE_TBL_OFFSET + ((x) * 2)) & 0xfffffffc)

#define IL_SCD_TXFIFO_POS_TID			(0)
#define IL_SCD_TXFIFO_POS_RA			(4)
#define IL_SCD_QUEUE_RA_TID_MAP_RATID_MSK	(0x01FF)


#endif 
