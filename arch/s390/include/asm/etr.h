/*
 *  include/asm-s390/etr.h
 *
 *  Copyright IBM Corp. 2006
 *  Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com)
 */
#ifndef __S390_ETR_H
#define __S390_ETR_H

struct etr_eacr {
	unsigned int e0		: 1;	
	unsigned int e1		: 1;	
	unsigned int _pad0	: 5;	
	unsigned int dp		: 1;	
	unsigned int p0		: 1;	
	unsigned int p1		: 1;	
	unsigned int _pad1	: 3;	
	unsigned int ea		: 1;	
	unsigned int es		: 1;	
	unsigned int sl		: 1;	
} __attribute__ ((packed));

enum etr_psc {
	etr_psc_operational = 0,
	etr_psc_semi_operational = 1,
	etr_psc_protocol_error =  4,
	etr_psc_no_symbols = 8,
	etr_psc_no_signal = 12,
	etr_psc_pps_mode = 13
};

enum etr_lpsc {
	etr_lpsc_operational_step = 0,
	etr_lpsc_operational_alt = 1,
	etr_lpsc_semi_operational = 2,
	etr_lpsc_protocol_error =  4,
	etr_lpsc_no_symbol_sync = 8,
	etr_lpsc_no_signal = 12,
	etr_lpsc_pps_mode = 13
};

struct etr_esw {
	struct etr_eacr eacr;		
	unsigned int y		: 1;	
	unsigned int _pad0	: 5;	
	unsigned int p		: 1;	
	unsigned int q		: 1;	
	unsigned int psc0	: 4;	
	unsigned int psc1	: 4;	
} __attribute__ ((packed));

struct etr_slsw {
	unsigned int vv1	: 1;	
	unsigned int vv2	: 1;	
	unsigned int vv3	: 1;	
	unsigned int vv4	: 1;	
	unsigned int _pad0	: 19;	
	unsigned int n		: 1;	
	unsigned int v1		: 1;	
	unsigned int v2		: 1;	
	unsigned int v3		: 1;	
	unsigned int v4		: 1;	
	unsigned int _pad1	: 4;	
} __attribute__ ((packed));

struct etr_edf1 {
	unsigned int u		: 1;	
	unsigned int _pad0	: 1;	
	unsigned int r		: 1;	
	unsigned int _pad1	: 4;	
	unsigned int a		: 1;	
	unsigned int net_id	: 8;	
	unsigned int etr_id	: 8;	
	unsigned int etr_pn	: 8;	
} __attribute__ ((packed));

struct etr_edf2 {
	unsigned int etv	: 32;	
} __attribute__ ((packed));

struct etr_edf3 {
	unsigned int rc		: 8;	
	unsigned int _pad0	: 3;	
	unsigned int c		: 1;	
	unsigned int tc		: 4;	
	unsigned int blto	: 8;	
					
	unsigned int buo	: 8;	
					
} __attribute__ ((packed));

struct etr_edf4 {
	unsigned int ed		: 8;	
	unsigned int _pad0	: 1;	
	unsigned int buc	: 5;	
					
	unsigned int em		: 6;	
	unsigned int dc		: 6;	
	unsigned int sc		: 6;	
} __attribute__ ((packed));

struct etr_aib {
	struct etr_esw esw;
	struct etr_slsw slsw;
	unsigned long long tsp;
	struct etr_edf1 edf1;
	struct etr_edf2 edf2;
	struct etr_edf3 edf3;
	struct etr_edf4 edf4;
	unsigned int reserved[16];
} __attribute__ ((packed,aligned(8)));

struct etr_irq_parm {
	unsigned int _pad0	: 8;
	unsigned int pc0	: 1;	
	unsigned int pc1	: 1;	
	unsigned int _pad1	: 3;
	unsigned int eai	: 1;	
	unsigned int _pad2	: 18;
} __attribute__ ((packed));

struct etr_ptff_qto {
	unsigned long long physical_clock;
	unsigned long long tod_offset;
	unsigned long long logical_tod_offset;
	unsigned long long tod_epoch_difference;
} __attribute__ ((packed));

static inline int etr_setr(struct etr_eacr *ctrl)
{
	int rc = -ENOSYS;

	asm volatile(
		"	.insn	s,0xb2160000,%1\n"
		"0:	la	%0,0\n"
		"1:\n"
		EX_TABLE(0b,1b)
		: "+d" (rc) : "Q" (*ctrl));
	return rc;
}

static inline int etr_stetr(struct etr_aib *aib)
{
	int rc = -ENOSYS;

	asm volatile(
		"	.insn	s,0xb2170000,%1\n"
		"0:	la	%0,0\n"
		"1:\n"
		EX_TABLE(0b,1b)
		: "+d" (rc) : "Q" (*aib));
	return rc;
}

static inline int etr_steai(struct etr_aib *aib, unsigned int func)
{
	register unsigned int reg0 asm("0") = func;
	int rc = -ENOSYS;

	asm volatile(
		"	.insn	s,0xb2b30000,%1\n"
		"0:	la	%0,0\n"
		"1:\n"
		EX_TABLE(0b,1b)
		: "+d" (rc) : "Q" (*aib), "d" (reg0));
	return rc;
}

#define ETR_STEAI_STEPPING_PORT		0x10
#define ETR_STEAI_ALTERNATE_PORT	0x11
#define ETR_STEAI_PORT_0		0x12
#define ETR_STEAI_PORT_1		0x13

static inline int etr_ptff(void *ptff_block, unsigned int func)
{
	register unsigned int reg0 asm("0") = func;
	register unsigned long reg1 asm("1") = (unsigned long) ptff_block;
	int rc = -ENOSYS;

	asm volatile(
		"	.word	0x0104\n"
		"	ipm	%0\n"
		"	srl	%0,28\n"
		: "=d" (rc), "=m" (ptff_block)
		: "d" (reg0), "d" (reg1), "m" (ptff_block) : "cc");
	return rc;
}

#define ETR_PTFF_QAF	0x00	
#define ETR_PTFF_QTO	0x01	
#define ETR_PTFF_QSI	0x02	
#define ETR_PTFF_ATO	0x40	
#define ETR_PTFF_STO	0x41	
#define ETR_PTFF_SFS	0x42	
#define ETR_PTFF_SGS	0x43	

void etr_switch_to_local(void);
void etr_sync_check(void);

struct stp_irq_parm {
	unsigned int _pad0	: 14;
	unsigned int tsc	: 1;	
	unsigned int lac	: 1;	
	unsigned int tcpc	: 1;	
	unsigned int _pad2	: 15;
} __attribute__ ((packed));

#define STP_OP_SYNC	1
#define STP_OP_CTRL	3

struct stp_sstpi {
	unsigned int rsvd0;
	unsigned int rsvd1 : 8;
	unsigned int stratum : 8;
	unsigned int vbits : 16;
	unsigned int leaps : 16;
	unsigned int tmd : 4;
	unsigned int ctn : 4;
	unsigned int rsvd2 : 3;
	unsigned int c : 1;
	unsigned int tst : 4;
	unsigned int tzo : 16;
	unsigned int dsto : 16;
	unsigned int ctrl : 16;
	unsigned int rsvd3 : 16;
	unsigned int tto;
	unsigned int rsvd4;
	unsigned int ctnid[3];
	unsigned int rsvd5;
	unsigned int todoff[4];
	unsigned int rsvd6[48];
} __attribute__ ((packed));

void stp_sync_check(void);
void stp_island_check(void);

#endif 
