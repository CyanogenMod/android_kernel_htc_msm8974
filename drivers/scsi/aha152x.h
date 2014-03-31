#ifndef _AHA152X_H
#define _AHA152X_H


#define AHA152X_MAXQUEUE 7

#define AHA152X_REVID "Adaptec 152x SCSI driver; $Revision: 2.7 $"

#define SCSISEQ      (HOSTIOPORT0+0x00)    
#define SXFRCTL0     (HOSTIOPORT0+0x01)    
#define SXFRCTL1     (HOSTIOPORT0+0x02)    
#define SCSISIG      (HOSTIOPORT0+0x03)    
#define SCSIRATE     (HOSTIOPORT0+0x04)    
#define SELID        (HOSTIOPORT0+0x05)    
#define SCSIID       SELID                 
#define SCSIDAT      (HOSTIOPORT0+0x06)    
#define SCSIBUS      (HOSTIOPORT0+0x07)    
#define STCNT0       (HOSTIOPORT0+0x08)    
#define STCNT1       (HOSTIOPORT0+0x09)    
#define STCNT2       (HOSTIOPORT0+0x0a)    
#define SSTAT0       (HOSTIOPORT0+0x0b)    
#define SSTAT1       (HOSTIOPORT0+0x0c)    
#define SSTAT2       (HOSTIOPORT0+0x0d)    
#define SCSITEST     (HOSTIOPORT0+0x0e)    
#define SSTAT3       SCSITEST              
#define SSTAT4       (HOSTIOPORT0+0x0f)    
#define SIMODE0      (HOSTIOPORT1+0x10)    
#define SIMODE1      (HOSTIOPORT1+0x11)    
#define DMACNTRL0    (HOSTIOPORT1+0x12)    
#define DMACNTRL1    (HOSTIOPORT1+0x13)    
#define DMASTAT      (HOSTIOPORT1+0x14)    
#define FIFOSTAT     (HOSTIOPORT1+0x15)    
#define DATAPORT     (HOSTIOPORT1+0x16)    
#define BRSTCNTRL    (HOSTIOPORT1+0x18)    
#define PORTA        (HOSTIOPORT1+0x1a)    
#define PORTB        (HOSTIOPORT1+0x1b)    
#define REV          (HOSTIOPORT1+0x1c)    
#define STACK        (HOSTIOPORT1+0x1d)    
#define TEST         (HOSTIOPORT1+0x1e)    

#define IO_RANGE        0x20

#define O_PORTA         0x1a               
#define O_PORTB         0x1b               
#define O_DMACNTRL1     0x13               
#define O_STACK         0x1d               

#define O_TC_PORTA      0x0a               
#define O_TC_PORTB      0x0b               
#define O_TC_DMACNTRL1  0x03               
#define O_TC_STACK      0x0d               


#define TEMODEO      0x80
#define ENSELO       0x40
#define ENSELI       0x20
#define ENRESELI     0x10
#define ENAUTOATNO   0x08
#define ENAUTOATNI   0x04
#define ENAUTOATNP   0x02
#define SCSIRSTO     0x01

#define SCSIEN       0x80
#define DMAEN        0x40
#define CH1          0x20
#define CLRSTCNT     0x10
#define SPIOEN       0x08
#define CLRCH1       0x02

#define BITBUCKET    0x80
#define SWRAPEN      0x40
#define ENSPCHK      0x20
#define STIMESEL     0x18    
#define STIMESEL_    3
#define ENSTIMER     0x04
#define BYTEALIGN    0x02

#define SIG_CDI          0x80
#define SIG_IOI          0x40
#define SIG_MSGI         0x20
#define SIG_ATNI         0x10
#define SIG_SELI         0x08
#define SIG_BSYI         0x04
#define SIG_REQI         0x02
#define SIG_ACKI         0x01

#define P_MASK       (SIG_MSGI|SIG_CDI|SIG_IOI)
#define P_DATAO      (0)
#define P_DATAI      (SIG_IOI)
#define P_CMD        (SIG_CDI)
#define P_STATUS     (SIG_CDI|SIG_IOI)
#define P_MSGO       (SIG_MSGI|SIG_CDI)
#define P_MSGI       (SIG_MSGI|SIG_CDI|SIG_IOI)

#define SIG_CDO          0x80
#define SIG_IOO          0x40
#define SIG_MSGO         0x20
#define SIG_ATNO         0x10
#define SIG_SELO         0x08
#define SIG_BSYO         0x04
#define SIG_REQO         0x02
#define SIG_ACKO         0x01

#define SXFR         0x70    
#define SXFR_        4
#define SOFS         0x0f    

#define OID          0x70
#define OID_         4
#define TID          0x07

#define GETSTCNT() ( (GETPORT(STCNT2)<<16) \
                   + (GETPORT(STCNT1)<< 8) \
                   + GETPORT(STCNT0) )

#define SETSTCNT(X) { SETPORT(STCNT2, ((X) & 0xFF0000) >> 16); \
                      SETPORT(STCNT1, ((X) & 0x00FF00) >>  8); \
                      SETPORT(STCNT0, ((X) & 0x0000FF) ); }

#define TARGET       0x80
#define SELDO        0x40
#define SELDI        0x20
#define SELINGO      0x10
#define SWRAP        0x08
#define SDONE        0x04
#define SPIORDY      0x02
#define DMADONE      0x01

#define SETSDONE     0x80
#define CLRSELDO     0x40
#define CLRSELDI     0x20
#define CLRSELINGO   0x10
#define CLRSWRAP     0x08
#define CLRSDONE     0x04
#define CLRSPIORDY   0x02
#define CLRDMADONE   0x01

#define SELTO        0x80
#define ATNTARG      0x40
#define SCSIRSTI     0x20
#define PHASEMIS     0x10
#define BUSFREE      0x08
#define SCSIPERR     0x04
#define PHASECHG     0x02
#define REQINIT      0x01

#define CLRSELTIMO   0x80
#define CLRATNO      0x40
#define CLRSCSIRSTI  0x20
#define CLRBUSFREE   0x08
#define CLRSCSIPERR  0x04
#define CLRPHASECHG  0x02
#define CLRREQINIT   0x01

#define SOFFSET      0x20
#define SEMPTY       0x10
#define SFULL        0x08
#define SFCNT        0x07    

#define SCSICNT      0xf0    
#define SCSICNT_     4
#define OFFCNT       0x0f    

#define SCTESTU      0x08
#define SCTESTD      0x04
#define STCTEST      0x01

#define SYNCERR      0x04
#define FWERR        0x02
#define FRERR        0x01

#define CLRSYNCERR   0x04
#define CLRFWERR     0x02
#define CLRFRERR     0x01

#define ENSELDO      0x40
#define ENSELDI      0x20
#define ENSELINGO    0x10
#define ENSWRAP      0x08
#define ENSDONE      0x04
#define ENSPIORDY    0x02
#define ENDMADONE    0x01

#define ENSELTIMO    0x80
#define ENATNTARG    0x40
#define ENSCSIRST    0x20
#define ENPHASEMIS   0x10
#define ENBUSFREE    0x08
#define ENSCSIPERR   0x04
#define ENPHASECHG   0x02
#define ENREQINIT    0x01

#define ENDMA        0x80
#define _8BIT        0x40
#define DMA          0x20
#define WRITE_READ   0x08
#define INTEN        0x04
#define RSTFIFO      0x02
#define SWINT        0x01

#define PWRDWN       0x80
#define STK          0x07    

#define ATDONE       0x80
#define WORDRDY      0x40
#define INTSTAT      0x20
#define DFIFOFULL    0x10
#define DFIFOEMP     0x08

#define BON          0xf0
#define BOFF         0x0f

#define BOFFTMR      0x40
#define BONTMR       0x20
#define STCNTH       0x10
#define STCNTM       0x08
#define STCNTL       0x04
#define SCSIBLK      0x02
#define DMABLK       0x01

typedef union {
  struct {
    unsigned reserved:2;    
    unsigned tardisc:1;     
    unsigned syncneg:1;     
    unsigned msgclasses:2;  
    unsigned boot:1;        
    unsigned dma:1;         
    unsigned id:3;          
    unsigned irq:2;         
    unsigned dmachan:2;     
    unsigned parity:1;      
  } fields;
  unsigned short port;
} aha152x_config ;

#define cf_parity     fields.parity
#define cf_dmachan    fields.dmachan
#define cf_irq        fields.irq
#define cf_id         fields.id
#define cf_dma        fields.dma
#define cf_boot       fields.boot
#define cf_msgclasses fields.msgclasses
#define cf_syncneg    fields.syncneg
#define cf_tardisc    fields.tardisc
#define cf_port       port


#define SETPORT(PORT, VAL)	outb( (VAL), (PORT) )
#define GETPORT(PORT)		inb( PORT )
#define SETBITS(PORT, BITS)	outb( (inb(PORT) | (BITS)), (PORT) )
#define CLRBITS(PORT, BITS)	outb( (inb(PORT) & ~(BITS)), (PORT) )
#define TESTHI(PORT, BITS)	((inb(PORT) & (BITS)) == (BITS))
#define TESTLO(PORT, BITS)	((inb(PORT) & (BITS)) == 0)

#define SETRATE(RATE)		SETPORT(SCSIRATE,(RATE) & 0x7f)

#if defined(AHA152X_DEBUG)
enum {
  debug_procinfo  = 0x0001,
  debug_queue     = 0x0002,
  debug_locking   = 0x0004,
  debug_intr      = 0x0008,
  debug_selection = 0x0010,
  debug_msgo      = 0x0020,
  debug_msgi      = 0x0040,
  debug_status    = 0x0080,
  debug_cmd       = 0x0100,
  debug_datai     = 0x0200,
  debug_datao     = 0x0400,
  debug_eh	  = 0x0800,
  debug_done      = 0x1000,
  debug_phases    = 0x2000,
};
#endif

struct aha152x_setup {
	int io_port;
	int irq;
	int scsiid;
	int reconnect;
	int parity;
	int synchronous;
	int delay;
	int ext_trans;
	int tc1550;
#if defined(AHA152X_DEBUG)
	int debug;
#endif
	char *conf;
};

struct Scsi_Host *aha152x_probe_one(struct aha152x_setup *);
void aha152x_release(struct Scsi_Host *);
int aha152x_host_reset_host(struct Scsi_Host *);

#endif 
