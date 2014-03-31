/*
 * Copyright (C) 2000, 2002-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * Kernel unwind support.
 */

#define UNW_VER(x)		((x) >> 48)
#define UNW_FLAG_MASK		0x0000ffff00000000
#define UNW_FLAG_OSMASK		0x0000f00000000000
#define UNW_FLAG_EHANDLER(x)	((x) & 0x0000000100000000L)
#define UNW_FLAG_UHANDLER(x)	((x) & 0x0000000200000000L)
#define UNW_LENGTH(x)		((x) & 0x00000000ffffffffL)

enum unw_register_index {
	
	UNW_REG_PRI_UNAT_GR,
	UNW_REG_PRI_UNAT_MEM,

	
	UNW_REG_BSP,					
	UNW_REG_BSPSTORE,
	UNW_REG_PFS,					
	UNW_REG_RNAT,
	
	UNW_REG_PSP,					
	
	UNW_REG_RP,

	
	UNW_REG_R4, UNW_REG_R5, UNW_REG_R6, UNW_REG_R7,
	UNW_REG_UNAT, UNW_REG_PR, UNW_REG_LC, UNW_REG_FPSR,
	UNW_REG_B1, UNW_REG_B2, UNW_REG_B3, UNW_REG_B4, UNW_REG_B5,
	UNW_REG_F2, UNW_REG_F3, UNW_REG_F4, UNW_REG_F5,
	UNW_REG_F16, UNW_REG_F17, UNW_REG_F18, UNW_REG_F19,
	UNW_REG_F20, UNW_REG_F21, UNW_REG_F22, UNW_REG_F23,
	UNW_REG_F24, UNW_REG_F25, UNW_REG_F26, UNW_REG_F27,
	UNW_REG_F28, UNW_REG_F29, UNW_REG_F30, UNW_REG_F31,
	UNW_NUM_REGS
};

struct unw_info_block {
	u64 header;
	u64 desc[0];		
	
};

struct unw_table {
	struct unw_table *next;		
	const char *name;
	unsigned long gp;		
	unsigned long segment_base;	
	unsigned long start;
	unsigned long end;
	const struct unw_table_entry *array;
	unsigned long length;
};

enum unw_where {
	UNW_WHERE_NONE,			
	UNW_WHERE_GR,			
	UNW_WHERE_FR,			
	UNW_WHERE_BR,			
	UNW_WHERE_SPREL,		
	UNW_WHERE_PSPREL,		
	UNW_WHERE_SPILL_HOME,		
	UNW_WHERE_GR_SAVE		
};

#define UNW_WHEN_NEVER	0x7fffffff

struct unw_reg_info {
	unsigned long val;		
	enum unw_where where;		
	int when;			
};

struct unw_reg_state {
	struct unw_reg_state *next;		
	struct unw_reg_info reg[UNW_NUM_REGS];	
};

struct unw_labeled_state {
	struct unw_labeled_state *next;		
	unsigned long label;			
	struct unw_reg_state saved_state;
};

struct unw_state_record {
	unsigned int first_region : 1;	
	unsigned int done : 1;		
	unsigned int any_spills : 1;	
	unsigned int in_body : 1;	
	unsigned long flags;		

	u8 *imask;			
	unsigned long pr_val;		
	unsigned long pr_mask;		
	long spill_offset;		
	int region_start;
	int region_len;
	int epilogue_start;
	int epilogue_count;
	int when_target;

	u8 gr_save_loc;			
	u8 return_link_reg;		

	struct unw_labeled_state *labeled_states;	
	struct unw_reg_state curr;	
};

enum unw_nat_type {
	UNW_NAT_NONE,		
	UNW_NAT_VAL,		
	UNW_NAT_MEMSTK,		
	UNW_NAT_REGSTK		
};

enum unw_insn_opcode {
	UNW_INSN_ADD,			
	UNW_INSN_ADD_PSP,		
	UNW_INSN_ADD_SP,		
	UNW_INSN_MOVE,			
	UNW_INSN_MOVE2,			
	UNW_INSN_MOVE_STACKED,		
	UNW_INSN_SETNAT_MEMSTK,		
	UNW_INSN_SETNAT_TYPE,		
	UNW_INSN_LOAD,			
	UNW_INSN_MOVE_SCRATCH,		
	UNW_INSN_MOVE_CONST,            
};

struct unw_insn {
	unsigned int opc	:  4;
	unsigned int dst	:  9;
	signed int val		: 19;
};

#define UNW_MAX_SCRIPT_LEN	(UNW_NUM_REGS + 5)

struct unw_script {
	unsigned long ip;		
	unsigned long pr_mask;		
	unsigned long pr_val;		
	rwlock_t lock;
	unsigned int flags;		
	unsigned short lru_chain;	
	unsigned short coll_chain;	
	unsigned short hint;		
	unsigned short count;		
	struct unw_insn insn[UNW_MAX_SCRIPT_LEN];
};
