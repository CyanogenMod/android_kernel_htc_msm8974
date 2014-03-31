/*
 * arch/s390/appldata/appldata.h
 *
 * Definitions and interface for Linux - z/VM Monitor Stream.
 *
 * Copyright IBM Corp. 2003, 2008
 *
 * Author: Gerald Schaefer <gerald.schaefer@de.ibm.com>
 */

#define APPLDATA_MAX_REC_SIZE	  4024	
					
#define APPLDATA_MAX_PROCS 100

#define APPLDATA_PROC_NAME_LENGTH 16	

#define APPLDATA_RECORD_MEM_ID		0x01	
#define APPLDATA_RECORD_OS_ID		0x02	
#define APPLDATA_RECORD_NET_SUM_ID	0x03	
#define APPLDATA_RECORD_PROC_ID		0x04

#define CTL_APPLDATA_TIMER 	2121	
#define CTL_APPLDATA_INTERVAL 	2122
#define CTL_APPLDATA_MEM	2123
#define CTL_APPLDATA_OS		2124
#define CTL_APPLDATA_NET_SUM	2125
#define CTL_APPLDATA_PROC	2126

struct appldata_ops {
	struct list_head list;
	struct ctl_table_header *sysctl_header;
	struct ctl_table *ctl_table;
	int    active;				

	
	char name[APPLDATA_PROC_NAME_LENGTH];	
	unsigned char record_nr;		
	void (*callback)(void *data);		
	void *data;				
	unsigned int size;			
	struct module *owner;			
	char mod_lvl[2];			
};

extern int appldata_register_ops(struct appldata_ops *ops);
extern void appldata_unregister_ops(struct appldata_ops *ops);
extern int appldata_diag(char record_nr, u16 function, unsigned long buffer,
			 u16 length, char *mod_lvl);

