/*
 * udlfb.c -- Framebuffer driver for DisplayLink USB controller
 *
 * Copyright (C) 2009 Roberto De Ioris <roberto@unbit.it>
 * Copyright (C) 2009 Jaya Kumar <jayakumar.lkml@gmail.com>
 * Copyright (C) 2009 Bernie Thompson <bernie@plugable.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Layout is based on skeletonfb by James Simmons and Geert Uytterhoeven,
 * usb-skeleton by GregKH.
 *
 * Device-specific portions based on information from Displaylink, with work
 * from Florian Echtler, Henrik Bjerregaard Pedersen, and others.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/prefetch.h>
#include <linux/delay.h>
#include <video/udlfb.h>
#include "edid.h"

static struct fb_fix_screeninfo dlfb_fix = {
	.id =           "udlfb",
	.type =         FB_TYPE_PACKED_PIXELS,
	.visual =       FB_VISUAL_TRUECOLOR,
	.xpanstep =     0,
	.ypanstep =     0,
	.ywrapstep =    0,
	.accel =        FB_ACCEL_NONE,
};

static const u32 udlfb_info_flags = FBINFO_DEFAULT | FBINFO_READS_FAST |
		FBINFO_VIRTFB |
		FBINFO_HWACCEL_IMAGEBLIT | FBINFO_HWACCEL_FILLRECT |
		FBINFO_HWACCEL_COPYAREA | FBINFO_MISC_ALWAYS_SETPAR;

static struct usb_device_id id_table[] = {
	{.idVendor = 0x17e9,
	 .bInterfaceClass = 0xff,
	 .bInterfaceSubClass = 0x00,
	 .bInterfaceProtocol = 0x00,
	 .match_flags = USB_DEVICE_ID_MATCH_VENDOR |
		USB_DEVICE_ID_MATCH_INT_CLASS |
		USB_DEVICE_ID_MATCH_INT_SUBCLASS |
		USB_DEVICE_ID_MATCH_INT_PROTOCOL,
	},
	{},
};
MODULE_DEVICE_TABLE(usb, id_table);

static bool console = 1; 
static bool fb_defio = 1;  
static bool shadow = 1; 
static int pixel_limit; 

static void dlfb_urb_completion(struct urb *urb);
static struct urb *dlfb_get_urb(struct dlfb_data *dev);
static int dlfb_submit_urb(struct dlfb_data *dev, struct urb * urb, size_t len);
static int dlfb_alloc_urb_list(struct dlfb_data *dev, int count, size_t size);
static void dlfb_free_urb_list(struct dlfb_data *dev);

/*
 * All DisplayLink bulk operations start with 0xAF, followed by specific code
 * All operations are written to buffers which then later get sent to device
 */
static char *dlfb_set_register(char *buf, u8 reg, u8 val)
{
	*buf++ = 0xAF;
	*buf++ = 0x20;
	*buf++ = reg;
	*buf++ = val;
	return buf;
}

static char *dlfb_vidreg_lock(char *buf)
{
	return dlfb_set_register(buf, 0xFF, 0x00);
}

static char *dlfb_vidreg_unlock(char *buf)
{
	return dlfb_set_register(buf, 0xFF, 0xFF);
}

static char *dlfb_blanking(char *buf, int fb_blank)
{
	u8 reg;

	switch (fb_blank) {
	case FB_BLANK_POWERDOWN:
		reg = 0x07;
		break;
	case FB_BLANK_HSYNC_SUSPEND:
		reg = 0x05;
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		reg = 0x03;
		break;
	case FB_BLANK_NORMAL:
		reg = 0x01;
		break;
	default:
		reg = 0x00;
	}

	buf = dlfb_set_register(buf, 0x1F, reg);

	return buf;
}

static char *dlfb_set_color_depth(char *buf, u8 selection)
{
	return dlfb_set_register(buf, 0x00, selection);
}

static char *dlfb_set_base16bpp(char *wrptr, u32 base)
{
	
	wrptr = dlfb_set_register(wrptr, 0x20, base >> 16);
	wrptr = dlfb_set_register(wrptr, 0x21, base >> 8);
	return dlfb_set_register(wrptr, 0x22, base);
}

static char *dlfb_set_base8bpp(char *wrptr, u32 base)
{
	wrptr = dlfb_set_register(wrptr, 0x26, base >> 16);
	wrptr = dlfb_set_register(wrptr, 0x27, base >> 8);
	return dlfb_set_register(wrptr, 0x28, base);
}

static char *dlfb_set_register_16(char *wrptr, u8 reg, u16 value)
{
	wrptr = dlfb_set_register(wrptr, reg, value >> 8);
	return dlfb_set_register(wrptr, reg+1, value);
}

static char *dlfb_set_register_16be(char *wrptr, u8 reg, u16 value)
{
	wrptr = dlfb_set_register(wrptr, reg, value);
	return dlfb_set_register(wrptr, reg+1, value >> 8);
}

static u16 dlfb_lfsr16(u16 actual_count)
{
	u32 lv = 0xFFFF; 

	while (actual_count--) {
		lv =	 ((lv << 1) |
			(((lv >> 15) ^ (lv >> 4) ^ (lv >> 2) ^ (lv >> 1)) & 1))
			& 0xFFFF;
	}

	return (u16) lv;
}

/*
 * This does LFSR conversion on the value that is to be written.
 * See LFSR explanation above for more detail.
 */
static char *dlfb_set_register_lfsr16(char *wrptr, u8 reg, u16 value)
{
	return dlfb_set_register_16(wrptr, reg, dlfb_lfsr16(value));
}

static char *dlfb_set_vid_cmds(char *wrptr, struct fb_var_screeninfo *var)
{
	u16 xds, yds;
	u16 xde, yde;
	u16 yec;

	
	xds = var->left_margin + var->hsync_len;
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x01, xds);
	
	xde = xds + var->xres;
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x03, xde);

	
	yds = var->upper_margin + var->vsync_len;
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x05, yds);
	
	yde = yds + var->yres;
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x07, yde);

	
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x09,
			xde + var->right_margin - 1);

	
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x0B, 1);

	
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x0D, var->hsync_len + 1);

	
	wrptr = dlfb_set_register_16(wrptr, 0x0F, var->xres);

	
	yec = var->yres + var->upper_margin + var->lower_margin +
			var->vsync_len;
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x11, yec);

	
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x13, 0);

	
	wrptr = dlfb_set_register_lfsr16(wrptr, 0x15, var->vsync_len);

	
	wrptr = dlfb_set_register_16(wrptr, 0x17, var->yres);

	
	wrptr = dlfb_set_register_16be(wrptr, 0x1B,
			200*1000*1000/var->pixclock);

	return wrptr;
}

static int dlfb_set_video_mode(struct dlfb_data *dev,
				struct fb_var_screeninfo *var)
{
	char *buf;
	char *wrptr;
	int retval = 0;
	int writesize;
	struct urb *urb;

	if (!atomic_read(&dev->usb_active))
		return -EPERM;

	urb = dlfb_get_urb(dev);
	if (!urb)
		return -ENOMEM;

	buf = (char *) urb->transfer_buffer;

	wrptr = dlfb_vidreg_lock(buf);
	wrptr = dlfb_set_color_depth(wrptr, 0x00);
	
	wrptr = dlfb_set_base16bpp(wrptr, 0);
	
	wrptr = dlfb_set_base8bpp(wrptr, dev->info->fix.smem_len);

	wrptr = dlfb_set_vid_cmds(wrptr, var);
	wrptr = dlfb_blanking(wrptr, FB_BLANK_UNBLANK);
	wrptr = dlfb_vidreg_unlock(wrptr);

	writesize = wrptr - buf;

	retval = dlfb_submit_urb(dev, urb, writesize);

	dev->blank_mode = FB_BLANK_UNBLANK;

	return retval;
}

static int dlfb_ops_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long page, pos;

	if (offset + size > info->fix.smem_len)
		return -EINVAL;

	pos = (unsigned long)info->fix.smem_start + offset;

	pr_notice("mmap() framebuffer addr:%lu size:%lu\n",
		  pos, size);

	while (size > 0) {
		page = vmalloc_to_pfn((void *)pos);
		if (remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED))
			return -EAGAIN;

		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	vma->vm_flags |= VM_RESERVED;	
	return 0;
}

static int dlfb_trim_hline(const u8 *bback, const u8 **bfront, int *width_bytes)
{
	int j, k;
	const unsigned long *back = (const unsigned long *) bback;
	const unsigned long *front = (const unsigned long *) *bfront;
	const int width = *width_bytes / sizeof(unsigned long);
	int identical = width;
	int start = width;
	int end = width;

	prefetch((void *) front);
	prefetch((void *) back);

	for (j = 0; j < width; j++) {
		if (back[j] != front[j]) {
			start = j;
			break;
		}
	}

	for (k = width - 1; k > j; k--) {
		if (back[k] != front[k]) {
			end = k+1;
			break;
		}
	}

	identical = start + (width - end);
	*bfront = (u8 *) &front[start];
	*width_bytes = (end - start) * sizeof(unsigned long);

	return identical * sizeof(unsigned long);
}

static void dlfb_compress_hline(
	const uint16_t **pixel_start_ptr,
	const uint16_t *const pixel_end,
	uint32_t *device_address_ptr,
	uint8_t **command_buffer_ptr,
	const uint8_t *const cmd_buffer_end)
{
	const uint16_t *pixel = *pixel_start_ptr;
	uint32_t dev_addr  = *device_address_ptr;
	uint8_t *cmd = *command_buffer_ptr;
	const int bpp = 2;

	while ((pixel_end > pixel) &&
	       (cmd_buffer_end - MIN_RLX_CMD_BYTES > cmd)) {
		uint8_t *raw_pixels_count_byte = 0;
		uint8_t *cmd_pixels_count_byte = 0;
		const uint16_t *raw_pixel_start = 0;
		const uint16_t *cmd_pixel_start, *cmd_pixel_end = 0;

		prefetchw((void *) cmd); 

		*cmd++ = 0xAF;
		*cmd++ = 0x6B;
		*cmd++ = (uint8_t) ((dev_addr >> 16) & 0xFF);
		*cmd++ = (uint8_t) ((dev_addr >> 8) & 0xFF);
		*cmd++ = (uint8_t) ((dev_addr) & 0xFF);

		cmd_pixels_count_byte = cmd++; 
		cmd_pixel_start = pixel;

		raw_pixels_count_byte = cmd++; 
		raw_pixel_start = pixel;

		cmd_pixel_end = pixel + min(MAX_CMD_PIXELS + 1,
			min((int)(pixel_end - pixel),
			    (int)(cmd_buffer_end - cmd) / bpp));

		prefetch_range((void *) pixel, (cmd_pixel_end - pixel) * bpp);

		while (pixel < cmd_pixel_end) {
			const uint16_t * const repeating_pixel = pixel;

			*(uint16_t *)cmd = cpu_to_be16p(pixel);
			cmd += 2;
			pixel++;

			if (unlikely((pixel < cmd_pixel_end) &&
				     (*pixel == *repeating_pixel))) {
				
				*raw_pixels_count_byte = ((repeating_pixel -
						raw_pixel_start) + 1) & 0xFF;

				while ((pixel < cmd_pixel_end)
				       && (*pixel == *repeating_pixel)) {
					pixel++;
				}

				
				*cmd++ = ((pixel - repeating_pixel) - 1) & 0xFF;

				
				raw_pixel_start = pixel;
				raw_pixels_count_byte = cmd++;
			}
		}

		if (pixel > raw_pixel_start) {
			
			*raw_pixels_count_byte = (pixel-raw_pixel_start) & 0xFF;
		}

		*cmd_pixels_count_byte = (pixel - cmd_pixel_start) & 0xFF;
		dev_addr += (pixel - cmd_pixel_start) * bpp;
	}

	if (cmd_buffer_end <= MIN_RLX_CMD_BYTES + cmd) {
		
		if (cmd_buffer_end > cmd)
			memset(cmd, 0xAF, cmd_buffer_end - cmd);
		cmd = (uint8_t *) cmd_buffer_end;
	}

	*command_buffer_ptr = cmd;
	*pixel_start_ptr = pixel;
	*device_address_ptr = dev_addr;

	return;
}

static int dlfb_render_hline(struct dlfb_data *dev, struct urb **urb_ptr,
			      const char *front, char **urb_buf_ptr,
			      u32 byte_offset, u32 byte_width,
			      int *ident_ptr, int *sent_ptr)
{
	const u8 *line_start, *line_end, *next_pixel;
	u32 dev_addr = dev->base16 + byte_offset;
	struct urb *urb = *urb_ptr;
	u8 *cmd = *urb_buf_ptr;
	u8 *cmd_end = (u8 *) urb->transfer_buffer + urb->transfer_buffer_length;

	line_start = (u8 *) (front + byte_offset);
	next_pixel = line_start;
	line_end = next_pixel + byte_width;

	if (dev->backing_buffer) {
		int offset;
		const u8 *back_start = (u8 *) (dev->backing_buffer
						+ byte_offset);

		*ident_ptr += dlfb_trim_hline(back_start, &next_pixel,
			&byte_width);

		offset = next_pixel - line_start;
		line_end = next_pixel + byte_width;
		dev_addr += offset;
		back_start += offset;
		line_start += offset;

		memcpy((char *)back_start, (char *) line_start,
		       byte_width);
	}

	while (next_pixel < line_end) {

		dlfb_compress_hline((const uint16_t **) &next_pixel,
			     (const uint16_t *) line_end, &dev_addr,
			(u8 **) &cmd, (u8 *) cmd_end);

		if (cmd >= cmd_end) {
			int len = cmd - (u8 *) urb->transfer_buffer;
			if (dlfb_submit_urb(dev, urb, len))
				return 1; 
			*sent_ptr += len;
			urb = dlfb_get_urb(dev);
			if (!urb)
				return 1; 
			*urb_ptr = urb;
			cmd = urb->transfer_buffer;
			cmd_end = &cmd[urb->transfer_buffer_length];
		}
	}

	*urb_buf_ptr = cmd;

	return 0;
}

int dlfb_handle_damage(struct dlfb_data *dev, int x, int y,
	       int width, int height, char *data)
{
	int i, ret;
	char *cmd;
	cycles_t start_cycles, end_cycles;
	int bytes_sent = 0;
	int bytes_identical = 0;
	struct urb *urb;
	int aligned_x;

	start_cycles = get_cycles();

	aligned_x = DL_ALIGN_DOWN(x, sizeof(unsigned long));
	width = DL_ALIGN_UP(width + (x-aligned_x), sizeof(unsigned long));
	x = aligned_x;

	if ((width <= 0) ||
	    (x + width > dev->info->var.xres) ||
	    (y + height > dev->info->var.yres))
		return -EINVAL;

	if (!atomic_read(&dev->usb_active))
		return 0;

	urb = dlfb_get_urb(dev);
	if (!urb)
		return 0;
	cmd = urb->transfer_buffer;

	for (i = y; i < y + height ; i++) {
		const int line_offset = dev->info->fix.line_length * i;
		const int byte_offset = line_offset + (x * BPP);

		if (dlfb_render_hline(dev, &urb,
				      (char *) dev->info->fix.smem_start,
				      &cmd, byte_offset, width * BPP,
				      &bytes_identical, &bytes_sent))
			goto error;
	}

	if (cmd > (char *) urb->transfer_buffer) {
		
		int len = cmd - (char *) urb->transfer_buffer;
		ret = dlfb_submit_urb(dev, urb, len);
		bytes_sent += len;
	} else
		dlfb_urb_completion(urb);

error:
	atomic_add(bytes_sent, &dev->bytes_sent);
	atomic_add(bytes_identical, &dev->bytes_identical);
	atomic_add(width*height*2, &dev->bytes_rendered);
	end_cycles = get_cycles();
	atomic_add(((unsigned int) ((end_cycles - start_cycles)
		    >> 10)), 
		   &dev->cpu_kcycles_used);

	return 0;
}

static ssize_t dlfb_ops_write(struct fb_info *info, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	ssize_t result;
	struct dlfb_data *dev = info->par;
	u32 offset = (u32) *ppos;

	result = fb_sys_write(info, buf, count, ppos);

	if (result > 0) {
		int start = max((int)(offset / info->fix.line_length) - 1, 0);
		int lines = min((u32)((result / info->fix.line_length) + 1),
				(u32)info->var.yres);

		dlfb_handle_damage(dev, 0, start, info->var.xres,
			lines, info->screen_base);
	}

	return result;
}

static void dlfb_ops_copyarea(struct fb_info *info,
				const struct fb_copyarea *area)
{

	struct dlfb_data *dev = info->par;

	sys_copyarea(info, area);

	dlfb_handle_damage(dev, area->dx, area->dy,
			area->width, area->height, info->screen_base);
}

static void dlfb_ops_imageblit(struct fb_info *info,
				const struct fb_image *image)
{
	struct dlfb_data *dev = info->par;

	sys_imageblit(info, image);

	dlfb_handle_damage(dev, image->dx, image->dy,
			image->width, image->height, info->screen_base);
}

static void dlfb_ops_fillrect(struct fb_info *info,
			  const struct fb_fillrect *rect)
{
	struct dlfb_data *dev = info->par;

	sys_fillrect(info, rect);

	dlfb_handle_damage(dev, rect->dx, rect->dy, rect->width,
			      rect->height, info->screen_base);
}

static void dlfb_dpy_deferred_io(struct fb_info *info,
				struct list_head *pagelist)
{
	struct page *cur;
	struct fb_deferred_io *fbdefio = info->fbdefio;
	struct dlfb_data *dev = info->par;
	struct urb *urb;
	char *cmd;
	cycles_t start_cycles, end_cycles;
	int bytes_sent = 0;
	int bytes_identical = 0;
	int bytes_rendered = 0;

	if (!fb_defio)
		return;

	if (!atomic_read(&dev->usb_active))
		return;

	start_cycles = get_cycles();

	urb = dlfb_get_urb(dev);
	if (!urb)
		return;

	cmd = urb->transfer_buffer;

	/* walk the written page list and render each to device */
	list_for_each_entry(cur, &fbdefio->pagelist, lru) {

		if (dlfb_render_hline(dev, &urb, (char *) info->fix.smem_start,
				  &cmd, cur->index << PAGE_SHIFT,
				  PAGE_SIZE, &bytes_identical, &bytes_sent))
			goto error;
		bytes_rendered += PAGE_SIZE;
	}

	if (cmd > (char *) urb->transfer_buffer) {
		
		int len = cmd - (char *) urb->transfer_buffer;
		dlfb_submit_urb(dev, urb, len);
		bytes_sent += len;
	} else
		dlfb_urb_completion(urb);

error:
	atomic_add(bytes_sent, &dev->bytes_sent);
	atomic_add(bytes_identical, &dev->bytes_identical);
	atomic_add(bytes_rendered, &dev->bytes_rendered);
	end_cycles = get_cycles();
	atomic_add(((unsigned int) ((end_cycles - start_cycles)
		    >> 10)), 
		   &dev->cpu_kcycles_used);
}

static int dlfb_get_edid(struct dlfb_data *dev, char *edid, int len)
{
	int i;
	int ret;
	char *rbuf;

	rbuf = kmalloc(2, GFP_KERNEL);
	if (!rbuf)
		return 0;

	for (i = 0; i < len; i++) {
		ret = usb_control_msg(dev->udev,
				    usb_rcvctrlpipe(dev->udev, 0), (0x02),
				    (0x80 | (0x02 << 5)), i << 8, 0xA1, rbuf, 2,
				    HZ);
		if (ret < 1) {
			pr_err("Read EDID byte %d failed err %x\n", i, ret);
			i--;
			break;
		}
		edid[i] = rbuf[1];
	}

	kfree(rbuf);

	return i;
}

static int dlfb_ops_ioctl(struct fb_info *info, unsigned int cmd,
				unsigned long arg)
{

	struct dlfb_data *dev = info->par;

	if (!atomic_read(&dev->usb_active))
		return 0;

	
	if (cmd == DLFB_IOCTL_RETURN_EDID) {
		void __user *edid = (void __user *)arg;
		if (copy_to_user(edid, dev->edid, dev->edid_size))
			return -EFAULT;
		return 0;
	}

	
	if (cmd == DLFB_IOCTL_REPORT_DAMAGE) {
		struct dloarea area;

		if (copy_from_user(&area, (void __user *)arg,
				  sizeof(struct dloarea)))
			return -EFAULT;

		if (info->fbdefio)
			info->fbdefio->delay = DL_DEFIO_WRITE_DISABLE;

		if (area.x < 0)
			area.x = 0;

		if (area.x > info->var.xres)
			area.x = info->var.xres;

		if (area.y < 0)
			area.y = 0;

		if (area.y > info->var.yres)
			area.y = info->var.yres;

		dlfb_handle_damage(dev, area.x, area.y, area.w, area.h,
			   info->screen_base);
	}

	return 0;
}

static int
dlfb_ops_setcolreg(unsigned regno, unsigned red, unsigned green,
	       unsigned blue, unsigned transp, struct fb_info *info)
{
	int err = 0;

	if (regno >= info->cmap.len)
		return 1;

	if (regno < 16) {
		if (info->var.red.offset == 10) {
			
			((u32 *) (info->pseudo_palette))[regno] =
			    ((red & 0xf800) >> 1) |
			    ((green & 0xf800) >> 6) | ((blue & 0xf800) >> 11);
		} else {
			
			((u32 *) (info->pseudo_palette))[regno] =
			    ((red & 0xf800)) |
			    ((green & 0xfc00) >> 5) | ((blue & 0xf800) >> 11);
		}
	}

	return err;
}

static int dlfb_ops_open(struct fb_info *info, int user)
{
	struct dlfb_data *dev = info->par;

	if ((user == 0) && (!console))
		return -EBUSY;

	
	if (dev->virtualized)
		return -ENODEV;

	dev->fb_count++;

	kref_get(&dev->kref);

	if (fb_defio && (info->fbdefio == NULL)) {
		

		struct fb_deferred_io *fbdefio;

		fbdefio = kmalloc(sizeof(struct fb_deferred_io), GFP_KERNEL);

		if (fbdefio) {
			fbdefio->delay = DL_DEFIO_WRITE_DELAY;
			fbdefio->deferred_io = dlfb_dpy_deferred_io;
		}

		info->fbdefio = fbdefio;
		fb_deferred_io_init(info);
	}

	pr_notice("open /dev/fb%d user=%d fb_info=%p count=%d\n",
	    info->node, user, info, dev->fb_count);

	return 0;
}

static void dlfb_free(struct kref *kref)
{
	struct dlfb_data *dev = container_of(kref, struct dlfb_data, kref);

	if (dev->backing_buffer)
		vfree(dev->backing_buffer);

	kfree(dev->edid);

	pr_warn("freeing dlfb_data %p\n", dev);

	kfree(dev);
}

static void dlfb_release_urb_work(struct work_struct *work)
{
	struct urb_node *unode = container_of(work, struct urb_node,
					      release_urb_work.work);

	up(&unode->dev->urbs.limit_sem);
}

static void dlfb_free_framebuffer(struct dlfb_data *dev)
{
	struct fb_info *info = dev->info;

	if (info) {
		int node = info->node;

		unregister_framebuffer(info);

		if (info->cmap.len != 0)
			fb_dealloc_cmap(&info->cmap);
		if (info->monspecs.modedb)
			fb_destroy_modedb(info->monspecs.modedb);
		if (info->screen_base)
			vfree(info->screen_base);

		fb_destroy_modelist(&info->modelist);

		dev->info = NULL;

		
		framebuffer_release(info);

		pr_warn("fb_info for /dev/fb%d has been freed\n", node);
	}

	
	kref_put(&dev->kref, dlfb_free);
}

static void dlfb_free_framebuffer_work(struct work_struct *work)
{
	struct dlfb_data *dev = container_of(work, struct dlfb_data,
					     free_framebuffer_work.work);
	dlfb_free_framebuffer(dev);
}
static int dlfb_ops_release(struct fb_info *info, int user)
{
	struct dlfb_data *dev = info->par;

	dev->fb_count--;

	
	if (dev->virtualized && (dev->fb_count == 0))
		schedule_delayed_work(&dev->free_framebuffer_work, HZ);

	if ((dev->fb_count == 0) && (info->fbdefio)) {
		fb_deferred_io_cleanup(info);
		kfree(info->fbdefio);
		info->fbdefio = NULL;
		info->fbops->fb_mmap = dlfb_ops_mmap;
	}

	pr_warn("released /dev/fb%d user=%d count=%d\n",
		  info->node, user, dev->fb_count);

	kref_put(&dev->kref, dlfb_free);

	return 0;
}

static int dlfb_is_valid_mode(struct fb_videomode *mode,
		struct fb_info *info)
{
	struct dlfb_data *dev = info->par;

	if (mode->xres * mode->yres > dev->sku_pixel_limit) {
		pr_warn("%dx%d beyond chip capabilities\n",
		       mode->xres, mode->yres);
		return 0;
	}

	pr_info("%dx%d @ %d Hz valid mode\n", mode->xres, mode->yres,
		mode->refresh);

	return 1;
}

static void dlfb_var_color_format(struct fb_var_screeninfo *var)
{
	const struct fb_bitfield red = { 11, 5, 0 };
	const struct fb_bitfield green = { 5, 6, 0 };
	const struct fb_bitfield blue = { 0, 5, 0 };

	var->bits_per_pixel = 16;
	var->red = red;
	var->green = green;
	var->blue = blue;
}

static int dlfb_ops_check_var(struct fb_var_screeninfo *var,
				struct fb_info *info)
{
	struct fb_videomode mode;

	
	if ((var->xres * var->yres * 2) > info->fix.smem_len)
		return -EINVAL;

	
	dlfb_var_color_format(var);

	fb_var_to_videomode(&mode, var);

	if (!dlfb_is_valid_mode(&mode, info))
		return -EINVAL;

	return 0;
}

static int dlfb_ops_set_par(struct fb_info *info)
{
	struct dlfb_data *dev = info->par;
	int result;
	u16 *pix_framebuffer;
	int i;

	pr_notice("set_par mode %dx%d\n", info->var.xres, info->var.yres);

	result = dlfb_set_video_mode(dev, &info->var);

	if ((result == 0) && (dev->fb_count == 0)) {

		

		pix_framebuffer = (u16 *) info->screen_base;
		for (i = 0; i < info->fix.smem_len / 2; i++)
			pix_framebuffer[i] = 0x37e6;

		dlfb_handle_damage(dev, 0, 0, info->var.xres, info->var.yres,
				   info->screen_base);
	}

	return result;
}

static char *dlfb_dummy_render(char *buf)
{
	*buf++ = 0xAF;
	*buf++ = 0x6A; 
	*buf++ = 0x00; 
	*buf++ = 0x00;
	*buf++ = 0x00;
	*buf++ = 0x01; 
	*buf++ = 0x00; 
	*buf++ = 0x00;
	*buf++ = 0x00;
	return buf;
}

static int dlfb_ops_blank(int blank_mode, struct fb_info *info)
{
	struct dlfb_data *dev = info->par;
	char *bufptr;
	struct urb *urb;

	pr_info("/dev/fb%d FB_BLANK mode %d --> %d\n",
		info->node, dev->blank_mode, blank_mode);

	if ((dev->blank_mode == FB_BLANK_POWERDOWN) &&
	    (blank_mode != FB_BLANK_POWERDOWN)) {

		
		dlfb_set_video_mode(dev, &info->var);
	}

	urb = dlfb_get_urb(dev);
	if (!urb)
		return 0;

	bufptr = (char *) urb->transfer_buffer;
	bufptr = dlfb_vidreg_lock(bufptr);
	bufptr = dlfb_blanking(bufptr, blank_mode);
	bufptr = dlfb_vidreg_unlock(bufptr);

	
	bufptr = dlfb_dummy_render(bufptr);

	dlfb_submit_urb(dev, urb, bufptr -
			(char *) urb->transfer_buffer);

	dev->blank_mode = blank_mode;

	return 0;
}

static struct fb_ops dlfb_ops = {
	.owner = THIS_MODULE,
	.fb_read = fb_sys_read,
	.fb_write = dlfb_ops_write,
	.fb_setcolreg = dlfb_ops_setcolreg,
	.fb_fillrect = dlfb_ops_fillrect,
	.fb_copyarea = dlfb_ops_copyarea,
	.fb_imageblit = dlfb_ops_imageblit,
	.fb_mmap = dlfb_ops_mmap,
	.fb_ioctl = dlfb_ops_ioctl,
	.fb_open = dlfb_ops_open,
	.fb_release = dlfb_ops_release,
	.fb_blank = dlfb_ops_blank,
	.fb_check_var = dlfb_ops_check_var,
	.fb_set_par = dlfb_ops_set_par,
};


static int dlfb_realloc_framebuffer(struct dlfb_data *dev, struct fb_info *info)
{
	int retval = -ENOMEM;
	int old_len = info->fix.smem_len;
	int new_len;
	unsigned char *old_fb = info->screen_base;
	unsigned char *new_fb;
	unsigned char *new_back = 0;

	pr_warn("Reallocating framebuffer. Addresses will change!\n");

	new_len = info->fix.line_length * info->var.yres;

	if (PAGE_ALIGN(new_len) > old_len) {
		new_fb = vmalloc(new_len);
		if (!new_fb) {
			pr_err("Virtual framebuffer alloc failed\n");
			goto error;
		}

		if (info->screen_base) {
			memcpy(new_fb, old_fb, old_len);
			vfree(info->screen_base);
		}

		info->screen_base = new_fb;
		info->fix.smem_len = PAGE_ALIGN(new_len);
		info->fix.smem_start = (unsigned long) new_fb;
		info->flags = udlfb_info_flags;

		if (shadow)
			new_back = vzalloc(new_len);
		if (!new_back)
			pr_info("No shadow/backing buffer allocated\n");
		else {
			if (dev->backing_buffer)
				vfree(dev->backing_buffer);
			dev->backing_buffer = new_back;
		}
	}

	retval = 0;

error:
	return retval;
}

static int dlfb_setup_modes(struct dlfb_data *dev,
			   struct fb_info *info,
			   char *default_edid, size_t default_edid_size)
{
	int i;
	const struct fb_videomode *default_vmode = NULL;
	int result = 0;
	char *edid;
	int tries = 3;

	if (info->dev) 
		mutex_lock(&info->lock);

	edid = kmalloc(EDID_LENGTH, GFP_KERNEL);
	if (!edid) {
		result = -ENOMEM;
		goto error;
	}

	fb_destroy_modelist(&info->modelist);
	memset(&info->monspecs, 0, sizeof(info->monspecs));

	while (tries--) {

		i = dlfb_get_edid(dev, edid, EDID_LENGTH);

		if (i >= EDID_LENGTH)
			fb_edid_to_monspecs(edid, &info->monspecs);

		if (info->monspecs.modedb_len > 0) {
			dev->edid = edid;
			dev->edid_size = i;
			break;
		}
	}

	
	if (info->monspecs.modedb_len == 0) {

		pr_err("Unable to get valid EDID from device/display\n");

		if (dev->edid) {
			fb_edid_to_monspecs(dev->edid, &info->monspecs);
			if (info->monspecs.modedb_len > 0)
				pr_err("Using previously queried EDID\n");
		}
	}

	
	if (info->monspecs.modedb_len == 0) {
		if (default_edid_size >= EDID_LENGTH) {
			fb_edid_to_monspecs(default_edid, &info->monspecs);
			if (info->monspecs.modedb_len > 0) {
				memcpy(edid, default_edid, default_edid_size);
				dev->edid = edid;
				dev->edid_size = default_edid_size;
				pr_err("Using default/backup EDID\n");
			}
		}
	}

	
	if (info->monspecs.modedb_len > 0) {

		for (i = 0; i < info->monspecs.modedb_len; i++) {
			if (dlfb_is_valid_mode(&info->monspecs.modedb[i], info))
				fb_add_videomode(&info->monspecs.modedb[i],
					&info->modelist);
			else {
				if (i == 0)
					
					info->monspecs.misc
						&= ~FB_MISC_1ST_DETAIL;
			}
		}

		default_vmode = fb_find_best_display(&info->monspecs,
						     &info->modelist);
	}

	
	if (default_vmode == NULL) {

		struct fb_videomode fb_vmode = {0};

		for (i = 0; i < VESA_MODEDB_SIZE; i++) {
			if (dlfb_is_valid_mode((struct fb_videomode *)
						&vesa_modes[i], info))
				fb_add_videomode(&vesa_modes[i],
						 &info->modelist);
		}

		fb_vmode.xres = 800;
		fb_vmode.yres = 600;
		fb_vmode.refresh = 60;
		default_vmode = fb_find_nearest_mode(&fb_vmode,
						     &info->modelist);
	}

	
	if ((default_vmode != NULL) && (dev->fb_count == 0)) {

		fb_videomode_to_var(&info->var, default_vmode);
		dlfb_var_color_format(&info->var);

		memcpy(&info->fix, &dlfb_fix, sizeof(dlfb_fix));
		info->fix.line_length = info->var.xres *
			(info->var.bits_per_pixel / 8);

		result = dlfb_realloc_framebuffer(dev, info);

	} else
		result = -EINVAL;

error:
	if (edid && (dev->edid != edid))
		kfree(edid);

	if (info->dev)
		mutex_unlock(&info->lock);

	return result;
}

static ssize_t metrics_bytes_rendered_show(struct device *fbdev,
				   struct device_attribute *a, char *buf) {
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dev = fb_info->par;
	return snprintf(buf, PAGE_SIZE, "%u\n",
			atomic_read(&dev->bytes_rendered));
}

static ssize_t metrics_bytes_identical_show(struct device *fbdev,
				   struct device_attribute *a, char *buf) {
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dev = fb_info->par;
	return snprintf(buf, PAGE_SIZE, "%u\n",
			atomic_read(&dev->bytes_identical));
}

static ssize_t metrics_bytes_sent_show(struct device *fbdev,
				   struct device_attribute *a, char *buf) {
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dev = fb_info->par;
	return snprintf(buf, PAGE_SIZE, "%u\n",
			atomic_read(&dev->bytes_sent));
}

static ssize_t metrics_cpu_kcycles_used_show(struct device *fbdev,
				   struct device_attribute *a, char *buf) {
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dev = fb_info->par;
	return snprintf(buf, PAGE_SIZE, "%u\n",
			atomic_read(&dev->cpu_kcycles_used));
}

static ssize_t edid_show(
			struct file *filp,
			struct kobject *kobj, struct bin_attribute *a,
			 char *buf, loff_t off, size_t count) {
	struct device *fbdev = container_of(kobj, struct device, kobj);
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dev = fb_info->par;

	if (dev->edid == NULL)
		return 0;

	if ((off >= dev->edid_size) || (count > dev->edid_size))
		return 0;

	if (off + count > dev->edid_size)
		count = dev->edid_size - off;

	pr_info("sysfs edid copy %p to %p, %d bytes\n",
		dev->edid, buf, (int) count);

	memcpy(buf, dev->edid, count);

	return count;
}

static ssize_t edid_store(
			struct file *filp,
			struct kobject *kobj, struct bin_attribute *a,
			char *src, loff_t src_off, size_t src_size) {
	struct device *fbdev = container_of(kobj, struct device, kobj);
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dev = fb_info->par;
	int ret;

	
	if ((src_size != EDID_LENGTH) || (src_off != 0))
		return -EINVAL;

	ret = dlfb_setup_modes(dev, fb_info, src, src_size);
	if (ret)
		return ret;

	if (!dev->edid || memcmp(src, dev->edid, src_size))
		return -EINVAL;

	pr_info("sysfs written EDID is new default\n");
	dlfb_ops_set_par(fb_info);
	return src_size;
}

static ssize_t metrics_reset_store(struct device *fbdev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(fbdev);
	struct dlfb_data *dev = fb_info->par;

	atomic_set(&dev->bytes_rendered, 0);
	atomic_set(&dev->bytes_identical, 0);
	atomic_set(&dev->bytes_sent, 0);
	atomic_set(&dev->cpu_kcycles_used, 0);

	return count;
}

static struct bin_attribute edid_attr = {
	.attr.name = "edid",
	.attr.mode = 0666,
	.size = EDID_LENGTH,
	.read = edid_show,
	.write = edid_store
};

static struct device_attribute fb_device_attrs[] = {
	__ATTR_RO(metrics_bytes_rendered),
	__ATTR_RO(metrics_bytes_identical),
	__ATTR_RO(metrics_bytes_sent),
	__ATTR_RO(metrics_cpu_kcycles_used),
	__ATTR(metrics_reset, S_IWUSR, NULL, metrics_reset_store),
};

static int dlfb_select_std_channel(struct dlfb_data *dev)
{
	int ret;
	u8 set_def_chn[] = {	   0x57, 0xCD, 0xDC, 0xA7,
				0x1C, 0x88, 0x5E, 0x15,
				0x60, 0xFE, 0xC6, 0x97,
				0x16, 0x3D, 0x47, 0xF2  };

	ret = usb_control_msg(dev->udev, usb_sndctrlpipe(dev->udev, 0),
			NR_USB_REQUEST_CHANNEL,
			(USB_DIR_OUT | USB_TYPE_VENDOR), 0, 0,
			set_def_chn, sizeof(set_def_chn), USB_CTRL_SET_TIMEOUT);
	return ret;
}

static int dlfb_parse_vendor_descriptor(struct dlfb_data *dev,
					struct usb_interface *interface)
{
	char *desc;
	char *buf;
	char *desc_end;

	int total_len = 0;

	buf = kzalloc(MAX_VENDOR_DESCRIPTOR_SIZE, GFP_KERNEL);
	if (!buf)
		return false;
	desc = buf;

	total_len = usb_get_descriptor(interface_to_usbdev(interface),
					0x5f, 
					0, desc, MAX_VENDOR_DESCRIPTOR_SIZE);

	
	if (total_len < 0) {
		if (0 == usb_get_extra_descriptor(interface->cur_altsetting,
			0x5f, &desc))
			total_len = (int) desc[0];
	}

	if (total_len > 5) {
		pr_info("vendor descriptor length:%x data:%02x %02x %02x %02x" \
			"%02x %02x %02x %02x %02x %02x %02x\n",
			total_len, desc[0],
			desc[1], desc[2], desc[3], desc[4], desc[5], desc[6],
			desc[7], desc[8], desc[9], desc[10]);

		if ((desc[0] != total_len) || 
		    (desc[1] != 0x5f) ||   
		    (desc[2] != 0x01) ||   
		    (desc[3] != 0x00) ||
		    (desc[4] != total_len - 2)) 
			goto unrecognized;

		desc_end = desc + total_len;
		desc += 5; 

		while (desc < desc_end) {
			u8 length;
			u16 key;

			key = le16_to_cpu(*((u16 *) desc));
			desc += sizeof(u16);
			length = *desc;
			desc++;

			switch (key) {
			case 0x0200: { 
				u32 max_area;
				max_area = le32_to_cpu(*((u32 *)desc));
				pr_warn("DL chip limited to %d pixel modes\n",
					max_area);
				dev->sku_pixel_limit = max_area;
				break;
			}
			default:
				break;
			}
			desc += length;
		}
	} else {
		pr_info("vendor descriptor not available (%d)\n", total_len);
	}

	goto success;

unrecognized:
	
	pr_err("Unrecognized vendor firmware descriptor\n");

success:
	kfree(buf);
	return true;
}

static void dlfb_init_framebuffer_work(struct work_struct *work);

static int dlfb_usb_probe(struct usb_interface *interface,
			const struct usb_device_id *id)
{
	struct usb_device *usbdev;
	struct dlfb_data *dev = 0;
	int retval = -ENOMEM;

	

	usbdev = interface_to_usbdev(interface);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		err("dlfb_usb_probe: failed alloc of dev struct\n");
		goto error;
	}

	kref_init(&dev->kref); 

	dev->udev = usbdev;
	dev->gdev = &usbdev->dev; 
	usb_set_intfdata(interface, dev);

	pr_info("%s %s - serial #%s\n",
		usbdev->manufacturer, usbdev->product, usbdev->serial);
	pr_info("vid_%04x&pid_%04x&rev_%04x driver's dlfb_data struct at %p\n",
		usbdev->descriptor.idVendor, usbdev->descriptor.idProduct,
		usbdev->descriptor.bcdDevice, dev);
	pr_info("console enable=%d\n", console);
	pr_info("fb_defio enable=%d\n", fb_defio);
	pr_info("shadow enable=%d\n", shadow);

	dev->sku_pixel_limit = 2048 * 1152; 

	if (!dlfb_parse_vendor_descriptor(dev, interface)) {
		pr_err("firmware not recognized. Assume incompatible device\n");
		goto error;
	}

	if (pixel_limit) {
		pr_warn("DL chip limit of %d overriden"
			" by module param to %d\n",
			dev->sku_pixel_limit, pixel_limit);
		dev->sku_pixel_limit = pixel_limit;
	}


	if (!dlfb_alloc_urb_list(dev, WRITES_IN_FLIGHT, MAX_TRANSFER)) {
		retval = -ENOMEM;
		pr_err("dlfb_alloc_urb_list failed\n");
		goto error;
	}

	kref_get(&dev->kref); 

	

	
	INIT_DELAYED_WORK(&dev->init_framebuffer_work,
			  dlfb_init_framebuffer_work);
	schedule_delayed_work(&dev->init_framebuffer_work, 0);

	return 0;

error:
	if (dev) {

		kref_put(&dev->kref, dlfb_free); 
		kref_put(&dev->kref, dlfb_free); 

		
	}

	return retval;
}

static void dlfb_init_framebuffer_work(struct work_struct *work)
{
	struct dlfb_data *dev = container_of(work, struct dlfb_data,
					     init_framebuffer_work.work);
	struct fb_info *info;
	int retval;
	int i;

	
	info = framebuffer_alloc(0, dev->gdev);
	if (!info) {
		retval = -ENOMEM;
		pr_err("framebuffer_alloc failed\n");
		goto error;
	}

	dev->info = info;
	info->par = dev;
	info->pseudo_palette = dev->pseudo_palette;
	info->fbops = &dlfb_ops;

	retval = fb_alloc_cmap(&info->cmap, 256, 0);
	if (retval < 0) {
		pr_err("fb_alloc_cmap failed %x\n", retval);
		goto error;
	}

	INIT_DELAYED_WORK(&dev->free_framebuffer_work,
			  dlfb_free_framebuffer_work);

	INIT_LIST_HEAD(&info->modelist);

	retval = dlfb_setup_modes(dev, info, NULL, 0);
	if (retval != 0) {
		pr_err("unable to find common mode for display and adapter\n");
		goto error;
	}

	

	atomic_set(&dev->usb_active, 1);
	dlfb_select_std_channel(dev);

	dlfb_ops_check_var(&info->var, info);
	dlfb_ops_set_par(info);

	retval = register_framebuffer(info);
	if (retval < 0) {
		pr_err("register_framebuffer failed %d\n", retval);
		goto error;
	}

	for (i = 0; i < ARRAY_SIZE(fb_device_attrs); i++) {
		retval = device_create_file(info->dev, &fb_device_attrs[i]);
		if (retval) {
			pr_warn("device_create_file failed %d\n", retval);
		}
	}

	retval = device_create_bin_file(info->dev, &edid_attr);
	if (retval) {
		pr_warn("device_create_bin_file failed %d\n", retval);
	}

	pr_info("DisplayLink USB device /dev/fb%d attached. %dx%d resolution."
			" Using %dK framebuffer memory\n", info->node,
			info->var.xres, info->var.yres,
			((dev->backing_buffer) ?
			info->fix.smem_len * 2 : info->fix.smem_len) >> 10);
	return;

error:
	dlfb_free_framebuffer(dev);
}

static void dlfb_usb_disconnect(struct usb_interface *interface)
{
	struct dlfb_data *dev;
	struct fb_info *info;
	int i;

	dev = usb_get_intfdata(interface);
	info = dev->info;

	pr_info("USB disconnect starting\n");

	
	dev->virtualized = true;

	
	atomic_set(&dev->usb_active, 0);

	
	dlfb_free_urb_list(dev);

	if (info) {
		
		for (i = 0; i < ARRAY_SIZE(fb_device_attrs); i++)
			device_remove_file(info->dev, &fb_device_attrs[i]);
		device_remove_bin_file(info->dev, &edid_attr);
		unlink_framebuffer(info);
	}

	usb_set_intfdata(interface, NULL);
	dev->udev = NULL;
	dev->gdev = NULL;

	
	if (dev->fb_count == 0)
		schedule_delayed_work(&dev->free_framebuffer_work, 0);

	
	kref_put(&dev->kref, dlfb_free);

	

	return;
}

static struct usb_driver dlfb_driver = {
	.name = "udlfb",
	.probe = dlfb_usb_probe,
	.disconnect = dlfb_usb_disconnect,
	.id_table = id_table,
};

module_usb_driver(dlfb_driver);

static void dlfb_urb_completion(struct urb *urb)
{
	struct urb_node *unode = urb->context;
	struct dlfb_data *dev = unode->dev;
	unsigned long flags;

	
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN)) {
			pr_err("%s - nonzero write bulk status received: %d\n",
				__func__, urb->status);
			atomic_set(&dev->lost_pixels, 1);
		}
	}

	urb->transfer_buffer_length = dev->urbs.size; 

	spin_lock_irqsave(&dev->urbs.lock, flags);
	list_add_tail(&unode->entry, &dev->urbs.list);
	dev->urbs.available++;
	spin_unlock_irqrestore(&dev->urbs.lock, flags);

	if (fb_defio)
		schedule_delayed_work(&unode->release_urb_work, 0);
	else
		up(&dev->urbs.limit_sem);
}

static void dlfb_free_urb_list(struct dlfb_data *dev)
{
	int count = dev->urbs.count;
	struct list_head *node;
	struct urb_node *unode;
	struct urb *urb;
	int ret;
	unsigned long flags;

	pr_notice("Freeing all render urbs\n");

	
	while (count--) {

		
		ret = down_interruptible(&dev->urbs.limit_sem);
		if (ret)
			break;

		spin_lock_irqsave(&dev->urbs.lock, flags);

		node = dev->urbs.list.next; 
		list_del_init(node);

		spin_unlock_irqrestore(&dev->urbs.lock, flags);

		unode = list_entry(node, struct urb_node, entry);
		urb = unode->urb;

		
		usb_free_coherent(urb->dev, dev->urbs.size,
				  urb->transfer_buffer, urb->transfer_dma);
		usb_free_urb(urb);
		kfree(node);
	}

	dev->urbs.count = 0;
}

static int dlfb_alloc_urb_list(struct dlfb_data *dev, int count, size_t size)
{
	int i = 0;
	struct urb *urb;
	struct urb_node *unode;
	char *buf;

	spin_lock_init(&dev->urbs.lock);

	dev->urbs.size = size;
	INIT_LIST_HEAD(&dev->urbs.list);

	while (i < count) {
		unode = kzalloc(sizeof(struct urb_node), GFP_KERNEL);
		if (!unode)
			break;
		unode->dev = dev;

		INIT_DELAYED_WORK(&unode->release_urb_work,
			  dlfb_release_urb_work);

		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			kfree(unode);
			break;
		}
		unode->urb = urb;

		buf = usb_alloc_coherent(dev->udev, MAX_TRANSFER, GFP_KERNEL,
					 &urb->transfer_dma);
		if (!buf) {
			kfree(unode);
			usb_free_urb(urb);
			break;
		}

		
		usb_fill_bulk_urb(urb, dev->udev, usb_sndbulkpipe(dev->udev, 1),
			buf, size, dlfb_urb_completion, unode);
		urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

		list_add_tail(&unode->entry, &dev->urbs.list);

		i++;
	}

	sema_init(&dev->urbs.limit_sem, i);
	dev->urbs.count = i;
	dev->urbs.available = i;

	pr_notice("allocated %d %d byte urbs\n", i, (int) size);

	return i;
}

static struct urb *dlfb_get_urb(struct dlfb_data *dev)
{
	int ret = 0;
	struct list_head *entry;
	struct urb_node *unode;
	struct urb *urb = NULL;
	unsigned long flags;

	
	ret = down_timeout(&dev->urbs.limit_sem, GET_URB_TIMEOUT);
	if (ret) {
		atomic_set(&dev->lost_pixels, 1);
		pr_warn("wait for urb interrupted: %x available: %d\n",
		       ret, dev->urbs.available);
		goto error;
	}

	spin_lock_irqsave(&dev->urbs.lock, flags);

	BUG_ON(list_empty(&dev->urbs.list)); 
	entry = dev->urbs.list.next;
	list_del_init(entry);
	dev->urbs.available--;

	spin_unlock_irqrestore(&dev->urbs.lock, flags);

	unode = list_entry(entry, struct urb_node, entry);
	urb = unode->urb;

error:
	return urb;
}

static int dlfb_submit_urb(struct dlfb_data *dev, struct urb *urb, size_t len)
{
	int ret;

	BUG_ON(len > dev->urbs.size);

	urb->transfer_buffer_length = len; 
	ret = usb_submit_urb(urb, GFP_KERNEL);
	if (ret) {
		dlfb_urb_completion(urb); 
		atomic_set(&dev->lost_pixels, 1);
		pr_err("usb_submit_urb error %x\n", ret);
	}
	return ret;
}

module_param(console, bool, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP);
MODULE_PARM_DESC(console, "Allow fbcon to open framebuffer");

module_param(fb_defio, bool, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP);
MODULE_PARM_DESC(fb_defio, "Page fault detection of mmap writes");

module_param(shadow, bool, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP);
MODULE_PARM_DESC(shadow, "Shadow vid mem. Disable to save mem but lose perf");

module_param(pixel_limit, int, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP);
MODULE_PARM_DESC(pixel_limit, "Force limit on max mode (in x*y pixels)");

MODULE_AUTHOR("Roberto De Ioris <roberto@unbit.it>, "
	      "Jaya Kumar <jayakumar.lkml@gmail.com>, "
	      "Bernie Thompson <bernie@plugable.com>");
MODULE_DESCRIPTION("DisplayLink kernel framebuffer driver");
MODULE_LICENSE("GPL");

