#include "headers.h"

static struct usb_device_id InterfaceUsbtable[] = {
	{ USB_DEVICE(BCM_USB_VENDOR_ID_T3, BCM_USB_PRODUCT_ID_T3) },
	{ USB_DEVICE(BCM_USB_VENDOR_ID_T3, BCM_USB_PRODUCT_ID_T3B) },
	{ USB_DEVICE(BCM_USB_VENDOR_ID_T3, BCM_USB_PRODUCT_ID_T3L) },
	{ USB_DEVICE(BCM_USB_VENDOR_ID_T3, BCM_USB_PRODUCT_ID_SM250) },
	{ USB_DEVICE(BCM_USB_VENDOR_ID_ZTE, BCM_USB_PRODUCT_ID_226) },
	{ USB_DEVICE(BCM_USB_VENDOR_ID_FOXCONN, BCM_USB_PRODUCT_ID_1901) },
	{ USB_DEVICE(BCM_USB_VENDOR_ID_ZTE, BCM_USB_PRODUCT_ID_ZTE_TU25) },
	{ }
};
MODULE_DEVICE_TABLE(usb, InterfaceUsbtable);

static int debug = -1;
module_param(debug, uint, 0600);
MODULE_PARM_DESC(debug, "Debug level (0=none,...,16=all)");

static const u32 default_msg =
	NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK
	| NETIF_MSG_TIMER | NETIF_MSG_TX_ERR | NETIF_MSG_RX_ERR
	| NETIF_MSG_IFUP | NETIF_MSG_IFDOWN;

static int InterfaceAdapterInit(PS_INTERFACE_ADAPTER Adapter);

static void InterfaceAdapterFree(PS_INTERFACE_ADAPTER psIntfAdapter)
{
	int i = 0;

	
	if (psIntfAdapter->psAdapter->LEDInfo.led_thread_running & BCM_LED_THREAD_RUNNING_ACTIVELY) {
		psIntfAdapter->psAdapter->DriverState = DRIVER_HALT;
		wake_up(&psIntfAdapter->psAdapter->LEDInfo.notify_led_event);
	}
	reset_card_proc(psIntfAdapter->psAdapter);

	while (psIntfAdapter->psAdapter->DeviceAccess) {
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
			"Device is being accessed.\n");
		msleep(100);
	}
	
	
	usb_free_urb(psIntfAdapter->psInterruptUrb);

	
	for (i = 0; i < MAXIMUM_USB_TCB; i++) {
		if (psIntfAdapter->asUsbTcb[i].urb  != NULL) {
			usb_free_urb(psIntfAdapter->asUsbTcb[i].urb);
			psIntfAdapter->asUsbTcb[i].urb = NULL;
		}
	}
	
	for (i = 0; i < MAXIMUM_USB_RCB; i++) {
		if (psIntfAdapter->asUsbRcb[i].urb != NULL) {
			kfree(psIntfAdapter->asUsbRcb[i].urb->transfer_buffer);
			usb_free_urb(psIntfAdapter->asUsbRcb[i].urb);
			psIntfAdapter->asUsbRcb[i].urb = NULL;
		}
	}
	AdapterFree(psIntfAdapter->psAdapter);
}

static void ConfigureEndPointTypesThroughEEPROM(PMINI_ADAPTER Adapter)
{
	unsigned long ulReg = 0;
	int bytes;

	
	ulReg = ntohl(EP2_MPS_REG);
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x128, 4, TRUE);
	ulReg = ntohl(EP2_MPS);
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x12C, 4, TRUE);

	ulReg = ntohl(EP2_CFG_REG);
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x132, 4, TRUE);
	if (((PS_INTERFACE_ADAPTER)(Adapter->pvInterfaceAdapter))->bHighSpeedDevice == TRUE) {
		ulReg = ntohl(EP2_CFG_INT);
		BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x136, 4, TRUE);
	} else {
		
		ulReg = ntohl(EP2_CFG_BULK);
		BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x136, 4, TRUE);
	}

	
	ulReg = ntohl(EP4_MPS_REG);
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x13C, 4, TRUE);
	ulReg = ntohl(EP4_MPS);
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x140, 4, TRUE);

	
	bytes = rdmalt(Adapter, 0x0F0110F8, (u32 *)&ulReg, sizeof(u32));
	if (bytes < 0) {
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
			"reading of Tx EP failed\n");
		return;
	}
	ulReg |= 0x6;

	ulReg = ntohl(ulReg);
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x1CC, 4, TRUE);

	ulReg = ntohl(EP4_CFG_REG);
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x1C8, 4, TRUE);
	
	ulReg = ntohl(ISO_MPS_REG);
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x1D2, 4, TRUE);
	ulReg = ntohl(ISO_MPS);
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x1D6, 4, TRUE);

	ReadBeceemEEPROM(Adapter, 0x1FC, (PUINT)&ulReg);
	ulReg &= 0x0101FFFF;
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x1FC, 4, TRUE);

	

	ReadBeceemEEPROM(Adapter, 0xA8, (PUINT)&ulReg);
	if ((ulReg&0x00FF0000)>>16 > 0x30) {
		ulReg = (ulReg&0xFF00FFFF)|(0x30<<16);
		BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0xA8, 4, TRUE);
	}
	ReadBeceemEEPROM(Adapter, 0x148, (PUINT)&ulReg);
	if ((ulReg&0x00FF0000)>>16 > 0x30) {
		ulReg = (ulReg&0xFF00FFFF)|(0x30<<16);
		BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x148, 4, TRUE);
	}
	ulReg = 0;
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x122, 4, TRUE);
	ulReg = 0;
	BeceemEEPROMBulkWrite(Adapter, (PUCHAR)&ulReg, 0x1C2, 4, TRUE);
}

static int usbbcm_device_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	int retval;
	PMINI_ADAPTER psAdapter;
	PS_INTERFACE_ADAPTER psIntfAdapter;
	struct net_device *ndev;

	
	ndev = alloc_etherdev_mq(sizeof(MINI_ADAPTER), NO_OF_QUEUES+1);
	if (ndev == NULL) {
		dev_err(&udev->dev, DRV_NAME ": no memory for device\n");
		return -ENOMEM;
	}

	SET_NETDEV_DEV(ndev, &intf->dev);

	psAdapter = netdev_priv(ndev);
	psAdapter->dev = ndev;
	psAdapter->msg_enable = netif_msg_init(debug, default_msg);

	

	psAdapter->stDebugState.debug_level = DBG_LVL_CURR;
	psAdapter->stDebugState.type = DBG_TYPE_INITEXIT;


	psAdapter->stDebugState.subtype[DBG_TYPE_INITEXIT] = 0xff;
	BCM_SHOW_DEBUG_BITMAP(psAdapter);

	retval = InitAdapter(psAdapter);
	if (retval) {
		dev_err(&udev->dev, DRV_NAME ": InitAdapter Failed\n");
		AdapterFree(psAdapter);
		return retval;
	}

	
	psIntfAdapter = kzalloc(sizeof(S_INTERFACE_ADAPTER), GFP_KERNEL);
	if (psIntfAdapter == NULL) {
		dev_err(&udev->dev, DRV_NAME ": no memory for Interface adapter\n");
		AdapterFree(psAdapter);
		return -ENOMEM;
	}

	psAdapter->pvInterfaceAdapter = psIntfAdapter;
	psIntfAdapter->psAdapter = psAdapter;

	
	psIntfAdapter->interface = intf;
	usb_set_intfdata(intf, psIntfAdapter);

	BCM_DEBUG_PRINT(psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
		"psIntfAdapter 0x%p\n", psIntfAdapter);
	retval = InterfaceAdapterInit(psIntfAdapter);
	if (retval) {
		if (-ENOENT == retval) {
			BCM_DEBUG_PRINT(psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
				"File Not Found.  Use app to download.\n");
			return STATUS_SUCCESS;
		}
		BCM_DEBUG_PRINT(psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
			"InterfaceAdapterInit failed.\n");
		usb_set_intfdata(intf, NULL);
		udev = interface_to_usbdev(intf);
		usb_put_dev(udev);
		InterfaceAdapterFree(psIntfAdapter);
		return retval;
	}
	if (psAdapter->chip_id > T3) {
		uint32_t uiNackZeroLengthInt = 4;

		retval = wrmalt(psAdapter, DISABLE_USB_ZERO_LEN_INT, &uiNackZeroLengthInt, sizeof(uiNackZeroLengthInt));
		if (retval)
			return retval;
	}

	
	if (USB_CONFIG_ATT_WAKEUP & udev->actconfig->desc.bmAttributes) {
		
		if (psAdapter->bDoSuspend) {
#ifdef CONFIG_PM
			pm_runtime_set_autosuspend_delay(&udev->dev, 0);
			intf->needs_remote_wakeup = 1;
			usb_enable_autosuspend(udev);
			device_init_wakeup(&intf->dev, 1);
			INIT_WORK(&psIntfAdapter->usbSuspendWork, putUsbSuspend);
			BCM_DEBUG_PRINT(psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
				"Enabling USB Auto-Suspend\n");
#endif
		} else {
			intf->needs_remote_wakeup = 0;
			usb_disable_autosuspend(udev);
		}
	}

	psAdapter->stDebugState.subtype[DBG_TYPE_INITEXIT] = 0x0;
	return retval;
}

static void usbbcm_disconnect(struct usb_interface *intf)
{
	PS_INTERFACE_ADAPTER psIntfAdapter = usb_get_intfdata(intf);
	PMINI_ADAPTER psAdapter;
	struct usb_device  *udev = interface_to_usbdev(intf);

	if (psIntfAdapter == NULL)
		return;

	psAdapter = psIntfAdapter->psAdapter;
	netif_device_detach(psAdapter->dev);

	if (psAdapter->bDoSuspend)
		intf->needs_remote_wakeup = 0;

	psAdapter->device_removed = TRUE ;
	usb_set_intfdata(intf, NULL);
	InterfaceAdapterFree(psIntfAdapter);
	usb_put_dev(udev);
}

static int AllocUsbCb(PS_INTERFACE_ADAPTER psIntfAdapter)
{
	int i = 0;

	for (i = 0; i < MAXIMUM_USB_TCB; i++) {
		psIntfAdapter->asUsbTcb[i].urb = usb_alloc_urb(0, GFP_KERNEL);

		if (psIntfAdapter->asUsbTcb[i].urb == NULL) {
			BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_PRINTK, 0, 0,
				"Can't allocate Tx urb for index %d\n", i);
			return -ENOMEM;
		}
	}

	for (i = 0; i < MAXIMUM_USB_RCB; i++) {
		psIntfAdapter->asUsbRcb[i].urb = usb_alloc_urb(0, GFP_KERNEL);

		if (psIntfAdapter->asUsbRcb[i].urb == NULL) {
			BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_PRINTK, 0, 0,
				"Can't allocate Rx urb for index %d\n", i);
			return -ENOMEM;
		}

		psIntfAdapter->asUsbRcb[i].urb->transfer_buffer = kmalloc(MAX_DATA_BUFFER_SIZE, GFP_KERNEL);

		if (psIntfAdapter->asUsbRcb[i].urb->transfer_buffer == NULL) {
			BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_PRINTK, 0, 0,
				"Can't allocate Rx buffer for index %d\n", i);
			return -ENOMEM;
		}
		psIntfAdapter->asUsbRcb[i].urb->transfer_buffer_length = MAX_DATA_BUFFER_SIZE;
	}
	return 0;
}

static int device_run(PS_INTERFACE_ADAPTER psIntfAdapter)
{
	int value = 0;
	UINT status = STATUS_SUCCESS;

	status = InitCardAndDownloadFirmware(psIntfAdapter->psAdapter);
	if (status != STATUS_SUCCESS) {
		pr_err(DRV_NAME "InitCardAndDownloadFirmware failed.\n");
		return status;
	}
	if (TRUE == psIntfAdapter->psAdapter->fw_download_done) {
		if (StartInterruptUrb(psIntfAdapter)) {
			BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
			"Cannot send interrupt in URB\n");
		}

		psIntfAdapter->psAdapter->waiting_to_fw_download_done = FALSE;
		value = wait_event_timeout(psIntfAdapter->psAdapter->ioctl_fw_dnld_wait_queue,
					psIntfAdapter->psAdapter->waiting_to_fw_download_done, 5*HZ);

		if (value == 0)
			pr_err(DRV_NAME ": Timeout waiting for mailbox interrupt.\n");

		if (register_control_device_interface(psIntfAdapter->psAdapter) < 0) {
			pr_err(DRV_NAME ": Register Control Device failed.\n");
			return -EIO;
		}
	}
	return 0;
}


static inline int bcm_usb_endpoint_num(const struct usb_endpoint_descriptor *epd)
{
	return epd->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
}

static inline int bcm_usb_endpoint_type(const struct usb_endpoint_descriptor *epd)
{
	return epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
}

static inline int bcm_usb_endpoint_dir_in(const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN);
}

static inline int bcm_usb_endpoint_dir_out(const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT);
}

static inline int bcm_usb_endpoint_xfer_bulk(const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
		USB_ENDPOINT_XFER_BULK);
}

static inline int bcm_usb_endpoint_xfer_control(const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
		USB_ENDPOINT_XFER_CONTROL);
}

static inline int bcm_usb_endpoint_xfer_int(const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
		USB_ENDPOINT_XFER_INT);
}

static inline int bcm_usb_endpoint_xfer_isoc(const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
		USB_ENDPOINT_XFER_ISOC);
}

static inline int bcm_usb_endpoint_is_bulk_in(const struct usb_endpoint_descriptor *epd)
{
	return bcm_usb_endpoint_xfer_bulk(epd) && bcm_usb_endpoint_dir_in(epd);
}

static inline int bcm_usb_endpoint_is_bulk_out(const struct usb_endpoint_descriptor *epd)
{
	return bcm_usb_endpoint_xfer_bulk(epd) && bcm_usb_endpoint_dir_out(epd);
}

static inline int bcm_usb_endpoint_is_int_in(const struct usb_endpoint_descriptor *epd)
{
	return bcm_usb_endpoint_xfer_int(epd) && bcm_usb_endpoint_dir_in(epd);
}

static inline int bcm_usb_endpoint_is_int_out(const struct usb_endpoint_descriptor *epd)
{
	return bcm_usb_endpoint_xfer_int(epd) && bcm_usb_endpoint_dir_out(epd);
}

static inline int bcm_usb_endpoint_is_isoc_in(const struct usb_endpoint_descriptor *epd)
{
	return bcm_usb_endpoint_xfer_isoc(epd) && bcm_usb_endpoint_dir_in(epd);
}

static inline int bcm_usb_endpoint_is_isoc_out(const struct usb_endpoint_descriptor *epd)
{
	return bcm_usb_endpoint_xfer_isoc(epd) && bcm_usb_endpoint_dir_out(epd);
}

static int InterfaceAdapterInit(PS_INTERFACE_ADAPTER psIntfAdapter)
{
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	size_t buffer_size;
	unsigned long value;
	int retval = 0;
	int usedIntOutForBulkTransfer = 0 ;
	BOOLEAN bBcm16 = FALSE;
	UINT uiData = 0;
	int bytes;

	
	psIntfAdapter->udev = usb_get_dev(interface_to_usbdev(psIntfAdapter->interface));

	psIntfAdapter->bHighSpeedDevice = (psIntfAdapter->udev->speed == USB_SPEED_HIGH);
	psIntfAdapter->psAdapter->interface_rdm = BcmRDM;
	psIntfAdapter->psAdapter->interface_wrm = BcmWRM;

	bytes = rdmalt(psIntfAdapter->psAdapter, CHIP_ID_REG,
			(u32 *)&(psIntfAdapter->psAdapter->chip_id), sizeof(u32));
	if (bytes < 0) {
		retval = bytes;
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_PRINTK, 0, 0, "CHIP ID Read Failed\n");
		return retval;
	}

	if (0xbece3200 == (psIntfAdapter->psAdapter->chip_id & ~(0xF0)))
		psIntfAdapter->psAdapter->chip_id &= ~0xF0;

	dev_info(&psIntfAdapter->udev->dev, "RDM Chip ID 0x%lx\n",
		 psIntfAdapter->psAdapter->chip_id);

	iface_desc = psIntfAdapter->interface->cur_altsetting;

	if (psIntfAdapter->psAdapter->chip_id == T3B) {
		
		BeceemEEPROMBulkRead(psIntfAdapter->psAdapter, &uiData, 0x0, 4);
		if (uiData == BECM)
			bBcm16 = TRUE;

		dev_info(&psIntfAdapter->udev->dev, "number of alternate setting %d\n",
			 psIntfAdapter->interface->num_altsetting);

		if (bBcm16 == TRUE) {
			
			if (psIntfAdapter->bHighSpeedDevice)
				retval = usb_set_interface(psIntfAdapter->udev, DEFAULT_SETTING_0, ALTERNATE_SETTING_1);
			BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
				"BCM16 is applicable on this dongle\n");
			if (retval || (psIntfAdapter->bHighSpeedDevice == FALSE)) {
				usedIntOutForBulkTransfer = EP2 ;
				endpoint = &iface_desc->endpoint[EP2].desc;
				BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
					 "Interface altsetting failed or modem is configured to Full Speed, hence will work on default setting 0\n");
				if (((psIntfAdapter->bHighSpeedDevice == TRUE) && (bcm_usb_endpoint_is_int_out(endpoint) == FALSE))
					|| ((psIntfAdapter->bHighSpeedDevice == FALSE) && (bcm_usb_endpoint_is_bulk_out(endpoint) == FALSE))) {
					BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
						"Configuring the EEPROM\n");
					
					ConfigureEndPointTypesThroughEEPROM(psIntfAdapter->psAdapter);

					retval = usb_reset_device(psIntfAdapter->udev);
					if (retval) {
						BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
							"reset failed.  Re-enumerating the device.\n");
						return retval ;
					}

				}
				if ((psIntfAdapter->bHighSpeedDevice == FALSE) && bcm_usb_endpoint_is_bulk_out(endpoint)) {
					
					UINT _uiData = ntohl(EP2_CFG_INT);
					BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
						"Reverting Bulk to INT as it is in Full Speed mode.\n");
					BeceemEEPROMBulkWrite(psIntfAdapter->psAdapter, (PUCHAR)&_uiData, 0x136, 4, TRUE);
				}
			} else {
				usedIntOutForBulkTransfer = EP4 ;
				endpoint = &iface_desc->endpoint[EP4].desc;
				BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
					"Choosing AltSetting as a default setting.\n");
				if (bcm_usb_endpoint_is_int_out(endpoint) == FALSE) {
					BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
						"Dongle does not have BCM16 Fix.\n");
					
					ConfigureEndPointTypesThroughEEPROM(psIntfAdapter->psAdapter);

					retval = usb_reset_device(psIntfAdapter->udev);
					if (retval) {
						BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
							"reset failed.  Re-enumerating the device.\n");
						return retval;
					}

				}
			}
		}
	}

	iface_desc = psIntfAdapter->interface->cur_altsetting;

	for (value = 0; value < iface_desc->desc.bNumEndpoints; ++value) {
		endpoint = &iface_desc->endpoint[value].desc;

		if (!psIntfAdapter->sBulkIn.bulk_in_endpointAddr && bcm_usb_endpoint_is_bulk_in(endpoint)) {
			buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
			psIntfAdapter->sBulkIn.bulk_in_size = buffer_size;
			psIntfAdapter->sBulkIn.bulk_in_endpointAddr = endpoint->bEndpointAddress;
			psIntfAdapter->sBulkIn.bulk_in_pipe =
					usb_rcvbulkpipe(psIntfAdapter->udev,
								psIntfAdapter->sBulkIn.bulk_in_endpointAddr);
		}

		if (!psIntfAdapter->sBulkOut.bulk_out_endpointAddr && bcm_usb_endpoint_is_bulk_out(endpoint)) {
			psIntfAdapter->sBulkOut.bulk_out_endpointAddr = endpoint->bEndpointAddress;
			psIntfAdapter->sBulkOut.bulk_out_pipe =
				usb_sndbulkpipe(psIntfAdapter->udev,
					psIntfAdapter->sBulkOut.bulk_out_endpointAddr);
		}

		if (!psIntfAdapter->sIntrIn.int_in_endpointAddr && bcm_usb_endpoint_is_int_in(endpoint)) {
			buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
			psIntfAdapter->sIntrIn.int_in_size = buffer_size;
			psIntfAdapter->sIntrIn.int_in_endpointAddr = endpoint->bEndpointAddress;
			psIntfAdapter->sIntrIn.int_in_interval = endpoint->bInterval;
			psIntfAdapter->sIntrIn.int_in_buffer =
						kmalloc(buffer_size, GFP_KERNEL);
			if (!psIntfAdapter->sIntrIn.int_in_buffer) {
				dev_err(&psIntfAdapter->udev->dev,
					"could not allocate interrupt_in_buffer\n");
				return -EINVAL;
			}
		}

		if (!psIntfAdapter->sIntrOut.int_out_endpointAddr && bcm_usb_endpoint_is_int_out(endpoint)) {
			if (!psIntfAdapter->sBulkOut.bulk_out_endpointAddr &&
				(psIntfAdapter->psAdapter->chip_id == T3B) && (value == usedIntOutForBulkTransfer)) {
				
				buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
				psIntfAdapter->sBulkOut.bulk_out_size = buffer_size;
				psIntfAdapter->sBulkOut.bulk_out_endpointAddr = endpoint->bEndpointAddress;
				psIntfAdapter->sBulkOut.bulk_out_pipe = usb_sndintpipe(psIntfAdapter->udev,
									psIntfAdapter->sBulkOut.bulk_out_endpointAddr);
				psIntfAdapter->sBulkOut.int_out_interval = endpoint->bInterval;
			} else if (value == EP6) {
				buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
				psIntfAdapter->sIntrOut.int_out_size = buffer_size;
				psIntfAdapter->sIntrOut.int_out_endpointAddr = endpoint->bEndpointAddress;
				psIntfAdapter->sIntrOut.int_out_interval = endpoint->bInterval;
				psIntfAdapter->sIntrOut.int_out_buffer = kmalloc(buffer_size, GFP_KERNEL);
				if (!psIntfAdapter->sIntrOut.int_out_buffer) {
					dev_err(&psIntfAdapter->udev->dev,
						"could not allocate interrupt_out_buffer\n");
					return -EINVAL;
				}
			}
		}
	}

	usb_set_intfdata(psIntfAdapter->interface, psIntfAdapter);

	psIntfAdapter->psAdapter->bcm_file_download = InterfaceFileDownload;
	psIntfAdapter->psAdapter->bcm_file_readback_from_chip =
				InterfaceFileReadbackFromChip;
	psIntfAdapter->psAdapter->interface_transmit = InterfaceTransmitPacket;

	retval = CreateInterruptUrb(psIntfAdapter);

	if (retval) {
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_PRINTK, 0, 0,
			"Cannot create interrupt urb\n");
		return retval;
	}

	retval = AllocUsbCb(psIntfAdapter);
	if (retval)
		return retval;

	return device_run(psIntfAdapter);
}

static int InterfaceSuspend(struct usb_interface *intf, pm_message_t message)
{
	PS_INTERFACE_ADAPTER  psIntfAdapter = usb_get_intfdata(intf);

	psIntfAdapter->bSuspended = TRUE;

	if (TRUE == psIntfAdapter->bPreparingForBusSuspend) {
		psIntfAdapter->bPreparingForBusSuspend = FALSE;

		if (psIntfAdapter->psAdapter->LinkStatus == LINKUP_DONE) {
			psIntfAdapter->psAdapter->IdleMode = TRUE ;
			BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
				"Host Entered in PMU Idle Mode.\n");
		} else {
			psIntfAdapter->psAdapter->bShutStatus = TRUE;
			BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_INITEXIT, DRV_ENTRY, DBG_LVL_ALL,
				"Host Entered in PMU Shutdown Mode.\n");
		}
	}
	psIntfAdapter->psAdapter->bPreparingForLowPowerMode = FALSE;

	
	wake_up(&psIntfAdapter->psAdapter->lowpower_mode_wait_queue);

	return 0;
}

static int InterfaceResume(struct usb_interface *intf)
{
	PS_INTERFACE_ADAPTER  psIntfAdapter = usb_get_intfdata(intf);

	mdelay(100);
	psIntfAdapter->bSuspended = FALSE;

	StartInterruptUrb(psIntfAdapter);
	InterfaceRx(psIntfAdapter);
	return 0;
}

static struct usb_driver usbbcm_driver = {
	.name = "usbbcm",
	.probe = usbbcm_device_probe,
	.disconnect = usbbcm_disconnect,
	.suspend = InterfaceSuspend,
	.resume = InterfaceResume,
	.id_table = InterfaceUsbtable,
	.supports_autosuspend = 1,
};

struct class *bcm_class;

static __init int bcm_init(void)
{
	printk(KERN_INFO "%s: %s, %s\n", DRV_NAME, DRV_DESCRIPTION, DRV_VERSION);
	printk(KERN_INFO "%s\n", DRV_COPYRIGHT);

	bcm_class = class_create(THIS_MODULE, DRV_NAME);
	if (IS_ERR(bcm_class)) {
		printk(KERN_ERR DRV_NAME ": could not create class\n");
		return PTR_ERR(bcm_class);
	}

	return usb_register(&usbbcm_driver);
}

static __exit void bcm_exit(void)
{
	usb_deregister(&usbbcm_driver);
	class_destroy(bcm_class);
}

module_init(bcm_init);
module_exit(bcm_exit);

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");
