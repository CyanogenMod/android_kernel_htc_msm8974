/*****************************************************************************
* Copyright 2005 - 2008 Broadcom Corporation.  All rights reserved.
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


#if !defined(__ASM_ARCH_REG_UMI_H)
#define __ASM_ARCH_REG_UMI_H

#include <csp/reg.h>
#include <mach/csp/mm_io.h>


#define HW_UMI_BASE       MM_IO_BASE_UMI

#define REG_UMI_FLASH0_TCR         __REG32(HW_UMI_BASE  + 0x00)
#define REG_UMI_FLASH1_TCR         __REG32(HW_UMI_BASE  + 0x04)
#define REG_UMI_FLASH2_TCR         __REG32(HW_UMI_BASE  + 0x08)
#define REG_UMI_MMD_ICR            __REG32(HW_UMI_BASE  + 0x0c)
#define REG_UMI_NAND_TCR           __REG32(HW_UMI_BASE  + 0x18)
#define REG_UMI_NAND_RCSR          __REG32(HW_UMI_BASE  + 0x1c)
#define REG_UMI_NAND_ECC_CSR       __REG32(HW_UMI_BASE  + 0x20)
#define REG_UMI_NAND_ECC_DATA      __REG32(HW_UMI_BASE  + 0x24)
#define REG_UMI_BCH_N              __REG32(HW_UMI_BASE  + 0x40)
#define REG_UMI_BCH_K              __REG32(HW_UMI_BASE  + 0x44)
#define REG_UMI_BCH_T              __REG32(HW_UMI_BASE  + 0x48)
#define REG_UMI_BCH_CTRL_STATUS    __REG32(HW_UMI_BASE  + 0x4C)
#define REG_UMI_BCH_WR_ECC_0       __REG32(HW_UMI_BASE  + 0x50)
#define REG_UMI_BCH_WR_ECC_1       __REG32(HW_UMI_BASE  + 0x54)
#define REG_UMI_BCH_WR_ECC_2       __REG32(HW_UMI_BASE  + 0x58)
#define REG_UMI_BCH_WR_ECC_3       __REG32(HW_UMI_BASE  + 0x5c)
#define REG_UMI_BCH_WR_ECC_4       __REG32(HW_UMI_BASE  + 0x60)
#define REG_UMI_BCH_RD_ERR_LOC_1_0 __REG32(HW_UMI_BASE  + 0x64)
#define REG_UMI_BCH_RD_ERR_LOC_3_2 __REG32(HW_UMI_BASE  + 0x68)
#define REG_UMI_BCH_RD_ERR_LOC_5_4 __REG32(HW_UMI_BASE  + 0x6c)
#define REG_UMI_BCH_RD_ERR_LOC_7_6 __REG32(HW_UMI_BASE  + 0x70)
#define REG_UMI_BCH_RD_ERR_LOC_9_8 __REG32(HW_UMI_BASE  + 0x74)
#define REG_UMI_BCH_RD_ERR_LOC_B_A __REG32(HW_UMI_BASE  + 0x78)

#define REG_UMI_TCR_WAITEN              0x80000000
#define REG_UMI_TCR_LOWFREQ             0x40000000
#define REG_UMI_TCR_MEMTYPE_SYNCWRITE   0x20000000
#define REG_UMI_TCR_MEMTYPE_SYNCREAD    0x10000000
#define REG_UMI_TCR_MEMTYPE_PAGEREAD    0x08000000
#define REG_UMI_TCR_MEMTYPE_PGSZ_MASK   0x07000000
#define REG_UMI_TCR_MEMTYPE_PGSZ_4      0x00000000
#define REG_UMI_TCR_MEMTYPE_PGSZ_8      0x01000000
#define REG_UMI_TCR_MEMTYPE_PGSZ_16     0x02000000
#define REG_UMI_TCR_MEMTYPE_PGSZ_32     0x03000000
#define REG_UMI_TCR_MEMTYPE_PGSZ_64     0x04000000
#define REG_UMI_TCR_MEMTYPE_PGSZ_128    0x05000000
#define REG_UMI_TCR_MEMTYPE_PGSZ_256    0x06000000
#define REG_UMI_TCR_MEMTYPE_PGSZ_512    0x07000000
#define REG_UMI_TCR_TPRC_TWLC_MASK      0x00f80000
#define REG_UMI_TCR_TBTA_MASK           0x00070000
#define REG_UMI_TCR_TWP_MASK            0x0000f800
#define REG_UMI_TCR_TWR_MASK            0x00000600
#define REG_UMI_TCR_TAS_MASK            0x00000180
#define REG_UMI_TCR_TOE_MASK            0x00000060
#define REG_UMI_TCR_TRC_TLC_MASK        0x0000001f

#define REG_UMI_MMD_ICR_FLASH_WP            0x8000
#define REG_UMI_MMD_ICR_XHCS                0x4000
#define REG_UMI_MMD_ICR_SDRAM2EN            0x2000
#define REG_UMI_MMD_ICR_INST512             0x1000
#define REG_UMI_MMD_ICR_DATA512             0x0800
#define REG_UMI_MMD_ICR_SDRAMEN             0x0400
#define REG_UMI_MMD_ICR_WAITPOL             0x0200
#define REG_UMI_MMD_ICR_BCLKSTOP            0x0100
#define REG_UMI_MMD_ICR_PERI1EN             0x0080
#define REG_UMI_MMD_ICR_PERI2EN             0x0040
#define REG_UMI_MMD_ICR_PERI3EN             0x0020
#define REG_UMI_MMD_ICR_MRSB1               0x0010
#define REG_UMI_MMD_ICR_MRSB0               0x0008
#define REG_UMI_MMD_ICR_MRSPOL              0x0004
#define REG_UMI_MMD_ICR_MRSMODE             0x0002
#define REG_UMI_MMD_ICR_MRSSTATE            0x0001

#define REG_UMI_NAND_TCR_CS_SWCTRL          0x80000000
#define REG_UMI_NAND_TCR_WORD16             0x40000000
#define REG_UMI_NAND_TCR_TBTA_MASK          0x00070000
#define REG_UMI_NAND_TCR_TWP_MASK           0x0000f800
#define REG_UMI_NAND_TCR_TWR_MASK           0x00000600
#define REG_UMI_NAND_TCR_TAS_MASK           0x00000180
#define REG_UMI_NAND_TCR_TOE_MASK           0x00000060
#define REG_UMI_NAND_TCR_TRC_TLC_MASK       0x0000001f

#define REG_UMI_NAND_RCSR_RDY               0x02
#define REG_UMI_NAND_RCSR_CS_ASSERTED       0x01

#define REG_UMI_NAND_ECC_CSR_NANDINT        0x80000000
#define REG_UMI_NAND_ECC_CSR_ECCINT_RAW     0x00800000
#define REG_UMI_NAND_ECC_CSR_RBINT_RAW      0x00400000
#define REG_UMI_NAND_ECC_CSR_ECCINT_ENABLE  0x00008000
#define REG_UMI_NAND_ECC_CSR_RBINT_ENABLE   0x00004000
#define REG_UMI_NAND_ECC_CSR_256BYTE        0x00000080
#define REG_UMI_NAND_ECC_CSR_ECC_ENABLE     0x00000001

#define REG_UMI_BCH_CTRL_STATUS_NB_CORR_ERROR_SHIFT 20
#define REG_UMI_BCH_CTRL_STATUS_NB_CORR_ERROR 0x00F00000
#define REG_UMI_BCH_CTRL_STATUS_UNCORR_ERR    0x00080000
#define REG_UMI_BCH_CTRL_STATUS_CORR_ERR      0x00040000
#define REG_UMI_BCH_CTRL_STATUS_RD_ECC_VALID  0x00020000
#define REG_UMI_BCH_CTRL_STATUS_WR_ECC_VALID  0x00010000
#define REG_UMI_BCH_CTRL_STATUS_PAUSE_ECC_DEC 0x00000010
#define REG_UMI_BCH_CTRL_STATUS_INT_EN        0x00000004
#define REG_UMI_BCH_CTRL_STATUS_ECC_RD_EN     0x00000002
#define REG_UMI_BCH_CTRL_STATUS_ECC_WR_EN     0x00000001
#define REG_UMI_BCH_ERR_LOC_MASK              0x00001FFF
#define REG_UMI_BCH_ERR_LOC_BYTE              0x00000007
#define REG_UMI_BCH_ERR_LOC_WORD              0x00000018
#define REG_UMI_BCH_ERR_LOC_PAGE              0x00001FE0
#define REG_UMI_BCH_ERR_LOC_ADDR(index)     (__REG32(HW_UMI_BASE + 0x64 + (index / 2)*4) >> ((index % 2) * 16))
#endif
