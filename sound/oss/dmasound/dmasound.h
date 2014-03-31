#ifndef _dmasound_h_
#define _dmasound_h_

#include <linux/types.h>

#define SND_NDEVS	256	
#define SND_DEV_CTL	0	
#define SND_DEV_SEQ	1	
#define SND_DEV_MIDIN	2	
#define SND_DEV_DSP	3	
#define SND_DEV_AUDIO	4	
#define SND_DEV_DSP16	5	
#define SND_DEV_STATUS	6	
#define SND_DEV_SEQ2	8	
#define SND_DEV_SNDPROC 9	
#define SND_DEV_PSS	SND_DEV_SNDPROC

#define DEBUG_DMASOUND 1

#define MAX_AUDIO_DEV	5
#define MAX_MIXER_DEV	4
#define MAX_SYNTH_DEV	3
#define MAX_MIDI_DEV	6
#define MAX_TIMER_DEV	3

#define MAX_CATCH_RADIUS	10

#define le2be16(x)	(((x)<<8 & 0xff00) | ((x)>>8 & 0x00ff))
#define le2be16dbl(x)	(((x)<<8 & 0xff00ff00) | ((x)>>8 & 0x00ff00ff))

#define IOCTL_IN(arg, ret) \
	do { int error = get_user(ret, (int __user *)(arg)); \
		if (error) return error; \
	} while (0)
#define IOCTL_OUT(arg, ret)	ioctl_return((int __user *)(arg), ret)

static inline int ioctl_return(int __user *addr, int value)
{
	return value < 0 ? value : put_user(value, addr);
}



#undef HAS_8BIT_TABLES

#if defined(CONFIG_DMASOUND_ATARI) || defined(CONFIG_DMASOUND_ATARI_MODULE) ||\
    defined(CONFIG_DMASOUND_PAULA) || defined(CONFIG_DMASOUND_PAULA_MODULE) ||\
    defined(CONFIG_DMASOUND_Q40) || defined(CONFIG_DMASOUND_Q40_MODULE)
#define HAS_8BIT_TABLES
#define MIN_BUFFERS	4
#define MIN_BUFSIZE	(1<<12)	
#define MIN_FRAG_SIZE	8	
#define MAX_BUFSIZE	(1<<17)	
#define MAX_FRAG_SIZE	15	

#else 

#define MIN_BUFFERS	2
#define MIN_BUFSIZE	(1<<8)	
#define MIN_FRAG_SIZE	8
#define MAX_BUFSIZE	(1<<18)	
#define MAX_FRAG_SIZE	16	
#endif

#define DEFAULT_N_BUFFERS 4
#define DEFAULT_BUFF_SIZE (1<<15)


extern int dmasound_init(void);
#ifdef MODULE
extern void dmasound_deinit(void);
#else
#define dmasound_deinit()	do { } while (0)
#endif


typedef struct {
    int format;		
    int stereo;		
    int size;		
    int speed;		
} SETTINGS;


typedef struct {
    const char *name;
    const char *name2;
    struct module *owner;
    void *(*dma_alloc)(unsigned int, gfp_t);
    void (*dma_free)(void *, unsigned int);
    int (*irqinit)(void);
#ifdef MODULE
    void (*irqcleanup)(void);
#endif
    void (*init)(void);
    void (*silence)(void);
    int (*setFormat)(int);
    int (*setVolume)(int);
    int (*setBass)(int);
    int (*setTreble)(int);
    int (*setGain)(int);
    void (*play)(void);
    void (*record)(void);		
    void (*mixer_init)(void);		
    int (*mixer_ioctl)(u_int, u_long);	
    int (*write_sq_setup)(void);	
    int (*read_sq_setup)(void);		
    int (*sq_open)(fmode_t);		
    int (*state_info)(char *, size_t);	
    void (*abort_read)(void);		
    int min_dsp_speed;
    int max_dsp_speed;
    int version ;
    int hardware_afmts ;		
					
    int capabilities ;		
    SETTINGS default_hard ;	
    SETTINGS default_soft ;	
} MACHINE;


typedef struct {
    ssize_t (*ct_ulaw)(const u_char __user *, size_t, u_char *, ssize_t *, ssize_t);
    ssize_t (*ct_alaw)(const u_char __user *, size_t, u_char *, ssize_t *, ssize_t);
    ssize_t (*ct_s8)(const u_char __user *, size_t, u_char *, ssize_t *, ssize_t);
    ssize_t (*ct_u8)(const u_char __user *, size_t, u_char *, ssize_t *, ssize_t);
    ssize_t (*ct_s16be)(const u_char __user *, size_t, u_char *, ssize_t *, ssize_t);
    ssize_t (*ct_u16be)(const u_char __user *, size_t, u_char *, ssize_t *, ssize_t);
    ssize_t (*ct_s16le)(const u_char __user *, size_t, u_char *, ssize_t *, ssize_t);
    ssize_t (*ct_u16le)(const u_char __user *, size_t, u_char *, ssize_t *, ssize_t);
} TRANS;

struct sound_settings {
    MACHINE mach;	
    SETTINGS hard;	
    SETTINGS soft;	
    SETTINGS dsp;	
    TRANS *trans_write;	
    int volume_left;	
    int volume_right;
    int bass;		
    int treble;
    int gain;
    int minDev;		
    spinlock_t lock;
};

extern struct sound_settings dmasound;

#ifdef HAS_8BIT_TABLES
extern char dmasound_ulaw2dma8[];
extern char dmasound_alaw2dma8[];
#endif


static inline int dmasound_set_volume(int volume)
{
	return dmasound.mach.setVolume(volume);
}

static inline int dmasound_set_bass(int bass)
{
	return dmasound.mach.setBass ? dmasound.mach.setBass(bass) : 50;
}

static inline int dmasound_set_treble(int treble)
{
	return dmasound.mach.setTreble ? dmasound.mach.setTreble(treble) : 50;
}

static inline int dmasound_set_gain(int gain)
{
	return dmasound.mach.setGain ? dmasound.mach.setGain(gain) : 100;
}



struct sound_queue {
    
    int numBufs;		
    int bufSize;		
    char **buffers;

    
    int locked ;		
    int user_frags ;		
    int user_frag_size ;	
    int max_count;		
    int block_size;		
    int max_active;		

    
    int front, rear, count;
    int rear_size;
    int active;
    wait_queue_head_t action_queue, open_queue, sync_queue;
    int non_blocking;
    int busy, syncing, xruns, died;
};

#define SLEEP(queue)		interruptible_sleep_on_timeout(&queue, HZ)
#define WAKE_UP(queue)		(wake_up_interruptible(&queue))

extern struct sound_queue dmasound_write_sq;
#define write_sq	dmasound_write_sq

extern int dmasound_catchRadius;
#define catchRadius	dmasound_catchRadius

#define BS_VAL 1

#define SW_INPUT_VOLUME_SCALE	4
#define SW_INPUT_VOLUME_DEFAULT	(128 / SW_INPUT_VOLUME_SCALE)

extern int expand_read_bal;	
extern uint software_input_volume; 

#endif 
