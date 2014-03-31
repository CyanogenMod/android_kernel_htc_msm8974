/*
 *  linux/drivers/message/fusion/mptctl.c
 *      mpt Ioctl driver.
 *      For use with LSI PCI chip/adapters
 *      running LSI Fusion MPT (Message Passing Technology) firmware.
 *
 *  Copyright (c) 1999-2008 LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 */
/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    NO WARRANTY
    THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
    LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
    solely responsible for determining the appropriateness of using and
    distributing the Program and assumes all risks associated with its
    exercise of rights under this Agreement, including but not limited to
    the risks and costs of program errors, damage to or loss of data,
    programs or equipment, and unavailability or interruption of operations.

    DISCLAIMER OF LIABILITY
    NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
    HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/delay.h>	
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/compat.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>

#define COPYRIGHT	"Copyright (c) 1999-2008 LSI Corporation"
#define MODULEAUTHOR	"LSI Corporation"
#include "mptbase.h"
#include "mptctl.h"

#define my_NAME		"Fusion MPT misc device (ioctl) driver"
#define my_VERSION	MPT_LINUX_VERSION_COMMON
#define MYNAM		"mptctl"

MODULE_AUTHOR(MODULEAUTHOR);
MODULE_DESCRIPTION(my_NAME);
MODULE_LICENSE("GPL");
MODULE_VERSION(my_VERSION);


static DEFINE_MUTEX(mpctl_mutex);
static u8 mptctl_id = MPT_MAX_PROTOCOL_DRIVERS;
static u8 mptctl_taskmgmt_id = MPT_MAX_PROTOCOL_DRIVERS;

static DECLARE_WAIT_QUEUE_HEAD ( mptctl_wait );


struct buflist {
	u8	*kptr;
	int	 len;
};

static int mptctl_fw_download(unsigned long arg);
static int mptctl_getiocinfo(unsigned long arg, unsigned int cmd);
static int mptctl_gettargetinfo(unsigned long arg);
static int mptctl_readtest(unsigned long arg);
static int mptctl_mpt_command(unsigned long arg);
static int mptctl_eventquery(unsigned long arg);
static int mptctl_eventenable(unsigned long arg);
static int mptctl_eventreport(unsigned long arg);
static int mptctl_replace_fw(unsigned long arg);

static int mptctl_do_reset(unsigned long arg);
static int mptctl_hp_hostinfo(unsigned long arg, unsigned int cmd);
static int mptctl_hp_targetinfo(unsigned long arg);

static int  mptctl_probe(struct pci_dev *, const struct pci_device_id *);
static void mptctl_remove(struct pci_dev *);

#ifdef CONFIG_COMPAT
static long compat_mpctl_ioctl(struct file *f, unsigned cmd, unsigned long arg);
#endif
static int mptctl_do_mpt_command(struct mpt_ioctl_command karg, void __user *mfPtr);
static int mptctl_do_fw_download(int ioc, char __user *ufwbuf, size_t fwlen);
static MptSge_t *kbuf_alloc_2_sgl(int bytes, u32 dir, int sge_offset, int *frags,
		struct buflist **blp, dma_addr_t *sglbuf_dma, MPT_ADAPTER *ioc);
static void kfree_sgl(MptSge_t *sgl, dma_addr_t sgl_dma,
		struct buflist *buflist, MPT_ADAPTER *ioc);

static int  mptctl_ioc_reset(MPT_ADAPTER *ioc, int reset_phase);

static int mptctl_event_process(MPT_ADAPTER *ioc, EventNotificationReply_t *pEvReply);
static struct fasync_struct *async_queue=NULL;

#define MAX_FRAGS_SPILL1	9
#define MAX_FRAGS_SPILL2	15
#define FRAGS_PER_BUCKET	(MAX_FRAGS_SPILL2 + 1)

#define MAX_CHAIN_FRAGS		(4 * MAX_FRAGS_SPILL2 + 1)

#define MAX_SGL_BYTES		((MAX_FRAGS_SPILL1 + 1 + (4 * FRAGS_PER_BUCKET)) * 8)

#define MAX_KMALLOC_SZ		(128*1024)

#define MPT_IOCTL_DEFAULT_TIMEOUT 10	

static inline int
mptctl_syscall_down(MPT_ADAPTER *ioc, int nonblock)
{
	int rc = 0;

	if (nonblock) {
		if (!mutex_trylock(&ioc->ioctl_cmds.mutex))
			rc = -EAGAIN;
	} else {
		if (mutex_lock_interruptible(&ioc->ioctl_cmds.mutex))
			rc = -ERESTARTSYS;
	}
	return rc;
}

static int
mptctl_reply(MPT_ADAPTER *ioc, MPT_FRAME_HDR *req, MPT_FRAME_HDR *reply)
{
	char	*sense_data;
	int	req_index;
	int	sz;

	if (!req)
		return 0;

	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "completing mpi function "
	    "(0x%02X), req=%p, reply=%p\n", ioc->name,  req->u.hdr.Function,
	    req, reply));

	if (ioc->ioctl_cmds.msg_context != req->u.hdr.MsgContext)
		goto out_continuation;

	ioc->ioctl_cmds.status |= MPT_MGMT_STATUS_COMMAND_GOOD;

	if (!reply)
		goto out;

	ioc->ioctl_cmds.status |= MPT_MGMT_STATUS_RF_VALID;
	sz = min(ioc->reply_sz, 4*reply->u.reply.MsgLength);
	memcpy(ioc->ioctl_cmds.reply, reply, sz);

	if (reply->u.reply.IOCStatus || reply->u.reply.IOCLogInfo)
		dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "iocstatus (0x%04X), loginfo (0x%08X)\n", ioc->name,
		    le16_to_cpu(reply->u.reply.IOCStatus),
		    le32_to_cpu(reply->u.reply.IOCLogInfo)));

	if ((req->u.hdr.Function == MPI_FUNCTION_SCSI_IO_REQUEST) ||
		(req->u.hdr.Function ==
		 MPI_FUNCTION_RAID_SCSI_IO_PASSTHROUGH)) {

		if (reply->u.sreply.SCSIStatus || reply->u.sreply.SCSIState)
			dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			"scsi_status (0x%02x), scsi_state (0x%02x), "
			"tag = (0x%04x), transfer_count (0x%08x)\n", ioc->name,
			reply->u.sreply.SCSIStatus,
			reply->u.sreply.SCSIState,
			le16_to_cpu(reply->u.sreply.TaskTag),
			le32_to_cpu(reply->u.sreply.TransferCount)));

		if (reply->u.sreply.SCSIState &
			MPI_SCSI_STATE_AUTOSENSE_VALID) {
			sz = req->u.scsireq.SenseBufferLength;
			req_index =
			    le16_to_cpu(req->u.frame.hwhdr.msgctxu.fld.req_idx);
			sense_data = ((u8 *)ioc->sense_buf_pool +
			     (req_index * MPT_SENSE_BUFFER_ALLOC));
			memcpy(ioc->ioctl_cmds.sense, sense_data, sz);
			ioc->ioctl_cmds.status |= MPT_MGMT_STATUS_SENSE_VALID;
		}
	}

 out:
	if (ioc->ioctl_cmds.status & MPT_MGMT_STATUS_PENDING) {
		if (req->u.hdr.Function == MPI_FUNCTION_SCSI_TASK_MGMT) {
			mpt_clear_taskmgmt_in_progress_flag(ioc);
			ioc->ioctl_cmds.status &= ~MPT_MGMT_STATUS_PENDING;
			complete(&ioc->ioctl_cmds.done);
			if (ioc->bus_type == SAS)
				ioc->schedule_target_reset(ioc);
		} else {
			ioc->ioctl_cmds.status &= ~MPT_MGMT_STATUS_PENDING;
			complete(&ioc->ioctl_cmds.done);
		}
	}

 out_continuation:
	if (reply && (reply->u.reply.MsgFlags &
	    MPI_MSGFLAGS_CONTINUATION_REPLY))
		return 0;
	return 1;
}


static int
mptctl_taskmgmt_reply(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf, MPT_FRAME_HDR *mr)
{
	if (!mf)
		return 0;

	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		"TaskMgmt completed (mf=%p, mr=%p)\n",
		ioc->name, mf, mr));

	ioc->taskmgmt_cmds.status |= MPT_MGMT_STATUS_COMMAND_GOOD;

	if (!mr)
		goto out;

	ioc->taskmgmt_cmds.status |= MPT_MGMT_STATUS_RF_VALID;
	memcpy(ioc->taskmgmt_cmds.reply, mr,
	    min(MPT_DEFAULT_FRAME_SIZE, 4 * mr->u.reply.MsgLength));
 out:
	if (ioc->taskmgmt_cmds.status & MPT_MGMT_STATUS_PENDING) {
		mpt_clear_taskmgmt_in_progress_flag(ioc);
		ioc->taskmgmt_cmds.status &= ~MPT_MGMT_STATUS_PENDING;
		complete(&ioc->taskmgmt_cmds.done);
		if (ioc->bus_type == SAS)
			ioc->schedule_target_reset(ioc);
		return 1;
	}
	return 0;
}

static int
mptctl_do_taskmgmt(MPT_ADAPTER *ioc, u8 tm_type, u8 bus_id, u8 target_id)
{
	MPT_FRAME_HDR	*mf;
	SCSITaskMgmt_t	*pScsiTm;
	SCSITaskMgmtReply_t *pScsiTmReply;
	int		 ii;
	int		 retval;
	unsigned long	 timeout;
	unsigned long	 time_count;
	u16		 iocstatus;


	mutex_lock(&ioc->taskmgmt_cmds.mutex);
	if (mpt_set_taskmgmt_in_progress_flag(ioc) != 0) {
		mutex_unlock(&ioc->taskmgmt_cmds.mutex);
		return -EPERM;
	}

	retval = 0;

	mf = mpt_get_msg_frame(mptctl_taskmgmt_id, ioc);
	if (mf == NULL) {
		dtmprintk(ioc,
			printk(MYIOC_s_WARN_FMT "TaskMgmt, no msg frames!!\n",
			ioc->name));
		mpt_clear_taskmgmt_in_progress_flag(ioc);
		retval = -ENOMEM;
		goto tm_done;
	}

	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT "TaskMgmt request (mf=%p)\n",
		ioc->name, mf));

	pScsiTm = (SCSITaskMgmt_t *) mf;
	memset(pScsiTm, 0, sizeof(SCSITaskMgmt_t));
	pScsiTm->Function = MPI_FUNCTION_SCSI_TASK_MGMT;
	pScsiTm->TaskType = tm_type;
	if ((tm_type == MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS) &&
		(ioc->bus_type == FC))
		pScsiTm->MsgFlags =
				MPI_SCSITASKMGMT_MSGFLAGS_LIPRESET_RESET_OPTION;
	pScsiTm->TargetID = target_id;
	pScsiTm->Bus = bus_id;
	pScsiTm->ChainOffset = 0;
	pScsiTm->Reserved = 0;
	pScsiTm->Reserved1 = 0;
	pScsiTm->TaskMsgContext = 0;
	for (ii= 0; ii < 8; ii++)
		pScsiTm->LUN[ii] = 0;
	for (ii=0; ii < 7; ii++)
		pScsiTm->Reserved2[ii] = 0;

	switch (ioc->bus_type) {
	case FC:
		timeout = 40;
		break;
	case SAS:
		timeout = 30;
		break;
	case SPI:
		default:
		timeout = 10;
		break;
	}

	dtmprintk(ioc,
		printk(MYIOC_s_DEBUG_FMT "TaskMgmt type=%d timeout=%ld\n",
		ioc->name, tm_type, timeout));

	INITIALIZE_MGMT_STATUS(ioc->taskmgmt_cmds.status)
	time_count = jiffies;
	if ((ioc->facts.IOCCapabilities & MPI_IOCFACTS_CAPABILITY_HIGH_PRI_Q) &&
	    (ioc->facts.MsgVersion >= MPI_VERSION_01_05))
		mpt_put_msg_frame_hi_pri(mptctl_taskmgmt_id, ioc, mf);
	else {
		retval = mpt_send_handshake_request(mptctl_taskmgmt_id, ioc,
		    sizeof(SCSITaskMgmt_t), (u32 *)pScsiTm, CAN_SLEEP);
		if (retval != 0) {
			dfailprintk(ioc,
				printk(MYIOC_s_ERR_FMT
				"TaskMgmt send_handshake FAILED!"
				" (ioc %p, mf %p, rc=%d) \n", ioc->name,
				ioc, mf, retval));
			mpt_free_msg_frame(ioc, mf);
			mpt_clear_taskmgmt_in_progress_flag(ioc);
			goto tm_done;
		}
	}

	
	ii = wait_for_completion_timeout(&ioc->taskmgmt_cmds.done, timeout*HZ);

	if (!(ioc->taskmgmt_cmds.status & MPT_MGMT_STATUS_COMMAND_GOOD)) {
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "TaskMgmt failed\n", ioc->name));
		mpt_free_msg_frame(ioc, mf);
		mpt_clear_taskmgmt_in_progress_flag(ioc);
		if (ioc->taskmgmt_cmds.status & MPT_MGMT_STATUS_DID_IOCRESET)
			retval = 0;
		else
			retval = -1; 
		goto tm_done;
	}

	if (!(ioc->taskmgmt_cmds.status & MPT_MGMT_STATUS_RF_VALID)) {
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "TaskMgmt failed\n", ioc->name));
		retval = -1; 
		goto tm_done;
	}

	pScsiTmReply = (SCSITaskMgmtReply_t *) ioc->taskmgmt_cmds.reply;
	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "TaskMgmt fw_channel = %d, fw_id = %d, task_type=0x%02X, "
	    "iocstatus=0x%04X\n\tloginfo=0x%08X, response_code=0x%02X, "
	    "term_cmnds=%d\n", ioc->name, pScsiTmReply->Bus,
	    pScsiTmReply->TargetID, tm_type,
	    le16_to_cpu(pScsiTmReply->IOCStatus),
	    le32_to_cpu(pScsiTmReply->IOCLogInfo),
	    pScsiTmReply->ResponseCode,
	    le32_to_cpu(pScsiTmReply->TerminationCount)));

	iocstatus = le16_to_cpu(pScsiTmReply->IOCStatus) & MPI_IOCSTATUS_MASK;

	if (iocstatus == MPI_IOCSTATUS_SCSI_TASK_TERMINATED ||
	   iocstatus == MPI_IOCSTATUS_SCSI_IOC_TERMINATED ||
	   iocstatus == MPI_IOCSTATUS_SUCCESS)
		retval = 0;
	else {
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "TaskMgmt failed\n", ioc->name));
		retval = -1; 
	}

 tm_done:
	mutex_unlock(&ioc->taskmgmt_cmds.mutex);
	CLEAR_MGMT_STATUS(ioc->taskmgmt_cmds.status)
	return retval;
}

static void
mptctl_timeout_expired(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf)
{
	unsigned long flags;
	int ret_val = -1;
	SCSIIORequest_t *scsi_req = (SCSIIORequest_t *) mf;
	u8 function = mf->u.hdr.Function;

	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT ": %s\n",
		ioc->name, __func__));

	if (mpt_fwfault_debug)
		mpt_halt_firmware(ioc);

	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	if (ioc->ioc_reset_in_progress) {
		spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
		CLEAR_MGMT_PENDING_STATUS(ioc->ioctl_cmds.status)
		mpt_free_msg_frame(ioc, mf);
		return;
	}
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);


	CLEAR_MGMT_PENDING_STATUS(ioc->ioctl_cmds.status)

	if (ioc->bus_type == SAS) {
		if (function == MPI_FUNCTION_SCSI_IO_REQUEST)
			ret_val = mptctl_do_taskmgmt(ioc,
				MPI_SCSITASKMGMT_TASKTYPE_TARGET_RESET,
				scsi_req->Bus, scsi_req->TargetID);
		else if (function == MPI_FUNCTION_RAID_SCSI_IO_PASSTHROUGH)
			ret_val = mptctl_do_taskmgmt(ioc,
				MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS,
				scsi_req->Bus, 0);
		if (!ret_val)
			return;
	} else {
		if ((function == MPI_FUNCTION_SCSI_IO_REQUEST) ||
			(function == MPI_FUNCTION_RAID_SCSI_IO_PASSTHROUGH))
			ret_val = mptctl_do_taskmgmt(ioc,
				MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS,
				scsi_req->Bus, 0);
		if (!ret_val)
			return;
	}

	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Calling Reset! \n",
		 ioc->name));
	mpt_Soft_Hard_ResetHandler(ioc, CAN_SLEEP);
	mpt_free_msg_frame(ioc, mf);
}


static int
mptctl_ioc_reset(MPT_ADAPTER *ioc, int reset_phase)
{
	switch(reset_phase) {
	case MPT_IOC_SETUP_RESET:
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: MPT_IOC_SETUP_RESET\n", ioc->name, __func__));
		break;
	case MPT_IOC_PRE_RESET:
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: MPT_IOC_PRE_RESET\n", ioc->name, __func__));
		break;
	case MPT_IOC_POST_RESET:
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: MPT_IOC_POST_RESET\n", ioc->name, __func__));
		if (ioc->ioctl_cmds.status & MPT_MGMT_STATUS_PENDING) {
			ioc->ioctl_cmds.status |= MPT_MGMT_STATUS_DID_IOCRESET;
			complete(&ioc->ioctl_cmds.done);
		}
		break;
	default:
		break;
	}

	return 1;
}

static int
mptctl_event_process(MPT_ADAPTER *ioc, EventNotificationReply_t *pEvReply)
{
	u8 event;

	event = le32_to_cpu(pEvReply->Event) & 0xFF;

	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "%s() called\n",
	    ioc->name, __func__));
	if(async_queue == NULL)
		return 1;

	 if (event == 0x21 ) {
		ioc->aen_event_read_flag=1;
		dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Raised SIGIO to application\n",
		    ioc->name));
		devtverboseprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "Raised SIGIO to application\n", ioc->name));
		kill_fasync(&async_queue, SIGIO, POLL_IN);
		return 1;
	 }

	if(ioc->aen_event_read_flag)
		return 1;

	if (ioc->events && (ioc->eventTypes & ( 1 << event))) {
		ioc->aen_event_read_flag=1;
		dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "Raised SIGIO to application\n", ioc->name));
		devtverboseprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "Raised SIGIO to application\n", ioc->name));
		kill_fasync(&async_queue, SIGIO, POLL_IN);
	}
	return 1;
}

static int
mptctl_release(struct inode *inode, struct file *filep)
{
	fasync_helper(-1, filep, 0, &async_queue);
	return 0;
}

static int
mptctl_fasync(int fd, struct file *filep, int mode)
{
	MPT_ADAPTER	*ioc;
	int ret;

	mutex_lock(&mpctl_mutex);
	list_for_each_entry(ioc, &ioc_list, list)
		ioc->aen_event_read_flag=0;

	ret = fasync_helper(fd, filep, mode, &async_queue);
	mutex_unlock(&mpctl_mutex);
	return ret;
}

static long
__mptctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	mpt_ioctl_header __user *uhdr = (void __user *) arg;
	mpt_ioctl_header	 khdr;
	int iocnum;
	unsigned iocnumX;
	int nonblock = (file->f_flags & O_NONBLOCK);
	int ret;
	MPT_ADAPTER *iocp = NULL;

	if (copy_from_user(&khdr, uhdr, sizeof(khdr))) {
		printk(KERN_ERR MYNAM "%s::mptctl_ioctl() @%d - "
				"Unable to copy mpt_ioctl_header data @ %p\n",
				__FILE__, __LINE__, uhdr);
		return -EFAULT;
	}
	ret = -ENXIO;				

	iocnumX = khdr.iocnum & 0xFF;
	if (((iocnum = mpt_verify_adapter(iocnumX, &iocp)) < 0) ||
	    (iocp == NULL))
		return -ENODEV;

	if (!iocp->active) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_ioctl() @%d - Controller disabled.\n",
				__FILE__, __LINE__);
		return -EFAULT;
	}

	if ((cmd & ~IOCSIZE_MASK) == (MPTIOCINFO & ~IOCSIZE_MASK)) {
		return mptctl_getiocinfo(arg, _IOC_SIZE(cmd));
	} else if (cmd == MPTTARGETINFO) {
		return mptctl_gettargetinfo(arg);
	} else if (cmd == MPTTEST) {
		return mptctl_readtest(arg);
	} else if (cmd == MPTEVENTQUERY) {
		return mptctl_eventquery(arg);
	} else if (cmd == MPTEVENTENABLE) {
		return mptctl_eventenable(arg);
	} else if (cmd == MPTEVENTREPORT) {
		return mptctl_eventreport(arg);
	} else if (cmd == MPTFWREPLACE) {
		return mptctl_replace_fw(arg);
	}

	if ((ret = mptctl_syscall_down(iocp, nonblock)) != 0)
		return ret;

	if (cmd == MPTFWDOWNLOAD)
		ret = mptctl_fw_download(arg);
	else if (cmd == MPTCOMMAND)
		ret = mptctl_mpt_command(arg);
	else if (cmd == MPTHARDRESET)
		ret = mptctl_do_reset(arg);
	else if ((cmd & ~IOCSIZE_MASK) == (HP_GETHOSTINFO & ~IOCSIZE_MASK))
		ret = mptctl_hp_hostinfo(arg, _IOC_SIZE(cmd));
	else if (cmd == HP_GETTARGETINFO)
		ret = mptctl_hp_targetinfo(arg);
	else
		ret = -EINVAL;

	mutex_unlock(&iocp->ioctl_cmds.mutex);

	return ret;
}

static long
mptctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;
	mutex_lock(&mpctl_mutex);
	ret = __mptctl_ioctl(file, cmd, arg);
	mutex_unlock(&mpctl_mutex);
	return ret;
}

static int mptctl_do_reset(unsigned long arg)
{
	struct mpt_ioctl_diag_reset __user *urinfo = (void __user *) arg;
	struct mpt_ioctl_diag_reset krinfo;
	MPT_ADAPTER		*iocp;

	if (copy_from_user(&krinfo, urinfo, sizeof(struct mpt_ioctl_diag_reset))) {
		printk(KERN_ERR MYNAM "%s@%d::mptctl_do_reset - "
				"Unable to copy mpt_ioctl_diag_reset struct @ %p\n",
				__FILE__, __LINE__, urinfo);
		return -EFAULT;
	}

	if (mpt_verify_adapter(krinfo.hdr.iocnum, &iocp) < 0) {
		printk(KERN_DEBUG MYNAM "%s@%d::mptctl_do_reset - ioc%d not found!\n",
				__FILE__, __LINE__, krinfo.hdr.iocnum);
		return -ENODEV; 
	}

	dctlprintk(iocp, printk(MYIOC_s_DEBUG_FMT "mptctl_do_reset called.\n",
	    iocp->name));

	if (mpt_HardResetHandler(iocp, CAN_SLEEP) != 0) {
		printk (MYIOC_s_ERR_FMT "%s@%d::mptctl_do_reset - reset failed.\n",
			iocp->name, __FILE__, __LINE__);
		return -1;
	}

	return 0;
}

static int
mptctl_fw_download(unsigned long arg)
{
	struct mpt_fw_xfer __user *ufwdl = (void __user *) arg;
	struct mpt_fw_xfer	 kfwdl;

	if (copy_from_user(&kfwdl, ufwdl, sizeof(struct mpt_fw_xfer))) {
		printk(KERN_ERR MYNAM "%s@%d::_ioctl_fwdl - "
				"Unable to copy mpt_fw_xfer struct @ %p\n",
				__FILE__, __LINE__, ufwdl);
		return -EFAULT;
	}

	return mptctl_do_fw_download(kfwdl.iocnum, kfwdl.bufp, kfwdl.fwlen);
}

static int
mptctl_do_fw_download(int ioc, char __user *ufwbuf, size_t fwlen)
{
	FWDownload_t		*dlmsg;
	MPT_FRAME_HDR		*mf;
	MPT_ADAPTER		*iocp;
	FWDownloadTCSGE_t	*ptsge;
	MptSge_t		*sgl, *sgIn;
	char			*sgOut;
	struct buflist		*buflist;
	struct buflist		*bl;
	dma_addr_t		 sgl_dma;
	int			 ret;
	int			 numfrags = 0;
	int			 maxfrags;
	int			 n = 0;
	u32			 sgdir;
	u32			 nib;
	int			 fw_bytes_copied = 0;
	int			 i;
	int			 sge_offset = 0;
	u16			 iocstat;
	pFWDownloadReply_t	 ReplyMsg = NULL;
	unsigned long		 timeleft;

	if (mpt_verify_adapter(ioc, &iocp) < 0) {
		printk(KERN_DEBUG MYNAM "ioctl_fwdl - ioc%d not found!\n",
				 ioc);
		return -ENODEV; 
	} else {

		if ((mf = mpt_get_msg_frame(mptctl_id, iocp)) == NULL)
			return -EAGAIN;
	}

	dctlprintk(iocp, printk(MYIOC_s_DEBUG_FMT
	    "mptctl_do_fwdl called. mptctl_id = %xh.\n", iocp->name, mptctl_id));
	dctlprintk(iocp, printk(MYIOC_s_DEBUG_FMT "DbG: kfwdl.bufp  = %p\n",
	    iocp->name, ufwbuf));
	dctlprintk(iocp, printk(MYIOC_s_DEBUG_FMT "DbG: kfwdl.fwlen = %d\n",
	    iocp->name, (int)fwlen));
	dctlprintk(iocp, printk(MYIOC_s_DEBUG_FMT "DbG: kfwdl.ioc   = %04xh\n",
	    iocp->name, ioc));

	dlmsg = (FWDownload_t*) mf;
	ptsge = (FWDownloadTCSGE_t *) &dlmsg->SGL;
	sgOut = (char *) (ptsge + 1);

	dlmsg->ImageType = MPI_FW_DOWNLOAD_ITYPE_FW;
	dlmsg->Reserved = 0;
	dlmsg->ChainOffset = 0;
	dlmsg->Function = MPI_FUNCTION_FW_DOWNLOAD;
	dlmsg->Reserved1[0] = dlmsg->Reserved1[1] = dlmsg->Reserved1[2] = 0;
	if (iocp->facts.MsgVersion >= MPI_VERSION_01_05)
		dlmsg->MsgFlags = MPI_FW_DOWNLOAD_MSGFLGS_LAST_SEGMENT;
	else
		dlmsg->MsgFlags = 0;


	ptsge->Reserved = 0;
	ptsge->ContextSize = 0;
	ptsge->DetailsLength = 12;
	ptsge->Flags = MPI_SGE_FLAGS_TRANSACTION_ELEMENT;
	ptsge->Reserved_0100_Checksum = 0;
	ptsge->ImageOffset = 0;
	ptsge->ImageSize = cpu_to_le32(fwlen);


	sgdir = 0x04000000;		
	sge_offset = sizeof(MPIHeader_t) + sizeof(FWDownloadTCSGE_t);
	if ((sgl = kbuf_alloc_2_sgl(fwlen, sgdir, sge_offset,
				    &numfrags, &buflist, &sgl_dma, iocp)) == NULL)
		return -ENOMEM;

	maxfrags = (iocp->req_sz - sizeof(MPIHeader_t) -
			sizeof(FWDownloadTCSGE_t))
			/ iocp->SGE_size;
	if (numfrags > maxfrags) {
		ret = -EMLINK;
		goto fwdl_out;
	}

	dctlprintk(iocp, printk(MYIOC_s_DEBUG_FMT "DbG: sgl buffer = %p, sgfrags = %d\n",
	    iocp->name, sgl, numfrags));

	ret = -EFAULT;
	sgIn = sgl;
	bl = buflist;
	for (i=0; i < numfrags; i++) {

		nib = (sgIn->FlagsLength & 0x30000000) >> 28;
		if (nib == 0 || nib == 3) {
			;
		} else if (sgIn->Address) {
			iocp->add_sge(sgOut, sgIn->FlagsLength, sgIn->Address);
			n++;
			if (copy_from_user(bl->kptr, ufwbuf+fw_bytes_copied, bl->len)) {
				printk(MYIOC_s_ERR_FMT "%s@%d::_ioctl_fwdl - "
					"Unable to copy f/w buffer hunk#%d @ %p\n",
					iocp->name, __FILE__, __LINE__, n, ufwbuf);
				goto fwdl_out;
			}
			fw_bytes_copied += bl->len;
		}
		sgIn++;
		bl++;
		sgOut += iocp->SGE_size;
	}

	DBG_DUMP_FW_DOWNLOAD(iocp, (u32 *)mf, numfrags);

	ReplyMsg = NULL;
	SET_MGMT_MSG_CONTEXT(iocp->ioctl_cmds.msg_context, dlmsg->MsgContext);
	INITIALIZE_MGMT_STATUS(iocp->ioctl_cmds.status)
	mpt_put_msg_frame(mptctl_id, iocp, mf);

	
retry_wait:
	timeleft = wait_for_completion_timeout(&iocp->ioctl_cmds.done, HZ*60);
	if (!(iocp->ioctl_cmds.status & MPT_MGMT_STATUS_COMMAND_GOOD)) {
		ret = -ETIME;
		printk(MYIOC_s_WARN_FMT "%s: failed\n", iocp->name, __func__);
		if (iocp->ioctl_cmds.status & MPT_MGMT_STATUS_DID_IOCRESET) {
			mpt_free_msg_frame(iocp, mf);
			goto fwdl_out;
		}
		if (!timeleft) {
			printk(MYIOC_s_WARN_FMT
			       "FW download timeout, doorbell=0x%08x\n",
			       iocp->name, mpt_GetIocState(iocp, 0));
			mptctl_timeout_expired(iocp, mf);
		} else
			goto retry_wait;
		goto fwdl_out;
	}

	if (!(iocp->ioctl_cmds.status & MPT_MGMT_STATUS_RF_VALID)) {
		printk(MYIOC_s_WARN_FMT "%s: failed\n", iocp->name, __func__);
		mpt_free_msg_frame(iocp, mf);
		ret = -ENODATA;
		goto fwdl_out;
	}

	if (sgl)
		kfree_sgl(sgl, sgl_dma, buflist, iocp);

	ReplyMsg = (pFWDownloadReply_t)iocp->ioctl_cmds.reply;
	iocstat = le16_to_cpu(ReplyMsg->IOCStatus) & MPI_IOCSTATUS_MASK;
	if (iocstat == MPI_IOCSTATUS_SUCCESS) {
		printk(MYIOC_s_INFO_FMT "F/W update successful!\n", iocp->name);
		return 0;
	} else if (iocstat == MPI_IOCSTATUS_INVALID_FUNCTION) {
		printk(MYIOC_s_WARN_FMT "Hmmm...  F/W download not supported!?!\n",
			iocp->name);
		printk(MYIOC_s_WARN_FMT "(time to go bang on somebodies door)\n",
			iocp->name);
		return -EBADRQC;
	} else if (iocstat == MPI_IOCSTATUS_BUSY) {
		printk(MYIOC_s_WARN_FMT "IOC_BUSY!\n", iocp->name);
		printk(MYIOC_s_WARN_FMT "(try again later?)\n", iocp->name);
		return -EBUSY;
	} else {
		printk(MYIOC_s_WARN_FMT "ioctl_fwdl() returned [bad] status = %04xh\n",
			iocp->name, iocstat);
		printk(MYIOC_s_WARN_FMT "(bad VooDoo)\n", iocp->name);
		return -ENOMSG;
	}
	return 0;

fwdl_out:

	CLEAR_MGMT_STATUS(iocp->ioctl_cmds.status);
	SET_MGMT_MSG_CONTEXT(iocp->ioctl_cmds.msg_context, 0);
        kfree_sgl(sgl, sgl_dma, buflist, iocp);
	return ret;
}

static MptSge_t *
kbuf_alloc_2_sgl(int bytes, u32 sgdir, int sge_offset, int *frags,
		 struct buflist **blp, dma_addr_t *sglbuf_dma, MPT_ADAPTER *ioc)
{
	MptSge_t	*sglbuf = NULL;		
						
	struct buflist	*buflist = NULL;	
	MptSge_t	*sgl;
	int		 numfrags = 0;
	int		 fragcnt = 0;
	int		 alloc_sz = min(bytes,MAX_KMALLOC_SZ);	
	int		 bytes_allocd = 0;
	int		 this_alloc;
	dma_addr_t	 pa;					
	int		 i, buflist_ent;
	int		 sg_spill = MAX_FRAGS_SPILL1;
	int		 dir;
	
	*frags = 0;
	*blp = NULL;

	i = MAX_SGL_BYTES / 8;
	buflist = kzalloc(i, GFP_USER);
	if (!buflist)
		return NULL;
	buflist_ent = 0;

	sglbuf = pci_alloc_consistent(ioc->pcidev, MAX_SGL_BYTES, sglbuf_dma);
	if (sglbuf == NULL)
		goto free_and_fail;

	if (sgdir & 0x04000000)
		dir = PCI_DMA_TODEVICE;
	else
		dir = PCI_DMA_FROMDEVICE;

	/* At start:
	 *	sgl = sglbuf = point to beginning of sg buffer
	 *	buflist_ent = 0 = first kernel structure
	 *	sg_spill = number of SGE that can be written before the first
	 *		chain element.
	 *
	 */
	sgl = sglbuf;
	sg_spill = ((ioc->req_sz - sge_offset)/ioc->SGE_size) - 1;
	while (bytes_allocd < bytes) {
		this_alloc = min(alloc_sz, bytes-bytes_allocd);
		buflist[buflist_ent].len = this_alloc;
		buflist[buflist_ent].kptr = pci_alloc_consistent(ioc->pcidev,
								 this_alloc,
								 &pa);
		if (buflist[buflist_ent].kptr == NULL) {
			alloc_sz = alloc_sz / 2;
			if (alloc_sz == 0) {
				printk(MYIOC_s_WARN_FMT "-SG: No can do - "
				    "not enough memory!   :-(\n", ioc->name);
				printk(MYIOC_s_WARN_FMT "-SG: (freeing %d frags)\n",
					ioc->name, numfrags);
				goto free_and_fail;
			}
			continue;
		} else {
			dma_addr_t dma_addr;

			bytes_allocd += this_alloc;
			sgl->FlagsLength = (0x10000000|sgdir|this_alloc);
			dma_addr = pci_map_single(ioc->pcidev,
				buflist[buflist_ent].kptr, this_alloc, dir);
			sgl->Address = dma_addr;

			fragcnt++;
			numfrags++;
			sgl++;
			buflist_ent++;
		}

		if (bytes_allocd >= bytes)
			break;

		
		if (fragcnt == sg_spill) {
			printk(MYIOC_s_WARN_FMT
			    "-SG: No can do - " "Chain required!   :-(\n", ioc->name);
			printk(MYIOC_s_WARN_FMT "(freeing %d frags)\n", ioc->name, numfrags);
			goto free_and_fail;
		}

		
		if (numfrags*8 > MAX_SGL_BYTES){
			
			printk(MYIOC_s_WARN_FMT "-SG: No can do - "
				"too many SG frags!   :-(\n", ioc->name);
			printk(MYIOC_s_WARN_FMT "-SG: (freeing %d frags)\n",
				ioc->name, numfrags);
			goto free_and_fail;
		}
	}

	
	sgl[-1].FlagsLength |= 0xC1000000;

	*frags = numfrags;
	*blp = buflist;

	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "-SG: kbuf_alloc_2_sgl() - "
	   "%d SG frags generated!\n", ioc->name, numfrags));

	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "-SG: kbuf_alloc_2_sgl() - "
	   "last (big) alloc_sz=%d\n", ioc->name, alloc_sz));

	return sglbuf;

free_and_fail:
	if (sglbuf != NULL) {
		for (i = 0; i < numfrags; i++) {
			dma_addr_t dma_addr;
			u8 *kptr;
			int len;

			if ((sglbuf[i].FlagsLength >> 24) == 0x30)
				continue;

			dma_addr = sglbuf[i].Address;
			kptr = buflist[i].kptr;
			len = buflist[i].len;

			pci_free_consistent(ioc->pcidev, len, kptr, dma_addr);
		}
		pci_free_consistent(ioc->pcidev, MAX_SGL_BYTES, sglbuf, *sglbuf_dma);
	}
	kfree(buflist);
	return NULL;
}

static void
kfree_sgl(MptSge_t *sgl, dma_addr_t sgl_dma, struct buflist *buflist, MPT_ADAPTER *ioc)
{
	MptSge_t	*sg = sgl;
	struct buflist	*bl = buflist;
	u32		 nib;
	int		 dir;
	int		 n = 0;

	if (sg->FlagsLength & 0x04000000)
		dir = PCI_DMA_TODEVICE;
	else
		dir = PCI_DMA_FROMDEVICE;

	nib = (sg->FlagsLength & 0xF0000000) >> 28;
	while (! (nib & 0x4)) { 
		
		if (nib == 0 || nib == 3) {
			;
		} else if (sg->Address) {
			dma_addr_t dma_addr;
			void *kptr;
			int len;

			dma_addr = sg->Address;
			kptr = bl->kptr;
			len = bl->len;
			pci_unmap_single(ioc->pcidev, dma_addr, len, dir);
			pci_free_consistent(ioc->pcidev, len, kptr, dma_addr);
			n++;
		}
		sg++;
		bl++;
		nib = (le32_to_cpu(sg->FlagsLength) & 0xF0000000) >> 28;
	}

	
	if (sg->Address) {
		dma_addr_t dma_addr;
		void *kptr;
		int len;

		dma_addr = sg->Address;
		kptr = bl->kptr;
		len = bl->len;
		pci_unmap_single(ioc->pcidev, dma_addr, len, dir);
		pci_free_consistent(ioc->pcidev, len, kptr, dma_addr);
		n++;
	}

	pci_free_consistent(ioc->pcidev, MAX_SGL_BYTES, sgl, sgl_dma);
	kfree(buflist);
	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "-SG: Free'd 1 SGL buf + %d kbufs!\n",
	    ioc->name, n));
}

static int
mptctl_getiocinfo (unsigned long arg, unsigned int data_size)
{
	struct mpt_ioctl_iocinfo __user *uarg = (void __user *) arg;
	struct mpt_ioctl_iocinfo *karg;
	MPT_ADAPTER		*ioc;
	struct pci_dev		*pdev;
	int			iocnum;
	unsigned int		port;
	int			cim_rev;
	u8			revision;
	struct scsi_device 	*sdev;
	VirtDevice		*vdevice;

	if (data_size == sizeof(struct mpt_ioctl_iocinfo_rev0))
		cim_rev = 0;
	else if (data_size == sizeof(struct mpt_ioctl_iocinfo_rev1))
		cim_rev = 1;
	else if (data_size == sizeof(struct mpt_ioctl_iocinfo))
		cim_rev = 2;
	else if (data_size == (sizeof(struct mpt_ioctl_iocinfo_rev0)+12))
		cim_rev = 0;	
	else
		return -EFAULT;

	karg = kmalloc(data_size, GFP_KERNEL);
	if (karg == NULL) {
		printk(KERN_ERR MYNAM "%s::mpt_ioctl_iocinfo() @%d - no memory available!\n",
				__FILE__, __LINE__);
		return -ENOMEM;
	}

	if (copy_from_user(karg, uarg, data_size)) {
		printk(KERN_ERR MYNAM "%s@%d::mptctl_getiocinfo - "
			"Unable to read in mpt_ioctl_iocinfo struct @ %p\n",
				__FILE__, __LINE__, uarg);
		kfree(karg);
		return -EFAULT;
	}

	if (((iocnum = mpt_verify_adapter(karg->hdr.iocnum, &ioc)) < 0) ||
	    (ioc == NULL)) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_getiocinfo() @%d - ioc%d not found!\n",
				__FILE__, __LINE__, iocnum);
		kfree(karg);
		return -ENODEV;
	}

	
	if (karg->hdr.maxDataSize != data_size) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_getiocinfo - "
			"Structure size mismatch. Command not completed.\n",
			ioc->name, __FILE__, __LINE__);
		kfree(karg);
		return -EFAULT;
	}

	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mptctl_getiocinfo called.\n",
	    ioc->name));

	if (ioc->bus_type == SAS)
		karg->adapterType = MPT_IOCTL_INTERFACE_SAS;
	else if (ioc->bus_type == FC)
		karg->adapterType = MPT_IOCTL_INTERFACE_FC;
	else
		karg->adapterType = MPT_IOCTL_INTERFACE_SCSI;

	if (karg->hdr.port > 1) {
		kfree(karg);
		return -EINVAL;
	}
	port = karg->hdr.port;

	karg->port = port;
	pdev = (struct pci_dev *) ioc->pcidev;

	karg->pciId = pdev->device;
	pci_read_config_byte(pdev, PCI_CLASS_REVISION, &revision);
	karg->hwRev = revision;
	karg->subSystemDevice = pdev->subsystem_device;
	karg->subSystemVendor = pdev->subsystem_vendor;

	if (cim_rev == 1) {
		karg->pciInfo.u.bits.busNumber = pdev->bus->number;
		karg->pciInfo.u.bits.deviceNumber = PCI_SLOT( pdev->devfn );
		karg->pciInfo.u.bits.functionNumber = PCI_FUNC( pdev->devfn );
	} else if (cim_rev == 2) {
		karg->pciInfo.u.bits.busNumber = pdev->bus->number;
		karg->pciInfo.u.bits.deviceNumber = PCI_SLOT( pdev->devfn );
		karg->pciInfo.u.bits.functionNumber = PCI_FUNC( pdev->devfn );
		karg->pciInfo.segmentID = pci_domain_nr(pdev->bus);
	}

	karg->numDevices = 0;
	if (ioc->sh) {
		shost_for_each_device(sdev, ioc->sh) {
			vdevice = sdev->hostdata;
			if (vdevice == NULL || vdevice->vtarget == NULL)
				continue;
			if (vdevice->vtarget->tflags &
			    MPT_TARGET_FLAGS_RAID_COMPONENT)
				continue;
			karg->numDevices++;
		}
	}

	karg->FWVersion = ioc->facts.FWVersion.Word;
	karg->BIOSVersion = ioc->biosVersion;

	strncpy (karg->driverVersion, MPT_LINUX_PACKAGE_NAME, MPT_IOCTL_VERSION_LENGTH);
	karg->driverVersion[MPT_IOCTL_VERSION_LENGTH-1]='\0';

	karg->busChangeEvent = 0;
	karg->hostId = ioc->pfacts[port].PortSCSIID;
	karg->rsvd[0] = karg->rsvd[1] = 0;

	if (copy_to_user((char __user *)arg, karg, data_size)) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_getiocinfo - "
			"Unable to write out mpt_ioctl_iocinfo struct @ %p\n",
			ioc->name, __FILE__, __LINE__, uarg);
		kfree(karg);
		return -EFAULT;
	}

	kfree(karg);
	return 0;
}

static int
mptctl_gettargetinfo (unsigned long arg)
{
	struct mpt_ioctl_targetinfo __user *uarg = (void __user *) arg;
	struct mpt_ioctl_targetinfo karg;
	MPT_ADAPTER		*ioc;
	VirtDevice		*vdevice;
	char			*pmem;
	int			*pdata;
	int			iocnum;
	int			numDevices = 0;
	int			lun;
	int			maxWordsLeft;
	int			numBytes;
	u8			port;
	struct scsi_device 	*sdev;

	if (copy_from_user(&karg, uarg, sizeof(struct mpt_ioctl_targetinfo))) {
		printk(KERN_ERR MYNAM "%s@%d::mptctl_gettargetinfo - "
			"Unable to read in mpt_ioctl_targetinfo struct @ %p\n",
				__FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	if (((iocnum = mpt_verify_adapter(karg.hdr.iocnum, &ioc)) < 0) ||
	    (ioc == NULL)) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_gettargetinfo() @%d - ioc%d not found!\n",
				__FILE__, __LINE__, iocnum);
		return -ENODEV;
	}

	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mptctl_gettargetinfo called.\n",
	    ioc->name));
	numBytes = karg.hdr.maxDataSize - sizeof(mpt_ioctl_header);
	maxWordsLeft = numBytes/sizeof(int);
	port = karg.hdr.port;

	if (maxWordsLeft <= 0) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_gettargetinfo() - no memory available!\n",
			ioc->name, __FILE__, __LINE__);
		return -ENOMEM;
	}


	pmem = kzalloc(numBytes, GFP_KERNEL);
	if (!pmem) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_gettargetinfo() - no memory available!\n",
			ioc->name, __FILE__, __LINE__);
		return -ENOMEM;
	}
	pdata =  (int *) pmem;

	if (ioc->sh){
		shost_for_each_device(sdev, ioc->sh) {
			if (!maxWordsLeft)
				continue;
			vdevice = sdev->hostdata;
			if (vdevice == NULL || vdevice->vtarget == NULL)
				continue;
			if (vdevice->vtarget->tflags &
			    MPT_TARGET_FLAGS_RAID_COMPONENT)
				continue;
			lun = (vdevice->vtarget->raidVolume) ? 0x80 : vdevice->lun;
			*pdata = (((u8)lun << 16) + (vdevice->vtarget->channel << 8) +
			    (vdevice->vtarget->id ));
			pdata++;
			numDevices++;
			--maxWordsLeft;
		}
	}
	karg.numDevices = numDevices;

	if (copy_to_user((char __user *)arg, &karg,
				sizeof(struct mpt_ioctl_targetinfo))) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_gettargetinfo - "
			"Unable to write out mpt_ioctl_targetinfo struct @ %p\n",
			ioc->name, __FILE__, __LINE__, uarg);
		kfree(pmem);
		return -EFAULT;
	}

	if (copy_to_user(uarg->targetInfo, pmem, numBytes)) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_gettargetinfo - "
			"Unable to write out mpt_ioctl_targetinfo struct @ %p\n",
			ioc->name, __FILE__, __LINE__, pdata);
		kfree(pmem);
		return -EFAULT;
	}

	kfree(pmem);

	return 0;
}

static int
mptctl_readtest (unsigned long arg)
{
	struct mpt_ioctl_test __user *uarg = (void __user *) arg;
	struct mpt_ioctl_test	 karg;
	MPT_ADAPTER *ioc;
	int iocnum;

	if (copy_from_user(&karg, uarg, sizeof(struct mpt_ioctl_test))) {
		printk(KERN_ERR MYNAM "%s@%d::mptctl_readtest - "
			"Unable to read in mpt_ioctl_test struct @ %p\n",
				__FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	if (((iocnum = mpt_verify_adapter(karg.hdr.iocnum, &ioc)) < 0) ||
	    (ioc == NULL)) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_readtest() @%d - ioc%d not found!\n",
				__FILE__, __LINE__, iocnum);
		return -ENODEV;
	}

	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mptctl_readtest called.\n",
	    ioc->name));

#ifdef MFCNT
	karg.chip_type = ioc->mfcnt;
#else
	karg.chip_type = ioc->pcidev->device;
#endif
	strncpy (karg.name, ioc->name, MPT_MAX_NAME);
	karg.name[MPT_MAX_NAME-1]='\0';
	strncpy (karg.product, ioc->prod_name, MPT_PRODUCT_LENGTH);
	karg.product[MPT_PRODUCT_LENGTH-1]='\0';

	if (copy_to_user((char __user *)arg, &karg, sizeof(struct mpt_ioctl_test))) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_readtest - "
			"Unable to write out mpt_ioctl_test struct @ %p\n",
			ioc->name, __FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	return 0;
}

static int
mptctl_eventquery (unsigned long arg)
{
	struct mpt_ioctl_eventquery __user *uarg = (void __user *) arg;
	struct mpt_ioctl_eventquery	 karg;
	MPT_ADAPTER *ioc;
	int iocnum;

	if (copy_from_user(&karg, uarg, sizeof(struct mpt_ioctl_eventquery))) {
		printk(KERN_ERR MYNAM "%s@%d::mptctl_eventquery - "
			"Unable to read in mpt_ioctl_eventquery struct @ %p\n",
				__FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	if (((iocnum = mpt_verify_adapter(karg.hdr.iocnum, &ioc)) < 0) ||
	    (ioc == NULL)) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_eventquery() @%d - ioc%d not found!\n",
				__FILE__, __LINE__, iocnum);
		return -ENODEV;
	}

	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mptctl_eventquery called.\n",
	    ioc->name));
	karg.eventEntries = MPTCTL_EVENT_LOG_SIZE;
	karg.eventTypes = ioc->eventTypes;

	if (copy_to_user((char __user *)arg, &karg, sizeof(struct mpt_ioctl_eventquery))) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_eventquery - "
			"Unable to write out mpt_ioctl_eventquery struct @ %p\n",
			ioc->name, __FILE__, __LINE__, uarg);
		return -EFAULT;
	}
	return 0;
}

static int
mptctl_eventenable (unsigned long arg)
{
	struct mpt_ioctl_eventenable __user *uarg = (void __user *) arg;
	struct mpt_ioctl_eventenable	 karg;
	MPT_ADAPTER *ioc;
	int iocnum;

	if (copy_from_user(&karg, uarg, sizeof(struct mpt_ioctl_eventenable))) {
		printk(KERN_ERR MYNAM "%s@%d::mptctl_eventenable - "
			"Unable to read in mpt_ioctl_eventenable struct @ %p\n",
				__FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	if (((iocnum = mpt_verify_adapter(karg.hdr.iocnum, &ioc)) < 0) ||
	    (ioc == NULL)) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_eventenable() @%d - ioc%d not found!\n",
				__FILE__, __LINE__, iocnum);
		return -ENODEV;
	}

	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mptctl_eventenable called.\n",
	    ioc->name));
	if (ioc->events == NULL) {
		int sz = MPTCTL_EVENT_LOG_SIZE * sizeof(MPT_IOCTL_EVENTS);
		ioc->events = kzalloc(sz, GFP_KERNEL);
		if (!ioc->events) {
			printk(MYIOC_s_ERR_FMT
			    ": ERROR - Insufficient memory to add adapter!\n",
			    ioc->name);
			return -ENOMEM;
		}
		ioc->alloc_total += sz;

		ioc->eventContext = 0;
        }

	ioc->eventTypes = karg.eventTypes;

	return 0;
}

static int
mptctl_eventreport (unsigned long arg)
{
	struct mpt_ioctl_eventreport __user *uarg = (void __user *) arg;
	struct mpt_ioctl_eventreport	 karg;
	MPT_ADAPTER		 *ioc;
	int			 iocnum;
	int			 numBytes, maxEvents, max;

	if (copy_from_user(&karg, uarg, sizeof(struct mpt_ioctl_eventreport))) {
		printk(KERN_ERR MYNAM "%s@%d::mptctl_eventreport - "
			"Unable to read in mpt_ioctl_eventreport struct @ %p\n",
				__FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	if (((iocnum = mpt_verify_adapter(karg.hdr.iocnum, &ioc)) < 0) ||
	    (ioc == NULL)) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_eventreport() @%d - ioc%d not found!\n",
				__FILE__, __LINE__, iocnum);
		return -ENODEV;
	}
	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mptctl_eventreport called.\n",
	    ioc->name));

	numBytes = karg.hdr.maxDataSize - sizeof(mpt_ioctl_header);
	maxEvents = numBytes/sizeof(MPT_IOCTL_EVENTS);


	max = MPTCTL_EVENT_LOG_SIZE < maxEvents ? MPTCTL_EVENT_LOG_SIZE : maxEvents;

	if ((max < 1) || !ioc->events)
		return -ENODATA;

	
	ioc->aen_event_read_flag=0;

	numBytes = max * sizeof(MPT_IOCTL_EVENTS);
	if (copy_to_user(uarg->eventData, ioc->events, numBytes)) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_eventreport - "
			"Unable to write out mpt_ioctl_eventreport struct @ %p\n",
			ioc->name, __FILE__, __LINE__, ioc->events);
		return -EFAULT;
	}

	return 0;
}

static int
mptctl_replace_fw (unsigned long arg)
{
	struct mpt_ioctl_replace_fw __user *uarg = (void __user *) arg;
	struct mpt_ioctl_replace_fw	 karg;
	MPT_ADAPTER		 *ioc;
	int			 iocnum;
	int			 newFwSize;

	if (copy_from_user(&karg, uarg, sizeof(struct mpt_ioctl_replace_fw))) {
		printk(KERN_ERR MYNAM "%s@%d::mptctl_replace_fw - "
			"Unable to read in mpt_ioctl_replace_fw struct @ %p\n",
				__FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	if (((iocnum = mpt_verify_adapter(karg.hdr.iocnum, &ioc)) < 0) ||
	    (ioc == NULL)) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_replace_fw() @%d - ioc%d not found!\n",
				__FILE__, __LINE__, iocnum);
		return -ENODEV;
	}

	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mptctl_replace_fw called.\n",
	    ioc->name));
	if (ioc->cached_fw == NULL)
		return 0;

	mpt_free_fw_memory(ioc);

	newFwSize = karg.newImageSize;

	if (newFwSize & 0x01)
		newFwSize += 1;
	if (newFwSize & 0x02)
		newFwSize += 2;

	mpt_alloc_fw_memory(ioc, newFwSize);
	if (ioc->cached_fw == NULL)
		return -ENOMEM;

	if (copy_from_user(ioc->cached_fw, uarg->newImage, newFwSize)) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_replace_fw - "
				"Unable to read in mpt_ioctl_replace_fw image "
				"@ %p\n", ioc->name, __FILE__, __LINE__, uarg);
		mpt_free_fw_memory(ioc);
		return -EFAULT;
	}

	ioc->facts.FWImageSize = newFwSize;
	return 0;
}

static int
mptctl_mpt_command (unsigned long arg)
{
	struct mpt_ioctl_command __user *uarg = (void __user *) arg;
	struct mpt_ioctl_command  karg;
	MPT_ADAPTER	*ioc;
	int		iocnum;
	int		rc;


	if (copy_from_user(&karg, uarg, sizeof(struct mpt_ioctl_command))) {
		printk(KERN_ERR MYNAM "%s@%d::mptctl_mpt_command - "
			"Unable to read in mpt_ioctl_command struct @ %p\n",
				__FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	if (((iocnum = mpt_verify_adapter(karg.hdr.iocnum, &ioc)) < 0) ||
	    (ioc == NULL)) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_mpt_command() @%d - ioc%d not found!\n",
				__FILE__, __LINE__, iocnum);
		return -ENODEV;
	}

	rc = mptctl_do_mpt_command (karg, &uarg->MF);

	return rc;
}

static int
mptctl_do_mpt_command (struct mpt_ioctl_command karg, void __user *mfPtr)
{
	MPT_ADAPTER	*ioc;
	MPT_FRAME_HDR	*mf = NULL;
	MPIHeader_t	*hdr;
	char		*psge;
	struct buflist	bufIn;	
	struct buflist	bufOut; 
	dma_addr_t	dma_addr_in;
	dma_addr_t	dma_addr_out;
	int		sgSize = 0;	
	int		iocnum, flagsLength;
	int		sz, rc = 0;
	int		msgContext;
	u16		req_idx;
	ulong 		timeout;
	unsigned long	timeleft;
	struct scsi_device *sdev;
	unsigned long	 flags;
	u8		 function;

	bufIn.kptr = bufOut.kptr = NULL;
	bufIn.len = bufOut.len = 0;

	if (((iocnum = mpt_verify_adapter(karg.hdr.iocnum, &ioc)) < 0) ||
	    (ioc == NULL)) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_do_mpt_command() @%d - ioc%d not found!\n",
				__FILE__, __LINE__, iocnum);
		return -ENODEV;
	}

	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	if (ioc->ioc_reset_in_progress) {
		spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
		printk(KERN_ERR MYNAM "%s@%d::mptctl_do_mpt_command - "
			"Busy with diagnostic reset\n", __FILE__, __LINE__);
		return -EBUSY;
	}
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);

	sz = karg.dataSgeOffset * 4;
	if (karg.dataInSize > 0)
		sz += ioc->SGE_size;
	if (karg.dataOutSize > 0)
		sz += ioc->SGE_size;

	if (sz > ioc->req_sz) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_do_mpt_command - "
			"Request frame too large (%d) maximum (%d)\n",
			ioc->name, __FILE__, __LINE__, sz, ioc->req_sz);
		return -EFAULT;
	}

        if ((mf = mpt_get_msg_frame(mptctl_id, ioc)) == NULL)
                return -EAGAIN;

	hdr = (MPIHeader_t *) mf;
	msgContext = le32_to_cpu(hdr->MsgContext);
	req_idx = le16_to_cpu(mf->u.frame.hwhdr.msgctxu.fld.req_idx);

	if (copy_from_user(mf, mfPtr, karg.dataSgeOffset * 4)) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_do_mpt_command - "
			"Unable to read MF from mpt_ioctl_command struct @ %p\n",
			ioc->name, __FILE__, __LINE__, mfPtr);
		function = -1;
		rc = -EFAULT;
		goto done_free_mem;
	}
	hdr->MsgContext = cpu_to_le32(msgContext);
	function = hdr->Function;


	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "sending mpi function (0x%02X), req=%p\n",
	    ioc->name, hdr->Function, mf));

	switch (function) {
	case MPI_FUNCTION_IOC_FACTS:
	case MPI_FUNCTION_PORT_FACTS:
		karg.dataOutSize  = karg.dataInSize = 0;
		break;

	case MPI_FUNCTION_CONFIG:
	{
		Config_t *config_frame;
		config_frame = (Config_t *)mf;
		dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "\ttype=0x%02x ext_type=0x%02x "
		    "number=0x%02x action=0x%02x\n", ioc->name,
		    config_frame->Header.PageType,
		    config_frame->ExtPageType,
		    config_frame->Header.PageNumber,
		    config_frame->Action));
		break;
	}

	case MPI_FUNCTION_FC_COMMON_TRANSPORT_SEND:
	case MPI_FUNCTION_FC_EX_LINK_SRVC_SEND:
	case MPI_FUNCTION_FW_UPLOAD:
	case MPI_FUNCTION_SCSI_ENCLOSURE_PROCESSOR:
	case MPI_FUNCTION_FW_DOWNLOAD:
	case MPI_FUNCTION_FC_PRIMITIVE_SEND:
	case MPI_FUNCTION_TOOLBOX:
	case MPI_FUNCTION_SAS_IO_UNIT_CONTROL:
		break;

	case MPI_FUNCTION_SCSI_IO_REQUEST:
		if (ioc->sh) {
			SCSIIORequest_t *pScsiReq = (SCSIIORequest_t *) mf;
			int qtag = MPI_SCSIIO_CONTROL_UNTAGGED;
			int scsidir = 0;
			int dataSize;
			u32 id;

			id = (ioc->devices_per_bus == 0) ? 256 : ioc->devices_per_bus;
			if (pScsiReq->TargetID > id) {
				printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_do_mpt_command - "
					"Target ID out of bounds. \n",
					ioc->name, __FILE__, __LINE__);
				rc = -ENODEV;
				goto done_free_mem;
			}

			if (pScsiReq->Bus >= ioc->number_of_buses) {
				printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_do_mpt_command - "
					"Target Bus out of bounds. \n",
					ioc->name, __FILE__, __LINE__);
				rc = -ENODEV;
				goto done_free_mem;
			}

			pScsiReq->MsgFlags &= ~MPI_SCSIIO_MSGFLGS_SENSE_WIDTH;
			pScsiReq->MsgFlags |= mpt_msg_flags(ioc);


			if (karg.maxSenseBytes > MPT_SENSE_BUFFER_SIZE)
				pScsiReq->SenseBufferLength = MPT_SENSE_BUFFER_SIZE;
			else
				pScsiReq->SenseBufferLength = karg.maxSenseBytes;

			pScsiReq->SenseBufferLowAddr =
				cpu_to_le32(ioc->sense_buf_low_dma
				   + (req_idx * MPT_SENSE_BUFFER_ALLOC));

			shost_for_each_device(sdev, ioc->sh) {
				struct scsi_target *starget = scsi_target(sdev);
				VirtTarget *vtarget = starget->hostdata;

				if (vtarget == NULL)
					continue;

				if ((pScsiReq->TargetID == vtarget->id) &&
				    (pScsiReq->Bus == vtarget->channel) &&
				    (vtarget->tflags & MPT_TARGET_FLAGS_Q_YES))
					qtag = MPI_SCSIIO_CONTROL_SIMPLEQ;
			}

			if (karg.dataOutSize > 0) {
				scsidir = MPI_SCSIIO_CONTROL_WRITE;
				dataSize = karg.dataOutSize;
			} else {
				scsidir = MPI_SCSIIO_CONTROL_READ;
				dataSize = karg.dataInSize;
			}

			pScsiReq->Control = cpu_to_le32(scsidir | qtag);
			pScsiReq->DataLength = cpu_to_le32(dataSize);


		} else {
			printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_do_mpt_command - "
				"SCSI driver is not loaded. \n",
				ioc->name, __FILE__, __LINE__);
			rc = -EFAULT;
			goto done_free_mem;
		}
		break;

	case MPI_FUNCTION_SMP_PASSTHROUGH:
		break;

	case MPI_FUNCTION_SATA_PASSTHROUGH:
		if (!ioc->sh) {
			printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_do_mpt_command - "
				"SCSI driver is not loaded. \n",
				ioc->name, __FILE__, __LINE__);
			rc = -EFAULT;
			goto done_free_mem;
		}
		break;

	case MPI_FUNCTION_RAID_ACTION:
		break;

	case MPI_FUNCTION_RAID_SCSI_IO_PASSTHROUGH:
		if (ioc->sh) {
			SCSIIORequest_t *pScsiReq = (SCSIIORequest_t *) mf;
			int qtag = MPI_SCSIIO_CONTROL_SIMPLEQ;
			int scsidir = MPI_SCSIIO_CONTROL_READ;
			int dataSize;

			pScsiReq->MsgFlags &= ~MPI_SCSIIO_MSGFLGS_SENSE_WIDTH;
			pScsiReq->MsgFlags |= mpt_msg_flags(ioc);


			if (karg.maxSenseBytes > MPT_SENSE_BUFFER_SIZE)
				pScsiReq->SenseBufferLength = MPT_SENSE_BUFFER_SIZE;
			else
				pScsiReq->SenseBufferLength = karg.maxSenseBytes;

			pScsiReq->SenseBufferLowAddr =
				cpu_to_le32(ioc->sense_buf_low_dma
				   + (req_idx * MPT_SENSE_BUFFER_ALLOC));


			if (karg.dataOutSize > 0) {
				scsidir = MPI_SCSIIO_CONTROL_WRITE;
				dataSize = karg.dataOutSize;
			} else {
				scsidir = MPI_SCSIIO_CONTROL_READ;
				dataSize = karg.dataInSize;
			}

			pScsiReq->Control = cpu_to_le32(scsidir | qtag);
			pScsiReq->DataLength = cpu_to_le32(dataSize);

		} else {
			printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_do_mpt_command - "
				"SCSI driver is not loaded. \n",
				ioc->name, __FILE__, __LINE__);
			rc = -EFAULT;
			goto done_free_mem;
		}
		break;

	case MPI_FUNCTION_SCSI_TASK_MGMT:
	{
		SCSITaskMgmt_t	*pScsiTm;
		pScsiTm = (SCSITaskMgmt_t *)mf;
		dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			"\tTaskType=0x%x MsgFlags=0x%x "
			"TaskMsgContext=0x%x id=%d channel=%d\n",
			ioc->name, pScsiTm->TaskType, le32_to_cpu
			(pScsiTm->TaskMsgContext), pScsiTm->MsgFlags,
			pScsiTm->TargetID, pScsiTm->Bus));
		break;
	}

	case MPI_FUNCTION_IOC_INIT:
		{
			IOCInit_t	*pInit = (IOCInit_t *) mf;
			u32		high_addr, sense_high;

			if (sizeof(dma_addr_t) == sizeof(u64)) {
				high_addr = cpu_to_le32((u32)((u64)ioc->req_frames_dma >> 32));
				sense_high= cpu_to_le32((u32)((u64)ioc->sense_buf_pool_dma >> 32));
			} else {
				high_addr = 0;
				sense_high= 0;
			}

			if ((pInit->Flags != 0) || (pInit->MaxDevices != ioc->facts.MaxDevices) ||
				(pInit->MaxBuses != ioc->facts.MaxBuses) ||
				(pInit->ReplyFrameSize != cpu_to_le16(ioc->reply_sz)) ||
				(pInit->HostMfaHighAddr != high_addr) ||
				(pInit->SenseBufferHighAddr != sense_high)) {
				printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_do_mpt_command - "
					"IOC_INIT issued with 1 or more incorrect parameters. Rejected.\n",
					ioc->name, __FILE__, __LINE__);
				rc = -EFAULT;
				goto done_free_mem;
			}
		}
		break;
	default:


		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_do_mpt_command - "
			"Illegal request (function 0x%x) \n",
			ioc->name, __FILE__, __LINE__, hdr->Function);
		rc = -EFAULT;
		goto done_free_mem;
	}

	psge = (char *) (((int *) mf) + karg.dataSgeOffset);
	flagsLength = 0;

	if (karg.dataOutSize > 0)
		sgSize ++;

	if (karg.dataInSize > 0)
		sgSize ++;

	if (sgSize > 0) {

		
		if (karg.dataOutSize > 0) {
			if (karg.dataInSize > 0) {
				flagsLength = ( MPI_SGE_FLAGS_SIMPLE_ELEMENT |
						MPI_SGE_FLAGS_END_OF_BUFFER |
						MPI_SGE_FLAGS_DIRECTION)
						<< MPI_SGE_FLAGS_SHIFT;
			} else {
				flagsLength = MPT_SGE_FLAGS_SSIMPLE_WRITE;
			}
			flagsLength |= karg.dataOutSize;
			bufOut.len = karg.dataOutSize;
			bufOut.kptr = pci_alloc_consistent(
					ioc->pcidev, bufOut.len, &dma_addr_out);

			if (bufOut.kptr == NULL) {
				rc = -ENOMEM;
				goto done_free_mem;
			} else {
				ioc->add_sge(psge, flagsLength, dma_addr_out);
				psge += ioc->SGE_size;

				if (copy_from_user(bufOut.kptr,
						karg.dataOutBufPtr,
						bufOut.len)) {
					printk(MYIOC_s_ERR_FMT
						"%s@%d::mptctl_do_mpt_command - Unable "
						"to read user data "
						"struct @ %p\n",
						ioc->name, __FILE__, __LINE__,karg.dataOutBufPtr);
					rc =  -EFAULT;
					goto done_free_mem;
				}
			}
		}

		if (karg.dataInSize > 0) {
			flagsLength = MPT_SGE_FLAGS_SSIMPLE_READ;
			flagsLength |= karg.dataInSize;

			bufIn.len = karg.dataInSize;
			bufIn.kptr = pci_alloc_consistent(ioc->pcidev,
					bufIn.len, &dma_addr_in);

			if (bufIn.kptr == NULL) {
				rc = -ENOMEM;
				goto done_free_mem;
			} else {
				ioc->add_sge(psge, flagsLength, dma_addr_in);
			}
		}
	} else  {
		ioc->add_sge(psge, flagsLength, (dma_addr_t) -1);
	}

	SET_MGMT_MSG_CONTEXT(ioc->ioctl_cmds.msg_context, hdr->MsgContext);
	INITIALIZE_MGMT_STATUS(ioc->ioctl_cmds.status)
	if (hdr->Function == MPI_FUNCTION_SCSI_TASK_MGMT) {

		mutex_lock(&ioc->taskmgmt_cmds.mutex);
		if (mpt_set_taskmgmt_in_progress_flag(ioc) != 0) {
			mutex_unlock(&ioc->taskmgmt_cmds.mutex);
			goto done_free_mem;
		}

		DBG_DUMP_TM_REQUEST_FRAME(ioc, (u32 *)mf);

		if ((ioc->facts.IOCCapabilities & MPI_IOCFACTS_CAPABILITY_HIGH_PRI_Q) &&
		    (ioc->facts.MsgVersion >= MPI_VERSION_01_05))
			mpt_put_msg_frame_hi_pri(mptctl_id, ioc, mf);
		else {
			rc =mpt_send_handshake_request(mptctl_id, ioc,
				sizeof(SCSITaskMgmt_t), (u32*)mf, CAN_SLEEP);
			if (rc != 0) {
				dfailprintk(ioc, printk(MYIOC_s_ERR_FMT
				    "send_handshake FAILED! (ioc %p, mf %p)\n",
				    ioc->name, ioc, mf));
				mpt_clear_taskmgmt_in_progress_flag(ioc);
				rc = -ENODATA;
				mutex_unlock(&ioc->taskmgmt_cmds.mutex);
				goto done_free_mem;
			}
		}

	} else
		mpt_put_msg_frame(mptctl_id, ioc, mf);

	
	timeout = (karg.timeout > 0) ? karg.timeout : MPT_IOCTL_DEFAULT_TIMEOUT;
retry_wait:
	timeleft = wait_for_completion_timeout(&ioc->ioctl_cmds.done,
				HZ*timeout);
	if (!(ioc->ioctl_cmds.status & MPT_MGMT_STATUS_COMMAND_GOOD)) {
		rc = -ETIME;
		dfailprintk(ioc, printk(MYIOC_s_ERR_FMT "%s: TIMED OUT!\n",
		    ioc->name, __func__));
		if (ioc->ioctl_cmds.status & MPT_MGMT_STATUS_DID_IOCRESET) {
			if (function == MPI_FUNCTION_SCSI_TASK_MGMT)
				mutex_unlock(&ioc->taskmgmt_cmds.mutex);
			goto done_free_mem;
		}
		if (!timeleft) {
			printk(MYIOC_s_WARN_FMT
			       "mpt cmd timeout, doorbell=0x%08x"
			       " function=0x%x\n",
			       ioc->name, mpt_GetIocState(ioc, 0), function);
			if (function == MPI_FUNCTION_SCSI_TASK_MGMT)
				mutex_unlock(&ioc->taskmgmt_cmds.mutex);
			mptctl_timeout_expired(ioc, mf);
			mf = NULL;
		} else
			goto retry_wait;
		goto done_free_mem;
	}

	if (function == MPI_FUNCTION_SCSI_TASK_MGMT)
		mutex_unlock(&ioc->taskmgmt_cmds.mutex);


	mf = NULL;

	if (ioc->ioctl_cmds.status & MPT_MGMT_STATUS_RF_VALID) {
		if (karg.maxReplyBytes < ioc->reply_sz) {
			sz = min(karg.maxReplyBytes,
				4*ioc->ioctl_cmds.reply[2]);
		} else {
			 sz = min(ioc->reply_sz, 4*ioc->ioctl_cmds.reply[2]);
		}
		if (sz > 0) {
			if (copy_to_user(karg.replyFrameBufPtr,
				 ioc->ioctl_cmds.reply, sz)){
				 printk(MYIOC_s_ERR_FMT
				     "%s@%d::mptctl_do_mpt_command - "
				 "Unable to write out reply frame %p\n",
				 ioc->name, __FILE__, __LINE__, karg.replyFrameBufPtr);
				 rc =  -ENODATA;
				 goto done_free_mem;
			}
		}
	}

	if (ioc->ioctl_cmds.status & MPT_MGMT_STATUS_SENSE_VALID) {
		sz = min(karg.maxSenseBytes, MPT_SENSE_BUFFER_SIZE);
		if (sz > 0) {
			if (copy_to_user(karg.senseDataPtr,
				ioc->ioctl_cmds.sense, sz)) {
				printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_do_mpt_command - "
				"Unable to write sense data to user %p\n",
				ioc->name, __FILE__, __LINE__,
				karg.senseDataPtr);
				rc =  -ENODATA;
				goto done_free_mem;
			}
		}
	}

	if ((ioc->ioctl_cmds.status & MPT_MGMT_STATUS_COMMAND_GOOD) &&
				(karg.dataInSize > 0) && (bufIn.kptr)) {

		if (copy_to_user(karg.dataInBufPtr,
				 bufIn.kptr, karg.dataInSize)) {
			printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_do_mpt_command - "
				"Unable to write data to user %p\n",
				ioc->name, __FILE__, __LINE__,
				karg.dataInBufPtr);
			rc =  -ENODATA;
		}
	}

done_free_mem:

	CLEAR_MGMT_STATUS(ioc->ioctl_cmds.status)
	SET_MGMT_MSG_CONTEXT(ioc->ioctl_cmds.msg_context, 0);

	if (bufOut.kptr != NULL) {
		pci_free_consistent(ioc->pcidev,
			bufOut.len, (void *) bufOut.kptr, dma_addr_out);
	}

	if (bufIn.kptr != NULL) {
		pci_free_consistent(ioc->pcidev,
			bufIn.len, (void *) bufIn.kptr, dma_addr_in);
	}

	if (mf)
		mpt_free_msg_frame(ioc, mf);

	return rc;
}

static int
mptctl_hp_hostinfo(unsigned long arg, unsigned int data_size)
{
	hp_host_info_t	__user *uarg = (void __user *) arg;
	MPT_ADAPTER		*ioc;
	struct pci_dev		*pdev;
	char                    *pbuf=NULL;
	dma_addr_t		buf_dma;
	hp_host_info_t		karg;
	CONFIGPARMS		cfg;
	ConfigPageHeader_t	hdr;
	int			iocnum;
	int			rc, cim_rev;
	ToolboxIstwiReadWriteRequest_t	*IstwiRWRequest;
	MPT_FRAME_HDR		*mf = NULL;
	MPIHeader_t		*mpi_hdr;
	unsigned long		timeleft;
	int			retval;

	if (data_size == sizeof(hp_host_info_t))
		cim_rev = 1;
	else if (data_size == sizeof(hp_host_info_rev0_t))
		cim_rev = 0;	
	else
		return -EFAULT;

	if (copy_from_user(&karg, uarg, sizeof(hp_host_info_t))) {
		printk(KERN_ERR MYNAM "%s@%d::mptctl_hp_host_info - "
			"Unable to read in hp_host_info struct @ %p\n",
				__FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	if (((iocnum = mpt_verify_adapter(karg.hdr.iocnum, &ioc)) < 0) ||
	    (ioc == NULL)) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_hp_hostinfo() @%d - ioc%d not found!\n",
				__FILE__, __LINE__, iocnum);
		return -ENODEV;
	}
	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT ": mptctl_hp_hostinfo called.\n",
	    ioc->name));

	pdev = (struct pci_dev *) ioc->pcidev;

	karg.vendor = pdev->vendor;
	karg.device = pdev->device;
	karg.subsystem_id = pdev->subsystem_device;
	karg.subsystem_vendor = pdev->subsystem_vendor;
	karg.devfn = pdev->devfn;
	karg.bus = pdev->bus->number;

	if (ioc->sh != NULL)
		karg.host_no = ioc->sh->host_no;
	else
		karg.host_no =  -1;

	karg.fw_version[0] = ioc->facts.FWVersion.Struct.Major >= 10 ?
		((ioc->facts.FWVersion.Struct.Major / 10) + '0') : '0';
	karg.fw_version[1] = (ioc->facts.FWVersion.Struct.Major % 10 ) + '0';
	karg.fw_version[2] = '.';
	karg.fw_version[3] = ioc->facts.FWVersion.Struct.Minor >= 10 ?
		((ioc->facts.FWVersion.Struct.Minor / 10) + '0') : '0';
	karg.fw_version[4] = (ioc->facts.FWVersion.Struct.Minor % 10 ) + '0';
	karg.fw_version[5] = '.';
	karg.fw_version[6] = ioc->facts.FWVersion.Struct.Unit >= 10 ?
		((ioc->facts.FWVersion.Struct.Unit / 10) + '0') : '0';
	karg.fw_version[7] = (ioc->facts.FWVersion.Struct.Unit % 10 ) + '0';
	karg.fw_version[8] = '.';
	karg.fw_version[9] = ioc->facts.FWVersion.Struct.Dev >= 10 ?
		((ioc->facts.FWVersion.Struct.Dev / 10) + '0') : '0';
	karg.fw_version[10] = (ioc->facts.FWVersion.Struct.Dev % 10 ) + '0';
	karg.fw_version[11] = '\0';

	hdr.PageVersion = 0;
	hdr.PageLength = 0;
	hdr.PageNumber = 0;
	hdr.PageType = MPI_CONFIG_PAGETYPE_MANUFACTURING;
	cfg.cfghdr.hdr = &hdr;
	cfg.physAddr = -1;
	cfg.pageAddr = 0;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;	
	cfg.timeout = 10;

	strncpy(karg.serial_number, " ", 24);
	if (mpt_config(ioc, &cfg) == 0) {
		if (cfg.cfghdr.hdr->PageLength > 0) {
			
			cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

			pbuf = pci_alloc_consistent(ioc->pcidev, hdr.PageLength * 4, &buf_dma);
			if (pbuf) {
				cfg.physAddr = buf_dma;
				if (mpt_config(ioc, &cfg) == 0) {
					ManufacturingPage0_t *pdata = (ManufacturingPage0_t *) pbuf;
					if (strlen(pdata->BoardTracerNumber) > 1) {
						strncpy(karg.serial_number, 									    pdata->BoardTracerNumber, 24);
						karg.serial_number[24-1]='\0';
					}
				}
				pci_free_consistent(ioc->pcidev, hdr.PageLength * 4, pbuf, buf_dma);
				pbuf = NULL;
			}
		}
	}
	rc = mpt_GetIocState(ioc, 1);
	switch (rc) {
	case MPI_IOC_STATE_OPERATIONAL:
		karg.ioc_status =  HP_STATUS_OK;
		break;

	case MPI_IOC_STATE_FAULT:
		karg.ioc_status =  HP_STATUS_FAILED;
		break;

	case MPI_IOC_STATE_RESET:
	case MPI_IOC_STATE_READY:
	default:
		karg.ioc_status =  HP_STATUS_OTHER;
		break;
	}

	karg.base_io_addr = pci_resource_start(pdev, 0);

	if ((ioc->bus_type == SAS) || (ioc->bus_type == FC))
		karg.bus_phys_width = HP_BUS_WIDTH_UNK;
	else
		karg.bus_phys_width = HP_BUS_WIDTH_16;

	karg.hard_resets = 0;
	karg.soft_resets = 0;
	karg.timeouts = 0;
	if (ioc->sh != NULL) {
		MPT_SCSI_HOST *hd =  shost_priv(ioc->sh);

		if (hd && (cim_rev == 1)) {
			karg.hard_resets = ioc->hard_resets;
			karg.soft_resets = ioc->soft_resets;
			karg.timeouts = ioc->timeouts;
		}
	}

	if ((mf = mpt_get_msg_frame(mptctl_id, ioc)) == NULL) {
		dfailprintk(ioc, printk(MYIOC_s_WARN_FMT
			"%s, no msg frames!!\n", ioc->name, __func__));
		goto out;
	}

	IstwiRWRequest = (ToolboxIstwiReadWriteRequest_t *)mf;
	mpi_hdr = (MPIHeader_t *) mf;
	memset(IstwiRWRequest,0,sizeof(ToolboxIstwiReadWriteRequest_t));
	IstwiRWRequest->Function = MPI_FUNCTION_TOOLBOX;
	IstwiRWRequest->Tool = MPI_TOOLBOX_ISTWI_READ_WRITE_TOOL;
	IstwiRWRequest->MsgContext = mpi_hdr->MsgContext;
	IstwiRWRequest->Flags = MPI_TB_ISTWI_FLAGS_READ;
	IstwiRWRequest->NumAddressBytes = 0x01;
	IstwiRWRequest->DataLength = cpu_to_le16(0x04);
	if (pdev->devfn & 1)
		IstwiRWRequest->DeviceAddr = 0xB2;
	else
		IstwiRWRequest->DeviceAddr = 0xB0;

	pbuf = pci_alloc_consistent(ioc->pcidev, 4, &buf_dma);
	if (!pbuf)
		goto out;
	ioc->add_sge((char *)&IstwiRWRequest->SGL,
	    (MPT_SGE_FLAGS_SSIMPLE_READ|4), buf_dma);

	retval = 0;
	SET_MGMT_MSG_CONTEXT(ioc->ioctl_cmds.msg_context,
				IstwiRWRequest->MsgContext);
	INITIALIZE_MGMT_STATUS(ioc->ioctl_cmds.status)
	mpt_put_msg_frame(mptctl_id, ioc, mf);

retry_wait:
	timeleft = wait_for_completion_timeout(&ioc->ioctl_cmds.done,
			HZ*MPT_IOCTL_DEFAULT_TIMEOUT);
	if (!(ioc->ioctl_cmds.status & MPT_MGMT_STATUS_COMMAND_GOOD)) {
		retval = -ETIME;
		printk(MYIOC_s_WARN_FMT "%s: failed\n", ioc->name, __func__);
		if (ioc->ioctl_cmds.status & MPT_MGMT_STATUS_DID_IOCRESET) {
			mpt_free_msg_frame(ioc, mf);
			goto out;
		}
		if (!timeleft) {
			printk(MYIOC_s_WARN_FMT
			       "HOST INFO command timeout, doorbell=0x%08x\n",
			       ioc->name, mpt_GetIocState(ioc, 0));
			mptctl_timeout_expired(ioc, mf);
		} else
			goto retry_wait;
		goto out;
	}

	if (ioc->ioctl_cmds.status & MPT_MGMT_STATUS_RF_VALID)
		karg.rsvd = *(u32 *)pbuf;

 out:
	CLEAR_MGMT_STATUS(ioc->ioctl_cmds.status)
	SET_MGMT_MSG_CONTEXT(ioc->ioctl_cmds.msg_context, 0);

	if (pbuf)
		pci_free_consistent(ioc->pcidev, 4, pbuf, buf_dma);

	if (copy_to_user((char __user *)arg, &karg, sizeof(hp_host_info_t))) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_hpgethostinfo - "
			"Unable to write out hp_host_info @ %p\n",
			ioc->name, __FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	return 0;

}

static int
mptctl_hp_targetinfo(unsigned long arg)
{
	hp_target_info_t __user *uarg = (void __user *) arg;
	SCSIDevicePage0_t	*pg0_alloc;
	SCSIDevicePage3_t	*pg3_alloc;
	MPT_ADAPTER		*ioc;
	MPT_SCSI_HOST 		*hd = NULL;
	hp_target_info_t	karg;
	int			iocnum;
	int			data_sz;
	dma_addr_t		page_dma;
	CONFIGPARMS	 	cfg;
	ConfigPageHeader_t	hdr;
	int			tmp, np, rc = 0;

	if (copy_from_user(&karg, uarg, sizeof(hp_target_info_t))) {
		printk(KERN_ERR MYNAM "%s@%d::mptctl_hp_targetinfo - "
			"Unable to read in hp_host_targetinfo struct @ %p\n",
				__FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	if (((iocnum = mpt_verify_adapter(karg.hdr.iocnum, &ioc)) < 0) ||
		(ioc == NULL)) {
		printk(KERN_DEBUG MYNAM "%s::mptctl_hp_targetinfo() @%d - ioc%d not found!\n",
				__FILE__, __LINE__, iocnum);
		return -ENODEV;
	}
	dctlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mptctl_hp_targetinfo called.\n",
	    ioc->name));

	if ((ioc->bus_type == SAS) || (ioc->bus_type == FC))
		return 0;

	if ((ioc->spi_data.sdp0length == 0) || (ioc->sh == NULL))
		return 0;

	if (ioc->sh->host_no != karg.hdr.host)
		return -ENODEV;

	data_sz = ioc->spi_data.sdp0length * 4;
	pg0_alloc = (SCSIDevicePage0_t *) pci_alloc_consistent(ioc->pcidev, data_sz, &page_dma);
	if (pg0_alloc) {
		hdr.PageVersion = ioc->spi_data.sdp0version;
		hdr.PageLength = data_sz;
		hdr.PageNumber = 0;
		hdr.PageType = MPI_CONFIG_PAGETYPE_SCSI_DEVICE;

		cfg.cfghdr.hdr = &hdr;
		cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
		cfg.dir = 0;
		cfg.timeout = 0;
		cfg.physAddr = page_dma;

		cfg.pageAddr = (karg.hdr.channel << 8) | karg.hdr.id;

		if ((rc = mpt_config(ioc, &cfg)) == 0) {
			np = le32_to_cpu(pg0_alloc->NegotiatedParameters);
			karg.negotiated_width = np & MPI_SCSIDEVPAGE0_NP_WIDE ?
					HP_BUS_WIDTH_16 : HP_BUS_WIDTH_8;

			if (np & MPI_SCSIDEVPAGE0_NP_NEG_SYNC_OFFSET_MASK) {
				tmp = (np & MPI_SCSIDEVPAGE0_NP_NEG_SYNC_PERIOD_MASK) >> 8;
				if (tmp < 0x09)
					karg.negotiated_speed = HP_DEV_SPEED_ULTRA320;
				else if (tmp <= 0x09)
					karg.negotiated_speed = HP_DEV_SPEED_ULTRA160;
				else if (tmp <= 0x0A)
					karg.negotiated_speed = HP_DEV_SPEED_ULTRA2;
				else if (tmp <= 0x0C)
					karg.negotiated_speed = HP_DEV_SPEED_ULTRA;
				else if (tmp <= 0x25)
					karg.negotiated_speed = HP_DEV_SPEED_FAST;
				else
					karg.negotiated_speed = HP_DEV_SPEED_ASYNC;
			} else
				karg.negotiated_speed = HP_DEV_SPEED_ASYNC;
		}

		pci_free_consistent(ioc->pcidev, data_sz, (u8 *) pg0_alloc, page_dma);
	}

	karg.message_rejects = -1;
	karg.phase_errors = -1;
	karg.parity_errors = -1;
	karg.select_timeouts = -1;

	hdr.PageVersion = 0;
	hdr.PageLength = 0;
	hdr.PageNumber = 3;
	hdr.PageType = MPI_CONFIG_PAGETYPE_SCSI_DEVICE;

	cfg.cfghdr.hdr = &hdr;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;
	cfg.timeout = 0;
	cfg.physAddr = -1;
	if ((mpt_config(ioc, &cfg) == 0) && (cfg.cfghdr.hdr->PageLength > 0)) {
		
		cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
		data_sz = (int) cfg.cfghdr.hdr->PageLength * 4;
		pg3_alloc = (SCSIDevicePage3_t *) pci_alloc_consistent(
							ioc->pcidev, data_sz, &page_dma);
		if (pg3_alloc) {
			cfg.physAddr = page_dma;
			cfg.pageAddr = (karg.hdr.channel << 8) | karg.hdr.id;
			if ((rc = mpt_config(ioc, &cfg)) == 0) {
				karg.message_rejects = (u32) le16_to_cpu(pg3_alloc->MsgRejectCount);
				karg.phase_errors = (u32) le16_to_cpu(pg3_alloc->PhaseErrorCount);
				karg.parity_errors = (u32) le16_to_cpu(pg3_alloc->ParityErrorCount);
			}
			pci_free_consistent(ioc->pcidev, data_sz, (u8 *) pg3_alloc, page_dma);
		}
	}
	hd = shost_priv(ioc->sh);
	if (hd != NULL)
		karg.select_timeouts = hd->sel_timeout[karg.hdr.id];

	if (copy_to_user((char __user *)arg, &karg, sizeof(hp_target_info_t))) {
		printk(MYIOC_s_ERR_FMT "%s@%d::mptctl_hp_target_info - "
			"Unable to write out mpt_ioctl_targetinfo struct @ %p\n",
			ioc->name, __FILE__, __LINE__, uarg);
		return -EFAULT;
	}

	return 0;
}


static const struct file_operations mptctl_fops = {
	.owner =	THIS_MODULE,
	.llseek =	no_llseek,
	.fasync = 	mptctl_fasync,
	.unlocked_ioctl = mptctl_ioctl,
	.release =	mptctl_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_mpctl_ioctl,
#endif
};

static struct miscdevice mptctl_miscdev = {
	MPT_MINOR,
	MYNAM,
	&mptctl_fops
};


#ifdef CONFIG_COMPAT

static int
compat_mptfwxfer_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct mpt_fw_xfer32 kfw32;
	struct mpt_fw_xfer kfw;
	MPT_ADAPTER *iocp = NULL;
	int iocnum, iocnumX;
	int nonblock = (filp->f_flags & O_NONBLOCK);
	int ret;


	if (copy_from_user(&kfw32, (char __user *)arg, sizeof(kfw32)))
		return -EFAULT;

	
	iocnumX = kfw32.iocnum & 0xFF;
	if (((iocnum = mpt_verify_adapter(iocnumX, &iocp)) < 0) ||
	    (iocp == NULL)) {
		printk(KERN_DEBUG MYNAM "::compat_mptfwxfer_ioctl @%d - ioc%d not found!\n",
			__LINE__, iocnumX);
		return -ENODEV;
	}

	if ((ret = mptctl_syscall_down(iocp, nonblock)) != 0)
		return ret;

	dctlprintk(iocp, printk(MYIOC_s_DEBUG_FMT "compat_mptfwxfer_ioctl() called\n",
	    iocp->name));
	kfw.iocnum = iocnum;
	kfw.fwlen = kfw32.fwlen;
	kfw.bufp = compat_ptr(kfw32.bufp);

	ret = mptctl_do_fw_download(kfw.iocnum, kfw.bufp, kfw.fwlen);

	mutex_unlock(&iocp->ioctl_cmds.mutex);

	return ret;
}

static int
compat_mpt_command(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct mpt_ioctl_command32 karg32;
	struct mpt_ioctl_command32 __user *uarg = (struct mpt_ioctl_command32 __user *) arg;
	struct mpt_ioctl_command karg;
	MPT_ADAPTER *iocp = NULL;
	int iocnum, iocnumX;
	int nonblock = (filp->f_flags & O_NONBLOCK);
	int ret;

	if (copy_from_user(&karg32, (char __user *)arg, sizeof(karg32)))
		return -EFAULT;

	
	iocnumX = karg32.hdr.iocnum & 0xFF;
	if (((iocnum = mpt_verify_adapter(iocnumX, &iocp)) < 0) ||
	    (iocp == NULL)) {
		printk(KERN_DEBUG MYNAM "::compat_mpt_command @%d - ioc%d not found!\n",
			__LINE__, iocnumX);
		return -ENODEV;
	}

	if ((ret = mptctl_syscall_down(iocp, nonblock)) != 0)
		return ret;

	dctlprintk(iocp, printk(MYIOC_s_DEBUG_FMT "compat_mpt_command() called\n",
	    iocp->name));
	
	karg.hdr.iocnum = karg32.hdr.iocnum;
	karg.hdr.port = karg32.hdr.port;
	karg.timeout = karg32.timeout;
	karg.maxReplyBytes = karg32.maxReplyBytes;

	karg.dataInSize = karg32.dataInSize;
	karg.dataOutSize = karg32.dataOutSize;
	karg.maxSenseBytes = karg32.maxSenseBytes;
	karg.dataSgeOffset = karg32.dataSgeOffset;

	karg.replyFrameBufPtr = (char __user *)(unsigned long)karg32.replyFrameBufPtr;
	karg.dataInBufPtr = (char __user *)(unsigned long)karg32.dataInBufPtr;
	karg.dataOutBufPtr = (char __user *)(unsigned long)karg32.dataOutBufPtr;
	karg.senseDataPtr = (char __user *)(unsigned long)karg32.senseDataPtr;

	ret = mptctl_do_mpt_command (karg, &uarg->MF);

	mutex_unlock(&iocp->ioctl_cmds.mutex);

	return ret;
}

static long compat_mpctl_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	long ret;
	mutex_lock(&mpctl_mutex);
	switch (cmd) {
	case MPTIOCINFO:
	case MPTIOCINFO1:
	case MPTIOCINFO2:
	case MPTTARGETINFO:
	case MPTEVENTQUERY:
	case MPTEVENTENABLE:
	case MPTEVENTREPORT:
	case MPTHARDRESET:
	case HP_GETHOSTINFO:
	case HP_GETTARGETINFO:
	case MPTTEST:
		ret = __mptctl_ioctl(f, cmd, arg);
		break;
	case MPTCOMMAND32:
		ret = compat_mpt_command(f, cmd, arg);
		break;
	case MPTFWDOWNLOAD32:
		ret = compat_mptfwxfer_ioctl(f, cmd, arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	mutex_unlock(&mpctl_mutex);
	return ret;
}

#endif



static int
mptctl_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	MPT_ADAPTER *ioc = pci_get_drvdata(pdev);

	mutex_init(&ioc->ioctl_cmds.mutex);
	init_completion(&ioc->ioctl_cmds.done);
	return 0;
}

static void
mptctl_remove(struct pci_dev *pdev)
{
}

static struct mpt_pci_driver mptctl_driver = {
  .probe		= mptctl_probe,
  .remove		= mptctl_remove,
};

static int __init mptctl_init(void)
{
	int err;
	int where = 1;

	show_mptmod_ver(my_NAME, my_VERSION);

	mpt_device_driver_register(&mptctl_driver, MPTCTL_DRIVER);

	
	err = misc_register(&mptctl_miscdev);
	if (err < 0) {
		printk(KERN_ERR MYNAM ": Can't register misc device [minor=%d].\n", MPT_MINOR);
		goto out_fail;
	}
	printk(KERN_INFO MYNAM ": Registered with Fusion MPT base driver\n");
	printk(KERN_INFO MYNAM ": /dev/%s @ (major,minor=%d,%d)\n",
			 mptctl_miscdev.name, MISC_MAJOR, mptctl_miscdev.minor);

	++where;
	mptctl_id = mpt_register(mptctl_reply, MPTCTL_DRIVER,
	    "mptctl_reply");
	if (!mptctl_id || mptctl_id >= MPT_MAX_PROTOCOL_DRIVERS) {
		printk(KERN_ERR MYNAM ": ERROR: Failed to register with Fusion MPT base driver\n");
		misc_deregister(&mptctl_miscdev);
		err = -EBUSY;
		goto out_fail;
	}

	mptctl_taskmgmt_id = mpt_register(mptctl_taskmgmt_reply, MPTCTL_DRIVER,
	    "mptctl_taskmgmt_reply");
	if (!mptctl_taskmgmt_id || mptctl_taskmgmt_id >= MPT_MAX_PROTOCOL_DRIVERS) {
		printk(KERN_ERR MYNAM ": ERROR: Failed to register with Fusion MPT base driver\n");
		mpt_deregister(mptctl_id);
		misc_deregister(&mptctl_miscdev);
		err = -EBUSY;
		goto out_fail;
	}

	mpt_reset_register(mptctl_id, mptctl_ioc_reset);
	mpt_event_register(mptctl_id, mptctl_event_process);

	return 0;

out_fail:

	mpt_device_driver_deregister(MPTCTL_DRIVER);

	return err;
}

static void mptctl_exit(void)
{
	misc_deregister(&mptctl_miscdev);
	printk(KERN_INFO MYNAM ": Deregistered /dev/%s @ (major,minor=%d,%d)\n",
			 mptctl_miscdev.name, MISC_MAJOR, mptctl_miscdev.minor);

	
	mpt_event_deregister(mptctl_id);

	
	mpt_reset_deregister(mptctl_id);

	
	mpt_deregister(mptctl_taskmgmt_id);
	mpt_deregister(mptctl_id);

        mpt_device_driver_deregister(MPTCTL_DRIVER);

}


module_init(mptctl_init);
module_exit(mptctl_exit);
