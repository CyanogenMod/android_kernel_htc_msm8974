/* IBM RS/6000 "XCOFF" file definitions for BFD.
   Copyright (C) 1990, 1991 Free Software Foundation, Inc.
   FIXME: Can someone provide a transliteration of this name into ASCII?
   Using the following chars caused a compiler warning on HIUX (so I replaced
   them with octal escapes), and isn't useful without an understanding of what
   character set it is.
   Written by Mimi Ph\373\364ng-Th\345o V\365 of IBM
   and John Gilmore of Cygnus Support.  */


struct external_filehdr {
	char f_magic[2];	
	char f_nscns[2];	
	char f_timdat[4];	
	char f_symptr[4];	
	char f_nsyms[4];	
	char f_opthdr[2];	
	char f_flags[2];	
};

        
#define U802WRMAGIC     0730    
#define U802ROMAGIC     0735    
#define U802TOCMAGIC    0737    

#define BADMAG(x)	\
	((x).f_magic != U802ROMAGIC && (x).f_magic != U802WRMAGIC && \
	 (x).f_magic != U802TOCMAGIC)

#define	FILHDR	struct external_filehdr
#define	FILHSZ	20




typedef struct
{
  unsigned char	magic[2];	
  unsigned char	vstamp[2];	
  unsigned char	tsize[4];	
  unsigned char	dsize[4];	
  unsigned char	bsize[4];	
  unsigned char	entry[4];	
  unsigned char	text_start[4];	
  unsigned char	data_start[4];	
  unsigned char	o_toc[4];	
  unsigned char	o_snentry[2];	
  unsigned char	o_sntext[2];	
  unsigned char	o_sndata[2];	
  unsigned char	o_sntoc[2];	
  unsigned char	o_snloader[2];	
  unsigned char	o_snbss[2];	
  unsigned char	o_algntext[2];	
  unsigned char	o_algndata[2];	
  unsigned char	o_modtype[2];	
  unsigned char o_cputype[2];	
  unsigned char	o_maxstack[4];	
  unsigned char o_maxdata[4];	
  unsigned char	o_resv2[12];	
}
AOUTHDR;

#define AOUTSZ 72
#define SMALL_AOUTSZ (28)
#define AOUTHDRSZ 72

#define	RS6K_AOUTHDR_OMAGIC	0x0107	
#define	RS6K_AOUTHDR_NMAGIC	0x0108	
#define	RS6K_AOUTHDR_ZMAGIC	0x010B	




struct external_scnhdr {
	char		s_name[8];	
	char		s_paddr[4];	
	char		s_vaddr[4];	
	char		s_size[4];	
	char		s_scnptr[4];	
	char		s_relptr[4];	
	char		s_lnnoptr[4];	
	char		s_nreloc[2];	
	char		s_nlnno[2];	
	char		s_flags[4];	
};

#define _TEXT	".text"
#define _DATA	".data"
#define _BSS	".bss"
#define _PAD	".pad"
#define _LOADER	".loader"

#define	SCNHDR	struct external_scnhdr
#define	SCNHSZ	40

#define STYP_LOADER 0x1000

#define STYP_DEBUG 0x2000

#define STYP_OVRFLO 0x8000


struct external_lineno {
	union {
		char l_symndx[4];	
		char l_paddr[4];	
	} l_addr;
	char l_lnno[2];	
};


#define	LINENO	struct external_lineno
#define	LINESZ	6



#define E_SYMNMLEN	8	
#define E_FILNMLEN	14	
#define E_DIMNUM	4	

struct external_syment
{
  union {
    char e_name[E_SYMNMLEN];
    struct {
      char e_zeroes[4];
      char e_offset[4];
    } e;
  } e;
  char e_value[4];
  char e_scnum[2];
  char e_type[2];
  char e_sclass[1];
  char e_numaux[1];
};



#define N_BTMASK	(017)
#define N_TMASK		(060)
#define N_BTSHFT	(4)
#define N_TSHIFT	(2)


union external_auxent {
	struct {
		char x_tagndx[4];	
		union {
			struct {
			    char  x_lnno[2]; 
			    char  x_size[2]; 
			} x_lnsz;
			char x_fsize[4];	
		} x_misc;
		union {
			struct {		
			    char x_lnnoptr[4];	
			    char x_endndx[4];	
			} x_fcn;
			struct {		
			    char x_dimen[E_DIMNUM][2];
			} x_ary;
		} x_fcnary;
		char x_tvndx[2];		
	} x_sym;

	union {
		char x_fname[E_FILNMLEN];
		struct {
			char x_zeroes[4];
			char x_offset[4];
		} x_n;
	} x_file;

	struct {
		char x_scnlen[4];			
		char x_nreloc[2];	
		char x_nlinno[2];	
	} x_scn;

        struct {
		char x_tvfill[4];	
		char x_tvlen[2];	
		char x_tvran[2][2];	
	} x_tv;		

	struct {
		unsigned char x_scnlen[4];
		unsigned char x_parmhash[4];
		unsigned char x_snhash[2];
		unsigned char x_smtyp[1];
		unsigned char x_smclas[1];
		unsigned char x_stab[4];
		unsigned char x_snstab[2];
	} x_csect;

};

#define	SYMENT	struct external_syment
#define	SYMESZ	18
#define	AUXENT	union external_auxent
#define	AUXESZ	18
#define DBXMASK 0x80		
#define SYMNAME_IN_DEBUG(symptr) ((symptr)->n_sclass & DBXMASK)





struct external_reloc {
  char r_vaddr[4];
  char r_symndx[4];
  char r_size[1];
  char r_type[1];
};


#define RELOC struct external_reloc
#define RELSZ 10

#define DEFAULT_DATA_SECTION_ALIGNMENT 4
#define DEFAULT_BSS_SECTION_ALIGNMENT 4
#define DEFAULT_TEXT_SECTION_ALIGNMENT 4
#define DEFAULT_SECTION_ALIGNMENT 4
