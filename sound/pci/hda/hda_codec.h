/*
 * Universal Interface for Intel High Definition Audio Codec
 *
 * Copyright (c) 2004 Takashi Iwai <tiwai@suse.de>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __SOUND_HDA_CODEC_H
#define __SOUND_HDA_CODEC_H

#include <sound/info.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/hwdep.h>

#define	AC_NODE_ROOT		0x00

enum {
	AC_GRP_AUDIO_FUNCTION = 0x01,
	AC_GRP_MODEM_FUNCTION = 0x02,
};
	
enum {
	AC_WID_AUD_OUT,		
	AC_WID_AUD_IN,		
	AC_WID_AUD_MIX,		
	AC_WID_AUD_SEL,		
	AC_WID_PIN,		
	AC_WID_POWER,		
	AC_WID_VOL_KNB,		
	AC_WID_BEEP,		
	AC_WID_VENDOR = 0x0f	
};

#define AC_VERB_GET_STREAM_FORMAT		0x0a00
#define AC_VERB_GET_AMP_GAIN_MUTE		0x0b00
#define AC_VERB_GET_PROC_COEF			0x0c00
#define AC_VERB_GET_COEF_INDEX			0x0d00
#define AC_VERB_PARAMETERS			0x0f00
#define AC_VERB_GET_CONNECT_SEL			0x0f01
#define AC_VERB_GET_CONNECT_LIST		0x0f02
#define AC_VERB_GET_PROC_STATE			0x0f03
#define AC_VERB_GET_SDI_SELECT			0x0f04
#define AC_VERB_GET_POWER_STATE			0x0f05
#define AC_VERB_GET_CONV			0x0f06
#define AC_VERB_GET_PIN_WIDGET_CONTROL		0x0f07
#define AC_VERB_GET_UNSOLICITED_RESPONSE	0x0f08
#define AC_VERB_GET_PIN_SENSE			0x0f09
#define AC_VERB_GET_BEEP_CONTROL		0x0f0a
#define AC_VERB_GET_EAPD_BTLENABLE		0x0f0c
#define AC_VERB_GET_DIGI_CONVERT_1		0x0f0d
#define AC_VERB_GET_DIGI_CONVERT_2		0x0f0e 
#define AC_VERB_GET_VOLUME_KNOB_CONTROL		0x0f0f
#define AC_VERB_GET_GPIO_DATA			0x0f15
#define AC_VERB_GET_GPIO_MASK			0x0f16
#define AC_VERB_GET_GPIO_DIRECTION		0x0f17
#define AC_VERB_GET_GPIO_WAKE_MASK		0x0f18
#define AC_VERB_GET_GPIO_UNSOLICITED_RSP_MASK	0x0f19
#define AC_VERB_GET_GPIO_STICKY_MASK		0x0f1a
#define AC_VERB_GET_CONFIG_DEFAULT		0x0f1c
#define AC_VERB_GET_SUBSYSTEM_ID		0x0f20
#define AC_VERB_GET_CVT_CHAN_COUNT		0x0f2d
#define AC_VERB_GET_HDMI_DIP_SIZE		0x0f2e
#define AC_VERB_GET_HDMI_ELDD			0x0f2f
#define AC_VERB_GET_HDMI_DIP_INDEX		0x0f30
#define AC_VERB_GET_HDMI_DIP_DATA		0x0f31
#define AC_VERB_GET_HDMI_DIP_XMIT		0x0f32
#define AC_VERB_GET_HDMI_CP_CTRL		0x0f33
#define AC_VERB_GET_HDMI_CHAN_SLOT		0x0f34

#define AC_VERB_SET_STREAM_FORMAT		0x200
#define AC_VERB_SET_AMP_GAIN_MUTE		0x300
#define AC_VERB_SET_PROC_COEF			0x400
#define AC_VERB_SET_COEF_INDEX			0x500
#define AC_VERB_SET_CONNECT_SEL			0x701
#define AC_VERB_SET_PROC_STATE			0x703
#define AC_VERB_SET_SDI_SELECT			0x704
#define AC_VERB_SET_POWER_STATE			0x705
#define AC_VERB_SET_CHANNEL_STREAMID		0x706
#define AC_VERB_SET_PIN_WIDGET_CONTROL		0x707
#define AC_VERB_SET_UNSOLICITED_ENABLE		0x708
#define AC_VERB_SET_PIN_SENSE			0x709
#define AC_VERB_SET_BEEP_CONTROL		0x70a
#define AC_VERB_SET_EAPD_BTLENABLE		0x70c
#define AC_VERB_SET_DIGI_CONVERT_1		0x70d
#define AC_VERB_SET_DIGI_CONVERT_2		0x70e
#define AC_VERB_SET_VOLUME_KNOB_CONTROL		0x70f
#define AC_VERB_SET_GPIO_DATA			0x715
#define AC_VERB_SET_GPIO_MASK			0x716
#define AC_VERB_SET_GPIO_DIRECTION		0x717
#define AC_VERB_SET_GPIO_WAKE_MASK		0x718
#define AC_VERB_SET_GPIO_UNSOLICITED_RSP_MASK	0x719
#define AC_VERB_SET_GPIO_STICKY_MASK		0x71a
#define AC_VERB_SET_CONFIG_DEFAULT_BYTES_0	0x71c
#define AC_VERB_SET_CONFIG_DEFAULT_BYTES_1	0x71d
#define AC_VERB_SET_CONFIG_DEFAULT_BYTES_2	0x71e
#define AC_VERB_SET_CONFIG_DEFAULT_BYTES_3	0x71f
#define AC_VERB_SET_EAPD				0x788
#define AC_VERB_SET_CODEC_RESET			0x7ff
#define AC_VERB_SET_CVT_CHAN_COUNT		0x72d
#define AC_VERB_SET_HDMI_DIP_INDEX		0x730
#define AC_VERB_SET_HDMI_DIP_DATA		0x731
#define AC_VERB_SET_HDMI_DIP_XMIT		0x732
#define AC_VERB_SET_HDMI_CP_CTRL		0x733
#define AC_VERB_SET_HDMI_CHAN_SLOT		0x734

#define AC_PAR_VENDOR_ID		0x00
#define AC_PAR_SUBSYSTEM_ID		0x01
#define AC_PAR_REV_ID			0x02
#define AC_PAR_NODE_COUNT		0x04
#define AC_PAR_FUNCTION_TYPE		0x05
#define AC_PAR_AUDIO_FG_CAP		0x08
#define AC_PAR_AUDIO_WIDGET_CAP		0x09
#define AC_PAR_PCM			0x0a
#define AC_PAR_STREAM			0x0b
#define AC_PAR_PIN_CAP			0x0c
#define AC_PAR_AMP_IN_CAP		0x0d
#define AC_PAR_CONNLIST_LEN		0x0e
#define AC_PAR_POWER_STATE		0x0f
#define AC_PAR_PROC_CAP			0x10
#define AC_PAR_GPIO_CAP			0x11
#define AC_PAR_AMP_OUT_CAP		0x12
#define AC_PAR_VOL_KNB_CAP		0x13
#define AC_PAR_HDMI_LPCM_CAP		0x20


#define AC_FGT_TYPE			(0xff<<0)
#define AC_FGT_TYPE_SHIFT		0
#define AC_FGT_UNSOL_CAP		(1<<8)

#define AC_AFG_OUT_DELAY		(0xf<<0)
#define AC_AFG_IN_DELAY			(0xf<<8)
#define AC_AFG_BEEP_GEN			(1<<16)

#define AC_WCAP_STEREO			(1<<0)	
#define AC_WCAP_IN_AMP			(1<<1)	
#define AC_WCAP_OUT_AMP			(1<<2)	
#define AC_WCAP_AMP_OVRD		(1<<3)	
#define AC_WCAP_FORMAT_OVRD		(1<<4)	
#define AC_WCAP_STRIPE			(1<<5)	
#define AC_WCAP_PROC_WID		(1<<6)	
#define AC_WCAP_UNSOL_CAP		(1<<7)	
#define AC_WCAP_CONN_LIST		(1<<8)	
#define AC_WCAP_DIGITAL			(1<<9)	
#define AC_WCAP_POWER			(1<<10)	
#define AC_WCAP_LR_SWAP			(1<<11)	
#define AC_WCAP_CP_CAPS			(1<<12) 
#define AC_WCAP_CHAN_CNT_EXT		(7<<13)	
#define AC_WCAP_DELAY			(0xf<<16)
#define AC_WCAP_DELAY_SHIFT		16
#define AC_WCAP_TYPE			(0xf<<20)
#define AC_WCAP_TYPE_SHIFT		20

#define AC_SUPPCM_RATES			(0xfff << 0)
#define AC_SUPPCM_BITS_8		(1<<16)
#define AC_SUPPCM_BITS_16		(1<<17)
#define AC_SUPPCM_BITS_20		(1<<18)
#define AC_SUPPCM_BITS_24		(1<<19)
#define AC_SUPPCM_BITS_32		(1<<20)

#define AC_SUPFMT_PCM			(1<<0)
#define AC_SUPFMT_FLOAT32		(1<<1)
#define AC_SUPFMT_AC3			(1<<2)

#define AC_GPIO_IO_COUNT		(0xff<<0)
#define AC_GPIO_O_COUNT			(0xff<<8)
#define AC_GPIO_O_COUNT_SHIFT		8
#define AC_GPIO_I_COUNT			(0xff<<16)
#define AC_GPIO_I_COUNT_SHIFT		16
#define AC_GPIO_UNSOLICITED		(1<<30)
#define AC_GPIO_WAKE			(1<<31)

#define AC_CONV_CHANNEL			(0xf<<0)
#define AC_CONV_STREAM			(0xf<<4)
#define AC_CONV_STREAM_SHIFT		4

#define AC_SDI_SELECT			(0xf<<0)

#define AC_FMT_CHAN_SHIFT		0
#define AC_FMT_CHAN_MASK		(0x0f << 0)
#define AC_FMT_BITS_SHIFT		4
#define AC_FMT_BITS_MASK		(7 << 4)
#define AC_FMT_BITS_8			(0 << 4)
#define AC_FMT_BITS_16			(1 << 4)
#define AC_FMT_BITS_20			(2 << 4)
#define AC_FMT_BITS_24			(3 << 4)
#define AC_FMT_BITS_32			(4 << 4)
#define AC_FMT_DIV_SHIFT		8
#define AC_FMT_DIV_MASK			(7 << 8)
#define AC_FMT_MULT_SHIFT		11
#define AC_FMT_MULT_MASK		(7 << 11)
#define AC_FMT_BASE_SHIFT		14
#define AC_FMT_BASE_48K			(0 << 14)
#define AC_FMT_BASE_44K			(1 << 14)
#define AC_FMT_TYPE_SHIFT		15
#define AC_FMT_TYPE_PCM			(0 << 15)
#define AC_FMT_TYPE_NON_PCM		(1 << 15)

#define AC_UNSOL_TAG			(0x3f<<0)
#define AC_UNSOL_ENABLED		(1<<7)
#define AC_USRSP_EN			AC_UNSOL_ENABLED

#define AC_UNSOL_RES_TAG		(0x3f<<26)
#define AC_UNSOL_RES_TAG_SHIFT		26
#define AC_UNSOL_RES_SUBTAG		(0x1f<<21)
#define AC_UNSOL_RES_SUBTAG_SHIFT	21
#define AC_UNSOL_RES_ELDV		(1<<1)	
#define AC_UNSOL_RES_PD			(1<<0)	
#define AC_UNSOL_RES_CP_STATE		(1<<1)	
#define AC_UNSOL_RES_CP_READY		(1<<0)	

#define AC_PINCAP_IMP_SENSE		(1<<0)	
#define AC_PINCAP_TRIG_REQ		(1<<1)	
#define AC_PINCAP_PRES_DETECT		(1<<2)	
#define AC_PINCAP_HP_DRV		(1<<3)	
#define AC_PINCAP_OUT			(1<<4)	
#define AC_PINCAP_IN			(1<<5)	
#define AC_PINCAP_BALANCE		(1<<6)	
#define AC_PINCAP_LR_SWAP		(1<<7)	
#define AC_PINCAP_HDMI			(1<<7)	
#define AC_PINCAP_DP			(1<<24)	
#define AC_PINCAP_VREF			(0x37<<8)
#define AC_PINCAP_VREF_SHIFT		8
#define AC_PINCAP_EAPD			(1<<16)	
#define AC_PINCAP_HBR			(1<<27)	
#define AC_PINCAP_VREF_HIZ		(1<<0)	
#define AC_PINCAP_VREF_50		(1<<1)	
#define AC_PINCAP_VREF_GRD		(1<<2)	
#define AC_PINCAP_VREF_80		(1<<4)	
#define AC_PINCAP_VREF_100		(1<<5)	

#define AC_AMPCAP_OFFSET		(0x7f<<0)  
#define AC_AMPCAP_OFFSET_SHIFT		0
#define AC_AMPCAP_NUM_STEPS		(0x7f<<8)  
#define AC_AMPCAP_NUM_STEPS_SHIFT	8
#define AC_AMPCAP_STEP_SIZE		(0x7f<<16) 
#define AC_AMPCAP_STEP_SIZE_SHIFT	16
#define AC_AMPCAP_MUTE			(1<<31)    
#define AC_AMPCAP_MUTE_SHIFT		31

#define AC_AMPCAP_MIN_MUTE		(1 << 30) 

#define AC_CLIST_LENGTH			(0x7f<<0)
#define AC_CLIST_LONG			(1<<7)

#define AC_PWRST_D0SUP			(1<<0)
#define AC_PWRST_D1SUP			(1<<1)
#define AC_PWRST_D2SUP			(1<<2)
#define AC_PWRST_D3SUP			(1<<3)
#define AC_PWRST_D3COLDSUP		(1<<4)
#define AC_PWRST_S3D3COLDSUP		(1<<29)
#define AC_PWRST_CLKSTOP		(1<<30)
#define AC_PWRST_EPSS			(1U<<31)

#define AC_PWRST_SETTING		(0xf<<0)
#define AC_PWRST_ACTUAL			(0xf<<4)
#define AC_PWRST_ACTUAL_SHIFT		4
#define AC_PWRST_D0			0x00
#define AC_PWRST_D1			0x01
#define AC_PWRST_D2			0x02
#define AC_PWRST_D3			0x03

#define AC_PCAP_BENIGN			(1<<0)
#define AC_PCAP_NUM_COEF		(0xff<<8)
#define AC_PCAP_NUM_COEF_SHIFT		8

#define AC_KNBCAP_NUM_STEPS		(0x7f<<0)
#define AC_KNBCAP_DELTA			(1<<7)

#define AC_LPCMCAP_48K_CP_CHNS		(0x0f<<0) 	
#define AC_LPCMCAP_48K_NO_CHNS		(0x0f<<4) 
#define AC_LPCMCAP_48K_20BIT		(1<<8)	
#define AC_LPCMCAP_48K_24BIT		(1<<9)	
#define AC_LPCMCAP_96K_CP_CHNS		(0x0f<<10) 	
#define AC_LPCMCAP_96K_NO_CHNS		(0x0f<<14) 
#define AC_LPCMCAP_96K_20BIT		(1<<18)	
#define AC_LPCMCAP_96K_24BIT		(1<<19)	
#define AC_LPCMCAP_192K_CP_CHNS		(0x0f<<20) 	
#define AC_LPCMCAP_192K_NO_CHNS		(0x0f<<24) 
#define AC_LPCMCAP_192K_20BIT		(1<<28)	
#define AC_LPCMCAP_192K_24BIT		(1<<29)	
#define AC_LPCMCAP_44K			(1<<30)	
#define AC_LPCMCAP_44K_MS		(1<<31)	


#define AC_AMP_MUTE			(1<<7)
#define AC_AMP_GAIN			(0x7f)
#define AC_AMP_GET_INDEX		(0xf<<0)

#define AC_AMP_GET_LEFT			(1<<13)
#define AC_AMP_GET_RIGHT		(0<<13)
#define AC_AMP_GET_OUTPUT		(1<<15)
#define AC_AMP_GET_INPUT		(0<<15)

#define AC_AMP_SET_INDEX		(0xf<<8)
#define AC_AMP_SET_INDEX_SHIFT		8
#define AC_AMP_SET_RIGHT		(1<<12)
#define AC_AMP_SET_LEFT			(1<<13)
#define AC_AMP_SET_INPUT		(1<<14)
#define AC_AMP_SET_OUTPUT		(1<<15)

#define AC_DIG1_ENABLE			(1<<0)
#define AC_DIG1_V			(1<<1)
#define AC_DIG1_VCFG			(1<<2)
#define AC_DIG1_EMPHASIS		(1<<3)
#define AC_DIG1_COPYRIGHT		(1<<4)
#define AC_DIG1_NONAUDIO		(1<<5)
#define AC_DIG1_PROFESSIONAL		(1<<6)
#define AC_DIG1_LEVEL			(1<<7)

#define AC_DIG2_CC			(0x7f<<0)

#define AC_PINCTL_EPT			(0x3<<0)
#define AC_PINCTL_EPT_NATIVE		0
#define AC_PINCTL_EPT_HBR		3
#define AC_PINCTL_VREFEN		(0x7<<0)
#define AC_PINCTL_VREF_HIZ		0	
#define AC_PINCTL_VREF_50		1	
#define AC_PINCTL_VREF_GRD		2	
#define AC_PINCTL_VREF_80		4	
#define AC_PINCTL_VREF_100		5	
#define AC_PINCTL_IN_EN			(1<<5)
#define AC_PINCTL_OUT_EN		(1<<6)
#define AC_PINCTL_HP_EN			(1<<7)

#define AC_PINSENSE_IMPEDANCE_MASK	(0x7fffffff)
#define AC_PINSENSE_PRESENCE		(1<<31)
#define AC_PINSENSE_ELDV		(1<<30)	

#define AC_EAPDBTL_BALANCED		(1<<0)
#define AC_EAPDBTL_EAPD			(1<<1)
#define AC_EAPDBTL_LR_SWAP		(1<<2)

#define AC_ELDD_ELD_VALID		(1<<31)
#define AC_ELDD_ELD_DATA		0xff

#define AC_DIPSIZE_ELD_BUF		(1<<3) 
#define AC_DIPSIZE_PACK_IDX		(0x07<<0) 

#define AC_DIPIDX_PACK_IDX		(0x07<<5) 
#define AC_DIPIDX_BYTE_IDX		(0x1f<<0) 

#define AC_DIPXMIT_MASK			(0x3<<6)
#define AC_DIPXMIT_DISABLE		(0x0<<6) 
#define AC_DIPXMIT_ONCE			(0x2<<6) 
#define AC_DIPXMIT_BEST			(0x3<<6) 

#define AC_CPCTRL_CES			(1<<9) 
#define AC_CPCTRL_READY			(1<<8) 
#define AC_CPCTRL_SUBTAG		(0x1f<<3) 
#define AC_CPCTRL_STATE			(3<<0) 

#define AC_CVTMAP_HDMI_SLOT		(0xf<<0) 
#define AC_CVTMAP_CHAN			(0xf<<4) 

#define AC_DEFCFG_SEQUENCE		(0xf<<0)
#define AC_DEFCFG_DEF_ASSOC		(0xf<<4)
#define AC_DEFCFG_ASSOC_SHIFT		4
#define AC_DEFCFG_MISC			(0xf<<8)
#define AC_DEFCFG_MISC_SHIFT		8
#define AC_DEFCFG_MISC_NO_PRESENCE	(1<<0)
#define AC_DEFCFG_COLOR			(0xf<<12)
#define AC_DEFCFG_COLOR_SHIFT		12
#define AC_DEFCFG_CONN_TYPE		(0xf<<16)
#define AC_DEFCFG_CONN_TYPE_SHIFT	16
#define AC_DEFCFG_DEVICE		(0xf<<20)
#define AC_DEFCFG_DEVICE_SHIFT		20
#define AC_DEFCFG_LOCATION		(0x3f<<24)
#define AC_DEFCFG_LOCATION_SHIFT	24
#define AC_DEFCFG_PORT_CONN		(0x3<<30)
#define AC_DEFCFG_PORT_CONN_SHIFT	30

enum {
	AC_JACK_LINE_OUT,
	AC_JACK_SPEAKER,
	AC_JACK_HP_OUT,
	AC_JACK_CD,
	AC_JACK_SPDIF_OUT,
	AC_JACK_DIG_OTHER_OUT,
	AC_JACK_MODEM_LINE_SIDE,
	AC_JACK_MODEM_HAND_SIDE,
	AC_JACK_LINE_IN,
	AC_JACK_AUX,
	AC_JACK_MIC_IN,
	AC_JACK_TELEPHONY,
	AC_JACK_SPDIF_IN,
	AC_JACK_DIG_OTHER_IN,
	AC_JACK_OTHER = 0xf,
};

enum {
	AC_JACK_CONN_UNKNOWN,
	AC_JACK_CONN_1_8,
	AC_JACK_CONN_1_4,
	AC_JACK_CONN_ATAPI,
	AC_JACK_CONN_RCA,
	AC_JACK_CONN_OPTICAL,
	AC_JACK_CONN_OTHER_DIGITAL,
	AC_JACK_CONN_OTHER_ANALOG,
	AC_JACK_CONN_DIN,
	AC_JACK_CONN_XLR,
	AC_JACK_CONN_RJ11,
	AC_JACK_CONN_COMB,
	AC_JACK_CONN_OTHER = 0xf,
};

enum {
	AC_JACK_COLOR_UNKNOWN,
	AC_JACK_COLOR_BLACK,
	AC_JACK_COLOR_GREY,
	AC_JACK_COLOR_BLUE,
	AC_JACK_COLOR_GREEN,
	AC_JACK_COLOR_RED,
	AC_JACK_COLOR_ORANGE,
	AC_JACK_COLOR_YELLOW,
	AC_JACK_COLOR_PURPLE,
	AC_JACK_COLOR_PINK,
	AC_JACK_COLOR_WHITE = 0xe,
	AC_JACK_COLOR_OTHER,
};

enum {
	AC_JACK_LOC_NONE,
	AC_JACK_LOC_REAR,
	AC_JACK_LOC_FRONT,
	AC_JACK_LOC_LEFT,
	AC_JACK_LOC_RIGHT,
	AC_JACK_LOC_TOP,
	AC_JACK_LOC_BOTTOM,
};
enum {
	AC_JACK_LOC_EXTERNAL = 0x00,
	AC_JACK_LOC_INTERNAL = 0x10,
	AC_JACK_LOC_SEPARATE = 0x20,
	AC_JACK_LOC_OTHER    = 0x30,
};
enum {
	
	AC_JACK_LOC_REAR_PANEL = 0x07,
	AC_JACK_LOC_DRIVE_BAY,
	
	AC_JACK_LOC_RISER = 0x17,
	AC_JACK_LOC_HDMI,
	AC_JACK_LOC_ATAPI,
	
	AC_JACK_LOC_MOBILE_IN = 0x37,
	AC_JACK_LOC_MOBILE_OUT,
};

enum {
	AC_JACK_PORT_COMPLEX,
	AC_JACK_PORT_NONE,
	AC_JACK_PORT_FIXED,
	AC_JACK_PORT_BOTH,
};

#define HDA_MAX_CONNECTIONS	32

#define HDA_MAX_CODEC_ADDRESS	0x0f

struct snd_array {
	unsigned int used;
	unsigned int alloced;
	unsigned int elem_size;
	unsigned int alloc_align;
	void *list;
};

void *snd_array_new(struct snd_array *array);
void snd_array_free(struct snd_array *array);
static inline void snd_array_init(struct snd_array *array, unsigned int size,
				  unsigned int align)
{
	array->elem_size = size;
	array->alloc_align = align;
}

static inline void *snd_array_elem(struct snd_array *array, unsigned int idx)
{
	return array->list + idx * array->elem_size;
}

static inline unsigned int snd_array_index(struct snd_array *array, void *ptr)
{
	return (unsigned long)(ptr - array->list) / array->elem_size;
}


struct hda_bus;
struct hda_beep;
struct hda_codec;
struct hda_pcm;
struct hda_pcm_stream;
struct hda_bus_unsolicited;

typedef u16 hda_nid_t;

struct hda_bus_ops {
	
	int (*command)(struct hda_bus *bus, unsigned int cmd);
	
	unsigned int (*get_response)(struct hda_bus *bus, unsigned int addr);
	
	void (*private_free)(struct hda_bus *);
	
	int (*attach_pcm)(struct hda_bus *bus, struct hda_codec *codec,
			  struct hda_pcm *pcm);
	
	void (*bus_reset)(struct hda_bus *bus);
#ifdef CONFIG_SND_HDA_POWER_SAVE
	
	void (*pm_notify)(struct hda_bus *bus);
#endif
};

struct hda_bus_template {
	void *private_data;
	struct pci_dev *pci;
	const char *modelname;
	int *power_save;
	struct hda_bus_ops ops;
};

struct hda_bus {
	struct snd_card *card;

	
	void *private_data;
	struct pci_dev *pci;
	const char *modelname;
	int *power_save;
	struct hda_bus_ops ops;

	
	struct list_head codec_list;
	
	struct hda_codec *caddr_tbl[HDA_MAX_CODEC_ADDRESS + 1];

	struct mutex cmd_mutex;
	struct mutex prepare_mutex;

	
	struct hda_bus_unsolicited *unsol;
	char workq_name[16];
	struct workqueue_struct *workq;	

	
	DECLARE_BITMAP(pcm_dev_bits, SNDRV_PCM_DEVICES);

	
	unsigned int needs_damn_long_delay :1;
	unsigned int allow_bus_reset:1;	
	unsigned int sync_write:1;	
	
	unsigned int shutdown :1;	
	unsigned int rirb_error:1;	
	unsigned int response_reset:1;	
	unsigned int in_reset:1;	
	unsigned int power_keep_link_on:1; 
};

struct hda_codec_preset {
	unsigned int id;
	unsigned int mask;
	unsigned int subs;
	unsigned int subs_mask;
	unsigned int rev;
	hda_nid_t afg, mfg;
	const char *name;
	int (*patch)(struct hda_codec *codec);
};
	
struct hda_codec_preset_list {
	const struct hda_codec_preset *preset;
	struct module *owner;
	struct list_head list;
};

int snd_hda_add_codec_preset(struct hda_codec_preset_list *preset);
int snd_hda_delete_codec_preset(struct hda_codec_preset_list *preset);

struct hda_codec_ops {
	int (*build_controls)(struct hda_codec *codec);
	int (*build_pcms)(struct hda_codec *codec);
	int (*init)(struct hda_codec *codec);
	void (*free)(struct hda_codec *codec);
	void (*unsol_event)(struct hda_codec *codec, unsigned int res);
	void (*set_power_state)(struct hda_codec *codec, hda_nid_t fg,
				unsigned int power_state);
#ifdef CONFIG_PM
	int (*suspend)(struct hda_codec *codec, pm_message_t state);
	int (*post_suspend)(struct hda_codec *codec);
	int (*pre_resume)(struct hda_codec *codec);
	int (*resume)(struct hda_codec *codec);
#endif
#ifdef CONFIG_SND_HDA_POWER_SAVE
	int (*check_power_status)(struct hda_codec *codec, hda_nid_t nid);
#endif
	void (*reboot_notify)(struct hda_codec *codec);
};

struct hda_cache_head {
	u32 key;		
	u16 val;		
	u16 next;		
};

struct hda_amp_info {
	struct hda_cache_head head;
	u32 amp_caps;		
	u16 vol[2];		
};

struct hda_cache_rec {
	u16 hash[64];			
	struct snd_array buf;		
};

struct hda_pcm_ops {
	int (*open)(struct hda_pcm_stream *info, struct hda_codec *codec,
		    struct snd_pcm_substream *substream);
	int (*close)(struct hda_pcm_stream *info, struct hda_codec *codec,
		     struct snd_pcm_substream *substream);
	int (*prepare)(struct hda_pcm_stream *info, struct hda_codec *codec,
		       unsigned int stream_tag, unsigned int format,
		       struct snd_pcm_substream *substream);
	int (*cleanup)(struct hda_pcm_stream *info, struct hda_codec *codec,
		       struct snd_pcm_substream *substream);
};

struct hda_pcm_stream {
	unsigned int substreams;	
	unsigned int channels_min;	
	unsigned int channels_max;	
	hda_nid_t nid;	
	u32 rates;	
	u64 formats;	
	unsigned int maxbps;	
	struct hda_pcm_ops ops;
};

enum {
	HDA_PCM_TYPE_AUDIO,
	HDA_PCM_TYPE_SPDIF,
	HDA_PCM_TYPE_HDMI,
	HDA_PCM_TYPE_MODEM,
	HDA_PCM_NTYPES
};

struct hda_pcm {
	char *name;
	struct hda_pcm_stream stream[2];
	unsigned int pcm_type;	
	int device;		
	struct snd_pcm *pcm;	
};

struct hda_codec {
	struct hda_bus *bus;
	unsigned int addr;	
	struct list_head list;	

	hda_nid_t afg;	
	hda_nid_t mfg;	

	
	u8 afg_function_id;
	u8 mfg_function_id;
	u8 afg_unsol;
	u8 mfg_unsol;
	u32 vendor_id;
	u32 subsystem_id;
	u32 revision_id;

	
	const struct hda_codec_preset *preset;
	struct module *owner;
	const char *vendor_name;	
	const char *chip_name;		
	const char *modelname;	

	
	struct hda_codec_ops patch_ops;

	
	unsigned int num_pcms;
	struct hda_pcm *pcm_info;

	
	void *spec;

	
	struct hda_beep *beep;
	unsigned int beep_mode;

	
	unsigned int num_nodes;
	hda_nid_t start_nid;
	u32 *wcaps;

	struct snd_array mixers;	
	struct snd_array nids;		

	struct hda_cache_rec amp_cache;	
	struct hda_cache_rec cmd_cache;	

	struct snd_array conn_lists;	

	struct mutex spdif_mutex;
	struct mutex control_mutex;
	struct snd_array spdif_out;
	unsigned int spdif_in_enable;	
	const hda_nid_t *slave_dig_outs; 
	struct snd_array init_pins;	
	struct snd_array driver_pins;	
	struct snd_array cvt_setups;	

#ifdef CONFIG_SND_HDA_HWDEP
	struct snd_hwdep *hwdep;	
	struct snd_array init_verbs;	
	struct snd_array hints;		
	struct snd_array user_pins;	
#endif

	
	unsigned int spdif_status_reset :1; 
	unsigned int pin_amp_workaround:1; 
	unsigned int single_adc_amp:1; 
	unsigned int no_sticky_stream:1; 
	unsigned int pins_shutup:1;	
	unsigned int no_trigger_sense:1; 
	unsigned int ignore_misc_bit:1; 
	unsigned int no_jack_detect:1;	
#ifdef CONFIG_SND_HDA_POWER_SAVE
	unsigned int power_on :1;	
	unsigned int power_transition :1; 
	int power_count;	
	struct delayed_work power_work; 
	unsigned long power_on_acct;
	unsigned long power_off_acct;
	unsigned long power_jiffies;
#endif

	
	void (*proc_widget_hook)(struct snd_info_buffer *buffer,
				 struct hda_codec *codec, hda_nid_t nid);

	
	struct snd_array jacktbl;

#ifdef CONFIG_SND_HDA_INPUT_JACK
	
	struct snd_array jacks;
#endif
};

enum {
	HDA_INPUT, HDA_OUTPUT
};


int snd_hda_bus_new(struct snd_card *card, const struct hda_bus_template *temp,
		    struct hda_bus **busp);
int snd_hda_codec_new(struct hda_bus *bus, unsigned int codec_addr,
		      struct hda_codec **codecp);
int snd_hda_codec_configure(struct hda_codec *codec);

unsigned int snd_hda_codec_read(struct hda_codec *codec, hda_nid_t nid,
				int direct,
				unsigned int verb, unsigned int parm);
int snd_hda_codec_write(struct hda_codec *codec, hda_nid_t nid, int direct,
			unsigned int verb, unsigned int parm);
#define snd_hda_param_read(codec, nid, param) \
	snd_hda_codec_read(codec, nid, 0, AC_VERB_PARAMETERS, param)
int snd_hda_get_sub_nodes(struct hda_codec *codec, hda_nid_t nid,
			  hda_nid_t *start_id);
int snd_hda_get_connections(struct hda_codec *codec, hda_nid_t nid,
			    hda_nid_t *conn_list, int max_conns);
int snd_hda_get_raw_connections(struct hda_codec *codec, hda_nid_t nid,
			    hda_nid_t *conn_list, int max_conns);
int snd_hda_get_conn_list(struct hda_codec *codec, hda_nid_t nid,
			  const hda_nid_t **listp);
int snd_hda_override_conn_list(struct hda_codec *codec, hda_nid_t nid, int nums,
			  const hda_nid_t *list);
int snd_hda_get_conn_index(struct hda_codec *codec, hda_nid_t mux,
			   hda_nid_t nid, int recursive);
int snd_hda_query_supported_pcm(struct hda_codec *codec, hda_nid_t nid,
				u32 *ratesp, u64 *formatsp, unsigned int *bpsp);

struct hda_verb {
	hda_nid_t nid;
	u32 verb;
	u32 param;
};

void snd_hda_sequence_write(struct hda_codec *codec,
			    const struct hda_verb *seq);

int snd_hda_queue_unsol_event(struct hda_bus *bus, u32 res, u32 res_ex);

#ifdef CONFIG_PM
int snd_hda_codec_write_cache(struct hda_codec *codec, hda_nid_t nid,
			      int direct, unsigned int verb, unsigned int parm);
void snd_hda_sequence_write_cache(struct hda_codec *codec,
				  const struct hda_verb *seq);
int snd_hda_codec_update_cache(struct hda_codec *codec, hda_nid_t nid,
			      int direct, unsigned int verb, unsigned int parm);
void snd_hda_codec_resume_cache(struct hda_codec *codec);
#else
#define snd_hda_codec_write_cache	snd_hda_codec_write
#define snd_hda_codec_update_cache	snd_hda_codec_write
#define snd_hda_sequence_write_cache	snd_hda_sequence_write
#endif

struct hda_pincfg {
	hda_nid_t nid;
	unsigned char ctrl;	
	unsigned char pad;	
	unsigned int cfg;	
};

unsigned int snd_hda_codec_get_pincfg(struct hda_codec *codec, hda_nid_t nid);
int snd_hda_codec_set_pincfg(struct hda_codec *codec, hda_nid_t nid,
			     unsigned int cfg);
int snd_hda_add_pincfg(struct hda_codec *codec, struct snd_array *list,
		       hda_nid_t nid, unsigned int cfg); 
void snd_hda_shutup_pins(struct hda_codec *codec);

struct hda_spdif_out {
	hda_nid_t nid;		
	unsigned int status;	
	unsigned short ctls;	
};
struct hda_spdif_out *snd_hda_spdif_out_of_nid(struct hda_codec *codec,
					       hda_nid_t nid);
void snd_hda_spdif_ctls_unassign(struct hda_codec *codec, int idx);
void snd_hda_spdif_ctls_assign(struct hda_codec *codec, int idx, hda_nid_t nid);

int snd_hda_build_controls(struct hda_bus *bus);
int snd_hda_codec_build_controls(struct hda_codec *codec);

int snd_hda_build_pcms(struct hda_bus *bus);
int snd_hda_codec_build_pcms(struct hda_codec *codec);

int snd_hda_codec_prepare(struct hda_codec *codec,
			  struct hda_pcm_stream *hinfo,
			  unsigned int stream,
			  unsigned int format,
			  struct snd_pcm_substream *substream);
void snd_hda_codec_cleanup(struct hda_codec *codec,
			   struct hda_pcm_stream *hinfo,
			   struct snd_pcm_substream *substream);

void snd_hda_codec_setup_stream(struct hda_codec *codec, hda_nid_t nid,
				u32 stream_tag,
				int channel_id, int format);
void __snd_hda_codec_cleanup_stream(struct hda_codec *codec, hda_nid_t nid,
				    int do_now);
#define snd_hda_codec_cleanup_stream(codec, nid) \
	__snd_hda_codec_cleanup_stream(codec, nid, 0)
unsigned int snd_hda_calc_stream_format(unsigned int rate,
					unsigned int channels,
					unsigned int format,
					unsigned int maxbps,
					unsigned short spdif_ctls);
int snd_hda_is_supported_format(struct hda_codec *codec, hda_nid_t nid,
				unsigned int format);

void snd_hda_get_codec_name(struct hda_codec *codec, char *name, int namelen);
void snd_hda_bus_reboot_notify(struct hda_bus *bus);
void snd_hda_codec_set_power_to_all(struct hda_codec *codec, hda_nid_t fg,
				    unsigned int power_state,
				    bool eapd_workaround);

#ifdef CONFIG_PM
int snd_hda_suspend(struct hda_bus *bus);
int snd_hda_resume(struct hda_bus *bus);
#endif

static inline
int hda_call_check_power_status(struct hda_codec *codec, hda_nid_t nid)
{
#ifdef CONFIG_SND_HDA_POWER_SAVE
	if (codec->patch_ops.check_power_status)
		return codec->patch_ops.check_power_status(codec, nid);
#endif
	return 0;
}

const char *snd_hda_get_jack_connectivity(u32 cfg);
const char *snd_hda_get_jack_type(u32 cfg);
const char *snd_hda_get_jack_location(u32 cfg);

#ifdef CONFIG_SND_HDA_POWER_SAVE
void snd_hda_power_up(struct hda_codec *codec);
void snd_hda_power_down(struct hda_codec *codec);
#define snd_hda_codec_needs_resume(codec) codec->power_count
void snd_hda_update_power_acct(struct hda_codec *codec);
#else
static inline void snd_hda_power_up(struct hda_codec *codec) {}
static inline void snd_hda_power_down(struct hda_codec *codec) {}
#define snd_hda_codec_needs_resume(codec) 1
#endif

#ifdef CONFIG_SND_HDA_PATCH_LOADER
int snd_hda_load_patch(struct hda_bus *bus, const char *patch);
#endif


#ifdef MODULE
#define EXPORT_SYMBOL_HDA(sym) EXPORT_SYMBOL_GPL(sym)
#else
#define EXPORT_SYMBOL_HDA(sym)
#endif

#endif 
