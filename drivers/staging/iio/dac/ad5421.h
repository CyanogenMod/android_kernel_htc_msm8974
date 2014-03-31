#ifndef __IIO_DAC_AD5421_H__
#define __IIO_DAC_AD5421_H__



enum ad5421_current_range {
	AD5421_CURRENT_RANGE_4mA_20mA,
	AD5421_CURRENT_RANGE_3mA8_21mA,
	AD5421_CURRENT_RANGE_3mA2_24mA,
};


struct ad5421_platform_data {
	bool external_vref;
	enum ad5421_current_range current_range;
};

#endif
