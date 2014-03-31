
#ifndef _ASM_SPARC_SUNBPP_H
#define _ASM_SPARC_SUNBPP_H

struct bpp_regs {
  
  __volatile__ __u32 p_csr;		
  __volatile__ __u32 p_addr;		
  __volatile__ __u32 p_bcnt;		
  __volatile__ __u32 p_tst_csr;		
  
  __volatile__ __u16 p_hcr;		
  __volatile__ __u16 p_ocr;		
  __volatile__ __u8 p_dr;		
  __volatile__ __u8 p_tcr;		
  __volatile__ __u8 p_or;		
  __volatile__ __u8 p_ir;		
  __volatile__ __u16 p_icr;		
};

#define P_HCR_TEST      0x8000      
#define P_HCR_DSW       0x7f00      
#define P_HCR_DDS       0x007f      

#define P_OCR_MEM_CLR   0x8000
#define P_OCR_DATA_SRC  0x4000      
#define P_OCR_DS_DSEL   0x2000      
#define P_OCR_BUSY_DSEL 0x1000      
#define P_OCR_ACK_DSEL  0x0800      
#define P_OCR_EN_DIAG   0x0400
#define P_OCR_BUSY_OP   0x0200      
#define P_OCR_ACK_OP    0x0100      
#define P_OCR_SRST      0x0080      
#define P_OCR_IDLE      0x0008      
#define P_OCR_V_ILCK    0x0002      
#define P_OCR_EN_VER    0x0001      

#define P_TCR_DIR       0x08
#define P_TCR_BUSY      0x04
#define P_TCR_ACK       0x02
#define P_TCR_DS        0x01        

#define P_OR_V3         0x20        
#define P_OR_V2         0x10        
#define P_OR_V1         0x08        
#define P_OR_INIT       0x04
#define P_OR_AFXN       0x02        
#define P_OR_SLCT_IN    0x01

#define P_IR_PE         0x04
#define P_IR_SLCT       0x02
#define P_IR_ERR        0x01

#define P_DS_IRQ        0x8000      
#define P_ACK_IRQ       0x4000      
#define P_BUSY_IRQ      0x2000      
#define P_PE_IRQ        0x1000      
#define P_SLCT_IRQ      0x0800      
#define P_ERR_IRQ       0x0400      
#define P_DS_IRQ_EN     0x0200      
#define P_ACK_IRQ_EN    0x0100      
#define P_BUSY_IRP      0x0080      
#define P_BUSY_IRQ_EN   0x0040      
#define P_PE_IRP        0x0020      
#define P_PE_IRQ_EN     0x0010      
#define P_SLCT_IRP      0x0008      
#define P_SLCT_IRQ_EN   0x0004      
#define P_ERR_IRP       0x0002      
#define P_ERR_IRQ_EN    0x0001      

#endif 
