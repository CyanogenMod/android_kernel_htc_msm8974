#ifndef __SOUND_SND_WAVEFRONT_H__
#define __SOUND_SND_WAVEFRONT_H__

#include "mpu401.h"
#include "hwdep.h"
#include "rawmidi.h"
#include "wavefront.h"  


struct _snd_wavefront_midi;
struct _snd_wavefront_card;
struct _snd_wavefront;

typedef struct _snd_wavefront_midi snd_wavefront_midi_t;
typedef struct _snd_wavefront_card snd_wavefront_card_t;
typedef struct _snd_wavefront snd_wavefront_t;

typedef enum { internal_mpu = 0, external_mpu = 1 } snd_wavefront_mpu_id;

struct _snd_wavefront_midi {
        unsigned long            base;        
	char                     isvirtual;   
	char			 istimer;     
        snd_wavefront_mpu_id     output_mpu;  
        snd_wavefront_mpu_id     input_mpu;   
        unsigned int             mode[2];     
	struct snd_rawmidi_substream	 *substream_output[2];
	struct snd_rawmidi_substream	 *substream_input[2];
	struct timer_list	 timer;
        spinlock_t               open;
        spinlock_t               virtual;     
};

#define	OUTPUT_READY	0x40
#define	INPUT_AVAIL	0x80
#define	MPU_ACK		0xFE
#define	UART_MODE_ON	0x3F

extern struct snd_rawmidi_ops snd_wavefront_midi_output;
extern struct snd_rawmidi_ops snd_wavefront_midi_input;

extern void   snd_wavefront_midi_enable_virtual (snd_wavefront_card_t *);
extern void   snd_wavefront_midi_disable_virtual (snd_wavefront_card_t *);
extern void   snd_wavefront_midi_interrupt (snd_wavefront_card_t *);
extern int    snd_wavefront_midi_start (snd_wavefront_card_t *);

struct _snd_wavefront {
	unsigned long    irq;   
	unsigned long    base;  
	struct resource	 *res_base; 

#define mpu_data_port    base 
#define mpu_command_port base + 1 
#define mpu_status_port  base + 1 
#define data_port        base + 2 
#define status_port      base + 3 
#define control_port     base + 3 
#define block_port       base + 4 
#define last_block_port  base + 6 


#define fx_status       base + 8 
#define fx_op           base + 8 
#define fx_lcr          base + 9 
#define fx_dsp_addr     base + 0xa
#define fx_dsp_page     base + 0xb 
#define fx_dsp_lsb      base + 0xc 
#define fx_dsp_msb      base + 0xd 
#define fx_mod_addr     base + 0xe
#define fx_mod_data     base + 0xf 

	volatile int irq_ok;               
        volatile int irq_cnt;              
	char debug;                        
	int freemem;                        

	char fw_version[2];                
	char hw_version[2];                
	char israw;                        
	char has_fx;                       
	char fx_initialized;               
	char prog_status[WF_MAX_PROGRAM];  
	char patch_status[WF_MAX_PATCH];   
	char sample_status[WF_MAX_SAMPLE]; 
	int samples_used;                  
	char interrupts_are_midi;          
	char rom_samples_rdonly;           
	spinlock_t irq_lock;
	wait_queue_head_t interrupt_sleeper; 
	snd_wavefront_midi_t midi;         
	struct snd_card *card;
};

struct _snd_wavefront_card {
	snd_wavefront_t wavefront;
#ifdef CONFIG_PNP
	struct pnp_dev *wss;
	struct pnp_dev *ctrl;
	struct pnp_dev *mpu;
	struct pnp_dev *synth;
#endif 
};

extern void snd_wavefront_internal_interrupt (snd_wavefront_card_t *card);
extern int  snd_wavefront_detect_irq (snd_wavefront_t *dev) ;
extern int  snd_wavefront_check_irq (snd_wavefront_t *dev, int irq);
extern int  snd_wavefront_restart (snd_wavefront_t *dev);
extern int  snd_wavefront_start (snd_wavefront_t *dev);
extern int  snd_wavefront_detect (snd_wavefront_card_t *card);
extern int  snd_wavefront_config_midi (snd_wavefront_t *dev) ;
extern int  snd_wavefront_cmd (snd_wavefront_t *, int, unsigned char *,
			       unsigned char *);

extern int snd_wavefront_synth_ioctl   (struct snd_hwdep *, 
					struct file *,
					unsigned int cmd, 
					unsigned long arg);
extern int  snd_wavefront_synth_open    (struct snd_hwdep *, struct file *);
extern int  snd_wavefront_synth_release (struct snd_hwdep *, struct file *);


extern int  snd_wavefront_fx_start  (snd_wavefront_t *);
extern int  snd_wavefront_fx_detect (snd_wavefront_t *);
extern int  snd_wavefront_fx_ioctl  (struct snd_hwdep *, 
				     struct file *,
				     unsigned int cmd, 
				     unsigned long arg);
extern int snd_wavefront_fx_open    (struct snd_hwdep *, struct file *);
extern int snd_wavefront_fx_release (struct snd_hwdep *, struct file *);


#define LOGNAME "WaveFront: "

#endif  
