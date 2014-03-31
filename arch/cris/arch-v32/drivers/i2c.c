/*!***************************************************************************
*!
*! FILE NAME  : i2c.c
*!
*! DESCRIPTION: implements an interface for IIC/I2C, both directly from other
*!              kernel modules (i2c_writereg/readreg) and from userspace using
*!              ioctl()'s
*!
*! Nov 30 1998  Torbjorn Eliasson  Initial version.
*!              Bjorn Wesen        Elinux kernel version.
*! Jan 14 2000  Johan Adolfsson    Fixed PB shadow register stuff -
*!                                 don't use PB_I2C if DS1302 uses same bits,
*!                                 use PB.
*| June 23 2003 Pieter Grimmerink  Added 'i2c_sendnack'. i2c_readreg now
*|                                 generates nack on last received byte,
*|                                 instead of ack.
*|                                 i2c_getack changed data level while clock
*|                                 was high, causing DS75 to see  a stop condition
*!
*! ---------------------------------------------------------------------------
*!
*! (C) Copyright 1999-2007 Axis Communications AB, LUND, SWEDEN
*!
*!***************************************************************************/


#include <linux/module.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/mutex.h>

#include <asm/etraxi2c.h>

#include <asm/io.h>
#include <asm/delay.h>

#include "i2c.h"


#define D(x)

#define I2C_MAJOR 123  
static DEFINE_MUTEX(i2c_mutex);
static const char i2c_name[] = "i2c";

#define CLOCK_LOW_TIME            8
#define CLOCK_HIGH_TIME           8
#define START_CONDITION_HOLD_TIME 8
#define STOP_CONDITION_HOLD_TIME  8
#define ENABLE_OUTPUT 0x01
#define ENABLE_INPUT 0x00
#define I2C_CLOCK_HIGH 1
#define I2C_CLOCK_LOW 0
#define I2C_DATA_HIGH 1
#define I2C_DATA_LOW 0

#define i2c_enable()
#define i2c_disable()


#define i2c_dir_out() crisv32_io_set_dir(&cris_i2c_data, crisv32_io_dir_out)
#define i2c_dir_in() crisv32_io_set_dir(&cris_i2c_data, crisv32_io_dir_in)


#define i2c_clk(x) crisv32_io_set(&cris_i2c_clk, x)
#define i2c_data(x) crisv32_io_set(&cris_i2c_data, x)


#define i2c_getbit() crisv32_io_rd(&cris_i2c_data)

#define i2c_delay(usecs) udelay(usecs)

static DEFINE_SPINLOCK(i2c_lock); 


static struct crisv32_iopin cris_i2c_clk;
static struct crisv32_iopin cris_i2c_data;




void
i2c_start(void)
{
	i2c_dir_out();
	i2c_delay(CLOCK_HIGH_TIME/6);
	i2c_data(I2C_DATA_HIGH);
	i2c_clk(I2C_CLOCK_HIGH);
	i2c_delay(CLOCK_HIGH_TIME);
	i2c_data(I2C_DATA_LOW);
	i2c_delay(START_CONDITION_HOLD_TIME);
	i2c_clk(I2C_CLOCK_LOW);
	i2c_delay(CLOCK_LOW_TIME);
}


void
i2c_stop(void)
{
	i2c_dir_out();

	i2c_clk(I2C_CLOCK_LOW);
	i2c_data(I2C_DATA_LOW);
	i2c_delay(CLOCK_LOW_TIME*2);
	i2c_clk(I2C_CLOCK_HIGH);
	i2c_delay(CLOCK_HIGH_TIME*2);
	i2c_data(I2C_DATA_HIGH);
	i2c_delay(STOP_CONDITION_HOLD_TIME);

	i2c_dir_in();
}


void
i2c_outbyte(unsigned char x)
{
	int i;

	i2c_dir_out();

	for (i = 0; i < 8; i++) {
		if (x & 0x80) {
			i2c_data(I2C_DATA_HIGH);
		} else {
			i2c_data(I2C_DATA_LOW);
		}

		i2c_delay(CLOCK_LOW_TIME/2);
		i2c_clk(I2C_CLOCK_HIGH);
		i2c_delay(CLOCK_HIGH_TIME);
		i2c_clk(I2C_CLOCK_LOW);
		i2c_delay(CLOCK_LOW_TIME/2);
		x <<= 1;
	}
	i2c_data(I2C_DATA_LOW);
	i2c_delay(CLOCK_LOW_TIME/2);

	i2c_dir_in();
}


unsigned char
i2c_inbyte(void)
{
	unsigned char aBitByte = 0;
	int i;

	
	i2c_disable();
	i2c_dir_in();
	i2c_delay(CLOCK_HIGH_TIME/2);

	
	aBitByte |= i2c_getbit();

	
	i2c_enable();
	i2c_delay(CLOCK_LOW_TIME/2);

	for (i = 1; i < 8; i++) {
		aBitByte <<= 1;
		
		i2c_clk(I2C_CLOCK_HIGH);
		i2c_delay(CLOCK_HIGH_TIME);
		i2c_clk(I2C_CLOCK_LOW);
		i2c_delay(CLOCK_LOW_TIME);

		
		i2c_disable();
		i2c_dir_in();
		i2c_delay(CLOCK_HIGH_TIME/2);

		
		aBitByte |= i2c_getbit();

		
		i2c_enable();
		i2c_delay(CLOCK_LOW_TIME/2);
	}
	i2c_clk(I2C_CLOCK_HIGH);
	i2c_delay(CLOCK_HIGH_TIME);

	i2c_clk(I2C_CLOCK_LOW);
	return aBitByte;
}


int
i2c_getack(void)
{
	int ack = 1;
	i2c_dir_out();
	i2c_data(I2C_DATA_HIGH);
	i2c_dir_in();
	i2c_delay(CLOCK_HIGH_TIME/4);
	i2c_clk(I2C_CLOCK_HIGH);
#if 0
	i2c_clk(1);
	i2c_data(1);
	i2c_data(1);
	i2c_disable();
	i2c_dir_in();
#endif

	i2c_delay(CLOCK_HIGH_TIME/2);
	if (i2c_getbit())
		ack = 0;
	i2c_delay(CLOCK_HIGH_TIME/2);
	if (!ack) {
		if (!i2c_getbit()) 
			ack = 1;
		i2c_delay(CLOCK_HIGH_TIME/2);
	}

#if 0
   i2c_data(I2C_DATA_LOW);

	i2c_enable();
	i2c_dir_out();
#endif
	i2c_clk(I2C_CLOCK_LOW);
	i2c_delay(CLOCK_HIGH_TIME/4);
	i2c_dir_out();
	i2c_data(I2C_DATA_HIGH);
	i2c_delay(CLOCK_LOW_TIME/2);
	return ack;
}

void
i2c_sendack(void)
{
	i2c_delay(CLOCK_LOW_TIME);
	i2c_dir_out();
	i2c_data(I2C_DATA_LOW);
	i2c_delay(CLOCK_HIGH_TIME/6);
	i2c_clk(I2C_CLOCK_HIGH);
	i2c_delay(CLOCK_HIGH_TIME);
	i2c_clk(I2C_CLOCK_LOW);
	i2c_delay(CLOCK_LOW_TIME/6);
	i2c_data(I2C_DATA_HIGH);
	i2c_delay(CLOCK_LOW_TIME);

	i2c_dir_in();
}

void
i2c_sendnack(void)
{
	i2c_delay(CLOCK_LOW_TIME);
	i2c_dir_out();
	i2c_data(I2C_DATA_HIGH);
	i2c_delay(CLOCK_HIGH_TIME/6);
	i2c_clk(I2C_CLOCK_HIGH);
	i2c_delay(CLOCK_HIGH_TIME);
	i2c_clk(I2C_CLOCK_LOW);
	i2c_delay(CLOCK_LOW_TIME);

	i2c_dir_in();
}

int
i2c_write(unsigned char theSlave, void *data, size_t nbytes)
{
	int error, cntr = 3;
	unsigned char bytes_wrote = 0;
	unsigned char value;
	unsigned long flags;

	spin_lock_irqsave(&i2c_lock, flags);

	do {
		error = 0;

		i2c_start();
		i2c_outbyte((theSlave & 0xfe));
		if (!i2c_getack())
			error = 1;
		for (bytes_wrote = 0; bytes_wrote < nbytes; bytes_wrote++) {
			memcpy(&value, data + bytes_wrote, sizeof value);
			i2c_outbyte(value);
			if (!i2c_getack())
				error |= 4;
		}
		i2c_stop();

	} while (error && cntr--);

	i2c_delay(CLOCK_LOW_TIME);

	spin_unlock_irqrestore(&i2c_lock, flags);

	return -error;
}

int
i2c_read(unsigned char theSlave, void *data, size_t nbytes)
{
	unsigned char b = 0;
	unsigned char bytes_read = 0;
	int error, cntr = 3;
	unsigned long flags;

	spin_lock_irqsave(&i2c_lock, flags);

	do {
		error = 0;
		memset(data, 0, nbytes);
		i2c_start();
		i2c_outbyte((theSlave | 0x01));
		if (!i2c_getack())
			error = 1;
		for (bytes_read = 0; bytes_read < nbytes; bytes_read++) {
			b = i2c_inbyte();
			memcpy(data + bytes_read, &b, sizeof b);

			if (bytes_read < (nbytes - 1))
				i2c_sendack();
		}
		i2c_sendnack();
		i2c_stop();
	} while (error && cntr--);

	spin_unlock_irqrestore(&i2c_lock, flags);

	return -error;
}

int
i2c_writereg(unsigned char theSlave, unsigned char theReg,
	     unsigned char theValue)
{
	int error, cntr = 3;
	unsigned long flags;

	spin_lock_irqsave(&i2c_lock, flags);

	do {
		error = 0;

		i2c_start();
		i2c_outbyte((theSlave & 0xfe));
		if(!i2c_getack())
			error = 1;
		i2c_dir_out();
		i2c_outbyte(theReg);
		if(!i2c_getack())
			error |= 2;
		i2c_outbyte(theValue);
		if(!i2c_getack())
			error |= 4;
		i2c_stop();
	} while(error && cntr--);

	i2c_delay(CLOCK_LOW_TIME);

	spin_unlock_irqrestore(&i2c_lock, flags);

	return -error;
}

unsigned char
i2c_readreg(unsigned char theSlave, unsigned char theReg)
{
	unsigned char b = 0;
	int error, cntr = 3;
	unsigned long flags;

	spin_lock_irqsave(&i2c_lock, flags);

	do {
		error = 0;
		i2c_start();

		i2c_outbyte((theSlave & 0xfe));
		if(!i2c_getack())
			error = 1;
		i2c_dir_out();
		i2c_outbyte(theReg);
		if(!i2c_getack())
			error |= 2;
		i2c_delay(CLOCK_LOW_TIME);
		i2c_start();
		i2c_outbyte(theSlave | 0x01);
		if(!i2c_getack())
			error |= 4;
		b = i2c_inbyte();
		i2c_sendnack();
		i2c_stop();

	} while(error && cntr--);

	spin_unlock_irqrestore(&i2c_lock, flags);

	return b;
}

static int
i2c_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int
i2c_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static long
i2c_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	if(_IOC_TYPE(cmd) != ETRAXI2C_IOCTYPE) {
		return -ENOTTY;
	}

	switch (_IOC_NR(cmd)) {
		case I2C_WRITEREG:
			
			D(printk("i2cw %d %d %d\n",
				 I2C_ARGSLAVE(arg),
				 I2C_ARGREG(arg),
				 I2C_ARGVALUE(arg)));

			mutex_lock(&i2c_mutex);
			ret = i2c_writereg(I2C_ARGSLAVE(arg),
					    I2C_ARGREG(arg),
					    I2C_ARGVALUE(arg));
			mutex_unlock(&i2c_mutex);
			return ret;

		case I2C_READREG:
		{
			unsigned char val;
			
			D(printk("i2cr %d %d ",
				I2C_ARGSLAVE(arg),
				I2C_ARGREG(arg)));
			mutex_lock(&i2c_mutex);
			val = i2c_readreg(I2C_ARGSLAVE(arg), I2C_ARGREG(arg));
			mutex_unlock(&i2c_mutex);
			D(printk("= %d\n", val));
			return val;
		}
		default:
			return -EINVAL;

	}

	return 0;
}

static const struct file_operations i2c_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl = i2c_ioctl,
	.open		= i2c_open,
	.release	= i2c_release,
	.llseek		= noop_llseek,
};

static int __init i2c_init(void)
{
	static int res;
	static int first = 1;

	if (!first)
		return res;

	first = 0;

	

	res = crisv32_io_get_name(&cris_i2c_data,
		CONFIG_ETRAX_V32_I2C_DATA_PORT);
	if (res < 0)
		return res;

	res = crisv32_io_get_name(&cris_i2c_clk, CONFIG_ETRAX_V32_I2C_CLK_PORT);
	crisv32_io_set_dir(&cris_i2c_clk, crisv32_io_dir_out);

	return res;
}


static int __init i2c_register(void)
{
	int res;

	res = i2c_init();
	if (res < 0)
		return res;

	

	res = register_chrdev(I2C_MAJOR, i2c_name, &i2c_fops);
	if (res < 0) {
		printk(KERN_ERR "i2c: couldn't get a major number.\n");
		return res;
	}

	printk(KERN_INFO
		"I2C driver v2.2, (c) 1999-2007 Axis Communications AB\n");

	return 0;
}
module_init(i2c_register);

