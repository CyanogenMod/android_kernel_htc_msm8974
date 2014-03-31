/*
 *  linux/drivers/message/fusion/mptbase.c
 *      This is the Fusion MPT base driver which supports multiple
 *      (SCSI + LAN) specialized protocol drivers.
 *      For use with LSI PCI chip/adapter(s)
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
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>		
#include <linux/dma-mapping.h>
#include <asm/io.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif
#include <linux/kthread.h>
#include <scsi/scsi_host.h>

#include "mptbase.h"
#include "lsi/mpi_log_fc.h"

#define my_NAME		"Fusion MPT base driver"
#define my_VERSION	MPT_LINUX_VERSION_COMMON
#define MYNAM		"mptbase"

MODULE_AUTHOR(MODULEAUTHOR);
MODULE_DESCRIPTION(my_NAME);
MODULE_LICENSE("GPL");
MODULE_VERSION(my_VERSION);


static int mpt_msi_enable_spi;
module_param(mpt_msi_enable_spi, int, 0);
MODULE_PARM_DESC(mpt_msi_enable_spi,
		 " Enable MSI Support for SPI controllers (default=0)");

static int mpt_msi_enable_fc;
module_param(mpt_msi_enable_fc, int, 0);
MODULE_PARM_DESC(mpt_msi_enable_fc,
		 " Enable MSI Support for FC controllers (default=0)");

static int mpt_msi_enable_sas;
module_param(mpt_msi_enable_sas, int, 0);
MODULE_PARM_DESC(mpt_msi_enable_sas,
		 " Enable MSI Support for SAS controllers (default=0)");

static int mpt_channel_mapping;
module_param(mpt_channel_mapping, int, 0);
MODULE_PARM_DESC(mpt_channel_mapping, " Mapping id's to channels (default=0)");

static int mpt_debug_level;
static int mpt_set_debug_level(const char *val, struct kernel_param *kp);
module_param_call(mpt_debug_level, mpt_set_debug_level, param_get_int,
		  &mpt_debug_level, 0600);
MODULE_PARM_DESC(mpt_debug_level,
		 " debug level - refer to mptdebug.h - (default=0)");

int mpt_fwfault_debug;
EXPORT_SYMBOL(mpt_fwfault_debug);
module_param(mpt_fwfault_debug, int, 0600);
MODULE_PARM_DESC(mpt_fwfault_debug,
		 "Enable detection of Firmware fault and halt Firmware on fault - (default=0)");

static char	MptCallbacksName[MPT_MAX_PROTOCOL_DRIVERS]
				[MPT_MAX_CALLBACKNAME_LEN+1];

#ifdef MFCNT
static int mfcounter = 0;
#define PRINT_MF_COUNT 20000
#endif


#define WHOINIT_UNKNOWN		0xAA

					
LIST_HEAD(ioc_list);
					
static MPT_CALLBACK		 MptCallbacks[MPT_MAX_PROTOCOL_DRIVERS];
					
static int			 MptDriverClass[MPT_MAX_PROTOCOL_DRIVERS];
					
static MPT_EVHANDLER		 MptEvHandlers[MPT_MAX_PROTOCOL_DRIVERS];
					
static MPT_RESETHANDLER		 MptResetHandlers[MPT_MAX_PROTOCOL_DRIVERS];
static struct mpt_pci_driver 	*MptDeviceDriverHandlers[MPT_MAX_PROTOCOL_DRIVERS];

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry 	*mpt_proc_root_dir;
#endif

static u8 mpt_base_index = MPT_MAX_PROTOCOL_DRIVERS;
static u8 last_drv_idx;

static irqreturn_t mpt_interrupt(int irq, void *bus_id);
static int	mptbase_reply(MPT_ADAPTER *ioc, MPT_FRAME_HDR *req,
		MPT_FRAME_HDR *reply);
static int	mpt_handshake_req_reply_wait(MPT_ADAPTER *ioc, int reqBytes,
			u32 *req, int replyBytes, u16 *u16reply, int maxwait,
			int sleepFlag);
static int	mpt_do_ioc_recovery(MPT_ADAPTER *ioc, u32 reason, int sleepFlag);
static void	mpt_detect_bound_ports(MPT_ADAPTER *ioc, struct pci_dev *pdev);
static void	mpt_adapter_disable(MPT_ADAPTER *ioc);
static void	mpt_adapter_dispose(MPT_ADAPTER *ioc);

static void	MptDisplayIocCapabilities(MPT_ADAPTER *ioc);
static int	MakeIocReady(MPT_ADAPTER *ioc, int force, int sleepFlag);
static int	GetIocFacts(MPT_ADAPTER *ioc, int sleepFlag, int reason);
static int	GetPortFacts(MPT_ADAPTER *ioc, int portnum, int sleepFlag);
static int	SendIocInit(MPT_ADAPTER *ioc, int sleepFlag);
static int	SendPortEnable(MPT_ADAPTER *ioc, int portnum, int sleepFlag);
static int	mpt_do_upload(MPT_ADAPTER *ioc, int sleepFlag);
static int	mpt_downloadboot(MPT_ADAPTER *ioc, MpiFwHeader_t *pFwHeader, int sleepFlag);
static int	mpt_diag_reset(MPT_ADAPTER *ioc, int ignore, int sleepFlag);
static int	KickStart(MPT_ADAPTER *ioc, int ignore, int sleepFlag);
static int	SendIocReset(MPT_ADAPTER *ioc, u8 reset_type, int sleepFlag);
static int	PrimeIocFifos(MPT_ADAPTER *ioc);
static int	WaitForDoorbellAck(MPT_ADAPTER *ioc, int howlong, int sleepFlag);
static int	WaitForDoorbellInt(MPT_ADAPTER *ioc, int howlong, int sleepFlag);
static int	WaitForDoorbellReply(MPT_ADAPTER *ioc, int howlong, int sleepFlag);
static int	GetLanConfigPages(MPT_ADAPTER *ioc);
static int	GetIoUnitPage2(MPT_ADAPTER *ioc);
int		mptbase_sas_persist_operation(MPT_ADAPTER *ioc, u8 persist_opcode);
static int	mpt_GetScsiPortSettings(MPT_ADAPTER *ioc, int portnum);
static int	mpt_readScsiDevicePageHeaders(MPT_ADAPTER *ioc, int portnum);
static void 	mpt_read_ioc_pg_1(MPT_ADAPTER *ioc);
static void 	mpt_read_ioc_pg_4(MPT_ADAPTER *ioc);
static void	mpt_get_manufacturing_pg_0(MPT_ADAPTER *ioc);
static int	SendEventNotification(MPT_ADAPTER *ioc, u8 EvSwitch,
	int sleepFlag);
static int	SendEventAck(MPT_ADAPTER *ioc, EventNotificationReply_t *evnp);
static int	mpt_host_page_access_control(MPT_ADAPTER *ioc, u8 access_control_value, int sleepFlag);
static int	mpt_host_page_alloc(MPT_ADAPTER *ioc, pIOCInit_t ioc_init);

#ifdef CONFIG_PROC_FS
static const struct file_operations mpt_summary_proc_fops;
static const struct file_operations mpt_version_proc_fops;
static const struct file_operations mpt_iocinfo_proc_fops;
#endif
static void	mpt_get_fw_exp_ver(char *buf, MPT_ADAPTER *ioc);

static int	ProcessEventNotification(MPT_ADAPTER *ioc,
		EventNotificationReply_t *evReply, int *evHandlers);
static void	mpt_iocstatus_info(MPT_ADAPTER *ioc, u32 ioc_status, MPT_FRAME_HDR *mf);
static void	mpt_fc_log_info(MPT_ADAPTER *ioc, u32 log_info);
static void	mpt_spi_log_info(MPT_ADAPTER *ioc, u32 log_info);
static void	mpt_sas_log_info(MPT_ADAPTER *ioc, u32 log_info , u8 cb_idx);
static int	mpt_read_ioc_pg_3(MPT_ADAPTER *ioc);
static void	mpt_inactive_raid_list_free(MPT_ADAPTER *ioc);

static int  __init    fusion_init  (void);
static void __exit    fusion_exit  (void);

#define CHIPREG_READ32(addr) 		readl_relaxed(addr)
#define CHIPREG_READ32_dmasync(addr)	readl(addr)
#define CHIPREG_WRITE32(addr,val) 	writel(val, addr)
#define CHIPREG_PIO_WRITE32(addr,val)	outl(val, (unsigned long)addr)
#define CHIPREG_PIO_READ32(addr) 	inl((unsigned long)addr)

static void
pci_disable_io_access(struct pci_dev *pdev)
{
	u16 command_reg;

	pci_read_config_word(pdev, PCI_COMMAND, &command_reg);
	command_reg &= ~1;
	pci_write_config_word(pdev, PCI_COMMAND, command_reg);
}

static void
pci_enable_io_access(struct pci_dev *pdev)
{
	u16 command_reg;

	pci_read_config_word(pdev, PCI_COMMAND, &command_reg);
	command_reg |= 1;
	pci_write_config_word(pdev, PCI_COMMAND, command_reg);
}

static int mpt_set_debug_level(const char *val, struct kernel_param *kp)
{
	int ret = param_set_int(val, kp);
	MPT_ADAPTER *ioc;

	if (ret)
		return ret;

	list_for_each_entry(ioc, &ioc_list, list)
		ioc->debug_level = mpt_debug_level;
	return 0;
}

static u8
mpt_get_cb_idx(MPT_DRIVER_CLASS dclass)
{
	u8 cb_idx;

	for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--)
		if (MptDriverClass[cb_idx] == dclass)
			return cb_idx;
	return 0;
}

static int
mpt_is_discovery_complete(MPT_ADAPTER *ioc)
{
	ConfigExtendedPageHeader_t hdr;
	CONFIGPARMS cfg;
	SasIOUnitPage0_t *buffer;
	dma_addr_t dma_handle;
	int rc = 0;

	memset(&hdr, 0, sizeof(ConfigExtendedPageHeader_t));
	memset(&cfg, 0, sizeof(CONFIGPARMS));
	hdr.PageVersion = MPI_SASIOUNITPAGE0_PAGEVERSION;
	hdr.PageType = MPI_CONFIG_PAGETYPE_EXTENDED;
	hdr.ExtPageType = MPI_CONFIG_EXTPAGETYPE_SAS_IO_UNIT;
	cfg.cfghdr.ehdr = &hdr;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;

	if ((mpt_config(ioc, &cfg)))
		goto out;
	if (!hdr.ExtPageLength)
		goto out;

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
	    &dma_handle);
	if (!buffer)
		goto out;

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	if ((mpt_config(ioc, &cfg)))
		goto out_free_consistent;

	if (!(buffer->PhyData[0].PortFlags &
	    MPI_SAS_IOUNIT0_PORT_FLAGS_DISCOVERY_IN_PROGRESS))
		rc = 1;

 out_free_consistent:
	pci_free_consistent(ioc->pcidev, hdr.ExtPageLength * 4,
	    buffer, dma_handle);
 out:
	return rc;
}


static int mpt_remove_dead_ioc_func(void *arg)
{
	MPT_ADAPTER *ioc = (MPT_ADAPTER *)arg;
	struct pci_dev *pdev;

	if ((ioc == NULL))
		return -1;

	pdev = ioc->pcidev;
	if ((pdev == NULL))
		return -1;

	pci_stop_and_remove_bus_device(pdev);
	return 0;
}



static void
mpt_fault_reset_work(struct work_struct *work)
{
	MPT_ADAPTER	*ioc =
	    container_of(work, MPT_ADAPTER, fault_reset_work.work);
	u32		 ioc_raw_state;
	int		 rc;
	unsigned long	 flags;
	MPT_SCSI_HOST	*hd;
	struct task_struct *p;

	if (ioc->ioc_reset_in_progress || !ioc->active)
		goto out;


	ioc_raw_state = mpt_GetIocState(ioc, 0);
	if ((ioc_raw_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_MASK) {
		printk(MYIOC_s_INFO_FMT "%s: IOC is non-operational !!!!\n",
		    ioc->name, __func__);

		hd = shost_priv(ioc->sh);
		ioc->schedule_dead_ioc_flush_running_cmds(hd);

		
		p = kthread_run(mpt_remove_dead_ioc_func, ioc,
				"mpt_dead_ioc_%d", ioc->id);
		if (IS_ERR(p))	{
			printk(MYIOC_s_ERR_FMT
				"%s: Running mpt_dead_ioc thread failed !\n",
				ioc->name, __func__);
		} else {
			printk(MYIOC_s_WARN_FMT
				"%s: Running mpt_dead_ioc thread success !\n",
				ioc->name, __func__);
		}
		return; 
	}

	if ((ioc_raw_state & MPI_IOC_STATE_MASK)
			== MPI_IOC_STATE_FAULT) {
		printk(MYIOC_s_WARN_FMT "IOC is in FAULT state (%04xh)!!!\n",
		       ioc->name, ioc_raw_state & MPI_DOORBELL_DATA_MASK);
		printk(MYIOC_s_WARN_FMT "Issuing HardReset from %s!!\n",
		       ioc->name, __func__);
		rc = mpt_HardResetHandler(ioc, CAN_SLEEP);
		printk(MYIOC_s_WARN_FMT "%s: HardReset: %s\n", ioc->name,
		       __func__, (rc == 0) ? "success" : "failed");
		ioc_raw_state = mpt_GetIocState(ioc, 0);
		if ((ioc_raw_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_FAULT)
			printk(MYIOC_s_WARN_FMT "IOC is in FAULT state after "
			    "reset (%04xh)\n", ioc->name, ioc_raw_state &
			    MPI_DOORBELL_DATA_MASK);
	} else if (ioc->bus_type == SAS && ioc->sas_discovery_quiesce_io) {
		if ((mpt_is_discovery_complete(ioc))) {
			devtprintk(ioc, printk(MYIOC_s_DEBUG_FMT "clearing "
			    "discovery_quiesce_io flag\n", ioc->name));
			ioc->sas_discovery_quiesce_io = 0;
		}
	}

 out:
	if (ioc->alt_ioc)
		ioc = ioc->alt_ioc;

	
	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	if (ioc->reset_work_q)
		queue_delayed_work(ioc->reset_work_q, &ioc->fault_reset_work,
			msecs_to_jiffies(MPT_POLLING_INTERVAL));
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
}


static void
mpt_turbo_reply(MPT_ADAPTER *ioc, u32 pa)
{
	MPT_FRAME_HDR *mf = NULL;
	MPT_FRAME_HDR *mr = NULL;
	u16 req_idx = 0;
	u8 cb_idx;

	dmfprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Got TURBO reply req_idx=%08x\n",
				ioc->name, pa));

	switch (pa >> MPI_CONTEXT_REPLY_TYPE_SHIFT) {
	case MPI_CONTEXT_REPLY_TYPE_SCSI_INIT:
		req_idx = pa & 0x0000FFFF;
		cb_idx = (pa & 0x00FF0000) >> 16;
		mf = MPT_INDEX_2_MFPTR(ioc, req_idx);
		break;
	case MPI_CONTEXT_REPLY_TYPE_LAN:
		cb_idx = mpt_get_cb_idx(MPTLAN_DRIVER);
		if ((pa & 0x58000000) == 0x58000000) {
			req_idx = pa & 0x0000FFFF;
			mf = MPT_INDEX_2_MFPTR(ioc, req_idx);
			mpt_free_msg_frame(ioc, mf);
			mb();
			return;
			break;
		}
		mr = (MPT_FRAME_HDR *) CAST_U32_TO_PTR(pa);
		break;
	case MPI_CONTEXT_REPLY_TYPE_SCSI_TARGET:
		cb_idx = mpt_get_cb_idx(MPTSTM_DRIVER);
		mr = (MPT_FRAME_HDR *) CAST_U32_TO_PTR(pa);
		break;
	default:
		cb_idx = 0;
		BUG();
	}

	
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS ||
		MptCallbacks[cb_idx] == NULL) {
		printk(MYIOC_s_WARN_FMT "%s: Invalid cb_idx (%d)!\n",
				__func__, ioc->name, cb_idx);
		goto out;
	}

	if (MptCallbacks[cb_idx](ioc, mf, mr))
		mpt_free_msg_frame(ioc, mf);
 out:
	mb();
}

static void
mpt_reply(MPT_ADAPTER *ioc, u32 pa)
{
	MPT_FRAME_HDR	*mf;
	MPT_FRAME_HDR	*mr;
	u16		 req_idx;
	u8		 cb_idx;
	int		 freeme;

	u32 reply_dma_low;
	u16 ioc_stat;



	reply_dma_low = (pa <<= 1);
	mr = (MPT_FRAME_HDR *)((u8 *)ioc->reply_frames +
			 (reply_dma_low - ioc->reply_frames_low_dma));

	req_idx = le16_to_cpu(mr->u.frame.hwhdr.msgctxu.fld.req_idx);
	cb_idx = mr->u.frame.hwhdr.msgctxu.fld.cb_idx;
	mf = MPT_INDEX_2_MFPTR(ioc, req_idx);

	dmfprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Got non-TURBO reply=%p req_idx=%x cb_idx=%x Function=%x\n",
			ioc->name, mr, req_idx, cb_idx, mr->u.hdr.Function));
	DBG_DUMP_REPLY_FRAME(ioc, (u32 *)mr);

	ioc_stat = le16_to_cpu(mr->u.reply.IOCStatus);
	if (ioc_stat & MPI_IOCSTATUS_FLAG_LOG_INFO_AVAILABLE) {
		u32	 log_info = le32_to_cpu(mr->u.reply.IOCLogInfo);
		if (ioc->bus_type == FC)
			mpt_fc_log_info(ioc, log_info);
		else if (ioc->bus_type == SPI)
			mpt_spi_log_info(ioc, log_info);
		else if (ioc->bus_type == SAS)
			mpt_sas_log_info(ioc, log_info, cb_idx);
	}

	if (ioc_stat & MPI_IOCSTATUS_MASK)
		mpt_iocstatus_info(ioc, (u32)ioc_stat, mf);

	
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS ||
		MptCallbacks[cb_idx] == NULL) {
		printk(MYIOC_s_WARN_FMT "%s: Invalid cb_idx (%d)!\n",
				__func__, ioc->name, cb_idx);
		freeme = 0;
		goto out;
	}

	freeme = MptCallbacks[cb_idx](ioc, mf, mr);

 out:
	
	CHIPREG_WRITE32(&ioc->chip->ReplyFifo, pa);

	if (freeme)
		mpt_free_msg_frame(ioc, mf);
	mb();
}

static irqreturn_t
mpt_interrupt(int irq, void *bus_id)
{
	MPT_ADAPTER *ioc = bus_id;
	u32 pa = CHIPREG_READ32_dmasync(&ioc->chip->ReplyFifo);

	if (pa == 0xFFFFFFFF)
		return IRQ_NONE;

	do {
		if (pa & MPI_ADDRESS_REPLY_A_BIT)
			mpt_reply(ioc, pa);
		else
			mpt_turbo_reply(ioc, pa);
		pa = CHIPREG_READ32_dmasync(&ioc->chip->ReplyFifo);
	} while (pa != 0xFFFFFFFF);

	return IRQ_HANDLED;
}

static int
mptbase_reply(MPT_ADAPTER *ioc, MPT_FRAME_HDR *req, MPT_FRAME_HDR *reply)
{
	EventNotificationReply_t *pEventReply;
	u8 event;
	int evHandlers;
	int freereq = 1;

	switch (reply->u.hdr.Function) {
	case MPI_FUNCTION_EVENT_NOTIFICATION:
		pEventReply = (EventNotificationReply_t *)reply;
		evHandlers = 0;
		ProcessEventNotification(ioc, pEventReply, &evHandlers);
		event = le32_to_cpu(pEventReply->Event) & 0xFF;
		if (pEventReply->MsgFlags & MPI_MSGFLAGS_CONTINUATION_REPLY)
			freereq = 0;
		if (event != MPI_EVENT_EVENT_CHANGE)
			break;
	case MPI_FUNCTION_CONFIG:
	case MPI_FUNCTION_SAS_IO_UNIT_CONTROL:
		ioc->mptbase_cmds.status |= MPT_MGMT_STATUS_COMMAND_GOOD;
		if (reply) {
			ioc->mptbase_cmds.status |= MPT_MGMT_STATUS_RF_VALID;
			memcpy(ioc->mptbase_cmds.reply, reply,
			    min(MPT_DEFAULT_FRAME_SIZE,
				4 * reply->u.reply.MsgLength));
		}
		if (ioc->mptbase_cmds.status & MPT_MGMT_STATUS_PENDING) {
			ioc->mptbase_cmds.status &= ~MPT_MGMT_STATUS_PENDING;
			complete(&ioc->mptbase_cmds.done);
		} else
			freereq = 0;
		if (ioc->mptbase_cmds.status & MPT_MGMT_STATUS_FREE_MF)
			freereq = 1;
		break;
	case MPI_FUNCTION_EVENT_ACK:
		devtverboseprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "EventAck reply received\n", ioc->name));
		break;
	default:
		printk(MYIOC_s_ERR_FMT
		    "Unexpected msg function (=%02Xh) reply received!\n",
		    ioc->name, reply->u.hdr.Function);
		break;
	}

	return freereq;
}

u8
mpt_register(MPT_CALLBACK cbfunc, MPT_DRIVER_CLASS dclass, char *func_name)
{
	u8 cb_idx;
	last_drv_idx = MPT_MAX_PROTOCOL_DRIVERS;

	for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--) {
		if (MptCallbacks[cb_idx] == NULL) {
			MptCallbacks[cb_idx] = cbfunc;
			MptDriverClass[cb_idx] = dclass;
			MptEvHandlers[cb_idx] = NULL;
			last_drv_idx = cb_idx;
			strlcpy(MptCallbacksName[cb_idx], func_name,
				MPT_MAX_CALLBACKNAME_LEN+1);
			break;
		}
	}

	return last_drv_idx;
}

void
mpt_deregister(u8 cb_idx)
{
	if (cb_idx && (cb_idx < MPT_MAX_PROTOCOL_DRIVERS)) {
		MptCallbacks[cb_idx] = NULL;
		MptDriverClass[cb_idx] = MPTUNKNOWN_DRIVER;
		MptEvHandlers[cb_idx] = NULL;

		last_drv_idx++;
	}
}

int
mpt_event_register(u8 cb_idx, MPT_EVHANDLER ev_cbfunc)
{
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return -1;

	MptEvHandlers[cb_idx] = ev_cbfunc;
	return 0;
}

void
mpt_event_deregister(u8 cb_idx)
{
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return;

	MptEvHandlers[cb_idx] = NULL;
}

int
mpt_reset_register(u8 cb_idx, MPT_RESETHANDLER reset_func)
{
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return -1;

	MptResetHandlers[cb_idx] = reset_func;
	return 0;
}

void
mpt_reset_deregister(u8 cb_idx)
{
	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return;

	MptResetHandlers[cb_idx] = NULL;
}

int
mpt_device_driver_register(struct mpt_pci_driver * dd_cbfunc, u8 cb_idx)
{
	MPT_ADAPTER	*ioc;
	const struct pci_device_id *id;

	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return -EINVAL;

	MptDeviceDriverHandlers[cb_idx] = dd_cbfunc;

	
	list_for_each_entry(ioc, &ioc_list, list) {
		id = ioc->pcidev->driver ?
		    ioc->pcidev->driver->id_table : NULL;
		if (dd_cbfunc->probe)
			dd_cbfunc->probe(ioc->pcidev, id);
	 }

	return 0;
}

void
mpt_device_driver_deregister(u8 cb_idx)
{
	struct mpt_pci_driver *dd_cbfunc;
	MPT_ADAPTER	*ioc;

	if (!cb_idx || cb_idx >= MPT_MAX_PROTOCOL_DRIVERS)
		return;

	dd_cbfunc = MptDeviceDriverHandlers[cb_idx];

	list_for_each_entry(ioc, &ioc_list, list) {
		if (dd_cbfunc->remove)
			dd_cbfunc->remove(ioc->pcidev);
	}

	MptDeviceDriverHandlers[cb_idx] = NULL;
}


MPT_FRAME_HDR*
mpt_get_msg_frame(u8 cb_idx, MPT_ADAPTER *ioc)
{
	MPT_FRAME_HDR *mf;
	unsigned long flags;
	u16	 req_idx;	

	

#ifdef MFCNT
	if (!ioc->active)
		printk(MYIOC_s_WARN_FMT "IOC Not Active! mpt_get_msg_frame "
		    "returning NULL!\n", ioc->name);
#endif

	
	if (!ioc->active)
		return NULL;

	spin_lock_irqsave(&ioc->FreeQlock, flags);
	if (!list_empty(&ioc->FreeQ)) {
		int req_offset;

		mf = list_entry(ioc->FreeQ.next, MPT_FRAME_HDR,
				u.frame.linkage.list);
		list_del(&mf->u.frame.linkage.list);
		mf->u.frame.linkage.arg1 = 0;
		mf->u.frame.hwhdr.msgctxu.fld.cb_idx = cb_idx;	
		req_offset = (u8 *)mf - (u8 *)ioc->req_frames;
								
		req_idx = req_offset / ioc->req_sz;
		mf->u.frame.hwhdr.msgctxu.fld.req_idx = cpu_to_le16(req_idx);
		mf->u.frame.hwhdr.msgctxu.fld.rsvd = 0;
		
		ioc->RequestNB[req_idx] = ioc->NB_for_64_byte_frame;
#ifdef MFCNT
		ioc->mfcnt++;
#endif
	}
	else
		mf = NULL;
	spin_unlock_irqrestore(&ioc->FreeQlock, flags);

#ifdef MFCNT
	if (mf == NULL)
		printk(MYIOC_s_WARN_FMT "IOC Active. No free Msg Frames! "
		    "Count 0x%x Max 0x%x\n", ioc->name, ioc->mfcnt,
		    ioc->req_depth);
	mfcounter++;
	if (mfcounter == PRINT_MF_COUNT)
		printk(MYIOC_s_INFO_FMT "MF Count 0x%x Max 0x%x \n", ioc->name,
		    ioc->mfcnt, ioc->req_depth);
#endif

	dmfprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mpt_get_msg_frame(%d,%d), got mf=%p\n",
	    ioc->name, cb_idx, ioc->id, mf));
	return mf;
}

void
mpt_put_msg_frame(u8 cb_idx, MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf)
{
	u32 mf_dma_addr;
	int req_offset;
	u16	 req_idx;	

	
	mf->u.frame.hwhdr.msgctxu.fld.cb_idx = cb_idx;		
	req_offset = (u8 *)mf - (u8 *)ioc->req_frames;
								
	req_idx = req_offset / ioc->req_sz;
	mf->u.frame.hwhdr.msgctxu.fld.req_idx = cpu_to_le16(req_idx);
	mf->u.frame.hwhdr.msgctxu.fld.rsvd = 0;

	DBG_DUMP_PUT_MSG_FRAME(ioc, (u32 *)mf);

	mf_dma_addr = (ioc->req_frames_low_dma + req_offset) | ioc->RequestNB[req_idx];
	dsgprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mf_dma_addr=%x req_idx=%d "
	    "RequestNB=%x\n", ioc->name, mf_dma_addr, req_idx,
	    ioc->RequestNB[req_idx]));
	CHIPREG_WRITE32(&ioc->chip->RequestFifo, mf_dma_addr);
}

void
mpt_put_msg_frame_hi_pri(u8 cb_idx, MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf)
{
	u32 mf_dma_addr;
	int req_offset;
	u16	 req_idx;	

	
	mf->u.frame.hwhdr.msgctxu.fld.cb_idx = cb_idx;
	req_offset = (u8 *)mf - (u8 *)ioc->req_frames;
	req_idx = req_offset / ioc->req_sz;
	mf->u.frame.hwhdr.msgctxu.fld.req_idx = cpu_to_le16(req_idx);
	mf->u.frame.hwhdr.msgctxu.fld.rsvd = 0;

	DBG_DUMP_PUT_MSG_FRAME(ioc, (u32 *)mf);

	mf_dma_addr = (ioc->req_frames_low_dma + req_offset);
	dsgprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mf_dma_addr=%x req_idx=%d\n",
		ioc->name, mf_dma_addr, req_idx));
	CHIPREG_WRITE32(&ioc->chip->RequestHiPriFifo, mf_dma_addr);
}

void
mpt_free_msg_frame(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf)
{
	unsigned long flags;

	
	spin_lock_irqsave(&ioc->FreeQlock, flags);
	if (cpu_to_le32(mf->u.frame.linkage.arg1) == 0xdeadbeaf)
		goto out;
	
	mf->u.frame.linkage.arg1 = cpu_to_le32(0xdeadbeaf);
	list_add_tail(&mf->u.frame.linkage.list, &ioc->FreeQ);
#ifdef MFCNT
	ioc->mfcnt--;
#endif
 out:
	spin_unlock_irqrestore(&ioc->FreeQlock, flags);
}

static void
mpt_add_sge(void *pAddr, u32 flagslength, dma_addr_t dma_addr)
{
	SGESimple32_t *pSge = (SGESimple32_t *) pAddr;
	pSge->FlagsLength = cpu_to_le32(flagslength);
	pSge->Address = cpu_to_le32(dma_addr);
}

static void
mpt_add_sge_64bit(void *pAddr, u32 flagslength, dma_addr_t dma_addr)
{
	SGESimple64_t *pSge = (SGESimple64_t *) pAddr;
	pSge->Address.Low = cpu_to_le32
			(lower_32_bits(dma_addr));
	pSge->Address.High = cpu_to_le32
			(upper_32_bits(dma_addr));
	pSge->FlagsLength = cpu_to_le32
			((flagslength | MPT_SGE_FLAGS_64_BIT_ADDRESSING));
}

static void
mpt_add_sge_64bit_1078(void *pAddr, u32 flagslength, dma_addr_t dma_addr)
{
	SGESimple64_t *pSge = (SGESimple64_t *) pAddr;
	u32 tmp;

	pSge->Address.Low = cpu_to_le32
			(lower_32_bits(dma_addr));
	tmp = (u32)(upper_32_bits(dma_addr));

	if ((((u64)dma_addr + MPI_SGE_LENGTH(flagslength)) >> 32)  == 9) {
		flagslength |=
		    MPI_SGE_SET_FLAGS(MPI_SGE_FLAGS_LOCAL_ADDRESS);
		tmp |= (1<<31);
		if (mpt_debug_level & MPT_DEBUG_36GB_MEM)
			printk(KERN_DEBUG "1078 P0M2 addressing for "
			    "addr = 0x%llx len = %d\n",
			    (unsigned long long)dma_addr,
			    MPI_SGE_LENGTH(flagslength));
	}

	pSge->Address.High = cpu_to_le32(tmp);
	pSge->FlagsLength = cpu_to_le32(
		(flagslength | MPT_SGE_FLAGS_64_BIT_ADDRESSING));
}

static void
mpt_add_chain(void *pAddr, u8 next, u16 length, dma_addr_t dma_addr)
{
		SGEChain32_t *pChain = (SGEChain32_t *) pAddr;
		pChain->Length = cpu_to_le16(length);
		pChain->Flags = MPI_SGE_FLAGS_CHAIN_ELEMENT;
		pChain->NextChainOffset = next;
		pChain->Address = cpu_to_le32(dma_addr);
}

static void
mpt_add_chain_64bit(void *pAddr, u8 next, u16 length, dma_addr_t dma_addr)
{
		SGEChain64_t *pChain = (SGEChain64_t *) pAddr;
		u32 tmp = dma_addr & 0xFFFFFFFF;

		pChain->Length = cpu_to_le16(length);
		pChain->Flags = (MPI_SGE_FLAGS_CHAIN_ELEMENT |
				 MPI_SGE_FLAGS_64_BIT_ADDRESSING);

		pChain->NextChainOffset = next;

		pChain->Address.Low = cpu_to_le32(tmp);
		tmp = (u32)(upper_32_bits(dma_addr));
		pChain->Address.High = cpu_to_le32(tmp);
}

int
mpt_send_handshake_request(u8 cb_idx, MPT_ADAPTER *ioc, int reqBytes, u32 *req, int sleepFlag)
{
	int	r = 0;
	u8	*req_as_bytes;
	int	 ii;


	ii = MFPTR_2_MPT_INDEX(ioc,(MPT_FRAME_HDR*)req);
	if (reqBytes >= 12 && ii >= 0 && ii < ioc->req_depth) {
		MPT_FRAME_HDR *mf = (MPT_FRAME_HDR*)req;
		mf->u.frame.hwhdr.msgctxu.fld.req_idx = cpu_to_le16(ii);
		mf->u.frame.hwhdr.msgctxu.fld.cb_idx = cb_idx;
	}

	
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	CHIPREG_WRITE32(&ioc->chip->Doorbell,
			((MPI_FUNCTION_HANDSHAKE<<MPI_DOORBELL_FUNCTION_SHIFT) |
			 ((reqBytes/4)<<MPI_DOORBELL_ADD_DWORDS_SHIFT)));

	
	if ((ii = WaitForDoorbellInt(ioc, 5, sleepFlag)) < 0) {
		return ii;
	}

	
	if (!(CHIPREG_READ32(&ioc->chip->Doorbell) & MPI_DOORBELL_ACTIVE))
		return -5;

	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "mpt_send_handshake_request start, WaitCnt=%d\n",
		ioc->name, ii));

	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	if ((r = WaitForDoorbellAck(ioc, 5, sleepFlag)) < 0) {
		return -2;
	}

	
	req_as_bytes = (u8 *) req;
	for (ii = 0; ii < reqBytes/4; ii++) {
		u32 word;

		word = ((req_as_bytes[(ii*4) + 0] <<  0) |
			(req_as_bytes[(ii*4) + 1] <<  8) |
			(req_as_bytes[(ii*4) + 2] << 16) |
			(req_as_bytes[(ii*4) + 3] << 24));
		CHIPREG_WRITE32(&ioc->chip->Doorbell, word);
		if ((r = WaitForDoorbellAck(ioc, 5, sleepFlag)) < 0) {
			r = -3;
			break;
		}
	}

	if (r >= 0 && WaitForDoorbellInt(ioc, 10, sleepFlag) >= 0)
		r = 0;
	else
		r = -4;

	
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	return r;
}


static int
mpt_host_page_access_control(MPT_ADAPTER *ioc, u8 access_control_value, int sleepFlag)
{
	int	 r = 0;

	
	if (CHIPREG_READ32(&ioc->chip->Doorbell)
	    & MPI_DOORBELL_ACTIVE)
	    return -1;

	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	CHIPREG_WRITE32(&ioc->chip->Doorbell,
		((MPI_FUNCTION_HOST_PAGEBUF_ACCESS_CONTROL
		 <<MPI_DOORBELL_FUNCTION_SHIFT) |
		 (access_control_value<<12)));

	
	if ((r = WaitForDoorbellAck(ioc, 5, sleepFlag)) < 0) {
		return -2;
	}else
		return 0;
}

static int
mpt_host_page_alloc(MPT_ADAPTER *ioc, pIOCInit_t ioc_init)
{
	char	*psge;
	int	flags_length;
	u32	host_page_buffer_sz=0;

	if(!ioc->HostPageBuffer) {

		host_page_buffer_sz =
		    le32_to_cpu(ioc->facts.HostPageBufferSGE.FlagsLength) & 0xFFFFFF;

		if(!host_page_buffer_sz)
			return 0; 

		
		while(host_page_buffer_sz > 0) {

			if((ioc->HostPageBuffer = pci_alloc_consistent(
			    ioc->pcidev,
			    host_page_buffer_sz,
			    &ioc->HostPageBuffer_dma)) != NULL) {

				dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
				    "host_page_buffer @ %p, dma @ %x, sz=%d bytes\n",
				    ioc->name, ioc->HostPageBuffer,
				    (u32)ioc->HostPageBuffer_dma,
				    host_page_buffer_sz));
				ioc->alloc_total += host_page_buffer_sz;
				ioc->HostPageBuffer_sz = host_page_buffer_sz;
				break;
			}

			host_page_buffer_sz -= (4*1024);
		}
	}

	if(!ioc->HostPageBuffer) {
		printk(MYIOC_s_ERR_FMT
		    "Failed to alloc memory for host_page_buffer!\n",
		    ioc->name);
		return -999;
	}

	psge = (char *)&ioc_init->HostPageBufferSGE;
	flags_length = MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_SYSTEM_ADDRESS |
	    MPI_SGE_FLAGS_HOST_TO_IOC |
	    MPI_SGE_FLAGS_END_OF_BUFFER;
	flags_length = flags_length << MPI_SGE_FLAGS_SHIFT;
	flags_length |= ioc->HostPageBuffer_sz;
	ioc->add_sge(psge, flags_length, ioc->HostPageBuffer_dma);
	ioc->facts.HostPageBufferSGE = ioc_init->HostPageBufferSGE;

return 0;
}

int
mpt_verify_adapter(int iocid, MPT_ADAPTER **iocpp)
{
	MPT_ADAPTER *ioc;

	list_for_each_entry(ioc,&ioc_list,list) {
		if (ioc->id == iocid) {
			*iocpp =ioc;
			return iocid;
		}
	}

	*iocpp = NULL;
	return -1;
}

static void
mpt_get_product_name(u16 vendor, u16 device, u8 revision, char *prod_name)
{
	char *product_str = NULL;

	if (vendor == PCI_VENDOR_ID_BROCADE) {
		switch (device)
		{
		case MPI_MANUFACTPAGE_DEVICEID_FC949E:
			switch (revision)
			{
			case 0x00:
				product_str = "BRE040 A0";
				break;
			case 0x01:
				product_str = "BRE040 A1";
				break;
			default:
				product_str = "BRE040";
				break;
			}
			break;
		}
		goto out;
	}

	switch (device)
	{
	case MPI_MANUFACTPAGE_DEVICEID_FC909:
		product_str = "LSIFC909 B1";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC919:
		product_str = "LSIFC919 B0";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC929:
		product_str = "LSIFC929 B0";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC919X:
		if (revision < 0x80)
			product_str = "LSIFC919X A0";
		else
			product_str = "LSIFC919XL A1";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC929X:
		if (revision < 0x80)
			product_str = "LSIFC929X A0";
		else
			product_str = "LSIFC929XL A1";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC939X:
		product_str = "LSIFC939X A1";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC949X:
		product_str = "LSIFC949X A1";
		break;
	case MPI_MANUFACTPAGE_DEVICEID_FC949E:
		switch (revision)
		{
		case 0x00:
			product_str = "LSIFC949E A0";
			break;
		case 0x01:
			product_str = "LSIFC949E A1";
			break;
		default:
			product_str = "LSIFC949E";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_53C1030:
		switch (revision)
		{
		case 0x00:
			product_str = "LSI53C1030 A0";
			break;
		case 0x01:
			product_str = "LSI53C1030 B0";
			break;
		case 0x03:
			product_str = "LSI53C1030 B1";
			break;
		case 0x07:
			product_str = "LSI53C1030 B2";
			break;
		case 0x08:
			product_str = "LSI53C1030 C0";
			break;
		case 0x80:
			product_str = "LSI53C1030T A0";
			break;
		case 0x83:
			product_str = "LSI53C1030T A2";
			break;
		case 0x87:
			product_str = "LSI53C1030T A3";
			break;
		case 0xc1:
			product_str = "LSI53C1020A A1";
			break;
		default:
			product_str = "LSI53C1030";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_1030_53C1035:
		switch (revision)
		{
		case 0x03:
			product_str = "LSI53C1035 A2";
			break;
		case 0x04:
			product_str = "LSI53C1035 B0";
			break;
		default:
			product_str = "LSI53C1035";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_SAS1064:
		switch (revision)
		{
		case 0x00:
			product_str = "LSISAS1064 A1";
			break;
		case 0x01:
			product_str = "LSISAS1064 A2";
			break;
		case 0x02:
			product_str = "LSISAS1064 A3";
			break;
		case 0x03:
			product_str = "LSISAS1064 A4";
			break;
		default:
			product_str = "LSISAS1064";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_SAS1064E:
		switch (revision)
		{
		case 0x00:
			product_str = "LSISAS1064E A0";
			break;
		case 0x01:
			product_str = "LSISAS1064E B0";
			break;
		case 0x02:
			product_str = "LSISAS1064E B1";
			break;
		case 0x04:
			product_str = "LSISAS1064E B2";
			break;
		case 0x08:
			product_str = "LSISAS1064E B3";
			break;
		default:
			product_str = "LSISAS1064E";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_SAS1068:
		switch (revision)
		{
		case 0x00:
			product_str = "LSISAS1068 A0";
			break;
		case 0x01:
			product_str = "LSISAS1068 B0";
			break;
		case 0x02:
			product_str = "LSISAS1068 B1";
			break;
		default:
			product_str = "LSISAS1068";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_SAS1068E:
		switch (revision)
		{
		case 0x00:
			product_str = "LSISAS1068E A0";
			break;
		case 0x01:
			product_str = "LSISAS1068E B0";
			break;
		case 0x02:
			product_str = "LSISAS1068E B1";
			break;
		case 0x04:
			product_str = "LSISAS1068E B2";
			break;
		case 0x08:
			product_str = "LSISAS1068E B3";
			break;
		default:
			product_str = "LSISAS1068E";
			break;
		}
		break;
	case MPI_MANUFACTPAGE_DEVID_SAS1078:
		switch (revision)
		{
		case 0x00:
			product_str = "LSISAS1078 A0";
			break;
		case 0x01:
			product_str = "LSISAS1078 B0";
			break;
		case 0x02:
			product_str = "LSISAS1078 C0";
			break;
		case 0x03:
			product_str = "LSISAS1078 C1";
			break;
		case 0x04:
			product_str = "LSISAS1078 C2";
			break;
		default:
			product_str = "LSISAS1078";
			break;
		}
		break;
	}

 out:
	if (product_str)
		sprintf(prod_name, "%s", product_str);
}

static int
mpt_mapresources(MPT_ADAPTER *ioc)
{
	u8		__iomem *mem;
	int		 ii;
	resource_size_t	 mem_phys;
	unsigned long	 port;
	u32		 msize;
	u32		 psize;
	u8		 revision;
	int		 r = -ENODEV;
	struct pci_dev *pdev;

	pdev = ioc->pcidev;
	ioc->bars = pci_select_bars(pdev, IORESOURCE_MEM);
	if (pci_enable_device_mem(pdev)) {
		printk(MYIOC_s_ERR_FMT "pci_enable_device_mem() "
		    "failed\n", ioc->name);
		return r;
	}
	if (pci_request_selected_regions(pdev, ioc->bars, "mpt")) {
		printk(MYIOC_s_ERR_FMT "pci_request_selected_regions() with "
		    "MEM failed\n", ioc->name);
		return r;
	}

	pci_read_config_byte(pdev, PCI_CLASS_REVISION, &revision);

	if (sizeof(dma_addr_t) > 4) {
		const uint64_t required_mask = dma_get_required_mask
		    (&pdev->dev);
		if (required_mask > DMA_BIT_MASK(32)
			&& !pci_set_dma_mask(pdev, DMA_BIT_MASK(64))
			&& !pci_set_consistent_dma_mask(pdev,
						 DMA_BIT_MASK(64))) {
			ioc->dma_mask = DMA_BIT_MASK(64);
			dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
				": 64 BIT PCI BUS DMA ADDRESSING SUPPORTED\n",
				ioc->name));
		} else if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))
			&& !pci_set_consistent_dma_mask(pdev,
						DMA_BIT_MASK(32))) {
			ioc->dma_mask = DMA_BIT_MASK(32);
			dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
				": 32 BIT PCI BUS DMA ADDRESSING SUPPORTED\n",
				ioc->name));
		} else {
			printk(MYIOC_s_WARN_FMT "no suitable DMA mask for %s\n",
			    ioc->name, pci_name(pdev));
			pci_release_selected_regions(pdev, ioc->bars);
			return r;
		}
	} else {
		if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))
			&& !pci_set_consistent_dma_mask(pdev,
						DMA_BIT_MASK(32))) {
			ioc->dma_mask = DMA_BIT_MASK(32);
			dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
				": 32 BIT PCI BUS DMA ADDRESSING SUPPORTED\n",
				ioc->name));
		} else {
			printk(MYIOC_s_WARN_FMT "no suitable DMA mask for %s\n",
			    ioc->name, pci_name(pdev));
			pci_release_selected_regions(pdev, ioc->bars);
			return r;
		}
	}

	mem_phys = msize = 0;
	port = psize = 0;
	for (ii = 0; ii < DEVICE_COUNT_RESOURCE; ii++) {
		if (pci_resource_flags(pdev, ii) & PCI_BASE_ADDRESS_SPACE_IO) {
			if (psize)
				continue;
			
			port = pci_resource_start(pdev, ii);
			psize = pci_resource_len(pdev, ii);
		} else {
			if (msize)
				continue;
			
			mem_phys = pci_resource_start(pdev, ii);
			msize = pci_resource_len(pdev, ii);
		}
	}
	ioc->mem_size = msize;

	mem = NULL;
	
	
	mem = ioremap(mem_phys, msize);
	if (mem == NULL) {
		printk(MYIOC_s_ERR_FMT ": ERROR - Unable to map adapter"
			" memory!\n", ioc->name);
		pci_release_selected_regions(pdev, ioc->bars);
		return -EINVAL;
	}
	ioc->memmap = mem;
	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT "mem = %p, mem_phys = %llx\n",
	    ioc->name, mem, (unsigned long long)mem_phys));

	ioc->mem_phys = mem_phys;
	ioc->chip = (SYSIF_REGS __iomem *)mem;

	
	ioc->pio_mem_phys = port;
	ioc->pio_chip = (SYSIF_REGS __iomem *)port;

	return 0;
}

int
mpt_attach(struct pci_dev *pdev, const struct pci_device_id *id)
{
	MPT_ADAPTER	*ioc;
	u8		 cb_idx;
	int		 r = -ENODEV;
	u8		 revision;
	u8		 pcixcmd;
	static int	 mpt_ids = 0;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *dent;
#endif

	ioc = kzalloc(sizeof(MPT_ADAPTER), GFP_ATOMIC);
	if (ioc == NULL) {
		printk(KERN_ERR MYNAM ": ERROR - Insufficient memory to add adapter!\n");
		return -ENOMEM;
	}

	ioc->id = mpt_ids++;
	sprintf(ioc->name, "ioc%d", ioc->id);
	dinitprintk(ioc, printk(KERN_WARNING MYNAM ": mpt_adapter_install\n"));

	ioc->debug_level = mpt_debug_level;
	if (mpt_debug_level)
		printk(KERN_INFO "mpt_debug_level=%xh\n", mpt_debug_level);

	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT ": mpt_adapter_install\n", ioc->name));

	ioc->pcidev = pdev;
	if (mpt_mapresources(ioc)) {
		kfree(ioc);
		return r;
	}

	if (ioc->dma_mask == DMA_BIT_MASK(64)) {
		if (pdev->device == MPI_MANUFACTPAGE_DEVID_SAS1078)
			ioc->add_sge = &mpt_add_sge_64bit_1078;
		else
			ioc->add_sge = &mpt_add_sge_64bit;
		ioc->add_chain = &mpt_add_chain_64bit;
		ioc->sg_addr_size = 8;
	} else {
		ioc->add_sge = &mpt_add_sge;
		ioc->add_chain = &mpt_add_chain;
		ioc->sg_addr_size = 4;
	}
	ioc->SGE_size = sizeof(u32) + ioc->sg_addr_size;

	ioc->alloc_total = sizeof(MPT_ADAPTER);
	ioc->req_sz = MPT_DEFAULT_FRAME_SIZE;		
	ioc->reply_sz = MPT_REPLY_FRAME_SIZE;


	spin_lock_init(&ioc->taskmgmt_lock);
	mutex_init(&ioc->internal_cmds.mutex);
	init_completion(&ioc->internal_cmds.done);
	mutex_init(&ioc->mptbase_cmds.mutex);
	init_completion(&ioc->mptbase_cmds.done);
	mutex_init(&ioc->taskmgmt_cmds.mutex);
	init_completion(&ioc->taskmgmt_cmds.done);

	ioc->eventTypes = 0;	
	ioc->eventContext = 0;
	ioc->eventLogSize = 0;
	ioc->events = NULL;

#ifdef MFCNT
	ioc->mfcnt = 0;
#endif

	ioc->sh = NULL;
	ioc->cached_fw = NULL;

	memset(&ioc->spi_data, 0, sizeof(SpiCfgData));

	INIT_LIST_HEAD(&ioc->fc_rports);

	
	INIT_LIST_HEAD(&ioc->list);


	
	INIT_DELAYED_WORK(&ioc->fault_reset_work, mpt_fault_reset_work);

	snprintf(ioc->reset_work_q_name, MPT_KOBJ_NAME_LEN,
		 "mpt_poll_%d", ioc->id);
	ioc->reset_work_q =
		create_singlethread_workqueue(ioc->reset_work_q_name);
	if (!ioc->reset_work_q) {
		printk(MYIOC_s_ERR_FMT "Insufficient memory to add adapter!\n",
		    ioc->name);
		pci_release_selected_regions(pdev, ioc->bars);
		kfree(ioc);
		return -ENOMEM;
	}

	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT "facts @ %p, pfacts[0] @ %p\n",
	    ioc->name, &ioc->facts, &ioc->pfacts[0]));

	pci_read_config_byte(pdev, PCI_CLASS_REVISION, &revision);
	mpt_get_product_name(pdev->vendor, pdev->device, revision, ioc->prod_name);

	switch (pdev->device)
	{
	case MPI_MANUFACTPAGE_DEVICEID_FC939X:
	case MPI_MANUFACTPAGE_DEVICEID_FC949X:
		ioc->errata_flag_1064 = 1;
	case MPI_MANUFACTPAGE_DEVICEID_FC909:
	case MPI_MANUFACTPAGE_DEVICEID_FC929:
	case MPI_MANUFACTPAGE_DEVICEID_FC919:
	case MPI_MANUFACTPAGE_DEVICEID_FC949E:
		ioc->bus_type = FC;
		break;

	case MPI_MANUFACTPAGE_DEVICEID_FC929X:
		if (revision < XL_929) {
			pci_read_config_byte(pdev, 0x6a, &pcixcmd);
			pcixcmd &= 0x8F;
			pci_write_config_byte(pdev, 0x6a, pcixcmd);
		} else {
			pci_read_config_byte(pdev, 0x6a, &pcixcmd);
			pcixcmd |= 0x08;
			pci_write_config_byte(pdev, 0x6a, pcixcmd);
		}
		ioc->bus_type = FC;
		break;

	case MPI_MANUFACTPAGE_DEVICEID_FC919X:
		pci_read_config_byte(pdev, 0x6a, &pcixcmd);
		pcixcmd &= 0x8F;
		pci_write_config_byte(pdev, 0x6a, pcixcmd);
		ioc->bus_type = FC;
		break;

	case MPI_MANUFACTPAGE_DEVID_53C1030:
		if (revision < C0_1030) {
			pci_read_config_byte(pdev, 0x6a, &pcixcmd);
			pcixcmd &= 0x8F;
			pci_write_config_byte(pdev, 0x6a, pcixcmd);
		}

	case MPI_MANUFACTPAGE_DEVID_1030_53C1035:
		ioc->bus_type = SPI;
		break;

	case MPI_MANUFACTPAGE_DEVID_SAS1064:
	case MPI_MANUFACTPAGE_DEVID_SAS1068:
		ioc->errata_flag_1064 = 1;
		ioc->bus_type = SAS;
		break;

	case MPI_MANUFACTPAGE_DEVID_SAS1064E:
	case MPI_MANUFACTPAGE_DEVID_SAS1068E:
	case MPI_MANUFACTPAGE_DEVID_SAS1078:
		ioc->bus_type = SAS;
		break;
	}


	switch (ioc->bus_type) {

	case SAS:
		ioc->msi_enable = mpt_msi_enable_sas;
		break;

	case SPI:
		ioc->msi_enable = mpt_msi_enable_spi;
		break;

	case FC:
		ioc->msi_enable = mpt_msi_enable_fc;
		break;

	default:
		ioc->msi_enable = 0;
		break;
	}

	ioc->fw_events_off = 1;

	if (ioc->errata_flag_1064)
		pci_disable_io_access(pdev);

	spin_lock_init(&ioc->FreeQlock);

	
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);
	ioc->active = 0;
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	
	pci_set_drvdata(ioc->pcidev, ioc);

	
	list_add_tail(&ioc->list, &ioc_list);

	mpt_detect_bound_ports(ioc, pdev);

	INIT_LIST_HEAD(&ioc->fw_event_list);
	spin_lock_init(&ioc->fw_event_lock);
	snprintf(ioc->fw_event_q_name, MPT_KOBJ_NAME_LEN, "mpt/%d", ioc->id);
	ioc->fw_event_q = create_singlethread_workqueue(ioc->fw_event_q_name);

	if ((r = mpt_do_ioc_recovery(ioc, MPT_HOSTEVENT_IOC_BRINGUP,
	    CAN_SLEEP)) != 0){
		printk(MYIOC_s_ERR_FMT "didn't initialize properly! (%d)\n",
		    ioc->name, r);

		list_del(&ioc->list);
		if (ioc->alt_ioc)
			ioc->alt_ioc->alt_ioc = NULL;
		iounmap(ioc->memmap);
		if (r != -5)
			pci_release_selected_regions(pdev, ioc->bars);

		destroy_workqueue(ioc->reset_work_q);
		ioc->reset_work_q = NULL;

		kfree(ioc);
		pci_set_drvdata(pdev, NULL);
		return r;
	}

	
	for(cb_idx = 0; cb_idx < MPT_MAX_PROTOCOL_DRIVERS; cb_idx++) {
		if(MptDeviceDriverHandlers[cb_idx] &&
		  MptDeviceDriverHandlers[cb_idx]->probe) {
			MptDeviceDriverHandlers[cb_idx]->probe(pdev,id);
		}
	}

#ifdef CONFIG_PROC_FS
	dent = proc_mkdir(ioc->name, mpt_proc_root_dir);
	if (dent) {
		proc_create_data("info", S_IRUGO, dent, &mpt_iocinfo_proc_fops, ioc);
		proc_create_data("summary", S_IRUGO, dent, &mpt_summary_proc_fops, ioc);
	}
#endif

	if (!ioc->alt_ioc)
		queue_delayed_work(ioc->reset_work_q, &ioc->fault_reset_work,
			msecs_to_jiffies(MPT_POLLING_INTERVAL));

	return 0;
}


void
mpt_detach(struct pci_dev *pdev)
{
	MPT_ADAPTER 	*ioc = pci_get_drvdata(pdev);
	char pname[32];
	u8 cb_idx;
	unsigned long flags;
	struct workqueue_struct *wq;

	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	wq = ioc->reset_work_q;
	ioc->reset_work_q = NULL;
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
	cancel_delayed_work(&ioc->fault_reset_work);
	destroy_workqueue(wq);

	spin_lock_irqsave(&ioc->fw_event_lock, flags);
	wq = ioc->fw_event_q;
	ioc->fw_event_q = NULL;
	spin_unlock_irqrestore(&ioc->fw_event_lock, flags);
	destroy_workqueue(wq);

	sprintf(pname, MPT_PROCFS_MPTBASEDIR "/%s/summary", ioc->name);
	remove_proc_entry(pname, NULL);
	sprintf(pname, MPT_PROCFS_MPTBASEDIR "/%s/info", ioc->name);
	remove_proc_entry(pname, NULL);
	sprintf(pname, MPT_PROCFS_MPTBASEDIR "/%s", ioc->name);
	remove_proc_entry(pname, NULL);

	
	for(cb_idx = 0; cb_idx < MPT_MAX_PROTOCOL_DRIVERS; cb_idx++) {
		if(MptDeviceDriverHandlers[cb_idx] &&
		  MptDeviceDriverHandlers[cb_idx]->remove) {
			MptDeviceDriverHandlers[cb_idx]->remove(pdev);
		}
	}

	
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);

	ioc->active = 0;
	synchronize_irq(pdev->irq);

	
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	CHIPREG_READ32(&ioc->chip->IntStatus);

	mpt_adapter_dispose(ioc);

}

#ifdef CONFIG_PM
int
mpt_suspend(struct pci_dev *pdev, pm_message_t state)
{
	u32 device_state;
	MPT_ADAPTER *ioc = pci_get_drvdata(pdev);

	device_state = pci_choose_state(pdev, state);
	printk(MYIOC_s_INFO_FMT "pci-suspend: pdev=0x%p, slot=%s, Entering "
	    "operating state [D%d]\n", ioc->name, pdev, pci_name(pdev),
	    device_state);

	
	if(SendIocReset(ioc, MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET, CAN_SLEEP)) {
		printk(MYIOC_s_ERR_FMT
		"pci-suspend:  IOC msg unit reset failed!\n", ioc->name);
	}

	
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);
	ioc->active = 0;

	
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	free_irq(ioc->pci_irq, ioc);
	if (ioc->msi_enable)
		pci_disable_msi(ioc->pcidev);
	ioc->pci_irq = -1;
	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_release_selected_regions(pdev, ioc->bars);
	pci_set_power_state(pdev, device_state);
	return 0;
}

int
mpt_resume(struct pci_dev *pdev)
{
	MPT_ADAPTER *ioc = pci_get_drvdata(pdev);
	u32 device_state = pdev->current_state;
	int recovery_state;
	int err;

	printk(MYIOC_s_INFO_FMT "pci-resume: pdev=0x%p, slot=%s, Previous "
	    "operating state [D%d]\n", ioc->name, pdev, pci_name(pdev),
	    device_state);

	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);
	ioc->pcidev = pdev;
	err = mpt_mapresources(ioc);
	if (err)
		return err;

	if (ioc->dma_mask == DMA_BIT_MASK(64)) {
		if (pdev->device == MPI_MANUFACTPAGE_DEVID_SAS1078)
			ioc->add_sge = &mpt_add_sge_64bit_1078;
		else
			ioc->add_sge = &mpt_add_sge_64bit;
		ioc->add_chain = &mpt_add_chain_64bit;
		ioc->sg_addr_size = 8;
	} else {

		ioc->add_sge = &mpt_add_sge;
		ioc->add_chain = &mpt_add_chain;
		ioc->sg_addr_size = 4;
	}
	ioc->SGE_size = sizeof(u32) + ioc->sg_addr_size;

	printk(MYIOC_s_INFO_FMT "pci-resume: ioc-state=0x%x,doorbell=0x%x\n",
	    ioc->name, (mpt_GetIocState(ioc, 1) >> MPI_IOC_STATE_SHIFT),
	    CHIPREG_READ32(&ioc->chip->Doorbell));

	if (ioc->bus_type == SAS && (pdev->device ==
	    MPI_MANUFACTPAGE_DEVID_SAS1068E || pdev->device ==
	    MPI_MANUFACTPAGE_DEVID_SAS1064E)) {
		if (KickStart(ioc, 1, CAN_SLEEP) < 0) {
			printk(MYIOC_s_WARN_FMT "pci-resume: Cannot recover\n",
			    ioc->name);
			goto out;
		}
	}

	
	printk(MYIOC_s_INFO_FMT "Sending mpt_do_ioc_recovery\n", ioc->name);
	recovery_state = mpt_do_ioc_recovery(ioc, MPT_HOSTEVENT_IOC_BRINGUP,
						 CAN_SLEEP);
	if (recovery_state != 0)
		printk(MYIOC_s_WARN_FMT "pci-resume: Cannot recover, "
		    "error:[%x]\n", ioc->name, recovery_state);
	else
		printk(MYIOC_s_INFO_FMT
		    "pci-resume: success\n", ioc->name);
 out:
	return 0;

}
#endif

static int
mpt_signal_reset(u8 index, MPT_ADAPTER *ioc, int reset_phase)
{
	if ((MptDriverClass[index] == MPTSPI_DRIVER &&
	     ioc->bus_type != SPI) ||
	    (MptDriverClass[index] == MPTFC_DRIVER &&
	     ioc->bus_type != FC) ||
	    (MptDriverClass[index] == MPTSAS_DRIVER &&
	     ioc->bus_type != SAS))
		return 0;
	return (MptResetHandlers[index])(ioc, reset_phase);
}

static int
mpt_do_ioc_recovery(MPT_ADAPTER *ioc, u32 reason, int sleepFlag)
{
	int	 hard_reset_done = 0;
	int	 alt_ioc_ready = 0;
	int	 hard;
	int	 rc=0;
	int	 ii;
	int	 ret = 0;
	int	 reset_alt_ioc_active = 0;
	int	 irq_allocated = 0;
	u8	*a;

	printk(MYIOC_s_INFO_FMT "Initiating %s\n", ioc->name,
	    reason == MPT_HOSTEVENT_IOC_BRINGUP ? "bringup" : "recovery");

	
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);
	ioc->active = 0;

	if (ioc->alt_ioc) {
		if (ioc->alt_ioc->active ||
		    reason == MPT_HOSTEVENT_IOC_RECOVER) {
			reset_alt_ioc_active = 1;
			CHIPREG_WRITE32(&ioc->alt_ioc->chip->IntMask,
				0xFFFFFFFF);
			ioc->alt_ioc->active = 0;
		}
	}

	hard = 1;
	if (reason == MPT_HOSTEVENT_IOC_BRINGUP)
		hard = 0;

	if ((hard_reset_done = MakeIocReady(ioc, hard, sleepFlag)) < 0) {
		if (hard_reset_done == -4) {
			printk(MYIOC_s_WARN_FMT "Owned by PEER..skipping!\n",
			    ioc->name);

			if (reset_alt_ioc_active && ioc->alt_ioc) {
				
				dprintk(ioc, printk(MYIOC_s_INFO_FMT
				    "alt_ioc reply irq re-enabled\n", ioc->alt_ioc->name));
				CHIPREG_WRITE32(&ioc->alt_ioc->chip->IntMask, MPI_HIM_DIM);
				ioc->alt_ioc->active = 1;
			}

		} else {
			printk(MYIOC_s_WARN_FMT
			    "NOT READY WARNING!\n", ioc->name);
		}
		ret = -1;
		goto out;
	}

	if (hard_reset_done && reset_alt_ioc_active && ioc->alt_ioc) {
		if ((rc = MakeIocReady(ioc->alt_ioc, 0, sleepFlag)) == 0)
			alt_ioc_ready = 1;
		else
			printk(MYIOC_s_WARN_FMT
			    ": alt-ioc Not ready WARNING!\n",
			    ioc->alt_ioc->name);
	}

	for (ii=0; ii<5; ii++) {
		
		if ((rc = GetIocFacts(ioc, sleepFlag, reason)) == 0)
			break;
	}


	if (ii == 5) {
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "Retry IocFacts failed rc=%x\n", ioc->name, rc));
		ret = -2;
	} else if (reason == MPT_HOSTEVENT_IOC_BRINGUP) {
		MptDisplayIocCapabilities(ioc);
	}

	if (alt_ioc_ready) {
		if ((rc = GetIocFacts(ioc->alt_ioc, sleepFlag, reason)) != 0) {
			dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			    "Initial Alt IocFacts failed rc=%x\n",
			    ioc->name, rc));
			rc = GetIocFacts(ioc->alt_ioc, sleepFlag, reason);
		}
		if (rc) {
			dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			    "Retry Alt IocFacts failed rc=%x\n", ioc->name, rc));
			alt_ioc_ready = 0;
			reset_alt_ioc_active = 0;
		} else if (reason == MPT_HOSTEVENT_IOC_BRINGUP) {
			MptDisplayIocCapabilities(ioc->alt_ioc);
		}
	}

	if ((ret == 0) && (reason == MPT_HOSTEVENT_IOC_BRINGUP) &&
	    (ioc->facts.Flags & MPI_IOCFACTS_FLAGS_FW_DOWNLOAD_BOOT)) {
		pci_release_selected_regions(ioc->pcidev, ioc->bars);
		ioc->bars = pci_select_bars(ioc->pcidev, IORESOURCE_MEM |
		    IORESOURCE_IO);
		if (pci_enable_device(ioc->pcidev))
			return -5;
		if (pci_request_selected_regions(ioc->pcidev, ioc->bars,
			"mpt"))
			return -5;
	}

	if ((ret == 0) && (reason == MPT_HOSTEVENT_IOC_BRINGUP)) {
		ioc->pci_irq = -1;
		if (ioc->pcidev->irq) {
			if (ioc->msi_enable && !pci_enable_msi(ioc->pcidev))
				printk(MYIOC_s_INFO_FMT "PCI-MSI enabled\n",
				    ioc->name);
			else
				ioc->msi_enable = 0;
			rc = request_irq(ioc->pcidev->irq, mpt_interrupt,
			    IRQF_SHARED, ioc->name, ioc);
			if (rc < 0) {
				printk(MYIOC_s_ERR_FMT "Unable to allocate "
				    "interrupt %d!\n",
				    ioc->name, ioc->pcidev->irq);
				if (ioc->msi_enable)
					pci_disable_msi(ioc->pcidev);
				ret = -EBUSY;
				goto out;
			}
			irq_allocated = 1;
			ioc->pci_irq = ioc->pcidev->irq;
			pci_set_master(ioc->pcidev);		
			pci_set_drvdata(ioc->pcidev, ioc);
			dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
			    "installed at interrupt %d\n", ioc->name,
			    ioc->pcidev->irq));
		}
	}

	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT "PrimeIocFifos\n",
	    ioc->name));
	if ((ret == 0) && ((rc = PrimeIocFifos(ioc)) != 0))
		ret = -3;

	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT "SendIocInit\n",
	    ioc->name));
	if ((ret == 0) && ((rc = SendIocInit(ioc, sleepFlag)) != 0))
		ret = -4;
	if (alt_ioc_ready && ((rc = PrimeIocFifos(ioc->alt_ioc)) != 0)) {
		printk(MYIOC_s_WARN_FMT
		    ": alt-ioc (%d) FIFO mgmt alloc WARNING!\n",
		    ioc->alt_ioc->name, rc);
		alt_ioc_ready = 0;
		reset_alt_ioc_active = 0;
	}

	if (alt_ioc_ready) {
		if ((rc = SendIocInit(ioc->alt_ioc, sleepFlag)) != 0) {
			alt_ioc_ready = 0;
			reset_alt_ioc_active = 0;
			printk(MYIOC_s_WARN_FMT
				": alt-ioc: (%d) init failure WARNING!\n",
					ioc->alt_ioc->name, rc);
		}
	}

	if (reason == MPT_HOSTEVENT_IOC_BRINGUP){
		if (ioc->upload_fw) {
			ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			    "firmware upload required!\n", ioc->name));

			if (ret == 0) {
				rc = mpt_do_upload(ioc, sleepFlag);
				if (rc == 0) {
					if (ioc->alt_ioc && ioc->alt_ioc->cached_fw) {
						ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
						    "mpt_upload:  alt_%s has cached_fw=%p \n",
						    ioc->name, ioc->alt_ioc->name, ioc->alt_ioc->cached_fw));
						ioc->cached_fw = NULL;
					}
				} else {
					printk(MYIOC_s_WARN_FMT
					    "firmware upload failure!\n", ioc->name);
					ret = -6;
				}
			}
		}
	}

	if ((ret == 0) && (!ioc->facts.EventState)) {
		dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
			"SendEventNotification\n",
		    ioc->name));
		ret = SendEventNotification(ioc, 1, sleepFlag);	
	}

	if (ioc->alt_ioc && alt_ioc_ready && !ioc->alt_ioc->facts.EventState)
		rc = SendEventNotification(ioc->alt_ioc, 1, sleepFlag);

	if (ret == 0) {
		
		CHIPREG_WRITE32(&ioc->chip->IntMask, MPI_HIM_DIM);
		ioc->active = 1;
	}
	if (rc == 0) {	
		if (reset_alt_ioc_active && ioc->alt_ioc) {
			
			dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "alt-ioc"
				"reply irq re-enabled\n",
				ioc->alt_ioc->name));
			CHIPREG_WRITE32(&ioc->alt_ioc->chip->IntMask,
				MPI_HIM_DIM);
			ioc->alt_ioc->active = 1;
		}
	}


	if ((ret == 0) && (reason == MPT_HOSTEVENT_IOC_BRINGUP)) {

		mutex_init(&ioc->raid_data.inactive_list_mutex);
		INIT_LIST_HEAD(&ioc->raid_data.inactive_list);

		switch (ioc->bus_type) {

		case SAS:
			
			if(ioc->facts.IOCExceptions &
			    MPI_IOCFACTS_EXCEPT_PERSISTENT_TABLE_FULL) {
				ret = mptbase_sas_persist_operation(ioc,
				    MPI_SAS_OP_CLEAR_NOT_PRESENT);
				if(ret != 0)
					goto out;
			}

			mpt_findImVolumes(ioc);

			mpt_read_ioc_pg_1(ioc);

			break;

		case FC:
			if ((ioc->pfacts[0].ProtocolFlags &
				MPI_PORTFACTS_PROTOCOL_LAN) &&
			    (ioc->lan_cnfg_page0.Header.PageLength == 0)) {
				(void) GetLanConfigPages(ioc);
				a = (u8*)&ioc->lan_cnfg_page1.HardwareAddressLow;
				dprintk(ioc, printk(MYIOC_s_DEBUG_FMT
					"LanAddr = %02X:%02X:%02X"
					":%02X:%02X:%02X\n",
					ioc->name, a[5], a[4],
					a[3], a[2], a[1], a[0]));
			}
			break;

		case SPI:
			mpt_GetScsiPortSettings(ioc, 0);

			mpt_readScsiDevicePageHeaders(ioc, 0);

			if (ioc->facts.MsgVersion >= MPI_VERSION_01_02)
				mpt_findImVolumes(ioc);

			mpt_read_ioc_pg_1(ioc);

			mpt_read_ioc_pg_4(ioc);

			break;
		}

		GetIoUnitPage2(ioc);
		mpt_get_manufacturing_pg_0(ioc);
	}

 out:
	if ((ret != 0) && irq_allocated) {
		free_irq(ioc->pci_irq, ioc);
		if (ioc->msi_enable)
			pci_disable_msi(ioc->pcidev);
	}
	return ret;
}

static void
mpt_detect_bound_ports(MPT_ADAPTER *ioc, struct pci_dev *pdev)
{
	struct pci_dev *peer=NULL;
	unsigned int slot = PCI_SLOT(pdev->devfn);
	unsigned int func = PCI_FUNC(pdev->devfn);
	MPT_ADAPTER *ioc_srch;

	dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "PCI device %s devfn=%x/%x,"
	    " searching for devfn match on %x or %x\n",
	    ioc->name, pci_name(pdev), pdev->bus->number,
	    pdev->devfn, func-1, func+1));

	peer = pci_get_slot(pdev->bus, PCI_DEVFN(slot,func-1));
	if (!peer) {
		peer = pci_get_slot(pdev->bus, PCI_DEVFN(slot,func+1));
		if (!peer)
			return;
	}

	list_for_each_entry(ioc_srch, &ioc_list, list) {
		struct pci_dev *_pcidev = ioc_srch->pcidev;
		if (_pcidev == peer) {
			
			if (ioc->alt_ioc != NULL) {
				printk(MYIOC_s_WARN_FMT
				    "Oops, already bound (%s <==> %s)!\n",
				    ioc->name, ioc->name, ioc->alt_ioc->name);
				break;
			} else if (ioc_srch->alt_ioc != NULL) {
				printk(MYIOC_s_WARN_FMT
				    "Oops, already bound (%s <==> %s)!\n",
				    ioc_srch->name, ioc_srch->name,
				    ioc_srch->alt_ioc->name);
				break;
			}
			dprintk(ioc, printk(MYIOC_s_DEBUG_FMT
				"FOUND! binding %s <==> %s\n",
				ioc->name, ioc->name, ioc_srch->name));
			ioc_srch->alt_ioc = ioc;
			ioc->alt_ioc = ioc_srch;
		}
	}
	pci_dev_put(peer);
}

static void
mpt_adapter_disable(MPT_ADAPTER *ioc)
{
	int sz;
	int ret;

	if (ioc->cached_fw != NULL) {
		ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			"%s: Pushing FW onto adapter\n", __func__, ioc->name));
		if ((ret = mpt_downloadboot(ioc, (MpiFwHeader_t *)
		    ioc->cached_fw, CAN_SLEEP)) < 0) {
			printk(MYIOC_s_WARN_FMT
			    ": firmware downloadboot failure (%d)!\n",
			    ioc->name, ret);
		}
	}

	if (mpt_GetIocState(ioc, 1) != MPI_IOC_STATE_READY) {
		if (!SendIocReset(ioc, MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET,
		    CAN_SLEEP)) {
			if (mpt_GetIocState(ioc, 1) != MPI_IOC_STATE_READY)
				printk(MYIOC_s_ERR_FMT "%s:  IOC msg unit "
				    "reset failed to put ioc in ready state!\n",
				    ioc->name, __func__);
		} else
			printk(MYIOC_s_ERR_FMT "%s:  IOC msg unit reset "
			    "failed!\n", ioc->name, __func__);
	}


	
	synchronize_irq(ioc->pcidev->irq);
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);
	ioc->active = 0;

	
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);
	CHIPREG_READ32(&ioc->chip->IntStatus);

	if (ioc->alloc != NULL) {
		sz = ioc->alloc_sz;
		dexitprintk(ioc, printk(MYIOC_s_INFO_FMT "free  @ %p, sz=%d bytes\n",
		    ioc->name, ioc->alloc, ioc->alloc_sz));
		pci_free_consistent(ioc->pcidev, sz,
				ioc->alloc, ioc->alloc_dma);
		ioc->reply_frames = NULL;
		ioc->req_frames = NULL;
		ioc->alloc = NULL;
		ioc->alloc_total -= sz;
	}

	if (ioc->sense_buf_pool != NULL) {
		sz = (ioc->req_depth * MPT_SENSE_BUFFER_ALLOC);
		pci_free_consistent(ioc->pcidev, sz,
				ioc->sense_buf_pool, ioc->sense_buf_pool_dma);
		ioc->sense_buf_pool = NULL;
		ioc->alloc_total -= sz;
	}

	if (ioc->events != NULL){
		sz = MPTCTL_EVENT_LOG_SIZE * sizeof(MPT_IOCTL_EVENTS);
		kfree(ioc->events);
		ioc->events = NULL;
		ioc->alloc_total -= sz;
	}

	mpt_free_fw_memory(ioc);

	kfree(ioc->spi_data.nvram);
	mpt_inactive_raid_list_free(ioc);
	kfree(ioc->raid_data.pIocPg2);
	kfree(ioc->raid_data.pIocPg3);
	ioc->spi_data.nvram = NULL;
	ioc->raid_data.pIocPg3 = NULL;

	if (ioc->spi_data.pIocPg4 != NULL) {
		sz = ioc->spi_data.IocPg4Sz;
		pci_free_consistent(ioc->pcidev, sz,
			ioc->spi_data.pIocPg4,
			ioc->spi_data.IocPg4_dma);
		ioc->spi_data.pIocPg4 = NULL;
		ioc->alloc_total -= sz;
	}

	if (ioc->ReqToChain != NULL) {
		kfree(ioc->ReqToChain);
		kfree(ioc->RequestNB);
		ioc->ReqToChain = NULL;
	}

	kfree(ioc->ChainToChain);
	ioc->ChainToChain = NULL;

	if (ioc->HostPageBuffer != NULL) {
		if((ret = mpt_host_page_access_control(ioc,
		    MPI_DB_HPBAC_FREE_BUFFER, NO_SLEEP)) != 0) {
			printk(MYIOC_s_ERR_FMT
			   ": %s: host page buffers free failed (%d)!\n",
			    ioc->name, __func__, ret);
		}
		dexitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			"HostPageBuffer free  @ %p, sz=%d bytes\n",
			ioc->name, ioc->HostPageBuffer,
			ioc->HostPageBuffer_sz));
		pci_free_consistent(ioc->pcidev, ioc->HostPageBuffer_sz,
		    ioc->HostPageBuffer, ioc->HostPageBuffer_dma);
		ioc->HostPageBuffer = NULL;
		ioc->HostPageBuffer_sz = 0;
		ioc->alloc_total -= ioc->HostPageBuffer_sz;
	}

	pci_set_drvdata(ioc->pcidev, NULL);
}
static void
mpt_adapter_dispose(MPT_ADAPTER *ioc)
{
	int sz_first, sz_last;

	if (ioc == NULL)
		return;

	sz_first = ioc->alloc_total;

	mpt_adapter_disable(ioc);

	if (ioc->pci_irq != -1) {
		free_irq(ioc->pci_irq, ioc);
		if (ioc->msi_enable)
			pci_disable_msi(ioc->pcidev);
		ioc->pci_irq = -1;
	}

	if (ioc->memmap != NULL) {
		iounmap(ioc->memmap);
		ioc->memmap = NULL;
	}

	pci_disable_device(ioc->pcidev);
	pci_release_selected_regions(ioc->pcidev, ioc->bars);

#if defined(CONFIG_MTRR) && 0
	if (ioc->mtrr_reg > 0) {
		mtrr_del(ioc->mtrr_reg, 0, 0);
		dprintk(ioc, printk(MYIOC_s_INFO_FMT "MTRR region de-registered\n", ioc->name));
	}
#endif

	
	list_del(&ioc->list);

	sz_last = ioc->alloc_total;
	dprintk(ioc, printk(MYIOC_s_INFO_FMT "free'd %d of %d bytes\n",
	    ioc->name, sz_first-sz_last+(int)sizeof(*ioc), sz_first));

	if (ioc->alt_ioc)
		ioc->alt_ioc->alt_ioc = NULL;

	kfree(ioc);
}

static void
MptDisplayIocCapabilities(MPT_ADAPTER *ioc)
{
	int i = 0;

	printk(KERN_INFO "%s: ", ioc->name);
	if (ioc->prod_name)
		printk("%s: ", ioc->prod_name);
	printk("Capabilities={");

	if (ioc->pfacts[0].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_INITIATOR) {
		printk("Initiator");
		i++;
	}

	if (ioc->pfacts[0].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_TARGET) {
		printk("%sTarget", i ? "," : "");
		i++;
	}

	if (ioc->pfacts[0].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_LAN) {
		printk("%sLAN", i ? "," : "");
		i++;
	}

#if 0
	if (ioc->pfacts[0].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_TARGET) {
		printk("%sLogBusAddr", i ? "," : "");
		i++;
	}
#endif

	printk("}\n");
}

static int
MakeIocReady(MPT_ADAPTER *ioc, int force, int sleepFlag)
{
	u32	 ioc_state;
	int	 statefault = 0;
	int	 cntdn;
	int	 hard_reset_done = 0;
	int	 r;
	int	 ii;
	int	 whoinit;

	
	ioc_state = mpt_GetIocState(ioc, 0);
	dhsprintk(ioc, printk(MYIOC_s_INFO_FMT "MakeIocReady [raw] state=%08x\n", ioc->name, ioc_state));

	if (ioc_state & MPI_DOORBELL_ACTIVE) {
		statefault = 1;
		printk(MYIOC_s_WARN_FMT "Unexpected doorbell active!\n",
				ioc->name);
	}

	
	if (!statefault &&
	    ((ioc_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_READY)) {
		dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
		    "IOC is in READY state\n", ioc->name));
		return 0;
	}

	if ((ioc_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_FAULT) {
		statefault = 2;
		printk(MYIOC_s_WARN_FMT "IOC is in FAULT state!!!\n",
		    ioc->name);
		printk(MYIOC_s_WARN_FMT "           FAULT code = %04xh\n",
		    ioc->name, ioc_state & MPI_DOORBELL_DATA_MASK);
	}

	if ((ioc_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_OPERATIONAL) {
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "IOC operational unexpected\n",
				ioc->name));

		whoinit = (ioc_state & MPI_DOORBELL_WHO_INIT_MASK) >> MPI_DOORBELL_WHO_INIT_SHIFT;
		dinitprintk(ioc, printk(MYIOC_s_INFO_FMT
			"whoinit 0x%x statefault %d force %d\n",
			ioc->name, whoinit, statefault, force));
		if (whoinit == MPI_WHOINIT_PCI_PEER)
			return -4;
		else {
			if ((statefault == 0 ) && (force == 0)) {
				if ((r = SendIocReset(ioc, MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET, sleepFlag)) == 0)
					return 0;
			}
			statefault = 3;
		}
	}

	hard_reset_done = KickStart(ioc, statefault||force, sleepFlag);
	if (hard_reset_done < 0)
		return -1;

	ii = 0;
	cntdn = ((sleepFlag == CAN_SLEEP) ? HZ : 1000) * 5;	

	while ((ioc_state = mpt_GetIocState(ioc, 1)) != MPI_IOC_STATE_READY) {
		if (ioc_state == MPI_IOC_STATE_OPERATIONAL) {
			if ((r = SendIocReset(ioc, MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET, sleepFlag)) != 0) {
				printk(MYIOC_s_ERR_FMT "IOC msg unit reset failed!\n", ioc->name);
				return -2;
			}
		} else if (ioc_state == MPI_IOC_STATE_RESET) {
			if ((r = SendIocReset(ioc, MPI_FUNCTION_IO_UNIT_RESET, sleepFlag)) != 0) {
				printk(MYIOC_s_ERR_FMT "IO unit reset failed!\n", ioc->name);
				return -3;
			}
		}

		ii++; cntdn--;
		if (!cntdn) {
			printk(MYIOC_s_ERR_FMT
				"Wait IOC_READY state (0x%x) timeout(%d)!\n",
				ioc->name, ioc_state, (int)((ii+5)/HZ));
			return -ETIME;
		}

		if (sleepFlag == CAN_SLEEP) {
			msleep(1);
		} else {
			mdelay (1);	
		}

	}

	if (statefault < 3) {
		printk(MYIOC_s_INFO_FMT "Recovered from %s\n", ioc->name,
			statefault == 1 ? "stuck handshake" : "IOC FAULT");
	}

	return hard_reset_done;
}

u32
mpt_GetIocState(MPT_ADAPTER *ioc, int cooked)
{
	u32 s, sc;

	
	s = CHIPREG_READ32(&ioc->chip->Doorbell);
	sc = s & MPI_IOC_STATE_MASK;

	
	ioc->last_state = sc;

	return cooked ? sc : s;
}

static int
GetIocFacts(MPT_ADAPTER *ioc, int sleepFlag, int reason)
{
	IOCFacts_t		 get_facts;
	IOCFactsReply_t		*facts;
	int			 r;
	int			 req_sz;
	int			 reply_sz;
	int			 sz;
	u32			 status, vv;
	u8			 shiftFactor=1;

	
	if (ioc->last_state == MPI_IOC_STATE_RESET) {
		printk(KERN_ERR MYNAM
		    ": ERROR - Can't get IOCFacts, %s NOT READY! (%08x)\n",
		    ioc->name, ioc->last_state);
		return -44;
	}

	facts = &ioc->facts;

	
	reply_sz = sizeof(*facts);
	memset(facts, 0, reply_sz);

	
	req_sz = sizeof(get_facts);
	memset(&get_facts, 0, req_sz);

	get_facts.Function = MPI_FUNCTION_IOC_FACTS;
	

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "Sending get IocFacts request req_sz=%d reply_sz=%d\n",
	    ioc->name, req_sz, reply_sz));

	r = mpt_handshake_req_reply_wait(ioc, req_sz, (u32*)&get_facts,
			reply_sz, (u16*)facts, 5 , sleepFlag);
	if (r != 0)
		return r;

	
	if (facts->MsgLength > offsetof(IOCFactsReply_t, RequestFrameSize)/sizeof(u32)) {
		if (reason == MPT_HOSTEVENT_IOC_BRINGUP) {
			if (ioc->FirstWhoInit == WHOINIT_UNKNOWN)
				ioc->FirstWhoInit = facts->WhoInit;
		}

		facts->MsgVersion = le16_to_cpu(facts->MsgVersion);
		facts->MsgContext = le32_to_cpu(facts->MsgContext);
		facts->IOCExceptions = le16_to_cpu(facts->IOCExceptions);
		facts->IOCStatus = le16_to_cpu(facts->IOCStatus);
		facts->IOCLogInfo = le32_to_cpu(facts->IOCLogInfo);
		status = le16_to_cpu(facts->IOCStatus) & MPI_IOCSTATUS_MASK;
		

		facts->ReplyQueueDepth = le16_to_cpu(facts->ReplyQueueDepth);
		facts->RequestFrameSize = le16_to_cpu(facts->RequestFrameSize);

		if (facts->MsgVersion < MPI_VERSION_01_02) {
			u16	 oldv = le16_to_cpu(facts->Reserved_0101_FWVersion);
			facts->FWVersion.Word =
					((oldv<<12) & 0xFF000000) |
					((oldv<<8)  & 0x000FFF00);
		} else
			facts->FWVersion.Word = le32_to_cpu(facts->FWVersion.Word);

		facts->ProductID = le16_to_cpu(facts->ProductID);

		if ((ioc->facts.ProductID & MPI_FW_HEADER_PID_PROD_MASK)
		    > MPI_FW_HEADER_PID_PROD_TARGET_SCSI)
			ioc->ir_firmware = 1;

		facts->CurrentHostMfaHighAddr =
				le32_to_cpu(facts->CurrentHostMfaHighAddr);
		facts->GlobalCredits = le16_to_cpu(facts->GlobalCredits);
		facts->CurrentSenseBufferHighAddr =
				le32_to_cpu(facts->CurrentSenseBufferHighAddr);
		facts->CurReplyFrameSize =
				le16_to_cpu(facts->CurReplyFrameSize);
		facts->IOCCapabilities = le32_to_cpu(facts->IOCCapabilities);

		if (facts->MsgLength >= (offsetof(IOCFactsReply_t,FWImageSize) + 7)/4 &&
		    facts->MsgVersion > MPI_VERSION_01_00) {
			facts->FWImageSize = le32_to_cpu(facts->FWImageSize);
		}

		sz = facts->FWImageSize;
		if ( sz & 0x01 )
			sz += 1;
		if ( sz & 0x02 )
			sz += 2;
		facts->FWImageSize = sz;

		if (!facts->RequestFrameSize) {
			
			printk(MYIOC_s_ERR_FMT "IOC reported invalid 0 request size!\n",
					ioc->name);
			return -55;
		}

		r = sz = facts->BlockSize;
		vv = ((63 / (sz * 4)) + 1) & 0x03;
		ioc->NB_for_64_byte_frame = vv;
		while ( sz )
		{
			shiftFactor++;
			sz = sz >> 1;
		}
		ioc->NBShiftFactor  = shiftFactor;
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "NB_for_64_byte_frame=%x NBShiftFactor=%x BlockSize=%x\n",
		    ioc->name, vv, shiftFactor, r));

		if (reason == MPT_HOSTEVENT_IOC_BRINGUP) {
			ioc->req_sz = min(MPT_DEFAULT_FRAME_SIZE, facts->RequestFrameSize * 4);
			ioc->req_depth = min_t(int, MPT_MAX_REQ_DEPTH, facts->GlobalCredits);
			ioc->reply_sz = MPT_REPLY_FRAME_SIZE;
			ioc->reply_depth = min_t(int, MPT_DEFAULT_REPLY_DEPTH, facts->ReplyQueueDepth);

			dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "reply_sz=%3d, reply_depth=%4d\n",
				ioc->name, ioc->reply_sz, ioc->reply_depth));
			dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "req_sz  =%3d, req_depth  =%4d\n",
				ioc->name, ioc->req_sz, ioc->req_depth));

			
			if ( (r = GetPortFacts(ioc, 0, sleepFlag)) != 0 )
				return r;
		}
	} else {
		printk(MYIOC_s_ERR_FMT
		     "Invalid IOC facts reply, msgLength=%d offsetof=%zd!\n",
		     ioc->name, facts->MsgLength, (offsetof(IOCFactsReply_t,
		     RequestFrameSize)/sizeof(u32)));
		return -66;
	}

	return 0;
}

static int
GetPortFacts(MPT_ADAPTER *ioc, int portnum, int sleepFlag)
{
	PortFacts_t		 get_pfacts;
	PortFactsReply_t	*pfacts;
	int			 ii;
	int			 req_sz;
	int			 reply_sz;
	int			 max_id;

	
	if (ioc->last_state == MPI_IOC_STATE_RESET) {
		printk(MYIOC_s_ERR_FMT "Can't get PortFacts NOT READY! (%08x)\n",
		    ioc->name, ioc->last_state );
		return -4;
	}

	pfacts = &ioc->pfacts[portnum];

	
	reply_sz = sizeof(*pfacts);
	memset(pfacts, 0, reply_sz);

	
	req_sz = sizeof(get_pfacts);
	memset(&get_pfacts, 0, req_sz);

	get_pfacts.Function = MPI_FUNCTION_PORT_FACTS;
	get_pfacts.PortNumber = portnum;
	

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending get PortFacts(%d) request\n",
			ioc->name, portnum));

	ii = mpt_handshake_req_reply_wait(ioc, req_sz, (u32*)&get_pfacts,
				reply_sz, (u16*)pfacts, 5 , sleepFlag);
	if (ii != 0)
		return ii;

	

	
	pfacts->MsgContext = le32_to_cpu(pfacts->MsgContext);
	pfacts->IOCStatus = le16_to_cpu(pfacts->IOCStatus);
	pfacts->IOCLogInfo = le32_to_cpu(pfacts->IOCLogInfo);
	pfacts->MaxDevices = le16_to_cpu(pfacts->MaxDevices);
	pfacts->PortSCSIID = le16_to_cpu(pfacts->PortSCSIID);
	pfacts->ProtocolFlags = le16_to_cpu(pfacts->ProtocolFlags);
	pfacts->MaxPostedCmdBuffers = le16_to_cpu(pfacts->MaxPostedCmdBuffers);
	pfacts->MaxPersistentIDs = le16_to_cpu(pfacts->MaxPersistentIDs);
	pfacts->MaxLanBuckets = le16_to_cpu(pfacts->MaxLanBuckets);

	max_id = (ioc->bus_type == SAS) ? pfacts->PortSCSIID :
	    pfacts->MaxDevices;
	ioc->devices_per_bus = (max_id > 255) ? 256 : max_id;
	ioc->number_of_buses = (ioc->devices_per_bus < 256) ? 1 : max_id/256;

	if (mpt_channel_mapping) {
		ioc->devices_per_bus = 1;
		ioc->number_of_buses = (max_id > 255) ? 255 : max_id;
	}

	return 0;
}

static int
SendIocInit(MPT_ADAPTER *ioc, int sleepFlag)
{
	IOCInit_t		 ioc_init;
	MPIDefaultReply_t	 init_reply;
	u32			 state;
	int			 r;
	int			 count;
	int			 cntdn;

	memset(&ioc_init, 0, sizeof(ioc_init));
	memset(&init_reply, 0, sizeof(init_reply));

	ioc_init.WhoInit = MPI_WHOINIT_HOST_DRIVER;
	ioc_init.Function = MPI_FUNCTION_IOC_INIT;

	if (ioc->facts.Flags & MPI_IOCFACTS_FLAGS_FW_DOWNLOAD_BOOT)
		ioc->upload_fw = 1;
	else
		ioc->upload_fw = 0;
	ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "upload_fw %d facts.Flags=%x\n",
		   ioc->name, ioc->upload_fw, ioc->facts.Flags));

	ioc_init.MaxDevices = (U8)ioc->devices_per_bus;
	ioc_init.MaxBuses = (U8)ioc->number_of_buses;

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "facts.MsgVersion=%x\n",
		   ioc->name, ioc->facts.MsgVersion));
	if (ioc->facts.MsgVersion >= MPI_VERSION_01_05) {
		
		ioc_init.MsgVersion = cpu_to_le16(MPI_VERSION);
	        ioc_init.HeaderVersion = cpu_to_le16(MPI_HEADER_VERSION);

		if (ioc->facts.Flags & MPI_IOCFACTS_FLAGS_HOST_PAGE_BUFFER_PERSISTENT) {
			ioc_init.HostPageBufferSGE = ioc->facts.HostPageBufferSGE;
		} else if(mpt_host_page_alloc(ioc, &ioc_init))
			return -99;
	}
	ioc_init.ReplyFrameSize = cpu_to_le16(ioc->reply_sz);	

	if (ioc->sg_addr_size == sizeof(u64)) {
		ioc_init.HostMfaHighAddr = cpu_to_le32((u32)((u64)ioc->alloc_dma >> 32));
		ioc_init.SenseBufferHighAddr = cpu_to_le32((u32)((u64)ioc->sense_buf_pool_dma >> 32));
	} else {
		
		ioc_init.HostMfaHighAddr = cpu_to_le32(0);
		ioc_init.SenseBufferHighAddr = cpu_to_le32(0);
	}

	ioc->facts.CurrentHostMfaHighAddr = ioc_init.HostMfaHighAddr;
	ioc->facts.CurrentSenseBufferHighAddr = ioc_init.SenseBufferHighAddr;
	ioc->facts.MaxDevices = ioc_init.MaxDevices;
	ioc->facts.MaxBuses = ioc_init.MaxBuses;

	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending IOCInit (req @ %p)\n",
			ioc->name, &ioc_init));

	r = mpt_handshake_req_reply_wait(ioc, sizeof(IOCInit_t), (u32*)&ioc_init,
				sizeof(MPIDefaultReply_t), (u16*)&init_reply, 10 , sleepFlag);
	if (r != 0) {
		printk(MYIOC_s_ERR_FMT "Sending IOCInit failed(%d)!\n",ioc->name, r);
		return r;
	}


	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending PortEnable (req @ %p)\n",
			ioc->name, &ioc_init));

	if ((r = SendPortEnable(ioc, 0, sleepFlag)) != 0) {
		printk(MYIOC_s_ERR_FMT "Sending PortEnable failed(%d)!\n",ioc->name, r);
		return r;
	}

	count = 0;
	cntdn = ((sleepFlag == CAN_SLEEP) ? HZ : 1000) * 60;	
	state = mpt_GetIocState(ioc, 1);
	while (state != MPI_IOC_STATE_OPERATIONAL && --cntdn) {
		if (sleepFlag == CAN_SLEEP) {
			msleep(1);
		} else {
			mdelay(1);
		}

		if (!cntdn) {
			printk(MYIOC_s_ERR_FMT "Wait IOC_OP state timeout(%d)!\n",
					ioc->name, (int)((count+5)/HZ));
			return -9;
		}

		state = mpt_GetIocState(ioc, 1);
		count++;
	}
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Wait IOC_OPERATIONAL state (cnt=%d)\n",
			ioc->name, count));

	ioc->aen_event_read_flag=0;
	return r;
}

static int
SendPortEnable(MPT_ADAPTER *ioc, int portnum, int sleepFlag)
{
	PortEnable_t		 port_enable;
	MPIDefaultReply_t	 reply_buf;
	int	 rc;
	int	 req_sz;
	int	 reply_sz;

	
	reply_sz = sizeof(MPIDefaultReply_t);
	memset(&reply_buf, 0, reply_sz);

	req_sz = sizeof(PortEnable_t);
	memset(&port_enable, 0, req_sz);

	port_enable.Function = MPI_FUNCTION_PORT_ENABLE;
	port_enable.PortNumber = portnum;

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending Port(%d)Enable (req @ %p)\n",
			ioc->name, portnum, &port_enable));

	if (ioc->ir_firmware || ioc->bus_type == SAS) {
		rc = mpt_handshake_req_reply_wait(ioc, req_sz,
		(u32*)&port_enable, reply_sz, (u16*)&reply_buf,
		300 , sleepFlag);
	} else {
		rc = mpt_handshake_req_reply_wait(ioc, req_sz,
		(u32*)&port_enable, reply_sz, (u16*)&reply_buf,
		30 , sleepFlag);
	}
	return rc;
}

int
mpt_alloc_fw_memory(MPT_ADAPTER *ioc, int size)
{
	int rc;

	if (ioc->cached_fw) {
		rc = 0;  
		goto out;
	}
	else if (ioc->alt_ioc && ioc->alt_ioc->cached_fw) {
		ioc->cached_fw = ioc->alt_ioc->cached_fw;  
		ioc->cached_fw_dma = ioc->alt_ioc->cached_fw_dma;
		rc = 0;
		goto out;
	}
	ioc->cached_fw = pci_alloc_consistent(ioc->pcidev, size, &ioc->cached_fw_dma);
	if (!ioc->cached_fw) {
		printk(MYIOC_s_ERR_FMT "Unable to allocate memory for the cached firmware image!\n",
		    ioc->name);
		rc = -1;
	} else {
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "FW Image  @ %p[%p], sz=%d[%x] bytes\n",
		    ioc->name, ioc->cached_fw, (void *)(ulong)ioc->cached_fw_dma, size, size));
		ioc->alloc_total += size;
		rc = 0;
	}
 out:
	return rc;
}

void
mpt_free_fw_memory(MPT_ADAPTER *ioc)
{
	int sz;

	if (!ioc->cached_fw)
		return;

	sz = ioc->facts.FWImageSize;
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "free_fw_memory: FW Image  @ %p[%p], sz=%d[%x] bytes\n",
		 ioc->name, ioc->cached_fw, (void *)(ulong)ioc->cached_fw_dma, sz, sz));
	pci_free_consistent(ioc->pcidev, sz, ioc->cached_fw, ioc->cached_fw_dma);
	ioc->alloc_total -= sz;
	ioc->cached_fw = NULL;
}

static int
mpt_do_upload(MPT_ADAPTER *ioc, int sleepFlag)
{
	u8			 reply[sizeof(FWUploadReply_t)];
	FWUpload_t		*prequest;
	FWUploadReply_t		*preply;
	FWUploadTCSGE_t		*ptcsge;
	u32			 flagsLength;
	int			 ii, sz, reply_sz;
	int			 cmdStatus;
	int			request_size;
	if ((sz = ioc->facts.FWImageSize) == 0)
		return 0;

	if (mpt_alloc_fw_memory(ioc, ioc->facts.FWImageSize) != 0)
		return -ENOMEM;

	dinitprintk(ioc, printk(MYIOC_s_INFO_FMT ": FW Image  @ %p[%p], sz=%d[%x] bytes\n",
	    ioc->name, ioc->cached_fw, (void *)(ulong)ioc->cached_fw_dma, sz, sz));

	prequest = (sleepFlag == NO_SLEEP) ? kzalloc(ioc->req_sz, GFP_ATOMIC) :
	    kzalloc(ioc->req_sz, GFP_KERNEL);
	if (!prequest) {
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "fw upload failed "
		    "while allocating memory \n", ioc->name));
		mpt_free_fw_memory(ioc);
		return -ENOMEM;
	}

	preply = (FWUploadReply_t *)&reply;

	reply_sz = sizeof(reply);
	memset(preply, 0, reply_sz);

	prequest->ImageType = MPI_FW_UPLOAD_ITYPE_FW_IOC_MEM;
	prequest->Function = MPI_FUNCTION_FW_UPLOAD;

	ptcsge = (FWUploadTCSGE_t *) &prequest->SGL;
	ptcsge->DetailsLength = 12;
	ptcsge->Flags = MPI_SGE_FLAGS_TRANSACTION_ELEMENT;
	ptcsge->ImageSize = cpu_to_le32(sz);
	ptcsge++;

	flagsLength = MPT_SGE_FLAGS_SSIMPLE_READ | sz;
	ioc->add_sge((char *)ptcsge, flagsLength, ioc->cached_fw_dma);
	request_size = offsetof(FWUpload_t, SGL) + sizeof(FWUploadTCSGE_t) +
	    ioc->SGE_size;
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending FW Upload "
	    " (req @ %p) fw_size=%d mf_request_size=%d\n", ioc->name, prequest,
	    ioc->facts.FWImageSize, request_size));
	DBG_DUMP_FW_REQUEST_FRAME(ioc, (u32 *)prequest);

	ii = mpt_handshake_req_reply_wait(ioc, request_size, (u32 *)prequest,
	    reply_sz, (u16 *)preply, 65 , sleepFlag);

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "FW Upload completed "
	    "rc=%x \n", ioc->name, ii));

	cmdStatus = -EFAULT;
	if (ii == 0) {
		int status;
		status = le16_to_cpu(preply->IOCStatus) &
				MPI_IOCSTATUS_MASK;
		if (status == MPI_IOCSTATUS_SUCCESS &&
		    ioc->facts.FWImageSize ==
		    le32_to_cpu(preply->ActualImageSize))
				cmdStatus = 0;
	}
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT ": do_upload cmdStatus=%d \n",
			ioc->name, cmdStatus));


	if (cmdStatus) {
		ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "fw upload failed, "
		    "freeing image \n", ioc->name));
		mpt_free_fw_memory(ioc);
	}
	kfree(prequest);

	return cmdStatus;
}

static int
mpt_downloadboot(MPT_ADAPTER *ioc, MpiFwHeader_t *pFwHeader, int sleepFlag)
{
	MpiExtImageHeader_t	*pExtImage;
	u32			 fwSize;
	u32			 diag0val;
	int			 count;
	u32			*ptrFw;
	u32			 diagRwData;
	u32			 nextImage;
	u32			 load_addr;
	u32 			 ioc_state=0;

	ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "downloadboot: fw size 0x%x (%d), FW Ptr %p\n",
				ioc->name, pFwHeader->ImageSize, pFwHeader->ImageSize, pFwHeader));

	CHIPREG_WRITE32(&ioc->chip->WriteSequence, 0xFF);
	CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_1ST_KEY_VALUE);
	CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_2ND_KEY_VALUE);
	CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_3RD_KEY_VALUE);
	CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_4TH_KEY_VALUE);
	CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_5TH_KEY_VALUE);

	CHIPREG_WRITE32(&ioc->chip->Diagnostic, (MPI_DIAG_PREVENT_IOC_BOOT | MPI_DIAG_DISABLE_ARM));

	
	if (sleepFlag == CAN_SLEEP) {
		msleep(1);
	} else {
		mdelay (1);
	}

	diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);
	CHIPREG_WRITE32(&ioc->chip->Diagnostic, diag0val | MPI_DIAG_RESET_ADAPTER);

	for (count = 0; count < 30; count ++) {
		diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);
		if (!(diag0val & MPI_DIAG_RESET_ADAPTER)) {
			ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "RESET_ADAPTER cleared, count=%d\n",
				ioc->name, count));
			break;
		}
		
		if (sleepFlag == CAN_SLEEP) {
			msleep (100);
		} else {
			mdelay (100);
		}
	}

	if ( count == 30 ) {
		ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "downloadboot failed! "
		"Unable to get MPI_DIAG_DRWE mode, diag0val=%x\n",
		ioc->name, diag0val));
		return -3;
	}

	CHIPREG_WRITE32(&ioc->chip->WriteSequence, 0xFF);
	CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_1ST_KEY_VALUE);
	CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_2ND_KEY_VALUE);
	CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_3RD_KEY_VALUE);
	CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_4TH_KEY_VALUE);
	CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_5TH_KEY_VALUE);

	
	CHIPREG_WRITE32(&ioc->chip->Diagnostic, (MPI_DIAG_RW_ENABLE | MPI_DIAG_DISABLE_ARM));

	fwSize = (pFwHeader->ImageSize + 3)/4;
	ptrFw = (u32 *) pFwHeader;

	if (ioc->errata_flag_1064)
		pci_enable_io_access(ioc->pcidev);

	CHIPREG_PIO_WRITE32(&ioc->pio_chip->DiagRwAddress, pFwHeader->LoadStartAddress);
	ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "LoadStart addr written 0x%x \n",
		ioc->name, pFwHeader->LoadStartAddress));

	ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Write FW Image: 0x%x bytes @ %p\n",
				ioc->name, fwSize*4, ptrFw));
	while (fwSize--) {
		CHIPREG_PIO_WRITE32(&ioc->pio_chip->DiagRwData, *ptrFw++);
	}

	nextImage = pFwHeader->NextImageHeaderOffset;
	while (nextImage) {
		pExtImage = (MpiExtImageHeader_t *) ((char *)pFwHeader + nextImage);

		load_addr = pExtImage->LoadStartAddress;

		fwSize = (pExtImage->ImageSize + 3) >> 2;
		ptrFw = (u32 *)pExtImage;

		ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Write Ext Image: 0x%x (%d) bytes @ %p load_addr=%x\n",
						ioc->name, fwSize*4, fwSize*4, ptrFw, load_addr));
		CHIPREG_PIO_WRITE32(&ioc->pio_chip->DiagRwAddress, load_addr);

		while (fwSize--) {
			CHIPREG_PIO_WRITE32(&ioc->pio_chip->DiagRwData, *ptrFw++);
		}
		nextImage = pExtImage->NextImageHeaderOffset;
	}

	
	ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Write IopResetVector Addr=%x! \n", ioc->name, 	pFwHeader->IopResetRegAddr));
	CHIPREG_PIO_WRITE32(&ioc->pio_chip->DiagRwAddress, pFwHeader->IopResetRegAddr);

	
	ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Write IopResetVector Value=%x! \n", ioc->name, pFwHeader->IopResetVectorValue));
	CHIPREG_PIO_WRITE32(&ioc->pio_chip->DiagRwData, pFwHeader->IopResetVectorValue);

	if (ioc->bus_type == SPI) {
		CHIPREG_PIO_WRITE32(&ioc->pio_chip->DiagRwAddress, 0x3F000000);
		diagRwData = CHIPREG_PIO_READ32(&ioc->pio_chip->DiagRwData);
		diagRwData |= 0x40000000;
		CHIPREG_PIO_WRITE32(&ioc->pio_chip->DiagRwAddress, 0x3F000000);
		CHIPREG_PIO_WRITE32(&ioc->pio_chip->DiagRwData, diagRwData);

	} else  {
		diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);
		CHIPREG_WRITE32(&ioc->chip->Diagnostic, diag0val |
		    MPI_DIAG_CLEAR_FLASH_BAD_SIG);

		
		if (sleepFlag == CAN_SLEEP) {
			msleep (1);
		} else {
			mdelay (1);
		}
	}

	if (ioc->errata_flag_1064)
		pci_disable_io_access(ioc->pcidev);

	diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);
	ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "downloadboot diag0val=%x, "
		"turning off PREVENT_IOC_BOOT, DISABLE_ARM, RW_ENABLE\n",
		ioc->name, diag0val));
	diag0val &= ~(MPI_DIAG_PREVENT_IOC_BOOT | MPI_DIAG_DISABLE_ARM | MPI_DIAG_RW_ENABLE);
	ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "downloadboot now diag0val=%x\n",
		ioc->name, diag0val));
	CHIPREG_WRITE32(&ioc->chip->Diagnostic, diag0val);

	
	CHIPREG_WRITE32(&ioc->chip->WriteSequence, 0xFF);

	if (ioc->bus_type == SAS) {
		ioc_state = mpt_GetIocState(ioc, 0);
		if ( (GetIocFacts(ioc, sleepFlag,
				MPT_HOSTEVENT_IOC_BRINGUP)) != 0 ) {
			ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT "GetIocFacts failed: IocState=%x\n",
					ioc->name, ioc_state));
			return -EFAULT;
		}
	}

	for (count=0; count<HZ*20; count++) {
		if ((ioc_state = mpt_GetIocState(ioc, 0)) & MPI_IOC_STATE_READY) {
			ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
				"downloadboot successful! (count=%d) IocState=%x\n",
				ioc->name, count, ioc_state));
			if (ioc->bus_type == SAS) {
				return 0;
			}
			if ((SendIocInit(ioc, sleepFlag)) != 0) {
				ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
					"downloadboot: SendIocInit failed\n",
					ioc->name));
				return -EFAULT;
			}
			ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
					"downloadboot: SendIocInit successful\n",
					ioc->name));
			return 0;
		}
		if (sleepFlag == CAN_SLEEP) {
			msleep (10);
		} else {
			mdelay (10);
		}
	}
	ddlprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		"downloadboot failed! IocState=%x\n",ioc->name, ioc_state));
	return -EFAULT;
}

static int
KickStart(MPT_ADAPTER *ioc, int force, int sleepFlag)
{
	int hard_reset_done = 0;
	u32 ioc_state=0;
	int cnt,cntdn;

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "KickStarting!\n", ioc->name));
	if (ioc->bus_type == SPI) {
		SendIocReset(ioc, MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET, sleepFlag);

		if (sleepFlag == CAN_SLEEP) {
			msleep (1000);
		} else {
			mdelay (1000);
		}
	}

	hard_reset_done = mpt_diag_reset(ioc, force, sleepFlag);
	if (hard_reset_done < 0)
		return hard_reset_done;

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Diagnostic reset successful!\n",
		ioc->name));

	cntdn = ((sleepFlag == CAN_SLEEP) ? HZ : 1000) * 2;	
	for (cnt=0; cnt<cntdn; cnt++) {
		ioc_state = mpt_GetIocState(ioc, 1);
		if ((ioc_state == MPI_IOC_STATE_READY) || (ioc_state == MPI_IOC_STATE_OPERATIONAL)) {
			dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "KickStart successful! (cnt=%d)\n",
 					ioc->name, cnt));
			return hard_reset_done;
		}
		if (sleepFlag == CAN_SLEEP) {
			msleep (10);
		} else {
			mdelay (10);
		}
	}

	dinitprintk(ioc, printk(MYIOC_s_ERR_FMT "Failed to come READY after reset! IocState=%x\n",
		ioc->name, mpt_GetIocState(ioc, 0)));
	return -1;
}

static int
mpt_diag_reset(MPT_ADAPTER *ioc, int ignore, int sleepFlag)
{
	u32 diag0val;
	u32 doorbell;
	int hard_reset_done = 0;
	int count = 0;
	u32 diag1val = 0;
	MpiFwHeader_t *cached_fw;	
	u8	 cb_idx;

	
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	if (ioc->pcidev->device == MPI_MANUFACTPAGE_DEVID_SAS1078) {

		if (!ignore)
			return 0;

		drsprintk(ioc, printk(MYIOC_s_WARN_FMT "%s: Doorbell=%p; 1078 reset "
			"address=%p\n",  ioc->name, __func__,
			&ioc->chip->Doorbell, &ioc->chip->Reset_1078));
		CHIPREG_WRITE32(&ioc->chip->Reset_1078, 0x07);
		if (sleepFlag == CAN_SLEEP)
			msleep(1);
		else
			mdelay(1);

		for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--) {
			if (MptResetHandlers[cb_idx])
				(*(MptResetHandlers[cb_idx]))(ioc,
						MPT_IOC_PRE_RESET);
		}

		for (count = 0; count < 60; count ++) {
			doorbell = CHIPREG_READ32(&ioc->chip->Doorbell);
			doorbell &= MPI_IOC_STATE_MASK;

			drsprintk(ioc, printk(MYIOC_s_DEBUG_FMT
				"looking for READY STATE: doorbell=%x"
			        " count=%d\n",
				ioc->name, doorbell, count));

			if (doorbell == MPI_IOC_STATE_READY) {
				return 1;
			}

			
			if (sleepFlag == CAN_SLEEP)
				msleep(1000);
			else
				mdelay(1000);
		}
		return -1;
	}

	
	diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);

	if (ioc->debug_level & MPT_DEBUG) {
		if (ioc->alt_ioc)
			diag1val = CHIPREG_READ32(&ioc->alt_ioc->chip->Diagnostic);
		dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "DbG1: diag0=%08x, diag1=%08x\n",
			ioc->name, diag0val, diag1val));
	}

	if (ignore || !(diag0val & MPI_DIAG_RESET_HISTORY)) {
		while ((diag0val & MPI_DIAG_DRWE) == 0) {
			CHIPREG_WRITE32(&ioc->chip->WriteSequence, 0xFF);
			CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_1ST_KEY_VALUE);
			CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_2ND_KEY_VALUE);
			CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_3RD_KEY_VALUE);
			CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_4TH_KEY_VALUE);
			CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_5TH_KEY_VALUE);

			
			if (sleepFlag == CAN_SLEEP) {
				msleep (100);
			} else {
				mdelay (100);
			}

			count++;
			if (count > 20) {
				printk(MYIOC_s_ERR_FMT "Enable Diagnostic mode FAILED! (%02xh)\n",
						ioc->name, diag0val);
				return -2;

			}

			diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);

			dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Wrote magic DiagWriteEn sequence (%x)\n",
					ioc->name, diag0val));
		}

		if (ioc->debug_level & MPT_DEBUG) {
			if (ioc->alt_ioc)
				diag1val = CHIPREG_READ32(&ioc->alt_ioc->chip->Diagnostic);
			dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "DbG2: diag0=%08x, diag1=%08x\n",
				ioc->name, diag0val, diag1val));
		}
		CHIPREG_WRITE32(&ioc->chip->Diagnostic, diag0val | MPI_DIAG_DISABLE_ARM);
		mdelay(1);

		CHIPREG_WRITE32(&ioc->chip->Diagnostic, diag0val | MPI_DIAG_RESET_ADAPTER);
		hard_reset_done = 1;
		dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Diagnostic reset performed\n",
				ioc->name));

		for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--) {
			if (MptResetHandlers[cb_idx]) {
				mpt_signal_reset(cb_idx,
					ioc, MPT_IOC_PRE_RESET);
				if (ioc->alt_ioc) {
					mpt_signal_reset(cb_idx,
					ioc->alt_ioc, MPT_IOC_PRE_RESET);
				}
			}
		}

		if (ioc->cached_fw)
			cached_fw = (MpiFwHeader_t *)ioc->cached_fw;
		else if (ioc->alt_ioc && ioc->alt_ioc->cached_fw)
			cached_fw = (MpiFwHeader_t *)ioc->alt_ioc->cached_fw;
		else
			cached_fw = NULL;
		if (cached_fw) {
			for (count = 0; count < 30; count ++) {
				diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);
				if (!(diag0val & MPI_DIAG_RESET_ADAPTER)) {
					break;
				}

				dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "cached_fw: diag0val=%x count=%d\n",
					ioc->name, diag0val, count));
				
				if (sleepFlag == CAN_SLEEP) {
					msleep (1000);
				} else {
					mdelay (1000);
				}
			}
			if ((count = mpt_downloadboot(ioc, cached_fw, sleepFlag)) < 0) {
				printk(MYIOC_s_WARN_FMT
					"firmware downloadboot failure (%d)!\n", ioc->name, count);
			}

		} else {
			for (count = 0; count < 60; count ++) {
				doorbell = CHIPREG_READ32(&ioc->chip->Doorbell);
				doorbell &= MPI_IOC_STATE_MASK;

				drsprintk(ioc, printk(MYIOC_s_DEBUG_FMT
				    "looking for READY STATE: doorbell=%x"
				    " count=%d\n", ioc->name, doorbell, count));

				if (doorbell == MPI_IOC_STATE_READY) {
					break;
				}

				
				if (sleepFlag == CAN_SLEEP) {
					msleep (1000);
				} else {
					mdelay (1000);
				}
			}

			if (doorbell != MPI_IOC_STATE_READY)
				printk(MYIOC_s_ERR_FMT "Failed to come READY "
				    "after reset! IocState=%x", ioc->name,
				    doorbell);
		}
	}

	diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);
	if (ioc->debug_level & MPT_DEBUG) {
		if (ioc->alt_ioc)
			diag1val = CHIPREG_READ32(&ioc->alt_ioc->chip->Diagnostic);
		dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "DbG3: diag0=%08x, diag1=%08x\n",
			ioc->name, diag0val, diag1val));
	}

	diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);
	count = 0;
	while ((diag0val & MPI_DIAG_DRWE) == 0) {
		CHIPREG_WRITE32(&ioc->chip->WriteSequence, 0xFF);
		CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_1ST_KEY_VALUE);
		CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_2ND_KEY_VALUE);
		CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_3RD_KEY_VALUE);
		CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_4TH_KEY_VALUE);
		CHIPREG_WRITE32(&ioc->chip->WriteSequence, MPI_WRSEQ_5TH_KEY_VALUE);

		
		if (sleepFlag == CAN_SLEEP) {
			msleep (100);
		} else {
			mdelay (100);
		}

		count++;
		if (count > 20) {
			printk(MYIOC_s_ERR_FMT "Enable Diagnostic mode FAILED! (%02xh)\n",
					ioc->name, diag0val);
			break;
		}
		diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);
	}
	diag0val &= ~MPI_DIAG_RESET_HISTORY;
	CHIPREG_WRITE32(&ioc->chip->Diagnostic, diag0val);
	diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);
	if (diag0val & MPI_DIAG_RESET_HISTORY) {
		printk(MYIOC_s_WARN_FMT "ResetHistory bit failed to clear!\n",
				ioc->name);
	}

	CHIPREG_WRITE32(&ioc->chip->WriteSequence, 0xFFFFFFFF);

	diag0val = CHIPREG_READ32(&ioc->chip->Diagnostic);
	if (diag0val & (MPI_DIAG_FLASH_BAD_SIG | MPI_DIAG_RESET_ADAPTER | MPI_DIAG_DISABLE_ARM)) {
		printk(MYIOC_s_ERR_FMT "Diagnostic reset FAILED! (%02xh)\n",
				ioc->name, diag0val);
		return -3;
	}

	if (ioc->debug_level & MPT_DEBUG) {
		if (ioc->alt_ioc)
			diag1val = CHIPREG_READ32(&ioc->alt_ioc->chip->Diagnostic);
		dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "DbG4: diag0=%08x, diag1=%08x\n",
			ioc->name, diag0val, diag1val));
	}

	ioc->facts.EventState = 0;

	if (ioc->alt_ioc)
		ioc->alt_ioc->facts.EventState = 0;

	return hard_reset_done;
}

static int
SendIocReset(MPT_ADAPTER *ioc, u8 reset_type, int sleepFlag)
{
	int r;
	u32 state;
	int cntdn, count;

	drsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending IOC reset(0x%02x)!\n",
			ioc->name, reset_type));
	CHIPREG_WRITE32(&ioc->chip->Doorbell, reset_type<<MPI_DOORBELL_FUNCTION_SHIFT);
	if ((r = WaitForDoorbellAck(ioc, 5, sleepFlag)) < 0)
		return r;

	count = 0;
	cntdn = ((sleepFlag == CAN_SLEEP) ? HZ : 1000) * 15;	

	while ((state = mpt_GetIocState(ioc, 1)) != MPI_IOC_STATE_READY) {
		cntdn--;
		count++;
		if (!cntdn) {
			if (sleepFlag != CAN_SLEEP)
				count *= 10;

			printk(MYIOC_s_ERR_FMT
			    "Wait IOC_READY state (0x%x) timeout(%d)!\n",
			    ioc->name, state, (int)((count+5)/HZ));
			return -ETIME;
		}

		if (sleepFlag == CAN_SLEEP) {
			msleep(1);
		} else {
			mdelay (1);	
		}
	}

	if (ioc->facts.Function)
		ioc->facts.EventState = 0;

	return 0;
}

static int
initChainBuffers(MPT_ADAPTER *ioc)
{
	u8		*mem;
	int		sz, ii, num_chain;
	int 		scale, num_sge, numSGE;

	if (ioc->ReqToChain == NULL) {
		sz = ioc->req_depth * sizeof(int);
		mem = kmalloc(sz, GFP_ATOMIC);
		if (mem == NULL)
			return -1;

		ioc->ReqToChain = (int *) mem;
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "ReqToChain alloc  @ %p, sz=%d bytes\n",
			 	ioc->name, mem, sz));
		mem = kmalloc(sz, GFP_ATOMIC);
		if (mem == NULL)
			return -1;

		ioc->RequestNB = (int *) mem;
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "RequestNB alloc  @ %p, sz=%d bytes\n",
			 	ioc->name, mem, sz));
	}
	for (ii = 0; ii < ioc->req_depth; ii++) {
		ioc->ReqToChain[ii] = MPT_HOST_NO_CHAIN;
	}

	scale = ioc->req_sz / ioc->SGE_size;
	if (ioc->sg_addr_size == sizeof(u64))
		num_sge =  scale + (ioc->req_sz - 60) / ioc->SGE_size;
	else
		num_sge =  1 + scale + (ioc->req_sz - 64) / ioc->SGE_size;

	if (ioc->sg_addr_size == sizeof(u64)) {
		numSGE = (scale - 1) * (ioc->facts.MaxChainDepth-1) + scale +
			(ioc->req_sz - 60) / ioc->SGE_size;
	} else {
		numSGE = 1 + (scale - 1) * (ioc->facts.MaxChainDepth-1) +
		    scale + (ioc->req_sz - 64) / ioc->SGE_size;
	}
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "num_sge=%d numSGE=%d\n",
		ioc->name, num_sge, numSGE));

	if (ioc->bus_type == FC) {
		if (numSGE > MPT_SCSI_FC_SG_DEPTH)
			numSGE = MPT_SCSI_FC_SG_DEPTH;
	} else {
		if (numSGE > MPT_SCSI_SG_DEPTH)
			numSGE = MPT_SCSI_SG_DEPTH;
	}

	num_chain = 1;
	while (numSGE - num_sge > 0) {
		num_chain++;
		num_sge += (scale - 1);
	}
	num_chain++;

	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Now numSGE=%d num_sge=%d num_chain=%d\n",
		ioc->name, numSGE, num_sge, num_chain));

	if (ioc->bus_type == SPI)
		num_chain *= MPT_SCSI_CAN_QUEUE;
	else if (ioc->bus_type == SAS)
		num_chain *= MPT_SAS_CAN_QUEUE;
	else
		num_chain *= MPT_FC_CAN_QUEUE;

	ioc->num_chain = num_chain;

	sz = num_chain * sizeof(int);
	if (ioc->ChainToChain == NULL) {
		mem = kmalloc(sz, GFP_ATOMIC);
		if (mem == NULL)
			return -1;

		ioc->ChainToChain = (int *) mem;
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "ChainToChain alloc @ %p, sz=%d bytes\n",
			 	ioc->name, mem, sz));
	} else {
		mem = (u8 *) ioc->ChainToChain;
	}
	memset(mem, 0xFF, sz);
	return num_chain;
}

static int
PrimeIocFifos(MPT_ADAPTER *ioc)
{
	MPT_FRAME_HDR *mf;
	unsigned long flags;
	dma_addr_t alloc_dma;
	u8 *mem;
	int i, reply_sz, sz, total_size, num_chain;
	u64	dma_mask;

	dma_mask = 0;

	

	if (ioc->reply_frames == NULL) {
		if ( (num_chain = initChainBuffers(ioc)) < 0)
			return -1;
		if (ioc->pcidev->device == MPI_MANUFACTPAGE_DEVID_SAS1078 &&
		    ioc->dma_mask > DMA_BIT_MASK(35)) {
			if (!pci_set_dma_mask(ioc->pcidev, DMA_BIT_MASK(32))
			    && !pci_set_consistent_dma_mask(ioc->pcidev,
			    DMA_BIT_MASK(32))) {
				dma_mask = DMA_BIT_MASK(35);
				d36memprintk(ioc, printk(MYIOC_s_DEBUG_FMT
				    "setting 35 bit addressing for "
				    "Request/Reply/Chain and Sense Buffers\n",
				    ioc->name));
			} else {
				
				pci_set_dma_mask(ioc->pcidev,
					DMA_BIT_MASK(64));
				pci_set_consistent_dma_mask(ioc->pcidev,
					DMA_BIT_MASK(64));

				printk(MYIOC_s_ERR_FMT
				    "failed setting 35 bit addressing for "
				    "Request/Reply/Chain and Sense Buffers\n",
				    ioc->name);
				return -1;
			}
		}

		total_size = reply_sz = (ioc->reply_sz * ioc->reply_depth);
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "ReplyBuffer sz=%d bytes, ReplyDepth=%d\n",
			 	ioc->name, ioc->reply_sz, ioc->reply_depth));
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "ReplyBuffer sz=%d[%x] bytes\n",
			 	ioc->name, reply_sz, reply_sz));

		sz = (ioc->req_sz * ioc->req_depth);
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "RequestBuffer sz=%d bytes, RequestDepth=%d\n",
			 	ioc->name, ioc->req_sz, ioc->req_depth));
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "RequestBuffer sz=%d[%x] bytes\n",
			 	ioc->name, sz, sz));
		total_size += sz;

		sz = num_chain * ioc->req_sz; 
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "ChainBuffer sz=%d bytes, ChainDepth=%d\n",
			 	ioc->name, ioc->req_sz, num_chain));
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "ChainBuffer sz=%d[%x] bytes num_chain=%d\n",
			 	ioc->name, sz, sz, num_chain));

		total_size += sz;
		mem = pci_alloc_consistent(ioc->pcidev, total_size, &alloc_dma);
		if (mem == NULL) {
			printk(MYIOC_s_ERR_FMT "Unable to allocate Reply, Request, Chain Buffers!\n",
				ioc->name);
			goto out_fail;
		}

		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Total alloc @ %p[%p], sz=%d[%x] bytes\n",
			 	ioc->name, mem, (void *)(ulong)alloc_dma, total_size, total_size));

		memset(mem, 0, total_size);
		ioc->alloc_total += total_size;
		ioc->alloc = mem;
		ioc->alloc_dma = alloc_dma;
		ioc->alloc_sz = total_size;
		ioc->reply_frames = (MPT_FRAME_HDR *) mem;
		ioc->reply_frames_low_dma = (u32) (alloc_dma & 0xFFFFFFFF);

		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "ReplyBuffers @ %p[%p]\n",
	 		ioc->name, ioc->reply_frames, (void *)(ulong)alloc_dma));

		alloc_dma += reply_sz;
		mem += reply_sz;

		

		ioc->req_frames = (MPT_FRAME_HDR *) mem;
		ioc->req_frames_dma = alloc_dma;

		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "RequestBuffers @ %p[%p]\n",
			 	ioc->name, mem, (void *)(ulong)alloc_dma));

		ioc->req_frames_low_dma = (u32) (alloc_dma & 0xFFFFFFFF);

#if defined(CONFIG_MTRR) && 0
		ioc->mtrr_reg = mtrr_add(ioc->req_frames_dma,
					 sz,
					 MTRR_TYPE_WRCOMB, 1);
		dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "MTRR region registered (base:size=%08x:%x)\n",
				ioc->name, ioc->req_frames_dma, sz));
#endif

		for (i = 0; i < ioc->req_depth; i++) {
			alloc_dma += ioc->req_sz;
			mem += ioc->req_sz;
		}

		ioc->ChainBuffer = mem;
		ioc->ChainBufferDMA = alloc_dma;

		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "ChainBuffers @ %p(%p)\n",
			ioc->name, ioc->ChainBuffer, (void *)(ulong)ioc->ChainBufferDMA));


		INIT_LIST_HEAD(&ioc->FreeChainQ);

		mem = (u8 *)ioc->ChainBuffer;
		for (i=0; i < num_chain; i++) {
			mf = (MPT_FRAME_HDR *) mem;
			list_add_tail(&mf->u.frame.linkage.list, &ioc->FreeChainQ);
			mem += ioc->req_sz;
		}

		alloc_dma = ioc->req_frames_dma;
		mem = (u8 *) ioc->req_frames;

		spin_lock_irqsave(&ioc->FreeQlock, flags);
		INIT_LIST_HEAD(&ioc->FreeQ);
		for (i = 0; i < ioc->req_depth; i++) {
			mf = (MPT_FRAME_HDR *) mem;

			
			list_add_tail(&mf->u.frame.linkage.list, &ioc->FreeQ);

			mem += ioc->req_sz;
		}
		spin_unlock_irqrestore(&ioc->FreeQlock, flags);

		sz = (ioc->req_depth * MPT_SENSE_BUFFER_ALLOC);
		ioc->sense_buf_pool =
			pci_alloc_consistent(ioc->pcidev, sz, &ioc->sense_buf_pool_dma);
		if (ioc->sense_buf_pool == NULL) {
			printk(MYIOC_s_ERR_FMT "Unable to allocate Sense Buffers!\n",
				ioc->name);
			goto out_fail;
		}

		ioc->sense_buf_low_dma = (u32) (ioc->sense_buf_pool_dma & 0xFFFFFFFF);
		ioc->alloc_total += sz;
		dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "SenseBuffers @ %p[%p]\n",
 			ioc->name, ioc->sense_buf_pool, (void *)(ulong)ioc->sense_buf_pool_dma));

	}

	alloc_dma = ioc->alloc_dma;
	dinitprintk(ioc, printk(MYIOC_s_DEBUG_FMT "ReplyBuffers @ %p[%p]\n",
	 	ioc->name, ioc->reply_frames, (void *)(ulong)alloc_dma));

	for (i = 0; i < ioc->reply_depth; i++) {
		
		CHIPREG_WRITE32(&ioc->chip->ReplyFifo, alloc_dma);
		alloc_dma += ioc->reply_sz;
	}

	if (dma_mask == DMA_BIT_MASK(35) && !pci_set_dma_mask(ioc->pcidev,
	    ioc->dma_mask) && !pci_set_consistent_dma_mask(ioc->pcidev,
	    ioc->dma_mask))
		d36memprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "restoring 64 bit addressing\n", ioc->name));

	return 0;

out_fail:

	if (ioc->alloc != NULL) {
		sz = ioc->alloc_sz;
		pci_free_consistent(ioc->pcidev,
				sz,
				ioc->alloc, ioc->alloc_dma);
		ioc->reply_frames = NULL;
		ioc->req_frames = NULL;
		ioc->alloc_total -= sz;
	}
	if (ioc->sense_buf_pool != NULL) {
		sz = (ioc->req_depth * MPT_SENSE_BUFFER_ALLOC);
		pci_free_consistent(ioc->pcidev,
				sz,
				ioc->sense_buf_pool, ioc->sense_buf_pool_dma);
		ioc->sense_buf_pool = NULL;
	}

	if (dma_mask == DMA_BIT_MASK(35) && !pci_set_dma_mask(ioc->pcidev,
	    DMA_BIT_MASK(64)) && !pci_set_consistent_dma_mask(ioc->pcidev,
	    DMA_BIT_MASK(64)))
		d36memprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "restoring 64 bit addressing\n", ioc->name));

	return -1;
}

/**
 *	mpt_handshake_req_reply_wait - Send MPT request to and receive reply
 *	from IOC via doorbell handshake method.
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@reqBytes: Size of the request in bytes
 *	@req: Pointer to MPT request frame
 *	@replyBytes: Expected size of the reply in bytes
 *	@u16reply: Pointer to area where reply should be written
 *	@maxwait: Max wait time for a reply (in seconds)
 *	@sleepFlag: Specifies whether the process can sleep
 *
 *	NOTES: It is the callers responsibility to byte-swap fields in the
 *	request which are greater than 1 byte in size.  It is also the
 *	callers responsibility to byte-swap response fields which are
 *	greater than 1 byte in size.
 *
 *	Returns 0 for success, non-zero for failure.
 */
static int
mpt_handshake_req_reply_wait(MPT_ADAPTER *ioc, int reqBytes, u32 *req,
		int replyBytes, u16 *u16reply, int maxwait, int sleepFlag)
{
	MPIDefaultReply_t *mptReply;
	int failcnt = 0;
	int t;

	ioc->hs_reply_idx = 0;
	mptReply = (MPIDefaultReply_t *) ioc->hs_reply;
	mptReply->MsgLength = 0;

	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);
	CHIPREG_WRITE32(&ioc->chip->Doorbell,
			((MPI_FUNCTION_HANDSHAKE<<MPI_DOORBELL_FUNCTION_SHIFT) |
			 ((reqBytes/4)<<MPI_DOORBELL_ADD_DWORDS_SHIFT)));

	if ((t = WaitForDoorbellInt(ioc, 5, sleepFlag)) < 0)
		failcnt++;

	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "HandShake request start reqBytes=%d, WaitCnt=%d%s\n",
			ioc->name, reqBytes, t, failcnt ? " - MISSING DOORBELL HANDSHAKE!" : ""));

	
	if (!(CHIPREG_READ32(&ioc->chip->Doorbell) & MPI_DOORBELL_ACTIVE))
			return -1;

	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);
	if (!failcnt && (t = WaitForDoorbellAck(ioc, 5, sleepFlag)) < 0)
		failcnt++;

	if (!failcnt) {
		int	 ii;
		u8	*req_as_bytes = (u8 *) req;

		for (ii = 0; !failcnt && ii < reqBytes/4; ii++) {
			u32 word = ((req_as_bytes[(ii*4) + 0] <<  0) |
				    (req_as_bytes[(ii*4) + 1] <<  8) |
				    (req_as_bytes[(ii*4) + 2] << 16) |
				    (req_as_bytes[(ii*4) + 3] << 24));

			CHIPREG_WRITE32(&ioc->chip->Doorbell, word);
			if ((t = WaitForDoorbellAck(ioc, 5, sleepFlag)) < 0)
				failcnt++;
		}

		dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Handshake request frame (@%p) header\n", ioc->name, req));
		DBG_DUMP_REQUEST_FRAME_HDR(ioc, (u32 *)req);

		dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "HandShake request post done, WaitCnt=%d%s\n",
				ioc->name, t, failcnt ? " - MISSING DOORBELL ACK!" : ""));

		if (!failcnt && (t = WaitForDoorbellReply(ioc, maxwait, sleepFlag)) < 0)
			failcnt++;

		dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "HandShake reply count=%d%s\n",
				ioc->name, t, failcnt ? " - MISSING DOORBELL REPLY!" : ""));

		for (ii=0; ii < min(replyBytes/2,mptReply->MsgLength*2); ii++)
			u16reply[ii] = ioc->hs_reply[ii];
	} else {
		return -99;
	}

	return -failcnt;
}

static int
WaitForDoorbellAck(MPT_ADAPTER *ioc, int howlong, int sleepFlag)
{
	int cntdn;
	int count = 0;
	u32 intstat=0;

	cntdn = 1000 * howlong;

	if (sleepFlag == CAN_SLEEP) {
		while (--cntdn) {
			msleep (1);
			intstat = CHIPREG_READ32(&ioc->chip->IntStatus);
			if (! (intstat & MPI_HIS_IOP_DOORBELL_STATUS))
				break;
			count++;
		}
	} else {
		while (--cntdn) {
			udelay (1000);
			intstat = CHIPREG_READ32(&ioc->chip->IntStatus);
			if (! (intstat & MPI_HIS_IOP_DOORBELL_STATUS))
				break;
			count++;
		}
	}

	if (cntdn) {
		dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "WaitForDoorbell ACK (count=%d)\n",
				ioc->name, count));
		return count;
	}

	printk(MYIOC_s_ERR_FMT "Doorbell ACK timeout (count=%d), IntStatus=%x!\n",
			ioc->name, count, intstat);
	return -1;
}

static int
WaitForDoorbellInt(MPT_ADAPTER *ioc, int howlong, int sleepFlag)
{
	int cntdn;
	int count = 0;
	u32 intstat=0;

	cntdn = 1000 * howlong;
	if (sleepFlag == CAN_SLEEP) {
		while (--cntdn) {
			intstat = CHIPREG_READ32(&ioc->chip->IntStatus);
			if (intstat & MPI_HIS_DOORBELL_INTERRUPT)
				break;
			msleep(1);
			count++;
		}
	} else {
		while (--cntdn) {
			intstat = CHIPREG_READ32(&ioc->chip->IntStatus);
			if (intstat & MPI_HIS_DOORBELL_INTERRUPT)
				break;
			udelay (1000);
			count++;
		}
	}

	if (cntdn) {
		dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "WaitForDoorbell INT (cnt=%d) howlong=%d\n",
				ioc->name, count, howlong));
		return count;
	}

	printk(MYIOC_s_ERR_FMT "Doorbell INT timeout (count=%d), IntStatus=%x!\n",
			ioc->name, count, intstat);
	return -1;
}

static int
WaitForDoorbellReply(MPT_ADAPTER *ioc, int howlong, int sleepFlag)
{
	int u16cnt = 0;
	int failcnt = 0;
	int t;
	u16 *hs_reply = ioc->hs_reply;
	volatile MPIDefaultReply_t *mptReply = (MPIDefaultReply_t *) ioc->hs_reply;
	u16 hword;

	hs_reply[0] = hs_reply[1] = hs_reply[7] = 0;

	u16cnt=0;
	if ((t = WaitForDoorbellInt(ioc, howlong, sleepFlag)) < 0) {
		failcnt++;
	} else {
		hs_reply[u16cnt++] = le16_to_cpu(CHIPREG_READ32(&ioc->chip->Doorbell) & 0x0000FFFF);
		CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);
		if ((t = WaitForDoorbellInt(ioc, 5, sleepFlag)) < 0)
			failcnt++;
		else {
			hs_reply[u16cnt++] = le16_to_cpu(CHIPREG_READ32(&ioc->chip->Doorbell) & 0x0000FFFF);
			CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);
		}
	}

	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "WaitCnt=%d First handshake reply word=%08x%s\n",
			ioc->name, t, le32_to_cpu(*(u32 *)hs_reply),
			failcnt ? " - MISSING DOORBELL HANDSHAKE!" : ""));

	for (u16cnt=2; !failcnt && u16cnt < (2 * mptReply->MsgLength); u16cnt++) {
		if ((t = WaitForDoorbellInt(ioc, 5, sleepFlag)) < 0)
			failcnt++;
		hword = le16_to_cpu(CHIPREG_READ32(&ioc->chip->Doorbell) & 0x0000FFFF);
		
		if (u16cnt < ARRAY_SIZE(ioc->hs_reply))
			hs_reply[u16cnt] = hword;
		CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);
	}

	if (!failcnt && (t = WaitForDoorbellInt(ioc, 5, sleepFlag)) < 0)
		failcnt++;
	CHIPREG_WRITE32(&ioc->chip->IntStatus, 0);

	if (failcnt) {
		printk(MYIOC_s_ERR_FMT "Handshake reply failure!\n",
				ioc->name);
		return -failcnt;
	}
#if 0
	else if (u16cnt != (2 * mptReply->MsgLength)) {
		return -101;
	}
	else if ((mptReply->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		return -102;
	}
#endif

	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Got Handshake reply:\n", ioc->name));
	DBG_DUMP_REPLY_FRAME(ioc, (u32 *)mptReply);

	dhsprintk(ioc, printk(MYIOC_s_DEBUG_FMT "WaitForDoorbell REPLY WaitCnt=%d (sz=%d)\n",
			ioc->name, t, u16cnt/2));
	return u16cnt/2;
}

static int
GetLanConfigPages(MPT_ADAPTER *ioc)
{
	ConfigPageHeader_t	 hdr;
	CONFIGPARMS		 cfg;
	LANPage0_t		*ppage0_alloc;
	dma_addr_t		 page0_dma;
	LANPage1_t		*ppage1_alloc;
	dma_addr_t		 page1_dma;
	int			 rc = 0;
	int			 data_sz;
	int			 copy_sz;

	
	hdr.PageVersion = 0;
	hdr.PageLength = 0;
	hdr.PageNumber = 0;
	hdr.PageType = MPI_CONFIG_PAGETYPE_LAN;
	cfg.cfghdr.hdr = &hdr;
	cfg.physAddr = -1;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;
	cfg.pageAddr = 0;
	cfg.timeout = 0;

	if ((rc = mpt_config(ioc, &cfg)) != 0)
		return rc;

	if (hdr.PageLength > 0) {
		data_sz = hdr.PageLength * 4;
		ppage0_alloc = (LANPage0_t *) pci_alloc_consistent(ioc->pcidev, data_sz, &page0_dma);
		rc = -ENOMEM;
		if (ppage0_alloc) {
			memset((u8 *)ppage0_alloc, 0, data_sz);
			cfg.physAddr = page0_dma;
			cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

			if ((rc = mpt_config(ioc, &cfg)) == 0) {
				
				copy_sz = min_t(int, sizeof(LANPage0_t), data_sz);
				memcpy(&ioc->lan_cnfg_page0, ppage0_alloc, copy_sz);

			}

			pci_free_consistent(ioc->pcidev, data_sz, (u8 *) ppage0_alloc, page0_dma);


		}

		if (rc)
			return rc;
	}

	
	hdr.PageVersion = 0;
	hdr.PageLength = 0;
	hdr.PageNumber = 1;
	hdr.PageType = MPI_CONFIG_PAGETYPE_LAN;
	cfg.cfghdr.hdr = &hdr;
	cfg.physAddr = -1;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;
	cfg.pageAddr = 0;

	if ((rc = mpt_config(ioc, &cfg)) != 0)
		return rc;

	if (hdr.PageLength == 0)
		return 0;

	data_sz = hdr.PageLength * 4;
	rc = -ENOMEM;
	ppage1_alloc = (LANPage1_t *) pci_alloc_consistent(ioc->pcidev, data_sz, &page1_dma);
	if (ppage1_alloc) {
		memset((u8 *)ppage1_alloc, 0, data_sz);
		cfg.physAddr = page1_dma;
		cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

		if ((rc = mpt_config(ioc, &cfg)) == 0) {
			
			copy_sz = min_t(int, sizeof(LANPage1_t), data_sz);
			memcpy(&ioc->lan_cnfg_page1, ppage1_alloc, copy_sz);
		}

		pci_free_consistent(ioc->pcidev, data_sz, (u8 *) ppage1_alloc, page1_dma);


	}

	return rc;
}


int
mptbase_sas_persist_operation(MPT_ADAPTER *ioc, u8 persist_opcode)
{
	SasIoUnitControlRequest_t	*sasIoUnitCntrReq;
	SasIoUnitControlReply_t		*sasIoUnitCntrReply;
	MPT_FRAME_HDR			*mf = NULL;
	MPIHeader_t			*mpi_hdr;
	int				ret = 0;
	unsigned long 	 		timeleft;

	mutex_lock(&ioc->mptbase_cmds.mutex);

	
	memset(ioc->mptbase_cmds.reply, 0 , MPT_DEFAULT_FRAME_SIZE);
	INITIALIZE_MGMT_STATUS(ioc->mptbase_cmds.status)

	
	switch(persist_opcode) {

	case MPI_SAS_OP_CLEAR_NOT_PRESENT:
	case MPI_SAS_OP_CLEAR_ALL_PERSISTENT:
		break;

	default:
		ret = -1;
		goto out;
	}

	printk(KERN_DEBUG  "%s: persist_opcode=%x\n",
		__func__, persist_opcode);

	if ((mf = mpt_get_msg_frame(mpt_base_index, ioc)) == NULL) {
		printk(KERN_DEBUG "%s: no msg frames!\n", __func__);
		ret = -1;
		goto out;
        }

	mpi_hdr = (MPIHeader_t *) mf;
	sasIoUnitCntrReq = (SasIoUnitControlRequest_t *)mf;
	memset(sasIoUnitCntrReq,0,sizeof(SasIoUnitControlRequest_t));
	sasIoUnitCntrReq->Function = MPI_FUNCTION_SAS_IO_UNIT_CONTROL;
	sasIoUnitCntrReq->MsgContext = mpi_hdr->MsgContext;
	sasIoUnitCntrReq->Operation = persist_opcode;

	mpt_put_msg_frame(mpt_base_index, ioc, mf);
	timeleft = wait_for_completion_timeout(&ioc->mptbase_cmds.done, 10*HZ);
	if (!(ioc->mptbase_cmds.status & MPT_MGMT_STATUS_COMMAND_GOOD)) {
		ret = -ETIME;
		printk(KERN_DEBUG "%s: failed\n", __func__);
		if (ioc->mptbase_cmds.status & MPT_MGMT_STATUS_DID_IOCRESET)
			goto out;
		if (!timeleft) {
			printk(MYIOC_s_WARN_FMT
			       "Issuing Reset from %s!!, doorbell=0x%08x\n",
			       ioc->name, __func__, mpt_GetIocState(ioc, 0));
			mpt_Soft_Hard_ResetHandler(ioc, CAN_SLEEP);
			mpt_free_msg_frame(ioc, mf);
		}
		goto out;
	}

	if (!(ioc->mptbase_cmds.status & MPT_MGMT_STATUS_RF_VALID)) {
		ret = -1;
		goto out;
	}

	sasIoUnitCntrReply =
	    (SasIoUnitControlReply_t *)ioc->mptbase_cmds.reply;
	if (le16_to_cpu(sasIoUnitCntrReply->IOCStatus) != MPI_IOCSTATUS_SUCCESS) {
		printk(KERN_DEBUG "%s: IOCStatus=0x%X IOCLogInfo=0x%X\n",
		    __func__, sasIoUnitCntrReply->IOCStatus,
		    sasIoUnitCntrReply->IOCLogInfo);
		printk(KERN_DEBUG "%s: failed\n", __func__);
		ret = -1;
	} else
		printk(KERN_DEBUG "%s: success\n", __func__);
 out:

	CLEAR_MGMT_STATUS(ioc->mptbase_cmds.status)
	mutex_unlock(&ioc->mptbase_cmds.mutex);
	return ret;
}


static void
mptbase_raid_process_event_data(MPT_ADAPTER *ioc,
    MpiEventDataRaid_t * pRaidEventData)
{
	int 	volume;
	int 	reason;
	int 	disk;
	int 	status;
	int 	flags;
	int 	state;

	volume	= pRaidEventData->VolumeID;
	reason	= pRaidEventData->ReasonCode;
	disk	= pRaidEventData->PhysDiskNum;
	status	= le32_to_cpu(pRaidEventData->SettingsStatus);
	flags	= (status >> 0) & 0xff;
	state	= (status >> 8) & 0xff;

	if (reason == MPI_EVENT_RAID_RC_DOMAIN_VAL_NEEDED) {
		return;
	}

	if ((reason >= MPI_EVENT_RAID_RC_PHYSDISK_CREATED &&
	     reason <= MPI_EVENT_RAID_RC_PHYSDISK_STATUS_CHANGED) ||
	    (reason == MPI_EVENT_RAID_RC_SMART_DATA)) {
		printk(MYIOC_s_INFO_FMT "RAID STATUS CHANGE for PhysDisk %d id=%d\n",
			ioc->name, disk, volume);
	} else {
		printk(MYIOC_s_INFO_FMT "RAID STATUS CHANGE for VolumeID %d\n",
			ioc->name, volume);
	}

	switch(reason) {
	case MPI_EVENT_RAID_RC_VOLUME_CREATED:
		printk(MYIOC_s_INFO_FMT "  volume has been created\n",
			ioc->name);
		break;

	case MPI_EVENT_RAID_RC_VOLUME_DELETED:

		printk(MYIOC_s_INFO_FMT "  volume has been deleted\n",
			ioc->name);
		break;

	case MPI_EVENT_RAID_RC_VOLUME_SETTINGS_CHANGED:
		printk(MYIOC_s_INFO_FMT "  volume settings have been changed\n",
			ioc->name);
		break;

	case MPI_EVENT_RAID_RC_VOLUME_STATUS_CHANGED:
		printk(MYIOC_s_INFO_FMT "  volume is now %s%s%s%s\n",
			ioc->name,
			state == MPI_RAIDVOL0_STATUS_STATE_OPTIMAL
			 ? "optimal"
			 : state == MPI_RAIDVOL0_STATUS_STATE_DEGRADED
			  ? "degraded"
			  : state == MPI_RAIDVOL0_STATUS_STATE_FAILED
			   ? "failed"
			   : "state unknown",
			flags & MPI_RAIDVOL0_STATUS_FLAG_ENABLED
			 ? ", enabled" : "",
			flags & MPI_RAIDVOL0_STATUS_FLAG_QUIESCED
			 ? ", quiesced" : "",
			flags & MPI_RAIDVOL0_STATUS_FLAG_RESYNC_IN_PROGRESS
			 ? ", resync in progress" : "" );
		break;

	case MPI_EVENT_RAID_RC_VOLUME_PHYSDISK_CHANGED:
		printk(MYIOC_s_INFO_FMT "  volume membership of PhysDisk %d has changed\n",
			ioc->name, disk);
		break;

	case MPI_EVENT_RAID_RC_PHYSDISK_CREATED:
		printk(MYIOC_s_INFO_FMT "  PhysDisk has been created\n",
			ioc->name);
		break;

	case MPI_EVENT_RAID_RC_PHYSDISK_DELETED:
		printk(MYIOC_s_INFO_FMT "  PhysDisk has been deleted\n",
			ioc->name);
		break;

	case MPI_EVENT_RAID_RC_PHYSDISK_SETTINGS_CHANGED:
		printk(MYIOC_s_INFO_FMT "  PhysDisk settings have been changed\n",
			ioc->name);
		break;

	case MPI_EVENT_RAID_RC_PHYSDISK_STATUS_CHANGED:
		printk(MYIOC_s_INFO_FMT "  PhysDisk is now %s%s%s\n",
			ioc->name,
			state == MPI_PHYSDISK0_STATUS_ONLINE
			 ? "online"
			 : state == MPI_PHYSDISK0_STATUS_MISSING
			  ? "missing"
			  : state == MPI_PHYSDISK0_STATUS_NOT_COMPATIBLE
			   ? "not compatible"
			   : state == MPI_PHYSDISK0_STATUS_FAILED
			    ? "failed"
			    : state == MPI_PHYSDISK0_STATUS_INITIALIZING
			     ? "initializing"
			     : state == MPI_PHYSDISK0_STATUS_OFFLINE_REQUESTED
			      ? "offline requested"
			      : state == MPI_PHYSDISK0_STATUS_FAILED_REQUESTED
			       ? "failed requested"
			       : state == MPI_PHYSDISK0_STATUS_OTHER_OFFLINE
			        ? "offline"
			        : "state unknown",
			flags & MPI_PHYSDISK0_STATUS_FLAG_OUT_OF_SYNC
			 ? ", out of sync" : "",
			flags & MPI_PHYSDISK0_STATUS_FLAG_QUIESCED
			 ? ", quiesced" : "" );
		break;

	case MPI_EVENT_RAID_RC_DOMAIN_VAL_NEEDED:
		printk(MYIOC_s_INFO_FMT "  Domain Validation needed for PhysDisk %d\n",
			ioc->name, disk);
		break;

	case MPI_EVENT_RAID_RC_SMART_DATA:
		printk(MYIOC_s_INFO_FMT "  SMART data received, ASC/ASCQ = %02xh/%02xh\n",
			ioc->name, pRaidEventData->ASC, pRaidEventData->ASCQ);
		break;

	case MPI_EVENT_RAID_RC_REPLACE_ACTION_STARTED:
		printk(MYIOC_s_INFO_FMT "  replacement of PhysDisk %d has started\n",
			ioc->name, disk);
		break;
	}
}

static int
GetIoUnitPage2(MPT_ADAPTER *ioc)
{
	ConfigPageHeader_t	 hdr;
	CONFIGPARMS		 cfg;
	IOUnitPage2_t		*ppage_alloc;
	dma_addr_t		 page_dma;
	int			 data_sz;
	int			 rc;

	
	hdr.PageVersion = 0;
	hdr.PageLength = 0;
	hdr.PageNumber = 2;
	hdr.PageType = MPI_CONFIG_PAGETYPE_IO_UNIT;
	cfg.cfghdr.hdr = &hdr;
	cfg.physAddr = -1;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;
	cfg.pageAddr = 0;
	cfg.timeout = 0;

	if ((rc = mpt_config(ioc, &cfg)) != 0)
		return rc;

	if (hdr.PageLength == 0)
		return 0;

	
	data_sz = hdr.PageLength * 4;
	rc = -ENOMEM;
	ppage_alloc = (IOUnitPage2_t *) pci_alloc_consistent(ioc->pcidev, data_sz, &page_dma);
	if (ppage_alloc) {
		memset((u8 *)ppage_alloc, 0, data_sz);
		cfg.physAddr = page_dma;
		cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

		
		if ((rc = mpt_config(ioc, &cfg)) == 0)
			ioc->biosVersion = le32_to_cpu(ppage_alloc->BiosVersion);

		pci_free_consistent(ioc->pcidev, data_sz, (u8 *) ppage_alloc, page_dma);
	}

	return rc;
}

static int
mpt_GetScsiPortSettings(MPT_ADAPTER *ioc, int portnum)
{
	u8			*pbuf;
	dma_addr_t		 buf_dma;
	CONFIGPARMS		 cfg;
	ConfigPageHeader_t	 header;
	int			 ii;
	int			 data, rc = 0;

	if (!ioc->spi_data.nvram) {
		int	 sz;
		u8	*mem;
		sz = MPT_MAX_SCSI_DEVICES * sizeof(int);
		mem = kmalloc(sz, GFP_ATOMIC);
		if (mem == NULL)
			return -EFAULT;

		ioc->spi_data.nvram = (int *) mem;

		dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "SCSI device NVRAM settings @ %p, sz=%d\n",
			ioc->name, ioc->spi_data.nvram, sz));
	}

	for (ii=0; ii < MPT_MAX_SCSI_DEVICES; ii++) {
		ioc->spi_data.nvram[ii] = MPT_HOST_NVRAM_INVALID;
	}

	header.PageVersion = 0;
	header.PageLength = 0;
	header.PageNumber = 0;
	header.PageType = MPI_CONFIG_PAGETYPE_SCSI_PORT;
	cfg.cfghdr.hdr = &header;
	cfg.physAddr = -1;
	cfg.pageAddr = portnum;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;
	cfg.timeout = 0;	
	if (mpt_config(ioc, &cfg) != 0)
		 return -EFAULT;

	if (header.PageLength > 0) {
		pbuf = pci_alloc_consistent(ioc->pcidev, header.PageLength * 4, &buf_dma);
		if (pbuf) {
			cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
			cfg.physAddr = buf_dma;
			if (mpt_config(ioc, &cfg) != 0) {
				ioc->spi_data.maxBusWidth = MPT_NARROW;
				ioc->spi_data.maxSyncOffset = 0;
				ioc->spi_data.minSyncFactor = MPT_ASYNC;
				ioc->spi_data.busType = MPT_HOST_BUS_UNKNOWN;
				rc = 1;
				ddvprintk(ioc, printk(MYIOC_s_DEBUG_FMT
					"Unable to read PortPage0 minSyncFactor=%x\n",
					ioc->name, ioc->spi_data.minSyncFactor));
			} else {
				SCSIPortPage0_t  *pPP0 = (SCSIPortPage0_t  *) pbuf;
				pPP0->Capabilities = le32_to_cpu(pPP0->Capabilities);
				pPP0->PhysicalInterface = le32_to_cpu(pPP0->PhysicalInterface);

				if ( (pPP0->Capabilities & MPI_SCSIPORTPAGE0_CAP_QAS) == 0 ) {
					ioc->spi_data.noQas |= MPT_TARGET_NO_NEGO_QAS;
					ddvprintk(ioc, printk(MYIOC_s_DEBUG_FMT
						"noQas due to Capabilities=%x\n",
						ioc->name, pPP0->Capabilities));
				}
				ioc->spi_data.maxBusWidth = pPP0->Capabilities & MPI_SCSIPORTPAGE0_CAP_WIDE ? 1 : 0;
				data = pPP0->Capabilities & MPI_SCSIPORTPAGE0_CAP_MAX_SYNC_OFFSET_MASK;
				if (data) {
					ioc->spi_data.maxSyncOffset = (u8) (data >> 16);
					data = pPP0->Capabilities & MPI_SCSIPORTPAGE0_CAP_MIN_SYNC_PERIOD_MASK;
					ioc->spi_data.minSyncFactor = (u8) (data >> 8);
					ddvprintk(ioc, printk(MYIOC_s_DEBUG_FMT
						"PortPage0 minSyncFactor=%x\n",
						ioc->name, ioc->spi_data.minSyncFactor));
				} else {
					ioc->spi_data.maxSyncOffset = 0;
					ioc->spi_data.minSyncFactor = MPT_ASYNC;
				}

				ioc->spi_data.busType = pPP0->PhysicalInterface & MPI_SCSIPORTPAGE0_PHY_SIGNAL_TYPE_MASK;

				if ((ioc->spi_data.busType == MPI_SCSIPORTPAGE0_PHY_SIGNAL_HVD) ||
					(ioc->spi_data.busType == MPI_SCSIPORTPAGE0_PHY_SIGNAL_SE))  {

					if (ioc->spi_data.minSyncFactor < MPT_ULTRA) {
						ioc->spi_data.minSyncFactor = MPT_ULTRA;
						ddvprintk(ioc, printk(MYIOC_s_DEBUG_FMT
							"HVD or SE detected, minSyncFactor=%x\n",
							ioc->name, ioc->spi_data.minSyncFactor));
					}
				}
			}
			if (pbuf) {
				pci_free_consistent(ioc->pcidev, header.PageLength * 4, pbuf, buf_dma);
			}
		}
	}

	header.PageVersion = 0;
	header.PageLength = 0;
	header.PageNumber = 2;
	header.PageType = MPI_CONFIG_PAGETYPE_SCSI_PORT;
	cfg.cfghdr.hdr = &header;
	cfg.physAddr = -1;
	cfg.pageAddr = portnum;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;
	if (mpt_config(ioc, &cfg) != 0)
		return -EFAULT;

	if (header.PageLength > 0) {
		pbuf = pci_alloc_consistent(ioc->pcidev, header.PageLength * 4, &buf_dma);
		if (pbuf) {
			cfg.action = MPI_CONFIG_ACTION_PAGE_READ_NVRAM;
			cfg.physAddr = buf_dma;
			if (mpt_config(ioc, &cfg) != 0) {
				rc = 1;
			} else if (ioc->pcidev->vendor == PCI_VENDOR_ID_ATTO) {

				ATTO_SCSIPortPage2_t *pPP2 = (ATTO_SCSIPortPage2_t  *) pbuf;
				ATTODeviceInfo_t *pdevice = NULL;
				u16 ATTOFlags;

				for (ii=0; ii < MPT_MAX_SCSI_DEVICES; ii++) {
				  pdevice = &pPP2->DeviceSettings[ii];
				  ATTOFlags = le16_to_cpu(pdevice->ATTOFlags);
				  data = 0;

				  if (ATTOFlags & ATTOFLAG_DISC)
				    data |= (MPI_SCSIPORTPAGE2_DEVICE_DISCONNECT_ENABLE);
				  if (ATTOFlags & ATTOFLAG_ID_ENB)
				    data |= (MPI_SCSIPORTPAGE2_DEVICE_ID_SCAN_ENABLE);
				  if (ATTOFlags & ATTOFLAG_LUN_ENB)
				    data |= (MPI_SCSIPORTPAGE2_DEVICE_LUN_SCAN_ENABLE);
				  if (ATTOFlags & ATTOFLAG_TAGGED)
				    data |= (MPI_SCSIPORTPAGE2_DEVICE_TAG_QUEUE_ENABLE);
				  if (!(ATTOFlags & ATTOFLAG_WIDE_ENB))
				    data |= (MPI_SCSIPORTPAGE2_DEVICE_WIDE_DISABLE);

				  data = (data << 16) | (pdevice->Period << 8) | 10;
				  ioc->spi_data.nvram[ii] = data;
				}
			} else {
				SCSIPortPage2_t *pPP2 = (SCSIPortPage2_t  *) pbuf;
				MpiDeviceInfo_t	*pdevice = NULL;

				ioc->spi_data.bus_reset =
				    (le32_to_cpu(pPP2->PortFlags) &
			        MPI_SCSIPORTPAGE2_PORT_FLAGS_AVOID_SCSI_RESET) ?
				    0 : 1 ;

				data = le32_to_cpu(pPP2->PortFlags) & MPI_SCSIPORTPAGE2_PORT_FLAGS_DV_MASK;
				ioc->spi_data.PortFlags = data;
				for (ii=0; ii < MPT_MAX_SCSI_DEVICES; ii++) {
					pdevice = &pPP2->DeviceSettings[ii];
					data = (le16_to_cpu(pdevice->DeviceFlags) << 16) |
						(pdevice->SyncFactor << 8) | pdevice->Timeout;
					ioc->spi_data.nvram[ii] = data;
				}
			}

			pci_free_consistent(ioc->pcidev, header.PageLength * 4, pbuf, buf_dma);
		}
	}


	return rc;
}

static int
mpt_readScsiDevicePageHeaders(MPT_ADAPTER *ioc, int portnum)
{
	CONFIGPARMS		 cfg;
	ConfigPageHeader_t	 header;

	header.PageVersion = 0;
	header.PageLength = 0;
	header.PageNumber = 1;
	header.PageType = MPI_CONFIG_PAGETYPE_SCSI_DEVICE;
	cfg.cfghdr.hdr = &header;
	cfg.physAddr = -1;
	cfg.pageAddr = portnum;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;
	cfg.timeout = 0;
	if (mpt_config(ioc, &cfg) != 0)
		 return -EFAULT;

	ioc->spi_data.sdp1version = cfg.cfghdr.hdr->PageVersion;
	ioc->spi_data.sdp1length = cfg.cfghdr.hdr->PageLength;

	header.PageVersion = 0;
	header.PageLength = 0;
	header.PageNumber = 0;
	header.PageType = MPI_CONFIG_PAGETYPE_SCSI_DEVICE;
	if (mpt_config(ioc, &cfg) != 0)
		 return -EFAULT;

	ioc->spi_data.sdp0version = cfg.cfghdr.hdr->PageVersion;
	ioc->spi_data.sdp0length = cfg.cfghdr.hdr->PageLength;

	dcprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Headers: 0: version %d length %d\n",
			ioc->name, ioc->spi_data.sdp0version, ioc->spi_data.sdp0length));

	dcprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Headers: 1: version %d length %d\n",
			ioc->name, ioc->spi_data.sdp1version, ioc->spi_data.sdp1length));
	return 0;
}

static void
mpt_inactive_raid_list_free(MPT_ADAPTER *ioc)
{
	struct inactive_raid_component_info *component_info, *pNext;

	if (list_empty(&ioc->raid_data.inactive_list))
		return;

	mutex_lock(&ioc->raid_data.inactive_list_mutex);
	list_for_each_entry_safe(component_info, pNext,
	    &ioc->raid_data.inactive_list, list) {
		list_del(&component_info->list);
		kfree(component_info);
	}
	mutex_unlock(&ioc->raid_data.inactive_list_mutex);
}

static void
mpt_inactive_raid_volumes(MPT_ADAPTER *ioc, u8 channel, u8 id)
{
	CONFIGPARMS			cfg;
	ConfigPageHeader_t		hdr;
	dma_addr_t			dma_handle;
	pRaidVolumePage0_t		buffer = NULL;
	int				i;
	RaidPhysDiskPage0_t 		phys_disk;
	struct inactive_raid_component_info *component_info;
	int				handle_inactive_volumes;

	memset(&cfg, 0 , sizeof(CONFIGPARMS));
	memset(&hdr, 0 , sizeof(ConfigPageHeader_t));
	hdr.PageType = MPI_CONFIG_PAGETYPE_RAID_VOLUME;
	cfg.pageAddr = (channel << 8) + id;
	cfg.cfghdr.hdr = &hdr;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;

	if (mpt_config(ioc, &cfg) != 0)
		goto out;

	if (!hdr.PageLength)
		goto out;

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.PageLength * 4,
	    &dma_handle);

	if (!buffer)
		goto out;

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;

	if (mpt_config(ioc, &cfg) != 0)
		goto out;

	if (!buffer->NumPhysDisks)
		goto out;

	handle_inactive_volumes =
	   (buffer->VolumeStatus.Flags & MPI_RAIDVOL0_STATUS_FLAG_VOLUME_INACTIVE ||
	   (buffer->VolumeStatus.Flags & MPI_RAIDVOL0_STATUS_FLAG_ENABLED) == 0 ||
	    buffer->VolumeStatus.State == MPI_RAIDVOL0_STATUS_STATE_FAILED ||
	    buffer->VolumeStatus.State == MPI_RAIDVOL0_STATUS_STATE_MISSING) ? 1 : 0;

	if (!handle_inactive_volumes)
		goto out;

	mutex_lock(&ioc->raid_data.inactive_list_mutex);
	for (i = 0; i < buffer->NumPhysDisks; i++) {
		if(mpt_raid_phys_disk_pg0(ioc,
		    buffer->PhysDisk[i].PhysDiskNum, &phys_disk) != 0)
			continue;

		if ((component_info = kmalloc(sizeof (*component_info),
		 GFP_KERNEL)) == NULL)
			continue;

		component_info->volumeID = id;
		component_info->volumeBus = channel;
		component_info->d.PhysDiskNum = phys_disk.PhysDiskNum;
		component_info->d.PhysDiskBus = phys_disk.PhysDiskBus;
		component_info->d.PhysDiskID = phys_disk.PhysDiskID;
		component_info->d.PhysDiskIOC = phys_disk.PhysDiskIOC;

		list_add_tail(&component_info->list,
		    &ioc->raid_data.inactive_list);
	}
	mutex_unlock(&ioc->raid_data.inactive_list_mutex);

 out:
	if (buffer)
		pci_free_consistent(ioc->pcidev, hdr.PageLength * 4, buffer,
		    dma_handle);
}

int
mpt_raid_phys_disk_pg0(MPT_ADAPTER *ioc, u8 phys_disk_num,
			RaidPhysDiskPage0_t *phys_disk)
{
	CONFIGPARMS			cfg;
	ConfigPageHeader_t		hdr;
	dma_addr_t			dma_handle;
	pRaidPhysDiskPage0_t		buffer = NULL;
	int				rc;

	memset(&cfg, 0 , sizeof(CONFIGPARMS));
	memset(&hdr, 0 , sizeof(ConfigPageHeader_t));
	memset(phys_disk, 0, sizeof(RaidPhysDiskPage0_t));

	hdr.PageVersion = MPI_RAIDPHYSDISKPAGE0_PAGEVERSION;
	hdr.PageType = MPI_CONFIG_PAGETYPE_RAID_PHYSDISK;
	cfg.cfghdr.hdr = &hdr;
	cfg.physAddr = -1;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;

	if (mpt_config(ioc, &cfg) != 0) {
		rc = -EFAULT;
		goto out;
	}

	if (!hdr.PageLength) {
		rc = -EFAULT;
		goto out;
	}

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.PageLength * 4,
	    &dma_handle);

	if (!buffer) {
		rc = -ENOMEM;
		goto out;
	}

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	cfg.pageAddr = phys_disk_num;

	if (mpt_config(ioc, &cfg) != 0) {
		rc = -EFAULT;
		goto out;
	}

	rc = 0;
	memcpy(phys_disk, buffer, sizeof(*buffer));
	phys_disk->MaxLBA = le32_to_cpu(buffer->MaxLBA);

 out:

	if (buffer)
		pci_free_consistent(ioc->pcidev, hdr.PageLength * 4, buffer,
		    dma_handle);

	return rc;
}

int
mpt_raid_phys_disk_get_num_paths(MPT_ADAPTER *ioc, u8 phys_disk_num)
{
	CONFIGPARMS		 	cfg;
	ConfigPageHeader_t	 	hdr;
	dma_addr_t			dma_handle;
	pRaidPhysDiskPage1_t		buffer = NULL;
	int				rc;

	memset(&cfg, 0 , sizeof(CONFIGPARMS));
	memset(&hdr, 0 , sizeof(ConfigPageHeader_t));

	hdr.PageVersion = MPI_RAIDPHYSDISKPAGE1_PAGEVERSION;
	hdr.PageType = MPI_CONFIG_PAGETYPE_RAID_PHYSDISK;
	hdr.PageNumber = 1;
	cfg.cfghdr.hdr = &hdr;
	cfg.physAddr = -1;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;

	if (mpt_config(ioc, &cfg) != 0) {
		rc = 0;
		goto out;
	}

	if (!hdr.PageLength) {
		rc = 0;
		goto out;
	}

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.PageLength * 4,
	    &dma_handle);

	if (!buffer) {
		rc = 0;
		goto out;
	}

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	cfg.pageAddr = phys_disk_num;

	if (mpt_config(ioc, &cfg) != 0) {
		rc = 0;
		goto out;
	}

	rc = buffer->NumPhysDiskPaths;
 out:

	if (buffer)
		pci_free_consistent(ioc->pcidev, hdr.PageLength * 4, buffer,
		    dma_handle);

	return rc;
}
EXPORT_SYMBOL(mpt_raid_phys_disk_get_num_paths);

int
mpt_raid_phys_disk_pg1(MPT_ADAPTER *ioc, u8 phys_disk_num,
		RaidPhysDiskPage1_t *phys_disk)
{
	CONFIGPARMS		 	cfg;
	ConfigPageHeader_t	 	hdr;
	dma_addr_t			dma_handle;
	pRaidPhysDiskPage1_t		buffer = NULL;
	int				rc;
	int				i;
	__le64				sas_address;

	memset(&cfg, 0 , sizeof(CONFIGPARMS));
	memset(&hdr, 0 , sizeof(ConfigPageHeader_t));
	rc = 0;

	hdr.PageVersion = MPI_RAIDPHYSDISKPAGE1_PAGEVERSION;
	hdr.PageType = MPI_CONFIG_PAGETYPE_RAID_PHYSDISK;
	hdr.PageNumber = 1;
	cfg.cfghdr.hdr = &hdr;
	cfg.physAddr = -1;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;

	if (mpt_config(ioc, &cfg) != 0) {
		rc = -EFAULT;
		goto out;
	}

	if (!hdr.PageLength) {
		rc = -EFAULT;
		goto out;
	}

	buffer = pci_alloc_consistent(ioc->pcidev, hdr.PageLength * 4,
	    &dma_handle);

	if (!buffer) {
		rc = -ENOMEM;
		goto out;
	}

	cfg.physAddr = dma_handle;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	cfg.pageAddr = phys_disk_num;

	if (mpt_config(ioc, &cfg) != 0) {
		rc = -EFAULT;
		goto out;
	}

	phys_disk->NumPhysDiskPaths = buffer->NumPhysDiskPaths;
	phys_disk->PhysDiskNum = phys_disk_num;
	for (i = 0; i < phys_disk->NumPhysDiskPaths; i++) {
		phys_disk->Path[i].PhysDiskID = buffer->Path[i].PhysDiskID;
		phys_disk->Path[i].PhysDiskBus = buffer->Path[i].PhysDiskBus;
		phys_disk->Path[i].OwnerIdentifier =
				buffer->Path[i].OwnerIdentifier;
		phys_disk->Path[i].Flags = le16_to_cpu(buffer->Path[i].Flags);
		memcpy(&sas_address, &buffer->Path[i].WWID, sizeof(__le64));
		sas_address = le64_to_cpu(sas_address);
		memcpy(&phys_disk->Path[i].WWID, &sas_address, sizeof(__le64));
		memcpy(&sas_address,
				&buffer->Path[i].OwnerWWID, sizeof(__le64));
		sas_address = le64_to_cpu(sas_address);
		memcpy(&phys_disk->Path[i].OwnerWWID,
				&sas_address, sizeof(__le64));
	}

 out:

	if (buffer)
		pci_free_consistent(ioc->pcidev, hdr.PageLength * 4, buffer,
		    dma_handle);

	return rc;
}
EXPORT_SYMBOL(mpt_raid_phys_disk_pg1);


int
mpt_findImVolumes(MPT_ADAPTER *ioc)
{
	IOCPage2_t		*pIoc2;
	u8			*mem;
	dma_addr_t		 ioc2_dma;
	CONFIGPARMS		 cfg;
	ConfigPageHeader_t	 header;
	int			 rc = 0;
	int			 iocpage2sz;
	int			 i;

	if (!ioc->ir_firmware)
		return 0;

	kfree(ioc->raid_data.pIocPg2);
	ioc->raid_data.pIocPg2 = NULL;
	mpt_inactive_raid_list_free(ioc);

	header.PageVersion = 0;
	header.PageLength = 0;
	header.PageNumber = 2;
	header.PageType = MPI_CONFIG_PAGETYPE_IOC;
	cfg.cfghdr.hdr = &header;
	cfg.physAddr = -1;
	cfg.pageAddr = 0;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;
	cfg.timeout = 0;
	if (mpt_config(ioc, &cfg) != 0)
		 return -EFAULT;

	if (header.PageLength == 0)
		return -EFAULT;

	iocpage2sz = header.PageLength * 4;
	pIoc2 = pci_alloc_consistent(ioc->pcidev, iocpage2sz, &ioc2_dma);
	if (!pIoc2)
		return -ENOMEM;

	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	cfg.physAddr = ioc2_dma;
	if (mpt_config(ioc, &cfg) != 0)
		goto out;

	mem = kmalloc(iocpage2sz, GFP_KERNEL);
	if (!mem) {
		rc = -ENOMEM;
		goto out;
	}

	memcpy(mem, (u8 *)pIoc2, iocpage2sz);
	ioc->raid_data.pIocPg2 = (IOCPage2_t *) mem;

	mpt_read_ioc_pg_3(ioc);

	for (i = 0; i < pIoc2->NumActiveVolumes ; i++)
		mpt_inactive_raid_volumes(ioc,
		    pIoc2->RaidVolume[i].VolumeBus,
		    pIoc2->RaidVolume[i].VolumeID);

 out:
	pci_free_consistent(ioc->pcidev, iocpage2sz, pIoc2, ioc2_dma);

	return rc;
}

static int
mpt_read_ioc_pg_3(MPT_ADAPTER *ioc)
{
	IOCPage3_t		*pIoc3;
	u8			*mem;
	CONFIGPARMS		 cfg;
	ConfigPageHeader_t	 header;
	dma_addr_t		 ioc3_dma;
	int			 iocpage3sz = 0;

	kfree(ioc->raid_data.pIocPg3);
	ioc->raid_data.pIocPg3 = NULL;

	header.PageVersion = 0;
	header.PageLength = 0;
	header.PageNumber = 3;
	header.PageType = MPI_CONFIG_PAGETYPE_IOC;
	cfg.cfghdr.hdr = &header;
	cfg.physAddr = -1;
	cfg.pageAddr = 0;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;
	cfg.timeout = 0;
	if (mpt_config(ioc, &cfg) != 0)
		return 0;

	if (header.PageLength == 0)
		return 0;

	iocpage3sz = header.PageLength * 4;
	pIoc3 = pci_alloc_consistent(ioc->pcidev, iocpage3sz, &ioc3_dma);
	if (!pIoc3)
		return 0;

	cfg.physAddr = ioc3_dma;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	if (mpt_config(ioc, &cfg) == 0) {
		mem = kmalloc(iocpage3sz, GFP_KERNEL);
		if (mem) {
			memcpy(mem, (u8 *)pIoc3, iocpage3sz);
			ioc->raid_data.pIocPg3 = (IOCPage3_t *) mem;
		}
	}

	pci_free_consistent(ioc->pcidev, iocpage3sz, pIoc3, ioc3_dma);

	return 0;
}

static void
mpt_read_ioc_pg_4(MPT_ADAPTER *ioc)
{
	IOCPage4_t		*pIoc4;
	CONFIGPARMS		 cfg;
	ConfigPageHeader_t	 header;
	dma_addr_t		 ioc4_dma;
	int			 iocpage4sz;

	header.PageVersion = 0;
	header.PageLength = 0;
	header.PageNumber = 4;
	header.PageType = MPI_CONFIG_PAGETYPE_IOC;
	cfg.cfghdr.hdr = &header;
	cfg.physAddr = -1;
	cfg.pageAddr = 0;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;
	cfg.timeout = 0;
	if (mpt_config(ioc, &cfg) != 0)
		return;

	if (header.PageLength == 0)
		return;

	if ( (pIoc4 = ioc->spi_data.pIocPg4) == NULL ) {
		iocpage4sz = (header.PageLength + 4) * 4; 
		pIoc4 = pci_alloc_consistent(ioc->pcidev, iocpage4sz, &ioc4_dma);
		if (!pIoc4)
			return;
		ioc->alloc_total += iocpage4sz;
	} else {
		ioc4_dma = ioc->spi_data.IocPg4_dma;
		iocpage4sz = ioc->spi_data.IocPg4Sz;
	}

	cfg.physAddr = ioc4_dma;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	if (mpt_config(ioc, &cfg) == 0) {
		ioc->spi_data.pIocPg4 = (IOCPage4_t *) pIoc4;
		ioc->spi_data.IocPg4_dma = ioc4_dma;
		ioc->spi_data.IocPg4Sz = iocpage4sz;
	} else {
		pci_free_consistent(ioc->pcidev, iocpage4sz, pIoc4, ioc4_dma);
		ioc->spi_data.pIocPg4 = NULL;
		ioc->alloc_total -= iocpage4sz;
	}
}

static void
mpt_read_ioc_pg_1(MPT_ADAPTER *ioc)
{
	IOCPage1_t		*pIoc1;
	CONFIGPARMS		 cfg;
	ConfigPageHeader_t	 header;
	dma_addr_t		 ioc1_dma;
	int			 iocpage1sz = 0;
	u32			 tmp;

	header.PageVersion = 0;
	header.PageLength = 0;
	header.PageNumber = 1;
	header.PageType = MPI_CONFIG_PAGETYPE_IOC;
	cfg.cfghdr.hdr = &header;
	cfg.physAddr = -1;
	cfg.pageAddr = 0;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.dir = 0;
	cfg.timeout = 0;
	if (mpt_config(ioc, &cfg) != 0)
		return;

	if (header.PageLength == 0)
		return;

	iocpage1sz = header.PageLength * 4;
	pIoc1 = pci_alloc_consistent(ioc->pcidev, iocpage1sz, &ioc1_dma);
	if (!pIoc1)
		return;

	cfg.physAddr = ioc1_dma;
	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	if (mpt_config(ioc, &cfg) == 0) {

		tmp = le32_to_cpu(pIoc1->Flags) & MPI_IOCPAGE1_REPLY_COALESCING;
		if (tmp == MPI_IOCPAGE1_REPLY_COALESCING) {
			tmp = le32_to_cpu(pIoc1->CoalescingTimeout);

			dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Coalescing Enabled Timeout = %d\n",
					ioc->name, tmp));

			if (tmp > MPT_COALESCING_TIMEOUT) {
				pIoc1->CoalescingTimeout = cpu_to_le32(MPT_COALESCING_TIMEOUT);

				cfg.dir = 1;
				cfg.action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
				if (mpt_config(ioc, &cfg) == 0) {
					dprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Reset Current Coalescing Timeout to = %d\n",
							ioc->name, MPT_COALESCING_TIMEOUT));

					cfg.action = MPI_CONFIG_ACTION_PAGE_WRITE_NVRAM;
					if (mpt_config(ioc, &cfg) == 0) {
						dprintk(ioc, printk(MYIOC_s_DEBUG_FMT
								"Reset NVRAM Coalescing Timeout to = %d\n",
								ioc->name, MPT_COALESCING_TIMEOUT));
					} else {
						dprintk(ioc, printk(MYIOC_s_DEBUG_FMT
								"Reset NVRAM Coalescing Timeout Failed\n",
								ioc->name));
					}

				} else {
					dprintk(ioc, printk(MYIOC_s_WARN_FMT
						"Reset of Current Coalescing Timeout Failed!\n",
						ioc->name));
				}
			}

		} else {
			dprintk(ioc, printk(MYIOC_s_WARN_FMT "Coalescing Disabled\n", ioc->name));
		}
	}

	pci_free_consistent(ioc->pcidev, iocpage1sz, pIoc1, ioc1_dma);

	return;
}

static void
mpt_get_manufacturing_pg_0(MPT_ADAPTER *ioc)
{
	CONFIGPARMS		cfg;
	ConfigPageHeader_t	hdr;
	dma_addr_t		buf_dma;
	ManufacturingPage0_t	*pbuf = NULL;

	memset(&cfg, 0 , sizeof(CONFIGPARMS));
	memset(&hdr, 0 , sizeof(ConfigPageHeader_t));

	hdr.PageType = MPI_CONFIG_PAGETYPE_MANUFACTURING;
	cfg.cfghdr.hdr = &hdr;
	cfg.physAddr = -1;
	cfg.action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfg.timeout = 10;

	if (mpt_config(ioc, &cfg) != 0)
		goto out;

	if (!cfg.cfghdr.hdr->PageLength)
		goto out;

	cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	pbuf = pci_alloc_consistent(ioc->pcidev, hdr.PageLength * 4, &buf_dma);
	if (!pbuf)
		goto out;

	cfg.physAddr = buf_dma;

	if (mpt_config(ioc, &cfg) != 0)
		goto out;

	memcpy(ioc->board_name, pbuf->BoardName, sizeof(ioc->board_name));
	memcpy(ioc->board_assembly, pbuf->BoardAssembly, sizeof(ioc->board_assembly));
	memcpy(ioc->board_tracer, pbuf->BoardTracerNumber, sizeof(ioc->board_tracer));

	out:

	if (pbuf)
		pci_free_consistent(ioc->pcidev, hdr.PageLength * 4, pbuf, buf_dma);
}

static int
SendEventNotification(MPT_ADAPTER *ioc, u8 EvSwitch, int sleepFlag)
{
	EventNotification_t	evn;
	MPIDefaultReply_t	reply_buf;

	memset(&evn, 0, sizeof(EventNotification_t));
	memset(&reply_buf, 0, sizeof(MPIDefaultReply_t));

	evn.Function = MPI_FUNCTION_EVENT_NOTIFICATION;
	evn.Switch = EvSwitch;
	evn.MsgContext = cpu_to_le32(mpt_base_index << 16);

	devtverboseprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "Sending EventNotification (%d) request %p\n",
	    ioc->name, EvSwitch, &evn));

	return mpt_handshake_req_reply_wait(ioc, sizeof(EventNotification_t),
	    (u32 *)&evn, sizeof(MPIDefaultReply_t), (u16 *)&reply_buf, 30,
	    sleepFlag);
}

static int
SendEventAck(MPT_ADAPTER *ioc, EventNotificationReply_t *evnp)
{
	EventAck_t	*pAck;

	if ((pAck = (EventAck_t *) mpt_get_msg_frame(mpt_base_index, ioc)) == NULL) {
		dfailprintk(ioc, printk(MYIOC_s_WARN_FMT "%s, no msg frames!!\n",
		    ioc->name, __func__));
		return -1;
	}

	devtverboseprintk(ioc, printk(MYIOC_s_DEBUG_FMT "Sending EventAck\n", ioc->name));

	pAck->Function     = MPI_FUNCTION_EVENT_ACK;
	pAck->ChainOffset  = 0;
	pAck->Reserved[0]  = pAck->Reserved[1] = 0;
	pAck->MsgFlags     = 0;
	pAck->Reserved1[0] = pAck->Reserved1[1] = pAck->Reserved1[2] = 0;
	pAck->Event        = evnp->Event;
	pAck->EventContext = evnp->EventContext;

	mpt_put_msg_frame(mpt_base_index, ioc, (MPT_FRAME_HDR *)pAck);

	return 0;
}

int
mpt_config(MPT_ADAPTER *ioc, CONFIGPARMS *pCfg)
{
	Config_t	*pReq;
	ConfigReply_t	*pReply;
	ConfigExtendedPageHeader_t  *pExtHdr = NULL;
	MPT_FRAME_HDR	*mf;
	int		 ii;
	int		 flagsLength;
	long		 timeout;
	int		 ret;
	u8		 page_type = 0, extend_page;
	unsigned long 	 timeleft;
	unsigned long	 flags;
    int		 in_isr;
	u8		 issue_hard_reset = 0;
	u8		 retry_count = 0;

	in_isr = in_interrupt();
	if (in_isr) {
		dcprintk(ioc, printk(MYIOC_s_WARN_FMT "Config request not allowed in ISR context!\n",
				ioc->name));
		return -EPERM;
    }

	
	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	if (ioc->ioc_reset_in_progress) {
		dfailprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: busy with host reset\n", ioc->name, __func__));
		spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
		return -EBUSY;
	}
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);

	
	if (!ioc->active ||
	    mpt_GetIocState(ioc, 1) != MPI_IOC_STATE_OPERATIONAL) {
		dfailprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: ioc not operational, %d, %xh\n",
		    ioc->name, __func__, ioc->active,
		    mpt_GetIocState(ioc, 0)));
		return -EFAULT;
	}

 retry_config:
	mutex_lock(&ioc->mptbase_cmds.mutex);
	
	memset(ioc->mptbase_cmds.reply, 0 , MPT_DEFAULT_FRAME_SIZE);
	INITIALIZE_MGMT_STATUS(ioc->mptbase_cmds.status)

	if ((mf = mpt_get_msg_frame(mpt_base_index, ioc)) == NULL) {
		dcprintk(ioc, printk(MYIOC_s_WARN_FMT
		"mpt_config: no msg frames!\n", ioc->name));
		ret = -EAGAIN;
		goto out;
	}

	pReq = (Config_t *)mf;
	pReq->Action = pCfg->action;
	pReq->Reserved = 0;
	pReq->ChainOffset = 0;
	pReq->Function = MPI_FUNCTION_CONFIG;

	
	pReq->ExtPageLength = 0;
	pReq->ExtPageType = 0;
	pReq->MsgFlags = 0;

	for (ii=0; ii < 8; ii++)
		pReq->Reserved2[ii] = 0;

	pReq->Header.PageVersion = pCfg->cfghdr.hdr->PageVersion;
	pReq->Header.PageLength = pCfg->cfghdr.hdr->PageLength;
	pReq->Header.PageNumber = pCfg->cfghdr.hdr->PageNumber;
	pReq->Header.PageType = (pCfg->cfghdr.hdr->PageType & MPI_CONFIG_PAGETYPE_MASK);

	if ((pCfg->cfghdr.hdr->PageType & MPI_CONFIG_PAGETYPE_MASK) == MPI_CONFIG_PAGETYPE_EXTENDED) {
		pExtHdr = (ConfigExtendedPageHeader_t *)pCfg->cfghdr.ehdr;
		pReq->ExtPageLength = cpu_to_le16(pExtHdr->ExtPageLength);
		pReq->ExtPageType = pExtHdr->ExtPageType;
		pReq->Header.PageType = MPI_CONFIG_PAGETYPE_EXTENDED;

		pReq->Header.PageLength = 0;
	}

	pReq->PageAddress = cpu_to_le32(pCfg->pageAddr);

	if (pCfg->dir)
		flagsLength = MPT_SGE_FLAGS_SSIMPLE_WRITE;
	else
		flagsLength = MPT_SGE_FLAGS_SSIMPLE_READ;

	if ((pCfg->cfghdr.hdr->PageType & MPI_CONFIG_PAGETYPE_MASK) ==
	    MPI_CONFIG_PAGETYPE_EXTENDED) {
		flagsLength |= pExtHdr->ExtPageLength * 4;
		page_type = pReq->ExtPageType;
		extend_page = 1;
	} else {
		flagsLength |= pCfg->cfghdr.hdr->PageLength * 4;
		page_type = pReq->Header.PageType;
		extend_page = 0;
	}

	dcprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "Sending Config request type 0x%x, page 0x%x and action %d\n",
	    ioc->name, page_type, pReq->Header.PageNumber, pReq->Action));

	ioc->add_sge((char *)&pReq->PageBufferSGE, flagsLength, pCfg->physAddr);
	timeout = (pCfg->timeout < 15) ? HZ*15 : HZ*pCfg->timeout;
	mpt_put_msg_frame(mpt_base_index, ioc, mf);
	timeleft = wait_for_completion_timeout(&ioc->mptbase_cmds.done,
		timeout);
	if (!(ioc->mptbase_cmds.status & MPT_MGMT_STATUS_COMMAND_GOOD)) {
		ret = -ETIME;
		dfailprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "Failed Sending Config request type 0x%x, page 0x%x,"
		    " action %d, status %xh, time left %ld\n\n",
			ioc->name, page_type, pReq->Header.PageNumber,
			pReq->Action, ioc->mptbase_cmds.status, timeleft));
		if (ioc->mptbase_cmds.status & MPT_MGMT_STATUS_DID_IOCRESET)
			goto out;
		if (!timeleft) {
			spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
			if (ioc->ioc_reset_in_progress) {
				spin_unlock_irqrestore(&ioc->taskmgmt_lock,
					flags);
				printk(MYIOC_s_INFO_FMT "%s: host reset in"
					" progress mpt_config timed out.!!\n",
					__func__, ioc->name);
				return -EFAULT;
			}
			spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
			issue_hard_reset = 1;
		}
		goto out;
	}

	if (!(ioc->mptbase_cmds.status & MPT_MGMT_STATUS_RF_VALID)) {
		ret = -1;
		goto out;
	}
	pReply = (ConfigReply_t	*)ioc->mptbase_cmds.reply;
	ret = le16_to_cpu(pReply->IOCStatus) & MPI_IOCSTATUS_MASK;
	if (ret == MPI_IOCSTATUS_SUCCESS) {
		if (extend_page) {
			pCfg->cfghdr.ehdr->ExtPageLength =
			    le16_to_cpu(pReply->ExtPageLength);
			pCfg->cfghdr.ehdr->ExtPageType =
			    pReply->ExtPageType;
		}
		pCfg->cfghdr.hdr->PageVersion = pReply->Header.PageVersion;
		pCfg->cfghdr.hdr->PageLength = pReply->Header.PageLength;
		pCfg->cfghdr.hdr->PageNumber = pReply->Header.PageNumber;
		pCfg->cfghdr.hdr->PageType = pReply->Header.PageType;

	}

	if (retry_count)
		printk(MYIOC_s_INFO_FMT "Retry completed "
		    "ret=0x%x timeleft=%ld\n",
		    ioc->name, ret, timeleft);

	dcprintk(ioc, printk(KERN_DEBUG "IOCStatus=%04xh, IOCLogInfo=%08xh\n",
	     ret, le32_to_cpu(pReply->IOCLogInfo)));

out:

	CLEAR_MGMT_STATUS(ioc->mptbase_cmds.status)
	mutex_unlock(&ioc->mptbase_cmds.mutex);
	if (issue_hard_reset) {
		issue_hard_reset = 0;
		printk(MYIOC_s_WARN_FMT
		       "Issuing Reset from %s!!, doorbell=0x%08x\n",
		       ioc->name, __func__, mpt_GetIocState(ioc, 0));
		if (retry_count == 0) {
			if (mpt_Soft_Hard_ResetHandler(ioc, CAN_SLEEP) != 0)
				retry_count++;
		} else
			mpt_HardResetHandler(ioc, CAN_SLEEP);

		mpt_free_msg_frame(ioc, mf);
		
		if (retry_count < 2) {
			printk(MYIOC_s_INFO_FMT
			    "Attempting Retry Config request"
			    " type 0x%x, page 0x%x,"
			    " action %d\n", ioc->name, page_type,
			    pCfg->cfghdr.hdr->PageNumber, pCfg->action);
			retry_count++;
			goto retry_config;
		}
	}
	return ret;

}

static int
mpt_ioc_reset(MPT_ADAPTER *ioc, int reset_phase)
{
	switch (reset_phase) {
	case MPT_IOC_SETUP_RESET:
		ioc->taskmgmt_quiesce_io = 1;
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: MPT_IOC_SETUP_RESET\n", ioc->name, __func__));
		break;
	case MPT_IOC_PRE_RESET:
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: MPT_IOC_PRE_RESET\n", ioc->name, __func__));
		break;
	case MPT_IOC_POST_RESET:
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "%s: MPT_IOC_POST_RESET\n",  ioc->name, __func__));
		if (ioc->mptbase_cmds.status & MPT_MGMT_STATUS_PENDING) {
			ioc->mptbase_cmds.status |=
			    MPT_MGMT_STATUS_DID_IOCRESET;
			complete(&ioc->mptbase_cmds.done);
		}
		if (ioc->taskmgmt_cmds.status & MPT_MGMT_STATUS_PENDING) {
			ioc->taskmgmt_cmds.status |=
				MPT_MGMT_STATUS_DID_IOCRESET;
			complete(&ioc->taskmgmt_cmds.done);
		}
		break;
	default:
		break;
	}

	return 1;		
}


#ifdef CONFIG_PROC_FS		
static int
procmpt_create(void)
{
	mpt_proc_root_dir = proc_mkdir(MPT_PROCFS_MPTBASEDIR, NULL);
	if (mpt_proc_root_dir == NULL)
		return -ENOTDIR;

	proc_create("summary", S_IRUGO, mpt_proc_root_dir, &mpt_summary_proc_fops);
	proc_create("version", S_IRUGO, mpt_proc_root_dir, &mpt_version_proc_fops);
	return 0;
}

static void
procmpt_destroy(void)
{
	remove_proc_entry("version", mpt_proc_root_dir);
	remove_proc_entry("summary", mpt_proc_root_dir);
	remove_proc_entry(MPT_PROCFS_MPTBASEDIR, NULL);
}

static void seq_mpt_print_ioc_summary(MPT_ADAPTER *ioc, struct seq_file *m, int showlan);

static int mpt_summary_proc_show(struct seq_file *m, void *v)
{
	MPT_ADAPTER *ioc = m->private;

	if (ioc) {
		seq_mpt_print_ioc_summary(ioc, m, 1);
	} else {
		list_for_each_entry(ioc, &ioc_list, list) {
			seq_mpt_print_ioc_summary(ioc, m, 1);
		}
	}

	return 0;
}

static int mpt_summary_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mpt_summary_proc_show, PDE(inode)->data);
}

static const struct file_operations mpt_summary_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= mpt_summary_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mpt_version_proc_show(struct seq_file *m, void *v)
{
	u8	 cb_idx;
	int	 scsi, fc, sas, lan, ctl, targ, dmp;
	char	*drvname;

	seq_printf(m, "%s-%s\n", "mptlinux", MPT_LINUX_VERSION_COMMON);
	seq_printf(m, "  Fusion MPT base driver\n");

	scsi = fc = sas = lan = ctl = targ = dmp = 0;
	for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--) {
		drvname = NULL;
		if (MptCallbacks[cb_idx]) {
			switch (MptDriverClass[cb_idx]) {
			case MPTSPI_DRIVER:
				if (!scsi++) drvname = "SPI host";
				break;
			case MPTFC_DRIVER:
				if (!fc++) drvname = "FC host";
				break;
			case MPTSAS_DRIVER:
				if (!sas++) drvname = "SAS host";
				break;
			case MPTLAN_DRIVER:
				if (!lan++) drvname = "LAN";
				break;
			case MPTSTM_DRIVER:
				if (!targ++) drvname = "SCSI target";
				break;
			case MPTCTL_DRIVER:
				if (!ctl++) drvname = "ioctl";
				break;
			}

			if (drvname)
				seq_printf(m, "  Fusion MPT %s driver\n", drvname);
		}
	}

	return 0;
}

static int mpt_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mpt_version_proc_show, NULL);
}

static const struct file_operations mpt_version_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= mpt_version_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mpt_iocinfo_proc_show(struct seq_file *m, void *v)
{
	MPT_ADAPTER	*ioc = m->private;
	char		 expVer[32];
	int		 sz;
	int		 p;

	mpt_get_fw_exp_ver(expVer, ioc);

	seq_printf(m, "%s:", ioc->name);
	if (ioc->facts.Flags & MPI_IOCFACTS_FLAGS_FW_DOWNLOAD_BOOT)
		seq_printf(m, "  (f/w download boot flag set)");

	seq_printf(m, "\n  ProductID = 0x%04x (%s)\n",
			ioc->facts.ProductID,
			ioc->prod_name);
	seq_printf(m, "  FWVersion = 0x%08x%s", ioc->facts.FWVersion.Word, expVer);
	if (ioc->facts.FWImageSize)
		seq_printf(m, " (fw_size=%d)", ioc->facts.FWImageSize);
	seq_printf(m, "\n  MsgVersion = 0x%04x\n", ioc->facts.MsgVersion);
	seq_printf(m, "  FirstWhoInit = 0x%02x\n", ioc->FirstWhoInit);
	seq_printf(m, "  EventState = 0x%02x\n", ioc->facts.EventState);

	seq_printf(m, "  CurrentHostMfaHighAddr = 0x%08x\n",
			ioc->facts.CurrentHostMfaHighAddr);
	seq_printf(m, "  CurrentSenseBufferHighAddr = 0x%08x\n",
			ioc->facts.CurrentSenseBufferHighAddr);

	seq_printf(m, "  MaxChainDepth = 0x%02x frames\n", ioc->facts.MaxChainDepth);
	seq_printf(m, "  MinBlockSize = 0x%02x bytes\n", 4*ioc->facts.BlockSize);

	seq_printf(m, "  RequestFrames @ 0x%p (Dma @ 0x%p)\n",
					(void *)ioc->req_frames, (void *)(ulong)ioc->req_frames_dma);
	sz = (ioc->req_sz * ioc->req_depth) + 128;
	sz = ((sz + 0x1000UL - 1UL) / 0x1000) * 0x1000;
	seq_printf(m, "    {CurReqSz=%d} x {CurReqDepth=%d} = %d bytes ^= 0x%x\n",
					ioc->req_sz, ioc->req_depth, ioc->req_sz*ioc->req_depth, sz);
	seq_printf(m, "    {MaxReqSz=%d}   {MaxReqDepth=%d}\n",
					4*ioc->facts.RequestFrameSize,
					ioc->facts.GlobalCredits);

	seq_printf(m, "  Frames   @ 0x%p (Dma @ 0x%p)\n",
					(void *)ioc->alloc, (void *)(ulong)ioc->alloc_dma);
	sz = (ioc->reply_sz * ioc->reply_depth) + 128;
	seq_printf(m, "    {CurRepSz=%d} x {CurRepDepth=%d} = %d bytes ^= 0x%x\n",
					ioc->reply_sz, ioc->reply_depth, ioc->reply_sz*ioc->reply_depth, sz);
	seq_printf(m, "    {MaxRepSz=%d}   {MaxRepDepth=%d}\n",
					ioc->facts.CurReplyFrameSize,
					ioc->facts.ReplyQueueDepth);

	seq_printf(m, "  MaxDevices = %d\n",
			(ioc->facts.MaxDevices==0) ? 255 : ioc->facts.MaxDevices);
	seq_printf(m, "  MaxBuses = %d\n", ioc->facts.MaxBuses);

	
	for (p=0; p < ioc->facts.NumberOfPorts; p++) {
		seq_printf(m, "  PortNumber = %d (of %d)\n",
				p+1,
				ioc->facts.NumberOfPorts);
		if (ioc->bus_type == FC) {
			if (ioc->pfacts[p].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_LAN) {
				u8 *a = (u8*)&ioc->lan_cnfg_page1.HardwareAddressLow;
				seq_printf(m, "    LanAddr = %02X:%02X:%02X:%02X:%02X:%02X\n",
						a[5], a[4], a[3], a[2], a[1], a[0]);
			}
			seq_printf(m, "    WWN = %08X%08X:%08X%08X\n",
					ioc->fc_port_page0[p].WWNN.High,
					ioc->fc_port_page0[p].WWNN.Low,
					ioc->fc_port_page0[p].WWPN.High,
					ioc->fc_port_page0[p].WWPN.Low);
		}
	}

	return 0;
}

static int mpt_iocinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mpt_iocinfo_proc_show, PDE(inode)->data);
}

static const struct file_operations mpt_iocinfo_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= mpt_iocinfo_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif		

static void
mpt_get_fw_exp_ver(char *buf, MPT_ADAPTER *ioc)
{
	buf[0] ='\0';
	if ((ioc->facts.FWVersion.Word >> 24) == 0x0E) {
		sprintf(buf, " (Exp %02d%02d)",
			(ioc->facts.FWVersion.Word >> 16) & 0x00FF,	
			(ioc->facts.FWVersion.Word >> 8) & 0x1F);	

		
		if ((ioc->facts.FWVersion.Word >> 8) & 0x80)
			strcat(buf, " [MDBG]");
	}
}

/**
 *	mpt_print_ioc_summary - Write ASCII summary of IOC to a buffer.
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@buffer: Pointer to buffer where IOC summary info should be written
 *	@size: Pointer to number of bytes we wrote (set by this routine)
 *	@len: Offset at which to start writing in buffer
 *	@showlan: Display LAN stuff?
 *
 *	This routine writes (english readable) ASCII text, which represents
 *	a summary of IOC information, to a buffer.
 */
void
mpt_print_ioc_summary(MPT_ADAPTER *ioc, char *buffer, int *size, int len, int showlan)
{
	char expVer[32];
	int y;

	mpt_get_fw_exp_ver(expVer, ioc);

	y = sprintf(buffer+len, "%s: %s, %s%08xh%s, Ports=%d, MaxQ=%d",
			ioc->name,
			ioc->prod_name,
			MPT_FW_REV_MAGIC_ID_STRING,	
			ioc->facts.FWVersion.Word,
			expVer,
			ioc->facts.NumberOfPorts,
			ioc->req_depth);

	if (showlan && (ioc->pfacts[0].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_LAN)) {
		u8 *a = (u8*)&ioc->lan_cnfg_page1.HardwareAddressLow;
		y += sprintf(buffer+len+y, ", LanAddr=%02X:%02X:%02X:%02X:%02X:%02X",
			a[5], a[4], a[3], a[2], a[1], a[0]);
	}

	y += sprintf(buffer+len+y, ", IRQ=%d", ioc->pci_irq);

	if (!ioc->active)
		y += sprintf(buffer+len+y, " (disabled)");

	y += sprintf(buffer+len+y, "\n");

	*size = y;
}

static void seq_mpt_print_ioc_summary(MPT_ADAPTER *ioc, struct seq_file *m, int showlan)
{
	char expVer[32];

	mpt_get_fw_exp_ver(expVer, ioc);

	seq_printf(m, "%s: %s, %s%08xh%s, Ports=%d, MaxQ=%d",
			ioc->name,
			ioc->prod_name,
			MPT_FW_REV_MAGIC_ID_STRING,	
			ioc->facts.FWVersion.Word,
			expVer,
			ioc->facts.NumberOfPorts,
			ioc->req_depth);

	if (showlan && (ioc->pfacts[0].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_LAN)) {
		u8 *a = (u8*)&ioc->lan_cnfg_page1.HardwareAddressLow;
		seq_printf(m, ", LanAddr=%02X:%02X:%02X:%02X:%02X:%02X",
			a[5], a[4], a[3], a[2], a[1], a[0]);
	}

	seq_printf(m, ", IRQ=%d", ioc->pci_irq);

	if (!ioc->active)
		seq_printf(m, " (disabled)");

	seq_putc(m, '\n');
}

int
mpt_set_taskmgmt_in_progress_flag(MPT_ADAPTER *ioc)
{
	unsigned long	 flags;
	int		 retval;

	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	if (ioc->ioc_reset_in_progress || ioc->taskmgmt_in_progress ||
	    (ioc->alt_ioc && ioc->alt_ioc->taskmgmt_in_progress)) {
		retval = -1;
		goto out;
	}
	retval = 0;
	ioc->taskmgmt_in_progress = 1;
	ioc->taskmgmt_quiesce_io = 1;
	if (ioc->alt_ioc) {
		ioc->alt_ioc->taskmgmt_in_progress = 1;
		ioc->alt_ioc->taskmgmt_quiesce_io = 1;
	}
 out:
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
	return retval;
}
EXPORT_SYMBOL(mpt_set_taskmgmt_in_progress_flag);

void
mpt_clear_taskmgmt_in_progress_flag(MPT_ADAPTER *ioc)
{
	unsigned long	 flags;

	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	ioc->taskmgmt_in_progress = 0;
	ioc->taskmgmt_quiesce_io = 0;
	if (ioc->alt_ioc) {
		ioc->alt_ioc->taskmgmt_in_progress = 0;
		ioc->alt_ioc->taskmgmt_quiesce_io = 0;
	}
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
}
EXPORT_SYMBOL(mpt_clear_taskmgmt_in_progress_flag);


void
mpt_halt_firmware(MPT_ADAPTER *ioc)
{
	u32	 ioc_raw_state;

	ioc_raw_state = mpt_GetIocState(ioc, 0);

	if ((ioc_raw_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_FAULT) {
		printk(MYIOC_s_ERR_FMT "IOC is in FAULT state (%04xh)!!!\n",
			ioc->name, ioc_raw_state & MPI_DOORBELL_DATA_MASK);
		panic("%s: IOC Fault (%04xh)!!!\n", ioc->name,
			ioc_raw_state & MPI_DOORBELL_DATA_MASK);
	} else {
		CHIPREG_WRITE32(&ioc->chip->Doorbell, 0xC0FFEE00);
		panic("%s: Firmware is halted due to command timeout\n",
			ioc->name);
	}
}
EXPORT_SYMBOL(mpt_halt_firmware);

int
mpt_SoftResetHandler(MPT_ADAPTER *ioc, int sleepFlag)
{
	int		 rc;
	int		 ii;
	u8		 cb_idx;
	unsigned long	 flags;
	u32		 ioc_state;
	unsigned long	 time_count;

	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT "SoftResetHandler Entered!\n",
		ioc->name));

	ioc_state = mpt_GetIocState(ioc, 0) & MPI_IOC_STATE_MASK;

	if (mpt_fwfault_debug)
		mpt_halt_firmware(ioc);

	if (ioc_state == MPI_IOC_STATE_FAULT ||
	    ioc_state == MPI_IOC_STATE_RESET) {
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "skipping, either in FAULT or RESET state!\n", ioc->name));
		return -1;
	}

	if (ioc->bus_type == FC) {
		dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		    "skipping, because the bus type is FC!\n", ioc->name));
		return -1;
	}

	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	if (ioc->ioc_reset_in_progress) {
		spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
		return -1;
	}
	ioc->ioc_reset_in_progress = 1;
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);

	rc = -1;

	for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--) {
		if (MptResetHandlers[cb_idx])
			mpt_signal_reset(cb_idx, ioc, MPT_IOC_SETUP_RESET);
	}

	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	if (ioc->taskmgmt_in_progress) {
		ioc->ioc_reset_in_progress = 0;
		spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
		return -1;
	}
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
	
	CHIPREG_WRITE32(&ioc->chip->IntMask, 0xFFFFFFFF);
	ioc->active = 0;
	time_count = jiffies;

	rc = SendIocReset(ioc, MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET, sleepFlag);

	for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--) {
		if (MptResetHandlers[cb_idx])
			mpt_signal_reset(cb_idx, ioc, MPT_IOC_PRE_RESET);
	}

	if (rc)
		goto out;

	ioc_state = mpt_GetIocState(ioc, 0) & MPI_IOC_STATE_MASK;
	if (ioc_state != MPI_IOC_STATE_READY)
		goto out;

	for (ii = 0; ii < 5; ii++) {
		
		rc = GetIocFacts(ioc, sleepFlag,
			MPT_HOSTEVENT_IOC_RECOVER);
		if (rc == 0)
			break;
		if (sleepFlag == CAN_SLEEP)
			msleep(100);
		else
			mdelay(100);
	}
	if (ii == 5)
		goto out;

	rc = PrimeIocFifos(ioc);
	if (rc != 0)
		goto out;

	rc = SendIocInit(ioc, sleepFlag);
	if (rc != 0)
		goto out;

	rc = SendEventNotification(ioc, 1, sleepFlag);
	if (rc != 0)
		goto out;

	if (ioc->hard_resets < -1)
		ioc->hard_resets++;


	ioc->active = 1;
	CHIPREG_WRITE32(&ioc->chip->IntMask, MPI_HIM_DIM);

 out:
	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	ioc->ioc_reset_in_progress = 0;
	ioc->taskmgmt_quiesce_io = 0;
	ioc->taskmgmt_in_progress = 0;
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);

	if (ioc->active) {	
		for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--) {
			if (MptResetHandlers[cb_idx])
				mpt_signal_reset(cb_idx, ioc,
					MPT_IOC_POST_RESET);
		}
	}

	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT
		"SoftResetHandler: completed (%d seconds): %s\n",
		ioc->name, jiffies_to_msecs(jiffies - time_count)/1000,
		((rc == 0) ? "SUCCESS" : "FAILED")));

	return rc;
}

int
mpt_Soft_Hard_ResetHandler(MPT_ADAPTER *ioc, int sleepFlag) {
	int ret = -1;

	ret = mpt_SoftResetHandler(ioc, sleepFlag);
	if (ret == 0)
		return ret;
	ret = mpt_HardResetHandler(ioc, sleepFlag);
	return ret;
}
EXPORT_SYMBOL(mpt_Soft_Hard_ResetHandler);

int
mpt_HardResetHandler(MPT_ADAPTER *ioc, int sleepFlag)
{
	int	 rc;
	u8	 cb_idx;
	unsigned long	 flags;
	unsigned long	 time_count;

	dtmprintk(ioc, printk(MYIOC_s_DEBUG_FMT "HardResetHandler Entered!\n", ioc->name));
#ifdef MFCNT
	printk(MYIOC_s_INFO_FMT "HardResetHandler Entered!\n", ioc->name);
	printk("MF count 0x%x !\n", ioc->mfcnt);
#endif
	if (mpt_fwfault_debug)
		mpt_halt_firmware(ioc);

	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	if (ioc->ioc_reset_in_progress) {
		spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
		ioc->wait_on_reset_completion = 1;
		do {
			ssleep(1);
		} while (ioc->ioc_reset_in_progress == 1);
		ioc->wait_on_reset_completion = 0;
		return ioc->reset_status;
	}
	if (ioc->wait_on_reset_completion) {
		spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);
		rc = 0;
		time_count = jiffies;
		goto exit;
	}
	ioc->ioc_reset_in_progress = 1;
	if (ioc->alt_ioc)
		ioc->alt_ioc->ioc_reset_in_progress = 1;
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);


	for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--) {
		if (MptResetHandlers[cb_idx]) {
			mpt_signal_reset(cb_idx, ioc, MPT_IOC_SETUP_RESET);
			if (ioc->alt_ioc)
				mpt_signal_reset(cb_idx, ioc->alt_ioc,
					MPT_IOC_SETUP_RESET);
		}
	}

	time_count = jiffies;
	rc = mpt_do_ioc_recovery(ioc, MPT_HOSTEVENT_IOC_RECOVER, sleepFlag);
	if (rc != 0) {
		printk(KERN_WARNING MYNAM
		       ": WARNING - (%d) Cannot recover %s, doorbell=0x%08x\n",
		       rc, ioc->name, mpt_GetIocState(ioc, 0));
	} else {
		if (ioc->hard_resets < -1)
			ioc->hard_resets++;
	}

	spin_lock_irqsave(&ioc->taskmgmt_lock, flags);
	ioc->ioc_reset_in_progress = 0;
	ioc->taskmgmt_quiesce_io = 0;
	ioc->taskmgmt_in_progress = 0;
	ioc->reset_status = rc;
	if (ioc->alt_ioc) {
		ioc->alt_ioc->ioc_reset_in_progress = 0;
		ioc->alt_ioc->taskmgmt_quiesce_io = 0;
		ioc->alt_ioc->taskmgmt_in_progress = 0;
	}
	spin_unlock_irqrestore(&ioc->taskmgmt_lock, flags);

	for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--) {
		if (MptResetHandlers[cb_idx]) {
			mpt_signal_reset(cb_idx, ioc, MPT_IOC_POST_RESET);
			if (ioc->alt_ioc)
				mpt_signal_reset(cb_idx,
					ioc->alt_ioc, MPT_IOC_POST_RESET);
		}
	}
exit:
	dtmprintk(ioc,
	    printk(MYIOC_s_DEBUG_FMT
		"HardResetHandler: completed (%d seconds): %s\n", ioc->name,
		jiffies_to_msecs(jiffies - time_count)/1000, ((rc == 0) ?
		"SUCCESS" : "FAILED")));

	return rc;
}

#ifdef CONFIG_FUSION_LOGGING
static void
mpt_display_event_info(MPT_ADAPTER *ioc, EventNotificationReply_t *pEventReply)
{
	char *ds = NULL;
	u32 evData0;
	int ii;
	u8 event;
	char *evStr = ioc->evStr;

	event = le32_to_cpu(pEventReply->Event) & 0xFF;
	evData0 = le32_to_cpu(pEventReply->Data[0]);

	switch(event) {
	case MPI_EVENT_NONE:
		ds = "None";
		break;
	case MPI_EVENT_LOG_DATA:
		ds = "Log Data";
		break;
	case MPI_EVENT_STATE_CHANGE:
		ds = "State Change";
		break;
	case MPI_EVENT_UNIT_ATTENTION:
		ds = "Unit Attention";
		break;
	case MPI_EVENT_IOC_BUS_RESET:
		ds = "IOC Bus Reset";
		break;
	case MPI_EVENT_EXT_BUS_RESET:
		ds = "External Bus Reset";
		break;
	case MPI_EVENT_RESCAN:
		ds = "Bus Rescan Event";
		break;
	case MPI_EVENT_LINK_STATUS_CHANGE:
		if (evData0 == MPI_EVENT_LINK_STATUS_FAILURE)
			ds = "Link Status(FAILURE) Change";
		else
			ds = "Link Status(ACTIVE) Change";
		break;
	case MPI_EVENT_LOOP_STATE_CHANGE:
		if (evData0 == MPI_EVENT_LOOP_STATE_CHANGE_LIP)
			ds = "Loop State(LIP) Change";
		else if (evData0 == MPI_EVENT_LOOP_STATE_CHANGE_LPE)
			ds = "Loop State(LPE) Change";
		else
			ds = "Loop State(LPB) Change";
		break;
	case MPI_EVENT_LOGOUT:
		ds = "Logout";
		break;
	case MPI_EVENT_EVENT_CHANGE:
		if (evData0)
			ds = "Events ON";
		else
			ds = "Events OFF";
		break;
	case MPI_EVENT_INTEGRATED_RAID:
	{
		u8 ReasonCode = (u8)(evData0 >> 16);
		switch (ReasonCode) {
		case MPI_EVENT_RAID_RC_VOLUME_CREATED :
			ds = "Integrated Raid: Volume Created";
			break;
		case MPI_EVENT_RAID_RC_VOLUME_DELETED :
			ds = "Integrated Raid: Volume Deleted";
			break;
		case MPI_EVENT_RAID_RC_VOLUME_SETTINGS_CHANGED :
			ds = "Integrated Raid: Volume Settings Changed";
			break;
		case MPI_EVENT_RAID_RC_VOLUME_STATUS_CHANGED :
			ds = "Integrated Raid: Volume Status Changed";
			break;
		case MPI_EVENT_RAID_RC_VOLUME_PHYSDISK_CHANGED :
			ds = "Integrated Raid: Volume Physdisk Changed";
			break;
		case MPI_EVENT_RAID_RC_PHYSDISK_CREATED :
			ds = "Integrated Raid: Physdisk Created";
			break;
		case MPI_EVENT_RAID_RC_PHYSDISK_DELETED :
			ds = "Integrated Raid: Physdisk Deleted";
			break;
		case MPI_EVENT_RAID_RC_PHYSDISK_SETTINGS_CHANGED :
			ds = "Integrated Raid: Physdisk Settings Changed";
			break;
		case MPI_EVENT_RAID_RC_PHYSDISK_STATUS_CHANGED :
			ds = "Integrated Raid: Physdisk Status Changed";
			break;
		case MPI_EVENT_RAID_RC_DOMAIN_VAL_NEEDED :
			ds = "Integrated Raid: Domain Validation Needed";
			break;
		case MPI_EVENT_RAID_RC_SMART_DATA :
			ds = "Integrated Raid; Smart Data";
			break;
		case MPI_EVENT_RAID_RC_REPLACE_ACTION_STARTED :
			ds = "Integrated Raid: Replace Action Started";
			break;
		default:
			ds = "Integrated Raid";
		break;
		}
		break;
	}
	case MPI_EVENT_SCSI_DEVICE_STATUS_CHANGE:
		ds = "SCSI Device Status Change";
		break;
	case MPI_EVENT_SAS_DEVICE_STATUS_CHANGE:
	{
		u8 id = (u8)(evData0);
		u8 channel = (u8)(evData0 >> 8);
		u8 ReasonCode = (u8)(evData0 >> 16);
		switch (ReasonCode) {
		case MPI_EVENT_SAS_DEV_STAT_RC_ADDED:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS Device Status Change: Added: "
			    "id=%d channel=%d", id, channel);
			break;
		case MPI_EVENT_SAS_DEV_STAT_RC_NOT_RESPONDING:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS Device Status Change: Deleted: "
			    "id=%d channel=%d", id, channel);
			break;
		case MPI_EVENT_SAS_DEV_STAT_RC_SMART_DATA:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS Device Status Change: SMART Data: "
			    "id=%d channel=%d", id, channel);
			break;
		case MPI_EVENT_SAS_DEV_STAT_RC_NO_PERSIST_ADDED:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS Device Status Change: No Persistancy: "
			    "id=%d channel=%d", id, channel);
			break;
		case MPI_EVENT_SAS_DEV_STAT_RC_UNSUPPORTED:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS Device Status Change: Unsupported Device "
			    "Discovered : id=%d channel=%d", id, channel);
			break;
		case MPI_EVENT_SAS_DEV_STAT_RC_INTERNAL_DEVICE_RESET:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS Device Status Change: Internal Device "
			    "Reset : id=%d channel=%d", id, channel);
			break;
		case MPI_EVENT_SAS_DEV_STAT_RC_TASK_ABORT_INTERNAL:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS Device Status Change: Internal Task "
			    "Abort : id=%d channel=%d", id, channel);
			break;
		case MPI_EVENT_SAS_DEV_STAT_RC_ABORT_TASK_SET_INTERNAL:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS Device Status Change: Internal Abort "
			    "Task Set : id=%d channel=%d", id, channel);
			break;
		case MPI_EVENT_SAS_DEV_STAT_RC_CLEAR_TASK_SET_INTERNAL:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS Device Status Change: Internal Clear "
			    "Task Set : id=%d channel=%d", id, channel);
			break;
		case MPI_EVENT_SAS_DEV_STAT_RC_QUERY_TASK_INTERNAL:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS Device Status Change: Internal Query "
			    "Task : id=%d channel=%d", id, channel);
			break;
		default:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS Device Status Change: Unknown: "
			    "id=%d channel=%d", id, channel);
			break;
		}
		break;
	}
	case MPI_EVENT_ON_BUS_TIMER_EXPIRED:
		ds = "Bus Timer Expired";
		break;
	case MPI_EVENT_QUEUE_FULL:
	{
		u16 curr_depth = (u16)(evData0 >> 16);
		u8 channel = (u8)(evData0 >> 8);
		u8 id = (u8)(evData0);

		snprintf(evStr, EVENT_DESCR_STR_SZ,
		   "Queue Full: channel=%d id=%d depth=%d",
		   channel, id, curr_depth);
		break;
	}
	case MPI_EVENT_SAS_SES:
		ds = "SAS SES Event";
		break;
	case MPI_EVENT_PERSISTENT_TABLE_FULL:
		ds = "Persistent Table Full";
		break;
	case MPI_EVENT_SAS_PHY_LINK_STATUS:
	{
		u8 LinkRates = (u8)(evData0 >> 8);
		u8 PhyNumber = (u8)(evData0);
		LinkRates = (LinkRates & MPI_EVENT_SAS_PLS_LR_CURRENT_MASK) >>
			MPI_EVENT_SAS_PLS_LR_CURRENT_SHIFT;
		switch (LinkRates) {
		case MPI_EVENT_SAS_PLS_LR_RATE_UNKNOWN:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			   "SAS PHY Link Status: Phy=%d:"
			   " Rate Unknown",PhyNumber);
			break;
		case MPI_EVENT_SAS_PLS_LR_RATE_PHY_DISABLED:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			   "SAS PHY Link Status: Phy=%d:"
			   " Phy Disabled",PhyNumber);
			break;
		case MPI_EVENT_SAS_PLS_LR_RATE_FAILED_SPEED_NEGOTIATION:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			   "SAS PHY Link Status: Phy=%d:"
			   " Failed Speed Nego",PhyNumber);
			break;
		case MPI_EVENT_SAS_PLS_LR_RATE_SATA_OOB_COMPLETE:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			   "SAS PHY Link Status: Phy=%d:"
			   " Sata OOB Completed",PhyNumber);
			break;
		case MPI_EVENT_SAS_PLS_LR_RATE_1_5:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			   "SAS PHY Link Status: Phy=%d:"
			   " Rate 1.5 Gbps",PhyNumber);
			break;
		case MPI_EVENT_SAS_PLS_LR_RATE_3_0:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			   "SAS PHY Link Status: Phy=%d:"
			   " Rate 3.0 Gbps", PhyNumber);
			break;
		case MPI_EVENT_SAS_PLS_LR_RATE_6_0:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			   "SAS PHY Link Status: Phy=%d:"
			   " Rate 6.0 Gbps", PhyNumber);
			break;
		default:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			   "SAS PHY Link Status: Phy=%d", PhyNumber);
			break;
		}
		break;
	}
	case MPI_EVENT_SAS_DISCOVERY_ERROR:
		ds = "SAS Discovery Error";
		break;
	case MPI_EVENT_IR_RESYNC_UPDATE:
	{
		u8 resync_complete = (u8)(evData0 >> 16);
		snprintf(evStr, EVENT_DESCR_STR_SZ,
		    "IR Resync Update: Complete = %d:",resync_complete);
		break;
	}
	case MPI_EVENT_IR2:
	{
		u8 id = (u8)(evData0);
		u8 channel = (u8)(evData0 >> 8);
		u8 phys_num = (u8)(evData0 >> 24);
		u8 ReasonCode = (u8)(evData0 >> 16);

		switch (ReasonCode) {
		case MPI_EVENT_IR2_RC_LD_STATE_CHANGED:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "IR2: LD State Changed: "
			    "id=%d channel=%d phys_num=%d",
			    id, channel, phys_num);
			break;
		case MPI_EVENT_IR2_RC_PD_STATE_CHANGED:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "IR2: PD State Changed "
			    "id=%d channel=%d phys_num=%d",
			    id, channel, phys_num);
			break;
		case MPI_EVENT_IR2_RC_BAD_BLOCK_TABLE_FULL:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "IR2: Bad Block Table Full: "
			    "id=%d channel=%d phys_num=%d",
			    id, channel, phys_num);
			break;
		case MPI_EVENT_IR2_RC_PD_INSERTED:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "IR2: PD Inserted: "
			    "id=%d channel=%d phys_num=%d",
			    id, channel, phys_num);
			break;
		case MPI_EVENT_IR2_RC_PD_REMOVED:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "IR2: PD Removed: "
			    "id=%d channel=%d phys_num=%d",
			    id, channel, phys_num);
			break;
		case MPI_EVENT_IR2_RC_FOREIGN_CFG_DETECTED:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "IR2: Foreign CFG Detected: "
			    "id=%d channel=%d phys_num=%d",
			    id, channel, phys_num);
			break;
		case MPI_EVENT_IR2_RC_REBUILD_MEDIUM_ERROR:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "IR2: Rebuild Medium Error: "
			    "id=%d channel=%d phys_num=%d",
			    id, channel, phys_num);
			break;
		case MPI_EVENT_IR2_RC_DUAL_PORT_ADDED:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "IR2: Dual Port Added: "
			    "id=%d channel=%d phys_num=%d",
			    id, channel, phys_num);
			break;
		case MPI_EVENT_IR2_RC_DUAL_PORT_REMOVED:
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "IR2: Dual Port Removed: "
			    "id=%d channel=%d phys_num=%d",
			    id, channel, phys_num);
			break;
		default:
			ds = "IR2";
		break;
		}
		break;
	}
	case MPI_EVENT_SAS_DISCOVERY:
	{
		if (evData0)
			ds = "SAS Discovery: Start";
		else
			ds = "SAS Discovery: Stop";
		break;
	}
	case MPI_EVENT_LOG_ENTRY_ADDED:
		ds = "SAS Log Entry Added";
		break;

	case MPI_EVENT_SAS_BROADCAST_PRIMITIVE:
	{
		u8 phy_num = (u8)(evData0);
		u8 port_num = (u8)(evData0 >> 8);
		u8 port_width = (u8)(evData0 >> 16);
		u8 primative = (u8)(evData0 >> 24);
		snprintf(evStr, EVENT_DESCR_STR_SZ,
		    "SAS Broadcase Primative: phy=%d port=%d "
		    "width=%d primative=0x%02x",
		    phy_num, port_num, port_width, primative);
		break;
	}

	case MPI_EVENT_SAS_INIT_DEVICE_STATUS_CHANGE:
	{
		u8 reason = (u8)(evData0);

		switch (reason) {
		case MPI_EVENT_SAS_INIT_RC_ADDED:
			ds = "SAS Initiator Status Change: Added";
			break;
		case MPI_EVENT_SAS_INIT_RC_REMOVED:
			ds = "SAS Initiator Status Change: Deleted";
			break;
		default:
			ds = "SAS Initiator Status Change";
			break;
		}
		break;
	}

	case MPI_EVENT_SAS_INIT_TABLE_OVERFLOW:
	{
		u8 max_init = (u8)(evData0);
		u8 current_init = (u8)(evData0 >> 8);

		snprintf(evStr, EVENT_DESCR_STR_SZ,
		    "SAS Initiator Device Table Overflow: max initiators=%02d "
		    "current initators=%02d",
		    max_init, current_init);
		break;
	}
	case MPI_EVENT_SAS_SMP_ERROR:
	{
		u8 status = (u8)(evData0);
		u8 port_num = (u8)(evData0 >> 8);
		u8 result = (u8)(evData0 >> 16);

		if (status == MPI_EVENT_SAS_SMP_FUNCTION_RESULT_VALID)
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS SMP Error: port=%d result=0x%02x",
			    port_num, result);
		else if (status == MPI_EVENT_SAS_SMP_CRC_ERROR)
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS SMP Error: port=%d : CRC Error",
			    port_num);
		else if (status == MPI_EVENT_SAS_SMP_TIMEOUT)
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS SMP Error: port=%d : Timeout",
			    port_num);
		else if (status == MPI_EVENT_SAS_SMP_NO_DESTINATION)
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS SMP Error: port=%d : No Destination",
			    port_num);
		else if (status == MPI_EVENT_SAS_SMP_BAD_DESTINATION)
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS SMP Error: port=%d : Bad Destination",
			    port_num);
		else
			snprintf(evStr, EVENT_DESCR_STR_SZ,
			    "SAS SMP Error: port=%d : status=0x%02x",
			    port_num, status);
		break;
	}

	case MPI_EVENT_SAS_EXPANDER_STATUS_CHANGE:
	{
		u8 reason = (u8)(evData0);

		switch (reason) {
		case MPI_EVENT_SAS_EXP_RC_ADDED:
			ds = "Expander Status Change: Added";
			break;
		case MPI_EVENT_SAS_EXP_RC_NOT_RESPONDING:
			ds = "Expander Status Change: Deleted";
			break;
		default:
			ds = "Expander Status Change";
			break;
		}
		break;
	}

	default:
		ds = "Unknown";
		break;
	}
	if (ds)
		strncpy(evStr, ds, EVENT_DESCR_STR_SZ);


	devtprintk(ioc, printk(MYIOC_s_DEBUG_FMT
	    "MPT event:(%02Xh) : %s\n",
	    ioc->name, event, evStr));

	devtverboseprintk(ioc, printk(KERN_DEBUG MYNAM
	    ": Event data:\n"));
	for (ii = 0; ii < le16_to_cpu(pEventReply->EventDataLength); ii++)
		devtverboseprintk(ioc, printk(" %08x",
		    le32_to_cpu(pEventReply->Data[ii])));
	devtverboseprintk(ioc, printk(KERN_DEBUG "\n"));
}
#endif
static int
ProcessEventNotification(MPT_ADAPTER *ioc, EventNotificationReply_t *pEventReply, int *evHandlers)
{
	u16 evDataLen;
	u32 evData0 = 0;
	int ii;
	u8 cb_idx;
	int r = 0;
	int handlers = 0;
	u8 event;

	event = le32_to_cpu(pEventReply->Event) & 0xFF;
	evDataLen = le16_to_cpu(pEventReply->EventDataLength);
	if (evDataLen) {
		evData0 = le32_to_cpu(pEventReply->Data[0]);
	}

#ifdef CONFIG_FUSION_LOGGING
	if (evDataLen)
		mpt_display_event_info(ioc, pEventReply);
#endif

	switch(event) {
	case MPI_EVENT_EVENT_CHANGE:		
		if (evDataLen) {
			u8 evState = evData0 & 0xFF;

			

			
			if (ioc->facts.Function) {
				ioc->facts.EventState = evState;
			}
		}
		break;
	case MPI_EVENT_INTEGRATED_RAID:
		mptbase_raid_process_event_data(ioc,
		    (MpiEventDataRaid_t *)pEventReply->Data);
		break;
	default:
		break;
	}

	/*
	 * Should this event be logged? Events are written sequentially.
	 * When buffer is full, start again at the top.
	 */
	if (ioc->events && (ioc->eventTypes & ( 1 << event))) {
		int idx;

		idx = ioc->eventContext % MPTCTL_EVENT_LOG_SIZE;

		ioc->events[idx].event = event;
		ioc->events[idx].eventContext = ioc->eventContext;

		for (ii = 0; ii < 2; ii++) {
			if (ii < evDataLen)
				ioc->events[idx].data[ii] = le32_to_cpu(pEventReply->Data[ii]);
			else
				ioc->events[idx].data[ii] =  0;
		}

		ioc->eventContext++;
	}


	for (cb_idx = MPT_MAX_PROTOCOL_DRIVERS-1; cb_idx; cb_idx--) {
		if (MptEvHandlers[cb_idx]) {
			devtverboseprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			    "Routing Event to event handler #%d\n",
			    ioc->name, cb_idx));
			r += (*(MptEvHandlers[cb_idx]))(ioc, pEventReply);
			handlers++;
		}
	}
	

	if (pEventReply->AckRequired == MPI_EVENT_NOTIFICATION_ACK_REQUIRED) {
		devtverboseprintk(ioc, printk(MYIOC_s_DEBUG_FMT
			"EventAck required\n",ioc->name));
		if ((ii = SendEventAck(ioc, pEventReply)) != 0) {
			devtverboseprintk(ioc, printk(MYIOC_s_DEBUG_FMT "SendEventAck returned %d\n",
					ioc->name, ii));
		}
	}

	*evHandlers = handlers;
	return r;
}

static void
mpt_fc_log_info(MPT_ADAPTER *ioc, u32 log_info)
{
	char *desc = "unknown";

	switch (log_info & 0xFF000000) {
	case MPI_IOCLOGINFO_FC_INIT_BASE:
		desc = "FCP Initiator";
		break;
	case MPI_IOCLOGINFO_FC_TARGET_BASE:
		desc = "FCP Target";
		break;
	case MPI_IOCLOGINFO_FC_LAN_BASE:
		desc = "LAN";
		break;
	case MPI_IOCLOGINFO_FC_MSG_BASE:
		desc = "MPI Message Layer";
		break;
	case MPI_IOCLOGINFO_FC_LINK_BASE:
		desc = "FC Link";
		break;
	case MPI_IOCLOGINFO_FC_CTX_BASE:
		desc = "Context Manager";
		break;
	case MPI_IOCLOGINFO_FC_INVALID_FIELD_BYTE_OFFSET:
		desc = "Invalid Field Offset";
		break;
	case MPI_IOCLOGINFO_FC_STATE_CHANGE:
		desc = "State Change Info";
		break;
	}

	printk(MYIOC_s_INFO_FMT "LogInfo(0x%08x): SubClass={%s}, Value=(0x%06x)\n",
			ioc->name, log_info, desc, (log_info & 0xFFFFFF));
}

static void
mpt_spi_log_info(MPT_ADAPTER *ioc, u32 log_info)
{
	u32 info = log_info & 0x00FF0000;
	char *desc = "unknown";

	switch (info) {
	case 0x00010000:
		desc = "bug! MID not found";
		break;

	case 0x00020000:
		desc = "Parity Error";
		break;

	case 0x00030000:
		desc = "ASYNC Outbound Overrun";
		break;

	case 0x00040000:
		desc = "SYNC Offset Error";
		break;

	case 0x00050000:
		desc = "BM Change";
		break;

	case 0x00060000:
		desc = "Msg In Overflow";
		break;

	case 0x00070000:
		desc = "DMA Error";
		break;

	case 0x00080000:
		desc = "Outbound DMA Overrun";
		break;

	case 0x00090000:
		desc = "Task Management";
		break;

	case 0x000A0000:
		desc = "Device Problem";
		break;

	case 0x000B0000:
		desc = "Invalid Phase Change";
		break;

	case 0x000C0000:
		desc = "Untagged Table Size";
		break;

	}

	printk(MYIOC_s_INFO_FMT "LogInfo(0x%08x): F/W: %s\n", ioc->name, log_info, desc);
}

	static char *originator_str[] = {
		"IOP",						
		"PL",						
		"IR"						
	};
	static char *iop_code_str[] = {
		NULL,						
		"Invalid SAS Address",				
		NULL,						
		"Invalid Page",					
		"Diag Message Error",				
		"Task Terminated",				
		"Enclosure Management",				
		"Target Mode"					
	};
	static char *pl_code_str[] = {
		NULL,						
		"Open Failure",					
		"Invalid Scatter Gather List",			
		"Wrong Relative Offset or Frame Length",	
		"Frame Transfer Error",				
		"Transmit Frame Connected Low",			
		"SATA Non-NCQ RW Error Bit Set",		
		"SATA Read Log Receive Data Error",		
		"SATA NCQ Fail All Commands After Error",	
		"SATA Error in Receive Set Device Bit FIS",	
		"Receive Frame Invalid Message",		
		"Receive Context Message Valid Error",		
		"Receive Frame Current Frame Error",		
		"SATA Link Down",				
		"Discovery SATA Init W IOS",			
		"Config Invalid Page",				
		"Discovery SATA Init Timeout",			
		"Reset",					
		"Abort",					
		"IO Not Yet Executed",				
		"IO Executed",					
		"Persistent Reservation Out Not Affiliation "
		    "Owner", 					
		"Open Transmit DMA Abort",			
		"IO Device Missing Delay Retry",		
		"IO Cancelled Due to Receive Error",		
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		"Enclosure Management"				
	};
	static char *ir_code_str[] = {
		"Raid Action Error",				
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL						
	};
	static char *raid_sub_code_str[] = {
		NULL, 						
		"Volume Creation Failed: Data Passed too "
		    "Large", 					
		"Volume Creation Failed: Duplicate Volumes "
		    "Attempted", 				
		"Volume Creation Failed: Max Number "
		    "Supported Volumes Exceeded",		
		"Volume Creation Failed: DMA Error",		
		"Volume Creation Failed: Invalid Volume Type",	
		"Volume Creation Failed: Error Reading "
		    "MFG Page 4", 				
		"Volume Creation Failed: Creating Internal "
		    "Structures", 				
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		"Activation failed: Already Active Volume", 	
		"Activation failed: Unsupported Volume Type", 	
		"Activation failed: Too Many Active Volumes", 	
		"Activation failed: Volume ID in Use", 		
		"Activation failed: Reported Failure", 		
		"Activation failed: Importing a Volume", 	
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		"Phys Disk failed: Too Many Phys Disks", 	
		"Phys Disk failed: Data Passed too Large",	
		"Phys Disk failed: DMA Error", 			
		"Phys Disk failed: Invalid <channel:id>", 	
		"Phys Disk failed: Creating Phys Disk Config "
		    "Page", 					
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		NULL,						
		"Compatibility Error: IR Disabled",		
		"Compatibility Error: Inquiry Command Failed",	
		"Compatibility Error: Device not Direct Access "
		    "Device ",					
		"Compatibility Error: Removable Device Found",	
		"Compatibility Error: Device SCSI Version not "
		    "2 or Higher", 				
		"Compatibility Error: SATA Device, 48 BIT LBA "
		    "not Supported", 				
		"Compatibility Error: Device doesn't have "
		    "512 Byte Block Sizes", 			
		"Compatibility Error: Volume Type Check Failed", 
		"Compatibility Error: Volume Type is "
		    "Unsupported by FW", 			
		"Compatibility Error: Disk Drive too Small for "
		    "use in Volume", 				
		"Compatibility Error: Phys Disk for Create "
		    "Volume not Found", 			
		"Compatibility Error: Too Many or too Few "
		    "Disks for Volume Type", 			
		"Compatibility Error: Disk stripe Sizes "
		    "Must be 64KB", 				
		"Compatibility Error: IME Size Limited to < 2TB", 
	};

static void
mpt_sas_log_info(MPT_ADAPTER *ioc, u32 log_info, u8 cb_idx)
{
union loginfo_type {
	u32	loginfo;
	struct {
		u32	subcode:16;
		u32	code:8;
		u32	originator:4;
		u32	bus_type:4;
	}dw;
};
	union loginfo_type sas_loginfo;
	char *originator_desc = NULL;
	char *code_desc = NULL;
	char *sub_code_desc = NULL;

	sas_loginfo.loginfo = log_info;
	if ((sas_loginfo.dw.bus_type != 3 ) &&
	    (sas_loginfo.dw.originator < ARRAY_SIZE(originator_str)))
		return;

	originator_desc = originator_str[sas_loginfo.dw.originator];

	switch (sas_loginfo.dw.originator) {

		case 0:  
			if (sas_loginfo.dw.code <
			    ARRAY_SIZE(iop_code_str))
				code_desc = iop_code_str[sas_loginfo.dw.code];
			break;
		case 1:  
			if (sas_loginfo.dw.code <
			    ARRAY_SIZE(pl_code_str))
				code_desc = pl_code_str[sas_loginfo.dw.code];
			break;
		case 2:  
			if (sas_loginfo.dw.code >=
			    ARRAY_SIZE(ir_code_str))
				break;
			code_desc = ir_code_str[sas_loginfo.dw.code];
			if (sas_loginfo.dw.subcode >=
			    ARRAY_SIZE(raid_sub_code_str))
				break;
			if (sas_loginfo.dw.code == 0)
				sub_code_desc =
				    raid_sub_code_str[sas_loginfo.dw.subcode];
			break;
		default:
			return;
	}

	if (sub_code_desc != NULL)
		printk(MYIOC_s_INFO_FMT
			"LogInfo(0x%08x): Originator={%s}, Code={%s},"
			" SubCode={%s} cb_idx %s\n",
			ioc->name, log_info, originator_desc, code_desc,
			sub_code_desc, MptCallbacksName[cb_idx]);
	else if (code_desc != NULL)
		printk(MYIOC_s_INFO_FMT
			"LogInfo(0x%08x): Originator={%s}, Code={%s},"
			" SubCode(0x%04x) cb_idx %s\n",
			ioc->name, log_info, originator_desc, code_desc,
			sas_loginfo.dw.subcode, MptCallbacksName[cb_idx]);
	else
		printk(MYIOC_s_INFO_FMT
			"LogInfo(0x%08x): Originator={%s}, Code=(0x%02x),"
			" SubCode(0x%04x) cb_idx %s\n",
			ioc->name, log_info, originator_desc,
			sas_loginfo.dw.code, sas_loginfo.dw.subcode,
			MptCallbacksName[cb_idx]);
}

static void
mpt_iocstatus_info_config(MPT_ADAPTER *ioc, u32 ioc_status, MPT_FRAME_HDR *mf)
{
	Config_t *pReq = (Config_t *)mf;
	char extend_desc[EVENT_DESCR_STR_SZ];
	char *desc = NULL;
	u32 form;
	u8 page_type;

	if (pReq->Header.PageType == MPI_CONFIG_PAGETYPE_EXTENDED)
		page_type = pReq->ExtPageType;
	else
		page_type = pReq->Header.PageType;

	form = le32_to_cpu(pReq->PageAddress);
	if (ioc_status == MPI_IOCSTATUS_CONFIG_INVALID_PAGE) {
		if (page_type == MPI_CONFIG_EXTPAGETYPE_SAS_DEVICE ||
		    page_type == MPI_CONFIG_EXTPAGETYPE_SAS_EXPANDER ||
		    page_type == MPI_CONFIG_EXTPAGETYPE_ENCLOSURE) {
			if ((form >> MPI_SAS_DEVICE_PGAD_FORM_SHIFT) ==
				MPI_SAS_DEVICE_PGAD_FORM_GET_NEXT_HANDLE)
				return;
		}
		if (page_type == MPI_CONFIG_PAGETYPE_FC_DEVICE)
			if ((form & MPI_FC_DEVICE_PGAD_FORM_MASK) ==
				MPI_FC_DEVICE_PGAD_FORM_NEXT_DID)
				return;
	}

	snprintf(extend_desc, EVENT_DESCR_STR_SZ,
	    "type=%02Xh, page=%02Xh, action=%02Xh, form=%08Xh",
	    page_type, pReq->Header.PageNumber, pReq->Action, form);

	switch (ioc_status) {

	case MPI_IOCSTATUS_CONFIG_INVALID_ACTION: 
		desc = "Config Page Invalid Action";
		break;

	case MPI_IOCSTATUS_CONFIG_INVALID_TYPE:   
		desc = "Config Page Invalid Type";
		break;

	case MPI_IOCSTATUS_CONFIG_INVALID_PAGE:   
		desc = "Config Page Invalid Page";
		break;

	case MPI_IOCSTATUS_CONFIG_INVALID_DATA:   
		desc = "Config Page Invalid Data";
		break;

	case MPI_IOCSTATUS_CONFIG_NO_DEFAULTS:    
		desc = "Config Page No Defaults";
		break;

	case MPI_IOCSTATUS_CONFIG_CANT_COMMIT:    
		desc = "Config Page Can't Commit";
		break;
	}

	if (!desc)
		return;

	dreplyprintk(ioc, printk(MYIOC_s_DEBUG_FMT "IOCStatus(0x%04X): %s: %s\n",
	    ioc->name, ioc_status, desc, extend_desc));
}

static void
mpt_iocstatus_info(MPT_ADAPTER *ioc, u32 ioc_status, MPT_FRAME_HDR *mf)
{
	u32 status = ioc_status & MPI_IOCSTATUS_MASK;
	char *desc = NULL;

	switch (status) {


	case MPI_IOCSTATUS_INVALID_FUNCTION: 
		desc = "Invalid Function";
		break;

	case MPI_IOCSTATUS_BUSY: 
		desc = "Busy";
		break;

	case MPI_IOCSTATUS_INVALID_SGL: 
		desc = "Invalid SGL";
		break;

	case MPI_IOCSTATUS_INTERNAL_ERROR: 
		desc = "Internal Error";
		break;

	case MPI_IOCSTATUS_RESERVED: 
		desc = "Reserved";
		break;

	case MPI_IOCSTATUS_INSUFFICIENT_RESOURCES: 
		desc = "Insufficient Resources";
		break;

	case MPI_IOCSTATUS_INVALID_FIELD: 
		desc = "Invalid Field";
		break;

	case MPI_IOCSTATUS_INVALID_STATE: 
		desc = "Invalid State";
		break;


	case MPI_IOCSTATUS_CONFIG_INVALID_ACTION: 
	case MPI_IOCSTATUS_CONFIG_INVALID_TYPE:   
	case MPI_IOCSTATUS_CONFIG_INVALID_PAGE:   
	case MPI_IOCSTATUS_CONFIG_INVALID_DATA:   
	case MPI_IOCSTATUS_CONFIG_NO_DEFAULTS:    
	case MPI_IOCSTATUS_CONFIG_CANT_COMMIT:    
		mpt_iocstatus_info_config(ioc, status, mf);
		break;


	case MPI_IOCSTATUS_SCSI_RECOVERED_ERROR: 
	case MPI_IOCSTATUS_SCSI_DATA_UNDERRUN: 
	case MPI_IOCSTATUS_SCSI_INVALID_BUS: 
	case MPI_IOCSTATUS_SCSI_INVALID_TARGETID: 
	case MPI_IOCSTATUS_SCSI_DEVICE_NOT_THERE: 
	case MPI_IOCSTATUS_SCSI_DATA_OVERRUN: 
	case MPI_IOCSTATUS_SCSI_IO_DATA_ERROR: 
	case MPI_IOCSTATUS_SCSI_PROTOCOL_ERROR: 
	case MPI_IOCSTATUS_SCSI_TASK_TERMINATED: 
	case MPI_IOCSTATUS_SCSI_RESIDUAL_MISMATCH: 
	case MPI_IOCSTATUS_SCSI_TASK_MGMT_FAILED: 
	case MPI_IOCSTATUS_SCSI_IOC_TERMINATED: 
	case MPI_IOCSTATUS_SCSI_EXT_TERMINATED: 
		break;


	case MPI_IOCSTATUS_TARGET_PRIORITY_IO: 
		desc = "Target: Priority IO";
		break;

	case MPI_IOCSTATUS_TARGET_INVALID_PORT: 
		desc = "Target: Invalid Port";
		break;

	case MPI_IOCSTATUS_TARGET_INVALID_IO_INDEX: 
		desc = "Target Invalid IO Index:";
		break;

	case MPI_IOCSTATUS_TARGET_ABORTED: 
		desc = "Target: Aborted";
		break;

	case MPI_IOCSTATUS_TARGET_NO_CONN_RETRYABLE: 
		desc = "Target: No Conn Retryable";
		break;

	case MPI_IOCSTATUS_TARGET_NO_CONNECTION: 
		desc = "Target: No Connection";
		break;

	case MPI_IOCSTATUS_TARGET_XFER_COUNT_MISMATCH: 
		desc = "Target: Transfer Count Mismatch";
		break;

	case MPI_IOCSTATUS_TARGET_STS_DATA_NOT_SENT: 
		desc = "Target: STS Data not Sent";
		break;

	case MPI_IOCSTATUS_TARGET_DATA_OFFSET_ERROR: 
		desc = "Target: Data Offset Error";
		break;

	case MPI_IOCSTATUS_TARGET_TOO_MUCH_WRITE_DATA: 
		desc = "Target: Too Much Write Data";
		break;

	case MPI_IOCSTATUS_TARGET_IU_TOO_SHORT: 
		desc = "Target: IU Too Short";
		break;

	case MPI_IOCSTATUS_TARGET_ACK_NAK_TIMEOUT: 
		desc = "Target: ACK NAK Timeout";
		break;

	case MPI_IOCSTATUS_TARGET_NAK_RECEIVED: 
		desc = "Target: Nak Received";
		break;


	case MPI_IOCSTATUS_FC_ABORTED: 
		desc = "FC: Aborted";
		break;

	case MPI_IOCSTATUS_FC_RX_ID_INVALID: 
		desc = "FC: RX ID Invalid";
		break;

	case MPI_IOCSTATUS_FC_DID_INVALID: 
		desc = "FC: DID Invalid";
		break;

	case MPI_IOCSTATUS_FC_NODE_LOGGED_OUT: 
		desc = "FC: Node Logged Out";
		break;

	case MPI_IOCSTATUS_FC_EXCHANGE_CANCELED: 
		desc = "FC: Exchange Canceled";
		break;


	case MPI_IOCSTATUS_LAN_DEVICE_NOT_FOUND: 
		desc = "LAN: Device not Found";
		break;

	case MPI_IOCSTATUS_LAN_DEVICE_FAILURE: 
		desc = "LAN: Device Failure";
		break;

	case MPI_IOCSTATUS_LAN_TRANSMIT_ERROR: 
		desc = "LAN: Transmit Error";
		break;

	case MPI_IOCSTATUS_LAN_TRANSMIT_ABORTED: 
		desc = "LAN: Transmit Aborted";
		break;

	case MPI_IOCSTATUS_LAN_RECEIVE_ERROR: 
		desc = "LAN: Receive Error";
		break;

	case MPI_IOCSTATUS_LAN_RECEIVE_ABORTED: 
		desc = "LAN: Receive Aborted";
		break;

	case MPI_IOCSTATUS_LAN_PARTIAL_PACKET: 
		desc = "LAN: Partial Packet";
		break;

	case MPI_IOCSTATUS_LAN_CANCELED: 
		desc = "LAN: Canceled";
		break;


	case MPI_IOCSTATUS_SAS_SMP_REQUEST_FAILED: 
		desc = "SAS: SMP Request Failed";
		break;

	case MPI_IOCSTATUS_SAS_SMP_DATA_OVERRUN: 
		desc = "SAS: SMP Data Overrun";
		break;

	default:
		desc = "Others";
		break;
	}

	if (!desc)
		return;

	dreplyprintk(ioc, printk(MYIOC_s_DEBUG_FMT "IOCStatus(0x%04X): %s\n",
	    ioc->name, status, desc));
}

EXPORT_SYMBOL(mpt_attach);
EXPORT_SYMBOL(mpt_detach);
#ifdef CONFIG_PM
EXPORT_SYMBOL(mpt_resume);
EXPORT_SYMBOL(mpt_suspend);
#endif
EXPORT_SYMBOL(ioc_list);
EXPORT_SYMBOL(mpt_register);
EXPORT_SYMBOL(mpt_deregister);
EXPORT_SYMBOL(mpt_event_register);
EXPORT_SYMBOL(mpt_event_deregister);
EXPORT_SYMBOL(mpt_reset_register);
EXPORT_SYMBOL(mpt_reset_deregister);
EXPORT_SYMBOL(mpt_device_driver_register);
EXPORT_SYMBOL(mpt_device_driver_deregister);
EXPORT_SYMBOL(mpt_get_msg_frame);
EXPORT_SYMBOL(mpt_put_msg_frame);
EXPORT_SYMBOL(mpt_put_msg_frame_hi_pri);
EXPORT_SYMBOL(mpt_free_msg_frame);
EXPORT_SYMBOL(mpt_send_handshake_request);
EXPORT_SYMBOL(mpt_verify_adapter);
EXPORT_SYMBOL(mpt_GetIocState);
EXPORT_SYMBOL(mpt_print_ioc_summary);
EXPORT_SYMBOL(mpt_HardResetHandler);
EXPORT_SYMBOL(mpt_config);
EXPORT_SYMBOL(mpt_findImVolumes);
EXPORT_SYMBOL(mpt_alloc_fw_memory);
EXPORT_SYMBOL(mpt_free_fw_memory);
EXPORT_SYMBOL(mptbase_sas_persist_operation);
EXPORT_SYMBOL(mpt_raid_phys_disk_pg0);

static int __init
fusion_init(void)
{
	u8 cb_idx;

	show_mptmod_ver(my_NAME, my_VERSION);
	printk(KERN_INFO COPYRIGHT "\n");

	for (cb_idx = 0; cb_idx < MPT_MAX_PROTOCOL_DRIVERS; cb_idx++) {
		MptCallbacks[cb_idx] = NULL;
		MptDriverClass[cb_idx] = MPTUNKNOWN_DRIVER;
		MptEvHandlers[cb_idx] = NULL;
		MptResetHandlers[cb_idx] = NULL;
	}

	mpt_base_index = mpt_register(mptbase_reply, MPTBASE_DRIVER,
	    "mptbase_reply");

	mpt_reset_register(mpt_base_index, mpt_ioc_reset);

#ifdef CONFIG_PROC_FS
	(void) procmpt_create();
#endif
	return 0;
}

static void __exit
fusion_exit(void)
{

	mpt_reset_deregister(mpt_base_index);

#ifdef CONFIG_PROC_FS
	procmpt_destroy();
#endif
}

module_init(fusion_init);
module_exit(fusion_exit);
