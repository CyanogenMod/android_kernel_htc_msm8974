/*
 * Device driver for the SYMBIOS/LSILOGIC 53C8XX and 53C1010 family 
 * of PCI-SCSI IO processors.
 *
 * Copyright (C) 1999-2001  Gerard Roudier <groudier@free.fr>
 *
 * This driver is derived from the Linux sym53c8xx driver.
 * Copyright (C) 1998-2000  Gerard Roudier
 *
 * The sym53c8xx driver is derived from the ncr53c8xx driver that had been 
 * a port of the FreeBSD ncr driver to Linux-1.2.13.
 *
 * The original ncr driver has been written for 386bsd and FreeBSD by
 *         Wolfgang Stanglmeier        <wolf@cologne.de>
 *         Stefan Esser                <se@mi.Uni-Koeln.de>
 * Copyright (C) 1994  Wolfgang Stanglmeier
 *
 * Other major contributions:
 *
 * NVRAM detection and reading.
 * Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
 *
 *-----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef SYM_DEFS_H
#define SYM_DEFS_H

#define SYM_VERSION "2.2.3"
#define SYM_DRIVER_NAME	"sym-" SYM_VERSION

struct sym_chip {
	u_short	device_id;
	u_short	revision_id;
	char	*name;
	u_char	burst_max;	
	u_char	offset_max;
	u_char	nr_divisor;
	u_char	lp_probe_bit;
	u_int	features;
#define FE_LED0		(1<<0)
#define FE_WIDE		(1<<1)    
#define FE_ULTRA	(1<<2)	  
#define FE_ULTRA2	(1<<3)	  
#define FE_DBLR		(1<<4)	  
#define FE_QUAD		(1<<5)	  
#define FE_ERL		(1<<6)    
#define FE_CLSE		(1<<7)    
#define FE_WRIE		(1<<8)    
#define FE_ERMP		(1<<9)    
#define FE_BOF		(1<<10)   
#define FE_DFS		(1<<11)   
#define FE_PFEN		(1<<12)   
#define FE_LDSTR	(1<<13)   
#define FE_RAM		(1<<14)   
#define FE_VARCLK	(1<<15)   
#define FE_RAM8K	(1<<16)   
#define FE_64BIT	(1<<17)   
#define FE_IO256	(1<<18)   
#define FE_NOPM		(1<<19)   
#define FE_LEDC		(1<<20)   
#define FE_ULTRA3	(1<<21)	  
#define FE_66MHZ	(1<<22)	  
#define FE_CRC		(1<<23)	  
#define FE_DIFF		(1<<24)	  
#define FE_DFBC		(1<<25)	  
#define FE_LCKFRQ	(1<<26)	  
#define FE_C10		(1<<27)	  
#define FE_U3EN		(1<<28)	  
#define FE_DAC		(1<<29)	  
#define FE_ISTAT1 	(1<<30)   

#define FE_CACHE_SET	(FE_ERL|FE_CLSE|FE_WRIE|FE_ERMP)
#define FE_CACHE0_SET	(FE_CACHE_SET & ~FE_ERL)
};

struct sym_reg {
  u8	nc_scntl0;	

  u8	nc_scntl1;	
        #define   ISCON   0x10  
        #define   CRST    0x08  
        #define   IARB    0x02  

  u8	nc_scntl2;	
	#define   SDU     0x80  
	#define   CHM     0x40  
	#define   WSS     0x08  
	#define   WSR     0x01  

  u8	nc_scntl3;	
	#define   EWS     0x08  
	#define   ULTRA   0x80  
				

  u8	nc_scid;	
	#define   RRE     0x40  
	#define   SRE     0x20  

  u8	nc_sxfer;	
				

  u8	nc_sdid;	

  u8	nc_gpreg;	

  u8	nc_sfbr;	

  u8	nc_socl;
	#define   CREQ	  0x80	
	#define   CACK	  0x40	
	#define   CBSY	  0x20	
	#define   CSEL	  0x10	
	#define   CATN	  0x08	
	#define   CMSG	  0x04	
	#define   CC_D	  0x02	
	#define   CI_O	  0x01	

  u8	nc_ssid;

  u8	nc_sbcl;

  u8	nc_dstat;
        #define   DFE     0x80  
        #define   MDPE    0x40  
        #define   BF      0x20  
        #define   ABRT    0x10  
        #define   SSI     0x08  
        #define   SIR     0x04  
        #define   IID     0x01  

  u8	nc_sstat0;
        #define   ILF     0x80  
        #define   ORF     0x40  
        #define   OLF     0x20  
        #define   AIP     0x10  
        #define   LOA     0x08  
        #define   WOA     0x04  
        #define   IRST    0x02  
        #define   SDP     0x01  

  u8	nc_sstat1;
	#define   FF3210  0xf0	

  u8	nc_sstat2;
        #define   ILF1    0x80  
        #define   ORF1    0x40  
        #define   OLF1    0x20  
        #define   DM      0x04  
        #define   LDSC    0x02  

  u8	nc_dsa;		
  u8	nc_dsa1;
  u8	nc_dsa2;
  u8	nc_dsa3;

  u8	nc_istat;	
        #define   CABRT   0x80  
        #define   SRST    0x40  
        #define   SIGP    0x20  
        #define   SEM     0x10  
        #define   CON     0x08  
        #define   INTF    0x04  
        #define   SIP     0x02  
        #define   DIP     0x01  

  u8	nc_istat1;	
        #define   FLSH    0x04  
        #define   SCRUN   0x02  
        #define   SIRQD   0x01  

  u8	nc_mbox0;	
  u8	nc_mbox1;	

	u8	nc_ctest0;
  u8	nc_ctest1;

  u8	nc_ctest2;
	#define   CSIGP   0x40
				

  u8	nc_ctest3;
	#define   FLF     0x08  
	#define   CLF	  0x04	
	#define   FM      0x02  
	#define   WRIE    0x01  
				

  u32	nc_temp;	

	u8	nc_dfifo;
  u8	nc_ctest4;
	#define   BDIS    0x80  
	#define   MPEE    0x08  

  u8	nc_ctest5;
	#define   DFS     0x20  
				

  u8	nc_ctest6;

  u32	nc_dbc;		
  u32	nc_dnad;	
  u32	nc_dsp;		
  u32	nc_dsps;	

  u8	nc_scratcha;	
  u8	nc_scratcha1;
  u8	nc_scratcha2;
  u8	nc_scratcha3;

  u8	nc_dmode;
	#define   BL_2    0x80  
	#define   BL_1    0x40  
	#define   ERL     0x08  
	#define   ERMP    0x04  
	#define   BOF     0x02  

  u8	nc_dien;
  u8	nc_sbr;

  u8	nc_dcntl;	
	#define   CLSE    0x80  
	#define   PFF     0x40  
	#define   PFEN    0x20  
	#define   SSM     0x10  
	#define   IRQM    0x08  
	#define   STD     0x04  
	#define   IRQD    0x02  
 	#define	  NOCOM   0x01	
				

  u32	nc_adder;

  u16	nc_sien;	
  u16	nc_sist;	
        #define   SBMC    0x1000
        #define   STO     0x0400
        #define   GEN     0x0200
        #define   HTH     0x0100
        #define   MA      0x80  
        #define   CMP     0x40  
        #define   SEL     0x20  
        #define   RSL     0x10  
        #define   SGE     0x08  
        #define   UDC     0x04  
        #define   RST     0x02  
        #define   PAR     0x01  

  u8	nc_slpar;
  u8	nc_swide;
  u8	nc_macntl;
  u8	nc_gpcntl;
  u8	nc_stime0;	
  u8	nc_stime1;	
  u16	nc_respid;	

  u8	nc_stest0;

  u8	nc_stest1;
	#define   SCLK    0x80	
	#define   DBLEN   0x08	
	#define   DBLSEL  0x04	
  

  u8	nc_stest2;
	#define   ROF     0x40	
	#define   EXT     0x02  

  u8	nc_stest3;
	#define   TE     0x80	
	#define   HSC    0x20	
	#define   CSF    0x02	

  u16	nc_sidl;	
  u8	nc_stest4;
	#define   SMODE  0xc0	
	#define    SMODE_HVD 0x40	
	#define    SMODE_SE  0x80	
	#define    SMODE_LVD 0xc0	
	#define   LCKFRQ 0x20	
				

  u8	nc_53_;
  u16	nc_sodl;	
	u8	nc_ccntl0;	
	#define   ENPMJ  0x80	
	#define   PMJCTL 0x40	
	#define   ENNDJ  0x20	
	#define   DISFC  0x10	
	#define   DILS   0x02	
	#define   DPR    0x01	

	u8	nc_ccntl1;	
	#define   ZMOD   0x80	
	#define   DDAC   0x08	
	#define   XTIMOD 0x04	
	#define   EXTIBMV 0x02	
	#define   EXDBMV 0x01	

  u16	nc_sbdl;	
  u16	nc_5a_;

  u8	nc_scr0;	
  u8	nc_scr1;
  u8	nc_scr2;
  u8	nc_scr3;

  u8	nc_scrx[64];	
	u32	nc_mmrs;	
	u32	nc_mmws;	
	u32	nc_sfs;		
	u32	nc_drs;		
	u32	nc_sbms;	
	u32	nc_dbms;	
	u32	nc_dnad64;	
	u16	nc_scntl4;	
	#define   U3EN    0x80	
	#define   AIPCKEN 0x40  
				
	#define   XCLKH_DT 0x08 
	#define   XCLKH_ST 0x04 
	#define   XCLKS_DT 0x02 
	#define   XCLKS_ST 0x01 
	u8	nc_aipcntl0;	
	u8	nc_aipcntl1;	
	#define DISAIP  0x08	
	u32	nc_pmjad1;	
	u32	nc_pmjad2;	
	u8	nc_rbc;		
	u8	nc_rbc1;
	u8	nc_rbc2;
	u8	nc_rbc3;

	u8	nc_ua;		
	u8	nc_ua1;
	u8	nc_ua2;
	u8	nc_ua3;
	u32	nc_esa;		
	u8	nc_ia;		
	u8	nc_ia1;
	u8	nc_ia2;
	u8	nc_ia3;
	u32	nc_sbc;		
	u32	nc_csbc;	
                                
	u16    nc_crcpad;	
	u8     nc_crccntl0;	
	#define   SNDCRC  0x10	
	u8     nc_crccntl1;	
	u32    nc_crcdata;	
	u32    nc_e8_;
	u32    nc_ec_;
	u16    nc_dfbc;		 
};


#define REGJ(p,r) (offsetof(struct sym_reg, p ## r))
#define REG(r) REGJ (nc_, r)


#define	SCR_DATA_OUT	0x00000000
#define	SCR_DATA_IN	0x01000000
#define	SCR_COMMAND	0x02000000
#define	SCR_STATUS	0x03000000
#define	SCR_DT_DATA_OUT	0x04000000
#define	SCR_DT_DATA_IN	0x05000000
#define SCR_MSG_OUT	0x06000000
#define SCR_MSG_IN      0x07000000
#define SCR_ILG_OUT	0x04000000
#define SCR_ILG_IN	0x05000000


#define OPC_MOVE          0x08000000

#define SCR_MOVE_ABS(l) ((0x00000000 | OPC_MOVE) | (l))
#define SCR_MOVE_TBL     (0x10000000 | OPC_MOVE)

#define SCR_CHMOV_ABS(l) ((0x00000000) | (l))
#define SCR_CHMOV_TBL     (0x10000000)

#ifdef SYM_CONF_TARGET_ROLE_SUPPORT

#define OPC_TCHMOVE        0x08000000

#define SCR_TCHMOVE_ABS(l) ((0x20000000 | OPC_TCHMOVE) | (l))
#define SCR_TCHMOVE_TBL     (0x30000000 | OPC_TCHMOVE)

#define SCR_TMOV_ABS(l)    ((0x20000000) | (l))
#define SCR_TMOV_TBL        (0x30000000)
#endif

struct sym_tblmove {
        u32  size;
        u32  addr;
};


#define	SCR_SEL_ABS	0x40000000
#define	SCR_SEL_ABS_ATN	0x41000000
#define	SCR_SEL_TBL	0x42000000
#define	SCR_SEL_TBL_ATN	0x43000000

#ifdef SYM_CONF_TARGET_ROLE_SUPPORT
#define	SCR_RESEL_ABS     0x40000000
#define	SCR_RESEL_ABS_ATN 0x41000000
#define	SCR_RESEL_TBL     0x42000000
#define	SCR_RESEL_TBL_ATN 0x43000000
#endif

struct sym_tblsel {
        u_char  sel_scntl4;	
        u_char  sel_sxfer;
        u_char  sel_id;
        u_char  sel_scntl3;
};

#define SCR_JMP_REL     0x04000000
#define SCR_ID(id)	(((u32)(id)) << 16)


#define	SCR_WAIT_DISC	0x48000000
#define SCR_WAIT_RESEL  0x50000000

#ifdef SYM_CONF_TARGET_ROLE_SUPPORT
#define	SCR_DISCONNECT	0x48000000
#endif


#define SCR_SET(f)     (0x58000000 | (f))
#define SCR_CLR(f)     (0x60000000 | (f))

#define	SCR_CARRY	0x00000400
#define	SCR_TRG		0x00000200
#define	SCR_ACK		0x00000040
#define	SCR_ATN		0x00000008



#define SCR_NO_FLUSH 0x01000000

#define SCR_COPY(n) (0xc0000000 | SCR_NO_FLUSH | (n))
#define SCR_COPY_F(n) (0xc0000000 | (n))


#define SCR_REG_OFS(ofs) ((((ofs) & 0x7f) << 16ul) + ((ofs) & 0x80)) 

#define SCR_SFBR_REG(reg,op,data) \
        (0x68000000 | (SCR_REG_OFS(REG(reg))) | (op) | (((data)&0xff)<<8ul))

#define SCR_REG_SFBR(reg,op,data) \
        (0x70000000 | (SCR_REG_OFS(REG(reg))) | (op) | (((data)&0xff)<<8ul))

#define SCR_REG_REG(reg,op,data) \
        (0x78000000 | (SCR_REG_OFS(REG(reg))) | (op) | (((data)&0xff)<<8ul))


#define      SCR_LOAD   0x00000000
#define      SCR_SHL    0x01000000
#define      SCR_OR     0x02000000
#define      SCR_XOR    0x03000000
#define      SCR_AND    0x04000000
#define      SCR_SHR    0x05000000
#define      SCR_ADD    0x06000000
#define      SCR_ADDC   0x07000000

#define      SCR_SFBR_DATA   (0x00800000>>8ul)	


#define	SCR_FROM_REG(reg) \
	SCR_REG_SFBR(reg,SCR_OR,0)

#define	SCR_TO_REG(reg) \
	SCR_SFBR_REG(reg,SCR_OR,0)

#define	SCR_LOAD_REG(reg,data) \
	SCR_REG_REG(reg,SCR_LOAD,data)

#define SCR_LOAD_SFBR(data) \
        (SCR_REG_SFBR (gpreg, SCR_LOAD, data))


#define SCR_REG_OFS2(ofs) (((ofs) & 0xff) << 16ul)
#define SCR_NO_FLUSH2	0x02000000
#define SCR_DSA_REL2	0x10000000

#define SCR_LOAD_R(reg, how, n) \
        (0xe1000000 | how | (SCR_REG_OFS2(REG(reg))) | (n))

#define SCR_STORE_R(reg, how, n) \
        (0xe0000000 | how | (SCR_REG_OFS2(REG(reg))) | (n))

#define SCR_LOAD_ABS(reg, n)	SCR_LOAD_R(reg, SCR_NO_FLUSH2, n)
#define SCR_LOAD_REL(reg, n)	SCR_LOAD_R(reg, SCR_NO_FLUSH2|SCR_DSA_REL2, n)
#define SCR_LOAD_ABS_F(reg, n)	SCR_LOAD_R(reg, 0, n)
#define SCR_LOAD_REL_F(reg, n)	SCR_LOAD_R(reg, SCR_DSA_REL2, n)

#define SCR_STORE_ABS(reg, n)	SCR_STORE_R(reg, SCR_NO_FLUSH2, n)
#define SCR_STORE_REL(reg, n)	SCR_STORE_R(reg, SCR_NO_FLUSH2|SCR_DSA_REL2,n)
#define SCR_STORE_ABS_F(reg, n)	SCR_STORE_R(reg, 0, n)
#define SCR_STORE_REL_F(reg, n)	SCR_STORE_R(reg, SCR_DSA_REL2, n)



#define SCR_NO_OP       0x80000000
#define SCR_JUMP        0x80080000
#define SCR_JUMP64      0x80480000
#define SCR_JUMPR       0x80880000
#define SCR_CALL        0x88080000
#define SCR_CALLR       0x88880000
#define SCR_RETURN      0x90080000
#define SCR_INT         0x98080000
#define SCR_INT_FLY     0x98180000

#define IFFALSE(arg)   (0x00080000 | (arg))
#define IFTRUE(arg)    (0x00000000 | (arg))

#define WHEN(phase)    (0x00030000 | (phase))
#define IF(phase)      (0x00020000 | (phase))

#define DATA(D)        (0x00040000 | ((D) & 0xff))
#define MASK(D,M)      (0x00040000 | (((M ^ 0xff) & 0xff) << 8ul)|((D) & 0xff))

#define CARRYSET       (0x00200000)



#define	M_COMPLETE	COMMAND_COMPLETE
#define	M_EXTENDED	EXTENDED_MESSAGE
#define	M_SAVE_DP	SAVE_POINTERS
#define	M_RESTORE_DP	RESTORE_POINTERS
#define	M_DISCONNECT	DISCONNECT
#define	M_ID_ERROR	INITIATOR_ERROR
#define	M_ABORT		ABORT_TASK_SET
#define	M_REJECT	MESSAGE_REJECT
#define	M_NOOP		NOP
#define	M_PARITY	MSG_PARITY_ERROR
#define	M_LCOMPLETE	LINKED_CMD_COMPLETE
#define	M_FCOMPLETE	LINKED_FLG_CMD_COMPLETE
#define	M_RESET		TARGET_RESET
#define	M_ABORT_TAG	ABORT_TASK
#define	M_CLEAR_QUEUE	CLEAR_TASK_SET
#define	M_INIT_REC	INITIATE_RECOVERY
#define	M_REL_REC	RELEASE_RECOVERY
#define	M_TERMINATE	(0x11)
#define	M_SIMPLE_TAG	SIMPLE_QUEUE_TAG
#define	M_HEAD_TAG	HEAD_OF_QUEUE_TAG
#define	M_ORDERED_TAG	ORDERED_QUEUE_TAG
#define	M_IGN_RESIDUE	IGNORE_WIDE_RESIDUE

#define	M_X_MODIFY_DP	EXTENDED_MODIFY_DATA_POINTER
#define	M_X_SYNC_REQ	EXTENDED_SDTR
#define	M_X_WIDE_REQ	EXTENDED_WDTR
#define	M_X_PPR_REQ	EXTENDED_PPR

#define	PPR_OPT_IU	(0x01)
#define	PPR_OPT_DT	(0x02)
#define	PPR_OPT_QAS	(0x04)
#define PPR_OPT_MASK	(0x07)


#define	S_GOOD		SAM_STAT_GOOD
#define	S_CHECK_COND	SAM_STAT_CHECK_CONDITION
#define	S_COND_MET	SAM_STAT_CONDITION_MET
#define	S_BUSY		SAM_STAT_BUSY
#define	S_INT		SAM_STAT_INTERMEDIATE
#define	S_INT_COND_MET	SAM_STAT_INTERMEDIATE_CONDITION_MET
#define	S_CONFLICT	SAM_STAT_RESERVATION_CONFLICT
#define	S_TERMINATED	SAM_STAT_COMMAND_TERMINATED
#define	S_QUEUE_FULL	SAM_STAT_TASK_SET_FULL
#define	S_ILLEGAL	(0xff)

#endif 
