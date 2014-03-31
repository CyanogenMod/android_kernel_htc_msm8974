/*

  Driver for the Marvell 8385 based compact flash WLAN cards.

  (C) 2007 by Holger Schurig <hs4233@mail.mn-solutions.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/firmware.h>
#include <linux/netdevice.h>

#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>

#include <linux/io.h>

#define DRV_NAME "libertas_cs"

#include "decl.h"
#include "defs.h"
#include "dev.h"



MODULE_AUTHOR("Holger Schurig <hs4233@mail.mn-solutions.de>");
MODULE_DESCRIPTION("Driver for Marvell 83xx compact flash WLAN cards");
MODULE_LICENSE("GPL");




struct if_cs_card {
	struct pcmcia_device *p_dev;
	struct lbs_private *priv;
	void __iomem *iobase;
	bool align_regs;
	u32 model;
};


enum {
	MODEL_UNKNOWN = 0x00,
	MODEL_8305 = 0x01,
	MODEL_8381 = 0x02,
	MODEL_8385 = 0x03
};

static const struct lbs_fw_table fw_table[] = {
	{ MODEL_8305, "libertas/cf8305.bin", NULL },
	{ MODEL_8305, "libertas_cs_helper.fw", NULL },
	{ MODEL_8381, "libertas/cf8381_helper.bin", "libertas/cf8381.bin" },
	{ MODEL_8381, "libertas_cs_helper.fw", "libertas_cs.fw" },
	{ MODEL_8385, "libertas/cf8385_helper.bin", "libertas/cf8385.bin" },
	{ MODEL_8385, "libertas_cs_helper.fw", "libertas_cs.fw" },
	{ 0, NULL, NULL }
};
MODULE_FIRMWARE("libertas/cf8305.bin");
MODULE_FIRMWARE("libertas/cf8381_helper.bin");
MODULE_FIRMWARE("libertas/cf8381.bin");
MODULE_FIRMWARE("libertas/cf8385_helper.bin");
MODULE_FIRMWARE("libertas/cf8385.bin");
MODULE_FIRMWARE("libertas_cs_helper.fw");
MODULE_FIRMWARE("libertas_cs.fw");




#ifdef DEBUG_IO
static int debug_output = 0;
#else
#define debug_output 0
#endif

static inline unsigned int if_cs_read8(struct if_cs_card *card, uint reg)
{
	unsigned int val = ioread8(card->iobase + reg);
	if (debug_output)
		printk(KERN_INFO "inb %08x<%02x\n", reg, val);
	return val;
}
static inline unsigned int if_cs_read16(struct if_cs_card *card, uint reg)
{
	unsigned int val = ioread16(card->iobase + reg);
	if (debug_output)
		printk(KERN_INFO "inw %08x<%04x\n", reg, val);
	return val;
}
static inline void if_cs_read16_rep(
	struct if_cs_card *card,
	uint reg,
	void *buf,
	unsigned long count)
{
	if (debug_output)
		printk(KERN_INFO "insw %08x<(0x%lx words)\n",
			reg, count);
	ioread16_rep(card->iobase + reg, buf, count);
}

static inline void if_cs_write8(struct if_cs_card *card, uint reg, u8 val)
{
	if (debug_output)
		printk(KERN_INFO "outb %08x>%02x\n", reg, val);
	iowrite8(val, card->iobase + reg);
}

static inline void if_cs_write16(struct if_cs_card *card, uint reg, u16 val)
{
	if (debug_output)
		printk(KERN_INFO "outw %08x>%04x\n", reg, val);
	iowrite16(val, card->iobase + reg);
}

static inline void if_cs_write16_rep(
	struct if_cs_card *card,
	uint reg,
	const void *buf,
	unsigned long count)
{
	if (debug_output)
		printk(KERN_INFO "outsw %08x>(0x%lx words)\n",
			reg, count);
	iowrite16_rep(card->iobase + reg, buf, count);
}


static int if_cs_poll_while_fw_download(struct if_cs_card *card, uint addr, u8 reg)
{
	int i;

	for (i = 0; i < 100000; i++) {
		u8 val = if_cs_read8(card, addr);
		if (val == reg)
			return 0;
		udelay(5);
	}
	return -ETIME;
}



#define IF_CS_BIT_TX			0x0001
#define IF_CS_BIT_RX			0x0002
#define IF_CS_BIT_COMMAND		0x0004
#define IF_CS_BIT_RESP			0x0008
#define IF_CS_BIT_EVENT			0x0010
#define	IF_CS_BIT_MASK			0x001f



#define IF_CS_HOST_STATUS		0x00000000

#define IF_CS_HOST_INT_CAUSE		0x00000002

#define IF_CS_HOST_INT_MASK		0x00000004

#define IF_CS_WRITE			0x00000016
#define IF_CS_WRITE_LEN			0x00000014
#define IF_CS_READ			0x00000010
#define IF_CS_READ_LEN			0x00000024

#define IF_CS_CMD			0x0000001A
#define IF_CS_CMD_LEN			0x00000018
#define IF_CS_RESP			0x00000012
#define IF_CS_RESP_LEN			0x00000030

#define IF_CS_CARD_STATUS		0x00000020
#define IF_CS_CARD_STATUS_MASK		0x7f00

#define IF_CS_CARD_INT_CAUSE		0x00000022

#define IF_CS_SQ_READ_LOW		0x00000028
#define IF_CS_SQ_HELPER_OK		0x10

#define IF_CS_SCRATCH			0x0000003F
#define IF_CS_SCRATCH_BOOT_OK		0x00
#define IF_CS_SCRATCH_HELPER_OK		0x5a

#define IF_CS_PRODUCT_ID		0x0000001C
#define IF_CS_CF8385_B1_REV		0x12
#define IF_CS_CF8381_B3_REV		0x04
#define IF_CS_CF8305_B1_REV		0x03

#define CF8305_MANFID		0x02db
#define CF8305_CARDID		0x8103
#define CF8381_MANFID		0x02db
#define CF8381_CARDID		0x6064
#define CF8385_MANFID		0x02df
#define CF8385_CARDID		0x8103

static inline u32 get_model(u16 manf_id, u16 card_id)
{
	
	if (manf_id == CF8305_MANFID && card_id == CF8305_CARDID)
		return MODEL_8305;
	else if (manf_id == CF8381_MANFID && card_id == CF8381_CARDID)
		return MODEL_8381;
	else if (manf_id == CF8385_MANFID && card_id == CF8385_CARDID)
		return MODEL_8385;
	return MODEL_UNKNOWN;
}


static inline void if_cs_enable_ints(struct if_cs_card *card)
{
	lbs_deb_enter(LBS_DEB_CS);
	if_cs_write16(card, IF_CS_HOST_INT_MASK, 0);
}

static inline void if_cs_disable_ints(struct if_cs_card *card)
{
	lbs_deb_enter(LBS_DEB_CS);
	if_cs_write16(card, IF_CS_HOST_INT_MASK, IF_CS_BIT_MASK);
}

static int if_cs_send_cmd(struct lbs_private *priv, u8 *buf, u16 nb)
{
	struct if_cs_card *card = (struct if_cs_card *)priv->card;
	int ret = -1;
	int loops = 0;

	lbs_deb_enter(LBS_DEB_CS);
	if_cs_disable_ints(card);

	
	while (1) {
		u16 status = if_cs_read16(card, IF_CS_CARD_STATUS);
		if (status & IF_CS_BIT_COMMAND)
			break;
		if (++loops > 100) {
			netdev_err(priv->dev, "card not ready for commands\n");
			goto done;
		}
		mdelay(1);
	}

	if_cs_write16(card, IF_CS_CMD_LEN, nb);

	if_cs_write16_rep(card, IF_CS_CMD, buf, nb / 2);
	
	if (nb & 1)
		if_cs_write8(card, IF_CS_CMD, buf[nb-1]);

	if_cs_write16(card, IF_CS_HOST_STATUS, IF_CS_BIT_COMMAND);

	if_cs_write16(card, IF_CS_HOST_INT_CAUSE, IF_CS_BIT_COMMAND);
	ret = 0;

done:
	if_cs_enable_ints(card);
	lbs_deb_leave_args(LBS_DEB_CS, "ret %d", ret);
	return ret;
}

static void if_cs_send_data(struct lbs_private *priv, u8 *buf, u16 nb)
{
	struct if_cs_card *card = (struct if_cs_card *)priv->card;
	u16 status;

	lbs_deb_enter(LBS_DEB_CS);
	if_cs_disable_ints(card);

	status = if_cs_read16(card, IF_CS_CARD_STATUS);
	BUG_ON((status & IF_CS_BIT_TX) == 0);

	if_cs_write16(card, IF_CS_WRITE_LEN, nb);

	
	if_cs_write16_rep(card, IF_CS_WRITE, buf, nb / 2);
	if (nb & 1)
		if_cs_write8(card, IF_CS_WRITE, buf[nb-1]);

	if_cs_write16(card, IF_CS_HOST_STATUS, IF_CS_BIT_TX);
	if_cs_write16(card, IF_CS_HOST_INT_CAUSE, IF_CS_BIT_TX);
	if_cs_enable_ints(card);

	lbs_deb_leave(LBS_DEB_CS);
}

static int if_cs_receive_cmdres(struct lbs_private *priv, u8 *data, u32 *len)
{
	unsigned long flags;
	int ret = -1;
	u16 status;

	lbs_deb_enter(LBS_DEB_CS);

	
	status = if_cs_read16(priv->card, IF_CS_CARD_STATUS);
	if ((status & IF_CS_BIT_RESP) == 0) {
		netdev_err(priv->dev, "no cmd response in card\n");
		*len = 0;
		goto out;
	}

	*len = if_cs_read16(priv->card, IF_CS_RESP_LEN);
	if ((*len == 0) || (*len > LBS_CMD_BUFFER_SIZE)) {
		netdev_err(priv->dev,
			   "card cmd buffer has invalid # of bytes (%d)\n",
			   *len);
		goto out;
	}

	
	if_cs_read16_rep(priv->card, IF_CS_RESP, data, *len/sizeof(u16));
	if (*len & 1)
		data[*len-1] = if_cs_read8(priv->card, IF_CS_RESP);

	*len -= 8;
	ret = 0;

	
	spin_lock_irqsave(&priv->driver_lock, flags);
	priv->dnld_sent = DNLD_RES_RECEIVED;
	spin_unlock_irqrestore(&priv->driver_lock, flags);

out:
	lbs_deb_leave_args(LBS_DEB_CS, "ret %d, len %d", ret, *len);
	return ret;
}

static struct sk_buff *if_cs_receive_data(struct lbs_private *priv)
{
	struct sk_buff *skb = NULL;
	u16 len;
	u8 *data;

	lbs_deb_enter(LBS_DEB_CS);

	len = if_cs_read16(priv->card, IF_CS_READ_LEN);
	if (len == 0 || len > MRVDRV_ETH_RX_PACKET_BUFFER_SIZE) {
		netdev_err(priv->dev,
			   "card data buffer has invalid # of bytes (%d)\n",
			   len);
		priv->dev->stats.rx_dropped++;
		goto dat_err;
	}

	skb = dev_alloc_skb(MRVDRV_ETH_RX_PACKET_BUFFER_SIZE + 2);
	if (!skb)
		goto out;
	skb_put(skb, len);
	skb_reserve(skb, 2);
	data = skb->data;

	
	if_cs_read16_rep(priv->card, IF_CS_READ, data, len/sizeof(u16));
	if (len & 1)
		data[len-1] = if_cs_read8(priv->card, IF_CS_READ);

dat_err:
	if_cs_write16(priv->card, IF_CS_HOST_STATUS, IF_CS_BIT_RX);
	if_cs_write16(priv->card, IF_CS_HOST_INT_CAUSE, IF_CS_BIT_RX);

out:
	lbs_deb_leave_args(LBS_DEB_CS, "ret %p", skb);
	return skb;
}

static irqreturn_t if_cs_interrupt(int irq, void *data)
{
	struct if_cs_card *card = data;
	struct lbs_private *priv = card->priv;
	u16 cause;

	lbs_deb_enter(LBS_DEB_CS);

	
	cause = if_cs_read16(card, IF_CS_CARD_INT_CAUSE);
	lbs_deb_cs("cause 0x%04x\n", cause);

	if (cause == 0) {
		
		return IRQ_NONE;
	}

	if (cause == 0xffff) {
		
		card->priv->surpriseremoved = 1;
		return IRQ_HANDLED;
	}

	if (cause & IF_CS_BIT_RX) {
		struct sk_buff *skb;
		lbs_deb_cs("rx packet\n");
		skb = if_cs_receive_data(priv);
		if (skb)
			lbs_process_rxed_packet(priv, skb);
	}

	if (cause & IF_CS_BIT_TX) {
		lbs_deb_cs("tx done\n");
		lbs_host_to_card_done(priv);
	}

	if (cause & IF_CS_BIT_RESP) {
		unsigned long flags;
		u8 i;

		lbs_deb_cs("cmd resp\n");
		spin_lock_irqsave(&priv->driver_lock, flags);
		i = (priv->resp_idx == 0) ? 1 : 0;
		spin_unlock_irqrestore(&priv->driver_lock, flags);

		BUG_ON(priv->resp_len[i]);
		if_cs_receive_cmdres(priv, priv->resp_buf[i],
			&priv->resp_len[i]);

		spin_lock_irqsave(&priv->driver_lock, flags);
		lbs_notify_command_response(priv, i);
		spin_unlock_irqrestore(&priv->driver_lock, flags);
	}

	if (cause & IF_CS_BIT_EVENT) {
		u16 status = if_cs_read16(priv->card, IF_CS_CARD_STATUS);
		if_cs_write16(priv->card, IF_CS_HOST_INT_CAUSE,
			IF_CS_BIT_EVENT);
		lbs_queue_event(priv, (status & IF_CS_CARD_STATUS_MASK) >> 8);
	}

	
	if_cs_write16(card, IF_CS_CARD_INT_CAUSE, cause & IF_CS_BIT_MASK);

	lbs_deb_leave(LBS_DEB_CS);
	return IRQ_HANDLED;
}





static int if_cs_prog_helper(struct if_cs_card *card, const struct firmware *fw)
{
	int ret = 0;
	int sent = 0;
	u8  scratch;

	lbs_deb_enter(LBS_DEB_CS);

	if (card->align_regs)
		scratch = if_cs_read16(card, IF_CS_SCRATCH) >> 8;
	else
		scratch = if_cs_read8(card, IF_CS_SCRATCH);

	if (scratch == IF_CS_SCRATCH_HELPER_OK)
		goto done;

	
	if (scratch != IF_CS_SCRATCH_BOOT_OK) {
		ret = -ENODEV;
		goto done;
	}

	lbs_deb_cs("helper size %td\n", fw->size);

	
	

	for (;;) {
		
		int count = 256;
		int remain = fw->size - sent;

		if (remain < count)
			count = remain;

		if_cs_write16(card, IF_CS_CMD_LEN, count);

		
		if (count)
			if_cs_write16_rep(card, IF_CS_CMD,
				&fw->data[sent],
				count >> 1);

		if_cs_write8(card, IF_CS_HOST_STATUS, IF_CS_BIT_COMMAND);

		if_cs_write16(card, IF_CS_HOST_INT_CAUSE, IF_CS_BIT_COMMAND);

		ret = if_cs_poll_while_fw_download(card, IF_CS_CARD_STATUS,
			IF_CS_BIT_COMMAND);
		if (ret < 0) {
			pr_err("can't download helper at 0x%x, ret %d\n",
			       sent, ret);
			goto done;
		}

		if (count == 0)
			break;

		sent += count;
	}

done:
	lbs_deb_leave_args(LBS_DEB_CS, "ret %d", ret);
	return ret;
}


static int if_cs_prog_real(struct if_cs_card *card, const struct firmware *fw)
{
	int ret = 0;
	int retry = 0;
	int len = 0;
	int sent;

	lbs_deb_enter(LBS_DEB_CS);

	lbs_deb_cs("fw size %td\n", fw->size);

	ret = if_cs_poll_while_fw_download(card, IF_CS_SQ_READ_LOW,
		IF_CS_SQ_HELPER_OK);
	if (ret < 0) {
		pr_err("helper firmware doesn't answer\n");
		goto done;
	}

	for (sent = 0; sent < fw->size; sent += len) {
		len = if_cs_read16(card, IF_CS_SQ_READ_LOW);
		if (len & 1) {
			retry++;
			pr_info("odd, need to retry this firmware block\n");
		} else {
			retry = 0;
		}

		if (retry > 20) {
			pr_err("could not download firmware\n");
			ret = -ENODEV;
			goto done;
		}
		if (retry) {
			sent -= len;
		}


		if_cs_write16(card, IF_CS_CMD_LEN, len);

		if_cs_write16_rep(card, IF_CS_CMD,
			&fw->data[sent],
			(len+1) >> 1);
		if_cs_write8(card, IF_CS_HOST_STATUS, IF_CS_BIT_COMMAND);
		if_cs_write16(card, IF_CS_HOST_INT_CAUSE, IF_CS_BIT_COMMAND);

		ret = if_cs_poll_while_fw_download(card, IF_CS_CARD_STATUS,
			IF_CS_BIT_COMMAND);
		if (ret < 0) {
			pr_err("can't download firmware at 0x%x\n", sent);
			goto done;
		}
	}

	ret = if_cs_poll_while_fw_download(card, IF_CS_SCRATCH, 0x5a);
	if (ret < 0)
		pr_err("firmware download failed\n");

done:
	lbs_deb_leave_args(LBS_DEB_CS, "ret %d", ret);
	return ret;
}




static int if_cs_host_to_card(struct lbs_private *priv,
	u8 type,
	u8 *buf,
	u16 nb)
{
	int ret = -1;

	lbs_deb_enter_args(LBS_DEB_CS, "type %d, bytes %d", type, nb);

	switch (type) {
	case MVMS_DAT:
		priv->dnld_sent = DNLD_DATA_SENT;
		if_cs_send_data(priv, buf, nb);
		ret = 0;
		break;
	case MVMS_CMD:
		priv->dnld_sent = DNLD_CMD_SENT;
		ret = if_cs_send_cmd(priv, buf, nb);
		break;
	default:
		netdev_err(priv->dev, "%s: unsupported type %d\n",
			   __func__, type);
	}

	lbs_deb_leave_args(LBS_DEB_CS, "ret %d", ret);
	return ret;
}


static void if_cs_release(struct pcmcia_device *p_dev)
{
	struct if_cs_card *card = p_dev->priv;

	lbs_deb_enter(LBS_DEB_CS);

	free_irq(p_dev->irq, card);
	pcmcia_disable_device(p_dev);
	if (card->iobase)
		ioport_unmap(card->iobase);

	lbs_deb_leave(LBS_DEB_CS);
}


static int if_cs_ioprobe(struct pcmcia_device *p_dev, void *priv_data)
{
	p_dev->resource[0]->flags &= ~IO_DATA_PATH_WIDTH;
	p_dev->resource[0]->flags |= IO_DATA_PATH_WIDTH_AUTO;

	if (p_dev->resource[1]->end) {
		pr_err("wrong CIS (check number of IO windows)\n");
		return -ENODEV;
	}

	
	return pcmcia_request_io(p_dev);
}

static int if_cs_probe(struct pcmcia_device *p_dev)
{
	int ret = -ENOMEM;
	unsigned int prod_id;
	struct lbs_private *priv;
	struct if_cs_card *card;
	const struct firmware *helper = NULL;
	const struct firmware *mainfw = NULL;

	lbs_deb_enter(LBS_DEB_CS);

	card = kzalloc(sizeof(struct if_cs_card), GFP_KERNEL);
	if (!card)
		goto out;

	card->p_dev = p_dev;
	p_dev->priv = card;

	p_dev->config_flags |= CONF_ENABLE_IRQ | CONF_AUTO_SET_IO;

	if (pcmcia_loop_config(p_dev, if_cs_ioprobe, NULL)) {
		pr_err("error in pcmcia_loop_config\n");
		goto out1;
	}

	if (!p_dev->irq)
		goto out1;

	
	card->iobase = ioport_map(p_dev->resource[0]->start,
				resource_size(p_dev->resource[0]));
	if (!card->iobase) {
		pr_err("error in ioport_map\n");
		ret = -EIO;
		goto out1;
	}

	ret = pcmcia_enable_device(p_dev);
	if (ret) {
		pr_err("error in pcmcia_enable_device\n");
		goto out2;
	}

	
	lbs_deb_cs("irq %d, io %pR", p_dev->irq, p_dev->resource[0]);

	card->align_regs = false;

	card->model = get_model(p_dev->manf_id, p_dev->card_id);
	if (card->model == MODEL_UNKNOWN) {
		pr_err("unsupported manf_id 0x%04x / card_id 0x%04x\n",
		       p_dev->manf_id, p_dev->card_id);
		goto out2;
	}

	
	prod_id = if_cs_read8(card, IF_CS_PRODUCT_ID);
	if (card->model == MODEL_8305) {
		card->align_regs = true;
		if (prod_id < IF_CS_CF8305_B1_REV) {
			pr_err("8305 rev B0 and older are not supported\n");
			ret = -ENODEV;
			goto out2;
		}
	}

	if ((card->model == MODEL_8381) && prod_id < IF_CS_CF8381_B3_REV) {
		pr_err("8381 rev B2 and older are not supported\n");
		ret = -ENODEV;
		goto out2;
	}

	if ((card->model == MODEL_8385) && prod_id < IF_CS_CF8385_B1_REV) {
		pr_err("8385 rev B0 and older are not supported\n");
		ret = -ENODEV;
		goto out2;
	}

	ret = lbs_get_firmware(&p_dev->dev, NULL, NULL, card->model,
				&fw_table[0], &helper, &mainfw);
	if (ret) {
		pr_err("failed to find firmware (%d)\n", ret);
		goto out2;
	}

	
	ret = if_cs_prog_helper(card, helper);
	if (ret == 0 && (card->model != MODEL_8305))
		ret = if_cs_prog_real(card, mainfw);
	if (ret)
		goto out2;

	
	priv = lbs_add_card(card, &p_dev->dev);
	if (!priv) {
		ret = -ENOMEM;
		goto out2;
	}

	
	card->priv = priv;
	priv->card = card;
	priv->hw_host_to_card = if_cs_host_to_card;
	priv->enter_deep_sleep = NULL;
	priv->exit_deep_sleep = NULL;
	priv->reset_deep_sleep_wakeup = NULL;
	priv->fw_ready = 1;

	
	ret = request_irq(p_dev->irq, if_cs_interrupt,
		IRQF_SHARED, DRV_NAME, card);
	if (ret) {
		pr_err("error in request_irq\n");
		goto out3;
	}

	if_cs_write16(card, IF_CS_CARD_INT_CAUSE, IF_CS_BIT_MASK);
	if_cs_enable_ints(card);

	
	if (lbs_start_card(priv) != 0) {
		pr_err("could not activate card\n");
		goto out3;
	}

	ret = 0;
	goto out;

out3:
	lbs_remove_card(priv);
out2:
	ioport_unmap(card->iobase);
out1:
	pcmcia_disable_device(p_dev);
out:
	if (helper)
		release_firmware(helper);
	if (mainfw)
		release_firmware(mainfw);

	lbs_deb_leave_args(LBS_DEB_CS, "ret %d", ret);
	return ret;
}


static void if_cs_detach(struct pcmcia_device *p_dev)
{
	struct if_cs_card *card = p_dev->priv;

	lbs_deb_enter(LBS_DEB_CS);

	lbs_stop_card(card->priv);
	lbs_remove_card(card->priv);
	if_cs_disable_ints(card);
	if_cs_release(p_dev);
	kfree(card);

	lbs_deb_leave(LBS_DEB_CS);
}




static const struct pcmcia_device_id if_cs_ids[] = {
	PCMCIA_DEVICE_MANF_CARD(CF8305_MANFID, CF8305_CARDID),
	PCMCIA_DEVICE_MANF_CARD(CF8381_MANFID, CF8381_CARDID),
	PCMCIA_DEVICE_MANF_CARD(CF8385_MANFID, CF8385_CARDID),
	
	PCMCIA_DEVICE_NULL,
};
MODULE_DEVICE_TABLE(pcmcia, if_cs_ids);


static struct pcmcia_driver lbs_driver = {
	.owner		= THIS_MODULE,
	.name		= DRV_NAME,
	.probe		= if_cs_probe,
	.remove		= if_cs_detach,
	.id_table       = if_cs_ids,
};


static int __init if_cs_init(void)
{
	int ret;

	lbs_deb_enter(LBS_DEB_CS);
	ret = pcmcia_register_driver(&lbs_driver);
	lbs_deb_leave(LBS_DEB_CS);
	return ret;
}


static void __exit if_cs_exit(void)
{
	lbs_deb_enter(LBS_DEB_CS);
	pcmcia_unregister_driver(&lbs_driver);
	lbs_deb_leave(LBS_DEB_CS);
}


module_init(if_cs_init);
module_exit(if_cs_exit);
