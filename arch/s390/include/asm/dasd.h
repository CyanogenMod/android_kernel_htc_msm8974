/* 
 * File...........: linux/drivers/s390/block/dasd.c
 * Author(s)......: Holger Smolinski <Holger.Smolinski@de.ibm.com>
 * Bugreports.to..: <Linux390@de.ibm.com>
 * (C) IBM Corporation, IBM Deutschland Entwicklung GmbH, 1999,2000
 * EMC Symmetrix ioctl Copyright EMC Corporation, 2008
 * Author.........: Nigel Hislop <hislop_nigel@emc.com>
 *
 * This file is the interface of the DASD device driver, which is exported to user space
 * any future changes wrt the API will result in a change of the APIVERSION reported
 * to userspace by the DASDAPIVER-ioctl
 *
 */

#ifndef DASD_H
#define DASD_H
#include <linux/types.h>
#include <linux/ioctl.h>

#define DASD_IOCTL_LETTER 'D'

#define DASD_API_VERSION 6

typedef struct dasd_information2_t {
        unsigned int devno;         
        unsigned int real_devno;    
        unsigned int schid;         
        unsigned int cu_type  : 16; 
        unsigned int cu_model :  8; 
        unsigned int dev_type : 16; 
        unsigned int dev_model : 8; 
        unsigned int open_count; 
        unsigned int req_queue_len; 
        unsigned int chanq_len;     
        char type[4];               
        unsigned int status;        
        unsigned int label_block;   
        unsigned int FBA_layout;    
        unsigned int characteristics_size;
        unsigned int confdata_size;
        char characteristics[64];   
        char configuration_data[256]; 
        unsigned int format;          
        unsigned int features;        
        unsigned int reserved0;       
        unsigned int reserved1;       
        unsigned int reserved2;       
        unsigned int reserved3;       
        unsigned int reserved4;       
        unsigned int reserved5;       
        unsigned int reserved6;       
        unsigned int reserved7;       
} dasd_information2_t;

#define DASD_FORMAT_NONE 0
#define DASD_FORMAT_LDL  1
#define DASD_FORMAT_CDL  2
#define DASD_FEATURE_DEFAULT	     0x00
#define DASD_FEATURE_READONLY	     0x01
#define DASD_FEATURE_USEDIAG	     0x02
#define DASD_FEATURE_INITIAL_ONLINE  0x04
#define DASD_FEATURE_ERPLOG	     0x08
#define DASD_FEATURE_FAILFAST	     0x10
#define DASD_FEATURE_FAILONSLCK      0x20
#define DASD_FEATURE_USERAW	     0x40

#define DASD_PARTN_BITS 2

typedef struct dasd_information_t {
        unsigned int devno;         
        unsigned int real_devno;    
        unsigned int schid;         
        unsigned int cu_type  : 16; 
        unsigned int cu_model :  8; 
        unsigned int dev_type : 16; 
        unsigned int dev_model : 8; 
        unsigned int open_count; 
        unsigned int req_queue_len; 
        unsigned int chanq_len;     
        char type[4];               
        unsigned int status;        
        unsigned int label_block;   
        unsigned int FBA_layout;    
        unsigned int characteristics_size;
        unsigned int confdata_size;
        char characteristics[64];   
        char configuration_data[256]; 
} dasd_information_t;

 
typedef struct dasd_rssd_perf_stats_t {
	unsigned char  invalid:1;
	unsigned char  format:3;
	unsigned char  data_format:4;
	unsigned char  unit_address;
	unsigned short device_status;
	unsigned int   nr_read_normal;
	unsigned int   nr_read_normal_hits;
	unsigned int   nr_write_normal;
	unsigned int   nr_write_fast_normal_hits;
	unsigned int   nr_read_seq;
	unsigned int   nr_read_seq_hits;
	unsigned int   nr_write_seq;
	unsigned int   nr_write_fast_seq_hits;
	unsigned int   nr_read_cache;
	unsigned int   nr_read_cache_hits;
	unsigned int   nr_write_cache;
	unsigned int   nr_write_fast_cache_hits;
	unsigned int   nr_inhibit_cache;
	unsigned int   nr_bybass_cache;
	unsigned int   nr_seq_dasd_to_cache;
	unsigned int   nr_dasd_to_cache;
	unsigned int   nr_cache_to_dasd;
	unsigned int   nr_delayed_fast_write;
	unsigned int   nr_normal_fast_write;
	unsigned int   nr_seq_fast_write;
	unsigned int   nr_cache_miss;
	unsigned char  status2;
	unsigned int   nr_quick_write_promotes;
	unsigned char  reserved;
	unsigned short ssid;
	unsigned char  reseved2[96];
} __attribute__((packed)) dasd_rssd_perf_stats_t;

typedef struct dasd_profile_info_t {
        unsigned int dasd_io_reqs;	 
        unsigned int dasd_io_sects;	 
        unsigned int dasd_io_secs[32];	 
        unsigned int dasd_io_times[32];	 
        unsigned int dasd_io_timps[32];	 
        unsigned int dasd_io_time1[32];	 
        unsigned int dasd_io_time2[32];	 
        unsigned int dasd_io_time2ps[32]; 
        unsigned int dasd_io_time3[32];	 
        unsigned int dasd_io_nr_req[32]; 
} dasd_profile_info_t;

typedef struct format_data_t {
	unsigned int start_unit; 
	unsigned int stop_unit;  
	unsigned int blksize;	 
	unsigned int intensity;
} format_data_t;

#define DASD_FMT_INT_FMT_R0 1 
#define DASD_FMT_INT_FMT_HA 2 
#define DASD_FMT_INT_INVAL  4 
#define DASD_FMT_INT_COMPAT 8 


typedef struct attrib_data_t {
	unsigned char operation:3;     
	unsigned char reserved:5;      
	__u16         nr_cyl;          
	__u8          reserved2[29];   
} __attribute__ ((packed)) attrib_data_t;

#define DASD_NORMAL_CACHE  0x0
#define DASD_BYPASS_CACHE  0x1
#define DASD_INHIBIT_LOAD  0x2
#define DASD_SEQ_ACCESS    0x3
#define DASD_SEQ_PRESTAGE  0x4
#define DASD_REC_ACCESS    0x5

typedef struct dasd_symmio_parms {
	unsigned char reserved[8];	
	unsigned long long psf_data;	
	unsigned long long rssd_result; 
	int psf_data_len;
	int rssd_result_len;
} __attribute__ ((packed)) dasd_symmio_parms_t;

struct dasd_snid_data {
	struct {
		__u8 group:2;
		__u8 reserve:2;
		__u8 mode:1;
		__u8 res:3;
	} __attribute__ ((packed)) path_state;
	__u8 pgid[11];
} __attribute__ ((packed));

struct dasd_snid_ioctl_data {
	struct dasd_snid_data data;
	__u8 path_mask;
} __attribute__ ((packed));



#define BIODASDDISABLE _IO(DASD_IOCTL_LETTER,0) 
#define BIODASDENABLE  _IO(DASD_IOCTL_LETTER,1)  
#define BIODASDRSRV    _IO(DASD_IOCTL_LETTER,2) 
#define BIODASDRLSE    _IO(DASD_IOCTL_LETTER,3) 
#define BIODASDSLCK    _IO(DASD_IOCTL_LETTER,4) 
#define BIODASDPRRST   _IO(DASD_IOCTL_LETTER,5)
#define BIODASDQUIESCE _IO(DASD_IOCTL_LETTER,6) 
#define BIODASDRESUME  _IO(DASD_IOCTL_LETTER,7) 


#define DASDAPIVER     _IOR(DASD_IOCTL_LETTER,0,int)
#define BIODASDINFO    _IOR(DASD_IOCTL_LETTER,1,dasd_information_t)
#define BIODASDPRRD    _IOR(DASD_IOCTL_LETTER,2,dasd_profile_info_t)
#define BIODASDINFO2   _IOR(DASD_IOCTL_LETTER,3,dasd_information2_t)
#define BIODASDPSRD    _IOR(DASD_IOCTL_LETTER,4,dasd_rssd_perf_stats_t)
#define BIODASDGATTR   _IOR(DASD_IOCTL_LETTER,5,attrib_data_t) 


#define BIODASDFMT     _IOW(DASD_IOCTL_LETTER,1,format_data_t) 
#define BIODASDSATTR   _IOW(DASD_IOCTL_LETTER,2,attrib_data_t) 

#define BIODASDSNID    _IOWR(DASD_IOCTL_LETTER, 1, struct dasd_snid_ioctl_data)

#define BIODASDSYMMIO  _IOWR(DASD_IOCTL_LETTER, 240, dasd_symmio_parms_t)

#endif				

