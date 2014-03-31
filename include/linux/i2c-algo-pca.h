#ifndef _LINUX_I2C_ALGO_PCA_H
#define _LINUX_I2C_ALGO_PCA_H

#define I2C_PCA_CHIP_9564	0x00
#define I2C_PCA_CHIP_9665	0x01

#define I2C_PCA_OSC_PER		3 

#define I2C_PCA_CON_330kHz	0x00
#define I2C_PCA_CON_288kHz	0x01
#define I2C_PCA_CON_217kHz	0x02
#define I2C_PCA_CON_146kHz	0x03
#define I2C_PCA_CON_88kHz	0x04
#define I2C_PCA_CON_59kHz	0x05
#define I2C_PCA_CON_44kHz	0x06
#define I2C_PCA_CON_36kHz	0x07

#define I2C_PCA_STA		0x00 
#define I2C_PCA_TO		0x00 
#define I2C_PCA_DAT		0x01 
#define I2C_PCA_ADR		0x02 
#define I2C_PCA_CON		0x03 

#define I2C_PCA_INDPTR          0x00 
#define I2C_PCA_IND             0x02 

#define I2C_PCA_ICOUNT          0x00 
#define I2C_PCA_IADR            0x01 
#define I2C_PCA_ISCLL           0x02 
#define I2C_PCA_ISCLH           0x03 
#define I2C_PCA_ITO             0x04 
#define I2C_PCA_IPRESET         0x05 
#define I2C_PCA_IMODE           0x06 

#define I2C_PCA_MODE_STD        0x00 
#define I2C_PCA_MODE_FAST       0x01 
#define I2C_PCA_MODE_FASTP      0x02 
#define I2C_PCA_MODE_TURBO      0x03 


#define I2C_PCA_CON_AA		0x80 
#define I2C_PCA_CON_ENSIO	0x40 
#define I2C_PCA_CON_STA		0x20 
#define I2C_PCA_CON_STO		0x10 
#define I2C_PCA_CON_SI		0x08 
#define I2C_PCA_CON_CR		0x07 

struct i2c_algo_pca_data {
	void 				*data;	
	void (*write_byte)		(void *data, int reg, int val);
	int  (*read_byte)		(void *data, int reg);
	int  (*wait_for_completion)	(void *data);
	void (*reset_chip)		(void *data);
	unsigned int			i2c_clock;
};

int i2c_pca_add_bus(struct i2c_adapter *);
int i2c_pca_add_numbered_bus(struct i2c_adapter *);

#endif 
