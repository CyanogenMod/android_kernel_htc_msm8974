/*
 *  Texas Instrument's Bluetooth Driver For Shared Transport.
 *
 *  Bluetooth Driver acts as interface between HCI core and
 *  TI Shared Transport Layer.
 *
 *  Copyright (C) 2009-2010 Texas Instruments
 *  Author: Raja Mani <raja_mani@ti.com>
 *	Pavan Savoy <pavan_savoy@ti.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#define DEBUG
#include <linux/platform_device.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <net/bluetooth/hci.h>

#include <linux/ti_wilink_st.h>

#define VERSION               "1.0"
#define MAX_BT_CHNL_IDS		3

#define BT_REGISTER_TIMEOUT   6000	

struct ti_st {
	struct hci_dev *hdev;
	char reg_status;
	long (*st_write) (struct sk_buff *);
	struct completion wait_reg_completion;
};

static inline void ti_st_tx_complete(struct ti_st *hst, int pkt_type)
{
	struct hci_dev *hdev = hst->hdev;

	
	switch (pkt_type) {
	case HCI_COMMAND_PKT:
		hdev->stat.cmd_tx++;
		break;

	case HCI_ACLDATA_PKT:
		hdev->stat.acl_tx++;
		break;

	case HCI_SCODATA_PKT:
		hdev->stat.sco_tx++;
		break;
	}
}


static void st_reg_completion_cb(void *priv_data, char data)
{
	struct ti_st *lhst = priv_data;

	
	lhst->reg_status = data;
	
	complete(&lhst->wait_reg_completion);
}

static long st_receive(void *priv_data, struct sk_buff *skb)
{
	struct ti_st *lhst = priv_data;
	int err;

	if (!skb)
		return -EFAULT;

	if (!lhst) {
		kfree_skb(skb);
		return -EFAULT;
	}

	skb->dev = (void *) lhst->hdev;

	
	err = hci_recv_frame(skb);
	if (err < 0) {
		BT_ERR("Unable to push skb to HCI core(%d)", err);
		return err;
	}

	lhst->hdev->stat.byte_rx += skb->len;

	return 0;
}

static struct st_proto_s ti_st_proto[MAX_BT_CHNL_IDS] = {
	{
		.chnl_id = HCI_ACLDATA_PKT, 
		.hdr_len = sizeof(struct hci_acl_hdr),
		.offset_len_in_hdr = offsetof(struct hci_acl_hdr, dlen),
		.len_size = 2,	
		.reserve = 8,
	},
	{
		.chnl_id = HCI_SCODATA_PKT, 
		.hdr_len = sizeof(struct hci_sco_hdr),
		.offset_len_in_hdr = offsetof(struct hci_sco_hdr, dlen),
		.len_size = 1, 
		.reserve = 8,
	},
	{
		.chnl_id = HCI_EVENT_PKT, 
		.hdr_len = sizeof(struct hci_event_hdr),
		.offset_len_in_hdr = offsetof(struct hci_event_hdr, plen),
		.len_size = 1, 
		.reserve = 8,
	},
};

static int ti_st_open(struct hci_dev *hdev)
{
	unsigned long timeleft;
	struct ti_st *hst;
	int err, i;

	BT_DBG("%s %p", hdev->name, hdev);

	if (test_and_set_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

	
	hst = hdev->driver_data;

	for (i = 0; i < MAX_BT_CHNL_IDS; i++) {
		ti_st_proto[i].priv_data = hst;
		ti_st_proto[i].max_frame_size = HCI_MAX_FRAME_SIZE;
		ti_st_proto[i].recv = st_receive;
		ti_st_proto[i].reg_complete_cb = st_reg_completion_cb;

		
		init_completion(&hst->wait_reg_completion);
		hst->reg_status = -EINPROGRESS;

		err = st_register(&ti_st_proto[i]);
		if (!err)
			goto done;

		if (err != -EINPROGRESS) {
			clear_bit(HCI_RUNNING, &hdev->flags);
			BT_ERR("st_register failed %d", err);
			return err;
		}

		BT_DBG("waiting for registration "
				"completion signal from ST");
		timeleft = wait_for_completion_timeout
			(&hst->wait_reg_completion,
			 msecs_to_jiffies(BT_REGISTER_TIMEOUT));
		if (!timeleft) {
			clear_bit(HCI_RUNNING, &hdev->flags);
			BT_ERR("Timeout(%d sec),didn't get reg "
					"completion signal from ST",
					BT_REGISTER_TIMEOUT / 1000);
			return -ETIMEDOUT;
		}

		if (hst->reg_status != 0) {
			clear_bit(HCI_RUNNING, &hdev->flags);
			BT_ERR("ST registration completed with invalid "
					"status %d", hst->reg_status);
			return -EAGAIN;
		}

done:
		hst->st_write = ti_st_proto[i].write;
		if (!hst->st_write) {
			BT_ERR("undefined ST write function");
			clear_bit(HCI_RUNNING, &hdev->flags);
			for (i = 0; i < MAX_BT_CHNL_IDS; i++) {
				
				err = st_unregister(&ti_st_proto[i]);
				if (err)
					BT_ERR("st_unregister() failed with "
							"error %d", err);
				hst->st_write = NULL;
			}
			return -EIO;
		}
	}
	return 0;
}

static int ti_st_close(struct hci_dev *hdev)
{
	int err, i;
	struct ti_st *hst = hdev->driver_data;

	if (!test_and_clear_bit(HCI_RUNNING, &hdev->flags))
		return 0;

	for (i = 0; i < MAX_BT_CHNL_IDS; i++) {
		err = st_unregister(&ti_st_proto[i]);
		if (err)
			BT_ERR("st_unregister(%d) failed with error %d",
					ti_st_proto[i].chnl_id, err);
	}

	hst->st_write = NULL;

	return err;
}

static int ti_st_send_frame(struct sk_buff *skb)
{
	struct hci_dev *hdev;
	struct ti_st *hst;
	long len;

	hdev = (struct hci_dev *)skb->dev;

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

	hst = hdev->driver_data;

	
	memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);

	BT_DBG("%s: type %d len %d", hdev->name, bt_cb(skb)->pkt_type,
			skb->len);

	len = hst->st_write(skb);
	if (len < 0) {
		kfree_skb(skb);
		BT_ERR("ST write failed (%ld)", len);
		
		return -EAGAIN;
	}

	
	hdev->stat.byte_tx += len;
	ti_st_tx_complete(hst, bt_cb(skb)->pkt_type);

	return 0;
}

static void ti_st_destruct(struct hci_dev *hdev)
{
	BT_DBG("%s", hdev->name);
}

static int bt_ti_probe(struct platform_device *pdev)
{
	static struct ti_st *hst;
	struct hci_dev *hdev;
	int err;

	hst = kzalloc(sizeof(struct ti_st), GFP_KERNEL);
	if (!hst)
		return -ENOMEM;

	
	hdev = hci_alloc_dev();
	if (!hdev) {
		kfree(hst);
		return -ENOMEM;
	}

	BT_DBG("hdev %p", hdev);

	hst->hdev = hdev;
	hdev->bus = HCI_UART;
	hdev->driver_data = hst;
	hdev->open = ti_st_open;
	hdev->close = ti_st_close;
	hdev->flush = NULL;
	hdev->send = ti_st_send_frame;
	hdev->destruct = ti_st_destruct;
	hdev->owner = THIS_MODULE;

	err = hci_register_dev(hdev);
	if (err < 0) {
		BT_ERR("Can't register HCI device error %d", err);
		kfree(hst);
		hci_free_dev(hdev);
		return err;
	}

	BT_DBG("HCI device registered (hdev %p)", hdev);

	dev_set_drvdata(&pdev->dev, hst);
	return err;
}

static int bt_ti_remove(struct platform_device *pdev)
{
	struct hci_dev *hdev;
	struct ti_st *hst = dev_get_drvdata(&pdev->dev);

	if (!hst)
		return -EFAULT;

	BT_DBG("%s", hst->hdev->name);

	hdev = hst->hdev;
	ti_st_close(hdev);
	hci_unregister_dev(hdev);

	hci_free_dev(hdev);
	kfree(hst);

	dev_set_drvdata(&pdev->dev, NULL);
	return 0;
}

static struct platform_driver btwilink_driver = {
	.probe = bt_ti_probe,
	.remove = bt_ti_remove,
	.driver = {
		.name = "btwilink",
		.owner = THIS_MODULE,
	},
};

static int __init btwilink_init(void)
{
	BT_INFO("Bluetooth Driver for TI WiLink - Version %s", VERSION);

	return platform_driver_register(&btwilink_driver);
}

static void __exit btwilink_exit(void)
{
	platform_driver_unregister(&btwilink_driver);
}

module_init(btwilink_init);
module_exit(btwilink_exit);


MODULE_AUTHOR("Raja Mani <raja_mani@ti.com>");
MODULE_DESCRIPTION("Bluetooth Driver for TI Shared Transport" VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
