
#ifndef __SOUND_SOC_FSL_MPC5200_DMA_H__
#define __SOUND_SOC_FSL_MPC5200_DMA_H__

#define PSC_STREAM_NAME_LEN 32

struct psc_dma_stream {
	struct snd_pcm_runtime *runtime;
	int active;
	struct psc_dma *psc_dma;
	struct bcom_task *bcom_task;
	int irq;
	struct snd_pcm_substream *stream;
	int period_next;
	int period_current;
	int period_bytes;
	int period_count;

	
	u32 ac97_slot_bits;
};

struct psc_dma {
	char name[32];
	struct mpc52xx_psc __iomem *psc_regs;
	struct mpc52xx_psc_fifo __iomem *fifo_regs;
	unsigned int irq;
	struct device *dev;
	spinlock_t lock;
	struct mutex mutex;
	u32 sicr;
	uint sysclk;
	int imr;
	int id;
	unsigned int slots;

	
	struct psc_dma_stream playback;
	struct psc_dma_stream capture;

	
	struct {
		unsigned long overrun_count;
		unsigned long underrun_count;
	} stats;
};

static inline struct psc_dma_stream *
to_psc_dma_stream(struct snd_pcm_substream *substream, struct psc_dma *psc_dma)
{
	if (substream->pstr->stream == SNDRV_PCM_STREAM_CAPTURE)
		return &psc_dma->capture;
	return &psc_dma->playback;
}

#endif 
