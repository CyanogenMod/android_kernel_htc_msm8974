/*
 * I/O Processor (IOP) defines and structures, mostly snagged from A/UX
 * header files.
 *
 * The original header from which this was taken is copyrighted. I've done some
 * rewriting (in fact my changes make this a bit more readable, IMHO) but some
 * more should be done.
 */


#define SCC_IOP_BASE_IIFX	(0x50F04000)
#define ISM_IOP_BASE_IIFX	(0x50F12000)

#define SCC_IOP_BASE_QUADRA	(0x50F0C000)
#define ISM_IOP_BASE_QUADRA	(0x50F1E000)


#define	IOP_BYPASS	0x01	
#define	IOP_AUTOINC	0x02	
#define	IOP_RUN		0x04	
#define	IOP_IRQ		0x08	
#define	IOP_INT0	0x10	
#define	IOP_INT1	0x20	
#define	IOP_HWINT	0x40	
#define	IOP_DMAINACTIVE	0x80	

#define NUM_IOPS	2
#define NUM_IOP_CHAN	7
#define NUM_IOP_MSGS	NUM_IOP_CHAN*8
#define IOP_MSG_LEN	32


#define IOP_NUM_SCC	0
#define IOP_NUM_ISM	1


#define IOP_MSG_IDLE		0       
#define IOP_MSG_NEW		1       
#define IOP_MSG_RCVD		2       
#define IOP_MSG_COMPLETE	3       


#define IOP_MSGSTATUS_UNUSED	0	
#define IOP_MSGSTATUS_WAITING	1	
#define IOP_MSGSTATUS_SENT	2	
#define IOP_MSGSTATUS_COMPLETE	3	
#define IOP_MSGSTATUS_UNSOL	6	


#define IOP_ADDR_MAX_SEND_CHAN	0x0200
#define IOP_ADDR_SEND_STATE	0x0201
#define IOP_ADDR_PATCH_CTRL	0x021F
#define IOP_ADDR_SEND_MSG	0x0220
#define IOP_ADDR_MAX_RECV_CHAN	0x0300
#define IOP_ADDR_RECV_STATE	0x0301
#define IOP_ADDR_ALIVE		0x031F
#define IOP_ADDR_RECV_MSG	0x0320

#ifndef __ASSEMBLY__


struct mac_iop {
    __u8	ram_addr_hi;	
    __u8	pad0;
    __u8	ram_addr_lo;	
    __u8	pad1;
    __u8	status_ctrl;	
    __u8	pad2[3];
    __u8	ram_data;	

    __u8	pad3[23];

    

    union {
	struct {		
	    __u8 sccb_cmd;	
	    __u8 pad4;
	    __u8 scca_cmd;	
	    __u8 pad5;
	    __u8 sccb_data;	
	    __u8 pad6;
	    __u8 scca_data;	
	} scc_regs;

	struct {		
	    __u8 wdata;		
	    __u8 pad7;
	    __u8 wmark;		
	    __u8 pad8;
	    __u8 wcrc;		
	    __u8 pad9;
	    __u8 wparams;	
	    __u8 pad10;
	    __u8 wphase;	
	    __u8 pad11;
	    __u8 wsetup;	
	    __u8 pad12;
	    __u8 wzeroes;	
	    __u8 pad13;
	    __u8 wones;		
	    __u8 pad14;
	    __u8 rdata;		
	    __u8 pad15;
	    __u8 rmark;		
	    __u8 pad16;
	    __u8 rerror;	
	    __u8 pad17;
	    __u8 rparams;	
	    __u8 pad18;
	    __u8 rphase;	
	    __u8 pad19;
	    __u8 rsetup;	
	    __u8 pad20;
	    __u8 rmode;		
	    __u8 pad21;
	    __u8 rhandshake;	
	} ism_regs;
    } b;
};


struct iop_msg {
	struct iop_msg	*next;		
	uint	iop_num;		
	uint	channel;		
	void	*caller_priv;		
	int	status;			
	__u8	message[IOP_MSG_LEN];	
	__u8	reply[IOP_MSG_LEN];	
	void	(*handler)(struct iop_msg *);
					
};

extern int iop_scc_present,iop_ism_present;

extern int iop_listen(uint, uint,
			void (*handler)(struct iop_msg *),
			const char *);
extern int iop_send_message(uint, uint, void *, uint, __u8 *,
			    void (*)(struct iop_msg *));
extern void iop_complete_message(struct iop_msg *);
extern void iop_upload_code(uint, __u8 *, uint, __u16);
extern void iop_download_code(uint, __u8 *, uint, __u16);
extern __u8 *iop_compare_code(uint, __u8 *, uint, __u16);

extern void iop_register_interrupts(void);

#endif 
