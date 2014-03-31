#ifndef __SOUND_ICE1712_H
#define __SOUND_ICE1712_H

/*
 *   ALSA driver for ICEnsemble ICE1712 (Envy24)
 *
 *	Copyright (c) 2000 Jaroslav Kysela <perex@perex.cz>
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
 *
 */

#include <sound/control.h>
#include <sound/ac97_codec.h>
#include <sound/rawmidi.h>
#include <sound/i2c.h>
#include <sound/ak4xxx-adda.h>
#include <sound/ak4114.h>
#include <sound/pt2258.h>
#include <sound/pcm.h>
#include <sound/mpu401.h>



#define ICEREG(ice, x) ((ice)->port + ICE1712_REG_##x)

#define ICE1712_REG_CONTROL		0x00	
#define   ICE1712_RESET			0x80	
#define   ICE1712_SERR_LEVEL		0x04	
#define   ICE1712_NATIVE		0x01	
#define ICE1712_REG_IRQMASK		0x01	
#define   ICE1712_IRQ_MPU1		0x80
#define   ICE1712_IRQ_TIMER		0x40
#define   ICE1712_IRQ_MPU2		0x20
#define   ICE1712_IRQ_PROPCM		0x10
#define   ICE1712_IRQ_FM		0x08	
#define   ICE1712_IRQ_PBKDS		0x04	
#define   ICE1712_IRQ_CONCAP		0x02	
#define   ICE1712_IRQ_CONPBK		0x01	
#define ICE1712_REG_IRQSTAT		0x02	
#define ICE1712_REG_INDEX		0x03	
#define ICE1712_REG_DATA		0x04	
#define ICE1712_REG_NMI_STAT1		0x05	
#define ICE1712_REG_NMI_DATA		0x06	
#define ICE1712_REG_NMI_INDEX		0x07	
#define ICE1712_REG_AC97_INDEX		0x08	
#define ICE1712_REG_AC97_CMD		0x09	
#define   ICE1712_AC97_COLD		0x80	
#define   ICE1712_AC97_WARM		0x40	
#define   ICE1712_AC97_WRITE		0x20	
#define   ICE1712_AC97_READ		0x10	
#define   ICE1712_AC97_READY		0x08	
#define   ICE1712_AC97_PBK_VSR		0x02	
#define   ICE1712_AC97_CAP_VSR		0x01	
#define ICE1712_REG_AC97_DATA		0x0a	
#define ICE1712_REG_MPU1_CTRL		0x0c	
#define ICE1712_REG_MPU1_DATA		0x0d	
#define ICE1712_REG_I2C_DEV_ADDR	0x10	
#define   ICE1712_I2C_WRITE		0x01	
#define ICE1712_REG_I2C_BYTE_ADDR	0x11	
#define ICE1712_REG_I2C_DATA		0x12	
#define ICE1712_REG_I2C_CTRL		0x13	
#define   ICE1712_I2C_EEPROM		0x80	
#define   ICE1712_I2C_BUSY		0x01	
#define ICE1712_REG_CONCAP_ADDR		0x14	
#define ICE1712_REG_CONCAP_COUNT	0x18	
#define ICE1712_REG_SERR_SHADOW		0x1b	
#define ICE1712_REG_MPU2_CTRL		0x1c	
#define ICE1712_REG_MPU2_DATA		0x1d	
#define ICE1712_REG_TIMER		0x1e	


#define ICE1712_IREG_PBK_COUNT_LO	0x00
#define ICE1712_IREG_PBK_COUNT_HI	0x01
#define ICE1712_IREG_PBK_CTRL		0x02
#define ICE1712_IREG_PBK_LEFT		0x03	
#define ICE1712_IREG_PBK_RIGHT		0x04	
#define ICE1712_IREG_PBK_SOFT		0x05	
#define ICE1712_IREG_PBK_RATE_LO	0x06
#define ICE1712_IREG_PBK_RATE_MID	0x07
#define ICE1712_IREG_PBK_RATE_HI	0x08
#define ICE1712_IREG_CAP_COUNT_LO	0x10
#define ICE1712_IREG_CAP_COUNT_HI	0x11
#define ICE1712_IREG_CAP_CTRL		0x12
#define ICE1712_IREG_GPIO_DATA		0x20
#define ICE1712_IREG_GPIO_WRITE_MASK	0x21
#define ICE1712_IREG_GPIO_DIRECTION	0x22
#define ICE1712_IREG_CONSUMER_POWERDOWN	0x30
#define ICE1712_IREG_PRO_POWERDOWN	0x31


#define ICEDS(ice, x) ((ice)->dmapath_port + ICE1712_DS_##x)

#define ICE1712_DS_INTMASK		0x00	
#define ICE1712_DS_INTSTAT		0x02	
#define ICE1712_DS_DATA			0x04	
#define ICE1712_DS_INDEX		0x08	


#define ICE1712_DSC_ADDR0		0x00	
#define ICE1712_DSC_COUNT0		0x01	
#define ICE1712_DSC_ADDR1		0x02	
#define ICE1712_DSC_COUNT1		0x03	
#define ICE1712_DSC_CONTROL		0x04	
#define   ICE1712_BUFFER1		0x80	
#define   ICE1712_BUFFER1_AUTO		0x40	
#define   ICE1712_BUFFER0_AUTO		0x20	
#define   ICE1712_FLUSH			0x10	
#define   ICE1712_STEREO		0x08	
#define   ICE1712_16BIT			0x04	
#define   ICE1712_PAUSE			0x02	
#define   ICE1712_START			0x01	
#define ICE1712_DSC_RATE		0x05	
#define ICE1712_DSC_VOLUME		0x06	


#define ICEMT(ice, x) ((ice)->profi_port + ICE1712_MT_##x)

#define ICE1712_MT_IRQ			0x00	
#define   ICE1712_MULTI_CAPTURE		0x80	
#define   ICE1712_MULTI_PLAYBACK	0x40	
#define   ICE1712_MULTI_CAPSTATUS	0x02	
#define   ICE1712_MULTI_PBKSTATUS	0x01	
#define ICE1712_MT_RATE			0x01	
#define   ICE1712_SPDIF_MASTER		0x10	
#define ICE1712_MT_I2S_FORMAT		0x02	
#define ICE1712_MT_AC97_INDEX		0x04	
#define ICE1712_MT_AC97_CMD		0x05	
#define ICE1712_MT_AC97_DATA		0x06	
#define ICE1712_MT_PLAYBACK_ADDR	0x10	
#define ICE1712_MT_PLAYBACK_SIZE	0x14	
#define ICE1712_MT_PLAYBACK_COUNT	0x16	
#define ICE1712_MT_PLAYBACK_CONTROL	0x18	
#define   ICE1712_CAPTURE_START_SHADOW	0x04	
#define   ICE1712_PLAYBACK_PAUSE	0x02	
#define   ICE1712_PLAYBACK_START	0x01	
#define ICE1712_MT_CAPTURE_ADDR		0x20	
#define ICE1712_MT_CAPTURE_SIZE		0x24	
#define ICE1712_MT_CAPTURE_COUNT	0x26	
#define ICE1712_MT_CAPTURE_CONTROL	0x28	
#define   ICE1712_CAPTURE_START		0x01	
#define ICE1712_MT_ROUTE_PSDOUT03	0x30	
#define ICE1712_MT_ROUTE_SPDOUT		0x32	
#define ICE1712_MT_ROUTE_CAPTURE	0x34	
#define ICE1712_MT_MONITOR_VOLUME	0x38	
#define ICE1712_MT_MONITOR_INDEX	0x3a	
#define ICE1712_MT_MONITOR_RATE		0x3b	
#define ICE1712_MT_MONITOR_ROUTECTRL	0x3c	
#define   ICE1712_ROUTE_AC97		0x01	
#define ICE1712_MT_MONITOR_PEAKINDEX	0x3e	
#define ICE1712_MT_MONITOR_PEAKDATA	0x3f	


#define ICE1712_CFG_CLOCK	0xc0
#define   ICE1712_CFG_CLOCK512	0x00	
#define   ICE1712_CFG_CLOCK384  0x40	
#define   ICE1712_CFG_EXT	0x80	
#define ICE1712_CFG_2xMPU401	0x20	
#define ICE1712_CFG_NO_CON_AC97 0x10	
#define ICE1712_CFG_ADC_MASK	0x0c	
#define ICE1712_CFG_DAC_MASK	0x03	
#define ICE1712_CFG_PRO_I2S	0x80	
#define ICE1712_CFG_AC97_PACKED	0x01	
#define ICE1712_CFG_I2S_VOLUME	0x80	
#define ICE1712_CFG_I2S_96KHZ	0x40	
#define ICE1712_CFG_I2S_RESMASK	0x30	
#define ICE1712_CFG_I2S_OTHER	0x0f	
#define ICE1712_CFG_I2S_CHIPID	0xfc	
#define ICE1712_CFG_SPDIF_IN	0x02	
#define ICE1712_CFG_SPDIF_OUT	0x01	

#define ICE1712_DMA_MODE_WRITE		0x48
#define ICE1712_DMA_AUTOINIT		0x10



struct snd_ice1712;

struct snd_ice1712_eeprom {
	unsigned int subvendor;	
	unsigned char size;	
	unsigned char version;	
	unsigned char data[32];
	unsigned int gpiomask;
	unsigned int gpiostate;
	unsigned int gpiodir;
};

enum {
	ICE_EEP1_CODEC = 0,	
	ICE_EEP1_ACLINK,	
	ICE_EEP1_I2SID,		
	ICE_EEP1_SPDIF,		
	ICE_EEP1_GPIO_MASK,	
	ICE_EEP1_GPIO_STATE,	
	ICE_EEP1_GPIO_DIR,	
	ICE_EEP1_AC97_MAIN_LO,	
	ICE_EEP1_AC97_MAIN_HI,	
	ICE_EEP1_AC97_PCM_LO,	
	ICE_EEP1_AC97_PCM_HI,	
	ICE_EEP1_AC97_REC_LO,	
	ICE_EEP1_AC97_REC_HI,	
	ICE_EEP1_AC97_RECSRC,	
	ICE_EEP1_DAC_ID,	
	ICE_EEP1_DAC_ID1,
	ICE_EEP1_DAC_ID2,
	ICE_EEP1_DAC_ID3,
	ICE_EEP1_ADC_ID,	
	ICE_EEP1_ADC_ID1,
	ICE_EEP1_ADC_ID2,
	ICE_EEP1_ADC_ID3
};

#define ice_has_con_ac97(ice)	(!((ice)->eeprom.data[ICE_EEP1_CODEC] & ICE1712_CFG_NO_CON_AC97))


struct snd_ak4xxx_private {
	unsigned int cif:1;		
	unsigned char caddr;		
	unsigned int data_mask;		
	unsigned int clk_mask;		
	unsigned int cs_mask;		
	unsigned int cs_addr;		
	unsigned int cs_none;		
	unsigned int add_flags;		
	unsigned int mask_flags;	
	struct snd_akm4xxx_ops {
		void (*set_rate_val)(struct snd_akm4xxx *ak, unsigned int rate);
	} ops;
};

struct snd_ice1712_spdif {
	unsigned char cs8403_bits;
	unsigned char cs8403_stream_bits;
	struct snd_kcontrol *stream_ctl;

	struct snd_ice1712_spdif_ops {
		void (*open)(struct snd_ice1712 *, struct snd_pcm_substream *);
		void (*setup_rate)(struct snd_ice1712 *, int rate);
		void (*close)(struct snd_ice1712 *, struct snd_pcm_substream *);
		void (*default_get)(struct snd_ice1712 *, struct snd_ctl_elem_value *ucontrol);
		int (*default_put)(struct snd_ice1712 *, struct snd_ctl_elem_value *ucontrol);
		void (*stream_get)(struct snd_ice1712 *, struct snd_ctl_elem_value *ucontrol);
		int (*stream_put)(struct snd_ice1712 *, struct snd_ctl_elem_value *ucontrol);
	} ops;
};


struct snd_ice1712 {
	unsigned long conp_dma_size;
	unsigned long conc_dma_size;
	unsigned long prop_dma_size;
	unsigned long proc_dma_size;
	int irq;

	unsigned long port;
	unsigned long ddma_port;
	unsigned long dmapath_port;
	unsigned long profi_port;

	struct pci_dev *pci;
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct snd_pcm *pcm_ds;
	struct snd_pcm *pcm_pro;
	struct snd_pcm_substream *playback_con_substream;
	struct snd_pcm_substream *playback_con_substream_ds[6];
	struct snd_pcm_substream *capture_con_substream;
	struct snd_pcm_substream *playback_pro_substream;
	struct snd_pcm_substream *capture_pro_substream;
	unsigned int playback_pro_size;
	unsigned int capture_pro_size;
	unsigned int playback_con_virt_addr[6];
	unsigned int playback_con_active_buf[6];
	unsigned int capture_con_virt_addr;
	unsigned int ac97_ext_id;
	struct snd_ac97 *ac97;
	struct snd_rawmidi *rmidi[2];

	spinlock_t reg_lock;
	struct snd_info_entry *proc_entry;

	struct snd_ice1712_eeprom eeprom;

	unsigned int pro_volumes[20];
	unsigned int omni:1;		
	unsigned int dxr_enable:1;	
	unsigned int vt1724:1;
	unsigned int vt1720:1;
	unsigned int has_spdif:1;	
	unsigned int force_pdma4:1;	
	unsigned int force_rdma1:1;	
	unsigned int midi_output:1;	
	unsigned int midi_input:1;	
	unsigned int own_routing:1;	
	unsigned int num_total_dacs;	
	unsigned int num_total_adcs;	
	unsigned int cur_rate;		

	struct mutex open_mutex;
	struct snd_pcm_substream *pcm_reserved[4];
	struct snd_pcm_hw_constraint_list *hw_rates; 

	unsigned int akm_codecs;
	struct snd_akm4xxx *akm;
	struct snd_ice1712_spdif spdif;

	struct mutex i2c_mutex;	
	struct snd_i2c_bus *i2c;		
	struct snd_i2c_device *cs8427;	
	unsigned int cs8427_timeout;	

	struct ice1712_gpio {
		unsigned int direction;		
		unsigned int write_mask;	
		unsigned int saved[2];		
		
		void (*set_mask)(struct snd_ice1712 *ice, unsigned int data);
		unsigned int (*get_mask)(struct snd_ice1712 *ice);
		void (*set_dir)(struct snd_ice1712 *ice, unsigned int data);
		unsigned int (*get_dir)(struct snd_ice1712 *ice);
		void (*set_data)(struct snd_ice1712 *ice, unsigned int data);
		unsigned int (*get_data)(struct snd_ice1712 *ice);
		
		void (*set_pro_rate)(struct snd_ice1712 *ice, unsigned int rate);
		void (*i2s_mclk_changed)(struct snd_ice1712 *ice);
	} gpio;
	struct mutex gpio_mutex;

	
	void *spec;

	
	int pro_rate_default;
	int (*is_spdif_master)(struct snd_ice1712 *ice);
	unsigned int (*get_rate)(struct snd_ice1712 *ice);
	void (*set_rate)(struct snd_ice1712 *ice, unsigned int rate);
	unsigned char (*set_mclk)(struct snd_ice1712 *ice, unsigned int rate);
	int (*set_spdif_clock)(struct snd_ice1712 *ice, int type);
	int (*get_spdif_master_type)(struct snd_ice1712 *ice);
	char **ext_clock_names;
	int ext_clock_count;
	void (*pro_open)(struct snd_ice1712 *, struct snd_pcm_substream *);
#ifdef CONFIG_PM
	int (*pm_suspend)(struct snd_ice1712 *);
	int (*pm_resume)(struct snd_ice1712 *);
	unsigned int pm_suspend_enabled:1;
	unsigned int pm_saved_is_spdif_master:1;
	unsigned int pm_saved_spdif_ctrl;
	unsigned char pm_saved_spdif_cfg;
	unsigned int pm_saved_route;
#endif
};


static inline void snd_ice1712_gpio_set_dir(struct snd_ice1712 *ice, unsigned int bits)
{
	ice->gpio.set_dir(ice, bits);
}

static inline unsigned int snd_ice1712_gpio_get_dir(struct snd_ice1712 *ice)
{
	return ice->gpio.get_dir(ice);
}

static inline void snd_ice1712_gpio_set_mask(struct snd_ice1712 *ice, unsigned int bits)
{
	ice->gpio.set_mask(ice, bits);
}

static inline void snd_ice1712_gpio_write(struct snd_ice1712 *ice, unsigned int val)
{
	ice->gpio.set_data(ice, val);
}

static inline unsigned int snd_ice1712_gpio_read(struct snd_ice1712 *ice)
{
	return ice->gpio.get_data(ice);
}

static inline void snd_ice1712_save_gpio_status(struct snd_ice1712 *ice)
{
	mutex_lock(&ice->gpio_mutex);
	ice->gpio.saved[0] = ice->gpio.direction;
	ice->gpio.saved[1] = ice->gpio.write_mask;
}

static inline void snd_ice1712_restore_gpio_status(struct snd_ice1712 *ice)
{
	ice->gpio.set_dir(ice, ice->gpio.saved[0]);
	ice->gpio.set_mask(ice, ice->gpio.saved[1]);
	ice->gpio.direction = ice->gpio.saved[0];
	ice->gpio.write_mask = ice->gpio.saved[1];
	mutex_unlock(&ice->gpio_mutex);
}

#define ICE1712_GPIO(xiface, xname, xindex, mask, invert, xaccess) \
{ .iface = xiface, .name = xname, .access = xaccess, .info = snd_ctl_boolean_mono_info, \
  .get = snd_ice1712_gpio_get, .put = snd_ice1712_gpio_put, \
  .private_value = mask | (invert << 24) }

int snd_ice1712_gpio_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int snd_ice1712_gpio_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

static inline void snd_ice1712_gpio_write_bits(struct snd_ice1712 *ice,
					       unsigned int mask, unsigned int bits)
{
	unsigned val;

	ice->gpio.direction |= mask;
	snd_ice1712_gpio_set_dir(ice, ice->gpio.direction);
	val = snd_ice1712_gpio_read(ice);
	val &= ~mask;
	val |= mask & bits;
	snd_ice1712_gpio_write(ice, val);
}

static inline int snd_ice1712_gpio_read_bits(struct snd_ice1712 *ice,
					      unsigned int mask)
{
	ice->gpio.direction &= ~mask;
	snd_ice1712_gpio_set_dir(ice, ice->gpio.direction);
	return  snd_ice1712_gpio_read(ice) & mask;
}

int snd_ice1724_get_route_val(struct snd_ice1712 *ice, int shift);
int snd_ice1724_put_route_val(struct snd_ice1712 *ice, unsigned int val,
								int shift);

int snd_ice1712_spdif_build_controls(struct snd_ice1712 *ice);

int snd_ice1712_akm4xxx_init(struct snd_akm4xxx *ak,
			     const struct snd_akm4xxx *template,
			     const struct snd_ak4xxx_private *priv,
			     struct snd_ice1712 *ice);
void snd_ice1712_akm4xxx_free(struct snd_ice1712 *ice);
int snd_ice1712_akm4xxx_build_controls(struct snd_ice1712 *ice);

int snd_ice1712_init_cs8427(struct snd_ice1712 *ice, int addr);

static inline void snd_ice1712_write(struct snd_ice1712 *ice, u8 addr, u8 data)
{
	outb(addr, ICEREG(ice, INDEX));
	outb(data, ICEREG(ice, DATA));
}

static inline u8 snd_ice1712_read(struct snd_ice1712 *ice, u8 addr)
{
	outb(addr, ICEREG(ice, INDEX));
	return inb(ICEREG(ice, DATA));
}



struct snd_ice1712_card_info {
	unsigned int subvendor;
	char *name;
	char *model;
	char *driver;
	int (*chip_init)(struct snd_ice1712 *);
	int (*build_controls)(struct snd_ice1712 *);
	unsigned int no_mpu401:1;
	unsigned int mpu401_1_info_flags;
	unsigned int mpu401_2_info_flags;
	const char *mpu401_1_name;
	const char *mpu401_2_name;
	const unsigned int eeprom_size;
	const unsigned char *eeprom_data;
};


#endif 
