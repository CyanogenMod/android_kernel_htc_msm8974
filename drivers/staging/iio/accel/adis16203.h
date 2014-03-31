#ifndef SPI_ADIS16203_H_
#define SPI_ADIS16203_H_

#define ADIS16203_STARTUP_DELAY	220 

#define ADIS16203_READ_REG(a)    a
#define ADIS16203_WRITE_REG(a) ((a) | 0x80)

#define ADIS16203_FLASH_CNT      0x00 
#define ADIS16203_SUPPLY_OUT     0x02 
#define ADIS16203_AUX_ADC        0x08 
#define ADIS16203_TEMP_OUT       0x0A 
#define ADIS16203_XINCL_OUT      0x0C 
#define ADIS16203_YINCL_OUT      0x0E 
#define ADIS16203_INCL_NULL      0x18 
#define ADIS16203_ALM_MAG1       0x20 
#define ADIS16203_ALM_MAG2       0x22 
#define ADIS16203_ALM_SMPL1      0x24 
#define ADIS16203_ALM_SMPL2      0x26 
#define ADIS16203_ALM_CTRL       0x28 
#define ADIS16203_AUX_DAC        0x30 
#define ADIS16203_GPIO_CTRL      0x32 
#define ADIS16203_MSC_CTRL       0x34 
#define ADIS16203_SMPL_PRD       0x36 
#define ADIS16203_AVG_CNT        0x38 
#define ADIS16203_SLP_CNT        0x3A 
#define ADIS16203_DIAG_STAT      0x3C 
#define ADIS16203_GLOB_CMD       0x3E 

#define ADIS16203_OUTPUTS        5

#define ADIS16203_MSC_CTRL_PWRUP_SELF_TEST	(1 << 10) 
#define ADIS16203_MSC_CTRL_REVERSE_ROT_EN	(1 << 9)  
#define ADIS16203_MSC_CTRL_SELF_TEST_EN	        (1 << 8)  
#define ADIS16203_MSC_CTRL_DATA_RDY_EN	        (1 << 2)  
#define ADIS16203_MSC_CTRL_ACTIVE_HIGH	        (1 << 1)  
#define ADIS16203_MSC_CTRL_DATA_RDY_DIO1	(1 << 0)  

#define ADIS16203_DIAG_STAT_ALARM2        (1<<9) 
#define ADIS16203_DIAG_STAT_ALARM1        (1<<8) 
#define ADIS16203_DIAG_STAT_SELFTEST_FAIL (1<<5) 
#define ADIS16203_DIAG_STAT_SPI_FAIL	  (1<<3) 
#define ADIS16203_DIAG_STAT_FLASH_UPT	  (1<<2) 
#define ADIS16203_DIAG_STAT_POWER_HIGH	  (1<<1) 
#define ADIS16203_DIAG_STAT_POWER_LOW	  (1<<0) 

#define ADIS16203_GLOB_CMD_SW_RESET	(1<<7)
#define ADIS16203_GLOB_CMD_CLEAR_STAT	(1<<4)
#define ADIS16203_GLOB_CMD_FACTORY_CAL	(1<<1)

#define ADIS16203_MAX_TX 12
#define ADIS16203_MAX_RX 10

#define ADIS16203_ERROR_ACTIVE          (1<<14)

struct adis16203_state {
	struct spi_device	*us;
	struct iio_trigger	*trig;
	struct mutex		buf_lock;
	u8			tx[ADIS16203_MAX_TX] ____cacheline_aligned;
	u8			rx[ADIS16203_MAX_RX];
};

int adis16203_set_irq(struct iio_dev *indio_dev, bool enable);

enum adis16203_scan {
	ADIS16203_SCAN_SUPPLY,
	ADIS16203_SCAN_AUX_ADC,
	ADIS16203_SCAN_TEMP,
	ADIS16203_SCAN_INCLI_X,
	ADIS16203_SCAN_INCLI_Y,
};

#ifdef CONFIG_IIO_BUFFER
void adis16203_remove_trigger(struct iio_dev *indio_dev);
int adis16203_probe_trigger(struct iio_dev *indio_dev);

ssize_t adis16203_read_data_from_ring(struct device *dev,
				      struct device_attribute *attr,
				      char *buf);

int adis16203_configure_ring(struct iio_dev *indio_dev);
void adis16203_unconfigure_ring(struct iio_dev *indio_dev);

#else 

static inline void adis16203_remove_trigger(struct iio_dev *indio_dev)
{
}

static inline int adis16203_probe_trigger(struct iio_dev *indio_dev)
{
	return 0;
}

static inline ssize_t
adis16203_read_data_from_ring(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	return 0;
}

static int adis16203_configure_ring(struct iio_dev *indio_dev)
{
	return 0;
}

static inline void adis16203_unconfigure_ring(struct iio_dev *indio_dev)
{
}

#endif 
#endif 
