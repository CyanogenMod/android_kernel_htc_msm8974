/* qlogicpti.h: Performance Technologies QlogicISP sbus card defines.
 *
 * Copyright (C) 1996 David S. Miller (davem@caipfs.rutgers.edu)
 */

#ifndef _QLOGICPTI_H
#define _QLOGICPTI_H

#define SBUS_CFG1	0x006UL
#define SBUS_CTRL	0x008UL
#define SBUS_STAT	0x00aUL
#define SBUS_SEMAPHORE	0x00cUL
#define CMD_DMA_CTRL	0x022UL
#define DATA_DMA_CTRL	0x042UL
#define MBOX0		0x080UL
#define MBOX1		0x082UL
#define MBOX2		0x084UL
#define MBOX3		0x086UL
#define MBOX4		0x088UL
#define MBOX5		0x08aUL
#define CPU_CMD		0x214UL
#define CPU_ORIDE	0x224UL
#define CPU_PCTRL	0x272UL
#define CPU_PDIFF	0x276UL
#define RISC_PSR	0x420UL
#define RISC_MTREG	0x42EUL
#define HCCTRL		0x440UL

#define MAX_TARGETS	16
#define MAX_LUNS	8

#define QLOGICPTI_REQ_QUEUE_LEN	255	
#define QLOGICPTI_MAX_SG(ql)	(4 + (((ql) > 0) ? 7*((ql) - 1) : 0))

#define MBOX_COMMAND_COMPLETE		0x4000
#define INVALID_COMMAND			0x4001
#define HOST_INTERFACE_ERROR		0x4002
#define TEST_FAILED			0x4003
#define COMMAND_ERROR			0x4005
#define COMMAND_PARAM_ERROR		0x4006

#define ASYNC_SCSI_BUS_RESET		0x8001
#define SYSTEM_ERROR			0x8002
#define REQUEST_TRANSFER_ERROR		0x8003
#define RESPONSE_TRANSFER_ERROR		0x8004
#define REQUEST_QUEUE_WAKEUP		0x8005
#define EXECUTION_TIMEOUT_RESET		0x8006

struct Entry_header {
#ifdef __BIG_ENDIAN
	u8	entry_cnt;
	u8	entry_type;
	u8	flags;
	u8	sys_def_1;
#else 
	u8	entry_type;
	u8	entry_cnt;
	u8	sys_def_1;
	u8	flags;
#endif
};

#define ENTRY_COMMAND		1
#define ENTRY_CONTINUATION	2
#define ENTRY_STATUS		3
#define ENTRY_MARKER		4
#define ENTRY_EXTENDED_COMMAND	5

#define EFLAG_CONTINUATION	1
#define EFLAG_BUSY		2
#define EFLAG_BAD_HEADER	4
#define EFLAG_BAD_PAYLOAD	8

struct dataseg {
	u32	d_base;
	u32	d_count;
};

struct Command_Entry {
	struct Entry_header	hdr;
	u32			handle;
#ifdef __BIG_ENDIAN
	u8			target_id;
	u8			target_lun;
#else 
	u8			target_lun;
	u8			target_id;
#endif
	u16			cdb_length;
	u16			control_flags;
	u16			rsvd;
	u16			time_out;
	u16			segment_cnt;
	u8			cdb[12];
	struct dataseg		dataseg[4];
};

#define CFLAG_NODISC		0x01
#define CFLAG_HEAD_TAG		0x02
#define CFLAG_ORDERED_TAG	0x04
#define CFLAG_SIMPLE_TAG	0x08
#define CFLAG_TAR_RTN		0x10
#define CFLAG_READ		0x20
#define CFLAG_WRITE		0x40

struct Ext_Command_Entry {
	struct Entry_header	hdr;
	u32			handle;
#ifdef __BIG_ENDIAN
	u8			target_id;
	u8			target_lun;
#else 
	u8			target_lun;
	u8			target_id;
#endif
	u16			cdb_length;
	u16			control_flags;
	u16			rsvd;
	u16			time_out;
	u16			segment_cnt;
	u8			cdb[44];
};

struct Continuation_Entry {
	struct Entry_header	hdr;
	u32			reserved;
	struct dataseg		dataseg[7];
};

struct Marker_Entry {
	struct Entry_header	hdr;
	u32			reserved;
#ifdef __BIG_ENDIAN
	u8			target_id;
	u8			target_lun;
#else 
	u8			target_lun;
	u8			target_id;
#endif
#ifdef __BIG_ENDIAN
	u8			rsvd;
	u8			modifier;
#else 
	u8			modifier;
	u8			rsvd;
#endif
	u8			rsvds[52];
};

#define SYNC_DEVICE	0
#define SYNC_TARGET	1
#define SYNC_ALL	2

struct Status_Entry {
	struct Entry_header	hdr;
	u32			handle;
	u16			scsi_status;
	u16			completion_status;
	u16			state_flags;
	u16			status_flags;
	u16			time;
	u16			req_sense_len;
	u32			residual;
	u8			rsvd[8];
	u8			req_sense_data[32];
};

#define CS_COMPLETE			0x0000
#define CS_INCOMPLETE			0x0001
#define CS_DMA_ERROR			0x0002
#define CS_TRANSPORT_ERROR		0x0003
#define CS_RESET_OCCURRED		0x0004
#define CS_ABORTED			0x0005
#define CS_TIMEOUT			0x0006
#define CS_DATA_OVERRUN			0x0007
#define CS_COMMAND_OVERRUN		0x0008
#define CS_STATUS_OVERRUN		0x0009
#define CS_BAD_MESSAGE			0x000a
#define CS_NO_MESSAGE_OUT		0x000b
#define CS_EXT_ID_FAILED		0x000c
#define CS_IDE_MSG_FAILED		0x000d
#define CS_ABORT_MSG_FAILED		0x000e
#define CS_REJECT_MSG_FAILED		0x000f
#define CS_NOP_MSG_FAILED		0x0010
#define CS_PARITY_ERROR_MSG_FAILED	0x0011
#define CS_DEVICE_RESET_MSG_FAILED	0x0012
#define CS_ID_MSG_FAILED		0x0013
#define CS_UNEXP_BUS_FREE		0x0014
#define CS_DATA_UNDERRUN		0x0015
#define CS_BUS_RESET			0x001c

#define SF_GOT_BUS			0x0100
#define SF_GOT_TARGET			0x0200
#define SF_SENT_CDB			0x0400
#define SF_TRANSFERRED_DATA		0x0800
#define SF_GOT_STATUS			0x1000
#define SF_GOT_SENSE			0x2000

#define STF_DISCONNECT			0x0001
#define STF_SYNCHRONOUS			0x0002
#define STF_PARITY_ERROR		0x0004
#define STF_BUS_RESET			0x0008
#define STF_DEVICE_RESET		0x0010
#define STF_ABORTED			0x0020
#define STF_TIMEOUT			0x0040
#define STF_NEGOTIATION			0x0080

#define MBOX_NO_OP			0x0000
#define MBOX_LOAD_RAM			0x0001
#define MBOX_EXEC_FIRMWARE		0x0002
#define MBOX_DUMP_RAM			0x0003
#define MBOX_WRITE_RAM_WORD		0x0004
#define MBOX_READ_RAM_WORD		0x0005
#define MBOX_MAILBOX_REG_TEST		0x0006
#define MBOX_VERIFY_CHECKSUM		0x0007
#define MBOX_ABOUT_FIRMWARE		0x0008
#define MBOX_CHECK_FIRMWARE		0x000e
#define MBOX_INIT_REQ_QUEUE		0x0010
#define MBOX_INIT_RES_QUEUE		0x0011
#define MBOX_EXECUTE_IOCB		0x0012
#define MBOX_WAKE_UP			0x0013
#define MBOX_STOP_FIRMWARE		0x0014
#define MBOX_ABORT			0x0015
#define MBOX_ABORT_DEVICE		0x0016
#define MBOX_ABORT_TARGET		0x0017
#define MBOX_BUS_RESET			0x0018
#define MBOX_STOP_QUEUE			0x0019
#define MBOX_START_QUEUE		0x001a
#define MBOX_SINGLE_STEP_QUEUE		0x001b
#define MBOX_ABORT_QUEUE		0x001c
#define MBOX_GET_DEV_QUEUE_STATUS	0x001d
#define MBOX_GET_FIRMWARE_STATUS	0x001f
#define MBOX_GET_INIT_SCSI_ID		0x0020
#define MBOX_GET_SELECT_TIMEOUT		0x0021
#define MBOX_GET_RETRY_COUNT		0x0022
#define MBOX_GET_TAG_AGE_LIMIT		0x0023
#define MBOX_GET_CLOCK_RATE		0x0024
#define MBOX_GET_ACT_NEG_STATE		0x0025
#define MBOX_GET_ASYNC_DATA_SETUP_TIME	0x0026
#define MBOX_GET_SBUS_PARAMS		0x0027
#define MBOX_GET_TARGET_PARAMS		0x0028
#define MBOX_GET_DEV_QUEUE_PARAMS	0x0029
#define MBOX_SET_INIT_SCSI_ID		0x0030
#define MBOX_SET_SELECT_TIMEOUT		0x0031
#define MBOX_SET_RETRY_COUNT		0x0032
#define MBOX_SET_TAG_AGE_LIMIT		0x0033
#define MBOX_SET_CLOCK_RATE		0x0034
#define MBOX_SET_ACTIVE_NEG_STATE	0x0035
#define MBOX_SET_ASYNC_DATA_SETUP_TIME	0x0036
#define MBOX_SET_SBUS_CONTROL_PARAMS	0x0037
#define MBOX_SET_TARGET_PARAMS		0x0038
#define MBOX_SET_DEV_QUEUE_PARAMS	0x0039

struct host_param {
	u_short		initiator_scsi_id;
	u_short		bus_reset_delay;
	u_short		retry_count;
	u_short		retry_delay;
	u_short		async_data_setup_time;
	u_short		req_ack_active_negation;
	u_short		data_line_active_negation;
	u_short		data_dma_burst_enable;
	u_short		command_dma_burst_enable;
	u_short		tag_aging;
	u_short		selection_timeout;
	u_short		max_queue_depth;
};


struct dev_param {
	u_short		device_flags;
	u_short		execution_throttle;
	u_short		synchronous_period;
	u_short		synchronous_offset;
	u_short		device_enable;
	u_short		reserved; 
};

#define RES_QUEUE_LEN		255	
#define QUEUE_ENTRY_LEN		64

#define NEXT_REQ_PTR(wheee)   (((wheee) + 1) & QLOGICPTI_REQ_QUEUE_LEN)
#define NEXT_RES_PTR(wheee)   (((wheee) + 1) & RES_QUEUE_LEN)
#define PREV_REQ_PTR(wheee)   (((wheee) - 1) & QLOGICPTI_REQ_QUEUE_LEN)
#define PREV_RES_PTR(wheee)   (((wheee) - 1) & RES_QUEUE_LEN)

struct pti_queue_entry {
	char __opaque[QUEUE_ENTRY_LEN];
};

struct scsi_cmnd;

struct qlogicpti {
	
	void __iomem             *qregs;                
	struct pti_queue_entry   *res_cpu;              
	struct pti_queue_entry   *req_cpu;              

	u_int	                  req_in_ptr;		
	u_int	                  res_out_ptr;		
	long	                  send_marker;		
	struct platform_device	 *op;
	unsigned long		  __pad;

	int                       cmd_count[MAX_TARGETS];
	unsigned long             tag_ages[MAX_TARGETS];

	struct scsi_cmnd         *cmd_slots[QLOGICPTI_REQ_QUEUE_LEN + 1];

	
	struct qlogicpti         *next;
	__u32                     res_dvma;             
	__u32                     req_dvma;             
	u_char	                  fware_majrev, fware_minrev, fware_micrev;
	struct Scsi_Host         *qhost;
	int                       qpti_id;
	int                       scsi_id;
	int                       prom_node;
	char                      prom_name[64];
	int                       irq;
	char                      differential, ultra, clock;
	unsigned char             bursts;
	struct	host_param        host_param;
	struct	dev_param         dev_param[MAX_TARGETS];

	void __iomem              *sreg;
#define SREG_TPOWER               0x80   
#define SREG_FUSE                 0x40   
#define SREG_PDISAB               0x20   
#define SREG_DSENSE               0x10   
#define SREG_IMASK                0x0c   
#define SREG_SPMASK               0x03   
	unsigned char             swsreg;
	unsigned int	
		gotirq	:	1,	
		is_pti	: 	1;	
};


#define SBUS_CFG1_EPAR          0x0100      
#define SBUS_CFG1_FMASK         0x00f0      
#define SBUS_CFG1_BENAB         0x0004      
#define SBUS_CFG1_B64           0x0003      
#define SBUS_CFG1_B32           0x0002      
#define SBUS_CFG1_B16           0x0001      
#define SBUS_CFG1_B8            0x0008      

#define SBUS_CTRL_EDIRQ         0x0020      
#define SBUS_CTRL_ECIRQ         0x0010      
#define SBUS_CTRL_ESIRQ         0x0008      
#define SBUS_CTRL_ERIRQ         0x0004      
#define SBUS_CTRL_GENAB         0x0002      
#define SBUS_CTRL_RESET         0x0001      

#define SBUS_STAT_DINT          0x0020      
#define SBUS_STAT_CINT          0x0010      
#define SBUS_STAT_SINT          0x0008      
#define SBUS_STAT_RINT          0x0004      
#define SBUS_STAT_GINT          0x0002      

#define SBUS_SEMAPHORE_STAT     0x0002      
#define SBUS_SEMAPHORE_LCK      0x0001      

#define DMA_CTRL_CSUSPEND       0x0010      
#define DMA_CTRL_CCLEAR         0x0008      
#define DMA_CTRL_FCLEAR         0x0004      
#define DMA_CTRL_CIRQ           0x0002      
#define DMA_CTRL_DMASTART       0x0001      

#define CPU_ORIDE_ETRIG         0x8000      
#define CPU_ORIDE_STEP          0x4000      
#define CPU_ORIDE_BKPT          0x2000      
#define CPU_ORIDE_PWRITE        0x1000      
#define CPU_ORIDE_OFORCE        0x0800      
#define CPU_ORIDE_LBACK         0x0400      
#define CPU_ORIDE_PTEST         0x0200      
#define CPU_ORIDE_TENAB         0x0100      
#define CPU_ORIDE_TPINS         0x0080      
#define CPU_ORIDE_FRESET        0x0008      
#define CPU_ORIDE_CTERM         0x0004      
#define CPU_ORIDE_RREG          0x0002      
#define CPU_ORIDE_RMOD          0x0001      

#define CPU_CMD_BRESET          0x300b      

#define CPU_PCTRL_PVALID        0x8000      
#define CPU_PCTRL_PHI           0x0400      
#define CPU_PCTRL_PLO           0x0200      
#define CPU_PCTRL_REQ           0x0100      
#define CPU_PCTRL_ACK           0x0080      
#define CPU_PCTRL_RST           0x0040      
#define CPU_PCTRL_BSY           0x0020      
#define CPU_PCTRL_SEL           0x0010      
#define CPU_PCTRL_ATN           0x0008      
#define CPU_PCTRL_MSG           0x0004      
#define CPU_PCTRL_CD            0x0002      
#define CPU_PCTRL_IO            0x0001      

#define CPU_PDIFF_SENSE         0x0200      
#define CPU_PDIFF_MODE          0x0100      
#define CPU_PDIFF_OENAB         0x0080      
#define CPU_PDIFF_PMASK         0x007c      
#define CPU_PDIFF_TGT           0x0002      
#define CPU_PDIFF_INIT          0x0001      

#define RISC_PSR_FTRUE          0x8000      
#define RISC_PSR_LCD            0x4000      
#define RISC_PSR_RIRQ           0x2000      
#define RISC_PSR_TOFLOW         0x1000      
#define RISC_PSR_AOFLOW         0x0800      
#define RISC_PSR_AMSB           0x0400      
#define RISC_PSR_ACARRY         0x0200      
#define RISC_PSR_AZERO          0x0100      
#define RISC_PSR_ULTRA          0x0020      
#define RISC_PSR_DIRQ           0x0010      
#define RISC_PSR_SIRQ           0x0008      
#define RISC_PSR_HIRQ           0x0004      
#define RISC_PSR_IPEND          0x0002      
#define RISC_PSR_FFALSE         0x0001      

#define RISC_MTREG_P1DFLT       0x1200      
#define RISC_MTREG_P0DFLT       0x0012      
#define RISC_MTREG_P1ULTRA      0x2300      
#define RISC_MTREG_P0ULTRA      0x0023      

#define HCCTRL_NOP              0x0000      
#define HCCTRL_RESET            0x1000      
#define HCCTRL_PAUSE            0x2000      
#define HCCTRL_REL              0x3000      
#define HCCTRL_STEP             0x4000      
#define HCCTRL_SHIRQ            0x5000      
#define HCCTRL_CHIRQ            0x6000      
#define HCCTRL_CRIRQ            0x7000      
#define HCCTRL_BKPT             0x8000      
#define HCCTRL_TMODE            0xf000      
#define HCCTRL_HIRQ             0x0080      
#define HCCTRL_RRIP             0x0040      
#define HCCTRL_RPAUSED          0x0020      
#define HCCTRL_EBENAB           0x0010      
#define HCCTRL_B1ENAB           0x0008      
#define HCCTRL_B0ENAB           0x0004      

#define for_each_qlogicpti(qp) \
        for((qp) = qptichain; (qp); (qp) = (qp)->next)

#endif 
