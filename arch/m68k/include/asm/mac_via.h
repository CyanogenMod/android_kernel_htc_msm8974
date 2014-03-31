
#ifndef _ASM_MAC_VIA_H_
#define _ASM_MAC_VIA_H_


#define VIA1_BASE	(0x50F00000)
#define VIA2_BASE	(0x50F02000)
#define  RBV_BASE	(0x50F26000)


#define VIA1A_vSccWrReq	0x80	
#define VIA1A_vRev8	0x40	
#define VIA1A_vHeadSel	0x20	
#define VIA1A_vOverlay	0x10    
#define VIA1A_vSync	0x08    

#define VIA1A_vVolume	0x07	
#define VIA1A_CPUID0	0x02	
#define VIA1A_CPUID1	0x04	
#define VIA1A_CPUID2	0x10	
#define VIA1A_CPUID3	0x40	

#define VIA1B_vSound	0x80	
#define VIA1B_vMystery	0x40    
#define VIA1B_vADBS2	0x20	
#define VIA1B_vADBS1	0x10	
#define VIA1B_vADBInt	0x08	
#define VIA1B_vRTCEnb	0x04	
#define VIA1B_vRTCClk	0x02    
#define VIA1B_vRTCData	0x01    

#define	EVRB_XCVR	0x08	
#define	EVRB_FULL	0x10	
#define	EVRB_SYSES	0x20	
#define	EVRB_AUXIE	0x00	
#define	EVRB_AUXID	0x40	
#define	EVRB_SFTWRIE	0x00	
#define	EVRB_SFTWRID	0x80	


#define VIA2A_vRAM1	0x80	
#define VIA2A_vRAM0	0x40	
#define VIA2A_vIRQE	0x20	
#define VIA2A_vIRQD	0x10	
#define VIA2A_vIRQC	0x08	
#define VIA2A_vIRQB	0x04	
#define VIA2A_vIRQA	0x02	
#define VIA2A_vIRQ9	0x01	



#define VIA2B_vVBL	0x80	
#define VIA2B_vSndJck	0x40	
#define VIA2B_vTfr0	0x20	
#define VIA2B_vTfr1	0x10	
#define VIA2B_vMode32	0x08	
#define VIA2B_vPower	0x04	
#define VIA2B_vBusLk	0x02	
#define VIA2B_vCDis	0x01	




#define vBufB	0x0000	
#define vBufAH	0x0200  
#define vDirB	0x0400  
#define vDirA	0x0600  
#define vT1CL	0x0800  
#define vT1CH	0x0a00  
#define vT1LL	0x0c00  
#define vT1LH	0x0e00  
#define vT2CL	0x1000  
#define vT2CH	0x1200  
#define vSR	0x1400  
#define vACR	0x1600  
#define vPCR	0x1800  
#define vIFR	0x1a00  
#define vIER	0x1c00  
#define vBufA	0x1e00  


#define rBufB   0x0000  
#define rExp	0x0001	
#define rSIFR	0x0002  
#define rIFR	0x1a03  
#define rMonP   0x0010  
#define rChpT   0x0011  
#define rSIER   0x0012  
#define rIER    0x1c13  
#define rBufA	rSIFR   

#define RBV_DEPTH  0x07	
#define RBV_MONID  0x38	
#define RBV_VIDOFF 0x40	
#define MON_15BW   (1<<3) 
#define MON_IIGS   (2<<3) 
#define MON_15RGB  (5<<3) 
#define MON_12OR13 (6<<3) 
#define MON_NONE   (7<<3) 

#define IER_SET_BIT(b) (0x80 | (1<<(b)) )
#define IER_CLR_BIT(b) (0x7F & (1<<(b)) )

#ifndef __ASSEMBLY__

extern volatile __u8 *via1,*via2;
extern int rbv_present,via_alt_mapping;

extern void via_register_interrupts(void);
extern void via_irq_enable(int);
extern void via_irq_disable(int);
extern void via_nubus_irq_startup(int irq);
extern void via_nubus_irq_shutdown(int irq);
extern void via1_irq(unsigned int irq, struct irq_desc *desc);
extern void via1_set_head(int);
extern int via2_scsi_drq_pending(void);

static inline int rbv_set_video_bpp(int bpp)
{
	char val = (bpp==1)?0:(bpp==2)?1:(bpp==4)?2:(bpp==8)?3:-1;
	if (!rbv_present || val<0) return -1;
	via2[rMonP] = (via2[rMonP] & ~RBV_DEPTH) | val;
	return 0;
}

#endif 

#endif 
