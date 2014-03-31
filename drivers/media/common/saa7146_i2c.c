#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <media/saa7146_vv.h>

static u32 saa7146_i2c_func(struct i2c_adapter *adapter)
{
	

	return	  I2C_FUNC_I2C
		| I2C_FUNC_SMBUS_QUICK
		| I2C_FUNC_SMBUS_READ_BYTE	| I2C_FUNC_SMBUS_WRITE_BYTE
		| I2C_FUNC_SMBUS_READ_BYTE_DATA | I2C_FUNC_SMBUS_WRITE_BYTE_DATA;
}

static inline u32 saa7146_i2c_status(struct saa7146_dev *dev)
{
	u32 iicsta = saa7146_read(dev, I2C_STATUS);
	
	return iicsta;
}

static int saa7146_i2c_msg_prepare(const struct i2c_msg *m, int num, __le32 *op)
{
	int h1, h2;
	int i, j, addr;
	int mem = 0, op_count = 0;

	
	for(i = 0; i < num; i++) {
		mem += m[i].len + 1;
	}

	mem = 1 + ((mem-1) / 3);

	if ((4 * mem) > SAA7146_I2C_MEM) {
		
		return -ENOMEM;
	}

	
	memset(op,0,sizeof(__le32)*mem);

	
	for(i = 0; i < num; i++) {

		addr = (m[i].addr*2) + ( (0 != (m[i].flags & I2C_M_RD)) ? 1 : 0);
		h1 = op_count/3; h2 = op_count%3;
		op[h1] |= cpu_to_le32(	    (u8)addr << ((3-h2)*8));
		op[h1] |= cpu_to_le32(SAA7146_I2C_START << ((3-h2)*2));
		op_count++;

		
		for(j = 0; j < m[i].len; j++) {
			
			h1 = op_count/3; h2 = op_count%3;
			op[h1] |= cpu_to_le32( (u32)((u8)m[i].buf[j]) << ((3-h2)*8));
			op[h1] |= cpu_to_le32(       SAA7146_I2C_CONT << ((3-h2)*2));
			op_count++;
		}

	}

	h1 = (op_count-1)/3; h2 = (op_count-1)%3;
	if ( SAA7146_I2C_CONT == (0x3 & (le32_to_cpu(op[h1]) >> ((3-h2)*2))) ) {
		op[h1] &= ~cpu_to_le32(0x2 << ((3-h2)*2));
		op[h1] |= cpu_to_le32(SAA7146_I2C_STOP << ((3-h2)*2));
	}

	
	return mem;
}

static int saa7146_i2c_msg_cleanup(const struct i2c_msg *m, int num, __le32 *op)
{
	int i, j;
	int op_count = 0;

	
	for(i = 0; i < num; i++) {

		op_count++;

		
		for(j = 0; j < m[i].len; j++) {
			
			m[i].buf[j] = (le32_to_cpu(op[op_count/3]) >> ((3-(op_count%3))*8));
			op_count++;
		}
	}

	return 0;
}

static int saa7146_i2c_reset(struct saa7146_dev *dev)
{
	
	u32 status = saa7146_i2c_status(dev);

	
	saa7146_write(dev, I2C_STATUS, dev->i2c_bitrate);
	saa7146_write(dev, I2C_TRANSFER, 0);

	
	if ( 0 != ( status & SAA7146_I2C_BUSY) ) {

		
		DEB_I2C("busy_state detected\n");

		
		saa7146_write(dev, I2C_STATUS, (dev->i2c_bitrate | MASK_07));
		saa7146_write(dev, MC2, (MASK_00 | MASK_16));
		msleep(SAA7146_I2C_DELAY);

		
		saa7146_write(dev, I2C_STATUS, dev->i2c_bitrate);
		saa7146_write(dev, MC2, (MASK_00 | MASK_16));
		msleep(SAA7146_I2C_DELAY);
	}

	
	status = saa7146_i2c_status(dev);

	if ( dev->i2c_bitrate != status ) {

		DEB_I2C("error_state detected. status:0x%08x\n", status);

		saa7146_write(dev, I2C_STATUS, (dev->i2c_bitrate | MASK_07));
		saa7146_write(dev, MC2, (MASK_00 | MASK_16));
		msleep(SAA7146_I2C_DELAY);

		
		saa7146_write(dev, I2C_STATUS, dev->i2c_bitrate);
		saa7146_write(dev, MC2, (MASK_00 | MASK_16));
		msleep(SAA7146_I2C_DELAY);

		saa7146_write(dev, I2C_STATUS, dev->i2c_bitrate);
		saa7146_write(dev, MC2, (MASK_00 | MASK_16));
		msleep(SAA7146_I2C_DELAY);
	}

	
	status = saa7146_i2c_status(dev);
	if ( dev->i2c_bitrate != status ) {
		DEB_I2C("fatal error. status:0x%08x\n", status);
		return -1;
	}

	return 0;
}

static int saa7146_i2c_writeout(struct saa7146_dev *dev, __le32 *dword, int short_delay)
{
	u32 status = 0, mc2 = 0;
	int trial = 0;
	unsigned long timeout;

	
	DEB_I2C("before: 0x%08x (status: 0x%08x), %d\n",
		*dword, saa7146_read(dev, I2C_STATUS), dev->i2c_op);

	if( 0 != (SAA7146_USE_I2C_IRQ & dev->ext->flags)) {

		saa7146_write(dev, I2C_STATUS,	 dev->i2c_bitrate);
		saa7146_write(dev, I2C_TRANSFER, le32_to_cpu(*dword));

		dev->i2c_op = 1;
		SAA7146_ISR_CLEAR(dev, MASK_16|MASK_17);
		SAA7146_IER_ENABLE(dev, MASK_16|MASK_17);
		saa7146_write(dev, MC2, (MASK_00 | MASK_16));

		timeout = HZ/100 + 1; 
		timeout = wait_event_interruptible_timeout(dev->i2c_wq, dev->i2c_op == 0, timeout);
		if (timeout == -ERESTARTSYS || dev->i2c_op) {
			SAA7146_IER_DISABLE(dev, MASK_16|MASK_17);
			SAA7146_ISR_CLEAR(dev, MASK_16|MASK_17);
			if (timeout == -ERESTARTSYS)
				
				return -ERESTARTSYS;

			pr_warn("%s %s [irq]: timed out waiting for end of xfer\n",
				dev->name, __func__);
			return -EIO;
		}
		status = saa7146_read(dev, I2C_STATUS);
	} else {
		saa7146_write(dev, I2C_STATUS,	 dev->i2c_bitrate);
		saa7146_write(dev, I2C_TRANSFER, le32_to_cpu(*dword));
		saa7146_write(dev, MC2, (MASK_00 | MASK_16));

		
		timeout = jiffies + HZ/100 + 1; 
		while(1) {
			mc2 = (saa7146_read(dev, MC2) & 0x1);
			if( 0 != mc2 ) {
				break;
			}
			if (time_after(jiffies,timeout)) {
				pr_warn("%s %s: timed out waiting for MC2\n",
					dev->name, __func__);
				return -EIO;
			}
		}
		
		timeout = jiffies + HZ/100 + 1; 
		
		saa7146_i2c_status(dev);
		while(1) {
			status = saa7146_i2c_status(dev);
			if ((status & 0x3) != 1)
				break;
			if (time_after(jiffies,timeout)) {
				pr_warn("%s %s [poll]: timed out waiting for end of xfer\n",
					dev->name, __func__);
				return -EIO;
			}
			if (++trial < 50 && short_delay)
				udelay(10);
			else
				msleep(1);
		}
	}

	
	if ( 0 != (status & (SAA7146_I2C_SPERR | SAA7146_I2C_APERR |
			     SAA7146_I2C_DTERR | SAA7146_I2C_DRERR |
			     SAA7146_I2C_AL    | SAA7146_I2C_ERR   |
			     SAA7146_I2C_BUSY)) ) {

		if ( 0 == (status & SAA7146_I2C_ERR) ||
		     0 == (status & SAA7146_I2C_BUSY) ) {
			
			DEB_I2C("unexpected i2c status %04x\n", status);
		}
		if( 0 != (status & SAA7146_I2C_SPERR) ) {
			DEB_I2C("error due to invalid start/stop condition\n");
		}
		if( 0 != (status & SAA7146_I2C_DTERR) ) {
			DEB_I2C("error in data transmission\n");
		}
		if( 0 != (status & SAA7146_I2C_DRERR) ) {
			DEB_I2C("error when receiving data\n");
		}
		if( 0 != (status & SAA7146_I2C_AL) ) {
			DEB_I2C("error because arbitration lost\n");
		}

		
		if( 0 != (status & SAA7146_I2C_APERR) ) {
			DEB_I2C("error in address phase\n");
			return -EREMOTEIO;
		}

		return -EIO;
	}

	
	*dword = cpu_to_le32(saa7146_read(dev, I2C_TRANSFER));

	DEB_I2C("after: 0x%08x\n", *dword);
	return 0;
}

static int saa7146_i2c_transfer(struct saa7146_dev *dev, const struct i2c_msg *msgs, int num, int retries)
{
	int i = 0, count = 0;
	__le32 *buffer = dev->d_i2c.cpu_addr;
	int err = 0;
	int short_delay = 0;

	if (mutex_lock_interruptible(&dev->i2c_lock))
		return -ERESTARTSYS;

	for(i=0;i<num;i++) {
		DEB_I2C("msg:%d/%d\n", i+1, num);
	}

	
	count = saa7146_i2c_msg_prepare(msgs, num, buffer);
	if ( 0 > count ) {
		err = -1;
		goto out;
	}

	if ( count > 3 || 0 != (SAA7146_I2C_SHORT_DELAY & dev->ext->flags) )
		short_delay = 1;

	do {
		
		err = saa7146_i2c_reset(dev);
		if ( 0 > err ) {
			DEB_I2C("could not reset i2c-device\n");
			goto out;
		}

		
		for(i = 0; i < count; i++) {
			err = saa7146_i2c_writeout(dev, &buffer[i], short_delay);
			if ( 0 != err) {
				if (-EREMOTEIO == err && 0 != (SAA7146_USE_I2C_IRQ & dev->ext->flags))
					goto out;
				DEB_I2C("error while sending message(s). starting again\n");
				break;
			}
		}
		if( 0 == err ) {
			err = num;
			break;
		}

		
		msleep(10);

	} while (err != num && retries--);

	
	if (err != num)
		goto out;

	
	if ( 0 != saa7146_i2c_msg_cleanup(msgs, num, buffer)) {
		DEB_I2C("could not cleanup i2c-message\n");
		err = -1;
		goto out;
	}

	
	DEB_I2C("transmission successful. (msg:%d)\n", err);
out:
	if( 0 == dev->revision ) {
		__le32 zero = 0;
		saa7146_i2c_reset(dev);
		if( 0 != saa7146_i2c_writeout(dev, &zero, short_delay)) {
			pr_info("revision 0 error. this should never happen\n");
		}
	}

	mutex_unlock(&dev->i2c_lock);
	return err;
}

static int saa7146_i2c_xfer(struct i2c_adapter* adapter, struct i2c_msg *msg, int num)
{
	struct v4l2_device *v4l2_dev = i2c_get_adapdata(adapter);
	struct saa7146_dev *dev = to_saa7146_dev(v4l2_dev);

	
	return saa7146_i2c_transfer(dev, msg, num, adapter->retries);
}



static struct i2c_algorithm saa7146_algo = {
	.master_xfer	= saa7146_i2c_xfer,
	.functionality	= saa7146_i2c_func,
};

int saa7146_i2c_adapter_prepare(struct saa7146_dev *dev, struct i2c_adapter *i2c_adapter, u32 bitrate)
{
	DEB_EE("bitrate: 0x%08x\n", bitrate);

	
	saa7146_write(dev, MC1, (MASK_08 | MASK_24));

	dev->i2c_bitrate = bitrate;
	saa7146_i2c_reset(dev);

	if (i2c_adapter) {
		i2c_set_adapdata(i2c_adapter, &dev->v4l2_dev);
		i2c_adapter->dev.parent    = &dev->pci->dev;
		i2c_adapter->algo	   = &saa7146_algo;
		i2c_adapter->algo_data     = NULL;
		i2c_adapter->timeout = SAA7146_I2C_TIMEOUT;
		i2c_adapter->retries = SAA7146_I2C_RETRIES;
	}

	return 0;
}
