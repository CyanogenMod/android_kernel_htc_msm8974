
#define DEBUG_L1OIP_INIT	0x00010000
#define DEBUG_L1OIP_SOCKET	0x00020000
#define DEBUG_L1OIP_MGR		0x00040000
#define DEBUG_L1OIP_MSG		0x00080000


#define L1OIP_MAX_LEN		2048		
#define L1OIP_MAX_PERFRAME	1400		


#define L1OIP_KEEPALIVE		15
#define L1OIP_TIMEOUT		65


#define L1OIP_DEFAULTPORT	931


struct l1oip_chan {
	struct dchannel		*dch;
	struct bchannel		*bch;
	u32			tx_counter;	
	u32			rx_counter;	
	u32			codecstate;	
#ifdef REORDER_DEBUG
	int			disorder_flag;
	struct sk_buff		*disorder_skb;
	u32			disorder_cnt;
#endif
};


struct l1oip {
	struct list_head        list;

	
	int			registered;	
	char			name[MISDN_MAX_IDLEN];
	int			idx;		
	int			pri;		
	int			d_idx;		
	int			b_num;		
	u32			id;		
	int			ondemand;	
	int			bundle;		
	int			codec;		
	int			limit;		

	
	struct timer_list	keep_tl;
	struct timer_list	timeout_tl;
	int			timeout_on;
	struct work_struct	workq;

	
	struct socket		*socket;	
	struct completion	socket_complete;
	struct task_struct	*socket_thread;
	spinlock_t		socket_lock;	
	u32			remoteip;	
	u16			localport;	
	u16			remoteport;	
	struct sockaddr_in	sin_local;	
	struct sockaddr_in	sin_remote;	
	struct msghdr		sendmsg;	
	struct kvec		sendiov;	

	
	struct l1oip_chan	chan[128];	
};

extern int l1oip_law_to_4bit(u8 *data, int len, u8 *result, u32 *state);
extern int l1oip_4bit_to_law(u8 *data, int len, u8 *result);
extern int l1oip_alaw_to_ulaw(u8 *data, int len, u8 *result);
extern int l1oip_ulaw_to_alaw(u8 *data, int len, u8 *result);
extern void l1oip_4bit_free(void);
extern int l1oip_4bit_alloc(int ulaw);
