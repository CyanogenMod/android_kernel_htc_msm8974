#ifndef __IA64_SETUP_H
#define __IA64_SETUP_H

#define COMMAND_LINE_SIZE	2048

extern struct ia64_boot_param {
	__u64 command_line;		
	__u64 efi_systab;		
	__u64 efi_memmap;		
	__u64 efi_memmap_size;		
	__u64 efi_memdesc_size;		
	__u32 efi_memdesc_version;	
	struct {
		__u16 num_cols;	
		__u16 num_rows;	
		__u16 orig_x;	
		__u16 orig_y;	
	} console_info;
	__u64 fpswa;		
	__u64 initrd_start;
	__u64 initrd_size;
} *ia64_boot_param;

#endif
