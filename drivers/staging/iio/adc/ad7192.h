/*
 * AD7190 AD7192 AD7195 SPI ADC driver
 *
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */
#ifndef IIO_ADC_AD7192_H_
#define IIO_ADC_AD7192_H_



struct ad7192_platform_data {
	u16		vref_mv;
	u8		clock_source_sel;
	u32		ext_clk_Hz;
	bool		refin2_en;
	bool		rej60_en;
	bool		sinc3_en;
	bool		chop_en;
	bool		buf_en;
	bool		unipolar_en;
	bool		burnout_curr_en;
};

#endif 
