#ifndef _GDTH_IOCTL_H
#define _GDTH_IOCTL_H


#define GDTIOCTL_MASK       ('J'<<8)
#define GDTIOCTL_GENERAL    (GDTIOCTL_MASK | 0) 
#define GDTIOCTL_DRVERS     (GDTIOCTL_MASK | 1) 
#define GDTIOCTL_CTRTYPE    (GDTIOCTL_MASK | 2) 
#define GDTIOCTL_OSVERS     (GDTIOCTL_MASK | 3) 
#define GDTIOCTL_HDRLIST    (GDTIOCTL_MASK | 4) 
#define GDTIOCTL_CTRCNT     (GDTIOCTL_MASK | 5) 
#define GDTIOCTL_LOCKDRV    (GDTIOCTL_MASK | 6) 
#define GDTIOCTL_LOCKCHN    (GDTIOCTL_MASK | 7) 
#define GDTIOCTL_EVENT      (GDTIOCTL_MASK | 8) 
#define GDTIOCTL_SCSI       (GDTIOCTL_MASK | 9) 
#define GDTIOCTL_RESET_BUS  (GDTIOCTL_MASK |10) 
#define GDTIOCTL_RESCAN     (GDTIOCTL_MASK |11) 
#define GDTIOCTL_RESET_DRV  (GDTIOCTL_MASK |12) 

#define GDTIOCTL_MAGIC  0xaffe0004
#define EVENT_SIZE      294 
#define GDTH_MAXSG      32                      

#define MAX_LDRIVES     255                     
#ifdef GDTH_IOCTL_PROC
#define MAX_HDRIVES     100                     
#else
#define MAX_HDRIVES     MAX_LDRIVES             
#endif

typedef struct {
    u32     sg_ptr;                         
    u32     sg_len;                         
} __attribute__((packed)) gdth_sg_str;

typedef struct {
    u64     sg_ptr;                         
    u32     sg_len;                         
} __attribute__((packed)) gdth_sg64_str;

typedef struct {
    u32     BoardNode;                      
    u32     CommandIndex;                   
    u16      OpCode;                         
    union {
        struct {
            u16      DeviceNo;               
            u32     BlockNo;                
            u32     BlockCnt;               
            u32     DestAddr;               
            u32     sg_canz;                
            gdth_sg_str sg_lst[GDTH_MAXSG];     
        } __attribute__((packed)) cache;                         
        struct {
            u16      DeviceNo;               
            u64     BlockNo;                
            u32     BlockCnt;               
            u64     DestAddr;               
            u32     sg_canz;                
            gdth_sg64_str sg_lst[GDTH_MAXSG];   
        } __attribute__((packed)) cache64;                       
        struct {
            u16      param_size;             
            u32     subfunc;                
            u32     channel;                
            u64     p_param;                
        } __attribute__((packed)) ioctl;                         
        struct {
            u16      reserved;
            union {
                struct {
                    u32  msg_handle;        
                    u64  msg_addr;          
                } __attribute__((packed)) msg;
                u8       data[12];          
            } su;
        } __attribute__((packed)) screen;                        
        struct {
            u16      reserved;
            u32     direction;              
            u32     mdisc_time;             
            u32     mcon_time;              
            u32     sdata;                  
            u32     sdlen;                  
            u32     clen;                   
            u8      cmd[12];                
            u8      target;                 
            u8      lun;                    
            u8      bus;                    
            u8      priority;               
            u32     sense_len;              
            u32     sense_data;             
            u32     link_p;                 
            u32     sg_ranz;                
            gdth_sg_str sg_lst[GDTH_MAXSG];     
        } __attribute__((packed)) raw;                           
        struct {
            u16      reserved;
            u32     direction;              
            u32     mdisc_time;             
            u32     mcon_time;              
            u64     sdata;                  
            u32     sdlen;                  
            u32     clen;                   
            u8      cmd[16];                
            u8      target;                 
            u8      lun;                    
            u8      bus;                    
            u8      priority;               
            u32     sense_len;              
            u64     sense_data;             
            u32     sg_ranz;                
            gdth_sg64_str sg_lst[GDTH_MAXSG];   
        } __attribute__((packed)) raw64;                         
    } u;
    
    u8      Service;                        
    u8      reserved;
    u16      Status;                         
    u32     Info;                           
    void        *RequestBuffer;                 
} __attribute__((packed)) gdth_cmd_str;

#define ES_ASYNC    1
#define ES_DRIVER   2
#define ES_TEST     3
#define ES_SYNC     4
typedef struct {
    u16                  size;               
    union {
        char                stream[16];
        struct {
            u16          ionode;
            u16          service;
            u32         index;
        } __attribute__((packed)) driver;
        struct {
            u16          ionode;
            u16          service;
            u16          status;
            u32         info;
            u8          scsi_coord[3];
        } __attribute__((packed)) async;
        struct {
            u16          ionode;
            u16          service;
            u16          status;
            u32         info;
            u16          hostdrive;
            u8          scsi_coord[3];
            u8          sense_key;
        } __attribute__((packed)) sync;
        struct {
            u32         l1, l2, l3, l4;
        } __attribute__((packed)) test;
    } eu;
    u32                 severity;
    u8                  event_string[256];          
} __attribute__((packed)) gdth_evt_data;

typedef struct {
    u32         first_stamp;
    u32         last_stamp;
    u16          same_count;
    u16          event_source;
    u16          event_idx;
    u8          application;
    u8          reserved;
    gdth_evt_data   event_data;
} __attribute__((packed)) gdth_evt_str;


#ifdef GDTH_IOCTL_PROC
typedef struct {
    u32                 magic;              
    u16                  ioctl;              
    u16                  ionode;             
    u16                  service;            
    u16                  timeout;            
    union {
        struct {
            u8          command[512];       
            u8          data[1];            
        } general;
        struct {
            u8          lock;               
            u8          drive_cnt;          
            u16          drives[MAX_HDRIVES];
        } lockdrv;
        struct {
            u8          lock;               
            u8          channel;            
        } lockchn;
        struct {
            int             erase;              
            int             handle;
            u8          evt[EVENT_SIZE];    
        } event;
        struct {
            u8          bus;                
            u8          target;             
            u8          lun;                
            u8          cmd_len;            
            u8          cmd[12];            
        } scsi;
        struct {
            u16          hdr_no;             
            u8          flag;               
        } rescan;
    } iu;
} gdth_iowr_str;

typedef struct {
    u32                 size;               
    u32                 status;             
    union {
        struct {
            u8          data[1];            
        } general;
        struct {
            u16          version;            
        } drvers;
        struct {
            u8          type;               
            u16          info;               
            u16          oem_id;             
            u16          bios_ver;           
            u16          access;             
            u16          ext_type;           
            u16          device_id;          
            u16          sub_device_id;      
        } ctrtype;
        struct {
            u8          version;            
            u8          subversion;         
            u16          revision;           
        } osvers;
        struct {
            u16          count;              
        } ctrcnt;
        struct {
            int             handle;
            u8          evt[EVENT_SIZE];    
        } event;
        struct {
            u8          bus;                
            u8          target;             
            u8          lun;                
            u8          cluster_type;       
        } hdr_list[MAX_HDRIVES];                
    } iu;
} gdth_iord_str;
#endif

typedef struct {
    u16 ionode;                              
    u16 timeout;                             
    u32 info;                                
    u16 status;                              
    unsigned long data_len;                             
    unsigned long sense_len;                            
    gdth_cmd_str command;                                          
} gdth_ioctl_general;

typedef struct {
    u16 ionode;                              
    u8 lock;                                
    u8 drive_cnt;                           
    u16 drives[MAX_HDRIVES];                 
} gdth_ioctl_lockdrv;

typedef struct {
    u16 ionode;                              
    u8 lock;                                
    u8 channel;                             
} gdth_ioctl_lockchn;

typedef struct {
    u8 version;                             
    u8 subversion;                          
    u16 revision;                            
} gdth_ioctl_osvers;

typedef struct {
    u16 ionode;                              
    u8 type;                                
    u16 info;                                
    u16 oem_id;                              
    u16 bios_ver;                            
    u16 access;                              
    u16 ext_type;                            
    u16 device_id;                           
    u16 sub_device_id;                       
} gdth_ioctl_ctrtype;

typedef struct {
    u16 ionode;
    int erase;                                  
    int handle;                                 
    gdth_evt_str event;
} gdth_ioctl_event;

typedef struct {
    u16 ionode;                              
    u8 flag;                                
    u16 hdr_no;                              
    struct {
        u8 bus;                             
        u8 target;                          
        u8 lun;                             
        u8 cluster_type;                    
    } hdr_list[MAX_HDRIVES];                    
} gdth_ioctl_rescan;

typedef struct {
    u16 ionode;                              
    u16 number;                              
    u16 status;                              
} gdth_ioctl_reset;

#endif
