/*
 * linux/include/asm-s390/qdio.h
 *
 * Copyright 2000,2008 IBM Corp.
 * Author(s): Utz Bacher <utz.bacher@de.ibm.com>
 *	      Jan Glauber <jang@linux.vnet.ibm.com>
 *
 */
#ifndef __QDIO_H__
#define __QDIO_H__

#include <linux/interrupt.h>
#include <asm/cio.h>
#include <asm/ccwdev.h>

#define QDIO_MAX_QUEUES_PER_IRQ		4
#define QDIO_MAX_BUFFERS_PER_Q		128
#define QDIO_MAX_BUFFERS_MASK		(QDIO_MAX_BUFFERS_PER_Q - 1)
#define QDIO_MAX_ELEMENTS_PER_BUFFER	16
#define QDIO_SBAL_SIZE			256

#define QDIO_QETH_QFMT			0
#define QDIO_ZFCP_QFMT			1
#define QDIO_IQDIO_QFMT			2

struct qdesfmt0 {
	u64 sliba;
	u64 sla;
	u64 slsba;
	u32	 : 32;
	u32 akey : 4;
	u32 bkey : 4;
	u32 ckey : 4;
	u32 dkey : 4;
	u32	 : 16;
} __attribute__ ((packed));

#define QDR_AC_MULTI_BUFFER_ENABLE 0x01

struct qdr {
	u32 qfmt   : 8;
	u32 pfmt   : 8;
	u32	   : 8;
	u32 ac	   : 8;
	u32	   : 8;
	u32 iqdcnt : 8;
	u32	   : 8;
	u32 oqdcnt : 8;
	u32	   : 8;
	u32 iqdsz  : 8;
	u32	   : 8;
	u32 oqdsz  : 8;
	
	u32 res[9];
	
	u64 qiba;
	u32	   : 32;
	u32 qkey   : 4;
	u32	   : 28;
	struct qdesfmt0 qdf0[126];
} __attribute__ ((packed, aligned(4096)));

#define QIB_AC_OUTBOUND_PCI_SUPPORTED	0x40
#define QIB_RFLAGS_ENABLE_QEBSM		0x80
#define QIB_RFLAGS_ENABLE_DATA_DIV	0x02

struct qib {
	u32 qfmt   : 8;
	u32 pfmt   : 8;
	u32 rflags : 8;
	u32 ac	   : 8;
	u32	   : 32;
	u64 isliba;
	u64 osliba;
	u32	   : 32;
	u32	   : 32;
	u8 ebcnam[8];
	
	u8 res[88];
	
	u8 parm[QDIO_MAX_BUFFERS_PER_Q];
} __attribute__ ((packed, aligned(256)));

struct slibe {
	u64 parms;
};

struct qaob {
	u64 res0[6];
	u8 res1;
	u8 res2;
	u8 res3;
	u8 aorc;
	u8 flags;
	u16 cbtbs;
	u8 sb_count;
	u64 sba[QDIO_MAX_ELEMENTS_PER_BUFFER];
	u16 dcount[QDIO_MAX_ELEMENTS_PER_BUFFER];
	u64 user0;
	u64 res4[2];
	u64 user1;
	u64 user2;
} __attribute__ ((packed, aligned(256)));

struct slib {
	u64 nsliba;
	u64 sla;
	u64 slsba;
	
	u8 res[1000];
	
	struct slibe slibe[QDIO_MAX_BUFFERS_PER_Q];
} __attribute__ ((packed, aligned(2048)));

#define SBAL_EFLAGS_LAST_ENTRY		0x40
#define SBAL_EFLAGS_CONTIGUOUS		0x20
#define SBAL_EFLAGS_FIRST_FRAG		0x04
#define SBAL_EFLAGS_MIDDLE_FRAG		0x08
#define SBAL_EFLAGS_LAST_FRAG		0x0c
#define SBAL_EFLAGS_MASK		0x6f

#define SBAL_SFLAGS0_PCI_REQ		0x40
#define SBAL_SFLAGS0_DATA_CONTINUATION	0x20

#define SBAL_SFLAGS0_TYPE_STATUS	0x00
#define SBAL_SFLAGS0_TYPE_WRITE		0x08
#define SBAL_SFLAGS0_TYPE_READ		0x10
#define SBAL_SFLAGS0_TYPE_WRITE_READ	0x18
#define SBAL_SFLAGS0_MORE_SBALS		0x04
#define SBAL_SFLAGS0_COMMAND		0x02
#define SBAL_SFLAGS0_LAST_SBAL		0x00
#define SBAL_SFLAGS0_ONLY_SBAL		SBAL_SFLAGS0_COMMAND
#define SBAL_SFLAGS0_MIDDLE_SBAL	SBAL_SFLAGS0_MORE_SBALS
#define SBAL_SFLAGS0_FIRST_SBAL (SBAL_SFLAGS0_MORE_SBALS | SBAL_SFLAGS0_COMMAND)

struct qdio_buffer_element {
	u8 eflags;
	
	u8 res1;
	
	u8 scount;
	u8 sflags;
	u32 length;
#ifdef CONFIG_32BIT
	
	void *res2;
	
#endif
	void *addr;
} __attribute__ ((packed, aligned(16)));

struct qdio_buffer {
	struct qdio_buffer_element element[QDIO_MAX_ELEMENTS_PER_BUFFER];
} __attribute__ ((packed, aligned(256)));

struct sl_element {
#ifdef CONFIG_32BIT
	
	unsigned long reserved;
	
#endif
	unsigned long sbal;
} __attribute__ ((packed));

struct sl {
	struct sl_element element[QDIO_MAX_BUFFERS_PER_Q];
} __attribute__ ((packed, aligned(1024)));

struct slsb {
	u8 val[QDIO_MAX_BUFFERS_PER_Q];
} __attribute__ ((packed, aligned(256)));

#define CHSC_AC2_MULTI_BUFFER_AVAILABLE	0x0080
#define CHSC_AC2_MULTI_BUFFER_ENABLED	0x0040
#define CHSC_AC2_DATA_DIV_AVAILABLE	0x0010
#define CHSC_AC2_DATA_DIV_ENABLED	0x0002

struct qdio_outbuf_state {
	u8 flags;
	struct qaob *aob;
	void *user;
};

#define QDIO_OUTBUF_STATE_FLAG_NONE	0x00
#define QDIO_OUTBUF_STATE_FLAG_PENDING	0x01

#define CHSC_AC1_INITIATE_INPUTQ	0x80


#define AC1_SIGA_INPUT_NEEDED		0x40	
#define AC1_SIGA_OUTPUT_NEEDED		0x20	
#define AC1_SIGA_SYNC_NEEDED		0x10	
#define AC1_AUTOMATIC_SYNC_ON_THININT	0x08	
#define AC1_AUTOMATIC_SYNC_ON_OUT_PCI	0x04	
#define AC1_SC_QEBSM_AVAILABLE		0x02	
#define AC1_SC_QEBSM_ENABLED		0x01	

#define CHSC_AC2_DATA_DIV_AVAILABLE	0x0010
#define CHSC_AC2_DATA_DIV_ENABLED	0x0002

#define CHSC_AC3_FORMAT2_CQ_AVAILABLE	0x8000

struct qdio_ssqd_desc {
	u8 flags;
	u8:8;
	u16 sch;
	u8 qfmt;
	u8 parm;
	u8 qdioac1;
	u8 sch_class;
	u8 pcnt;
	u8 icnt;
	u8:8;
	u8 ocnt;
	u8:8;
	u8 mbccnt;
	u16 qdioac2;
	u64 sch_token;
	u8 mro;
	u8 mri;
	u16 qdioac3;
	u16:16;
	u8:8;
	u8 mmwc;
} __attribute__ ((packed));

typedef void qdio_handler_t(struct ccw_device *, unsigned int, int,
			    int, int, unsigned long);

#define QDIO_ERROR_SIGA_TARGET			0x02
#define QDIO_ERROR_SIGA_ACCESS_EXCEPTION	0x10
#define QDIO_ERROR_SIGA_BUSY			0x20
#define QDIO_ERROR_ACTIVATE_CHECK_CONDITION	0x40
#define QDIO_ERROR_SLSB_STATE			0x80

#define QDIO_FLAG_CLEANUP_USING_CLEAR		0x01
#define QDIO_FLAG_CLEANUP_USING_HALT		0x02

struct qdio_initialize {
	struct ccw_device *cdev;
	unsigned char q_format;
	unsigned char qdr_ac;
	unsigned char adapter_name[8];
	unsigned int qib_param_field_format;
	unsigned char *qib_param_field;
	unsigned char qib_rflags;
	unsigned long *input_slib_elements;
	unsigned long *output_slib_elements;
	unsigned int no_input_qs;
	unsigned int no_output_qs;
	qdio_handler_t *input_handler;
	qdio_handler_t *output_handler;
	void (**queue_start_poll_array) (struct ccw_device *, int,
					  unsigned long);
	int scan_threshold;
	unsigned long int_parm;
	void **input_sbal_addr_array;
	void **output_sbal_addr_array;
	struct qdio_outbuf_state *output_sbal_state_array;
};

#define QDIO_STATE_INACTIVE		0x00000002 
#define QDIO_STATE_ESTABLISHED		0x00000004 
#define QDIO_STATE_ACTIVE		0x00000008 
#define QDIO_STATE_STOPPED		0x00000010 

#define QDIO_FLAG_SYNC_INPUT		0x01
#define QDIO_FLAG_SYNC_OUTPUT		0x02
#define QDIO_FLAG_PCI_OUT		0x10

extern int qdio_allocate(struct qdio_initialize *);
extern int qdio_establish(struct qdio_initialize *);
extern int qdio_activate(struct ccw_device *);
extern void qdio_release_aob(struct qaob *);
extern int do_QDIO(struct ccw_device *, unsigned int, int, unsigned int,
		   unsigned int);
extern int qdio_start_irq(struct ccw_device *, int);
extern int qdio_stop_irq(struct ccw_device *, int);
extern int qdio_get_next_buffers(struct ccw_device *, int, int *, int *);
extern int qdio_shutdown(struct ccw_device *, int);
extern int qdio_free(struct ccw_device *);
extern int qdio_get_ssqd_desc(struct ccw_device *, struct qdio_ssqd_desc *);

#endif 
