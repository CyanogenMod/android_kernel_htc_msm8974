/*
 *  drivers/block/ataflop.c
 *
 *  Copyright (C) 1993  Greg Harp
 *  Atari Support by Bjoern Brauel, Roman Hodek
 *
 *  Big cleanup Sep 11..14 1994 Roman Hodek:
 *   - Driver now works interrupt driven
 *   - Support for two drives; should work, but I cannot test that :-(
 *   - Reading is done in whole tracks and buffered to speed up things
 *   - Disk change detection and drive deselecting after motor-off
 *     similar to TOS
 *   - Autodetection of disk format (DD/HD); untested yet, because I
 *     don't have an HD drive :-(
 *
 *  Fixes Nov 13 1994 Martin Schaller:
 *   - Autodetection works now
 *   - Support for 5 1/4'' disks
 *   - Removed drive type (unknown on atari)
 *   - Do seeks with 8 Mhz
 *
 *  Changes by Andreas Schwab:
 *   - After errors in multiple read mode try again reading single sectors
 *  (Feb 1995):
 *   - Clean up error handling
 *   - Set blk_size for proper size checking
 *   - Initialize track register when testing presence of floppy
 *   - Implement some ioctl's
 *
 *  Changes by Torsten Lang:
 *   - When probing the floppies we should add the FDCCMDADD_H flag since
 *     the FDC will otherwise wait forever when no disk is inserted...
 *
 * ++ Freddi Aschwanden (fa) 20.9.95 fixes for medusa:
 *  - MFPDELAY() after each FDC access -> atari 
 *  - more/other disk formats
 *  - DMA to the block buffer directly if we have a 32bit DMA
 *  - for medusa, the step rate is always 3ms
 *  - on medusa, use only cache_push()
 * Roman:
 *  - Make disk format numbering independent from minors
 *  - Let user set max. supported drive type (speeds up format
 *    detection, saves buffer space)
 *
 * Roman 10/15/95:
 *  - implement some more ioctls
 *  - disk formatting
 *  
 * Andreas 95/12/12:
 *  - increase gap size at start of track for HD/ED disks
 *
 * Michael (MSch) 11/07/96:
 *  - implemented FDSETPRM and FDDEFPRM ioctl
 *
 * Andreas (97/03/19):
 *  - implemented missing BLK* ioctls
 *
 *  Things left to do:
 *   - Formatting
 *   - Maybe a better strategy for disk change detection (does anyone
 *     know one?)
 */

#include <linux/module.h>

#include <linux/fd.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>

#include <asm/atafd.h>
#include <asm/atafdreg.h>
#include <asm/atariints.h>
#include <asm/atari_stdma.h>
#include <asm/atari_stram.h>

#define	FD_MAX_UNITS 2

#undef DEBUG

static DEFINE_MUTEX(ataflop_mutex);
static struct request *fd_request;
static int fdc_queue;

static struct atari_disk_type {
	const char	*name;
	unsigned	spt;		
	unsigned	blocks;		
	unsigned	fdc_speed;	
	unsigned 	stretch;	
} atari_disk_type[] = {
	{ "d360",  9, 720, 0, 0},	
	{ "D360",  9, 720, 0, 1},	
	{ "D720",  9,1440, 0, 0},	
	{ "D820", 10,1640, 0, 0},	
#define	MAX_TYPE_DD 3
	{ "h1200",15,2400, 3, 0},	
	{ "H1440",18,2880, 3, 0},	
	{ "H1640",20,3280, 3, 0},	
#define	MAX_TYPE_HD 6
	{ "E2880",36,5760, 3, 0},	
	{ "E3280",40,6560, 3, 0},	
#define	MAX_TYPE_ED 8
	{ "H1680",21,3360, 3, 0},	
	{ "h410",10,820, 0, 1},		
	{ "h1476",18,2952, 3, 0},	
	{ "H1722",21,3444, 3, 0},	
	{ "h420",10,840, 0, 1},		
	{ "H830",10,1660, 0, 0},	
	{ "h1494",18,2952, 3, 0},	
	{ "H1743",21,3486, 3, 0},	
	{ "h880",11,1760, 0, 0},	
	{ "D1040",13,2080, 0, 0},	
	{ "D1120",14,2240, 0, 0},	
	{ "h1600",20,3200, 3, 0},	
	{ "H1760",22,3520, 3, 0},	
	{ "H1920",24,3840, 3, 0},	
	{ "E3200",40,6400, 3, 0},	
	{ "E3520",44,7040, 3, 0},	
	{ "E3840",48,7680, 3, 0},	
	{ "H1840",23,3680, 3, 0},	
	{ "D800",10,1600, 0, 0},	
};

static int StartDiskType[] = {
	MAX_TYPE_DD,
	MAX_TYPE_HD,
	MAX_TYPE_ED
};

#define	TYPE_DD		0
#define	TYPE_HD		1
#define	TYPE_ED		2

static int DriveType = TYPE_HD;

static DEFINE_SPINLOCK(ataflop_lock);

static struct {
	int 	 index;
	unsigned drive_types;
} minor2disktype[] = {
	{  0, TYPE_DD },	
	{  4, TYPE_HD },	
	{  1, TYPE_DD },	
	{  2, TYPE_DD },	
	{  1, TYPE_DD },	
	{  2, TYPE_DD },	
	{  5, TYPE_HD },	
	{  7, TYPE_ED },	
	{  8, TYPE_ED },	
	{  5, TYPE_HD },	
	{  9, TYPE_HD },	
	{ 10, TYPE_DD },	
	{  3, TYPE_DD },	
	{ 11, TYPE_HD },	
	{ 12, TYPE_HD },	
	{ 13, TYPE_DD },	
	{ 14, TYPE_DD },	
	{ 15, TYPE_HD },	
	{ 16, TYPE_HD },	
	{ 17, TYPE_DD },	
	{ 18, TYPE_DD },	
	{ 19, TYPE_DD },	
	{ 20, TYPE_HD },	
	{ 21, TYPE_HD },	
	{ 22, TYPE_HD },	
	{ 23, TYPE_ED },	
	{ 24, TYPE_ED },	
	{ 25, TYPE_ED },	
	{ 26, TYPE_HD },	
	{ 27, TYPE_DD },	
	{  6, TYPE_HD },	
};

#define NUM_DISK_MINORS ARRAY_SIZE(minor2disktype)

#define MAX_DISK_SIZE 3280

static struct atari_disk_type user_params[FD_MAX_UNITS];

static struct atari_disk_type default_params[FD_MAX_UNITS];

static struct atari_floppy_struct {
	int connected;				
	int autoprobe;				

	struct atari_disk_type	*disktype;	

	int track;		
	unsigned int steprate;	
	unsigned int wpstat;	
	int flags;		
	struct gendisk *disk;
	int ref;
	int type;
} unit[FD_MAX_UNITS];

#define	UD	unit[drive]
#define	UDT	unit[drive].disktype
#define	SUD	unit[SelectedDrive]
#define	SUDT	unit[SelectedDrive].disktype


#define FDC_READ(reg) ({			\
    		\
    unsigned short __val;			\
    		\
    dma_wd.dma_mode_status = 0x80 | (reg);	\
    udelay(25);					\
    __val = dma_wd.fdc_acces_seccount;		\
    MFPDELAY();					\
    		\
    __val & 0xff;				\
})

#define FDC_WRITE(reg,val)			\
    do {					\
			\
			\
	dma_wd.dma_mode_status = 0x80 | (reg);	\
	udelay(25);				\
	dma_wd.fdc_acces_seccount = (val);	\
	MFPDELAY();				\
        	\
    } while(0)



static int MaxSectors[] = {
	11, 22, 44
};
static int BufferSize[] = {
	15*512, 30*512, 60*512
};

#define	BUFFER_SIZE	(BufferSize[DriveType])

unsigned char *DMABuffer;			  
static unsigned long PhysDMABuffer;   

static int UseTrackbuffer = -1;		  
module_param(UseTrackbuffer, int, 0);

unsigned char *TrackBuffer;			  
static unsigned long PhysTrackBuffer; 
static int BufferDrive, BufferSide, BufferTrack;
static int read_track;		

#define	SECTOR_BUFFER(sec)	(TrackBuffer + ((sec)-1)*512)
#define	IS_BUFFERED(drive,side,track) \
    (BufferDrive == (drive) && BufferSide == (side) && BufferTrack == (track))

static int SelectedDrive = 0;
static int ReqCmd, ReqBlock;
static int ReqSide, ReqTrack, ReqSector, ReqCnt;
static int HeadSettleFlag = 0;
static unsigned char *ReqData, *ReqBuffer;
static int MotorOn = 0, MotorOffTrys;
static int IsFormatting = 0, FormatError;

static int UserSteprate[FD_MAX_UNITS] = { -1, -1 };
module_param_array(UserSteprate, int, NULL, 0);

static volatile int fdc_busy = 0;
static DECLARE_WAIT_QUEUE_HEAD(fdc_wait);
static DECLARE_WAIT_QUEUE_HEAD(format_wait);

static unsigned long changed_floppies = 0xff, fake_change = 0;
#define	CHECK_CHANGE_DELAY	HZ/2

#define	FD_MOTOR_OFF_DELAY	(3*HZ)
#define	FD_MOTOR_OFF_MAXTRY	(10*20)

#define FLOPPY_TIMEOUT		(6*HZ)
#define RECALIBRATE_ERRORS	4	
#define MAX_ERRORS		8	


static int Probing = 0;

static int NeedSeek = 0;


#ifdef DEBUG
#define DPRINT(a)	printk a
#else
#define DPRINT(a)
#endif


static void fd_select_side( int side );
static void fd_select_drive( int drive );
static void fd_deselect( void );
static void fd_motor_off_timer( unsigned long dummy );
static void check_change( unsigned long dummy );
static irqreturn_t floppy_irq (int irq, void *dummy);
static void fd_error( void );
static int do_format(int drive, int type, struct atari_format_descr *desc);
static void do_fd_action( int drive );
static void fd_calibrate( void );
static void fd_calibrate_done( int status );
static void fd_seek( void );
static void fd_seek_done( int status );
static void fd_rwsec( void );
static void fd_readtrack_check( unsigned long dummy );
static void fd_rwsec_done( int status );
static void fd_rwsec_done1(int status);
static void fd_writetrack( void );
static void fd_writetrack_done( int status );
static void fd_times_out( unsigned long dummy );
static void finish_fdc( void );
static void finish_fdc_done( int dummy );
static void setup_req_params( int drive );
static void redo_fd_request( void);
static int fd_locked_ioctl(struct block_device *bdev, fmode_t mode, unsigned int
                     cmd, unsigned long param);
static void fd_probe( int drive );
static int fd_test_drive_present( int drive );
static void config_types( void );
static int floppy_open(struct block_device *bdev, fmode_t mode);
static int floppy_release(struct gendisk *disk, fmode_t mode);


static DEFINE_TIMER(motor_off_timer, fd_motor_off_timer, 0, 0);
static DEFINE_TIMER(readtrack_timer, fd_readtrack_check, 0, 0);
static DEFINE_TIMER(timeout_timer, fd_times_out, 0, 0);
static DEFINE_TIMER(fd_timer, check_change, 0, 0);
	
static void fd_end_request_cur(int err)
{
	if (!__blk_end_request_cur(fd_request, err))
		fd_request = NULL;
}

static inline void start_motor_off_timer(void)
{
	mod_timer(&motor_off_timer, jiffies + FD_MOTOR_OFF_DELAY);
	MotorOffTrys = 0;
}

static inline void start_check_change_timer( void )
{
	mod_timer(&fd_timer, jiffies + CHECK_CHANGE_DELAY);
}

static inline void start_timeout(void)
{
	mod_timer(&timeout_timer, jiffies + FLOPPY_TIMEOUT);
}

static inline void stop_timeout(void)
{
	del_timer(&timeout_timer);
}


static void fd_select_side( int side )
{
	unsigned long flags;

	
	local_irq_save(flags);
  
	sound_ym.rd_data_reg_sel = 14; 
	sound_ym.wd_data = (side == 0) ? sound_ym.rd_data_reg_sel | 0x01 :
	                                 sound_ym.rd_data_reg_sel & 0xfe;

	local_irq_restore(flags);
}



static void fd_select_drive( int drive )
{
	unsigned long flags;
	unsigned char tmp;
  
	if (drive == SelectedDrive)
	  return;

	
	local_irq_save(flags);
	sound_ym.rd_data_reg_sel = 14; 
	tmp = sound_ym.rd_data_reg_sel;
	sound_ym.wd_data = (tmp | DSKDRVNONE) & ~(drive == 0 ? DSKDRV0 : DSKDRV1);
	atari_dont_touch_floppy_select = 1;
	local_irq_restore(flags);

	
	FDC_WRITE( FDCREG_TRACK, UD.track );
	udelay(25);

	
	if (UDT)
		if (ATARIHW_PRESENT(FDCSPEED))
			dma_wd.fdc_speed = UDT->fdc_speed;
	
	SelectedDrive = drive;
}



static void fd_deselect( void )
{
	unsigned long flags;

	
	local_irq_save(flags);
	atari_dont_touch_floppy_select = 0;
	sound_ym.rd_data_reg_sel=14;	
	sound_ym.wd_data = (sound_ym.rd_data_reg_sel |
			    (MACH_IS_FALCON ? 3 : 7)); 
	SelectedDrive = -1;
	local_irq_restore(flags);
}



static void fd_motor_off_timer( unsigned long dummy )
{
	unsigned char status;

	if (SelectedDrive < 0)
		
		return;

	if (stdma_islocked())
		goto retry;

	status = FDC_READ( FDCREG_STATUS );

	if (!(status & 0x80)) {
		
		MotorOn = 0;
		fd_deselect();
		return;
	}
	

  retry:
	mod_timer(&motor_off_timer,
		  jiffies + (MotorOffTrys++ < FD_MOTOR_OFF_MAXTRY ? HZ/20 : HZ/2));
}



static void check_change( unsigned long dummy )
{
	static int    drive = 0;

	unsigned long flags;
	unsigned char old_porta;
	int			  stat;

	if (++drive > 1 || !UD.connected)
		drive = 0;

	
	local_irq_save(flags);

	if (!stdma_islocked()) {
		sound_ym.rd_data_reg_sel = 14;
		old_porta = sound_ym.rd_data_reg_sel;
		sound_ym.wd_data = (old_porta | DSKDRVNONE) &
			               ~(drive == 0 ? DSKDRV0 : DSKDRV1);
		stat = !!(FDC_READ( FDCREG_STATUS ) & FDCSTAT_WPROT);
		sound_ym.wd_data = old_porta;

		if (stat != UD.wpstat) {
			DPRINT(( "wpstat[%d] = %d\n", drive, stat ));
			UD.wpstat = stat;
			set_bit (drive, &changed_floppies);
		}
	}
	local_irq_restore(flags);

	start_check_change_timer();
}

 

static inline void set_head_settle_flag(void)
{
	HeadSettleFlag = FDCCMDADD_E;
}

static inline int get_head_settle_flag(void)
{
	int	tmp = HeadSettleFlag;
	HeadSettleFlag = 0;
	return( tmp );
}

static inline void copy_buffer(void *from, void *to)
{
	ulong *p1 = (ulong *)from, *p2 = (ulong *)to;
	int cnt;

	for (cnt = 512/4; cnt; cnt--)
		*p2++ = *p1++;
}

  
  


static void (*FloppyIRQHandler)( int status ) = NULL;

static irqreturn_t floppy_irq (int irq, void *dummy)
{
	unsigned char status;
	void (*handler)( int );

	handler = xchg(&FloppyIRQHandler, NULL);

	if (handler) {
		nop();
		status = FDC_READ( FDCREG_STATUS );
		DPRINT(("FDC irq, status = %02x handler = %08lx\n",status,(unsigned long)handler));
		handler( status );
	}
	else {
		DPRINT(("FDC irq, no handler\n"));
	}
	return IRQ_HANDLED;
}



static void fd_error( void )
{
	if (IsFormatting) {
		IsFormatting = 0;
		FormatError = 1;
		wake_up( &format_wait );
		return;
	}

	if (!fd_request)
		return;

	fd_request->errors++;
	if (fd_request->errors >= MAX_ERRORS) {
		printk(KERN_ERR "fd%d: too many errors.\n", SelectedDrive );
		fd_end_request_cur(-EIO);
	}
	else if (fd_request->errors == RECALIBRATE_ERRORS) {
		printk(KERN_WARNING "fd%d: recalibrating\n", SelectedDrive );
		if (SelectedDrive != -1)
			SUD.track = -1;
	}
	redo_fd_request();
}



#define	SET_IRQ_HANDLER(proc) do { FloppyIRQHandler = (proc); } while(0)



#define FILL(n,val)		\
    do {			\
	memset( p, val, n );	\
	p += n;			\
    } while(0)

static int do_format(int drive, int type, struct atari_format_descr *desc)
{
	unsigned char	*p;
	int sect, nsect;
	unsigned long	flags;

	DPRINT(("do_format( dr=%d tr=%d he=%d offs=%d )\n",
		drive, desc->track, desc->head, desc->sect_offset ));

	local_irq_save(flags);
	while( fdc_busy ) sleep_on( &fdc_wait );
	fdc_busy = 1;
	stdma_lock(floppy_irq, NULL);
	atari_turnon_irq( IRQ_MFP_FDC ); 
	local_irq_restore(flags);

	if (type) {
		if (--type >= NUM_DISK_MINORS ||
		    minor2disktype[type].drive_types > DriveType) {
			redo_fd_request();
			return -EINVAL;
		}
		type = minor2disktype[type].index;
		UDT = &atari_disk_type[type];
	}

	if (!UDT || desc->track >= UDT->blocks/UDT->spt/2 || desc->head >= 2) {
		redo_fd_request();
		return -EINVAL;
	}

	nsect = UDT->spt;
	p = TrackBuffer;
	BufferDrive = -1;
	
	del_timer( &motor_off_timer );

	FILL( 60 * (nsect / 9), 0x4e );
	for( sect = 0; sect < nsect; ++sect ) {
		FILL( 12, 0 );
		FILL( 3, 0xf5 );
		*p++ = 0xfe;
		*p++ = desc->track;
		*p++ = desc->head;
		*p++ = (nsect + sect - desc->sect_offset) % nsect + 1;
		*p++ = 2;
		*p++ = 0xf7;
		FILL( 22, 0x4e );
		FILL( 12, 0 );
		FILL( 3, 0xf5 );
		*p++ = 0xfb;
		FILL( 512, 0xe5 );
		*p++ = 0xf7;
		FILL( 40, 0x4e );
	}
	FILL( TrackBuffer+BUFFER_SIZE-p, 0x4e );

	IsFormatting = 1;
	FormatError = 0;
	ReqTrack = desc->track;
	ReqSide  = desc->head;
	do_fd_action( drive );

	sleep_on( &format_wait );

	redo_fd_request();
	return( FormatError ? -EIO : 0 );	
}



static void do_fd_action( int drive )
{
	DPRINT(("do_fd_action\n"));
	
	if (UseTrackbuffer && !IsFormatting) {
	repeat:
	    if (IS_BUFFERED( drive, ReqSide, ReqTrack )) {
		if (ReqCmd == READ) {
		    copy_buffer( SECTOR_BUFFER(ReqSector), ReqData );
		    if (++ReqCnt < blk_rq_cur_sectors(fd_request)) {
			
			setup_req_params( drive );
			goto repeat;
		    }
		    else {
			
			fd_end_request_cur(0);
			redo_fd_request();
			return;
		    }
		}
		else {
		    copy_buffer( ReqData, SECTOR_BUFFER(ReqSector) );
		}
	    }
	}

	if (SelectedDrive != drive)
		fd_select_drive( drive );
    
	if (UD.track == -1)
		fd_calibrate();
	else if (UD.track != ReqTrack << UDT->stretch)
		fd_seek();
	else if (IsFormatting)
		fd_writetrack();
	else
		fd_rwsec();
}



static void fd_calibrate( void )
{
	if (SUD.track >= 0) {
		fd_calibrate_done( 0 );
		return;
	}

	if (ATARIHW_PRESENT(FDCSPEED))
		dma_wd.fdc_speed = 0; 	;
	DPRINT(("fd_calibrate\n"));
	SET_IRQ_HANDLER( fd_calibrate_done );
	
	FDC_WRITE( FDCREG_CMD, FDCCMD_RESTORE | SUD.steprate );

	NeedSeek = 1;
	MotorOn = 1;
	start_timeout();
	
}


static void fd_calibrate_done( int status )
{
	DPRINT(("fd_calibrate_done()\n"));
	stop_timeout();
    
	
	if (ATARIHW_PRESENT(FDCSPEED))
		dma_wd.fdc_speed = SUDT->fdc_speed;
	if (status & FDCSTAT_RECNF) {
		printk(KERN_ERR "fd%d: restore failed\n", SelectedDrive );
		fd_error();
	}
	else {
		SUD.track = 0;
		fd_seek();
	}
}
  
  
  
static void fd_seek( void )
{
	if (SUD.track == ReqTrack << SUDT->stretch) {
		fd_seek_done( 0 );
		return;
	}

	if (ATARIHW_PRESENT(FDCSPEED)) {
		dma_wd.fdc_speed = 0;	
		MFPDELAY();
	}

	DPRINT(("fd_seek() to track %d\n",ReqTrack));
	FDC_WRITE( FDCREG_DATA, ReqTrack << SUDT->stretch);
	udelay(25);
	SET_IRQ_HANDLER( fd_seek_done );
	FDC_WRITE( FDCREG_CMD, FDCCMD_SEEK | SUD.steprate );

	MotorOn = 1;
	set_head_settle_flag();
	start_timeout();
	
}


static void fd_seek_done( int status )
{
	DPRINT(("fd_seek_done()\n"));
	stop_timeout();
	
	
	if (ATARIHW_PRESENT(FDCSPEED))
		dma_wd.fdc_speed = SUDT->fdc_speed;
	if (status & FDCSTAT_RECNF) {
		printk(KERN_ERR "fd%d: seek error (to track %d)\n",
				SelectedDrive, ReqTrack );
		
		SUD.track = -1;
		fd_error();
	}
	else {
		SUD.track = ReqTrack << SUDT->stretch;
		NeedSeek = 0;
		if (IsFormatting)
			fd_writetrack();
		else
			fd_rwsec();
	}
}



static int MultReadInProgress = 0;


static void fd_rwsec( void )
{
	unsigned long paddr, flags;
	unsigned int  rwflag, old_motoron;
	unsigned int track;
	
	DPRINT(("fd_rwsec(), Sec=%d, Access=%c\n",ReqSector, ReqCmd == WRITE ? 'w' : 'r' ));
	if (ReqCmd == WRITE) {
		if (ATARIHW_PRESENT(EXTD_DMA)) {
			paddr = virt_to_phys(ReqData);
		}
		else {
			copy_buffer( ReqData, DMABuffer );
			paddr = PhysDMABuffer;
		}
		dma_cache_maintenance( paddr, 512, 1 );
		rwflag = 0x100;
	}
	else {
		if (read_track)
			paddr = PhysTrackBuffer;
		else
			paddr = ATARIHW_PRESENT(EXTD_DMA) ? 
				virt_to_phys(ReqData) : PhysDMABuffer;
		rwflag = 0;
	}

	fd_select_side( ReqSide );
  
	
	FDC_WRITE( FDCREG_SECTOR, read_track ? 1 : ReqSector );
	MFPDELAY();
	
	if (SUDT->stretch) {
		track = FDC_READ( FDCREG_TRACK);
		MFPDELAY();
		FDC_WRITE( FDCREG_TRACK, track >> SUDT->stretch);
	}
	udelay(25);
  
	
	local_irq_save(flags);
	dma_wd.dma_lo = (unsigned char)paddr;
	MFPDELAY();
	paddr >>= 8;
	dma_wd.dma_md = (unsigned char)paddr;
	MFPDELAY();
	paddr >>= 8;
	if (ATARIHW_PRESENT(EXTD_DMA))
		st_dma_ext_dmahi = (unsigned short)paddr;
	else
		dma_wd.dma_hi = (unsigned char)paddr;
	MFPDELAY();
	local_irq_restore(flags);
  
	  
	dma_wd.dma_mode_status = 0x90 | rwflag;  
	MFPDELAY();
	dma_wd.dma_mode_status = 0x90 | (rwflag ^ 0x100);  
	MFPDELAY();
	dma_wd.dma_mode_status = 0x90 | rwflag;
	MFPDELAY();
  
	
	dma_wd.fdc_acces_seccount = read_track ? SUDT->spt : 1;
  
	udelay(25);  
  
	
	dma_wd.dma_mode_status = FDCSELREG_STP | rwflag;
	udelay(25);
	SET_IRQ_HANDLER( fd_rwsec_done );
	dma_wd.fdc_acces_seccount =
	  (get_head_settle_flag() |
	   (rwflag ? FDCCMD_WRSEC : (FDCCMD_RDSEC | (read_track ? FDCCMDADD_M : 0))));

	old_motoron = MotorOn;
	MotorOn = 1;
	NeedSeek = 1;
	

	if (read_track) {
		MultReadInProgress = 1;
		mod_timer(&readtrack_timer,
			  
			  jiffies + HZ/5 + (old_motoron ? 0 : HZ));
	}
	start_timeout();
}

    
static void fd_readtrack_check( unsigned long dummy )
{
	unsigned long flags, addr, addr2;

	local_irq_save(flags);

	if (!MultReadInProgress) {
		local_irq_restore(flags);
		return;
	}

	
	
	addr = 0;
	do {
		addr2 = addr;
		addr = dma_wd.dma_lo & 0xff;
		MFPDELAY();
		addr |= (dma_wd.dma_md & 0xff) << 8;
		MFPDELAY();
		if (ATARIHW_PRESENT( EXTD_DMA ))
			addr |= (st_dma_ext_dmahi & 0xffff) << 16;
		else
			addr |= (dma_wd.dma_hi & 0xff) << 16;
		MFPDELAY();
	} while(addr != addr2);
  
	if (addr >= PhysTrackBuffer + SUDT->spt*512) {
		SET_IRQ_HANDLER( NULL );
		MultReadInProgress = 0;
		local_irq_restore(flags);
		DPRINT(("fd_readtrack_check(): done\n"));
		FDC_WRITE( FDCREG_CMD, FDCCMD_FORCI );
		udelay(25);

		fd_rwsec_done1(0);
	}
	else {
		
		local_irq_restore(flags);
		DPRINT(("fd_readtrack_check(): not yet finished\n"));
		mod_timer(&readtrack_timer, jiffies + HZ/5/10);
	}
}


static void fd_rwsec_done( int status )
{
	DPRINT(("fd_rwsec_done()\n"));

	if (read_track) {
		del_timer(&readtrack_timer);
		if (!MultReadInProgress)
			return;
		MultReadInProgress = 0;
	}
	fd_rwsec_done1(status);
}

static void fd_rwsec_done1(int status)
{
	unsigned int track;

	stop_timeout();
	
	
	if (SUDT->stretch) {
		track = FDC_READ( FDCREG_TRACK);
		MFPDELAY();
		FDC_WRITE( FDCREG_TRACK, track << SUDT->stretch);
	}

	if (!UseTrackbuffer) {
		dma_wd.dma_mode_status = 0x90;
		MFPDELAY();
		if (!(dma_wd.dma_mode_status & 0x01)) {
			printk(KERN_ERR "fd%d: DMA error\n", SelectedDrive );
			goto err_end;
		}
	}
	MFPDELAY();

	if (ReqCmd == WRITE && (status & FDCSTAT_WPROT)) {
		printk(KERN_NOTICE "fd%d: is write protected\n", SelectedDrive );
		goto err_end;
	}	
	if ((status & FDCSTAT_RECNF) &&
	    !(read_track && FDC_READ(FDCREG_SECTOR) > SUDT->spt)) {
		if (Probing) {
			if (SUDT > atari_disk_type) {
			    if (SUDT[-1].blocks > ReqBlock) {
				
				SUDT--;
				set_capacity(unit[SelectedDrive].disk,
							SUDT->blocks);
			    } else
				Probing = 0;
			}
			else {
				if (SUD.flags & FTD_MSG)
					printk(KERN_INFO "fd%d: Auto-detected floppy type %s\n",
					       SelectedDrive, SUDT->name );
				Probing=0;
			}
		} else {	
			if (SUD.autoprobe) {
				SUDT = atari_disk_type + StartDiskType[DriveType];
				set_capacity(unit[SelectedDrive].disk,
							SUDT->blocks);
				Probing = 1;
			}
		}
		if (Probing) {
			if (ATARIHW_PRESENT(FDCSPEED)) {
				dma_wd.fdc_speed = SUDT->fdc_speed;
				MFPDELAY();
			}
			setup_req_params( SelectedDrive );
			BufferDrive = -1;
			do_fd_action( SelectedDrive );
			return;
		}

		printk(KERN_ERR "fd%d: sector %d not found (side %d, track %d)\n",
		       SelectedDrive, FDC_READ (FDCREG_SECTOR), ReqSide, ReqTrack );
		goto err_end;
	}
	if (status & FDCSTAT_CRC) {
		printk(KERN_ERR "fd%d: CRC error (side %d, track %d, sector %d)\n",
		       SelectedDrive, ReqSide, ReqTrack, FDC_READ (FDCREG_SECTOR) );
		goto err_end;
	}
	if (status & FDCSTAT_LOST) {
		printk(KERN_ERR "fd%d: lost data (side %d, track %d, sector %d)\n",
		       SelectedDrive, ReqSide, ReqTrack, FDC_READ (FDCREG_SECTOR) );
		goto err_end;
	}

	Probing = 0;
	
	if (ReqCmd == READ) {
		if (!read_track) {
			void *addr;
			addr = ATARIHW_PRESENT( EXTD_DMA ) ? ReqData : DMABuffer;
			dma_cache_maintenance( virt_to_phys(addr), 512, 0 );
			if (!ATARIHW_PRESENT( EXTD_DMA ))
				copy_buffer (addr, ReqData);
		} else {
			dma_cache_maintenance( PhysTrackBuffer, MaxSectors[DriveType] * 512, 0 );
			BufferDrive = SelectedDrive;
			BufferSide  = ReqSide;
			BufferTrack = ReqTrack;
			copy_buffer (SECTOR_BUFFER (ReqSector), ReqData);
		}
	}
  
	if (++ReqCnt < blk_rq_cur_sectors(fd_request)) {
		
		setup_req_params( SelectedDrive );
		do_fd_action( SelectedDrive );
	}
	else {
		
		fd_end_request_cur(0);
		redo_fd_request();
	}
	return;
  
  err_end:
	BufferDrive = -1;
	fd_error();
}


static void fd_writetrack( void )
{
	unsigned long paddr, flags;
	unsigned int track;
	
	DPRINT(("fd_writetrack() Tr=%d Si=%d\n", ReqTrack, ReqSide ));

	paddr = PhysTrackBuffer;
	dma_cache_maintenance( paddr, BUFFER_SIZE, 1 );

	fd_select_side( ReqSide );
  
	
	if (SUDT->stretch) {
		track = FDC_READ( FDCREG_TRACK);
		MFPDELAY();
		FDC_WRITE(FDCREG_TRACK,track >> SUDT->stretch);
	}
	udelay(40);
  
	
	local_irq_save(flags);
	dma_wd.dma_lo = (unsigned char)paddr;
	MFPDELAY();
	paddr >>= 8;
	dma_wd.dma_md = (unsigned char)paddr;
	MFPDELAY();
	paddr >>= 8;
	if (ATARIHW_PRESENT( EXTD_DMA ))
		st_dma_ext_dmahi = (unsigned short)paddr;
	else
		dma_wd.dma_hi = (unsigned char)paddr;
	MFPDELAY();
	local_irq_restore(flags);
  
	  
	dma_wd.dma_mode_status = 0x190;  
	MFPDELAY();
	dma_wd.dma_mode_status = 0x90;  
	MFPDELAY();
	dma_wd.dma_mode_status = 0x190;
	MFPDELAY();
  
	
	dma_wd.fdc_acces_seccount = BUFFER_SIZE/512;
	udelay(40);  
  
	
	dma_wd.dma_mode_status = FDCSELREG_STP | 0x100;
	udelay(40);
	SET_IRQ_HANDLER( fd_writetrack_done );
	dma_wd.fdc_acces_seccount = FDCCMD_WRTRA | get_head_settle_flag(); 

	MotorOn = 1;
	start_timeout();
	
}


static void fd_writetrack_done( int status )
{
	DPRINT(("fd_writetrack_done()\n"));

	stop_timeout();

	if (status & FDCSTAT_WPROT) {
		printk(KERN_NOTICE "fd%d: is write protected\n", SelectedDrive );
		goto err_end;
	}	
	if (status & FDCSTAT_LOST) {
		printk(KERN_ERR "fd%d: lost data (side %d, track %d)\n",
				SelectedDrive, ReqSide, ReqTrack );
		goto err_end;
	}

	wake_up( &format_wait );
	return;

  err_end:
	fd_error();
}

static void fd_times_out( unsigned long dummy )
{
	atari_disable_irq( IRQ_MFP_FDC );
	if (!FloppyIRQHandler) goto end; 

	SET_IRQ_HANDLER( NULL );
	if (UseTrackbuffer)
		del_timer( &readtrack_timer );
	FDC_WRITE( FDCREG_CMD, FDCCMD_FORCI );
	udelay( 25 );
	
	printk(KERN_ERR "floppy timeout\n" );
	fd_error();
  end:
	atari_enable_irq( IRQ_MFP_FDC );
}



static void finish_fdc( void )
{
	if (!NeedSeek) {
		finish_fdc_done( 0 );
	}
	else {
		DPRINT(("finish_fdc: dummy seek started\n"));
		FDC_WRITE (FDCREG_DATA, SUD.track);
		SET_IRQ_HANDLER( finish_fdc_done );
		FDC_WRITE (FDCREG_CMD, FDCCMD_SEEK);
		MotorOn = 1;
		start_timeout();
	  }
}


static void finish_fdc_done( int dummy )
{
	unsigned long flags;

	DPRINT(("finish_fdc_done entered\n"));
	stop_timeout();
	NeedSeek = 0;

	if (timer_pending(&fd_timer) && time_before(fd_timer.expires, jiffies + 5))
		mod_timer(&fd_timer, jiffies + 5);
	else
		start_check_change_timer();
	start_motor_off_timer();

	local_irq_save(flags);
	stdma_release();
	fdc_busy = 0;
	wake_up( &fdc_wait );
	local_irq_restore(flags);

	DPRINT(("finish_fdc() finished\n"));
}


static unsigned int floppy_check_events(struct gendisk *disk,
					unsigned int clearing)
{
	struct atari_floppy_struct *p = disk->private_data;
	unsigned int drive = p - unit;
	if (test_bit (drive, &fake_change)) {
		
		return DISK_EVENT_MEDIA_CHANGE;
	}
	if (test_bit (drive, &changed_floppies)) {
		
		return DISK_EVENT_MEDIA_CHANGE;
	}
	if (UD.wpstat) {
		return DISK_EVENT_MEDIA_CHANGE;
	}

	return 0;
}

static int floppy_revalidate(struct gendisk *disk)
{
	struct atari_floppy_struct *p = disk->private_data;
	unsigned int drive = p - unit;

	if (test_bit(drive, &changed_floppies) ||
	    test_bit(drive, &fake_change) ||
	    p->disktype == 0) {
		if (UD.flags & FTD_MSG)
			printk(KERN_ERR "floppy: clear format %p!\n", UDT);
		BufferDrive = -1;
		clear_bit(drive, &fake_change);
		clear_bit(drive, &changed_floppies);
		if (default_params[drive].blocks == 0)
			UDT = NULL;
		else
			UDT = &default_params[drive];
	}
	return 0;
}



static void setup_req_params( int drive )
{
	int block = ReqBlock + ReqCnt;

	ReqTrack = block / UDT->spt;
	ReqSector = block - ReqTrack * UDT->spt + 1;
	ReqSide = ReqTrack & 1;
	ReqTrack >>= 1;
	ReqData = ReqBuffer + 512 * ReqCnt;

	if (UseTrackbuffer)
		read_track = (ReqCmd == READ && fd_request->errors == 0);
	else
		read_track = 0;

	DPRINT(("Request params: Si=%d Tr=%d Se=%d Data=%08lx\n",ReqSide,
			ReqTrack, ReqSector, (unsigned long)ReqData ));
}

static struct request *set_next_request(void)
{
	struct request_queue *q;
	int old_pos = fdc_queue;
	struct request *rq = NULL;

	do {
		q = unit[fdc_queue].disk->queue;
		if (++fdc_queue == FD_MAX_UNITS)
			fdc_queue = 0;
		if (q) {
			rq = blk_fetch_request(q);
			if (rq)
				break;
		}
	} while (fdc_queue != old_pos);

	return rq;
}


static void redo_fd_request(void)
{
	int drive, type;
	struct atari_floppy_struct *floppy;

	DPRINT(("redo_fd_request: fd_request=%p dev=%s fd_request->sector=%ld\n",
		fd_request, fd_request ? fd_request->rq_disk->disk_name : "",
		fd_request ? blk_rq_pos(fd_request) : 0 ));

	IsFormatting = 0;

repeat:
	if (!fd_request) {
		fd_request = set_next_request();
		if (!fd_request)
			goto the_end;
	}

	floppy = fd_request->rq_disk->private_data;
	drive = floppy - unit;
	type = floppy->type;
	
	if (!UD.connected) {
		
		printk(KERN_ERR "Unknown Device: fd%d\n", drive );
		fd_end_request_cur(-EIO);
		goto repeat;
	}
		
	if (type == 0) {
		if (!UDT) {
			Probing = 1;
			UDT = atari_disk_type + StartDiskType[DriveType];
			set_capacity(floppy->disk, UDT->blocks);
			UD.autoprobe = 1;
		}
	} 
	else {
		
		if (--type >= NUM_DISK_MINORS) {
			printk(KERN_WARNING "fd%d: invalid disk format", drive );
			fd_end_request_cur(-EIO);
			goto repeat;
		}
		if (minor2disktype[type].drive_types > DriveType)  {
			printk(KERN_WARNING "fd%d: unsupported disk format", drive );
			fd_end_request_cur(-EIO);
			goto repeat;
		}
		type = minor2disktype[type].index;
		UDT = &atari_disk_type[type];
		set_capacity(floppy->disk, UDT->blocks);
		UD.autoprobe = 0;
	}
	
	if (blk_rq_pos(fd_request) + 1 > UDT->blocks) {
		fd_end_request_cur(-EIO);
		goto repeat;
	}

	
	del_timer( &motor_off_timer );
		
	ReqCnt = 0;
	ReqCmd = rq_data_dir(fd_request);
	ReqBlock = blk_rq_pos(fd_request);
	ReqBuffer = fd_request->buffer;
	setup_req_params( drive );
	do_fd_action( drive );

	return;

  the_end:
	finish_fdc();
}


void do_fd_request(struct request_queue * q)
{
	DPRINT(("do_fd_request for pid %d\n",current->pid));
	while( fdc_busy ) sleep_on( &fdc_wait );
	fdc_busy = 1;
	stdma_lock(floppy_irq, NULL);

	atari_disable_irq( IRQ_MFP_FDC );
	redo_fd_request();
	atari_enable_irq( IRQ_MFP_FDC );
}

static int fd_locked_ioctl(struct block_device *bdev, fmode_t mode,
		    unsigned int cmd, unsigned long param)
{
	struct gendisk *disk = bdev->bd_disk;
	struct atari_floppy_struct *floppy = disk->private_data;
	int drive = floppy - unit;
	int type = floppy->type;
	struct atari_format_descr fmt_desc;
	struct atari_disk_type *dtp;
	struct floppy_struct getprm;
	int settype;
	struct floppy_struct setprm;
	void __user *argp = (void __user *)param;

	switch (cmd) {
	case FDGETPRM:
		if (type) {
			if (--type >= NUM_DISK_MINORS)
				return -ENODEV;
			if (minor2disktype[type].drive_types > DriveType)
				return -ENODEV;
			type = minor2disktype[type].index;
			dtp = &atari_disk_type[type];
			if (UD.flags & FTD_MSG)
			    printk (KERN_ERR "floppy%d: found dtp %p name %s!\n",
			        drive, dtp, dtp->name);
		}
		else {
			if (!UDT)
				return -ENXIO;
			else
				dtp = UDT;
		}
		memset((void *)&getprm, 0, sizeof(getprm));
		getprm.size = dtp->blocks;
		getprm.sect = dtp->spt;
		getprm.head = 2;
		getprm.track = dtp->blocks/dtp->spt/2;
		getprm.stretch = dtp->stretch;
		if (copy_to_user(argp, &getprm, sizeof(getprm)))
			return -EFAULT;
		return 0;
	}
	switch (cmd) {
	case FDSETPRM:
	case FDDEFPRM:

		
		if (floppy->ref != 1 && floppy->ref != -1)
			return -EBUSY;
		if (copy_from_user(&setprm, argp, sizeof(setprm)))
			return -EFAULT;

		if (floppy_check_events(disk, 0))
		        floppy_revalidate(disk);

		if (UD.flags & FTD_MSG)
		    printk (KERN_INFO "floppy%d: setting size %d spt %d str %d!\n",
			drive, setprm.size, setprm.sect, setprm.stretch);

		
		if (type) {
		        
			redo_fd_request();
			return -EINVAL;
		}


		for (settype = 0; settype < NUM_DISK_MINORS; settype++) {
			int setidx = 0;
			if (minor2disktype[settype].drive_types > DriveType) {
				
				continue;
			}
			setidx = minor2disktype[settype].index;
			dtp = &atari_disk_type[setidx];

			
			if (   dtp->blocks  == setprm.size 
			    && dtp->spt     == setprm.sect
			    && dtp->stretch == setprm.stretch ) {
				if (UD.flags & FTD_MSG)
				    printk (KERN_INFO "floppy%d: setting %s %p!\n",
				        drive, dtp->name, dtp);
				UDT = dtp;
				set_capacity(floppy->disk, UDT->blocks);

				if (cmd == FDDEFPRM) {
				  
				  default_params[drive].name    = dtp->name;
				  default_params[drive].spt     = dtp->spt;
				  default_params[drive].blocks  = dtp->blocks;
				  default_params[drive].fdc_speed = dtp->fdc_speed;
				  default_params[drive].stretch = dtp->stretch;
				}
				
				return 0;
			}

		}

		

	       	if (cmd == FDDEFPRM) {
			
			dtp = &default_params[drive];
		} else
			
			dtp = &user_params[drive];

		dtp->name   = "user format";
		dtp->blocks = setprm.size;
		dtp->spt    = setprm.sect;
		if (setprm.sect > 14) 
			dtp->fdc_speed = 3;
		else
			dtp->fdc_speed = 0;
		dtp->stretch = setprm.stretch;

		if (UD.flags & FTD_MSG)
			printk (KERN_INFO "floppy%d: blk %d spt %d str %d!\n",
				drive, dtp->blocks, dtp->spt, dtp->stretch);

		
		if (setprm.track != dtp->blocks/dtp->spt/2 ||
		    setprm.head != 2) {
			redo_fd_request();
			return -EINVAL;
		}

		UDT = dtp;
		set_capacity(floppy->disk, UDT->blocks);

		return 0;
	case FDMSGON:
		UD.flags |= FTD_MSG;
		return 0;
	case FDMSGOFF:
		UD.flags &= ~FTD_MSG;
		return 0;
	case FDSETEMSGTRESH:
		return -EINVAL;
	case FDFMTBEG:
		return 0;
	case FDFMTTRK:
		if (floppy->ref != 1 && floppy->ref != -1)
			return -EBUSY;
		if (copy_from_user(&fmt_desc, argp, sizeof(fmt_desc)))
			return -EFAULT;
		return do_format(drive, type, &fmt_desc);
	case FDCLRPRM:
		UDT = NULL;
		
		default_params[drive].blocks  = 0;
		set_capacity(floppy->disk, MAX_DISK_SIZE * 2);
	case FDFMTEND:
	case FDFLUSH:
		
		BufferDrive = -1;
		set_bit(drive, &fake_change);
		check_disk_change(bdev);
		return 0;
	default:
		return -EINVAL;
	}
}

static int fd_ioctl(struct block_device *bdev, fmode_t mode,
			     unsigned int cmd, unsigned long arg)
{
	int ret;

	mutex_lock(&ataflop_mutex);
	ret = fd_locked_ioctl(bdev, mode, cmd, arg);
	mutex_unlock(&ataflop_mutex);

	return ret;
}


static void __init fd_probe( int drive )
{
	UD.connected = 0;
	UDT  = NULL;

	if (!fd_test_drive_present( drive ))
		return;

	UD.connected = 1;
	UD.track     = 0;
	switch( UserSteprate[drive] ) {
	case 2:
		UD.steprate = FDCSTEP_2;
		break;
	case 3:
		UD.steprate = FDCSTEP_3;
		break;
	case 6:
		UD.steprate = FDCSTEP_6;
		break;
	case 12:
		UD.steprate = FDCSTEP_12;
		break;
	default: 
		if (ATARIHW_PRESENT( FDCSPEED ) || MACH_IS_MEDUSA)
			UD.steprate = FDCSTEP_3;
		else
			UD.steprate = FDCSTEP_6;
		break;
	}
	MotorOn = 1;	
}



static int __init fd_test_drive_present( int drive )
{
	unsigned long timeout;
	unsigned char status;
	int ok;
	
	if (drive >= (MACH_IS_FALCON ? 1 : 2)) return( 0 );
	fd_select_drive( drive );

	
	atari_turnoff_irq( IRQ_MFP_FDC );
	FDC_WRITE (FDCREG_TRACK, 0xff00);
	FDC_WRITE( FDCREG_CMD, FDCCMD_RESTORE | FDCCMDADD_H | FDCSTEP_6 );

	timeout = jiffies + 2*HZ+HZ/2;
	while (time_before(jiffies, timeout))
		if (!(st_mfp.par_dt_reg & 0x20))
			break;

	status = FDC_READ( FDCREG_STATUS );
	ok = (status & FDCSTAT_TR00) != 0;

	FDC_WRITE( FDCREG_CMD, FDCCMD_FORCI );
	udelay(500);
	status = FDC_READ( FDCREG_STATUS );
	udelay(20);

	if (ok) {
		
		FDC_WRITE( FDCREG_DATA, 0 );
		FDC_WRITE( FDCREG_CMD, FDCCMD_SEEK );
		while( st_mfp.par_dt_reg & 0x20 )
			;
		status = FDC_READ( FDCREG_STATUS );
	}

	atari_turnon_irq( IRQ_MFP_FDC );
	return( ok );
}



static void __init config_types( void )
{
	int drive, cnt = 0;

	
	if (ATARIHW_PRESENT(FDCSPEED))
		dma_wd.fdc_speed = 0;

	printk(KERN_INFO "Probing floppy drive(s):\n");
	for( drive = 0; drive < FD_MAX_UNITS; drive++ ) {
		fd_probe( drive );
		if (UD.connected) {
			printk(KERN_INFO "fd%d\n", drive);
			++cnt;
		}
	}

	if (FDC_READ( FDCREG_STATUS ) & FDCSTAT_BUSY) {
		FDC_WRITE( FDCREG_CMD, FDCCMD_FORCI );
		udelay(500);
		FDC_READ( FDCREG_STATUS );
		udelay(20);
	}
	
	if (cnt > 0) {
		start_motor_off_timer();
		if (cnt == 1) fd_select_drive( 0 );
		start_check_change_timer();
	}
}


static int floppy_open(struct block_device *bdev, fmode_t mode)
{
	struct atari_floppy_struct *p = bdev->bd_disk->private_data;
	int type  = MINOR(bdev->bd_dev) >> 2;

	DPRINT(("fd_open: type=%d\n",type));
	if (p->ref && p->type != type)
		return -EBUSY;

	if (p->ref == -1 || (p->ref && mode & FMODE_EXCL))
		return -EBUSY;

	if (mode & FMODE_EXCL)
		p->ref = -1;
	else
		p->ref++;

	p->type = type;

	if (mode & FMODE_NDELAY)
		return 0;

	if (mode & (FMODE_READ|FMODE_WRITE)) {
		check_disk_change(bdev);
		if (mode & FMODE_WRITE) {
			if (p->wpstat) {
				if (p->ref < 0)
					p->ref = 0;
				else
					p->ref--;
				return -EROFS;
			}
		}
	}
	return 0;
}

static int floppy_unlocked_open(struct block_device *bdev, fmode_t mode)
{
	int ret;

	mutex_lock(&ataflop_mutex);
	ret = floppy_open(bdev, mode);
	mutex_unlock(&ataflop_mutex);

	return ret;
}

static int floppy_release(struct gendisk *disk, fmode_t mode)
{
	struct atari_floppy_struct *p = disk->private_data;
	mutex_lock(&ataflop_mutex);
	if (p->ref < 0)
		p->ref = 0;
	else if (!p->ref--) {
		printk(KERN_ERR "floppy_release with fd_ref == 0");
		p->ref = 0;
	}
	mutex_unlock(&ataflop_mutex);
	return 0;
}

static const struct block_device_operations floppy_fops = {
	.owner		= THIS_MODULE,
	.open		= floppy_unlocked_open,
	.release	= floppy_release,
	.ioctl		= fd_ioctl,
	.check_events	= floppy_check_events,
	.revalidate_disk= floppy_revalidate,
};

static struct kobject *floppy_find(dev_t dev, int *part, void *data)
{
	int drive = *part & 3;
	int type  = *part >> 2;
	if (drive >= FD_MAX_UNITS || type > NUM_DISK_MINORS)
		return NULL;
	*part = 0;
	return get_disk(unit[drive].disk);
}

static int __init atari_floppy_init (void)
{
	int i;

	if (!MACH_IS_ATARI)
		
		return -ENODEV;

	if (register_blkdev(FLOPPY_MAJOR,"fd"))
		return -EBUSY;

	for (i = 0; i < FD_MAX_UNITS; i++) {
		unit[i].disk = alloc_disk(1);
		if (!unit[i].disk)
			goto Enomem;
	}

	if (UseTrackbuffer < 0)
		UseTrackbuffer = !MACH_IS_MEDUSA;

	
	SelectedDrive = -1;
	BufferDrive = -1;

	DMABuffer = atari_stram_alloc(BUFFER_SIZE+512, "ataflop");
	if (!DMABuffer) {
		printk(KERN_ERR "atari_floppy_init: cannot get dma buffer\n");
		goto Enomem;
	}
	TrackBuffer = DMABuffer + 512;
	PhysDMABuffer = virt_to_phys(DMABuffer);
	PhysTrackBuffer = virt_to_phys(TrackBuffer);
	BufferDrive = BufferSide = BufferTrack = -1;

	for (i = 0; i < FD_MAX_UNITS; i++) {
		unit[i].track = -1;
		unit[i].flags = 0;
		unit[i].disk->major = FLOPPY_MAJOR;
		unit[i].disk->first_minor = i;
		sprintf(unit[i].disk->disk_name, "fd%d", i);
		unit[i].disk->fops = &floppy_fops;
		unit[i].disk->private_data = &unit[i];
		unit[i].disk->queue = blk_init_queue(do_fd_request,
					&ataflop_lock);
		if (!unit[i].disk->queue)
			goto Enomem;
		set_capacity(unit[i].disk, MAX_DISK_SIZE * 2);
		add_disk(unit[i].disk);
	}

	blk_register_region(MKDEV(FLOPPY_MAJOR, 0), 256, THIS_MODULE,
				floppy_find, NULL, NULL);

	printk(KERN_INFO "Atari floppy driver: max. %cD, %strack buffering\n",
	       DriveType == 0 ? 'D' : DriveType == 1 ? 'H' : 'E',
	       UseTrackbuffer ? "" : "no ");
	config_types();

	return 0;
Enomem:
	while (i--) {
		struct request_queue *q = unit[i].disk->queue;

		put_disk(unit[i].disk);
		if (q)
			blk_cleanup_queue(q);
	}

	unregister_blkdev(FLOPPY_MAJOR, "fd");
	return -ENOMEM;
}

#ifndef MODULE
static int __init atari_floppy_setup(char *str)
{
	int ints[3 + FD_MAX_UNITS];
	int i;

	if (!MACH_IS_ATARI)
		return 0;

	str = get_options(str, 3 + FD_MAX_UNITS, ints);
	
	if (ints[0] < 1) {
		printk(KERN_ERR "ataflop_setup: no arguments!\n" );
		return 0;
	}
	else if (ints[0] > 2+FD_MAX_UNITS) {
		printk(KERN_ERR "ataflop_setup: too many arguments\n" );
	}

	if (ints[1] < 0 || ints[1] > 2)
		printk(KERN_ERR "ataflop_setup: bad drive type\n" );
	else
		DriveType = ints[1];

	if (ints[0] >= 2)
		UseTrackbuffer = (ints[2] > 0);

	for( i = 3; i <= ints[0] && i-3 < FD_MAX_UNITS; ++i ) {
		if (ints[i] != 2 && ints[i] != 3 && ints[i] != 6 && ints[i] != 12)
			printk(KERN_ERR "ataflop_setup: bad steprate\n" );
		else
			UserSteprate[i-3] = ints[i];
	}
	return 1;
}

__setup("floppy=", atari_floppy_setup);
#endif

static void __exit atari_floppy_exit(void)
{
	int i;
	blk_unregister_region(MKDEV(FLOPPY_MAJOR, 0), 256);
	for (i = 0; i < FD_MAX_UNITS; i++) {
		struct request_queue *q = unit[i].disk->queue;

		del_gendisk(unit[i].disk);
		put_disk(unit[i].disk);
		blk_cleanup_queue(q);
	}
	unregister_blkdev(FLOPPY_MAJOR, "fd");

	del_timer_sync(&fd_timer);
	atari_stram_free( DMABuffer );
}

module_init(atari_floppy_init)
module_exit(atari_floppy_exit)

MODULE_LICENSE("GPL");
