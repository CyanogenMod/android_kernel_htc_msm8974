#ifndef __IDE_GD_H
#define __IDE_GD_H

#define DRV_NAME "ide-gd"
#define PFX DRV_NAME ": "

#define IDE_GD_DEBUG_LOG	0

#if IDE_GD_DEBUG_LOG
#define ide_debug_log(lvl, fmt, args...) __ide_debug_log(lvl, fmt, ## args)
#else
#define ide_debug_log(lvl, fmt, args...) do {} while (0)
#endif

struct ide_disk_obj {
	ide_drive_t		*drive;
	struct ide_driver	*driver;
	struct gendisk		*disk;
	struct device		dev;
	unsigned int		openers;	

	
	struct ide_atapi_pc queued_pc;

	
	u8 sense_key, asc, ascq;

	int progress_indication;

	
	
	int blocks, block_size, bs_factor;
	
	u8 cap_desc[8];
	
	u8 flexible_disk_page[32];
};

sector_t ide_gd_capacity(ide_drive_t *);

#endif 
