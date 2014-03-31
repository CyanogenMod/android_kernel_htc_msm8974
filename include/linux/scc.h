
#ifndef	_SCC_H
#define	_SCC_H



#define PA0HZP		0x00	
#define EAGLE		0x01    
#define PC100		0x02	
#define PRIMUS		0x04	
#define DRSI		0x08	
#define BAYCOM		0x10	


enum SCC_ioctl_cmds {
	SIOCSCCRESERVED = SIOCDEVPRIVATE,
	SIOCSCCCFG,
	SIOCSCCINI,
	SIOCSCCCHANINI,
	SIOCSCCSMEM,
	SIOCSCCGKISS,
	SIOCSCCSKISS,
	SIOCSCCGSTAT,
	SIOCSCCCAL
};


enum L1_params {
	PARAM_DATA,
	PARAM_TXDELAY,
	PARAM_PERSIST,
	PARAM_SLOTTIME,
	PARAM_TXTAIL,
	PARAM_FULLDUP,
	PARAM_SOFTDCD,		
	PARAM_MUTE,		
	PARAM_DTR,
	PARAM_RTS,
	PARAM_SPEED,
	PARAM_ENDDELAY,		
	PARAM_GROUP,
	PARAM_IDLE,
	PARAM_MIN,
	PARAM_MAXKEY,
	PARAM_WAIT,
	PARAM_MAXDEFER,
	PARAM_TX,
	PARAM_HWEVENT = 31,
	PARAM_RETURN = 255	
};


enum FULLDUP_modes {
	KISS_DUPLEX_HALF,	
	KISS_DUPLEX_FULL,	
	KISS_DUPLEX_LINK,	
	KISS_DUPLEX_OPTIMA	
};


#define TIMER_OFF	65535U	
#define NO_SUCH_PARAM	65534U	


enum HWEVENT_opts {
	HWEV_DCD_ON,
	HWEV_DCD_OFF,
	HWEV_ALL_SENT
};


#define RXGROUP		0100	
#define TXGROUP		0200	


enum CLOCK_sources {
	CLK_DPLL,	
	CLK_EXTERNAL,	
	CLK_DIVIDER,	
			
	CLK_BRG		
			
};


enum TX_state {
	TXS_IDLE,	
	TXS_BUSY,	
	TXS_ACTIVE,	
	TXS_NEWFRAME,	
	TXS_IDLE2,	
	TXS_WAIT,	
	TXS_TIMEOUT	
};

typedef unsigned long io_port;	


struct scc_stat {
        long rxints;            
        long txints;            
        long exints;            
        long spints;            

        long txframes;          
        long rxframes;          
        long rxerrs;            
        long txerrs;		
        
	unsigned int nospace;	
	unsigned int rx_over;	
	unsigned int tx_under;	

	unsigned int tx_state;	
	int tx_queued;		

	unsigned int maxqueue;	
	unsigned int bufsize;	
};

struct scc_modem {
	long speed;		
	char clocksrc;		
	char nrz;			
};

struct scc_kiss_cmd {
	int  	 command;	
	unsigned param;		
};

struct scc_hw_config {
	io_port data_a;		
	io_port ctrl_a;		
	io_port data_b;		
	io_port ctrl_b;		
	io_port vector_latch;	
	io_port	special;	

	int	irq;		
	long	clock;		
	char	option;		

	char brand;		
	char escc;		
};



struct scc_mem_config {
	unsigned int dummy;
	unsigned int bufsize;
};

struct scc_calibrate {
	unsigned int time;
	unsigned char pattern;
};

#ifdef __KERNEL__

enum {TX_OFF, TX_ON};	


#define VECTOR_MASK	0x06
#define TXINT		0x00
#define EXINT		0x02
#define RXINT		0x04
#define SPINT		0x06

#ifdef CONFIG_SCC_DELAY
#define Inb(port)	inb_p(port)
#define Outb(port, val)	outb_p(val, port)
#else
#define Inb(port)	inb(port)
#define Outb(port, val)	outb(val, port)
#endif


struct scc_kiss {
	unsigned char txdelay;		
	unsigned char persist;		
	unsigned char slottime;		
	unsigned char tailtime;		/* Delay after last byte written */
	unsigned char fulldup;		
	unsigned char waittime;		
	unsigned int  maxkeyup;		
	unsigned int  mintime;		
	unsigned int  idletime;		
	unsigned int  maxdefer;		
	unsigned char tx_inhibit;		
	unsigned char group;		
	unsigned char mode;		
	unsigned char softdcd;		
};



struct scc_channel {
	int init;			

	struct net_device *dev;		
	struct net_device_stats dev_stat;

	char brand;			
	long clock;			

	io_port ctrl;			
	io_port	data;			
	io_port special;		
	int irq;			

	char option;
	char enhanced;			

	unsigned char wreg[16]; 	/* Copy of last written value in WRx */
	unsigned char status;		
	unsigned char dcd;		

        struct scc_kiss kiss;		
        struct scc_stat stat;		
        struct scc_modem modem; 	

        struct sk_buff_head tx_queue;	
        struct sk_buff *rx_buff;	
        struct sk_buff *tx_buff;	

	
	struct timer_list tx_t;		
	struct timer_list tx_wdog;	
	
	
	spinlock_t	lock;		
};

#endif 
#endif 
