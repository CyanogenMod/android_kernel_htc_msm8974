/*
 * Driver for Nuvoton Technology Corporation w83667hg/w83677hg-i CIR
 *
 * Copyright (C) 2010 Jarod Wilson <jarod@redhat.com>
 * Copyright (C) 2009 Nuvoton PS Team
 *
 * Special thanks to Nuvoton for providing hardware, spec sheets and
 * sample code upon which portions of this driver are based. Indirect
 * thanks also to Maxim Levitsky, whose ene_ir driver this driver is
 * modeled after.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <linux/spinlock.h>
#include <linux/ioctl.h>

#define NVT_DRIVER_NAME "nuvoton-cir"

static int debug;


#define nvt_pr(level, text, ...) \
	printk(level KBUILD_MODNAME ": " text, ## __VA_ARGS__)

#define nvt_dbg(text, ...) \
	if (debug) \
		printk(KERN_DEBUG \
			KBUILD_MODNAME ": " text "\n" , ## __VA_ARGS__)

#define nvt_dbg_verbose(text, ...) \
	if (debug > 1) \
		printk(KERN_DEBUG \
			KBUILD_MODNAME ": " text "\n" , ## __VA_ARGS__)

#define nvt_dbg_wake(text, ...) \
	if (debug > 2) \
		printk(KERN_DEBUG \
			KBUILD_MODNAME ": " text "\n" , ## __VA_ARGS__)


#define TX_BUF_LEN 256
#define RX_BUF_LEN 32

struct nvt_dev {
	struct pnp_dev *pdev;
	struct rc_dev *rdev;

	spinlock_t nvt_lock;

	
	u8 buf[RX_BUF_LEN];
	unsigned int pkts;

	struct {
		spinlock_t lock;
		u8 buf[TX_BUF_LEN];
		unsigned int buf_count;
		unsigned int cur_buf_num;
		wait_queue_head_t queue;
		u8 tx_state;
	} tx;

	
	u8 cr_efir;
	u8 cr_efdr;

	
	unsigned long cir_addr;
	unsigned long cir_wake_addr;
	int cir_irq;
	int cir_wake_irq;

	
	u8 chip_major;
	u8 chip_minor;

	
	bool hw_learning_capable;
	bool hw_tx_capable;

	
	bool learning_enabled;
	bool carrier_detect_enabled;

	
	u8 wake_state;
	
	u8 study_state;
	
	u32 carrier;
};

#define ST_STUDY_NONE      0x0
#define ST_STUDY_START     0x1
#define ST_STUDY_CARRIER   0x2
#define ST_STUDY_ALL_RECV  0x4

#define ST_WAKE_NONE	0x0
#define ST_WAKE_START	0x1
#define ST_WAKE_FINISH	0x2

#define ST_RX_WAIT_7F		0x1
#define ST_RX_WAIT_HEAD		0x2
#define ST_RX_WAIT_SILENT_END	0x4

#define ST_TX_NONE	0x0
#define ST_TX_REQUEST	0x2
#define ST_TX_REPLY	0x4

#define BUF_PULSE_BIT	0x80
#define BUF_LEN_MASK	0x7f
#define BUF_REPEAT_BYTE	0x70
#define BUF_REPEAT_MASK	0xf0


#define CIR_IOREG_LENGTH	0x0f

#define CIR_RX_LIMIT_COUNT	0x7d0

#define CIR_IRCON	0x00
#define CIR_IRSTS	0x01
#define CIR_IREN	0x02
#define CIR_RXFCONT	0x03
#define CIR_CP		0x04
#define CIR_CC		0x05
#define CIR_SLCH	0x06
#define CIR_SLCL	0x07
#define CIR_FIFOCON	0x08
#define CIR_IRFIFOSTS	0x09
#define CIR_SRXFIFO	0x0a
#define CIR_TXFCONT	0x0b
#define CIR_STXFIFO	0x0c
#define CIR_FCCH	0x0d
#define CIR_FCCL	0x0e
#define CIR_IRFSM	0x0f

#define CIR_IRCON_RECV	 0x80
#define CIR_IRCON_WIREN	 0x40
#define CIR_IRCON_TXEN	 0x20
#define CIR_IRCON_RXEN	 0x10
#define CIR_IRCON_WRXINV 0x08
#define CIR_IRCON_RXINV	 0x04

#define CIR_IRCON_SAMPLE_PERIOD_SEL_1	0x00
#define CIR_IRCON_SAMPLE_PERIOD_SEL_25	0x01
#define CIR_IRCON_SAMPLE_PERIOD_SEL_50	0x02
#define CIR_IRCON_SAMPLE_PERIOD_SEL_100	0x03

#define CIR_IRCON_SAMPLE_PERIOD_SEL	CIR_IRCON_SAMPLE_PERIOD_SEL_50

#define CIR_IRSTS_RDR	0x80
#define CIR_IRSTS_RTR	0x40
#define CIR_IRSTS_PE	0x20
#define CIR_IRSTS_RFO	0x10
#define CIR_IRSTS_TE	0x08
#define CIR_IRSTS_TTR	0x04
#define CIR_IRSTS_TFU	0x02
#define CIR_IRSTS_GH	0x01

#define CIR_IREN_RDR	0x80
#define CIR_IREN_RTR	0x40
#define CIR_IREN_PE	0x20
#define CIR_IREN_RFO	0x10
#define CIR_IREN_TE	0x08
#define CIR_IREN_TTR	0x04
#define CIR_IREN_TFU	0x02
#define CIR_IREN_GH	0x01

#define CIR_FIFOCON_TXFIFOCLR		0x80

#define CIR_FIFOCON_TX_TRIGGER_LEV_31	0x00
#define CIR_FIFOCON_TX_TRIGGER_LEV_24	0x10
#define CIR_FIFOCON_TX_TRIGGER_LEV_16	0x20
#define CIR_FIFOCON_TX_TRIGGER_LEV_8	0x30

#define CIR_FIFOCON_TX_TRIGGER_LEV	CIR_FIFOCON_TX_TRIGGER_LEV_16

#define CIR_FIFOCON_RXFIFOCLR		0x08

#define CIR_FIFOCON_RX_TRIGGER_LEV_1	0x00
#define CIR_FIFOCON_RX_TRIGGER_LEV_8	0x01
#define CIR_FIFOCON_RX_TRIGGER_LEV_16	0x02
#define CIR_FIFOCON_RX_TRIGGER_LEV_24	0x03

#define CIR_FIFOCON_RX_TRIGGER_LEV	CIR_FIFOCON_RX_TRIGGER_LEV_24

#define CIR_IRFIFOSTS_IR_PENDING	0x80
#define CIR_IRFIFOSTS_RX_GS		0x40
#define CIR_IRFIFOSTS_RX_FTA		0x20
#define CIR_IRFIFOSTS_RX_EMPTY		0x10
#define CIR_IRFIFOSTS_RX_FULL		0x08
#define CIR_IRFIFOSTS_TX_FTA		0x04
#define CIR_IRFIFOSTS_TX_EMPTY		0x02
#define CIR_IRFIFOSTS_TX_FULL		0x01


#define CIR_WAKE_IRCON			0x00
#define CIR_WAKE_IRSTS			0x01
#define CIR_WAKE_IREN			0x02
#define CIR_WAKE_FIFO_CMP_DEEP		0x03
#define CIR_WAKE_FIFO_CMP_TOL		0x04
#define CIR_WAKE_FIFO_COUNT		0x05
#define CIR_WAKE_SLCH			0x06
#define CIR_WAKE_SLCL			0x07
#define CIR_WAKE_FIFOCON		0x08
#define CIR_WAKE_SRXFSTS		0x09
#define CIR_WAKE_SAMPLE_RX_FIFO		0x0a
#define CIR_WAKE_WR_FIFO_DATA		0x0b
#define CIR_WAKE_RD_FIFO_ONLY		0x0c
#define CIR_WAKE_RD_FIFO_ONLY_IDX	0x0d
#define CIR_WAKE_FIFO_IGNORE		0x0e
#define CIR_WAKE_IRFSM			0x0f

#define CIR_WAKE_IRCON_DEC_RST		0x80
#define CIR_WAKE_IRCON_MODE1		0x40
#define CIR_WAKE_IRCON_MODE0		0x20
#define CIR_WAKE_IRCON_RXEN		0x10
#define CIR_WAKE_IRCON_R		0x08
#define CIR_WAKE_IRCON_RXINV		0x04

#define CIR_WAKE_IRCON_SAMPLE_PERIOD_SEL	CIR_IRCON_SAMPLE_PERIOD_SEL_50

#define CIR_WAKE_IRSTS_RDR		0x80
#define CIR_WAKE_IRSTS_RTR		0x40
#define CIR_WAKE_IRSTS_PE		0x20
#define CIR_WAKE_IRSTS_RFO		0x10
#define CIR_WAKE_IRSTS_GH		0x08
#define CIR_WAKE_IRSTS_IR_PENDING	0x01

#define CIR_WAKE_IREN_RDR		0x80
#define CIR_WAKE_IREN_RTR		0x40
#define CIR_WAKE_IREN_PE		0x20
#define CIR_WAKE_IREN_RFO		0x10
#define CIR_WAKE_IREN_TE		0x08
#define CIR_WAKE_IREN_TTR		0x04
#define CIR_WAKE_IREN_TFU		0x02
#define CIR_WAKE_IREN_GH		0x01

#define CIR_WAKE_FIFOCON_RXFIFOCLR	0x08

#define CIR_WAKE_FIFOCON_RX_TRIGGER_LEV_67	0x00
#define CIR_WAKE_FIFOCON_RX_TRIGGER_LEV_66	0x01
#define CIR_WAKE_FIFOCON_RX_TRIGGER_LEV_65	0x02
#define CIR_WAKE_FIFOCON_RX_TRIGGER_LEV_64	0x03

#define CIR_WAKE_FIFOCON_RX_TRIGGER_LEV	CIR_WAKE_FIFOCON_RX_TRIGGER_LEV_67

#define CIR_WAKE_IRFIFOSTS_RX_GS	0x80
#define CIR_WAKE_IRFIFOSTS_RX_FTA	0x40
#define CIR_WAKE_IRFIFOSTS_RX_EMPTY	0x20
#define CIR_WAKE_IRFIFOSTS_RX_FULL	0x10

#define CIR_WAKE_FIFO_CMP_BYTES		65
#define CIR_WAKE_CMP_TOLERANCE		5

#define CR_EFIR			0x2e
#define CR_EFDR			0x2f

#define CR_EFIR2		0x4e
#define CR_EFDR2		0x4f

#define EFER_EFM_ENABLE		0x87
#define EFER_EFM_DISABLE	0xaa

#define CHIP_ID_HIGH_667	0xa5
#define CHIP_ID_HIGH_677B	0xb4
#define CHIP_ID_HIGH_677C	0xc3
#define CHIP_ID_LOW_667		0x13
#define CHIP_ID_LOW_677B2	0x72
#define CHIP_ID_LOW_677B3	0x73
#define CHIP_ID_LOW_677C	0x33

#define CR_SOFTWARE_RESET	0x02
#define CR_LOGICAL_DEV_SEL	0x07
#define CR_CHIP_ID_HI		0x20
#define CR_CHIP_ID_LO		0x21
#define CR_DEV_POWER_DOWN	0x22 
#define CR_OUTPUT_PIN_SEL	0x27
#define CR_MULTIFUNC_PIN_SEL	0x2c
#define CR_LOGICAL_DEV_EN	0x30 
#define CR_CIR_BASE_ADDR_HI	0x60
#define CR_CIR_BASE_ADDR_LO	0x61
#define CR_CIR_IRQ_RSRC		0x70
#define CR_ACPI_CIR_WAKE	0xe0
#define CR_ACPI_IRQ_EVENTS	0xf6
#define CR_ACPI_IRQ_EVENTS2	0xf7

#define LOGICAL_DEV_LPT		0x01
#define LOGICAL_DEV_CIR		0x06
#define LOGICAL_DEV_ACPI	0x0a
#define LOGICAL_DEV_CIR_WAKE	0x0e

#define LOGICAL_DEV_DISABLE	0x00
#define LOGICAL_DEV_ENABLE	0x01

#define CIR_WAKE_ENABLE_BIT	0x08
#define CIR_INTR_MOUSE_IRQ_BIT	0x80
#define PME_INTR_CIR_PASS_BIT	0x08

#define OUTPUT_PIN_SEL_MASK	0xbc
#define OUTPUT_ENABLE_CIR	0x01 
#define OUTPUT_ENABLE_CIRWB	0x40 

#define MULTIFUNC_PIN_SEL_MASK	0x1f
#define MULTIFUNC_ENABLE_CIR	0x80 
#define MULTIFUNC_ENABLE_CIRWB	0x20 


#define CONTROLLER_BUF_LEN_MIN 830

#define KEYBOARD_BUF_LEN_MAX 650
#define KEYBOARD_BUF_LEN_MIN 610

#define MOUSE_BUF_LEN_MIN 565

#define CIR_SAMPLE_PERIOD 50
#define CIR_SAMPLE_LOW_INACCURACY 0.85

#define MAX_SILENCE_TIME 60000

#if CIR_IRCON_SAMPLE_PERIOD_SEL == CIR_IRCON_SAMPLE_PERIOD_SEL_100
#define SAMPLE_PERIOD 100

#elif CIR_IRCON_SAMPLE_PERIOD_SEL == CIR_IRCON_SAMPLE_PERIOD_SEL_50
#define SAMPLE_PERIOD 50

#elif CIR_IRCON_SAMPLE_PERIOD_SEL == CIR_IRCON_SAMPLE_PERIOD_SEL_25
#define SAMPLE_PERIOD 25

#else
#define SAMPLE_PERIOD 1
#endif

#define MAX_CARRIER 60000
#define MIN_CARRIER 30000
