/*
 * AD7780/AD7781 SPI ADC driver
 *
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */
#ifndef IIO_ADC_AD7780_H_
#define IIO_ADC_AD7780_H_



struct ad7780_platform_data {
	u16				vref_mv;
	int				gpio_pdrst;
};

#endif 
