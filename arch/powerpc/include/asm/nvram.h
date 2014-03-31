/*
 * NVRAM definitions and access functions.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _ASM_POWERPC_NVRAM_H
#define _ASM_POWERPC_NVRAM_H

#define NVRAM_SIG_SP	0x02	
#define NVRAM_SIG_OF	0x50	
#define NVRAM_SIG_FW	0x51	
#define NVRAM_SIG_HW	0x52	
#define NVRAM_SIG_FLIP	0x5a	
#define NVRAM_SIG_APPL	0x5f	
#define NVRAM_SIG_SYS	0x70	
#define NVRAM_SIG_CFG	0x71	
#define NVRAM_SIG_ELOG	0x72	
#define NVRAM_SIG_VEND	0x7e	
#define NVRAM_SIG_FREE	0x7f	
#define NVRAM_SIG_OS	0xa0	
#define NVRAM_SIG_PANIC	0xa1	

#ifdef __KERNEL__

#include <linux/errno.h>
#include <linux/list.h>

#ifdef CONFIG_PPC_PSERIES
extern int nvram_write_error_log(char * buff, int length,
					 unsigned int err_type, unsigned int err_seq);
extern int nvram_read_error_log(char * buff, int length,
					 unsigned int * err_type, unsigned int *err_seq);
extern int nvram_clear_error_log(void);
extern int pSeries_nvram_init(void);
#endif 

#ifdef CONFIG_MMIO_NVRAM
extern int mmio_nvram_init(void);
#else
static inline int mmio_nvram_init(void)
{
	return -ENODEV;
}
#endif

extern int __init nvram_scan_partitions(void);
extern loff_t nvram_create_partition(const char *name, int sig,
				     int req_size, int min_size);
extern int nvram_remove_partition(const char *name, int sig,
					const char *exceptions[]);
extern int nvram_get_partition_size(loff_t data_index);
extern loff_t nvram_find_partition(const char *name, int sig, int *out_size);

#endif 


enum {
	pmac_nvram_OF,		
	pmac_nvram_XPRAM,	
	pmac_nvram_NR		
};

#ifdef __KERNEL__
extern int	pmac_get_partition(int partition);

extern u8	pmac_xpram_read(int xpaddr);
extern void	pmac_xpram_write(int xpaddr, u8 data);

extern void	nvram_sync(void);

extern ssize_t nvram_get_size(void);

extern unsigned char nvram_read_byte(int i);
extern void nvram_write_byte(unsigned char c, int i);
#endif

#define PMAC_XPRAM_MACHINE_LOC	0xe4
#define PMAC_XPRAM_SOUND_VOLUME	0x08

struct pmac_machine_location {
	unsigned int	latitude;	
	unsigned int	longitude;	
	unsigned int	delta;		
};


#define OBSOLETE_PMAC_NVRAM_GET_OFFSET \
				_IOWR('p', 0x40, int)

#define IOC_NVRAM_GET_OFFSET	_IOWR('p', 0x42, int)	
#define IOC_NVRAM_SYNC		_IO('p', 0x43)		

#endif 
