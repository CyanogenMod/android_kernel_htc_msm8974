/*
 * Copyright (C) 1999 - 2010 Intel Corporation.
 * Copyright (C) 2010 OKI SEMICONDUCTOR Co., LTD.
 *
 * This code was derived from the Intel e1000e Linux driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
 */

#include "pch_gbe.h"
#include "pch_gbe_phy.h"

#define PHY_MAX_REG_ADDRESS   0x1F	

#define PHY_CONTROL           0x00  
#define PHY_STATUS            0x01  
#define PHY_ID1               0x02  
#define PHY_ID2               0x03  
#define PHY_AUTONEG_ADV       0x04  
#define PHY_LP_ABILITY        0x05  
#define PHY_AUTONEG_EXP       0x06  
#define PHY_NEXT_PAGE_TX      0x07  
#define PHY_LP_NEXT_PAGE      0x08  
#define PHY_1000T_CTRL        0x09  
#define PHY_1000T_STATUS      0x0A  
#define PHY_EXT_STATUS        0x0F  
#define PHY_PHYSP_CONTROL     0x10  
#define PHY_EXT_PHYSP_CONTROL 0x14  
#define PHY_LED_CONTROL       0x18  
#define PHY_EXT_PHYSP_STATUS  0x1B  

#define MII_CR_SPEED_SELECT_MSB 0x0040	
#define MII_CR_COLL_TEST_ENABLE 0x0080	
#define MII_CR_FULL_DUPLEX      0x0100	
#define MII_CR_RESTART_AUTO_NEG 0x0200	
#define MII_CR_ISOLATE          0x0400	
#define MII_CR_POWER_DOWN       0x0800	
#define MII_CR_AUTO_NEG_EN      0x1000	
#define MII_CR_SPEED_SELECT_LSB 0x2000	
#define MII_CR_LOOPBACK         0x4000	
#define MII_CR_RESET            0x8000	
#define MII_CR_SPEED_1000       0x0040
#define MII_CR_SPEED_100        0x2000
#define MII_CR_SPEED_10         0x0000

#define MII_SR_EXTENDED_CAPS     0x0001	
#define MII_SR_JABBER_DETECT     0x0002	
#define MII_SR_LINK_STATUS       0x0004	
#define MII_SR_AUTONEG_CAPS      0x0008	
#define MII_SR_REMOTE_FAULT      0x0010	
#define MII_SR_AUTONEG_COMPLETE  0x0020	
#define MII_SR_PREAMBLE_SUPPRESS 0x0040	
#define MII_SR_EXTENDED_STATUS   0x0100	
#define MII_SR_100T2_HD_CAPS     0x0200	
#define MII_SR_100T2_FD_CAPS     0x0400	
#define MII_SR_10T_HD_CAPS       0x0800	
#define MII_SR_10T_FD_CAPS       0x1000	
#define MII_SR_100X_HD_CAPS      0x2000	
#define MII_SR_100X_FD_CAPS      0x4000	
#define MII_SR_100T4_CAPS        0x8000	

#define PHY_REVISION_MASK        0x000F

#define PHYSP_CTRL_ASSERT_CRS_TX  0x0800


#define PHY_CONTROL_DEFAULT         0x1140 
#define PHY_AUTONEG_ADV_DEFAULT     0x01e0 
#define PHY_NEXT_PAGE_TX_DEFAULT    0x2001 
#define PHY_1000T_CTRL_DEFAULT      0x0300 
#define PHY_PHYSP_CONTROL_DEFAULT   0x01EE 

s32 pch_gbe_phy_get_id(struct pch_gbe_hw *hw)
{
	struct pch_gbe_phy_info *phy = &hw->phy;
	s32 ret;
	u16 phy_id1;
	u16 phy_id2;

	ret = pch_gbe_phy_read_reg_miic(hw, PHY_ID1, &phy_id1);
	if (ret)
		return ret;
	ret = pch_gbe_phy_read_reg_miic(hw, PHY_ID2, &phy_id2);
	if (ret)
		return ret;
	phy->id = (u32)phy_id1;
	phy->id = ((phy->id << 6) | ((phy_id2 & 0xFC00) >> 10));
	phy->revision = (u32) (phy_id2 & 0x000F);
	pr_debug("phy->id : 0x%08x  phy->revision : 0x%08x\n",
		 phy->id, phy->revision);
	return 0;
}

s32 pch_gbe_phy_read_reg_miic(struct pch_gbe_hw *hw, u32 offset, u16 *data)
{
	struct pch_gbe_phy_info *phy = &hw->phy;

	if (offset > PHY_MAX_REG_ADDRESS) {
		pr_err("PHY Address %d is out of range\n", offset);
		return -EINVAL;
	}
	*data = pch_gbe_mac_ctrl_miim(hw, phy->addr, PCH_GBE_HAL_MIIM_READ,
				      offset, (u16)0);
	return 0;
}

s32 pch_gbe_phy_write_reg_miic(struct pch_gbe_hw *hw, u32 offset, u16 data)
{
	struct pch_gbe_phy_info *phy = &hw->phy;

	if (offset > PHY_MAX_REG_ADDRESS) {
		pr_err("PHY Address %d is out of range\n", offset);
		return -EINVAL;
	}
	pch_gbe_mac_ctrl_miim(hw, phy->addr, PCH_GBE_HAL_MIIM_WRITE,
				 offset, data);
	return 0;
}

void pch_gbe_phy_sw_reset(struct pch_gbe_hw *hw)
{
	u16 phy_ctrl;

	pch_gbe_phy_read_reg_miic(hw, PHY_CONTROL, &phy_ctrl);
	phy_ctrl |= MII_CR_RESET;
	pch_gbe_phy_write_reg_miic(hw, PHY_CONTROL, phy_ctrl);
	udelay(1);
}

void pch_gbe_phy_hw_reset(struct pch_gbe_hw *hw)
{
	pch_gbe_phy_write_reg_miic(hw, PHY_CONTROL, PHY_CONTROL_DEFAULT);
	pch_gbe_phy_write_reg_miic(hw, PHY_AUTONEG_ADV,
					PHY_AUTONEG_ADV_DEFAULT);
	pch_gbe_phy_write_reg_miic(hw, PHY_NEXT_PAGE_TX,
					PHY_NEXT_PAGE_TX_DEFAULT);
	pch_gbe_phy_write_reg_miic(hw, PHY_1000T_CTRL, PHY_1000T_CTRL_DEFAULT);
	pch_gbe_phy_write_reg_miic(hw, PHY_PHYSP_CONTROL,
					PHY_PHYSP_CONTROL_DEFAULT);
}

void pch_gbe_phy_power_up(struct pch_gbe_hw *hw)
{
	u16 mii_reg;

	mii_reg = 0;
	
	pch_gbe_phy_read_reg_miic(hw, PHY_CONTROL, &mii_reg);
	mii_reg &= ~MII_CR_POWER_DOWN;
	pch_gbe_phy_write_reg_miic(hw, PHY_CONTROL, mii_reg);
}

void pch_gbe_phy_power_down(struct pch_gbe_hw *hw)
{
	u16 mii_reg;

	mii_reg = 0;
	pch_gbe_phy_read_reg_miic(hw, PHY_CONTROL, &mii_reg);
	mii_reg |= MII_CR_POWER_DOWN;
	pch_gbe_phy_write_reg_miic(hw, PHY_CONTROL, mii_reg);
	mdelay(1);
}

inline void pch_gbe_phy_set_rgmii(struct pch_gbe_hw *hw)
{
	pch_gbe_phy_sw_reset(hw);
}

void pch_gbe_phy_init_setting(struct pch_gbe_hw *hw)
{
	struct pch_gbe_adapter *adapter;
	struct ethtool_cmd     cmd = { .cmd = ETHTOOL_GSET };
	int ret;
	u16 mii_reg;

	adapter = container_of(hw, struct pch_gbe_adapter, hw);
	ret = mii_ethtool_gset(&adapter->mii, &cmd);
	if (ret)
		pr_err("Error: mii_ethtool_gset\n");

	ethtool_cmd_speed_set(&cmd, hw->mac.link_speed);
	cmd.duplex = hw->mac.link_duplex;
	cmd.advertising = hw->phy.autoneg_advertised;
	cmd.autoneg = hw->mac.autoneg;
	pch_gbe_phy_write_reg_miic(hw, MII_BMCR, BMCR_RESET);
	ret = mii_ethtool_sset(&adapter->mii, &cmd);
	if (ret)
		pr_err("Error: mii_ethtool_sset\n");

	pch_gbe_phy_sw_reset(hw);

	pch_gbe_phy_read_reg_miic(hw, PHY_PHYSP_CONTROL, &mii_reg);
	mii_reg |= PHYSP_CTRL_ASSERT_CRS_TX;
	pch_gbe_phy_write_reg_miic(hw, PHY_PHYSP_CONTROL, mii_reg);

}
