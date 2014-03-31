/*
 * Copyright (c) 1996 John Shifflett, GeoLog Consulting
 *    john@geolog.com
 *    jshiffle@netcom.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/module.h>

#include <linux/string.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>

#include <asm/irq.h>

#include "wd33c93.h"

#define optimum_sx_per(hostdata) (hostdata)->sx_table[1].period_ns


#define WD33C93_VERSION    "1.26++"
#define WD33C93_DATE       "10/Feb/2007"

MODULE_AUTHOR("John Shifflett");
MODULE_DESCRIPTION("Generic WD33C93 SCSI driver");
MODULE_LICENSE("GPL");


static char *setup_args[] = { "", "", "", "", "", "", "", "", "", "" };

static char *setup_strings;
module_param(setup_strings, charp, 0);

static void wd33c93_execute(struct Scsi_Host *instance);

#ifdef CONFIG_WD33C93_PIO
static inline uchar
read_wd33c93(const wd33c93_regs regs, uchar reg_num)
{
	uchar data;

	outb(reg_num, regs.SASR);
	data = inb(regs.SCMD);
	return data;
}

static inline unsigned long
read_wd33c93_count(const wd33c93_regs regs)
{
	unsigned long value;

	outb(WD_TRANSFER_COUNT_MSB, regs.SASR);
	value = inb(regs.SCMD) << 16;
	value |= inb(regs.SCMD) << 8;
	value |= inb(regs.SCMD);
	return value;
}

static inline uchar
read_aux_stat(const wd33c93_regs regs)
{
	return inb(regs.SASR);
}

static inline void
write_wd33c93(const wd33c93_regs regs, uchar reg_num, uchar value)
{
      outb(reg_num, regs.SASR);
      outb(value, regs.SCMD);
}

static inline void
write_wd33c93_count(const wd33c93_regs regs, unsigned long value)
{
	outb(WD_TRANSFER_COUNT_MSB, regs.SASR);
	outb((value >> 16) & 0xff, regs.SCMD);
	outb((value >> 8) & 0xff, regs.SCMD);
	outb( value & 0xff, regs.SCMD);
}

#define write_wd33c93_cmd(regs, cmd) \
	write_wd33c93((regs), WD_COMMAND, (cmd))

static inline void
write_wd33c93_cdb(const wd33c93_regs regs, uint len, uchar cmnd[])
{
	int i;

	outb(WD_CDB_1, regs.SASR);
	for (i=0; i<len; i++)
		outb(cmnd[i], regs.SCMD);
}

#else 
static inline uchar
read_wd33c93(const wd33c93_regs regs, uchar reg_num)
{
	*regs.SASR = reg_num;
	mb();
	return (*regs.SCMD);
}

static unsigned long
read_wd33c93_count(const wd33c93_regs regs)
{
	unsigned long value;

	*regs.SASR = WD_TRANSFER_COUNT_MSB;
	mb();
	value = *regs.SCMD << 16;
	value |= *regs.SCMD << 8;
	value |= *regs.SCMD;
	mb();
	return value;
}

static inline uchar
read_aux_stat(const wd33c93_regs regs)
{
	return *regs.SASR;
}

static inline void
write_wd33c93(const wd33c93_regs regs, uchar reg_num, uchar value)
{
	*regs.SASR = reg_num;
	mb();
	*regs.SCMD = value;
	mb();
}

static void
write_wd33c93_count(const wd33c93_regs regs, unsigned long value)
{
	*regs.SASR = WD_TRANSFER_COUNT_MSB;
	mb();
	*regs.SCMD = value >> 16;
	*regs.SCMD = value >> 8;
	*regs.SCMD = value;
	mb();
}

static inline void
write_wd33c93_cmd(const wd33c93_regs regs, uchar cmd)
{
	*regs.SASR = WD_COMMAND;
	mb();
	*regs.SCMD = cmd;
	mb();
}

static inline void
write_wd33c93_cdb(const wd33c93_regs regs, uint len, uchar cmnd[])
{
	int i;

	*regs.SASR = WD_CDB_1;
	for (i = 0; i < len; i++)
		*regs.SCMD = cmnd[i];
}
#endif 

static inline uchar
read_1_byte(const wd33c93_regs regs)
{
	uchar asr;
	uchar x = 0;

	write_wd33c93(regs, WD_CONTROL, CTRL_IDI | CTRL_EDI | CTRL_POLLED);
	write_wd33c93_cmd(regs, WD_CMD_TRANS_INFO | 0x80);
	do {
		asr = read_aux_stat(regs);
		if (asr & ASR_DBR)
			x = read_wd33c93(regs, WD_DATA);
	} while (!(asr & ASR_INT));
	return x;
}

static int
round_period(unsigned int period, const struct sx_period *sx_table)
{
	int x;

	for (x = 1; sx_table[x].period_ns; x++) {
		if ((period <= sx_table[x - 0].period_ns) &&
		    (period > sx_table[x - 1].period_ns)) {
			return x;
		}
	}
	return 7;
}

static uchar
calc_sync_xfer(unsigned int period, unsigned int offset, unsigned int fast,
               const struct sx_period *sx_table)
{
	uchar result;

	if (offset && fast) {
		fast = STR_FSS;
		period *= 2;
	} else {
		fast = 0;
	}
	period *= 4;		
	result = sx_table[round_period(period,sx_table)].reg_value;
	result |= (offset < OPTIMUM_SX_OFF) ? offset : OPTIMUM_SX_OFF;
	result |= fast;
	return result;
}

static inline void
calc_sync_msg(unsigned int period, unsigned int offset, unsigned int fast,
                uchar  msg[2])
{
	period /= 4;
	if (offset && fast)
		period /= 2;
	msg[0] = period;
	msg[1] = offset;
}

static int
wd33c93_queuecommand_lck(struct scsi_cmnd *cmd,
		void (*done)(struct scsi_cmnd *))
{
	struct WD33C93_hostdata *hostdata;
	struct scsi_cmnd *tmp;

	hostdata = (struct WD33C93_hostdata *) cmd->device->host->hostdata;

	DB(DB_QUEUE_COMMAND,
	   printk("Q-%d-%02x( ", cmd->device->id, cmd->cmnd[0]))

	cmd->host_scribble = NULL;
	cmd->scsi_done = done;
	cmd->result = 0;


	if (scsi_bufflen(cmd)) {
		cmd->SCp.buffer = scsi_sglist(cmd);
		cmd->SCp.buffers_residual = scsi_sg_count(cmd) - 1;
		cmd->SCp.ptr = sg_virt(cmd->SCp.buffer);
		cmd->SCp.this_residual = cmd->SCp.buffer->length;
	} else {
		cmd->SCp.buffer = NULL;
		cmd->SCp.buffers_residual = 0;
		cmd->SCp.ptr = NULL;
		cmd->SCp.this_residual = 0;
	}


	cmd->SCp.Status = ILLEGAL_STATUS_BYTE;


	spin_lock_irq(&hostdata->lock);

	if (!(hostdata->input_Q) || (cmd->cmnd[0] == REQUEST_SENSE)) {
		cmd->host_scribble = (uchar *) hostdata->input_Q;
		hostdata->input_Q = cmd;
	} else {		
		for (tmp = (struct scsi_cmnd *) hostdata->input_Q;
		     tmp->host_scribble;
		     tmp = (struct scsi_cmnd *) tmp->host_scribble) ;
		tmp->host_scribble = (uchar *) cmd;
	}


	wd33c93_execute(cmd->device->host);

	DB(DB_QUEUE_COMMAND, printk(")Q "))

	spin_unlock_irq(&hostdata->lock);
	return 0;
}

DEF_SCSI_QCMD(wd33c93_queuecommand)

static void
wd33c93_execute(struct Scsi_Host *instance)
{
	struct WD33C93_hostdata *hostdata =
	    (struct WD33C93_hostdata *) instance->hostdata;
	const wd33c93_regs regs = hostdata->regs;
	struct scsi_cmnd *cmd, *prev;

	DB(DB_EXECUTE, printk("EX("))
	if (hostdata->selecting || hostdata->connected) {
		DB(DB_EXECUTE, printk(")EX-0 "))
		return;
	}


	cmd = (struct scsi_cmnd *) hostdata->input_Q;
	prev = NULL;
	while (cmd) {
		if (!(hostdata->busy[cmd->device->id] & (1 << cmd->device->lun)))
			break;
		prev = cmd;
		cmd = (struct scsi_cmnd *) cmd->host_scribble;
	}

	

	if (!cmd) {
		DB(DB_EXECUTE, printk(")EX-1 "))
		return;
	}

	

	if (prev)
		prev->host_scribble = cmd->host_scribble;
	else
		hostdata->input_Q = (struct scsi_cmnd *) cmd->host_scribble;

#ifdef PROC_STATISTICS
	hostdata->cmd_cnt[cmd->device->id]++;
#endif


	if (cmd->sc_data_direction == DMA_TO_DEVICE)
		write_wd33c93(regs, WD_DESTINATION_ID, cmd->device->id);
	else
		write_wd33c93(regs, WD_DESTINATION_ID, cmd->device->id | DSTID_DPD);


	cmd->SCp.phase = 0;	
	if (hostdata->disconnect == DIS_NEVER)
		goto no;
	if (hostdata->disconnect == DIS_ALWAYS)
		goto yes;
	if (cmd->device->type == 1)	
		goto yes;
	if (hostdata->disconnected_Q)	
		goto yes;
	if (!(hostdata->input_Q))	
		goto no;
	for (prev = (struct scsi_cmnd *) hostdata->input_Q; prev;
	     prev = (struct scsi_cmnd *) prev->host_scribble) {
		if ((prev->device->id != cmd->device->id) ||
		    (prev->device->lun != cmd->device->lun)) {
			for (prev = (struct scsi_cmnd *) hostdata->input_Q; prev;
			     prev = (struct scsi_cmnd *) prev->host_scribble)
				prev->SCp.phase = 1;
			goto yes;
		}
	}

	goto no;

 yes:
	cmd->SCp.phase = 1;

#ifdef PROC_STATISTICS
	hostdata->disc_allowed_cnt[cmd->device->id]++;
#endif

 no:

	write_wd33c93(regs, WD_SOURCE_ID, ((cmd->SCp.phase) ? SRCID_ER : 0));

	write_wd33c93(regs, WD_TARGET_LUN, cmd->device->lun);
	write_wd33c93(regs, WD_SYNCHRONOUS_TRANSFER,
		      hostdata->sync_xfer[cmd->device->id]);
	hostdata->busy[cmd->device->id] |= (1 << cmd->device->lun);

	if ((hostdata->level2 == L2_NONE) ||
	    (hostdata->sync_stat[cmd->device->id] == SS_UNSET)) {


		hostdata->selecting = cmd;

		if (hostdata->sync_stat[cmd->device->id] == SS_UNSET)
			hostdata->sync_stat[cmd->device->id] = SS_FIRST;
		hostdata->state = S_SELECTING;
		write_wd33c93_count(regs, 0);	
		write_wd33c93_cmd(regs, WD_CMD_SEL_ATN);
	} else {


		hostdata->connected = cmd;
		write_wd33c93(regs, WD_COMMAND_PHASE, 0);


		write_wd33c93_cdb(regs, cmd->cmd_len, cmd->cmnd);


		write_wd33c93(regs, WD_OWN_ID, cmd->cmd_len);


		if ((cmd->SCp.phase == 0) && (hostdata->no_dma == 0)) {
			if (hostdata->dma_setup(cmd,
			    (cmd->sc_data_direction == DMA_TO_DEVICE) ?
			     DATA_OUT_DIR : DATA_IN_DIR))
				write_wd33c93_count(regs, 0);	
			else {
				write_wd33c93_count(regs,
						    cmd->SCp.this_residual);
				write_wd33c93(regs, WD_CONTROL,
					      CTRL_IDI | CTRL_EDI | hostdata->dma_mode);
				hostdata->dma = D_DMA_RUNNING;
			}
		} else
			write_wd33c93_count(regs, 0);	

		hostdata->state = S_RUNNING_LEVEL2;
		write_wd33c93_cmd(regs, WD_CMD_SEL_ATN_XFER);
	}


	DB(DB_EXECUTE,
	   printk("%s)EX-2 ", (cmd->SCp.phase) ? "d:" : ""))
}

static void
transfer_pio(const wd33c93_regs regs, uchar * buf, int cnt,
	     int data_in_dir, struct WD33C93_hostdata *hostdata)
{
	uchar asr;

	DB(DB_TRANSFER,
	   printk("(%p,%d,%s:", buf, cnt, data_in_dir ? "in" : "out"))

	write_wd33c93(regs, WD_CONTROL, CTRL_IDI | CTRL_EDI | CTRL_POLLED);
	write_wd33c93_count(regs, cnt);
	write_wd33c93_cmd(regs, WD_CMD_TRANS_INFO);
	if (data_in_dir) {
		do {
			asr = read_aux_stat(regs);
			if (asr & ASR_DBR)
				*buf++ = read_wd33c93(regs, WD_DATA);
		} while (!(asr & ASR_INT));
	} else {
		do {
			asr = read_aux_stat(regs);
			if (asr & ASR_DBR)
				write_wd33c93(regs, WD_DATA, *buf++);
		} while (!(asr & ASR_INT));
	}


}

static void
transfer_bytes(const wd33c93_regs regs, struct scsi_cmnd *cmd,
		int data_in_dir)
{
	struct WD33C93_hostdata *hostdata;
	unsigned long length;

	hostdata = (struct WD33C93_hostdata *) cmd->device->host->hostdata;

	if (!cmd->SCp.this_residual && cmd->SCp.buffers_residual) {
		++cmd->SCp.buffer;
		--cmd->SCp.buffers_residual;
		cmd->SCp.this_residual = cmd->SCp.buffer->length;
		cmd->SCp.ptr = sg_virt(cmd->SCp.buffer);
	}
	if (!cmd->SCp.this_residual) 
		return;

	write_wd33c93(regs, WD_SYNCHRONOUS_TRANSFER,
		      hostdata->sync_xfer[cmd->device->id]);


	if (hostdata->no_dma || hostdata->dma_setup(cmd, data_in_dir)) {
#ifdef PROC_STATISTICS
		hostdata->pio_cnt++;
#endif
		transfer_pio(regs, (uchar *) cmd->SCp.ptr,
			     cmd->SCp.this_residual, data_in_dir, hostdata);
		length = cmd->SCp.this_residual;
		cmd->SCp.this_residual = read_wd33c93_count(regs);
		cmd->SCp.ptr += (length - cmd->SCp.this_residual);
	}


	else {
#ifdef PROC_STATISTICS
		hostdata->dma_cnt++;
#endif
		write_wd33c93(regs, WD_CONTROL, CTRL_IDI | CTRL_EDI | hostdata->dma_mode);
		write_wd33c93_count(regs, cmd->SCp.this_residual);

		if ((hostdata->level2 >= L2_DATA) ||
		    (hostdata->level2 == L2_BASIC && cmd->SCp.phase == 0)) {
			write_wd33c93(regs, WD_COMMAND_PHASE, 0x45);
			write_wd33c93_cmd(regs, WD_CMD_SEL_ATN_XFER);
			hostdata->state = S_RUNNING_LEVEL2;
		} else
			write_wd33c93_cmd(regs, WD_CMD_TRANS_INFO);

		hostdata->dma = D_DMA_RUNNING;
	}
}

void
wd33c93_intr(struct Scsi_Host *instance)
{
	struct WD33C93_hostdata *hostdata =
	    (struct WD33C93_hostdata *) instance->hostdata;
	const wd33c93_regs regs = hostdata->regs;
	struct scsi_cmnd *patch, *cmd;
	uchar asr, sr, phs, id, lun, *ucp, msg;
	unsigned long length, flags;

	asr = read_aux_stat(regs);
	if (!(asr & ASR_INT) || (asr & ASR_BSY))
		return;

	spin_lock_irqsave(&hostdata->lock, flags);

#ifdef PROC_STATISTICS
	hostdata->int_cnt++;
#endif

	cmd = (struct scsi_cmnd *) hostdata->connected;	
	sr = read_wd33c93(regs, WD_SCSI_STATUS);	
	phs = read_wd33c93(regs, WD_COMMAND_PHASE);

	DB(DB_INTR, printk("{%02x:%02x-", asr, sr))

	    if (hostdata->dma == D_DMA_RUNNING) {
		DB(DB_TRANSFER,
		   printk("[%p/%d:", cmd->SCp.ptr, cmd->SCp.this_residual))
		    hostdata->dma_stop(cmd->device->host, cmd, 1);
		hostdata->dma = D_DMA_OFF;
		length = cmd->SCp.this_residual;
		cmd->SCp.this_residual = read_wd33c93_count(regs);
		cmd->SCp.ptr += (length - cmd->SCp.this_residual);
		DB(DB_TRANSFER,
		   printk("%p/%d]", cmd->SCp.ptr, cmd->SCp.this_residual))
	}

	switch (sr) {
	case CSR_TIMEOUT:
		DB(DB_INTR, printk("TIMEOUT"))

		    if (hostdata->state == S_RUNNING_LEVEL2)
			hostdata->connected = NULL;
		else {
			cmd = (struct scsi_cmnd *) hostdata->selecting;	
			hostdata->selecting = NULL;
		}

		cmd->result = DID_NO_CONNECT << 16;
		hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
		hostdata->state = S_UNCONNECTED;
		cmd->scsi_done(cmd);


		spin_unlock_irqrestore(&hostdata->lock, flags);


		wd33c93_execute(instance);
		break;


	case CSR_SELECT:
		DB(DB_INTR, printk("SELECT"))
		    hostdata->connected = cmd =
		    (struct scsi_cmnd *) hostdata->selecting;
		hostdata->selecting = NULL;

		

		hostdata->outgoing_msg[0] = (0x80 | 0x00 | cmd->device->lun);
		if (cmd->SCp.phase)
			hostdata->outgoing_msg[0] |= 0x40;

		if (hostdata->sync_stat[cmd->device->id] == SS_FIRST) {

			hostdata->sync_stat[cmd->device->id] = SS_WAITING;


			hostdata->outgoing_msg[1] = EXTENDED_MESSAGE;
			hostdata->outgoing_msg[2] = 3;
			hostdata->outgoing_msg[3] = EXTENDED_SDTR;
			if (hostdata->no_sync & (1 << cmd->device->id)) {
				calc_sync_msg(hostdata->default_sx_per, 0,
						0, hostdata->outgoing_msg + 4);
			} else {
				calc_sync_msg(optimum_sx_per(hostdata),
						OPTIMUM_SX_OFF,
						hostdata->fast,
						hostdata->outgoing_msg + 4);
			}
			hostdata->outgoing_len = 6;
#ifdef SYNC_DEBUG
			ucp = hostdata->outgoing_msg + 1;
			printk(" sending SDTR %02x03%02x%02x%02x ",
				ucp[0], ucp[2], ucp[3], ucp[4]);
#endif
		} else
			hostdata->outgoing_len = 1;

		hostdata->state = S_CONNECTED;
		spin_unlock_irqrestore(&hostdata->lock, flags);
		break;

	case CSR_XFER_DONE | PHS_DATA_IN:
	case CSR_UNEXP | PHS_DATA_IN:
	case CSR_SRV_REQ | PHS_DATA_IN:
		DB(DB_INTR,
		   printk("IN-%d.%d", cmd->SCp.this_residual,
			  cmd->SCp.buffers_residual))
		    transfer_bytes(regs, cmd, DATA_IN_DIR);
		if (hostdata->state != S_RUNNING_LEVEL2)
			hostdata->state = S_CONNECTED;
		spin_unlock_irqrestore(&hostdata->lock, flags);
		break;

	case CSR_XFER_DONE | PHS_DATA_OUT:
	case CSR_UNEXP | PHS_DATA_OUT:
	case CSR_SRV_REQ | PHS_DATA_OUT:
		DB(DB_INTR,
		   printk("OUT-%d.%d", cmd->SCp.this_residual,
			  cmd->SCp.buffers_residual))
		    transfer_bytes(regs, cmd, DATA_OUT_DIR);
		if (hostdata->state != S_RUNNING_LEVEL2)
			hostdata->state = S_CONNECTED;
		spin_unlock_irqrestore(&hostdata->lock, flags);
		break;


	case CSR_XFER_DONE | PHS_COMMAND:
	case CSR_UNEXP | PHS_COMMAND:
	case CSR_SRV_REQ | PHS_COMMAND:
		DB(DB_INTR, printk("CMND-%02x", cmd->cmnd[0]))
		    transfer_pio(regs, cmd->cmnd, cmd->cmd_len, DATA_OUT_DIR,
				 hostdata);
		hostdata->state = S_CONNECTED;
		spin_unlock_irqrestore(&hostdata->lock, flags);
		break;

	case CSR_XFER_DONE | PHS_STATUS:
	case CSR_UNEXP | PHS_STATUS:
	case CSR_SRV_REQ | PHS_STATUS:
		DB(DB_INTR, printk("STATUS="))
		cmd->SCp.Status = read_1_byte(regs);
		DB(DB_INTR, printk("%02x", cmd->SCp.Status))
		    if (hostdata->level2 >= L2_BASIC) {
			sr = read_wd33c93(regs, WD_SCSI_STATUS);	
			udelay(7);
			hostdata->state = S_RUNNING_LEVEL2;
			write_wd33c93(regs, WD_COMMAND_PHASE, 0x50);
			write_wd33c93_cmd(regs, WD_CMD_SEL_ATN_XFER);
		} else {
			hostdata->state = S_CONNECTED;
		}
		spin_unlock_irqrestore(&hostdata->lock, flags);
		break;

	case CSR_XFER_DONE | PHS_MESS_IN:
	case CSR_UNEXP | PHS_MESS_IN:
	case CSR_SRV_REQ | PHS_MESS_IN:
		DB(DB_INTR, printk("MSG_IN="))

		msg = read_1_byte(regs);
		sr = read_wd33c93(regs, WD_SCSI_STATUS);	
		udelay(7);

		hostdata->incoming_msg[hostdata->incoming_ptr] = msg;
		if (hostdata->incoming_msg[0] == EXTENDED_MESSAGE)
			msg = EXTENDED_MESSAGE;
		else
			hostdata->incoming_ptr = 0;

		cmd->SCp.Message = msg;
		switch (msg) {

		case COMMAND_COMPLETE:
			DB(DB_INTR, printk("CCMP"))
			    write_wd33c93_cmd(regs, WD_CMD_NEGATE_ACK);
			hostdata->state = S_PRE_CMP_DISC;
			break;

		case SAVE_POINTERS:
			DB(DB_INTR, printk("SDP"))
			    write_wd33c93_cmd(regs, WD_CMD_NEGATE_ACK);
			hostdata->state = S_CONNECTED;
			break;

		case RESTORE_POINTERS:
			DB(DB_INTR, printk("RDP"))
			    if (hostdata->level2 >= L2_BASIC) {
				write_wd33c93(regs, WD_COMMAND_PHASE, 0x45);
				write_wd33c93_cmd(regs, WD_CMD_SEL_ATN_XFER);
				hostdata->state = S_RUNNING_LEVEL2;
			} else {
				write_wd33c93_cmd(regs, WD_CMD_NEGATE_ACK);
				hostdata->state = S_CONNECTED;
			}
			break;

		case DISCONNECT:
			DB(DB_INTR, printk("DIS"))
			    cmd->device->disconnect = 1;
			write_wd33c93_cmd(regs, WD_CMD_NEGATE_ACK);
			hostdata->state = S_PRE_TMP_DISC;
			break;

		case MESSAGE_REJECT:
			DB(DB_INTR, printk("REJ"))
#ifdef SYNC_DEBUG
			    printk("-REJ-");
#endif
			if (hostdata->sync_stat[cmd->device->id] == SS_WAITING) {
				hostdata->sync_stat[cmd->device->id] = SS_SET;
				
				hostdata->sync_xfer[cmd->device->id] =
					calc_sync_xfer(hostdata->default_sx_per
						/ 4, 0, 0, hostdata->sx_table);
			}
			write_wd33c93_cmd(regs, WD_CMD_NEGATE_ACK);
			hostdata->state = S_CONNECTED;
			break;

		case EXTENDED_MESSAGE:
			DB(DB_INTR, printk("EXT"))

			    ucp = hostdata->incoming_msg;

#ifdef SYNC_DEBUG
			printk("%02x", ucp[hostdata->incoming_ptr]);
#endif
			

			if ((hostdata->incoming_ptr >= 2) &&
			    (hostdata->incoming_ptr == (ucp[1] + 1))) {

				switch (ucp[2]) {	
				case EXTENDED_SDTR:
					
					id = calc_sync_xfer(hostdata->
							default_sx_per / 4, 0,
							0, hostdata->sx_table);
					if (hostdata->sync_stat[cmd->device->id] !=
					    SS_WAITING) {


						write_wd33c93_cmd(regs, WD_CMD_ASSERT_ATN);	
						hostdata->outgoing_msg[0] =
						    EXTENDED_MESSAGE;
						hostdata->outgoing_msg[1] = 3;
						hostdata->outgoing_msg[2] =
						    EXTENDED_SDTR;
						calc_sync_msg(hostdata->
							default_sx_per, 0,
							0, hostdata->outgoing_msg + 3);
						hostdata->outgoing_len = 5;
					} else {
						if (ucp[4]) 
							id = calc_sync_xfer(ucp[3], ucp[4],
									hostdata->fast,
									hostdata->sx_table);
						else if (ucp[3]) 
							id = calc_sync_xfer(ucp[3], ucp[4],
									0, hostdata->sx_table);
					}
					hostdata->sync_xfer[cmd->device->id] = id;
#ifdef SYNC_DEBUG
					printk(" sync_xfer=%02x\n",
					       hostdata->sync_xfer[cmd->device->id]);
#endif
					hostdata->sync_stat[cmd->device->id] =
					    SS_SET;
					write_wd33c93_cmd(regs,
							  WD_CMD_NEGATE_ACK);
					hostdata->state = S_CONNECTED;
					break;
				case EXTENDED_WDTR:
					write_wd33c93_cmd(regs, WD_CMD_ASSERT_ATN);	
					printk("sending WDTR ");
					hostdata->outgoing_msg[0] =
					    EXTENDED_MESSAGE;
					hostdata->outgoing_msg[1] = 2;
					hostdata->outgoing_msg[2] =
					    EXTENDED_WDTR;
					hostdata->outgoing_msg[3] = 0;	
					hostdata->outgoing_len = 4;
					write_wd33c93_cmd(regs,
							  WD_CMD_NEGATE_ACK);
					hostdata->state = S_CONNECTED;
					break;
				default:
					write_wd33c93_cmd(regs, WD_CMD_ASSERT_ATN);	
					printk
					    ("Rejecting Unknown Extended Message(%02x). ",
					     ucp[2]);
					hostdata->outgoing_msg[0] =
					    MESSAGE_REJECT;
					hostdata->outgoing_len = 1;
					write_wd33c93_cmd(regs,
							  WD_CMD_NEGATE_ACK);
					hostdata->state = S_CONNECTED;
					break;
				}
				hostdata->incoming_ptr = 0;
			}

			

			else {
				hostdata->incoming_ptr++;
				write_wd33c93_cmd(regs, WD_CMD_NEGATE_ACK);
				hostdata->state = S_CONNECTED;
			}
			break;

		default:
			printk("Rejecting Unknown Message(%02x) ", msg);
			write_wd33c93_cmd(regs, WD_CMD_ASSERT_ATN);	
			hostdata->outgoing_msg[0] = MESSAGE_REJECT;
			hostdata->outgoing_len = 1;
			write_wd33c93_cmd(regs, WD_CMD_NEGATE_ACK);
			hostdata->state = S_CONNECTED;
		}
		spin_unlock_irqrestore(&hostdata->lock, flags);
		break;


	case CSR_SEL_XFER_DONE:


		write_wd33c93(regs, WD_SOURCE_ID, SRCID_ER);
		if (phs == 0x60) {
			DB(DB_INTR, printk("SX-DONE"))
			    cmd->SCp.Message = COMMAND_COMPLETE;
			lun = read_wd33c93(regs, WD_TARGET_LUN);
			DB(DB_INTR, printk(":%d.%d", cmd->SCp.Status, lun))
			    hostdata->connected = NULL;
			hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
			hostdata->state = S_UNCONNECTED;
			if (cmd->SCp.Status == ILLEGAL_STATUS_BYTE)
				cmd->SCp.Status = lun;
			if (cmd->cmnd[0] == REQUEST_SENSE
			    && cmd->SCp.Status != GOOD)
				cmd->result =
				    (cmd->
				     result & 0x00ffff) | (DID_ERROR << 16);
			else
				cmd->result =
				    cmd->SCp.Status | (cmd->SCp.Message << 8);
			cmd->scsi_done(cmd);

			spin_unlock_irqrestore(&hostdata->lock, flags);
			wd33c93_execute(instance);
		} else {
			printk
			    ("%02x:%02x:%02x: Unknown SEL_XFER_DONE phase!!---",
			     asr, sr, phs);
			spin_unlock_irqrestore(&hostdata->lock, flags);
		}
		break;


	case CSR_SDP:
		DB(DB_INTR, printk("SDP"))
		    hostdata->state = S_RUNNING_LEVEL2;
		write_wd33c93(regs, WD_COMMAND_PHASE, 0x41);
		write_wd33c93_cmd(regs, WD_CMD_SEL_ATN_XFER);
		spin_unlock_irqrestore(&hostdata->lock, flags);
		break;

	case CSR_XFER_DONE | PHS_MESS_OUT:
	case CSR_UNEXP | PHS_MESS_OUT:
	case CSR_SRV_REQ | PHS_MESS_OUT:
		DB(DB_INTR, printk("MSG_OUT="))

		    if (hostdata->outgoing_len == 0) {
			hostdata->outgoing_len = 1;
			hostdata->outgoing_msg[0] = NOP;
		}
		transfer_pio(regs, hostdata->outgoing_msg,
			     hostdata->outgoing_len, DATA_OUT_DIR, hostdata);
		DB(DB_INTR, printk("%02x", hostdata->outgoing_msg[0]))
		    hostdata->outgoing_len = 0;
		hostdata->state = S_CONNECTED;
		spin_unlock_irqrestore(&hostdata->lock, flags);
		break;

	case CSR_UNEXP_DISC:



		write_wd33c93(regs, WD_SOURCE_ID, SRCID_ER);
		if (cmd == NULL) {
			printk(" - Already disconnected! ");
			hostdata->state = S_UNCONNECTED;
			spin_unlock_irqrestore(&hostdata->lock, flags);
			return;
		}
		DB(DB_INTR, printk("UNEXP_DISC"))
		    hostdata->connected = NULL;
		hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
		hostdata->state = S_UNCONNECTED;
		if (cmd->cmnd[0] == REQUEST_SENSE && cmd->SCp.Status != GOOD)
			cmd->result =
			    (cmd->result & 0x00ffff) | (DID_ERROR << 16);
		else
			cmd->result = cmd->SCp.Status | (cmd->SCp.Message << 8);
		cmd->scsi_done(cmd);

		
		spin_unlock_irqrestore(&hostdata->lock, flags);
		wd33c93_execute(instance);
		break;

	case CSR_DISC:


		write_wd33c93(regs, WD_SOURCE_ID, SRCID_ER);
		DB(DB_INTR, printk("DISC"))
		    if (cmd == NULL) {
			printk(" - Already disconnected! ");
			hostdata->state = S_UNCONNECTED;
		}
		switch (hostdata->state) {
		case S_PRE_CMP_DISC:
			hostdata->connected = NULL;
			hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
			hostdata->state = S_UNCONNECTED;
			DB(DB_INTR, printk(":%d", cmd->SCp.Status))
			    if (cmd->cmnd[0] == REQUEST_SENSE
				&& cmd->SCp.Status != GOOD)
				cmd->result =
				    (cmd->
				     result & 0x00ffff) | (DID_ERROR << 16);
			else
				cmd->result =
				    cmd->SCp.Status | (cmd->SCp.Message << 8);
			cmd->scsi_done(cmd);
			break;
		case S_PRE_TMP_DISC:
		case S_RUNNING_LEVEL2:
			cmd->host_scribble = (uchar *) hostdata->disconnected_Q;
			hostdata->disconnected_Q = cmd;
			hostdata->connected = NULL;
			hostdata->state = S_UNCONNECTED;

#ifdef PROC_STATISTICS
			hostdata->disc_done_cnt[cmd->device->id]++;
#endif

			break;
		default:
			printk("*** Unexpected DISCONNECT interrupt! ***");
			hostdata->state = S_UNCONNECTED;
		}

		spin_unlock_irqrestore(&hostdata->lock, flags);
		wd33c93_execute(instance);
		break;

	case CSR_RESEL_AM:
	case CSR_RESEL:
		DB(DB_INTR, printk("RESEL%s", sr == CSR_RESEL_AM ? "_AM" : ""))

		    if (hostdata->level2 <= L2_NONE) {

			if (hostdata->selecting) {
				cmd = (struct scsi_cmnd *) hostdata->selecting;
				hostdata->selecting = NULL;
				hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
				cmd->host_scribble =
				    (uchar *) hostdata->input_Q;
				hostdata->input_Q = cmd;
			}
		}

		else {

			if (cmd) {
				if (phs == 0x00) {
					hostdata->busy[cmd->device->id] &=
					    ~(1 << cmd->device->lun);
					cmd->host_scribble =
					    (uchar *) hostdata->input_Q;
					hostdata->input_Q = cmd;
				} else {
					printk
					    ("---%02x:%02x:%02x-TROUBLE: Intrusive ReSelect!---",
					     asr, sr, phs);
					while (1)
						printk("\r");
				}
			}

		}

		

		id = read_wd33c93(regs, WD_SOURCE_ID);
		id &= SRCID_MASK;


		if (sr == CSR_RESEL_AM) {
			lun = read_wd33c93(regs, WD_DATA);
			if (hostdata->level2 < L2_RESELECT)
				write_wd33c93_cmd(regs, WD_CMD_NEGATE_ACK);
			lun &= 7;
		} else {
			
			for (lun = 255; lun; lun--) {
				if ((asr = read_aux_stat(regs)) & ASR_INT)
					break;
				udelay(10);
			}
			if (!(asr & ASR_INT)) {
				printk
				    ("wd33c93: Reselected without IDENTIFY\n");
				lun = 0;
			} else {
				
				sr = read_wd33c93(regs, WD_SCSI_STATUS);
				udelay(7);
				if (sr == (CSR_ABORT | PHS_MESS_IN) ||
				    sr == (CSR_UNEXP | PHS_MESS_IN) ||
				    sr == (CSR_SRV_REQ | PHS_MESS_IN)) {
					
					lun = read_1_byte(regs);
					
					asr = read_aux_stat(regs);
					if (!(asr & ASR_INT)) {
						udelay(10);
						asr = read_aux_stat(regs);
						if (!(asr & ASR_INT))
							printk
							    ("wd33c93: No int after LUN on RESEL (%02x)\n",
							     asr);
					}
					sr = read_wd33c93(regs, WD_SCSI_STATUS);
					udelay(7);
					if (sr != CSR_MSGIN)
						printk
						    ("wd33c93: Not paused with ACK on RESEL (%02x)\n",
						     sr);
					lun &= 7;
					write_wd33c93_cmd(regs,
							  WD_CMD_NEGATE_ACK);
				} else {
					printk
					    ("wd33c93: Not MSG_IN on reselect (%02x)\n",
					     sr);
					lun = 0;
				}
			}
		}

		

		cmd = (struct scsi_cmnd *) hostdata->disconnected_Q;
		patch = NULL;
		while (cmd) {
			if (id == cmd->device->id && lun == cmd->device->lun)
				break;
			patch = cmd;
			cmd = (struct scsi_cmnd *) cmd->host_scribble;
		}

		

		if (!cmd) {
			printk
			    ("---TROUBLE: target %d.%d not in disconnect queue---",
			     id, lun);
			spin_unlock_irqrestore(&hostdata->lock, flags);
			return;
		}

		

		if (patch)
			patch->host_scribble = cmd->host_scribble;
		else
			hostdata->disconnected_Q =
			    (struct scsi_cmnd *) cmd->host_scribble;
		hostdata->connected = cmd;


		if (cmd->sc_data_direction == DMA_TO_DEVICE)
			write_wd33c93(regs, WD_DESTINATION_ID, cmd->device->id);
		else
			write_wd33c93(regs, WD_DESTINATION_ID,
				      cmd->device->id | DSTID_DPD);
		if (hostdata->level2 >= L2_RESELECT) {
			write_wd33c93_count(regs, 0);	
			write_wd33c93(regs, WD_COMMAND_PHASE, 0x45);
			write_wd33c93_cmd(regs, WD_CMD_SEL_ATN_XFER);
			hostdata->state = S_RUNNING_LEVEL2;
		} else
			hostdata->state = S_CONNECTED;

		    spin_unlock_irqrestore(&hostdata->lock, flags);
		break;

	default:
		printk("--UNKNOWN INTERRUPT:%02x:%02x:%02x--", asr, sr, phs);
		spin_unlock_irqrestore(&hostdata->lock, flags);
	}

	DB(DB_INTR, printk("} "))

}

static void
reset_wd33c93(struct Scsi_Host *instance)
{
	struct WD33C93_hostdata *hostdata =
	    (struct WD33C93_hostdata *) instance->hostdata;
	const wd33c93_regs regs = hostdata->regs;
	uchar sr;

#ifdef CONFIG_SGI_IP22
	{
		int busycount = 0;
		extern void sgiwd93_reset(unsigned long);
		
		while ((read_aux_stat(regs) & ASR_BSY) && busycount++ < 100)
			udelay (10);
	
	if (read_aux_stat(regs) & ASR_BSY)
		sgiwd93_reset(instance->base); 
	}
#endif

	write_wd33c93(regs, WD_OWN_ID, OWNID_EAF | OWNID_RAF |
		      instance->this_id | hostdata->clock_freq);
	write_wd33c93(regs, WD_CONTROL, CTRL_IDI | CTRL_EDI | CTRL_POLLED);
	write_wd33c93(regs, WD_SYNCHRONOUS_TRANSFER,
		      calc_sync_xfer(hostdata->default_sx_per / 4,
				     DEFAULT_SX_OFF, 0, hostdata->sx_table));
	write_wd33c93(regs, WD_COMMAND, WD_CMD_RESET);


#ifdef CONFIG_MVME147_SCSI
	udelay(25);		
#endif

	while (!(read_aux_stat(regs) & ASR_INT))
		;
	sr = read_wd33c93(regs, WD_SCSI_STATUS);

	hostdata->microcode = read_wd33c93(regs, WD_CDB_1);
	if (sr == 0x00)
		hostdata->chip = C_WD33C93;
	else if (sr == 0x01) {
		write_wd33c93(regs, WD_QUEUE_TAG, 0xa5);	
		sr = read_wd33c93(regs, WD_QUEUE_TAG);
		if (sr == 0xa5) {
			hostdata->chip = C_WD33C93B;
			write_wd33c93(regs, WD_QUEUE_TAG, 0);
		} else
			hostdata->chip = C_WD33C93A;
	} else
		hostdata->chip = C_UNKNOWN_CHIP;

	if (hostdata->chip != C_WD33C93B)	
		hostdata->fast = 0;

	write_wd33c93(regs, WD_TIMEOUT_PERIOD, TIMEOUT_PERIOD_VALUE);
	write_wd33c93(regs, WD_CONTROL, CTRL_IDI | CTRL_EDI | CTRL_POLLED);
}

int
wd33c93_host_reset(struct scsi_cmnd * SCpnt)
{
	struct Scsi_Host *instance;
	struct WD33C93_hostdata *hostdata;
	int i;

	instance = SCpnt->device->host;
	hostdata = (struct WD33C93_hostdata *) instance->hostdata;

	printk("scsi%d: reset. ", instance->host_no);
	disable_irq(instance->irq);

	hostdata->dma_stop(instance, NULL, 0);
	for (i = 0; i < 8; i++) {
		hostdata->busy[i] = 0;
		hostdata->sync_xfer[i] =
			calc_sync_xfer(DEFAULT_SX_PER / 4, DEFAULT_SX_OFF,
					0, hostdata->sx_table);
		hostdata->sync_stat[i] = SS_UNSET;	
	}
	hostdata->input_Q = NULL;
	hostdata->selecting = NULL;
	hostdata->connected = NULL;
	hostdata->disconnected_Q = NULL;
	hostdata->state = S_UNCONNECTED;
	hostdata->dma = D_DMA_OFF;
	hostdata->incoming_ptr = 0;
	hostdata->outgoing_len = 0;

	reset_wd33c93(instance);
	SCpnt->result = DID_RESET << 16;
	enable_irq(instance->irq);
	return SUCCESS;
}

int
wd33c93_abort(struct scsi_cmnd * cmd)
{
	struct Scsi_Host *instance;
	struct WD33C93_hostdata *hostdata;
	wd33c93_regs regs;
	struct scsi_cmnd *tmp, *prev;

	disable_irq(cmd->device->host->irq);

	instance = cmd->device->host;
	hostdata = (struct WD33C93_hostdata *) instance->hostdata;
	regs = hostdata->regs;


	tmp = (struct scsi_cmnd *) hostdata->input_Q;
	prev = NULL;
	while (tmp) {
		if (tmp == cmd) {
			if (prev)
				prev->host_scribble = cmd->host_scribble;
			else
				hostdata->input_Q =
				    (struct scsi_cmnd *) cmd->host_scribble;
			cmd->host_scribble = NULL;
			cmd->result = DID_ABORT << 16;
			printk
			    ("scsi%d: Abort - removing command from input_Q. ",
			     instance->host_no);
			enable_irq(cmd->device->host->irq);
			cmd->scsi_done(cmd);
			return SUCCESS;
		}
		prev = tmp;
		tmp = (struct scsi_cmnd *) tmp->host_scribble;
	}


	if (hostdata->connected == cmd) {
		uchar sr, asr;
		unsigned long timeout;

		printk("scsi%d: Aborting connected command - ",
		       instance->host_no);

		printk("stopping DMA - ");
		if (hostdata->dma == D_DMA_RUNNING) {
			hostdata->dma_stop(instance, cmd, 0);
			hostdata->dma = D_DMA_OFF;
		}

		printk("sending wd33c93 ABORT command - ");
		write_wd33c93(regs, WD_CONTROL,
			      CTRL_IDI | CTRL_EDI | CTRL_POLLED);
		write_wd33c93_cmd(regs, WD_CMD_ABORT);


		printk("flushing fifo - ");
		timeout = 1000000;
		do {
			asr = read_aux_stat(regs);
			if (asr & ASR_DBR)
				read_wd33c93(regs, WD_DATA);
		} while (!(asr & ASR_INT) && timeout-- > 0);
		sr = read_wd33c93(regs, WD_SCSI_STATUS);
		printk
		    ("asr=%02x, sr=%02x, %ld bytes un-transferred (timeout=%ld) - ",
		     asr, sr, read_wd33c93_count(regs), timeout);


		printk("sending wd33c93 DISCONNECT command - ");
		write_wd33c93_cmd(regs, WD_CMD_DISCONNECT);

		timeout = 1000000;
		asr = read_aux_stat(regs);
		while ((asr & ASR_CIP) && timeout-- > 0)
			asr = read_aux_stat(regs);
		sr = read_wd33c93(regs, WD_SCSI_STATUS);
		printk("asr=%02x, sr=%02x.", asr, sr);

		hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
		hostdata->connected = NULL;
		hostdata->state = S_UNCONNECTED;
		cmd->result = DID_ABORT << 16;

		wd33c93_execute(instance);

		enable_irq(cmd->device->host->irq);
		cmd->scsi_done(cmd);
		return SUCCESS;
	}


	tmp = (struct scsi_cmnd *) hostdata->disconnected_Q;
	while (tmp) {
		if (tmp == cmd) {
			printk
			    ("scsi%d: Abort - command found on disconnected_Q - ",
			     instance->host_no);
			printk("Abort SNOOZE. ");
			enable_irq(cmd->device->host->irq);
			return FAILED;
		}
		tmp = (struct scsi_cmnd *) tmp->host_scribble;
	}


	wd33c93_execute(instance);

	enable_irq(cmd->device->host->irq);
	printk("scsi%d: warning : SCSI command probably completed successfully"
	       "         before abortion. ", instance->host_no);
	return FAILED;
}

#define MAX_WD33C93_HOSTS 4
#define MAX_SETUP_ARGS ARRAY_SIZE(setup_args)
#define SETUP_BUFFER_SIZE 200
static char setup_buffer[SETUP_BUFFER_SIZE];
static char setup_used[MAX_SETUP_ARGS];
static int done_setup = 0;

static int
wd33c93_setup(char *str)
{
	int i;
	char *p1, *p2;


	p1 = setup_buffer;
	*p1 = '\0';
	if (str)
		strncpy(p1, str, SETUP_BUFFER_SIZE - strlen(setup_buffer));
	setup_buffer[SETUP_BUFFER_SIZE - 1] = '\0';
	p1 = setup_buffer;
	i = 0;
	while (*p1 && (i < MAX_SETUP_ARGS)) {
		p2 = strchr(p1, ',');
		if (p2) {
			*p2 = '\0';
			if (p1 != p2)
				setup_args[i] = p1;
			p1 = p2 + 1;
			i++;
		} else {
			setup_args[i] = p1;
			break;
		}
	}
	for (i = 0; i < MAX_SETUP_ARGS; i++)
		setup_used[i] = 0;
	done_setup = 1;

	return 1;
}
__setup("wd33c93=", wd33c93_setup);

static int
check_setup_args(char *key, int *flags, int *val, char *buf)
{
	int x;
	char *cp;

	for (x = 0; x < MAX_SETUP_ARGS; x++) {
		if (setup_used[x])
			continue;
		if (!strncmp(setup_args[x], key, strlen(key)))
			break;
		if (!strncmp(setup_args[x], "next", strlen("next")))
			return 0;
	}
	if (x == MAX_SETUP_ARGS)
		return 0;
	setup_used[x] = 1;
	cp = setup_args[x] + strlen(key);
	*val = -1;
	if (*cp != ':')
		return ++x;
	cp++;
	if ((*cp >= '0') && (*cp <= '9')) {
		*val = simple_strtoul(cp, NULL, 0);
	}
	return ++x;
}

static inline unsigned int
round_4(unsigned int x)
{
	switch (x & 3) {
		case 1: --x;
			break;
		case 2: ++x;
		case 3: ++x;
	}
	return x;
}

static void
calc_sx_table(unsigned int mhz, struct sx_period sx_table[9])
{
	unsigned int d, i;
	if (mhz < 11)
		d = 2;	
	else if (mhz < 16)
		d = 3;	
	else
		d = 4;	

	d = (100000 * d) / 2 / mhz; 

	sx_table[0].period_ns = 1;
	sx_table[0].reg_value = 0x20;
	for (i = 1; i < 8; i++) {
		sx_table[i].period_ns = round_4((i+1)*d / 100);
		sx_table[i].reg_value = (i+1)*0x10;
	}
	sx_table[7].reg_value = 0;
	sx_table[8].period_ns = 0;
	sx_table[8].reg_value = 0;
}

static uchar
set_clk_freq(int freq, int *mhz)
{
	int x = freq;
	if (WD33C93_FS_8_10 == freq)
		freq = 8;
	else if (WD33C93_FS_12_15 == freq)
		freq = 12;
	else if (WD33C93_FS_16_20 == freq)
		freq = 16;
	else if (freq > 7 && freq < 11)
		x = WD33C93_FS_8_10;
		else if (freq > 11 && freq < 16)
		x = WD33C93_FS_12_15;
		else if (freq > 15 && freq < 21)
		x = WD33C93_FS_16_20;
	else {
			
		x = WD33C93_FS_8_10;
		freq = 8;
	}
	*mhz = freq;
	return x;
}

static inline void set_resync ( struct WD33C93_hostdata *hd, int mask )
{
	int i;
	for (i = 0; i < 8; i++)
		if (mask & (1 << i))
			hd->sync_stat[i] = SS_UNSET;
}

void
wd33c93_init(struct Scsi_Host *instance, const wd33c93_regs regs,
	     dma_setup_t setup, dma_stop_t stop, int clock_freq)
{
	struct WD33C93_hostdata *hostdata;
	int i;
	int flags;
	int val;
	char buf[32];

	if (!done_setup && setup_strings)
		wd33c93_setup(setup_strings);

	hostdata = (struct WD33C93_hostdata *) instance->hostdata;

	hostdata->regs = regs;
	hostdata->clock_freq = set_clk_freq(clock_freq, &i);
	calc_sx_table(i, hostdata->sx_table);
	hostdata->dma_setup = setup;
	hostdata->dma_stop = stop;
	hostdata->dma_bounce_buffer = NULL;
	hostdata->dma_bounce_len = 0;
	for (i = 0; i < 8; i++) {
		hostdata->busy[i] = 0;
		hostdata->sync_xfer[i] =
			calc_sync_xfer(DEFAULT_SX_PER / 4, DEFAULT_SX_OFF,
					0, hostdata->sx_table);
		hostdata->sync_stat[i] = SS_UNSET;	
#ifdef PROC_STATISTICS
		hostdata->cmd_cnt[i] = 0;
		hostdata->disc_allowed_cnt[i] = 0;
		hostdata->disc_done_cnt[i] = 0;
#endif
	}
	hostdata->input_Q = NULL;
	hostdata->selecting = NULL;
	hostdata->connected = NULL;
	hostdata->disconnected_Q = NULL;
	hostdata->state = S_UNCONNECTED;
	hostdata->dma = D_DMA_OFF;
	hostdata->level2 = L2_BASIC;
	hostdata->disconnect = DIS_ADAPTIVE;
	hostdata->args = DEBUG_DEFAULTS;
	hostdata->incoming_ptr = 0;
	hostdata->outgoing_len = 0;
	hostdata->default_sx_per = DEFAULT_SX_PER;
	hostdata->no_dma = 0;	

#ifdef PROC_INTERFACE
	hostdata->proc = PR_VERSION | PR_INFO | PR_STATISTICS |
	    PR_CONNECTED | PR_INPUTQ | PR_DISCQ | PR_STOP;
#ifdef PROC_STATISTICS
	hostdata->dma_cnt = 0;
	hostdata->pio_cnt = 0;
	hostdata->int_cnt = 0;
#endif
#endif

	if (check_setup_args("clock", &flags, &val, buf)) {
		hostdata->clock_freq = set_clk_freq(val, &val);
		calc_sx_table(val, hostdata->sx_table);
	}

	if (check_setup_args("nosync", &flags, &val, buf))
		hostdata->no_sync = val;

	if (check_setup_args("nodma", &flags, &val, buf))
		hostdata->no_dma = (val == -1) ? 1 : val;

	if (check_setup_args("period", &flags, &val, buf))
		hostdata->default_sx_per =
		    hostdata->sx_table[round_period((unsigned int) val,
		                                    hostdata->sx_table)].period_ns;

	if (check_setup_args("disconnect", &flags, &val, buf)) {
		if ((val >= DIS_NEVER) && (val <= DIS_ALWAYS))
			hostdata->disconnect = val;
		else
			hostdata->disconnect = DIS_ADAPTIVE;
	}

	if (check_setup_args("level2", &flags, &val, buf))
		hostdata->level2 = val;

	if (check_setup_args("debug", &flags, &val, buf))
		hostdata->args = val & DB_MASK;

	if (check_setup_args("burst", &flags, &val, buf))
		hostdata->dma_mode = val ? CTRL_BURST:CTRL_DMA;

	if (WD33C93_FS_16_20 == hostdata->clock_freq 
		&& check_setup_args("fast", &flags, &val, buf))
		hostdata->fast = !!val;

	if ((i = check_setup_args("next", &flags, &val, buf))) {
		while (i)
			setup_used[--i] = 1;
	}
#ifdef PROC_INTERFACE
	if (check_setup_args("proc", &flags, &val, buf))
		hostdata->proc = val;
#endif

	spin_lock_irq(&hostdata->lock);
	reset_wd33c93(instance);
	spin_unlock_irq(&hostdata->lock);

	printk("wd33c93-%d: chip=%s/%d no_sync=0x%x no_dma=%d",
	       instance->host_no,
	       (hostdata->chip == C_WD33C93) ? "WD33c93" : (hostdata->chip ==
							    C_WD33C93A) ?
	       "WD33c93A" : (hostdata->chip ==
			     C_WD33C93B) ? "WD33c93B" : "unknown",
	       hostdata->microcode, hostdata->no_sync, hostdata->no_dma);
#ifdef DEBUGGING_ON
	printk(" debug_flags=0x%02x\n", hostdata->args);
#else
	printk(" debugging=OFF\n");
#endif
	printk("           setup_args=");
	for (i = 0; i < MAX_SETUP_ARGS; i++)
		printk("%s,", setup_args[i]);
	printk("\n");
	printk("           Version %s - %s\n", WD33C93_VERSION, WD33C93_DATE);
}

int
wd33c93_proc_info(struct Scsi_Host *instance, char *buf, char **start, off_t off, int len, int in)
{

#ifdef PROC_INTERFACE

	char *bp;
	char tbuf[128];
	struct WD33C93_hostdata *hd;
	struct scsi_cmnd *cmd;
	int x;
	static int stop = 0;

	hd = (struct WD33C93_hostdata *) instance->hostdata;


	if (in) {
		buf[len] = '\0';
		for (bp = buf; *bp; ) {
			while (',' == *bp || ' ' == *bp)
				++bp;
		if (!strncmp(bp, "debug:", 6)) {
				hd->args = simple_strtoul(bp+6, &bp, 0) & DB_MASK;
		} else if (!strncmp(bp, "disconnect:", 11)) {
				x = simple_strtoul(bp+11, &bp, 0);
			if (x < DIS_NEVER || x > DIS_ALWAYS)
				x = DIS_ADAPTIVE;
			hd->disconnect = x;
		} else if (!strncmp(bp, "period:", 7)) {
			x = simple_strtoul(bp+7, &bp, 0);
			hd->default_sx_per =
				hd->sx_table[round_period((unsigned int) x,
							  hd->sx_table)].period_ns;
		} else if (!strncmp(bp, "resync:", 7)) {
				set_resync(hd, (int)simple_strtoul(bp+7, &bp, 0));
		} else if (!strncmp(bp, "proc:", 5)) {
				hd->proc = simple_strtoul(bp+5, &bp, 0);
		} else if (!strncmp(bp, "nodma:", 6)) {
				hd->no_dma = simple_strtoul(bp+6, &bp, 0);
		} else if (!strncmp(bp, "level2:", 7)) {
				hd->level2 = simple_strtoul(bp+7, &bp, 0);
			} else if (!strncmp(bp, "burst:", 6)) {
				hd->dma_mode =
					simple_strtol(bp+6, &bp, 0) ? CTRL_BURST:CTRL_DMA;
			} else if (!strncmp(bp, "fast:", 5)) {
				x = !!simple_strtol(bp+5, &bp, 0);
				if (x != hd->fast)
					set_resync(hd, 0xff);
				hd->fast = x;
			} else if (!strncmp(bp, "nosync:", 7)) {
				x = simple_strtoul(bp+7, &bp, 0);
				set_resync(hd, x ^ hd->no_sync);
				hd->no_sync = x;
			} else {
				break; 
			}
		}
		return len;
	}

	spin_lock_irq(&hd->lock);
	bp = buf;
	*bp = '\0';
	if (hd->proc & PR_VERSION) {
		sprintf(tbuf, "\nVersion %s - %s.",
			WD33C93_VERSION, WD33C93_DATE);
		strcat(bp, tbuf);
	}
	if (hd->proc & PR_INFO) {
		sprintf(tbuf, "\nclock_freq=%02x no_sync=%02x no_dma=%d"
			" dma_mode=%02x fast=%d",
			hd->clock_freq, hd->no_sync, hd->no_dma, hd->dma_mode, hd->fast);
		strcat(bp, tbuf);
		strcat(bp, "\nsync_xfer[] =       ");
		for (x = 0; x < 7; x++) {
			sprintf(tbuf, "\t%02x", hd->sync_xfer[x]);
			strcat(bp, tbuf);
		}
		strcat(bp, "\nsync_stat[] =       ");
		for (x = 0; x < 7; x++) {
			sprintf(tbuf, "\t%02x", hd->sync_stat[x]);
			strcat(bp, tbuf);
		}
	}
#ifdef PROC_STATISTICS
	if (hd->proc & PR_STATISTICS) {
		strcat(bp, "\ncommands issued:    ");
		for (x = 0; x < 7; x++) {
			sprintf(tbuf, "\t%ld", hd->cmd_cnt[x]);
			strcat(bp, tbuf);
		}
		strcat(bp, "\ndisconnects allowed:");
		for (x = 0; x < 7; x++) {
			sprintf(tbuf, "\t%ld", hd->disc_allowed_cnt[x]);
			strcat(bp, tbuf);
		}
		strcat(bp, "\ndisconnects done:   ");
		for (x = 0; x < 7; x++) {
			sprintf(tbuf, "\t%ld", hd->disc_done_cnt[x]);
			strcat(bp, tbuf);
		}
		sprintf(tbuf,
			"\ninterrupts: %ld, DATA_PHASE ints: %ld DMA, %ld PIO",
			hd->int_cnt, hd->dma_cnt, hd->pio_cnt);
		strcat(bp, tbuf);
	}
#endif
	if (hd->proc & PR_CONNECTED) {
		strcat(bp, "\nconnected:     ");
		if (hd->connected) {
			cmd = (struct scsi_cmnd *) hd->connected;
			sprintf(tbuf, " %d:%d(%02x)",
				cmd->device->id, cmd->device->lun, cmd->cmnd[0]);
			strcat(bp, tbuf);
		}
	}
	if (hd->proc & PR_INPUTQ) {
		strcat(bp, "\ninput_Q:       ");
		cmd = (struct scsi_cmnd *) hd->input_Q;
		while (cmd) {
			sprintf(tbuf, " %d:%d(%02x)",
				cmd->device->id, cmd->device->lun, cmd->cmnd[0]);
			strcat(bp, tbuf);
			cmd = (struct scsi_cmnd *) cmd->host_scribble;
		}
	}
	if (hd->proc & PR_DISCQ) {
		strcat(bp, "\ndisconnected_Q:");
		cmd = (struct scsi_cmnd *) hd->disconnected_Q;
		while (cmd) {
			sprintf(tbuf, " %d:%d(%02x)",
				cmd->device->id, cmd->device->lun, cmd->cmnd[0]);
			strcat(bp, tbuf);
			cmd = (struct scsi_cmnd *) cmd->host_scribble;
		}
	}
	strcat(bp, "\n");
	spin_unlock_irq(&hd->lock);
	*start = buf;
	if (stop) {
		stop = 0;
		return 0;
	}
	if (off > 0x40000)	
		stop = 1;
	if (hd->proc & PR_STOP)	
		stop = 1;
	return strlen(bp);

#else				

	return 0;

#endif				

}

EXPORT_SYMBOL(wd33c93_host_reset);
EXPORT_SYMBOL(wd33c93_init);
EXPORT_SYMBOL(wd33c93_abort);
EXPORT_SYMBOL(wd33c93_queuecommand);
EXPORT_SYMBOL(wd33c93_intr);
EXPORT_SYMBOL(wd33c93_proc_info);
