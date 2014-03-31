#ifndef __DRIVERS_PARIDE_H__
#define __DRIVERS_PARIDE_H__

/* 
	paride.h	(c) 1997-8  Grant R. Guenther <grant@torque.net>
   		                    Under the terms of the GPL.

   This file defines the interface between the high-level parallel
   IDE device drivers (pd, pf, pcd, pt) and the adapter chips.

*/


#define PARIDE_H_VERSION 	"1.01"


#define	PI_PD	0	
#define PI_PCD	1	
#define PI_PF   2	
#define PI_PT	3	
#define PI_PG   4       


struct pi_adapter  {

	struct pi_protocol *proto;   
	int	port;		     
	int	mode;		     
	int     delay;		     
	int	devtype;	     
	char    *device;	     
	int     unit;		     
	int	saved_r0;	     
	int	saved_r2;	     
	int	reserved;	     
	unsigned long	private;     

	wait_queue_head_t parq;     
	void	*pardev;	     
	char	*parname;	     
	int	claimed;	     
	void (*claim_cont)(void);    
};

typedef struct pi_adapter PIA;


extern int pi_init(PIA *pi, 
	int autoprobe,		
	int port, 		
	int mode, 		
	int unit,		
	int protocol, 		
	int delay, 		
	char * scratch, 	
	int devtype,		
	int verbose,		
	char *device		
	);			

extern void pi_release(PIA *pi);


extern void pi_write_regr(PIA *pi, int cont, int regr, int val);

extern int pi_read_regr(PIA *pi, int cont, int regr);

extern void pi_write_block(PIA *pi, char * buf, int count);

extern void pi_read_block(PIA *pi, char * buf, int count);

extern void pi_connect(PIA *pi);

extern void pi_disconnect(PIA *pi);

extern void pi_do_claimed(PIA *pi, void (*cont)(void));
extern int pi_schedule_claimed(PIA *pi, void (*cont)(void));


#define delay_p			(pi->delay?udelay(pi->delay):(void)0)
#define out_p(offs,byte)	outb(byte,pi->port+offs); delay_p;
#define in_p(offs)		(delay_p,inb(pi->port+offs))

#define w0(byte)                {out_p(0,byte);}
#define r0()                    (in_p(0) & 0xff)
#define w1(byte)                {out_p(1,byte);}
#define r1()                    (in_p(1) & 0xff)
#define w2(byte)                {out_p(2,byte);}
#define r2()                    (in_p(2) & 0xff)
#define w3(byte)                {out_p(3,byte);}
#define w4(byte)                {out_p(4,byte);}
#define r4()                    (in_p(4) & 0xff)
#define w4w(data)     		{outw(data,pi->port+4); delay_p;}
#define w4l(data)     		{outl(data,pi->port+4); delay_p;}
#define r4w()         		(delay_p,inw(pi->port+4)&0xffff)
#define r4l()         		(delay_p,inl(pi->port+4)&0xffffffff)

static inline u16 pi_swab16( char *b, int k)

{ 	union { u16 u; char t[2]; } r;

	r.t[0]=b[2*k+1]; r.t[1]=b[2*k];
        return r.u;
}

static inline u32 pi_swab32( char *b, int k)

{ 	union { u32 u; char f[4]; } r;

	r.f[0]=b[4*k+1]; r.f[1]=b[4*k];
	r.f[2]=b[4*k+3]; r.f[3]=b[4*k+2];
        return r.u;
}

struct pi_protocol {

	char	name[8];	
	int	index;		

	int	max_mode;	
	int	epp_first;	
	
	int	default_delay;  
	int	max_units;	

	void (*write_regr)(PIA *,int,int,int);
	int  (*read_regr)(PIA *,int,int);
	void (*write_block)(PIA *,char *,int);
	void (*read_block)(PIA *,char *,int);

	void (*connect)(PIA *);
	void (*disconnect)(PIA *);
	
	int  (*test_port)(PIA *);
	int  (*probe_unit)(PIA *);
	int  (*test_proto)(PIA *,char *,int);
	void (*log_adapter)(PIA *,char *,int);
	
	int (*init_proto)(PIA *);
	void (*release_proto)(PIA *);
	struct module *owner;
};

typedef struct pi_protocol PIP;

extern int paride_register( PIP * );
extern void paride_unregister ( PIP * );

#endif 
