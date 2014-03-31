/*
 * CPUMF HW sampler functions and internal structures
 *
 *    Copyright IBM Corp. 2010
 *    Author(s): Heinz Graalfs <graalfs@de.ibm.com>
 */

#ifndef HWSAMPLER_H_
#define HWSAMPLER_H_

#include <linux/workqueue.h>

struct hws_qsi_info_block          
{ 
	unsigned int b0_13:14;      
	unsigned int as:1;          
	unsigned int b15_21:7;      
	unsigned int es:1;          
	unsigned int b23_29:7;      
	unsigned int cs:1;          
	unsigned int:1;             
	unsigned int bsdes:16;      
	unsigned int:16;            
	unsigned long min_sampl_rate; 
	unsigned long max_sampl_rate; 
	unsigned long tear;         
	unsigned long dear;         
	unsigned int rsvrd0;        
	unsigned int cpu_speed;     
	unsigned long long rsvrd1;  
	unsigned long long rsvrd2;  
};

struct hws_ssctl_request_block     
{ 
	unsigned int s:1;           
	unsigned int h:1;           
	unsigned long b2_53:52;     
	unsigned int es:1;          
	unsigned int b55_61:7;      
	unsigned int cs:1;          
	unsigned int b63:1;         
	unsigned long interval;     
	unsigned long tear;         
	unsigned long dear;         
	
	unsigned long rsvrd1;       
	unsigned long rsvrd2;       
	unsigned long rsvrd3;       
	unsigned long rsvrd4;       
};

struct hws_cpu_buffer {
	unsigned long first_sdbt;       
	unsigned long worker_entry;
	unsigned long sample_overflow;  
	struct hws_qsi_info_block qsi;
	struct hws_ssctl_request_block ssctl;
	struct work_struct worker;
	atomic_t ext_params;
	unsigned long req_alert;
	unsigned long loss_of_sample_data;
	unsigned long invalid_entry_address;
	unsigned long incorrect_sdbt_entry;
	unsigned long sample_auth_change_alert;
	unsigned int finish:1;
	unsigned int oom:1;
	unsigned int stop_mode:1;
};

struct hws_data_entry {
	unsigned int def:16;        
	unsigned int R:4;           
	unsigned int U:4;           
	unsigned int z:2;           
	unsigned int T:1;           
	unsigned int W:1;           
	unsigned int P:1;           
	unsigned int AS:2;          
	unsigned int I:1;           
	unsigned int:16;
	unsigned int prim_asn:16;   
	unsigned long long ia;      
	unsigned long long lpp;     
	unsigned long long vpp;     
};

struct hws_trailer_entry {
	unsigned int f:1;           
	unsigned int a:1;           
	unsigned long:62;           
	unsigned long overflow;     
	unsigned long timestamp;    
	unsigned long timestamp1;   
	unsigned long reserved1;    
	unsigned long reserved2;    
	unsigned long progusage1;   
	unsigned long progusage2;   
};

int hwsampler_setup(void);
int hwsampler_shutdown(void);
int hwsampler_allocate(unsigned long sdbt, unsigned long sdb);
int hwsampler_deallocate(void);
unsigned long hwsampler_query_min_interval(void);
unsigned long hwsampler_query_max_interval(void);
int hwsampler_start_all(unsigned long interval);
int hwsampler_stop_all(void);
int hwsampler_deactivate(unsigned int cpu);
int hwsampler_activate(unsigned int cpu);
unsigned long hwsampler_get_sample_overflow_count(unsigned int cpu);

#endif 
