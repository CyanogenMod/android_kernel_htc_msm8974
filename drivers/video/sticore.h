#ifndef STICORE_H
#define STICORE_H


#if 0
#define DPRINTK(x)	printk x
#else
#define DPRINTK(x) 
#endif

#define MAX_STI_ROMS 4		

#define STI_REGION_MAX 8	
#define STI_DEV_NAME_LENGTH 32
#define STI_MONITOR_MAX 256

#define STI_FONT_HPROMAN8 1
#define STI_FONT_KANA8 2

 
#include <asm/io.h>

#define STI_WAIT 1

#define STI_PTR(p)	( virt_to_phys(p) )
#define PTR_STI(p)	( phys_to_virt((unsigned long)p) )
#define STI_CALL(func, flags, inptr, outptr, glob_cfg)	\
       ({						\
               pdc_sti_call( func, STI_PTR(flags),	\
				   STI_PTR(inptr),	\
				   STI_PTR(outptr),	\
				   STI_PTR(glob_cfg));	\
       })


#define sti_onscreen_x(sti) (sti->glob_cfg->onscreen_x)
#define sti_onscreen_y(sti) (sti->glob_cfg->onscreen_y)

#define sti_font_x(sti) (PTR_STI(sti->font)->width)
#define sti_font_y(sti) (PTR_STI(sti->font)->height)



typedef union region {
	struct { 
		u32 offset	: 14;	
		u32 sys_only	: 1;	
		u32 cache	: 1;	
		u32 btlb	: 1;	
		u32 last	: 1;	
		u32 length	: 14;	
	} region_desc;

	u32 region;			
} region_t;

#define REGION_OFFSET_TO_PHYS( rt, hpa ) \
	(((rt).region_desc.offset << 12) + (hpa))

struct sti_glob_cfg_ext {
	 u8 curr_mon;			
	 u8 friendly_boot;		
	s16 power;			
	s32 freq_ref;			
	u32 sti_mem_addr;		
	u32 future_ptr; 		
};

struct sti_glob_cfg {
	s32 text_planes;		
	s16 onscreen_x;			
	s16 onscreen_y;			
	s16 offscreen_x;		
	s16 offscreen_y;		
	s16 total_x;			
	s16 total_y;			
	u32 region_ptrs[STI_REGION_MAX]; 
	s32 reent_lvl;			
	u32 save_addr;			
	u32 ext_ptr;			
};



struct sti_init_flags {
	u32 wait : 1;		
	u32 reset : 1;		
	u32 text : 1;		
	u32 nontext : 1;	
	u32 clear : 1;		
	u32 cmap_blk : 1;	
	u32 enable_be_timer : 1; 
	u32 enable_be_int : 1;	
	u32 no_chg_tx : 1;	
	u32 no_chg_ntx : 1;	
	u32 no_chg_bet : 1;	
	u32 no_chg_bei : 1;	
	u32 init_cmap_tx : 1;	
	u32 cmt_chg : 1;	
	u32 retain_ie : 1;	
	u32 caller_bootrom : 1;	
	u32 caller_kernel : 1;	
	u32 caller_other : 1;	
	u32 pad	: 14;		
	u32 future_ptr; 	
};

struct sti_init_inptr_ext {
	u8  config_mon_type;	
	u8  pad[1];		
	u16 inflight_data;	
	u32 future_ptr; 	
};

struct sti_init_inptr {
	s32 text_planes;	
	u32 ext_ptr;		
};


struct sti_init_outptr {
	s32 errno;		
	s32 text_planes;	
	u32 future_ptr; 	
};




struct sti_conf_flags {
	u32 wait : 1;		
	u32 pad : 31;		
	u32 future_ptr; 	
};

struct sti_conf_inptr {
	u32 future_ptr; 	
};

struct sti_conf_outptr_ext {
	u32 crt_config[3];		
	u32 crt_hdw[3];
	u32 future_ptr;
};

struct sti_conf_outptr {
	s32 errno;		
	s16 onscreen_x;		
	s16 onscreen_y;		
	s16 offscreen_x;	
	s16 offscreen_y;	
	s16 total_x;		
	s16 total_y;		
	s32 bits_per_pixel;	
	s32 bits_used;		
	s32 planes;		
	 u8 dev_name[STI_DEV_NAME_LENGTH]; 
	u32 attributes;		
	u32 ext_ptr;		
};

struct sti_rom {
	 u8 type[4];
	 u8 res004;
	 u8 num_mons;
	 u8 revno[2];
	u32 graphics_id[2];

	u32 font_start;
	u32 statesize;
	u32 last_addr;
	u32 region_list;

	u16 reentsize;
	u16 maxtime;
	u32 mon_tbl_addr;
	u32 user_data_addr;
	u32 sti_mem_req;

	u32 user_data_size;
	u16 power;
	 u8 bus_support;
	 u8 ext_bus_support;
	 u8 alt_code_type;
	 u8 ext_dd_struct[3];
	u32 cfb_addr;

	u32 init_graph;
	u32 state_mgmt;
	u32 font_unpmv;
	u32 block_move;
	u32 self_test;
	u32 excep_hdlr;
	u32 inq_conf;
	u32 set_cm_entry;
	u32 dma_ctrl;
	 u8 res040[7 * 4];
	
	u32 init_graph_addr;
	u32 state_mgmt_addr;
	u32 font_unp_addr;
	u32 block_move_addr;
	u32 self_test_addr;
	u32 excep_hdlr_addr;
	u32 inq_conf_addr;
	u32 set_cm_entry_addr;
	u32 image_unpack_addr;
	u32 pa_risx_addrs[7];
};

struct sti_rom_font {
	u16 first_char;
	u16 last_char;
	 u8 width;
	 u8 height;
	 u8 font_type;		
	 u8 bytes_per_char;
	u32 next_font;
	 u8 underline_height;
	 u8 underline_pos;
	 u8 res008[2];
};


struct sti_cooked_font {
        struct sti_rom_font *raw;
	struct sti_cooked_font *next_font;
};

struct sti_cooked_rom {
        struct sti_rom *raw;
	struct sti_cooked_font *font_start;
};


struct sti_font_inptr {
	u32 font_start_addr;	
	s16 index;		
	u8 fg_color;		
	u8 bg_color;		
	s16 dest_x;		
	s16 dest_y;		
	u32 future_ptr; 	
};

struct sti_font_flags {
	u32 wait : 1;		
	u32 non_text : 1;	
	u32 pad : 30;		
	u32 future_ptr; 	
};
	
struct sti_font_outptr {
	s32 errno;		
	u32 future_ptr; 	
};


struct sti_blkmv_flags {
	u32 wait : 1;		
	u32 color : 1;		
	u32 clear : 1;		
	u32 non_text : 1;	
	u32 pad : 28;		
	u32 future_ptr; 	
};

struct sti_blkmv_inptr {
	u8 fg_color;		
	u8 bg_color;		
	s16 src_x;		
	s16 src_y;		
	s16 dest_x;		
	s16 dest_y;		
	s16 width;		
	s16 height;		
	u32 future_ptr; 	
};

struct sti_blkmv_outptr {
	s32 errno;		
	u32 future_ptr; 	
};



struct sti_struct {
	spinlock_t lock;
		
	
	int font_width;	
	int font_height;
	
	int sti_mem_request;
	u32 graphics_id[2];

	struct sti_cooked_rom *rom;

	unsigned long font_unpmv;
	unsigned long block_move;
	unsigned long init_graph;
	unsigned long inq_conf;

	
	int text_planes;
	region_t regions[STI_REGION_MAX];
	unsigned long regions_phys[STI_REGION_MAX];

	struct sti_glob_cfg *glob_cfg;
	struct sti_cooked_font *font;	

	struct sti_conf_outptr outptr; 
	struct sti_conf_outptr_ext outptr_ext;

	struct pci_dev *pd;

	
	u8 rm_entry[16]; 

	
	struct fb_info *info;
};



struct sti_struct *sti_get_rom(unsigned int index); 


void sti_putc(struct sti_struct *sti, int c, int y, int x);
void sti_set(struct sti_struct *sti, int src_y, int src_x,
	     int height, int width, u8 color);
void sti_clear(struct sti_struct *sti, int src_y, int src_x,
	       int height, int width, int c);
void sti_bmove(struct sti_struct *sti, int src_y, int src_x,
	       int dst_y, int dst_x, int height, int width);

#endif	
