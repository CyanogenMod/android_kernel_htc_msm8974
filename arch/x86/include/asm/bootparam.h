#ifndef _ASM_X86_BOOTPARAM_H
#define _ASM_X86_BOOTPARAM_H

#include <linux/types.h>
#include <linux/screen_info.h>
#include <linux/apm_bios.h>
#include <linux/edd.h>
#include <asm/e820.h>
#include <asm/ist.h>
#include <video/edid.h>

#define SETUP_NONE			0
#define SETUP_E820_EXT			1
#define SETUP_DTB			2

struct setup_data {
	__u64 next;
	__u32 type;
	__u32 len;
	__u8 data[0];
};

struct setup_header {
	__u8	setup_sects;
	__u16	root_flags;
	__u32	syssize;
	__u16	ram_size;
#define RAMDISK_IMAGE_START_MASK	0x07FF
#define RAMDISK_PROMPT_FLAG		0x8000
#define RAMDISK_LOAD_FLAG		0x4000
	__u16	vid_mode;
	__u16	root_dev;
	__u16	boot_flag;
	__u16	jump;
	__u32	header;
	__u16	version;
	__u32	realmode_swtch;
	__u16	start_sys;
	__u16	kernel_version;
	__u8	type_of_loader;
	__u8	loadflags;
#define LOADED_HIGH	(1<<0)
#define QUIET_FLAG	(1<<5)
#define KEEP_SEGMENTS	(1<<6)
#define CAN_USE_HEAP	(1<<7)
	__u16	setup_move_size;
	__u32	code32_start;
	__u32	ramdisk_image;
	__u32	ramdisk_size;
	__u32	bootsect_kludge;
	__u16	heap_end_ptr;
	__u8	ext_loader_ver;
	__u8	ext_loader_type;
	__u32	cmd_line_ptr;
	__u32	initrd_addr_max;
	__u32	kernel_alignment;
	__u8	relocatable_kernel;
	__u8	_pad2[3];
	__u32	cmdline_size;
	__u32	hardware_subarch;
	__u64	hardware_subarch_data;
	__u32	payload_offset;
	__u32	payload_length;
	__u64	setup_data;
	__u64	pref_address;
	__u32	init_size;
} __attribute__((packed));

struct sys_desc_table {
	__u16 length;
	__u8  table[14];
};

struct olpc_ofw_header {
	__u32 ofw_magic;	
	__u32 ofw_version;
	__u32 cif_handler;	
	__u32 irq_desc_table;
} __attribute__((packed));

struct efi_info {
	__u32 efi_loader_signature;
	__u32 efi_systab;
	__u32 efi_memdesc_size;
	__u32 efi_memdesc_version;
	__u32 efi_memmap;
	__u32 efi_memmap_size;
	__u32 efi_systab_hi;
	__u32 efi_memmap_hi;
};

struct boot_params {
	struct screen_info screen_info;			
	struct apm_bios_info apm_bios_info;		
	__u8  _pad2[4];					
	__u64  tboot_addr;				
	struct ist_info ist_info;			
	__u8  _pad3[16];				
	__u8  hd0_info[16];			
	__u8  hd1_info[16];			
	struct sys_desc_table sys_desc_table;		
	struct olpc_ofw_header olpc_ofw_header;		
	__u8  _pad4[128];				
	struct edid_info edid_info;			
	struct efi_info efi_info;			
	__u32 alt_mem_k;				
	__u32 scratch;			
	__u8  e820_entries;				
	__u8  eddbuf_entries;				
	__u8  edd_mbr_sig_buf_entries;			
	__u8  _pad6[6];					
	struct setup_header hdr;    	
	__u8  _pad7[0x290-0x1f1-sizeof(struct setup_header)];
	__u32 edd_mbr_sig_buffer[EDD_MBR_SIG_MAX];	
	struct e820entry e820_map[E820MAX];		
	__u8  _pad8[48];				
	struct edd_info eddbuf[EDDMAXNR];		
	__u8  _pad9[276];				
} __attribute__((packed));

enum {
	X86_SUBARCH_PC = 0,
	X86_SUBARCH_LGUEST,
	X86_SUBARCH_XEN,
	X86_SUBARCH_MRST,
	X86_SUBARCH_CE4100,
	X86_NR_SUBARCHS,
};



#endif 
