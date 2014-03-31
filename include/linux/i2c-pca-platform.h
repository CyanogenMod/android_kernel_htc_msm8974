#ifndef I2C_PCA9564_PLATFORM_H
#define I2C_PCA9564_PLATFORM_H

struct i2c_pca9564_pf_platform_data {
	int gpio;		
	int i2c_clock_speed;	
	int timeout;		
};

#endif 
