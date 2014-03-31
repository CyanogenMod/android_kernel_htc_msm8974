/*
 * linux/drivers/video/skeletonfb.c -- Skeleton for a frame buffer device
 *
 *  Modified to new api Jan 2001 by James Simmons (jsimmons@transvirtual.com)
 *
 *  Created 28 Dec 1997 by Geert Uytterhoeven
 *
 *
 *  I have started rewriting this driver as a example of the upcoming new API
 *  The primary goal is to remove the console code from fbdev and place it
 *  into fbcon.c. This reduces the code and makes writing a new fbdev driver
 *  easy since the author doesn't need to worry about console internals. It
 *  also allows the ability to run fbdev without a console/tty system on top 
 *  of it. 
 *
 *  First the roles of struct fb_info and struct display have changed. Struct
 *  display will go away. The way the new framebuffer console code will
 *  work is that it will act to translate data about the tty/console in 
 *  struct vc_data to data in a device independent way in struct fb_info. Then
 *  various functions in struct fb_ops will be called to store the device 
 *  dependent state in the par field in struct fb_info and to change the 
 *  hardware to that state. This allows a very clean separation of the fbdev
 *  layer from the console layer. It also allows one to use fbdev on its own
 *  which is a bounus for embedded devices. The reason this approach works is  
 *  for each framebuffer device when used as a tty/console device is allocated
 *  a set of virtual terminals to it. Only one virtual terminal can be active 
 *  per framebuffer device. We already have all the data we need in struct 
 *  vc_data so why store a bunch of colormaps and other fbdev specific data
 *  per virtual terminal. 
 *
 *  As you can see doing this makes the con parameter pretty much useless
 *  for struct fb_ops functions, as it should be. Also having struct  
 *  fb_var_screeninfo and other data in fb_info pretty much eliminates the 
 *  need for get_fix and get_var. Once all drivers use the fix, var, and cmap
 *  fbcon can be written around these fields. This will also eliminate the
 *  need to regenerate struct fb_var_screeninfo, struct fb_fix_screeninfo
 *  struct fb_cmap every time get_var, get_fix, get_cmap functions are called
 *  as many drivers do now. 
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>


static char *mode_option __devinitdata;

 

struct xxx_par;

static struct fb_fix_screeninfo xxxfb_fix __devinitdata = {
	.id =		"FB's name", 
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_PSEUDOCOLOR,
	.xpanstep =	1,
	.ypanstep =	1,
	.ywrapstep =	1, 
	.accel =	FB_ACCEL_NONE,
};


 
static struct fb_info info;

static struct xxx_par __initdata current_par;

int xxxfb_init(void);

static int xxxfb_open(struct fb_info *info, int user)
{
    return 0;
}

static int xxxfb_release(struct fb_info *info, int user)
{
    return 0;
}

static int xxxfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
    
    return 0;	   	
}

static int xxxfb_set_par(struct fb_info *info)
{
    struct xxx_par *par = info->par;
    
    return 0;	
}

static int xxxfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
    if (regno >= 256)  
       return -EINVAL;

    
    if (info->var.grayscale) {
       
       red = green = blue = (red * 77 + green * 151 + blue * 28) >> 8;
    }


#define CNVT_TOHW(val,width) ((((val)<<(width))+0x7FFF-(val))>>16)
    red = CNVT_TOHW(red, info->var.red.length);
    green = CNVT_TOHW(green, info->var.green.length);
    blue = CNVT_TOHW(blue, info->var.blue.length);
    transp = CNVT_TOHW(transp, info->var.transp.length);
#undef CNVT_TOHW
    if (info->fix.visual == FB_VISUAL_DIRECTCOLOR ||
	info->fix.visual == FB_VISUAL_TRUECOLOR)
	    write_{red|green|blue|transp}_to_clut();

    /* This is the point were you need to fill up the contents of
     * info->pseudo_palette. This structure is used _only_ by fbcon, thus
     * it only contains 16 entries to match the number of colors supported
     * by the console. The pseudo_palette is used only if the visual is
     * in directcolor or truecolor mode.  With other visuals, the
     * pseudo_palette is not used. (This might change in the future.)
     *
     * The contents of the pseudo_palette is in raw pixel format.  Ie, each
     * entry can be written directly to the framebuffer without any conversion.
     * The pseudo_palette is (void *).  However, if using the generic
     * drawing functions (cfb_imageblit, cfb_fillrect), the pseudo_palette
     * must be casted to (u32 *) _regardless_ of the bits per pixel. If the
     * driver is using its own drawing functions, then it can use whatever
     * size it wants.
     */
    if (info->fix.visual == FB_VISUAL_TRUECOLOR ||
	info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
	    u32 v;

	    if (regno >= 16)
		    return -EINVAL;

	    v = (red << info->var.red.offset) |
		    (green << info->var.green.offset) |
		    (blue << info->var.blue.offset) |
		    (transp << info->var.transp.offset);

	    ((u32*)(info->pseudo_palette))[regno] = v;
    }

    
    return 0;
}

static int xxxfb_pan_display(struct fb_var_screeninfo *var,
			     struct fb_info *info)
{


    
    return 0;
}

static int xxxfb_blank(int blank_mode, struct fb_info *info)
{
    
    return 0;
}



void xxxfb_fillrect(struct fb_info *p, const struct fb_fillrect *region)
{
}

void xxxfb_copyarea(struct fb_info *p, const struct fb_copyarea *area) 
{
}


void xxxfb_imageblit(struct fb_info *p, const struct fb_image *image) 
{

}

int xxxfb_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
}

void xxxfb_rotate(struct fb_info *info, int angle)
{
}

int xxxfb_sync(struct fb_info *info)
{
	return 0;
}


static struct fb_ops xxxfb_ops = {
	.owner		= THIS_MODULE,
	.fb_open	= xxxfb_open,
	.fb_read	= xxxfb_read,
	.fb_write	= xxxfb_write,
	.fb_release	= xxxfb_release,
	.fb_check_var	= xxxfb_check_var,
	.fb_set_par	= xxxfb_set_par,
	.fb_setcolreg	= xxxfb_setcolreg,
	.fb_blank	= xxxfb_blank,
	.fb_pan_display	= xxxfb_pan_display,
	.fb_fillrect	= xxxfb_fillrect, 	
	.fb_copyarea	= xxxfb_copyarea,	
	.fb_imageblit	= xxxfb_imageblit,	
	.fb_cursor	= xxxfb_cursor,		
	.fb_rotate	= xxxfb_rotate,
	.fb_sync	= xxxfb_sync,
	.fb_ioctl	= xxxfb_ioctl,
	.fb_mmap	= xxxfb_mmap,
};



static int __devinit xxxfb_probe(struct pci_dev *dev,
			      const struct pci_device_id *ent)
{
    struct fb_info *info;
    struct xxx_par *par;
    struct device *device = &dev->dev; 
    int cmap_len, retval;	
   
    info = framebuffer_alloc(sizeof(struct xxx_par), device);

    if (!info) {
	    
    }

    par = info->par;

    info->screen_base = framebuffer_virtual_memory;
    info->fbops = &xxxfb_ops;
    info->fix = xxxfb_fix; 
    info->pseudo_palette = pseudo_palette; 
    info->flags = FBINFO_DEFAULT;


    info->pixmap.addr = kmalloc(PIXMAP_SIZE, GFP_KERNEL);
    if (!info->pixmap.addr) {
	    
    }

    info->pixmap.size = PIXMAP_SIZE;

    info->pixmap.flags = FB_PIXMAP_SYSTEM;

    info->pixmap.scan_align = 4;

    info->pixmap.buf_align = 4;

    info->pixmap.access_align = 32;

    if (!mode_option)
	mode_option = "640x480@60";	 	

    retval = fb_find_mode(&info->var, info, mode_option, NULL, 0, NULL, 8);
  
    if (!retval || retval == 4)
	return -EINVAL;			

    
    if (fb_alloc_cmap(&info->cmap, cmap_len, 0))
	return -ENOMEM;
	
	
    info->var = xxxfb_var;

    xxxfb_check_var(&info->var, info);

    

    if (register_framebuffer(info) < 0) {
	fb_dealloc_cmap(&info->cmap);
	return -EINVAL;
    }
    printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
	   info->fix.id);
    pci_set_drvdata(dev, info); 
    return 0;
}

static void __devexit xxxfb_remove(struct pci_dev *dev)
{
	struct fb_info *info = pci_get_drvdata(dev);
	

	if (info) {
		unregister_framebuffer(info);
		fb_dealloc_cmap(&info->cmap);
		
		framebuffer_release(info);
	}
}

#ifdef CONFIG_PCI
#ifdef CONFIG_PM
static int xxxfb_suspend(struct pci_dev *dev, pm_message_t msg)
{
	struct fb_info *info = pci_get_drvdata(dev);
	struct xxxfb_par *par = info->par;

	
	return 0;
}

static int xxxfb_resume(struct pci_dev *dev)
{
	struct fb_info *info = pci_get_drvdata(dev);
	struct xxxfb_par *par = info->par;

	
	return 0;
}
#else
#define xxxfb_suspend NULL
#define xxxfb_resume NULL
#endif 

static struct pci_device_id xxxfb_id_table[] = {
	{ PCI_VENDOR_ID_XXX, PCI_DEVICE_ID_XXX,
	  PCI_ANY_ID, PCI_ANY_ID, PCI_BASE_CLASS_DISPLAY << 16,
	  PCI_CLASS_MASK, 0 },
	{ 0, }
};

static struct pci_driver xxxfb_driver = {
	.name =		"xxxfb",
	.id_table =	xxxfb_id_table,
	.probe =	xxxfb_probe,
	.remove =	__devexit_p(xxxfb_remove),
	.suspend =      xxxfb_suspend, 
	.resume =       xxxfb_resume,  
};

MODULE_DEVICE_TABLE(pci, xxxfb_id_table);

int __init xxxfb_init(void)
{
#ifndef MODULE
	char *option = NULL;

	if (fb_get_options("xxxfb", &option))
		return -ENODEV;
	xxxfb_setup(option);
#endif

	return pci_register_driver(&xxxfb_driver);
}

static void __exit xxxfb_exit(void)
{
	pci_unregister_driver(&xxxfb_driver);
}
#else 
#include <linux/platform_device.h>

#ifdef CONFIG_PM
static int xxxfb_suspend(struct platform_device *dev, pm_message_t msg)
{
	struct fb_info *info = platform_get_drvdata(dev);
	struct xxxfb_par *par = info->par;

	
	return 0;
}

static int xxxfb_resume(struct platform_dev *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);
	struct xxxfb_par *par = info->par;

	
	return 0;
}
#else
#define xxxfb_suspend NULL
#define xxxfb_resume NULL
#endif 

static struct platform_device_driver xxxfb_driver = {
	.probe = xxxfb_probe,
	.remove = xxxfb_remove,
	.suspend = xxxfb_suspend, 
	.resume = xxxfb_resume,   
	.driver = {
		.name = "xxxfb",
	},
};

static struct platform_device *xxxfb_device;

#ifndef MODULE

int __init xxxfb_setup(char *options)
{
    
}
#endif 

static int __init xxxfb_init(void)
{
	int ret;
#ifndef MODULE
	char *option = NULL;

	if (fb_get_options("xxxfb", &option))
		return -ENODEV;
	xxxfb_setup(option);
#endif
	ret = platform_driver_register(&xxxfb_driver);

	if (!ret) {
		xxxfb_device = platform_device_register_simple("xxxfb", 0,
								NULL, 0);

		if (IS_ERR(xxxfb_device)) {
			platform_driver_unregister(&xxxfb_driver);
			ret = PTR_ERR(xxxfb_device);
		}
	}

	return ret;
}

static void __exit xxxfb_exit(void)
{
	platform_device_unregister(xxxfb_device);
	platform_driver_unregister(&xxxfb_driver);
}
#endif 




module_init(xxxfb_init);
module_exit(xxxfb_remove);

MODULE_LICENSE("GPL");
