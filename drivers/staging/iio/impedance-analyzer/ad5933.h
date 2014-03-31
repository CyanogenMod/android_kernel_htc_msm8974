/*
 * AD5933 AD5934 Impedance Converter, Network Analyzer
 *
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef IIO_ADC_AD5933_H_
#define IIO_ADC_AD5933_H_



struct ad5933_platform_data {
	unsigned long			ext_clk_Hz;
	unsigned short			vref_mv;
};

#endif 
