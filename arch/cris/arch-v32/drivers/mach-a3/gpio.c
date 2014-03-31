/*
 * Artec-3 general port I/O device
 *
 * Copyright (c) 2007 Axis Communications AB
 *
 * Authors:    Bjorn Wesen      (initial version)
 *             Ola Knutsson     (LED handling)
 *             Johan Adolfsson  (read/set directions, write, port G,
 *                               port to ETRAX FS.
 *             Ricard Wanderlof (PWM for Artpec-3)
 *
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

#include <asm/etraxgpio.h>
#include <hwregs/reg_map.h>
#include <hwregs/reg_rdwr.h>
#include <hwregs/gio_defs.h>
#include <hwregs/intr_vect_defs.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/pinmux.h>

#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
#include "../i2c.h"

#define VIRT_I2C_ADDR 0x40
#endif


#define GPIO_MAJOR 120  

#define I2C_INTERRUPT_BITS 0x300 

#define D(x)

#if 0
static int dp_cnt;
#define DP(x) \
	do { \
		dp_cnt++; \
		if (dp_cnt % 1000 == 0) \
			x; \
	} while (0)
#else
#define DP(x)
#endif

static DEFINE_MUTEX(gpio_mutex);
static char gpio_name[] = "etrax gpio";

#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
static int virtual_gpio_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg);
#endif
static long gpio_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t gpio_write(struct file *file, const char __user *buf,
	size_t count, loff_t *off);
static int gpio_open(struct inode *inode, struct file *filp);
static int gpio_release(struct inode *inode, struct file *filp);
static unsigned int gpio_poll(struct file *filp,
	struct poll_table_struct *wait);


struct gpio_private {
	struct gpio_private *next;
	
	unsigned char clk_mask;
	unsigned char data_mask;
	unsigned char write_msb;
	unsigned char pad1;
	
	unsigned long highalarm, lowalarm;
	wait_queue_head_t alarm_wq;
	int minor;
};

static void gpio_set_alarm(struct gpio_private *priv);
static int gpio_leds_ioctl(unsigned int cmd, unsigned long arg);
static int gpio_pwm_ioctl(struct gpio_private *priv, unsigned int cmd,
	unsigned long arg);



static struct gpio_private *alarmlist;

static int wanted_interrupts;

static DEFINE_SPINLOCK(gpio_lock);

#define NUM_PORTS (GPIO_MINOR_LAST+1)
#define GIO_REG_RD_ADDR(reg) \
	(unsigned long *)(regi_gio + REG_RD_ADDR_gio_##reg)
#define GIO_REG_WR_ADDR(reg) \
	(unsigned long *)(regi_gio + REG_WR_ADDR_gio_##reg)
static unsigned long led_dummy;
static unsigned long port_d_dummy;	
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
static unsigned long port_e_dummy;	
static unsigned long virtual_dummy;
static unsigned long virtual_rw_pv_oe = CONFIG_ETRAX_DEF_GIO_PV_OE;
static unsigned short cached_virtual_gpio_read;
#endif

static unsigned long *data_out[NUM_PORTS] = {
	GIO_REG_WR_ADDR(rw_pa_dout),
	GIO_REG_WR_ADDR(rw_pb_dout),
	&led_dummy,
	GIO_REG_WR_ADDR(rw_pc_dout),
	&port_d_dummy,
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	&port_e_dummy,
	&virtual_dummy,
#endif
};

static unsigned long *data_in[NUM_PORTS] = {
	GIO_REG_RD_ADDR(r_pa_din),
	GIO_REG_RD_ADDR(r_pb_din),
	&led_dummy,
	GIO_REG_RD_ADDR(r_pc_din),
	GIO_REG_RD_ADDR(r_pd_din),
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	&port_e_dummy,
	&virtual_dummy,
#endif
};

static unsigned long changeable_dir[NUM_PORTS] = {
	CONFIG_ETRAX_PA_CHANGEABLE_DIR,
	CONFIG_ETRAX_PB_CHANGEABLE_DIR,
	0,
	CONFIG_ETRAX_PC_CHANGEABLE_DIR,
	0,
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	0,
	CONFIG_ETRAX_PV_CHANGEABLE_DIR,
#endif
};

static unsigned long changeable_bits[NUM_PORTS] = {
	CONFIG_ETRAX_PA_CHANGEABLE_BITS,
	CONFIG_ETRAX_PB_CHANGEABLE_BITS,
	0,
	CONFIG_ETRAX_PC_CHANGEABLE_BITS,
	0,
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	0,
	CONFIG_ETRAX_PV_CHANGEABLE_BITS,
#endif
};

static unsigned long *dir_oe[NUM_PORTS] = {
	GIO_REG_WR_ADDR(rw_pa_oe),
	GIO_REG_WR_ADDR(rw_pb_oe),
	&led_dummy,
	GIO_REG_WR_ADDR(rw_pc_oe),
	&port_d_dummy,
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	&port_e_dummy,
	&virtual_rw_pv_oe,
#endif
};

static void gpio_set_alarm(struct gpio_private *priv)
{
	int bit;
	int intr_cfg;
	int mask;
	int pins;
	unsigned long flags;

	spin_lock_irqsave(&gpio_lock, flags);
	intr_cfg = REG_RD_INT(gio, regi_gio, rw_intr_cfg);
	pins = REG_RD_INT(gio, regi_gio, rw_intr_pins);
	mask = REG_RD_INT(gio, regi_gio, rw_intr_mask) & I2C_INTERRUPT_BITS;

	for (bit = 0; bit < 32; bit++) {
		int intr = bit % 8;
		int pin = bit / 8;
		if (priv->minor < GPIO_MINOR_LEDS)
			pin += priv->minor * 4;
		else
			pin += (priv->minor - 1) * 4;

		if (priv->highalarm & (1<<bit)) {
			intr_cfg |= (regk_gio_hi << (intr * 3));
			mask |= 1 << intr;
			wanted_interrupts = mask & 0xff;
			pins |= pin << (intr * 4);
		} else if (priv->lowalarm & (1<<bit)) {
			intr_cfg |= (regk_gio_lo << (intr * 3));
			mask |= 1 << intr;
			wanted_interrupts = mask & 0xff;
			pins |= pin << (intr * 4);
		}
	}

	REG_WR_INT(gio, regi_gio, rw_intr_cfg, intr_cfg);
	REG_WR_INT(gio, regi_gio, rw_intr_pins, pins);
	REG_WR_INT(gio, regi_gio, rw_intr_mask, mask);

	spin_unlock_irqrestore(&gpio_lock, flags);
}

static unsigned int gpio_poll(struct file *file, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct gpio_private *priv = file->private_data;
	unsigned long data;
	unsigned long tmp;

	if (priv->minor >= GPIO_MINOR_PWM0 &&
	    priv->minor <= GPIO_MINOR_LAST_PWM)
		return 0;

	poll_wait(file, &priv->alarm_wq, wait);
	if (priv->minor <= GPIO_MINOR_D) {
		data = readl(data_in[priv->minor]);
		REG_WR_INT(gio, regi_gio, rw_ack_intr, wanted_interrupts);
		tmp = REG_RD_INT(gio, regi_gio, rw_intr_mask);
		tmp &= I2C_INTERRUPT_BITS;
		tmp |= wanted_interrupts;
		REG_WR_INT(gio, regi_gio, rw_intr_mask, tmp);
	} else
		return 0;

	if ((data & priv->highalarm) || (~data & priv->lowalarm))
		mask = POLLIN|POLLRDNORM;

	DP(printk(KERN_DEBUG "gpio_poll ready: mask 0x%08X\n", mask));
	return mask;
}

static irqreturn_t gpio_interrupt(int irq, void *dev_id)
{
	reg_gio_rw_intr_mask intr_mask;
	reg_gio_r_masked_intr masked_intr;
	reg_gio_rw_ack_intr ack_intr;
	unsigned long flags;
	unsigned long tmp;
	unsigned long tmp2;
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	unsigned char enable_gpiov_ack = 0;
#endif

	
	masked_intr = REG_RD(gio, regi_gio, r_masked_intr);
	tmp = REG_TYPE_CONV(unsigned long, reg_gio_r_masked_intr, masked_intr);

	
	spin_lock_irqsave(&gpio_lock, flags);
	tmp &= wanted_interrupts;
	spin_unlock_irqrestore(&gpio_lock, flags);

#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	if (tmp & (1 << CONFIG_ETRAX_VIRTUAL_GPIO_INTERRUPT_PA_PIN)) {
		i2c_read(VIRT_I2C_ADDR, (void *)&cached_virtual_gpio_read,
			sizeof(cached_virtual_gpio_read));
		enable_gpiov_ack = 1;
	}
#endif

	
	ack_intr = REG_TYPE_CONV(reg_gio_rw_ack_intr, unsigned long, tmp);
	REG_WR(gio, regi_gio, rw_ack_intr, ack_intr);

	
	intr_mask = REG_RD(gio, regi_gio, rw_intr_mask);
	tmp2 = REG_TYPE_CONV(unsigned long, reg_gio_rw_intr_mask, intr_mask);
	tmp2 &= ~tmp;
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	if (enable_gpiov_ack)
		tmp2 |= (1 << CONFIG_ETRAX_VIRTUAL_GPIO_INTERRUPT_PA_PIN);
#endif
	intr_mask = REG_TYPE_CONV(reg_gio_rw_intr_mask, unsigned long, tmp2);
	REG_WR(gio, regi_gio, rw_intr_mask, intr_mask);

	return IRQ_RETVAL(tmp);
}

static void gpio_write_bit(unsigned long *port, unsigned char data, int bit,
	unsigned char clk_mask, unsigned char data_mask)
{
	unsigned long shadow = readl(port) & ~clk_mask;
	writel(shadow, port);
	if (data & 1 << bit)
		shadow |= data_mask;
	else
		shadow &= ~data_mask;
	writel(shadow, port);
	
	shadow |= clk_mask;
	writel(shadow, port);
}

static void gpio_write_byte(struct gpio_private *priv, unsigned long *port,
		unsigned char data)
{
	int i;

	if (priv->write_msb)
		for (i = 7; i >= 0; i--)
			gpio_write_bit(port, data, i, priv->clk_mask,
				priv->data_mask);
	else
		for (i = 0; i <= 7; i++)
			gpio_write_bit(port, data, i, priv->clk_mask,
				priv->data_mask);
}


static ssize_t gpio_write(struct file *file, const char __user *buf,
	size_t count, loff_t *off)
{
	struct gpio_private *priv = file->private_data;
	unsigned long flags;
	ssize_t retval = count;
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	if (priv->minor == GPIO_MINOR_V)
		return -EFAULT;
#endif
	if (priv->minor == GPIO_MINOR_LEDS)
		return -EFAULT;

	if (priv->minor >= GPIO_MINOR_PWM0 &&
	    priv->minor <= GPIO_MINOR_LAST_PWM)
		return -EFAULT;

	if (!access_ok(VERIFY_READ, buf, count))
		return -EFAULT;

	
	
	if (priv->clk_mask == 0 || priv->data_mask == 0)
		return -EPERM;

	D(printk(KERN_DEBUG "gpio_write: %lu to data 0x%02X clk 0x%02X "
		"msb: %i\n",
		count, priv->data_mask, priv->clk_mask, priv->write_msb));

	spin_lock_irqsave(&gpio_lock, flags);

	while (count--)
		gpio_write_byte(priv, data_out[priv->minor], *buf++);

	spin_unlock_irqrestore(&gpio_lock, flags);
	return retval;
}

static int gpio_open(struct inode *inode, struct file *filp)
{
	struct gpio_private *priv;
	int p = iminor(inode);

	if (p > GPIO_MINOR_LAST_PWM ||
	    (p > GPIO_MINOR_LAST && p < GPIO_MINOR_PWM0))
		return -EINVAL;

	priv = kmalloc(sizeof(struct gpio_private), GFP_KERNEL);

	if (!priv)
		return -ENOMEM;

	mutex_lock(&gpio_mutex);
	memset(priv, 0, sizeof(*priv));

	priv->minor = p;
	filp->private_data = priv;

	
	if (p <= GPIO_MINOR_LAST) {

		priv->clk_mask = 0;
		priv->data_mask = 0;
		priv->highalarm = 0;
		priv->lowalarm = 0;

		init_waitqueue_head(&priv->alarm_wq);

		
		spin_lock_irq(&gpio_lock);
		priv->next = alarmlist;
		alarmlist = priv;
		spin_unlock_irq(&gpio_lock);
	}

	mutex_unlock(&gpio_mutex);
	return 0;
}

static int gpio_release(struct inode *inode, struct file *filp)
{
	struct gpio_private *p;
	struct gpio_private *todel;
	
	unsigned long a_high, a_low;

	
	todel = filp->private_data;

	
	if (todel->minor <= GPIO_MINOR_LAST) {
		spin_lock_irq(&gpio_lock);
		p = alarmlist;

		if (p == todel)
			alarmlist = todel->next;
		 else {
			while (p->next != todel)
				p = p->next;
			p->next = todel->next;
		}

		
		p = alarmlist;
		a_high = 0;
		a_low = 0;
		while (p) {
			if (p->minor == GPIO_MINOR_A) {
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
				p->lowalarm |= (1 << CONFIG_ETRAX_VIRTUAL_GPIO_INTERRUPT_PA_PIN);
#endif
				a_high |= p->highalarm;
				a_low |= p->lowalarm;
			}

			p = p->next;
		}

#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
		a_low |= (1 << CONFIG_ETRAX_VIRTUAL_GPIO_INTERRUPT_PA_PIN);
#endif

		spin_unlock_irq(&gpio_lock);
	}
	kfree(todel);

	return 0;
}


inline unsigned long setget_input(struct gpio_private *priv, unsigned long arg)
{
	unsigned long flags;
	unsigned long dir_shadow;

	spin_lock_irqsave(&gpio_lock, flags);

	dir_shadow = readl(dir_oe[priv->minor]) &
		~(arg & changeable_dir[priv->minor]);
	writel(dir_shadow, dir_oe[priv->minor]);

	spin_unlock_irqrestore(&gpio_lock, flags);

	if (priv->minor == GPIO_MINOR_C)
		dir_shadow ^= 0xFFFF;		
#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	else if (priv->minor == GPIO_MINOR_V)
		dir_shadow ^= 0xFFFF;		
#endif
	else
		dir_shadow ^= 0xFFFFFFFF;	

	return dir_shadow;

} 

static inline unsigned long setget_output(struct gpio_private *priv,
	unsigned long arg)
{
	unsigned long flags;
	unsigned long dir_shadow;

	spin_lock_irqsave(&gpio_lock, flags);

	dir_shadow = readl(dir_oe[priv->minor]) |
		(arg & changeable_dir[priv->minor]);
	writel(dir_shadow, dir_oe[priv->minor]);

	spin_unlock_irqrestore(&gpio_lock, flags);
	return dir_shadow;
} 

static long gpio_ioctl_unlocked(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	unsigned long flags;
	unsigned long val;
	unsigned long shadow;
	struct gpio_private *priv = file->private_data;

	if (_IOC_TYPE(cmd) != ETRAXGPIO_IOCTYPE)
		return -ENOTTY;

	

#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	if (priv->minor == GPIO_MINOR_V)
		return virtual_gpio_ioctl(file, cmd, arg);
#endif

	if (priv->minor == GPIO_MINOR_LEDS)
		return gpio_leds_ioctl(cmd, arg);

	if (priv->minor >= GPIO_MINOR_PWM0 &&
	    priv->minor <= GPIO_MINOR_LAST_PWM)
		return gpio_pwm_ioctl(priv, cmd, arg);

	switch (_IOC_NR(cmd)) {
	case IO_READBITS: 
		
		return readl(data_in[priv->minor]);
	case IO_SETBITS:
		spin_lock_irqsave(&gpio_lock, flags);
		
		shadow = readl(data_out[priv->minor]) |
			(arg & changeable_bits[priv->minor]);
		writel(shadow, data_out[priv->minor]);
		spin_unlock_irqrestore(&gpio_lock, flags);
		break;
	case IO_CLRBITS:
		spin_lock_irqsave(&gpio_lock, flags);
		
		shadow = readl(data_out[priv->minor]) &
			~(arg & changeable_bits[priv->minor]);
		writel(shadow, data_out[priv->minor]);
		spin_unlock_irqrestore(&gpio_lock, flags);
		break;
	case IO_HIGHALARM:
		
		priv->highalarm |= arg;
		gpio_set_alarm(priv);
		break;
	case IO_LOWALARM:
		
		priv->lowalarm |= arg;
		gpio_set_alarm(priv);
		break;
	case IO_CLRALARM:
		
		priv->highalarm &= ~arg;
		priv->lowalarm  &= ~arg;
		gpio_set_alarm(priv);
		break;
	case IO_READDIR: 
		
		return readl(dir_oe[priv->minor]);

	case IO_SETINPUT: 
		return setget_input(priv, arg);

	case IO_SETOUTPUT: 
		return setget_output(priv, arg);

	case IO_CFG_WRITE_MODE:
	{
		int res = -EPERM;
		unsigned long dir_shadow, clk_mask, data_mask, write_msb;

		clk_mask = arg & 0xFF;
		data_mask = (arg >> 8) & 0xFF;
		write_msb = (arg >> 16) & 0x01;

		spin_lock_irqsave(&gpio_lock, flags);
		dir_shadow = readl(dir_oe[priv->minor]);
		if ((clk_mask & changeable_bits[priv->minor]) &&
		    (data_mask & changeable_bits[priv->minor]) &&
		    (clk_mask & dir_shadow) &&
		    (data_mask & dir_shadow)) {
			priv->clk_mask = clk_mask;
			priv->data_mask = data_mask;
			priv->write_msb = write_msb;
			res = 0;
		}
		spin_unlock_irqrestore(&gpio_lock, flags);

		return res;
	}
	case IO_READ_INBITS:
		
		val = readl(data_in[priv->minor]);
		if (copy_to_user((void __user *)arg, &val, sizeof(val)))
			return -EFAULT;
		return 0;
	case IO_READ_OUTBITS:
		 
		val = *data_out[priv->minor];
		if (copy_to_user((void __user *)arg, &val, sizeof(val)))
			return -EFAULT;
		break;
	case IO_SETGET_INPUT:
		if (copy_from_user(&val, (void __user *)arg, sizeof(val)))
			return -EFAULT;
		val = setget_input(priv, val);
		if (copy_to_user((void __user *)arg, &val, sizeof(val)))
			return -EFAULT;
		break;
	case IO_SETGET_OUTPUT:
		if (copy_from_user(&val, (void __user *)arg, sizeof(val)))
			return -EFAULT;
		val = setget_output(priv, val);
		if (copy_to_user((void __user *)arg, &val, sizeof(val)))
			return -EFAULT;
		break;
	default:
		return -EINVAL;
	} 

	return 0;
}

static long gpio_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
       long ret;

       mutex_lock(&gpio_mutex);
       ret = gpio_ioctl_unlocked(file, cmd, arg);
       mutex_unlock(&gpio_mutex);

       return ret;
}

#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
static int virtual_gpio_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long flags;
	unsigned short val;
	unsigned short shadow;
	struct gpio_private *priv = file->private_data;

	switch (_IOC_NR(cmd)) {
	case IO_SETBITS:
		spin_lock_irqsave(&gpio_lock, flags);
		
		i2c_read(VIRT_I2C_ADDR, (void *)&shadow, sizeof(shadow));
		shadow |= ~readl(dir_oe[priv->minor]) |
			(arg & changeable_bits[priv->minor]);
		i2c_write(VIRT_I2C_ADDR, (void *)&shadow, sizeof(shadow));
		spin_unlock_irqrestore(&gpio_lock, flags);
		break;
	case IO_CLRBITS:
		spin_lock_irqsave(&gpio_lock, flags);
		
		i2c_read(VIRT_I2C_ADDR, (void *)&shadow, sizeof(shadow));
		shadow |= ~readl(dir_oe[priv->minor]) &
			~(arg & changeable_bits[priv->minor]);
		i2c_write(VIRT_I2C_ADDR, (void *)&shadow, sizeof(shadow));
		spin_unlock_irqrestore(&gpio_lock, flags);
		break;
	case IO_HIGHALARM:
		
		priv->highalarm |= arg;
		break;
	case IO_LOWALARM:
		
		priv->lowalarm |= arg;
		break;
	case IO_CLRALARM:
		
		priv->highalarm &= ~arg;
		priv->lowalarm  &= ~arg;
		break;
	case IO_CFG_WRITE_MODE:
	{
		unsigned long dir_shadow;
		dir_shadow = readl(dir_oe[priv->minor]);

		priv->clk_mask = arg & 0xFF;
		priv->data_mask = (arg >> 8) & 0xFF;
		priv->write_msb = (arg >> 16) & 0x01;
		if (!((priv->clk_mask & changeable_bits[priv->minor]) &&
		      (priv->data_mask & changeable_bits[priv->minor]) &&
		      (priv->clk_mask & dir_shadow) &&
		      (priv->data_mask & dir_shadow))) {
			priv->clk_mask = 0;
			priv->data_mask = 0;
			return -EPERM;
		}
		break;
	}
	case IO_READ_INBITS:
		
		val = cached_virtual_gpio_read & ~readl(dir_oe[priv->minor]);
		if (copy_to_user((void __user *)arg, &val, sizeof(val)))
			return -EFAULT;
		return 0;

	case IO_READ_OUTBITS:
		 
		i2c_read(VIRT_I2C_ADDR, (void *)&val, sizeof(val));
		val &= readl(dir_oe[priv->minor]);
		if (copy_to_user((void __user *)arg, &val, sizeof(val)))
			return -EFAULT;
		break;
	case IO_SETGET_INPUT:
	{
		unsigned short input_mask = ~readl(dir_oe[priv->minor]);
		if (copy_from_user(&val, (void __user *)arg, sizeof(val)))
			return -EFAULT;
		val = setget_input(priv, val);
		if (copy_to_user((void __user *)arg, &val, sizeof(val)))
			return -EFAULT;
		if ((input_mask & val) != input_mask) {
			unsigned short change = input_mask ^ val;
			i2c_read(VIRT_I2C_ADDR, (void *)&shadow,
				sizeof(shadow));
			shadow &= ~change;
			shadow |= val;
			i2c_write(VIRT_I2C_ADDR, (void *)&shadow,
				sizeof(shadow));
		}
		break;
	}
	case IO_SETGET_OUTPUT:
		if (copy_from_user(&val, (void __user *)arg, sizeof(val)))
			return -EFAULT;
		val = setget_output(priv, val);
		if (copy_to_user((void __user *)arg, &val, sizeof(val)))
			return -EFAULT;
		break;
	default:
		return -EINVAL;
	} 
	return 0;
}
#endif 

static int gpio_leds_ioctl(unsigned int cmd, unsigned long arg)
{
	unsigned char green;
	unsigned char red;

	switch (_IOC_NR(cmd)) {
	case IO_LEDACTIVE_SET:
		green = ((unsigned char) arg) & 1;
		red   = (((unsigned char) arg) >> 1) & 1;
		CRIS_LED_ACTIVE_SET_G(green);
		CRIS_LED_ACTIVE_SET_R(red);
		break;

	default:
		return -EINVAL;
	} 

	return 0;
}

static int gpio_pwm_set_mode(unsigned long arg, int pwm_port)
{
	int pinmux_pwm = pinmux_pwm0 + pwm_port;
	int mode;
	reg_gio_rw_pwm0_ctrl rw_pwm_ctrl = {
		.ccd_val = 0,
		.ccd_override = regk_gio_no,
		.mode = regk_gio_no
	};
	int allocstatus;

	if (get_user(mode, &((struct io_pwm_set_mode *) arg)->mode))
		return -EFAULT;
	rw_pwm_ctrl.mode = mode;
	if (mode != PWM_OFF)
		allocstatus = crisv32_pinmux_alloc_fixed(pinmux_pwm);
	else
		allocstatus = crisv32_pinmux_dealloc_fixed(pinmux_pwm);
	if (allocstatus)
		return allocstatus;
	REG_WRITE(reg_gio_rw_pwm0_ctrl, REG_ADDR(gio, regi_gio, rw_pwm0_ctrl) +
		12 * pwm_port, rw_pwm_ctrl);
	return 0;
}

static int gpio_pwm_set_period(unsigned long arg, int pwm_port)
{
	struct io_pwm_set_period periods;
	reg_gio_rw_pwm0_var rw_pwm_widths;

	if (copy_from_user(&periods, (void __user *)arg, sizeof(periods)))
		return -EFAULT;
	if (periods.lo > 8191 || periods.hi > 8191)
		return -EINVAL;
	rw_pwm_widths.lo = periods.lo;
	rw_pwm_widths.hi = periods.hi;
	REG_WRITE(reg_gio_rw_pwm0_var, REG_ADDR(gio, regi_gio, rw_pwm0_var) +
		12 * pwm_port, rw_pwm_widths);
	return 0;
}

static int gpio_pwm_set_duty(unsigned long arg, int pwm_port)
{
	unsigned int duty;
	reg_gio_rw_pwm0_data rw_pwm_duty;

	if (get_user(duty, &((struct io_pwm_set_duty *) arg)->duty))
		return -EFAULT;
	if (duty > 255)
		return -EINVAL;
	rw_pwm_duty.data = duty;
	REG_WRITE(reg_gio_rw_pwm0_data, REG_ADDR(gio, regi_gio, rw_pwm0_data) +
		12 * pwm_port, rw_pwm_duty);
	return 0;
}

static int gpio_pwm_ioctl(struct gpio_private *priv, unsigned int cmd,
	unsigned long arg)
{
	int pwm_port = priv->minor - GPIO_MINOR_PWM0;

	switch (_IOC_NR(cmd)) {
	case IO_PWM_SET_MODE:
		return gpio_pwm_set_mode(arg, pwm_port);
	case IO_PWM_SET_PERIOD:
		return gpio_pwm_set_period(arg, pwm_port);
	case IO_PWM_SET_DUTY:
		return gpio_pwm_set_duty(arg, pwm_port);
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct file_operations gpio_fops = {
	.owner		= THIS_MODULE,
	.poll		= gpio_poll,
	.unlocked_ioctl	= gpio_ioctl,
	.write		= gpio_write,
	.open		= gpio_open,
	.release	= gpio_release,
	.llseek		= noop_llseek,
};

#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
static void __init virtual_gpio_init(void)
{
	reg_gio_rw_intr_cfg intr_cfg;
	reg_gio_rw_intr_mask intr_mask;
	unsigned short shadow;

	shadow = ~virtual_rw_pv_oe; 
	shadow |= CONFIG_ETRAX_DEF_GIO_PV_OUT;
	i2c_write(VIRT_I2C_ADDR, (void *)&shadow, sizeof(shadow));

	intr_cfg = REG_RD(gio, regi_gio, rw_intr_cfg);
	intr_mask = REG_RD(gio, regi_gio, rw_intr_mask);

	switch (CONFIG_ETRAX_VIRTUAL_GPIO_INTERRUPT_PA_PIN) {
	case 0:
		intr_cfg.pa0 = regk_gio_lo;
		intr_mask.pa0 = regk_gio_yes;
		break;
	case 1:
		intr_cfg.pa1 = regk_gio_lo;
		intr_mask.pa1 = regk_gio_yes;
		break;
	case 2:
		intr_cfg.pa2 = regk_gio_lo;
		intr_mask.pa2 = regk_gio_yes;
		break;
	case 3:
		intr_cfg.pa3 = regk_gio_lo;
		intr_mask.pa3 = regk_gio_yes;
		break;
	case 4:
		intr_cfg.pa4 = regk_gio_lo;
		intr_mask.pa4 = regk_gio_yes;
		break;
	case 5:
		intr_cfg.pa5 = regk_gio_lo;
		intr_mask.pa5 = regk_gio_yes;
		break;
	case 6:
		intr_cfg.pa6 = regk_gio_lo;
		intr_mask.pa6 = regk_gio_yes;
		break;
	case 7:
		intr_cfg.pa7 = regk_gio_lo;
		intr_mask.pa7 = regk_gio_yes;
		break;
	}

	REG_WR(gio, regi_gio, rw_intr_cfg, intr_cfg);
	REG_WR(gio, regi_gio, rw_intr_mask, intr_mask);
}
#endif


static int __init gpio_init(void)
{
	int res;

	printk(KERN_INFO "ETRAX FS GPIO driver v2.7, (c) 2003-2008 "
		"Axis Communications AB\n");

	

	res = register_chrdev(GPIO_MAJOR, gpio_name, &gpio_fops);
	if (res < 0) {
		printk(KERN_ERR "gpio: couldn't get a major number.\n");
		return res;
	}

	
	CRIS_LED_NETWORK_GRP0_SET(0);
	CRIS_LED_NETWORK_GRP1_SET(0);
	CRIS_LED_ACTIVE_SET(0);
	CRIS_LED_DISK_READ(0);
	CRIS_LED_DISK_WRITE(0);

	int res2 = request_irq(GIO_INTR_VECT, gpio_interrupt,
		IRQF_SHARED | IRQF_DISABLED, "gpio", &alarmlist);
	if (res2) {
		printk(KERN_ERR "err: irq for gpio\n");
		return res2;
	}

	
	REG_WR_INT(gio, regi_gio, rw_intr_pins, 0);

#ifdef CONFIG_ETRAX_VIRTUAL_GPIO
	virtual_gpio_init();
#endif

	return res;
}


module_init(gpio_init);
