#ifndef SPI_ADIS16201_H_
#define SPI_ADIS16201_H_

#define ADIS16201_STARTUP_DELAY	220 

#define ADIS16201_READ_REG(a)    a
#define ADIS16201_WRITE_REG(a) ((a) | 0x80)

#define ADIS16201_FLASH_CNT      0x00 
#define ADIS16201_SUPPLY_OUT     0x02 
#define ADIS16201_XACCL_OUT      0x04 
#define ADIS16201_YACCL_OUT      0x06 
#define ADIS16201_AUX_ADC        0x08 
#define ADIS16201_TEMP_OUT       0x0A 
#define ADIS16201_XINCL_OUT      0x0C 
#define ADIS16201_YINCL_OUT      0x0E 
#define ADIS16201_XACCL_OFFS     0x10 
#define ADIS16201_YACCL_OFFS     0x12 
#define ADIS16201_XACCL_SCALE    0x14 
#define ADIS16201_YACCL_SCALE    0x16 
#define ADIS16201_XINCL_OFFS     0x18 
#define ADIS16201_YINCL_OFFS     0x1A 
#define ADIS16201_XINCL_SCALE    0x1C 
#define ADIS16201_YINCL_SCALE    0x1E 
#define ADIS16201_ALM_MAG1       0x20 
#define ADIS16201_ALM_MAG2       0x22 
#define ADIS16201_ALM_SMPL1      0x24 
#define ADIS16201_ALM_SMPL2      0x26 
#define ADIS16201_ALM_CTRL       0x28 
#define ADIS16201_AUX_DAC        0x30 
#define ADIS16201_GPIO_CTRL      0x32 
#define ADIS16201_MSC_CTRL       0x34 
#define ADIS16201_SMPL_PRD       0x36 
#define ADIS16201_AVG_CNT        0x38 
#define ADIS16201_SLP_CNT        0x3A 
#define ADIS16201_DIAG_STAT      0x3C 
#define ADIS16201_GLOB_CMD       0x3E 

#define ADIS16201_OUTPUTS        7

#define ADIS16201_MSC_CTRL_SELF_TEST_EN	        (1 << 8)  
#define ADIS16201_MSC_CTRL_DATA_RDY_EN	        (1 << 2)  
#define ADIS16201_MSC_CTRL_ACTIVE_HIGH	        (1 << 1)  
#define ADIS16201_MSC_CTRL_DATA_RDY_DIO1	(1 << 0)  

#define ADIS16201_DIAG_STAT_ALARM2        (1<<9) 
#define ADIS16201_DIAG_STAT_ALARM1        (1<<8) 
#define ADIS16201_DIAG_STAT_SPI_FAIL	  (1<<3) 
#define ADIS16201_DIAG_STAT_FLASH_UPT	  (1<<2) 
#define ADIS16201_DIAG_STAT_POWER_HIGH	  (1<<1) 
#define ADIS16201_DIAG_STAT_POWER_LOW	  (1<<0) 

#define ADIS16201_GLOB_CMD_SW_RESET	(1<<7)
#define ADIS16201_GLOB_CMD_FACTORY_CAL	(1<<1)

#define ADIS16201_MAX_TX 14
#define ADIS16201_MAX_RX 14

#define ADIS16201_ERROR_ACTIVE          (1<<14)

struct adis16201_state {
	struct spi_device	*us;
	struct iio_trigger	*trig;
	struct mutex		buf_lock;
	u8			tx[14] ____cacheline_aligned;
	u8			rx[14];
};

int adis16201_set_irq(struct iio_dev *indio_dev, bool enable);

enum adis16201_scan {
	ADIS16201_SCAN_SUPPLY,
	ADIS16201_SCAN_ACC_X,
	ADIS16201_SCAN_ACC_Y,
	ADIS16201_SCAN_AUX_ADC,
	ADIS16201_SCAN_TEMP,
	ADIS16201_SCAN_INCLI_X,
	ADIS16201_SCAN_INCLI_Y,
};

#ifdef CONFIG_IIO_BUFFER
void adis16201_remove_trigger(struct iio_dev *indio_dev);
int adis16201_probe_trigger(struct iio_dev *indio_dev);

ssize_t adis16201_read_data_from_ring(struct device *dev,
				      struct device_attribute *attr,
				      char *buf);

int adis16201_configure_ring(struct iio_dev *indio_dev);
void adis16201_unconfigure_ring(struct iio_dev *indio_dev);

#else 

static inline void adis16201_remove_trigger(struct iio_dev *indio_dev)
{
}

static inline int adis16201_probe_trigger(struct iio_dev *indio_dev)
{
	return 0;
}

static inline ssize_t
adis16201_read_data_from_ring(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	return 0;
}

static int adis16201_configure_ring(struct iio_dev *indio_dev)
{
	return 0;
}

static inline void adis16201_unconfigure_ring(struct iio_dev *indio_dev)
{
}

static inline int adis16201_initialize_ring(struct iio_ring_buffer *ring)
{
	return 0;
}

static inline void adis16201_uninitialize_ring(struct iio_ring_buffer *ring)
{
}

#endif 
#endif 
