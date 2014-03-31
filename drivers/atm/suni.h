 
/* Written 1995-2000 by Werner Almesberger, EPFL LRC/ICA */

#ifndef DRIVER_ATM_SUNI_H
#define DRIVER_ATM_SUNI_H

#include <linux/atmdev.h>
#include <linux/atmioc.h>
#include <linux/sonet.h>


#define SUNI_MRI		0x00	
#define SUNI_MC			0x01	
#define SUNI_MIS		0x02	
			  
#define SUNI_MCM		0x04	
#define SUNI_MCT		0x05	
#define SUNI_CSCS		0x06	
#define SUNI_CRCS		0x07	
			     
#define SUNI_RSOP_CIE		0x10	
#define SUNI_RSOP_SIS		0x11	
#define SUNI_RSOP_SBL		0x12	
#define SUNI_RSOP_SBM		0x13	
#define SUNI_TSOP_CTRL		0x14	
#define SUNI_TSOP_DIAG		0x15	
			     
#define SUNI_RLOP_CS		0x18	
#define SUNI_RLOP_IES		0x19	
#define SUNI_RLOP_LBL		0x1A	
#define SUNI_RLOP_LB		0x1B	
#define SUNI_RLOP_LBM		0x1C	
#define SUNI_RLOP_LFL		0x1D	
#define SUNI_RLOP_LF 		0x1E	
#define SUNI_RLOP_LFM		0x1F	
#define SUNI_TLOP_CTRL		0x20	
#define SUNI_TLOP_DIAG		0x21	
			     
#define SUNI_SSTB_CTRL		0x28
#define SUNI_RPOP_SC		0x30	
#define SUNI_RPOP_IS		0x31	
			     
#define SUNI_RPOP_IE		0x33	
			     
#define SUNI_RPOP_PSL		0x37	
#define SUNI_RPOP_PBL		0x38	
#define SUNI_RPOP_PBM		0x39	
#define SUNI_RPOP_PFL		0x3A	
#define SUNI_RPOP_PFM		0x3B	
			     
#define SUNI_RPOP_PBC		0x3D	
#define SUNI_RPOP_RC		0x3D	
			     
#define SUNI_TPOP_CD		0x40	
#define SUNI_TPOP_PC		0x41	
			     
#define SUNI_TPOP_APL		0x45	
#define SUNI_TPOP_APM		0x46	
			     
#define SUNI_TPOP_PSL		0x48	
#define SUNI_TPOP_PS		0x49	
			     
#define SUNI_RACP_CS		0x50	
#define SUNI_RACP_IES		0x51	
#define SUNI_RACP_MHP		0x52	
#define SUNI_RACP_MHM		0x53	
#define SUNI_RACP_CHEC		0x54	
#define SUNI_RACP_UHEC		0x55	
#define SUNI_RACP_RCCL		0x56	
#define SUNI_RACP_RCC		0x57	
#define SUNI_RACP_RCCM		0x58	
#define SUNI_RACP_CFG		0x59	
			     
#define SUNI_TACP_CS		0x60	
#define SUNI_TACP_IUCHP		0x61	
#define SUNI_TACP_IUCPOP	0x62	
#define SUNI_TACP_FIFO		0x63	
#define SUNI_TACP_TCCL		0x64	
#define SUNI_TACP_TCC		0x65	
#define SUNI_TACP_TCCM		0x66	
#define SUNI_TACP_CFG		0x67	
#define SUNI_SPTB_CTRL		0x68	
			     
#define	SUNI_MT			0x80	
			     



#define SUNI_MRI_ID		0x0f	
#define SUNI_MRI_ID_SHIFT 	0
#define SUNI_MRI_TYPE		0x70	
#define SUNI_MRI_TYPE_SHIFT 	4
#define SUNI_MRI_TYPE_PM5346	0x3	
#define SUNI_MRI_TYPE_PM5347	0x4	
#define SUNI_MRI_TYPE_PM5350	0x7	
#define SUNI_MRI_TYPE_PM5355	0x1	
#define SUNI_MRI_RESET		0x80	

#define SUNI_MCM_LLE		0x20	
#define SUNI_MCM_DLE		0x10	

#define SUNI_MCT_LOOPT		0x01	
#define SUNI_MCT_DLE		0x02	
#define SUNI_MCT_LLE		0x04	
#define SUNI_MCT_FIXPTR		0x20	
#define SUNI_MCT_LCDV		0x40	
#define SUNI_MCT_LCDE		0x80	
#define SUNI_RSOP_CIE_OOFE	0x01	
#define SUNI_RSOP_CIE_LOFE	0x02	
#define SUNI_RSOP_CIE_LOSE	0x04	
#define SUNI_RSOP_CIE_BIPEE	0x08	
#define SUNI_RSOP_CIE_FOOF	0x20	
#define SUNI_RSOP_CIE_DDS	0x40	

#define SUNI_RSOP_SIS_OOFV	0x01	
#define SUNI_RSOP_SIS_LOFV	0x02	
#define SUNI_RSOP_SIS_LOSV	0x04	
#define SUNI_RSOP_SIS_OOFI	0x08	
#define SUNI_RSOP_SIS_LOFI	0x10	
#define SUNI_RSOP_SIS_LOSI	0x20	
#define SUNI_RSOP_SIS_BIPEI	0x40	

#define SUNI_TSOP_CTRL_LAIS	0x01	
#define SUNI_TSOP_CTRL_DS	0x40	

#define SUNI_TSOP_DIAG_DFP	0x01	
#define SUNI_TSOP_DIAG_DBIP8	0x02	
#define SUNI_TSOP_DIAG_DLOS	0x04	

#define SUNI_TLOP_DIAG_DBIP	0x01	

#define SUNI_SSTB_CTRL_LEN16	0x01	

#define SUNI_RPOP_RC_ENSS	0x40	

#define SUNI_TPOP_DIAG_PAIS	0x01	
#define SUNI_TPOP_DIAG_DB3	0x02	

#define SUNI_TPOP_APM_APTR	0x03	
#define SUNI_TPOP_APM_APTR_SHIFT 0
#define SUNI_TPOP_APM_S		0x0c	
#define SUNI_TPOP_APM_S_SHIFT	2
#define SUNI_TPOP_APM_NDF	0xf0	 
#define SUNI_TPOP_APM_NDF_SHIFT	4

#define SUNI_TPOP_S_SONET	0	
#define SUNI_TPOP_S_SDH		2	

#define SUNI_RACP_IES_FOVRI	0x02	
#define SUNI_RACP_IES_UHCSI	0x04	
#define SUNI_RACP_IES_CHCSI	0x08	
#define SUNI_RACP_IES_OOCDI	0x10	
#define SUNI_RACP_IES_FIFOE	0x20	
#define SUNI_RACP_IES_HCSE	0x40	
#define SUNI_RACP_IES_OOCDE	0x80	

#define SUNI_TACP_CS_FIFORST	0x01	
#define SUNI_TACP_CS_DSCR	0x02	
#define SUNI_TACP_CS_HCAADD	0x04	
#define SUNI_TACP_CS_DHCS	0x10	
#define SUNI_TACP_CS_FOVRI	0x20	
#define SUNI_TACP_CS_TSOCI	0x40	
#define SUNI_TACP_CS_FIFOE	0x80	

#define SUNI_TACP_IUCHP_CLP	0x01	
#define SUNI_TACP_IUCHP_PTI	0x0e	
#define SUNI_TACP_IUCHP_PTI_SHIFT 1
#define SUNI_TACP_IUCHP_GFC	0xf0	
#define SUNI_TACP_IUCHP_GFC_SHIFT 4

#define SUNI_SPTB_CTRL_LEN16	0x01	

#define SUNI_MT_HIZIO		0x01	
#define SUNI_MT_HIZDATA		0x02	
#define SUNI_MT_IOTST		0x04	
#define SUNI_MT_DBCTRL		0x08	
#define SUNI_MT_PMCTST		0x10	
#define SUNI_MT_DS27_53		0x80	


#define SUNI_IDLE_PATTERN       0x6a    


#ifdef __KERNEL__
struct suni_priv {
	struct k_sonet_stats sonet_stats;	
	int loop_mode;				
	int type;				
	struct atm_dev *dev;			
	struct suni_priv *next;			
};

int suni_init(struct atm_dev *dev);
#endif

#endif
