typedef struct filehdr {
        unsigned short  f_magic;        
        unsigned short  f_nscns;        
        long            f_timdat;       
        long            f_symptr;       
        long            f_nsyms;        
        unsigned short  f_opthdr;       
        unsigned short  f_flags;        
} FILHDR;
#define FILHSZ  sizeof(FILHDR)

#define OMAGIC		0407
#define MIPSEBMAGIC	0x160
#define MIPSELMAGIC	0x162

typedef struct scnhdr {
        char            s_name[8];      
        long            s_paddr;        
        long            s_vaddr;        
        long            s_size;         
        long            s_scnptr;       
        long            s_relptr;       
        long            s_lnnoptr;      
        unsigned short  s_nreloc;       
        unsigned short  s_nlnno;        
        long            s_flags;        
} SCNHDR;
#define SCNHSZ		sizeof(SCNHDR)
#define SCNROUND	((long)16)

typedef struct aouthdr {
        short   magic;          
        short   vstamp;         
        long    tsize;          
        long    dsize;          
        long    bsize;          
        long    entry;          
        long    text_start;     
        long    data_start;     
        long    bss_start;      
        long    gprmask;        
        long    cprmask[4];     
        long    gp_value;       
} AOUTHDR;
#define AOUTHSZ sizeof(AOUTHDR)

#define OMAGIC		0407
#define NMAGIC		0410
#define ZMAGIC		0413
#define SMAGIC		0411
#define LIBMAGIC        0443

#define N_TXTOFF(f, a) \
 ((a).magic == ZMAGIC || (a).magic == LIBMAGIC ? 0 : \
  ((a).vstamp < 23 ? \
   ((FILHSZ + AOUTHSZ + (f).f_nscns * SCNHSZ + 7) & 0xfffffff8) : \
   ((FILHSZ + AOUTHSZ + (f).f_nscns * SCNHSZ + SCNROUND-1) & ~(SCNROUND-1)) ) )
#define N_DATOFF(f, a) \
  N_TXTOFF(f, a) + (a).tsize;
