
#ifndef __DIVA_XDI_UM_CFG_MESSSGE_H__
#define __DIVA_XDI_UM_CFG_MESSAGE_H__


#define DIVA_XDI_UM_CMD_GET_CARD_ORDINAL	0

/*
  no acknowledge will be generated, memory block will be written in the
  memory at given offset
*/
#define DIVA_XDI_UM_CMD_WRITE_SDRAM_BLOCK	1

#define DIVA_XDI_UM_CMD_WRITE_FPGA				2

#define DIVA_XDI_UM_CMD_READ_SDRAM				3

#define DIVA_XDI_UM_CMD_GET_SERIAL_NR			4

#define DIVA_XDI_UM_CMD_GET_PCI_HW_CONFIG	5

#define DIVA_XDI_UM_CMD_RESET_ADAPTER			6

#define DIVA_XDI_UM_CMD_START_ADAPTER			7

#define DIVA_XDI_UM_CMD_STOP_ADAPTER			8

#define DIVA_XDI_UM_CMD_GET_CARD_STATE		9

#define DIVA_XDI_UM_CMD_READ_XLOG_ENTRY		10

#define DIVA_XDI_UM_CMD_SET_PROTOCOL_FEATURES	11

typedef struct _diva_xdi_um_cfg_cmd_data_set_features {
	dword features;
} diva_xdi_um_cfg_cmd_data_set_features_t;

typedef struct _diva_xdi_um_cfg_cmd_data_start {
	dword offset;
	dword features;
} diva_xdi_um_cfg_cmd_data_start_t;

typedef struct _diva_xdi_um_cfg_cmd_data_write_sdram {
	dword ram_number;
	dword offset;
	dword length;
} diva_xdi_um_cfg_cmd_data_write_sdram_t;

typedef struct _diva_xdi_um_cfg_cmd_data_write_fpga {
	dword fpga_number;
	dword image_length;
} diva_xdi_um_cfg_cmd_data_write_fpga_t;

typedef struct _diva_xdi_um_cfg_cmd_data_read_sdram {
	dword ram_number;
	dword offset;
	dword length;
} diva_xdi_um_cfg_cmd_data_read_sdram_t;

typedef union _diva_xdi_um_cfg_cmd_data {
	diva_xdi_um_cfg_cmd_data_write_sdram_t write_sdram;
	diva_xdi_um_cfg_cmd_data_write_fpga_t write_fpga;
	diva_xdi_um_cfg_cmd_data_read_sdram_t read_sdram;
	diva_xdi_um_cfg_cmd_data_start_t start;
	diva_xdi_um_cfg_cmd_data_set_features_t features;
} diva_xdi_um_cfg_cmd_data_t;

typedef struct _diva_xdi_um_cfg_cmd {
	dword adapter;		
	dword command;
	diva_xdi_um_cfg_cmd_data_t command_data;
	dword data_length;	
} diva_xdi_um_cfg_cmd_t;

#endif
