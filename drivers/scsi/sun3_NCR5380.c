 
/* 
 * NCR 5380 generic driver routines.  These should make it *trivial*
 * 	to implement 5380 SCSI drivers under Linux with a non-trantor
 *	architecture.
 *
 *	Note that these routines also work with NR53c400 family chips.
 *
 * Copyright 1993, Drew Eckhardt
 *	Visionary Computing 
 *	(Unix and Linux consulting and custom programming)
 * 	drew@colorado.edu
 *	+1 (303) 666-5836
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


#if (NDEBUG & NDEBUG_LISTS)
#define LIST(x,y) \
  { printk("LINE:%d   Adding %p to %p\n", __LINE__, (void*)(x), (void*)(y)); \
    if ((x)==(y)) udelay(5); }
#define REMOVE(w,x,y,z) \
  { printk("LINE:%d   Removing: %p->%p  %p->%p \n", __LINE__, \
	   (void*)(w), (void*)(x), (void*)(y), (void*)(z)); \
    if ((x)==(y)) udelay(5); }
#else
#define LIST(x,y)
#define REMOVE(w,x,y,z)
#endif

#ifndef notyet
#undef LINKED
#endif

/*
 * Design
 * Issues :
 *
 * The other Linux SCSI drivers were written when Linux was Intel PC-only,
 * and specifically for each board rather than each chip.  This makes their
 * adaptation to platforms like the Mac (Some of which use NCR5380's)
 * more difficult than it has to be.
 *
 * Also, many of the SCSI drivers were written before the command queuing
 * routines were implemented, meaning their implementations of queued 
 * commands were hacked on rather than designed in from the start.
 *
 * When I designed the Linux SCSI drivers I figured that 
 * while having two different SCSI boards in a system might be useful
 * for debugging things, two of the same type wouldn't be used.
 * Well, I was wrong and a number of users have mailed me about running
 * multiple high-performance SCSI boards in a server.
 *
 * Finally, when I get questions from users, I have no idea what 
 * revision of my driver they are running.
 *
 * This driver attempts to address these problems :
 * This is a generic 5380 driver.  To use it on a different platform, 
 * one simply writes appropriate system specific macros (ie, data
 * transfer - some PC's will use the I/O bus, 68K's must use 
 * memory mapped) and drops this file in their 'C' wrapper.
 *
 * As far as command queueing, two queues are maintained for 
 * each 5380 in the system - commands that haven't been issued yet,
 * and commands that are currently executing.  This means that an 
 * unlimited number of commands may be queued, letting 
 * more commands propagate from the higher driver levels giving higher 
 * throughput.  Note that both I_T_L and I_T_L_Q nexuses are supported, 
 * allowing multiple commands to propagate all the way to a SCSI-II device 
 * while a command is already executing.
 *
 * To solve the multiple-boards-in-the-same-system problem, 
 * there is a separate instance structure for each instance
 * of a 5380 in the system.  So, multiple NCR5380 drivers will
 * be able to coexist with appropriate changes to the high level
 * SCSI code.  
 *
 * A NCR5380_PUBLIC_REVISION macro is provided, with the release
 * number (updated for each public release) printed by the 
 * NCR5380_print_options command, which should be called from the 
 * wrapper detect function, so that I know what release of the driver
 * users are using.
 *
 * Issues specific to the NCR5380 : 
 *
 * When used in a PIO or pseudo-dma mode, the NCR5380 is a braindead 
 * piece of hardware that requires you to sit in a loop polling for 
 * the REQ signal as long as you are connected.  Some devices are 
 * brain dead (ie, many TEXEL CD ROM drives) and won't disconnect 
 * while doing long seek operations.
 * 
 * The workaround for this is to keep track of devices that have
 * disconnected.  If the device hasn't disconnected, for commands that
 * should disconnect, we do something like 
 *
 * while (!REQ is asserted) { sleep for N usecs; poll for M usecs }
 * 
 * Some tweaking of N and M needs to be done.  An algorithm based 
 * on "time to data" would give the best results as long as short time
 * to datas (ie, on the same track) were considered, however these 
 * broken devices are the exception rather than the rule and I'd rather
 * spend my time optimizing for the normal case.
 *
 * Architecture :
 *
 * At the heart of the design is a coroutine, NCR5380_main,
 * which is started when not running by the interrupt handler,
 * timer, and queue command function.  It attempts to establish
 * I_T_L or I_T_L_Q nexuses by removing the commands from the 
 * issue queue and calling NCR5380_select() if a nexus 
 * is not established. 
 *
 * Once a nexus is established, the NCR5380_information_transfer()
 * phase goes through the various phases as instructed by the target.
 * if the target goes into MSG IN and sends a DISCONNECT message,
 * the command structure is placed into the per instance disconnected
 * queue, and NCR5380_main tries to find more work.  If USLEEP
 * was defined, and the target is idle for too long, the system
 * will try to sleep.
 *
 * If a command has disconnected, eventually an interrupt will trigger,
 * calling NCR5380_intr()  which will in turn call NCR5380_reselect
 * to reestablish a nexus.  This will run main if necessary.
 *
 * On command termination, the done function will be called as 
 * appropriate.
 *
 * SCSI pointers are maintained in the SCp field of SCSI command 
 * structures, being initialized after the command is connected
 * in NCR5380_select, and set as appropriate in NCR5380_information_transfer.
 * Note that in violation of the standard, an implicit SAVE POINTERS operation
 * is done, since some BROKEN disks fail to issue an explicit SAVE POINTERS.
 */


static struct Scsi_Host *first_instance = NULL;
static struct scsi_host_template *the_template = NULL;

#define	SETUP_HOSTDATA(in)				\
    struct NCR5380_hostdata *hostdata =			\
	(struct NCR5380_hostdata *)(in)->hostdata
#define	HOSTDATA(in) ((struct NCR5380_hostdata *)(in)->hostdata)

#define	NEXT(cmd)		((struct scsi_cmnd *)(cmd)->host_scribble)
#define	SET_NEXT(cmd, next)	((cmd)->host_scribble = (void *)(next))
#define	NEXTADDR(cmd)		((struct scsi_cmnd **)&((cmd)->host_scribble))

#define	HOSTNO		instance->host_no
#define	H_NO(cmd)	(cmd)->device->host->host_no

#define SGADDR(buffer) (void *)(((unsigned long)sg_virt(((buffer)))))

#ifdef SUPPORT_TAGS


#undef TAG_NONE
#define TAG_NONE 0xff

#if (MAX_TAGS % 32) != 0
#error "MAX_TAGS must be a multiple of 32!"
#endif

typedef struct {
    char	allocated[MAX_TAGS/8];
    int		nr_allocated;
    int		queue_size;
} TAG_ALLOC;

static TAG_ALLOC TagAlloc[8][8]; 


static void __init init_tags( void )
{
    int target, lun;
    TAG_ALLOC *ta;
    
    if (!setup_use_tagged_queuing)
	return;
    
    for( target = 0; target < 8; ++target ) {
	for( lun = 0; lun < 8; ++lun ) {
	    ta = &TagAlloc[target][lun];
	    memset( &ta->allocated, 0, MAX_TAGS/8 );
	    ta->nr_allocated = 0;
	    ta->queue_size = MAX_TAGS;
	}
    }
}


 

static int is_lun_busy(struct scsi_cmnd *cmd, int should_be_tagged)
{
    SETUP_HOSTDATA(cmd->device->host);

    if (hostdata->busy[cmd->device->id] & (1 << cmd->device->lun))
	return( 1 );
    if (!should_be_tagged ||
	!setup_use_tagged_queuing || !cmd->device->tagged_supported)
	return( 0 );
    if (TagAlloc[cmd->device->id][cmd->device->lun].nr_allocated >=
	TagAlloc[cmd->device->id][cmd->device->lun].queue_size ) {
	TAG_PRINTK( "scsi%d: target %d lun %d: no free tags\n",
		    H_NO(cmd), cmd->device->id, cmd->device->lun );
	return( 1 );
    }
    return( 0 );
}



static void cmd_get_tag(struct scsi_cmnd *cmd, int should_be_tagged)
{
    SETUP_HOSTDATA(cmd->device->host);

    if (!should_be_tagged ||
	!setup_use_tagged_queuing || !cmd->device->tagged_supported) {
	cmd->tag = TAG_NONE;
	hostdata->busy[cmd->device->id] |= (1 << cmd->device->lun);
	TAG_PRINTK( "scsi%d: target %d lun %d now allocated by untagged "
		    "command\n", H_NO(cmd), cmd->device->id, cmd->device->lun );
    }
    else {
	TAG_ALLOC *ta = &TagAlloc[cmd->device->id][cmd->device->lun];

	cmd->tag = find_first_zero_bit( &ta->allocated, MAX_TAGS );
	set_bit( cmd->tag, &ta->allocated );
	ta->nr_allocated++;
	TAG_PRINTK( "scsi%d: using tag %d for target %d lun %d "
		    "(now %d tags in use)\n",
		    H_NO(cmd), cmd->tag, cmd->device->id, cmd->device->lun,
		    ta->nr_allocated );
    }
}



static void cmd_free_tag(struct scsi_cmnd *cmd)
{
    SETUP_HOSTDATA(cmd->device->host);

    if (cmd->tag == TAG_NONE) {
	hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
	TAG_PRINTK( "scsi%d: target %d lun %d untagged cmd finished\n",
		    H_NO(cmd), cmd->device->id, cmd->device->lun );
    }
    else if (cmd->tag >= MAX_TAGS) {
	printk(KERN_NOTICE "scsi%d: trying to free bad tag %d!\n",
		H_NO(cmd), cmd->tag );
    }
    else {
	TAG_ALLOC *ta = &TagAlloc[cmd->device->id][cmd->device->lun];
	clear_bit( cmd->tag, &ta->allocated );
	ta->nr_allocated--;
	TAG_PRINTK( "scsi%d: freed tag %d for target %d lun %d\n",
		    H_NO(cmd), cmd->tag, cmd->device->id, cmd->device->lun );
    }
}


static void free_all_tags( void )
{
    int target, lun;
    TAG_ALLOC *ta;

    if (!setup_use_tagged_queuing)
	return;
    
    for( target = 0; target < 8; ++target ) {
	for( lun = 0; lun < 8; ++lun ) {
	    ta = &TagAlloc[target][lun];
	    memset( &ta->allocated, 0, MAX_TAGS/8 );
	    ta->nr_allocated = 0;
	}
    }
}

#endif 



static __inline__ void initialize_SCp(struct scsi_cmnd *cmd)
{

    if (scsi_bufflen(cmd)) {
	cmd->SCp.buffer = scsi_sglist(cmd);
	cmd->SCp.buffers_residual = scsi_sg_count(cmd) - 1;
	cmd->SCp.ptr = (char *) SGADDR(cmd->SCp.buffer);
	cmd->SCp.this_residual = cmd->SCp.buffer->length;
    } else {
	cmd->SCp.buffer = NULL;
	cmd->SCp.buffers_residual = 0;
	cmd->SCp.ptr = NULL;
	cmd->SCp.this_residual = 0;
    }
    
}

#include <linux/delay.h>

#if 1
static struct {
    unsigned char mask;
    const char * name;} 
signals[] = {{ SR_DBP, "PARITY"}, { SR_RST, "RST" }, { SR_BSY, "BSY" }, 
    { SR_REQ, "REQ" }, { SR_MSG, "MSG" }, { SR_CD,  "CD" }, { SR_IO, "IO" }, 
    { SR_SEL, "SEL" }, {0, NULL}}, 
basrs[] = {{BASR_ATN, "ATN"}, {BASR_ACK, "ACK"}, {0, NULL}},
icrs[] = {{ICR_ASSERT_RST, "ASSERT RST"},{ICR_ASSERT_ACK, "ASSERT ACK"},
    {ICR_ASSERT_BSY, "ASSERT BSY"}, {ICR_ASSERT_SEL, "ASSERT SEL"}, 
    {ICR_ASSERT_ATN, "ASSERT ATN"}, {ICR_ASSERT_DATA, "ASSERT DATA"}, 
    {0, NULL}},
mrs[] = {{MR_BLOCK_DMA_MODE, "MODE BLOCK DMA"}, {MR_TARGET, "MODE TARGET"}, 
    {MR_ENABLE_PAR_CHECK, "MODE PARITY CHECK"}, {MR_ENABLE_PAR_INTR, 
    "MODE PARITY INTR"}, {MR_ENABLE_EOP_INTR,"MODE EOP INTR"},
    {MR_MONITOR_BSY, "MODE MONITOR BSY"},
    {MR_DMA_MODE, "MODE DMA"}, {MR_ARBITRATE, "MODE ARBITRATION"}, 
    {0, NULL}};


static void NCR5380_print(struct Scsi_Host *instance) {
    unsigned char status, data, basr, mr, icr, i;
    unsigned long flags;

    local_irq_save(flags);
    data = NCR5380_read(CURRENT_SCSI_DATA_REG);
    status = NCR5380_read(STATUS_REG);
    mr = NCR5380_read(MODE_REG);
    icr = NCR5380_read(INITIATOR_COMMAND_REG);
    basr = NCR5380_read(BUS_AND_STATUS_REG);
    local_irq_restore(flags);
    printk("STATUS_REG: %02x ", status);
    for (i = 0; signals[i].mask ; ++i) 
	if (status & signals[i].mask)
	    printk(",%s", signals[i].name);
    printk("\nBASR: %02x ", basr);
    for (i = 0; basrs[i].mask ; ++i) 
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

static struct {
    unsigned char value;
    const char *name;
} phases[] = {
    {PHASE_DATAOUT, "DATAOUT"}, {PHASE_DATAIN, "DATAIN"}, {PHASE_CMDOUT, "CMDOUT"},
    {PHASE_STATIN, "STATIN"}, {PHASE_MSGOUT, "MSGOUT"}, {PHASE_MSGIN, "MSGIN"},
    {PHASE_UNKNOWN, "UNKNOWN"}};


static void NCR5380_print_phase(struct Scsi_Host *instance)
{
    unsigned char status;
    int i;

    status = NCR5380_read(STATUS_REG);
    if (!(status & SR_REQ)) 
	printk(KERN_DEBUG "scsi%d: REQ not asserted, phase unknown.\n", HOSTNO);
    else {
	for (i = 0; (phases[i].value != PHASE_UNKNOWN) && 
	    (phases[i].value != (status & PHASE_MASK)); ++i); 
	printk(KERN_DEBUG "scsi%d: phase %s\n", HOSTNO, phases[i].name);
    }
}

#else 

__inline__ void NCR5380_print(struct Scsi_Host *instance) { };
__inline__ void NCR5380_print_phase(struct Scsi_Host *instance) { };

#endif


#include <linux/gfp.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>

static volatile int main_running = 0;
static DECLARE_WORK(NCR5380_tqueue, NCR5380_main);

static __inline__ void queue_main(void)
{
    if (!main_running) {
	schedule_work(&NCR5380_tqueue);
    }
}


static inline void NCR5380_all_init (void)
{
    static int done = 0;
    if (!done) {
	INI_PRINTK("scsi : NCR5380_all_init()\n");
	done = 1;
    }
}

 

static void __init NCR5380_print_options (struct Scsi_Host *instance)
{
    printk(" generic options"
#ifdef AUTOSENSE 
    " AUTOSENSE"
#endif
#ifdef REAL_DMA
    " REAL DMA"
#endif
#ifdef PARITY
    " PARITY"
#endif
#ifdef SUPPORT_TAGS
    " SCSI-2 TAGGED QUEUING"
#endif
    );
    printk(" generic release=%d", NCR5380_PUBLIC_RELEASE);
}


static void NCR5380_print_status (struct Scsi_Host *instance)
{
    char *pr_bfr;
    char *start;
    int len;

    NCR_PRINT(NDEBUG_ANY);
    NCR_PRINT_PHASE(NDEBUG_ANY);

    pr_bfr = (char *) __get_free_page(GFP_ATOMIC);
    if (!pr_bfr) {
	printk("NCR5380_print_status: no memory for print buffer\n");
	return;
    }
    len = NCR5380_proc_info(instance, pr_bfr, &start, 0, PAGE_SIZE, 0);
    pr_bfr[len] = 0;
    printk("\n%s\n", pr_bfr);
    free_page((unsigned long) pr_bfr);
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
#define SPRINTF(fmt,args...) \
  do { if (pos + strlen(fmt) + 20  < buffer + length) \
	 pos += sprintf(pos, fmt , ## args); } while(0)
static
char *lprint_Scsi_Cmnd(struct scsi_cmnd *cmd, char *pos, char *buffer,
		       int length);

static int NCR5380_proc_info(struct Scsi_Host *instance, char *buffer,
			     char **start, off_t offset, int length, int inout)
{
    char *pos = buffer;
    struct NCR5380_hostdata *hostdata;
    struct scsi_cmnd *ptr;
    unsigned long flags;
    off_t begin = 0;
#define check_offset()				\
    do {					\
	if (pos - buffer < offset - begin) {	\
	    begin += pos - buffer;		\
	    pos = buffer;			\
	}					\
    } while (0)

    hostdata = (struct NCR5380_hostdata *)instance->hostdata;

    if (inout) { /* Has data been written to the file ? */
	return(-ENOSYS);  
    }
    SPRINTF("NCR5380 core release=%d.\n", NCR5380_PUBLIC_RELEASE);
    check_offset();
    local_irq_save(flags);
    SPRINTF("NCR5380: coroutine is%s running.\n", main_running ? "" : "n't");
    check_offset();
    if (!hostdata->connected)
	SPRINTF("scsi%d: no currently connected command\n", HOSTNO);
    else
	pos = lprint_Scsi_Cmnd ((struct scsi_cmnd *) hostdata->connected,
				pos, buffer, length);
    SPRINTF("scsi%d: issue_queue\n", HOSTNO);
    check_offset();
    for (ptr = (struct scsi_cmnd *) hostdata->issue_queue; ptr; ptr = NEXT(ptr))
    {
	pos = lprint_Scsi_Cmnd (ptr, pos, buffer, length);
	check_offset();
    }

    SPRINTF("scsi%d: disconnected_queue\n", HOSTNO);
    check_offset();
    for (ptr = (struct scsi_cmnd *) hostdata->disconnected_queue; ptr;
	 ptr = NEXT(ptr)) {
	pos = lprint_Scsi_Cmnd (ptr, pos, buffer, length);
	check_offset();
    }

    local_irq_restore(flags);
    *start = buffer + (offset - begin);
    if (pos - buffer < offset - begin)
	return 0;
    else if (pos - buffer - (offset - begin) < length)
	return pos - buffer - (offset - begin);
    return length;
}

static char *lprint_Scsi_Cmnd(struct scsi_cmnd *cmd, char *pos, char *buffer,
			      int length)
{
    int i, s;
    unsigned char *command;
    SPRINTF("scsi%d: destination target %d, lun %d\n",
	    H_NO(cmd), cmd->device->id, cmd->device->lun);
    SPRINTF("        command = ");
    command = cmd->cmnd;
    SPRINTF("%2d (0x%02x)", command[0], command[0]);
    for (i = 1, s = COMMAND_SIZE(command[0]); i < s; ++i)
	SPRINTF(" %02x", command[i]);
    SPRINTF("\n");
    return pos;
}



static int __init NCR5380_init(struct Scsi_Host *instance, int flags)
{
    int i;
    SETUP_HOSTDATA(instance);

    NCR5380_all_init();

    hostdata->aborted = 0;
    hostdata->id_mask = 1 << instance->this_id;
    hostdata->id_higher_mask = 0;
    for (i = hostdata->id_mask; i <= 0x80; i <<= 1)
	if (i > hostdata->id_mask)
	    hostdata->id_higher_mask |= i;
    for (i = 0; i < 8; ++i)
	hostdata->busy[i] = 0;
#ifdef SUPPORT_TAGS
    init_tags();
#endif
#if defined (REAL_DMA)
    hostdata->dma_len = 0;
#endif
    hostdata->targets_present = 0;
    hostdata->connected = NULL;
    hostdata->issue_queue = NULL;
    hostdata->disconnected_queue = NULL;
    hostdata->flags = FLAG_CHECK_LAST_BYTE_SENT;

    if (!the_template) {
	the_template = instance->hostt;
	first_instance = instance;
    }
	

#ifndef AUTOSENSE
    if ((instance->cmd_per_lun > 1) || (instance->can_queue > 1))
	 printk("scsi%d: WARNING : support for multiple outstanding commands enabled\n"
	        "        without AUTOSENSE option, contingent allegiance conditions may\n"
	        "        be incorrectly cleared.\n", HOSTNO);
#endif 

    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
    NCR5380_write(MODE_REG, MR_BASE);
    NCR5380_write(TARGET_COMMAND_REG, 0);
    NCR5380_write(SELECT_ENABLE_REG, 0);

    return 0;
}

static void NCR5380_exit(struct Scsi_Host *instance)
{
	
}


static int NCR5380_queue_command_lck(struct scsi_cmnd *cmd,
				 void (*done)(struct scsi_cmnd *))
{
    SETUP_HOSTDATA(cmd->device->host);
    struct scsi_cmnd *tmp;
    unsigned long flags;

#if (NDEBUG & NDEBUG_NO_WRITE)
    switch (cmd->cmnd[0]) {
    case WRITE_6:
    case WRITE_10:
	printk(KERN_NOTICE "scsi%d: WRITE attempted with NO_WRITE debugging flag set\n",
	       H_NO(cmd));
	cmd->result = (DID_ERROR << 16);
	done(cmd);
	return 0;
    }
#endif 


#ifdef NCR5380_STATS
# if 0
    if (!hostdata->connected && !hostdata->issue_queue &&
	!hostdata->disconnected_queue) {
	hostdata->timebase = jiffies;
    }
# endif
# ifdef NCR5380_STAT_LIMIT
    if (scsi_bufflen(cmd) > NCR5380_STAT_LIMIT)
# endif
	switch (cmd->cmnd[0])
	{
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


    SET_NEXT(cmd, NULL);
    cmd->scsi_done = done;

    cmd->result = 0;



    local_irq_save(flags);
    if (!(hostdata->issue_queue) || (cmd->cmnd[0] == REQUEST_SENSE)) {
	LIST(cmd, hostdata->issue_queue);
	SET_NEXT(cmd, hostdata->issue_queue);
	hostdata->issue_queue = cmd;
    } else {
	for (tmp = (struct scsi_cmnd *)hostdata->issue_queue;
	     NEXT(tmp); tmp = NEXT(tmp))
	    ;
	LIST(cmd, tmp);
	SET_NEXT(tmp, cmd);
    }

    local_irq_restore(flags);

    QU_PRINTK("scsi%d: command added to %s of queue\n", H_NO(cmd),
	      (cmd->cmnd[0] == REQUEST_SENSE) ? "head" : "tail");

    if (in_interrupt() || ((flags >> 8) & 7) >= 6)
	queue_main();
    else
	NCR5380_main(NULL);
    return 0;
}

static DEF_SCSI_QCMD(NCR5380_queue_command)

 	
    
static void NCR5380_main (struct work_struct *bl)
{
    struct scsi_cmnd *tmp, *prev;
    struct Scsi_Host *instance = first_instance;
    struct NCR5380_hostdata *hostdata = HOSTDATA(instance);
    int done;
    unsigned long flags;
    

    if (main_running)
    	return;
    main_running = 1;

    local_save_flags(flags);
    do {
	local_irq_disable(); 
	done = 1;
	
	if (!hostdata->connected) {
	    MAIN_PRINTK( "scsi%d: not connected\n", HOSTNO );
#if (NDEBUG & NDEBUG_LISTS)
	    for (tmp = (struct scsi_cmnd *) hostdata->issue_queue, prev = NULL;
		 tmp && (tmp != prev); prev = tmp, tmp = NEXT(tmp))
		;
	    if ((tmp == prev) && tmp) printk(" LOOP\n");
#endif
	    for (tmp = (struct scsi_cmnd *) hostdata->issue_queue,
		 prev = NULL; tmp; prev = tmp, tmp = NEXT(tmp) ) {

#if (NDEBUG & NDEBUG_LISTS)
		if (prev != tmp)
		    printk("MAIN tmp=%p   target=%d   busy=%d lun=%d\n",
			   tmp, tmp->target, hostdata->busy[tmp->target],
			   tmp->lun);
#endif
		
		
		if (
#ifdef SUPPORT_TAGS
		    !is_lun_busy( tmp, tmp->cmnd[0] != REQUEST_SENSE)
#else
		    !(hostdata->busy[tmp->device->id] & (1 << tmp->device->lun))
#endif
		    ) {
		    
		    local_irq_disable();
		    if (prev) {
		        REMOVE(prev, NEXT(prev), tmp, NEXT(tmp));
			SET_NEXT(prev, NEXT(tmp));
		    } else {
		        REMOVE(-1, hostdata->issue_queue, tmp, NEXT(tmp));
			hostdata->issue_queue = NEXT(tmp);
		    }
		    SET_NEXT(tmp, NULL);
		    
		    
		    local_irq_restore(flags);
		    
		    MAIN_PRINTK("scsi%d: main(): command for target %d "
				"lun %d removed from issue_queue\n",
				HOSTNO, tmp->target, tmp->lun);
		    
#ifdef SUPPORT_TAGS
		    cmd_get_tag( tmp, tmp->cmnd[0] != REQUEST_SENSE );
#endif
		    if (!NCR5380_select(instance, tmp, 
			    (tmp->cmnd[0] == REQUEST_SENSE) ? TAG_NONE : 
			    TAG_NEXT)) {
			break;
		    } else {
			local_irq_disable();
			LIST(tmp, hostdata->issue_queue);
			SET_NEXT(tmp, hostdata->issue_queue);
			hostdata->issue_queue = tmp;
#ifdef SUPPORT_TAGS
			cmd_free_tag( tmp );
#endif
			local_irq_restore(flags);
			MAIN_PRINTK("scsi%d: main(): select() failed, "
				    "returned to issue_queue\n", HOSTNO);
			if (hostdata->connected)
			    break;
		    }
		} 
	    } 
	} 
	if (hostdata->connected 
#ifdef REAL_DMA
	    && !hostdata->dma_len
#endif
	    ) {
	    local_irq_restore(flags);
	    MAIN_PRINTK("scsi%d: main: performing information transfer\n",
			HOSTNO);
	    NCR5380_information_transfer(instance);
	    MAIN_PRINTK("scsi%d: main: done set false\n", HOSTNO);
	    done = 0;
	}
    } while (!done);

    main_running = 0;
    local_irq_restore(flags);
}


#ifdef REAL_DMA

static void NCR5380_dma_complete( struct Scsi_Host *instance )
{
    SETUP_HOSTDATA(instance);
    int           transfered;
    unsigned char **data;
    volatile int  *count;

    if (!hostdata->connected) {
	printk(KERN_WARNING "scsi%d: received end of DMA interrupt with "
	       "no connected cmd\n", HOSTNO);
	return;
    }

    DMA_PRINTK("scsi%d: real DMA transfer complete, basr 0x%X, sr 0x%X\n",
	       HOSTNO, NCR5380_read(BUS_AND_STATUS_REG),
	       NCR5380_read(STATUS_REG));

    if((sun3scsi_dma_finish(rq_data_dir(hostdata->connected->request)))) {
	    printk("scsi%d: overrun in UDC counter -- not prepared to deal with this!\n", HOSTNO);
	    printk("please e-mail sammy@sammy.net with a description of how this\n");
	    printk("error was produced.\n");
	    BUG();
    }

    
    if((NCR5380_read(BUS_AND_STATUS_REG) & (BASR_PHASE_MATCH |
					    BASR_ACK)) ==
       (BASR_PHASE_MATCH | BASR_ACK)) {
	    printk("scsi%d: BASR %02x\n", HOSTNO, NCR5380_read(BUS_AND_STATUS_REG));
	    printk("scsi%d: bus stuck in data phase -- probably a single byte "
		   "overrun!\n", HOSTNO);
	    printk("not prepared for this error!\n");
	    printk("please e-mail sammy@sammy.net with a description of how this\n");
	    printk("error was produced.\n");
	    BUG();
    }



    (void) NCR5380_read(RESET_PARITY_INTERRUPT_REG);
    NCR5380_write(MODE_REG, MR_BASE);
    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);

    transfered = hostdata->dma_len - NCR5380_dma_residual(instance);
    hostdata->dma_len = 0;

    data = (unsigned char **) &(hostdata->connected->SCp.ptr);
    count = &(hostdata->connected->SCp.this_residual);
    *data += transfered;
    *count -= transfered;

}
#endif 



static irqreturn_t NCR5380_intr (int irq, void *dev_id)
{
    struct Scsi_Host *instance = first_instance;
    int done = 1, handled = 0;
    unsigned char basr;

    INT_PRINTK("scsi%d: NCR5380 irq triggered\n", HOSTNO);

    
    basr = NCR5380_read(BUS_AND_STATUS_REG);
    INT_PRINTK("scsi%d: BASR=%02x\n", HOSTNO, basr);
    
    if (basr & BASR_IRQ) {
	NCR_PRINT(NDEBUG_INTR);
	if ((NCR5380_read(STATUS_REG) & (SR_SEL|SR_IO)) == (SR_SEL|SR_IO)) {
	    done = 0;
	    INT_PRINTK("scsi%d: SEL interrupt\n", HOSTNO);
	    NCR5380_reselect(instance);
	    (void) NCR5380_read(RESET_PARITY_INTERRUPT_REG);
	}
	else if (basr & BASR_PARITY_ERROR) {
	    INT_PRINTK("scsi%d: PARITY interrupt\n", HOSTNO);
	    (void) NCR5380_read(RESET_PARITY_INTERRUPT_REG);
	}
	else if ((NCR5380_read(STATUS_REG) & SR_RST) == SR_RST) {
	    INT_PRINTK("scsi%d: RESET interrupt\n", HOSTNO);
	    (void)NCR5380_read(RESET_PARITY_INTERRUPT_REG);
	}
	else {

#if defined(REAL_DMA)

	    if ((NCR5380_read(MODE_REG) & MR_DMA_MODE) &&
		((basr & BASR_END_DMA_TRANSFER) || 
		 !(basr & BASR_PHASE_MATCH))) {
		    
		INT_PRINTK("scsi%d: PHASE MISM or EOP interrupt\n", HOSTNO);
		NCR5380_dma_complete( instance );
		done = 0;
	    } else
#endif 
	    {
		if (basr & BASR_PHASE_MATCH)
		   INT_PRINTK("scsi%d: unknown interrupt, "
			   "BASR 0x%x, MR 0x%x, SR 0x%x\n",
			   HOSTNO, basr, NCR5380_read(MODE_REG),
			   NCR5380_read(STATUS_REG));
		(void) NCR5380_read(RESET_PARITY_INTERRUPT_REG);
#ifdef SUN3_SCSI_VME
		dregs->csr |= CSR_DMA_ENABLE;
#endif
	    }
	} 
	handled = 1;
    } 
    else {

	printk(KERN_NOTICE "scsi%d: interrupt without IRQ bit set in BASR, "
	       "BASR 0x%X, MR 0x%X, SR 0x%x\n", HOSTNO, basr,
	       NCR5380_read(MODE_REG), NCR5380_read(STATUS_REG));
	(void) NCR5380_read(RESET_PARITY_INTERRUPT_REG);
#ifdef SUN3_SCSI_VME
		dregs->csr |= CSR_DMA_ENABLE;
#endif
    }
    
    if (!done) {
	INT_PRINTK("scsi%d: in int routine, calling main\n", HOSTNO);
	
	queue_main();
    }
    return IRQ_RETVAL(handled);
}

#ifdef NCR5380_STATS
static void collect_stats(struct NCR5380_hostdata *hostdata,
			  struct scsi_cmnd *cmd)
{
# ifdef NCR5380_STAT_LIMIT
    if (scsi_bufflen(cmd) > NCR5380_STAT_LIMIT)
# endif
	switch (cmd->cmnd[0])
	{
	    case WRITE:
	    case WRITE_6:
	    case WRITE_10:
		hostdata->time_write[cmd->device->id] += (jiffies - hostdata->timebase);
		
		hostdata->pendingw--;
		break;
	    case READ:
	    case READ_6:
	    case READ_10:
		hostdata->time_read[cmd->device->id] += (jiffies - hostdata->timebase);
		
		hostdata->pendingr--;
		break;
	}
}
#endif


static int NCR5380_select(struct Scsi_Host *instance, struct scsi_cmnd *cmd,
			  int tag)
{
    SETUP_HOSTDATA(instance);
    unsigned char tmp[3], phase;
    unsigned char *data;
    int len;
    unsigned long timeout;
    unsigned long flags;

    hostdata->restart_select = 0;
    NCR_PRINT(NDEBUG_ARBITRATION);
    ARB_PRINTK("scsi%d: starting arbitration, id = %d\n", HOSTNO,
	       instance->this_id);


    local_irq_save(flags);
    if (hostdata->connected) {
	local_irq_restore(flags);
	return -1;
    }
    NCR5380_write(TARGET_COMMAND_REG, 0);


    
    NCR5380_write(OUTPUT_DATA_REG, hostdata->id_mask);
    NCR5380_write(MODE_REG, MR_ARBITRATE);

    local_irq_restore(flags);

    
#ifdef NCR_TIMEOUT
    {
      unsigned long timeout = jiffies + 2*NCR_TIMEOUT;

      while (!(NCR5380_read(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_PROGRESS)
	   && time_before(jiffies, timeout) && !hostdata->connected)
	;
      if (time_after_eq(jiffies, timeout))
      {
	printk("scsi : arbitration timeout at %d\n", __LINE__);
	NCR5380_write(MODE_REG, MR_BASE);
	NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
	return -1;
      }
    }
#else 
    while (!(NCR5380_read(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_PROGRESS)
	 && !hostdata->connected);
#endif

    ARB_PRINTK("scsi%d: arbitration complete\n", HOSTNO);

    if (hostdata->connected) {
	NCR5380_write(MODE_REG, MR_BASE); 
	return -1;
    }

    udelay(3);

    
    if ((NCR5380_read(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_LOST) ||
	(NCR5380_read(CURRENT_SCSI_DATA_REG) & hostdata->id_higher_mask) ||
	(NCR5380_read(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_LOST) ||
	hostdata->connected) {
	NCR5380_write(MODE_REG, MR_BASE); 
	ARB_PRINTK("scsi%d: lost arbitration, deasserting MR_ARBITRATE\n",
		   HOSTNO);
	return -1;
    }

     
    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_SEL |
					 ICR_ASSERT_BSY ) ;
    
    if ((NCR5380_read(INITIATOR_COMMAND_REG) & ICR_ARBITRATION_LOST) ||
	hostdata->connected) {
	NCR5380_write(MODE_REG, MR_BASE);
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	ARB_PRINTK("scsi%d: lost arbitration, deasserting ICR_ASSERT_SEL\n",
		   HOSTNO);
	return -1;
    }


#ifdef CONFIG_ATARI_SCSI_TOSHIBA_DELAY
    
    udelay(15);
#else
    udelay(2);
#endif
    
    if (hostdata->connected) {
	NCR5380_write(MODE_REG, MR_BASE);
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	return -1;
    }

    ARB_PRINTK("scsi%d: won arbitration\n", HOSTNO);


    NCR5380_write(OUTPUT_DATA_REG, (hostdata->id_mask | (1 << cmd->device->id)));


    NCR5380_write(INITIATOR_COMMAND_REG, (ICR_BASE | ICR_ASSERT_BSY | 
	ICR_ASSERT_DATA | ICR_ASSERT_ATN | ICR_ASSERT_SEL ));
    NCR5380_write(MODE_REG, MR_BASE);


    if (hostdata->connected) {
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	return -1;
    }

    NCR5380_write(SELECT_ENABLE_REG, 0);

    udelay(1);        

    
    NCR5380_write(INITIATOR_COMMAND_REG, (ICR_BASE | ICR_ASSERT_DATA | 
	ICR_ASSERT_ATN | ICR_ASSERT_SEL));


    udelay(1);

    SEL_PRINTK("scsi%d: selecting target %d\n", HOSTNO, cmd->device->id);


    timeout = jiffies + 25; 


#if 0

    while (time_before(jiffies, timeout) && !(NCR5380_read(STATUS_REG) & 
	(SR_BSY | SR_IO)));

    if ((NCR5380_read(STATUS_REG) & (SR_SEL | SR_IO)) == 
	    (SR_SEL | SR_IO)) {
	    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	    NCR5380_reselect(instance);
	    printk (KERN_ERR "scsi%d: reselection after won arbitration?\n",
		    HOSTNO);
	    NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
	    return -1;
    }
#else
    while (time_before(jiffies, timeout) && !(NCR5380_read(STATUS_REG) & SR_BSY));
#endif


    udelay(1);

    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);

    if (!(NCR5380_read(STATUS_REG) & SR_BSY)) {
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	if (hostdata->targets_present & (1 << cmd->device->id)) {
	    printk(KERN_ERR "scsi%d: weirdness\n", HOSTNO);
	    if (hostdata->restart_select)
		printk(KERN_NOTICE "\trestart select\n");
	    NCR_PRINT(NDEBUG_ANY);
	    NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
	    return -1;
	}
	cmd->result = DID_BAD_TARGET << 16;
#ifdef NCR5380_STATS
	collect_stats(hostdata, cmd);
#endif
#ifdef SUPPORT_TAGS
	cmd_free_tag( cmd );
#endif
	cmd->scsi_done(cmd);
	NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
	SEL_PRINTK("scsi%d: target did not respond within 250ms\n", HOSTNO);
	NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
	return 0;
    } 

    hostdata->targets_present |= (1 << cmd->device->id);


    
    while (!(NCR5380_read(STATUS_REG) & SR_REQ));

    SEL_PRINTK("scsi%d: target %d selected, going into MESSAGE OUT phase.\n",
	       HOSTNO, cmd->device->id);
    tmp[0] = IDENTIFY(1, cmd->device->lun);

#ifdef SUPPORT_TAGS
    if (cmd->tag != TAG_NONE) {
	tmp[1] = hostdata->last_message = SIMPLE_QUEUE_TAG;
	tmp[2] = cmd->tag;
	len = 3;
    } else 
	len = 1;
#else
    len = 1;
    cmd->tag=0;
#endif 

    
    data = tmp;
    phase = PHASE_MSGOUT;
    NCR5380_transfer_pio(instance, &phase, &len, &data);
    SEL_PRINTK("scsi%d: nexus established.\n", HOSTNO);
    
    hostdata->connected = cmd;
#ifndef SUPPORT_TAGS
    hostdata->busy[cmd->device->id] |= (1 << cmd->device->lun);
#endif    
#ifdef SUN3_SCSI_VME
    dregs->csr |= CSR_INTR;
#endif
    initialize_SCp(cmd);


    return 0;
}



static int NCR5380_transfer_pio( struct Scsi_Host *instance, 
				 unsigned char *phase, int *count,
				 unsigned char **data)
{
    register unsigned char p = *phase, tmp;
    register int c = *count;
    register unsigned char *d = *data;


    NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(p));

    do {
	while (!((tmp = NCR5380_read(STATUS_REG)) & SR_REQ));

	HSH_PRINTK("scsi%d: REQ detected\n", HOSTNO);

		
	if ((tmp & PHASE_MASK) != p) {
	    PIO_PRINTK("scsi%d: phase mismatch\n", HOSTNO);
	    NCR_PRINT_PHASE(NDEBUG_PIO);
	    break;
	}

	
	if (!(p & SR_IO)) 
	    NCR5380_write(OUTPUT_DATA_REG, *d);
	else 
	    *d = NCR5380_read(CURRENT_SCSI_DATA_REG);

	++d;


	if (!(p & SR_IO)) {
	    if (!((p & SR_MSG) && c > 1)) {
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | 
		    ICR_ASSERT_DATA);
		NCR_PRINT(NDEBUG_PIO);
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | 
			ICR_ASSERT_DATA | ICR_ASSERT_ACK);
	    } else {
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE |
		    ICR_ASSERT_DATA | ICR_ASSERT_ATN);
		NCR_PRINT(NDEBUG_PIO);
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | 
		    ICR_ASSERT_DATA | ICR_ASSERT_ATN | ICR_ASSERT_ACK);
	    }
	} else {
	    NCR_PRINT(NDEBUG_PIO);
	    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ACK);
	}

	while (NCR5380_read(STATUS_REG) & SR_REQ);

	HSH_PRINTK("scsi%d: req false, handshake complete\n", HOSTNO);

	if (!(p == PHASE_MSGIN && c == 1)) {  
	    if (p == PHASE_MSGOUT && c > 1)
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
	    else
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
	} 
    } while (--c);

    PIO_PRINTK("scsi%d: residual %d\n", HOSTNO, c);

    *count = c;
    *data = d;
    tmp = NCR5380_read(STATUS_REG);
    if ((tmp & SR_REQ) || (p == PHASE_MSGIN && c == 0))
	*phase = tmp & PHASE_MASK;
    else 
	*phase = PHASE_UNKNOWN;

    if (!c || (*phase == p))
	return 0;
    else 
	return -1;
}


static int do_abort (struct Scsi_Host *host) 
{
    unsigned char tmp, *msgptr, phase;
    int len;

    
    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);

    
    while (!((tmp = NCR5380_read(STATUS_REG)) & SR_REQ));

    NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(tmp));

    if ((tmp & PHASE_MASK) != PHASE_MSGOUT) {
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN | 
		      ICR_ASSERT_ACK);
	while (NCR5380_read(STATUS_REG) & SR_REQ);
	NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN);
    }
   
    tmp = ABORT;
    msgptr = &tmp;
    len = 1;
    phase = PHASE_MSGOUT;
    NCR5380_transfer_pio (host, &phase, &len, &msgptr);


    return len ? -1 : 0;
}

#if defined(REAL_DMA)


static int NCR5380_transfer_dma( struct Scsi_Host *instance, 
				 unsigned char *phase, int *count,
				 unsigned char **data)
{
    SETUP_HOSTDATA(instance);
    register int c = *count;
    register unsigned char p = *phase;
    unsigned long flags;

    
    if(!sun3_dma_setup_done) {
	 printk("scsi%d: transfer_dma without setup!\n", HOSTNO);
	 BUG();
    }
    hostdata->dma_len = c;

    DMA_PRINTK("scsi%d: initializing DMA for %s, %d bytes %s %p\n",
	       HOSTNO, (p & SR_IO) ? "reading" : "writing",
	       c, (p & SR_IO) ? "to" : "from", *data);

    
    local_irq_save(flags);
    
    
    sun3scsi_dma_start(c, *data);
    
    if (p & SR_IO) {
	    NCR5380_write(TARGET_COMMAND_REG, 1);
	    NCR5380_read(RESET_PARITY_INTERRUPT_REG);
	    NCR5380_write(INITIATOR_COMMAND_REG, 0);
	    NCR5380_write(MODE_REG, (NCR5380_read(MODE_REG) | MR_DMA_MODE | MR_ENABLE_EOP_INTR));
	    NCR5380_write(START_DMA_INITIATOR_RECEIVE_REG, 0);
    } else {
	    NCR5380_write(TARGET_COMMAND_REG, 0);
	    NCR5380_read(RESET_PARITY_INTERRUPT_REG);
	    NCR5380_write(INITIATOR_COMMAND_REG, ICR_ASSERT_DATA);
	    NCR5380_write(MODE_REG, (NCR5380_read(MODE_REG) | MR_DMA_MODE | MR_ENABLE_EOP_INTR));
	    NCR5380_write(START_DMA_SEND_REG, 0);
    }

#ifdef SUN3_SCSI_VME
    dregs->csr |= CSR_DMA_ENABLE;
#endif

    local_irq_restore(flags);

    sun3_dma_active = 1;
    return 0;
}
#endif 

 
static void NCR5380_information_transfer (struct Scsi_Host *instance)
{
    SETUP_HOSTDATA(instance);
    unsigned long flags;
    unsigned char msgout = NOP;
    int sink = 0;
    int len;
#if defined(REAL_DMA)
    int transfersize;
#endif
    unsigned char *data;
    unsigned char phase, tmp, extended_msg[10], old_phase=0xff;
    struct scsi_cmnd *cmd = (struct scsi_cmnd *) hostdata->connected;

#ifdef SUN3_SCSI_VME
    dregs->csr |= CSR_INTR;
#endif

    while (1) {
	tmp = NCR5380_read(STATUS_REG);
	
	if (tmp & SR_REQ) {
	    phase = (tmp & PHASE_MASK); 
 	    if (phase != old_phase) {
		old_phase = phase;
		NCR_PRINT_PHASE(NDEBUG_INFORMATION);
	    }

	    if(phase == PHASE_CMDOUT) {
		    void *d;
		    unsigned long count;

		if (!cmd->SCp.this_residual && cmd->SCp.buffers_residual) {
			count = cmd->SCp.buffer->length;
			d = SGADDR(cmd->SCp.buffer);
		} else {
			count = cmd->SCp.this_residual;
			d = cmd->SCp.ptr;
		}
#ifdef REAL_DMA
		
		if((count > SUN3_DMA_MINSIZE) && (sun3_dma_setup_done
						  != cmd))
		{
			if (cmd->request->cmd_type == REQ_TYPE_FS) {
				sun3scsi_dma_setup(d, count,
						   rq_data_dir(cmd->request));
				sun3_dma_setup_done = cmd;
			}
		}
#endif
#ifdef SUN3_SCSI_VME
		dregs->csr |= CSR_INTR;
#endif
	    }

	    
	    if (sink && (phase != PHASE_MSGOUT)) {
		NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(tmp));

		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ATN | 
		    ICR_ASSERT_ACK);
		while (NCR5380_read(STATUS_REG) & SR_REQ);
		NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | 
		    ICR_ASSERT_ATN);
		sink = 0;
		continue;
	    }

	    switch (phase) {
	    case PHASE_DATAOUT:
#if (NDEBUG & NDEBUG_NO_DATAOUT)
		printk("scsi%d: NDEBUG_NO_DATAOUT set, attempted DATAOUT "
		       "aborted\n", HOSTNO);
		sink = 1;
		do_abort(instance);
		cmd->result = DID_ERROR  << 16;
		cmd->scsi_done(cmd);
		return;
#endif
	    case PHASE_DATAIN:
		if (!cmd->SCp.this_residual && cmd->SCp.buffers_residual) {
		    ++cmd->SCp.buffer;
		    --cmd->SCp.buffers_residual;
		    cmd->SCp.this_residual = cmd->SCp.buffer->length;
		    cmd->SCp.ptr = SGADDR(cmd->SCp.buffer);
		    INF_PRINTK("scsi%d: %d bytes and %d buffers left\n",
			       HOSTNO, cmd->SCp.this_residual,
			       cmd->SCp.buffers_residual);
		}



#if defined(REAL_DMA)
		if((transfersize =
		    NCR5380_dma_xfer_len(instance,cmd,phase)) > SUN3_DMA_MINSIZE) {
		    len = transfersize;
		    cmd->SCp.phase = phase;

		    if (NCR5380_transfer_dma(instance, &phase,
			&len, (unsigned char **) &cmd->SCp.ptr)) {
 
			printk(KERN_NOTICE "scsi%d: switching target %d "
			       "lun %d to slow handshake\n", HOSTNO,
			       cmd->device->id, cmd->device->lun);
			cmd->device->borken = 1;
			NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | 
			    ICR_ASSERT_ATN);
			sink = 1;
			do_abort(instance);
			cmd->result = DID_ERROR  << 16;
			cmd->scsi_done(cmd);
			
		    } else {
#ifdef REAL_DMA
				    return;
#else			
			cmd->SCp.this_residual -= transfersize - len;
#endif
		    }
		} else 
#endif 
		  NCR5380_transfer_pio(instance, &phase, 
		    (int *) &cmd->SCp.this_residual, (unsigned char **)
		    &cmd->SCp.ptr);
#ifdef REAL_DMA
		
		if(sun3_dma_setup_done == cmd)
			sun3_dma_setup_done = NULL;
#endif

		break;
	    case PHASE_MSGIN:
		len = 1;
		data = &tmp;
		NCR5380_write(SELECT_ENABLE_REG, 0); 	
		NCR5380_transfer_pio(instance, &phase, &len, &data);
		cmd->SCp.Message = tmp;
		
		switch (tmp) {
#ifdef LINKED
		case LINKED_CMD_COMPLETE:
		case LINKED_FLG_CMD_COMPLETE:
		    
		    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		    
		    LNK_PRINTK("scsi%d: target %d lun %d linked command "
			       "complete.\n", HOSTNO, cmd->device->id, cmd->device->lun);

		    
		    NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);

		    if (!cmd->next_link) {
			 printk(KERN_NOTICE "scsi%d: target %d lun %d "
				"linked command complete, no next_link\n",
				HOSTNO, cmd->device->id, cmd->device->lun);
			    sink = 1;
			    do_abort (instance);
			    return;
		    }

		    initialize_SCp(cmd->next_link);
		    cmd->next_link->tag = cmd->tag;
		    cmd->result = cmd->SCp.Status | (cmd->SCp.Message << 8); 
		    LNK_PRINTK("scsi%d: target %d lun %d linked request "
			       "done, calling scsi_done().\n",
			       HOSTNO, cmd->device->id, cmd->device->lun);
#ifdef NCR5380_STATS
		    collect_stats(hostdata, cmd);
#endif
		    cmd->scsi_done(cmd);
		    cmd = hostdata->connected;
		    break;
#endif 
		case ABORT:
		case COMMAND_COMPLETE: 
		    
		    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		    hostdata->connected = NULL;
		    QU_PRINTK("scsi%d: command for target %d, lun %d "
			      "completed\n", HOSTNO, cmd->device->id, cmd->device->lun);
#ifdef SUPPORT_TAGS
		    cmd_free_tag( cmd );
		    if (status_byte(cmd->SCp.Status) == QUEUE_FULL) {
			TAG_ALLOC *ta = &TagAlloc[cmd->device->id][cmd->device->lun];
			TAG_PRINTK("scsi%d: target %d lun %d returned "
				   "QUEUE_FULL after %d commands\n",
				   HOSTNO, cmd->device->id, cmd->device->lun,
				   ta->nr_allocated);
			if (ta->queue_size > ta->nr_allocated)
			    ta->nr_allocated = ta->queue_size;
		    }
#else
		    hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
#endif
		    
		    NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);


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

		    if ((cmd->cmnd[0] != REQUEST_SENSE) && 
			(status_byte(cmd->SCp.Status) == CHECK_CONDITION)) {
			scsi_eh_prep_cmnd(cmd, &hostdata->ses, NULL, 0, ~0);
			ASEN_PRINTK("scsi%d: performing request sense\n",
				    HOSTNO);

			local_irq_save(flags);
			LIST(cmd,hostdata->issue_queue);
			SET_NEXT(cmd, hostdata->issue_queue);
		        hostdata->issue_queue = (struct scsi_cmnd *) cmd;
		        local_irq_restore(flags);
			QU_PRINTK("scsi%d: REQUEST SENSE added to head of "
				  "issue queue\n", H_NO(cmd));
		   } else
#endif 
		   {
#ifdef NCR5380_STATS
		       collect_stats(hostdata, cmd);
#endif
		       cmd->scsi_done(cmd);
		    }

		    NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
		    NCR5380_write(TARGET_COMMAND_REG, 0);
		    
		    while ((NCR5380_read(STATUS_REG) & SR_BSY) && !hostdata->connected)
			barrier();

		    return;
		case MESSAGE_REJECT:
		    
		    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		    
		    NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
		    switch (hostdata->last_message) {
		    case HEAD_OF_QUEUE_TAG:
		    case ORDERED_QUEUE_TAG:
		    case SIMPLE_QUEUE_TAG:
			cmd->device->tagged_supported = 0;
			hostdata->busy[cmd->device->id] |= (1 << cmd->device->lun);
			cmd->tag = TAG_NONE;
			TAG_PRINTK("scsi%d: target %d lun %d rejected "
				   "QUEUE_TAG message; tagged queuing "
				   "disabled\n",
				   HOSTNO, cmd->device->id, cmd->device->lun);
			break;
		    }
		    break;
		case DISCONNECT:
		    
		    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		    local_irq_save(flags);
		    cmd->device->disconnect = 1;
		    LIST(cmd,hostdata->disconnected_queue);
		    SET_NEXT(cmd, hostdata->disconnected_queue);
		    hostdata->connected = NULL;
		    hostdata->disconnected_queue = cmd;
		    local_irq_restore(flags);
		    QU_PRINTK("scsi%d: command for target %d lun %d was "
			      "moved from connected to the "
			      "disconnected_queue\n", HOSTNO, 
			      cmd->device->id, cmd->device->lun);
		    NCR5380_write(TARGET_COMMAND_REG, 0);

		    
		    NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
		    
		    while ((NCR5380_read(STATUS_REG) & SR_BSY) && !hostdata->connected)
		    	barrier();
#ifdef SUN3_SCSI_VME
		    dregs->csr |= CSR_DMA_ENABLE;
#endif
		    return;
		case SAVE_POINTERS:
		case RESTORE_POINTERS:
		    
		    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
		    
		    NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
		    break;
		case EXTENDED_MESSAGE:
		    extended_msg[0] = EXTENDED_MESSAGE;
		    
		    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);

		    EXT_PRINTK("scsi%d: receiving extended message\n", HOSTNO);

		    len = 2;
		    data = extended_msg + 1;
		    phase = PHASE_MSGIN;
		    NCR5380_transfer_pio(instance, &phase, &len, &data);
		    EXT_PRINTK("scsi%d: length=%d, code=0x%02x\n", HOSTNO,
			       (int)extended_msg[1], (int)extended_msg[2]);

		    if (!len && extended_msg[1] <= 
			(sizeof (extended_msg) - 1)) {
			
			NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);
			len = extended_msg[1] - 1;
			data = extended_msg + 3;
			phase = PHASE_MSGIN;

			NCR5380_transfer_pio(instance, &phase, &len, &data);
			EXT_PRINTK("scsi%d: message received, residual %d\n",
				   HOSTNO, len);

			switch (extended_msg[2]) {
			case EXTENDED_SDTR:
			case EXTENDED_WDTR:
			case EXTENDED_MODIFY_DATA_POINTER:
			case EXTENDED_EXTENDED_IDENTIFY:
			    tmp = 0;
			}
		    } else if (len) {
			printk(KERN_NOTICE "scsi%d: error receiving "
			       "extended message\n", HOSTNO);
			tmp = 0;
		    } else {
			printk(KERN_NOTICE "scsi%d: extended message "
			       "code %02x length %d is too long\n",
			       HOSTNO, extended_msg[2], extended_msg[1]);
			tmp = 0;
		    }
		

		default:
		    if (!tmp) {
			printk(KERN_DEBUG "scsi%d: rejecting message ", HOSTNO);
			spi_print_msg(extended_msg);
			printk("\n");
		    } else if (tmp != EXTENDED_MESSAGE)
			printk(KERN_DEBUG "scsi%d: rejecting unknown "
			       "message %02x from target %d, lun %d\n",
			       HOSTNO, tmp, cmd->device->id, cmd->device->lun);
		    else
			printk(KERN_DEBUG "scsi%d: rejecting unknown "
			       "extended message "
			       "code %02x, length %d from target %d, lun %d\n",
			       HOSTNO, extended_msg[1], extended_msg[0],
			       cmd->device->id, cmd->device->lun);
   

		    msgout = MESSAGE_REJECT;
		    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | 
			ICR_ASSERT_ATN);
		    break;
		} 
		break;
	    case PHASE_MSGOUT:
		len = 1;
		data = &msgout;
		hostdata->last_message = msgout;
		NCR5380_transfer_pio(instance, &phase, &len, &data);
		if (msgout == ABORT) {
#ifdef SUPPORT_TAGS
		    cmd_free_tag( cmd );
#else
		    hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
#endif
		    hostdata->connected = NULL;
		    cmd->result = DID_ERROR << 16;
#ifdef NCR5380_STATS
		    collect_stats(hostdata, cmd);
#endif
		    cmd->scsi_done(cmd);
		    NCR5380_write(SELECT_ENABLE_REG, hostdata->id_mask);
		    return;
		}
		msgout = NOP;
		break;
	    case PHASE_CMDOUT:
		len = cmd->cmd_len;
		data = cmd->cmnd;
		NCR5380_transfer_pio(instance, &phase, &len, 
		    &data);
		break;
	    case PHASE_STATIN:
		len = 1;
		data = &tmp;
		NCR5380_transfer_pio(instance, &phase, &len, &data);
		cmd->SCp.Status = tmp;
		break;
	    default:
		printk("scsi%d: unknown phase\n", HOSTNO);
		NCR_PRINT(NDEBUG_ANY);
	    } 
	}  
    } 
}



static void NCR5380_reselect (struct Scsi_Host *instance)
{
    SETUP_HOSTDATA(instance);
    unsigned char target_mask;
    unsigned char lun;
#ifdef SUPPORT_TAGS
    unsigned char tag;
#endif
    unsigned char msg[3];
    struct scsi_cmnd *tmp = NULL, *prev;


    NCR5380_write(MODE_REG, MR_BASE);
    hostdata->restart_select = 1;

    target_mask = NCR5380_read(CURRENT_SCSI_DATA_REG) & ~(hostdata->id_mask);

    RSL_PRINTK("scsi%d: reselect\n", HOSTNO);


    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_BSY);
    
    while (NCR5380_read(STATUS_REG) & SR_SEL);
    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);


    while (!(NCR5380_read(STATUS_REG) & SR_REQ));

#if 1
    
    NCR5380_write(TARGET_COMMAND_REG, PHASE_SR_TO_TCR(PHASE_MSGIN));

    
    msg[0] = NCR5380_read(CURRENT_SCSI_DATA_REG);
#endif

    if (!(msg[0] & 0x80)) {
	printk(KERN_DEBUG "scsi%d: expecting IDENTIFY message, got ", HOSTNO);
	spi_print_msg(msg);
	do_abort(instance);
	return;
    }
    lun = (msg[0] & 0x07);


    for (tmp = (struct scsi_cmnd *) hostdata->disconnected_queue, prev = NULL;
	 tmp; prev = tmp, tmp = NEXT(tmp) ) {
	if ((target_mask == (1 << tmp->device->id)) && (lun == tmp->device->lun)
#ifdef SUPPORT_TAGS
	    && (tag == tmp->tag) 
#endif
	    ) {
	    if (prev) {
		REMOVE(prev, NEXT(prev), tmp, NEXT(tmp));
		SET_NEXT(prev, NEXT(tmp));
	    } else {
		REMOVE(-1, hostdata->disconnected_queue, tmp, NEXT(tmp));
		hostdata->disconnected_queue = NEXT(tmp);
	    }
	    SET_NEXT(tmp, NULL);
	    break;
	}
    }
    
    if (!tmp) {
	printk(KERN_WARNING "scsi%d: warning: target bitmask %02x lun %d "
#ifdef SUPPORT_TAGS
		"tag %d "
#endif
		"not in disconnected_queue.\n",
		HOSTNO, target_mask, lun
#ifdef SUPPORT_TAGS
		, tag
#endif
		);
	do_abort(instance);
	return;
    }
#if 1
    
    {
	    void *d;
	    unsigned long count;

	    if (!tmp->SCp.this_residual && tmp->SCp.buffers_residual) {
		    count = tmp->SCp.buffer->length;
		    d = SGADDR(tmp->SCp.buffer);
	    } else {
		    count = tmp->SCp.this_residual;
		    d = tmp->SCp.ptr;
	    }
#ifdef REAL_DMA
	    
	    if((count > SUN3_DMA_MINSIZE) && (sun3_dma_setup_done != tmp))
	    {
		    sun3scsi_dma_setup(d, count, rq_data_dir(tmp->request));
		    sun3_dma_setup_done = tmp;
	    }
#endif
    }
#endif

    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_ACK);
    
    NCR5380_write(INITIATOR_COMMAND_REG, ICR_BASE);

#ifdef SUPPORT_TAGS
    tag = TAG_NONE;
    if (phase == PHASE_MSGIN && setup_use_tagged_queuing) {
	
	NCR5380_write( INITIATOR_COMMAND_REG, ICR_BASE );
	len = 2;
	data = msg+1;
	if (!NCR5380_transfer_pio(instance, &phase, &len, &data) &&
	    msg[1] == SIMPLE_QUEUE_TAG)
	    tag = msg[2];
	TAG_PRINTK("scsi%d: target mask %02x, lun %d sent tag %d at "
		   "reselection\n", HOSTNO, target_mask, lun, tag);
    }
#endif
    
    hostdata->connected = tmp;
    RSL_PRINTK("scsi%d: nexus established, target = %d, lun = %d, tag = %d\n",
	       HOSTNO, tmp->target, tmp->lun, tmp->tag);
}



static int NCR5380_abort(struct scsi_cmnd *cmd)
{
    struct Scsi_Host *instance = cmd->device->host;
    SETUP_HOSTDATA(instance);
    struct scsi_cmnd *tmp, **prev;
    unsigned long flags;

    printk(KERN_NOTICE "scsi%d: aborting command\n", HOSTNO);
    scsi_print_command(cmd);

    NCR5380_print_status (instance);

    local_irq_save(flags);
    
    ABRT_PRINTK("scsi%d: abort called basr 0x%02x, sr 0x%02x\n", HOSTNO,
		NCR5380_read(BUS_AND_STATUS_REG),
		NCR5380_read(STATUS_REG));

#if 1

    if (hostdata->connected == cmd) {

	ABRT_PRINTK("scsi%d: aborting connected command\n", HOSTNO);


 

	if (do_abort(instance) == 0) {
	  hostdata->aborted = 1;
	  hostdata->connected = NULL;
	  cmd->result = DID_ABORT << 16;
#ifdef SUPPORT_TAGS
	  cmd_free_tag( cmd );
#else
	  hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
#endif
	  local_irq_restore(flags);
	  cmd->scsi_done(cmd);
	  return SCSI_ABORT_SUCCESS;
	} else {
	  printk("scsi%d: abort of connected command failed!\n", HOSTNO);
	  return SCSI_ABORT_ERROR;
	} 
   }
#endif

    for (prev = (struct scsi_cmnd **) &(hostdata->issue_queue),
	tmp = (struct scsi_cmnd *) hostdata->issue_queue;
	tmp; prev = NEXTADDR(tmp), tmp = NEXT(tmp))
	if (cmd == tmp) {
	    REMOVE(5, *prev, tmp, NEXT(tmp));
	    (*prev) = NEXT(tmp);
	    SET_NEXT(tmp, NULL);
	    tmp->result = DID_ABORT << 16;
	    local_irq_restore(flags);
	    ABRT_PRINTK("scsi%d: abort removed command from issue queue.\n",
			HOSTNO);
	    tmp->scsi_done(tmp);
	    return SCSI_ABORT_SUCCESS;
	}


    if (hostdata->connected) {
	local_irq_restore(flags);
	ABRT_PRINTK("scsi%d: abort failed, command connected.\n", HOSTNO);
        return SCSI_ABORT_SNOOZE;
    }


    for (tmp = (struct scsi_cmnd *) hostdata->disconnected_queue; tmp;
	 tmp = NEXT(tmp)) 
        if (cmd == tmp) {
            local_irq_restore(flags);
	    ABRT_PRINTK("scsi%d: aborting disconnected command.\n", HOSTNO);
  
            if (NCR5380_select (instance, cmd, (int) cmd->tag)) 
		return SCSI_ABORT_BUSY;

	    ABRT_PRINTK("scsi%d: nexus reestablished.\n", HOSTNO);

	    do_abort (instance);

	    local_irq_save(flags);
	    for (prev = (struct scsi_cmnd **) &(hostdata->disconnected_queue),
		tmp = (struct scsi_cmnd *) hostdata->disconnected_queue;
		tmp; prev = NEXTADDR(tmp), tmp = NEXT(tmp) )
		    if (cmd == tmp) {
		    REMOVE(5, *prev, tmp, NEXT(tmp));
		    *prev = NEXT(tmp);
		    SET_NEXT(tmp, NULL);
		    tmp->result = DID_ABORT << 16;
#ifdef SUPPORT_TAGS
		    cmd_free_tag( tmp );
#else
		    hostdata->busy[cmd->device->id] &= ~(1 << cmd->device->lun);
#endif
		    local_irq_restore(flags);
		    tmp->scsi_done(tmp);
		    return SCSI_ABORT_SUCCESS;
		}
	}


    local_irq_restore(flags);
    printk(KERN_INFO "scsi%d: warning : SCSI command probably completed successfully before abortion\n", HOSTNO); 

    return SCSI_ABORT_NOT_RUNNING;
}


 

static int NCR5380_bus_reset(struct scsi_cmnd *cmd)
{
    SETUP_HOSTDATA(cmd->device->host);
    int           i;
    unsigned long flags;
#if 1
    struct scsi_cmnd *connected, *disconnected_queue;
#endif


    NCR5380_print_status (cmd->device->host);

    
    NCR5380_write( TARGET_COMMAND_REG,
		   PHASE_SR_TO_TCR( NCR5380_read(STATUS_REG) ));
    
    NCR5380_write( INITIATOR_COMMAND_REG, ICR_BASE | ICR_ASSERT_RST );
    udelay (40);
    
    NCR5380_write( INITIATOR_COMMAND_REG, ICR_BASE );
    NCR5380_write( MODE_REG, MR_BASE );
    NCR5380_write( TARGET_COMMAND_REG, 0 );
    NCR5380_write( SELECT_ENABLE_REG, 0 );
    (void)NCR5380_read( RESET_PARITY_INTERRUPT_REG );

#if 1 
      

    

    local_irq_save(flags);
    connected = (struct scsi_cmnd *)hostdata->connected;
    hostdata->connected = NULL;
    disconnected_queue = (struct scsi_cmnd *)hostdata->disconnected_queue;
    hostdata->disconnected_queue = NULL;
#ifdef SUPPORT_TAGS
    free_all_tags();
#endif
    for( i = 0; i < 8; ++i )
	hostdata->busy[i] = 0;
#ifdef REAL_DMA
    hostdata->dma_len = 0;
#endif
    local_irq_restore(flags);


    if ((cmd = connected)) {
	ABRT_PRINTK("scsi%d: reset aborted a connected command\n", H_NO(cmd));
	cmd->result = (cmd->result & 0xffff) | (DID_RESET << 16);
	cmd->scsi_done( cmd );
    }

    for (i = 0; (cmd = disconnected_queue); ++i) {
	disconnected_queue = NEXT(cmd);
	SET_NEXT(cmd, NULL);
	cmd->result = (cmd->result & 0xffff) | (DID_RESET << 16);
	cmd->scsi_done( cmd );
    }
    if (i > 0)
	ABRT_PRINTK("scsi: reset aborted %d disconnected command(s)\n", i);


    return SCSI_RESET_SUCCESS | SCSI_RESET_BUS_RESET;
#else 

    



    if (hostdata->issue_queue)
	ABRT_PRINTK("scsi%d: reset aborted issued command(s)\n", H_NO(cmd));
    if (hostdata->connected) 
	ABRT_PRINTK("scsi%d: reset aborted a connected command\n", H_NO(cmd));
    if (hostdata->disconnected_queue)
	ABRT_PRINTK("scsi%d: reset aborted disconnected command(s)\n", H_NO(cmd));

    local_irq_save(flags);
    hostdata->issue_queue = NULL;
    hostdata->connected = NULL;
    hostdata->disconnected_queue = NULL;
#ifdef SUPPORT_TAGS
    free_all_tags();
#endif
    for( i = 0; i < 8; ++i )
	hostdata->busy[i] = 0;
#ifdef REAL_DMA
    hostdata->dma_len = 0;
#endif
    local_irq_restore(flags);

    
    return SCSI_RESET_WAKEUP | SCSI_RESET_BUS_RESET;
#endif 
}

