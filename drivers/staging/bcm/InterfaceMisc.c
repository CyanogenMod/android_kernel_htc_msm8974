#include "headers.h"

INT InterfaceRDM(PS_INTERFACE_ADAPTER psIntfAdapter,
		UINT addr,
		PVOID buff,
		INT len)
{
	int bytes;
	USHORT usRetries = 0;

	if (psIntfAdapter == NULL) {
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_PRINTK, 0, 0, "Interface Adapter is NULL");
		return -EINVAL;
	}

	if (psIntfAdapter->psAdapter->device_removed == TRUE) {
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_PRINTK, 0, 0, "Device got removed");
		return -ENODEV;
	}

	if ((psIntfAdapter->psAdapter->StopAllXaction == TRUE) && (psIntfAdapter->psAdapter->chip_id >= T3LPB))	{
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_OTHERS, RDM, DBG_LVL_ALL, "Currently Xaction is not allowed on the bus");
		return -EACCES;
	}

	if (psIntfAdapter->bSuspended == TRUE || psIntfAdapter->bPreparingForBusSuspend == TRUE) {
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_OTHERS, RDM, DBG_LVL_ALL, "Bus is in suspended states hence RDM not allowed..");
		return -EACCES;
	}
	psIntfAdapter->psAdapter->DeviceAccess = TRUE;

	do {
		bytes = usb_control_msg(psIntfAdapter->udev,
					usb_rcvctrlpipe(psIntfAdapter->udev, 0),
					0x02,
					0xC2,
					(addr & 0xFFFF),
					((addr >> 16) & 0xFFFF),
					buff,
					len,
					5000);

		usRetries++;
		if (-ENODEV == bytes) {
			psIntfAdapter->psAdapter->device_removed = TRUE;
			break;
		}

	} while ((bytes < 0) && (usRetries < MAX_RDM_WRM_RETIRES));

	if (bytes < 0)
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_OTHERS, RDM, DBG_LVL_ALL, "RDM failed status :%d, retires :%d", bytes, usRetries);
	else
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_OTHERS, RDM, DBG_LVL_ALL, "RDM sent %d", bytes);

	psIntfAdapter->psAdapter->DeviceAccess = FALSE;
	return bytes;
}

INT InterfaceWRM(PS_INTERFACE_ADAPTER psIntfAdapter,
		UINT addr,
		PVOID buff,
		INT len)
{
	int retval = 0;
	USHORT usRetries = 0;

	if (psIntfAdapter == NULL) {
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_PRINTK, 0, 0, "Interface Adapter  is NULL");
		return -EINVAL;
	}

	if (psIntfAdapter->psAdapter->device_removed == TRUE) {
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_PRINTK, 0, 0, "Device got removed");
		return -ENODEV;
	}

	if ((psIntfAdapter->psAdapter->StopAllXaction == TRUE) && (psIntfAdapter->psAdapter->chip_id >= T3LPB)) {
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_OTHERS, WRM, DBG_LVL_ALL, "Currently Xaction is not allowed on the bus...");
		return -EACCES;
	}

	if (psIntfAdapter->bSuspended == TRUE || psIntfAdapter->bPreparingForBusSuspend == TRUE) {
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_OTHERS, WRM, DBG_LVL_ALL, "Bus is in suspended states hence RDM not allowed..");
		return -EACCES;
	}

	psIntfAdapter->psAdapter->DeviceAccess = TRUE;

	do {
		retval = usb_control_msg(psIntfAdapter->udev,
					usb_sndctrlpipe(psIntfAdapter->udev, 0),
					0x01,
					0x42,
					(addr & 0xFFFF),
					((addr >> 16) & 0xFFFF),
					buff,
					len,
					5000);

		usRetries++;
		if (-ENODEV == retval) {
			psIntfAdapter->psAdapter->device_removed = TRUE;
			break;
		}

	} while ((retval < 0) && (usRetries < MAX_RDM_WRM_RETIRES));

	if (retval < 0)	{
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_OTHERS, WRM, DBG_LVL_ALL, "WRM failed status :%d, retires :%d", retval, usRetries);
		psIntfAdapter->psAdapter->DeviceAccess = FALSE;
		return retval;
	} else {
		psIntfAdapter->psAdapter->DeviceAccess = FALSE;
		BCM_DEBUG_PRINT(psIntfAdapter->psAdapter, DBG_TYPE_OTHERS, WRM, DBG_LVL_ALL, "WRM sent %d", retval);
		return STATUS_SUCCESS;
	}
}

INT BcmRDM(PVOID arg,
	UINT addr,
	PVOID buff,
	INT len)
{
	return InterfaceRDM((PS_INTERFACE_ADAPTER)arg, addr, buff, len);
}

INT BcmWRM(PVOID arg,
	UINT addr,
	PVOID buff,
	INT len)
{
	return InterfaceWRM((PS_INTERFACE_ADAPTER)arg, addr, buff, len);
}

INT Bcm_clear_halt_of_endpoints(PMINI_ADAPTER Adapter)
{
	PS_INTERFACE_ADAPTER psIntfAdapter = (PS_INTERFACE_ADAPTER)(Adapter->pvInterfaceAdapter);
	INT status = STATUS_SUCCESS;


	
	Bcm_kill_all_URBs(psIntfAdapter);

	
	status = usb_clear_halt(psIntfAdapter->udev, psIntfAdapter->sIntrIn.int_in_pipe);
	if (status != STATUS_SUCCESS)
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_OTHERS, INTF_INIT, DBG_LVL_ALL, "Unable to Clear Halt of Interrupt IN end point. :%d ", status);

	status = usb_clear_halt(psIntfAdapter->udev, psIntfAdapter->sBulkIn.bulk_in_pipe);
	if (status != STATUS_SUCCESS)
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_OTHERS, INTF_INIT, DBG_LVL_ALL, "Unable to Clear Halt of Bulk IN end point. :%d ", status);

	status = usb_clear_halt(psIntfAdapter->udev, psIntfAdapter->sBulkOut.bulk_out_pipe);
	if (status != STATUS_SUCCESS)
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_OTHERS, INTF_INIT, DBG_LVL_ALL, "Unable to Clear Halt of Bulk OUT end point. :%d ", status);

	return status;
}

VOID Bcm_kill_all_URBs(PS_INTERFACE_ADAPTER psIntfAdapter)
{
	struct urb *tempUrb = NULL;
	UINT i;


	
	if (psIntfAdapter->psInterruptUrb != NULL) {
		if (psIntfAdapter->psInterruptUrb->status == -EINPROGRESS)
			usb_kill_urb(psIntfAdapter->psInterruptUrb);
	}

	
	for (i = 0; i < MAXIMUM_USB_TCB; i++) {
		tempUrb = psIntfAdapter->asUsbTcb[i].urb;
		if (tempUrb) {
			if (tempUrb->status == -EINPROGRESS)
				usb_kill_urb(tempUrb);
		}
	}

	for (i = 0; i < MAXIMUM_USB_RCB; i++) {
		tempUrb = psIntfAdapter->asUsbRcb[i].urb;
		if (tempUrb) {
			if (tempUrb->status == -EINPROGRESS)
				usb_kill_urb(tempUrb);
		}
	}

	atomic_set(&psIntfAdapter->uNumTcbUsed, 0);
	atomic_set(&psIntfAdapter->uCurrTcb, 0);

	atomic_set(&psIntfAdapter->uNumRcbUsed, 0);
	atomic_set(&psIntfAdapter->uCurrRcb, 0);
}

VOID putUsbSuspend(struct work_struct *work)
{
	PS_INTERFACE_ADAPTER psIntfAdapter = NULL;
	struct usb_interface *intf = NULL;
	psIntfAdapter = container_of(work, S_INTERFACE_ADAPTER, usbSuspendWork);
	intf = psIntfAdapter->interface;

	if (psIntfAdapter->bSuspended == FALSE)
		usb_autopm_put_interface(intf);
}

