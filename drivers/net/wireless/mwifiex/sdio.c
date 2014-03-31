/*
 * Marvell Wireless LAN device driver: SDIO specific handling
 *
 * Copyright (C) 2011, Marvell International Ltd.
 *
 * This software file (the "File") is distributed by Marvell International
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991
 * (the "License").  You may use, redistribute and/or modify this File in
 * accordance with the terms and conditions of the License, a copy of which
 * is available by writing to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 * this warranty disclaimer.
 */

#include <linux/firmware.h>

#include "decl.h"
#include "ioctl.h"
#include "util.h"
#include "fw.h"
#include "main.h"
#include "wmm.h"
#include "11n.h"
#include "sdio.h"


#define SDIO_VERSION	"1.0"

static u8 user_rmmod;

static struct mwifiex_if_ops sdio_ops;

static struct semaphore add_remove_card_sem;

static int mwifiex_sdio_resume(struct device *dev);

static int
mwifiex_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	int ret;
	struct sdio_mmc_card *card = NULL;

	pr_debug("info: vendor=0x%4.04X device=0x%4.04X class=%d function=%d\n",
		 func->vendor, func->device, func->class, func->num);

	card = kzalloc(sizeof(struct sdio_mmc_card), GFP_KERNEL);
	if (!card)
		return -ENOMEM;

	card->func = func;

	func->card->quirks |= MMC_QUIRK_BLKSZ_FOR_BYTE_MODE;

	sdio_claim_host(func);
	ret = sdio_enable_func(func);
	sdio_release_host(func);

	if (ret) {
		pr_err("%s: failed to enable function\n", __func__);
		kfree(card);
		return -EIO;
	}

	if (mwifiex_add_card(card, &add_remove_card_sem, &sdio_ops,
			     MWIFIEX_SDIO)) {
		pr_err("%s: add card failed\n", __func__);
		kfree(card);
		sdio_claim_host(func);
		ret = sdio_disable_func(func);
		sdio_release_host(func);
		ret = -1;
	}

	return ret;
}

static void
mwifiex_sdio_remove(struct sdio_func *func)
{
	struct sdio_mmc_card *card;
	struct mwifiex_adapter *adapter;
	struct mwifiex_private *priv;
	int i;

	pr_debug("info: SDIO func num=%d\n", func->num);

	card = sdio_get_drvdata(func);
	if (!card)
		return;

	adapter = card->adapter;
	if (!adapter || !adapter->priv_num)
		return;

	if (user_rmmod) {
		if (adapter->is_suspended)
			mwifiex_sdio_resume(adapter->dev);

		for (i = 0; i < adapter->priv_num; i++)
			if ((GET_BSS_ROLE(adapter->priv[i]) ==
						MWIFIEX_BSS_ROLE_STA) &&
			    adapter->priv[i]->media_connected)
				mwifiex_deauthenticate(adapter->priv[i], NULL);

		priv = mwifiex_get_priv(adapter, MWIFIEX_BSS_ROLE_ANY);
		mwifiex_disable_auto_ds(priv);
		mwifiex_init_shutdown_fw(priv, MWIFIEX_FUNC_SHUTDOWN);
	}

	mwifiex_remove_card(card->adapter, &add_remove_card_sem);
	kfree(card);
}

static int mwifiex_sdio_suspend(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	struct sdio_mmc_card *card;
	struct mwifiex_adapter *adapter;
	mmc_pm_flag_t pm_flag = 0;
	int hs_actived = 0;
	int i;
	int ret = 0;

	if (func) {
		pm_flag = sdio_get_host_pm_caps(func);
		pr_debug("cmd: %s: suspend: PM flag = 0x%x\n",
			 sdio_func_id(func), pm_flag);
		if (!(pm_flag & MMC_PM_KEEP_POWER)) {
			pr_err("%s: cannot remain alive while host is"
				" suspended\n", sdio_func_id(func));
			return -ENOSYS;
		}

		card = sdio_get_drvdata(func);
		if (!card || !card->adapter) {
			pr_err("suspend: invalid card or adapter\n");
			return 0;
		}
	} else {
		pr_err("suspend: sdio_func is not specified\n");
		return 0;
	}

	adapter = card->adapter;

	
	hs_actived = mwifiex_enable_hs(adapter);
	if (hs_actived) {
		pr_debug("cmd: suspend with MMC_PM_KEEP_POWER\n");
		ret = sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER);
	}

	
	adapter->is_suspended = true;

	for (i = 0; i < adapter->priv_num; i++)
		netif_carrier_off(adapter->priv[i]->netdev);

	return ret;
}

static int mwifiex_sdio_resume(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	struct sdio_mmc_card *card;
	struct mwifiex_adapter *adapter;
	mmc_pm_flag_t pm_flag = 0;
	int i;

	if (func) {
		pm_flag = sdio_get_host_pm_caps(func);
		card = sdio_get_drvdata(func);
		if (!card || !card->adapter) {
			pr_err("resume: invalid card or adapter\n");
			return 0;
		}
	} else {
		pr_err("resume: sdio_func is not specified\n");
		return 0;
	}

	adapter = card->adapter;

	if (!adapter->is_suspended) {
		dev_warn(adapter->dev, "device already resumed\n");
		return 0;
	}

	adapter->is_suspended = false;

	for (i = 0; i < adapter->priv_num; i++)
		if (adapter->priv[i]->media_connected)
			netif_carrier_on(adapter->priv[i]->netdev);

	
	mwifiex_cancel_hs(mwifiex_get_priv(adapter, MWIFIEX_BSS_ROLE_STA),
			  MWIFIEX_ASYNC_CMD);

	return 0;
}

#define SDIO_DEVICE_ID_MARVELL_8787   (0x9119)
#define SDIO_DEVICE_ID_MARVELL_8797   (0x9129)

static const struct sdio_device_id mwifiex_ids[] = {
	{SDIO_DEVICE(SDIO_VENDOR_ID_MARVELL, SDIO_DEVICE_ID_MARVELL_8787)},
	{SDIO_DEVICE(SDIO_VENDOR_ID_MARVELL, SDIO_DEVICE_ID_MARVELL_8797)},
	{},
};

MODULE_DEVICE_TABLE(sdio, mwifiex_ids);

static const struct dev_pm_ops mwifiex_sdio_pm_ops = {
	.suspend = mwifiex_sdio_suspend,
	.resume = mwifiex_sdio_resume,
};

static struct sdio_driver mwifiex_sdio = {
	.name = "mwifiex_sdio",
	.id_table = mwifiex_ids,
	.probe = mwifiex_sdio_probe,
	.remove = mwifiex_sdio_remove,
	.drv = {
		.owner = THIS_MODULE,
		.pm = &mwifiex_sdio_pm_ops,
	}
};

static int
mwifiex_write_reg(struct mwifiex_adapter *adapter, u32 reg, u32 data)
{
	struct sdio_mmc_card *card = adapter->card;
	int ret = -1;

	sdio_claim_host(card->func);
	sdio_writeb(card->func, (u8) data, reg, &ret);
	sdio_release_host(card->func);

	return ret;
}

static int
mwifiex_read_reg(struct mwifiex_adapter *adapter, u32 reg, u32 *data)
{
	struct sdio_mmc_card *card = adapter->card;
	int ret = -1;
	u8 val;

	sdio_claim_host(card->func);
	val = sdio_readb(card->func, reg, &ret);
	sdio_release_host(card->func);

	*data = val;

	return ret;
}

static int
mwifiex_write_data_sync(struct mwifiex_adapter *adapter,
			u8 *buffer, u32 pkt_len, u32 port)
{
	struct sdio_mmc_card *card = adapter->card;
	int ret = -1;
	u8 blk_mode =
		(port & MWIFIEX_SDIO_BYTE_MODE_MASK) ? BYTE_MODE : BLOCK_MODE;
	u32 blk_size = (blk_mode == BLOCK_MODE) ? MWIFIEX_SDIO_BLOCK_SIZE : 1;
	u32 blk_cnt =
		(blk_mode ==
		 BLOCK_MODE) ? (pkt_len /
				MWIFIEX_SDIO_BLOCK_SIZE) : pkt_len;
	u32 ioport = (port & MWIFIEX_SDIO_IO_PORT_MASK);

	if (adapter->is_suspended) {
		dev_err(adapter->dev,
			"%s: not allowed while suspended\n", __func__);
		return -1;
	}

	sdio_claim_host(card->func);

	if (!sdio_writesb(card->func, ioport, buffer, blk_cnt * blk_size))
		ret = 0;

	sdio_release_host(card->func);

	return ret;
}

static int mwifiex_read_data_sync(struct mwifiex_adapter *adapter, u8 *buffer,
				  u32 len, u32 port, u8 claim)
{
	struct sdio_mmc_card *card = adapter->card;
	int ret = -1;
	u8 blk_mode = (port & MWIFIEX_SDIO_BYTE_MODE_MASK) ? BYTE_MODE
		       : BLOCK_MODE;
	u32 blk_size = (blk_mode == BLOCK_MODE) ? MWIFIEX_SDIO_BLOCK_SIZE : 1;
	u32 blk_cnt = (blk_mode == BLOCK_MODE) ? (len / MWIFIEX_SDIO_BLOCK_SIZE)
			: len;
	u32 ioport = (port & MWIFIEX_SDIO_IO_PORT_MASK);

	if (claim)
		sdio_claim_host(card->func);

	if (!sdio_readsb(card->func, buffer, ioport, blk_cnt * blk_size))
		ret = 0;

	if (claim)
		sdio_release_host(card->func);

	return ret;
}

/*
 * This function wakes up the card.
 *
 * A host power up command is written to the card configuration
 * register to wake up the card.
 */
static int mwifiex_pm_wakeup_card(struct mwifiex_adapter *adapter)
{
	dev_dbg(adapter->dev, "event: wakeup device...\n");

	return mwifiex_write_reg(adapter, CONFIGURATION_REG, HOST_POWER_UP);
}

static int mwifiex_pm_wakeup_card_complete(struct mwifiex_adapter *adapter)
{
	dev_dbg(adapter->dev, "cmd: wakeup device completed\n");

	return mwifiex_write_reg(adapter, CONFIGURATION_REG, 0);
}

static int mwifiex_init_sdio_ioport(struct mwifiex_adapter *adapter)
{
	u32 reg;

	adapter->ioport = 0;

	
	if (!mwifiex_read_reg(adapter, IO_PORT_0_REG, &reg))
		adapter->ioport |= (reg & 0xff);
	else
		return -1;

	if (!mwifiex_read_reg(adapter, IO_PORT_1_REG, &reg))
		adapter->ioport |= ((reg & 0xff) << 8);
	else
		return -1;

	if (!mwifiex_read_reg(adapter, IO_PORT_2_REG, &reg))
		adapter->ioport |= ((reg & 0xff) << 16);
	else
		return -1;

	pr_debug("info: SDIO FUNC1 IO port: %#x\n", adapter->ioport);

	
	if (!mwifiex_read_reg(adapter, HOST_INT_RSR_REG, &reg))
		mwifiex_write_reg(adapter, HOST_INT_RSR_REG,
				  reg | SDIO_INT_MASK);
	else
		return -1;

	
	if (!mwifiex_read_reg(adapter, CARD_MISC_CFG_REG, &reg))
		mwifiex_write_reg(adapter, CARD_MISC_CFG_REG,
				  reg | AUTO_RE_ENABLE_INT);
	else
		return -1;

	return 0;
}

static int mwifiex_write_data_to_card(struct mwifiex_adapter *adapter,
				      u8 *payload, u32 pkt_len, u32 port)
{
	u32 i = 0;
	int ret;

	do {
		ret = mwifiex_write_data_sync(adapter, payload, pkt_len, port);
		if (ret) {
			i++;
			dev_err(adapter->dev, "host_to_card, write iomem"
					" (%d) failed: %d\n", i, ret);
			if (mwifiex_write_reg(adapter, CONFIGURATION_REG, 0x04))
				dev_err(adapter->dev, "write CFG reg failed\n");

			ret = -1;
			if (i > MAX_WRITE_IOMEM_RETRY)
				return ret;
		}
	} while (ret == -1);

	return ret;
}

static int mwifiex_get_rd_port(struct mwifiex_adapter *adapter, u8 *port)
{
	struct sdio_mmc_card *card = adapter->card;
	u16 rd_bitmap = card->mp_rd_bitmap;

	dev_dbg(adapter->dev, "data: mp_rd_bitmap=0x%04x\n", rd_bitmap);

	if (!(rd_bitmap & (CTRL_PORT_MASK | DATA_PORT_MASK)))
		return -1;

	if (card->mp_rd_bitmap & CTRL_PORT_MASK) {
		card->mp_rd_bitmap &= (u16) (~CTRL_PORT_MASK);
		*port = CTRL_PORT;
		dev_dbg(adapter->dev, "data: port=%d mp_rd_bitmap=0x%04x\n",
			*port, card->mp_rd_bitmap);
	} else {
		if (card->mp_rd_bitmap & (1 << card->curr_rd_port)) {
			card->mp_rd_bitmap &= (u16)
						(~(1 << card->curr_rd_port));
			*port = card->curr_rd_port;

			if (++card->curr_rd_port == MAX_PORT)
				card->curr_rd_port = 1;
		} else {
			return -1;
		}

		dev_dbg(adapter->dev,
			"data: port=%d mp_rd_bitmap=0x%04x -> 0x%04x\n",
			*port, rd_bitmap, card->mp_rd_bitmap);
	}
	return 0;
}

static int mwifiex_get_wr_port_data(struct mwifiex_adapter *adapter, u8 *port)
{
	struct sdio_mmc_card *card = adapter->card;
	u16 wr_bitmap = card->mp_wr_bitmap;

	dev_dbg(adapter->dev, "data: mp_wr_bitmap=0x%04x\n", wr_bitmap);

	if (!(wr_bitmap & card->mp_data_port_mask))
		return -1;

	if (card->mp_wr_bitmap & (1 << card->curr_wr_port)) {
		card->mp_wr_bitmap &= (u16) (~(1 << card->curr_wr_port));
		*port = card->curr_wr_port;
		if (++card->curr_wr_port == card->mp_end_port)
			card->curr_wr_port = 1;
	} else {
		adapter->data_sent = true;
		return -EBUSY;
	}

	if (*port == CTRL_PORT) {
		dev_err(adapter->dev, "invalid data port=%d cur port=%d"
			" mp_wr_bitmap=0x%04x -> 0x%04x\n",
			*port, card->curr_wr_port, wr_bitmap,
			card->mp_wr_bitmap);
		return -1;
	}

	dev_dbg(adapter->dev, "data: port=%d mp_wr_bitmap=0x%04x -> 0x%04x\n",
		*port, wr_bitmap, card->mp_wr_bitmap);

	return 0;
}

static int
mwifiex_sdio_poll_card_status(struct mwifiex_adapter *adapter, u8 bits)
{
	u32 tries;
	u32 cs;

	for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
		if (mwifiex_read_reg(adapter, CARD_STATUS_REG, &cs))
			break;
		else if ((cs & bits) == bits)
			return 0;

		usleep_range(10, 20);
	}

	dev_err(adapter->dev, "poll card status failed, tries = %d\n", tries);

	return -1;
}

static int
mwifiex_sdio_read_fw_status(struct mwifiex_adapter *adapter, u16 *dat)
{
	u32 fws0, fws1;

	if (mwifiex_read_reg(adapter, CARD_FW_STATUS0_REG, &fws0))
		return -1;

	if (mwifiex_read_reg(adapter, CARD_FW_STATUS1_REG, &fws1))
		return -1;

	*dat = (u16) ((fws1 << 8) | fws0);

	return 0;
}

/*
 * This function disables the host interrupt.
 *
 * The host interrupt mask is read, the disable bit is reset and
 * written back to the card host interrupt mask register.
 */
static int mwifiex_sdio_disable_host_int(struct mwifiex_adapter *adapter)
{
	u32 host_int_mask;

	
	if (mwifiex_read_reg(adapter, HOST_INT_MASK_REG, &host_int_mask))
		return -1;

	
	host_int_mask &= ~HOST_INT_DISABLE;

	if (mwifiex_write_reg(adapter, HOST_INT_MASK_REG, host_int_mask)) {
		dev_err(adapter->dev, "disable host interrupt failed\n");
		return -1;
	}

	return 0;
}

/*
 * This function enables the host interrupt.
 *
 * The host interrupt enable mask is written to the card
 * host interrupt mask register.
 */
static int mwifiex_sdio_enable_host_int(struct mwifiex_adapter *adapter)
{
	
	if (mwifiex_write_reg(adapter, HOST_INT_MASK_REG, HOST_INT_ENABLE)) {
		dev_err(adapter->dev, "enable host interrupt failed\n");
		return -1;
	}
	return 0;
}

static int mwifiex_sdio_card_to_host(struct mwifiex_adapter *adapter,
				     u32 *type, u8 *buffer,
				     u32 npayload, u32 ioport)
{
	int ret;
	u32 nb;

	if (!buffer) {
		dev_err(adapter->dev, "%s: buffer is NULL\n", __func__);
		return -1;
	}

	ret = mwifiex_read_data_sync(adapter, buffer, npayload, ioport, 1);

	if (ret) {
		dev_err(adapter->dev, "%s: read iomem failed: %d\n", __func__,
			ret);
		return -1;
	}

	nb = le16_to_cpu(*(__le16 *) (buffer));
	if (nb > npayload) {
		dev_err(adapter->dev, "%s: invalid packet, nb=%d npayload=%d\n",
			__func__, nb, npayload);
		return -1;
	}

	*type = le16_to_cpu(*(__le16 *) (buffer + 2));

	return ret;
}

static int mwifiex_prog_fw_w_helper(struct mwifiex_adapter *adapter,
				    struct mwifiex_fw_image *fw)
{
	int ret;
	u8 *firmware = fw->fw_buf;
	u32 firmware_len = fw->fw_len;
	u32 offset = 0;
	u32 base0, base1;
	u8 *fwbuf;
	u16 len = 0;
	u32 txlen, tx_blocks = 0, tries;
	u32 i = 0;

	if (!firmware_len) {
		dev_err(adapter->dev,
			"firmware image not found! Terminating download\n");
		return -1;
	}

	dev_dbg(adapter->dev, "info: downloading FW image (%d bytes)\n",
		firmware_len);

	
	fwbuf = kzalloc(MWIFIEX_UPLD_SIZE, GFP_KERNEL);
	if (!fwbuf) {
		dev_err(adapter->dev,
			"unable to alloc buffer for FW. Terminating dnld\n");
		return -ENOMEM;
	}

	
	do {
		ret = mwifiex_sdio_poll_card_status(adapter, CARD_IO_READY |
						    DN_LD_CARD_RDY);
		if (ret) {
			dev_err(adapter->dev, "FW download with helper:"
				" poll status timeout @ %d\n", offset);
			goto done;
		}

		
		if (offset >= firmware_len)
			break;

		for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
			ret = mwifiex_read_reg(adapter, HOST_F1_RD_BASE_0,
					       &base0);
			if (ret) {
				dev_err(adapter->dev,
					"dev BASE0 register read failed: "
					"base0=%#04X(%d). Terminating dnld\n",
					base0, base0);
				goto done;
			}
			ret = mwifiex_read_reg(adapter, HOST_F1_RD_BASE_1,
					       &base1);
			if (ret) {
				dev_err(adapter->dev,
					"dev BASE1 register read failed: "
					"base1=%#04X(%d). Terminating dnld\n",
					base1, base1);
				goto done;
			}
			len = (u16) (((base1 & 0xff) << 8) | (base0 & 0xff));

			if (len)
				break;

			usleep_range(10, 20);
		}

		if (!len) {
			break;
		} else if (len > MWIFIEX_UPLD_SIZE) {
			dev_err(adapter->dev,
				"FW dnld failed @ %d, invalid length %d\n",
				offset, len);
			ret = -1;
			goto done;
		}

		txlen = len;

		if (len & BIT(0)) {
			i++;
			if (i > MAX_WRITE_IOMEM_RETRY) {
				dev_err(adapter->dev,
					"FW dnld failed @ %d, over max retry\n",
					offset);
				ret = -1;
				goto done;
			}
			dev_err(adapter->dev, "CRC indicated by the helper:"
				" len = 0x%04X, txlen = %d\n", len, txlen);
			len &= ~BIT(0);
			
			txlen = 0;
		} else {
			i = 0;

			if (firmware_len - offset < txlen)
				txlen = firmware_len - offset;

			tx_blocks = (txlen + MWIFIEX_SDIO_BLOCK_SIZE - 1)
				    / MWIFIEX_SDIO_BLOCK_SIZE;

			
			memmove(fwbuf, &firmware[offset], txlen);
		}

		ret = mwifiex_write_data_sync(adapter, fwbuf, tx_blocks *
					      MWIFIEX_SDIO_BLOCK_SIZE,
					      adapter->ioport);
		if (ret) {
			dev_err(adapter->dev,
				"FW download, write iomem (%d) failed @ %d\n",
				i, offset);
			if (mwifiex_write_reg(adapter, CONFIGURATION_REG, 0x04))
				dev_err(adapter->dev, "write CFG reg failed\n");

			ret = -1;
			goto done;
		}

		offset += txlen;
	} while (true);

	dev_dbg(adapter->dev, "info: FW download over, size %d bytes\n",
		offset);

	ret = 0;
done:
	kfree(fwbuf);
	return ret;
}

static int mwifiex_check_fw_status(struct mwifiex_adapter *adapter,
				   u32 poll_num)
{
	int ret = 0;
	u16 firmware_stat;
	u32 tries;
	u32 winner_status;

	
	for (tries = 0; tries < poll_num; tries++) {
		ret = mwifiex_sdio_read_fw_status(adapter, &firmware_stat);
		if (ret)
			continue;
		if (firmware_stat == FIRMWARE_READY_SDIO) {
			ret = 0;
			break;
		} else {
			mdelay(100);
			ret = -1;
		}
	}

	if (ret) {
		if (mwifiex_read_reg
		    (adapter, CARD_FW_STATUS0_REG, &winner_status))
			winner_status = 0;

		if (winner_status)
			adapter->winner = 0;
		else
			adapter->winner = 1;
	}
	return ret;
}

static void mwifiex_interrupt_status(struct mwifiex_adapter *adapter)
{
	struct sdio_mmc_card *card = adapter->card;
	u32 sdio_ireg;
	unsigned long flags;

	if (mwifiex_read_data_sync(adapter, card->mp_regs, MAX_MP_REGS,
				   REG_PORT | MWIFIEX_SDIO_BYTE_MODE_MASK,
				   0)) {
		dev_err(adapter->dev, "read mp_regs failed\n");
		return;
	}

	sdio_ireg = card->mp_regs[HOST_INTSTATUS_REG];
	if (sdio_ireg) {
		dev_dbg(adapter->dev, "int: sdio_ireg = %#x\n", sdio_ireg);
		spin_lock_irqsave(&adapter->int_lock, flags);
		adapter->int_status |= sdio_ireg;
		spin_unlock_irqrestore(&adapter->int_lock, flags);
	}
}

static void
mwifiex_sdio_interrupt(struct sdio_func *func)
{
	struct mwifiex_adapter *adapter;
	struct sdio_mmc_card *card;

	card = sdio_get_drvdata(func);
	if (!card || !card->adapter) {
		pr_debug("int: func=%p card=%p adapter=%p\n",
			 func, card, card ? card->adapter : NULL);
		return;
	}
	adapter = card->adapter;

	if (adapter->surprise_removed)
		return;

	if (!adapter->pps_uapsd_mode && adapter->ps_state == PS_STATE_SLEEP)
		adapter->ps_state = PS_STATE_AWAKE;

	mwifiex_interrupt_status(adapter);
	queue_work(adapter->workqueue, &adapter->main_work);
}

static int mwifiex_decode_rx_packet(struct mwifiex_adapter *adapter,
				    struct sk_buff *skb, u32 upld_typ)
{
	u8 *cmd_buf;

	skb_pull(skb, INTF_HEADER_LEN);

	switch (upld_typ) {
	case MWIFIEX_TYPE_DATA:
		dev_dbg(adapter->dev, "info: --- Rx: Data packet ---\n");
		mwifiex_handle_rx_packet(adapter, skb);
		break;

	case MWIFIEX_TYPE_CMD:
		dev_dbg(adapter->dev, "info: --- Rx: Cmd Response ---\n");
		
		if (!adapter->curr_cmd) {
			cmd_buf = adapter->upld_buf;

			if (adapter->ps_state == PS_STATE_SLEEP_CFM)
				mwifiex_process_sleep_confirm_resp(adapter,
								   skb->data,
								   skb->len);

			memcpy(cmd_buf, skb->data,
			       min_t(u32, MWIFIEX_SIZE_OF_CMD_BUFFER,
				     skb->len));

			dev_kfree_skb_any(skb);
		} else {
			adapter->cmd_resp_received = true;
			adapter->curr_cmd->resp_skb = skb;
		}
		break;

	case MWIFIEX_TYPE_EVENT:
		dev_dbg(adapter->dev, "info: --- Rx: Event ---\n");
		adapter->event_cause = *(u32 *) skb->data;

		skb_pull(skb, MWIFIEX_EVENT_HEADER_LEN);

		if ((skb->len > 0) && (skb->len  < MAX_EVENT_SIZE))
			memcpy(adapter->event_body, skb->data, skb->len);

		
		adapter->event_received = true;
		adapter->event_skb = skb;

		break;

	default:
		dev_err(adapter->dev, "unknown upload type %#x\n", upld_typ);
		dev_kfree_skb_any(skb);
		break;
	}

	return 0;
}

static int mwifiex_sdio_card_to_host_mp_aggr(struct mwifiex_adapter *adapter,
					     struct sk_buff *skb, u8 port)
{
	struct sdio_mmc_card *card = adapter->card;
	s32 f_do_rx_aggr = 0;
	s32 f_do_rx_cur = 0;
	s32 f_aggr_cur = 0;
	struct sk_buff *skb_deaggr;
	u32 pind;
	u32 pkt_len, pkt_type = 0;
	u8 *curr_ptr;
	u32 rx_len = skb->len;

	if (port == CTRL_PORT) {
		
		dev_dbg(adapter->dev, "info: %s: no aggregation for cmd "
			"response\n", __func__);

		f_do_rx_cur = 1;
		goto rx_curr_single;
	}

	if (!card->mpa_rx.enabled) {
		dev_dbg(adapter->dev, "info: %s: rx aggregation disabled\n",
			__func__);

		f_do_rx_cur = 1;
		goto rx_curr_single;
	}

	if (card->mp_rd_bitmap & (~((u16) CTRL_PORT_MASK))) {
		
		dev_dbg(adapter->dev, "info: %s: not last packet\n", __func__);

		if (MP_RX_AGGR_IN_PROGRESS(card)) {
			if (MP_RX_AGGR_BUF_HAS_ROOM(card, skb->len)) {
				f_aggr_cur = 1;
			} else {
				
				f_do_rx_aggr = 1;
				f_do_rx_cur = 1;
			}
		} else {
			
			f_aggr_cur = 1;
		}

	} else {
		
		dev_dbg(adapter->dev, "info: %s: last packet\n", __func__);

		if (MP_RX_AGGR_IN_PROGRESS(card)) {
			f_do_rx_aggr = 1;
			if (MP_RX_AGGR_BUF_HAS_ROOM(card, skb->len))
				f_aggr_cur = 1;
			else
				
				f_do_rx_cur = 1;
		} else {
			f_do_rx_cur = 1;
		}
	}

	if (f_aggr_cur) {
		dev_dbg(adapter->dev, "info: current packet aggregation\n");
		
		MP_RX_AGGR_SETUP(card, skb, port);

		if (MP_RX_AGGR_PKT_LIMIT_REACHED(card) ||
		    MP_RX_AGGR_PORT_LIMIT_REACHED(card)) {
			dev_dbg(adapter->dev, "info: %s: aggregated packet "
				"limit reached\n", __func__);
			
			f_do_rx_aggr = 1;
		}
	}

	if (f_do_rx_aggr) {
		
		dev_dbg(adapter->dev, "info: do_rx_aggr: num of packets: %d\n",
			card->mpa_rx.pkt_cnt);

		if (mwifiex_read_data_sync(adapter, card->mpa_rx.buf,
					   card->mpa_rx.buf_len,
					   (adapter->ioport | 0x1000 |
					    (card->mpa_rx.ports << 4)) +
					   card->mpa_rx.start_port, 1))
			goto error;

		curr_ptr = card->mpa_rx.buf;

		for (pind = 0; pind < card->mpa_rx.pkt_cnt; pind++) {

			
			pkt_len = *(u16 *) &curr_ptr[0];
			pkt_type = *(u16 *) &curr_ptr[2];

			
			skb_deaggr = card->mpa_rx.skb_arr[pind];

			if ((pkt_type == MWIFIEX_TYPE_DATA) && (pkt_len <=
					 card->mpa_rx.len_arr[pind])) {

				memcpy(skb_deaggr->data, curr_ptr, pkt_len);

				skb_trim(skb_deaggr, pkt_len);

				
				mwifiex_decode_rx_packet(adapter, skb_deaggr,
							 pkt_type);
			} else {
				dev_err(adapter->dev, "wrong aggr pkt:"
					" type=%d len=%d max_len=%d\n",
					pkt_type, pkt_len,
					card->mpa_rx.len_arr[pind]);
				dev_kfree_skb_any(skb_deaggr);
			}
			curr_ptr += card->mpa_rx.len_arr[pind];
		}
		MP_RX_AGGR_BUF_RESET(card);
	}

rx_curr_single:
	if (f_do_rx_cur) {
		dev_dbg(adapter->dev, "info: RX: port: %d, rx_len: %d\n",
			port, rx_len);

		if (mwifiex_sdio_card_to_host(adapter, &pkt_type,
					      skb->data, skb->len,
					      adapter->ioport + port))
			goto error;

		mwifiex_decode_rx_packet(adapter, skb, pkt_type);
	}

	return 0;

error:
	if (MP_RX_AGGR_IN_PROGRESS(card)) {
		
		for (pind = 0; pind < card->mpa_rx.pkt_cnt; pind++) {
			
			skb_deaggr = card->mpa_rx.skb_arr[pind];
			dev_kfree_skb_any(skb_deaggr);
		}
		MP_RX_AGGR_BUF_RESET(card);
	}

	if (f_do_rx_cur)
		
		dev_kfree_skb_any(skb);

	return -1;
}

static int mwifiex_process_int_status(struct mwifiex_adapter *adapter)
{
	struct sdio_mmc_card *card = adapter->card;
	int ret = 0;
	u8 sdio_ireg;
	struct sk_buff *skb;
	u8 port = CTRL_PORT;
	u32 len_reg_l, len_reg_u;
	u32 rx_blocks;
	u16 rx_len;
	unsigned long flags;

	spin_lock_irqsave(&adapter->int_lock, flags);
	sdio_ireg = adapter->int_status;
	adapter->int_status = 0;
	spin_unlock_irqrestore(&adapter->int_lock, flags);

	if (!sdio_ireg)
		return ret;

	if (sdio_ireg & DN_LD_HOST_INT_STATUS) {
		card->mp_wr_bitmap = ((u16) card->mp_regs[WR_BITMAP_U]) << 8;
		card->mp_wr_bitmap |= (u16) card->mp_regs[WR_BITMAP_L];
		dev_dbg(adapter->dev, "int: DNLD: wr_bitmap=0x%04x\n",
			card->mp_wr_bitmap);
		if (adapter->data_sent &&
		    (card->mp_wr_bitmap & card->mp_data_port_mask)) {
			dev_dbg(adapter->dev,
				"info:  <--- Tx DONE Interrupt --->\n");
			adapter->data_sent = false;
		}
	}

	if (adapter->cmd_sent) {
		card->mp_wr_bitmap |=
			(u16) card->mp_regs[WR_BITMAP_L] & CTRL_PORT_MASK;
		if (card->mp_wr_bitmap & CTRL_PORT_MASK)
			adapter->cmd_sent = false;
	}

	dev_dbg(adapter->dev, "info: cmd_sent=%d data_sent=%d\n",
		adapter->cmd_sent, adapter->data_sent);
	if (sdio_ireg & UP_LD_HOST_INT_STATUS) {
		card->mp_rd_bitmap = ((u16) card->mp_regs[RD_BITMAP_U]) << 8;
		card->mp_rd_bitmap |= (u16) card->mp_regs[RD_BITMAP_L];
		dev_dbg(adapter->dev, "int: UPLD: rd_bitmap=0x%04x\n",
			card->mp_rd_bitmap);

		while (true) {
			ret = mwifiex_get_rd_port(adapter, &port);
			if (ret) {
				dev_dbg(adapter->dev,
					"info: no more rd_port available\n");
				break;
			}
			len_reg_l = RD_LEN_P0_L + (port << 1);
			len_reg_u = RD_LEN_P0_U + (port << 1);
			rx_len = ((u16) card->mp_regs[len_reg_u]) << 8;
			rx_len |= (u16) card->mp_regs[len_reg_l];
			dev_dbg(adapter->dev, "info: RX: port=%d rx_len=%u\n",
				port, rx_len);
			rx_blocks =
				(rx_len + MWIFIEX_SDIO_BLOCK_SIZE -
				 1) / MWIFIEX_SDIO_BLOCK_SIZE;
			if (rx_len <= INTF_HEADER_LEN ||
			    (rx_blocks * MWIFIEX_SDIO_BLOCK_SIZE) >
			     MWIFIEX_RX_DATA_BUF_SIZE) {
				dev_err(adapter->dev, "invalid rx_len=%d\n",
					rx_len);
				return -1;
			}
			rx_len = (u16) (rx_blocks * MWIFIEX_SDIO_BLOCK_SIZE);

			skb = dev_alloc_skb(rx_len);

			if (!skb) {
				dev_err(adapter->dev, "%s: failed to alloc skb",
					__func__);
				return -1;
			}

			skb_put(skb, rx_len);

			dev_dbg(adapter->dev, "info: rx_len = %d skb->len = %d\n",
				rx_len, skb->len);

			if (mwifiex_sdio_card_to_host_mp_aggr(adapter, skb,
							      port)) {
				u32 cr = 0;

				dev_err(adapter->dev, "card_to_host_mpa failed:"
					" int status=%#x\n", sdio_ireg);
				if (mwifiex_read_reg(adapter,
						     CONFIGURATION_REG, &cr))
					dev_err(adapter->dev,
						"read CFG reg failed\n");

				dev_dbg(adapter->dev,
					"info: CFG reg val = %d\n", cr);
				if (mwifiex_write_reg(adapter,
						      CONFIGURATION_REG,
						      (cr | 0x04)))
					dev_err(adapter->dev,
						"write CFG reg failed\n");

				dev_dbg(adapter->dev, "info: write success\n");
				if (mwifiex_read_reg(adapter,
						     CONFIGURATION_REG, &cr))
					dev_err(adapter->dev,
						"read CFG reg failed\n");

				dev_dbg(adapter->dev,
					"info: CFG reg val =%x\n", cr);
				return -1;
			}
		}
	}

	return 0;
}

static int mwifiex_host_to_card_mp_aggr(struct mwifiex_adapter *adapter,
					u8 *payload, u32 pkt_len, u8 port,
					u32 next_pkt_len)
{
	struct sdio_mmc_card *card = adapter->card;
	int ret = 0;
	s32 f_send_aggr_buf = 0;
	s32 f_send_cur_buf = 0;
	s32 f_precopy_cur_buf = 0;
	s32 f_postcopy_cur_buf = 0;

	if ((!card->mpa_tx.enabled) || (port == CTRL_PORT)) {
		dev_dbg(adapter->dev, "info: %s: tx aggregation disabled\n",
			__func__);

		f_send_cur_buf = 1;
		goto tx_curr_single;
	}

	if (next_pkt_len) {
		
		dev_dbg(adapter->dev, "info: %s: more packets in queue.\n",
			__func__);

		if (MP_TX_AGGR_IN_PROGRESS(card)) {
			if (!MP_TX_AGGR_PORT_LIMIT_REACHED(card) &&
			    MP_TX_AGGR_BUF_HAS_ROOM(card, pkt_len)) {
				f_precopy_cur_buf = 1;

				if (!(card->mp_wr_bitmap &
				      (1 << card->curr_wr_port)) ||
				    !MP_TX_AGGR_BUF_HAS_ROOM(
					    card, pkt_len + next_pkt_len))
					f_send_aggr_buf = 1;
			} else {
				
				f_send_aggr_buf = 1;

				if (MP_TX_AGGR_PORT_LIMIT_REACHED(card) ||
				    !(card->mp_wr_bitmap &
				      (1 << card->curr_wr_port)))
					f_send_cur_buf = 1;
				else
					f_postcopy_cur_buf = 1;
			}
		} else {
			if (MP_TX_AGGR_BUF_HAS_ROOM(card, pkt_len) &&
			    (card->mp_wr_bitmap & (1 << card->curr_wr_port)))
				f_precopy_cur_buf = 1;
			else
				f_send_cur_buf = 1;
		}
	} else {
		
		dev_dbg(adapter->dev, "info: %s: Last packet in Tx Queue.\n",
			__func__);

		if (MP_TX_AGGR_IN_PROGRESS(card)) {
			
			f_send_aggr_buf = 1;

			if (MP_TX_AGGR_BUF_HAS_ROOM(card, pkt_len))
				f_precopy_cur_buf = 1;
			else
				
				f_send_cur_buf = 1;
		} else {
			f_send_cur_buf = 1;
		}
	}

	if (f_precopy_cur_buf) {
		dev_dbg(adapter->dev, "data: %s: precopy current buffer\n",
			__func__);
		MP_TX_AGGR_BUF_PUT(card, payload, pkt_len, port);

		if (MP_TX_AGGR_PKT_LIMIT_REACHED(card) ||
		    MP_TX_AGGR_PORT_LIMIT_REACHED(card))
			
			f_send_aggr_buf = 1;
	}

	if (f_send_aggr_buf) {
		dev_dbg(adapter->dev, "data: %s: send aggr buffer: %d %d\n",
			__func__,
				card->mpa_tx.start_port, card->mpa_tx.ports);
		ret = mwifiex_write_data_to_card(adapter, card->mpa_tx.buf,
						 card->mpa_tx.buf_len,
						 (adapter->ioport | 0x1000 |
						 (card->mpa_tx.ports << 4)) +
						  card->mpa_tx.start_port);

		MP_TX_AGGR_BUF_RESET(card);
	}

tx_curr_single:
	if (f_send_cur_buf) {
		dev_dbg(adapter->dev, "data: %s: send current buffer %d\n",
			__func__, port);
		ret = mwifiex_write_data_to_card(adapter, payload, pkt_len,
						 adapter->ioport + port);
	}

	if (f_postcopy_cur_buf) {
		dev_dbg(adapter->dev, "data: %s: postcopy current buffer\n",
			__func__);
		MP_TX_AGGR_BUF_PUT(card, payload, pkt_len, port);
	}

	return ret;
}

static int mwifiex_sdio_host_to_card(struct mwifiex_adapter *adapter,
				     u8 type, struct sk_buff *skb,
				     struct mwifiex_tx_param *tx_param)
{
	struct sdio_mmc_card *card = adapter->card;
	int ret;
	u32 buf_block_len;
	u32 blk_size;
	u8 port = CTRL_PORT;
	u8 *payload = (u8 *)skb->data;
	u32 pkt_len = skb->len;

	
	blk_size = MWIFIEX_SDIO_BLOCK_SIZE;
	buf_block_len = (pkt_len + blk_size - 1) / blk_size;
	*(u16 *) &payload[0] = (u16) pkt_len;
	*(u16 *) &payload[2] = type;

	if (type == MWIFIEX_TYPE_DATA) {
		ret = mwifiex_get_wr_port_data(adapter, &port);
		if (ret) {
			dev_err(adapter->dev, "%s: no wr_port available\n",
				__func__);
			return ret;
		}
	} else {
		adapter->cmd_sent = true;
		

		if (pkt_len <= INTF_HEADER_LEN ||
		    pkt_len > MWIFIEX_UPLD_SIZE)
			dev_err(adapter->dev, "%s: payload=%p, nb=%d\n",
				__func__, payload, pkt_len);
	}

	
	pkt_len = buf_block_len * blk_size;

	if (tx_param)
		ret = mwifiex_host_to_card_mp_aggr(adapter, payload, pkt_len,
						   port, tx_param->next_pkt_len
						   );
	else
		ret = mwifiex_host_to_card_mp_aggr(adapter, payload, pkt_len,
						   port, 0);

	if (ret) {
		if (type == MWIFIEX_TYPE_CMD)
			adapter->cmd_sent = false;
		if (type == MWIFIEX_TYPE_DATA)
			adapter->data_sent = false;
	} else {
		if (type == MWIFIEX_TYPE_DATA) {
			if (!(card->mp_wr_bitmap & (1 << card->curr_wr_port)))
				adapter->data_sent = true;
			else
				adapter->data_sent = false;
		}
	}

	return ret;
}

static int mwifiex_alloc_sdio_mpa_buffers(struct mwifiex_adapter *adapter,
				   u32 mpa_tx_buf_size, u32 mpa_rx_buf_size)
{
	struct sdio_mmc_card *card = adapter->card;
	int ret = 0;

	card->mpa_tx.buf = kzalloc(mpa_tx_buf_size, GFP_KERNEL);
	if (!card->mpa_tx.buf) {
		dev_err(adapter->dev, "could not alloc buffer for MP-A TX\n");
		ret = -1;
		goto error;
	}

	card->mpa_tx.buf_size = mpa_tx_buf_size;

	card->mpa_rx.buf = kzalloc(mpa_rx_buf_size, GFP_KERNEL);
	if (!card->mpa_rx.buf) {
		dev_err(adapter->dev, "could not alloc buffer for MP-A RX\n");
		ret = -1;
		goto error;
	}

	card->mpa_rx.buf_size = mpa_rx_buf_size;

error:
	if (ret) {
		kfree(card->mpa_tx.buf);
		kfree(card->mpa_rx.buf);
	}

	return ret;
}

static void
mwifiex_unregister_dev(struct mwifiex_adapter *adapter)
{
	struct sdio_mmc_card *card = adapter->card;

	if (adapter->card) {
		
		sdio_claim_host(card->func);
		sdio_release_irq(card->func);
		sdio_disable_func(card->func);
		sdio_release_host(card->func);
		sdio_set_drvdata(card->func, NULL);
	}
}

static int mwifiex_register_dev(struct mwifiex_adapter *adapter)
{
	int ret = 0;
	struct sdio_mmc_card *card = adapter->card;
	struct sdio_func *func = card->func;

	
	card->adapter = adapter;

	sdio_claim_host(func);

	
	ret = sdio_claim_irq(func, mwifiex_sdio_interrupt);
	if (ret) {
		pr_err("claim irq failed: ret=%d\n", ret);
		goto disable_func;
	}

	
	ret = sdio_set_block_size(card->func, MWIFIEX_SDIO_BLOCK_SIZE);
	if (ret) {
		pr_err("cannot set SDIO block size\n");
		ret = -1;
		goto release_irq;
	}

	sdio_release_host(func);
	sdio_set_drvdata(func, card);

	adapter->dev = &func->dev;

	switch (func->device) {
	case SDIO_DEVICE_ID_MARVELL_8797:
		strcpy(adapter->fw_name, SD8797_DEFAULT_FW_NAME);
		break;
	case SDIO_DEVICE_ID_MARVELL_8787:
	default:
		strcpy(adapter->fw_name, SD8787_DEFAULT_FW_NAME);
		break;
	}

	return 0;

release_irq:
	sdio_release_irq(func);
disable_func:
	sdio_disable_func(func);
	sdio_release_host(func);
	adapter->card = NULL;

	return -1;
}

static int mwifiex_init_sdio(struct mwifiex_adapter *adapter)
{
	struct sdio_mmc_card *card = adapter->card;
	int ret;
	u32 sdio_ireg;

	mwifiex_read_reg(adapter, HOST_INTSTATUS_REG, &sdio_ireg);

	
	mwifiex_sdio_disable_host_int(adapter);

	
	mwifiex_init_sdio_ioport(adapter);

	
	card->mp_rd_bitmap = 0;
	card->mp_wr_bitmap = 0;
	card->curr_rd_port = 1;
	card->curr_wr_port = 1;

	card->mp_data_port_mask = DATA_PORT_MASK;

	card->mpa_tx.buf_len = 0;
	card->mpa_tx.pkt_cnt = 0;
	card->mpa_tx.start_port = 0;

	card->mpa_tx.enabled = 1;
	card->mpa_tx.pkt_aggr_limit = SDIO_MP_AGGR_DEF_PKT_LIMIT;

	card->mpa_rx.buf_len = 0;
	card->mpa_rx.pkt_cnt = 0;
	card->mpa_rx.start_port = 0;

	card->mpa_rx.enabled = 1;
	card->mpa_rx.pkt_aggr_limit = SDIO_MP_AGGR_DEF_PKT_LIMIT;

	
	card->mp_regs = kzalloc(MAX_MP_REGS, GFP_KERNEL);
	if (!card->mp_regs) {
		dev_err(adapter->dev, "failed to alloc mp_regs\n");
		return -ENOMEM;
	}

	ret = mwifiex_alloc_sdio_mpa_buffers(adapter,
					     SDIO_MP_TX_AGGR_DEF_BUF_SIZE,
					     SDIO_MP_RX_AGGR_DEF_BUF_SIZE);
	if (ret) {
		dev_err(adapter->dev, "failed to alloc sdio mp-a buffers\n");
		kfree(card->mp_regs);
		return -1;
	}

	return ret;
}

static void mwifiex_cleanup_mpa_buf(struct mwifiex_adapter *adapter)
{
	struct sdio_mmc_card *card = adapter->card;

	MP_TX_AGGR_BUF_RESET(card);
	MP_RX_AGGR_BUF_RESET(card);
}

static void mwifiex_cleanup_sdio(struct mwifiex_adapter *adapter)
{
	struct sdio_mmc_card *card = adapter->card;

	kfree(card->mp_regs);
	kfree(card->mpa_tx.buf);
	kfree(card->mpa_rx.buf);
}

static void
mwifiex_update_mp_end_port(struct mwifiex_adapter *adapter, u16 port)
{
	struct sdio_mmc_card *card = adapter->card;
	int i;

	card->mp_end_port = port;

	card->mp_data_port_mask = DATA_PORT_MASK;

	for (i = 1; i <= MAX_PORT - card->mp_end_port; i++)
		card->mp_data_port_mask &= ~(1 << (MAX_PORT - i));

	card->curr_wr_port = 1;

	dev_dbg(adapter->dev, "cmd: mp_end_port %d, data port mask 0x%x\n",
		port, card->mp_data_port_mask);
}

static struct mwifiex_if_ops sdio_ops = {
	.init_if = mwifiex_init_sdio,
	.cleanup_if = mwifiex_cleanup_sdio,
	.check_fw_status = mwifiex_check_fw_status,
	.prog_fw = mwifiex_prog_fw_w_helper,
	.register_dev = mwifiex_register_dev,
	.unregister_dev = mwifiex_unregister_dev,
	.enable_int = mwifiex_sdio_enable_host_int,
	.process_int_status = mwifiex_process_int_status,
	.host_to_card = mwifiex_sdio_host_to_card,
	.wakeup = mwifiex_pm_wakeup_card,
	.wakeup_complete = mwifiex_pm_wakeup_card_complete,

	
	.update_mp_end_port = mwifiex_update_mp_end_port,
	.cleanup_mpa_buf = mwifiex_cleanup_mpa_buf,
	.cmdrsp_complete = mwifiex_sdio_cmdrsp_complete,
	.event_complete = mwifiex_sdio_event_complete,
};

static int
mwifiex_sdio_init_module(void)
{
	sema_init(&add_remove_card_sem, 1);

	
	user_rmmod = 0;

	return sdio_register_driver(&mwifiex_sdio);
}

static void
mwifiex_sdio_cleanup_module(void)
{
	if (!down_interruptible(&add_remove_card_sem))
		up(&add_remove_card_sem);

	
	user_rmmod = 1;

	sdio_unregister_driver(&mwifiex_sdio);
}

module_init(mwifiex_sdio_init_module);
module_exit(mwifiex_sdio_cleanup_module);

MODULE_AUTHOR("Marvell International Ltd.");
MODULE_DESCRIPTION("Marvell WiFi-Ex SDIO Driver version " SDIO_VERSION);
MODULE_VERSION(SDIO_VERSION);
MODULE_LICENSE("GPL v2");
MODULE_FIRMWARE(SD8787_DEFAULT_FW_NAME);
MODULE_FIRMWARE(SD8797_DEFAULT_FW_NAME);
