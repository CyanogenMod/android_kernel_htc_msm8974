/*
 * Freescale DMA ALSA SoC PCM driver
 *
 * Author: Timur Tabi <timur@freescale.com>
 *
 * Copyright 2007-2010 Freescale Semiconductor, Inc.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 * This driver implements ASoC support for the Elo DMA controller, which is
 * the DMA controller on Freescale 83xx, 85xx, and 86xx SOCs. In ALSA terms,
 * the PCM driver is what handles the DMA buffer.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gfp.h>
#include <linux/of_platform.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <asm/io.h>

#include "fsl_dma.h"
#include "fsl_ssi.h"	

#define FSLDMA_PCM_FORMATS (SNDRV_PCM_FMTBIT_S8 	| \
			    SNDRV_PCM_FMTBIT_U8 	| \
			    SNDRV_PCM_FMTBIT_S16_LE     | \
			    SNDRV_PCM_FMTBIT_S16_BE     | \
			    SNDRV_PCM_FMTBIT_U16_LE     | \
			    SNDRV_PCM_FMTBIT_U16_BE     | \
			    SNDRV_PCM_FMTBIT_S24_LE     | \
			    SNDRV_PCM_FMTBIT_S24_BE     | \
			    SNDRV_PCM_FMTBIT_U24_LE     | \
			    SNDRV_PCM_FMTBIT_U24_BE     | \
			    SNDRV_PCM_FMTBIT_S32_LE     | \
			    SNDRV_PCM_FMTBIT_S32_BE     | \
			    SNDRV_PCM_FMTBIT_U32_LE     | \
			    SNDRV_PCM_FMTBIT_U32_BE)

#define FSLDMA_PCM_RATES (SNDRV_PCM_RATE_5512 | SNDRV_PCM_RATE_8000_192000 | \
			  SNDRV_PCM_RATE_CONTINUOUS)

struct dma_object {
	struct snd_soc_platform_driver dai;
	dma_addr_t ssi_stx_phys;
	dma_addr_t ssi_srx_phys;
	unsigned int ssi_fifo_depth;
	struct ccsr_dma_channel __iomem *channel;
	unsigned int irq;
	bool assigned;
	char path[1];
};

#define NUM_DMA_LINKS   2

struct fsl_dma_private {
	struct fsl_dma_link_descriptor link[NUM_DMA_LINKS];
	struct ccsr_dma_channel __iomem *dma_channel;
	unsigned int irq;
	struct snd_pcm_substream *substream;
	dma_addr_t ssi_sxx_phys;
	unsigned int ssi_fifo_depth;
	dma_addr_t ld_buf_phys;
	unsigned int current_link;
	dma_addr_t dma_buf_phys;
	dma_addr_t dma_buf_next;
	dma_addr_t dma_buf_end;
	size_t period_size;
	unsigned int num_periods;
};

static const struct snd_pcm_hardware fsl_dma_hardware = {

	.info   		= SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_JOINT_DUPLEX |
				  SNDRV_PCM_INFO_PAUSE,
	.formats		= FSLDMA_PCM_FORMATS,
	.rates  		= FSLDMA_PCM_RATES,
	.rate_min       	= 5512,
	.rate_max       	= 192000,
	.period_bytes_min       = 512,  	
	.period_bytes_max       = (u32) -1,
	.periods_min    	= NUM_DMA_LINKS,
	.periods_max    	= (unsigned int) -1,
	.buffer_bytes_max       = 128 * 1024,   
};

static void fsl_dma_abort_stream(struct snd_pcm_substream *substream)
{
	unsigned long flags;

	snd_pcm_stream_lock_irqsave(substream, flags);

	if (snd_pcm_running(substream))
		snd_pcm_stop(substream, SNDRV_PCM_STATE_XRUN);

	snd_pcm_stream_unlock_irqrestore(substream, flags);
}

static void fsl_dma_update_pointers(struct fsl_dma_private *dma_private)
{
	struct fsl_dma_link_descriptor *link =
		&dma_private->link[dma_private->current_link];

	if (dma_private->substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		link->source_addr = cpu_to_be32(dma_private->dma_buf_next);
#ifdef CONFIG_PHYS_64BIT
		link->source_attr = cpu_to_be32(CCSR_DMA_ATR_SNOOP |
			upper_32_bits(dma_private->dma_buf_next));
#endif
	} else {
		link->dest_addr = cpu_to_be32(dma_private->dma_buf_next);
#ifdef CONFIG_PHYS_64BIT
		link->dest_attr = cpu_to_be32(CCSR_DMA_ATR_SNOOP |
			upper_32_bits(dma_private->dma_buf_next));
#endif
	}

	
	dma_private->dma_buf_next += dma_private->period_size;

	if (dma_private->dma_buf_next >= dma_private->dma_buf_end)
		dma_private->dma_buf_next = dma_private->dma_buf_phys;

	if (++dma_private->current_link >= NUM_DMA_LINKS)
		dma_private->current_link = 0;
}

static irqreturn_t fsl_dma_isr(int irq, void *dev_id)
{
	struct fsl_dma_private *dma_private = dev_id;
	struct snd_pcm_substream *substream = dma_private->substream;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct ccsr_dma_channel __iomem *dma_channel = dma_private->dma_channel;
	irqreturn_t ret = IRQ_NONE;
	u32 sr, sr2 = 0;

	sr = in_be32(&dma_channel->sr);

	if (sr & CCSR_DMA_SR_TE) {
		dev_err(dev, "dma transmit error\n");
		fsl_dma_abort_stream(substream);
		sr2 |= CCSR_DMA_SR_TE;
		ret = IRQ_HANDLED;
	}

	if (sr & CCSR_DMA_SR_CH)
		ret = IRQ_HANDLED;

	if (sr & CCSR_DMA_SR_PE) {
		dev_err(dev, "dma programming error\n");
		fsl_dma_abort_stream(substream);
		sr2 |= CCSR_DMA_SR_PE;
		ret = IRQ_HANDLED;
	}

	if (sr & CCSR_DMA_SR_EOLNI) {
		sr2 |= CCSR_DMA_SR_EOLNI;
		ret = IRQ_HANDLED;
	}

	if (sr & CCSR_DMA_SR_CB)
		ret = IRQ_HANDLED;

	if (sr & CCSR_DMA_SR_EOSI) {
		
		snd_pcm_period_elapsed(substream);

		if (dma_private->num_periods != NUM_DMA_LINKS)
			fsl_dma_update_pointers(dma_private);

		sr2 |= CCSR_DMA_SR_EOSI;
		ret = IRQ_HANDLED;
	}

	if (sr & CCSR_DMA_SR_EOLSI) {
		sr2 |= CCSR_DMA_SR_EOLSI;
		ret = IRQ_HANDLED;
	}

	
	if (sr2)
		out_be32(&dma_channel->sr, sr2);

	return ret;
}

static int fsl_dma_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	static u64 fsl_dma_dmamask = DMA_BIT_MASK(36);
	int ret;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &fsl_dma_dmamask;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = fsl_dma_dmamask;


	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, card->dev,
			fsl_dma_hardware.buffer_bytes_max,
			&pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream->dma_buffer);
		if (ret) {
			dev_err(card->dev, "can't alloc playback dma buffer\n");
			return ret;
		}
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, card->dev,
			fsl_dma_hardware.buffer_bytes_max,
			&pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream->dma_buffer);
		if (ret) {
			dev_err(card->dev, "can't alloc capture dma buffer\n");
			snd_dma_free_pages(&pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream->dma_buffer);
			return ret;
		}
	}

	return 0;
}

static int fsl_dma_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct dma_object *dma =
		container_of(rtd->platform->driver, struct dma_object, dai);
	struct fsl_dma_private *dma_private;
	struct ccsr_dma_channel __iomem *dma_channel;
	dma_addr_t ld_buf_phys;
	u64 temp_link;  	
	u32 mr;
	unsigned int channel;
	int ret = 0;
	unsigned int i;

	ret = snd_pcm_hw_constraint_integer(runtime,
		SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_err(dev, "invalid buffer size\n");
		return ret;
	}

	channel = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? 0 : 1;

	if (dma->assigned) {
		dev_err(dev, "dma channel already assigned\n");
		return -EBUSY;
	}

	dma_private = dma_alloc_coherent(dev, sizeof(struct fsl_dma_private),
					 &ld_buf_phys, GFP_KERNEL);
	if (!dma_private) {
		dev_err(dev, "can't allocate dma private data\n");
		return -ENOMEM;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_private->ssi_sxx_phys = dma->ssi_stx_phys;
	else
		dma_private->ssi_sxx_phys = dma->ssi_srx_phys;

	dma_private->ssi_fifo_depth = dma->ssi_fifo_depth;
	dma_private->dma_channel = dma->channel;
	dma_private->irq = dma->irq;
	dma_private->substream = substream;
	dma_private->ld_buf_phys = ld_buf_phys;
	dma_private->dma_buf_phys = substream->dma_buffer.addr;

	ret = request_irq(dma_private->irq, fsl_dma_isr, 0, "fsldma-audio",
			  dma_private);
	if (ret) {
		dev_err(dev, "can't register ISR for IRQ %u (ret=%i)\n",
			dma_private->irq, ret);
		dma_free_coherent(dev, sizeof(struct fsl_dma_private),
			dma_private, dma_private->ld_buf_phys);
		return ret;
	}

	dma->assigned = 1;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	snd_soc_set_runtime_hwparams(substream, &fsl_dma_hardware);
	runtime->private_data = dma_private;

	

	dma_channel = dma_private->dma_channel;

	temp_link = dma_private->ld_buf_phys +
		sizeof(struct fsl_dma_link_descriptor);

	for (i = 0; i < NUM_DMA_LINKS; i++) {
		dma_private->link[i].next = cpu_to_be64(temp_link);

		temp_link += sizeof(struct fsl_dma_link_descriptor);
	}
	
	dma_private->link[i - 1].next = cpu_to_be64(dma_private->ld_buf_phys);

	
	out_be32(&dma_channel->clndar,
		CCSR_DMA_CLNDAR_ADDR(dma_private->ld_buf_phys));
	out_be32(&dma_channel->eclndar,
		CCSR_DMA_ECLNDAR_ADDR(dma_private->ld_buf_phys));

	
	out_be32(&dma_channel->bcr, 0);

	mr = in_be32(&dma_channel->mr) &
		~(CCSR_DMA_MR_CA | CCSR_DMA_MR_DAHE | CCSR_DMA_MR_SAHE);

	mr |= CCSR_DMA_MR_EOSIE | CCSR_DMA_MR_EIE | CCSR_DMA_MR_EMP_EN |
		CCSR_DMA_MR_EMS_EN;

	mr |= (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
		CCSR_DMA_MR_DAHE : CCSR_DMA_MR_SAHE;

	out_be32(&dma_channel->mr, mr);

	return 0;
}

static int fsl_dma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct fsl_dma_private *dma_private = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;

	
	unsigned int sample_bits =
		snd_pcm_format_physical_width(params_format(hw_params));

	
	unsigned int sample_bytes = sample_bits / 8;

	
	dma_addr_t ssi_sxx_phys = dma_private->ssi_sxx_phys;

	
	size_t buffer_size = params_buffer_bytes(hw_params);

	
	size_t period_size = params_period_bytes(hw_params);

	
	dma_addr_t temp_addr = substream->dma_buffer.addr;

	
	struct ccsr_dma_channel __iomem *dma_channel = dma_private->dma_channel;

	u32 mr; 

	unsigned int i;

	
	dma_private->period_size = period_size;
	dma_private->num_periods = params_periods(hw_params);
	dma_private->dma_buf_end = dma_private->dma_buf_phys + buffer_size;
	dma_private->dma_buf_next = dma_private->dma_buf_phys +
		(NUM_DMA_LINKS * period_size);

	if (dma_private->dma_buf_next >= dma_private->dma_buf_end)
		
		dma_private->dma_buf_next = dma_private->dma_buf_phys;

	mr = in_be32(&dma_channel->mr) & ~(CCSR_DMA_MR_BWC_MASK |
		  CCSR_DMA_MR_SAHTS_MASK | CCSR_DMA_MR_DAHTS_MASK);

	switch (sample_bits) {
	case 8:
		mr |= CCSR_DMA_MR_DAHTS_1 | CCSR_DMA_MR_SAHTS_1;
		ssi_sxx_phys += 3;
		break;
	case 16:
		mr |= CCSR_DMA_MR_DAHTS_2 | CCSR_DMA_MR_SAHTS_2;
		ssi_sxx_phys += 2;
		break;
	case 32:
		mr |= CCSR_DMA_MR_DAHTS_4 | CCSR_DMA_MR_SAHTS_4;
		break;
	default:
		
		dev_err(dev, "unsupported sample size %u\n", sample_bits);
		return -EINVAL;
	}

	mr |= CCSR_DMA_MR_BWC((dma_private->ssi_fifo_depth - 2) * sample_bytes);

	out_be32(&dma_channel->mr, mr);

	for (i = 0; i < NUM_DMA_LINKS; i++) {
		struct fsl_dma_link_descriptor *link = &dma_private->link[i];

		link->count = cpu_to_be32(period_size);

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			link->source_addr = cpu_to_be32(temp_addr);
			link->source_attr = cpu_to_be32(CCSR_DMA_ATR_SNOOP |
				upper_32_bits(temp_addr));

			link->dest_addr = cpu_to_be32(ssi_sxx_phys);
			link->dest_attr = cpu_to_be32(CCSR_DMA_ATR_NOSNOOP |
				upper_32_bits(ssi_sxx_phys));
		} else {
			link->source_addr = cpu_to_be32(ssi_sxx_phys);
			link->source_attr = cpu_to_be32(CCSR_DMA_ATR_NOSNOOP |
				upper_32_bits(ssi_sxx_phys));

			link->dest_addr = cpu_to_be32(temp_addr);
			link->dest_attr = cpu_to_be32(CCSR_DMA_ATR_SNOOP |
				upper_32_bits(temp_addr));
		}

		temp_addr += period_size;
	}

	return 0;
}

static snd_pcm_uframes_t fsl_dma_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct fsl_dma_private *dma_private = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct ccsr_dma_channel __iomem *dma_channel = dma_private->dma_channel;
	dma_addr_t position;
	snd_pcm_uframes_t frames;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		position = in_be32(&dma_channel->sar);
#ifdef CONFIG_PHYS_64BIT
		position |= (u64)(in_be32(&dma_channel->satr) &
				  CCSR_DMA_ATR_ESAD_MASK) << 32;
#endif
	} else {
		position = in_be32(&dma_channel->dar);
#ifdef CONFIG_PHYS_64BIT
		position |= (u64)(in_be32(&dma_channel->datr) &
				  CCSR_DMA_ATR_ESAD_MASK) << 32;
#endif
	}

	if (!position)
		return 0;

	if ((position < dma_private->dma_buf_phys) ||
	    (position > dma_private->dma_buf_end)) {
		dev_err(dev, "dma pointer is out of range, halting stream\n");
		return SNDRV_PCM_POS_XRUN;
	}

	frames = bytes_to_frames(runtime, position - dma_private->dma_buf_phys);

	if (frames == runtime->buffer_size)
		frames = 0;

	return frames;
}

static int fsl_dma_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct fsl_dma_private *dma_private = runtime->private_data;

	if (dma_private) {
		struct ccsr_dma_channel __iomem *dma_channel;

		dma_channel = dma_private->dma_channel;

		
		out_be32(&dma_channel->mr, CCSR_DMA_MR_CA);
		out_be32(&dma_channel->mr, 0);

		
		out_be32(&dma_channel->sr, -1);
		out_be32(&dma_channel->clndar, 0);
		out_be32(&dma_channel->eclndar, 0);
		out_be32(&dma_channel->satr, 0);
		out_be32(&dma_channel->sar, 0);
		out_be32(&dma_channel->datr, 0);
		out_be32(&dma_channel->dar, 0);
		out_be32(&dma_channel->bcr, 0);
		out_be32(&dma_channel->nlndar, 0);
		out_be32(&dma_channel->enlndar, 0);
	}

	return 0;
}

static int fsl_dma_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct fsl_dma_private *dma_private = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct dma_object *dma =
		container_of(rtd->platform->driver, struct dma_object, dai);

	if (dma_private) {
		if (dma_private->irq)
			free_irq(dma_private->irq, dma_private);

		if (dma_private->ld_buf_phys) {
			dma_unmap_single(dev, dma_private->ld_buf_phys,
					 sizeof(dma_private->link),
					 DMA_TO_DEVICE);
		}

		
		dma_free_coherent(dev, sizeof(struct fsl_dma_private),
				  dma_private, dma_private->ld_buf_phys);
		substream->runtime->private_data = NULL;
	}

	dma->assigned = 0;

	return 0;
}

static void fsl_dma_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pcm->streams); i++) {
		substream = pcm->streams[i].substream;
		if (substream) {
			snd_dma_free_pages(&substream->dma_buffer);
			substream->dma_buffer.area = NULL;
			substream->dma_buffer.addr = 0;
		}
	}
}

static struct device_node *find_ssi_node(struct device_node *dma_channel_np)
{
	struct device_node *ssi_np, *np;

	for_each_compatible_node(ssi_np, NULL, "fsl,mpc8610-ssi") {
		np = of_parse_phandle(ssi_np, "fsl,playback-dma", 0);
		of_node_put(np);
		if (np == dma_channel_np)
			return ssi_np;

		np = of_parse_phandle(ssi_np, "fsl,capture-dma", 0);
		of_node_put(np);
		if (np == dma_channel_np)
			return ssi_np;
	}

	return NULL;
}

static struct snd_pcm_ops fsl_dma_ops = {
	.open   	= fsl_dma_open,
	.close  	= fsl_dma_close,
	.ioctl  	= snd_pcm_lib_ioctl,
	.hw_params      = fsl_dma_hw_params,
	.hw_free	= fsl_dma_hw_free,
	.pointer	= fsl_dma_pointer,
};

static int __devinit fsl_soc_dma_probe(struct platform_device *pdev)
 {
	struct dma_object *dma;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *ssi_np;
	struct resource res;
	const uint32_t *iprop;
	int ret;

	
	ssi_np = find_ssi_node(np);
	if (!ssi_np) {
		dev_err(&pdev->dev, "cannot find parent SSI node\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(ssi_np, 0, &res);
	if (ret) {
		dev_err(&pdev->dev, "could not determine resources for %s\n",
			ssi_np->full_name);
		of_node_put(ssi_np);
		return ret;
	}

	dma = kzalloc(sizeof(*dma) + strlen(np->full_name), GFP_KERNEL);
	if (!dma) {
		dev_err(&pdev->dev, "could not allocate dma object\n");
		of_node_put(ssi_np);
		return -ENOMEM;
	}

	strcpy(dma->path, np->full_name);
	dma->dai.ops = &fsl_dma_ops;
	dma->dai.pcm_new = fsl_dma_new;
	dma->dai.pcm_free = fsl_dma_free_dma_buffers;

	
	dma->ssi_stx_phys = res.start + offsetof(struct ccsr_ssi, stx0);
	dma->ssi_srx_phys = res.start + offsetof(struct ccsr_ssi, srx0);

	iprop = of_get_property(ssi_np, "fsl,fifo-depth", NULL);
	if (iprop)
		dma->ssi_fifo_depth = be32_to_cpup(iprop);
	else
                
		dma->ssi_fifo_depth = 8;

	of_node_put(ssi_np);

	ret = snd_soc_register_platform(&pdev->dev, &dma->dai);
	if (ret) {
		dev_err(&pdev->dev, "could not register platform\n");
		kfree(dma);
		return ret;
	}

	dma->channel = of_iomap(np, 0);
	dma->irq = irq_of_parse_and_map(np, 0);

	dev_set_drvdata(&pdev->dev, dma);

	return 0;
}

static int __devexit fsl_soc_dma_remove(struct platform_device *pdev)
{
	struct dma_object *dma = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_platform(&pdev->dev);
	iounmap(dma->channel);
	irq_dispose_mapping(dma->irq);
	kfree(dma);

	return 0;
}

static const struct of_device_id fsl_soc_dma_ids[] = {
	{ .compatible = "fsl,ssi-dma-channel", },
	{}
};
MODULE_DEVICE_TABLE(of, fsl_soc_dma_ids);

static struct platform_driver fsl_soc_dma_driver = {
	.driver = {
		.name = "fsl-pcm-audio",
		.owner = THIS_MODULE,
		.of_match_table = fsl_soc_dma_ids,
	},
	.probe = fsl_soc_dma_probe,
	.remove = __devexit_p(fsl_soc_dma_remove),
};

module_platform_driver(fsl_soc_dma_driver);

MODULE_AUTHOR("Timur Tabi <timur@freescale.com>");
MODULE_DESCRIPTION("Freescale Elo DMA ASoC PCM Driver");
MODULE_LICENSE("GPL v2");
