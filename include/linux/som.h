#ifndef _LINUX_SOM_H
#define _LINUX_SOM_H


#include <linux/time.h>

#define SOM_PAGESIZE 4096

struct som_hdr {
	short		system_id;		
	short		a_magic;		
	unsigned int	version_id;		
	struct timespec	file_time;		
	unsigned int	entry_space;		
	unsigned int	entry_subspace;		
	unsigned int	entry_offset;		
	unsigned int	aux_header_location;	
	unsigned int	aux_header_size;	
	unsigned int	som_length;		
	unsigned int	presumed_dp;		
	unsigned int	space_location;		
	unsigned int	space_total;		
	unsigned int	subspace_location;	
	unsigned int	subspace_total;		
	unsigned int	loader_fixup_location;	
	unsigned int	loader_fixup_total;	
	unsigned int	space_strings_location;	
	unsigned int	space_strings_size;	
	unsigned int	init_array_location;	
	unsigned int	init_array_total;	
	unsigned int	compiler_location;	
	unsigned int	compiler_total;		
	unsigned int	symbol_location;	
	unsigned int	symbol_total;		
	unsigned int	fixup_request_location;	
	unsigned int	fixup_request_total;	
	unsigned int	symbol_strings_location;
	unsigned int	symbol_strings_size;	
	unsigned int	unloadable_sp_location;	
	unsigned int	unloadable_sp_size;	
	unsigned int	checksum;
};


#define SOM_SID_PARISC_1_0	0x020b
#define SOM_SID_PARISC_1_1	0x0210
#define SOM_SID_PARISC_2_0	0x0214


#define SOM_LIB_EXEC		0x0104
#define SOM_RELOCATABLE		0x0106
#define SOM_EXEC_NONSHARE	0x0107
#define SOM_EXEC_SHARE		0x0108
#define SOM_EXEC_DEMAND		0x010B
#define SOM_LIB_DYN		0x010D
#define SOM_LIB_SHARE		0x010E
#define SOM_LIB_RELOC		0x0619


#define SOM_ID_OLD		85082112
#define SOM_ID_NEW		87102412

struct aux_id {
	unsigned int	mandatory :1;	
	unsigned int	copy	  :1;	
	unsigned int	append	  :1;	
	unsigned int	ignore	  :1;	
	unsigned int	reserved  :12;
	unsigned int	type	  :16;	
	unsigned int	length;		
};

struct som_exec_auxhdr {
	struct aux_id	som_auxhdr;
	int		exec_tsize;	
	int		exec_tmem;	
	int		exec_tfile;	
	int		exec_dsize;	
	int		exec_dmem;	
	int		exec_dfile;	
	int		exec_bsize;	
	int		exec_entry;	
	int		exec_flags;	
	int		exec_bfill;	
};

union name_pt {
	char *		n_name;
	unsigned int	n_strx;
};

struct space_dictionary_record {
	union name_pt	name;			
	unsigned int	is_loadable	:1;	
	unsigned int	is_defined	:1;	
	unsigned int	is_private	:1;	
	unsigned int	has_intermediate_code :1; 
	unsigned int	is_tspecific	:1;	
	unsigned int	reserved	:11;	
	unsigned int	sort_key	:8;	
	unsigned int	reserved2	:8;	

	int		space_number;		
	int		subspace_index;		
	unsigned int	subspace_quantity;	
	int		loader_fix_index;	
	unsigned int	loader_fix_quantity;	
	int		init_pointer_index;	
	unsigned int	init_pointer_quantity;	
};

struct subspace_dictionary_record {
	int		space_index;
	unsigned int	access_control_bits :7;
	unsigned int	memory_resident	:1;
	unsigned int	dup_common	:1;
	unsigned int	is_common	:1;
	unsigned int	quadrant	:2;
	unsigned int	initially_frozen :1;
	unsigned int	is_first	:1;
	unsigned int	code_only	:1;
	unsigned int	sort_key	:8;
	unsigned int	replicate_init	:1;
	unsigned int	continuation	:1;
	unsigned int	is_tspecific	:1;
	unsigned int	is_comdat	:1;
	unsigned int	reserved	:4;

	int		file_loc_init_value;
	unsigned int	initialization_length;
	unsigned int	subspace_start;
	unsigned int	subspace_length;

	unsigned int	reserved2	:5;
	unsigned int	alignment	:27;

	union name_pt	name;
	int		fixup_request_index;
	unsigned int	fixup_request_quantity;
};

#endif 
