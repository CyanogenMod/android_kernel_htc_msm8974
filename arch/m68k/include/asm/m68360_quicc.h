
#ifndef __M68360_QUICC_H
#define __M68360_QUICC_H

#include <asm/m68360_regs.h>
#include <asm/m68360_pram.h>



typedef struct quicc_bd {
    volatile unsigned short     status;
    volatile unsigned short     length;
    volatile unsigned char      *buf;     
} QUICC_BD;


#ifdef MOTOROLA_ORIGINAL
struct user_data {
    
    volatile unsigned char      udata_bd_ucode[0x400]; 
    volatile unsigned char      udata_bd[0x200];       
    volatile unsigned char      ucode_ext[0x100];      
    volatile unsigned char      RESERVED1[0x500];      
};
#else
struct user_data {
    
    volatile unsigned char      udata_bd_ucode[0x400]; 
    volatile unsigned char      udata_bd1[0x200];       
    volatile unsigned char      ucode_bd_scratch[0x100]; 
    volatile unsigned char      udata_bd2[0x100];       
    volatile unsigned char      RESERVED1[0x400];      
};
#endif


typedef struct quicc {
	union {
		struct quicc32_pram ch_pram_tbl[32];		
		struct user_data		u;
	}ch_or_u;	

    
	union {
		struct scc_pram {
			union {
				struct hdlc_pram        h;
				struct uart_pram        u;
				struct bisync_pram      b;
				struct transparent_pram t;
				unsigned char   RESERVED66[0x70];
			} pscc;               
			union {
				struct {
					unsigned char       RESERVED70[0x10];
					struct spi_pram     spi;
					unsigned char       RESERVED72[0x8];
					struct timer_pram   timer;
				} timer_spi;
				struct {
					struct idma_pram idma;
					unsigned char       RESERVED67[0x4];
					union {
						struct smc_uart_pram u;
						struct smc_trnsp_pram t;
					} psmc;
				} idma_smc;
			} pothers;
		} scc;
		struct ethernet_pram    enet_scc;
		struct global_multi_pram        m;
		unsigned char   pr[0x100];
	} pram[4];

    

    
    
    volatile unsigned long      sim_mcr;        
    volatile unsigned short     sim_simtr;      
    volatile unsigned char      RESERVED2[0x2]; 
    volatile unsigned char      sim_avr;        
    volatile unsigned char      sim_rsr;        
    volatile unsigned char      RESERVED3[0x2]; 
    volatile unsigned char      sim_clkocr;     
    volatile unsigned char      RESERVED62[0x3];        
    volatile unsigned short     sim_pllcr;      
    volatile unsigned char      RESERVED63[0x2];        
    volatile unsigned short     sim_cdvcr;      
    volatile unsigned short     sim_pepar;      
    volatile unsigned char      RESERVED64[0xa];        
    volatile unsigned char      sim_sypcr;      
    volatile unsigned char      sim_swiv;       
    volatile unsigned char      RESERVED6[0x2]; 
    volatile unsigned short     sim_picr;       
    volatile unsigned char      RESERVED7[0x2]; 
    volatile unsigned short     sim_pitr;       
    volatile unsigned char      RESERVED8[0x3]; 
    volatile unsigned char      sim_swsr;       
    volatile unsigned long      sim_bkar;       
    volatile unsigned long      sim_bkcr;       
    volatile unsigned char      RESERVED10[0x8];        
    
    volatile unsigned long      memc_gmr;       
    volatile unsigned short     memc_mstat;     
    volatile unsigned char      RESERVED11[0xa];        
    volatile unsigned long      memc_br0;       
    volatile unsigned long      memc_or0;       
    volatile unsigned char      RESERVED12[0x8];        
    volatile unsigned long      memc_br1;       
    volatile unsigned long      memc_or1;       
    volatile unsigned char      RESERVED13[0x8];        
    volatile unsigned long      memc_br2;       
    volatile unsigned long      memc_or2;       
    volatile unsigned char      RESERVED14[0x8];        
    volatile unsigned long      memc_br3;       
    volatile unsigned long      memc_or3;       
    volatile unsigned char      RESERVED15[0x8];        
    volatile unsigned long      memc_br4;       
    volatile unsigned long      memc_or4;       
    volatile unsigned char      RESERVED16[0x8];        
    volatile unsigned long      memc_br5;       
    volatile unsigned long      memc_or5;       
    volatile unsigned char      RESERVED17[0x8];        
    volatile unsigned long      memc_br6;       
    volatile unsigned long      memc_or6;       
    volatile unsigned char      RESERVED18[0x8];        
    volatile unsigned long      memc_br7;       
    volatile unsigned long      memc_or7;       
    volatile unsigned char      RESERVED9[0x28];        
    
    volatile unsigned short     test_tstmra;    
    volatile unsigned short     test_tstmrb;    
    volatile unsigned short     test_tstsc;     
    volatile unsigned short     test_tstrc;     
    volatile unsigned short     test_creg;      
    volatile unsigned short     test_dreg;      
    volatile unsigned char      RESERVED58[0x404];      
    
    volatile unsigned short     idma_iccr;      
    volatile unsigned char      RESERVED19[0x2];        
    volatile unsigned short     idma1_cmr;      
    volatile unsigned char      RESERVED68[0x2];        
    volatile unsigned long      idma1_sapr;     
    volatile unsigned long      idma1_dapr;     
    volatile unsigned long      idma1_bcr;      
    volatile unsigned char      idma1_fcr;      
    volatile unsigned char      RESERVED20;     
    volatile unsigned char      idma1_cmar;     
    volatile unsigned char      RESERVED21;     
    volatile unsigned char      idma1_csr;      
    volatile unsigned char      RESERVED22[0x3];        
    
    volatile unsigned char      sdma_sdsr;      
    volatile unsigned char      RESERVED23;     
    volatile unsigned short     sdma_sdcr;      
    volatile unsigned long      sdma_sdar;      
    
    volatile unsigned char      RESERVED69[0x2];        
    volatile unsigned short     idma2_cmr;      
    volatile unsigned long      idma2_sapr;     
    volatile unsigned long      idma2_dapr;     
    volatile unsigned long      idma2_bcr;      
    volatile unsigned char      idma2_fcr;      
    volatile unsigned char      RESERVED24;     
    volatile unsigned char      idma2_cmar;     
    volatile unsigned char      RESERVED25;     
    volatile unsigned char      idma2_csr;      
    volatile unsigned char      RESERVED26[0x7];        
    
    volatile unsigned long      intr_cicr;      
    volatile unsigned long      intr_cipr;      
    volatile unsigned long      intr_cimr;      
    volatile unsigned long      intr_cisr;      
    
    volatile unsigned short     pio_padir;      
    volatile unsigned short     pio_papar;      
    volatile unsigned short     pio_paodr;      
    volatile unsigned short     pio_padat;      
    volatile unsigned char      RESERVED28[0x8];        
    volatile unsigned short     pio_pcdir;      
    volatile unsigned short     pio_pcpar;      
    volatile unsigned short     pio_pcso;       
    volatile unsigned short     pio_pcdat;      
    volatile unsigned short     pio_pcint;      
    volatile unsigned char      RESERVED29[0x16];       
    
    volatile unsigned short     timer_tgcr;     
    volatile unsigned char      RESERVED30[0xe];        
    volatile unsigned short     timer_tmr1;     
    volatile unsigned short     timer_tmr2;     
    volatile unsigned short     timer_trr1;     
    volatile unsigned short     timer_trr2;     
    volatile unsigned short     timer_tcr1;     
    volatile unsigned short     timer_tcr2;     
    volatile unsigned short     timer_tcn1;     
    volatile unsigned short     timer_tcn2;     
    volatile unsigned short     timer_tmr3;     
    volatile unsigned short     timer_tmr4;     
    volatile unsigned short     timer_trr3;     
    volatile unsigned short     timer_trr4;     
    volatile unsigned short     timer_tcr3;     
    volatile unsigned short     timer_tcr4;     
    volatile unsigned short     timer_tcn3;     
    volatile unsigned short     timer_tcn4;     
    volatile unsigned short     timer_ter1;     
    volatile unsigned short     timer_ter2;     
    volatile unsigned short     timer_ter3;     
    volatile unsigned short     timer_ter4;     
    volatile unsigned char      RESERVED34[0x8];        
    
    volatile unsigned short     cp_cr;          
    volatile unsigned char      RESERVED35[0x2];        
    volatile unsigned short     cp_rccr;        
    volatile unsigned char      RESERVED37;     
    volatile unsigned char      cp_rmds;        
    volatile unsigned long      cp_rmdr;        
    volatile unsigned short     cp_rctr1;       
    volatile unsigned short     cp_rctr2;       
    volatile unsigned short     cp_rctr3;       
    volatile unsigned short     cp_rctr4;       
    volatile unsigned char      RESERVED59[0x2];        
    volatile unsigned short     cp_rter;        
    volatile unsigned char      RESERVED38[0x2];        
    volatile unsigned short     cp_rtmr;        
    volatile unsigned char      RESERVED39[0x14];       
    
    union {
        volatile unsigned long l;
        struct {
            volatile unsigned short BRGC_RESERV:14;
            volatile unsigned short rst:1;
            volatile unsigned short en:1;
            volatile unsigned short extc:2;
            volatile unsigned short atb:1;
            volatile unsigned short cd:12;
            volatile unsigned short div16:1;
        } b;
    } brgc[4];                                  
    
    struct scc_regs {
        union {
            struct {
                
                volatile unsigned short GSMR_RESERV2:1;
                volatile unsigned short edge:2;
                volatile unsigned short tci:1;
                volatile unsigned short tsnc:2;
                volatile unsigned short rinv:1;
                volatile unsigned short tinv:1;
                volatile unsigned short tpl:3;
                volatile unsigned short tpp:2;
                volatile unsigned short tend:1;
                volatile unsigned short tdcr:2;
                volatile unsigned short rdcr:2;
                volatile unsigned short renc:3;
                volatile unsigned short tenc:3;
                volatile unsigned short diag:2;
                volatile unsigned short enr:1;
                volatile unsigned short ent:1;
                volatile unsigned short mode:4;
                
                volatile unsigned short GSMR_RESERV1:14;
                volatile unsigned short pri:1;
                volatile unsigned short gde:1;
                volatile unsigned short tcrc:2;
                volatile unsigned short revd:1;
                volatile unsigned short trx:1;
                volatile unsigned short ttx:1;
                volatile unsigned short cdp:1;
                volatile unsigned short ctsp:1;
                volatile unsigned short cds:1;
                volatile unsigned short ctss:1;
                volatile unsigned short tfl:1;
                volatile unsigned short rfw:1;
                volatile unsigned short txsy:1;
                volatile unsigned short synl:2;
                volatile unsigned short rtsm:1;
                volatile unsigned short rsyn:1;
            } b;
            struct {
                volatile unsigned long low;
                volatile unsigned long high;
            } w;
        } scc_gsmr;                         
        volatile unsigned short scc_psmr;   
        volatile unsigned char  RESERVED42[0x2]; 
        volatile unsigned short scc_todr; 
        volatile unsigned short scc_dsr;        
        volatile unsigned short scc_scce;       
        volatile unsigned char  RESERVED43[0x2];
        volatile unsigned short scc_sccm;       
        volatile unsigned char  RESERVED44[0x1];
        volatile unsigned char  scc_sccs;       
        volatile unsigned char  RESERVED45[0x8]; 
    } scc_regs[4];
    
    struct smc_regs {
        volatile unsigned char  RESERVED46[0x2]; 
        volatile unsigned short smc_smcmr;       
        volatile unsigned char  RESERVED60[0x2]; 
        volatile unsigned char  smc_smce;        
        volatile unsigned char  RESERVED47[0x3]; 
        volatile unsigned char  smc_smcm;        
        volatile unsigned char  RESERVED48[0x5]; 
    } smc_regs[2];
    
    volatile unsigned short     spi_spmode;     
    volatile unsigned char      RESERVED51[0x4];        
    volatile unsigned char      spi_spie;       
    volatile unsigned char      RESERVED52[0x3];        
    volatile unsigned char      spi_spim;       
    volatile unsigned char      RESERVED53[0x2];        
    volatile unsigned char      spi_spcom;      
    volatile unsigned char      RESERVED54[0x4];        
    
    volatile unsigned short     pip_pipc;       
    volatile unsigned char      RESERVED65[0x2];        
    volatile unsigned short     pip_ptpr;       
    volatile unsigned long      pip_pbdir;      
    volatile unsigned long      pip_pbpar;      
    volatile unsigned long      pip_pbodr;      
    volatile unsigned long      pip_pbdat;      
    volatile unsigned char      RESERVED71[0x18];       
    
    volatile unsigned long      si_simode;      
    volatile unsigned char      si_sigmr;       
    volatile unsigned char      RESERVED55;     
    volatile unsigned char      si_sistr;       
    volatile unsigned char      si_sicmr;       
    volatile unsigned char      RESERVED56[0x4]; 
    volatile unsigned long      si_sicr;        
    volatile unsigned long      si_sirp;        
    volatile unsigned char      RESERVED57[0xc]; 
    volatile unsigned short     si_siram[0x80]; 
} QUICC;

#endif

