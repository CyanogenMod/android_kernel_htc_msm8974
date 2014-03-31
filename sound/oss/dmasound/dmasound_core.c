/*
 *  linux/sound/oss/dmasound/dmasound_core.c
 *
 *
 *  OSS/Free compatible Atari TT/Falcon and Amiga DMA sound driver for
 *  Linux/m68k
 *  Extended to support Power Macintosh for Linux/ppc by Paul Mackerras
 *
 *  (c) 1995 by Michael Schlueter & Michael Marte
 *
 *  Michael Schlueter (michael@duck.syd.de) did the basic structure of the VFS
 *  interface and the u-law to signed byte conversion.
 *
 *  Michael Marte (marte@informatik.uni-muenchen.de) did the sound queue,
 *  /dev/mixer, /dev/sndstat and complemented the VFS interface. He would like
 *  to thank:
 *    - Michael Schlueter for initial ideas and documentation on the MFP and
 *	the DMA sound hardware.
 *    - Therapy? for their CD 'Troublegum' which really made me rock.
 *
 *  /dev/sndstat is based on code by Hannu Savolainen, the author of the
 *  VoxWare family of drivers.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 *
 *  History:
 *
 *	1995/8/25	First release
 *
 *	1995/9/02	Roman Hodek:
 *			  - Fixed atari_stram_alloc() call, the timer
 *			    programming and several race conditions
 *	1995/9/14	Roman Hodek:
 *			  - After some discussion with Michael Schlueter,
 *			    revised the interrupt disabling
 *			  - Slightly speeded up U8->S8 translation by using
 *			    long operations where possible
 *			  - Added 4:3 interpolation for /dev/audio
 *
 *	1995/9/20	Torsten Scherer:
 *			  - Fixed a bug in sq_write and changed /dev/audio
 *			    converting to play at 12517Hz instead of 6258Hz.
 *
 *	1995/9/23	Torsten Scherer:
 *			  - Changed sq_interrupt() and sq_play() to pre-program
 *			    the DMA for another frame while there's still one
 *			    running. This allows the IRQ response to be
 *			    arbitrarily delayed and playing will still continue.
 *
 *	1995/10/14	Guenther Kelleter, Torsten Scherer:
 *			  - Better support for Falcon audio (the Falcon doesn't
 *			    raise an IRQ at the end of a frame, but at the
 *			    beginning instead!). uses 'if (codec_dma)' in lots
 *			    of places to simply switch between Falcon and TT
 *			    code.
 *
 *	1995/11/06	Torsten Scherer:
 *			  - Started introducing a hardware abstraction scheme
 *			    (may perhaps also serve for Amigas?)
 *			  - Can now play samples at almost all frequencies by
 *			    means of a more generalized expand routine
 *			  - Takes a good deal of care to cut data only at
 *			    sample sizes
 *			  - Buffer size is now a kernel runtime option
 *			  - Implemented fsync() & several minor improvements
 *			Guenther Kelleter:
 *			  - Useful hints and bug fixes
 *			  - Cross-checked it for Falcons
 *
 *	1996/3/9	Geert Uytterhoeven:
 *			  - Support added for Amiga, A-law, 16-bit little
 *			    endian.
 *			  - Unification to drivers/sound/dmasound.c.
 *
 *	1996/4/6	Martin Mitchell:
 *			  - Updated to 1.3 kernel.
 *
 *	1996/6/13       Topi Kanerva:
 *			  - Fixed things that were broken (mainly the amiga
 *			    14-bit routines)
 *			  - /dev/sndstat shows now the real hardware frequency
 *			  - The lowpass filter is disabled by default now
 *
 *	1996/9/25	Geert Uytterhoeven:
 *			  - Modularization
 *
 *	1998/6/10	Andreas Schwab:
 *			  - Converted to use sound_core
 *
 *	1999/12/28	Richard Zidlicky:
 *			  - Added support for Q40
 *
 *	2000/2/27	Geert Uytterhoeven:
 *			  - Clean up and split the code into 4 parts:
 *			      o dmasound_core: machine-independent code
 *			      o dmasound_atari: Atari TT and Falcon support
 *			      o dmasound_awacs: Apple PowerMac support
 *			      o dmasound_paula: Amiga support
 *
 *	2000/3/25	Geert Uytterhoeven:
 *			  - Integration of dmasound_q40
 *			  - Small clean ups
 *
 *	2001/01/26 [1.0] Iain Sandoe
 *			  - make /dev/sndstat show revision & edition info.
 *			  - since dmasound.mach.sq_setup() can fail on pmac
 *			    its type has been changed to int and the returns
 *			    are checked.
 *		   [1.1]  - stop missing translations from being called.
 *	2001/02/08 [1.2]  - remove unused translation tables & move machine-
 *			    specific tables to low-level.
 *			  - return correct info. for SNDCTL_DSP_GETFMTS.
 *		   [1.3]  - implement SNDCTL_DSP_GETCAPS fully.
 *		   [1.4]  - make /dev/sndstat text length usage deterministic.
 *			  - make /dev/sndstat call to low-level
 *			    dmasound.mach.state_info() pass max space to ll driver.
 *			  - tidy startup banners and output info.
 *		   [1.5]  - tidy up a little (removed some unused #defines in
 *			    dmasound.h)
 *			  - fix up HAS_RECORD conditionalisation.
 *			  - add record code in places it is missing...
 *			  - change buf-sizes to bytes to allow < 1kb for pmac
 *			    if user param entry is < 256 the value is taken to
 *			    be in kb > 256 is taken to be in bytes.
 *			  - make default buff/frag params conditional on
 *			    machine to allow smaller values for pmac.
 *			  - made the ioctls, read & write comply with the OSS
 *			    rules on setting params.
 *			  - added parsing of _setup() params for record.
 *	2001/04/04 [1.6]  - fix bug where sample rates higher than maximum were
 *			    being reported as OK.
 *			  - fix open() to return -EBUSY as per OSS doc. when
 *			    audio is in use - this is independent of O_NOBLOCK.
 *			  - fix bug where SNDCTL_DSP_POST was blocking.
 */



#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sound.h>
#include <linux/init.h>
#include <linux/soundcard.h>
#include <linux/poll.h>
#include <linux/mutex.h>

#include <asm/uaccess.h>

#include "dmasound.h"

#define DMASOUND_CORE_REVISION 1
#define DMASOUND_CORE_EDITION 6


static DEFINE_MUTEX(dmasound_core_mutex);
int dmasound_catchRadius = 0;
module_param(dmasound_catchRadius, int, 0);

static unsigned int numWriteBufs = DEFAULT_N_BUFFERS;
module_param(numWriteBufs, int, 0);
static unsigned int writeBufSize = DEFAULT_BUFF_SIZE ;	
module_param(writeBufSize, int, 0);

MODULE_LICENSE("GPL");

#ifdef MODULE
static int sq_unit = -1;
static int mixer_unit = -1;
static int state_unit = -1;
static int irq_installed;
#endif 

static fmode_t shared_resource_owner;
static int shared_resources_initialised;


struct sound_settings dmasound = {
	.lock = __SPIN_LOCK_UNLOCKED(dmasound.lock)
};

static inline void sound_silence(void)
{
	dmasound.mach.silence(); 
}

static inline int sound_set_format(int format)
{
	return dmasound.mach.setFormat(format);
}


static int sound_set_speed(int speed)
{
	if (speed < 0)
		return dmasound.soft.speed;

	if (dmasound.mach.max_dsp_speed &&
	   (speed > dmasound.mach.max_dsp_speed))
		speed = dmasound.mach.max_dsp_speed ;

	dmasound.soft.speed = speed;

	if (dmasound.minDev == SND_DEV_DSP)
		dmasound.dsp.speed = dmasound.soft.speed;

	return dmasound.soft.speed;
}

static int sound_set_stereo(int stereo)
{
	if (stereo < 0)
		return dmasound.soft.stereo;

	stereo = !!stereo;    

	dmasound.soft.stereo = stereo;
	if (dmasound.minDev == SND_DEV_DSP)
		dmasound.dsp.stereo = stereo;

	return stereo;
}

static ssize_t sound_copy_translate(TRANS *trans, const u_char __user *userPtr,
				    size_t userCount, u_char frame[],
				    ssize_t *frameUsed, ssize_t frameLeft)
{
	ssize_t (*ct_func)(const u_char __user *, size_t, u_char *, ssize_t *, ssize_t);

	switch (dmasound.soft.format) {
	    case AFMT_MU_LAW:
		ct_func = trans->ct_ulaw;
		break;
	    case AFMT_A_LAW:
		ct_func = trans->ct_alaw;
		break;
	    case AFMT_S8:
		ct_func = trans->ct_s8;
		break;
	    case AFMT_U8:
		ct_func = trans->ct_u8;
		break;
	    case AFMT_S16_BE:
		ct_func = trans->ct_s16be;
		break;
	    case AFMT_U16_BE:
		ct_func = trans->ct_u16be;
		break;
	    case AFMT_S16_LE:
		ct_func = trans->ct_s16le;
		break;
	    case AFMT_U16_LE:
		ct_func = trans->ct_u16le;
		break;
	    default:
		return 0;
	}
	if (ct_func)
		return ct_func(userPtr, userCount, frame, frameUsed, frameLeft);
	return 0;
}


static struct {
    int busy;
    int modify_counter;
} mixer;

static int mixer_open(struct inode *inode, struct file *file)
{
	mutex_lock(&dmasound_core_mutex);
	if (!try_module_get(dmasound.mach.owner)) {
		mutex_unlock(&dmasound_core_mutex);
		return -ENODEV;
	}
	mixer.busy = 1;
	mutex_unlock(&dmasound_core_mutex);
	return 0;
}

static int mixer_release(struct inode *inode, struct file *file)
{
	mutex_lock(&dmasound_core_mutex);
	mixer.busy = 0;
	module_put(dmasound.mach.owner);
	mutex_unlock(&dmasound_core_mutex);
	return 0;
}

static int mixer_ioctl(struct file *file, u_int cmd, u_long arg)
{
	if (_SIOC_DIR(cmd) & _SIOC_WRITE)
	    mixer.modify_counter++;
	switch (cmd) {
	    case OSS_GETVERSION:
		return IOCTL_OUT(arg, SOUND_VERSION);
	    case SOUND_MIXER_INFO:
		{
		    mixer_info info;
		    memset(&info, 0, sizeof(info));
		    strlcpy(info.id, dmasound.mach.name2, sizeof(info.id));
		    strlcpy(info.name, dmasound.mach.name2, sizeof(info.name));
		    info.modify_counter = mixer.modify_counter;
		    if (copy_to_user((void __user *)arg, &info, sizeof(info)))
			    return -EFAULT;
		    return 0;
		}
	}
	if (dmasound.mach.mixer_ioctl)
	    return dmasound.mach.mixer_ioctl(cmd, arg);
	return -EINVAL;
}

static long mixer_unlocked_ioctl(struct file *file, u_int cmd, u_long arg)
{
	int ret;

	mutex_lock(&dmasound_core_mutex);
	ret = mixer_ioctl(file, cmd, arg);
	mutex_unlock(&dmasound_core_mutex);

	return ret;
}

static const struct file_operations mixer_fops =
{
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.unlocked_ioctl	= mixer_unlocked_ioctl,
	.open		= mixer_open,
	.release	= mixer_release,
};

static void mixer_init(void)
{
#ifndef MODULE
	int mixer_unit;
#endif
	mixer_unit = register_sound_mixer(&mixer_fops, -1);
	if (mixer_unit < 0)
		return;

	mixer.busy = 0;
	dmasound.treble = 0;
	dmasound.bass = 0;
	if (dmasound.mach.mixer_init)
	    dmasound.mach.mixer_init();
}



struct sound_queue dmasound_write_sq;
static void sq_reset_output(void) ;

static int sq_allocate_buffers(struct sound_queue *sq, int num, int size)
{
	int i;

	if (sq->buffers)
		return 0;
	sq->numBufs = num;
	sq->bufSize = size;
	sq->buffers = kmalloc (num * sizeof(char *), GFP_KERNEL);
	if (!sq->buffers)
		return -ENOMEM;
	for (i = 0; i < num; i++) {
		sq->buffers[i] = dmasound.mach.dma_alloc(size, GFP_KERNEL);
		if (!sq->buffers[i]) {
			while (i--)
				dmasound.mach.dma_free(sq->buffers[i], size);
			kfree(sq->buffers);
			sq->buffers = NULL;
			return -ENOMEM;
		}
	}
	return 0;
}

static void sq_release_buffers(struct sound_queue *sq)
{
	int i;

	if (sq->buffers) {
		for (i = 0; i < sq->numBufs; i++)
			dmasound.mach.dma_free(sq->buffers[i], sq->bufSize);
		kfree(sq->buffers);
		sq->buffers = NULL;
	}
}


static int sq_setup(struct sound_queue *sq)
{
	int (*setup_func)(void) = NULL;
	int hard_frame ;

	if (sq->locked) { 
#ifdef DEBUG_DMASOUND
printk("dmasound_core: tried to sq_setup a locked queue\n") ;
#endif
		return -EINVAL ;
	}
	sq->locked = 1 ; 


	dmasound.mach.init();


	if (sq->user_frags <= 0) {
		sq->max_count = sq->numBufs ;
		sq->max_active = sq->numBufs ;
		sq->block_size = sq->bufSize;
		
		sq->user_frags = sq->numBufs ;
		sq->user_frag_size = sq->bufSize ;
		sq->user_frag_size *=
			(dmasound.soft.size * (dmasound.soft.stereo+1) ) ;
		sq->user_frag_size /=
			(dmasound.hard.size * (dmasound.hard.stereo+1) ) ;
	} else {
		
		sq->block_size = sq->user_frag_size ;
		sq->block_size *=
			(dmasound.hard.size * (dmasound.hard.stereo+1) ) ;
		sq->block_size /=
			(dmasound.soft.size * (dmasound.soft.stereo+1) ) ;
		
		sq->block_size *= dmasound.hard.speed ;
		sq->block_size /= dmasound.soft.speed ;
		
		hard_frame =
			(dmasound.hard.size * (dmasound.hard.stereo+1))/8 ;
		sq->block_size +=  (hard_frame - 1) ;
		sq->block_size &= ~(hard_frame - 1) ; 
		
		if ( sq->block_size <= 0 || sq->block_size > sq->bufSize) {
#ifdef DEBUG_DMASOUND
printk("dmasound_core: invalid frag size (user set %d)\n", sq->user_frag_size) ;
#endif
			sq->block_size = sq->bufSize ;
		}
		if ( sq->user_frags <= sq->numBufs ) {
			sq->max_count = sq->user_frags ;
			
			sq->max_active = (sq->max_active <= sq->max_count) ?
				sq->max_active : sq->max_count ;
		} else {
#ifdef DEBUG_DMASOUND
printk("dmasound_core: invalid frag count (user set %d)\n", sq->user_frags) ;
#endif
			sq->max_count =
			sq->max_active = sq->numBufs ;
		}
	}
	sq->front = sq->count = sq->rear_size = 0;
	sq->syncing = 0;
	sq->active = 0;

	if (sq == &write_sq) {
	    sq->rear = -1;
	    setup_func = dmasound.mach.write_sq_setup;
	}
	if (setup_func)
	    return setup_func();
	return 0 ;
}

static inline void sq_play(void)
{
	dmasound.mach.play();
}

static ssize_t sq_write(struct file *file, const char __user *src, size_t uLeft,
			loff_t *ppos)
{
	ssize_t uWritten = 0;
	u_char *dest;
	ssize_t uUsed = 0, bUsed, bLeft;
	unsigned long flags ;

	if (uLeft == 0)
		return 0;


	if (shared_resources_initialised == 0) {
		dmasound.mach.init() ;
		shared_resources_initialised = 1 ;
	}


	if (write_sq.locked == 0) {
		if ((uWritten = sq_setup(&write_sq)) < 0) return uWritten ;
		uWritten = 0 ;
	}



	spin_lock_irqsave(&dmasound.lock, flags);
	write_sq.syncing &= ~2 ; 
	spin_unlock_irqrestore(&dmasound.lock, flags);

	if (write_sq.count > 0 &&
	    (bLeft = write_sq.block_size-write_sq.rear_size) > 0) {
		dest = write_sq.buffers[write_sq.rear];
		bUsed = write_sq.rear_size;
		uUsed = sound_copy_translate(dmasound.trans_write, src, uLeft,
					     dest, &bUsed, bLeft);
		if (uUsed <= 0)
			return uUsed;
		src += uUsed;
		uWritten += uUsed;
		uLeft = (uUsed <= uLeft) ? (uLeft - uUsed) : 0 ; 
		write_sq.rear_size = bUsed;
	}

	while (uLeft) {
		while (write_sq.count >= write_sq.max_active) {
			sq_play();
			if (write_sq.non_blocking)
				return uWritten > 0 ? uWritten : -EAGAIN;
			SLEEP(write_sq.action_queue);
			if (signal_pending(current))
				return uWritten > 0 ? uWritten : -EINTR;
		}


		dest = write_sq.buffers[(write_sq.rear+1) % write_sq.max_count];
		bUsed = 0;
		bLeft = write_sq.block_size;
		uUsed = sound_copy_translate(dmasound.trans_write, src, uLeft,
					     dest, &bUsed, bLeft);
		if (uUsed <= 0)
			break;
		src += uUsed;
		uWritten += uUsed;
		uLeft = (uUsed <= uLeft) ? (uLeft - uUsed) : 0 ; 
		if (bUsed) {
			write_sq.rear = (write_sq.rear+1) % write_sq.max_count;
			write_sq.rear_size = bUsed;
			write_sq.count++;
		}
	} 

	sq_play();

	return uUsed < 0? uUsed: uWritten;
}

static unsigned int sq_poll(struct file *file, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	int retVal;
	
	if (write_sq.locked == 0) {
		if ((retVal = sq_setup(&write_sq)) < 0)
			return retVal;
		return 0;
	}
	if (file->f_mode & FMODE_WRITE )
		poll_wait(file, &write_sq.action_queue, wait);
	if (file->f_mode & FMODE_WRITE)
		if (write_sq.count < write_sq.max_active || write_sq.block_size - write_sq.rear_size > 0)
			mask |= POLLOUT | POLLWRNORM;
	return mask;

}

static inline void sq_init_waitqueue(struct sound_queue *sq)
{
	init_waitqueue_head(&sq->action_queue);
	init_waitqueue_head(&sq->open_queue);
	init_waitqueue_head(&sq->sync_queue);
	sq->busy = 0;
}

#if 0 
static inline void sq_wake_up(struct sound_queue *sq, struct file *file,
			      fmode_t mode)
{
	if (file->f_mode & mode) {
		sq->busy = 0; 
		WAKE_UP(sq->open_queue);
	}
}
#endif

static int sq_open2(struct sound_queue *sq, struct file *file, fmode_t mode,
		    int numbufs, int bufsize)
{
	int rc = 0;

	if (file->f_mode & mode) {
		if (sq->busy) {
#if 0 
			rc = -EBUSY;
			if (file->f_flags & O_NONBLOCK)
				return rc;
			rc = -EINTR;
			while (sq->busy) {
				SLEEP(sq->open_queue);
				if (signal_pending(current))
					return rc;
			}
			rc = 0;
#else
			return -EBUSY ;
#endif
		}
		sq->busy = 1; 

		if (( rc = sq_allocate_buffers(sq, numbufs, bufsize))) {
#if 0 
			sq_wake_up(sq, file, mode);
#else
			sq->busy = 0 ;
#endif
			return rc;
		}

		sq->non_blocking = file->f_flags & O_NONBLOCK;
	}
	return rc;
}

#define write_sq_init_waitqueue()	sq_init_waitqueue(&write_sq)
#if 0 
#define write_sq_wake_up(file)		sq_wake_up(&write_sq, file, FMODE_WRITE)
#endif
#define write_sq_release_buffers()	sq_release_buffers(&write_sq)
#define write_sq_open(file)	\
	sq_open2(&write_sq, file, FMODE_WRITE, numWriteBufs, writeBufSize )

static int sq_open(struct inode *inode, struct file *file)
{
	int rc;

	mutex_lock(&dmasound_core_mutex);
	if (!try_module_get(dmasound.mach.owner)) {
		mutex_unlock(&dmasound_core_mutex);
		return -ENODEV;
	}

	rc = write_sq_open(file); 
	if (rc)
		goto out;
	if (file->f_mode & FMODE_READ) {
		
		rc = -ENXIO ; 
		goto out;
	}

	if (dmasound.mach.sq_open)
	    dmasound.mach.sq_open(file->f_mode);


	dmasound.minDev = iminor(inode) & 0x0f;


	if (shared_resource_owner == 0) {
		dmasound.soft = dmasound.mach.default_soft ;
		dmasound.dsp = dmasound.mach.default_soft ;
		dmasound.hard = dmasound.mach.default_hard ;
	}

#ifndef DMASOUND_STRICT_OSS_COMPLIANCE
	if (dmasound.minDev == SND_DEV_AUDIO) {
		sound_set_speed(8000);
		sound_set_stereo(0);
		sound_set_format(AFMT_MU_LAW);
	}
#endif
	mutex_unlock(&dmasound_core_mutex);
	return 0;
 out:
	module_put(dmasound.mach.owner);
	mutex_unlock(&dmasound_core_mutex);
	return rc;
}

static void sq_reset_output(void)
{
	sound_silence(); 
	write_sq.active = 0;
	write_sq.count = 0;
	write_sq.rear_size = 0;
	
	write_sq.front = 0 ;
	write_sq.rear = -1 ; 

	
	write_sq.locked = 0 ;
	write_sq.user_frags = 0 ;
	write_sq.user_frag_size = 0 ;
}

static void sq_reset(void)
{
	sq_reset_output() ;

	
	shared_resources_initialised = 0 ;
}

static int sq_fsync(struct file *filp, struct dentry *dentry)
{
	int rc = 0;
	int timeout = 5;

	write_sq.syncing |= 1;
	sq_play();	

	while (write_sq.active) {
		SLEEP(write_sq.sync_queue);
		if (signal_pending(current)) {
			sq_reset_output();
			rc = -EINTR;
			break;
		}
		if (!--timeout) {
			printk(KERN_WARNING "dmasound: Timeout draining output\n");
			sq_reset_output();
			rc = -EIO;
			break;
		}
	}

	
	write_sq.syncing = 0 ;
	return rc;
}

static int sq_release(struct inode *inode, struct file *file)
{
	int rc = 0;

	mutex_lock(&dmasound_core_mutex);

	if (file->f_mode & FMODE_WRITE) {
		if (write_sq.busy)
			rc = sq_fsync(file, file->f_path.dentry);

		sq_reset_output() ; 
		write_sq_release_buffers();
		write_sq.busy = 0;
	}

	if (file->f_mode & shared_resource_owner) { 
		shared_resource_owner = 0 ;
		shared_resources_initialised = 0 ;
		dmasound.hard = dmasound.mach.default_hard ;
	}

	module_put(dmasound.mach.owner);

#if 0 

	
	
	
	read_sq_wake_up(file); 
	write_sq_wake_up(file); 
#endif 

	mutex_unlock(&dmasound_core_mutex);

	return rc;
}


static int shared_resources_are_mine(fmode_t md)
{
	if (shared_resource_owner)
		return (shared_resource_owner & md) != 0;
	else {
		shared_resource_owner = md ;
		return 1 ;
	}
}


static int queues_are_quiescent(void)
{
	if (write_sq.locked)
		return 0 ;
	return 1 ;
}



static int set_queue_frags(struct sound_queue *sq, int bufs, int size)
{
	if (sq->locked) {
#ifdef DEBUG_DMASOUND
printk("dmasound_core: tried to set_queue_frags on a locked queue\n") ;
#endif
		return -EINVAL ;
	}

	if ((size < MIN_FRAG_SIZE) || (size > MAX_FRAG_SIZE))
		return -EINVAL ;
	size = (1<<size) ; 
	if (size > sq->bufSize)
		return -EINVAL ; 

	if (bufs <= 0)
		return -EINVAL ;
	if (bufs > sq->numBufs) 
		bufs = sq->numBufs ;

	sq->user_frags =
	sq->max_active = bufs ;
	sq->user_frag_size = size ;

	return 0 ;
}

static int sq_ioctl(struct file *file, u_int cmd, u_long arg)
{
	int val, result;
	u_long fmt;
	int data;
	int size, nbufs;
	audio_buf_info info;

	switch (cmd) {
	case SNDCTL_DSP_RESET:
		sq_reset();
		return 0;
		break ;
	case SNDCTL_DSP_GETFMTS:
		fmt = dmasound.mach.hardware_afmts ; 
		return IOCTL_OUT(arg, fmt);
		break ;
	case SNDCTL_DSP_GETBLKSIZE:
		size = 0 ;
		if (file->f_mode & FMODE_WRITE) {
			if ( !write_sq.locked )
				sq_setup(&write_sq) ;
			size = write_sq.user_frag_size ;
		}
		return IOCTL_OUT(arg, size);
		break ;
	case SNDCTL_DSP_POST:
		write_sq.syncing |= 0x2 ;
		sq_play() ;
		return 0 ;
	case SNDCTL_DSP_SYNC:
		result = 0 ;
		if (file->f_mode & FMODE_WRITE) {
			result = sq_fsync(file, file->f_path.dentry);
			sq_reset_output() ;
		}
		
		if (file->f_mode & shared_resource_owner)
			shared_resources_initialised = 0 ;
		return result ;
		break ;
	case SOUND_PCM_READ_RATE:
		return IOCTL_OUT(arg, dmasound.soft.speed);
	case SNDCTL_DSP_SPEED:
		if (shared_resources_are_mine(file->f_mode)) {
			IOCTL_IN(arg, data);
			data = sound_set_speed(data) ;
			shared_resources_initialised = 0 ;
			return IOCTL_OUT(arg, data);
		} else
			return -EINVAL ;
		break ;
	case SNDCTL_DSP_STEREO:
		if (shared_resources_are_mine(file->f_mode) &&
		    queues_are_quiescent()) {
			IOCTL_IN(arg, data);
			shared_resources_initialised = 0 ;
			return IOCTL_OUT(arg, sound_set_stereo(data));
		} else
			return -EINVAL ;
		break ;
	case SOUND_PCM_WRITE_CHANNELS:
		if (shared_resources_are_mine(file->f_mode) &&
		    queues_are_quiescent()) {
			IOCTL_IN(arg, data);
			
			shared_resources_initialised = 0 ;
			return IOCTL_OUT(arg, sound_set_stereo(data-1)+1);
		} else
			return -EINVAL ;
		break ;
	case SNDCTL_DSP_SETFMT:
		if (shared_resources_are_mine(file->f_mode) &&
		    queues_are_quiescent()) {
		    	int format;
			IOCTL_IN(arg, data);
			shared_resources_initialised = 0 ;
			format = sound_set_format(data);
			result = IOCTL_OUT(arg, format);
			if (result < 0)
				return result;
			if (format != data && data != AFMT_QUERY)
				return -EINVAL;
			return 0;
		} else
			return -EINVAL ;
	case SNDCTL_DSP_SUBDIVIDE:
		return -EINVAL ;
	case SNDCTL_DSP_SETFRAGMENT:
		IOCTL_IN(arg, data);
		result = 0 ;
		nbufs = (data >> 16) & 0x7fff ; 
		size = data & 0xffff;
		if (file->f_mode & FMODE_WRITE) {
			result = set_queue_frags(&write_sq, nbufs, size) ;
			if (result)
				return result ;
		}
		return IOCTL_OUT(arg, data);
		break ;
	case SNDCTL_DSP_GETOSPACE:
		if (file->f_mode & FMODE_WRITE) {
			if ( !write_sq.locked )
				sq_setup(&write_sq) ;
			info.fragments = write_sq.max_active - write_sq.count;
			info.fragstotal = write_sq.max_active;
			info.fragsize = write_sq.user_frag_size;
			info.bytes = info.fragments * info.fragsize;
			if (copy_to_user((void __user *)arg, &info, sizeof(info)))
				return -EFAULT;
			return 0;
		} else
			return -EINVAL ;
		break ;
	case SNDCTL_DSP_GETCAPS:
		val = dmasound.mach.capabilities & 0xffffff00;
		return IOCTL_OUT(arg,val);

	default:
		return mixer_ioctl(file, cmd, arg);
	}
	return -EINVAL;
}

static long sq_unlocked_ioctl(struct file *file, u_int cmd, u_long arg)
{
	int ret;

	mutex_lock(&dmasound_core_mutex);
	ret = sq_ioctl(file, cmd, arg);
	mutex_unlock(&dmasound_core_mutex);

	return ret;
}

static const struct file_operations sq_fops =
{
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= sq_write,
	.poll		= sq_poll,
	.unlocked_ioctl	= sq_unlocked_ioctl,
	.open		= sq_open,
	.release	= sq_release,
};

static int sq_init(void)
{
	const struct file_operations *fops = &sq_fops;
#ifndef MODULE
	int sq_unit;
#endif

	sq_unit = register_sound_dsp(fops, -1);
	if (sq_unit < 0) {
		printk(KERN_ERR "dmasound_core: couldn't register fops\n") ;
		return sq_unit ;
	}

	write_sq_init_waitqueue();


	if (shared_resource_owner == 0) {
		dmasound.soft = dmasound.mach.default_soft ;
		dmasound.hard = dmasound.mach.default_hard ;
		dmasound.dsp = dmasound.mach.default_soft ;
		shared_resources_initialised = 0 ;
	}
	return 0 ;
}




#define STAT_BUFF_LEN 768


#define LOW_LEVEL_STAT_ALLOC 162

static struct {
    int busy;
    char buf[STAT_BUFF_LEN];	
    int len, ptr;
} state;


static char *get_afmt_string(int afmt)
{
        switch(afmt) {
            case AFMT_MU_LAW:
                return "mu-law";
                break;
            case AFMT_A_LAW:
                return "A-law";
                break;
            case AFMT_U8:
                return "unsigned 8 bit";
                break;
            case AFMT_S8:
                return "signed 8 bit";
                break;
            case AFMT_S16_BE:
                return "signed 16 bit BE";
                break;
            case AFMT_U16_BE:
                return "unsigned 16 bit BE";
                break;
            case AFMT_S16_LE:
                return "signed 16 bit LE";
                break;
            case AFMT_U16_LE:
                return "unsigned 16 bit LE";
                break;
	    case 0:
		return "format not set" ;
		break ;
            default:
                break ;
        }
        return "ERROR: Unsupported AFMT_XXXX code" ;
}

static int state_open(struct inode *inode, struct file *file)
{
	char *buffer = state.buf;
	int len = 0;
	int ret;

	mutex_lock(&dmasound_core_mutex);
	ret = -EBUSY;
	if (state.busy)
		goto out;

	ret = -ENODEV;
	if (!try_module_get(dmasound.mach.owner))
		goto out;

	state.ptr = 0;
	state.busy = 1;

	len += sprintf(buffer+len, "%sDMA sound driver rev %03d :\n",
		dmasound.mach.name, (DMASOUND_CORE_REVISION<<4) +
		((dmasound.mach.version>>8) & 0x0f));
	len += sprintf(buffer+len,
		"Core driver edition %02d.%02d : %s driver edition %02d.%02d\n",
		DMASOUND_CORE_REVISION, DMASOUND_CORE_EDITION, dmasound.mach.name2,
		(dmasound.mach.version >> 8), (dmasound.mach.version & 0xff)) ;


	if (dmasound.mach.state_info)
		len += dmasound.mach.state_info(buffer+len,
			(size_t) LOW_LEVEL_STAT_ALLOC) ;


	

	len += sprintf(buffer+len,"\t\t === Formats & settings ===\n") ;
	len += sprintf(buffer+len,"Parameter %20s%20s\n","soft","hard") ;
	len += sprintf(buffer+len,"Format   :%20s%20s\n",
		get_afmt_string(dmasound.soft.format),
		get_afmt_string(dmasound.hard.format));

	len += sprintf(buffer+len,"Samp Rate:%14d s/sec%14d s/sec\n",
		       dmasound.soft.speed, dmasound.hard.speed);

	len += sprintf(buffer+len,"Channels :%20s%20s\n",
		       dmasound.soft.stereo ? "stereo" : "mono",
		       dmasound.hard.stereo ? "stereo" : "mono" );

	

	len += sprintf(buffer+len,"\t\t === Sound Queue status ===\n");
	len += sprintf(buffer+len,"Allocated:%8s%6s\n","Buffers","Size") ;
	len += sprintf(buffer+len,"%9s:%8d%6d\n",
		"write", write_sq.numBufs, write_sq.bufSize) ;
	len += sprintf(buffer+len,
		"Current  : MaxFrg FragSiz MaxAct Frnt Rear "
		"Cnt RrSize A B S L  xruns\n") ;
	len += sprintf(buffer+len,"%9s:%7d%8d%7d%5d%5d%4d%7d%2d%2d%2d%2d%7d\n",
		"write", write_sq.max_count, write_sq.block_size,
		write_sq.max_active, write_sq.front, write_sq.rear,
		write_sq.count, write_sq.rear_size, write_sq.active,
		write_sq.busy, write_sq.syncing, write_sq.locked, write_sq.xruns) ;
#ifdef DEBUG_DMASOUND
printk("dmasound: stat buffer used %d bytes\n", len) ;
#endif

	if (len >= STAT_BUFF_LEN)
		printk(KERN_ERR "dmasound_core: stat buffer overflowed!\n");

	state.len = len;
	ret = 0;
out:
	mutex_unlock(&dmasound_core_mutex);
	return ret;
}

static int state_release(struct inode *inode, struct file *file)
{
	mutex_lock(&dmasound_core_mutex);
	state.busy = 0;
	module_put(dmasound.mach.owner);
	mutex_unlock(&dmasound_core_mutex);
	return 0;
}

static ssize_t state_read(struct file *file, char __user *buf, size_t count,
			  loff_t *ppos)
{
	int n = state.len - state.ptr;
	if (n > count)
		n = count;
	if (n <= 0)
		return 0;
	if (copy_to_user(buf, &state.buf[state.ptr], n))
		return -EFAULT;
	state.ptr += n;
	return n;
}

static const struct file_operations state_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= state_read,
	.open		= state_open,
	.release	= state_release,
};

static int state_init(void)
{
#ifndef MODULE
	int state_unit;
#endif
	state_unit = register_sound_special(&state_fops, SND_DEV_STATUS);
	if (state_unit < 0)
		return state_unit ;
	state.busy = 0;
	return 0 ;
}



int dmasound_init(void)
{
	int res ;
#ifdef MODULE
	if (irq_installed)
		return -EBUSY;
#endif

	

	
	if ((res = sq_init()) < 0)
		return res ;

	
	if ((res = state_init()) < 0)
		return res ;

	
	mixer_init();

	if (!dmasound.mach.irqinit()) {
		printk(KERN_ERR "DMA sound driver: Interrupt initialization failed\n");
		return -ENODEV;
	}
#ifdef MODULE
	irq_installed = 1;
#endif

	printk(KERN_INFO "%s DMA sound driver rev %03d installed\n",
		dmasound.mach.name, (DMASOUND_CORE_REVISION<<4) +
		((dmasound.mach.version>>8) & 0x0f));
	printk(KERN_INFO
		"Core driver edition %02d.%02d : %s driver edition %02d.%02d\n",
		DMASOUND_CORE_REVISION, DMASOUND_CORE_EDITION, dmasound.mach.name2,
		(dmasound.mach.version >> 8), (dmasound.mach.version & 0xff)) ;
	printk(KERN_INFO "Write will use %4d fragments of %7d bytes as default\n",
		numWriteBufs, writeBufSize) ;
	return 0;
}

#ifdef MODULE

void dmasound_deinit(void)
{
	if (irq_installed) {
		sound_silence();
		dmasound.mach.irqcleanup();
		irq_installed = 0;
	}

	write_sq_release_buffers();

	if (mixer_unit >= 0)
		unregister_sound_mixer(mixer_unit);
	if (state_unit >= 0)
		unregister_sound_special(state_unit);
	if (sq_unit >= 0)
		unregister_sound_dsp(sq_unit);
}

#else 

static int dmasound_setup(char *str)
{
	int ints[6], size;

	str = get_options(str, ARRAY_SIZE(ints), ints);

	


	switch (ints[0]) {
	case 3:
		if ((ints[3] < 0) || (ints[3] > MAX_CATCH_RADIUS))
			printk("dmasound_setup: invalid catch radius, using default = %d\n", catchRadius);
		else
			catchRadius = ints[3];
		
	case 2:
		if (ints[1] < MIN_BUFFERS)
			printk("dmasound_setup: invalid number of buffers, using default = %d\n", numWriteBufs);
		else
			numWriteBufs = ints[1];
		
	case 1:
		if ((size = ints[2]) < 256) 
			size <<= 10 ;
                if (size < MIN_BUFSIZE || size > MAX_BUFSIZE)
                        printk("dmasound_setup: invalid write buffer size, using default = %d\n", writeBufSize);
                else
                        writeBufSize = size;
	case 0:
		break;
	default:
		printk("dmasound_setup: invalid number of arguments\n");
		return 0;
	}
	return 1;
}

__setup("dmasound=", dmasound_setup);

#endif 


#ifdef HAS_8BIT_TABLES

char dmasound_ulaw2dma8[] = {
	-126,	-122,	-118,	-114,	-110,	-106,	-102,	-98,
	-94,	-90,	-86,	-82,	-78,	-74,	-70,	-66,
	-63,	-61,	-59,	-57,	-55,	-53,	-51,	-49,
	-47,	-45,	-43,	-41,	-39,	-37,	-35,	-33,
	-31,	-30,	-29,	-28,	-27,	-26,	-25,	-24,
	-23,	-22,	-21,	-20,	-19,	-18,	-17,	-16,
	-16,	-15,	-15,	-14,	-14,	-13,	-13,	-12,
	-12,	-11,	-11,	-10,	-10,	-9,	-9,	-8,
	-8,	-8,	-7,	-7,	-7,	-7,	-6,	-6,
	-6,	-6,	-5,	-5,	-5,	-5,	-4,	-4,
	-4,	-4,	-4,	-4,	-3,	-3,	-3,	-3,
	-3,	-3,	-3,	-3,	-2,	-2,	-2,	-2,
	-2,	-2,	-2,	-2,	-2,	-2,	-2,	-2,
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	0,
	125,	121,	117,	113,	109,	105,	101,	97,
	93,	89,	85,	81,	77,	73,	69,	65,
	62,	60,	58,	56,	54,	52,	50,	48,
	46,	44,	42,	40,	38,	36,	34,	32,
	30,	29,	28,	27,	26,	25,	24,	23,
	22,	21,	20,	19,	18,	17,	16,	15,
	15,	14,	14,	13,	13,	12,	12,	11,
	11,	10,	10,	9,	9,	8,	8,	7,
	7,	7,	6,	6,	6,	6,	5,	5,
	5,	5,	4,	4,	4,	4,	3,	3,
	3,	3,	3,	3,	2,	2,	2,	2,
	2,	2,	2,	2,	1,	1,	1,	1,
	1,	1,	1,	1,	1,	1,	1,	1,
	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0
};


char dmasound_alaw2dma8[] = {
	-22,	-21,	-24,	-23,	-18,	-17,	-20,	-19,
	-30,	-29,	-32,	-31,	-26,	-25,	-28,	-27,
	-11,	-11,	-12,	-12,	-9,	-9,	-10,	-10,
	-15,	-15,	-16,	-16,	-13,	-13,	-14,	-14,
	-86,	-82,	-94,	-90,	-70,	-66,	-78,	-74,
	-118,	-114,	-126,	-122,	-102,	-98,	-110,	-106,
	-43,	-41,	-47,	-45,	-35,	-33,	-39,	-37,
	-59,	-57,	-63,	-61,	-51,	-49,	-55,	-53,
	-2,	-2,	-2,	-2,	-2,	-2,	-2,	-2,
	-2,	-2,	-2,	-2,	-2,	-2,	-2,	-2,
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,
	-6,	-6,	-6,	-6,	-5,	-5,	-5,	-5,
	-8,	-8,	-8,	-8,	-7,	-7,	-7,	-7,
	-3,	-3,	-3,	-3,	-3,	-3,	-3,	-3,
	-4,	-4,	-4,	-4,	-4,	-4,	-4,	-4,
	21,	20,	23,	22,	17,	16,	19,	18,
	29,	28,	31,	30,	25,	24,	27,	26,
	10,	10,	11,	11,	8,	8,	9,	9,
	14,	14,	15,	15,	12,	12,	13,	13,
	86,	82,	94,	90,	70,	66,	78,	74,
	118,	114,	126,	122,	102,	98,	110,	106,
	43,	41,	47,	45,	35,	33,	39,	37,
	59,	57,	63,	61,	51,	49,	55,	53,
	1,	1,	1,	1,	1,	1,	1,	1,
	1,	1,	1,	1,	1,	1,	1,	1,
	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,
	5,	5,	5,	5,	4,	4,	4,	4,
	7,	7,	7,	7,	6,	6,	6,	6,
	2,	2,	2,	2,	2,	2,	2,	2,
	3,	3,	3,	3,	3,	3,	3,	3
};
#endif 


EXPORT_SYMBOL(dmasound);
EXPORT_SYMBOL(dmasound_init);
#ifdef MODULE
EXPORT_SYMBOL(dmasound_deinit);
#endif
EXPORT_SYMBOL(dmasound_write_sq);
EXPORT_SYMBOL(dmasound_catchRadius);
#ifdef HAS_8BIT_TABLES
EXPORT_SYMBOL(dmasound_ulaw2dma8);
EXPORT_SYMBOL(dmasound_alaw2dma8);
#endif
