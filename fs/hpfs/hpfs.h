

#if !defined(__LITTLE_ENDIAN) && !defined(__BIG_ENDIAN)
#error unknown endian
#endif


typedef u32 secno;			

typedef secno dnode_secno;		
typedef secno fnode_secno;		
typedef secno anode_secno;		

typedef u32 time32_t;		



#define BB_MAGIC 0xaa55

struct hpfs_boot_block
{
  u8 jmp[3];
  u8 oem_id[8];
  u8 bytes_per_sector[2];	
  u8 sectors_per_cluster;
  u8 n_reserved_sectors[2];
  u8 n_fats;
  u8 n_rootdir_entries[2];
  u8 n_sectors_s[2];
  u8 media_byte;
  u16 sectors_per_fat;
  u16 sectors_per_track;
  u16 heads_per_cyl;
  u32 n_hidden_sectors;
  u32 n_sectors_l;		
  u8 drive_number;
  u8 mbz;
  u8 sig_28h;			
  u8 vol_serno[4];
  u8 vol_label[11];
  u8 sig_hpfs[8];		
  u8 pad[448];
  u16 magic;			
};




#define SB_MAGIC 0xf995e849

struct hpfs_super_block
{
  u32 magic;				
  u32 magic1;				
  u8 version;				
  u8 funcversion;			
  u16 zero;				
  fnode_secno root;			
  secno n_sectors;			
  u32 n_badblocks;			
  secno bitmaps;			
  u32 zero1;				
  secno badblocks;			
  u32 zero3;				
  time32_t last_chkdsk;			
  time32_t last_optimize;		
  secno n_dir_band;			
  secno dir_band_start;			
  secno dir_band_end;			
  secno dir_band_bitmap;		
  u8 volume_name[32];			
  secno user_id_table;			
  u32 zero6[103];			
};




#define SP_MAGIC 0xf9911849

struct hpfs_spare_block
{
  u32 magic;				
  u32 magic1;				

#ifdef __LITTLE_ENDIAN
  u8 dirty: 1;				
  u8 sparedir_used: 1;			
  u8 hotfixes_used: 1;			
  u8 bad_sector: 1;			
  u8 bad_bitmap: 1;			
  u8 fast: 1;				
  u8 old_wrote: 1;			
  u8 old_wrote_1: 1;			
#else
  u8 old_wrote_1: 1;			
  u8 old_wrote: 1;			
  u8 fast: 1;				
  u8 bad_bitmap: 1;			
  u8 bad_sector: 1;			
  u8 hotfixes_used: 1;			
  u8 sparedir_used: 1;			
  u8 dirty: 1;				
#endif

#ifdef __LITTLE_ENDIAN
  u8 install_dasd_limits: 1;		
  u8 resynch_dasd_limits: 1;
  u8 dasd_limits_operational: 1;
  u8 multimedia_active: 1;
  u8 dce_acls_active: 1;
  u8 dasd_limits_dirty: 1;
  u8 flag67: 2;
#else
  u8 flag67: 2;
  u8 dasd_limits_dirty: 1;
  u8 dce_acls_active: 1;
  u8 multimedia_active: 1;
  u8 dasd_limits_operational: 1;
  u8 resynch_dasd_limits: 1;
  u8 install_dasd_limits: 1;		
#endif

  u8 mm_contlgulty;
  u8 unused;

  secno hotfix_map;			
  u32 n_spares_used;			
  u32 n_spares;				
  u32 n_dnode_spares_free;		
  u32 n_dnode_spares;			
  secno code_page_dir;			
  u32 n_code_pages;			
  u32 super_crc;			
  u32 spare_crc;			
  u32 zero1[15];			
  dnode_secno spare_dnodes[100];	
  u32 zero2[1];				
};


#define BAD_MAGIC 0
       






#define CP_DIR_MAGIC 0x494521f7

struct code_page_directory
{
  u32 magic;				
  u32 n_code_pages;			
  u32 zero1[2];
  struct {
    u16 ix;				
    u16 code_page_number;		
    u32 bounds;				
    secno code_page_data;		
    u16 index;				
    u16 unknown;			
  } array[31];				
};


#define CP_DATA_MAGIC 0x894521f7

struct code_page_data
{
  u32 magic;				
  u32 n_used;				
  u32 bounds[3];			
  u16 offs[3];				
  struct {
    u16 ix;				
    u16 code_page_number;		
    u16 unknown;			
    u8 map[128];			
    u16 zero2;
  } code_page[3];
  u8 incognita[78];
};






#define DNODE_MAGIC   0x77e40aae

struct dnode {
  u32 magic;				
  u32 first_free;			
#ifdef __LITTLE_ENDIAN
  u8 root_dnode: 1;			
  u8 increment_me: 7;			
#else
  u8 increment_me: 7;			
  u8 root_dnode: 1;			
#endif
  u8 increment_me2[3];
  secno up;				
  dnode_secno self;			
  u8 dirent[2028];			
};

struct hpfs_dirent {
  u16 length;				

#ifdef __LITTLE_ENDIAN
  u8 first: 1;				
  u8 has_acl: 1;
  u8 down: 1;				
  u8 last: 1;				
  u8 has_ea: 1;				
  u8 has_xtd_perm: 1;			
  u8 has_explicit_acl: 1;
  u8 has_needea: 1;			
#else
  u8 has_needea: 1;			
  u8 has_explicit_acl: 1;
  u8 has_xtd_perm: 1;			
  u8 has_ea: 1;				
  u8 last: 1;				
  u8 down: 1;				
  u8 has_acl: 1;
  u8 first: 1;				
#endif

#ifdef __LITTLE_ENDIAN
  u8 read_only: 1;			
  u8 hidden: 1;				
  u8 system: 1;				
  u8 flag11: 1;				
  u8 directory: 1;			
  u8 archive: 1;			
  u8 not_8x3: 1;			
  u8 flag15: 1;
#else
  u8 flag15: 1;
  u8 not_8x3: 1;			
  u8 archive: 1;			
  u8 directory: 1;			
  u8 flag11: 1;				
  u8 system: 1;				
  u8 hidden: 1;				
  u8 read_only: 1;			
#endif

  fnode_secno fnode;			
  time32_t write_date;			
  u32 file_size;			
  time32_t read_date;			
  time32_t creation_date;			
  u32 ea_size;				
  u8 no_of_acls;			
  u8 ix;				
  u8 namelen, name[1];			
};




struct bplus_leaf_node
{
  u32 file_secno;			
  u32 length;				
  secno disk_secno;			
};

struct bplus_internal_node
{
  u32 file_secno;			
  anode_secno down;			
};

struct bplus_header
{
#ifdef __LITTLE_ENDIAN
  u8 hbff: 1;			
  u8 flag1234: 4;
  u8 fnode_parent: 1;			
  u8 binary_search: 1;			
  u8 internal: 1;			
#else
  u8 internal: 1;			
  u8 binary_search: 1;			
  u8 fnode_parent: 1;			
  u8 flag1234: 4;
  u8 hbff: 1;			
#endif
  u8 fill[3];
  u8 n_free_nodes;			
  u8 n_used_nodes;			
  u16 first_free;			
  union {
    struct bplus_internal_node internal[0]; 
    struct bplus_leaf_node external[0];	    
  } u;
};



#define FNODE_MAGIC 0xf7e40aae

struct fnode
{
  u32 magic;				
  u32 zero1[2];				
  u8 len, name[15];			
  fnode_secno up;			
  secno acl_size_l;
  secno acl_secno;
  u16 acl_size_s;
  u8 acl_anode;
  u8 zero2;				
  u32 ea_size_l;			
  secno ea_secno;			
  u16 ea_size_s;			

#ifdef __LITTLE_ENDIAN
  u8 flag0: 1;
  u8 ea_anode: 1;			
  u8 flag234567: 6;
#else
  u8 flag234567: 6;
  u8 ea_anode: 1;			
  u8 flag0: 1;
#endif

#ifdef __LITTLE_ENDIAN
  u8 dirflag: 1;			
  u8 flag9012345: 7;
#else
  u8 flag9012345: 7;
  u8 dirflag: 1;			
#endif

  struct bplus_header btree;		
  union {
    struct bplus_leaf_node external[8];
    struct bplus_internal_node internal[12];
  } u;

  u32 file_size;			
  u32 n_needea;				
  u8 user_id[16];			
  u16 ea_offs;				
  u8 dasd_limit_treshhold;
  u8 dasd_limit_delta;
  u32 dasd_limit;
  u32 dasd_usage;
  u8 ea[316];				
};



#define ANODE_MAGIC 0x37e40aae

struct anode
{
  u32 magic;				
  anode_secno self;			
  secno up;				

  struct bplus_header btree;		
  union {
    struct bplus_leaf_node external[40];
    struct bplus_internal_node internal[60];
  } u;

  u32 fill[3];				
};



struct extended_attribute
{
#ifdef __LITTLE_ENDIAN
  u8 indirect: 1;			
  u8 anode: 1;				
  u8 flag23456: 5;
  u8 needea: 1;				
#else
  u8 needea: 1;				
  u8 flag23456: 5;
  u8 anode: 1;				
  u8 indirect: 1;			
#endif
  u8 namelen;				
  u8 valuelen_lo;			
  u8 valuelen_hi;			
  u8 name[0];
};

