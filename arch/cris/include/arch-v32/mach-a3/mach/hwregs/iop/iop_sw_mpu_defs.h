#ifndef __iop_sw_mpu_defs_h
#define __iop_sw_mpu_defs_h

#ifndef REG_RD
#define REG_RD( scope, inst, reg ) \
  REG_READ( reg_##scope##_##reg, \
            (inst) + REG_RD_ADDR_##scope##_##reg )
#endif

#ifndef REG_WR
#define REG_WR( scope, inst, reg, val ) \
  REG_WRITE( reg_##scope##_##reg, \
             (inst) + REG_WR_ADDR_##scope##_##reg, (val) )
#endif

#ifndef REG_RD_VECT
#define REG_RD_VECT( scope, inst, reg, index ) \
  REG_READ( reg_##scope##_##reg, \
            (inst) + REG_RD_ADDR_##scope##_##reg + \
	    (index) * STRIDE_##scope##_##reg )
#endif

#ifndef REG_WR_VECT
#define REG_WR_VECT( scope, inst, reg, index, val ) \
  REG_WRITE( reg_##scope##_##reg, \
             (inst) + REG_WR_ADDR_##scope##_##reg + \
	     (index) * STRIDE_##scope##_##reg, (val) )
#endif

#ifndef REG_RD_INT
#define REG_RD_INT( scope, inst, reg ) \
  REG_READ( int, (inst) + REG_RD_ADDR_##scope##_##reg )
#endif

#ifndef REG_WR_INT
#define REG_WR_INT( scope, inst, reg, val ) \
  REG_WRITE( int, (inst) + REG_WR_ADDR_##scope##_##reg, (val) )
#endif

#ifndef REG_RD_INT_VECT
#define REG_RD_INT_VECT( scope, inst, reg, index ) \
  REG_READ( int, (inst) + REG_RD_ADDR_##scope##_##reg + \
	    (index) * STRIDE_##scope##_##reg )
#endif

#ifndef REG_WR_INT_VECT
#define REG_WR_INT_VECT( scope, inst, reg, index, val ) \
  REG_WRITE( int, (inst) + REG_WR_ADDR_##scope##_##reg + \
	     (index) * STRIDE_##scope##_##reg, (val) )
#endif

#ifndef REG_TYPE_CONV
#define REG_TYPE_CONV( type, orgtype, val ) \
  ( { union { orgtype o; type n; } r; r.o = val; r.n; } )
#endif

#ifndef reg_page_size
#define reg_page_size 8192
#endif

#ifndef REG_ADDR
#define REG_ADDR( scope, inst, reg ) \
  ( (inst) + REG_RD_ADDR_##scope##_##reg )
#endif

#ifndef REG_ADDR_VECT
#define REG_ADDR_VECT( scope, inst, reg, index ) \
  ( (inst) + REG_RD_ADDR_##scope##_##reg + \
    (index) * STRIDE_##scope##_##reg )
#endif


typedef struct {
  unsigned int cfg : 2;
  unsigned int dummy1 : 30;
} reg_iop_sw_mpu_rw_sw_cfg_owner;
#define REG_RD_ADDR_iop_sw_mpu_rw_sw_cfg_owner 0
#define REG_WR_ADDR_iop_sw_mpu_rw_sw_cfg_owner 0

typedef unsigned int reg_iop_sw_mpu_r_spu_trace;
#define REG_RD_ADDR_iop_sw_mpu_r_spu_trace 4

typedef unsigned int reg_iop_sw_mpu_r_spu_fsm_trace;
#define REG_RD_ADDR_iop_sw_mpu_r_spu_fsm_trace 8

typedef struct {
  unsigned int keep_owner : 1;
  unsigned int cmd        : 2;
  unsigned int size       : 3;
  unsigned int wr_spu_mem : 1;
  unsigned int dummy1     : 25;
} reg_iop_sw_mpu_rw_mc_ctrl;
#define REG_RD_ADDR_iop_sw_mpu_rw_mc_ctrl 12
#define REG_WR_ADDR_iop_sw_mpu_rw_mc_ctrl 12

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_mpu_rw_mc_data;
#define REG_RD_ADDR_iop_sw_mpu_rw_mc_data 16
#define REG_WR_ADDR_iop_sw_mpu_rw_mc_data 16

typedef unsigned int reg_iop_sw_mpu_rw_mc_addr;
#define REG_RD_ADDR_iop_sw_mpu_rw_mc_addr 20
#define REG_WR_ADDR_iop_sw_mpu_rw_mc_addr 20

typedef unsigned int reg_iop_sw_mpu_rs_mc_data;
#define REG_RD_ADDR_iop_sw_mpu_rs_mc_data 24

typedef unsigned int reg_iop_sw_mpu_r_mc_data;
#define REG_RD_ADDR_iop_sw_mpu_r_mc_data 28

typedef struct {
  unsigned int busy_cpu     : 1;
  unsigned int busy_mpu     : 1;
  unsigned int busy_spu     : 1;
  unsigned int owned_by_cpu : 1;
  unsigned int owned_by_mpu : 1;
  unsigned int owned_by_spu : 1;
  unsigned int dummy1       : 26;
} reg_iop_sw_mpu_r_mc_stat;
#define REG_RD_ADDR_iop_sw_mpu_r_mc_stat 32

typedef struct {
  unsigned int byte0 : 8;
  unsigned int byte1 : 8;
  unsigned int byte2 : 8;
  unsigned int byte3 : 8;
} reg_iop_sw_mpu_rw_bus_clr_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus_clr_mask 36
#define REG_WR_ADDR_iop_sw_mpu_rw_bus_clr_mask 36

typedef struct {
  unsigned int byte0 : 8;
  unsigned int byte1 : 8;
  unsigned int byte2 : 8;
  unsigned int byte3 : 8;
} reg_iop_sw_mpu_rw_bus_set_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus_set_mask 40
#define REG_WR_ADDR_iop_sw_mpu_rw_bus_set_mask 40

typedef struct {
  unsigned int byte0 : 1;
  unsigned int byte1 : 1;
  unsigned int byte2 : 1;
  unsigned int byte3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_sw_mpu_rw_bus_oe_clr_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus_oe_clr_mask 44
#define REG_WR_ADDR_iop_sw_mpu_rw_bus_oe_clr_mask 44

typedef struct {
  unsigned int byte0 : 1;
  unsigned int byte1 : 1;
  unsigned int byte2 : 1;
  unsigned int byte3 : 1;
  unsigned int dummy1 : 28;
} reg_iop_sw_mpu_rw_bus_oe_set_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_bus_oe_set_mask 48
#define REG_WR_ADDR_iop_sw_mpu_rw_bus_oe_set_mask 48

typedef unsigned int reg_iop_sw_mpu_r_bus_in;
#define REG_RD_ADDR_iop_sw_mpu_r_bus_in 52

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_mpu_rw_gio_clr_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_gio_clr_mask 56
#define REG_WR_ADDR_iop_sw_mpu_rw_gio_clr_mask 56

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_mpu_rw_gio_set_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_gio_set_mask 60
#define REG_WR_ADDR_iop_sw_mpu_rw_gio_set_mask 60

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_mpu_rw_gio_oe_clr_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_gio_oe_clr_mask 64
#define REG_WR_ADDR_iop_sw_mpu_rw_gio_oe_clr_mask 64

typedef struct {
  unsigned int val : 32;
} reg_iop_sw_mpu_rw_gio_oe_set_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_gio_oe_set_mask 68
#define REG_WR_ADDR_iop_sw_mpu_rw_gio_oe_set_mask 68

typedef unsigned int reg_iop_sw_mpu_r_gio_in;
#define REG_RD_ADDR_iop_sw_mpu_r_gio_in 72

typedef struct {
  unsigned int intr0  : 1;
  unsigned int intr1  : 1;
  unsigned int intr2  : 1;
  unsigned int intr3  : 1;
  unsigned int intr4  : 1;
  unsigned int intr5  : 1;
  unsigned int intr6  : 1;
  unsigned int intr7  : 1;
  unsigned int intr8  : 1;
  unsigned int intr9  : 1;
  unsigned int intr10 : 1;
  unsigned int intr11 : 1;
  unsigned int intr12 : 1;
  unsigned int intr13 : 1;
  unsigned int intr14 : 1;
  unsigned int intr15 : 1;
  unsigned int intr16 : 1;
  unsigned int intr17 : 1;
  unsigned int intr18 : 1;
  unsigned int intr19 : 1;
  unsigned int intr20 : 1;
  unsigned int intr21 : 1;
  unsigned int intr22 : 1;
  unsigned int intr23 : 1;
  unsigned int intr24 : 1;
  unsigned int intr25 : 1;
  unsigned int intr26 : 1;
  unsigned int intr27 : 1;
  unsigned int intr28 : 1;
  unsigned int intr29 : 1;
  unsigned int intr30 : 1;
  unsigned int intr31 : 1;
} reg_iop_sw_mpu_rw_cpu_intr;
#define REG_RD_ADDR_iop_sw_mpu_rw_cpu_intr 76
#define REG_WR_ADDR_iop_sw_mpu_rw_cpu_intr 76

typedef struct {
  unsigned int intr0  : 1;
  unsigned int intr1  : 1;
  unsigned int intr2  : 1;
  unsigned int intr3  : 1;
  unsigned int intr4  : 1;
  unsigned int intr5  : 1;
  unsigned int intr6  : 1;
  unsigned int intr7  : 1;
  unsigned int intr8  : 1;
  unsigned int intr9  : 1;
  unsigned int intr10 : 1;
  unsigned int intr11 : 1;
  unsigned int intr12 : 1;
  unsigned int intr13 : 1;
  unsigned int intr14 : 1;
  unsigned int intr15 : 1;
  unsigned int intr16 : 1;
  unsigned int intr17 : 1;
  unsigned int intr18 : 1;
  unsigned int intr19 : 1;
  unsigned int intr20 : 1;
  unsigned int intr21 : 1;
  unsigned int intr22 : 1;
  unsigned int intr23 : 1;
  unsigned int intr24 : 1;
  unsigned int intr25 : 1;
  unsigned int intr26 : 1;
  unsigned int intr27 : 1;
  unsigned int intr28 : 1;
  unsigned int intr29 : 1;
  unsigned int intr30 : 1;
  unsigned int intr31 : 1;
} reg_iop_sw_mpu_r_cpu_intr;
#define REG_RD_ADDR_iop_sw_mpu_r_cpu_intr 80

typedef struct {
  unsigned int spu_intr0      : 1;
  unsigned int trigger_grp0   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr1      : 1;
  unsigned int trigger_grp1   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int spu_intr2      : 1;
  unsigned int trigger_grp2   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr3      : 1;
  unsigned int trigger_grp3   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_rw_intr_grp0_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_intr_grp0_mask 84
#define REG_WR_ADDR_iop_sw_mpu_rw_intr_grp0_mask 84

typedef struct {
  unsigned int spu_intr0 : 1;
  unsigned int dummy1    : 3;
  unsigned int spu_intr1 : 1;
  unsigned int dummy2    : 3;
  unsigned int spu_intr2 : 1;
  unsigned int dummy3    : 3;
  unsigned int spu_intr3 : 1;
  unsigned int dummy4    : 19;
} reg_iop_sw_mpu_rw_ack_intr_grp0;
#define REG_RD_ADDR_iop_sw_mpu_rw_ack_intr_grp0 88
#define REG_WR_ADDR_iop_sw_mpu_rw_ack_intr_grp0 88

typedef struct {
  unsigned int spu_intr0      : 1;
  unsigned int trigger_grp0   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr1      : 1;
  unsigned int trigger_grp1   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int spu_intr2      : 1;
  unsigned int trigger_grp2   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr3      : 1;
  unsigned int trigger_grp3   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_r_intr_grp0;
#define REG_RD_ADDR_iop_sw_mpu_r_intr_grp0 92

typedef struct {
  unsigned int spu_intr0      : 1;
  unsigned int trigger_grp0   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr1      : 1;
  unsigned int trigger_grp1   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int spu_intr2      : 1;
  unsigned int trigger_grp2   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr3      : 1;
  unsigned int trigger_grp3   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_r_masked_intr_grp0;
#define REG_RD_ADDR_iop_sw_mpu_r_masked_intr_grp0 96

typedef struct {
  unsigned int spu_intr4      : 1;
  unsigned int trigger_grp4   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr5      : 1;
  unsigned int trigger_grp5   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int spu_intr6      : 1;
  unsigned int trigger_grp6   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr7      : 1;
  unsigned int trigger_grp7   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_rw_intr_grp1_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_intr_grp1_mask 100
#define REG_WR_ADDR_iop_sw_mpu_rw_intr_grp1_mask 100

typedef struct {
  unsigned int spu_intr4 : 1;
  unsigned int dummy1    : 3;
  unsigned int spu_intr5 : 1;
  unsigned int dummy2    : 3;
  unsigned int spu_intr6 : 1;
  unsigned int dummy3    : 3;
  unsigned int spu_intr7 : 1;
  unsigned int dummy4    : 19;
} reg_iop_sw_mpu_rw_ack_intr_grp1;
#define REG_RD_ADDR_iop_sw_mpu_rw_ack_intr_grp1 104
#define REG_WR_ADDR_iop_sw_mpu_rw_ack_intr_grp1 104

typedef struct {
  unsigned int spu_intr4      : 1;
  unsigned int trigger_grp4   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr5      : 1;
  unsigned int trigger_grp5   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int spu_intr6      : 1;
  unsigned int trigger_grp6   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr7      : 1;
  unsigned int trigger_grp7   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_r_intr_grp1;
#define REG_RD_ADDR_iop_sw_mpu_r_intr_grp1 108

typedef struct {
  unsigned int spu_intr4      : 1;
  unsigned int trigger_grp4   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr5      : 1;
  unsigned int trigger_grp5   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int spu_intr6      : 1;
  unsigned int trigger_grp6   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr7      : 1;
  unsigned int trigger_grp7   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_r_masked_intr_grp1;
#define REG_RD_ADDR_iop_sw_mpu_r_masked_intr_grp1 112

typedef struct {
  unsigned int spu_intr8      : 1;
  unsigned int trigger_grp0   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr9      : 1;
  unsigned int trigger_grp1   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int spu_intr10     : 1;
  unsigned int trigger_grp2   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr11     : 1;
  unsigned int trigger_grp3   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_rw_intr_grp2_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_intr_grp2_mask 116
#define REG_WR_ADDR_iop_sw_mpu_rw_intr_grp2_mask 116

typedef struct {
  unsigned int spu_intr8  : 1;
  unsigned int dummy1     : 3;
  unsigned int spu_intr9  : 1;
  unsigned int dummy2     : 3;
  unsigned int spu_intr10 : 1;
  unsigned int dummy3     : 3;
  unsigned int spu_intr11 : 1;
  unsigned int dummy4     : 19;
} reg_iop_sw_mpu_rw_ack_intr_grp2;
#define REG_RD_ADDR_iop_sw_mpu_rw_ack_intr_grp2 120
#define REG_WR_ADDR_iop_sw_mpu_rw_ack_intr_grp2 120

typedef struct {
  unsigned int spu_intr8      : 1;
  unsigned int trigger_grp0   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr9      : 1;
  unsigned int trigger_grp1   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int spu_intr10     : 1;
  unsigned int trigger_grp2   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr11     : 1;
  unsigned int trigger_grp3   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_r_intr_grp2;
#define REG_RD_ADDR_iop_sw_mpu_r_intr_grp2 124

typedef struct {
  unsigned int spu_intr8      : 1;
  unsigned int trigger_grp0   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr9      : 1;
  unsigned int trigger_grp1   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int spu_intr10     : 1;
  unsigned int trigger_grp2   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr11     : 1;
  unsigned int trigger_grp3   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_r_masked_intr_grp2;
#define REG_RD_ADDR_iop_sw_mpu_r_masked_intr_grp2 128

typedef struct {
  unsigned int spu_intr12     : 1;
  unsigned int trigger_grp4   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr13     : 1;
  unsigned int trigger_grp5   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int spu_intr14     : 1;
  unsigned int trigger_grp6   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr15     : 1;
  unsigned int trigger_grp7   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_rw_intr_grp3_mask;
#define REG_RD_ADDR_iop_sw_mpu_rw_intr_grp3_mask 132
#define REG_WR_ADDR_iop_sw_mpu_rw_intr_grp3_mask 132

typedef struct {
  unsigned int spu_intr12 : 1;
  unsigned int dummy1     : 3;
  unsigned int spu_intr13 : 1;
  unsigned int dummy2     : 3;
  unsigned int spu_intr14 : 1;
  unsigned int dummy3     : 3;
  unsigned int spu_intr15 : 1;
  unsigned int dummy4     : 19;
} reg_iop_sw_mpu_rw_ack_intr_grp3;
#define REG_RD_ADDR_iop_sw_mpu_rw_ack_intr_grp3 136
#define REG_WR_ADDR_iop_sw_mpu_rw_ack_intr_grp3 136

typedef struct {
  unsigned int spu_intr12     : 1;
  unsigned int trigger_grp4   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr13     : 1;
  unsigned int trigger_grp5   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int spu_intr14     : 1;
  unsigned int trigger_grp6   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr15     : 1;
  unsigned int trigger_grp7   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_r_intr_grp3;
#define REG_RD_ADDR_iop_sw_mpu_r_intr_grp3 140

typedef struct {
  unsigned int spu_intr12     : 1;
  unsigned int trigger_grp4   : 1;
  unsigned int fifo_out_extra : 1;
  unsigned int dmc_out        : 1;
  unsigned int spu_intr13     : 1;
  unsigned int trigger_grp5   : 1;
  unsigned int fifo_in_extra  : 1;
  unsigned int dmc_in         : 1;
  unsigned int spu_intr14     : 1;
  unsigned int trigger_grp6   : 1;
  unsigned int timer_grp0     : 1;
  unsigned int fifo_out       : 1;
  unsigned int spu_intr15     : 1;
  unsigned int trigger_grp7   : 1;
  unsigned int timer_grp1     : 1;
  unsigned int fifo_in        : 1;
  unsigned int dummy1         : 16;
} reg_iop_sw_mpu_r_masked_intr_grp3;
#define REG_RD_ADDR_iop_sw_mpu_r_masked_intr_grp3 144


enum {
  regk_iop_sw_mpu_copy                     = 0x00000000,
  regk_iop_sw_mpu_cpu                      = 0x00000000,
  regk_iop_sw_mpu_mpu                      = 0x00000001,
  regk_iop_sw_mpu_no                       = 0x00000000,
  regk_iop_sw_mpu_nop                      = 0x00000000,
  regk_iop_sw_mpu_rd                       = 0x00000002,
  regk_iop_sw_mpu_reg_copy                 = 0x00000001,
  regk_iop_sw_mpu_rw_bus_clr_mask_default  = 0x00000000,
  regk_iop_sw_mpu_rw_bus_oe_clr_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_bus_oe_set_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_bus_set_mask_default  = 0x00000000,
  regk_iop_sw_mpu_rw_gio_clr_mask_default  = 0x00000000,
  regk_iop_sw_mpu_rw_gio_oe_clr_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_gio_oe_set_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_gio_set_mask_default  = 0x00000000,
  regk_iop_sw_mpu_rw_intr_grp0_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_intr_grp1_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_intr_grp2_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_intr_grp3_mask_default = 0x00000000,
  regk_iop_sw_mpu_rw_sw_cfg_owner_default  = 0x00000000,
  regk_iop_sw_mpu_set                      = 0x00000001,
  regk_iop_sw_mpu_spu                      = 0x00000002,
  regk_iop_sw_mpu_wr                       = 0x00000003,
  regk_iop_sw_mpu_yes                      = 0x00000001
};
#endif 
