#ifndef _IDE_H
#define _IDE_H
/*
 *  linux/include/linux/ide.h
 *
 *  Copyright (C) 1994-2002  Linus Torvalds & authors
 */

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/ata.h>
#include <linux/blkdev.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/bio.h>
#include <linux/pci.h>
#include <linux/completion.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#ifdef CONFIG_BLK_DEV_IDEACPI
#include <acpi/acpi.h>
#endif
#include <asm/byteorder.h>
#include <asm/io.h>

#include <linux/cdrom.h>

#if defined(CONFIG_CRIS) || defined(CONFIG_FRV) || defined(CONFIG_MN10300)
# define SUPPORT_VLB_SYNC 0
#else
# define SUPPORT_VLB_SYNC 1
#endif

#define IDE_DEFAULT_MAX_FAILURES	1
#define ERROR_MAX	8	
#define ERROR_RESET	3	
#define ERROR_RECAL	1	

struct device;

enum {
	IDE_DRV_ERROR_GENERAL	= 101,
	IDE_DRV_ERROR_FILEMARK	= 102,
	IDE_DRV_ERROR_EOD	= 103,
};

#define IDE_NR_PORTS		(10)

struct ide_io_ports {
	unsigned long	data_addr;

	union {
		unsigned long error_addr;	
		unsigned long feature_addr;	
	};

	unsigned long	nsect_addr;
	unsigned long	lbal_addr;
	unsigned long	lbam_addr;
	unsigned long	lbah_addr;

	unsigned long	device_addr;

	union {
		unsigned long status_addr;	
		unsigned long command_addr;	
	};

	unsigned long	ctl_addr;

	unsigned long	irq_addr;
};

#define OK_STAT(stat,good,bad)	(((stat)&((good)|(bad)))==(good))

#define BAD_R_STAT	(ATA_BUSY | ATA_ERR)
#define BAD_W_STAT	(BAD_R_STAT | ATA_DF)
#define BAD_STAT	(BAD_R_STAT | ATA_DRQ)
#define DRIVE_READY	(ATA_DRDY | ATA_DSC)

#define BAD_CRC		(ATA_ABORTED | ATA_ICRC)

#define SATA_NR_PORTS		(3)	

#define SATA_STATUS_OFFSET	(0)
#define SATA_ERROR_OFFSET	(1)
#define SATA_CONTROL_OFFSET	(2)

#define PRD_BYTES       8
#define PRD_ENTRIES	256

#define PARTN_BITS	6	
#define MAX_DRIVES	2	
#define SECTOR_SIZE	512

enum {
	
	WAIT_DRQ	= 1 * HZ,	
	
	WAIT_READY	= 5 * HZ,	
	
	WAIT_PIDENTIFY	= 10 * HZ,	
	
	WAIT_WORSTCASE	= 30 * HZ,	
	
	WAIT_CMD	= 10 * HZ,	
	
	WAIT_FLOPPY_CMD	= 50 * HZ,	
	WAIT_TAPE_CMD	= 900 * HZ,	
	
	WAIT_MIN_SLEEP	= HZ / 50,	
};

#define REQ_DRIVE_RESET		0x20
#define REQ_DEVSET_EXEC		0x21
#define REQ_PARK_HEADS		0x22
#define REQ_UNPARK_HEADS	0x23

enum {		ide_unknown,	ide_generic,	ide_pci,
		ide_cmd640,	ide_dtc2278,	ide_ali14xx,
		ide_qd65xx,	ide_umc8672,	ide_ht6560b,
		ide_4drives,	ide_pmac,	ide_acorn,
		ide_au1xxx,	ide_palm3710
};

typedef u8 hwif_chipset_t;

struct ide_hw {
	union {
		struct ide_io_ports	io_ports;
		unsigned long		io_ports_array[IDE_NR_PORTS];
	};

	int		irq;			
	struct device	*dev, *parent;
	unsigned long	config;
};

static inline void ide_std_init_ports(struct ide_hw *hw,
				      unsigned long io_addr,
				      unsigned long ctl_addr)
{
	unsigned int i;

	for (i = 0; i <= 7; i++)
		hw->io_ports_array[i] = io_addr++;

	hw->io_ports.ctl_addr = ctl_addr;
}

#define MAX_HWIFS	10


#define ide_scsi	0x21
#define ide_disk	0x20
#define ide_optical	0x7
#define ide_cdrom	0x5
#define ide_tape	0x1
#define ide_floppy	0x0

enum {
	IDE_SFLAG_SET_GEOMETRY		= (1 << 0),
	IDE_SFLAG_RECALIBRATE		= (1 << 1),
	IDE_SFLAG_SET_MULTMODE		= (1 << 2),
};

typedef enum {
	ide_stopped,	
	ide_started,	
} ide_startstop_t;

enum {
	IDE_VALID_ERROR 		= (1 << 1),
	IDE_VALID_FEATURE		= IDE_VALID_ERROR,
	IDE_VALID_NSECT 		= (1 << 2),
	IDE_VALID_LBAL			= (1 << 3),
	IDE_VALID_LBAM			= (1 << 4),
	IDE_VALID_LBAH			= (1 << 5),
	IDE_VALID_DEVICE		= (1 << 6),
	IDE_VALID_LBA			= IDE_VALID_LBAL |
					  IDE_VALID_LBAM |
					  IDE_VALID_LBAH,
	IDE_VALID_OUT_TF		= IDE_VALID_FEATURE |
					  IDE_VALID_NSECT |
					  IDE_VALID_LBA,
	IDE_VALID_IN_TF 		= IDE_VALID_NSECT |
					  IDE_VALID_LBA,
	IDE_VALID_OUT_HOB		= IDE_VALID_OUT_TF,
	IDE_VALID_IN_HOB		= IDE_VALID_ERROR |
					  IDE_VALID_NSECT |
					  IDE_VALID_LBA,
};

enum {
	IDE_TFLAG_LBA48			= (1 << 0),
	IDE_TFLAG_WRITE			= (1 << 1),
	IDE_TFLAG_CUSTOM_HANDLER	= (1 << 2),
	IDE_TFLAG_DMA_PIO_FALLBACK	= (1 << 3),
	
	IDE_TFLAG_IO_16BIT		= (1 << 4),
	
	IDE_TFLAG_DYN			= (1 << 5),
	IDE_TFLAG_FS			= (1 << 6),
	IDE_TFLAG_MULTI_PIO		= (1 << 7),
	IDE_TFLAG_SET_XFER		= (1 << 8),
};

enum {
	IDE_FTFLAG_FLAGGED		= (1 << 0),
	IDE_FTFLAG_SET_IN_FLAGS		= (1 << 1),
	IDE_FTFLAG_OUT_DATA		= (1 << 2),
	IDE_FTFLAG_IN_DATA		= (1 << 3),
};

struct ide_taskfile {
	u8	data;		
	union {			
		u8 error;	
		u8 feature;	
	};
	u8	nsect;		
	u8	lbal;		
	u8	lbam;		
	u8	lbah;		
	u8	device;		
	union {			
		u8 status;	
		u8 command;	
	};
};

struct ide_cmd {
	struct ide_taskfile	tf;
	struct ide_taskfile	hob;
	struct {
		struct {
			u8		tf;
			u8		hob;
		} out, in;
	} valid;

	u16			tf_flags;
	u8			ftf_flags;	
	int			protocol;

	int			sg_nents;	  
	int			orig_sg_nents;
	int			sg_dma_direction; 

	unsigned int		nbytes;
	unsigned int		nleft;
	unsigned int		last_xfer_len;

	struct scatterlist	*cursg;
	unsigned int		cursg_ofs;

	struct request		*rq;		
};

enum {
	
	PC_FLAG_ABORT			= (1 << 0),
	PC_FLAG_SUPPRESS_ERROR		= (1 << 1),
	PC_FLAG_WAIT_FOR_DSC		= (1 << 2),
	PC_FLAG_DMA_OK			= (1 << 3),
	PC_FLAG_DMA_IN_PROGRESS		= (1 << 4),
	PC_FLAG_DMA_ERROR		= (1 << 5),
	PC_FLAG_WRITING			= (1 << 6),
};

#define ATAPI_WAIT_PC		(60 * HZ)

struct ide_atapi_pc {
	
	u8 c[12];
	
	int retries;
	int error;

	
	int req_xfer;

	
	struct request *rq;

	unsigned long flags;

	unsigned long timeout;
};

struct ide_devset;
struct ide_driver;

#ifdef CONFIG_BLK_DEV_IDEACPI
struct ide_acpi_drive_link;
struct ide_acpi_hwif_link;
#endif

struct ide_drive_s;

struct ide_disk_ops {
	int		(*check)(struct ide_drive_s *, const char *);
	int		(*get_capacity)(struct ide_drive_s *);
	void		(*unlock_native_capacity)(struct ide_drive_s *);
	void		(*setup)(struct ide_drive_s *);
	void		(*flush)(struct ide_drive_s *);
	int		(*init_media)(struct ide_drive_s *, struct gendisk *);
	int		(*set_doorlock)(struct ide_drive_s *, struct gendisk *,
					int);
	ide_startstop_t	(*do_request)(struct ide_drive_s *, struct request *,
				      sector_t);
	int		(*ioctl)(struct ide_drive_s *, struct block_device *,
				 fmode_t, unsigned int, unsigned long);
};

enum {
	IDE_AFLAG_DRQ_INTERRUPT		= (1 << 0),

	
	
	IDE_AFLAG_NO_EJECT		= (1 << 1),
	
	IDE_AFLAG_PRE_ATAPI12		= (1 << 2),
	
	IDE_AFLAG_TOCADDR_AS_BCD	= (1 << 3),
	
	IDE_AFLAG_TOCTRACKS_AS_BCD	= (1 << 4),
	
	IDE_AFLAG_TOC_VALID		= (1 << 6),
	
	IDE_AFLAG_DOOR_LOCKED		= (1 << 7),
	
	IDE_AFLAG_NO_SPEED_SELECT	= (1 << 8),
	IDE_AFLAG_VERTOS_300_SSD	= (1 << 9),
	IDE_AFLAG_VERTOS_600_ESD	= (1 << 10),
	IDE_AFLAG_SANYO_3CD		= (1 << 11),
	IDE_AFLAG_FULL_CAPS_PAGE	= (1 << 12),
	IDE_AFLAG_PLAY_AUDIO_OK		= (1 << 13),
	IDE_AFLAG_LE_SPEED_FIELDS	= (1 << 14),

	
	
	IDE_AFLAG_CLIK_DRIVE		= (1 << 15),
	
	IDE_AFLAG_ZIP_DRIVE		= (1 << 16),
	
	IDE_AFLAG_SRFP			= (1 << 17),

	
	IDE_AFLAG_IGNORE_DSC		= (1 << 18),
	
	IDE_AFLAG_ADDRESS_VALID		= (1 <<	19),
	
	IDE_AFLAG_BUSY			= (1 << 20),
	
	IDE_AFLAG_DETECT_BS		= (1 << 21),
	
	IDE_AFLAG_FILEMARK		= (1 << 22),
	
	IDE_AFLAG_MEDIUM_PRESENT	= (1 << 23),

	IDE_AFLAG_NO_AUTOCLOSE		= (1 << 24),
};

enum {
	
	IDE_DFLAG_KEEP_SETTINGS		= (1 << 0),
	
	IDE_DFLAG_USING_DMA		= (1 << 1),
	
	IDE_DFLAG_UNMASK		= (1 << 2),
	
	IDE_DFLAG_NOFLUSH		= (1 << 3),
	
	IDE_DFLAG_DSC_OVERLAP		= (1 << 4),
	
	IDE_DFLAG_NICE1			= (1 << 5),
	
	IDE_DFLAG_PRESENT		= (1 << 6),
	
	IDE_DFLAG_NOHPA			= (1 << 7),
	
	IDE_DFLAG_ID_READ		= (1 << 8),
	IDE_DFLAG_NOPROBE		= (1 << 9),
	
	IDE_DFLAG_REMOVABLE		= (1 << 10),
	
	IDE_DFLAG_ATTACH		= (1 << 11),
	IDE_DFLAG_FORCED_GEOM		= (1 << 12),
	
	IDE_DFLAG_NO_UNMASK		= (1 << 13),
	
	IDE_DFLAG_NO_IO_32BIT		= (1 << 14),
	
	IDE_DFLAG_DOORLOCKING		= (1 << 15),
	
	IDE_DFLAG_NODMA			= (1 << 16),
	
	IDE_DFLAG_BLOCKED		= (1 << 17),
	
	IDE_DFLAG_SLEEPING		= (1 << 18),
	IDE_DFLAG_POST_RESET		= (1 << 19),
	IDE_DFLAG_UDMA33_WARNED		= (1 << 20),
	IDE_DFLAG_LBA48			= (1 << 21),
	
	IDE_DFLAG_WCACHE		= (1 << 22),
	
	IDE_DFLAG_NOWERR		= (1 << 23),
	
	IDE_DFLAG_DMA_PIO_RETRY		= (1 << 24),
	IDE_DFLAG_LBA			= (1 << 25),
	
	IDE_DFLAG_NO_UNLOAD		= (1 << 26),
	
	IDE_DFLAG_PARKED		= (1 << 27),
	IDE_DFLAG_MEDIA_CHANGED		= (1 << 28),
	
	IDE_DFLAG_WP			= (1 << 29),
	IDE_DFLAG_FORMAT_IN_PROGRESS	= (1 << 30),
	IDE_DFLAG_NIEN_QUIRK		= (1 << 31),
};

struct ide_drive_s {
	char		name[4];	
        char            driver_req[10];	

	struct request_queue	*queue;	

	struct request		*rq;	
	void		*driver_data;	
	u16			*id;	
#ifdef CONFIG_IDE_PROC_FS
	struct proc_dir_entry *proc;	
	const struct ide_proc_devset *settings; 
#endif
	struct hwif_s		*hwif;	

	const struct ide_disk_ops *disk_ops;

	unsigned long dev_flags;

	unsigned long sleep;		
	unsigned long timeout;		

	u8	special_flags;		

	u8	select;			
	u8	retry_pio;		
	u8	waiting_for_dma;	
	u8	dma;			

        u8	init_speed;	
        u8	current_speed;	
	u8	desired_speed;	
	u8	pio_mode;	
	u8	dma_mode;	
	u8	dn;		
	u8	acoustic;	
	u8	media;		
	u8	ready_stat;	
	u8	mult_count;	
	u8	mult_req;	
	u8	io_32bit;	
	u8	bad_wstat;	
	u8	head;		
	u8	sect;		
	u8	bios_head;	
	u8	bios_sect;	

	
	u8 pc_delay;

	unsigned int	bios_cyl;	
	unsigned int	cyl;		
	void		*drive_data;	
	unsigned int	failures;	
	unsigned int	max_failures;	
	u64		probed_capacity;
	u64		capacity64;	

	int		lun;		
	int		crc_count;	

	unsigned long	debug_mask;	

#ifdef CONFIG_BLK_DEV_IDEACPI
	struct ide_acpi_drive_link *acpidata;
#endif
	struct list_head list;
	struct device	gendev;
	struct completion gendev_rel_comp;	

	
	struct ide_atapi_pc *pc;

	
	struct ide_atapi_pc *failed_pc;

	
	int  (*pc_callback)(struct ide_drive_s *, int);

	ide_startstop_t (*irq_handler)(struct ide_drive_s *);

	unsigned long atapi_flags;

	struct ide_atapi_pc request_sense_pc;

	
	bool sense_rq_armed;
	struct request sense_rq;
	struct request_sense sense_data;
};

typedef struct ide_drive_s ide_drive_t;

#define to_ide_device(dev)		container_of(dev, ide_drive_t, gendev)

#define to_ide_drv(obj, cont_type)	\
	container_of(obj, struct cont_type, dev)

#define ide_drv_g(disk, cont_type)	\
	container_of((disk)->private_data, struct cont_type, driver)

struct ide_port_info;

struct ide_tp_ops {
	void	(*exec_command)(struct hwif_s *, u8);
	u8	(*read_status)(struct hwif_s *);
	u8	(*read_altstatus)(struct hwif_s *);
	void	(*write_devctl)(struct hwif_s *, u8);

	void	(*dev_select)(ide_drive_t *);
	void	(*tf_load)(ide_drive_t *, struct ide_taskfile *, u8);
	void	(*tf_read)(ide_drive_t *, struct ide_taskfile *, u8);

	void	(*input_data)(ide_drive_t *, struct ide_cmd *,
			      void *, unsigned int);
	void	(*output_data)(ide_drive_t *, struct ide_cmd *,
			       void *, unsigned int);
};

extern const struct ide_tp_ops default_tp_ops;

struct ide_port_ops {
	void	(*init_dev)(ide_drive_t *);
	void	(*set_pio_mode)(struct hwif_s *, ide_drive_t *);
	void	(*set_dma_mode)(struct hwif_s *, ide_drive_t *);
	int	(*reset_poll)(ide_drive_t *);
	void	(*pre_reset)(ide_drive_t *);
	void	(*resetproc)(ide_drive_t *);
	void	(*maskproc)(ide_drive_t *, int);
	void	(*quirkproc)(ide_drive_t *);
	void	(*clear_irq)(ide_drive_t *);
	int	(*test_irq)(struct hwif_s *);

	u8	(*mdma_filter)(ide_drive_t *);
	u8	(*udma_filter)(ide_drive_t *);

	u8	(*cable_detect)(struct hwif_s *);
};

struct ide_dma_ops {
	void	(*dma_host_set)(struct ide_drive_s *, int);
	int	(*dma_setup)(struct ide_drive_s *, struct ide_cmd *);
	void	(*dma_start)(struct ide_drive_s *);
	int	(*dma_end)(struct ide_drive_s *);
	int	(*dma_test_irq)(struct ide_drive_s *);
	void	(*dma_lost_irq)(struct ide_drive_s *);
	
	int	(*dma_check)(struct ide_drive_s *, struct ide_cmd *);
	int	(*dma_timer_expiry)(struct ide_drive_s *);
	void	(*dma_clear)(struct ide_drive_s *);
	u8	(*dma_sff_read_status)(struct hwif_s *);
};

enum {
	IDE_PFLAG_PROBING		= (1 << 0),
};

struct ide_host;

typedef struct hwif_s {
	struct hwif_s *mate;		
	struct proc_dir_entry *proc;	

	struct ide_host *host;

	char name[6];			

	struct ide_io_ports	io_ports;

	unsigned long	sata_scr[SATA_NR_PORTS];

	ide_drive_t	*devices[MAX_DRIVES + 1];

	unsigned long	port_flags;

	u8 major;	
	u8 index;	
	u8 channel;	

	u32 host_flags;

	u8 pio_mask;

	u8 ultra_mask;
	u8 mwdma_mask;
	u8 swdma_mask;

	u8 cbl;		

	hwif_chipset_t chipset;	

	struct device *dev;

	void (*rw_disk)(ide_drive_t *, struct request *);

	const struct ide_tp_ops		*tp_ops;
	const struct ide_port_ops	*port_ops;
	const struct ide_dma_ops	*dma_ops;

	
	unsigned int	*dmatable_cpu;
	
	dma_addr_t	dmatable_dma;

	
	int prd_max_nents;
	
	int prd_ent_size;

	
	struct scatterlist *sg_table;
	int sg_max_nents;		

	struct ide_cmd cmd;		

	int		rqsize;		
	int		irq;		

	unsigned long	dma_base;	

	unsigned long	config_data;	
	unsigned long	select_data;	

	unsigned long	extra_base;	
	unsigned	extra_ports;	

	unsigned	present    : 1;	
	unsigned	busy	   : 1; 

	struct device		gendev;
	struct device		*portdev;

	struct completion gendev_rel_comp; 

	void		*hwif_data;	

#ifdef CONFIG_BLK_DEV_IDEACPI
	struct ide_acpi_hwif_link *acpidata;
#endif

	
	ide_startstop_t	(*handler)(ide_drive_t *);

	
	unsigned int polling : 1;

	
	ide_drive_t *cur_dev;

	
	struct request *rq;

	
	struct timer_list timer;
	
	unsigned long poll_timeout;
	
	int (*expiry)(ide_drive_t *);

	int req_gen;
	int req_gen_timer;

	spinlock_t lock;
} ____cacheline_internodealigned_in_smp ide_hwif_t;

#define MAX_HOST_PORTS 4

struct ide_host {
	ide_hwif_t	*ports[MAX_HOST_PORTS + 1];
	unsigned int	n_ports;
	struct device	*dev[2];

	int		(*init_chipset)(struct pci_dev *);

	void		(*get_lock)(irq_handler_t, void *);
	void		(*release_lock)(void);

	irq_handler_t	irq_handler;

	unsigned long	host_flags;

	int		irq_flags;

	void		*host_priv;
	ide_hwif_t	*cur_port;	

	
	volatile unsigned long	host_busy;
};

#define IDE_HOST_BUSY 0

typedef ide_startstop_t (ide_handler_t)(ide_drive_t *);
typedef int (ide_expiry_t)(ide_drive_t *);

typedef void (xfer_func_t)(ide_drive_t *, struct ide_cmd *, void *, unsigned);

extern struct mutex ide_setting_mtx;


#define DS_SYNC	(1 << 0)

struct ide_devset {
	int		(*get)(ide_drive_t *);
	int		(*set)(ide_drive_t *, int);
	unsigned int	flags;
};

#define __DEVSET(_flags, _get, _set) { \
	.flags	= _flags, \
	.get	= _get,	\
	.set	= _set,	\
}

#define ide_devset_get(name, field) \
static int get_##name(ide_drive_t *drive) \
{ \
	return drive->field; \
}

#define ide_devset_set(name, field) \
static int set_##name(ide_drive_t *drive, int arg) \
{ \
	drive->field = arg; \
	return 0; \
}

#define ide_devset_get_flag(name, flag) \
static int get_##name(ide_drive_t *drive) \
{ \
	return !!(drive->dev_flags & flag); \
}

#define ide_devset_set_flag(name, flag) \
static int set_##name(ide_drive_t *drive, int arg) \
{ \
	if (arg) \
		drive->dev_flags |= flag; \
	else \
		drive->dev_flags &= ~flag; \
	return 0; \
}

#define __IDE_DEVSET(_name, _flags, _get, _set) \
const struct ide_devset ide_devset_##_name = \
	__DEVSET(_flags, _get, _set)

#define IDE_DEVSET(_name, _flags, _get, _set) \
static __IDE_DEVSET(_name, _flags, _get, _set)

#define ide_devset_rw(_name, _func) \
IDE_DEVSET(_name, 0, get_##_func, set_##_func)

#define ide_devset_w(_name, _func) \
IDE_DEVSET(_name, 0, NULL, set_##_func)

#define ide_ext_devset_rw(_name, _func) \
__IDE_DEVSET(_name, 0, get_##_func, set_##_func)

#define ide_ext_devset_rw_sync(_name, _func) \
__IDE_DEVSET(_name, DS_SYNC, get_##_func, set_##_func)

#define ide_decl_devset(_name) \
extern const struct ide_devset ide_devset_##_name

ide_decl_devset(io_32bit);
ide_decl_devset(keepsettings);
ide_decl_devset(pio_mode);
ide_decl_devset(unmaskirq);
ide_decl_devset(using_dma);

#ifdef CONFIG_IDE_PROC_FS

#define ide_devset_rw_field(_name, _field) \
ide_devset_get(_name, _field); \
ide_devset_set(_name, _field); \
IDE_DEVSET(_name, DS_SYNC, get_##_name, set_##_name)

#define ide_devset_rw_flag(_name, _field) \
ide_devset_get_flag(_name, _field); \
ide_devset_set_flag(_name, _field); \
IDE_DEVSET(_name, DS_SYNC, get_##_name, set_##_name)

struct ide_proc_devset {
	const char		*name;
	const struct ide_devset	*setting;
	int			min, max;
	int			(*mulf)(ide_drive_t *);
	int			(*divf)(ide_drive_t *);
};

#define __IDE_PROC_DEVSET(_name, _min, _max, _mulf, _divf) { \
	.name = __stringify(_name), \
	.setting = &ide_devset_##_name, \
	.min = _min, \
	.max = _max, \
	.mulf = _mulf, \
	.divf = _divf, \
}

#define IDE_PROC_DEVSET(_name, _min, _max) \
__IDE_PROC_DEVSET(_name, _min, _max, NULL, NULL)

typedef struct {
	const char	*name;
	umode_t		mode;
	const struct file_operations *proc_fops;
} ide_proc_entry_t;

void proc_ide_create(void);
void proc_ide_destroy(void);
void ide_proc_register_port(ide_hwif_t *);
void ide_proc_port_register_devices(ide_hwif_t *);
void ide_proc_unregister_device(ide_drive_t *);
void ide_proc_unregister_port(ide_hwif_t *);
void ide_proc_register_driver(ide_drive_t *, struct ide_driver *);
void ide_proc_unregister_driver(ide_drive_t *, struct ide_driver *);

extern const struct file_operations ide_capacity_proc_fops;
extern const struct file_operations ide_geometry_proc_fops;
#else
static inline void proc_ide_create(void) { ; }
static inline void proc_ide_destroy(void) { ; }
static inline void ide_proc_register_port(ide_hwif_t *hwif) { ; }
static inline void ide_proc_port_register_devices(ide_hwif_t *hwif) { ; }
static inline void ide_proc_unregister_device(ide_drive_t *drive) { ; }
static inline void ide_proc_unregister_port(ide_hwif_t *hwif) { ; }
static inline void ide_proc_register_driver(ide_drive_t *drive,
					    struct ide_driver *driver) { ; }
static inline void ide_proc_unregister_driver(ide_drive_t *drive,
					      struct ide_driver *driver) { ; }
#endif

enum {
	
	IDE_DBG_FUNC =			(1 << 0),
	
	IDE_DBG_SENSE =			(1 << 1),
	
	IDE_DBG_PC =			(1 << 2),
	
	IDE_DBG_RQ =			(1 << 3),
	
	IDE_DBG_PROBE =			(1 << 4),
};

#define __ide_debug_log(lvl, fmt, args...)				\
{									\
	if (unlikely(drive->debug_mask & lvl))				\
		printk(KERN_INFO DRV_NAME ": %s: " fmt "\n",		\
					  __func__, ## args);		\
}

enum {
	IDE_PM_START_SUSPEND,
	IDE_PM_FLUSH_CACHE	= IDE_PM_START_SUSPEND,
	IDE_PM_STANDBY,

	IDE_PM_START_RESUME,
	IDE_PM_RESTORE_PIO	= IDE_PM_START_RESUME,
	IDE_PM_IDLE,
	IDE_PM_RESTORE_DMA,

	IDE_PM_COMPLETED,
};

int generic_ide_suspend(struct device *, pm_message_t);
int generic_ide_resume(struct device *);

void ide_complete_power_step(ide_drive_t *, struct request *);
ide_startstop_t ide_start_power_step(ide_drive_t *, struct request *);
void ide_complete_pm_rq(ide_drive_t *, struct request *);
void ide_check_pm_state(ide_drive_t *, struct request *);

struct ide_driver {
	const char			*version;
	ide_startstop_t	(*do_request)(ide_drive_t *, struct request *, sector_t);
	struct device_driver	gen_driver;
	int		(*probe)(ide_drive_t *);
	void		(*remove)(ide_drive_t *);
	void		(*resume)(ide_drive_t *);
	void		(*shutdown)(ide_drive_t *);
#ifdef CONFIG_IDE_PROC_FS
	ide_proc_entry_t *		(*proc_entries)(ide_drive_t *);
	const struct ide_proc_devset *	(*proc_devsets)(ide_drive_t *);
#endif
};

#define to_ide_driver(drv) container_of(drv, struct ide_driver, gen_driver)

int ide_device_get(ide_drive_t *);
void ide_device_put(ide_drive_t *);

struct ide_ioctl_devset {
	unsigned int	get_ioctl;
	unsigned int	set_ioctl;
	const struct ide_devset *setting;
};

int ide_setting_ioctl(ide_drive_t *, struct block_device *, unsigned int,
		      unsigned long, const struct ide_ioctl_devset *);

int generic_ide_ioctl(ide_drive_t *, struct block_device *, unsigned, unsigned long);

extern int ide_vlb_clk;
extern int ide_pci_clk;

int ide_end_rq(ide_drive_t *, struct request *, int, unsigned int);
void ide_kill_rq(ide_drive_t *, struct request *);

void __ide_set_handler(ide_drive_t *, ide_handler_t *, unsigned int);
void ide_set_handler(ide_drive_t *, ide_handler_t *, unsigned int);

void ide_execute_command(ide_drive_t *, struct ide_cmd *, ide_handler_t *,
			 unsigned int);

void ide_pad_transfer(ide_drive_t *, int, int);

ide_startstop_t ide_error(ide_drive_t *, const char *, u8);

void ide_fix_driveid(u16 *);

extern void ide_fixstring(u8 *, const int, const int);

int ide_busy_sleep(ide_drive_t *, unsigned long, int);

int __ide_wait_stat(ide_drive_t *, u8, u8, unsigned long, u8 *);
int ide_wait_stat(ide_startstop_t *, ide_drive_t *, u8, u8, unsigned long);

ide_startstop_t ide_do_park_unpark(ide_drive_t *, struct request *);
ide_startstop_t ide_do_devset(ide_drive_t *, struct request *);

extern ide_startstop_t ide_do_reset (ide_drive_t *);

extern int ide_devset_execute(ide_drive_t *drive,
			      const struct ide_devset *setting, int arg);

void ide_complete_cmd(ide_drive_t *, struct ide_cmd *, u8, u8);
int ide_complete_rq(ide_drive_t *, int, unsigned int);

void ide_tf_readback(ide_drive_t *drive, struct ide_cmd *cmd);
void ide_tf_dump(const char *, struct ide_cmd *);

void ide_exec_command(ide_hwif_t *, u8);
u8 ide_read_status(ide_hwif_t *);
u8 ide_read_altstatus(ide_hwif_t *);
void ide_write_devctl(ide_hwif_t *, u8);

void ide_dev_select(ide_drive_t *);
void ide_tf_load(ide_drive_t *, struct ide_taskfile *, u8);
void ide_tf_read(ide_drive_t *, struct ide_taskfile *, u8);

void ide_input_data(ide_drive_t *, struct ide_cmd *, void *, unsigned int);
void ide_output_data(ide_drive_t *, struct ide_cmd *, void *, unsigned int);

void SELECT_MASK(ide_drive_t *, int);

u8 ide_read_error(ide_drive_t *);
void ide_read_bcount_and_ireason(ide_drive_t *, u16 *, u8 *);

int ide_check_ireason(ide_drive_t *, struct request *, int, int, int);

int ide_check_atapi_device(ide_drive_t *, const char *);

void ide_init_pc(struct ide_atapi_pc *);

extern wait_queue_head_t ide_park_wq;
ssize_t ide_park_show(struct device *dev, struct device_attribute *attr,
		      char *buf);
ssize_t ide_park_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t len);

enum {
	REQ_IDETAPE_PC1		= (1 << 0), 
	REQ_IDETAPE_PC2		= (1 << 1), 
	REQ_IDETAPE_READ	= (1 << 2),
	REQ_IDETAPE_WRITE	= (1 << 3),
};

int ide_queue_pc_tail(ide_drive_t *, struct gendisk *, struct ide_atapi_pc *,
		      void *, unsigned int);

int ide_do_test_unit_ready(ide_drive_t *, struct gendisk *);
int ide_do_start_stop(ide_drive_t *, struct gendisk *, int);
int ide_set_media_lock(ide_drive_t *, struct gendisk *, int);
void ide_create_request_sense_cmd(ide_drive_t *, struct ide_atapi_pc *);
void ide_retry_pc(ide_drive_t *drive);

void ide_prep_sense(ide_drive_t *drive, struct request *rq);
int ide_queue_sense_rq(ide_drive_t *drive, void *special);

int ide_cd_expiry(ide_drive_t *);

int ide_cd_get_xferlen(struct request *);

ide_startstop_t ide_issue_pc(ide_drive_t *, struct ide_cmd *);

ide_startstop_t do_rw_taskfile(ide_drive_t *, struct ide_cmd *);

void ide_pio_bytes(ide_drive_t *, struct ide_cmd *, unsigned int, unsigned int);

void ide_finish_cmd(ide_drive_t *, struct ide_cmd *, u8);

int ide_raw_taskfile(ide_drive_t *, struct ide_cmd *, u8 *, u16);
int ide_no_data_taskfile(ide_drive_t *, struct ide_cmd *);

int ide_taskfile_ioctl(ide_drive_t *, unsigned long);

int ide_dev_read_id(ide_drive_t *, u8, u16 *, int);

extern int ide_driveid_update(ide_drive_t *);
extern int ide_config_drive_speed(ide_drive_t *, u8);
extern u8 eighty_ninty_three (ide_drive_t *);
extern int taskfile_lib_get_identify(ide_drive_t *drive, u8 *);

extern int ide_wait_not_busy(ide_hwif_t *hwif, unsigned long timeout);

extern void ide_stall_queue(ide_drive_t *drive, unsigned long timeout);

extern void ide_timer_expiry(unsigned long);
extern irqreturn_t ide_intr(int irq, void *dev_id);
extern void do_ide_request(struct request_queue *);
extern void ide_requeue_and_plug(ide_drive_t *drive, struct request *rq);

void ide_init_disk(struct gendisk *, ide_drive_t *);

#ifdef CONFIG_IDEPCI_PCIBUS_ORDER
extern int __ide_pci_register_driver(struct pci_driver *driver, struct module *owner, const char *mod_name);
#define ide_pci_register_driver(d) __ide_pci_register_driver(d, THIS_MODULE, KBUILD_MODNAME)
#else
#define ide_pci_register_driver(d) pci_register_driver(d)
#endif

static inline int ide_pci_is_in_compatibility_mode(struct pci_dev *dev)
{
	if ((dev->class >> 8) == PCI_CLASS_STORAGE_IDE && (dev->class & 5) != 5)
		return 1;
	return 0;
}

void ide_pci_setup_ports(struct pci_dev *, const struct ide_port_info *,
			 struct ide_hw *, struct ide_hw **);
void ide_setup_pci_noise(struct pci_dev *, const struct ide_port_info *);

#ifdef CONFIG_BLK_DEV_IDEDMA_PCI
int ide_pci_set_master(struct pci_dev *, const char *);
unsigned long ide_pci_dma_base(ide_hwif_t *, const struct ide_port_info *);
int ide_pci_check_simplex(ide_hwif_t *, const struct ide_port_info *);
int ide_hwif_setup_dma(ide_hwif_t *, const struct ide_port_info *);
#else
static inline int ide_hwif_setup_dma(ide_hwif_t *hwif,
				     const struct ide_port_info *d)
{
	return -EINVAL;
}
#endif

struct ide_pci_enablebit {
	u8	reg;	
	u8	mask;	
	u8	val;	
};

enum {
	
	IDE_HFLAG_ISA_PORTS		= (1 << 0),
	
	IDE_HFLAG_SINGLE		= (1 << 1),
	
	IDE_HFLAG_PIO_NO_BLACKLIST	= (1 << 2),
	
	IDE_HFLAG_QD_2ND_PORT		= (1 << 3),
	
	IDE_HFLAG_ABUSE_PREFETCH	= (1 << 4),
	
	IDE_HFLAG_ABUSE_FAST_DEVSEL	= (1 << 5),
	
	IDE_HFLAG_ABUSE_DMA_MODES	= (1 << 6),
	IDE_HFLAG_SET_PIO_MODE_KEEP_DMA	= (1 << 7),
	
	IDE_HFLAG_POST_SET_MODE		= (1 << 8),
	
	IDE_HFLAG_NO_SET_MODE		= (1 << 9),
	
	IDE_HFLAG_TRUST_BIOS_FOR_DMA	= (1 << 10),
	
	IDE_HFLAG_CS5520		= (1 << 11),
	
	IDE_HFLAG_NO_ATAPI_DMA		= (1 << 12),
	
	IDE_HFLAG_NON_BOOTABLE		= (1 << 13),
	
	IDE_HFLAG_NO_DMA		= (1 << 14),
	
	IDE_HFLAG_NO_AUTODMA		= (1 << 15),
	
	IDE_HFLAG_MMIO			= (1 << 16),
	
	IDE_HFLAG_NO_LBA48		= (1 << 17),
	
	IDE_HFLAG_NO_LBA48_DMA		= (1 << 18),
	
	IDE_HFLAG_ERROR_STOPS_FIFO	= (1 << 19),
	
	IDE_HFLAG_SERIALIZE		= (1 << 20),
	
	IDE_HFLAG_DTC2278		= (1 << 21),
	
	IDE_HFLAG_4DRIVES		= (1 << 22),
	
	IDE_HFLAG_TRM290		= (1 << 23),
	
	IDE_HFLAG_IO_32BIT		= (1 << 24),
	
	IDE_HFLAG_UNMASK_IRQS		= (1 << 25),
	IDE_HFLAG_BROKEN_ALTSTATUS	= (1 << 26),
	
	IDE_HFLAG_SERIALIZE_DMA		= (1 << 27),
	
	IDE_HFLAG_CLEAR_SIMPLEX		= (1 << 28),
	
	IDE_HFLAG_NO_DSC		= (1 << 29),
	
	IDE_HFLAG_NO_IO_32BIT		= (1 << 30),
	
	IDE_HFLAG_NO_UNMASK_IRQS	= (1 << 31),
};

#ifdef CONFIG_BLK_DEV_OFFBOARD
# define IDE_HFLAG_OFF_BOARD	0
#else
# define IDE_HFLAG_OFF_BOARD	IDE_HFLAG_NON_BOOTABLE
#endif

struct ide_port_info {
	char			*name;

	int			(*init_chipset)(struct pci_dev *);

	void			(*get_lock)(irq_handler_t, void *);
	void			(*release_lock)(void);

	void			(*init_iops)(ide_hwif_t *);
	void                    (*init_hwif)(ide_hwif_t *);
	int			(*init_dma)(ide_hwif_t *,
					    const struct ide_port_info *);

	const struct ide_tp_ops		*tp_ops;
	const struct ide_port_ops	*port_ops;
	const struct ide_dma_ops	*dma_ops;

	struct ide_pci_enablebit	enablebits[2];

	hwif_chipset_t		chipset;

	u16			max_sectors;	

	u32			host_flags;

	int			irq_flags;

	u8			pio_mask;
	u8			swdma_mask;
	u8			mwdma_mask;
	u8			udma_mask;
};

int ide_pci_init_one(struct pci_dev *, const struct ide_port_info *, void *);
int ide_pci_init_two(struct pci_dev *, struct pci_dev *,
		     const struct ide_port_info *, void *);
void ide_pci_remove(struct pci_dev *);

#ifdef CONFIG_PM
int ide_pci_suspend(struct pci_dev *, pm_message_t);
int ide_pci_resume(struct pci_dev *);
#else
#define ide_pci_suspend NULL
#define ide_pci_resume NULL
#endif

void ide_map_sg(ide_drive_t *, struct ide_cmd *);
void ide_init_sg_cmd(struct ide_cmd *, unsigned int);

#define BAD_DMA_DRIVE		0
#define GOOD_DMA_DRIVE		1

struct drive_list_entry {
	const char *id_model;
	const char *id_firmware;
};

int ide_in_drive_list(u16 *, const struct drive_list_entry *);

#ifdef CONFIG_BLK_DEV_IDEDMA
int ide_dma_good_drive(ide_drive_t *);
int __ide_dma_bad_drive(ide_drive_t *);

u8 ide_find_dma_mode(ide_drive_t *, u8);

static inline u8 ide_max_dma_mode(ide_drive_t *drive)
{
	return ide_find_dma_mode(drive, XFER_UDMA_6);
}

void ide_dma_off_quietly(ide_drive_t *);
void ide_dma_off(ide_drive_t *);
void ide_dma_on(ide_drive_t *);
int ide_set_dma(ide_drive_t *);
void ide_check_dma_crc(ide_drive_t *);
ide_startstop_t ide_dma_intr(ide_drive_t *);

int ide_allocate_dma_engine(ide_hwif_t *);
void ide_release_dma_engine(ide_hwif_t *);

int ide_dma_prepare(ide_drive_t *, struct ide_cmd *);
void ide_dma_unmap_sg(ide_drive_t *, struct ide_cmd *);

#ifdef CONFIG_BLK_DEV_IDEDMA_SFF
int config_drive_for_dma(ide_drive_t *);
int ide_build_dmatable(ide_drive_t *, struct ide_cmd *);
void ide_dma_host_set(ide_drive_t *, int);
int ide_dma_setup(ide_drive_t *, struct ide_cmd *);
extern void ide_dma_start(ide_drive_t *);
int ide_dma_end(ide_drive_t *);
int ide_dma_test_irq(ide_drive_t *);
int ide_dma_sff_timer_expiry(ide_drive_t *);
u8 ide_dma_sff_read_status(ide_hwif_t *);
extern const struct ide_dma_ops sff_dma_ops;
#else
static inline int config_drive_for_dma(ide_drive_t *drive) { return 0; }
#endif 

void ide_dma_lost_irq(ide_drive_t *);
ide_startstop_t ide_dma_timeout_retry(ide_drive_t *, int);

#else
static inline u8 ide_find_dma_mode(ide_drive_t *drive, u8 speed) { return 0; }
static inline u8 ide_max_dma_mode(ide_drive_t *drive) { return 0; }
static inline void ide_dma_off_quietly(ide_drive_t *drive) { ; }
static inline void ide_dma_off(ide_drive_t *drive) { ; }
static inline void ide_dma_on(ide_drive_t *drive) { ; }
static inline void ide_dma_verbose(ide_drive_t *drive) { ; }
static inline int ide_set_dma(ide_drive_t *drive) { return 1; }
static inline void ide_check_dma_crc(ide_drive_t *drive) { ; }
static inline ide_startstop_t ide_dma_intr(ide_drive_t *drive) { return ide_stopped; }
static inline ide_startstop_t ide_dma_timeout_retry(ide_drive_t *drive, int error) { return ide_stopped; }
static inline void ide_release_dma_engine(ide_hwif_t *hwif) { ; }
static inline int ide_dma_prepare(ide_drive_t *drive,
				  struct ide_cmd *cmd) { return 1; }
static inline void ide_dma_unmap_sg(ide_drive_t *drive,
				    struct ide_cmd *cmd) { ; }
#endif 

#ifdef CONFIG_BLK_DEV_IDEACPI
int ide_acpi_init(void);
bool ide_port_acpi(ide_hwif_t *hwif);
extern int ide_acpi_exec_tfs(ide_drive_t *drive);
extern void ide_acpi_get_timing(ide_hwif_t *hwif);
extern void ide_acpi_push_timing(ide_hwif_t *hwif);
void ide_acpi_init_port(ide_hwif_t *);
void ide_acpi_port_init_devices(ide_hwif_t *);
extern void ide_acpi_set_state(ide_hwif_t *hwif, int on);
#else
static inline int ide_acpi_init(void) { return 0; }
static inline bool ide_port_acpi(ide_hwif_t *hwif) { return 0; }
static inline int ide_acpi_exec_tfs(ide_drive_t *drive) { return 0; }
static inline void ide_acpi_get_timing(ide_hwif_t *hwif) { ; }
static inline void ide_acpi_push_timing(ide_hwif_t *hwif) { ; }
static inline void ide_acpi_init_port(ide_hwif_t *hwif) { ; }
static inline void ide_acpi_port_init_devices(ide_hwif_t *hwif) { ; }
static inline void ide_acpi_set_state(ide_hwif_t *hwif, int on) {}
#endif

void ide_register_region(struct gendisk *);
void ide_unregister_region(struct gendisk *);

void ide_check_nien_quirk_list(ide_drive_t *);
void ide_undecoded_slave(ide_drive_t *);

void ide_port_apply_params(ide_hwif_t *);
int ide_sysfs_register_port(ide_hwif_t *);

struct ide_host *ide_host_alloc(const struct ide_port_info *, struct ide_hw **,
				unsigned int);
void ide_host_free(struct ide_host *);
int ide_host_register(struct ide_host *, const struct ide_port_info *,
		      struct ide_hw **);
int ide_host_add(const struct ide_port_info *, struct ide_hw **, unsigned int,
		 struct ide_host **);
void ide_host_remove(struct ide_host *);
int ide_legacy_device_add(const struct ide_port_info *, unsigned long);
void ide_port_unregister_devices(ide_hwif_t *);
void ide_port_scan(ide_hwif_t *);

static inline void *ide_get_hwifdata (ide_hwif_t * hwif)
{
	return hwif->hwif_data;
}

static inline void ide_set_hwifdata (ide_hwif_t * hwif, void *data)
{
	hwif->hwif_data = data;
}

extern void ide_toggle_bounce(ide_drive_t *drive, int on);

u64 ide_get_lba_addr(struct ide_cmd *, int);
u8 ide_dump_status(ide_drive_t *, const char *, u8);

struct ide_timing {
	u8  mode;
	u8  setup;	
	u16 act8b;	
	u16 rec8b;	
	u16 cyc8b;	
	u16 active;	
	u16 recover;	
	u16 cycle;	
	u16 udma;	
};

enum {
	IDE_TIMING_SETUP	= (1 << 0),
	IDE_TIMING_ACT8B	= (1 << 1),
	IDE_TIMING_REC8B	= (1 << 2),
	IDE_TIMING_CYC8B	= (1 << 3),
	IDE_TIMING_8BIT		= IDE_TIMING_ACT8B | IDE_TIMING_REC8B |
				  IDE_TIMING_CYC8B,
	IDE_TIMING_ACTIVE	= (1 << 4),
	IDE_TIMING_RECOVER	= (1 << 5),
	IDE_TIMING_CYCLE	= (1 << 6),
	IDE_TIMING_UDMA		= (1 << 7),
	IDE_TIMING_ALL		= IDE_TIMING_SETUP | IDE_TIMING_8BIT |
				  IDE_TIMING_ACTIVE | IDE_TIMING_RECOVER |
				  IDE_TIMING_CYCLE | IDE_TIMING_UDMA,
};

struct ide_timing *ide_timing_find_mode(u8);
u16 ide_pio_cycle_time(ide_drive_t *, u8);
void ide_timing_merge(struct ide_timing *, struct ide_timing *,
		      struct ide_timing *, unsigned int);
int ide_timing_compute(ide_drive_t *, u8, struct ide_timing *, int, int);

#ifdef CONFIG_IDE_XFER_MODE
int ide_scan_pio_blacklist(char *);
const char *ide_xfer_verbose(u8);
int ide_pio_need_iordy(ide_drive_t *, const u8);
int ide_set_pio_mode(ide_drive_t *, u8);
int ide_set_dma_mode(ide_drive_t *, u8);
void ide_set_pio(ide_drive_t *, u8);
int ide_set_xfer_rate(ide_drive_t *, u8);
#else
static inline void ide_set_pio(ide_drive_t *drive, u8 pio) { ; }
static inline int ide_set_xfer_rate(ide_drive_t *drive, u8 rate) { return -1; }
#endif

static inline void ide_set_max_pio(ide_drive_t *drive)
{
	ide_set_pio(drive, 255);
}

char *ide_media_string(ide_drive_t *);

extern struct device_attribute ide_dev_attrs[];
extern struct bus_type ide_bus_type;
extern struct class *ide_port_class;

static inline void ide_dump_identify(u8 *id)
{
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_NONE, 16, 2, id, 512, 0);
}

static inline int hwif_to_node(ide_hwif_t *hwif)
{
	return hwif->dev ? dev_to_node(hwif->dev) : -1;
}

static inline ide_drive_t *ide_get_pair_dev(ide_drive_t *drive)
{
	ide_drive_t *peer = drive->hwif->devices[(drive->dn ^ 1) & 1];

	return (peer->dev_flags & IDE_DFLAG_PRESENT) ? peer : NULL;
}

static inline void *ide_get_drivedata(ide_drive_t *drive)
{
	return drive->drive_data;
}

static inline void ide_set_drivedata(ide_drive_t *drive, void *data)
{
	drive->drive_data = data;
}

#define ide_port_for_each_dev(i, dev, port) \
	for ((i) = 0; ((dev) = (port)->devices[i]) || (i) < MAX_DRIVES; (i)++)

#define ide_port_for_each_present_dev(i, dev, port) \
	for ((i) = 0; ((dev) = (port)->devices[i]) || (i) < MAX_DRIVES; (i)++) \
		if ((dev)->dev_flags & IDE_DFLAG_PRESENT)

#define ide_host_for_each_port(i, port, host) \
	for ((i) = 0; ((port) = (host)->ports[i]) || (i) < MAX_HOST_PORTS; (i)++)

#endif 
