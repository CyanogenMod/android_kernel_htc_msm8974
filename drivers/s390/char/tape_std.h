/*
 *  drivers/s390/char/tape_std.h
 *    standard tape device functions for ibm tapes.
 *
 *    Copyright (C) IBM Corp. 2001,2006
 *    Author(s): Carsten Otte <cotte@de.ibm.com>
 *		 Tuan Ngo-Anh <ngoanh@de.ibm.com>
 *		 Martin Schwidefsky <schwidefsky@de.ibm.com>
 */

#ifndef _TAPE_STD_H
#define _TAPE_STD_H

#include <asm/tape390.h>

#define MAX_BLOCKSIZE   65535

#define INVALID_00		0x00	
#define BACKSPACEBLOCK		0x27	
#define BACKSPACEFILE		0x2f	
#define DATA_SEC_ERASE		0x97	
#define ERASE_GAP		0x17	
#define FORSPACEBLOCK		0x37	
#define FORSPACEFILE		0x3F	
#define FORCE_STREAM_CNT	0xEB	
#define NOP			0x03	
#define READ_FORWARD		0x02	
#define REWIND			0x07	
#define REWIND_UNLOAD		0x0F	
#define SENSE			0x04	
#define NEW_MODE_SET		0xEB	
#define WRITE_CMD		0x01	
#define WRITETAPEMARK		0x1F	

#define ASSIGN			0xB7	
#define CONTROL_ACCESS		0xE3	
#define DIAG_MODE_SET		0x0B	
#define LOAD_DISPLAY		0x9F	
#define LOCATE			0x4F	
#define LOOP_WRITE_TO_READ	0x8B	
#define MODE_SET_DB		0xDB	
#define MODE_SET_C3		0xC3	
#define MODE_SET_CB		0xCB	
#define MODE_SET_D3		0xD3	
#define READ_BACKWARD		0x0C	
#define READ_BLOCK_ID		0x22	
#define READ_BUFFER		0x12	
#define READ_BUFF_LOG		0x24	
#define RELEASE			0xD4	
#define REQ_TRK_IN_ERROR	0x1B	
#define RESERVE			0xF4	
#define SENSE_GROUP_ID		0x34	
#define SENSE_ID		0xE4	
#define READ_DEV_CHAR		0x64	
#define SET_DIAGNOSE		0x4B	
#define SET_GROUP_ID		0xAF	
#define SET_TAPE_WRITE_IMMED	0xC3	
#define SUSPEND			0x5B	
#define SYNC			0x43	
#define UNASSIGN		0xC7	
#define PERF_SUBSYS_FUNC	0x77	
#define READ_CONFIG_DATA	0xFA	
#define READ_MESSAGE_ID		0x4E	
#define READ_SUBSYS_DATA	0x3E	
#define SET_INTERFACE_ID	0x73	

#define SENSE_COMMAND_REJECT		0x80
#define SENSE_INTERVENTION_REQUIRED	0x40
#define SENSE_BUS_OUT_CHECK		0x20
#define SENSE_EQUIPMENT_CHECK		0x10
#define SENSE_DATA_CHECK		0x08
#define SENSE_OVERRUN			0x04
#define SENSE_DEFERRED_UNIT_CHECK	0x02
#define SENSE_ASSIGNED_ELSEWHERE	0x01

#define SENSE_LOCATE_FAILURE		0x80
#define SENSE_DRIVE_ONLINE		0x40
#define SENSE_RESERVED			0x20
#define SENSE_RECORD_SEQUENCE_ERR	0x10
#define SENSE_BEGINNING_OF_TAPE		0x08
#define SENSE_WRITE_MODE		0x04
#define SENSE_WRITE_PROTECT		0x02
#define SENSE_NOT_CAPABLE		0x01

#define SENSE_CHANNEL_ADAPTER_CODE	0xE0
#define SENSE_CHANNEL_ADAPTER_LOC	0x10
#define SENSE_REPORTING_CU		0x08
#define SENSE_AUTOMATIC_LOADER		0x04
#define SENSE_TAPE_SYNC_MODE		0x02
#define SENSE_TAPE_POSITIONING		0x01

struct tape_request *tape_std_read_block(struct tape_device *, size_t);
void tape_std_read_backward(struct tape_device *device,
			    struct tape_request *request);
struct tape_request *tape_std_write_block(struct tape_device *, size_t);
struct tape_request *tape_std_bread(struct tape_device *, struct request *);
void tape_std_free_bread(struct tape_request *);
void tape_std_check_locate(struct tape_device *, struct tape_request *);
struct tape_request *tape_std_bwrite(struct request *,
				     struct tape_device *, int);

int tape_std_assign(struct tape_device *);
int tape_std_unassign(struct tape_device *);
int tape_std_read_block_id(struct tape_device *device, __u64 *id);
int tape_std_display(struct tape_device *, struct display_struct *disp);
int tape_std_terminate_write(struct tape_device *);

int tape_std_mtbsf(struct tape_device *, int);
int tape_std_mtbsfm(struct tape_device *, int);
int tape_std_mtbsr(struct tape_device *, int);
int tape_std_mtcompression(struct tape_device *, int);
int tape_std_mteom(struct tape_device *, int);
int tape_std_mterase(struct tape_device *, int);
int tape_std_mtfsf(struct tape_device *, int);
int tape_std_mtfsfm(struct tape_device *, int);
int tape_std_mtfsr(struct tape_device *, int);
int tape_std_mtload(struct tape_device *, int);
int tape_std_mtnop(struct tape_device *, int);
int tape_std_mtoffl(struct tape_device *, int);
int tape_std_mtreset(struct tape_device *, int);
int tape_std_mtreten(struct tape_device *, int);
int tape_std_mtrew(struct tape_device *, int);
int tape_std_mtsetblk(struct tape_device *, int);
int tape_std_mtunload(struct tape_device *, int);
int tape_std_mtweof(struct tape_device *, int);

void tape_std_default_handler(struct tape_device *);
void tape_std_unexpect_uchk_handler(struct tape_device *);
void tape_std_irq(struct tape_device *);
void tape_std_process_eov(struct tape_device *);

void tape_std_error_recovery(struct tape_device *);
void tape_std_error_recovery_has_failed(struct tape_device *,int error_id);
void tape_std_error_recovery_succeded(struct tape_device *);
void tape_std_error_recovery_do_retry(struct tape_device *);
void tape_std_error_recovery_read_opposite(struct tape_device *);
void tape_std_error_recovery_HWBUG(struct tape_device *, int condno);

enum s390_tape_type {
        tape_3480,
        tape_3490,
        tape_3590,
        tape_3592,
};

#endif 
