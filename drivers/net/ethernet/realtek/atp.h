
#include <linux/if_ether.h>
#include <linux/types.h>

struct rx_header {
    ushort pad;			
    ushort rx_count;
    ushort rx_status;		
    ushort cur_addr;		
};

#define PAR_DATA	0
#define PAR_STATUS	1
#define PAR_CONTROL 2

enum chip_type { RTL8002, RTL8012 };

#define Ctrl_LNibRead	0x08	
#define Ctrl_HNibRead	0
#define Ctrl_LNibWrite	0x08	
#define Ctrl_HNibWrite	0
#define Ctrl_SelData	0x04	
#define Ctrl_IRQEN	0x10	

#define EOW	0xE0
#define EOC	0xE0
#define WrAddr	0x40	
#define RdAddr	0xC0
#define HNib	0x10

enum page0_regs
{
    
    PAR0 = 0, PAR1 = 1, PAR2 = 2, PAR3 = 3, PAR4 = 4, PAR5 = 5,
    TxCNT0 = 6, TxCNT1 = 7,		
    TxSTAT = 8, RxSTAT = 9,		
    ISR = 10, IMR = 11,			
    CMR1 = 12,				
    CMR2 = 13,				
    MODSEL = 14,			
    MAR = 14,				
    CMR2_h = 0x1d, };

enum eepage_regs
{ PROM_CMD = 6, PROM_DATA = 7 };	


#define ISR_TxOK	0x01
#define ISR_RxOK	0x04
#define ISR_TxErr	0x02
#define ISRh_RxErr	0x11	

#define CMR1h_MUX	0x08	
#define CMR1h_RESET	0x04	
#define CMR1h_RxENABLE	0x02	
#define CMR1h_TxENABLE	0x01	
#define CMR1h_TxRxOFF	0x00
#define CMR1_ReXmit	0x08	
#define CMR1_Xmit	0x04	
#define	CMR1_IRQ	0x02	
#define	CMR1_BufEnb	0x01	
#define	CMR1_NextPkt	0x01	

#define CMR2_NULL	8
#define CMR2_IRQOUT	9
#define CMR2_RAMTEST	10
#define CMR2_EEPROM	12	

#define CMR2h_OFF	0	
#define CMR2h_Physical	1	
#define CMR2h_Normal	2	
#define CMR2h_PROMISC	3	

static inline unsigned char inbyte(unsigned short port)
{
    unsigned char _v;
    __asm__ __volatile__ ("inb %w1,%b0" :"=a" (_v):"d" (port));
    return _v;
}

static inline unsigned char read_nibble(short port, unsigned char offset)
{
    unsigned char retval;
    outb(EOC+offset, port + PAR_DATA);
    outb(RdAddr+offset, port + PAR_DATA);
    inbyte(port + PAR_STATUS);		
    retval = inbyte(port + PAR_STATUS);
    outb(EOC+offset, port + PAR_DATA);

    return retval;
}

static inline unsigned char read_byte_mode0(short ioaddr)
{
    unsigned char low_nib;

    outb(Ctrl_LNibRead, ioaddr + PAR_CONTROL);
    inbyte(ioaddr + PAR_STATUS);
    low_nib = (inbyte(ioaddr + PAR_STATUS) >> 3) & 0x0f;
    outb(Ctrl_HNibRead, ioaddr + PAR_CONTROL);
    inbyte(ioaddr + PAR_STATUS);	
    inbyte(ioaddr + PAR_STATUS);	
    return low_nib | ((inbyte(ioaddr + PAR_STATUS) << 1) & 0xf0);
}

static inline unsigned char read_byte_mode2(short ioaddr)
{
    unsigned char low_nib;

    outb(Ctrl_LNibRead, ioaddr + PAR_CONTROL);
    inbyte(ioaddr + PAR_STATUS);
    low_nib = (inbyte(ioaddr + PAR_STATUS) >> 3) & 0x0f;
    outb(Ctrl_HNibRead, ioaddr + PAR_CONTROL);
    inbyte(ioaddr + PAR_STATUS);	
    return low_nib | ((inbyte(ioaddr + PAR_STATUS) << 1) & 0xf0);
}

static inline unsigned char read_byte_mode4(short ioaddr)
{
    unsigned char low_nib;

    outb(RdAddr | MAR, ioaddr + PAR_DATA);
    low_nib = (inbyte(ioaddr + PAR_STATUS) >> 3) & 0x0f;
    outb(RdAddr | HNib | MAR, ioaddr + PAR_DATA);
    return low_nib | ((inbyte(ioaddr + PAR_STATUS) << 1) & 0xf0);
}

static inline unsigned char read_byte_mode6(short ioaddr)
{
    unsigned char low_nib;

    outb(RdAddr | MAR, ioaddr + PAR_DATA);
    inbyte(ioaddr + PAR_STATUS);
    low_nib = (inbyte(ioaddr + PAR_STATUS) >> 3) & 0x0f;
    outb(RdAddr | HNib | MAR, ioaddr + PAR_DATA);
    inbyte(ioaddr + PAR_STATUS);
    return low_nib | ((inbyte(ioaddr + PAR_STATUS) << 1) & 0xf0);
}

static inline void
write_reg(short port, unsigned char reg, unsigned char value)
{
    unsigned char outval;
    outb(EOC | reg, port + PAR_DATA);
    outval = WrAddr | reg;
    outb(outval, port + PAR_DATA);
    outb(outval, port + PAR_DATA);	

    outval &= 0xf0;
    outval |= value;
    outb(outval, port + PAR_DATA);
    outval &= 0x1f;
    outb(outval, port + PAR_DATA);
    outb(outval, port + PAR_DATA);

    outb(EOC | outval, port + PAR_DATA);
}

static inline void
write_reg_high(short port, unsigned char reg, unsigned char value)
{
    unsigned char outval = EOC | HNib | reg;

    outb(outval, port + PAR_DATA);
    outval &= WrAddr | HNib | 0x0f;
    outb(outval, port + PAR_DATA);
    outb(outval, port + PAR_DATA);	

    outval = WrAddr | HNib | value;
    outb(outval, port + PAR_DATA);
    outval &= HNib | 0x0f;		
    outb(outval, port + PAR_DATA);
    outb(outval, port + PAR_DATA);

    outb(EOC | HNib | outval, port + PAR_DATA);
}

/* Write a byte out using nibble mode.  The low nibble is written first. */
static inline void
write_reg_byte(short port, unsigned char reg, unsigned char value)
{
    unsigned char outval;
    outb(EOC | reg, port + PAR_DATA); 	
    outval = WrAddr | reg;
    outb(outval, port + PAR_DATA);
    outb(outval, port + PAR_DATA);	

    outb((outval & 0xf0) | (value & 0x0f), port + PAR_DATA);
    outb(value & 0x0f, port + PAR_DATA);
    value >>= 4;
    outb(value, port + PAR_DATA);
    outb(0x10 | value, port + PAR_DATA);
    outb(0x10 | value, port + PAR_DATA);

    outb(EOC  | value, port + PAR_DATA); 	
}

static inline void write_byte_mode0(short ioaddr, unsigned char value)
{
    outb(value & 0x0f, ioaddr + PAR_DATA);
    outb((value>>4) | 0x10, ioaddr + PAR_DATA);
}

static inline void write_byte_mode1(short ioaddr, unsigned char value)
{
    outb(value & 0x0f, ioaddr + PAR_DATA);
    outb(Ctrl_IRQEN | Ctrl_LNibWrite, ioaddr + PAR_CONTROL);
    outb((value>>4) | 0x10, ioaddr + PAR_DATA);
    outb(Ctrl_IRQEN | Ctrl_HNibWrite, ioaddr + PAR_CONTROL);
}

static inline void write_word_mode0(short ioaddr, unsigned short value)
{
    outb(value & 0x0f, ioaddr + PAR_DATA);
    value >>= 4;
    outb((value & 0x0f) | 0x10, ioaddr + PAR_DATA);
    value >>= 4;
    outb(value & 0x0f, ioaddr + PAR_DATA);
    value >>= 4;
    outb((value & 0x0f) | 0x10, ioaddr + PAR_DATA);
}

#define EE_SHIFT_CLK	0x04	
#define EE_CS		0x02	
#define EE_CLK_HIGH	0x12
#define EE_CLK_LOW	0x16
#define EE_DATA_WRITE	0x01	
#define EE_DATA_READ	0x08	

#define eeprom_delay(ticks) \
do { int _i = 40; while (--_i > 0) { __SLOW_DOWN_IO; }} while (0)

#define EE_WRITE_CMD(offset)	(((5 << 6) + (offset)) << 17)
#define EE_READ(offset) 	(((6 << 6) + (offset)) << 17)
#define EE_ERASE(offset)	(((7 << 6) + (offset)) << 17)
#define EE_CMD_SIZE	27	
