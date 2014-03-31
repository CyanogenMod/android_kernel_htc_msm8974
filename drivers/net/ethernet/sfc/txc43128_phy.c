/****************************************************************************
 * Driver for Solarflare Solarstorm network controllers and boards
 * Copyright 2006-2011 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */


#include <linux/delay.h>
#include <linux/slab.h>
#include "efx.h"
#include "mdio_10g.h"
#include "phy.h"
#include "nic.h"

#define TXC_REQUIRED_DEVS (MDIO_DEVS_PCS |	\
			   MDIO_DEVS_PMAPMD |	\
			   MDIO_DEVS_PHYXS)

#define TXC_LOOPBACKS ((1 << LOOPBACK_PCS) |	\
		       (1 << LOOPBACK_PMAPMD) |	\
		       (1 << LOOPBACK_PHYXS_WS))

#define TXCNAME "TXC43128"
#define TXC_MAX_RESET_TIME	500
#define TXC_RESET_WAIT		10
#define TXC_BIST_DURATION	50


#define TXC_GLRGS_GLCMD		0xc004
#define TXC_GLCMD_L01PD_LBN	5
#define TXC_GLCMD_L23PD_LBN	6
#define TXC_GLCMD_LMTSWRST_LBN	14

#define TXC_GLRGS_GSGQLCTL	0xc01a
#define TXC_GSGQLCT_SGQLEN_LBN	15
#define TXC_GSGQLCT_LNSL_LBN	13
#define TXC_GSGQLCT_LNSL_WIDTH	2

#define TXC_ALRGS_ATXCTL	0xc040
#define TXC_ATXCTL_TXPD3_LBN	15
#define TXC_ATXCTL_TXPD2_LBN	14
#define TXC_ATXCTL_TXPD1_LBN	13
#define TXC_ATXCTL_TXPD0_LBN	12

#define TXC_ALRGS_ATXAMP0	0xc041
#define TXC_ALRGS_ATXAMP1	0xc042
#define TXC_ATXAMP_LANE02_LBN	3
#define TXC_ATXAMP_LANE13_LBN	11

#define TXC_ATXAMP_1280_mV	0
#define TXC_ATXAMP_1200_mV	8
#define TXC_ATXAMP_1120_mV	12
#define TXC_ATXAMP_1060_mV	14
#define TXC_ATXAMP_0820_mV	25
#define TXC_ATXAMP_0720_mV	26
#define TXC_ATXAMP_0580_mV	27
#define TXC_ATXAMP_0440_mV	28

#define TXC_ATXAMP_0820_BOTH					\
	((TXC_ATXAMP_0820_mV << TXC_ATXAMP_LANE02_LBN)		\
	 | (TXC_ATXAMP_0820_mV << TXC_ATXAMP_LANE13_LBN))

#define TXC_ATXAMP_DEFAULT	0x6060 

#define TXC_ALRGS_ATXPRE0	0xc043
#define TXC_ALRGS_ATXPRE1	0xc044

#define TXC_ATXPRE_NONE 0
#define TXC_ATXPRE_DEFAULT	0x1010 

#define TXC_ALRGS_ARXCTL	0xc045
#define TXC_ARXCTL_RXPD3_LBN	15
#define TXC_ARXCTL_RXPD2_LBN	14
#define TXC_ARXCTL_RXPD1_LBN	13
#define TXC_ARXCTL_RXPD0_LBN	12

#define TXC_MRGS_CTL		0xc340
#define TXC_MCTL_RESET_LBN	15	
#define TXC_MCTL_TXLED_LBN	14	
#define TXC_MCTL_RXLED_LBN	13	

#define TXC_GPIO_OUTPUT		0xc346
#define TXC_GPIO_DIR		0xc348

#define TXC_BIST_CTL		0xc280
#define TXC_BIST_TXFRMCNT	0xc281
#define TXC_BIST_RX0FRMCNT	0xc282
#define TXC_BIST_RX1FRMCNT	0xc283
#define TXC_BIST_RX2FRMCNT	0xc284
#define TXC_BIST_RX3FRMCNT	0xc285
#define TXC_BIST_RX0ERRCNT	0xc286
#define TXC_BIST_RX1ERRCNT	0xc287
#define TXC_BIST_RX2ERRCNT	0xc288
#define TXC_BIST_RX3ERRCNT	0xc289

#define TXC_BIST_CTRL_TYPE_LBN	10
#define TXC_BIST_CTRL_TYPE_TSD	0	
#define TXC_BIST_CTRL_TYPE_CRP	1	
#define TXC_BIST_CTRL_TYPE_CJP	2	
#define TXC_BIST_CTRL_TYPE_TSR	3	
#define TXC_BIST_CTRL_B10EN_LBN	12
#define TXC_BIST_CTRL_ENAB_LBN	13
#define TXC_BIST_CTRL_STOP_LBN	14
#define TXC_BIST_CTRL_STRT_LBN	15

#define TXC_MTDIABLO_CTRL	0xc34f
#define TXC_MTDIABLO_CTRL_PMA_LOOP_LBN	10

struct txc43128_data {
	unsigned long bug10934_timer;
	enum efx_phy_mode phy_mode;
	enum efx_loopback_mode loopback_mode;
};

#define BUG10934_RESET_INTERVAL (5 * HZ)

static void txc_reset_logic(struct efx_nic *efx);

void falcon_txc_set_gpio_val(struct efx_nic *efx, int pin, int on)
{
	efx_mdio_set_flag(efx, MDIO_MMD_PHYXS, TXC_GPIO_OUTPUT, 1 << pin, on);
}

void falcon_txc_set_gpio_dir(struct efx_nic *efx, int pin, int dir)
{
	efx_mdio_set_flag(efx, MDIO_MMD_PHYXS, TXC_GPIO_DIR, 1 << pin, dir);
}

static int txc_reset_phy(struct efx_nic *efx)
{
	int rc = efx_mdio_reset_mmd(efx, MDIO_MMD_PMAPMD,
				    TXC_MAX_RESET_TIME / TXC_RESET_WAIT,
				    TXC_RESET_WAIT);
	if (rc < 0)
		goto fail;

	
	rc = efx_mdio_check_mmds(efx, TXC_REQUIRED_DEVS);
	if (rc < 0)
		goto fail;

	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, TXCNAME ": reset timed out!\n");
	return rc;
}

static int txc_bist_one(struct efx_nic *efx, int mmd, int test)
{
	int ctrl, bctl;
	int lane;
	int rc = 0;

	
	ctrl = efx_mdio_read(efx, MDIO_MMD_PCS, TXC_MTDIABLO_CTRL);
	ctrl |= (1 << TXC_MTDIABLO_CTRL_PMA_LOOP_LBN);
	efx_mdio_write(efx, MDIO_MMD_PCS, TXC_MTDIABLO_CTRL, ctrl);

	
	
	bctl = (test << TXC_BIST_CTRL_TYPE_LBN);
	efx_mdio_write(efx, mmd, TXC_BIST_CTL, bctl);

	
	bctl |= (1 << TXC_BIST_CTRL_ENAB_LBN);
	efx_mdio_write(efx, mmd, TXC_BIST_CTL, bctl);

	
	efx_mdio_write(efx, mmd, TXC_BIST_CTL,
		       bctl | (1 << TXC_BIST_CTRL_STRT_LBN));

	
	udelay(TXC_BIST_DURATION);

	
	bctl |= (1 << TXC_BIST_CTRL_STOP_LBN);
	efx_mdio_write(efx, mmd, TXC_BIST_CTL, bctl);

	
	while (bctl & (1 << TXC_BIST_CTRL_STOP_LBN))
		bctl = efx_mdio_read(efx, mmd, TXC_BIST_CTL);

	for (lane = 0; lane < 4; lane++) {
		int count = efx_mdio_read(efx, mmd, TXC_BIST_RX0ERRCNT + lane);
		if (count != 0) {
			netif_err(efx, hw, efx->net_dev, TXCNAME": BIST error. "
				  "Lane %d had %d errs\n", lane, count);
			rc = -EIO;
		}
		count = efx_mdio_read(efx, mmd, TXC_BIST_RX0FRMCNT + lane);
		if (count == 0) {
			netif_err(efx, hw, efx->net_dev, TXCNAME": BIST error. "
				  "Lane %d got 0 frames\n", lane);
			rc = -EIO;
		}
	}

	if (rc == 0)
		netif_info(efx, hw, efx->net_dev, TXCNAME": BIST pass\n");

	
	efx_mdio_write(efx, mmd, TXC_BIST_CTL, 0);

	
	ctrl &= ~(1 << TXC_MTDIABLO_CTRL_PMA_LOOP_LBN);
	efx_mdio_write(efx, MDIO_MMD_PCS, TXC_MTDIABLO_CTRL, ctrl);

	return rc;
}

static int txc_bist(struct efx_nic *efx)
{
	return txc_bist_one(efx, MDIO_MMD_PCS, TXC_BIST_CTRL_TYPE_TSD);
}

static void txc_apply_defaults(struct efx_nic *efx)
{
	int mctrl;


	
	efx_mdio_write(efx, MDIO_MMD_PHYXS, TXC_ALRGS_ATXPRE0, TXC_ATXPRE_NONE);
	efx_mdio_write(efx, MDIO_MMD_PHYXS, TXC_ALRGS_ATXPRE1, TXC_ATXPRE_NONE);

	
	efx_mdio_write(efx, MDIO_MMD_PHYXS,
		       TXC_ALRGS_ATXAMP0, TXC_ATXAMP_0820_BOTH);
	efx_mdio_write(efx, MDIO_MMD_PHYXS,
		       TXC_ALRGS_ATXAMP1, TXC_ATXAMP_0820_BOTH);

	efx_mdio_write(efx, MDIO_MMD_PMAPMD,
		       TXC_ALRGS_ATXPRE0, TXC_ATXPRE_DEFAULT);
	efx_mdio_write(efx, MDIO_MMD_PMAPMD,
		       TXC_ALRGS_ATXPRE1, TXC_ATXPRE_DEFAULT);
	efx_mdio_write(efx, MDIO_MMD_PMAPMD,
		       TXC_ALRGS_ATXAMP0, TXC_ATXAMP_DEFAULT);
	efx_mdio_write(efx, MDIO_MMD_PMAPMD,
		       TXC_ALRGS_ATXAMP1, TXC_ATXAMP_DEFAULT);

	
	mctrl = efx_mdio_read(efx, MDIO_MMD_PHYXS, TXC_MRGS_CTL);

	
	mctrl &= ~((1 << TXC_MCTL_TXLED_LBN) | (1 << TXC_MCTL_RXLED_LBN));
	efx_mdio_write(efx, MDIO_MMD_PHYXS, TXC_MRGS_CTL, mctrl);

	
	txc_reset_logic(efx);

	falcon_board(efx)->type->init_phy(efx);
}

static int txc43128_phy_probe(struct efx_nic *efx)
{
	struct txc43128_data *phy_data;

	
	phy_data = kzalloc(sizeof(*phy_data), GFP_KERNEL);
	if (!phy_data)
		return -ENOMEM;
	efx->phy_data = phy_data;
	phy_data->phy_mode = efx->phy_mode;

	efx->mdio.mmds = TXC_REQUIRED_DEVS;
	efx->mdio.mode_support = MDIO_SUPPORTS_C45 | MDIO_EMULATE_C22;

	efx->loopback_modes = TXC_LOOPBACKS | FALCON_XMAC_LOOPBACKS;

	return 0;
}

static int txc43128_phy_init(struct efx_nic *efx)
{
	int rc;

	rc = txc_reset_phy(efx);
	if (rc < 0)
		return rc;

	rc = txc_bist(efx);
	if (rc < 0)
		return rc;

	txc_apply_defaults(efx);

	return 0;
}

static void txc_glrgs_lane_power(struct efx_nic *efx, int mmd)
{
	int pd = (1 << TXC_GLCMD_L01PD_LBN) | (1 << TXC_GLCMD_L23PD_LBN);
	int ctl = efx_mdio_read(efx, mmd, TXC_GLRGS_GLCMD);

	if (!(efx->phy_mode & PHY_MODE_LOW_POWER))
		ctl &= ~pd;
	else
		ctl |= pd;

	efx_mdio_write(efx, mmd, TXC_GLRGS_GLCMD, ctl);
}

static void txc_analog_lane_power(struct efx_nic *efx, int mmd)
{
	int txpd = (1 << TXC_ATXCTL_TXPD3_LBN) | (1 << TXC_ATXCTL_TXPD2_LBN)
		| (1 << TXC_ATXCTL_TXPD1_LBN) | (1 << TXC_ATXCTL_TXPD0_LBN);
	int rxpd = (1 << TXC_ARXCTL_RXPD3_LBN) | (1 << TXC_ARXCTL_RXPD2_LBN)
		| (1 << TXC_ARXCTL_RXPD1_LBN) | (1 << TXC_ARXCTL_RXPD0_LBN);
	int txctl = efx_mdio_read(efx, mmd, TXC_ALRGS_ATXCTL);
	int rxctl = efx_mdio_read(efx, mmd, TXC_ALRGS_ARXCTL);

	if (!(efx->phy_mode & PHY_MODE_LOW_POWER)) {
		txctl &= ~txpd;
		rxctl &= ~rxpd;
	} else {
		txctl |= txpd;
		rxctl |= rxpd;
	}

	efx_mdio_write(efx, mmd, TXC_ALRGS_ATXCTL, txctl);
	efx_mdio_write(efx, mmd, TXC_ALRGS_ARXCTL, rxctl);
}

static void txc_set_power(struct efx_nic *efx)
{
	
	efx_mdio_set_mmds_lpower(efx,
				 !!(efx->phy_mode & PHY_MODE_LOW_POWER),
				 TXC_REQUIRED_DEVS);

	txc_glrgs_lane_power(efx, MDIO_MMD_PCS);
	txc_glrgs_lane_power(efx, MDIO_MMD_PHYXS);

	
	txc_analog_lane_power(efx, MDIO_MMD_PMAPMD);
	txc_analog_lane_power(efx, MDIO_MMD_PHYXS);
}

static void txc_reset_logic_mmd(struct efx_nic *efx, int mmd)
{
	int val = efx_mdio_read(efx, mmd, TXC_GLRGS_GLCMD);
	int tries = 50;

	val |= (1 << TXC_GLCMD_LMTSWRST_LBN);
	efx_mdio_write(efx, mmd, TXC_GLRGS_GLCMD, val);
	while (tries--) {
		val = efx_mdio_read(efx, mmd, TXC_GLRGS_GLCMD);
		if (!(val & (1 << TXC_GLCMD_LMTSWRST_LBN)))
			break;
		udelay(1);
	}
	if (!tries)
		netif_info(efx, hw, efx->net_dev,
			   TXCNAME " Logic reset timed out!\n");
}

static void txc_reset_logic(struct efx_nic *efx)
{
	txc_reset_logic_mmd(efx, MDIO_MMD_PCS);
}

static bool txc43128_phy_read_link(struct efx_nic *efx)
{
	return efx_mdio_links_ok(efx, TXC_REQUIRED_DEVS);
}

static int txc43128_phy_reconfigure(struct efx_nic *efx)
{
	struct txc43128_data *phy_data = efx->phy_data;
	enum efx_phy_mode mode_change = efx->phy_mode ^ phy_data->phy_mode;
	bool loop_change = LOOPBACK_CHANGED(phy_data, efx, TXC_LOOPBACKS);

	if (efx->phy_mode & mode_change & PHY_MODE_TX_DISABLED) {
		txc_reset_phy(efx);
		txc_apply_defaults(efx);
		falcon_reset_xaui(efx);
		mode_change &= ~PHY_MODE_TX_DISABLED;
	}

	efx_mdio_transmit_disable(efx);
	efx_mdio_phy_reconfigure(efx);
	if (mode_change & PHY_MODE_LOW_POWER)
		txc_set_power(efx);

	if (loop_change || mode_change)
		txc_reset_logic(efx);

	phy_data->phy_mode = efx->phy_mode;
	phy_data->loopback_mode = efx->loopback_mode;

	return 0;
}

static void txc43128_phy_fini(struct efx_nic *efx)
{
	
	efx_mdio_write(efx, MDIO_MMD_PMAPMD, MDIO_PMA_LASI_CTRL, 0);
}

static void txc43128_phy_remove(struct efx_nic *efx)
{
	kfree(efx->phy_data);
	efx->phy_data = NULL;
}

static bool txc43128_phy_poll(struct efx_nic *efx)
{
	struct txc43128_data *data = efx->phy_data;
	bool was_up = efx->link_state.up;

	efx->link_state.up = txc43128_phy_read_link(efx);
	efx->link_state.speed = 10000;
	efx->link_state.fd = true;
	efx->link_state.fc = efx->wanted_fc;

	if (efx->link_state.up || (efx->loopback_mode != LOOPBACK_NONE)) {
		data->bug10934_timer = jiffies;
	} else {
		if (time_after_eq(jiffies, (data->bug10934_timer +
					    BUG10934_RESET_INTERVAL))) {
			data->bug10934_timer = jiffies;
			txc_reset_logic(efx);
		}
	}

	return efx->link_state.up != was_up;
}

static const char *const txc43128_test_names[] = {
	"bist"
};

static const char *txc43128_test_name(struct efx_nic *efx, unsigned int index)
{
	if (index < ARRAY_SIZE(txc43128_test_names))
		return txc43128_test_names[index];
	return NULL;
}

static int txc43128_run_tests(struct efx_nic *efx, int *results, unsigned flags)
{
	int rc;

	if (!(flags & ETH_TEST_FL_OFFLINE))
		return 0;

	rc = txc_reset_phy(efx);
	if (rc < 0)
		return rc;

	rc = txc_bist(efx);
	txc_apply_defaults(efx);
	results[0] = rc ? -1 : 1;
	return rc;
}

static void txc43128_get_settings(struct efx_nic *efx, struct ethtool_cmd *ecmd)
{
	mdio45_ethtool_gset(&efx->mdio, ecmd);
}

const struct efx_phy_operations falcon_txc_phy_ops = {
	.probe		= txc43128_phy_probe,
	.init		= txc43128_phy_init,
	.reconfigure	= txc43128_phy_reconfigure,
	.poll		= txc43128_phy_poll,
	.fini		= txc43128_phy_fini,
	.remove		= txc43128_phy_remove,
	.get_settings	= txc43128_get_settings,
	.set_settings	= efx_mdio_set_settings,
	.test_alive	= efx_mdio_test_alive,
	.run_tests	= txc43128_run_tests,
	.test_name	= txc43128_test_name,
};
