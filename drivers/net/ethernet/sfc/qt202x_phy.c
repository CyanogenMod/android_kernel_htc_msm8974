/****************************************************************************
 * Driver for Solarflare Solarstorm network controllers and boards
 * Copyright 2006-2010 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include "efx.h"
#include "mdio_10g.h"
#include "phy.h"
#include "nic.h"

#define QT202X_REQUIRED_DEVS (MDIO_DEVS_PCS |		\
			      MDIO_DEVS_PMAPMD |	\
			      MDIO_DEVS_PHYXS)

#define QT202X_LOOPBACKS ((1 << LOOPBACK_PCS) |		\
			  (1 << LOOPBACK_PMAPMD) |	\
			  (1 << LOOPBACK_PHYXS_WS))

#define MDIO_QUAKE_LED0_REG	(0xD006)

#define PCS_FW_HEARTBEAT_REG	0xd7ee
#define PCS_FW_HEARTB_LBN	0
#define PCS_FW_HEARTB_WIDTH	8
#define PCS_FW_PRODUCT_CODE_1	0xd7f0
#define PCS_FW_VERSION_1	0xd7f3
#define PCS_FW_BUILD_1		0xd7f6
#define PCS_UC8051_STATUS_REG	0xd7fd
#define PCS_UC_STATUS_LBN	0
#define PCS_UC_STATUS_WIDTH	8
#define PCS_UC_STATUS_FW_SAVE	0x20
#define PMA_PMD_MODE_REG	0xc301
#define PMA_PMD_RXIN_SEL_LBN	6
#define PMA_PMD_FTX_CTRL2_REG	0xc309
#define PMA_PMD_FTX_STATIC_LBN	13
#define PMA_PMD_VEND1_REG	0xc001
#define PMA_PMD_VEND1_LBTXD_LBN	15
#define PCS_VEND1_REG		0xc000
#define PCS_VEND1_LBTXD_LBN	5

void falcon_qt202x_set_led(struct efx_nic *p, int led, int mode)
{
	int addr = MDIO_QUAKE_LED0_REG + led;
	efx_mdio_write(p, MDIO_MMD_PMAPMD, addr, mode);
}

struct qt202x_phy_data {
	enum efx_phy_mode phy_mode;
	bool bug17190_in_bad_state;
	unsigned long bug17190_timer;
	u32 firmware_ver;
};

#define QT2022C2_MAX_RESET_TIME 500
#define QT2022C2_RESET_WAIT 10

#define QT2025C_MAX_HEARTB_TIME (5 * HZ)
#define QT2025C_HEARTB_WAIT 100
#define QT2025C_MAX_FWSTART_TIME (25 * HZ / 10)
#define QT2025C_FWSTART_WAIT 100

#define BUG17190_INTERVAL (2 * HZ)

static int qt2025c_wait_heartbeat(struct efx_nic *efx)
{
	unsigned long timeout = jiffies + QT2025C_MAX_HEARTB_TIME;
	int reg, old_counter = 0;

	
	for (;;) {
		int counter;
		reg = efx_mdio_read(efx, MDIO_MMD_PCS, PCS_FW_HEARTBEAT_REG);
		if (reg < 0)
			return reg;
		counter = ((reg >> PCS_FW_HEARTB_LBN) &
			    ((1 << PCS_FW_HEARTB_WIDTH) - 1));
		if (old_counter == 0)
			old_counter = counter;
		else if (counter != old_counter)
			break;
		if (time_after(jiffies, timeout)) {
			netif_err(efx, hw, efx->net_dev,
				  "If an SFP+ direct attach cable is"
				  " connected, please check that it complies"
				  " with the SFP+ specification\n");
			return -ETIMEDOUT;
		}
		msleep(QT2025C_HEARTB_WAIT);
	}

	return 0;
}

static int qt2025c_wait_fw_status_good(struct efx_nic *efx)
{
	unsigned long timeout = jiffies + QT2025C_MAX_FWSTART_TIME;
	int reg;

	
	for (;;) {
		reg = efx_mdio_read(efx, MDIO_MMD_PCS, PCS_UC8051_STATUS_REG);
		if (reg < 0)
			return reg;
		if ((reg &
		     ((1 << PCS_UC_STATUS_WIDTH) - 1) << PCS_UC_STATUS_LBN) >=
		    PCS_UC_STATUS_FW_SAVE)
			break;
		if (time_after(jiffies, timeout))
			return -ETIMEDOUT;
		msleep(QT2025C_FWSTART_WAIT);
	}

	return 0;
}

static void qt2025c_restart_firmware(struct efx_nic *efx)
{
	
	efx_mdio_write(efx, 3, 0xe854, 0x00c0);
	efx_mdio_write(efx, 3, 0xe854, 0x0040);
	msleep(50);
}

static int qt2025c_wait_reset(struct efx_nic *efx)
{
	int rc;

	rc = qt2025c_wait_heartbeat(efx);
	if (rc != 0)
		return rc;

	rc = qt2025c_wait_fw_status_good(efx);
	if (rc == -ETIMEDOUT) {
		netif_dbg(efx, hw, efx->net_dev,
			  "bashing QT2025C microcontroller\n");
		qt2025c_restart_firmware(efx);
		rc = qt2025c_wait_heartbeat(efx);
		if (rc != 0)
			return rc;
		rc = qt2025c_wait_fw_status_good(efx);
	}

	return rc;
}

static void qt2025c_firmware_id(struct efx_nic *efx)
{
	struct qt202x_phy_data *phy_data = efx->phy_data;
	u8 firmware_id[9];
	size_t i;

	for (i = 0; i < sizeof(firmware_id); i++)
		firmware_id[i] = efx_mdio_read(efx, MDIO_MMD_PCS,
					       PCS_FW_PRODUCT_CODE_1 + i);
	netif_info(efx, probe, efx->net_dev,
		   "QT2025C firmware %xr%d v%d.%d.%d.%d [20%02d-%02d-%02d]\n",
		   (firmware_id[0] << 8) | firmware_id[1], firmware_id[2],
		   firmware_id[3] >> 4, firmware_id[3] & 0xf,
		   firmware_id[4], firmware_id[5],
		   firmware_id[6], firmware_id[7], firmware_id[8]);
	phy_data->firmware_ver = ((firmware_id[3] & 0xf0) << 20) |
				 ((firmware_id[3] & 0x0f) << 16) |
				 (firmware_id[4] << 8) | firmware_id[5];
}

static void qt2025c_bug17190_workaround(struct efx_nic *efx)
{
	struct qt202x_phy_data *phy_data = efx->phy_data;

	if (efx->link_state.up ||
	    !efx_mdio_links_ok(efx, MDIO_DEVS_PMAPMD | MDIO_DEVS_PHYXS)) {
		phy_data->bug17190_in_bad_state = false;
		return;
	}

	if (!phy_data->bug17190_in_bad_state) {
		phy_data->bug17190_in_bad_state = true;
		phy_data->bug17190_timer = jiffies + BUG17190_INTERVAL;
		return;
	}

	if (time_after_eq(jiffies, phy_data->bug17190_timer)) {
		netif_dbg(efx, hw, efx->net_dev, "bashing QT2025C PMA/PMD\n");
		efx_mdio_set_flag(efx, MDIO_MMD_PMAPMD, MDIO_CTRL1,
				  MDIO_PMA_CTRL1_LOOPBACK, true);
		msleep(100);
		efx_mdio_set_flag(efx, MDIO_MMD_PMAPMD, MDIO_CTRL1,
				  MDIO_PMA_CTRL1_LOOPBACK, false);
		phy_data->bug17190_timer = jiffies + BUG17190_INTERVAL;
	}
}

static int qt2025c_select_phy_mode(struct efx_nic *efx)
{
	struct qt202x_phy_data *phy_data = efx->phy_data;
	struct falcon_board *board = falcon_board(efx);
	int reg, rc, i;
	uint16_t phy_op_mode;

	if (phy_data->firmware_ver < 0x02000100)
		return 0;

	phy_op_mode = (efx->loopback_mode == LOOPBACK_NONE) ? 0x0038 : 0x0020;

	
	reg = efx_mdio_read(efx, 1, 0xc319);
	if ((reg & 0x0038) == phy_op_mode)
		return 0;
	netif_dbg(efx, hw, efx->net_dev, "Switching PHY to mode 0x%04x\n",
		  phy_op_mode);

	efx_mdio_write(efx, 1, 0xc300, 0x0000);
	if (board->major == 0 && board->minor < 2) {
		efx_mdio_write(efx, 1, 0xc303, 0x4498);
		for (i = 0; i < 9; i++) {
			efx_mdio_write(efx, 1, 0xc303, 0x4488);
			efx_mdio_write(efx, 1, 0xc303, 0x4480);
			efx_mdio_write(efx, 1, 0xc303, 0x4490);
			efx_mdio_write(efx, 1, 0xc303, 0x4498);
		}
	} else {
		efx_mdio_write(efx, 1, 0xc303, 0x0920);
		efx_mdio_write(efx, 1, 0xd008, 0x0004);
		for (i = 0; i < 9; i++) {
			efx_mdio_write(efx, 1, 0xc303, 0x0900);
			efx_mdio_write(efx, 1, 0xd008, 0x0005);
			efx_mdio_write(efx, 1, 0xc303, 0x0920);
			efx_mdio_write(efx, 1, 0xd008, 0x0004);
		}
		efx_mdio_write(efx, 1, 0xc303, 0x4900);
	}
	efx_mdio_write(efx, 1, 0xc303, 0x4900);
	efx_mdio_write(efx, 1, 0xc302, 0x0004);
	efx_mdio_write(efx, 1, 0xc316, 0x0013);
	efx_mdio_write(efx, 1, 0xc318, 0x0054);
	efx_mdio_write(efx, 1, 0xc319, phy_op_mode);
	efx_mdio_write(efx, 1, 0xc31a, 0x0098);
	efx_mdio_write(efx, 3, 0x0026, 0x0e00);
	efx_mdio_write(efx, 3, 0x0027, 0x0013);
	efx_mdio_write(efx, 3, 0x0028, 0xa528);
	efx_mdio_write(efx, 1, 0xd006, 0x000a);
	efx_mdio_write(efx, 1, 0xd007, 0x0009);
	efx_mdio_write(efx, 1, 0xd008, 0x0004);
	efx_mdio_write(efx, 1, 0xc317, 0x00ff);
	efx_mdio_set_flag(efx, 1, PMA_PMD_MODE_REG,
			  1 << PMA_PMD_RXIN_SEL_LBN, false);
	efx_mdio_write(efx, 1, 0xc300, 0x0002);
	msleep(20);

	
	qt2025c_restart_firmware(efx);

	
	rc = qt2025c_wait_reset(efx);
	if (rc < 0) {
		netif_err(efx, hw, efx->net_dev,
			  "PHY microcontroller reset during mode switch "
			  "timed out\n");
		return rc;
	}

	return 0;
}

static int qt202x_reset_phy(struct efx_nic *efx)
{
	int rc;

	if (efx->phy_type == PHY_TYPE_QT2025C) {
		rc = qt2025c_wait_reset(efx);
		if (rc < 0)
			goto fail;
	} else {
		rc = efx_mdio_reset_mmd(efx, MDIO_MMD_PHYXS,
					QT2022C2_MAX_RESET_TIME /
					QT2022C2_RESET_WAIT,
					QT2022C2_RESET_WAIT);
		if (rc < 0)
			goto fail;
	}

	
	msleep(250);

	falcon_board(efx)->type->init_phy(efx);

	return 0;

 fail:
	netif_err(efx, hw, efx->net_dev, "PHY reset timed out\n");
	return rc;
}

static int qt202x_phy_probe(struct efx_nic *efx)
{
	struct qt202x_phy_data *phy_data;

	phy_data = kzalloc(sizeof(struct qt202x_phy_data), GFP_KERNEL);
	if (!phy_data)
		return -ENOMEM;
	efx->phy_data = phy_data;
	phy_data->phy_mode = efx->phy_mode;
	phy_data->bug17190_in_bad_state = false;
	phy_data->bug17190_timer = 0;

	efx->mdio.mmds = QT202X_REQUIRED_DEVS;
	efx->mdio.mode_support = MDIO_SUPPORTS_C45 | MDIO_EMULATE_C22;
	efx->loopback_modes = QT202X_LOOPBACKS | FALCON_XMAC_LOOPBACKS;
	return 0;
}

static int qt202x_phy_init(struct efx_nic *efx)
{
	u32 devid;
	int rc;

	rc = qt202x_reset_phy(efx);
	if (rc) {
		netif_err(efx, probe, efx->net_dev, "PHY init failed\n");
		return rc;
	}

	devid = efx_mdio_read_id(efx, MDIO_MMD_PHYXS);
	netif_info(efx, probe, efx->net_dev,
		   "PHY ID reg %x (OUI %06x model %02x revision %x)\n",
		   devid, efx_mdio_id_oui(devid), efx_mdio_id_model(devid),
		   efx_mdio_id_rev(devid));

	if (efx->phy_type == PHY_TYPE_QT2025C)
		qt2025c_firmware_id(efx);

	return 0;
}

static int qt202x_link_ok(struct efx_nic *efx)
{
	return efx_mdio_links_ok(efx, QT202X_REQUIRED_DEVS);
}

static bool qt202x_phy_poll(struct efx_nic *efx)
{
	bool was_up = efx->link_state.up;

	efx->link_state.up = qt202x_link_ok(efx);
	efx->link_state.speed = 10000;
	efx->link_state.fd = true;
	efx->link_state.fc = efx->wanted_fc;

	if (efx->phy_type == PHY_TYPE_QT2025C)
		qt2025c_bug17190_workaround(efx);

	return efx->link_state.up != was_up;
}

static int qt202x_phy_reconfigure(struct efx_nic *efx)
{
	struct qt202x_phy_data *phy_data = efx->phy_data;

	if (efx->phy_type == PHY_TYPE_QT2025C) {
		int rc = qt2025c_select_phy_mode(efx);
		if (rc)
			return rc;

		mdio_set_flag(
			&efx->mdio, efx->mdio.prtad, MDIO_MMD_PMAPMD,
			PMA_PMD_FTX_CTRL2_REG, 1 << PMA_PMD_FTX_STATIC_LBN,
			efx->phy_mode & PHY_MODE_TX_DISABLED ||
			efx->phy_mode & PHY_MODE_LOW_POWER ||
			efx->loopback_mode == LOOPBACK_PCS ||
			efx->loopback_mode == LOOPBACK_PMAPMD);
	} else {
		
		if (!(efx->phy_mode & PHY_MODE_TX_DISABLED) &&
		    (phy_data->phy_mode & PHY_MODE_TX_DISABLED))
			qt202x_reset_phy(efx);

		efx_mdio_transmit_disable(efx);
	}

	efx_mdio_phy_reconfigure(efx);

	phy_data->phy_mode = efx->phy_mode;

	return 0;
}

static void qt202x_phy_get_settings(struct efx_nic *efx, struct ethtool_cmd *ecmd)
{
	mdio45_ethtool_gset(&efx->mdio, ecmd);
}

static void qt202x_phy_remove(struct efx_nic *efx)
{
	
	kfree(efx->phy_data);
	efx->phy_data = NULL;
}

const struct efx_phy_operations falcon_qt202x_phy_ops = {
	.probe		 = qt202x_phy_probe,
	.init		 = qt202x_phy_init,
	.reconfigure	 = qt202x_phy_reconfigure,
	.poll		 = qt202x_phy_poll,
	.fini		 = efx_port_dummy_op_void,
	.remove		 = qt202x_phy_remove,
	.get_settings	 = qt202x_phy_get_settings,
	.set_settings	 = efx_mdio_set_settings,
	.test_alive	 = efx_mdio_test_alive,
};
