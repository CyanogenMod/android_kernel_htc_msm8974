/* -------------------------------------------------------------------------
 * i2c-algo-bit.c i2c driver algorithms for bit-shift adapters
 * -------------------------------------------------------------------------
 *   Copyright (C) 1995-2000 Simon G. Vogl

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.
 * ------------------------------------------------------------------------- */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>



#ifdef DEBUG
#define bit_dbg(level, dev, format, args...) \
	do { \
		if (i2c_debug >= level) \
			dev_dbg(dev, format, ##args); \
	} while (0)
#else
#define bit_dbg(level, dev, format, args...) \
	do {} while (0)
#endif 


static int bit_test;	
module_param(bit_test, int, S_IRUGO);
MODULE_PARM_DESC(bit_test, "lines testing - 0 off; 1 report; 2 fail if stuck");

#ifdef DEBUG
static int i2c_debug = 1;
module_param(i2c_debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(i2c_debug,
		 "debug level - 0 off; 1 normal; 2 verbose; 3 very verbose");
#endif


#define setsda(adap, val)	adap->setsda(adap->data, val)
#define setscl(adap, val)	adap->setscl(adap->data, val)
#define getsda(adap)		adap->getsda(adap->data)
#define getscl(adap)		adap->getscl(adap->data)

static inline void sdalo(struct i2c_algo_bit_data *adap)
{
	setsda(adap, 0);
	udelay((adap->udelay + 1) / 2);
}

static inline void sdahi(struct i2c_algo_bit_data *adap)
{
	setsda(adap, 1);
	udelay((adap->udelay + 1) / 2);
}

static inline void scllo(struct i2c_algo_bit_data *adap)
{
	setscl(adap, 0);
	udelay(adap->udelay / 2);
}

static int sclhi(struct i2c_algo_bit_data *adap)
{
	unsigned long start;

	setscl(adap, 1);

	
	if (!adap->getscl)
		goto done;

	start = jiffies;
	while (!getscl(adap)) {
		if (time_after(jiffies, start + adap->timeout)) {
			if (getscl(adap))
				break;
			return -ETIMEDOUT;
		}
		cpu_relax();
	}
#ifdef DEBUG
	if (jiffies != start && i2c_debug >= 3)
		pr_debug("i2c-algo-bit: needed %ld jiffies for SCL to go "
			 "high\n", jiffies - start);
#endif

done:
	udelay(adap->udelay);
	return 0;
}


static void i2c_start(struct i2c_algo_bit_data *adap)
{
	
	setsda(adap, 0);
	udelay(adap->udelay);
	scllo(adap);
}

static void i2c_repstart(struct i2c_algo_bit_data *adap)
{
	
	sdahi(adap);
	sclhi(adap);
	setsda(adap, 0);
	udelay(adap->udelay);
	scllo(adap);
}


static void i2c_stop(struct i2c_algo_bit_data *adap)
{
	
	sdalo(adap);
	sclhi(adap);
	setsda(adap, 1);
	udelay(adap->udelay);
}



static int i2c_outb(struct i2c_adapter *i2c_adap, unsigned char c)
{
	int i;
	int sb;
	int ack;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	
	for (i = 7; i >= 0; i--) {
		sb = (c >> i) & 1;
		setsda(adap, sb);
		udelay((adap->udelay + 1) / 2);
		if (sclhi(adap) < 0) { 
			bit_dbg(1, &i2c_adap->dev, "i2c_outb: 0x%02x, "
				"timeout at bit #%d\n", (int)c, i);
			return -ETIMEDOUT;
		}
		scllo(adap);
	}
	sdahi(adap);
	if (sclhi(adap) < 0) { 
		bit_dbg(1, &i2c_adap->dev, "i2c_outb: 0x%02x, "
			"timeout at ack\n", (int)c);
		return -ETIMEDOUT;
	}

	ack = !getsda(adap);    
	bit_dbg(2, &i2c_adap->dev, "i2c_outb: 0x%02x %s\n", (int)c,
		ack ? "A" : "NA");

	scllo(adap);
	return ack;
	
}


static int i2c_inb(struct i2c_adapter *i2c_adap)
{
	
	
	int i;
	unsigned char indata = 0;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	
	sdahi(adap);
	for (i = 0; i < 8; i++) {
		if (sclhi(adap) < 0) { 
			bit_dbg(1, &i2c_adap->dev, "i2c_inb: timeout at bit "
				"#%d\n", 7 - i);
			return -ETIMEDOUT;
		}
		indata *= 2;
		if (getsda(adap))
			indata |= 0x01;
		setscl(adap, 0);
		udelay(i == 7 ? adap->udelay / 2 : adap->udelay);
	}
	
	return indata;
}

static int test_bus(struct i2c_adapter *i2c_adap)
{
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;
	const char *name = i2c_adap->name;
	int scl, sda, ret;

	if (adap->pre_xfer) {
		ret = adap->pre_xfer(i2c_adap);
		if (ret < 0)
			return -ENODEV;
	}

	if (adap->getscl == NULL)
		pr_info("%s: Testing SDA only, SCL is not readable\n", name);

	sda = getsda(adap);
	scl = (adap->getscl == NULL) ? 1 : getscl(adap);
	if (!scl || !sda) {
		printk(KERN_WARNING
		       "%s: bus seems to be busy (scl=%d, sda=%d)\n",
		       name, scl, sda);
		goto bailout;
	}

	sdalo(adap);
	sda = getsda(adap);
	scl = (adap->getscl == NULL) ? 1 : getscl(adap);
	if (sda) {
		printk(KERN_WARNING "%s: SDA stuck high!\n", name);
		goto bailout;
	}
	if (!scl) {
		printk(KERN_WARNING "%s: SCL unexpected low "
		       "while pulling SDA low!\n", name);
		goto bailout;
	}

	sdahi(adap);
	sda = getsda(adap);
	scl = (adap->getscl == NULL) ? 1 : getscl(adap);
	if (!sda) {
		printk(KERN_WARNING "%s: SDA stuck low!\n", name);
		goto bailout;
	}
	if (!scl) {
		printk(KERN_WARNING "%s: SCL unexpected low "
		       "while pulling SDA high!\n", name);
		goto bailout;
	}

	scllo(adap);
	sda = getsda(adap);
	scl = (adap->getscl == NULL) ? 0 : getscl(adap);
	if (scl) {
		printk(KERN_WARNING "%s: SCL stuck high!\n", name);
		goto bailout;
	}
	if (!sda) {
		printk(KERN_WARNING "%s: SDA unexpected low "
		       "while pulling SCL low!\n", name);
		goto bailout;
	}

	sclhi(adap);
	sda = getsda(adap);
	scl = (adap->getscl == NULL) ? 1 : getscl(adap);
	if (!scl) {
		printk(KERN_WARNING "%s: SCL stuck low!\n", name);
		goto bailout;
	}
	if (!sda) {
		printk(KERN_WARNING "%s: SDA unexpected low "
		       "while pulling SCL high!\n", name);
		goto bailout;
	}

	if (adap->post_xfer)
		adap->post_xfer(i2c_adap);

	pr_info("%s: Test OK\n", name);
	return 0;
bailout:
	sdahi(adap);
	sclhi(adap);

	if (adap->post_xfer)
		adap->post_xfer(i2c_adap);

	return -ENODEV;
}


static int try_address(struct i2c_adapter *i2c_adap,
		       unsigned char addr, int retries)
{
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;
	int i, ret = 0;

	for (i = 0; i <= retries; i++) {
		ret = i2c_outb(i2c_adap, addr);
		if (ret == 1 || i == retries)
			break;
		bit_dbg(3, &i2c_adap->dev, "emitting stop condition\n");
		i2c_stop(adap);
		udelay(adap->udelay);
		yield();
		bit_dbg(3, &i2c_adap->dev, "emitting start condition\n");
		i2c_start(adap);
	}
	if (i && ret)
		bit_dbg(1, &i2c_adap->dev, "Used %d tries to %s client at "
			"0x%02x: %s\n", i + 1,
			addr & 1 ? "read from" : "write to", addr >> 1,
			ret == 1 ? "success" : "failed, timeout?");
	return ret;
}

static int sendbytes(struct i2c_adapter *i2c_adap, struct i2c_msg *msg)
{
	const unsigned char *temp = msg->buf;
	int count = msg->len;
	unsigned short nak_ok = msg->flags & I2C_M_IGNORE_NAK;
	int retval;
	int wrcount = 0;

	while (count > 0) {
		retval = i2c_outb(i2c_adap, *temp);

		
		if ((retval > 0) || (nak_ok && (retval == 0))) {
			count--;
			temp++;
			wrcount++;

		} else if (retval == 0) {
			dev_err(&i2c_adap->dev, "sendbytes: NAK bailout.\n");
			return -EIO;

		} else {
			dev_err(&i2c_adap->dev, "sendbytes: error %d\n",
					retval);
			return retval;
		}
	}
	return wrcount;
}

static int acknak(struct i2c_adapter *i2c_adap, int is_ack)
{
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	
	if (is_ack)		
		setsda(adap, 0);
	udelay((adap->udelay + 1) / 2);
	if (sclhi(adap) < 0) {	
		dev_err(&i2c_adap->dev, "readbytes: ack/nak timeout\n");
		return -ETIMEDOUT;
	}
	scllo(adap);
	return 0;
}

static int readbytes(struct i2c_adapter *i2c_adap, struct i2c_msg *msg)
{
	int inval;
	int rdcount = 0;	
	unsigned char *temp = msg->buf;
	int count = msg->len;
	const unsigned flags = msg->flags;

	while (count > 0) {
		inval = i2c_inb(i2c_adap);
		if (inval >= 0) {
			*temp = inval;
			rdcount++;
		} else {   
			break;
		}

		temp++;
		count--;

		if (rdcount == 1 && (flags & I2C_M_RECV_LEN)) {
			if (inval <= 0 || inval > I2C_SMBUS_BLOCK_MAX) {
				if (!(flags & I2C_M_NO_RD_ACK))
					acknak(i2c_adap, 0);
				dev_err(&i2c_adap->dev, "readbytes: invalid "
					"block length (%d)\n", inval);
				return -EPROTO;
			}
			count += inval;
			msg->len += inval;
		}

		bit_dbg(2, &i2c_adap->dev, "readbytes: 0x%02x %s\n",
			inval,
			(flags & I2C_M_NO_RD_ACK)
				? "(no ack/nak)"
				: (count ? "A" : "NA"));

		if (!(flags & I2C_M_NO_RD_ACK)) {
			inval = acknak(i2c_adap, count);
			if (inval < 0)
				return inval;
		}
	}
	return rdcount;
}

static int bit_doAddress(struct i2c_adapter *i2c_adap, struct i2c_msg *msg)
{
	unsigned short flags = msg->flags;
	unsigned short nak_ok = msg->flags & I2C_M_IGNORE_NAK;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	unsigned char addr;
	int ret, retries;

	retries = nak_ok ? 0 : i2c_adap->retries;

	if (flags & I2C_M_TEN) {
		
		addr = 0xf0 | ((msg->addr >> 7) & 0x06);
		bit_dbg(2, &i2c_adap->dev, "addr0: %d\n", addr);
		
		ret = try_address(i2c_adap, addr, retries);
		if ((ret != 1) && !nak_ok)  {
			dev_err(&i2c_adap->dev,
				"died at extended address code\n");
			return -ENXIO;
		}
		
		ret = i2c_outb(i2c_adap, msg->addr & 0xff);
		if ((ret != 1) && !nak_ok) {
			
			dev_err(&i2c_adap->dev, "died at 2nd address code\n");
			return -ENXIO;
		}
		if (flags & I2C_M_RD) {
			bit_dbg(3, &i2c_adap->dev, "emitting repeated "
				"start condition\n");
			i2c_repstart(adap);
			
			addr |= 0x01;
			ret = try_address(i2c_adap, addr, retries);
			if ((ret != 1) && !nak_ok) {
				dev_err(&i2c_adap->dev,
					"died at repeated address code\n");
				return -EIO;
			}
		}
	} else {		
		addr = msg->addr << 1;
		if (flags & I2C_M_RD)
			addr |= 1;
		if (flags & I2C_M_REV_DIR_ADDR)
			addr ^= 1;
		ret = try_address(i2c_adap, addr, retries);
		if ((ret != 1) && !nak_ok)
			return -ENXIO;
	}

	return 0;
}

static int bit_xfer(struct i2c_adapter *i2c_adap,
		    struct i2c_msg msgs[], int num)
{
	struct i2c_msg *pmsg;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;
	int i, ret;
	unsigned short nak_ok;

	if (adap->pre_xfer) {
		ret = adap->pre_xfer(i2c_adap);
		if (ret < 0)
			return ret;
	}

	bit_dbg(3, &i2c_adap->dev, "emitting start condition\n");
	i2c_start(adap);
	for (i = 0; i < num; i++) {
		pmsg = &msgs[i];
		nak_ok = pmsg->flags & I2C_M_IGNORE_NAK;
		if (!(pmsg->flags & I2C_M_NOSTART)) {
			if (i) {
				bit_dbg(3, &i2c_adap->dev, "emitting "
					"repeated start condition\n");
				i2c_repstart(adap);
			}
			ret = bit_doAddress(i2c_adap, pmsg);
			if ((ret != 0) && !nak_ok) {
				bit_dbg(1, &i2c_adap->dev, "NAK from "
					"device addr 0x%02x msg #%d\n",
					msgs[i].addr, i);
				goto bailout;
			}
		}
		if (pmsg->flags & I2C_M_RD) {
			
			ret = readbytes(i2c_adap, pmsg);
			if (ret >= 1)
				bit_dbg(2, &i2c_adap->dev, "read %d byte%s\n",
					ret, ret == 1 ? "" : "s");
			if (ret < pmsg->len) {
				if (ret >= 0)
					ret = -EIO;
				goto bailout;
			}
		} else {
			
			ret = sendbytes(i2c_adap, pmsg);
			if (ret >= 1)
				bit_dbg(2, &i2c_adap->dev, "wrote %d byte%s\n",
					ret, ret == 1 ? "" : "s");
			if (ret < pmsg->len) {
				if (ret >= 0)
					ret = -EIO;
				goto bailout;
			}
		}
	}
	ret = i;

bailout:
	bit_dbg(3, &i2c_adap->dev, "emitting stop condition\n");
	i2c_stop(adap);

	if (adap->post_xfer)
		adap->post_xfer(i2c_adap);
	return ret;
}

static u32 bit_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL |
	       I2C_FUNC_SMBUS_READ_BLOCK_DATA |
	       I2C_FUNC_SMBUS_BLOCK_PROC_CALL |
	       I2C_FUNC_10BIT_ADDR | I2C_FUNC_PROTOCOL_MANGLING;
}



const struct i2c_algorithm i2c_bit_algo = {
	.master_xfer	= bit_xfer,
	.functionality	= bit_func,
};
EXPORT_SYMBOL(i2c_bit_algo);

static int __i2c_bit_add_bus(struct i2c_adapter *adap,
			     int (*add_adapter)(struct i2c_adapter *))
{
	struct i2c_algo_bit_data *bit_adap = adap->algo_data;
	int ret;

	if (bit_test) {
		ret = test_bus(adap);
		if (bit_test >= 2 && ret < 0)
			return -ENODEV;
	}

	
	adap->algo = &i2c_bit_algo;
	adap->retries = 3;

	ret = add_adapter(adap);
	if (ret < 0)
		return ret;

	
	if (bit_adap->getscl == NULL) {
		dev_warn(&adap->dev, "Not I2C compliant: can't read SCL\n");
		dev_warn(&adap->dev, "Bus may be unreliable\n");
	}
	return 0;
}

int i2c_bit_add_bus(struct i2c_adapter *adap)
{
	return __i2c_bit_add_bus(adap, i2c_add_adapter);
}
EXPORT_SYMBOL(i2c_bit_add_bus);

int i2c_bit_add_numbered_bus(struct i2c_adapter *adap)
{
	return __i2c_bit_add_bus(adap, i2c_add_numbered_adapter);
}
EXPORT_SYMBOL(i2c_bit_add_numbered_bus);

MODULE_AUTHOR("Simon G. Vogl <simon@tk.uni-linz.ac.at>");
MODULE_DESCRIPTION("I2C-Bus bit-banging algorithm");
MODULE_LICENSE("GPL");
