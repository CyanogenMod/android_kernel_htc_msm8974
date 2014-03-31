/* envctrl.c: Temperature and Fan monitoring on Machines providing it.
 *
 * Copyright (C) 1998  Eddie C. Dost  (ecd@skynet.be)
 * Copyright (C) 2000  Vinh Truong    (vinh.truong@eng.sun.com)
 * VT - The implementation is to support Sun Microelectronics (SME) platform
 *      environment monitoring.  SME platforms use pcf8584 as the i2c bus 
 *      controller to access pcf8591 (8-bit A/D and D/A converter) and 
 *      pcf8571 (256 x 8-bit static low-voltage RAM with I2C-bus interface).
 *      At board level, it follows SME Firmware I2C Specification. Reference:
 * 	http://www-eu2.semiconductors.com/pip/PCF8584P
 * 	http://www-eu2.semiconductors.com/pip/PCF8574AP
 * 	http://www-eu2.semiconductors.com/pip/PCF8591P
 *
 * EB - Added support for CP1500 Global Address and PS/Voltage monitoring.
 * 		Eric Brower <ebrower@usa.net>
 *
 * DB - Audit every copy_to_user in envctrl_read.
 *              Daniele Bellucci <bellucda@tiscali.it>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/miscdevice.h>
#include <linux/kmod.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <asm/uaccess.h>
#include <asm/envctrl.h>
#include <asm/io.h>

#define DRIVER_NAME	"envctrl"
#define PFX		DRIVER_NAME ": "

#define ENVCTRL_MINOR	162

#define PCF8584_ADDRESS	0x55

#define CONTROL_PIN	0x80
#define CONTROL_ES0	0x40
#define CONTROL_ES1	0x20
#define CONTROL_ES2	0x10
#define CONTROL_ENI	0x08
#define CONTROL_STA	0x04
#define CONTROL_STO	0x02
#define CONTROL_ACK	0x01

#define STATUS_PIN	0x80
#define STATUS_STS	0x20
#define STATUS_BER	0x10
#define STATUS_LRB	0x08
#define STATUS_AD0	0x08
#define STATUS_AAB	0x04
#define STATUS_LAB	0x02
#define STATUS_BB	0x01

#define BUS_CLK_90	0x00
#define BUS_CLK_45	0x01
#define BUS_CLK_11	0x02
#define BUS_CLK_1_5	0x03

#define CLK_3		0x00
#define CLK_4_43	0x10
#define CLK_6		0x14
#define CLK_8		0x18
#define CLK_12		0x1c

#define OBD_SEND_START	0xc5    
#define OBD_SEND_STOP 	0xc3    

#define PCF8584_MAX_CHANNELS            8
#define PCF8584_GLOBALADDR_TYPE			6  
#define PCF8584_FANSTAT_TYPE            3  
#define PCF8584_VOLTAGE_TYPE            2  
#define PCF8584_TEMP_TYPE	        	1  

#define ENVCTRL_NOMON				0
#define ENVCTRL_CPUTEMP_MON			1    
#define ENVCTRL_CPUVOLTAGE_MON	  	2    
#define ENVCTRL_FANSTAT_MON  		3    
#define ENVCTRL_ETHERTEMP_MON		4    
					     
#define ENVCTRL_VOLTAGESTAT_MON	  	5    
#define ENVCTRL_MTHRBDTEMP_MON		6    
#define ENVCTRL_SCSITEMP_MON		7    
#define ENVCTRL_GLOBALADDR_MON		8    

#define I2C_ADC				0    
#define I2C_GPIO			1    

#define ENVCTRL_TRANSLATE_NO		0
#define ENVCTRL_TRANSLATE_PARTIAL	1
#define ENVCTRL_TRANSLATE_COMBINED	2
#define ENVCTRL_TRANSLATE_FULL		3     
#define ENVCTRL_TRANSLATE_SCALE		4     

#define ENVCTRL_MAX_CPU			4
#define CHANNEL_DESC_SZ			256

#define ENVCTRL_GLOBALADDR_ADDR_MASK 	0x1F
#define ENVCTRL_GLOBALADDR_PSTAT_MASK	0x60

#define ENVCTRL_CPCI_IGNORED_NODE		0x70

#define PCF8584_DATA	0x00
#define PCF8584_CSR	0x01

struct pcf8584_channel {
        unsigned char chnl_no;
        unsigned char io_direction;
        unsigned char type;
        unsigned char last;
};

 
struct pcf8584_tblprop {
        unsigned int type;
        unsigned int scale;  
        unsigned int offset; 
        unsigned int size;
};

struct i2c_child_t {
	
	unsigned char i2ctype;
        unsigned long addr;    
        struct pcf8584_channel chnl_array[PCF8584_MAX_CHANNELS];

	 
	unsigned int total_chnls;	
	unsigned char fan_mask;		
	unsigned char voltage_mask;	
        struct pcf8584_tblprop tblprop_array[PCF8584_MAX_CHANNELS];

	
	unsigned int total_tbls;	
        char *tables;			
	char chnls_desc[CHANNEL_DESC_SZ]; 
	char mon_type[PCF8584_MAX_CHANNELS];
};

static void __iomem *i2c;
static struct i2c_child_t i2c_childlist[ENVCTRL_MAX_CPU*2];
static unsigned char chnls_mask[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
static unsigned int warning_temperature = 0;
static unsigned int shutdown_temperature = 0;
static char read_cpu;

static struct i2c_child_t *envctrl_get_i2c_child(unsigned char);

static void envtrl_i2c_test_pin(void)
{
	int limit = 1000000;

	while (--limit > 0) {
		if (!(readb(i2c + PCF8584_CSR) & STATUS_PIN)) 
			break;
		udelay(1);
	} 

	if (limit <= 0)
		printk(KERN_INFO PFX "Pin status will not clear.\n");
}

static void envctrl_i2c_test_bb(void)
{
	int limit = 1000000;

	while (--limit > 0) {
		
		if (readb(i2c + PCF8584_CSR) & STATUS_BB)
			break;
		udelay(1);
	} 

	if (limit <= 0)
		printk(KERN_INFO PFX "Busy bit will not clear.\n");
}

static int envctrl_i2c_read_addr(unsigned char addr)
{
	envctrl_i2c_test_bb();

	
	writeb(addr + 1, i2c + PCF8584_DATA);

	envctrl_i2c_test_bb();

	writeb(OBD_SEND_START, i2c + PCF8584_CSR);

	
	envtrl_i2c_test_pin();

	
	if (!(readb(i2c + PCF8584_CSR) & STATUS_LRB)) {
		return readb(i2c + PCF8584_DATA);
	} else {
		writeb(OBD_SEND_STOP, i2c + PCF8584_CSR);
		return 0;
	}
}

static void envctrl_i2c_write_addr(unsigned char addr)
{
	envctrl_i2c_test_bb();
	writeb(addr, i2c + PCF8584_DATA);

	
	writeb(OBD_SEND_START, i2c + PCF8584_CSR);
}

static unsigned char envctrl_i2c_read_data(void)
{
	envtrl_i2c_test_pin();
	writeb(CONTROL_ES0, i2c + PCF8584_CSR);  
	return readb(i2c + PCF8584_DATA);
}

static void envctrl_i2c_write_data(unsigned char port)
{
	envtrl_i2c_test_pin();
	writeb(port, i2c + PCF8584_DATA);
}

static void envctrl_i2c_stop(void)
{
	envtrl_i2c_test_pin();
	writeb(OBD_SEND_STOP, i2c + PCF8584_CSR);
}

static unsigned char envctrl_i2c_read_8591(unsigned char addr, unsigned char port)
{
	
	envctrl_i2c_write_addr(addr);

	
	envctrl_i2c_write_data(port);
	envctrl_i2c_stop();

	
	envctrl_i2c_read_addr(addr);

	
	envctrl_i2c_read_data();
	envctrl_i2c_stop();

	return readb(i2c + PCF8584_DATA);
}

static unsigned char envctrl_i2c_read_8574(unsigned char addr)
{
	unsigned char rd;

	envctrl_i2c_read_addr(addr);

	
	rd = envctrl_i2c_read_data();
	envctrl_i2c_stop();
	return rd;
}

static int envctrl_i2c_data_translate(unsigned char data, int translate_type,
				      int scale, char *tbl, char *bufdata)
{
	int len = 0;

	switch (translate_type) {
	case ENVCTRL_TRANSLATE_NO:
		
		len = 1;
		bufdata[0] = data;
		break;

	case ENVCTRL_TRANSLATE_FULL:
		
		len = 1;
		bufdata[0] = tbl[data];
		break;

	case ENVCTRL_TRANSLATE_SCALE:
		
		sprintf(bufdata,"%d ", (tbl[data] * 10) / (scale));
		len = strlen(bufdata);
		bufdata[len - 1] = bufdata[len - 2];
		bufdata[len - 2] = '.';
		break;

	default:
		break;
	};

	return len;
}

static int envctrl_read_cpu_info(int cpu, struct i2c_child_t *pchild,
				 char mon_type, unsigned char *bufdata)
{
	unsigned char data;
	int i;
	char *tbl, j = -1;

	
	for (i = 0; i < PCF8584_MAX_CHANNELS; i++) {
		if (pchild->mon_type[i] == mon_type) {
			if (++j == cpu) {
				break;
			}
		}
	}

	if (j != cpu)
		return 0;

        
	data = envctrl_i2c_read_8591((unsigned char)pchild->addr,
				     (unsigned char)pchild->chnl_array[i].chnl_no);

	
	tbl = pchild->tables + pchild->tblprop_array[i].offset;

	return envctrl_i2c_data_translate(data, pchild->tblprop_array[i].type,
					  pchild->tblprop_array[i].scale,
					  tbl, bufdata);
}

static int envctrl_read_noncpu_info(struct i2c_child_t *pchild,
				    char mon_type, unsigned char *bufdata)
{
	unsigned char data;
	int i;
	char *tbl = NULL;

	for (i = 0; i < PCF8584_MAX_CHANNELS; i++) {
		if (pchild->mon_type[i] == mon_type)
			break;
	}

	if (i >= PCF8584_MAX_CHANNELS)
		return 0;

        
	data = envctrl_i2c_read_8591((unsigned char)pchild->addr,
				     (unsigned char)pchild->chnl_array[i].chnl_no);

	
	tbl = pchild->tables + pchild->tblprop_array[i].offset;

	return envctrl_i2c_data_translate(data, pchild->tblprop_array[i].type,
					  pchild->tblprop_array[i].scale,
					  tbl, bufdata);
}

static int envctrl_i2c_fan_status(struct i2c_child_t *pchild,
				  unsigned char data,
				  char *bufdata)
{
	unsigned char tmp, ret = 0;
	int i, j = 0;

	tmp = data & pchild->fan_mask;

	if (tmp == pchild->fan_mask) {
		
		ret = ENVCTRL_ALL_FANS_GOOD;
	} else if (tmp == 0) {
		
		ret = ENVCTRL_ALL_FANS_BAD;
	} else {
		for (i = 0; i < PCF8584_MAX_CHANNELS;i++) {
			if (pchild->fan_mask & chnls_mask[i]) {
				if (!(chnls_mask[i] & tmp))
					ret |= chnls_mask[j];

				j++;
			}
		}
	}

	bufdata[0] = ret;
	return 1;
}

static int envctrl_i2c_globaladdr(struct i2c_child_t *pchild,
				  unsigned char data,
				  char *bufdata)
{
	bufdata[0] = (data & ENVCTRL_GLOBALADDR_ADDR_MASK);
	return 1;
}

static unsigned char envctrl_i2c_voltage_status(struct i2c_child_t *pchild,
						unsigned char data,
						char *bufdata)
{
	unsigned char tmp, ret = 0;
	int i, j = 0;

	tmp = data & pchild->voltage_mask;

	
	if (tmp == pchild->voltage_mask) {
		
		ret = ENVCTRL_VOLTAGE_POWERSUPPLY_GOOD;
	} else if (tmp == 0) {
		
		ret = ENVCTRL_VOLTAGE_POWERSUPPLY_BAD;
	} else {
		
		for (i = 0; i < PCF8584_MAX_CHANNELS; i++) {
			if (pchild->voltage_mask & chnls_mask[i]) {
				j++;

				
				if (!(chnls_mask[i] & tmp))
					break; 
			}
		}

		if (j == 1)
			ret = ENVCTRL_VOLTAGE_BAD;
		else
			ret = ENVCTRL_POWERSUPPLY_BAD;
	}

	bufdata[0] = ret;
	return 1;
}

static ssize_t
envctrl_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct i2c_child_t *pchild;
	unsigned char data[10];
	int ret = 0;


	switch ((int)(long)file->private_data) {
	case ENVCTRL_RD_WARNING_TEMPERATURE:
		if (warning_temperature == 0)
			return 0;

		data[0] = (unsigned char)(warning_temperature);
		ret = 1;
		if (copy_to_user(buf, data, ret))
			ret = -EFAULT;
		break;

	case ENVCTRL_RD_SHUTDOWN_TEMPERATURE:
		if (shutdown_temperature == 0)
			return 0;

		data[0] = (unsigned char)(shutdown_temperature);
		ret = 1;
		if (copy_to_user(buf, data, ret))
			ret = -EFAULT;
		break;

	case ENVCTRL_RD_MTHRBD_TEMPERATURE:
		if (!(pchild = envctrl_get_i2c_child(ENVCTRL_MTHRBDTEMP_MON)))
			return 0;
		ret = envctrl_read_noncpu_info(pchild, ENVCTRL_MTHRBDTEMP_MON, data);
		if (copy_to_user(buf, data, ret))
			ret = -EFAULT;
		break;

	case ENVCTRL_RD_CPU_TEMPERATURE:
		if (!(pchild = envctrl_get_i2c_child(ENVCTRL_CPUTEMP_MON)))
			return 0;
		ret = envctrl_read_cpu_info(read_cpu, pchild, ENVCTRL_CPUTEMP_MON, data);

		
		if (copy_to_user(buf, data, ret))
			ret = -EFAULT;
		break;

	case ENVCTRL_RD_CPU_VOLTAGE:
		if (!(pchild = envctrl_get_i2c_child(ENVCTRL_CPUVOLTAGE_MON)))
			return 0;
		ret = envctrl_read_cpu_info(read_cpu, pchild, ENVCTRL_CPUVOLTAGE_MON, data);

		
		if (copy_to_user(buf, data, ret))
			ret = -EFAULT;
		break;

	case ENVCTRL_RD_SCSI_TEMPERATURE:
		if (!(pchild = envctrl_get_i2c_child(ENVCTRL_SCSITEMP_MON)))
			return 0;
		ret = envctrl_read_noncpu_info(pchild, ENVCTRL_SCSITEMP_MON, data);
		if (copy_to_user(buf, data, ret))
			ret = -EFAULT;
		break;

	case ENVCTRL_RD_ETHERNET_TEMPERATURE:
		if (!(pchild = envctrl_get_i2c_child(ENVCTRL_ETHERTEMP_MON)))
			return 0;
		ret = envctrl_read_noncpu_info(pchild, ENVCTRL_ETHERTEMP_MON, data);
		if (copy_to_user(buf, data, ret))
			ret = -EFAULT;
		break;

	case ENVCTRL_RD_FAN_STATUS:
		if (!(pchild = envctrl_get_i2c_child(ENVCTRL_FANSTAT_MON)))
			return 0;
		data[0] = envctrl_i2c_read_8574(pchild->addr);
		ret = envctrl_i2c_fan_status(pchild,data[0], data);
		if (copy_to_user(buf, data, ret))
			ret = -EFAULT;
		break;
	
	case ENVCTRL_RD_GLOBALADDRESS:
		if (!(pchild = envctrl_get_i2c_child(ENVCTRL_GLOBALADDR_MON)))
			return 0;
		data[0] = envctrl_i2c_read_8574(pchild->addr);
		ret = envctrl_i2c_globaladdr(pchild, data[0], data);
		if (copy_to_user(buf, data, ret))
			ret = -EFAULT;
		break;

	case ENVCTRL_RD_VOLTAGE_STATUS:
		if (!(pchild = envctrl_get_i2c_child(ENVCTRL_VOLTAGESTAT_MON)))
			
			if (!(pchild = envctrl_get_i2c_child(ENVCTRL_GLOBALADDR_MON)))
				return 0;
		data[0] = envctrl_i2c_read_8574(pchild->addr);
		ret = envctrl_i2c_voltage_status(pchild, data[0], data);
		if (copy_to_user(buf, data, ret))
			ret = -EFAULT;
		break;

	default:
		break;

	};

	return ret;
}

static long
envctrl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	char __user *infobuf;

	switch (cmd) {
	case ENVCTRL_RD_WARNING_TEMPERATURE:
	case ENVCTRL_RD_SHUTDOWN_TEMPERATURE:
	case ENVCTRL_RD_MTHRBD_TEMPERATURE:
	case ENVCTRL_RD_FAN_STATUS:
	case ENVCTRL_RD_VOLTAGE_STATUS:
	case ENVCTRL_RD_ETHERNET_TEMPERATURE:
	case ENVCTRL_RD_SCSI_TEMPERATURE:
	case ENVCTRL_RD_GLOBALADDRESS:
		file->private_data = (void *)(long)cmd;
		break;

	case ENVCTRL_RD_CPU_TEMPERATURE:
	case ENVCTRL_RD_CPU_VOLTAGE:
		infobuf = (char __user *) arg;
		if (infobuf == NULL) {
			read_cpu = 0;
		}else {
			get_user(read_cpu, infobuf);
		}

		
		file->private_data = (void *)(long)cmd;
		break;

	default:
		return -EINVAL;
	};

	return 0;
}

static int
envctrl_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static int
envctrl_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations envctrl_fops = {
	.owner =		THIS_MODULE,
	.read =			envctrl_read,
	.unlocked_ioctl =	envctrl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl =		envctrl_ioctl,
#endif
	.open =			envctrl_open,
	.release =		envctrl_release,
	.llseek =		noop_llseek,
};	

static struct miscdevice envctrl_dev = {
	ENVCTRL_MINOR,
	"envctrl",
	&envctrl_fops
};

static void envctrl_set_mon(struct i2c_child_t *pchild,
			    const char *chnl_desc,
			    int chnl_no)
{
	if (!(strcmp(chnl_desc,"temp,cpu")) ||
	    !(strcmp(chnl_desc,"temp,cpu0")) ||
	    !(strcmp(chnl_desc,"temp,cpu1")) ||
	    !(strcmp(chnl_desc,"temp,cpu2")) ||
	    !(strcmp(chnl_desc,"temp,cpu3")))
		pchild->mon_type[chnl_no] = ENVCTRL_CPUTEMP_MON;

	if (!(strcmp(chnl_desc,"vddcore,cpu0")) ||
	    !(strcmp(chnl_desc,"vddcore,cpu1")) ||
	    !(strcmp(chnl_desc,"vddcore,cpu2")) ||
	    !(strcmp(chnl_desc,"vddcore,cpu3")))
		pchild->mon_type[chnl_no] = ENVCTRL_CPUVOLTAGE_MON;

	if (!(strcmp(chnl_desc,"temp,motherboard")))
		pchild->mon_type[chnl_no] = ENVCTRL_MTHRBDTEMP_MON;

	if (!(strcmp(chnl_desc,"temp,scsi")))
		pchild->mon_type[chnl_no] = ENVCTRL_SCSITEMP_MON;

	if (!(strcmp(chnl_desc,"temp,ethernet")))
		pchild->mon_type[chnl_no] = ENVCTRL_ETHERTEMP_MON;
}

static void envctrl_init_adc(struct i2c_child_t *pchild, struct device_node *dp)
{
	int i = 0, len;
	const char *pos;
	const unsigned int *pval;

	
	pos = of_get_property(dp, "channels-description", &len);

	while (len > 0) {
		int l = strlen(pos) + 1;
		envctrl_set_mon(pchild, pos, i++);
		len -= l;
		pos += l;
	}

	
	pval = of_get_property(dp, "warning-temp", NULL);
	if (pval)
		warning_temperature = *pval;

	pval = of_get_property(dp, "shutdown-temp", NULL);
	if (pval)
		shutdown_temperature = *pval;
}

static void envctrl_init_fanstat(struct i2c_child_t *pchild)
{
	int i;

	
	for (i = 0; i < pchild->total_chnls; i++)
		pchild->fan_mask |= chnls_mask[(pchild->chnl_array[i]).chnl_no];

	pchild->mon_type[0] = ENVCTRL_FANSTAT_MON;
}

static void envctrl_init_globaladdr(struct i2c_child_t *pchild)
{
	int i;

	for (i = 0; i < pchild->total_chnls; i++) {
		if (PCF8584_VOLTAGE_TYPE == pchild->chnl_array[i].type) {
			pchild->voltage_mask |= chnls_mask[i];
		}
	}

	pchild->mon_type[0] = ENVCTRL_GLOBALADDR_MON;
}

static void envctrl_init_voltage_status(struct i2c_child_t *pchild)
{
	int i;

	
	for (i = 0; i < pchild->total_chnls; i++)
		pchild->voltage_mask |= chnls_mask[(pchild->chnl_array[i]).chnl_no];

	pchild->mon_type[0] = ENVCTRL_VOLTAGESTAT_MON;
}

static void envctrl_init_i2c_child(struct device_node *dp,
				   struct i2c_child_t *pchild)
{
	int len, i, tbls_size = 0;
	const void *pval;

	
	pval = of_get_property(dp, "reg", &len);
	memcpy(&pchild->addr, pval, len);

	
	pval = of_get_property(dp, "translation", &len);
	if (pval && len > 0) {
		memcpy(pchild->tblprop_array, pval, len);
                pchild->total_tbls = len / sizeof(struct pcf8584_tblprop);
		for (i = 0; i < pchild->total_tbls; i++) {
			if ((pchild->tblprop_array[i].size + pchild->tblprop_array[i].offset) > tbls_size) {
				tbls_size = pchild->tblprop_array[i].size + pchild->tblprop_array[i].offset;
			}
		}

                pchild->tables = kmalloc(tbls_size, GFP_KERNEL);
		if (pchild->tables == NULL){
			printk(KERN_ERR PFX "Failed to allocate table.\n");
			return;
		}
		pval = of_get_property(dp, "tables", &len);
                if (!pval || len <= 0) {
			printk(KERN_ERR PFX "Failed to get table.\n");
			return;
		}
		memcpy(pchild->tables, pval, len);
	}

	if (ENVCTRL_CPCI_IGNORED_NODE == pchild->addr) {
		struct device_node *root_node;
		int len;

		root_node = of_find_node_by_path("/");
		if (!strcmp(root_node->name, "SUNW,UltraSPARC-IIi-cEngine")) {
			for (len = 0; len < PCF8584_MAX_CHANNELS; ++len) {
				pchild->mon_type[len] = ENVCTRL_NOMON;
			}
			return;
		}
	}

	
	pval = of_get_property(dp, "channels-in-use", &len);
	memcpy(pchild->chnl_array, pval, len);
	pchild->total_chnls = len / sizeof(struct pcf8584_channel);

	for (i = 0; i < pchild->total_chnls; i++) {
		switch (pchild->chnl_array[i].type) {
		case PCF8584_TEMP_TYPE:
			envctrl_init_adc(pchild, dp);
			break;

		case PCF8584_GLOBALADDR_TYPE:
			envctrl_init_globaladdr(pchild);
			i = pchild->total_chnls;
			break;

		case PCF8584_FANSTAT_TYPE:
			envctrl_init_fanstat(pchild);
			i = pchild->total_chnls;
			break;

		case PCF8584_VOLTAGE_TYPE:
			if (pchild->i2ctype == I2C_ADC) {
				envctrl_init_adc(pchild,dp);
			} else {
				envctrl_init_voltage_status(pchild);
			}
			i = pchild->total_chnls;
			break;

		default:
			break;
		};
	}
}

static struct i2c_child_t *envctrl_get_i2c_child(unsigned char mon_type)
{
	int i, j;

	for (i = 0; i < ENVCTRL_MAX_CPU*2; i++) {
		for (j = 0; j < PCF8584_MAX_CHANNELS; j++) {
			if (i2c_childlist[i].mon_type[j] == mon_type) {
				return (struct i2c_child_t *)(&(i2c_childlist[i]));
			}
		}
	}
	return NULL;
}

static void envctrl_do_shutdown(void)
{
	static int inprog = 0;
	int ret;

	if (inprog != 0)
		return;

	inprog = 1;
	printk(KERN_CRIT "kenvctrld: WARNING: Shutting down the system now.\n");
	ret = orderly_poweroff(true);
	if (ret < 0) {
		printk(KERN_CRIT "kenvctrld: WARNING: system shutdown failed!\n"); 
		inprog = 0;  
	}
}

static struct task_struct *kenvctrld_task;

static int kenvctrld(void *__unused)
{
	int poll_interval;
	int whichcpu;
	char tempbuf[10];
	struct i2c_child_t *cputemp;

	if (NULL == (cputemp = envctrl_get_i2c_child(ENVCTRL_CPUTEMP_MON))) {
		printk(KERN_ERR  PFX
		       "kenvctrld unable to monitor CPU temp-- exiting\n");
		return -ENODEV;
	}

	poll_interval = 5000; 

	printk(KERN_INFO PFX "%s starting...\n", current->comm);
	for (;;) {
		msleep_interruptible(poll_interval);

		if (kthread_should_stop())
			break;
		
		for (whichcpu = 0; whichcpu < ENVCTRL_MAX_CPU; ++whichcpu) {
			if (0 < envctrl_read_cpu_info(whichcpu, cputemp,
						      ENVCTRL_CPUTEMP_MON,
						      tempbuf)) {
				if (tempbuf[0] >= shutdown_temperature) {
					printk(KERN_CRIT 
						"%s: WARNING: CPU%i temperature %i C meets or exceeds "\
						"shutdown threshold %i C\n", 
						current->comm, whichcpu, 
						tempbuf[0], shutdown_temperature);
					envctrl_do_shutdown();
				}
			}
		}
	}
	printk(KERN_INFO PFX "%s exiting...\n", current->comm);
	return 0;
}

static int __devinit envctrl_probe(struct platform_device *op)
{
	struct device_node *dp;
	int index, err;

	if (i2c)
		return -EINVAL;

	i2c = of_ioremap(&op->resource[0], 0, 0x2, DRIVER_NAME);
	if (!i2c)
		return -ENOMEM;

	index = 0;
	dp = op->dev.of_node->child;
	while (dp) {
		if (!strcmp(dp->name, "gpio")) {
			i2c_childlist[index].i2ctype = I2C_GPIO;
			envctrl_init_i2c_child(dp, &(i2c_childlist[index++]));
		} else if (!strcmp(dp->name, "adc")) {
			i2c_childlist[index].i2ctype = I2C_ADC;
			envctrl_init_i2c_child(dp, &(i2c_childlist[index++]));
		}

		dp = dp->sibling;
	}

	
	writeb(CONTROL_PIN, i2c + PCF8584_CSR);
	writeb(PCF8584_ADDRESS, i2c + PCF8584_DATA);

	 
	writeb(CONTROL_PIN | CONTROL_ES1, i2c + PCF8584_CSR);
	writeb(CLK_4_43 | BUS_CLK_90, i2c + PCF8584_DATA);

	
	writeb(CONTROL_PIN | CONTROL_ES0 | CONTROL_ACK, i2c + PCF8584_CSR);
	udelay(200);

	
	err = misc_register(&envctrl_dev);
	if (err) {
		printk(KERN_ERR PFX "Unable to get misc minor %d\n",
		       envctrl_dev.minor);
		goto out_iounmap;
	}

	printk(KERN_INFO PFX "Initialized ");
	for (--index; index >= 0; --index) {
		printk("[%s 0x%lx]%s", 
			(I2C_ADC == i2c_childlist[index].i2ctype) ? "adc" : 
			((I2C_GPIO == i2c_childlist[index].i2ctype) ? "gpio" : "unknown"), 
			i2c_childlist[index].addr, (0 == index) ? "\n" : " ");
	}

	kenvctrld_task = kthread_run(kenvctrld, NULL, "kenvctrld");
	if (IS_ERR(kenvctrld_task)) {
		err = PTR_ERR(kenvctrld_task);
		goto out_deregister;
	}

	return 0;

out_deregister:
	misc_deregister(&envctrl_dev);
out_iounmap:
	of_iounmap(&op->resource[0], i2c, 0x2);
	for (index = 0; index < ENVCTRL_MAX_CPU * 2; index++)
		kfree(i2c_childlist[index].tables);

	return err;
}

static int __devexit envctrl_remove(struct platform_device *op)
{
	int index;

	kthread_stop(kenvctrld_task);

	of_iounmap(&op->resource[0], i2c, 0x2);
	misc_deregister(&envctrl_dev);

	for (index = 0; index < ENVCTRL_MAX_CPU * 2; index++)
		kfree(i2c_childlist[index].tables);

	return 0;
}

static const struct of_device_id envctrl_match[] = {
	{
		.name = "i2c",
		.compatible = "i2cpcf,8584",
	},
	{},
};
MODULE_DEVICE_TABLE(of, envctrl_match);

static struct platform_driver envctrl_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = envctrl_match,
	},
	.probe		= envctrl_probe,
	.remove		= __devexit_p(envctrl_remove),
};

module_platform_driver(envctrl_driver);

MODULE_LICENSE("GPL");
