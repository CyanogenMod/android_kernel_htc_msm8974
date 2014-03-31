
#include <asm/byteorder.h>
#include <linux/completion.h>
#include <linux/mutex.h>

typedef struct {
	unsigned	device_type	:5;	
	unsigned	reserved0_765	:3;	
	unsigned	reserved1_6t0	:7;	
	unsigned	rmb		:1;	
	unsigned	ansi_version	:3;	
	unsigned	ecma_version	:3;	
	unsigned	iso_version	:2;	
	unsigned	response_format :4;	
	unsigned	reserved3_45	:2;	
	unsigned	reserved3_6	:1;	
	unsigned	reserved3_7	:1;	
	u8		additional_length;	
	u8		rsv5, rsv6, rsv7;	
	u8		vendor_id[8];		
	u8		product_id[16];		
	u8		revision_level[4];	
	u8		vendor_specific[20];	
	u8		reserved56t95[40];	
						
} idetape_inquiry_result_t;

typedef struct {
	unsigned	reserved0_10	:2;	
	unsigned	bpu		:1;		
	unsigned	reserved0_543	:3;	
	unsigned	eop		:1;	
	unsigned	bop		:1;	
	u8		partition;		
	u8		reserved2, reserved3;	
	u32		first_block;		
	u32		last_block;		
	u8		reserved12;		
	u8		blocks_in_buffer[3];	
	u32		bytes_in_buffer;	
} idetape_read_position_result_t;

#define COMPRESSION_PAGE           0x0f
#define COMPRESSION_PAGE_LENGTH    16

#define CAPABILITIES_PAGE          0x2a
#define CAPABILITIES_PAGE_LENGTH   20

#define TAPE_PARAMTR_PAGE          0x2b
#define TAPE_PARAMTR_PAGE_LENGTH   16

#define NUMBER_RETRIES_PAGE        0x2f
#define NUMBER_RETRIES_PAGE_LENGTH 4

#define BLOCK_SIZE_PAGE            0x30
#define BLOCK_SIZE_PAGE_LENGTH     4

#define BUFFER_FILLING_PAGE        0x33
#define BUFFER_FILLING_PAGE_LENGTH 4

#define VENDOR_IDENT_PAGE          0x36
#define VENDOR_IDENT_PAGE_LENGTH   8

#define LOCATE_STATUS_PAGE         0x37
#define LOCATE_STATUS_PAGE_LENGTH  0

#define MODE_HEADER_LENGTH         4


typedef struct {
	unsigned	error_code	:7;	
	unsigned	valid		:1;	
	u8		reserved1	:8;	
	unsigned	sense_key	:4;	
	unsigned	reserved2_4	:1;	
	unsigned	ili		:1;	
	unsigned	eom		:1;	
	unsigned	filemark 	:1;	
	u32		information __attribute__ ((packed));
	u8		asl;			
	u32		command_specific;	
	u8		asc;			
	u8		ascq;			
	u8		replaceable_unit_code;	
	unsigned	sk_specific1 	:7;	
	unsigned	sksv		:1;	
	u8		sk_specific2;		
	u8		sk_specific3;		
	u8		pad[2];			
} idetape_request_sense_result_t;

typedef struct {
        u8              mode_data_length;       
        u8              medium_type;            
        u8              dsp;                    
        u8              bdl;                    
} osst_mode_parameter_header_t;

typedef struct {
        u8              density_code;           
        u8              blocks[3];              
        u8              reserved4;              
        u8              length[3];              
} osst_parameter_block_descriptor_t;

typedef struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        ps              :1;
        unsigned        reserved0       :1;     
	unsigned        page_code       :6;     
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        page_code       :6;     
        unsigned        reserved0       :1;     
        unsigned        ps              :1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
        u8              page_length;            
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        dce             :1;     
        unsigned        dcc             :1;     
	unsigned        reserved2       :6;     
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        reserved2       :6;     
        unsigned        dcc             :1;     
        unsigned        dce             :1;     
#else
#error "Please fix <asm/byteorder.h>"
#endif
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        dde             :1;     
        unsigned        red             :2;     
	unsigned        reserved3       :5;     
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        reserved3       :5;     
        unsigned        red             :2;     
        unsigned        dde             :1;     
#else
#error "Please fix <asm/byteorder.h>"
#endif
        u32             ca;                     
        u32             da;                     
        u8              reserved[4];            
} osst_data_compression_page_t;

typedef struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        ps              :1;
        unsigned        reserved1_6     :1;     
	unsigned        page_code       :6;     
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        page_code       :6;     
        unsigned        reserved1_6     :1;     
        unsigned        ps              :1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
        u8              page_length;            
        u8              map;                    
        u8              apd;                    
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        fdp             :1;     
        unsigned        sdp             :1;     
        unsigned        idp             :1;     
        unsigned        psum            :2;     
	unsigned        reserved4_012   :3;     
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        reserved4_012   :3;     
        unsigned        psum            :2;     
        unsigned        idp             :1;     
        unsigned        sdp             :1;     
        unsigned        fdp             :1;     
#else
#error "Please fix <asm/byteorder.h>"
#endif
        u8              mfr;                    
        u8              reserved[2];            
} osst_medium_partition_page_t;

typedef struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        reserved1_67    :2;
	unsigned        page_code       :6;     
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        page_code       :6;     
        unsigned        reserved1_67    :2;
#else
#error "Please fix <asm/byteorder.h>"
#endif
        u8              page_length;            
        u8              reserved2, reserved3;
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        reserved4_67    :2;
        unsigned        sprev           :1;     
        unsigned        reserved4_1234  :4;
	unsigned        ro              :1;     
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        ro              :1;     
        unsigned        reserved4_1234  :4;
        unsigned        sprev           :1;     
        unsigned        reserved4_67    :2;
#else
#error "Please fix <asm/byteorder.h>"
#endif
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        reserved5_67    :2;
        unsigned        qfa             :1;     
        unsigned        reserved5_4     :1;
        unsigned        efmt            :1;     
	unsigned        reserved5_012   :3;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        reserved5_012   :3;
        unsigned        efmt            :1;     
        unsigned        reserved5_4     :1;
        unsigned        qfa             :1;     
        unsigned        reserved5_67    :2;
#else
#error "Please fix <asm/byteorder.h>"
#endif
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        cmprs           :1;     
        unsigned        ecc             :1;     
	unsigned        reserved6_45    :2;       
        unsigned        eject           :1;     
        unsigned        prevent         :1;     
        unsigned        locked          :1;     
	unsigned        lock            :1;     
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        lock            :1;     
        unsigned        locked          :1;     
        unsigned        prevent         :1;     
        unsigned        eject           :1;     
	unsigned        reserved6_45    :2;       
        unsigned        ecc             :1;     
        unsigned        cmprs           :1;     
#else
#error "Please fix <asm/byteorder.h>"
#endif
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        blk32768        :1;     
                                                
                                                
        unsigned        reserved7_3_6   :4;
        unsigned        blk1024         :1;     
        unsigned        blk512          :1;     
	unsigned        reserved7_0     :1;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        reserved7_0     :1;
        unsigned        blk512          :1;     
        unsigned        blk1024         :1;     
        unsigned        reserved7_3_6   :4;
        unsigned        blk32768        :1;     
                                                
                                                
#else
#error "Please fix <asm/byteorder.h>"
#endif
        __be16          max_speed;              
        u8              reserved10, reserved11;
        __be16          ctl;                    
        __be16          speed;                  
        __be16          buffer_size;            
        u8              reserved18, reserved19;
} osst_capabilities_page_t;

typedef struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        ps              :1;
        unsigned        reserved1_6     :1;
	unsigned        page_code       :6;     
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        page_code       :6;     
        unsigned        reserved1_6     :1;
        unsigned        ps              :1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
        u8              page_length;            
        u8              reserved2;
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        one             :1;
        unsigned        reserved2_6     :1;
        unsigned        record32_5      :1;
        unsigned        record32        :1;
        unsigned        reserved2_23    :2;
        unsigned        play32_5        :1;
	unsigned        play32          :1;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        play32          :1;
        unsigned        play32_5        :1;
        unsigned        reserved2_23    :2;
        unsigned        record32        :1;
        unsigned        record32_5      :1;
        unsigned        reserved2_6     :1;
        unsigned        one             :1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
} osst_block_size_page_t;

typedef struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
        unsigned        ps              :1;
        unsigned        reserved1_6     :1;
	unsigned        page_code       :6;     
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned        page_code       :6;     
        unsigned        reserved1_6     :1;
        unsigned        ps              :1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	u8		reserved2;
	u8		density;
	u8		reserved3,reserved4;
	__be16		segtrk;
	__be16		trks;
	u8		reserved5,reserved6,reserved7,reserved8,reserved9,reserved10;
} osst_tape_paramtr_page_t;


#define OS_CONFIG_PARTITION     (0xff)
#define OS_DATA_PARTITION       (0)
#define OS_PARTITION_VERSION    (1)

typedef struct os_partition_s {
        __u8    partition_num;
        __u8    par_desc_ver;
        __be16  wrt_pass_cntr;
        __be32  first_frame_ppos;
        __be32  last_frame_ppos;
        __be32  eod_frame_ppos;
} os_partition_t;

typedef struct os_dat_entry_s {
        __be32  blk_sz;
        __be16  blk_cnt;
        __u8    flags;
        __u8    reserved;
} os_dat_entry_t;

#define OS_DAT_FLAGS_DATA       (0xc)
#define OS_DAT_FLAGS_MARK       (0x1)

typedef struct os_dat_s {
        __u8            dat_sz;
        __u8            reserved1;
        __u8            entry_cnt;
        __u8            reserved3;
        os_dat_entry_t  dat_list[16];
} os_dat_t;

#define OS_FRAME_TYPE_FILL      (0)
#define OS_FRAME_TYPE_EOD       (1 << 0)
#define OS_FRAME_TYPE_MARKER    (1 << 1)
#define OS_FRAME_TYPE_HEADER    (1 << 3)
#define OS_FRAME_TYPE_DATA      (1 << 7)

typedef struct os_aux_s {
        __be32          format_id;              
        char            application_sig[4];     
        __be32          hdwr;                   
        __be32          update_frame_cntr;      
        __u8            frame_type;
        __u8            frame_type_reserved;
        __u8            reserved_18_19[2];
        os_partition_t  partition;
        __u8            reserved_36_43[8];
        __be32          frame_seq_num;
        __be32          logical_blk_num_high;
        __be32          logical_blk_num;
        os_dat_t        dat;
        __u8            reserved188_191[4];
        __be32          filemark_cnt;
        __be32          phys_fm;
        __be32          last_mark_ppos;
        __u8            reserved204_223[20];

         __be32         next_mark_ppos;         
	 __be32		last_mark_lbn;		
         __u8           linux_specific[24];

        __u8            reserved_256_511[256];
} os_aux_t;

#define OS_FM_TAB_MAX 1024

typedef struct os_fm_tab_s {
	__u8		fm_part_num;
	__u8		reserved_1;
	__u8		fm_tab_ent_sz;
	__u8		reserved_3;
	__be16		fm_tab_ent_cnt;
	__u8		reserved6_15[10];
	__be32		fm_tab_ent[OS_FM_TAB_MAX];
} os_fm_tab_t;

typedef struct os_ext_trk_ey_s {
	__u8		et_part_num;
	__u8		fmt;
	__be16		fm_tab_off;
	__u8		reserved4_7[4];
	__be32		last_hlb_hi;
	__be32		last_hlb;
	__be32		last_pp;
	__u8		reserved20_31[12];
} os_ext_trk_ey_t;

typedef struct os_ext_trk_tb_s {
	__u8		nr_stream_part;
	__u8		reserved_1;
	__u8		et_ent_sz;
	__u8		reserved3_15[13];
	os_ext_trk_ey_t	dat_ext_trk_ey;
	os_ext_trk_ey_t	qfa_ext_trk_ey;
} os_ext_trk_tb_t;

typedef struct os_header_s {
        char            ident_str[8];
        __u8            major_rev;
        __u8            minor_rev;
	__be16		ext_trk_tb_off;
        __u8            reserved12_15[4];
        __u8            pt_par_num;
        __u8            pt_reserved1_3[3];
        os_partition_t  partition[16];
	__be32		cfg_col_width;
	__be32		dat_col_width;
	__be32		qfa_col_width;
	__u8		cartridge[16];
	__u8		reserved304_511[208];
	__be32		old_filemark_list[16680/4];		
	os_ext_trk_tb_t	ext_track_tb;
	__u8		reserved17272_17735[464];
	os_fm_tab_t	dat_fm_tab;
	os_fm_tab_t	qfa_fm_tab;
	__u8		reserved25960_32767[6808];
} os_header_t;


#define OS_FRAME_SIZE   (32 * 1024 + 512)
#define OS_DATA_SIZE    (32 * 1024)
#define OS_AUX_SIZE     (512)

struct osst_buffer {
  unsigned char in_use;
  unsigned char dma;	
  int buffer_size;
  int buffer_blocks;
  int buffer_bytes;
  int read_pointer;
  int writing;
  int midlevel_result;
  int syscall_result;
  struct osst_request *last_SRpnt;
  struct st_cmdstatus cmdstat;
  struct rq_map_data map_data;
  unsigned char *b_data;
  os_aux_t *aux;               
  unsigned short use_sg;       
  unsigned short sg_segs;      
  unsigned short orig_sg_segs; 
  struct scatterlist sg[1];    
} ;

struct osst_tape {
  struct scsi_driver *driver;
  unsigned capacity;
  struct scsi_device *device;
  struct mutex lock;           
  struct completion wait;      
  struct osst_buffer * buffer;

  
  unsigned char omit_blklims;
  unsigned char do_auto_lock;
  unsigned char can_bsr;
  unsigned char can_partitions;
  unsigned char two_fm;
  unsigned char fast_mteom;
  unsigned char restr_dma;
  unsigned char scsi2_logical;
  unsigned char default_drvbuffer;  
  unsigned char pos_unknown;        
  int write_threshold;
  int timeout;			
  int long_timeout;		

  
  struct st_modedef modes[ST_NBR_MODES];
  int current_mode;

  
  int partition;
  int new_partition;
  int nbr_partitions;    
  struct st_partstat ps[ST_NBR_PARTITIONS];
  unsigned char dirty;
  unsigned char ready;
  unsigned char write_prot;
  unsigned char drv_write_prot;
  unsigned char in_use;
  unsigned char blksize_changed;
  unsigned char density_changed;
  unsigned char compression_changed;
  unsigned char drv_buffer;
  unsigned char density;
  unsigned char door_locked;
  unsigned char rew_at_close;
  unsigned char inited;
  int block_size;
  int min_block;
  int max_block;
  int recover_count;            
  int abort_count;
  int write_count;
  int read_count;
  int recover_erreg;            
  int	   os_fw_rev;			       
  unsigned char  raw;                          
  unsigned char  poll;                         
  unsigned char  frame_in_buffer;	       
  int      frame_seq_number;                   
  int      logical_blk_num;                    
  unsigned first_frame_position;               
  unsigned last_frame_position;                
  int      cur_frames;                         
  int      max_frames;                         
  char     application_sig[5];                 
  unsigned char  fast_open;                    
  unsigned short wrt_pass_cntr;                
  int      update_frame_cntr;                  
  int      onstream_write_error;               
  int      header_ok;                          
  int      linux_media;                        
  int      linux_media_version;
  os_header_t * header_cache;		       
  int      filemark_cnt;
  int      first_mark_ppos;
  int      last_mark_ppos;
  int      last_mark_lbn;			
  int      first_data_ppos;
  int      eod_frame_ppos;
  int      eod_frame_lfa;
  int      write_type;				
  int      read_error_frame;			
  unsigned long cmd_start_time;
  unsigned long max_cmd_time;

#if DEBUG
  unsigned char write_pending;
  int nbr_finished;
  int nbr_waits;
  unsigned char last_cmnd[6];
  unsigned char last_sense[16];
#endif
  struct gendisk *drive;
} ;

struct osst_request {
	unsigned char cmd[MAX_COMMAND_SIZE];
	unsigned char sense[SCSI_SENSE_BUFFERSIZE];
	int result;
	struct osst_tape *stp;
	struct completion *waiting;
	struct bio *bio;
};

#define OS_WRITE_DATA      0
#define OS_WRITE_EOD       1
#define OS_WRITE_NEW_MARK  2
#define OS_WRITE_LAST_MARK 3
#define OS_WRITE_HEADER    4
#define OS_WRITE_FILLER    5

#define OS_WRITING_COMPLETE 3
