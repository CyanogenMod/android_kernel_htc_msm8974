#ifdef __KERNEL__
#ifndef __ASM_POWERPC_REG_FSL_EMB_H__
#define __ASM_POWERPC_REG_FSL_EMB_H__

#ifndef __ASSEMBLY__
#define mfpmr(rn)	({unsigned int rval; \
			asm volatile("mfpmr %0," __stringify(rn) \
				     : "=r" (rval)); rval;})
#define mtpmr(rn, v)	asm volatile("mtpmr " __stringify(rn) ",%0" : : "r" (v))
#endif 

#define PMRN_PMC0	0x010	
#define PMRN_PMC1	0x011	
#define PMRN_PMC2	0x012	
#define PMRN_PMC3	0x013	
#define PMRN_PMLCA0	0x090	
#define PMRN_PMLCA1	0x091	
#define PMRN_PMLCA2	0x092	
#define PMRN_PMLCA3	0x093	

#define PMLCA_FC	0x80000000	
#define PMLCA_FCS	0x40000000	
#define PMLCA_FCU	0x20000000	
#define PMLCA_FCM1	0x10000000	
#define PMLCA_FCM0	0x08000000	
#define PMLCA_CE	0x04000000	

#define PMLCA_EVENT_MASK 0x00ff0000	
#define PMLCA_EVENT_SHIFT	16

#define PMRN_PMLCB0	0x110	
#define PMRN_PMLCB1	0x111	
#define PMRN_PMLCB2	0x112	
#define PMRN_PMLCB3	0x113	

#define PMLCB_THRESHMUL_MASK	0x0700	
#define PMLCB_THRESHMUL_SHIFT	8

#define PMLCB_THRESHOLD_MASK	0x003f	
#define PMLCB_THRESHOLD_SHIFT	0

#define PMRN_PMGC0	0x190	

#define PMGC0_FAC	0x80000000	
#define PMGC0_PMIE	0x40000000	
#define PMGC0_FCECE	0x20000000	

#define PMRN_UPMC0	0x000	
#define PMRN_UPMC1	0x001	
#define PMRN_UPMC2	0x002	
#define PMRN_UPMC3	0x003	
#define PMRN_UPMLCA0	0x080	
#define PMRN_UPMLCA1	0x081	
#define PMRN_UPMLCA2	0x082	
#define PMRN_UPMLCA3	0x083	
#define PMRN_UPMLCB0	0x100	
#define PMRN_UPMLCB1	0x101	
#define PMRN_UPMLCB2	0x102	
#define PMRN_UPMLCB3	0x103	
#define PMRN_UPMGC0	0x180	


#endif 
#endif 
