/*
 * Audio support data for ISDN4Linux.
 *
 * Copyright 2002/2003 by Andreas Eversberg (jolly@eversberg.eu)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#define DEBUG_DSP_CTRL		0x0001
#define DEBUG_DSP_CORE		0x0002
#define DEBUG_DSP_DTMF		0x0004
#define DEBUG_DSP_CMX		0x0010
#define DEBUG_DSP_TONE		0x0020
#define DEBUG_DSP_BLOWFISH	0x0040
#define DEBUG_DSP_DELAY		0x0100
#define DEBUG_DSP_CLOCK		0x0200
#define DEBUG_DSP_DTMFCOEFF	0x8000 

#define DSP_OPT_ULAW		(1 << 0)
#define DSP_OPT_NOHARDWARE	(1 << 1)

#include <linux/timer.h>
#include <linux/workqueue.h>

#include "dsp_ecdis.h"

extern int dsp_options;
extern int dsp_debug;
extern int dsp_poll;
extern int dsp_tics;
extern spinlock_t dsp_lock;
extern struct work_struct dsp_workq;
extern u32 dsp_poll_diff; 


extern s32 dsp_audio_alaw_to_s32[256];
extern s32 dsp_audio_ulaw_to_s32[256];
extern s32 *dsp_audio_law_to_s32;
extern u8 dsp_audio_s16_to_law[65536];
extern u8 dsp_audio_alaw_to_ulaw[256];
extern u8 dsp_audio_mix_law[65536];
extern u8 dsp_audio_seven2law[128];
extern u8 dsp_audio_law2seven[256];
extern void dsp_audio_generate_law_tables(void);
extern void dsp_audio_generate_s2law_table(void);
extern void dsp_audio_generate_seven(void);
extern void dsp_audio_generate_mix_table(void);
extern void dsp_audio_generate_ulaw_samples(void);
extern void dsp_audio_generate_volume_changes(void);
extern u8 dsp_silence;



#define MAX_POLL	256	

#define CMX_BUFF_SIZE	0x8000	
#define CMX_BUFF_HALF	0x4000	
#define CMX_BUFF_MASK	0x7fff	

#define MAX_SECONDS_JITTER_CHECK 5

extern struct timer_list dsp_spl_tl;
extern u32 dsp_spl_jiffies;


struct dsp;
struct dsp_conf_member {
	struct list_head	list;
	struct dsp		*dsp;
};

struct dsp_conf {
	struct list_head	list;
	u32			id;
	struct list_head	mlist;
	int			software; 
	int			hardware; 
	
};



#define DSP_DTMF_NPOINTS 102

#define ECHOCAN_BUFF_SIZE 0x400 
#define ECHOCAN_BUFF_MASK 0x3ff 

struct dsp_dtmf {
	int		enable; 
	int		treshold; 
	int		software; 
	int		hardware; 
	int		size; 
	signed short	buffer[DSP_DTMF_NPOINTS];
	
	u8		lastwhat, lastdigit;
	int		count;
	u8		digits[16]; 
};


struct dsp_pipeline {
	rwlock_t  lock;
	struct list_head list;
	int inuse;
};


struct dsp_tone {
	int		software; 
	int		hardware; 
	int		tone;
	void		*pattern;
	int		count;
	int		index;
	struct timer_list tl;
};


struct dsp_echo {
	int		software; 
	int		hardware; 
};


struct dsp {
	struct list_head list;
	struct mISDNchannel	ch;
	struct mISDNchannel	*up;
	unsigned char	name[64];
	int		b_active;
	struct dsp_echo	echo;
	int		rx_disabled; 
	int		rx_is_off; 
	int		tx_mix;
	struct dsp_tone	tone;
	struct dsp_dtmf	dtmf;
	int		tx_volume, rx_volume;

	
	struct work_struct	workq;
	struct sk_buff_head	sendq;
	int		hdlc;	
	int		data_pending;	

	
	u32		conf_id;
	struct dsp_conf	*conf;
	struct dsp_conf_member
	*member;

	
	int		rx_W; 
	int		rx_R; 
	int		rx_init; 
	int		tx_W; 
	int		tx_R; 
	int		rx_delay[MAX_SECONDS_JITTER_CHECK];
	int		tx_delay[MAX_SECONDS_JITTER_CHECK];
	u8		tx_buff[CMX_BUFF_SIZE];
	u8		rx_buff[CMX_BUFF_SIZE];
	int		last_tx; 
	int		cmx_delay; 
	int		tx_dejitter; 
	int		tx_data; 

	
	struct dsp_features features;
	int		features_rx_off; 
	int		features_fill_empty; 
	int		pcm_slot_rx; 
	int		pcm_bank_rx;
	int		pcm_slot_tx;
	int		pcm_bank_tx;
	int		hfc_conf; 

	
	int		bf_enable;
	u32		bf_p[18];
	u32		bf_s[1024];
	int		bf_crypt_pos;
	u8		bf_data_in[9];
	u8		bf_crypt_out[9];
	int		bf_decrypt_in_pos;
	int		bf_decrypt_out_pos;
	u8		bf_crypt_inring[16];
	u8		bf_data_out[9];
	int		bf_sync;

	struct dsp_pipeline
	pipeline;
};


extern void dsp_change_volume(struct sk_buff *skb, int volume);

extern struct list_head dsp_ilist;
extern struct list_head conf_ilist;
extern void dsp_cmx_debug(struct dsp *dsp);
extern void dsp_cmx_hardware(struct dsp_conf *conf, struct dsp *dsp);
extern int dsp_cmx_conf(struct dsp *dsp, u32 conf_id);
extern void dsp_cmx_receive(struct dsp *dsp, struct sk_buff *skb);
extern void dsp_cmx_hdlc(struct dsp *dsp, struct sk_buff *skb);
extern void dsp_cmx_send(void *arg);
extern void dsp_cmx_transmit(struct dsp *dsp, struct sk_buff *skb);
extern int dsp_cmx_del_conf_member(struct dsp *dsp);
extern int dsp_cmx_del_conf(struct dsp_conf *conf);

extern void dsp_dtmf_goertzel_init(struct dsp *dsp);
extern void dsp_dtmf_hardware(struct dsp *dsp);
extern u8 *dsp_dtmf_goertzel_decode(struct dsp *dsp, u8 *data, int len,
				    int fmt);

extern int dsp_tone(struct dsp *dsp, int tone);
extern void dsp_tone_copy(struct dsp *dsp, u8 *data, int len);
extern void dsp_tone_timeout(void *arg);

extern void dsp_bf_encrypt(struct dsp *dsp, u8 *data, int len);
extern void dsp_bf_decrypt(struct dsp *dsp, u8 *data, int len);
extern int dsp_bf_init(struct dsp *dsp, const u8 *key, unsigned int keylen);
extern void dsp_bf_cleanup(struct dsp *dsp);

extern int  dsp_pipeline_module_init(void);
extern void dsp_pipeline_module_exit(void);
extern int  dsp_pipeline_init(struct dsp_pipeline *pipeline);
extern void dsp_pipeline_destroy(struct dsp_pipeline *pipeline);
extern int  dsp_pipeline_build(struct dsp_pipeline *pipeline, const char *cfg);
extern void dsp_pipeline_process_tx(struct dsp_pipeline *pipeline, u8 *data,
				    int len);
extern void dsp_pipeline_process_rx(struct dsp_pipeline *pipeline, u8 *data,
				    int len, unsigned int txlen);
