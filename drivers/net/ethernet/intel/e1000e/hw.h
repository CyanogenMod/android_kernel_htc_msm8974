/*******************************************************************************

  Intel PRO/1000 Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _E1000_HW_H_
#define _E1000_HW_H_

#include <linux/types.h>

struct e1000_hw;
struct e1000_adapter;

#include "defines.h"

#define er32(reg)	__er32(hw, E1000_##reg)
#define ew32(reg,val)	__ew32(hw, E1000_##reg, (val))
#define e1e_flush()	er32(STATUS)

#define E1000_WRITE_REG_ARRAY(a, reg, offset, value) \
	(writel((value), ((a)->hw_addr + reg + ((offset) << 2))))

#define E1000_READ_REG_ARRAY(a, reg, offset) \
	(readl((a)->hw_addr + reg + ((offset) << 2)))

enum e1e_registers {
	E1000_CTRL     = 0x00000, 
	E1000_STATUS   = 0x00008, 
	E1000_EECD     = 0x00010, 
	E1000_EERD     = 0x00014, 
	E1000_CTRL_EXT = 0x00018, 
	E1000_FLA      = 0x0001C, 
	E1000_MDIC     = 0x00020, 
	E1000_SCTL     = 0x00024, 
	E1000_FCAL     = 0x00028, 
	E1000_FCAH     = 0x0002C, 
	E1000_FEXTNVM4 = 0x00024, 
	E1000_FEXTNVM  = 0x00028, 
	E1000_FCT      = 0x00030, 
	E1000_VET      = 0x00038, 
	E1000_ICR      = 0x000C0, 
	E1000_ITR      = 0x000C4, 
	E1000_ICS      = 0x000C8, 
	E1000_IMS      = 0x000D0, 
	E1000_IMC      = 0x000D8, 
	E1000_EIAC_82574 = 0x000DC, 
	E1000_IAM      = 0x000E0, 
	E1000_IVAR     = 0x000E4, 
	E1000_EITR_82574_BASE = 0x000E8, 
#define E1000_EITR_82574(_n) (E1000_EITR_82574_BASE + (_n << 2))
	E1000_RCTL     = 0x00100, 
	E1000_FCTTV    = 0x00170, 
	E1000_TXCW     = 0x00178, 
	E1000_RXCW     = 0x00180, 
	E1000_TCTL     = 0x00400, 
	E1000_TCTL_EXT = 0x00404, 
	E1000_TIPG     = 0x00410, 
	E1000_AIT      = 0x00458, 
	E1000_LEDCTL   = 0x00E00, 
	E1000_EXTCNF_CTRL  = 0x00F00, 
	E1000_EXTCNF_SIZE  = 0x00F08, 
	E1000_PHY_CTRL     = 0x00F10, 
#define E1000_POEMB	E1000_PHY_CTRL	
	E1000_PBA      = 0x01000, 
	E1000_PBS      = 0x01008, 
	E1000_EEMNGCTL = 0x01010, 
	E1000_EEWR     = 0x0102C, 
	E1000_FLOP     = 0x0103C, 
	E1000_PBA_ECC  = 0x01100, 
	E1000_ERT      = 0x02008, 
	E1000_FCRTL    = 0x02160, 
	E1000_FCRTH    = 0x02168, 
	E1000_PSRCTL   = 0x02170, 
	E1000_RDBAL    = 0x02800, 
	E1000_RDBAH    = 0x02804, 
	E1000_RDLEN    = 0x02808, 
	E1000_RDH      = 0x02810, 
	E1000_RDT      = 0x02818, 
	E1000_RDTR     = 0x02820, 
	E1000_RXDCTL_BASE = 0x02828, 
#define E1000_RXDCTL(_n)   (E1000_RXDCTL_BASE + (_n << 8))
	E1000_RADV     = 0x0282C, 

/* Convenience macros
 *
 * Note: "_n" is the queue number of the register to be written to.
 *
 * Example usage:
 * E1000_RDBAL_REG(current_rx_queue)
 *
 */
#define E1000_RDBAL_REG(_n)   (E1000_RDBAL + (_n << 8))
	E1000_KABGTXD  = 0x03004, 
	E1000_TDBAL    = 0x03800, 
	E1000_TDBAH    = 0x03804, 
	E1000_TDLEN    = 0x03808, 
	E1000_TDH      = 0x03810, 
	E1000_TDT      = 0x03818, 
	E1000_TIDV     = 0x03820, 
	E1000_TXDCTL_BASE = 0x03828, 
#define E1000_TXDCTL(_n)   (E1000_TXDCTL_BASE + (_n << 8))
	E1000_TADV     = 0x0382C, 
	E1000_TARC_BASE = 0x03840, 
#define E1000_TARC(_n)   (E1000_TARC_BASE + (_n << 8))
	E1000_CRCERRS  = 0x04000, 
	E1000_ALGNERRC = 0x04004, 
	E1000_SYMERRS  = 0x04008, 
	E1000_RXERRC   = 0x0400C, 
	E1000_MPC      = 0x04010, 
	E1000_SCC      = 0x04014, 
	E1000_ECOL     = 0x04018, 
	E1000_MCC      = 0x0401C, 
	E1000_LATECOL  = 0x04020, 
	E1000_COLC     = 0x04028, 
	E1000_DC       = 0x04030, 
	E1000_TNCRS    = 0x04034, 
	E1000_SEC      = 0x04038, 
	E1000_CEXTERR  = 0x0403C, 
	E1000_RLEC     = 0x04040, 
	E1000_XONRXC   = 0x04048, 
	E1000_XONTXC   = 0x0404C, 
	E1000_XOFFRXC  = 0x04050, 
	E1000_XOFFTXC  = 0x04054, 
	E1000_FCRUC    = 0x04058, 
	E1000_PRC64    = 0x0405C, 
	E1000_PRC127   = 0x04060, 
	E1000_PRC255   = 0x04064, 
	E1000_PRC511   = 0x04068, 
	E1000_PRC1023  = 0x0406C, 
	E1000_PRC1522  = 0x04070, 
	E1000_GPRC     = 0x04074, 
	E1000_BPRC     = 0x04078, 
	E1000_MPRC     = 0x0407C, 
	E1000_GPTC     = 0x04080, 
	E1000_GORCL    = 0x04088, 
	E1000_GORCH    = 0x0408C, 
	E1000_GOTCL    = 0x04090, 
	E1000_GOTCH    = 0x04094, 
	E1000_RNBC     = 0x040A0, 
	E1000_RUC      = 0x040A4, 
	E1000_RFC      = 0x040A8, 
	E1000_ROC      = 0x040AC, 
	E1000_RJC      = 0x040B0, 
	E1000_MGTPRC   = 0x040B4, 
	E1000_MGTPDC   = 0x040B8, 
	E1000_MGTPTC   = 0x040BC, 
	E1000_TORL     = 0x040C0, 
	E1000_TORH     = 0x040C4, 
	E1000_TOTL     = 0x040C8, 
	E1000_TOTH     = 0x040CC, 
	E1000_TPR      = 0x040D0, 
	E1000_TPT      = 0x040D4, 
	E1000_PTC64    = 0x040D8, 
	E1000_PTC127   = 0x040DC, 
	E1000_PTC255   = 0x040E0, 
	E1000_PTC511   = 0x040E4, 
	E1000_PTC1023  = 0x040E8, 
	E1000_PTC1522  = 0x040EC, 
	E1000_MPTC     = 0x040F0, 
	E1000_BPTC     = 0x040F4, 
	E1000_TSCTC    = 0x040F8, 
	E1000_TSCTFC   = 0x040FC, 
	E1000_IAC      = 0x04100, 
	E1000_ICRXPTC  = 0x04104, 
	E1000_ICRXATC  = 0x04108, 
	E1000_ICTXPTC  = 0x0410C, 
	E1000_ICTXATC  = 0x04110, 
	E1000_ICTXQEC  = 0x04118, 
	E1000_ICTXQMTC = 0x0411C, 
	E1000_ICRXDMTC = 0x04120, 
	E1000_ICRXOC   = 0x04124, 
	E1000_RXCSUM   = 0x05000, 
	E1000_RFCTL    = 0x05008, 
	E1000_MTA      = 0x05200, 
	E1000_RAL_BASE = 0x05400, 
#define E1000_RAL(_n)   (E1000_RAL_BASE + ((_n) * 8))
#define E1000_RA        (E1000_RAL(0))
	E1000_RAH_BASE = 0x05404, 
#define E1000_RAH(_n)   (E1000_RAH_BASE + ((_n) * 8))
	E1000_VFTA     = 0x05600, 
	E1000_WUC      = 0x05800, 
	E1000_WUFC     = 0x05808, 
	E1000_WUS      = 0x05810, 
	E1000_MRQC     = 0x05818, 
	E1000_MANC     = 0x05820, 
	E1000_FFLT     = 0x05F00, 
	E1000_HOST_IF  = 0x08800, 

	E1000_KMRNCTRLSTA = 0x00034, 
	E1000_MANC2H    = 0x05860, 
	E1000_MDEF_BASE = 0x05890, 
#define E1000_MDEF(_n)   (E1000_MDEF_BASE + ((_n) * 4))
	E1000_SW_FW_SYNC = 0x05B5C, 
	E1000_GCR	= 0x05B00, 
	E1000_GCR2      = 0x05B64, 
	E1000_FACTPS    = 0x05B30, 
	E1000_SWSM      = 0x05B50, 
	E1000_FWSM      = 0x05B54, 
	E1000_SWSM2     = 0x05B58, 
	E1000_RETA_BASE = 0x05C00, 
#define E1000_RETA(_n)	(E1000_RETA_BASE + ((_n) * 4))
	E1000_RSSRK_BASE = 0x05C80, 
#define E1000_RSSRK(_n)	(E1000_RSSRK_BASE + ((_n) * 4))
	E1000_FFLT_DBG  = 0x05F04, 
	E1000_PCH_RAICC_BASE = 0x05F50, 
#define E1000_PCH_RAICC(_n)	(E1000_PCH_RAICC_BASE + ((_n) * 4))
#define E1000_CRC_OFFSET	E1000_PCH_RAICC_BASE
	E1000_HICR      = 0x08F00, 
};

#define E1000_MAX_PHY_ADDR		4

#define IGP01E1000_PHY_PORT_CONFIG	0x10 
#define IGP01E1000_PHY_PORT_STATUS	0x11 
#define IGP01E1000_PHY_PORT_CTRL	0x12 
#define IGP01E1000_PHY_LINK_HEALTH	0x13 
#define IGP02E1000_PHY_POWER_MGMT	0x19 
#define IGP01E1000_PHY_PAGE_SELECT	0x1F 
#define BM_PHY_PAGE_SELECT		22   
#define IGP_PAGE_SHIFT			5
#define PHY_REG_MASK			0x1F

#define BM_WUC_PAGE			800
#define BM_WUC_ADDRESS_OPCODE		0x11
#define BM_WUC_DATA_OPCODE		0x12
#define BM_WUC_ENABLE_PAGE		769
#define BM_WUC_ENABLE_REG		17
#define BM_WUC_ENABLE_BIT		(1 << 2)
#define BM_WUC_HOST_WU_BIT		(1 << 4)
#define BM_WUC_ME_WU_BIT		(1 << 5)

#define BM_WUC	PHY_REG(BM_WUC_PAGE, 1)
#define BM_WUFC PHY_REG(BM_WUC_PAGE, 2)
#define BM_WUS	PHY_REG(BM_WUC_PAGE, 3)

#define IGP01E1000_PHY_PCS_INIT_REG	0x00B4
#define IGP01E1000_PHY_POLARITY_MASK	0x0078

#define IGP01E1000_PSCR_AUTO_MDIX	0x1000
#define IGP01E1000_PSCR_FORCE_MDI_MDIX	0x2000 

#define IGP01E1000_PSCFR_SMART_SPEED	0x0080

#define IGP02E1000_PM_SPD		0x0001 
#define IGP02E1000_PM_D0_LPLU		0x0002 
#define IGP02E1000_PM_D3_LPLU		0x0004 

#define IGP01E1000_PLHR_SS_DOWNGRADE	0x8000

#define IGP01E1000_PSSR_POLARITY_REVERSED	0x0002
#define IGP01E1000_PSSR_MDIX			0x0800
#define IGP01E1000_PSSR_SPEED_MASK		0xC000
#define IGP01E1000_PSSR_SPEED_1000MBPS		0xC000

#define IGP02E1000_PHY_CHANNEL_NUM		4
#define IGP02E1000_PHY_AGC_A			0x11B1
#define IGP02E1000_PHY_AGC_B			0x12B1
#define IGP02E1000_PHY_AGC_C			0x14B1
#define IGP02E1000_PHY_AGC_D			0x18B1

#define IGP02E1000_AGC_LENGTH_SHIFT	9 
#define IGP02E1000_AGC_LENGTH_MASK	0x7F
#define IGP02E1000_AGC_RANGE		15

#define E1000_VFTA_ENTRY_SHIFT		5
#define E1000_VFTA_ENTRY_MASK		0x7F
#define E1000_VFTA_ENTRY_BIT_SHIFT_MASK	0x1F

#define E1000_HICR_EN			0x01  
#define E1000_HICR_C			0x02
#define E1000_HICR_FW_RESET_ENABLE	0x40
#define E1000_HICR_FW_RESET		0x80

#define E1000_FWSM_MODE_MASK		0xE
#define E1000_FWSM_MODE_SHIFT		1

#define E1000_MNG_IAMT_MODE		0x3
#define E1000_MNG_DHCP_COOKIE_LENGTH	0x10
#define E1000_MNG_DHCP_COOKIE_OFFSET	0x6F0
#define E1000_MNG_DHCP_COMMAND_TIMEOUT	10
#define E1000_MNG_DHCP_TX_PAYLOAD_CMD	64
#define E1000_MNG_DHCP_COOKIE_STATUS_PARSING	0x1
#define E1000_MNG_DHCP_COOKIE_STATUS_VLAN	0x2

#define E1000_STM_OPCODE  0xDB00

#define E1000_KMRNCTRLSTA_OFFSET	0x001F0000
#define E1000_KMRNCTRLSTA_OFFSET_SHIFT	16
#define E1000_KMRNCTRLSTA_REN		0x00200000
#define E1000_KMRNCTRLSTA_CTRL_OFFSET	0x1    
#define E1000_KMRNCTRLSTA_DIAG_OFFSET	0x3    
#define E1000_KMRNCTRLSTA_TIMEOUTS	0x4    
#define E1000_KMRNCTRLSTA_INBAND_PARAM	0x9    
#define E1000_KMRNCTRLSTA_IBIST_DISABLE	0x0200 
#define E1000_KMRNCTRLSTA_DIAG_NELPBK	0x1000 
#define E1000_KMRNCTRLSTA_K1_CONFIG	0x7
#define E1000_KMRNCTRLSTA_K1_ENABLE	0x0002
#define E1000_KMRNCTRLSTA_HD_CTRL	0x10   

#define IFE_PHY_EXTENDED_STATUS_CONTROL	0x10
#define IFE_PHY_SPECIAL_CONTROL		0x11 
#define IFE_PHY_SPECIAL_CONTROL_LED	0x1B 
#define IFE_PHY_MDIX_CONTROL		0x1C 

#define IFE_PESC_POLARITY_REVERSED	0x0100

#define IFE_PSC_AUTO_POLARITY_DISABLE		0x0010
#define IFE_PSC_FORCE_POLARITY			0x0020

#define IFE_PSCL_PROBE_MODE		0x0020
#define IFE_PSCL_PROBE_LEDS_OFF		0x0006 
#define IFE_PSCL_PROBE_LEDS_ON		0x0007 

#define IFE_PMC_MDIX_STATUS	0x0020 
#define IFE_PMC_FORCE_MDIX	0x0040 
#define IFE_PMC_AUTO_MDIX	0x0080 

#define E1000_CABLE_LENGTH_UNDEFINED	0xFF

#define E1000_DEV_ID_82571EB_COPPER		0x105E
#define E1000_DEV_ID_82571EB_FIBER		0x105F
#define E1000_DEV_ID_82571EB_SERDES		0x1060
#define E1000_DEV_ID_82571EB_QUAD_COPPER	0x10A4
#define E1000_DEV_ID_82571PT_QUAD_COPPER	0x10D5
#define E1000_DEV_ID_82571EB_QUAD_FIBER		0x10A5
#define E1000_DEV_ID_82571EB_QUAD_COPPER_LP	0x10BC
#define E1000_DEV_ID_82571EB_SERDES_DUAL	0x10D9
#define E1000_DEV_ID_82571EB_SERDES_QUAD	0x10DA
#define E1000_DEV_ID_82572EI_COPPER		0x107D
#define E1000_DEV_ID_82572EI_FIBER		0x107E
#define E1000_DEV_ID_82572EI_SERDES		0x107F
#define E1000_DEV_ID_82572EI			0x10B9
#define E1000_DEV_ID_82573E			0x108B
#define E1000_DEV_ID_82573E_IAMT		0x108C
#define E1000_DEV_ID_82573L			0x109A
#define E1000_DEV_ID_82574L			0x10D3
#define E1000_DEV_ID_82574LA			0x10F6
#define E1000_DEV_ID_82583V                     0x150C

#define E1000_DEV_ID_80003ES2LAN_COPPER_DPT	0x1096
#define E1000_DEV_ID_80003ES2LAN_SERDES_DPT	0x1098
#define E1000_DEV_ID_80003ES2LAN_COPPER_SPT	0x10BA
#define E1000_DEV_ID_80003ES2LAN_SERDES_SPT	0x10BB

#define E1000_DEV_ID_ICH8_82567V_3		0x1501
#define E1000_DEV_ID_ICH8_IGP_M_AMT		0x1049
#define E1000_DEV_ID_ICH8_IGP_AMT		0x104A
#define E1000_DEV_ID_ICH8_IGP_C			0x104B
#define E1000_DEV_ID_ICH8_IFE			0x104C
#define E1000_DEV_ID_ICH8_IFE_GT		0x10C4
#define E1000_DEV_ID_ICH8_IFE_G			0x10C5
#define E1000_DEV_ID_ICH8_IGP_M			0x104D
#define E1000_DEV_ID_ICH9_IGP_AMT		0x10BD
#define E1000_DEV_ID_ICH9_BM			0x10E5
#define E1000_DEV_ID_ICH9_IGP_M_AMT		0x10F5
#define E1000_DEV_ID_ICH9_IGP_M			0x10BF
#define E1000_DEV_ID_ICH9_IGP_M_V		0x10CB
#define E1000_DEV_ID_ICH9_IGP_C			0x294C
#define E1000_DEV_ID_ICH9_IFE			0x10C0
#define E1000_DEV_ID_ICH9_IFE_GT		0x10C3
#define E1000_DEV_ID_ICH9_IFE_G			0x10C2
#define E1000_DEV_ID_ICH10_R_BM_LM		0x10CC
#define E1000_DEV_ID_ICH10_R_BM_LF		0x10CD
#define E1000_DEV_ID_ICH10_R_BM_V		0x10CE
#define E1000_DEV_ID_ICH10_D_BM_LM		0x10DE
#define E1000_DEV_ID_ICH10_D_BM_LF		0x10DF
#define E1000_DEV_ID_ICH10_D_BM_V		0x1525
#define E1000_DEV_ID_PCH_M_HV_LM		0x10EA
#define E1000_DEV_ID_PCH_M_HV_LC		0x10EB
#define E1000_DEV_ID_PCH_D_HV_DM		0x10EF
#define E1000_DEV_ID_PCH_D_HV_DC		0x10F0
#define E1000_DEV_ID_PCH2_LV_LM			0x1502
#define E1000_DEV_ID_PCH2_LV_V			0x1503

#define E1000_REVISION_4 4

#define E1000_FUNC_1 1

#define E1000_ALT_MAC_ADDRESS_OFFSET_LAN0   0
#define E1000_ALT_MAC_ADDRESS_OFFSET_LAN1   3

enum e1000_mac_type {
	e1000_82571,
	e1000_82572,
	e1000_82573,
	e1000_82574,
	e1000_82583,
	e1000_80003es2lan,
	e1000_ich8lan,
	e1000_ich9lan,
	e1000_ich10lan,
	e1000_pchlan,
	e1000_pch2lan,
};

enum e1000_media_type {
	e1000_media_type_unknown = 0,
	e1000_media_type_copper = 1,
	e1000_media_type_fiber = 2,
	e1000_media_type_internal_serdes = 3,
	e1000_num_media_types
};

enum e1000_nvm_type {
	e1000_nvm_unknown = 0,
	e1000_nvm_none,
	e1000_nvm_eeprom_spi,
	e1000_nvm_flash_hw,
	e1000_nvm_flash_sw
};

enum e1000_nvm_override {
	e1000_nvm_override_none = 0,
	e1000_nvm_override_spi_small,
	e1000_nvm_override_spi_large
};

enum e1000_phy_type {
	e1000_phy_unknown = 0,
	e1000_phy_none,
	e1000_phy_m88,
	e1000_phy_igp,
	e1000_phy_igp_2,
	e1000_phy_gg82563,
	e1000_phy_igp_3,
	e1000_phy_ife,
	e1000_phy_bm,
	e1000_phy_82578,
	e1000_phy_82577,
	e1000_phy_82579,
};

enum e1000_bus_width {
	e1000_bus_width_unknown = 0,
	e1000_bus_width_pcie_x1,
	e1000_bus_width_pcie_x2,
	e1000_bus_width_pcie_x4 = 4,
	e1000_bus_width_32,
	e1000_bus_width_64,
	e1000_bus_width_reserved
};

enum e1000_1000t_rx_status {
	e1000_1000t_rx_status_not_ok = 0,
	e1000_1000t_rx_status_ok,
	e1000_1000t_rx_status_undefined = 0xFF
};

enum e1000_rev_polarity{
	e1000_rev_polarity_normal = 0,
	e1000_rev_polarity_reversed,
	e1000_rev_polarity_undefined = 0xFF
};

enum e1000_fc_mode {
	e1000_fc_none = 0,
	e1000_fc_rx_pause,
	e1000_fc_tx_pause,
	e1000_fc_full,
	e1000_fc_default = 0xFF
};

enum e1000_ms_type {
	e1000_ms_hw_default = 0,
	e1000_ms_force_master,
	e1000_ms_force_slave,
	e1000_ms_auto
};

enum e1000_smart_speed {
	e1000_smart_speed_default = 0,
	e1000_smart_speed_on,
	e1000_smart_speed_off
};

enum e1000_serdes_link_state {
	e1000_serdes_link_down = 0,
	e1000_serdes_link_autoneg_progress,
	e1000_serdes_link_autoneg_complete,
	e1000_serdes_link_forced_up
};

struct e1000_rx_desc {
	__le64 buffer_addr; 
	__le16 length;      
	__le16 csum;	
	u8  status;      
	u8  errors;      
	__le16 special;
};

union e1000_rx_desc_extended {
	struct {
		__le64 buffer_addr;
		__le64 reserved;
	} read;
	struct {
		struct {
			__le32 mrq;	      
			union {
				__le32 rss;	    
				struct {
					__le16 ip_id;  
					__le16 csum;   
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;     
			__le16 length;
			__le16 vlan;	     
		} upper;
	} wb;  
};

#define MAX_PS_BUFFERS 4
union e1000_rx_desc_packet_split {
	struct {
		
		__le64 buffer_addr[MAX_PS_BUFFERS];
	} read;
	struct {
		struct {
			__le32 mrq;	      
			union {
				__le32 rss;	      
				struct {
					__le16 ip_id;    
					__le16 csum;     
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;     
			__le16 length0;	  
			__le16 vlan;	     
		} middle;
		struct {
			__le16 header_status;
			__le16 length[3];	
		} upper;
		__le64 reserved;
	} wb; 
};

struct e1000_tx_desc {
	__le64 buffer_addr;      
	union {
		__le32 data;
		struct {
			__le16 length;    
			u8 cso;	
			u8 cmd;	
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status;     
			u8 css;	
			__le16 special;
		} fields;
	} upper;
};

struct e1000_context_desc {
	union {
		__le32 ip_config;
		struct {
			u8 ipcss;      
			u8 ipcso;      
			__le16 ipcse;     
		} ip_fields;
	} lower_setup;
	union {
		__le32 tcp_config;
		struct {
			u8 tucss;      
			u8 tucso;      
			__le16 tucse;     
		} tcp_fields;
	} upper_setup;
	__le32 cmd_and_length;
	union {
		__le32 data;
		struct {
			u8 status;     
			u8 hdr_len;    
			__le16 mss;       
		} fields;
	} tcp_seg_setup;
};

struct e1000_data_desc {
	__le64 buffer_addr;   
	union {
		__le32 data;
		struct {
			__le16 length;    
			u8 typ_len_ext;
			u8 cmd;
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status;     
			u8 popts;      
			__le16 special;   
		} fields;
	} upper;
};

struct e1000_hw_stats {
	u64 crcerrs;
	u64 algnerrc;
	u64 symerrs;
	u64 rxerrc;
	u64 mpc;
	u64 scc;
	u64 ecol;
	u64 mcc;
	u64 latecol;
	u64 colc;
	u64 dc;
	u64 tncrs;
	u64 sec;
	u64 cexterr;
	u64 rlec;
	u64 xonrxc;
	u64 xontxc;
	u64 xoffrxc;
	u64 xofftxc;
	u64 fcruc;
	u64 prc64;
	u64 prc127;
	u64 prc255;
	u64 prc511;
	u64 prc1023;
	u64 prc1522;
	u64 gprc;
	u64 bprc;
	u64 mprc;
	u64 gptc;
	u64 gorc;
	u64 gotc;
	u64 rnbc;
	u64 ruc;
	u64 rfc;
	u64 roc;
	u64 rjc;
	u64 mgprc;
	u64 mgpdc;
	u64 mgptc;
	u64 tor;
	u64 tot;
	u64 tpr;
	u64 tpt;
	u64 ptc64;
	u64 ptc127;
	u64 ptc255;
	u64 ptc511;
	u64 ptc1023;
	u64 ptc1522;
	u64 mptc;
	u64 bptc;
	u64 tsctc;
	u64 tsctfc;
	u64 iac;
	u64 icrxptc;
	u64 icrxatc;
	u64 ictxptc;
	u64 ictxatc;
	u64 ictxqec;
	u64 ictxqmtc;
	u64 icrxdmtc;
	u64 icrxoc;
};

struct e1000_phy_stats {
	u32 idle_errors;
	u32 receive_errors;
};

struct e1000_host_mng_dhcp_cookie {
	u32 signature;
	u8  status;
	u8  reserved0;
	u16 vlan_id;
	u32 reserved1;
	u16 reserved2;
	u8  reserved3;
	u8  checksum;
};

struct e1000_host_command_header {
	u8 command_id;
	u8 command_length;
	u8 command_options;
	u8 checksum;
};

#define E1000_HI_MAX_DATA_LENGTH     252
struct e1000_host_command_info {
	struct e1000_host_command_header command_header;
	u8 command_data[E1000_HI_MAX_DATA_LENGTH];
};

struct e1000_host_mng_command_header {
	u8  command_id;
	u8  checksum;
	u16 reserved1;
	u16 reserved2;
	u16 command_length;
};

#define E1000_HI_MAX_MNG_DATA_LENGTH 0x6F8
struct e1000_host_mng_command_info {
	struct e1000_host_mng_command_header command_header;
	u8 command_data[E1000_HI_MAX_MNG_DATA_LENGTH];
};

struct e1000_mac_operations {
	s32  (*id_led_init)(struct e1000_hw *);
	s32  (*blink_led)(struct e1000_hw *);
	bool (*check_mng_mode)(struct e1000_hw *);
	s32  (*check_for_link)(struct e1000_hw *);
	s32  (*cleanup_led)(struct e1000_hw *);
	void (*clear_hw_cntrs)(struct e1000_hw *);
	void (*clear_vfta)(struct e1000_hw *);
	s32  (*get_bus_info)(struct e1000_hw *);
	void (*set_lan_id)(struct e1000_hw *);
	s32  (*get_link_up_info)(struct e1000_hw *, u16 *, u16 *);
	s32  (*led_on)(struct e1000_hw *);
	s32  (*led_off)(struct e1000_hw *);
	void (*update_mc_addr_list)(struct e1000_hw *, u8 *, u32);
	s32  (*reset_hw)(struct e1000_hw *);
	s32  (*init_hw)(struct e1000_hw *);
	s32  (*setup_link)(struct e1000_hw *);
	s32  (*setup_physical_interface)(struct e1000_hw *);
	s32  (*setup_led)(struct e1000_hw *);
	void (*write_vfta)(struct e1000_hw *, u32, u32);
	void (*config_collision_dist)(struct e1000_hw *);
	s32  (*read_mac_addr)(struct e1000_hw *);
};

struct e1000_phy_operations {
	s32  (*acquire)(struct e1000_hw *);
	s32  (*cfg_on_link_up)(struct e1000_hw *);
	s32  (*check_polarity)(struct e1000_hw *);
	s32  (*check_reset_block)(struct e1000_hw *);
	s32  (*commit)(struct e1000_hw *);
	s32  (*force_speed_duplex)(struct e1000_hw *);
	s32  (*get_cfg_done)(struct e1000_hw *hw);
	s32  (*get_cable_length)(struct e1000_hw *);
	s32  (*get_info)(struct e1000_hw *);
	s32  (*set_page)(struct e1000_hw *, u16);
	s32  (*read_reg)(struct e1000_hw *, u32, u16 *);
	s32  (*read_reg_locked)(struct e1000_hw *, u32, u16 *);
	s32  (*read_reg_page)(struct e1000_hw *, u32, u16 *);
	void (*release)(struct e1000_hw *);
	s32  (*reset)(struct e1000_hw *);
	s32  (*set_d0_lplu_state)(struct e1000_hw *, bool);
	s32  (*set_d3_lplu_state)(struct e1000_hw *, bool);
	s32  (*write_reg)(struct e1000_hw *, u32, u16);
	s32  (*write_reg_locked)(struct e1000_hw *, u32, u16);
	s32  (*write_reg_page)(struct e1000_hw *, u32, u16);
	void (*power_up)(struct e1000_hw *);
	void (*power_down)(struct e1000_hw *);
};

struct e1000_nvm_operations {
	s32  (*acquire)(struct e1000_hw *);
	s32  (*read)(struct e1000_hw *, u16, u16, u16 *);
	void (*release)(struct e1000_hw *);
	void (*reload)(struct e1000_hw *);
	s32  (*update)(struct e1000_hw *);
	s32  (*valid_led_default)(struct e1000_hw *, u16 *);
	s32  (*validate)(struct e1000_hw *);
	s32  (*write)(struct e1000_hw *, u16, u16, u16 *);
};

struct e1000_mac_info {
	struct e1000_mac_operations ops;
	u8 addr[ETH_ALEN];
	u8 perm_addr[ETH_ALEN];

	enum e1000_mac_type type;

	u32 collision_delta;
	u32 ledctl_default;
	u32 ledctl_mode1;
	u32 ledctl_mode2;
	u32 mc_filter_type;
	u32 tx_packet_delta;
	u32 txcw;

	u16 current_ifs_val;
	u16 ifs_max_val;
	u16 ifs_min_val;
	u16 ifs_ratio;
	u16 ifs_step_size;
	u16 mta_reg_count;

	
	#define MAX_MTA_REG 128
	u32 mta_shadow[MAX_MTA_REG];
	u16 rar_entry_count;

	u8  forced_speed_duplex;

	bool adaptive_ifs;
	bool has_fwsm;
	bool arc_subsystem_valid;
	bool autoneg;
	bool autoneg_failed;
	bool get_link_status;
	bool in_ifs_mode;
	bool serdes_has_link;
	bool tx_pkt_filtering;
	enum e1000_serdes_link_state serdes_link_state;
};

struct e1000_phy_info {
	struct e1000_phy_operations ops;

	enum e1000_phy_type type;

	enum e1000_1000t_rx_status local_rx;
	enum e1000_1000t_rx_status remote_rx;
	enum e1000_ms_type ms_type;
	enum e1000_ms_type original_ms_type;
	enum e1000_rev_polarity cable_polarity;
	enum e1000_smart_speed smart_speed;

	u32 addr;
	u32 id;
	u32 reset_delay_us; 
	u32 revision;

	enum e1000_media_type media_type;

	u16 autoneg_advertised;
	u16 autoneg_mask;
	u16 cable_length;
	u16 max_cable_length;
	u16 min_cable_length;

	u8 mdix;

	bool disable_polarity_correction;
	bool is_mdix;
	bool polarity_correction;
	bool speed_downgraded;
	bool autoneg_wait_to_complete;
};

struct e1000_nvm_info {
	struct e1000_nvm_operations ops;

	enum e1000_nvm_type type;
	enum e1000_nvm_override override;

	u32 flash_bank_size;
	u32 flash_base_addr;

	u16 word_size;
	u16 delay_usec;
	u16 address_bits;
	u16 opcode_bits;
	u16 page_size;
};

struct e1000_bus_info {
	enum e1000_bus_width width;

	u16 func;
};

struct e1000_fc_info {
	u32 high_water;          
	u32 low_water;           
	u16 pause_time;          
	u16 refresh_time;        
	bool send_xon;           
	bool strict_ieee;        
	enum e1000_fc_mode current_mode; 
	enum e1000_fc_mode requested_mode; 
};

struct e1000_dev_spec_82571 {
	bool laa_is_present;
	u32 smb_counter;
};

struct e1000_dev_spec_80003es2lan {
	bool  mdic_wa_enable;
};

struct e1000_shadow_ram {
	u16  value;
	bool modified;
};

#define E1000_ICH8_SHADOW_RAM_WORDS		2048

struct e1000_dev_spec_ich8lan {
	bool kmrn_lock_loss_workaround_enabled;
	struct e1000_shadow_ram shadow_ram[E1000_ICH8_SHADOW_RAM_WORDS];
	bool nvm_k1_enabled;
	bool eee_disable;
};

struct e1000_hw {
	struct e1000_adapter *adapter;

	void __iomem *hw_addr;
	void __iomem *flash_address;

	struct e1000_mac_info  mac;
	struct e1000_fc_info   fc;
	struct e1000_phy_info  phy;
	struct e1000_nvm_info  nvm;
	struct e1000_bus_info  bus;
	struct e1000_host_mng_dhcp_cookie mng_cookie;

	union {
		struct e1000_dev_spec_82571	e82571;
		struct e1000_dev_spec_80003es2lan e80003es2lan;
		struct e1000_dev_spec_ich8lan	ich8lan;
	} dev_spec;
};

#endif
