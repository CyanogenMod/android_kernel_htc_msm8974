#ifndef _AC97_CODEC_H_
#define _AC97_CODEC_H_

#include <linux/types.h>
#include <linux/soundcard.h>

#define  AC97_RESET               0x0000      
#define  AC97_MASTER_VOL_STEREO   0x0002      
#define  AC97_HEADPHONE_VOL       0x0004      
#define  AC97_MASTER_VOL_MONO     0x0006      
#define  AC97_MASTER_TONE         0x0008      
#define  AC97_PCBEEP_VOL          0x000a      
#define  AC97_PHONE_VOL           0x000c      
#define  AC97_MIC_VOL             0x000e      
#define  AC97_LINEIN_VOL          0x0010      
#define  AC97_CD_VOL              0x0012      
#define  AC97_VIDEO_VOL           0x0014      
#define  AC97_AUX_VOL             0x0016      
#define  AC97_PCMOUT_VOL          0x0018      
#define  AC97_RECORD_SELECT       0x001a      
#define  AC97_RECORD_GAIN         0x001c
#define  AC97_RECORD_GAIN_MIC     0x001e
#define  AC97_GENERAL_PURPOSE     0x0020
#define  AC97_3D_CONTROL          0x0022
#define  AC97_MODEM_RATE          0x0024
#define  AC97_POWER_CONTROL       0x0026

#define AC97_EXTENDED_ID          0x0028       
#define AC97_EXTENDED_STATUS      0x002A       
#define AC97_PCM_FRONT_DAC_RATE   0x002C       
#define AC97_PCM_SURR_DAC_RATE    0x002E       
#define AC97_PCM_LFE_DAC_RATE     0x0030       
#define AC97_PCM_LR_ADC_RATE      0x0032       
#define AC97_PCM_MIC_ADC_RATE     0x0034       
#define AC97_CENTER_LFE_MASTER    0x0036       
#define AC97_SURROUND_MASTER      0x0038       
#define AC97_RESERVED_3A          0x003A       

#define AC97_SPDIF_CONTROL        0x003A       

#define AC97_EXTENDED_MODEM_ID    0x003C
#define AC97_EXTEND_MODEM_STAT    0x003E
#define AC97_LINE1_RATE           0x0040
#define AC97_LINE2_RATE           0x0042
#define AC97_HANDSET_RATE         0x0044
#define AC97_LINE1_LEVEL          0x0046
#define AC97_LINE2_LEVEL          0x0048
#define AC97_HANDSET_LEVEL        0x004A
#define AC97_GPIO_CONFIG          0x004C
#define AC97_GPIO_POLARITY        0x004E
#define AC97_GPIO_STICKY          0x0050
#define AC97_GPIO_WAKE_UP         0x0052
#define AC97_GPIO_STATUS          0x0054
#define AC97_MISC_MODEM_STAT      0x0056
#define AC97_RESERVED_58          0x0058


#define AC97_VENDOR_ID1           0x007c
#define AC97_VENDOR_ID2           0x007e

#define AC97_MUTE                 0x8000
#define AC97_MICBOOST             0x0040
#define AC97_LEFTVOL              0x3f00
#define AC97_RIGHTVOL             0x003f

#define AC97_RECMUX_MIC           0x0000
#define AC97_RECMUX_CD            0x0101
#define AC97_RECMUX_VIDEO         0x0202
#define AC97_RECMUX_AUX           0x0303
#define AC97_RECMUX_LINE          0x0404
#define AC97_RECMUX_STEREO_MIX    0x0505
#define AC97_RECMUX_MONO_MIX      0x0606
#define AC97_RECMUX_PHONE         0x0707

#define AC97_GP_LPBK              0x0080       
#define AC97_GP_MS                0x0100       
#define AC97_GP_MIX               0x0200       
#define AC97_GP_RLBK              0x0400       
#define AC97_GP_LLBK              0x0800       
#define AC97_GP_LD                0x1000       
#define AC97_GP_3D                0x2000       
#define AC97_GP_ST                0x4000       
#define AC97_GP_POP               0x8000       

#define AC97_EA_VRA               0x0001       
#define AC97_EA_DRA               0x0002       
#define AC97_EA_SPDIF             0x0004       
#define AC97_EA_VRM               0x0008       
#define AC97_EA_CDAC              0x0040       
#define AC97_EA_SDAC              0x0040       
#define AC97_EA_LDAC              0x0080       
#define AC97_EA_MDAC              0x0100       
#define AC97_EA_SPCV              0x0400       
#define AC97_EA_PRI               0x0800       
#define AC97_EA_PRJ               0x1000       
#define AC97_EA_PRK               0x2000       
#define AC97_EA_PRL               0x4000       
#define AC97_EA_SLOT_MASK         0xffcf       
#define AC97_EA_SPSA_3_4          0x0000       
#define AC97_EA_SPSA_7_8          0x0010       
#define AC97_EA_SPSA_6_9          0x0020       
#define AC97_EA_SPSA_10_11        0x0030       

#define AC97_SC_PRO               0x0001       
#define AC97_SC_NAUDIO            0x0002       
#define AC97_SC_COPY              0x0004       /* Copyright status */
#define AC97_SC_PRE               0x0008       
#define AC97_SC_CC_MASK           0x07f0       
#define AC97_SC_L                 0x0800       
#define AC97_SC_SPSR_MASK         0xcfff       
#define AC97_SC_SPSR_44K          0x0000       
#define AC97_SC_SPSR_48K          0x2000       
#define AC97_SC_SPSR_32K          0x3000       
#define AC97_SC_DRS               0x4000       
#define AC97_SC_V                 0x8000       


#define AC97_PWR_MDM              0x0010       
#define AC97_PWR_REF              0x0008       
#define AC97_PWR_ANL              0x0004       
#define AC97_PWR_DAC              0x0002       
#define AC97_PWR_ADC              0x0001       

#define AC97_PWR_PR0              0x0100       
#define AC97_PWR_PR1              0x0200       
#define AC97_PWR_PR2              0x0400       
#define AC97_PWR_PR3              0x0800       
#define AC97_PWR_PR4              0x1000       
#define AC97_PWR_PR5              0x2000       
#define AC97_PWR_PR6              0x4000       
#define AC97_PWR_PR7              0x8000       

#define AC97_EXTID_VRA            0x0001
#define AC97_EXTID_DRA            0x0002
#define AC97_EXTID_SPDIF          0x0004
#define AC97_EXTID_VRM            0x0008
#define AC97_EXTID_DSA0           0x0010
#define AC97_EXTID_DSA1           0x0020
#define AC97_EXTID_CDAC           0x0040
#define AC97_EXTID_SDAC           0x0080
#define AC97_EXTID_LDAC           0x0100
#define AC97_EXTID_AMAP           0x0200
#define AC97_EXTID_REV0           0x0400
#define AC97_EXTID_REV1           0x0800
#define AC97_EXTID_ID0            0x4000
#define AC97_EXTID_ID1            0x8000

#define AC97_EXTSTAT_VRA          0x0001
#define AC97_EXTSTAT_DRA          0x0002
#define AC97_EXTSTAT_SPDIF        0x0004
#define AC97_EXTSTAT_VRM          0x0008
#define AC97_EXTSTAT_SPSA0        0x0010
#define AC97_EXTSTAT_SPSA1        0x0020
#define AC97_EXTSTAT_CDAC         0x0040
#define AC97_EXTSTAT_SDAC         0x0080
#define AC97_EXTSTAT_LDAC         0x0100
#define AC97_EXTSTAT_MADC         0x0200
#define AC97_EXTSTAT_SPCV         0x0400
#define AC97_EXTSTAT_PRI          0x0800
#define AC97_EXTSTAT_PRJ          0x1000
#define AC97_EXTSTAT_PRK          0x2000
#define AC97_EXTSTAT_PRL          0x4000

#define AC97_EXTID_VRA            0x0001
#define AC97_EXTID_DRA            0x0002
#define AC97_EXTID_SPDIF          0x0004
#define AC97_EXTID_VRM            0x0008
#define AC97_EXTID_DSA0           0x0010
#define AC97_EXTID_DSA1           0x0020
#define AC97_EXTID_CDAC           0x0040
#define AC97_EXTID_SDAC           0x0080
#define AC97_EXTID_LDAC           0x0100
#define AC97_EXTID_AMAP           0x0200
#define AC97_EXTID_REV0           0x0400
#define AC97_EXTID_REV1           0x0800
#define AC97_EXTID_ID0            0x4000
#define AC97_EXTID_ID1            0x8000

#define AC97_EXTSTAT_VRA          0x0001
#define AC97_EXTSTAT_DRA          0x0002
#define AC97_EXTSTAT_SPDIF        0x0004
#define AC97_EXTSTAT_VRM          0x0008
#define AC97_EXTSTAT_SPSA0        0x0010
#define AC97_EXTSTAT_SPSA1        0x0020
#define AC97_EXTSTAT_CDAC         0x0040
#define AC97_EXTSTAT_SDAC         0x0080
#define AC97_EXTSTAT_LDAC         0x0100
#define AC97_EXTSTAT_MADC         0x0200
#define AC97_EXTSTAT_SPCV         0x0400
#define AC97_EXTSTAT_PRI          0x0800
#define AC97_EXTSTAT_PRJ          0x1000
#define AC97_EXTSTAT_PRK          0x2000
#define AC97_EXTSTAT_PRL          0x4000

#define AC97_PWR_D0               0x0000      
#define AC97_PWR_D1              AC97_PWR_PR0|AC97_PWR_PR1|AC97_PWR_PR4
#define AC97_PWR_D2              AC97_PWR_PR0|AC97_PWR_PR1|AC97_PWR_PR2|AC97_PWR_PR3|AC97_PWR_PR4
#define AC97_PWR_D3              AC97_PWR_PR0|AC97_PWR_PR1|AC97_PWR_PR2|AC97_PWR_PR3|AC97_PWR_PR4
#define AC97_PWR_ANLOFF          AC97_PWR_PR2|AC97_PWR_PR3  

#define AC97_REG_CNT 64


#define AC97_STEREO_MASK (SOUND_MASK_VOLUME|SOUND_MASK_PCM|\
	SOUND_MASK_LINE|SOUND_MASK_CD|\
	SOUND_MASK_ALTPCM|SOUND_MASK_IGAIN|\
	SOUND_MASK_LINE1|SOUND_MASK_VIDEO)

#define AC97_SUPPORTED_MASK (AC97_STEREO_MASK | \
	SOUND_MASK_BASS|SOUND_MASK_TREBLE|\
	SOUND_MASK_SPEAKER|SOUND_MASK_MIC|\
	SOUND_MASK_PHONEIN|SOUND_MASK_PHONEOUT)

#define AC97_RECORD_MASK (SOUND_MASK_MIC|\
	SOUND_MASK_CD|SOUND_MASK_IGAIN|SOUND_MASK_VIDEO|\
	SOUND_MASK_LINE1| SOUND_MASK_LINE|\
	SOUND_MASK_PHONEIN)

#define supported_mixer(CODEC,FOO) ((FOO >= 0) && \
                                    (FOO < SOUND_MIXER_NRDEVICES) && \
                                    (CODEC)->supported_mixers & (1<<FOO) )

struct ac97_codec {
	
	struct list_head list;

	
	void *private_data;

	char *name;
	int id;
	int dev_mixer; 
	int type;
	u32 model;

	unsigned int modem:1;

	struct ac97_ops *codec_ops;

	u16  (*codec_read)  (struct ac97_codec *codec, u8 reg);
	void (*codec_write) (struct ac97_codec *codec, u8 reg, u16 val);

	
	void  (*codec_wait)  (struct ac97_codec *codec);

	
	void  (*codec_unregister) (struct ac97_codec *codec);
	
	struct ac97_driver *driver;
	void *driver_private;	
	
	spinlock_t lock;
	
	
	int modcnt;
	int supported_mixers;
	int stereo_mixers;
	int record_sources;

	
	int flags;

	int bit_resolution;

	
	int  (*read_mixer) (struct ac97_codec *codec, int oss_channel);
	void (*write_mixer)(struct ac97_codec *codec, int oss_channel,
			    unsigned int left, unsigned int right);
	int  (*recmask_io) (struct ac97_codec *codec, int rw, int mask);
	int  (*mixer_ioctl)(struct ac97_codec *codec, unsigned int cmd, unsigned long arg);

	
	unsigned int mixer_state[SOUND_MIXER_NRDEVICES];

	
	int  (*modem_ioctl)(struct ac97_codec *codec, unsigned int cmd, unsigned long arg);
};

 
struct ac97_ops
{
	
	int (*init)(struct ac97_codec *c);
	
	int (*amplifier)(struct ac97_codec *codec, int on);
	
	int (*digital)(struct ac97_codec *codec, int slots, int rate, int mode);
#define AUDIO_DIGITAL		0x8000
#define AUDIO_PRO		0x4000
#define AUDIO_DRS		0x2000
#define AUDIO_CCMASK		0x003F
	
#define AC97_DELUDED_MODEM	1	
#define AC97_NO_PCM_VOLUME	2	
#define AC97_DEFAULT_POWER_OFF 4 
};

extern int ac97_probe_codec(struct ac97_codec *);

extern struct ac97_codec *ac97_alloc_codec(void);
extern void ac97_release_codec(struct ac97_codec *codec);

struct ac97_driver {
	struct list_head list;
	char *name;
	u32 codec_id;
	u32 codec_mask;
	int (*probe) (struct ac97_codec *codec, struct ac97_driver *driver);
	void (*remove) (struct ac97_codec *codec, struct ac97_driver *driver);
};

enum {
	AC97_TUNE_DEFAULT = -1, 
	AC97_TUNE_NONE = 0,     
	AC97_TUNE_HP_ONLY,      
	AC97_TUNE_SWAP_HP,      
	AC97_TUNE_SWAP_SURROUND, 
	AC97_TUNE_AD_SHARING,   
	AC97_TUNE_ALC_JACK,     
};

struct ac97_quirk {
	unsigned short vendor;  
	unsigned short device;  
	unsigned short mask;    
	const char *name;       
	int type;               
};

#endif 
