/*
 * AD9832 SPI DDS driver
 *
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */
#ifndef IIO_DDS_AD9832_H_
#define IIO_DDS_AD9832_H_


#define AD9832_FREQ0LL		0x0
#define AD9832_FREQ0HL		0x1
#define AD9832_FREQ0LM		0x2
#define AD9832_FREQ0HM		0x3
#define AD9832_FREQ1LL		0x4
#define AD9832_FREQ1HL		0x5
#define AD9832_FREQ1LM		0x6
#define AD9832_FREQ1HM		0x7
#define AD9832_PHASE0L		0x8
#define AD9832_PHASE0H		0x9
#define AD9832_PHASE1L		0xA
#define AD9832_PHASE1H		0xB
#define AD9832_PHASE2L		0xC
#define AD9832_PHASE2H		0xD
#define AD9832_PHASE3L		0xE
#define AD9832_PHASE3H		0xF

#define AD9832_PHASE_SYM	0x10
#define AD9832_FREQ_SYM		0x11
#define AD9832_PINCTRL_EN	0x12
#define AD9832_OUTPUT_EN	0x13


#define AD9832_CMD_PHA8BITSW	0x1
#define AD9832_CMD_PHA16BITSW	0x0
#define AD9832_CMD_FRE8BITSW	0x3
#define AD9832_CMD_FRE16BITSW	0x2
#define AD9832_CMD_FPSELECT	0x6
#define AD9832_CMD_SYNCSELSRC	0x8
#define AD9832_CMD_SLEEPRESCLR	0xC

#define AD9832_FREQ		(1 << 11)
#define AD9832_PHASE(x)		(((x) & 3) << 9)
#define AD9832_SYNC		(1 << 13)
#define AD9832_SELSRC		(1 << 12)
#define AD9832_SLEEP		(1 << 13)
#define AD9832_RESET		(1 << 12)
#define AD9832_CLR		(1 << 11)
#define CMD_SHIFT		12
#define ADD_SHIFT		8
#define AD9832_FREQ_BITS	32
#define AD9832_PHASE_BITS	12
#define RES_MASK(bits)		((1 << (bits)) - 1)


struct ad9832_state {
	struct spi_device		*spi;
	struct regulator		*reg;
	unsigned long			mclk;
	unsigned short			ctrl_fp;
	unsigned short			ctrl_ss;
	unsigned short			ctrl_src;
	struct spi_transfer		xfer;
	struct spi_message		msg;
	struct spi_transfer		freq_xfer[4];
	struct spi_message		freq_msg;
	struct spi_transfer		phase_xfer[2];
	struct spi_message		phase_msg;
	union {
		unsigned short		freq_data[4]____cacheline_aligned;
		unsigned short		phase_data[2];
		unsigned short		data;
	};
};



struct ad9832_platform_data {
	unsigned long		mclk;
	unsigned long		freq0;
	unsigned long		freq1;
	unsigned short		phase0;
	unsigned short		phase1;
	unsigned short		phase2;
	unsigned short		phase3;
};

#endif 
