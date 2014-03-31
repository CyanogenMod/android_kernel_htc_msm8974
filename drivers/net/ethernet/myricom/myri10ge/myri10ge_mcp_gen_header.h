#ifndef __MYRI10GE_MCP_GEN_HEADER_H__
#define __MYRI10GE_MCP_GEN_HEADER_H__


#define MCP_HEADER_PTR_OFFSET  0x3c

#define MCP_TYPE_MX 0x4d582020	
#define MCP_TYPE_PCIE 0x70636965	
#define MCP_TYPE_ETH 0x45544820	
#define MCP_TYPE_MCP0 0x4d435030	
#define MCP_TYPE_DFLT 0x20202020	
#define MCP_TYPE_ETHZ 0x4554485a	

struct mcp_gen_header {
	
	unsigned header_length;
	__be32 mcp_type;
	char version[128];
	unsigned mcp_private;	

	
	unsigned sram_size;
	unsigned string_specs;	
	unsigned string_specs_len;


	
	unsigned char mcp_index;
	unsigned char disable_rabbit;
	unsigned char unaligned_tlp;
	unsigned char pcie_link_algo;
	unsigned counters_addr;
	unsigned copy_block_info;	
	unsigned short handoff_id_major;	
	unsigned short handoff_id_caps;	
	unsigned msix_table_addr;	
	unsigned bss_addr;	
	unsigned features;
	unsigned ee_hdr_addr;
	unsigned led_pattern;
	unsigned led_pattern_dflt;
	
};

struct zmcp_info {
	unsigned info_len;
	unsigned zmcp_addr;
	unsigned zmcp_len;
	unsigned mcp_edata;
};

#endif				
