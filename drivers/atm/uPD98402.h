 
/* Written 1995 by Werner Almesberger, EPFL LRC */


#ifndef DRIVERS_ATM_uPD98402_H
#define DRIVERS_ATM_uPD98402_H


#define uPD98402_CMR		0x00	
#define uPD98402_MDR		0x01	
#define uPD98402_PICR		0x02	
#define uPD98402_PIMR		0x03	
#define uPD98402_ACR		0x04	
#define uPD98402_ACMR		0x05	
#define uPD98402_PCR		0x06	
#define uPD98402_PCMR		0x07	
#define uPD98402_IACM		0x08	
#define uPD98402_B1ECT		0x09	
#define uPD98402_B2ECT		0x0a	
#define uPD98402_B3ECT		0x0b	
#define uPD98402_PFECB		0x0c	
#define uPD98402_LECCT		0x0d	
#define uPD98402_HECCT		0x0e	
#define uPD98402_FJCT		0x0f	
#define uPD98402_PCOCR		0x10	
#define uPD98402_PCOMR		0x11	
#define uPD98402_C11T		0x20	
#define uPD98402_C12T		0x21	
#define uPD98402_C13T		0x22	
#define uPD98402_F1T		0x23	
#define uPD98402_K2T		0x25	
#define uPD98402_C2T		0x26	
#define uPD98402_F2T		0x27	
#define uPD98402_C11R		0x30	
#define uPD98402_C12R		0x31	
#define uPD98402_C13R		0x32	
#define uPD98402_F1R		0x33	
#define uPD98402_K2R		0x35	
#define uPD98402_C2R		0x36	
#define uPD98402_F2R		0x37	

#define uPD98402_CMR_PFRF	0x01	
#define uPD98402_CMR_LFRF	0x02	
#define uPD98402_CMR_PAIS	0x04	
#define uPD98402_CMR_LAIS	0x08	

#define uPD98402_MDR_ALP	0x01	
#define uPD98402_MDR_TPLP	0x02	
#define uPD98402_MDR_RPLP	0x04	
#define uPD98402_MDR_SS0	0x08	
#define uPD98402_MDR_SS1	0x10	
#define uPD98402_MDR_SS_MASK	0x18	
#define uPD98402_MDR_SS_SHIFT	3	
#define uPD98402_MDR_HEC	0x20	
#define uPD98402_MDR_FSR	0x40	
#define uPD98402_MDR_CSR	0x80	

#define uPD98402_INT_PFM	0x01	
#define uPD98402_INT_ALM	0x02	
#define uPD98402_INT_RFO	0x04	
#define uPD98402_INT_PCO	0x08	
#define uPD98402_INT_OTD	0x20	
#define uPD98402_INT_LOS	0x40	
#define uPD98402_INT_LOF	0x80	

#define uPD98402_ALM_PFRF	0x01	
#define uPD98402_ALM_LFRF	0x02	
#define uPD98402_ALM_PAIS	0x04	
#define uPD98402_ALM_LAIS	0x08	
#define uPD98402_ALM_LOD	0x10	
#define uPD98402_ALM_LOP	0x20	
#define uPD98402_ALM_OOF	0x40	

#define uPD98402_PFM_PFEB	0x01	
#define uPD98402_PFM_LFEB	0x02	
#define uPD98402_PFM_B3E	0x04	
#define uPD98402_PFM_B2E	0x08	
#define uPD98402_PFM_B1E	0x10	
#define uPD98402_PFM_FJ		0x20	

#define uPD98402_IACM_PFRF	0x01	
#define uPD98402_IACM_LFRF	0x02	

#define uPD98402_PCO_B1EC	0x01	
#define uPD98402_PCO_B2EC	0x02	
#define uPD98402_PCO_B3EC	0x04	
#define uPD98402_PCO_PFBC	0x08	
#define uPD98402_PCO_LFBC	0x10	
#define uPD98402_PCO_HECC	0x20	
#define uPD98402_PCO_FJC	0x40	


int uPD98402_init(struct atm_dev *dev);

#endif
