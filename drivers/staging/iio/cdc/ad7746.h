/*
 * AD7746 capacitive sensor driver supporting AD7745, AD7746 and AD7747
 *
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef IIO_CDC_AD7746_H_
#define IIO_CDC_AD7746_H_


#define AD7466_EXCLVL_0		0 
#define AD7466_EXCLVL_1		1 
#define AD7466_EXCLVL_2		2 
#define AD7466_EXCLVL_3		3 

struct ad7746_platform_data {
	unsigned char exclvl;	
	bool exca_en;		
	bool exca_inv_en;	
	bool excb_en;		
	bool excb_inv_en;	
};

#endif 
