
/* The industrial I/O core, trigger consumer handling functions
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifdef CONFIG_IIO_TRIGGER
void iio_device_register_trigger_consumer(struct iio_dev *indio_dev);

void iio_device_unregister_trigger_consumer(struct iio_dev *indio_dev);

#else

static int iio_device_register_trigger_consumer(struct iio_dev *indio_dev)
{
	return 0;
};

static void iio_device_unregister_trigger_consumer(struct iio_dev *indio_dev)
{
};

#endif 



