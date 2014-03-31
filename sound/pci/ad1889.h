/* Analog Devices 1889 audio driver
 * Copyright (C) 2004, Kyle McMartin <kyle@parisc-linux.org>
 */

#ifndef __AD1889_H__
#define __AD1889_H__

#define AD_DS_WSMC	0x00 
#define  AD_DS_WSMC_SYEN 0x0004 
#define  AD_DS_WSMC_SYRQ 0x0030 
#define  AD_DS_WSMC_WA16 0x0100 
#define  AD_DS_WSMC_WAST 0x0200 
#define  AD_DS_WSMC_WAEN 0x0400 
#define  AD_DS_WSMC_WARQ 0x3000 

#define AD_DS_RAMC	0x02 
#define  AD_DS_RAMC_AD16 0x0001 
#define  AD_DS_RAMC_ADST 0x0002 
#define  AD_DS_RAMC_ADEN 0x0004 
#define  AD_DS_RAMC_ACRQ 0x0030 
#define  AD_DS_RAMC_REEN 0x0400 
#define  AD_DS_RAMC_RERQ 0x3000 

#define AD_DS_WADA	0x04 
#define  AD_DS_WADA_RWAM 0x0080 
#define  AD_DS_WADA_RWAA 0x001f 
#define  AD_DS_WADA_LWAM 0x8000 
#define  AD_DS_WADA_LWAA 0x3e00 

#define AD_DS_SYDA	0x06 
#define  AD_DS_SYDA_RSYM 0x0080 
#define  AD_DS_SYDA_RSYA 0x001f 
#define  AD_DS_SYDA_LSYM 0x8000 
#define  AD_DS_SYDA_LSYA 0x3e00 

#define AD_DS_WAS	0x08 
#define  AD_DS_WAS_WAS   0xffff 

#define AD_DS_RES	0x0a 
#define  AD_DS_RES_RES   0xffff 

#define AD_DS_CCS	0x0c 
#define  AD_DS_CCS_ADO   0x0001 
#define  AD_DS_CCS_REO   0x0002 
#define  AD_DS_CCS_SYU   0x0004 
#define  AD_DS_CCS_WAU   0x0008 
#define  AD_DS_CCS_XTD   0x0100 
#define  AD_DS_CCS_PDALL 0x0400 
#define  AD_DS_CCS_CLKEN 0x8000 

#define AD_DMA_RESBA	0x40 
#define AD_DMA_RESCA	0x44 
#define AD_DMA_RESBC	0x48 
#define AD_DMA_RESCC	0x4c 

#define AD_DMA_ADCBA	0x50 
#define AD_DMA_ADCCA	0x54 
#define AD_DMA_ADCBC	0x58 
#define AD_DMA_ADCCC	0x5c 

#define AD_DMA_SYNBA	0x60 
#define AD_DMA_SYNCA	0x64 
#define AD_DMA_SYNBC	0x68 
#define AD_DMA_SYNCC	0x6c 

#define AD_DMA_WAVBA	0x70 
#define AD_DMA_WAVCA	0x74 
#define AD_DMA_WAVBC	0x78 
#define AD_DMA_WAVCC	0x7c 

#define AD_DMA_RESIC	0x80 
#define AD_DMA_RESIB	0x84 

#define AD_DMA_ADCIC	0x88 
#define AD_DMA_ADCIB	0x8c 

#define AD_DMA_SYNIC	0x90 
#define AD_DMA_SYNIB	0x94 

#define AD_DMA_WAVIC	0x98 
#define AD_DMA_WAVIB	0x9c 

#define  AD_DMA_ICC	0xffffff 
#define  AD_DMA_IBC	0xffffff 

#define AD_DMA_ADC	0xa8	
#define AD_DMA_SYNTH	0xb0	
#define AD_DMA_WAV	0xb8	
#define AD_DMA_RES	0xa0	

#define  AD_DMA_SGDE	0x0001 
#define  AD_DMA_LOOP	0x0002 
#define  AD_DMA_IM	0x000c 
#define  AD_DMA_IM_DIS	(~AD_DMA_IM)	
#define  AD_DMA_IM_CNT	0x0004 
#define  AD_DMA_IM_SGD	0x0008 
#define  AD_DMA_IM_EOL	0x000c 
#define  AD_DMA_SGDS	0x0030 
#define  AD_DMA_SFLG	0x0040 
#define  AD_DMA_EOL	0x0080 

#define AD_DMA_DISR	0xc0 
#define  AD_DMA_DISR_RESI 0x000001 
#define  AD_DMA_DISR_ADCI 0x000002 
#define  AD_DMA_DISR_SYNI 0x000004 
#define  AD_DMA_DISR_WAVI 0x000008 
#define  AD_DMA_DISR_SEPS 0x000040 
#define  AD_DMA_DISR_PMAI 0x004000 
#define  AD_DMA_DISR_PTAI 0x008000 
#define  AD_DMA_DISR_PTAE 0x010000 
#define  AD_DMA_DISR_PMAE 0x020000 

#define  AD_INTR_MASK     (AD_DMA_DISR_RESI|AD_DMA_DISR_ADCI| \
                           AD_DMA_DISR_WAVI|AD_DMA_DISR_SYNI| \
                           AD_DMA_DISR_PMAI|AD_DMA_DISR_PTAI)

#define AD_DMA_CHSS	0xc4 
#define  AD_DMA_CHSS_RESS 0x000001 
#define  AD_DMA_CHSS_ADCS 0x000002 
#define  AD_DMA_CHSS_SYNS 0x000004 
#define  AD_DMA_CHSS_WAVS 0x000008 

#define AD_GPIO_IPC	0xc8	
#define AD_GPIO_OP	0xca	
#define AD_GPIO_IP	0xcc	

#define AD_AC97_BASE	0x100	

#define AD_AC97_RESET   0x100   

#define AD_AC97_PWR_CTL	0x126	
#define  AD_AC97_PWR_ADC 0x0001 
#define  AD_AC97_PWR_DAC 0x0002 
#define  AD_AC97_PWR_PR0 0x0100 
#define  AD_AC97_PWR_PR1 0x0200 

#define AD_MISC_CTL     0x176 
#define  AD_MISC_CTL_DACZ   0x8000 
#define  AD_MISC_CTL_ARSR   0x0001 
#define  AD_MISC_CTL_ALSR   0x0100
#define  AD_MISC_CTL_DLSR   0x0400
#define  AD_MISC_CTL_DRSR   0x0004

#define AD_AC97_SR0     0x178 
#define  AD_AC97_SR0_48K 0xbb80 
#define AD_AC97_SR1     0x17a 

#define AD_AC97_ACIC	0x180 
#define  AD_AC97_ACIC_ACIE  0x0001 
#define  AD_AC97_ACIC_ACRD  0x0002 
#define  AD_AC97_ACIC_ASOE  0x0004 
#define  AD_AC97_ACIC_VSRM  0x0008 
#define  AD_AC97_ACIC_FSDH  0x0100 
#define  AD_AC97_ACIC_FSYH  0x0200 
#define  AD_AC97_ACIC_ACRDY 0x8000 


#define AD_DS_MEMSIZE	512
#define AD_OPL_MEMSIZE	16
#define AD_MIDI_MEMSIZE	16

#define AD_WAV_STATE	0
#define AD_ADC_STATE	1
#define AD_MAX_STATES	2

#define AD_CHAN_WAV	0x0001
#define AD_CHAN_ADC	0x0002
#define AD_CHAN_RES	0x0004
#define AD_CHAN_SYN	0x0008


#define BUFFER_BYTES_MAX	(256 * 1024)
#define PERIOD_BYTES_MIN	32
#define PERIOD_BYTES_MAX	(BUFFER_BYTES_MAX / 2)
#define PERIODS_MIN		2
#define PERIODS_MAX		(BUFFER_BYTES_MAX / PERIOD_BYTES_MIN)

#endif 
