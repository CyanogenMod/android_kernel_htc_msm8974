/*
 * Generic EDAC defs
 *
 * Author: Dave Jiang <djiang@mvista.com>
 *
 * 2006-2008 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 */
#ifndef _LINUX_EDAC_H_
#define _LINUX_EDAC_H_

#include <linux/atomic.h>
#include <linux/kobject.h>
#include <linux/completion.h>
#include <linux/workqueue.h>

struct device;

#define EDAC_OPSTATE_INVAL	-1
#define EDAC_OPSTATE_POLL	0
#define EDAC_OPSTATE_NMI	1
#define EDAC_OPSTATE_INT	2

extern int edac_op_state;
extern int edac_err_assert;
extern atomic_t edac_handlers;
extern struct bus_type edac_subsys;

extern int edac_handler_set(void);
extern void edac_atomic_assert_error(void);
extern struct bus_type *edac_get_sysfs_subsys(void);
extern void edac_put_sysfs_subsys(void);

static inline void opstate_init(void)
{
	switch (edac_op_state) {
	case EDAC_OPSTATE_POLL:
	case EDAC_OPSTATE_NMI:
		break;
	default:
		edac_op_state = EDAC_OPSTATE_POLL;
	}
	return;
}

#define EDAC_MC_LABEL_LEN	31
#define MC_PROC_NAME_MAX_LEN	7

enum dev_type {
	DEV_UNKNOWN = 0,
	DEV_X1,
	DEV_X2,
	DEV_X4,
	DEV_X8,
	DEV_X16,
	DEV_X32,		
	DEV_X64			
};

#define DEV_FLAG_UNKNOWN	BIT(DEV_UNKNOWN)
#define DEV_FLAG_X1		BIT(DEV_X1)
#define DEV_FLAG_X2		BIT(DEV_X2)
#define DEV_FLAG_X4		BIT(DEV_X4)
#define DEV_FLAG_X8		BIT(DEV_X8)
#define DEV_FLAG_X16		BIT(DEV_X16)
#define DEV_FLAG_X32		BIT(DEV_X32)
#define DEV_FLAG_X64		BIT(DEV_X64)

enum mem_type {
	MEM_EMPTY = 0,
	MEM_RESERVED,
	MEM_UNKNOWN,
	MEM_FPM,
	MEM_EDO,
	MEM_BEDO,
	MEM_SDR,
	MEM_RDR,
	MEM_DDR,
	MEM_RDDR,
	MEM_RMBS,
	MEM_DDR2,
	MEM_FB_DDR2,
	MEM_RDDR2,
	MEM_XDR,
	MEM_DDR3,
	MEM_RDDR3,
};

#define MEM_FLAG_EMPTY		BIT(MEM_EMPTY)
#define MEM_FLAG_RESERVED	BIT(MEM_RESERVED)
#define MEM_FLAG_UNKNOWN	BIT(MEM_UNKNOWN)
#define MEM_FLAG_FPM		BIT(MEM_FPM)
#define MEM_FLAG_EDO		BIT(MEM_EDO)
#define MEM_FLAG_BEDO		BIT(MEM_BEDO)
#define MEM_FLAG_SDR		BIT(MEM_SDR)
#define MEM_FLAG_RDR		BIT(MEM_RDR)
#define MEM_FLAG_DDR		BIT(MEM_DDR)
#define MEM_FLAG_RDDR		BIT(MEM_RDDR)
#define MEM_FLAG_RMBS		BIT(MEM_RMBS)
#define MEM_FLAG_DDR2           BIT(MEM_DDR2)
#define MEM_FLAG_FB_DDR2        BIT(MEM_FB_DDR2)
#define MEM_FLAG_RDDR2          BIT(MEM_RDDR2)
#define MEM_FLAG_XDR            BIT(MEM_XDR)
#define MEM_FLAG_DDR3		 BIT(MEM_DDR3)
#define MEM_FLAG_RDDR3		 BIT(MEM_RDDR3)

enum edac_type {
	EDAC_UNKNOWN = 0,	
	EDAC_NONE,		
	EDAC_RESERVED,		
	EDAC_PARITY,		
	EDAC_EC,		
	EDAC_SECDED,		
	EDAC_S2ECD2ED,		
	EDAC_S4ECD4ED,		
	EDAC_S8ECD8ED,		
	EDAC_S16ECD16ED,	
};

#define EDAC_FLAG_UNKNOWN	BIT(EDAC_UNKNOWN)
#define EDAC_FLAG_NONE		BIT(EDAC_NONE)
#define EDAC_FLAG_PARITY	BIT(EDAC_PARITY)
#define EDAC_FLAG_EC		BIT(EDAC_EC)
#define EDAC_FLAG_SECDED	BIT(EDAC_SECDED)
#define EDAC_FLAG_S2ECD2ED	BIT(EDAC_S2ECD2ED)
#define EDAC_FLAG_S4ECD4ED	BIT(EDAC_S4ECD4ED)
#define EDAC_FLAG_S8ECD8ED	BIT(EDAC_S8ECD8ED)
#define EDAC_FLAG_S16ECD16ED	BIT(EDAC_S16ECD16ED)

enum scrub_type {
	SCRUB_UNKNOWN = 0,	
	SCRUB_NONE,		
	SCRUB_SW_PROG,		
	SCRUB_SW_SRC,		
	SCRUB_SW_PROG_SRC,	
	SCRUB_SW_TUNABLE,	
	SCRUB_HW_PROG,		
	SCRUB_HW_SRC,		
	SCRUB_HW_PROG_SRC,	
	SCRUB_HW_TUNABLE	
};

#define SCRUB_FLAG_SW_PROG	BIT(SCRUB_SW_PROG)
#define SCRUB_FLAG_SW_SRC	BIT(SCRUB_SW_SRC)
#define SCRUB_FLAG_SW_PROG_SRC	BIT(SCRUB_SW_PROG_SRC)
#define SCRUB_FLAG_SW_TUN	BIT(SCRUB_SW_SCRUB_TUNABLE)
#define SCRUB_FLAG_HW_PROG	BIT(SCRUB_HW_PROG)
#define SCRUB_FLAG_HW_SRC	BIT(SCRUB_HW_SRC)
#define SCRUB_FLAG_HW_PROG_SRC	BIT(SCRUB_HW_PROG_SRC)
#define SCRUB_FLAG_HW_TUN	BIT(SCRUB_HW_TUNABLE)


#define	OP_ALLOC		0x100
#define OP_RUNNING_POLL		0x201
#define OP_RUNNING_INTERRUPT	0x202
#define OP_RUNNING_POLL_INTR	0x203
#define OP_OFFLINE		0x300


struct rank_info {
	int chan_idx;
	u32 ce_count;
	char label[EDAC_MC_LABEL_LEN + 1];
	struct csrow_info *csrow;	
};

struct csrow_info {
	unsigned long first_page;	
	unsigned long last_page;	
	unsigned long page_mask;	
	u32 nr_pages;		
	u32 grain;		
	int csrow_idx;		
	enum dev_type dtype;	
	u32 ue_count;		
	u32 ce_count;		
	enum mem_type mtype;	
	enum edac_type edac_mode;	
	struct mem_ctl_info *mci;	

	struct kobject kobj;	

	
	u32 nr_channels;
	struct rank_info *channels;
};

struct mcidev_sysfs_group {
	const char *name;				
	const struct mcidev_sysfs_attribute *mcidev_attr; 
};

struct mcidev_sysfs_group_kobj {
	struct list_head list;		

	struct kobject kobj;		

	const struct mcidev_sysfs_group *grp;	
	struct mem_ctl_info *mci;	
};

struct mcidev_sysfs_attribute {
	
	struct attribute attr;
	const struct mcidev_sysfs_group *grp;	

	
        ssize_t (*show)(struct mem_ctl_info *,char *);
        ssize_t (*store)(struct mem_ctl_info *, const char *,size_t);
};

struct mem_ctl_info {
	struct list_head link;	

	struct module *owner;	

	unsigned long mtype_cap;	
	unsigned long edac_ctl_cap;	
	unsigned long edac_cap;	
	unsigned long scrub_cap;	
	enum scrub_type scrub_mode;	

	int (*set_sdram_scrub_rate) (struct mem_ctl_info * mci, u32 bw);

	int (*get_sdram_scrub_rate) (struct mem_ctl_info * mci);


	
	void (*edac_check) (struct mem_ctl_info * mci);

	
	unsigned long (*ctl_page_to_phys) (struct mem_ctl_info * mci,
					   unsigned long page);
	int mc_idx;
	int nr_csrows;
	struct csrow_info *csrows;
	struct device *dev;
	const char *mod_name;
	const char *mod_ver;
	const char *ctl_name;
	const char *dev_name;
	char proc_name[MC_PROC_NAME_MAX_LEN + 1];
	void *pvt_info;
	u32 ue_noinfo_count;	
	u32 ce_noinfo_count;	
	u32 ue_count;		
	u32 ce_count;		
	unsigned long start_time;	

	struct completion complete;

	
	struct kobject edac_mci_kobj;

	
	struct list_head grp_kobj_list;

	const struct mcidev_sysfs_attribute *mc_driver_sysfs_attributes;

	
	struct delayed_work work;

	
	int op_state;
};

#endif
