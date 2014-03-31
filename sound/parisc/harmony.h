/* Hewlett-Packard Harmony audio driver
 * Copyright (C) 2004, Kyle McMartin <kyle@parisc-linux.org>
 */

#ifndef __HARMONY_H__
#define __HARMONY_H__

struct harmony_buffer {
        unsigned long addr;
        int buf;
        int count;
        int size;
        int coherent;
};

struct snd_harmony {
        int irq;

        unsigned long hpa; 
        void __iomem *iobase; 

        struct parisc_device *dev;

        struct {
                u32 gain;
                u32 rate;
                u32 format;
                u32 stereo;
		int playing;
		int capturing;
        } st;

        struct snd_dma_device dma; 
        struct harmony_buffer pbuf;
	struct harmony_buffer cbuf;

        struct snd_dma_buffer gdma; 
        struct snd_dma_buffer sdma; 

        struct {
                unsigned long play_intr;
	        unsigned long rec_intr;
                unsigned long graveyard_intr;
                unsigned long silence_intr;
        } stats;

        struct snd_pcm *pcm;
        struct snd_card *card;
        struct snd_pcm_substream *psubs;
	struct snd_pcm_substream *csubs;
        struct snd_info_entry *proc;

        spinlock_t lock;
        spinlock_t mixer_lock;
};

#define MAX_PCM_DEVICES     1
#define MAX_PCM_SUBSTREAMS  4
#define MAX_MIDI_DEVICES    0

#define HARMONY_SIZE       64

#define BUF_SIZE     PAGE_SIZE
#define MAX_BUFS     16
#define MAX_BUF_SIZE (MAX_BUFS * BUF_SIZE)

#define PLAYBACK_BUFS    MAX_BUFS
#define RECORD_BUFS      MAX_BUFS
#define GRAVEYARD_BUFS   1
#define GRAVEYARD_BUFSZ  (GRAVEYARD_BUFS*BUF_SIZE)
#define SILENCE_BUFS     1
#define SILENCE_BUFSZ    (SILENCE_BUFS*BUF_SIZE)

#define HARMONY_ID       0x000
#define HARMONY_RESET    0x004
#define HARMONY_CNTL     0x008
#define HARMONY_GAINCTL  0x00c
#define HARMONY_PNXTADD  0x010
#define HARMONY_PCURADD  0x014
#define HARMONY_RNXTADD  0x018
#define HARMONY_RCURADD  0x01c
#define HARMONY_DSTATUS  0x020
#define HARMONY_OV       0x024
#define HARMONY_PIO      0x028
#define HARMONY_DIAG     0x03c

#define HARMONY_CNTL_C          0x80000000
#define HARMONY_CNTL_ST         0x00000020
#define HARMONY_CNTL_44100      0x00000015      
#define HARMONY_CNTL_8000       0x00000008      

#define HARMONY_DSTATUS_ID      0x00000000 
#define HARMONY_DSTATUS_PN      0x00000200 
#define HARMONY_DSTATUS_RN      0x00000002 
#define HARMONY_DSTATUS_IE      0x80000000 

#define HARMONY_DF_16BIT_LINEAR 0x00000000
#define HARMONY_DF_8BIT_ULAW    0x00000001
#define HARMONY_DF_8BIT_ALAW    0x00000002

#define HARMONY_SS_MONO         0x00000000
#define HARMONY_SS_STEREO       0x00000001

#define HARMONY_GAIN_SILENCE    0x01F00FFF
#define HARMONY_GAIN_DEFAULT    0x01F00FFF

#define HARMONY_GAIN_HE_SHIFT   27 
#define HARMONY_GAIN_HE_MASK    (1 << HARMONY_GAIN_HE_SHIFT)
#define HARMONY_GAIN_LE_SHIFT   26 
#define HARMONY_GAIN_LE_MASK    (1 << HARMONY_GAIN_LE_SHIFT)
#define HARMONY_GAIN_SE_SHIFT   25 
#define HARMONY_GAIN_SE_MASK    (1 << HARMONY_GAIN_SE_SHIFT)
#define HARMONY_GAIN_IS_SHIFT   24 
#define HARMONY_GAIN_IS_MASK    (1 << HARMONY_GAIN_IS_SHIFT)

#define HARMONY_GAIN_MA         0x0f
#define HARMONY_GAIN_MA_SHIFT   20
#define HARMONY_GAIN_MA_MASK    (HARMONY_GAIN_MA << HARMONY_GAIN_MA_SHIFT)

#define HARMONY_GAIN_IN         0x0f
#define HARMONY_GAIN_LI_SHIFT   16
#define HARMONY_GAIN_LI_MASK    (HARMONY_GAIN_IN << HARMONY_GAIN_LI_SHIFT)
#define HARMONY_GAIN_RI_SHIFT   12
#define HARMONY_GAIN_RI_MASK    (HARMONY_GAIN_IN << HARMONY_GAIN_RI_SHIFT)

#define HARMONY_GAIN_OUT        0x3f
#define HARMONY_GAIN_LO_SHIFT   6
#define HARMONY_GAIN_LO_MASK    (HARMONY_GAIN_OUT << HARMONY_GAIN_LO_SHIFT)
#define HARMONY_GAIN_RO_SHIFT   0
#define HARMONY_GAIN_RO_MASK    (HARMONY_GAIN_OUT << HARMONY_GAIN_RO_SHIFT)

#define HARMONY_MAX_OUT (HARMONY_GAIN_RO_MASK >> HARMONY_GAIN_RO_SHIFT)
#define HARMONY_MAX_IN  (HARMONY_GAIN_RI_MASK >> HARMONY_GAIN_RI_SHIFT)
#define HARMONY_MAX_MON (HARMONY_GAIN_MA_MASK >> HARMONY_GAIN_MA_SHIFT)

#define HARMONY_SR_8KHZ         0x08
#define HARMONY_SR_16KHZ        0x09
#define HARMONY_SR_27KHZ        0x0A
#define HARMONY_SR_32KHZ        0x0B
#define HARMONY_SR_48KHZ        0x0E
#define HARMONY_SR_9KHZ         0x0F
#define HARMONY_SR_5KHZ         0x10
#define HARMONY_SR_11KHZ        0x11
#define HARMONY_SR_18KHZ        0x12
#define HARMONY_SR_22KHZ        0x13
#define HARMONY_SR_37KHZ        0x14
#define HARMONY_SR_44KHZ        0x15
#define HARMONY_SR_33KHZ        0x16
#define HARMONY_SR_6KHZ         0x17

#endif 
