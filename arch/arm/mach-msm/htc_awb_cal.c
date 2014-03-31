/* Code to extract Camera AWB calibration information from ATAG
set up by the bootloader.

Copyright (C) 2008 Google, Inc.
Author: Dmitry Shmidt <dimitrysh@google.com>

This software is licensed under the terms of the GNU General Public
License version 2, as published by the Free Software Foundation, and
may be copied, distributed, and modified under those terms.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <asm/setup.h>

#include <linux/fs.h>
#include <linux/syscalls.h>

#include <linux/of.h>



#define AWB_CAL_MAX_SIZE	0x4000U     
#define DUAL_CAL_SIZE 3342

#define CALIBRATION_DATA_PATH "/calibration_data"
#define CAM_AWB_CAL_DATA "cam_awb"

struct qct_lsc_struct{
	unsigned long int	lsc_verify;
	unsigned long int	lsc_fuseid[4];
	float 			pCalcParam[17*13*4];
	unsigned long int	lsc_checksum;
};

struct qct_awb_lsc_struct{
	unsigned long int caBuff[8];
	struct qct_lsc_struct qct_lsc_data;
	
	unsigned long int flashcaBuff[8];  
	
	unsigned long int aec_caBuff[9]; 
	unsigned long int alight_caBuff[8]; 
	unsigned long int dualflashcaBuff[12];  
};

static unsigned char cam_awb_ram[AWB_CAL_MAX_SIZE];

int gCAM_AWB_CAL_LEN;

unsigned char *dummy(unsigned char *p)
{
    return p;
}

unsigned char *get_cam_awb_cal( void )
{
     struct device_node *offset = of_find_node_by_path(CALIBRATION_DATA_PATH);
     int p_size;
     unsigned char *p_data;
#ifdef CAM_AWB_CAL_DEBUG
     unsigned int i;
#endif

     p_size = 0;
     p_data = NULL;
     if (offset) {
          
          p_data = (unsigned char*) of_get_property(offset, CAM_AWB_CAL_DATA, &p_size);
#ifdef CAM_AWB_CAL_DEBUG
          if (p_data) {
		  printk("[CAM]size = %d ", p_size);
               for (i = 0; i < p_size; ++i)
                   printk("%02x ", p_data[i]);
          }
#endif
     }
        if (p_data != NULL) {
		gCAM_AWB_CAL_LEN = p_size;
            memcpy(cam_awb_ram, p_data, p_size);
        }

	return( cam_awb_ram );
}
EXPORT_SYMBOL(get_cam_awb_cal);

#if 0
static int __init parse_tag_cam_awb_cal(const struct tag *tag)
{
	unsigned char *dptr = (unsigned char *)(&tag->u);
	unsigned size;

	pr_info("[CAM]ALB_DBG, %s\n", __func__);

	size = min((tag->hdr.size - 2) * sizeof(__u32), AWB_CAL_MAX_SIZE);

	printk(KERN_INFO "[CAM]CAM_AWB_CAL Data size = %d , 0x%x, size = %d (%d,%d)\n",
			tag->hdr.size, tag->hdr.tag, size,
			((tag->hdr.size - 2) * sizeof(__u32)), (AWB_CAL_MAX_SIZE));

	gCAM_AWB_CAL_LEN = size;
	memcpy(cam_awb_ram, dummy(dptr), size); 


#ifdef ATAG_CAM_AWB_CAL_DEBUG
   {
	 int *pint, i;

	 printk(KERN_INFO "[CAM]parse_tag_cam_awb_cal():\n");

	 pint = (int *)cam_awb_ram;

	 for (i = 0; i < 1024; i++)
	   printk(KERN_INFO "%x\n", pint[i]);

   }
#endif

	return 0;
}

__tagtable(ATAG_MSM_AWB_CAL, parse_tag_cam_awb_cal);
#endif

static ssize_t awb_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned char *ptr;

	ptr = get_cam_awb_cal();
	

	ret = sizeof(struct qct_awb_lsc_struct);
	
	printk(KERN_INFO "[CAM]awb_calibration_show(%d)\n", ret);
	memcpy(buf, ptr, ret);

#ifdef CAM_AWB_CAL_DEBUG
   {
	 int i, *pint;
	 printk(KERN_INFO "[CAM]awb_calibration_show():\n");
	 pint = (int *)buf;
	 for (i = 0; i < 914; i++)
	   printk(KERN_INFO "%d-%x\n", i, pint[i]);

   }
#endif

	return ret;
}

static ssize_t awb_calibration_front_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned char *ptr;

	if (gCAM_AWB_CAL_LEN < AWB_CAL_MAX_SIZE) {
		pr_err("[CAM]%s: gCAM_AWB_CAL_LEN(%d) < AWB_CAL_MAX_SIZE(%d)\n", __func__, gCAM_AWB_CAL_LEN, AWB_CAL_MAX_SIZE);
		return 0;
	}

	ptr = get_cam_awb_cal();
	

	ret = sizeof(struct qct_awb_lsc_struct);
	
	printk(KERN_INFO "[CAM]awb_calibration_front_show(%d)\n", ret);
	memcpy(buf, ptr + 0x1000U, ret);


#ifdef CAM_AWB_CAL_DEBUG
   {
	 int i, *pint;
	 printk(KERN_INFO "[CAM]awb_calibration_front_show():\n");
	 pint = (int *)buf;
	 for (i = 0; i < 898; i++)
	   printk(KERN_INFO "%x\n", pint[i]);

   }
#endif

	return ret;
}

static ssize_t awb_calibration_sub_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned char *ptr;

	if (gCAM_AWB_CAL_LEN < AWB_CAL_MAX_SIZE) {
		pr_err("[CAM]%s: gCAM_AWB_CAL_LEN(%d) < AWB_CAL_MAX_SIZE(%d)\n", __func__, gCAM_AWB_CAL_LEN, AWB_CAL_MAX_SIZE);
		return 0;
	}

	ptr = get_cam_awb_cal();
	

	ret = sizeof(struct qct_awb_lsc_struct);
	
	printk(KERN_INFO "[CAM]awb_calibration_sub_show(%d)\n", ret);
	memcpy(buf, ptr + 0x2000U, ret);


#ifdef CAM_AWB_CAL_DEBUG
   {
	 int i, *pint;
	 printk(KERN_INFO "[CAM]awb_calibration_sub_show():\n");
	 pint = (int *)buf;
	 for (i = 0; i < 898; i++)
	   printk(KERN_INFO "%x\n", pint[i]);

   }
#endif

	return ret;
}


static ssize_t awb_calibration_3D_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = DUAL_CAL_SIZE;
	unsigned char *ptr;

	ptr = get_cam_awb_cal();
	

	printk(KERN_INFO "[CAM]awb_calibration_3D_show(%d)\n", ret);
	memcpy(buf, ptr + 0x3000U, ret);


#ifdef CAM_AWB_CAL_DEBUG
   {
	 int i, *pint;
	 printk(KERN_INFO "[CAM]awb_calibration_3D_show():\n");
	 pint = (int *)buf;
	 for (i = 0; i < DUAL_CAL_SIZE; i++)
	   printk(KERN_INFO "%x\n", pint[i]);

   }
#endif

	return ret;
}

static DEVICE_ATTR(awb_cal, 0444, awb_calibration_show, NULL);
static DEVICE_ATTR(awb_cal_front, 0444, awb_calibration_front_show, NULL);
static DEVICE_ATTR(awb_cal_sub, 0444, awb_calibration_sub_show, NULL);
static DEVICE_ATTR(awb_cal_3D, 0444, awb_calibration_3D_show, NULL);


static struct kobject *cam_awb_cal;

static int cam_get_awb_cal(void)
{
	int ret ;

	
	cam_awb_cal = kobject_create_and_add("android_camera_awb_cal", NULL);
	if (cam_awb_cal == NULL) {
		pr_info("[CAM]cam_get_awb_cal: subsystem_register failed\n");
		ret = -ENOMEM;
		return ret ;
	}

	ret = sysfs_create_file(cam_awb_cal, &dev_attr_awb_cal.attr);
	if (ret) {
		pr_info("[CAM]cam_get_awb_cal:: sysfs_create_file failed\n");
		kobject_del(cam_awb_cal);
		goto end;
	}


	ret = sysfs_create_file(cam_awb_cal, &dev_attr_awb_cal_front.attr);
	if (ret) {
		pr_info("[CAM]cam_get_awb_cal_front:: sysfs_create_file failed\n");
		kobject_del(cam_awb_cal);
		goto end;
	}

	ret = sysfs_create_file(cam_awb_cal, &dev_attr_awb_cal_sub.attr);
	if (ret) {
		pr_info("[CAM]cam_get_awb_cal_sub:: sysfs_create_file failed\n");
		kobject_del(cam_awb_cal);
		goto end;
	}

	ret = sysfs_create_file(cam_awb_cal, &dev_attr_awb_cal_3D.attr);
	if (ret) {
		pr_info("[CAM]cam_get_awb_cal_3D:: sysfs_create_file failed\n");
		kobject_del(cam_awb_cal);
		goto end;
	}

end:
	return 0 ;
}

late_initcall(cam_get_awb_cal);
