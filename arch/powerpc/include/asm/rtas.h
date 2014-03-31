#ifndef _POWERPC_RTAS_H
#define _POWERPC_RTAS_H
#ifdef __KERNEL__

#include <linux/spinlock.h>
#include <asm/page.h>

/*
 * Definitions for talking to the RTAS on CHRP machines.
 *
 * Copyright (C) 2001 Peter Bergner
 * Copyright (C) 2001 PPC 64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define RTAS_UNKNOWN_SERVICE (-1)
#define RTAS_INSTANTIATE_MAX (1ULL<<30) 

#define RTAS_RMOBUF_MAX (64 * 1024)

#define RTAS_NOT_SUSPENDABLE	-9004
#define RTAS_BUSY		-2    
#define RTAS_EXTENDED_DELAY_MIN	9900
#define RTAS_EXTENDED_DELAY_MAX	9905


typedef u32 rtas_arg_t;

struct rtas_args {
	u32 token;
	u32 nargs;
	u32 nret; 
	rtas_arg_t args[16];
	rtas_arg_t *rets;     
};  

struct rtas_t {
	unsigned long entry;		
	unsigned long base;		
	unsigned long size;
	arch_spinlock_t lock;
	struct rtas_args args;
	struct device_node *dev;	
};

struct rtas_suspend_me_data {
	atomic_t working; 
	atomic_t done;
	int token; 
	atomic_t error;
	struct completion *complete; 
};

#define RTAS_INTERNAL_ERROR		0x80000000 
#define RTAS_EPOW_WARNING		0x40000000 
#define RTAS_HOTPLUG_EVENTS		0x10000000 
#define RTAS_IO_EVENTS			0x08000000 
#define RTAS_EVENT_SCAN_ALL_EVENTS	0xffffffff

#define RTAS_SEVERITY_FATAL		0x5
#define RTAS_SEVERITY_ERROR		0x4
#define RTAS_SEVERITY_ERROR_SYNC	0x3
#define RTAS_SEVERITY_WARNING		0x2
#define RTAS_SEVERITY_EVENT		0x1
#define RTAS_SEVERITY_NO_ERROR		0x0

#define RTAS_DISP_FULLY_RECOVERED	0x0
#define RTAS_DISP_LIMITED_RECOVERY	0x1
#define RTAS_DISP_NOT_RECOVERED		0x2

#define RTAS_INITIATOR_UNKNOWN		0x0
#define RTAS_INITIATOR_CPU		0x1
#define RTAS_INITIATOR_PCI		0x2
#define RTAS_INITIATOR_ISA		0x3
#define RTAS_INITIATOR_MEMORY		0x4
#define RTAS_INITIATOR_POWERMGM		0x5

#define RTAS_TARGET_UNKNOWN		0x0
#define RTAS_TARGET_CPU			0x1
#define RTAS_TARGET_PCI			0x2
#define RTAS_TARGET_ISA			0x3
#define RTAS_TARGET_MEMORY		0x4
#define RTAS_TARGET_POWERMGM		0x5

#define RTAS_TYPE_RETRY			0x01
#define RTAS_TYPE_TCE_ERR		0x02
#define RTAS_TYPE_INTERN_DEV_FAIL	0x03
#define RTAS_TYPE_TIMEOUT		0x04
#define RTAS_TYPE_DATA_PARITY		0x05
#define RTAS_TYPE_ADDR_PARITY		0x06
#define RTAS_TYPE_CACHE_PARITY		0x07
#define RTAS_TYPE_ADDR_INVALID		0x08
#define RTAS_TYPE_ECC_UNCORR		0x09
#define RTAS_TYPE_ECC_CORR		0x0a
#define RTAS_TYPE_EPOW			0x40
#define RTAS_TYPE_PLATFORM		0xE0
#define RTAS_TYPE_IO			0xE1
#define RTAS_TYPE_INFO			0xE2
#define RTAS_TYPE_DEALLOC		0xE3
#define RTAS_TYPE_DUMP			0xE4
 
#define RTAS_TYPE_PMGM_POWER_SW_ON	0x60
#define RTAS_TYPE_PMGM_POWER_SW_OFF	0x61
#define RTAS_TYPE_PMGM_LID_OPEN		0x62
#define RTAS_TYPE_PMGM_LID_CLOSE	0x63
#define RTAS_TYPE_PMGM_SLEEP_BTN	0x64
#define RTAS_TYPE_PMGM_WAKE_BTN		0x65
#define RTAS_TYPE_PMGM_BATTERY_WARN	0x66
#define RTAS_TYPE_PMGM_BATTERY_CRIT	0x67
#define RTAS_TYPE_PMGM_SWITCH_TO_BAT	0x68
#define RTAS_TYPE_PMGM_SWITCH_TO_AC	0x69
#define RTAS_TYPE_PMGM_KBD_OR_MOUSE	0x6a
#define RTAS_TYPE_PMGM_ENCLOS_OPEN	0x6b
#define RTAS_TYPE_PMGM_ENCLOS_CLOSED	0x6c
#define RTAS_TYPE_PMGM_RING_INDICATE	0x6d
#define RTAS_TYPE_PMGM_LAN_ATTENTION	0x6e
#define RTAS_TYPE_PMGM_TIME_ALARM	0x6f
#define RTAS_TYPE_PMGM_CONFIG_CHANGE	0x70
#define RTAS_TYPE_PMGM_SERVICE_PROC	0x71

#define RTAS_VECTOR_EXTERNAL_INTERRUPT	0x500

struct rtas_error_log {
	unsigned long version:8;		
	unsigned long severity:3;		
	unsigned long disposition:2;		
	unsigned long extended:1;		
	unsigned long  :2;	
	unsigned long initiator:4;		
	unsigned long target:4;			
	unsigned long type:8;			
	unsigned long extended_log_length:32;	
	unsigned char buffer[1];		
						
};

#define RTAS_V6EXT_LOG_FORMAT_EVENT_LOG	14

#define RTAS_V6EXT_COMPANY_ID_IBM	(('I' << 24) | ('B' << 16) | ('M' << 8))

struct rtas_ext_event_log_v6 {
	
	uint32_t log_valid:1;		
	uint32_t unrecoverable_error:1;	
	uint32_t recoverable_error:1;	
					
	uint32_t degraded_operation:1;	
					
					
	uint32_t predictive_error:1;
	uint32_t new_log:1;		
					
	uint32_t big_endian:1;		
	uint32_t :1;			
	
	uint32_t :8;			
	
	uint32_t powerpc_format:1;	
					
	uint32_t :3;			
	uint32_t log_format:4;		
					
	
	uint32_t :8;			
	
	uint8_t reserved[8];		
	
	uint32_t company_id;		
					
					
	
	uint8_t vendor_log[1];		
					
};


#define PSERIES_ELOG_SECT_ID_PRIV_HDR		(('P' << 8) | 'H')
#define PSERIES_ELOG_SECT_ID_USER_HDR		(('U' << 8) | 'H')
#define PSERIES_ELOG_SECT_ID_PRIMARY_SRC	(('P' << 8) | 'S')
#define PSERIES_ELOG_SECT_ID_EXTENDED_UH	(('E' << 8) | 'H')
#define PSERIES_ELOG_SECT_ID_FAILING_MTMS	(('M' << 8) | 'T')
#define PSERIES_ELOG_SECT_ID_SECONDARY_SRC	(('S' << 8) | 'S')
#define PSERIES_ELOG_SECT_ID_DUMP_LOCATOR	(('D' << 8) | 'H')
#define PSERIES_ELOG_SECT_ID_FW_ERROR		(('S' << 8) | 'W')
#define PSERIES_ELOG_SECT_ID_IMPACT_PART_ID	(('L' << 8) | 'P')
#define PSERIES_ELOG_SECT_ID_LOGIC_RESOURCE_ID	(('L' << 8) | 'R')
#define PSERIES_ELOG_SECT_ID_HMC_ID		(('H' << 8) | 'M')
#define PSERIES_ELOG_SECT_ID_EPOW		(('E' << 8) | 'P')
#define PSERIES_ELOG_SECT_ID_IO_EVENT		(('I' << 8) | 'E')
#define PSERIES_ELOG_SECT_ID_MANUFACT_INFO	(('M' << 8) | 'I')
#define PSERIES_ELOG_SECT_ID_CALL_HOME		(('C' << 8) | 'H')
#define PSERIES_ELOG_SECT_ID_USER_DEF		(('U' << 8) | 'D')

struct pseries_errorlog {
	uint16_t id;			
	uint16_t length;		
	uint8_t version;		
	uint8_t subtype;		
	uint16_t creator_component;	
	uint8_t data[];			
};

struct pseries_errorlog *get_pseries_errorlog(struct rtas_error_log *log,
					      uint16_t section_id);

extern void (*rtas_flash_term_hook)(int);

extern struct rtas_t rtas;

extern void enter_rtas(unsigned long);
extern int rtas_token(const char *service);
extern int rtas_service_present(const char *service);
extern int rtas_call(int token, int, int, int *, ...);
extern void rtas_restart(char *cmd);
extern void rtas_power_off(void);
extern void rtas_halt(void);
extern void rtas_os_term(char *str);
extern int rtas_get_sensor(int sensor, int index, int *state);
extern int rtas_get_power_level(int powerdomain, int *level);
extern int rtas_set_power_level(int powerdomain, int level, int *setlevel);
extern bool rtas_indicator_present(int token, int *maxindex);
extern int rtas_set_indicator(int indicator, int index, int new_value);
extern int rtas_set_indicator_fast(int indicator, int index, int new_value);
extern void rtas_progress(char *s, unsigned short hex);
extern void rtas_initialize(void);
extern int rtas_suspend_cpu(struct rtas_suspend_me_data *data);
extern int rtas_suspend_last_cpu(struct rtas_suspend_me_data *data);
extern int rtas_ibm_suspend_me(struct rtas_args *);

struct rtc_time;
extern unsigned long rtas_get_boot_time(void);
extern void rtas_get_rtc_time(struct rtc_time *rtc_time);
extern int rtas_set_rtc_time(struct rtc_time *rtc_time);

extern unsigned int rtas_busy_delay_time(int status);
extern unsigned int rtas_busy_delay(int status);

extern int early_init_dt_scan_rtas(unsigned long node,
		const char *uname, int depth, void *data);

extern void pSeries_log_error(char *buf, unsigned int err_type, int fatal);

#ifdef CONFIG_PPC_RTAS_DAEMON
extern void rtas_cancel_event_scan(void);
#else
static inline void rtas_cancel_event_scan(void) { }
#endif

#define ERR_FLAG_ALREADY_LOGGED	0x0
#define ERR_FLAG_BOOT		0x1 	
#define ERR_TYPE_RTAS_LOG	0x2	
#define ERR_TYPE_KERNEL_PANIC	0x4	
#define ERR_TYPE_KERNEL_PANIC_GZ 0x8	

#define ERR_TYPE_MASK \
	(ERR_TYPE_RTAS_LOG | ERR_TYPE_KERNEL_PANIC | ERR_TYPE_KERNEL_PANIC_GZ)

#define RTAS_DEBUG KERN_DEBUG "RTAS: "
 
#define RTAS_ERROR_LOG_MAX 2048

extern int rtas_get_error_log_max(void);

#define EVENT_SCAN_ALL_EVENTS	0xf0000000
#define SURVEILLANCE_TOKEN	9000
#define LOG_NUMBER		64		
#define LOG_NUMBER_MASK		(LOG_NUMBER-1)


#define RTAS_DATA_BUF_SIZE 4096
extern spinlock_t rtas_data_buf_lock;
extern char rtas_data_buf[RTAS_DATA_BUF_SIZE];

extern unsigned long rtas_rmo_buf;

#define GLOBAL_INTERRUPT_QUEUE 9005

static inline u32 rtas_config_addr(int busno, int devfn, int reg)
{
	return ((reg & 0xf00) << 20) | ((busno & 0xff) << 16) |
			(devfn << 8) | (reg & 0xff);
}

extern void __cpuinit rtas_give_timebase(void);
extern void __cpuinit rtas_take_timebase(void);

#ifdef CONFIG_PPC_RTAS
static inline int page_is_rtas_user_buf(unsigned long pfn)
{
	unsigned long paddr = (pfn << PAGE_SHIFT);
	if (paddr >= rtas_rmo_buf && paddr < (rtas_rmo_buf + RTAS_RMOBUF_MAX))
		return 1;
	return 0;
}
#else
static inline int page_is_rtas_user_buf(unsigned long pfn) { return 0;}
#endif

extern int call_rtas(const char *, int, int, unsigned long *, ...);

#endif 
#endif 
