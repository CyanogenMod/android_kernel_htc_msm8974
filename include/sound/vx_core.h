/*
 * Driver for Digigram VX soundcards
 *
 * Hardware core part
 *
 * Copyright (c) 2002 by Takashi Iwai <tiwai@suse.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef __SOUND_VX_COMMON_H
#define __SOUND_VX_COMMON_H

#include <sound/pcm.h>
#include <sound/hwdep.h>
#include <linux/interrupt.h>

#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
#if !defined(CONFIG_USE_VXLOADER) && !defined(CONFIG_SND_VX_LIB) 
#define SND_VX_FW_LOADER	
#endif
#endif

struct firmware;
struct device;

#define VX_DRIVER_VERSION	0x010000	

#define SIZE_MAX_CMD    0x10
#define SIZE_MAX_STATUS 0x10

struct vx_rmh {
	u16	LgCmd;		
	u16	LgStat;		
	u32	Cmd[SIZE_MAX_CMD];
	u32	Stat[SIZE_MAX_STATUS];
	u16	DspStat;	
};
	
typedef u64 pcx_time_t;

#define VX_MAX_PIPES	16
#define VX_MAX_PERIODS	32
#define VX_MAX_CODECS	2

struct vx_ibl_info {
	int size;	
	int max_size;	
	int min_size;	
	int granularity;	
};

struct vx_pipe {
	int number;
	unsigned int is_capture: 1;
	unsigned int data_mode: 1;
	unsigned int running: 1;
	unsigned int prepared: 1;
	int channels;
	unsigned int differed_type;
	pcx_time_t pcx_time;
	struct snd_pcm_substream *substream;

	int hbuf_size;		
	int buffer_bytes;	
	int period_bytes;	
	int hw_ptr;		
	int position;		
	int transferred;	
	int align;		
	u64 cur_count;		

	unsigned int references;     
	struct vx_pipe *monitoring_pipe;  

	struct tasklet_struct start_tq;
};

struct vx_core;

struct snd_vx_ops {
	
	unsigned char (*in8)(struct vx_core *chip, int reg);
	unsigned int (*in32)(struct vx_core *chip, int reg);
	void (*out8)(struct vx_core *chip, int reg, unsigned char val);
	void (*out32)(struct vx_core *chip, int reg, unsigned int val);
	
	int (*test_and_ack)(struct vx_core *chip);
	void (*validate_irq)(struct vx_core *chip, int enable);
	
	void (*write_codec)(struct vx_core *chip, int codec, unsigned int data);
	void (*akm_write)(struct vx_core *chip, int reg, unsigned int data);
	void (*reset_codec)(struct vx_core *chip);
	void (*change_audio_source)(struct vx_core *chip, int src);
	void (*set_clock_source)(struct vx_core *chp, int src);
	
	int (*load_dsp)(struct vx_core *chip, int idx, const struct firmware *fw);
	void (*reset_dsp)(struct vx_core *chip);
	void (*reset_board)(struct vx_core *chip, int cold_reset);
	int (*add_controls)(struct vx_core *chip);
	
	void (*dma_write)(struct vx_core *chip, struct snd_pcm_runtime *runtime,
			  struct vx_pipe *pipe, int count);
	void (*dma_read)(struct vx_core *chip, struct snd_pcm_runtime *runtime,
			  struct vx_pipe *pipe, int count);
};

struct snd_vx_hardware {
	const char *name;
	int type;	

	
	unsigned int num_codecs;
	unsigned int num_ins;
	unsigned int num_outs;
	unsigned int output_level_max;
	const unsigned int *output_level_db_scale;
};

#define SND_VX_HWDEP_ID		"VX Loader"

enum {
	
	VX_TYPE_BOARD,		
	VX_TYPE_V2,		
	VX_TYPE_MIC,		
	
	VX_TYPE_VXPOCKET,	
	VX_TYPE_VXP440,		
	VX_TYPE_NUMS
};

enum {
	VX_STAT_XILINX_LOADED	= (1 << 0),	
	VX_STAT_DEVICE_INIT	= (1 << 1),	
	VX_STAT_CHIP_INIT	= (1 << 2),	
	VX_STAT_IN_SUSPEND	= (1 << 10),	
	VX_STAT_IS_STALE	= (1 << 15)	
};

#define VX_ANALOG_OUT_LEVEL_MAX		0xe3

struct vx_core {
	
	struct snd_card *card;
	struct snd_pcm *pcm[VX_MAX_CODECS];
	int type;	

	int irq;
	

	
	struct snd_vx_hardware *hw;
	struct snd_vx_ops *ops;

	spinlock_t lock;
	spinlock_t irq_lock;
	struct tasklet_struct tq;

	unsigned int chip_status;
	unsigned int pcm_running;

	struct device *dev;
	struct snd_hwdep *hwdep;

	struct vx_rmh irq_rmh;	

	unsigned int audio_info; 
	unsigned int audio_ins;
	unsigned int audio_outs;
	struct vx_pipe **playback_pipes;
	struct vx_pipe **capture_pipes;

	
	unsigned int audio_source;	
	unsigned int audio_source_target;
	unsigned int clock_mode;	
	unsigned int clock_source;	
	unsigned int freq;		
	unsigned int freq_detected;	
	unsigned int uer_detected;	
	unsigned int uer_bits;	
	struct vx_ibl_info ibl;	

	
	int output_level[VX_MAX_CODECS][2];	
	int audio_gain[2][4];			
	unsigned char audio_active[4];		
	int audio_monitor[4];			
	unsigned char audio_monitor_active[4];	

	struct mutex mixer_mutex;

	const struct firmware *firmware[4]; 
};


struct vx_core *snd_vx_create(struct snd_card *card, struct snd_vx_hardware *hw,
			      struct snd_vx_ops *ops, int extra_size);
int snd_vx_setup_firmware(struct vx_core *chip);
int snd_vx_load_boot_image(struct vx_core *chip, const struct firmware *dsp);
int snd_vx_dsp_boot(struct vx_core *chip, const struct firmware *dsp);
int snd_vx_dsp_load(struct vx_core *chip, const struct firmware *dsp);

void snd_vx_free_firmware(struct vx_core *chip);

irqreturn_t snd_vx_irq_handler(int irq, void *dev);

static inline int vx_test_and_ack(struct vx_core *chip)
{
	return chip->ops->test_and_ack(chip);
}

static inline void vx_validate_irq(struct vx_core *chip, int enable)
{
	chip->ops->validate_irq(chip, enable);
}

static inline unsigned char snd_vx_inb(struct vx_core *chip, int reg)
{
	return chip->ops->in8(chip, reg);
}

static inline unsigned int snd_vx_inl(struct vx_core *chip, int reg)
{
	return chip->ops->in32(chip, reg);
}

static inline void snd_vx_outb(struct vx_core *chip, int reg, unsigned char val)
{
	chip->ops->out8(chip, reg, val);
}

static inline void snd_vx_outl(struct vx_core *chip, int reg, unsigned int val)
{
	chip->ops->out32(chip, reg, val);
}

#define vx_inb(chip,reg)	snd_vx_inb(chip, VX_##reg)
#define vx_outb(chip,reg,val)	snd_vx_outb(chip, VX_##reg,val)
#define vx_inl(chip,reg)	snd_vx_inl(chip, VX_##reg)
#define vx_outl(chip,reg,val)	snd_vx_outl(chip, VX_##reg,val)

static inline void vx_reset_dsp(struct vx_core *chip)
{
	chip->ops->reset_dsp(chip);
}

int vx_send_msg(struct vx_core *chip, struct vx_rmh *rmh);
int vx_send_msg_nolock(struct vx_core *chip, struct vx_rmh *rmh);
int vx_send_rih(struct vx_core *chip, int cmd);
int vx_send_rih_nolock(struct vx_core *chip, int cmd);

void vx_reset_codec(struct vx_core *chip, int cold_reset);

int snd_vx_check_reg_bit(struct vx_core *chip, int reg, int mask, int bit, int time);
#define vx_check_isr(chip,mask,bit,time) snd_vx_check_reg_bit(chip, VX_ISR, mask, bit, time)
#define vx_wait_isr_bit(chip,bit) vx_check_isr(chip, bit, bit, 200)
#define vx_wait_for_rx_full(chip) vx_wait_isr_bit(chip, ISR_RX_FULL)


static inline void vx_pseudo_dma_write(struct vx_core *chip, struct snd_pcm_runtime *runtime,
				       struct vx_pipe *pipe, int count)
{
	chip->ops->dma_write(chip, runtime, pipe, count);
}

static inline void vx_pseudo_dma_read(struct vx_core *chip, struct snd_pcm_runtime *runtime,
				      struct vx_pipe *pipe, int count)
{
	chip->ops->dma_read(chip, runtime, pipe, count);
}



#define VX_ERR_MASK	0x1000000
#define vx_get_error(err)	(-(err) & ~VX_ERR_MASK)


int snd_vx_pcm_new(struct vx_core *chip);
void vx_pcm_update_intr(struct vx_core *chip, unsigned int events);

int snd_vx_mixer_new(struct vx_core *chip);
void vx_toggle_dac_mute(struct vx_core *chip, int mute);
int vx_sync_audio_source(struct vx_core *chip);
int vx_set_monitor_level(struct vx_core *chip, int audio, int level, int active);

void vx_set_iec958_status(struct vx_core *chip, unsigned int bits);
int vx_set_clock(struct vx_core *chip, unsigned int freq);
void vx_set_internal_clock(struct vx_core *chip, unsigned int freq);
int vx_change_frequency(struct vx_core *chip);


int snd_vx_suspend(struct vx_core *card, pm_message_t state);
int snd_vx_resume(struct vx_core *card);


#define vx_has_new_dsp(chip)	((chip)->type != VX_TYPE_BOARD)
#define vx_is_pcmcia(chip)	((chip)->type >= VX_TYPE_VXPOCKET)

enum {
	VX_AUDIO_SRC_DIGITAL,
	VX_AUDIO_SRC_LINE,
	VX_AUDIO_SRC_MIC
};

enum {
	INTERNAL_QUARTZ,
	UER_SYNC
};

enum {
	VX_CLOCK_MODE_AUTO,	
	VX_CLOCK_MODE_INTERNAL,	
	VX_CLOCK_MODE_EXTERNAL	
};

enum {
	VX_UER_MODE_CONSUMER,
	VX_UER_MODE_PROFESSIONAL,
	VX_UER_MODE_NOT_PRESENT,
};

enum {
	VX_ICR,
	VX_CVR,
	VX_ISR,
	VX_IVR,
	VX_RXH,
	VX_TXH = VX_RXH,
	VX_RXM,
	VX_TXM = VX_RXM,
	VX_RXL,
	VX_TXL = VX_RXL,
	VX_DMA,
	VX_CDSP,
	VX_RFREQ,
	VX_RUER_V2,
	VX_GAIN,
	VX_DATA = VX_GAIN,
	VX_MEMIRQ,
	VX_ACQ,
	VX_BIT0,
	VX_BIT1,
	VX_MIC0,
	VX_MIC1,
	VX_MIC2,
	VX_MIC3,
	VX_PLX0,
	VX_PLX1,
	VX_PLX2,

	VX_LOFREQ,  
	VX_HIFREQ,  
	VX_CSUER,   
	VX_RUER,    

	VX_REG_MAX,

	
	VX_RESET_DMA = VX_ISR,
	VX_CFG = VX_RFREQ,
	VX_STATUS = VX_MEMIRQ,
	VX_SELMIC = VX_MIC0,
	VX_COMPOT = VX_MIC1,
	VX_SCOMPR = VX_MIC2,
	VX_GLIMIT = VX_MIC3,
	VX_INTCSR = VX_PLX0,
	VX_CNTRL = VX_PLX1,
	VX_GPIOC = VX_PLX2,

	
	VX_MICRO = VX_MEMIRQ,
	VX_CODEC2 = VX_MEMIRQ,
	VX_DIALOG = VX_ACQ,

};

enum {
	RMH_SSIZE_FIXED = 0,	
	RMH_SSIZE_ARG = 1,	
	RMH_SSIZE_MASK = 2,	
};


#define ICR_HF1		0x10
#define ICR_HF0		0x08
#define ICR_TREQ	0x02	
#define ICR_RREQ	0x01	

#define CVR_HC		0x80

#define ISR_HF3		0x10
#define ISR_HF2		0x08
#define ISR_CHK		0x10
#define ISR_ERR		0x08
#define ISR_TX_READY	0x04
#define ISR_TX_EMPTY	0x02
#define ISR_RX_FULL	0x01

#define VX_DATA_CODEC_MASK	0x80
#define VX_DATA_XICOR_MASK	0x80

#define VX_SUER_FREQ_MASK		0x0c
#define VX_SUER_FREQ_32KHz_MASK		0x0c
#define VX_SUER_FREQ_44KHz_MASK		0x00
#define VX_SUER_FREQ_48KHz_MASK		0x04
#define VX_SUER_DATA_PRESENT_MASK	0x02
#define VX_SUER_CLOCK_PRESENT_MASK	0x01

#define VX_CUER_HH_BITC_SEL_MASK	0x08
#define VX_CUER_MH_BITC_SEL_MASK	0x04
#define VX_CUER_ML_BITC_SEL_MASK	0x02
#define VX_CUER_LL_BITC_SEL_MASK	0x01

#define XX_UER_CBITS_OFFSET_MASK	0x1f


#define VX_AUDIO_INFO_REAL_TIME	(1<<0)	
#define VX_AUDIO_INFO_OFFLINE	(1<<1)	
#define VX_AUDIO_INFO_MPEG1	(1<<5)
#define VX_AUDIO_INFO_MPEG2	(1<<6)
#define VX_AUDIO_INFO_LINEAR_8	(1<<7)
#define VX_AUDIO_INFO_LINEAR_16	(1<<8)
#define VX_AUDIO_INFO_LINEAR_24	(1<<9)

#define VXP_IRQ_OFFSET		0x40 
#define IRQ_MESS_WRITE_END          0x30
#define IRQ_MESS_WRITE_NEXT         0x32
#define IRQ_MESS_READ_NEXT          0x34
#define IRQ_MESS_READ_END           0x36
#define IRQ_MESSAGE                 0x38
#define IRQ_RESET_CHK               0x3A
#define IRQ_CONNECT_STREAM_NEXT     0x26
#define IRQ_CONNECT_STREAM_END      0x28
#define IRQ_PAUSE_START_CONNECT     0x2A
#define IRQ_END_CONNECTION          0x2C

#define ASYNC_EVENTS_PENDING            0x008000
#define HBUFFER_EVENTS_PENDING          0x004000   
#define NOTIF_EVENTS_PENDING            0x002000
#define TIME_CODE_EVENT_PENDING         0x001000
#define FREQUENCY_CHANGE_EVENT_PENDING  0x000800
#define END_OF_BUFFER_EVENTS_PENDING    0x000400
#define FATAL_DSP_ERROR                 0xff0000

 
#define HEADER_FMT_BASE			0xFED00000
#define HEADER_FMT_MONO			0x000000C0
#define HEADER_FMT_INTEL		0x00008000
#define HEADER_FMT_16BITS		0x00002000
#define HEADER_FMT_24BITS		0x00004000
#define HEADER_FMT_UPTO11		0x00000200	
#define HEADER_FMT_UPTO32		0x00000100	

#define XX_CODEC_SELECTOR               0x20
#define XX_CODEC_ADC_CONTROL_REGISTER   0x01
#define XX_CODEC_DAC_CONTROL_REGISTER   0x02
#define XX_CODEC_LEVEL_LEFT_REGISTER    0x03
#define XX_CODEC_LEVEL_RIGHT_REGISTER   0x04
#define XX_CODEC_PORT_MODE_REGISTER     0x05
#define XX_CODEC_STATUS_REPORT_REGISTER 0x06
#define XX_CODEC_CLOCK_CONTROL_REGISTER 0x07

#define CVAL_M110DB		0x000	
#define CVAL_M99DB		0x02C
#define CVAL_M21DB		0x163
#define CVAL_M18DB		0x16F
#define CVAL_M10DB		0x18F
#define CVAL_0DB		0x1B7
#define CVAL_18DB		0x1FF	
#define CVAL_MAX		0x1FF

#define AUDIO_IO_HAS_MUTE_LEVEL			0x400000
#define AUDIO_IO_HAS_MUTE_MONITORING_1		0x200000
#define AUDIO_IO_HAS_MUTE_MONITORING_2		0x100000
#define VALID_AUDIO_IO_DIGITAL_LEVEL		0x01
#define VALID_AUDIO_IO_MONITORING_LEVEL		0x02
#define VALID_AUDIO_IO_MUTE_LEVEL		0x04
#define VALID_AUDIO_IO_MUTE_MONITORING_1	0x08
#define VALID_AUDIO_IO_MUTE_MONITORING_2	0x10


#endif 
