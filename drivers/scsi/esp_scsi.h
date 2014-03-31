/* esp_scsi.h: Defines and structures for the ESP drier.
 *
 * Copyright (C) 2007 David S. Miller (davem@davemloft.net)
 */

#ifndef _ESP_SCSI_H
#define _ESP_SCSI_H

					
#define ESP_TCLOW	0x00UL		
#define ESP_TCMED	0x01UL		
#define ESP_FDATA	0x02UL		
#define ESP_CMD		0x03UL		
#define ESP_STATUS	0x04UL		
#define ESP_BUSID	ESP_STATUS	
#define ESP_INTRPT	0x05UL		
#define ESP_TIMEO	ESP_INTRPT	
#define ESP_SSTEP	0x06UL		
#define ESP_STP		ESP_SSTEP	
#define ESP_FFLAGS	0x07UL		
#define ESP_SOFF	ESP_FFLAGS	
#define ESP_CFG1	0x08UL		
#define ESP_CFACT	0x09UL		
#define ESP_STATUS2	ESP_CFACT	
#define ESP_CTEST	0x0aUL		
#define ESP_CFG2	0x0bUL		
#define ESP_CFG3	0x0cUL		
#define ESP_TCHI	0x0eUL		
#define ESP_UID		ESP_TCHI	
#define FAS_RLO		ESP_TCHI	
#define ESP_FGRND	0x0fUL		
#define FAS_RHI		ESP_FGRND	

#define SBUS_ESP_REG_SIZE	0x40UL


#define ESP_CONFIG1_ID        0x07      
#define ESP_CONFIG1_CHTEST    0x08      
#define ESP_CONFIG1_PENABLE   0x10      
#define ESP_CONFIG1_PARTEST   0x20      
#define ESP_CONFIG1_SRRDISAB  0x40      
#define ESP_CONFIG1_SLCABLE   0x80      

#define ESP_CONFIG2_DMAPARITY 0x01      
#define ESP_CONFIG2_REGPARITY 0x02      
#define ESP_CONFIG2_BADPARITY 0x04      
#define ESP_CONFIG2_SCSI2ENAB 0x08      
#define ESP_CONFIG2_HI        0x10      
#define ESP_CONFIG2_HMEFENAB  0x10      
#define ESP_CONFIG2_BCM       0x20      
#define ESP_CONFIG2_DISPINT   0x20      
#define ESP_CONFIG2_FENAB     0x40      
#define ESP_CONFIG2_SPL       0x40      
#define ESP_CONFIG2_MKDONE    0x40      
#define ESP_CONFIG2_HME32     0x80      
#define ESP_CONFIG2_MAGIC     0xe0      

#define ESP_CONFIG3_FCLOCK    0x01     
#define ESP_CONFIG3_TEM       0x01     
#define ESP_CONFIG3_FAST      0x02     
#define ESP_CONFIG3_ADMA      0x02     
#define ESP_CONFIG3_TENB      0x04     
#define ESP_CONFIG3_SRB       0x04     
#define ESP_CONFIG3_TMS       0x08     
#define ESP_CONFIG3_FCLK      0x08     
#define ESP_CONFIG3_IDMSG     0x10     
#define ESP_CONFIG3_FSCSI     0x10     
#define ESP_CONFIG3_GTM       0x20     
#define ESP_CONFIG3_IDBIT3    0x20     
#define ESP_CONFIG3_TBMS      0x40     
#define ESP_CONFIG3_EWIDE     0x40     
#define ESP_CONFIG3_IMS       0x80     
#define ESP_CONFIG3_OBPUSH    0x80     

#define ESP_CMD_NULL          0x00     
#define ESP_CMD_FLUSH         0x01     
#define ESP_CMD_RC            0x02     
#define ESP_CMD_RS            0x03     

#define ESP_CMD_TI            0x10     
#define ESP_CMD_ICCSEQ        0x11     
#define ESP_CMD_MOK           0x12     
#define ESP_CMD_TPAD          0x18     
#define ESP_CMD_SATN          0x1a     
#define ESP_CMD_RATN          0x1b     

#define ESP_CMD_SMSG          0x20     
#define ESP_CMD_SSTAT         0x21     
#define ESP_CMD_SDATA         0x22     
#define ESP_CMD_DSEQ          0x23     
#define ESP_CMD_TSEQ          0x24     
#define ESP_CMD_TCCSEQ        0x25     
#define ESP_CMD_DCNCT         0x27     
#define ESP_CMD_RMSG          0x28     
#define ESP_CMD_RCMD          0x29     
#define ESP_CMD_RDATA         0x2a     
#define ESP_CMD_RCSEQ         0x2b     

#define ESP_CMD_RSEL          0x40     
#define ESP_CMD_SEL           0x41     
#define ESP_CMD_SELA          0x42     
#define ESP_CMD_SELAS         0x43     
#define ESP_CMD_ESEL          0x44     
#define ESP_CMD_DSEL          0x45     
#define ESP_CMD_SA3           0x46     
#define ESP_CMD_RSEL3         0x47     

#define ESP_CMD_DMA           0x80     

#define ESP_STAT_PIO          0x01     
#define ESP_STAT_PCD          0x02     
#define ESP_STAT_PMSG         0x04     
#define ESP_STAT_PMASK        0x07     
#define ESP_STAT_TDONE        0x08     
#define ESP_STAT_TCNT         0x10     
#define ESP_STAT_PERR         0x20     
#define ESP_STAT_SPAM         0x40     
#define ESP_STAT_INTR         0x80             

#define ESP_DOP   (0)                                       
#define ESP_DIP   (ESP_STAT_PIO)                            
#define ESP_CMDP  (ESP_STAT_PCD)                            
#define ESP_STATP (ESP_STAT_PCD|ESP_STAT_PIO)               
#define ESP_MOP   (ESP_STAT_PMSG|ESP_STAT_PCD)              
#define ESP_MIP   (ESP_STAT_PMSG|ESP_STAT_PCD|ESP_STAT_PIO) 

#define ESP_STAT2_SCHBIT      0x01 
#define ESP_STAT2_FFLAGS      0x02 
#define ESP_STAT2_XCNT        0x04 
#define ESP_STAT2_CREGA       0x08 
#define ESP_STAT2_WIDE        0x10 
#define ESP_STAT2_F1BYTE      0x20 
#define ESP_STAT2_FMSB        0x40 
#define ESP_STAT2_FEMPTY      0x80 

#define ESP_INTR_S            0x01     
#define ESP_INTR_SATN         0x02     
#define ESP_INTR_RSEL         0x04     
#define ESP_INTR_FDONE        0x08     
#define ESP_INTR_BSERV        0x10     
#define ESP_INTR_DC           0x20     
#define ESP_INTR_IC           0x40     
#define ESP_INTR_SR           0x80     

#define ESP_STEP_VBITS        0x07     
#define ESP_STEP_ASEL         0x00     
#define ESP_STEP_SID          0x01     
#define ESP_STEP_NCMD         0x02     
#define ESP_STEP_PPC          0x03     
#define ESP_STEP_FINI4        0x04     

#define ESP_STEP_FINI5        0x05
#define ESP_STEP_FINI6        0x06
#define ESP_STEP_FINI7        0x07

#define ESP_TEST_TARG         0x01     
#define ESP_TEST_INI          0x02     
#define ESP_TEST_TS           0x04     

#define ESP_UID_F100A         0x00     
#define ESP_UID_F236          0x02     
#define ESP_UID_REV           0x07     
#define ESP_UID_FAM           0xf8     

#define ESP_FF_FBYTES         0x1f     
#define ESP_FF_ONOTZERO       0x20     
#define ESP_FF_SSTEP          0xe0     

#define ESP_CCF_F0            0x00     
#define ESP_CCF_NEVER         0x01     
#define ESP_CCF_F2            0x02     
#define ESP_CCF_F3            0x03     
#define ESP_CCF_F4            0x04     
#define ESP_CCF_F5            0x05     
#define ESP_CCF_F6            0x06     
#define ESP_CCF_F7            0x07     

#define ESP_BUSID_RESELID     0x10
#define ESP_BUSID_CTR32BIT    0x40

#define ESP_BUS_TIMEOUT        250     
#define ESP_TIMEO_CONST       8192
#define ESP_NEG_DEFP(mhz, cfact) \
        ((ESP_BUS_TIMEOUT * ((mhz) / 1000)) / (8192 * (cfact)))
#define ESP_HZ_TO_CYCLE(hertz)  ((1000000000) / ((hertz) / 1000))
#define ESP_TICK(ccf, cycle)  ((7682 * (ccf) * (cycle) / 1000))

#define SYNC_DEFP_SLOW            0x32   
#define SYNC_DEFP_FAST            0x19   

struct esp_cmd_priv {
	union {
		dma_addr_t	dma_addr;
		int		num_sg;
	} u;

	int			cur_residue;
	struct scatterlist	*cur_sg;
	int			tot_residue;
};
#define ESP_CMD_PRIV(CMD)	((struct esp_cmd_priv *)(&(CMD)->SCp))

enum esp_rev {
	ESP100     = 0x00,  
	ESP100A    = 0x01,  
	ESP236     = 0x02,
	FAS236     = 0x03,
	FAS100A    = 0x04,
	FAST       = 0x05,
	FASHME     = 0x06,
};

struct esp_cmd_entry {
	struct list_head	list;

	struct scsi_cmnd	*cmd;

	unsigned int		saved_cur_residue;
	struct scatterlist	*saved_cur_sg;
	unsigned int		saved_tot_residue;

	u8			flags;
#define ESP_CMD_FLAG_WRITE	0x01 
#define ESP_CMD_FLAG_ABORT	0x02 
#define ESP_CMD_FLAG_AUTOSENSE	0x04 

	u8			tag[2];

	u8			status;
	u8			message;

	unsigned char		*sense_ptr;
	unsigned char		*saved_sense_ptr;
	dma_addr_t		sense_dma;

	struct completion	*eh_done;
};

#define ESP_DEFAULT_TAGS	16

#define ESP_MAX_TARGET		16
#define ESP_MAX_LUN		8
#define ESP_MAX_TAG		256

struct esp_lun_data {
	struct esp_cmd_entry	*non_tagged_cmd;
	int			num_tagged;
	int			hold;
	struct esp_cmd_entry	*tagged_cmds[ESP_MAX_TAG];
};

struct esp_target_data {
	u8			esp_period;
	u8			esp_offset;
	u8			esp_config3;

	u8			flags;
#define ESP_TGT_WIDE		0x01
#define ESP_TGT_DISCONNECT	0x02
#define ESP_TGT_NEGO_WIDE	0x04
#define ESP_TGT_NEGO_SYNC	0x08
#define ESP_TGT_CHECK_NEGO	0x40
#define ESP_TGT_BROKEN		0x80

	u8			nego_goal_period;
	u8			nego_goal_offset;
	u8			nego_goal_width;
	u8			nego_goal_tags;

	struct scsi_target	*starget;
};

struct esp_event_ent {
	u8			type;
#define ESP_EVENT_TYPE_EVENT	0x01
#define ESP_EVENT_TYPE_CMD	0x02
	u8			val;

	u8			sreg;
	u8			seqreg;
	u8			sreg2;
	u8			ireg;
	u8			select_state;
	u8			event;
	u8			__pad;
};

struct esp;
struct esp_driver_ops {
	void (*esp_write8)(struct esp *esp, u8 val, unsigned long reg);
	u8 (*esp_read8)(struct esp *esp, unsigned long reg);

	dma_addr_t (*map_single)(struct esp *esp, void *buf,
				 size_t sz, int dir);
	int (*map_sg)(struct esp *esp, struct scatterlist *sg,
		      int num_sg, int dir);
	void (*unmap_single)(struct esp *esp, dma_addr_t addr,
			     size_t sz, int dir);
	void (*unmap_sg)(struct esp *esp, struct scatterlist *sg,
			 int num_sg, int dir);

	int (*irq_pending)(struct esp *esp);

	u32 (*dma_length_limit)(struct esp *esp, u32 dma_addr,
				u32 dma_len);

	void (*reset_dma)(struct esp *esp);

	void (*dma_drain)(struct esp *esp);

	
	void (*dma_invalidate)(struct esp *esp);

	void (*send_dma_cmd)(struct esp *esp, u32 dma_addr, u32 esp_count,
			     u32 dma_count, int write, u8 cmd);

	int (*dma_error)(struct esp *esp);
};

#define ESP_MAX_MSG_SZ		8
#define ESP_EVENT_LOG_SZ	32

#define ESP_QUICKIRQ_LIMIT	100
#define ESP_RESELECT_TAG_LIMIT	2500

struct esp {
	void __iomem		*regs;
	void __iomem		*dma_regs;

	const struct esp_driver_ops *ops;

	struct Scsi_Host	*host;
	void			*dev;

	struct esp_cmd_entry	*active_cmd;

	struct list_head	queued_cmds;
	struct list_head	active_cmds;

	u8			*command_block;
	dma_addr_t		command_block_dma;

	unsigned int		data_dma_len;

	u8			sreg;
	u8			seqreg;
	u8			sreg2;
	u8			ireg;

	u32			prev_hme_dmacsr;
	u8			prev_soff;
	u8			prev_stp;
	u8			prev_cfg3;
	u8			__pad;

	struct list_head	esp_cmd_pool;

	struct esp_target_data	target[ESP_MAX_TARGET];

	int			fifo_cnt;
	u8			fifo[16];

	struct esp_event_ent	esp_event_log[ESP_EVENT_LOG_SZ];
	int			esp_event_cur;

	u8			msg_out[ESP_MAX_MSG_SZ];
	int			msg_out_len;

	u8			msg_in[ESP_MAX_MSG_SZ];
	int			msg_in_len;

	u8			bursts;
	u8			config1;
	u8			config2;

	u8			scsi_id;
	u32			scsi_id_mask;

	enum esp_rev		rev;

	u32			flags;
#define ESP_FLAG_DIFFERENTIAL	0x00000001
#define ESP_FLAG_RESETTING	0x00000002
#define ESP_FLAG_DOING_SLOWCMD	0x00000004
#define ESP_FLAG_WIDE_CAPABLE	0x00000008
#define ESP_FLAG_QUICKIRQ_CHECK	0x00000010
#define ESP_FLAG_DISABLE_SYNC	0x00000020

	u8			select_state;
#define ESP_SELECT_NONE		0x00 
#define ESP_SELECT_BASIC	0x01 
#define ESP_SELECT_MSGOUT	0x02 

	
	u8			event;
#define ESP_EVENT_NONE		0x00
#define ESP_EVENT_CMD_START	0x01
#define ESP_EVENT_CMD_DONE	0x02
#define ESP_EVENT_DATA_IN	0x03
#define ESP_EVENT_DATA_OUT	0x04
#define ESP_EVENT_DATA_DONE	0x05
#define ESP_EVENT_MSGIN		0x06
#define ESP_EVENT_MSGIN_MORE	0x07
#define ESP_EVENT_MSGIN_DONE	0x08
#define ESP_EVENT_MSGOUT	0x09
#define ESP_EVENT_MSGOUT_DONE	0x0a
#define ESP_EVENT_STATUS	0x0b
#define ESP_EVENT_FREE_BUS	0x0c
#define ESP_EVENT_CHECK_PHASE	0x0d
#define ESP_EVENT_RESET		0x10

	
	u32			cfact;
	u32			cfreq;
	u32			ccycle;
	u32			ctick;
	u32			neg_defp;
	u32			sync_defp;

	
	u32			max_period;
	u32			min_period;
	u32			radelay;

	
	u8			*cmd_bytes_ptr;
	int			cmd_bytes_left;

	struct completion	*eh_reset;

	void			*dma;
	int			dmarev;
};

extern struct scsi_host_template scsi_esp_template;
extern int scsi_esp_register(struct esp *, struct device *);

extern void scsi_esp_unregister(struct esp *);
extern irqreturn_t scsi_esp_intr(int, void *);
extern void scsi_esp_cmd(struct esp *, u8);

#endif 
