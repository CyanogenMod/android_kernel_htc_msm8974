/*
 *  linux/sound/oss/dmasound/dmasound_paula.c
 *
 *  Amiga `Paula' DMA Sound Driver
 *
 *  See linux/sound/oss/dmasound/dmasound_core.c for copyright and credits
 *  prior to 28/01/2001
 *
 *  28/01/2001 [0.1] Iain Sandoe
 *		     - added versioning
 *		     - put in and populated the hardware_afmts field.
 *             [0.2] - put in SNDCTL_DSP_GETCAPS value.
 *	       [0.3] - put in constraint on state buffer usage.
 *	       [0.4] - put in default hard/soft settings
*/


#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/soundcard.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <asm/uaccess.h>
#include <asm/setup.h>
#include <asm/amigahw.h>
#include <asm/amigaints.h>
#include <asm/machdep.h>

#include "dmasound.h"

#define DMASOUND_PAULA_REVISION 0
#define DMASOUND_PAULA_EDITION 4

#define custom amiga_custom

extern volatile u_short amiga_audio_min_period;



extern u_short amiga_audio_period;



#define AMI_AUDIO_OFF	(DMAF_AUD0 | DMAF_AUD1 | DMAF_AUD2 | DMAF_AUD3)
#define AMI_AUDIO_8	(DMAF_SETCLR | DMAF_MASTER | DMAF_AUD0 | DMAF_AUD1)
#define AMI_AUDIO_14	(AMI_AUDIO_8 | DMAF_AUD2 | DMAF_AUD3)



static int write_sq_block_size_half, write_sq_block_size_quarter;




static void *AmiAlloc(unsigned int size, gfp_t flags);
static void AmiFree(void *obj, unsigned int size);
static int AmiIrqInit(void);
#ifdef MODULE
static void AmiIrqCleanUp(void);
#endif
static void AmiSilence(void);
static void AmiInit(void);
static int AmiSetFormat(int format);
static int AmiSetVolume(int volume);
static int AmiSetTreble(int treble);
static void AmiPlayNextFrame(int index);
static void AmiPlay(void);
static irqreturn_t AmiInterrupt(int irq, void *dummy);

#ifdef CONFIG_HEARTBEAT


static void (*saved_heartbeat)(int) = NULL;

static inline void disable_heartbeat(void)
{
	if (mach_heartbeat) {
	    saved_heartbeat = mach_heartbeat;
	    mach_heartbeat = NULL;
	}
	AmiSetTreble(dmasound.treble);
}

static inline void enable_heartbeat(void)
{
	if (saved_heartbeat)
	    mach_heartbeat = saved_heartbeat;
}
#else 
#define disable_heartbeat()	do { } while (0)
#define enable_heartbeat()	do { } while (0)
#endif 



static void AmiMixerInit(void);
static int AmiMixerIoctl(u_int cmd, u_long arg);
static int AmiWriteSqSetup(void);
static int AmiStateInfo(char *buffer, size_t space);






static ssize_t ami_ct_s8(const u_char __user *userPtr, size_t userCount,
			 u_char frame[], ssize_t *frameUsed, ssize_t frameLeft)
{
	ssize_t count, used;

	if (!dmasound.soft.stereo) {
		void *p = &frame[*frameUsed];
		count = min_t(unsigned long, userCount, frameLeft) & ~1;
		used = count;
		if (copy_from_user(p, userPtr, count))
			return -EFAULT;
	} else {
		u_char *left = &frame[*frameUsed>>1];
		u_char *right = left+write_sq_block_size_half;
		count = min_t(unsigned long, userCount, frameLeft)>>1 & ~1;
		used = count*2;
		while (count > 0) {
			if (get_user(*left++, userPtr++)
			    || get_user(*right++, userPtr++))
				return -EFAULT;
			count--;
		}
	}
	*frameUsed += used;
	return used;
}



#define GENERATE_AMI_CT8(funcname, convsample)				\
static ssize_t funcname(const u_char __user *userPtr, size_t userCount,	\
			u_char frame[], ssize_t *frameUsed,		\
			ssize_t frameLeft)				\
{									\
	ssize_t count, used;						\
									\
	if (!dmasound.soft.stereo) {					\
		u_char *p = &frame[*frameUsed];				\
		count = min_t(size_t, userCount, frameLeft) & ~1;	\
		used = count;						\
		while (count > 0) {					\
			u_char data;					\
			if (get_user(data, userPtr++))			\
				return -EFAULT;				\
			*p++ = convsample(data);			\
			count--;					\
		}							\
	} else {							\
		u_char *left = &frame[*frameUsed>>1];			\
		u_char *right = left+write_sq_block_size_half;		\
		count = min_t(size_t, userCount, frameLeft)>>1 & ~1;	\
		used = count*2;						\
		while (count > 0) {					\
			u_char data;					\
			if (get_user(data, userPtr++))			\
				return -EFAULT;				\
			*left++ = convsample(data);			\
			if (get_user(data, userPtr++))			\
				return -EFAULT;				\
			*right++ = convsample(data);			\
			count--;					\
		}							\
	}								\
	*frameUsed += used;						\
	return used;							\
}

#define AMI_CT_ULAW(x)	(dmasound_ulaw2dma8[(x)])
#define AMI_CT_ALAW(x)	(dmasound_alaw2dma8[(x)])
#define AMI_CT_U8(x)	((x) ^ 0x80)

GENERATE_AMI_CT8(ami_ct_ulaw, AMI_CT_ULAW)
GENERATE_AMI_CT8(ami_ct_alaw, AMI_CT_ALAW)
GENERATE_AMI_CT8(ami_ct_u8, AMI_CT_U8)



#define GENERATE_AMI_CT_16(funcname, convsample)			\
static ssize_t funcname(const u_char __user *userPtr, size_t userCount,	\
			u_char frame[], ssize_t *frameUsed,		\
			ssize_t frameLeft)				\
{									\
	const u_short __user *ptr = (const u_short __user *)userPtr;	\
	ssize_t count, used;						\
	u_short data;							\
									\
	if (!dmasound.soft.stereo) {					\
		u_char *high = &frame[*frameUsed>>1];			\
		u_char *low = high+write_sq_block_size_half;		\
		count = min_t(size_t, userCount, frameLeft)>>1 & ~1;	\
		used = count*2;						\
		while (count > 0) {					\
			if (get_user(data, ptr++))			\
				return -EFAULT;				\
			data = convsample(data);			\
			*high++ = data>>8;				\
			*low++ = (data>>2) & 0x3f;			\
			count--;					\
		}							\
	} else {							\
		u_char *lefth = &frame[*frameUsed>>2];			\
		u_char *leftl = lefth+write_sq_block_size_quarter;	\
		u_char *righth = lefth+write_sq_block_size_half;	\
		u_char *rightl = righth+write_sq_block_size_quarter;	\
		count = min_t(size_t, userCount, frameLeft)>>2 & ~1;	\
		used = count*4;						\
		while (count > 0) {					\
			if (get_user(data, ptr++))			\
				return -EFAULT;				\
			data = convsample(data);			\
			*lefth++ = data>>8;				\
			*leftl++ = (data>>2) & 0x3f;			\
			if (get_user(data, ptr++))			\
				return -EFAULT;				\
			data = convsample(data);			\
			*righth++ = data>>8;				\
			*rightl++ = (data>>2) & 0x3f;			\
			count--;					\
		}							\
	}								\
	*frameUsed += used;						\
	return used;							\
}

#define AMI_CT_S16BE(x)	(x)
#define AMI_CT_U16BE(x)	((x) ^ 0x8000)
#define AMI_CT_S16LE(x)	(le2be16((x)))
#define AMI_CT_U16LE(x)	(le2be16((x)) ^ 0x8000)

GENERATE_AMI_CT_16(ami_ct_s16be, AMI_CT_S16BE)
GENERATE_AMI_CT_16(ami_ct_u16be, AMI_CT_U16BE)
GENERATE_AMI_CT_16(ami_ct_s16le, AMI_CT_S16LE)
GENERATE_AMI_CT_16(ami_ct_u16le, AMI_CT_U16LE)


static TRANS transAmiga = {
	.ct_ulaw	= ami_ct_ulaw,
	.ct_alaw	= ami_ct_alaw,
	.ct_s8		= ami_ct_s8,
	.ct_u8		= ami_ct_u8,
	.ct_s16be	= ami_ct_s16be,
	.ct_u16be	= ami_ct_u16be,
	.ct_s16le	= ami_ct_s16le,
	.ct_u16le	= ami_ct_u16le,
};


static inline void StopDMA(void)
{
	custom.aud[0].audvol = custom.aud[1].audvol = 0;
	custom.aud[2].audvol = custom.aud[3].audvol = 0;
	custom.dmacon = AMI_AUDIO_OFF;
	enable_heartbeat();
}

static void *AmiAlloc(unsigned int size, gfp_t flags)
{
	return amiga_chip_alloc((long)size, "dmasound [Paula]");
}

static void AmiFree(void *obj, unsigned int size)
{
	amiga_chip_free (obj);
}

static int __init AmiIrqInit(void)
{
	
	StopDMA();

	
	if (request_irq(IRQ_AMIGA_AUD0, AmiInterrupt, 0, "DMA sound",
			AmiInterrupt))
		return 0;
	return 1;
}

#ifdef MODULE
static void AmiIrqCleanUp(void)
{
	
	StopDMA();
	
	free_irq(IRQ_AMIGA_AUD0, AmiInterrupt);
}
#endif 

static void AmiSilence(void)
{
	
	StopDMA();
}


static void AmiInit(void)
{
	int period, i;

	AmiSilence();

	if (dmasound.soft.speed)
		period = amiga_colorclock/dmasound.soft.speed-1;
	else
		period = amiga_audio_min_period;
	dmasound.hard = dmasound.soft;
	dmasound.trans_write = &transAmiga;

	if (period < amiga_audio_min_period) {
		
		period = amiga_audio_min_period;
	} else if (period > 65535) {
		period = 65535;
	}
	dmasound.hard.speed = amiga_colorclock/(period+1);

	for (i = 0; i < 4; i++)
		custom.aud[i].audper = period;
	amiga_audio_period = period;
}


static int AmiSetFormat(int format)
{
	int size;

	

	switch (format) {
	case AFMT_QUERY:
		return dmasound.soft.format;
	case AFMT_MU_LAW:
	case AFMT_A_LAW:
	case AFMT_U8:
	case AFMT_S8:
		size = 8;
		break;
	case AFMT_S16_BE:
	case AFMT_U16_BE:
	case AFMT_S16_LE:
	case AFMT_U16_LE:
		size = 16;
		break;
	default: 
		size = 8;
		format = AFMT_S8;
	}

	dmasound.soft.format = format;
	dmasound.soft.size = size;
	if (dmasound.minDev == SND_DEV_DSP) {
		dmasound.dsp.format = format;
		dmasound.dsp.size = dmasound.soft.size;
	}
	AmiInit();

	return format;
}


#define VOLUME_VOXWARE_TO_AMI(v) \
	(((v) < 0) ? 0 : ((v) > 100) ? 64 : ((v) * 64)/100)
#define VOLUME_AMI_TO_VOXWARE(v) ((v)*100/64)

static int AmiSetVolume(int volume)
{
	dmasound.volume_left = VOLUME_VOXWARE_TO_AMI(volume & 0xff);
	custom.aud[0].audvol = dmasound.volume_left;
	dmasound.volume_right = VOLUME_VOXWARE_TO_AMI((volume & 0xff00) >> 8);
	custom.aud[1].audvol = dmasound.volume_right;
	if (dmasound.hard.size == 16) {
		if (dmasound.volume_left == 64 && dmasound.volume_right == 64) {
			custom.aud[2].audvol = 1;
			custom.aud[3].audvol = 1;
		} else {
			custom.aud[2].audvol = 0;
			custom.aud[3].audvol = 0;
		}
	}
	return VOLUME_AMI_TO_VOXWARE(dmasound.volume_left) |
	       (VOLUME_AMI_TO_VOXWARE(dmasound.volume_right) << 8);
}

static int AmiSetTreble(int treble)
{
	dmasound.treble = treble;
	if (treble < 50)
		ciaa.pra &= ~0x02;
	else
		ciaa.pra |= 0x02;
	return treble;
}


#define AMI_PLAY_LOADED		1
#define AMI_PLAY_PLAYING	2
#define AMI_PLAY_MASK		3


static void AmiPlayNextFrame(int index)
{
	u_char *start, *ch0, *ch1, *ch2, *ch3;
	u_long size;

	start = write_sq.buffers[write_sq.front];
	size = (write_sq.count == index ? write_sq.rear_size
					: write_sq.block_size)>>1;

	if (dmasound.hard.stereo) {
		ch0 = start;
		ch1 = start+write_sq_block_size_half;
		size >>= 1;
	} else {
		ch0 = start;
		ch1 = start;
	}

	disable_heartbeat();
	custom.aud[0].audvol = dmasound.volume_left;
	custom.aud[1].audvol = dmasound.volume_right;
	if (dmasound.hard.size == 8) {
		custom.aud[0].audlc = (u_short *)ZTWO_PADDR(ch0);
		custom.aud[0].audlen = size;
		custom.aud[1].audlc = (u_short *)ZTWO_PADDR(ch1);
		custom.aud[1].audlen = size;
		custom.dmacon = AMI_AUDIO_8;
	} else {
		size >>= 1;
		custom.aud[0].audlc = (u_short *)ZTWO_PADDR(ch0);
		custom.aud[0].audlen = size;
		custom.aud[1].audlc = (u_short *)ZTWO_PADDR(ch1);
		custom.aud[1].audlen = size;
		if (dmasound.volume_left == 64 && dmasound.volume_right == 64) {
			
			ch3 = ch0+write_sq_block_size_quarter;
			ch2 = ch1+write_sq_block_size_quarter;
			custom.aud[2].audvol = 1;  
			custom.aud[3].audvol = 1;  
			custom.aud[2].audlc = (u_short *)ZTWO_PADDR(ch2);
			custom.aud[2].audlen = size;
			custom.aud[3].audlc = (u_short *)ZTWO_PADDR(ch3);
			custom.aud[3].audlen = size;
			custom.dmacon = AMI_AUDIO_14;
		} else {
			custom.aud[2].audvol = 0;
			custom.aud[3].audvol = 0;
			custom.dmacon = AMI_AUDIO_8;
		}
	}
	write_sq.front = (write_sq.front+1) % write_sq.max_count;
	write_sq.active |= AMI_PLAY_LOADED;
}


static void AmiPlay(void)
{
	int minframes = 1;

	custom.intena = IF_AUD0;

	if (write_sq.active & AMI_PLAY_LOADED) {
		
		custom.intena = IF_SETCLR | IF_AUD0;
		return;
	}

	if (write_sq.active & AMI_PLAY_PLAYING)
		
		minframes = 2;

	if (write_sq.count < minframes) {
		
		custom.intena = IF_SETCLR | IF_AUD0;
		return;
	}

	if (write_sq.count <= minframes &&
	    write_sq.rear_size < write_sq.block_size && !write_sq.syncing) {
		custom.intena = IF_SETCLR | IF_AUD0;
		return;
	}

	AmiPlayNextFrame(minframes);

	custom.intena = IF_SETCLR | IF_AUD0;
}


static irqreturn_t AmiInterrupt(int irq, void *dummy)
{
	int minframes = 1;

	custom.intena = IF_AUD0;

	if (!write_sq.active) {
		WAKE_UP(write_sq.sync_queue);
		return IRQ_HANDLED;
	}

	if (write_sq.active & AMI_PLAY_PLAYING) {
		
		write_sq.count--;
		WAKE_UP(write_sq.action_queue);
	}

	if (write_sq.active & AMI_PLAY_LOADED)
		
		minframes = 2;

	
	write_sq.active = (write_sq.active<<1) & AMI_PLAY_MASK;

	if (!write_sq.active)
		
		StopDMA();

	custom.intena = IF_SETCLR | IF_AUD0;

	if (write_sq.count >= minframes)
		
		AmiPlay();

	if (!write_sq.active)
		WAKE_UP(write_sq.sync_queue);
	return IRQ_HANDLED;
}




static void __init AmiMixerInit(void)
{
	dmasound.volume_left = 64;
	dmasound.volume_right = 64;
	custom.aud[0].audvol = dmasound.volume_left;
	custom.aud[3].audvol = 1;	
	custom.aud[1].audvol = dmasound.volume_right;
	custom.aud[2].audvol = 1;	
	dmasound.treble = 50;
}

static int AmiMixerIoctl(u_int cmd, u_long arg)
{
	int data;
	switch (cmd) {
	    case SOUND_MIXER_READ_DEVMASK:
		    return IOCTL_OUT(arg, SOUND_MASK_VOLUME | SOUND_MASK_TREBLE);
	    case SOUND_MIXER_READ_RECMASK:
		    return IOCTL_OUT(arg, 0);
	    case SOUND_MIXER_READ_STEREODEVS:
		    return IOCTL_OUT(arg, SOUND_MASK_VOLUME);
	    case SOUND_MIXER_READ_VOLUME:
		    return IOCTL_OUT(arg,
			    VOLUME_AMI_TO_VOXWARE(dmasound.volume_left) |
			    VOLUME_AMI_TO_VOXWARE(dmasound.volume_right) << 8);
	    case SOUND_MIXER_WRITE_VOLUME:
		    IOCTL_IN(arg, data);
		    return IOCTL_OUT(arg, dmasound_set_volume(data));
	    case SOUND_MIXER_READ_TREBLE:
		    return IOCTL_OUT(arg, dmasound.treble);
	    case SOUND_MIXER_WRITE_TREBLE:
		    IOCTL_IN(arg, data);
		    return IOCTL_OUT(arg, dmasound_set_treble(data));
	}
	return -EINVAL;
}


static int AmiWriteSqSetup(void)
{
	write_sq_block_size_half = write_sq.block_size>>1;
	write_sq_block_size_quarter = write_sq_block_size_half>>1;
	return 0;
}


static int AmiStateInfo(char *buffer, size_t space)
{
	int len = 0;
	len += sprintf(buffer+len, "\tsound.volume_left = %d [0...64]\n",
		       dmasound.volume_left);
	len += sprintf(buffer+len, "\tsound.volume_right = %d [0...64]\n",
		       dmasound.volume_right);
	if (len >= space) {
		printk(KERN_ERR "dmasound_paula: overflowed state buffer alloc.\n") ;
		len = space ;
	}
	return len;
}



static SETTINGS def_hard = {
	.format	= AFMT_S8,
	.stereo	= 0,
	.size	= 8,
	.speed	= 8000
} ;

static SETTINGS def_soft = {
	.format	= AFMT_U8,
	.stereo	= 0,
	.size	= 8,
	.speed	= 8000
} ;

static MACHINE machAmiga = {
	.name		= "Amiga",
	.name2		= "AMIGA",
	.owner		= THIS_MODULE,
	.dma_alloc	= AmiAlloc,
	.dma_free	= AmiFree,
	.irqinit	= AmiIrqInit,
#ifdef MODULE
	.irqcleanup	= AmiIrqCleanUp,
#endif 
	.init		= AmiInit,
	.silence	= AmiSilence,
	.setFormat	= AmiSetFormat,
	.setVolume	= AmiSetVolume,
	.setTreble	= AmiSetTreble,
	.play		= AmiPlay,
	.mixer_init	= AmiMixerInit,
	.mixer_ioctl	= AmiMixerIoctl,
	.write_sq_setup	= AmiWriteSqSetup,
	.state_info	= AmiStateInfo,
	.min_dsp_speed	= 8000,
	.version	= ((DMASOUND_PAULA_REVISION<<8) | DMASOUND_PAULA_EDITION),
	.hardware_afmts	= (AFMT_S8 | AFMT_S16_BE), 
	.capabilities	= DSP_CAP_BATCH          
};




static int __init amiga_audio_probe(struct platform_device *pdev)
{
	dmasound.mach = machAmiga;
	dmasound.mach.default_hard = def_hard ;
	dmasound.mach.default_soft = def_soft ;
	return dmasound_init();
}

static int __exit amiga_audio_remove(struct platform_device *pdev)
{
	dmasound_deinit();
	return 0;
}

static struct platform_driver amiga_audio_driver = {
	.remove = __exit_p(amiga_audio_remove),
	.driver   = {
		.name	= "amiga-audio",
		.owner	= THIS_MODULE,
	},
};

static int __init amiga_audio_init(void)
{
	return platform_driver_probe(&amiga_audio_driver, amiga_audio_probe);
}

module_init(amiga_audio_init);

static void __exit amiga_audio_exit(void)
{
	platform_driver_unregister(&amiga_audio_driver);
}

module_exit(amiga_audio_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:amiga-audio");
