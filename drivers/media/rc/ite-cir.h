/*
 * Driver for ITE Tech Inc. IT8712F/IT8512F CIR
 *
 * Copyright (C) 2010 Juan Jesús García de Soria <skandalfo@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#define ITE_DRIVER_NAME "ite-cir"

#define ite_pr(level, text, ...) \
	printk(level KBUILD_MODNAME ": " text, ## __VA_ARGS__)
#define ite_dbg(text, ...) do { \
	if (debug) \
		printk(KERN_DEBUG \
			KBUILD_MODNAME ": " text "\n" , ## __VA_ARGS__); \
} while (0)

#define ite_dbg_verbose(text, ...) do {\
	if (debug > 1) \
		printk(KERN_DEBUG \
			KBUILD_MODNAME ": " text "\n" , ## __VA_ARGS__); \
} while (0)

#define ITE_TX_FIFO_LEN 32
#define ITE_RX_FIFO_LEN 32

#define ITE_IRQ_TX_FIFO        1
#define ITE_IRQ_RX_FIFO        2
#define ITE_IRQ_RX_FIFO_OVERRUN    4

struct ite_dev;

struct ite_dev_params {
	
	const char *model;

	
	int io_region_size;

	
	int io_rsrc_no;

	
	bool hw_tx_capable;

	
	u32 sample_period;

	
	unsigned int rx_low_carrier_freq;

	
	unsigned int rx_high_carrier_freq;

	
	unsigned int tx_carrier_freq;

	
	int tx_duty_cycle;

	
	int (*get_irq_causes) (struct ite_dev *dev);

	
	void (*enable_rx) (struct ite_dev *dev);

	void (*idle_rx) (struct ite_dev *dev);

	
	void (*disable_rx) (struct ite_dev *dev);

	
	int (*get_rx_bytes) (struct ite_dev *dev, u8 *buf, int buf_size);

	
	void (*enable_tx_interrupt) (struct ite_dev *dev);

	
	void (*disable_tx_interrupt) (struct ite_dev *dev);

	
	int (*get_tx_used_slots) (struct ite_dev *dev);

	
	void (*put_tx_byte) (struct ite_dev *dev, u8 value);

	
	void (*disable) (struct ite_dev *dev);

	
	void (*init_hardware) (struct ite_dev *dev);

	
	void (*set_carrier_params) (struct ite_dev *dev, bool high_freq,
				    bool use_demodulator, u8 carrier_freq_bits,
				    u8 allowance_bits, u8 pulse_width_bits);
};

struct ite_dev {
	struct pnp_dev *pdev;
	struct rc_dev *rdev;
	struct ir_raw_event rawir;

	
	spinlock_t lock;
	bool in_use, transmitting;

	
	int tx_fifo_allowance;
	wait_queue_head_t tx_queue, tx_ended;

	
	unsigned long cir_addr;
	int cir_irq;

	
	struct ite_dev_params params;
};


#define ITE_BAUDRATE_DIVISOR		1

#define ITE_LCF_MIN_CARRIER_FREQ	27000
#define ITE_LCF_MAX_CARRIER_FREQ	58000

#define ITE_HCF_MIN_CARRIER_FREQ	400000
#define ITE_HCF_MAX_CARRIER_FREQ	500000

#define ITE_DEFAULT_CARRIER_FREQ	38000

#define ITE_IDLE_TIMEOUT		200000000UL

#define ITE_MIN_IDLE_TIMEOUT		100000000UL
#define ITE_MAX_IDLE_TIMEOUT		1000000000UL

#define ITE_BITS_TO_NS(bits, sample_period) \
((u32) ((bits) * ITE_BAUDRATE_DIVISOR * sample_period))

#define ITE_RXDCR_PER_10000_STEP 625

#define ITE_CFQ_400		0x03
#define ITE_CFQ_450		0x08
#define ITE_CFQ_480		0x0b
#define ITE_CFQ_500		0x0d

#define ITE_TXMPW_A		0x02
#define ITE_TXMPW_B		0x03
#define ITE_TXMPW_C		0x04
#define ITE_TXMPW_D		0x05
#define ITE_TXMPW_E		0x06

#define ITE_RXDCR_DEFAULT	0x01	
#define ITE_RXDCR_MAX		0x07	

#define ITE_TX_PULSE		0x00
#define ITE_TX_SPACE		0x80
#define ITE_TX_MAX_RLE		0x80
#define ITE_TX_RLE_MASK		0x7f


#define IT87_DR		0x00	
#define IT87_IER	0x01	
#define IT87_RCR	0x02	
#define IT87_TCR1	0x03	
#define IT87_TCR2	0x04	
#define IT87_TSR	0x05	
#define IT87_RSR	0x06	
#define IT87_BDLR	0x05	
#define IT87_BDHR	0x06	
#define IT87_IIR	0x07	

#define IT87_IOREG_LENGTH 0x08	

#define IT87_TLDLIE	0x01	
#define IT87_RDAIE	0x02	
#define IT87_RFOIE	0x04	
#define IT87_IEC	0x08	
#define IT87_BR		0x10	
#define IT87_RESET	0x20	

#define IT87_RXDCR	0x07	
#define IT87_RXACT	0x08	
#define IT87_RXEND	0x10	
#define IT87_RXEN	0x20	
#define IT87_HCFS	0x40	
#define IT87_RDWOS	0x80	

#define IT87_TXMPM	0x03	
#define IT87_TXMPM_DEFAULT 0x00	
#define IT87_TXENDF	0x04	
#define IT87_TXRLE	0x08	
#define IT87_FIFOTL	0x30	
#define IT87_FIFOTL_DEFAULT 0x20	
#define IT87_ILE	0x40	
#define IT87_FIFOCLR	0x80	

#define IT87_TXMPW	0x07	
#define IT87_TXMPW_DEFAULT 0x04	
#define IT87_CFQ	0xf8	
#define IT87_CFQ_SHIFT	3	

#define IT87_TXFBC	0x3f	

#define IT87_RXFBC	0x3f	
#define IT87_RXFTO	0x80	

#define IT87_IP		0x01	
#define IT87_II		0x06	
#define IT87_II_NOINT	0x00	
#define IT87_II_TXLDL	0x02	
#define IT87_II_RXDS	0x04	
#define IT87_II_RXFO	0x06	


#define IT85_C0DR	0x00	
#define IT85_C0MSTCR	0x01	
#define IT85_C0IER	0x02	
#define IT85_C0IIR	0x03	
#define IT85_C0CFR	0x04	
#define IT85_C0RCR	0x05	
#define IT85_C0TCR	0x06	
#define IT85_C0SCK	0x07	
#define IT85_C0BDLR	0x08	
#define IT85_C0BDHR	0x09	
#define IT85_C0TFSR	0x0a	
#define IT85_C0RFSR	0x0b	
#define IT85_C0WCL	0x0d	
#define IT85_C0WCR	0x0e	
#define IT85_C0WPS	0x0f	

#define IT85_IOREG_LENGTH 0x10	

#define IT85_RESET	0x01	
#define IT85_FIFOCLR	0x02	
#define IT85_FIFOTL	0x0c	
#define IT85_FIFOTL_DEFAULT 0x08	
#define IT85_ILE	0x10	
#define IT85_ILSEL	0x20	

#define IT85_TLDLIE	0x01	
#define IT85_RDAIE	0x02	
#define IT85_RFOIE	0x04	
#define IT85_IEC	0x80	

#define IT85_TLDLI	0x01	
#define IT85_RDAI	0x02	
#define IT85_RFOI	0x04	
#define IT85_NIP	0x80	

#define IT85_CFQ	0x1f	
#define IT85_HCFS	0x20	

#define IT85_RXDCR	0x07	
#define IT85_RXACT	0x08	
#define IT85_RXEND	0x10	
#define IT85_RDWOS	0x20	
#define IT85_RXEN	0x80	

#define IT85_TXMPW	0x07	
#define IT85_TXMPW_DEFAULT 0x04	
#define IT85_TXMPM	0x18	
#define IT85_TXMPM_DEFAULT 0x00	
#define IT85_TXENDF	0x20	
#define IT85_TXRLE	0x40	

#define IT85_SCKS	0x01	
#define IT85_TXDCKG	0x02	
#define IT85_DLL1P8E	0x04	
#define IT85_DLLTE	0x08	
#define IT85_BRCM	0x70	
#define IT85_DLLOCK	0x80	

#define IT85_TXFBC	0x3f	

#define IT85_RXFBC	0x3f	
#define IT85_RXFTO	0x80	

#define IT85_WCL	0x3f	

#define IT85_CIRPOSIE	0x01	
#define IT85_CIRPOIS	0x02	
#define IT85_CIRPOII	0x04	
#define IT85_RCRST	0x10	
#define IT85_WCRST	0x20	



#define IT8708_BANKSEL	0x07	
#define IT8708_HRAE	0x80	

#define IT8708_C0DR	0x00	
#define IT8708_C0MSTCR	0x01	
#define IT8708_C0IER	0x02	
#define IT8708_C0IIR	0x03	
#define IT8708_C0RFSR	0x04	
#define IT8708_C0RCR	0x05	
#define IT8708_C0TFSR	0x06	
#define IT8708_C0TCR	0x07	

#define IT8708_C0BDLR	0x01	
#define IT8708_C0BDHR	0x02	
#define IT8708_C0CFR	0x04	

#define IT8708_C0SCK	0x03	
#define IT8708_C0WCL	0x05	
#define IT8708_C0WCR	0x06	
#define IT8708_C0WPS	0x07	

#define IT8708_IOREG_LENGTH 0x08	

#define IT8708_CSCRR	0x00
#define IT8708_CGPINTR	0x01

#define IT8708_CSCRR_SCRB 0x3f
#define IT8708_CSCRR_PM	0x80

#define IT8708_CGPINT	0x01


#define IT8709_RAM_IDX	0x00	
#define IT8709_RAM_VAL	0x01	

#define IT8709_IOREG_LENGTH 0x02	

#define IT8709_MODE	0x1a	
#define IT8709_REG_IDX	0x1b	
#define IT8709_REG_VAL	0x1c	/* value read/to be written */
#define IT8709_IIR	0x1e	
#define IT8709_RFSR	0x1f	
#define IT8709_FIFO	0x20	

#define IT8709_IDLE	0x00
#define IT8709_WRITE	0x01
#define IT8709_READ	0x02
