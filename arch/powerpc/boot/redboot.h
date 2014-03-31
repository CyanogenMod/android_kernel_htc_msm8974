#ifndef _PPC_REDBOOT_H
#define _PPC_REDBOOT_H

//   Copyright (c) 2002, 2003 Gary Thomas (<gary@mlbassoc.com>
//   Copyright (c) 1997 Dan Malek (dmalek@jlc.net)


typedef struct bd_info {
    unsigned int   bi_tag;        
    unsigned int   bi_size;       
    unsigned int   bi_revision;   
    unsigned int   bi_bdate;      
    unsigned int   bi_memstart;   
    unsigned int   bi_memsize;    
    unsigned int   bi_intfreq;    
    unsigned int   bi_busfreq;    
    unsigned int   bi_cpmfreq;    
    unsigned int   bi_brgfreq;    
    unsigned int   bi_vco;        
    unsigned int   bi_pci_freq;   
    unsigned int   bi_baudrate;   
    unsigned int   bi_immr;       
    unsigned char  bi_enetaddr[6];
    unsigned int   bi_flashbase;  
    unsigned int   bi_flashsize;  
    int            bi_flashwidth; 
    unsigned char *bi_cmdline;    
    unsigned char  bi_esa[3][6];  
    unsigned int   bi_ramdisk_begin, bi_ramdisk_end;
    struct {                      
        short x_res;              
        short y_res;              
        short bpp;                
        short mode;               
        unsigned long fb;         
    } bi_video;
    void         (*bi_cputc)(char);   
    char         (*bi_cgetc)(void);   
    int          (*bi_ctstc)(void);   
} bd_t;

#define BI_REV 0x0102    

#define bi_pci_busfreq bi_pci_freq
#define bi_immr_base   bi_immr
#endif
