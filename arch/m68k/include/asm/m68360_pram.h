
#ifndef __PRAM_H
#define __PRAM_H

#define VALID_SLOT	0x8000
#define WRAP_SLOT	0x4000

struct global_multi_pram {
    unsigned long   mcbase;		
    unsigned short  qmcstate;		
    unsigned short  mrblr;		
    unsigned short  tx_s_ptr;		
    unsigned short  rxptr;		
    unsigned short  grfthr;		
    unsigned short  grfcnt;		
    unsigned long   intbase;		
    unsigned long   iintptr;		
    unsigned short  rx_s_ptr;		

    unsigned short  txptr;		
    unsigned long   c_mask32;		
    unsigned short  tsatrx[32];		
    unsigned short  tsattx[32];		
    unsigned short  c_mask16;		
};

struct quicc32_pram {

    unsigned short  tbase;		
    unsigned short  chamr;		
    unsigned long   tstate;		
    unsigned long   txintr;		
    unsigned short  tbptr;		
    unsigned short  txcntr;		
    unsigned long   tupack;		
    unsigned long   zistate;		
    unsigned long   tcrc;		
    unsigned short  intmask;		
    unsigned short  bdflags;		
    unsigned short  rbase;		
    unsigned short  mflr;		
    unsigned long   rstate;		
    unsigned long   rxintr;		
    unsigned short  rbptr;		
    unsigned short  rxbyc;		
    unsigned long   rpack;		
    unsigned long   zdstate;		
    unsigned long   rcrc;		
    unsigned short  maxc;		
    unsigned short  tmp_mb;		
};



struct hdlc_pram {
    unsigned short  rbase;          
    unsigned short  tbase;          
    unsigned char   rfcr;           
    unsigned char   tfcr;           
    unsigned short  mrblr;          
    unsigned long   rstate;         
    unsigned long   rptr;           
    unsigned short  rbptr;          
    unsigned short  rcount;         
    unsigned long   rtemp;          
    unsigned long   tstate;         
    unsigned long   tptr;           
    unsigned short  tbptr;          
    unsigned short  tcount;         
    unsigned long   ttemp;          
    unsigned long   rcrc;           
    unsigned long   tcrc;           
   
    unsigned char   RESERVED1[4];   
    unsigned long   c_mask;         
    unsigned long   c_pres;         
    unsigned short  disfc;          
    unsigned short  crcec;          
    unsigned short  abtsc;          
    unsigned short  nmarc;          
    unsigned short  retrc;          
    unsigned short  mflr;           
    unsigned short  max_cnt;        
    unsigned short  rfthr;          
    unsigned short  rfcnt;          
    unsigned short  hmask;          
    unsigned short  haddr1;         
    unsigned short  haddr2;         
    unsigned short  haddr3;         
    unsigned short  haddr4;         
    unsigned short  tmp;            
    unsigned short  tmp_mb;         
};




#define CC_INVALID  0x8000          
#define CC_REJ      0x4000          
#define CC_CHAR     0x00ff          

struct uart_pram {
    unsigned short  rbase;          
    unsigned short  tbase;          
    unsigned char   rfcr;           
    unsigned char   tfcr;           
    unsigned short  mrblr;          
    unsigned long   rstate;         
    unsigned long   rptr;           
    unsigned short  rbptr;          
    unsigned short  rcount;         
    unsigned long   rx_temp;        
    unsigned long   tstate;         
    unsigned long   tptr;           
    unsigned short  tbptr;          
    unsigned short  tcount;         
    unsigned long   ttemp;          
    unsigned long   rcrc;           
    unsigned long   tcrc;           
   
    unsigned char   RESERVED1[8];   
    unsigned short  max_idl;        
    unsigned short  idlc;           
    unsigned short  brkcr;          
                   
    unsigned short  parec;          
    unsigned short  frmer;          
    unsigned short  nosec;          
    unsigned short  brkec;          
    unsigned short  brkln;          
                   
    unsigned short  uaddr1;         
    unsigned short  uaddr2;         
    unsigned short  rtemp;          
    unsigned short  toseq;          
    unsigned short  cc[8];          
    unsigned short  rccm;           
    unsigned short  rccr;           
    unsigned short  rlbc;           
};




struct bisync_pram {
    unsigned short  rbase;          
    unsigned short  tbase;          
    unsigned char   rfcr;           
    unsigned char   tfcr;           
    unsigned short  mrblr;          
    unsigned long   rstate;         
    unsigned long   rptr;           
    unsigned short  rbptr;          
    unsigned short  rcount;         
    unsigned long   rtemp;          
    unsigned long   tstate;         
    unsigned long   tptr;           
    unsigned short  tbptr;          
    unsigned short  tcount;         
    unsigned long   ttemp;          
    unsigned long   rcrc;           
    unsigned long   tcrc;           
   
    unsigned char   RESERVED1[4];   
    unsigned long   crcc;           
    unsigned short  prcrc;          
    unsigned short  ptcrc;          
    unsigned short  parec;          
    unsigned short  bsync;          
    unsigned short  bdle;           
    unsigned short  cc[8];          
    unsigned short  rccm;           
};

struct iom2_pram {
    unsigned short  ci_data;        
    unsigned short  monitor_data;   
    unsigned short  tstate;         
    unsigned short  rstate;         
};


#define SPI_R       0x8000          

struct spi_pram {
    unsigned short  rbase;          
    unsigned short  tbase;          
    unsigned char   rfcr;           
    unsigned char   tfcr;           
    unsigned short  mrblr;          
    unsigned long   rstate;         
    unsigned long   rptr;           
    unsigned short  rbptr;          
    unsigned short  rcount;         
    unsigned long   rtemp;          
    unsigned long   tstate;         
    unsigned long   tptr;           
    unsigned short  tbptr;          
    unsigned short  tcount;         
    unsigned long   ttemp;          
};

struct smc_uart_pram {
    unsigned short  rbase;          
    unsigned short  tbase;          
    unsigned char   rfcr;           
    unsigned char   tfcr;           
    unsigned short  mrblr;          
    unsigned long   rstate;         
    unsigned long   rptr;           
    unsigned short  rbptr;          
    unsigned short  rcount;         
    unsigned long   rtemp;          
    unsigned long   tstate;         
    unsigned long   tptr;           
    unsigned short  tbptr;          
    unsigned short  tcount;         
    unsigned long   ttemp;          
    unsigned short  max_idl;        
    unsigned short  idlc;           
    unsigned short  brkln;          
    unsigned short  brkec;          
    unsigned short  brkcr;          
    unsigned short  r_mask;         
};

struct smc_trnsp_pram {
    unsigned short  rbase;          
    unsigned short  tbase;          
    unsigned char   rfcr;           
    unsigned char   tfcr;           
    unsigned short  mrblr;          
    unsigned long   rstate;         
    unsigned long   rptr;           
    unsigned short  rbptr;          
    unsigned short  rcount;         
    unsigned long   rtemp;          
    unsigned long   tstate;         
    unsigned long   tptr;           
    unsigned short  tbptr;          
    unsigned short  tcount;         
    unsigned long   ttemp;          
    unsigned short  reserved[5];    
};

struct idma_pram {
    unsigned short  ibase;          
    unsigned short  ibptr;          
    unsigned long   istate;         
    unsigned long   itemp;          
};

struct ethernet_pram {
    unsigned short  rbase;          
    unsigned short  tbase;          
    unsigned char   rfcr;           
    unsigned char   tfcr;           
    unsigned short  mrblr;          
    unsigned long   rstate;         
    unsigned long   rptr;           
    unsigned short  rbptr;          
    unsigned short  rcount;         
    unsigned long   rtemp;          
    unsigned long   tstate;         
    unsigned long   tptr;           
    unsigned short  tbptr;          
    unsigned short  tcount;         
    unsigned long   ttemp;          
    unsigned long   rcrc;           
    unsigned long   tcrc;           
   
    unsigned long   c_pres;         
    unsigned long   c_mask;         
    unsigned long   crcec;          
    unsigned long   alec;           
    unsigned long   disfc;          
    unsigned short  pads;           
    unsigned short  ret_lim;        
    unsigned short  ret_cnt;        
    unsigned short  mflr;           
    unsigned short  minflr;         
    unsigned short  maxd1;          
    unsigned short  maxd2;          
    unsigned short  maxd;           
    unsigned short  dma_cnt;        
    unsigned short  max_b;          
    unsigned short  gaddr1;         
    unsigned short  gaddr2;         
    unsigned short  gaddr3;         
    unsigned short  gaddr4;         
    unsigned long   tbuf0_data0;    
    unsigned long   tbuf0_data1;    
    unsigned long   tbuf0_rba0;
    unsigned long   tbuf0_crc;
    unsigned short  tbuf0_bcnt;
    union {
        unsigned char b[6];
        struct {
            unsigned short high;
            unsigned short middl;
            unsigned short low;
        } w;
    } paddr;
    unsigned short  p_per;          
    unsigned short  rfbd_ptr;       
    unsigned short  tfbd_ptr;       
    unsigned short  tlbd_ptr;       
    unsigned long   tbuf1_data0;    
    unsigned long   tbuf1_data1;    
    unsigned long   tbuf1_rba0;
    unsigned long   tbuf1_crc;
    unsigned short  tbuf1_bcnt;
    unsigned short  tx_len;         
    unsigned short  iaddr1;         
    unsigned short  iaddr2;         
    unsigned short  iaddr3;         
    unsigned short  iaddr4;         
    unsigned short  boff_cnt;       
    unsigned short  taddr_h;        
    unsigned short  taddr_m;        
    unsigned short  taddr_l;        
};

struct transparent_pram {
    unsigned short  rbase;          
    unsigned short  tbase;          
    unsigned char   rfcr;           
    unsigned char   tfcr;           
    unsigned short  mrblr;          
    unsigned long   rstate;         
    unsigned long   rptr;           
    unsigned short  rbptr;          
    unsigned short  rcount;         
    unsigned long   rtemp;          
    unsigned long   tstate;         
    unsigned long   tptr;           
    unsigned short  tbptr;          
    unsigned short  tcount;         
    unsigned long   ttemp;          
    unsigned long   rcrc;           
    unsigned long   tcrc;           
   
    unsigned long   crc_p;          
    unsigned long   crc_c;          
};

struct timer_pram {
    unsigned short  tm_base;        
    unsigned short  tm_ptr;         
    unsigned short  r_tmr;          
    unsigned short  r_tmv;          
    unsigned long   tm_cmd;         
    unsigned long   tm_cnt;         
};

#endif
