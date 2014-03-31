/* 
 * NCR 5380 generic driver routines.  These should make it *trivial*
 *      to implement 5380 SCSI drivers under Linux with a non-trantor
 *      architecture.
 *
 *      Note that these routines also work with NR53c400 family chips.
 *
 * Copyright 1993, Drew Eckhardt
 *      Visionary Computing 
 *      (Unix and Linux consulting and custom programming)
 *      drew@colorado.edu
 *      +1 (303) 666-5836
 *
 * DISTRIBUTION RELEASE 6. 
 *
 * For more information, please consult 
 *
 * NCR 5380 Family
 * SCSI Protocol Controller
 * Databook
 *
 * NCR Microelectronics
 * 1635 Aeroplaza Drive
 * Colorado Springs, CO 80916
 * 1+ (719) 578-3400
 * 1+ (800) 334-5454
 */


#include <scsi/scsi_dbg.h>
#include <scsi/scsi_transport_spi.h>

#ifndef NDEBUG
#define NDEBUG 0
#endif
#ifndef NDEBUG_ABORT
#define NDEBUG_ABORT 0
#endif

#if (NDEBUG & NDEBUG_LISTS)
#define LIST(x,y) {printk("LINE:%d   Adding %p to %p\n", __LINE__, (void*)(x), (void*)(y)); if ((x)==(y)) udelay(5); }
#define REMOVE(w,x,y,z) {printk("LINE:%d   Removing: %p->%p  %p->%p \n", __LINE__, (void*)(w), (void*)(x), (void*)(y), (void*)(z)); if ((x)==(y)) udelay(5); }
#else
#define LIST(x,y)
#define REMOVE(w,x,y,z)
#endif

#ifndef notyet
#undef LINKED
#undef REAL_DMA
#endif

#ifdef REAL_DMA_POLL
#undef READ_OVERRUNS
#define READ_OVERRUNS
#endif

#ifdef BOARD_REQUIRES_NO_DELAY
#define io_recovery_delay(x)
#else
#define io_recovery_delay(x)	udelay(x)
#endif



static int do_abort(struct Scsi_Host *host);
static void do_reset(struct Scsi_Host *host);


static __inline__ void initialize_SCp(Scsi_Cmnd * cmd)
{

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
}

 
static int NCR5380_poll_politely(struct Scsi_Host *instance, int reg, int bit, int val, int t)
{
	NCR5380_local_declare();
	int n = 500;		
	unsigned long end = jiffies + t;
	int r;
	
	NCR5380_setup(instance);

	while( n-- > 0)
	{
		r = NCR5380_read(reg);
		if((r & bit) == val)
			return 0;
		cpu_relax();
	}
	
	
	while(time_before(jiffies, end))
	{
		r = NCR5380_read(reg);
		if((r & bit) == val)
			return 0;
		if(!in_interrupt())
			cond_resched();
		else
			cpu_relax();
	}
	return -ETIMEDOUT;
}

static struct {
	unsigned char value;
	const char *name;
} phases[] __maybe_unused = {
	{PHASE_DATAOUT, "DATAOUT"}, 
	{PHASE_DATAIN, "DATAIN"}, 
	{PHASE_CMDOUT, "CMDOUT"}, 
	{PHASE_STATIN, "STATIN"}, 
	{PHASE_MSGOUT, "MSGOUT"}, 
	{PHASE_MSGIN, "MSGIN"}, 
	{PHASE_UNKNOWN, "UNKNOWN"}
};

#if NDEBUG
static struct {
	unsigned char mask;
	const char *name;
} signals[] = { 
	{SR_DBP, "PARITY"}, 
	{SR_RST, "RST"}, 
	{SR_BSY, "BSY"}, 
	{SR_REQ, "REQ"}, 
	{SR_MSG, "MSG"}, 
	{SR_CD, "CD"}, 
	{SR_IO, "IO"}, 
	{SR_SEL, "SEL"}, 
	{0, NULL}
}, 
basrs[] = {
	{BASR_ATN, "ATN"}, 
	{BASR_ACK, "ACK"}, 
	{0, NULL}
}, 
icrs[] = { 
	{ICR_ASSERT_RST, "ASSERT RST"}, 
	{ICR_ASSERT_ACK, "ASSERT ACK"}, 
	{ICR_ASSERT_BSY, "ASSERT BSY"}, 
	{ICR_ASSERT_SEL, "ASSERT SEL"}, 
	{ICR_ASSERT_ATN, "ASSERT ATN"}, 
	{ICR_ASSERT_DATA, "ASSERT DATA"}, 
	{0, NULL}
}, 
mrs[] = { 
	{MR_BLOCK_DMA_MODE, "MODE BLOCK DMA"}, 
	{MR_TARGET, "MODE TARGET"}, 
	{MR_ENABLE_PAR_CHECK, "MODE PARITY CHECK"}, 
	{MR_ENABLE_PAR_INTR, "MODE PARITY INTR"}, 
	{MR_MONITOR_BSY, "MODE MONITOR BSY"}, 
	{MR_DMA_MODE, "MODE DMA"}, 
	{MR_ARBITRATE, "MODE ARBITRATION"}, 
	{0, NULL}
};


static void NCR5380_print(struct Scsi_Host *instance)
{
	NCR5380_local_declare();
	unsigned char status, data, basr, mr, icr, i;
	NCR5380_setup(instance);

	data = NCR5380_read(CURRENT_SCSI_DATA_REG);
	status = NCR5380_read(STATUS_REG);
	mr = NCR5380_read(MODE_REG);
	icr = NCR5380_read(INITIATOR_COMMAND_REG);
	basr = NCR5380_read(BUS_AND_STATUS_REG);

	printk("STATUS_REG: %02x ", status);
	for (i = 0; signals[i].mask; ++i)
		if (status & signals[i].mask)
			printk(",%s", signals[i].name);
	printk("\nBASR: %02x ", basr);
	for (i = 0; basrs[i].mask; ++i)
		if (basr & basrs[i].mask)
			printk(",%s", basrs[i].name);
	printk("\nICR: %02x ", icr);
	for (i = 0; icrs[i].mask; ++i)
		if (icr & icrs[i].mask)
			printk(",%s", icrs[i].name);
	printk("\nMODE: %02x ", mr);
	for (i = 0; mrs[i].mask; ++i)
		if (mr & mrs[i].mask)
			printk(",%s", mrs[i].name);
	printk("\n");
}



static void NCR5380_print_phase(struct Scsi_Host *instance)
{
	NCR5380_local_declare();
	unsigned char status;
	int i;
	NCR5380_setup(instance);

	status = NCR5380_read(STATUS_REG);
	if (!(status & SR_REQ))
		printk("scsi%d : REQ not asserted, phase unknown.\n", instance->host_no);
	else {
		for (i = 0; (phases[i].value != PHASE_UNKNOWN) && (phases[i].value != (status & PHASE_MASK)); ++i);
		printk("scsi%d : phase %s\n", instance->host_no, phases[i].name);
	}
}
#endif

#ifndef USLEEP_SLEEP
#define USLEEP_SLEEP (20*HZ/1000)
#endif
#ifndef USLEEP_POLL
#define USLEEP_POLL (200*HZ/1000)
#endif
#ifndef USLEEP_WAITLONG
#define USLEEP_WAITLONG USLEEP_SLEEP
#endif


static int should_disconnect(unsigned char cmd)
{
	switch (cmd) {
	case READ_6:
	case WRITE_6:
	case SEEK_6:
	case READ_10:
	case WRITE_10:
	case SEEK_10:
		return DISCONNECT_TIME_TO_DATA;
	case FORMAT_UNIT:
	case SEARCH_HIGH:
	case SEARCH_LOW:
	case SEARCH_EQUAL:
		return DISCONNECT_LONG;
	default:
		return DISCONNECT_NONE;
	}
}

static void NCR5380_set_timer(struct NCR5380_hostdata *hostdata, unsigned long timeout)
{
	hostdata->time_expires = jiffies + timeout;
	schedule_delayed_work(&hostdata->coroutine, timeout);
}


static int probe_irq __initdata = 0;

 
static irqreturn_t __init probe_intr(int irq, void *dev_id)
{
	probe_irq = irq;
	return IRQ_HANDLED;
}


static int __init __maybe_unused NCR5380_probe_irq(struct Scsi_Host *instance,
						int possible)
{
	NCR5380_local_declare();
	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *) instance->hostdata;
	unsigned long timeout;
	int trying_irqs, i, mask;
	NCR5380_setup(instance);

	for (trying_irqs = i = 0, mask = 1; i < 16; ++i, mask <<= 1)
		if ((mask & possible) && (request_irq(i, &probe_intr, IRQF_DISABLED, "NCR-probe", NULL) == 0))
			trying_irqs |= mask;

	timeout = jiffies + (250 * HZ / 1000);
	probe_irq = SCSI_IRQ_NONE;


	NCR5380_write(TARGET_COMMAND_REG, 0);
	NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
	NCR5380_write(OUTPUT_DATA_REG, hostdata->id_mask);
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA | ICR_ASSERT_SEL);

	while (probe_irq == SCSI_IRQ_NONE && time_before(jiffies, timeout))
		schedule_timeout_uninterruptible(1);
	
	NCR5380_write(SELECT_ENABLE_REG, 0);
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);

	for (i = 0, mask = 1; i < 16; ++i, mask <<= 1)
		if (trying_irqs & mask)
			free_irq(i, NULL);

	return probe_irq;
}


static void __init __maybe_unused
NCR5380_print_options(struct Scsi_Host *instance)
{
	printk(" generic options"
#ifdef AUTOPROBE_IRQ
	       " AUTOPROBE_IRQ"
#endif
#ifdef AUTOSENSE
	       " AUTOSENSE"
#endif
#ifdef DIFFERENTIAL
	       " DIFFERENTIAL"
#endif
#ifdef REAL_DMA
	       " REAL DMA"
#endif
#ifdef REAL_DMA_POLL
	       " REAL DMA POLL"
#endif
#ifdef PARITY
	       " PARITY"
#endif
#ifdef PSEUDO_DMA
	       " PSEUDO DMA"
#endif
#ifdef UNSAFE
	       " UNSAFE "
#endif
	    );
	printk(" USLEEP, USLEEP_POLL=%d USLEEP_SLEEP=%d", USLEEP_POLL, USLEEP_SLEEP);
	printk(" generic release=%d", NCR5380_PUBLIC_RELEASE);
	if (((struct NCR5380_hostdata *) instance->hostdata)->flags & FLAG_NCR53C400) {
		printk(" ncr53c400 release=%d", NCR53C400_PUBLIC_RELEASE);
	}
}


static void NCR5380_print_status(struct Scsi_Host *instance)
{
	NCR5380_dprint(NDEBUG_ANY, instance);
	NCR5380_dprint_phase(NDEBUG_ANY, instance);
}

/*
 * /proc/scsi/[dtc pas16 t128 generic]/[0-ASC_NUM_BOARD_SUPPORTED]
 *
 * *buffer: I/O buffer
 * **start: if inout == FALSE pointer into buffer where user read should start
 * offset: current offset
 * length: length of buffer
 * hostno: Scsi_Host host_no
 * inout: TRUE - user is writing; FALSE - user is reading
 *
 * Return the number of bytes read from or written
 */

#undef SPRINTF
#define SPRINTF(args...) do { if(pos < buffer + length-80) pos += sprintf(pos, ## args); } while(0)
static
char *lprint_Scsi_Cmnd(Scsi_Cmnd * cmd, char *pos, char *buffer, int length);
static
char *lprint_command(unsigned char *cmd, char *pos, char *buffer, int len);
static
char *lprint_opcode(int opcode, char *pos, char *buffer, int length);

static int __maybe_unused NCR5380_proc_info(struct Scsi_Host *instance,
	char *buffer, char **start, off_t offset, int length, int inout)
{
	char *pos = buffer;
	struct NCR5380_hostdata *hostdata;
	Scsi_Cmnd *ptr;

	hostdata = (struct NCR5380_hostdata *) instance->hostdata;

	if (inout) {		/* Has data been written to the file ? */
#ifdef DTC_PUBLIC_RELEASE
		dtc_wmaxi = dtc_maxi = 0;
#endif
#ifdef PAS16_PUBLIC_RELEASE
		pas_wmaxi = pas_maxi = 0;
#endif
		return (-ENOSYS);	
	}
	SPRINTF("NCR5380 core release=%d.   ", NCR5380_PUBLIC_RELEASE);
	if (((struct NCR5380_hostdata *) instance->hostdata)->flags & FLAG_NCR53C400)
		SPRINTF("ncr53c400 release=%d.  ", NCR53C400_PUBLIC_RELEASE);
#ifdef DTC_PUBLIC_RELEASE
	SPRINTF("DTC 3180/3280 release %d", DTC_PUBLIC_RELEASE);
#endif
#ifdef T128_PUBLIC_RELEASE
	SPRINTF("T128 release %d", T128_PUBLIC_RELEASE);
#endif
#ifdef GENERIC_NCR5380_PUBLIC_RELEASE
	SPRINTF("Generic5380 release %d", GENERIC_NCR5380_PUBLIC_RELEASE);
#endif
#ifdef PAS16_PUBLIC_RELEASE
	SPRINTF("PAS16 release=%d", PAS16_PUBLIC_RELEASE);
#endif

	SPRINTF("\nBase Addr: 0x%05lX    ", (long) instance->base);
	SPRINTF("io_port: %04x      ", (int) instance->io_port);
	if (instance->irq == SCSI_IRQ_NONE)
		SPRINTF("IRQ: None.\n");
	else
		SPRINTF("IRQ: %d.\n", instance->irq);

#ifdef DTC_PUBLIC_RELEASE
	SPRINTF("Highwater I/O busy_spin_counts -- write: %d  read: %d\n", dtc_wmaxi, dtc_maxi);
#endif
#ifdef PAS16_PUBLIC_RELEASE
	SPRINTF("Highwater I/O busy_spin_counts -- write: %d  read: %d\n", pas_wmaxi, pas_maxi);
#endif
	spin_lock_irq(instance->host_lock);
	if (!hostdata->connected)
		SPRINTF("scsi%d: no currently connected command\n", instance->host_no);
	else
		pos = lprint_Scsi_Cmnd((Scsi_Cmnd *) hostdata->connected, pos, buffer, length);
	SPRINTF("scsi%d: issue_queue\n", instance->host_no);
	for (ptr = (Scsi_Cmnd *) hostdata->issue_queue; ptr; ptr = (Scsi_Cmnd *) ptr->host_scribble)
		pos = lprint_Scsi_Cmnd(ptr, pos, buffer, length);

	SPRINTF("scsi%d: disconnected_queue\n", instance->host_no);
	for (ptr = (Scsi_Cmnd *) hostdata->disconnected_queue; ptr; ptr = (Scsi_Cmnd *) ptr->host_scribble)
		pos = lprint_Scsi_Cmnd(ptr, pos, buffer, length);
	spin_unlock_irq(instance->host_lock);
	
	*start = buffer;
	if (pos - buffer < offset)
		return 0;
	else if (pos - buffer - offset < length)
		return pos - buffer - offset;
	return length;
}

static char *lprint_Scsi_Cmnd(Scsi_Cmnd * cmd, char *pos, char *buffer, int length)
{
	SPRINTF("scsi%d : destination target %d, lun %d\n", cmd->device->host->host_no, cmd->device->id, cmd->device->lun);
	SPRINTF("        command = ");
	pos = lprint_command(cmd->cmnd, pos, buffer, length);
	return (pos);
}

static char *lprint_command(unsigned char *command, char *pos, char *buffer, int length)
{
	int i, s;
	pos = lprint_opcode(command[0], pos, buffer, length);
	for (i = 1, s = COMMAND_SIZE(command[0]); i < s; ++i)
		SPRINTF("%02x ", command[i]);
	SPRINTF("\n");
	return (pos);
}

static char *lprint_opcode(int opcode, char *pos, char *buffer, int length)
{
	SPRINTF("%2d (0x%02x)", opcode, opcode);
	return (pos);
}



static int __devinit NCR5380_init(struct Scsi_Host *instance, int flags)
{
	NCR5380_local_declare();
	int i, pass;
	unsigned long timeout;
	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *) instance->hostdata;

	if(in_interrupt())
		printk(KERN_ERR "NCR5380_init called with interrupts off!\n");

#ifdef NCR53C400
	if (flags & FLAG_NCR53C400)
		instance->NCR5380_instance_name += NCR53C400_address_adjust;
#endif

	NCR5380_setup(instance);

	hostdata->aborted = 0;
	hostdata->id_mask = 1 << instance->this_id;
	for (i = hostdata->id_mask; i <= 0x80; i <<= 1)
		if (i > hostdata->id_mask)
			hostdata->id_higher_mask |= i;
	for (i = 0; i < 8; ++i)
		hostdata->busy[i] = 0;
#ifdef REAL_DMA
	hostdata->dmalen = 0;
#endif
	hostdata->targets_present = 0;
	hostdata->connected = NULL;
	hostdata->issue_queue = NULL;
	hostdata->disconnected_queue = NULL;
	
	INIT_DELAYED_WORK(&hostdata->coroutine, NCR5380_main);
	
#ifdef NCR5380_STATS
	for (i = 0; i < 8; ++i) {
		hostdata->time_read[i] = 0;
		hostdata->time_write[i] = 0;
		hostdata->bytes_read[i] = 0;
		hostdata->bytes_write[i] = 0;
	}
	hostdata->timebase = 0;
	hostdata->pendingw = 0;
	hostdata->pendingr = 0;
#endif

	
	if (flags & FLAG_NCR53C400)
		hostdata->flags = FLAG_HAS_LAST_BYTE_SENT | flags;
	else
		hostdata->flags = FLAG_CHECK_LAST_BYTE_SENT | flags;

	hostdata->host = instance;
	hostdata->time_expires = 0;

#ifndef AUTOSENSE
	if ((instance->cmd_per_lun > 1) || instance->can_queue > 1)
		    printk(KERN_WARNING "scsi%d : WARNING : support for multiple outstanding commands enabled\n" "         without AUTOSENSE option, contingent allegiance conditions may\n"
		    	   "         be incorrectly cleared.\n", instance->host_no);
#endif				

	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	NCR5380_write(MODE_REG, MR_BASE);
	NCR5380_write(TARGET_COMMAND_REG, 0);
	NCR5380_write(SELECT_ENABLE_REG, 0);

#ifdef NCR53C400
	if (hostdata->flags & FLAG_NCR53C400) {
		NCR5380_write(C400_CONTROL_STATUS_REG, CSR_BASE);
	}
#endif


	for (pass = 1; (NCR5380_read(STATUS_REG) & SR_BSY) && pass <= 6; ++pass) {
		switch (pass) {
		case 1:
		case 3:
		case 5:
			printk(KERN_INFO "scsi%d: SCSI bus busy, waiting up to five seconds\n", instance->host_no);
			timeout = jiffies + 5 * HZ;
			NCR5380_poll_politely(instance, STATUS_REG, SR_BSY, 0, 5*HZ);
			break;
		case 2:
			printk(KERN_WARNING "scsi%d: bus busy, attempting abort\n", instance->host_no);
			do_abort(instance);
			break;
		case 4:
			printk(KERN_WARNING "scsi%d: bus busy, attempting reset\n", instance->host_no);
			do_reset(instance);
			break;
		case 6:
			printk(KERN_ERR "scsi%d: bus locked solid or invalid override\n", instance->host_no);
			return -ENXIO;
		}
	}
	return 0;
}


static void NCR5380_exit(struct Scsi_Host *instance)
{
	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *) instance->hostdata;

	cancel_delayed_work_sync(&hostdata->coroutine);
}


static int NCR5380_queue_command_lck(Scsi_Cmnd * cmd, void (*done) (Scsi_Cmnd *))
{
	struct Scsi_Host *instance = cmd->device->host;
	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *) instance->hostdata;
	Scsi_Cmnd *tmp;

#if (NDEBUG & NDEBUG_NO_WRITE)
	switch (cmd->cmnd[0]) {
	case WRITE_6:
	case WRITE_10:
		printk("scsi%d : WRITE attempted with NO_WRITE debugging flag set\n", instance->host_no);
		cmd->result = (DID_ERROR << 16);
		done(cmd);
		return 0;
	}
#endif				

#ifdef NCR5380_STATS
	switch (cmd->cmnd[0]) {
		case WRITE:
		case WRITE_6:
		case WRITE_10:
			hostdata->time_write[cmd->device->id] -= (jiffies - hostdata->timebase);
			hostdata->bytes_write[cmd->device->id] += scsi_bufflen(cmd);
			hostdata->pendingw++;
			break;
		case READ:
		case READ_6:
		case READ_10:
			hostdata->time_read[cmd->device->id] -= (jiffies - hostdata->timebase);
			hostdata->bytes_read[cmd->device->id] += scsi_bufflen(cmd);
			hostdata->pendingr++;
			break;
	}
#endif


	cmd->host_scribble = NULL;
	cmd->scsi_done = done;
	cmd->result = 0;


	if (!(hostdata->issue_queue) || (cmd->cmnd[0] == REQUEST_SENSE)) {
		LIST(cmd, hostdata->issue_queue);
		cmd->host_scribble = (unsigned char *) hostdata->issue_queue;
		hostdata->issue_queue = cmd;
	} else {
		for (tmp = (Scsi_Cmnd *) hostdata->issue_queue; tmp->host_scribble; tmp = (Scsi_Cmnd *) tmp->host_scribble);
		LIST(cmd, tmp);
		tmp->host_scribble = (unsigned char *) cmd;
	}
	dprintk(NDEBUG_QUEUES, ("scsi%d : command added to %s of queue\n", instance->host_no, (cmd->cmnd[0] == REQUEST_SENSE) ? "head" : "tail"));

	
	
	schedule_delayed_work(&hostdata->coroutine, 0);
	return 0;
}

static DEF_SCSI_QCMD(NCR5380_queue_command)


static void NCR5380_main(struct work_struct *work)
{
	struct NCR5380_hostdata *hostdata =
		container_of(work, struct NCR5380_hostdata, coroutine.work);
	struct Scsi_Host *instance = hostdata->host;
	Scsi_Cmnd *tmp, *prev;
	int done;
	
	spin_lock_irq(instance->host_lock);
	do {
		
		done = 1;
		if (!hostdata->connected && !hostdata->selecting) {
			dprintk(NDEBUG_MAIN, ("scsi%d : not connected\n", instance->host_no));
			for (tmp = (Scsi_Cmnd *) hostdata->issue_queue, prev = NULL; tmp; prev = tmp, tmp = (Scsi_Cmnd *) tmp->host_scribble) 
			{
				if (prev != tmp)
					dprintk(NDEBUG_LISTS, ("MAIN tmp=%p   target=%d   busy=%d lun=%d\n", tmp, tmp->target, hostdata->busy[tmp->target], tmp->lun));
				
				if (!(hostdata->busy[tmp->device->id] & (1 << tmp->device->lun))) {
					if (prev) {
						REMOVE(prev, prev->host_scribble, tmp, tmp->host_scribble);
						prev->host_scribble = tmp->host_scribble;
					} else {
						REMOVE(-1, hostdata->issue_queue, tmp, tmp->host_scribble);
						hostdata->issue_queue = (Scsi_Cmnd *) tmp->host_scribble;
					}
					tmp->host_scribble = NULL;

					dprintk(NDEBUG_MAIN|NDEBUG_QUEUES, ("scsi%d : main() : command for target %d lun %d removed from issue_queue\n", instance->host_no, tmp->target, tmp->lun));
	
					hostdata->selecting = NULL;
					

					if (!NCR5380_select(instance, tmp,
							    (tmp->cmnd[0] == REQUEST_SENSE) ? TAG_NONE : TAG_NEXT)) {
						break;
					} else {
						LIST(tmp, hostdata->issue_queue);
						tmp->host_scribble = (unsigned char *) hostdata->issue_queue;
						hostdata->issue_queue = tmp;
						done = 0;
						dprintk(NDEBUG_MAIN|NDEBUG_QUEUES, ("scsi%d : main(): select() failed, returned to issue_queue\n", instance->host_no));
					}
					
				}	
			}	
			
		}	
		if (hostdata->selecting) {
			tmp = (Scsi_Cmnd *) hostdata->selecting;
			
			if (!NCR5380_select(instance, tmp, (tmp->cmnd[0] == REQUEST_SENSE) ? TAG_NONE : TAG_NEXT)) {
				
			} else {
				printk(KERN_DEBUG "scsi%d: device %d did not respond in time\n", instance->host_no, tmp->device->id);
				LIST(tmp, hostdata->issue_queue);
				tmp->host_scribble = (unsigned char *) hostdata->issue_queue;
				hostdata->issue_queue = tmp;
				NCR5380_set_timer(hostdata, USLEEP_WAITLONG);
			}
		}	
		if (hostdata->connected
#ifdef REAL_DMA
		    && !hostdata->dmalen
#endif
		    && (!hostdata->time_expires || time_before_eq(hostdata->time_expires, jiffies))
		    ) {
			dprintk(NDEBUG_MAIN, ("scsi%d : main() : performing information transfer\n", instance->host_no));
			NCR5380_information_transfer(instance);
			dprintk(NDEBUG_MAIN, ("scsi%d : main() : done set false\n", instance->host_no));
			done = 0;
		} else
			break;
	} while (!done);
	
	spin_unlock_irq(instance->host_lock);
}

#ifndef DONT_USE_INTR


static irqreturn_t NCR5380_intr(int dummy, void *dev_id)
{
	NCR5380_local_declare();
	struct Scsi_Host *instance = dev_id;
	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *) instance->hostdata;
	int done;
	unsigned char basr;
	unsigned long flags;

	dprintk(NDEBUG_INTR, ("scsi : NCR5380 irq %d triggered\n",
		instance->irq));

	do {
		done = 1;
		spin_lock_irqsave(instance->host_lock, flags);
		
		NCR5380_setup(instance);
		basr = NCR5380_read(BUS_AND_STATUS_REG);
		
		if (basr & BASR_IRQ) {
			NCR5380_dprint(NDEBUG_INTR, instance);
			if ((NCR5380_read(STATUS_REG) & (SR_SEL | SR_IO)) == (SR_SEL | SR_IO)) {
				done = 0;
				dprintk(NDEBUG_INTR, ("scsi%d : SEL interrupt\n", instance->host_no));
				NCR5380_reselect(instance);
				(void) NCR5380_read(RESET_PARITY_INTERRUPT_REG);
			} else if (basr & BASR_PARITY_ERROR) {
				dprintk(NDEBUG_INTR, ("scsi%d : PARITY interrupt\n", instance->host_no));
				(void) NCR5380_read(RESET_PARITY_INTERRUPT_REG);
			} else if ((NCR5380_read(STATUS_REG) & SR_RST) == SR_RST) {
				dprintk(NDEBUG_INTR, ("scsi%d : RESET interrupt\n", instance->host_no));
				(void) NCR5380_read(RESET_PARITY_INTERRUPT_REG);
			} else {
#if defined(REAL_DMA)

				if ((NCR5380_read(MODE_REG) & MR_DMA) && ((basr & BASR_END_DMA_TRANSFER) || !(basr & BASR_PHASE_MATCH))) {
					int transferred;

					if (!hostdata->connected)
						panic("scsi%d : received end of DMA interrupt with no connected cmd\n", instance->hostno);

					transferred = (hostdata->dmalen - NCR5380_dma_residual(instance));
					hostdata->connected->SCp.this_residual -= transferred;
					hostdata->connected->SCp.ptr += transferred;
					hostdata->dmalen = 0;

					(void) NCR5380_read(RESET_PARITY_INTERRUPT_REG);
							
					
					NCR5380_poll_politely(hostdata, BUS_AND_STATUS_REG, BASR_ACK, 0, 2*HZ);

					NCR5380_write(MODE_REG, MR_BASE);
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
				}
#else
				dprintk(NDEBUG_INTR, ("scsi : unknown interrupt, BASR 0x%X, MR 0x%X, SR 0x%x\n", basr, NCR5380_read(MODE_REG), NCR5380_read(STATUS_REG)));
				(void) NCR5380_read(RESET_PARITY_INTERRUPT_REG);
#endif
			}
		}	
		spin_unlock_irqrestore(instance->host_lock, flags);
		if(!done)
			schedule_delayed_work(&hostdata->coroutine, 0);
	} while (!done);
	return IRQ_HANDLED;
}

#endif 

 
static void collect_stats(struct NCR5380_hostdata *hostdata, Scsi_Cmnd * cmd) 
{
#ifdef NCR5380_STATS
	switch (cmd->cmnd[0]) {
	case WRITE:
	case WRITE_6:
	case WRITE_10:
		hostdata->time_write[scmd_id(cmd)] += (jiffies - hostdata->timebase);
		hostdata->pendingw--;
		break;
	case READ:
	case READ_6:
	case READ_10:
		hostdata->time_read[scmd_id(cmd)] += (jiffies - hostdata->timebase);
		hostdata->pendingr--;
		break;
	}
#endif
}


 
static int NCR5380_select(struct Scsi_Host *instance, Scsi_Cmnd * cmd, int tag) 
{
	NCR5380_local_declare();
	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *) instance->hostdata;
	unsigned char tmp[3], phase;
	unsigned char *data;
	int len;
	unsigned long timeout;
	unsigned char value;
	int err;
	NCR5380_setup(instance);

	if (hostdata->selecting)
		goto part2;

	hostdata->restart_select = 0;

	NCR5380_dprint(NDEBUG_ARBITRATION, instance);
	dprintk(NDEBUG_ARBITRATION, ("scsi%d : starting arbitration, id = %d\n", instance->host_no, instance->this_id));


	NCR5380_write(TARGET_COMMAND_REG, 0);


	NCR5380_write(OUTPUT_DATA_REG, hostdata->id_mask);
	NCR5380_write(MODE_REG, MR_ARBITRATE);


	spin_unlock_irq(instance->host_lock);
	err = NCR5380_poll_politely(instance, INITIATOR_COMMAND_REG, ICR_ARBITRATION_PROGRESS, ICR_ARBITRATION_PROGRESS, 5*HZ);
	spin_lock_irq(instance->host_lock);
	if (err < 0) {
		printk(KERN_DEBUG "scsi: arbitration timeout at %d\n", __LINE__);
		NCR5380_write(MODE_REG, MR_BASE);
		NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
		goto failed;
	}

	dprintk(NDEBUG_ARBITRATION, ("scsi%d : arbitration complete\n", instance->host_no));


	udelay(3);

	
	if ((NCR5380_read(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_LOST) || (NCR5380_read(CURRENT_SCSI_DATA_REG) & hostdata->id_higher_mask) || (NCR5380_read(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_LOST)) {
		NCR5380_write(MODE_REG, MR_BASE);
		dprintk(NDEBUG_ARBITRATION, ("scsi%d : lost arbitration, deasserting MR_ARBITRATE\n", instance->host_no));
		goto failed;
	}
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_SEL);

	if (!(hostdata->flags & FLAG_DTC3181E) &&
	    (NCR5380_read(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_LOST)) {
		NCR5380_write(MODE_REG, MR_BASE);
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		dprintk(NDEBUG_ARBITRATION, ("scsi%d : lost arbitration, deasserting ICR_ASSERT_SEL\n", instance->host_no));
		goto failed;
	}

	udelay(2);

	dprintk(NDEBUG_ARBITRATION, ("scsi%d : won arbitration\n", instance->host_no));


	NCR5380_write(OUTPUT_DATA_REG, (hostdata->id_mask | (1 << scmd_id(cmd))));


	NCR5380_write(INITIATOR_COMMAND_REG, (ICR_BASE | ICR_ASSERT_BSY | ICR_ASSERT_DATA | ICR_ASSERT_ATN | ICR_ASSERT_SEL));
	NCR5380_write(MODE_REG, MR_BASE);

	NCR5380_write(SELECT_ENABLE_REG, 0);

	udelay(1);		

	
	NCR5380_write(INITIATOR_COMMAND_REG, (ICR_BASE | ICR_ASSERT_DATA | ICR_ASSERT_ATN | ICR_ASSERT_SEL));


	udelay(1);

	dprintk(NDEBUG_SELECTION, ("scsi%d : selecting target %d\n", instance->host_no, scmd_id(cmd)));


	timeout = jiffies + (250 * HZ / 1000);


	hostdata->select_time = 0;	
	hostdata->selecting = cmd;

part2:
	value = NCR5380_read(STATUS_REG) & (SR_BSY | SR_IO);

	if (!value && (hostdata->select_time < HZ/4)) {
		
		hostdata->select_time++;	
		NCR5380_set_timer(hostdata, 1);
		return 0;	
	}

	hostdata->selecting = NULL;
	if ((NCR5380_read(STATUS_REG) & (SR_SEL | SR_IO)) == (SR_SEL | SR_IO)) {
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		NCR5380_reselect(instance);
		printk("scsi%d : reselection after won arbitration?\n", instance->host_no);
		NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
		return -1;
	}

	udelay(1);

	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);

	if (!(NCR5380_read(STATUS_REG) & SR_BSY)) {
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		if (hostdata->targets_present & (1 << scmd_id(cmd))) {
			printk(KERN_DEBUG "scsi%d : weirdness\n", instance->host_no);
			if (hostdata->restart_select)
				printk(KERN_DEBUG "\trestart select\n");
			NCR5380_dprint(NDEBUG_SELECTION, instance);
			NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
			return -1;
		}
		cmd->result = DID_BAD_TARGET << 16;
		collect_stats(hostdata, cmd);
		cmd->scsi_done(cmd);
		NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
		dprintk(NDEBUG_SELECTION, ("scsi%d : target did not respond within 250ms\n", instance->host_no));
		NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
		return 0;
	}
	hostdata->targets_present |= (1 << scmd_id(cmd));


	

	spin_unlock_irq(instance->host_lock);
	err = NCR5380_poll_politely(instance, STATUS_REG, SR_REQ, SR_REQ, HZ);
	spin_lock_irq(instance->host_lock);
	
	if(err) {
		printk(KERN_ERR "scsi%d: timeout at NCR5380.c:%d\n", instance->host_no, __LINE__);
		NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
		goto failed;
	}

	dprintk(NDEBUG_SELECTION, ("scsi%d : target %d selected, going into MESSAGE OUT phase.\n", instance->host_no, cmd->device->id));
	tmp[0] = IDENTIFY(((instance->irq == SCSI_IRQ_NONE) ? 0 : 1), cmd->device->lun);

	len = 1;
	cmd->tag = 0;

	
	data = tmp;
	phase = PHASE_MSGOUT;
	NCR5380_transfer_pio(instance, &phase, &len, &data);
	dprintk(NDEBUG_SELECTION, ("scsi%d : nexus established.\n", instance->host_no));
	
	hostdata->connected = cmd;
	hostdata->busy[cmd->device->id] |= (1 << cmd->device->lun);

	initialize_SCp(cmd);

	return 0;

	
failed:
	return -1;

}



static int NCR5380_transfer_pio(struct Scsi_Host *instance, unsigned char *phase, int *count, unsigned char **data) {
	NCR5380_local_declare();
	unsigned char p = *phase, tmp;
	int c = *count;
	unsigned char *d = *data;
	int break_allowed = 0;
	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *) instance->hostdata;
	NCR5380_setup(instance);

	if (!(p & SR_IO))
		dprintk(NDEBUG_PIO, ("scsi%d : pio write %d bytes\n", instance->host_no, c));
	else
		dprintk(NDEBUG_PIO, ("scsi%d : pio read %d bytes\n", instance->host_no, c));


	 NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(p));

	if ((p == PHASE_DATAIN) || (p == PHASE_DATAOUT)) {
		break_allowed = 1;
	}
	do {


		
		while (!((tmp = NCR5380_read(STATUS_REG)) & SR_REQ) && !break_allowed);
		if (!(tmp & SR_REQ)) {
			
			NCR5380_set_timer(hostdata, USLEEP_SLEEP);
			break;
		}

		dprintk(NDEBUG_HANDSHAKE, ("scsi%d : REQ detected\n", instance->host_no));

		
		if ((tmp & PHASE_MASK) != p) {
			dprintk(NDEBUG_HANDSHAKE, ("scsi%d : phase mismatch\n", instance->host_no));
			NCR5380_dprint_phase(NDEBUG_HANDSHAKE, instance);
			break;
		}
		
		if (!(p & SR_IO))
			NCR5380_write(OUTPUT_DATA_REG, *d);
		else
			*d = NCR5380_read(CURRENT_SCSI_DATA_REG);

		++d;


		if (!(p & SR_IO)) {
			if (!((p & SR_MSG) && c > 1)) {
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA);
				NCR5380_dprint(NDEBUG_PIO, instance);
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA | ICR_ASSERT_ACK);
			} else {
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA | ICR_ASSERT_ATN);
				NCR5380_dprint(NDEBUG_PIO, instance);
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA | ICR_ASSERT_ATN | ICR_ASSERT_ACK);
			}
		} else {
			NCR5380_dprint(NDEBUG_PIO, instance);
			NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ACK);
		}

		
		NCR5380_poll_politely(instance, STATUS_REG, SR_REQ, 0, 5*HZ);
		dprintk(NDEBUG_HANDSHAKE, ("scsi%d : req false, handshake complete\n", instance->host_no));

		if (!(p == PHASE_MSGIN && c == 1)) {
			if (p == PHASE_MSGOUT && c > 1)
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
			else
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		}
	} while (--c);

	dprintk(NDEBUG_PIO, ("scsi%d : residual %d\n", instance->host_no, c));

	*count = c;
	*data = d;
	tmp = NCR5380_read(STATUS_REG);
	if (tmp & SR_REQ)
		*phase = tmp & PHASE_MASK;
	else
		*phase = PHASE_UNKNOWN;

	if (!c || (*phase == p))
		return 0;
	else
		return -1;
}

 
static void do_reset(struct Scsi_Host *host) {
	NCR5380_local_declare();
	NCR5380_setup(host);

	NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(NCR5380_read(STATUS_REG) & PHASE_MASK));
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_RST);
	udelay(25);
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
}


static int do_abort(struct Scsi_Host *host) {
	NCR5380_local_declare();
	unsigned char *msgptr, phase, tmp;
	int len;
	int rc;
	NCR5380_setup(host);


	
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);


	rc = NCR5380_poll_politely(host, STATUS_REG, SR_REQ, SR_REQ, 60 * HZ);
	
	if(rc < 0)
		return -1;

	tmp = (unsigned char)rc;
	
	NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(tmp));

	if ((tmp & PHASE_MASK) != PHASE_MSGOUT) {
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN | ICR_ASSERT_ACK);
		rc = NCR5380_poll_politely(host, STATUS_REG, SR_REQ, 0, 3*HZ);
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
		if(rc == -1)
			return -1;
	}
	tmp = ABORT;
	msgptr = &tmp;
	len = 1;
	phase = PHASE_MSGOUT;
	NCR5380_transfer_pio(host, &phase, &len, &msgptr);


	return len ? -1 : 0;
}

#if defined(REAL_DMA) || defined(PSEUDO_DMA) || defined (REAL_DMA_POLL)


static int NCR5380_transfer_dma(struct Scsi_Host *instance, unsigned char *phase, int *count, unsigned char **data) {
	NCR5380_local_declare();
	register int c = *count;
	register unsigned char p = *phase;
	register unsigned char *d = *data;
	unsigned char tmp;
	int foo;
#if defined(REAL_DMA_POLL)
	int cnt, toPIO;
	unsigned char saved_data = 0, overrun = 0, residue;
#endif

	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *) instance->hostdata;

	NCR5380_setup(instance);

	if ((tmp = (NCR5380_read(STATUS_REG) & PHASE_MASK)) != p) {
		*phase = tmp;
		return -1;
	}
#if defined(REAL_DMA) || defined(REAL_DMA_POLL)
#ifdef READ_OVERRUNS
	if (p & SR_IO) {
		c -= 2;
	}
#endif
	dprintk(NDEBUG_DMA, ("scsi%d : initializing DMA channel %d for %s, %d bytes %s %0x\n", instance->host_no, instance->dma_channel, (p & SR_IO) ? "reading" : "writing", c, (p & SR_IO) ? "to" : "from", (unsigned) d));
	hostdata->dma_len = (p & SR_IO) ? NCR5380_dma_read_setup(instance, d, c) : NCR5380_dma_write_setup(instance, d, c);
#endif

	NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(p));

#ifdef REAL_DMA
	NCR5380_write(MODE_REG, MR_BASE | MR_DMA_MODE | MR_ENABLE_EOP_INTR | MR_MONITOR_BSY);
#elif defined(REAL_DMA_POLL)
	NCR5380_write(MODE_REG, MR_BASE | MR_DMA_MODE);
#else

#if defined(PSEUDO_DMA) && defined(UNSAFE)
	spin_unlock_irq(instance->host_lock);
#endif
	
	if (hostdata->flags & FLAG_NCR53C400)
		NCR5380_write(MODE_REG, MR_BASE | MR_DMA_MODE |
				MR_ENABLE_PAR_CHECK | MR_ENABLE_PAR_INTR |
				MR_ENABLE_EOP_INTR | MR_MONITOR_BSY);
	else
		NCR5380_write(MODE_REG, MR_BASE | MR_DMA_MODE);
#endif				

	dprintk(NDEBUG_DMA, ("scsi%d : mode reg = 0x%X\n", instance->host_no, NCR5380_read(MODE_REG)));


	if (p & SR_IO) {
		io_recovery_delay(1);
		NCR5380_write(START_DMA_INITIATOR_RECEIVE_REG, 0);
	} else {
		io_recovery_delay(1);
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_DATA);
		io_recovery_delay(1);
		NCR5380_write(START_DMA_SEND_REG, 0);
		io_recovery_delay(1);
	}

#if defined(REAL_DMA_POLL)
	do {
		tmp = NCR5380_read(BUS_AND_STATUS_REG);
	} while ((tmp & BASR_PHASE_MATCH) && !(tmp & (BASR_BUSY_ERROR | BASR_END_DMA_TRANSFER)));


	if (p & SR_IO) {
#ifdef READ_OVERRUNS
		udelay(10);
		if (((NCR5380_read(BUS_AND_STATUS_REG) & (BASR_PHASE_MATCH | BASR_ACK)) == (BASR_PHASE_MATCH | BASR_ACK))) {
			saved_data = NCR5380_read(INPUT_DATA_REGISTER);
			overrun = 1;
		}
#endif
	} else {
		int limit = 100;
		while (((tmp = NCR5380_read(BUS_AND_STATUS_REG)) & BASR_ACK) || (NCR5380_read(STATUS_REG) & SR_REQ)) {
			if (!(tmp & BASR_PHASE_MATCH))
				break;
			if (--limit < 0)
				break;
		}
	}

	dprintk(NDEBUG_DMA, ("scsi%d : polled DMA transfer complete, basr 0x%X, sr 0x%X\n", instance->host_no, tmp, NCR5380_read(STATUS_REG)));

	NCR5380_write(MODE_REG, MR_BASE);
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);

	residue = NCR5380_dma_residual(instance);
	c -= residue;
	*count -= c;
	*data += c;
	*phase = NCR5380_read(STATUS_REG) & PHASE_MASK;

#ifdef READ_OVERRUNS
	if (*phase == p && (p & SR_IO) && residue == 0) {
		if (overrun) {
			dprintk(NDEBUG_DMA, ("Got an input overrun, using saved byte\n"));
			**data = saved_data;
			*data += 1;
			*count -= 1;
			cnt = toPIO = 1;
		} else {
			printk("No overrun??\n");
			cnt = toPIO = 2;
		}
		dprintk(NDEBUG_DMA, ("Doing %d-byte PIO to 0x%X\n", cnt, *data));
		NCR5380_transfer_pio(instance, phase, &cnt, data);
		*count -= toPIO - cnt;
	}
#endif

	dprintk(NDEBUG_DMA, ("Return with data ptr = 0x%X, count %d, last 0x%X, next 0x%X\n", *data, *count, *(*data + *count - 1), *(*data + *count)));
	return 0;

#elif defined(REAL_DMA)
	return 0;
#else				
	if (p & SR_IO) {
#ifdef DMA_WORKS_RIGHT
		foo = NCR5380_pread(instance, d, c);
#else
		int diff = 1;
		if (hostdata->flags & FLAG_NCR53C400) {
			diff = 0;
		}
		if (!(foo = NCR5380_pread(instance, d, c - diff))) {

			if (!(hostdata->flags & FLAG_NCR53C400)) {
				while (!(NCR5380_read(BUS_AND_STATUS_REG) & BASR_DRQ));
				
				while (NCR5380_read(STATUS_REG) & SR_REQ);
				d[c - 1] = NCR5380_read(INPUT_DATA_REG);
			}
		}
#endif
	} else {
#ifdef DMA_WORKS_RIGHT
		foo = NCR5380_pwrite(instance, d, c);
#else
		int timeout;
		dprintk(NDEBUG_C400_PWRITE, ("About to pwrite %d bytes\n", c));
		if (!(foo = NCR5380_pwrite(instance, d, c))) {
			if (!(hostdata->flags & FLAG_HAS_LAST_BYTE_SENT)) {
				timeout = 20000;
				while (!(NCR5380_read(BUS_AND_STATUS_REG) & BASR_DRQ) && (NCR5380_read(BUS_AND_STATUS_REG) & BASR_PHASE_MATCH));

				if (!timeout)
					dprintk(NDEBUG_LAST_BYTE_SENT, ("scsi%d : timed out on last byte\n", instance->host_no));

				if (hostdata->flags & FLAG_CHECK_LAST_BYTE_SENT) {
					hostdata->flags &= ~FLAG_CHECK_LAST_BYTE_SENT;
					if (NCR5380_read(TARGET_COMMAND_REG) & TCR_LAST_BYTE_SENT) {
						hostdata->flags |= FLAG_HAS_LAST_BYTE_SENT;
						dprintk(NDEBUG_LAST_WRITE_SENT, ("scsi%d : last bit sent works\n", instance->host_no));
					}
				}
			} else {
				dprintk(NDEBUG_C400_PWRITE, ("Waiting for LASTBYTE\n"));
				while (!(NCR5380_read(TARGET_COMMAND_REG) & TCR_LAST_BYTE_SENT));
				dprintk(NDEBUG_C400_PWRITE, ("Got LASTBYTE\n"));
			}
		}
#endif
	}
	NCR5380_write(MODE_REG, MR_BASE);
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);

	if ((!(p & SR_IO)) && (hostdata->flags & FLAG_NCR53C400)) {
		dprintk(NDEBUG_C400_PWRITE, ("53C400w: Checking for IRQ\n"));
		if (NCR5380_read(BUS_AND_STATUS_REG) & BASR_IRQ) {
			dprintk(NDEBUG_C400_PWRITE, ("53C400w:    got it, reading reset interrupt reg\n"));
			NCR5380_read(RESET_PARITY_INTERRUPT_REG);
		} else {
			printk("53C400w:    IRQ NOT THERE!\n");
		}
	}
	*data = d + c;
	*count = 0;
	*phase = NCR5380_read(STATUS_REG) & PHASE_MASK;
#if defined(PSEUDO_DMA) && defined(UNSAFE)
	spin_lock_irq(instance->host_lock);
#endif				
	return foo;
#endif				
}
#endif				


static void NCR5380_information_transfer(struct Scsi_Host *instance) {
	NCR5380_local_declare();
	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *)instance->hostdata;
	unsigned char msgout = NOP;
	int sink = 0;
	int len;
#if defined(PSEUDO_DMA) || defined(REAL_DMA_POLL)
	int transfersize;
#endif
	unsigned char *data;
	unsigned char phase, tmp, extended_msg[10], old_phase = 0xff;
	Scsi_Cmnd *cmd = (Scsi_Cmnd *) hostdata->connected;
	
	unsigned long poll_time = jiffies + USLEEP_POLL;

	NCR5380_setup(instance);

	while (1) {
		tmp = NCR5380_read(STATUS_REG);
		
		if (tmp & SR_REQ) {
			phase = (tmp & PHASE_MASK);
			if (phase != old_phase) {
				old_phase = phase;
				NCR5380_dprint_phase(NDEBUG_INFORMATION, instance);
			}
			if (sink && (phase != PHASE_MSGOUT)) {
				NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(tmp));

				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN | ICR_ASSERT_ACK);
				while (NCR5380_read(STATUS_REG) & SR_REQ);
				NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
				sink = 0;
				continue;
			}
			switch (phase) {
			case PHASE_DATAIN:
			case PHASE_DATAOUT:
#if (NDEBUG & NDEBUG_NO_DATAOUT)
				printk("scsi%d : NDEBUG_NO_DATAOUT set, attempted DATAOUT aborted\n", instance->host_no);
				sink = 1;
				do_abort(instance);
				cmd->result = DID_ERROR << 16;
				cmd->scsi_done(cmd);
				return;
#endif

				if (!cmd->SCp.this_residual && cmd->SCp.buffers_residual) {
					++cmd->SCp.buffer;
					--cmd->SCp.buffers_residual;
					cmd->SCp.this_residual = cmd->SCp.buffer->length;
					cmd->SCp.ptr = sg_virt(cmd->SCp.buffer);
					dprintk(NDEBUG_INFORMATION, ("scsi%d : %d bytes and %d buffers left\n", instance->host_no, cmd->SCp.this_residual, cmd->SCp.buffers_residual));
				}

#if defined(PSEUDO_DMA) || defined(REAL_DMA_POLL)
#ifdef NCR5380_dma_xfer_len
				if (!cmd->device->borken && !(hostdata->flags & FLAG_NO_PSEUDO_DMA) && (transfersize = NCR5380_dma_xfer_len(instance, cmd)) != 0) {
#else
				transfersize = cmd->transfersize;

#ifdef LIMIT_TRANSFERSIZE	
				if (transfersize > 512)
					transfersize = 512;
#endif				

				if (!cmd->device->borken && transfersize && !(hostdata->flags & FLAG_NO_PSEUDO_DMA) && cmd->SCp.this_residual && !(cmd->SCp.this_residual % transfersize)) {
					if (transfersize > 32 * 1024)
						transfersize = 32 * 1024;
#endif
					len = transfersize;
					if (NCR5380_transfer_dma(instance, &phase, &len, (unsigned char **) &cmd->SCp.ptr)) {
						scmd_printk(KERN_INFO, cmd,
							    "switching to slow handshake\n");
						cmd->device->borken = 1;
						NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
						sink = 1;
						do_abort(instance);
						cmd->result = DID_ERROR << 16;
						cmd->scsi_done(cmd);
						
					} else
						cmd->SCp.this_residual -= transfersize - len;
				} else
#endif				
					NCR5380_transfer_pio(instance, &phase, (int *) &cmd->SCp.this_residual, (unsigned char **)
							     &cmd->SCp.ptr);
				break;
			case PHASE_MSGIN:
				len = 1;
				data = &tmp;
				NCR5380_transfer_pio(instance, &phase, &len, &data);
				cmd->SCp.Message = tmp;

				switch (tmp) {
#ifdef LINKED
				case LINKED_CMD_COMPLETE:
				case LINKED_FLG_CMD_COMPLETE:
					
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
					dprintk(NDEBUG_LINKED, ("scsi%d : target %d lun %d linked command complete.\n", instance->host_no, cmd->device->id, cmd->device->lun));
					if (!cmd->next_link) {
						printk("scsi%d : target %d lun %d linked command complete, no next_link\n" instance->host_no, cmd->device->id, cmd->device->lun);
						sink = 1;
						do_abort(instance);
						return;
					}
					initialize_SCp(cmd->next_link);
					
					cmd->next_link->tag = cmd->tag;
					cmd->result = cmd->SCp.Status | (cmd->SCp.Message << 8);
					dprintk(NDEBUG_LINKED, ("scsi%d : target %d lun %d linked request done, calling scsi_done().\n", instance->host_no, cmd->device->id, cmd->device->lun));
					collect_stats(hostdata, cmd);
					cmd->scsi_done(cmd);
					cmd = hostdata->connected;
					break;
#endif				
				case ABORT:
				case COMMAND_COMPLETE:
					
					sink = 1;
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
					hostdata->connected = NULL;
					dprintk(NDEBUG_QUEUES, ("scsi%d : command for target %d, lun %d completed\n", instance->host_no, cmd->device->id, cmd->device->lun));
					hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);


					if (cmd->cmnd[0] != REQUEST_SENSE)
						cmd->result = cmd->SCp.Status | (cmd->SCp.Message << 8);
					else if (status_byte(cmd->SCp.Status) != GOOD)
						cmd->result = (cmd->result & 0x00ffff) | (DID_ERROR << 16);

#ifdef AUTOSENSE
					if ((cmd->cmnd[0] == REQUEST_SENSE) &&
						hostdata->ses.cmd_len) {
						scsi_eh_restore_cmnd(cmd, &hostdata->ses);
						hostdata->ses.cmd_len = 0 ;
					}

					if ((cmd->cmnd[0] != REQUEST_SENSE) && (status_byte(cmd->SCp.Status) == CHECK_CONDITION)) {
						scsi_eh_prep_cmnd(cmd, &hostdata->ses, NULL, 0, ~0);

						dprintk(NDEBUG_AUTOSENSE, ("scsi%d : performing request sense\n", instance->host_no));

						LIST(cmd, hostdata->issue_queue);
						cmd->host_scribble = (unsigned char *)
						    hostdata->issue_queue;
						hostdata->issue_queue = (Scsi_Cmnd *) cmd;
						dprintk(NDEBUG_QUEUES, ("scsi%d : REQUEST SENSE added to head of issue queue\n", instance->host_no));
					} else
#endif				
					{
						collect_stats(hostdata, cmd);
						cmd->scsi_done(cmd);
					}

					NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
					NCR5380_write(TARGET_COMMAND_REG, 0);

					while ((NCR5380_read(STATUS_REG) & SR_BSY) && !hostdata->connected)
						barrier();
					return;
				case MESSAGE_REJECT:
					
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
					switch (hostdata->last_message) {
					case HEAD_OF_QUEUE_TAG:
					case ORDERED_QUEUE_TAG:
					case SIMPLE_QUEUE_TAG:
						cmd->device->simple_tags = 0;
						hostdata->busy[cmd->device->id] |= (1 << cmd->device->lun);
						break;
					default:
						break;
					}
				case DISCONNECT:{
						
						NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
						cmd->device->disconnect = 1;
						LIST(cmd, hostdata->disconnected_queue);
						cmd->host_scribble = (unsigned char *)
						    hostdata->disconnected_queue;
						hostdata->connected = NULL;
						hostdata->disconnected_queue = cmd;
						dprintk(NDEBUG_QUEUES, ("scsi%d : command for target %d lun %d was moved from connected to" "  the disconnected_queue\n", instance->host_no, cmd->device->id, cmd->device->lun));
						NCR5380_write(TARGET_COMMAND_REG, 0);

						
						NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
						
						
						while ((NCR5380_read(STATUS_REG) & SR_BSY) && !hostdata->connected)
							barrier();
						return;
					}
				case SAVE_POINTERS:
				case RESTORE_POINTERS:
					
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
					break;
				case EXTENDED_MESSAGE:
					extended_msg[0] = EXTENDED_MESSAGE;
					
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
					dprintk(NDEBUG_EXTENDED, ("scsi%d : receiving extended message\n", instance->host_no));

					len = 2;
					data = extended_msg + 1;
					phase = PHASE_MSGIN;
					NCR5380_transfer_pio(instance, &phase, &len, &data);

					dprintk(NDEBUG_EXTENDED, ("scsi%d : length=%d, code=0x%02x\n", instance->host_no, (int) extended_msg[1], (int) extended_msg[2]));

					if (!len && extended_msg[1] <= (sizeof(extended_msg) - 1)) {
						
						NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
						len = extended_msg[1] - 1;
						data = extended_msg + 3;
						phase = PHASE_MSGIN;

						NCR5380_transfer_pio(instance, &phase, &len, &data);
						dprintk(NDEBUG_EXTENDED, ("scsi%d : message received, residual %d\n", instance->host_no, len));

						switch (extended_msg[2]) {
						case EXTENDED_SDTR:
						case EXTENDED_WDTR:
						case EXTENDED_MODIFY_DATA_POINTER:
						case EXTENDED_EXTENDED_IDENTIFY:
							tmp = 0;
						}
					} else if (len) {
						printk("scsi%d: error receiving extended message\n", instance->host_no);
						tmp = 0;
					} else {
						printk("scsi%d: extended message code %02x length %d is too long\n", instance->host_no, extended_msg[2], extended_msg[1]);
						tmp = 0;
					}
					

				default:
					if (!tmp) {
						printk("scsi%d: rejecting message ", instance->host_no);
						spi_print_msg(extended_msg);
						printk("\n");
					} else if (tmp != EXTENDED_MESSAGE)
						scmd_printk(KERN_INFO, cmd,
							"rejecting unknown message %02x\n",tmp);
					else
						scmd_printk(KERN_INFO, cmd,
							"rejecting unknown extended message code %02x, length %d\n", extended_msg[1], extended_msg[0]);

					msgout = MESSAGE_REJECT;
					NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
					break;
				}	
				break;
			case PHASE_MSGOUT:
				len = 1;
				data = &msgout;
				hostdata->last_message = msgout;
				NCR5380_transfer_pio(instance, &phase, &len, &data);
				if (msgout == ABORT) {
					hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
					hostdata->connected = NULL;
					cmd->result = DID_ERROR << 16;
					collect_stats(hostdata, cmd);
					cmd->scsi_done(cmd);
					NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
					return;
				}
				msgout = NOP;
				break;
			case PHASE_CMDOUT:
				len = cmd->cmd_len;
				data = cmd->cmnd;
				NCR5380_transfer_pio(instance, &phase, &len, &data);
				if (!cmd->device->disconnect && should_disconnect(cmd->cmnd[0])) {
					NCR5380_set_timer(hostdata, USLEEP_SLEEP);
					dprintk(NDEBUG_USLEEP, ("scsi%d : issued command, sleeping until %ul\n", instance->host_no, hostdata->time_expires));
					return;
				}
				break;
			case PHASE_STATIN:
				len = 1;
				data = &tmp;
				NCR5380_transfer_pio(instance, &phase, &len, &data);
				cmd->SCp.Status = tmp;
				break;
			default:
				printk("scsi%d : unknown phase\n", instance->host_no);
				NCR5380_dprint(NDEBUG_ALL, instance);
			}	
		}		
		else {
			if (!cmd->device->disconnect && time_after_eq(jiffies, poll_time)) {
				NCR5380_set_timer(hostdata, USLEEP_SLEEP);
				dprintk(NDEBUG_USLEEP, ("scsi%d : poll timed out, sleeping until %ul\n", instance->host_no, hostdata->time_expires));
				return;
			}
		}
	}			
}


static void NCR5380_reselect(struct Scsi_Host *instance) {
	NCR5380_local_declare();
	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *)
	 instance->hostdata;
	unsigned char target_mask;
	unsigned char lun, phase;
	int len;
	unsigned char msg[3];
	unsigned char *data;
	Scsi_Cmnd *tmp = NULL, *prev;
	int abort = 0;
	NCR5380_setup(instance);


	NCR5380_write(MODE_REG, MR_BASE);
	hostdata->restart_select = 1;

	target_mask = NCR5380_read(CURRENT_SCSI_DATA_REG) & ~(hostdata->id_mask);
	dprintk(NDEBUG_SELECTION, ("scsi%d : reselect\n", instance->host_no));


	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_BSY);

		
	if(NCR5380_poll_politely(instance, STATUS_REG, SR_SEL, 0, 2*HZ)<0)
		abort = 1;
		
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);


	if(NCR5380_poll_politely(instance, STATUS_REG, SR_REQ, SR_REQ, 2*HZ))
		abort = 1;

	len = 1;
	data = msg;
	phase = PHASE_MSGIN;
	NCR5380_transfer_pio(instance, &phase, &len, &data);

	if (!(msg[0] & 0x80)) {
		printk(KERN_ERR "scsi%d : expecting IDENTIFY message, got ", instance->host_no);
		spi_print_msg(msg);
		abort = 1;
	} else {
		
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		lun = (msg[0] & 0x07);




		for (tmp = (Scsi_Cmnd *) hostdata->disconnected_queue, prev = NULL; tmp; prev = tmp, tmp = (Scsi_Cmnd *) tmp->host_scribble)
			if ((target_mask == (1 << tmp->device->id)) && (lun == tmp->device->lun)
			    ) {
				if (prev) {
					REMOVE(prev, prev->host_scribble, tmp, tmp->host_scribble);
					prev->host_scribble = tmp->host_scribble;
				} else {
					REMOVE(-1, hostdata->disconnected_queue, tmp, tmp->host_scribble);
					hostdata->disconnected_queue = (Scsi_Cmnd *) tmp->host_scribble;
				}
				tmp->host_scribble = NULL;
				break;
			}
		if (!tmp) {
			printk(KERN_ERR "scsi%d : warning : target bitmask %02x lun %d not in disconnect_queue.\n", instance->host_no, target_mask, lun);
			abort = 1;
		}
	}

	if (abort) {
		do_abort(instance);
	} else {
		hostdata->connected = tmp;
		dprintk(NDEBUG_RESELECTION, ("scsi%d : nexus established, target = %d, lun = %d, tag = %d\n", instance->host_no, tmp->target, tmp->lun, tmp->tag));
	}
}


#ifdef REAL_DMA
static void NCR5380_dma_complete(NCR5380_instance * instance) {
	NCR5380_local_declare();
	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *) instance->hostdata;
	int transferred;
	NCR5380_setup(instance);


	NCR5380_poll_politely(instance, BUS_AND_STATUS_REG, BASR_ACK, 0, 5*HZ);

	NCR5380_write(MODE_REG, MR_BASE);
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);


	if (!(hostdata->connected->SCp.phase & SR_CD)) {
		transferred = instance->dmalen - NCR5380_dma_residual();
		hostdata->connected->SCp.this_residual -= transferred;
		hostdata->connected->SCp.ptr += transferred;
	}
}
#endif				


static int NCR5380_abort(Scsi_Cmnd * cmd) {
	NCR5380_local_declare();
	struct Scsi_Host *instance = cmd->device->host;
	struct NCR5380_hostdata *hostdata = (struct NCR5380_hostdata *) instance->hostdata;
	Scsi_Cmnd *tmp, **prev;
	
	printk(KERN_WARNING "scsi%d : aborting command\n", instance->host_no);
	scsi_print_command(cmd);

	NCR5380_print_status(instance);

	NCR5380_setup(instance);

	dprintk(NDEBUG_ABORT, ("scsi%d : abort called\n", instance->host_no));
	dprintk(NDEBUG_ABORT, ("        basr 0x%X, sr 0x%X\n", NCR5380_read(BUS_AND_STATUS_REG), NCR5380_read(STATUS_REG)));

#if 0

	if (hostdata->connected == cmd) {
		dprintk(NDEBUG_ABORT, ("scsi%d : aborting connected command\n", instance->host_no));
		hostdata->aborted = 1;

		NCR5380_write(INITIATOR_COMMAND_REG, ICR_ASSERT_ATN);


		return 0;
	}
#endif

 
	dprintk(NDEBUG_ABORT, ("scsi%d : abort going into loop.\n", instance->host_no));
	for (prev = (Scsi_Cmnd **) & (hostdata->issue_queue), tmp = (Scsi_Cmnd *) hostdata->issue_queue; tmp; prev = (Scsi_Cmnd **) & (tmp->host_scribble), tmp = (Scsi_Cmnd *) tmp->host_scribble)
		if (cmd == tmp) {
			REMOVE(5, *prev, tmp, tmp->host_scribble);
			(*prev) = (Scsi_Cmnd *) tmp->host_scribble;
			tmp->host_scribble = NULL;
			tmp->result = DID_ABORT << 16;
			dprintk(NDEBUG_ABORT, ("scsi%d : abort removed command from issue queue.\n", instance->host_no));
			tmp->scsi_done(tmp);
			return SUCCESS;
		}
#if (NDEBUG  & NDEBUG_ABORT)
	
		else if (prev == tmp)
			printk(KERN_ERR "scsi%d : LOOP\n", instance->host_no);
#endif


	if (hostdata->connected) {
		dprintk(NDEBUG_ABORT, ("scsi%d : abort failed, command connected.\n", instance->host_no));
		return FAILED;
	}

	for (tmp = (Scsi_Cmnd *) hostdata->disconnected_queue; tmp; tmp = (Scsi_Cmnd *) tmp->host_scribble)
		if (cmd == tmp) {
			dprintk(NDEBUG_ABORT, ("scsi%d : aborting disconnected command.\n", instance->host_no));

			if (NCR5380_select(instance, cmd, (int) cmd->tag))
				return FAILED;
			dprintk(NDEBUG_ABORT, ("scsi%d : nexus reestablished.\n", instance->host_no));

			do_abort(instance);

			for (prev = (Scsi_Cmnd **) & (hostdata->disconnected_queue), tmp = (Scsi_Cmnd *) hostdata->disconnected_queue; tmp; prev = (Scsi_Cmnd **) & (tmp->host_scribble), tmp = (Scsi_Cmnd *) tmp->host_scribble)
				if (cmd == tmp) {
					REMOVE(5, *prev, tmp, tmp->host_scribble);
					*prev = (Scsi_Cmnd *) tmp->host_scribble;
					tmp->host_scribble = NULL;
					tmp->result = DID_ABORT << 16;
					tmp->scsi_done(tmp);
					return SUCCESS;
				}
		}
	printk(KERN_WARNING "scsi%d : warning : SCSI command probably completed successfully\n"
			"         before abortion\n", instance->host_no);
	return FAILED;
}



static int NCR5380_bus_reset(Scsi_Cmnd * cmd)
{
	struct Scsi_Host *instance = cmd->device->host;

	NCR5380_local_declare();
	NCR5380_setup(instance);
	NCR5380_print_status(instance);

	spin_lock_irq(instance->host_lock);
	do_reset(instance);
	spin_unlock_irq(instance->host_lock);

	return SUCCESS;
}
