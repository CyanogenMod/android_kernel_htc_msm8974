#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_device.h>

#include "usb.h"
#include "scsiglue.h"
#include "transport.h"

static void usb_stor_blocking_completion(struct urb *urb)
{
	struct completion *urb_done_ptr = urb->context;

	
	complete(urb_done_ptr);
}

static int usb_stor_msg_common(struct us_data *us, int timeout)
{
	struct completion urb_done;
	long timeleft;
	int status;

	
	if (test_bit(US_FLIDX_ABORTING, &us->dflags))
		return -EIO;

	init_completion(&urb_done);

	us->current_urb->context = &urb_done;
	us->current_urb->actual_length = 0;
	us->current_urb->error_count = 0;
	us->current_urb->status = 0;

	us->current_urb->transfer_flags = 0;
	if (us->current_urb->transfer_buffer == us->iobuf)
		us->current_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	us->current_urb->transfer_dma = us->iobuf_dma;
	us->current_urb->setup_dma = us->cr_dma;

	status = usb_submit_urb(us->current_urb, GFP_NOIO);
	if (status)
		return status;

	set_bit(US_FLIDX_URB_ACTIVE, &us->dflags);

	if (test_bit(US_FLIDX_ABORTING, &us->dflags)) {
		if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->dflags)) {
			
			usb_unlink_urb(us->current_urb);
		}
	}

	timeleft = wait_for_completion_interruptible_timeout(&urb_done,
					timeout ? : MAX_SCHEDULE_TIMEOUT);
	clear_bit(US_FLIDX_URB_ACTIVE, &us->dflags);

	if (timeleft <= 0) {
		usb_kill_urb(us->current_urb);
	}

	return us->current_urb->status;
}

int usb_stor_control_msg(struct us_data *us, unsigned int pipe,
		 u8 request, u8 requesttype, u16 value, u16 index,
		 void *data, u16 size, int timeout)
{
	int status;

	

	
	us->cr->bRequestType = requesttype;
	us->cr->bRequest = request;
	us->cr->wValue = cpu_to_le16(value);
	us->cr->wIndex = cpu_to_le16(index);
	us->cr->wLength = cpu_to_le16(size);

	
	usb_fill_control_urb(us->current_urb, us->pusb_dev, pipe,
			 (unsigned char *) us->cr, data, size,
			 usb_stor_blocking_completion, NULL);
	status = usb_stor_msg_common(us, timeout);

	
	if (status == 0)
		status = us->current_urb->actual_length;
	return status;
}

int usb_stor_clear_halt(struct us_data *us, unsigned int pipe)
{
	int result;
	int endp = usb_pipeendpoint(pipe);

	
	if (usb_pipein(pipe))
		endp |= USB_DIR_IN;

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
		USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT,
		USB_ENDPOINT_HALT, endp,
		NULL, 0, 3*HZ);

	
	if (result >= 0)
		usb_reset_endpoint(us->pusb_dev, endp);

	return result;
}

static int interpret_urb_result(struct us_data *us, unsigned int pipe,
		unsigned int length, int result, unsigned int partial)
{
	
	switch (result) {
	
	case 0:
		if (partial != length) {
			
			return USB_STOR_XFER_SHORT;
		}
		
		return USB_STOR_XFER_GOOD;
	case -EPIPE:
		if (usb_pipecontrol(pipe)) {
			
			return USB_STOR_XFER_STALLED;
		}
		
		if (usb_stor_clear_halt(us, pipe) < 0)
			return USB_STOR_XFER_ERROR;
		return USB_STOR_XFER_STALLED;
	case -EOVERFLOW:
		
		return USB_STOR_XFER_LONG;
	case -ECONNRESET:
		
		return USB_STOR_XFER_ERROR;
	case -EREMOTEIO:
		
		return USB_STOR_XFER_SHORT;
	case -EIO:
		
		return USB_STOR_XFER_ERROR;
	default:
		
		return USB_STOR_XFER_ERROR;
	}
}

int usb_stor_bulk_transfer_buf(struct us_data *us, unsigned int pipe,
	void *buf, unsigned int length, unsigned int *act_len)
{
	int result;

	

	
	usb_fill_bulk_urb(us->current_urb, us->pusb_dev, pipe, buf,
				length, usb_stor_blocking_completion, NULL);
	result = usb_stor_msg_common(us, 0);

	
	if (act_len)
		*act_len = us->current_urb->actual_length;

	return interpret_urb_result(us, pipe, length, result,
					us->current_urb->actual_length);
}

static int usb_stor_bulk_transfer_sglist(struct us_data *us, unsigned int pipe,
		struct scatterlist *sg, int num_sg, unsigned int length,
		unsigned int *act_len)
{
	int result;

	
	if (test_bit(US_FLIDX_ABORTING, &us->dflags))
		return USB_STOR_XFER_ERROR;

	
	result = usb_sg_init(&us->current_sg, us->pusb_dev, pipe, 0,
					sg, num_sg, length, GFP_NOIO);
	if (result) {
		
		return USB_STOR_XFER_ERROR;
	}

	set_bit(US_FLIDX_SG_ACTIVE, &us->dflags);

	
	if (test_bit(US_FLIDX_ABORTING, &us->dflags)) {
		
		if (test_and_clear_bit(US_FLIDX_SG_ACTIVE, &us->dflags)) {
			
			usb_sg_cancel(&us->current_sg);
		}
	}

	
	usb_sg_wait(&us->current_sg);
	clear_bit(US_FLIDX_SG_ACTIVE, &us->dflags);

	result = us->current_sg.status;
	if (act_len)
		*act_len = us->current_sg.bytes;

	return interpret_urb_result(us, pipe, length,
					result, us->current_sg.bytes);
}

int usb_stor_bulk_srb(struct us_data *us, unsigned int pipe,
					struct scsi_cmnd *srb)
{
	unsigned int partial;
	int result = usb_stor_bulk_transfer_sglist(us, pipe, scsi_sglist(srb),
				      scsi_sg_count(srb), scsi_bufflen(srb),
				      &partial);

	scsi_set_resid(srb, scsi_bufflen(srb) - partial);
	return result;
}

int usb_stor_bulk_transfer_sg(struct us_data *us, unsigned int pipe,
		void *buf, unsigned int length_left, int use_sg, int *residual)
{
	int result;
	unsigned int partial;

	
	
	if (use_sg) {
		
		result = usb_stor_bulk_transfer_sglist(us, pipe,
				(struct scatterlist *) buf, use_sg,
				length_left, &partial);
		length_left -= partial;
	} else {
		
		result = usb_stor_bulk_transfer_buf(us, pipe, buf,
							length_left, &partial);
		length_left -= partial;
	}

	
	if (residual)
		*residual = length_left;
	return result;
}

void usb_stor_invoke_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	int need_auto_sense;
	int result;

	
	usb_stor_print_cmd(srb);
	
	scsi_set_resid(srb, 0);
	result = us->transport(srb, us); 

	if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
		
		srb->result = DID_ABORT << 16;
		goto Handle_Errors;
	}

	
	if (result == USB_STOR_TRANSPORT_ERROR) {
		
		srb->result = DID_ERROR << 16;
		goto Handle_Errors;
	}

	
	if (result == USB_STOR_TRANSPORT_NO_SENSE) {
		srb->result = SAM_STAT_CHECK_CONDITION;
		return;
	}

	srb->result = SAM_STAT_GOOD;

	
	need_auto_sense = 0;

	if ((us->protocol == USB_PR_CB || us->protocol == USB_PR_DPCM_USB) &&
				srb->sc_data_direction != DMA_FROM_DEVICE) {
		
		need_auto_sense = 1;
	}

	if (result == USB_STOR_TRANSPORT_FAILED) {
		
		need_auto_sense = 1;
	}

	
	if (need_auto_sense) {
		int temp_result;
		struct scsi_eh_save ses;

		pr_info("Issuing auto-REQUEST_SENSE\n");

		scsi_eh_prep_cmnd(srb, &ses, NULL, 0, US_SENSE_SIZE);

		
		if (us->subclass == USB_SC_RBC ||
			us->subclass == USB_SC_SCSI ||
			us->subclass == USB_SC_CYP_ATACB) {
			srb->cmd_len = 6;
		} else {
			srb->cmd_len = 12;
		}
		
		scsi_set_resid(srb, 0);
		temp_result = us->transport(us->srb, us);

		
		scsi_eh_restore_cmnd(srb, &ses);

		if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
			
			srb->result = DID_ABORT << 16;
			goto Handle_Errors;
		}
		if (temp_result != USB_STOR_TRANSPORT_GOOD) {
			
			srb->result = DID_ERROR << 16;
			if (!(us->fflags & US_FL_SCM_MULT_TARG))
				goto Handle_Errors;
			return;
		}

		
		srb->result = SAM_STAT_CHECK_CONDITION;

		if (result == USB_STOR_TRANSPORT_GOOD &&
			(srb->sense_buffer[2] & 0xaf) == 0 &&
			srb->sense_buffer[12] == 0 &&
			srb->sense_buffer[13] == 0) {
			srb->result = SAM_STAT_GOOD;
			srb->sense_buffer[0] = 0x0;
		}
	}

	
	if (srb->result == SAM_STAT_GOOD && scsi_bufflen(srb) -
				scsi_get_resid(srb) < srb->underflow)
		srb->result = (DID_ERROR << 16);
		

	return;

Handle_Errors:
	scsi_lock(us_to_host(us));
	set_bit(US_FLIDX_RESETTING, &us->dflags);
	clear_bit(US_FLIDX_ABORTING, &us->dflags);
	scsi_unlock(us_to_host(us));

	mutex_unlock(&us->dev_mutex);
	result = usb_stor_port_reset(us);
	mutex_lock(&us->dev_mutex);

	if (result < 0) {
		scsi_lock(us_to_host(us));
		usb_stor_report_device_reset(us);
		scsi_unlock(us_to_host(us));
		us->transport_reset(us);
	}
	clear_bit(US_FLIDX_RESETTING, &us->dflags);
}

void ENE_stor_invoke_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	int result = 0;

	
	usb_stor_print_cmd(srb);
	
	scsi_set_resid(srb, 0);
	if (!(us->SM_Status.Ready))
		result = ENE_InitMedia(us);

	if (us->Power_IsResum == true) {
		result = ENE_InitMedia(us);
		us->Power_IsResum = false;
	}

	if (us->SM_Status.Ready)
		result = SM_SCSIIrp(us, srb);

	if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
		
		srb->result = DID_ABORT << 16;
		goto Handle_Errors;
	}

	
	if (result == USB_STOR_TRANSPORT_ERROR) {
		
		srb->result = DID_ERROR << 16;
		goto Handle_Errors;
	}

	
	if (result == USB_STOR_TRANSPORT_NO_SENSE) {
		srb->result = SAM_STAT_CHECK_CONDITION;
		return;
	}

	srb->result = SAM_STAT_GOOD;
	if (result == USB_STOR_TRANSPORT_FAILED) {
		
		
		BuildSenseBuffer(srb, us->SrbStatus);
		srb->result = SAM_STAT_CHECK_CONDITION;
	}

	
	if (srb->result == SAM_STAT_GOOD && scsi_bufflen(srb) -
					scsi_get_resid(srb) < srb->underflow)
		srb->result = (DID_ERROR << 16);
		

	return;

Handle_Errors:
	scsi_lock(us_to_host(us));
	set_bit(US_FLIDX_RESETTING, &us->dflags);
	clear_bit(US_FLIDX_ABORTING, &us->dflags);
	scsi_unlock(us_to_host(us));

	mutex_unlock(&us->dev_mutex);
	result = usb_stor_port_reset(us);
	mutex_lock(&us->dev_mutex);

	if (result < 0) {
		scsi_lock(us_to_host(us));
		usb_stor_report_device_reset(us);
		scsi_unlock(us_to_host(us));
		us->transport_reset(us);
	}
	clear_bit(US_FLIDX_RESETTING, &us->dflags);
}

void BuildSenseBuffer(struct scsi_cmnd *srb, int SrbStatus)
{
	BYTE    *buf = srb->sense_buffer;
	BYTE    asc;

	pr_info("transport --- BuildSenseBuffer\n");
	switch (SrbStatus) {
	case SS_NOT_READY:
		asc = 0x3a;
		break;  
	case SS_MEDIUM_ERR:
		asc = 0x0c;
		break;  
	case SS_ILLEGAL_REQUEST:
		asc = 0x20;
		break;  
	default:
		asc = 0x00;
		break;  
	}

	memset(buf, 0, 18);
	buf[0x00] = 0xf0;
	buf[0x02] = SrbStatus;
	buf[0x07] = 0x0b;
	buf[0x0c] = asc;
}

void usb_stor_stop_transport(struct us_data *us)
{
	

	if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->dflags)) {
		
		usb_unlink_urb(us->current_urb);
	}

	if (test_and_clear_bit(US_FLIDX_SG_ACTIVE, &us->dflags)) {
		
		usb_sg_cancel(&us->current_sg);
	}
}

int usb_stor_Bulk_max_lun(struct us_data *us)
{
	int result;

	
	
	us->iobuf[0] = 0;
	result = usb_stor_control_msg(us, us->recv_ctrl_pipe,
				 US_BULK_GET_MAX_LUN,
				 USB_DIR_IN | USB_TYPE_CLASS |
				 USB_RECIP_INTERFACE,
				 0, us->ifnum, us->iobuf, 1, HZ);


	
	if (result > 0)
		return us->iobuf[0];

	return 0;
}

int usb_stor_Bulk_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap *) us->iobuf;
	struct bulk_cs_wrap *bcs = (struct bulk_cs_wrap *) us->iobuf;
	unsigned int transfer_length = scsi_bufflen(srb);
	unsigned int residue;
	int result;
	int fake_sense = 0;
	unsigned int cswlen;
	unsigned int cbwlen = US_BULK_CB_WRAP_LEN;

	
	
	if (unlikely(us->fflags & US_FL_BULK32)) {
		cbwlen = 32;
		us->iobuf[31] = 0;
	}

	
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength = cpu_to_le32(transfer_length);
	bcb->Flags = srb->sc_data_direction == DMA_FROM_DEVICE ? 1 << 7 : 0;
	bcb->Tag = ++us->tag;
	bcb->Lun = srb->device->lun;
	if (us->fflags & US_FL_SCM_MULT_TARG)
		bcb->Lun |= srb->device->id << 4;
	bcb->Length = srb->cmd_len;

	
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, srb->cmnd, bcb->Length);

	
	
	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
						bcb, cbwlen, NULL);
	
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	if (unlikely(us->fflags & US_FL_GO_SLOW))
		udelay(125);

	
	if (transfer_length) {
		unsigned int pipe;
		if (srb->sc_data_direction == DMA_FROM_DEVICE)
			pipe = us->recv_bulk_pipe;
		else
			pipe = us->send_bulk_pipe;

		result = usb_stor_bulk_srb(us, pipe, srb);
		
		if (result == USB_STOR_XFER_ERROR)
			return USB_STOR_TRANSPORT_ERROR;

		if (result == USB_STOR_XFER_LONG)
			fake_sense = 1;
	}

	
	
	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe, bcs,
						US_BULK_CS_WRAP_LEN, &cswlen);

	if (result == USB_STOR_XFER_SHORT && cswlen == 0) {
		
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe, bcs,
						US_BULK_CS_WRAP_LEN, &cswlen);
	}

	
	if (result == USB_STOR_XFER_STALLED) {
		
		
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe, bcs,
						US_BULK_CS_WRAP_LEN, NULL);
	}

	
	
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	
	residue = le32_to_cpu(bcs->Residue);
	if (!(bcs->Tag == us->tag ||
		(us->fflags & US_FL_BULK_IGNORE_TAG)) ||
		bcs->Status > US_BULK_STAT_PHASE) {
		
		return USB_STOR_TRANSPORT_ERROR;
	}

	if (!us->bcs_signature) {
		us->bcs_signature = bcs->Signature;
		
	} else if (bcs->Signature != us->bcs_signature) {
		return USB_STOR_TRANSPORT_ERROR;
	}

	if (residue && !(us->fflags & US_FL_IGNORE_RESIDUE)) {

		if (bcs->Status == US_BULK_STAT_OK &&
				scsi_get_resid(srb) == 0 &&
					((srb->cmnd[0] == INQUIRY &&
						transfer_length == 36) ||
					(srb->cmnd[0] == READ_CAPACITY &&
						transfer_length == 8))) {
			us->fflags |= US_FL_IGNORE_RESIDUE;

		} else {
			residue = min(residue, transfer_length);
			scsi_set_resid(srb, max(scsi_get_resid(srb),
							(int) residue));
		}
	}

	
	switch (bcs->Status) {
	case US_BULK_STAT_OK:
		if (fake_sense) {
			memcpy(srb->sense_buffer, usb_stor_sense_invalidCDB,
					sizeof(usb_stor_sense_invalidCDB));
			return USB_STOR_TRANSPORT_NO_SENSE;
		}
		return USB_STOR_TRANSPORT_GOOD;

	case US_BULK_STAT_FAIL:
		return USB_STOR_TRANSPORT_FAILED;

	case US_BULK_STAT_PHASE:
		return USB_STOR_TRANSPORT_ERROR;
	}
	return USB_STOR_TRANSPORT_ERROR;
}

static int usb_stor_reset_common(struct us_data *us,
		u8 request, u8 requesttype,
		u16 value, u16 index, void *data, u16 size)
{
	int result;
	int result2;

	
	if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
		
		return -EIO;
	}

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			request, requesttype, value, index, data, size,	5*HZ);

	if (result < 0) {
		
		return result;
	}

	wait_event_interruptible_timeout(us->delay_wait,
			test_bit(US_FLIDX_DISCONNECTING, &us->dflags),	HZ*6);

	if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
		
		return -EIO;
	}

	
	result = usb_stor_clear_halt(us, us->recv_bulk_pipe);

	
	result2 = usb_stor_clear_halt(us, us->send_bulk_pipe);

	
	if (result >= 0)
		result = result2;
	
		
	
		
	return result;
}

int usb_stor_Bulk_reset(struct us_data *us)
{
	
	return usb_stor_reset_common(us, US_BULK_RESET_REQUEST,
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, NULL, 0);
}

int usb_stor_port_reset(struct us_data *us)
{
	int result;

	
	result = usb_lock_device_for_reset(us->pusb_dev, us->pusb_intf);
	if (result < 0)
		pr_info("unable to lock device for reset: %d\n", result);
	else {
		
		if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
			result = -EIO;
			
		} else {
			result = usb_reset_device(us->pusb_dev);
		}
		usb_unlock_device(us->pusb_dev);
	}
	return result;
}


