/* 
 * NCR 5380 defines
 *
 * Copyright 1993, Drew Eckhardt
 *	Visionary Computing
 *	(Unix consulting and custom programming)
 * 	drew@colorado.edu
 *      +1 (303) 666-5836
 *
 * DISTRIBUTION RELEASE 7
 *
 * For more information, please consult 
 *
 * NCR 5380 Family
 * SCSI Protocol Controller
 * Databook
 * NCR Microelectronics
 * 1635 Aeroplaza Drive
 * Colorado Springs, CO 80916
 * 1+ (719) 578-3400
 * 1+ (800) 334-5454
 */


#ifndef NCR5380_H
#define NCR5380_H

#include <linux/interrupt.h>

#ifdef AUTOSENSE
#include <scsi/scsi_eh.h>
#endif

#define NCR5380_PUBLIC_RELEASE 7
#define NCR53C400_PUBLIC_RELEASE 2

#define NDEBUG_ARBITRATION	0x1
#define NDEBUG_AUTOSENSE	0x2
#define NDEBUG_DMA		0x4
#define NDEBUG_HANDSHAKE	0x8
#define NDEBUG_INFORMATION	0x10
#define NDEBUG_INIT		0x20
#define NDEBUG_INTR		0x40
#define NDEBUG_LINKED		0x80
#define NDEBUG_MAIN		0x100
#define NDEBUG_NO_DATAOUT	0x200
#define NDEBUG_NO_WRITE		0x400
#define NDEBUG_PIO		0x800
#define NDEBUG_PSEUDO_DMA	0x1000
#define NDEBUG_QUEUES		0x2000
#define NDEBUG_RESELECTION	0x4000
#define NDEBUG_SELECTION	0x8000
#define NDEBUG_USLEEP		0x10000
#define NDEBUG_LAST_BYTE_SENT	0x20000
#define NDEBUG_RESTART_SELECT	0x40000
#define NDEBUG_EXTENDED		0x80000
#define NDEBUG_C400_PREAD	0x100000
#define NDEBUG_C400_PWRITE	0x200000
#define NDEBUG_LISTS		0x400000

#define NDEBUG_ANY		0xFFFFFFFFUL


#define OUTPUT_DATA_REG         0	
#define CURRENT_SCSI_DATA_REG   0	

#define INITIATOR_COMMAND_REG	1	
#define ICR_ASSERT_RST		0x80	
#define ICR_ARBITRATION_PROGRESS 0x40	
#define ICR_TRI_STATE		0x40	
#define ICR_ARBITRATION_LOST	0x20	
#define ICR_DIFF_ENABLE		0x20	
#define ICR_ASSERT_ACK		0x10	
#define ICR_ASSERT_BSY		0x08	
#define ICR_ASSERT_SEL 		0x04	
#define ICR_ASSERT_ATN		0x02	
#define ICR_ASSERT_DATA		0x01	

#ifdef DIFFERENTIAL
#define ICR_BASE		ICR_DIFF_ENABLE
#else
#define ICR_BASE		0
#endif

#define MODE_REG		2
#define MR_BLOCK_DMA_MODE	0x80	
#define MR_TARGET		0x40	
#define MR_ENABLE_PAR_CHECK	0x20	
#define MR_ENABLE_PAR_INTR	0x10	
#define MR_ENABLE_EOP_INTR	0x08	
#define MR_MONITOR_BSY		0x04	
#define MR_DMA_MODE		0x02	
#define MR_ARBITRATE		0x01	

#ifdef PARITY
#define MR_BASE			MR_ENABLE_PAR_CHECK
#else
#define MR_BASE			0
#endif

#define TARGET_COMMAND_REG	3
#define TCR_LAST_BYTE_SENT	0x80	
#define TCR_ASSERT_REQ		0x08	
#define TCR_ASSERT_MSG		0x04	
#define TCR_ASSERT_CD		0x02	
#define TCR_ASSERT_IO		0x01	

#define STATUS_REG		4	
#define SR_RST			0x80
#define SR_BSY			0x40
#define SR_REQ			0x20
#define SR_MSG			0x10
#define SR_CD			0x08
#define SR_IO			0x04
#define SR_SEL			0x02
#define SR_DBP			0x01

#define SELECT_ENABLE_REG	4	

#define BUS_AND_STATUS_REG	5	
#define BASR_END_DMA_TRANSFER	0x80	
#define BASR_DRQ		0x40	
#define BASR_PARITY_ERROR	0x20	
#define BASR_IRQ		0x10	
#define BASR_PHASE_MATCH	0x08	
#define BASR_BUSY_ERROR		0x04	
#define BASR_ATN 		0x02	
#define BASR_ACK		0x01	

#define START_DMA_SEND_REG	5	

#define INPUT_DATA_REG			6	

#define START_DMA_TARGET_RECEIVE_REG	6	

#define RESET_PARITY_INTERRUPT_REG	7	

#define START_DMA_INITIATOR_RECEIVE_REG 7	

#define C400_CONTROL_STATUS_REG NCR53C400_register_offset-8	

#define CSR_RESET              0x80	
#define CSR_53C80_REG          0x80	
#define CSR_TRANS_DIR          0x40	
#define CSR_SCSI_BUFF_INTR     0x20	
#define CSR_53C80_INTR         0x10	
#define CSR_SHARED_INTR        0x08	
#define CSR_HOST_BUF_NOT_RDY   0x04	
#define CSR_SCSI_BUF_RDY       0x02	
#define CSR_GATED_53C80_IRQ    0x01	

#if 0
#define CSR_BASE CSR_SCSI_BUFF_INTR | CSR_53C80_INTR
#else
#define CSR_BASE CSR_53C80_INTR
#endif

#define C400_BLOCK_COUNTER_REG   NCR53C400_register_offset-7	

#define C400_RESUME_TRANSFER_REG NCR53C400_register_offset-6	

#define C400_HOST_BUFFER         NCR53C400_register_offset-4	


#define PHASE_MASK 	(SR_MSG | SR_CD | SR_IO)

#define PHASE_DATAOUT		0
#define PHASE_DATAIN		SR_IO
#define PHASE_CMDOUT		SR_CD
#define PHASE_STATIN		(SR_CD | SR_IO)
#define PHASE_MSGOUT		(SR_MSG | SR_CD)
#define PHASE_MSGIN		(SR_MSG | SR_CD | SR_IO)
#define PHASE_UNKNOWN		0xff


#define PHASE_SR_TO_TCR(phase) ((phase) >> 2)


#define DISCONNECT_NONE		0
#define DISCONNECT_TIME_TO_DATA	1
#define DISCONNECT_LONG		2


#define TAG_NEXT	-1	
#define TAG_NONE	-2	


#define SCSI_IRQ_NONE	255
#define DMA_NONE	255
#define IRQ_AUTO	254
#define DMA_AUTO	254
#define PORT_AUTO	0xffff	

#define FLAG_HAS_LAST_BYTE_SENT		1	
#define FLAG_CHECK_LAST_BYTE_SENT	2	
#define FLAG_NCR53C400			4	
#define FLAG_NO_PSEUDO_DMA		8	
#define FLAG_DTC3181E			16	

#ifndef ASM
struct NCR5380_hostdata {
	NCR5380_implementation_fields;		
	struct Scsi_Host *host;			
	unsigned char id_mask, id_higher_mask;	
	unsigned char targets_present;		
	volatile unsigned char busy[8];		
#if defined(REAL_DMA) || defined(REAL_DMA_POLL)
	volatile int dma_len;			
#endif
	volatile unsigned char last_message;	
	volatile Scsi_Cmnd *connected;		
	volatile Scsi_Cmnd *issue_queue;	
	volatile Scsi_Cmnd *disconnected_queue;	
	volatile int restart_select;		
	volatile unsigned aborted:1;		
	int flags;
	unsigned long time_expires;		
	int select_time;			
	volatile Scsi_Cmnd *selecting;
	struct delayed_work coroutine;		
#ifdef NCR5380_STATS
	unsigned timebase;			
	long time_read[8];			
	long time_write[8];			
	unsigned long bytes_read[8];		
	unsigned long bytes_write[8];		/* bytes written */
	unsigned pendingr;
	unsigned pendingw;
#endif
#ifdef AUTOSENSE
	struct scsi_eh_save ses;
#endif
};

#ifdef __KERNEL__

#define dprintk(a,b)			do {} while(0)
#define NCR5380_dprint(a,b)		do {} while(0)
#define NCR5380_dprint_phase(a,b)	do {} while(0)

#if defined(AUTOPROBE_IRQ)
static int NCR5380_probe_irq(struct Scsi_Host *instance, int possible);
#endif
static int NCR5380_init(struct Scsi_Host *instance, int flags);
static void NCR5380_exit(struct Scsi_Host *instance);
static void NCR5380_information_transfer(struct Scsi_Host *instance);
#ifndef DONT_USE_INTR
static irqreturn_t NCR5380_intr(int irq, void *dev_id);
#endif
static void NCR5380_main(struct work_struct *work);
static void __maybe_unused NCR5380_print_options(struct Scsi_Host *instance);
#ifdef NDEBUG
static void NCR5380_print_phase(struct Scsi_Host *instance);
static void NCR5380_print(struct Scsi_Host *instance);
#endif
static int NCR5380_abort(Scsi_Cmnd * cmd);
static int NCR5380_bus_reset(Scsi_Cmnd * cmd);
static int NCR5380_queue_command(struct Scsi_Host *, struct scsi_cmnd *);
static int __maybe_unused NCR5380_proc_info(struct Scsi_Host *instance,
	char *buffer, char **start, off_t offset, int length, int inout);

static void NCR5380_reselect(struct Scsi_Host *instance);
static int NCR5380_select(struct Scsi_Host *instance, Scsi_Cmnd * cmd, int tag);
#if defined(PSEUDO_DMA) || defined(REAL_DMA) || defined(REAL_DMA_POLL)
static int NCR5380_transfer_dma(struct Scsi_Host *instance, unsigned char *phase, int *count, unsigned char **data);
#endif
static int NCR5380_transfer_pio(struct Scsi_Host *instance, unsigned char *phase, int *count, unsigned char **data);

#if (defined(REAL_DMA) || defined(REAL_DMA_POLL))

#if defined(i386) || defined(__alpha__)

 
static __inline__ int NCR5380_pc_dma_setup(struct Scsi_Host *instance, unsigned char *ptr, unsigned int count, unsigned char mode)
{
	unsigned limit;
	unsigned long bus_addr = virt_to_bus(ptr);
	unsigned long flags;

	if (instance->dma_channel <= 3) {
		if (count > 65536)
			count = 65536;
		limit = 65536 - (bus_addr & 0xFFFF);
	} else {
		if (count > 65536 * 2)
			count = 65536 * 2;
		limit = 65536 * 2 - (bus_addr & 0x1FFFF);
	}

	if (count > limit)
		count = limit;

	if ((count & 1) || (bus_addr & 1))
		panic("scsi%d : attempted unaligned DMA transfer\n", instance->host_no);
	
	flags=claim_dma_lock();
	disable_dma(instance->dma_channel);
	clear_dma_ff(instance->dma_channel);
	set_dma_addr(instance->dma_channel, bus_addr);
	set_dma_count(instance->dma_channel, count);
	set_dma_mode(instance->dma_channel, mode);
	enable_dma(instance->dma_channel);
	release_dma_lock(flags);
	
	return count;
}


static __inline__ int NCR5380_pc_dma_write_setup(struct Scsi_Host *instance, unsigned char *src, unsigned int count)
{
	return NCR5380_pc_dma_setup(instance, src, count, DMA_MODE_WRITE);
}


static __inline__ int NCR5380_pc_dma_read_setup(struct Scsi_Host *instance, unsigned char *src, unsigned int count)
{
	return NCR5380_pc_dma_setup(instance, src, count, DMA_MODE_READ);
}


static __inline__ int NCR5380_pc_dma_residual(struct Scsi_Host *instance)
{
	unsigned long flags;
	int tmp;

	flags = claim_dma_lock();
	clear_dma_ff(instance->dma_channel);
	tmp = get_dma_residue(instance->dma_channel);
	release_dma_lock(flags);
	
	return tmp;
}
#endif				
#endif				
#endif				
#endif				
#endif				
