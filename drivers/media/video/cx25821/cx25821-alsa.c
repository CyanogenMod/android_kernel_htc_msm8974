/*
 *  Driver for the Conexant CX25821 PCIe bridge
 *
 *  Copyright (C) 2009 Conexant Systems Inc.
 *  Authors  <shu.lin@conexant.com>, <hiep.huynh@conexant.com>
 *	Based on SAA713x ALSA driver and CX88 driver
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, version 2
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "cx25821.h"
#include "cx25821-reg.h"

#define AUDIO_SRAM_CHANNEL	SRAM_CH08

#define dprintk(level, fmt, arg...)				\
do {								\
	if (debug >= level)					\
		pr_info("%s/1: " fmt, chip->dev->name, ##arg);	\
} while (0)
#define dprintk_core(level, fmt, arg...)				\
do {									\
	if (debug >= level)						\
		printk(KERN_DEBUG "%s/1: " fmt, chip->dev->name, ##arg); \
} while (0)


static struct snd_card *snd_cx25821_cards[SNDRV_CARDS];
static int devno;

struct cx25821_audio_buffer {
	unsigned int bpl;
	struct btcx_riscmem risc;
	struct videobuf_dmabuf dma;
};

struct cx25821_audio_dev {
	struct cx25821_dev *dev;
	struct cx25821_dmaqueue q;

	
	struct pci_dev *pci;

	
	int irq;

	struct snd_card *card;

	unsigned long iobase;
	spinlock_t reg_lock;
	atomic_t count;

	unsigned int dma_size;
	unsigned int period_size;
	unsigned int num_periods;

	struct videobuf_dmabuf *dma_risc;

	struct cx25821_audio_buffer *buf;

	struct snd_pcm_substream *substream;
};



static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	
static bool enable[SNDRV_CARDS] = { 1, [1 ... (SNDRV_CARDS - 1)] = 1 };

module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable cx25821 soundcard. default enabled.");

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for cx25821 capture interface(s).");


MODULE_DESCRIPTION("ALSA driver module for cx25821 based capture cards");
MODULE_AUTHOR("Hiep Huynh");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{Conexant,25821}");	

static unsigned int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "enable debug messages");

#define AUD_INT_DN_RISCI1       (1 <<  0)
#define AUD_INT_UP_RISCI1       (1 <<  1)
#define AUD_INT_RDS_DN_RISCI1   (1 <<  2)
#define AUD_INT_DN_RISCI2       (1 <<  4)	
#define AUD_INT_UP_RISCI2       (1 <<  5)
#define AUD_INT_RDS_DN_RISCI2   (1 <<  6)
#define AUD_INT_DN_SYNC         (1 << 12)
#define AUD_INT_UP_SYNC         (1 << 13)
#define AUD_INT_RDS_DN_SYNC     (1 << 14)
#define AUD_INT_OPC_ERR         (1 << 16)
#define AUD_INT_BER_IRQ         (1 << 20)
#define AUD_INT_MCHG_IRQ        (1 << 21)
#define GP_COUNT_CONTROL_RESET	0x3

#define PCI_MSK_AUD_EXT   (1 <<  4)
#define PCI_MSK_AUD_INT   (1 <<  3)

static int _cx25821_start_audio_dma(struct cx25821_audio_dev *chip)
{
	struct cx25821_audio_buffer *buf = chip->buf;
	struct cx25821_dev *dev = chip->dev;
	struct sram_channel *audio_ch =
	    &cx25821_sram_channels[AUDIO_SRAM_CHANNEL];
	u32 tmp = 0;

	
	cx25821_set_gpiopin_direction(chip->dev, 0, 0);

	
	cx_clear(AUD_INT_DMA_CTL,
		 FLD_AUD_DST_A_RISC_EN | FLD_AUD_DST_A_FIFO_EN);

	
	cx25821_sram_channel_setup_audio(chip->dev, audio_ch, buf->bpl,
					 buf->risc.dma);

	
	cx_write(AUD_A_LNGTH, buf->bpl);

	
	
	cx_write(AUD_A_GPCNT_CTL, GP_COUNT_CONTROL_RESET);
	atomic_set(&chip->count, 0);

	
	tmp = cx_read(AUD_A_CFG);
	cx_write(AUD_A_CFG, tmp | FLD_AUD_DST_PK_MODE | FLD_AUD_DST_ENABLE |
		 FLD_AUD_CLK_ENABLE);


	
	cx_write(AUD_A_INT_MSK, FLD_AUD_DST_RISCI1 | FLD_AUD_DST_OF |
		 FLD_AUD_DST_SYNC | FLD_AUD_DST_OPC_ERR);

	
	cx_write(AUD_A_INT_STAT, ~0);

	
	cx_set(PCI_INT_MSK, chip->dev->pci_irqmask | PCI_MSK_AUD_INT);

	
	tmp = cx_read(AUD_INT_DMA_CTL);
	cx_set(AUD_INT_DMA_CTL, tmp |
	       (FLD_AUD_DST_A_RISC_EN | FLD_AUD_DST_A_FIFO_EN));

	mdelay(100);
	return 0;
}

static int _cx25821_stop_audio_dma(struct cx25821_audio_dev *chip)
{
	struct cx25821_dev *dev = chip->dev;

	
	cx_clear(AUD_INT_DMA_CTL,
		 FLD_AUD_DST_A_RISC_EN | FLD_AUD_DST_A_FIFO_EN);

	
	cx_clear(PCI_INT_MSK, PCI_MSK_AUD_INT);
	cx_clear(AUD_A_INT_MSK, AUD_INT_OPC_ERR | AUD_INT_DN_SYNC |
		 AUD_INT_DN_RISCI2 | AUD_INT_DN_RISCI1);

	return 0;
}

#define MAX_IRQ_LOOP 50

static char *cx25821_aud_irqs[32] = {
	"dn_risci1", "up_risci1", "rds_dn_risc1",	
	NULL,						
	"dn_risci2", "up_risci2", "rds_dn_risc2",	
	NULL,						
	"dnf_of", "upf_uf", "rds_dnf_uf",		
	NULL,						
	"dn_sync", "up_sync", "rds_dn_sync",		
	NULL,						
	"opc_err", "par_err", "rip_err",		
	"pci_abort", "ber_irq", "mchg_irq"		
};

static void cx25821_aud_irq(struct cx25821_audio_dev *chip, u32 status,
			    u32 mask)
{
	struct cx25821_dev *dev = chip->dev;

	if (0 == (status & mask))
		return;

	cx_write(AUD_A_INT_STAT, status);
	if (debug > 1 || (status & mask & ~0xff))
		cx25821_print_irqbits(dev->name, "irq aud", cx25821_aud_irqs,
				ARRAY_SIZE(cx25821_aud_irqs), status, mask);

	
	if (status & AUD_INT_OPC_ERR) {
		pr_warn("WARNING %s/1: Audio risc op code error\n", dev->name);

		cx_clear(AUD_INT_DMA_CTL,
			 FLD_AUD_DST_A_RISC_EN | FLD_AUD_DST_A_FIFO_EN);
		cx25821_sram_channel_dump_audio(dev,
				&cx25821_sram_channels[AUDIO_SRAM_CHANNEL]);
	}
	if (status & AUD_INT_DN_SYNC) {
		pr_warn("WARNING %s: Downstream sync error!\n", dev->name);
		cx_write(AUD_A_GPCNT_CTL, GP_COUNT_CONTROL_RESET);
		return;
	}

	
	if (status & AUD_INT_DN_RISCI1) {
		atomic_set(&chip->count, cx_read(AUD_A_GPCNT));
		snd_pcm_period_elapsed(chip->substream);
	}
}

static irqreturn_t cx25821_irq(int irq, void *dev_id)
{
	struct cx25821_audio_dev *chip = dev_id;
	struct cx25821_dev *dev = chip->dev;
	u32 status, pci_status;
	u32 audint_status, audint_mask;
	int loop, handled = 0;
	int audint_count = 0;

	audint_status = cx_read(AUD_A_INT_STAT);
	audint_mask = cx_read(AUD_A_INT_MSK);
	audint_count = cx_read(AUD_A_GPCNT);
	status = cx_read(PCI_INT_STAT);

	for (loop = 0; loop < 1; loop++) {
		status = cx_read(PCI_INT_STAT);
		if (0 == status) {
			status = cx_read(PCI_INT_STAT);
			audint_status = cx_read(AUD_A_INT_STAT);
			audint_mask = cx_read(AUD_A_INT_MSK);

			if (status) {
				handled = 1;
				cx_write(PCI_INT_STAT, status);

				cx25821_aud_irq(chip, audint_status,
						audint_mask);
				break;
			} else {
				goto out;
			}
		}

		handled = 1;
		cx_write(PCI_INT_STAT, status);

		cx25821_aud_irq(chip, audint_status, audint_mask);
	}

	pci_status = cx_read(PCI_INT_STAT);

	if (handled)
		cx_write(PCI_INT_STAT, pci_status);

out:
	return IRQ_RETVAL(handled);
}

static int dsp_buffer_free(struct cx25821_audio_dev *chip)
{
	BUG_ON(!chip->dma_size);

	dprintk(2, "Freeing buffer\n");
	videobuf_dma_unmap(&chip->pci->dev, chip->dma_risc);
	videobuf_dma_free(chip->dma_risc);
	btcx_riscmem_free(chip->pci, &chip->buf->risc);
	kfree(chip->buf);

	chip->dma_risc = NULL;
	chip->dma_size = 0;

	return 0;
}


#define DEFAULT_FIFO_SIZE	384
static struct snd_pcm_hardware snd_cx25821_digital_hw = {
	.info = SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_MMAP_VALID,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,

	.rates = SNDRV_PCM_RATE_48000,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
	.period_bytes_min = DEFAULT_FIFO_SIZE / 3,
	.period_bytes_max = DEFAULT_FIFO_SIZE / 3,
	.periods_min = 1,
	.periods_max = AUDIO_LINE_SIZE,
	
	.buffer_bytes_max = (AUDIO_LINE_SIZE * AUDIO_LINE_SIZE),
};

static int snd_cx25821_pcm_open(struct snd_pcm_substream *substream)
{
	struct cx25821_audio_dev *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int err;
	unsigned int bpl = 0;

	if (!chip) {
		pr_err("DEBUG: cx25821 can't find device struct. Can't proceed with open\n");
		return -ENODEV;
	}

	err = snd_pcm_hw_constraint_pow2(runtime, 0,
					 SNDRV_PCM_HW_PARAM_PERIODS);
	if (err < 0)
		goto _error;

	chip->substream = substream;

	runtime->hw = snd_cx25821_digital_hw;

	if (cx25821_sram_channels[AUDIO_SRAM_CHANNEL].fifo_size !=
	    DEFAULT_FIFO_SIZE) {
		
		bpl = cx25821_sram_channels[AUDIO_SRAM_CHANNEL].fifo_size / 3;
		bpl &= ~7;	

		if (bpl > AUDIO_LINE_SIZE)
			bpl = AUDIO_LINE_SIZE;

		runtime->hw.period_bytes_min = bpl;
		runtime->hw.period_bytes_max = bpl;
	}

	return 0;
_error:
	dprintk(1, "Error opening PCM!\n");
	return err;
}

static int snd_cx25821_close(struct snd_pcm_substream *substream)
{
	return 0;
}

static int snd_cx25821_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *hw_params)
{
	struct cx25821_audio_dev *chip = snd_pcm_substream_chip(substream);
	struct videobuf_dmabuf *dma;

	struct cx25821_audio_buffer *buf;
	int ret;

	if (substream->runtime->dma_area) {
		dsp_buffer_free(chip);
		substream->runtime->dma_area = NULL;
	}

	chip->period_size = params_period_bytes(hw_params);
	chip->num_periods = params_periods(hw_params);
	chip->dma_size = chip->period_size * params_periods(hw_params);

	BUG_ON(!chip->dma_size);
	BUG_ON(chip->num_periods & (chip->num_periods - 1));

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (NULL == buf)
		return -ENOMEM;

	if (chip->period_size > AUDIO_LINE_SIZE)
		chip->period_size = AUDIO_LINE_SIZE;

	buf->bpl = chip->period_size;

	dma = &buf->dma;
	videobuf_dma_init(dma);
	ret = videobuf_dma_init_kernel(dma, PCI_DMA_FROMDEVICE,
			(PAGE_ALIGN(chip->dma_size) >> PAGE_SHIFT));
	if (ret < 0)
		goto error;

	ret = videobuf_dma_map(&chip->pci->dev, dma);
	if (ret < 0)
		goto error;

	ret = cx25821_risc_databuffer_audio(chip->pci, &buf->risc, dma->sglist,
			chip->period_size, chip->num_periods, 1);
	if (ret < 0) {
		pr_info("DEBUG: ERROR after cx25821_risc_databuffer_audio()\n");
		goto error;
	}

	
	buf->risc.jmp[0] = cpu_to_le32(RISC_JUMP | RISC_IRQ1 | RISC_CNT_INC);
	buf->risc.jmp[1] = cpu_to_le32(buf->risc.dma);
	buf->risc.jmp[2] = cpu_to_le32(0);	

	chip->buf = buf;
	chip->dma_risc = dma;

	substream->runtime->dma_area = chip->dma_risc->vaddr;
	substream->runtime->dma_bytes = chip->dma_size;
	substream->runtime->dma_addr = 0;

	return 0;

error:
	kfree(buf);
	return ret;
}

static int snd_cx25821_hw_free(struct snd_pcm_substream *substream)
{
	struct cx25821_audio_dev *chip = snd_pcm_substream_chip(substream);

	if (substream->runtime->dma_area) {
		dsp_buffer_free(chip);
		substream->runtime->dma_area = NULL;
	}

	return 0;
}

static int snd_cx25821_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int snd_cx25821_card_trigger(struct snd_pcm_substream *substream,
				    int cmd)
{
	struct cx25821_audio_dev *chip = snd_pcm_substream_chip(substream);
	int err = 0;

	
	spin_lock(&chip->reg_lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		err = _cx25821_start_audio_dma(chip);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		err = _cx25821_stop_audio_dma(chip);
		break;
	default:
		err = -EINVAL;
		break;
	}

	spin_unlock(&chip->reg_lock);

	return err;
}

static snd_pcm_uframes_t snd_cx25821_pointer(struct snd_pcm_substream
					     *substream)
{
	struct cx25821_audio_dev *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	u16 count;

	count = atomic_read(&chip->count);

	return runtime->period_size * (count & (runtime->periods - 1));
}

static struct page *snd_cx25821_page(struct snd_pcm_substream *substream,
				     unsigned long offset)
{
	void *pageptr = substream->runtime->dma_area + offset;

	return vmalloc_to_page(pageptr);
}

static struct snd_pcm_ops snd_cx25821_pcm_ops = {
	.open = snd_cx25821_pcm_open,
	.close = snd_cx25821_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_cx25821_hw_params,
	.hw_free = snd_cx25821_hw_free,
	.prepare = snd_cx25821_prepare,
	.trigger = snd_cx25821_card_trigger,
	.pointer = snd_cx25821_pointer,
	.page = snd_cx25821_page,
};

static int snd_cx25821_pcm(struct cx25821_audio_dev *chip, int device,
			   char *name)
{
	struct snd_pcm *pcm;
	int err;

	err = snd_pcm_new(chip->card, name, device, 0, 1, &pcm);
	if (err < 0) {
		pr_info("ERROR: FAILED snd_pcm_new() in %s\n", __func__);
		return err;
	}
	pcm->private_data = chip;
	pcm->info_flags = 0;
	strcpy(pcm->name, name);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_cx25821_pcm_ops);

	return 0;
}



static DEFINE_PCI_DEVICE_TABLE(cx25821_audio_pci_tbl) = {
	{0x14f1, 0x0920, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0,}
};

MODULE_DEVICE_TABLE(pci, cx25821_audio_pci_tbl);


static void snd_cx25821_dev_free(struct snd_card *card)
{
	struct cx25821_audio_dev *chip = card->private_data;

	
	snd_card_free(chip->card);
}

static int cx25821_audio_initdev(struct cx25821_dev *dev)
{
	struct snd_card *card;
	struct cx25821_audio_dev *chip;
	int err;

	if (devno >= SNDRV_CARDS) {
		pr_info("DEBUG ERROR: devno >= SNDRV_CARDS %s\n", __func__);
		return -ENODEV;
	}

	if (!enable[devno]) {
		++devno;
		pr_info("DEBUG ERROR: !enable[devno] %s\n", __func__);
		return -ENOENT;
	}

	err = snd_card_create(index[devno], id[devno], THIS_MODULE,
			sizeof(struct cx25821_audio_dev), &card);
	if (err < 0) {
		pr_info("DEBUG ERROR: cannot create snd_card_new in %s\n",
			__func__);
		return err;
	}

	strcpy(card->driver, "cx25821");

	
	card->private_free = snd_cx25821_dev_free;
	chip = card->private_data;
	spin_lock_init(&chip->reg_lock);

	chip->dev = dev;
	chip->card = card;
	chip->pci = dev->pci;
	chip->iobase = pci_resource_start(dev->pci, 0);

	chip->irq = dev->pci->irq;

	err = request_irq(dev->pci->irq, cx25821_irq,
			  IRQF_SHARED, chip->dev->name, chip);

	if (err < 0) {
		pr_err("ERROR %s: can't get IRQ %d for ALSA\n", chip->dev->name,
			dev->pci->irq);
		goto error;
	}

	err = snd_cx25821_pcm(chip, 0, "cx25821 Digital");
	if (err < 0) {
		pr_info("DEBUG ERROR: cannot create snd_cx25821_pcm %s\n",
			__func__);
		goto error;
	}

	snd_card_set_dev(card, &chip->pci->dev);

	strcpy(card->shortname, "cx25821");
	sprintf(card->longname, "%s at 0x%lx irq %d", chip->dev->name,
		chip->iobase, chip->irq);
	strcpy(card->mixername, "CX25821");

	pr_info("%s/%i: ALSA support for cx25821 boards\n", card->driver,
		devno);

	err = snd_card_register(card);
	if (err < 0) {
		pr_info("DEBUG ERROR: cannot register sound card %s\n",
			__func__);
		goto error;
	}

	snd_cx25821_cards[devno] = card;

	devno++;
	return 0;

error:
	snd_card_free(card);
	return err;
}

static void cx25821_audio_fini(void)
{
	snd_card_free(snd_cx25821_cards[0]);
}

static int cx25821_alsa_init(void)
{
	struct cx25821_dev *dev = NULL;
	struct list_head *list;

	mutex_lock(&cx25821_devlist_mutex);
	list_for_each(list, &cx25821_devlist) {
		dev = list_entry(list, struct cx25821_dev, devlist);
		cx25821_audio_initdev(dev);
	}
	mutex_unlock(&cx25821_devlist_mutex);

	if (dev == NULL)
		pr_info("ERROR ALSA: no cx25821 cards found\n");

	return 0;

}

late_initcall(cx25821_alsa_init);
module_exit(cx25821_audio_fini);

