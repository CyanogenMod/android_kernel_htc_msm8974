#ifndef _GDTH_H
#define _GDTH_H

/*
 * Header file for the GDT Disk Array/Storage RAID controllers driver for Linux
 * 
 * gdth.h Copyright (C) 1995-06 ICP vortex, Achim Leubner
 * See gdth.c for further informations and 
 * below for supported controller types
 *
 * <achim_leubner@adaptec.com>
 *
 * $Id: gdth.h,v 1.58 2006/01/11 16:14:09 achim Exp $
 */

#include <linux/types.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


#define GDTH_VERSION_STR        "3.05"
#define GDTH_VERSION            3
#define GDTH_SUBVERSION         5

#define PROTOCOL_VERSION        1

#define OEM_ID_ICP      0x941c
#define OEM_ID_INTEL    0x8000

#define GDT_ISA         0x01                    
#define GDT_EISA        0x02                    
#define GDT_PCI         0x03                    
#define GDT_PCINEW      0x04                    
#define GDT_PCIMPR      0x05                    
#define GDT3_ID         0x0130941c              
#define GDT3A_ID        0x0230941c              
#define GDT3B_ID        0x0330941c              
#define GDT2_ID         0x0120941c              

#ifndef PCI_VENDOR_ID_VORTEX
#define PCI_VENDOR_ID_VORTEX            0x1119  
#endif
#ifndef PCI_VENDOR_ID_INTEL
#define PCI_VENDOR_ID_INTEL             0x8086  
#endif

#ifndef PCI_DEVICE_ID_VORTEX_GDT60x0
#define PCI_DEVICE_ID_VORTEX_GDT60x0    0       
#define PCI_DEVICE_ID_VORTEX_GDT6000B   1       
#define PCI_DEVICE_ID_VORTEX_GDT6x10    2       
#define PCI_DEVICE_ID_VORTEX_GDT6x20    3       
#define PCI_DEVICE_ID_VORTEX_GDT6530    4       
#define PCI_DEVICE_ID_VORTEX_GDT6550    5       
#define PCI_DEVICE_ID_VORTEX_GDT6x17    6       
#define PCI_DEVICE_ID_VORTEX_GDT6x27    7       
#define PCI_DEVICE_ID_VORTEX_GDT6537    8       
#define PCI_DEVICE_ID_VORTEX_GDT6557    9       
#define PCI_DEVICE_ID_VORTEX_GDT6x15    10      
#define PCI_DEVICE_ID_VORTEX_GDT6x25    11      
#define PCI_DEVICE_ID_VORTEX_GDT6535    12      
#define PCI_DEVICE_ID_VORTEX_GDT6555    13      
#endif

#ifndef PCI_DEVICE_ID_VORTEX_GDT6x17RP
#define PCI_DEVICE_ID_VORTEX_GDT6x17RP  0x100   
#define PCI_DEVICE_ID_VORTEX_GDT6x27RP  0x101   
#define PCI_DEVICE_ID_VORTEX_GDT6537RP  0x102   
#define PCI_DEVICE_ID_VORTEX_GDT6557RP  0x103   
#define PCI_DEVICE_ID_VORTEX_GDT6x11RP  0x104   
#define PCI_DEVICE_ID_VORTEX_GDT6x21RP  0x105   
#endif
#ifndef PCI_DEVICE_ID_VORTEX_GDT6x17RD
#define PCI_DEVICE_ID_VORTEX_GDT6x17RD  0x110   
#define PCI_DEVICE_ID_VORTEX_GDT6x27RD  0x111   
#define PCI_DEVICE_ID_VORTEX_GDT6537RD  0x112   
#define PCI_DEVICE_ID_VORTEX_GDT6557RD  0x113   
#define PCI_DEVICE_ID_VORTEX_GDT6x11RD  0x114   
#define PCI_DEVICE_ID_VORTEX_GDT6x21RD  0x115   
#define PCI_DEVICE_ID_VORTEX_GDT6x18RD  0x118   
#define PCI_DEVICE_ID_VORTEX_GDT6x28RD  0x119   
#define PCI_DEVICE_ID_VORTEX_GDT6x38RD  0x11A   
#define PCI_DEVICE_ID_VORTEX_GDT6x58RD  0x11B   
#define PCI_DEVICE_ID_VORTEX_GDT7x18RN  0x168   
#define PCI_DEVICE_ID_VORTEX_GDT7x28RN  0x169   
#define PCI_DEVICE_ID_VORTEX_GDT7x38RN  0x16A   
#define PCI_DEVICE_ID_VORTEX_GDT7x58RN  0x16B   
#endif

#ifndef PCI_DEVICE_ID_VORTEX_GDT6x19RD
#define PCI_DEVICE_ID_VORTEX_GDT6x19RD  0x210   
#define PCI_DEVICE_ID_VORTEX_GDT6x29RD  0x211   
#define PCI_DEVICE_ID_VORTEX_GDT7x19RN  0x260   
#define PCI_DEVICE_ID_VORTEX_GDT7x29RN  0x261   
#endif

#ifndef PCI_DEVICE_ID_VORTEX_GDTMAXRP
#define PCI_DEVICE_ID_VORTEX_GDTMAXRP   0x2ff   
#endif

#ifndef PCI_DEVICE_ID_VORTEX_GDTNEWRX
#define PCI_DEVICE_ID_VORTEX_GDTNEWRX   0x300
#endif

#ifndef PCI_DEVICE_ID_VORTEX_GDTNEWRX2
#define PCI_DEVICE_ID_VORTEX_GDTNEWRX2  0x301
#endif        

#ifndef PCI_DEVICE_ID_INTEL_SRC
#define PCI_DEVICE_ID_INTEL_SRC         0x600
#endif

#ifndef PCI_DEVICE_ID_INTEL_SRC_XSCALE
#define PCI_DEVICE_ID_INTEL_SRC_XSCALE  0x601
#endif

#define GDTH_SCRATCH    PAGE_SIZE               
#define GDTH_MAXCMDS    120
#define GDTH_MAXC_P_L   16                      
#define GDTH_MAX_RAW    2                       
#define MAXOFFSETS      128
#define MAXHA           16
#define MAXID           127
#define MAXLUN          8
#define MAXBUS          6
#define MAX_EVENTS      100                     
#define MAX_RES_ARGS    40                      
#define MAXCYLS         1024
#define HEADS           64
#define SECS            32                      
#define MEDHEADS        127
#define MEDSECS         63                      
#define BIGHEADS        255
#define BIGSECS         63                      

#define UNUSED_CMND     ((Scsi_Cmnd *)-1)
#define INTERNAL_CMND   ((Scsi_Cmnd *)-2)
#define SCREEN_CMND     ((Scsi_Cmnd *)-3)
#define SPECIAL_SCP(p)  (p==UNUSED_CMND || p==INTERNAL_CMND || p==SCREEN_CMND)

#define SCSIRAWSERVICE  3
#define CACHESERVICE    9
#define SCREENSERVICE   11

#define MSG_INV_HANDLE  -1                      
#define MSGLEN          16                      
#define MSG_SIZE        34                      
#define MSG_REQUEST     0                       

#define SECTOR_SIZE     0x200                   

#define DPMEM_MAGIC     0xC0FFEE11
#define IC_HEADER_BYTES 48
#define IC_QUEUE_BYTES  4
#define DPMEM_COMMAND_OFFSET    IC_HEADER_BYTES+IC_QUEUE_BYTES*MAXOFFSETS

#define CLUSTER_DRIVE         1
#define CLUSTER_MOUNTED       2
#define CLUSTER_RESERVED      4
#define CLUSTER_RESERVE_STATE (CLUSTER_DRIVE|CLUSTER_MOUNTED|CLUSTER_RESERVED)

#define GDT_INIT        0                       
#define GDT_READ        1                       
#define GDT_WRITE       2                       
#define GDT_INFO        3                       
#define GDT_FLUSH       4                       
#define GDT_IOCTL       5                       
#define GDT_DEVTYPE     9                       
#define GDT_MOUNT       10                      
#define GDT_UNMOUNT     11                      
#define GDT_SET_FEAT    12                      
#define GDT_GET_FEAT    13                      
#define GDT_WRITE_THR   16                      
#define GDT_READ_THR    17                      
#define GDT_EXT_INFO    18                      
#define GDT_RESET       19                      
#define GDT_RESERVE_DRV 20                      
#define GDT_RELEASE_DRV 21                      
#define GDT_CLUST_INFO  22                      
#define GDT_RW_ATTRIBS  23                      
#define GDT_CLUST_RESET 24                      
#define GDT_FREEZE_IO   25                      
#define GDT_UNFREEZE_IO 26                      
#define GDT_X_INIT_HOST 29                      
#define GDT_X_INFO      30                      

#define GDT_RESERVE     14                      
#define GDT_RELEASE     15                      
#define GDT_RESERVE_ALL 16                      
#define GDT_RELEASE_ALL 17                      
#define GDT_RESET_BUS   18                      
#define GDT_SCAN_START  19                      
#define GDT_SCAN_END    20                        
#define GDT_X_INIT_RAW  21                      

#define GDT_REALTIME    3                       
#define GDT_X_INIT_SCR  4                       

#define SCSI_DR_INFO    0x00                                       
#define SCSI_CHAN_CNT   0x05                       
#define SCSI_DR_LIST    0x06                    
#define SCSI_DEF_CNT    0x15                    
#define DSK_STATISTICS  0x4b                    
#define IOCHAN_DESC     0x5d                    
#define IOCHAN_RAW_DESC 0x5e                    
#define L_CTRL_PATTERN  0x20000000L             
#define ARRAY_INFO      0x12                    
#define ARRAY_DRV_LIST  0x0f                    
#define ARRAY_DRV_LIST2 0x34                    
#define LA_CTRL_PATTERN 0x10000000L             
#define CACHE_DRV_CNT   0x01                    
#define CACHE_DRV_LIST  0x02                    
#define CACHE_INFO      0x04                    
#define CACHE_CONFIG    0x05                    
#define CACHE_DRV_INFO  0x07                    
#define BOARD_FEATURES  0x15                    
#define BOARD_INFO      0x28                    
#define SET_PERF_MODES  0x82                    
#define GET_PERF_MODES  0x83                    
#define CACHE_READ_OEM_STRING_RECORD 0x84        
#define HOST_GET        0x10001L                
#define IO_CHANNEL      0x00020000L             
#define INVALID_CHANNEL 0x0000ffffL             

#define S_OK            1                       
#define S_GENERR        6                       
#define S_BSY           7                       
#define S_CACHE_UNKNOWN 12                      
#define S_RAW_SCSI      12                      
#define S_RAW_ILL       0xff                    
#define S_NOFUNC        -2                      
#define S_CACHE_RESERV  -24                        

#define INIT_RETRIES    100000                  
#define INIT_TIMEOUT    100000                  
#define POLL_TIMEOUT    10000                   

#define DEFAULT_PRI     0x20
#define IOCTL_PRI       0x10
#define HIGH_PRI        0x08

#define GDTH_DATA_IN    0x01000000L             
#define GDTH_DATA_OUT   0x00000000L             

#define ID0REG          0x0c80                  
#define EINTENABREG     0x0c89                  
#define SEMA0REG        0x0c8a                  
#define SEMA1REG        0x0c8b                  
#define LDOORREG        0x0c8d                  
#define EDENABREG       0x0c8e                  
#define EDOORREG        0x0c8f                  
#define MAILBOXREG      0x0c90                  
#define EISAREG         0x0cc0                  

#define LINUX_OS        8                       
#define SECS32          0x1f                    
#define BIOS_ID_OFFS    0x10                    
#define LOCALBOARD      0                       
#define ASYNCINDEX      0                       
#define SPEZINDEX       1                       
#define COALINDEX       (GDTH_MAXCMDS + 2)

#define SCATTER_GATHER  1                       
#define GDT_WR_THROUGH  0x100                   
#define GDT_64BIT       0x200                   

#include "gdth_ioctl.h"

typedef struct {                               
    u32     msg_handle;                     
    u32     msg_len;                        
    u32     msg_alen;                       
    u8      msg_answer;                     
    u8      msg_ext;                        
    u8      msg_reserved[2];
    char        msg_text[MSGLEN+2];             
} __attribute__((packed)) gdth_msg_str;



typedef struct {
    u32     status;
    u32     ext_status;
    u32     info0;
    u32     info1;
} __attribute__((packed)) gdth_coal_status;

typedef struct {
    u32     version;            
    u32     st_mode;            
    u32     st_buff_addr1;      
    u32     st_buff_u_addr1;    
    u32     st_buff_indx1;      
    u32     st_buff_addr2;      
    u32     st_buff_u_addr2;    
    u32     st_buff_indx2;      
    u32     st_buff_size;       
    u32     cmd_mode;            
    u32     cmd_buff_addr1;        
    u32     cmd_buff_u_addr1;   
    u32     cmd_buff_indx1;     
    u32     cmd_buff_addr2;        
    u32     cmd_buff_u_addr2;   
    u32     cmd_buff_indx2;     
    u32     cmd_buff_size;      
    u32     reserved1;
    u32     reserved2;
} __attribute__((packed)) gdth_perf_modes;

typedef struct {
    u8      vendor[8];                      
    u8      product[16];                    
    u8      revision[4];                    
    u32     sy_rate;                        
    u32     sy_max_rate;                    
    u32     no_ldrive;                      
    u32     blkcnt;                         
    u16      blksize;                        
    u8      available;                      
    u8      init;                           
    u8      devtype;                        
    u8      rm_medium;                      
    u8      wp_medium;                      
    u8      ansi;                           
    u8      protocol;                       
    u8      sync;                           
    u8      disc;                           
    u8      queueing;                       
    u8      cached;                         
    u8      target_id;                      
    u8      lun;                            
    u8      orphan;                         
    u32     last_error;                     
    u32     last_result;                    
    u32     check_errors;                   
    u8      percent;                        
    u8      last_check;                     
    u8      res[2];
    u32     flags;                          
    u8      multi_bus;                      
    u8      mb_status;                      
    u8      res2[2];
    u8      mb_alt_status;                  
    u8      mb_alt_bid;                     
    u8      mb_alt_tid;                     
    u8      res3;
    u8      fc_flag;                        
    u8      res4;
    u16      fc_frame_size;                  
    char        wwn[8];                         
} __attribute__((packed)) gdth_diskinfo_str;

typedef struct {
    u32     channel_no;                     
    u32     drive_cnt;                      
    u8      siop_id;                        
    u8      siop_state;                      
} __attribute__((packed)) gdth_getch_str;

typedef struct {
    u32     sc_no;                          
    u32     sc_cnt;                         
    u32     sc_list[MAXID];                 
} __attribute__((packed)) gdth_drlist_str;

typedef struct {
    u8      sddc_type;                      
    u8      sddc_format;                    
    u8      sddc_len;                       
    u8      sddc_res;
    u32     sddc_cnt;                       
} __attribute__((packed)) gdth_defcnt_str;

typedef struct {
    u32     bid;                            
    u32     first;                          
    u32     entries;                        
    u32     count;                          
    u32     mon_time;                       
    struct {
        u8  tid;                            
        u8  lun;                            
        u8  res[2];
        u32 blk_size;                       
        u32 rd_count;                       
        u32 wr_count;                       /* bytes written */
        u32 rd_blk_count;                   
        u32 wr_blk_count;                   /* blocks written */
        u32 retries;                        
        u32 reassigns;                      
    } __attribute__((packed)) list[1];
} __attribute__((packed)) gdth_dskstat_str;

typedef struct {
    u32     version;                        
    u8      list_entries;                   
    u8      first_chan;                     
    u8      last_chan;                      
    u8      chan_count;                     
    u32     list_offset;                    
} __attribute__((packed)) gdth_iochan_header;

typedef struct {
    gdth_iochan_header  hdr;
    struct {
        u32         address;                
        u8          type;                   
        u8          local_no;               
        u16          features;               
    } __attribute__((packed)) list[MAXBUS];
} __attribute__((packed)) gdth_iochan_str;

typedef struct {
    gdth_iochan_header  hdr;
    struct {
        u8      proc_id;                    
        u8      proc_defect;                
        u8      reserved[2];
    } __attribute__((packed)) list[MAXBUS];
} __attribute__((packed)) gdth_raw_iochan_str;

typedef struct {
    u32     al_controller;                  
    u8      al_cache_drive;                 
    u8      al_status;                      
    u8      al_res[2];     
} __attribute__((packed)) gdth_arraycomp_str;

typedef struct {
    u8      ai_type;                        
    u8      ai_cache_drive_cnt;             
    u8      ai_state;                       
    u8      ai_master_cd;                   
    u32     ai_master_controller;           
    u32     ai_size;                        
    u32     ai_striping_size;               
    u32     ai_secsize;                     
    u32     ai_err_info;                    
    u8      ai_name[8];                     
    u8      ai_controller_cnt;              
    u8      ai_removable;                   
    u8      ai_write_protected;             
    u8      ai_devtype;                     
    gdth_arraycomp_str  ai_drives[35];          
    u8      ai_drive_entries;               
    u8      ai_protected;                   
    u8      ai_verify_state;                
    u8      ai_ext_state;                   
    u8      ai_expand_state;                
    u8      ai_reserved[3];
} __attribute__((packed)) gdth_arrayinf_str;

typedef struct {
    u32     controller_no;                  
    u8      cd_handle;                      
    u8      is_arrayd;                      
    u8      is_master;                      
    u8      is_parity;                      
    u8      is_hotfix;                      
    u8      res[3];
} __attribute__((packed)) gdth_alist_str;

typedef struct {
    u32     entries_avail;                  
    u32     entries_init;                   
    u32     first_entry;                    
    u32     list_offset;                    
    gdth_alist_str list[1];                     
} __attribute__((packed)) gdth_arcdl_str;

typedef struct {
    u32     version;                        
    u16      state;                          
    u16      strategy;                       
    u16      write_back;                     
    u16      block_size;                     
} __attribute__((packed)) gdth_cpar_str;

typedef struct {
    u32     csize;                          
    u32     read_cnt;                       
    u32     write_cnt;
    u32     tr_hits;                        
    u32     sec_hits;
    u32     sec_miss;                       
} __attribute__((packed)) gdth_cstat_str;

typedef struct {
    gdth_cpar_str   cpar;
    gdth_cstat_str  cstat;
} __attribute__((packed)) gdth_cinfo_str;

typedef struct {
    u8      cd_name[8];                     
    u32     cd_devtype;                     
    u32     cd_ldcnt;                       
    u32     cd_last_error;                  
    u8      cd_initialized;                 
    u8      cd_removable;                   
    u8      cd_write_protected;             
    u8      cd_flags;                       
    u32     ld_blkcnt;                      
    u32     ld_blksize;                     
    u32     ld_dcnt;                        
    u32     ld_slave;                       
    u32     ld_dtype;                       
    u32     ld_last_error;                  
    u8      ld_name[8];                     
    u8      ld_error;                       
} __attribute__((packed)) gdth_cdrinfo_str;

typedef struct {
    u32     ctl_version;
    u32     file_major_version;
    u32     file_minor_version;
    u32     buffer_size;
    u32     cpy_count;
    u32     ext_error;
    u32     oem_id;
    u32     board_id;
} __attribute__((packed)) gdth_oem_str_params;

typedef struct {
    u8      product_0_1_name[16];
    u8      product_4_5_name[16];
    u8      product_cluster_name[16];
    u8      product_reserved[16];
    u8      scsi_cluster_target_vendor_id[16];
    u8      cluster_raid_fw_name[16];
    u8      oem_brand_name[16];
    u8      oem_raid_type[16];
    u8      bios_type[13];
    u8      bios_title[50];
    u8      oem_company_name[37];
    u32     pci_id_1;
    u32     pci_id_2;
    u8      validation_status[80];
    u8      reserved_1[4];
    u8      scsi_host_drive_inquiry_vendor_id[16];
    u8      library_file_template[16];
    u8      reserved_2[16];
    u8      tool_name_1[32];
    u8      tool_name_2[32];
    u8      tool_name_3[32];
    u8      oem_contact_1[84];
    u8      oem_contact_2[84];
    u8      oem_contact_3[84];
} __attribute__((packed)) gdth_oem_str;

typedef struct {
    gdth_oem_str_params params;
    gdth_oem_str        text;
} __attribute__((packed)) gdth_oem_str_ioctl;

typedef struct {
    u8      chaining;                       
    u8      striping;                       
    u8      mirroring;                      
    u8      raid;                           
} __attribute__((packed)) gdth_bfeat_str;

typedef struct {
    u32     ser_no;                         
    u8      oem_id[2];                      
    u16      ep_flags;                       
    u32     proc_id;                        
    u32     memsize;                        
    u8      mem_banks;                      
    u8      chan_type;                      
    u8      chan_count;                     
    u8      rdongle_pres;                   
    u32     epr_fw_ver;                     
    u32     upd_fw_ver;                     
    u32     upd_revision;                   
    char        type_string[16];                
    char        raid_string[16];                
    u8      update_pres;                    
    u8      xor_pres;                       
    u8      prom_type;                      
    u8      prom_count;                     
    u32     dup_pres;                       
    u32     chan_pres;                      
    u32     mem_pres;                       
    u8      ft_bus_system;                  
    u8      subtype_valid;                  
    u8      board_subtype;                  
    u8      ramparity_pres;                 
} __attribute__((packed)) gdth_binfo_str; 

typedef struct {
    char        name[8];                        
    u32     size;                           
    u8      host_drive;                     
    u8      log_drive;                      
    u8      reserved;
    u8      rw_attribs;                     
    u32     start_sec;                      
} __attribute__((packed)) gdth_hentry_str;

typedef struct {
    u32     entries;                        
    u32     offset;                         
    u8      secs_p_head;                    
    u8      heads_p_cyl;                    
    u8      reserved;
    u8      clust_drvtype;                  
    u32     location;                       
    gdth_hentry_str entry[MAX_HDRIVES];         
} __attribute__((packed)) gdth_hget_str;    



typedef struct {
    u8              S_Cmd_Indx;             
    u8 volatile     S_Status;               
    u16              reserved1;
    u32             S_Info[4];              
    u8 volatile     Sema0;                  
    u8              reserved2[3];
    u8              Cmd_Index;              
    u8              reserved3[3];
    u16 volatile     Status;                 
    u16              Service;                
    u32             Info[2];                
    struct {
        u16          offset;                 
        u16          serv_id;                
    } __attribute__((packed)) comm_queue[MAXOFFSETS];            
    u32             bios_reserved[2];
    u8              gdt_dpr_cmd[1];         
} __attribute__((packed)) gdt_dpr_if;

typedef struct {
    u32     magic;                          
    u16      need_deinit;                    
    u8      switch_support;                 
    u8      padding[9];
    u8      os_used[16];                    
    u8      unused[28];
    u8      fw_magic;                       
} __attribute__((packed)) gdt_pci_sram;

typedef struct {
    u8      os_used[16];                    
    u16      need_deinit;                    
    u8      switch_support;                 
    u8      padding;
} __attribute__((packed)) gdt_eisa_sram;


typedef struct {
    union {
        struct {
            u8      bios_used[0x3c00-32];   
            u32     magic;                  
            u16      need_deinit;            
            u8      switch_support;         
            u8      padding[9];
            u8      os_used[16];            
        } __attribute__((packed)) dp_sram;
        u8          bios_area[0x4000];      
    } bu;
    union {
        gdt_dpr_if      ic;                     
        u8          if_area[0x3000];        
    } u;
    struct {
        u8          memlock;                
        u8          event;                  
        u8          irqen;                  
        u8          irqdel;                 
        u8 volatile Sema1;                  
        u8          rq;                     
    } __attribute__((packed)) io;
} __attribute__((packed)) gdt2_dpram_str;

typedef struct {
    union {
        gdt_dpr_if      ic;                     
        u8          if_area[0xff0-sizeof(gdt_pci_sram)];
    } u;
    gdt_pci_sram        gdt6sr;                 
    struct {
        u8          unused0[1];
        u8 volatile Sema1;                  
        u8          unused1[3];
        u8          irqen;                  
        u8          unused2[2];
        u8          event;                  
        u8          unused3[3];
        u8          irqdel;                 
        u8          unused4[3];
    } __attribute__((packed)) io;
} __attribute__((packed)) gdt6_dpram_str;

typedef struct {
    u8              cfg_reg;        
    u8              unused1[0x3f];
    u8 volatile     sema0_reg;              
    u8 volatile     sema1_reg;              
    u8              unused2[2];
    u16 volatile     status;                 
    u16              service;                
    u32             info[2];                
    u8              unused3[0x10];
    u8              ldoor_reg;              
    u8              unused4[3];
    u8 volatile     edoor_reg;              
    u8              unused5[3];
    u8              control0;               
    u8              control1;               
    u8              unused6[0x16];
} __attribute__((packed)) gdt6c_plx_regs;

typedef struct {
    union {
        gdt_dpr_if      ic;                     
        u8          if_area[0x4000-sizeof(gdt_pci_sram)];
    } u;
    gdt_pci_sram        gdt6sr;                 
} __attribute__((packed)) gdt6c_dpram_str;

typedef struct {
    u8              unused1[16];
    u8 volatile     sema0_reg;              
    u8              unused2;
    u8 volatile     sema1_reg;              
    u8              unused3;
    u16 volatile     status;                 
    u16              service;                
    u32             info[2];                
    u8              ldoor_reg;              
    u8              unused4[11];
    u8 volatile     edoor_reg;              
    u8              unused5[7];
    u8              edoor_en_reg;           
    u8              unused6[27];
    u32             unused7[939];         
    u32             severity;       
    char                evt_str[256];           
} __attribute__((packed)) gdt6m_i960_regs;

typedef struct {
    gdt6m_i960_regs     i960r;                  
    union {
        gdt_dpr_if      ic;                     
        u8          if_area[0x3000-sizeof(gdt_pci_sram)];
    } u;
    gdt_pci_sram        gdt6sr;                 
} __attribute__((packed)) gdt6m_dpram_str;


typedef struct {
    struct pci_dev      *pdev;
    unsigned long               dpmem;                  
    unsigned long               io;                     
} gdth_pci_str;


typedef struct {
    struct Scsi_Host    *shost;
    struct list_head    list;
    u16      	hanum;
    u16              oem_id;                 
    u16              type;                   
    u32             stype;                  
    u16              fw_vers;                
    u16              cache_feat;             
    u16              raw_feat;               
    u16              screen_feat;            
    u16              bmic;                   
    void __iomem        *brd;                   
    u32             brd_phys;               
    gdt6c_plx_regs      *plx;                   
    gdth_cmd_str        cmdext;
    gdth_cmd_str        *pccb;                  
    u32             ccb_phys;               
#ifdef INT_COAL
    gdth_coal_status    *coal_stat;             
    u64             coal_stat_phys;         
#endif
    char                *pscratch;              
    u64             scratch_phys;           
    u8              scratch_busy;           
    u8              dma64_support;          
    gdth_msg_str        *pmsg;                  
    u64             msg_phys;               
    u8              scan_mode;              
    u8              irq;                    
    u8              drq;                    
    u16              status;                 
    u16              service;                
    u32             info;
    u32             info2;                  
    Scsi_Cmnd           *req_first;             
    struct {
        u8          present;                
        u8          is_logdrv;              
        u8          is_arraydrv;            
        u8          is_master;              
        u8          is_parity;              
        u8          is_hotfix;              
        u8          master_no;              
        u8          lock;                   
        u8          heads;                  
        u8          secs;
        u16          devtype;                
        u64         size;                   
        u8          ldr_no;                 
        u8          rw_attribs;             
        u8          cluster_type;           
        u8          media_changed;          
        u32         start_sec;              
    } hdr[MAX_LDRIVES];                         
    struct {
        u8          lock;                   
        u8          pdev_cnt;               
        u8          local_no;               
        u8          io_cnt[MAXID];          
        u32         address;                
        u32         id_list[MAXID];         
    } raw[MAXBUS];                              
    struct {
        Scsi_Cmnd       *cmnd;                  
        u16          service;                
    } cmd_tab[GDTH_MAXCMDS];                    
    struct gdth_cmndinfo {                      
        int index;
        int internal_command;                   
        gdth_cmd_str *internal_cmd_str;         
        dma_addr_t sense_paddr;                 
        u8 priority;
	int timeout_count;			
        volatile int wait_for_completion;
        u16 status;
        u32 info;
        enum dma_data_direction dma_dir;
        int phase;                              
        int OpCode;
    } cmndinfo[GDTH_MAXCMDS];                   
    u8              bus_cnt;                
    u8              tid_cnt;                
    u8              bus_id[MAXBUS];         
    u8              virt_bus;               
    u8              more_proc;              
    u16              cmd_cnt;                
    u16              cmd_len;                
    u16              cmd_offs_dpmem;         
    u16              ic_all_size;            
    gdth_cpar_str       cpar;                   
    gdth_bfeat_str      bfeat;                  
    gdth_binfo_str      binfo;                  
    gdth_evt_data       dvr;                    
    spinlock_t          smp_lock;
    struct pci_dev      *pdev;
    char                oem_name[8];
#ifdef GDTH_DMA_STATISTICS
    unsigned long               dma32_cnt, dma64_cnt;   
#endif
    struct scsi_device         *sdev;
} gdth_ha_str;

static inline struct gdth_cmndinfo *gdth_cmnd_priv(struct scsi_cmnd* cmd)
{
	return (struct gdth_cmndinfo *)cmd->host_scribble;
}

typedef struct {
    u8      type_qual;
    u8      modif_rmb;
    u8      version;
    u8      resp_aenc;
    u8      add_length;
    u8      reserved1;
    u8      reserved2;
    u8      misc;
    u8      vendor[8];
    u8      product[16];
    u8      revision[4];
} __attribute__((packed)) gdth_inq_data;

typedef struct {
    u32     last_block_no;
    u32     block_length;
} __attribute__((packed)) gdth_rdcap_data;

typedef struct {
    u64     last_block_no;
    u32     block_length;
} __attribute__((packed)) gdth_rdcap16_data;

typedef struct {
    u8      errorcode;
    u8      segno;
    u8      key;
    u32     info;
    u8      add_length;
    u32     cmd_info;
    u8      adsc;
    u8      adsq;
    u8      fruc;
    u8      key_spec[3];
} __attribute__((packed)) gdth_sense_data;

typedef struct {
    struct {
        u8  data_length;
        u8  med_type;
        u8  dev_par;
        u8  bd_length;
    } __attribute__((packed)) hd;
    struct {
        u8  dens_code;
        u8  block_count[3];
        u8  reserved;
        u8  block_length[3];
    } __attribute__((packed)) bd;
} __attribute__((packed)) gdth_modep_data;

typedef struct {
    unsigned long       b[10];                          
} __attribute__((packed)) gdth_stackframe;



int gdth_proc_info(struct Scsi_Host *, char *,char **,off_t,int,int);

#endif
