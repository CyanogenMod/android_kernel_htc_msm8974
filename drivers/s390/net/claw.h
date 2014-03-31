

#define CCW_CLAW_CMD_WRITE           0x01      
#define CCW_CLAW_CMD_READ            0x02      
#define CCW_CLAW_CMD_NOP             0x03      
#define CCW_CLAW_CMD_SENSE           0x04      
#define CCW_CLAW_CMD_SIGNAL_SMOD     0x05      
#define CCW_CLAW_CMD_TIC             0x08      
#define CCW_CLAW_CMD_READHEADER      0x12      
#define CCW_CLAW_CMD_READFF          0x22      
#define CCW_CLAW_CMD_SENSEID         0xe4      



#define MORE_to_COME_FLAG       0x04   
#define CLAW_IDLE               0x00   
#define CLAW_BUSY               0xff   
#define CLAW_PENDING            0x00   
#define CLAW_COMPLETE           0xff   


#define SYSTEM_VALIDATE_REQUEST   0x01  
#define SYSTEM_VALIDATE_RESPONSE  0x02  
#define CONNECTION_REQUEST        0x21  
#define CONNECTION_RESPONSE       0x22  
#define CONNECTION_CONFIRM        0x23  
#define DISCONNECT                0x24  
#define CLAW_ERROR                0x41  
#define CLAW_VERSION_ID           2     


#define CLAW_ADAPTER_SENSE_BYTE 0x41   


#define CLAW_RC_NAME_MISMATCH       166  
#define CLAW_RC_WRONG_VERSION       167  
#define CLAW_RC_HOST_RCV_TOO_SMALL  180  
					 
                                         


#define HOST_APPL_NAME          "TCPIP   "
#define WS_APPL_NAME_IP_LINK    "TCPIP   "
#define WS_APPL_NAME_IP_NAME	"IP      "
#define WS_APPL_NAME_API_LINK   "API     "
#define WS_APPL_NAME_PACKED     "PACKED  "
#define WS_NAME_NOT_DEF         "NOT_DEF "
#define PACKING_ASK		1
#define PACK_SEND		2
#define DO_PACKED		3

#define MAX_ENVELOPE_SIZE       65536
#define CLAW_DEFAULT_MTU_SIZE   4096
#define DEF_PACK_BUFSIZE	32768
#define READ_CHANNEL		0
#define WRITE_CHANNEL		1

#define TB_TX                   0          
#define TB_STOP                 1          
#define TB_RETRY                2          
#define TB_NOBUFFER             3          
#define CLAW_MAX_LINK_ID        1
#define CLAW_MAX_DEV            256        
#define MAX_NAME_LEN            8          
#define CLAW_FRAME_SIZE         4096
#define CLAW_ID_SIZE		20+3


#define CLAW_STOP                0
#define CLAW_START_HALT_IO       1
#define CLAW_START_SENSEID       2
#define CLAW_START_READ          3
#define CLAW_START_WRITE         4

#define LOCK_YES             0
#define LOCK_NO              1

#define CLAW_DBF_TEXT(level, name, text) \
	do { \
		debug_text_event(claw_dbf_##name, level, text); \
	} while (0)

#define CLAW_DBF_HEX(level,name,addr,len) \
do { \
	debug_event(claw_dbf_##name,level,(void*)(addr),len); \
} while (0)

static inline int claw_dbf_passes(debug_info_t *dbf_grp, int level)
{
	return (level <= dbf_grp->level);
}

#define CLAW_DBF_TEXT_(level,name,text...) \
	do { \
		if (claw_dbf_passes(claw_dbf_##name, level)) { \
			sprintf(debug_buffer, text); \
			debug_text_event(claw_dbf_##name, level, \
						debug_buffer); \
		} \
	} while (0)

enum claw_channel_types {
	
	claw_channel_type_none,

	
	claw_channel_type_claw
};




struct clawh {
        __u16  length;     
        __u8   opcode;     
        __u8   flag;       
};

struct clawph {
       __u16 len;  	
       __u8  flag;  	
       __u8  link_num;	
};

struct endccw {
	__u32     real;            
       __u8      write1;          
        __u8      read1;           
        __u16     reserved;        
        struct ccw1    write1_nop1;
        struct ccw1    write1_nop2;
        struct ccw1    write2_nop1;
        struct ccw1    write2_nop2;
        struct ccw1    read1_nop1;
        struct ccw1    read1_nop2;
        struct ccw1    read2_nop1;
        struct ccw1    read2_nop2;
};

struct ccwbk {
        void   *next;        
        __u32     real;         
        void      *p_buffer;    
        struct clawh     header;       
        struct ccw1    write;   
        struct ccw1    w_read_FF; 
        struct ccw1    w_TIC_1;        
        struct ccw1    read;         
        struct ccw1    read_h;        
        struct ccw1    signal;       
        struct ccw1    r_TIC_1;        
        struct ccw1    r_read_FF;      
        struct ccw1    r_TIC_2;        
};

struct clawctl {
        __u8    command;      
        __u8    version;      
        __u8    linkid;       
        __u8    correlator;   
        __u8    rc;           
        __u8    reserved1;    
        __u8    reserved2;    
        __u8    reserved3;    
        __u8    data[24];     
};

struct sysval  {
        char    WS_name[8];        
        char    host_name[8];      
        __u16   read_frame_size;   
        __u16   write_frame_size;  
        __u8    reserved[4];       
};

struct conncmd  {
        char     WS_name[8];       
        char     host_name[8];     
        __u16    reserved1[2];     
        __u8     reserved2[4];     
};

struct clawwerror  {
        char      reserved1[8];   
        char      reserved2[8];   
        char      reserved3[8];   
};

struct clawbuf  {
       char      buffer[MAX_ENVELOPE_SIZE];   
};


struct chbk {
        unsigned int        devno;
        int                 irq;
	char 		    id[CLAW_ID_SIZE];
       __u32               IO_active;
        __u8                claw_state;
        struct irb          *irb;
       	struct ccw_device   *cdev;  
	struct net_device   *ndev;
        wait_queue_head_t   wait;
        struct tasklet_struct    tasklet;
        struct timer_list   timer;
        unsigned long       flag_a;    
#define CLAW_BH_ACTIVE      0
        unsigned long       flag_b;    
#define CLAW_WRITE_ACTIVE   0
        __u8                last_dstat;
        __u8                flag;
	struct sk_buff_head collect_queue;
	spinlock_t collect_lock;
#define CLAW_WRITE      0x02      
#define CLAW_READ	0x01      
#define CLAW_TIMER      0x80      
};


struct claw_env {
        unsigned int            devno[2];       
        char                    host_name[9];   
        char                    adapter_name [9]; 
        char                    api_type[9];    
        void                    *p_priv;        
        __u16                   read_buffers;   
        __u16                   write_buffers;  
        __u16                   read_size;      
        __u16                   write_size;     
        __u16                   dev_id;         
	__u8			packing;	
        __u8                    in_use;         
        struct net_device       *ndev;    	
};


struct claw_privbk {
        void *p_buff_ccw;
        __u32      p_buff_ccw_num;
        void  *p_buff_read;
        __u32      p_buff_read_num;
        __u32      p_buff_pages_perread;
        void  *p_buff_write;
        __u32      p_buff_write_num;
        __u32      p_buff_pages_perwrite;
        long       active_link_ID;           
        struct ccwbk *p_write_free_chain;     
        struct ccwbk *p_write_active_first;   
        struct ccwbk *p_write_active_last;    
        struct ccwbk *p_read_active_first;    
        struct ccwbk *p_read_active_last;     
        struct endccw *p_end_ccw;              
        struct ccwbk *p_claw_signal_blk;      
        __u32      write_free_count;       
	struct     net_device_stats  stats; 
        struct chbk channel[2];            
        __u8       mtc_skipping;
        int        mtc_offset;
        int        mtc_logical_link;
        void       *p_mtc_envelope;
	struct	   sk_buff	*pk_skb;	
	int	   pk_cnt;
        struct clawctl ctl_bk;
        struct claw_env *p_env;
        __u8       system_validate_comp;
        __u8       release_pend;
        __u8      checksum_received_ip_pkts;
	__u8      buffs_alloc;
        struct endccw  end_ccw;
        unsigned long  tbusy;

};



#define CCWBK_SIZE sizeof(struct ccwbk)


