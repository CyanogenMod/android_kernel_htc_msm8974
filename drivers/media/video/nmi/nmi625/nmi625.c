/*****************************************************************************
 Copyright(c) 2010 NMI Inc. All Rights Reserved
 
 File name : nmi_hw.c
 
 Description : NM625 host interface
 
 History : 
 ----------------------------------------------------------------------
 2010/05/17 	ssw		initial
*******************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <mach/gpio.h>

#include "nmi625.h"
#include "nmi625-i2c.h"

#define NMI_NAME		"dmb"
#define NMI625_HW_CHIP_ID_CHECK

int i2c_init(void);

int dmb_open (struct inode *inode, struct file *filp);
long dmb_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
int dmb_release (struct inode *inode, struct file *filp);

void dmb_hw_setting(void);
void dmb_hw_init(void);
void dmb_hw_deinit(void);
void dmb_hw_rst(unsigned long, unsigned long);


extern unsigned int HWREV;


static struct file_operations dmb_fops = 
{
	.owner		= THIS_MODULE,
	.unlocked_ioctl		= dmb_ioctl,
	.open		= dmb_open,
	.release	= dmb_release,
};

static struct miscdevice nmi_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = NMI_NAME,
    .fops = &dmb_fops,
};

int dmb_open (struct inode *inode, struct file *filp)
{
	DMB_OPEN_INFO_T *hOpen = NULL;

	printk("dmb open\n");

	hOpen = (DMB_OPEN_INFO_T *)kmalloc(sizeof(DMB_OPEN_INFO_T), GFP_KERNEL);
	filp->private_data = hOpen;

	oneseg_power(1);
	nmi625_i2c_init();

	return 0;
}

int dmb_release (struct inode *inode, struct file *filp)
{

	printk("dmb release \n");

    	nmi625_i2c_deinit();
    	oneseg_power(0);
	return 0;
}

long dmb_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	long res = 1;

	ioctl_info info;

	int	err = 0, size;

	if(_IOC_TYPE(cmd) != IOCTL_MAGIC) return -EINVAL;
	if(_IOC_NR(cmd) >= IOCTL_MAXNR) return -EINVAL;

	size = _IOC_SIZE(cmd);

	if(size) {
		if(_IOC_DIR(cmd) & _IOC_READ)
			err = access_ok(VERIFY_WRITE, (void *) arg, size);
		else if(_IOC_DIR(cmd) & _IOC_WRITE)
			err = access_ok(VERIFY_READ, (void *) arg, size);
		if(!err) {
			printk("%s : Wrong argument! cmd(0x%08x) _IOC_NR(%d) _IOC_TYPE(0x%x) _IOC_SIZE(%d) _IOC_DIR(0x%x)\n", 
			__FUNCTION__, cmd, _IOC_NR(cmd), _IOC_TYPE(cmd), _IOC_SIZE(cmd), _IOC_DIR(cmd));
			return err;
		}
	}

	switch(cmd) 
	{
		case IOCTL_DMB_RESET:
			if (copy_from_user(&info, (void *)arg, size))
				return -EFAULT;
			
			
			printk("DMB_RESET enter.., info.buf[0] %d, info.buf[1] %d\n", info.buf[0], info.buf[1]);
			dmb_hw_rst(info.buf[0], info.buf[1]);
			break;
		case IOCTL_DMB_INIT:
			nmi625_i2c_init();
			break;
		case IOCTL_DMB_BYTE_READ:
		
			break;
		case IOCTL_DMB_WORD_READ:
			
			break;
		case IOCTL_DMB_LONG_READ:
			
			break;
		case IOCTL_DMB_BULK_READ:
			if (copy_from_user(&info, (void *)arg, size))
				return -EFAULT;
			res = nmi625_i2c_read(0, (u16)info.dev_addr, (u8 *)(info.buf), info.size);
			if (copy_to_user((void *)arg, (void *)&info, size))
				return -EFAULT;
			break;
		case IOCTL_DMB_BYTE_WRITE:
			
			
			break;
		case IOCTL_DMB_WORD_WRITE:
			
			break;
		case IOCTL_DMB_LONG_WRITE:
			
			break;
		case IOCTL_DMB_BULK_WRITE:
			if (copy_from_user(&info, (void *)arg, size))
				return -EFAULT;
			nmi625_i2c_write(0, (u16)info.dev_addr, (u8 *)(info.buf), info.size);
			break;
		case IOCTL_DMB_TUNER_SELECT:
			
			break;
		case IOCTL_DMB_TUNER_READ:
			
			break;
		case IOCTL_DMB_TUNER_WRITE:
			
			break;
		case IOCTL_DMB_TUNER_SET_FREQ:
			
			break;
		case IOCTL_DMB_POWER_ON:
			printk("DMB_POWER_ON enter..\n");
			dmb_hw_init();
			break;
		case IOCTL_DMB_POWER_OFF:
			printk("DMB_POWER_OFF enter..\n");
			
			dmb_hw_deinit();
			break;
		default:
			printk("dmb_ioctl : Undefined ioctl command\n");
			res = 1;
			break;
	}
	return res;
}


void dmb_hw_setting(void)
{



}


void dmb_hw_setting_gpio_free(void)
{
    
}
	
void dmb_hw_init(void)
{

}

void dmb_hw_rst(unsigned long _arg1, unsigned long _arg2)
{
	
	
}

void dmb_hw_deinit(void)
{

}

int dmb_init(void)
{
	int result;

	printk("dmb dmb_init \n");

	
	result = misc_register(&nmi_misc_device);
        if (result) {
                printk(KERN_ERR "[ISDB-T]: can't regist nmi device\n");
                return result;
        }

	
	
	

	return 0;
}

void dmb_exit(void)
{
	printk("dmb dmb_exit \n");



	
	misc_deregister(&nmi_misc_device);
}

module_init(dmb_init);
module_exit(dmb_exit);

MODULE_LICENSE("Dual BSD/GPL");

