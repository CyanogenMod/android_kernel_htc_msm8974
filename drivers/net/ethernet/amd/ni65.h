/* am7990 (lance) definitions
 *
 * This is an extension to the Linux operating system, and is covered by
 * same GNU General Public License that covers that work.
 *
 * Michael Hipp
 * email: mhipp@student.uni-tuebingen.de
 *
 * sources: (mail me or ask archie if you need them)
 *    crynwr-packet-driver
 */


#define CSR0_ERR	0x8000	
#define CSR0_BABL	0x4000	
#define CSR0_CERR	0x2000	
#define CSR0_MISS	0x1000	
#define CSR0_MERR	0x0800	
#define CSR0_RINT	0x0400	
#define CSR0_TINT       0x0200	
#define CSR0_IDON	0x0100	
#define CSR0_INTR	0x0080	
#define CSR0_INEA	0x0040	
#define CSR0_RXON	0x0020	
#define CSR0_TXON	0x0010	
#define CSR0_TDMD	0x0008	
#define CSR0_STOP	0x0004	
#define CSR0_STRT	0x0002	
#define CSR0_INIT	0x0001	

#define CSR0_CLRALL    0x7f00	

#define M_PROM		0x8000	
#define M_INTL		0x0040	
#define M_DRTY		0x0020	
#define M_COLL		0x0010	
#define M_DTCR		0x0008	
#define M_LOOP		0x0004	
#define M_DTX		0x0002	
#define M_DRX		0x0001	



#define RCV_OWN		0x80	
#define RCV_ERR		0x40	
#define RCV_FRAM	0x20	
#define RCV_OFLO	0x10	
#define RCV_CRC		0x08	
#define RCV_BUF_ERR	0x04	
#define RCV_START	0x02	
#define RCV_END		0x01	



#define XMIT_OWN	0x80	
#define XMIT_ERR	0x40	
#define XMIT_RETRY	0x10	
#define XMIT_1_RETRY	0x08	
#define XMIT_DEF	0x04	
#define XMIT_START	0x02	
#define XMIT_END	0x01	


#define XMIT_TDRMASK    0x03ff	
#define XMIT_RTRY 	0x0400	
#define XMIT_LCAR 	0x0800	
#define XMIT_LCOL 	0x1000	
#define XMIT_RESERV 	0x2000	
#define XMIT_UFLO 	0x4000	
#define XMIT_BUFF 	0x8000	

struct init_block {
	unsigned short mode;
	unsigned char eaddr[6];
	unsigned char filter[8];
	
	u32 rrp;		
	
	u32 trp;		
};

struct rmd {			
	union {
		volatile u32 buffer;
		struct {
			volatile unsigned char dummy[3];
			volatile unsigned char status;
		} s;
	} u;
	volatile short blen;
	volatile unsigned short mlen;
};

struct tmd {
	union {
		volatile u32 buffer;
		struct {
			volatile unsigned char dummy[3];
			volatile unsigned char status;
		} s;
	} u;
	volatile unsigned short blen;
	volatile unsigned short status2;
};
