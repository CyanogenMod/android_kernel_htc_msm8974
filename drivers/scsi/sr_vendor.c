
#include <linux/cdrom.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/bcd.h>
#include <linux/blkdev.h>
#include <linux/slab.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_ioctl.h>

#include "sr.h"

#if 0
#define DEBUG
#endif


#define VENDOR_SCSI3           1	

#define VENDOR_NEC             2
#define VENDOR_TOSHIBA         3
#define VENDOR_WRITER          4	

#define VENDOR_TIMEOUT	30*HZ

void sr_vendor_init(Scsi_CD *cd)
{
#ifndef CONFIG_BLK_DEV_SR_VENDOR
	cd->vendor = VENDOR_SCSI3;
#else
	const char *vendor = cd->device->vendor;
	const char *model = cd->device->model;
	
	
	cd->vendor = VENDOR_SCSI3;
	if (cd->readcd_known)
		
		return;

	if (cd->device->type == TYPE_WORM) {
		cd->vendor = VENDOR_WRITER;

	} else if (!strncmp(vendor, "NEC", 3)) {
		cd->vendor = VENDOR_NEC;
		if (!strncmp(model, "CD-ROM DRIVE:25", 15) ||
		    !strncmp(model, "CD-ROM DRIVE:36", 15) ||
		    !strncmp(model, "CD-ROM DRIVE:83", 15) ||
		    !strncmp(model, "CD-ROM DRIVE:84 ", 16)
#if 0
		    || !strncmp(model, "CD-ROM DRIVE:500", 16)
#endif
		    )
			
			cd->cdi.mask |= CDC_MULTI_SESSION;

	} else if (!strncmp(vendor, "TOSHIBA", 7)) {
		cd->vendor = VENDOR_TOSHIBA;

	}
#endif
}



int sr_set_blocklength(Scsi_CD *cd, int blocklength)
{
	unsigned char *buffer;	
	struct packet_command cgc;
	struct ccs_modesel_head *modesel;
	int rc, density = 0;

#ifdef CONFIG_BLK_DEV_SR_VENDOR
	if (cd->vendor == VENDOR_TOSHIBA)
		density = (blocklength > 2048) ? 0x81 : 0x83;
#endif

	buffer = kmalloc(512, GFP_KERNEL | GFP_DMA);
	if (!buffer)
		return -ENOMEM;

#ifdef DEBUG
	printk("%s: MODE SELECT 0x%x/%d\n", cd->cdi.name, density, blocklength);
#endif
	memset(&cgc, 0, sizeof(struct packet_command));
	cgc.cmd[0] = MODE_SELECT;
	cgc.cmd[1] = (1 << 4);
	cgc.cmd[4] = 12;
	modesel = (struct ccs_modesel_head *) buffer;
	memset(modesel, 0, sizeof(*modesel));
	modesel->block_desc_length = 0x08;
	modesel->density = density;
	modesel->block_length_med = (blocklength >> 8) & 0xff;
	modesel->block_length_lo = blocklength & 0xff;
	cgc.buffer = buffer;
	cgc.buflen = sizeof(*modesel);
	cgc.data_direction = DMA_TO_DEVICE;
	cgc.timeout = VENDOR_TIMEOUT;
	if (0 == (rc = sr_do_ioctl(cd, &cgc))) {
		cd->device->sector_size = blocklength;
	}
#ifdef DEBUG
	else
		printk("%s: switching blocklength to %d bytes failed\n",
		       cd->cdi.name, blocklength);
#endif
	kfree(buffer);
	return rc;
}


int sr_cd_check(struct cdrom_device_info *cdi)
{
	Scsi_CD *cd = cdi->handle;
	unsigned long sector;
	unsigned char *buffer;	
	struct packet_command cgc;
	int rc, no_multi;

	if (cd->cdi.mask & CDC_MULTI_SESSION)
		return 0;

	buffer = kmalloc(512, GFP_KERNEL | GFP_DMA);
	if (!buffer)
		return -ENOMEM;

	sector = 0;		
	no_multi = 0;		
	rc = 0;

	memset(&cgc, 0, sizeof(struct packet_command));

	switch (cd->vendor) {

	case VENDOR_SCSI3:
		cgc.cmd[0] = READ_TOC;
		cgc.cmd[8] = 12;
		cgc.cmd[9] = 0x40;
		cgc.buffer = buffer;
		cgc.buflen = 12;
		cgc.quiet = 1;
		cgc.data_direction = DMA_FROM_DEVICE;
		cgc.timeout = VENDOR_TIMEOUT;
		rc = sr_do_ioctl(cd, &cgc);
		if (rc != 0)
			break;
		if ((buffer[0] << 8) + buffer[1] < 0x0a) {
			printk(KERN_INFO "%s: Hmm, seems the drive "
			   "doesn't support multisession CD's\n", cd->cdi.name);
			no_multi = 1;
			break;
		}
		sector = buffer[11] + (buffer[10] << 8) +
		    (buffer[9] << 16) + (buffer[8] << 24);
		if (buffer[6] <= 1) {
			
			sector = 0;
		}
		break;

#ifdef CONFIG_BLK_DEV_SR_VENDOR
	case VENDOR_NEC:{
			unsigned long min, sec, frame;
			cgc.cmd[0] = 0xde;
			cgc.cmd[1] = 0x03;
			cgc.cmd[2] = 0xb0;
			cgc.buffer = buffer;
			cgc.buflen = 0x16;
			cgc.quiet = 1;
			cgc.data_direction = DMA_FROM_DEVICE;
			cgc.timeout = VENDOR_TIMEOUT;
			rc = sr_do_ioctl(cd, &cgc);
			if (rc != 0)
				break;
			if (buffer[14] != 0 && buffer[14] != 0xb0) {
				printk(KERN_INFO "%s: Hmm, seems the cdrom "
				       "doesn't support multisession CD's\n",
				       cd->cdi.name);
				no_multi = 1;
				break;
			}
			min = bcd2bin(buffer[15]);
			sec = bcd2bin(buffer[16]);
			frame = bcd2bin(buffer[17]);
			sector = min * CD_SECS * CD_FRAMES + sec * CD_FRAMES + frame;
			break;
		}

	case VENDOR_TOSHIBA:{
			unsigned long min, sec, frame;

			cgc.cmd[0] = 0xc7;
			cgc.cmd[1] = 0x03;
			cgc.buffer = buffer;
			cgc.buflen = 4;
			cgc.quiet = 1;
			cgc.data_direction = DMA_FROM_DEVICE;
			cgc.timeout = VENDOR_TIMEOUT;
			rc = sr_do_ioctl(cd, &cgc);
			if (rc == -EINVAL) {
				printk(KERN_INFO "%s: Hmm, seems the drive "
				       "doesn't support multisession CD's\n",
				       cd->cdi.name);
				no_multi = 1;
				break;
			}
			if (rc != 0)
				break;
			min = bcd2bin(buffer[1]);
			sec = bcd2bin(buffer[2]);
			frame = bcd2bin(buffer[3]);
			sector = min * CD_SECS * CD_FRAMES + sec * CD_FRAMES + frame;
			if (sector)
				sector -= CD_MSF_OFFSET;
			sr_set_blocklength(cd, 2048);
			break;
		}

	case VENDOR_WRITER:
		cgc.cmd[0] = READ_TOC;
		cgc.cmd[8] = 0x04;
		cgc.cmd[9] = 0x40;
		cgc.buffer = buffer;
		cgc.buflen = 0x04;
		cgc.quiet = 1;
		cgc.data_direction = DMA_FROM_DEVICE;
		cgc.timeout = VENDOR_TIMEOUT;
		rc = sr_do_ioctl(cd, &cgc);
		if (rc != 0) {
			break;
		}
		if ((rc = buffer[2]) == 0) {
			printk(KERN_WARNING
			       "%s: No finished session\n", cd->cdi.name);
			break;
		}
		cgc.cmd[0] = READ_TOC;	
		cgc.cmd[6] = rc & 0x7f;	
		cgc.cmd[8] = 0x0c;
		cgc.cmd[9] = 0x40;
		cgc.buffer = buffer;
		cgc.buflen = 12;
		cgc.quiet = 1;
		cgc.data_direction = DMA_FROM_DEVICE;
		cgc.timeout = VENDOR_TIMEOUT;
		rc = sr_do_ioctl(cd, &cgc);
		if (rc != 0) {
			break;
		}
		sector = buffer[11] + (buffer[10] << 8) +
		    (buffer[9] << 16) + (buffer[8] << 24);
		break;
#endif				

	default:
		
		printk(KERN_WARNING
		   "%s: unknown vendor code (%i), not initialized ?\n",
		       cd->cdi.name, cd->vendor);
		sector = 0;
		no_multi = 1;
		break;
	}
	cd->ms_offset = sector;
	cd->xa_flag = 0;
	if (CDS_AUDIO != sr_disk_status(cdi) && 1 == sr_is_xa(cd))
		cd->xa_flag = 1;

	if (2048 != cd->device->sector_size) {
		sr_set_blocklength(cd, 2048);
	}
	if (no_multi)
		cdi->mask |= CDC_MULTI_SESSION;

#ifdef DEBUG
	if (sector)
		printk(KERN_DEBUG "%s: multisession offset=%lu\n",
		       cd->cdi.name, sector);
#endif
	kfree(buffer);
	return rc;
}
