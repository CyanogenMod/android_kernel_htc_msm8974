/*
 *  Driver for Ensoniq ES1370/ES1371 AudioPCI soundcard
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>,
 *		     Thomas Sailer <sailer@ife.ee.ethz.ch>
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


#include <asm/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/gameport.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/rawmidi.h>
#ifdef CHIP1371
#include <sound/ac97_codec.h>
#else
#include <sound/ak4531_codec.h>
#endif
#include <sound/initval.h>
#include <sound/asoundef.h>

#ifndef CHIP1371
#undef CHIP1370
#define CHIP1370
#endif

#ifdef CHIP1370
#define DRIVER_NAME "ENS1370"
#else
#define DRIVER_NAME "ENS1371"
#endif


MODULE_AUTHOR("Jaroslav Kysela <perex@perex.cz>, Thomas Sailer <sailer@ife.ee.ethz.ch>");
MODULE_LICENSE("GPL");
#ifdef CHIP1370
MODULE_DESCRIPTION("Ensoniq AudioPCI ES1370");
MODULE_SUPPORTED_DEVICE("{{Ensoniq,AudioPCI-97 ES1370},"
	        "{Creative Labs,SB PCI64/128 (ES1370)}}");
#endif
#ifdef CHIP1371
MODULE_DESCRIPTION("Ensoniq/Creative AudioPCI ES1371+");
MODULE_SUPPORTED_DEVICE("{{Ensoniq,AudioPCI ES1371/73},"
		"{Ensoniq,AudioPCI ES1373},"
		"{Creative Labs,Ectiva EV1938},"
		"{Creative Labs,SB PCI64/128 (ES1371/73)},"
		"{Creative Labs,Vibra PCI128},"
		"{Ectiva,EV1938}}");
#endif

#if defined(CONFIG_GAMEPORT) || (defined(MODULE) && defined(CONFIG_GAMEPORT_MODULE))
#define SUPPORT_JOYSTICK
#endif

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;	
#ifdef SUPPORT_JOYSTICK
#ifdef CHIP1371
static int joystick_port[SNDRV_CARDS];
#else
static bool joystick[SNDRV_CARDS];
#endif
#endif
#ifdef CHIP1371
static int spdif[SNDRV_CARDS];
static int lineio[SNDRV_CARDS];
#endif

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for Ensoniq AudioPCI soundcard.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for Ensoniq AudioPCI soundcard.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable Ensoniq AudioPCI soundcard.");
#ifdef SUPPORT_JOYSTICK
#ifdef CHIP1371
module_param_array(joystick_port, int, NULL, 0444);
MODULE_PARM_DESC(joystick_port, "Joystick port address.");
#else
module_param_array(joystick, bool, NULL, 0444);
MODULE_PARM_DESC(joystick, "Enable joystick.");
#endif
#endif 
#ifdef CHIP1371
module_param_array(spdif, int, NULL, 0444);
MODULE_PARM_DESC(spdif, "S/PDIF output (-1 = none, 0 = auto, 1 = force).");
module_param_array(lineio, int, NULL, 0444);
MODULE_PARM_DESC(lineio, "Line In to Rear Out (0 = auto, 1 = force).");
#endif

#define ES1371REV_ES1373_A  0x04
#define ES1371REV_ES1373_B  0x06
#define ES1371REV_CT5880_A  0x07
#define CT5880REV_CT5880_C  0x02
#define CT5880REV_CT5880_D  0x03	
#define CT5880REV_CT5880_E  0x04	
#define ES1371REV_ES1371_B  0x09
#define EV1938REV_EV1938_A  0x00
#define ES1371REV_ES1373_8  0x08


#define ES_REG(ensoniq, x) ((ensoniq)->port + ES_REG_##x)

#define ES_REG_CONTROL	0x00	
#define   ES_1370_ADC_STOP	(1<<31)		
#define   ES_1370_XCTL1 	(1<<30)		
#define   ES_1373_BYPASS_P1	(1<<31)		
#define   ES_1373_BYPASS_P2	(1<<30)		
#define   ES_1373_BYPASS_R	(1<<29)		
#define   ES_1373_TEST_BIT	(1<<28)		
#define   ES_1373_RECEN_B	(1<<27)		
#define   ES_1373_SPDIF_THRU	(1<<26)		
#define   ES_1371_JOY_ASEL(o)	(((o)&0x03)<<24)
#define   ES_1371_JOY_ASELM	(0x03<<24)	
#define   ES_1371_JOY_ASELI(i)  (((i)>>24)&0x03)
#define   ES_1371_GPIO_IN(i)	(((i)>>20)&0x0f)
#define   ES_1370_PCLKDIVO(o)	(((o)&0x1fff)<<16)
#define   ES_1370_PCLKDIVM	((0x1fff)<<16)	
#define   ES_1370_PCLKDIVI(i)	(((i)>>16)&0x1fff)
#define   ES_1371_GPIO_OUT(o)	(((o)&0x0f)<<16)
#define   ES_1371_GPIO_OUTM     (0x0f<<16)	
#define   ES_MSFMTSEL		(1<<15)		
#define   ES_1370_M_SBB		(1<<14)		
#define   ES_1371_SYNC_RES	(1<<14)		
#define   ES_1370_WTSRSEL(o)	(((o)&0x03)<<12)
#define   ES_1370_WTSRSELM	(0x03<<12)	
#define   ES_1371_ADC_STOP	(1<<13)		
#define   ES_1371_PWR_INTRM	(1<<12)		
#define   ES_1370_DAC_SYNC	(1<<11)		
#define   ES_1371_M_CB		(1<<11)		
#define   ES_CCB_INTRM		(1<<10)		
#define   ES_1370_M_CB		(1<<9)		
#define   ES_1370_XCTL0		(1<<8)		
#define   ES_1371_PDLEV(o)	(((o)&0x03)<<8)	
#define   ES_1371_PDLEVM	(0x03<<8)	
#define   ES_BREQ		(1<<7)		
#define   ES_DAC1_EN		(1<<6)		
#define   ES_DAC2_EN		(1<<5)		
#define   ES_ADC_EN		(1<<4)		
#define   ES_UART_EN		(1<<3)		
#define   ES_JYSTK_EN		(1<<2)		
#define   ES_1370_CDC_EN	(1<<1)		
#define   ES_1371_XTALCKDIS	(1<<1)		
#define   ES_1370_SERR_DISABLE	(1<<0)		
#define   ES_1371_PCICLKDIS     (1<<0)		
#define ES_REG_STATUS	0x04	
#define   ES_INTR               (1<<31)		
#define   ES_1371_ST_AC97_RST	(1<<29)		
#define   ES_1373_REAR_BIT27	(1<<27)		
#define   ES_1373_REAR_BIT26	(1<<26)
#define   ES_1373_REAR_BIT24	(1<<24)
#define   ES_1373_GPIO_INT_EN(o)(((o)&0x0f)<<20)
#define   ES_1373_SPDIF_EN	(1<<18)		
#define   ES_1373_SPDIF_TEST	(1<<17)		
#define   ES_1371_TEST          (1<<16)		
#define   ES_1373_GPIO_INT(i)	(((i)&0x0f)>>12)
#define   ES_1370_CSTAT		(1<<10)		
#define   ES_1370_CBUSY         (1<<9)		
#define   ES_1370_CWRIP		(1<<8)		
#define   ES_1371_SYNC_ERR	(1<<8)		
#define   ES_1371_VC(i)         (((i)>>6)&0x03)	
#define   ES_1370_VC(i)		(((i)>>5)&0x03)	
#define   ES_1371_MPWR          (1<<5)		
#define   ES_MCCB		(1<<4)		
#define   ES_UART		(1<<3)		
#define   ES_DAC1		(1<<2)		
#define   ES_DAC2		(1<<1)		
#define   ES_ADC		(1<<0)		
#define ES_REG_UART_DATA 0x08	
#define ES_REG_UART_STATUS 0x09	
#define   ES_RXINT		(1<<7)		
#define   ES_TXINT		(1<<2)		
#define   ES_TXRDY		(1<<1)		
#define   ES_RXRDY		(1<<0)		
#define ES_REG_UART_CONTROL 0x09	
#define   ES_RXINTEN		(1<<7)		
#define   ES_TXINTENO(o)	(((o)&0x03)<<5)	
#define   ES_TXINTENM		(0x03<<5)	
#define   ES_TXINTENI(i)	(((i)>>5)&0x03)
#define   ES_CNTRL(o)		(((o)&0x03)<<0)	
#define   ES_CNTRLM		(0x03<<0)	
#define ES_REG_UART_RES	0x0a	
#define   ES_TEST_MODE		(1<<0)		
#define ES_REG_MEM_PAGE	0x0c	
#define   ES_MEM_PAGEO(o)	(((o)&0x0f)<<0)	
#define   ES_MEM_PAGEM		(0x0f<<0)	
#define   ES_MEM_PAGEI(i)	(((i)>>0)&0x0f) 
#define ES_REG_1370_CODEC 0x10	
#define   ES_1370_CODEC_WRITE(a,d) ((((a)&0xff)<<8)|(((d)&0xff)<<0))
#define ES_REG_1371_CODEC 0x14	
#define   ES_1371_CODEC_RDY	   (1<<31)	
#define   ES_1371_CODEC_WIP	   (1<<30)	
#define   EV_1938_CODEC_MAGIC	   (1<<26)
#define   ES_1371_CODEC_PIRD	   (1<<23)	
#define   ES_1371_CODEC_WRITE(a,d) ((((a)&0x7f)<<16)|(((d)&0xffff)<<0))
#define   ES_1371_CODEC_READS(a)   ((((a)&0x7f)<<16)|ES_1371_CODEC_PIRD)
#define   ES_1371_CODEC_READ(i)    (((i)>>0)&0xffff)

#define ES_REG_1371_SMPRATE 0x10	
#define   ES_1371_SRC_RAM_ADDRO(o) (((o)&0x7f)<<25)
#define   ES_1371_SRC_RAM_ADDRM	   (0x7f<<25)	
#define   ES_1371_SRC_RAM_ADDRI(i) (((i)>>25)&0x7f)
#define   ES_1371_SRC_RAM_WE	   (1<<24)	
#define   ES_1371_SRC_RAM_BUSY     (1<<23)	
#define   ES_1371_SRC_DISABLE      (1<<22)	
#define   ES_1371_DIS_P1	   (1<<21)	
#define   ES_1371_DIS_P2	   (1<<20)	
#define   ES_1371_DIS_R1	   (1<<19)	
#define   ES_1371_SRC_RAM_DATAO(o) (((o)&0xffff)<<0)
#define   ES_1371_SRC_RAM_DATAM	   (0xffff<<0)	
#define   ES_1371_SRC_RAM_DATAI(i) (((i)>>0)&0xffff)

#define ES_REG_1371_LEGACY 0x18	
#define   ES_1371_JFAST		(1<<31)		
#define   ES_1371_HIB		(1<<30)		
#define   ES_1371_VSB		(1<<29)		
#define   ES_1371_VMPUO(o)	(((o)&0x03)<<27)
#define   ES_1371_VMPUM		(0x03<<27)	
#define   ES_1371_VMPUI(i)	(((i)>>27)&0x03)
#define   ES_1371_VCDCO(o)	(((o)&0x03)<<25)
#define   ES_1371_VCDCM		(0x03<<25)	
#define   ES_1371_VCDCI(i)	(((i)>>25)&0x03)
#define   ES_1371_FIRQ		(1<<24)		
#define   ES_1371_SDMACAP	(1<<23)		
#define   ES_1371_SPICAP	(1<<22)		
#define   ES_1371_MDMACAP	(1<<21)		
#define   ES_1371_MPICAP	(1<<20)		
#define   ES_1371_ADCAP		(1<<19)		
#define   ES_1371_SVCAP		(1<<18)		
#define   ES_1371_CDCCAP	(1<<17)		
#define   ES_1371_BACAP		(1<<16)		
#define   ES_1371_EXI(i)	(((i)>>8)&0x07)	
#define   ES_1371_AI(i)		(((i)>>3)&0x1f)	
#define   ES_1371_WR		(1<<2)	
#define   ES_1371_LEGINT	(1<<0)	

#define ES_REG_CHANNEL_STATUS 0x1c 

#define ES_REG_SERIAL	0x20	
#define   ES_1371_DAC_TEST	(1<<22)		
#define   ES_P2_END_INCO(o)	(((o)&0x07)<<19)
#define   ES_P2_END_INCM	(0x07<<19)	
#define   ES_P2_END_INCI(i)	(((i)>>16)&0x07)
#define   ES_P2_ST_INCO(o)	(((o)&0x07)<<16)
#define   ES_P2_ST_INCM		(0x07<<16)	
#define   ES_P2_ST_INCI(i)	(((i)<<16)&0x07)
#define   ES_R1_LOOP_SEL	(1<<15)		
#define   ES_P2_LOOP_SEL	(1<<14)		
#define   ES_P1_LOOP_SEL	(1<<13)		
#define   ES_P2_PAUSE		(1<<12)		
#define   ES_P1_PAUSE		(1<<11)		
#define   ES_R1_INT_EN		(1<<10)		
#define   ES_P2_INT_EN		(1<<9)		
#define   ES_P1_INT_EN		(1<<8)		
#define   ES_P1_SCT_RLD		(1<<7)		
#define   ES_P2_DAC_SEN		(1<<6)		
#define   ES_R1_MODEO(o)	(((o)&0x03)<<4)	
#define   ES_R1_MODEM		(0x03<<4)	
#define   ES_R1_MODEI(i)	(((i)>>4)&0x03)
#define   ES_P2_MODEO(o)	(((o)&0x03)<<2)	
#define   ES_P2_MODEM		(0x03<<2)	
#define   ES_P2_MODEI(i)	(((i)>>2)&0x03)
#define   ES_P1_MODEO(o)	(((o)&0x03)<<0)	
#define   ES_P1_MODEM		(0x03<<0)	
#define   ES_P1_MODEI(i)	(((i)>>0)&0x03)

#define ES_REG_DAC1_COUNT 0x24	
#define ES_REG_DAC2_COUNT 0x28	
#define ES_REG_ADC_COUNT  0x2c	
#define   ES_REG_CURR_COUNT(i)  (((i)>>16)&0xffff)
#define   ES_REG_COUNTO(o)	(((o)&0xffff)<<0)
#define   ES_REG_COUNTM		(0xffff<<0)
#define   ES_REG_COUNTI(i)	(((i)>>0)&0xffff)

#define ES_REG_DAC1_FRAME 0x30	
#define ES_REG_DAC1_SIZE  0x34	
#define ES_REG_DAC2_FRAME 0x38	
#define ES_REG_DAC2_SIZE  0x3c	
#define ES_REG_ADC_FRAME  0x30	
#define ES_REG_ADC_SIZE	  0x34	
#define   ES_REG_FCURR_COUNTO(o) (((o)&0xffff)<<16)
#define   ES_REG_FCURR_COUNTM    (0xffff<<16)
#define   ES_REG_FCURR_COUNTI(i) (((i)>>14)&0x3fffc)
#define   ES_REG_FSIZEO(o)	 (((o)&0xffff)<<0)
#define   ES_REG_FSIZEM		 (0xffff<<0)
#define   ES_REG_FSIZEI(i)	 (((i)>>0)&0xffff)
#define ES_REG_PHANTOM_FRAME 0x38 
#define ES_REG_PHANTOM_COUNT 0x3c 

#define ES_REG_UART_FIFO  0x30	
#define   ES_REG_UF_VALID	 (1<<8)
#define   ES_REG_UF_BYTEO(o)	 (((o)&0xff)<<0)
#define   ES_REG_UF_BYTEM	 (0xff<<0)
#define   ES_REG_UF_BYTEI(i)	 (((i)>>0)&0xff)



#define ES_PAGE_DAC	0x0c
#define ES_PAGE_ADC	0x0d
#define ES_PAGE_UART	0x0e
#define ES_PAGE_UART1	0x0f


#define ES_SMPREG_DAC1		0x70
#define ES_SMPREG_DAC2		0x74
#define ES_SMPREG_ADC		0x78
#define ES_SMPREG_VOL_ADC	0x6c
#define ES_SMPREG_VOL_DAC1	0x7c
#define ES_SMPREG_VOL_DAC2	0x7e
#define ES_SMPREG_TRUNC_N	0x00
#define ES_SMPREG_INT_REGS	0x01
#define ES_SMPREG_ACCUM_FRAC	0x02
#define ES_SMPREG_VFREQ_FRAC	0x03


#define ES_1370_SRCLOCK	   1411200
#define ES_1370_SRTODIV(x) (ES_1370_SRCLOCK/(x)-2)


#define ES_MODE_PLAY1	0x0001
#define ES_MODE_PLAY2	0x0002
#define ES_MODE_CAPTURE	0x0004

#define ES_MODE_OUTPUT	0x0001	
#define ES_MODE_INPUT	0x0002	


struct ensoniq {
	spinlock_t reg_lock;
	struct mutex src_mutex;

	int irq;

	unsigned long playback1size;
	unsigned long playback2size;
	unsigned long capture3size;

	unsigned long port;
	unsigned int mode;
	unsigned int uartm;	

	unsigned int ctrl;	
	unsigned int sctrl;	
	unsigned int cssr;	
	unsigned int uartc;	
	unsigned int rev;	

	union {
#ifdef CHIP1371
		struct {
			struct snd_ac97 *ac97;
		} es1371;
#else
		struct {
			int pclkdiv_lock;
			struct snd_ak4531 *ak4531;
		} es1370;
#endif
	} u;

	struct pci_dev *pci;
	struct snd_card *card;
	struct snd_pcm *pcm1;	
	struct snd_pcm *pcm2;	
	struct snd_pcm_substream *playback1_substream;
	struct snd_pcm_substream *playback2_substream;
	struct snd_pcm_substream *capture_substream;
	unsigned int p1_dma_size;
	unsigned int p2_dma_size;
	unsigned int c_dma_size;
	unsigned int p1_period_size;
	unsigned int p2_period_size;
	unsigned int c_period_size;
	struct snd_rawmidi *rmidi;
	struct snd_rawmidi_substream *midi_input;
	struct snd_rawmidi_substream *midi_output;

	unsigned int spdif;
	unsigned int spdif_default;
	unsigned int spdif_stream;

#ifdef CHIP1370
	struct snd_dma_buffer dma_bug;
#endif

#ifdef SUPPORT_JOYSTICK
	struct gameport *gameport;
#endif
};

static irqreturn_t snd_audiopci_interrupt(int irq, void *dev_id);

static DEFINE_PCI_DEVICE_TABLE(snd_audiopci_ids) = {
#ifdef CHIP1370
	{ PCI_VDEVICE(ENSONIQ, 0x5000), 0, },	
#endif
#ifdef CHIP1371
	{ PCI_VDEVICE(ENSONIQ, 0x1371), 0, },	
	{ PCI_VDEVICE(ENSONIQ, 0x5880), 0, },	
	{ PCI_VDEVICE(ECTIVA, 0x8938), 0, },	
#endif
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, snd_audiopci_ids);


#define POLL_COUNT	0xa000

#ifdef CHIP1370
static unsigned int snd_es1370_fixed_rates[] =
	{5512, 11025, 22050, 44100};
static struct snd_pcm_hw_constraint_list snd_es1370_hw_constraints_rates = {
	.count = 4, 
	.list = snd_es1370_fixed_rates,
	.mask = 0,
};
static struct snd_ratnum es1370_clock = {
	.num = ES_1370_SRCLOCK,
	.den_min = 29, 
	.den_max = 353,
	.den_step = 1,
};
static struct snd_pcm_hw_constraint_ratnums snd_es1370_hw_constraints_clock = {
	.nrats = 1,
	.rats = &es1370_clock,
};
#else
static struct snd_ratden es1371_dac_clock = {
	.num_min = 3000 * (1 << 15),
	.num_max = 48000 * (1 << 15),
	.num_step = 3000,
	.den = 1 << 15,
};
static struct snd_pcm_hw_constraint_ratdens snd_es1371_hw_constraints_dac_clock = {
	.nrats = 1,
	.rats = &es1371_dac_clock,
};
static struct snd_ratnum es1371_adc_clock = {
	.num = 48000 << 15,
	.den_min = 32768, 
	.den_max = 393216,
	.den_step = 1,
};
static struct snd_pcm_hw_constraint_ratnums snd_es1371_hw_constraints_adc_clock = {
	.nrats = 1,
	.rats = &es1371_adc_clock,
};
#endif
static const unsigned int snd_ensoniq_sample_shift[] =
	{0, 1, 1, 2};


#ifdef CHIP1371

static unsigned int snd_es1371_wait_src_ready(struct ensoniq * ensoniq)
{
	unsigned int t, r = 0;

	for (t = 0; t < POLL_COUNT; t++) {
		r = inl(ES_REG(ensoniq, 1371_SMPRATE));
		if ((r & ES_1371_SRC_RAM_BUSY) == 0)
			return r;
		cond_resched();
	}
	snd_printk(KERN_ERR "wait src ready timeout 0x%lx [0x%x]\n",
		   ES_REG(ensoniq, 1371_SMPRATE), r);
	return 0;
}

static unsigned int snd_es1371_src_read(struct ensoniq * ensoniq, unsigned short reg)
{
	unsigned int temp, i, orig, r;

	
	temp = orig = snd_es1371_wait_src_ready(ensoniq);

	
	r = temp & (ES_1371_SRC_DISABLE | ES_1371_DIS_P1 |
		    ES_1371_DIS_P2 | ES_1371_DIS_R1);
	r |= ES_1371_SRC_RAM_ADDRO(reg) | 0x10000;
	outl(r, ES_REG(ensoniq, 1371_SMPRATE));

	
	temp = snd_es1371_wait_src_ready(ensoniq);
	
	if ((temp & 0x00870000) != 0x00010000) {
		
		for (i = 0; i < POLL_COUNT; i++) {
			temp = inl(ES_REG(ensoniq, 1371_SMPRATE));
			if ((temp & 0x00870000) == 0x00010000)
				break;
		}
	}

		
	r = orig & (ES_1371_SRC_DISABLE | ES_1371_DIS_P1 |
		   ES_1371_DIS_P2 | ES_1371_DIS_R1);
	r |= ES_1371_SRC_RAM_ADDRO(reg);
	outl(r, ES_REG(ensoniq, 1371_SMPRATE));
	
	return temp;
}

static void snd_es1371_src_write(struct ensoniq * ensoniq,
				 unsigned short reg, unsigned short data)
{
	unsigned int r;

	r = snd_es1371_wait_src_ready(ensoniq) &
	    (ES_1371_SRC_DISABLE | ES_1371_DIS_P1 |
	     ES_1371_DIS_P2 | ES_1371_DIS_R1);
	r |= ES_1371_SRC_RAM_ADDRO(reg) | ES_1371_SRC_RAM_DATAO(data);
	outl(r | ES_1371_SRC_RAM_WE, ES_REG(ensoniq, 1371_SMPRATE));
}

#endif 

#ifdef CHIP1370

static void snd_es1370_codec_write(struct snd_ak4531 *ak4531,
				   unsigned short reg, unsigned short val)
{
	struct ensoniq *ensoniq = ak4531->private_data;
	unsigned long end_time = jiffies + HZ / 10;

#if 0
	printk(KERN_DEBUG
	       "CODEC WRITE: reg = 0x%x, val = 0x%x (0x%x), creg = 0x%x\n",
	       reg, val, ES_1370_CODEC_WRITE(reg, val), ES_REG(ensoniq, 1370_CODEC));
#endif
	do {
		if (!(inl(ES_REG(ensoniq, STATUS)) & ES_1370_CSTAT)) {
			outw(ES_1370_CODEC_WRITE(reg, val), ES_REG(ensoniq, 1370_CODEC));
			return;
		}
		schedule_timeout_uninterruptible(1);
	} while (time_after(end_time, jiffies));
	snd_printk(KERN_ERR "codec write timeout, status = 0x%x\n",
		   inl(ES_REG(ensoniq, STATUS)));
}

#endif 

#ifdef CHIP1371

static inline bool is_ev1938(struct ensoniq *ensoniq)
{
	return ensoniq->pci->device == 0x8938;
}

static void snd_es1371_codec_write(struct snd_ac97 *ac97,
				   unsigned short reg, unsigned short val)
{
	struct ensoniq *ensoniq = ac97->private_data;
	unsigned int t, x, flag;

	flag = is_ev1938(ensoniq) ? EV_1938_CODEC_MAGIC : 0;
	mutex_lock(&ensoniq->src_mutex);
	for (t = 0; t < POLL_COUNT; t++) {
		if (!(inl(ES_REG(ensoniq, 1371_CODEC)) & ES_1371_CODEC_WIP)) {
			
			x = snd_es1371_wait_src_ready(ensoniq);
			outl((x & (ES_1371_SRC_DISABLE | ES_1371_DIS_P1 |
			           ES_1371_DIS_P2 | ES_1371_DIS_R1)) | 0x00010000,
			     ES_REG(ensoniq, 1371_SMPRATE));
			for (t = 0; t < POLL_COUNT; t++) {
				if ((inl(ES_REG(ensoniq, 1371_SMPRATE)) & 0x00870000) ==
				    0x00000000)
					break;
			}
			
			for (t = 0; t < POLL_COUNT; t++) {
				if ((inl(ES_REG(ensoniq, 1371_SMPRATE)) & 0x00870000) ==
				    0x00010000)
					break;
			}
			outl(ES_1371_CODEC_WRITE(reg, val) | flag,
			     ES_REG(ensoniq, 1371_CODEC));
			
			snd_es1371_wait_src_ready(ensoniq);
			outl(x, ES_REG(ensoniq, 1371_SMPRATE));
			mutex_unlock(&ensoniq->src_mutex);
			return;
		}
	}
	mutex_unlock(&ensoniq->src_mutex);
	snd_printk(KERN_ERR "codec write timeout at 0x%lx [0x%x]\n",
		   ES_REG(ensoniq, 1371_CODEC), inl(ES_REG(ensoniq, 1371_CODEC)));
}

static unsigned short snd_es1371_codec_read(struct snd_ac97 *ac97,
					    unsigned short reg)
{
	struct ensoniq *ensoniq = ac97->private_data;
	unsigned int t, x, flag, fail = 0;

	flag = is_ev1938(ensoniq) ? EV_1938_CODEC_MAGIC : 0;
      __again:
	mutex_lock(&ensoniq->src_mutex);
	for (t = 0; t < POLL_COUNT; t++) {
		if (!(inl(ES_REG(ensoniq, 1371_CODEC)) & ES_1371_CODEC_WIP)) {
			
			x = snd_es1371_wait_src_ready(ensoniq);
			outl((x & (ES_1371_SRC_DISABLE | ES_1371_DIS_P1 |
			           ES_1371_DIS_P2 | ES_1371_DIS_R1)) | 0x00010000,
			     ES_REG(ensoniq, 1371_SMPRATE));
			for (t = 0; t < POLL_COUNT; t++) {
				if ((inl(ES_REG(ensoniq, 1371_SMPRATE)) & 0x00870000) ==
				    0x00000000)
					break;
			}
			
			for (t = 0; t < POLL_COUNT; t++) {
				if ((inl(ES_REG(ensoniq, 1371_SMPRATE)) & 0x00870000) ==
				    0x00010000)
					break;
			}
			outl(ES_1371_CODEC_READS(reg) | flag,
			     ES_REG(ensoniq, 1371_CODEC));
			
			snd_es1371_wait_src_ready(ensoniq);
			outl(x, ES_REG(ensoniq, 1371_SMPRATE));
			
			for (t = 0; t < POLL_COUNT; t++) {
				if (!(inl(ES_REG(ensoniq, 1371_CODEC)) & ES_1371_CODEC_WIP))
					break;		
			}
			
			for (t = 0; t < POLL_COUNT; t++) {
				if ((x = inl(ES_REG(ensoniq, 1371_CODEC))) & ES_1371_CODEC_RDY) {
					if (is_ev1938(ensoniq)) {
						for (t = 0; t < 100; t++)
							inl(ES_REG(ensoniq, CONTROL));
						x = inl(ES_REG(ensoniq, 1371_CODEC));
					}
					mutex_unlock(&ensoniq->src_mutex);
					return ES_1371_CODEC_READ(x);
				}
			}
			mutex_unlock(&ensoniq->src_mutex);
			if (++fail > 10) {
				snd_printk(KERN_ERR "codec read timeout (final) "
					   "at 0x%lx, reg = 0x%x [0x%x]\n",
					   ES_REG(ensoniq, 1371_CODEC), reg,
					   inl(ES_REG(ensoniq, 1371_CODEC)));
				return 0;
			}
			goto __again;
		}
	}
	mutex_unlock(&ensoniq->src_mutex);
	snd_printk(KERN_ERR "es1371: codec read timeout at 0x%lx [0x%x]\n",
		   ES_REG(ensoniq, 1371_CODEC), inl(ES_REG(ensoniq, 1371_CODEC)));
	return 0;
}

static void snd_es1371_codec_wait(struct snd_ac97 *ac97)
{
	msleep(750);
	snd_es1371_codec_read(ac97, AC97_RESET);
	snd_es1371_codec_read(ac97, AC97_VENDOR_ID1);
	snd_es1371_codec_read(ac97, AC97_VENDOR_ID2);
	msleep(50);
}

static void snd_es1371_adc_rate(struct ensoniq * ensoniq, unsigned int rate)
{
	unsigned int n, truncm, freq, result;

	mutex_lock(&ensoniq->src_mutex);
	n = rate / 3000;
	if ((1 << n) & ((1 << 15) | (1 << 13) | (1 << 11) | (1 << 9)))
		n--;
	truncm = (21 * n - 1) | 1;
	freq = ((48000UL << 15) / rate) * n;
	result = (48000UL << 15) / (freq / n);
	if (rate >= 24000) {
		if (truncm > 239)
			truncm = 239;
		snd_es1371_src_write(ensoniq, ES_SMPREG_ADC + ES_SMPREG_TRUNC_N,
				(((239 - truncm) >> 1) << 9) | (n << 4));
	} else {
		if (truncm > 119)
			truncm = 119;
		snd_es1371_src_write(ensoniq, ES_SMPREG_ADC + ES_SMPREG_TRUNC_N,
				0x8000 | (((119 - truncm) >> 1) << 9) | (n << 4));
	}
	snd_es1371_src_write(ensoniq, ES_SMPREG_ADC + ES_SMPREG_INT_REGS,
			     (snd_es1371_src_read(ensoniq, ES_SMPREG_ADC +
						  ES_SMPREG_INT_REGS) & 0x00ff) |
			     ((freq >> 5) & 0xfc00));
	snd_es1371_src_write(ensoniq, ES_SMPREG_ADC + ES_SMPREG_VFREQ_FRAC, freq & 0x7fff);
	snd_es1371_src_write(ensoniq, ES_SMPREG_VOL_ADC, n << 8);
	snd_es1371_src_write(ensoniq, ES_SMPREG_VOL_ADC + 1, n << 8);
	mutex_unlock(&ensoniq->src_mutex);
}

static void snd_es1371_dac1_rate(struct ensoniq * ensoniq, unsigned int rate)
{
	unsigned int freq, r;

	mutex_lock(&ensoniq->src_mutex);
	freq = ((rate << 15) + 1500) / 3000;
	r = (snd_es1371_wait_src_ready(ensoniq) & (ES_1371_SRC_DISABLE |
						   ES_1371_DIS_P2 | ES_1371_DIS_R1)) |
		ES_1371_DIS_P1;
	outl(r, ES_REG(ensoniq, 1371_SMPRATE));
	snd_es1371_src_write(ensoniq, ES_SMPREG_DAC1 + ES_SMPREG_INT_REGS,
			     (snd_es1371_src_read(ensoniq, ES_SMPREG_DAC1 +
						  ES_SMPREG_INT_REGS) & 0x00ff) |
			     ((freq >> 5) & 0xfc00));
	snd_es1371_src_write(ensoniq, ES_SMPREG_DAC1 + ES_SMPREG_VFREQ_FRAC, freq & 0x7fff);
	r = (snd_es1371_wait_src_ready(ensoniq) & (ES_1371_SRC_DISABLE |
						   ES_1371_DIS_P2 | ES_1371_DIS_R1));
	outl(r, ES_REG(ensoniq, 1371_SMPRATE));
	mutex_unlock(&ensoniq->src_mutex);
}

static void snd_es1371_dac2_rate(struct ensoniq * ensoniq, unsigned int rate)
{
	unsigned int freq, r;

	mutex_lock(&ensoniq->src_mutex);
	freq = ((rate << 15) + 1500) / 3000;
	r = (snd_es1371_wait_src_ready(ensoniq) & (ES_1371_SRC_DISABLE |
						   ES_1371_DIS_P1 | ES_1371_DIS_R1)) |
		ES_1371_DIS_P2;
	outl(r, ES_REG(ensoniq, 1371_SMPRATE));
	snd_es1371_src_write(ensoniq, ES_SMPREG_DAC2 + ES_SMPREG_INT_REGS,
			     (snd_es1371_src_read(ensoniq, ES_SMPREG_DAC2 +
						  ES_SMPREG_INT_REGS) & 0x00ff) |
			     ((freq >> 5) & 0xfc00));
	snd_es1371_src_write(ensoniq, ES_SMPREG_DAC2 + ES_SMPREG_VFREQ_FRAC,
			     freq & 0x7fff);
	r = (snd_es1371_wait_src_ready(ensoniq) & (ES_1371_SRC_DISABLE |
						   ES_1371_DIS_P1 | ES_1371_DIS_R1));
	outl(r, ES_REG(ensoniq, 1371_SMPRATE));
	mutex_unlock(&ensoniq->src_mutex);
}

#endif 

static int snd_ensoniq_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	{
		unsigned int what = 0;
		struct snd_pcm_substream *s;
		snd_pcm_group_for_each_entry(s, substream) {
			if (s == ensoniq->playback1_substream) {
				what |= ES_P1_PAUSE;
				snd_pcm_trigger_done(s, substream);
			} else if (s == ensoniq->playback2_substream) {
				what |= ES_P2_PAUSE;
				snd_pcm_trigger_done(s, substream);
			} else if (s == ensoniq->capture_substream)
				return -EINVAL;
		}
		spin_lock(&ensoniq->reg_lock);
		if (cmd == SNDRV_PCM_TRIGGER_PAUSE_PUSH)
			ensoniq->sctrl |= what;
		else
			ensoniq->sctrl &= ~what;
		outl(ensoniq->sctrl, ES_REG(ensoniq, SERIAL));
		spin_unlock(&ensoniq->reg_lock);
		break;
	}
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_STOP:
	{
		unsigned int what = 0;
		struct snd_pcm_substream *s;
		snd_pcm_group_for_each_entry(s, substream) {
			if (s == ensoniq->playback1_substream) {
				what |= ES_DAC1_EN;
				snd_pcm_trigger_done(s, substream);
			} else if (s == ensoniq->playback2_substream) {
				what |= ES_DAC2_EN;
				snd_pcm_trigger_done(s, substream);
			} else if (s == ensoniq->capture_substream) {
				what |= ES_ADC_EN;
				snd_pcm_trigger_done(s, substream);
			}
		}
		spin_lock(&ensoniq->reg_lock);
		if (cmd == SNDRV_PCM_TRIGGER_START)
			ensoniq->ctrl |= what;
		else
			ensoniq->ctrl &= ~what;
		outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
		spin_unlock(&ensoniq->reg_lock);
		break;
	}
	default:
		return -EINVAL;
	}
	return 0;
}


static int snd_ensoniq_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
}

static int snd_ensoniq_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int snd_ensoniq_playback1_prepare(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int mode = 0;

	ensoniq->p1_dma_size = snd_pcm_lib_buffer_bytes(substream);
	ensoniq->p1_period_size = snd_pcm_lib_period_bytes(substream);
	if (snd_pcm_format_width(runtime->format) == 16)
		mode |= 0x02;
	if (runtime->channels > 1)
		mode |= 0x01;
	spin_lock_irq(&ensoniq->reg_lock);
	ensoniq->ctrl &= ~ES_DAC1_EN;
#ifdef CHIP1371
	
	if (runtime->rate == 48000)
		ensoniq->ctrl |= ES_1373_BYPASS_P1;
	else
		ensoniq->ctrl &= ~ES_1373_BYPASS_P1;
#endif
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	outl(ES_MEM_PAGEO(ES_PAGE_DAC), ES_REG(ensoniq, MEM_PAGE));
	outl(runtime->dma_addr, ES_REG(ensoniq, DAC1_FRAME));
	outl((ensoniq->p1_dma_size >> 2) - 1, ES_REG(ensoniq, DAC1_SIZE));
	ensoniq->sctrl &= ~(ES_P1_LOOP_SEL | ES_P1_PAUSE | ES_P1_SCT_RLD | ES_P1_MODEM);
	ensoniq->sctrl |= ES_P1_INT_EN | ES_P1_MODEO(mode);
	outl(ensoniq->sctrl, ES_REG(ensoniq, SERIAL));
	outl((ensoniq->p1_period_size >> snd_ensoniq_sample_shift[mode]) - 1,
	     ES_REG(ensoniq, DAC1_COUNT));
#ifdef CHIP1370
	ensoniq->ctrl &= ~ES_1370_WTSRSELM;
	switch (runtime->rate) {
	case 5512: ensoniq->ctrl |= ES_1370_WTSRSEL(0); break;
	case 11025: ensoniq->ctrl |= ES_1370_WTSRSEL(1); break;
	case 22050: ensoniq->ctrl |= ES_1370_WTSRSEL(2); break;
	case 44100: ensoniq->ctrl |= ES_1370_WTSRSEL(3); break;
	default: snd_BUG();
	}
#endif
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	spin_unlock_irq(&ensoniq->reg_lock);
#ifndef CHIP1370
	snd_es1371_dac1_rate(ensoniq, runtime->rate);
#endif
	return 0;
}

static int snd_ensoniq_playback2_prepare(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int mode = 0;

	ensoniq->p2_dma_size = snd_pcm_lib_buffer_bytes(substream);
	ensoniq->p2_period_size = snd_pcm_lib_period_bytes(substream);
	if (snd_pcm_format_width(runtime->format) == 16)
		mode |= 0x02;
	if (runtime->channels > 1)
		mode |= 0x01;
	spin_lock_irq(&ensoniq->reg_lock);
	ensoniq->ctrl &= ~ES_DAC2_EN;
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	outl(ES_MEM_PAGEO(ES_PAGE_DAC), ES_REG(ensoniq, MEM_PAGE));
	outl(runtime->dma_addr, ES_REG(ensoniq, DAC2_FRAME));
	outl((ensoniq->p2_dma_size >> 2) - 1, ES_REG(ensoniq, DAC2_SIZE));
	ensoniq->sctrl &= ~(ES_P2_LOOP_SEL | ES_P2_PAUSE | ES_P2_DAC_SEN |
			    ES_P2_END_INCM | ES_P2_ST_INCM | ES_P2_MODEM);
	ensoniq->sctrl |= ES_P2_INT_EN | ES_P2_MODEO(mode) |
			  ES_P2_END_INCO(mode & 2 ? 2 : 1) | ES_P2_ST_INCO(0);
	outl(ensoniq->sctrl, ES_REG(ensoniq, SERIAL));
	outl((ensoniq->p2_period_size >> snd_ensoniq_sample_shift[mode]) - 1,
	     ES_REG(ensoniq, DAC2_COUNT));
#ifdef CHIP1370
	if (!(ensoniq->u.es1370.pclkdiv_lock & ES_MODE_CAPTURE)) {
		ensoniq->ctrl &= ~ES_1370_PCLKDIVM;
		ensoniq->ctrl |= ES_1370_PCLKDIVO(ES_1370_SRTODIV(runtime->rate));
		ensoniq->u.es1370.pclkdiv_lock |= ES_MODE_PLAY2;
	}
#endif
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	spin_unlock_irq(&ensoniq->reg_lock);
#ifndef CHIP1370
	snd_es1371_dac2_rate(ensoniq, runtime->rate);
#endif
	return 0;
}

static int snd_ensoniq_capture_prepare(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int mode = 0;

	ensoniq->c_dma_size = snd_pcm_lib_buffer_bytes(substream);
	ensoniq->c_period_size = snd_pcm_lib_period_bytes(substream);
	if (snd_pcm_format_width(runtime->format) == 16)
		mode |= 0x02;
	if (runtime->channels > 1)
		mode |= 0x01;
	spin_lock_irq(&ensoniq->reg_lock);
	ensoniq->ctrl &= ~ES_ADC_EN;
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	outl(ES_MEM_PAGEO(ES_PAGE_ADC), ES_REG(ensoniq, MEM_PAGE));
	outl(runtime->dma_addr, ES_REG(ensoniq, ADC_FRAME));
	outl((ensoniq->c_dma_size >> 2) - 1, ES_REG(ensoniq, ADC_SIZE));
	ensoniq->sctrl &= ~(ES_R1_LOOP_SEL | ES_R1_MODEM);
	ensoniq->sctrl |= ES_R1_INT_EN | ES_R1_MODEO(mode);
	outl(ensoniq->sctrl, ES_REG(ensoniq, SERIAL));
	outl((ensoniq->c_period_size >> snd_ensoniq_sample_shift[mode]) - 1,
	     ES_REG(ensoniq, ADC_COUNT));
#ifdef CHIP1370
	if (!(ensoniq->u.es1370.pclkdiv_lock & ES_MODE_PLAY2)) {
		ensoniq->ctrl &= ~ES_1370_PCLKDIVM;
		ensoniq->ctrl |= ES_1370_PCLKDIVO(ES_1370_SRTODIV(runtime->rate));
		ensoniq->u.es1370.pclkdiv_lock |= ES_MODE_CAPTURE;
	}
#endif
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	spin_unlock_irq(&ensoniq->reg_lock);
#ifndef CHIP1370
	snd_es1371_adc_rate(ensoniq, runtime->rate);
#endif
	return 0;
}

static snd_pcm_uframes_t snd_ensoniq_playback1_pointer(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);
	size_t ptr;

	spin_lock(&ensoniq->reg_lock);
	if (inl(ES_REG(ensoniq, CONTROL)) & ES_DAC1_EN) {
		outl(ES_MEM_PAGEO(ES_PAGE_DAC), ES_REG(ensoniq, MEM_PAGE));
		ptr = ES_REG_FCURR_COUNTI(inl(ES_REG(ensoniq, DAC1_SIZE)));
		ptr = bytes_to_frames(substream->runtime, ptr);
	} else {
		ptr = 0;
	}
	spin_unlock(&ensoniq->reg_lock);
	return ptr;
}

static snd_pcm_uframes_t snd_ensoniq_playback2_pointer(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);
	size_t ptr;

	spin_lock(&ensoniq->reg_lock);
	if (inl(ES_REG(ensoniq, CONTROL)) & ES_DAC2_EN) {
		outl(ES_MEM_PAGEO(ES_PAGE_DAC), ES_REG(ensoniq, MEM_PAGE));
		ptr = ES_REG_FCURR_COUNTI(inl(ES_REG(ensoniq, DAC2_SIZE)));
		ptr = bytes_to_frames(substream->runtime, ptr);
	} else {
		ptr = 0;
	}
	spin_unlock(&ensoniq->reg_lock);
	return ptr;
}

static snd_pcm_uframes_t snd_ensoniq_capture_pointer(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);
	size_t ptr;

	spin_lock(&ensoniq->reg_lock);
	if (inl(ES_REG(ensoniq, CONTROL)) & ES_ADC_EN) {
		outl(ES_MEM_PAGEO(ES_PAGE_ADC), ES_REG(ensoniq, MEM_PAGE));
		ptr = ES_REG_FCURR_COUNTI(inl(ES_REG(ensoniq, ADC_SIZE)));
		ptr = bytes_to_frames(substream->runtime, ptr);
	} else {
		ptr = 0;
	}
	spin_unlock(&ensoniq->reg_lock);
	return ptr;
}

static struct snd_pcm_hardware snd_ensoniq_playback1 =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID |
				 SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_SYNC_START),
	.formats =		SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE,
	.rates =
#ifndef CHIP1370
				SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
#else
				(SNDRV_PCM_RATE_KNOT | 	
				 SNDRV_PCM_RATE_11025 | SNDRV_PCM_RATE_22050 | 
				 SNDRV_PCM_RATE_44100),
#endif
	.rate_min =		4000,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	(128*1024),
	.period_bytes_min =	64,
	.period_bytes_max =	(128*1024),
	.periods_min =		1,
	.periods_max =		1024,
	.fifo_size =		0,
};

static struct snd_pcm_hardware snd_ensoniq_playback2 =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_PAUSE | 
				 SNDRV_PCM_INFO_SYNC_START),
	.formats =		SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE,
	.rates =		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		4000,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	(128*1024),
	.period_bytes_min =	64,
	.period_bytes_max =	(128*1024),
	.periods_min =		1,
	.periods_max =		1024,
	.fifo_size =		0,
};

static struct snd_pcm_hardware snd_ensoniq_capture =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_SYNC_START),
	.formats =		SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE,
	.rates =		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		4000,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	(128*1024),
	.period_bytes_min =	64,
	.period_bytes_max =	(128*1024),
	.periods_min =		1,
	.periods_max =		1024,
	.fifo_size =		0,
};

static int snd_ensoniq_playback1_open(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	ensoniq->mode |= ES_MODE_PLAY1;
	ensoniq->playback1_substream = substream;
	runtime->hw = snd_ensoniq_playback1;
	snd_pcm_set_sync(substream);
	spin_lock_irq(&ensoniq->reg_lock);
	if (ensoniq->spdif && ensoniq->playback2_substream == NULL)
		ensoniq->spdif_stream = ensoniq->spdif_default;
	spin_unlock_irq(&ensoniq->reg_lock);
#ifdef CHIP1370
	snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				   &snd_es1370_hw_constraints_rates);
#else
	snd_pcm_hw_constraint_ratdens(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				      &snd_es1371_hw_constraints_dac_clock);
#endif
	return 0;
}

static int snd_ensoniq_playback2_open(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	ensoniq->mode |= ES_MODE_PLAY2;
	ensoniq->playback2_substream = substream;
	runtime->hw = snd_ensoniq_playback2;
	snd_pcm_set_sync(substream);
	spin_lock_irq(&ensoniq->reg_lock);
	if (ensoniq->spdif && ensoniq->playback1_substream == NULL)
		ensoniq->spdif_stream = ensoniq->spdif_default;
	spin_unlock_irq(&ensoniq->reg_lock);
#ifdef CHIP1370
	snd_pcm_hw_constraint_ratnums(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				      &snd_es1370_hw_constraints_clock);
#else
	snd_pcm_hw_constraint_ratdens(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				      &snd_es1371_hw_constraints_dac_clock);
#endif
	return 0;
}

static int snd_ensoniq_capture_open(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	ensoniq->mode |= ES_MODE_CAPTURE;
	ensoniq->capture_substream = substream;
	runtime->hw = snd_ensoniq_capture;
	snd_pcm_set_sync(substream);
#ifdef CHIP1370
	snd_pcm_hw_constraint_ratnums(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				      &snd_es1370_hw_constraints_clock);
#else
	snd_pcm_hw_constraint_ratnums(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				      &snd_es1371_hw_constraints_adc_clock);
#endif
	return 0;
}

static int snd_ensoniq_playback1_close(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);

	ensoniq->playback1_substream = NULL;
	ensoniq->mode &= ~ES_MODE_PLAY1;
	return 0;
}

static int snd_ensoniq_playback2_close(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);

	ensoniq->playback2_substream = NULL;
	spin_lock_irq(&ensoniq->reg_lock);
#ifdef CHIP1370
	ensoniq->u.es1370.pclkdiv_lock &= ~ES_MODE_PLAY2;
#endif
	ensoniq->mode &= ~ES_MODE_PLAY2;
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static int snd_ensoniq_capture_close(struct snd_pcm_substream *substream)
{
	struct ensoniq *ensoniq = snd_pcm_substream_chip(substream);

	ensoniq->capture_substream = NULL;
	spin_lock_irq(&ensoniq->reg_lock);
#ifdef CHIP1370
	ensoniq->u.es1370.pclkdiv_lock &= ~ES_MODE_CAPTURE;
#endif
	ensoniq->mode &= ~ES_MODE_CAPTURE;
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static struct snd_pcm_ops snd_ensoniq_playback1_ops = {
	.open =		snd_ensoniq_playback1_open,
	.close =	snd_ensoniq_playback1_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	snd_ensoniq_hw_params,
	.hw_free =	snd_ensoniq_hw_free,
	.prepare =	snd_ensoniq_playback1_prepare,
	.trigger =	snd_ensoniq_trigger,
	.pointer =	snd_ensoniq_playback1_pointer,
};

static struct snd_pcm_ops snd_ensoniq_playback2_ops = {
	.open =		snd_ensoniq_playback2_open,
	.close =	snd_ensoniq_playback2_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	snd_ensoniq_hw_params,
	.hw_free =	snd_ensoniq_hw_free,
	.prepare =	snd_ensoniq_playback2_prepare,
	.trigger =	snd_ensoniq_trigger,
	.pointer =	snd_ensoniq_playback2_pointer,
};

static struct snd_pcm_ops snd_ensoniq_capture_ops = {
	.open =		snd_ensoniq_capture_open,
	.close =	snd_ensoniq_capture_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	snd_ensoniq_hw_params,
	.hw_free =	snd_ensoniq_hw_free,
	.prepare =	snd_ensoniq_capture_prepare,
	.trigger =	snd_ensoniq_trigger,
	.pointer =	snd_ensoniq_capture_pointer,
};

static int __devinit snd_ensoniq_pcm(struct ensoniq * ensoniq, int device,
				     struct snd_pcm ** rpcm)
{
	struct snd_pcm *pcm;
	int err;

	if (rpcm)
		*rpcm = NULL;
#ifdef CHIP1370
	err = snd_pcm_new(ensoniq->card, "ES1370/1", device, 1, 1, &pcm);
#else
	err = snd_pcm_new(ensoniq->card, "ES1371/1", device, 1, 1, &pcm);
#endif
	if (err < 0)
		return err;

#ifdef CHIP1370
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_ensoniq_playback2_ops);
#else
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_ensoniq_playback1_ops);
#endif
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_ensoniq_capture_ops);

	pcm->private_data = ensoniq;
	pcm->info_flags = 0;
#ifdef CHIP1370
	strcpy(pcm->name, "ES1370 DAC2/ADC");
#else
	strcpy(pcm->name, "ES1371 DAC2/ADC");
#endif
	ensoniq->pcm1 = pcm;

	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
					      snd_dma_pci_data(ensoniq->pci), 64*1024, 128*1024);

	if (rpcm)
		*rpcm = pcm;
	return 0;
}

static int __devinit snd_ensoniq_pcm2(struct ensoniq * ensoniq, int device,
				      struct snd_pcm ** rpcm)
{
	struct snd_pcm *pcm;
	int err;

	if (rpcm)
		*rpcm = NULL;
#ifdef CHIP1370
	err = snd_pcm_new(ensoniq->card, "ES1370/2", device, 1, 0, &pcm);
#else
	err = snd_pcm_new(ensoniq->card, "ES1371/2", device, 1, 0, &pcm);
#endif
	if (err < 0)
		return err;

#ifdef CHIP1370
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_ensoniq_playback1_ops);
#else
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_ensoniq_playback2_ops);
#endif
	pcm->private_data = ensoniq;
	pcm->info_flags = 0;
#ifdef CHIP1370
	strcpy(pcm->name, "ES1370 DAC1");
#else
	strcpy(pcm->name, "ES1371 DAC1");
#endif
	ensoniq->pcm2 = pcm;

	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
					      snd_dma_pci_data(ensoniq->pci), 64*1024, 128*1024);

	if (rpcm)
		*rpcm = pcm;
	return 0;
}


#ifdef CHIP1371
static int snd_ens1373_spdif_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_ens1373_spdif_default_get(struct snd_kcontrol *kcontrol,
                                         struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	spin_lock_irq(&ensoniq->reg_lock);
	ucontrol->value.iec958.status[0] = (ensoniq->spdif_default >> 0) & 0xff;
	ucontrol->value.iec958.status[1] = (ensoniq->spdif_default >> 8) & 0xff;
	ucontrol->value.iec958.status[2] = (ensoniq->spdif_default >> 16) & 0xff;
	ucontrol->value.iec958.status[3] = (ensoniq->spdif_default >> 24) & 0xff;
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static int snd_ens1373_spdif_default_put(struct snd_kcontrol *kcontrol,
                                         struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	unsigned int val;
	int change;

	val = ((u32)ucontrol->value.iec958.status[0] << 0) |
	      ((u32)ucontrol->value.iec958.status[1] << 8) |
	      ((u32)ucontrol->value.iec958.status[2] << 16) |
	      ((u32)ucontrol->value.iec958.status[3] << 24);
	spin_lock_irq(&ensoniq->reg_lock);
	change = ensoniq->spdif_default != val;
	ensoniq->spdif_default = val;
	if (change && ensoniq->playback1_substream == NULL &&
	    ensoniq->playback2_substream == NULL)
		outl(val, ES_REG(ensoniq, CHANNEL_STATUS));
	spin_unlock_irq(&ensoniq->reg_lock);
	return change;
}

static int snd_ens1373_spdif_mask_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = 0xff;
	ucontrol->value.iec958.status[1] = 0xff;
	ucontrol->value.iec958.status[2] = 0xff;
	ucontrol->value.iec958.status[3] = 0xff;
	return 0;
}

static int snd_ens1373_spdif_stream_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	spin_lock_irq(&ensoniq->reg_lock);
	ucontrol->value.iec958.status[0] = (ensoniq->spdif_stream >> 0) & 0xff;
	ucontrol->value.iec958.status[1] = (ensoniq->spdif_stream >> 8) & 0xff;
	ucontrol->value.iec958.status[2] = (ensoniq->spdif_stream >> 16) & 0xff;
	ucontrol->value.iec958.status[3] = (ensoniq->spdif_stream >> 24) & 0xff;
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static int snd_ens1373_spdif_stream_put(struct snd_kcontrol *kcontrol,
                                        struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	unsigned int val;
	int change;

	val = ((u32)ucontrol->value.iec958.status[0] << 0) |
	      ((u32)ucontrol->value.iec958.status[1] << 8) |
	      ((u32)ucontrol->value.iec958.status[2] << 16) |
	      ((u32)ucontrol->value.iec958.status[3] << 24);
	spin_lock_irq(&ensoniq->reg_lock);
	change = ensoniq->spdif_stream != val;
	ensoniq->spdif_stream = val;
	if (change && (ensoniq->playback1_substream != NULL ||
		       ensoniq->playback2_substream != NULL))
		outl(val, ES_REG(ensoniq, CHANNEL_STATUS));
	spin_unlock_irq(&ensoniq->reg_lock);
	return change;
}

#define ES1371_SPDIF(xname) \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .info = snd_es1371_spdif_info, \
  .get = snd_es1371_spdif_get, .put = snd_es1371_spdif_put }

#define snd_es1371_spdif_info		snd_ctl_boolean_mono_info

static int snd_es1371_spdif_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	
	spin_lock_irq(&ensoniq->reg_lock);
	ucontrol->value.integer.value[0] = ensoniq->ctrl & ES_1373_SPDIF_THRU ? 1 : 0;
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static int snd_es1371_spdif_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	unsigned int nval1, nval2;
	int change;
	
	nval1 = ucontrol->value.integer.value[0] ? ES_1373_SPDIF_THRU : 0;
	nval2 = ucontrol->value.integer.value[0] ? ES_1373_SPDIF_EN : 0;
	spin_lock_irq(&ensoniq->reg_lock);
	change = (ensoniq->ctrl & ES_1373_SPDIF_THRU) != nval1;
	ensoniq->ctrl &= ~ES_1373_SPDIF_THRU;
	ensoniq->ctrl |= nval1;
	ensoniq->cssr &= ~ES_1373_SPDIF_EN;
	ensoniq->cssr |= nval2;
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	outl(ensoniq->cssr, ES_REG(ensoniq, STATUS));
	spin_unlock_irq(&ensoniq->reg_lock);
	return change;
}


static struct snd_kcontrol_new snd_es1371_mixer_spdif[] __devinitdata = {
	ES1371_SPDIF(SNDRV_CTL_NAME_IEC958("",PLAYBACK,SWITCH)),
	{
		.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
		.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,DEFAULT),
		.info =		snd_ens1373_spdif_info,
		.get =		snd_ens1373_spdif_default_get,
		.put =		snd_ens1373_spdif_default_put,
	},
	{
		.access =	SNDRV_CTL_ELEM_ACCESS_READ,
		.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
		.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,MASK),
		.info =		snd_ens1373_spdif_info,
		.get =		snd_ens1373_spdif_mask_get
	},
	{
		.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
		.name =		SNDRV_CTL_NAME_IEC958("",PLAYBACK,PCM_STREAM),
		.info =		snd_ens1373_spdif_info,
		.get =		snd_ens1373_spdif_stream_get,
		.put =		snd_ens1373_spdif_stream_put
	},
};


#define snd_es1373_rear_info		snd_ctl_boolean_mono_info

static int snd_es1373_rear_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	int val = 0;
	
	spin_lock_irq(&ensoniq->reg_lock);
	if ((ensoniq->cssr & (ES_1373_REAR_BIT27|ES_1373_REAR_BIT26|
			      ES_1373_REAR_BIT24)) == ES_1373_REAR_BIT26)
	    	val = 1;
	ucontrol->value.integer.value[0] = val;
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static int snd_es1373_rear_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	unsigned int nval1;
	int change;
	
	nval1 = ucontrol->value.integer.value[0] ?
		ES_1373_REAR_BIT26 : (ES_1373_REAR_BIT27|ES_1373_REAR_BIT24);
	spin_lock_irq(&ensoniq->reg_lock);
	change = (ensoniq->cssr & (ES_1373_REAR_BIT27|
				   ES_1373_REAR_BIT26|ES_1373_REAR_BIT24)) != nval1;
	ensoniq->cssr &= ~(ES_1373_REAR_BIT27|ES_1373_REAR_BIT26|ES_1373_REAR_BIT24);
	ensoniq->cssr |= nval1;
	outl(ensoniq->cssr, ES_REG(ensoniq, STATUS));
	spin_unlock_irq(&ensoniq->reg_lock);
	return change;
}

static struct snd_kcontrol_new snd_ens1373_rear __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		"AC97 2ch->4ch Copy Switch",
	.info =		snd_es1373_rear_info,
	.get =		snd_es1373_rear_get,
	.put =		snd_es1373_rear_put,
};

#define snd_es1373_line_info		snd_ctl_boolean_mono_info

static int snd_es1373_line_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	int val = 0;
	
	spin_lock_irq(&ensoniq->reg_lock);
	if ((ensoniq->ctrl & ES_1371_GPIO_OUTM) >= 4)
	    	val = 1;
	ucontrol->value.integer.value[0] = val;
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static int snd_es1373_line_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	int changed;
	unsigned int ctrl;
	
	spin_lock_irq(&ensoniq->reg_lock);
	ctrl = ensoniq->ctrl;
	if (ucontrol->value.integer.value[0])
		ensoniq->ctrl |= ES_1371_GPIO_OUT(4);	
	else
		ensoniq->ctrl &= ~ES_1371_GPIO_OUT(4);
	changed = (ctrl != ensoniq->ctrl);
	if (changed)
		outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	spin_unlock_irq(&ensoniq->reg_lock);
	return changed;
}

static struct snd_kcontrol_new snd_ens1373_line __devinitdata =
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		"Line In->Rear Out Switch",
	.info =		snd_es1373_line_info,
	.get =		snd_es1373_line_get,
	.put =		snd_es1373_line_put,
};

static void snd_ensoniq_mixer_free_ac97(struct snd_ac97 *ac97)
{
	struct ensoniq *ensoniq = ac97->private_data;
	ensoniq->u.es1371.ac97 = NULL;
}

struct es1371_quirk {
	unsigned short vid;		
	unsigned short did;		
	unsigned char rev;		
};

static int es1371_quirk_lookup(struct ensoniq *ensoniq,
				struct es1371_quirk *list)
{
	while (list->vid != (unsigned short)PCI_ANY_ID) {
		if (ensoniq->pci->vendor == list->vid &&
		    ensoniq->pci->device == list->did &&
		    ensoniq->rev == list->rev)
			return 1;
		list++;
	}
	return 0;
}

static struct es1371_quirk es1371_spdif_present[] __devinitdata = {
	{ .vid = PCI_VENDOR_ID_ENSONIQ, .did = PCI_DEVICE_ID_ENSONIQ_CT5880, .rev = CT5880REV_CT5880_C },
	{ .vid = PCI_VENDOR_ID_ENSONIQ, .did = PCI_DEVICE_ID_ENSONIQ_CT5880, .rev = CT5880REV_CT5880_D },
	{ .vid = PCI_VENDOR_ID_ENSONIQ, .did = PCI_DEVICE_ID_ENSONIQ_CT5880, .rev = CT5880REV_CT5880_E },
	{ .vid = PCI_VENDOR_ID_ENSONIQ, .did = PCI_DEVICE_ID_ENSONIQ_ES1371, .rev = ES1371REV_CT5880_A },
	{ .vid = PCI_VENDOR_ID_ENSONIQ, .did = PCI_DEVICE_ID_ENSONIQ_ES1371, .rev = ES1371REV_ES1373_8 },
	{ .vid = PCI_ANY_ID, .did = PCI_ANY_ID }
};

static struct snd_pci_quirk ens1373_line_quirk[] __devinitdata = {
	SND_PCI_QUIRK_ID(0x1274, 0x2000), 
	SND_PCI_QUIRK_ID(0x1458, 0xa000), 
	{ } 
};

static int __devinit snd_ensoniq_1371_mixer(struct ensoniq *ensoniq,
					    int has_spdif, int has_line)
{
	struct snd_card *card = ensoniq->card;
	struct snd_ac97_bus *pbus;
	struct snd_ac97_template ac97;
	int err;
	static struct snd_ac97_bus_ops ops = {
		.write = snd_es1371_codec_write,
		.read = snd_es1371_codec_read,
		.wait = snd_es1371_codec_wait,
	};

	if ((err = snd_ac97_bus(card, 0, &ops, NULL, &pbus)) < 0)
		return err;

	memset(&ac97, 0, sizeof(ac97));
	ac97.private_data = ensoniq;
	ac97.private_free = snd_ensoniq_mixer_free_ac97;
	ac97.pci = ensoniq->pci;
	ac97.scaps = AC97_SCAP_AUDIO;
	if ((err = snd_ac97_mixer(pbus, &ac97, &ensoniq->u.es1371.ac97)) < 0)
		return err;
	if (has_spdif > 0 ||
	    (!has_spdif && es1371_quirk_lookup(ensoniq, es1371_spdif_present))) {
		struct snd_kcontrol *kctl;
		int i, is_spdif = 0;

		ensoniq->spdif_default = ensoniq->spdif_stream =
			SNDRV_PCM_DEFAULT_CON_SPDIF;
		outl(ensoniq->spdif_default, ES_REG(ensoniq, CHANNEL_STATUS));

		if (ensoniq->u.es1371.ac97->ext_id & AC97_EI_SPDIF)
			is_spdif++;

		for (i = 0; i < ARRAY_SIZE(snd_es1371_mixer_spdif); i++) {
			kctl = snd_ctl_new1(&snd_es1371_mixer_spdif[i], ensoniq);
			if (!kctl)
				return -ENOMEM;
			kctl->id.index = is_spdif;
			err = snd_ctl_add(card, kctl);
			if (err < 0)
				return err;
		}
	}
	if (ensoniq->u.es1371.ac97->ext_id & AC97_EI_SDAC) {
		
		ensoniq->cssr &= ~(ES_1373_REAR_BIT27|ES_1373_REAR_BIT24);
		ensoniq->cssr |= ES_1373_REAR_BIT26;
		err = snd_ctl_add(card, snd_ctl_new1(&snd_ens1373_rear, ensoniq));
		if (err < 0)
			return err;
	}
	if (has_line > 0 ||
	    snd_pci_quirk_lookup(ensoniq->pci, ens1373_line_quirk)) {
		 err = snd_ctl_add(card, snd_ctl_new1(&snd_ens1373_line,
						      ensoniq));
		 if (err < 0)
			 return err;
	}

	return 0;
}

#endif 

#ifdef CHIP1370
#define ENSONIQ_CONTROL(xname, mask) \
{ .iface = SNDRV_CTL_ELEM_IFACE_CARD, .name = xname, .info = snd_ensoniq_control_info, \
  .get = snd_ensoniq_control_get, .put = snd_ensoniq_control_put, \
  .private_value = mask }

#define snd_ensoniq_control_info	snd_ctl_boolean_mono_info

static int snd_ensoniq_control_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	int mask = kcontrol->private_value;
	
	spin_lock_irq(&ensoniq->reg_lock);
	ucontrol->value.integer.value[0] = ensoniq->ctrl & mask ? 1 : 0;
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static int snd_ensoniq_control_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct ensoniq *ensoniq = snd_kcontrol_chip(kcontrol);
	int mask = kcontrol->private_value;
	unsigned int nval;
	int change;
	
	nval = ucontrol->value.integer.value[0] ? mask : 0;
	spin_lock_irq(&ensoniq->reg_lock);
	change = (ensoniq->ctrl & mask) != nval;
	ensoniq->ctrl &= ~mask;
	ensoniq->ctrl |= nval;
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	spin_unlock_irq(&ensoniq->reg_lock);
	return change;
}


static struct snd_kcontrol_new snd_es1370_controls[2] __devinitdata = {
ENSONIQ_CONTROL("PCM 0 Output also on Line-In Jack", ES_1370_XCTL0),
ENSONIQ_CONTROL("Mic +5V bias", ES_1370_XCTL1)
};

#define ES1370_CONTROLS ARRAY_SIZE(snd_es1370_controls)

static void snd_ensoniq_mixer_free_ak4531(struct snd_ak4531 *ak4531)
{
	struct ensoniq *ensoniq = ak4531->private_data;
	ensoniq->u.es1370.ak4531 = NULL;
}

static int __devinit snd_ensoniq_1370_mixer(struct ensoniq * ensoniq)
{
	struct snd_card *card = ensoniq->card;
	struct snd_ak4531 ak4531;
	unsigned int idx;
	int err;

	
	outw(ES_1370_CODEC_WRITE(AK4531_RESET, 0x02), ES_REG(ensoniq, 1370_CODEC));
	inw(ES_REG(ensoniq, 1370_CODEC));
	udelay(100);
	outw(ES_1370_CODEC_WRITE(AK4531_RESET, 0x03), ES_REG(ensoniq, 1370_CODEC));
	inw(ES_REG(ensoniq, 1370_CODEC));
	udelay(100);

	memset(&ak4531, 0, sizeof(ak4531));
	ak4531.write = snd_es1370_codec_write;
	ak4531.private_data = ensoniq;
	ak4531.private_free = snd_ensoniq_mixer_free_ak4531;
	if ((err = snd_ak4531_mixer(card, &ak4531, &ensoniq->u.es1370.ak4531)) < 0)
		return err;
	for (idx = 0; idx < ES1370_CONTROLS; idx++) {
		err = snd_ctl_add(card, snd_ctl_new1(&snd_es1370_controls[idx], ensoniq));
		if (err < 0)
			return err;
	}
	return 0;
}

#endif 

#ifdef SUPPORT_JOYSTICK

#ifdef CHIP1371
static int __devinit snd_ensoniq_get_joystick_port(int dev)
{
	switch (joystick_port[dev]) {
	case 0: 
	case 1: 
	case 0x200:
	case 0x208:
	case 0x210:
	case 0x218:
		return joystick_port[dev];

	default:
		printk(KERN_ERR "ens1371: invalid joystick port %#x", joystick_port[dev]);
		return 0;
	}
}
#else
static inline int snd_ensoniq_get_joystick_port(int dev)
{
	return joystick[dev] ? 0x200 : 0;
}
#endif

static int __devinit snd_ensoniq_create_gameport(struct ensoniq *ensoniq, int dev)
{
	struct gameport *gp;
	int io_port;

	io_port = snd_ensoniq_get_joystick_port(dev);

	switch (io_port) {
	case 0:
		return -ENOSYS;

	case 1: 
		for (io_port = 0x200; io_port <= 0x218; io_port += 8)
			if (request_region(io_port, 8, "ens137x: gameport"))
				break;
		if (io_port > 0x218) {
			printk(KERN_WARNING "ens137x: no gameport ports available\n");
			return -EBUSY;
		}
		break;

	default:
		if (!request_region(io_port, 8, "ens137x: gameport")) {
			printk(KERN_WARNING "ens137x: gameport io port 0x%#x in use\n",
			       io_port);
			return -EBUSY;
		}
		break;
	}

	ensoniq->gameport = gp = gameport_allocate_port();
	if (!gp) {
		printk(KERN_ERR "ens137x: cannot allocate memory for gameport\n");
		release_region(io_port, 8);
		return -ENOMEM;
	}

	gameport_set_name(gp, "ES137x");
	gameport_set_phys(gp, "pci%s/gameport0", pci_name(ensoniq->pci));
	gameport_set_dev_parent(gp, &ensoniq->pci->dev);
	gp->io = io_port;

	ensoniq->ctrl |= ES_JYSTK_EN;
#ifdef CHIP1371
	ensoniq->ctrl &= ~ES_1371_JOY_ASELM;
	ensoniq->ctrl |= ES_1371_JOY_ASEL((io_port - 0x200) / 8);
#endif
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));

	gameport_register_port(ensoniq->gameport);

	return 0;
}

static void snd_ensoniq_free_gameport(struct ensoniq *ensoniq)
{
	if (ensoniq->gameport) {
		int port = ensoniq->gameport->io;

		gameport_unregister_port(ensoniq->gameport);
		ensoniq->gameport = NULL;
		ensoniq->ctrl &= ~ES_JYSTK_EN;
		outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
		release_region(port, 8);
	}
}
#else
static inline int snd_ensoniq_create_gameport(struct ensoniq *ensoniq, long port) { return -ENOSYS; }
static inline void snd_ensoniq_free_gameport(struct ensoniq *ensoniq) { }
#endif 


static void snd_ensoniq_proc_read(struct snd_info_entry *entry, 
				  struct snd_info_buffer *buffer)
{
	struct ensoniq *ensoniq = entry->private_data;

#ifdef CHIP1370
	snd_iprintf(buffer, "Ensoniq AudioPCI ES1370\n\n");
#else
	snd_iprintf(buffer, "Ensoniq AudioPCI ES1371\n\n");
#endif
	snd_iprintf(buffer, "Joystick enable  : %s\n",
		    ensoniq->ctrl & ES_JYSTK_EN ? "on" : "off");
#ifdef CHIP1370
	snd_iprintf(buffer, "MIC +5V bias     : %s\n",
		    ensoniq->ctrl & ES_1370_XCTL1 ? "on" : "off");
	snd_iprintf(buffer, "Line In to AOUT  : %s\n",
		    ensoniq->ctrl & ES_1370_XCTL0 ? "on" : "off");
#else
	snd_iprintf(buffer, "Joystick port    : 0x%x\n",
		    (ES_1371_JOY_ASELI(ensoniq->ctrl) * 8) + 0x200);
#endif
}

static void __devinit snd_ensoniq_proc_init(struct ensoniq * ensoniq)
{
	struct snd_info_entry *entry;

	if (! snd_card_proc_new(ensoniq->card, "audiopci", &entry))
		snd_info_set_text_ops(entry, ensoniq, snd_ensoniq_proc_read);
}


static int snd_ensoniq_free(struct ensoniq *ensoniq)
{
	snd_ensoniq_free_gameport(ensoniq);
	if (ensoniq->irq < 0)
		goto __hw_end;
#ifdef CHIP1370
	outl(ES_1370_SERR_DISABLE, ES_REG(ensoniq, CONTROL));	
	outl(0, ES_REG(ensoniq, SERIAL));	
#else
	outl(0, ES_REG(ensoniq, CONTROL));	
	outl(0, ES_REG(ensoniq, SERIAL));	
#endif
	if (ensoniq->irq >= 0)
		synchronize_irq(ensoniq->irq);
	pci_set_power_state(ensoniq->pci, 3);
      __hw_end:
#ifdef CHIP1370
	if (ensoniq->dma_bug.area)
		snd_dma_free_pages(&ensoniq->dma_bug);
#endif
	if (ensoniq->irq >= 0)
		free_irq(ensoniq->irq, ensoniq);
	pci_release_regions(ensoniq->pci);
	pci_disable_device(ensoniq->pci);
	kfree(ensoniq);
	return 0;
}

static int snd_ensoniq_dev_free(struct snd_device *device)
{
	struct ensoniq *ensoniq = device->device_data;
	return snd_ensoniq_free(ensoniq);
}

#ifdef CHIP1371
static struct snd_pci_quirk es1371_amplifier_hack[] __devinitdata = {
	SND_PCI_QUIRK_ID(0x107b, 0x2150),	
	SND_PCI_QUIRK_ID(0x13bd, 0x100c),	
	SND_PCI_QUIRK_ID(0x1102, 0x5938),	
	SND_PCI_QUIRK_ID(0x1102, 0x8938),	
	{ } 
};

static struct es1371_quirk es1371_ac97_reset_hack[] = {
	{ .vid = PCI_VENDOR_ID_ENSONIQ, .did = PCI_DEVICE_ID_ENSONIQ_CT5880, .rev = CT5880REV_CT5880_C },
	{ .vid = PCI_VENDOR_ID_ENSONIQ, .did = PCI_DEVICE_ID_ENSONIQ_CT5880, .rev = CT5880REV_CT5880_D },
	{ .vid = PCI_VENDOR_ID_ENSONIQ, .did = PCI_DEVICE_ID_ENSONIQ_CT5880, .rev = CT5880REV_CT5880_E },
	{ .vid = PCI_VENDOR_ID_ENSONIQ, .did = PCI_DEVICE_ID_ENSONIQ_ES1371, .rev = ES1371REV_CT5880_A },
	{ .vid = PCI_VENDOR_ID_ENSONIQ, .did = PCI_DEVICE_ID_ENSONIQ_ES1371, .rev = ES1371REV_ES1373_8 },
	{ .vid = PCI_ANY_ID, .did = PCI_ANY_ID }
};
#endif

static void snd_ensoniq_chip_init(struct ensoniq *ensoniq)
{
#ifdef CHIP1371
	int idx;
#endif
#ifdef CHIP1370
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	outl(ensoniq->sctrl, ES_REG(ensoniq, SERIAL));
	outl(ES_MEM_PAGEO(ES_PAGE_ADC), ES_REG(ensoniq, MEM_PAGE));
	outl(ensoniq->dma_bug.addr, ES_REG(ensoniq, PHANTOM_FRAME));
	outl(0, ES_REG(ensoniq, PHANTOM_COUNT));
#else
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	outl(ensoniq->sctrl, ES_REG(ensoniq, SERIAL));
	outl(0, ES_REG(ensoniq, 1371_LEGACY));
	if (es1371_quirk_lookup(ensoniq, es1371_ac97_reset_hack)) {
	    outl(ensoniq->cssr, ES_REG(ensoniq, STATUS));
	    msleep(20);
	}
	
	outl(ensoniq->ctrl | ES_1371_SYNC_RES, ES_REG(ensoniq, CONTROL));
	inl(ES_REG(ensoniq, CONTROL));
	udelay(20);
	outl(ensoniq->ctrl, ES_REG(ensoniq, CONTROL));
	
	snd_es1371_wait_src_ready(ensoniq);	
	outl(ES_1371_SRC_DISABLE, ES_REG(ensoniq, 1371_SMPRATE));
	for (idx = 0; idx < 0x80; idx++)
		snd_es1371_src_write(ensoniq, idx, 0);
	snd_es1371_src_write(ensoniq, ES_SMPREG_DAC1 + ES_SMPREG_TRUNC_N, 16 << 4);
	snd_es1371_src_write(ensoniq, ES_SMPREG_DAC1 + ES_SMPREG_INT_REGS, 16 << 10);
	snd_es1371_src_write(ensoniq, ES_SMPREG_DAC2 + ES_SMPREG_TRUNC_N, 16 << 4);
	snd_es1371_src_write(ensoniq, ES_SMPREG_DAC2 + ES_SMPREG_INT_REGS, 16 << 10);
	snd_es1371_src_write(ensoniq, ES_SMPREG_VOL_ADC, 1 << 12);
	snd_es1371_src_write(ensoniq, ES_SMPREG_VOL_ADC + 1, 1 << 12);
	snd_es1371_src_write(ensoniq, ES_SMPREG_VOL_DAC1, 1 << 12);
	snd_es1371_src_write(ensoniq, ES_SMPREG_VOL_DAC1 + 1, 1 << 12);
	snd_es1371_src_write(ensoniq, ES_SMPREG_VOL_DAC2, 1 << 12);
	snd_es1371_src_write(ensoniq, ES_SMPREG_VOL_DAC2 + 1, 1 << 12);
	snd_es1371_adc_rate(ensoniq, 22050);
	snd_es1371_dac1_rate(ensoniq, 22050);
	snd_es1371_dac2_rate(ensoniq, 22050);
	snd_es1371_wait_src_ready(ensoniq);
	outl(0, ES_REG(ensoniq, 1371_SMPRATE));
	
	outl(ES_1371_CODEC_WRITE(0, 0), ES_REG(ensoniq, 1371_CODEC));
#endif
	outb(ensoniq->uartc = 0x00, ES_REG(ensoniq, UART_CONTROL));
	outb(0x00, ES_REG(ensoniq, UART_RES));
	outl(ensoniq->cssr, ES_REG(ensoniq, STATUS));
	synchronize_irq(ensoniq->irq);
}

#ifdef CONFIG_PM
static int snd_ensoniq_suspend(struct pci_dev *pci, pm_message_t state)
{
	struct snd_card *card = pci_get_drvdata(pci);
	struct ensoniq *ensoniq = card->private_data;
	
	snd_power_change_state(card, SNDRV_CTL_POWER_D3hot);

	snd_pcm_suspend_all(ensoniq->pcm1);
	snd_pcm_suspend_all(ensoniq->pcm2);
	
#ifdef CHIP1371	
	snd_ac97_suspend(ensoniq->u.es1371.ac97);
#else
	
	outw(ES_1370_CODEC_WRITE(AK4531_RESET, 0x02), ES_REG(ensoniq, 1370_CODEC));
	inw(ES_REG(ensoniq, 1370_CODEC));
	udelay(100);
	outw(ES_1370_CODEC_WRITE(AK4531_RESET, 0x03), ES_REG(ensoniq, 1370_CODEC));
	inw(ES_REG(ensoniq, 1370_CODEC));
	udelay(100);
	snd_ak4531_suspend(ensoniq->u.es1370.ak4531);
#endif	

	pci_disable_device(pci);
	pci_save_state(pci);
	pci_set_power_state(pci, pci_choose_state(pci, state));
	return 0;
}

static int snd_ensoniq_resume(struct pci_dev *pci)
{
	struct snd_card *card = pci_get_drvdata(pci);
	struct ensoniq *ensoniq = card->private_data;

	pci_set_power_state(pci, PCI_D0);
	pci_restore_state(pci);
	if (pci_enable_device(pci) < 0) {
		printk(KERN_ERR DRIVER_NAME ": pci_enable_device failed, "
		       "disabling device\n");
		snd_card_disconnect(card);
		return -EIO;
	}
	pci_set_master(pci);

	snd_ensoniq_chip_init(ensoniq);

#ifdef CHIP1371	
	snd_ac97_resume(ensoniq->u.es1371.ac97);
#else
	snd_ak4531_resume(ensoniq->u.es1370.ak4531);
#endif	
	snd_power_change_state(card, SNDRV_CTL_POWER_D0);
	return 0;
}
#endif 


static int __devinit snd_ensoniq_create(struct snd_card *card,
				     struct pci_dev *pci,
				     struct ensoniq ** rensoniq)
{
	struct ensoniq *ensoniq;
	int err;
	static struct snd_device_ops ops = {
		.dev_free =	snd_ensoniq_dev_free,
	};

	*rensoniq = NULL;
	if ((err = pci_enable_device(pci)) < 0)
		return err;
	ensoniq = kzalloc(sizeof(*ensoniq), GFP_KERNEL);
	if (ensoniq == NULL) {
		pci_disable_device(pci);
		return -ENOMEM;
	}
	spin_lock_init(&ensoniq->reg_lock);
	mutex_init(&ensoniq->src_mutex);
	ensoniq->card = card;
	ensoniq->pci = pci;
	ensoniq->irq = -1;
	if ((err = pci_request_regions(pci, "Ensoniq AudioPCI")) < 0) {
		kfree(ensoniq);
		pci_disable_device(pci);
		return err;
	}
	ensoniq->port = pci_resource_start(pci, 0);
	if (request_irq(pci->irq, snd_audiopci_interrupt, IRQF_SHARED,
			KBUILD_MODNAME, ensoniq)) {
		snd_printk(KERN_ERR "unable to grab IRQ %d\n", pci->irq);
		snd_ensoniq_free(ensoniq);
		return -EBUSY;
	}
	ensoniq->irq = pci->irq;
#ifdef CHIP1370
	if (snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, snd_dma_pci_data(pci),
				16, &ensoniq->dma_bug) < 0) {
		snd_printk(KERN_ERR "unable to allocate space for phantom area - dma_bug\n");
		snd_ensoniq_free(ensoniq);
		return -EBUSY;
	}
#endif
	pci_set_master(pci);
	ensoniq->rev = pci->revision;
#ifdef CHIP1370
#if 0
	ensoniq->ctrl = ES_1370_CDC_EN | ES_1370_SERR_DISABLE |
		ES_1370_PCLKDIVO(ES_1370_SRTODIV(8000));
#else	
	ensoniq->ctrl = ES_1370_CDC_EN | ES_1370_PCLKDIVO(ES_1370_SRTODIV(8000));
#endif
	ensoniq->sctrl = 0;
#else
	ensoniq->ctrl = 0;
	ensoniq->sctrl = 0;
	ensoniq->cssr = 0;
	if (snd_pci_quirk_lookup(pci, es1371_amplifier_hack))
		ensoniq->ctrl |= ES_1371_GPIO_OUT(1);	

	if (es1371_quirk_lookup(ensoniq, es1371_ac97_reset_hack))
		ensoniq->cssr |= ES_1371_ST_AC97_RST;
#endif

	snd_ensoniq_chip_init(ensoniq);

	if ((err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, ensoniq, &ops)) < 0) {
		snd_ensoniq_free(ensoniq);
		return err;
	}

	snd_ensoniq_proc_init(ensoniq);

	snd_card_set_dev(card, &pci->dev);

	*rensoniq = ensoniq;
	return 0;
}


static void snd_ensoniq_midi_interrupt(struct ensoniq * ensoniq)
{
	struct snd_rawmidi *rmidi = ensoniq->rmidi;
	unsigned char status, mask, byte;

	if (rmidi == NULL)
		return;
	
	spin_lock(&ensoniq->reg_lock);
	mask = ensoniq->uartm & ES_MODE_INPUT ? ES_RXRDY : 0;
	while (mask) {
		status = inb(ES_REG(ensoniq, UART_STATUS));
		if ((status & mask) == 0)
			break;
		byte = inb(ES_REG(ensoniq, UART_DATA));
		snd_rawmidi_receive(ensoniq->midi_input, &byte, 1);
	}
	spin_unlock(&ensoniq->reg_lock);

	
	spin_lock(&ensoniq->reg_lock);
	mask = ensoniq->uartm & ES_MODE_OUTPUT ? ES_TXRDY : 0;
	while (mask) {
		status = inb(ES_REG(ensoniq, UART_STATUS));
		if ((status & mask) == 0)
			break;
		if (snd_rawmidi_transmit(ensoniq->midi_output, &byte, 1) != 1) {
			ensoniq->uartc &= ~ES_TXINTENM;
			outb(ensoniq->uartc, ES_REG(ensoniq, UART_CONTROL));
			mask &= ~ES_TXRDY;
		} else {
			outb(byte, ES_REG(ensoniq, UART_DATA));
		}
	}
	spin_unlock(&ensoniq->reg_lock);
}

static int snd_ensoniq_midi_input_open(struct snd_rawmidi_substream *substream)
{
	struct ensoniq *ensoniq = substream->rmidi->private_data;

	spin_lock_irq(&ensoniq->reg_lock);
	ensoniq->uartm |= ES_MODE_INPUT;
	ensoniq->midi_input = substream;
	if (!(ensoniq->uartm & ES_MODE_OUTPUT)) {
		outb(ES_CNTRL(3), ES_REG(ensoniq, UART_CONTROL));
		outb(ensoniq->uartc = 0, ES_REG(ensoniq, UART_CONTROL));
		outl(ensoniq->ctrl |= ES_UART_EN, ES_REG(ensoniq, CONTROL));
	}
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static int snd_ensoniq_midi_input_close(struct snd_rawmidi_substream *substream)
{
	struct ensoniq *ensoniq = substream->rmidi->private_data;

	spin_lock_irq(&ensoniq->reg_lock);
	if (!(ensoniq->uartm & ES_MODE_OUTPUT)) {
		outb(ensoniq->uartc = 0, ES_REG(ensoniq, UART_CONTROL));
		outl(ensoniq->ctrl &= ~ES_UART_EN, ES_REG(ensoniq, CONTROL));
	} else {
		outb(ensoniq->uartc &= ~ES_RXINTEN, ES_REG(ensoniq, UART_CONTROL));
	}
	ensoniq->midi_input = NULL;
	ensoniq->uartm &= ~ES_MODE_INPUT;
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static int snd_ensoniq_midi_output_open(struct snd_rawmidi_substream *substream)
{
	struct ensoniq *ensoniq = substream->rmidi->private_data;

	spin_lock_irq(&ensoniq->reg_lock);
	ensoniq->uartm |= ES_MODE_OUTPUT;
	ensoniq->midi_output = substream;
	if (!(ensoniq->uartm & ES_MODE_INPUT)) {
		outb(ES_CNTRL(3), ES_REG(ensoniq, UART_CONTROL));
		outb(ensoniq->uartc = 0, ES_REG(ensoniq, UART_CONTROL));
		outl(ensoniq->ctrl |= ES_UART_EN, ES_REG(ensoniq, CONTROL));
	}
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static int snd_ensoniq_midi_output_close(struct snd_rawmidi_substream *substream)
{
	struct ensoniq *ensoniq = substream->rmidi->private_data;

	spin_lock_irq(&ensoniq->reg_lock);
	if (!(ensoniq->uartm & ES_MODE_INPUT)) {
		outb(ensoniq->uartc = 0, ES_REG(ensoniq, UART_CONTROL));
		outl(ensoniq->ctrl &= ~ES_UART_EN, ES_REG(ensoniq, CONTROL));
	} else {
		outb(ensoniq->uartc &= ~ES_TXINTENM, ES_REG(ensoniq, UART_CONTROL));
	}
	ensoniq->midi_output = NULL;
	ensoniq->uartm &= ~ES_MODE_OUTPUT;
	spin_unlock_irq(&ensoniq->reg_lock);
	return 0;
}

static void snd_ensoniq_midi_input_trigger(struct snd_rawmidi_substream *substream, int up)
{
	unsigned long flags;
	struct ensoniq *ensoniq = substream->rmidi->private_data;
	int idx;

	spin_lock_irqsave(&ensoniq->reg_lock, flags);
	if (up) {
		if ((ensoniq->uartc & ES_RXINTEN) == 0) {
			
			for (idx = 0; idx < 32; idx++)
				inb(ES_REG(ensoniq, UART_DATA));
			ensoniq->uartc |= ES_RXINTEN;
			outb(ensoniq->uartc, ES_REG(ensoniq, UART_CONTROL));
		}
	} else {
		if (ensoniq->uartc & ES_RXINTEN) {
			ensoniq->uartc &= ~ES_RXINTEN;
			outb(ensoniq->uartc, ES_REG(ensoniq, UART_CONTROL));
		}
	}
	spin_unlock_irqrestore(&ensoniq->reg_lock, flags);
}

static void snd_ensoniq_midi_output_trigger(struct snd_rawmidi_substream *substream, int up)
{
	unsigned long flags;
	struct ensoniq *ensoniq = substream->rmidi->private_data;
	unsigned char byte;

	spin_lock_irqsave(&ensoniq->reg_lock, flags);
	if (up) {
		if (ES_TXINTENI(ensoniq->uartc) == 0) {
			ensoniq->uartc |= ES_TXINTENO(1);
			
			while (ES_TXINTENI(ensoniq->uartc) == 1 &&
			       (inb(ES_REG(ensoniq, UART_STATUS)) & ES_TXRDY)) {
				if (snd_rawmidi_transmit(substream, &byte, 1) != 1) {
					ensoniq->uartc &= ~ES_TXINTENM;
				} else {
					outb(byte, ES_REG(ensoniq, UART_DATA));
				}
			}
			outb(ensoniq->uartc, ES_REG(ensoniq, UART_CONTROL));
		}
	} else {
		if (ES_TXINTENI(ensoniq->uartc) == 1) {
			ensoniq->uartc &= ~ES_TXINTENM;
			outb(ensoniq->uartc, ES_REG(ensoniq, UART_CONTROL));
		}
	}
	spin_unlock_irqrestore(&ensoniq->reg_lock, flags);
}

static struct snd_rawmidi_ops snd_ensoniq_midi_output =
{
	.open =		snd_ensoniq_midi_output_open,
	.close =	snd_ensoniq_midi_output_close,
	.trigger =	snd_ensoniq_midi_output_trigger,
};

static struct snd_rawmidi_ops snd_ensoniq_midi_input =
{
	.open =		snd_ensoniq_midi_input_open,
	.close =	snd_ensoniq_midi_input_close,
	.trigger =	snd_ensoniq_midi_input_trigger,
};

static int __devinit snd_ensoniq_midi(struct ensoniq * ensoniq, int device,
				      struct snd_rawmidi **rrawmidi)
{
	struct snd_rawmidi *rmidi;
	int err;

	if (rrawmidi)
		*rrawmidi = NULL;
	if ((err = snd_rawmidi_new(ensoniq->card, "ES1370/1", device, 1, 1, &rmidi)) < 0)
		return err;
#ifdef CHIP1370
	strcpy(rmidi->name, "ES1370");
#else
	strcpy(rmidi->name, "ES1371");
#endif
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT, &snd_ensoniq_midi_output);
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_INPUT, &snd_ensoniq_midi_input);
	rmidi->info_flags |= SNDRV_RAWMIDI_INFO_OUTPUT | SNDRV_RAWMIDI_INFO_INPUT |
		SNDRV_RAWMIDI_INFO_DUPLEX;
	rmidi->private_data = ensoniq;
	ensoniq->rmidi = rmidi;
	if (rrawmidi)
		*rrawmidi = rmidi;
	return 0;
}


static irqreturn_t snd_audiopci_interrupt(int irq, void *dev_id)
{
	struct ensoniq *ensoniq = dev_id;
	unsigned int status, sctrl;

	if (ensoniq == NULL)
		return IRQ_NONE;

	status = inl(ES_REG(ensoniq, STATUS));
	if (!(status & ES_INTR))
		return IRQ_NONE;

	spin_lock(&ensoniq->reg_lock);
	sctrl = ensoniq->sctrl;
	if (status & ES_DAC1)
		sctrl &= ~ES_P1_INT_EN;
	if (status & ES_DAC2)
		sctrl &= ~ES_P2_INT_EN;
	if (status & ES_ADC)
		sctrl &= ~ES_R1_INT_EN;
	outl(sctrl, ES_REG(ensoniq, SERIAL));
	outl(ensoniq->sctrl, ES_REG(ensoniq, SERIAL));
	spin_unlock(&ensoniq->reg_lock);

	if (status & ES_UART)
		snd_ensoniq_midi_interrupt(ensoniq);
	if ((status & ES_DAC2) && ensoniq->playback2_substream)
		snd_pcm_period_elapsed(ensoniq->playback2_substream);
	if ((status & ES_ADC) && ensoniq->capture_substream)
		snd_pcm_period_elapsed(ensoniq->capture_substream);
	if ((status & ES_DAC1) && ensoniq->playback1_substream)
		snd_pcm_period_elapsed(ensoniq->playback1_substream);
	return IRQ_HANDLED;
}

static int __devinit snd_audiopci_probe(struct pci_dev *pci,
					const struct pci_device_id *pci_id)
{
	static int dev;
	struct snd_card *card;
	struct ensoniq *ensoniq;
	int err, pcm_devs[2];

	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) {
		dev++;
		return -ENOENT;
	}

	err = snd_card_create(index[dev], id[dev], THIS_MODULE, 0, &card);
	if (err < 0)
		return err;

	if ((err = snd_ensoniq_create(card, pci, &ensoniq)) < 0) {
		snd_card_free(card);
		return err;
	}
	card->private_data = ensoniq;

	pcm_devs[0] = 0; pcm_devs[1] = 1;
#ifdef CHIP1370
	if ((err = snd_ensoniq_1370_mixer(ensoniq)) < 0) {
		snd_card_free(card);
		return err;
	}
#endif
#ifdef CHIP1371
	if ((err = snd_ensoniq_1371_mixer(ensoniq, spdif[dev], lineio[dev])) < 0) {
		snd_card_free(card);
		return err;
	}
#endif
	if ((err = snd_ensoniq_pcm(ensoniq, 0, NULL)) < 0) {
		snd_card_free(card);
		return err;
	}
	if ((err = snd_ensoniq_pcm2(ensoniq, 1, NULL)) < 0) {
		snd_card_free(card);
		return err;
	}
	if ((err = snd_ensoniq_midi(ensoniq, 0, NULL)) < 0) {
		snd_card_free(card);
		return err;
	}

	snd_ensoniq_create_gameport(ensoniq, dev);

	strcpy(card->driver, DRIVER_NAME);

	strcpy(card->shortname, "Ensoniq AudioPCI");
	sprintf(card->longname, "%s %s at 0x%lx, irq %i",
		card->shortname,
		card->driver,
		ensoniq->port,
		ensoniq->irq);

	if ((err = snd_card_register(card)) < 0) {
		snd_card_free(card);
		return err;
	}

	pci_set_drvdata(pci, card);
	dev++;
	return 0;
}

static void __devexit snd_audiopci_remove(struct pci_dev *pci)
{
	snd_card_free(pci_get_drvdata(pci));
	pci_set_drvdata(pci, NULL);
}

static struct pci_driver driver = {
	.name = KBUILD_MODNAME,
	.id_table = snd_audiopci_ids,
	.probe = snd_audiopci_probe,
	.remove = __devexit_p(snd_audiopci_remove),
#ifdef CONFIG_PM
	.suspend = snd_ensoniq_suspend,
	.resume = snd_ensoniq_resume,
#endif
};
	
static int __init alsa_card_ens137x_init(void)
{
	return pci_register_driver(&driver);
}

static void __exit alsa_card_ens137x_exit(void)
{
	pci_unregister_driver(&driver);
}

module_init(alsa_card_ens137x_init)
module_exit(alsa_card_ens137x_exit)
