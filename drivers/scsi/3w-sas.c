/*
   3w-sas.c -- LSI 3ware SAS/SATA-RAID Controller device driver for Linux.

   Written By: Adam Radford <linuxraid@lsi.com>

   Copyright (C) 2009 LSI Corporation.

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

   Controllers supported by this driver:

   LSI 3ware 9750 6Gb/s SAS/SATA-RAID

   Bugs/Comments/Suggestions should be mailed to:
   linuxraid@lsi.com

   For more information, goto:
   http://www.lsi.com

   History
   -------
   3.26.02.000 - Initial driver release.
*/

#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_cmnd.h>
#include "3w-sas.h"

#define TW_DRIVER_VERSION "3.26.02.000"
static DEFINE_MUTEX(twl_chrdev_mutex);
static TW_Device_Extension *twl_device_extension_list[TW_MAX_SLOT];
static unsigned int twl_device_extension_count;
static int twl_major = -1;
extern struct timezone sys_tz;

MODULE_AUTHOR ("LSI");
MODULE_DESCRIPTION ("LSI 3ware SAS/SATA-RAID Linux Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(TW_DRIVER_VERSION);

static int use_msi;
module_param(use_msi, int, S_IRUGO);
MODULE_PARM_DESC(use_msi, "Use Message Signaled Interrupts. Default: 0");

static int twl_reset_device_extension(TW_Device_Extension *tw_dev, int ioctl_reset);


static ssize_t twl_sysfs_aen_read(struct file *filp, struct kobject *kobj,
				  struct bin_attribute *bin_attr,
				  char *outbuf, loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct Scsi_Host *shost = class_to_shost(dev);
	TW_Device_Extension *tw_dev = (TW_Device_Extension *)shost->hostdata;
	unsigned long flags = 0;
	ssize_t ret;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	spin_lock_irqsave(tw_dev->host->host_lock, flags);
	ret = memory_read_from_buffer(outbuf, count, &offset, tw_dev->event_queue[0], sizeof(TW_Event) * TW_Q_LENGTH);
	spin_unlock_irqrestore(tw_dev->host->host_lock, flags);

	return ret;
} 

static struct bin_attribute twl_sysfs_aen_read_attr = {
	.attr = {
		.name = "3ware_aen_read",
		.mode = S_IRUSR,
	}, 
	.size = 0,
	.read = twl_sysfs_aen_read
};

static ssize_t twl_sysfs_compat_info(struct file *filp, struct kobject *kobj,
				     struct bin_attribute *bin_attr,
				     char *outbuf, loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct Scsi_Host *shost = class_to_shost(dev);
	TW_Device_Extension *tw_dev = (TW_Device_Extension *)shost->hostdata;
	unsigned long flags = 0;
	ssize_t ret;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	spin_lock_irqsave(tw_dev->host->host_lock, flags);
	ret = memory_read_from_buffer(outbuf, count, &offset, &tw_dev->tw_compat_info, sizeof(TW_Compatibility_Info));
	spin_unlock_irqrestore(tw_dev->host->host_lock, flags);

	return ret;
} 

static struct bin_attribute twl_sysfs_compat_info_attr = {
	.attr = {
		.name = "3ware_compat_info",
		.mode = S_IRUSR,
	}, 
	.size = 0,
	.read = twl_sysfs_compat_info
};

static ssize_t twl_show_stats(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *host = class_to_shost(dev);
	TW_Device_Extension *tw_dev = (TW_Device_Extension *)host->hostdata;
	unsigned long flags = 0;
	ssize_t len;

	spin_lock_irqsave(tw_dev->host->host_lock, flags);
	len = snprintf(buf, PAGE_SIZE, "3w-sas Driver version: %s\n"
		       "Current commands posted:   %4d\n"
		       "Max commands posted:       %4d\n"
		       "Last sgl length:           %4d\n"
		       "Max sgl length:            %4d\n"
		       "Last sector count:         %4d\n"
		       "Max sector count:          %4d\n"
		       "SCSI Host Resets:          %4d\n"
		       "AEN's:                     %4d\n", 
		       TW_DRIVER_VERSION,
		       tw_dev->posted_request_count,
		       tw_dev->max_posted_request_count,
		       tw_dev->sgl_entries,
		       tw_dev->max_sgl_entries,
		       tw_dev->sector_count,
		       tw_dev->max_sector_count,
		       tw_dev->num_resets,
		       tw_dev->aen_count);
	spin_unlock_irqrestore(tw_dev->host->host_lock, flags);
	return len;
} 

static int twl_change_queue_depth(struct scsi_device *sdev, int queue_depth,
				  int reason)
{
	if (reason != SCSI_QDEPTH_DEFAULT)
		return -EOPNOTSUPP;

	if (queue_depth > TW_Q_LENGTH-2)
		queue_depth = TW_Q_LENGTH-2;
	scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, queue_depth);
	return queue_depth;
} 

static struct device_attribute twl_host_stats_attr = {
	.attr = {
		.name = 	"3ware_stats",
		.mode =		S_IRUGO,
	},
	.show = twl_show_stats
};

static struct device_attribute *twl_host_attrs[] = {
	&twl_host_stats_attr,
	NULL,
};

static char *twl_aen_severity_lookup(unsigned char severity_code)
{
	char *retval = NULL;

	if ((severity_code < (unsigned char) TW_AEN_SEVERITY_ERROR) ||
	    (severity_code > (unsigned char) TW_AEN_SEVERITY_DEBUG))
		goto out;

	retval = twl_aen_severity_table[severity_code];
out:
	return retval;
} 

static void twl_aen_queue_event(TW_Device_Extension *tw_dev, TW_Command_Apache_Header *header)
{
	u32 local_time;
	struct timeval time;
	TW_Event *event;
	unsigned short aen;
	char host[16];
	char *error_str;

	tw_dev->aen_count++;

	
	event = tw_dev->event_queue[tw_dev->error_index];

	host[0] = '\0';
	if (tw_dev->host)
		sprintf(host, " scsi%d:", tw_dev->host->host_no);

	aen = le16_to_cpu(header->status_block.error);
	memset(event, 0, sizeof(TW_Event));

	event->severity = TW_SEV_OUT(header->status_block.severity__reserved);
	do_gettimeofday(&time);
	local_time = (u32)(time.tv_sec - (sys_tz.tz_minuteswest * 60));
	event->time_stamp_sec = local_time;
	event->aen_code = aen;
	event->retrieved = TW_AEN_NOT_RETRIEVED;
	event->sequence_id = tw_dev->error_sequence_id;
	tw_dev->error_sequence_id++;

	
	error_str = &(header->err_specific_desc[strlen(header->err_specific_desc)+1]);

	header->err_specific_desc[sizeof(header->err_specific_desc) - 1] = '\0';
	event->parameter_len = strlen(header->err_specific_desc);
	memcpy(event->parameter_data, header->err_specific_desc, event->parameter_len + 1 + strlen(error_str));
	if (event->severity != TW_AEN_SEVERITY_DEBUG)
		printk(KERN_WARNING "3w-sas:%s AEN: %s (0x%02X:0x%04X): %s:%s.\n",
		       host,
		       twl_aen_severity_lookup(TW_SEV_OUT(header->status_block.severity__reserved)),
		       TW_MESSAGE_SOURCE_CONTROLLER_EVENT, aen, error_str,
		       header->err_specific_desc);
	else
		tw_dev->aen_count--;

	tw_dev->error_index = (tw_dev->error_index + 1 ) % TW_Q_LENGTH;
} 

static int twl_post_command_packet(TW_Device_Extension *tw_dev, int request_id)
{
	dma_addr_t command_que_value;

	command_que_value = tw_dev->command_packet_phys[request_id];
	command_que_value += TW_COMMAND_OFFSET;

	
	writel((u32)((u64)command_que_value >> 32), TWL_HIBQPH_REG_ADDR(tw_dev));
	
	writel((u32)(command_que_value | TWL_PULL_MODE), TWL_HIBQPL_REG_ADDR(tw_dev));

	tw_dev->state[request_id] = TW_S_POSTED;
	tw_dev->posted_request_count++;
	if (tw_dev->posted_request_count > tw_dev->max_posted_request_count)
		tw_dev->max_posted_request_count = tw_dev->posted_request_count;

	return 0;
} 

static int twl_map_scsi_sg_data(TW_Device_Extension *tw_dev, int request_id)
{
	int use_sg;
	struct scsi_cmnd *cmd = tw_dev->srb[request_id];

	use_sg = scsi_dma_map(cmd);
	if (!use_sg)
		return 0;
	else if (use_sg < 0) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x1, "Failed to map scatter gather list");
		return 0;
	}

	cmd->SCp.phase = TW_PHASE_SGLIST;
	cmd->SCp.have_data_in = use_sg;

	return use_sg;
} 

static int twl_scsiop_execute_scsi(TW_Device_Extension *tw_dev, int request_id, char *cdb, int use_sg, TW_SG_Entry_ISO *sglistarg)
{
	TW_Command_Full *full_command_packet;
	TW_Command_Apache *command_packet;
	int i, sg_count;
	struct scsi_cmnd *srb = NULL;
	struct scatterlist *sglist = NULL, *sg;
	int retval = 1;

	if (tw_dev->srb[request_id]) {
		srb = tw_dev->srb[request_id];
		if (scsi_sglist(srb))
			sglist = scsi_sglist(srb);
	}

	
	full_command_packet = tw_dev->command_packet_virt[request_id];
	full_command_packet->header.header_desc.size_header = 128;
	full_command_packet->header.status_block.error = 0;
	full_command_packet->header.status_block.severity__reserved = 0;

	command_packet = &full_command_packet->command.newcommand;
	command_packet->status = 0;
	command_packet->opcode__reserved = TW_OPRES_IN(0, TW_OP_EXECUTE_SCSI);

	
	if (!cdb)
		memcpy(command_packet->cdb, srb->cmnd, TW_MAX_CDB_LEN);
	else
		memcpy(command_packet->cdb, cdb, TW_MAX_CDB_LEN);

	if (srb) {
		command_packet->unit = srb->device->id;
		command_packet->request_id__lunl =
			cpu_to_le16(TW_REQ_LUN_IN(srb->device->lun, request_id));
	} else {
		command_packet->request_id__lunl =
			cpu_to_le16(TW_REQ_LUN_IN(0, request_id));
		command_packet->unit = 0;
	}

	command_packet->sgl_offset = 16;

	if (!sglistarg) {
		
		if (scsi_sg_count(srb)) {
			sg_count = twl_map_scsi_sg_data(tw_dev, request_id);
			if (sg_count == 0)
				goto out;

			scsi_for_each_sg(srb, sg, sg_count, i) {
				command_packet->sg_list[i].address = TW_CPU_TO_SGL(sg_dma_address(sg));
				command_packet->sg_list[i].length = TW_CPU_TO_SGL(sg_dma_len(sg));
			}
			command_packet->sgl_entries__lunh = cpu_to_le16(TW_REQ_LUN_IN((srb->device->lun >> 4), scsi_sg_count(tw_dev->srb[request_id])));
		}
	} else {
		
		for (i = 0; i < use_sg; i++) {
			command_packet->sg_list[i].address = TW_CPU_TO_SGL(sglistarg[i].address);
			command_packet->sg_list[i].length = TW_CPU_TO_SGL(sglistarg[i].length);
		}
		command_packet->sgl_entries__lunh = cpu_to_le16(TW_REQ_LUN_IN(0, use_sg));
	}

	
	if (srb) {
		tw_dev->sector_count = scsi_bufflen(srb) / 512;
		if (tw_dev->sector_count > tw_dev->max_sector_count)
			tw_dev->max_sector_count = tw_dev->sector_count;
		tw_dev->sgl_entries = scsi_sg_count(srb);
		if (tw_dev->sgl_entries > tw_dev->max_sgl_entries)
			tw_dev->max_sgl_entries = tw_dev->sgl_entries;
	}

	
	retval = twl_post_command_packet(tw_dev, request_id);

out:
	return retval;
} 

static int twl_aen_read_queue(TW_Device_Extension *tw_dev, int request_id)
{
	char cdb[TW_MAX_CDB_LEN];
	TW_SG_Entry_ISO sglist[1];
	TW_Command_Full *full_command_packet;
	int retval = 1;

	full_command_packet = tw_dev->command_packet_virt[request_id];
	memset(full_command_packet, 0, sizeof(TW_Command_Full));

	
	memset(&cdb, 0, TW_MAX_CDB_LEN);
	cdb[0] = REQUEST_SENSE; 
	cdb[4] = TW_ALLOCATION_LENGTH; 

	
	memset(&sglist, 0, sizeof(TW_SG_Entry_ISO));
	sglist[0].length = TW_SECTOR_SIZE;
	sglist[0].address = tw_dev->generic_buffer_phys[request_id];

	
	tw_dev->srb[request_id] = NULL;

	
	if (twl_scsiop_execute_scsi(tw_dev, request_id, cdb, 1, sglist)) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x2, "Post failed while reading AEN queue");
		goto out;
	}
	retval = 0;
out:
	return retval;
} 

static void twl_aen_sync_time(TW_Device_Extension *tw_dev, int request_id)
{
	u32 schedulertime;
	struct timeval utc;
	TW_Command_Full *full_command_packet;
	TW_Command *command_packet;
	TW_Param_Apache *param;
	u32 local_time;

	
	full_command_packet = tw_dev->command_packet_virt[request_id];
	memset(full_command_packet, 0, sizeof(TW_Command_Full));
	command_packet = &full_command_packet->command.oldcommand;
	command_packet->opcode__sgloffset = TW_OPSGL_IN(2, TW_OP_SET_PARAM);
	command_packet->request_id = request_id;
	command_packet->byte8_offset.param.sgl[0].address = TW_CPU_TO_SGL(tw_dev->generic_buffer_phys[request_id]);
	command_packet->byte8_offset.param.sgl[0].length = TW_CPU_TO_SGL(TW_SECTOR_SIZE);
	command_packet->size = TW_COMMAND_SIZE;
	command_packet->byte6_offset.parameter_count = cpu_to_le16(1);

	
	param = (TW_Param_Apache *)tw_dev->generic_buffer_virt[request_id];
	memset(param, 0, TW_SECTOR_SIZE);
	param->table_id = cpu_to_le16(TW_TIMEKEEP_TABLE | 0x8000); 
	param->parameter_id = cpu_to_le16(0x3); 
	param->parameter_size_bytes = cpu_to_le16(4);

	do_gettimeofday(&utc);
	local_time = (u32)(utc.tv_sec - (sys_tz.tz_minuteswest * 60));
	schedulertime = local_time - (3 * 86400);
	schedulertime = cpu_to_le32(schedulertime % 604800);

	memcpy(param->data, &schedulertime, sizeof(u32));

	
	tw_dev->srb[request_id] = NULL;

	
	twl_post_command_packet(tw_dev, request_id);
} 

static void twl_get_request_id(TW_Device_Extension *tw_dev, int *request_id)
{
	*request_id = tw_dev->free_queue[tw_dev->free_head];
	tw_dev->free_head = (tw_dev->free_head + 1) % TW_Q_LENGTH;
	tw_dev->state[*request_id] = TW_S_STARTED;
} 

static void twl_free_request_id(TW_Device_Extension *tw_dev, int request_id)
{
	tw_dev->free_queue[tw_dev->free_tail] = request_id;
	tw_dev->state[request_id] = TW_S_FINISHED;
	tw_dev->free_tail = (tw_dev->free_tail + 1) % TW_Q_LENGTH;
} 

static int twl_aen_complete(TW_Device_Extension *tw_dev, int request_id)
{
	TW_Command_Full *full_command_packet;
	TW_Command *command_packet;
	TW_Command_Apache_Header *header;
	unsigned short aen;
	int retval = 1;

	header = (TW_Command_Apache_Header *)tw_dev->generic_buffer_virt[request_id];
	tw_dev->posted_request_count--;
	aen = le16_to_cpu(header->status_block.error);
	full_command_packet = tw_dev->command_packet_virt[request_id];
	command_packet = &full_command_packet->command.oldcommand;

	
	if (TW_OP_OUT(command_packet->opcode__sgloffset) == TW_OP_SET_PARAM) {
		
		if (twl_aen_read_queue(tw_dev, request_id))
			goto out2;
	        else {
			retval = 0;
			goto out;
		}
	}

	switch (aen) {
	case TW_AEN_QUEUE_EMPTY:
		
		break;
	case TW_AEN_SYNC_TIME_WITH_HOST:
		twl_aen_sync_time(tw_dev, request_id);
		retval = 0;
		goto out;
	default:
		twl_aen_queue_event(tw_dev, header);

		
		if (twl_aen_read_queue(tw_dev, request_id))
			goto out2;
		else {
			retval = 0;
			goto out;
		}
	}
	retval = 0;
out2:
	tw_dev->state[request_id] = TW_S_COMPLETED;
	twl_free_request_id(tw_dev, request_id);
	clear_bit(TW_IN_ATTENTION_LOOP, &tw_dev->flags);
out:
	return retval;
} 

static int twl_poll_response(TW_Device_Extension *tw_dev, int request_id, int seconds)
{
	unsigned long before;
	dma_addr_t mfa;
	u32 regh, regl;
	u32 response;
	int retval = 1;
	int found = 0;

	before = jiffies;

	while (!found) {
		if (sizeof(dma_addr_t) > 4) {
			regh = readl(TWL_HOBQPH_REG_ADDR(tw_dev));
			regl = readl(TWL_HOBQPL_REG_ADDR(tw_dev));
			mfa = ((u64)regh << 32) | regl;
		} else
			mfa = readl(TWL_HOBQPL_REG_ADDR(tw_dev));

		response = (u32)mfa;

		if (TW_RESID_OUT(response) == request_id)
			found = 1;

		if (time_after(jiffies, before + HZ * seconds))
			goto out;

		msleep(50);
	}
	retval = 0;
out: 
	return retval;
} 

static int twl_aen_drain_queue(TW_Device_Extension *tw_dev, int no_check_reset)
{
	int request_id = 0;
	char cdb[TW_MAX_CDB_LEN];
	TW_SG_Entry_ISO sglist[1];
	int finished = 0, count = 0;
	TW_Command_Full *full_command_packet;
	TW_Command_Apache_Header *header;
	unsigned short aen;
	int first_reset = 0, queue = 0, retval = 1;

	if (no_check_reset)
		first_reset = 0;
	else
		first_reset = 1;

	full_command_packet = tw_dev->command_packet_virt[request_id];
	memset(full_command_packet, 0, sizeof(TW_Command_Full));

	
	memset(&cdb, 0, TW_MAX_CDB_LEN);
	cdb[0] = REQUEST_SENSE; 
	cdb[4] = TW_ALLOCATION_LENGTH; 

	
	memset(&sglist, 0, sizeof(TW_SG_Entry_ISO));
	sglist[0].length = TW_SECTOR_SIZE;
	sglist[0].address = tw_dev->generic_buffer_phys[request_id];

	
	tw_dev->srb[request_id] = NULL;

	do {
		
		if (twl_scsiop_execute_scsi(tw_dev, request_id, cdb, 1, sglist)) {
			TW_PRINTK(tw_dev->host, TW_DRIVER, 0x3, "Error posting request sense");
			goto out;
		}

		
		if (twl_poll_response(tw_dev, request_id, 30)) {
			TW_PRINTK(tw_dev->host, TW_DRIVER, 0x4, "No valid response while draining AEN queue");
			tw_dev->posted_request_count--;
			goto out;
		}

		tw_dev->posted_request_count--;
		header = (TW_Command_Apache_Header *)tw_dev->generic_buffer_virt[request_id];
		aen = le16_to_cpu(header->status_block.error);
		queue = 0;
		count++;

		switch (aen) {
		case TW_AEN_QUEUE_EMPTY:
			if (first_reset != 1)
				goto out;
			else
				finished = 1;
			break;
		case TW_AEN_SOFT_RESET:
			if (first_reset == 0)
				first_reset = 1;
			else
				queue = 1;
			break;
		case TW_AEN_SYNC_TIME_WITH_HOST:
			break;
		default:
			queue = 1;
		}

		
		if (queue)
			twl_aen_queue_event(tw_dev, header);
	} while ((finished == 0) && (count < TW_MAX_AEN_DRAIN));

	if (count == TW_MAX_AEN_DRAIN)
		goto out;

	retval = 0;
out:
	tw_dev->state[request_id] = TW_S_INITIAL;
	return retval;
} 

static int twl_allocate_memory(TW_Device_Extension *tw_dev, int size, int which)
{
	int i;
	dma_addr_t dma_handle;
	unsigned long *cpu_addr;
	int retval = 1;

	cpu_addr = pci_alloc_consistent(tw_dev->tw_pci_dev, size*TW_Q_LENGTH, &dma_handle);
	if (!cpu_addr) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x5, "Memory allocation failed");
		goto out;
	}

	memset(cpu_addr, 0, size*TW_Q_LENGTH);

	for (i = 0; i < TW_Q_LENGTH; i++) {
		switch(which) {
		case 0:
			tw_dev->command_packet_phys[i] = dma_handle+(i*size);
			tw_dev->command_packet_virt[i] = (TW_Command_Full *)((unsigned char *)cpu_addr + (i*size));
			break;
		case 1:
			tw_dev->generic_buffer_phys[i] = dma_handle+(i*size);
			tw_dev->generic_buffer_virt[i] = (unsigned long *)((unsigned char *)cpu_addr + (i*size));
			break;
		case 2:
			tw_dev->sense_buffer_phys[i] = dma_handle+(i*size);
			tw_dev->sense_buffer_virt[i] = (TW_Command_Apache_Header *)((unsigned char *)cpu_addr + (i*size));
			break;
		}
	}
	retval = 0;
out:
	return retval;
} 

static void twl_load_sgl(TW_Device_Extension *tw_dev, TW_Command_Full *full_command_packet, int request_id, dma_addr_t dma_handle, int length)
{
	TW_Command *oldcommand;
	TW_Command_Apache *newcommand;
	TW_SG_Entry_ISO *sgl;
	unsigned int pae = 0;

	if ((sizeof(long) < 8) && (sizeof(dma_addr_t) > 4))
		pae = 1;

	if (TW_OP_OUT(full_command_packet->command.newcommand.opcode__reserved) == TW_OP_EXECUTE_SCSI) {
		newcommand = &full_command_packet->command.newcommand;
		newcommand->request_id__lunl =
			cpu_to_le16(TW_REQ_LUN_IN(TW_LUN_OUT(newcommand->request_id__lunl), request_id));
		if (length) {
			newcommand->sg_list[0].address = TW_CPU_TO_SGL(dma_handle + sizeof(TW_Ioctl_Buf_Apache) - 1);
			newcommand->sg_list[0].length = TW_CPU_TO_SGL(length);
		}
		newcommand->sgl_entries__lunh =
			cpu_to_le16(TW_REQ_LUN_IN(TW_LUN_OUT(newcommand->sgl_entries__lunh), length ? 1 : 0));
	} else {
		oldcommand = &full_command_packet->command.oldcommand;
		oldcommand->request_id = request_id;

		if (TW_SGL_OUT(oldcommand->opcode__sgloffset)) {
			
			sgl = (TW_SG_Entry_ISO *)((u32 *)oldcommand+oldcommand->size - (sizeof(TW_SG_Entry_ISO)/4) + pae + (sizeof(dma_addr_t) > 4 ? 1 : 0));
			sgl->address = TW_CPU_TO_SGL(dma_handle + sizeof(TW_Ioctl_Buf_Apache) - 1);
			sgl->length = TW_CPU_TO_SGL(length);
			oldcommand->size += pae;
			oldcommand->size += sizeof(dma_addr_t) > 4 ? 1 : 0;
		}
	}
} 

static long twl_chrdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long timeout;
	unsigned long *cpu_addr, data_buffer_length_adjusted = 0, flags = 0;
	dma_addr_t dma_handle;
	int request_id = 0;
	TW_Ioctl_Driver_Command driver_command;
	struct inode *inode = file->f_dentry->d_inode;
	TW_Ioctl_Buf_Apache *tw_ioctl;
	TW_Command_Full *full_command_packet;
	TW_Device_Extension *tw_dev = twl_device_extension_list[iminor(inode)];
	int retval = -EFAULT;
	void __user *argp = (void __user *)arg;

	mutex_lock(&twl_chrdev_mutex);

	
	if (mutex_lock_interruptible(&tw_dev->ioctl_lock)) {
		retval = -EINTR;
		goto out;
	}

	
	if (copy_from_user(&driver_command, argp, sizeof(TW_Ioctl_Driver_Command)))
		goto out2;

	
	if (driver_command.buffer_length > TW_MAX_SECTORS * 2048) {
		retval = -EINVAL;
		goto out2;
	}

	
	data_buffer_length_adjusted = (driver_command.buffer_length + 511) & ~511;

	
	cpu_addr = dma_alloc_coherent(&tw_dev->tw_pci_dev->dev, data_buffer_length_adjusted+sizeof(TW_Ioctl_Buf_Apache) - 1, &dma_handle, GFP_KERNEL);
	if (!cpu_addr) {
		retval = -ENOMEM;
		goto out2;
	}

	tw_ioctl = (TW_Ioctl_Buf_Apache *)cpu_addr;

	
	if (copy_from_user(tw_ioctl, argp, driver_command.buffer_length + sizeof(TW_Ioctl_Buf_Apache) - 1))
		goto out3;

	
	switch (cmd) {
	case TW_IOCTL_FIRMWARE_PASS_THROUGH:
		spin_lock_irqsave(tw_dev->host->host_lock, flags);
		twl_get_request_id(tw_dev, &request_id);

		
		tw_dev->srb[request_id] = NULL;

		
		tw_dev->chrdev_request_id = request_id;

		full_command_packet = (TW_Command_Full *)&tw_ioctl->firmware_command;

		
		twl_load_sgl(tw_dev, full_command_packet, request_id, dma_handle, data_buffer_length_adjusted);

		memcpy(tw_dev->command_packet_virt[request_id], &(tw_ioctl->firmware_command), sizeof(TW_Command_Full));

		
		twl_post_command_packet(tw_dev, request_id);
		spin_unlock_irqrestore(tw_dev->host->host_lock, flags);

		timeout = TW_IOCTL_CHRDEV_TIMEOUT*HZ;

		
		timeout = wait_event_timeout(tw_dev->ioctl_wqueue, tw_dev->chrdev_request_id == TW_IOCTL_CHRDEV_FREE, timeout);

		
		if (tw_dev->chrdev_request_id != TW_IOCTL_CHRDEV_FREE) {
			
			printk(KERN_WARNING "3w-sas: scsi%d: WARNING: (0x%02X:0x%04X): Character ioctl (0x%x) timed out, resetting card.\n",
			       tw_dev->host->host_no, TW_DRIVER, 0x6,
			       cmd);
			retval = -EIO;
			twl_reset_device_extension(tw_dev, 1);
			goto out3;
		}

		
		memcpy(&(tw_ioctl->firmware_command), tw_dev->command_packet_virt[request_id], sizeof(TW_Command_Full));
		
		
		spin_lock_irqsave(tw_dev->host->host_lock, flags);
		tw_dev->posted_request_count--;
		tw_dev->state[request_id] = TW_S_COMPLETED;
		twl_free_request_id(tw_dev, request_id);
		spin_unlock_irqrestore(tw_dev->host->host_lock, flags);
		break;
	default:
		retval = -ENOTTY;
		goto out3;
	}

	
	if (copy_to_user(argp, tw_ioctl, sizeof(TW_Ioctl_Buf_Apache) + driver_command.buffer_length - 1) == 0)
		retval = 0;
out3:
	
	dma_free_coherent(&tw_dev->tw_pci_dev->dev, data_buffer_length_adjusted+sizeof(TW_Ioctl_Buf_Apache) - 1, cpu_addr, dma_handle);
out2:
	mutex_unlock(&tw_dev->ioctl_lock);
out:
	mutex_unlock(&twl_chrdev_mutex);
	return retval;
} 

static int twl_chrdev_open(struct inode *inode, struct file *file)
{
	unsigned int minor_number;
	int retval = -ENODEV;

	if (!capable(CAP_SYS_ADMIN)) {
		retval = -EACCES;
		goto out;
	}

	minor_number = iminor(inode);
	if (minor_number >= twl_device_extension_count)
		goto out;
	retval = 0;
out:
	return retval;
} 

static const struct file_operations twl_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= twl_chrdev_ioctl,
	.open		= twl_chrdev_open,
	.release	= NULL,
	.llseek		= noop_llseek,
};

static int twl_fill_sense(TW_Device_Extension *tw_dev, int i, int request_id, int copy_sense, int print_host)
{
	TW_Command_Apache_Header *header;
	TW_Command_Full *full_command_packet;
	unsigned short error;
	char *error_str;
	int retval = 1;

	header = tw_dev->sense_buffer_virt[i];
	full_command_packet = tw_dev->command_packet_virt[request_id];

	
	error_str = &(header->err_specific_desc[strlen(header->err_specific_desc) + 1]);

	
	error = le16_to_cpu(header->status_block.error);
	if ((error != TW_ERROR_LOGICAL_UNIT_NOT_SUPPORTED) && (error != TW_ERROR_UNIT_OFFLINE) && (error != TW_ERROR_INVALID_FIELD_IN_CDB)) {
		if (print_host)
			printk(KERN_WARNING "3w-sas: scsi%d: ERROR: (0x%02X:0x%04X): %s:%s.\n",
			       tw_dev->host->host_no,
			       TW_MESSAGE_SOURCE_CONTROLLER_ERROR,
			       header->status_block.error,
			       error_str, 
			       header->err_specific_desc);
		else
			printk(KERN_WARNING "3w-sas: ERROR: (0x%02X:0x%04X): %s:%s.\n",
			       TW_MESSAGE_SOURCE_CONTROLLER_ERROR,
			       header->status_block.error,
			       error_str,
			       header->err_specific_desc);
	}

	if (copy_sense) {
		memcpy(tw_dev->srb[request_id]->sense_buffer, header->sense_data, TW_SENSE_DATA_LENGTH);
		tw_dev->srb[request_id]->result = (full_command_packet->command.newcommand.status << 1);
		goto out;
	}
out:
	return retval;
} 

static void twl_free_device_extension(TW_Device_Extension *tw_dev)
{
	if (tw_dev->command_packet_virt[0])
		pci_free_consistent(tw_dev->tw_pci_dev,
				    sizeof(TW_Command_Full)*TW_Q_LENGTH,
				    tw_dev->command_packet_virt[0],
				    tw_dev->command_packet_phys[0]);

	if (tw_dev->generic_buffer_virt[0])
		pci_free_consistent(tw_dev->tw_pci_dev,
				    TW_SECTOR_SIZE*TW_Q_LENGTH,
				    tw_dev->generic_buffer_virt[0],
				    tw_dev->generic_buffer_phys[0]);

	if (tw_dev->sense_buffer_virt[0])
		pci_free_consistent(tw_dev->tw_pci_dev,
				    sizeof(TW_Command_Apache_Header)*
				    TW_Q_LENGTH,
				    tw_dev->sense_buffer_virt[0],
				    tw_dev->sense_buffer_phys[0]);

	kfree(tw_dev->event_queue[0]);
} 

static void *twl_get_param(TW_Device_Extension *tw_dev, int request_id, int table_id, int parameter_id, int parameter_size_bytes)
{
	TW_Command_Full *full_command_packet;
	TW_Command *command_packet;
	TW_Param_Apache *param;
	void *retval = NULL;

	
	full_command_packet = tw_dev->command_packet_virt[request_id];
	memset(full_command_packet, 0, sizeof(TW_Command_Full));
	command_packet = &full_command_packet->command.oldcommand;

	command_packet->opcode__sgloffset = TW_OPSGL_IN(2, TW_OP_GET_PARAM);
	command_packet->size              = TW_COMMAND_SIZE;
	command_packet->request_id        = request_id;
	command_packet->byte6_offset.block_count = cpu_to_le16(1);

	
	param = (TW_Param_Apache *)tw_dev->generic_buffer_virt[request_id];
	memset(param, 0, TW_SECTOR_SIZE);
	param->table_id = cpu_to_le16(table_id | 0x8000);
	param->parameter_id = cpu_to_le16(parameter_id);
	param->parameter_size_bytes = cpu_to_le16(parameter_size_bytes);

	command_packet->byte8_offset.param.sgl[0].address = TW_CPU_TO_SGL(tw_dev->generic_buffer_phys[request_id]);
	command_packet->byte8_offset.param.sgl[0].length = TW_CPU_TO_SGL(TW_SECTOR_SIZE);

	
	twl_post_command_packet(tw_dev, request_id);

	
	if (twl_poll_response(tw_dev, request_id, 30))
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x7, "No valid response during get param")
	else
		retval = (void *)&(param->data[0]);

	tw_dev->posted_request_count--;
	tw_dev->state[request_id] = TW_S_INITIAL;

	return retval;
} 

static int twl_initconnection(TW_Device_Extension *tw_dev, int message_credits,
 			      u32 set_features, unsigned short current_fw_srl, 
			      unsigned short current_fw_arch_id, 
			      unsigned short current_fw_branch, 
			      unsigned short current_fw_build, 
			      unsigned short *fw_on_ctlr_srl, 
			      unsigned short *fw_on_ctlr_arch_id, 
			      unsigned short *fw_on_ctlr_branch, 
			      unsigned short *fw_on_ctlr_build, 
			      u32 *init_connect_result)
{
	TW_Command_Full *full_command_packet;
	TW_Initconnect *tw_initconnect;
	int request_id = 0, retval = 1;

	
	full_command_packet = tw_dev->command_packet_virt[request_id];
	memset(full_command_packet, 0, sizeof(TW_Command_Full));
	full_command_packet->header.header_desc.size_header = 128;
	
	tw_initconnect = (TW_Initconnect *)&full_command_packet->command.oldcommand;
	tw_initconnect->opcode__reserved = TW_OPRES_IN(0, TW_OP_INIT_CONNECTION);
	tw_initconnect->request_id = request_id;
	tw_initconnect->message_credits = cpu_to_le16(message_credits);
	tw_initconnect->features = set_features;

	
	tw_initconnect->features |= sizeof(dma_addr_t) > 4 ? 1 : 0;

	tw_initconnect->features = cpu_to_le32(tw_initconnect->features);

	if (set_features & TW_EXTENDED_INIT_CONNECT) {
		tw_initconnect->size = TW_INIT_COMMAND_PACKET_SIZE_EXTENDED;
		tw_initconnect->fw_srl = cpu_to_le16(current_fw_srl);
		tw_initconnect->fw_arch_id = cpu_to_le16(current_fw_arch_id);
		tw_initconnect->fw_branch = cpu_to_le16(current_fw_branch);
		tw_initconnect->fw_build = cpu_to_le16(current_fw_build);
	} else 
		tw_initconnect->size = TW_INIT_COMMAND_PACKET_SIZE;

	
	twl_post_command_packet(tw_dev, request_id);

	
	if (twl_poll_response(tw_dev, request_id, 30)) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x8, "No valid response during init connection");
	} else {
		if (set_features & TW_EXTENDED_INIT_CONNECT) {
			*fw_on_ctlr_srl = le16_to_cpu(tw_initconnect->fw_srl);
			*fw_on_ctlr_arch_id = le16_to_cpu(tw_initconnect->fw_arch_id);
			*fw_on_ctlr_branch = le16_to_cpu(tw_initconnect->fw_branch);
			*fw_on_ctlr_build = le16_to_cpu(tw_initconnect->fw_build);
			*init_connect_result = le32_to_cpu(tw_initconnect->result);
		}
		retval = 0;
	}

	tw_dev->posted_request_count--;
	tw_dev->state[request_id] = TW_S_INITIAL;

	return retval;
} 

static int twl_initialize_device_extension(TW_Device_Extension *tw_dev)
{
	int i, retval = 1;

	
	if (twl_allocate_memory(tw_dev, sizeof(TW_Command_Full), 0)) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x9, "Command packet memory allocation failed");
		goto out;
	}

	
	if (twl_allocate_memory(tw_dev, TW_SECTOR_SIZE, 1)) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0xa, "Generic memory allocation failed");
		goto out;
	}

	
	if (twl_allocate_memory(tw_dev, sizeof(TW_Command_Apache_Header), 2)) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0xb, "Sense buffer allocation failed");
		goto out;
	}

	
	tw_dev->event_queue[0] = kcalloc(TW_Q_LENGTH, sizeof(TW_Event), GFP_KERNEL);
	if (!tw_dev->event_queue[0]) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0xc, "Event info memory allocation failed");
		goto out;
	}

	for (i = 0; i < TW_Q_LENGTH; i++) {
		tw_dev->event_queue[i] = (TW_Event *)((unsigned char *)tw_dev->event_queue[0] + (i * sizeof(TW_Event)));
		tw_dev->free_queue[i] = i;
		tw_dev->state[i] = TW_S_INITIAL;
	}

	tw_dev->free_head = TW_Q_START;
	tw_dev->free_tail = TW_Q_START;
	tw_dev->error_sequence_id = 1;
	tw_dev->chrdev_request_id = TW_IOCTL_CHRDEV_FREE;

	mutex_init(&tw_dev->ioctl_lock);
	init_waitqueue_head(&tw_dev->ioctl_wqueue);

	retval = 0;
out:
	return retval;
} 

static void twl_unmap_scsi_data(TW_Device_Extension *tw_dev, int request_id)
{
	struct scsi_cmnd *cmd = tw_dev->srb[request_id];

	if (cmd->SCp.phase == TW_PHASE_SGLIST)
		scsi_dma_unmap(cmd);
} 

static int twl_handle_attention_interrupt(TW_Device_Extension *tw_dev)
{
	int retval = 1;
	u32 request_id, doorbell;

	
	doorbell = readl(TWL_HOBDB_REG_ADDR(tw_dev));

	
	if (doorbell & TWL_DOORBELL_CONTROLLER_ERROR) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0xd, "Microcontroller Error: clearing");
		goto out;
	}

	
	if (doorbell & TWL_DOORBELL_ATTENTION_INTERRUPT) {
		if (!(test_and_set_bit(TW_IN_ATTENTION_LOOP, &tw_dev->flags))) {
			twl_get_request_id(tw_dev, &request_id);
			if (twl_aen_read_queue(tw_dev, request_id)) {
				tw_dev->state[request_id] = TW_S_COMPLETED;
				twl_free_request_id(tw_dev, request_id);
				clear_bit(TW_IN_ATTENTION_LOOP, &tw_dev->flags);
			}
		}
	}

	retval = 0;
out:
	
	TWL_CLEAR_DB_INTERRUPT(tw_dev);

	
	readl(TWL_HOBDBC_REG_ADDR(tw_dev));

	return retval;
} 

static irqreturn_t twl_interrupt(int irq, void *dev_instance)
{
	TW_Device_Extension *tw_dev = (TW_Device_Extension *)dev_instance;
	int i, handled = 0, error = 0;
	dma_addr_t mfa = 0;
	u32 reg, regl, regh, response, request_id = 0;
	struct scsi_cmnd *cmd;
	TW_Command_Full *full_command_packet;

	spin_lock(tw_dev->host->host_lock);

	
	reg = readl(TWL_HISTAT_REG_ADDR(tw_dev));

	
	if (!(reg & TWL_HISTATUS_VALID_INTERRUPT))
		goto twl_interrupt_bail;

	handled = 1;

	
	if (test_bit(TW_IN_RESET, &tw_dev->flags))
		goto twl_interrupt_bail;

	
	if (reg & TWL_HISTATUS_ATTENTION_INTERRUPT) {
		if (twl_handle_attention_interrupt(tw_dev)) {
			TWL_MASK_INTERRUPTS(tw_dev);
			goto twl_interrupt_bail;
		}
	}

	
	while (reg & TWL_HISTATUS_RESPONSE_INTERRUPT) {
		if (sizeof(dma_addr_t) > 4) {
			regh = readl(TWL_HOBQPH_REG_ADDR(tw_dev));
			regl = readl(TWL_HOBQPL_REG_ADDR(tw_dev));
			mfa = ((u64)regh << 32) | regl;
		} else
			mfa = readl(TWL_HOBQPL_REG_ADDR(tw_dev));

		error = 0;
		response = (u32)mfa;

		
		if (!TW_NOTMFA_OUT(response)) {
			for (i=0;i<TW_Q_LENGTH;i++) {
				if (tw_dev->sense_buffer_phys[i] == mfa) {
					request_id = le16_to_cpu(tw_dev->sense_buffer_virt[i]->header_desc.request_id);
					if (tw_dev->srb[request_id] != NULL)
						error = twl_fill_sense(tw_dev, i, request_id, 1, 1);
					else {
						
						if (request_id != tw_dev->chrdev_request_id)
							error = twl_fill_sense(tw_dev, i, request_id, 0, 1);
						else
							memcpy(tw_dev->command_packet_virt[request_id], tw_dev->sense_buffer_virt[i], sizeof(TW_Command_Apache_Header));
					}

					
					writel((u32)((u64)tw_dev->sense_buffer_phys[i] >> 32), TWL_HOBQPH_REG_ADDR(tw_dev));
					writel((u32)tw_dev->sense_buffer_phys[i], TWL_HOBQPL_REG_ADDR(tw_dev));
					break;
				}
			}
		} else
			request_id = TW_RESID_OUT(response);

		full_command_packet = tw_dev->command_packet_virt[request_id];

		
		if (tw_dev->state[request_id] != TW_S_POSTED) {
			if (tw_dev->srb[request_id] != NULL) {
				TW_PRINTK(tw_dev->host, TW_DRIVER, 0xe, "Received a request id that wasn't posted");
				TWL_MASK_INTERRUPTS(tw_dev);
				goto twl_interrupt_bail;
			}
		}

		
		if (tw_dev->srb[request_id] == NULL) {
			if (request_id != tw_dev->chrdev_request_id) {
				if (twl_aen_complete(tw_dev, request_id))
					TW_PRINTK(tw_dev->host, TW_DRIVER, 0xf, "Error completing AEN during attention interrupt");
			} else {
				tw_dev->chrdev_request_id = TW_IOCTL_CHRDEV_FREE;
				wake_up(&tw_dev->ioctl_wqueue);
			}
		} else {
			cmd = tw_dev->srb[request_id];

			if (!error)
				cmd->result = (DID_OK << 16);
			
			
			if ((scsi_sg_count(cmd) <= 1) && (full_command_packet->command.newcommand.status == 0)) {
				if (full_command_packet->command.newcommand.sg_list[0].length < scsi_bufflen(tw_dev->srb[request_id]))
					scsi_set_resid(cmd, scsi_bufflen(cmd) - full_command_packet->command.newcommand.sg_list[0].length);
			}

			
			tw_dev->state[request_id] = TW_S_COMPLETED;
			twl_free_request_id(tw_dev, request_id);
			tw_dev->posted_request_count--;
			tw_dev->srb[request_id]->scsi_done(tw_dev->srb[request_id]);
			twl_unmap_scsi_data(tw_dev, request_id);
		}

		
		reg = readl(TWL_HISTAT_REG_ADDR(tw_dev));
	}

twl_interrupt_bail:
	spin_unlock(tw_dev->host->host_lock);
	return IRQ_RETVAL(handled);
} 

static int twl_poll_register(TW_Device_Extension *tw_dev, void *reg, u32 value, u32 result, int seconds)
{
	unsigned long before;
	int retval = 1;
	u32 reg_value;

	reg_value = readl(reg);
	before = jiffies;

        while ((reg_value & value) != result) {
		reg_value = readl(reg);
		if (time_after(jiffies, before + HZ * seconds))
			goto out;
		msleep(50);
	}
	retval = 0;
out:
	return retval;
} 

static int twl_reset_sequence(TW_Device_Extension *tw_dev, int soft_reset)
{
	int retval = 1;
	int i = 0;
	u32 status = 0;
	unsigned short fw_on_ctlr_srl = 0, fw_on_ctlr_arch_id = 0;
	unsigned short fw_on_ctlr_branch = 0, fw_on_ctlr_build = 0;
	u32 init_connect_result = 0;
	int tries = 0;
	int do_soft_reset = soft_reset;

	while (tries < TW_MAX_RESET_TRIES) {
		
		if (do_soft_reset) {
			TWL_SOFT_RESET(tw_dev);

			
			if (twl_poll_register(tw_dev, TWL_SCRPD3_REG_ADDR(tw_dev), TWL_CONTROLLER_READY, 0x0, 30)) {
				TW_PRINTK(tw_dev->host, TW_DRIVER, 0x10, "Controller never went non-ready during reset sequence");
				tries++;
				continue;
			}
			if (twl_poll_register(tw_dev, TWL_SCRPD3_REG_ADDR(tw_dev), TWL_CONTROLLER_READY, TWL_CONTROLLER_READY, 60)) {
				TW_PRINTK(tw_dev->host, TW_DRIVER, 0x11, "Controller not ready during reset sequence");
				tries++;
				continue;
			}
		}

		
		if (twl_initconnection(tw_dev, TW_INIT_MESSAGE_CREDITS,
				       TW_EXTENDED_INIT_CONNECT, TW_CURRENT_DRIVER_SRL,
				       TW_9750_ARCH_ID, TW_CURRENT_DRIVER_BRANCH,
				       TW_CURRENT_DRIVER_BUILD, &fw_on_ctlr_srl,
				       &fw_on_ctlr_arch_id, &fw_on_ctlr_branch,
				       &fw_on_ctlr_build, &init_connect_result)) {
			TW_PRINTK(tw_dev->host, TW_DRIVER, 0x12, "Initconnection failed while checking SRL");
			do_soft_reset = 1;
			tries++;
			continue;
		}

		
		while (i < TW_Q_LENGTH) {
			writel((u32)((u64)tw_dev->sense_buffer_phys[i] >> 32), TWL_HOBQPH_REG_ADDR(tw_dev));
			writel((u32)tw_dev->sense_buffer_phys[i], TWL_HOBQPL_REG_ADDR(tw_dev));

			
			status = readl(TWL_STATUS_REG_ADDR(tw_dev));
			if (!(status & TWL_STATUS_OVERRUN_SUBMIT))
			    i++;
		}

		
		status = readl(TWL_STATUS_REG_ADDR(tw_dev));
		if (status) {
			TW_PRINTK(tw_dev->host, TW_DRIVER, 0x13, "Bad controller status after loading sense buffers");
			do_soft_reset = 1;
			tries++;
			continue;
		}

		
		if (twl_aen_drain_queue(tw_dev, soft_reset)) {
			TW_PRINTK(tw_dev->host, TW_DRIVER, 0x14, "AEN drain failed during reset sequence");
			do_soft_reset = 1;
			tries++;
			continue;
		}

		
		strncpy(tw_dev->tw_compat_info.driver_version, TW_DRIVER_VERSION, strlen(TW_DRIVER_VERSION));
		tw_dev->tw_compat_info.driver_srl_high = TW_CURRENT_DRIVER_SRL;
		tw_dev->tw_compat_info.driver_branch_high = TW_CURRENT_DRIVER_BRANCH;
		tw_dev->tw_compat_info.driver_build_high = TW_CURRENT_DRIVER_BUILD;
		tw_dev->tw_compat_info.driver_srl_low = TW_BASE_FW_SRL;
		tw_dev->tw_compat_info.driver_branch_low = TW_BASE_FW_BRANCH;
		tw_dev->tw_compat_info.driver_build_low = TW_BASE_FW_BUILD;
		tw_dev->tw_compat_info.fw_on_ctlr_srl = fw_on_ctlr_srl;
		tw_dev->tw_compat_info.fw_on_ctlr_branch = fw_on_ctlr_branch;
		tw_dev->tw_compat_info.fw_on_ctlr_build = fw_on_ctlr_build;

		
		retval = 0;
		goto out;
	}
out:
	return retval;
} 

static int twl_reset_device_extension(TW_Device_Extension *tw_dev, int ioctl_reset)
{
	int i = 0, retval = 1;
	unsigned long flags = 0;

	
	if (ioctl_reset)
		scsi_block_requests(tw_dev->host);

	set_bit(TW_IN_RESET, &tw_dev->flags);
	TWL_MASK_INTERRUPTS(tw_dev);
	TWL_CLEAR_DB_INTERRUPT(tw_dev);

	spin_lock_irqsave(tw_dev->host->host_lock, flags);

	
	for (i = 0; i < TW_Q_LENGTH; i++) {
		if ((tw_dev->state[i] != TW_S_FINISHED) &&
		    (tw_dev->state[i] != TW_S_INITIAL) &&
		    (tw_dev->state[i] != TW_S_COMPLETED)) {
			if (tw_dev->srb[i]) {
				tw_dev->srb[i]->result = (DID_RESET << 16);
				tw_dev->srb[i]->scsi_done(tw_dev->srb[i]);
				twl_unmap_scsi_data(tw_dev, i);
			}
		}
	}

	
	for (i = 0; i < TW_Q_LENGTH; i++) {
		tw_dev->free_queue[i] = i;
		tw_dev->state[i] = TW_S_INITIAL;
	}
	tw_dev->free_head = TW_Q_START;
	tw_dev->free_tail = TW_Q_START;
	tw_dev->posted_request_count = 0;

	spin_unlock_irqrestore(tw_dev->host->host_lock, flags);

	if (twl_reset_sequence(tw_dev, 1))
		goto out;

	TWL_UNMASK_INTERRUPTS(tw_dev);

	clear_bit(TW_IN_RESET, &tw_dev->flags);
	tw_dev->chrdev_request_id = TW_IOCTL_CHRDEV_FREE;

	retval = 0;
out:
	if (ioctl_reset)
		scsi_unblock_requests(tw_dev->host);
	return retval;
} 

static int twl_scsi_biosparam(struct scsi_device *sdev, struct block_device *bdev, sector_t capacity, int geom[])
{
	int heads, sectors;
	TW_Device_Extension *tw_dev;

	tw_dev = (TW_Device_Extension *)sdev->host->hostdata;

	if (capacity >= 0x200000) {
		heads = 255;
		sectors = 63;
	} else {
		heads = 64;
		sectors = 32;
	}

	geom[0] = heads;
	geom[1] = sectors;
	geom[2] = sector_div(capacity, heads * sectors); 

	return 0;
} 

static int twl_scsi_eh_reset(struct scsi_cmnd *SCpnt)
{
	TW_Device_Extension *tw_dev = NULL;
	int retval = FAILED;

	tw_dev = (TW_Device_Extension *)SCpnt->device->host->hostdata;

	tw_dev->num_resets++;

	sdev_printk(KERN_WARNING, SCpnt->device,
		"WARNING: (0x%02X:0x%04X): Command (0x%x) timed out, resetting card.\n",
		TW_DRIVER, 0x2c, SCpnt->cmnd[0]);

	
	mutex_lock(&tw_dev->ioctl_lock);

	
	if (twl_reset_device_extension(tw_dev, 0)) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x15, "Controller reset failed during scsi host reset");
		goto out;
	}

	retval = SUCCESS;
out:
	mutex_unlock(&tw_dev->ioctl_lock);
	return retval;
} 

static int twl_scsi_queue_lck(struct scsi_cmnd *SCpnt, void (*done)(struct scsi_cmnd *))
{
	int request_id, retval;
	TW_Device_Extension *tw_dev = (TW_Device_Extension *)SCpnt->device->host->hostdata;

	
	if (test_bit(TW_IN_RESET, &tw_dev->flags)) {
		retval = SCSI_MLQUEUE_HOST_BUSY;
		goto out;
	}

	
	SCpnt->scsi_done = done;
		
	
	twl_get_request_id(tw_dev, &request_id);

	
	tw_dev->srb[request_id] = SCpnt;

	
	SCpnt->SCp.phase = TW_PHASE_INITIAL;

	retval = twl_scsiop_execute_scsi(tw_dev, request_id, NULL, 0, NULL);
	if (retval) {
		tw_dev->state[request_id] = TW_S_COMPLETED;
		twl_free_request_id(tw_dev, request_id);
		SCpnt->result = (DID_ERROR << 16);
		done(SCpnt);
		retval = 0;
	}
out:
	return retval;
} 

static DEF_SCSI_QCMD(twl_scsi_queue)

static void __twl_shutdown(TW_Device_Extension *tw_dev)
{
	
	TWL_MASK_INTERRUPTS(tw_dev);

	
	free_irq(tw_dev->tw_pci_dev->irq, tw_dev);

	printk(KERN_WARNING "3w-sas: Shutting down host %d.\n", tw_dev->host->host_no);

	
	if (twl_initconnection(tw_dev, 1, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL)) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x16, "Connection shutdown failed");
	} else {
		printk(KERN_WARNING "3w-sas: Shutdown complete.\n");
	}

	
	TWL_CLEAR_DB_INTERRUPT(tw_dev);
} 

static void twl_shutdown(struct pci_dev *pdev)
{
	struct Scsi_Host *host = pci_get_drvdata(pdev);
	TW_Device_Extension *tw_dev;

	if (!host)
		return;

	tw_dev = (TW_Device_Extension *)host->hostdata;

	if (tw_dev->online) 
		__twl_shutdown(tw_dev);
} 

static int twl_slave_configure(struct scsi_device *sdev)
{
	
	blk_queue_rq_timeout(sdev->request_queue, 60 * HZ);

	return 0;
} 

static struct scsi_host_template driver_template = {
	.module			= THIS_MODULE,
	.name			= "3w-sas",
	.queuecommand		= twl_scsi_queue,
	.eh_host_reset_handler	= twl_scsi_eh_reset,
	.bios_param		= twl_scsi_biosparam,
	.change_queue_depth	= twl_change_queue_depth,
	.can_queue		= TW_Q_LENGTH-2,
	.slave_configure	= twl_slave_configure,
	.this_id		= -1,
	.sg_tablesize		= TW_LIBERATOR_MAX_SGL_LENGTH,
	.max_sectors		= TW_MAX_SECTORS,
	.cmd_per_lun		= TW_MAX_CMDS_PER_LUN,
	.use_clustering		= ENABLE_CLUSTERING,
	.shost_attrs		= twl_host_attrs,
	.emulated		= 1
};

static int __devinit twl_probe(struct pci_dev *pdev, const struct pci_device_id *dev_id)
{
	struct Scsi_Host *host = NULL;
	TW_Device_Extension *tw_dev;
	int retval = -ENODEV;
	int *ptr_phycount, phycount=0;

	retval = pci_enable_device(pdev);
	if (retval) {
		TW_PRINTK(host, TW_DRIVER, 0x17, "Failed to enable pci device");
		goto out_disable_device;
	}

	pci_set_master(pdev);
	pci_try_set_mwi(pdev);

	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64))
	    || pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64)))
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32))
		    || pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32))) {
			TW_PRINTK(host, TW_DRIVER, 0x18, "Failed to set dma mask");
			retval = -ENODEV;
			goto out_disable_device;
		}

	host = scsi_host_alloc(&driver_template, sizeof(TW_Device_Extension));
	if (!host) {
		TW_PRINTK(host, TW_DRIVER, 0x19, "Failed to allocate memory for device extension");
		retval = -ENOMEM;
		goto out_disable_device;
	}
	tw_dev = shost_priv(host);

	
	tw_dev->host = host;
	tw_dev->tw_pci_dev = pdev;

	if (twl_initialize_device_extension(tw_dev)) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x1a, "Failed to initialize device extension");
		goto out_free_device_extension;
	}

	
	retval = pci_request_regions(pdev, "3w-sas");
	if (retval) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x1b, "Failed to get mem region");
		goto out_free_device_extension;
	}

	
	tw_dev->base_addr = pci_iomap(pdev, 1, 0);
	if (!tw_dev->base_addr) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x1c, "Failed to ioremap");
		goto out_release_mem_region;
	}

	
	TWL_MASK_INTERRUPTS(tw_dev);

	
	if (twl_reset_sequence(tw_dev, 0)) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x1d, "Controller reset failed during probe");
		goto out_iounmap;
	}

	
	host->max_id = TW_MAX_UNITS;
	host->max_cmd_len = TW_MAX_CDB_LEN;
	host->max_lun = TW_MAX_LUNS;
	host->max_channel = 0;

	
	retval = scsi_add_host(host, &pdev->dev);
	if (retval) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x1e, "scsi add host failed");
		goto out_iounmap;
	}

	pci_set_drvdata(pdev, host);

	printk(KERN_WARNING "3w-sas: scsi%d: Found an LSI 3ware %s Controller at 0x%llx, IRQ: %d.\n",
	       host->host_no,
	       (char *)twl_get_param(tw_dev, 1, TW_VERSION_TABLE,
				     TW_PARAM_MODEL, TW_PARAM_MODEL_LENGTH),
	       (u64)pci_resource_start(pdev, 1), pdev->irq);

	ptr_phycount = twl_get_param(tw_dev, 2, TW_PARAM_PHY_SUMMARY_TABLE,
				     TW_PARAM_PHYCOUNT, TW_PARAM_PHYCOUNT_LENGTH);
	if (ptr_phycount)
		phycount = le32_to_cpu(*(int *)ptr_phycount);

	printk(KERN_WARNING "3w-sas: scsi%d: Firmware %s, BIOS %s, Phys: %d.\n",
	       host->host_no,
	       (char *)twl_get_param(tw_dev, 1, TW_VERSION_TABLE,
				     TW_PARAM_FWVER, TW_PARAM_FWVER_LENGTH),
	       (char *)twl_get_param(tw_dev, 2, TW_VERSION_TABLE,
				     TW_PARAM_BIOSVER, TW_PARAM_BIOSVER_LENGTH),
	       phycount);

	
	if (use_msi && !pci_enable_msi(pdev))
		set_bit(TW_USING_MSI, &tw_dev->flags);

	
	retval = request_irq(pdev->irq, twl_interrupt, IRQF_SHARED, "3w-sas", tw_dev);
	if (retval) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x1f, "Error requesting IRQ");
		goto out_remove_host;
	}

	twl_device_extension_list[twl_device_extension_count] = tw_dev;
	twl_device_extension_count++;

	
	TWL_UNMASK_INTERRUPTS(tw_dev);
	
	
	scsi_scan_host(host);

	
	if (sysfs_create_bin_file(&host->shost_dev.kobj, &twl_sysfs_aen_read_attr))
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x20, "Failed to create sysfs binary file: 3ware_aen_read");
	if (sysfs_create_bin_file(&host->shost_dev.kobj, &twl_sysfs_compat_info_attr))
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x21, "Failed to create sysfs binary file: 3ware_compat_info");

	if (twl_major == -1) {
		if ((twl_major = register_chrdev (0, "twl", &twl_fops)) < 0)
			TW_PRINTK(host, TW_DRIVER, 0x22, "Failed to register character device");
	}
	tw_dev->online = 1;
	return 0;

out_remove_host:
	if (test_bit(TW_USING_MSI, &tw_dev->flags))
		pci_disable_msi(pdev);
	scsi_remove_host(host);
out_iounmap:
	iounmap(tw_dev->base_addr);
out_release_mem_region:
	pci_release_regions(pdev);
out_free_device_extension:
	twl_free_device_extension(tw_dev);
	scsi_host_put(host);
out_disable_device:
	pci_disable_device(pdev);

	return retval;
} 

static void twl_remove(struct pci_dev *pdev)
{
	struct Scsi_Host *host = pci_get_drvdata(pdev);
	TW_Device_Extension *tw_dev;

	if (!host)
		return;

	tw_dev = (TW_Device_Extension *)host->hostdata;

	if (!tw_dev->online)
		return;

	
	sysfs_remove_bin_file(&host->shost_dev.kobj, &twl_sysfs_aen_read_attr);
	sysfs_remove_bin_file(&host->shost_dev.kobj, &twl_sysfs_compat_info_attr);

	scsi_remove_host(tw_dev->host);

	
	if (twl_major >= 0) {
		unregister_chrdev(twl_major, "twl");
		twl_major = -1;
	}

	
	__twl_shutdown(tw_dev);

	
	if (test_bit(TW_USING_MSI, &tw_dev->flags))
		pci_disable_msi(pdev);

	
	iounmap(tw_dev->base_addr);

	
	pci_release_regions(pdev);

	
	twl_free_device_extension(tw_dev);

	scsi_host_put(tw_dev->host);
	pci_disable_device(pdev);
	twl_device_extension_count--;
} 

#ifdef CONFIG_PM
static int twl_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct Scsi_Host *host = pci_get_drvdata(pdev);
	TW_Device_Extension *tw_dev = (TW_Device_Extension *)host->hostdata;

	printk(KERN_WARNING "3w-sas: Suspending host %d.\n", tw_dev->host->host_no);
	
	TWL_MASK_INTERRUPTS(tw_dev);

	free_irq(tw_dev->tw_pci_dev->irq, tw_dev);

	
	if (twl_initconnection(tw_dev, 1, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL)) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x23, "Connection shutdown failed during suspend");
	} else {
		printk(KERN_WARNING "3w-sas: Suspend complete.\n");
	}

	
	TWL_CLEAR_DB_INTERRUPT(tw_dev);

	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
} 

static int twl_resume(struct pci_dev *pdev)
{
	int retval = 0;
	struct Scsi_Host *host = pci_get_drvdata(pdev);
	TW_Device_Extension *tw_dev = (TW_Device_Extension *)host->hostdata;

	printk(KERN_WARNING "3w-sas: Resuming host %d.\n", tw_dev->host->host_no);
	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);

	retval = pci_enable_device(pdev);
	if (retval) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x24, "Enable device failed during resume");
		return retval;
	}

	pci_set_master(pdev);
	pci_try_set_mwi(pdev);

	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64))
	    || pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64)))
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32))
		    || pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32))) {
			TW_PRINTK(host, TW_DRIVER, 0x25, "Failed to set dma mask during resume");
			retval = -ENODEV;
			goto out_disable_device;
		}

	
	if (twl_reset_sequence(tw_dev, 0)) {
		retval = -ENODEV;
		goto out_disable_device;
	}

	
	retval = request_irq(pdev->irq, twl_interrupt, IRQF_SHARED, "3w-sas", tw_dev);
	if (retval) {
		TW_PRINTK(tw_dev->host, TW_DRIVER, 0x26, "Error requesting IRQ during resume");
		retval = -ENODEV;
		goto out_disable_device;
	}

	
	if (test_bit(TW_USING_MSI, &tw_dev->flags))
		pci_enable_msi(pdev);

	
	TWL_UNMASK_INTERRUPTS(tw_dev);

	printk(KERN_WARNING "3w-sas: Resume complete.\n");
	return 0;

out_disable_device:
	scsi_remove_host(host);
	pci_disable_device(pdev);

	return retval;
} 
#endif

static struct pci_device_id twl_pci_tbl[] __devinitdata = {
	{ PCI_VDEVICE(3WARE, PCI_DEVICE_ID_3WARE_9750) },
	{ }
};
MODULE_DEVICE_TABLE(pci, twl_pci_tbl);

static struct pci_driver twl_driver = {
	.name		= "3w-sas",
	.id_table	= twl_pci_tbl,
	.probe		= twl_probe,
	.remove		= twl_remove,
#ifdef CONFIG_PM
	.suspend	= twl_suspend,
	.resume		= twl_resume,
#endif
	.shutdown	= twl_shutdown
};

static int __init twl_init(void)
{
	printk(KERN_INFO "LSI 3ware SAS/SATA-RAID Controller device driver for Linux v%s.\n", TW_DRIVER_VERSION);

	return pci_register_driver(&twl_driver);
} 

static void __exit twl_exit(void)
{
	pci_unregister_driver(&twl_driver);
} 

module_init(twl_init);
module_exit(twl_exit);

