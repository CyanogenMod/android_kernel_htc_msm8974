/*
 * max3107.h - spi uart protocol driver header for Maxim 3107
 *
 * Copyright (C) Aavamobile 2009
 * Based on serial_max3100.h by Christian Pellegrin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _MAX3107_H
#define _MAX3107_H

#define MAX3107_PARITY_ERROR	1
#define MAX3107_FRAME_ERROR	2
#define MAX3107_OVERRUN_ERROR	4
#define MAX3107_ALL_ERRORS	(MAX3107_PARITY_ERROR | \
				 MAX3107_FRAME_ERROR | \
				 MAX3107_OVERRUN_ERROR)

#define MAX3107_GPIO_BASE	88
#define MAX3107_GPIO_COUNT	4


#define MAX3107_RESET_GPIO	87


#define MAX3107_RESET_DELAY	10

#define MAX3107_WAKEUP_DELAY	50


#define MAX3107_DISABLE_FORCED_SLEEP	0
#define MAX3107_ENABLE_FORCED_SLEEP	1
#define MAX3107_DISABLE_AUTOSLEEP	2
#define MAX3107_ENABLE_AUTOSLEEP	3



#define MAX3107_SPI_SPEED	(3125000 * 2)

#define MAX3107_WRITE_BIT	(1 << 15)

#define MAX3107_SPI_RX_DATA_MASK	(0x00ff)

#define MAX3107_SPI_TX_DATA_MASK	(0x00ff)

#define MAX3107_RHR_REG			(0x0000) 
#define MAX3107_THR_REG			(0x0000) 
#define MAX3107_IRQEN_REG		(0x0100) 
#define MAX3107_IRQSTS_REG		(0x0200) 
#define MAX3107_LSR_IRQEN_REG		(0x0300) 
#define MAX3107_LSR_IRQSTS_REG		(0x0400) 
#define MAX3107_SPCHR_IRQEN_REG		(0x0500) 
#define MAX3107_SPCHR_IRQSTS_REG	(0x0600) 
#define MAX3107_STS_IRQEN_REG		(0x0700) 
#define MAX3107_STS_IRQSTS_REG		(0x0800) 
#define MAX3107_MODE1_REG		(0x0900) 
#define MAX3107_MODE2_REG		(0x0a00) 
#define MAX3107_LCR_REG			(0x0b00) 
#define MAX3107_RXTO_REG		(0x0c00) 
#define MAX3107_HDPIXDELAY_REG		(0x0d00) 
#define MAX3107_IRDA_REG		(0x0e00) 
#define MAX3107_FLOWLVL_REG		(0x0f00) 
#define MAX3107_FIFOTRIGLVL_REG		(0x1000) 
#define MAX3107_TXFIFOLVL_REG		(0x1100) 
#define MAX3107_RXFIFOLVL_REG		(0x1200) 
#define MAX3107_FLOWCTRL_REG		(0x1300) 
#define MAX3107_XON1_REG		(0x1400) 
#define MAX3107_XON2_REG		(0x1500) 
#define MAX3107_XOFF1_REG		(0x1600) 
#define MAX3107_XOFF2_REG		(0x1700) 
#define MAX3107_GPIOCFG_REG		(0x1800) 
#define MAX3107_GPIODATA_REG		(0x1900) 
#define MAX3107_PLLCFG_REG		(0x1a00) 
#define MAX3107_BRGCFG_REG		(0x1b00) 
#define MAX3107_BRGDIVLSB_REG		(0x1c00) 
#define MAX3107_BRGDIVMSB_REG		(0x1d00) 
#define MAX3107_CLKSRC_REG		(0x1e00) 
#define MAX3107_REVID_REG		(0x1f00) 

#define MAX3107_IRQ_LSR_BIT	(1 << 0) 
#define MAX3107_IRQ_SPCHR_BIT	(1 << 1) 
#define MAX3107_IRQ_STS_BIT	(1 << 2) 
#define MAX3107_IRQ_RXFIFO_BIT	(1 << 3) 
#define MAX3107_IRQ_TXFIFO_BIT	(1 << 4) 
#define MAX3107_IRQ_TXEMPTY_BIT	(1 << 5) 
#define MAX3107_IRQ_RXEMPTY_BIT	(1 << 6) 
#define MAX3107_IRQ_CTS_BIT	(1 << 7) 

#define MAX3107_LSR_RXTO_BIT	(1 << 0) 
#define MAX3107_LSR_RXOVR_BIT	(1 << 1) 
#define MAX3107_LSR_RXPAR_BIT	(1 << 2) 
#define MAX3107_LSR_FRERR_BIT	(1 << 3) 
#define MAX3107_LSR_RXBRK_BIT	(1 << 4) 
#define MAX3107_LSR_RXNOISE_BIT	(1 << 5) 
#define MAX3107_LSR_UNDEF6_BIT	(1 << 6) 
#define MAX3107_LSR_CTS_BIT	(1 << 7) 

#define MAX3107_SPCHR_XON1_BIT		(1 << 0) 
#define MAX3107_SPCHR_XON2_BIT		(1 << 1) 
#define MAX3107_SPCHR_XOFF1_BIT		(1 << 2) 
#define MAX3107_SPCHR_XOFF2_BIT		(1 << 3) 
#define MAX3107_SPCHR_BREAK_BIT		(1 << 4) 
#define MAX3107_SPCHR_MULTIDROP_BIT	(1 << 5) 
#define MAX3107_SPCHR_UNDEF6_BIT	(1 << 6) 
#define MAX3107_SPCHR_UNDEF7_BIT	(1 << 7) 

#define MAX3107_STS_GPIO0_BIT		(1 << 0) 
#define MAX3107_STS_GPIO1_BIT		(1 << 1) 
#define MAX3107_STS_GPIO2_BIT		(1 << 2) 
#define MAX3107_STS_GPIO3_BIT		(1 << 3) 
#define MAX3107_STS_UNDEF4_BIT		(1 << 4) 
#define MAX3107_STS_CLKREADY_BIT	(1 << 5) 
#define MAX3107_STS_SLEEP_BIT		(1 << 6) 
#define MAX3107_STS_UNDEF7_BIT		(1 << 7) 

#define MAX3107_MODE1_RXDIS_BIT		(1 << 0) 
#define MAX3107_MODE1_TXDIS_BIT		(1 << 1) 
#define MAX3107_MODE1_TXHIZ_BIT		(1 << 2) 
#define MAX3107_MODE1_RTSHIZ_BIT	(1 << 3) 
#define MAX3107_MODE1_TRNSCVCTRL_BIT	(1 << 4) 
#define MAX3107_MODE1_FORCESLEEP_BIT	(1 << 5) 
#define MAX3107_MODE1_AUTOSLEEP_BIT	(1 << 6) 
#define MAX3107_MODE1_IRQSEL_BIT	(1 << 7) 

#define MAX3107_MODE2_RST_BIT		(1 << 0) 
#define MAX3107_MODE2_FIFORST_BIT	(1 << 1) 
#define MAX3107_MODE2_RXTRIGINV_BIT	(1 << 2) 
#define MAX3107_MODE2_RXEMPTINV_BIT	(1 << 3) 
#define MAX3107_MODE2_SPCHR_BIT		(1 << 4) 
#define MAX3107_MODE2_LOOPBACK_BIT	(1 << 5) 
#define MAX3107_MODE2_MULTIDROP_BIT	(1 << 6) 
#define MAX3107_MODE2_ECHOSUPR_BIT	(1 << 7) 

#define MAX3107_LCR_LENGTH0_BIT		(1 << 0) 
#define MAX3107_LCR_LENGTH1_BIT		(1 << 1) 
#define MAX3107_LCR_STOPLEN_BIT		(1 << 2) 
#define MAX3107_LCR_PARITY_BIT		(1 << 3) 
#define MAX3107_LCR_EVENPARITY_BIT	(1 << 4) 
#define MAX3107_LCR_FORCEPARITY_BIT	(1 << 5) 
#define MAX3107_LCR_TXBREAK_BIT		(1 << 6) 
#define MAX3107_LCR_RTS_BIT		(1 << 7) 
#define MAX3107_LCR_WORD_LEN_5		(0x0000)
#define MAX3107_LCR_WORD_LEN_6		(0x0001)
#define MAX3107_LCR_WORD_LEN_7		(0x0002)
#define MAX3107_LCR_WORD_LEN_8		(0x0003)


#define MAX3107_IRDA_IRDAEN_BIT		(1 << 0) 
#define MAX3107_IRDA_SIR_BIT		(1 << 1) 
#define MAX3107_IRDA_SHORTIR_BIT	(1 << 2) 
#define MAX3107_IRDA_MIR_BIT		(1 << 3) 
#define MAX3107_IRDA_RXINV_BIT		(1 << 4) 
#define MAX3107_IRDA_TXINV_BIT		(1 << 5) 
#define MAX3107_IRDA_UNDEF6_BIT		(1 << 6) 
#define MAX3107_IRDA_UNDEF7_BIT		(1 << 7) 

#define MAX3107_FLOWLVL_HALT_MASK	(0x000f) 
#define MAX3107_FLOWLVL_RES_MASK	(0x00f0) 
#define MAX3107_FLOWLVL_HALT(words)	((words/8) & 0x000f)
#define MAX3107_FLOWLVL_RES(words)	(((words/8) & 0x000f) << 4)

#define MAX3107_FIFOTRIGLVL_TX_MASK	(0x000f) 
#define MAX3107_FIFOTRIGLVL_RX_MASK	(0x00f0) 
#define MAX3107_FIFOTRIGLVL_TX(words)	((words/8) & 0x000f)
#define MAX3107_FIFOTRIGLVL_RX(words)	(((words/8) & 0x000f) << 4)

#define MAX3107_FLOWCTRL_AUTORTS_BIT	(1 << 0) 
#define MAX3107_FLOWCTRL_AUTOCTS_BIT	(1 << 1) 
#define MAX3107_FLOWCTRL_GPIADDR_BIT	(1 << 2) 
#define MAX3107_FLOWCTRL_SWFLOWEN_BIT	(1 << 3) 
#define MAX3107_FLOWCTRL_SWFLOW0_BIT	(1 << 4) 
#define MAX3107_FLOWCTRL_SWFLOW1_BIT	(1 << 5) 
#define MAX3107_FLOWCTRL_SWFLOW2_BIT	(1 << 6) 
#define MAX3107_FLOWCTRL_SWFLOW3_BIT	(1 << 7) 

#define MAX3107_GPIOCFG_GP0OUT_BIT	(1 << 0) 
#define MAX3107_GPIOCFG_GP1OUT_BIT	(1 << 1) 
#define MAX3107_GPIOCFG_GP2OUT_BIT	(1 << 2) 
#define MAX3107_GPIOCFG_GP3OUT_BIT	(1 << 3) 
#define MAX3107_GPIOCFG_GP0OD_BIT	(1 << 4) 
#define MAX3107_GPIOCFG_GP1OD_BIT	(1 << 5) 
#define MAX3107_GPIOCFG_GP2OD_BIT	(1 << 6) 
#define MAX3107_GPIOCFG_GP3OD_BIT	(1 << 7) 

#define MAX3107_GPIODATA_GP0OUT_BIT	(1 << 0) 
#define MAX3107_GPIODATA_GP1OUT_BIT	(1 << 1) 
#define MAX3107_GPIODATA_GP2OUT_BIT	(1 << 2) 
#define MAX3107_GPIODATA_GP3OUT_BIT	(1 << 3) 
#define MAX3107_GPIODATA_GP0IN_BIT	(1 << 4) 
#define MAX3107_GPIODATA_GP1IN_BIT	(1 << 5) 
#define MAX3107_GPIODATA_GP2IN_BIT	(1 << 6) 
#define MAX3107_GPIODATA_GP3IN_BIT	(1 << 7) 

#define MAX3107_PLLCFG_PREDIV_MASK	(0x003f) 
#define MAX3107_PLLCFG_PLLFACTOR_MASK	(0x00c0) 

#define MAX3107_BRGCFG_FRACT_MASK	(0x000f) 
#define MAX3107_BRGCFG_2XMODE_BIT	(1 << 4) 
#define MAX3107_BRGCFG_4XMODE_BIT	(1 << 5) 
#define MAX3107_BRGCFG_UNDEF6_BIT	(1 << 6) 
#define MAX3107_BRGCFG_UNDEF7_BIT	(1 << 7) 

#define MAX3107_CLKSRC_INTOSC_BIT	(1 << 0) 
#define MAX3107_CLKSRC_CRYST_BIT	(1 << 1) 
#define MAX3107_CLKSRC_PLL_BIT		(1 << 2) 
#define MAX3107_CLKSRC_PLLBYP_BIT	(1 << 3) 
#define MAX3107_CLKSRC_EXTCLK_BIT	(1 << 4) 
#define MAX3107_CLKSRC_UNDEF5_BIT	(1 << 5) 
#define MAX3107_CLKSRC_UNDEF6_BIT	(1 << 6) 
#define MAX3107_CLKSRC_CLK2RTS_BIT	(1 << 7) 


#define MAX3107_RX_FIFO_SIZE	128
#define MAX3107_TX_FIFO_SIZE	128
#define MAX3107_REVID1		0x00a0
#define MAX3107_REVID2		0x00a1


#define MAX3107_BRG13_B300	(0x0A9400 | 0x05)
#define MAX3107_BRG13_B600	(0x054A00 | 0x03)
#define MAX3107_BRG13_B1200	(0x02A500 | 0x01)
#define MAX3107_BRG13_B2400	(0x015200 | 0x09)
#define MAX3107_BRG13_B4800	(0x00A900 | 0x04)
#define MAX3107_BRG13_B9600	(0x005400 | 0x0A)
#define MAX3107_BRG13_B19200	(0x002A00 | 0x05)
#define MAX3107_BRG13_B38400	(0x001500 | 0x03)
#define MAX3107_BRG13_B57600	(0x000E00 | 0x02)
#define MAX3107_BRG13_B115200	(0x000700 | 0x01)
#define MAX3107_BRG13_B230400	(0x000300 | 0x08)
#define MAX3107_BRG13_B460800	(0x000100 | 0x0c)
#define MAX3107_BRG13_B921600	(0x000100 | 0x1c)

#define MAX3107_BRG26_B300	(0x152800 | 0x0A)
#define MAX3107_BRG26_B600	(0x0A9400 | 0x05)
#define MAX3107_BRG26_B1200	(0x054A00 | 0x03)
#define MAX3107_BRG26_B2400	(0x02A500 | 0x01)
#define MAX3107_BRG26_B4800	(0x015200 | 0x09)
#define MAX3107_BRG26_B9600	(0x00A900 | 0x04)
#define MAX3107_BRG26_B19200	(0x005400 | 0x0A)
#define MAX3107_BRG26_B38400	(0x002A00 | 0x05)
#define MAX3107_BRG26_B57600	(0x001C00 | 0x03)
#define MAX3107_BRG26_B115200	(0x000E00 | 0x02)
#define MAX3107_BRG26_B230400	(0x000700 | 0x01)
#define MAX3107_BRG26_B460800	(0x000300 | 0x08)
#define MAX3107_BRG26_B921600	(0x000100 | 0x0C)

#define MAX3107_BRG13_IB300	(0x008000 | 0x00)
#define MAX3107_BRG13_IB600	(0x004000 | 0x00)
#define MAX3107_BRG13_IB1200	(0x002000 | 0x00)
#define MAX3107_BRG13_IB2400	(0x001000 | 0x00)
#define MAX3107_BRG13_IB4800	(0x000800 | 0x00)
#define MAX3107_BRG13_IB9600	(0x000400 | 0x00)
#define MAX3107_BRG13_IB19200	(0x000200 | 0x00)
#define MAX3107_BRG13_IB38400	(0x000100 | 0x00)
#define MAX3107_BRG13_IB57600	(0x000000 | 0x0B)
#define MAX3107_BRG13_IB115200	(0x000000 | 0x05)
#define MAX3107_BRG13_IB230400	(0x000000 | 0x03)
#define MAX3107_BRG13_IB460800	(0x000000 | 0x00)
#define MAX3107_BRG13_IB921600	(0x000000 | 0x00)


struct baud_table {
	int baud;
	u32 new_brg;
};

struct max3107_port {
	
	struct uart_port port;

	
	struct spi_device *spi;

#if defined(CONFIG_GPIOLIB)
	
	struct gpio_chip chip;
#endif

	
	struct workqueue_struct *workqueue;
	struct work_struct work;

	
	spinlock_t data_lock;

	
	int ext_clk;		
	int loopback;		
	int baud;			

	
	int suspended;		
	int tx_fifo_empty;	
	int rx_enabled;		
	int tx_enabled;		

	u16 irqen_reg;		
	
	u16 mode1_reg;		
	int mode1_commit;	
	u16 lcr_reg;		
	int lcr_commit;		
	u32 brg_cfg;		
	int brg_commit;		
	struct baud_table *baud_tbl;
	int handle_irq;		

	
	u16 *rxbuf;
	u8  *rxstr;
	
	u16 *txbuf;

	struct max3107_plat *pdata;	
};

struct max3107_plat {
	
	int loopback;
	
	int ext_clk;
	
	void (*init)(struct max3107_port *s);
	
	int (*configure)(struct max3107_port *s);
	
	void (*hw_suspend) (struct max3107_port *s, int suspend);
	
	int polled_mode;
	
	int poll_time;
};

extern int max3107_rw(struct max3107_port *s, u8 *tx, u8 *rx, int len);
extern void max3107_hw_susp(struct max3107_port *s, int suspend);
extern int max3107_probe(struct spi_device *spi, struct max3107_plat *pdata);
extern int max3107_remove(struct spi_device *spi);
extern int max3107_suspend(struct spi_device *spi, pm_message_t state);
extern int max3107_resume(struct spi_device *spi);

#endif 
