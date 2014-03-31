#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_device.h>

#include "usb.h"
#include "scsiglue.h"
#include "transport.h"
#include "init.h"

int ENE_InitMedia(struct us_data *us)
{
	int	result;
	BYTE	MiscReg03 = 0;

	printk(KERN_INFO "--- Init Media ---\n");
	result = ENE_Read_BYTE(us, REG_CARD_STATUS, &MiscReg03);
	if (result != USB_STOR_XFER_GOOD) {
		printk(KERN_ERR "Read register fail !!\n");
		return USB_STOR_TRANSPORT_ERROR;
	}
	printk(KERN_INFO "MiscReg03 = %x\n", MiscReg03);

	if (MiscReg03 & 0x02) {
		if (!us->SM_Status.Ready && !us->MS_Status.Ready) {
			result = ENE_SMInit(us);
			if (result != USB_STOR_XFER_GOOD) {
				return USB_STOR_TRANSPORT_ERROR;
			}
		}

	}
	return result;
}

int ENE_Read_BYTE(struct us_data *us, WORD index, void *buf)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap *) us->iobuf;
	int result;

	memset(bcb, 0, sizeof(struct bulk_cb_wrap));
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength	= 0x01;
	bcb->Flags			= 0x80;
	bcb->CDB[0]			= 0xED;
	bcb->CDB[2]			= (BYTE)(index>>8);
	bcb->CDB[3]			= (BYTE)index;

	result = ENE_SendScsiCmd(us, FDIR_READ, buf, 0);
	return result;
}

int ENE_SMInit(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap *) us->iobuf;
	int	result;
	BYTE	buf[0x200];

	printk(KERN_INFO "transport --- ENE_SMInit\n");

	result = ENE_LoadBinCode(us, SM_INIT_PATTERN);
	if (result != USB_STOR_XFER_GOOD) {
		printk(KERN_INFO "Load SM Init Code Fail !!\n");
		return USB_STOR_TRANSPORT_ERROR;
	}

	memset(bcb, 0, sizeof(struct bulk_cb_wrap));
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength	= 0x200;
	bcb->Flags			= 0x80;
	bcb->CDB[0]			= 0xF1;
	bcb->CDB[1]			= 0x01;

	result = ENE_SendScsiCmd(us, FDIR_READ, &buf, 0);
	if (result != USB_STOR_XFER_GOOD) {
		printk(KERN_ERR
		       "Execution SM Init Code Fail !! result = %x\n", result);
		return USB_STOR_TRANSPORT_ERROR;
	}

	us->SM_Status = *(PSM_STATUS)&buf[0];

	us->SM_DeviceID = buf[1];
	us->SM_CardID   = buf[2];

	if (us->SM_Status.Insert && us->SM_Status.Ready) {
		printk(KERN_INFO "Insert     = %x\n", us->SM_Status.Insert);
		printk(KERN_INFO "Ready      = %x\n", us->SM_Status.Ready);
		printk(KERN_INFO "WtP        = %x\n", us->SM_Status.WtP);
		printk(KERN_INFO "DeviceID   = %x\n", us->SM_DeviceID);
		printk(KERN_INFO "CardID     = %x\n", us->SM_CardID);
		MediaChange = 1;
		Check_D_MediaFmt(us);
	} else {
		printk(KERN_ERR "SM Card Not Ready --- %x\n", buf[0]);
		return USB_STOR_TRANSPORT_ERROR;
	}

	return USB_STOR_TRANSPORT_GOOD;
}

int ENE_LoadBinCode(struct us_data *us, BYTE flag)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap *) us->iobuf;
	int result;
	
	PBYTE buf;

	
	if (us->BIN_FLAG == flag)
		return USB_STOR_TRANSPORT_GOOD;

	buf = kmalloc(0x800, GFP_KERNEL);
	if (buf == NULL)
		return USB_STOR_TRANSPORT_ERROR;
	switch (flag) {
	
	case SM_INIT_PATTERN:
		printk(KERN_INFO "SM_INIT_PATTERN\n");
		memcpy(buf, SM_Init, 0x800);
		break;
	case SM_RW_PATTERN:
		printk(KERN_INFO "SM_RW_PATTERN\n");
		memcpy(buf, SM_Rdwr, 0x800);
		break;
	}

	memset(bcb, 0, sizeof(struct bulk_cb_wrap));
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength = 0x800;
	bcb->Flags = 0x00;
	bcb->CDB[0] = 0xEF;

	result = ENE_SendScsiCmd(us, FDIR_WRITE, buf, 0);

	kfree(buf);
	us->BIN_FLAG = flag;
	return result;
}

int ENE_SendScsiCmd(struct us_data *us, BYTE fDir, void *buf, int use_sg)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap *) us->iobuf;
	struct bulk_cs_wrap *bcs = (struct bulk_cs_wrap *) us->iobuf;

	int result;
	unsigned int transfer_length = bcb->DataTransferLength,
		     cswlen = 0, partial = 0;
	unsigned int residue;

	
	
	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
					    bcb, US_BULK_CB_WRAP_LEN, NULL);
	if (result != USB_STOR_XFER_GOOD) {
		printk(KERN_ERR "send cmd to out endpoint fail ---\n");
		return USB_STOR_TRANSPORT_ERROR;
	}

	if (buf) {
		unsigned int pipe = fDir;

		if (fDir == FDIR_READ)
			pipe = us->recv_bulk_pipe;
		else
			pipe = us->send_bulk_pipe;

		
		if (use_sg)
			result = usb_stor_bulk_srb(us, pipe, us->srb);
		else
			result = usb_stor_bulk_transfer_sg(us, pipe, buf,
						transfer_length, 0, &partial);
		if (result != USB_STOR_XFER_GOOD) {
			printk(KERN_ERR "data transfer fail ---\n");
			return USB_STOR_TRANSPORT_ERROR;
		}
	}

	
	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe, bcs,
						US_BULK_CS_WRAP_LEN, &cswlen);

	if (result == USB_STOR_XFER_SHORT && cswlen == 0) {
		printk(KERN_WARNING "Received 0-length CSW; retrying...\n");
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
					bcs, US_BULK_CS_WRAP_LEN, &cswlen);
	}

	if (result == USB_STOR_XFER_STALLED) {
		
		printk(KERN_WARNING "Attempting to get CSW (2nd try)...\n");
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
						bcs, US_BULK_CS_WRAP_LEN, NULL);
	}

	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	
	residue = le32_to_cpu(bcs->Residue);

	if (residue && !(us->fflags & US_FL_IGNORE_RESIDUE)) {
		residue = min(residue, transfer_length);
		if (us->srb)
			scsi_set_resid(us->srb, max(scsi_get_resid(us->srb),
					(int) residue));
	}

	if (bcs->Status != US_BULK_STAT_OK)
		return USB_STOR_TRANSPORT_ERROR;

	return USB_STOR_TRANSPORT_GOOD;
}

int ENE_Read_Data(struct us_data *us, void *buf, unsigned int length)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap *) us->iobuf;
	struct bulk_cs_wrap *bcs = (struct bulk_cs_wrap *) us->iobuf;
	int result;

	
	
	memset(bcb, 0, sizeof(struct bulk_cb_wrap));
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength = length;
	bcb->Flags = 0x80;
	bcb->CDB[0] = 0xED;
	bcb->CDB[2] = 0xFF;
	bcb->CDB[3] = 0x81;

	
	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
						US_BULK_CB_WRAP_LEN, NULL);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	
	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
						buf, length, NULL);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	
	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe, bcs,
						US_BULK_CS_WRAP_LEN, NULL);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;
	if (bcs->Status != US_BULK_STAT_OK)
		return USB_STOR_TRANSPORT_ERROR;

	return USB_STOR_TRANSPORT_GOOD;
}

int ENE_Write_Data(struct us_data *us, void *buf, unsigned int length)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap *) us->iobuf;
	struct bulk_cs_wrap *bcs = (struct bulk_cs_wrap *) us->iobuf;
	int result;

	
	
	memset(bcb, 0, sizeof(struct bulk_cb_wrap));
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength = length;
	bcb->Flags = 0x00;
	bcb->CDB[0] = 0xEE;
	bcb->CDB[2] = 0xFF;
	bcb->CDB[3] = 0x81;

	
	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
						US_BULK_CB_WRAP_LEN, NULL);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	
	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
						buf, length, NULL);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	
	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe, bcs,
						US_BULK_CS_WRAP_LEN, NULL);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;
	if (bcs->Status != US_BULK_STAT_OK)
		return USB_STOR_TRANSPORT_ERROR;

	return USB_STOR_TRANSPORT_GOOD;
}

void usb_stor_print_cmd(struct scsi_cmnd *srb)
{
	PBYTE	Cdb = srb->cmnd;
	DWORD	cmd = Cdb[0];
	DWORD	bn  =	((Cdb[2] << 24) & 0xff000000) |
			((Cdb[3] << 16) & 0x00ff0000) |
			((Cdb[4] << 8) & 0x0000ff00) |
			((Cdb[5] << 0) & 0x000000ff);
	WORD	blen = ((Cdb[7] << 8) & 0xff00) | ((Cdb[8] << 0) & 0x00ff);

	switch (cmd) {
	case TEST_UNIT_READY:
		break;
	case INQUIRY:
		printk(KERN_INFO "scsi cmd %X --- SCSIOP_INQUIRY\n", cmd);
		break;
	case MODE_SENSE:
		printk(KERN_INFO "scsi cmd %X --- SCSIOP_MODE_SENSE\n", cmd);
		break;
	case START_STOP:
		printk(KERN_INFO "scsi cmd %X --- SCSIOP_START_STOP\n", cmd);
		break;
	case READ_CAPACITY:
		printk(KERN_INFO "scsi cmd %X --- SCSIOP_READ_CAPACITY\n", cmd);
		break;
	case READ_10:
		break;
	case WRITE_10:
		break;
	case ALLOW_MEDIUM_REMOVAL:
		printk(KERN_INFO
			"scsi cmd %X --- SCSIOP_ALLOW_MEDIUM_REMOVAL\n", cmd);
		break;
	default:
		printk(KERN_INFO "scsi cmd %X --- Other cmd\n", cmd);
		break;
	}
	bn = 0;
	blen = 0;
}

